# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3934/4433 lines (88.74%)

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
|   7464752 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7464757 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7464757 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    640526 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    640531 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    640531 |   35 | `	sxu32 nH = 5381;` |
|    640531 |   36 | `	zEnd = &zIn[nLen];` |
|    725201 |   37 | `	for(;;){` |
|   1450407 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1234795 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1105857 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    965261 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    640531 |   43 | `	return nH;` |
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
|   3163588 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3163593 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3163593 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3163593 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3163593 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3163593 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3163593 |  112 | `	pNode->nHash = nHash;` |
|   3163593 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3163593 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3163593 |  115 | `	return pNode;` |
|   1581799 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    270292 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    270297 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    270297 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    270297 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    270297 |  133 | `	pNode->pMap  = &(*pMap);` |
|    270297 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    270297 |  135 | `	pNode->nHash = nHash;` |
|    270297 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    270297 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    270297 |  138 | `	pNode->nValIdx = nValIdx;` |
|    270297 |  139 | `	return pNode;` |
|    135151 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3433880 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3433885 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2951427 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2951427 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1475711 |  150 | `	}` |
|   3433885 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3433885 |  153 | `	if( pMap->pFirst == 0 ){` |
|     91847 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     91847 |  156 | `		pMap->pCur = pNode;` |
|     45926 |  157 | `	}else{` |
|   3342043 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3433885 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3433885 |  174 | `	++pMap->nEntry;` |
|   3433885 |  175 | `}` |
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
|   3433880 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3433885 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     96971 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     96971 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     96971 |  245 | `		if( nNew < 1 ){` |
|     91847 |  246 | `			nNew = 16;` |
|     45921 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     96971 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     96971 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     96971 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     96971 |  260 | `		pMap->apBucket = apNew;` |
|     96971 |  261 | `		pMap->nSize = nNew;` |
|     96971 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     91847 |  264 | `			return SXRET_OK;` |
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
|   3342043 |  289 | `	return SXRET_OK;` |
|   1716945 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3163588 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3163593 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3163555 |  310 | `		if( pValue ){` |
|   3163549 |  311 | `			sSafeVal = *pValue;` |
|   3163549 |  312 | `			pValue = &sSafeVal;` |
|   1581772 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3163555 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3163555 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3163555 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3163549 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1581772 |  322 | `		}` |
|   3163555 |  323 | `		nIdx = pObj->nIdx;` |
|   1581780 |  324 | `	}else{` |
|        39 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3163593 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3163593 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3163593 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3163593 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        39 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        19 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3163593 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3163593 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3163593 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3163593 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3163593 |  349 | `	return SXRET_OK;` |
|   1581799 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    270292 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    270297 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    224511 |  370 | `		if( pValue ){` |
|    224201 |  371 | `			sSafeVal = *pValue;` |
|    224201 |  372 | `			pValue = &sSafeVal;` |
|    112098 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    224511 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    224511 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    224511 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    224201 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    112098 |  382 | `		}` |
|    224511 |  383 | `		nIdx = pObj->nIdx;` |
|    112258 |  384 | `	}else{` |
|     45791 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    270297 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    270297 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    270297 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    270297 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     45791 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     22893 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    270297 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    270297 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    270297 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    270297 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    270297 |  409 | `	return SXRET_OK;` |
|    135151 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4287892 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4287897 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       725 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4287177 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4287177 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110564122 |  433 | `	for(;;){` |
| 221128249 |  434 | `		if( pNode == 0 ){` |
|   4282209 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216846040 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216843028 |  438 | `			&& pNode->nHash == nHash` |
| 108422497 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      4973 |  441 | `				if( ppNode ){` |
|      4955 |  442 | `					*ppNode = pNode;` |
|      2475 |  443 | `				}` |
|      4973 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216841073 |  447 | `		pNode = pNode->pNextCollide;` |
|         1 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4282209 |  450 | `	return SXERR_NOTFOUND;` |
|   2143951 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    406126 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    406131 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     35897 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    370239 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    370239 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    306249 |  475 | `	for(;;){` |
|    612503 |  476 | `		if( pNode == 0 ){` |
|    312149 |  477 | `			break;` |
|         - |  478 | `		}` |
|    300354 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    298843 |  480 | `			&& pNode->nHash == nHash` |
|    177761 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     58195 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     58095 |  484 | `				if( ppNode ){` |
|     58067 |  485 | `					*ppNode = pNode;` |
|     29031 |  486 | `				}` |
|     58095 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    242269 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    312149 |  493 | `	return SXERR_NOTFOUND;` |
|    203068 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    406258 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    406263 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    406263 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    406263 |  504 | `	int isNeg = FALSE, nDigit;` |
|    406263 |  505 | `	if( zIn >= zEnd ){` |
|        23 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    406241 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    406237 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    406237 |  516 | `	zDigit = zIn;` |
|    203550 |  517 | `	for(;;){` |
|    407105 |  518 | `		if( zIn >= zEnd ){` |
|       251 |  519 | `			break;` |
|         - |  520 | `		}` |
|    406855 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    405987 |  523 | `			return FALSE;` |
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
|    203134 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    140820 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    140825 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    140825 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    135965 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  559 | `		}` |
|    135965 |  560 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  562 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  563 | `			 * to an integer lookup for key 0. */` |
|    135951 |  564 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    135951 |  565 | `			goto result;` |
|         - |  566 | `		}` |
|         7 |  567 | `	}` |
|         - |  568 | `	/* Perform an int lookup */` |
|      4879 |  569 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  570 | `		/* Force an integer cast */` |
|        27 |  571 | `		PH7_MemObjToInteger(pKey);` |
|        13 |  572 | `	}` |
|         - |  573 | `	/* Perform an int lookup */` |
|      4879 |  574 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     70410 |  575 | `result:` |
|    140825 |  576 | `	if( rc == SXRET_OK ){` |
|         - |  577 | `		/* Node found */` |
|     62209 |  578 | `		if( ppNode ){` |
|     62159 |  579 | `			*ppNode = pNode;` |
|     31077 |  580 | `		}` |
|     62209 |  581 | `		return SXRET_OK;` |
|         - |  582 | `	}` |
|         - |  583 | `	/* No such entry */` |
|     78621 |  584 | `	return SXERR_NOTFOUND;` |
|     70415 |  585 | `}` |
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
|   1021752 |  617 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  618 | `{` |
|   1021757 |  619 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  620 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  621 | `		return TRUE;` |
|         - |  622 | `	}` |
|   1021751 |  623 | `	return FALSE;` |
|    510881 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  627 | ` * hashmap.` |
|         - |  628 | ` * If a node with the given key already exists in the database` |
|         - |  629 | ` * then this function overwrite the old value.` |
|         - |  630 | ` */` |
|   3387636 |  631 | `static sxi32 HashmapInsert(` |
|         - |  632 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  633 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  634 | `	ph7_value *pVal    /* Node value */` |
|         - |  635 | `	)` |
|         5 |  636 | `{` |
|   3387641 |  637 | `	ph7_hashmap_node *pNode = 0;` |
|   3387641 |  638 | `	sxi32 rc = SXRET_OK;` |
|   3387641 |  639 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    224505 |  640 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  641 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  642 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  643 | `			 * path and filed it under 0). */` |
|         8 |  644 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  645 | `		}` |
|    224505 |  646 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       229 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|         - |  649 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  650 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  651 | `		 * overwriting nothing and bumping the auto-index). */` |
|    336413 |  652 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    112136 |  653 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  654 | `				/* Overwrite the old value */` |
|         - |  655 | `				ph7_value *pElem;` |
|       483 |  656 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       483 |  657 | `				if( pElem ){` |
|       483 |  658 | `					if( pVal ){` |
|       483 |  659 | `						PH7_MemObjStore(pVal,pElem);` |
|       244 |  660 | `					}else{` |
|         - |  661 | `						/* Nullify the entry */` |
|       ! 0 |  662 | `						PH7_MemObjToNull(pElem);` |
|         - |  663 | `					}` |
|       239 |  664 | `				}` |
|       483 |  665 | `				return SXRET_OK;` |
|         - |  666 | `		}` |
|    223799 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
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
|    223669 |  680 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    223669 |  681 | `		return rc;` |
|         - |  682 | `	}` |
|   1581568 |  683 | `IntKey:` |
|   3163369 |  684 | `	if( pKey ){` |
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
|   1021727 |  716 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  717 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  718 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  719 | `		}` |
|   1021725 |  720 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  721 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  722 | `		}` |
|         - |  723 | `		/* Assign an automatic index */` |
|   1021719 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1021719 |  725 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1021717 |  726 | `			++pMap->iNextIdx;` |
|    510856 |  727 | `		}` |
|         - |  728 | `	}` |
|         - |  729 | `	/* Insertion result */` |
|   3163181 |  730 | `	return rc;` |
|   1693823 |  731 | `}` |
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
|     45834 |  759 | `static sxi32 HashmapInsertByRef(` |
|         - |  760 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  761 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  762 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  763 | `	)` |
|         5 |  764 | `{` |
|     45839 |  765 | `	ph7_hashmap_node *pNode = 0;` |
|     45839 |  766 | `	sxi32 rc = SXRET_OK;` |
|     45839 |  767 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     45803 |  768 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  769 | `			/* Force a string cast */` |
|       ! 0 |  770 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  771 | `		}` |
|     45803 |  772 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  773 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  774 | `				/* Automatic index assign */` |
|       ! 0 |  775 | `				pKey = 0;` |
|       ! 0 |  776 | `			}` |
|         3 |  777 | `			goto IntKey;` |
|         - |  778 | `		}` |
|     68699 |  779 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     22898 |  780 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  781 | `				/* Overwrite */` |
|        11 |  782 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  783 | `				pNode->nValIdx = nRefIdx;` |
|         - |  784 | `				/* Install in the reference table */` |
|        11 |  785 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  786 | `				return SXRET_OK;` |
|         - |  787 | `		}` |
|         - |  788 | `		/* Perform a blob-key insertion */` |
|     45791 |  789 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     45791 |  790 | `		return rc;` |
|         - |  791 | `	}` |
|        18 |  792 | `IntKey:` |
|        39 |  793 | `	if( pKey ){` |
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
|        33 |  812 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  813 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  814 | `		}` |
|         - |  815 | `		/* Assign an automatic index */` |
|        33 |  816 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|        33 |  817 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|        33 |  818 | `			++pMap->iNextIdx;` |
|        16 |  819 | `		}` |
|         - |  820 | `	}` |
|         - |  821 | `	/* Insertion result */` |
|        39 |  822 | `	return rc;` |
|     22922 |  823 | `}` |
|         - |  824 | `/*` |
|         - |  825 | ` * Extract node value.` |
|         - |  826 | ` */` |
|   1445417 |  827 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  828 | `{` |
|         - |  829 | `	/* Point to the desired object */` |
|         - |  830 | `	ph7_value *pObj;` |
|   1445422 |  831 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1445422 |  832 | `	return pObj;` |
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
|         - |  848 | `	/* Preserve key */` |
|       465 |  849 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  850 | `		/* Int64 key */` |
|       333 |  851 | `		if( !bPreserve ){` |
|         - |  852 | `			/* Assign an automatic index */` |
|       185 |  853 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        95 |  854 | `		}else{` |
|       149 |  855 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  856 | `		}` |
|       169 |  857 | `	}else{` |
|         - |  858 | `		/* Blob key */` |
|       133 |  859 | `		if( !bPreserve ){` |
|         - |  860 | `			/* treat it like an automatically-indexed element, drop the` |
|         - |  861 | `			 * original string key entirely */` |
|        35 |  862 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        18 |  863 | `		}else{` |
|       148 |  864 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|        49 |  865 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - |  866 | `		}` |
|         - |  867 | `	}` |
|       465 |  868 | `	return rc;` |
|       235 |  869 | `}` |
|         - |  870 | `/*` |
|         - |  871 | ` * Compare two node values.` |
|         - |  872 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  873 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  874 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  875 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  876 | ` * documenation.` |
|         - |  877 | ` */` |
|     71774 |  878 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  879 | `{` |
|         - |  880 | `	ph7_value sObj1,sObj2;` |
|         - |  881 | `	sxi32 rc;` |
|     71779 |  882 | `	if( pLeft == pRight ){` |
|         - |  883 | `		/*` |
|         - |  884 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  885 | `		 * below for more information on this sceanario.` |
|         - |  886 | `		 */` |
|       ! 0 |  887 | `		return 0;` |
|         - |  888 | `	}` |
|         - |  889 | `	/* Do the comparison */` |
|     71779 |  890 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     71779 |  891 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     71779 |  892 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     71779 |  893 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     71779 |  894 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     71779 |  895 | `	PH7_MemObjRelease(&sObj1);` |
|     71779 |  896 | `	PH7_MemObjRelease(&sObj2);` |
|     71779 |  897 | `	return rc;` |
|     35866 |  898 | `}` |
|         - |  899 | `/*` |
|         - |  900 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  901 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  902 | ` */` |
|     13992 |  903 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  904 | `{` |
|     13997 |  905 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  906 | `	sxu32 nBucket;` |
|         - |  907 | `	/* Remove old collision links */` |
|     13997 |  908 | `	if( pEntry->pPrevCollide ){` |
|     11391 |  909 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5704 |  910 | `	}else{` |
|      2611 |  911 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  912 | `	}` |
|     13997 |  913 | `	if( pEntry->pNextCollide ){` |
|      1137 |  914 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       581 |  915 | `	}` |
|     13997 |  916 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  917 | `	/* Compute the new hash */` |
|     13997 |  918 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     13997 |  919 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     13997 |  920 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  921 | `	/* Link to the new bucket */` |
|     13997 |  922 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13997 |  923 | `	if( pMap->apBucket[nBucket] ){` |
|     11716 |  924 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5868 |  925 | `	}` |
|     13997 |  926 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13997 |  927 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  928 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  929 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  930 | `	 * the no-overflow invariant uniform). */` |
|     13997 |  931 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     13997 |  932 | `		pMap->iNextIdx++;` |
|      6996 |  933 | `	}` |
|     13997 |  934 | `}` |
|         - |  935 | `/*` |
|         - |  936 | ` * Perform a linear search on a given hashmap.` |
|         - |  937 | ` * Write a pointer to the target node on success.` |
|         - |  938 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  939 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  940 | ` * for more information.` |
|         - |  941 | ` */` |
|     33034 |  942 | `static int HashmapFindValue(` |
|         - |  943 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  944 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - |  945 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - |  946 | `	int bStrict      /* TRUE for strict comparison */` |
|         - |  947 | `	)` |
|         5 |  948 | `{` |
|         - |  949 | `	ph7_hashmap_node *pEntry;` |
|         - |  950 | `	ph7_value sVal,*pVal;` |
|         - |  951 | `	ph7_value sNeedle;` |
|         - |  952 | `	sxi32 rc;` |
|         - |  953 | `	sxu32 n;` |
|         - |  954 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     33039 |  955 | `	pEntry = pMap->pFirst;` |
|     33039 |  956 | `	n = pMap->nEntry;` |
|     33039 |  957 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33039 |  958 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     78713 |  959 | `	for(;;){` |
|    157430 |  960 | `		if( n < 1 ){` |
|       115 |  961 | `			break;` |
|         - |  962 | `		}` |
|         - |  963 | `		/* Extract node value */` |
|    157316 |  964 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    157316 |  965 | `		if( pVal ){` |
|         - |  966 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  967 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  968 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  969 | `			 * so null needles/values take the same path as everything else` |
|         - |  970 | `			 * (the historical null-to-null shortcut here made` |
|         - |  971 | `			 * in_array(null, [""]) false where php says true). */` |
|    157316 |  972 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    157316 |  973 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    157316 |  974 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    157316 |  975 | `			PH7_MemObjRelease(&sVal);` |
|    157316 |  976 | `			PH7_MemObjRelease(&sNeedle);` |
|    157316 |  977 | `			if( rc == 0 ){` |
|     32925 |  978 | `				if( ppNode ){` |
|        23 |  979 | `					*ppNode = pEntry;` |
|        11 |  980 | `				}` |
|         - |  981 | `				/* Match found*/` |
|     32925 |  982 | `				return SXRET_OK;` |
|         - |  983 | `			}` |
|     62196 |  984 | `		}` |
|         - |  985 | `		/* Point to the next entry */` |
|    124396 |  986 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    124396 |  987 | `		n--;` |
|         5 |  988 | `	}` |
|         - |  989 | `	/* No such entry */` |
|       115 |  990 | `	return SXERR_NOTFOUND;` |
|     16522 |  991 | `}` |
|         - |  992 | `/*` |
|         - |  993 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|         - |  994 | ` * for values comparison.` |
|         - |  995 | ` * Write a pointer to the target node on success.` |
|         - |  996 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  997 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|         - |  998 | ` * for more information.` |
|         - |  999 | ` */` |
|        22 | 1000 | `static int HashmapFindValueByCallback(` |
|         - | 1001 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|         - | 1002 | `	ph7_value *pNeedle,    /* Lookup key */` |
|         - | 1003 | `	ph7_value *pCallback,  /* User defined callback */` |
|         - | 1004 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|         - | 1005 | `	)` |
|         1 | 1006 | `{` |
|         - | 1007 | `	ph7_hashmap_node *pEntry;` |
|         - | 1008 | `	ph7_value sResult,*pVal;` |
|         - | 1009 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|         - | 1010 | `	sxi32 rc;` |
|         - | 1011 | `	sxu32 n;` |
|        23 | 1012 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|         - | 1013 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|         - | 1014 | `		 * exception is not thrown again, and let the caller wind down. */` |
|       ! 0 | 1015 | `		return SXERR_NOTFOUND;` |
|         - | 1016 | `	}` |
|         - | 1017 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|        23 | 1018 | `	pEntry = pMap->pFirst;` |
|        23 | 1019 | `	n = pMap->nEntry;` |
|         - | 1020 | `	/* Store callback result here */` |
|        23 | 1021 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|         - | 1022 | `	/* First argument to the callback */` |
|        23 | 1023 | `	apArg[0] = pNeedle;` |
|        25 | 1024 | `	for(;;){` |
|        51 | 1025 | `		if( n < 1 ){` |
|         9 | 1026 | `			break;` |
|         - | 1027 | `		}` |
|         - | 1028 | `		/* Extract node value */` |
|        43 | 1029 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        43 | 1030 | `		if( pVal ){` |
|         - | 1031 | `			/* Invoke the user callback */` |
|        43 | 1032 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|        43 | 1033 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|        43 | 1034 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 1035 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|         - | 1036 | `				 * and report no match for the rest of the run. */` |
|         5 | 1037 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|         5 | 1038 | `				PH7_MemObjRelease(&sResult);` |
|         5 | 1039 | `				return SXERR_NOTFOUND;` |
|         - | 1040 | `			}` |
|        39 | 1041 | `			if( rc == SXRET_OK ){` |
|         - | 1042 | `				/* Extract callback result */` |
|        39 | 1043 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 1044 | `					/* Perform an int cast */` |
|       ! 0 | 1045 | `					PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 1046 | `				}` |
|        39 | 1047 | `				rc = (sxi32)sResult.x.iVal;` |
|        39 | 1048 | `				PH7_MemObjRelease(&sResult);` |
|        39 | 1049 | `				if( rc == 0 ){` |
|         - | 1050 | `					/* Match found*/` |
|        11 | 1051 | `					if( ppNode ){` |
|       ! 0 | 1052 | `						*ppNode = pEntry;` |
|       ! 0 | 1053 | `					}` |
|        11 | 1054 | `					return SXRET_OK;` |
|         - | 1055 | `				}` |
|        14 | 1056 | `			}` |
|        14 | 1057 | `		}` |
|         - | 1058 | `		/* Point to the next entry */` |
|        29 | 1059 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 1060 | `		n--;` |
|         1 | 1061 | `	}` |
|         - | 1062 | `	/* No such entry */` |
|         9 | 1063 | `	return SXERR_NOTFOUND;` |
|        12 | 1064 | `}` |
|         - | 1065 | `/*` |
|         - | 1066 | ` * Compare two hashmaps.` |
|         - | 1067 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1068 | ` * Note on array comparison operators.` |
|         - | 1069 | ` *  According to the PHP language reference manual.` |
|         - | 1070 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1071 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1072 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1073 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1074 | ` *                          order and of the same types.` |
|         - | 1075 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1076 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1077 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1078 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1079 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1080 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1081 | ` * <?php` |
|         - | 1082 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1083 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1084 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1085 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1086 | ` * var_dump($c);` |
|         - | 1087 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1088 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1089 | ` * var_dump($c);` |
|         - | 1090 | ` * ?>` |
|         - | 1091 | ` * When executed, this script will print the following:` |
|         - | 1092 | ` * Union of $a and $b:` |
|         - | 1093 | ` * array(3) {` |
|         - | 1094 | ` *  ["a"]=>` |
|         - | 1095 | ` *  string(5) "apple"` |
|         - | 1096 | ` *  ["b"]=>` |
|         - | 1097 | ` * string(6) "banana"` |
|         - | 1098 | ` *  ["c"]=>` |
|         - | 1099 | ` * string(6) "cherry"` |
|         - | 1100 | ` * }` |
|         - | 1101 | ` * Union of $b and $a:` |
|         - | 1102 | ` * array(3) {` |
|         - | 1103 | ` * ["a"]=>` |
|         - | 1104 | ` * string(4) "pear"` |
|         - | 1105 | ` * ["b"]=>` |
|         - | 1106 | ` * string(10) "strawberry"` |
|         - | 1107 | ` * ["c"]=>` |
|         - | 1108 | ` * string(6) "cherry"` |
|         - | 1109 | ` * }` |
|         - | 1110 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1111 | ` */` |
|        30 | 1112 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1113 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1114 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1115 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1116 | `	)` |
|         1 | 1117 | `{` |
|         - | 1118 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1119 | `	sxi32 rc;` |
|         - | 1120 | `	sxu32 n;` |
|        31 | 1121 | `	if( pLeft == pRight ){` |
|         - | 1122 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1123 | `		 * Unlike the zend engine.` |
|         - | 1124 | `		 */` |
|         3 | 1125 | `		return 0;` |
|         - | 1126 | `	}` |
|        29 | 1127 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1128 | `		/* Must have the same number of entries */` |
|         5 | 1129 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1130 | `	}` |
|         - | 1131 | `	/* Point to the first inserted entry of the left hashmap */` |
|        25 | 1132 | `	pLe = pLeft->pFirst;` |
|        25 | 1133 | `	pRe = 0; /* cc warning */` |
|         - | 1134 | `	/* Perform the comparison */` |
|        25 | 1135 | `	n = pLeft->nEntry;` |
|        59 | 1136 | `	for(;;){` |
|       119 | 1137 | `		if( n < 1 ){` |
|        23 | 1138 | `			break;` |
|         - | 1139 | `		}` |
|        97 | 1140 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1141 | `			/* Int key */` |
|        89 | 1142 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|        45 | 1143 | `		}else{` |
|         9 | 1144 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1145 | `			/* Blob key */` |
|         9 | 1146 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1147 | `		}` |
|        97 | 1148 | `		if( rc != SXRET_OK ){` |
|         - | 1149 | `			/* No such entry in the right side */` |
|       ! 0 | 1150 | `			return 1;` |
|         - | 1151 | `		}` |
|        97 | 1152 | `		rc = 0;` |
|        97 | 1153 | `		if( bStrict ){` |
|         - | 1154 | `			/* Make sure,the keys are of the same type */` |
|        81 | 1155 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1156 | `				rc = 1;` |
|       ! 0 | 1157 | `			}` |
|        40 | 1158 | `		}` |
|        97 | 1159 | `		if( !rc ){` |
|         - | 1160 | `			/* Compare nodes */` |
|        97 | 1161 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|        48 | 1162 | `		}` |
|        97 | 1163 | `		if( rc != 0 ){` |
|         - | 1164 | `			/* Nodes key/value differ */` |
|         3 | 1165 | `			return rc;` |
|         - | 1166 | `		}` |
|         - | 1167 | `		/* Point to the next entry */` |
|        95 | 1168 | `		pLe = pLe->pPrev; /* Reverse link */` |
|        95 | 1169 | `		n--;` |
|         1 | 1170 | `	}` |
|        23 | 1171 | `	return 0; /* Hashmaps are equals */` |
|        16 | 1172 | `}` |
|         - | 1173 | `/*` |
|         - | 1174 | ` * Duplicate a hashmap node.` |
|         - | 1175 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1176 | ` */` |
|    664144 | 1177 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1178 | `	ph7_hashmap *pDest,` |
|         - | 1179 | `	ph7_hashmap_node *pEntry,` |
|         - | 1180 | `	ph7_value *pVal,` |
|         - | 1181 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1182 | `	)` |
|         5 | 1183 | `{` |
|         - | 1184 | `	ph7_value sSafeVal;` |
|         - | 1185 | `	ph7_value sKey;` |
|         - | 1186 | `	sxi32 rc;` |
|         - | 1187 |  |
|    664149 | 1188 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|         - | 1189 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|         - | 1190 | `		 * Re-insert it by reference so the reference survives the duplication` |
|         - | 1191 | `		 * instead of being flattened to a value copy. This keeps spread` |
|         - | 1192 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|         - | 1193 | `		 * with PHP semantics. */` |
|         7 | 1194 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|         7 | 1195 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         3 | 1196 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|         3 | 1197 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|         3 | 1198 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|         3 | 1199 | `			PH7_MemObjRelease(&sKey);` |
|         2 | 1200 | `		}else{` |
|         5 | 1201 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         5 | 1202 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|         2 | 1203 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1204 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1205 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1206 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1207 | `			}else{ /* Dup: preserve the int key */` |
|       ! 0 | 1208 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1209 | `			}` |
|         - | 1210 | `		}` |
|         7 | 1211 | `		return rc;` |
|         - | 1212 | `	}` |
|    664143 | 1213 | `	sSafeVal = *pVal;` |
|         - | 1214 |  |
|    664143 | 1215 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1216 | `		/* Blob key insertion */` |
|      3967 | 1217 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      3967 | 1218 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      3967 | 1219 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      3967 | 1220 | `		PH7_MemObjRelease(&sKey);` |
|      1986 | 1221 | `	}else{` |
|         - | 1222 | `		/* Int key */` |
|    660181 | 1223 | `		if( iAction == 0 ){ /* Merge */` |
|    659939 | 1224 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    330212 | 1225 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1226 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1227 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1228 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1229 | `		}else{ /* Dup */` |
|       215 | 1230 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1231 | `		}` |
|         - | 1232 | `	}` |
|    664143 | 1233 | `	return rc;` |
|    332077 | 1234 | `}` |
|         - | 1235 | `/*` |
|         - | 1236 | ` * Merge two hashmaps.` |
|         - | 1237 | ` * Note on the merge process` |
|         - | 1238 | ` * According to the PHP language reference manual.` |
|         - | 1239 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1240 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1241 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1242 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1243 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1244 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1245 | ` *  keys starting from zero in the result array.` |
|         - | 1246 | ` */` |
|      2780 | 1247 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1248 | `{` |
|         - | 1249 | `	ph7_hashmap_node *pEntry;` |
|         - | 1250 | `	ph7_value *pVal;` |
|         - | 1251 | `	sxi32 rc;` |
|         - | 1252 | `	sxu32 n;` |
|      2785 | 1253 | `	if( pSrc == pDest ){` |
|         - | 1254 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1255 | `		 * Unlike the zend engine.` |
|         - | 1256 | `		 */` |
|       ! 0 | 1257 | `		return SXRET_OK;` |
|         - | 1258 | `	}` |
|         - | 1259 | `	/* Point to the first inserted entry in the source */` |
|      2785 | 1260 | `	pEntry = pSrc->pFirst;` |
|         - | 1261 | `	/* Perform the merge */` |
|    662777 | 1262 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1263 | `		/* Extract the node value */` |
|    659997 | 1264 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    659997 | 1265 | `		if( pVal ){` |
|         - | 1266 | `			/* Make a local copy of the value.` |
|         - | 1267 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1268 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1269 | `			 * to the old pool.` |
|         - | 1270 | `			 */` |
|    659997 | 1271 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    330001 | 1272 | `		}else{` |
|       ! 0 | 1273 | `			rc = SXRET_OK;` |
|         - | 1274 | `		}` |
|    659997 | 1275 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1276 | `			return rc;` |
|         - | 1277 | `		}` |
|         - | 1278 | `		/* Point to the next entry */` |
|    659997 | 1279 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    330001 | 1280 | `	}` |
|      2785 | 1281 | `	return SXRET_OK;` |
|      1395 | 1282 | `}` |
|         - | 1283 | `/*` |
|         - | 1284 | ` * Overwrite entries with the same key.` |
|         - | 1285 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1286 | ` *  According to the PHP language reference manual.` |
|         - | 1287 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1288 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1289 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1290 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1291 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1292 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1293 | ` *  overwriting the previous values.` |
|         - | 1294 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1295 | ` *  by whatever type is in the second array.` |
|         - | 1296 | ` */` |
|        34 | 1297 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1298 | `{` |
|         - | 1299 | `	ph7_hashmap_node *pEntry;` |
|         - | 1300 | `	ph7_value *pVal;` |
|         - | 1301 | `	sxi32 rc;` |
|         - | 1302 | `	sxu32 n;` |
|        36 | 1303 | `	if( pSrc == pDest ){` |
|         - | 1304 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1305 | `		 * Unlike the zend engine.` |
|         - | 1306 | `		 */` |
|       ! 0 | 1307 | `		return SXRET_OK;` |
|         - | 1308 | `	}` |
|         - | 1309 | `	/* Point to the first inserted entry in the source */` |
|        36 | 1310 | `	pEntry = pSrc->pFirst;` |
|         - | 1311 | `	/* Perform the merge */` |
|        80 | 1312 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1313 | `		/* Extract the node value */` |
|        46 | 1314 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        46 | 1315 | `		if( pVal ){` |
|        46 | 1316 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        24 | 1317 | `		}else{` |
|       ! 0 | 1318 | `			rc = SXRET_OK;` |
|         - | 1319 | `		}` |
|        46 | 1320 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1321 | `			return rc;` |
|         - | 1322 | `		}` |
|         - | 1323 | `		/* Point to the next entry */` |
|        46 | 1324 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1325 | `	}` |
|        36 | 1326 | `	return SXRET_OK;` |
|        19 | 1327 | `}` |
|         - | 1328 | `/*` |
|         - | 1329 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1330 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1331 | ` */` |
|      3868 | 1332 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1333 | `{` |
|         - | 1334 | `	ph7_hashmap_node *pEntry;` |
|         - | 1335 | `	ph7_value *pVal;` |
|         - | 1336 | `	sxi32 rc;` |
|         - | 1337 | `	sxu32 n;` |
|      3873 | 1338 | `	if( pSrc == pDest ){` |
|         - | 1339 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1340 | `		 * Unlike the zend engine.` |
|         - | 1341 | `		 */` |
|       ! 0 | 1342 | `		return SXRET_OK;` |
|         - | 1343 | `	}` |
|         - | 1344 | `	/* Point to the first inserted entry in the source */` |
|      3873 | 1345 | `	pEntry = pSrc->pFirst;` |
|         - | 1346 | `	/* Perform the duplication */` |
|      7981 | 1347 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1348 | `		/* Extract the node value */` |
|      4113 | 1349 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4113 | 1350 | `		if( pVal ){` |
|      4113 | 1351 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2059 | 1352 | `		}else{` |
|       ! 0 | 1353 | `			rc = SXRET_OK;` |
|         - | 1354 | `		}` |
|      4113 | 1355 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1356 | `			return rc;` |
|         - | 1357 | `		}` |
|         - | 1358 | `		/* Point to the next entry */` |
|      4113 | 1359 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2059 | 1360 | `	}` |
|      3873 | 1361 | `	return SXRET_OK;` |
|      1939 | 1362 | `}` |
|         - | 1363 | `/*` |
|         - | 1364 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1365 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1366 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1367 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1368 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1369 | ` * this instead of PH7_HashmapDup.` |
|         - | 1370 | ` */` |
|        12 | 1371 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1372 | `{` |
|         - | 1373 | `	ph7_hashmap_node *pEntry;` |
|         - | 1374 | `	ph7_value *pVal;` |
|         - | 1375 | `	sxi32 rc;` |
|         - | 1376 | `	sxu32 n;` |
|        13 | 1377 | `	if( pSrc == pDest ){` |
|       ! 0 | 1378 | `		return SXRET_OK;` |
|         - | 1379 | `	}` |
|        13 | 1380 | `	pEntry = pSrc->pFirst;` |
|       749 | 1381 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1382 | `		/* Extract the node value (resolves foreign references) */` |
|       737 | 1383 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       736 | 1384 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       496 | 1385 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1386 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1387 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1388 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1389 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1390 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1391 | `			pVal = 0;` |
|         2 | 1392 | `		}` |
|       737 | 1393 | `		if( pVal ){` |
|       733 | 1394 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1093 | 1395 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       364 | 1396 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       365 | 1397 | `			}else{` |
|         5 | 1398 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1399 | `			}` |
|       733 | 1400 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1401 | `				return rc;` |
|         - | 1402 | `			}` |
|       366 | 1403 | `		}` |
|         - | 1404 | `		/* Point to the next entry */` |
|       737 | 1405 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       369 | 1406 | `	}` |
|        13 | 1407 | `	return SXRET_OK;` |
|         7 | 1408 | `}` |
|         - | 1409 | `/*` |
|         - | 1410 | ` * Count the map references held by BY-REFERENCE foreach steps iterating the` |
|         - | 1411 | `` * given hashmap. php's `foreach ($a as &$v)` iterates the LIVE array —`` |
|         - | 1412 | ` * appends/deletes inside the body are visited — so a by-ref step's retain` |
|         - | 1413 | ` * must not make writes through the source variable COW-separate away from` |
|         - | 1414 | ` * the loop's map. By-VALUE steps are deliberately NOT discounted: their` |
|         - | 1415 | ` * retain is exactly what makes an in-loop write separate, which is php's` |
|         - | 1416 | ` * iterate-a-snapshot semantic.` |
|         - | 1417 | ` */` |
|        50 | 1418 | `static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)` |
|         3 | 1419 | `{` |
|         - | 1420 | `	ph7_foreach_step *pStep;` |
|        53 | 1421 | `	sxi32 nRef = 0;` |
|       103 | 1422 | `	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        53 | 1423 | `		if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        45 | 1424 | `			nRef++;` |
|        21 | 1425 | `		}` |
|        28 | 1426 | `	}` |
|        53 | 1427 | `	return nRef;` |
|         3 | 1428 | `}` |
|         - | 1429 | `/*` |
|         - | 1430 | ` * Copy-on-write separation for arrays.` |
|         - | 1431 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1432 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1433 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1434 | ` * References held by active by-ref foreach steps do not count as sharers` |
|         - | 1435 | `` * (see HashmapByRefStepRefs): writes during `foreach ($a as &$v)` must land`` |
|         - | 1436 | ` * on the live map the loop is walking, like php.` |
|         - | 1437 | ` */` |
|    233308 | 1438 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1439 | `{` |
|    233313 | 1440 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1441 | `	ph7_hashmap *pNew;` |
|         - | 1442 | `	ph7_value *pBacking;` |
|         - | 1443 | `	sxu32 nValIdx;` |
|         - | 1444 | `	int bValueInPool;` |
|    233313 | 1445 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    233313 | 1446 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1447 | `		/* Sole owner, no separation needed */` |
|    230631 | 1448 | `		return pMap;` |
|         - | 1449 | `	}` |
|      2687 | 1450 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1451 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1452 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1453 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1454 | `		return pMap;` |
|         - | 1455 | `	}` |
|         - | 1456 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1457 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1458 | `	 * frame is popped. */` |
|      2561 | 1459 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2561 | 1460 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2556 | 1461 | `		if( pBacking && pBacking != pValue` |
|      2532 | 1462 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2513 | 1463 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1464 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2513 | 1465 | `			pMap->iRef--;` |
|      2513 | 1466 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1467 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2467 | 1468 | `				pMap->iRef++;` |
|      2467 | 1469 | `				return pMap;` |
|         - | 1470 | `			}` |
|        48 | 1471 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|        48 | 1472 | `			if( pNew == 0 ){` |
|       ! 0 | 1473 | `				pMap->iRef++;` |
|       ! 0 | 1474 | `				return pMap;` |
|         - | 1475 | `			}` |
|        48 | 1476 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1477 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1478 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1479 | `				pMap->iRef++;` |
|       ! 0 | 1480 | `				return pMap;` |
|         - | 1481 | `			}` |
|        48 | 1482 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|        48 | 1483 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1484 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1485 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1486 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1487 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1488 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1489 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1490 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|        48 | 1491 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|        48 | 1492 | `			if( pBacking ){` |
|        48 | 1493 | `				pBacking->x.pOther = pNew;` |
|        23 | 1494 | `			}` |
|         - | 1495 | `			/* Update the stack value to match */` |
|        48 | 1496 | `			pValue->x.pOther = pNew;` |
|        48 | 1497 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|        48 | 1498 | `			return pNew;` |
|         - | 1499 | `		}` |
|        24 | 1500 | `	}` |
|         - | 1501 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1502 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1503 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1504 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1505 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1506 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1507 | `	 * pBacking in the backing-variable branch above). */` |
|        50 | 1508 | `	nValIdx = pValue->nIdx;` |
|        74 | 1509 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        48 | 1510 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        50 | 1511 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        50 | 1512 | `	if( pNew == 0 ){` |
|         - | 1513 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1514 | `		return pMap;` |
|         - | 1515 | `	}` |
|        50 | 1516 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1517 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1518 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1519 | `		return pMap;` |
|         - | 1520 | `	}` |
|        50 | 1521 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        50 | 1522 | `	pMap->iRef--;` |
|        50 | 1523 | `	if( bValueInPool ){` |
|         - | 1524 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        50 | 1525 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        50 | 1526 | `		if( pValue == 0 ){` |
|       ! 0 | 1527 | `			return pNew;` |
|         - | 1528 | `		}` |
|        24 | 1529 | `	}` |
|        50 | 1530 | `	pValue->x.pOther = pNew;` |
|        50 | 1531 | `	return pNew;` |
|    116659 | 1532 | `}` |
|         - | 1533 | `/*` |
|         - | 1534 | ` * Perform the union of two hashmaps.` |
|         - | 1535 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1536 | ` * with a variable holding an array as follows:` |
|         - | 1537 | ` * <?php` |
|         - | 1538 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1539 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1540 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1541 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1542 | ` * var_dump($c);` |
|         - | 1543 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1544 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1545 | ` * var_dump($c);` |
|         - | 1546 | ` * ?>` |
|         - | 1547 | ` * When executed, this script will print the following:` |
|         - | 1548 | ` * Union of $a and $b:` |
|         - | 1549 | ` * array(3) {` |
|         - | 1550 | ` *  ["a"]=>` |
|         - | 1551 | ` *  string(5) "apple"` |
|         - | 1552 | ` *  ["b"]=>` |
|         - | 1553 | ` * string(6) "banana"` |
|         - | 1554 | ` *  ["c"]=>` |
|         - | 1555 | ` * string(6) "cherry"` |
|         - | 1556 | ` * }` |
|         - | 1557 | ` * Union of $b and $a:` |
|         - | 1558 | ` * array(3) {` |
|         - | 1559 | ` * ["a"]=>` |
|         - | 1560 | ` * string(4) "pear"` |
|         - | 1561 | ` * ["b"]=>` |
|         - | 1562 | ` * string(10) "strawberry"` |
|         - | 1563 | ` * ["c"]=>` |
|         - | 1564 | ` * string(6) "cherry"` |
|         - | 1565 | ` * }` |
|         - | 1566 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1567 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1568 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1569 | ` */` |
|      3746 | 1570 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1571 | `{` |
|         - | 1572 | `	ph7_hashmap_node *pEntry;` |
|      3751 | 1573 | `	sxi32 rc = SXRET_OK;` |
|         - | 1574 | `	ph7_value *pObj;` |
|         - | 1575 | `	sxu32 n;` |
|      3751 | 1576 | `	if( pLeft == pRight ){` |
|         - | 1577 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1578 | `		 * Unlike the zend engine.` |
|         - | 1579 | `		 */` |
|       ! 0 | 1580 | `		return SXRET_OK;` |
|         - | 1581 | `	}` |
|         - | 1582 | `	/* Perform the union */` |
|      3751 | 1583 | `	pEntry = pRight->pFirst;` |
|      3785 | 1584 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1585 | `		/* Make sure the given key does not exists in the left array */` |
|        38 | 1586 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1587 | `			/* BLOB key */` |
|        24 | 1588 | `			if( SXRET_OK !=` |
|        20 | 1589 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        20 | 1590 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        20 | 1591 | `					if( pObj ){` |
|        20 | 1592 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1593 | `						/* Perform the insertion */` |
|        20 | 1594 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1595 | `							&sSafeVal,0,FALSE);` |
|        20 | 1596 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1597 | `							return rc;` |
|         - | 1598 | `						}` |
|         8 | 1599 | `					}` |
|         8 | 1600 | `			}` |
|        14 | 1601 | `		}else{` |
|         - | 1602 | `			/* INT key */` |
|        16 | 1603 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        11 | 1604 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        11 | 1605 | `				if( pObj ){` |
|        11 | 1606 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1607 | `					/* Perform the insertion */` |
|        11 | 1608 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        11 | 1609 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1610 | `						return rc;` |
|         - | 1611 | `					}` |
|         5 | 1612 | `				}` |
|         5 | 1613 | `			}` |
|         - | 1614 | `		}` |
|         - | 1615 | `		/* Point to the next entry */` |
|        38 | 1616 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 1617 | `	}` |
|      3751 | 1618 | `	return SXRET_OK;` |
|      1878 | 1619 | `}` |
|         - | 1620 | `/*` |
|         - | 1621 | ` * Allocate a new hashmap.` |
|         - | 1622 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1623 | ` */` |
|    143550 | 1624 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1625 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1626 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1627 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1628 | `	)` |
|         5 | 1629 | `{` |
|         - | 1630 | `	ph7_hashmap *pMap;` |
|         - | 1631 | `	/* Allocate a new instance */` |
|    143555 | 1632 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    143555 | 1633 | `	if( pMap == 0 ){` |
|       ! 0 | 1634 | `		return 0;` |
|         - | 1635 | `	}` |
|         - | 1636 | `	/* Zero the structure */` |
|    143555 | 1637 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1638 | `	/* Fill in the structure */` |
|    143555 | 1639 | `	pMap->pVm = &(*pVm);` |
|    143555 | 1640 | `	pMap->iRef = 1;` |
|         - | 1641 | `	/* Default hash functions */` |
|    143555 | 1642 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    143555 | 1643 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    143555 | 1644 | `	return pMap;` |
|     71780 | 1645 | `}` |
|         - | 1646 | `/*` |
|         - | 1647 | ` * Install superglobals in the given virtual machine.` |
|         - | 1648 | ` * Note on superglobals.` |
|         - | 1649 | ` *  According to the PHP language reference manual.` |
|         - | 1650 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1651 | `*   Description` |
|         - | 1652 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1653 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1654 | `*   global $variable; to access them within functions or methods.` |
|         - | 1655 | `*   These superglobal variables are:` |
|         - | 1656 | `*    $GLOBALS` |
|         - | 1657 | `*    $_SERVER` |
|         - | 1658 | `*    $_GET` |
|         - | 1659 | `*    $_POST` |
|         - | 1660 | `*    $_FILES` |
|         - | 1661 | `*    $_COOKIE` |
|         - | 1662 | `*    $_SESSION` |
|         - | 1663 | `*    $_REQUEST` |
|         - | 1664 | `*    $_ENV` |
|         - | 1665 | `*/` |
|      3350 | 1666 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1667 | `{` |
|         - | 1668 | `	static const char * azSuper[] = {` |
|         - | 1669 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1670 | `		"_GET",      /* $_GET */` |
|         - | 1671 | `		"_POST",     /* $_POST */` |
|         - | 1672 | `		"_FILES",    /* $_FILES */` |
|         - | 1673 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1674 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1675 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1676 | `		"_ENV",      /* $_ENV */` |
|         - | 1677 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1678 | `		"argv"       /* $argv */` |
|         - | 1679 | `	};` |
|         - | 1680 | `	ph7_hashmap *pMap;` |
|         - | 1681 | `	ph7_value *pObj;` |
|         - | 1682 | `	SyString *pFile;` |
|         - | 1683 | `	sxi32 rc;` |
|         - | 1684 | `	sxu32 n;` |
|         - | 1685 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      3355 | 1686 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3355 | 1687 | `	if( pMap == 0 ){` |
|       ! 0 | 1688 | `		return SXERR_MEM;` |
|         - | 1689 | `	}` |
|      3355 | 1690 | `	pVm->pGlobal = pMap;` |
|         - | 1691 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3355 | 1692 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3355 | 1693 | `	if( pObj == 0 ){` |
|       ! 0 | 1694 | `		return SXERR_MEM;` |
|         - | 1695 | `	}` |
|      3355 | 1696 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1697 | `	/* Record object index */` |
|      3355 | 1698 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1699 | `	/* Install the special $GLOBALS array */` |
|      3355 | 1700 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3355 | 1701 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1702 | `		return rc;` |
|         - | 1703 | `	}` |
|         - | 1704 | `	/* Install superglobals now */` |
|     36855 | 1705 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1706 | `		ph7_value *pSuper;` |
|         - | 1707 | `		/* Request an empty array */` |
|     33505 | 1708 | `		pSuper = ph7_new_array(&(*pVm));` |
|     33505 | 1709 | `		if( pSuper == 0 ){` |
|       ! 0 | 1710 | `			return SXERR_MEM;` |
|         - | 1711 | `		}` |
|         - | 1712 | `		/* Install */` |
|     33505 | 1713 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     33505 | 1714 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1715 | `			return rc;` |
|         - | 1716 | `		}` |
|         - | 1717 | `		/* Release the value now it have been installed */` |
|     33505 | 1718 | `		ph7_release_value(&(*pVm),pSuper);` |
|     16755 | 1719 | `	}` |
|         - | 1720 | `	/* Set some $_SERVER entries */` |
|      3355 | 1721 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1722 | `	/*` |
|         - | 1723 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1724 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1725 | `	 */` |
|      6705 | 1726 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1727 | `		"SCRIPT_FILENAME",` |
|      1675 | 1728 | `		pFile ? pFile->zString : ":Memory:",` |
|      3350 | 1729 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1730 | `		);` |
|         - | 1731 | `	/* All done,all super-global are installed now */` |
|      3355 | 1732 | `	return SXRET_OK;` |
|      1680 | 1733 | `}` |
|         - | 1734 | `/*` |
|         - | 1735 | ` * Release a hashmap.` |
|         - | 1736 | ` */` |
|    101774 | 1737 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1738 | `{` |
|         - | 1739 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    101779 | 1740 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1741 | `	sxu32 n;` |
|    101779 | 1742 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1743 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1744 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1745 | `		return SXRET_OK;` |
|         - | 1746 | `	}` |
|    101779 | 1747 | `	if( pMap->pActiveSteps ){` |
|         - | 1748 | `		/* Every node is about to be freed WITHOUT going through` |
|         - | 1749 | `		 * PH7_HashmapUnlinkNode, so its cursor fixup never runs. Park any` |
|         - | 1750 | `		 * live foreach cursor on this map (reachable: array_erase() on the` |
|         - | 1751 | `		 * live map of a by-ref foreach — the CowSeparate discount keeps the` |
|         - | 1752 | `		 * loop's map writable). A NULL cursor ends the loop cleanly at the` |
|         - | 1753 | `		 * next step, or resumes on a fresh insert via the link-time re-arm. */` |
|         - | 1754 | `		ph7_foreach_step *pStep;` |
|        17 | 1755 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|         9 | 1756 | `			pStep->pCursor = 0;` |
|         5 | 1757 | `		}` |
|         4 | 1758 | `	}` |
|         - | 1759 | `	/* Start the release process */` |
|    101779 | 1760 | `	n = 0;` |
|    101779 | 1761 | `	pEntry = pMap->pFirst;` |
|   1724978 | 1762 | `	for(;;){` |
|   3449961 | 1763 | `		if( n >= pMap->nEntry ){` |
|    101779 | 1764 | `			break;` |
|         - | 1765 | `		}` |
|   3348187 | 1766 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1767 | `		/* Remove the reference from the foreign table */` |
|   3348187 | 1768 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3348187 | 1769 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1770 | `			/* Restore the ph7_value to the free list */` |
|   3348157 | 1771 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1674076 | 1772 | `		}` |
|         - | 1773 | `		/* Release the node */` |
|   3348187 | 1774 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    196375 | 1775 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     98185 | 1776 | `		}` |
|   3348187 | 1777 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1778 | `		/* Point to the next entry */` |
|   3348187 | 1779 | `		pEntry = pNext;` |
|   3348187 | 1780 | `		n++;` |
|         5 | 1781 | `	}` |
|    101779 | 1782 | `	if( pMap->nEntry > 0 ){` |
|         - | 1783 | `		/* Release the hash bucket */` |
|     76711 | 1784 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     38353 | 1785 | `	}` |
|    101779 | 1786 | `	if( FreeDS ){` |
|         - | 1787 | `		/* Free the whole instance */` |
|    101753 | 1788 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     50879 | 1789 | `	}else{` |
|         - | 1790 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1791 | `		pMap->apBucket = 0;` |
|        28 | 1792 | `		pMap->iNextIdx = 0;` |
|        28 | 1793 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1794 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1795 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1796 | `	}` |
|    101779 | 1797 | `	return SXRET_OK;` |
|     50892 | 1798 | `}` |
|         - | 1799 | `/*` |
|         - | 1800 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1801 | ` * If the count reaches zero which mean no more variables` |
|         - | 1802 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1803 | ` */` |
|    840942 | 1804 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1805 | `{` |
|    840947 | 1806 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1807 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    840947 | 1808 | `	pMap->iRef--;` |
|    840947 | 1809 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    101733 | 1810 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     50864 | 1811 | `	}` |
|    840947 | 1812 | `}` |
|         - | 1813 | `/*` |
|         - | 1814 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1815 | ` * Write a pointer to the target node on success.` |
|         - | 1816 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1817 | ` */` |
|    140992 | 1818 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1819 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1820 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1821 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1822 | `	)` |
|         5 | 1823 | `{` |
|         - | 1824 | `	sxi32 rc;` |
|    140997 | 1825 | `	if( pMap->nEntry < 1 ){` |
|         - | 1826 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1827 | `		 */` |
|       177 | 1828 | `		return SXERR_NOTFOUND;` |
|         - | 1829 | `	}` |
|    140825 | 1830 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    140825 | 1831 | `	return rc;` |
|     70501 | 1832 | `}` |
|         - | 1833 | `/*` |
|         - | 1834 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1835 | ` * hashmap.` |
|         - | 1836 | ` * If a node with the given key already exists in the database` |
|         - | 1837 | ` * then this function overwrite the old value.` |
|         - | 1838 | ` */` |
|   2727450 | 1839 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 1840 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1841 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1842 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 1843 | `	)` |
|         5 | 1844 | `{` |
|         - | 1845 | `	sxi32 rc;` |
|         - | 1846 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 1847 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 1848 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 1849 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|   2727455 | 1850 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2727455 | 1851 | `	return rc;` |
|         5 | 1852 | `}` |
|         - | 1853 | `/*` |
|         - | 1854 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 1855 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 1856 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 1857 | ` * This is the same routine that backs array_merge().` |
|         - | 1858 | ` */` |
|       654 | 1859 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1860 | `{` |
|       655 | 1861 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         1 | 1862 | `}` |
|         - | 1863 | `/*` |
|         - | 1864 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 1865 | ` * hashmap.` |
|         - | 1866 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 1867 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 1868 | ` * The insertion by reference is triggered when the following` |
|         - | 1869 | ` * expression is encountered.` |
|         - | 1870 | ` * $var = 10;` |
|         - | 1871 | ` *  $a = array(&var);` |
|         - | 1872 | ` * OR` |
|         - | 1873 | ` *  $a[] =& $var;` |
|         - | 1874 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 1875 | ` * over it's contents.` |
|         - | 1876 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 1877 | ` * removed when the foreign ph7_value is unset.` |
|         - | 1878 | ` * Example:` |
|         - | 1879 | ` *  $var = 10;` |
|         - | 1880 | ` *  $a[] =& $var;` |
|         - | 1881 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 1882 | ` *  //Unset the foreign ph7_value now` |
|         - | 1883 | ` *  unset($var);` |
|         - | 1884 | ` *  echo count($a); //0` |
|         - | 1885 | ` * Note that this is a PH7 eXtension.` |
|         - | 1886 | ` * Refer to the official documentation for more information.` |
|         - | 1887 | ` * If a node with the given key already exists in the database` |
|         - | 1888 | ` * then this function overwrite the old value.` |
|         - | 1889 | ` */` |
|     45828 | 1890 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1891 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1892 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1893 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1894 | `	)` |
|         5 | 1895 | `{` |
|         - | 1896 | `	sxi32 rc;` |
|     45833 | 1897 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1898 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1899 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1900 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1901 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1902 | `		return PH7_ABORT;` |
|         - | 1903 | `	}` |
|     45833 | 1904 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     45833 | 1905 | `	return rc;` |
|     22919 | 1906 | `}` |
|         - | 1907 | `/*` |
|         - | 1908 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1909 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1910 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1911 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1912 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1913 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1914 | ` */` |
|     18862 | 1915 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1916 | `{` |
|     18867 | 1917 | `	pStep->pCursor = pMap->pFirst;` |
|     18867 | 1918 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     18867 | 1919 | `	pMap->pActiveSteps = pStep;` |
|     18867 | 1920 | `}` |
|         - | 1921 | `/*` |
|         - | 1922 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1923 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1924 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1925 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1926 | ` */` |
|     18762 | 1927 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1928 | `{` |
|     18767 | 1929 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     18767 | 1930 | `	while( *ppLink ){` |
|     18767 | 1931 | `		if( *ppLink == pStep ){` |
|     18767 | 1932 | `			*ppLink = pStep->pNextActive;` |
|     18767 | 1933 | `			pStep->pNextActive = 0;` |
|     18767 | 1934 | `			return;` |
|         - | 1935 | `		}` |
|       ! 0 | 1936 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1937 | `	}` |
|      9386 | 1938 | `}` |
|         - | 1939 | `/*` |
|         - | 1940 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 1941 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 1942 | ` * return NULL.` |
|         - | 1943 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 1944 | ` */` |
|        64 | 1945 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         1 | 1946 | `{` |
|        65 | 1947 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|        65 | 1948 | `	if( pCur == 0 ){` |
|         - | 1949 | `		/* End of the list,return null */` |
|        27 | 1950 | `		return 0;` |
|         - | 1951 | `	}` |
|         - | 1952 | `	/* Advance the node cursor */` |
|        39 | 1953 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        39 | 1954 | `	return pCur;` |
|        33 | 1955 | `}` |
|         - | 1956 | `/*` |
|         - | 1957 | ` * Extract a node value.` |
|         - | 1958 | ` */` |
|    589694 | 1959 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1960 | `{` |
|    589699 | 1961 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    589699 | 1962 | `	if( pEntry ){` |
|    589699 | 1963 | `		if( bStore ){` |
|    234045 | 1964 | `			PH7_MemObjStore(pEntry,pValue);` |
|    117025 | 1965 | `		}else{` |
|    355659 | 1966 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1967 | `		}` |
|    294800 | 1968 | `	}else{` |
|       ! 0 | 1969 | `		PH7_MemObjRelease(pValue);` |
|         - | 1970 | `	}` |
|    589699 | 1971 | `}` |
|         - | 1972 | `/*` |
|         - | 1973 | ` * Extract a node key.` |
|         - | 1974 | ` */` |
|    155498 | 1975 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 1976 | `{` |
|         - | 1977 | `	/* Fill with the current key */` |
|    155503 | 1978 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    150343 | 1979 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 1980 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 1981 | `		}` |
|    150343 | 1982 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    150343 | 1983 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     75174 | 1984 | `	}else{` |
|      5165 | 1985 | `		SyBlobReset(&pKey->sBlob);` |
|      5165 | 1986 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5165 | 1987 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 1988 | `	}` |
|    155503 | 1989 | `}` |
|         - | 1990 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 1991 | `/*` |
|         - | 1992 | ` * Store the address of nodes value in the given container.` |
|         - | 1993 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 1994 | ` * defined in 'builtin.c' for more information.` |
|         - | 1995 | ` */` |
|        12 | 1996 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         1 | 1997 | `{` |
|        13 | 1998 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 1999 | `	ph7_value *pValue;` |
|         - | 2000 | `	sxu32 n;` |
|         - | 2001 | `	/* Initialize the container */` |
|        13 | 2002 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 2003 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 2004 | `		/* Extract node value */` |
|        21 | 2005 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        21 | 2006 | `		if( pValue ){` |
|        21 | 2007 | `			SySetPut(pOut,(const void *)&pValue);` |
|        10 | 2008 | `		}` |
|         - | 2009 | `		/* Point to the next entry */` |
|        21 | 2010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        11 | 2011 | `	}` |
|         - | 2012 | `	/* Total inserted entries */` |
|        13 | 2013 | `	return (int)SySetUsed(pOut);` |
|         1 | 2014 | `}` |
|         - | 2015 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 2016 | `/* SPDX-SnippetBegin */` |
|         - | 2017 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|         - | 2018 | `/* SPDX-License-Identifier: blessing */` |
|         - | 2019 | `/*` |
|         - | 2020 | ` * Merge sort.` |
|         - | 2021 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|         - | 2022 | ` * Status: Public domain` |
|         - | 2023 | ` */` |
|         - | 2024 | `/* Node comparison callback signature */` |
|         - | 2025 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|         - | 2026 | `/*` |
|         - | 2027 | `** Inputs:` |
|         - | 2028 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 2029 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 2030 | `**   cmp:     A pointer to the comparison function.` |
|         - | 2031 | `**` |
|         - | 2032 | `** Return Value:` |
|         - | 2033 | `**   A pointer to the head of a sorted list containing the elements` |
|         - | 2034 | `**   of both a and b.` |
|         - | 2035 | `**` |
|         - | 2036 | `** Side effects:` |
|         - | 2037 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|         - | 2038 | `**   changed.` |
|         - | 2039 | `*/` |
|     35930 | 2040 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2041 | `{` |
|         - | 2042 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2043 | `    /* Prevent compiler warning */` |
|     35935 | 2044 | `	result.pNext = result.pPrev = 0;` |
|     35935 | 2045 | `	pTail = &result;` |
|    107851 | 2046 | `	while( pA && pB ){` |
|     71921 | 2047 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47612 | 2048 | `			pTail->pPrev = pA;` |
|     47612 | 2049 | `			pA->pNext = pTail;` |
|     47612 | 2050 | `			pTail = pA;` |
|     47612 | 2051 | `			pA = pA->pPrev;` |
|     23781 | 2052 | `		}else{` |
|     24314 | 2053 | `			pTail->pPrev = pB;` |
|     24314 | 2054 | `			pB->pNext = pTail;` |
|     24314 | 2055 | `			pTail = pB;` |
|     24314 | 2056 | `			pB = pB->pPrev;` |
|         - | 2057 | `		}` |
|         5 | 2058 | `	}` |
|     35935 | 2059 | `	if( pA ){` |
|     25380 | 2060 | `		pTail->pPrev = pA;` |
|     25380 | 2061 | `		pA->pNext = pTail;` |
|     23265 | 2062 | `	}else if( pB ){` |
|     10334 | 2063 | `		pTail->pPrev = pB;` |
|     10334 | 2064 | `		pB->pNext = pTail;` |
|      5152 | 2065 | `	}else{` |
|       231 | 2066 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2067 | `	}` |
|     35935 | 2068 | `	return result.pPrev;` |
|         5 | 2069 | `}` |
|         - | 2070 | `/*` |
|         - | 2071 | `** Inputs:` |
|         - | 2072 | `**   Map:       Input hashmap` |
|         - | 2073 | `**   cmp:       A comparison function.` |
|         - | 2074 | `**` |
|         - | 2075 | `** Return Value:` |
|         - | 2076 | `**   Sorted hashmap.` |
|         - | 2077 | `**` |
|         - | 2078 | `** Side effects:` |
|         - | 2079 | `**   The "next" pointers for elements in list are changed.` |
|         - | 2080 | `*/` |
|         - | 2081 | `#define N_SORT_BUCKET  32` |
|       750 | 2082 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2083 | `{` |
|         - | 2084 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2085 | `	sxu32 i;` |
|       755 | 2086 | `	SyZero(a,sizeof(a));` |
|         - | 2087 | `	/* Point to the first inserted entry */` |
|       755 | 2088 | `	pIn = pMap->pFirst;` |
|     14759 | 2089 | `	while( pIn ){` |
|     14009 | 2090 | `		p = pIn;` |
|     14009 | 2091 | `		pIn = p->pPrev;` |
|     14009 | 2092 | `		p->pPrev = 0;` |
|     26689 | 2093 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26689 | 2094 | `			if( a[i]==0 ){` |
|     14009 | 2095 | `				a[i] = p;` |
|     14009 | 2096 | `				break;` |
|       ! 0 | 2097 | `			}else{` |
|     12685 | 2098 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12685 | 2099 | `				a[i] = 0;` |
|         - | 2100 | `			}` |
|      6345 | 2101 | `		}` |
|     14009 | 2102 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2103 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2104 | `			 * But that is impossible.` |
|         - | 2105 | `			 */` |
|       ! 0 | 2106 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2107 | `		}` |
|         5 | 2108 | `	}` |
|       755 | 2109 | `	p = a[0];` |
|     24005 | 2110 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     23255 | 2111 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     11630 | 2112 | `	}` |
|       755 | 2113 | `	p->pNext = 0;` |
|         - | 2114 | `	/* Reflect the change */` |
|       755 | 2115 | `	pMap->pFirst = p;` |
|         - | 2116 | `	/* Reset the loop cursor */` |
|       755 | 2117 | `	pMap->pCur = pMap->pFirst;` |
|       755 | 2118 | `	return SXRET_OK;` |
|         5 | 2119 | `}` |
|         - | 2120 | `/* SPDX-SnippetEnd */` |
|         - | 2121 | `/*` |
|         - | 2122 | ` * Node comparison callback.` |
|         - | 2123 | ` * used-by: [sort(),asort(),...]` |
|         - | 2124 | ` */` |
|     71644 | 2125 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2126 | `{` |
|         - | 2127 | `	ph7_value sA,sB;` |
|         - | 2128 | `	sxi32 iFlags;` |
|         - | 2129 | `	int rc;` |
|     71649 | 2130 | `	if( pCmpData == 0 ){` |
|         - | 2131 | `		/* Perform a standard comparison */` |
|     71625 | 2132 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     71625 | 2133 | `		return rc;` |
|         - | 2134 | `	}` |
|        25 | 2135 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2136 | `	/* Duplicate node values */` |
|        25 | 2137 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        25 | 2138 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        25 | 2139 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        25 | 2140 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        25 | 2141 | `	if( iFlags == 5 ){` |
|         - | 2142 | `		/* String cast */` |
|         - | 2143 | `		const char *zA,*zB;` |
|         - | 2144 | `		sxu32 nA,nB,nMin;` |
|        15 | 2145 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2146 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2147 | `		}` |
|        15 | 2148 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2149 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2150 | `		}` |
|         - | 2151 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        15 | 2152 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        15 | 2153 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        15 | 2154 | `		nA = SyBlobLength(&sA.sBlob);` |
|        15 | 2155 | `		nB = SyBlobLength(&sB.sBlob);` |
|        15 | 2156 | `		nMin = nA < nB ? nA : nB;` |
|        15 | 2157 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        15 | 2158 | `		if( rc == 0 ){` |
|         5 | 2159 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2160 | `			else if( nA > nB ) rc = 1;` |
|         2 | 2161 | `		}` |
|         8 | 2162 | `	}else{` |
|         - | 2163 | `		/* Numeric cast */` |
|        11 | 2164 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2165 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2166 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2167 | `	}` |
|        25 | 2168 | `	PH7_MemObjRelease(&sA);` |
|        25 | 2169 | `	PH7_MemObjRelease(&sB);` |
|        25 | 2170 | `	return rc;` |
|     35801 | 2171 | `}` |
|         - | 2172 | `/*` |
|         - | 2173 | ` * Shared key comparison for ksort()/krsort(): php 8 semantics. Two string` |
|         - | 2174 | ` * keys compare bytewise. Mixed int/string keys: a NUMERIC string compares` |
|         - | 2175 | ` * numerically with the int key; a non-numeric one makes the int key compare` |
|         - | 2176 | ` * AS A STRING ("5" < "b", so int keys land before alphabetic ones — pre-fix` |
|         - | 2177 | ` * PHL cast "b" to 0 and sorted string keys first).` |
|         - | 2178 | ` */` |
|         - | 2179 | `/* True lexicographic compare (memcmp on the common prefix, length breaks` |
|         - | 2180 | ` * ties) — SyBlobCmp compares LENGTH first, which is fine for equality but` |
|         - | 2181 | ` * wrong for ordering ("c" would sort before "a.y"). */` |
|        36 | 2182 | `static sxi32 HashmapLexCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|         2 | 2183 | `{` |
|        38 | 2184 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|        38 | 2185 | `	sxi32 rc = nMin ? SyMemcmp(zA,zB,nMin) : 0;` |
|        38 | 2186 | `	if( rc == 0 ){` |
|       ! 0 | 2187 | `		rc = (sxi32)nA - (sxi32)nB;` |
|       ! 0 | 2188 | `	}` |
|        38 | 2189 | `	return rc;` |
|         2 | 2190 | `}` |
|        58 | 2191 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|         2 | 2192 | `{` |
|         - | 2193 | `	sxi32 rc;` |
|        60 | 2194 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2195 | `		/* Perform a string comparison */` |
|        32 | 2196 | `		rc = HashmapLexCmp((const char *)SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey),` |
|        20 | 2197 | `			(const char *)SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|        12 | 2198 | `	}else{` |
|         - | 2199 | `		SyString sStr;` |
|        39 | 2200 | `		sxi64 iA = 0,iB = 0;` |
|        39 | 2201 | `		int bNum = 1;` |
|        39 | 2202 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|        11 | 2203 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|        11 | 2204 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|        11 | 2205 | `				bNum = 0;` |
|         6 | 2206 | `			}else{` |
|       ! 0 | 2207 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2208 | `			}` |
|         6 | 2209 | `		}else{` |
|        29 | 2210 | `			iA = pA->xKey.iKey;` |
|         - | 2211 | `		}` |
|        39 | 2212 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         7 | 2213 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         7 | 2214 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|         7 | 2215 | `				bNum = 0;` |
|         4 | 2216 | `			}else{` |
|       ! 0 | 2217 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2218 | `			}` |
|         4 | 2219 | `		}else{` |
|        33 | 2220 | `			iB = pB->xKey.iKey;` |
|         - | 2221 | `		}` |
|        39 | 2222 | `		if( bNum ){` |
|        23 | 2223 | `			rc = iA < iB ? -1 : (iA > iB ? 1 : 0);` |
|        12 | 2224 | `		}else{` |
|         - | 2225 | `			/* Render the int key and compare bytewise like php */` |
|         - | 2226 | `			char zNumA[24],zNumB[24];` |
|         - | 2227 | `			SyString sA,sB;` |
|        17 | 2228 | `			if( pA->iType != HASHMAP_BLOB_NODE ){` |
|         7 | 2229 | `				sxu32 n = SyBufferFormat(zNumA,sizeof(zNumA),"%qd",pA->xKey.iKey);` |
|         7 | 2230 | `				SyStringInitFromBuf(&sA,zNumA,n);` |
|         4 | 2231 | `			}else{` |
|        11 | 2232 | `				SyStringInitFromBuf(&sA,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|         - | 2233 | `			}` |
|        17 | 2234 | `			if( pB->iType != HASHMAP_BLOB_NODE ){` |
|        11 | 2235 | `				sxu32 n = SyBufferFormat(zNumB,sizeof(zNumB),"%qd",pB->xKey.iKey);` |
|        11 | 2236 | `				SyStringInitFromBuf(&sB,zNumB,n);` |
|         6 | 2237 | `			}else{` |
|         7 | 2238 | `				SyStringInitFromBuf(&sB,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         - | 2239 | `			}` |
|        17 | 2240 | `			rc = HashmapLexCmp(sA.zString,sA.nByte,sB.zString,sB.nByte);` |
|         - | 2241 | `		}` |
|         - | 2242 | `	}` |
|        60 | 2243 | `	return rc;` |
|         2 | 2244 | `}` |
|         - | 2245 | `/*` |
|         - | 2246 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2247 | ` * used-by: [ksort()]` |
|         - | 2248 | ` */` |
|        44 | 2249 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2250 | `{` |
|        22 | 2251 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        46 | 2252 | `	return HashmapKeyNodeCmp(pA,pB);` |
|         2 | 2253 | `}` |
|         - | 2254 | `/*` |
|         - | 2255 | ` * Node comparison callback.` |
|         - | 2256 | ` * Used by: [rsort(),arsort()];` |
|         - | 2257 | ` */` |
|        78 | 2258 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2259 | `{` |
|         - | 2260 | `	ph7_value sA,sB;` |
|         - | 2261 | `	sxi32 iFlags;` |
|         - | 2262 | `	int rc;` |
|        79 | 2263 | `	if( pCmpData == 0 ){` |
|         - | 2264 | `		/* Perform a standard comparison */` |
|        59 | 2265 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|        59 | 2266 | `		return -rc;` |
|         - | 2267 | `	}` |
|        21 | 2268 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2269 | `	/* Duplicate node values */` |
|        21 | 2270 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        21 | 2271 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        21 | 2272 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        21 | 2273 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        21 | 2274 | `	if( iFlags == 5 ){` |
|         - | 2275 | `		/* String cast */` |
|         - | 2276 | `		const char *zA,*zB;` |
|         - | 2277 | `		sxu32 nA,nB,nMin;` |
|        11 | 2278 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2279 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2280 | `		}` |
|        11 | 2281 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2282 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2283 | `		}` |
|         - | 2284 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        11 | 2285 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        11 | 2286 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        11 | 2287 | `		nA = SyBlobLength(&sA.sBlob);` |
|        11 | 2288 | `		nB = SyBlobLength(&sB.sBlob);` |
|        11 | 2289 | `		nMin = nA < nB ? nA : nB;` |
|        11 | 2290 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        11 | 2291 | `		if( rc == 0 ){` |
|         3 | 2292 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2293 | `			else if( nA > nB ) rc = 1;` |
|         1 | 2294 | `		}` |
|         6 | 2295 | `	}else{` |
|         - | 2296 | `		/* Numeric cast */` |
|        11 | 2297 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2298 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2299 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2300 | `	}` |
|        21 | 2301 | `	PH7_MemObjRelease(&sA);` |
|        21 | 2302 | `	PH7_MemObjRelease(&sB);` |
|        21 | 2303 | `	return -rc;` |
|        40 | 2304 | `}` |
|         - | 2305 | `/*` |
|         - | 2306 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2307 | ` * used-by: [usort(),uasort()]` |
|         - | 2308 | ` */` |
|       110 | 2309 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2310 | `{` |
|         - | 2311 | `	ph7_value sResult,*pCallback;` |
|         - | 2312 | `	ph7_value *pV1,*pV2;` |
|         - | 2313 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2314 | `	sxi32 rc;` |
|         - | 2315 | `	/* Point to the desired callback */` |
|       112 | 2316 | `	pCallback = (ph7_value *)pCmpData;` |
|       112 | 2317 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2318 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2319 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2320 | `		return 0;` |
|         - | 2321 | `	}` |
|         - | 2322 | `	/* initialize the result value */` |
|       106 | 2323 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2324 | `	/* Extract nodes values */` |
|       106 | 2325 | `	pV1 = HashmapExtractNodeValue(pA);` |
|       106 | 2326 | `	pV2 = HashmapExtractNodeValue(pB);` |
|       106 | 2327 | `	apArg[0] = pV1;` |
|       106 | 2328 | `	apArg[1] = pV2;` |
|         - | 2329 | `	/* Invoke the callback */` |
|       106 | 2330 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       106 | 2331 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2332 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2333 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2334 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2335 | `		rc = 0;` |
|       102 | 2336 | `	}else if( rc != SXRET_OK ){` |
|         - | 2337 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2338 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2339 | `	}else{` |
|         - | 2340 | `		/* Extract callback result */` |
|        98 | 2341 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2342 | `			/* Perform an int cast */` |
|       ! 0 | 2343 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2344 | `		}` |
|        98 | 2345 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2346 | `	}` |
|       106 | 2347 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2348 | `	/* Callback result */` |
|       106 | 2349 | `	return rc;` |
|        57 | 2350 | `}` |
|         - | 2351 | `/*` |
|         - | 2352 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2353 | ` * used-by: [krsort()]` |
|         - | 2354 | ` */` |
|        14 | 2355 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2356 | `{` |
|         7 | 2357 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        15 | 2358 | `	return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|         1 | 2359 | `}` |
|         - | 2360 | `/*` |
|         - | 2361 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2362 | ` * used-by: [uksort()]` |
|         - | 2363 | ` */` |
|         6 | 2364 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2365 | `{` |
|         - | 2366 | `	ph7_value sResult,*pCallback;` |
|         - | 2367 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2368 | `	ph7_value sK1,sK2;` |
|         - | 2369 | `	sxi32 rc;` |
|         - | 2370 | `	/* Point to the desired callback */` |
|         7 | 2371 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2372 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2373 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2374 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2375 | `		return 0;` |
|         - | 2376 | `	}` |
|         - | 2377 | `	/* initialize the result value */` |
|         7 | 2378 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2379 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2380 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2381 | `	/* Extract nodes keys */` |
|         7 | 2382 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2383 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2384 | `	apArg[0] = &sK1;` |
|         7 | 2385 | `	apArg[1] = &sK2;` |
|         - | 2386 | `	/* Mark keys as constants */` |
|         7 | 2387 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2388 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2389 | `	/* Invoke the callback */` |
|         7 | 2390 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2391 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2392 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2393 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2394 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2395 | `		rc = 0;` |
|         7 | 2396 | `	}else if( rc != SXRET_OK ){` |
|         - | 2397 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2398 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2399 | `	}else{` |
|         - | 2400 | `		/* Extract callback result */` |
|         7 | 2401 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2402 | `			/* Perform an int cast */` |
|       ! 0 | 2403 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2404 | `		}` |
|         7 | 2405 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2406 | `	}` |
|         7 | 2407 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2408 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2409 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2410 | `	/* Callback result */` |
|         7 | 2411 | `	return rc;` |
|         4 | 2412 | `}` |
|         - | 2413 | `/*` |
|         - | 2414 | ` * Node comparison callback: Random node comparison.` |
|         - | 2415 | ` * used-by: [shuffle()]` |
|         - | 2416 | ` */` |
|        20 | 2417 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2418 | `{` |
|         - | 2419 | `	sxu32 n;` |
|         9 | 2420 | `	SXUNUSED(pB); /* cc warning */` |
|         9 | 2421 | `	SXUNUSED(pCmpData);` |
|         - | 2422 | `	/* Grab a random number */` |
|        21 | 2423 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2424 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2425 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2426 | `	 */` |
|        21 | 2427 | `	return n&1 ? 1 : -1;` |
|         1 | 2428 | `}` |
|         - | 2429 | `/*` |
|         - | 2430 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2431 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2432 | ` */` |
|       680 | 2433 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2434 | `{` |
|         - | 2435 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2436 | `	sxu32 i;` |
|         - | 2437 | `	/* Rehash all entries */` |
|       685 | 2438 | `	pLast = p = pMap->pFirst;` |
|       685 | 2439 | `	pMap->iNextIdx = 0;` |
|       685 | 2440 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|       685 | 2441 | `	i = 0;` |
|      7224 | 2442 | `	for( ;; ){` |
|     14453 | 2443 | `		if( i >= pMap->nEntry ){` |
|       685 | 2444 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       685 | 2445 | `			break;` |
|         - | 2446 | `		}` |
|     13773 | 2447 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2448 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2449 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2450 | `			/* Change key type */` |
|         5 | 2451 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2452 | `		}` |
|     13773 | 2453 | `		HashmapRehashIntNode(p);` |
|         - | 2454 | `		/* Point to the next entry */` |
|     13773 | 2455 | `		i++;` |
|     13773 | 2456 | `		pLast = p;` |
|     13773 | 2457 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2458 | `	}` |
|       685 | 2459 | `}` |
|         - | 2460 | `/*` |
|         - | 2461 | ` * Array functions implementation.` |
|         - | 2462 | ` * Status:` |
|         - | 2463 | ` *  Stable.` |
|         - | 2464 | ` */` |
|         - | 2465 | `/*` |
|         - | 2466 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2467 | ` * Sort an array.` |
|         - | 2468 | ` * Parameters` |
|         - | 2469 | ` *  $array` |
|         - | 2470 | ` *   The input array.` |
|         - | 2471 | ` * $sort_flags` |
|         - | 2472 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2473 | ` *  Sorting type flags:` |
|         - | 2474 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2475 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2476 | ` *   SORT_STRING - compare items as strings` |
|         - | 2477 | ` * Return` |
|         - | 2478 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2479 | ` *` |
|         - | 2480 | ` */` |
|      1016 | 2481 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2482 | `{` |
|         - | 2483 | `	ph7_hashmap *pMap;` |
|         - | 2484 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1021 | 2485 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2486 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2487 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2488 | `		return PH7_OK;` |
|         - | 2489 | `	}` |
|         - | 2490 | `	/* Point to the internal representation of the input hashmap */` |
|      1021 | 2491 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1021 | 2492 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1021 | 2493 | `	if( pMap->nEntry > 1 ){` |
|       661 | 2494 | `		sxi32 iCmpFlags = 0;` |
|       661 | 2495 | `		if( nArg > 1 ){` |
|         - | 2496 | `			/* Extract comparison flags */` |
|         3 | 2497 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2498 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2499 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2500 | `			}` |
|         1 | 2501 | `		}` |
|         - | 2502 | `		/* Do the merge sort */` |
|       661 | 2503 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2504 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       661 | 2505 | `		HashmapSortRehash(pMap);` |
|       328 | 2506 | `	}` |
|         - | 2507 | `	/* All done,return TRUE */` |
|      1021 | 2508 | `	ph7_result_bool(pCtx,1);` |
|      1021 | 2509 | `	return PH7_OK;` |
|       513 | 2510 | `}` |
|         - | 2511 | `/*` |
|         - | 2512 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2513 | ` *  Sort an array and maintain index association.` |
|         - | 2514 | ` * Parameters` |
|         - | 2515 | ` *  $array` |
|         - | 2516 | ` *   The input array.` |
|         - | 2517 | ` * $sort_flags` |
|         - | 2518 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2519 | ` *  Sorting type flags:` |
|         - | 2520 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2521 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2522 | ` *   SORT_STRING - compare items as strings` |
|         - | 2523 | ` * Return` |
|         - | 2524 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2525 | ` */` |
|        32 | 2526 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2527 | `{` |
|         - | 2528 | `	ph7_hashmap *pMap;` |
|         - | 2529 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2530 | `	if( nArg < 1 ){` |
|       ! 0 | 2531 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2532 | `			"ArgumentCountError",` |
|         - | 2533 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2534 | `			);` |
|         - | 2535 | `	}` |
|         - | 2536 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        37 | 2537 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2538 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2539 | `			"TypeError",` |
|         - | 2540 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2541 | `			ph7_type_name(apArg[0])` |
|         - | 2542 | `			);` |
|         - | 2543 | `	}` |
|         - | 2544 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 2545 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        25 | 2546 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        25 | 2547 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2548 | `		sxi32 iCmpFlags = 0;` |
|        21 | 2549 | `		if( nArg > 1 ){` |
|         - | 2550 | `			/* Extract comparison flags */` |
|         5 | 2551 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2552 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2553 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2554 | `			}` |
|         2 | 2555 | `		}` |
|         - | 2556 | `		/* Do the merge sort */` |
|        21 | 2557 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2558 | `		/* Fix the last link broken by the merge */` |
|        49 | 2559 | `		while(pMap->pLast->pPrev){` |
|        29 | 2560 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2561 | `		}` |
|        10 | 2562 | `	}` |
|         - | 2563 | `	/* All done,return TRUE */` |
|        25 | 2564 | `	ph7_result_bool(pCtx,1);` |
|        25 | 2565 | `	return PH7_OK;` |
|        21 | 2566 | `}` |
|         - | 2567 | `/*` |
|         - | 2568 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2569 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2570 | ` * Parameters` |
|         - | 2571 | ` *  $array` |
|         - | 2572 | ` *   The input array.` |
|         - | 2573 | ` * $sort_flags` |
|         - | 2574 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2575 | ` *  Sorting type flags:` |
|         - | 2576 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2577 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2578 | ` *   SORT_STRING - compare items as strings` |
|         - | 2579 | ` * Return` |
|         - | 2580 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2581 | ` */` |
|        30 | 2582 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2583 | `{` |
|         - | 2584 | `	ph7_hashmap *pMap;` |
|         - | 2585 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        35 | 2586 | `	if( nArg < 1 ){` |
|       ! 0 | 2587 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2588 | `			"ArgumentCountError",` |
|         - | 2589 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2590 | `			);` |
|         - | 2591 | `	}` |
|         - | 2592 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2593 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2594 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2595 | `			"TypeError",` |
|         - | 2596 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2597 | `			ph7_type_name(apArg[0])` |
|         - | 2598 | `			);` |
|         - | 2599 | `	}` |
|         - | 2600 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2601 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2602 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2603 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2604 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2605 | `		if( nArg > 1 ){` |
|         - | 2606 | `			/* Extract comparison flags */` |
|         5 | 2607 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2608 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2609 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2610 | `			}` |
|         2 | 2611 | `		}` |
|         - | 2612 | `		/* Do the merge sort */` |
|        19 | 2613 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2614 | `		/* Fix the last link broken by the merge */` |
|        35 | 2615 | `		while(pMap->pLast->pPrev){` |
|        17 | 2616 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2617 | `		}` |
|         9 | 2618 | `	}` |
|         - | 2619 | `	/* All done,return TRUE */` |
|        23 | 2620 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2621 | `	return PH7_OK;` |
|        20 | 2622 | `}` |
|         - | 2623 | `/*` |
|         - | 2624 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2625 | ` *  Sort an array by key.` |
|         - | 2626 | ` * Parameters` |
|         - | 2627 | ` *  $array` |
|         - | 2628 | ` *   The input array.` |
|         - | 2629 | ` * $sort_flags` |
|         - | 2630 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2631 | ` *  Sorting type flags:` |
|         - | 2632 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2633 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2634 | ` *   SORT_STRING - compare items as strings` |
|         - | 2635 | ` * Return` |
|         - | 2636 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2637 | ` */` |
|        14 | 2638 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2639 | `{` |
|         - | 2640 | `	ph7_hashmap *pMap;` |
|         - | 2641 | `	/* Make sure we are dealing with a valid hashmap */` |
|        16 | 2642 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2643 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2644 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2645 | `		return PH7_OK;` |
|         - | 2646 | `	}` |
|         - | 2647 | `	/* Point to the internal representation of the input hashmap */` |
|        16 | 2648 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        16 | 2649 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        16 | 2650 | `	if( pMap->nEntry > 1 ){` |
|        16 | 2651 | `		sxi32 iCmpFlags = 0;` |
|        16 | 2652 | `		if( nArg > 1 ){` |
|         - | 2653 | `			/* Extract comparison flags */` |
|       ! 0 | 2654 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2655 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2656 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2657 | `			}` |
|       ! 0 | 2658 | `		}` |
|         - | 2659 | `		/* Do the merge sort */` |
|        16 | 2660 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2661 | `		/* Fix the last link broken by the merge */` |
|        38 | 2662 | `		while(pMap->pLast->pPrev){` |
|        23 | 2663 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2664 | `		}` |
|         7 | 2665 | `	}` |
|         - | 2666 | `	/* All done,return TRUE */` |
|        16 | 2667 | `	ph7_result_bool(pCtx,1);` |
|        16 | 2668 | `	return PH7_OK;` |
|         9 | 2669 | `}` |
|         - | 2670 | `/*` |
|         - | 2671 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2672 | ` *  Sort an array by key in reverse order.` |
|         - | 2673 | ` * Parameters` |
|         - | 2674 | ` *  $array` |
|         - | 2675 | ` *   The input array.` |
|         - | 2676 | ` * $sort_flags` |
|         - | 2677 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2678 | ` *  Sorting type flags:` |
|         - | 2679 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2680 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2681 | ` *   SORT_STRING - compare items as strings` |
|         - | 2682 | ` * Return` |
|         - | 2683 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2684 | ` */` |
|         4 | 2685 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2686 | `{` |
|         - | 2687 | `	ph7_hashmap *pMap;` |
|         - | 2688 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2689 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2690 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2691 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2692 | `		return PH7_OK;` |
|         - | 2693 | `	}` |
|         - | 2694 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 2695 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         5 | 2696 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 2697 | `	if( pMap->nEntry > 1 ){` |
|         5 | 2698 | `		sxi32 iCmpFlags = 0;` |
|         5 | 2699 | `		if( nArg > 1 ){` |
|         - | 2700 | `			/* Extract comparison flags */` |
|       ! 0 | 2701 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2702 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2703 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2704 | `			}` |
|       ! 0 | 2705 | `		}` |
|         - | 2706 | `		/* Do the merge sort */` |
|         5 | 2707 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2708 | `		/* Fix the last link broken by the merge */` |
|        17 | 2709 | `		while(pMap->pLast->pPrev){` |
|        13 | 2710 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2711 | `		}` |
|         2 | 2712 | `	}` |
|         - | 2713 | `	/* All done,return TRUE */` |
|         5 | 2714 | `	ph7_result_bool(pCtx,1);` |
|         5 | 2715 | `	return PH7_OK;` |
|         3 | 2716 | `}` |
|         - | 2717 | `/*` |
|         - | 2718 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2719 | ` * Sort an array in reverse order.` |
|         - | 2720 | ` * Parameters` |
|         - | 2721 | ` *  $array` |
|         - | 2722 | ` *   The input array.` |
|         - | 2723 | ` * $sort_flags` |
|         - | 2724 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2725 | ` *  Sorting type flags:` |
|         - | 2726 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2727 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2728 | ` *   SORT_STRING - compare items as strings` |
|         - | 2729 | ` * Return` |
|         - | 2730 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2731 | ` */` |
|         2 | 2732 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2733 | `{` |
|         - | 2734 | `	ph7_hashmap *pMap;` |
|         - | 2735 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2736 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2737 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2738 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2739 | `		return PH7_OK;` |
|         - | 2740 | `	}` |
|         - | 2741 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2742 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2743 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2744 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2745 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2746 | `		if( nArg > 1 ){` |
|         - | 2747 | `			/* Extract comparison flags */` |
|       ! 0 | 2748 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2749 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2750 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2751 | `			}` |
|       ! 0 | 2752 | `		}` |
|         - | 2753 | `		/* Do the merge sort */` |
|         3 | 2754 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2755 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         3 | 2756 | `		HashmapSortRehash(pMap);` |
|         1 | 2757 | `	}` |
|         - | 2758 | `	/* All done,return TRUE */` |
|         3 | 2759 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2760 | `	return PH7_OK;` |
|         2 | 2761 | `}` |
|         - | 2762 | `/*` |
|         - | 2763 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2764 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2765 | ` * Parameters` |
|         - | 2766 | ` *  $array` |
|         - | 2767 | ` *   The input array.` |
|         - | 2768 | ` * $cmp_function` |
|         - | 2769 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2770 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2771 | ` *  to, or greater than the second.` |
|         - | 2772 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2773 | ` * Return` |
|         - | 2774 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2775 | ` */` |
|        18 | 2776 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2777 | `{` |
|         - | 2778 | `	ph7_hashmap *pMap;` |
|         - | 2779 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 2780 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2781 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2782 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2783 | `		return PH7_OK;` |
|         - | 2784 | `	}` |
|         - | 2785 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 2786 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        20 | 2787 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 2788 | `	if( pMap->nEntry > 1 ){` |
|        20 | 2789 | `		ph7_value *pCallback = 0;` |
|         - | 2790 | `		ProcNodeCmp xCmp;` |
|        20 | 2791 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        20 | 2792 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2793 | `			/* Point to the desired callback */` |
|        20 | 2794 | `			pCallback = apArg[1];` |
|        11 | 2795 | `		}else{` |
|         - | 2796 | `			/* Use the default comparison function */` |
|       ! 0 | 2797 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2798 | `		}` |
|         - | 2799 | `		/* Do the merge sort */` |
|        20 | 2800 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        20 | 2801 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2802 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        20 | 2803 | `		HashmapSortRehash(pMap);` |
|        20 | 2804 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2805 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2806 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2807 | `			return PH7_EXCEPTION;` |
|         - | 2808 | `		}` |
|         5 | 2809 | `	}` |
|         - | 2810 | `	/* All done,return TRUE */` |
|        12 | 2811 | `	ph7_result_bool(pCtx,1);` |
|        12 | 2812 | `	return PH7_OK;` |
|        11 | 2813 | `}` |
|         - | 2814 | `/*` |
|         - | 2815 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2816 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2817 | ` *  and maintain index association.` |
|         - | 2818 | ` * Parameters` |
|         - | 2819 | ` *  $array` |
|         - | 2820 | ` *   The input array.` |
|         - | 2821 | ` * $cmp_function` |
|         - | 2822 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2823 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2824 | ` *  to, or greater than the second.` |
|         - | 2825 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2826 | ` * Return` |
|         - | 2827 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2828 | ` */` |
|        10 | 2829 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2830 | `{` |
|         - | 2831 | `	ph7_hashmap *pMap;` |
|         - | 2832 | `	/* Make sure we are dealing with a valid hashmap */` |
|        11 | 2833 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2834 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2835 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2836 | `		return PH7_OK;` |
|         - | 2837 | `	}` |
|         - | 2838 | `	/* Point to the internal representation of the input hashmap */` |
|        11 | 2839 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        11 | 2840 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        11 | 2841 | `	if( pMap->nEntry > 1 ){` |
|        11 | 2842 | `		ph7_value *pCallback = 0;` |
|         - | 2843 | `		ProcNodeCmp xCmp;` |
|        11 | 2844 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        11 | 2845 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2846 | `			/* Point to the desired callback */` |
|        11 | 2847 | `			pCallback = apArg[1];` |
|         6 | 2848 | `		}else{` |
|         - | 2849 | `			/* Use the default comparison function */` |
|       ! 0 | 2850 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2851 | `		}` |
|         - | 2852 | `		/* Do the merge sort */` |
|        11 | 2853 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        11 | 2854 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2855 | `		/* Fix the last link broken by the merge */` |
|        23 | 2856 | `		while(pMap->pLast->pPrev){` |
|        13 | 2857 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2858 | `		}` |
|        11 | 2859 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2860 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2861 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2862 | `			return PH7_EXCEPTION;` |
|         - | 2863 | `		}` |
|         5 | 2864 | `	}` |
|         - | 2865 | `	/* All done,return TRUE */` |
|        11 | 2866 | `	ph7_result_bool(pCtx,1);` |
|        11 | 2867 | `	return PH7_OK;` |
|         6 | 2868 | `}` |
|         - | 2869 | `/*` |
|         - | 2870 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2871 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2872 | ` *  function and maintain index association.` |
|         - | 2873 | ` * Parameters` |
|         - | 2874 | ` *  $array` |
|         - | 2875 | ` *   The input array.` |
|         - | 2876 | ` * $cmp_function` |
|         - | 2877 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2878 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2879 | ` *  to, or greater than the second.` |
|         - | 2880 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2881 | ` * Return` |
|         - | 2882 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2883 | ` */` |
|         2 | 2884 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2885 | `{` |
|         - | 2886 | `	ph7_hashmap *pMap;` |
|         - | 2887 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2888 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2889 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2890 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2891 | `		return PH7_OK;` |
|         - | 2892 | `	}` |
|         - | 2893 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2894 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2895 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2896 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2897 | `		ph7_value *pCallback = 0;` |
|         - | 2898 | `		ProcNodeCmp xCmp;` |
|         3 | 2899 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2900 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2901 | `			/* Point to the desired callback */` |
|         3 | 2902 | `			pCallback = apArg[1];` |
|         2 | 2903 | `		}else{` |
|         - | 2904 | `			/* Use the default comparison function */` |
|       ! 0 | 2905 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2906 | `		}` |
|         - | 2907 | `		/* Do the merge sort */` |
|         3 | 2908 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2909 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2910 | `		/* Fix the last link broken by the merge */` |
|         3 | 2911 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2912 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2913 | `		}` |
|         3 | 2914 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2915 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2916 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2917 | `			return PH7_EXCEPTION;` |
|         - | 2918 | `		}` |
|         1 | 2919 | `	}` |
|         - | 2920 | `	/* All done,return TRUE */` |
|         3 | 2921 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2922 | `	return PH7_OK;` |
|         2 | 2923 | `}` |
|         - | 2924 | `/*` |
|         - | 2925 | ` * bool shuffle(array &$array)` |
|         - | 2926 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2927 | ` * Parameters` |
|         - | 2928 | ` *  $array` |
|         - | 2929 | ` *   The input array.` |
|         - | 2930 | ` * Return` |
|         - | 2931 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2932 | ` *` |
|         - | 2933 | ` */` |
|         2 | 2934 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2935 | `{` |
|         - | 2936 | `	ph7_hashmap *pMap;` |
|         - | 2937 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2938 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2939 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2940 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2941 | `		return PH7_OK;` |
|         - | 2942 | `	}` |
|         - | 2943 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2944 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2945 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2946 | `	if( pMap->nEntry > 1 ){` |
|         - | 2947 | `		/* Do the merge sort */` |
|         3 | 2948 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 2949 | `		/* Fix the last link broken by the merge */` |
|         9 | 2950 | `		while(pMap->pLast->pPrev){` |
|         7 | 2951 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2952 | `		}` |
|         1 | 2953 | `	}` |
|         - | 2954 | `	/* All done,return TRUE */` |
|         3 | 2955 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2956 | `	return PH7_OK;` |
|         2 | 2957 | `}` |
|         - | 2958 | `/*` |
|         - | 2959 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 2960 | ` *   Count all elements in an array, or something in an object.` |
|         - | 2961 | ` * Parameters` |
|         - | 2962 | ` *  $var` |
|         - | 2963 | ` *   The array or the object.` |
|         - | 2964 | ` * $mode` |
|         - | 2965 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 2966 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 2967 | ` *  all the elements of a multidimensional array.` |
|         - | 2968 | ` * Return` |
|         - | 2969 | ` *  Returns the number of elements in the array.` |
|         - | 2970 | ` */` |
|      1870 | 2971 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2972 | `{` |
|      1875 | 2973 | `	int bRecursive = FALSE;` |
|      1875 | 2974 | `	int bCycleDetected = FALSE;` |
|         - | 2975 | `	sxi64 iCount;` |
|      1875 | 2976 | `	if( nArg < 1 ){` |
|       ! 0 | 2977 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2978 | `			"ArgumentCountError",` |
|         - | 2979 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2980 | `			);` |
|         - | 2981 | `	}` |
|      1875 | 2982 | `	if( nArg > 2 ){` |
|         4 | 2983 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2984 | `			"ArgumentCountError",` |
|         - | 2985 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 2986 | `			nArg` |
|         - | 2987 | `			);` |
|         - | 2988 | `	}` |
|         - | 2989 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 2990 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 2991 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1873 | 2992 | `	if( nArg > 1 ){` |
|        45 | 2993 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        45 | 2994 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        11 | 2995 | `			return PH7_VmThrowException(pCtx,` |
|         - | 2996 | `				"ValueError",` |
|         - | 2997 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 2998 | `				);` |
|         - | 2999 | `		}` |
|        34 | 3000 | `		bRecursive = iMode == 1;` |
|        16 | 3001 | `	}` |
|      1865 | 3002 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3003 | `		/* Countable object: dispatch to ->count() */` |
|        73 | 3004 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        62 | 3005 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        62 | 3006 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        62 | 3007 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        59 | 3008 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 3009 | `					"count",sizeof("count")-1);` |
|        59 | 3010 | `				if( pMeth ){` |
|         - | 3011 | `					ph7_value sResult;` |
|        59 | 3012 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        59 | 3013 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        59 | 3014 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        59 | 3015 | `					PH7_MemObjRelease(&sResult);` |
|        59 | 3016 | `					return PH7_OK;` |
|         - | 3017 | `				}` |
|       ! 0 | 3018 | `			}` |
|         1 | 3019 | `		}` |
|        22 | 3020 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3021 | `			"TypeError",` |
|         - | 3022 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3023 | `			ph7_type_name(apArg[0])` |
|         - | 3024 | `			);` |
|         - | 3025 | `	}` |
|         - | 3026 | `	/* Count */` |
|      1797 | 3027 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1797 | 3028 | `	if( bCycleDetected ){` |
|         3 | 3029 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3030 | `	}` |
|      1797 | 3031 | `	ph7_result_int64(pCtx,iCount);` |
|      1797 | 3032 | `	return PH7_OK;` |
|       940 | 3033 | `}` |
|         - | 3034 | `/*` |
|         - | 3035 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3036 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3037 | ` * Parameters` |
|         - | 3038 | ` * $key` |
|         - | 3039 | ` *   Value to check.` |
|         - | 3040 | ` * $search` |
|         - | 3041 | ` *  An array with keys to check.` |
|         - | 3042 | ` * Return` |
|         - | 3043 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3044 | ` */` |
|        90 | 3045 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3046 | `{` |
|         - | 3047 | `	sxi32 rc;` |
|        94 | 3048 | `	if( nArg != 2 ){` |
|         - | 3049 | `		/* PHP requires exactly two arguments */` |
|         4 | 3050 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3051 | `			"ArgumentCountError",` |
|         - | 3052 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         1 | 3053 | `			nArg` |
|         - | 3054 | `			);` |
|         - | 3055 | `	}` |
|         - | 3056 | `	/* Make sure we are dealing with a valid hashmap */` |
|        92 | 3057 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3058 | `		/* Type mismatch -> TypeError */` |
|         8 | 3059 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3060 | `			"TypeError",` |
|         - | 3061 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3062 | `			ph7_type_name(apArg[1])` |
|         - | 3063 | `			);` |
|         - | 3064 | `	}` |
|         - | 3065 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        88 | 3066 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         - | 3067 | `		/* PH7_VmThrowDeprecatedFmt, not ph7_context_throw_error_format: the latter PREPENDS` |
|         - | 3068 | `		 * "array_key_exists(): " and php's message carries no such prefix. */` |
|         3 | 3069 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 3070 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3071 | `			"use an empty string instead"` |
|         - | 3072 | `			);` |
|        87 | 3073 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3074 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3075 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3076 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3077 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3078 | `				,rVal` |
|         - | 3079 | `				);` |
|         1 | 3080 | `		}` |
|         1 | 3081 | `	}` |
|         - | 3082 | `	/* Perform the lookup */` |
|        88 | 3083 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3084 | `	/* lookup result */` |
|        88 | 3085 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        88 | 3086 | `	return PH7_OK;` |
|        49 | 3087 | `}` |
|         - | 3088 | `/*` |
|         - | 3089 | ` * value array_pop(array $array)` |
|         - | 3090 | ` *   POP the last inserted element from the array.` |
|         - | 3091 | ` * Parameter` |
|         - | 3092 | ` *  The array to get the value from.` |
|         - | 3093 | ` * Return` |
|         - | 3094 | ` *  Poped value or NULL on failure.` |
|         - | 3095 | ` */` |
|       102 | 3096 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3097 | `{` |
|         - | 3098 | `	ph7_hashmap *pMap;` |
|         - | 3099 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|       106 | 3100 | `	if( nArg != 1 ){` |
|         4 | 3101 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3102 | `			"ArgumentCountError",` |
|         - | 3103 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         1 | 3104 | `			nArg` |
|         - | 3105 | `			);` |
|         - | 3106 | `	}` |
|         - | 3107 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3108 | `	 * error message as official PHP. Check the index to detect constants. */` |
|       104 | 3109 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3110 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3111 | `			"Error",` |
|         - | 3112 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3113 | `			);` |
|         - | 3114 | `	}` |
|         - | 3115 | `	/* Make sure we are dealing with a valid hashmap */` |
|        98 | 3116 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3117 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3118 | `			"TypeError",` |
|         - | 3119 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3120 | `			ph7_type_name(apArg[0])` |
|         - | 3121 | `			);` |
|         - | 3122 | `	}` |
|        95 | 3123 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        95 | 3124 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        95 | 3125 | `	if( pMap->nEntry < 1 ){` |
|         - | 3126 | `		/* Nothing to pop,return NULL */` |
|         3 | 3127 | `		ph7_result_null(pCtx);` |
|         2 | 3128 | `	}else{` |
|        93 | 3129 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3130 | `		ph7_value *pObj;` |
|        93 | 3131 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        93 | 3132 | `		if( pObj ){` |
|         - | 3133 | `			/* Node value */` |
|        93 | 3134 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3135 | `			/* Unlink the node */` |
|        93 | 3136 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        47 | 3137 | `		}else{` |
|       ! 0 | 3138 | `			ph7_result_null(pCtx);` |
|         - | 3139 | `		}` |
|         - | 3140 | `		/* Reset the cursor */` |
|        93 | 3141 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3142 | `	}` |
|        95 | 3143 | `	return PH7_OK;` |
|        55 | 3144 | `}` |
|         - | 3145 | `/*` |
|         - | 3146 | ` * int array_push($array,$var,...)` |
|         - | 3147 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3148 | ` * Parameters` |
|         - | 3149 | ` *  array` |
|         - | 3150 | ` *    The input array.` |
|         - | 3151 | ` *  var` |
|         - | 3152 | ` *   On or more value to push.` |
|         - | 3153 | ` * Return` |
|         - | 3154 | ` *  New array count (including old items).` |
|         - | 3155 | ` */` |
|        22 | 3156 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3157 | `{` |
|         - | 3158 | `	ph7_hashmap *pMap;` |
|         - | 3159 | `	sxi32 rc;` |
|         - | 3160 | `	int i;` |
|        27 | 3161 | `	if( nArg < 1 ){` |
|       ! 0 | 3162 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3163 | `			"ArgumentCountError",` |
|         - | 3164 | `			"array_push() expects at least 1 argument, %d given",` |
|       ! 0 | 3165 | `			nArg` |
|         - | 3166 | `			);` |
|         - | 3167 | `	}` |
|         - | 3168 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3169 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        27 | 3170 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3171 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3172 | `			"Error",` |
|         - | 3173 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3174 | `			);` |
|         - | 3175 | `	}` |
|         - | 3176 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3177 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3178 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3179 | `			"TypeError",` |
|         - | 3180 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3181 | `			ph7_type_name(apArg[0])` |
|         - | 3182 | `			);` |
|         - | 3183 | `	}` |
|         - | 3184 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3185 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3186 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3187 | `	/* Start pushing given values */` |
|        34 | 3188 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3189 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3190 | `		if( rc != SXRET_OK ){` |
|         3 | 3191 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3192 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3193 | `				return rc;` |
|         - | 3194 | `			}` |
|       ! 0 | 3195 | `			break;` |
|         - | 3196 | `		}` |
|         9 | 3197 | `	}` |
|         - | 3198 | `	/* Return the new count */` |
|        15 | 3199 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3200 | `	return PH7_OK;` |
|        16 | 3201 | `}` |
|         - | 3202 | `/*` |
|         - | 3203 | ` * value array_shift(array $array)` |
|         - | 3204 | ` *   Shift an element off the beginning of array.` |
|         - | 3205 | ` * Parameter` |
|         - | 3206 | ` *  The array to get the value from.` |
|         - | 3207 | ` * Return` |
|         - | 3208 | ` *  Shifted value or NULL on failure.` |
|         - | 3209 | ` */` |
|        44 | 3210 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3211 | `{` |
|         - | 3212 | `	ph7_hashmap *pMap;` |
|         - | 3213 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        49 | 3214 | `	if( nArg != 1 ){` |
|         4 | 3215 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3216 | `			"ArgumentCountError",` |
|         - | 3217 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         1 | 3218 | `			nArg` |
|         - | 3219 | `			);` |
|         - | 3220 | `	}` |
|         - | 3221 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3222 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3223 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3224 | `			"Error",` |
|         - | 3225 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3226 | `			);` |
|         - | 3227 | `	}` |
|         - | 3228 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3229 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3230 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3231 | `			"TypeError",` |
|         - | 3232 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3233 | `			ph7_type_name(apArg[0])` |
|         - | 3234 | `			);` |
|         - | 3235 | `	}` |
|         - | 3236 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3237 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3238 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3239 | `	if( pMap->nEntry < 1 ){` |
|         - | 3240 | `		/* Empty hashmap,return NULL */` |
|         3 | 3241 | `		ph7_result_null(pCtx);` |
|         2 | 3242 | `	}else{` |
|        39 | 3243 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3244 | `		ph7_value *pObj;` |
|         - | 3245 | `		sxu32 n;` |
|        39 | 3246 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3247 | `		if( pObj ){` |
|         - | 3248 | `			/* Node value */` |
|        39 | 3249 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3250 | `			/* Unlink the first node */` |
|        39 | 3251 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3252 | `		}else{` |
|       ! 0 | 3253 | `			ph7_result_null(pCtx);` |
|         - | 3254 | `		}` |
|         - | 3255 | `		/* Rehash all int keys */` |
|        39 | 3256 | `		n = pMap->nEntry;` |
|        39 | 3257 | `		pEntry = pMap->pFirst;` |
|        39 | 3258 | `		pMap->iNextIdx = 0;` |
|        39 | 3259 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|        47 | 3260 | `		for(;;){` |
|        99 | 3261 | `			if( n < 1 ){` |
|        39 | 3262 | `				break;` |
|         - | 3263 | `			}` |
|        65 | 3264 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3265 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3266 | `			}` |
|         - | 3267 | `			/* Point to the next entry */` |
|        65 | 3268 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3269 | `			n--;` |
|         5 | 3270 | `		}` |
|         - | 3271 | `		/* Reset the cursor */` |
|        39 | 3272 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3273 | `	}` |
|        41 | 3274 | `	return PH7_OK;` |
|        27 | 3275 | `}` |
|         - | 3276 | `/*` |
|         - | 3277 | ` * Extract the node cursor value.` |
|         - | 3278 | ` */` |
|      1094 | 3279 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3280 | `{` |
|      1095 | 3281 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3282 | `	ph7_value *pVal;` |
|      1095 | 3283 | `	if( pCur == 0 ){` |
|         - | 3284 | `		/* Cursor does not point to anything,return FALSE */` |
|        39 | 3285 | `		ph7_result_bool(pCtx,0);` |
|        39 | 3286 | `		return PH7_OK;` |
|         - | 3287 | `	}` |
|      1057 | 3288 | `	if( iDirection != 0 ){` |
|       201 | 3289 | `		if( iDirection > 0 ){` |
|         - | 3290 | `			/* Point to the next entry */` |
|       199 | 3291 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       199 | 3292 | `			pCur = pMap->pCur;` |
|       100 | 3293 | `		}else{` |
|         - | 3294 | `			/* Point to the previous entry */` |
|         3 | 3295 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3296 | `			pCur = pMap->pCur;` |
|         - | 3297 | `		}` |
|       201 | 3298 | `		if( pCur == 0 ){` |
|         - | 3299 | `			/* End of input reached,return FALSE */` |
|        83 | 3300 | `			ph7_result_bool(pCtx,0);` |
|        83 | 3301 | `			return PH7_OK;` |
|         - | 3302 | `		}` |
|        59 | 3303 | `	}` |
|         - | 3304 | `	/* Point to the desired element */` |
|       975 | 3305 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       975 | 3306 | `	if( pVal ){` |
|       975 | 3307 | `		ph7_result_value(pCtx,pVal);` |
|       488 | 3308 | `	}else{` |
|       ! 0 | 3309 | `		ph7_result_bool(pCtx,0);` |
|         - | 3310 | `	}` |
|       975 | 3311 | `	return PH7_OK;` |
|       548 | 3312 | `}` |
|         - | 3313 | `/*` |
|         - | 3314 | ` * value current(array $array)` |
|         - | 3315 | ` *  Return the current element in an array.` |
|         - | 3316 | ` * Parameter` |
|         - | 3317 | ` *  $input: The input array.` |
|         - | 3318 | ` * Return` |
|         - | 3319 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3320 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3321 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3322 | ` *  is empty, current() returns FALSE.` |
|         - | 3323 | ` */` |
|       302 | 3324 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3325 | `{` |
|       303 | 3326 | `	if( nArg < 1 ){` |
|         - | 3327 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3328 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3329 | `		return PH7_OK;` |
|         - | 3330 | `	}` |
|         - | 3331 | `	/* Make sure we are dealing with a valid hashmap */` |
|       303 | 3332 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3333 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3334 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3335 | `		return PH7_OK;` |
|         - | 3336 | `	}` |
|       303 | 3337 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       303 | 3338 | `	return PH7_OK;` |
|       152 | 3339 | `}` |
|         - | 3340 | `/*` |
|         - | 3341 | ` * value next(array $input)` |
|         - | 3342 | ` *  Advance the internal array pointer of an array.` |
|         - | 3343 | ` * Parameter` |
|         - | 3344 | ` *  $input: The input array.` |
|         - | 3345 | ` * Return` |
|         - | 3346 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3347 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3348 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3349 | ` */` |
|       198 | 3350 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3351 | `{` |
|       199 | 3352 | `	if( nArg < 1 ){` |
|         - | 3353 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3354 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3355 | `		return PH7_OK;` |
|         - | 3356 | `	}` |
|         - | 3357 | `	/* Make sure we are dealing with a valid hashmap */` |
|       199 | 3358 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3359 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3360 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3361 | `		return PH7_OK;` |
|         - | 3362 | `	}` |
|       199 | 3363 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       199 | 3364 | `	return PH7_OK;` |
|       100 | 3365 | `}` |
|         - | 3366 | `/*` |
|         - | 3367 | ` * value prev(array $input)` |
|         - | 3368 | ` *  Rewind the internal array pointer.` |
|         - | 3369 | ` * Parameter` |
|         - | 3370 | ` *  $input: The input array.` |
|         - | 3371 | ` * Return` |
|         - | 3372 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3373 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3374 | ` *  elements.` |
|         - | 3375 | ` */` |
|         2 | 3376 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3377 | `{` |
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
|         3 | 3389 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3390 | `	return PH7_OK;` |
|         2 | 3391 | `}` |
|         - | 3392 | `/*` |
|         - | 3393 | ` * value end(array $input)` |
|         - | 3394 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3395 | ` * Parameter` |
|         - | 3396 | ` *  $input: The input array.` |
|         - | 3397 | ` * Return` |
|         - | 3398 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3399 | ` */` |
|       348 | 3400 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3401 | `{` |
|         - | 3402 | `	ph7_hashmap *pMap;` |
|       349 | 3403 | `	if( nArg < 1 ){` |
|         - | 3404 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3405 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3406 | `		return PH7_OK;` |
|         - | 3407 | `	}` |
|         - | 3408 | `	/* Make sure we are dealing with a valid hashmap */` |
|       349 | 3409 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3410 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3411 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3412 | `		return PH7_OK;` |
|         - | 3413 | `	}` |
|         - | 3414 | `	/* Point to the internal representation of the input hashmap */` |
|       349 | 3415 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3416 | `	/* Point to the last node */` |
|       349 | 3417 | `	pMap->pCur = pMap->pLast;` |
|         - | 3418 | `	/* Return the last node value */` |
|       349 | 3419 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       349 | 3420 | `	return PH7_OK;` |
|       175 | 3421 | `}` |
|         - | 3422 | `/*` |
|         - | 3423 | ` * value reset(array $array )` |
|         - | 3424 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3425 | ` * Parameter` |
|         - | 3426 | ` *  $input: The input array.` |
|         - | 3427 | ` * Return` |
|         - | 3428 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3429 | ` */` |
|       244 | 3430 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3431 | `{` |
|         - | 3432 | `	ph7_hashmap *pMap;` |
|       245 | 3433 | `	if( nArg < 1 ){` |
|         - | 3434 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3435 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3436 | `		return PH7_OK;` |
|         - | 3437 | `	}` |
|         - | 3438 | `	/* Make sure we are dealing with a valid hashmap */` |
|       245 | 3439 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3440 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3441 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3442 | `		return PH7_OK;` |
|         - | 3443 | `	}` |
|         - | 3444 | `	/* Point to the internal representation of the input hashmap */` |
|       245 | 3445 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3446 | `	/* Point to the first node */` |
|       245 | 3447 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3448 | `	/* Return the last node value if available */` |
|       245 | 3449 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       245 | 3450 | `	return PH7_OK;` |
|       123 | 3451 | `}` |
|         - | 3452 | `/*` |
|         - | 3453 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3454 | ` * array_key_first() and array_key_last().` |
|         - | 3455 | ` */` |
|       672 | 3456 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3457 | `{` |
|       673 | 3458 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3459 | `		/* Key is integer */` |
|       283 | 3460 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       142 | 3461 | `	}else{` |
|         - | 3462 | `		/* Key is blob */` |
|       586 | 3463 | `		ph7_result_string(pCtx,` |
|       390 | 3464 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3465 | `	}` |
|       673 | 3466 | `}` |
|         - | 3467 | `/*` |
|         - | 3468 | ` * value key(array $array)` |
|         - | 3469 | ` *   Fetch a key from an array` |
|         - | 3470 | ` * Parameter` |
|         - | 3471 | ` *  $input` |
|         - | 3472 | ` *   The input array.` |
|         - | 3473 | ` * Return` |
|         - | 3474 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3475 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3476 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3477 | ` *  is empty, key() returns NULL.` |
|         - | 3478 | ` */` |
|       776 | 3479 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3480 | `{` |
|         - | 3481 | `	ph7_hashmap_node *pCur;` |
|         - | 3482 | `	ph7_hashmap *pMap;` |
|       777 | 3483 | `	if( nArg < 1 ){` |
|         - | 3484 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3485 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3486 | `		return PH7_OK;` |
|         - | 3487 | `	}` |
|         - | 3488 | `	/* Make sure we are dealing with a valid hashmap */` |
|       777 | 3489 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3490 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3491 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3492 | `		return PH7_OK;` |
|         - | 3493 | `	}` |
|       777 | 3494 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       777 | 3495 | `	pCur = pMap->pCur;` |
|       777 | 3496 | `	if( pCur == 0 ){` |
|         - | 3497 | `		/* Cursor does not point to anything,return NULL */` |
|       121 | 3498 | `		ph7_result_null(pCtx);` |
|       121 | 3499 | `		return PH7_OK;` |
|         - | 3500 | `	}` |
|       657 | 3501 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       657 | 3502 | `	return PH7_OK;` |
|       389 | 3503 | `}` |
|         - | 3504 | `/*` |
|         - | 3505 | ` * array each(array $input)` |
|         - | 3506 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3507 | ` * Parameter` |
|         - | 3508 | ` *  $input` |
|         - | 3509 | ` *    The input array.` |
|         - | 3510 | ` * Return` |
|         - | 3511 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3512 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3513 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3514 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3515 | ` *  each() returns FALSE.` |
|         - | 3516 | ` */` |
|        22 | 3517 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3518 | `{` |
|         - | 3519 | `	ph7_hashmap_node *pCur;` |
|         - | 3520 | `	ph7_hashmap *pMap;` |
|         - | 3521 | `	ph7_value *pArray;` |
|         - | 3522 | `	ph7_value *pVal;` |
|         - | 3523 | `	ph7_value sKey;` |
|        23 | 3524 | `	if( nArg < 1 ){` |
|         - | 3525 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3526 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3527 | `		return PH7_OK;` |
|         - | 3528 | `	}` |
|         - | 3529 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3530 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3531 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3532 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3533 | `		return PH7_OK;` |
|         - | 3534 | `	}` |
|         - | 3535 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3536 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3537 | `	if( pMap->pCur == 0 ){` |
|         - | 3538 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3539 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3540 | `		return PH7_OK;` |
|         - | 3541 | `	}` |
|        15 | 3542 | `	pCur = pMap->pCur;` |
|         - | 3543 | `	/* Create a new array */` |
|        15 | 3544 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3545 | `	if( pArray == 0 ){` |
|       ! 0 | 3546 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3547 | `		return PH7_OK;` |
|         - | 3548 | `	}` |
|        15 | 3549 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3550 | `	/* Insert the current value */` |
|        15 | 3551 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3552 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3553 | `	/* Make the key */` |
|        15 | 3554 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3555 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3556 | `	}else{` |
|         9 | 3557 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3558 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3559 | `	}` |
|         - | 3560 | `	/* Insert the current key */` |
|        15 | 3561 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3562 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3563 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3564 | `	/* Advance the cursor */` |
|        15 | 3565 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3566 | `	/* Return the current entry */` |
|        15 | 3567 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3568 | `	return PH7_OK;` |
|        12 | 3569 | `}` |
|         - | 3570 | `/*` |
|         - | 3571 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3572 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3573 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3574 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3575 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3576 | ` */` |
|         - | 3577 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3578 | `/*` |
|         - | 3579 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3580 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3581 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3582 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3583 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3584 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3585 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3586 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3587 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3588 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3589 | ` */` |
|         - | 3590 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3591 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3592 | `/*` |
|         - | 3593 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3594 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3595 | ` */` |
|       ! 0 | 3596 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       ! 0 | 3597 | `{` |
|       ! 0 | 3598 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 3599 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       ! 0 | 3600 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       ! 0 | 3601 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       ! 0 | 3602 | `		zBuf[n] = 0;` |
|       ! 0 | 3603 | `		return zBuf;` |
|         - | 3604 | `	}` |
|       ! 0 | 3605 | `	return ph7_type_name(pVal);` |
|       ! 0 | 3606 | `}` |
|         - | 3607 | `/*` |
|         - | 3608 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3609 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3610 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3611 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3612 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3613 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3614 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3615 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3616 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3617 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3618 | ` */` |
|       156 | 3619 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3620 | `{` |
|       157 | 3621 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3622 | `	sxu64 uVal = 0;` |
|       157 | 3623 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3624 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3625 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3626 | `		bNeg = (z[0] == '-');` |
|         3 | 3627 | `		z++;` |
|         1 | 3628 | `	}` |
|       237 | 3629 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3630 | `		int d = z[0] - '0';` |
|         - | 3631 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3632 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3633 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3634 | `			bOverflow = 1;` |
|       ! 0 | 3635 | `		}else{` |
|        81 | 3636 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3637 | `		}` |
|        81 | 3638 | `		bDigit = 1;` |
|        81 | 3639 | `		z++;` |
|         1 | 3640 | `	}` |
|       157 | 3641 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3642 | `		bReal = 1;` |
|         3 | 3643 | `		z++;` |
|         5 | 3644 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3645 | `			bDigit = 1;` |
|         3 | 3646 | `			z++;` |
|         1 | 3647 | `		}` |
|         1 | 3648 | `	}` |
|         - | 3649 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3650 | `	if( !bDigit ){` |
|        61 | 3651 | `		return RANGE_IN_ERROR;` |
|         - | 3652 | `	}` |
|         - | 3653 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3654 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3655 | `		z++;` |
|         9 | 3656 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3657 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3658 | `			return RANGE_IN_ERROR;` |
|         - | 3659 | `		}` |
|         9 | 3660 | `		bReal = 1;` |
|        17 | 3661 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3662 | `	}` |
|         - | 3663 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3664 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3665 | `	if( z != zEnd ){` |
|        13 | 3666 | `		return RANGE_IN_ERROR;` |
|         - | 3667 | `	}` |
|        84 | 3668 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3669 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3670 | `		bReal = 1;` |
|        84 | 3671 | `	}` |
|        43 | 3672 | `	if( bReal ){` |
|        11 | 3673 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3674 | `		return RANGE_IN_DOUBLE;` |
|         - | 3675 | `	}` |
|         - | 3676 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3677 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3678 | `	return RANGE_IN_LONG;` |
|        58 | 3679 | `}` |
|         - | 3680 | `/*` |
|         - | 3681 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3682 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3683 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3684 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3685 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3686 | ` */` |
|       328 | 3687 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3688 | `{` |
|         - | 3689 | `	char zMsg[160];` |
|       329 | 3690 | `	*pRc = PH7_OK;` |
|       329 | 3691 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3692 | `		char zType[80];` |
|       ! 0 | 3693 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3694 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       ! 0 | 3695 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3696 | `		return FALSE;` |
|         - | 3697 | `	}` |
|       329 | 3698 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3699 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3700 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3701 | `			iArg,zName);` |
|         5 | 3702 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3703 | `		*pbNullCoerced = TRUE;` |
|         2 | 3704 | `	}` |
|       329 | 3705 | `	return TRUE;` |
|       165 | 3706 | `}` |
|         - | 3707 | `/*` |
|         - | 3708 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3709 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3710 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3711 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3712 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3713 | ` */` |
|        60 | 3714 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3715 | `{` |
|        61 | 3716 | `	*pRc = PH7_OK;` |
|        61 | 3717 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3718 | `		char zType[80];` |
|       ! 0 | 3719 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3720 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       ! 0 | 3721 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3722 | `		return RANGE_IN_ERROR;` |
|         - | 3723 | `	}` |
|        61 | 3724 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3725 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3726 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3727 | `		*pLong = 0;` |
|         3 | 3728 | `		return RANGE_IN_LONG;` |
|         - | 3729 | `	}` |
|        59 | 3730 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3731 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3732 | `		return RANGE_IN_DOUBLE;` |
|         - | 3733 | `	}` |
|        35 | 3734 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3735 | `		const char *zStr;` |
|         - | 3736 | `		int nLen;` |
|         - | 3737 | `		sxu8 iKind;` |
|         3 | 3738 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3739 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3740 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3741 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3742 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3743 | `		}` |
|         3 | 3744 | `		return iKind;` |
|         - | 3745 | `	}` |
|         - | 3746 | `	/* int / bool */` |
|        33 | 3747 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3748 | `	return RANGE_IN_LONG;` |
|        31 | 3749 | `}` |
|         - | 3750 | `/*` |
|         - | 3751 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3752 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3753 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3754 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3755 | ` */` |
|       296 | 3756 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3757 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3758 | `{` |
|         - | 3759 | `	char zMsg[160];` |
|         - | 3760 | `	double r;` |
|       297 | 3761 | `	*pRc = PH7_OK;` |
|       297 | 3762 | `	if( bNullCoerced ){` |
|         - | 3763 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3764 | `		*pLong = 0;` |
|         5 | 3765 | `		*pDouble = 0.0;` |
|         5 | 3766 | `		return RANGE_IN_LONG;` |
|         - | 3767 | `	}` |
|       293 | 3768 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3769 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3770 | `check_dval:` |
|        25 | 3771 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3772 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3773 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3774 | `			return RANGE_IN_ERROR;` |
|         - | 3775 | `		}` |
|        21 | 3776 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3777 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3778 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3779 | `			return RANGE_IN_ERROR;` |
|         - | 3780 | `		}` |
|        17 | 3781 | `		*pDouble = r;` |
|        17 | 3782 | `		return RANGE_IN_DOUBLE;` |
|         - | 3783 | `	}` |
|       273 | 3784 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3785 | `		const char *zStr;` |
|         - | 3786 | `		int nLen;` |
|         - | 3787 | `		sxu8 iKind;` |
|        81 | 3788 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3789 | `		if( nLen == 0 ){` |
|         7 | 3790 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3791 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3792 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3793 | `			*pLong = 0;` |
|         5 | 3794 | `			*pDouble = 0.0;` |
|        41 | 3795 | `			return RANGE_IN_LONG;` |
|         - | 3796 | `		}` |
|        77 | 3797 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3798 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3799 | `			r = *pDouble;` |
|         5 | 3800 | `			goto check_dval;` |
|         - | 3801 | `		}` |
|        73 | 3802 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3803 | `			*pDouble = (double)*pLong;` |
|        23 | 3804 | `			if( nLen == 1 ){` |
|         - | 3805 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3806 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3807 | `				return RANGE_IN_DIGIT;` |
|         - | 3808 | `			}` |
|        15 | 3809 | `			return RANGE_IN_LONG;` |
|         - | 3810 | `		}` |
|        51 | 3811 | `		if( nLen != 1 ){` |
|        10 | 3812 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3813 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3814 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3815 | `		}` |
|        51 | 3816 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3817 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3818 | `		*pLong = 0;` |
|        51 | 3819 | `		*pDouble = 0.0;` |
|        51 | 3820 | `		return RANGE_IN_STRING;` |
|         - | 3821 | `	}` |
|         - | 3822 | `	/* int / bool */` |
|       193 | 3823 | `	*pLong = ph7_value_to_int64(pIn);` |
|       193 | 3824 | `	*pDouble = (double)*pLong;` |
|       193 | 3825 | `	return RANGE_IN_LONG;` |
|       149 | 3826 | `}` |
|         - | 3827 | `/*` |
|         - | 3828 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3829 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3830 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3831 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3832 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3833 | ` * exactly like php's two macros.` |
|         - | 3834 | ` */` |
|         6 | 3835 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3836 | `{` |
|        10 | 3837 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3838 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3839 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3840 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3841 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3842 | `}` |
|         6 | 3843 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3844 | `{` |
|         - | 3845 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3846 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3847 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3848 | `	const unsigned int nBuf = 1500;` |
|         7 | 3849 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3850 | `	if( zMsg == 0 ){` |
|       ! 0 | 3851 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3852 | `	}` |
|         7 | 3853 | `	snprintf(zMsg,nBuf,` |
|         - | 3854 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3855 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3856 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3857 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3858 | `}` |
|         - | 3859 | `/*` |
|         - | 3860 | ` * Set the element container to the next range element and append it to the` |
|         - | 3861 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3862 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3863 | ` * below stay one line per iteration.` |
|         - | 3864 | ` */` |
|      1680 | 3865 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3866 | `{` |
|      1681 | 3867 | `	ph7_value_int64(pValue,iVal);` |
|      1681 | 3868 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3869 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3870 | `	}` |
|      1681 | 3871 | `	return PH7_OK;` |
|       841 | 3872 | `}` |
|        70 | 3873 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3874 | `{` |
|        71 | 3875 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3876 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3877 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3878 | `	}` |
|        71 | 3879 | `	return PH7_OK;` |
|        36 | 3880 | `}` |
|       168 | 3881 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3882 | `{` |
|       169 | 3883 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3884 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3885 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3886 | `	}` |
|       169 | 3887 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3888 | `	return PH7_OK;` |
|        85 | 3889 | `}` |
|         - | 3890 | `/*` |
|         - | 3891 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3892 | ` *  Create an array containing a range of elements.` |
|         - | 3893 | ` * Return` |
|         - | 3894 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3895 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3896 | ` */` |
|       166 | 3897 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3898 | `{` |
|         - | 3899 | `	ph7_value *pValue,*pArray;` |
|       167 | 3900 | `	sxi32 rc = PH7_OK;` |
|       167 | 3901 | `	int is_step_double = 0,is_step_negative = 0;` |
|       167 | 3902 | `	double step_double = 1.0;` |
|       167 | 3903 | `	sxi64 step = 1;` |
|         - | 3904 | `	sxu8 start_type,end_type;` |
|       167 | 3905 | `	sxi64 start_long = 0,end_long = 0;` |
|       167 | 3906 | `	double start_double = 0.0,end_double = 0.0;` |
|       167 | 3907 | `	unsigned char cStart = 0,cEnd = 0;` |
|       167 | 3908 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3909 | `	sxu32 i,size;` |
|         - | 3910 |  |
|         - | 3911 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       167 | 3912 | `	if( nArg > 3 ){` |
|         4 | 3913 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3914 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3915 | `	}` |
|       165 | 3916 | `	if( nArg < 2 ){` |
|         - | 3917 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3918 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3919 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3920 | `	}` |
|         - | 3921 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3922 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       165 | 3923 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       ! 0 | 3924 | `		return rc;` |
|         - | 3925 | `	}` |
|       165 | 3926 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3927 | `		return rc;` |
|         - | 3928 | `	}` |
|       165 | 3929 | `	if( nArg > 2 ){` |
|        61 | 3930 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        61 | 3931 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         3 | 3932 | `			return rc;` |
|         - | 3933 | `		}` |
|        59 | 3934 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3935 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3936 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3937 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3938 | `			}` |
|        23 | 3939 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3940 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3941 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3942 | `			}` |
|         - | 3943 | `			/* We only want positive step values. */` |
|        21 | 3944 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3945 | `				is_step_negative = 1;` |
|       ! 0 | 3946 | `				step_double *= -1;` |
|       ! 0 | 3947 | `			}` |
|         - | 3948 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 3949 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 3950 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 3951 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 3952 | `				step = (sxi64)step_double;` |
|        19 | 3953 | `				if( (double)step != step_double ){` |
|        17 | 3954 | `					is_step_double = 1;` |
|         8 | 3955 | `				}` |
|        10 | 3956 | `			}else{` |
|         - | 3957 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 3958 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 3959 | `				is_step_double = 1;` |
|         - | 3960 | `			}` |
|        11 | 3961 | `		}else{` |
|         - | 3962 | `			/* We only want positive step values. */` |
|        35 | 3963 | `			if( step < 0 ){` |
|        11 | 3964 | `				if( step == SMALLEST_INT64 ){` |
|         - | 3965 | `					/* -step would overflow */` |
|         4 | 3966 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 3967 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 3968 | `				}` |
|         9 | 3969 | `				is_step_negative = 1;` |
|         9 | 3970 | `				step = -step;` |
|         4 | 3971 | `			}` |
|        33 | 3972 | `			step_double = (double)step;` |
|         - | 3973 | `		}` |
|        53 | 3974 | `		if( step_double == 0.0 ){` |
|         7 | 3975 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3976 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 3977 | `		}` |
|        23 | 3978 | `	}` |
|       151 | 3979 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       151 | 3980 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 3981 | `		return rc;` |
|         - | 3982 | `	}` |
|       147 | 3983 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       147 | 3984 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 3985 | `		return rc;` |
|         - | 3986 | `	}` |
|         - | 3987 | `	/* Element container + result array */` |
|       143 | 3988 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       143 | 3989 | `	pArray = ph7_context_new_array(pCtx);` |
|       143 | 3990 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 3991 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3992 | `	}` |
|         - | 3993 | `	/* If the range is given as strings, generate an array of characters. */` |
|       143 | 3994 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 3995 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 3996 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 3997 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 3998 | `			 * and the range is numeric. */` |
|        15 | 3999 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 4000 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 4001 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4002 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 4003 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 4004 | `				}` |
|         7 | 4005 | `				end_type = RANGE_IN_LONG;` |
|         4 | 4006 | `			}else{` |
|         9 | 4007 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 4008 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4009 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 4010 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 4011 | `				}` |
|         9 | 4012 | `				start_type = RANGE_IN_LONG;` |
|         - | 4013 | `			}` |
|        15 | 4014 | `			goto handle_numeric_inputs;` |
|         - | 4015 | `		}` |
|        23 | 4016 | `		if( is_step_double ){` |
|         - | 4017 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 4018 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 4019 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4020 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 4021 | `					" of characters, inputs converted to 0");` |
|         1 | 4022 | `			}` |
|         5 | 4023 | `			start_type = RANGE_IN_LONG;` |
|         5 | 4024 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4025 | `			goto handle_numeric_inputs;` |
|         - | 4026 | `		}` |
|         - | 4027 | `		/* Generate an array of characters */` |
|        19 | 4028 | `		if( cStart > cEnd ){` |
|         - | 4029 | `			/* Decreasing char range */` |
|         - | 4030 | `			int iCur;` |
|         3 | 4031 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4032 | `				goto boundary_error;` |
|         - | 4033 | `			}` |
|        17 | 4034 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4035 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4036 | `					return rc;` |
|         - | 4037 | `				}` |
|         8 | 4038 | `			}` |
|        18 | 4039 | `		}else if( cEnd > cStart ){` |
|         - | 4040 | `			/* Increasing char range */` |
|         - | 4041 | `			int iCur;` |
|        15 | 4042 | `			if( is_step_negative ){` |
|         3 | 4043 | `				goto negative_step_error;` |
|         - | 4044 | `			}` |
|        13 | 4045 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4046 | `				goto boundary_error;` |
|         - | 4047 | `			}` |
|       163 | 4048 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4049 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4050 | `					return rc;` |
|         - | 4051 | `				}` |
|        77 | 4052 | `			}` |
|         6 | 4053 | `		}else{` |
|         3 | 4054 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4055 | `				return rc;` |
|         - | 4056 | `			}` |
|         - | 4057 | `		}` |
|        15 | 4058 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4059 | `		return PH7_OK;` |
|         - | 4060 | `	}` |
|        53 | 4061 | `handle_numeric_inputs:` |
|       133 | 4062 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4063 | `		/* Float range */` |
|         - | 4064 | `		double elem,calc;` |
|        25 | 4065 | `		if( start_double > end_double ){` |
|         - | 4066 | `			/* Decreasing float range */` |
|         7 | 4067 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4068 | `				goto boundary_error;` |
|         - | 4069 | `			}` |
|         7 | 4070 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4071 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4072 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4073 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4074 | `			}` |
|         5 | 4075 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4076 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4077 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4078 | `					return rc;` |
|         - | 4079 | `				}` |
|         8 | 4080 | `			}` |
|        21 | 4081 | `		}else if( end_double > start_double ){` |
|         - | 4082 | `			/* Increasing float range */` |
|        17 | 4083 | `			if( is_step_negative ){` |
|       ! 0 | 4084 | `				goto negative_step_error;` |
|         - | 4085 | `			}` |
|        17 | 4086 | `			if( end_double - start_double < step_double ){` |
|         3 | 4087 | `				goto boundary_error;` |
|         - | 4088 | `			}` |
|        15 | 4089 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4090 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4091 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4092 | `			}` |
|        11 | 4093 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4094 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4095 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4096 | `					return rc;` |
|         - | 4097 | `				}` |
|        28 | 4098 | `			}` |
|         6 | 4099 | `		}else{` |
|         3 | 4100 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4101 | `				return rc;` |
|         - | 4102 | `			}` |
|         - | 4103 | `		}` |
|         9 | 4104 | `	}else{` |
|         - | 4105 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4106 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4107 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       101 | 4108 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4109 | `		sxu64 calc;` |
|       101 | 4110 | `		if( start_long > end_long ){` |
|         - | 4111 | `			/* Decreasing int range */` |
|        19 | 4112 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4113 | `				goto boundary_error;` |
|         - | 4114 | `			}` |
|        17 | 4115 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4116 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4117 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4118 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4119 | `			}` |
|        15 | 4120 | `			size = (sxu32)(calc + 1);` |
|       101 | 4121 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4122 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4123 | `					return rc;` |
|         - | 4124 | `				}` |
|        44 | 4125 | `			}` |
|        90 | 4126 | `		}else if( end_long > start_long ){` |
|         - | 4127 | `			/* Increasing int range */` |
|        77 | 4128 | `			if( is_step_negative ){` |
|         3 | 4129 | `				goto negative_step_error;` |
|         - | 4130 | `			}` |
|        75 | 4131 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4132 | `				goto boundary_error;` |
|         - | 4133 | `			}` |
|        73 | 4134 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        73 | 4135 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4136 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4137 | `			}` |
|        69 | 4138 | `			size = (sxu32)(calc + 1);` |
|      1657 | 4139 | `			for( i = 0 ; i < size ; ++i ){` |
|      1589 | 4140 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4141 | `					return rc;` |
|         - | 4142 | `				}` |
|       795 | 4143 | `			}` |
|        35 | 4144 | `		}else{` |
|         7 | 4145 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4146 | `				return rc;` |
|         - | 4147 | `			}` |
|         - | 4148 | `		}` |
|         - | 4149 | `	}` |
|         - | 4150 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4151 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       105 | 4152 | `	ph7_result_value(pCtx,pArray);` |
|       105 | 4153 | `	return PH7_OK;` |
|         2 | 4154 | `negative_step_error:` |
|         5 | 4155 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4156 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4157 | `boundary_error:` |
|         9 | 4158 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4159 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        84 | 4160 | `}` |
|         - | 4161 | `/*` |
|         - | 4162 | ` * array array_values(array $array)` |
|         - | 4163 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4164 | ` * Parameters` |
|         - | 4165 | ` *  $array` |
|         - | 4166 | ` *   The input array.` |
|         - | 4167 | ` * Return` |
|         - | 4168 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4169 | ` */` |
|        48 | 4170 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 4171 | `{` |
|         - | 4172 | `	ph7_hashmap_node *pNode;` |
|         - | 4173 | `	ph7_hashmap *pMap;` |
|         - | 4174 | `	ph7_value *pArray;` |
|         - | 4175 | `	ph7_value *pObj;` |
|         - | 4176 | `	sxu32 n;` |
|        51 | 4177 | `	if( nArg != 1 ){` |
|         - | 4178 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         4 | 4179 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4180 | `			"ArgumentCountError",` |
|         - | 4181 | `			"array_values() expects exactly 1 argument, %d given",` |
|         1 | 4182 | `			nArg` |
|         - | 4183 | `			);` |
|         - | 4184 | `	}` |
|         - | 4185 | `	/* Make sure we are dealing with a valid hashmap */` |
|        48 | 4186 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4187 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4188 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4189 | `			"TypeError",` |
|         - | 4190 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4191 | `			ph7_type_name(apArg[0])` |
|         - | 4192 | `			);` |
|         - | 4193 | `	}` |
|         - | 4194 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4195 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4196 | `	/* Create a new array */` |
|        46 | 4197 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4198 | `	if( pArray == 0 ){` |
|       ! 0 | 4199 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4200 | `		return PH7_OK;` |
|         - | 4201 | `	}` |
|         - | 4202 | `	/* Perform the requested operation */` |
|        46 | 4203 | `	pNode = pMap->pFirst;` |
|       144 | 4204 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4205 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4206 | `		if( pObj ){` |
|         - | 4207 | `			/* perform the insertion */` |
|       100 | 4208 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4209 | `		}` |
|         - | 4210 | `		/* Point to the next entry */` |
|       100 | 4211 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4212 | `	}` |
|         - | 4213 | `	/* return the new array */` |
|        46 | 4214 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4215 | `	return PH7_OK;` |
|        27 | 4216 | `}` |
|         - | 4217 | `/*` |
|         - | 4218 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4219 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4220 | ` * Parameters` |
|         - | 4221 | ` *  $input` |
|         - | 4222 | ` *   An array containing keys to return.` |
|         - | 4223 | ` * $search_value` |
|         - | 4224 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4225 | ` * $strict` |
|         - | 4226 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4227 | ` * Return` |
|         - | 4228 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4229 | ` */` |
|       160 | 4230 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4231 | `{` |
|         - | 4232 | `	ph7_hashmap_node *pNode;` |
|         - | 4233 | `	ph7_hashmap *pMap;` |
|         - | 4234 | `	ph7_value *pArray;` |
|         - | 4235 | `	ph7_value sObj;` |
|         - | 4236 | `	ph7_value sVal;` |
|         - | 4237 | `	SyString sKey;` |
|         - | 4238 | `	int bStrict;` |
|         - | 4239 | `	sxi32 rc;` |
|         - | 4240 | `	sxu32 n;` |
|       164 | 4241 | `	if( nArg < 1 ){` |
|         - | 4242 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4243 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4244 | `			"ArgumentCountError",` |
|         - | 4245 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4246 | `			);` |
|         - | 4247 | `	}` |
|         - | 4248 | `	/* Make sure we are dealing with a valid hashmap */` |
|       164 | 4249 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4250 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4251 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4252 | `			"TypeError",` |
|         - | 4253 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4254 | `			ph7_type_name(apArg[0])` |
|         - | 4255 | `			);` |
|         - | 4256 | `	}` |
|         - | 4257 | `	/* Point to the internal representation of the input hashmap */` |
|       161 | 4258 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4259 | `	/* Create a new array */` |
|       161 | 4260 | `	pArray = ph7_context_new_array(pCtx);` |
|       161 | 4261 | `	if( pArray == 0 ){` |
|       ! 0 | 4262 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4263 | `		return PH7_OK;` |
|         - | 4264 | `	}` |
|       161 | 4265 | `	bStrict = FALSE;` |
|       161 | 4266 | `	if( nArg > 2 ){` |
|         - | 4267 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         9 | 4268 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4269 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4270 | `				"TypeError",` |
|         - | 4271 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4272 | `				ph7_type_name(apArg[2])` |
|         - | 4273 | `				);` |
|         - | 4274 | `		}` |
|         9 | 4275 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4276 | `	}` |
|         - | 4277 | `	/* Perform the requested operation */` |
|       161 | 4278 | `	pNode = pMap->pFirst;` |
|       161 | 4279 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1463 | 4280 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1305 | 4281 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       185 | 4282 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        94 | 4283 | `		}else{` |
|      1122 | 4284 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1122 | 4285 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4286 | `		}` |
|      1305 | 4287 | `		rc = 0;` |
|      1305 | 4288 | `		if( nArg > 1 ){` |
|        65 | 4289 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4290 | `			if( pValue ){` |
|         - | 4291 | `				ph7_value sNeedle;` |
|        65 | 4292 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4293 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4294 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4295 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4296 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4297 | `				 * corrupt every later comparison. */` |
|        65 | 4298 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4299 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4300 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4301 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4302 | `			}` |
|        32 | 4303 | `		}` |
|      1305 | 4304 | `		if( rc == 0 ){` |
|         - | 4305 | `			/* Perform the insertion */` |
|      1273 | 4306 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       635 | 4307 | `		}` |
|      1305 | 4308 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4309 | `		/* Point to the next entry */` |
|      1305 | 4310 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       654 | 4311 | `	}` |
|         - | 4312 | `	/* return the new array */` |
|       161 | 4313 | `	ph7_result_value(pCtx,pArray);` |
|       161 | 4314 | `	return PH7_OK;` |
|        84 | 4315 | `}` |
|         - | 4316 | `/*` |
|         - | 4317 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4318 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4319 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4320 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4321 | ` * Parameters` |
|         - | 4322 | ` *  $arr1` |
|         - | 4323 | ` *   First array` |
|         - | 4324 | ` *  $arr2` |
|         - | 4325 | ` *   Second array` |
|         - | 4326 | ` * Return` |
|         - | 4327 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4328 | ` * Note` |
|         - | 4329 | ` *  This function is a symisc eXtension.` |
|         - | 4330 | ` */` |
|         4 | 4331 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4332 | `{` |
|         - | 4333 | `	ph7_hashmap *p1,*p2;` |
|         - | 4334 | `	int rc;` |
|         5 | 4335 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4336 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4337 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4338 | `		return PH7_OK;` |
|         - | 4339 | `	}` |
|         - | 4340 | `	/* Point to the hashmaps */` |
|         5 | 4341 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4342 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4343 | `	rc = (p1 == p2);` |
|         - | 4344 | `	/* Same instance? */` |
|         5 | 4345 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4346 | `	return PH7_OK;` |
|         3 | 4347 | `}` |
|         - | 4348 | `/*` |
|         - | 4349 | ` * array array_merge(array ...$arrays)` |
|         - | 4350 | ` *  Merge one or more arrays.` |
|         - | 4351 | ` * Parameters` |
|         - | 4352 | ` *  ...$arrays` |
|         - | 4353 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4354 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4355 | ` * Return` |
|         - | 4356 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4357 | ` *  with no arguments.` |
|         - | 4358 | ` */` |
|      1056 | 4359 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4360 | `{` |
|         - | 4361 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4362 | `	ph7_value *pArray;` |
|         - | 4363 | `	int i;` |
|         - | 4364 | `	/* Create a new array */` |
|      1061 | 4365 | `	pArray = ph7_context_new_array(pCtx);` |
|      1061 | 4366 | `	if( pArray == 0 ){` |
|       ! 0 | 4367 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4368 | `		return PH7_OK;` |
|         - | 4369 | `	}` |
|         - | 4370 | `	/* Point to the internal representation of the hashmap */` |
|      1061 | 4371 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4372 | `	/* Start merging */` |
|      3163 | 4373 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4374 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2111 | 4375 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4376 | `			/* Type mismatch -> TypeError */` |
|         8 | 4377 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4378 | `				"TypeError",` |
|         - | 4379 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4380 | `				i + 1,` |
|         4 | 4381 | `				ph7_type_name(apArg[i])` |
|         - | 4382 | `				);` |
|       ! 0 | 4383 | `		}else{` |
|      2107 | 4384 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4385 | `			/* Merge the two hashmaps */` |
|      2107 | 4386 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4387 | `		}` |
|      1056 | 4388 | `	}` |
|         - | 4389 | `	/* Return the freshly created array */` |
|      1057 | 4390 | `	ph7_result_value(pCtx,pArray);` |
|      1057 | 4391 | `	return PH7_OK;` |
|       533 | 4392 | `}` |
|         - | 4393 | `/*` |
|         - | 4394 | ` * array array_copy(array $source)` |
|         - | 4395 | ` *  Make a blind copy of the target array.` |
|         - | 4396 | ` * Parameters` |
|         - | 4397 | ` *  $source` |
|         - | 4398 | ` *   Target array` |
|         - | 4399 | ` * Return` |
|         - | 4400 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4401 | ` * Note` |
|         - | 4402 | ` *  This function is a symisc eXtension.` |
|         - | 4403 | ` */` |
|        18 | 4404 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4405 | `{` |
|         - | 4406 | `	ph7_hashmap *pMap;` |
|         - | 4407 | `	ph7_value *pArray;` |
|        19 | 4408 | `	if( nArg < 1 ){` |
|         - | 4409 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4410 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4411 | `		return PH7_OK;` |
|         - | 4412 | `	}` |
|         - | 4413 | `	/* Create a new array */` |
|        19 | 4414 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4415 | `	if( pArray == 0 ){` |
|       ! 0 | 4416 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4417 | `		return PH7_OK;` |
|         - | 4418 | `	}` |
|         - | 4419 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4420 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4421 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4422 | `		/* Point to the internal representation of the source */` |
|        19 | 4423 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4424 | `		/* Perform the copy */` |
|        19 | 4425 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4426 | `	}else{` |
|         - | 4427 | `		/* Simple insertion */` |
|       ! 0 | 4428 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4429 | `	}` |
|         - | 4430 | `	/* Return the duplicated array */` |
|        19 | 4431 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4432 | `	return PH7_OK;` |
|        10 | 4433 | `}` |
|         - | 4434 | `/*` |
|         - | 4435 | ` * bool array_erase(array $source)` |
|         - | 4436 | ` *  Remove all elements from a given array.` |
|         - | 4437 | ` * Parameters` |
|         - | 4438 | ` *  $source` |
|         - | 4439 | ` *   Target array` |
|         - | 4440 | ` * Return` |
|         - | 4441 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4442 | ` * Note` |
|         - | 4443 | ` *  This function is a symisc eXtension.` |
|         - | 4444 | ` */` |
|        26 | 4445 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4446 | `{` |
|         - | 4447 | `	ph7_hashmap *pMap;` |
|        28 | 4448 | `	if( nArg < 1 ){` |
|         - | 4449 | `		/* Missing arguments */` |
|       ! 0 | 4450 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4451 | `		return PH7_OK;` |
|         - | 4452 | `	}` |
|         - | 4453 | `	/* Point to the target hashmap */` |
|        28 | 4454 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4455 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4456 | `	/* Erase */` |
|        28 | 4457 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4458 | `	return PH7_OK;` |
|        15 | 4459 | `}` |
|         - | 4460 | `/*` |
|         - | 4461 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4462 | ` *  Extract a slice of the array.` |
|         - | 4463 | ` * Parameters` |
|         - | 4464 | ` *  $array` |
|         - | 4465 | ` *    The input array.` |
|         - | 4466 | ` * $offset` |
|         - | 4467 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4468 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4469 | ` * $length (optional, nullable)` |
|         - | 4470 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4471 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4472 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4473 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4474 | ` * $preserve_keys (optional)` |
|         - | 4475 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4476 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4477 | ` * Return` |
|         - | 4478 | ` *   The new slice.` |
|         - | 4479 | ` */` |
|        46 | 4480 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4481 | `{` |
|         - | 4482 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4483 | `	ph7_hashmap_node *pCur;` |
|         - | 4484 | `	ph7_value *pArray;` |
|         - | 4485 | `	int iLength,iOfft;` |
|         - | 4486 | `	int bPreserve;` |
|         - | 4487 | `	sxi32 rc;` |
|        51 | 4488 | `	if( nArg < 2 ){` |
|       ! 0 | 4489 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4490 | `			"ArgumentCountError",` |
|         - | 4491 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4492 | `			nArg` |
|         - | 4493 | `			);` |
|         - | 4494 | `	}` |
|        51 | 4495 | `	if( nArg > 4 ){` |
|         4 | 4496 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4497 | `			"ArgumentCountError",` |
|         - | 4498 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4499 | `			nArg` |
|         - | 4500 | `			);` |
|         - | 4501 | `	}` |
|        49 | 4502 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4503 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4504 | `			"TypeError",` |
|         - | 4505 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4506 | `			ph7_type_name(apArg[0])` |
|         - | 4507 | `			);` |
|         - | 4508 | `	}` |
|         - | 4509 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        62 | 4510 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        65 | 4511 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4512 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4513 | `			"TypeError",` |
|         - | 4514 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4515 | `			ph7_type_name(apArg[1])` |
|         - | 4516 | `			);` |
|         - | 4517 | `	}` |
|         - | 4518 | `	/* Validate $length type if provided: nullable int */` |
|        45 | 4519 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        26 | 4520 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        26 | 4521 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4522 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4523 | `				"TypeError",` |
|         - | 4524 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4525 | `				ph7_type_name(apArg[2])` |
|         - | 4526 | `				);` |
|         - | 4527 | `		}` |
|         8 | 4528 | `	}` |
|         - | 4529 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        43 | 4530 | `	if( nArg > 3 ){` |
|         7 | 4531 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4532 | `			ph7_value_is_resource(apArg[3]) ){` |
|       ! 0 | 4533 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4534 | `				"TypeError",` |
|         - | 4535 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 4536 | `				ph7_type_name(apArg[3])` |
|         - | 4537 | `				);` |
|         - | 4538 | `		}` |
|         2 | 4539 | `	}` |
|         - | 4540 | `	/* Point the internal representation of the target array */` |
|        43 | 4541 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        43 | 4542 | `	bPreserve = FALSE;` |
|         - | 4543 | `	/* Get the offset */` |
|         - | 4544 | `	{` |
|        43 | 4545 | `		sxi64 iTmp = 0;` |
|        43 | 4546 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|        43 | 4547 | `		if( rcArg != PH7_OK ){` |
|       ! 0 | 4548 | `			return rcArg;` |
|         - | 4549 | `		}` |
|        43 | 4550 | `		iOfft = (int)iTmp;` |
|         - | 4551 | `	}` |
|        43 | 4552 | `	if( iOfft < 0 ){` |
|         5 | 4553 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4554 | `		if( iOfft < 0 ){` |
|         3 | 4555 | `			iOfft = 0;` |
|         1 | 4556 | `		}` |
|         2 | 4557 | `	}` |
|        43 | 4558 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4559 | `		/* Offset past end of array, return empty array */` |
|         5 | 4560 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4561 | `		if( pArray == 0 ){` |
|       ! 0 | 4562 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4563 | `			return PH7_OK;` |
|         - | 4564 | `		}` |
|         5 | 4565 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4566 | `		return PH7_OK;` |
|         - | 4567 | `	}` |
|         - | 4568 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        39 | 4569 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        39 | 4570 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        17 | 4571 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        17 | 4572 | `		if( iLength < 0 ){` |
|         5 | 4573 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4574 | `		}` |
|        17 | 4575 | `		if( iLength < 0 ){` |
|         3 | 4576 | `			iLength = 0;` |
|         1 | 4577 | `		}` |
|        17 | 4578 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4579 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4580 | `		}` |
|         8 | 4581 | `	}` |
|        39 | 4582 | `	if( nArg > 3 ){` |
|         5 | 4583 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4584 | `	}` |
|         - | 4585 | `	/* Create a new array */` |
|        39 | 4586 | `	pArray = ph7_context_new_array(pCtx);` |
|        39 | 4587 | `	if( pArray == 0 ){` |
|       ! 0 | 4588 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4589 | `		return PH7_OK;` |
|         - | 4590 | `	}` |
|        39 | 4591 | `	if( iLength < 1 ){` |
|         - | 4592 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4593 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4594 | `		return PH7_OK;` |
|         - | 4595 | `	}` |
|         - | 4596 | `	/* Point to the desired entry */` |
|        35 | 4597 | `	pCur = pSrc->pFirst;` |
|        29 | 4598 | `	for(;;){` |
|        63 | 4599 | `		if( iOfft < 1 ){` |
|        35 | 4600 | `			break;` |
|         - | 4601 | `		}` |
|         - | 4602 | `		/* Point to the next entry */` |
|        33 | 4603 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4604 | `		iOfft--;` |
|         5 | 4605 | `	}` |
|         - | 4606 | `	/* Point to the internal representation of the hashmap */` |
|        35 | 4607 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        54 | 4608 | `	for(;;){` |
|       113 | 4609 | `		if( iLength < 1 ){` |
|        35 | 4610 | `			break;` |
|         - | 4611 | `		}` |
|         - | 4612 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4613 | `		{` |
|        83 | 4614 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        83 | 4615 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4616 | `		}` |
|        83 | 4617 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4618 | `			break;` |
|         - | 4619 | `		}` |
|         - | 4620 | `		/* Point to the next entry */` |
|        83 | 4621 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        83 | 4622 | `		iLength--;` |
|         5 | 4623 | `	}` |
|         - | 4624 | `	/* Return the freshly created array */` |
|        35 | 4625 | `	ph7_result_value(pCtx,pArray);` |
|        35 | 4626 | `	return PH7_OK;` |
|        28 | 4627 | `}` |
|         - | 4628 | `/*` |
|         - | 4629 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4630 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4631 | ` * beginning (becomes the new pFirst).` |
|         - | 4632 | ` */` |
|        38 | 4633 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4634 | `{` |
|         - | 4635 | `	ph7_hashmap_node *pNode;` |
|         - | 4636 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4637 | `	pNode = pMap->pLast;` |
|        39 | 4638 | `	if( pNode == 0 ){` |
|       ! 0 | 4639 | `		return;` |
|         - | 4640 | `	}` |
|        39 | 4641 | `	if( pNode->pNext == 0 ){` |
|         - | 4642 | `		/* Only node in the list, nothing to move */` |
|         5 | 4643 | `		return;` |
|         - | 4644 | `	}` |
|        35 | 4645 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4646 | `		/* Already in the correct position */` |
|         9 | 4647 | `		return;` |
|         - | 4648 | `	}` |
|         - | 4649 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4650 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4651 | `	pMap->pLast->pPrev = 0;` |
|         - | 4652 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4653 | `	if( pAfter == 0 ){` |
|         - | 4654 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4655 | `		pNode->pNext = 0;` |
|         3 | 4656 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4657 | `		if( pMap->pFirst ){` |
|         3 | 4658 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4659 | `		}` |
|         3 | 4660 | `		pMap->pFirst = pNode;` |
|         2 | 4661 | `	}else{` |
|        25 | 4662 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4663 | `		pNode->pPrev = pOldNext;` |
|        25 | 4664 | `		pNode->pNext = pAfter;` |
|        25 | 4665 | `		pAfter->pPrev = pNode;` |
|        25 | 4666 | `		if( pOldNext ){` |
|        25 | 4667 | `			pOldNext->pNext = pNode;` |
|        13 | 4668 | `		}else{` |
|       ! 0 | 4669 | `			pMap->pLast = pNode;` |
|         - | 4670 | `		}` |
|         - | 4671 | `	}` |
|        20 | 4672 | `}` |
|         - | 4673 | `/*` |
|         - | 4674 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4675 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4676 | ` * Parameters` |
|         - | 4677 | ` *  $array` |
|         - | 4678 | ` *    The input array.` |
|         - | 4679 | ` *  $offset` |
|         - | 4680 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4681 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4682 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4683 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4684 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4685 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4686 | ` *  $length (optional)` |
|         - | 4687 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4688 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4689 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4690 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4691 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4692 | ` *  $replacement (optional)` |
|         - | 4693 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4694 | ` *    with elements from this array.` |
|         - | 4695 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4696 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4697 | ` *    offset.` |
|         - | 4698 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4699 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4700 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4701 | ` * Return` |
|         - | 4702 | ` *   A new array consisting of the extracted elements.` |
|         - | 4703 | ` */` |
|        64 | 4704 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4705 | `{` |
|         - | 4706 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4707 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4708 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4709 | `	int iLength,iOfft,i;` |
|         - | 4710 | `	sxi32 rc;` |
|        66 | 4711 | `	if( nArg < 2 ){` |
|       ! 0 | 4712 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4713 | `			"ArgumentCountError",` |
|         - | 4714 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4715 | `			nArg` |
|         - | 4716 | `			);` |
|         - | 4717 | `	}` |
|        66 | 4718 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4719 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4720 | `			"TypeError",` |
|         - | 4721 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4722 | `			ph7_type_name(apArg[0])` |
|         - | 4723 | `			);` |
|         - | 4724 | `	}` |
|         - | 4725 | `	/* Point to the internal representation of the target array */` |
|        63 | 4726 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4727 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4728 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4729 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4730 | `	if( iOfft < 0 ){` |
|         9 | 4731 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4732 | `		if( iOfft < 0 ){` |
|         3 | 4733 | `			iOfft = 0;` |
|         2 | 4734 | `		}` |
|        59 | 4735 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4736 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4737 | `	}` |
|         - | 4738 | `	/* Get the length and clamp to valid range.` |
|         - | 4739 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4740 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4741 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4742 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4743 | `		if( iLength < 0 ){` |
|         7 | 4744 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4745 | `			if( iLength < 0 ){` |
|         3 | 4746 | `				iLength = 0;` |
|         1 | 4747 | `			}` |
|         3 | 4748 | `		}` |
|        45 | 4749 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4750 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4751 | `		}` |
|        22 | 4752 | `	}` |
|         - | 4753 | `	/* Create the result array for removed elements */` |
|        63 | 4754 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4755 | `	if( pArray == 0 ){` |
|       ! 0 | 4756 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4757 | `		return PH7_OK;` |
|         - | 4758 | `	}` |
|         - | 4759 | `	/* Get replacement array if provided */` |
|        63 | 4760 | `	pRep = 0;` |
|        63 | 4761 | `	if( nArg > 3 ){` |
|        27 | 4762 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4763 | `			/* Perform an array cast */` |
|         3 | 4764 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4765 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4766 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4767 | `			}` |
|         2 | 4768 | `		}else{` |
|        25 | 4769 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4770 | `		}` |
|        27 | 4771 | `		if( pRep ){` |
|         - | 4772 | `			/* Reset the loop cursor */` |
|        27 | 4773 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4774 | `		}` |
|        13 | 4775 | `	}` |
|         - | 4776 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4777 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4778 | `	/* Navigate to the offset position */` |
|        63 | 4779 | `	pCur = pSrc->pFirst;` |
|       131 | 4780 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4781 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4782 | `	}` |
|         - | 4783 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4784 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4785 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4786 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4787 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4788 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4789 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4790 | `		pPrev = pCur->pPrev;` |
|        79 | 4791 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4792 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4793 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4794 | `			break;` |
|         - | 4795 | `		}` |
|        79 | 4796 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4797 | `	}` |
|         - | 4798 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4799 | `	if( pRep ){` |
|         - | 4800 | `		ph7_value sSafeVal;` |
|        78 | 4801 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4802 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4803 | `			if( pRvalue ){` |
|         - | 4804 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4805 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4806 | `				 * since it points into that same pool. */` |
|        39 | 4807 | `				sSafeVal = *pRvalue;` |
|        39 | 4808 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4809 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4810 | `					pNewNode = pSrc->pLast;` |
|        39 | 4811 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4812 | `					pInsertAfter = pNewNode;` |
|        19 | 4813 | `				}` |
|        19 | 4814 | `			}` |
|         1 | 4815 | `		}` |
|        13 | 4816 | `	}` |
|         - | 4817 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4818 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4819 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4820 | `	 * and removals left gaps. */` |
|         - | 4821 | `	{` |
|        63 | 4822 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4823 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4824 | `		pSrc->iNextIdx = 0;` |
|       233 | 4825 | `		while( n > 0 ){` |
|       171 | 4826 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4827 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4828 | `			}` |
|       171 | 4829 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4830 | `			n--;` |
|         1 | 4831 | `		}` |
|        63 | 4832 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4833 | `	}` |
|         - | 4834 | `	/* Return the freshly created array */` |
|        63 | 4835 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4836 | `	return PH7_OK;` |
|        34 | 4837 | `}` |
|         - | 4838 | `/*` |
|         - | 4839 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4840 | ` *  Checks if a value exists in an array.` |
|         - | 4841 | ` * Parameters` |
|         - | 4842 | ` *  $needle` |
|         - | 4843 | ` *   The searched value.` |
|         - | 4844 | ` *   Note:` |
|         - | 4845 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4846 | ` * $haystack` |
|         - | 4847 | ` *  The target array.` |
|         - | 4848 | ` * $strict` |
|         - | 4849 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4850 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4851 | ` */` |
|     32842 | 4852 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4853 | `{` |
|         - | 4854 | `	ph7_value *pNeedle;` |
|         - | 4855 | `	int bStrict;` |
|         - | 4856 | `	int rc;` |
|     32847 | 4857 | `	if( nArg < 2 ){` |
|         - | 4858 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4859 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4860 | `		return PH7_OK;` |
|         - | 4861 | `	}` |
|     32847 | 4862 | `	pNeedle = apArg[0];` |
|     32847 | 4863 | `	bStrict = 0;` |
|     32847 | 4864 | `	if( nArg > 2 ){` |
|        53 | 4865 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4866 | `	}` |
|     32847 | 4867 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4868 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4869 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4870 | `		/* Set the comparison result */` |
|       ! 0 | 4871 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4872 | `		return PH7_OK;` |
|         - | 4873 | `	}` |
|         - | 4874 | `	/* Perform the lookup */` |
|     32847 | 4875 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4876 | `	/* Lookup result */` |
|     32847 | 4877 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     32847 | 4878 | `	return PH7_OK;` |
|     16426 | 4879 | `}` |
|         - | 4880 | `/*` |
|         - | 4881 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4882 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4883 | ` * Parameters` |
|         - | 4884 | ` * $needle` |
|         - | 4885 | ` *   The searched value.` |
|         - | 4886 | ` * $haystack` |
|         - | 4887 | ` *   The array.` |
|         - | 4888 | ` * $strict` |
|         - | 4889 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4890 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4891 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4892 | ` * Return` |
|         - | 4893 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4894 | ` */` |
|        26 | 4895 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4896 | `{` |
|         - | 4897 | `	ph7_hashmap_node *pEntry;` |
|         - | 4898 | `	ph7_value *pVal,sNeedle;` |
|         - | 4899 | `	ph7_hashmap *pMap;` |
|         - | 4900 | `	ph7_value sVal;` |
|         - | 4901 | `	int bStrict;` |
|         - | 4902 | `	sxu32 n;` |
|         - | 4903 | `	int rc;` |
|        28 | 4904 | `	if( nArg < 2 ){` |
|         - | 4905 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4906 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4907 | `			"ArgumentCountError",` |
|         - | 4908 | `			"array_search() expects at least 2 arguments, %d given",` |
|       ! 0 | 4909 | `			nArg` |
|         - | 4910 | `			);` |
|         - | 4911 | `	}` |
|        28 | 4912 | `	bStrict = FALSE;` |
|        28 | 4913 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4914 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4915 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4916 | `			"TypeError",` |
|         - | 4917 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4918 | `			ph7_type_name(apArg[1])` |
|         - | 4919 | `			);` |
|         - | 4920 | `	}` |
|        25 | 4921 | `	if( nArg > 2 ){` |
|         - | 4922 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        11 | 4923 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4924 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4925 | `				"TypeError",` |
|         - | 4926 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4927 | `				ph7_type_name(apArg[2])` |
|         - | 4928 | `				);` |
|         - | 4929 | `		}` |
|        11 | 4930 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4931 | `	}` |
|         - | 4932 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4933 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4934 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4935 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 4936 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 4937 | `	pEntry = pMap->pFirst;` |
|        25 | 4938 | `	n = pMap->nEntry;` |
|        28 | 4939 | `	for(;;){` |
|        57 | 4940 | `		if( !n ){` |
|         9 | 4941 | `			break;` |
|         - | 4942 | `		}` |
|         - | 4943 | `		/* Extract node value */` |
|        49 | 4944 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 4945 | `		if( pVal ){` |
|         - | 4946 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4947 | `			 * can change their type.` |
|         - | 4948 | `			 */` |
|        49 | 4949 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 4950 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 4951 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 4952 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 4953 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 4954 | `			if( rc == 0 ){` |
|         - | 4955 | `				/* Match found,return key */` |
|        17 | 4956 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4957 | `					/* INT key */` |
|        11 | 4958 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 4959 | `				}else{` |
|         7 | 4960 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4961 | `					/* Blob key */` |
|         7 | 4962 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4963 | `				}` |
|        17 | 4964 | `				return PH7_OK;` |
|         - | 4965 | `			}` |
|        16 | 4966 | `		}` |
|         - | 4967 | `		/* Point to the next entry */` |
|        33 | 4968 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 4969 | `		n--;` |
|         1 | 4970 | `	}` |
|         - | 4971 | `	/* No such value,return FALSE */` |
|         9 | 4972 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4973 | `	return PH7_OK;` |
|        15 | 4974 | `}` |
|         - | 4975 | `/*` |
|         - | 4976 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4977 | ` *  Computes the difference of arrays.` |
|         - | 4978 | ` * Parameters` |
|         - | 4979 | ` *  $array1` |
|         - | 4980 | ` *    The array to compare from` |
|         - | 4981 | ` *  $array2` |
|         - | 4982 | ` *    An array to compare against` |
|         - | 4983 | ` *  $...` |
|         - | 4984 | ` *   More arrays to compare against` |
|         - | 4985 | ` * Return` |
|         - | 4986 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4987 | ` *  are not present in any of the other arrays.` |
|         - | 4988 | ` */` |
|        20 | 4989 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 4990 | `{` |
|         - | 4991 | `	ph7_hashmap_node *pEntry;` |
|         - | 4992 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4993 | `	ph7_value *pArray;` |
|         - | 4994 | `	ph7_value *pVal;` |
|         - | 4995 | `	sxi32 rc;` |
|         - | 4996 | `	sxu32 n;` |
|         - | 4997 | `	int i;` |
|         - | 4998 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 4999 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 5000 | `	 * debugging difficult. */` |
|        23 | 5001 | `	if( nArg < 1 ){` |
|       ! 0 | 5002 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5003 | `			"ArgumentCountError",` |
|         - | 5004 | `			"array_diff() expects at least 1 argument, %d given",` |
|       ! 0 | 5005 | `			nArg` |
|         - | 5006 | `			);` |
|         - | 5007 | `	}` |
|        23 | 5008 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5009 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5010 | `			"TypeError",` |
|         - | 5011 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5012 | `			ph7_type_name(apArg[0])` |
|         - | 5013 | `			);` |
|         - | 5014 | `	}` |
|        36 | 5015 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 5016 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5017 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5018 | `				"TypeError",` |
|         - | 5019 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 5020 | `				i + 1,` |
|         2 | 5021 | `				ph7_type_name(apArg[i])` |
|         - | 5022 | `				);` |
|         - | 5023 | `		}` |
|         9 | 5024 | `	}` |
|        17 | 5025 | `	if( nArg == 1 ){` |
|         - | 5026 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5027 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5028 | `		return PH7_OK;` |
|         - | 5029 | `	}` |
|         - | 5030 | `	/* Create a new array */` |
|        15 | 5031 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5032 | `	if( pArray == 0 ){` |
|       ! 0 | 5033 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5034 | `		return PH7_OK;` |
|         - | 5035 | `	}` |
|         - | 5036 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5037 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5038 | `	/* Perform the diff */` |
|        15 | 5039 | `	pEntry = pSrc->pFirst;` |
|        15 | 5040 | `	n = pSrc->nEntry;` |
|        27 | 5041 | `	for(;;){` |
|        55 | 5042 | `		if( n < 1 ){` |
|        15 | 5043 | `			break;` |
|         - | 5044 | `		}` |
|         - | 5045 | `		/* Extract the node value */` |
|        41 | 5046 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5047 | `		if( pVal ){` |
|        69 | 5048 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5049 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5050 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5051 | `				/* Perform the lookup */` |
|        45 | 5052 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5053 | `				if( rc == SXRET_OK ){` |
|         - | 5054 | `					/* Value exist */` |
|        17 | 5055 | `					break;` |
|         - | 5056 | `				}` |
|        15 | 5057 | `			}` |
|        41 | 5058 | `			if( i >= nArg ){` |
|         - | 5059 | `				/* Perform the insertion */` |
|        25 | 5060 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5061 | `			}` |
|        20 | 5062 | `		}` |
|         - | 5063 | `		/* Point to the next entry */` |
|        41 | 5064 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5065 | `		n--;` |
|         1 | 5066 | `	}` |
|         - | 5067 | `	/* Return the freshly created array */` |
|        15 | 5068 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5069 | `	return PH7_OK;` |
|        13 | 5070 | `}` |
|         - | 5071 | `/*` |
|         - | 5072 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5073 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5074 | ` * Parameters` |
|         - | 5075 | ` *  $array1` |
|         - | 5076 | ` *    The array to compare from` |
|         - | 5077 | ` *  $array2` |
|         - | 5078 | ` *    An array to compare against` |
|         - | 5079 | ` *  $...` |
|         - | 5080 | ` *   More arrays to compare against.` |
|         - | 5081 | ` * $callback` |
|         - | 5082 | ` *  The callback comparison function.` |
|         - | 5083 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5084 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5085 | ` *  than the second.` |
|         - | 5086 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5087 | ` * Return` |
|         - | 5088 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5089 | ` *  are not present in any of the other arrays.` |
|         - | 5090 | ` */` |
|        20 | 5091 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5092 | `{` |
|         - | 5093 | `	ph7_hashmap_node *pEntry;` |
|         - | 5094 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5095 | `	ph7_value *pCallback;` |
|         - | 5096 | `	ph7_value *pArray;` |
|         - | 5097 | `	ph7_value *pVal;` |
|         - | 5098 | `	sxi32 rc;` |
|         - | 5099 | `	sxu32 n;` |
|         - | 5100 | `	int i;` |
|         - | 5101 |  |
|         - | 5102 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        25 | 5103 | `	if( nArg < 2 ){` |
|       ! 0 | 5104 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5105 | `			"ArgumentCountError",` |
|         - | 5106 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       ! 0 | 5107 | `			nArg` |
|         - | 5108 | `			);` |
|         - | 5109 | `	}` |
|        25 | 5110 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5111 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5112 | `			"TypeError",` |
|         - | 5113 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5114 | `			ph7_type_name(apArg[0])` |
|         - | 5115 | `			);` |
|         - | 5116 | `	}` |
|         - | 5117 |  |
|        23 | 5118 | `	if( nArg == 2 ){` |
|         - | 5119 | `		/* Only the original array and the callback were provided. */` |
|         - | 5120 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5121 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5122 | `		 * validation order.` |
|         - | 5123 | `		 */` |
|         4 | 5124 | `	} else {` |
|         - | 5125 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5126 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5127 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5128 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5129 | `					"TypeError",` |
|         - | 5130 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5131 | `					i + 1,` |
|         6 | 5132 | `					ph7_type_name(apArg[i])` |
|         - | 5133 | `					);` |
|         - | 5134 | `			}` |
|         7 | 5135 | `		}` |
|         - | 5136 | `	}` |
|         - | 5137 |  |
|         - | 5138 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5139 | `	pCallback = apArg[nArg - 1];` |
|         - | 5140 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5141 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5142 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5143 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5144 | `				"TypeError",` |
|         - | 5145 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5146 | `				nArg` |
|         - | 5147 | `				);` |
|         - | 5148 | `		}` |
|         6 | 5149 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5150 | `			int len;` |
|         3 | 5151 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5152 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5153 | `				"TypeError",` |
|         - | 5154 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5155 | `				nArg,` |
|         1 | 5156 | `				zName` |
|         - | 5157 | `				);` |
|         - | 5158 | `		}` |
|         4 | 5159 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5160 | `			"TypeError",` |
|         - | 5161 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5162 | `			nArg` |
|         - | 5163 | `			);` |
|         - | 5164 | `	}` |
|         - | 5165 |  |
|         7 | 5166 | `	if( nArg == 2 ){` |
|         - | 5167 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5168 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5169 | `		return PH7_OK;` |
|         - | 5170 | `	}` |
|         - | 5171 |  |
|         - | 5172 | `	/* Create a new array */` |
|         5 | 5173 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5174 | `	if( pArray == 0 ){` |
|       ! 0 | 5175 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5176 | `		return PH7_OK;` |
|         - | 5177 | `	}` |
|         - | 5178 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5179 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5180 | `	/* Perform the diff */` |
|         5 | 5181 | `	pEntry = pSrc->pFirst;` |
|         5 | 5182 | `	n = pSrc->nEntry;` |
|         5 | 5183 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5184 | `	for(;;){` |
|        11 | 5185 | `		if( n < 1 ){` |
|         3 | 5186 | `			break;` |
|         - | 5187 | `		}` |
|         - | 5188 | `		/* Extract the node value */` |
|         9 | 5189 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5190 | `		if( pVal ){` |
|        15 | 5191 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5192 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5193 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5194 | `				/* Perform the lookup */` |
|         9 | 5195 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5196 | `				if( rc == SXRET_OK ){` |
|         - | 5197 | `					/* Value exist */` |
|         3 | 5198 | `					break;` |
|         - | 5199 | `				}` |
|         4 | 5200 | `			}` |
|         9 | 5201 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5202 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5203 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5204 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5205 | `				return PH7_EXCEPTION;` |
|         - | 5206 | `			}` |
|         7 | 5207 | `			if( i >= (nArg - 1)){` |
|         - | 5208 | `				/* Perform the insertion */` |
|         5 | 5209 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5210 | `			}` |
|         3 | 5211 | `		}` |
|         - | 5212 | `		/* Point to the next entry */` |
|         7 | 5213 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5214 | `		n--;` |
|         1 | 5215 | `	}` |
|         - | 5216 | `	/* Return the freshly created array */` |
|         3 | 5217 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5218 | `	return PH7_OK;` |
|        15 | 5219 | `}` |
|         - | 5220 | `/*` |
|         - | 5221 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5222 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5223 | ` * Parameters` |
|         - | 5224 | ` *  $array1` |
|         - | 5225 | ` *    The array to compare from` |
|         - | 5226 | ` *  $array2` |
|         - | 5227 | ` *    An array to compare against` |
|         - | 5228 | ` *  $...` |
|         - | 5229 | ` *   More arrays to compare against` |
|         - | 5230 | ` * Return` |
|         - | 5231 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5232 | ` *  are not present in any of the other arrays.` |
|         - | 5233 | ` */` |
|        20 | 5234 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5235 | `{` |
|         - | 5236 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5237 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5238 | `	ph7_value *pArray;` |
|         - | 5239 | `	ph7_value *pVal;` |
|         - | 5240 | `	sxi32 rc;` |
|         - | 5241 | `	sxu32 n;` |
|         - | 5242 | `	int i;` |
|         - | 5243 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5244 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5245 | `	 * accompanying integration tests to pass. */` |
|        24 | 5246 | `	if( nArg < 1 ){` |
|       ! 0 | 5247 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5248 | `			"ArgumentCountError",` |
|         - | 5249 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5250 | `			nArg` |
|         - | 5251 | `			);` |
|         - | 5252 | `	}` |
|        24 | 5253 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5254 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5255 | `			"TypeError",` |
|         - | 5256 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5257 | `			ph7_type_name(apArg[0])` |
|         - | 5258 | `			);` |
|         - | 5259 | `	}` |
|        37 | 5260 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5261 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5262 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5263 | `				"TypeError",` |
|         - | 5264 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5265 | `				i + 1,` |
|         4 | 5266 | `				ph7_type_name(apArg[i])` |
|         - | 5267 | `				);` |
|         - | 5268 | `		}` |
|        10 | 5269 | `	}` |
|        15 | 5270 | `	if( nArg == 1 ){` |
|         - | 5271 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5272 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5273 | `		return PH7_OK;` |
|         - | 5274 | `	}` |
|         - | 5275 | `	/* Create a new array */` |
|        13 | 5276 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5277 | `	if( pArray == 0 ){` |
|       ! 0 | 5278 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5279 | `		return PH7_OK;` |
|         - | 5280 | `	}` |
|         - | 5281 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5282 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5283 | `	/* Perform the diff */` |
|        13 | 5284 | `	pEntry = pSrc->pFirst;` |
|        13 | 5285 | `	n = pSrc->nEntry;` |
|        13 | 5286 | `	pN1 = pN2 = 0;` |
|        34 | 5287 | `	for(;;){` |
|         - | 5288 | `		int keep;` |
|        41 | 5289 | `		if( n < 1 ){` |
|        13 | 5290 | `			break;` |
|         - | 5291 | `		}` |
|         - | 5292 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5293 | `		keep = 1;` |
|        47 | 5294 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5295 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5296 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5297 | `			/* Perform a key lookup first */` |
|        33 | 5298 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5299 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5300 | `			}else{` |
|        21 | 5301 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5302 | `			}` |
|        33 | 5303 | `			if( rc != SXRET_OK ){` |
|         - | 5304 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5305 | `				continue;` |
|         - | 5306 | `			}` |
|         - | 5307 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5308 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5309 | `			if( pVal ){` |
|         - | 5310 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5311 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5312 | `				if( pVal2 ){` |
|         - | 5313 | `					ph7_value sV1,sV2;` |
|         - | 5314 | `					sxi32 cmp;` |
|         - | 5315 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5316 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5317 | `					 * null element used to come back bool(false) in the` |
|         - | 5318 | `					 * caller's array). */` |
|        17 | 5319 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5320 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5321 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5322 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5323 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5324 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5325 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5326 | `					if( cmp == 0 ){` |
|         - | 5327 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5328 | `						keep = 0;` |
|        15 | 5329 | `						break;` |
|         - | 5330 | `					}` |
|         1 | 5331 | `				}` |
|         1 | 5332 | `			}` |
|         2 | 5333 | `		}` |
|        29 | 5334 | `		if( keep ){` |
|         - | 5335 | `			/* Perform the insertion */` |
|        15 | 5336 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5337 | `		}` |
|         - | 5338 | `		/* Point to the next entry */` |
|        29 | 5339 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5340 | `		n--;` |
|         1 | 5341 | `	}` |
|         - | 5342 | `	/* Return the freshly created array */` |
|        13 | 5343 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5344 | `	return PH7_OK;` |
|        14 | 5345 | `}` |
|         - | 5346 | `/*` |
|         - | 5347 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5348 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5349 | ` *  by a user supplied callback function.` |
|         - | 5350 | ` * Parameters` |
|         - | 5351 | ` *  $array1` |
|         - | 5352 | ` *    The array to compare from` |
|         - | 5353 | ` *  $array2` |
|         - | 5354 | ` *    An array to compare against` |
|         - | 5355 | ` *  $...` |
|         - | 5356 | ` *   More arrays to compare against.` |
|         - | 5357 | ` *  $key_compare_func` |
|         - | 5358 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5359 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5360 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5361 | ` * Return` |
|         - | 5362 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5363 | ` *  are not present in any of the other arrays.` |
|         - | 5364 | ` */` |
|        22 | 5365 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5366 | `{` |
|         - | 5367 | `	ph7_hashmap_node *pEntry;` |
|         - | 5368 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5369 | `	ph7_value *pCallback;` |
|         - | 5370 | `	ph7_value *pArray;` |
|         - | 5371 | `	sxi32 rc;` |
|         - | 5372 | `	sxu32 n;` |
|         - | 5373 | `	int i;` |
|         - | 5374 |  |
|         - | 5375 | `	/* Argument validation mimicking PHP errors. */` |
|        26 | 5376 | `	if( nArg < 2 ){` |
|       ! 0 | 5377 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5378 | `			"ArgumentCountError",` |
|         - | 5379 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       ! 0 | 5380 | `			nArg` |
|         - | 5381 | `			);` |
|         - | 5382 | `	}` |
|        26 | 5383 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5384 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5385 | `			"TypeError",` |
|         - | 5386 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5387 | `			ph7_type_name(apArg[0])` |
|         - | 5388 | `			);` |
|         - | 5389 | `	}` |
|         - | 5390 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5391 | `	 * expected to be a callback. */` |
|        38 | 5392 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5393 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5394 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5395 | `				"TypeError",` |
|         - | 5396 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5397 | `				i + 1,` |
|         2 | 5398 | `				ph7_type_name(apArg[i])` |
|         - | 5399 | `				);` |
|         - | 5400 | `		}` |
|         9 | 5401 | `	}` |
|         - | 5402 | `	/* Point to the callback value */` |
|        22 | 5403 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5404 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5405 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5406 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5407 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5408 | `		 * string given" which we also reproduce. */` |
|         9 | 5409 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5410 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5411 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5412 | `				"TypeError",` |
|         - | 5413 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5414 | `				nArg` |
|         - | 5415 | `				);` |
|         - | 5416 | `		}` |
|         6 | 5417 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5418 | `			/* neither array nor string */` |
|         8 | 5419 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5420 | `				"TypeError",` |
|         - | 5421 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5422 | `				nArg` |
|         - | 5423 | `				);` |
|         - | 5424 | `		}` |
|         - | 5425 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5426 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5427 | `			"TypeError",` |
|         - | 5428 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5429 | `			nArg,` |
|       ! 0 | 5430 | `			ph7_type_name(pCallback)` |
|         - | 5431 | `			);` |
|         - | 5432 | `	}` |
|        13 | 5433 | `	if( nArg == 2 ){` |
|         - | 5434 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5435 | `		 * input array. */` |
|         3 | 5436 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5437 | `		return PH7_OK;` |
|         - | 5438 | `	}` |
|         - | 5439 | `	/* Create a new array */` |
|        11 | 5440 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5441 | `	if( pArray == 0 ){` |
|       ! 0 | 5442 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5443 | `		return PH7_OK;` |
|         - | 5444 | `	}` |
|         - | 5445 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5446 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5447 | `	/* Perform the diff */` |
|        11 | 5448 | `	pEntry = pSrc->pFirst;` |
|        11 | 5449 | `	n = pSrc->nEntry;` |
|        21 | 5450 | `	for(;;){` |
|         - | 5451 | `		int keep;` |
|        27 | 5452 | `		if( n < 1 ){` |
|         9 | 5453 | `			break;` |
|         - | 5454 | `		}` |
|        19 | 5455 | `		keep = 1;` |
|        31 | 5456 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5457 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5458 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5459 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5460 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5461 | `			while( pIt ){` |
|         - | 5462 | `				/* build temporary key values for callback */` |
|         - | 5463 | `				ph7_value key1, key2, result;` |
|         - | 5464 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5465 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5466 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5467 | `				}else{` |
|         - | 5468 | `					SyString sStr;` |
|        33 | 5469 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5470 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5471 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5472 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5473 | `				}` |
|        33 | 5474 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5475 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5476 | `				}else{` |
|         - | 5477 | `					SyString sStr;` |
|        33 | 5478 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5479 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5480 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5481 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5482 | `				}` |
|        33 | 5483 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5484 | `				/* call user callback with (key1, key2) */` |
|         - | 5485 | `				{` |
|         - | 5486 | `					ph7_value *apK[2];` |
|        33 | 5487 | `					apK[0] = &key1;` |
|        33 | 5488 | `					apK[1] = &key2;` |
|        33 | 5489 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5490 | `				}` |
|        33 | 5491 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5492 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5493 | `					 * array_uintersect (which signal back from` |
|         - | 5494 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5495 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5496 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5497 | `					PH7_MemObjRelease(&result);` |
|         3 | 5498 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5499 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5500 | `					return PH7_EXCEPTION;` |
|         - | 5501 | `				}` |
|        31 | 5502 | `				if( rc == SXRET_OK ){` |
|        31 | 5503 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5504 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5505 | `					}` |
|        31 | 5506 | `					if( result.x.iVal == 0 ){` |
|         - | 5507 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5508 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5509 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5510 | `						if( pVal1 && pVal2 ){` |
|         - | 5511 | `							ph7_value sV1,sV2;` |
|         - | 5512 | `							sxi32 cmp;` |
|         - | 5513 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5514 | `							 * place and these are LIVE array elements. */` |
|        13 | 5515 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5516 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5517 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5518 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5519 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5520 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5521 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5522 | `							if( cmp == 0 ){` |
|         9 | 5523 | `								keep = 0;` |
|         9 | 5524 | `								PH7_MemObjRelease(&result);` |
|         - | 5525 | `								/* release keys too before breaking */` |
|         9 | 5526 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5527 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5528 | `								break;` |
|         - | 5529 | `							}` |
|         2 | 5530 | `						}` |
|         2 | 5531 | `					}` |
|        11 | 5532 | `				}` |
|        23 | 5533 | `				PH7_MemObjRelease(&result);` |
|        23 | 5534 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5535 | `				PH7_MemObjRelease(&key2);` |
|         - | 5536 | `				/* move to next node */` |
|        23 | 5537 | `				pIt = pIt->pPrev;` |
|        23 | 5538 | `				if( keep == 0 ) break;` |
|         1 | 5539 | `			}` |
|        21 | 5540 | `			if( keep == 0 ) break;` |
|         7 | 5541 | `		}` |
|        17 | 5542 | `		if( keep ){` |
|         - | 5543 | `			/* Perform the insertion */` |
|         9 | 5544 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5545 | `		}` |
|         - | 5546 | `		/* Point to the next entry */` |
|        17 | 5547 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5548 | `		n--;` |
|         1 | 5549 | `	}` |
|         - | 5550 | `	/* Return the freshly created array */` |
|         9 | 5551 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5552 | `	return PH7_OK;` |
|        15 | 5553 | `}` |
|         - | 5554 | `/*` |
|         - | 5555 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5556 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5557 | ` * Parameters` |
|         - | 5558 | ` *  $array1` |
|         - | 5559 | ` *    The array to compare from` |
|         - | 5560 | ` *  $array2` |
|         - | 5561 | ` *    An array to compare against` |
|         - | 5562 | ` *  $...` |
|         - | 5563 | ` *   More arrays to compare against` |
|         - | 5564 | ` * Return` |
|         - | 5565 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5566 | ` *  in any of the other arrays.` |
|         - | 5567 | ` * Note that NULL is returned on failure.` |
|         - | 5568 | ` */` |
|        12 | 5569 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5570 | `{` |
|         - | 5571 | `	ph7_hashmap_node *pEntry;` |
|         - | 5572 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5573 | `	ph7_value *pArray;` |
|         - | 5574 | `	sxi32 rc;` |
|         - | 5575 | `	sxu32 n;` |
|         - | 5576 | `	int i;` |
|         - | 5577 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5578 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5579 | `	 * helpers. */` |
|        15 | 5580 | `	if( nArg < 1 ){` |
|       ! 0 | 5581 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5582 | `			"ArgumentCountError",` |
|         - | 5583 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5584 | `			nArg` |
|         - | 5585 | `			);` |
|         - | 5586 | `	}` |
|        15 | 5587 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5588 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5589 | `			"TypeError",` |
|         - | 5590 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5591 | `			ph7_type_name(apArg[0])` |
|         - | 5592 | `			);` |
|         - | 5593 | `	}` |
|        20 | 5594 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5595 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5596 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5597 | `				"TypeError",` |
|         - | 5598 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5599 | `				i + 1,` |
|         2 | 5600 | `				ph7_type_name(apArg[i])` |
|         - | 5601 | `				);` |
|         - | 5602 | `		}` |
|         5 | 5603 | `	}` |
|         9 | 5604 | `	if( nArg == 1 ){` |
|         - | 5605 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5606 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5607 | `		return PH7_OK;` |
|         - | 5608 | `	}` |
|         - | 5609 | `	/* Create a new array */` |
|         7 | 5610 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5611 | `	if( pArray == 0 ){` |
|       ! 0 | 5612 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5613 | `		return PH7_OK;` |
|         - | 5614 | `	}` |
|         - | 5615 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5616 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5617 | `	/* Perfrom the diff */` |
|         7 | 5618 | `	pEntry = pSrc->pFirst;` |
|         7 | 5619 | `	n = pSrc->nEntry;` |
|        12 | 5620 | `	for(;;){` |
|        25 | 5621 | `		if( n < 1 ){` |
|         7 | 5622 | `			break;` |
|         - | 5623 | `		}` |
|        31 | 5624 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5625 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5626 | `				/* ignore */` |
|       ! 0 | 5627 | `				continue;` |
|         - | 5628 | `			}` |
|        23 | 5629 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5630 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5631 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5632 | `				/* Blob lookup */` |
|        17 | 5633 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5634 | `			}else{` |
|         - | 5635 | `				/* Int lookup */` |
|         7 | 5636 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5637 | `			}` |
|        23 | 5638 | `			if( rc == SXRET_OK ){` |
|         - | 5639 | `				/* Key exists,break immediately */` |
|        11 | 5640 | `				break;` |
|         - | 5641 | `			}` |
|         7 | 5642 | `		}` |
|        19 | 5643 | `		if( i >= nArg ){` |
|         - | 5644 | `			/* Perform the insertion */` |
|         9 | 5645 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5646 | `		}` |
|         - | 5647 | `		/* Point to the next entry */` |
|        19 | 5648 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5649 | `		n--;` |
|         1 | 5650 | `	}` |
|         - | 5651 | `	/* Return the freshly created array */` |
|         7 | 5652 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5653 | `	return PH7_OK;` |
|         9 | 5654 | `}` |
|         - | 5655 | `/*` |
|         - | 5656 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5657 | ` *  Computes the intersection of arrays.` |
|         - | 5658 | ` * Parameters` |
|         - | 5659 | ` *  $array1` |
|         - | 5660 | ` *    The array to compare from` |
|         - | 5661 | ` *  $array2` |
|         - | 5662 | ` *    An array to compare against` |
|         - | 5663 | ` *  $...` |
|         - | 5664 | ` *   More arrays to compare against` |
|         - | 5665 | ` * Return` |
|         - | 5666 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5667 | ` *  in all of the parameters.` |
|         - | 5668 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5669 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5670 | ` */` |
|        20 | 5671 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5672 | `{` |
|         - | 5673 | `	ph7_hashmap_node *pEntry;` |
|         - | 5674 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5675 | `	ph7_value *pArray;` |
|         - | 5676 | `	ph7_value *pVal;` |
|         - | 5677 | `	sxi32 rc;` |
|         - | 5678 | `	sxu32 n;` |
|         - | 5679 | `	int i;` |
|        23 | 5680 | `	if( nArg < 1 ){` |
|       ! 0 | 5681 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5682 | `			"ArgumentCountError",` |
|         - | 5683 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       ! 0 | 5684 | `			nArg` |
|         - | 5685 | `			);` |
|         - | 5686 | `	}` |
|        23 | 5687 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5688 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5689 | `			"TypeError",` |
|         - | 5690 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5691 | `			ph7_type_name(apArg[0])` |
|         - | 5692 | `			);` |
|         - | 5693 | `	}` |
|        36 | 5694 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5695 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5696 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5697 | `				"TypeError",` |
|         - | 5698 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5699 | `				i + 1,` |
|         2 | 5700 | `				ph7_type_name(apArg[i])` |
|         - | 5701 | `				);` |
|         - | 5702 | `		}` |
|         9 | 5703 | `	}` |
|        17 | 5704 | `	if( nArg == 1 ){` |
|         - | 5705 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5706 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5707 | `		return PH7_OK;` |
|         - | 5708 | `	}` |
|         - | 5709 | `	/* Create a new array */` |
|        15 | 5710 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5711 | `	if( pArray == 0 ){` |
|       ! 0 | 5712 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5713 | `		return PH7_OK;` |
|         - | 5714 | `	}` |
|         - | 5715 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5716 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5717 | `	/* Perform the intersection */` |
|        15 | 5718 | `	pEntry = pSrc->pFirst;` |
|        15 | 5719 | `	n = pSrc->nEntry;` |
|        31 | 5720 | `	for(;;){` |
|        63 | 5721 | `		if( n < 1 ){` |
|        15 | 5722 | `			break;` |
|         - | 5723 | `		}` |
|         - | 5724 | `		/* Extract the node value */` |
|        49 | 5725 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5726 | `		if( pVal ){` |
|        79 | 5727 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5728 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5729 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5730 | `				/* Perform the lookup */` |
|        55 | 5731 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5732 | `				if( rc != SXRET_OK ){` |
|         - | 5733 | `					/* Value does not exist */` |
|        25 | 5734 | `					break;` |
|         - | 5735 | `				}` |
|        16 | 5736 | `			}` |
|        49 | 5737 | `			if( i >= nArg ){` |
|         - | 5738 | `				/* Perform the insertion */` |
|        25 | 5739 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5740 | `			}` |
|        24 | 5741 | `		}` |
|         - | 5742 | `		/* Point to the next entry */` |
|        49 | 5743 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5744 | `		n--;` |
|         1 | 5745 | `	}` |
|         - | 5746 | `	/* Return the freshly created array */` |
|        15 | 5747 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5748 | `	return PH7_OK;` |
|        13 | 5749 | `}` |
|         - | 5750 | `/*` |
|         - | 5751 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5752 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5753 | ` * Parameters` |
|         - | 5754 | ` *  $array1` |
|         - | 5755 | ` *    The array to compare from` |
|         - | 5756 | ` *  $array2` |
|         - | 5757 | ` *    An array to compare against` |
|         - | 5758 | ` *  $...` |
|         - | 5759 | ` *   More arrays to compare against` |
|         - | 5760 | ` * Return` |
|         - | 5761 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5762 | ` *  in all the arguments, with matching keys.` |
|         - | 5763 | ` */` |
|        20 | 5764 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5765 | `{` |
|         - | 5766 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5767 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5768 | `	ph7_value *pArray;` |
|         - | 5769 | `	ph7_value *pVal;` |
|         - | 5770 | `	sxi32 rc;` |
|         - | 5771 | `	sxu32 n;` |
|         - | 5772 | `	int i;` |
|        23 | 5773 | `	if( nArg < 1 ){` |
|       ! 0 | 5774 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5775 | `			"ArgumentCountError",` |
|         - | 5776 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5777 | `			nArg` |
|         - | 5778 | `			);` |
|         - | 5779 | `	}` |
|        23 | 5780 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5781 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5782 | `			"TypeError",` |
|         - | 5783 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5784 | `			ph7_type_name(apArg[0])` |
|         - | 5785 | `			);` |
|         - | 5786 | `	}` |
|        36 | 5787 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5788 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5789 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5790 | `				"TypeError",` |
|         - | 5791 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5792 | `				i + 1,` |
|         2 | 5793 | `				ph7_type_name(apArg[i])` |
|         - | 5794 | `				);` |
|         - | 5795 | `		}` |
|         9 | 5796 | `	}` |
|        17 | 5797 | `	if( nArg == 1 ){` |
|         - | 5798 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5799 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5800 | `		return PH7_OK;` |
|         - | 5801 | `	}` |
|         - | 5802 | `	/* Create a new array */` |
|        15 | 5803 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5804 | `	if( pArray == 0 ){` |
|       ! 0 | 5805 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5806 | `		return PH7_OK;` |
|         - | 5807 | `	}` |
|         - | 5808 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5809 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5810 | `	/* Perform the intersection */` |
|        15 | 5811 | `	pEntry = pSrc->pFirst;` |
|        15 | 5812 | `	n = pSrc->nEntry;` |
|        15 | 5813 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5814 | `	for(;;){` |
|        47 | 5815 | `		if( n < 1 ){` |
|        15 | 5816 | `			break;` |
|         - | 5817 | `		}` |
|         - | 5818 | `		/* Extract the node value */` |
|        33 | 5819 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5820 | `		if( pVal ){` |
|        53 | 5821 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5822 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5823 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5824 | `				/* Perform a key lookup first */` |
|        37 | 5825 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5826 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5827 | `				}else{` |
|        23 | 5828 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5829 | `				}` |
|        37 | 5830 | `				if( rc != SXRET_OK ){` |
|         - | 5831 | `					/* No such key,break immediately */` |
|         7 | 5832 | `					break;` |
|         - | 5833 | `				}` |
|         - | 5834 | `				/* Perform the lookup */` |
|        31 | 5835 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5836 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5837 | `					/* Value does not exist */` |
|         6 | 5838 | `					break;` |
|         - | 5839 | `				}` |
|        11 | 5840 | `			}` |
|        33 | 5841 | `			if( i >= nArg ){` |
|         - | 5842 | `				/* Perform the insertion */` |
|        17 | 5843 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5844 | `			}` |
|        16 | 5845 | `		}` |
|         - | 5846 | `		/* Point to the next entry */` |
|        33 | 5847 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5848 | `		n--;` |
|         1 | 5849 | `	}` |
|         - | 5850 | `	/* Return the freshly created array */` |
|        15 | 5851 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5852 | `	return PH7_OK;` |
|        13 | 5853 | `}` |
|         - | 5854 | `/*` |
|         - | 5855 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5856 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5857 | ` * Parameters` |
|         - | 5858 | ` *  $array1` |
|         - | 5859 | ` *    The array to compare from` |
|         - | 5860 | ` *  $...` |
|         - | 5861 | ` *   More arrays to compare against` |
|         - | 5862 | ` * Return` |
|         - | 5863 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5864 | ` *  have keys that are present in all arguments.` |
|         - | 5865 | ` * Note that NULL is returned on failure.` |
|         - | 5866 | ` */` |
|        20 | 5867 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5868 | `{` |
|         - | 5869 | `	ph7_hashmap_node *pEntry;` |
|         - | 5870 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5871 | `	ph7_value *pArray;` |
|         - | 5872 | `	sxi32 rc;` |
|         - | 5873 | `	sxu32 n;` |
|         - | 5874 | `	int i;` |
|        23 | 5875 | `	if( nArg < 1 ){` |
|       ! 0 | 5876 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5877 | `			"ArgumentCountError",` |
|         - | 5878 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5879 | `			nArg` |
|         - | 5880 | `			);` |
|         - | 5881 | `	}` |
|        23 | 5882 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5883 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5884 | `			"TypeError",` |
|         - | 5885 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5886 | `			ph7_type_name(apArg[0])` |
|         - | 5887 | `			);` |
|         - | 5888 | `	}` |
|        36 | 5889 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5890 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5891 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5892 | `				"TypeError",` |
|         - | 5893 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5894 | `				i + 1,` |
|         2 | 5895 | `				ph7_type_name(apArg[i])` |
|         - | 5896 | `				);` |
|         - | 5897 | `		}` |
|         9 | 5898 | `	}` |
|        17 | 5899 | `	if( nArg == 1 ){` |
|         - | 5900 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5901 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5902 | `		return PH7_OK;` |
|         - | 5903 | `	}` |
|         - | 5904 | `	/* Create a new array */` |
|        15 | 5905 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5906 | `	if( pArray == 0 ){` |
|       ! 0 | 5907 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5908 | `		return PH7_OK;` |
|         - | 5909 | `	}` |
|         - | 5910 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5911 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5912 | `	/* Perform the intersection */` |
|        15 | 5913 | `	pEntry = pSrc->pFirst;` |
|        15 | 5914 | `	n = pSrc->nEntry;` |
|        24 | 5915 | `	for(;;){` |
|        49 | 5916 | `		if( n < 1 ){` |
|        15 | 5917 | `			break;` |
|         - | 5918 | `		}` |
|        57 | 5919 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5920 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5921 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5922 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5923 | `				/* Blob lookup */` |
|        27 | 5924 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5925 | `			}else{` |
|         - | 5926 | `				/* Int key */` |
|        13 | 5927 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5928 | `			}` |
|        39 | 5929 | `			if( rc != SXRET_OK ){` |
|         - | 5930 | `				/* Key does not exist, break immediately */` |
|        17 | 5931 | `				break;` |
|         - | 5932 | `			}` |
|        12 | 5933 | `		}` |
|        35 | 5934 | `		if( i >= nArg ){` |
|         - | 5935 | `			/* Perform the insertion */` |
|        19 | 5936 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5937 | `		}` |
|         - | 5938 | `		/* Point to the next entry */` |
|        35 | 5939 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5940 | `		n--;` |
|         1 | 5941 | `	}` |
|         - | 5942 | `	/* Return the freshly created array */` |
|        15 | 5943 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5944 | `	return PH7_OK;` |
|        13 | 5945 | `}` |
|         - | 5946 | `/*` |
|         - | 5947 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5948 | ` *  Computes the intersection of arrays.` |
|         - | 5949 | ` * Parameters` |
|         - | 5950 | ` *  $array1` |
|         - | 5951 | ` *    The array to compare from` |
|         - | 5952 | ` *  $array2` |
|         - | 5953 | ` *    An array to compare against` |
|         - | 5954 | ` *  $...` |
|         - | 5955 | ` *   More arrays to compare against` |
|         - | 5956 | ` * $callback` |
|         - | 5957 | ` *  The callback comparison function.` |
|         - | 5958 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5959 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5960 | ` *  than the second.` |
|         - | 5961 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5962 | ` * Return` |
|         - | 5963 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5964 | ` *  in all of the parameters. .` |
|         - | 5965 | ` * Note that NULL is returned on failure.` |
|         - | 5966 | ` */` |
|        24 | 5967 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5968 | `{` |
|         - | 5969 | `	ph7_hashmap_node *pEntry;` |
|         - | 5970 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5971 | `	ph7_value *pCallback;` |
|         - | 5972 | `	ph7_value *pArray;` |
|         - | 5973 | `	ph7_value *pVal;` |
|         - | 5974 | `	sxi32 rc;` |
|         - | 5975 | `	sxu32 n;` |
|         - | 5976 | `	int i;` |
|         - | 5977 |  |
|         - | 5978 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        29 | 5979 | `	if( nArg < 2 ){` |
|       ! 0 | 5980 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5981 | `			"ArgumentCountError",` |
|         - | 5982 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       ! 0 | 5983 | `			nArg` |
|         - | 5984 | `			);` |
|         - | 5985 | `	}` |
|        29 | 5986 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5987 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5988 | `			"TypeError",` |
|         - | 5989 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5990 | `			ph7_type_name(apArg[0])` |
|         - | 5991 | `			);` |
|         - | 5992 | `	}` |
|         - | 5993 |  |
|        27 | 5994 | `	if( nArg == 2 ){` |
|         - | 5995 | `		/* Only the original array and the callback were provided. */` |
|         - | 5996 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 5997 | `		 * validation ordering. */` |
|         3 | 5998 | `	} else {` |
|         - | 5999 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 6000 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 6001 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6002 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6003 | `					"TypeError",` |
|         - | 6004 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 6005 | `					i + 1,` |
|         2 | 6006 | `					ph7_type_name(apArg[i])` |
|         - | 6007 | `					);` |
|         - | 6008 | `			}` |
|        13 | 6009 | `		}` |
|         - | 6010 | `	}` |
|         - | 6011 |  |
|         - | 6012 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 6013 | `	pCallback = apArg[nArg - 1];` |
|         - | 6014 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 6015 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 6016 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 6017 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 6018 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 6019 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 6020 | `			 */` |
|         9 | 6021 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 6022 | `			if( pCb->nEntry != 2 ){` |
|         4 | 6023 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6024 | `					"TypeError",` |
|         - | 6025 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6026 | `					nArg` |
|         - | 6027 | `					);` |
|         - | 6028 | `			}` |
|         - | 6029 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6030 | `			{` |
|         6 | 6031 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6032 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6033 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6034 | `					int nMethodLen;` |
|         6 | 6035 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6036 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6037 | `					if( pClass ){` |
|         - | 6038 | `						/* Class exists but method is missing. */` |
|         4 | 6039 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6040 | `							"TypeError",` |
|         - | 6041 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6042 | `							nArg,` |
|         1 | 6043 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6044 | `							zMethod` |
|         - | 6045 | `							);` |
|         - | 6046 | `					}` |
|         - | 6047 | `					/* Class not found */` |
|         - | 6048 | `					{` |
|         - | 6049 | `						int nName;` |
|         3 | 6050 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6051 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6052 | `							"TypeError",` |
|         - | 6053 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6054 | `							nArg,` |
|         1 | 6055 | `							zName` |
|         - | 6056 | `							);` |
|         - | 6057 | `					}` |
|         - | 6058 | `				}` |
|         - | 6059 | `			}` |
|         - | 6060 | `			/* Fallback message */` |
|       ! 0 | 6061 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6062 | `				"TypeError",` |
|         - | 6063 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6064 | `				nArg` |
|         - | 6065 | `				);` |
|         - | 6066 | `		}` |
|         6 | 6067 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6068 | `			int len;` |
|         3 | 6069 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6070 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6071 | `				"TypeError",` |
|         - | 6072 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6073 | `				nArg,` |
|         1 | 6074 | `				zName` |
|         - | 6075 | `				);` |
|         - | 6076 | `		}` |
|         4 | 6077 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6078 | `			"TypeError",` |
|         - | 6079 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6080 | `			nArg` |
|         - | 6081 | `			);` |
|         - | 6082 | `	}` |
|         - | 6083 |  |
|        11 | 6084 | `	if( nArg == 2 ){` |
|         - | 6085 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6086 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6087 | `		return PH7_OK;` |
|         - | 6088 | `	}` |
|         - | 6089 |  |
|         - | 6090 | `	/* Create a new array */` |
|         7 | 6091 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6092 | `	if( pArray == 0 ){` |
|       ! 0 | 6093 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6094 | `		return PH7_OK;` |
|         - | 6095 | `	}` |
|         - | 6096 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6097 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6098 | `	/* Perform the intersection */` |
|         7 | 6099 | `	pEntry = pSrc->pFirst;` |
|         7 | 6100 | `	n = pSrc->nEntry;` |
|         7 | 6101 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6102 | `	for(;;){` |
|        19 | 6103 | `		if( n < 1 ){` |
|         5 | 6104 | `			break;` |
|         - | 6105 | `		}` |
|         - | 6106 | `		/* Extract the node value */` |
|        15 | 6107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6108 | `		if( pVal ){` |
|        23 | 6109 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6110 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6111 | `					/* ignore */` |
|       ! 0 | 6112 | `					continue;` |
|         - | 6113 | `				}` |
|         - | 6114 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6115 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6116 | `				/* Perform the lookup */` |
|        15 | 6117 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6118 | `				if( rc != SXRET_OK ){` |
|         - | 6119 | `					/* Value does not exist */` |
|         7 | 6120 | `					break;` |
|         - | 6121 | `				}` |
|         5 | 6122 | `			}` |
|        15 | 6123 | `			if( i >= (nArg-1) ){` |
|         - | 6124 | `				/* Perform the insertion */` |
|         9 | 6125 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6126 | `			}` |
|         7 | 6127 | `		}` |
|        15 | 6128 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6129 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6130 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6131 | `			return PH7_EXCEPTION;` |
|         - | 6132 | `		}` |
|         - | 6133 | `		/* Point to the next entry */` |
|        13 | 6134 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6135 | `		n--;` |
|         1 | 6136 | `	}` |
|         - | 6137 | `	/* Return the freshly created array */` |
|         5 | 6138 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6139 | `	return PH7_OK;` |
|        17 | 6140 | `}` |
|         - | 6141 | `/*` |
|         - | 6142 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6143 | ` *  Fill an array with values.` |
|         - | 6144 | ` * Parameters` |
|         - | 6145 | ` *  $start_index` |
|         - | 6146 | ` *    The first index of the returned array.` |
|         - | 6147 | ` *  $num` |
|         - | 6148 | ` *   Number of elements to insert.` |
|         - | 6149 | ` *  $value` |
|         - | 6150 | ` *    Value to use for filling.` |
|         - | 6151 | ` * Return` |
|         - | 6152 | ` *  The filled array or null on failure.` |
|         - | 6153 | ` */` |
|       240 | 6154 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6155 | `{` |
|         - | 6156 | `	ph7_value *pArray;` |
|         - | 6157 | `	int i,nEntry;` |
|         - | 6158 |  |
|         - | 6159 | `	/* PHP enforces argument count and type checks. */` |
|       244 | 6160 | `	if( nArg != 3 ){` |
|         - | 6161 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6162 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6163 | `			"ArgumentCountError",` |
|         - | 6164 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         1 | 6165 | `			nArg` |
|         - | 6166 | `			);` |
|         - | 6167 | `	}` |
|         - | 6168 |  |
|         - | 6169 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6170 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6171 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6172 | `	 * and NULLs are rejected outright. */` |
|       357 | 6173 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       361 | 6174 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       ! 0 | 6175 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6176 | `			"TypeError",` |
|         - | 6177 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       ! 0 | 6178 | `			ph7_type_name(apArg[0])` |
|         - | 6179 | `			);` |
|         - | 6180 | `	}` |
|       242 | 6181 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6182 | `		int len;` |
|         8 | 6183 | `		sxu8 bReal = FALSE;` |
|         8 | 6184 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6185 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6186 | `			/* Non‑numeric string is an error. */` |
|         3 | 6187 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6188 | `				"TypeError",` |
|         - | 6189 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6190 | `				);` |
|         - | 6191 | `		}` |
|         5 | 6192 | `		if( bReal ){` |
|         - | 6193 | `			/* float-string -> deprecation warning */` |
|         4 | 6194 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6195 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6196 | `				zStr` |
|         - | 6197 | `				);` |
|         1 | 6198 | `		}` |
|         2 | 6199 | `	}` |
|         - | 6200 |  |
|         - | 6201 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6202 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6203 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6204 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6205 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6206 | `			"TypeError",` |
|         - | 6207 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6208 | `			ph7_type_name(apArg[1])` |
|         - | 6209 | `			);` |
|         - | 6210 | `	}` |
|       239 | 6211 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6212 | `		int len;` |
|         3 | 6213 | `		sxu8 bReal = FALSE;` |
|         3 | 6214 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6215 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6216 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6217 | `				"TypeError",` |
|         - | 6218 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6219 | `				);` |
|         - | 6220 | `		}` |
|       ! 0 | 6221 | `	}` |
|         - | 6222 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6223 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6224 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6225 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6226 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6227 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6228 | `		if( d != (double)i64 ){` |
|         7 | 6229 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6230 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6231 | `				d` |
|         - | 6232 | `				);` |
|         2 | 6233 | `		}` |
|         2 | 6234 | `	}` |
|         - | 6235 |  |
|         - | 6236 | `	/* Total number of entries to insert */` |
|       236 | 6237 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6238 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6239 | `	if( nEntry < 0 ){` |
|         3 | 6240 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6241 | `			"ValueError",` |
|         - | 6242 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6243 | `			);` |
|         - | 6244 | `	}` |
|         - | 6245 |  |
|         - | 6246 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6247 | `	if( nEntry == 0 ){` |
|         7 | 6248 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6249 | `		return PH7_OK;` |
|         - | 6250 | `	}` |
|         - | 6251 |  |
|         - | 6252 | `	/* Create a new array */` |
|       227 | 6253 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6254 | `	if( pArray == 0 ){` |
|       ! 0 | 6255 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6256 | `	}` |
|         - | 6257 |  |
|         - | 6258 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6259 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6260 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6261 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6262 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6263 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6264 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6265 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6266 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6267 | `		}` |
|   1058803 | 6268 | `	}` |
|         - | 6269 | `	/* Return the filled array */` |
|       227 | 6270 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6271 | `	return PH7_OK;` |
|       124 | 6272 | `}` |
|         - | 6273 | `/*` |
|         - | 6274 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6275 | ` *  Fill an array with values, specifying keys.` |
|         - | 6276 | ` * Parameters` |
|         - | 6277 | ` *  $input` |
|         - | 6278 | ` *   Array of values that will be used as key.` |
|         - | 6279 | ` *  $value` |
|         - | 6280 | ` *    Value to use for filling.` |
|         - | 6281 | ` * Return` |
|         - | 6282 | ` *  The filled array.` |
|         - | 6283 | ` * Throws` |
|         - | 6284 | ` *  ValueError if $input is not an array.` |
|         - | 6285 | ` */` |
|        22 | 6286 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6287 | `{` |
|         - | 6288 | `	ph7_hashmap_node *pEntry;` |
|         - | 6289 | `	ph7_hashmap *pSrc;` |
|         - | 6290 | `	ph7_value *pArray;` |
|         - | 6291 | `	sxu32 n;` |
|         - | 6292 | `	/* PHP enforces exactly 2 arguments. */` |
|        25 | 6293 | `	if( nArg != 2 ){` |
|         4 | 6294 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6295 | `			"ArgumentCountError",` |
|         - | 6296 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         1 | 6297 | `			nArg` |
|         - | 6298 | `			);` |
|         - | 6299 | `	}` |
|         - | 6300 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6301 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6302 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6303 | `			"TypeError",` |
|         - | 6304 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6305 | `			ph7_type_name(apArg[0])` |
|         - | 6306 | `			);` |
|         - | 6307 | `	}` |
|         - | 6308 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6309 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6310 | `	/* Create a new array */` |
|        17 | 6311 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6312 | `	if( pArray == 0 ){` |
|       ! 0 | 6313 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6314 | `		return PH7_OK;` |
|         - | 6315 | `	}` |
|         - | 6316 | `	/* Perform the requested operation */` |
|        17 | 6317 | `	pEntry = pSrc->pFirst;` |
|        45 | 6318 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6319 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6320 | `		/* Point to the next entry */` |
|        29 | 6321 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6322 | `	}` |
|         - | 6323 | `	/* Return the filled array */` |
|        17 | 6324 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6325 | `	return PH7_OK;` |
|        14 | 6326 | `}` |
|         - | 6327 | `/*` |
|         - | 6328 | ` * array array_combine(array $keys,array $values)` |
|         - | 6329 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6330 | ` * Parameters` |
|         - | 6331 | ` *  $keys` |
|         - | 6332 | ` *    Array of keys to be used.` |
|         - | 6333 | ` * $values` |
|         - | 6334 | ` *   Array of values to be used.` |
|         - | 6335 | ` * Return` |
|         - | 6336 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6337 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6338 | ` *  not an array.` |
|         - | 6339 | ` */` |
|        16 | 6340 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6341 | `{` |
|         - | 6342 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6343 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6344 | `	ph7_value *pArray;` |
|         - | 6345 | `	sxu32 n;` |
|         - | 6346 | `	/* PHP enforces argument count and type checks. */` |
|        20 | 6347 | `	if( nArg != 2 ){` |
|         - | 6348 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       ! 0 | 6349 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6350 | `			"ArgumentCountError",` |
|         - | 6351 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       ! 0 | 6352 | `			nArg` |
|         - | 6353 | `			);` |
|         - | 6354 | `	}` |
|         - | 6355 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6356 | `	 * argument index in the error message. */` |
|        20 | 6357 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6358 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6359 | `			"TypeError",` |
|         - | 6360 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6361 | `			ph7_type_name(apArg[0])` |
|         - | 6362 | `			);` |
|         - | 6363 | `	}` |
|        17 | 6364 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6365 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6366 | `			"TypeError",` |
|         - | 6367 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6368 | `			ph7_type_name(apArg[1])` |
|         - | 6369 | `			);` |
|         - | 6370 | `	}` |
|         - | 6371 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6372 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6373 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6374 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6375 | `		/* Length mismatch -> ValueError */` |
|         3 | 6376 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6377 | `			"ValueError",` |
|         - | 6378 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6379 | `			);` |
|         - | 6380 | `	}` |
|         - | 6381 | `	/* Create a new array */` |
|        11 | 6382 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6383 | `	if( pArray == 0 ){` |
|       ! 0 | 6384 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6385 | `		return PH7_OK;` |
|         - | 6386 | `	}` |
|         - | 6387 | `	/* Perform the requested operation */` |
|        11 | 6388 | `	pKe = pKey->pFirst;` |
|        11 | 6389 | `	pVe = pValue->pFirst;` |
|        33 | 6390 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6391 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6392 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6393 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6394 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6395 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6396 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6397 | `		 * original array must not be mutated. */` |
|        23 | 6398 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6399 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6400 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6401 | `			if( pTmpKey ){` |
|         5 | 6402 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6403 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6404 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6405 | `				pKeyCopy = pTmpKey;` |
|         2 | 6406 | `			}` |
|         2 | 6407 | `		}` |
|        23 | 6408 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6409 | `		/* Point to the next entry */` |
|        23 | 6410 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6411 | `		pVe = pVe->pPrev;` |
|        12 | 6412 | `	}` |
|         - | 6413 | `	/* Return the filled array */` |
|        11 | 6414 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6415 | `	return PH7_OK;` |
|        12 | 6416 | `}` |
|         - | 6417 | `/*` |
|         - | 6418 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6419 | ` *  Return an array with elements in reverse order.` |
|         - | 6420 | ` * Parameters` |
|         - | 6421 | ` *  $array` |
|         - | 6422 | ` *   The input array.` |
|         - | 6423 | ` *  $preserve_keys (optional)` |
|         - | 6424 | ` *   If set to TRUE keys are preserved.` |
|         - | 6425 | ` * Return` |
|         - | 6426 | ` *  The reversed array.` |
|         - | 6427 | ` */` |
|        18 | 6428 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 6429 | `{` |
|         - | 6430 | `	ph7_hashmap_node *pEntry;` |
|         - | 6431 | `	ph7_hashmap *pSrc;` |
|         - | 6432 | `	ph7_value *pArray;` |
|         - | 6433 | `	int bPreserve;` |
|         - | 6434 | `	sxu32 n;` |
|        20 | 6435 | `	if( nArg < 1 ){` |
|       ! 0 | 6436 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6437 | `			"ArgumentCountError",` |
|         - | 6438 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       ! 0 | 6439 | `			nArg` |
|         - | 6440 | `			);` |
|         - | 6441 | `	}` |
|         - | 6442 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6443 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6444 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6445 | `			"TypeError",` |
|         - | 6446 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6447 | `			ph7_type_name(apArg[0])` |
|         - | 6448 | `			);` |
|         - | 6449 | `	}` |
|        17 | 6450 | `	bPreserve = FALSE;` |
|        17 | 6451 | `	if( nArg > 1 ){` |
|         7 | 6452 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6453 | `	}` |
|         - | 6454 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6455 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6456 | `	/* Create a new array */` |
|        17 | 6457 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6458 | `	if( pArray == 0 ){` |
|       ! 0 | 6459 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6460 | `		return PH7_OK;` |
|         - | 6461 | `	}` |
|         - | 6462 | `	/* Perform the requested operation */` |
|        17 | 6463 | `	pEntry = pSrc->pLast;` |
|        55 | 6464 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6465 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6466 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6467 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6468 | `		/* Point to the previous entry */` |
|        39 | 6469 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6470 | `	}` |
|        17 | 6471 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6472 | `	return PH7_OK;` |
|        11 | 6473 | `}` |
|         - | 6474 | `/*` |
|         - | 6475 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6476 | ` *  Removes duplicate values from an array.` |
|         - | 6477 | ` * Parameters` |
|         - | 6478 | ` *  $array` |
|         - | 6479 | ` *   The input array.` |
|         - | 6480 | ` *  $flags` |
|         - | 6481 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6482 | ` *   behavior using these values:` |
|         - | 6483 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6484 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6485 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6486 | ` * Return` |
|         - | 6487 | ` *  The filtered array.` |
|         - | 6488 | ` */` |
|        22 | 6489 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6490 | `{` |
|         - | 6491 | `	ph7_hashmap_node *pEntry;` |
|         - | 6492 | `	ph7_value *pNeedle;` |
|         - | 6493 | `	ph7_hashmap *pSrc;` |
|         - | 6494 | `	ph7_value *pArray;` |
|         - | 6495 | `	int bStrict;` |
|         - | 6496 | `	sxi32 rc;` |
|         - | 6497 | `	sxu32 n;` |
|        25 | 6498 | `	if( nArg < 1 ){` |
|         - | 6499 | `		/* Missing arguments, throw ArgumentCountError */` |
|       ! 0 | 6500 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6501 | `			"ArgumentCountError",` |
|         - | 6502 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6503 | `			);` |
|         - | 6504 | `	}` |
|        25 | 6505 | `	if( nArg > 2 ){` |
|         - | 6506 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6507 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6508 | `			"ArgumentCountError",` |
|         - | 6509 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6510 | `			nArg` |
|         - | 6511 | `			);` |
|         - | 6512 | `	}` |
|         - | 6513 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6514 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6515 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6516 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6517 | `			"TypeError",` |
|         - | 6518 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6519 | `			ph7_type_name(apArg[0])` |
|         - | 6520 | `			);` |
|         - | 6521 | `	}` |
|        19 | 6522 | `	bStrict = FALSE;` |
|         - | 6523 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6524 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6525 | `	/* Create a new array */` |
|        19 | 6526 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6527 | `	if( pArray == 0 ){` |
|       ! 0 | 6528 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6529 | `		return PH7_OK;` |
|         - | 6530 | `	}` |
|         - | 6531 | `	/* Perform the requested operation */` |
|        19 | 6532 | `	pEntry = pSrc->pFirst;` |
|        83 | 6533 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6534 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6535 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6536 | `		if( pNeedle ){` |
|        65 | 6537 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6538 | `		}` |
|        65 | 6539 | `		if( rc != SXRET_OK ){` |
|         - | 6540 | `			/* Perform the insertion */` |
|        37 | 6541 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6542 | `		}` |
|         - | 6543 | `		/* Point to the next entry */` |
|        65 | 6544 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6545 | `	}` |
|         - | 6546 | `	/* Return the freshly created array */` |
|        19 | 6547 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6548 | `	return PH7_OK;` |
|        14 | 6549 | `}` |
|         - | 6550 | `/*` |
|         - | 6551 | ` * array array_flip(array $input)` |
|         - | 6552 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6553 | ` * Parameter` |
|         - | 6554 | ` *  $input` |
|         - | 6555 | ` *   Input array.` |
|         - | 6556 | ` * Return` |
|         - | 6557 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6558 | ` */` |
|        30 | 6559 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6560 | `{` |
|         - | 6561 | `	ph7_hashmap_node *pEntry;` |
|         - | 6562 | `	ph7_hashmap *pSrc;` |
|         - | 6563 | `	ph7_value *pArray;` |
|         - | 6564 | `	ph7_value *pKey;` |
|         - | 6565 | `	ph7_value sVal;` |
|         - | 6566 | `	sxu32 n;` |
|         - | 6567 |  |
|         - | 6568 | `	/* PHP requires exactly one argument */` |
|        33 | 6569 | `	if( nArg != 1 ){` |
|         - | 6570 | `		/* Use ArgumentCountError like other array helpers */` |
|         4 | 6571 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6572 | `			"ArgumentCountError",` |
|         - | 6573 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         1 | 6574 | `			nArg` |
|         - | 6575 | `			);` |
|         - | 6576 | `	}` |
|         - | 6577 | `	/* Make sure we are dealing with a valid hashmap */` |
|        30 | 6578 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6579 | `		/* Type mismatch -> TypeError */` |
|         4 | 6580 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6581 | `			"TypeError",` |
|         - | 6582 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6583 | `			ph7_type_name(apArg[0])` |
|         - | 6584 | `			);` |
|         - | 6585 | `	}` |
|         - | 6586 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6587 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6588 | `	/* Create a new array */` |
|        27 | 6589 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6590 | `	if( pArray == 0 ){` |
|       ! 0 | 6591 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6592 | `		return PH7_OK;` |
|         - | 6593 | `	}` |
|         - | 6594 | `	/* Start processing */` |
|        27 | 6595 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6596 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6597 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6598 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6599 | `		if( pKey ){` |
|         - | 6600 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6601 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6602 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6603 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6604 | `					);` |
|     22236 | 6605 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6606 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6607 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6608 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6609 | `				}else{` |
|         - | 6610 | `					SyString sStr;` |
|      2227 | 6611 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6612 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6613 | `				}` |
|         - | 6614 | `				/* Perform the insertion */` |
|     22227 | 6615 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6616 | `				/* Safely release the value because each inserted entry` |
|         - | 6617 | `				 * has its own private copy of the value.` |
|         - | 6618 | `				 */` |
|     22227 | 6619 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6620 | `			}else{` |
|         - | 6621 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6622 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6623 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6624 | `					);` |
|         - | 6625 | `			}` |
|     11118 | 6626 | `		}` |
|         - | 6627 | `		/* Point to the next entry */` |
|     22237 | 6628 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6629 | `	}` |
|         - | 6630 | `	/* Return the freshly created array */` |
|        27 | 6631 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6632 | `	return PH7_OK;` |
|        18 | 6633 | `}` |
|         - | 6634 | `/*` |
|         - | 6635 | ` * number array_sum(array $array )` |
|         - | 6636 | ` *  Calculate the sum of values in an array.` |
|         - | 6637 | ` * Parameters` |
|         - | 6638 | ` *  $array: The input array.` |
|         - | 6639 | ` * Return` |
|         - | 6640 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6641 | ` */` |
|        24 | 6642 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6643 | `{` |
|         - | 6644 | `	ph7_hashmap_node *pEntry;` |
|         - | 6645 | `	ph7_value *pObj;` |
|        26 | 6646 | `	double dSum = 0;` |
|         - | 6647 | `	sxu32 n;` |
|        26 | 6648 | `	pEntry = pMap->pFirst;` |
|        92 | 6649 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        68 | 6650 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        68 | 6651 | `		if( pObj ){` |
|        68 | 6652 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        30 | 6653 | `				dSum += pObj->rVal;` |
|        54 | 6654 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6655 | `				dSum += (double)pObj->x.iVal;` |
|        30 | 6656 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        16 | 6657 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6658 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|         - | 6659 | `					 * resource cases below already did; only this one was silent) */` |
|         3 | 6660 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6661 | `						"Addition is not supported on type string");` |
|        14 | 6662 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6663 | `					double dv = 0;` |
|        13 | 6664 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6665 | `					dSum += dv;` |
|         8 | 6666 | `				}` |
|        12 | 6667 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6668 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6669 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6670 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6671 | `				/* php names the CLASS here, not the literal word "object" */` |
|       ! 0 | 6672 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       ! 0 | 6673 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6674 | `					"Addition is not supported on type %s",` |
|       ! 0 | 6675 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         3 | 6676 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6677 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6678 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6679 | `			}` |
|         - | 6680 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6681 | `		}` |
|         - | 6682 | `		/* Point to the next entry */` |
|        68 | 6683 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6684 | `	}` |
|         - | 6685 | `	/* Return sum */` |
|        26 | 6686 | `	ph7_result_double(pCtx,dSum);` |
|        26 | 6687 | `}` |
|       688 | 6688 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6689 | `{` |
|         - | 6690 | `	ph7_hashmap_node *pEntry;` |
|         - | 6691 | `	ph7_value *pObj;` |
|       690 | 6692 | `	sxi64 nSum = 0;` |
|         - | 6693 | `	sxu32 n;` |
|       690 | 6694 | `	pEntry = pMap->pFirst;` |
|      4702 | 6695 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4014 | 6696 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4014 | 6697 | `		if( pObj ){` |
|      4014 | 6698 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3994 | 6699 | `				nSum += pObj->x.iVal;` |
|      2018 | 6700 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        12 | 6701 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6702 | `					/* php warns and SKIPS a non-numeric string */` |
|         5 | 6703 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6704 | `						"Addition is not supported on type string");` |
|        10 | 6705 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         8 | 6706 | `					sxi64 nv = 0;` |
|         8 | 6707 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         8 | 6708 | `					nSum += nv;` |
|         5 | 6709 | `				}` |
|        17 | 6710 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 6711 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6712 | `					"array_sum(): Addition is not supported on type array");` |
|        10 | 6713 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6714 | `				/* php names the CLASS here, not the literal word "object" */` |
|         3 | 6715 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         5 | 6716 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6717 | `					"Addition is not supported on type %s",` |
|         2 | 6718 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         7 | 6719 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6720 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6721 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6722 | `			}` |
|         - | 6723 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      2006 | 6724 | `		}` |
|         - | 6725 | `		/* Point to the next entry */` |
|      4014 | 6726 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2008 | 6727 | `	}` |
|         - | 6728 | `	/* Return sum */` |
|       690 | 6729 | `	ph7_result_int64(pCtx,nSum);` |
|       690 | 6730 | `}` |
|         - | 6731 | `/* number array_sum(array $array )` |
|         - | 6732 | ` * (See block-coment above)` |
|         - | 6733 | ` */` |
|       724 | 6734 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6735 | `{` |
|         - | 6736 | `	ph7_hashmap_node *pEntry;` |
|         - | 6737 | `	ph7_hashmap *pMap;` |
|         - | 6738 | `	ph7_value *pObj;` |
|       728 | 6739 | `	int useDouble = 0;` |
|         - | 6740 | `	sxu32 n;` |
|         - | 6741 | `	/* PHP requires exactly one argument */` |
|       728 | 6742 | `	if( nArg != 1 ){` |
|         4 | 6743 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6744 | `			"ArgumentCountError",` |
|         - | 6745 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         1 | 6746 | `			nArg` |
|         - | 6747 | `			);` |
|         - | 6748 | `	}` |
|         - | 6749 | `	/* Make sure we are dealing with a valid hashmap */` |
|       725 | 6750 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6751 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6752 | `		char zBuf[64];` |
|         8 | 6753 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6754 | `			"TypeError",` |
|         - | 6755 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6756 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6757 | `			);` |
|         - | 6758 | `	}` |
|       720 | 6759 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       720 | 6760 | `	if( pMap->nEntry < 1 ){` |
|         - | 6761 | `		/* Nothing to compute,return 0 */` |
|         7 | 6762 | `		ph7_result_int(pCtx,0);` |
|         7 | 6763 | `		return PH7_OK;` |
|         - | 6764 | `	}` |
|         - | 6765 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6766 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6767 | `	 */` |
|       714 | 6768 | `	pEntry = pMap->pFirst;` |
|      4734 | 6769 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4046 | 6770 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4046 | 6771 | `		if( pObj ){` |
|      4046 | 6772 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        20 | 6773 | `				useDouble = 1;` |
|        20 | 6774 | `				break;` |
|         - | 6775 | `			}` |
|      4028 | 6776 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        18 | 6777 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        18 | 6778 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6779 | `				sxu32 i;` |
|        32 | 6780 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        22 | 6781 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6782 | `						useDouble = 1;` |
|         7 | 6783 | `						break;` |
|         - | 6784 | `					}` |
|         9 | 6785 | `				}` |
|        18 | 6786 | `				if( useDouble ){` |
|         7 | 6787 | `					break;` |
|         - | 6788 | `				}` |
|         5 | 6789 | `			}` |
|      2010 | 6790 | `		}` |
|      4022 | 6791 | `		pEntry = pEntry->pPrev;` |
|      2012 | 6792 | `	}` |
|       714 | 6793 | `	if( useDouble ){` |
|        26 | 6794 | `		DoubleSum(pCtx,pMap);` |
|        14 | 6795 | `	}else{` |
|       690 | 6796 | `		Int64Sum(pCtx,pMap);` |
|         - | 6797 | `	}` |
|       714 | 6798 | `	return PH7_OK;` |
|       366 | 6799 | `}` |
|         - | 6800 | `/*` |
|         - | 6801 | ` * number array_product(array $array )` |
|         - | 6802 | ` *  Calculate the product of values in an array.` |
|         - | 6803 | ` * Parameters` |
|         - | 6804 | ` *  $array: The input array.` |
|         - | 6805 | ` * Return` |
|         - | 6806 | ` *  Returns the product of values as an integer or float.` |
|         - | 6807 | ` */` |
|         2 | 6808 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6809 | `{` |
|         - | 6810 | `	ph7_hashmap_node *pEntry;` |
|         - | 6811 | `	ph7_value *pObj;` |
|         - | 6812 | `	double dProd;` |
|         - | 6813 | `	sxu32 n;` |
|         3 | 6814 | `	pEntry = pMap->pFirst;` |
|         3 | 6815 | `	dProd = 1;` |
|         7 | 6816 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6817 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6818 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6819 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6820 | `				dProd *= pObj->rVal;` |
|         4 | 6821 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6822 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6823 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6824 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6825 | `					double dv = 0;` |
|       ! 0 | 6826 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6827 | `					dProd *= dv;` |
|       ! 0 | 6828 | `				}` |
|       ! 0 | 6829 | `			}` |
|         2 | 6830 | `		}` |
|         - | 6831 | `		/* Point to the next entry */` |
|         5 | 6832 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6833 | `	}` |
|         - | 6834 | `	/* Return product */` |
|         3 | 6835 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6836 | `}` |
|         2 | 6837 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6838 | `{` |
|         - | 6839 | `	ph7_hashmap_node *pEntry;` |
|         - | 6840 | `	ph7_value *pObj;` |
|         - | 6841 | `	sxi64 nProd;` |
|         - | 6842 | `	sxu32 n;` |
|         3 | 6843 | `	pEntry = pMap->pFirst;` |
|         3 | 6844 | `	nProd = 1;` |
|         9 | 6845 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6846 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6847 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6848 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6849 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6850 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6851 | `				nProd *= pObj->x.iVal;` |
|         3 | 6852 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6853 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6854 | `					sxi64 nv = 0;` |
|       ! 0 | 6855 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6856 | `					nProd *= nv;` |
|       ! 0 | 6857 | `				}` |
|       ! 0 | 6858 | `			}` |
|         3 | 6859 | `		}` |
|         - | 6860 | `		/* Point to the next entry */` |
|         7 | 6861 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6862 | `	}` |
|         - | 6863 | `	/* Return product */` |
|         3 | 6864 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6865 | `}` |
|         - | 6866 | `/* number array_product(array $array )` |
|         - | 6867 | ` * (See block-block comment above)` |
|         - | 6868 | ` */` |
|        16 | 6869 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6870 | `{` |
|         - | 6871 | `	ph7_hashmap *pMap;` |
|         - | 6872 | `	ph7_value *pObj;` |
|        17 | 6873 | `	if( nArg < 1 ){` |
|         - | 6874 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6875 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6876 | `		return PH7_OK;` |
|         - | 6877 | `	}` |
|         - | 6878 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        17 | 6879 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6880 | `		char zBuf[64];` |
|        16 | 6881 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6882 | `			"TypeError",` |
|         - | 6883 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         5 | 6884 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6885 | `			);` |
|         - | 6886 | `	}` |
|         7 | 6887 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6888 | `	if( pMap->nEntry < 1 ){` |
|         - | 6889 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6890 | `		ph7_result_int(pCtx,1);` |
|         3 | 6891 | `		return PH7_OK;` |
|         - | 6892 | `	}` |
|         - | 6893 | `	/* If the first element is of type float,then perform floating` |
|         - | 6894 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6895 | `	 */` |
|         5 | 6896 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6897 | `	if( pObj == 0 ){` |
|       ! 0 | 6898 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6899 | `		return PH7_OK;` |
|         - | 6900 | `	}` |
|         5 | 6901 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6902 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6903 | `	}else{` |
|         3 | 6904 | `		Int64Prod(pCtx,pMap);` |
|         - | 6905 | `	}` |
|         5 | 6906 | `	return PH7_OK;` |
|         9 | 6907 | `}` |
|         - | 6908 | `/*` |
|         - | 6909 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6910 | ` *  Pick one or more random entries out of an array.` |
|         - | 6911 | ` * Parameters` |
|         - | 6912 | ` * $input` |
|         - | 6913 | ` *  The input array.` |
|         - | 6914 | ` * $num_req` |
|         - | 6915 | ` *  Specifies how many entries you want to pick.` |
|         - | 6916 | ` * Return` |
|         - | 6917 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6918 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6919 | ` *  NULL is returned on failure.` |
|         - | 6920 | ` */` |
|        36 | 6921 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6922 | `{` |
|         - | 6923 | `	ph7_hashmap_node *pNode;` |
|         - | 6924 | `	ph7_hashmap *pMap;` |
|        37 | 6925 | `	int nItem = 1;` |
|        37 | 6926 | `	if( nArg < 1 ){` |
|         - | 6927 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6928 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6929 | `		return PH7_OK;` |
|         - | 6930 | `	}` |
|         - | 6931 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        37 | 6932 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6933 | `		char zBuf[64];` |
|        10 | 6934 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6935 | `			"TypeError",` |
|         - | 6936 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6937 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6938 | `			);` |
|         - | 6939 | `	}` |
|         - | 6940 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6941 | `	 * check, matching its ZPP-before-body ordering. */` |
|        31 | 6942 | `	if( nArg > 1 ){` |
|        23 | 6943 | `		ph7_value *pNum = apArg[1];` |
|        22 | 6944 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        23 | 6945 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6946 | `			char zBuf[64];` |
|       ! 0 | 6947 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6948 | `				"TypeError",` |
|         - | 6949 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|       ! 0 | 6950 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6951 | `				);` |
|         - | 6952 | `		}` |
|        23 | 6953 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6954 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6955 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6956 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6957 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6958 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6959 | `			int len;` |
|         9 | 6960 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6961 | `			sxi64 iLong; double dReal;` |
|         9 | 6962 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6963 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6964 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6965 | `					"TypeError",` |
|         - | 6966 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6967 | `					);` |
|         - | 6968 | `			}` |
|         - | 6969 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6970 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6971 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6972 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6973 | `			}` |
|         3 | 6974 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6975 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6976 | `			nItem = (int)iLong;` |
|         2 | 6977 | `		}else{` |
|        15 | 6978 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6979 | `		}` |
|         8 | 6980 | `	}` |
|         - | 6981 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6982 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6983 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6984 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6985 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6986 | `			"ValueError",` |
|         - | 6987 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 6988 | `			);` |
|         - | 6989 | `	}` |
|         - | 6990 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 6991 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 6992 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6993 | `			"ValueError",` |
|         - | 6994 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 6995 | `			);` |
|         - | 6996 | `	}` |
|        13 | 6997 | `	if( nItem < 2 ){` |
|         - | 6998 | `		sxu32 nEntry;` |
|         - | 6999 | `		/* Select a random number */` |
|         9 | 7000 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 7001 | `		/* Extract the desired entry.` |
|         - | 7002 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 7003 | `		 */` |
|         9 | 7004 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         3 | 7005 | `			pNode = pMap->pLast;` |
|         3 | 7006 | `			nEntry = pMap->nEntry - nEntry;` |
|         3 | 7007 | `			if( nEntry > 1 ){` |
|       ! 0 | 7008 | `				for(;;){` |
|       ! 0 | 7009 | `					if( nEntry == 0 ){` |
|       ! 0 | 7010 | `						break;` |
|         - | 7011 | `					}` |
|         - | 7012 | `					/* Point to the previous entry */` |
|       ! 0 | 7013 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 7014 | `					nEntry--;` |
|       ! 0 | 7015 | `				}` |
|       ! 0 | 7016 | `			}` |
|         2 | 7017 | `		}else{` |
|         7 | 7018 | `			pNode = pMap->pFirst;` |
|         5 | 7019 | `			for(;;){` |
|        10 | 7020 | `				if( nEntry == 0 ){` |
|         7 | 7021 | `					break;` |
|         - | 7022 | `				}` |
|         - | 7023 | `				/* Point to the next entry */` |
|         4 | 7024 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         4 | 7025 | `				nEntry--;` |
|         1 | 7026 | `			}` |
|         - | 7027 | `		}` |
|         9 | 7028 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 7029 | `			/* Int key */` |
|         7 | 7030 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 7031 | `		}else{` |
|         - | 7032 | `			/* Blob key */` |
|         3 | 7033 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 7034 | `		}` |
|         5 | 7035 | `	}else{` |
|         - | 7036 | `		ph7_value sKey,*pArray;` |
|         - | 7037 | `		ph7_hashmap *pDest;` |
|         - | 7038 | `		/* Create a new array */` |
|         5 | 7039 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7040 | `		if( pArray == 0 ){` |
|       ! 0 | 7041 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7042 | `			return PH7_OK;` |
|         - | 7043 | `		}` |
|         - | 7044 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7045 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7046 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7047 | `		/* Copy the first n items */` |
|         5 | 7048 | `		pNode = pMap->pFirst;` |
|         5 | 7049 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7050 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7051 | `		}` |
|        15 | 7052 | `		while( nItem > 0){` |
|        11 | 7053 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7054 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7055 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7056 | `			/* Point to the next entry */` |
|        11 | 7057 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7058 | `			nItem--;` |
|         1 | 7059 | `		}` |
|         - | 7060 | `		/* Shuffle the array */` |
|         5 | 7061 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7062 | `		/* Rehash node */` |
|         5 | 7063 | `		HashmapSortRehash(pDest);` |
|         - | 7064 | `		/* Return the random array */` |
|         5 | 7065 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7066 | `	}` |
|        13 | 7067 | `	return PH7_OK;` |
|        19 | 7068 | `}` |
|         - | 7069 | `/*` |
|         - | 7070 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7071 | ` *  Split an array into chunks.` |
|         - | 7072 | ` * Parameters` |
|         - | 7073 | ` * $input` |
|         - | 7074 | ` *   The array to work on` |
|         - | 7075 | ` * $size` |
|         - | 7076 | ` *   The size of each chunk` |
|         - | 7077 | ` * $preserve_keys` |
|         - | 7078 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7079 | ` *   the chunk numerically.` |
|         - | 7080 | ` * Return` |
|         - | 7081 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7082 | ` *  zero, with each dimension containing size elements.` |
|         - | 7083 | ` */` |
|        36 | 7084 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7085 | `{` |
|         - | 7086 | `	ph7_value *pArray,*pChunk;` |
|         - | 7087 | `	ph7_hashmap_node *pEntry;` |
|         - | 7088 | `	ph7_hashmap *pMap;` |
|         - | 7089 | `	int bPreserve;` |
|         - | 7090 | `	sxu32 nChunk;` |
|         - | 7091 | `	sxu32 nSize;` |
|         - | 7092 | `	sxu32 n;` |
|         - | 7093 | `	/* Argument count and types follow PHP semantics. */` |
|        41 | 7094 | `	if( nArg < 2 ){` |
|         - | 7095 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       ! 0 | 7096 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7097 | `			"ArgumentCountError",` |
|         - | 7098 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7099 | `			nArg` |
|         - | 7100 | `			);` |
|         - | 7101 | `	}` |
|        41 | 7102 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7103 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7104 | `			"TypeError",` |
|         - | 7105 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7106 | `			ph7_type_name(apArg[0])` |
|         - | 7107 | `			);` |
|         - | 7108 | `	}` |
|         - | 7109 | `	/* Create a new array */` |
|        38 | 7110 | `	pArray = ph7_context_new_array(pCtx);` |
|        38 | 7111 | `	if( pArray == 0 ){` |
|       ! 0 | 7112 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7113 | `		return PH7_OK;` |
|         - | 7114 | `	}` |
|         - | 7115 | `	/* Point to the internal representation of the input hashmap */` |
|        38 | 7116 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7117 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7118 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        51 | 7119 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        72 | 7120 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        34 | 7121 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7122 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7123 | `			"TypeError",` |
|         - | 7124 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7125 | `			ph7_type_name(apArg[1])` |
|         - | 7126 | `			);` |
|         - | 7127 | `	}` |
|         - | 7128 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7129 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7130 | `	 * precision and PHP emits a deprecation warning. */` |
|        38 | 7131 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7132 | `		int len;` |
|         3 | 7133 | `		sxu8 bReal = FALSE;` |
|         3 | 7134 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7135 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7136 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7137 | `				"TypeError",` |
|         - | 7138 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7139 | `				);` |
|         - | 7140 | `		}` |
|       ! 0 | 7141 | `		if( bReal ){` |
|         - | 7142 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7143 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7144 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7145 | `				zStr` |
|         - | 7146 | `				);` |
|       ! 0 | 7147 | `		}` |
|       ! 0 | 7148 | `	}` |
|         - | 7149 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7150 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7151 | `	 * later via ph7_value_to_int. */` |
|        35 | 7152 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7153 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7154 | `		sxi64 i = (sxi64)d;` |
|         3 | 7155 | `		if( d != (double)i ){` |
|         4 | 7156 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7157 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7158 | `				d` |
|         - | 7159 | `				);` |
|         1 | 7160 | `		}` |
|         1 | 7161 | `	}` |
|         - | 7162 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7163 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7164 | `	{` |
|        35 | 7165 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        35 | 7166 | `		if( nSizeSigned < 1 ){` |
|         - | 7167 | `			/* size <= 0 -> ValueError */` |
|         6 | 7168 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7169 | `				"ValueError",` |
|         - | 7170 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7171 | `				);` |
|         - | 7172 | `		}` |
|        29 | 7173 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7174 | `	}` |
|        29 | 7175 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7176 | `		/* Return the whole array */` |
|         3 | 7177 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7178 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7179 | `		return PH7_OK;` |
|         - | 7180 | `	}` |
|        27 | 7181 | `	bPreserve = 0;` |
|        27 | 7182 | `	if( nArg > 2 ){` |
|         - | 7183 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7184 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7185 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7186 | `		 * normally, matching PHP behaviour. */` |
|        30 | 7187 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        31 | 7188 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7189 | `			ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 7190 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7191 | `				"TypeError",` |
|         - | 7192 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 7193 | `				ph7_type_name(apArg[2])` |
|         - | 7194 | `				);` |
|         - | 7195 | `		}` |
|        21 | 7196 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7197 | `	}` |
|         - | 7198 | `	/* Start processing */` |
|        27 | 7199 | `	pEntry = pMap->pFirst;` |
|        27 | 7200 | `	nChunk = 0;` |
|        27 | 7201 | `	pChunk = 0;` |
|        27 | 7202 | `	n = pMap->nEntry;` |
|        56 | 7203 | `	for( ;; ){` |
|       113 | 7204 | `		if( n < 1 ){` |
|         - | 7205 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7206 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7207 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7208 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7209 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7210 | `			 * exists. */` |
|        27 | 7211 | `			if( pChunk ){` |
|        27 | 7212 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7213 | `			}` |
|        27 | 7214 | `			break;` |
|         - | 7215 | `		}` |
|        87 | 7216 | `		if( nChunk < 1 ){` |
|        71 | 7217 | `			if( pChunk ){` |
|         - | 7218 | `				/* Put the first chunk */` |
|        45 | 7219 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7220 | `			}` |
|         - | 7221 | `			/* Create a new dimension */` |
|        71 | 7222 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7223 | `												   * will be automatically released as soon we return` |
|         - | 7224 | `												   * from this function */` |
|        71 | 7225 | `			if( pChunk == 0 ){` |
|       ! 0 | 7226 | `				break;` |
|         - | 7227 | `			}` |
|        71 | 7228 | `			nChunk = nSize;` |
|        35 | 7229 | `		}` |
|         - | 7230 | `		/* Insert the entry */` |
|        87 | 7231 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7232 | `		/* Point to the next entry */` |
|        87 | 7233 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7234 | `		nChunk--;` |
|        87 | 7235 | `		n--;` |
|         1 | 7236 | `	}` |
|         - | 7237 | `	/* Return the multidimensional array */` |
|        27 | 7238 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7239 | `	return PH7_OK;` |
|        23 | 7240 | `}` |
|         - | 7241 | `/*` |
|         - | 7242 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7243 | ` *  Pad array to the specified length with a value.` |
|         - | 7244 | ` * $input` |
|         - | 7245 | ` *   Initial array of values to pad.` |
|         - | 7246 | ` * $pad_size` |
|         - | 7247 | ` *   New size of the array.` |
|         - | 7248 | ` * $pad_value` |
|         - | 7249 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7250 | ` */` |
|         - | 7251 | `/*` |
|         - | 7252 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7253 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7254 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7255 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7256 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7257 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7258 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7259 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7260 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7261 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7262 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7263 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7264 | ` */` |
|        50 | 7265 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7266 | `	ph7_context *pCtx,` |
|         - | 7267 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7268 | `	int iArg,              /* 1-based argument position */` |
|         - | 7269 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7270 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7271 | `	)` |
|         1 | 7272 | `{` |
|        51 | 7273 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7274 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7275 | `			"ValueError",` |
|         - | 7276 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7277 | `			zFunc,iArg,zParam` |
|         - | 7278 | `			);` |
|         - | 7279 | `	}` |
|        37 | 7280 | `	return SXRET_OK;` |
|        26 | 7281 | `}` |
|        62 | 7282 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7283 | `{` |
|         - | 7284 | `	ph7_hashmap *pMap;` |
|         - | 7285 | `	ph7_value *pArray;` |
|         - | 7286 | `	sxi64 iLen,iAbs;` |
|         - | 7287 | `	int nEntry;` |
|         - | 7288 | `	sxi32 rc;` |
|        65 | 7289 | `	if( nArg != 3 ){` |
|         4 | 7290 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7291 | `			"ArgumentCountError",` |
|         - | 7292 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         1 | 7293 | `			nArg` |
|         - | 7294 | `			);` |
|         - | 7295 | `	}` |
|        62 | 7296 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7297 | `		char zBuf[64];` |
|        11 | 7298 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7299 | `			"TypeError",` |
|         - | 7300 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7301 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7302 | `			);` |
|         - | 7303 | `	}` |
|         - | 7304 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7305 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7306 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7307 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        54 | 7308 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        55 | 7309 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7310 | `		char zBuf[64];` |
|       ! 0 | 7311 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7312 | `			"TypeError",` |
|         - | 7313 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7314 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7315 | `			);` |
|         - | 7316 | `	}` |
|        55 | 7317 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7318 | `		int nStr;` |
|        11 | 7319 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7320 | `		sxi64 iLong; double dReal;` |
|        11 | 7321 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7322 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7323 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7324 | `				"TypeError",` |
|         - | 7325 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7326 | `				);` |
|         - | 7327 | `		}` |
|         7 | 7328 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7329 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7330 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7331 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7332 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7333 | `					"TypeError",` |
|         - | 7334 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7335 | `					);` |
|         - | 7336 | `			}` |
|         3 | 7337 | `			iLen = (sxi64)dReal;` |
|         3 | 7338 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7339 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7340 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7341 | `					zStr` |
|         - | 7342 | `					);` |
|       ! 0 | 7343 | `			}` |
|         2 | 7344 | `		}else{` |
|         5 | 7345 | `			iLen = iLong;` |
|         - | 7346 | `		}` |
|         4 | 7347 | `	}else{` |
|        45 | 7348 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7349 | `	}` |
|         - | 7350 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7351 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7352 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7353 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7354 | `	 * overflow). */` |
|        51 | 7355 | `	iAbs = iLen;` |
|        51 | 7356 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7357 | `		iAbs = -iAbs;` |
|         7 | 7358 | `	}` |
|        51 | 7359 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7360 | `	if( rc != SXRET_OK ){` |
|        15 | 7361 | `		return rc;` |
|         - | 7362 | `	}` |
|        37 | 7363 | `	nEntry = (int)iLen;` |
|         - | 7364 | `	/* Create a new array */` |
|        37 | 7365 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7366 | `	if( pArray == 0 ){` |
|       ! 0 | 7367 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7368 | `	}` |
|        37 | 7369 | `	if( nEntry < 0 ){` |
|        11 | 7370 | `		nEntry = -nEntry;` |
|        11 | 7371 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7372 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7373 | `			/* Insert given items first */` |
|        25 | 7374 | `			while( nEntry > 0 ){` |
|        19 | 7375 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7376 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7377 | `				}` |
|        19 | 7378 | `				nEntry--;` |
|         1 | 7379 | `			}` |
|         - | 7380 | `			/* Merge the two arrays */` |
|         7 | 7381 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7382 | `		}else{` |
|         5 | 7383 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7384 | `		}` |
|        32 | 7385 | `	}else if( nEntry > 0 ){` |
|        25 | 7386 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7387 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7388 | `			/* Merge the two arrays first */` |
|        19 | 7389 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7390 | `			/* Insert given items */` |
|       275 | 7391 | `			while( nEntry > 0 ){` |
|       257 | 7392 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7393 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7394 | `				}` |
|       257 | 7395 | `				nEntry--;` |
|         1 | 7396 | `			}` |
|        10 | 7397 | `		}else{` |
|         7 | 7398 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7399 | `		}` |
|        13 | 7400 | `	}else{` |
|         - | 7401 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7402 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7403 | `	}` |
|         - | 7404 | `	/* Return the new array */` |
|        37 | 7405 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7406 | `	return PH7_OK;` |
|        34 | 7407 | `}` |
|         - | 7408 | `/*` |
|         - | 7409 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7410 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7411 | ` * Parameters` |
|         - | 7412 | ` * $array` |
|         - | 7413 | ` *   The array in which elements are replaced.` |
|         - | 7414 | ` * $array1` |
|         - | 7415 | ` *   The array from which elements will be extracted.` |
|         - | 7416 | ` * ....` |
|         - | 7417 | ` *  More arrays from which elements will be extracted.` |
|         - | 7418 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7419 | ` * Return` |
|         - | 7420 | ` *  Returns an array.` |
|         - | 7421 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7422 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7423 | ` */` |
|        20 | 7424 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7425 | `{` |
|         - | 7426 | `	ph7_hashmap *pMap;` |
|         - | 7427 | `	ph7_value *pArray;` |
|         - | 7428 | `	int i;` |
|        23 | 7429 | `	if( nArg < 1 ){` |
|       ! 0 | 7430 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7431 | `			"ArgumentCountError",` |
|         - | 7432 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7433 | `			);` |
|         - | 7434 | `	}` |
|        23 | 7435 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7436 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7437 | `			"TypeError",` |
|         - | 7438 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7439 | `			ph7_type_name(apArg[0])` |
|         - | 7440 | `			);` |
|         - | 7441 | `	}` |
|         - | 7442 | `	/* Create a new array */` |
|        20 | 7443 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7444 | `	if( pArray == 0 ){` |
|       ! 0 | 7445 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7446 | `		return PH7_OK;` |
|         - | 7447 | `	}` |
|         - | 7448 | `	/* Overwrite from the first array */` |
|        20 | 7449 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7450 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7451 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7452 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7453 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7454 | `			/* Type mismatch -> TypeError */` |
|         4 | 7455 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7456 | `				"TypeError",` |
|         - | 7457 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7458 | `				i + 1,` |
|         2 | 7459 | `				ph7_type_name(apArg[i])` |
|         - | 7460 | `				);` |
|         - | 7461 | `		}` |
|         - | 7462 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7463 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7464 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7465 | `	}` |
|         - | 7466 | `	/* Return the new array */` |
|        17 | 7467 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7468 | `	return PH7_OK;` |
|        13 | 7469 | `}` |
|         - | 7470 | `/*` |
|         - | 7471 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7472 | ` *  Filters elements of an array using a callback function.` |
|         - | 7473 | ` * Parameters` |
|         - | 7474 | ` *  $input` |
|         - | 7475 | ` *    The array to iterate over` |
|         - | 7476 | ` * $callback` |
|         - | 7477 | ` *    The callback function to use` |
|         - | 7478 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7479 | ` *    will be removed.` |
|         - | 7480 | ` * Return` |
|         - | 7481 | ` *  The filtered array.` |
|         - | 7482 | ` */` |
|        30 | 7483 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7484 | `{` |
|         - | 7485 | `	ph7_hashmap_node *pEntry;` |
|         - | 7486 | `	ph7_hashmap *pMap;` |
|         - | 7487 | `	ph7_value *pArray;` |
|         - | 7488 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7489 | `	ph7_value *pValue;` |
|         - | 7490 | `	sxi32 rc;` |
|         - | 7491 | `	int keep;` |
|         - | 7492 | `	sxu32 n;` |
|        32 | 7493 | `	if( nArg < 1 ){` |
|         - | 7494 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7495 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7496 | `		return PH7_OK;` |
|         - | 7497 | `	}` |
|         - | 7498 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        32 | 7499 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7500 | `		char zBuf[64];` |
|        19 | 7501 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7502 | `			"TypeError",` |
|         - | 7503 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 7504 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7505 | `			);` |
|         - | 7506 | `	}` |
|         - | 7507 | `	/* Create a new array */` |
|        20 | 7508 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7509 | `	if( pArray == 0 ){` |
|       ! 0 | 7510 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7511 | `		return PH7_OK;` |
|         - | 7512 | `	}` |
|         - | 7513 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7514 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7515 | `	pEntry = pMap->pFirst;` |
|        20 | 7516 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7517 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7518 | `	/* Perform the requested operation */` |
|        78 | 7519 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7520 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7521 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7522 | `		if( pValue == 0 ){` |
|         - | 7523 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7524 | `			keep = FALSE;` |
|        64 | 7525 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7526 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7527 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7528 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7529 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7530 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7531 | `					int len;` |
|         3 | 7532 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7533 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7534 | `						"TypeError",` |
|         - | 7535 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7536 | `						zName` |
|         - | 7537 | `						);` |
|       ! 0 | 7538 | `				}else{` |
|       ! 0 | 7539 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7540 | `						"TypeError",` |
|         - | 7541 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7542 | `						ph7_type_name(apArg[1])` |
|         - | 7543 | `						);` |
|         - | 7544 | `				}` |
|         - | 7545 | `			}` |
|        33 | 7546 | `			keep = FALSE;` |
|        33 | 7547 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7548 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7549 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7550 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7551 | `				return PH7_EXCEPTION;` |
|         - | 7552 | `			}` |
|        31 | 7553 | `			if( rc == SXRET_OK ){` |
|         - | 7554 | `				/* Perform a boolean cast */` |
|        31 | 7555 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7556 | `			}` |
|        31 | 7557 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7558 | `		}else{` |
|         - | 7559 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7560 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7561 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7562 | `			 */` |
|        29 | 7563 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7564 | `		}` |
|        59 | 7565 | `		if( keep ){` |
|         - | 7566 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7567 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7568 | `		}` |
|         - | 7569 | `		/* Point to the next entry */` |
|        59 | 7570 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7571 | `	}` |
|        15 | 7572 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7573 | `	return PH7_OK;` |
|        17 | 7574 | `}` |
|         - | 7575 | `/*` |
|         - | 7576 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7577 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7578 | ` * Parameters` |
|         - | 7579 | ` *  $callback` |
|         - | 7580 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7581 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7582 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7583 | ` *   are zipped together.` |
|         - | 7584 | ` *  $array` |
|         - | 7585 | ` *   The first array to run through the callback function.` |
|         - | 7586 | ` *  $arrays` |
|         - | 7587 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7588 | ` * Return` |
|         - | 7589 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7590 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7591 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7592 | ` *  padding shorter arrays with NULL.` |
|         - | 7593 | ` */` |
|        58 | 7594 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7595 | `{` |
|         - | 7596 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7597 | `	ph7_hashmap_node *pEntry;` |
|         - | 7598 | `	ph7_hashmap *pMap;` |
|         - | 7599 | `	ph7_vm *pVm;` |
|         - | 7600 | `	int bNullCallback;` |
|         - | 7601 | `	sxi32 rc;` |
|         - | 7602 | `	int i;` |
|         - | 7603 | `	sxu32 n;` |
|        61 | 7604 | `	if( nArg < 2 ){` |
|       ! 0 | 7605 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7606 | `			"ArgumentCountError",` |
|         - | 7607 | `			"array_map() expects at least 2 arguments, %d given",` |
|       ! 0 | 7608 | `			nArg` |
|         - | 7609 | `			);` |
|         - | 7610 | `	}` |
|        61 | 7611 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        61 | 7612 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7613 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7614 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7615 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7616 | `				"TypeError",` |
|         - | 7617 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7618 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7619 | `				zFunc` |
|         - | 7620 | `				);` |
|         - | 7621 | `		}` |
|         3 | 7622 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7623 | `			"TypeError",` |
|         - | 7624 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7625 | `			"no array or string given"` |
|         - | 7626 | `			);` |
|         - | 7627 | `	}` |
|         - | 7628 | `	/* Every remaining argument must be an array */` |
|       121 | 7629 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        69 | 7630 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7631 | `			if( i == 1 ){` |
|         4 | 7632 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7633 | `					"TypeError",` |
|         - | 7634 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7635 | `					ph7_type_name(apArg[1])` |
|         - | 7636 | `					);` |
|         - | 7637 | `			}` |
|       ! 0 | 7638 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7639 | `				"TypeError",` |
|         - | 7640 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7641 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7642 | `				);` |
|         - | 7643 | `		}` |
|        34 | 7644 | `	}` |
|        54 | 7645 | `	pVm = pCtx->pVm;` |
|         - | 7646 | `	/* Create a new array */` |
|        54 | 7647 | `	pArray = ph7_context_new_array(pCtx);` |
|        54 | 7648 | `	if( pArray == 0 ){` |
|       ! 0 | 7649 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7650 | `		return PH7_OK;` |
|         - | 7651 | `	}` |
|        54 | 7652 | `	PH7_MemObjInit(pVm,&sResult);` |
|        54 | 7653 | `	PH7_MemObjInit(pVm,&sKey);` |
|        54 | 7654 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7655 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7656 | `	if( nArg == 2 ){` |
|         - | 7657 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        44 | 7658 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        44 | 7659 | `		pEntry = pMap->pFirst;` |
|       134 | 7660 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7661 | `			/* Extract the node value */` |
|        96 | 7662 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        96 | 7663 | `			if( pValue ){` |
|         - | 7664 | `				/* Extract the node key */` |
|        96 | 7665 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        96 | 7666 | `				if( bNullCallback ){` |
|         - | 7667 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7668 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7669 | `				}else{` |
|         - | 7670 | `					/* Invoke the supplied callback */` |
|        86 | 7671 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        86 | 7672 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7673 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7674 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7675 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7676 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7677 | `						return PH7_EXCEPTION;` |
|         - | 7678 | `					}` |
|         - | 7679 | `					/* Insert the callback return value */` |
|        82 | 7680 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7681 | `				}` |
|        92 | 7682 | `				PH7_MemObjRelease(&sKey);` |
|        92 | 7683 | `				PH7_MemObjRelease(&sResult);` |
|        45 | 7684 | `			}` |
|         - | 7685 | `			/* Point to the next entry */` |
|        92 | 7686 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        47 | 7687 | `		}` |
|        21 | 7688 | `	}else{` |
|         - | 7689 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7690 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7691 | `		int nArrays = nArg - 1;` |
|         - | 7692 | `		ph7_hashmap_node **apCur;` |
|         - | 7693 | `		ph7_value **apCallArg;` |
|         - | 7694 | `		ph7_value sNull;` |
|        11 | 7695 | `		sxu32 nMax = 0;` |
|        11 | 7696 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7697 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7698 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7699 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7700 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7701 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7702 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7703 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7704 | `			return PH7_OK;` |
|         - | 7705 | `		}` |
|        11 | 7706 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7707 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7708 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7709 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7710 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7711 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7712 | `				nMax = pMap->nEntry;` |
|         6 | 7713 | `			}` |
|        12 | 7714 | `		}` |
|        35 | 7715 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7716 | `			ph7_value *pZip = 0;` |
|        25 | 7717 | `			if( bNullCallback ){` |
|         - | 7718 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7719 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7720 | `			}` |
|        79 | 7721 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7722 | `				ph7_value *pv = &sNull;` |
|        55 | 7723 | `				if( apCur[i] ){` |
|        53 | 7724 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7725 | `					if( pNodeVal ){` |
|        53 | 7726 | `						pv = pNodeVal;` |
|        26 | 7727 | `					}` |
|        53 | 7728 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7729 | `				}` |
|        55 | 7730 | `				if( bNullCallback ){` |
|         9 | 7731 | `					if( pZip ){` |
|         9 | 7732 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7733 | `					}` |
|         5 | 7734 | `				}else{` |
|        47 | 7735 | `					apCallArg[i] = pv;` |
|         - | 7736 | `				}` |
|        28 | 7737 | `			}` |
|        25 | 7738 | `			if( bNullCallback ){` |
|         5 | 7739 | `				if( pZip ){` |
|         5 | 7740 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7741 | `				}` |
|         3 | 7742 | `			}else{` |
|        21 | 7743 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7744 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7745 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7746 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7747 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7748 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7749 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7750 | `					return PH7_EXCEPTION;` |
|         - | 7751 | `				}` |
|        21 | 7752 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7753 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7754 | `			}` |
|        13 | 7755 | `		}` |
|        11 | 7756 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7757 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7758 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7759 | `	}` |
|        50 | 7760 | `	PH7_MemObjRelease(&sKey);` |
|        50 | 7761 | `	PH7_MemObjRelease(&sResult);` |
|        50 | 7762 | `	ph7_result_value(pCtx,pArray);` |
|        50 | 7763 | `	return PH7_OK;` |
|        32 | 7764 | `}` |
|         - | 7765 | `/*` |
|         - | 7766 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7767 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7768 | ` * Parameters` |
|         - | 7769 | ` *  $array` |
|         - | 7770 | ` *   The input array.` |
|         - | 7771 | ` *  $callback` |
|         - | 7772 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7773 | ` *  $initial` |
|         - | 7774 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7775 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7776 | ` * Return` |
|         - | 7777 | ` *  Returns the resulting value.` |
|         - | 7778 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7779 | ` */` |
|        30 | 7780 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7781 | `{` |
|         - | 7782 | `	ph7_hashmap_node *pEntry;` |
|         - | 7783 | `	ph7_hashmap *pMap;` |
|         - | 7784 | `	ph7_value *pValue;` |
|         - | 7785 | `	ph7_value sResult;` |
|         - | 7786 | `	sxi32 rc;` |
|         - | 7787 | `	sxu32 n;` |
|        35 | 7788 | `	if( nArg < 2 ){` |
|       ! 0 | 7789 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7790 | `			"ArgumentCountError",` |
|         - | 7791 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       ! 0 | 7792 | `			nArg` |
|         - | 7793 | `			);` |
|         - | 7794 | `	}` |
|        35 | 7795 | `	if( nArg > 3 ){` |
|         4 | 7796 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7797 | `			"ArgumentCountError",` |
|         - | 7798 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7799 | `			nArg` |
|         - | 7800 | `			);` |
|         - | 7801 | `	}` |
|        33 | 7802 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7803 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7804 | `			"TypeError",` |
|         - | 7805 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7806 | `			ph7_type_name(apArg[0])` |
|         - | 7807 | `			);` |
|         - | 7808 | `	}` |
|        31 | 7809 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7810 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7811 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7812 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7813 | `				"TypeError",` |
|         - | 7814 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7815 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7816 | `				zFunc` |
|         - | 7817 | `				);` |
|         - | 7818 | `		}` |
|         9 | 7819 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7820 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7821 | `				"TypeError",` |
|         - | 7822 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7823 | `				"array callback must have exactly two members"` |
|         - | 7824 | `				);` |
|         - | 7825 | `		}` |
|         6 | 7826 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7827 | `			"TypeError",` |
|         - | 7828 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7829 | `			"no array or string given"` |
|         - | 7830 | `			);` |
|         - | 7831 | `	}` |
|         - | 7832 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7833 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7834 | `	/* Assume a NULL initial value */` |
|        19 | 7835 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7836 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7837 | `	if( nArg > 2 ){` |
|         - | 7838 | `		/* Set the initial value */` |
|        13 | 7839 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7840 | `	}` |
|         - | 7841 | `	/* Perform the requested operation */` |
|        19 | 7842 | `	pEntry = pMap->pFirst;` |
|        55 | 7843 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7844 | `		/* Extract the node value */` |
|        39 | 7845 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7846 | `		/* Invoke the supplied callback */` |
|        39 | 7847 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7848 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7849 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7850 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7851 | `			return PH7_EXCEPTION;` |
|         - | 7852 | `		}` |
|         - | 7853 | `		/* Point to the next entry */` |
|        37 | 7854 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7855 | `	}` |
|        17 | 7856 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7857 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7858 | `	return PH7_OK;` |
|        20 | 7859 | `}` |
|         - | 7860 | `/*` |
|         - | 7861 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7862 | ` *  Apply a user function to every member of an array.` |
|         - | 7863 | ` * Parameters` |
|         - | 7864 | ` *  $array` |
|         - | 7865 | ` *   The input array.` |
|         - | 7866 | ` *  $funcname` |
|         - | 7867 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7868 | ` *   the first, and the key/index second.` |
|         - | 7869 | ` * Note:` |
|         - | 7870 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7871 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7872 | ` *  be made in the original array itself.` |
|         - | 7873 | ` *  $userdata` |
|         - | 7874 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7875 | ` *   to the callback funcname.` |
|         - | 7876 | ` * Return` |
|         - | 7877 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7878 | ` */` |
|        34 | 7879 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7880 | `{` |
|         - | 7881 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7882 | `	ph7_hashmap_node *pEntry;` |
|         - | 7883 | `	ph7_hashmap *pMap;` |
|         - | 7884 | `	sxu32 n;` |
|        39 | 7885 | `	if( nArg < 2 ){` |
|       ! 0 | 7886 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7887 | `			"ArgumentCountError",` |
|         - | 7888 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7889 | `			nArg` |
|         - | 7890 | `			);` |
|         - | 7891 | `	}` |
|        39 | 7892 | `	if( nArg > 3 ){` |
|         4 | 7893 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7894 | `			"ArgumentCountError",` |
|         - | 7895 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7896 | `			nArg` |
|         - | 7897 | `			);` |
|         - | 7898 | `	}` |
|        37 | 7899 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7900 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7901 | `			"TypeError",` |
|         - | 7902 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7903 | `			ph7_type_name(apArg[0])` |
|         - | 7904 | `			);` |
|         - | 7905 | `	}` |
|        35 | 7906 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7907 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7908 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7909 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7910 | `				"TypeError",` |
|         - | 7911 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7912 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7913 | `				zFunc` |
|         - | 7914 | `				);` |
|         - | 7915 | `		}` |
|        12 | 7916 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7917 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7918 | `				"TypeError",` |
|         - | 7919 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7920 | `				"array callback must have exactly two members"` |
|         - | 7921 | `				);` |
|         - | 7922 | `		}` |
|         6 | 7923 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7924 | `			"TypeError",` |
|         - | 7925 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7926 | `			"no array or string given"` |
|         - | 7927 | `			);` |
|         - | 7928 | `	}` |
|        21 | 7929 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7930 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7931 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7932 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7933 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7934 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7935 | `	/* Perform the desired operation */` |
|        21 | 7936 | `	pEntry = pMap->pFirst;` |
|        61 | 7937 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7938 | `		/* Extract the node value */` |
|        43 | 7939 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7940 | `		if( pValue ){` |
|         - | 7941 | `			sxi32 rcW;` |
|         - | 7942 | `			/* Extract the entry key */` |
|        43 | 7943 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7944 | `			/* Invoke the supplied callback */` |
|        43 | 7945 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7946 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7947 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7948 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7949 | `				return PH7_EXCEPTION;` |
|         - | 7950 | `			}` |
|        20 | 7951 | `		}` |
|         - | 7952 | `		/* Point to the next entry */` |
|        41 | 7953 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7954 | `	}` |
|         - | 7955 | `	/* All done, return TRUE */` |
|        19 | 7956 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7957 | `	return PH7_OK;` |
|        22 | 7958 | `}` |
|         - | 7959 | `/*` |
|         - | 7960 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7961 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7962 | ` */` |
|        22 | 7963 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7964 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7965 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7966 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7967 | `	int iNest             /* Nesting level */` |
|         - | 7968 | `	)` |
|         1 | 7969 | `{` |
|         - | 7970 | `	ph7_hashmap_node *pEntry;` |
|         - | 7971 | `	ph7_value *pValue,sKey;` |
|         - | 7972 | `	sxi32 rc;` |
|         - | 7973 | `	sxu32 n;` |
|         - | 7974 | `	/* Iterate through hashmap entries */` |
|        23 | 7975 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7976 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7977 | `	pEntry = pMap->pFirst;` |
|        59 | 7978 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7979 | `		/* Extract the node value */` |
|        37 | 7980 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7981 | `		if( pValue ){` |
|        37 | 7982 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7983 | `				if( iNest < 32 ){` |
|         - | 7984 | `					/* Recurse */` |
|        11 | 7985 | `					iNest++;` |
|        11 | 7986 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 7987 | `					iNest--;` |
|        11 | 7988 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7989 | `						return PH7_EXCEPTION;` |
|         - | 7990 | `					}` |
|         5 | 7991 | `				}` |
|         6 | 7992 | `			}else{` |
|         - | 7993 | `				/* Extract the node key */` |
|        27 | 7994 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7995 | `				/* Invoke the supplied callback */` |
|        27 | 7996 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 7997 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 7998 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 7999 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8000 | `					return PH7_EXCEPTION;` |
|         - | 8001 | `				}` |
|         - | 8002 | `			}` |
|        18 | 8003 | `		}` |
|         - | 8004 | `		/* Point to the next entry */` |
|        37 | 8005 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 8006 | `	}` |
|        23 | 8007 | `	return PH7_OK;` |
|        12 | 8008 | `}` |
|         - | 8009 | `/*` |
|         - | 8010 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8011 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 8012 | ` * Parameters` |
|         - | 8013 | ` *  $array` |
|         - | 8014 | ` *   The input array.` |
|         - | 8015 | ` *  $funcname` |
|         - | 8016 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8017 | ` *   the first, and the key/index second.` |
|         - | 8018 | ` * Note:` |
|         - | 8019 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8020 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8021 | ` *  be made in the original array itself.` |
|         - | 8022 | ` *  $userdata` |
|         - | 8023 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8024 | ` *   to the callback funcname.` |
|         - | 8025 | ` * Return` |
|         - | 8026 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8027 | ` */` |
|        26 | 8028 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8029 | `{` |
|         - | 8030 | `	ph7_hashmap *pMap;` |
|        31 | 8031 | `	if( nArg < 2 ){` |
|       ! 0 | 8032 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8033 | `			"ArgumentCountError",` |
|         - | 8034 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       ! 0 | 8035 | `			nArg` |
|         - | 8036 | `			);` |
|         - | 8037 | `	}` |
|        31 | 8038 | `	if( nArg > 3 ){` |
|         4 | 8039 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8040 | `			"ArgumentCountError",` |
|         - | 8041 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8042 | `			nArg` |
|         - | 8043 | `			);` |
|         - | 8044 | `	}` |
|        29 | 8045 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8046 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8047 | `			"TypeError",` |
|         - | 8048 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8049 | `			ph7_type_name(apArg[0])` |
|         - | 8050 | `			);` |
|         - | 8051 | `	}` |
|        27 | 8052 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8053 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8054 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8055 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8056 | `				"TypeError",` |
|         - | 8057 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8058 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8059 | `				zFunc` |
|         - | 8060 | `				);` |
|         - | 8061 | `		}` |
|        12 | 8062 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8063 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8064 | `				"TypeError",` |
|         - | 8065 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8066 | `				"array callback must have exactly two members"` |
|         - | 8067 | `				);` |
|         - | 8068 | `		}` |
|         6 | 8069 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8070 | `			"TypeError",` |
|         - | 8071 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8072 | `			"no array or string given"` |
|         - | 8073 | `			);` |
|         - | 8074 | `	}` |
|         - | 8075 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8076 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8077 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8078 | `	/* Perform the desired operation */` |
|        13 | 8079 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8080 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8081 | `		return PH7_EXCEPTION;` |
|         - | 8082 | `	}` |
|         - | 8083 | `	/* All done, return TRUE */` |
|        13 | 8084 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8085 | `	return PH7_OK;` |
|        18 | 8086 | `}` |
|         - | 8087 | `/*` |
|         - | 8088 | ` * bool array_is_list(array $array)` |
|         - | 8089 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8090 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8091 | ` * Return` |
|         - | 8092 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8093 | ` */` |
|         - | 8094 | `/*` |
|         - | 8095 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8096 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8097 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8098 | ` */` |
|       246 | 8099 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8100 | `{` |
|       247 | 8101 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       247 | 8102 | `	sxi64 iExpect = 0;` |
|         - | 8103 | `	sxu32 n;` |
|       555 | 8104 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       409 | 8105 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8106 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       101 | 8107 | `			return 0;` |
|         - | 8108 | `		}` |
|       309 | 8109 | `		++iExpect;` |
|       309 | 8110 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       155 | 8111 | `	}` |
|       147 | 8112 | `	return 1;` |
|       124 | 8113 | `}` |
|        12 | 8114 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8115 | `{` |
|        13 | 8116 | `	if( nArg < 1 ){` |
|       ! 0 | 8117 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8118 | `			"ArgumentCountError",` |
|         - | 8119 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8120 | `			);` |
|         - | 8121 | `	}` |
|        13 | 8122 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8123 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8124 | `			"TypeError",` |
|         - | 8125 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8126 | `			ph7_type_name(apArg[0])` |
|         - | 8127 | `			);` |
|         - | 8128 | `	}` |
|        13 | 8129 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8130 | `	return PH7_OK;` |
|         7 | 8131 | `}` |
|         - | 8132 | `/*` |
|         - | 8133 | ` * mixed array_first(array $array)` |
|         - | 8134 | ` * mixed array_last(array $array)` |
|         - | 8135 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8136 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8137 | ` *  untouched (unlike reset()/end()).` |
|         - | 8138 | ` */` |
|        18 | 8139 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8140 | `{` |
|         - | 8141 | `	ph7_hashmap *pMap;` |
|         - | 8142 | `	ph7_hashmap_node *pNode;` |
|         - | 8143 | `	ph7_value *pVal;` |
|        19 | 8144 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        19 | 8145 | `	if( nArg < 1 ){` |
|       ! 0 | 8146 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8147 | `			"ArgumentCountError",` |
|         - | 8148 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8149 | `			zName` |
|         - | 8150 | `			);` |
|         - | 8151 | `	}` |
|        19 | 8152 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8153 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8154 | `			"TypeError",` |
|         - | 8155 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8156 | `			zName,` |
|         1 | 8157 | `			ph7_type_name(apArg[0])` |
|         - | 8158 | `			);` |
|         - | 8159 | `	}` |
|        17 | 8160 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8161 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8162 | `	if( pNode == 0 ){` |
|         - | 8163 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8164 | `		ph7_result_null(pCtx);` |
|         5 | 8165 | `		return PH7_OK;` |
|         - | 8166 | `	}` |
|        13 | 8167 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8168 | `	if( pVal ){` |
|        13 | 8169 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8170 | `	}else{` |
|       ! 0 | 8171 | `		ph7_result_null(pCtx);` |
|         - | 8172 | `	}` |
|        13 | 8173 | `	return PH7_OK;` |
|        10 | 8174 | `}` |
|         8 | 8175 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8176 | `{` |
|         9 | 8177 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8178 | `}` |
|        10 | 8179 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8180 | `{` |
|        11 | 8181 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8182 | `}` |
|         - | 8183 | `/*` |
|         - | 8184 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8185 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8186 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8187 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8188 | ` *  untouched.` |
|         - | 8189 | ` */` |
|        22 | 8190 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8191 | `{` |
|         - | 8192 | `	ph7_hashmap *pMap;` |
|         - | 8193 | `	ph7_hashmap_node *pNode;` |
|        23 | 8194 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        23 | 8195 | `	if( nArg < 1 ){` |
|       ! 0 | 8196 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8197 | `			"ArgumentCountError",` |
|         - | 8198 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8199 | `			zName` |
|         - | 8200 | `			);` |
|         - | 8201 | `	}` |
|        23 | 8202 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8203 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8204 | `			"TypeError",` |
|         - | 8205 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8206 | `			zName,` |
|         1 | 8207 | `			ph7_type_name(apArg[0])` |
|         - | 8208 | `			);` |
|         - | 8209 | `	}` |
|        21 | 8210 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8211 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8212 | `	if( pNode == 0 ){` |
|         - | 8213 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8214 | `		ph7_result_null(pCtx);` |
|         5 | 8215 | `		return PH7_OK;` |
|         - | 8216 | `	}` |
|        17 | 8217 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8218 | `	return PH7_OK;` |
|        12 | 8219 | `}` |
|        10 | 8220 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8221 | `{` |
|        11 | 8222 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8223 | `}` |
|        12 | 8224 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8225 | `{` |
|        13 | 8226 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8227 | `}` |
|         - | 8228 | `/*` |
|         - | 8229 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8230 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8231 | ` * array_column() for both the column value and the index key.` |
|         - | 8232 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8233 | ` * container or the key is absent.` |
|         - | 8234 | ` */` |
|        32 | 8235 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8236 | `{` |
|        33 | 8237 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8238 | `		ph7_hashmap_node *pNode;` |
|        25 | 8239 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8240 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8241 | `		}` |
|        11 | 8242 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8243 | `		ph7_value sName;` |
|         - | 8244 | `		const char *zName;` |
|         - | 8245 | `		ph7_value *pAttr;` |
|         - | 8246 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8247 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8248 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8249 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8250 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8251 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8252 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8253 | `		return pAttr;` |
|         - | 8254 | `	}` |
|         5 | 8255 | `	return 0;` |
|        17 | 8256 | `}` |
|         - | 8257 | `/*` |
|         - | 8258 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8259 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8260 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8261 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8262 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8263 | ` *  Each row may be an array or an object.` |
|         - | 8264 | ` */` |
|        12 | 8265 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8266 | `{` |
|         - | 8267 | `	ph7_hashmap_node *pNode;` |
|         - | 8268 | `	ph7_hashmap *pMap;` |
|         - | 8269 | `	ph7_value *pArray;` |
|         - | 8270 | `	ph7_value *pRow;` |
|         - | 8271 | `	ph7_value *pCol;` |
|         - | 8272 | `	ph7_value *pIdx;` |
|         - | 8273 | `	int bWantCol;` |
|         - | 8274 | `	int bWantIdx;` |
|         - | 8275 | `	sxu32 n;` |
|        13 | 8276 | `	if( nArg < 2 ){` |
|       ! 0 | 8277 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8278 | `			"ArgumentCountError",` |
|         - | 8279 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8280 | `			nArg` |
|         - | 8281 | `			);` |
|         - | 8282 | `	}` |
|        13 | 8283 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8284 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8285 | `			"TypeError",` |
|         - | 8286 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8287 | `			ph7_type_name(apArg[0])` |
|         - | 8288 | `			);` |
|         - | 8289 | `	}` |
|        13 | 8290 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8291 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8292 | `	if( pArray == 0 ){` |
|       ! 0 | 8293 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8294 | `		return PH7_OK;` |
|         - | 8295 | `	}` |
|         - | 8296 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8297 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8298 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8299 | `	pNode = pMap->pFirst;` |
|        33 | 8300 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8301 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8302 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8303 | `		if( pRow == 0 ){` |
|       ! 0 | 8304 | `			continue;` |
|         - | 8305 | `		}` |
|        21 | 8306 | `		if( bWantCol ){` |
|        19 | 8307 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8308 | `			if( pCol == 0 ){` |
|         - | 8309 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8310 | `				continue;` |
|         - | 8311 | `			}` |
|         9 | 8312 | `		}else{` |
|         3 | 8313 | `			pCol = pRow;` |
|         - | 8314 | `		}` |
|        19 | 8315 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8316 | `		if( pIdx ){` |
|        13 | 8317 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8318 | `		}else{` |
|         7 | 8319 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8320 | `		}` |
|        10 | 8321 | `	}` |
|        13 | 8322 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8323 | `	return PH7_OK;` |
|         7 | 8324 | `}` |
|         - | 8325 | `/*` |
|         - | 8326 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8327 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8328 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8329 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8330 | ` */` |
|        28 | 8331 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8332 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8333 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8334 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8335 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8336 | `	)` |
|         1 | 8337 | `{` |
|         - | 8338 | `	ph7_hashmap_node *pEntry;` |
|         - | 8339 | `	ph7_hashmap *pMap;` |
|         - | 8340 | `	ph7_value *pValue;` |
|         - | 8341 | `	ph7_value *apCbArg[2];` |
|         - | 8342 | `	ph7_value sKey;` |
|         - | 8343 | `	ph7_value sResult;` |
|         - | 8344 | `	sxi32 rc;` |
|         - | 8345 | `	sxu32 n;` |
|        29 | 8346 | `	*ppMatch = 0;` |
|        29 | 8347 | `	if( nArg < 2 ){` |
|       ! 0 | 8348 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8349 | `			"ArgumentCountError",` |
|         - | 8350 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8351 | `			zName,nArg` |
|         - | 8352 | `			);` |
|         - | 8353 | `	}` |
|        29 | 8354 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8355 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8356 | `			"TypeError",` |
|         - | 8357 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8358 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8359 | `			);` |
|         - | 8360 | `	}` |
|        29 | 8361 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8362 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8363 | `			"TypeError",` |
|         - | 8364 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8365 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8366 | `			);` |
|         - | 8367 | `	}` |
|        29 | 8368 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8369 | `	pEntry = pMap->pFirst;` |
|        29 | 8370 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8371 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8372 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8373 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8374 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8375 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8376 | `		if( pValue ){` |
|         - | 8377 | `			/* The callback receives ($value, $key). */` |
|        59 | 8378 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8379 | `			apCbArg[0] = pValue;` |
|        59 | 8380 | `			apCbArg[1] = &sKey;` |
|        59 | 8381 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8382 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8383 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8384 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8385 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8386 | `				return PH7_EXCEPTION;` |
|         - | 8387 | `			}` |
|        59 | 8388 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8389 | `				*ppMatch = pEntry;` |
|        15 | 8390 | `				break;` |
|         - | 8391 | `			}` |
|        22 | 8392 | `		}` |
|        45 | 8393 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8394 | `	}` |
|        29 | 8395 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8396 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8397 | `	return PH7_OK;` |
|        15 | 8398 | `}` |
|         - | 8399 | `/*` |
|         - | 8400 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8401 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8402 | ` *  is truthy, or NULL if none match.` |
|         - | 8403 | ` */` |
|         6 | 8404 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8405 | `{` |
|         - | 8406 | `	ph7_hashmap_node *pMatch;` |
|         - | 8407 | `	ph7_value *pVal;` |
|         - | 8408 | `	sxi32 rc;` |
|         7 | 8409 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8410 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8411 | `		return rc;` |
|         - | 8412 | `	}` |
|         7 | 8413 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8414 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8415 | `	}else{` |
|         3 | 8416 | `		ph7_result_null(pCtx);` |
|         - | 8417 | `	}` |
|         7 | 8418 | `	return PH7_OK;` |
|         4 | 8419 | `}` |
|         - | 8420 | `/*` |
|         - | 8421 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8422 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8423 | ` *  is truthy, or NULL if none match.` |
|         - | 8424 | ` */` |
|         6 | 8425 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8426 | `{` |
|         - | 8427 | `	ph7_hashmap_node *pMatch;` |
|         - | 8428 | `	sxi32 rc;` |
|         7 | 8429 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8430 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8431 | `		return rc;` |
|         - | 8432 | `	}` |
|         7 | 8433 | `	if( pMatch == 0 ){` |
|         3 | 8434 | `		ph7_result_null(pCtx);` |
|         6 | 8435 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8436 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8437 | `	}else{` |
|         4 | 8438 | `		ph7_result_string(pCtx,` |
|         2 | 8439 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8440 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8441 | `	}` |
|         7 | 8442 | `	return PH7_OK;` |
|         4 | 8443 | `}` |
|         - | 8444 | `/*` |
|         - | 8445 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8446 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8447 | ` *  FALSE for an empty array.` |
|         - | 8448 | ` */` |
|         8 | 8449 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8450 | `{` |
|         - | 8451 | `	ph7_hashmap_node *pMatch;` |
|         - | 8452 | `	sxi32 rc;` |
|         9 | 8453 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8454 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8455 | `		return rc;` |
|         - | 8456 | `	}` |
|         9 | 8457 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8458 | `	return PH7_OK;` |
|         5 | 8459 | `}` |
|         - | 8460 | `/*` |
|         - | 8461 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8462 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8463 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8464 | ` */` |
|         8 | 8465 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8466 | `{` |
|         - | 8467 | `	ph7_hashmap_node *pMatch;` |
|         - | 8468 | `	sxi32 rc;` |
|         9 | 8469 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8470 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8471 | `		return rc;` |
|         - | 8472 | `	}` |
|         9 | 8473 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8474 | `	return PH7_OK;` |
|         5 | 8475 | `}` |
|         - | 8476 | `/*` |
|         - | 8477 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8478 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8479 | ` */` |
|         - | 8480 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8481 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8482 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8483 | `{` |
|        84 | 8484 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8485 | `	(void)pVm;` |
|        84 | 8486 | `	p->nCount++;` |
|        84 | 8487 | `	if( p->pArray ){` |
|         - | 8488 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8489 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8490 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8491 | `	}` |
|        84 | 8492 | `	return SXRET_OK;` |
|         4 | 8493 | `}` |
|         - | 8494 | `/*` |
|         - | 8495 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8496 | ` */` |
|        30 | 8497 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8498 | `{` |
|         - | 8499 | `	struct IterCollect sCol;` |
|         - | 8500 | `	ph7_value *pArray;` |
|         - | 8501 | `	sxi32 rc;` |
|        34 | 8502 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8503 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8504 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8505 | `	sCol.pArray = pArray;` |
|        34 | 8506 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8507 | `	sCol.nCount = 0;` |
|        34 | 8508 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8509 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8510 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8511 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8512 | `		sxu32 n;` |
|         9 | 8513 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8514 | `			ph7_value sKey, *pVal;` |
|         7 | 8515 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8516 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8517 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8518 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8519 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8520 | `			pEntry = pEntry->pPrev;` |
|         4 | 8521 | `		}` |
|         3 | 8522 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8523 | `		return PH7_OK;` |
|         - | 8524 | `	}` |
|        32 | 8525 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8526 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8527 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8528 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8529 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8530 | `			ph7_type_name(apArg[0]));` |
|         - | 8531 | `	}` |
|        30 | 8532 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8533 | `	return PH7_OK;` |
|        19 | 8534 | `}` |
|         - | 8535 | `/*` |
|         - | 8536 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8537 | ` */` |
|         8 | 8538 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8539 | `{` |
|         - | 8540 | `	struct IterCollect sCol;` |
|         - | 8541 | `	sxi32 rc;` |
|         9 | 8542 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8543 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8544 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8545 | `		return PH7_OK;` |
|         - | 8546 | `	}` |
|         7 | 8547 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8548 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8549 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8550 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8551 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8552 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8553 | `			ph7_type_name(apArg[0]));` |
|         - | 8554 | `	}` |
|         7 | 8555 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8556 | `	return PH7_OK;` |
|         5 | 8557 | `}` |
|         - | 8558 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8559 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8560 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8561 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8562 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8563 | `{` |
|        33 | 8564 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8565 | `	ph7_value sResult;` |
|         - | 8566 | `	SySet aArg;` |
|         - | 8567 | `	sxi32 rc;` |
|         - | 8568 | `	int bContinue;` |
|        16 | 8569 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8570 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8571 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8572 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8573 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8574 | `		sxu32 n;` |
|        17 | 8575 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8576 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8577 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8578 | `			pEntry = pEntry->pPrev;` |
|         5 | 8579 | `		}` |
|         4 | 8580 | `	}` |
|        33 | 8581 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8582 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8583 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8584 | `	SySetRelease(&aArg);` |
|        33 | 8585 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8586 | `	p->nCount++;` |
|        31 | 8587 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8588 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8589 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8590 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8591 | `}` |
|         - | 8592 | `/*` |
|         - | 8593 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8594 | ` */` |
|        12 | 8595 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8596 | `{` |
|         - | 8597 | `	struct IterApply sApp;` |
|         - | 8598 | `	sxi32 rc;` |
|        13 | 8599 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8600 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8601 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8602 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8603 | `	}` |
|        13 | 8604 | `	sApp.pCallback = apArg[1];` |
|        13 | 8605 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8606 | `	sApp.nCount = 0;` |
|        13 | 8607 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8608 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8609 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8610 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8611 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8612 | `			ph7_type_name(apArg[0]));` |
|         - | 8613 | `	}` |
|        11 | 8614 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8615 | `	return PH7_OK;` |
|         7 | 8616 | `}` |
|         - | 8617 | `/*` |
|         - | 8618 | ` * Table of hashmap functions.` |
|         - | 8619 | ` */` |
|         - | 8620 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8621 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8622 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8623 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8624 | `	{"count",             ph7_hashmap_count },` |
|         - | 8625 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8626 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8627 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8628 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8629 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8630 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8631 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8632 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8633 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8634 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8635 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8636 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8637 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8638 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8639 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8640 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8641 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8642 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8643 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8644 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8645 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8646 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8647 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8648 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8649 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8650 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8651 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8652 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8653 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8654 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8655 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8656 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8657 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8658 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8659 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8660 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8661 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8662 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8663 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8664 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8665 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8666 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8667 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8668 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8669 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8670 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8671 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8672 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8673 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8674 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8675 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8676 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8677 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8678 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8679 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8680 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8681 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8682 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8683 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8684 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8685 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8686 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8687 | `	{"current",           ph7_hashmap_current },` |
|         - | 8688 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8689 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8690 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8691 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8692 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8693 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8694 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8695 | `};` |
|         - | 8696 | `/*` |
|         - | 8697 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8698 | ` */` |
|      3342 | 8699 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8700 | `{` |
|         - | 8701 | `	sxu32 n;` |
|    250655 | 8702 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    247313 | 8703 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    123659 | 8704 | `	}` |
|      3347 | 8705 | `}` |
|         - | 8706 | `/*` |
|         - | 8707 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8708 | ` * the BLOB given as the first argument.` |
|         - | 8709 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8710 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8711 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8712 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8713 | ` */` |
|         - | 8714 | `/*` |
|         - | 8715 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8716 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8717 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8718 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8719 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8720 | ` */` |
|       120 | 8721 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         3 | 8722 | `{` |
|       123 | 8723 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8724 | `	ph7_value *pObj;` |
|       123 | 8725 | `	sxu32 n = 0;` |
|         - | 8726 | `	int isRef;` |
|       123 | 8727 | `	sxi32 rc = SXRET_OK;` |
|         - | 8728 | `	int i;` |
|       195 | 8729 | `	for(;;){` |
|       393 | 8730 | `		if( n >= pMap->nEntry ){` |
|       123 | 8731 | `			break;` |
|         - | 8732 | `		}` |
|       273 | 8733 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       273 | 8734 | `		isRef = (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0;` |
|       273 | 8735 | `		if( ShowType ){` |
|         - | 8736 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8737 | `			 * on the next line at the same indent (php). */` |
|       105 | 8738 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        71 | 8739 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        37 | 8740 | `			}` |
|        37 | 8741 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        23 | 8742 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        12 | 8743 | `			}else{` |
|        21 | 8744 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8745 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8746 | `			}` |
|        37 | 8747 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        37 | 8748 | `			if( pObj ){` |
|        37 | 8749 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        37 | 8750 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8751 | `					break;` |
|         - | 8752 | `				}` |
|        17 | 8753 | `			}` |
|        20 | 8754 | `		}else{` |
|         - | 8755 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8756 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8757 | `			 * php's extra blank line. References carry no marker. */` |
|      1294 | 8758 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1058 | 8759 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       530 | 8760 | `			}` |
|       238 | 8761 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       125 | 8762 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        63 | 8763 | `			}else{` |
|       170 | 8764 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8765 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8766 | `			}` |
|       236 | 8767 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       132 | 8768 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8769 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8770 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8771 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8772 | `					break;` |
|         - | 8773 | `				}` |
|        13 | 8774 | `			}else{` |
|       214 | 8775 | `				if( pObj ){` |
|       214 | 8776 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       106 | 8777 | `				}` |
|       214 | 8778 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8779 | `			}` |
|         - | 8780 | `		}` |
|         - | 8781 | `		/* Point to the next entry */` |
|       273 | 8782 | `		n++;` |
|       273 | 8783 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 8784 | `	}` |
|       123 | 8785 | `	return rc;` |
|         3 | 8786 | `}` |
|       116 | 8787 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8788 | `{` |
|         - | 8789 | `	sxi32 rc;` |
|         - | 8790 | `	int i;` |
|       118 | 8791 | `	if( nDepth > 31 ){` |
|         - | 8792 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8793 | `		/* Nesting limit reached */` |
|       ! 0 | 8794 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8795 | `		return SXERR_LIMIT;` |
|         - | 8796 | `	}` |
|       118 | 8797 | `	if( ShowType ){` |
|         - | 8798 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8799 | `		 * newline (a nested array is itself an entry value line). */` |
|        14 | 8800 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        14 | 8801 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        14 | 8802 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        14 | 8803 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8804 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8805 | `		}` |
|        14 | 8806 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        14 | 8807 | `		return rc;` |
|         - | 8808 | `	}` |
|         - | 8809 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       105 | 8810 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       297 | 8811 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8812 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8813 | `	}` |
|       105 | 8814 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       105 | 8815 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       297 | 8816 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8817 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8818 | `	}` |
|       105 | 8819 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       105 | 8820 | `	return rc;` |
|        60 | 8821 | `}` |
|         - | 8822 | `/*` |
|         - | 8823 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8824 | ` * retrieved entry.` |
|         - | 8825 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8826 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8827 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8828 | ` * a value different from PH7_OK.` |
|         - | 8829 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8830 | ` */` |
|     33754 | 8831 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8832 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8833 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8834 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8835 | `	)` |
|         5 | 8836 | `{` |
|         - | 8837 | `	ph7_hashmap_node *pEntry;` |
|         - | 8838 | `	ph7_value sKey,sValue;` |
|         - | 8839 | `	sxi32 rc;` |
|         - | 8840 | `	sxu32 n;` |
|         - | 8841 | `	/* Initialize walker parameter */` |
|     33759 | 8842 | `	rc = SXRET_OK;` |
|     33759 | 8843 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     33759 | 8844 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     33759 | 8845 | `	n = pMap->nEntry;` |
|     33759 | 8846 | `	pEntry = pMap->pFirst;` |
|         - | 8847 | `	/* Start the iteration process */` |
|     91914 | 8848 | `	for(;;){` |
|    183833 | 8849 | `		if( n < 1 ){` |
|     33759 | 8850 | `			break;` |
|         - | 8851 | `		}` |
|         - | 8852 | `		/* Extract a copy of the key and a copy the current value */` |
|    150079 | 8853 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    150079 | 8854 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8855 | `		/* Invoke the user callback */` |
|    150079 | 8856 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8857 | `		/* Release the copy of the key and the value */` |
|    150079 | 8858 | `		PH7_MemObjRelease(&sKey);` |
|    150079 | 8859 | `		PH7_MemObjRelease(&sValue);` |
|    150079 | 8860 | `		if( rc != PH7_OK ){` |
|         - | 8861 | `			/* Callback request an operation abort */` |
|       ! 0 | 8862 | `			return SXERR_ABORT;` |
|         - | 8863 | `		}` |
|         - | 8864 | `		/* Point to the next entry */` |
|    150079 | 8865 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    150079 | 8866 | `		n--;` |
|         5 | 8867 | `	}` |
|         - | 8868 | `	/* All done */` |
|     33759 | 8869 | `	return SXRET_OK;` |
|     16882 | 8870 | `}` |
|         - | 8871 |  |
