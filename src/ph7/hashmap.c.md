# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3997/4414 lines (90.55%)

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
|   7466746 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7466751 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7466751 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    648748 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    648753 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    648753 |   35 | `	sxu32 nH = 5381;` |
|    648753 |   36 | `	zEnd = &zIn[nLen];` |
|    735701 |   37 | `	for(;;){` |
|   1471407 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1252683 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1122889 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    979607 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    648753 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      1930 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      1935 |   54 | `	sxi64 iCount = 0;` |
|      1935 |   55 | `	if( !bRecursive ){` |
|      1761 |   56 | `		iCount = pMap->nEntry;` |
|       883 |   57 | `	}else{` |
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
|      1935 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3165596 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3165601 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3165601 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3165601 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3165601 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3165601 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3165601 |  112 | `	pNode->nHash = nHash;` |
|   3165601 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3165601 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3165601 |  115 | `	return pNode;` |
|   1582803 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    274478 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    274483 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    274483 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    274483 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    274483 |  133 | `	pNode->pMap  = &(*pMap);` |
|    274483 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    274483 |  135 | `	pNode->nHash = nHash;` |
|    274483 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    274483 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    274483 |  138 | `	pNode->nValIdx = nValIdx;` |
|    274483 |  139 | `	return pNode;` |
|    137244 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3440074 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3440079 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2953319 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2953319 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1476657 |  150 | `	}` |
|   3440079 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3440079 |  153 | `	if( pMap->pFirst == 0 ){` |
|     93175 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     93175 |  156 | `		pMap->pCur = pNode;` |
|     46590 |  157 | `	}else{` |
|   3346909 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3440079 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3440079 |  174 | `	++pMap->nEntry;` |
|   3440079 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7986 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7991 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7991 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7991 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7521 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3763 |  187 | `	}else{` |
|       472 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7991 |  190 | `	if( pNode->pNextCollide ){` |
|      5197 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2597 |  192 | `	}` |
|      7991 |  193 | `	if( pMap->pFirst == pNode ){` |
|       199 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        97 |  195 | `	}` |
|      7991 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       231 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|       113 |  199 | `	}` |
|      7991 |  200 | `	if( pMap->pActiveSteps ){` |
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
|      7991 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7991 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       209 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       209 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       209 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|       102 |  218 | `		}` |
|       102 |  219 | `	}` |
|      7991 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7729 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3862 |  222 | `	}` |
|      7991 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7991 |  224 | `	pMap->nEntry--;` |
|      7991 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|       123 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|       123 |  228 | `		pMap->apBucket = 0;` |
|       123 |  229 | `		pMap->nSize = 0;` |
|       123 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        59 |  231 | `	}` |
|      7991 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3440074 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3440079 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     98283 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     98283 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     98283 |  245 | `		if( nNew < 1 ){` |
|     93175 |  246 | `			nNew = 16;` |
|     46585 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     98283 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     98283 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     98283 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     98283 |  260 | `		pMap->apBucket = apNew;` |
|     98283 |  261 | `		pMap->nSize = nNew;` |
|     98283 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     93175 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5113 |  267 | `		pEntry = pMap->pFirst;` |
|      5113 |  268 | `		n = 0;` |
|   2111482 |  269 | `		for( ;; ){` |
|   4222969 |  270 | `			if( n >= pMap->nEntry ){` |
|      5113 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   4217861 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   4217861 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4217861 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3597123 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3597123 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1798559 |  280 | `			}` |
|   4217861 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   4217861 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4217861 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5113 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2554 |  288 | `	}` |
|   3346909 |  289 | `	return SXRET_OK;` |
|   1720042 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3165596 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3165601 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3165563 |  310 | `		if( pValue ){` |
|   3165557 |  311 | `			sSafeVal = *pValue;` |
|   3165557 |  312 | `			pValue = &sSafeVal;` |
|   1582776 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3165563 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3165563 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3165563 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3165557 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1582776 |  322 | `		}` |
|   3165563 |  323 | `		nIdx = pObj->nIdx;` |
|   1582784 |  324 | `	}else{` |
|        39 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3165601 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3165601 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3165601 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3165601 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        39 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        19 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3165601 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3165601 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3165601 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3165601 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3165601 |  349 | `	return SXRET_OK;` |
|   1582803 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    274478 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    274483 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    226383 |  370 | `		if( pValue ){` |
|    226093 |  371 | `			sSafeVal = *pValue;` |
|    226093 |  372 | `			pValue = &sSafeVal;` |
|    113044 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    226383 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    226383 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    226383 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    226093 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    113044 |  382 | `		}` |
|    226383 |  383 | `		nIdx = pObj->nIdx;` |
|    113194 |  384 | `	}else{` |
|     48105 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    274483 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    274483 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    274483 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    274483 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     48105 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     24050 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    274483 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    274483 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    274483 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    274483 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    274483 |  409 | `	return SXRET_OK;` |
|    137244 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4287866 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4287871 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       725 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4287151 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4287151 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110564106 |  433 | `	for(;;){` |
| 221128217 |  434 | `		if( pNode == 0 ){` |
|   4282181 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216846036 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216843024 |  438 | `			&& pNode->nHash == nHash` |
| 108422496 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      4975 |  441 | `				if( ppNode ){` |
|      4957 |  442 | `					*ppNode = pNode;` |
|      2476 |  443 | `				}` |
|      4975 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216841067 |  447 | `		pNode = pNode->pNextCollide;` |
|         1 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4282181 |  450 | `	return SXERR_NOTFOUND;` |
|   2143938 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    410828 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    410833 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     36563 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    374275 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    374275 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    308939 |  475 | `	for(;;){` |
|    617883 |  476 | `		if( pNode == 0 ){` |
|    314771 |  477 | `			break;` |
|         - |  478 | `		}` |
|    303112 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    301601 |  480 | `			&& pNode->nHash == nHash` |
|    179847 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     59609 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     59509 |  484 | `				if( ppNode ){` |
|     59481 |  485 | `					*ppNode = pNode;` |
|     29738 |  486 | `				}` |
|     59509 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    243613 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    314771 |  493 | `	return SXERR_NOTFOUND;` |
|    205419 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    410960 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    410965 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    410965 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    410965 |  504 | `	int isNeg = FALSE, nDigit;` |
|    410965 |  505 | `	if( zIn >= zEnd ){` |
|       ! 0 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    410965 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    410961 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    410961 |  516 | `	zDigit = zIn;` |
|    205912 |  517 | `	for(;;){` |
|    411829 |  518 | `		if( zIn >= zEnd ){` |
|       251 |  519 | `			break;` |
|         - |  520 | `		}` |
|    411579 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    410711 |  523 | `			return FALSE;` |
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
|    205485 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    141358 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    141363 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    141363 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    136509 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast */` |
|       ! 0 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  559 | `		}` |
|    136509 |  560 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Perform a blob lookup */` |
|    136489 |  562 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    136489 |  563 | `			goto result;` |
|         - |  564 | `		}` |
|        10 |  565 | `	}` |
|         - |  566 | `	/* Perform an int lookup */` |
|      4879 |  567 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  568 | `		/* Force an integer cast */` |
|        35 |  569 | `		PH7_MemObjToInteger(pKey);` |
|        17 |  570 | `	}` |
|         - |  571 | `	/* Perform an int lookup */` |
|      4879 |  572 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     70679 |  573 | `result:` |
|    141363 |  574 | `	if( rc == SXRET_OK ){` |
|         - |  575 | `		/* Node found */` |
|     63645 |  576 | `		if( ppNode ){` |
|     63595 |  577 | `			*ppNode = pNode;` |
|     31795 |  578 | `		}` |
|     63645 |  579 | `		return SXRET_OK;` |
|         - |  580 | `	}` |
|         - |  581 | `	/* No such entry */` |
|     77723 |  582 | `	return SXERR_NOTFOUND;` |
|     70684 |  583 | `}` |
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
|   1023762 |  606 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  607 | `{` |
|   1023767 |  608 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  609 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  610 | `		return TRUE;` |
|         - |  611 | `	}` |
|   1023761 |  612 | `	return FALSE;` |
|    511886 |  613 | `}` |
|         - |  614 | `/*` |
|         - |  615 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  616 | ` * hashmap.` |
|         - |  617 | ` * If a node with the given key already exists in the database` |
|         - |  618 | ` * then this function overwrite the old value.` |
|         - |  619 | ` */` |
|   3391496 |  620 | `static sxi32 HashmapInsert(` |
|         - |  621 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  622 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  623 | `	ph7_value *pVal    /* Node value */` |
|         - |  624 | `	)` |
|         5 |  625 | `{` |
|   3391501 |  626 | `	ph7_hashmap_node *pNode = 0;` |
|   3391501 |  627 | `	sxi32 rc = SXRET_OK;` |
|   3391501 |  628 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    229915 |  629 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  630 | `			/* Force a string cast */` |
|         3 |  631 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  632 | `		}` |
|    229915 |  633 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|      3793 |  634 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  635 | `				/* Automatic index assign */` |
|      3565 |  636 | `				pKey = 0;` |
|      1780 |  637 | `			}` |
|      3793 |  638 | `			goto IntKey;` |
|         - |  639 | `		}` |
|    339188 |  640 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    113061 |  641 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  642 | `				/* Overwrite the old value */` |
|         - |  643 | `				ph7_value *pElem;` |
|       460 |  644 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       460 |  645 | `				if( pElem ){` |
|       460 |  646 | `					if( pVal ){` |
|       460 |  647 | `						PH7_MemObjStore(pVal,pElem);` |
|       232 |  648 | `					}else{` |
|         - |  649 | `						/* Nullify the entry */` |
|       ! 0 |  650 | `						PH7_MemObjToNull(pElem);` |
|         - |  651 | `					}` |
|       228 |  652 | `				}` |
|       460 |  653 | `				return SXRET_OK;` |
|         - |  654 | `		}` |
|    225671 |  655 | `		if( pMap == pMap->pVm->pGlobal ){` |
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
|    225541 |  668 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    225541 |  669 | `		return rc;` |
|         - |  670 | `	}` |
|   1580793 |  671 | `IntKey:` |
|   3165379 |  672 | `	if( pKey ){` |
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
|   1023737 |  704 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  705 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  706 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  707 | `		}` |
|   1023735 |  708 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  709 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  710 | `		}` |
|         - |  711 | `		/* Assign an automatic index */` |
|   1023729 |  712 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1023729 |  713 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1023727 |  714 | `			++pMap->iNextIdx;` |
|    511861 |  715 | `		}` |
|         - |  716 | `	}` |
|         - |  717 | `	/* Insertion result */` |
|   3165189 |  718 | `	return rc;` |
|   1695753 |  719 | `}` |
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
|     48148 |  747 | `static sxi32 HashmapInsertByRef(` |
|         - |  748 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  749 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  750 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  751 | `	)` |
|         5 |  752 | `{` |
|     48153 |  753 | `	ph7_hashmap_node *pNode = 0;` |
|     48153 |  754 | `	sxi32 rc = SXRET_OK;` |
|     48153 |  755 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     48117 |  756 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  757 | `			/* Force a string cast */` |
|       ! 0 |  758 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  759 | `		}` |
|     48117 |  760 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  761 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  762 | `				/* Automatic index assign */` |
|       ! 0 |  763 | `				pKey = 0;` |
|       ! 0 |  764 | `			}` |
|         3 |  765 | `			goto IntKey;` |
|         - |  766 | `		}` |
|     72170 |  767 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     24055 |  768 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  769 | `				/* Overwrite */` |
|        11 |  770 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  771 | `				pNode->nValIdx = nRefIdx;` |
|         - |  772 | `				/* Install in the reference table */` |
|        11 |  773 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  774 | `				return SXRET_OK;` |
|         - |  775 | `		}` |
|         - |  776 | `		/* Perform a blob-key insertion */` |
|     48105 |  777 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     48105 |  778 | `		return rc;` |
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
|     24079 |  811 | `}` |
|         - |  812 | `/*` |
|         - |  813 | ` * Extract node value.` |
|         - |  814 | ` */` |
|   1450759 |  815 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  816 | `{` |
|         - |  817 | `	/* Point to the desired object */` |
|         - |  818 | `	ph7_value *pObj;` |
|   1450764 |  819 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1450764 |  820 | `	return pObj;` |
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
|     71784 |  866 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  867 | `{` |
|         - |  868 | `	ph7_value sObj1,sObj2;` |
|         - |  869 | `	sxi32 rc;` |
|     71789 |  870 | `	if( pLeft == pRight ){` |
|         - |  871 | `		/*` |
|         - |  872 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  873 | `		 * below for more information on this sceanario.` |
|         - |  874 | `		 */` |
|       ! 0 |  875 | `		return 0;` |
|         - |  876 | `	}` |
|         - |  877 | `	/* Do the comparison */` |
|     71789 |  878 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     71789 |  879 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     71789 |  880 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     71789 |  881 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     71789 |  882 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     71789 |  883 | `	PH7_MemObjRelease(&sObj1);` |
|     71789 |  884 | `	PH7_MemObjRelease(&sObj2);` |
|     71789 |  885 | `	return rc;` |
|     35913 |  886 | `}` |
|         - |  887 | `/*` |
|         - |  888 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  889 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  890 | ` */` |
|     14004 |  891 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  892 | `{` |
|     14009 |  893 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  894 | `	sxu32 nBucket;` |
|         - |  895 | `	/* Remove old collision links */` |
|     14009 |  896 | `	if( pEntry->pPrevCollide ){` |
|     11373 |  897 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5705 |  898 | `	}else{` |
|      2641 |  899 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  900 | `	}` |
|     14009 |  901 | `	if( pEntry->pNextCollide ){` |
|      1145 |  902 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       585 |  903 | `	}` |
|     14009 |  904 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  905 | `	/* Compute the new hash */` |
|     14009 |  906 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     14009 |  907 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     14009 |  908 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  909 | `	/* Link to the new bucket */` |
|     14009 |  910 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14009 |  911 | `	if( pMap->apBucket[nBucket] ){` |
|     11708 |  912 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5867 |  913 | `	}` |
|     14009 |  914 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14009 |  915 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  916 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  917 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  918 | `	 * the no-overflow invariant uniform). */` |
|     14009 |  919 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     14009 |  920 | `		pMap->iNextIdx++;` |
|      7002 |  921 | `	}` |
|     14009 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Perform a linear search on a given hashmap.` |
|         - |  925 | ` * Write a pointer to the target node on success.` |
|         - |  926 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  927 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  928 | ` * for more information.` |
|         - |  929 | ` */` |
|     33528 |  930 | `static int HashmapFindValue(` |
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
|     33533 |  943 | `	pEntry = pMap->pFirst;` |
|     33533 |  944 | `	n = pMap->nEntry;` |
|     33533 |  945 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33533 |  946 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     79627 |  947 | `	for(;;){` |
|    159260 |  948 | `		if( n < 1 ){` |
|       115 |  949 | `			break;` |
|         - |  950 | `		}` |
|         - |  951 | `		/* Extract node value */` |
|    159146 |  952 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    159146 |  953 | `		if( pVal ){` |
|         - |  954 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  955 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  956 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  957 | `			 * so null needles/values take the same path as everything else` |
|         - |  958 | `			 * (the historical null-to-null shortcut here made` |
|         - |  959 | `			 * in_array(null, [""]) false where php says true). */` |
|    159146 |  960 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    159146 |  961 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    159146 |  962 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    159146 |  963 | `			PH7_MemObjRelease(&sVal);` |
|    159146 |  964 | `			PH7_MemObjRelease(&sNeedle);` |
|    159146 |  965 | `			if( rc == 0 ){` |
|     33419 |  966 | `				if( ppNode ){` |
|        23 |  967 | `					*ppNode = pEntry;` |
|        11 |  968 | `				}` |
|         - |  969 | `				/* Match found*/` |
|     33419 |  970 | `				return SXRET_OK;` |
|         - |  971 | `			}` |
|     62863 |  972 | `		}` |
|         - |  973 | `		/* Point to the next entry */` |
|    125732 |  974 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    125732 |  975 | `		n--;` |
|         5 |  976 | `	}` |
|         - |  977 | `	/* No such entry */` |
|       115 |  978 | `	return SXERR_NOTFOUND;` |
|     16769 |  979 | `}` |
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
|    664558 | 1165 | `static sxi32 HashmapDuplicateNode(` |
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
|    664563 | 1176 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
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
|    664557 | 1201 | `	sSafeVal = *pVal;` |
|         - | 1202 |  |
|    664557 | 1203 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1204 | `		/* Blob key insertion */` |
|      4109 | 1205 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4109 | 1206 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4109 | 1207 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4109 | 1208 | `		PH7_MemObjRelease(&sKey);` |
|      2057 | 1209 | `	}else{` |
|         - | 1210 | `		/* Int key */` |
|    660453 | 1211 | `		if( iAction == 0 ){ /* Merge */` |
|    660211 | 1212 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    330348 | 1213 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1214 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1215 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1216 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1217 | `		}else{ /* Dup */` |
|       215 | 1218 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1219 | `		}` |
|         - | 1220 | `	}` |
|    664557 | 1221 | `	return rc;` |
|    332284 | 1222 | `}` |
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
|      2772 | 1235 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1236 | `{` |
|         - | 1237 | `	ph7_hashmap_node *pEntry;` |
|         - | 1238 | `	ph7_value *pVal;` |
|         - | 1239 | `	sxi32 rc;` |
|         - | 1240 | `	sxu32 n;` |
|      2777 | 1241 | `	if( pSrc == pDest ){` |
|         - | 1242 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1243 | `		 * Unlike the zend engine.` |
|         - | 1244 | `		 */` |
|       ! 0 | 1245 | `		return SXRET_OK;` |
|         - | 1246 | `	}` |
|         - | 1247 | `	/* Point to the first inserted entry in the source */` |
|      2777 | 1248 | `	pEntry = pSrc->pFirst;` |
|         - | 1249 | `	/* Perform the merge */` |
|    663041 | 1250 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1251 | `		/* Extract the node value */` |
|    660269 | 1252 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    660269 | 1253 | `		if( pVal ){` |
|         - | 1254 | `			/* Make a local copy of the value.` |
|         - | 1255 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1256 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1257 | `			 * to the old pool.` |
|         - | 1258 | `			 */` |
|    660269 | 1259 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    330137 | 1260 | `		}else{` |
|       ! 0 | 1261 | `			rc = SXRET_OK;` |
|         - | 1262 | `		}` |
|    660269 | 1263 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1264 | `			return rc;` |
|         - | 1265 | `		}` |
|         - | 1266 | `		/* Point to the next entry */` |
|    660269 | 1267 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    330137 | 1268 | `	}` |
|      2777 | 1269 | `	return SXRET_OK;` |
|      1391 | 1270 | `}` |
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
|      4010 | 1320 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1321 | `{` |
|         - | 1322 | `	ph7_hashmap_node *pEntry;` |
|         - | 1323 | `	ph7_value *pVal;` |
|         - | 1324 | `	sxi32 rc;` |
|         - | 1325 | `	sxu32 n;` |
|      4015 | 1326 | `	if( pSrc == pDest ){` |
|         - | 1327 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1328 | `		 * Unlike the zend engine.` |
|         - | 1329 | `		 */` |
|       ! 0 | 1330 | `		return SXRET_OK;` |
|         - | 1331 | `	}` |
|         - | 1332 | `	/* Point to the first inserted entry in the source */` |
|      4015 | 1333 | `	pEntry = pSrc->pFirst;` |
|         - | 1334 | `	/* Perform the duplication */` |
|      8265 | 1335 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1336 | `		/* Extract the node value */` |
|      4255 | 1337 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4255 | 1338 | `		if( pVal ){` |
|      4255 | 1339 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2130 | 1340 | `		}else{` |
|       ! 0 | 1341 | `			rc = SXRET_OK;` |
|         - | 1342 | `		}` |
|      4255 | 1343 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1344 | `			return rc;` |
|         - | 1345 | `		}` |
|         - | 1346 | `		/* Point to the next entry */` |
|      4255 | 1347 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2130 | 1348 | `	}` |
|      4015 | 1349 | `	return SXRET_OK;` |
|      2010 | 1350 | `}` |
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
|    234220 | 1426 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1427 | `{` |
|    234225 | 1428 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1429 | `	ph7_hashmap *pNew;` |
|         - | 1430 | `	ph7_value *pBacking;` |
|         - | 1431 | `	sxu32 nValIdx;` |
|         - | 1432 | `	int bValueInPool;` |
|    234225 | 1433 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    234225 | 1434 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1435 | `		/* Sole owner, no separation needed */` |
|    231643 | 1436 | `		return pMap;` |
|         - | 1437 | `	}` |
|      2587 | 1438 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1439 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1440 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1441 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1442 | `		return pMap;` |
|         - | 1443 | `	}` |
|         - | 1444 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1445 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1446 | `	 * frame is popped. */` |
|      2461 | 1447 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2461 | 1448 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2456 | 1449 | `		if( pBacking && pBacking != pValue` |
|      2432 | 1450 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2413 | 1451 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1452 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2413 | 1453 | `			pMap->iRef--;` |
|      2413 | 1454 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1455 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2367 | 1456 | `				pMap->iRef++;` |
|      2367 | 1457 | `				return pMap;` |
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
|    117115 | 1520 | `}` |
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
|      3888 | 1558 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1559 | `{` |
|         - | 1560 | `	ph7_hashmap_node *pEntry;` |
|      3893 | 1561 | `	sxi32 rc = SXRET_OK;` |
|         - | 1562 | `	ph7_value *pObj;` |
|         - | 1563 | `	sxu32 n;` |
|      3893 | 1564 | `	if( pLeft == pRight ){` |
|         - | 1565 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1566 | `		 * Unlike the zend engine.` |
|         - | 1567 | `		 */` |
|       ! 0 | 1568 | `		return SXRET_OK;` |
|         - | 1569 | `	}` |
|         - | 1570 | `	/* Perform the union */` |
|      3893 | 1571 | `	pEntry = pRight->pFirst;` |
|      3927 | 1572 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
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
|      3893 | 1606 | `	return SXRET_OK;` |
|      1949 | 1607 | `}` |
|         - | 1608 | `/*` |
|         - | 1609 | ` * Allocate a new hashmap.` |
|         - | 1610 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1611 | ` */` |
|    146564 | 1612 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1613 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1614 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1615 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1616 | `	)` |
|         5 | 1617 | `{` |
|         - | 1618 | `	ph7_hashmap *pMap;` |
|         - | 1619 | `	/* Allocate a new instance */` |
|    146569 | 1620 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    146569 | 1621 | `	if( pMap == 0 ){` |
|       ! 0 | 1622 | `		return 0;` |
|         - | 1623 | `	}` |
|         - | 1624 | `	/* Zero the structure */` |
|    146569 | 1625 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1626 | `	/* Fill in the structure */` |
|    146569 | 1627 | `	pMap->pVm = &(*pVm);` |
|    146569 | 1628 | `	pMap->iRef = 1;` |
|         - | 1629 | `	/* Default hash functions */` |
|    146569 | 1630 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    146569 | 1631 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    146569 | 1632 | `	return pMap;` |
|     73287 | 1633 | `}` |
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
|      3558 | 1654 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|      3563 | 1674 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3563 | 1675 | `	if( pMap == 0 ){` |
|       ! 0 | 1676 | `		return SXERR_MEM;` |
|         - | 1677 | `	}` |
|      3563 | 1678 | `	pVm->pGlobal = pMap;` |
|         - | 1679 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3563 | 1680 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3563 | 1681 | `	if( pObj == 0 ){` |
|       ! 0 | 1682 | `		return SXERR_MEM;` |
|         - | 1683 | `	}` |
|      3563 | 1684 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1685 | `	/* Record object index */` |
|      3563 | 1686 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1687 | `	/* Install the special $GLOBALS array */` |
|      3563 | 1688 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3563 | 1689 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1690 | `		return rc;` |
|         - | 1691 | `	}` |
|         - | 1692 | `	/* Install superglobals now */` |
|     39143 | 1693 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1694 | `		ph7_value *pSuper;` |
|         - | 1695 | `		/* Request an empty array */` |
|     35585 | 1696 | `		pSuper = ph7_new_array(&(*pVm));` |
|     35585 | 1697 | `		if( pSuper == 0 ){` |
|       ! 0 | 1698 | `			return SXERR_MEM;` |
|         - | 1699 | `		}` |
|         - | 1700 | `		/* Install */` |
|     35585 | 1701 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     35585 | 1702 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1703 | `			return rc;` |
|         - | 1704 | `		}` |
|         - | 1705 | `		/* Release the value now it have been installed */` |
|     35585 | 1706 | `		ph7_release_value(&(*pVm),pSuper);` |
|     17795 | 1707 | `	}` |
|         - | 1708 | `	/* Set some $_SERVER entries */` |
|      3563 | 1709 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1710 | `	/*` |
|         - | 1711 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1712 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1713 | `	 */` |
|      7121 | 1714 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1715 | `		"SCRIPT_FILENAME",` |
|      1779 | 1716 | `		pFile ? pFile->zString : ":Memory:",` |
|      3558 | 1717 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1718 | `		);` |
|         - | 1719 | `	/* All done,all super-global are installed now */` |
|      3563 | 1720 | `	return SXRET_OK;` |
|      1784 | 1721 | `}` |
|         - | 1722 | `/*` |
|         - | 1723 | ` * Release a hashmap.` |
|         - | 1724 | ` */` |
|    102366 | 1725 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1726 | `{` |
|         - | 1727 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    102371 | 1728 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1729 | `	sxu32 n;` |
|    102371 | 1730 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1731 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1732 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1733 | `		return SXRET_OK;` |
|         - | 1734 | `	}` |
|    102371 | 1735 | `	if( pMap->pActiveSteps ){` |
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
|    102371 | 1748 | `	n = 0;` |
|    102371 | 1749 | `	pEntry = pMap->pFirst;` |
|   1726427 | 1750 | `	for(;;){` |
|   3452859 | 1751 | `		if( n >= pMap->nEntry ){` |
|    102371 | 1752 | `			break;` |
|         - | 1753 | `		}` |
|   3350493 | 1754 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1755 | `		/* Remove the reference from the foreign table */` |
|   3350493 | 1756 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3350493 | 1757 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1758 | `			/* Restore the ph7_value to the free list */` |
|   3350483 | 1759 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1675239 | 1760 | `		}` |
|         - | 1761 | `		/* Release the node */` |
|   3350493 | 1762 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    196873 | 1763 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     98434 | 1764 | `		}` |
|   3350493 | 1765 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1766 | `		/* Point to the next entry */` |
|   3350493 | 1767 | `		pEntry = pNext;` |
|   3350493 | 1768 | `		n++;` |
|         5 | 1769 | `	}` |
|    102371 | 1770 | `	if( pMap->nEntry > 0 ){` |
|         - | 1771 | `		/* Release the hash bucket */` |
|     77281 | 1772 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     38638 | 1773 | `	}` |
|    102371 | 1774 | `	if( FreeDS ){` |
|         - | 1775 | `		/* Free the whole instance */` |
|    102345 | 1776 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     51175 | 1777 | `	}else{` |
|         - | 1778 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1779 | `		pMap->apBucket = 0;` |
|        28 | 1780 | `		pMap->iNextIdx = 0;` |
|        28 | 1781 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1782 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1783 | `	}` |
|    102371 | 1784 | `	return SXRET_OK;` |
|     51188 | 1785 | `}` |
|         - | 1786 | `/*` |
|         - | 1787 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1788 | ` * If the count reaches zero which mean no more variables` |
|         - | 1789 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1790 | ` */` |
|    847668 | 1791 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1792 | `{` |
|    847673 | 1793 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1794 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    847673 | 1795 | `	pMap->iRef--;` |
|    847673 | 1796 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    102325 | 1797 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     51160 | 1798 | `	}` |
|    847673 | 1799 | `}` |
|         - | 1800 | `/*` |
|         - | 1801 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1802 | ` * Write a pointer to the target node on success.` |
|         - | 1803 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1804 | ` */` |
|    141530 | 1805 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1806 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1807 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1808 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1809 | `	)` |
|         5 | 1810 | `{` |
|         - | 1811 | `	sxi32 rc;` |
|    141535 | 1812 | `	if( pMap->nEntry < 1 ){` |
|         - | 1813 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1814 | `		 */` |
|       177 | 1815 | `		return SXERR_NOTFOUND;` |
|         - | 1816 | `	}` |
|    141363 | 1817 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    141363 | 1818 | `	return rc;` |
|     70770 | 1819 | `}` |
|         - | 1820 | `/*` |
|         - | 1821 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1822 | ` * hashmap.` |
|         - | 1823 | ` * If a node with the given key already exists in the database` |
|         - | 1824 | ` * then this function overwrite the old value.` |
|         - | 1825 | ` */` |
|   2731038 | 1826 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   2731043 | 1837 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2731043 | 1838 | `	return rc;` |
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
|     48142 | 1877 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1878 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1879 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1880 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1881 | `	)` |
|         5 | 1882 | `{` |
|         - | 1883 | `	sxi32 rc;` |
|     48147 | 1884 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1885 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1886 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1887 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1888 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1889 | `		return PH7_ABORT;` |
|         - | 1890 | `	}` |
|     48147 | 1891 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     48147 | 1892 | `	return rc;` |
|     24076 | 1893 | `}` |
|         - | 1894 | `/*` |
|         - | 1895 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1896 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1897 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1898 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1899 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1900 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1901 | ` */` |
|     19008 | 1902 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1903 | `{` |
|     19013 | 1904 | `	pStep->pCursor = pMap->pFirst;` |
|     19013 | 1905 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     19013 | 1906 | `	pMap->pActiveSteps = pStep;` |
|     19013 | 1907 | `}` |
|         - | 1908 | `/*` |
|         - | 1909 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1910 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1911 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1912 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1913 | ` */` |
|     18908 | 1914 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1915 | `{` |
|     18913 | 1916 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     18913 | 1917 | `	while( *ppLink ){` |
|     18913 | 1918 | `		if( *ppLink == pStep ){` |
|     18913 | 1919 | `			*ppLink = pStep->pNextActive;` |
|     18913 | 1920 | `			pStep->pNextActive = 0;` |
|     18913 | 1921 | `			return;` |
|         - | 1922 | `		}` |
|       ! 0 | 1923 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1924 | `	}` |
|      9459 | 1925 | `}` |
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
|    592836 | 1946 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1947 | `{` |
|    592841 | 1948 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    592841 | 1949 | `	if( pEntry ){` |
|    592841 | 1950 | `		if( bStore ){` |
|    235223 | 1951 | `			PH7_MemObjStore(pEntry,pValue);` |
|    117614 | 1952 | `		}else{` |
|    357623 | 1953 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1954 | `		}` |
|    296455 | 1955 | `	}else{` |
|       ! 0 | 1956 | `		PH7_MemObjRelease(pValue);` |
|         - | 1957 | `	}` |
|    592841 | 1958 | `}` |
|         - | 1959 | `/*` |
|         - | 1960 | ` * Extract a node key.` |
|         - | 1961 | ` */` |
|    156164 | 1962 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 1963 | `{` |
|         - | 1964 | `	/* Fill with the current key */` |
|    156169 | 1965 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    150869 | 1966 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 1967 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 1968 | `		}` |
|    150869 | 1969 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    150869 | 1970 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     75437 | 1971 | `	}else{` |
|      5305 | 1972 | `		SyBlobReset(&pKey->sBlob);` |
|      5305 | 1973 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5305 | 1974 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 1975 | `	}` |
|    156169 | 1976 | `}` |
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
|     35946 | 2027 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2028 | `{` |
|         - | 2029 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2030 | `    /* Prevent compiler warning */` |
|     35951 | 2031 | `	result.pNext = result.pPrev = 0;` |
|     35951 | 2032 | `	pTail = &result;` |
|    107877 | 2033 | `	while( pA && pB ){` |
|     71931 | 2034 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47546 | 2035 | `			pTail->pPrev = pA;` |
|     47546 | 2036 | `			pA->pNext = pTail;` |
|     47546 | 2037 | `			pTail = pA;` |
|     47546 | 2038 | `			pA = pA->pPrev;` |
|     23765 | 2039 | `		}else{` |
|     24390 | 2040 | `			pTail->pPrev = pB;` |
|     24390 | 2041 | `			pB->pNext = pTail;` |
|     24390 | 2042 | `			pTail = pB;` |
|     24390 | 2043 | `			pB = pB->pPrev;` |
|         - | 2044 | `		}` |
|         5 | 2045 | `	}` |
|     35951 | 2046 | `	if( pA ){` |
|     25407 | 2047 | `		pTail->pPrev = pA;` |
|     25407 | 2048 | `		pA->pNext = pTail;` |
|     23285 | 2049 | `	}else if( pB ){` |
|     10329 | 2050 | `		pTail->pPrev = pB;` |
|     10329 | 2051 | `		pB->pNext = pTail;` |
|      5132 | 2052 | `	}else{` |
|       225 | 2053 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2054 | `	}` |
|     35951 | 2055 | `	return result.pPrev;` |
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
|     14771 | 2076 | `	while( pIn ){` |
|     14021 | 2077 | `		p = pIn;` |
|     14021 | 2078 | `		pIn = p->pPrev;` |
|     14021 | 2079 | `		p->pPrev = 0;` |
|     26717 | 2080 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26717 | 2081 | `			if( a[i]==0 ){` |
|     14021 | 2082 | `				a[i] = p;` |
|     14021 | 2083 | `				break;` |
|       ! 0 | 2084 | `			}else{` |
|     12701 | 2085 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12701 | 2086 | `				a[i] = 0;` |
|         - | 2087 | `			}` |
|      6353 | 2088 | `		}` |
|     14021 | 2089 | `		if( i==N_SORT_BUCKET-1 ){` |
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
|     71654 | 2112 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2113 | `{` |
|         - | 2114 | `	ph7_value sA,sB;` |
|         - | 2115 | `	sxi32 iFlags;` |
|         - | 2116 | `	int rc;` |
|     71659 | 2117 | `	if( pCmpData == 0 ){` |
|         - | 2118 | `		/* Perform a standard comparison */` |
|     71635 | 2119 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     71635 | 2120 | `		return rc;` |
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
|     35848 | 2158 | `}` |
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
|        10 | 2407 | `	SXUNUSED(pB); /* cc warning */` |
|        10 | 2408 | `	SXUNUSED(pCmpData);` |
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
|      7230 | 2428 | `	for( ;; ){` |
|     14465 | 2429 | `		if( i >= pMap->nEntry ){` |
|       685 | 2430 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       685 | 2431 | `			break;` |
|         - | 2432 | `		}` |
|     13785 | 2433 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2434 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2435 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2436 | `			/* Change key type */` |
|         5 | 2437 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2438 | `		}` |
|     13785 | 2439 | `		HashmapRehashIntNode(p);` |
|         - | 2440 | `		/* Point to the next entry */` |
|     13785 | 2441 | `		i++;` |
|     13785 | 2442 | `		pLast = p;` |
|     13785 | 2443 | `		p = p->pPrev; /* Reverse link */` |
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
|      1012 | 2467 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2468 | `{` |
|         - | 2469 | `	ph7_hashmap *pMap;` |
|         - | 2470 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1017 | 2471 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2472 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2473 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2474 | `		return PH7_OK;` |
|         - | 2475 | `	}` |
|         - | 2476 | `	/* Point to the internal representation of the input hashmap */` |
|      1017 | 2477 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1017 | 2478 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1017 | 2479 | `	if( pMap->nEntry > 1 ){` |
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
|      1017 | 2494 | `	ph7_result_bool(pCtx,1);` |
|      1017 | 2495 | `	return PH7_OK;` |
|       511 | 2496 | `}` |
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
|      1862 | 2957 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2958 | `{` |
|      1867 | 2959 | `	int bRecursive = FALSE;` |
|      1867 | 2960 | `	int bCycleDetected = FALSE;` |
|         - | 2961 | `	sxi64 iCount;` |
|      1867 | 2962 | `	if( nArg < 1 ){` |
|         3 | 2963 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2964 | `			"ArgumentCountError",` |
|         - | 2965 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2966 | `			);` |
|         - | 2967 | `	}` |
|      1865 | 2968 | `	if( nArg > 2 ){` |
|         4 | 2969 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2970 | `			"ArgumentCountError",` |
|         - | 2971 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 2972 | `			nArg` |
|         - | 2973 | `			);` |
|         - | 2974 | `	}` |
|         - | 2975 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 2976 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 2977 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1863 | 2978 | `	if( nArg > 1 ){` |
|        45 | 2979 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        45 | 2980 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        12 | 2981 | `			return PH7_VmThrowException(pCtx,` |
|         - | 2982 | `				"ValueError",` |
|         - | 2983 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 2984 | `				);` |
|         - | 2985 | `		}` |
|        34 | 2986 | `		bRecursive = iMode == 1;` |
|        16 | 2987 | `	}` |
|      1855 | 2988 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 2989 | `		/* Countable object: dispatch to ->count() */` |
|        73 | 2990 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        62 | 2991 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        62 | 2992 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        62 | 2993 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
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
|      1787 | 3013 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1787 | 3014 | `	if( bCycleDetected ){` |
|         3 | 3015 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3016 | `	}` |
|      1787 | 3017 | `	ph7_result_int64(pCtx,iCount);` |
|      1787 | 3018 | `	return PH7_OK;` |
|       936 | 3019 | `}` |
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
|        26 | 3154 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
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
|         4 | 4214 | `{` |
|         - | 4215 | `	ph7_hashmap_node *pNode;` |
|         - | 4216 | `	ph7_hashmap *pMap;` |
|         - | 4217 | `	ph7_value *pArray;` |
|         - | 4218 | `	ph7_value sObj;` |
|         - | 4219 | `	ph7_value sVal;` |
|         - | 4220 | `	SyString sKey;` |
|         - | 4221 | `	int bStrict;` |
|         - | 4222 | `	sxi32 rc;` |
|         - | 4223 | `	sxu32 n;` |
|       166 | 4224 | `	if( nArg < 1 ){` |
|         - | 4225 | `		/* Missing argument,throw ArgumentCountError */` |
|         3 | 4226 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4227 | `			"ArgumentCountError",` |
|         - | 4228 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4229 | `			);` |
|         - | 4230 | `	}` |
|         - | 4231 | `	/* Make sure we are dealing with a valid hashmap */` |
|       163 | 4232 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4233 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4234 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4235 | `			"TypeError",` |
|         - | 4236 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4237 | `			ph7_type_name(apArg[0])` |
|         - | 4238 | `			);` |
|         - | 4239 | `	}` |
|         - | 4240 | `	/* Point to the internal representation of the input hashmap */` |
|       161 | 4241 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4242 | `	/* Create a new array */` |
|       161 | 4243 | `	pArray = ph7_context_new_array(pCtx);` |
|       161 | 4244 | `	if( pArray == 0 ){` |
|       ! 0 | 4245 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4246 | `		return PH7_OK;` |
|         - | 4247 | `	}` |
|       161 | 4248 | `	bStrict = FALSE;` |
|       161 | 4249 | `	if( nArg > 2 ){` |
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
|       159 | 4261 | `	pNode = pMap->pFirst;` |
|       159 | 4262 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1455 | 4263 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1299 | 4264 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       179 | 4265 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        91 | 4266 | `		}else{` |
|      1122 | 4267 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1122 | 4268 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4269 | `		}` |
|      1299 | 4270 | `		rc = 0;` |
|      1299 | 4271 | `		if( nArg > 1 ){` |
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
|      1299 | 4287 | `		if( rc == 0 ){` |
|         - | 4288 | `			/* Perform the insertion */` |
|      1267 | 4289 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       632 | 4290 | `		}` |
|      1299 | 4291 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4292 | `		/* Point to the next entry */` |
|      1299 | 4293 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       651 | 4294 | `	}` |
|         - | 4295 | `	/* return the new array */` |
|       159 | 4296 | `	ph7_result_value(pCtx,pArray);` |
|       159 | 4297 | `	return PH7_OK;` |
|        85 | 4298 | `}` |
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
|      1052 | 4342 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4343 | `{` |
|         - | 4344 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4345 | `	ph7_value *pArray;` |
|         - | 4346 | `	int i;` |
|         - | 4347 | `	/* Create a new array */` |
|      1057 | 4348 | `	pArray = ph7_context_new_array(pCtx);` |
|      1057 | 4349 | `	if( pArray == 0 ){` |
|       ! 0 | 4350 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4351 | `		return PH7_OK;` |
|         - | 4352 | `	}` |
|         - | 4353 | `	/* Point to the internal representation of the hashmap */` |
|      1057 | 4354 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4355 | `	/* Start merging */` |
|      3151 | 4356 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4357 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2103 | 4358 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4359 | `			/* Type mismatch -> TypeError */` |
|         8 | 4360 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4361 | `				"TypeError",` |
|         - | 4362 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4363 | `				i + 1,` |
|         4 | 4364 | `				ph7_type_name(apArg[i])` |
|         - | 4365 | `				);` |
|       ! 0 | 4366 | `		}else{` |
|      2099 | 4367 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4368 | `			/* Merge the two hashmaps */` |
|      2099 | 4369 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4370 | `		}` |
|      1052 | 4371 | `	}` |
|         - | 4372 | `	/* Return the freshly created array */` |
|      1053 | 4373 | `	ph7_result_value(pCtx,pArray);` |
|      1053 | 4374 | `	return PH7_OK;` |
|       531 | 4375 | `}` |
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
|        43 | 4527 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        43 | 4528 | `	if( iOfft < 0 ){` |
|         5 | 4529 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4530 | `		if( iOfft < 0 ){` |
|         3 | 4531 | `			iOfft = 0;` |
|         1 | 4532 | `		}` |
|         2 | 4533 | `	}` |
|        43 | 4534 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4535 | `		/* Offset past end of array, return empty array */` |
|         5 | 4536 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4537 | `		if( pArray == 0 ){` |
|       ! 0 | 4538 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4539 | `			return PH7_OK;` |
|         - | 4540 | `		}` |
|         5 | 4541 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4542 | `		return PH7_OK;` |
|         - | 4543 | `	}` |
|         - | 4544 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        39 | 4545 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        39 | 4546 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        17 | 4547 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        17 | 4548 | `		if( iLength < 0 ){` |
|         5 | 4549 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4550 | `		}` |
|        17 | 4551 | `		if( iLength < 0 ){` |
|         3 | 4552 | `			iLength = 0;` |
|         1 | 4553 | `		}` |
|        17 | 4554 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4555 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4556 | `		}` |
|         8 | 4557 | `	}` |
|        39 | 4558 | `	if( nArg > 3 ){` |
|         5 | 4559 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4560 | `	}` |
|         - | 4561 | `	/* Create a new array */` |
|        39 | 4562 | `	pArray = ph7_context_new_array(pCtx);` |
|        39 | 4563 | `	if( pArray == 0 ){` |
|       ! 0 | 4564 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4565 | `		return PH7_OK;` |
|         - | 4566 | `	}` |
|        39 | 4567 | `	if( iLength < 1 ){` |
|         - | 4568 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4569 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4570 | `		return PH7_OK;` |
|         - | 4571 | `	}` |
|         - | 4572 | `	/* Point to the desired entry */` |
|        35 | 4573 | `	pCur = pSrc->pFirst;` |
|        29 | 4574 | `	for(;;){` |
|        63 | 4575 | `		if( iOfft < 1 ){` |
|        35 | 4576 | `			break;` |
|         - | 4577 | `		}` |
|         - | 4578 | `		/* Point to the next entry */` |
|        33 | 4579 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4580 | `		iOfft--;` |
|         5 | 4581 | `	}` |
|         - | 4582 | `	/* Point to the internal representation of the hashmap */` |
|        35 | 4583 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        54 | 4584 | `	for(;;){` |
|       113 | 4585 | `		if( iLength < 1 ){` |
|        35 | 4586 | `			break;` |
|         - | 4587 | `		}` |
|         - | 4588 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4589 | `		{` |
|        83 | 4590 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        83 | 4591 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4592 | `		}` |
|        83 | 4593 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4594 | `			break;` |
|         - | 4595 | `		}` |
|         - | 4596 | `		/* Point to the next entry */` |
|        83 | 4597 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        83 | 4598 | `		iLength--;` |
|         5 | 4599 | `	}` |
|         - | 4600 | `	/* Return the freshly created array */` |
|        35 | 4601 | `	ph7_result_value(pCtx,pArray);` |
|        35 | 4602 | `	return PH7_OK;` |
|        31 | 4603 | `}` |
|         - | 4604 | `/*` |
|         - | 4605 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4606 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4607 | ` * beginning (becomes the new pFirst).` |
|         - | 4608 | ` */` |
|        38 | 4609 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4610 | `{` |
|         - | 4611 | `	ph7_hashmap_node *pNode;` |
|         - | 4612 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4613 | `	pNode = pMap->pLast;` |
|        39 | 4614 | `	if( pNode == 0 ){` |
|       ! 0 | 4615 | `		return;` |
|         - | 4616 | `	}` |
|        39 | 4617 | `	if( pNode->pNext == 0 ){` |
|         - | 4618 | `		/* Only node in the list, nothing to move */` |
|         5 | 4619 | `		return;` |
|         - | 4620 | `	}` |
|        35 | 4621 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4622 | `		/* Already in the correct position */` |
|         9 | 4623 | `		return;` |
|         - | 4624 | `	}` |
|         - | 4625 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4626 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4627 | `	pMap->pLast->pPrev = 0;` |
|         - | 4628 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4629 | `	if( pAfter == 0 ){` |
|         - | 4630 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4631 | `		pNode->pNext = 0;` |
|         3 | 4632 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4633 | `		if( pMap->pFirst ){` |
|         3 | 4634 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4635 | `		}` |
|         3 | 4636 | `		pMap->pFirst = pNode;` |
|         2 | 4637 | `	}else{` |
|        25 | 4638 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4639 | `		pNode->pPrev = pOldNext;` |
|        25 | 4640 | `		pNode->pNext = pAfter;` |
|        25 | 4641 | `		pAfter->pPrev = pNode;` |
|        25 | 4642 | `		if( pOldNext ){` |
|        25 | 4643 | `			pOldNext->pNext = pNode;` |
|        13 | 4644 | `		}else{` |
|       ! 0 | 4645 | `			pMap->pLast = pNode;` |
|         - | 4646 | `		}` |
|         - | 4647 | `	}` |
|        20 | 4648 | `}` |
|         - | 4649 | `/*` |
|         - | 4650 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4651 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4652 | ` * Parameters` |
|         - | 4653 | ` *  $array` |
|         - | 4654 | ` *    The input array.` |
|         - | 4655 | ` *  $offset` |
|         - | 4656 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4657 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4658 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4659 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4660 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4661 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4662 | ` *  $length (optional)` |
|         - | 4663 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4664 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4665 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4666 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4667 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4668 | ` *  $replacement (optional)` |
|         - | 4669 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4670 | ` *    with elements from this array.` |
|         - | 4671 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4672 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4673 | ` *    offset.` |
|         - | 4674 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4675 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4676 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4677 | ` * Return` |
|         - | 4678 | ` *   A new array consisting of the extracted elements.` |
|         - | 4679 | ` */` |
|        68 | 4680 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4681 | `{` |
|         - | 4682 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4683 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4684 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4685 | `	int iLength,iOfft,i;` |
|         - | 4686 | `	sxi32 rc;` |
|        72 | 4687 | `	if( nArg < 2 ){` |
|         8 | 4688 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4689 | `			"ArgumentCountError",` |
|         - | 4690 | `			"array_splice() expects at least 2 arguments, %d given",` |
|         2 | 4691 | `			nArg` |
|         - | 4692 | `			);` |
|         - | 4693 | `	}` |
|        66 | 4694 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4695 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4696 | `			"TypeError",` |
|         - | 4697 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4698 | `			ph7_type_name(apArg[0])` |
|         - | 4699 | `			);` |
|         - | 4700 | `	}` |
|         - | 4701 | `	/* Point to the internal representation of the target array */` |
|        63 | 4702 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4703 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4704 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4705 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4706 | `	if( iOfft < 0 ){` |
|         9 | 4707 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4708 | `		if( iOfft < 0 ){` |
|         3 | 4709 | `			iOfft = 0;` |
|         2 | 4710 | `		}` |
|        59 | 4711 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4712 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4713 | `	}` |
|         - | 4714 | `	/* Get the length and clamp to valid range.` |
|         - | 4715 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4716 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4717 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4718 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4719 | `		if( iLength < 0 ){` |
|         7 | 4720 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4721 | `			if( iLength < 0 ){` |
|         3 | 4722 | `				iLength = 0;` |
|         1 | 4723 | `			}` |
|         3 | 4724 | `		}` |
|        45 | 4725 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4726 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4727 | `		}` |
|        22 | 4728 | `	}` |
|         - | 4729 | `	/* Create the result array for removed elements */` |
|        63 | 4730 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4731 | `	if( pArray == 0 ){` |
|       ! 0 | 4732 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4733 | `		return PH7_OK;` |
|         - | 4734 | `	}` |
|         - | 4735 | `	/* Get replacement array if provided */` |
|        63 | 4736 | `	pRep = 0;` |
|        63 | 4737 | `	if( nArg > 3 ){` |
|        27 | 4738 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4739 | `			/* Perform an array cast */` |
|         3 | 4740 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4741 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4742 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4743 | `			}` |
|         2 | 4744 | `		}else{` |
|        25 | 4745 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4746 | `		}` |
|        27 | 4747 | `		if( pRep ){` |
|         - | 4748 | `			/* Reset the loop cursor */` |
|        27 | 4749 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4750 | `		}` |
|        13 | 4751 | `	}` |
|         - | 4752 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4753 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4754 | `	/* Navigate to the offset position */` |
|        63 | 4755 | `	pCur = pSrc->pFirst;` |
|       131 | 4756 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4757 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4758 | `	}` |
|         - | 4759 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4760 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4761 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4762 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4763 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4764 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4765 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4766 | `		pPrev = pCur->pPrev;` |
|        79 | 4767 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4768 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4769 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4770 | `			break;` |
|         - | 4771 | `		}` |
|        79 | 4772 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4773 | `	}` |
|         - | 4774 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4775 | `	if( pRep ){` |
|         - | 4776 | `		ph7_value sSafeVal;` |
|        78 | 4777 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4778 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4779 | `			if( pRvalue ){` |
|         - | 4780 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4781 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4782 | `				 * since it points into that same pool. */` |
|        39 | 4783 | `				sSafeVal = *pRvalue;` |
|        39 | 4784 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4785 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4786 | `					pNewNode = pSrc->pLast;` |
|        39 | 4787 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4788 | `					pInsertAfter = pNewNode;` |
|        19 | 4789 | `				}` |
|        19 | 4790 | `			}` |
|         1 | 4791 | `		}` |
|        13 | 4792 | `	}` |
|         - | 4793 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4794 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4795 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4796 | `	 * and removals left gaps. */` |
|         - | 4797 | `	{` |
|        63 | 4798 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4799 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4800 | `		pSrc->iNextIdx = 0;` |
|       233 | 4801 | `		while( n > 0 ){` |
|       171 | 4802 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4803 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4804 | `			}` |
|       171 | 4805 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4806 | `			n--;` |
|         1 | 4807 | `		}` |
|        63 | 4808 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4809 | `	}` |
|         - | 4810 | `	/* Return the freshly created array */` |
|        63 | 4811 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4812 | `	return PH7_OK;` |
|        38 | 4813 | `}` |
|         - | 4814 | `/*` |
|         - | 4815 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4816 | ` *  Checks if a value exists in an array.` |
|         - | 4817 | ` * Parameters` |
|         - | 4818 | ` *  $needle` |
|         - | 4819 | ` *   The searched value.` |
|         - | 4820 | ` *   Note:` |
|         - | 4821 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4822 | ` * $haystack` |
|         - | 4823 | ` *  The target array.` |
|         - | 4824 | ` * $strict` |
|         - | 4825 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4826 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4827 | ` */` |
|     33336 | 4828 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4829 | `{` |
|         - | 4830 | `	ph7_value *pNeedle;` |
|         - | 4831 | `	int bStrict;` |
|         - | 4832 | `	int rc;` |
|     33341 | 4833 | `	if( nArg < 2 ){` |
|         - | 4834 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4835 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4836 | `		return PH7_OK;` |
|         - | 4837 | `	}` |
|     33341 | 4838 | `	pNeedle = apArg[0];` |
|     33341 | 4839 | `	bStrict = 0;` |
|     33341 | 4840 | `	if( nArg > 2 ){` |
|        53 | 4841 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4842 | `	}` |
|     33341 | 4843 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4844 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4845 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4846 | `		/* Set the comparison result */` |
|       ! 0 | 4847 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4848 | `		return PH7_OK;` |
|         - | 4849 | `	}` |
|         - | 4850 | `	/* Perform the lookup */` |
|     33341 | 4851 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4852 | `	/* Lookup result */` |
|     33341 | 4853 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     33341 | 4854 | `	return PH7_OK;` |
|     16673 | 4855 | `}` |
|         - | 4856 | `/*` |
|         - | 4857 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4858 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4859 | ` * Parameters` |
|         - | 4860 | ` * $needle` |
|         - | 4861 | ` *   The searched value.` |
|         - | 4862 | ` * $haystack` |
|         - | 4863 | ` *   The array.` |
|         - | 4864 | ` * $strict` |
|         - | 4865 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4866 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4867 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4868 | ` * Return` |
|         - | 4869 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4870 | ` */` |
|        32 | 4871 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4872 | `{` |
|         - | 4873 | `	ph7_hashmap_node *pEntry;` |
|         - | 4874 | `	ph7_value *pVal,sNeedle;` |
|         - | 4875 | `	ph7_hashmap *pMap;` |
|         - | 4876 | `	ph7_value sVal;` |
|         - | 4877 | `	int bStrict;` |
|         - | 4878 | `	sxu32 n;` |
|         - | 4879 | `	int rc;` |
|        37 | 4880 | `	if( nArg < 2 ){` |
|         - | 4881 | `		/* Missing argument,throw ArgumentCountError */` |
|         8 | 4882 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4883 | `			"ArgumentCountError",` |
|         - | 4884 | `			"array_search() expects at least 2 arguments, %d given",` |
|         2 | 4885 | `			nArg` |
|         - | 4886 | `			);` |
|         - | 4887 | `	}` |
|        31 | 4888 | `	bStrict = FALSE;` |
|        31 | 4889 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4890 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4891 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4892 | `			"TypeError",` |
|         - | 4893 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4894 | `			ph7_type_name(apArg[1])` |
|         - | 4895 | `			);` |
|         - | 4896 | `	}` |
|        28 | 4897 | `	if( nArg > 2 ){` |
|         - | 4898 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        14 | 4899 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4900 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4901 | `				"TypeError",` |
|         - | 4902 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4903 | `				ph7_type_name(apArg[2])` |
|         - | 4904 | `				);` |
|         - | 4905 | `		}` |
|        11 | 4906 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4907 | `	}` |
|         - | 4908 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4909 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4910 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4911 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 4912 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 4913 | `	pEntry = pMap->pFirst;` |
|        25 | 4914 | `	n = pMap->nEntry;` |
|        28 | 4915 | `	for(;;){` |
|        57 | 4916 | `		if( !n ){` |
|         9 | 4917 | `			break;` |
|         - | 4918 | `		}` |
|         - | 4919 | `		/* Extract node value */` |
|        49 | 4920 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 4921 | `		if( pVal ){` |
|         - | 4922 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4923 | `			 * can change their type.` |
|         - | 4924 | `			 */` |
|        49 | 4925 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 4926 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 4927 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 4928 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 4929 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 4930 | `			if( rc == 0 ){` |
|         - | 4931 | `				/* Match found,return key */` |
|        17 | 4932 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4933 | `					/* INT key */` |
|        11 | 4934 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 4935 | `				}else{` |
|         7 | 4936 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4937 | `					/* Blob key */` |
|         7 | 4938 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4939 | `				}` |
|        17 | 4940 | `				return PH7_OK;` |
|         - | 4941 | `			}` |
|        16 | 4942 | `		}` |
|         - | 4943 | `		/* Point to the next entry */` |
|        33 | 4944 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 4945 | `		n--;` |
|         1 | 4946 | `	}` |
|         - | 4947 | `	/* No such value,return FALSE */` |
|         9 | 4948 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4949 | `	return PH7_OK;` |
|        21 | 4950 | `}` |
|         - | 4951 | `/*` |
|         - | 4952 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4953 | ` *  Computes the difference of arrays.` |
|         - | 4954 | ` * Parameters` |
|         - | 4955 | ` *  $array1` |
|         - | 4956 | ` *    The array to compare from` |
|         - | 4957 | ` *  $array2` |
|         - | 4958 | ` *    An array to compare against` |
|         - | 4959 | ` *  $...` |
|         - | 4960 | ` *   More arrays to compare against` |
|         - | 4961 | ` * Return` |
|         - | 4962 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4963 | ` *  are not present in any of the other arrays.` |
|         - | 4964 | ` */` |
|        22 | 4965 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4966 | `{` |
|         - | 4967 | `	ph7_hashmap_node *pEntry;` |
|         - | 4968 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4969 | `	ph7_value *pArray;` |
|         - | 4970 | `	ph7_value *pVal;` |
|         - | 4971 | `	sxi32 rc;` |
|         - | 4972 | `	sxu32 n;` |
|         - | 4973 | `	int i;` |
|         - | 4974 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 4975 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 4976 | `	 * debugging difficult. */` |
|        26 | 4977 | `	if( nArg < 1 ){` |
|         4 | 4978 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4979 | `			"ArgumentCountError",` |
|         - | 4980 | `			"array_diff() expects at least 1 argument, %d given",` |
|         1 | 4981 | `			nArg` |
|         - | 4982 | `			);` |
|         - | 4983 | `	}` |
|        23 | 4984 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4985 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4986 | `			"TypeError",` |
|         - | 4987 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4988 | `			ph7_type_name(apArg[0])` |
|         - | 4989 | `			);` |
|         - | 4990 | `	}` |
|        36 | 4991 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 4992 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 4993 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4994 | `				"TypeError",` |
|         - | 4995 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 4996 | `				i + 1,` |
|         2 | 4997 | `				ph7_type_name(apArg[i])` |
|         - | 4998 | `				);` |
|         - | 4999 | `		}` |
|         9 | 5000 | `	}` |
|        17 | 5001 | `	if( nArg == 1 ){` |
|         - | 5002 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5003 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5004 | `		return PH7_OK;` |
|         - | 5005 | `	}` |
|         - | 5006 | `	/* Create a new array */` |
|        15 | 5007 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5008 | `	if( pArray == 0 ){` |
|       ! 0 | 5009 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5010 | `		return PH7_OK;` |
|         - | 5011 | `	}` |
|         - | 5012 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5013 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5014 | `	/* Perform the diff */` |
|        15 | 5015 | `	pEntry = pSrc->pFirst;` |
|        15 | 5016 | `	n = pSrc->nEntry;` |
|        27 | 5017 | `	for(;;){` |
|        55 | 5018 | `		if( n < 1 ){` |
|        15 | 5019 | `			break;` |
|         - | 5020 | `		}` |
|         - | 5021 | `		/* Extract the node value */` |
|        41 | 5022 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5023 | `		if( pVal ){` |
|        69 | 5024 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5025 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5026 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5027 | `				/* Perform the lookup */` |
|        45 | 5028 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5029 | `				if( rc == SXRET_OK ){` |
|         - | 5030 | `					/* Value exist */` |
|        17 | 5031 | `					break;` |
|         - | 5032 | `				}` |
|        15 | 5033 | `			}` |
|        41 | 5034 | `			if( i >= nArg ){` |
|         - | 5035 | `				/* Perform the insertion */` |
|        25 | 5036 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5037 | `			}` |
|        20 | 5038 | `		}` |
|         - | 5039 | `		/* Point to the next entry */` |
|        41 | 5040 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5041 | `		n--;` |
|         1 | 5042 | `	}` |
|         - | 5043 | `	/* Return the freshly created array */` |
|        15 | 5044 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5045 | `	return PH7_OK;` |
|        15 | 5046 | `}` |
|         - | 5047 | `/*` |
|         - | 5048 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5049 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5050 | ` * Parameters` |
|         - | 5051 | ` *  $array1` |
|         - | 5052 | ` *    The array to compare from` |
|         - | 5053 | ` *  $array2` |
|         - | 5054 | ` *    An array to compare against` |
|         - | 5055 | ` *  $...` |
|         - | 5056 | ` *   More arrays to compare against.` |
|         - | 5057 | ` * $callback` |
|         - | 5058 | ` *  The callback comparison function.` |
|         - | 5059 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5060 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5061 | ` *  than the second.` |
|         - | 5062 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5063 | ` * Return` |
|         - | 5064 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5065 | ` *  are not present in any of the other arrays.` |
|         - | 5066 | ` */` |
|        22 | 5067 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5068 | `{` |
|         - | 5069 | `	ph7_hashmap_node *pEntry;` |
|         - | 5070 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5071 | `	ph7_value *pCallback;` |
|         - | 5072 | `	ph7_value *pArray;` |
|         - | 5073 | `	ph7_value *pVal;` |
|         - | 5074 | `	sxi32 rc;` |
|         - | 5075 | `	sxu32 n;` |
|         - | 5076 | `	int i;` |
|         - | 5077 |  |
|         - | 5078 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        27 | 5079 | `	if( nArg < 2 ){` |
|         4 | 5080 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5081 | `			"ArgumentCountError",` |
|         - | 5082 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|         1 | 5083 | `			nArg` |
|         - | 5084 | `			);` |
|         - | 5085 | `	}` |
|        25 | 5086 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5087 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5088 | `			"TypeError",` |
|         - | 5089 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5090 | `			ph7_type_name(apArg[0])` |
|         - | 5091 | `			);` |
|         - | 5092 | `	}` |
|         - | 5093 |  |
|        23 | 5094 | `	if( nArg == 2 ){` |
|         - | 5095 | `		/* Only the original array and the callback were provided. */` |
|         - | 5096 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5097 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5098 | `		 * validation order.` |
|         - | 5099 | `		 */` |
|         4 | 5100 | `	} else {` |
|         - | 5101 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5102 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5103 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5104 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5105 | `					"TypeError",` |
|         - | 5106 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5107 | `					i + 1,` |
|         6 | 5108 | `					ph7_type_name(apArg[i])` |
|         - | 5109 | `					);` |
|         - | 5110 | `			}` |
|         7 | 5111 | `		}` |
|         - | 5112 | `	}` |
|         - | 5113 |  |
|         - | 5114 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5115 | `	pCallback = apArg[nArg - 1];` |
|         - | 5116 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5117 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5118 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5119 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5120 | `				"TypeError",` |
|         - | 5121 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5122 | `				nArg` |
|         - | 5123 | `				);` |
|         - | 5124 | `		}` |
|         6 | 5125 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5126 | `			int len;` |
|         3 | 5127 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5128 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5129 | `				"TypeError",` |
|         - | 5130 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5131 | `				nArg,` |
|         1 | 5132 | `				zName` |
|         - | 5133 | `				);` |
|         - | 5134 | `		}` |
|         4 | 5135 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5136 | `			"TypeError",` |
|         - | 5137 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5138 | `			nArg` |
|         - | 5139 | `			);` |
|         - | 5140 | `	}` |
|         - | 5141 |  |
|         7 | 5142 | `	if( nArg == 2 ){` |
|         - | 5143 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5144 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5145 | `		return PH7_OK;` |
|         - | 5146 | `	}` |
|         - | 5147 |  |
|         - | 5148 | `	/* Create a new array */` |
|         5 | 5149 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5150 | `	if( pArray == 0 ){` |
|       ! 0 | 5151 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5152 | `		return PH7_OK;` |
|         - | 5153 | `	}` |
|         - | 5154 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5155 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5156 | `	/* Perform the diff */` |
|         5 | 5157 | `	pEntry = pSrc->pFirst;` |
|         5 | 5158 | `	n = pSrc->nEntry;` |
|         5 | 5159 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5160 | `	for(;;){` |
|        11 | 5161 | `		if( n < 1 ){` |
|         3 | 5162 | `			break;` |
|         - | 5163 | `		}` |
|         - | 5164 | `		/* Extract the node value */` |
|         9 | 5165 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5166 | `		if( pVal ){` |
|        15 | 5167 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5168 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5169 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5170 | `				/* Perform the lookup */` |
|         9 | 5171 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5172 | `				if( rc == SXRET_OK ){` |
|         - | 5173 | `					/* Value exist */` |
|         3 | 5174 | `					break;` |
|         - | 5175 | `				}` |
|         4 | 5176 | `			}` |
|         9 | 5177 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5178 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5179 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5180 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5181 | `				return PH7_EXCEPTION;` |
|         - | 5182 | `			}` |
|         7 | 5183 | `			if( i >= (nArg - 1)){` |
|         - | 5184 | `				/* Perform the insertion */` |
|         5 | 5185 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5186 | `			}` |
|         3 | 5187 | `		}` |
|         - | 5188 | `		/* Point to the next entry */` |
|         7 | 5189 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5190 | `		n--;` |
|         1 | 5191 | `	}` |
|         - | 5192 | `	/* Return the freshly created array */` |
|         3 | 5193 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5194 | `	return PH7_OK;` |
|        16 | 5195 | `}` |
|         - | 5196 | `/*` |
|         - | 5197 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5198 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5199 | ` * Parameters` |
|         - | 5200 | ` *  $array1` |
|         - | 5201 | ` *    The array to compare from` |
|         - | 5202 | ` *  $array2` |
|         - | 5203 | ` *    An array to compare against` |
|         - | 5204 | ` *  $...` |
|         - | 5205 | ` *   More arrays to compare against` |
|         - | 5206 | ` * Return` |
|         - | 5207 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5208 | ` *  are not present in any of the other arrays.` |
|         - | 5209 | ` */` |
|        22 | 5210 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5211 | `{` |
|         - | 5212 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5213 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5214 | `	ph7_value *pArray;` |
|         - | 5215 | `	ph7_value *pVal;` |
|         - | 5216 | `	sxi32 rc;` |
|         - | 5217 | `	sxu32 n;` |
|         - | 5218 | `	int i;` |
|         - | 5219 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5220 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5221 | `	 * accompanying integration tests to pass. */` |
|        27 | 5222 | `	if( nArg < 1 ){` |
|         4 | 5223 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5224 | `			"ArgumentCountError",` |
|         - | 5225 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|         1 | 5226 | `			nArg` |
|         - | 5227 | `			);` |
|         - | 5228 | `	}` |
|        24 | 5229 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5230 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5231 | `			"TypeError",` |
|         - | 5232 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5233 | `			ph7_type_name(apArg[0])` |
|         - | 5234 | `			);` |
|         - | 5235 | `	}` |
|        37 | 5236 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5237 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5238 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5239 | `				"TypeError",` |
|         - | 5240 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5241 | `				i + 1,` |
|         4 | 5242 | `				ph7_type_name(apArg[i])` |
|         - | 5243 | `				);` |
|         - | 5244 | `		}` |
|        10 | 5245 | `	}` |
|        15 | 5246 | `	if( nArg == 1 ){` |
|         - | 5247 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5248 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5249 | `		return PH7_OK;` |
|         - | 5250 | `	}` |
|         - | 5251 | `	/* Create a new array */` |
|        13 | 5252 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5253 | `	if( pArray == 0 ){` |
|       ! 0 | 5254 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5255 | `		return PH7_OK;` |
|         - | 5256 | `	}` |
|         - | 5257 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5258 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5259 | `	/* Perform the diff */` |
|        13 | 5260 | `	pEntry = pSrc->pFirst;` |
|        13 | 5261 | `	n = pSrc->nEntry;` |
|        13 | 5262 | `	pN1 = pN2 = 0;` |
|        34 | 5263 | `	for(;;){` |
|         - | 5264 | `		int keep;` |
|        41 | 5265 | `		if( n < 1 ){` |
|        13 | 5266 | `			break;` |
|         - | 5267 | `		}` |
|         - | 5268 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5269 | `		keep = 1;` |
|        47 | 5270 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5271 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5272 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5273 | `			/* Perform a key lookup first */` |
|        33 | 5274 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5275 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5276 | `			}else{` |
|        21 | 5277 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5278 | `			}` |
|        33 | 5279 | `			if( rc != SXRET_OK ){` |
|         - | 5280 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5281 | `				continue;` |
|         - | 5282 | `			}` |
|         - | 5283 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5284 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5285 | `			if( pVal ){` |
|         - | 5286 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5287 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5288 | `				if( pVal2 ){` |
|         - | 5289 | `					ph7_value sV1,sV2;` |
|         - | 5290 | `					sxi32 cmp;` |
|         - | 5291 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5292 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5293 | `					 * null element used to come back bool(false) in the` |
|         - | 5294 | `					 * caller's array). */` |
|        17 | 5295 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5296 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5297 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5298 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5299 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5300 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5301 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5302 | `					if( cmp == 0 ){` |
|         - | 5303 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5304 | `						keep = 0;` |
|        15 | 5305 | `						break;` |
|         - | 5306 | `					}` |
|         1 | 5307 | `				}` |
|         1 | 5308 | `			}` |
|         2 | 5309 | `		}` |
|        29 | 5310 | `		if( keep ){` |
|         - | 5311 | `			/* Perform the insertion */` |
|        15 | 5312 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5313 | `		}` |
|         - | 5314 | `		/* Point to the next entry */` |
|        29 | 5315 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5316 | `		n--;` |
|         1 | 5317 | `	}` |
|         - | 5318 | `	/* Return the freshly created array */` |
|        13 | 5319 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5320 | `	return PH7_OK;` |
|        16 | 5321 | `}` |
|         - | 5322 | `/*` |
|         - | 5323 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5324 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5325 | ` *  by a user supplied callback function.` |
|         - | 5326 | ` * Parameters` |
|         - | 5327 | ` *  $array1` |
|         - | 5328 | ` *    The array to compare from` |
|         - | 5329 | ` *  $array2` |
|         - | 5330 | ` *    An array to compare against` |
|         - | 5331 | ` *  $...` |
|         - | 5332 | ` *   More arrays to compare against.` |
|         - | 5333 | ` *  $key_compare_func` |
|         - | 5334 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5335 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5336 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5337 | ` * Return` |
|         - | 5338 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5339 | ` *  are not present in any of the other arrays.` |
|         - | 5340 | ` */` |
|        24 | 5341 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5342 | `{` |
|         - | 5343 | `	ph7_hashmap_node *pEntry;` |
|         - | 5344 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5345 | `	ph7_value *pCallback;` |
|         - | 5346 | `	ph7_value *pArray;` |
|         - | 5347 | `	sxi32 rc;` |
|         - | 5348 | `	sxu32 n;` |
|         - | 5349 | `	int i;` |
|         - | 5350 |  |
|         - | 5351 | `	/* Argument validation mimicking PHP errors. */` |
|        29 | 5352 | `	if( nArg < 2 ){` |
|         4 | 5353 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5354 | `			"ArgumentCountError",` |
|         - | 5355 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|         1 | 5356 | `			nArg` |
|         - | 5357 | `			);` |
|         - | 5358 | `	}` |
|        26 | 5359 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5360 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5361 | `			"TypeError",` |
|         - | 5362 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5363 | `			ph7_type_name(apArg[0])` |
|         - | 5364 | `			);` |
|         - | 5365 | `	}` |
|         - | 5366 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5367 | `	 * expected to be a callback. */` |
|        38 | 5368 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5369 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5370 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5371 | `				"TypeError",` |
|         - | 5372 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5373 | `				i + 1,` |
|         2 | 5374 | `				ph7_type_name(apArg[i])` |
|         - | 5375 | `				);` |
|         - | 5376 | `		}` |
|         9 | 5377 | `	}` |
|         - | 5378 | `	/* Point to the callback value */` |
|        22 | 5379 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5380 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5381 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5382 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5383 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5384 | `		 * string given" which we also reproduce. */` |
|         9 | 5385 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5386 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5387 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5388 | `				"TypeError",` |
|         - | 5389 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5390 | `				nArg` |
|         - | 5391 | `				);` |
|         - | 5392 | `		}` |
|         6 | 5393 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5394 | `			/* neither array nor string */` |
|         8 | 5395 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5396 | `				"TypeError",` |
|         - | 5397 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5398 | `				nArg` |
|         - | 5399 | `				);` |
|         - | 5400 | `		}` |
|         - | 5401 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5402 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5403 | `			"TypeError",` |
|         - | 5404 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5405 | `			nArg,` |
|       ! 0 | 5406 | `			ph7_type_name(pCallback)` |
|         - | 5407 | `			);` |
|         - | 5408 | `	}` |
|        13 | 5409 | `	if( nArg == 2 ){` |
|         - | 5410 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5411 | `		 * input array. */` |
|         3 | 5412 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5413 | `		return PH7_OK;` |
|         - | 5414 | `	}` |
|         - | 5415 | `	/* Create a new array */` |
|        11 | 5416 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5417 | `	if( pArray == 0 ){` |
|       ! 0 | 5418 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5419 | `		return PH7_OK;` |
|         - | 5420 | `	}` |
|         - | 5421 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5422 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5423 | `	/* Perform the diff */` |
|        11 | 5424 | `	pEntry = pSrc->pFirst;` |
|        11 | 5425 | `	n = pSrc->nEntry;` |
|        21 | 5426 | `	for(;;){` |
|         - | 5427 | `		int keep;` |
|        27 | 5428 | `		if( n < 1 ){` |
|         9 | 5429 | `			break;` |
|         - | 5430 | `		}` |
|        19 | 5431 | `		keep = 1;` |
|        31 | 5432 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5433 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5434 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5435 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5436 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5437 | `			while( pIt ){` |
|         - | 5438 | `				/* build temporary key values for callback */` |
|         - | 5439 | `				ph7_value key1, key2, result;` |
|         - | 5440 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5441 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5442 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5443 | `				}else{` |
|         - | 5444 | `					SyString sStr;` |
|        33 | 5445 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5446 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5447 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5448 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5449 | `				}` |
|        33 | 5450 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5451 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5452 | `				}else{` |
|         - | 5453 | `					SyString sStr;` |
|        33 | 5454 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5455 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5456 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5457 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5458 | `				}` |
|        33 | 5459 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5460 | `				/* call user callback with (key1, key2) */` |
|         - | 5461 | `				{` |
|         - | 5462 | `					ph7_value *apK[2];` |
|        33 | 5463 | `					apK[0] = &key1;` |
|        33 | 5464 | `					apK[1] = &key2;` |
|        33 | 5465 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5466 | `				}` |
|        33 | 5467 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5468 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5469 | `					 * array_uintersect (which signal back from` |
|         - | 5470 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5471 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5472 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5473 | `					PH7_MemObjRelease(&result);` |
|         3 | 5474 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5475 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5476 | `					return PH7_EXCEPTION;` |
|         - | 5477 | `				}` |
|        31 | 5478 | `				if( rc == SXRET_OK ){` |
|        31 | 5479 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5480 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5481 | `					}` |
|        31 | 5482 | `					if( result.x.iVal == 0 ){` |
|         - | 5483 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5484 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5485 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5486 | `						if( pVal1 && pVal2 ){` |
|         - | 5487 | `							ph7_value sV1,sV2;` |
|         - | 5488 | `							sxi32 cmp;` |
|         - | 5489 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5490 | `							 * place and these are LIVE array elements. */` |
|        13 | 5491 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5492 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5493 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5494 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5495 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5496 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5497 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5498 | `							if( cmp == 0 ){` |
|         9 | 5499 | `								keep = 0;` |
|         9 | 5500 | `								PH7_MemObjRelease(&result);` |
|         - | 5501 | `								/* release keys too before breaking */` |
|         9 | 5502 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5503 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5504 | `								break;` |
|         - | 5505 | `							}` |
|         2 | 5506 | `						}` |
|         2 | 5507 | `					}` |
|        11 | 5508 | `				}` |
|        23 | 5509 | `				PH7_MemObjRelease(&result);` |
|        23 | 5510 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5511 | `				PH7_MemObjRelease(&key2);` |
|         - | 5512 | `				/* move to next node */` |
|        23 | 5513 | `				pIt = pIt->pPrev;` |
|        23 | 5514 | `				if( keep == 0 ) break;` |
|         1 | 5515 | `			}` |
|        21 | 5516 | `			if( keep == 0 ) break;` |
|         7 | 5517 | `		}` |
|        17 | 5518 | `		if( keep ){` |
|         - | 5519 | `			/* Perform the insertion */` |
|         9 | 5520 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5521 | `		}` |
|         - | 5522 | `		/* Point to the next entry */` |
|        17 | 5523 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5524 | `		n--;` |
|         1 | 5525 | `	}` |
|         - | 5526 | `	/* Return the freshly created array */` |
|         9 | 5527 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5528 | `	return PH7_OK;` |
|        17 | 5529 | `}` |
|         - | 5530 | `/*` |
|         - | 5531 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5532 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5533 | ` * Parameters` |
|         - | 5534 | ` *  $array1` |
|         - | 5535 | ` *    The array to compare from` |
|         - | 5536 | ` *  $array2` |
|         - | 5537 | ` *    An array to compare against` |
|         - | 5538 | ` *  $...` |
|         - | 5539 | ` *   More arrays to compare against` |
|         - | 5540 | ` * Return` |
|         - | 5541 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5542 | ` *  in any of the other arrays.` |
|         - | 5543 | ` * Note that NULL is returned on failure.` |
|         - | 5544 | ` */` |
|        14 | 5545 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5546 | `{` |
|         - | 5547 | `	ph7_hashmap_node *pEntry;` |
|         - | 5548 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5549 | `	ph7_value *pArray;` |
|         - | 5550 | `	sxi32 rc;` |
|         - | 5551 | `	sxu32 n;` |
|         - | 5552 | `	int i;` |
|         - | 5553 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5554 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5555 | `	 * helpers. */` |
|        18 | 5556 | `	if( nArg < 1 ){` |
|         4 | 5557 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5558 | `			"ArgumentCountError",` |
|         - | 5559 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|         1 | 5560 | `			nArg` |
|         - | 5561 | `			);` |
|         - | 5562 | `	}` |
|        15 | 5563 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5564 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5565 | `			"TypeError",` |
|         - | 5566 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5567 | `			ph7_type_name(apArg[0])` |
|         - | 5568 | `			);` |
|         - | 5569 | `	}` |
|        20 | 5570 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5571 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5572 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5573 | `				"TypeError",` |
|         - | 5574 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5575 | `				i + 1,` |
|         2 | 5576 | `				ph7_type_name(apArg[i])` |
|         - | 5577 | `				);` |
|         - | 5578 | `		}` |
|         5 | 5579 | `	}` |
|         9 | 5580 | `	if( nArg == 1 ){` |
|         - | 5581 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5582 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5583 | `		return PH7_OK;` |
|         - | 5584 | `	}` |
|         - | 5585 | `	/* Create a new array */` |
|         7 | 5586 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5587 | `	if( pArray == 0 ){` |
|       ! 0 | 5588 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5589 | `		return PH7_OK;` |
|         - | 5590 | `	}` |
|         - | 5591 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5592 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5593 | `	/* Perfrom the diff */` |
|         7 | 5594 | `	pEntry = pSrc->pFirst;` |
|         7 | 5595 | `	n = pSrc->nEntry;` |
|        12 | 5596 | `	for(;;){` |
|        25 | 5597 | `		if( n < 1 ){` |
|         7 | 5598 | `			break;` |
|         - | 5599 | `		}` |
|        31 | 5600 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5601 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5602 | `				/* ignore */` |
|       ! 0 | 5603 | `				continue;` |
|         - | 5604 | `			}` |
|        23 | 5605 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5606 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5607 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5608 | `				/* Blob lookup */` |
|        17 | 5609 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5610 | `			}else{` |
|         - | 5611 | `				/* Int lookup */` |
|         7 | 5612 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5613 | `			}` |
|        23 | 5614 | `			if( rc == SXRET_OK ){` |
|         - | 5615 | `				/* Key exists,break immediately */` |
|        11 | 5616 | `				break;` |
|         - | 5617 | `			}` |
|         7 | 5618 | `		}` |
|        19 | 5619 | `		if( i >= nArg ){` |
|         - | 5620 | `			/* Perform the insertion */` |
|         9 | 5621 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5622 | `		}` |
|         - | 5623 | `		/* Point to the next entry */` |
|        19 | 5624 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5625 | `		n--;` |
|         1 | 5626 | `	}` |
|         - | 5627 | `	/* Return the freshly created array */` |
|         7 | 5628 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5629 | `	return PH7_OK;` |
|        11 | 5630 | `}` |
|         - | 5631 | `/*` |
|         - | 5632 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5633 | ` *  Computes the intersection of arrays.` |
|         - | 5634 | ` * Parameters` |
|         - | 5635 | ` *  $array1` |
|         - | 5636 | ` *    The array to compare from` |
|         - | 5637 | ` *  $array2` |
|         - | 5638 | ` *    An array to compare against` |
|         - | 5639 | ` *  $...` |
|         - | 5640 | ` *   More arrays to compare against` |
|         - | 5641 | ` * Return` |
|         - | 5642 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5643 | ` *  in all of the parameters.` |
|         - | 5644 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5645 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5646 | ` */` |
|        22 | 5647 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5648 | `{` |
|         - | 5649 | `	ph7_hashmap_node *pEntry;` |
|         - | 5650 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5651 | `	ph7_value *pArray;` |
|         - | 5652 | `	ph7_value *pVal;` |
|         - | 5653 | `	sxi32 rc;` |
|         - | 5654 | `	sxu32 n;` |
|         - | 5655 | `	int i;` |
|        26 | 5656 | `	if( nArg < 1 ){` |
|         4 | 5657 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5658 | `			"ArgumentCountError",` |
|         - | 5659 | `			"array_intersect() expects at least 1 argument, %d given",` |
|         1 | 5660 | `			nArg` |
|         - | 5661 | `			);` |
|         - | 5662 | `	}` |
|        23 | 5663 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5664 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5665 | `			"TypeError",` |
|         - | 5666 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5667 | `			ph7_type_name(apArg[0])` |
|         - | 5668 | `			);` |
|         - | 5669 | `	}` |
|        36 | 5670 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5671 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5672 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5673 | `				"TypeError",` |
|         - | 5674 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5675 | `				i + 1,` |
|         2 | 5676 | `				ph7_type_name(apArg[i])` |
|         - | 5677 | `				);` |
|         - | 5678 | `		}` |
|         9 | 5679 | `	}` |
|        17 | 5680 | `	if( nArg == 1 ){` |
|         - | 5681 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5682 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5683 | `		return PH7_OK;` |
|         - | 5684 | `	}` |
|         - | 5685 | `	/* Create a new array */` |
|        15 | 5686 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5687 | `	if( pArray == 0 ){` |
|       ! 0 | 5688 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5689 | `		return PH7_OK;` |
|         - | 5690 | `	}` |
|         - | 5691 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5692 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5693 | `	/* Perform the intersection */` |
|        15 | 5694 | `	pEntry = pSrc->pFirst;` |
|        15 | 5695 | `	n = pSrc->nEntry;` |
|        31 | 5696 | `	for(;;){` |
|        63 | 5697 | `		if( n < 1 ){` |
|        15 | 5698 | `			break;` |
|         - | 5699 | `		}` |
|         - | 5700 | `		/* Extract the node value */` |
|        49 | 5701 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5702 | `		if( pVal ){` |
|        79 | 5703 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5704 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5705 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5706 | `				/* Perform the lookup */` |
|        55 | 5707 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5708 | `				if( rc != SXRET_OK ){` |
|         - | 5709 | `					/* Value does not exist */` |
|        25 | 5710 | `					break;` |
|         - | 5711 | `				}` |
|        16 | 5712 | `			}` |
|        49 | 5713 | `			if( i >= nArg ){` |
|         - | 5714 | `				/* Perform the insertion */` |
|        25 | 5715 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5716 | `			}` |
|        24 | 5717 | `		}` |
|         - | 5718 | `		/* Point to the next entry */` |
|        49 | 5719 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5720 | `		n--;` |
|         1 | 5721 | `	}` |
|         - | 5722 | `	/* Return the freshly created array */` |
|        15 | 5723 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5724 | `	return PH7_OK;` |
|        15 | 5725 | `}` |
|         - | 5726 | `/*` |
|         - | 5727 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5728 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5729 | ` * Parameters` |
|         - | 5730 | ` *  $array1` |
|         - | 5731 | ` *    The array to compare from` |
|         - | 5732 | ` *  $array2` |
|         - | 5733 | ` *    An array to compare against` |
|         - | 5734 | ` *  $...` |
|         - | 5735 | ` *   More arrays to compare against` |
|         - | 5736 | ` * Return` |
|         - | 5737 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5738 | ` *  in all the arguments, with matching keys.` |
|         - | 5739 | ` */` |
|        22 | 5740 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5741 | `{` |
|         - | 5742 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5743 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5744 | `	ph7_value *pArray;` |
|         - | 5745 | `	ph7_value *pVal;` |
|         - | 5746 | `	sxi32 rc;` |
|         - | 5747 | `	sxu32 n;` |
|         - | 5748 | `	int i;` |
|        26 | 5749 | `	if( nArg < 1 ){` |
|         4 | 5750 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5751 | `			"ArgumentCountError",` |
|         - | 5752 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|         1 | 5753 | `			nArg` |
|         - | 5754 | `			);` |
|         - | 5755 | `	}` |
|        23 | 5756 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5757 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5758 | `			"TypeError",` |
|         - | 5759 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5760 | `			ph7_type_name(apArg[0])` |
|         - | 5761 | `			);` |
|         - | 5762 | `	}` |
|        36 | 5763 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5764 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5765 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5766 | `				"TypeError",` |
|         - | 5767 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5768 | `				i + 1,` |
|         2 | 5769 | `				ph7_type_name(apArg[i])` |
|         - | 5770 | `				);` |
|         - | 5771 | `		}` |
|         9 | 5772 | `	}` |
|        17 | 5773 | `	if( nArg == 1 ){` |
|         - | 5774 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5775 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5776 | `		return PH7_OK;` |
|         - | 5777 | `	}` |
|         - | 5778 | `	/* Create a new array */` |
|        15 | 5779 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5780 | `	if( pArray == 0 ){` |
|       ! 0 | 5781 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5782 | `		return PH7_OK;` |
|         - | 5783 | `	}` |
|         - | 5784 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5785 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5786 | `	/* Perform the intersection */` |
|        15 | 5787 | `	pEntry = pSrc->pFirst;` |
|        15 | 5788 | `	n = pSrc->nEntry;` |
|        15 | 5789 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5790 | `	for(;;){` |
|        47 | 5791 | `		if( n < 1 ){` |
|        15 | 5792 | `			break;` |
|         - | 5793 | `		}` |
|         - | 5794 | `		/* Extract the node value */` |
|        33 | 5795 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5796 | `		if( pVal ){` |
|        53 | 5797 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5798 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5799 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5800 | `				/* Perform a key lookup first */` |
|        37 | 5801 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5802 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5803 | `				}else{` |
|        23 | 5804 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5805 | `				}` |
|        37 | 5806 | `				if( rc != SXRET_OK ){` |
|         - | 5807 | `					/* No such key,break immediately */` |
|         7 | 5808 | `					break;` |
|         - | 5809 | `				}` |
|         - | 5810 | `				/* Perform the lookup */` |
|        31 | 5811 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5812 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5813 | `					/* Value does not exist */` |
|         6 | 5814 | `					break;` |
|         - | 5815 | `				}` |
|        11 | 5816 | `			}` |
|        33 | 5817 | `			if( i >= nArg ){` |
|         - | 5818 | `				/* Perform the insertion */` |
|        17 | 5819 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5820 | `			}` |
|        16 | 5821 | `		}` |
|         - | 5822 | `		/* Point to the next entry */` |
|        33 | 5823 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5824 | `		n--;` |
|         1 | 5825 | `	}` |
|         - | 5826 | `	/* Return the freshly created array */` |
|        15 | 5827 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5828 | `	return PH7_OK;` |
|        15 | 5829 | `}` |
|         - | 5830 | `/*` |
|         - | 5831 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5832 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5833 | ` * Parameters` |
|         - | 5834 | ` *  $array1` |
|         - | 5835 | ` *    The array to compare from` |
|         - | 5836 | ` *  $...` |
|         - | 5837 | ` *   More arrays to compare against` |
|         - | 5838 | ` * Return` |
|         - | 5839 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5840 | ` *  have keys that are present in all arguments.` |
|         - | 5841 | ` * Note that NULL is returned on failure.` |
|         - | 5842 | ` */` |
|        22 | 5843 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5844 | `{` |
|         - | 5845 | `	ph7_hashmap_node *pEntry;` |
|         - | 5846 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5847 | `	ph7_value *pArray;` |
|         - | 5848 | `	sxi32 rc;` |
|         - | 5849 | `	sxu32 n;` |
|         - | 5850 | `	int i;` |
|        26 | 5851 | `	if( nArg < 1 ){` |
|         4 | 5852 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5853 | `			"ArgumentCountError",` |
|         - | 5854 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|         1 | 5855 | `			nArg` |
|         - | 5856 | `			);` |
|         - | 5857 | `	}` |
|        23 | 5858 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5859 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5860 | `			"TypeError",` |
|         - | 5861 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5862 | `			ph7_type_name(apArg[0])` |
|         - | 5863 | `			);` |
|         - | 5864 | `	}` |
|        36 | 5865 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5866 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5867 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5868 | `				"TypeError",` |
|         - | 5869 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5870 | `				i + 1,` |
|         2 | 5871 | `				ph7_type_name(apArg[i])` |
|         - | 5872 | `				);` |
|         - | 5873 | `		}` |
|         9 | 5874 | `	}` |
|        17 | 5875 | `	if( nArg == 1 ){` |
|         - | 5876 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5877 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5878 | `		return PH7_OK;` |
|         - | 5879 | `	}` |
|         - | 5880 | `	/* Create a new array */` |
|        15 | 5881 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5882 | `	if( pArray == 0 ){` |
|       ! 0 | 5883 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5884 | `		return PH7_OK;` |
|         - | 5885 | `	}` |
|         - | 5886 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5887 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5888 | `	/* Perform the intersection */` |
|        15 | 5889 | `	pEntry = pSrc->pFirst;` |
|        15 | 5890 | `	n = pSrc->nEntry;` |
|        24 | 5891 | `	for(;;){` |
|        49 | 5892 | `		if( n < 1 ){` |
|        15 | 5893 | `			break;` |
|         - | 5894 | `		}` |
|        57 | 5895 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5896 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5897 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5898 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5899 | `				/* Blob lookup */` |
|        27 | 5900 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5901 | `			}else{` |
|         - | 5902 | `				/* Int key */` |
|        13 | 5903 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5904 | `			}` |
|        39 | 5905 | `			if( rc != SXRET_OK ){` |
|         - | 5906 | `				/* Key does not exist, break immediately */` |
|        17 | 5907 | `				break;` |
|         - | 5908 | `			}` |
|        12 | 5909 | `		}` |
|        35 | 5910 | `		if( i >= nArg ){` |
|         - | 5911 | `			/* Perform the insertion */` |
|        19 | 5912 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5913 | `		}` |
|         - | 5914 | `		/* Point to the next entry */` |
|        35 | 5915 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5916 | `		n--;` |
|         1 | 5917 | `	}` |
|         - | 5918 | `	/* Return the freshly created array */` |
|        15 | 5919 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5920 | `	return PH7_OK;` |
|        15 | 5921 | `}` |
|         - | 5922 | `/*` |
|         - | 5923 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5924 | ` *  Computes the intersection of arrays.` |
|         - | 5925 | ` * Parameters` |
|         - | 5926 | ` *  $array1` |
|         - | 5927 | ` *    The array to compare from` |
|         - | 5928 | ` *  $array2` |
|         - | 5929 | ` *    An array to compare against` |
|         - | 5930 | ` *  $...` |
|         - | 5931 | ` *   More arrays to compare against` |
|         - | 5932 | ` * $callback` |
|         - | 5933 | ` *  The callback comparison function.` |
|         - | 5934 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5935 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5936 | ` *  than the second.` |
|         - | 5937 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5938 | ` * Return` |
|         - | 5939 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5940 | ` *  in all of the parameters. .` |
|         - | 5941 | ` * Note that NULL is returned on failure.` |
|         - | 5942 | ` */` |
|        26 | 5943 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5944 | `{` |
|         - | 5945 | `	ph7_hashmap_node *pEntry;` |
|         - | 5946 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5947 | `	ph7_value *pCallback;` |
|         - | 5948 | `	ph7_value *pArray;` |
|         - | 5949 | `	ph7_value *pVal;` |
|         - | 5950 | `	sxi32 rc;` |
|         - | 5951 | `	sxu32 n;` |
|         - | 5952 | `	int i;` |
|         - | 5953 |  |
|         - | 5954 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        31 | 5955 | `	if( nArg < 2 ){` |
|         4 | 5956 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5957 | `			"ArgumentCountError",` |
|         - | 5958 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|         1 | 5959 | `			nArg` |
|         - | 5960 | `			);` |
|         - | 5961 | `	}` |
|        29 | 5962 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5963 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5964 | `			"TypeError",` |
|         - | 5965 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5966 | `			ph7_type_name(apArg[0])` |
|         - | 5967 | `			);` |
|         - | 5968 | `	}` |
|         - | 5969 |  |
|        27 | 5970 | `	if( nArg == 2 ){` |
|         - | 5971 | `		/* Only the original array and the callback were provided. */` |
|         - | 5972 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 5973 | `		 * validation ordering. */` |
|         3 | 5974 | `	} else {` |
|         - | 5975 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 5976 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 5977 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5978 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5979 | `					"TypeError",` |
|         - | 5980 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5981 | `					i + 1,` |
|         2 | 5982 | `					ph7_type_name(apArg[i])` |
|         - | 5983 | `					);` |
|         - | 5984 | `			}` |
|        13 | 5985 | `		}` |
|         - | 5986 | `	}` |
|         - | 5987 |  |
|         - | 5988 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 5989 | `	pCallback = apArg[nArg - 1];` |
|         - | 5990 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 5991 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 5992 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5993 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 5994 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 5995 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 5996 | `			 */` |
|         9 | 5997 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 5998 | `			if( pCb->nEntry != 2 ){` |
|         4 | 5999 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6000 | `					"TypeError",` |
|         - | 6001 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6002 | `					nArg` |
|         - | 6003 | `					);` |
|         - | 6004 | `			}` |
|         - | 6005 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6006 | `			{` |
|         6 | 6007 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6008 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6009 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6010 | `					int nMethodLen;` |
|         6 | 6011 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6012 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6013 | `					if( pClass ){` |
|         - | 6014 | `						/* Class exists but method is missing. */` |
|         4 | 6015 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6016 | `							"TypeError",` |
|         - | 6017 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6018 | `							nArg,` |
|         1 | 6019 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6020 | `							zMethod` |
|         - | 6021 | `							);` |
|         - | 6022 | `					}` |
|         - | 6023 | `					/* Class not found */` |
|         - | 6024 | `					{` |
|         - | 6025 | `						int nName;` |
|         3 | 6026 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6027 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6028 | `							"TypeError",` |
|         - | 6029 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6030 | `							nArg,` |
|         1 | 6031 | `							zName` |
|         - | 6032 | `							);` |
|         - | 6033 | `					}` |
|         - | 6034 | `				}` |
|         - | 6035 | `			}` |
|         - | 6036 | `			/* Fallback message */` |
|       ! 0 | 6037 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6038 | `				"TypeError",` |
|         - | 6039 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6040 | `				nArg` |
|         - | 6041 | `				);` |
|         - | 6042 | `		}` |
|         6 | 6043 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6044 | `			int len;` |
|         3 | 6045 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6046 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6047 | `				"TypeError",` |
|         - | 6048 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6049 | `				nArg,` |
|         1 | 6050 | `				zName` |
|         - | 6051 | `				);` |
|         - | 6052 | `		}` |
|         4 | 6053 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6054 | `			"TypeError",` |
|         - | 6055 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6056 | `			nArg` |
|         - | 6057 | `			);` |
|         - | 6058 | `	}` |
|         - | 6059 |  |
|        11 | 6060 | `	if( nArg == 2 ){` |
|         - | 6061 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6062 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6063 | `		return PH7_OK;` |
|         - | 6064 | `	}` |
|         - | 6065 |  |
|         - | 6066 | `	/* Create a new array */` |
|         7 | 6067 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6068 | `	if( pArray == 0 ){` |
|       ! 0 | 6069 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6070 | `		return PH7_OK;` |
|         - | 6071 | `	}` |
|         - | 6072 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6073 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6074 | `	/* Perform the intersection */` |
|         7 | 6075 | `	pEntry = pSrc->pFirst;` |
|         7 | 6076 | `	n = pSrc->nEntry;` |
|         7 | 6077 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6078 | `	for(;;){` |
|        19 | 6079 | `		if( n < 1 ){` |
|         5 | 6080 | `			break;` |
|         - | 6081 | `		}` |
|         - | 6082 | `		/* Extract the node value */` |
|        15 | 6083 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6084 | `		if( pVal ){` |
|        23 | 6085 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6086 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6087 | `					/* ignore */` |
|       ! 0 | 6088 | `					continue;` |
|         - | 6089 | `				}` |
|         - | 6090 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6091 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6092 | `				/* Perform the lookup */` |
|        15 | 6093 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6094 | `				if( rc != SXRET_OK ){` |
|         - | 6095 | `					/* Value does not exist */` |
|         7 | 6096 | `					break;` |
|         - | 6097 | `				}` |
|         5 | 6098 | `			}` |
|        15 | 6099 | `			if( i >= (nArg-1) ){` |
|         - | 6100 | `				/* Perform the insertion */` |
|         9 | 6101 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6102 | `			}` |
|         7 | 6103 | `		}` |
|        15 | 6104 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6105 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6106 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6107 | `			return PH7_EXCEPTION;` |
|         - | 6108 | `		}` |
|         - | 6109 | `		/* Point to the next entry */` |
|        13 | 6110 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6111 | `		n--;` |
|         1 | 6112 | `	}` |
|         - | 6113 | `	/* Return the freshly created array */` |
|         5 | 6114 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6115 | `	return PH7_OK;` |
|        18 | 6116 | `}` |
|         - | 6117 | `/*` |
|         - | 6118 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6119 | ` *  Fill an array with values.` |
|         - | 6120 | ` * Parameters` |
|         - | 6121 | ` *  $start_index` |
|         - | 6122 | ` *    The first index of the returned array.` |
|         - | 6123 | ` *  $num` |
|         - | 6124 | ` *   Number of elements to insert.` |
|         - | 6125 | ` *  $value` |
|         - | 6126 | ` *    Value to use for filling.` |
|         - | 6127 | ` * Return` |
|         - | 6128 | ` *  The filled array or null on failure.` |
|         - | 6129 | ` */` |
|       244 | 6130 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6131 | `{` |
|         - | 6132 | `	ph7_value *pArray;` |
|         - | 6133 | `	int i,nEntry;` |
|         - | 6134 |  |
|         - | 6135 | `	/* PHP enforces argument count and type checks. */` |
|       249 | 6136 | `	if( nArg != 3 ){` |
|         - | 6137 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         8 | 6138 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6139 | `			"ArgumentCountError",` |
|         - | 6140 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         2 | 6141 | `			nArg` |
|         - | 6142 | `			);` |
|         - | 6143 | `	}` |
|         - | 6144 |  |
|         - | 6145 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6146 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6147 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6148 | `	 * and NULLs are rejected outright. */` |
|       359 | 6149 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       362 | 6150 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|         4 | 6151 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6152 | `			"TypeError",` |
|         - | 6153 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|         1 | 6154 | `			ph7_type_name(apArg[0])` |
|         - | 6155 | `			);` |
|         - | 6156 | `	}` |
|       242 | 6157 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6158 | `		int len;` |
|         8 | 6159 | `		sxu8 bReal = FALSE;` |
|         8 | 6160 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6161 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6162 | `			/* Non‑numeric string is an error. */` |
|         3 | 6163 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6164 | `				"TypeError",` |
|         - | 6165 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6166 | `				);` |
|         - | 6167 | `		}` |
|         5 | 6168 | `		if( bReal ){` |
|         - | 6169 | `			/* float-string -> deprecation warning */` |
|         4 | 6170 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6171 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6172 | `				zStr` |
|         - | 6173 | `				);` |
|         1 | 6174 | `		}` |
|         2 | 6175 | `	}` |
|         - | 6176 |  |
|         - | 6177 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6178 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6179 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6180 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6181 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6182 | `			"TypeError",` |
|         - | 6183 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6184 | `			ph7_type_name(apArg[1])` |
|         - | 6185 | `			);` |
|         - | 6186 | `	}` |
|       239 | 6187 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6188 | `		int len;` |
|         3 | 6189 | `		sxu8 bReal = FALSE;` |
|         3 | 6190 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6191 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6192 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6193 | `				"TypeError",` |
|         - | 6194 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6195 | `				);` |
|         - | 6196 | `		}` |
|       ! 0 | 6197 | `	}` |
|         - | 6198 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6199 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6200 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6201 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6202 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6203 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6204 | `		if( d != (double)i64 ){` |
|         7 | 6205 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6206 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6207 | `				d` |
|         - | 6208 | `				);` |
|         2 | 6209 | `		}` |
|         2 | 6210 | `	}` |
|         - | 6211 |  |
|         - | 6212 | `	/* Total number of entries to insert */` |
|       236 | 6213 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6214 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6215 | `	if( nEntry < 0 ){` |
|         3 | 6216 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6217 | `			"ValueError",` |
|         - | 6218 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6219 | `			);` |
|         - | 6220 | `	}` |
|         - | 6221 |  |
|         - | 6222 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6223 | `	if( nEntry == 0 ){` |
|         7 | 6224 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6225 | `		return PH7_OK;` |
|         - | 6226 | `	}` |
|         - | 6227 |  |
|         - | 6228 | `	/* Create a new array */` |
|       227 | 6229 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6230 | `	if( pArray == 0 ){` |
|       ! 0 | 6231 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6232 | `	}` |
|         - | 6233 |  |
|         - | 6234 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6235 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6236 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6237 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6238 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6239 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6240 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6241 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6242 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6243 | `		}` |
|   1058803 | 6244 | `	}` |
|         - | 6245 | `	/* Return the filled array */` |
|       227 | 6246 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6247 | `	return PH7_OK;` |
|       127 | 6248 | `}` |
|         - | 6249 | `/*` |
|         - | 6250 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6251 | ` *  Fill an array with values, specifying keys.` |
|         - | 6252 | ` * Parameters` |
|         - | 6253 | ` *  $input` |
|         - | 6254 | ` *   Array of values that will be used as key.` |
|         - | 6255 | ` *  $value` |
|         - | 6256 | ` *    Value to use for filling.` |
|         - | 6257 | ` * Return` |
|         - | 6258 | ` *  The filled array.` |
|         - | 6259 | ` * Throws` |
|         - | 6260 | ` *  ValueError if $input is not an array.` |
|         - | 6261 | ` */` |
|        26 | 6262 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6263 | `{` |
|         - | 6264 | `	ph7_hashmap_node *pEntry;` |
|         - | 6265 | `	ph7_hashmap *pSrc;` |
|         - | 6266 | `	ph7_value *pArray;` |
|         - | 6267 | `	sxu32 n;` |
|         - | 6268 | `	/* PHP enforces exactly 2 arguments. */` |
|        31 | 6269 | `	if( nArg != 2 ){` |
|        12 | 6270 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6271 | `			"ArgumentCountError",` |
|         - | 6272 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         3 | 6273 | `			nArg` |
|         - | 6274 | `			);` |
|         - | 6275 | `	}` |
|         - | 6276 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6277 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6278 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6279 | `			"TypeError",` |
|         - | 6280 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6281 | `			ph7_type_name(apArg[0])` |
|         - | 6282 | `			);` |
|         - | 6283 | `	}` |
|         - | 6284 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6285 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6286 | `	/* Create a new array */` |
|        17 | 6287 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6288 | `	if( pArray == 0 ){` |
|       ! 0 | 6289 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6290 | `		return PH7_OK;` |
|         - | 6291 | `	}` |
|         - | 6292 | `	/* Perform the requested operation */` |
|        17 | 6293 | `	pEntry = pSrc->pFirst;` |
|        45 | 6294 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6295 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6296 | `		/* Point to the next entry */` |
|        29 | 6297 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6298 | `	}` |
|         - | 6299 | `	/* Return the filled array */` |
|        17 | 6300 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6301 | `	return PH7_OK;` |
|        18 | 6302 | `}` |
|         - | 6303 | `/*` |
|         - | 6304 | ` * array array_combine(array $keys,array $values)` |
|         - | 6305 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6306 | ` * Parameters` |
|         - | 6307 | ` *  $keys` |
|         - | 6308 | ` *    Array of keys to be used.` |
|         - | 6309 | ` * $values` |
|         - | 6310 | ` *   Array of values to be used.` |
|         - | 6311 | ` * Return` |
|         - | 6312 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6313 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6314 | ` *  not an array.` |
|         - | 6315 | ` */` |
|        18 | 6316 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6317 | `{` |
|         - | 6318 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6319 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6320 | `	ph7_value *pArray;` |
|         - | 6321 | `	sxu32 n;` |
|         - | 6322 | `	/* PHP enforces argument count and type checks. */` |
|        23 | 6323 | `	if( nArg != 2 ){` |
|         - | 6324 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6325 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6326 | `			"ArgumentCountError",` |
|         - | 6327 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|         1 | 6328 | `			nArg` |
|         - | 6329 | `			);` |
|         - | 6330 | `	}` |
|         - | 6331 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6332 | `	 * argument index in the error message. */` |
|        20 | 6333 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6334 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6335 | `			"TypeError",` |
|         - | 6336 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6337 | `			ph7_type_name(apArg[0])` |
|         - | 6338 | `			);` |
|         - | 6339 | `	}` |
|        17 | 6340 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6341 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6342 | `			"TypeError",` |
|         - | 6343 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6344 | `			ph7_type_name(apArg[1])` |
|         - | 6345 | `			);` |
|         - | 6346 | `	}` |
|         - | 6347 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6348 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6349 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6350 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6351 | `		/* Length mismatch -> ValueError */` |
|         3 | 6352 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6353 | `			"ValueError",` |
|         - | 6354 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6355 | `			);` |
|         - | 6356 | `	}` |
|         - | 6357 | `	/* Create a new array */` |
|        11 | 6358 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6359 | `	if( pArray == 0 ){` |
|       ! 0 | 6360 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6361 | `		return PH7_OK;` |
|         - | 6362 | `	}` |
|         - | 6363 | `	/* Perform the requested operation */` |
|        11 | 6364 | `	pKe = pKey->pFirst;` |
|        11 | 6365 | `	pVe = pValue->pFirst;` |
|        33 | 6366 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6367 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6368 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6369 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6370 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6371 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6372 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6373 | `		 * original array must not be mutated. */` |
|        23 | 6374 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6375 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6376 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6377 | `			if( pTmpKey ){` |
|         5 | 6378 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6379 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6380 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6381 | `				pKeyCopy = pTmpKey;` |
|         2 | 6382 | `			}` |
|         2 | 6383 | `		}` |
|        23 | 6384 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6385 | `		/* Point to the next entry */` |
|        23 | 6386 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6387 | `		pVe = pVe->pPrev;` |
|        12 | 6388 | `	}` |
|         - | 6389 | `	/* Return the filled array */` |
|        11 | 6390 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6391 | `	return PH7_OK;` |
|        14 | 6392 | `}` |
|         - | 6393 | `/*` |
|         - | 6394 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6395 | ` *  Return an array with elements in reverse order.` |
|         - | 6396 | ` * Parameters` |
|         - | 6397 | ` *  $array` |
|         - | 6398 | ` *   The input array.` |
|         - | 6399 | ` *  $preserve_keys (optional)` |
|         - | 6400 | ` *   If set to TRUE keys are preserved.` |
|         - | 6401 | ` * Return` |
|         - | 6402 | ` *  The reversed array.` |
|         - | 6403 | ` */` |
|        20 | 6404 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6405 | `{` |
|         - | 6406 | `	ph7_hashmap_node *pEntry;` |
|         - | 6407 | `	ph7_hashmap *pSrc;` |
|         - | 6408 | `	ph7_value *pArray;` |
|         - | 6409 | `	int bPreserve;` |
|         - | 6410 | `	sxu32 n;` |
|        23 | 6411 | `	if( nArg < 1 ){` |
|         4 | 6412 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6413 | `			"ArgumentCountError",` |
|         - | 6414 | `			"array_reverse() expects at least 1 argument, %d given",` |
|         1 | 6415 | `			nArg` |
|         - | 6416 | `			);` |
|         - | 6417 | `	}` |
|         - | 6418 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6419 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6420 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6421 | `			"TypeError",` |
|         - | 6422 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6423 | `			ph7_type_name(apArg[0])` |
|         - | 6424 | `			);` |
|         - | 6425 | `	}` |
|        17 | 6426 | `	bPreserve = FALSE;` |
|        17 | 6427 | `	if( nArg > 1 ){` |
|         7 | 6428 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6429 | `	}` |
|         - | 6430 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6431 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6432 | `	/* Create a new array */` |
|        17 | 6433 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6434 | `	if( pArray == 0 ){` |
|       ! 0 | 6435 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6436 | `		return PH7_OK;` |
|         - | 6437 | `	}` |
|         - | 6438 | `	/* Perform the requested operation */` |
|        17 | 6439 | `	pEntry = pSrc->pLast;` |
|        55 | 6440 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6441 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6442 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6443 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6444 | `		/* Point to the previous entry */` |
|        39 | 6445 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6446 | `	}` |
|        17 | 6447 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6448 | `	return PH7_OK;` |
|        13 | 6449 | `}` |
|         - | 6450 | `/*` |
|         - | 6451 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6452 | ` *  Removes duplicate values from an array.` |
|         - | 6453 | ` * Parameters` |
|         - | 6454 | ` *  $array` |
|         - | 6455 | ` *   The input array.` |
|         - | 6456 | ` *  $flags` |
|         - | 6457 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6458 | ` *   behavior using these values:` |
|         - | 6459 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6460 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6461 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6462 | ` * Return` |
|         - | 6463 | ` *  The filtered array.` |
|         - | 6464 | ` */` |
|        24 | 6465 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6466 | `{` |
|         - | 6467 | `	ph7_hashmap_node *pEntry;` |
|         - | 6468 | `	ph7_value *pNeedle;` |
|         - | 6469 | `	ph7_hashmap *pSrc;` |
|         - | 6470 | `	ph7_value *pArray;` |
|         - | 6471 | `	int bStrict;` |
|         - | 6472 | `	sxi32 rc;` |
|         - | 6473 | `	sxu32 n;` |
|        28 | 6474 | `	if( nArg < 1 ){` |
|         - | 6475 | `		/* Missing arguments, throw ArgumentCountError */` |
|         3 | 6476 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6477 | `			"ArgumentCountError",` |
|         - | 6478 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6479 | `			);` |
|         - | 6480 | `	}` |
|        25 | 6481 | `	if( nArg > 2 ){` |
|         - | 6482 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6483 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6484 | `			"ArgumentCountError",` |
|         - | 6485 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6486 | `			nArg` |
|         - | 6487 | `			);` |
|         - | 6488 | `	}` |
|         - | 6489 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6490 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6491 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6492 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6493 | `			"TypeError",` |
|         - | 6494 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6495 | `			ph7_type_name(apArg[0])` |
|         - | 6496 | `			);` |
|         - | 6497 | `	}` |
|        19 | 6498 | `	bStrict = FALSE;` |
|         - | 6499 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6500 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6501 | `	/* Create a new array */` |
|        19 | 6502 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6503 | `	if( pArray == 0 ){` |
|       ! 0 | 6504 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6505 | `		return PH7_OK;` |
|         - | 6506 | `	}` |
|         - | 6507 | `	/* Perform the requested operation */` |
|        19 | 6508 | `	pEntry = pSrc->pFirst;` |
|        83 | 6509 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6510 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6511 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6512 | `		if( pNeedle ){` |
|        65 | 6513 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6514 | `		}` |
|        65 | 6515 | `		if( rc != SXRET_OK ){` |
|         - | 6516 | `			/* Perform the insertion */` |
|        37 | 6517 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6518 | `		}` |
|         - | 6519 | `		/* Point to the next entry */` |
|        65 | 6520 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6521 | `	}` |
|         - | 6522 | `	/* Return the freshly created array */` |
|        19 | 6523 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6524 | `	return PH7_OK;` |
|        16 | 6525 | `}` |
|         - | 6526 | `/*` |
|         - | 6527 | ` * array array_flip(array $input)` |
|         - | 6528 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6529 | ` * Parameter` |
|         - | 6530 | ` *  $input` |
|         - | 6531 | ` *   Input array.` |
|         - | 6532 | ` * Return` |
|         - | 6533 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6534 | ` */` |
|        34 | 6535 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6536 | `{` |
|         - | 6537 | `	ph7_hashmap_node *pEntry;` |
|         - | 6538 | `	ph7_hashmap *pSrc;` |
|         - | 6539 | `	ph7_value *pArray;` |
|         - | 6540 | `	ph7_value *pKey;` |
|         - | 6541 | `	ph7_value sVal;` |
|         - | 6542 | `	sxu32 n;` |
|         - | 6543 |  |
|         - | 6544 | `	/* PHP requires exactly one argument */` |
|        39 | 6545 | `	if( nArg != 1 ){` |
|         - | 6546 | `		/* Use ArgumentCountError like other array helpers */` |
|         8 | 6547 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6548 | `			"ArgumentCountError",` |
|         - | 6549 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         2 | 6550 | `			nArg` |
|         - | 6551 | `			);` |
|         - | 6552 | `	}` |
|         - | 6553 | `	/* Make sure we are dealing with a valid hashmap */` |
|        33 | 6554 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6555 | `		/* Type mismatch -> TypeError */` |
|         8 | 6556 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6557 | `			"TypeError",` |
|         - | 6558 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6559 | `			ph7_type_name(apArg[0])` |
|         - | 6560 | `			);` |
|         - | 6561 | `	}` |
|         - | 6562 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6563 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6564 | `	/* Create a new array */` |
|        27 | 6565 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6566 | `	if( pArray == 0 ){` |
|       ! 0 | 6567 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6568 | `		return PH7_OK;` |
|         - | 6569 | `	}` |
|         - | 6570 | `	/* Start processing */` |
|        27 | 6571 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6572 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6573 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6574 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6575 | `		if( pKey ){` |
|         - | 6576 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6577 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6578 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6579 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6580 | `					);` |
|     22236 | 6581 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6582 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6583 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6584 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6585 | `				}else{` |
|         - | 6586 | `					SyString sStr;` |
|      2227 | 6587 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6588 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6589 | `				}` |
|         - | 6590 | `				/* Perform the insertion */` |
|     22227 | 6591 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6592 | `				/* Safely release the value because each inserted entry` |
|         - | 6593 | `				 * has its own private copy of the value.` |
|         - | 6594 | `				 */` |
|     22227 | 6595 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6596 | `			}else{` |
|         - | 6597 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6598 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6599 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6600 | `					);` |
|         - | 6601 | `			}` |
|     11118 | 6602 | `		}` |
|         - | 6603 | `		/* Point to the next entry */` |
|     22237 | 6604 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6605 | `	}` |
|         - | 6606 | `	/* Return the freshly created array */` |
|        27 | 6607 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6608 | `	return PH7_OK;` |
|        22 | 6609 | `}` |
|         - | 6610 | `/*` |
|         - | 6611 | ` * number array_sum(array $array )` |
|         - | 6612 | ` *  Calculate the sum of values in an array.` |
|         - | 6613 | ` * Parameters` |
|         - | 6614 | ` *  $array: The input array.` |
|         - | 6615 | ` * Return` |
|         - | 6616 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6617 | ` */` |
|        24 | 6618 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6619 | `{` |
|         - | 6620 | `	ph7_hashmap_node *pEntry;` |
|         - | 6621 | `	ph7_value *pObj;` |
|        25 | 6622 | `	double dSum = 0;` |
|         - | 6623 | `	sxu32 n;` |
|        25 | 6624 | `	pEntry = pMap->pFirst;` |
|        91 | 6625 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        67 | 6626 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        67 | 6627 | `		if( pObj ){` |
|        67 | 6628 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        29 | 6629 | `				dSum += pObj->rVal;` |
|        53 | 6630 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6631 | `				dSum += (double)pObj->x.iVal;` |
|        29 | 6632 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        15 | 6633 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6634 | `					double dv = 0;` |
|        13 | 6635 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6636 | `					dSum += dv;` |
|         7 | 6637 | `				}` |
|        12 | 6638 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6639 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6640 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6641 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6642 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6643 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6644 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6645 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6646 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6647 | `			}` |
|         - | 6648 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6649 | `		}` |
|         - | 6650 | `		/* Point to the next entry */` |
|        67 | 6651 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        34 | 6652 | `	}` |
|         - | 6653 | `	/* Return sum */` |
|        25 | 6654 | `	ph7_result_double(pCtx,dSum);` |
|        25 | 6655 | `}` |
|       680 | 6656 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6657 | `{` |
|         - | 6658 | `	ph7_hashmap_node *pEntry;` |
|         - | 6659 | `	ph7_value *pObj;` |
|       682 | 6660 | `	sxi64 nSum = 0;` |
|         - | 6661 | `	sxu32 n;` |
|       682 | 6662 | `	pEntry = pMap->pFirst;` |
|      4672 | 6663 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      3992 | 6664 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      3992 | 6665 | `		if( pObj ){` |
|      3992 | 6666 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3982 | 6667 | `				nSum += pObj->x.iVal;` |
|      2001 | 6668 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         7 | 6669 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         5 | 6670 | `					sxi64 nv = 0;` |
|         5 | 6671 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         5 | 6672 | `					nSum += nv;` |
|         3 | 6673 | `				}` |
|         8 | 6674 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6675 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6676 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6677 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6678 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6679 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6680 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6681 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6682 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6683 | `			}` |
|         - | 6684 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      1995 | 6685 | `		}` |
|         - | 6686 | `		/* Point to the next entry */` |
|      3992 | 6687 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      1997 | 6688 | `	}` |
|         - | 6689 | `	/* Return sum */` |
|       682 | 6690 | `	ph7_result_int64(pCtx,nSum);` |
|       682 | 6691 | `}` |
|         - | 6692 | `/* number array_sum(array $array )` |
|         - | 6693 | ` * (See block-coment above)` |
|         - | 6694 | ` */` |
|       718 | 6695 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6696 | `{` |
|         - | 6697 | `	ph7_hashmap_node *pEntry;` |
|         - | 6698 | `	ph7_hashmap *pMap;` |
|         - | 6699 | `	ph7_value *pObj;` |
|       723 | 6700 | `	int useDouble = 0;` |
|         - | 6701 | `	sxu32 n;` |
|         - | 6702 | `	/* PHP requires exactly one argument */` |
|       723 | 6703 | `	if( nArg != 1 ){` |
|         8 | 6704 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6705 | `			"ArgumentCountError",` |
|         - | 6706 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         2 | 6707 | `			nArg` |
|         - | 6708 | `			);` |
|         - | 6709 | `	}` |
|         - | 6710 | `	/* Make sure we are dealing with a valid hashmap */` |
|       717 | 6711 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6712 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6713 | `		char zBuf[64];` |
|         8 | 6714 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6715 | `			"TypeError",` |
|         - | 6716 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6717 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6718 | `			);` |
|         - | 6719 | `	}` |
|       712 | 6720 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       712 | 6721 | `	if( pMap->nEntry < 1 ){` |
|         - | 6722 | `		/* Nothing to compute,return 0 */` |
|         7 | 6723 | `		ph7_result_int(pCtx,0);` |
|         7 | 6724 | `		return PH7_OK;` |
|         - | 6725 | `	}` |
|         - | 6726 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6727 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6728 | `	 */` |
|       706 | 6729 | `	pEntry = pMap->pFirst;` |
|      4704 | 6730 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4024 | 6731 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4024 | 6732 | `		if( pObj ){` |
|      4024 | 6733 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        19 | 6734 | `				useDouble = 1;` |
|        19 | 6735 | `				break;` |
|         - | 6736 | `			}` |
|      4006 | 6737 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        13 | 6738 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        13 | 6739 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6740 | `				sxu32 i;` |
|        23 | 6741 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        17 | 6742 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6743 | `						useDouble = 1;` |
|         7 | 6744 | `						break;` |
|         - | 6745 | `					}` |
|         6 | 6746 | `				}` |
|        13 | 6747 | `				if( useDouble ){` |
|         7 | 6748 | `					break;` |
|         - | 6749 | `				}` |
|         3 | 6750 | `			}` |
|      1999 | 6751 | `		}` |
|      4000 | 6752 | `		pEntry = pEntry->pPrev;` |
|      2001 | 6753 | `	}` |
|       706 | 6754 | `	if( useDouble ){` |
|        25 | 6755 | `		DoubleSum(pCtx,pMap);` |
|        13 | 6756 | `	}else{` |
|       682 | 6757 | `		Int64Sum(pCtx,pMap);` |
|         - | 6758 | `	}` |
|       706 | 6759 | `	return PH7_OK;` |
|       364 | 6760 | `}` |
|         - | 6761 | `/*` |
|         - | 6762 | ` * number array_product(array $array )` |
|         - | 6763 | ` *  Calculate the product of values in an array.` |
|         - | 6764 | ` * Parameters` |
|         - | 6765 | ` *  $array: The input array.` |
|         - | 6766 | ` * Return` |
|         - | 6767 | ` *  Returns the product of values as an integer or float.` |
|         - | 6768 | ` */` |
|         2 | 6769 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6770 | `{` |
|         - | 6771 | `	ph7_hashmap_node *pEntry;` |
|         - | 6772 | `	ph7_value *pObj;` |
|         - | 6773 | `	double dProd;` |
|         - | 6774 | `	sxu32 n;` |
|         3 | 6775 | `	pEntry = pMap->pFirst;` |
|         3 | 6776 | `	dProd = 1;` |
|         7 | 6777 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6778 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6779 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6780 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6781 | `				dProd *= pObj->rVal;` |
|         4 | 6782 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6783 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6784 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6785 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6786 | `					double dv = 0;` |
|       ! 0 | 6787 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6788 | `					dProd *= dv;` |
|       ! 0 | 6789 | `				}` |
|       ! 0 | 6790 | `			}` |
|         2 | 6791 | `		}` |
|         - | 6792 | `		/* Point to the next entry */` |
|         5 | 6793 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6794 | `	}` |
|         - | 6795 | `	/* Return product */` |
|         3 | 6796 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6797 | `}` |
|         2 | 6798 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6799 | `{` |
|         - | 6800 | `	ph7_hashmap_node *pEntry;` |
|         - | 6801 | `	ph7_value *pObj;` |
|         - | 6802 | `	sxi64 nProd;` |
|         - | 6803 | `	sxu32 n;` |
|         3 | 6804 | `	pEntry = pMap->pFirst;` |
|         3 | 6805 | `	nProd = 1;` |
|         9 | 6806 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6807 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6808 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6809 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6810 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6811 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6812 | `				nProd *= pObj->x.iVal;` |
|         3 | 6813 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6814 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6815 | `					sxi64 nv = 0;` |
|       ! 0 | 6816 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6817 | `					nProd *= nv;` |
|       ! 0 | 6818 | `				}` |
|       ! 0 | 6819 | `			}` |
|         3 | 6820 | `		}` |
|         - | 6821 | `		/* Point to the next entry */` |
|         7 | 6822 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6823 | `	}` |
|         - | 6824 | `	/* Return product */` |
|         3 | 6825 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6826 | `}` |
|         - | 6827 | `/* number array_product(array $array )` |
|         - | 6828 | ` * (See block-block comment above)` |
|         - | 6829 | ` */` |
|        18 | 6830 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6831 | `{` |
|         - | 6832 | `	ph7_hashmap *pMap;` |
|         - | 6833 | `	ph7_value *pObj;` |
|        19 | 6834 | `	if( nArg < 1 ){` |
|         - | 6835 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6836 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6837 | `		return PH7_OK;` |
|         - | 6838 | `	}` |
|         - | 6839 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        19 | 6840 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6841 | `		char zBuf[64];` |
|        19 | 6842 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6843 | `			"TypeError",` |
|         - | 6844 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 6845 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6846 | `			);` |
|         - | 6847 | `	}` |
|         7 | 6848 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6849 | `	if( pMap->nEntry < 1 ){` |
|         - | 6850 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6851 | `		ph7_result_int(pCtx,1);` |
|         3 | 6852 | `		return PH7_OK;` |
|         - | 6853 | `	}` |
|         - | 6854 | `	/* If the first element is of type float,then perform floating` |
|         - | 6855 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6856 | `	 */` |
|         5 | 6857 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6858 | `	if( pObj == 0 ){` |
|       ! 0 | 6859 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6860 | `		return PH7_OK;` |
|         - | 6861 | `	}` |
|         5 | 6862 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6863 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6864 | `	}else{` |
|         3 | 6865 | `		Int64Prod(pCtx,pMap);` |
|         - | 6866 | `	}` |
|         5 | 6867 | `	return PH7_OK;` |
|        10 | 6868 | `}` |
|         - | 6869 | `/*` |
|         - | 6870 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6871 | ` *  Pick one or more random entries out of an array.` |
|         - | 6872 | ` * Parameters` |
|         - | 6873 | ` * $input` |
|         - | 6874 | ` *  The input array.` |
|         - | 6875 | ` * $num_req` |
|         - | 6876 | ` *  Specifies how many entries you want to pick.` |
|         - | 6877 | ` * Return` |
|         - | 6878 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6879 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6880 | ` *  NULL is returned on failure.` |
|         - | 6881 | ` */` |
|        42 | 6882 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6883 | `{` |
|         - | 6884 | `	ph7_hashmap_node *pNode;` |
|         - | 6885 | `	ph7_hashmap *pMap;` |
|        43 | 6886 | `	int nItem = 1;` |
|        43 | 6887 | `	if( nArg < 1 ){` |
|         - | 6888 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6889 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6890 | `		return PH7_OK;` |
|         - | 6891 | `	}` |
|         - | 6892 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        43 | 6893 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6894 | `		char zBuf[64];` |
|        10 | 6895 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6896 | `			"TypeError",` |
|         - | 6897 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6898 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6899 | `			);` |
|         - | 6900 | `	}` |
|         - | 6901 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6902 | `	 * check, matching its ZPP-before-body ordering. */` |
|        37 | 6903 | `	if( nArg > 1 ){` |
|        29 | 6904 | `		ph7_value *pNum = apArg[1];` |
|        28 | 6905 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        24 | 6906 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6907 | `			char zBuf[64];` |
|        10 | 6908 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6909 | `				"TypeError",` |
|         - | 6910 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|         3 | 6911 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6912 | `				);` |
|         - | 6913 | `		}` |
|        23 | 6914 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6915 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6916 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6917 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6918 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6919 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6920 | `			int len;` |
|         9 | 6921 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6922 | `			sxi64 iLong; double dReal;` |
|         9 | 6923 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6924 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6925 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6926 | `					"TypeError",` |
|         - | 6927 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6928 | `					);` |
|         - | 6929 | `			}` |
|         - | 6930 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6931 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6932 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6933 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6934 | `			}` |
|         3 | 6935 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6936 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6937 | `			nItem = (int)iLong;` |
|         2 | 6938 | `		}else{` |
|        15 | 6939 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6940 | `		}` |
|         8 | 6941 | `	}` |
|         - | 6942 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6943 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6944 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6945 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6946 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6947 | `			"ValueError",` |
|         - | 6948 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 6949 | `			);` |
|         - | 6950 | `	}` |
|         - | 6951 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 6952 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 6953 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6954 | `			"ValueError",` |
|         - | 6955 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 6956 | `			);` |
|         - | 6957 | `	}` |
|        13 | 6958 | `	if( nItem < 2 ){` |
|         - | 6959 | `		sxu32 nEntry;` |
|         - | 6960 | `		/* Select a random number */` |
|         9 | 6961 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 6962 | `		/* Extract the desired entry.` |
|         - | 6963 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 6964 | `		 */` |
|         9 | 6965 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         2 | 6966 | `			pNode = pMap->pLast;` |
|         2 | 6967 | `			nEntry = pMap->nEntry - nEntry;` |
|         2 | 6968 | `			if( nEntry > 1 ){` |
|       ! 0 | 6969 | `				for(;;){` |
|       ! 0 | 6970 | `					if( nEntry == 0 ){` |
|       ! 0 | 6971 | `						break;` |
|         - | 6972 | `					}` |
|         - | 6973 | `					/* Point to the previous entry */` |
|       ! 0 | 6974 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 6975 | `					nEntry--;` |
|       ! 0 | 6976 | `				}` |
|       ! 0 | 6977 | `			}` |
|         1 | 6978 | `		}else{` |
|         8 | 6979 | `			pNode = pMap->pFirst;` |
|         7 | 6980 | `			for(;;){` |
|        11 | 6981 | `				if( nEntry == 0 ){` |
|         8 | 6982 | `					break;` |
|         - | 6983 | `				}` |
|         - | 6984 | `				/* Point to the next entry */` |
|         4 | 6985 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         4 | 6986 | `				nEntry--;` |
|         1 | 6987 | `			}` |
|         - | 6988 | `		}` |
|         9 | 6989 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 6990 | `			/* Int key */` |
|         7 | 6991 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 6992 | `		}else{` |
|         - | 6993 | `			/* Blob key */` |
|         3 | 6994 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 6995 | `		}` |
|         5 | 6996 | `	}else{` |
|         - | 6997 | `		ph7_value sKey,*pArray;` |
|         - | 6998 | `		ph7_hashmap *pDest;` |
|         - | 6999 | `		/* Create a new array */` |
|         5 | 7000 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7001 | `		if( pArray == 0 ){` |
|       ! 0 | 7002 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7003 | `			return PH7_OK;` |
|         - | 7004 | `		}` |
|         - | 7005 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7006 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7007 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7008 | `		/* Copy the first n items */` |
|         5 | 7009 | `		pNode = pMap->pFirst;` |
|         5 | 7010 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7011 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7012 | `		}` |
|        15 | 7013 | `		while( nItem > 0){` |
|        11 | 7014 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7015 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7016 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7017 | `			/* Point to the next entry */` |
|        11 | 7018 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7019 | `			nItem--;` |
|         1 | 7020 | `		}` |
|         - | 7021 | `		/* Shuffle the array */` |
|         5 | 7022 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7023 | `		/* Rehash node */` |
|         5 | 7024 | `		HashmapSortRehash(pDest);` |
|         - | 7025 | `		/* Return the random array */` |
|         5 | 7026 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7027 | `	}` |
|        13 | 7028 | `	return PH7_OK;` |
|        22 | 7029 | `}` |
|         - | 7030 | `/*` |
|         - | 7031 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7032 | ` *  Split an array into chunks.` |
|         - | 7033 | ` * Parameters` |
|         - | 7034 | ` * $input` |
|         - | 7035 | ` *   The array to work on` |
|         - | 7036 | ` * $size` |
|         - | 7037 | ` *   The size of each chunk` |
|         - | 7038 | ` * $preserve_keys` |
|         - | 7039 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7040 | ` *   the chunk numerically.` |
|         - | 7041 | ` * Return` |
|         - | 7042 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7043 | ` *  zero, with each dimension containing size elements.` |
|         - | 7044 | ` */` |
|        42 | 7045 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7046 | `{` |
|         - | 7047 | `	ph7_value *pArray,*pChunk;` |
|         - | 7048 | `	ph7_hashmap_node *pEntry;` |
|         - | 7049 | `	ph7_hashmap *pMap;` |
|         - | 7050 | `	int bPreserve;` |
|         - | 7051 | `	sxu32 nChunk;` |
|         - | 7052 | `	sxu32 nSize;` |
|         - | 7053 | `	sxu32 n;` |
|         - | 7054 | `	/* Argument count and types follow PHP semantics. */` |
|        47 | 7055 | `	if( nArg < 2 ){` |
|         - | 7056 | `		/* fewer than required arguments -> ArgumentCountError */` |
|         4 | 7057 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7058 | `			"ArgumentCountError",` |
|         - | 7059 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|         1 | 7060 | `			nArg` |
|         - | 7061 | `			);` |
|         - | 7062 | `	}` |
|        45 | 7063 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7064 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7065 | `			"TypeError",` |
|         - | 7066 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7067 | `			ph7_type_name(apArg[0])` |
|         - | 7068 | `			);` |
|         - | 7069 | `	}` |
|         - | 7070 | `	/* Create a new array */` |
|        43 | 7071 | `	pArray = ph7_context_new_array(pCtx);` |
|        43 | 7072 | `	if( pArray == 0 ){` |
|       ! 0 | 7073 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7074 | `		return PH7_OK;` |
|         - | 7075 | `	}` |
|         - | 7076 | `	/* Point to the internal representation of the input hashmap */` |
|        43 | 7077 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7078 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7079 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        57 | 7080 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        81 | 7081 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        38 | 7082 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7083 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7084 | `			"TypeError",` |
|         - | 7085 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7086 | `			ph7_type_name(apArg[1])` |
|         - | 7087 | `			);` |
|         - | 7088 | `	}` |
|         - | 7089 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7090 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7091 | `	 * precision and PHP emits a deprecation warning. */` |
|        43 | 7092 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7093 | `		int len;` |
|         3 | 7094 | `		sxu8 bReal = FALSE;` |
|         3 | 7095 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7096 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7097 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7098 | `				"TypeError",` |
|         - | 7099 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7100 | `				);` |
|         - | 7101 | `		}` |
|       ! 0 | 7102 | `		if( bReal ){` |
|         - | 7103 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7104 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7105 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7106 | `				zStr` |
|         - | 7107 | `				);` |
|       ! 0 | 7108 | `		}` |
|       ! 0 | 7109 | `	}` |
|         - | 7110 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7111 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7112 | `	 * later via ph7_value_to_int. */` |
|        40 | 7113 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7114 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7115 | `		sxi64 i = (sxi64)d;` |
|         3 | 7116 | `		if( d != (double)i ){` |
|         4 | 7117 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7118 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7119 | `				d` |
|         - | 7120 | `				);` |
|         1 | 7121 | `		}` |
|         1 | 7122 | `	}` |
|         - | 7123 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7124 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7125 | `	{` |
|        40 | 7126 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        40 | 7127 | `		if( nSizeSigned < 1 ){` |
|         - | 7128 | `			/* size <= 0 -> ValueError */` |
|         6 | 7129 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7130 | `				"ValueError",` |
|         - | 7131 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7132 | `				);` |
|         - | 7133 | `		}` |
|        35 | 7134 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7135 | `	}` |
|        35 | 7136 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7137 | `		/* Return the whole array */` |
|         3 | 7138 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7139 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7140 | `		return PH7_OK;` |
|         - | 7141 | `	}` |
|        33 | 7142 | `	bPreserve = 0;` |
|        33 | 7143 | `	if( nArg > 2 ){` |
|         - | 7144 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7145 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7146 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7147 | `		 * normally, matching PHP behaviour. */` |
|        35 | 7148 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        35 | 7149 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7150 | `			ph7_value_is_resource(apArg[2]) ){` |
|         8 | 7151 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7152 | `				"TypeError",` |
|         - | 7153 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|         4 | 7154 | `				ph7_type_name(apArg[2])` |
|         - | 7155 | `				);` |
|         - | 7156 | `		}` |
|        21 | 7157 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7158 | `	}` |
|         - | 7159 | `	/* Start processing */` |
|        27 | 7160 | `	pEntry = pMap->pFirst;` |
|        27 | 7161 | `	nChunk = 0;` |
|        27 | 7162 | `	pChunk = 0;` |
|        27 | 7163 | `	n = pMap->nEntry;` |
|        56 | 7164 | `	for( ;; ){` |
|       113 | 7165 | `		if( n < 1 ){` |
|         - | 7166 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7167 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7168 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7169 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7170 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7171 | `			 * exists. */` |
|        27 | 7172 | `			if( pChunk ){` |
|        27 | 7173 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7174 | `			}` |
|        27 | 7175 | `			break;` |
|         - | 7176 | `		}` |
|        87 | 7177 | `		if( nChunk < 1 ){` |
|        71 | 7178 | `			if( pChunk ){` |
|         - | 7179 | `				/* Put the first chunk */` |
|        45 | 7180 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7181 | `			}` |
|         - | 7182 | `			/* Create a new dimension */` |
|        71 | 7183 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7184 | `												   * will be automatically released as soon we return` |
|         - | 7185 | `												   * from this function */` |
|        71 | 7186 | `			if( pChunk == 0 ){` |
|       ! 0 | 7187 | `				break;` |
|         - | 7188 | `			}` |
|        71 | 7189 | `			nChunk = nSize;` |
|        35 | 7190 | `		}` |
|         - | 7191 | `		/* Insert the entry */` |
|        87 | 7192 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7193 | `		/* Point to the next entry */` |
|        87 | 7194 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7195 | `		nChunk--;` |
|        87 | 7196 | `		n--;` |
|         1 | 7197 | `	}` |
|         - | 7198 | `	/* Return the multidimensional array */` |
|        27 | 7199 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7200 | `	return PH7_OK;` |
|        26 | 7201 | `}` |
|         - | 7202 | `/*` |
|         - | 7203 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7204 | ` *  Pad array to the specified length with a value.` |
|         - | 7205 | ` * $input` |
|         - | 7206 | ` *   Initial array of values to pad.` |
|         - | 7207 | ` * $pad_size` |
|         - | 7208 | ` *   New size of the array.` |
|         - | 7209 | ` * $pad_value` |
|         - | 7210 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7211 | ` */` |
|         - | 7212 | `/*` |
|         - | 7213 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7214 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7215 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7216 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7217 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7218 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7219 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7220 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7221 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7222 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7223 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7224 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7225 | ` */` |
|        50 | 7226 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7227 | `	ph7_context *pCtx,` |
|         - | 7228 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7229 | `	int iArg,              /* 1-based argument position */` |
|         - | 7230 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7231 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7232 | `	)` |
|         1 | 7233 | `{` |
|        51 | 7234 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7235 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7236 | `			"ValueError",` |
|         - | 7237 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7238 | `			zFunc,iArg,zParam` |
|         - | 7239 | `			);` |
|         - | 7240 | `	}` |
|        37 | 7241 | `	return SXRET_OK;` |
|        26 | 7242 | `}` |
|        72 | 7243 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7244 | `{` |
|         - | 7245 | `	ph7_hashmap *pMap;` |
|         - | 7246 | `	ph7_value *pArray;` |
|         - | 7247 | `	sxi64 iLen,iAbs;` |
|         - | 7248 | `	int nEntry;` |
|         - | 7249 | `	sxi32 rc;` |
|        77 | 7250 | `	if( nArg != 3 ){` |
|        12 | 7251 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7252 | `			"ArgumentCountError",` |
|         - | 7253 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         3 | 7254 | `			nArg` |
|         - | 7255 | `			);` |
|         - | 7256 | `	}` |
|        68 | 7257 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7258 | `		char zBuf[64];` |
|        14 | 7259 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7260 | `			"TypeError",` |
|         - | 7261 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 7262 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7263 | `			);` |
|         - | 7264 | `	}` |
|         - | 7265 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7266 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7267 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7268 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        58 | 7269 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        56 | 7270 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7271 | `		char zBuf[64];` |
|         7 | 7272 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7273 | `			"TypeError",` |
|         - | 7274 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|         4 | 7275 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7276 | `			);` |
|         - | 7277 | `	}` |
|        55 | 7278 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7279 | `		int nStr;` |
|        11 | 7280 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7281 | `		sxi64 iLong; double dReal;` |
|        11 | 7282 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7283 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7284 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7285 | `				"TypeError",` |
|         - | 7286 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7287 | `				);` |
|         - | 7288 | `		}` |
|         7 | 7289 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7290 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7291 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7292 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7293 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7294 | `					"TypeError",` |
|         - | 7295 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7296 | `					);` |
|         - | 7297 | `			}` |
|         3 | 7298 | `			iLen = (sxi64)dReal;` |
|         3 | 7299 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7300 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7301 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7302 | `					zStr` |
|         - | 7303 | `					);` |
|       ! 0 | 7304 | `			}` |
|         2 | 7305 | `		}else{` |
|         5 | 7306 | `			iLen = iLong;` |
|         - | 7307 | `		}` |
|         4 | 7308 | `	}else{` |
|        45 | 7309 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7310 | `	}` |
|         - | 7311 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7312 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7313 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7314 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7315 | `	 * overflow). */` |
|        51 | 7316 | `	iAbs = iLen;` |
|        51 | 7317 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7318 | `		iAbs = -iAbs;` |
|         7 | 7319 | `	}` |
|        51 | 7320 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7321 | `	if( rc != SXRET_OK ){` |
|        15 | 7322 | `		return rc;` |
|         - | 7323 | `	}` |
|        37 | 7324 | `	nEntry = (int)iLen;` |
|         - | 7325 | `	/* Create a new array */` |
|        37 | 7326 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7327 | `	if( pArray == 0 ){` |
|       ! 0 | 7328 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7329 | `	}` |
|        37 | 7330 | `	if( nEntry < 0 ){` |
|        11 | 7331 | `		nEntry = -nEntry;` |
|        11 | 7332 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7333 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7334 | `			/* Insert given items first */` |
|        25 | 7335 | `			while( nEntry > 0 ){` |
|        19 | 7336 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7337 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7338 | `				}` |
|        19 | 7339 | `				nEntry--;` |
|         1 | 7340 | `			}` |
|         - | 7341 | `			/* Merge the two arrays */` |
|         7 | 7342 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7343 | `		}else{` |
|         5 | 7344 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7345 | `		}` |
|        32 | 7346 | `	}else if( nEntry > 0 ){` |
|        25 | 7347 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7348 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7349 | `			/* Merge the two arrays first */` |
|        19 | 7350 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7351 | `			/* Insert given items */` |
|       275 | 7352 | `			while( nEntry > 0 ){` |
|       257 | 7353 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7354 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7355 | `				}` |
|       257 | 7356 | `				nEntry--;` |
|         1 | 7357 | `			}` |
|        10 | 7358 | `		}else{` |
|         7 | 7359 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7360 | `		}` |
|        13 | 7361 | `	}else{` |
|         - | 7362 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7363 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7364 | `	}` |
|         - | 7365 | `	/* Return the new array */` |
|        37 | 7366 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7367 | `	return PH7_OK;` |
|        41 | 7368 | `}` |
|         - | 7369 | `/*` |
|         - | 7370 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7371 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7372 | ` * Parameters` |
|         - | 7373 | ` * $array` |
|         - | 7374 | ` *   The array in which elements are replaced.` |
|         - | 7375 | ` * $array1` |
|         - | 7376 | ` *   The array from which elements will be extracted.` |
|         - | 7377 | ` * ....` |
|         - | 7378 | ` *  More arrays from which elements will be extracted.` |
|         - | 7379 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7380 | ` * Return` |
|         - | 7381 | ` *  Returns an array.` |
|         - | 7382 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7383 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7384 | ` */` |
|        22 | 7385 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7386 | `{` |
|         - | 7387 | `	ph7_hashmap *pMap;` |
|         - | 7388 | `	ph7_value *pArray;` |
|         - | 7389 | `	int i;` |
|        26 | 7390 | `	if( nArg < 1 ){` |
|         3 | 7391 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7392 | `			"ArgumentCountError",` |
|         - | 7393 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7394 | `			);` |
|         - | 7395 | `	}` |
|        23 | 7396 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7397 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7398 | `			"TypeError",` |
|         - | 7399 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7400 | `			ph7_type_name(apArg[0])` |
|         - | 7401 | `			);` |
|         - | 7402 | `	}` |
|         - | 7403 | `	/* Create a new array */` |
|        20 | 7404 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7405 | `	if( pArray == 0 ){` |
|       ! 0 | 7406 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7407 | `		return PH7_OK;` |
|         - | 7408 | `	}` |
|         - | 7409 | `	/* Overwrite from the first array */` |
|        20 | 7410 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7411 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7412 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7413 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7414 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7415 | `			/* Type mismatch -> TypeError */` |
|         4 | 7416 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7417 | `				"TypeError",` |
|         - | 7418 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7419 | `				i + 1,` |
|         2 | 7420 | `				ph7_type_name(apArg[i])` |
|         - | 7421 | `				);` |
|         - | 7422 | `		}` |
|         - | 7423 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7424 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7425 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7426 | `	}` |
|         - | 7427 | `	/* Return the new array */` |
|        17 | 7428 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7429 | `	return PH7_OK;` |
|        15 | 7430 | `}` |
|         - | 7431 | `/*` |
|         - | 7432 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7433 | ` *  Filters elements of an array using a callback function.` |
|         - | 7434 | ` * Parameters` |
|         - | 7435 | ` *  $input` |
|         - | 7436 | ` *    The array to iterate over` |
|         - | 7437 | ` * $callback` |
|         - | 7438 | ` *    The callback function to use` |
|         - | 7439 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7440 | ` *    will be removed.` |
|         - | 7441 | ` * Return` |
|         - | 7442 | ` *  The filtered array.` |
|         - | 7443 | ` */` |
|        32 | 7444 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7445 | `{` |
|         - | 7446 | `	ph7_hashmap_node *pEntry;` |
|         - | 7447 | `	ph7_hashmap *pMap;` |
|         - | 7448 | `	ph7_value *pArray;` |
|         - | 7449 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7450 | `	ph7_value *pValue;` |
|         - | 7451 | `	sxi32 rc;` |
|         - | 7452 | `	int keep;` |
|         - | 7453 | `	sxu32 n;` |
|        34 | 7454 | `	if( nArg < 1 ){` |
|         - | 7455 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7456 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7457 | `		return PH7_OK;` |
|         - | 7458 | `	}` |
|         - | 7459 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        34 | 7460 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7461 | `		char zBuf[64];` |
|        22 | 7462 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7463 | `			"TypeError",` |
|         - | 7464 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         7 | 7465 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7466 | `			);` |
|         - | 7467 | `	}` |
|         - | 7468 | `	/* Create a new array */` |
|        20 | 7469 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7470 | `	if( pArray == 0 ){` |
|       ! 0 | 7471 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7472 | `		return PH7_OK;` |
|         - | 7473 | `	}` |
|         - | 7474 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7475 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7476 | `	pEntry = pMap->pFirst;` |
|        20 | 7477 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7478 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7479 | `	/* Perform the requested operation */` |
|        78 | 7480 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7481 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7482 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7483 | `		if( pValue == 0 ){` |
|         - | 7484 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7485 | `			keep = FALSE;` |
|        64 | 7486 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7487 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7488 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7489 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7490 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7491 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7492 | `					int len;` |
|         3 | 7493 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7494 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7495 | `						"TypeError",` |
|         - | 7496 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7497 | `						zName` |
|         - | 7498 | `						);` |
|       ! 0 | 7499 | `				}else{` |
|       ! 0 | 7500 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7501 | `						"TypeError",` |
|         - | 7502 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7503 | `						ph7_type_name(apArg[1])` |
|         - | 7504 | `						);` |
|         - | 7505 | `				}` |
|         - | 7506 | `			}` |
|        33 | 7507 | `			keep = FALSE;` |
|        33 | 7508 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7509 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7510 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7511 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7512 | `				return PH7_EXCEPTION;` |
|         - | 7513 | `			}` |
|        31 | 7514 | `			if( rc == SXRET_OK ){` |
|         - | 7515 | `				/* Perform a boolean cast */` |
|        31 | 7516 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7517 | `			}` |
|        31 | 7518 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7519 | `		}else{` |
|         - | 7520 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7521 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7522 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7523 | `			 */` |
|        29 | 7524 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7525 | `		}` |
|        59 | 7526 | `		if( keep ){` |
|         - | 7527 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7528 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7529 | `		}` |
|         - | 7530 | `		/* Point to the next entry */` |
|        59 | 7531 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7532 | `	}` |
|        15 | 7533 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7534 | `	return PH7_OK;` |
|        18 | 7535 | `}` |
|         - | 7536 | `/*` |
|         - | 7537 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7538 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7539 | ` * Parameters` |
|         - | 7540 | ` *  $callback` |
|         - | 7541 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7542 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7543 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7544 | ` *   are zipped together.` |
|         - | 7545 | ` *  $array` |
|         - | 7546 | ` *   The first array to run through the callback function.` |
|         - | 7547 | ` *  $arrays` |
|         - | 7548 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7549 | ` * Return` |
|         - | 7550 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7551 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7552 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7553 | ` *  padding shorter arrays with NULL.` |
|         - | 7554 | ` */` |
|        62 | 7555 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7556 | `{` |
|         - | 7557 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7558 | `	ph7_hashmap_node *pEntry;` |
|         - | 7559 | `	ph7_hashmap *pMap;` |
|         - | 7560 | `	ph7_vm *pVm;` |
|         - | 7561 | `	int bNullCallback;` |
|         - | 7562 | `	sxi32 rc;` |
|         - | 7563 | `	int i;` |
|         - | 7564 | `	sxu32 n;` |
|        67 | 7565 | `	if( nArg < 2 ){` |
|         8 | 7566 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7567 | `			"ArgumentCountError",` |
|         - | 7568 | `			"array_map() expects at least 2 arguments, %d given",` |
|         2 | 7569 | `			nArg` |
|         - | 7570 | `			);` |
|         - | 7571 | `	}` |
|        62 | 7572 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        62 | 7573 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7574 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7575 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7576 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7577 | `				"TypeError",` |
|         - | 7578 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7579 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7580 | `				zFunc` |
|         - | 7581 | `				);` |
|         - | 7582 | `		}` |
|         3 | 7583 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7584 | `			"TypeError",` |
|         - | 7585 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7586 | `			"no array or string given"` |
|         - | 7587 | `			);` |
|         - | 7588 | `	}` |
|         - | 7589 | `	/* Every remaining argument must be an array */` |
|       121 | 7590 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        69 | 7591 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7592 | `			if( i == 1 ){` |
|         4 | 7593 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7594 | `					"TypeError",` |
|         - | 7595 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7596 | `					ph7_type_name(apArg[1])` |
|         - | 7597 | `					);` |
|         - | 7598 | `			}` |
|       ! 0 | 7599 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7600 | `				"TypeError",` |
|         - | 7601 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7602 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7603 | `				);` |
|         - | 7604 | `		}` |
|        34 | 7605 | `	}` |
|        54 | 7606 | `	pVm = pCtx->pVm;` |
|         - | 7607 | `	/* Create a new array */` |
|        54 | 7608 | `	pArray = ph7_context_new_array(pCtx);` |
|        54 | 7609 | `	if( pArray == 0 ){` |
|       ! 0 | 7610 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7611 | `		return PH7_OK;` |
|         - | 7612 | `	}` |
|        54 | 7613 | `	PH7_MemObjInit(pVm,&sResult);` |
|        54 | 7614 | `	PH7_MemObjInit(pVm,&sKey);` |
|        54 | 7615 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7616 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7617 | `	if( nArg == 2 ){` |
|         - | 7618 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        44 | 7619 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        44 | 7620 | `		pEntry = pMap->pFirst;` |
|       134 | 7621 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7622 | `			/* Extract the node value */` |
|        96 | 7623 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        96 | 7624 | `			if( pValue ){` |
|         - | 7625 | `				/* Extract the node key */` |
|        96 | 7626 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        96 | 7627 | `				if( bNullCallback ){` |
|         - | 7628 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7629 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7630 | `				}else{` |
|         - | 7631 | `					/* Invoke the supplied callback */` |
|        86 | 7632 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        86 | 7633 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7634 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7635 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7636 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7637 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7638 | `						return PH7_EXCEPTION;` |
|         - | 7639 | `					}` |
|         - | 7640 | `					/* Insert the callback return value */` |
|        82 | 7641 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7642 | `				}` |
|        92 | 7643 | `				PH7_MemObjRelease(&sKey);` |
|        92 | 7644 | `				PH7_MemObjRelease(&sResult);` |
|        45 | 7645 | `			}` |
|         - | 7646 | `			/* Point to the next entry */` |
|        92 | 7647 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        47 | 7648 | `		}` |
|        21 | 7649 | `	}else{` |
|         - | 7650 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7651 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7652 | `		int nArrays = nArg - 1;` |
|         - | 7653 | `		ph7_hashmap_node **apCur;` |
|         - | 7654 | `		ph7_value **apCallArg;` |
|         - | 7655 | `		ph7_value sNull;` |
|        11 | 7656 | `		sxu32 nMax = 0;` |
|        11 | 7657 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7658 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7659 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7660 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7661 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7662 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7663 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7664 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7665 | `			return PH7_OK;` |
|         - | 7666 | `		}` |
|        11 | 7667 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7668 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7669 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7670 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7671 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7672 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7673 | `				nMax = pMap->nEntry;` |
|         6 | 7674 | `			}` |
|        12 | 7675 | `		}` |
|        35 | 7676 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7677 | `			ph7_value *pZip = 0;` |
|        25 | 7678 | `			if( bNullCallback ){` |
|         - | 7679 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7680 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7681 | `			}` |
|        79 | 7682 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7683 | `				ph7_value *pv = &sNull;` |
|        55 | 7684 | `				if( apCur[i] ){` |
|        53 | 7685 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7686 | `					if( pNodeVal ){` |
|        53 | 7687 | `						pv = pNodeVal;` |
|        26 | 7688 | `					}` |
|        53 | 7689 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7690 | `				}` |
|        55 | 7691 | `				if( bNullCallback ){` |
|         9 | 7692 | `					if( pZip ){` |
|         9 | 7693 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7694 | `					}` |
|         5 | 7695 | `				}else{` |
|        47 | 7696 | `					apCallArg[i] = pv;` |
|         - | 7697 | `				}` |
|        28 | 7698 | `			}` |
|        25 | 7699 | `			if( bNullCallback ){` |
|         5 | 7700 | `				if( pZip ){` |
|         5 | 7701 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7702 | `				}` |
|         3 | 7703 | `			}else{` |
|        21 | 7704 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7705 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7706 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7707 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7708 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7709 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7710 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7711 | `					return PH7_EXCEPTION;` |
|         - | 7712 | `				}` |
|        21 | 7713 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7714 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7715 | `			}` |
|        13 | 7716 | `		}` |
|        11 | 7717 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7718 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7719 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7720 | `	}` |
|        50 | 7721 | `	PH7_MemObjRelease(&sKey);` |
|        50 | 7722 | `	PH7_MemObjRelease(&sResult);` |
|        50 | 7723 | `	ph7_result_value(pCtx,pArray);` |
|        50 | 7724 | `	return PH7_OK;` |
|        36 | 7725 | `}` |
|         - | 7726 | `/*` |
|         - | 7727 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7728 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7729 | ` * Parameters` |
|         - | 7730 | ` *  $array` |
|         - | 7731 | ` *   The input array.` |
|         - | 7732 | ` *  $callback` |
|         - | 7733 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7734 | ` *  $initial` |
|         - | 7735 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7736 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7737 | ` * Return` |
|         - | 7738 | ` *  Returns the resulting value.` |
|         - | 7739 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7740 | ` */` |
|        34 | 7741 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7742 | `{` |
|         - | 7743 | `	ph7_hashmap_node *pEntry;` |
|         - | 7744 | `	ph7_hashmap *pMap;` |
|         - | 7745 | `	ph7_value *pValue;` |
|         - | 7746 | `	ph7_value sResult;` |
|         - | 7747 | `	sxi32 rc;` |
|         - | 7748 | `	sxu32 n;` |
|        39 | 7749 | `	if( nArg < 2 ){` |
|         8 | 7750 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7751 | `			"ArgumentCountError",` |
|         - | 7752 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|         2 | 7753 | `			nArg` |
|         - | 7754 | `			);` |
|         - | 7755 | `	}` |
|        35 | 7756 | `	if( nArg > 3 ){` |
|         4 | 7757 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7758 | `			"ArgumentCountError",` |
|         - | 7759 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7760 | `			nArg` |
|         - | 7761 | `			);` |
|         - | 7762 | `	}` |
|        33 | 7763 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7764 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7765 | `			"TypeError",` |
|         - | 7766 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7767 | `			ph7_type_name(apArg[0])` |
|         - | 7768 | `			);` |
|         - | 7769 | `	}` |
|        31 | 7770 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7771 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7772 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7773 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7774 | `				"TypeError",` |
|         - | 7775 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7776 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7777 | `				zFunc` |
|         - | 7778 | `				);` |
|         - | 7779 | `		}` |
|         9 | 7780 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7781 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7782 | `				"TypeError",` |
|         - | 7783 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7784 | `				"array callback must have exactly two members"` |
|         - | 7785 | `				);` |
|         - | 7786 | `		}` |
|         6 | 7787 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7788 | `			"TypeError",` |
|         - | 7789 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7790 | `			"no array or string given"` |
|         - | 7791 | `			);` |
|         - | 7792 | `	}` |
|         - | 7793 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7794 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7795 | `	/* Assume a NULL initial value */` |
|        19 | 7796 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7797 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7798 | `	if( nArg > 2 ){` |
|         - | 7799 | `		/* Set the initial value */` |
|        13 | 7800 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7801 | `	}` |
|         - | 7802 | `	/* Perform the requested operation */` |
|        19 | 7803 | `	pEntry = pMap->pFirst;` |
|        55 | 7804 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7805 | `		/* Extract the node value */` |
|        39 | 7806 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7807 | `		/* Invoke the supplied callback */` |
|        39 | 7808 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7809 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7810 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7811 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7812 | `			return PH7_EXCEPTION;` |
|         - | 7813 | `		}` |
|         - | 7814 | `		/* Point to the next entry */` |
|        37 | 7815 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7816 | `	}` |
|        17 | 7817 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7818 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7819 | `	return PH7_OK;` |
|        22 | 7820 | `}` |
|         - | 7821 | `/*` |
|         - | 7822 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7823 | ` *  Apply a user function to every member of an array.` |
|         - | 7824 | ` * Parameters` |
|         - | 7825 | ` *  $array` |
|         - | 7826 | ` *   The input array.` |
|         - | 7827 | ` *  $funcname` |
|         - | 7828 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7829 | ` *   the first, and the key/index second.` |
|         - | 7830 | ` * Note:` |
|         - | 7831 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7832 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7833 | ` *  be made in the original array itself.` |
|         - | 7834 | ` *  $userdata` |
|         - | 7835 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7836 | ` *   to the callback funcname.` |
|         - | 7837 | ` * Return` |
|         - | 7838 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7839 | ` */` |
|        38 | 7840 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7841 | `{` |
|         - | 7842 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7843 | `	ph7_hashmap_node *pEntry;` |
|         - | 7844 | `	ph7_hashmap *pMap;` |
|         - | 7845 | `	sxu32 n;` |
|        43 | 7846 | `	if( nArg < 2 ){` |
|         8 | 7847 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7848 | `			"ArgumentCountError",` |
|         - | 7849 | `			"array_walk() expects at least 2 arguments, %d given",` |
|         2 | 7850 | `			nArg` |
|         - | 7851 | `			);` |
|         - | 7852 | `	}` |
|        39 | 7853 | `	if( nArg > 3 ){` |
|         4 | 7854 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7855 | `			"ArgumentCountError",` |
|         - | 7856 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7857 | `			nArg` |
|         - | 7858 | `			);` |
|         - | 7859 | `	}` |
|        37 | 7860 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7861 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7862 | `			"TypeError",` |
|         - | 7863 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7864 | `			ph7_type_name(apArg[0])` |
|         - | 7865 | `			);` |
|         - | 7866 | `	}` |
|        35 | 7867 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7868 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7869 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7870 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7871 | `				"TypeError",` |
|         - | 7872 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7873 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7874 | `				zFunc` |
|         - | 7875 | `				);` |
|         - | 7876 | `		}` |
|        12 | 7877 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7878 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7879 | `				"TypeError",` |
|         - | 7880 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7881 | `				"array callback must have exactly two members"` |
|         - | 7882 | `				);` |
|         - | 7883 | `		}` |
|         6 | 7884 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7885 | `			"TypeError",` |
|         - | 7886 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7887 | `			"no array or string given"` |
|         - | 7888 | `			);` |
|         - | 7889 | `	}` |
|        21 | 7890 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7891 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7892 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7893 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7894 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7895 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7896 | `	/* Perform the desired operation */` |
|        21 | 7897 | `	pEntry = pMap->pFirst;` |
|        61 | 7898 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7899 | `		/* Extract the node value */` |
|        43 | 7900 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7901 | `		if( pValue ){` |
|         - | 7902 | `			sxi32 rcW;` |
|         - | 7903 | `			/* Extract the entry key */` |
|        43 | 7904 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7905 | `			/* Invoke the supplied callback */` |
|        43 | 7906 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7907 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7908 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7909 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7910 | `				return PH7_EXCEPTION;` |
|         - | 7911 | `			}` |
|        20 | 7912 | `		}` |
|         - | 7913 | `		/* Point to the next entry */` |
|        41 | 7914 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7915 | `	}` |
|         - | 7916 | `	/* All done, return TRUE */` |
|        19 | 7917 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7918 | `	return PH7_OK;` |
|        24 | 7919 | `}` |
|         - | 7920 | `/*` |
|         - | 7921 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7922 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7923 | ` */` |
|        22 | 7924 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7925 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7926 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7927 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7928 | `	int iNest             /* Nesting level */` |
|         - | 7929 | `	)` |
|         1 | 7930 | `{` |
|         - | 7931 | `	ph7_hashmap_node *pEntry;` |
|         - | 7932 | `	ph7_value *pValue,sKey;` |
|         - | 7933 | `	sxi32 rc;` |
|         - | 7934 | `	sxu32 n;` |
|         - | 7935 | `	/* Iterate through hashmap entries */` |
|        23 | 7936 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7937 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7938 | `	pEntry = pMap->pFirst;` |
|        59 | 7939 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7940 | `		/* Extract the node value */` |
|        37 | 7941 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7942 | `		if( pValue ){` |
|        37 | 7943 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7944 | `				if( iNest < 32 ){` |
|         - | 7945 | `					/* Recurse */` |
|        11 | 7946 | `					iNest++;` |
|        11 | 7947 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 7948 | `					iNest--;` |
|        11 | 7949 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7950 | `						return PH7_EXCEPTION;` |
|         - | 7951 | `					}` |
|         5 | 7952 | `				}` |
|         6 | 7953 | `			}else{` |
|         - | 7954 | `				/* Extract the node key */` |
|        27 | 7955 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7956 | `				/* Invoke the supplied callback */` |
|        27 | 7957 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 7958 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 7959 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 7960 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7961 | `					return PH7_EXCEPTION;` |
|         - | 7962 | `				}` |
|         - | 7963 | `			}` |
|        18 | 7964 | `		}` |
|         - | 7965 | `		/* Point to the next entry */` |
|        37 | 7966 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7967 | `	}` |
|        23 | 7968 | `	return PH7_OK;` |
|        12 | 7969 | `}` |
|         - | 7970 | `/*` |
|         - | 7971 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7972 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 7973 | ` * Parameters` |
|         - | 7974 | ` *  $array` |
|         - | 7975 | ` *   The input array.` |
|         - | 7976 | ` *  $funcname` |
|         - | 7977 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7978 | ` *   the first, and the key/index second.` |
|         - | 7979 | ` * Note:` |
|         - | 7980 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7981 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7982 | ` *  be made in the original array itself.` |
|         - | 7983 | ` *  $userdata` |
|         - | 7984 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7985 | ` *   to the callback funcname.` |
|         - | 7986 | ` * Return` |
|         - | 7987 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7988 | ` */` |
|        30 | 7989 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7990 | `{` |
|         - | 7991 | `	ph7_hashmap *pMap;` |
|        35 | 7992 | `	if( nArg < 2 ){` |
|         8 | 7993 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7994 | `			"ArgumentCountError",` |
|         - | 7995 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|         2 | 7996 | `			nArg` |
|         - | 7997 | `			);` |
|         - | 7998 | `	}` |
|        31 | 7999 | `	if( nArg > 3 ){` |
|         4 | 8000 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8001 | `			"ArgumentCountError",` |
|         - | 8002 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8003 | `			nArg` |
|         - | 8004 | `			);` |
|         - | 8005 | `	}` |
|        29 | 8006 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8007 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8008 | `			"TypeError",` |
|         - | 8009 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8010 | `			ph7_type_name(apArg[0])` |
|         - | 8011 | `			);` |
|         - | 8012 | `	}` |
|        27 | 8013 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8014 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8015 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8016 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8017 | `				"TypeError",` |
|         - | 8018 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8019 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8020 | `				zFunc` |
|         - | 8021 | `				);` |
|         - | 8022 | `		}` |
|        12 | 8023 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8024 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8025 | `				"TypeError",` |
|         - | 8026 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8027 | `				"array callback must have exactly two members"` |
|         - | 8028 | `				);` |
|         - | 8029 | `		}` |
|         6 | 8030 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8031 | `			"TypeError",` |
|         - | 8032 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8033 | `			"no array or string given"` |
|         - | 8034 | `			);` |
|         - | 8035 | `	}` |
|         - | 8036 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8037 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8038 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8039 | `	/* Perform the desired operation */` |
|        13 | 8040 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8041 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8042 | `		return PH7_EXCEPTION;` |
|         - | 8043 | `	}` |
|         - | 8044 | `	/* All done, return TRUE */` |
|        13 | 8045 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8046 | `	return PH7_OK;` |
|        20 | 8047 | `}` |
|         - | 8048 | `/*` |
|         - | 8049 | ` * bool array_is_list(array $array)` |
|         - | 8050 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8051 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8052 | ` * Return` |
|         - | 8053 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8054 | ` */` |
|         - | 8055 | `/*` |
|         - | 8056 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8057 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8058 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8059 | ` */` |
|       246 | 8060 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8061 | `{` |
|       247 | 8062 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       247 | 8063 | `	sxi64 iExpect = 0;` |
|         - | 8064 | `	sxu32 n;` |
|       555 | 8065 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       409 | 8066 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8067 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       101 | 8068 | `			return 0;` |
|         - | 8069 | `		}` |
|       309 | 8070 | `		++iExpect;` |
|       309 | 8071 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       155 | 8072 | `	}` |
|       147 | 8073 | `	return 1;` |
|       124 | 8074 | `}` |
|        12 | 8075 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8076 | `{` |
|        13 | 8077 | `	if( nArg < 1 ){` |
|       ! 0 | 8078 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8079 | `			"ArgumentCountError",` |
|         - | 8080 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8081 | `			);` |
|         - | 8082 | `	}` |
|        13 | 8083 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8084 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8085 | `			"TypeError",` |
|         - | 8086 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8087 | `			ph7_type_name(apArg[0])` |
|         - | 8088 | `			);` |
|         - | 8089 | `	}` |
|        13 | 8090 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8091 | `	return PH7_OK;` |
|         7 | 8092 | `}` |
|         - | 8093 | `/*` |
|         - | 8094 | ` * mixed array_first(array $array)` |
|         - | 8095 | ` * mixed array_last(array $array)` |
|         - | 8096 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8097 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8098 | ` *  untouched (unlike reset()/end()).` |
|         - | 8099 | ` */` |
|        20 | 8100 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8101 | `{` |
|         - | 8102 | `	ph7_hashmap *pMap;` |
|         - | 8103 | `	ph7_hashmap_node *pNode;` |
|         - | 8104 | `	ph7_value *pVal;` |
|        21 | 8105 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        21 | 8106 | `	if( nArg < 1 ){` |
|         4 | 8107 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8108 | `			"ArgumentCountError",` |
|         - | 8109 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8110 | `			zName` |
|         - | 8111 | `			);` |
|         - | 8112 | `	}` |
|        19 | 8113 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8114 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8115 | `			"TypeError",` |
|         - | 8116 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8117 | `			zName,` |
|         1 | 8118 | `			ph7_type_name(apArg[0])` |
|         - | 8119 | `			);` |
|         - | 8120 | `	}` |
|        17 | 8121 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8122 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8123 | `	if( pNode == 0 ){` |
|         - | 8124 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8125 | `		ph7_result_null(pCtx);` |
|         5 | 8126 | `		return PH7_OK;` |
|         - | 8127 | `	}` |
|        13 | 8128 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8129 | `	if( pVal ){` |
|        13 | 8130 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8131 | `	}else{` |
|       ! 0 | 8132 | `		ph7_result_null(pCtx);` |
|         - | 8133 | `	}` |
|        13 | 8134 | `	return PH7_OK;` |
|        11 | 8135 | `}` |
|        10 | 8136 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8137 | `{` |
|        11 | 8138 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8139 | `}` |
|        10 | 8140 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8141 | `{` |
|        11 | 8142 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8143 | `}` |
|         - | 8144 | `/*` |
|         - | 8145 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8146 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8147 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8148 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8149 | ` *  untouched.` |
|         - | 8150 | ` */` |
|        24 | 8151 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8152 | `{` |
|         - | 8153 | `	ph7_hashmap *pMap;` |
|         - | 8154 | `	ph7_hashmap_node *pNode;` |
|        25 | 8155 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        25 | 8156 | `	if( nArg < 1 ){` |
|         4 | 8157 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8158 | `			"ArgumentCountError",` |
|         - | 8159 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8160 | `			zName` |
|         - | 8161 | `			);` |
|         - | 8162 | `	}` |
|        23 | 8163 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8164 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8165 | `			"TypeError",` |
|         - | 8166 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8167 | `			zName,` |
|         1 | 8168 | `			ph7_type_name(apArg[0])` |
|         - | 8169 | `			);` |
|         - | 8170 | `	}` |
|        21 | 8171 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8172 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8173 | `	if( pNode == 0 ){` |
|         - | 8174 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8175 | `		ph7_result_null(pCtx);` |
|         5 | 8176 | `		return PH7_OK;` |
|         - | 8177 | `	}` |
|        17 | 8178 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8179 | `	return PH7_OK;` |
|        13 | 8180 | `}` |
|        12 | 8181 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8182 | `{` |
|        13 | 8183 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8184 | `}` |
|        12 | 8185 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8186 | `{` |
|        13 | 8187 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8188 | `}` |
|         - | 8189 | `/*` |
|         - | 8190 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8191 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8192 | ` * array_column() for both the column value and the index key.` |
|         - | 8193 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8194 | ` * container or the key is absent.` |
|         - | 8195 | ` */` |
|        32 | 8196 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8197 | `{` |
|        33 | 8198 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8199 | `		ph7_hashmap_node *pNode;` |
|        25 | 8200 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8201 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8202 | `		}` |
|        11 | 8203 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8204 | `		ph7_value sName;` |
|         - | 8205 | `		const char *zName;` |
|         - | 8206 | `		ph7_value *pAttr;` |
|         - | 8207 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8208 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8209 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8210 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8211 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8212 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8213 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8214 | `		return pAttr;` |
|         - | 8215 | `	}` |
|         5 | 8216 | `	return 0;` |
|        17 | 8217 | `}` |
|         - | 8218 | `/*` |
|         - | 8219 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8220 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8221 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8222 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8223 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8224 | ` *  Each row may be an array or an object.` |
|         - | 8225 | ` */` |
|        12 | 8226 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8227 | `{` |
|         - | 8228 | `	ph7_hashmap_node *pNode;` |
|         - | 8229 | `	ph7_hashmap *pMap;` |
|         - | 8230 | `	ph7_value *pArray;` |
|         - | 8231 | `	ph7_value *pRow;` |
|         - | 8232 | `	ph7_value *pCol;` |
|         - | 8233 | `	ph7_value *pIdx;` |
|         - | 8234 | `	int bWantCol;` |
|         - | 8235 | `	int bWantIdx;` |
|         - | 8236 | `	sxu32 n;` |
|        13 | 8237 | `	if( nArg < 2 ){` |
|       ! 0 | 8238 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8239 | `			"ArgumentCountError",` |
|         - | 8240 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8241 | `			nArg` |
|         - | 8242 | `			);` |
|         - | 8243 | `	}` |
|        13 | 8244 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8245 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8246 | `			"TypeError",` |
|         - | 8247 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8248 | `			ph7_type_name(apArg[0])` |
|         - | 8249 | `			);` |
|         - | 8250 | `	}` |
|        13 | 8251 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8252 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8253 | `	if( pArray == 0 ){` |
|       ! 0 | 8254 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8255 | `		return PH7_OK;` |
|         - | 8256 | `	}` |
|         - | 8257 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8258 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8259 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8260 | `	pNode = pMap->pFirst;` |
|        33 | 8261 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8262 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8263 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8264 | `		if( pRow == 0 ){` |
|       ! 0 | 8265 | `			continue;` |
|         - | 8266 | `		}` |
|        21 | 8267 | `		if( bWantCol ){` |
|        19 | 8268 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8269 | `			if( pCol == 0 ){` |
|         - | 8270 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8271 | `				continue;` |
|         - | 8272 | `			}` |
|         9 | 8273 | `		}else{` |
|         3 | 8274 | `			pCol = pRow;` |
|         - | 8275 | `		}` |
|        19 | 8276 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8277 | `		if( pIdx ){` |
|        13 | 8278 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8279 | `		}else{` |
|         7 | 8280 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8281 | `		}` |
|        10 | 8282 | `	}` |
|        13 | 8283 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8284 | `	return PH7_OK;` |
|         7 | 8285 | `}` |
|         - | 8286 | `/*` |
|         - | 8287 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8288 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8289 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8290 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8291 | ` */` |
|        28 | 8292 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8293 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8294 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8295 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8296 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8297 | `	)` |
|         1 | 8298 | `{` |
|         - | 8299 | `	ph7_hashmap_node *pEntry;` |
|         - | 8300 | `	ph7_hashmap *pMap;` |
|         - | 8301 | `	ph7_value *pValue;` |
|         - | 8302 | `	ph7_value *apCbArg[2];` |
|         - | 8303 | `	ph7_value sKey;` |
|         - | 8304 | `	ph7_value sResult;` |
|         - | 8305 | `	sxi32 rc;` |
|         - | 8306 | `	sxu32 n;` |
|        29 | 8307 | `	*ppMatch = 0;` |
|        29 | 8308 | `	if( nArg < 2 ){` |
|       ! 0 | 8309 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8310 | `			"ArgumentCountError",` |
|         - | 8311 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8312 | `			zName,nArg` |
|         - | 8313 | `			);` |
|         - | 8314 | `	}` |
|        29 | 8315 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8316 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8317 | `			"TypeError",` |
|         - | 8318 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8319 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8320 | `			);` |
|         - | 8321 | `	}` |
|        29 | 8322 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8323 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8324 | `			"TypeError",` |
|         - | 8325 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8326 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8327 | `			);` |
|         - | 8328 | `	}` |
|        29 | 8329 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8330 | `	pEntry = pMap->pFirst;` |
|        29 | 8331 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8332 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8333 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8334 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8335 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8336 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8337 | `		if( pValue ){` |
|         - | 8338 | `			/* The callback receives ($value, $key). */` |
|        59 | 8339 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8340 | `			apCbArg[0] = pValue;` |
|        59 | 8341 | `			apCbArg[1] = &sKey;` |
|        59 | 8342 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8343 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8344 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8345 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8346 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8347 | `				return PH7_EXCEPTION;` |
|         - | 8348 | `			}` |
|        59 | 8349 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8350 | `				*ppMatch = pEntry;` |
|        15 | 8351 | `				break;` |
|         - | 8352 | `			}` |
|        22 | 8353 | `		}` |
|        45 | 8354 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8355 | `	}` |
|        29 | 8356 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8357 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8358 | `	return PH7_OK;` |
|        15 | 8359 | `}` |
|         - | 8360 | `/*` |
|         - | 8361 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8362 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8363 | ` *  is truthy, or NULL if none match.` |
|         - | 8364 | ` */` |
|         6 | 8365 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8366 | `{` |
|         - | 8367 | `	ph7_hashmap_node *pMatch;` |
|         - | 8368 | `	ph7_value *pVal;` |
|         - | 8369 | `	sxi32 rc;` |
|         7 | 8370 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8371 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8372 | `		return rc;` |
|         - | 8373 | `	}` |
|         7 | 8374 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8375 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8376 | `	}else{` |
|         3 | 8377 | `		ph7_result_null(pCtx);` |
|         - | 8378 | `	}` |
|         7 | 8379 | `	return PH7_OK;` |
|         4 | 8380 | `}` |
|         - | 8381 | `/*` |
|         - | 8382 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8383 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8384 | ` *  is truthy, or NULL if none match.` |
|         - | 8385 | ` */` |
|         6 | 8386 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8387 | `{` |
|         - | 8388 | `	ph7_hashmap_node *pMatch;` |
|         - | 8389 | `	sxi32 rc;` |
|         7 | 8390 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8391 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8392 | `		return rc;` |
|         - | 8393 | `	}` |
|         7 | 8394 | `	if( pMatch == 0 ){` |
|         3 | 8395 | `		ph7_result_null(pCtx);` |
|         6 | 8396 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8397 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8398 | `	}else{` |
|         4 | 8399 | `		ph7_result_string(pCtx,` |
|         2 | 8400 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8401 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8402 | `	}` |
|         7 | 8403 | `	return PH7_OK;` |
|         4 | 8404 | `}` |
|         - | 8405 | `/*` |
|         - | 8406 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8407 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8408 | ` *  FALSE for an empty array.` |
|         - | 8409 | ` */` |
|         8 | 8410 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8411 | `{` |
|         - | 8412 | `	ph7_hashmap_node *pMatch;` |
|         - | 8413 | `	sxi32 rc;` |
|         9 | 8414 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8415 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8416 | `		return rc;` |
|         - | 8417 | `	}` |
|         9 | 8418 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8419 | `	return PH7_OK;` |
|         5 | 8420 | `}` |
|         - | 8421 | `/*` |
|         - | 8422 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8423 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8424 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8425 | ` */` |
|         8 | 8426 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8427 | `{` |
|         - | 8428 | `	ph7_hashmap_node *pMatch;` |
|         - | 8429 | `	sxi32 rc;` |
|         9 | 8430 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8431 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8432 | `		return rc;` |
|         - | 8433 | `	}` |
|         9 | 8434 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8435 | `	return PH7_OK;` |
|         5 | 8436 | `}` |
|         - | 8437 | `/*` |
|         - | 8438 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8439 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8440 | ` */` |
|         - | 8441 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8442 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8443 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8444 | `{` |
|        84 | 8445 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8446 | `	(void)pVm;` |
|        84 | 8447 | `	p->nCount++;` |
|        84 | 8448 | `	if( p->pArray ){` |
|         - | 8449 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8450 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8451 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8452 | `	}` |
|        84 | 8453 | `	return SXRET_OK;` |
|         4 | 8454 | `}` |
|         - | 8455 | `/*` |
|         - | 8456 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8457 | ` */` |
|        30 | 8458 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8459 | `{` |
|         - | 8460 | `	struct IterCollect sCol;` |
|         - | 8461 | `	ph7_value *pArray;` |
|         - | 8462 | `	sxi32 rc;` |
|        34 | 8463 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8464 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8465 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8466 | `	sCol.pArray = pArray;` |
|        34 | 8467 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8468 | `	sCol.nCount = 0;` |
|        34 | 8469 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8470 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8471 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8472 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8473 | `		sxu32 n;` |
|         9 | 8474 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8475 | `			ph7_value sKey, *pVal;` |
|         7 | 8476 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8477 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8478 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8479 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8480 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8481 | `			pEntry = pEntry->pPrev;` |
|         4 | 8482 | `		}` |
|         3 | 8483 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8484 | `		return PH7_OK;` |
|         - | 8485 | `	}` |
|        32 | 8486 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8487 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8488 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8489 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8490 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8491 | `			ph7_type_name(apArg[0]));` |
|         - | 8492 | `	}` |
|        30 | 8493 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8494 | `	return PH7_OK;` |
|        19 | 8495 | `}` |
|         - | 8496 | `/*` |
|         - | 8497 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8498 | ` */` |
|         8 | 8499 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8500 | `{` |
|         - | 8501 | `	struct IterCollect sCol;` |
|         - | 8502 | `	sxi32 rc;` |
|         9 | 8503 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8504 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8505 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8506 | `		return PH7_OK;` |
|         - | 8507 | `	}` |
|         7 | 8508 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8509 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8510 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8511 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8512 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8513 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8514 | `			ph7_type_name(apArg[0]));` |
|         - | 8515 | `	}` |
|         7 | 8516 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8517 | `	return PH7_OK;` |
|         5 | 8518 | `}` |
|         - | 8519 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8520 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8521 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8522 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8523 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8524 | `{` |
|        33 | 8525 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8526 | `	ph7_value sResult;` |
|         - | 8527 | `	SySet aArg;` |
|         - | 8528 | `	sxi32 rc;` |
|         - | 8529 | `	int bContinue;` |
|        16 | 8530 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8531 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8532 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8533 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8534 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8535 | `		sxu32 n;` |
|        17 | 8536 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8537 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8538 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8539 | `			pEntry = pEntry->pPrev;` |
|         5 | 8540 | `		}` |
|         4 | 8541 | `	}` |
|        33 | 8542 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8543 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8544 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8545 | `	SySetRelease(&aArg);` |
|        33 | 8546 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8547 | `	p->nCount++;` |
|        31 | 8548 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8549 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8550 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8551 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8552 | `}` |
|         - | 8553 | `/*` |
|         - | 8554 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8555 | ` */` |
|        12 | 8556 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8557 | `{` |
|         - | 8558 | `	struct IterApply sApp;` |
|         - | 8559 | `	sxi32 rc;` |
|        13 | 8560 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8561 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8562 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8563 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8564 | `	}` |
|        13 | 8565 | `	sApp.pCallback = apArg[1];` |
|        13 | 8566 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8567 | `	sApp.nCount = 0;` |
|        13 | 8568 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8569 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8570 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8571 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8572 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8573 | `			ph7_type_name(apArg[0]));` |
|         - | 8574 | `	}` |
|        11 | 8575 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8576 | `	return PH7_OK;` |
|         7 | 8577 | `}` |
|         - | 8578 | `/*` |
|         - | 8579 | ` * Table of hashmap functions.` |
|         - | 8580 | ` */` |
|         - | 8581 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8582 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8583 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8584 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8585 | `	{"count",             ph7_hashmap_count },` |
|         - | 8586 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8587 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8588 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8589 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8590 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8591 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8592 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8593 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8594 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8595 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8596 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8597 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8598 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8599 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8600 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8601 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8602 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8603 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8604 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8605 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8606 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8607 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8608 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8609 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8610 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8611 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8612 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8613 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8614 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8615 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8616 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8617 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8618 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8619 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8620 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8621 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8622 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8623 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8624 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8625 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8626 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8627 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8628 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8629 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8630 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8631 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8632 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8633 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8634 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8635 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8636 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8637 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8638 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8639 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8640 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8641 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8642 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8643 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8644 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8645 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8646 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8647 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8648 | `	{"current",           ph7_hashmap_current },` |
|         - | 8649 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8650 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8651 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8652 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8653 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8654 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8655 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8656 | `};` |
|         - | 8657 | `/*` |
|         - | 8658 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8659 | ` */` |
|      3550 | 8660 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8661 | `{` |
|         - | 8662 | `	sxu32 n;` |
|    266255 | 8663 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    262705 | 8664 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    131355 | 8665 | `	}` |
|      3555 | 8666 | `}` |
|         - | 8667 | `/*` |
|         - | 8668 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8669 | ` * the BLOB given as the first argument.` |
|         - | 8670 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8671 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8672 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8673 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8674 | ` */` |
|         - | 8675 | `/*` |
|         - | 8676 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8677 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8678 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8679 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8680 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8681 | ` */` |
|       120 | 8682 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8683 | `{` |
|       122 | 8684 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8685 | `	ph7_value *pObj;` |
|       122 | 8686 | `	sxu32 n = 0;` |
|         - | 8687 | `	int isRef;` |
|       122 | 8688 | `	sxi32 rc = SXRET_OK;` |
|         - | 8689 | `	int i;` |
|       195 | 8690 | `	for(;;){` |
|       392 | 8691 | `		if( n >= pMap->nEntry ){` |
|       122 | 8692 | `			break;` |
|         - | 8693 | `		}` |
|       272 | 8694 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       272 | 8695 | `		isRef = (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0;` |
|       272 | 8696 | `		if( ShowType ){` |
|         - | 8697 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8698 | `			 * on the next line at the same indent (php). */` |
|       104 | 8699 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        70 | 8700 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        36 | 8701 | `			}` |
|        36 | 8702 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        23 | 8703 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        12 | 8704 | `			}else{` |
|        20 | 8705 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8706 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8707 | `			}` |
|        36 | 8708 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        36 | 8709 | `			if( pObj ){` |
|        36 | 8710 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        36 | 8711 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8712 | `					break;` |
|         - | 8713 | `				}` |
|        17 | 8714 | `			}` |
|        19 | 8715 | `		}else{` |
|         - | 8716 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8717 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8718 | `			 * php's extra blank line. References carry no marker. */` |
|      1294 | 8719 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1058 | 8720 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       530 | 8721 | `			}` |
|       238 | 8722 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       125 | 8723 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        63 | 8724 | `			}else{` |
|       170 | 8725 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8726 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8727 | `			}` |
|       236 | 8728 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       132 | 8729 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8730 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8731 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8732 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8733 | `					break;` |
|         - | 8734 | `				}` |
|        13 | 8735 | `			}else{` |
|       214 | 8736 | `				if( pObj ){` |
|       214 | 8737 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       106 | 8738 | `				}` |
|       214 | 8739 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8740 | `			}` |
|         - | 8741 | `		}` |
|         - | 8742 | `		/* Point to the next entry */` |
|       272 | 8743 | `		n++;` |
|       272 | 8744 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 8745 | `	}` |
|       122 | 8746 | `	return rc;` |
|         2 | 8747 | `}` |
|       116 | 8748 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8749 | `{` |
|         - | 8750 | `	sxi32 rc;` |
|         - | 8751 | `	int i;` |
|       118 | 8752 | `	if( nDepth > 31 ){` |
|         - | 8753 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8754 | `		/* Nesting limit reached */` |
|       ! 0 | 8755 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8756 | `		return SXERR_LIMIT;` |
|         - | 8757 | `	}` |
|       118 | 8758 | `	if( ShowType ){` |
|         - | 8759 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8760 | `		 * newline (a nested array is itself an entry value line). */` |
|        14 | 8761 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        14 | 8762 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        14 | 8763 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        14 | 8764 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8765 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8766 | `		}` |
|        14 | 8767 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        14 | 8768 | `		return rc;` |
|         - | 8769 | `	}` |
|         - | 8770 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       105 | 8771 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       297 | 8772 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8773 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8774 | `	}` |
|       105 | 8775 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       105 | 8776 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       297 | 8777 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8778 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8779 | `	}` |
|       105 | 8780 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       105 | 8781 | `	return rc;` |
|        60 | 8782 | `}` |
|         - | 8783 | `/*` |
|         - | 8784 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8785 | ` * retrieved entry.` |
|         - | 8786 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8787 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8788 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8789 | ` * a value different from PH7_OK.` |
|         - | 8790 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8791 | ` */` |
|     34250 | 8792 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8793 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8794 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8795 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8796 | `	)` |
|         5 | 8797 | `{` |
|         - | 8798 | `	ph7_hashmap_node *pEntry;` |
|         - | 8799 | `	ph7_value sKey,sValue;` |
|         - | 8800 | `	sxi32 rc;` |
|         - | 8801 | `	sxu32 n;` |
|         - | 8802 | `	/* Initialize walker parameter */` |
|     34255 | 8803 | `	rc = SXRET_OK;` |
|     34255 | 8804 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     34255 | 8805 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     34255 | 8806 | `	n = pMap->nEntry;` |
|     34255 | 8807 | `	pEntry = pMap->pFirst;` |
|         - | 8808 | `	/* Start the iteration process */` |
|     92424 | 8809 | `	for(;;){` |
|    184853 | 8810 | `		if( n < 1 ){` |
|     34255 | 8811 | `			break;` |
|         - | 8812 | `		}` |
|         - | 8813 | `		/* Extract a copy of the key and a copy the current value */` |
|    150603 | 8814 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    150603 | 8815 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8816 | `		/* Invoke the user callback */` |
|    150603 | 8817 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8818 | `		/* Release the copy of the key and the value */` |
|    150603 | 8819 | `		PH7_MemObjRelease(&sKey);` |
|    150603 | 8820 | `		PH7_MemObjRelease(&sValue);` |
|    150603 | 8821 | `		if( rc != PH7_OK ){` |
|         - | 8822 | `			/* Callback request an operation abort */` |
|       ! 0 | 8823 | `			return SXERR_ABORT;` |
|         - | 8824 | `		}` |
|         - | 8825 | `		/* Point to the next entry */` |
|    150603 | 8826 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    150603 | 8827 | `		n--;` |
|         5 | 8828 | `	}` |
|         - | 8829 | `	/* All done */` |
|     34255 | 8830 | `	return SXRET_OK;` |
|     17130 | 8831 | `}` |
|         - | 8832 |  |
