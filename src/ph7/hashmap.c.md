# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4007/4426 lines (90.53%)

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
|   7466928 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7466933 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7466933 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    648614 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    648619 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    648619 |   35 | `	sxu32 nH = 5381;` |
|    648619 |   36 | `	zEnd = &zIn[nLen];` |
|    735802 |   37 | `	for(;;){` |
|   1471609 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1252651 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1122797 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    980441 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    648619 |   43 | `	return nH;` |
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
|   3165774 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3165779 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3165779 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3165779 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3165779 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3165779 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3165779 |  112 | `	pNode->nHash = nHash;` |
|   3165779 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3165779 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3165779 |  115 | `	return pNode;` |
|   1582892 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    274568 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    274573 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    274573 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    274573 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    274573 |  133 | `	pNode->pMap  = &(*pMap);` |
|    274573 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    274573 |  135 | `	pNode->nHash = nHash;` |
|    274573 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    274573 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    274573 |  138 | `	pNode->nValIdx = nValIdx;` |
|    274573 |  139 | `	return pNode;` |
|    137289 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3440342 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3440347 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2953181 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2953181 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1476588 |  150 | `	}` |
|   3440347 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3440347 |  153 | `	if( pMap->pFirst == 0 ){` |
|     93079 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     93079 |  156 | `		pMap->pCur = pNode;` |
|     46542 |  157 | `	}else{` |
|   3347273 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3440347 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3440347 |  174 | `	++pMap->nEntry;` |
|   3440347 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7962 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7967 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7967 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7967 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7497 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3751 |  187 | `	}else{` |
|       472 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7967 |  190 | `	if( pNode->pNextCollide ){` |
|      5179 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2588 |  192 | `	}` |
|      7967 |  193 | `	if( pMap->pFirst == pNode ){` |
|       199 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        97 |  195 | `	}` |
|      7967 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       231 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|       113 |  199 | `	}` |
|      7967 |  200 | `	if( pMap->pActiveSteps ){` |
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
|      7967 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7967 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       209 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       209 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       209 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|       102 |  218 | `		}` |
|       102 |  219 | `	}` |
|      7967 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7705 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3850 |  222 | `	}` |
|      7967 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7967 |  224 | `	pMap->nEntry--;` |
|      7967 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|       123 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|       123 |  228 | `		pMap->apBucket = 0;` |
|       123 |  229 | `		pMap->nSize = 0;` |
|       123 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        59 |  231 | `	}` |
|      7967 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3440342 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3440347 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     98207 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     98207 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     98207 |  245 | `		if( nNew < 1 ){` |
|     93079 |  246 | `			nNew = 16;` |
|     46537 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     98207 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     98207 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     98207 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     98207 |  260 | `		pMap->apBucket = apNew;` |
|     98207 |  261 | `		pMap->nSize = nNew;` |
|     98207 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     93079 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5133 |  267 | `		pEntry = pMap->pFirst;` |
|      5133 |  268 | `		n = 0;` |
|   2112692 |  269 | `		for( ;; ){` |
|   4225389 |  270 | `			if( n >= pMap->nEntry ){` |
|      5133 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   4220261 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   4220261 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4220261 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3597937 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3597937 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1798966 |  280 | `			}` |
|   4220261 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   4220261 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4220261 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5133 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2564 |  288 | `	}` |
|   3347273 |  289 | `	return SXRET_OK;` |
|   1720176 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3165774 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3165779 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3165741 |  310 | `		if( pValue ){` |
|   3165735 |  311 | `			sSafeVal = *pValue;` |
|   3165735 |  312 | `			pValue = &sSafeVal;` |
|   1582865 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3165741 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3165741 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3165741 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3165735 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1582865 |  322 | `		}` |
|   3165741 |  323 | `		nIdx = pObj->nIdx;` |
|   1582873 |  324 | `	}else{` |
|        39 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3165779 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3165779 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3165779 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3165779 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        39 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        19 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3165779 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3165779 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3165779 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3165779 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3165779 |  349 | `	return SXRET_OK;` |
|   1582892 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    274568 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    274573 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    226289 |  370 | `		if( pValue ){` |
|    225979 |  371 | `			sSafeVal = *pValue;` |
|    225979 |  372 | `			pValue = &sSafeVal;` |
|    112987 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    226289 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    226289 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    226289 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    225979 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    112987 |  382 | `		}` |
|    226289 |  383 | `		nIdx = pObj->nIdx;` |
|    113147 |  384 | `	}else{` |
|     48289 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    274573 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    274573 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    274573 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    274573 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     48289 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     24142 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    274573 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    274573 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    274573 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    274573 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    274573 |  409 | `	return SXRET_OK;` |
|    137289 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4287874 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4287879 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       725 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4287159 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4287159 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110564110 |  433 | `	for(;;){` |
| 221128225 |  434 | `		if( pNode == 0 ){` |
|   4282181 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216846044 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216843032 |  438 | `			&& pNode->nHash == nHash` |
| 108422504 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      4983 |  441 | `				if( ppNode ){` |
|      4965 |  442 | `					*ppNode = pNode;` |
|      2480 |  443 | `				}` |
|      4983 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216841067 |  447 | `		pNode = pNode->pNextCollide;` |
|         1 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4282181 |  450 | `	return SXERR_NOTFOUND;` |
|   2143942 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    410666 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    410671 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     36625 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    374051 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    374051 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    308723 |  475 | `	for(;;){` |
|    617451 |  476 | `		if( pNode == 0 ){` |
|    315141 |  477 | `			break;` |
|         - |  478 | `		}` |
|    302310 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    300799 |  480 | `			&& pNode->nHash == nHash` |
|    179149 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     59015 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     58915 |  484 | `				if( ppNode ){` |
|     58887 |  485 | `					*ppNode = pNode;` |
|     29441 |  486 | `				}` |
|     58915 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    243405 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    315141 |  493 | `	return SXERR_NOTFOUND;` |
|    205338 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    410798 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    410803 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    410803 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    410803 |  504 | `	int isNeg = FALSE, nDigit;` |
|    410803 |  505 | `	if( zIn >= zEnd ){` |
|       ! 0 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    410803 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    410799 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    410799 |  516 | `	zDigit = zIn;` |
|    205831 |  517 | `	for(;;){` |
|    411667 |  518 | `		if( zIn >= zEnd ){` |
|       251 |  519 | `			break;` |
|         - |  520 | `		}` |
|    411417 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    410549 |  523 | `			return FALSE;` |
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
|    205404 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    141094 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    141099 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    141099 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    136237 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast */` |
|       ! 0 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  559 | `		}` |
|    136237 |  560 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Perform a blob lookup */` |
|    136217 |  562 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    136217 |  563 | `			goto result;` |
|         - |  564 | `		}` |
|        10 |  565 | `	}` |
|         - |  566 | `	/* Perform an int lookup */` |
|      4887 |  567 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  568 | `		/* Force an integer cast */` |
|        35 |  569 | `		PH7_MemObjToInteger(pKey);` |
|        17 |  570 | `	}` |
|         - |  571 | `	/* Perform an int lookup */` |
|      4887 |  572 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     70547 |  573 | `result:` |
|    141099 |  574 | `	if( rc == SXRET_OK ){` |
|         - |  575 | `		/* Node found */` |
|     63039 |  576 | `		if( ppNode ){` |
|     62989 |  577 | `			*ppNode = pNode;` |
|     31492 |  578 | `		}` |
|     63039 |  579 | `		return SXRET_OK;` |
|         - |  580 | `	}` |
|         - |  581 | `	/* No such entry */` |
|     78065 |  582 | `	return SXERR_NOTFOUND;` |
|     70552 |  583 | `}` |
|         - |  584 | `/*` |
|         - |  585 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  586 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  587 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  588 | ` * via HashmapAppendIndexBusy.` |
|         - |  589 | ` */` |
|   2141466 |  590 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  591 | `{` |
|   2141471 |  592 | `	if( iKey >= pMap->iNextIdx ){` |
|   2141201 |  593 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  594 | `		/* Make sure the automatic index is not reserved */` |
|   2141201 |  595 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  596 | `			pMap->iNextIdx++;` |
|       ! 0 |  597 | `		}` |
|   1070598 |  598 | `	}` |
|   2141471 |  599 | `}` |
|         - |  600 | `/*` |
|         - |  601 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  602 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  603 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  604 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  605 | ` */` |
|   1023940 |  606 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  607 | `{` |
|   1023945 |  608 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  609 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  610 | `		return TRUE;` |
|         - |  611 | `	}` |
|   1023939 |  612 | `	return FALSE;` |
|    511975 |  613 | `}` |
|         - |  614 | `/*` |
|         - |  615 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  616 | ` * hashmap.` |
|         - |  617 | ` * If a node with the given key already exists in the database` |
|         - |  618 | ` * then this function overwrite the old value.` |
|         - |  619 | ` */` |
|   3391600 |  620 | `static sxi32 HashmapInsert(` |
|         - |  621 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  622 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  623 | `	ph7_value *pVal    /* Node value */` |
|         - |  624 | `	)` |
|         5 |  625 | `{` |
|   3391605 |  626 | `	ph7_hashmap_node *pNode = 0;` |
|   3391605 |  627 | `	sxi32 rc = SXRET_OK;` |
|   3391605 |  628 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    229857 |  629 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  630 | `			/* Force a string cast */` |
|         3 |  631 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  632 | `		}` |
|    229857 |  633 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|      3809 |  634 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  635 | `				/* Automatic index assign */` |
|      3581 |  636 | `				pKey = 0;` |
|      1788 |  637 | `			}` |
|      3809 |  638 | `			goto IntKey;` |
|         - |  639 | `		}` |
|    339077 |  640 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    113024 |  641 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  642 | `				/* Overwrite the old value */` |
|         - |  643 | `				ph7_value *pElem;` |
|       480 |  644 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       480 |  645 | `				if( pElem ){` |
|       480 |  646 | `					if( pVal ){` |
|       480 |  647 | `						PH7_MemObjStore(pVal,pElem);` |
|       242 |  648 | `					}else{` |
|         - |  649 | `						/* Nullify the entry */` |
|       ! 0 |  650 | `						PH7_MemObjToNull(pElem);` |
|         - |  651 | `					}` |
|       238 |  652 | `				}` |
|       480 |  653 | `				return SXRET_OK;` |
|         - |  654 | `		}` |
|    225577 |  655 | `		if( pMap == pMap->pVm->pGlobal ){` |
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
|    225447 |  668 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    225447 |  669 | `		return rc;` |
|         - |  670 | `	}` |
|   1580874 |  671 | `IntKey:` |
|   3165557 |  672 | `	if( pKey ){` |
|   2141647 |  673 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  674 | `			/* Force an integer cast */` |
|       261 |  675 | `			PH7_MemObjToInteger(pKey);` |
|       130 |  676 | `		}` |
|   2141647 |  677 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  678 | `			/* Overwrite the old value */` |
|         - |  679 | `			ph7_value *pElem;` |
|       181 |  680 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       181 |  681 | `			if( pElem ){` |
|       181 |  682 | `				if( pVal ){` |
|       181 |  683 | `					PH7_MemObjStore(pVal,pElem);` |
|        91 |  684 | `				}else{` |
|         - |  685 | `					/* Nullify the entry */` |
|       ! 0 |  686 | `					PH7_MemObjToNull(pElem);` |
|         - |  687 | `				}` |
|        90 |  688 | `			}` |
|       181 |  689 | `			return SXRET_OK;` |
|         - |  690 | `		}` |
|   2141467 |  691 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  692 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  693 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  694 | `			char zKey[24];` |
|         3 |  695 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  696 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  697 | `		}` |
|         - |  698 | `		/* Perform a 64-bit-int-key insertion */` |
|   2141465 |  699 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2141465 |  700 | `		if( rc == SXRET_OK ){` |
|   2141465 |  701 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1070730 |  702 | `		}` |
|   1070735 |  703 | `	}else{` |
|   1023915 |  704 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  705 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  706 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  707 | `		}` |
|   1023913 |  708 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  709 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  710 | `		}` |
|         - |  711 | `		/* Assign an automatic index */` |
|   1023907 |  712 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1023907 |  713 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1023905 |  714 | `			++pMap->iNextIdx;` |
|    511950 |  715 | `		}` |
|         - |  716 | `	}` |
|         - |  717 | `	/* Insertion result */` |
|   3165367 |  718 | `	return rc;` |
|   1695805 |  719 | `}` |
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
|     48332 |  747 | `static sxi32 HashmapInsertByRef(` |
|         - |  748 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  749 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  750 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  751 | `	)` |
|         5 |  752 | `{` |
|     48337 |  753 | `	ph7_hashmap_node *pNode = 0;` |
|     48337 |  754 | `	sxi32 rc = SXRET_OK;` |
|     48337 |  755 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     48301 |  756 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  757 | `			/* Force a string cast */` |
|       ! 0 |  758 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  759 | `		}` |
|     48301 |  760 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  761 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  762 | `				/* Automatic index assign */` |
|       ! 0 |  763 | `				pKey = 0;` |
|       ! 0 |  764 | `			}` |
|         3 |  765 | `			goto IntKey;` |
|         - |  766 | `		}` |
|     72446 |  767 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     24147 |  768 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  769 | `				/* Overwrite */` |
|        11 |  770 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  771 | `				pNode->nValIdx = nRefIdx;` |
|         - |  772 | `				/* Install in the reference table */` |
|        11 |  773 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  774 | `				return SXRET_OK;` |
|         - |  775 | `		}` |
|         - |  776 | `		/* Perform a blob-key insertion */` |
|     48289 |  777 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     48289 |  778 | `		return rc;` |
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
|     24171 |  811 | `}` |
|         - |  812 | `/*` |
|         - |  813 | ` * Extract node value.` |
|         - |  814 | ` */` |
|   1449851 |  815 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  816 | `{` |
|         - |  817 | `	/* Point to the desired object */` |
|         - |  818 | `	ph7_value *pObj;` |
|   1449856 |  819 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1449856 |  820 | `	return pObj;` |
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
|     71876 |  866 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  867 | `{` |
|         - |  868 | `	ph7_value sObj1,sObj2;` |
|         - |  869 | `	sxi32 rc;` |
|     71881 |  870 | `	if( pLeft == pRight ){` |
|         - |  871 | `		/*` |
|         - |  872 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  873 | `		 * below for more information on this sceanario.` |
|         - |  874 | `		 */` |
|       ! 0 |  875 | `		return 0;` |
|         - |  876 | `	}` |
|         - |  877 | `	/* Do the comparison */` |
|     71881 |  878 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     71881 |  879 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     71881 |  880 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     71881 |  881 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     71881 |  882 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     71881 |  883 | `	PH7_MemObjRelease(&sObj1);` |
|     71881 |  884 | `	PH7_MemObjRelease(&sObj2);` |
|     71881 |  885 | `	return rc;` |
|     35896 |  886 | `}` |
|         - |  887 | `/*` |
|         - |  888 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  889 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  890 | ` */` |
|     14000 |  891 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  892 | `{` |
|     14005 |  893 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  894 | `	sxu32 nBucket;` |
|         - |  895 | `	/* Remove old collision links */` |
|     14005 |  896 | `	if( pEntry->pPrevCollide ){` |
|     11398 |  897 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5710 |  898 | `	}else{` |
|      2612 |  899 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  900 | `	}` |
|     14005 |  901 | `	if( pEntry->pNextCollide ){` |
|      1143 |  902 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       588 |  903 | `	}` |
|     14005 |  904 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  905 | `	/* Compute the new hash */` |
|     14005 |  906 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     14005 |  907 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     14005 |  908 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  909 | `	/* Link to the new bucket */` |
|     14005 |  910 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14005 |  911 | `	if( pMap->apBucket[nBucket] ){` |
|     11727 |  912 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5876 |  913 | `	}` |
|     14005 |  914 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14005 |  915 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  916 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  917 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  918 | `	 * the no-overflow invariant uniform). */` |
|     14005 |  919 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     14005 |  920 | `		pMap->iNextIdx++;` |
|      7000 |  921 | `	}` |
|     14005 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Perform a linear search on a given hashmap.` |
|         - |  925 | ` * Write a pointer to the target node on success.` |
|         - |  926 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  927 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  928 | ` * for more information.` |
|         - |  929 | ` */` |
|     33320 |  930 | `static int HashmapFindValue(` |
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
|     33325 |  943 | `	pEntry = pMap->pFirst;` |
|     33325 |  944 | `	n = pMap->nEntry;` |
|     33325 |  945 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33325 |  946 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     79269 |  947 | `	for(;;){` |
|    158542 |  948 | `		if( n < 1 ){` |
|       115 |  949 | `			break;` |
|         - |  950 | `		}` |
|         - |  951 | `		/* Extract node value */` |
|    158428 |  952 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    158428 |  953 | `		if( pVal ){` |
|         - |  954 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  955 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  956 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  957 | `			 * so null needles/values take the same path as everything else` |
|         - |  958 | `			 * (the historical null-to-null shortcut here made` |
|         - |  959 | `			 * in_array(null, [""]) false where php says true). */` |
|    158428 |  960 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    158428 |  961 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    158428 |  962 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    158428 |  963 | `			PH7_MemObjRelease(&sVal);` |
|    158428 |  964 | `			PH7_MemObjRelease(&sNeedle);` |
|    158428 |  965 | `			if( rc == 0 ){` |
|     33211 |  966 | `				if( ppNode ){` |
|        23 |  967 | `					*ppNode = pEntry;` |
|        11 |  968 | `				}` |
|         - |  969 | `				/* Match found*/` |
|     33211 |  970 | `				return SXRET_OK;` |
|         - |  971 | `			}` |
|     62609 |  972 | `		}` |
|         - |  973 | `		/* Point to the next entry */` |
|    125222 |  974 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    125222 |  975 | `		n--;` |
|         5 |  976 | `	}` |
|         - |  977 | `	/* No such entry */` |
|       115 |  978 | `	return SXERR_NOTFOUND;` |
|     16665 |  979 | `}` |
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
|    664450 | 1165 | `static sxi32 HashmapDuplicateNode(` |
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
|    664455 | 1176 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
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
|    664449 | 1201 | `	sSafeVal = *pVal;` |
|         - | 1202 |  |
|    664449 | 1203 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1204 | `		/* Blob key insertion */` |
|      4125 | 1205 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4125 | 1206 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4125 | 1207 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4125 | 1208 | `		PH7_MemObjRelease(&sKey);` |
|      2065 | 1209 | `	}else{` |
|         - | 1210 | `		/* Int key */` |
|    660329 | 1211 | `		if( iAction == 0 ){ /* Merge */` |
|    660087 | 1212 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    330286 | 1213 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1214 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1215 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1216 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1217 | `		}else{ /* Dup */` |
|       215 | 1218 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1219 | `		}` |
|         - | 1220 | `	}` |
|    664449 | 1221 | `	return rc;` |
|    332230 | 1222 | `}` |
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
|      2780 | 1235 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1236 | `{` |
|         - | 1237 | `	ph7_hashmap_node *pEntry;` |
|         - | 1238 | `	ph7_value *pVal;` |
|         - | 1239 | `	sxi32 rc;` |
|         - | 1240 | `	sxu32 n;` |
|      2785 | 1241 | `	if( pSrc == pDest ){` |
|         - | 1242 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1243 | `		 * Unlike the zend engine.` |
|         - | 1244 | `		 */` |
|       ! 0 | 1245 | `		return SXRET_OK;` |
|         - | 1246 | `	}` |
|         - | 1247 | `	/* Point to the first inserted entry in the source */` |
|      2785 | 1248 | `	pEntry = pSrc->pFirst;` |
|         - | 1249 | `	/* Perform the merge */` |
|    662925 | 1250 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1251 | `		/* Extract the node value */` |
|    660145 | 1252 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    660145 | 1253 | `		if( pVal ){` |
|         - | 1254 | `			/* Make a local copy of the value.` |
|         - | 1255 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1256 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1257 | `			 * to the old pool.` |
|         - | 1258 | `			 */` |
|    660145 | 1259 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    330075 | 1260 | `		}else{` |
|       ! 0 | 1261 | `			rc = SXRET_OK;` |
|         - | 1262 | `		}` |
|    660145 | 1263 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1264 | `			return rc;` |
|         - | 1265 | `		}` |
|         - | 1266 | `		/* Point to the next entry */` |
|    660145 | 1267 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    330075 | 1268 | `	}` |
|      2785 | 1269 | `	return SXRET_OK;` |
|      1395 | 1270 | `}` |
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
|      4026 | 1320 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1321 | `{` |
|         - | 1322 | `	ph7_hashmap_node *pEntry;` |
|         - | 1323 | `	ph7_value *pVal;` |
|         - | 1324 | `	sxi32 rc;` |
|         - | 1325 | `	sxu32 n;` |
|      4031 | 1326 | `	if( pSrc == pDest ){` |
|         - | 1327 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1328 | `		 * Unlike the zend engine.` |
|         - | 1329 | `		 */` |
|       ! 0 | 1330 | `		return SXRET_OK;` |
|         - | 1331 | `	}` |
|         - | 1332 | `	/* Point to the first inserted entry in the source */` |
|      4031 | 1333 | `	pEntry = pSrc->pFirst;` |
|         - | 1334 | `	/* Perform the duplication */` |
|      8297 | 1335 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1336 | `		/* Extract the node value */` |
|      4271 | 1337 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4271 | 1338 | `		if( pVal ){` |
|      4271 | 1339 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2138 | 1340 | `		}else{` |
|       ! 0 | 1341 | `			rc = SXRET_OK;` |
|         - | 1342 | `		}` |
|      4271 | 1343 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1344 | `			return rc;` |
|         - | 1345 | `		}` |
|         - | 1346 | `		/* Point to the next entry */` |
|      4271 | 1347 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2138 | 1348 | `	}` |
|      4031 | 1349 | `	return SXRET_OK;` |
|      2018 | 1350 | `}` |
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
|        50 | 1406 | `static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)` |
|         3 | 1407 | `{` |
|         - | 1408 | `	ph7_foreach_step *pStep;` |
|        53 | 1409 | `	sxi32 nRef = 0;` |
|       103 | 1410 | `	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        53 | 1411 | `		if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        45 | 1412 | `			nRef++;` |
|        21 | 1413 | `		}` |
|        28 | 1414 | `	}` |
|        53 | 1415 | `	return nRef;` |
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
|    234380 | 1426 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1427 | `{` |
|    234385 | 1428 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1429 | `	ph7_hashmap *pNew;` |
|         - | 1430 | `	ph7_value *pBacking;` |
|         - | 1431 | `	sxu32 nValIdx;` |
|         - | 1432 | `	int bValueInPool;` |
|    234385 | 1433 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    234385 | 1434 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1435 | `		/* Sole owner, no separation needed */` |
|    231695 | 1436 | `		return pMap;` |
|         - | 1437 | `	}` |
|      2695 | 1438 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1439 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1440 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1441 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1442 | `		return pMap;` |
|         - | 1443 | `	}` |
|         - | 1444 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1445 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1446 | `	 * frame is popped. */` |
|      2569 | 1447 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2569 | 1448 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2564 | 1449 | `		if( pBacking && pBacking != pValue` |
|      2540 | 1450 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2521 | 1451 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1452 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2521 | 1453 | `			pMap->iRef--;` |
|      2521 | 1454 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1455 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2475 | 1456 | `				pMap->iRef++;` |
|      2475 | 1457 | `				return pMap;` |
|         - | 1458 | `			}` |
|        48 | 1459 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|        48 | 1460 | `			if( pNew == 0 ){` |
|       ! 0 | 1461 | `				pMap->iRef++;` |
|       ! 0 | 1462 | `				return pMap;` |
|         - | 1463 | `			}` |
|        48 | 1464 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1465 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1466 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1467 | `				pMap->iRef++;` |
|       ! 0 | 1468 | `				return pMap;` |
|         - | 1469 | `			}` |
|        48 | 1470 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|        48 | 1471 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1472 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1473 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1474 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1475 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1476 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1477 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1478 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|        48 | 1479 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|        48 | 1480 | `			if( pBacking ){` |
|        48 | 1481 | `				pBacking->x.pOther = pNew;` |
|        23 | 1482 | `			}` |
|         - | 1483 | `			/* Update the stack value to match */` |
|        48 | 1484 | `			pValue->x.pOther = pNew;` |
|        48 | 1485 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|        48 | 1486 | `			return pNew;` |
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
|    117195 | 1520 | `}` |
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
|      3904 | 1558 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1559 | `{` |
|         - | 1560 | `	ph7_hashmap_node *pEntry;` |
|      3909 | 1561 | `	sxi32 rc = SXRET_OK;` |
|         - | 1562 | `	ph7_value *pObj;` |
|         - | 1563 | `	sxu32 n;` |
|      3909 | 1564 | `	if( pLeft == pRight ){` |
|         - | 1565 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1566 | `		 * Unlike the zend engine.` |
|         - | 1567 | `		 */` |
|       ! 0 | 1568 | `		return SXRET_OK;` |
|         - | 1569 | `	}` |
|         - | 1570 | `	/* Perform the union */` |
|      3909 | 1571 | `	pEntry = pRight->pFirst;` |
|      3943 | 1572 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
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
|      3909 | 1606 | `	return SXRET_OK;` |
|      1957 | 1607 | `}` |
|         - | 1608 | `/*` |
|         - | 1609 | ` * Allocate a new hashmap.` |
|         - | 1610 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1611 | ` */` |
|    146588 | 1612 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1613 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1614 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1615 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1616 | `	)` |
|         5 | 1617 | `{` |
|         - | 1618 | `	ph7_hashmap *pMap;` |
|         - | 1619 | `	/* Allocate a new instance */` |
|    146593 | 1620 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    146593 | 1621 | `	if( pMap == 0 ){` |
|       ! 0 | 1622 | `		return 0;` |
|         - | 1623 | `	}` |
|         - | 1624 | `	/* Zero the structure */` |
|    146593 | 1625 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1626 | `	/* Fill in the structure */` |
|    146593 | 1627 | `	pMap->pVm = &(*pVm);` |
|    146593 | 1628 | `	pMap->iRef = 1;` |
|         - | 1629 | `	/* Default hash functions */` |
|    146593 | 1630 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    146593 | 1631 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    146593 | 1632 | `	return pMap;` |
|     73299 | 1633 | `}` |
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
|      3574 | 1654 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|      3579 | 1674 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3579 | 1675 | `	if( pMap == 0 ){` |
|       ! 0 | 1676 | `		return SXERR_MEM;` |
|         - | 1677 | `	}` |
|      3579 | 1678 | `	pVm->pGlobal = pMap;` |
|         - | 1679 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3579 | 1680 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3579 | 1681 | `	if( pObj == 0 ){` |
|       ! 0 | 1682 | `		return SXERR_MEM;` |
|         - | 1683 | `	}` |
|      3579 | 1684 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1685 | `	/* Record object index */` |
|      3579 | 1686 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1687 | `	/* Install the special $GLOBALS array */` |
|      3579 | 1688 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3579 | 1689 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1690 | `		return rc;` |
|         - | 1691 | `	}` |
|         - | 1692 | `	/* Install superglobals now */` |
|     39319 | 1693 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1694 | `		ph7_value *pSuper;` |
|         - | 1695 | `		/* Request an empty array */` |
|     35745 | 1696 | `		pSuper = ph7_new_array(&(*pVm));` |
|     35745 | 1697 | `		if( pSuper == 0 ){` |
|       ! 0 | 1698 | `			return SXERR_MEM;` |
|         - | 1699 | `		}` |
|         - | 1700 | `		/* Install */` |
|     35745 | 1701 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     35745 | 1702 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1703 | `			return rc;` |
|         - | 1704 | `		}` |
|         - | 1705 | `		/* Release the value now it have been installed */` |
|     35745 | 1706 | `		ph7_release_value(&(*pVm),pSuper);` |
|     17875 | 1707 | `	}` |
|         - | 1708 | `	/* Set some $_SERVER entries */` |
|      3579 | 1709 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1710 | `	/*` |
|         - | 1711 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1712 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1713 | `	 */` |
|      7153 | 1714 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1715 | `		"SCRIPT_FILENAME",` |
|      1787 | 1716 | `		pFile ? pFile->zString : ":Memory:",` |
|      3574 | 1717 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1718 | `		);` |
|         - | 1719 | `	/* All done,all super-global are installed now */` |
|      3579 | 1720 | `	return SXRET_OK;` |
|      1792 | 1721 | `}` |
|         - | 1722 | `/*` |
|         - | 1723 | ` * Release a hashmap.` |
|         - | 1724 | ` */` |
|    102198 | 1725 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1726 | `{` |
|         - | 1727 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    102203 | 1728 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1729 | `	sxu32 n;` |
|    102203 | 1730 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1731 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1732 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1733 | `		return SXRET_OK;` |
|         - | 1734 | `	}` |
|    102203 | 1735 | `	if( pMap->pActiveSteps ){` |
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
|    102203 | 1748 | `	n = 0;` |
|    102203 | 1749 | `	pEntry = pMap->pFirst;` |
|   1726329 | 1750 | `	for(;;){` |
|   3452663 | 1751 | `		if( n >= pMap->nEntry ){` |
|    102203 | 1752 | `			break;` |
|         - | 1753 | `		}` |
|   3350465 | 1754 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1755 | `		/* Remove the reference from the foreign table */` |
|   3350465 | 1756 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3350465 | 1757 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1758 | `			/* Restore the ph7_value to the free list */` |
|   3350435 | 1759 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1675215 | 1760 | `		}` |
|         - | 1761 | `		/* Release the node */` |
|   3350465 | 1762 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    196681 | 1763 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     98338 | 1764 | `		}` |
|   3350465 | 1765 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1766 | `		/* Point to the next entry */` |
|   3350465 | 1767 | `		pEntry = pNext;` |
|   3350465 | 1768 | `		n++;` |
|         5 | 1769 | `	}` |
|    102203 | 1770 | `	if( pMap->nEntry > 0 ){` |
|         - | 1771 | `		/* Release the hash bucket */` |
|     77121 | 1772 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     38558 | 1773 | `	}` |
|    102203 | 1774 | `	if( FreeDS ){` |
|         - | 1775 | `		/* Free the whole instance */` |
|    102177 | 1776 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     51091 | 1777 | `	}else{` |
|         - | 1778 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1779 | `		pMap->apBucket = 0;` |
|        28 | 1780 | `		pMap->iNextIdx = 0;` |
|        28 | 1781 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1782 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1783 | `	}` |
|    102203 | 1784 | `	return SXRET_OK;` |
|     51104 | 1785 | `}` |
|         - | 1786 | `/*` |
|         - | 1787 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1788 | ` * If the count reaches zero which mean no more variables` |
|         - | 1789 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1790 | ` */` |
|    846992 | 1791 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1792 | `{` |
|    846997 | 1793 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1794 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    846997 | 1795 | `	pMap->iRef--;` |
|    846997 | 1796 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    102157 | 1797 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     51076 | 1798 | `	}` |
|    846997 | 1799 | `}` |
|         - | 1800 | `/*` |
|         - | 1801 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1802 | ` * Write a pointer to the target node on success.` |
|         - | 1803 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1804 | ` */` |
|    141266 | 1805 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1806 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1807 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1808 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1809 | `	)` |
|         5 | 1810 | `{` |
|         - | 1811 | `	sxi32 rc;` |
|    141271 | 1812 | `	if( pMap->nEntry < 1 ){` |
|         - | 1813 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1814 | `		 */` |
|       177 | 1815 | `		return SXERR_NOTFOUND;` |
|         - | 1816 | `	}` |
|    141099 | 1817 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    141099 | 1818 | `	return rc;` |
|     70638 | 1819 | `}` |
|         - | 1820 | `/*` |
|         - | 1821 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1822 | ` * hashmap.` |
|         - | 1823 | ` * If a node with the given key already exists in the database` |
|         - | 1824 | ` * then this function overwrite the old value.` |
|         - | 1825 | ` */` |
|   2731266 | 1826 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   2731271 | 1837 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2731271 | 1838 | `	return rc;` |
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
|     48326 | 1877 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1878 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1879 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1880 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1881 | `	)` |
|         5 | 1882 | `{` |
|         - | 1883 | `	sxi32 rc;` |
|     48331 | 1884 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1885 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1886 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1887 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1888 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1889 | `		return PH7_ABORT;` |
|         - | 1890 | `	}` |
|     48331 | 1891 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     48331 | 1892 | `	return rc;` |
|     24168 | 1893 | `}` |
|         - | 1894 | `/*` |
|         - | 1895 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1896 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1897 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1898 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1899 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1900 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1901 | ` */` |
|     19020 | 1902 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1903 | `{` |
|     19025 | 1904 | `	pStep->pCursor = pMap->pFirst;` |
|     19025 | 1905 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     19025 | 1906 | `	pMap->pActiveSteps = pStep;` |
|     19025 | 1907 | `}` |
|         - | 1908 | `/*` |
|         - | 1909 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1910 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1911 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1912 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1913 | ` */` |
|     18920 | 1914 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1915 | `{` |
|     18925 | 1916 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     18925 | 1917 | `	while( *ppLink ){` |
|     18925 | 1918 | `		if( *ppLink == pStep ){` |
|     18925 | 1919 | `			*ppLink = pStep->pNextActive;` |
|     18925 | 1920 | `			pStep->pNextActive = 0;` |
|     18925 | 1921 | `			return;` |
|         - | 1922 | `		}` |
|       ! 0 | 1923 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1924 | `	}` |
|      9465 | 1925 | `}` |
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
|    592710 | 1946 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1947 | `{` |
|    592715 | 1948 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    592715 | 1949 | `	if( pEntry ){` |
|    592715 | 1950 | `		if( bStore ){` |
|    235241 | 1951 | `			PH7_MemObjStore(pEntry,pValue);` |
|    117623 | 1952 | `		}else{` |
|    357479 | 1953 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1954 | `		}` |
|    296266 | 1955 | `	}else{` |
|       ! 0 | 1956 | `		PH7_MemObjRelease(pValue);` |
|         - | 1957 | `	}` |
|    592715 | 1958 | `}` |
|         - | 1959 | `/*` |
|         - | 1960 | ` * Extract a node key.` |
|         - | 1961 | ` */` |
|    156442 | 1962 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 1963 | `{` |
|         - | 1964 | `	/* Fill with the current key */` |
|    156447 | 1965 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    151131 | 1966 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 1967 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 1968 | `		}` |
|    151131 | 1969 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    151131 | 1970 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     75568 | 1971 | `	}else{` |
|      5321 | 1972 | `		SyBlobReset(&pKey->sBlob);` |
|      5321 | 1973 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5321 | 1974 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 1975 | `	}` |
|    156447 | 1976 | `}` |
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
|     35942 | 2027 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2028 | `{` |
|         - | 2029 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2030 | `    /* Prevent compiler warning */` |
|     35947 | 2031 | `	result.pNext = result.pPrev = 0;` |
|     35947 | 2032 | `	pTail = &result;` |
|    107965 | 2033 | `	while( pA && pB ){` |
|     72023 | 2034 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47627 | 2035 | `			pTail->pPrev = pA;` |
|     47627 | 2036 | `			pA->pNext = pTail;` |
|     47627 | 2037 | `			pTail = pA;` |
|     47627 | 2038 | `			pA = pA->pPrev;` |
|     23792 | 2039 | `		}else{` |
|     24401 | 2040 | `			pTail->pPrev = pB;` |
|     24401 | 2041 | `			pB->pNext = pTail;` |
|     24401 | 2042 | `			pTail = pB;` |
|     24401 | 2043 | `			pB = pB->pPrev;` |
|         - | 2044 | `		}` |
|         5 | 2045 | `	}` |
|     35947 | 2046 | `	if( pA ){` |
|     25390 | 2047 | `		pTail->pPrev = pA;` |
|     25390 | 2048 | `		pA->pNext = pTail;` |
|     23269 | 2049 | `	}else if( pB ){` |
|     10340 | 2050 | `		pTail->pPrev = pB;` |
|     10340 | 2051 | `		pB->pNext = pTail;` |
|      5158 | 2052 | `	}else{` |
|       227 | 2053 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2054 | `	}` |
|     35947 | 2055 | `	return result.pPrev;` |
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
|       750 | 2069 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2070 | `{` |
|         - | 2071 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2072 | `	sxu32 i;` |
|       755 | 2073 | `	SyZero(a,sizeof(a));` |
|         - | 2074 | `	/* Point to the first inserted entry */` |
|       755 | 2075 | `	pIn = pMap->pFirst;` |
|     14767 | 2076 | `	while( pIn ){` |
|     14017 | 2077 | `		p = pIn;` |
|     14017 | 2078 | `		pIn = p->pPrev;` |
|     14017 | 2079 | `		p->pPrev = 0;` |
|     26709 | 2080 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26709 | 2081 | `			if( a[i]==0 ){` |
|     14017 | 2082 | `				a[i] = p;` |
|     14017 | 2083 | `				break;` |
|       ! 0 | 2084 | `			}else{` |
|     12697 | 2085 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12697 | 2086 | `				a[i] = 0;` |
|         - | 2087 | `			}` |
|      6351 | 2088 | `		}` |
|     14017 | 2089 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2090 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2091 | `			 * But that is impossible.` |
|         - | 2092 | `			 */` |
|       ! 0 | 2093 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2094 | `		}` |
|         5 | 2095 | `	}` |
|       755 | 2096 | `	p = a[0];` |
|     24005 | 2097 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     23255 | 2098 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     11630 | 2099 | `	}` |
|       755 | 2100 | `	p->pNext = 0;` |
|         - | 2101 | `	/* Reflect the change */` |
|       755 | 2102 | `	pMap->pFirst = p;` |
|         - | 2103 | `	/* Reset the loop cursor */` |
|       755 | 2104 | `	pMap->pCur = pMap->pFirst;` |
|       755 | 2105 | `	return SXRET_OK;` |
|         5 | 2106 | `}` |
|         - | 2107 | `/* SPDX-SnippetEnd */` |
|         - | 2108 | `/*` |
|         - | 2109 | ` * Node comparison callback.` |
|         - | 2110 | ` * used-by: [sort(),asort(),...]` |
|         - | 2111 | ` */` |
|     71746 | 2112 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2113 | `{` |
|         - | 2114 | `	ph7_value sA,sB;` |
|         - | 2115 | `	sxi32 iFlags;` |
|         - | 2116 | `	int rc;` |
|     71751 | 2117 | `	if( pCmpData == 0 ){` |
|         - | 2118 | `		/* Perform a standard comparison */` |
|     71727 | 2119 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     71727 | 2120 | `		return rc;` |
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
|     35831 | 2158 | `}` |
|         - | 2159 | `/*` |
|         - | 2160 | ` * Shared key comparison for ksort()/krsort(): php 8 semantics. Two string` |
|         - | 2161 | ` * keys compare bytewise. Mixed int/string keys: a NUMERIC string compares` |
|         - | 2162 | ` * numerically with the int key; a non-numeric one makes the int key compare` |
|         - | 2163 | ` * AS A STRING ("5" < "b", so int keys land before alphabetic ones — pre-fix` |
|         - | 2164 | ` * PHL cast "b" to 0 and sorted string keys first).` |
|         - | 2165 | ` */` |
|         - | 2166 | `/* True lexicographic compare (memcmp on the common prefix, length breaks` |
|         - | 2167 | ` * ties) — SyBlobCmp compares LENGTH first, which is fine for equality but` |
|         - | 2168 | ` * wrong for ordering ("c" would sort before "a.y"). */` |
|        36 | 2169 | `static sxi32 HashmapLexCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|         2 | 2170 | `{` |
|        38 | 2171 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|        38 | 2172 | `	sxi32 rc = nMin ? SyMemcmp(zA,zB,nMin) : 0;` |
|        38 | 2173 | `	if( rc == 0 ){` |
|       ! 0 | 2174 | `		rc = (sxi32)nA - (sxi32)nB;` |
|       ! 0 | 2175 | `	}` |
|        38 | 2176 | `	return rc;` |
|         2 | 2177 | `}` |
|        58 | 2178 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|         2 | 2179 | `{` |
|         - | 2180 | `	sxi32 rc;` |
|        60 | 2181 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2182 | `		/* Perform a string comparison */` |
|        32 | 2183 | `		rc = HashmapLexCmp((const char *)SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey),` |
|        20 | 2184 | `			(const char *)SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|        12 | 2185 | `	}else{` |
|         - | 2186 | `		SyString sStr;` |
|        39 | 2187 | `		sxi64 iA = 0,iB = 0;` |
|        39 | 2188 | `		int bNum = 1;` |
|        39 | 2189 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|        11 | 2190 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|        11 | 2191 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|        11 | 2192 | `				bNum = 0;` |
|         6 | 2193 | `			}else{` |
|       ! 0 | 2194 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2195 | `			}` |
|         6 | 2196 | `		}else{` |
|        29 | 2197 | `			iA = pA->xKey.iKey;` |
|         - | 2198 | `		}` |
|        39 | 2199 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         7 | 2200 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         7 | 2201 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|         7 | 2202 | `				bNum = 0;` |
|         4 | 2203 | `			}else{` |
|       ! 0 | 2204 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2205 | `			}` |
|         4 | 2206 | `		}else{` |
|        33 | 2207 | `			iB = pB->xKey.iKey;` |
|         - | 2208 | `		}` |
|        39 | 2209 | `		if( bNum ){` |
|        23 | 2210 | `			rc = iA < iB ? -1 : (iA > iB ? 1 : 0);` |
|        12 | 2211 | `		}else{` |
|         - | 2212 | `			/* Render the int key and compare bytewise like php */` |
|         - | 2213 | `			char zNumA[24],zNumB[24];` |
|         - | 2214 | `			SyString sA,sB;` |
|        17 | 2215 | `			if( pA->iType != HASHMAP_BLOB_NODE ){` |
|         7 | 2216 | `				sxu32 n = SyBufferFormat(zNumA,sizeof(zNumA),"%qd",pA->xKey.iKey);` |
|         7 | 2217 | `				SyStringInitFromBuf(&sA,zNumA,n);` |
|         4 | 2218 | `			}else{` |
|        11 | 2219 | `				SyStringInitFromBuf(&sA,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|         - | 2220 | `			}` |
|        17 | 2221 | `			if( pB->iType != HASHMAP_BLOB_NODE ){` |
|        11 | 2222 | `				sxu32 n = SyBufferFormat(zNumB,sizeof(zNumB),"%qd",pB->xKey.iKey);` |
|        11 | 2223 | `				SyStringInitFromBuf(&sB,zNumB,n);` |
|         6 | 2224 | `			}else{` |
|         7 | 2225 | `				SyStringInitFromBuf(&sB,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         - | 2226 | `			}` |
|        17 | 2227 | `			rc = HashmapLexCmp(sA.zString,sA.nByte,sB.zString,sB.nByte);` |
|         - | 2228 | `		}` |
|         - | 2229 | `	}` |
|        60 | 2230 | `	return rc;` |
|         2 | 2231 | `}` |
|         - | 2232 | `/*` |
|         - | 2233 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2234 | ` * used-by: [ksort()]` |
|         - | 2235 | ` */` |
|        44 | 2236 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2237 | `{` |
|        22 | 2238 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        46 | 2239 | `	return HashmapKeyNodeCmp(pA,pB);` |
|         2 | 2240 | `}` |
|         - | 2241 | `/*` |
|         - | 2242 | ` * Node comparison callback.` |
|         - | 2243 | ` * Used by: [rsort(),arsort()];` |
|         - | 2244 | ` */` |
|        78 | 2245 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2246 | `{` |
|         - | 2247 | `	ph7_value sA,sB;` |
|         - | 2248 | `	sxi32 iFlags;` |
|         - | 2249 | `	int rc;` |
|        79 | 2250 | `	if( pCmpData == 0 ){` |
|         - | 2251 | `		/* Perform a standard comparison */` |
|        59 | 2252 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|        59 | 2253 | `		return -rc;` |
|         - | 2254 | `	}` |
|        21 | 2255 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2256 | `	/* Duplicate node values */` |
|        21 | 2257 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        21 | 2258 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        21 | 2259 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        21 | 2260 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        21 | 2261 | `	if( iFlags == 5 ){` |
|         - | 2262 | `		/* String cast */` |
|         - | 2263 | `		const char *zA,*zB;` |
|         - | 2264 | `		sxu32 nA,nB,nMin;` |
|        11 | 2265 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2266 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2267 | `		}` |
|        11 | 2268 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2269 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2270 | `		}` |
|         - | 2271 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        11 | 2272 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        11 | 2273 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        11 | 2274 | `		nA = SyBlobLength(&sA.sBlob);` |
|        11 | 2275 | `		nB = SyBlobLength(&sB.sBlob);` |
|        11 | 2276 | `		nMin = nA < nB ? nA : nB;` |
|        11 | 2277 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        11 | 2278 | `		if( rc == 0 ){` |
|         3 | 2279 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2280 | `			else if( nA > nB ) rc = 1;` |
|         1 | 2281 | `		}` |
|         6 | 2282 | `	}else{` |
|         - | 2283 | `		/* Numeric cast */` |
|        11 | 2284 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2285 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2286 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2287 | `	}` |
|        21 | 2288 | `	PH7_MemObjRelease(&sA);` |
|        21 | 2289 | `	PH7_MemObjRelease(&sB);` |
|        21 | 2290 | `	return -rc;` |
|        40 | 2291 | `}` |
|         - | 2292 | `/*` |
|         - | 2293 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2294 | ` * used-by: [usort(),uasort()]` |
|         - | 2295 | ` */` |
|       110 | 2296 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2297 | `{` |
|         - | 2298 | `	ph7_value sResult,*pCallback;` |
|         - | 2299 | `	ph7_value *pV1,*pV2;` |
|         - | 2300 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2301 | `	sxi32 rc;` |
|         - | 2302 | `	/* Point to the desired callback */` |
|       112 | 2303 | `	pCallback = (ph7_value *)pCmpData;` |
|       112 | 2304 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2305 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2306 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2307 | `		return 0;` |
|         - | 2308 | `	}` |
|         - | 2309 | `	/* initialize the result value */` |
|       106 | 2310 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2311 | `	/* Extract nodes values */` |
|       106 | 2312 | `	pV1 = HashmapExtractNodeValue(pA);` |
|       106 | 2313 | `	pV2 = HashmapExtractNodeValue(pB);` |
|       106 | 2314 | `	apArg[0] = pV1;` |
|       106 | 2315 | `	apArg[1] = pV2;` |
|         - | 2316 | `	/* Invoke the callback */` |
|       106 | 2317 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       106 | 2318 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2319 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2320 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2321 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2322 | `		rc = 0;` |
|       102 | 2323 | `	}else if( rc != SXRET_OK ){` |
|         - | 2324 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2325 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2326 | `	}else{` |
|         - | 2327 | `		/* Extract callback result */` |
|        98 | 2328 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2329 | `			/* Perform an int cast */` |
|       ! 0 | 2330 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2331 | `		}` |
|        98 | 2332 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2333 | `	}` |
|       106 | 2334 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2335 | `	/* Callback result */` |
|       106 | 2336 | `	return rc;` |
|        57 | 2337 | `}` |
|         - | 2338 | `/*` |
|         - | 2339 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2340 | ` * used-by: [krsort()]` |
|         - | 2341 | ` */` |
|        14 | 2342 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2343 | `{` |
|         7 | 2344 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        15 | 2345 | `	return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|         1 | 2346 | `}` |
|         - | 2347 | `/*` |
|         - | 2348 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2349 | ` * used-by: [uksort()]` |
|         - | 2350 | ` */` |
|         6 | 2351 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2352 | `{` |
|         - | 2353 | `	ph7_value sResult,*pCallback;` |
|         - | 2354 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2355 | `	ph7_value sK1,sK2;` |
|         - | 2356 | `	sxi32 rc;` |
|         - | 2357 | `	/* Point to the desired callback */` |
|         7 | 2358 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2359 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2360 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2361 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2362 | `		return 0;` |
|         - | 2363 | `	}` |
|         - | 2364 | `	/* initialize the result value */` |
|         7 | 2365 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2366 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2367 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2368 | `	/* Extract nodes keys */` |
|         7 | 2369 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2370 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2371 | `	apArg[0] = &sK1;` |
|         7 | 2372 | `	apArg[1] = &sK2;` |
|         - | 2373 | `	/* Mark keys as constants */` |
|         7 | 2374 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2375 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2376 | `	/* Invoke the callback */` |
|         7 | 2377 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2378 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2379 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2380 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2381 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2382 | `		rc = 0;` |
|         7 | 2383 | `	}else if( rc != SXRET_OK ){` |
|         - | 2384 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2385 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2386 | `	}else{` |
|         - | 2387 | `		/* Extract callback result */` |
|         7 | 2388 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2389 | `			/* Perform an int cast */` |
|       ! 0 | 2390 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2391 | `		}` |
|         7 | 2392 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2393 | `	}` |
|         7 | 2394 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2395 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2396 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2397 | `	/* Callback result */` |
|         7 | 2398 | `	return rc;` |
|         4 | 2399 | `}` |
|         - | 2400 | `/*` |
|         - | 2401 | ` * Node comparison callback: Random node comparison.` |
|         - | 2402 | ` * used-by: [shuffle()]` |
|         - | 2403 | ` */` |
|        20 | 2404 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2405 | `{` |
|         - | 2406 | `	sxu32 n;` |
|        11 | 2407 | `	SXUNUSED(pB); /* cc warning */` |
|        11 | 2408 | `	SXUNUSED(pCmpData);` |
|         - | 2409 | `	/* Grab a random number */` |
|        21 | 2410 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2411 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2412 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2413 | `	 */` |
|        21 | 2414 | `	return n&1 ? 1 : -1;` |
|         1 | 2415 | `}` |
|         - | 2416 | `/*` |
|         - | 2417 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2418 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2419 | ` */` |
|       680 | 2420 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2421 | `{` |
|         - | 2422 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2423 | `	sxu32 i;` |
|         - | 2424 | `	/* Rehash all entries */` |
|       685 | 2425 | `	pLast = p = pMap->pFirst;` |
|       685 | 2426 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|       685 | 2427 | `	i = 0;` |
|      7228 | 2428 | `	for( ;; ){` |
|     14461 | 2429 | `		if( i >= pMap->nEntry ){` |
|       685 | 2430 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       685 | 2431 | `			break;` |
|         - | 2432 | `		}` |
|     13781 | 2433 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2434 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2435 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2436 | `			/* Change key type */` |
|         5 | 2437 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2438 | `		}` |
|     13781 | 2439 | `		HashmapRehashIntNode(p);` |
|         - | 2440 | `		/* Point to the next entry */` |
|     13781 | 2441 | `		i++;` |
|     13781 | 2442 | `		pLast = p;` |
|     13781 | 2443 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2444 | `	}` |
|       685 | 2445 | `}` |
|         - | 2446 | `/*` |
|         - | 2447 | ` * Array functions implementation.` |
|         - | 2448 | ` * Status:` |
|         - | 2449 | ` *  Stable.` |
|         - | 2450 | ` */` |
|         - | 2451 | `/*` |
|         - | 2452 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2453 | ` * Sort an array.` |
|         - | 2454 | ` * Parameters` |
|         - | 2455 | ` *  $array` |
|         - | 2456 | ` *   The input array.` |
|         - | 2457 | ` * $sort_flags` |
|         - | 2458 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2459 | ` *  Sorting type flags:` |
|         - | 2460 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2461 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2462 | ` *   SORT_STRING - compare items as strings` |
|         - | 2463 | ` * Return` |
|         - | 2464 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2465 | ` *` |
|         - | 2466 | ` */` |
|      1016 | 2467 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2468 | `{` |
|         - | 2469 | `	ph7_hashmap *pMap;` |
|         - | 2470 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1021 | 2471 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2472 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2473 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2474 | `		return PH7_OK;` |
|         - | 2475 | `	}` |
|         - | 2476 | `	/* Point to the internal representation of the input hashmap */` |
|      1021 | 2477 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1021 | 2478 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1021 | 2479 | `	if( pMap->nEntry > 1 ){` |
|       661 | 2480 | `		sxi32 iCmpFlags = 0;` |
|       661 | 2481 | `		if( nArg > 1 ){` |
|         - | 2482 | `			/* Extract comparison flags */` |
|         3 | 2483 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2484 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2485 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2486 | `			}` |
|         1 | 2487 | `		}` |
|         - | 2488 | `		/* Do the merge sort */` |
|       661 | 2489 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2490 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       661 | 2491 | `		HashmapSortRehash(pMap);` |
|       328 | 2492 | `	}` |
|         - | 2493 | `	/* All done,return TRUE */` |
|      1021 | 2494 | `	ph7_result_bool(pCtx,1);` |
|      1021 | 2495 | `	return PH7_OK;` |
|       513 | 2496 | `}` |
|         - | 2497 | `/*` |
|         - | 2498 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2499 | ` *  Sort an array and maintain index association.` |
|         - | 2500 | ` * Parameters` |
|         - | 2501 | ` *  $array` |
|         - | 2502 | ` *   The input array.` |
|         - | 2503 | ` * $sort_flags` |
|         - | 2504 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2505 | ` *  Sorting type flags:` |
|         - | 2506 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2507 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2508 | ` *   SORT_STRING - compare items as strings` |
|         - | 2509 | ` * Return` |
|         - | 2510 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2511 | ` */` |
|        34 | 2512 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2513 | `{` |
|         - | 2514 | `	ph7_hashmap *pMap;` |
|         - | 2515 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        39 | 2516 | `	if( nArg < 1 ){` |
|         3 | 2517 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2518 | `			"ArgumentCountError",` |
|         - | 2519 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2520 | `			);` |
|         - | 2521 | `	}` |
|         - | 2522 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        37 | 2523 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2524 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2525 | `			"TypeError",` |
|         - | 2526 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2527 | `			ph7_type_name(apArg[0])` |
|         - | 2528 | `			);` |
|         - | 2529 | `	}` |
|         - | 2530 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 2531 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        25 | 2532 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        25 | 2533 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2534 | `		sxi32 iCmpFlags = 0;` |
|        21 | 2535 | `		if( nArg > 1 ){` |
|         - | 2536 | `			/* Extract comparison flags */` |
|         5 | 2537 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2538 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2539 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2540 | `			}` |
|         2 | 2541 | `		}` |
|         - | 2542 | `		/* Do the merge sort */` |
|        21 | 2543 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2544 | `		/* Fix the last link broken by the merge */` |
|        49 | 2545 | `		while(pMap->pLast->pPrev){` |
|        29 | 2546 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2547 | `		}` |
|        10 | 2548 | `	}` |
|         - | 2549 | `	/* All done,return TRUE */` |
|        25 | 2550 | `	ph7_result_bool(pCtx,1);` |
|        25 | 2551 | `	return PH7_OK;` |
|        22 | 2552 | `}` |
|         - | 2553 | `/*` |
|         - | 2554 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2555 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2556 | ` * Parameters` |
|         - | 2557 | ` *  $array` |
|         - | 2558 | ` *   The input array.` |
|         - | 2559 | ` * $sort_flags` |
|         - | 2560 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2561 | ` *  Sorting type flags:` |
|         - | 2562 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2563 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2564 | ` *   SORT_STRING - compare items as strings` |
|         - | 2565 | ` * Return` |
|         - | 2566 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2567 | ` */` |
|        32 | 2568 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2569 | `{` |
|         - | 2570 | `	ph7_hashmap *pMap;` |
|         - | 2571 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2572 | `	if( nArg < 1 ){` |
|         3 | 2573 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2574 | `			"ArgumentCountError",` |
|         - | 2575 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2576 | `			);` |
|         - | 2577 | `	}` |
|         - | 2578 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2579 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2580 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2581 | `			"TypeError",` |
|         - | 2582 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2583 | `			ph7_type_name(apArg[0])` |
|         - | 2584 | `			);` |
|         - | 2585 | `	}` |
|         - | 2586 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2587 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2588 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2589 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2590 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2591 | `		if( nArg > 1 ){` |
|         - | 2592 | `			/* Extract comparison flags */` |
|         5 | 2593 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2594 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2595 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2596 | `			}` |
|         2 | 2597 | `		}` |
|         - | 2598 | `		/* Do the merge sort */` |
|        19 | 2599 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2600 | `		/* Fix the last link broken by the merge */` |
|        35 | 2601 | `		while(pMap->pLast->pPrev){` |
|        17 | 2602 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2603 | `		}` |
|         9 | 2604 | `	}` |
|         - | 2605 | `	/* All done,return TRUE */` |
|        23 | 2606 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2607 | `	return PH7_OK;` |
|        21 | 2608 | `}` |
|         - | 2609 | `/*` |
|         - | 2610 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2611 | ` *  Sort an array by key.` |
|         - | 2612 | ` * Parameters` |
|         - | 2613 | ` *  $array` |
|         - | 2614 | ` *   The input array.` |
|         - | 2615 | ` * $sort_flags` |
|         - | 2616 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2617 | ` *  Sorting type flags:` |
|         - | 2618 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2619 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2620 | ` *   SORT_STRING - compare items as strings` |
|         - | 2621 | ` * Return` |
|         - | 2622 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2623 | ` */` |
|        14 | 2624 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2625 | `{` |
|         - | 2626 | `	ph7_hashmap *pMap;` |
|         - | 2627 | `	/* Make sure we are dealing with a valid hashmap */` |
|        16 | 2628 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2629 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2630 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2631 | `		return PH7_OK;` |
|         - | 2632 | `	}` |
|         - | 2633 | `	/* Point to the internal representation of the input hashmap */` |
|        16 | 2634 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        16 | 2635 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        16 | 2636 | `	if( pMap->nEntry > 1 ){` |
|        16 | 2637 | `		sxi32 iCmpFlags = 0;` |
|        16 | 2638 | `		if( nArg > 1 ){` |
|         - | 2639 | `			/* Extract comparison flags */` |
|       ! 0 | 2640 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2641 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2642 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2643 | `			}` |
|       ! 0 | 2644 | `		}` |
|         - | 2645 | `		/* Do the merge sort */` |
|        16 | 2646 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2647 | `		/* Fix the last link broken by the merge */` |
|        38 | 2648 | `		while(pMap->pLast->pPrev){` |
|        23 | 2649 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2650 | `		}` |
|         7 | 2651 | `	}` |
|         - | 2652 | `	/* All done,return TRUE */` |
|        16 | 2653 | `	ph7_result_bool(pCtx,1);` |
|        16 | 2654 | `	return PH7_OK;` |
|         9 | 2655 | `}` |
|         - | 2656 | `/*` |
|         - | 2657 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2658 | ` *  Sort an array by key in reverse order.` |
|         - | 2659 | ` * Parameters` |
|         - | 2660 | ` *  $array` |
|         - | 2661 | ` *   The input array.` |
|         - | 2662 | ` * $sort_flags` |
|         - | 2663 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2664 | ` *  Sorting type flags:` |
|         - | 2665 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2666 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2667 | ` *   SORT_STRING - compare items as strings` |
|         - | 2668 | ` * Return` |
|         - | 2669 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2670 | ` */` |
|         4 | 2671 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2672 | `{` |
|         - | 2673 | `	ph7_hashmap *pMap;` |
|         - | 2674 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2675 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2676 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2677 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2678 | `		return PH7_OK;` |
|         - | 2679 | `	}` |
|         - | 2680 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 2681 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         5 | 2682 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 2683 | `	if( pMap->nEntry > 1 ){` |
|         5 | 2684 | `		sxi32 iCmpFlags = 0;` |
|         5 | 2685 | `		if( nArg > 1 ){` |
|         - | 2686 | `			/* Extract comparison flags */` |
|       ! 0 | 2687 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2688 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2689 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2690 | `			}` |
|       ! 0 | 2691 | `		}` |
|         - | 2692 | `		/* Do the merge sort */` |
|         5 | 2693 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2694 | `		/* Fix the last link broken by the merge */` |
|        17 | 2695 | `		while(pMap->pLast->pPrev){` |
|        13 | 2696 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2697 | `		}` |
|         2 | 2698 | `	}` |
|         - | 2699 | `	/* All done,return TRUE */` |
|         5 | 2700 | `	ph7_result_bool(pCtx,1);` |
|         5 | 2701 | `	return PH7_OK;` |
|         3 | 2702 | `}` |
|         - | 2703 | `/*` |
|         - | 2704 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2705 | ` * Sort an array in reverse order.` |
|         - | 2706 | ` * Parameters` |
|         - | 2707 | ` *  $array` |
|         - | 2708 | ` *   The input array.` |
|         - | 2709 | ` * $sort_flags` |
|         - | 2710 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2711 | ` *  Sorting type flags:` |
|         - | 2712 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2713 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2714 | ` *   SORT_STRING - compare items as strings` |
|         - | 2715 | ` * Return` |
|         - | 2716 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2717 | ` */` |
|         2 | 2718 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2719 | `{` |
|         - | 2720 | `	ph7_hashmap *pMap;` |
|         - | 2721 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2722 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2723 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2724 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2725 | `		return PH7_OK;` |
|         - | 2726 | `	}` |
|         - | 2727 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2728 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2729 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2730 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2731 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2732 | `		if( nArg > 1 ){` |
|         - | 2733 | `			/* Extract comparison flags */` |
|       ! 0 | 2734 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2735 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2736 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2737 | `			}` |
|       ! 0 | 2738 | `		}` |
|         - | 2739 | `		/* Do the merge sort */` |
|         3 | 2740 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2741 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         3 | 2742 | `		HashmapSortRehash(pMap);` |
|         1 | 2743 | `	}` |
|         - | 2744 | `	/* All done,return TRUE */` |
|         3 | 2745 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2746 | `	return PH7_OK;` |
|         2 | 2747 | `}` |
|         - | 2748 | `/*` |
|         - | 2749 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2750 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2751 | ` * Parameters` |
|         - | 2752 | ` *  $array` |
|         - | 2753 | ` *   The input array.` |
|         - | 2754 | ` * $cmp_function` |
|         - | 2755 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2756 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2757 | ` *  to, or greater than the second.` |
|         - | 2758 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2759 | ` * Return` |
|         - | 2760 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2761 | ` */` |
|        18 | 2762 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2763 | `{` |
|         - | 2764 | `	ph7_hashmap *pMap;` |
|         - | 2765 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 2766 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2767 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2768 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2769 | `		return PH7_OK;` |
|         - | 2770 | `	}` |
|         - | 2771 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 2772 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        20 | 2773 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 2774 | `	if( pMap->nEntry > 1 ){` |
|        20 | 2775 | `		ph7_value *pCallback = 0;` |
|         - | 2776 | `		ProcNodeCmp xCmp;` |
|        20 | 2777 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        20 | 2778 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2779 | `			/* Point to the desired callback */` |
|        20 | 2780 | `			pCallback = apArg[1];` |
|        11 | 2781 | `		}else{` |
|         - | 2782 | `			/* Use the default comparison function */` |
|       ! 0 | 2783 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2784 | `		}` |
|         - | 2785 | `		/* Do the merge sort */` |
|        20 | 2786 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        20 | 2787 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2788 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        20 | 2789 | `		HashmapSortRehash(pMap);` |
|        20 | 2790 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2791 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2792 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2793 | `			return PH7_EXCEPTION;` |
|         - | 2794 | `		}` |
|         5 | 2795 | `	}` |
|         - | 2796 | `	/* All done,return TRUE */` |
|        12 | 2797 | `	ph7_result_bool(pCtx,1);` |
|        12 | 2798 | `	return PH7_OK;` |
|        11 | 2799 | `}` |
|         - | 2800 | `/*` |
|         - | 2801 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2802 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2803 | ` *  and maintain index association.` |
|         - | 2804 | ` * Parameters` |
|         - | 2805 | ` *  $array` |
|         - | 2806 | ` *   The input array.` |
|         - | 2807 | ` * $cmp_function` |
|         - | 2808 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2809 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2810 | ` *  to, or greater than the second.` |
|         - | 2811 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2812 | ` * Return` |
|         - | 2813 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2814 | ` */` |
|        10 | 2815 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2816 | `{` |
|         - | 2817 | `	ph7_hashmap *pMap;` |
|         - | 2818 | `	/* Make sure we are dealing with a valid hashmap */` |
|        11 | 2819 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2820 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2821 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2822 | `		return PH7_OK;` |
|         - | 2823 | `	}` |
|         - | 2824 | `	/* Point to the internal representation of the input hashmap */` |
|        11 | 2825 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        11 | 2826 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        11 | 2827 | `	if( pMap->nEntry > 1 ){` |
|        11 | 2828 | `		ph7_value *pCallback = 0;` |
|         - | 2829 | `		ProcNodeCmp xCmp;` |
|        11 | 2830 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        11 | 2831 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2832 | `			/* Point to the desired callback */` |
|        11 | 2833 | `			pCallback = apArg[1];` |
|         6 | 2834 | `		}else{` |
|         - | 2835 | `			/* Use the default comparison function */` |
|       ! 0 | 2836 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2837 | `		}` |
|         - | 2838 | `		/* Do the merge sort */` |
|        11 | 2839 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        11 | 2840 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2841 | `		/* Fix the last link broken by the merge */` |
|        23 | 2842 | `		while(pMap->pLast->pPrev){` |
|        13 | 2843 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2844 | `		}` |
|        11 | 2845 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2846 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2847 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2848 | `			return PH7_EXCEPTION;` |
|         - | 2849 | `		}` |
|         5 | 2850 | `	}` |
|         - | 2851 | `	/* All done,return TRUE */` |
|        11 | 2852 | `	ph7_result_bool(pCtx,1);` |
|        11 | 2853 | `	return PH7_OK;` |
|         6 | 2854 | `}` |
|         - | 2855 | `/*` |
|         - | 2856 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2857 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2858 | ` *  function and maintain index association.` |
|         - | 2859 | ` * Parameters` |
|         - | 2860 | ` *  $array` |
|         - | 2861 | ` *   The input array.` |
|         - | 2862 | ` * $cmp_function` |
|         - | 2863 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2864 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2865 | ` *  to, or greater than the second.` |
|         - | 2866 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2867 | ` * Return` |
|         - | 2868 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2869 | ` */` |
|         2 | 2870 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2871 | `{` |
|         - | 2872 | `	ph7_hashmap *pMap;` |
|         - | 2873 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2874 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2875 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2876 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2877 | `		return PH7_OK;` |
|         - | 2878 | `	}` |
|         - | 2879 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2880 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2881 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2882 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2883 | `		ph7_value *pCallback = 0;` |
|         - | 2884 | `		ProcNodeCmp xCmp;` |
|         3 | 2885 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2886 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2887 | `			/* Point to the desired callback */` |
|         3 | 2888 | `			pCallback = apArg[1];` |
|         2 | 2889 | `		}else{` |
|         - | 2890 | `			/* Use the default comparison function */` |
|       ! 0 | 2891 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2892 | `		}` |
|         - | 2893 | `		/* Do the merge sort */` |
|         3 | 2894 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2895 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2896 | `		/* Fix the last link broken by the merge */` |
|         3 | 2897 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2898 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2899 | `		}` |
|         3 | 2900 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2901 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2902 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2903 | `			return PH7_EXCEPTION;` |
|         - | 2904 | `		}` |
|         1 | 2905 | `	}` |
|         - | 2906 | `	/* All done,return TRUE */` |
|         3 | 2907 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2908 | `	return PH7_OK;` |
|         2 | 2909 | `}` |
|         - | 2910 | `/*` |
|         - | 2911 | ` * bool shuffle(array &$array)` |
|         - | 2912 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2913 | ` * Parameters` |
|         - | 2914 | ` *  $array` |
|         - | 2915 | ` *   The input array.` |
|         - | 2916 | ` * Return` |
|         - | 2917 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2918 | ` *` |
|         - | 2919 | ` */` |
|         2 | 2920 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2921 | `{` |
|         - | 2922 | `	ph7_hashmap *pMap;` |
|         - | 2923 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2924 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2925 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2926 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2927 | `		return PH7_OK;` |
|         - | 2928 | `	}` |
|         - | 2929 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2930 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2931 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2932 | `	if( pMap->nEntry > 1 ){` |
|         - | 2933 | `		/* Do the merge sort */` |
|         3 | 2934 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 2935 | `		/* Fix the last link broken by the merge */` |
|        10 | 2936 | `		while(pMap->pLast->pPrev){` |
|         8 | 2937 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2938 | `		}` |
|         1 | 2939 | `	}` |
|         - | 2940 | `	/* All done,return TRUE */` |
|         3 | 2941 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2942 | `	return PH7_OK;` |
|         2 | 2943 | `}` |
|         - | 2944 | `/*` |
|         - | 2945 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 2946 | ` *   Count all elements in an array, or something in an object.` |
|         - | 2947 | ` * Parameters` |
|         - | 2948 | ` *  $var` |
|         - | 2949 | ` *   The array or the object.` |
|         - | 2950 | ` * $mode` |
|         - | 2951 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 2952 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 2953 | ` *  all the elements of a multidimensional array.` |
|         - | 2954 | ` * Return` |
|         - | 2955 | ` *  Returns the number of elements in the array.` |
|         - | 2956 | ` */` |
|      1872 | 2957 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2958 | `{` |
|      1877 | 2959 | `	int bRecursive = FALSE;` |
|      1877 | 2960 | `	int bCycleDetected = FALSE;` |
|         - | 2961 | `	sxi64 iCount;` |
|      1877 | 2962 | `	if( nArg < 1 ){` |
|         3 | 2963 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2964 | `			"ArgumentCountError",` |
|         - | 2965 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2966 | `			);` |
|         - | 2967 | `	}` |
|      1875 | 2968 | `	if( nArg > 2 ){` |
|         4 | 2969 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2970 | `			"ArgumentCountError",` |
|         - | 2971 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 2972 | `			nArg` |
|         - | 2973 | `			);` |
|         - | 2974 | `	}` |
|         - | 2975 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 2976 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 2977 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1873 | 2978 | `	if( nArg > 1 ){` |
|        45 | 2979 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        45 | 2980 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        12 | 2981 | `			return PH7_VmThrowException(pCtx,` |
|         - | 2982 | `				"ValueError",` |
|         - | 2983 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 2984 | `				);` |
|         - | 2985 | `		}` |
|        34 | 2986 | `		bRecursive = iMode == 1;` |
|        16 | 2987 | `	}` |
|      1865 | 2988 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 2989 | `		/* Countable object: dispatch to ->count() */` |
|        73 | 2990 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        61 | 2991 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        61 | 2992 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        61 | 2993 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        59 | 2994 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 2995 | `					"count",sizeof("count")-1);` |
|        59 | 2996 | `				if( pMeth ){` |
|         - | 2997 | `					ph7_value sResult;` |
|        59 | 2998 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        59 | 2999 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        59 | 3000 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        59 | 3001 | `					PH7_MemObjRelease(&sResult);` |
|        59 | 3002 | `					return PH7_OK;` |
|         - | 3003 | `				}` |
|       ! 0 | 3004 | `			}` |
|         1 | 3005 | `		}` |
|        22 | 3006 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3007 | `			"TypeError",` |
|         - | 3008 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3009 | `			ph7_type_name(apArg[0])` |
|         - | 3010 | `			);` |
|         - | 3011 | `	}` |
|         - | 3012 | `	/* Count */` |
|      1797 | 3013 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1797 | 3014 | `	if( bCycleDetected ){` |
|         3 | 3015 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3016 | `	}` |
|      1797 | 3017 | `	ph7_result_int64(pCtx,iCount);` |
|      1797 | 3018 | `	return PH7_OK;` |
|       941 | 3019 | `}` |
|         - | 3020 | `/*` |
|         - | 3021 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3022 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3023 | ` * Parameters` |
|         - | 3024 | ` * $key` |
|         - | 3025 | ` *   Value to check.` |
|         - | 3026 | ` * $search` |
|         - | 3027 | ` *  An array with keys to check.` |
|         - | 3028 | ` * Return` |
|         - | 3029 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3030 | ` */` |
|        94 | 3031 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3032 | `{` |
|         - | 3033 | `	sxi32 rc;` |
|        99 | 3034 | `	if( nArg != 2 ){` |
|         - | 3035 | `		/* PHP requires exactly two arguments */` |
|        12 | 3036 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3037 | `			"ArgumentCountError",` |
|         - | 3038 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         3 | 3039 | `			nArg` |
|         - | 3040 | `			);` |
|         - | 3041 | `	}` |
|         - | 3042 | `	/* Make sure we are dealing with a valid hashmap */` |
|        93 | 3043 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3044 | `		/* Type mismatch -> TypeError */` |
|         8 | 3045 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3046 | `			"TypeError",` |
|         - | 3047 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3048 | `			ph7_type_name(apArg[1])` |
|         - | 3049 | `			);` |
|         - | 3050 | `	}` |
|         - | 3051 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        88 | 3052 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         3 | 3053 | `		ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3054 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3055 | `			"use an empty string instead"` |
|         - | 3056 | `			);` |
|        87 | 3057 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3058 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3059 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3060 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3061 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3062 | `				,rVal` |
|         - | 3063 | `				);` |
|         1 | 3064 | `		}` |
|         1 | 3065 | `	}` |
|         - | 3066 | `	/* Perform the lookup */` |
|        88 | 3067 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3068 | `	/* lookup result */` |
|        88 | 3069 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        88 | 3070 | `	return PH7_OK;` |
|        52 | 3071 | `}` |
|         - | 3072 | `/*` |
|         - | 3073 | ` * value array_pop(array $array)` |
|         - | 3074 | ` *   POP the last inserted element from the array.` |
|         - | 3075 | ` * Parameter` |
|         - | 3076 | ` *  The array to get the value from.` |
|         - | 3077 | ` * Return` |
|         - | 3078 | ` *  Poped value or NULL on failure.` |
|         - | 3079 | ` */` |
|       104 | 3080 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3081 | `{` |
|         - | 3082 | `	ph7_hashmap *pMap;` |
|         - | 3083 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|       109 | 3084 | `	if( nArg != 1 ){` |
|         8 | 3085 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3086 | `			"ArgumentCountError",` |
|         - | 3087 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         2 | 3088 | `			nArg` |
|         - | 3089 | `			);` |
|         - | 3090 | `	}` |
|         - | 3091 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3092 | `	 * error message as official PHP. Check the index to detect constants. */` |
|       104 | 3093 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3094 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3095 | `			"Error",` |
|         - | 3096 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3097 | `			);` |
|         - | 3098 | `	}` |
|         - | 3099 | `	/* Make sure we are dealing with a valid hashmap */` |
|        98 | 3100 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3101 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3102 | `			"TypeError",` |
|         - | 3103 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3104 | `			ph7_type_name(apArg[0])` |
|         - | 3105 | `			);` |
|         - | 3106 | `	}` |
|        95 | 3107 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        95 | 3108 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        95 | 3109 | `	if( pMap->nEntry < 1 ){` |
|         - | 3110 | `		/* Nothing to pop,return NULL */` |
|         3 | 3111 | `		ph7_result_null(pCtx);` |
|         2 | 3112 | `	}else{` |
|        93 | 3113 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3114 | `		ph7_value *pObj;` |
|        93 | 3115 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        93 | 3116 | `		if( pObj ){` |
|         - | 3117 | `			/* Node value */` |
|        93 | 3118 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3119 | `			/* Unlink the node */` |
|        93 | 3120 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        47 | 3121 | `		}else{` |
|       ! 0 | 3122 | `			ph7_result_null(pCtx);` |
|         - | 3123 | `		}` |
|         - | 3124 | `		/* Reset the cursor */` |
|        93 | 3125 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3126 | `	}` |
|        95 | 3127 | `	return PH7_OK;` |
|        57 | 3128 | `}` |
|         - | 3129 | `/*` |
|         - | 3130 | ` * int array_push($array,$var,...)` |
|         - | 3131 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3132 | ` * Parameters` |
|         - | 3133 | ` *  array` |
|         - | 3134 | ` *    The input array.` |
|         - | 3135 | ` *  var` |
|         - | 3136 | ` *   On or more value to push.` |
|         - | 3137 | ` * Return` |
|         - | 3138 | ` *  New array count (including old items).` |
|         - | 3139 | ` */` |
|        24 | 3140 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3141 | `{` |
|         - | 3142 | `	ph7_hashmap *pMap;` |
|         - | 3143 | `	sxi32 rc;` |
|         - | 3144 | `	int i;` |
|        29 | 3145 | `	if( nArg < 1 ){` |
|         4 | 3146 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3147 | `			"ArgumentCountError",` |
|         - | 3148 | `			"array_push() expects at least 1 argument, %d given",` |
|         1 | 3149 | `			nArg` |
|         - | 3150 | `			);` |
|         - | 3151 | `	}` |
|         - | 3152 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3153 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        27 | 3154 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3155 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3156 | `			"Error",` |
|         - | 3157 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3158 | `			);` |
|         - | 3159 | `	}` |
|         - | 3160 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3161 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3162 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3163 | `			"TypeError",` |
|         - | 3164 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3165 | `			ph7_type_name(apArg[0])` |
|         - | 3166 | `			);` |
|         - | 3167 | `	}` |
|         - | 3168 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3169 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3170 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3171 | `	/* Start pushing given values */` |
|        34 | 3172 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3173 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3174 | `		if( rc != SXRET_OK ){` |
|         3 | 3175 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3176 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3177 | `				return rc;` |
|         - | 3178 | `			}` |
|       ! 0 | 3179 | `			break;` |
|         - | 3180 | `		}` |
|         9 | 3181 | `	}` |
|         - | 3182 | `	/* Return the new count */` |
|        15 | 3183 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3184 | `	return PH7_OK;` |
|        17 | 3185 | `}` |
|         - | 3186 | `/*` |
|         - | 3187 | ` * value array_shift(array $array)` |
|         - | 3188 | ` *   Shift an element off the beginning of array.` |
|         - | 3189 | ` * Parameter` |
|         - | 3190 | ` *  The array to get the value from.` |
|         - | 3191 | ` * Return` |
|         - | 3192 | ` *  Shifted value or NULL on failure.` |
|         - | 3193 | ` */` |
|        46 | 3194 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3195 | `{` |
|         - | 3196 | `	ph7_hashmap *pMap;` |
|         - | 3197 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        51 | 3198 | `	if( nArg != 1 ){` |
|         8 | 3199 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3200 | `			"ArgumentCountError",` |
|         - | 3201 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         2 | 3202 | `			nArg` |
|         - | 3203 | `			);` |
|         - | 3204 | `	}` |
|         - | 3205 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3206 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3207 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3208 | `			"Error",` |
|         - | 3209 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3210 | `			);` |
|         - | 3211 | `	}` |
|         - | 3212 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3213 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3214 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3215 | `			"TypeError",` |
|         - | 3216 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3217 | `			ph7_type_name(apArg[0])` |
|         - | 3218 | `			);` |
|         - | 3219 | `	}` |
|         - | 3220 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3221 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3222 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3223 | `	if( pMap->nEntry < 1 ){` |
|         - | 3224 | `		/* Empty hashmap,return NULL */` |
|         3 | 3225 | `		ph7_result_null(pCtx);` |
|         2 | 3226 | `	}else{` |
|        39 | 3227 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3228 | `		ph7_value *pObj;` |
|         - | 3229 | `		sxu32 n;` |
|        39 | 3230 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3231 | `		if( pObj ){` |
|         - | 3232 | `			/* Node value */` |
|        39 | 3233 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3234 | `			/* Unlink the first node */` |
|        39 | 3235 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3236 | `		}else{` |
|       ! 0 | 3237 | `			ph7_result_null(pCtx);` |
|         - | 3238 | `		}` |
|         - | 3239 | `		/* Rehash all int keys */` |
|        39 | 3240 | `		n = pMap->nEntry;` |
|        39 | 3241 | `		pEntry = pMap->pFirst;` |
|        39 | 3242 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|        47 | 3243 | `		for(;;){` |
|        99 | 3244 | `			if( n < 1 ){` |
|        39 | 3245 | `				break;` |
|         - | 3246 | `			}` |
|        65 | 3247 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3248 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3249 | `			}` |
|         - | 3250 | `			/* Point to the next entry */` |
|        65 | 3251 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3252 | `			n--;` |
|         5 | 3253 | `		}` |
|         - | 3254 | `		/* Reset the cursor */` |
|        39 | 3255 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3256 | `	}` |
|        41 | 3257 | `	return PH7_OK;` |
|        28 | 3258 | `}` |
|         - | 3259 | `/*` |
|         - | 3260 | ` * Extract the node cursor value.` |
|         - | 3261 | ` */` |
|      1094 | 3262 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3263 | `{` |
|      1095 | 3264 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3265 | `	ph7_value *pVal;` |
|      1095 | 3266 | `	if( pCur == 0 ){` |
|         - | 3267 | `		/* Cursor does not point to anything,return FALSE */` |
|        39 | 3268 | `		ph7_result_bool(pCtx,0);` |
|        39 | 3269 | `		return PH7_OK;` |
|         - | 3270 | `	}` |
|      1057 | 3271 | `	if( iDirection != 0 ){` |
|       201 | 3272 | `		if( iDirection > 0 ){` |
|         - | 3273 | `			/* Point to the next entry */` |
|       199 | 3274 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       199 | 3275 | `			pCur = pMap->pCur;` |
|       100 | 3276 | `		}else{` |
|         - | 3277 | `			/* Point to the previous entry */` |
|         3 | 3278 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3279 | `			pCur = pMap->pCur;` |
|         - | 3280 | `		}` |
|       201 | 3281 | `		if( pCur == 0 ){` |
|         - | 3282 | `			/* End of input reached,return FALSE */` |
|        83 | 3283 | `			ph7_result_bool(pCtx,0);` |
|        83 | 3284 | `			return PH7_OK;` |
|         - | 3285 | `		}` |
|        59 | 3286 | `	}` |
|         - | 3287 | `	/* Point to the desired element */` |
|       975 | 3288 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       975 | 3289 | `	if( pVal ){` |
|       975 | 3290 | `		ph7_result_value(pCtx,pVal);` |
|       488 | 3291 | `	}else{` |
|       ! 0 | 3292 | `		ph7_result_bool(pCtx,0);` |
|         - | 3293 | `	}` |
|       975 | 3294 | `	return PH7_OK;` |
|       548 | 3295 | `}` |
|         - | 3296 | `/*` |
|         - | 3297 | ` * value current(array $array)` |
|         - | 3298 | ` *  Return the current element in an array.` |
|         - | 3299 | ` * Parameter` |
|         - | 3300 | ` *  $input: The input array.` |
|         - | 3301 | ` * Return` |
|         - | 3302 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3303 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3304 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3305 | ` *  is empty, current() returns FALSE.` |
|         - | 3306 | ` */` |
|       302 | 3307 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3308 | `{` |
|       303 | 3309 | `	if( nArg < 1 ){` |
|         - | 3310 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3311 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3312 | `		return PH7_OK;` |
|         - | 3313 | `	}` |
|         - | 3314 | `	/* Make sure we are dealing with a valid hashmap */` |
|       303 | 3315 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3316 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3317 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3318 | `		return PH7_OK;` |
|         - | 3319 | `	}` |
|       303 | 3320 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       303 | 3321 | `	return PH7_OK;` |
|       152 | 3322 | `}` |
|         - | 3323 | `/*` |
|         - | 3324 | ` * value next(array $input)` |
|         - | 3325 | ` *  Advance the internal array pointer of an array.` |
|         - | 3326 | ` * Parameter` |
|         - | 3327 | ` *  $input: The input array.` |
|         - | 3328 | ` * Return` |
|         - | 3329 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3330 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3331 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3332 | ` */` |
|       198 | 3333 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3334 | `{` |
|       199 | 3335 | `	if( nArg < 1 ){` |
|         - | 3336 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3337 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3338 | `		return PH7_OK;` |
|         - | 3339 | `	}` |
|         - | 3340 | `	/* Make sure we are dealing with a valid hashmap */` |
|       199 | 3341 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3342 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3343 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3344 | `		return PH7_OK;` |
|         - | 3345 | `	}` |
|       199 | 3346 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       199 | 3347 | `	return PH7_OK;` |
|       100 | 3348 | `}` |
|         - | 3349 | `/*` |
|         - | 3350 | ` * value prev(array $input)` |
|         - | 3351 | ` *  Rewind the internal array pointer.` |
|         - | 3352 | ` * Parameter` |
|         - | 3353 | ` *  $input: The input array.` |
|         - | 3354 | ` * Return` |
|         - | 3355 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3356 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3357 | ` *  elements.` |
|         - | 3358 | ` */` |
|         2 | 3359 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3360 | `{` |
|         3 | 3361 | `	if( nArg < 1 ){` |
|         - | 3362 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3363 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3364 | `		return PH7_OK;` |
|         - | 3365 | `	}` |
|         - | 3366 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3367 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3368 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3369 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3370 | `		return PH7_OK;` |
|         - | 3371 | `	}` |
|         3 | 3372 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3373 | `	return PH7_OK;` |
|         2 | 3374 | `}` |
|         - | 3375 | `/*` |
|         - | 3376 | ` * value end(array $input)` |
|         - | 3377 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3378 | ` * Parameter` |
|         - | 3379 | ` *  $input: The input array.` |
|         - | 3380 | ` * Return` |
|         - | 3381 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3382 | ` */` |
|       348 | 3383 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3384 | `{` |
|         - | 3385 | `	ph7_hashmap *pMap;` |
|       349 | 3386 | `	if( nArg < 1 ){` |
|         - | 3387 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3388 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3389 | `		return PH7_OK;` |
|         - | 3390 | `	}` |
|         - | 3391 | `	/* Make sure we are dealing with a valid hashmap */` |
|       349 | 3392 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3393 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3394 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3395 | `		return PH7_OK;` |
|         - | 3396 | `	}` |
|         - | 3397 | `	/* Point to the internal representation of the input hashmap */` |
|       349 | 3398 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3399 | `	/* Point to the last node */` |
|       349 | 3400 | `	pMap->pCur = pMap->pLast;` |
|         - | 3401 | `	/* Return the last node value */` |
|       349 | 3402 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       349 | 3403 | `	return PH7_OK;` |
|       175 | 3404 | `}` |
|         - | 3405 | `/*` |
|         - | 3406 | ` * value reset(array $array )` |
|         - | 3407 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3408 | ` * Parameter` |
|         - | 3409 | ` *  $input: The input array.` |
|         - | 3410 | ` * Return` |
|         - | 3411 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3412 | ` */` |
|       244 | 3413 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3414 | `{` |
|         - | 3415 | `	ph7_hashmap *pMap;` |
|       245 | 3416 | `	if( nArg < 1 ){` |
|         - | 3417 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3418 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3419 | `		return PH7_OK;` |
|         - | 3420 | `	}` |
|         - | 3421 | `	/* Make sure we are dealing with a valid hashmap */` |
|       245 | 3422 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3423 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3424 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3425 | `		return PH7_OK;` |
|         - | 3426 | `	}` |
|         - | 3427 | `	/* Point to the internal representation of the input hashmap */` |
|       245 | 3428 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3429 | `	/* Point to the first node */` |
|       245 | 3430 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3431 | `	/* Return the last node value if available */` |
|       245 | 3432 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       245 | 3433 | `	return PH7_OK;` |
|       123 | 3434 | `}` |
|         - | 3435 | `/*` |
|         - | 3436 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3437 | ` * array_key_first() and array_key_last().` |
|         - | 3438 | ` */` |
|       672 | 3439 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3440 | `{` |
|       673 | 3441 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3442 | `		/* Key is integer */` |
|       283 | 3443 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       142 | 3444 | `	}else{` |
|         - | 3445 | `		/* Key is blob */` |
|       586 | 3446 | `		ph7_result_string(pCtx,` |
|       390 | 3447 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3448 | `	}` |
|       673 | 3449 | `}` |
|         - | 3450 | `/*` |
|         - | 3451 | ` * value key(array $array)` |
|         - | 3452 | ` *   Fetch a key from an array` |
|         - | 3453 | ` * Parameter` |
|         - | 3454 | ` *  $input` |
|         - | 3455 | ` *   The input array.` |
|         - | 3456 | ` * Return` |
|         - | 3457 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3458 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3459 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3460 | ` *  is empty, key() returns NULL.` |
|         - | 3461 | ` */` |
|       776 | 3462 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3463 | `{` |
|         - | 3464 | `	ph7_hashmap_node *pCur;` |
|         - | 3465 | `	ph7_hashmap *pMap;` |
|       777 | 3466 | `	if( nArg < 1 ){` |
|         - | 3467 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3468 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3469 | `		return PH7_OK;` |
|         - | 3470 | `	}` |
|         - | 3471 | `	/* Make sure we are dealing with a valid hashmap */` |
|       777 | 3472 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3473 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3474 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3475 | `		return PH7_OK;` |
|         - | 3476 | `	}` |
|       777 | 3477 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       777 | 3478 | `	pCur = pMap->pCur;` |
|       777 | 3479 | `	if( pCur == 0 ){` |
|         - | 3480 | `		/* Cursor does not point to anything,return NULL */` |
|       121 | 3481 | `		ph7_result_null(pCtx);` |
|       121 | 3482 | `		return PH7_OK;` |
|         - | 3483 | `	}` |
|       657 | 3484 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       657 | 3485 | `	return PH7_OK;` |
|       389 | 3486 | `}` |
|         - | 3487 | `/*` |
|         - | 3488 | ` * array each(array $input)` |
|         - | 3489 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3490 | ` * Parameter` |
|         - | 3491 | ` *  $input` |
|         - | 3492 | ` *    The input array.` |
|         - | 3493 | ` * Return` |
|         - | 3494 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3495 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3496 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3497 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3498 | ` *  each() returns FALSE.` |
|         - | 3499 | ` */` |
|        22 | 3500 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3501 | `{` |
|         - | 3502 | `	ph7_hashmap_node *pCur;` |
|         - | 3503 | `	ph7_hashmap *pMap;` |
|         - | 3504 | `	ph7_value *pArray;` |
|         - | 3505 | `	ph7_value *pVal;` |
|         - | 3506 | `	ph7_value sKey;` |
|        23 | 3507 | `	if( nArg < 1 ){` |
|         - | 3508 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3509 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3510 | `		return PH7_OK;` |
|         - | 3511 | `	}` |
|         - | 3512 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3513 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3514 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3515 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3516 | `		return PH7_OK;` |
|         - | 3517 | `	}` |
|         - | 3518 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3519 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3520 | `	if( pMap->pCur == 0 ){` |
|         - | 3521 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3522 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3523 | `		return PH7_OK;` |
|         - | 3524 | `	}` |
|        15 | 3525 | `	pCur = pMap->pCur;` |
|         - | 3526 | `	/* Create a new array */` |
|        15 | 3527 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3528 | `	if( pArray == 0 ){` |
|       ! 0 | 3529 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3530 | `		return PH7_OK;` |
|         - | 3531 | `	}` |
|        15 | 3532 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3533 | `	/* Insert the current value */` |
|        15 | 3534 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3535 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3536 | `	/* Make the key */` |
|        15 | 3537 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3538 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3539 | `	}else{` |
|         9 | 3540 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3541 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3542 | `	}` |
|         - | 3543 | `	/* Insert the current key */` |
|        15 | 3544 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3545 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3546 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3547 | `	/* Advance the cursor */` |
|        15 | 3548 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3549 | `	/* Return the current entry */` |
|        15 | 3550 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3551 | `	return PH7_OK;` |
|        12 | 3552 | `}` |
|         - | 3553 | `/*` |
|         - | 3554 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3555 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3556 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3557 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3558 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3559 | ` */` |
|         - | 3560 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3561 | `/*` |
|         - | 3562 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3563 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3564 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3565 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3566 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3567 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3568 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3569 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3570 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3571 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3572 | ` */` |
|         - | 3573 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3574 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3575 | `/*` |
|         - | 3576 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3577 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3578 | ` */` |
|         8 | 3579 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|         1 | 3580 | `{` |
|         9 | 3581 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|         3 | 3582 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|         3 | 3583 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|         3 | 3584 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|         3 | 3585 | `		zBuf[n] = 0;` |
|         3 | 3586 | `		return zBuf;` |
|         - | 3587 | `	}` |
|         7 | 3588 | `	return ph7_type_name(pVal);` |
|         5 | 3589 | `}` |
|         - | 3590 | `/*` |
|         - | 3591 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3592 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3593 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3594 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3595 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3596 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3597 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3598 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3599 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3600 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3601 | ` */` |
|       156 | 3602 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3603 | `{` |
|       157 | 3604 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3605 | `	sxu64 uVal = 0;` |
|       157 | 3606 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3607 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3608 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3609 | `		bNeg = (z[0] == '-');` |
|         3 | 3610 | `		z++;` |
|         1 | 3611 | `	}` |
|       237 | 3612 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3613 | `		int d = z[0] - '0';` |
|         - | 3614 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3615 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3616 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3617 | `			bOverflow = 1;` |
|       ! 0 | 3618 | `		}else{` |
|        81 | 3619 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3620 | `		}` |
|        81 | 3621 | `		bDigit = 1;` |
|        81 | 3622 | `		z++;` |
|         1 | 3623 | `	}` |
|       157 | 3624 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3625 | `		bReal = 1;` |
|         3 | 3626 | `		z++;` |
|         5 | 3627 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3628 | `			bDigit = 1;` |
|         3 | 3629 | `			z++;` |
|         1 | 3630 | `		}` |
|         1 | 3631 | `	}` |
|         - | 3632 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3633 | `	if( !bDigit ){` |
|        61 | 3634 | `		return RANGE_IN_ERROR;` |
|         - | 3635 | `	}` |
|         - | 3636 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3637 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3638 | `		z++;` |
|         9 | 3639 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3640 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3641 | `			return RANGE_IN_ERROR;` |
|         - | 3642 | `		}` |
|         9 | 3643 | `		bReal = 1;` |
|        17 | 3644 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3645 | `	}` |
|         - | 3646 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3647 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3648 | `	if( z != zEnd ){` |
|        13 | 3649 | `		return RANGE_IN_ERROR;` |
|         - | 3650 | `	}` |
|        84 | 3651 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3652 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3653 | `		bReal = 1;` |
|        84 | 3654 | `	}` |
|        43 | 3655 | `	if( bReal ){` |
|        11 | 3656 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3657 | `		return RANGE_IN_DOUBLE;` |
|         - | 3658 | `	}` |
|         - | 3659 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3660 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3661 | `	return RANGE_IN_LONG;` |
|        58 | 3662 | `}` |
|         - | 3663 | `/*` |
|         - | 3664 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3665 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3666 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3667 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3668 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3669 | ` */` |
|       338 | 3670 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3671 | `{` |
|         - | 3672 | `	char zMsg[160];` |
|       339 | 3673 | `	*pRc = PH7_OK;` |
|       339 | 3674 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3675 | `		char zType[80];` |
|        10 | 3676 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3677 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|         3 | 3678 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         7 | 3679 | `		return FALSE;` |
|         - | 3680 | `	}` |
|       333 | 3681 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3682 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3683 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3684 | `			iArg,zName);` |
|         5 | 3685 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3686 | `		*pbNullCoerced = TRUE;` |
|         2 | 3687 | `	}` |
|       333 | 3688 | `	return TRUE;` |
|       170 | 3689 | `}` |
|         - | 3690 | `/*` |
|         - | 3691 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3692 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3693 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3694 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3695 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3696 | ` */` |
|        62 | 3697 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3698 | `{` |
|        63 | 3699 | `	*pRc = PH7_OK;` |
|        63 | 3700 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3701 | `		char zType[80];` |
|         4 | 3702 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3703 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|         1 | 3704 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         3 | 3705 | `		return RANGE_IN_ERROR;` |
|         - | 3706 | `	}` |
|        61 | 3707 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3708 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3709 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3710 | `		*pLong = 0;` |
|         3 | 3711 | `		return RANGE_IN_LONG;` |
|         - | 3712 | `	}` |
|        59 | 3713 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3714 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3715 | `		return RANGE_IN_DOUBLE;` |
|         - | 3716 | `	}` |
|        35 | 3717 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3718 | `		const char *zStr;` |
|         - | 3719 | `		int nLen;` |
|         - | 3720 | `		sxu8 iKind;` |
|         3 | 3721 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3722 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3723 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3724 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3725 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3726 | `		}` |
|         3 | 3727 | `		return iKind;` |
|         - | 3728 | `	}` |
|         - | 3729 | `	/* int / bool */` |
|        33 | 3730 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3731 | `	return RANGE_IN_LONG;` |
|        32 | 3732 | `}` |
|         - | 3733 | `/*` |
|         - | 3734 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3735 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3736 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3737 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3738 | ` */` |
|       296 | 3739 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3740 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3741 | `{` |
|         - | 3742 | `	char zMsg[160];` |
|         - | 3743 | `	double r;` |
|       297 | 3744 | `	*pRc = PH7_OK;` |
|       297 | 3745 | `	if( bNullCoerced ){` |
|         - | 3746 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3747 | `		*pLong = 0;` |
|         5 | 3748 | `		*pDouble = 0.0;` |
|         5 | 3749 | `		return RANGE_IN_LONG;` |
|         - | 3750 | `	}` |
|       293 | 3751 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3752 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3753 | `check_dval:` |
|        25 | 3754 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3755 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3756 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3757 | `			return RANGE_IN_ERROR;` |
|         - | 3758 | `		}` |
|        21 | 3759 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3760 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3761 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3762 | `			return RANGE_IN_ERROR;` |
|         - | 3763 | `		}` |
|        17 | 3764 | `		*pDouble = r;` |
|        17 | 3765 | `		return RANGE_IN_DOUBLE;` |
|         - | 3766 | `	}` |
|       273 | 3767 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3768 | `		const char *zStr;` |
|         - | 3769 | `		int nLen;` |
|         - | 3770 | `		sxu8 iKind;` |
|        81 | 3771 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3772 | `		if( nLen == 0 ){` |
|         7 | 3773 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3774 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3775 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3776 | `			*pLong = 0;` |
|         5 | 3777 | `			*pDouble = 0.0;` |
|        41 | 3778 | `			return RANGE_IN_LONG;` |
|         - | 3779 | `		}` |
|        77 | 3780 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3781 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3782 | `			r = *pDouble;` |
|         5 | 3783 | `			goto check_dval;` |
|         - | 3784 | `		}` |
|        73 | 3785 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3786 | `			*pDouble = (double)*pLong;` |
|        23 | 3787 | `			if( nLen == 1 ){` |
|         - | 3788 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3789 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3790 | `				return RANGE_IN_DIGIT;` |
|         - | 3791 | `			}` |
|        15 | 3792 | `			return RANGE_IN_LONG;` |
|         - | 3793 | `		}` |
|        51 | 3794 | `		if( nLen != 1 ){` |
|        10 | 3795 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3796 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3797 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3798 | `		}` |
|        51 | 3799 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3800 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3801 | `		*pLong = 0;` |
|        51 | 3802 | `		*pDouble = 0.0;` |
|        51 | 3803 | `		return RANGE_IN_STRING;` |
|         - | 3804 | `	}` |
|         - | 3805 | `	/* int / bool */` |
|       193 | 3806 | `	*pLong = ph7_value_to_int64(pIn);` |
|       193 | 3807 | `	*pDouble = (double)*pLong;` |
|       193 | 3808 | `	return RANGE_IN_LONG;` |
|       149 | 3809 | `}` |
|         - | 3810 | `/*` |
|         - | 3811 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3812 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3813 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3814 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3815 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3816 | ` * exactly like php's two macros.` |
|         - | 3817 | ` */` |
|         6 | 3818 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3819 | `{` |
|        10 | 3820 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3821 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3822 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3823 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3824 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3825 | `}` |
|         6 | 3826 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3827 | `{` |
|         - | 3828 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3829 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3830 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3831 | `	const unsigned int nBuf = 1500;` |
|         7 | 3832 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3833 | `	if( zMsg == 0 ){` |
|       ! 0 | 3834 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3835 | `	}` |
|         7 | 3836 | `	snprintf(zMsg,nBuf,` |
|         - | 3837 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3838 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3839 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3840 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3841 | `}` |
|         - | 3842 | `/*` |
|         - | 3843 | ` * Set the element container to the next range element and append it to the` |
|         - | 3844 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3845 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3846 | ` * below stay one line per iteration.` |
|         - | 3847 | ` */` |
|      1680 | 3848 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3849 | `{` |
|      1681 | 3850 | `	ph7_value_int64(pValue,iVal);` |
|      1681 | 3851 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3852 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3853 | `	}` |
|      1681 | 3854 | `	return PH7_OK;` |
|       841 | 3855 | `}` |
|        70 | 3856 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3857 | `{` |
|        71 | 3858 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3859 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3860 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3861 | `	}` |
|        71 | 3862 | `	return PH7_OK;` |
|        36 | 3863 | `}` |
|       168 | 3864 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3865 | `{` |
|       169 | 3866 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3867 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3868 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3869 | `	}` |
|       169 | 3870 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3871 | `	return PH7_OK;` |
|        85 | 3872 | `}` |
|         - | 3873 | `/*` |
|         - | 3874 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3875 | ` *  Create an array containing a range of elements.` |
|         - | 3876 | ` * Return` |
|         - | 3877 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3878 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3879 | ` */` |
|       174 | 3880 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3881 | `{` |
|         - | 3882 | `	ph7_value *pValue,*pArray;` |
|       175 | 3883 | `	sxi32 rc = PH7_OK;` |
|       175 | 3884 | `	int is_step_double = 0,is_step_negative = 0;` |
|       175 | 3885 | `	double step_double = 1.0;` |
|       175 | 3886 | `	sxi64 step = 1;` |
|         - | 3887 | `	sxu8 start_type,end_type;` |
|       175 | 3888 | `	sxi64 start_long = 0,end_long = 0;` |
|       175 | 3889 | `	double start_double = 0.0,end_double = 0.0;` |
|       175 | 3890 | `	unsigned char cStart = 0,cEnd = 0;` |
|       175 | 3891 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3892 | `	sxu32 i,size;` |
|         - | 3893 |  |
|         - | 3894 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       175 | 3895 | `	if( nArg > 3 ){` |
|         4 | 3896 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3897 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3898 | `	}` |
|       173 | 3899 | `	if( nArg < 2 ){` |
|         - | 3900 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3901 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3902 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3903 | `	}` |
|         - | 3904 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3905 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       173 | 3906 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|         7 | 3907 | `		return rc;` |
|         - | 3908 | `	}` |
|       167 | 3909 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3910 | `		return rc;` |
|         - | 3911 | `	}` |
|       167 | 3912 | `	if( nArg > 2 ){` |
|        63 | 3913 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        63 | 3914 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         5 | 3915 | `			return rc;` |
|         - | 3916 | `		}` |
|        59 | 3917 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3918 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3919 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3920 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3921 | `			}` |
|        23 | 3922 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3923 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3924 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3925 | `			}` |
|         - | 3926 | `			/* We only want positive step values. */` |
|        21 | 3927 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3928 | `				is_step_negative = 1;` |
|       ! 0 | 3929 | `				step_double *= -1;` |
|       ! 0 | 3930 | `			}` |
|         - | 3931 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 3932 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 3933 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 3934 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 3935 | `				step = (sxi64)step_double;` |
|        19 | 3936 | `				if( (double)step != step_double ){` |
|        17 | 3937 | `					is_step_double = 1;` |
|         8 | 3938 | `				}` |
|        10 | 3939 | `			}else{` |
|         - | 3940 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 3941 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 3942 | `				is_step_double = 1;` |
|         - | 3943 | `			}` |
|        11 | 3944 | `		}else{` |
|         - | 3945 | `			/* We only want positive step values. */` |
|        35 | 3946 | `			if( step < 0 ){` |
|        11 | 3947 | `				if( step == SMALLEST_INT64 ){` |
|         - | 3948 | `					/* -step would overflow */` |
|         4 | 3949 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 3950 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 3951 | `				}` |
|         9 | 3952 | `				is_step_negative = 1;` |
|         9 | 3953 | `				step = -step;` |
|         4 | 3954 | `			}` |
|        33 | 3955 | `			step_double = (double)step;` |
|         - | 3956 | `		}` |
|        53 | 3957 | `		if( step_double == 0.0 ){` |
|         7 | 3958 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3959 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 3960 | `		}` |
|        23 | 3961 | `	}` |
|       151 | 3962 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       151 | 3963 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 3964 | `		return rc;` |
|         - | 3965 | `	}` |
|       147 | 3966 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       147 | 3967 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 3968 | `		return rc;` |
|         - | 3969 | `	}` |
|         - | 3970 | `	/* Element container + result array */` |
|       143 | 3971 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       143 | 3972 | `	pArray = ph7_context_new_array(pCtx);` |
|       143 | 3973 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 3974 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3975 | `	}` |
|         - | 3976 | `	/* If the range is given as strings, generate an array of characters. */` |
|       143 | 3977 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 3978 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 3979 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 3980 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 3981 | `			 * and the range is numeric. */` |
|        15 | 3982 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 3983 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 3984 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3985 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 3986 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 3987 | `				}` |
|         7 | 3988 | `				end_type = RANGE_IN_LONG;` |
|         4 | 3989 | `			}else{` |
|         9 | 3990 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 3991 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3992 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 3993 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 3994 | `				}` |
|         9 | 3995 | `				start_type = RANGE_IN_LONG;` |
|         - | 3996 | `			}` |
|        15 | 3997 | `			goto handle_numeric_inputs;` |
|         - | 3998 | `		}` |
|        23 | 3999 | `		if( is_step_double ){` |
|         - | 4000 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 4001 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 4002 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4003 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 4004 | `					" of characters, inputs converted to 0");` |
|         1 | 4005 | `			}` |
|         5 | 4006 | `			start_type = RANGE_IN_LONG;` |
|         5 | 4007 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4008 | `			goto handle_numeric_inputs;` |
|         - | 4009 | `		}` |
|         - | 4010 | `		/* Generate an array of characters */` |
|        19 | 4011 | `		if( cStart > cEnd ){` |
|         - | 4012 | `			/* Decreasing char range */` |
|         - | 4013 | `			int iCur;` |
|         3 | 4014 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4015 | `				goto boundary_error;` |
|         - | 4016 | `			}` |
|        17 | 4017 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4018 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4019 | `					return rc;` |
|         - | 4020 | `				}` |
|         8 | 4021 | `			}` |
|        18 | 4022 | `		}else if( cEnd > cStart ){` |
|         - | 4023 | `			/* Increasing char range */` |
|         - | 4024 | `			int iCur;` |
|        15 | 4025 | `			if( is_step_negative ){` |
|         3 | 4026 | `				goto negative_step_error;` |
|         - | 4027 | `			}` |
|        13 | 4028 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4029 | `				goto boundary_error;` |
|         - | 4030 | `			}` |
|       163 | 4031 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4032 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4033 | `					return rc;` |
|         - | 4034 | `				}` |
|        77 | 4035 | `			}` |
|         6 | 4036 | `		}else{` |
|         3 | 4037 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4038 | `				return rc;` |
|         - | 4039 | `			}` |
|         - | 4040 | `		}` |
|        15 | 4041 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4042 | `		return PH7_OK;` |
|         - | 4043 | `	}` |
|        53 | 4044 | `handle_numeric_inputs:` |
|       133 | 4045 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4046 | `		/* Float range */` |
|         - | 4047 | `		double elem,calc;` |
|        25 | 4048 | `		if( start_double > end_double ){` |
|         - | 4049 | `			/* Decreasing float range */` |
|         7 | 4050 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4051 | `				goto boundary_error;` |
|         - | 4052 | `			}` |
|         7 | 4053 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4054 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4055 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4056 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4057 | `			}` |
|         5 | 4058 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4059 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4060 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4061 | `					return rc;` |
|         - | 4062 | `				}` |
|         8 | 4063 | `			}` |
|        21 | 4064 | `		}else if( end_double > start_double ){` |
|         - | 4065 | `			/* Increasing float range */` |
|        17 | 4066 | `			if( is_step_negative ){` |
|       ! 0 | 4067 | `				goto negative_step_error;` |
|         - | 4068 | `			}` |
|        17 | 4069 | `			if( end_double - start_double < step_double ){` |
|         3 | 4070 | `				goto boundary_error;` |
|         - | 4071 | `			}` |
|        15 | 4072 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4073 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4074 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4075 | `			}` |
|        11 | 4076 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4077 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4078 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4079 | `					return rc;` |
|         - | 4080 | `				}` |
|        28 | 4081 | `			}` |
|         6 | 4082 | `		}else{` |
|         3 | 4083 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4084 | `				return rc;` |
|         - | 4085 | `			}` |
|         - | 4086 | `		}` |
|         9 | 4087 | `	}else{` |
|         - | 4088 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4089 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4090 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       101 | 4091 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4092 | `		sxu64 calc;` |
|       101 | 4093 | `		if( start_long > end_long ){` |
|         - | 4094 | `			/* Decreasing int range */` |
|        19 | 4095 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4096 | `				goto boundary_error;` |
|         - | 4097 | `			}` |
|        17 | 4098 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4099 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4100 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4101 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4102 | `			}` |
|        15 | 4103 | `			size = (sxu32)(calc + 1);` |
|       101 | 4104 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4105 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4106 | `					return rc;` |
|         - | 4107 | `				}` |
|        44 | 4108 | `			}` |
|        90 | 4109 | `		}else if( end_long > start_long ){` |
|         - | 4110 | `			/* Increasing int range */` |
|        77 | 4111 | `			if( is_step_negative ){` |
|         3 | 4112 | `				goto negative_step_error;` |
|         - | 4113 | `			}` |
|        75 | 4114 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4115 | `				goto boundary_error;` |
|         - | 4116 | `			}` |
|        73 | 4117 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        73 | 4118 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4119 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4120 | `			}` |
|        69 | 4121 | `			size = (sxu32)(calc + 1);` |
|      1657 | 4122 | `			for( i = 0 ; i < size ; ++i ){` |
|      1589 | 4123 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4124 | `					return rc;` |
|         - | 4125 | `				}` |
|       795 | 4126 | `			}` |
|        35 | 4127 | `		}else{` |
|         7 | 4128 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4129 | `				return rc;` |
|         - | 4130 | `			}` |
|         - | 4131 | `		}` |
|         - | 4132 | `	}` |
|         - | 4133 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4134 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       105 | 4135 | `	ph7_result_value(pCtx,pArray);` |
|       105 | 4136 | `	return PH7_OK;` |
|         2 | 4137 | `negative_step_error:` |
|         5 | 4138 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4139 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4140 | `boundary_error:` |
|         9 | 4141 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4142 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        88 | 4143 | `}` |
|         - | 4144 | `/*` |
|         - | 4145 | ` * array array_values(array $array)` |
|         - | 4146 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4147 | ` * Parameters` |
|         - | 4148 | ` *  $array` |
|         - | 4149 | ` *   The input array.` |
|         - | 4150 | ` * Return` |
|         - | 4151 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4152 | ` */` |
|        50 | 4153 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4154 | `{` |
|         - | 4155 | `	ph7_hashmap_node *pNode;` |
|         - | 4156 | `	ph7_hashmap *pMap;` |
|         - | 4157 | `	ph7_value *pArray;` |
|         - | 4158 | `	ph7_value *pObj;` |
|         - | 4159 | `	sxu32 n;` |
|        54 | 4160 | `	if( nArg != 1 ){` |
|         - | 4161 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         8 | 4162 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4163 | `			"ArgumentCountError",` |
|         - | 4164 | `			"array_values() expects exactly 1 argument, %d given",` |
|         2 | 4165 | `			nArg` |
|         - | 4166 | `			);` |
|         - | 4167 | `	}` |
|         - | 4168 | `	/* Make sure we are dealing with a valid hashmap */` |
|        49 | 4169 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4170 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4171 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4172 | `			"TypeError",` |
|         - | 4173 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4174 | `			ph7_type_name(apArg[0])` |
|         - | 4175 | `			);` |
|         - | 4176 | `	}` |
|         - | 4177 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4178 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4179 | `	/* Create a new array */` |
|        46 | 4180 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4181 | `	if( pArray == 0 ){` |
|       ! 0 | 4182 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4183 | `		return PH7_OK;` |
|         - | 4184 | `	}` |
|         - | 4185 | `	/* Perform the requested operation */` |
|        46 | 4186 | `	pNode = pMap->pFirst;` |
|       144 | 4187 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4188 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4189 | `		if( pObj ){` |
|         - | 4190 | `			/* perform the insertion */` |
|       100 | 4191 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4192 | `		}` |
|         - | 4193 | `		/* Point to the next entry */` |
|       100 | 4194 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4195 | `	}` |
|         - | 4196 | `	/* return the new array */` |
|        46 | 4197 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4198 | `	return PH7_OK;` |
|        29 | 4199 | `}` |
|         - | 4200 | `/*` |
|         - | 4201 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4202 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4203 | ` * Parameters` |
|         - | 4204 | ` *  $input` |
|         - | 4205 | ` *   An array containing keys to return.` |
|         - | 4206 | ` * $search_value` |
|         - | 4207 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4208 | ` * $strict` |
|         - | 4209 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4210 | ` * Return` |
|         - | 4211 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4212 | ` */` |
|       162 | 4213 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4214 | `{` |
|         - | 4215 | `	ph7_hashmap_node *pNode;` |
|         - | 4216 | `	ph7_hashmap *pMap;` |
|         - | 4217 | `	ph7_value *pArray;` |
|         - | 4218 | `	ph7_value sObj;` |
|         - | 4219 | `	ph7_value sVal;` |
|         - | 4220 | `	SyString sKey;` |
|         - | 4221 | `	int bStrict;` |
|         - | 4222 | `	sxi32 rc;` |
|         - | 4223 | `	sxu32 n;` |
|       167 | 4224 | `	if( nArg < 1 ){` |
|         - | 4225 | `		/* Missing argument,throw ArgumentCountError */` |
|         3 | 4226 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4227 | `			"ArgumentCountError",` |
|         - | 4228 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4229 | `			);` |
|         - | 4230 | `	}` |
|         - | 4231 | `	/* Make sure we are dealing with a valid hashmap */` |
|       165 | 4232 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4233 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4234 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4235 | `			"TypeError",` |
|         - | 4236 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4237 | `			ph7_type_name(apArg[0])` |
|         - | 4238 | `			);` |
|         - | 4239 | `	}` |
|         - | 4240 | `	/* Point to the internal representation of the input hashmap */` |
|       163 | 4241 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4242 | `	/* Create a new array */` |
|       163 | 4243 | `	pArray = ph7_context_new_array(pCtx);` |
|       163 | 4244 | `	if( pArray == 0 ){` |
|       ! 0 | 4245 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4246 | `		return PH7_OK;` |
|         - | 4247 | `	}` |
|       163 | 4248 | `	bStrict = FALSE;` |
|       163 | 4249 | `	if( nArg > 2 ){` |
|         - | 4250 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        12 | 4251 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4252 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4253 | `				"TypeError",` |
|         - | 4254 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4255 | `				ph7_type_name(apArg[2])` |
|         - | 4256 | `				);` |
|         - | 4257 | `		}` |
|         9 | 4258 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4259 | `	}` |
|         - | 4260 | `	/* Perform the requested operation */` |
|       160 | 4261 | `	pNode = pMap->pFirst;` |
|       160 | 4262 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1456 | 4263 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1300 | 4264 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       179 | 4265 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        91 | 4266 | `		}else{` |
|      1122 | 4267 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1122 | 4268 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4269 | `		}` |
|      1300 | 4270 | `		rc = 0;` |
|      1300 | 4271 | `		if( nArg > 1 ){` |
|        65 | 4272 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4273 | `			if( pValue ){` |
|         - | 4274 | `				ph7_value sNeedle;` |
|        65 | 4275 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4276 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4277 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4278 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4279 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4280 | `				 * corrupt every later comparison. */` |
|        65 | 4281 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4282 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4283 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4284 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4285 | `			}` |
|        32 | 4286 | `		}` |
|      1300 | 4287 | `		if( rc == 0 ){` |
|         - | 4288 | `			/* Perform the insertion */` |
|      1268 | 4289 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       632 | 4290 | `		}` |
|      1300 | 4291 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4292 | `		/* Point to the next entry */` |
|      1300 | 4293 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       652 | 4294 | `	}` |
|         - | 4295 | `	/* return the new array */` |
|       160 | 4296 | `	ph7_result_value(pCtx,pArray);` |
|       160 | 4297 | `	return PH7_OK;` |
|        86 | 4298 | `}` |
|         - | 4299 | `/*` |
|         - | 4300 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4301 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4302 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4303 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4304 | ` * Parameters` |
|         - | 4305 | ` *  $arr1` |
|         - | 4306 | ` *   First array` |
|         - | 4307 | ` *  $arr2` |
|         - | 4308 | ` *   Second array` |
|         - | 4309 | ` * Return` |
|         - | 4310 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4311 | ` * Note` |
|         - | 4312 | ` *  This function is a symisc eXtension.` |
|         - | 4313 | ` */` |
|         4 | 4314 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4315 | `{` |
|         - | 4316 | `	ph7_hashmap *p1,*p2;` |
|         - | 4317 | `	int rc;` |
|         5 | 4318 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4319 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4320 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4321 | `		return PH7_OK;` |
|         - | 4322 | `	}` |
|         - | 4323 | `	/* Point to the hashmaps */` |
|         5 | 4324 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4325 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4326 | `	rc = (p1 == p2);` |
|         - | 4327 | `	/* Same instance? */` |
|         5 | 4328 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4329 | `	return PH7_OK;` |
|         3 | 4330 | `}` |
|         - | 4331 | `/*` |
|         - | 4332 | ` * array array_merge(array ...$arrays)` |
|         - | 4333 | ` *  Merge one or more arrays.` |
|         - | 4334 | ` * Parameters` |
|         - | 4335 | ` *  ...$arrays` |
|         - | 4336 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4337 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4338 | ` * Return` |
|         - | 4339 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4340 | ` *  with no arguments.` |
|         - | 4341 | ` */` |
|      1056 | 4342 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4343 | `{` |
|         - | 4344 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4345 | `	ph7_value *pArray;` |
|         - | 4346 | `	int i;` |
|         - | 4347 | `	/* Create a new array */` |
|      1061 | 4348 | `	pArray = ph7_context_new_array(pCtx);` |
|      1061 | 4349 | `	if( pArray == 0 ){` |
|       ! 0 | 4350 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4351 | `		return PH7_OK;` |
|         - | 4352 | `	}` |
|         - | 4353 | `	/* Point to the internal representation of the hashmap */` |
|      1061 | 4354 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4355 | `	/* Start merging */` |
|      3163 | 4356 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4357 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2111 | 4358 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4359 | `			/* Type mismatch -> TypeError */` |
|         8 | 4360 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4361 | `				"TypeError",` |
|         - | 4362 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4363 | `				i + 1,` |
|         4 | 4364 | `				ph7_type_name(apArg[i])` |
|         - | 4365 | `				);` |
|       ! 0 | 4366 | `		}else{` |
|      2107 | 4367 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4368 | `			/* Merge the two hashmaps */` |
|      2107 | 4369 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4370 | `		}` |
|      1056 | 4371 | `	}` |
|         - | 4372 | `	/* Return the freshly created array */` |
|      1057 | 4373 | `	ph7_result_value(pCtx,pArray);` |
|      1057 | 4374 | `	return PH7_OK;` |
|       533 | 4375 | `}` |
|         - | 4376 | `/*` |
|         - | 4377 | ` * array array_copy(array $source)` |
|         - | 4378 | ` *  Make a blind copy of the target array.` |
|         - | 4379 | ` * Parameters` |
|         - | 4380 | ` *  $source` |
|         - | 4381 | ` *   Target array` |
|         - | 4382 | ` * Return` |
|         - | 4383 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4384 | ` * Note` |
|         - | 4385 | ` *  This function is a symisc eXtension.` |
|         - | 4386 | ` */` |
|        18 | 4387 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4388 | `{` |
|         - | 4389 | `	ph7_hashmap *pMap;` |
|         - | 4390 | `	ph7_value *pArray;` |
|        19 | 4391 | `	if( nArg < 1 ){` |
|         - | 4392 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4393 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4394 | `		return PH7_OK;` |
|         - | 4395 | `	}` |
|         - | 4396 | `	/* Create a new array */` |
|        19 | 4397 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4398 | `	if( pArray == 0 ){` |
|       ! 0 | 4399 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4400 | `		return PH7_OK;` |
|         - | 4401 | `	}` |
|         - | 4402 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4403 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4404 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4405 | `		/* Point to the internal representation of the source */` |
|        19 | 4406 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4407 | `		/* Perform the copy */` |
|        19 | 4408 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4409 | `	}else{` |
|         - | 4410 | `		/* Simple insertion */` |
|       ! 0 | 4411 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4412 | `	}` |
|         - | 4413 | `	/* Return the duplicated array */` |
|        19 | 4414 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4415 | `	return PH7_OK;` |
|        10 | 4416 | `}` |
|         - | 4417 | `/*` |
|         - | 4418 | ` * bool array_erase(array $source)` |
|         - | 4419 | ` *  Remove all elements from a given array.` |
|         - | 4420 | ` * Parameters` |
|         - | 4421 | ` *  $source` |
|         - | 4422 | ` *   Target array` |
|         - | 4423 | ` * Return` |
|         - | 4424 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4425 | ` * Note` |
|         - | 4426 | ` *  This function is a symisc eXtension.` |
|         - | 4427 | ` */` |
|        26 | 4428 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4429 | `{` |
|         - | 4430 | `	ph7_hashmap *pMap;` |
|        28 | 4431 | `	if( nArg < 1 ){` |
|         - | 4432 | `		/* Missing arguments */` |
|       ! 0 | 4433 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4434 | `		return PH7_OK;` |
|         - | 4435 | `	}` |
|         - | 4436 | `	/* Point to the target hashmap */` |
|        28 | 4437 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4438 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4439 | `	/* Erase */` |
|        28 | 4440 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4441 | `	return PH7_OK;` |
|        15 | 4442 | `}` |
|         - | 4443 | `/*` |
|         - | 4444 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4445 | ` *  Extract a slice of the array.` |
|         - | 4446 | ` * Parameters` |
|         - | 4447 | ` *  $array` |
|         - | 4448 | ` *    The input array.` |
|         - | 4449 | ` * $offset` |
|         - | 4450 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4451 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4452 | ` * $length (optional, nullable)` |
|         - | 4453 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4454 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4455 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4456 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4457 | ` * $preserve_keys (optional)` |
|         - | 4458 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4459 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4460 | ` * Return` |
|         - | 4461 | ` *   The new slice.` |
|         - | 4462 | ` */` |
|        52 | 4463 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4464 | `{` |
|         - | 4465 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4466 | `	ph7_hashmap_node *pCur;` |
|         - | 4467 | `	ph7_value *pArray;` |
|         - | 4468 | `	int iLength,iOfft;` |
|         - | 4469 | `	int bPreserve;` |
|         - | 4470 | `	sxi32 rc;` |
|        57 | 4471 | `	if( nArg < 2 ){` |
|         8 | 4472 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4473 | `			"ArgumentCountError",` |
|         - | 4474 | `			"array_slice() expects at least 2 arguments, %d given",` |
|         2 | 4475 | `			nArg` |
|         - | 4476 | `			);` |
|         - | 4477 | `	}` |
|        53 | 4478 | `	if( nArg > 4 ){` |
|         4 | 4479 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4480 | `			"ArgumentCountError",` |
|         - | 4481 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4482 | `			nArg` |
|         - | 4483 | `			);` |
|         - | 4484 | `	}` |
|        51 | 4485 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4486 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4487 | `			"TypeError",` |
|         - | 4488 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4489 | `			ph7_type_name(apArg[0])` |
|         - | 4490 | `			);` |
|         - | 4491 | `	}` |
|         - | 4492 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        65 | 4493 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        68 | 4494 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4495 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4496 | `			"TypeError",` |
|         - | 4497 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4498 | `			ph7_type_name(apArg[1])` |
|         - | 4499 | `			);` |
|         - | 4500 | `	}` |
|         - | 4501 | `	/* Validate $length type if provided: nullable int */` |
|        47 | 4502 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        29 | 4503 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        29 | 4504 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4505 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4506 | `				"TypeError",` |
|         - | 4507 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4508 | `				ph7_type_name(apArg[2])` |
|         - | 4509 | `				);` |
|         - | 4510 | `		}` |
|         9 | 4511 | `	}` |
|         - | 4512 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        45 | 4513 | `	if( nArg > 3 ){` |
|        10 | 4514 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4515 | `			ph7_value_is_resource(apArg[3]) ){` |
|         4 | 4516 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4517 | `				"TypeError",` |
|         - | 4518 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|         2 | 4519 | `				ph7_type_name(apArg[3])` |
|         - | 4520 | `				);` |
|         - | 4521 | `		}` |
|         2 | 4522 | `	}` |
|         - | 4523 | `	/* Point the internal representation of the target array */` |
|        43 | 4524 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        43 | 4525 | `	bPreserve = FALSE;` |
|         - | 4526 | `	/* Get the offset */` |
|         - | 4527 | `	{` |
|        43 | 4528 | `		sxi64 iTmp = 0;` |
|        43 | 4529 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|        43 | 4530 | `		if( rcArg != PH7_OK ){` |
|       ! 0 | 4531 | `			return rcArg;` |
|         - | 4532 | `		}` |
|        43 | 4533 | `		iOfft = (int)iTmp;` |
|         - | 4534 | `	}` |
|        43 | 4535 | `	if( iOfft < 0 ){` |
|         5 | 4536 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4537 | `		if( iOfft < 0 ){` |
|         3 | 4538 | `			iOfft = 0;` |
|         1 | 4539 | `		}` |
|         2 | 4540 | `	}` |
|        43 | 4541 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4542 | `		/* Offset past end of array, return empty array */` |
|         5 | 4543 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4544 | `		if( pArray == 0 ){` |
|       ! 0 | 4545 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4546 | `			return PH7_OK;` |
|         - | 4547 | `		}` |
|         5 | 4548 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4549 | `		return PH7_OK;` |
|         - | 4550 | `	}` |
|         - | 4551 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        39 | 4552 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        39 | 4553 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        17 | 4554 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        17 | 4555 | `		if( iLength < 0 ){` |
|         5 | 4556 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4557 | `		}` |
|        17 | 4558 | `		if( iLength < 0 ){` |
|         3 | 4559 | `			iLength = 0;` |
|         1 | 4560 | `		}` |
|        17 | 4561 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4562 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4563 | `		}` |
|         8 | 4564 | `	}` |
|        39 | 4565 | `	if( nArg > 3 ){` |
|         5 | 4566 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4567 | `	}` |
|         - | 4568 | `	/* Create a new array */` |
|        39 | 4569 | `	pArray = ph7_context_new_array(pCtx);` |
|        39 | 4570 | `	if( pArray == 0 ){` |
|       ! 0 | 4571 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4572 | `		return PH7_OK;` |
|         - | 4573 | `	}` |
|        39 | 4574 | `	if( iLength < 1 ){` |
|         - | 4575 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4576 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4577 | `		return PH7_OK;` |
|         - | 4578 | `	}` |
|         - | 4579 | `	/* Point to the desired entry */` |
|        35 | 4580 | `	pCur = pSrc->pFirst;` |
|        29 | 4581 | `	for(;;){` |
|        63 | 4582 | `		if( iOfft < 1 ){` |
|        35 | 4583 | `			break;` |
|         - | 4584 | `		}` |
|         - | 4585 | `		/* Point to the next entry */` |
|        33 | 4586 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4587 | `		iOfft--;` |
|         5 | 4588 | `	}` |
|         - | 4589 | `	/* Point to the internal representation of the hashmap */` |
|        35 | 4590 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        54 | 4591 | `	for(;;){` |
|       113 | 4592 | `		if( iLength < 1 ){` |
|        35 | 4593 | `			break;` |
|         - | 4594 | `		}` |
|         - | 4595 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4596 | `		{` |
|        83 | 4597 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        83 | 4598 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4599 | `		}` |
|        83 | 4600 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4601 | `			break;` |
|         - | 4602 | `		}` |
|         - | 4603 | `		/* Point to the next entry */` |
|        83 | 4604 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        83 | 4605 | `		iLength--;` |
|         5 | 4606 | `	}` |
|         - | 4607 | `	/* Return the freshly created array */` |
|        35 | 4608 | `	ph7_result_value(pCtx,pArray);` |
|        35 | 4609 | `	return PH7_OK;` |
|        31 | 4610 | `}` |
|         - | 4611 | `/*` |
|         - | 4612 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4613 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4614 | ` * beginning (becomes the new pFirst).` |
|         - | 4615 | ` */` |
|        38 | 4616 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4617 | `{` |
|         - | 4618 | `	ph7_hashmap_node *pNode;` |
|         - | 4619 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4620 | `	pNode = pMap->pLast;` |
|        39 | 4621 | `	if( pNode == 0 ){` |
|       ! 0 | 4622 | `		return;` |
|         - | 4623 | `	}` |
|        39 | 4624 | `	if( pNode->pNext == 0 ){` |
|         - | 4625 | `		/* Only node in the list, nothing to move */` |
|         5 | 4626 | `		return;` |
|         - | 4627 | `	}` |
|        35 | 4628 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4629 | `		/* Already in the correct position */` |
|         9 | 4630 | `		return;` |
|         - | 4631 | `	}` |
|         - | 4632 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4633 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4634 | `	pMap->pLast->pPrev = 0;` |
|         - | 4635 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4636 | `	if( pAfter == 0 ){` |
|         - | 4637 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4638 | `		pNode->pNext = 0;` |
|         3 | 4639 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4640 | `		if( pMap->pFirst ){` |
|         3 | 4641 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4642 | `		}` |
|         3 | 4643 | `		pMap->pFirst = pNode;` |
|         2 | 4644 | `	}else{` |
|        25 | 4645 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4646 | `		pNode->pPrev = pOldNext;` |
|        25 | 4647 | `		pNode->pNext = pAfter;` |
|        25 | 4648 | `		pAfter->pPrev = pNode;` |
|        25 | 4649 | `		if( pOldNext ){` |
|        25 | 4650 | `			pOldNext->pNext = pNode;` |
|        13 | 4651 | `		}else{` |
|       ! 0 | 4652 | `			pMap->pLast = pNode;` |
|         - | 4653 | `		}` |
|         - | 4654 | `	}` |
|        20 | 4655 | `}` |
|         - | 4656 | `/*` |
|         - | 4657 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4658 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4659 | ` * Parameters` |
|         - | 4660 | ` *  $array` |
|         - | 4661 | ` *    The input array.` |
|         - | 4662 | ` *  $offset` |
|         - | 4663 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4664 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4665 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4666 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4667 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4668 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4669 | ` *  $length (optional)` |
|         - | 4670 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4671 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4672 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4673 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4674 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4675 | ` *  $replacement (optional)` |
|         - | 4676 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4677 | ` *    with elements from this array.` |
|         - | 4678 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4679 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4680 | ` *    offset.` |
|         - | 4681 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4682 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4683 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4684 | ` * Return` |
|         - | 4685 | ` *   A new array consisting of the extracted elements.` |
|         - | 4686 | ` */` |
|        68 | 4687 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4688 | `{` |
|         - | 4689 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4690 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4691 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4692 | `	int iLength,iOfft,i;` |
|         - | 4693 | `	sxi32 rc;` |
|        72 | 4694 | `	if( nArg < 2 ){` |
|         8 | 4695 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4696 | `			"ArgumentCountError",` |
|         - | 4697 | `			"array_splice() expects at least 2 arguments, %d given",` |
|         2 | 4698 | `			nArg` |
|         - | 4699 | `			);` |
|         - | 4700 | `	}` |
|        66 | 4701 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4702 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4703 | `			"TypeError",` |
|         - | 4704 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4705 | `			ph7_type_name(apArg[0])` |
|         - | 4706 | `			);` |
|         - | 4707 | `	}` |
|         - | 4708 | `	/* Point to the internal representation of the target array */` |
|        63 | 4709 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4710 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4711 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4712 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4713 | `	if( iOfft < 0 ){` |
|         9 | 4714 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4715 | `		if( iOfft < 0 ){` |
|         3 | 4716 | `			iOfft = 0;` |
|         2 | 4717 | `		}` |
|        59 | 4718 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4719 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4720 | `	}` |
|         - | 4721 | `	/* Get the length and clamp to valid range.` |
|         - | 4722 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4723 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4724 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4725 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4726 | `		if( iLength < 0 ){` |
|         7 | 4727 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4728 | `			if( iLength < 0 ){` |
|         3 | 4729 | `				iLength = 0;` |
|         1 | 4730 | `			}` |
|         3 | 4731 | `		}` |
|        45 | 4732 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4733 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4734 | `		}` |
|        22 | 4735 | `	}` |
|         - | 4736 | `	/* Create the result array for removed elements */` |
|        63 | 4737 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4738 | `	if( pArray == 0 ){` |
|       ! 0 | 4739 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4740 | `		return PH7_OK;` |
|         - | 4741 | `	}` |
|         - | 4742 | `	/* Get replacement array if provided */` |
|        63 | 4743 | `	pRep = 0;` |
|        63 | 4744 | `	if( nArg > 3 ){` |
|        27 | 4745 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4746 | `			/* Perform an array cast */` |
|         3 | 4747 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4748 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4749 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4750 | `			}` |
|         2 | 4751 | `		}else{` |
|        25 | 4752 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4753 | `		}` |
|        27 | 4754 | `		if( pRep ){` |
|         - | 4755 | `			/* Reset the loop cursor */` |
|        27 | 4756 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4757 | `		}` |
|        13 | 4758 | `	}` |
|         - | 4759 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4760 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4761 | `	/* Navigate to the offset position */` |
|        63 | 4762 | `	pCur = pSrc->pFirst;` |
|       131 | 4763 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4764 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4765 | `	}` |
|         - | 4766 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4767 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4768 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4769 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4770 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4771 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4772 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4773 | `		pPrev = pCur->pPrev;` |
|        79 | 4774 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4775 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4776 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4777 | `			break;` |
|         - | 4778 | `		}` |
|        79 | 4779 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4780 | `	}` |
|         - | 4781 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4782 | `	if( pRep ){` |
|         - | 4783 | `		ph7_value sSafeVal;` |
|        78 | 4784 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4785 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4786 | `			if( pRvalue ){` |
|         - | 4787 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4788 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4789 | `				 * since it points into that same pool. */` |
|        39 | 4790 | `				sSafeVal = *pRvalue;` |
|        39 | 4791 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4792 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4793 | `					pNewNode = pSrc->pLast;` |
|        39 | 4794 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4795 | `					pInsertAfter = pNewNode;` |
|        19 | 4796 | `				}` |
|        19 | 4797 | `			}` |
|         1 | 4798 | `		}` |
|        13 | 4799 | `	}` |
|         - | 4800 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4801 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4802 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4803 | `	 * and removals left gaps. */` |
|         - | 4804 | `	{` |
|        63 | 4805 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4806 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4807 | `		pSrc->iNextIdx = 0;` |
|       233 | 4808 | `		while( n > 0 ){` |
|       171 | 4809 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4810 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4811 | `			}` |
|       171 | 4812 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4813 | `			n--;` |
|         1 | 4814 | `		}` |
|        63 | 4815 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4816 | `	}` |
|         - | 4817 | `	/* Return the freshly created array */` |
|        63 | 4818 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4819 | `	return PH7_OK;` |
|        38 | 4820 | `}` |
|         - | 4821 | `/*` |
|         - | 4822 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4823 | ` *  Checks if a value exists in an array.` |
|         - | 4824 | ` * Parameters` |
|         - | 4825 | ` *  $needle` |
|         - | 4826 | ` *   The searched value.` |
|         - | 4827 | ` *   Note:` |
|         - | 4828 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4829 | ` * $haystack` |
|         - | 4830 | ` *  The target array.` |
|         - | 4831 | ` * $strict` |
|         - | 4832 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4833 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4834 | ` */` |
|     33128 | 4835 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4836 | `{` |
|         - | 4837 | `	ph7_value *pNeedle;` |
|         - | 4838 | `	int bStrict;` |
|         - | 4839 | `	int rc;` |
|     33133 | 4840 | `	if( nArg < 2 ){` |
|         - | 4841 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4842 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4843 | `		return PH7_OK;` |
|         - | 4844 | `	}` |
|     33133 | 4845 | `	pNeedle = apArg[0];` |
|     33133 | 4846 | `	bStrict = 0;` |
|     33133 | 4847 | `	if( nArg > 2 ){` |
|        53 | 4848 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4849 | `	}` |
|     33133 | 4850 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4851 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4852 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4853 | `		/* Set the comparison result */` |
|       ! 0 | 4854 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4855 | `		return PH7_OK;` |
|         - | 4856 | `	}` |
|         - | 4857 | `	/* Perform the lookup */` |
|     33133 | 4858 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4859 | `	/* Lookup result */` |
|     33133 | 4860 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     33133 | 4861 | `	return PH7_OK;` |
|     16569 | 4862 | `}` |
|         - | 4863 | `/*` |
|         - | 4864 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4865 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4866 | ` * Parameters` |
|         - | 4867 | ` * $needle` |
|         - | 4868 | ` *   The searched value.` |
|         - | 4869 | ` * $haystack` |
|         - | 4870 | ` *   The array.` |
|         - | 4871 | ` * $strict` |
|         - | 4872 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4873 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4874 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4875 | ` * Return` |
|         - | 4876 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4877 | ` */` |
|        32 | 4878 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4879 | `{` |
|         - | 4880 | `	ph7_hashmap_node *pEntry;` |
|         - | 4881 | `	ph7_value *pVal,sNeedle;` |
|         - | 4882 | `	ph7_hashmap *pMap;` |
|         - | 4883 | `	ph7_value sVal;` |
|         - | 4884 | `	int bStrict;` |
|         - | 4885 | `	sxu32 n;` |
|         - | 4886 | `	int rc;` |
|        37 | 4887 | `	if( nArg < 2 ){` |
|         - | 4888 | `		/* Missing argument,throw ArgumentCountError */` |
|         8 | 4889 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4890 | `			"ArgumentCountError",` |
|         - | 4891 | `			"array_search() expects at least 2 arguments, %d given",` |
|         2 | 4892 | `			nArg` |
|         - | 4893 | `			);` |
|         - | 4894 | `	}` |
|        31 | 4895 | `	bStrict = FALSE;` |
|        31 | 4896 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4897 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4898 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4899 | `			"TypeError",` |
|         - | 4900 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4901 | `			ph7_type_name(apArg[1])` |
|         - | 4902 | `			);` |
|         - | 4903 | `	}` |
|        28 | 4904 | `	if( nArg > 2 ){` |
|         - | 4905 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        14 | 4906 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4907 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4908 | `				"TypeError",` |
|         - | 4909 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4910 | `				ph7_type_name(apArg[2])` |
|         - | 4911 | `				);` |
|         - | 4912 | `		}` |
|        11 | 4913 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4914 | `	}` |
|         - | 4915 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4916 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4917 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4918 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 4919 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 4920 | `	pEntry = pMap->pFirst;` |
|        25 | 4921 | `	n = pMap->nEntry;` |
|        28 | 4922 | `	for(;;){` |
|        57 | 4923 | `		if( !n ){` |
|         9 | 4924 | `			break;` |
|         - | 4925 | `		}` |
|         - | 4926 | `		/* Extract node value */` |
|        49 | 4927 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 4928 | `		if( pVal ){` |
|         - | 4929 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4930 | `			 * can change their type.` |
|         - | 4931 | `			 */` |
|        49 | 4932 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 4933 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 4934 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 4935 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 4936 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 4937 | `			if( rc == 0 ){` |
|         - | 4938 | `				/* Match found,return key */` |
|        17 | 4939 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4940 | `					/* INT key */` |
|        11 | 4941 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 4942 | `				}else{` |
|         7 | 4943 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4944 | `					/* Blob key */` |
|         7 | 4945 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4946 | `				}` |
|        17 | 4947 | `				return PH7_OK;` |
|         - | 4948 | `			}` |
|        16 | 4949 | `		}` |
|         - | 4950 | `		/* Point to the next entry */` |
|        33 | 4951 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 4952 | `		n--;` |
|         1 | 4953 | `	}` |
|         - | 4954 | `	/* No such value,return FALSE */` |
|         9 | 4955 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4956 | `	return PH7_OK;` |
|        21 | 4957 | `}` |
|         - | 4958 | `/*` |
|         - | 4959 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4960 | ` *  Computes the difference of arrays.` |
|         - | 4961 | ` * Parameters` |
|         - | 4962 | ` *  $array1` |
|         - | 4963 | ` *    The array to compare from` |
|         - | 4964 | ` *  $array2` |
|         - | 4965 | ` *    An array to compare against` |
|         - | 4966 | ` *  $...` |
|         - | 4967 | ` *   More arrays to compare against` |
|         - | 4968 | ` * Return` |
|         - | 4969 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4970 | ` *  are not present in any of the other arrays.` |
|         - | 4971 | ` */` |
|        22 | 4972 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4973 | `{` |
|         - | 4974 | `	ph7_hashmap_node *pEntry;` |
|         - | 4975 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4976 | `	ph7_value *pArray;` |
|         - | 4977 | `	ph7_value *pVal;` |
|         - | 4978 | `	sxi32 rc;` |
|         - | 4979 | `	sxu32 n;` |
|         - | 4980 | `	int i;` |
|         - | 4981 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 4982 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 4983 | `	 * debugging difficult. */` |
|        26 | 4984 | `	if( nArg < 1 ){` |
|         4 | 4985 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4986 | `			"ArgumentCountError",` |
|         - | 4987 | `			"array_diff() expects at least 1 argument, %d given",` |
|         1 | 4988 | `			nArg` |
|         - | 4989 | `			);` |
|         - | 4990 | `	}` |
|        23 | 4991 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4992 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4993 | `			"TypeError",` |
|         - | 4994 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4995 | `			ph7_type_name(apArg[0])` |
|         - | 4996 | `			);` |
|         - | 4997 | `	}` |
|        36 | 4998 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 4999 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5000 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5001 | `				"TypeError",` |
|         - | 5002 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 5003 | `				i + 1,` |
|         2 | 5004 | `				ph7_type_name(apArg[i])` |
|         - | 5005 | `				);` |
|         - | 5006 | `		}` |
|         9 | 5007 | `	}` |
|        17 | 5008 | `	if( nArg == 1 ){` |
|         - | 5009 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5010 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5011 | `		return PH7_OK;` |
|         - | 5012 | `	}` |
|         - | 5013 | `	/* Create a new array */` |
|        15 | 5014 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5015 | `	if( pArray == 0 ){` |
|       ! 0 | 5016 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5017 | `		return PH7_OK;` |
|         - | 5018 | `	}` |
|         - | 5019 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5020 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5021 | `	/* Perform the diff */` |
|        15 | 5022 | `	pEntry = pSrc->pFirst;` |
|        15 | 5023 | `	n = pSrc->nEntry;` |
|        27 | 5024 | `	for(;;){` |
|        55 | 5025 | `		if( n < 1 ){` |
|        15 | 5026 | `			break;` |
|         - | 5027 | `		}` |
|         - | 5028 | `		/* Extract the node value */` |
|        41 | 5029 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5030 | `		if( pVal ){` |
|        69 | 5031 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5032 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5033 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5034 | `				/* Perform the lookup */` |
|        45 | 5035 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5036 | `				if( rc == SXRET_OK ){` |
|         - | 5037 | `					/* Value exist */` |
|        17 | 5038 | `					break;` |
|         - | 5039 | `				}` |
|        15 | 5040 | `			}` |
|        41 | 5041 | `			if( i >= nArg ){` |
|         - | 5042 | `				/* Perform the insertion */` |
|        25 | 5043 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5044 | `			}` |
|        20 | 5045 | `		}` |
|         - | 5046 | `		/* Point to the next entry */` |
|        41 | 5047 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5048 | `		n--;` |
|         1 | 5049 | `	}` |
|         - | 5050 | `	/* Return the freshly created array */` |
|        15 | 5051 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5052 | `	return PH7_OK;` |
|        15 | 5053 | `}` |
|         - | 5054 | `/*` |
|         - | 5055 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5056 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5057 | ` * Parameters` |
|         - | 5058 | ` *  $array1` |
|         - | 5059 | ` *    The array to compare from` |
|         - | 5060 | ` *  $array2` |
|         - | 5061 | ` *    An array to compare against` |
|         - | 5062 | ` *  $...` |
|         - | 5063 | ` *   More arrays to compare against.` |
|         - | 5064 | ` * $callback` |
|         - | 5065 | ` *  The callback comparison function.` |
|         - | 5066 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5067 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5068 | ` *  than the second.` |
|         - | 5069 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5070 | ` * Return` |
|         - | 5071 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5072 | ` *  are not present in any of the other arrays.` |
|         - | 5073 | ` */` |
|        22 | 5074 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5075 | `{` |
|         - | 5076 | `	ph7_hashmap_node *pEntry;` |
|         - | 5077 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5078 | `	ph7_value *pCallback;` |
|         - | 5079 | `	ph7_value *pArray;` |
|         - | 5080 | `	ph7_value *pVal;` |
|         - | 5081 | `	sxi32 rc;` |
|         - | 5082 | `	sxu32 n;` |
|         - | 5083 | `	int i;` |
|         - | 5084 |  |
|         - | 5085 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        27 | 5086 | `	if( nArg < 2 ){` |
|         4 | 5087 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5088 | `			"ArgumentCountError",` |
|         - | 5089 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|         1 | 5090 | `			nArg` |
|         - | 5091 | `			);` |
|         - | 5092 | `	}` |
|        25 | 5093 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5094 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5095 | `			"TypeError",` |
|         - | 5096 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5097 | `			ph7_type_name(apArg[0])` |
|         - | 5098 | `			);` |
|         - | 5099 | `	}` |
|         - | 5100 |  |
|        23 | 5101 | `	if( nArg == 2 ){` |
|         - | 5102 | `		/* Only the original array and the callback were provided. */` |
|         - | 5103 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5104 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5105 | `		 * validation order.` |
|         - | 5106 | `		 */` |
|         4 | 5107 | `	} else {` |
|         - | 5108 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5109 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5110 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5111 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5112 | `					"TypeError",` |
|         - | 5113 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5114 | `					i + 1,` |
|         6 | 5115 | `					ph7_type_name(apArg[i])` |
|         - | 5116 | `					);` |
|         - | 5117 | `			}` |
|         7 | 5118 | `		}` |
|         - | 5119 | `	}` |
|         - | 5120 |  |
|         - | 5121 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5122 | `	pCallback = apArg[nArg - 1];` |
|         - | 5123 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5124 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5125 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5126 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5127 | `				"TypeError",` |
|         - | 5128 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5129 | `				nArg` |
|         - | 5130 | `				);` |
|         - | 5131 | `		}` |
|         6 | 5132 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5133 | `			int len;` |
|         3 | 5134 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5135 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5136 | `				"TypeError",` |
|         - | 5137 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5138 | `				nArg,` |
|         1 | 5139 | `				zName` |
|         - | 5140 | `				);` |
|         - | 5141 | `		}` |
|         4 | 5142 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5143 | `			"TypeError",` |
|         - | 5144 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5145 | `			nArg` |
|         - | 5146 | `			);` |
|         - | 5147 | `	}` |
|         - | 5148 |  |
|         7 | 5149 | `	if( nArg == 2 ){` |
|         - | 5150 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5151 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5152 | `		return PH7_OK;` |
|         - | 5153 | `	}` |
|         - | 5154 |  |
|         - | 5155 | `	/* Create a new array */` |
|         5 | 5156 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5157 | `	if( pArray == 0 ){` |
|       ! 0 | 5158 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5159 | `		return PH7_OK;` |
|         - | 5160 | `	}` |
|         - | 5161 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5162 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5163 | `	/* Perform the diff */` |
|         5 | 5164 | `	pEntry = pSrc->pFirst;` |
|         5 | 5165 | `	n = pSrc->nEntry;` |
|         5 | 5166 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5167 | `	for(;;){` |
|        11 | 5168 | `		if( n < 1 ){` |
|         3 | 5169 | `			break;` |
|         - | 5170 | `		}` |
|         - | 5171 | `		/* Extract the node value */` |
|         9 | 5172 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5173 | `		if( pVal ){` |
|        15 | 5174 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5175 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5176 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5177 | `				/* Perform the lookup */` |
|         9 | 5178 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5179 | `				if( rc == SXRET_OK ){` |
|         - | 5180 | `					/* Value exist */` |
|         3 | 5181 | `					break;` |
|         - | 5182 | `				}` |
|         4 | 5183 | `			}` |
|         9 | 5184 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5185 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5186 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5187 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5188 | `				return PH7_EXCEPTION;` |
|         - | 5189 | `			}` |
|         7 | 5190 | `			if( i >= (nArg - 1)){` |
|         - | 5191 | `				/* Perform the insertion */` |
|         5 | 5192 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5193 | `			}` |
|         3 | 5194 | `		}` |
|         - | 5195 | `		/* Point to the next entry */` |
|         7 | 5196 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5197 | `		n--;` |
|         1 | 5198 | `	}` |
|         - | 5199 | `	/* Return the freshly created array */` |
|         3 | 5200 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5201 | `	return PH7_OK;` |
|        16 | 5202 | `}` |
|         - | 5203 | `/*` |
|         - | 5204 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5205 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5206 | ` * Parameters` |
|         - | 5207 | ` *  $array1` |
|         - | 5208 | ` *    The array to compare from` |
|         - | 5209 | ` *  $array2` |
|         - | 5210 | ` *    An array to compare against` |
|         - | 5211 | ` *  $...` |
|         - | 5212 | ` *   More arrays to compare against` |
|         - | 5213 | ` * Return` |
|         - | 5214 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5215 | ` *  are not present in any of the other arrays.` |
|         - | 5216 | ` */` |
|        22 | 5217 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5218 | `{` |
|         - | 5219 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5220 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5221 | `	ph7_value *pArray;` |
|         - | 5222 | `	ph7_value *pVal;` |
|         - | 5223 | `	sxi32 rc;` |
|         - | 5224 | `	sxu32 n;` |
|         - | 5225 | `	int i;` |
|         - | 5226 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5227 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5228 | `	 * accompanying integration tests to pass. */` |
|        27 | 5229 | `	if( nArg < 1 ){` |
|         4 | 5230 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5231 | `			"ArgumentCountError",` |
|         - | 5232 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|         1 | 5233 | `			nArg` |
|         - | 5234 | `			);` |
|         - | 5235 | `	}` |
|        24 | 5236 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5237 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5238 | `			"TypeError",` |
|         - | 5239 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5240 | `			ph7_type_name(apArg[0])` |
|         - | 5241 | `			);` |
|         - | 5242 | `	}` |
|        37 | 5243 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5244 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5245 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5246 | `				"TypeError",` |
|         - | 5247 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5248 | `				i + 1,` |
|         4 | 5249 | `				ph7_type_name(apArg[i])` |
|         - | 5250 | `				);` |
|         - | 5251 | `		}` |
|        10 | 5252 | `	}` |
|        15 | 5253 | `	if( nArg == 1 ){` |
|         - | 5254 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5255 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5256 | `		return PH7_OK;` |
|         - | 5257 | `	}` |
|         - | 5258 | `	/* Create a new array */` |
|        13 | 5259 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5260 | `	if( pArray == 0 ){` |
|       ! 0 | 5261 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5262 | `		return PH7_OK;` |
|         - | 5263 | `	}` |
|         - | 5264 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5265 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5266 | `	/* Perform the diff */` |
|        13 | 5267 | `	pEntry = pSrc->pFirst;` |
|        13 | 5268 | `	n = pSrc->nEntry;` |
|        13 | 5269 | `	pN1 = pN2 = 0;` |
|        34 | 5270 | `	for(;;){` |
|         - | 5271 | `		int keep;` |
|        41 | 5272 | `		if( n < 1 ){` |
|        13 | 5273 | `			break;` |
|         - | 5274 | `		}` |
|         - | 5275 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5276 | `		keep = 1;` |
|        47 | 5277 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5278 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5279 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5280 | `			/* Perform a key lookup first */` |
|        33 | 5281 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5282 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5283 | `			}else{` |
|        21 | 5284 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5285 | `			}` |
|        33 | 5286 | `			if( rc != SXRET_OK ){` |
|         - | 5287 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5288 | `				continue;` |
|         - | 5289 | `			}` |
|         - | 5290 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5291 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5292 | `			if( pVal ){` |
|         - | 5293 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5294 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5295 | `				if( pVal2 ){` |
|         - | 5296 | `					ph7_value sV1,sV2;` |
|         - | 5297 | `					sxi32 cmp;` |
|         - | 5298 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5299 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5300 | `					 * null element used to come back bool(false) in the` |
|         - | 5301 | `					 * caller's array). */` |
|        17 | 5302 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5303 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5304 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5305 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5306 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5307 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5308 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5309 | `					if( cmp == 0 ){` |
|         - | 5310 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5311 | `						keep = 0;` |
|        15 | 5312 | `						break;` |
|         - | 5313 | `					}` |
|         1 | 5314 | `				}` |
|         1 | 5315 | `			}` |
|         2 | 5316 | `		}` |
|        29 | 5317 | `		if( keep ){` |
|         - | 5318 | `			/* Perform the insertion */` |
|        15 | 5319 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5320 | `		}` |
|         - | 5321 | `		/* Point to the next entry */` |
|        29 | 5322 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5323 | `		n--;` |
|         1 | 5324 | `	}` |
|         - | 5325 | `	/* Return the freshly created array */` |
|        13 | 5326 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5327 | `	return PH7_OK;` |
|        16 | 5328 | `}` |
|         - | 5329 | `/*` |
|         - | 5330 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5331 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5332 | ` *  by a user supplied callback function.` |
|         - | 5333 | ` * Parameters` |
|         - | 5334 | ` *  $array1` |
|         - | 5335 | ` *    The array to compare from` |
|         - | 5336 | ` *  $array2` |
|         - | 5337 | ` *    An array to compare against` |
|         - | 5338 | ` *  $...` |
|         - | 5339 | ` *   More arrays to compare against.` |
|         - | 5340 | ` *  $key_compare_func` |
|         - | 5341 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5342 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5343 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5344 | ` * Return` |
|         - | 5345 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5346 | ` *  are not present in any of the other arrays.` |
|         - | 5347 | ` */` |
|        24 | 5348 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5349 | `{` |
|         - | 5350 | `	ph7_hashmap_node *pEntry;` |
|         - | 5351 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5352 | `	ph7_value *pCallback;` |
|         - | 5353 | `	ph7_value *pArray;` |
|         - | 5354 | `	sxi32 rc;` |
|         - | 5355 | `	sxu32 n;` |
|         - | 5356 | `	int i;` |
|         - | 5357 |  |
|         - | 5358 | `	/* Argument validation mimicking PHP errors. */` |
|        29 | 5359 | `	if( nArg < 2 ){` |
|         4 | 5360 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5361 | `			"ArgumentCountError",` |
|         - | 5362 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|         1 | 5363 | `			nArg` |
|         - | 5364 | `			);` |
|         - | 5365 | `	}` |
|        26 | 5366 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5367 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5368 | `			"TypeError",` |
|         - | 5369 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5370 | `			ph7_type_name(apArg[0])` |
|         - | 5371 | `			);` |
|         - | 5372 | `	}` |
|         - | 5373 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5374 | `	 * expected to be a callback. */` |
|        38 | 5375 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5376 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5377 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5378 | `				"TypeError",` |
|         - | 5379 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5380 | `				i + 1,` |
|         2 | 5381 | `				ph7_type_name(apArg[i])` |
|         - | 5382 | `				);` |
|         - | 5383 | `		}` |
|         9 | 5384 | `	}` |
|         - | 5385 | `	/* Point to the callback value */` |
|        22 | 5386 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5387 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5388 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5389 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5390 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5391 | `		 * string given" which we also reproduce. */` |
|         9 | 5392 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5393 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5394 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5395 | `				"TypeError",` |
|         - | 5396 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5397 | `				nArg` |
|         - | 5398 | `				);` |
|         - | 5399 | `		}` |
|         6 | 5400 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5401 | `			/* neither array nor string */` |
|         8 | 5402 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5403 | `				"TypeError",` |
|         - | 5404 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5405 | `				nArg` |
|         - | 5406 | `				);` |
|         - | 5407 | `		}` |
|         - | 5408 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5409 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5410 | `			"TypeError",` |
|         - | 5411 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5412 | `			nArg,` |
|       ! 0 | 5413 | `			ph7_type_name(pCallback)` |
|         - | 5414 | `			);` |
|         - | 5415 | `	}` |
|        13 | 5416 | `	if( nArg == 2 ){` |
|         - | 5417 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5418 | `		 * input array. */` |
|         3 | 5419 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5420 | `		return PH7_OK;` |
|         - | 5421 | `	}` |
|         - | 5422 | `	/* Create a new array */` |
|        11 | 5423 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5424 | `	if( pArray == 0 ){` |
|       ! 0 | 5425 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5426 | `		return PH7_OK;` |
|         - | 5427 | `	}` |
|         - | 5428 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5429 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5430 | `	/* Perform the diff */` |
|        11 | 5431 | `	pEntry = pSrc->pFirst;` |
|        11 | 5432 | `	n = pSrc->nEntry;` |
|        21 | 5433 | `	for(;;){` |
|         - | 5434 | `		int keep;` |
|        27 | 5435 | `		if( n < 1 ){` |
|         9 | 5436 | `			break;` |
|         - | 5437 | `		}` |
|        19 | 5438 | `		keep = 1;` |
|        31 | 5439 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5440 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5441 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5442 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5443 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5444 | `			while( pIt ){` |
|         - | 5445 | `				/* build temporary key values for callback */` |
|         - | 5446 | `				ph7_value key1, key2, result;` |
|         - | 5447 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5448 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5449 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5450 | `				}else{` |
|         - | 5451 | `					SyString sStr;` |
|        33 | 5452 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5453 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5454 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5455 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5456 | `				}` |
|        33 | 5457 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5458 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5459 | `				}else{` |
|         - | 5460 | `					SyString sStr;` |
|        33 | 5461 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5462 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5463 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5464 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5465 | `				}` |
|        33 | 5466 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5467 | `				/* call user callback with (key1, key2) */` |
|         - | 5468 | `				{` |
|         - | 5469 | `					ph7_value *apK[2];` |
|        33 | 5470 | `					apK[0] = &key1;` |
|        33 | 5471 | `					apK[1] = &key2;` |
|        33 | 5472 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5473 | `				}` |
|        33 | 5474 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5475 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5476 | `					 * array_uintersect (which signal back from` |
|         - | 5477 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5478 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5479 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5480 | `					PH7_MemObjRelease(&result);` |
|         3 | 5481 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5482 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5483 | `					return PH7_EXCEPTION;` |
|         - | 5484 | `				}` |
|        31 | 5485 | `				if( rc == SXRET_OK ){` |
|        31 | 5486 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5487 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5488 | `					}` |
|        31 | 5489 | `					if( result.x.iVal == 0 ){` |
|         - | 5490 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5491 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5492 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5493 | `						if( pVal1 && pVal2 ){` |
|         - | 5494 | `							ph7_value sV1,sV2;` |
|         - | 5495 | `							sxi32 cmp;` |
|         - | 5496 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5497 | `							 * place and these are LIVE array elements. */` |
|        13 | 5498 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5499 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5500 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5501 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5502 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5503 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5504 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5505 | `							if( cmp == 0 ){` |
|         9 | 5506 | `								keep = 0;` |
|         9 | 5507 | `								PH7_MemObjRelease(&result);` |
|         - | 5508 | `								/* release keys too before breaking */` |
|         9 | 5509 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5510 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5511 | `								break;` |
|         - | 5512 | `							}` |
|         2 | 5513 | `						}` |
|         2 | 5514 | `					}` |
|        11 | 5515 | `				}` |
|        23 | 5516 | `				PH7_MemObjRelease(&result);` |
|        23 | 5517 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5518 | `				PH7_MemObjRelease(&key2);` |
|         - | 5519 | `				/* move to next node */` |
|        23 | 5520 | `				pIt = pIt->pPrev;` |
|        23 | 5521 | `				if( keep == 0 ) break;` |
|         1 | 5522 | `			}` |
|        21 | 5523 | `			if( keep == 0 ) break;` |
|         7 | 5524 | `		}` |
|        17 | 5525 | `		if( keep ){` |
|         - | 5526 | `			/* Perform the insertion */` |
|         9 | 5527 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5528 | `		}` |
|         - | 5529 | `		/* Point to the next entry */` |
|        17 | 5530 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5531 | `		n--;` |
|         1 | 5532 | `	}` |
|         - | 5533 | `	/* Return the freshly created array */` |
|         9 | 5534 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5535 | `	return PH7_OK;` |
|        17 | 5536 | `}` |
|         - | 5537 | `/*` |
|         - | 5538 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5539 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5540 | ` * Parameters` |
|         - | 5541 | ` *  $array1` |
|         - | 5542 | ` *    The array to compare from` |
|         - | 5543 | ` *  $array2` |
|         - | 5544 | ` *    An array to compare against` |
|         - | 5545 | ` *  $...` |
|         - | 5546 | ` *   More arrays to compare against` |
|         - | 5547 | ` * Return` |
|         - | 5548 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5549 | ` *  in any of the other arrays.` |
|         - | 5550 | ` * Note that NULL is returned on failure.` |
|         - | 5551 | ` */` |
|        14 | 5552 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5553 | `{` |
|         - | 5554 | `	ph7_hashmap_node *pEntry;` |
|         - | 5555 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5556 | `	ph7_value *pArray;` |
|         - | 5557 | `	sxi32 rc;` |
|         - | 5558 | `	sxu32 n;` |
|         - | 5559 | `	int i;` |
|         - | 5560 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5561 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5562 | `	 * helpers. */` |
|        18 | 5563 | `	if( nArg < 1 ){` |
|         4 | 5564 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5565 | `			"ArgumentCountError",` |
|         - | 5566 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|         1 | 5567 | `			nArg` |
|         - | 5568 | `			);` |
|         - | 5569 | `	}` |
|        15 | 5570 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5571 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5572 | `			"TypeError",` |
|         - | 5573 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5574 | `			ph7_type_name(apArg[0])` |
|         - | 5575 | `			);` |
|         - | 5576 | `	}` |
|        20 | 5577 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5578 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5579 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5580 | `				"TypeError",` |
|         - | 5581 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5582 | `				i + 1,` |
|         2 | 5583 | `				ph7_type_name(apArg[i])` |
|         - | 5584 | `				);` |
|         - | 5585 | `		}` |
|         5 | 5586 | `	}` |
|         9 | 5587 | `	if( nArg == 1 ){` |
|         - | 5588 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5589 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5590 | `		return PH7_OK;` |
|         - | 5591 | `	}` |
|         - | 5592 | `	/* Create a new array */` |
|         7 | 5593 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5594 | `	if( pArray == 0 ){` |
|       ! 0 | 5595 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5596 | `		return PH7_OK;` |
|         - | 5597 | `	}` |
|         - | 5598 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5599 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5600 | `	/* Perfrom the diff */` |
|         7 | 5601 | `	pEntry = pSrc->pFirst;` |
|         7 | 5602 | `	n = pSrc->nEntry;` |
|        12 | 5603 | `	for(;;){` |
|        25 | 5604 | `		if( n < 1 ){` |
|         7 | 5605 | `			break;` |
|         - | 5606 | `		}` |
|        31 | 5607 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5608 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5609 | `				/* ignore */` |
|       ! 0 | 5610 | `				continue;` |
|         - | 5611 | `			}` |
|        23 | 5612 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5613 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5614 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5615 | `				/* Blob lookup */` |
|        17 | 5616 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5617 | `			}else{` |
|         - | 5618 | `				/* Int lookup */` |
|         7 | 5619 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5620 | `			}` |
|        23 | 5621 | `			if( rc == SXRET_OK ){` |
|         - | 5622 | `				/* Key exists,break immediately */` |
|        11 | 5623 | `				break;` |
|         - | 5624 | `			}` |
|         7 | 5625 | `		}` |
|        19 | 5626 | `		if( i >= nArg ){` |
|         - | 5627 | `			/* Perform the insertion */` |
|         9 | 5628 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5629 | `		}` |
|         - | 5630 | `		/* Point to the next entry */` |
|        19 | 5631 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5632 | `		n--;` |
|         1 | 5633 | `	}` |
|         - | 5634 | `	/* Return the freshly created array */` |
|         7 | 5635 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5636 | `	return PH7_OK;` |
|        11 | 5637 | `}` |
|         - | 5638 | `/*` |
|         - | 5639 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5640 | ` *  Computes the intersection of arrays.` |
|         - | 5641 | ` * Parameters` |
|         - | 5642 | ` *  $array1` |
|         - | 5643 | ` *    The array to compare from` |
|         - | 5644 | ` *  $array2` |
|         - | 5645 | ` *    An array to compare against` |
|         - | 5646 | ` *  $...` |
|         - | 5647 | ` *   More arrays to compare against` |
|         - | 5648 | ` * Return` |
|         - | 5649 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5650 | ` *  in all of the parameters.` |
|         - | 5651 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5652 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5653 | ` */` |
|        22 | 5654 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5655 | `{` |
|         - | 5656 | `	ph7_hashmap_node *pEntry;` |
|         - | 5657 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5658 | `	ph7_value *pArray;` |
|         - | 5659 | `	ph7_value *pVal;` |
|         - | 5660 | `	sxi32 rc;` |
|         - | 5661 | `	sxu32 n;` |
|         - | 5662 | `	int i;` |
|        26 | 5663 | `	if( nArg < 1 ){` |
|         4 | 5664 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5665 | `			"ArgumentCountError",` |
|         - | 5666 | `			"array_intersect() expects at least 1 argument, %d given",` |
|         1 | 5667 | `			nArg` |
|         - | 5668 | `			);` |
|         - | 5669 | `	}` |
|        23 | 5670 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5671 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5672 | `			"TypeError",` |
|         - | 5673 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5674 | `			ph7_type_name(apArg[0])` |
|         - | 5675 | `			);` |
|         - | 5676 | `	}` |
|        36 | 5677 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5678 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5679 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5680 | `				"TypeError",` |
|         - | 5681 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5682 | `				i + 1,` |
|         2 | 5683 | `				ph7_type_name(apArg[i])` |
|         - | 5684 | `				);` |
|         - | 5685 | `		}` |
|         9 | 5686 | `	}` |
|        17 | 5687 | `	if( nArg == 1 ){` |
|         - | 5688 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5689 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5690 | `		return PH7_OK;` |
|         - | 5691 | `	}` |
|         - | 5692 | `	/* Create a new array */` |
|        15 | 5693 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5694 | `	if( pArray == 0 ){` |
|       ! 0 | 5695 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5696 | `		return PH7_OK;` |
|         - | 5697 | `	}` |
|         - | 5698 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5699 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5700 | `	/* Perform the intersection */` |
|        15 | 5701 | `	pEntry = pSrc->pFirst;` |
|        15 | 5702 | `	n = pSrc->nEntry;` |
|        31 | 5703 | `	for(;;){` |
|        63 | 5704 | `		if( n < 1 ){` |
|        15 | 5705 | `			break;` |
|         - | 5706 | `		}` |
|         - | 5707 | `		/* Extract the node value */` |
|        49 | 5708 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5709 | `		if( pVal ){` |
|        79 | 5710 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5711 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5712 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5713 | `				/* Perform the lookup */` |
|        55 | 5714 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5715 | `				if( rc != SXRET_OK ){` |
|         - | 5716 | `					/* Value does not exist */` |
|        25 | 5717 | `					break;` |
|         - | 5718 | `				}` |
|        16 | 5719 | `			}` |
|        49 | 5720 | `			if( i >= nArg ){` |
|         - | 5721 | `				/* Perform the insertion */` |
|        25 | 5722 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5723 | `			}` |
|        24 | 5724 | `		}` |
|         - | 5725 | `		/* Point to the next entry */` |
|        49 | 5726 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5727 | `		n--;` |
|         1 | 5728 | `	}` |
|         - | 5729 | `	/* Return the freshly created array */` |
|        15 | 5730 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5731 | `	return PH7_OK;` |
|        15 | 5732 | `}` |
|         - | 5733 | `/*` |
|         - | 5734 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5735 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5736 | ` * Parameters` |
|         - | 5737 | ` *  $array1` |
|         - | 5738 | ` *    The array to compare from` |
|         - | 5739 | ` *  $array2` |
|         - | 5740 | ` *    An array to compare against` |
|         - | 5741 | ` *  $...` |
|         - | 5742 | ` *   More arrays to compare against` |
|         - | 5743 | ` * Return` |
|         - | 5744 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5745 | ` *  in all the arguments, with matching keys.` |
|         - | 5746 | ` */` |
|        22 | 5747 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5748 | `{` |
|         - | 5749 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5750 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5751 | `	ph7_value *pArray;` |
|         - | 5752 | `	ph7_value *pVal;` |
|         - | 5753 | `	sxi32 rc;` |
|         - | 5754 | `	sxu32 n;` |
|         - | 5755 | `	int i;` |
|        26 | 5756 | `	if( nArg < 1 ){` |
|         4 | 5757 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5758 | `			"ArgumentCountError",` |
|         - | 5759 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|         1 | 5760 | `			nArg` |
|         - | 5761 | `			);` |
|         - | 5762 | `	}` |
|        23 | 5763 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5764 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5765 | `			"TypeError",` |
|         - | 5766 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5767 | `			ph7_type_name(apArg[0])` |
|         - | 5768 | `			);` |
|         - | 5769 | `	}` |
|        36 | 5770 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5771 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5772 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5773 | `				"TypeError",` |
|         - | 5774 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5775 | `				i + 1,` |
|         2 | 5776 | `				ph7_type_name(apArg[i])` |
|         - | 5777 | `				);` |
|         - | 5778 | `		}` |
|         9 | 5779 | `	}` |
|        17 | 5780 | `	if( nArg == 1 ){` |
|         - | 5781 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5782 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5783 | `		return PH7_OK;` |
|         - | 5784 | `	}` |
|         - | 5785 | `	/* Create a new array */` |
|        15 | 5786 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5787 | `	if( pArray == 0 ){` |
|       ! 0 | 5788 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5789 | `		return PH7_OK;` |
|         - | 5790 | `	}` |
|         - | 5791 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5792 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5793 | `	/* Perform the intersection */` |
|        15 | 5794 | `	pEntry = pSrc->pFirst;` |
|        15 | 5795 | `	n = pSrc->nEntry;` |
|        15 | 5796 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5797 | `	for(;;){` |
|        47 | 5798 | `		if( n < 1 ){` |
|        15 | 5799 | `			break;` |
|         - | 5800 | `		}` |
|         - | 5801 | `		/* Extract the node value */` |
|        33 | 5802 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5803 | `		if( pVal ){` |
|        53 | 5804 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5805 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5806 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5807 | `				/* Perform a key lookup first */` |
|        37 | 5808 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5809 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5810 | `				}else{` |
|        23 | 5811 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5812 | `				}` |
|        37 | 5813 | `				if( rc != SXRET_OK ){` |
|         - | 5814 | `					/* No such key,break immediately */` |
|         7 | 5815 | `					break;` |
|         - | 5816 | `				}` |
|         - | 5817 | `				/* Perform the lookup */` |
|        31 | 5818 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5819 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5820 | `					/* Value does not exist */` |
|         6 | 5821 | `					break;` |
|         - | 5822 | `				}` |
|        11 | 5823 | `			}` |
|        33 | 5824 | `			if( i >= nArg ){` |
|         - | 5825 | `				/* Perform the insertion */` |
|        17 | 5826 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5827 | `			}` |
|        16 | 5828 | `		}` |
|         - | 5829 | `		/* Point to the next entry */` |
|        33 | 5830 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5831 | `		n--;` |
|         1 | 5832 | `	}` |
|         - | 5833 | `	/* Return the freshly created array */` |
|        15 | 5834 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5835 | `	return PH7_OK;` |
|        15 | 5836 | `}` |
|         - | 5837 | `/*` |
|         - | 5838 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5839 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5840 | ` * Parameters` |
|         - | 5841 | ` *  $array1` |
|         - | 5842 | ` *    The array to compare from` |
|         - | 5843 | ` *  $...` |
|         - | 5844 | ` *   More arrays to compare against` |
|         - | 5845 | ` * Return` |
|         - | 5846 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5847 | ` *  have keys that are present in all arguments.` |
|         - | 5848 | ` * Note that NULL is returned on failure.` |
|         - | 5849 | ` */` |
|        22 | 5850 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5851 | `{` |
|         - | 5852 | `	ph7_hashmap_node *pEntry;` |
|         - | 5853 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5854 | `	ph7_value *pArray;` |
|         - | 5855 | `	sxi32 rc;` |
|         - | 5856 | `	sxu32 n;` |
|         - | 5857 | `	int i;` |
|        26 | 5858 | `	if( nArg < 1 ){` |
|         4 | 5859 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5860 | `			"ArgumentCountError",` |
|         - | 5861 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|         1 | 5862 | `			nArg` |
|         - | 5863 | `			);` |
|         - | 5864 | `	}` |
|        23 | 5865 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5866 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5867 | `			"TypeError",` |
|         - | 5868 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5869 | `			ph7_type_name(apArg[0])` |
|         - | 5870 | `			);` |
|         - | 5871 | `	}` |
|        36 | 5872 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5873 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5874 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5875 | `				"TypeError",` |
|         - | 5876 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5877 | `				i + 1,` |
|         2 | 5878 | `				ph7_type_name(apArg[i])` |
|         - | 5879 | `				);` |
|         - | 5880 | `		}` |
|         9 | 5881 | `	}` |
|        17 | 5882 | `	if( nArg == 1 ){` |
|         - | 5883 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5884 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5885 | `		return PH7_OK;` |
|         - | 5886 | `	}` |
|         - | 5887 | `	/* Create a new array */` |
|        15 | 5888 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5889 | `	if( pArray == 0 ){` |
|       ! 0 | 5890 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5891 | `		return PH7_OK;` |
|         - | 5892 | `	}` |
|         - | 5893 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5894 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5895 | `	/* Perform the intersection */` |
|        15 | 5896 | `	pEntry = pSrc->pFirst;` |
|        15 | 5897 | `	n = pSrc->nEntry;` |
|        24 | 5898 | `	for(;;){` |
|        49 | 5899 | `		if( n < 1 ){` |
|        15 | 5900 | `			break;` |
|         - | 5901 | `		}` |
|        57 | 5902 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5903 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5904 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5905 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5906 | `				/* Blob lookup */` |
|        27 | 5907 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5908 | `			}else{` |
|         - | 5909 | `				/* Int key */` |
|        13 | 5910 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5911 | `			}` |
|        39 | 5912 | `			if( rc != SXRET_OK ){` |
|         - | 5913 | `				/* Key does not exist, break immediately */` |
|        17 | 5914 | `				break;` |
|         - | 5915 | `			}` |
|        12 | 5916 | `		}` |
|        35 | 5917 | `		if( i >= nArg ){` |
|         - | 5918 | `			/* Perform the insertion */` |
|        19 | 5919 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5920 | `		}` |
|         - | 5921 | `		/* Point to the next entry */` |
|        35 | 5922 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5923 | `		n--;` |
|         1 | 5924 | `	}` |
|         - | 5925 | `	/* Return the freshly created array */` |
|        15 | 5926 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5927 | `	return PH7_OK;` |
|        15 | 5928 | `}` |
|         - | 5929 | `/*` |
|         - | 5930 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5931 | ` *  Computes the intersection of arrays.` |
|         - | 5932 | ` * Parameters` |
|         - | 5933 | ` *  $array1` |
|         - | 5934 | ` *    The array to compare from` |
|         - | 5935 | ` *  $array2` |
|         - | 5936 | ` *    An array to compare against` |
|         - | 5937 | ` *  $...` |
|         - | 5938 | ` *   More arrays to compare against` |
|         - | 5939 | ` * $callback` |
|         - | 5940 | ` *  The callback comparison function.` |
|         - | 5941 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5942 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5943 | ` *  than the second.` |
|         - | 5944 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5945 | ` * Return` |
|         - | 5946 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5947 | ` *  in all of the parameters. .` |
|         - | 5948 | ` * Note that NULL is returned on failure.` |
|         - | 5949 | ` */` |
|        26 | 5950 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5951 | `{` |
|         - | 5952 | `	ph7_hashmap_node *pEntry;` |
|         - | 5953 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5954 | `	ph7_value *pCallback;` |
|         - | 5955 | `	ph7_value *pArray;` |
|         - | 5956 | `	ph7_value *pVal;` |
|         - | 5957 | `	sxi32 rc;` |
|         - | 5958 | `	sxu32 n;` |
|         - | 5959 | `	int i;` |
|         - | 5960 |  |
|         - | 5961 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        31 | 5962 | `	if( nArg < 2 ){` |
|         4 | 5963 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5964 | `			"ArgumentCountError",` |
|         - | 5965 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|         1 | 5966 | `			nArg` |
|         - | 5967 | `			);` |
|         - | 5968 | `	}` |
|        29 | 5969 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5970 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5971 | `			"TypeError",` |
|         - | 5972 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5973 | `			ph7_type_name(apArg[0])` |
|         - | 5974 | `			);` |
|         - | 5975 | `	}` |
|         - | 5976 |  |
|        27 | 5977 | `	if( nArg == 2 ){` |
|         - | 5978 | `		/* Only the original array and the callback were provided. */` |
|         - | 5979 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 5980 | `		 * validation ordering. */` |
|         3 | 5981 | `	} else {` |
|         - | 5982 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 5983 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 5984 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5985 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5986 | `					"TypeError",` |
|         - | 5987 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5988 | `					i + 1,` |
|         2 | 5989 | `					ph7_type_name(apArg[i])` |
|         - | 5990 | `					);` |
|         - | 5991 | `			}` |
|        13 | 5992 | `		}` |
|         - | 5993 | `	}` |
|         - | 5994 |  |
|         - | 5995 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 5996 | `	pCallback = apArg[nArg - 1];` |
|         - | 5997 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 5998 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 5999 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 6000 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 6001 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 6002 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 6003 | `			 */` |
|         9 | 6004 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 6005 | `			if( pCb->nEntry != 2 ){` |
|         4 | 6006 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6007 | `					"TypeError",` |
|         - | 6008 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6009 | `					nArg` |
|         - | 6010 | `					);` |
|         - | 6011 | `			}` |
|         - | 6012 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6013 | `			{` |
|         6 | 6014 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6015 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6016 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6017 | `					int nMethodLen;` |
|         6 | 6018 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6019 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6020 | `					if( pClass ){` |
|         - | 6021 | `						/* Class exists but method is missing. */` |
|         4 | 6022 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6023 | `							"TypeError",` |
|         - | 6024 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6025 | `							nArg,` |
|         1 | 6026 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6027 | `							zMethod` |
|         - | 6028 | `							);` |
|         - | 6029 | `					}` |
|         - | 6030 | `					/* Class not found */` |
|         - | 6031 | `					{` |
|         - | 6032 | `						int nName;` |
|         3 | 6033 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6034 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6035 | `							"TypeError",` |
|         - | 6036 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6037 | `							nArg,` |
|         1 | 6038 | `							zName` |
|         - | 6039 | `							);` |
|         - | 6040 | `					}` |
|         - | 6041 | `				}` |
|         - | 6042 | `			}` |
|         - | 6043 | `			/* Fallback message */` |
|       ! 0 | 6044 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6045 | `				"TypeError",` |
|         - | 6046 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6047 | `				nArg` |
|         - | 6048 | `				);` |
|         - | 6049 | `		}` |
|         6 | 6050 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6051 | `			int len;` |
|         3 | 6052 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6053 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6054 | `				"TypeError",` |
|         - | 6055 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6056 | `				nArg,` |
|         1 | 6057 | `				zName` |
|         - | 6058 | `				);` |
|         - | 6059 | `		}` |
|         4 | 6060 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6061 | `			"TypeError",` |
|         - | 6062 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6063 | `			nArg` |
|         - | 6064 | `			);` |
|         - | 6065 | `	}` |
|         - | 6066 |  |
|        11 | 6067 | `	if( nArg == 2 ){` |
|         - | 6068 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6069 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6070 | `		return PH7_OK;` |
|         - | 6071 | `	}` |
|         - | 6072 |  |
|         - | 6073 | `	/* Create a new array */` |
|         7 | 6074 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6075 | `	if( pArray == 0 ){` |
|       ! 0 | 6076 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6077 | `		return PH7_OK;` |
|         - | 6078 | `	}` |
|         - | 6079 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6080 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6081 | `	/* Perform the intersection */` |
|         7 | 6082 | `	pEntry = pSrc->pFirst;` |
|         7 | 6083 | `	n = pSrc->nEntry;` |
|         7 | 6084 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6085 | `	for(;;){` |
|        19 | 6086 | `		if( n < 1 ){` |
|         5 | 6087 | `			break;` |
|         - | 6088 | `		}` |
|         - | 6089 | `		/* Extract the node value */` |
|        15 | 6090 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6091 | `		if( pVal ){` |
|        23 | 6092 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6093 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6094 | `					/* ignore */` |
|       ! 0 | 6095 | `					continue;` |
|         - | 6096 | `				}` |
|         - | 6097 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6098 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6099 | `				/* Perform the lookup */` |
|        15 | 6100 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6101 | `				if( rc != SXRET_OK ){` |
|         - | 6102 | `					/* Value does not exist */` |
|         7 | 6103 | `					break;` |
|         - | 6104 | `				}` |
|         5 | 6105 | `			}` |
|        15 | 6106 | `			if( i >= (nArg-1) ){` |
|         - | 6107 | `				/* Perform the insertion */` |
|         9 | 6108 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6109 | `			}` |
|         7 | 6110 | `		}` |
|        15 | 6111 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6112 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6113 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6114 | `			return PH7_EXCEPTION;` |
|         - | 6115 | `		}` |
|         - | 6116 | `		/* Point to the next entry */` |
|        13 | 6117 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6118 | `		n--;` |
|         1 | 6119 | `	}` |
|         - | 6120 | `	/* Return the freshly created array */` |
|         5 | 6121 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6122 | `	return PH7_OK;` |
|        18 | 6123 | `}` |
|         - | 6124 | `/*` |
|         - | 6125 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6126 | ` *  Fill an array with values.` |
|         - | 6127 | ` * Parameters` |
|         - | 6128 | ` *  $start_index` |
|         - | 6129 | ` *    The first index of the returned array.` |
|         - | 6130 | ` *  $num` |
|         - | 6131 | ` *   Number of elements to insert.` |
|         - | 6132 | ` *  $value` |
|         - | 6133 | ` *    Value to use for filling.` |
|         - | 6134 | ` * Return` |
|         - | 6135 | ` *  The filled array or null on failure.` |
|         - | 6136 | ` */` |
|       244 | 6137 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6138 | `{` |
|         - | 6139 | `	ph7_value *pArray;` |
|         - | 6140 | `	int i,nEntry;` |
|         - | 6141 |  |
|         - | 6142 | `	/* PHP enforces argument count and type checks. */` |
|       249 | 6143 | `	if( nArg != 3 ){` |
|         - | 6144 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         8 | 6145 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6146 | `			"ArgumentCountError",` |
|         - | 6147 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         2 | 6148 | `			nArg` |
|         - | 6149 | `			);` |
|         - | 6150 | `	}` |
|         - | 6151 |  |
|         - | 6152 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6153 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6154 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6155 | `	 * and NULLs are rejected outright. */` |
|       359 | 6156 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       362 | 6157 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|         4 | 6158 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6159 | `			"TypeError",` |
|         - | 6160 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|         1 | 6161 | `			ph7_type_name(apArg[0])` |
|         - | 6162 | `			);` |
|         - | 6163 | `	}` |
|       242 | 6164 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6165 | `		int len;` |
|         8 | 6166 | `		sxu8 bReal = FALSE;` |
|         8 | 6167 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6168 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6169 | `			/* Non‑numeric string is an error. */` |
|         3 | 6170 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6171 | `				"TypeError",` |
|         - | 6172 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6173 | `				);` |
|         - | 6174 | `		}` |
|         5 | 6175 | `		if( bReal ){` |
|         - | 6176 | `			/* float-string -> deprecation warning */` |
|         4 | 6177 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6178 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6179 | `				zStr` |
|         - | 6180 | `				);` |
|         1 | 6181 | `		}` |
|         2 | 6182 | `	}` |
|         - | 6183 |  |
|         - | 6184 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6185 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6186 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6187 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6188 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6189 | `			"TypeError",` |
|         - | 6190 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6191 | `			ph7_type_name(apArg[1])` |
|         - | 6192 | `			);` |
|         - | 6193 | `	}` |
|       239 | 6194 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6195 | `		int len;` |
|         3 | 6196 | `		sxu8 bReal = FALSE;` |
|         3 | 6197 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6198 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6199 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6200 | `				"TypeError",` |
|         - | 6201 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6202 | `				);` |
|         - | 6203 | `		}` |
|       ! 0 | 6204 | `	}` |
|         - | 6205 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6206 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6207 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6208 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6209 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6210 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6211 | `		if( d != (double)i64 ){` |
|         7 | 6212 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6213 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6214 | `				d` |
|         - | 6215 | `				);` |
|         2 | 6216 | `		}` |
|         2 | 6217 | `	}` |
|         - | 6218 |  |
|         - | 6219 | `	/* Total number of entries to insert */` |
|       236 | 6220 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6221 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6222 | `	if( nEntry < 0 ){` |
|         3 | 6223 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6224 | `			"ValueError",` |
|         - | 6225 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6226 | `			);` |
|         - | 6227 | `	}` |
|         - | 6228 |  |
|         - | 6229 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6230 | `	if( nEntry == 0 ){` |
|         7 | 6231 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6232 | `		return PH7_OK;` |
|         - | 6233 | `	}` |
|         - | 6234 |  |
|         - | 6235 | `	/* Create a new array */` |
|       227 | 6236 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6237 | `	if( pArray == 0 ){` |
|       ! 0 | 6238 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6239 | `	}` |
|         - | 6240 |  |
|         - | 6241 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6242 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6243 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6244 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6245 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6246 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6247 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6248 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6249 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6250 | `		}` |
|   1058803 | 6251 | `	}` |
|         - | 6252 | `	/* Return the filled array */` |
|       227 | 6253 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6254 | `	return PH7_OK;` |
|       127 | 6255 | `}` |
|         - | 6256 | `/*` |
|         - | 6257 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6258 | ` *  Fill an array with values, specifying keys.` |
|         - | 6259 | ` * Parameters` |
|         - | 6260 | ` *  $input` |
|         - | 6261 | ` *   Array of values that will be used as key.` |
|         - | 6262 | ` *  $value` |
|         - | 6263 | ` *    Value to use for filling.` |
|         - | 6264 | ` * Return` |
|         - | 6265 | ` *  The filled array.` |
|         - | 6266 | ` * Throws` |
|         - | 6267 | ` *  ValueError if $input is not an array.` |
|         - | 6268 | ` */` |
|        26 | 6269 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6270 | `{` |
|         - | 6271 | `	ph7_hashmap_node *pEntry;` |
|         - | 6272 | `	ph7_hashmap *pSrc;` |
|         - | 6273 | `	ph7_value *pArray;` |
|         - | 6274 | `	sxu32 n;` |
|         - | 6275 | `	/* PHP enforces exactly 2 arguments. */` |
|        31 | 6276 | `	if( nArg != 2 ){` |
|        12 | 6277 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6278 | `			"ArgumentCountError",` |
|         - | 6279 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         3 | 6280 | `			nArg` |
|         - | 6281 | `			);` |
|         - | 6282 | `	}` |
|         - | 6283 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6284 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6285 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6286 | `			"TypeError",` |
|         - | 6287 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6288 | `			ph7_type_name(apArg[0])` |
|         - | 6289 | `			);` |
|         - | 6290 | `	}` |
|         - | 6291 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6292 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6293 | `	/* Create a new array */` |
|        17 | 6294 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6295 | `	if( pArray == 0 ){` |
|       ! 0 | 6296 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6297 | `		return PH7_OK;` |
|         - | 6298 | `	}` |
|         - | 6299 | `	/* Perform the requested operation */` |
|        17 | 6300 | `	pEntry = pSrc->pFirst;` |
|        45 | 6301 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6302 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6303 | `		/* Point to the next entry */` |
|        29 | 6304 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6305 | `	}` |
|         - | 6306 | `	/* Return the filled array */` |
|        17 | 6307 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6308 | `	return PH7_OK;` |
|        18 | 6309 | `}` |
|         - | 6310 | `/*` |
|         - | 6311 | ` * array array_combine(array $keys,array $values)` |
|         - | 6312 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6313 | ` * Parameters` |
|         - | 6314 | ` *  $keys` |
|         - | 6315 | ` *    Array of keys to be used.` |
|         - | 6316 | ` * $values` |
|         - | 6317 | ` *   Array of values to be used.` |
|         - | 6318 | ` * Return` |
|         - | 6319 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6320 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6321 | ` *  not an array.` |
|         - | 6322 | ` */` |
|        18 | 6323 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6324 | `{` |
|         - | 6325 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6326 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6327 | `	ph7_value *pArray;` |
|         - | 6328 | `	sxu32 n;` |
|         - | 6329 | `	/* PHP enforces argument count and type checks. */` |
|        23 | 6330 | `	if( nArg != 2 ){` |
|         - | 6331 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6332 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6333 | `			"ArgumentCountError",` |
|         - | 6334 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|         1 | 6335 | `			nArg` |
|         - | 6336 | `			);` |
|         - | 6337 | `	}` |
|         - | 6338 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6339 | `	 * argument index in the error message. */` |
|        20 | 6340 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6341 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6342 | `			"TypeError",` |
|         - | 6343 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6344 | `			ph7_type_name(apArg[0])` |
|         - | 6345 | `			);` |
|         - | 6346 | `	}` |
|        17 | 6347 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6348 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6349 | `			"TypeError",` |
|         - | 6350 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6351 | `			ph7_type_name(apArg[1])` |
|         - | 6352 | `			);` |
|         - | 6353 | `	}` |
|         - | 6354 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6355 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6356 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6357 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6358 | `		/* Length mismatch -> ValueError */` |
|         3 | 6359 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6360 | `			"ValueError",` |
|         - | 6361 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6362 | `			);` |
|         - | 6363 | `	}` |
|         - | 6364 | `	/* Create a new array */` |
|        11 | 6365 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6366 | `	if( pArray == 0 ){` |
|       ! 0 | 6367 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6368 | `		return PH7_OK;` |
|         - | 6369 | `	}` |
|         - | 6370 | `	/* Perform the requested operation */` |
|        11 | 6371 | `	pKe = pKey->pFirst;` |
|        11 | 6372 | `	pVe = pValue->pFirst;` |
|        33 | 6373 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6374 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6375 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6376 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6377 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6378 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6379 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6380 | `		 * original array must not be mutated. */` |
|        23 | 6381 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6382 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6383 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6384 | `			if( pTmpKey ){` |
|         5 | 6385 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6386 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6387 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6388 | `				pKeyCopy = pTmpKey;` |
|         2 | 6389 | `			}` |
|         2 | 6390 | `		}` |
|        23 | 6391 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6392 | `		/* Point to the next entry */` |
|        23 | 6393 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6394 | `		pVe = pVe->pPrev;` |
|        12 | 6395 | `	}` |
|         - | 6396 | `	/* Return the filled array */` |
|        11 | 6397 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6398 | `	return PH7_OK;` |
|        14 | 6399 | `}` |
|         - | 6400 | `/*` |
|         - | 6401 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6402 | ` *  Return an array with elements in reverse order.` |
|         - | 6403 | ` * Parameters` |
|         - | 6404 | ` *  $array` |
|         - | 6405 | ` *   The input array.` |
|         - | 6406 | ` *  $preserve_keys (optional)` |
|         - | 6407 | ` *   If set to TRUE keys are preserved.` |
|         - | 6408 | ` * Return` |
|         - | 6409 | ` *  The reversed array.` |
|         - | 6410 | ` */` |
|        20 | 6411 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6412 | `{` |
|         - | 6413 | `	ph7_hashmap_node *pEntry;` |
|         - | 6414 | `	ph7_hashmap *pSrc;` |
|         - | 6415 | `	ph7_value *pArray;` |
|         - | 6416 | `	int bPreserve;` |
|         - | 6417 | `	sxu32 n;` |
|        23 | 6418 | `	if( nArg < 1 ){` |
|         4 | 6419 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6420 | `			"ArgumentCountError",` |
|         - | 6421 | `			"array_reverse() expects at least 1 argument, %d given",` |
|         1 | 6422 | `			nArg` |
|         - | 6423 | `			);` |
|         - | 6424 | `	}` |
|         - | 6425 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6426 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6427 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6428 | `			"TypeError",` |
|         - | 6429 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6430 | `			ph7_type_name(apArg[0])` |
|         - | 6431 | `			);` |
|         - | 6432 | `	}` |
|        17 | 6433 | `	bPreserve = FALSE;` |
|        17 | 6434 | `	if( nArg > 1 ){` |
|         7 | 6435 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6436 | `	}` |
|         - | 6437 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6438 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6439 | `	/* Create a new array */` |
|        17 | 6440 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6441 | `	if( pArray == 0 ){` |
|       ! 0 | 6442 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6443 | `		return PH7_OK;` |
|         - | 6444 | `	}` |
|         - | 6445 | `	/* Perform the requested operation */` |
|        17 | 6446 | `	pEntry = pSrc->pLast;` |
|        55 | 6447 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6448 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6449 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6450 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6451 | `		/* Point to the previous entry */` |
|        39 | 6452 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6453 | `	}` |
|        17 | 6454 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6455 | `	return PH7_OK;` |
|        13 | 6456 | `}` |
|         - | 6457 | `/*` |
|         - | 6458 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6459 | ` *  Removes duplicate values from an array.` |
|         - | 6460 | ` * Parameters` |
|         - | 6461 | ` *  $array` |
|         - | 6462 | ` *   The input array.` |
|         - | 6463 | ` *  $flags` |
|         - | 6464 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6465 | ` *   behavior using these values:` |
|         - | 6466 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6467 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6468 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6469 | ` * Return` |
|         - | 6470 | ` *  The filtered array.` |
|         - | 6471 | ` */` |
|        24 | 6472 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6473 | `{` |
|         - | 6474 | `	ph7_hashmap_node *pEntry;` |
|         - | 6475 | `	ph7_value *pNeedle;` |
|         - | 6476 | `	ph7_hashmap *pSrc;` |
|         - | 6477 | `	ph7_value *pArray;` |
|         - | 6478 | `	int bStrict;` |
|         - | 6479 | `	sxi32 rc;` |
|         - | 6480 | `	sxu32 n;` |
|        28 | 6481 | `	if( nArg < 1 ){` |
|         - | 6482 | `		/* Missing arguments, throw ArgumentCountError */` |
|         3 | 6483 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6484 | `			"ArgumentCountError",` |
|         - | 6485 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6486 | `			);` |
|         - | 6487 | `	}` |
|        25 | 6488 | `	if( nArg > 2 ){` |
|         - | 6489 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6490 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6491 | `			"ArgumentCountError",` |
|         - | 6492 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6493 | `			nArg` |
|         - | 6494 | `			);` |
|         - | 6495 | `	}` |
|         - | 6496 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6497 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6498 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6499 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6500 | `			"TypeError",` |
|         - | 6501 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6502 | `			ph7_type_name(apArg[0])` |
|         - | 6503 | `			);` |
|         - | 6504 | `	}` |
|        19 | 6505 | `	bStrict = FALSE;` |
|         - | 6506 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6507 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6508 | `	/* Create a new array */` |
|        19 | 6509 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6510 | `	if( pArray == 0 ){` |
|       ! 0 | 6511 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6512 | `		return PH7_OK;` |
|         - | 6513 | `	}` |
|         - | 6514 | `	/* Perform the requested operation */` |
|        19 | 6515 | `	pEntry = pSrc->pFirst;` |
|        83 | 6516 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6517 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6518 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6519 | `		if( pNeedle ){` |
|        65 | 6520 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6521 | `		}` |
|        65 | 6522 | `		if( rc != SXRET_OK ){` |
|         - | 6523 | `			/* Perform the insertion */` |
|        37 | 6524 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6525 | `		}` |
|         - | 6526 | `		/* Point to the next entry */` |
|        65 | 6527 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6528 | `	}` |
|         - | 6529 | `	/* Return the freshly created array */` |
|        19 | 6530 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6531 | `	return PH7_OK;` |
|        16 | 6532 | `}` |
|         - | 6533 | `/*` |
|         - | 6534 | ` * array array_flip(array $input)` |
|         - | 6535 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6536 | ` * Parameter` |
|         - | 6537 | ` *  $input` |
|         - | 6538 | ` *   Input array.` |
|         - | 6539 | ` * Return` |
|         - | 6540 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6541 | ` */` |
|        34 | 6542 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6543 | `{` |
|         - | 6544 | `	ph7_hashmap_node *pEntry;` |
|         - | 6545 | `	ph7_hashmap *pSrc;` |
|         - | 6546 | `	ph7_value *pArray;` |
|         - | 6547 | `	ph7_value *pKey;` |
|         - | 6548 | `	ph7_value sVal;` |
|         - | 6549 | `	sxu32 n;` |
|         - | 6550 |  |
|         - | 6551 | `	/* PHP requires exactly one argument */` |
|        39 | 6552 | `	if( nArg != 1 ){` |
|         - | 6553 | `		/* Use ArgumentCountError like other array helpers */` |
|         8 | 6554 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6555 | `			"ArgumentCountError",` |
|         - | 6556 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         2 | 6557 | `			nArg` |
|         - | 6558 | `			);` |
|         - | 6559 | `	}` |
|         - | 6560 | `	/* Make sure we are dealing with a valid hashmap */` |
|        33 | 6561 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6562 | `		/* Type mismatch -> TypeError */` |
|         8 | 6563 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6564 | `			"TypeError",` |
|         - | 6565 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6566 | `			ph7_type_name(apArg[0])` |
|         - | 6567 | `			);` |
|         - | 6568 | `	}` |
|         - | 6569 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6570 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6571 | `	/* Create a new array */` |
|        27 | 6572 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6573 | `	if( pArray == 0 ){` |
|       ! 0 | 6574 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6575 | `		return PH7_OK;` |
|         - | 6576 | `	}` |
|         - | 6577 | `	/* Start processing */` |
|        27 | 6578 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6579 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6580 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6581 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6582 | `		if( pKey ){` |
|         - | 6583 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6584 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6585 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6586 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6587 | `					);` |
|     22236 | 6588 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6589 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6590 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6591 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6592 | `				}else{` |
|         - | 6593 | `					SyString sStr;` |
|      2227 | 6594 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6595 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6596 | `				}` |
|         - | 6597 | `				/* Perform the insertion */` |
|     22227 | 6598 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6599 | `				/* Safely release the value because each inserted entry` |
|         - | 6600 | `				 * has its own private copy of the value.` |
|         - | 6601 | `				 */` |
|     22227 | 6602 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6603 | `			}else{` |
|         - | 6604 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6605 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6606 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6607 | `					);` |
|         - | 6608 | `			}` |
|     11118 | 6609 | `		}` |
|         - | 6610 | `		/* Point to the next entry */` |
|     22237 | 6611 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6612 | `	}` |
|         - | 6613 | `	/* Return the freshly created array */` |
|        27 | 6614 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6615 | `	return PH7_OK;` |
|        22 | 6616 | `}` |
|         - | 6617 | `/*` |
|         - | 6618 | ` * number array_sum(array $array )` |
|         - | 6619 | ` *  Calculate the sum of values in an array.` |
|         - | 6620 | ` * Parameters` |
|         - | 6621 | ` *  $array: The input array.` |
|         - | 6622 | ` * Return` |
|         - | 6623 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6624 | ` */` |
|        24 | 6625 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6626 | `{` |
|         - | 6627 | `	ph7_hashmap_node *pEntry;` |
|         - | 6628 | `	ph7_value *pObj;` |
|        26 | 6629 | `	double dSum = 0;` |
|         - | 6630 | `	sxu32 n;` |
|        26 | 6631 | `	pEntry = pMap->pFirst;` |
|        92 | 6632 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        68 | 6633 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        68 | 6634 | `		if( pObj ){` |
|        68 | 6635 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        30 | 6636 | `				dSum += pObj->rVal;` |
|        54 | 6637 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6638 | `				dSum += (double)pObj->x.iVal;` |
|        30 | 6639 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        16 | 6640 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6641 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|         - | 6642 | `					 * resource cases below already did; only this one was silent) */` |
|         3 | 6643 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6644 | `						"Addition is not supported on type string");` |
|        14 | 6645 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6646 | `					double dv = 0;` |
|        13 | 6647 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6648 | `					dSum += dv;` |
|         8 | 6649 | `				}` |
|        12 | 6650 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6651 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6652 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6653 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6654 | `				/* php names the CLASS here, not the literal word "object" */` |
|       ! 0 | 6655 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       ! 0 | 6656 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6657 | `					"Addition is not supported on type %s",` |
|       ! 0 | 6658 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         3 | 6659 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6660 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6661 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6662 | `			}` |
|         - | 6663 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6664 | `		}` |
|         - | 6665 | `		/* Point to the next entry */` |
|        68 | 6666 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6667 | `	}` |
|         - | 6668 | `	/* Return sum */` |
|        26 | 6669 | `	ph7_result_double(pCtx,dSum);` |
|        26 | 6670 | `}` |
|       688 | 6671 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6672 | `{` |
|         - | 6673 | `	ph7_hashmap_node *pEntry;` |
|         - | 6674 | `	ph7_value *pObj;` |
|       690 | 6675 | `	sxi64 nSum = 0;` |
|         - | 6676 | `	sxu32 n;` |
|       690 | 6677 | `	pEntry = pMap->pFirst;` |
|      4702 | 6678 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4014 | 6679 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4014 | 6680 | `		if( pObj ){` |
|      4014 | 6681 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3994 | 6682 | `				nSum += pObj->x.iVal;` |
|      2018 | 6683 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        12 | 6684 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6685 | `					/* php warns and SKIPS a non-numeric string */` |
|         5 | 6686 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6687 | `						"Addition is not supported on type string");` |
|        10 | 6688 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         8 | 6689 | `					sxi64 nv = 0;` |
|         8 | 6690 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         8 | 6691 | `					nSum += nv;` |
|         5 | 6692 | `				}` |
|        17 | 6693 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 6694 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6695 | `					"array_sum(): Addition is not supported on type array");` |
|        10 | 6696 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6697 | `				/* php names the CLASS here, not the literal word "object" */` |
|         3 | 6698 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         5 | 6699 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6700 | `					"Addition is not supported on type %s",` |
|         2 | 6701 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         7 | 6702 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6703 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6704 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6705 | `			}` |
|         - | 6706 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      2006 | 6707 | `		}` |
|         - | 6708 | `		/* Point to the next entry */` |
|      4014 | 6709 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2008 | 6710 | `	}` |
|         - | 6711 | `	/* Return sum */` |
|       690 | 6712 | `	ph7_result_int64(pCtx,nSum);` |
|       690 | 6713 | `}` |
|         - | 6714 | `/* number array_sum(array $array )` |
|         - | 6715 | ` * (See block-coment above)` |
|         - | 6716 | ` */` |
|       726 | 6717 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6718 | `{` |
|         - | 6719 | `	ph7_hashmap_node *pEntry;` |
|         - | 6720 | `	ph7_hashmap *pMap;` |
|         - | 6721 | `	ph7_value *pObj;` |
|       731 | 6722 | `	int useDouble = 0;` |
|         - | 6723 | `	sxu32 n;` |
|         - | 6724 | `	/* PHP requires exactly one argument */` |
|       731 | 6725 | `	if( nArg != 1 ){` |
|         8 | 6726 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6727 | `			"ArgumentCountError",` |
|         - | 6728 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         2 | 6729 | `			nArg` |
|         - | 6730 | `			);` |
|         - | 6731 | `	}` |
|         - | 6732 | `	/* Make sure we are dealing with a valid hashmap */` |
|       725 | 6733 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6734 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6735 | `		char zBuf[64];` |
|         8 | 6736 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6737 | `			"TypeError",` |
|         - | 6738 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6739 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6740 | `			);` |
|         - | 6741 | `	}` |
|       720 | 6742 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       720 | 6743 | `	if( pMap->nEntry < 1 ){` |
|         - | 6744 | `		/* Nothing to compute,return 0 */` |
|         7 | 6745 | `		ph7_result_int(pCtx,0);` |
|         7 | 6746 | `		return PH7_OK;` |
|         - | 6747 | `	}` |
|         - | 6748 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6749 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6750 | `	 */` |
|       714 | 6751 | `	pEntry = pMap->pFirst;` |
|      4734 | 6752 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4046 | 6753 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4046 | 6754 | `		if( pObj ){` |
|      4046 | 6755 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        20 | 6756 | `				useDouble = 1;` |
|        20 | 6757 | `				break;` |
|         - | 6758 | `			}` |
|      4028 | 6759 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        18 | 6760 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        18 | 6761 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6762 | `				sxu32 i;` |
|        32 | 6763 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        22 | 6764 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6765 | `						useDouble = 1;` |
|         7 | 6766 | `						break;` |
|         - | 6767 | `					}` |
|         9 | 6768 | `				}` |
|        18 | 6769 | `				if( useDouble ){` |
|         7 | 6770 | `					break;` |
|         - | 6771 | `				}` |
|         5 | 6772 | `			}` |
|      2010 | 6773 | `		}` |
|      4022 | 6774 | `		pEntry = pEntry->pPrev;` |
|      2012 | 6775 | `	}` |
|       714 | 6776 | `	if( useDouble ){` |
|        26 | 6777 | `		DoubleSum(pCtx,pMap);` |
|        14 | 6778 | `	}else{` |
|       690 | 6779 | `		Int64Sum(pCtx,pMap);` |
|         - | 6780 | `	}` |
|       714 | 6781 | `	return PH7_OK;` |
|       368 | 6782 | `}` |
|         - | 6783 | `/*` |
|         - | 6784 | ` * number array_product(array $array )` |
|         - | 6785 | ` *  Calculate the product of values in an array.` |
|         - | 6786 | ` * Parameters` |
|         - | 6787 | ` *  $array: The input array.` |
|         - | 6788 | ` * Return` |
|         - | 6789 | ` *  Returns the product of values as an integer or float.` |
|         - | 6790 | ` */` |
|         2 | 6791 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6792 | `{` |
|         - | 6793 | `	ph7_hashmap_node *pEntry;` |
|         - | 6794 | `	ph7_value *pObj;` |
|         - | 6795 | `	double dProd;` |
|         - | 6796 | `	sxu32 n;` |
|         3 | 6797 | `	pEntry = pMap->pFirst;` |
|         3 | 6798 | `	dProd = 1;` |
|         7 | 6799 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6800 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6801 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6802 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6803 | `				dProd *= pObj->rVal;` |
|         4 | 6804 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6805 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6806 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6807 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6808 | `					double dv = 0;` |
|       ! 0 | 6809 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6810 | `					dProd *= dv;` |
|       ! 0 | 6811 | `				}` |
|       ! 0 | 6812 | `			}` |
|         2 | 6813 | `		}` |
|         - | 6814 | `		/* Point to the next entry */` |
|         5 | 6815 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6816 | `	}` |
|         - | 6817 | `	/* Return product */` |
|         3 | 6818 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6819 | `}` |
|         2 | 6820 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6821 | `{` |
|         - | 6822 | `	ph7_hashmap_node *pEntry;` |
|         - | 6823 | `	ph7_value *pObj;` |
|         - | 6824 | `	sxi64 nProd;` |
|         - | 6825 | `	sxu32 n;` |
|         3 | 6826 | `	pEntry = pMap->pFirst;` |
|         3 | 6827 | `	nProd = 1;` |
|         9 | 6828 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6829 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6830 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6831 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6832 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6833 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6834 | `				nProd *= pObj->x.iVal;` |
|         3 | 6835 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6836 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6837 | `					sxi64 nv = 0;` |
|       ! 0 | 6838 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6839 | `					nProd *= nv;` |
|       ! 0 | 6840 | `				}` |
|       ! 0 | 6841 | `			}` |
|         3 | 6842 | `		}` |
|         - | 6843 | `		/* Point to the next entry */` |
|         7 | 6844 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6845 | `	}` |
|         - | 6846 | `	/* Return product */` |
|         3 | 6847 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6848 | `}` |
|         - | 6849 | `/* number array_product(array $array )` |
|         - | 6850 | ` * (See block-block comment above)` |
|         - | 6851 | ` */` |
|        18 | 6852 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6853 | `{` |
|         - | 6854 | `	ph7_hashmap *pMap;` |
|         - | 6855 | `	ph7_value *pObj;` |
|        19 | 6856 | `	if( nArg < 1 ){` |
|         - | 6857 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6858 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6859 | `		return PH7_OK;` |
|         - | 6860 | `	}` |
|         - | 6861 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        19 | 6862 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6863 | `		char zBuf[64];` |
|        19 | 6864 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6865 | `			"TypeError",` |
|         - | 6866 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 6867 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6868 | `			);` |
|         - | 6869 | `	}` |
|         7 | 6870 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6871 | `	if( pMap->nEntry < 1 ){` |
|         - | 6872 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6873 | `		ph7_result_int(pCtx,1);` |
|         3 | 6874 | `		return PH7_OK;` |
|         - | 6875 | `	}` |
|         - | 6876 | `	/* If the first element is of type float,then perform floating` |
|         - | 6877 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6878 | `	 */` |
|         5 | 6879 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6880 | `	if( pObj == 0 ){` |
|       ! 0 | 6881 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6882 | `		return PH7_OK;` |
|         - | 6883 | `	}` |
|         5 | 6884 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6885 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6886 | `	}else{` |
|         3 | 6887 | `		Int64Prod(pCtx,pMap);` |
|         - | 6888 | `	}` |
|         5 | 6889 | `	return PH7_OK;` |
|        10 | 6890 | `}` |
|         - | 6891 | `/*` |
|         - | 6892 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6893 | ` *  Pick one or more random entries out of an array.` |
|         - | 6894 | ` * Parameters` |
|         - | 6895 | ` * $input` |
|         - | 6896 | ` *  The input array.` |
|         - | 6897 | ` * $num_req` |
|         - | 6898 | ` *  Specifies how many entries you want to pick.` |
|         - | 6899 | ` * Return` |
|         - | 6900 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6901 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6902 | ` *  NULL is returned on failure.` |
|         - | 6903 | ` */` |
|        42 | 6904 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6905 | `{` |
|         - | 6906 | `	ph7_hashmap_node *pNode;` |
|         - | 6907 | `	ph7_hashmap *pMap;` |
|        43 | 6908 | `	int nItem = 1;` |
|        43 | 6909 | `	if( nArg < 1 ){` |
|         - | 6910 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6911 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6912 | `		return PH7_OK;` |
|         - | 6913 | `	}` |
|         - | 6914 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        43 | 6915 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6916 | `		char zBuf[64];` |
|        10 | 6917 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6918 | `			"TypeError",` |
|         - | 6919 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6920 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6921 | `			);` |
|         - | 6922 | `	}` |
|         - | 6923 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6924 | `	 * check, matching its ZPP-before-body ordering. */` |
|        37 | 6925 | `	if( nArg > 1 ){` |
|        29 | 6926 | `		ph7_value *pNum = apArg[1];` |
|        28 | 6927 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        24 | 6928 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6929 | `			char zBuf[64];` |
|        10 | 6930 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6931 | `				"TypeError",` |
|         - | 6932 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|         3 | 6933 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6934 | `				);` |
|         - | 6935 | `		}` |
|        23 | 6936 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6937 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6938 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6939 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6940 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6941 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6942 | `			int len;` |
|         9 | 6943 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6944 | `			sxi64 iLong; double dReal;` |
|         9 | 6945 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6946 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6947 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6948 | `					"TypeError",` |
|         - | 6949 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6950 | `					);` |
|         - | 6951 | `			}` |
|         - | 6952 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6953 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6954 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6955 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6956 | `			}` |
|         3 | 6957 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6958 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6959 | `			nItem = (int)iLong;` |
|         2 | 6960 | `		}else{` |
|        15 | 6961 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6962 | `		}` |
|         8 | 6963 | `	}` |
|         - | 6964 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6965 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6966 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6967 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6968 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6969 | `			"ValueError",` |
|         - | 6970 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 6971 | `			);` |
|         - | 6972 | `	}` |
|         - | 6973 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 6974 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 6975 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6976 | `			"ValueError",` |
|         - | 6977 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 6978 | `			);` |
|         - | 6979 | `	}` |
|        13 | 6980 | `	if( nItem < 2 ){` |
|         - | 6981 | `		sxu32 nEntry;` |
|         - | 6982 | `		/* Select a random number */` |
|         9 | 6983 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 6984 | `		/* Extract the desired entry.` |
|         - | 6985 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 6986 | `		 */` |
|         9 | 6987 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         3 | 6988 | `			pNode = pMap->pLast;` |
|         3 | 6989 | `			nEntry = pMap->nEntry - nEntry;` |
|         3 | 6990 | `			if( nEntry > 1 ){` |
|       ! 0 | 6991 | `				for(;;){` |
|       ! 0 | 6992 | `					if( nEntry == 0 ){` |
|       ! 0 | 6993 | `						break;` |
|         - | 6994 | `					}` |
|         - | 6995 | `					/* Point to the previous entry */` |
|       ! 0 | 6996 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 6997 | `					nEntry--;` |
|       ! 0 | 6998 | `				}` |
|       ! 0 | 6999 | `			}` |
|         3 | 7000 | `		}else{` |
|         7 | 7001 | `			pNode = pMap->pFirst;` |
|         3 | 7002 | `			for(;;){` |
|        11 | 7003 | `				if( nEntry == 0 ){` |
|         7 | 7004 | `					break;` |
|         - | 7005 | `				}` |
|         - | 7006 | `				/* Point to the next entry */` |
|         5 | 7007 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         5 | 7008 | `				nEntry--;` |
|         1 | 7009 | `			}` |
|         - | 7010 | `		}` |
|         9 | 7011 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 7012 | `			/* Int key */` |
|         7 | 7013 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 7014 | `		}else{` |
|         - | 7015 | `			/* Blob key */` |
|         3 | 7016 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 7017 | `		}` |
|         5 | 7018 | `	}else{` |
|         - | 7019 | `		ph7_value sKey,*pArray;` |
|         - | 7020 | `		ph7_hashmap *pDest;` |
|         - | 7021 | `		/* Create a new array */` |
|         5 | 7022 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7023 | `		if( pArray == 0 ){` |
|       ! 0 | 7024 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7025 | `			return PH7_OK;` |
|         - | 7026 | `		}` |
|         - | 7027 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7028 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7029 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7030 | `		/* Copy the first n items */` |
|         5 | 7031 | `		pNode = pMap->pFirst;` |
|         5 | 7032 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7033 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7034 | `		}` |
|        15 | 7035 | `		while( nItem > 0){` |
|        11 | 7036 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7037 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7038 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7039 | `			/* Point to the next entry */` |
|        11 | 7040 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7041 | `			nItem--;` |
|         1 | 7042 | `		}` |
|         - | 7043 | `		/* Shuffle the array */` |
|         5 | 7044 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7045 | `		/* Rehash node */` |
|         5 | 7046 | `		HashmapSortRehash(pDest);` |
|         - | 7047 | `		/* Return the random array */` |
|         5 | 7048 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7049 | `	}` |
|        13 | 7050 | `	return PH7_OK;` |
|        22 | 7051 | `}` |
|         - | 7052 | `/*` |
|         - | 7053 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7054 | ` *  Split an array into chunks.` |
|         - | 7055 | ` * Parameters` |
|         - | 7056 | ` * $input` |
|         - | 7057 | ` *   The array to work on` |
|         - | 7058 | ` * $size` |
|         - | 7059 | ` *   The size of each chunk` |
|         - | 7060 | ` * $preserve_keys` |
|         - | 7061 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7062 | ` *   the chunk numerically.` |
|         - | 7063 | ` * Return` |
|         - | 7064 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7065 | ` *  zero, with each dimension containing size elements.` |
|         - | 7066 | ` */` |
|        42 | 7067 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7068 | `{` |
|         - | 7069 | `	ph7_value *pArray,*pChunk;` |
|         - | 7070 | `	ph7_hashmap_node *pEntry;` |
|         - | 7071 | `	ph7_hashmap *pMap;` |
|         - | 7072 | `	int bPreserve;` |
|         - | 7073 | `	sxu32 nChunk;` |
|         - | 7074 | `	sxu32 nSize;` |
|         - | 7075 | `	sxu32 n;` |
|         - | 7076 | `	/* Argument count and types follow PHP semantics. */` |
|        47 | 7077 | `	if( nArg < 2 ){` |
|         - | 7078 | `		/* fewer than required arguments -> ArgumentCountError */` |
|         4 | 7079 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7080 | `			"ArgumentCountError",` |
|         - | 7081 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|         1 | 7082 | `			nArg` |
|         - | 7083 | `			);` |
|         - | 7084 | `	}` |
|        45 | 7085 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7086 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7087 | `			"TypeError",` |
|         - | 7088 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7089 | `			ph7_type_name(apArg[0])` |
|         - | 7090 | `			);` |
|         - | 7091 | `	}` |
|         - | 7092 | `	/* Create a new array */` |
|        43 | 7093 | `	pArray = ph7_context_new_array(pCtx);` |
|        43 | 7094 | `	if( pArray == 0 ){` |
|       ! 0 | 7095 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7096 | `		return PH7_OK;` |
|         - | 7097 | `	}` |
|         - | 7098 | `	/* Point to the internal representation of the input hashmap */` |
|        43 | 7099 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7100 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7101 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        57 | 7102 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        81 | 7103 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        38 | 7104 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7105 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7106 | `			"TypeError",` |
|         - | 7107 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7108 | `			ph7_type_name(apArg[1])` |
|         - | 7109 | `			);` |
|         - | 7110 | `	}` |
|         - | 7111 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7112 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7113 | `	 * precision and PHP emits a deprecation warning. */` |
|        43 | 7114 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7115 | `		int len;` |
|         3 | 7116 | `		sxu8 bReal = FALSE;` |
|         3 | 7117 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7118 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7119 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7120 | `				"TypeError",` |
|         - | 7121 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7122 | `				);` |
|         - | 7123 | `		}` |
|       ! 0 | 7124 | `		if( bReal ){` |
|         - | 7125 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7126 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7127 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7128 | `				zStr` |
|         - | 7129 | `				);` |
|       ! 0 | 7130 | `		}` |
|       ! 0 | 7131 | `	}` |
|         - | 7132 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7133 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7134 | `	 * later via ph7_value_to_int. */` |
|        40 | 7135 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7136 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7137 | `		sxi64 i = (sxi64)d;` |
|         3 | 7138 | `		if( d != (double)i ){` |
|         4 | 7139 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7140 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7141 | `				d` |
|         - | 7142 | `				);` |
|         1 | 7143 | `		}` |
|         1 | 7144 | `	}` |
|         - | 7145 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7146 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7147 | `	{` |
|        40 | 7148 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        40 | 7149 | `		if( nSizeSigned < 1 ){` |
|         - | 7150 | `			/* size <= 0 -> ValueError */` |
|         6 | 7151 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7152 | `				"ValueError",` |
|         - | 7153 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7154 | `				);` |
|         - | 7155 | `		}` |
|        35 | 7156 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7157 | `	}` |
|        35 | 7158 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7159 | `		/* Return the whole array */` |
|         3 | 7160 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7161 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7162 | `		return PH7_OK;` |
|         - | 7163 | `	}` |
|        33 | 7164 | `	bPreserve = 0;` |
|        33 | 7165 | `	if( nArg > 2 ){` |
|         - | 7166 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7167 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7168 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7169 | `		 * normally, matching PHP behaviour. */` |
|        35 | 7170 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        35 | 7171 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7172 | `			ph7_value_is_resource(apArg[2]) ){` |
|         8 | 7173 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7174 | `				"TypeError",` |
|         - | 7175 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|         4 | 7176 | `				ph7_type_name(apArg[2])` |
|         - | 7177 | `				);` |
|         - | 7178 | `		}` |
|        21 | 7179 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7180 | `	}` |
|         - | 7181 | `	/* Start processing */` |
|        27 | 7182 | `	pEntry = pMap->pFirst;` |
|        27 | 7183 | `	nChunk = 0;` |
|        27 | 7184 | `	pChunk = 0;` |
|        27 | 7185 | `	n = pMap->nEntry;` |
|        56 | 7186 | `	for( ;; ){` |
|       113 | 7187 | `		if( n < 1 ){` |
|         - | 7188 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7189 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7190 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7191 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7192 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7193 | `			 * exists. */` |
|        27 | 7194 | `			if( pChunk ){` |
|        27 | 7195 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7196 | `			}` |
|        27 | 7197 | `			break;` |
|         - | 7198 | `		}` |
|        87 | 7199 | `		if( nChunk < 1 ){` |
|        71 | 7200 | `			if( pChunk ){` |
|         - | 7201 | `				/* Put the first chunk */` |
|        45 | 7202 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7203 | `			}` |
|         - | 7204 | `			/* Create a new dimension */` |
|        71 | 7205 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7206 | `												   * will be automatically released as soon we return` |
|         - | 7207 | `												   * from this function */` |
|        71 | 7208 | `			if( pChunk == 0 ){` |
|       ! 0 | 7209 | `				break;` |
|         - | 7210 | `			}` |
|        71 | 7211 | `			nChunk = nSize;` |
|        35 | 7212 | `		}` |
|         - | 7213 | `		/* Insert the entry */` |
|        87 | 7214 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7215 | `		/* Point to the next entry */` |
|        87 | 7216 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7217 | `		nChunk--;` |
|        87 | 7218 | `		n--;` |
|         1 | 7219 | `	}` |
|         - | 7220 | `	/* Return the multidimensional array */` |
|        27 | 7221 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7222 | `	return PH7_OK;` |
|        26 | 7223 | `}` |
|         - | 7224 | `/*` |
|         - | 7225 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7226 | ` *  Pad array to the specified length with a value.` |
|         - | 7227 | ` * $input` |
|         - | 7228 | ` *   Initial array of values to pad.` |
|         - | 7229 | ` * $pad_size` |
|         - | 7230 | ` *   New size of the array.` |
|         - | 7231 | ` * $pad_value` |
|         - | 7232 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7233 | ` */` |
|         - | 7234 | `/*` |
|         - | 7235 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7236 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7237 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7238 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7239 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7240 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7241 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7242 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7243 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7244 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7245 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7246 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7247 | ` */` |
|        50 | 7248 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7249 | `	ph7_context *pCtx,` |
|         - | 7250 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7251 | `	int iArg,              /* 1-based argument position */` |
|         - | 7252 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7253 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7254 | `	)` |
|         1 | 7255 | `{` |
|        51 | 7256 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7257 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7258 | `			"ValueError",` |
|         - | 7259 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7260 | `			zFunc,iArg,zParam` |
|         - | 7261 | `			);` |
|         - | 7262 | `	}` |
|        37 | 7263 | `	return SXRET_OK;` |
|        26 | 7264 | `}` |
|        72 | 7265 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7266 | `{` |
|         - | 7267 | `	ph7_hashmap *pMap;` |
|         - | 7268 | `	ph7_value *pArray;` |
|         - | 7269 | `	sxi64 iLen,iAbs;` |
|         - | 7270 | `	int nEntry;` |
|         - | 7271 | `	sxi32 rc;` |
|        77 | 7272 | `	if( nArg != 3 ){` |
|        12 | 7273 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7274 | `			"ArgumentCountError",` |
|         - | 7275 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         3 | 7276 | `			nArg` |
|         - | 7277 | `			);` |
|         - | 7278 | `	}` |
|        68 | 7279 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7280 | `		char zBuf[64];` |
|        14 | 7281 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7282 | `			"TypeError",` |
|         - | 7283 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 7284 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7285 | `			);` |
|         - | 7286 | `	}` |
|         - | 7287 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7288 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7289 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7290 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        58 | 7291 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        56 | 7292 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7293 | `		char zBuf[64];` |
|         7 | 7294 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7295 | `			"TypeError",` |
|         - | 7296 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|         4 | 7297 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7298 | `			);` |
|         - | 7299 | `	}` |
|        55 | 7300 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7301 | `		int nStr;` |
|        11 | 7302 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7303 | `		sxi64 iLong; double dReal;` |
|        11 | 7304 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7305 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7306 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7307 | `				"TypeError",` |
|         - | 7308 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7309 | `				);` |
|         - | 7310 | `		}` |
|         7 | 7311 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7312 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7313 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7314 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7315 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7316 | `					"TypeError",` |
|         - | 7317 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7318 | `					);` |
|         - | 7319 | `			}` |
|         3 | 7320 | `			iLen = (sxi64)dReal;` |
|         3 | 7321 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7322 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7323 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7324 | `					zStr` |
|         - | 7325 | `					);` |
|       ! 0 | 7326 | `			}` |
|         2 | 7327 | `		}else{` |
|         5 | 7328 | `			iLen = iLong;` |
|         - | 7329 | `		}` |
|         4 | 7330 | `	}else{` |
|        45 | 7331 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7332 | `	}` |
|         - | 7333 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7334 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7335 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7336 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7337 | `	 * overflow). */` |
|        51 | 7338 | `	iAbs = iLen;` |
|        51 | 7339 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7340 | `		iAbs = -iAbs;` |
|         7 | 7341 | `	}` |
|        51 | 7342 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7343 | `	if( rc != SXRET_OK ){` |
|        15 | 7344 | `		return rc;` |
|         - | 7345 | `	}` |
|        37 | 7346 | `	nEntry = (int)iLen;` |
|         - | 7347 | `	/* Create a new array */` |
|        37 | 7348 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7349 | `	if( pArray == 0 ){` |
|       ! 0 | 7350 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7351 | `	}` |
|        37 | 7352 | `	if( nEntry < 0 ){` |
|        11 | 7353 | `		nEntry = -nEntry;` |
|        11 | 7354 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7355 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7356 | `			/* Insert given items first */` |
|        25 | 7357 | `			while( nEntry > 0 ){` |
|        19 | 7358 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7359 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7360 | `				}` |
|        19 | 7361 | `				nEntry--;` |
|         1 | 7362 | `			}` |
|         - | 7363 | `			/* Merge the two arrays */` |
|         7 | 7364 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7365 | `		}else{` |
|         5 | 7366 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7367 | `		}` |
|        32 | 7368 | `	}else if( nEntry > 0 ){` |
|        25 | 7369 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7370 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7371 | `			/* Merge the two arrays first */` |
|        19 | 7372 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7373 | `			/* Insert given items */` |
|       275 | 7374 | `			while( nEntry > 0 ){` |
|       257 | 7375 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7376 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7377 | `				}` |
|       257 | 7378 | `				nEntry--;` |
|         1 | 7379 | `			}` |
|        10 | 7380 | `		}else{` |
|         7 | 7381 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7382 | `		}` |
|        13 | 7383 | `	}else{` |
|         - | 7384 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7385 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7386 | `	}` |
|         - | 7387 | `	/* Return the new array */` |
|        37 | 7388 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7389 | `	return PH7_OK;` |
|        41 | 7390 | `}` |
|         - | 7391 | `/*` |
|         - | 7392 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7393 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7394 | ` * Parameters` |
|         - | 7395 | ` * $array` |
|         - | 7396 | ` *   The array in which elements are replaced.` |
|         - | 7397 | ` * $array1` |
|         - | 7398 | ` *   The array from which elements will be extracted.` |
|         - | 7399 | ` * ....` |
|         - | 7400 | ` *  More arrays from which elements will be extracted.` |
|         - | 7401 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7402 | ` * Return` |
|         - | 7403 | ` *  Returns an array.` |
|         - | 7404 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7405 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7406 | ` */` |
|        22 | 7407 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7408 | `{` |
|         - | 7409 | `	ph7_hashmap *pMap;` |
|         - | 7410 | `	ph7_value *pArray;` |
|         - | 7411 | `	int i;` |
|        26 | 7412 | `	if( nArg < 1 ){` |
|         3 | 7413 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7414 | `			"ArgumentCountError",` |
|         - | 7415 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7416 | `			);` |
|         - | 7417 | `	}` |
|        23 | 7418 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7419 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7420 | `			"TypeError",` |
|         - | 7421 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7422 | `			ph7_type_name(apArg[0])` |
|         - | 7423 | `			);` |
|         - | 7424 | `	}` |
|         - | 7425 | `	/* Create a new array */` |
|        20 | 7426 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7427 | `	if( pArray == 0 ){` |
|       ! 0 | 7428 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7429 | `		return PH7_OK;` |
|         - | 7430 | `	}` |
|         - | 7431 | `	/* Overwrite from the first array */` |
|        20 | 7432 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7433 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7434 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7435 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7436 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7437 | `			/* Type mismatch -> TypeError */` |
|         4 | 7438 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7439 | `				"TypeError",` |
|         - | 7440 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7441 | `				i + 1,` |
|         2 | 7442 | `				ph7_type_name(apArg[i])` |
|         - | 7443 | `				);` |
|         - | 7444 | `		}` |
|         - | 7445 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7446 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7447 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7448 | `	}` |
|         - | 7449 | `	/* Return the new array */` |
|        17 | 7450 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7451 | `	return PH7_OK;` |
|        15 | 7452 | `}` |
|         - | 7453 | `/*` |
|         - | 7454 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7455 | ` *  Filters elements of an array using a callback function.` |
|         - | 7456 | ` * Parameters` |
|         - | 7457 | ` *  $input` |
|         - | 7458 | ` *    The array to iterate over` |
|         - | 7459 | ` * $callback` |
|         - | 7460 | ` *    The callback function to use` |
|         - | 7461 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7462 | ` *    will be removed.` |
|         - | 7463 | ` * Return` |
|         - | 7464 | ` *  The filtered array.` |
|         - | 7465 | ` */` |
|        32 | 7466 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7467 | `{` |
|         - | 7468 | `	ph7_hashmap_node *pEntry;` |
|         - | 7469 | `	ph7_hashmap *pMap;` |
|         - | 7470 | `	ph7_value *pArray;` |
|         - | 7471 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7472 | `	ph7_value *pValue;` |
|         - | 7473 | `	sxi32 rc;` |
|         - | 7474 | `	int keep;` |
|         - | 7475 | `	sxu32 n;` |
|        34 | 7476 | `	if( nArg < 1 ){` |
|         - | 7477 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7478 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7479 | `		return PH7_OK;` |
|         - | 7480 | `	}` |
|         - | 7481 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        34 | 7482 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7483 | `		char zBuf[64];` |
|        22 | 7484 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7485 | `			"TypeError",` |
|         - | 7486 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         7 | 7487 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7488 | `			);` |
|         - | 7489 | `	}` |
|         - | 7490 | `	/* Create a new array */` |
|        20 | 7491 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7492 | `	if( pArray == 0 ){` |
|       ! 0 | 7493 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7494 | `		return PH7_OK;` |
|         - | 7495 | `	}` |
|         - | 7496 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7497 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7498 | `	pEntry = pMap->pFirst;` |
|        20 | 7499 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7500 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7501 | `	/* Perform the requested operation */` |
|        78 | 7502 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7503 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7504 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7505 | `		if( pValue == 0 ){` |
|         - | 7506 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7507 | `			keep = FALSE;` |
|        64 | 7508 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7509 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7510 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7511 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7512 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7513 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7514 | `					int len;` |
|         3 | 7515 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7516 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7517 | `						"TypeError",` |
|         - | 7518 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7519 | `						zName` |
|         - | 7520 | `						);` |
|       ! 0 | 7521 | `				}else{` |
|       ! 0 | 7522 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7523 | `						"TypeError",` |
|         - | 7524 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7525 | `						ph7_type_name(apArg[1])` |
|         - | 7526 | `						);` |
|         - | 7527 | `				}` |
|         - | 7528 | `			}` |
|        33 | 7529 | `			keep = FALSE;` |
|        33 | 7530 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7531 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7532 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7533 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7534 | `				return PH7_EXCEPTION;` |
|         - | 7535 | `			}` |
|        31 | 7536 | `			if( rc == SXRET_OK ){` |
|         - | 7537 | `				/* Perform a boolean cast */` |
|        31 | 7538 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7539 | `			}` |
|        31 | 7540 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7541 | `		}else{` |
|         - | 7542 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7543 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7544 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7545 | `			 */` |
|        29 | 7546 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7547 | `		}` |
|        59 | 7548 | `		if( keep ){` |
|         - | 7549 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7550 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7551 | `		}` |
|         - | 7552 | `		/* Point to the next entry */` |
|        59 | 7553 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7554 | `	}` |
|        15 | 7555 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7556 | `	return PH7_OK;` |
|        18 | 7557 | `}` |
|         - | 7558 | `/*` |
|         - | 7559 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7560 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7561 | ` * Parameters` |
|         - | 7562 | ` *  $callback` |
|         - | 7563 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7564 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7565 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7566 | ` *   are zipped together.` |
|         - | 7567 | ` *  $array` |
|         - | 7568 | ` *   The first array to run through the callback function.` |
|         - | 7569 | ` *  $arrays` |
|         - | 7570 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7571 | ` * Return` |
|         - | 7572 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7573 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7574 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7575 | ` *  padding shorter arrays with NULL.` |
|         - | 7576 | ` */` |
|        62 | 7577 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7578 | `{` |
|         - | 7579 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7580 | `	ph7_hashmap_node *pEntry;` |
|         - | 7581 | `	ph7_hashmap *pMap;` |
|         - | 7582 | `	ph7_vm *pVm;` |
|         - | 7583 | `	int bNullCallback;` |
|         - | 7584 | `	sxi32 rc;` |
|         - | 7585 | `	int i;` |
|         - | 7586 | `	sxu32 n;` |
|        67 | 7587 | `	if( nArg < 2 ){` |
|         8 | 7588 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7589 | `			"ArgumentCountError",` |
|         - | 7590 | `			"array_map() expects at least 2 arguments, %d given",` |
|         2 | 7591 | `			nArg` |
|         - | 7592 | `			);` |
|         - | 7593 | `	}` |
|        62 | 7594 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        62 | 7595 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7596 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7597 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7598 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7599 | `				"TypeError",` |
|         - | 7600 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7601 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7602 | `				zFunc` |
|         - | 7603 | `				);` |
|         - | 7604 | `		}` |
|         3 | 7605 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7606 | `			"TypeError",` |
|         - | 7607 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7608 | `			"no array or string given"` |
|         - | 7609 | `			);` |
|         - | 7610 | `	}` |
|         - | 7611 | `	/* Every remaining argument must be an array */` |
|       121 | 7612 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        69 | 7613 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7614 | `			if( i == 1 ){` |
|         4 | 7615 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7616 | `					"TypeError",` |
|         - | 7617 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7618 | `					ph7_type_name(apArg[1])` |
|         - | 7619 | `					);` |
|         - | 7620 | `			}` |
|       ! 0 | 7621 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7622 | `				"TypeError",` |
|         - | 7623 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7624 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7625 | `				);` |
|         - | 7626 | `		}` |
|        34 | 7627 | `	}` |
|        54 | 7628 | `	pVm = pCtx->pVm;` |
|         - | 7629 | `	/* Create a new array */` |
|        54 | 7630 | `	pArray = ph7_context_new_array(pCtx);` |
|        54 | 7631 | `	if( pArray == 0 ){` |
|       ! 0 | 7632 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7633 | `		return PH7_OK;` |
|         - | 7634 | `	}` |
|        54 | 7635 | `	PH7_MemObjInit(pVm,&sResult);` |
|        54 | 7636 | `	PH7_MemObjInit(pVm,&sKey);` |
|        54 | 7637 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7638 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7639 | `	if( nArg == 2 ){` |
|         - | 7640 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        44 | 7641 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        44 | 7642 | `		pEntry = pMap->pFirst;` |
|       134 | 7643 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7644 | `			/* Extract the node value */` |
|        96 | 7645 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        96 | 7646 | `			if( pValue ){` |
|         - | 7647 | `				/* Extract the node key */` |
|        96 | 7648 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        96 | 7649 | `				if( bNullCallback ){` |
|         - | 7650 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7651 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7652 | `				}else{` |
|         - | 7653 | `					/* Invoke the supplied callback */` |
|        86 | 7654 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        86 | 7655 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7656 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7657 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7658 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7659 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7660 | `						return PH7_EXCEPTION;` |
|         - | 7661 | `					}` |
|         - | 7662 | `					/* Insert the callback return value */` |
|        82 | 7663 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7664 | `				}` |
|        92 | 7665 | `				PH7_MemObjRelease(&sKey);` |
|        92 | 7666 | `				PH7_MemObjRelease(&sResult);` |
|        45 | 7667 | `			}` |
|         - | 7668 | `			/* Point to the next entry */` |
|        92 | 7669 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        47 | 7670 | `		}` |
|        21 | 7671 | `	}else{` |
|         - | 7672 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7673 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7674 | `		int nArrays = nArg - 1;` |
|         - | 7675 | `		ph7_hashmap_node **apCur;` |
|         - | 7676 | `		ph7_value **apCallArg;` |
|         - | 7677 | `		ph7_value sNull;` |
|        11 | 7678 | `		sxu32 nMax = 0;` |
|        11 | 7679 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7680 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7681 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7682 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7683 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7684 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7685 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7686 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7687 | `			return PH7_OK;` |
|         - | 7688 | `		}` |
|        11 | 7689 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7690 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7691 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7692 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7693 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7694 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7695 | `				nMax = pMap->nEntry;` |
|         6 | 7696 | `			}` |
|        12 | 7697 | `		}` |
|        35 | 7698 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7699 | `			ph7_value *pZip = 0;` |
|        25 | 7700 | `			if( bNullCallback ){` |
|         - | 7701 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7702 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7703 | `			}` |
|        79 | 7704 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7705 | `				ph7_value *pv = &sNull;` |
|        55 | 7706 | `				if( apCur[i] ){` |
|        53 | 7707 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7708 | `					if( pNodeVal ){` |
|        53 | 7709 | `						pv = pNodeVal;` |
|        26 | 7710 | `					}` |
|        53 | 7711 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7712 | `				}` |
|        55 | 7713 | `				if( bNullCallback ){` |
|         9 | 7714 | `					if( pZip ){` |
|         9 | 7715 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7716 | `					}` |
|         5 | 7717 | `				}else{` |
|        47 | 7718 | `					apCallArg[i] = pv;` |
|         - | 7719 | `				}` |
|        28 | 7720 | `			}` |
|        25 | 7721 | `			if( bNullCallback ){` |
|         5 | 7722 | `				if( pZip ){` |
|         5 | 7723 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7724 | `				}` |
|         3 | 7725 | `			}else{` |
|        21 | 7726 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7727 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7728 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7729 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7730 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7731 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7732 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7733 | `					return PH7_EXCEPTION;` |
|         - | 7734 | `				}` |
|        21 | 7735 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7736 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7737 | `			}` |
|        13 | 7738 | `		}` |
|        11 | 7739 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7740 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7741 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7742 | `	}` |
|        50 | 7743 | `	PH7_MemObjRelease(&sKey);` |
|        50 | 7744 | `	PH7_MemObjRelease(&sResult);` |
|        50 | 7745 | `	ph7_result_value(pCtx,pArray);` |
|        50 | 7746 | `	return PH7_OK;` |
|        36 | 7747 | `}` |
|         - | 7748 | `/*` |
|         - | 7749 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7750 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7751 | ` * Parameters` |
|         - | 7752 | ` *  $array` |
|         - | 7753 | ` *   The input array.` |
|         - | 7754 | ` *  $callback` |
|         - | 7755 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7756 | ` *  $initial` |
|         - | 7757 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7758 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7759 | ` * Return` |
|         - | 7760 | ` *  Returns the resulting value.` |
|         - | 7761 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7762 | ` */` |
|        34 | 7763 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7764 | `{` |
|         - | 7765 | `	ph7_hashmap_node *pEntry;` |
|         - | 7766 | `	ph7_hashmap *pMap;` |
|         - | 7767 | `	ph7_value *pValue;` |
|         - | 7768 | `	ph7_value sResult;` |
|         - | 7769 | `	sxi32 rc;` |
|         - | 7770 | `	sxu32 n;` |
|        39 | 7771 | `	if( nArg < 2 ){` |
|         8 | 7772 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7773 | `			"ArgumentCountError",` |
|         - | 7774 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|         2 | 7775 | `			nArg` |
|         - | 7776 | `			);` |
|         - | 7777 | `	}` |
|        35 | 7778 | `	if( nArg > 3 ){` |
|         4 | 7779 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7780 | `			"ArgumentCountError",` |
|         - | 7781 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7782 | `			nArg` |
|         - | 7783 | `			);` |
|         - | 7784 | `	}` |
|        33 | 7785 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7786 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7787 | `			"TypeError",` |
|         - | 7788 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7789 | `			ph7_type_name(apArg[0])` |
|         - | 7790 | `			);` |
|         - | 7791 | `	}` |
|        31 | 7792 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7793 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7794 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7795 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7796 | `				"TypeError",` |
|         - | 7797 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7798 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7799 | `				zFunc` |
|         - | 7800 | `				);` |
|         - | 7801 | `		}` |
|         9 | 7802 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7803 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7804 | `				"TypeError",` |
|         - | 7805 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7806 | `				"array callback must have exactly two members"` |
|         - | 7807 | `				);` |
|         - | 7808 | `		}` |
|         6 | 7809 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7810 | `			"TypeError",` |
|         - | 7811 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7812 | `			"no array or string given"` |
|         - | 7813 | `			);` |
|         - | 7814 | `	}` |
|         - | 7815 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7816 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7817 | `	/* Assume a NULL initial value */` |
|        19 | 7818 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7819 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7820 | `	if( nArg > 2 ){` |
|         - | 7821 | `		/* Set the initial value */` |
|        13 | 7822 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7823 | `	}` |
|         - | 7824 | `	/* Perform the requested operation */` |
|        19 | 7825 | `	pEntry = pMap->pFirst;` |
|        55 | 7826 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7827 | `		/* Extract the node value */` |
|        39 | 7828 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7829 | `		/* Invoke the supplied callback */` |
|        39 | 7830 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7831 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7832 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7833 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7834 | `			return PH7_EXCEPTION;` |
|         - | 7835 | `		}` |
|         - | 7836 | `		/* Point to the next entry */` |
|        37 | 7837 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7838 | `	}` |
|        17 | 7839 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7840 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7841 | `	return PH7_OK;` |
|        22 | 7842 | `}` |
|         - | 7843 | `/*` |
|         - | 7844 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7845 | ` *  Apply a user function to every member of an array.` |
|         - | 7846 | ` * Parameters` |
|         - | 7847 | ` *  $array` |
|         - | 7848 | ` *   The input array.` |
|         - | 7849 | ` *  $funcname` |
|         - | 7850 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7851 | ` *   the first, and the key/index second.` |
|         - | 7852 | ` * Note:` |
|         - | 7853 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7854 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7855 | ` *  be made in the original array itself.` |
|         - | 7856 | ` *  $userdata` |
|         - | 7857 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7858 | ` *   to the callback funcname.` |
|         - | 7859 | ` * Return` |
|         - | 7860 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7861 | ` */` |
|        38 | 7862 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7863 | `{` |
|         - | 7864 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7865 | `	ph7_hashmap_node *pEntry;` |
|         - | 7866 | `	ph7_hashmap *pMap;` |
|         - | 7867 | `	sxu32 n;` |
|        43 | 7868 | `	if( nArg < 2 ){` |
|         8 | 7869 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7870 | `			"ArgumentCountError",` |
|         - | 7871 | `			"array_walk() expects at least 2 arguments, %d given",` |
|         2 | 7872 | `			nArg` |
|         - | 7873 | `			);` |
|         - | 7874 | `	}` |
|        39 | 7875 | `	if( nArg > 3 ){` |
|         4 | 7876 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7877 | `			"ArgumentCountError",` |
|         - | 7878 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7879 | `			nArg` |
|         - | 7880 | `			);` |
|         - | 7881 | `	}` |
|        37 | 7882 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7883 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7884 | `			"TypeError",` |
|         - | 7885 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7886 | `			ph7_type_name(apArg[0])` |
|         - | 7887 | `			);` |
|         - | 7888 | `	}` |
|        35 | 7889 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7890 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7891 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7892 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7893 | `				"TypeError",` |
|         - | 7894 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7895 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7896 | `				zFunc` |
|         - | 7897 | `				);` |
|         - | 7898 | `		}` |
|        12 | 7899 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7900 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7901 | `				"TypeError",` |
|         - | 7902 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7903 | `				"array callback must have exactly two members"` |
|         - | 7904 | `				);` |
|         - | 7905 | `		}` |
|         6 | 7906 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7907 | `			"TypeError",` |
|         - | 7908 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7909 | `			"no array or string given"` |
|         - | 7910 | `			);` |
|         - | 7911 | `	}` |
|        21 | 7912 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7913 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7914 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7915 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7916 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7917 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7918 | `	/* Perform the desired operation */` |
|        21 | 7919 | `	pEntry = pMap->pFirst;` |
|        61 | 7920 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7921 | `		/* Extract the node value */` |
|        43 | 7922 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7923 | `		if( pValue ){` |
|         - | 7924 | `			sxi32 rcW;` |
|         - | 7925 | `			/* Extract the entry key */` |
|        43 | 7926 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7927 | `			/* Invoke the supplied callback */` |
|        43 | 7928 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7929 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7930 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7931 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7932 | `				return PH7_EXCEPTION;` |
|         - | 7933 | `			}` |
|        20 | 7934 | `		}` |
|         - | 7935 | `		/* Point to the next entry */` |
|        41 | 7936 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7937 | `	}` |
|         - | 7938 | `	/* All done, return TRUE */` |
|        19 | 7939 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7940 | `	return PH7_OK;` |
|        24 | 7941 | `}` |
|         - | 7942 | `/*` |
|         - | 7943 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7944 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7945 | ` */` |
|        22 | 7946 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7947 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7948 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7949 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7950 | `	int iNest             /* Nesting level */` |
|         - | 7951 | `	)` |
|         1 | 7952 | `{` |
|         - | 7953 | `	ph7_hashmap_node *pEntry;` |
|         - | 7954 | `	ph7_value *pValue,sKey;` |
|         - | 7955 | `	sxi32 rc;` |
|         - | 7956 | `	sxu32 n;` |
|         - | 7957 | `	/* Iterate through hashmap entries */` |
|        23 | 7958 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7959 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7960 | `	pEntry = pMap->pFirst;` |
|        59 | 7961 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7962 | `		/* Extract the node value */` |
|        37 | 7963 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7964 | `		if( pValue ){` |
|        37 | 7965 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7966 | `				if( iNest < 32 ){` |
|         - | 7967 | `					/* Recurse */` |
|        11 | 7968 | `					iNest++;` |
|        11 | 7969 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 7970 | `					iNest--;` |
|        11 | 7971 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7972 | `						return PH7_EXCEPTION;` |
|         - | 7973 | `					}` |
|         5 | 7974 | `				}` |
|         6 | 7975 | `			}else{` |
|         - | 7976 | `				/* Extract the node key */` |
|        27 | 7977 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7978 | `				/* Invoke the supplied callback */` |
|        27 | 7979 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 7980 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 7981 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 7982 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7983 | `					return PH7_EXCEPTION;` |
|         - | 7984 | `				}` |
|         - | 7985 | `			}` |
|        18 | 7986 | `		}` |
|         - | 7987 | `		/* Point to the next entry */` |
|        37 | 7988 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7989 | `	}` |
|        23 | 7990 | `	return PH7_OK;` |
|        12 | 7991 | `}` |
|         - | 7992 | `/*` |
|         - | 7993 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7994 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 7995 | ` * Parameters` |
|         - | 7996 | ` *  $array` |
|         - | 7997 | ` *   The input array.` |
|         - | 7998 | ` *  $funcname` |
|         - | 7999 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8000 | ` *   the first, and the key/index second.` |
|         - | 8001 | ` * Note:` |
|         - | 8002 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8003 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8004 | ` *  be made in the original array itself.` |
|         - | 8005 | ` *  $userdata` |
|         - | 8006 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8007 | ` *   to the callback funcname.` |
|         - | 8008 | ` * Return` |
|         - | 8009 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8010 | ` */` |
|        30 | 8011 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8012 | `{` |
|         - | 8013 | `	ph7_hashmap *pMap;` |
|        35 | 8014 | `	if( nArg < 2 ){` |
|         8 | 8015 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8016 | `			"ArgumentCountError",` |
|         - | 8017 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|         2 | 8018 | `			nArg` |
|         - | 8019 | `			);` |
|         - | 8020 | `	}` |
|        31 | 8021 | `	if( nArg > 3 ){` |
|         4 | 8022 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8023 | `			"ArgumentCountError",` |
|         - | 8024 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8025 | `			nArg` |
|         - | 8026 | `			);` |
|         - | 8027 | `	}` |
|        29 | 8028 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8029 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8030 | `			"TypeError",` |
|         - | 8031 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8032 | `			ph7_type_name(apArg[0])` |
|         - | 8033 | `			);` |
|         - | 8034 | `	}` |
|        27 | 8035 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8036 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8037 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8038 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8039 | `				"TypeError",` |
|         - | 8040 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8041 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8042 | `				zFunc` |
|         - | 8043 | `				);` |
|         - | 8044 | `		}` |
|        12 | 8045 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8046 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8047 | `				"TypeError",` |
|         - | 8048 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8049 | `				"array callback must have exactly two members"` |
|         - | 8050 | `				);` |
|         - | 8051 | `		}` |
|         6 | 8052 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8053 | `			"TypeError",` |
|         - | 8054 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8055 | `			"no array or string given"` |
|         - | 8056 | `			);` |
|         - | 8057 | `	}` |
|         - | 8058 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8059 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8060 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8061 | `	/* Perform the desired operation */` |
|        13 | 8062 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8063 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8064 | `		return PH7_EXCEPTION;` |
|         - | 8065 | `	}` |
|         - | 8066 | `	/* All done, return TRUE */` |
|        13 | 8067 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8068 | `	return PH7_OK;` |
|        20 | 8069 | `}` |
|         - | 8070 | `/*` |
|         - | 8071 | ` * bool array_is_list(array $array)` |
|         - | 8072 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8073 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8074 | ` * Return` |
|         - | 8075 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8076 | ` */` |
|         - | 8077 | `/*` |
|         - | 8078 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8079 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8080 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8081 | ` */` |
|       246 | 8082 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8083 | `{` |
|       247 | 8084 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       247 | 8085 | `	sxi64 iExpect = 0;` |
|         - | 8086 | `	sxu32 n;` |
|       555 | 8087 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       409 | 8088 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8089 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       101 | 8090 | `			return 0;` |
|         - | 8091 | `		}` |
|       309 | 8092 | `		++iExpect;` |
|       309 | 8093 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       155 | 8094 | `	}` |
|       147 | 8095 | `	return 1;` |
|       124 | 8096 | `}` |
|        12 | 8097 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8098 | `{` |
|        13 | 8099 | `	if( nArg < 1 ){` |
|       ! 0 | 8100 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8101 | `			"ArgumentCountError",` |
|         - | 8102 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8103 | `			);` |
|         - | 8104 | `	}` |
|        13 | 8105 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8106 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8107 | `			"TypeError",` |
|         - | 8108 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8109 | `			ph7_type_name(apArg[0])` |
|         - | 8110 | `			);` |
|         - | 8111 | `	}` |
|        13 | 8112 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8113 | `	return PH7_OK;` |
|         7 | 8114 | `}` |
|         - | 8115 | `/*` |
|         - | 8116 | ` * mixed array_first(array $array)` |
|         - | 8117 | ` * mixed array_last(array $array)` |
|         - | 8118 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8119 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8120 | ` *  untouched (unlike reset()/end()).` |
|         - | 8121 | ` */` |
|        20 | 8122 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8123 | `{` |
|         - | 8124 | `	ph7_hashmap *pMap;` |
|         - | 8125 | `	ph7_hashmap_node *pNode;` |
|         - | 8126 | `	ph7_value *pVal;` |
|        21 | 8127 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        21 | 8128 | `	if( nArg < 1 ){` |
|         4 | 8129 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8130 | `			"ArgumentCountError",` |
|         - | 8131 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8132 | `			zName` |
|         - | 8133 | `			);` |
|         - | 8134 | `	}` |
|        19 | 8135 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8136 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8137 | `			"TypeError",` |
|         - | 8138 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8139 | `			zName,` |
|         1 | 8140 | `			ph7_type_name(apArg[0])` |
|         - | 8141 | `			);` |
|         - | 8142 | `	}` |
|        17 | 8143 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8144 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8145 | `	if( pNode == 0 ){` |
|         - | 8146 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8147 | `		ph7_result_null(pCtx);` |
|         5 | 8148 | `		return PH7_OK;` |
|         - | 8149 | `	}` |
|        13 | 8150 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8151 | `	if( pVal ){` |
|        13 | 8152 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8153 | `	}else{` |
|       ! 0 | 8154 | `		ph7_result_null(pCtx);` |
|         - | 8155 | `	}` |
|        13 | 8156 | `	return PH7_OK;` |
|        11 | 8157 | `}` |
|        10 | 8158 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8159 | `{` |
|        11 | 8160 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8161 | `}` |
|        10 | 8162 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8163 | `{` |
|        11 | 8164 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8165 | `}` |
|         - | 8166 | `/*` |
|         - | 8167 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8168 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8169 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8170 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8171 | ` *  untouched.` |
|         - | 8172 | ` */` |
|        24 | 8173 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8174 | `{` |
|         - | 8175 | `	ph7_hashmap *pMap;` |
|         - | 8176 | `	ph7_hashmap_node *pNode;` |
|        25 | 8177 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        25 | 8178 | `	if( nArg < 1 ){` |
|         4 | 8179 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8180 | `			"ArgumentCountError",` |
|         - | 8181 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8182 | `			zName` |
|         - | 8183 | `			);` |
|         - | 8184 | `	}` |
|        23 | 8185 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8186 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8187 | `			"TypeError",` |
|         - | 8188 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8189 | `			zName,` |
|         1 | 8190 | `			ph7_type_name(apArg[0])` |
|         - | 8191 | `			);` |
|         - | 8192 | `	}` |
|        21 | 8193 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8194 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8195 | `	if( pNode == 0 ){` |
|         - | 8196 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8197 | `		ph7_result_null(pCtx);` |
|         5 | 8198 | `		return PH7_OK;` |
|         - | 8199 | `	}` |
|        17 | 8200 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8201 | `	return PH7_OK;` |
|        13 | 8202 | `}` |
|        12 | 8203 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8204 | `{` |
|        13 | 8205 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8206 | `}` |
|        12 | 8207 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8208 | `{` |
|        13 | 8209 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8210 | `}` |
|         - | 8211 | `/*` |
|         - | 8212 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8213 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8214 | ` * array_column() for both the column value and the index key.` |
|         - | 8215 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8216 | ` * container or the key is absent.` |
|         - | 8217 | ` */` |
|        32 | 8218 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8219 | `{` |
|        33 | 8220 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8221 | `		ph7_hashmap_node *pNode;` |
|        25 | 8222 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8223 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8224 | `		}` |
|        11 | 8225 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8226 | `		ph7_value sName;` |
|         - | 8227 | `		const char *zName;` |
|         - | 8228 | `		ph7_value *pAttr;` |
|         - | 8229 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8230 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8231 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8232 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8233 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8234 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8235 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8236 | `		return pAttr;` |
|         - | 8237 | `	}` |
|         5 | 8238 | `	return 0;` |
|        17 | 8239 | `}` |
|         - | 8240 | `/*` |
|         - | 8241 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8242 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8243 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8244 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8245 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8246 | ` *  Each row may be an array or an object.` |
|         - | 8247 | ` */` |
|        12 | 8248 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8249 | `{` |
|         - | 8250 | `	ph7_hashmap_node *pNode;` |
|         - | 8251 | `	ph7_hashmap *pMap;` |
|         - | 8252 | `	ph7_value *pArray;` |
|         - | 8253 | `	ph7_value *pRow;` |
|         - | 8254 | `	ph7_value *pCol;` |
|         - | 8255 | `	ph7_value *pIdx;` |
|         - | 8256 | `	int bWantCol;` |
|         - | 8257 | `	int bWantIdx;` |
|         - | 8258 | `	sxu32 n;` |
|        13 | 8259 | `	if( nArg < 2 ){` |
|       ! 0 | 8260 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8261 | `			"ArgumentCountError",` |
|         - | 8262 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8263 | `			nArg` |
|         - | 8264 | `			);` |
|         - | 8265 | `	}` |
|        13 | 8266 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8267 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8268 | `			"TypeError",` |
|         - | 8269 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8270 | `			ph7_type_name(apArg[0])` |
|         - | 8271 | `			);` |
|         - | 8272 | `	}` |
|        13 | 8273 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8274 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8275 | `	if( pArray == 0 ){` |
|       ! 0 | 8276 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8277 | `		return PH7_OK;` |
|         - | 8278 | `	}` |
|         - | 8279 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8280 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8281 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8282 | `	pNode = pMap->pFirst;` |
|        33 | 8283 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8284 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8285 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8286 | `		if( pRow == 0 ){` |
|       ! 0 | 8287 | `			continue;` |
|         - | 8288 | `		}` |
|        21 | 8289 | `		if( bWantCol ){` |
|        19 | 8290 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8291 | `			if( pCol == 0 ){` |
|         - | 8292 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8293 | `				continue;` |
|         - | 8294 | `			}` |
|         9 | 8295 | `		}else{` |
|         3 | 8296 | `			pCol = pRow;` |
|         - | 8297 | `		}` |
|        19 | 8298 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8299 | `		if( pIdx ){` |
|        13 | 8300 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8301 | `		}else{` |
|         7 | 8302 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8303 | `		}` |
|        10 | 8304 | `	}` |
|        13 | 8305 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8306 | `	return PH7_OK;` |
|         7 | 8307 | `}` |
|         - | 8308 | `/*` |
|         - | 8309 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8310 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8311 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8312 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8313 | ` */` |
|        28 | 8314 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8315 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8316 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8317 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8318 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8319 | `	)` |
|         1 | 8320 | `{` |
|         - | 8321 | `	ph7_hashmap_node *pEntry;` |
|         - | 8322 | `	ph7_hashmap *pMap;` |
|         - | 8323 | `	ph7_value *pValue;` |
|         - | 8324 | `	ph7_value *apCbArg[2];` |
|         - | 8325 | `	ph7_value sKey;` |
|         - | 8326 | `	ph7_value sResult;` |
|         - | 8327 | `	sxi32 rc;` |
|         - | 8328 | `	sxu32 n;` |
|        29 | 8329 | `	*ppMatch = 0;` |
|        29 | 8330 | `	if( nArg < 2 ){` |
|       ! 0 | 8331 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8332 | `			"ArgumentCountError",` |
|         - | 8333 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8334 | `			zName,nArg` |
|         - | 8335 | `			);` |
|         - | 8336 | `	}` |
|        29 | 8337 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8338 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8339 | `			"TypeError",` |
|         - | 8340 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8341 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8342 | `			);` |
|         - | 8343 | `	}` |
|        29 | 8344 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8345 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8346 | `			"TypeError",` |
|         - | 8347 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8348 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8349 | `			);` |
|         - | 8350 | `	}` |
|        29 | 8351 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8352 | `	pEntry = pMap->pFirst;` |
|        29 | 8353 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8354 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8355 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8356 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8357 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8358 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8359 | `		if( pValue ){` |
|         - | 8360 | `			/* The callback receives ($value, $key). */` |
|        59 | 8361 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8362 | `			apCbArg[0] = pValue;` |
|        59 | 8363 | `			apCbArg[1] = &sKey;` |
|        59 | 8364 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8365 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8366 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8367 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8368 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8369 | `				return PH7_EXCEPTION;` |
|         - | 8370 | `			}` |
|        59 | 8371 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8372 | `				*ppMatch = pEntry;` |
|        15 | 8373 | `				break;` |
|         - | 8374 | `			}` |
|        22 | 8375 | `		}` |
|        45 | 8376 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8377 | `	}` |
|        29 | 8378 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8379 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8380 | `	return PH7_OK;` |
|        15 | 8381 | `}` |
|         - | 8382 | `/*` |
|         - | 8383 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8384 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8385 | ` *  is truthy, or NULL if none match.` |
|         - | 8386 | ` */` |
|         6 | 8387 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8388 | `{` |
|         - | 8389 | `	ph7_hashmap_node *pMatch;` |
|         - | 8390 | `	ph7_value *pVal;` |
|         - | 8391 | `	sxi32 rc;` |
|         7 | 8392 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8393 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8394 | `		return rc;` |
|         - | 8395 | `	}` |
|         7 | 8396 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8397 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8398 | `	}else{` |
|         3 | 8399 | `		ph7_result_null(pCtx);` |
|         - | 8400 | `	}` |
|         7 | 8401 | `	return PH7_OK;` |
|         4 | 8402 | `}` |
|         - | 8403 | `/*` |
|         - | 8404 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8405 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8406 | ` *  is truthy, or NULL if none match.` |
|         - | 8407 | ` */` |
|         6 | 8408 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8409 | `{` |
|         - | 8410 | `	ph7_hashmap_node *pMatch;` |
|         - | 8411 | `	sxi32 rc;` |
|         7 | 8412 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8413 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8414 | `		return rc;` |
|         - | 8415 | `	}` |
|         7 | 8416 | `	if( pMatch == 0 ){` |
|         3 | 8417 | `		ph7_result_null(pCtx);` |
|         6 | 8418 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8419 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8420 | `	}else{` |
|         4 | 8421 | `		ph7_result_string(pCtx,` |
|         2 | 8422 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8423 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8424 | `	}` |
|         7 | 8425 | `	return PH7_OK;` |
|         4 | 8426 | `}` |
|         - | 8427 | `/*` |
|         - | 8428 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8429 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8430 | ` *  FALSE for an empty array.` |
|         - | 8431 | ` */` |
|         8 | 8432 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8433 | `{` |
|         - | 8434 | `	ph7_hashmap_node *pMatch;` |
|         - | 8435 | `	sxi32 rc;` |
|         9 | 8436 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8437 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8438 | `		return rc;` |
|         - | 8439 | `	}` |
|         9 | 8440 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8441 | `	return PH7_OK;` |
|         5 | 8442 | `}` |
|         - | 8443 | `/*` |
|         - | 8444 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8445 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8446 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8447 | ` */` |
|         8 | 8448 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8449 | `{` |
|         - | 8450 | `	ph7_hashmap_node *pMatch;` |
|         - | 8451 | `	sxi32 rc;` |
|         9 | 8452 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8453 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8454 | `		return rc;` |
|         - | 8455 | `	}` |
|         9 | 8456 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8457 | `	return PH7_OK;` |
|         5 | 8458 | `}` |
|         - | 8459 | `/*` |
|         - | 8460 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8461 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8462 | ` */` |
|         - | 8463 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8464 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8465 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8466 | `{` |
|        84 | 8467 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8468 | `	(void)pVm;` |
|        84 | 8469 | `	p->nCount++;` |
|        84 | 8470 | `	if( p->pArray ){` |
|         - | 8471 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8472 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8473 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8474 | `	}` |
|        84 | 8475 | `	return SXRET_OK;` |
|         4 | 8476 | `}` |
|         - | 8477 | `/*` |
|         - | 8478 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8479 | ` */` |
|        30 | 8480 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8481 | `{` |
|         - | 8482 | `	struct IterCollect sCol;` |
|         - | 8483 | `	ph7_value *pArray;` |
|         - | 8484 | `	sxi32 rc;` |
|        34 | 8485 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8486 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8487 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8488 | `	sCol.pArray = pArray;` |
|        34 | 8489 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8490 | `	sCol.nCount = 0;` |
|        34 | 8491 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8492 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8493 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8494 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8495 | `		sxu32 n;` |
|         9 | 8496 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8497 | `			ph7_value sKey, *pVal;` |
|         7 | 8498 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8499 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8500 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8501 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8502 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8503 | `			pEntry = pEntry->pPrev;` |
|         4 | 8504 | `		}` |
|         3 | 8505 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8506 | `		return PH7_OK;` |
|         - | 8507 | `	}` |
|        32 | 8508 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8509 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8510 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8511 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8512 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8513 | `			ph7_type_name(apArg[0]));` |
|         - | 8514 | `	}` |
|        30 | 8515 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8516 | `	return PH7_OK;` |
|        19 | 8517 | `}` |
|         - | 8518 | `/*` |
|         - | 8519 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8520 | ` */` |
|         8 | 8521 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8522 | `{` |
|         - | 8523 | `	struct IterCollect sCol;` |
|         - | 8524 | `	sxi32 rc;` |
|         9 | 8525 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8526 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8527 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8528 | `		return PH7_OK;` |
|         - | 8529 | `	}` |
|         7 | 8530 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8531 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8532 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8533 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8534 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8535 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8536 | `			ph7_type_name(apArg[0]));` |
|         - | 8537 | `	}` |
|         7 | 8538 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8539 | `	return PH7_OK;` |
|         5 | 8540 | `}` |
|         - | 8541 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8542 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8543 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8544 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8545 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8546 | `{` |
|        33 | 8547 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8548 | `	ph7_value sResult;` |
|         - | 8549 | `	SySet aArg;` |
|         - | 8550 | `	sxi32 rc;` |
|         - | 8551 | `	int bContinue;` |
|        16 | 8552 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8553 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8554 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8555 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8556 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8557 | `		sxu32 n;` |
|        17 | 8558 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8559 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8560 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8561 | `			pEntry = pEntry->pPrev;` |
|         5 | 8562 | `		}` |
|         4 | 8563 | `	}` |
|        33 | 8564 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8565 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8566 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8567 | `	SySetRelease(&aArg);` |
|        33 | 8568 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8569 | `	p->nCount++;` |
|        31 | 8570 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8571 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8572 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8573 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8574 | `}` |
|         - | 8575 | `/*` |
|         - | 8576 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8577 | ` */` |
|        12 | 8578 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8579 | `{` |
|         - | 8580 | `	struct IterApply sApp;` |
|         - | 8581 | `	sxi32 rc;` |
|        13 | 8582 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8583 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8584 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8585 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8586 | `	}` |
|        13 | 8587 | `	sApp.pCallback = apArg[1];` |
|        13 | 8588 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8589 | `	sApp.nCount = 0;` |
|        13 | 8590 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8591 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8592 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8593 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8594 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8595 | `			ph7_type_name(apArg[0]));` |
|         - | 8596 | `	}` |
|        11 | 8597 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8598 | `	return PH7_OK;` |
|         7 | 8599 | `}` |
|         - | 8600 | `/*` |
|         - | 8601 | ` * Table of hashmap functions.` |
|         - | 8602 | ` */` |
|         - | 8603 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8604 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8605 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8606 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8607 | `	{"count",             ph7_hashmap_count },` |
|         - | 8608 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8609 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8610 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8611 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8612 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8613 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8614 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8615 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8616 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8617 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8618 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8619 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8620 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8621 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8622 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8623 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8624 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8625 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8626 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8627 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8628 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8629 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8630 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8631 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8632 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8633 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8634 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8635 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8636 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8637 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8638 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8639 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8640 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8641 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8642 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8643 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8644 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8645 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8646 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8647 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8648 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8649 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8650 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8651 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8652 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8653 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8654 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8655 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8656 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8657 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8658 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8659 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8660 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8661 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8662 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8663 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8664 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8665 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8666 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8667 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8668 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8669 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8670 | `	{"current",           ph7_hashmap_current },` |
|         - | 8671 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8672 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8673 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8674 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8675 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8676 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8677 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8678 | `};` |
|         - | 8679 | `/*` |
|         - | 8680 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8681 | ` */` |
|      3566 | 8682 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8683 | `{` |
|         - | 8684 | `	sxu32 n;` |
|    267455 | 8685 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    263889 | 8686 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    131947 | 8687 | `	}` |
|      3571 | 8688 | `}` |
|         - | 8689 | `/*` |
|         - | 8690 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8691 | ` * the BLOB given as the first argument.` |
|         - | 8692 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8693 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8694 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8695 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8696 | ` */` |
|         - | 8697 | `/*` |
|         - | 8698 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8699 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8700 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8701 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8702 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8703 | ` */` |
|       120 | 8704 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8705 | `{` |
|       122 | 8706 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8707 | `	ph7_value *pObj;` |
|       122 | 8708 | `	sxu32 n = 0;` |
|         - | 8709 | `	int isRef;` |
|       122 | 8710 | `	sxi32 rc = SXRET_OK;` |
|         - | 8711 | `	int i;` |
|       195 | 8712 | `	for(;;){` |
|       392 | 8713 | `		if( n >= pMap->nEntry ){` |
|       122 | 8714 | `			break;` |
|         - | 8715 | `		}` |
|       272 | 8716 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       272 | 8717 | `		isRef = (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0;` |
|       272 | 8718 | `		if( ShowType ){` |
|         - | 8719 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8720 | `			 * on the next line at the same indent (php). */` |
|       104 | 8721 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        70 | 8722 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        36 | 8723 | `			}` |
|        36 | 8724 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        23 | 8725 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        12 | 8726 | `			}else{` |
|        20 | 8727 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8728 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8729 | `			}` |
|        36 | 8730 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        36 | 8731 | `			if( pObj ){` |
|        36 | 8732 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        36 | 8733 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8734 | `					break;` |
|         - | 8735 | `				}` |
|        17 | 8736 | `			}` |
|        19 | 8737 | `		}else{` |
|         - | 8738 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8739 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8740 | `			 * php's extra blank line. References carry no marker. */` |
|      1294 | 8741 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1058 | 8742 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       530 | 8743 | `			}` |
|       238 | 8744 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       125 | 8745 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        63 | 8746 | `			}else{` |
|       170 | 8747 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8748 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8749 | `			}` |
|       236 | 8750 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       132 | 8751 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8752 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8753 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8754 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8755 | `					break;` |
|         - | 8756 | `				}` |
|        13 | 8757 | `			}else{` |
|       214 | 8758 | `				if( pObj ){` |
|       214 | 8759 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       106 | 8760 | `				}` |
|       214 | 8761 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8762 | `			}` |
|         - | 8763 | `		}` |
|         - | 8764 | `		/* Point to the next entry */` |
|       272 | 8765 | `		n++;` |
|       272 | 8766 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 8767 | `	}` |
|       122 | 8768 | `	return rc;` |
|         2 | 8769 | `}` |
|       116 | 8770 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8771 | `{` |
|         - | 8772 | `	sxi32 rc;` |
|         - | 8773 | `	int i;` |
|       118 | 8774 | `	if( nDepth > 31 ){` |
|         - | 8775 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8776 | `		/* Nesting limit reached */` |
|       ! 0 | 8777 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8778 | `		return SXERR_LIMIT;` |
|         - | 8779 | `	}` |
|       118 | 8780 | `	if( ShowType ){` |
|         - | 8781 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8782 | `		 * newline (a nested array is itself an entry value line). */` |
|        14 | 8783 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        14 | 8784 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        14 | 8785 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        14 | 8786 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8787 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8788 | `		}` |
|        14 | 8789 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        14 | 8790 | `		return rc;` |
|         - | 8791 | `	}` |
|         - | 8792 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       105 | 8793 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       297 | 8794 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8795 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8796 | `	}` |
|       105 | 8797 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       105 | 8798 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       297 | 8799 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8800 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8801 | `	}` |
|       105 | 8802 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       105 | 8803 | `	return rc;` |
|        60 | 8804 | `}` |
|         - | 8805 | `/*` |
|         - | 8806 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8807 | ` * retrieved entry.` |
|         - | 8808 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8809 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8810 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8811 | ` * a value different from PH7_OK.` |
|         - | 8812 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8813 | ` */` |
|     34046 | 8814 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8815 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8816 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8817 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8818 | `	)` |
|         5 | 8819 | `{` |
|         - | 8820 | `	ph7_hashmap_node *pEntry;` |
|         - | 8821 | `	ph7_value sKey,sValue;` |
|         - | 8822 | `	sxi32 rc;` |
|         - | 8823 | `	sxu32 n;` |
|         - | 8824 | `	/* Initialize walker parameter */` |
|     34051 | 8825 | `	rc = SXRET_OK;` |
|     34051 | 8826 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     34051 | 8827 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     34051 | 8828 | `	n = pMap->nEntry;` |
|     34051 | 8829 | `	pEntry = pMap->pFirst;` |
|         - | 8830 | `	/* Start the iteration process */` |
|     92453 | 8831 | `	for(;;){` |
|    184911 | 8832 | `		if( n < 1 ){` |
|     34051 | 8833 | `			break;` |
|         - | 8834 | `		}` |
|         - | 8835 | `		/* Extract a copy of the key and a copy the current value */` |
|    150865 | 8836 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    150865 | 8837 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8838 | `		/* Invoke the user callback */` |
|    150865 | 8839 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8840 | `		/* Release the copy of the key and the value */` |
|    150865 | 8841 | `		PH7_MemObjRelease(&sKey);` |
|    150865 | 8842 | `		PH7_MemObjRelease(&sValue);` |
|    150865 | 8843 | `		if( rc != PH7_OK ){` |
|         - | 8844 | `			/* Callback request an operation abort */` |
|       ! 0 | 8845 | `			return SXERR_ABORT;` |
|         - | 8846 | `		}` |
|         - | 8847 | `		/* Point to the next entry */` |
|    150865 | 8848 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    150865 | 8849 | `		n--;` |
|         5 | 8850 | `	}` |
|         - | 8851 | `	/* All done */` |
|     34051 | 8852 | `	return SXRET_OK;` |
|     17028 | 8853 | `}` |
|         - | 8854 |  |
