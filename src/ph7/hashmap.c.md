# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1110/1206 lines (92.04%)

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
|         - |   16 | `/* HASHMAP_NODE_FOREIGN_OBJ (node control flag) is declared in ph7int.h too. */` |
|         - |   17 | `/*` |
|         - |   18 | ` * Default hash function for int [i.e; 64-bit integer] keys.` |
|         - |   19 | ` */` |
|   9502980 |   20 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   21 | `{` |
|   9502985 |   22 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   9502985 |   23 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   24 | `}` |
|         - |   25 | `/*` |
|         - |   26 | ` * Default hash function for string/BLOB keys.` |
|         - |   27 | ` */` |
|   5693596 |   28 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   29 | `{` |
|   5693601 |   30 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   31 | `	unsigned char *zEnd;` |
|   5693601 |   32 | `	sxu32 nH = 5381;` |
|   5693601 |   33 | `	zEnd = &zIn[nLen];` |
|   6403841 |   34 | `	for(;;){` |
|  12807687 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   8479647 |   36 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   7363717 |   37 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   7232623 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   39 | `	}` |
|   5693601 |   40 | `	return nH;` |
|         5 |   41 | `}` |
|         - |   42 | `/*` |
|         - |   43 | ` * Return the total number of entries in a given hashmap.` |
|         - |   44 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   45 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   46 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   47 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   48 | ` */` |
|      1138 |   49 | `PH7_PRIVATE sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   50 | `{` |
|      1143 |   51 | `	sxi64 iCount = 0;` |
|      1143 |   52 | `	if( !bRecursive ){` |
|       969 |   53 | `		iCount = pMap->nEntry;` |
|       487 |   54 | `	}else{` |
|         - |   55 | `		/* Recursive hashmap walk */` |
|       175 |   56 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|         - |   57 | `		ph7_value *pElem;` |
|       175 |   58 | `		sxu32 n = 0;` |
|         - |   59 | `		/* Mark this map as being counted */` |
|       175 |   60 | `		pMap->iFlags \|= HASHMAP_COUNTING;` |
|       215 |   61 | `		for(;;){` |
|       431 |   62 | `			if( n >= pMap->nEntry ){` |
|       175 |   63 | `				break;` |
|         - |   64 | `			}` |
|         - |   65 | `			/* Point to the element value */` |
|       257 |   66 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|       257 |   67 | `			if( pElem ){` |
|       257 |   68 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|       151 |   69 | `					ph7_hashmap *pSub = (ph7_hashmap *)pElem->x.pOther;` |
|       151 |   70 | `					if( pSub->iFlags & HASHMAP_COUNTING ){` |
|         - |   71 | `						/* Cycle detected — skip this entry */` |
|         3 |   72 | `						if( pCycleDetected ){` |
|         3 |   73 | `							*pCycleDetected = TRUE;` |
|         1 |   74 | `						}` |
|         2 |   75 | `					}else{` |
|       149 |   76 | `						iCount += HashmapCount(pSub,TRUE,pCycleDetected);` |
|         - |   77 | `					}` |
|        75 |   78 | `				}` |
|       128 |   79 | `			}` |
|         - |   80 | `			/* Point to the next entry */` |
|       257 |   81 | `			pEntry = pEntry->pNext;` |
|       257 |   82 | `			++n;` |
|         1 |   83 | `		}` |
|         - |   84 | `		/* Clear the counting flag */` |
|       175 |   85 | `		pMap->iFlags &= ~HASHMAP_COUNTING;` |
|         - |   86 | `		/* Update count */` |
|       175 |   87 | `		iCount += pMap->nEntry;` |
|         - |   88 | `	}` |
|      1143 |   89 | `	return iCount;` |
|         5 |   90 | `}` |
|         - |   91 | `/*` |
|         - |   92 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   93 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   94 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   95 | ` */` |
|   4571200 |   96 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |   97 | `{` |
|         - |   98 | `	ph7_hashmap_node *pNode;` |
|         - |   99 | `	/* Allocate a new node */` |
|   4571205 |  100 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   4571205 |  101 | `	if( pNode == 0 ){` |
|       ! 0 |  102 | `		return 0;` |
|         - |  103 | `	}` |
|         - |  104 | `	/* Zero the stucture */` |
|   4571205 |  105 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  106 | `	/* Fill in the structure */` |
|   4571205 |  107 | `	pNode->pMap  = &(*pMap);` |
|   4571205 |  108 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   4571205 |  109 | `	pNode->nHash = nHash;` |
|   4571205 |  110 | `	pNode->xKey.iKey = iKey;` |
|   4571205 |  111 | `	pNode->nValIdx  = nValIdx;` |
|   4571205 |  112 | `	return pNode;` |
|   2285605 |  113 | `}` |
|         - |  114 | `/*` |
|         - |  115 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  116 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  117 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  118 | ` */` |
|   3082642 |  119 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  120 | `{` |
|         - |  121 | `	ph7_hashmap_node *pNode;` |
|         - |  122 | `	/* Allocate a new node */` |
|   3082647 |  123 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3082647 |  124 | `	if( pNode == 0 ){` |
|       ! 0 |  125 | `		return 0;` |
|         - |  126 | `	}` |
|         - |  127 | `	/* Zero the stucture */` |
|   3082647 |  128 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  129 | `	/* Fill in the structure */` |
|   3082647 |  130 | `	pNode->pMap  = &(*pMap);` |
|   3082647 |  131 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   3082647 |  132 | `	pNode->nHash = nHash;` |
|   3082647 |  133 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   3082647 |  134 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   3082647 |  135 | `	pNode->nValIdx = nValIdx;` |
|   3082647 |  136 | `	return pNode;` |
|   1541326 |  137 | `}` |
|         - |  138 | `/*` |
|         - |  139 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  140 | ` */` |
|   7653842 |  141 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  142 | `{` |
|         - |  143 | `	/* Link */` |
|   7653847 |  144 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   4113127 |  145 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   4113127 |  146 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   2056561 |  147 | `	}` |
|   7653847 |  148 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  149 | `	/* Link to the map list */` |
|   7653847 |  150 | `	if( pMap->pFirst == 0 ){` |
|   1365927 |  151 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  152 | `		/* Point to the first inserted node */` |
|   1365927 |  153 | `		pMap->pCur = pNode;` |
|    682966 |  154 | `	}else{` |
|   6287925 |  155 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  156 | `	}` |
|   7653847 |  157 | `	if( pMap->pActiveSteps ){` |
|         - |  158 | `		/* Re-arm any live foreach cursor parked past the end: php's by-ref` |
|         - |  159 | `		 * foreach iterates the LIVE array, so an element appended while the` |
|         - |  160 | `		 * loop stands on the last node (worklist idiom), or after the body` |
|         - |  161 | `		 * emptied the map, is still visited. A registered step with a NULL` |
|         - |  162 | `		 * cursor is always mid-loop — natural exhaustion unregisters before` |
|         - |  163 | `		 * the loop ends. */` |
|         - |  164 | `		ph7_foreach_step *pStep;` |
|        34 |  165 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        18 |  166 | `			if( pStep->pCursor == 0 ){` |
|        14 |  167 | `				pStep->pCursor = pNode;` |
|         6 |  168 | `			}` |
|        10 |  169 | `		}` |
|         8 |  170 | `	}` |
|   7653847 |  171 | `	++pMap->nEntry;` |
|   7653847 |  172 | `}` |
|         - |  173 | `/*` |
|         - |  174 | ` * Unlink a node from the hashmap.` |
|         - |  175 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  176 | ` */` |
|      7846 |  177 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  178 | `{` |
|      7851 |  179 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7851 |  180 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  181 | `	/* Unlink from the corresponding bucket */` |
|      7851 |  182 | `	if( pNode->pPrevCollide == 0 ){` |
|      7279 |  183 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3642 |  184 | `	}else{` |
|       575 |  185 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  186 | `	}` |
|      7851 |  187 | `	if( pNode->pNextCollide ){` |
|      5129 |  188 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2562 |  189 | `	}` |
|      7851 |  190 | `	if( pMap->pFirst == pNode ){` |
|       297 |  191 | `		pMap->pFirst = pNode->pPrev;` |
|       146 |  192 | `	}` |
|      7851 |  193 | `	if( pMap->pCur == pNode ){` |
|         - |  194 | `		/* Advance the node cursor */` |
|       295 |  195 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|       145 |  196 | `	}` |
|      7851 |  197 | `	if( pMap->pActiveSteps ){` |
|         - |  198 | `		/* Advance any live foreach cursor parked on this node (delete during` |
|         - |  199 | `		 * live-map iteration: by-ref foreach, $GLOBALS, snapshot fallbacks). */` |
|         - |  200 | `		ph7_foreach_step *pStep;` |
|        29 |  201 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        15 |  202 | `			if( pStep->pCursor == pNode ){` |
|         3 |  203 | `				pStep->pCursor = pNode->pPrev; /* Reverse link */` |
|         1 |  204 | `			}` |
|         8 |  205 | `		}` |
|         7 |  206 | `	}` |
|         - |  207 | `	/* Unlink from the map list */` |
|      7851 |  208 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7851 |  209 | `	if( bRestore ){` |
|         - |  210 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|      1179 |  211 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  212 | `		/* Restore to the freelist — but only if this node was the LAST holder. A` |
|         - |  213 | `` 		 * node's own value can be held by a name too (`$r = &$a[0]; unset($a);` `` |
|         - |  214 | `		 * must leave $r reading 7, not destroy it), and a FOREIGN node may be the` |
|         - |  215 | `		 * last thing holding a slot whose frame is already gone (that frame left it` |
|         - |  216 | `		 * standing for this very node), which nothing else would ever free. */` |
|      1179 |  217 | `		PH7_VmReleaseUnheldSlot(pVm,pNode->nValIdx);` |
|       587 |  218 | `	}` |
|      7851 |  219 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7403 |  220 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3699 |  221 | `	}` |
|      7851 |  222 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7851 |  223 | `	pMap->nEntry--;` |
|      7851 |  224 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  225 | `		/* Free the hash-bucket */` |
|       135 |  226 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|       135 |  227 | `		pMap->apBucket = 0;` |
|       135 |  228 | `		pMap->nSize = 0;` |
|       135 |  229 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        65 |  230 | `	}` |
|      7851 |  231 | `}` |
|         - |  232 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  233 | `/*` |
|         - |  234 | ` * Grow the hash-table and rehash all entries.` |
|         - |  235 | ` */` |
|   7653842 |  236 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  237 | `{` |
|   7653847 |  238 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   1373325 |  239 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  240 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   1373325 |  241 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  242 | `		sxu32 nBucket;` |
|         - |  243 | `		sxu32 n;` |
|   1373325 |  244 | `		if( nNew < 1 ){` |
|   1365927 |  245 | `			nNew = 16;` |
|    682961 |  246 | `		}` |
|         - |  247 | `		/* Allocate a new bucket */` |
|   1373325 |  248 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   1373325 |  249 | `		if( apNew == 0 ){` |
|       ! 0 |  250 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  251 | `				return SXERR_MEM; /* Fatal */` |
|         - |  252 | `			}` |
|         - |  253 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  254 | `			return SXRET_OK;` |
|         - |  255 | `		}` |
|         - |  256 | `		/* Zero the table */` |
|   1373325 |  257 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  258 | `		/* Reflect the change */` |
|   1373325 |  259 | `		pMap->apBucket = apNew;` |
|   1373325 |  260 | `		pMap->nSize = nNew;` |
|   1373325 |  261 | `		if( apOld == 0 ){` |
|         - |  262 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   1365927 |  263 | `			return SXRET_OK;` |
|         - |  264 | `		}` |
|         - |  265 | `		/* Rehash old entries */` |
|      7403 |  266 | `		pEntry = pMap->pFirst;` |
|      7403 |  267 | `		n = 0;` |
|   2659011 |  268 | `		for( ;; ){` |
|   5318027 |  269 | `			if( n >= pMap->nEntry ){` |
|      7403 |  270 | `				break;` |
|         - |  271 | `			}` |
|         - |  272 | `			/* Clear the old collision link */` |
|   5310629 |  273 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  274 | `			/* Link to the new bucket */` |
|   5310629 |  275 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   5310629 |  276 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   4521367 |  277 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   4521367 |  278 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   2260681 |  279 | `			}` |
|   5310629 |  280 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  281 | `			/* Point to the next entry */` |
|   5310629 |  282 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   5310629 |  283 | `			n++;` |
|         5 |  284 | `		}` |
|         - |  285 | `		/* Free the old table */` |
|      7403 |  286 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      3699 |  287 | `	}` |
|   6287925 |  288 | `	return SXRET_OK;` |
|   3826926 |  289 | `}` |
|         - |  290 | `/*` |
|         - |  291 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  292 | ` * hashmap.` |
|         - |  293 | ` */` |
|   4571200 |  294 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  295 | `{` |
|         - |  296 | `	ph7_hashmap_node *pNode;` |
|         - |  297 | `	sxu32 nIdx;` |
|         - |  298 | `	sxu32 nHash;` |
|         - |  299 | `	sxi32 rc;` |
|   4571205 |  300 | `	if( !isForeign ){` |
|         - |  301 | `		ph7_value *pObj;` |
|         - |  302 | `		ph7_value sSafeVal;` |
|         - |  303 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  304 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  305 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  306 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  307 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  308 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   4571071 |  309 | `		if( pValue ){` |
|   4571035 |  310 | `			sSafeVal = *pValue;` |
|   4571035 |  311 | `			pValue = &sSafeVal;` |
|   2285515 |  312 | `		}` |
|         - |  313 | `		/* Reserve a ph7_value for the value */` |
|   4571071 |  314 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   4571071 |  315 | `		if( pObj == 0 ){` |
|       ! 0 |  316 | `			return SXERR_MEM;` |
|         - |  317 | `		}` |
|   4571071 |  318 | `		if( pValue ){` |
|         - |  319 | `			/* Duplicate the value */` |
|   4571035 |  320 | `			PH7_MemObjStore(pValue,pObj);` |
|   2285515 |  321 | `		}` |
|   4571071 |  322 | `		nIdx = pObj->nIdx;` |
|   2285538 |  323 | `	}else{` |
|       138 |  324 | `		nIdx = nRefIdx;` |
|         - |  325 | `	}` |
|         - |  326 | `	/* Hash the key */` |
|   4571205 |  327 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  328 | `	/* Allocate a new int node */` |
|   4571205 |  329 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   4571205 |  330 | `	if( pNode == 0 ){` |
|       ! 0 |  331 | `		return SXERR_MEM;` |
|         - |  332 | `	}` |
|   4571205 |  333 | `	if( isForeign ){` |
|         - |  334 | `		/* Mark as a foregin entry */` |
|       138 |  335 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        67 |  336 | `	}` |
|         - |  337 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   4571205 |  338 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   4571205 |  339 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  340 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  341 | `		return rc;` |
|         - |  342 | `	}` |
|         - |  343 | `	/* Perform the insertion */` |
|   4571205 |  344 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  345 | `	/* Install in the reference table */` |
|   4571205 |  346 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  347 | `	/* All done */` |
|   4571205 |  348 | `	return SXRET_OK;` |
|   2285605 |  349 | `}` |
|         - |  350 | `/*` |
|         - |  351 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  352 | ` * hashmap.` |
|         - |  353 | ` */` |
|   3082642 |  354 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  355 | `{` |
|         - |  356 | `	ph7_hashmap_node *pNode;` |
|         - |  357 | `	sxu32 nHash;` |
|         - |  358 | `	sxu32 nIdx;` |
|         - |  359 | `	sxi32 rc;` |
|   3082647 |  360 | `	if( !isForeign ){` |
|         - |  361 | `		ph7_value *pObj;` |
|         - |  362 | `		ph7_value sSafeVal;` |
|         - |  363 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  364 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  365 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  366 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  367 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  368 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3026459 |  369 | `		if( pValue ){` |
|   3026363 |  370 | `			sSafeVal = *pValue;` |
|   3026363 |  371 | `			pValue = &sSafeVal;` |
|   1513179 |  372 | `		}` |
|         - |  373 | `		/* Reserve a ph7_value for the value */` |
|   3026459 |  374 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3026459 |  375 | `		if( pObj == 0 ){` |
|       ! 0 |  376 | `			return SXERR_MEM;` |
|         - |  377 | `		}` |
|   3026459 |  378 | `		if( pValue ){` |
|         - |  379 | `			/* Duplicate the value */` |
|   3026363 |  380 | `			PH7_MemObjStore(pValue,pObj);` |
|   1513179 |  381 | `		}` |
|   3026459 |  382 | `		nIdx = pObj->nIdx;` |
|   1513232 |  383 | `	}else{` |
|     56193 |  384 | `		nIdx = nRefIdx;` |
|         - |  385 | `	}` |
|         - |  386 | `	/* Hash the key */` |
|   3082647 |  387 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  388 | `	/* Allocate a new blob node */` |
|   3082647 |  389 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   3082647 |  390 | `	if( pNode == 0 ){` |
|       ! 0 |  391 | `		return SXERR_MEM;` |
|         - |  392 | `	}` |
|   3082647 |  393 | `	if( isForeign ){` |
|         - |  394 | `		/* Mark as a foregin entry */` |
|     56193 |  395 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     28094 |  396 | `	}` |
|         - |  397 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3082647 |  398 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3082647 |  399 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  400 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  401 | `		return rc;` |
|         - |  402 | `	}` |
|         - |  403 | `	/* Perform the insertion */` |
|   3082647 |  404 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  405 | `	/* Install in the reference table */` |
|   3082647 |  406 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  407 | `	/* All done */` |
|   3082647 |  408 | `	return SXRET_OK;` |
|   1541326 |  409 | `}` |
|         - |  410 | `/*` |
|         - |  411 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  412 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  413 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  414 | ` */` |
|   4916792 |  415 | `PH7_PRIVATE sxi32 HashmapLookupIntKey(` |
|         - |  416 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  417 | `	sxi64 iKey,                /* lookup key */` |
|         - |  418 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  419 | `	)` |
|         5 |  420 | `{` |
|         - |  421 | `	ph7_hashmap_node *pNode;` |
|         - |  422 | `	sxu32 nHash;` |
|   4916797 |  423 | `	if( pMap->nEntry < 1 ){` |
|         - |  424 | `		/* Don't bother hashing,there is no entry anyway */` |
|      4993 |  425 | `		return SXERR_NOTFOUND;` |
|         - |  426 | `	}` |
|         - |  427 | `	/* Hash the key first */` |
|   4911809 |  428 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  429 | `	/* Point to the appropriate bucket */` |
|   4911809 |  430 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  431 | `	/* Perform the lookup */` |
| 110881936 |  432 | `	for(;;){` |
| 221763854 |  433 | `		if( pNode == 0 ){` |
|   4296051 |  434 | `			break;` |
|         - |  435 | `		}` |
| 217467803 |  436 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 217464721 |  437 | `			&& pNode->nHash == nHash` |
| 109038711 |  438 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  439 | `				/* Node found */` |
|    615763 |  440 | `				if( ppNode ){` |
|    615729 |  441 | `					*ppNode = pNode;` |
|    307862 |  442 | `				}` |
|    615763 |  443 | `				return SXRET_OK;` |
|         - |  444 | `		}` |
|         - |  445 | `		/* Follow the collision link */` |
| 216852047 |  446 | `		pNode = pNode->pNextCollide;` |
|         2 |  447 | `	}` |
|         - |  448 | `	/* No such entry */` |
|   4296051 |  449 | `	return SXERR_NOTFOUND;` |
|   2458405 |  450 | `}` |
|         - |  451 | `/*` |
|         - |  452 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  453 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  454 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  455 | ` */` |
|   3272426 |  456 | `PH7_PRIVATE sxi32 HashmapLookupBlobKey(` |
|         - |  457 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  458 | `	const void *pKey,           /* Lookup key */` |
|         - |  459 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  460 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  461 | `	)` |
|         5 |  462 | `{` |
|         - |  463 | `	ph7_hashmap_node *pNode;` |
|         - |  464 | `	sxu32 nHash;` |
|   3272431 |  465 | `	if( pMap->nEntry < 1 ){` |
|         - |  466 | `		/* Don't bother hashing,there is no entry anyway */` |
|    661477 |  467 | `		return SXERR_NOTFOUND;` |
|         - |  468 | `	}` |
|         - |  469 | `	/* Hash the key first */` |
|   2610959 |  470 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  471 | `	/* Point to the appropriate bucket */` |
|   2610959 |  472 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  473 | `	/* Perform the lookup */` |
|   1661274 |  474 | `	for(;;){` |
|   3322553 |  475 | `		if( pNode == 0 ){` |
|   2521483 |  476 | `			break;` |
|         - |  477 | `		}` |
|    801070 |  478 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    799378 |  479 | `			&& pNode->nHash == nHash` |
|    443655 |  480 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     89629 |  481 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  482 | `				/* Node found */` |
|     89481 |  483 | `				if( ppNode ){` |
|     89449 |  484 | `					*ppNode = pNode;` |
|     44722 |  485 | `				}` |
|     89481 |  486 | `				return SXRET_OK;` |
|         - |  487 | `		}` |
|         - |  488 | `		/* Follow the collision link */` |
|    711599 |  489 | `		pNode = pNode->pNextCollide;` |
|         5 |  490 | `	}` |
|         - |  491 | `	/* No such entry */` |
|   2521483 |  492 | `	return SXERR_NOTFOUND;` |
|   1636218 |  493 | `}` |
|         - |  494 | `/*` |
|         - |  495 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  496 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  497 | ` */` |
|   3272632 |  498 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  499 | `{` |
|   3272637 |  500 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|   3272637 |  501 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  502 | `	const char *zDigit;` |
|   3272637 |  503 | `	int isNeg = FALSE, nDigit;` |
|   3272637 |  504 | `	if( zIn >= zEnd ){` |
|       103 |  505 | `		return FALSE;` |
|         - |  506 | `	}` |
|         - |  507 | `	/* php's rule (_zend_handle_numeric_str_ex), byte for byte:` |
|         - |  508 | `	 *   - a leading '-' is allowed, a leading '+' is NOT ('+1' stays a` |
|         - |  509 | `	 *     string key)` |
|         - |  510 | `	 *   - after the sign, a leading '0' disqualifies the key unless the` |
|         - |  511 | `	 *     WHOLE key is the single digit "0" -- so "00", "01", "-0" and` |
|         - |  512 | `	 *     "-01" all stay string keys. The length is measured over the key,` |
|         - |  513 | `	 *     sign included, which is what makes "-0" fail.` |
|         - |  514 | `	 * PH7 tested the leading zero BEFORE skipping the sign and accepted '+',` |
|         - |  515 | `	 * so $a['-0'], $a['+0'], $a['+1'] and $a['-01'] canonicalised onto the` |
|         - |  516 | `	 * integer keys 0/0/1/-1 -- silently COLLIDING with a genuine 0/1/-1 entry` |
|         - |  517 | `	 * ($a = ['-0'=>'a','0'=>'d'] kept one element where php keeps two) and` |
|         - |  518 | `	 * carrying the wrong key through array_keys/array_flip/json_decode/` |
|         - |  519 | `	 * serialize/array_count_values alike. */` |
|   3272537 |  520 | `	if( zIn[0] == '-' && &zIn[1] < zEnd ){` |
|        93 |  521 | `		isNeg = TRUE;` |
|        93 |  522 | `		zIn++;` |
|        45 |  523 | `	}` |
|   3272537 |  524 | `	if( zIn < zEnd && zIn[0] == '0' && SyBlobLength(pKey) > 1 ){` |
|         - |  525 | `		/* Leading zero: octal-looking, signed zero, or just padded */` |
|       149 |  526 | `		return FALSE;` |
|         - |  527 | `	}` |
|   3272391 |  528 | `	zDigit = zIn;` |
|   1637360 |  529 | `	for(;;){` |
|   3274725 |  530 | `		if( zIn >= zEnd ){` |
|       847 |  531 | `			break;` |
|         - |  532 | `		}` |
|   3273883 |  533 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  534 | `			/* Key does not look like a decimal number */` |
|   3271549 |  535 | `			return FALSE;` |
|         - |  536 | `		}` |
|      2339 |  537 | `		zIn++;` |
|         5 |  538 | `	}` |
|         - |  539 | `	/* An all-digit key that overflows the signed 64-bit range is NOT an integer` |
|         - |  540 | `	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip` |
|         - |  541 | `	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to` |
|         - |  542 | `	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */` |
|       847 |  543 | `	nDigit = (int)(zEnd - zDigit);` |
|       847 |  544 | `	if( nDigit < 1 ){` |
|         - |  545 | `		/* A lone "-" (the digit loop rejects it first; kept defensive) */` |
|       ! 0 |  546 | `		return FALSE;` |
|         - |  547 | `	}` |
|       866 |  548 | `	if( nDigit > 19 \|\|` |
|       439 |  549 | `		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){` |
|        22 |  550 | `		return FALSE;` |
|         - |  551 | `	}` |
|       827 |  552 | `	return TRUE;` |
|   1636321 |  553 | `}` |
|         - |  554 | `/*` |
|         - |  555 | ` * TRUE when this key value lands on an INTEGER key — the same fold HashmapLookup` |
|         - |  556 | ` * and HashmapInsert perform below, exposed so a DIAGNOSTIC can name the key the` |
|         - |  557 | ` * way the lookup saw it rather than the way it was written ($a["10"] misses the` |
|         - |  558 | `` * integer key 10, so php's warning says `Undefined array key 10`, unquoted).`` |
|         - |  559 | ` * A non-integer key is left as a STRING with its blob ready to print — including` |
|         - |  560 | ` * the NULL key, which folds to "" exactly as the lookup folds it.` |
|         - |  561 | ` */` |
|        72 |  562 | `PH7_PRIVATE int PH7_HashmapKeyIsInt(ph7_value *pKey)` |
|         5 |  563 | `{` |
|        77 |  564 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|        65 |  565 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  566 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  567 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  568 | `		}` |
|        65 |  569 | `		return HashmapIsIntKey(&pKey->sBlob) ? TRUE : FALSE;` |
|         - |  570 | `	}` |
|         - |  571 | `	/* int / float / BOOL all reach an integer key ($a[false] is $a[0]) */` |
|        14 |  572 | `	return TRUE;` |
|        41 |  573 | `}` |
|         - |  574 | `/*` |
|         - |  575 | ` * Check if a given key exists in the given hashmap.` |
|         - |  576 | ` * Write a pointer to the target node on success.` |
|         - |  577 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  578 | ` */` |
|    800830 |  579 | `static sxi32 HashmapLookup(` |
|         - |  580 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  581 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  582 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  583 | `	)` |
|         5 |  584 | `{` |
|    800835 |  585 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  586 | `	sxi32 rc;` |
|    800835 |  587 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    190099 |  588 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  589 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|        32 |  590 | `			PH7_MemObjToString(&(*pKey));` |
|        15 |  591 | `		}` |
|    190099 |  592 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  593 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  594 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  595 | `			 * to an integer lookup for key 0. */` |
|    189787 |  596 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    189787 |  597 | `			goto result;` |
|         - |  598 | `		}` |
|       156 |  599 | `	}` |
|         - |  600 | `	/* Perform an int lookup */` |
|    611053 |  601 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  602 | `		/* Force an integer cast */` |
|       333 |  603 | `		PH7_MemObjToInteger(pKey);` |
|       164 |  604 | `	}` |
|         - |  605 | `	/* Perform an int lookup */` |
|    611053 |  606 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|    400415 |  607 | `result:` |
|    800835 |  608 | `	if( rc == SXRET_OK ){` |
|         - |  609 | `		/* Node found */` |
|    699427 |  610 | `		if( ppNode ){` |
|    699369 |  611 | `			*ppNode = pNode;` |
|    349682 |  612 | `		}` |
|    699427 |  613 | `		return SXRET_OK;` |
|         - |  614 | `	}` |
|         - |  615 | `	/* No such entry */` |
|    101413 |  616 | `	return SXERR_NOTFOUND;` |
|    400420 |  617 | `}` |
|         - |  618 | `/*` |
|         - |  619 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  620 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  621 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  622 | ` * via HashmapAppendIndexBusy.` |
|         - |  623 | ` */` |
|   2150206 |  624 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  625 | `{` |
|   2150211 |  626 | `	if( !pMap->bIntKeySeen ){` |
|         - |  627 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|      4923 |  628 | `		pMap->bIntKeySeen = 1;` |
|      4923 |  629 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|      4923 |  630 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  631 | `			pMap->iNextIdx++;` |
|       ! 0 |  632 | `		}` |
|      4923 |  633 | `		return;` |
|         - |  634 | `	}` |
|   2145293 |  635 | `	if( iKey >= pMap->iNextIdx ){` |
|   2144953 |  636 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  637 | `		/* Make sure the automatic index is not reserved */` |
|   2144953 |  638 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  639 | `			pMap->iNextIdx++;` |
|       ! 0 |  640 | `		}` |
|   1072478 |  641 | `	}` |
|   1075108 |  642 | `}` |
|         - |  643 | `/*` |
|         - |  644 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  645 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  646 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  647 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  648 | ` */` |
|   2414842 |  649 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  650 | `{` |
|   2414847 |  651 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  652 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  653 | `		return TRUE;` |
|         - |  654 | `	}` |
|   2414841 |  655 | `	return FALSE;` |
|   1207426 |  656 | `}` |
|         - |  657 | `/*` |
|         - |  658 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  659 | ` * hashmap.` |
|         - |  660 | ` * If a node with the given key already exists in the database` |
|         - |  661 | ` * then this function overwrite the old value.` |
|         - |  662 | ` */` |
|   7591886 |  663 | `PH7_PRIVATE sxi32 HashmapInsert(` |
|         - |  664 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  665 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  666 | `	ph7_value *pVal    /* Node value */` |
|         - |  667 | `	)` |
|         5 |  668 | `{` |
|   7591891 |  669 | `	ph7_hashmap_node *pNode = 0;` |
|   7591891 |  670 | `	sxi32 rc = SXRET_OK;` |
|   7591891 |  671 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|   3026287 |  672 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  673 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  674 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  675 | `			 * path and filed it under 0). */` |
|         7 |  676 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  677 | `		}` |
|   3026287 |  678 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       502 |  679 | `			goto IntKey;` |
|         - |  680 | `		}` |
|         - |  681 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  682 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  683 | `		 * overwriting nothing and bumping the auto-index). */` |
|   4538681 |  684 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   1512892 |  685 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  686 | `				/* Overwrite the old value */` |
|         - |  687 | `				ph7_value *pElem;` |
|       257 |  688 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       257 |  689 | `				if( pElem ){` |
|       257 |  690 | `					if( pVal ){` |
|       257 |  691 | `						PH7_MemObjStore(pVal,pElem);` |
|       131 |  692 | `					}else{` |
|         - |  693 | `						/* Nullify the entry */` |
|       ! 0 |  694 | `						PH7_MemObjToNull(pElem);` |
|         - |  695 | `					}` |
|       126 |  696 | `				}` |
|       257 |  697 | `				return SXRET_OK;` |
|         - |  698 | `		}` |
|   3025537 |  699 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  700 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  701 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       162 |  702 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  703 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  704 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  705 | `				return SXRET_OK;` |
|         - |  706 | `			}` |
|       242 |  707 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       160 |  708 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        80 |  709 | `				pVal,SXU32_HIGH);` |
|         - |  710 | `		}` |
|         - |  711 | `		/* Perform a blob-key insertion */` |
|   3025377 |  712 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   3025377 |  713 | `		return rc;` |
|         - |  714 | `	}` |
|   2282802 |  715 | `IntKey:` |
|   4566107 |  716 | `	if( pKey ){` |
|   2151379 |  717 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  718 | `			/* Force an integer cast */` |
|       516 |  719 | `			PH7_MemObjToInteger(pKey);` |
|       256 |  720 | `		}` |
|   2151379 |  721 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  722 | `			/* Overwrite the old value */` |
|         - |  723 | `			ph7_value *pElem;` |
|      1178 |  724 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      1178 |  725 | `			if( pElem ){` |
|      1178 |  726 | `				if( pVal ){` |
|      1178 |  727 | `					PH7_MemObjStore(pVal,pElem);` |
|       590 |  728 | `				}else{` |
|         - |  729 | `					/* Nullify the entry */` |
|       ! 0 |  730 | `					PH7_MemObjToNull(pElem);` |
|         - |  731 | `				}` |
|       588 |  732 | `			}` |
|      1178 |  733 | `			return SXRET_OK;` |
|         - |  734 | `		}` |
|   2150203 |  735 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  736 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  737 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  738 | `			char zKey[24];` |
|         3 |  739 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  740 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  741 | `		}` |
|         - |  742 | `		/* Perform a 64-bit-int-key insertion */` |
|   2150201 |  743 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2150201 |  744 | `		if( rc == SXRET_OK ){` |
|   2150201 |  745 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1075098 |  746 | `		}` |
|   1075103 |  747 | `	}else{` |
|   2414733 |  748 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  749 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  750 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  751 | `		}` |
|   2414731 |  752 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  753 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  754 | `		}` |
|         - |  755 | `		/* Assign an automatic index */` |
|   2414725 |  756 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   2414725 |  757 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   2414723 |  758 | `			++pMap->iNextIdx;` |
|   1207359 |  759 | `		}` |
|         - |  760 | `	}` |
|         - |  761 | `	/* Insertion result */` |
|   4564921 |  762 | `	return rc;` |
|   3795948 |  763 | `}` |
|         - |  764 | `/*` |
|         - |  765 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - |  766 | ` * hashmap.` |
|         - |  767 | ` * This is insertion by reference so be careful to mark the node` |
|         - |  768 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - |  769 | ` * The insertion by reference is triggered when the following` |
|         - |  770 | ` * expression is encountered.` |
|         - |  771 | ` * $var = 10;` |
|         - |  772 | ` *  $a = array(&var);` |
|         - |  773 | ` * OR` |
|         - |  774 | ` *  $a[] =& $var;` |
|         - |  775 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - |  776 | ` * over it's contents.` |
|         - |  777 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - |  778 | ` * removed when the foreign ph7_value is unset.` |
|         - |  779 | ` * Example:` |
|         - |  780 | ` *  $var = 10;` |
|         - |  781 | ` *  $a[] =& $var;` |
|         - |  782 | ` *  echo count($a).PHP_EOL; //1` |
|         - |  783 | ` *  //Unset the foreign ph7_value now` |
|         - |  784 | ` *  unset($var);` |
|         - |  785 | ` *  echo count($a); //0` |
|         - |  786 | ` * Note that this is a PH7 eXtension.` |
|         - |  787 | ` * Refer to the official documentation for more information.` |
|         - |  788 | ` * If a node with the given key already exists in the database` |
|         - |  789 | ` * then this function overwrite the old value.` |
|         - |  790 | ` */` |
|     56320 |  791 | `static sxi32 HashmapInsertByRef(` |
|         - |  792 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  793 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  794 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  795 | `	)` |
|         5 |  796 | `{` |
|     56325 |  797 | `	ph7_hashmap_node *pNode = 0;` |
|     56325 |  798 | `	sxi32 rc = SXRET_OK;` |
|     56325 |  799 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|     56201 |  800 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  801 | ``			/* Force a string cast. NULL casts to "": `$a[null] =& $x` binds under the`` |
|         - |  802 | `			 * EMPTY STRING key, symmetric with HashmapInsert (the by-value path). */` |
|         3 |  803 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  804 | `		}` |
|     56201 |  805 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  806 | `			goto IntKey;` |
|         - |  807 | `		}` |
|         - |  808 | ``		/* An empty key is a REAL key: `$a[""] =& $x` binds (and OVERWRITES an existing`` |
|         - |  809 | `		 * "" element) under "", it does NOT auto-index. The legacy path turned "" into` |
|         - |  810 | ``		 * the next integer slot — `$a[""] =& $x` filed under 0 and a second write added`` |
|         - |  811 | `		 * a duplicate rather than rebinding. A genuine auto-index caller passes` |
|         - |  812 | `		 * pKey == 0 (a literal null pointer), handled at IntKey below, never a "" blob. */` |
|     84296 |  813 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     28097 |  814 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  815 | `				/* Overwrite */` |
|         8 |  816 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|         8 |  817 | `				pNode->nValIdx = nRefIdx;` |
|         - |  818 | `				/* Install in the reference table */` |
|         8 |  819 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|         8 |  820 | `				return SXRET_OK;` |
|         - |  821 | `		}` |
|         - |  822 | `		/* Perform a blob-key insertion */` |
|     56193 |  823 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     56193 |  824 | `		return rc;` |
|         - |  825 | `	}` |
|        62 |  826 | `IntKey:` |
|       129 |  827 | `	if( pKey ){` |
|        12 |  828 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  829 | `			/* Force an integer cast */` |
|         3 |  830 | `			PH7_MemObjToInteger(pKey);` |
|         1 |  831 | `		}` |
|        12 |  832 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  833 | `			/* Overwrite */` |
|       ! 0 |  834 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       ! 0 |  835 | `			pNode->nValIdx = nRefIdx;` |
|         - |  836 | `			/* Install in the reference table */` |
|       ! 0 |  837 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       ! 0 |  838 | `			return SXRET_OK;` |
|         - |  839 | `		}` |
|         - |  840 | `		/* Perform a 64-bit-int-key insertion */` |
|        12 |  841 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|        12 |  842 | `		if( rc == SXRET_OK ){` |
|        12 |  843 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|         5 |  844 | `		}` |
|         7 |  845 | `	}else{` |
|       119 |  846 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  847 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  848 | `		}` |
|         - |  849 | `		/* Assign an automatic index */` |
|       119 |  850 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|       119 |  851 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|       119 |  852 | `			++pMap->iNextIdx;` |
|        58 |  853 | `		}` |
|         - |  854 | `	}` |
|         - |  855 | `	/* Insertion result */` |
|       129 |  856 | `	return rc;` |
|     28165 |  857 | `}` |
|         - |  858 | `/*` |
|         - |  859 | ` * Is this element a php REFERENCE — that is, does the value it points at have a` |
|         - |  860 | ` * holder BESIDES this node?` |
|         - |  861 | ` *` |
|         - |  862 | ` * php's answer is a refcount: an element is a reference while at least two things` |
|         - |  863 | ` * share the value, and the marker (and the shared-through-a-copy behaviour that` |
|         - |  864 | ` * goes with it) disappears with the second-to-last holder. Every node is filed in` |
|         - |  865 | ` * its own slot's reference record, so "another holder" is simply a holder count of` |
|         - |  866 | `` * two or more — a name bound to the value (`$r = &$a[1]`), a second array node`` |
|         - |  867 | `` * (`$b = [&$x]; $c = [&$x]`), or the variable a FOREIGN node points at.`` |
|         - |  868 | ` *` |
|         - |  869 | `` * Reading the FOREIGN flag instead answered "was this element created by `&`",`` |
|         - |  870 | `` * which stops being true the moment the other side goes away: `$v = 10; $a = [&$v];`` |
|         - |  871 | `` * unset($v);` left the element marked `&int(10)` where php says `int(10)`, and — the`` |
|         - |  872 | ` * half that was not cosmetic — a COPY of that array still shared the slot, so` |
|         - |  873 | `` * `$j = $i; $j[0] = 99;` wrote through to `$i[0]`.`` |
|         - |  874 | ` */` |
|    833472 |  875 | `PH7_PRIVATE int PH7_HashmapNodeIsRef(ph7_hashmap_node *pNode)` |
|         5 |  876 | `{` |
|    833477 |  877 | `	return PH7_VmSlotHolderCount(pNode->pMap->pVm,pNode->nValIdx) >= 2;` |
|         5 |  878 | `}` |
|         - |  879 | `/*` |
|         - |  880 | ` * Extract node value.` |
|         - |  881 | ` */` |
|   1870028 |  882 | `PH7_PRIVATE ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  883 | `{` |
|         - |  884 | `	/* Point to the desired object */` |
|         - |  885 | `	ph7_value *pObj;` |
|   1870033 |  886 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1870033 |  887 | `	return pObj;` |
|         5 |  888 | `}` |
|         - |  889 | `/*` |
|         - |  890 | ` * Insert a node in the given hashmap.` |
|         - |  891 | ` * If a node with the given key already exists in the database` |
|         - |  892 | ` * then this function overwrite the old value.` |
|         - |  893 | ` */` |
|      1440 |  894 | `PH7_PRIVATE sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  895 | `{` |
|         - |  896 | `	ph7_value *pObj;` |
|         - |  897 | `	sxi32 rc;` |
|         - |  898 | `	/* Extract the node value */` |
|      1445 |  899 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|      1445 |  900 | `	if( pObj == 0 ){` |
|       ! 0 |  901 | `		return SXERR_EMPTY;` |
|         - |  902 | `	}` |
|      1445 |  903 | `	if( PH7_HashmapNodeIsRef(&(*pNode)) ){` |
|         - |  904 | `		/* A referenced element keeps its reference through the copy (php: array_slice()` |
|         - |  905 | ``		 * of an array holding `$r = &$a[1]` still var_dumps that element as &int(2)).`` |
|         - |  906 | `		 * Same rule HashmapDuplicateNode applies for array_merge()/spread. */` |
|         3 |  907 | `		sxu32 nRefIdx = pNode->nValIdx;` |
|         - |  908 | `		ph7_value sKey;` |
|         3 |  909 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         3 |  910 | `			if( !bPreserve ){` |
|         3 |  911 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - |  912 | `			}` |
|       ! 0 |  913 | `			PH7_MemObjInitFromInt(pMap->pVm,&sKey,pNode->xKey.iKey);` |
|       ! 0 |  914 | `		}else{` |
|       ! 0 |  915 | `			if( !bPreserve ){` |
|       ! 0 |  916 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - |  917 | `			}` |
|       ! 0 |  918 | `			PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       ! 0 |  919 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|       ! 0 |  920 | `				SyBlobLength(&pNode->xKey.sKey));` |
|         - |  921 | `		}` |
|       ! 0 |  922 | `		rc = HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|       ! 0 |  923 | `		PH7_MemObjRelease(&sKey);` |
|       ! 0 |  924 | `		return rc;` |
|         - |  925 | `	}` |
|         - |  926 | `	/* Preserve key */` |
|      1443 |  927 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  928 | `		/* Int64 key */` |
|      1237 |  929 | `		if( !bPreserve ){` |
|         - |  930 | `			/* Assign an automatic index */` |
|       377 |  931 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|       191 |  932 | `		}else{` |
|       864 |  933 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  934 | `		}` |
|       621 |  935 | `	}else{` |
|         - |  936 | `		/* Blob key */` |
|       210 |  937 | `		if( !bPreserve ){` |
|         - |  938 | `			/* treat it like an automatically-indexed element, drop the` |
|         - |  939 | `			 * original string key entirely */` |
|        35 |  940 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        18 |  941 | `		}else{` |
|       262 |  942 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|        86 |  943 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - |  944 | `		}` |
|         - |  945 | `	}` |
|      1443 |  946 | `	return rc;` |
|       725 |  947 | `}` |
|         - |  948 | `/*` |
|         - |  949 | ` * Compare two node values.` |
|         - |  950 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  951 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  952 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  953 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  954 | ` * documenation.` |
|         - |  955 | ` */` |
|     94761 |  956 | `PH7_PRIVATE sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  957 | `{` |
|         - |  958 | `	ph7_value sObj1,sObj2;` |
|         - |  959 | `	sxi32 rc;` |
|     94766 |  960 | `	if( pLeft == pRight ){` |
|         - |  961 | `		/*` |
|         - |  962 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  963 | `		 * below for more information on this sceanario.` |
|         - |  964 | `		 */` |
|       ! 0 |  965 | `		return 0;` |
|         - |  966 | `	}` |
|         - |  967 | `	/* Do the comparison */` |
|     94766 |  968 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     94766 |  969 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     94766 |  970 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     94766 |  971 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     94766 |  972 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     94766 |  973 | `	PH7_MemObjRelease(&sObj1);` |
|     94766 |  974 | `	PH7_MemObjRelease(&sObj2);` |
|     94766 |  975 | `	return rc;` |
|     47235 |  976 | `}` |
|         - |  977 | `/*` |
|         - |  978 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  979 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  980 | ` */` |
|     19976 |  981 | `PH7_PRIVATE void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  982 | `{` |
|     19981 |  983 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  984 | `	sxu32 nBucket;` |
|         - |  985 | `	/* Remove old collision links */` |
|     19981 |  986 | `	if( pEntry->pPrevCollide ){` |
|     14064 |  987 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      6965 |  988 | `	}else{` |
|      5922 |  989 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  990 | `	}` |
|     19981 |  991 | `	if( pEntry->pNextCollide ){` |
|      1424 |  992 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       706 |  993 | `	}` |
|     19981 |  994 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  995 | `	/* Compute the new hash */` |
|     19981 |  996 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     19981 |  997 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     19981 |  998 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  999 | `	/* Link to the new bucket */` |
|     19981 | 1000 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     19981 | 1001 | `	if( pMap->apBucket[nBucket] ){` |
|     14436 | 1002 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      7151 | 1003 | `	}` |
|     19981 | 1004 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     19981 | 1005 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - | 1006 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - | 1007 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - | 1008 | `	 * the no-overflow invariant uniform). */` |
|     19981 | 1009 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     19981 | 1010 | `		pMap->iNextIdx++;` |
|      9988 | 1011 | `	}` |
|     19981 | 1012 | `}` |
|         - | 1013 | `/*` |
|         - | 1014 | ` * Perform a linear search on a given hashmap.` |
|         - | 1015 | ` * Write a pointer to the target node on success.` |
|         - | 1016 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1017 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - | 1018 | ` * for more information.` |
|         - | 1019 | ` */` |
|     38272 | 1020 | `PH7_PRIVATE int HashmapFindValue(` |
|         - | 1021 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - | 1022 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - | 1023 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - | 1024 | `	int bStrict      /* TRUE for strict comparison */` |
|         - | 1025 | `	)` |
|         5 | 1026 | `{` |
|         - | 1027 | `	ph7_hashmap_node *pEntry;` |
|         - | 1028 | `	ph7_value sVal,*pVal;` |
|         - | 1029 | `	ph7_value sNeedle;` |
|         - | 1030 | `	sxi32 rc;` |
|         - | 1031 | `	sxu32 n;` |
|         - | 1032 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     38277 | 1033 | `	pEntry = pMap->pFirst;` |
|     38277 | 1034 | `	n = pMap->nEntry;` |
|     38277 | 1035 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     38277 | 1036 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     97853 | 1037 | `	for(;;){` |
|    195709 | 1038 | `		if( n < 1 ){` |
|        33 | 1039 | `			break;` |
|         - | 1040 | `		}` |
|         - | 1041 | `		/* Extract node value */` |
|    195679 | 1042 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    195679 | 1043 | `		if( pVal ){` |
|         - | 1044 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - | 1045 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - | 1046 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - | 1047 | `			 * so null needles/values take the same path as everything else` |
|         - | 1048 | `			 * (the historical null-to-null shortcut here made` |
|         - | 1049 | `			 * in_array(null, [""]) false where php says true). */` |
|    195679 | 1050 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    195679 | 1051 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    195679 | 1052 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    195679 | 1053 | `			PH7_MemObjRelease(&sVal);` |
|    195679 | 1054 | `			PH7_MemObjRelease(&sNeedle);` |
|    195679 | 1055 | `			if( rc == 0 ){` |
|     38247 | 1056 | `				if( ppNode ){` |
|       ! 0 | 1057 | `					*ppNode = pEntry;` |
|       ! 0 | 1058 | `				}` |
|         - | 1059 | `				/* Match found*/` |
|     38247 | 1060 | `				return SXRET_OK;` |
|         - | 1061 | `			}` |
|     78717 | 1062 | `		}` |
|         - | 1063 | `		/* Point to the next entry */` |
|    157437 | 1064 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    157437 | 1065 | `		n--;` |
|         5 | 1066 | `	}` |
|         - | 1067 | `	/* No such entry */` |
|        33 | 1068 | `	return SXERR_NOTFOUND;` |
|     19141 | 1069 | `}` |
|         - | 1070 | `/*` |
|         - | 1071 | ` * The element comparison array_diff()/array_intersect() and their _assoc pair` |
|         - | 1072 | ` * use, which is NOT the engine's value comparison: php's manual defines all four` |
|         - | 1073 | ` * as` |
|         - | 1074 | ` *     (string)$elem1 === (string)$elem2` |
|         - | 1075 | ` * a PURE string comparison — not numeric-string aware, so array_diff(["10"],` |
|         - | 1076 | ` * ["1e1"]) keeps "10" — where PHL used to call PH7_MemObjCmp with bStrict. That` |
|         - | 1077 | ` * made no int ever match its own decimal string, so array_diff([1,2,3],` |
|         - | 1078 | ` * ["1","2"]) answered the whole first array instead of [2=>3].` |
|         - | 1079 | ` *` |
|         - | 1080 | ` * bUserVisible picks the coercion: TRUE emits php's user-visible diagnostics (an` |
|         - | 1081 | ` * ARRAY element warns "Array to string conversion", an object with no` |
|         - | 1082 | ` * __toString() throws the catchable "could not be converted to string" Error,` |
|         - | 1083 | ` * reported through *pRc so the builtin answers the throw instead of a result),` |
|         - | 1084 | ` * FALSE renders silently. The two diff families need different answers there:` |
|         - | 1085 | ` * the _assoc pair converts LAZILY, only when a key matched, so it coerces` |
|         - | 1086 | ` * user-visibly right here; array_diff/array_intersect convert every element of` |
|         - | 1087 | ` * every input array up front (php sorts them), so those pre-pass with` |
|         - | 1088 | ` * HashmapStringifyElems and compare silently afterwards — which is what makes` |
|         - | 1089 | ` * the warning COUNT and the "throws even though an earlier element matched"` |
|         - | 1090 | ` * behaviour come out php-exact.` |
|         - | 1091 | ` *` |
|         - | 1092 | ` * Both operands are coerced on COPIES: these are live array elements, and a` |
|         - | 1093 | ` * diff must not rewrite the caller's array.` |
|         - | 1094 | ` */` |
|       620 | 1095 | `PH7_PRIVATE int HashmapValueStrEq(ph7_value *pA,ph7_value *pB,int bUserVisible,sxi32 *pRc)` |
|         3 | 1096 | `{` |
|         - | 1097 | `	ph7_value sA,sB;` |
|       623 | 1098 | `	int bEq = FALSE;` |
|         - | 1099 | `	sxi32 rc;` |
|       623 | 1100 | `	*pRc = SXRET_OK;` |
|         - | 1101 | `	/* Two fast paths that need no rendering at all, because each type's string` |
|         - | 1102 | `	 * form is canonical and injective: two STRINGS already ARE their string form,` |
|         - | 1103 | `	 * and two INTS are string-equal exactly when they are equal. Without them` |
|         - | 1104 | `	 * array_diff() over a pair of integer ranges formatted both operands of every` |
|         - | 1105 | `	 * one of its O(n*m) comparisons (~4x slower than the strict compare it` |
|         - | 1106 | `	 * replaced). A value carrying MEMOBJ_INT alongside MEMOBJ_REAL is an integral` |
|         - | 1107 | `	 * FLOAT, whose "1" can equal an int's — the mask sends it down the slow path` |
|         - | 1108 | `	 * rather than comparing rVal-derived iVal, and bools/null/resources likewise. */` |
|       623 | 1109 | `	if( (pA->iFlags & MEMOBJ_STRING) && (pB->iFlags & MEMOBJ_STRING) ){` |
|       278 | 1110 | `		return SyBlobLength(&pA->sBlob) == SyBlobLength(&pB->sBlob)` |
|       322 | 1111 | `		    && ( SyBlobLength(&pA->sBlob) == 0` |
|        92 | 1112 | `		      \|\| SyMemcmp(SyBlobData(&pA->sBlob),SyBlobData(&pB->sBlob),` |
|        92 | 1113 | `		                  SyBlobLength(&pA->sBlob)) == 0 );` |
|         - | 1114 | `	}` |
|       390 | 1115 | `	if( (pA->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT` |
|       369 | 1116 | `	 && (pB->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT ){` |
|       200 | 1117 | `		return pA->x.iVal == pB->x.iVal;` |
|         - | 1118 | `	}` |
|       194 | 1119 | `	PH7_MemObjInit(pA->pVm,&sA);` |
|       194 | 1120 | `	PH7_MemObjInit(pA->pVm,&sB);` |
|       194 | 1121 | `	PH7_MemObjLoad(pA,&sA);` |
|       194 | 1122 | `	PH7_MemObjLoad(pB,&sB);` |
|       194 | 1123 | `	rc = bUserVisible ? PH7_MemObjToStringUV(&sA) : PH7_MemObjToString(&sA);` |
|       194 | 1124 | `	if( rc == SXRET_OK ){` |
|       194 | 1125 | `		rc = bUserVisible ? PH7_MemObjToStringUV(&sB) : PH7_MemObjToString(&sB);` |
|        96 | 1126 | `	}` |
|       194 | 1127 | `	if( rc != SXRET_OK ){` |
|         3 | 1128 | `		*pRc = rc;` |
|       193 | 1129 | `	}else if( SyBlobLength(&sA.sBlob) == SyBlobLength(&sB.sBlob) ){` |
|       254 | 1130 | `		bEq = SyBlobLength(&sA.sBlob) == 0` |
|       170 | 1131 | `		   \|\| SyMemcmp(SyBlobData(&sA.sBlob),SyBlobData(&sB.sBlob),SyBlobLength(&sA.sBlob)) == 0;` |
|        85 | 1132 | `	}` |
|       194 | 1133 | `	PH7_MemObjRelease(&sA);` |
|       194 | 1134 | `	PH7_MemObjRelease(&sB);` |
|       194 | 1135 | `	return bEq;` |
|       313 | 1136 | `}` |
|         - | 1137 | `/*` |
|         - | 1138 | ` * Run the USER-VISIBLE string coercion over every element of pMap once, in` |
|         - | 1139 | ` * insertion order, discarding the result: php's array_diff/array_intersect sort` |
|         - | 1140 | ` * each input array, which converts every element exactly once, so this is where` |
|         - | 1141 | ` * their "Array to string conversion" warnings and their not-stringable-object` |
|         - | 1142 | ` * Error come from. Doing it as a pre-pass is what lets` |
|         - | 1143 | ` * array_diff([1,2],[1,new P()]) throw the way php's does even though the first` |
|         - | 1144 | ` * element already matched. Returns the throw status, SXRET_OK otherwise.` |
|         - | 1145 | ` */` |
|       186 | 1146 | `PH7_PRIVATE sxi32 HashmapStringifyElems(ph7_hashmap *pMap)` |
|         2 | 1147 | `{` |
|       188 | 1148 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       188 | 1149 | `	sxu32 n = pMap->nEntry;` |
|       588 | 1150 | `	while( n > 0 && pEntry ){` |
|       410 | 1151 | `		ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|       410 | 1152 | `		if( pVal && (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1153 | `			ph7_value sTmp;` |
|         - | 1154 | `			sxi32 rc;` |
|       246 | 1155 | `			PH7_MemObjInit(pMap->pVm,&sTmp);` |
|       246 | 1156 | `			PH7_MemObjLoad(pVal,&sTmp);` |
|       246 | 1157 | `			rc = PH7_MemObjToStringUV(&sTmp);` |
|       246 | 1158 | `			PH7_MemObjRelease(&sTmp);` |
|       246 | 1159 | `			if( rc != SXRET_OK ){` |
|         9 | 1160 | `				return rc;` |
|         - | 1161 | `			}` |
|       118 | 1162 | `		}` |
|       402 | 1163 | `		pEntry = pEntry->pPrev; /* Reverse link — insertion order */` |
|       402 | 1164 | `		n--;` |
|         2 | 1165 | `	}` |
|       180 | 1166 | `	return SXRET_OK;` |
|        95 | 1167 | `}` |
|         - | 1168 | `/*` |
|         - | 1169 | ` * Perform a linear search on a given hashmap, comparing values the way` |
|         - | 1170 | ` * array_diff()/array_intersect() do (see HashmapValueStrEq). Writes a pointer to` |
|         - | 1171 | ` * the target node on success; SXERR_NOTFOUND otherwise, with *pRc carrying the` |
|         - | 1172 | ` * status of a coercion that threw.` |
|         - | 1173 | ` */` |
|       228 | 1174 | `PH7_PRIVATE int HashmapFindStringValue(` |
|         - | 1175 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - | 1176 | `	ph7_value *pNeedle,  /* Lookup value */` |
|         - | 1177 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success */` |
|         - | 1178 | `	sxi32 *pRc           /* OUT: coercion status */` |
|         - | 1179 | `	)` |
|         2 | 1180 | `{` |
|       230 | 1181 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       230 | 1182 | `	sxu32 n = pMap->nEntry;` |
|       230 | 1183 | `	*pRc = SXRET_OK;` |
|       602 | 1184 | `	while( n > 0 && pEntry ){` |
|       502 | 1185 | `		ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|       502 | 1186 | `		if( pVal ){` |
|       502 | 1187 | `			if( HashmapValueStrEq(pNeedle,pVal,/*bUserVisible*/0,pRc) ){` |
|       130 | 1188 | `				if( ppNode ){` |
|       ! 0 | 1189 | `					*ppNode = pEntry;` |
|       ! 0 | 1190 | `				}` |
|       130 | 1191 | `				return SXRET_OK;` |
|         - | 1192 | `			}` |
|       374 | 1193 | `			if( *pRc != SXRET_OK ){` |
|       ! 0 | 1194 | `				return SXERR_NOTFOUND;` |
|         - | 1195 | `			}` |
|       186 | 1196 | `		}` |
|       374 | 1197 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       374 | 1198 | `		n--;` |
|         2 | 1199 | `	}` |
|       102 | 1200 | `	return SXERR_NOTFOUND;` |
|       116 | 1201 | `}` |
|         - | 1202 | `/*` |
|         - | 1203 | ` * Compare two hashmaps.` |
|         - | 1204 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1205 | ` * Note on array comparison operators.` |
|         - | 1206 | ` *  According to the PHP language reference manual.` |
|         - | 1207 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1208 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1209 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1210 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1211 | ` *                          order and of the same types.` |
|         - | 1212 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1213 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1214 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1215 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1216 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1217 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1218 | ` * <?php` |
|         - | 1219 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1220 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1221 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1222 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1223 | ` * var_dump($c);` |
|         - | 1224 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1225 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1226 | ` * var_dump($c);` |
|         - | 1227 | ` * ?>` |
|         - | 1228 | ` * When executed, this script will print the following:` |
|         - | 1229 | ` * Union of $a and $b:` |
|         - | 1230 | ` * array(3) {` |
|         - | 1231 | ` *  ["a"]=>` |
|         - | 1232 | ` *  string(5) "apple"` |
|         - | 1233 | ` *  ["b"]=>` |
|         - | 1234 | ` * string(6) "banana"` |
|         - | 1235 | ` *  ["c"]=>` |
|         - | 1236 | ` * string(6) "cherry"` |
|         - | 1237 | ` * }` |
|         - | 1238 | ` * Union of $b and $a:` |
|         - | 1239 | ` * array(3) {` |
|         - | 1240 | ` * ["a"]=>` |
|         - | 1241 | ` * string(4) "pear"` |
|         - | 1242 | ` * ["b"]=>` |
|         - | 1243 | ` * string(10) "strawberry"` |
|         - | 1244 | ` * ["c"]=>` |
|         - | 1245 | ` * string(6) "cherry"` |
|         - | 1246 | ` * }` |
|         - | 1247 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1248 | ` */` |
|        86 | 1249 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1250 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1251 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1252 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1253 | `	)` |
|         3 | 1254 | `{` |
|         - | 1255 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1256 | `	sxi32 rc;` |
|         - | 1257 | `	sxu32 n;` |
|        89 | 1258 | `	if( pLeft == pRight ){` |
|         - | 1259 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1260 | `		 * Unlike the zend engine.` |
|         - | 1261 | `		 */` |
|         7 | 1262 | `		return 0;` |
|         - | 1263 | `	}` |
|        83 | 1264 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1265 | `		/* Must have the same number of entries */` |
|         8 | 1266 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1267 | `	}` |
|        77 | 1268 | `	if( bStrict ){` |
|         - | 1269 | `		/* PHP's '===' on arrays is ORDER-SENSITIVE: the two maps must hold the` |
|         - | 1270 | `		 * same key/value pairs, with identical key types, in the same insertion` |
|         - | 1271 | `		 * order. Walk both in insertion order (pFirst, then the pPrev chain, per` |
|         - | 1272 | `		 * this file's forward-iteration convention) in lockstep and compare each` |
|         - | 1273 | `		 * position's key then value. (Loose '==' below stays order-insensitive,` |
|         - | 1274 | `		 * matching each left key by lookup into the right map.) */` |
|        56 | 1275 | `		ph7_hashmap_node *pLs = pLeft->pFirst;` |
|        56 | 1276 | `		ph7_hashmap_node *pRs = pRight->pFirst;` |
|      1154 | 1277 | `		for( n = pLeft->nEntry ; n > 0 ; n-- ){` |
|         - | 1278 | `			/* Keys must match in type and value at this position */` |
|      1114 | 1279 | `			if( pLs->iType != pRs->iType ){` |
|       ! 0 | 1280 | `				return 1;` |
|         - | 1281 | `			}` |
|      1114 | 1282 | `			if( pLs->iType == HASHMAP_INT_NODE ){` |
|      1076 | 1283 | `				if( pLs->xKey.iKey != pRs->xKey.iKey ){` |
|         3 | 1284 | `					return 1;` |
|         - | 1285 | `				}` |
|       538 | 1286 | `			}else{` |
|        39 | 1287 | `				SyBlob *pLk = &pLs->xKey.sKey;` |
|        39 | 1288 | `				SyBlob *pRk = &pRs->xKey.sKey;` |
|        38 | 1289 | `				if( SyBlobLength(pLk) != SyBlobLength(pRk)` |
|        39 | 1290 | `				 \|\| (SyBlobLength(pLk) > 0` |
|        38 | 1291 | `				  && SyMemcmp(SyBlobData(pLk),SyBlobData(pRk),SyBlobLength(pLk)) != 0) ){` |
|         7 | 1292 | `					return 1;` |
|         - | 1293 | `				}` |
|         - | 1294 | `			}` |
|         - | 1295 | `			/* Values must be strictly identical */` |
|      1106 | 1296 | `			if( HashmapNodeCmp(pLs,pRs,TRUE) != 0 ){` |
|         7 | 1297 | `				return 1;` |
|         - | 1298 | `			}` |
|      1100 | 1299 | `			pLs = pLs->pPrev; /* Reverse link = insertion order */` |
|      1100 | 1300 | `			pRs = pRs->pPrev;` |
|       551 | 1301 | `		}` |
|        42 | 1302 | `		return 0; /* Same pairs, same order */` |
|         - | 1303 | `	}` |
|         - | 1304 | `	/* Point to the first inserted entry of the left hashmap */` |
|        23 | 1305 | `	pLe = pLeft->pFirst;` |
|        23 | 1306 | `	pRe = 0; /* cc warning */` |
|         - | 1307 | `	/* Perform the comparison */` |
|        23 | 1308 | `	n = pLeft->nEntry;` |
|        23 | 1309 | `	for(;;){` |
|        49 | 1310 | `		if( n < 1 ){` |
|        18 | 1311 | `			break;` |
|         - | 1312 | `		}` |
|        33 | 1313 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1314 | `			/* Int key */` |
|        25 | 1315 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|        14 | 1316 | `		}else{` |
|         9 | 1317 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1318 | `			/* Blob key */` |
|         9 | 1319 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1320 | `		}` |
|        33 | 1321 | `		if( rc != SXRET_OK ){` |
|         - | 1322 | `			/* No such entry in the right side */` |
|       ! 0 | 1323 | `			return 1;` |
|         - | 1324 | `		}` |
|        33 | 1325 | `		rc = 0;` |
|        33 | 1326 | `		if( bStrict ){` |
|         - | 1327 | `			/* Make sure,the keys are of the same type */` |
|       ! 0 | 1328 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1329 | `				rc = 1;` |
|       ! 0 | 1330 | `			}` |
|       ! 0 | 1331 | `		}` |
|        33 | 1332 | `		if( !rc ){` |
|         - | 1333 | `			/* Compare nodes */` |
|        33 | 1334 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|        15 | 1335 | `		}` |
|        33 | 1336 | `		if( rc != 0 ){` |
|         - | 1337 | `			/* Nodes key/value differ */` |
|         6 | 1338 | `			return rc;` |
|         - | 1339 | `		}` |
|         - | 1340 | `		/* Point to the next entry */` |
|        28 | 1341 | `		pLe = pLe->pPrev; /* Reverse link */` |
|        28 | 1342 | `		n--;` |
|         2 | 1343 | `	}` |
|        18 | 1344 | `	return 0; /* Hashmaps are equals */` |
|        46 | 1345 | `}` |
|         - | 1346 | `/*` |
|         - | 1347 | ` * Duplicate a hashmap node.` |
|         - | 1348 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1349 | ` */` |
|    830220 | 1350 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1351 | `	ph7_hashmap *pDest,` |
|         - | 1352 | `	ph7_hashmap_node *pEntry,` |
|         - | 1353 | `	ph7_value *pVal,` |
|         - | 1354 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1355 | `	)` |
|         5 | 1356 | `{` |
|         - | 1357 | `	ph7_value sSafeVal;` |
|         - | 1358 | `	ph7_value sKey;` |
|         - | 1359 | `	sxi32 rc;` |
|         - | 1360 |  |
|    830225 | 1361 | `	if( PH7_HashmapNodeIsRef(&(*pEntry)) ){` |
|         - | 1362 | ``		/* The source node is a reference — either a FOREIGN one (`[&$x]`, the node points`` |
|         - | 1363 | `		 * at an outside slot) or, the case PH7 missed, an element somebody took a` |
|         - | 1364 | ``		 * reference TO (`$r = &$a[1]`). php carries an element's reference bit through`` |
|         - | 1365 | `		 * array COPIES, so array_merge()/array_slice()/array_replace()/spread all keep` |
|         - | 1366 | ``		 * var_dump'ing it as `&int(2)`; flattening it to a value copy lost that. */`` |
|        24 | 1367 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|        24 | 1368 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         9 | 1369 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|         9 | 1370 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|         9 | 1371 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|         9 | 1372 | `			PH7_MemObjRelease(&sKey);` |
|         5 | 1373 | `		}else{` |
|        16 | 1374 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         7 | 1375 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|        13 | 1376 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1377 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1378 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1379 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1380 | `			}else{ /* Dup: preserve the int key */` |
|        10 | 1381 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1382 | `			}` |
|         - | 1383 | `		}` |
|        24 | 1384 | `		return rc;` |
|         - | 1385 | `	}` |
|    830203 | 1386 | `	sSafeVal = *pVal;` |
|         - | 1387 |  |
|    830203 | 1388 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1389 | `		/* Blob key insertion */` |
|      5527 | 1390 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      5527 | 1391 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      5527 | 1392 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      5527 | 1393 | `		PH7_MemObjRelease(&sKey);` |
|      2766 | 1394 | `	}else{` |
|         - | 1395 | `		/* Int key */` |
|    824681 | 1396 | `		if( iAction == 0 ){ /* Merge */` |
|    819379 | 1397 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    414994 | 1398 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1399 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1400 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1401 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1402 | `		}else{ /* Dup */` |
|      5277 | 1403 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1404 | `		}` |
|         - | 1405 | `	}` |
|    830203 | 1406 | `	return rc;` |
|    415115 | 1407 | `}` |
|         - | 1408 | `/*` |
|         - | 1409 | ` * Merge two hashmaps.` |
|         - | 1410 | ` * Note on the merge process` |
|         - | 1411 | ` * According to the PHP language reference manual.` |
|         - | 1412 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1413 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1414 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1415 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1416 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1417 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1418 | ` *  keys starting from zero in the result array.` |
|         - | 1419 | ` */` |
|      3154 | 1420 | `PH7_PRIVATE sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1421 | `{` |
|         - | 1422 | `	ph7_hashmap_node *pEntry;` |
|         - | 1423 | `	ph7_value *pVal;` |
|         - | 1424 | `	sxi32 rc;` |
|         - | 1425 | `	sxu32 n;` |
|      3159 | 1426 | `	if( pSrc == pDest ){` |
|         - | 1427 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1428 | `		 * Unlike the zend engine.` |
|         - | 1429 | `		 */` |
|       ! 0 | 1430 | `		return SXRET_OK;` |
|         - | 1431 | `	}` |
|         - | 1432 | `	/* Point to the first inserted entry in the source */` |
|      3159 | 1433 | `	pEntry = pSrc->pFirst;` |
|         - | 1434 | `	/* Perform the merge */` |
|    822619 | 1435 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1436 | `		/* Extract the node value */` |
|    819465 | 1437 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    819465 | 1438 | `		if( pVal ){` |
|         - | 1439 | `			/* Make a local copy of the value.` |
|         - | 1440 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1441 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1442 | `			 * to the old pool.` |
|         - | 1443 | `			 */` |
|    819465 | 1444 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    409735 | 1445 | `		}else{` |
|       ! 0 | 1446 | `			rc = SXRET_OK;` |
|         - | 1447 | `		}` |
|    819465 | 1448 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1449 | `			return rc;` |
|         - | 1450 | `		}` |
|         - | 1451 | `		/* Point to the next entry */` |
|    819465 | 1452 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    409735 | 1453 | `	}` |
|      3159 | 1454 | `	return SXRET_OK;` |
|      1582 | 1455 | `}` |
|         - | 1456 | `/*` |
|         - | 1457 | ` * Overwrite entries with the same key.` |
|         - | 1458 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1459 | ` *  According to the PHP language reference manual.` |
|         - | 1460 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1461 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1462 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1463 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1464 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1465 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1466 | ` *  overwriting the previous values.` |
|         - | 1467 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1468 | ` *  by whatever type is in the second array.` |
|         - | 1469 | ` */` |
|        34 | 1470 | `PH7_PRIVATE sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1471 | `{` |
|         - | 1472 | `	ph7_hashmap_node *pEntry;` |
|         - | 1473 | `	ph7_value *pVal;` |
|         - | 1474 | `	sxi32 rc;` |
|         - | 1475 | `	sxu32 n;` |
|        36 | 1476 | `	if( pSrc == pDest ){` |
|         - | 1477 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1478 | `		 * Unlike the zend engine.` |
|         - | 1479 | `		 */` |
|       ! 0 | 1480 | `		return SXRET_OK;` |
|         - | 1481 | `	}` |
|         - | 1482 | `	/* Point to the first inserted entry in the source */` |
|        36 | 1483 | `	pEntry = pSrc->pFirst;` |
|         - | 1484 | `	/* Perform the merge */` |
|        80 | 1485 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1486 | `		/* Extract the node value */` |
|        46 | 1487 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        46 | 1488 | `		if( pVal ){` |
|        46 | 1489 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        24 | 1490 | `		}else{` |
|       ! 0 | 1491 | `			rc = SXRET_OK;` |
|         - | 1492 | `		}` |
|        46 | 1493 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1494 | `			return rc;` |
|         - | 1495 | `		}` |
|         - | 1496 | `		/* Point to the next entry */` |
|        46 | 1497 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1498 | `	}` |
|        36 | 1499 | `	return SXRET_OK;` |
|        19 | 1500 | `}` |
|         - | 1501 | `/*` |
|         - | 1502 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1503 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1504 | ` */` |
|      9582 | 1505 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1506 | `{` |
|         - | 1507 | `	ph7_hashmap_node *pEntry;` |
|         - | 1508 | `	ph7_value *pVal;` |
|         - | 1509 | `	sxi32 rc;` |
|         - | 1510 | `	sxu32 n;` |
|      9587 | 1511 | `	if( pSrc == pDest ){` |
|         - | 1512 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1513 | `		 * Unlike the zend engine.` |
|         - | 1514 | `		 */` |
|       ! 0 | 1515 | `		return SXRET_OK;` |
|         - | 1516 | `	}` |
|         - | 1517 | `	/* Point to the first inserted entry in the source */` |
|      9587 | 1518 | `	pEntry = pSrc->pFirst;` |
|         - | 1519 | `	/* Perform the duplication */` |
|     20303 | 1520 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1521 | `		/* Extract the node value */` |
|     10721 | 1522 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     10721 | 1523 | `		if( pVal ){` |
|     10721 | 1524 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      5363 | 1525 | `		}else{` |
|       ! 0 | 1526 | `			rc = SXRET_OK;` |
|         - | 1527 | `		}` |
|     10721 | 1528 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1529 | `			return rc;` |
|         - | 1530 | `		}` |
|         - | 1531 | `		/* Point to the next entry */` |
|     10721 | 1532 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      5363 | 1533 | `	}` |
|      9587 | 1534 | `	return SXRET_OK;` |
|      4796 | 1535 | `}` |
|         - | 1536 | `/*` |
|         - | 1537 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1538 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1539 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1540 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1541 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1542 | ` * this instead of PH7_HashmapDup.` |
|         - | 1543 | ` */` |
|        12 | 1544 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1545 | `{` |
|         - | 1546 | `	ph7_hashmap_node *pEntry;` |
|         - | 1547 | `	ph7_value *pVal;` |
|         - | 1548 | `	sxi32 rc;` |
|         - | 1549 | `	sxu32 n;` |
|        13 | 1550 | `	if( pSrc == pDest ){` |
|       ! 0 | 1551 | `		return SXRET_OK;` |
|         - | 1552 | `	}` |
|        13 | 1553 | `	pEntry = pSrc->pFirst;` |
|       915 | 1554 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1555 | `		/* Extract the node value (resolves foreign references) */` |
|       903 | 1556 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       902 | 1557 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       585 | 1558 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1559 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1560 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1561 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1562 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1563 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1564 | `			pVal = 0;` |
|         2 | 1565 | `		}` |
|       903 | 1566 | `		if( pVal ){` |
|       899 | 1567 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1342 | 1568 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       447 | 1569 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       448 | 1570 | `			}else{` |
|         5 | 1571 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1572 | `			}` |
|       899 | 1573 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1574 | `				return rc;` |
|         - | 1575 | `			}` |
|       449 | 1576 | `		}` |
|         - | 1577 | `		/* Point to the next entry */` |
|       903 | 1578 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       452 | 1579 | `	}` |
|        13 | 1580 | `	return SXRET_OK;` |
|         7 | 1581 | `}` |
|         - | 1582 | `/*` |
|         - | 1583 | ` * Count the map references held by BY-REFERENCE foreach steps iterating the` |
|         - | 1584 | `` * given hashmap. php's `foreach ($a as &$v)` iterates the LIVE array —`` |
|         - | 1585 | ` * appends/deletes inside the body are visited — so a by-ref step's retain` |
|         - | 1586 | ` * must not make writes through the source variable COW-separate away from` |
|         - | 1587 | ` * the loop's map. By-VALUE steps are deliberately NOT discounted: their` |
|         - | 1588 | ` * retain is exactly what makes an in-loop write separate, which is php's` |
|         - | 1589 | ` * iterate-a-snapshot semantic.` |
|         - | 1590 | ` */` |
|        46 | 1591 | `static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)` |
|         3 | 1592 | `{` |
|         - | 1593 | `	ph7_foreach_step *pStep;` |
|        49 | 1594 | `	sxi32 nRef = 0;` |
|        95 | 1595 | `	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        49 | 1596 | `		if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        45 | 1597 | `			nRef++;` |
|        21 | 1598 | `		}` |
|        26 | 1599 | `	}` |
|        49 | 1600 | `	return nRef;` |
|         3 | 1601 | `}` |
|         - | 1602 | `/*` |
|         - | 1603 | ` * Copy-on-write separation for arrays.` |
|         - | 1604 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1605 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1606 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1607 | ` * References held by active by-ref foreach steps do not count as sharers` |
|         - | 1608 | `` * (see HashmapByRefStepRefs): writes during `foreach ($a as &$v)` must land`` |
|         - | 1609 | ` * on the live map the loop is walking, like php.` |
|         - | 1610 | ` */` |
|    341970 | 1611 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1612 | `{` |
|    341975 | 1613 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1614 | `	ph7_hashmap *pNew;` |
|         - | 1615 | `	ph7_value *pBacking;` |
|         - | 1616 | `	sxu32 nValIdx;` |
|         - | 1617 | `	int bValueInPool;` |
|    341975 | 1618 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    341975 | 1619 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1620 | `		/* Sole owner, no separation needed */` |
|    337689 | 1621 | `		return pMap;` |
|         - | 1622 | `	}` |
|      4291 | 1623 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1624 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1625 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1626 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       172 | 1627 | `		return pMap;` |
|         - | 1628 | `	}` |
|         - | 1629 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1630 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1631 | `	 * frame is popped. */` |
|      4121 | 1632 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      4095 | 1633 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      4090 | 1634 | `		if( pBacking && pBacking != pValue` |
|      3765 | 1635 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      3445 | 1636 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1637 | `			/* Undo the stack ref to reveal true sharing count */` |
|      3403 | 1638 | `			pMap->iRef--;` |
|      3403 | 1639 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1640 | `				/* After undoing stack ref, sole owner — no separation */` |
|      3207 | 1641 | `				pMap->iRef++;` |
|      3207 | 1642 | `				return pMap;` |
|         - | 1643 | `			}` |
|       199 | 1644 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|       199 | 1645 | `			if( pNew == 0 ){` |
|       ! 0 | 1646 | `				pMap->iRef++;` |
|       ! 0 | 1647 | `				return pMap;` |
|         - | 1648 | `			}` |
|       199 | 1649 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1650 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1651 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1652 | `				pMap->iRef++;` |
|       ! 0 | 1653 | `				return pMap;` |
|         - | 1654 | `			}` |
|       199 | 1655 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|       199 | 1656 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1657 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1658 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1659 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1660 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1661 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1662 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1663 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|       199 | 1664 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|       199 | 1665 | `			if( pBacking ){` |
|       199 | 1666 | `				pBacking->x.pOther = pNew;` |
|        98 | 1667 | `			}` |
|         - | 1668 | `			/* Update the stack value to match */` |
|       199 | 1669 | `			pValue->x.pOther = pNew;` |
|       199 | 1670 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|       199 | 1671 | `			return pNew;` |
|         - | 1672 | `		}` |
|       346 | 1673 | `	}` |
|         - | 1674 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1675 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1676 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1677 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1678 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1679 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1680 | `	 * pBacking in the backing-variable branch above). */` |
|       723 | 1681 | `	nValIdx = pValue->nIdx;` |
|      1069 | 1682 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|       718 | 1683 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|       723 | 1684 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|       723 | 1685 | `	if( pNew == 0 ){` |
|         - | 1686 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1687 | `		return pMap;` |
|         - | 1688 | `	}` |
|       723 | 1689 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1690 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1691 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1692 | `		return pMap;` |
|         - | 1693 | `	}` |
|       723 | 1694 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|       723 | 1695 | `	pMap->iRef--;` |
|       723 | 1696 | `	if( bValueInPool ){` |
|         - | 1697 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|       654 | 1698 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|       654 | 1699 | `		if( pValue == 0 ){` |
|       ! 0 | 1700 | `			return pNew;` |
|         - | 1701 | `		}` |
|       325 | 1702 | `	}` |
|       723 | 1703 | `	pValue->x.pOther = pNew;` |
|       723 | 1704 | `	return pNew;` |
|    170990 | 1705 | `}` |
|         - | 1706 | `/*` |
|         - | 1707 | ` * Perform the union of two hashmaps.` |
|         - | 1708 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1709 | ` * with a variable holding an array as follows:` |
|         - | 1710 | ` * <?php` |
|         - | 1711 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1712 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1713 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1714 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1715 | ` * var_dump($c);` |
|         - | 1716 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1717 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1718 | ` * var_dump($c);` |
|         - | 1719 | ` * ?>` |
|         - | 1720 | ` * When executed, this script will print the following:` |
|         - | 1721 | ` * Union of $a and $b:` |
|         - | 1722 | ` * array(3) {` |
|         - | 1723 | ` *  ["a"]=>` |
|         - | 1724 | ` *  string(5) "apple"` |
|         - | 1725 | ` *  ["b"]=>` |
|         - | 1726 | ` * string(6) "banana"` |
|         - | 1727 | ` *  ["c"]=>` |
|         - | 1728 | ` * string(6) "cherry"` |
|         - | 1729 | ` * }` |
|         - | 1730 | ` * Union of $b and $a:` |
|         - | 1731 | ` * array(3) {` |
|         - | 1732 | ` * ["a"]=>` |
|         - | 1733 | ` * string(4) "pear"` |
|         - | 1734 | ` * ["b"]=>` |
|         - | 1735 | ` * string(10) "strawberry"` |
|         - | 1736 | ` * ["c"]=>` |
|         - | 1737 | ` * string(6) "cherry"` |
|         - | 1738 | ` * }` |
|         - | 1739 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1740 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1741 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1742 | ` */` |
|      4610 | 1743 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1744 | `{` |
|         - | 1745 | `	ph7_hashmap_node *pEntry;` |
|      4615 | 1746 | `	sxi32 rc = SXRET_OK;` |
|         - | 1747 | `	ph7_value *pObj;` |
|         - | 1748 | `	sxu32 n;` |
|      4615 | 1749 | `	if( pLeft == pRight ){` |
|         - | 1750 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1751 | `		 * Unlike the zend engine.` |
|         - | 1752 | `		 */` |
|       ! 0 | 1753 | `		return SXRET_OK;` |
|         - | 1754 | `	}` |
|         - | 1755 | `	/* Perform the union */` |
|      4615 | 1756 | `	pEntry = pRight->pFirst;` |
|      4661 | 1757 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1758 | `		/* Make sure the given key does not exists in the left array */` |
|        50 | 1759 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1760 | `			/* BLOB key */` |
|        24 | 1761 | `			if( SXRET_OK !=` |
|        20 | 1762 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        20 | 1763 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        20 | 1764 | `					if( pObj ){` |
|        20 | 1765 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1766 | `						/* Perform the insertion */` |
|        20 | 1767 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1768 | `							&sSafeVal,0,FALSE);` |
|        20 | 1769 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1770 | `							return rc;` |
|         - | 1771 | `						}` |
|         8 | 1772 | `					}` |
|         8 | 1773 | `			}` |
|        14 | 1774 | `		}else{` |
|         - | 1775 | `			/* INT key */` |
|        28 | 1776 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        15 | 1777 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        15 | 1778 | `				if( pObj ){` |
|        15 | 1779 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1780 | `					/* Perform the insertion */` |
|        15 | 1781 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        15 | 1782 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1783 | `						return rc;` |
|         - | 1784 | `					}` |
|         7 | 1785 | `				}` |
|         7 | 1786 | `			}` |
|         - | 1787 | `		}` |
|         - | 1788 | `		/* Point to the next entry */` |
|        50 | 1789 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        27 | 1790 | `	}` |
|      4615 | 1791 | `	return SXRET_OK;` |
|      2310 | 1792 | `}` |
|         - | 1793 | `/*` |
|         - | 1794 | ` * Allocate a new hashmap.` |
|         - | 1795 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1796 | ` */` |
|   3708070 | 1797 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1798 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1799 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1800 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1801 | `	)` |
|         5 | 1802 | `{` |
|         - | 1803 | `	ph7_hashmap *pMap;` |
|         - | 1804 | `	/* Allocate a new instance */` |
|   3708075 | 1805 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   3708075 | 1806 | `	if( pMap == 0 ){` |
|       ! 0 | 1807 | `		return 0;` |
|         - | 1808 | `	}` |
|         - | 1809 | `	/* Zero the structure */` |
|   3708075 | 1810 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1811 | `	/* Fill in the structure */` |
|   3708075 | 1812 | `	pMap->pVm = &(*pVm);` |
|   3708075 | 1813 | `	pMap->iRef = 1;` |
|         - | 1814 | `	/* Default hash functions */` |
|   3708075 | 1815 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   3708075 | 1816 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   3708075 | 1817 | `	return pMap;` |
|   1854040 | 1818 | `}` |
|         - | 1819 | `/*` |
|         - | 1820 | ` * Install superglobals in the given virtual machine.` |
|         - | 1821 | ` * Note on superglobals.` |
|         - | 1822 | ` *  According to the PHP language reference manual.` |
|         - | 1823 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1824 | `*   Description` |
|         - | 1825 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1826 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1827 | `*   global $variable; to access them within functions or methods.` |
|         - | 1828 | `*   These superglobal variables are:` |
|         - | 1829 | `*    $GLOBALS` |
|         - | 1830 | `*    $_SERVER` |
|         - | 1831 | `*    $_GET` |
|         - | 1832 | `*    $_POST` |
|         - | 1833 | `*    $_FILES` |
|         - | 1834 | `*    $_COOKIE` |
|         - | 1835 | `*    $_SESSION` |
|         - | 1836 | `*    $_REQUEST` |
|         - | 1837 | `*    $_ENV` |
|         - | 1838 | `*/` |
|      4084 | 1839 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1840 | `{` |
|         - | 1841 | `	static const char * azSuper[] = {` |
|         - | 1842 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1843 | `		"_GET",      /* $_GET */` |
|         - | 1844 | `		"_POST",     /* $_POST */` |
|         - | 1845 | `		"_FILES",    /* $_FILES */` |
|         - | 1846 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1847 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1848 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1849 | `		"_ENV",      /* $_ENV */` |
|         - | 1850 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1851 | `		"argv"       /* $argv */` |
|         - | 1852 | `	};` |
|         - | 1853 | `	ph7_hashmap *pMap;` |
|         - | 1854 | `	ph7_value *pObj;` |
|         - | 1855 | `	SyString *pFile;` |
|         - | 1856 | `	sxi32 rc;` |
|         - | 1857 | `	sxu32 n;` |
|         - | 1858 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      4089 | 1859 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      4089 | 1860 | `	if( pMap == 0 ){` |
|       ! 0 | 1861 | `		return SXERR_MEM;` |
|         - | 1862 | `	}` |
|      4089 | 1863 | `	pVm->pGlobal = pMap;` |
|         - | 1864 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      4089 | 1865 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      4089 | 1866 | `	if( pObj == 0 ){` |
|       ! 0 | 1867 | `		return SXERR_MEM;` |
|         - | 1868 | `	}` |
|      4089 | 1869 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1870 | `	/* Record object index */` |
|      4089 | 1871 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1872 | `	/* Install the special $GLOBALS array */` |
|      4089 | 1873 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      4089 | 1874 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1875 | `		return rc;` |
|         - | 1876 | `	}` |
|         - | 1877 | `	/* Install superglobals now */` |
|     44929 | 1878 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1879 | `		ph7_value *pSuper;` |
|         - | 1880 | `		/* Request an empty array */` |
|     40845 | 1881 | `		pSuper = ph7_new_array(&(*pVm));` |
|     40845 | 1882 | `		if( pSuper == 0 ){` |
|       ! 0 | 1883 | `			return SXERR_MEM;` |
|         - | 1884 | `		}` |
|         - | 1885 | `		/* Install */` |
|     40845 | 1886 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     40845 | 1887 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1888 | `			return rc;` |
|         - | 1889 | `		}` |
|         - | 1890 | `		/* Release the value now it have been installed */` |
|     40845 | 1891 | `		ph7_release_value(&(*pVm),pSuper);` |
|     20425 | 1892 | `	}` |
|         - | 1893 | `	/* Set some $_SERVER entries */` |
|      4089 | 1894 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1895 | `	/*` |
|         - | 1896 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1897 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1898 | `	 */` |
|      8173 | 1899 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1900 | `		"SCRIPT_FILENAME",` |
|      2042 | 1901 | `		pFile ? pFile->zString : ":Memory:",` |
|      4084 | 1902 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1903 | `		);` |
|         - | 1904 | `	/* All done,all super-global are installed now */` |
|      4089 | 1905 | `	return SXRET_OK;` |
|      2047 | 1906 | `}` |
|         - | 1907 | `/*` |
|         - | 1908 | ` * Release a hashmap.` |
|         - | 1909 | ` */` |
|   3552372 | 1910 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1911 | `{` |
|         - | 1912 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   3552377 | 1913 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1914 | `	sxu32 n;` |
|   3552377 | 1915 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1916 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1917 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1918 | `		return SXRET_OK;` |
|         - | 1919 | `	}` |
|   3552377 | 1920 | `	if( pMap->pActiveSteps ){` |
|         - | 1921 | `		/* Every node is about to be freed WITHOUT going through` |
|         - | 1922 | `		 * PH7_HashmapUnlinkNode, so its cursor fixup never runs. Park any` |
|         - | 1923 | `		 * live foreach cursor on this map (reachable: array_erase() on the` |
|         - | 1924 | `		 * live map of a by-ref foreach — the CowSeparate discount keeps the` |
|         - | 1925 | `		 * loop's map writable). A NULL cursor ends the loop cleanly at the` |
|         - | 1926 | `		 * next step, or resumes on a fresh insert via the link-time re-arm. */` |
|         - | 1927 | `		ph7_foreach_step *pStep;` |
|        17 | 1928 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|         9 | 1929 | `			pStep->pCursor = 0;` |
|         5 | 1930 | `		}` |
|         4 | 1931 | `	}` |
|         - | 1932 | `	/* Start the release process */` |
|   3552377 | 1933 | `	n = 0;` |
|   3552377 | 1934 | `	pEntry = pMap->pFirst;` |
|   5537524 | 1935 | `	for(;;){` |
|  11075053 | 1936 | `		if( n >= pMap->nEntry ){` |
|   3552377 | 1937 | `			break;` |
|         - | 1938 | `		}` |
|   7522681 | 1939 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1940 | `		/* Remove the reference from the foreign table */` |
|   7522681 | 1941 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|         - | 1942 | `		/* Restore the ph7_value to the free list if this node was its last holder` |
|         - | 1943 | `		 * (PH7_HashmapUnlinkNode explains both halves) */` |
|   7522681 | 1944 | `		PH7_VmReleaseUnheldSlot(pVm,pEntry->nValIdx);` |
|         - | 1945 | `		/* Release the node */` |
|   7522681 | 1946 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   2986401 | 1947 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   1493198 | 1948 | `		}` |
|   7522681 | 1949 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1950 | `		/* Point to the next entry */` |
|   7522681 | 1951 | `		pEntry = pNext;` |
|   7522681 | 1952 | `		n++;` |
|         5 | 1953 | `	}` |
|   3552377 | 1954 | `	if( pMap->nEntry > 0 ){` |
|         - | 1955 | `		/* Release the hash bucket */` |
|   1343109 | 1956 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|    671552 | 1957 | `	}` |
|   3552377 | 1958 | `	if( FreeDS ){` |
|         - | 1959 | `		/* Free the whole instance */` |
|   3552367 | 1960 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   1776186 | 1961 | `	}else{` |
|         - | 1962 | `		/* Keep the instance but reset it's fields */` |
|        12 | 1963 | `		pMap->apBucket = 0;` |
|        12 | 1964 | `		pMap->iNextIdx = 0;` |
|        12 | 1965 | `	pMap->bIntKeySeen = 0;` |
|        12 | 1966 | `		pMap->nEntry = pMap->nSize = 0;` |
|        12 | 1967 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1968 | `	}` |
|   3552377 | 1969 | `	return SXRET_OK;` |
|   1776191 | 1970 | `}` |
|         - | 1971 | `/*` |
|         - | 1972 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1973 | ` * If the count reaches zero which mean no more variables` |
|         - | 1974 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1975 | ` */` |
|   6668234 | 1976 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1977 | `{` |
|   6668239 | 1978 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1979 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|   6668239 | 1980 | `	pMap->iRef--;` |
|   6668239 | 1981 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   3552347 | 1982 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   1776171 | 1983 | `	}` |
|   6668239 | 1984 | `}` |
|         - | 1985 | `/*` |
|         - | 1986 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1987 | ` * Write a pointer to the target node on success.` |
|         - | 1988 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1989 | ` */` |
|    801556 | 1990 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1991 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1992 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1993 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1994 | `	)` |
|         5 | 1995 | `{` |
|         - | 1996 | `	sxi32 rc;` |
|    801561 | 1997 | `	if( pMap->nEntry < 1 ){` |
|         - | 1998 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1999 | `		 */` |
|       731 | 2000 | `		return SXERR_NOTFOUND;` |
|         - | 2001 | `	}` |
|    800835 | 2002 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    800835 | 2003 | `	return rc;` |
|    400783 | 2004 | `}` |
|         - | 2005 | `/*` |
|         - | 2006 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 2007 | ` * hashmap.` |
|         - | 2008 | ` * If a node with the given key already exists in the database` |
|         - | 2009 | ` * then this function overwrite the old value.` |
|         - | 2010 | ` */` |
|   6772030 | 2011 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 2012 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2013 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 2014 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 2015 | `	)` |
|         5 | 2016 | `{` |
|         - | 2017 | `	sxi32 rc;` |
|         - | 2018 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 2019 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 2020 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 2021 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|   6772035 | 2022 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   6772035 | 2023 | `	return rc;` |
|         5 | 2024 | `}` |
|         - | 2025 | `/*` |
|         - | 2026 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 2027 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 2028 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 2029 | ` * This is the same routine that backs array_merge().` |
|         - | 2030 | ` */` |
|       662 | 2031 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 2032 | `{` |
|       664 | 2033 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         2 | 2034 | `}` |
|         - | 2035 | `/*` |
|         - | 2036 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 2037 | ` * hashmap.` |
|         - | 2038 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 2039 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 2040 | ` * The insertion by reference is triggered when the following` |
|         - | 2041 | ` * expression is encountered.` |
|         - | 2042 | ` * $var = 10;` |
|         - | 2043 | ` *  $a = array(&var);` |
|         - | 2044 | ` * OR` |
|         - | 2045 | ` *  $a[] =& $var;` |
|         - | 2046 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 2047 | ` * over it's contents.` |
|         - | 2048 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 2049 | ` * removed when the foreign ph7_value is unset.` |
|         - | 2050 | ` * Example:` |
|         - | 2051 | ` *  $var = 10;` |
|         - | 2052 | ` *  $a[] =& $var;` |
|         - | 2053 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 2054 | ` *  //Unset the foreign ph7_value now` |
|         - | 2055 | ` *  unset($var);` |
|         - | 2056 | ` *  echo count($a); //0` |
|         - | 2057 | ` * Note that this is a PH7 eXtension.` |
|         - | 2058 | ` * Refer to the official documentation for more information.` |
|         - | 2059 | ` * If a node with the given key already exists in the database` |
|         - | 2060 | ` * then this function overwrite the old value.` |
|         - | 2061 | ` */` |
|     56304 | 2062 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 2063 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2064 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 2065 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 2066 | `	)` |
|         5 | 2067 | `{` |
|         - | 2068 | `	sxi32 rc;` |
|     56309 | 2069 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 2070 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 2071 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 2072 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 2073 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 2074 | `		return PH7_ABORT;` |
|         - | 2075 | `	}` |
|     56309 | 2076 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     56309 | 2077 | `	return rc;` |
|     28157 | 2078 | `}` |
|         - | 2079 | `/*` |
|         - | 2080 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 2081 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 2082 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 2083 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 2084 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 2085 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 2086 | ` */` |
|     27638 | 2087 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 2088 | `{` |
|     27643 | 2089 | `	pStep->pCursor = pMap->pFirst;` |
|     27643 | 2090 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     27643 | 2091 | `	pMap->pActiveSteps = pStep;` |
|     27643 | 2092 | `}` |
|         - | 2093 | `/*` |
|         - | 2094 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 2095 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 2096 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 2097 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 2098 | ` */` |
|     27526 | 2099 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 2100 | `{` |
|     27531 | 2101 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     27531 | 2102 | `	while( *ppLink ){` |
|     27531 | 2103 | `		if( *ppLink == pStep ){` |
|     27531 | 2104 | `			*ppLink = pStep->pNextActive;` |
|     27531 | 2105 | `			pStep->pNextActive = 0;` |
|     27531 | 2106 | `			return;` |
|         - | 2107 | `		}` |
|       ! 0 | 2108 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 2109 | `	}` |
|     13768 | 2110 | `}` |
|         - | 2111 | `/*` |
|         - | 2112 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 2113 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 2114 | ` * return NULL.` |
|         - | 2115 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 2116 | ` */` |
|        64 | 2117 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         1 | 2118 | `{` |
|        65 | 2119 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|        65 | 2120 | `	if( pCur == 0 ){` |
|         - | 2121 | `		/* End of the list,return null */` |
|        27 | 2122 | `		return 0;` |
|         - | 2123 | `	}` |
|         - | 2124 | `	/* Advance the node cursor */` |
|        39 | 2125 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        39 | 2126 | `	return pCur;` |
|        33 | 2127 | `}` |
|         - | 2128 | `/*` |
|         - | 2129 | ` * Extract a node value.` |
|         - | 2130 | ` */` |
|    800312 | 2131 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 2132 | `{` |
|    800317 | 2133 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    800317 | 2134 | `	if( pEntry ){` |
|    800317 | 2135 | `		if( bStore ){` |
|    306041 | 2136 | `			PH7_MemObjStore(pEntry,pValue);` |
|    153023 | 2137 | `		}else{` |
|    494281 | 2138 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 2139 | `		}` |
|    399860 | 2140 | `	}else{` |
|       ! 0 | 2141 | `		PH7_MemObjRelease(pValue);` |
|         - | 2142 | `	}` |
|    800317 | 2143 | `}` |
|         - | 2144 | `/*` |
|         - | 2145 | ` * Extract a node key.` |
|         - | 2146 | ` */` |
|    248882 | 2147 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2148 | `{` |
|         - | 2149 | `	/* Fill with the current key */` |
|    248887 | 2150 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    241225 | 2151 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        41 | 2152 | `			SyBlobRelease(&pKey->sBlob);` |
|        20 | 2153 | `		}` |
|    241225 | 2154 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    241225 | 2155 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|    120615 | 2156 | `	}else{` |
|      7667 | 2157 | `		SyBlobReset(&pKey->sBlob);` |
|      7667 | 2158 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      7667 | 2159 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2160 | `	}` |
|    248887 | 2161 | `}` |
|         - | 2162 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 2163 | `/*` |
|         - | 2164 | ` * Store the address of nodes value in the given container.` |
|         - | 2165 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 2166 | ` * defined in 'builtin.c' for more information.` |
|         - | 2167 | ` */` |
|        26 | 2168 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         2 | 2169 | `{` |
|        28 | 2170 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2171 | `	ph7_value *pValue;` |
|         - | 2172 | `	sxu32 n;` |
|         - | 2173 | `	/* Initialize the container */` |
|        28 | 2174 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        78 | 2175 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 2176 | `		/* Extract node value */` |
|        52 | 2177 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        52 | 2178 | `		if( pValue ){` |
|        52 | 2179 | `			SySetPut(pOut,(const void *)&pValue);` |
|        25 | 2180 | `		}` |
|         - | 2181 | `		/* Point to the next entry */` |
|        52 | 2182 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        27 | 2183 | `	}` |
|         - | 2184 | `	/* Total inserted entries */` |
|        28 | 2185 | `	return (int)SySetUsed(pOut);` |
|         2 | 2186 | `}` |
|         - | 2187 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 2188 | `/*` |
|         - | 2189 | ` * Table of hashmap functions.` |
|         - | 2190 | ` */` |
|         - | 2191 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 2192 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 2193 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 2194 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 2195 | `	{"count",             ph7_hashmap_count },` |
|         - | 2196 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 2197 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 2198 | `	{"key_exists",        ph7_hashmap_key_exists },` |
|         - | 2199 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 2200 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 2201 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 2202 | `	{"array_unshift",     ph7_hashmap_unshift },` |
|         - | 2203 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 2204 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 2205 | `	{"max",               ph7_hashmap_max     },` |
|         - | 2206 | `	{"min",               ph7_hashmap_min     },` |
|         - | 2207 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 2208 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 2209 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 2210 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 2211 | `	{"array_merge_recursive", ph7_hashmap_merge_recursive },` |
|         - | 2212 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 2213 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 2214 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 2215 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 2216 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 2217 | `	{"array_udiff_assoc", ph7_hashmap_udiff_assoc },` |
|         - | 2218 | `	{"array_udiff_uassoc",ph7_hashmap_udiff_uassoc },` |
|         - | 2219 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 2220 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 2221 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 2222 | `	{"array_diff_ukey",   ph7_hashmap_diff_ukey },` |
|         - | 2223 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 2224 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 2225 | `	{"array_intersect_uassoc", ph7_hashmap_intersect_uassoc},` |
|         - | 2226 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 2227 | `	{"array_uintersect_assoc", ph7_hashmap_uintersect_assoc},` |
|         - | 2228 | `	{"array_uintersect_uassoc", ph7_hashmap_uintersect_uassoc},` |
|         - | 2229 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 2230 | `	{"array_multisort",   ph7_hashmap_multisort },` |
|         - | 2231 | `	{"array_intersect_ukey",  ph7_hashmap_intersect_ukey},` |
|         - | 2232 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 2233 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 2234 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 2235 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 2236 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 2237 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 2238 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 2239 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 2240 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 2241 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 2242 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 2243 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 2244 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 2245 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 2246 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 2247 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 2248 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 2249 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 2250 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 2251 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 2252 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 2253 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 2254 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 2255 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 2256 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 2257 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 2258 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 2259 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 2260 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 2261 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 2262 | `	{"natsort",           ph7_hashmap_natsort },` |
|         - | 2263 | `	{"natcasesort",       ph7_hashmap_natsort },` |
|         - | 2264 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 2265 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 2266 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 2267 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 2268 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 2269 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 2270 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 2271 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 2272 | `	{"range",             ph7_hashmap_range   },` |
|         - | 2273 | `	{"current",           ph7_hashmap_current },` |
|         - | 2274 | `	{"each",              ph7_hashmap_each    },` |
|         - | 2275 | `	{"pos",               ph7_hashmap_current },` |
|         - | 2276 | `	{"next",              ph7_hashmap_next    },` |
|         - | 2277 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 2278 | `	{"end",               ph7_hashmap_end     },` |
|         - | 2279 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 2280 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 2281 | `};` |
|         - | 2282 | `/*` |
|         - | 2283 | ` * Register the built-in hashmap functions defined above.` |
|         - | 2284 | ` */` |
|      4076 | 2285 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 2286 | `{` |
|         - | 2287 | `	sxu32 n;` |
|    366845 | 2288 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    362769 | 2289 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    181387 | 2290 | `	}` |
|      4081 | 2291 | `}` |
|         - | 2292 | `/*` |
|         - | 2293 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 2294 | ` * the BLOB given as the first argument.` |
|         - | 2295 | ` * This function is typically invoked when the user issue a call to` |
|         - | 2296 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 2297 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 2298 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 2299 | ` */` |
|         - | 2300 | `/*` |
|         - | 2301 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 2302 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 2303 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 2304 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 2305 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 2306 | ` *` |
|         - | 2307 | ` * bProp says the entries are an object's PROPERTIES: php then reads each key` |
|         - | 2308 | ``  * through zend_unmangle_property_name, so "\0C\0p" prints as `["p":"C":private]` `` |
|         - | 2309 | `` * and "\0*\0p" as `["p":protected]`. That is how a get_debug_info handler (and a`` |
|         - | 2310 | ` * userland __debugInfo()) labels a non-public slot, and it is the ONLY place the` |
|         - | 2311 | `` * decode happens — `var_dump((array)$obj)` shows the mangled key raw.`` |
|         - | 2312 | ` */` |
|       876 | 2313 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth,int bProp)` |
|         5 | 2314 | `{` |
|       881 | 2315 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2316 | `	ph7_value *pObj;` |
|       881 | 2317 | `	sxu32 n = 0;` |
|         - | 2318 | `	int isRef;` |
|       881 | 2319 | `	sxi32 rc = SXRET_OK;` |
|         - | 2320 | `	int i;` |
|      1312 | 2321 | `	for(;;){` |
|      2629 | 2322 | `		if( n >= pMap->nEntry ){` |
|       881 | 2323 | `			break;` |
|         - | 2324 | `		}` |
|      1753 | 2325 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 2326 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 2327 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|      1753 | 2328 | `		isRef = PH7_HashmapNodeIsRef(pEntry);` |
|      1753 | 2329 | `		if( ShowType ){` |
|         - | 2330 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 2331 | `			 * on the next line at the same indent (php). */` |
|      4311 | 2332 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|      2969 | 2333 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      1487 | 2334 | `			}` |
|      1347 | 2335 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       877 | 2336 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|       441 | 2337 | `			}else{` |
|         - | 2338 | `				SyString sCls,sNm;` |
|       475 | 2339 | `				if( bProp && PH7_UnmangleAttrName((const char *)SyBlobData(&pEntry->xKey.sKey),` |
|        34 | 2340 | `					SyBlobLength(&pEntry->xKey.sKey),&sCls,&sNm) ){` |
|        70 | 2341 | `					SyBlobFormat(&(*pOut),"[\"%z\"",&sNm);` |
|        70 | 2342 | `					if( sCls.nByte > 0 ){` |
|        23 | 2343 | `						if( sCls.zString[0] == '*' ){` |
|         3 | 2344 | `							SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|         2 | 2345 | `						}else{` |
|        21 | 2346 | `							SyBlobFormat(&(*pOut),":\"%z\":private",&sCls);` |
|         - | 2347 | `						}` |
|        11 | 2348 | `					}` |
|        70 | 2349 | `					SyBlobAppend(&(*pOut),"]=>",sizeof("]=>")-1);` |
|        36 | 2350 | `				}else{` |
|       608 | 2351 | `					SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|       201 | 2352 | `						SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2353 | `				}` |
|         - | 2354 | `			}` |
|      1347 | 2355 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      1347 | 2356 | `			if( pObj ){` |
|      1347 | 2357 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|      1347 | 2358 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2359 | `					break;` |
|         - | 2360 | `				}` |
|       671 | 2361 | `			}` |
|       676 | 2362 | `		}else{` |
|         - | 2363 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 2364 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 2365 | `			 * php's extra blank line. References carry no marker. */` |
|      2434 | 2366 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      2028 | 2367 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      1016 | 2368 | `			}` |
|       410 | 2369 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       173 | 2370 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        88 | 2371 | `			}else{` |
|         - | 2372 | `				SyString sCls,sNm;` |
|       238 | 2373 | `				if( bProp && PH7_UnmangleAttrName((const char *)SyBlobData(&pEntry->xKey.sKey),` |
|        47 | 2374 | `					SyBlobLength(&pEntry->xKey.sKey),&sCls,&sNm) ){` |
|        96 | 2375 | `					SyBlobFormat(&(*pOut),"[%z",&sNm);` |
|        96 | 2376 | `					if( sCls.nByte > 0 ){` |
|        11 | 2377 | `						if( sCls.zString[0] == '*' ){` |
|         3 | 2378 | `							SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|         2 | 2379 | `						}else{` |
|         9 | 2380 | `							SyBlobFormat(&(*pOut),":%z:private",&sCls);` |
|         - | 2381 | `						}` |
|         5 | 2382 | `					}` |
|        96 | 2383 | `					SyBlobAppend(&(*pOut),"] => ",sizeof("] => ")-1);` |
|        49 | 2384 | `				}else{` |
|       215 | 2385 | `					SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        71 | 2386 | `						SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2387 | `				}` |
|         - | 2388 | `			}` |
|       406 | 2389 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       234 | 2390 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        57 | 2391 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        57 | 2392 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        57 | 2393 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2394 | `					break;` |
|         - | 2395 | `				}` |
|        30 | 2396 | `			}else{` |
|       356 | 2397 | `				if( pObj ){` |
|       356 | 2398 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       176 | 2399 | `				}` |
|       356 | 2400 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 2401 | `			}` |
|         - | 2402 | `		}` |
|         - | 2403 | `		/* Point to the next entry */` |
|      1753 | 2404 | `		n++;` |
|      1753 | 2405 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         5 | 2406 | `	}` |
|       881 | 2407 | `	return rc;` |
|         5 | 2408 | `}` |
|       804 | 2409 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         5 | 2410 | `{` |
|         - | 2411 | `	sxi32 rc;` |
|         - | 2412 | `	int i;` |
|       809 | 2413 | `	if( nDepth > 31 ){` |
|         - | 2414 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 2415 | `		/* Nesting limit reached */` |
|       ! 0 | 2416 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 2417 | `		return SXERR_LIMIT;` |
|         - | 2418 | `	}` |
|       809 | 2419 | `	if( ShowType ){` |
|         - | 2420 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 2421 | `		 * newline (a nested array is itself an entry value line). */` |
|       657 | 2422 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|       657 | 2423 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       657 | 2424 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth,0);` |
|       793 | 2425 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       140 | 2426 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        72 | 2427 | `		}` |
|       657 | 2428 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       657 | 2429 | `		return rc;` |
|         - | 2430 | `	}` |
|         - | 2431 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       156 | 2432 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       524 | 2433 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       370 | 2434 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       186 | 2435 | `	}` |
|       156 | 2436 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       156 | 2437 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth,0);` |
|       524 | 2438 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       370 | 2439 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       186 | 2440 | `	}` |
|       156 | 2441 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       156 | 2442 | `	return rc;` |
|       407 | 2443 | `}` |
|         - | 2444 | `/*` |
|         - | 2445 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 2446 | ` * retrieved entry.` |
|         - | 2447 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 2448 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 2449 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 2450 | ` * a value different from PH7_OK.` |
|         - | 2451 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 2452 | ` */` |
|     45694 | 2453 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 2454 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2455 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 2456 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 2457 | `	)` |
|         5 | 2458 | `{` |
|         - | 2459 | `	ph7_hashmap_node *pEntry;` |
|         - | 2460 | `	ph7_value sKey,sValue;` |
|         - | 2461 | `	sxi32 rc;` |
|         - | 2462 | `	sxu32 n;` |
|         - | 2463 | `	/* Initialize walker parameter */` |
|     45699 | 2464 | `	rc = SXRET_OK;` |
|     45699 | 2465 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     45699 | 2466 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     45699 | 2467 | `	n = pMap->nEntry;` |
|     45699 | 2468 | `	pEntry = pMap->pFirst;` |
|         - | 2469 | `	/* Start the iteration process */` |
|    143208 | 2470 | `	for(;;){` |
|    286421 | 2471 | `		if( n < 1 ){` |
|     45671 | 2472 | `			break;` |
|         - | 2473 | `		}` |
|         - | 2474 | `		/* Extract a copy of the key and a copy the current value */` |
|    240755 | 2475 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    240755 | 2476 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 2477 | `		/* Invoke the user callback */` |
|    240755 | 2478 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 2479 | `		/* Release the copy of the key and the value */` |
|    240755 | 2480 | `		PH7_MemObjRelease(&sKey);` |
|    240755 | 2481 | `		PH7_MemObjRelease(&sValue);` |
|    240755 | 2482 | `		if( rc != PH7_OK ){` |
|         - | 2483 | `			/* Callback request an operation abort */` |
|        31 | 2484 | `			return SXERR_ABORT;` |
|         - | 2485 | `		}` |
|         - | 2486 | `		/* Point to the next entry */` |
|    240727 | 2487 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    240727 | 2488 | `		n--;` |
|         5 | 2489 | `	}` |
|         - | 2490 | `	/* All done */` |
|     45671 | 2491 | `	return SXRET_OK;` |
|     22852 | 2492 | `}` |
|         - | 2493 |  |
