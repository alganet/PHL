# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3981/4396 lines (90.56%)

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
|   7451586 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7451591 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7451591 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    642060 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    642065 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    642065 |   35 | `	sxu32 nH = 5381;` |
|    642065 |   36 | `	zEnd = &zIn[nLen];` |
|    727781 |   37 | `	for(;;){` |
|   1455567 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1239043 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1111511 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    969077 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    642065 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      1382 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      1387 |   54 | `	sxi64 iCount = 0;` |
|      1387 |   55 | `	if( !bRecursive ){` |
|      1213 |   56 | `		iCount = pMap->nEntry;` |
|       609 |   57 | `	}else{` |
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
|      1387 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3152010 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3152015 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3152015 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3152015 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3152015 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3152015 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3152015 |  112 | `	pNode->nHash = nHash;` |
|   3152015 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3152015 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3152015 |  115 | `	return pNode;` |
|   1576010 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    271292 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    271297 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    271297 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    271297 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    271297 |  133 | `	pNode->pMap  = &(*pMap);` |
|    271297 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    271297 |  135 | `	pNode->nHash = nHash;` |
|    271297 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    271297 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    271297 |  138 | `	pNode->nValIdx = nValIdx;` |
|    271297 |  139 | `	return pNode;` |
|    135651 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3423302 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3423307 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2942195 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2942195 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1471095 |  150 | `	}` |
|   3423307 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3423307 |  153 | `	if( pMap->pFirst == 0 ){` |
|     91737 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     91737 |  156 | `		pMap->pCur = pNode;` |
|     45871 |  157 | `	}else{` |
|   3331575 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3423307 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3423307 |  174 | `	++pMap->nEntry;` |
|   3423307 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7648 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7653 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7653 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7653 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7195 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3600 |  187 | `	}else{` |
|       460 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7653 |  190 | `	if( pNode->pNextCollide ){` |
|      5037 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2517 |  192 | `	}` |
|      7653 |  193 | `	if( pMap->pFirst == pNode ){` |
|       145 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        70 |  195 | `	}` |
|      7653 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       145 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|        70 |  199 | `	}` |
|      7653 |  200 | `	if( pMap->pActiveSteps ){` |
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
|      7653 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7653 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       107 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       107 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       107 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|        51 |  218 | `		}` |
|        51 |  219 | `	}` |
|      7653 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7501 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3748 |  222 | `	}` |
|      7653 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7653 |  224 | `	pMap->nEntry--;` |
|      7653 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|        83 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|        83 |  228 | `		pMap->apBucket = 0;` |
|        83 |  229 | `		pMap->nSize = 0;` |
|        83 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        39 |  231 | `	}` |
|      7653 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3423302 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3423307 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     96773 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     96773 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     96773 |  245 | `		if( nNew < 1 ){` |
|     91737 |  246 | `			nNew = 16;` |
|     45866 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     96773 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     96773 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     96773 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     96773 |  260 | `		pMap->apBucket = apNew;` |
|     96773 |  261 | `		pMap->nSize = nNew;` |
|     96773 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     91737 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5041 |  267 | `		pEntry = pMap->pFirst;` |
|      5041 |  268 | `		n = 0;` |
|   2105830 |  269 | `		for( ;; ){` |
|   4211665 |  270 | `			if( n >= pMap->nEntry ){` |
|      5041 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   4206629 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   4206629 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4206629 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3591845 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3591845 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1795920 |  280 | `			}` |
|   4206629 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   4206629 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4206629 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5041 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2518 |  288 | `	}` |
|   3331575 |  289 | `	return SXRET_OK;` |
|   1711656 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3152010 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3152015 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3151977 |  310 | `		if( pValue ){` |
|   3151971 |  311 | `			sSafeVal = *pValue;` |
|   3151971 |  312 | `			pValue = &sSafeVal;` |
|   1575983 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3151977 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3151977 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3151977 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3151971 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1575983 |  322 | `		}` |
|   3151977 |  323 | `		nIdx = pObj->nIdx;` |
|   1575991 |  324 | `	}else{` |
|        39 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3152015 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3152015 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3152015 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3152015 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        39 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        19 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3152015 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3152015 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3152015 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3152015 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3152015 |  349 | `	return SXRET_OK;` |
|   1576010 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    271292 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    271297 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    224143 |  370 | `		if( pValue ){` |
|    223853 |  371 | `			sSafeVal = *pValue;` |
|    223853 |  372 | `			pValue = &sSafeVal;` |
|    111924 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    224143 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    224143 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    224143 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    223853 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    111924 |  382 | `		}` |
|    224143 |  383 | `		nIdx = pObj->nIdx;` |
|    112074 |  384 | `	}else{` |
|     47159 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    271297 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    271297 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    271297 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    271297 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     47159 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     23577 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    271297 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    271297 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    271297 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    271297 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    271297 |  409 | `	return SXRET_OK;` |
|    135651 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4286506 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4286511 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       691 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4285825 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4285825 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110563443 |  433 | `	for(;;){` |
| 221126891 |  434 | `		if( pNode == 0 ){` |
|   4282071 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216844820 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216841808 |  438 | `			&& pNode->nHash == nHash` |
| 108421280 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      3759 |  441 | `				if( ppNode ){` |
|      3741 |  442 | `					*ppNode = pNode;` |
|      1868 |  443 | `				}` |
|      3759 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216841067 |  447 | `		pNode = pNode->pNextCollide;` |
|         1 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4282071 |  450 | `	return SXERR_NOTFOUND;` |
|   2143258 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    406702 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    406707 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     35939 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    370773 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    370773 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    306594 |  475 | `	for(;;){` |
|    613193 |  476 | `		if( pNode == 0 ){` |
|    311781 |  477 | `			break;` |
|         - |  478 | `		}` |
|    301412 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    299901 |  480 | `			&& pNode->nHash == nHash` |
|    178741 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     59097 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     58997 |  484 | `				if( ppNode ){` |
|     58969 |  485 | `					*ppNode = pNode;` |
|     29482 |  486 | `				}` |
|     58997 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    242425 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    311781 |  493 | `	return SXERR_NOTFOUND;` |
|    203356 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    406834 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    406839 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    406839 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    406839 |  504 | `	int isNeg = FALSE, nDigit;` |
|    406839 |  505 | `	if( zIn >= zEnd ){` |
|       ! 0 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    406839 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    406835 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    406835 |  516 | `	zDigit = zIn;` |
|    203849 |  517 | `	for(;;){` |
|    407703 |  518 | `		if( zIn >= zEnd ){` |
|       251 |  519 | `			break;` |
|         - |  520 | `		}` |
|    407453 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    406585 |  523 | `			return FALSE;` |
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
|    203422 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    139308 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    139313 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    139313 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    135591 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast */` |
|       ! 0 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  559 | `		}` |
|    135591 |  560 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Perform a blob lookup */` |
|    135571 |  562 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    135571 |  563 | `			goto result;` |
|         - |  564 | `		}` |
|        10 |  565 | `	}` |
|         - |  566 | `	/* Perform an int lookup */` |
|      3747 |  567 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  568 | `		/* Force an integer cast */` |
|        35 |  569 | `		PH7_MemObjToInteger(pKey);` |
|        17 |  570 | `	}` |
|         - |  571 | `	/* Perform an int lookup */` |
|      3747 |  572 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     69654 |  573 | `result:` |
|    139313 |  574 | `	if( rc == SXRET_OK ){` |
|         - |  575 | `		/* Node found */` |
|     62033 |  576 | `		if( ppNode ){` |
|     61983 |  577 | `			*ppNode = pNode;` |
|     30989 |  578 | `		}` |
|     62033 |  579 | `		return SXRET_OK;` |
|         - |  580 | `	}` |
|         - |  581 | `	/* No such entry */` |
|     77285 |  582 | `	return SXERR_NOTFOUND;` |
|     69659 |  583 | `}` |
|         - |  584 | `/*` |
|         - |  585 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  586 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  587 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  588 | ` * via HashmapAppendIndexBusy.` |
|         - |  589 | ` */` |
|   2141396 |  590 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  591 | `{` |
|   2141401 |  592 | `	if( iKey >= pMap->iNextIdx ){` |
|   2141137 |  593 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  594 | `		/* Make sure the automatic index is not reserved */` |
|   2141137 |  595 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  596 | `			pMap->iNextIdx++;` |
|       ! 0 |  597 | `		}` |
|   1070566 |  598 | `	}` |
|   2141401 |  599 | `}` |
|         - |  600 | `/*` |
|         - |  601 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  602 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  603 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  604 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  605 | ` */` |
|   1010256 |  606 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  607 | `{` |
|   1010261 |  608 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  609 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  610 | `		return TRUE;` |
|         - |  611 | `	}` |
|   1010255 |  612 | `	return FALSE;` |
|    505133 |  613 | `}` |
|         - |  614 | `/*` |
|         - |  615 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  616 | ` * hashmap.` |
|         - |  617 | ` * If a node with the given key already exists in the database` |
|         - |  618 | ` * then this function overwrite the old value.` |
|         - |  619 | ` */` |
|   3375564 |  620 | `static sxi32 HashmapInsert(` |
|         - |  621 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  622 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  623 | `	ph7_value *pVal    /* Node value */` |
|         - |  624 | `	)` |
|         5 |  625 | `{` |
|   3375569 |  626 | `	ph7_hashmap_node *pNode = 0;` |
|   3375569 |  627 | `	sxi32 rc = SXRET_OK;` |
|   3375569 |  628 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    227597 |  629 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  630 | `			/* Force a string cast */` |
|         3 |  631 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  632 | `		}` |
|    227597 |  633 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|      3737 |  634 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  635 | `				/* Automatic index assign */` |
|      3509 |  636 | `				pKey = 0;` |
|      1752 |  637 | `			}` |
|      3737 |  638 | `			goto IntKey;` |
|         - |  639 | `		}` |
|    335795 |  640 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    111930 |  641 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|    223431 |  655 | `		if( pMap == pMap->pVm->pGlobal ){` |
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
|    223301 |  668 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    223301 |  669 | `		return rc;` |
|         - |  670 | `	}` |
|   1573986 |  671 | `IntKey:` |
|   3151709 |  672 | `	if( pKey ){` |
|   2141483 |  673 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  674 | `			/* Force an integer cast */` |
|       261 |  675 | `			PH7_MemObjToInteger(pKey);` |
|       130 |  676 | `		}` |
|   2141483 |  677 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  678 | `			/* Overwrite the old value */` |
|         - |  679 | `			ph7_value *pElem;` |
|        87 |  680 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|        87 |  681 | `			if( pElem ){` |
|        87 |  682 | `				if( pVal ){` |
|        87 |  683 | `					PH7_MemObjStore(pVal,pElem);` |
|        44 |  684 | `				}else{` |
|         - |  685 | `					/* Nullify the entry */` |
|       ! 0 |  686 | `					PH7_MemObjToNull(pElem);` |
|         - |  687 | `				}` |
|        43 |  688 | `			}` |
|        87 |  689 | `			return SXRET_OK;` |
|         - |  690 | `		}` |
|   2141397 |  691 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  692 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  693 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  694 | `			char zKey[24];` |
|         3 |  695 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  696 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  697 | `		}` |
|         - |  698 | `		/* Perform a 64-bit-int-key insertion */` |
|   2141395 |  699 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2141395 |  700 | `		if( rc == SXRET_OK ){` |
|   2141395 |  701 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1070695 |  702 | `		}` |
|   1070700 |  703 | `	}else{` |
|   1010231 |  704 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  705 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  706 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  707 | `		}` |
|   1010229 |  708 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  709 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  710 | `		}` |
|         - |  711 | `		/* Assign an automatic index */` |
|   1010223 |  712 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1010223 |  713 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1010221 |  714 | `			++pMap->iNextIdx;` |
|    505108 |  715 | `		}` |
|         - |  716 | `	}` |
|         - |  717 | `	/* Insertion result */` |
|   3151613 |  718 | `	return rc;` |
|   1687787 |  719 | `}` |
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
|     47202 |  747 | `static sxi32 HashmapInsertByRef(` |
|         - |  748 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  749 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  750 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  751 | `	)` |
|         5 |  752 | `{` |
|     47207 |  753 | `	ph7_hashmap_node *pNode = 0;` |
|     47207 |  754 | `	sxi32 rc = SXRET_OK;` |
|     47207 |  755 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     47171 |  756 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  757 | `			/* Force a string cast */` |
|       ! 0 |  758 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  759 | `		}` |
|     47171 |  760 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  761 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  762 | `				/* Automatic index assign */` |
|       ! 0 |  763 | `				pKey = 0;` |
|       ! 0 |  764 | `			}` |
|         3 |  765 | `			goto IntKey;` |
|         - |  766 | `		}` |
|     70751 |  767 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     23582 |  768 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  769 | `				/* Overwrite */` |
|        11 |  770 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  771 | `				pNode->nValIdx = nRefIdx;` |
|         - |  772 | `				/* Install in the reference table */` |
|        11 |  773 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  774 | `				return SXRET_OK;` |
|         - |  775 | `		}` |
|         - |  776 | `		/* Perform a blob-key insertion */` |
|     47159 |  777 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     47159 |  778 | `		return rc;` |
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
|     23606 |  811 | `}` |
|         - |  812 | `/*` |
|         - |  813 | ` * Extract node value.` |
|         - |  814 | ` */` |
|   1433775 |  815 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  816 | `{` |
|         - |  817 | `	/* Point to the desired object */` |
|         - |  818 | `	ph7_value *pObj;` |
|   1433780 |  819 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1433780 |  820 | `	return pObj;` |
|         5 |  821 | `}` |
|         - |  822 | `/*` |
|         - |  823 | ` * Insert a node in the given hashmap.` |
|         - |  824 | ` * If a node with the given key already exists in the database` |
|         - |  825 | ` * then this function overwrite the old value.` |
|         - |  826 | ` */` |
|       448 |  827 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  828 | `{` |
|         - |  829 | `	ph7_value *pObj;` |
|         - |  830 | `	sxi32 rc;` |
|         - |  831 | `	/* Extract the node value */` |
|       453 |  832 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|       453 |  833 | `	if( pObj == 0 ){` |
|       ! 0 |  834 | `		return SXERR_EMPTY;` |
|         - |  835 | `	}` |
|         - |  836 | `	/* Preserve key */` |
|       453 |  837 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  838 | `		/* Int64 key */` |
|       321 |  839 | `		if( !bPreserve ){` |
|         - |  840 | `			/* Assign an automatic index */` |
|       173 |  841 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        89 |  842 | `		}else{` |
|       149 |  843 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  844 | `		}` |
|       163 |  845 | `	}else{` |
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
|       453 |  856 | `	return rc;` |
|       229 |  857 | `}` |
|         - |  858 | `/*` |
|         - |  859 | ` * Compare two node values.` |
|         - |  860 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  861 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  862 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  863 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  864 | ` * documenation.` |
|         - |  865 | ` */` |
|     71479 |  866 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  867 | `{` |
|         - |  868 | `	ph7_value sObj1,sObj2;` |
|         - |  869 | `	sxi32 rc;` |
|     71484 |  870 | `	if( pLeft == pRight ){` |
|         - |  871 | `		/*` |
|         - |  872 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  873 | `		 * below for more information on this sceanario.` |
|         - |  874 | `		 */` |
|       ! 0 |  875 | `		return 0;` |
|         - |  876 | `	}` |
|         - |  877 | `	/* Do the comparison */` |
|     71484 |  878 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     71484 |  879 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     71484 |  880 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     71484 |  881 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     71484 |  882 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     71484 |  883 | `	PH7_MemObjRelease(&sObj1);` |
|     71484 |  884 | `	PH7_MemObjRelease(&sObj2);` |
|     71484 |  885 | `	return rc;` |
|     35771 |  886 | `}` |
|         - |  887 | `/*` |
|         - |  888 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  889 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  890 | ` */` |
|     13756 |  891 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  892 | `{` |
|     13761 |  893 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  894 | `	sxu32 nBucket;` |
|         - |  895 | `	/* Remove old collision links */` |
|     13761 |  896 | `	if( pEntry->pPrevCollide ){` |
|     11280 |  897 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5656 |  898 | `	}else{` |
|      2486 |  899 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  900 | `	}` |
|     13761 |  901 | `	if( pEntry->pNextCollide ){` |
|      1123 |  902 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       578 |  903 | `	}` |
|     13761 |  904 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  905 | `	/* Compute the new hash */` |
|     13761 |  906 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     13761 |  907 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     13761 |  908 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  909 | `	/* Link to the new bucket */` |
|     13761 |  910 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13761 |  911 | `	if( pMap->apBucket[nBucket] ){` |
|     11607 |  912 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5815 |  913 | `	}` |
|     13761 |  914 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13761 |  915 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  916 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  917 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  918 | `	 * the no-overflow invariant uniform). */` |
|     13761 |  919 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     13761 |  920 | `		pMap->iNextIdx++;` |
|      6878 |  921 | `	}` |
|     13761 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Perform a linear search on a given hashmap.` |
|         - |  925 | ` * Write a pointer to the target node on success.` |
|         - |  926 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  927 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  928 | ` * for more information.` |
|         - |  929 | ` */` |
|     33334 |  930 | `static int HashmapFindValue(` |
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
|     33339 |  943 | `	pEntry = pMap->pFirst;` |
|     33339 |  944 | `	n = pMap->nEntry;` |
|     33339 |  945 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33339 |  946 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     79123 |  947 | `	for(;;){` |
|    158248 |  948 | `		if( n < 1 ){` |
|       115 |  949 | `			break;` |
|         - |  950 | `		}` |
|         - |  951 | `		/* Extract node value */` |
|    158134 |  952 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    158134 |  953 | `		if( pVal ){` |
|         - |  954 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  955 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  956 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  957 | `			 * so null needles/values take the same path as everything else` |
|         - |  958 | `			 * (the historical null-to-null shortcut here made` |
|         - |  959 | `			 * in_array(null, [""]) false where php says true). */` |
|    158134 |  960 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    158134 |  961 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    158134 |  962 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    158134 |  963 | `			PH7_MemObjRelease(&sVal);` |
|    158134 |  964 | `			PH7_MemObjRelease(&sNeedle);` |
|    158134 |  965 | `			if( rc == 0 ){` |
|     33225 |  966 | `				if( ppNode ){` |
|        23 |  967 | `					*ppNode = pEntry;` |
|        11 |  968 | `				}` |
|         - |  969 | `				/* Match found*/` |
|     33225 |  970 | `				return SXRET_OK;` |
|         - |  971 | `			}` |
|     62456 |  972 | `		}` |
|         - |  973 | `		/* Point to the next entry */` |
|    124914 |  974 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    124914 |  975 | `		n--;` |
|         5 |  976 | `	}` |
|         - |  977 | `	/* No such entry */` |
|       115 |  978 | `	return SXERR_NOTFOUND;` |
|     16672 |  979 | `}` |
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
|    656794 | 1165 | `static sxi32 HashmapDuplicateNode(` |
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
|    656799 | 1176 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
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
|    656793 | 1201 | `	sSafeVal = *pVal;` |
|         - | 1202 |  |
|    656793 | 1203 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1204 | `		/* Blob key insertion */` |
|      4063 | 1205 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4063 | 1206 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4063 | 1207 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4063 | 1208 | `		PH7_MemObjRelease(&sKey);` |
|      2034 | 1209 | `	}else{` |
|         - | 1210 | `		/* Int key */` |
|    652735 | 1211 | `		if( iAction == 0 ){ /* Merge */` |
|    652503 | 1212 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    326484 | 1213 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1214 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1215 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1216 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1217 | `		}else{ /* Dup */` |
|       205 | 1218 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1219 | `		}` |
|         - | 1220 | `	}` |
|    656793 | 1221 | `	return rc;` |
|    328402 | 1222 | `}` |
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
|      2748 | 1235 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1236 | `{` |
|         - | 1237 | `	ph7_hashmap_node *pEntry;` |
|         - | 1238 | `	ph7_value *pVal;` |
|         - | 1239 | `	sxi32 rc;` |
|         - | 1240 | `	sxu32 n;` |
|      2753 | 1241 | `	if( pSrc == pDest ){` |
|         - | 1242 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1243 | `		 * Unlike the zend engine.` |
|         - | 1244 | `		 */` |
|       ! 0 | 1245 | `		return SXRET_OK;` |
|         - | 1246 | `	}` |
|         - | 1247 | `	/* Point to the first inserted entry in the source */` |
|      2753 | 1248 | `	pEntry = pSrc->pFirst;` |
|         - | 1249 | `	/* Perform the merge */` |
|    655309 | 1250 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1251 | `		/* Extract the node value */` |
|    652561 | 1252 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    652561 | 1253 | `		if( pVal ){` |
|         - | 1254 | `			/* Make a local copy of the value.` |
|         - | 1255 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1256 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1257 | `			 * to the old pool.` |
|         - | 1258 | `			 */` |
|    652561 | 1259 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    326283 | 1260 | `		}else{` |
|       ! 0 | 1261 | `			rc = SXRET_OK;` |
|         - | 1262 | `		}` |
|    652561 | 1263 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1264 | `			return rc;` |
|         - | 1265 | `		}` |
|         - | 1266 | `		/* Point to the next entry */` |
|    652561 | 1267 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    326283 | 1268 | `	}` |
|      2753 | 1269 | `	return SXRET_OK;` |
|      1379 | 1270 | `}` |
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
|      3958 | 1320 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1321 | `{` |
|         - | 1322 | `	ph7_hashmap_node *pEntry;` |
|         - | 1323 | `	ph7_value *pVal;` |
|         - | 1324 | `	sxi32 rc;` |
|         - | 1325 | `	sxu32 n;` |
|      3963 | 1326 | `	if( pSrc == pDest ){` |
|         - | 1327 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1328 | `		 * Unlike the zend engine.` |
|         - | 1329 | `		 */` |
|       ! 0 | 1330 | `		return SXRET_OK;` |
|         - | 1331 | `	}` |
|         - | 1332 | `	/* Point to the first inserted entry in the source */` |
|      3963 | 1333 | `	pEntry = pSrc->pFirst;` |
|         - | 1334 | `	/* Perform the duplication */` |
|      8157 | 1335 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1336 | `		/* Extract the node value */` |
|      4199 | 1337 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4199 | 1338 | `		if( pVal ){` |
|      4199 | 1339 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2102 | 1340 | `		}else{` |
|       ! 0 | 1341 | `			rc = SXRET_OK;` |
|         - | 1342 | `		}` |
|      4199 | 1343 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1344 | `			return rc;` |
|         - | 1345 | `		}` |
|         - | 1346 | `		/* Point to the next entry */` |
|      4199 | 1347 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2102 | 1348 | `	}` |
|      3963 | 1349 | `	return SXRET_OK;` |
|      1984 | 1350 | `}` |
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
|    231336 | 1426 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1427 | `{` |
|    231341 | 1428 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1429 | `	ph7_hashmap *pNew;` |
|         - | 1430 | `	ph7_value *pBacking;` |
|         - | 1431 | `	sxu32 nValIdx;` |
|         - | 1432 | `	int bValueInPool;` |
|    231341 | 1433 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    231341 | 1434 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1435 | `		/* Sole owner, no separation needed */` |
|    228913 | 1436 | `		return pMap;` |
|         - | 1437 | `	}` |
|      2433 | 1438 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1439 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1440 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1441 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1442 | `		return pMap;` |
|         - | 1443 | `	}` |
|         - | 1444 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1445 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1446 | `	 * frame is popped. */` |
|      2307 | 1447 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2307 | 1448 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2302 | 1449 | `		if( pBacking && pBacking != pValue` |
|      2278 | 1450 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2259 | 1451 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1452 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2259 | 1453 | `			pMap->iRef--;` |
|      2259 | 1454 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1455 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2217 | 1456 | `				pMap->iRef++;` |
|      2217 | 1457 | `				return pMap;` |
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
|    115673 | 1520 | `}` |
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
|    144368 | 1612 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1613 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1614 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1615 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1616 | `	)` |
|         5 | 1617 | `{` |
|         - | 1618 | `	ph7_hashmap *pMap;` |
|         - | 1619 | `	/* Allocate a new instance */` |
|    144373 | 1620 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    144373 | 1621 | `	if( pMap == 0 ){` |
|       ! 0 | 1622 | `		return 0;` |
|         - | 1623 | `	}` |
|         - | 1624 | `	/* Zero the structure */` |
|    144373 | 1625 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1626 | `	/* Fill in the structure */` |
|    144373 | 1627 | `	pMap->pVm = &(*pVm);` |
|    144373 | 1628 | `	pMap->iRef = 1;` |
|         - | 1629 | `	/* Default hash functions */` |
|    144373 | 1630 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    144373 | 1631 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    144373 | 1632 | `	return pMap;` |
|     72189 | 1633 | `}` |
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
|    101142 | 1725 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1726 | `{` |
|         - | 1727 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    101147 | 1728 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1729 | `	sxu32 n;` |
|    101147 | 1730 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1731 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1732 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1733 | `		return SXRET_OK;` |
|         - | 1734 | `	}` |
|    101147 | 1735 | `	if( pMap->pActiveSteps ){` |
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
|    101147 | 1748 | `	n = 0;` |
|    101147 | 1749 | `	pEntry = pMap->pFirst;` |
|   1718741 | 1750 | `	for(;;){` |
|   3437487 | 1751 | `		if( n >= pMap->nEntry ){` |
|    101147 | 1752 | `			break;` |
|         - | 1753 | `		}` |
|   3336345 | 1754 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1755 | `		/* Remove the reference from the foreign table */` |
|   3336345 | 1756 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3336345 | 1757 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1758 | `			/* Restore the ph7_value to the free list */` |
|   3336335 | 1759 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1668165 | 1760 | `		}` |
|         - | 1761 | `		/* Release the node */` |
|   3336345 | 1762 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    196033 | 1763 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     98014 | 1764 | `		}` |
|   3336345 | 1765 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1766 | `		/* Point to the next entry */` |
|   3336345 | 1767 | `		pEntry = pNext;` |
|   3336345 | 1768 | `		n++;` |
|         5 | 1769 | `	}` |
|    101147 | 1770 | `	if( pMap->nEntry > 0 ){` |
|         - | 1771 | `		/* Release the hash bucket */` |
|     76361 | 1772 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     38178 | 1773 | `	}` |
|    101147 | 1774 | `	if( FreeDS ){` |
|         - | 1775 | `		/* Free the whole instance */` |
|    101123 | 1776 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     50564 | 1777 | `	}else{` |
|         - | 1778 | `		/* Keep the instance but reset it's fields */` |
|        26 | 1779 | `		pMap->apBucket = 0;` |
|        26 | 1780 | `		pMap->iNextIdx = 0;` |
|        26 | 1781 | `		pMap->nEntry = pMap->nSize = 0;` |
|        26 | 1782 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1783 | `	}` |
|    101147 | 1784 | `	return SXRET_OK;` |
|     50576 | 1785 | `}` |
|         - | 1786 | `/*` |
|         - | 1787 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1788 | ` * If the count reaches zero which mean no more variables` |
|         - | 1789 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1790 | ` */` |
|    833436 | 1791 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1792 | `{` |
|    833441 | 1793 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1794 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    833441 | 1795 | `	pMap->iRef--;` |
|    833441 | 1796 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    101103 | 1797 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     50549 | 1798 | `	}` |
|    833441 | 1799 | `}` |
|         - | 1800 | `/*` |
|         - | 1801 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1802 | ` * Write a pointer to the target node on success.` |
|         - | 1803 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1804 | ` */` |
|    139460 | 1805 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1806 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1807 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1808 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1809 | `	)` |
|         5 | 1810 | `{` |
|         - | 1811 | `	sxi32 rc;` |
|    139465 | 1812 | `	if( pMap->nEntry < 1 ){` |
|         - | 1813 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1814 | `		 */` |
|       157 | 1815 | `		return SXERR_NOTFOUND;` |
|         - | 1816 | `	}` |
|    139313 | 1817 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    139313 | 1818 | `	return rc;` |
|     69735 | 1819 | `}` |
|         - | 1820 | `/*` |
|         - | 1821 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1822 | ` * hashmap.` |
|         - | 1823 | ` * If a node with the given key already exists in the database` |
|         - | 1824 | ` * then this function overwrite the old value.` |
|         - | 1825 | ` */` |
|   2722834 | 1826 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   2722839 | 1837 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2722839 | 1838 | `	return rc;` |
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
|     47196 | 1877 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1878 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1879 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1880 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1881 | `	)` |
|         5 | 1882 | `{` |
|         - | 1883 | `	sxi32 rc;` |
|     47201 | 1884 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1885 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1886 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1887 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1888 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1889 | `		return PH7_ABORT;` |
|         - | 1890 | `	}` |
|     47201 | 1891 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     47201 | 1892 | `	return rc;` |
|     23603 | 1893 | `}` |
|         - | 1894 | `/*` |
|         - | 1895 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1896 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1897 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1898 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1899 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1900 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1901 | ` */` |
|     18810 | 1902 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1903 | `{` |
|     18815 | 1904 | `	pStep->pCursor = pMap->pFirst;` |
|     18815 | 1905 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     18815 | 1906 | `	pMap->pActiveSteps = pStep;` |
|     18815 | 1907 | `}` |
|         - | 1908 | `/*` |
|         - | 1909 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1910 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1911 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1912 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1913 | ` */` |
|     18712 | 1914 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1915 | `{` |
|     18717 | 1916 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     18717 | 1917 | `	while( *ppLink ){` |
|     18717 | 1918 | `		if( *ppLink == pStep ){` |
|     18717 | 1919 | `			*ppLink = pStep->pNextActive;` |
|     18717 | 1920 | `			pStep->pNextActive = 0;` |
|     18717 | 1921 | `			return;` |
|         - | 1922 | `		}` |
|       ! 0 | 1923 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1924 | `	}` |
|      9361 | 1925 | `}` |
|         - | 1926 | `/*` |
|         - | 1927 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 1928 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 1929 | ` * return NULL.` |
|         - | 1930 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 1931 | ` */` |
|        50 | 1932 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         1 | 1933 | `{` |
|        51 | 1934 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|        51 | 1935 | `	if( pCur == 0 ){` |
|         - | 1936 | `		/* End of the list,return null */` |
|        21 | 1937 | `		return 0;` |
|         - | 1938 | `	}` |
|         - | 1939 | `	/* Advance the node cursor */` |
|        31 | 1940 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        31 | 1941 | `	return pCur;` |
|        26 | 1942 | `}` |
|         - | 1943 | `/*` |
|         - | 1944 | ` * Extract a node value.` |
|         - | 1945 | ` */` |
|    585730 | 1946 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1947 | `{` |
|    585735 | 1948 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    585735 | 1949 | `	if( pEntry ){` |
|    585735 | 1950 | `		if( bStore ){` |
|    232357 | 1951 | `			PH7_MemObjStore(pEntry,pValue);` |
|    116181 | 1952 | `		}else{` |
|    353383 | 1953 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1954 | `		}` |
|    292923 | 1955 | `	}else{` |
|       ! 0 | 1956 | `		PH7_MemObjRelease(pValue);` |
|         - | 1957 | `	}` |
|    585735 | 1958 | `}` |
|         - | 1959 | `/*` |
|         - | 1960 | ` * Extract a node key.` |
|         - | 1961 | ` */` |
|    153782 | 1962 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 1963 | `{` |
|         - | 1964 | `	/* Fill with the current key */` |
|    153787 | 1965 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    148829 | 1966 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 1967 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 1968 | `		}` |
|    148829 | 1969 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    148829 | 1970 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     74417 | 1971 | `	}else{` |
|      4963 | 1972 | `		SyBlobReset(&pKey->sBlob);` |
|      4963 | 1973 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      4963 | 1974 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 1975 | `	}` |
|    153787 | 1976 | `}` |
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
|     35630 | 2027 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2028 | `{` |
|         - | 2029 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2030 | `    /* Prevent compiler warning */` |
|     35635 | 2031 | `	result.pNext = result.pPrev = 0;` |
|     35635 | 2032 | `	pTail = &result;` |
|    107257 | 2033 | `	while( pA && pB ){` |
|     71627 | 2034 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47354 | 2035 | `			pTail->pPrev = pA;` |
|     47354 | 2036 | `			pA->pNext = pTail;` |
|     47354 | 2037 | `			pTail = pA;` |
|     47354 | 2038 | `			pA = pA->pPrev;` |
|     23683 | 2039 | `		}else{` |
|     24278 | 2040 | `			pTail->pPrev = pB;` |
|     24278 | 2041 | `			pB->pNext = pTail;` |
|     24278 | 2042 | `			pTail = pB;` |
|     24278 | 2043 | `			pB = pB->pPrev;` |
|         - | 2044 | `		}` |
|         5 | 2045 | `	}` |
|     35635 | 2046 | `	if( pA ){` |
|     25150 | 2047 | `		pTail->pPrev = pA;` |
|     25150 | 2048 | `		pA->pNext = pTail;` |
|     23085 | 2049 | `	}else if( pB ){` |
|     10260 | 2050 | `		pTail->pPrev = pB;` |
|     10260 | 2051 | `		pB->pNext = pTail;` |
|      5110 | 2052 | `	}else{` |
|       235 | 2053 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2054 | `	}` |
|     35635 | 2055 | `	return result.pPrev;` |
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
|     14681 | 2076 | `	while( pIn ){` |
|     13939 | 2077 | `		p = pIn;` |
|     13939 | 2078 | `		pIn = p->pPrev;` |
|     13939 | 2079 | `		p->pPrev = 0;` |
|     26567 | 2080 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26567 | 2081 | `			if( a[i]==0 ){` |
|     13939 | 2082 | `				a[i] = p;` |
|     13939 | 2083 | `				break;` |
|       ! 0 | 2084 | `			}else{` |
|     12633 | 2085 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12633 | 2086 | `				a[i] = 0;` |
|         - | 2087 | `			}` |
|      6319 | 2088 | `		}` |
|     13939 | 2089 | `		if( i==N_SORT_BUCKET-1 ){` |
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
|     71349 | 2112 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2113 | `{` |
|         - | 2114 | `	ph7_value sA,sB;` |
|         - | 2115 | `	sxi32 iFlags;` |
|         - | 2116 | `	int rc;` |
|     71354 | 2117 | `	if( pCmpData == 0 ){` |
|         - | 2118 | `		/* Perform a standard comparison */` |
|     71330 | 2119 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     71330 | 2120 | `		return rc;` |
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
|     35706 | 2158 | `}` |
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
|      7188 | 2415 | `	for( ;; ){` |
|     14381 | 2416 | `		if( i >= pMap->nEntry ){` |
|       679 | 2417 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       679 | 2418 | `			break;` |
|         - | 2419 | `		}` |
|     13707 | 2420 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2421 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2422 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2423 | `			/* Change key type */` |
|         5 | 2424 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2425 | `		}` |
|     13707 | 2426 | `		HashmapRehashIntNode(p);` |
|         - | 2427 | `		/* Point to the next entry */` |
|     13707 | 2428 | `		i++;` |
|     13707 | 2429 | `		pLast = p;` |
|     13707 | 2430 | `		p = p->pPrev; /* Reverse link */` |
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
|      1282 | 2944 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2945 | `{` |
|      1287 | 2946 | `	int bRecursive = FALSE;` |
|      1287 | 2947 | `	int bCycleDetected = FALSE;` |
|         - | 2948 | `	sxi64 iCount;` |
|      1287 | 2949 | `	if( nArg < 1 ){` |
|         3 | 2950 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2951 | `			"ArgumentCountError",` |
|         - | 2952 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2953 | `			);` |
|         - | 2954 | `	}` |
|      1285 | 2955 | `	if( nArg > 2 ){` |
|         4 | 2956 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2957 | `			"ArgumentCountError",` |
|         - | 2958 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 2959 | `			nArg` |
|         - | 2960 | `			);` |
|         - | 2961 | `	}` |
|         - | 2962 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 2963 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 2964 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1283 | 2965 | `	if( nArg > 1 ){` |
|        45 | 2966 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        45 | 2967 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        12 | 2968 | `			return PH7_VmThrowException(pCtx,` |
|         - | 2969 | `				"ValueError",` |
|         - | 2970 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 2971 | `				);` |
|         - | 2972 | `		}` |
|        34 | 2973 | `		bRecursive = iMode == 1;` |
|        16 | 2974 | `	}` |
|      1275 | 2975 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 2976 | `		/* Countable object: dispatch to ->count() */` |
|        41 | 2977 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        29 | 2978 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        29 | 2979 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        29 | 2980 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        26 | 2981 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 2982 | `					"count",sizeof("count")-1);` |
|        26 | 2983 | `				if( pMeth ){` |
|         - | 2984 | `					ph7_value sResult;` |
|        26 | 2985 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        26 | 2986 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        26 | 2987 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        26 | 2988 | `					PH7_MemObjRelease(&sResult);` |
|        26 | 2989 | `					return PH7_OK;` |
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
|      1239 | 3000 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1239 | 3001 | `	if( bCycleDetected ){` |
|         3 | 3002 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3003 | `	}` |
|      1239 | 3004 | `	ph7_result_int64(pCtx,iCount);` |
|      1239 | 3005 | `	return PH7_OK;` |
|       646 | 3006 | `}` |
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
|        18 | 3067 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3068 | `{` |
|         - | 3069 | `	ph7_hashmap *pMap;` |
|         - | 3070 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        23 | 3071 | `	if( nArg != 1 ){` |
|         8 | 3072 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3073 | `			"ArgumentCountError",` |
|         - | 3074 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         2 | 3075 | `			nArg` |
|         - | 3076 | `			);` |
|         - | 3077 | `	}` |
|         - | 3078 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3079 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        18 | 3080 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3081 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3082 | `			"Error",` |
|         - | 3083 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3084 | `			);` |
|         - | 3085 | `	}` |
|         - | 3086 | `	/* Make sure we are dealing with a valid hashmap */` |
|        12 | 3087 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3088 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3089 | `			"TypeError",` |
|         - | 3090 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3091 | `			ph7_type_name(apArg[0])` |
|         - | 3092 | `			);` |
|         - | 3093 | `	}` |
|         9 | 3094 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         9 | 3095 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         9 | 3096 | `	if( pMap->nEntry < 1 ){` |
|         - | 3097 | `		/* Nothing to pop,return NULL */` |
|         3 | 3098 | `		ph7_result_null(pCtx);` |
|         2 | 3099 | `	}else{` |
|         7 | 3100 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3101 | `		ph7_value *pObj;` |
|         7 | 3102 | `		pObj = HashmapExtractNodeValue(pLast);` |
|         7 | 3103 | `		if( pObj ){` |
|         - | 3104 | `			/* Node value */` |
|         7 | 3105 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3106 | `			/* Unlink the node */` |
|         7 | 3107 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|         4 | 3108 | `		}else{` |
|       ! 0 | 3109 | `			ph7_result_null(pCtx);` |
|         - | 3110 | `		}` |
|         - | 3111 | `		/* Reset the cursor */` |
|         7 | 3112 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3113 | `	}` |
|         9 | 3114 | `	return PH7_OK;` |
|        14 | 3115 | `}` |
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
|        38 | 3181 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3182 | `{` |
|         - | 3183 | `	ph7_hashmap *pMap;` |
|         - | 3184 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        43 | 3185 | `	if( nArg != 1 ){` |
|         8 | 3186 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3187 | `			"ArgumentCountError",` |
|         - | 3188 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         2 | 3189 | `			nArg` |
|         - | 3190 | `			);` |
|         - | 3191 | `	}` |
|         - | 3192 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        39 | 3193 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3194 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3195 | `			"Error",` |
|         - | 3196 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3197 | `			);` |
|         - | 3198 | `	}` |
|         - | 3199 | `	/* Make sure we are dealing with a valid hashmap */` |
|        35 | 3200 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3201 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3202 | `			"TypeError",` |
|         - | 3203 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3204 | `			ph7_type_name(apArg[0])` |
|         - | 3205 | `			);` |
|         - | 3206 | `	}` |
|         - | 3207 | `	/* Point to the internal representation of the hashmap */` |
|        33 | 3208 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        33 | 3209 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        33 | 3210 | `	if( pMap->nEntry < 1 ){` |
|         - | 3211 | `		/* Empty hashmap,return NULL */` |
|         3 | 3212 | `		ph7_result_null(pCtx);` |
|         2 | 3213 | `	}else{` |
|        31 | 3214 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3215 | `		ph7_value *pObj;` |
|         - | 3216 | `		sxu32 n;` |
|        31 | 3217 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        31 | 3218 | `		if( pObj ){` |
|         - | 3219 | `			/* Node value */` |
|        31 | 3220 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3221 | `			/* Unlink the first node */` |
|        31 | 3222 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        18 | 3223 | `		}else{` |
|       ! 0 | 3224 | `			ph7_result_null(pCtx);` |
|         - | 3225 | `		}` |
|         - | 3226 | `		/* Rehash all int keys */` |
|        31 | 3227 | `		n = pMap->nEntry;` |
|        31 | 3228 | `		pEntry = pMap->pFirst;` |
|        31 | 3229 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|        40 | 3230 | `		for(;;){` |
|        85 | 3231 | `			if( n < 1 ){` |
|        31 | 3232 | `				break;` |
|         - | 3233 | `			}` |
|        59 | 3234 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        59 | 3235 | `				HashmapRehashIntNode(pEntry);` |
|        27 | 3236 | `			}` |
|         - | 3237 | `			/* Point to the next entry */` |
|        59 | 3238 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        59 | 3239 | `			n--;` |
|         5 | 3240 | `		}` |
|         - | 3241 | `		/* Reset the cursor */` |
|        31 | 3242 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3243 | `	}` |
|        33 | 3244 | `	return PH7_OK;` |
|        24 | 3245 | `}` |
|         - | 3246 | `/*` |
|         - | 3247 | ` * Extract the node cursor value.` |
|         - | 3248 | ` */` |
|       110 | 3249 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3250 | `{` |
|       111 | 3251 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3252 | `	ph7_value *pVal;` |
|       111 | 3253 | `	if( pCur == 0 ){` |
|         - | 3254 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3255 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3256 | `		return PH7_OK;` |
|         - | 3257 | `	}` |
|       103 | 3258 | `	if( iDirection != 0 ){` |
|        35 | 3259 | `		if( iDirection > 0 ){` |
|         - | 3260 | `			/* Point to the next entry */` |
|        33 | 3261 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 3262 | `			pCur = pMap->pCur;` |
|        17 | 3263 | `		}else{` |
|         - | 3264 | `			/* Point to the previous entry */` |
|         3 | 3265 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3266 | `			pCur = pMap->pCur;` |
|         - | 3267 | `		}` |
|        35 | 3268 | `		if( pCur == 0 ){` |
|         - | 3269 | `			/* End of input reached,return FALSE */` |
|         9 | 3270 | `			ph7_result_bool(pCtx,0);` |
|         9 | 3271 | `			return PH7_OK;` |
|         - | 3272 | `		}` |
|        13 | 3273 | `	}` |
|         - | 3274 | `	/* Point to the desired element */` |
|        95 | 3275 | `	pVal = HashmapExtractNodeValue(pCur);` |
|        95 | 3276 | `	if( pVal ){` |
|        95 | 3277 | `		ph7_result_value(pCtx,pVal);` |
|        48 | 3278 | `	}else{` |
|       ! 0 | 3279 | `		ph7_result_bool(pCtx,0);` |
|         - | 3280 | `	}` |
|        95 | 3281 | `	return PH7_OK;` |
|        56 | 3282 | `}` |
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
|        36 | 3294 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3295 | `{` |
|        37 | 3296 | `	if( nArg < 1 ){` |
|         - | 3297 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3298 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3299 | `		return PH7_OK;` |
|         - | 3300 | `	}` |
|         - | 3301 | `	/* Make sure we are dealing with a valid hashmap */` |
|        37 | 3302 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3303 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3304 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3305 | `		return PH7_OK;` |
|         - | 3306 | `	}` |
|        37 | 3307 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|        37 | 3308 | `	return PH7_OK;` |
|        19 | 3309 | `}` |
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
|        32 | 3320 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3321 | `{` |
|        33 | 3322 | `	if( nArg < 1 ){` |
|         - | 3323 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3324 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3325 | `		return PH7_OK;` |
|         - | 3326 | `	}` |
|         - | 3327 | `	/* Make sure we are dealing with a valid hashmap */` |
|        33 | 3328 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3329 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3330 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3331 | `		return PH7_OK;` |
|         - | 3332 | `	}` |
|        33 | 3333 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|        33 | 3334 | `	return PH7_OK;` |
|        17 | 3335 | `}` |
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
|        38 | 3400 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3401 | `{` |
|         - | 3402 | `	ph7_hashmap *pMap;` |
|        39 | 3403 | `	if( nArg < 1 ){` |
|         - | 3404 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3405 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3406 | `		return PH7_OK;` |
|         - | 3407 | `	}` |
|         - | 3408 | `	/* Make sure we are dealing with a valid hashmap */` |
|        39 | 3409 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3410 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3411 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3412 | `		return PH7_OK;` |
|         - | 3413 | `	}` |
|         - | 3414 | `	/* Point to the internal representation of the input hashmap */` |
|        39 | 3415 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3416 | `	/* Point to the first node */` |
|        39 | 3417 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3418 | `	/* Return the last node value if available */` |
|        39 | 3419 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|        39 | 3420 | `	return PH7_OK;` |
|        20 | 3421 | `}` |
|         - | 3422 | `/*` |
|         - | 3423 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3424 | ` * array_key_first() and array_key_last().` |
|         - | 3425 | ` */` |
|        72 | 3426 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3427 | `{` |
|        73 | 3428 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3429 | `		/* Key is integer */` |
|        41 | 3430 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|        21 | 3431 | `	}else{` |
|         - | 3432 | `		/* Key is blob */` |
|        49 | 3433 | `		ph7_result_string(pCtx,` |
|        32 | 3434 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3435 | `	}` |
|        73 | 3436 | `}` |
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
|        70 | 3449 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3450 | `{` |
|         - | 3451 | `	ph7_hashmap_node *pCur;` |
|         - | 3452 | `	ph7_hashmap *pMap;` |
|        71 | 3453 | `	if( nArg < 1 ){` |
|         - | 3454 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3455 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3456 | `		return PH7_OK;` |
|         - | 3457 | `	}` |
|         - | 3458 | `	/* Make sure we are dealing with a valid hashmap */` |
|        71 | 3459 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3460 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3461 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3462 | `		return PH7_OK;` |
|         - | 3463 | `	}` |
|        71 | 3464 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        71 | 3465 | `	pCur = pMap->pCur;` |
|        71 | 3466 | `	if( pCur == 0 ){` |
|         - | 3467 | `		/* Cursor does not point to anything,return NULL */` |
|        15 | 3468 | `		ph7_result_null(pCtx);` |
|        15 | 3469 | `		return PH7_OK;` |
|         - | 3470 | `	}` |
|        57 | 3471 | `	HashmapResultNodeKey(pCtx,pCur);` |
|        57 | 3472 | `	return PH7_OK;` |
|        36 | 3473 | `}` |
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
|        38 | 4140 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4141 | `{` |
|         - | 4142 | `	ph7_hashmap_node *pNode;` |
|         - | 4143 | `	ph7_hashmap *pMap;` |
|         - | 4144 | `	ph7_value *pArray;` |
|         - | 4145 | `	ph7_value *pObj;` |
|         - | 4146 | `	sxu32 n;` |
|        42 | 4147 | `	if( nArg != 1 ){` |
|         - | 4148 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         8 | 4149 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4150 | `			"ArgumentCountError",` |
|         - | 4151 | `			"array_values() expects exactly 1 argument, %d given",` |
|         2 | 4152 | `			nArg` |
|         - | 4153 | `			);` |
|         - | 4154 | `	}` |
|         - | 4155 | `	/* Make sure we are dealing with a valid hashmap */` |
|        37 | 4156 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4157 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4158 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4159 | `			"TypeError",` |
|         - | 4160 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4161 | `			ph7_type_name(apArg[0])` |
|         - | 4162 | `			);` |
|         - | 4163 | `	}` |
|         - | 4164 | `	/* Point to the internal representation that describe the input hashmap */` |
|        34 | 4165 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4166 | `	/* Create a new array */` |
|        34 | 4167 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 4168 | `	if( pArray == 0 ){` |
|       ! 0 | 4169 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4170 | `		return PH7_OK;` |
|         - | 4171 | `	}` |
|         - | 4172 | `	/* Perform the requested operation */` |
|        34 | 4173 | `	pNode = pMap->pFirst;` |
|       110 | 4174 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        78 | 4175 | `		pObj = HashmapExtractNodeValue(pNode);` |
|        78 | 4176 | `		if( pObj ){` |
|         - | 4177 | `			/* perform the insertion */` |
|        78 | 4178 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        38 | 4179 | `		}` |
|         - | 4180 | `		/* Point to the next entry */` |
|        78 | 4181 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        40 | 4182 | `	}` |
|         - | 4183 | `	/* return the new array */` |
|        34 | 4184 | `	ph7_result_value(pCtx,pArray);` |
|        34 | 4185 | `	return PH7_OK;` |
|        23 | 4186 | `}` |
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
|      1040 | 4329 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4330 | `{` |
|         - | 4331 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4332 | `	ph7_value *pArray;` |
|         - | 4333 | `	int i;` |
|         - | 4334 | `	/* Create a new array */` |
|      1045 | 4335 | `	pArray = ph7_context_new_array(pCtx);` |
|      1045 | 4336 | `	if( pArray == 0 ){` |
|       ! 0 | 4337 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4338 | `		return PH7_OK;` |
|         - | 4339 | `	}` |
|         - | 4340 | `	/* Point to the internal representation of the hashmap */` |
|      1045 | 4341 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4342 | `	/* Start merging */` |
|      3115 | 4343 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4344 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2079 | 4345 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4346 | `			/* Type mismatch -> TypeError */` |
|         8 | 4347 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4348 | `				"TypeError",` |
|         - | 4349 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4350 | `				i + 1,` |
|         4 | 4351 | `				ph7_type_name(apArg[i])` |
|         - | 4352 | `				);` |
|       ! 0 | 4353 | `		}else{` |
|      2075 | 4354 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4355 | `			/* Merge the two hashmaps */` |
|      2075 | 4356 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4357 | `		}` |
|      1040 | 4358 | `	}` |
|         - | 4359 | `	/* Return the freshly created array */` |
|      1041 | 4360 | `	ph7_result_value(pCtx,pArray);` |
|      1041 | 4361 | `	return PH7_OK;` |
|       525 | 4362 | `}` |
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
|        16 | 4374 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4375 | `{` |
|         - | 4376 | `	ph7_hashmap *pMap;` |
|         - | 4377 | `	ph7_value *pArray;` |
|        17 | 4378 | `	if( nArg < 1 ){` |
|         - | 4379 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4380 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4381 | `		return PH7_OK;` |
|         - | 4382 | `	}` |
|         - | 4383 | `	/* Create a new array */` |
|        17 | 4384 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 4385 | `	if( pArray == 0 ){` |
|       ! 0 | 4386 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4387 | `		return PH7_OK;` |
|         - | 4388 | `	}` |
|         - | 4389 | `	/* Point to the internal representation of the hashmap */` |
|        17 | 4390 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        17 | 4391 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4392 | `		/* Point to the internal representation of the source */` |
|        17 | 4393 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4394 | `		/* Perform the copy */` |
|        17 | 4395 | `		PH7_HashmapDup(pSrc,pMap);` |
|         9 | 4396 | `	}else{` |
|         - | 4397 | `		/* Simple insertion */` |
|       ! 0 | 4398 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4399 | `	}` |
|         - | 4400 | `	/* Return the duplicated array */` |
|        17 | 4401 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 4402 | `	return PH7_OK;` |
|         9 | 4403 | `}` |
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
|        24 | 4415 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4416 | `{` |
|         - | 4417 | `	ph7_hashmap *pMap;` |
|        26 | 4418 | `	if( nArg < 1 ){` |
|         - | 4419 | `		/* Missing arguments */` |
|       ! 0 | 4420 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4421 | `		return PH7_OK;` |
|         - | 4422 | `	}` |
|         - | 4423 | `	/* Point to the target hashmap */` |
|        26 | 4424 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        26 | 4425 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4426 | `	/* Erase */` |
|        26 | 4427 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        26 | 4428 | `	return PH7_OK;` |
|        14 | 4429 | `}` |
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
|        50 | 4450 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4451 | `{` |
|         - | 4452 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4453 | `	ph7_hashmap_node *pCur;` |
|         - | 4454 | `	ph7_value *pArray;` |
|         - | 4455 | `	int iLength,iOfft;` |
|         - | 4456 | `	int bPreserve;` |
|         - | 4457 | `	sxi32 rc;` |
|        55 | 4458 | `	if( nArg < 2 ){` |
|         8 | 4459 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4460 | `			"ArgumentCountError",` |
|         - | 4461 | `			"array_slice() expects at least 2 arguments, %d given",` |
|         2 | 4462 | `			nArg` |
|         - | 4463 | `			);` |
|         - | 4464 | `	}` |
|        51 | 4465 | `	if( nArg > 4 ){` |
|         4 | 4466 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4467 | `			"ArgumentCountError",` |
|         - | 4468 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4469 | `			nArg` |
|         - | 4470 | `			);` |
|         - | 4471 | `	}` |
|        49 | 4472 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4473 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4474 | `			"TypeError",` |
|         - | 4475 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4476 | `			ph7_type_name(apArg[0])` |
|         - | 4477 | `			);` |
|         - | 4478 | `	}` |
|         - | 4479 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        62 | 4480 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        65 | 4481 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4482 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4483 | `			"TypeError",` |
|         - | 4484 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4485 | `			ph7_type_name(apArg[1])` |
|         - | 4486 | `			);` |
|         - | 4487 | `	}` |
|         - | 4488 | `	/* Validate $length type if provided: nullable int */` |
|        45 | 4489 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        26 | 4490 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        26 | 4491 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4492 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4493 | `				"TypeError",` |
|         - | 4494 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4495 | `				ph7_type_name(apArg[2])` |
|         - | 4496 | `				);` |
|         - | 4497 | `		}` |
|         8 | 4498 | `	}` |
|         - | 4499 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        43 | 4500 | `	if( nArg > 3 ){` |
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
|        41 | 4511 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 4512 | `	bPreserve = FALSE;` |
|         - | 4513 | `	/* Get the offset */` |
|        41 | 4514 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        41 | 4515 | `	if( iOfft < 0 ){` |
|         5 | 4516 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4517 | `		if( iOfft < 0 ){` |
|         3 | 4518 | `			iOfft = 0;` |
|         1 | 4519 | `		}` |
|         2 | 4520 | `	}` |
|        41 | 4521 | `	if( iOfft >= (int)pSrc->nEntry ){` |
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
|        37 | 4532 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        37 | 4533 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        15 | 4534 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        15 | 4535 | `		if( iLength < 0 ){` |
|         5 | 4536 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4537 | `		}` |
|        15 | 4538 | `		if( iLength < 0 ){` |
|         3 | 4539 | `			iLength = 0;` |
|         1 | 4540 | `		}` |
|        15 | 4541 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4542 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4543 | `		}` |
|         7 | 4544 | `	}` |
|        37 | 4545 | `	if( nArg > 3 ){` |
|         5 | 4546 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4547 | `	}` |
|         - | 4548 | `	/* Create a new array */` |
|        37 | 4549 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 4550 | `	if( pArray == 0 ){` |
|       ! 0 | 4551 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4552 | `		return PH7_OK;` |
|         - | 4553 | `	}` |
|        37 | 4554 | `	if( iLength < 1 ){` |
|         - | 4555 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4556 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4557 | `		return PH7_OK;` |
|         - | 4558 | `	}` |
|         - | 4559 | `	/* Point to the desired entry */` |
|        33 | 4560 | `	pCur = pSrc->pFirst;` |
|        28 | 4561 | `	for(;;){` |
|        61 | 4562 | `		if( iOfft < 1 ){` |
|        33 | 4563 | `			break;` |
|         - | 4564 | `		}` |
|         - | 4565 | `		/* Point to the next entry */` |
|        33 | 4566 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4567 | `		iOfft--;` |
|         5 | 4568 | `	}` |
|         - | 4569 | `	/* Point to the internal representation of the hashmap */` |
|        33 | 4570 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        51 | 4571 | `	for(;;){` |
|       107 | 4572 | `		if( iLength < 1 ){` |
|        33 | 4573 | `			break;` |
|         - | 4574 | `		}` |
|         - | 4575 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4576 | `		{` |
|        79 | 4577 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        79 | 4578 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4579 | `		}` |
|        79 | 4580 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4581 | `			break;` |
|         - | 4582 | `		}` |
|         - | 4583 | `		/* Point to the next entry */` |
|        79 | 4584 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        79 | 4585 | `		iLength--;` |
|         5 | 4586 | `	}` |
|         - | 4587 | `	/* Return the freshly created array */` |
|        33 | 4588 | `	ph7_result_value(pCtx,pArray);` |
|        33 | 4589 | `	return PH7_OK;` |
|        30 | 4590 | `}` |
|         - | 4591 | `/*` |
|         - | 4592 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4593 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4594 | ` * beginning (becomes the new pFirst).` |
|         - | 4595 | ` */` |
|        30 | 4596 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4597 | `{` |
|         - | 4598 | `	ph7_hashmap_node *pNode;` |
|         - | 4599 | `	ph7_hashmap_node *pOldNext;` |
|        31 | 4600 | `	pNode = pMap->pLast;` |
|        31 | 4601 | `	if( pNode == 0 ){` |
|       ! 0 | 4602 | `		return;` |
|         - | 4603 | `	}` |
|        31 | 4604 | `	if( pNode->pNext == 0 ){` |
|         - | 4605 | `		/* Only node in the list, nothing to move */` |
|         5 | 4606 | `		return;` |
|         - | 4607 | `	}` |
|        27 | 4608 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4609 | `		/* Already in the correct position */` |
|         9 | 4610 | `		return;` |
|         - | 4611 | `	}` |
|         - | 4612 | `	/* Unlink pNode from the end of the list */` |
|        19 | 4613 | `	pMap->pLast = pNode->pNext;` |
|        19 | 4614 | `	pMap->pLast->pPrev = 0;` |
|         - | 4615 | `	/* Insert pNode after pAfter in iteration order */` |
|        19 | 4616 | `	if( pAfter == 0 ){` |
|         - | 4617 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4618 | `		pNode->pNext = 0;` |
|         3 | 4619 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4620 | `		if( pMap->pFirst ){` |
|         3 | 4621 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4622 | `		}` |
|         3 | 4623 | `		pMap->pFirst = pNode;` |
|         2 | 4624 | `	}else{` |
|        17 | 4625 | `		pOldNext = pAfter->pPrev;` |
|        17 | 4626 | `		pNode->pPrev = pOldNext;` |
|        17 | 4627 | `		pNode->pNext = pAfter;` |
|        17 | 4628 | `		pAfter->pPrev = pNode;` |
|        17 | 4629 | `		if( pOldNext ){` |
|        17 | 4630 | `			pOldNext->pNext = pNode;` |
|         9 | 4631 | `		}else{` |
|       ! 0 | 4632 | `			pMap->pLast = pNode;` |
|         - | 4633 | `		}` |
|         - | 4634 | `	}` |
|        16 | 4635 | `}` |
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
|        54 | 4667 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4668 | `{` |
|         - | 4669 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4670 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4671 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4672 | `	int iLength,iOfft,i;` |
|         - | 4673 | `	sxi32 rc;` |
|        58 | 4674 | `	if( nArg < 2 ){` |
|         8 | 4675 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4676 | `			"ArgumentCountError",` |
|         - | 4677 | `			"array_splice() expects at least 2 arguments, %d given",` |
|         2 | 4678 | `			nArg` |
|         - | 4679 | `			);` |
|         - | 4680 | `	}` |
|        52 | 4681 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4682 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4683 | `			"TypeError",` |
|         - | 4684 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4685 | `			ph7_type_name(apArg[0])` |
|         - | 4686 | `			);` |
|         - | 4687 | `	}` |
|         - | 4688 | `	/* Point to the internal representation of the target array */` |
|        49 | 4689 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        49 | 4690 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4691 | `	/* Get the offset and clamp to valid range */` |
|        49 | 4692 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        49 | 4693 | `	if( iOfft < 0 ){` |
|         7 | 4694 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         7 | 4695 | `		if( iOfft < 0 ){` |
|         3 | 4696 | `			iOfft = 0;` |
|         2 | 4697 | `		}` |
|        46 | 4698 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4699 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4700 | `	}` |
|         - | 4701 | `	/* Get the length and clamp to valid range.` |
|         - | 4702 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        49 | 4703 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        49 | 4704 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        31 | 4705 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        31 | 4706 | `		if( iLength < 0 ){` |
|         7 | 4707 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4708 | `			if( iLength < 0 ){` |
|         3 | 4709 | `				iLength = 0;` |
|         1 | 4710 | `			}` |
|         3 | 4711 | `		}` |
|        31 | 4712 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4713 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4714 | `		}` |
|        15 | 4715 | `	}` |
|         - | 4716 | `	/* Create the result array for removed elements */` |
|        49 | 4717 | `	pArray = ph7_context_new_array(pCtx);` |
|        49 | 4718 | `	if( pArray == 0 ){` |
|       ! 0 | 4719 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4720 | `		return PH7_OK;` |
|         - | 4721 | `	}` |
|         - | 4722 | `	/* Get replacement array if provided */` |
|        49 | 4723 | `	pRep = 0;` |
|        49 | 4724 | `	if( nArg > 3 ){` |
|        21 | 4725 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4726 | `			/* Perform an array cast */` |
|         3 | 4727 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4728 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4729 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4730 | `			}` |
|         2 | 4731 | `		}else{` |
|        19 | 4732 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4733 | `		}` |
|        21 | 4734 | `		if( pRep ){` |
|         - | 4735 | `			/* Reset the loop cursor */` |
|        21 | 4736 | `			pRep->pCur = pRep->pFirst;` |
|        10 | 4737 | `		}` |
|        10 | 4738 | `	}` |
|         - | 4739 | `	/* Early return if nothing to remove and no replacement */` |
|        49 | 4740 | `	if( iLength < 1 && pRep == 0 ){` |
|         9 | 4741 | `		ph7_result_value(pCtx,pArray);` |
|         9 | 4742 | `		return PH7_OK;` |
|         - | 4743 | `	}` |
|         - | 4744 | `	/* Navigate to the offset position */` |
|        41 | 4745 | `	pCur = pSrc->pFirst;` |
|        85 | 4746 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        45 | 4747 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        23 | 4748 | `	}` |
|         - | 4749 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4750 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4751 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        41 | 4752 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4753 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        41 | 4754 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       111 | 4755 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        71 | 4756 | `		pPrev = pCur->pPrev;` |
|        71 | 4757 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        71 | 4758 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        71 | 4759 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4760 | `			break;` |
|         - | 4761 | `		}` |
|        71 | 4762 | `		pCur = pPrev; /* Reverse link */` |
|        36 | 4763 | `	}` |
|         - | 4764 | `	/* Insert replacement elements at the correct position */` |
|        41 | 4765 | `	if( pRep ){` |
|         - | 4766 | `		ph7_value sSafeVal;` |
|        61 | 4767 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        31 | 4768 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        31 | 4769 | `			if( pRvalue ){` |
|         - | 4770 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4771 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4772 | `				 * since it points into that same pool. */` |
|        31 | 4773 | `				sSafeVal = *pRvalue;` |
|        31 | 4774 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        31 | 4775 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        31 | 4776 | `					pNewNode = pSrc->pLast;` |
|        31 | 4777 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        31 | 4778 | `					pInsertAfter = pNewNode;` |
|        15 | 4779 | `				}` |
|        15 | 4780 | `			}` |
|         1 | 4781 | `		}` |
|        10 | 4782 | `	}` |
|         - | 4783 | `	/* Return the freshly created array */` |
|        41 | 4784 | `	ph7_result_value(pCtx,pArray);` |
|        41 | 4785 | `	return PH7_OK;` |
|        31 | 4786 | `}` |
|         - | 4787 | `/*` |
|         - | 4788 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4789 | ` *  Checks if a value exists in an array.` |
|         - | 4790 | ` * Parameters` |
|         - | 4791 | ` *  $needle` |
|         - | 4792 | ` *   The searched value.` |
|         - | 4793 | ` *   Note:` |
|         - | 4794 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4795 | ` * $haystack` |
|         - | 4796 | ` *  The target array.` |
|         - | 4797 | ` * $strict` |
|         - | 4798 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4799 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4800 | ` */` |
|     33142 | 4801 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4802 | `{` |
|         - | 4803 | `	ph7_value *pNeedle;` |
|         - | 4804 | `	int bStrict;` |
|         - | 4805 | `	int rc;` |
|     33147 | 4806 | `	if( nArg < 2 ){` |
|         - | 4807 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4808 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4809 | `		return PH7_OK;` |
|         - | 4810 | `	}` |
|     33147 | 4811 | `	pNeedle = apArg[0];` |
|     33147 | 4812 | `	bStrict = 0;` |
|     33147 | 4813 | `	if( nArg > 2 ){` |
|        53 | 4814 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4815 | `	}` |
|     33147 | 4816 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4817 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4818 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4819 | `		/* Set the comparison result */` |
|       ! 0 | 4820 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4821 | `		return PH7_OK;` |
|         - | 4822 | `	}` |
|         - | 4823 | `	/* Perform the lookup */` |
|     33147 | 4824 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4825 | `	/* Lookup result */` |
|     33147 | 4826 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     33147 | 4827 | `	return PH7_OK;` |
|     16576 | 4828 | `}` |
|         - | 4829 | `/*` |
|         - | 4830 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4831 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4832 | ` * Parameters` |
|         - | 4833 | ` * $needle` |
|         - | 4834 | ` *   The searched value.` |
|         - | 4835 | ` * $haystack` |
|         - | 4836 | ` *   The array.` |
|         - | 4837 | ` * $strict` |
|         - | 4838 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4839 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4840 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4841 | ` * Return` |
|         - | 4842 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4843 | ` */` |
|        32 | 4844 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4845 | `{` |
|         - | 4846 | `	ph7_hashmap_node *pEntry;` |
|         - | 4847 | `	ph7_value *pVal,sNeedle;` |
|         - | 4848 | `	ph7_hashmap *pMap;` |
|         - | 4849 | `	ph7_value sVal;` |
|         - | 4850 | `	int bStrict;` |
|         - | 4851 | `	sxu32 n;` |
|         - | 4852 | `	int rc;` |
|        37 | 4853 | `	if( nArg < 2 ){` |
|         - | 4854 | `		/* Missing argument,throw ArgumentCountError */` |
|         8 | 4855 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4856 | `			"ArgumentCountError",` |
|         - | 4857 | `			"array_search() expects at least 2 arguments, %d given",` |
|         2 | 4858 | `			nArg` |
|         - | 4859 | `			);` |
|         - | 4860 | `	}` |
|        31 | 4861 | `	bStrict = FALSE;` |
|        31 | 4862 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4863 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4864 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4865 | `			"TypeError",` |
|         - | 4866 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4867 | `			ph7_type_name(apArg[1])` |
|         - | 4868 | `			);` |
|         - | 4869 | `	}` |
|        28 | 4870 | `	if( nArg > 2 ){` |
|         - | 4871 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        14 | 4872 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4873 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4874 | `				"TypeError",` |
|         - | 4875 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4876 | `				ph7_type_name(apArg[2])` |
|         - | 4877 | `				);` |
|         - | 4878 | `		}` |
|        11 | 4879 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4880 | `	}` |
|         - | 4881 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4882 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4883 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4884 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 4885 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 4886 | `	pEntry = pMap->pFirst;` |
|        25 | 4887 | `	n = pMap->nEntry;` |
|        28 | 4888 | `	for(;;){` |
|        57 | 4889 | `		if( !n ){` |
|         9 | 4890 | `			break;` |
|         - | 4891 | `		}` |
|         - | 4892 | `		/* Extract node value */` |
|        49 | 4893 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 4894 | `		if( pVal ){` |
|         - | 4895 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4896 | `			 * can change their type.` |
|         - | 4897 | `			 */` |
|        49 | 4898 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 4899 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 4900 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 4901 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 4902 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 4903 | `			if( rc == 0 ){` |
|         - | 4904 | `				/* Match found,return key */` |
|        17 | 4905 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4906 | `					/* INT key */` |
|        11 | 4907 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 4908 | `				}else{` |
|         7 | 4909 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4910 | `					/* Blob key */` |
|         7 | 4911 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4912 | `				}` |
|        17 | 4913 | `				return PH7_OK;` |
|         - | 4914 | `			}` |
|        16 | 4915 | `		}` |
|         - | 4916 | `		/* Point to the next entry */` |
|        33 | 4917 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 4918 | `		n--;` |
|         1 | 4919 | `	}` |
|         - | 4920 | `	/* No such value,return FALSE */` |
|         9 | 4921 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4922 | `	return PH7_OK;` |
|        21 | 4923 | `}` |
|         - | 4924 | `/*` |
|         - | 4925 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4926 | ` *  Computes the difference of arrays.` |
|         - | 4927 | ` * Parameters` |
|         - | 4928 | ` *  $array1` |
|         - | 4929 | ` *    The array to compare from` |
|         - | 4930 | ` *  $array2` |
|         - | 4931 | ` *    An array to compare against` |
|         - | 4932 | ` *  $...` |
|         - | 4933 | ` *   More arrays to compare against` |
|         - | 4934 | ` * Return` |
|         - | 4935 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4936 | ` *  are not present in any of the other arrays.` |
|         - | 4937 | ` */` |
|        22 | 4938 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4939 | `{` |
|         - | 4940 | `	ph7_hashmap_node *pEntry;` |
|         - | 4941 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4942 | `	ph7_value *pArray;` |
|         - | 4943 | `	ph7_value *pVal;` |
|         - | 4944 | `	sxi32 rc;` |
|         - | 4945 | `	sxu32 n;` |
|         - | 4946 | `	int i;` |
|         - | 4947 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 4948 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 4949 | `	 * debugging difficult. */` |
|        26 | 4950 | `	if( nArg < 1 ){` |
|         4 | 4951 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4952 | `			"ArgumentCountError",` |
|         - | 4953 | `			"array_diff() expects at least 1 argument, %d given",` |
|         1 | 4954 | `			nArg` |
|         - | 4955 | `			);` |
|         - | 4956 | `	}` |
|        23 | 4957 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4958 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4959 | `			"TypeError",` |
|         - | 4960 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4961 | `			ph7_type_name(apArg[0])` |
|         - | 4962 | `			);` |
|         - | 4963 | `	}` |
|        36 | 4964 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 4965 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 4966 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4967 | `				"TypeError",` |
|         - | 4968 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 4969 | `				i + 1,` |
|         2 | 4970 | `				ph7_type_name(apArg[i])` |
|         - | 4971 | `				);` |
|         - | 4972 | `		}` |
|         9 | 4973 | `	}` |
|        17 | 4974 | `	if( nArg == 1 ){` |
|         - | 4975 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 4976 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 4977 | `		return PH7_OK;` |
|         - | 4978 | `	}` |
|         - | 4979 | `	/* Create a new array */` |
|        15 | 4980 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 4981 | `	if( pArray == 0 ){` |
|       ! 0 | 4982 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4983 | `		return PH7_OK;` |
|         - | 4984 | `	}` |
|         - | 4985 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 4986 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4987 | `	/* Perform the diff */` |
|        15 | 4988 | `	pEntry = pSrc->pFirst;` |
|        15 | 4989 | `	n = pSrc->nEntry;` |
|        27 | 4990 | `	for(;;){` |
|        55 | 4991 | `		if( n < 1 ){` |
|        15 | 4992 | `			break;` |
|         - | 4993 | `		}` |
|         - | 4994 | `		/* Extract the node value */` |
|        41 | 4995 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 4996 | `		if( pVal ){` |
|        69 | 4997 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 4998 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 4999 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5000 | `				/* Perform the lookup */` |
|        45 | 5001 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5002 | `				if( rc == SXRET_OK ){` |
|         - | 5003 | `					/* Value exist */` |
|        17 | 5004 | `					break;` |
|         - | 5005 | `				}` |
|        15 | 5006 | `			}` |
|        41 | 5007 | `			if( i >= nArg ){` |
|         - | 5008 | `				/* Perform the insertion */` |
|        25 | 5009 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5010 | `			}` |
|        20 | 5011 | `		}` |
|         - | 5012 | `		/* Point to the next entry */` |
|        41 | 5013 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5014 | `		n--;` |
|         1 | 5015 | `	}` |
|         - | 5016 | `	/* Return the freshly created array */` |
|        15 | 5017 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5018 | `	return PH7_OK;` |
|        15 | 5019 | `}` |
|         - | 5020 | `/*` |
|         - | 5021 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5022 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5023 | ` * Parameters` |
|         - | 5024 | ` *  $array1` |
|         - | 5025 | ` *    The array to compare from` |
|         - | 5026 | ` *  $array2` |
|         - | 5027 | ` *    An array to compare against` |
|         - | 5028 | ` *  $...` |
|         - | 5029 | ` *   More arrays to compare against.` |
|         - | 5030 | ` * $callback` |
|         - | 5031 | ` *  The callback comparison function.` |
|         - | 5032 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5033 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5034 | ` *  than the second.` |
|         - | 5035 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5036 | ` * Return` |
|         - | 5037 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5038 | ` *  are not present in any of the other arrays.` |
|         - | 5039 | ` */` |
|        22 | 5040 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5041 | `{` |
|         - | 5042 | `	ph7_hashmap_node *pEntry;` |
|         - | 5043 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5044 | `	ph7_value *pCallback;` |
|         - | 5045 | `	ph7_value *pArray;` |
|         - | 5046 | `	ph7_value *pVal;` |
|         - | 5047 | `	sxi32 rc;` |
|         - | 5048 | `	sxu32 n;` |
|         - | 5049 | `	int i;` |
|         - | 5050 |  |
|         - | 5051 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        27 | 5052 | `	if( nArg < 2 ){` |
|         4 | 5053 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5054 | `			"ArgumentCountError",` |
|         - | 5055 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|         1 | 5056 | `			nArg` |
|         - | 5057 | `			);` |
|         - | 5058 | `	}` |
|        25 | 5059 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5060 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5061 | `			"TypeError",` |
|         - | 5062 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5063 | `			ph7_type_name(apArg[0])` |
|         - | 5064 | `			);` |
|         - | 5065 | `	}` |
|         - | 5066 |  |
|        23 | 5067 | `	if( nArg == 2 ){` |
|         - | 5068 | `		/* Only the original array and the callback were provided. */` |
|         - | 5069 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5070 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5071 | `		 * validation order.` |
|         - | 5072 | `		 */` |
|         4 | 5073 | `	} else {` |
|         - | 5074 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5075 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5076 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5077 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5078 | `					"TypeError",` |
|         - | 5079 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5080 | `					i + 1,` |
|         6 | 5081 | `					ph7_type_name(apArg[i])` |
|         - | 5082 | `					);` |
|         - | 5083 | `			}` |
|         7 | 5084 | `		}` |
|         - | 5085 | `	}` |
|         - | 5086 |  |
|         - | 5087 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5088 | `	pCallback = apArg[nArg - 1];` |
|         - | 5089 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5090 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5091 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5092 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5093 | `				"TypeError",` |
|         - | 5094 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5095 | `				nArg` |
|         - | 5096 | `				);` |
|         - | 5097 | `		}` |
|         6 | 5098 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5099 | `			int len;` |
|         3 | 5100 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5101 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5102 | `				"TypeError",` |
|         - | 5103 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5104 | `				nArg,` |
|         1 | 5105 | `				zName` |
|         - | 5106 | `				);` |
|         - | 5107 | `		}` |
|         4 | 5108 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5109 | `			"TypeError",` |
|         - | 5110 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5111 | `			nArg` |
|         - | 5112 | `			);` |
|         - | 5113 | `	}` |
|         - | 5114 |  |
|         7 | 5115 | `	if( nArg == 2 ){` |
|         - | 5116 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5117 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5118 | `		return PH7_OK;` |
|         - | 5119 | `	}` |
|         - | 5120 |  |
|         - | 5121 | `	/* Create a new array */` |
|         5 | 5122 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5123 | `	if( pArray == 0 ){` |
|       ! 0 | 5124 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5125 | `		return PH7_OK;` |
|         - | 5126 | `	}` |
|         - | 5127 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5128 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5129 | `	/* Perform the diff */` |
|         5 | 5130 | `	pEntry = pSrc->pFirst;` |
|         5 | 5131 | `	n = pSrc->nEntry;` |
|         5 | 5132 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5133 | `	for(;;){` |
|        11 | 5134 | `		if( n < 1 ){` |
|         3 | 5135 | `			break;` |
|         - | 5136 | `		}` |
|         - | 5137 | `		/* Extract the node value */` |
|         9 | 5138 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5139 | `		if( pVal ){` |
|        15 | 5140 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5141 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5142 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5143 | `				/* Perform the lookup */` |
|         9 | 5144 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5145 | `				if( rc == SXRET_OK ){` |
|         - | 5146 | `					/* Value exist */` |
|         3 | 5147 | `					break;` |
|         - | 5148 | `				}` |
|         4 | 5149 | `			}` |
|         9 | 5150 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5151 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5152 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5153 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5154 | `				return PH7_EXCEPTION;` |
|         - | 5155 | `			}` |
|         7 | 5156 | `			if( i >= (nArg - 1)){` |
|         - | 5157 | `				/* Perform the insertion */` |
|         5 | 5158 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5159 | `			}` |
|         3 | 5160 | `		}` |
|         - | 5161 | `		/* Point to the next entry */` |
|         7 | 5162 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5163 | `		n--;` |
|         1 | 5164 | `	}` |
|         - | 5165 | `	/* Return the freshly created array */` |
|         3 | 5166 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5167 | `	return PH7_OK;` |
|        16 | 5168 | `}` |
|         - | 5169 | `/*` |
|         - | 5170 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5171 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5172 | ` * Parameters` |
|         - | 5173 | ` *  $array1` |
|         - | 5174 | ` *    The array to compare from` |
|         - | 5175 | ` *  $array2` |
|         - | 5176 | ` *    An array to compare against` |
|         - | 5177 | ` *  $...` |
|         - | 5178 | ` *   More arrays to compare against` |
|         - | 5179 | ` * Return` |
|         - | 5180 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5181 | ` *  are not present in any of the other arrays.` |
|         - | 5182 | ` */` |
|        22 | 5183 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5184 | `{` |
|         - | 5185 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5186 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5187 | `	ph7_value *pArray;` |
|         - | 5188 | `	ph7_value *pVal;` |
|         - | 5189 | `	sxi32 rc;` |
|         - | 5190 | `	sxu32 n;` |
|         - | 5191 | `	int i;` |
|         - | 5192 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5193 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5194 | `	 * accompanying integration tests to pass. */` |
|        27 | 5195 | `	if( nArg < 1 ){` |
|         4 | 5196 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5197 | `			"ArgumentCountError",` |
|         - | 5198 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|         1 | 5199 | `			nArg` |
|         - | 5200 | `			);` |
|         - | 5201 | `	}` |
|        24 | 5202 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5203 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5204 | `			"TypeError",` |
|         - | 5205 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5206 | `			ph7_type_name(apArg[0])` |
|         - | 5207 | `			);` |
|         - | 5208 | `	}` |
|        37 | 5209 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5210 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5211 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5212 | `				"TypeError",` |
|         - | 5213 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5214 | `				i + 1,` |
|         4 | 5215 | `				ph7_type_name(apArg[i])` |
|         - | 5216 | `				);` |
|         - | 5217 | `		}` |
|        10 | 5218 | `	}` |
|        15 | 5219 | `	if( nArg == 1 ){` |
|         - | 5220 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5221 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5222 | `		return PH7_OK;` |
|         - | 5223 | `	}` |
|         - | 5224 | `	/* Create a new array */` |
|        13 | 5225 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5226 | `	if( pArray == 0 ){` |
|       ! 0 | 5227 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5228 | `		return PH7_OK;` |
|         - | 5229 | `	}` |
|         - | 5230 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5231 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5232 | `	/* Perform the diff */` |
|        13 | 5233 | `	pEntry = pSrc->pFirst;` |
|        13 | 5234 | `	n = pSrc->nEntry;` |
|        13 | 5235 | `	pN1 = pN2 = 0;` |
|        34 | 5236 | `	for(;;){` |
|         - | 5237 | `		int keep;` |
|        41 | 5238 | `		if( n < 1 ){` |
|        13 | 5239 | `			break;` |
|         - | 5240 | `		}` |
|         - | 5241 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5242 | `		keep = 1;` |
|        47 | 5243 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5244 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5245 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5246 | `			/* Perform a key lookup first */` |
|        33 | 5247 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5248 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5249 | `			}else{` |
|        21 | 5250 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5251 | `			}` |
|        33 | 5252 | `			if( rc != SXRET_OK ){` |
|         - | 5253 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5254 | `				continue;` |
|         - | 5255 | `			}` |
|         - | 5256 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5257 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5258 | `			if( pVal ){` |
|         - | 5259 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5260 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5261 | `				if( pVal2 ){` |
|         - | 5262 | `					ph7_value sV1,sV2;` |
|         - | 5263 | `					sxi32 cmp;` |
|         - | 5264 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5265 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5266 | `					 * null element used to come back bool(false) in the` |
|         - | 5267 | `					 * caller's array). */` |
|        17 | 5268 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5269 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5270 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5271 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5272 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5273 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5274 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5275 | `					if( cmp == 0 ){` |
|         - | 5276 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5277 | `						keep = 0;` |
|        15 | 5278 | `						break;` |
|         - | 5279 | `					}` |
|         1 | 5280 | `				}` |
|         1 | 5281 | `			}` |
|         2 | 5282 | `		}` |
|        29 | 5283 | `		if( keep ){` |
|         - | 5284 | `			/* Perform the insertion */` |
|        15 | 5285 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5286 | `		}` |
|         - | 5287 | `		/* Point to the next entry */` |
|        29 | 5288 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5289 | `		n--;` |
|         1 | 5290 | `	}` |
|         - | 5291 | `	/* Return the freshly created array */` |
|        13 | 5292 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5293 | `	return PH7_OK;` |
|        16 | 5294 | `}` |
|         - | 5295 | `/*` |
|         - | 5296 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5297 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5298 | ` *  by a user supplied callback function.` |
|         - | 5299 | ` * Parameters` |
|         - | 5300 | ` *  $array1` |
|         - | 5301 | ` *    The array to compare from` |
|         - | 5302 | ` *  $array2` |
|         - | 5303 | ` *    An array to compare against` |
|         - | 5304 | ` *  $...` |
|         - | 5305 | ` *   More arrays to compare against.` |
|         - | 5306 | ` *  $key_compare_func` |
|         - | 5307 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5308 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5309 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5310 | ` * Return` |
|         - | 5311 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5312 | ` *  are not present in any of the other arrays.` |
|         - | 5313 | ` */` |
|        24 | 5314 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5315 | `{` |
|         - | 5316 | `	ph7_hashmap_node *pEntry;` |
|         - | 5317 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5318 | `	ph7_value *pCallback;` |
|         - | 5319 | `	ph7_value *pArray;` |
|         - | 5320 | `	sxi32 rc;` |
|         - | 5321 | `	sxu32 n;` |
|         - | 5322 | `	int i;` |
|         - | 5323 |  |
|         - | 5324 | `	/* Argument validation mimicking PHP errors. */` |
|        29 | 5325 | `	if( nArg < 2 ){` |
|         4 | 5326 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5327 | `			"ArgumentCountError",` |
|         - | 5328 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|         1 | 5329 | `			nArg` |
|         - | 5330 | `			);` |
|         - | 5331 | `	}` |
|        26 | 5332 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5333 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5334 | `			"TypeError",` |
|         - | 5335 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5336 | `			ph7_type_name(apArg[0])` |
|         - | 5337 | `			);` |
|         - | 5338 | `	}` |
|         - | 5339 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5340 | `	 * expected to be a callback. */` |
|        38 | 5341 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5342 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5343 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5344 | `				"TypeError",` |
|         - | 5345 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5346 | `				i + 1,` |
|         2 | 5347 | `				ph7_type_name(apArg[i])` |
|         - | 5348 | `				);` |
|         - | 5349 | `		}` |
|         9 | 5350 | `	}` |
|         - | 5351 | `	/* Point to the callback value */` |
|        22 | 5352 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5353 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5354 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5355 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5356 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5357 | `		 * string given" which we also reproduce. */` |
|         9 | 5358 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5359 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5360 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5361 | `				"TypeError",` |
|         - | 5362 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5363 | `				nArg` |
|         - | 5364 | `				);` |
|         - | 5365 | `		}` |
|         6 | 5366 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5367 | `			/* neither array nor string */` |
|         8 | 5368 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5369 | `				"TypeError",` |
|         - | 5370 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5371 | `				nArg` |
|         - | 5372 | `				);` |
|         - | 5373 | `		}` |
|         - | 5374 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5375 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5376 | `			"TypeError",` |
|         - | 5377 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5378 | `			nArg,` |
|       ! 0 | 5379 | `			ph7_type_name(pCallback)` |
|         - | 5380 | `			);` |
|         - | 5381 | `	}` |
|        13 | 5382 | `	if( nArg == 2 ){` |
|         - | 5383 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5384 | `		 * input array. */` |
|         3 | 5385 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5386 | `		return PH7_OK;` |
|         - | 5387 | `	}` |
|         - | 5388 | `	/* Create a new array */` |
|        11 | 5389 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5390 | `	if( pArray == 0 ){` |
|       ! 0 | 5391 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5392 | `		return PH7_OK;` |
|         - | 5393 | `	}` |
|         - | 5394 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5395 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5396 | `	/* Perform the diff */` |
|        11 | 5397 | `	pEntry = pSrc->pFirst;` |
|        11 | 5398 | `	n = pSrc->nEntry;` |
|        21 | 5399 | `	for(;;){` |
|         - | 5400 | `		int keep;` |
|        27 | 5401 | `		if( n < 1 ){` |
|         9 | 5402 | `			break;` |
|         - | 5403 | `		}` |
|        19 | 5404 | `		keep = 1;` |
|        31 | 5405 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5406 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5407 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5408 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5409 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5410 | `			while( pIt ){` |
|         - | 5411 | `				/* build temporary key values for callback */` |
|         - | 5412 | `				ph7_value key1, key2, result;` |
|         - | 5413 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5414 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5415 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5416 | `				}else{` |
|         - | 5417 | `					SyString sStr;` |
|        33 | 5418 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5419 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5420 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5421 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5422 | `				}` |
|        33 | 5423 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5424 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5425 | `				}else{` |
|         - | 5426 | `					SyString sStr;` |
|        33 | 5427 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5428 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5429 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5430 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5431 | `				}` |
|        33 | 5432 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5433 | `				/* call user callback with (key1, key2) */` |
|         - | 5434 | `				{` |
|         - | 5435 | `					ph7_value *apK[2];` |
|        33 | 5436 | `					apK[0] = &key1;` |
|        33 | 5437 | `					apK[1] = &key2;` |
|        33 | 5438 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5439 | `				}` |
|        33 | 5440 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5441 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5442 | `					 * array_uintersect (which signal back from` |
|         - | 5443 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5444 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5445 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5446 | `					PH7_MemObjRelease(&result);` |
|         3 | 5447 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5448 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5449 | `					return PH7_EXCEPTION;` |
|         - | 5450 | `				}` |
|        31 | 5451 | `				if( rc == SXRET_OK ){` |
|        31 | 5452 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5453 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5454 | `					}` |
|        31 | 5455 | `					if( result.x.iVal == 0 ){` |
|         - | 5456 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5457 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5458 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5459 | `						if( pVal1 && pVal2 ){` |
|         - | 5460 | `							ph7_value sV1,sV2;` |
|         - | 5461 | `							sxi32 cmp;` |
|         - | 5462 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5463 | `							 * place and these are LIVE array elements. */` |
|        13 | 5464 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5465 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5466 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5467 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5468 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5469 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5470 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5471 | `							if( cmp == 0 ){` |
|         9 | 5472 | `								keep = 0;` |
|         9 | 5473 | `								PH7_MemObjRelease(&result);` |
|         - | 5474 | `								/* release keys too before breaking */` |
|         9 | 5475 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5476 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5477 | `								break;` |
|         - | 5478 | `							}` |
|         2 | 5479 | `						}` |
|         2 | 5480 | `					}` |
|        11 | 5481 | `				}` |
|        23 | 5482 | `				PH7_MemObjRelease(&result);` |
|        23 | 5483 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5484 | `				PH7_MemObjRelease(&key2);` |
|         - | 5485 | `				/* move to next node */` |
|        23 | 5486 | `				pIt = pIt->pPrev;` |
|        23 | 5487 | `				if( keep == 0 ) break;` |
|         1 | 5488 | `			}` |
|        21 | 5489 | `			if( keep == 0 ) break;` |
|         7 | 5490 | `		}` |
|        17 | 5491 | `		if( keep ){` |
|         - | 5492 | `			/* Perform the insertion */` |
|         9 | 5493 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5494 | `		}` |
|         - | 5495 | `		/* Point to the next entry */` |
|        17 | 5496 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5497 | `		n--;` |
|         1 | 5498 | `	}` |
|         - | 5499 | `	/* Return the freshly created array */` |
|         9 | 5500 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5501 | `	return PH7_OK;` |
|        17 | 5502 | `}` |
|         - | 5503 | `/*` |
|         - | 5504 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5505 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5506 | ` * Parameters` |
|         - | 5507 | ` *  $array1` |
|         - | 5508 | ` *    The array to compare from` |
|         - | 5509 | ` *  $array2` |
|         - | 5510 | ` *    An array to compare against` |
|         - | 5511 | ` *  $...` |
|         - | 5512 | ` *   More arrays to compare against` |
|         - | 5513 | ` * Return` |
|         - | 5514 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5515 | ` *  in any of the other arrays.` |
|         - | 5516 | ` * Note that NULL is returned on failure.` |
|         - | 5517 | ` */` |
|        14 | 5518 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5519 | `{` |
|         - | 5520 | `	ph7_hashmap_node *pEntry;` |
|         - | 5521 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5522 | `	ph7_value *pArray;` |
|         - | 5523 | `	sxi32 rc;` |
|         - | 5524 | `	sxu32 n;` |
|         - | 5525 | `	int i;` |
|         - | 5526 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5527 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5528 | `	 * helpers. */` |
|        18 | 5529 | `	if( nArg < 1 ){` |
|         4 | 5530 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5531 | `			"ArgumentCountError",` |
|         - | 5532 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|         1 | 5533 | `			nArg` |
|         - | 5534 | `			);` |
|         - | 5535 | `	}` |
|        15 | 5536 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5537 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5538 | `			"TypeError",` |
|         - | 5539 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5540 | `			ph7_type_name(apArg[0])` |
|         - | 5541 | `			);` |
|         - | 5542 | `	}` |
|        20 | 5543 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5544 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5545 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5546 | `				"TypeError",` |
|         - | 5547 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5548 | `				i + 1,` |
|         2 | 5549 | `				ph7_type_name(apArg[i])` |
|         - | 5550 | `				);` |
|         - | 5551 | `		}` |
|         5 | 5552 | `	}` |
|         9 | 5553 | `	if( nArg == 1 ){` |
|         - | 5554 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5555 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5556 | `		return PH7_OK;` |
|         - | 5557 | `	}` |
|         - | 5558 | `	/* Create a new array */` |
|         7 | 5559 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5560 | `	if( pArray == 0 ){` |
|       ! 0 | 5561 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5562 | `		return PH7_OK;` |
|         - | 5563 | `	}` |
|         - | 5564 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5565 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5566 | `	/* Perfrom the diff */` |
|         7 | 5567 | `	pEntry = pSrc->pFirst;` |
|         7 | 5568 | `	n = pSrc->nEntry;` |
|        12 | 5569 | `	for(;;){` |
|        25 | 5570 | `		if( n < 1 ){` |
|         7 | 5571 | `			break;` |
|         - | 5572 | `		}` |
|        31 | 5573 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5574 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5575 | `				/* ignore */` |
|       ! 0 | 5576 | `				continue;` |
|         - | 5577 | `			}` |
|        23 | 5578 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5579 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5580 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5581 | `				/* Blob lookup */` |
|        17 | 5582 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5583 | `			}else{` |
|         - | 5584 | `				/* Int lookup */` |
|         7 | 5585 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5586 | `			}` |
|        23 | 5587 | `			if( rc == SXRET_OK ){` |
|         - | 5588 | `				/* Key exists,break immediately */` |
|        11 | 5589 | `				break;` |
|         - | 5590 | `			}` |
|         7 | 5591 | `		}` |
|        19 | 5592 | `		if( i >= nArg ){` |
|         - | 5593 | `			/* Perform the insertion */` |
|         9 | 5594 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5595 | `		}` |
|         - | 5596 | `		/* Point to the next entry */` |
|        19 | 5597 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5598 | `		n--;` |
|         1 | 5599 | `	}` |
|         - | 5600 | `	/* Return the freshly created array */` |
|         7 | 5601 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5602 | `	return PH7_OK;` |
|        11 | 5603 | `}` |
|         - | 5604 | `/*` |
|         - | 5605 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5606 | ` *  Computes the intersection of arrays.` |
|         - | 5607 | ` * Parameters` |
|         - | 5608 | ` *  $array1` |
|         - | 5609 | ` *    The array to compare from` |
|         - | 5610 | ` *  $array2` |
|         - | 5611 | ` *    An array to compare against` |
|         - | 5612 | ` *  $...` |
|         - | 5613 | ` *   More arrays to compare against` |
|         - | 5614 | ` * Return` |
|         - | 5615 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5616 | ` *  in all of the parameters.` |
|         - | 5617 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5618 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5619 | ` */` |
|        22 | 5620 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5621 | `{` |
|         - | 5622 | `	ph7_hashmap_node *pEntry;` |
|         - | 5623 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5624 | `	ph7_value *pArray;` |
|         - | 5625 | `	ph7_value *pVal;` |
|         - | 5626 | `	sxi32 rc;` |
|         - | 5627 | `	sxu32 n;` |
|         - | 5628 | `	int i;` |
|        26 | 5629 | `	if( nArg < 1 ){` |
|         4 | 5630 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5631 | `			"ArgumentCountError",` |
|         - | 5632 | `			"array_intersect() expects at least 1 argument, %d given",` |
|         1 | 5633 | `			nArg` |
|         - | 5634 | `			);` |
|         - | 5635 | `	}` |
|        23 | 5636 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5637 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5638 | `			"TypeError",` |
|         - | 5639 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5640 | `			ph7_type_name(apArg[0])` |
|         - | 5641 | `			);` |
|         - | 5642 | `	}` |
|        36 | 5643 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5644 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5645 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5646 | `				"TypeError",` |
|         - | 5647 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5648 | `				i + 1,` |
|         2 | 5649 | `				ph7_type_name(apArg[i])` |
|         - | 5650 | `				);` |
|         - | 5651 | `		}` |
|         9 | 5652 | `	}` |
|        17 | 5653 | `	if( nArg == 1 ){` |
|         - | 5654 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5655 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5656 | `		return PH7_OK;` |
|         - | 5657 | `	}` |
|         - | 5658 | `	/* Create a new array */` |
|        15 | 5659 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5660 | `	if( pArray == 0 ){` |
|       ! 0 | 5661 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5662 | `		return PH7_OK;` |
|         - | 5663 | `	}` |
|         - | 5664 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5665 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5666 | `	/* Perform the intersection */` |
|        15 | 5667 | `	pEntry = pSrc->pFirst;` |
|        15 | 5668 | `	n = pSrc->nEntry;` |
|        31 | 5669 | `	for(;;){` |
|        63 | 5670 | `		if( n < 1 ){` |
|        15 | 5671 | `			break;` |
|         - | 5672 | `		}` |
|         - | 5673 | `		/* Extract the node value */` |
|        49 | 5674 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5675 | `		if( pVal ){` |
|        79 | 5676 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5677 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5678 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5679 | `				/* Perform the lookup */` |
|        55 | 5680 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5681 | `				if( rc != SXRET_OK ){` |
|         - | 5682 | `					/* Value does not exist */` |
|        25 | 5683 | `					break;` |
|         - | 5684 | `				}` |
|        16 | 5685 | `			}` |
|        49 | 5686 | `			if( i >= nArg ){` |
|         - | 5687 | `				/* Perform the insertion */` |
|        25 | 5688 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5689 | `			}` |
|        24 | 5690 | `		}` |
|         - | 5691 | `		/* Point to the next entry */` |
|        49 | 5692 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5693 | `		n--;` |
|         1 | 5694 | `	}` |
|         - | 5695 | `	/* Return the freshly created array */` |
|        15 | 5696 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5697 | `	return PH7_OK;` |
|        15 | 5698 | `}` |
|         - | 5699 | `/*` |
|         - | 5700 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5701 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5702 | ` * Parameters` |
|         - | 5703 | ` *  $array1` |
|         - | 5704 | ` *    The array to compare from` |
|         - | 5705 | ` *  $array2` |
|         - | 5706 | ` *    An array to compare against` |
|         - | 5707 | ` *  $...` |
|         - | 5708 | ` *   More arrays to compare against` |
|         - | 5709 | ` * Return` |
|         - | 5710 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5711 | ` *  in all the arguments, with matching keys.` |
|         - | 5712 | ` */` |
|        22 | 5713 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5714 | `{` |
|         - | 5715 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5716 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5717 | `	ph7_value *pArray;` |
|         - | 5718 | `	ph7_value *pVal;` |
|         - | 5719 | `	sxi32 rc;` |
|         - | 5720 | `	sxu32 n;` |
|         - | 5721 | `	int i;` |
|        26 | 5722 | `	if( nArg < 1 ){` |
|         4 | 5723 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5724 | `			"ArgumentCountError",` |
|         - | 5725 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|         1 | 5726 | `			nArg` |
|         - | 5727 | `			);` |
|         - | 5728 | `	}` |
|        23 | 5729 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5730 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5731 | `			"TypeError",` |
|         - | 5732 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5733 | `			ph7_type_name(apArg[0])` |
|         - | 5734 | `			);` |
|         - | 5735 | `	}` |
|        36 | 5736 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5737 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5738 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5739 | `				"TypeError",` |
|         - | 5740 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5741 | `				i + 1,` |
|         2 | 5742 | `				ph7_type_name(apArg[i])` |
|         - | 5743 | `				);` |
|         - | 5744 | `		}` |
|         9 | 5745 | `	}` |
|        17 | 5746 | `	if( nArg == 1 ){` |
|         - | 5747 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5748 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5749 | `		return PH7_OK;` |
|         - | 5750 | `	}` |
|         - | 5751 | `	/* Create a new array */` |
|        15 | 5752 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5753 | `	if( pArray == 0 ){` |
|       ! 0 | 5754 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5755 | `		return PH7_OK;` |
|         - | 5756 | `	}` |
|         - | 5757 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5758 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5759 | `	/* Perform the intersection */` |
|        15 | 5760 | `	pEntry = pSrc->pFirst;` |
|        15 | 5761 | `	n = pSrc->nEntry;` |
|        15 | 5762 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5763 | `	for(;;){` |
|        47 | 5764 | `		if( n < 1 ){` |
|        15 | 5765 | `			break;` |
|         - | 5766 | `		}` |
|         - | 5767 | `		/* Extract the node value */` |
|        33 | 5768 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5769 | `		if( pVal ){` |
|        53 | 5770 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5771 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5772 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5773 | `				/* Perform a key lookup first */` |
|        37 | 5774 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5775 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5776 | `				}else{` |
|        23 | 5777 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5778 | `				}` |
|        37 | 5779 | `				if( rc != SXRET_OK ){` |
|         - | 5780 | `					/* No such key,break immediately */` |
|         7 | 5781 | `					break;` |
|         - | 5782 | `				}` |
|         - | 5783 | `				/* Perform the lookup */` |
|        31 | 5784 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5785 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5786 | `					/* Value does not exist */` |
|         6 | 5787 | `					break;` |
|         - | 5788 | `				}` |
|        11 | 5789 | `			}` |
|        33 | 5790 | `			if( i >= nArg ){` |
|         - | 5791 | `				/* Perform the insertion */` |
|        17 | 5792 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5793 | `			}` |
|        16 | 5794 | `		}` |
|         - | 5795 | `		/* Point to the next entry */` |
|        33 | 5796 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5797 | `		n--;` |
|         1 | 5798 | `	}` |
|         - | 5799 | `	/* Return the freshly created array */` |
|        15 | 5800 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5801 | `	return PH7_OK;` |
|        15 | 5802 | `}` |
|         - | 5803 | `/*` |
|         - | 5804 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5805 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5806 | ` * Parameters` |
|         - | 5807 | ` *  $array1` |
|         - | 5808 | ` *    The array to compare from` |
|         - | 5809 | ` *  $...` |
|         - | 5810 | ` *   More arrays to compare against` |
|         - | 5811 | ` * Return` |
|         - | 5812 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5813 | ` *  have keys that are present in all arguments.` |
|         - | 5814 | ` * Note that NULL is returned on failure.` |
|         - | 5815 | ` */` |
|        22 | 5816 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5817 | `{` |
|         - | 5818 | `	ph7_hashmap_node *pEntry;` |
|         - | 5819 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5820 | `	ph7_value *pArray;` |
|         - | 5821 | `	sxi32 rc;` |
|         - | 5822 | `	sxu32 n;` |
|         - | 5823 | `	int i;` |
|        26 | 5824 | `	if( nArg < 1 ){` |
|         4 | 5825 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5826 | `			"ArgumentCountError",` |
|         - | 5827 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|         1 | 5828 | `			nArg` |
|         - | 5829 | `			);` |
|         - | 5830 | `	}` |
|        23 | 5831 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5832 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5833 | `			"TypeError",` |
|         - | 5834 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5835 | `			ph7_type_name(apArg[0])` |
|         - | 5836 | `			);` |
|         - | 5837 | `	}` |
|        36 | 5838 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5839 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5840 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5841 | `				"TypeError",` |
|         - | 5842 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5843 | `				i + 1,` |
|         2 | 5844 | `				ph7_type_name(apArg[i])` |
|         - | 5845 | `				);` |
|         - | 5846 | `		}` |
|         9 | 5847 | `	}` |
|        17 | 5848 | `	if( nArg == 1 ){` |
|         - | 5849 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5850 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5851 | `		return PH7_OK;` |
|         - | 5852 | `	}` |
|         - | 5853 | `	/* Create a new array */` |
|        15 | 5854 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5855 | `	if( pArray == 0 ){` |
|       ! 0 | 5856 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5857 | `		return PH7_OK;` |
|         - | 5858 | `	}` |
|         - | 5859 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5860 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5861 | `	/* Perform the intersection */` |
|        15 | 5862 | `	pEntry = pSrc->pFirst;` |
|        15 | 5863 | `	n = pSrc->nEntry;` |
|        24 | 5864 | `	for(;;){` |
|        49 | 5865 | `		if( n < 1 ){` |
|        15 | 5866 | `			break;` |
|         - | 5867 | `		}` |
|        57 | 5868 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5869 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5870 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5871 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5872 | `				/* Blob lookup */` |
|        27 | 5873 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5874 | `			}else{` |
|         - | 5875 | `				/* Int key */` |
|        13 | 5876 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5877 | `			}` |
|        39 | 5878 | `			if( rc != SXRET_OK ){` |
|         - | 5879 | `				/* Key does not exist, break immediately */` |
|        17 | 5880 | `				break;` |
|         - | 5881 | `			}` |
|        12 | 5882 | `		}` |
|        35 | 5883 | `		if( i >= nArg ){` |
|         - | 5884 | `			/* Perform the insertion */` |
|        19 | 5885 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5886 | `		}` |
|         - | 5887 | `		/* Point to the next entry */` |
|        35 | 5888 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5889 | `		n--;` |
|         1 | 5890 | `	}` |
|         - | 5891 | `	/* Return the freshly created array */` |
|        15 | 5892 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5893 | `	return PH7_OK;` |
|        15 | 5894 | `}` |
|         - | 5895 | `/*` |
|         - | 5896 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5897 | ` *  Computes the intersection of arrays.` |
|         - | 5898 | ` * Parameters` |
|         - | 5899 | ` *  $array1` |
|         - | 5900 | ` *    The array to compare from` |
|         - | 5901 | ` *  $array2` |
|         - | 5902 | ` *    An array to compare against` |
|         - | 5903 | ` *  $...` |
|         - | 5904 | ` *   More arrays to compare against` |
|         - | 5905 | ` * $callback` |
|         - | 5906 | ` *  The callback comparison function.` |
|         - | 5907 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5908 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5909 | ` *  than the second.` |
|         - | 5910 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5911 | ` * Return` |
|         - | 5912 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5913 | ` *  in all of the parameters. .` |
|         - | 5914 | ` * Note that NULL is returned on failure.` |
|         - | 5915 | ` */` |
|        26 | 5916 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5917 | `{` |
|         - | 5918 | `	ph7_hashmap_node *pEntry;` |
|         - | 5919 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5920 | `	ph7_value *pCallback;` |
|         - | 5921 | `	ph7_value *pArray;` |
|         - | 5922 | `	ph7_value *pVal;` |
|         - | 5923 | `	sxi32 rc;` |
|         - | 5924 | `	sxu32 n;` |
|         - | 5925 | `	int i;` |
|         - | 5926 |  |
|         - | 5927 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        31 | 5928 | `	if( nArg < 2 ){` |
|         4 | 5929 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5930 | `			"ArgumentCountError",` |
|         - | 5931 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|         1 | 5932 | `			nArg` |
|         - | 5933 | `			);` |
|         - | 5934 | `	}` |
|        29 | 5935 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5936 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5937 | `			"TypeError",` |
|         - | 5938 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5939 | `			ph7_type_name(apArg[0])` |
|         - | 5940 | `			);` |
|         - | 5941 | `	}` |
|         - | 5942 |  |
|        27 | 5943 | `	if( nArg == 2 ){` |
|         - | 5944 | `		/* Only the original array and the callback were provided. */` |
|         - | 5945 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 5946 | `		 * validation ordering. */` |
|         3 | 5947 | `	} else {` |
|         - | 5948 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 5949 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 5950 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5951 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5952 | `					"TypeError",` |
|         - | 5953 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5954 | `					i + 1,` |
|         2 | 5955 | `					ph7_type_name(apArg[i])` |
|         - | 5956 | `					);` |
|         - | 5957 | `			}` |
|        13 | 5958 | `		}` |
|         - | 5959 | `	}` |
|         - | 5960 |  |
|         - | 5961 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 5962 | `	pCallback = apArg[nArg - 1];` |
|         - | 5963 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 5964 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 5965 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5966 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 5967 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 5968 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 5969 | `			 */` |
|         9 | 5970 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 5971 | `			if( pCb->nEntry != 2 ){` |
|         4 | 5972 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5973 | `					"TypeError",` |
|         - | 5974 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5975 | `					nArg` |
|         - | 5976 | `					);` |
|         - | 5977 | `			}` |
|         - | 5978 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 5979 | `			{` |
|         6 | 5980 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 5981 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 5982 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 5983 | `					int nMethodLen;` |
|         6 | 5984 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 5985 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 5986 | `					if( pClass ){` |
|         - | 5987 | `						/* Class exists but method is missing. */` |
|         4 | 5988 | `						return PH7_VmThrowException(pCtx,` |
|         - | 5989 | `							"TypeError",` |
|         - | 5990 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 5991 | `							nArg,` |
|         1 | 5992 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 5993 | `							zMethod` |
|         - | 5994 | `							);` |
|         - | 5995 | `					}` |
|         - | 5996 | `					/* Class not found */` |
|         - | 5997 | `					{` |
|         - | 5998 | `						int nName;` |
|         3 | 5999 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6000 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6001 | `							"TypeError",` |
|         - | 6002 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6003 | `							nArg,` |
|         1 | 6004 | `							zName` |
|         - | 6005 | `							);` |
|         - | 6006 | `					}` |
|         - | 6007 | `				}` |
|         - | 6008 | `			}` |
|         - | 6009 | `			/* Fallback message */` |
|       ! 0 | 6010 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6011 | `				"TypeError",` |
|         - | 6012 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6013 | `				nArg` |
|         - | 6014 | `				);` |
|         - | 6015 | `		}` |
|         6 | 6016 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6017 | `			int len;` |
|         3 | 6018 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6019 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6020 | `				"TypeError",` |
|         - | 6021 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6022 | `				nArg,` |
|         1 | 6023 | `				zName` |
|         - | 6024 | `				);` |
|         - | 6025 | `		}` |
|         4 | 6026 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6027 | `			"TypeError",` |
|         - | 6028 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6029 | `			nArg` |
|         - | 6030 | `			);` |
|         - | 6031 | `	}` |
|         - | 6032 |  |
|        11 | 6033 | `	if( nArg == 2 ){` |
|         - | 6034 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6035 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6036 | `		return PH7_OK;` |
|         - | 6037 | `	}` |
|         - | 6038 |  |
|         - | 6039 | `	/* Create a new array */` |
|         7 | 6040 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6041 | `	if( pArray == 0 ){` |
|       ! 0 | 6042 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6043 | `		return PH7_OK;` |
|         - | 6044 | `	}` |
|         - | 6045 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6046 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6047 | `	/* Perform the intersection */` |
|         7 | 6048 | `	pEntry = pSrc->pFirst;` |
|         7 | 6049 | `	n = pSrc->nEntry;` |
|         7 | 6050 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6051 | `	for(;;){` |
|        19 | 6052 | `		if( n < 1 ){` |
|         5 | 6053 | `			break;` |
|         - | 6054 | `		}` |
|         - | 6055 | `		/* Extract the node value */` |
|        15 | 6056 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6057 | `		if( pVal ){` |
|        23 | 6058 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6059 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6060 | `					/* ignore */` |
|       ! 0 | 6061 | `					continue;` |
|         - | 6062 | `				}` |
|         - | 6063 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6064 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6065 | `				/* Perform the lookup */` |
|        15 | 6066 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6067 | `				if( rc != SXRET_OK ){` |
|         - | 6068 | `					/* Value does not exist */` |
|         7 | 6069 | `					break;` |
|         - | 6070 | `				}` |
|         5 | 6071 | `			}` |
|        15 | 6072 | `			if( i >= (nArg-1) ){` |
|         - | 6073 | `				/* Perform the insertion */` |
|         9 | 6074 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6075 | `			}` |
|         7 | 6076 | `		}` |
|        15 | 6077 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6078 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6079 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6080 | `			return PH7_EXCEPTION;` |
|         - | 6081 | `		}` |
|         - | 6082 | `		/* Point to the next entry */` |
|        13 | 6083 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6084 | `		n--;` |
|         1 | 6085 | `	}` |
|         - | 6086 | `	/* Return the freshly created array */` |
|         5 | 6087 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6088 | `	return PH7_OK;` |
|        18 | 6089 | `}` |
|         - | 6090 | `/*` |
|         - | 6091 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6092 | ` *  Fill an array with values.` |
|         - | 6093 | ` * Parameters` |
|         - | 6094 | ` *  $start_index` |
|         - | 6095 | ` *    The first index of the returned array.` |
|         - | 6096 | ` *  $num` |
|         - | 6097 | ` *   Number of elements to insert.` |
|         - | 6098 | ` *  $value` |
|         - | 6099 | ` *    Value to use for filling.` |
|         - | 6100 | ` * Return` |
|         - | 6101 | ` *  The filled array or null on failure.` |
|         - | 6102 | ` */` |
|       244 | 6103 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6104 | `{` |
|         - | 6105 | `	ph7_value *pArray;` |
|         - | 6106 | `	int i,nEntry;` |
|         - | 6107 |  |
|         - | 6108 | `	/* PHP enforces argument count and type checks. */` |
|       249 | 6109 | `	if( nArg != 3 ){` |
|         - | 6110 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         8 | 6111 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6112 | `			"ArgumentCountError",` |
|         - | 6113 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         2 | 6114 | `			nArg` |
|         - | 6115 | `			);` |
|         - | 6116 | `	}` |
|         - | 6117 |  |
|         - | 6118 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6119 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6120 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6121 | `	 * and NULLs are rejected outright. */` |
|       359 | 6122 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       362 | 6123 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|         4 | 6124 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6125 | `			"TypeError",` |
|         - | 6126 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|         1 | 6127 | `			ph7_type_name(apArg[0])` |
|         - | 6128 | `			);` |
|         - | 6129 | `	}` |
|       242 | 6130 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6131 | `		int len;` |
|         8 | 6132 | `		sxu8 bReal = FALSE;` |
|         8 | 6133 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6134 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6135 | `			/* Non‑numeric string is an error. */` |
|         3 | 6136 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6137 | `				"TypeError",` |
|         - | 6138 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6139 | `				);` |
|         - | 6140 | `		}` |
|         5 | 6141 | `		if( bReal ){` |
|         - | 6142 | `			/* float-string -> deprecation warning */` |
|         4 | 6143 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6144 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6145 | `				zStr` |
|         - | 6146 | `				);` |
|         1 | 6147 | `		}` |
|         2 | 6148 | `	}` |
|         - | 6149 |  |
|         - | 6150 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6151 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6152 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6153 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6154 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6155 | `			"TypeError",` |
|         - | 6156 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6157 | `			ph7_type_name(apArg[1])` |
|         - | 6158 | `			);` |
|         - | 6159 | `	}` |
|       239 | 6160 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6161 | `		int len;` |
|         3 | 6162 | `		sxu8 bReal = FALSE;` |
|         3 | 6163 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6164 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6165 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6166 | `				"TypeError",` |
|         - | 6167 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6168 | `				);` |
|         - | 6169 | `		}` |
|       ! 0 | 6170 | `	}` |
|         - | 6171 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6172 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6173 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6174 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6175 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6176 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6177 | `		if( d != (double)i64 ){` |
|         7 | 6178 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6179 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6180 | `				d` |
|         - | 6181 | `				);` |
|         2 | 6182 | `		}` |
|         2 | 6183 | `	}` |
|         - | 6184 |  |
|         - | 6185 | `	/* Total number of entries to insert */` |
|       236 | 6186 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6187 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6188 | `	if( nEntry < 0 ){` |
|         3 | 6189 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6190 | `			"ValueError",` |
|         - | 6191 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6192 | `			);` |
|         - | 6193 | `	}` |
|         - | 6194 |  |
|         - | 6195 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6196 | `	if( nEntry == 0 ){` |
|         7 | 6197 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6198 | `		return PH7_OK;` |
|         - | 6199 | `	}` |
|         - | 6200 |  |
|         - | 6201 | `	/* Create a new array */` |
|       227 | 6202 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6203 | `	if( pArray == 0 ){` |
|       ! 0 | 6204 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6205 | `	}` |
|         - | 6206 |  |
|         - | 6207 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6208 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6209 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6210 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6211 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6212 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6213 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6214 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6215 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6216 | `		}` |
|   1058803 | 6217 | `	}` |
|         - | 6218 | `	/* Return the filled array */` |
|       227 | 6219 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6220 | `	return PH7_OK;` |
|       127 | 6221 | `}` |
|         - | 6222 | `/*` |
|         - | 6223 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6224 | ` *  Fill an array with values, specifying keys.` |
|         - | 6225 | ` * Parameters` |
|         - | 6226 | ` *  $input` |
|         - | 6227 | ` *   Array of values that will be used as key.` |
|         - | 6228 | ` *  $value` |
|         - | 6229 | ` *    Value to use for filling.` |
|         - | 6230 | ` * Return` |
|         - | 6231 | ` *  The filled array.` |
|         - | 6232 | ` * Throws` |
|         - | 6233 | ` *  ValueError if $input is not an array.` |
|         - | 6234 | ` */` |
|        26 | 6235 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6236 | `{` |
|         - | 6237 | `	ph7_hashmap_node *pEntry;` |
|         - | 6238 | `	ph7_hashmap *pSrc;` |
|         - | 6239 | `	ph7_value *pArray;` |
|         - | 6240 | `	sxu32 n;` |
|         - | 6241 | `	/* PHP enforces exactly 2 arguments. */` |
|        31 | 6242 | `	if( nArg != 2 ){` |
|        12 | 6243 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6244 | `			"ArgumentCountError",` |
|         - | 6245 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         3 | 6246 | `			nArg` |
|         - | 6247 | `			);` |
|         - | 6248 | `	}` |
|         - | 6249 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6250 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6251 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6252 | `			"TypeError",` |
|         - | 6253 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6254 | `			ph7_type_name(apArg[0])` |
|         - | 6255 | `			);` |
|         - | 6256 | `	}` |
|         - | 6257 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6258 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6259 | `	/* Create a new array */` |
|        17 | 6260 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6261 | `	if( pArray == 0 ){` |
|       ! 0 | 6262 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6263 | `		return PH7_OK;` |
|         - | 6264 | `	}` |
|         - | 6265 | `	/* Perform the requested operation */` |
|        17 | 6266 | `	pEntry = pSrc->pFirst;` |
|        45 | 6267 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6268 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6269 | `		/* Point to the next entry */` |
|        29 | 6270 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6271 | `	}` |
|         - | 6272 | `	/* Return the filled array */` |
|        17 | 6273 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6274 | `	return PH7_OK;` |
|        18 | 6275 | `}` |
|         - | 6276 | `/*` |
|         - | 6277 | ` * array array_combine(array $keys,array $values)` |
|         - | 6278 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6279 | ` * Parameters` |
|         - | 6280 | ` *  $keys` |
|         - | 6281 | ` *    Array of keys to be used.` |
|         - | 6282 | ` * $values` |
|         - | 6283 | ` *   Array of values to be used.` |
|         - | 6284 | ` * Return` |
|         - | 6285 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6286 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6287 | ` *  not an array.` |
|         - | 6288 | ` */` |
|        18 | 6289 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6290 | `{` |
|         - | 6291 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6292 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6293 | `	ph7_value *pArray;` |
|         - | 6294 | `	sxu32 n;` |
|         - | 6295 | `	/* PHP enforces argument count and type checks. */` |
|        23 | 6296 | `	if( nArg != 2 ){` |
|         - | 6297 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6298 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6299 | `			"ArgumentCountError",` |
|         - | 6300 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|         1 | 6301 | `			nArg` |
|         - | 6302 | `			);` |
|         - | 6303 | `	}` |
|         - | 6304 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6305 | `	 * argument index in the error message. */` |
|        20 | 6306 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6307 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6308 | `			"TypeError",` |
|         - | 6309 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6310 | `			ph7_type_name(apArg[0])` |
|         - | 6311 | `			);` |
|         - | 6312 | `	}` |
|        17 | 6313 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6314 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6315 | `			"TypeError",` |
|         - | 6316 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6317 | `			ph7_type_name(apArg[1])` |
|         - | 6318 | `			);` |
|         - | 6319 | `	}` |
|         - | 6320 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6321 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6322 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6323 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6324 | `		/* Length mismatch -> ValueError */` |
|         3 | 6325 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6326 | `			"ValueError",` |
|         - | 6327 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6328 | `			);` |
|         - | 6329 | `	}` |
|         - | 6330 | `	/* Create a new array */` |
|        11 | 6331 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6332 | `	if( pArray == 0 ){` |
|       ! 0 | 6333 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6334 | `		return PH7_OK;` |
|         - | 6335 | `	}` |
|         - | 6336 | `	/* Perform the requested operation */` |
|        11 | 6337 | `	pKe = pKey->pFirst;` |
|        11 | 6338 | `	pVe = pValue->pFirst;` |
|        33 | 6339 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6340 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6341 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6342 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6343 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6344 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6345 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6346 | `		 * original array must not be mutated. */` |
|        23 | 6347 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6348 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6349 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6350 | `			if( pTmpKey ){` |
|         5 | 6351 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6352 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6353 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6354 | `				pKeyCopy = pTmpKey;` |
|         2 | 6355 | `			}` |
|         2 | 6356 | `		}` |
|        23 | 6357 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6358 | `		/* Point to the next entry */` |
|        23 | 6359 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6360 | `		pVe = pVe->pPrev;` |
|        12 | 6361 | `	}` |
|         - | 6362 | `	/* Return the filled array */` |
|        11 | 6363 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6364 | `	return PH7_OK;` |
|        14 | 6365 | `}` |
|         - | 6366 | `/*` |
|         - | 6367 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6368 | ` *  Return an array with elements in reverse order.` |
|         - | 6369 | ` * Parameters` |
|         - | 6370 | ` *  $array` |
|         - | 6371 | ` *   The input array.` |
|         - | 6372 | ` *  $preserve_keys (optional)` |
|         - | 6373 | ` *   If set to TRUE keys are preserved.` |
|         - | 6374 | ` * Return` |
|         - | 6375 | ` *  The reversed array.` |
|         - | 6376 | ` */` |
|        20 | 6377 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6378 | `{` |
|         - | 6379 | `	ph7_hashmap_node *pEntry;` |
|         - | 6380 | `	ph7_hashmap *pSrc;` |
|         - | 6381 | `	ph7_value *pArray;` |
|         - | 6382 | `	int bPreserve;` |
|         - | 6383 | `	sxu32 n;` |
|        23 | 6384 | `	if( nArg < 1 ){` |
|         4 | 6385 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6386 | `			"ArgumentCountError",` |
|         - | 6387 | `			"array_reverse() expects at least 1 argument, %d given",` |
|         1 | 6388 | `			nArg` |
|         - | 6389 | `			);` |
|         - | 6390 | `	}` |
|         - | 6391 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6392 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6393 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6394 | `			"TypeError",` |
|         - | 6395 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6396 | `			ph7_type_name(apArg[0])` |
|         - | 6397 | `			);` |
|         - | 6398 | `	}` |
|        17 | 6399 | `	bPreserve = FALSE;` |
|        17 | 6400 | `	if( nArg > 1 ){` |
|         7 | 6401 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6402 | `	}` |
|         - | 6403 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6404 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6405 | `	/* Create a new array */` |
|        17 | 6406 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6407 | `	if( pArray == 0 ){` |
|       ! 0 | 6408 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6409 | `		return PH7_OK;` |
|         - | 6410 | `	}` |
|         - | 6411 | `	/* Perform the requested operation */` |
|        17 | 6412 | `	pEntry = pSrc->pLast;` |
|        55 | 6413 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6414 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6415 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6416 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6417 | `		/* Point to the previous entry */` |
|        39 | 6418 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6419 | `	}` |
|        17 | 6420 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6421 | `	return PH7_OK;` |
|        13 | 6422 | `}` |
|         - | 6423 | `/*` |
|         - | 6424 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6425 | ` *  Removes duplicate values from an array.` |
|         - | 6426 | ` * Parameters` |
|         - | 6427 | ` *  $array` |
|         - | 6428 | ` *   The input array.` |
|         - | 6429 | ` *  $flags` |
|         - | 6430 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6431 | ` *   behavior using these values:` |
|         - | 6432 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6433 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6434 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6435 | ` * Return` |
|         - | 6436 | ` *  The filtered array.` |
|         - | 6437 | ` */` |
|        24 | 6438 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6439 | `{` |
|         - | 6440 | `	ph7_hashmap_node *pEntry;` |
|         - | 6441 | `	ph7_value *pNeedle;` |
|         - | 6442 | `	ph7_hashmap *pSrc;` |
|         - | 6443 | `	ph7_value *pArray;` |
|         - | 6444 | `	int bStrict;` |
|         - | 6445 | `	sxi32 rc;` |
|         - | 6446 | `	sxu32 n;` |
|        28 | 6447 | `	if( nArg < 1 ){` |
|         - | 6448 | `		/* Missing arguments, throw ArgumentCountError */` |
|         3 | 6449 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6450 | `			"ArgumentCountError",` |
|         - | 6451 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6452 | `			);` |
|         - | 6453 | `	}` |
|        25 | 6454 | `	if( nArg > 2 ){` |
|         - | 6455 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6456 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6457 | `			"ArgumentCountError",` |
|         - | 6458 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6459 | `			nArg` |
|         - | 6460 | `			);` |
|         - | 6461 | `	}` |
|         - | 6462 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6463 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6464 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6465 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6466 | `			"TypeError",` |
|         - | 6467 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6468 | `			ph7_type_name(apArg[0])` |
|         - | 6469 | `			);` |
|         - | 6470 | `	}` |
|        19 | 6471 | `	bStrict = FALSE;` |
|         - | 6472 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6473 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6474 | `	/* Create a new array */` |
|        19 | 6475 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6476 | `	if( pArray == 0 ){` |
|       ! 0 | 6477 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6478 | `		return PH7_OK;` |
|         - | 6479 | `	}` |
|         - | 6480 | `	/* Perform the requested operation */` |
|        19 | 6481 | `	pEntry = pSrc->pFirst;` |
|        83 | 6482 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6483 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6484 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6485 | `		if( pNeedle ){` |
|        65 | 6486 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6487 | `		}` |
|        65 | 6488 | `		if( rc != SXRET_OK ){` |
|         - | 6489 | `			/* Perform the insertion */` |
|        37 | 6490 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6491 | `		}` |
|         - | 6492 | `		/* Point to the next entry */` |
|        65 | 6493 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6494 | `	}` |
|         - | 6495 | `	/* Return the freshly created array */` |
|        19 | 6496 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6497 | `	return PH7_OK;` |
|        16 | 6498 | `}` |
|         - | 6499 | `/*` |
|         - | 6500 | ` * array array_flip(array $input)` |
|         - | 6501 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6502 | ` * Parameter` |
|         - | 6503 | ` *  $input` |
|         - | 6504 | ` *   Input array.` |
|         - | 6505 | ` * Return` |
|         - | 6506 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6507 | ` */` |
|        34 | 6508 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6509 | `{` |
|         - | 6510 | `	ph7_hashmap_node *pEntry;` |
|         - | 6511 | `	ph7_hashmap *pSrc;` |
|         - | 6512 | `	ph7_value *pArray;` |
|         - | 6513 | `	ph7_value *pKey;` |
|         - | 6514 | `	ph7_value sVal;` |
|         - | 6515 | `	sxu32 n;` |
|         - | 6516 |  |
|         - | 6517 | `	/* PHP requires exactly one argument */` |
|        39 | 6518 | `	if( nArg != 1 ){` |
|         - | 6519 | `		/* Use ArgumentCountError like other array helpers */` |
|         8 | 6520 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6521 | `			"ArgumentCountError",` |
|         - | 6522 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         2 | 6523 | `			nArg` |
|         - | 6524 | `			);` |
|         - | 6525 | `	}` |
|         - | 6526 | `	/* Make sure we are dealing with a valid hashmap */` |
|        33 | 6527 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6528 | `		/* Type mismatch -> TypeError */` |
|         8 | 6529 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6530 | `			"TypeError",` |
|         - | 6531 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6532 | `			ph7_type_name(apArg[0])` |
|         - | 6533 | `			);` |
|         - | 6534 | `	}` |
|         - | 6535 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6536 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6537 | `	/* Create a new array */` |
|        27 | 6538 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6539 | `	if( pArray == 0 ){` |
|       ! 0 | 6540 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6541 | `		return PH7_OK;` |
|         - | 6542 | `	}` |
|         - | 6543 | `	/* Start processing */` |
|        27 | 6544 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6545 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6546 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6547 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6548 | `		if( pKey ){` |
|         - | 6549 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6550 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6551 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6552 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6553 | `					);` |
|     22236 | 6554 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6555 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6556 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6557 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6558 | `				}else{` |
|         - | 6559 | `					SyString sStr;` |
|      2227 | 6560 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6561 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6562 | `				}` |
|         - | 6563 | `				/* Perform the insertion */` |
|     22227 | 6564 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6565 | `				/* Safely release the value because each inserted entry` |
|         - | 6566 | `				 * has its own private copy of the value.` |
|         - | 6567 | `				 */` |
|     22227 | 6568 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6569 | `			}else{` |
|         - | 6570 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6571 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6572 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6573 | `					);` |
|         - | 6574 | `			}` |
|     11118 | 6575 | `		}` |
|         - | 6576 | `		/* Point to the next entry */` |
|     22237 | 6577 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6578 | `	}` |
|         - | 6579 | `	/* Return the freshly created array */` |
|        27 | 6580 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6581 | `	return PH7_OK;` |
|        22 | 6582 | `}` |
|         - | 6583 | `/*` |
|         - | 6584 | ` * number array_sum(array $array )` |
|         - | 6585 | ` *  Calculate the sum of values in an array.` |
|         - | 6586 | ` * Parameters` |
|         - | 6587 | ` *  $array: The input array.` |
|         - | 6588 | ` * Return` |
|         - | 6589 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6590 | ` */` |
|        24 | 6591 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6592 | `{` |
|         - | 6593 | `	ph7_hashmap_node *pEntry;` |
|         - | 6594 | `	ph7_value *pObj;` |
|        25 | 6595 | `	double dSum = 0;` |
|         - | 6596 | `	sxu32 n;` |
|        25 | 6597 | `	pEntry = pMap->pFirst;` |
|        91 | 6598 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        67 | 6599 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        67 | 6600 | `		if( pObj ){` |
|        67 | 6601 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        29 | 6602 | `				dSum += pObj->rVal;` |
|        53 | 6603 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6604 | `				dSum += (double)pObj->x.iVal;` |
|        29 | 6605 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        15 | 6606 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6607 | `					double dv = 0;` |
|        13 | 6608 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6609 | `					dSum += dv;` |
|         7 | 6610 | `				}` |
|        12 | 6611 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6612 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6613 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6614 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6615 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6616 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6617 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6618 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6619 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6620 | `			}` |
|         - | 6621 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6622 | `		}` |
|         - | 6623 | `		/* Point to the next entry */` |
|        67 | 6624 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        34 | 6625 | `	}` |
|         - | 6626 | `	/* Return sum */` |
|        25 | 6627 | `	ph7_result_double(pCtx,dSum);` |
|        25 | 6628 | `}` |
|       680 | 6629 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6630 | `{` |
|         - | 6631 | `	ph7_hashmap_node *pEntry;` |
|         - | 6632 | `	ph7_value *pObj;` |
|       682 | 6633 | `	sxi64 nSum = 0;` |
|         - | 6634 | `	sxu32 n;` |
|       682 | 6635 | `	pEntry = pMap->pFirst;` |
|      4672 | 6636 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      3992 | 6637 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      3992 | 6638 | `		if( pObj ){` |
|      3992 | 6639 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3982 | 6640 | `				nSum += pObj->x.iVal;` |
|      2001 | 6641 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         7 | 6642 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         5 | 6643 | `					sxi64 nv = 0;` |
|         5 | 6644 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         5 | 6645 | `					nSum += nv;` |
|         3 | 6646 | `				}` |
|         8 | 6647 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6648 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6649 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6650 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6651 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6652 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6653 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6654 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6655 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6656 | `			}` |
|         - | 6657 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      1995 | 6658 | `		}` |
|         - | 6659 | `		/* Point to the next entry */` |
|      3992 | 6660 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      1997 | 6661 | `	}` |
|         - | 6662 | `	/* Return sum */` |
|       682 | 6663 | `	ph7_result_int64(pCtx,nSum);` |
|       682 | 6664 | `}` |
|         - | 6665 | `/* number array_sum(array $array )` |
|         - | 6666 | ` * (See block-coment above)` |
|         - | 6667 | ` */` |
|       718 | 6668 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6669 | `{` |
|         - | 6670 | `	ph7_hashmap_node *pEntry;` |
|         - | 6671 | `	ph7_hashmap *pMap;` |
|         - | 6672 | `	ph7_value *pObj;` |
|       723 | 6673 | `	int useDouble = 0;` |
|         - | 6674 | `	sxu32 n;` |
|         - | 6675 | `	/* PHP requires exactly one argument */` |
|       723 | 6676 | `	if( nArg != 1 ){` |
|         8 | 6677 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6678 | `			"ArgumentCountError",` |
|         - | 6679 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         2 | 6680 | `			nArg` |
|         - | 6681 | `			);` |
|         - | 6682 | `	}` |
|         - | 6683 | `	/* Make sure we are dealing with a valid hashmap */` |
|       717 | 6684 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6685 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6686 | `		char zBuf[64];` |
|         8 | 6687 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6688 | `			"TypeError",` |
|         - | 6689 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6690 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6691 | `			);` |
|         - | 6692 | `	}` |
|       712 | 6693 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       712 | 6694 | `	if( pMap->nEntry < 1 ){` |
|         - | 6695 | `		/* Nothing to compute,return 0 */` |
|         7 | 6696 | `		ph7_result_int(pCtx,0);` |
|         7 | 6697 | `		return PH7_OK;` |
|         - | 6698 | `	}` |
|         - | 6699 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6700 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6701 | `	 */` |
|       706 | 6702 | `	pEntry = pMap->pFirst;` |
|      4704 | 6703 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4024 | 6704 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4024 | 6705 | `		if( pObj ){` |
|      4024 | 6706 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        19 | 6707 | `				useDouble = 1;` |
|        19 | 6708 | `				break;` |
|         - | 6709 | `			}` |
|      4006 | 6710 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        13 | 6711 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        13 | 6712 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6713 | `				sxu32 i;` |
|        23 | 6714 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        17 | 6715 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6716 | `						useDouble = 1;` |
|         7 | 6717 | `						break;` |
|         - | 6718 | `					}` |
|         6 | 6719 | `				}` |
|        13 | 6720 | `				if( useDouble ){` |
|         7 | 6721 | `					break;` |
|         - | 6722 | `				}` |
|         3 | 6723 | `			}` |
|      1999 | 6724 | `		}` |
|      4000 | 6725 | `		pEntry = pEntry->pPrev;` |
|      2001 | 6726 | `	}` |
|       706 | 6727 | `	if( useDouble ){` |
|        25 | 6728 | `		DoubleSum(pCtx,pMap);` |
|        13 | 6729 | `	}else{` |
|       682 | 6730 | `		Int64Sum(pCtx,pMap);` |
|         - | 6731 | `	}` |
|       706 | 6732 | `	return PH7_OK;` |
|       364 | 6733 | `}` |
|         - | 6734 | `/*` |
|         - | 6735 | ` * number array_product(array $array )` |
|         - | 6736 | ` *  Calculate the product of values in an array.` |
|         - | 6737 | ` * Parameters` |
|         - | 6738 | ` *  $array: The input array.` |
|         - | 6739 | ` * Return` |
|         - | 6740 | ` *  Returns the product of values as an integer or float.` |
|         - | 6741 | ` */` |
|         2 | 6742 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6743 | `{` |
|         - | 6744 | `	ph7_hashmap_node *pEntry;` |
|         - | 6745 | `	ph7_value *pObj;` |
|         - | 6746 | `	double dProd;` |
|         - | 6747 | `	sxu32 n;` |
|         3 | 6748 | `	pEntry = pMap->pFirst;` |
|         3 | 6749 | `	dProd = 1;` |
|         7 | 6750 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6751 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6752 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6753 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6754 | `				dProd *= pObj->rVal;` |
|         4 | 6755 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6756 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6757 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6758 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6759 | `					double dv = 0;` |
|       ! 0 | 6760 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6761 | `					dProd *= dv;` |
|       ! 0 | 6762 | `				}` |
|       ! 0 | 6763 | `			}` |
|         2 | 6764 | `		}` |
|         - | 6765 | `		/* Point to the next entry */` |
|         5 | 6766 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6767 | `	}` |
|         - | 6768 | `	/* Return product */` |
|         3 | 6769 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6770 | `}` |
|         2 | 6771 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6772 | `{` |
|         - | 6773 | `	ph7_hashmap_node *pEntry;` |
|         - | 6774 | `	ph7_value *pObj;` |
|         - | 6775 | `	sxi64 nProd;` |
|         - | 6776 | `	sxu32 n;` |
|         3 | 6777 | `	pEntry = pMap->pFirst;` |
|         3 | 6778 | `	nProd = 1;` |
|         9 | 6779 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6780 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6781 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6782 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6783 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6784 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6785 | `				nProd *= pObj->x.iVal;` |
|         3 | 6786 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6787 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6788 | `					sxi64 nv = 0;` |
|       ! 0 | 6789 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6790 | `					nProd *= nv;` |
|       ! 0 | 6791 | `				}` |
|       ! 0 | 6792 | `			}` |
|         3 | 6793 | `		}` |
|         - | 6794 | `		/* Point to the next entry */` |
|         7 | 6795 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6796 | `	}` |
|         - | 6797 | `	/* Return product */` |
|         3 | 6798 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6799 | `}` |
|         - | 6800 | `/* number array_product(array $array )` |
|         - | 6801 | ` * (See block-block comment above)` |
|         - | 6802 | ` */` |
|        18 | 6803 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6804 | `{` |
|         - | 6805 | `	ph7_hashmap *pMap;` |
|         - | 6806 | `	ph7_value *pObj;` |
|        19 | 6807 | `	if( nArg < 1 ){` |
|         - | 6808 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6809 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6810 | `		return PH7_OK;` |
|         - | 6811 | `	}` |
|         - | 6812 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        19 | 6813 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6814 | `		char zBuf[64];` |
|        19 | 6815 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6816 | `			"TypeError",` |
|         - | 6817 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 6818 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6819 | `			);` |
|         - | 6820 | `	}` |
|         7 | 6821 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6822 | `	if( pMap->nEntry < 1 ){` |
|         - | 6823 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6824 | `		ph7_result_int(pCtx,1);` |
|         3 | 6825 | `		return PH7_OK;` |
|         - | 6826 | `	}` |
|         - | 6827 | `	/* If the first element is of type float,then perform floating` |
|         - | 6828 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6829 | `	 */` |
|         5 | 6830 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6831 | `	if( pObj == 0 ){` |
|       ! 0 | 6832 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6833 | `		return PH7_OK;` |
|         - | 6834 | `	}` |
|         5 | 6835 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6836 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6837 | `	}else{` |
|         3 | 6838 | `		Int64Prod(pCtx,pMap);` |
|         - | 6839 | `	}` |
|         5 | 6840 | `	return PH7_OK;` |
|        10 | 6841 | `}` |
|         - | 6842 | `/*` |
|         - | 6843 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6844 | ` *  Pick one or more random entries out of an array.` |
|         - | 6845 | ` * Parameters` |
|         - | 6846 | ` * $input` |
|         - | 6847 | ` *  The input array.` |
|         - | 6848 | ` * $num_req` |
|         - | 6849 | ` *  Specifies how many entries you want to pick.` |
|         - | 6850 | ` * Return` |
|         - | 6851 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6852 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6853 | ` *  NULL is returned on failure.` |
|         - | 6854 | ` */` |
|        42 | 6855 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6856 | `{` |
|         - | 6857 | `	ph7_hashmap_node *pNode;` |
|         - | 6858 | `	ph7_hashmap *pMap;` |
|        43 | 6859 | `	int nItem = 1;` |
|        43 | 6860 | `	if( nArg < 1 ){` |
|         - | 6861 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6862 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6863 | `		return PH7_OK;` |
|         - | 6864 | `	}` |
|         - | 6865 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        43 | 6866 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6867 | `		char zBuf[64];` |
|        10 | 6868 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6869 | `			"TypeError",` |
|         - | 6870 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6871 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6872 | `			);` |
|         - | 6873 | `	}` |
|         - | 6874 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6875 | `	 * check, matching its ZPP-before-body ordering. */` |
|        37 | 6876 | `	if( nArg > 1 ){` |
|        29 | 6877 | `		ph7_value *pNum = apArg[1];` |
|        28 | 6878 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        24 | 6879 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6880 | `			char zBuf[64];` |
|        10 | 6881 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6882 | `				"TypeError",` |
|         - | 6883 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|         3 | 6884 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6885 | `				);` |
|         - | 6886 | `		}` |
|        23 | 6887 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6888 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6889 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6890 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6891 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6892 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6893 | `			int len;` |
|         9 | 6894 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6895 | `			sxi64 iLong; double dReal;` |
|         9 | 6896 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6897 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6898 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6899 | `					"TypeError",` |
|         - | 6900 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6901 | `					);` |
|         - | 6902 | `			}` |
|         - | 6903 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6904 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6905 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6906 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6907 | `			}` |
|         3 | 6908 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6909 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6910 | `			nItem = (int)iLong;` |
|         2 | 6911 | `		}else{` |
|        15 | 6912 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6913 | `		}` |
|         8 | 6914 | `	}` |
|         - | 6915 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6916 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6917 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6918 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6919 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6920 | `			"ValueError",` |
|         - | 6921 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 6922 | `			);` |
|         - | 6923 | `	}` |
|         - | 6924 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 6925 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 6926 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6927 | `			"ValueError",` |
|         - | 6928 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 6929 | `			);` |
|         - | 6930 | `	}` |
|        13 | 6931 | `	if( nItem < 2 ){` |
|         - | 6932 | `		sxu32 nEntry;` |
|         - | 6933 | `		/* Select a random number */` |
|         9 | 6934 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 6935 | `		/* Extract the desired entry.` |
|         - | 6936 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 6937 | `		 */` |
|         9 | 6938 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         4 | 6939 | `			pNode = pMap->pLast;` |
|         4 | 6940 | `			nEntry = pMap->nEntry - nEntry;` |
|         4 | 6941 | `			if( nEntry > 1 ){` |
|       ! 0 | 6942 | `				for(;;){` |
|       ! 0 | 6943 | `					if( nEntry == 0 ){` |
|       ! 0 | 6944 | `						break;` |
|         - | 6945 | `					}` |
|         - | 6946 | `					/* Point to the previous entry */` |
|       ! 0 | 6947 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 6948 | `					nEntry--;` |
|       ! 0 | 6949 | `				}` |
|       ! 0 | 6950 | `			}` |
|         3 | 6951 | `		}else{` |
|         6 | 6952 | `			pNode = pMap->pFirst;` |
|         4 | 6953 | `			for(;;){` |
|         9 | 6954 | `				if( nEntry == 0 ){` |
|         6 | 6955 | `					break;` |
|         - | 6956 | `				}` |
|         - | 6957 | `				/* Point to the next entry */` |
|         4 | 6958 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         4 | 6959 | `				nEntry--;` |
|         1 | 6960 | `			}` |
|         - | 6961 | `		}` |
|         9 | 6962 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 6963 | `			/* Int key */` |
|         7 | 6964 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 6965 | `		}else{` |
|         - | 6966 | `			/* Blob key */` |
|         3 | 6967 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 6968 | `		}` |
|         5 | 6969 | `	}else{` |
|         - | 6970 | `		ph7_value sKey,*pArray;` |
|         - | 6971 | `		ph7_hashmap *pDest;` |
|         - | 6972 | `		/* Create a new array */` |
|         5 | 6973 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 6974 | `		if( pArray == 0 ){` |
|       ! 0 | 6975 | `			ph7_result_null(pCtx);` |
|       ! 0 | 6976 | `			return PH7_OK;` |
|         - | 6977 | `		}` |
|         - | 6978 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 6979 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 6980 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 6981 | `		/* Copy the first n items */` |
|         5 | 6982 | `		pNode = pMap->pFirst;` |
|         5 | 6983 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 6984 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 6985 | `		}` |
|        15 | 6986 | `		while( nItem > 0){` |
|        11 | 6987 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 6988 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 6989 | `			PH7_MemObjRelease(&sKey);` |
|         - | 6990 | `			/* Point to the next entry */` |
|        11 | 6991 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 6992 | `			nItem--;` |
|         1 | 6993 | `		}` |
|         - | 6994 | `		/* Shuffle the array */` |
|         5 | 6995 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 6996 | `		/* Rehash node */` |
|         5 | 6997 | `		HashmapSortRehash(pDest);` |
|         - | 6998 | `		/* Return the random array */` |
|         5 | 6999 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7000 | `	}` |
|        13 | 7001 | `	return PH7_OK;` |
|        22 | 7002 | `}` |
|         - | 7003 | `/*` |
|         - | 7004 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7005 | ` *  Split an array into chunks.` |
|         - | 7006 | ` * Parameters` |
|         - | 7007 | ` * $input` |
|         - | 7008 | ` *   The array to work on` |
|         - | 7009 | ` * $size` |
|         - | 7010 | ` *   The size of each chunk` |
|         - | 7011 | ` * $preserve_keys` |
|         - | 7012 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7013 | ` *   the chunk numerically.` |
|         - | 7014 | ` * Return` |
|         - | 7015 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7016 | ` *  zero, with each dimension containing size elements.` |
|         - | 7017 | ` */` |
|        42 | 7018 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7019 | `{` |
|         - | 7020 | `	ph7_value *pArray,*pChunk;` |
|         - | 7021 | `	ph7_hashmap_node *pEntry;` |
|         - | 7022 | `	ph7_hashmap *pMap;` |
|         - | 7023 | `	int bPreserve;` |
|         - | 7024 | `	sxu32 nChunk;` |
|         - | 7025 | `	sxu32 nSize;` |
|         - | 7026 | `	sxu32 n;` |
|         - | 7027 | `	/* Argument count and types follow PHP semantics. */` |
|        47 | 7028 | `	if( nArg < 2 ){` |
|         - | 7029 | `		/* fewer than required arguments -> ArgumentCountError */` |
|         4 | 7030 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7031 | `			"ArgumentCountError",` |
|         - | 7032 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|         1 | 7033 | `			nArg` |
|         - | 7034 | `			);` |
|         - | 7035 | `	}` |
|        45 | 7036 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7037 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7038 | `			"TypeError",` |
|         - | 7039 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7040 | `			ph7_type_name(apArg[0])` |
|         - | 7041 | `			);` |
|         - | 7042 | `	}` |
|         - | 7043 | `	/* Create a new array */` |
|        43 | 7044 | `	pArray = ph7_context_new_array(pCtx);` |
|        43 | 7045 | `	if( pArray == 0 ){` |
|       ! 0 | 7046 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7047 | `		return PH7_OK;` |
|         - | 7048 | `	}` |
|         - | 7049 | `	/* Point to the internal representation of the input hashmap */` |
|        43 | 7050 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7051 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7052 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        57 | 7053 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        81 | 7054 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        38 | 7055 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7056 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7057 | `			"TypeError",` |
|         - | 7058 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7059 | `			ph7_type_name(apArg[1])` |
|         - | 7060 | `			);` |
|         - | 7061 | `	}` |
|         - | 7062 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7063 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7064 | `	 * precision and PHP emits a deprecation warning. */` |
|        43 | 7065 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7066 | `		int len;` |
|         3 | 7067 | `		sxu8 bReal = FALSE;` |
|         3 | 7068 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7069 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7070 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7071 | `				"TypeError",` |
|         - | 7072 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7073 | `				);` |
|         - | 7074 | `		}` |
|       ! 0 | 7075 | `		if( bReal ){` |
|         - | 7076 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7077 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7078 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7079 | `				zStr` |
|         - | 7080 | `				);` |
|       ! 0 | 7081 | `		}` |
|       ! 0 | 7082 | `	}` |
|         - | 7083 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7084 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7085 | `	 * later via ph7_value_to_int. */` |
|        40 | 7086 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7087 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7088 | `		sxi64 i = (sxi64)d;` |
|         3 | 7089 | `		if( d != (double)i ){` |
|         4 | 7090 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7091 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7092 | `				d` |
|         - | 7093 | `				);` |
|         1 | 7094 | `		}` |
|         1 | 7095 | `	}` |
|         - | 7096 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7097 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7098 | `	{` |
|        40 | 7099 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        40 | 7100 | `		if( nSizeSigned < 1 ){` |
|         - | 7101 | `			/* size <= 0 -> ValueError */` |
|         6 | 7102 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7103 | `				"ValueError",` |
|         - | 7104 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7105 | `				);` |
|         - | 7106 | `		}` |
|        35 | 7107 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7108 | `	}` |
|        35 | 7109 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7110 | `		/* Return the whole array */` |
|         3 | 7111 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7112 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7113 | `		return PH7_OK;` |
|         - | 7114 | `	}` |
|        33 | 7115 | `	bPreserve = 0;` |
|        33 | 7116 | `	if( nArg > 2 ){` |
|         - | 7117 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7118 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7119 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7120 | `		 * normally, matching PHP behaviour. */` |
|        35 | 7121 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        35 | 7122 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7123 | `			ph7_value_is_resource(apArg[2]) ){` |
|         8 | 7124 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7125 | `				"TypeError",` |
|         - | 7126 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|         4 | 7127 | `				ph7_type_name(apArg[2])` |
|         - | 7128 | `				);` |
|         - | 7129 | `		}` |
|        21 | 7130 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7131 | `	}` |
|         - | 7132 | `	/* Start processing */` |
|        27 | 7133 | `	pEntry = pMap->pFirst;` |
|        27 | 7134 | `	nChunk = 0;` |
|        27 | 7135 | `	pChunk = 0;` |
|        27 | 7136 | `	n = pMap->nEntry;` |
|        56 | 7137 | `	for( ;; ){` |
|       113 | 7138 | `		if( n < 1 ){` |
|         - | 7139 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7140 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7141 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7142 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7143 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7144 | `			 * exists. */` |
|        27 | 7145 | `			if( pChunk ){` |
|        27 | 7146 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7147 | `			}` |
|        27 | 7148 | `			break;` |
|         - | 7149 | `		}` |
|        87 | 7150 | `		if( nChunk < 1 ){` |
|        71 | 7151 | `			if( pChunk ){` |
|         - | 7152 | `				/* Put the first chunk */` |
|        45 | 7153 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7154 | `			}` |
|         - | 7155 | `			/* Create a new dimension */` |
|        71 | 7156 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7157 | `												   * will be automatically released as soon we return` |
|         - | 7158 | `												   * from this function */` |
|        71 | 7159 | `			if( pChunk == 0 ){` |
|       ! 0 | 7160 | `				break;` |
|         - | 7161 | `			}` |
|        71 | 7162 | `			nChunk = nSize;` |
|        35 | 7163 | `		}` |
|         - | 7164 | `		/* Insert the entry */` |
|        87 | 7165 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7166 | `		/* Point to the next entry */` |
|        87 | 7167 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7168 | `		nChunk--;` |
|        87 | 7169 | `		n--;` |
|         1 | 7170 | `	}` |
|         - | 7171 | `	/* Return the multidimensional array */` |
|        27 | 7172 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7173 | `	return PH7_OK;` |
|        26 | 7174 | `}` |
|         - | 7175 | `/*` |
|         - | 7176 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7177 | ` *  Pad array to the specified length with a value.` |
|         - | 7178 | ` * $input` |
|         - | 7179 | ` *   Initial array of values to pad.` |
|         - | 7180 | ` * $pad_size` |
|         - | 7181 | ` *   New size of the array.` |
|         - | 7182 | ` * $pad_value` |
|         - | 7183 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7184 | ` */` |
|         - | 7185 | `/*` |
|         - | 7186 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7187 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7188 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7189 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7190 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7191 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7192 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7193 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7194 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7195 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7196 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7197 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7198 | ` */` |
|        50 | 7199 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7200 | `	ph7_context *pCtx,` |
|         - | 7201 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7202 | `	int iArg,              /* 1-based argument position */` |
|         - | 7203 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7204 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7205 | `	)` |
|         1 | 7206 | `{` |
|        51 | 7207 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7208 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7209 | `			"ValueError",` |
|         - | 7210 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7211 | `			zFunc,iArg,zParam` |
|         - | 7212 | `			);` |
|         - | 7213 | `	}` |
|        37 | 7214 | `	return SXRET_OK;` |
|        26 | 7215 | `}` |
|        72 | 7216 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7217 | `{` |
|         - | 7218 | `	ph7_hashmap *pMap;` |
|         - | 7219 | `	ph7_value *pArray;` |
|         - | 7220 | `	sxi64 iLen,iAbs;` |
|         - | 7221 | `	int nEntry;` |
|         - | 7222 | `	sxi32 rc;` |
|        77 | 7223 | `	if( nArg != 3 ){` |
|        12 | 7224 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7225 | `			"ArgumentCountError",` |
|         - | 7226 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         3 | 7227 | `			nArg` |
|         - | 7228 | `			);` |
|         - | 7229 | `	}` |
|        68 | 7230 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7231 | `		char zBuf[64];` |
|        14 | 7232 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7233 | `			"TypeError",` |
|         - | 7234 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 7235 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7236 | `			);` |
|         - | 7237 | `	}` |
|         - | 7238 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7239 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7240 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7241 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        58 | 7242 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        56 | 7243 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7244 | `		char zBuf[64];` |
|         7 | 7245 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7246 | `			"TypeError",` |
|         - | 7247 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|         4 | 7248 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7249 | `			);` |
|         - | 7250 | `	}` |
|        55 | 7251 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7252 | `		int nStr;` |
|        11 | 7253 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7254 | `		sxi64 iLong; double dReal;` |
|        11 | 7255 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7256 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7257 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7258 | `				"TypeError",` |
|         - | 7259 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7260 | `				);` |
|         - | 7261 | `		}` |
|         7 | 7262 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7263 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7264 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7265 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7266 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7267 | `					"TypeError",` |
|         - | 7268 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7269 | `					);` |
|         - | 7270 | `			}` |
|         3 | 7271 | `			iLen = (sxi64)dReal;` |
|         3 | 7272 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7273 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7274 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7275 | `					zStr` |
|         - | 7276 | `					);` |
|       ! 0 | 7277 | `			}` |
|         2 | 7278 | `		}else{` |
|         5 | 7279 | `			iLen = iLong;` |
|         - | 7280 | `		}` |
|         4 | 7281 | `	}else{` |
|        45 | 7282 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7283 | `	}` |
|         - | 7284 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7285 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7286 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7287 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7288 | `	 * overflow). */` |
|        51 | 7289 | `	iAbs = iLen;` |
|        51 | 7290 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7291 | `		iAbs = -iAbs;` |
|         7 | 7292 | `	}` |
|        51 | 7293 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7294 | `	if( rc != SXRET_OK ){` |
|        15 | 7295 | `		return rc;` |
|         - | 7296 | `	}` |
|        37 | 7297 | `	nEntry = (int)iLen;` |
|         - | 7298 | `	/* Create a new array */` |
|        37 | 7299 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7300 | `	if( pArray == 0 ){` |
|       ! 0 | 7301 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7302 | `	}` |
|        37 | 7303 | `	if( nEntry < 0 ){` |
|        11 | 7304 | `		nEntry = -nEntry;` |
|        11 | 7305 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7306 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7307 | `			/* Insert given items first */` |
|        25 | 7308 | `			while( nEntry > 0 ){` |
|        19 | 7309 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7310 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7311 | `				}` |
|        19 | 7312 | `				nEntry--;` |
|         1 | 7313 | `			}` |
|         - | 7314 | `			/* Merge the two arrays */` |
|         7 | 7315 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7316 | `		}else{` |
|         5 | 7317 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7318 | `		}` |
|        32 | 7319 | `	}else if( nEntry > 0 ){` |
|        25 | 7320 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7321 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7322 | `			/* Merge the two arrays first */` |
|        19 | 7323 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7324 | `			/* Insert given items */` |
|       275 | 7325 | `			while( nEntry > 0 ){` |
|       257 | 7326 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7327 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7328 | `				}` |
|       257 | 7329 | `				nEntry--;` |
|         1 | 7330 | `			}` |
|        10 | 7331 | `		}else{` |
|         7 | 7332 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7333 | `		}` |
|        13 | 7334 | `	}else{` |
|         - | 7335 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7336 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7337 | `	}` |
|         - | 7338 | `	/* Return the new array */` |
|        37 | 7339 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7340 | `	return PH7_OK;` |
|        41 | 7341 | `}` |
|         - | 7342 | `/*` |
|         - | 7343 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7344 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7345 | ` * Parameters` |
|         - | 7346 | ` * $array` |
|         - | 7347 | ` *   The array in which elements are replaced.` |
|         - | 7348 | ` * $array1` |
|         - | 7349 | ` *   The array from which elements will be extracted.` |
|         - | 7350 | ` * ....` |
|         - | 7351 | ` *  More arrays from which elements will be extracted.` |
|         - | 7352 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7353 | ` * Return` |
|         - | 7354 | ` *  Returns an array.` |
|         - | 7355 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7356 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7357 | ` */` |
|        22 | 7358 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7359 | `{` |
|         - | 7360 | `	ph7_hashmap *pMap;` |
|         - | 7361 | `	ph7_value *pArray;` |
|         - | 7362 | `	int i;` |
|        26 | 7363 | `	if( nArg < 1 ){` |
|         3 | 7364 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7365 | `			"ArgumentCountError",` |
|         - | 7366 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7367 | `			);` |
|         - | 7368 | `	}` |
|        23 | 7369 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7370 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7371 | `			"TypeError",` |
|         - | 7372 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7373 | `			ph7_type_name(apArg[0])` |
|         - | 7374 | `			);` |
|         - | 7375 | `	}` |
|         - | 7376 | `	/* Create a new array */` |
|        20 | 7377 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7378 | `	if( pArray == 0 ){` |
|       ! 0 | 7379 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7380 | `		return PH7_OK;` |
|         - | 7381 | `	}` |
|         - | 7382 | `	/* Overwrite from the first array */` |
|        20 | 7383 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7384 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7385 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7386 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7387 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7388 | `			/* Type mismatch -> TypeError */` |
|         4 | 7389 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7390 | `				"TypeError",` |
|         - | 7391 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7392 | `				i + 1,` |
|         2 | 7393 | `				ph7_type_name(apArg[i])` |
|         - | 7394 | `				);` |
|         - | 7395 | `		}` |
|         - | 7396 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7397 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7398 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7399 | `	}` |
|         - | 7400 | `	/* Return the new array */` |
|        17 | 7401 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7402 | `	return PH7_OK;` |
|        15 | 7403 | `}` |
|         - | 7404 | `/*` |
|         - | 7405 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7406 | ` *  Filters elements of an array using a callback function.` |
|         - | 7407 | ` * Parameters` |
|         - | 7408 | ` *  $input` |
|         - | 7409 | ` *    The array to iterate over` |
|         - | 7410 | ` * $callback` |
|         - | 7411 | ` *    The callback function to use` |
|         - | 7412 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7413 | ` *    will be removed.` |
|         - | 7414 | ` * Return` |
|         - | 7415 | ` *  The filtered array.` |
|         - | 7416 | ` */` |
|        32 | 7417 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7418 | `{` |
|         - | 7419 | `	ph7_hashmap_node *pEntry;` |
|         - | 7420 | `	ph7_hashmap *pMap;` |
|         - | 7421 | `	ph7_value *pArray;` |
|         - | 7422 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7423 | `	ph7_value *pValue;` |
|         - | 7424 | `	sxi32 rc;` |
|         - | 7425 | `	int keep;` |
|         - | 7426 | `	sxu32 n;` |
|        34 | 7427 | `	if( nArg < 1 ){` |
|         - | 7428 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7429 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7430 | `		return PH7_OK;` |
|         - | 7431 | `	}` |
|         - | 7432 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        34 | 7433 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7434 | `		char zBuf[64];` |
|        22 | 7435 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7436 | `			"TypeError",` |
|         - | 7437 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         7 | 7438 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7439 | `			);` |
|         - | 7440 | `	}` |
|         - | 7441 | `	/* Create a new array */` |
|        20 | 7442 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7443 | `	if( pArray == 0 ){` |
|       ! 0 | 7444 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7445 | `		return PH7_OK;` |
|         - | 7446 | `	}` |
|         - | 7447 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7448 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7449 | `	pEntry = pMap->pFirst;` |
|        20 | 7450 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7451 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7452 | `	/* Perform the requested operation */` |
|        78 | 7453 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7454 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7455 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7456 | `		if( pValue == 0 ){` |
|         - | 7457 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7458 | `			keep = FALSE;` |
|        64 | 7459 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7460 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7461 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7462 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7463 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7464 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7465 | `					int len;` |
|         3 | 7466 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7467 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7468 | `						"TypeError",` |
|         - | 7469 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7470 | `						zName` |
|         - | 7471 | `						);` |
|       ! 0 | 7472 | `				}else{` |
|       ! 0 | 7473 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7474 | `						"TypeError",` |
|         - | 7475 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7476 | `						ph7_type_name(apArg[1])` |
|         - | 7477 | `						);` |
|         - | 7478 | `				}` |
|         - | 7479 | `			}` |
|        33 | 7480 | `			keep = FALSE;` |
|        33 | 7481 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7482 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7483 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7484 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7485 | `				return PH7_EXCEPTION;` |
|         - | 7486 | `			}` |
|        31 | 7487 | `			if( rc == SXRET_OK ){` |
|         - | 7488 | `				/* Perform a boolean cast */` |
|        31 | 7489 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7490 | `			}` |
|        31 | 7491 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7492 | `		}else{` |
|         - | 7493 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7494 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7495 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7496 | `			 */` |
|        29 | 7497 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7498 | `		}` |
|        59 | 7499 | `		if( keep ){` |
|         - | 7500 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7501 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7502 | `		}` |
|         - | 7503 | `		/* Point to the next entry */` |
|        59 | 7504 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7505 | `	}` |
|        15 | 7506 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7507 | `	return PH7_OK;` |
|        18 | 7508 | `}` |
|         - | 7509 | `/*` |
|         - | 7510 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7511 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7512 | ` * Parameters` |
|         - | 7513 | ` *  $callback` |
|         - | 7514 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7515 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7516 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7517 | ` *   are zipped together.` |
|         - | 7518 | ` *  $array` |
|         - | 7519 | ` *   The first array to run through the callback function.` |
|         - | 7520 | ` *  $arrays` |
|         - | 7521 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7522 | ` * Return` |
|         - | 7523 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7524 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7525 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7526 | ` *  padding shorter arrays with NULL.` |
|         - | 7527 | ` */` |
|        62 | 7528 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7529 | `{` |
|         - | 7530 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7531 | `	ph7_hashmap_node *pEntry;` |
|         - | 7532 | `	ph7_hashmap *pMap;` |
|         - | 7533 | `	ph7_vm *pVm;` |
|         - | 7534 | `	int bNullCallback;` |
|         - | 7535 | `	sxi32 rc;` |
|         - | 7536 | `	int i;` |
|         - | 7537 | `	sxu32 n;` |
|        67 | 7538 | `	if( nArg < 2 ){` |
|         8 | 7539 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7540 | `			"ArgumentCountError",` |
|         - | 7541 | `			"array_map() expects at least 2 arguments, %d given",` |
|         2 | 7542 | `			nArg` |
|         - | 7543 | `			);` |
|         - | 7544 | `	}` |
|        62 | 7545 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        62 | 7546 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7547 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7548 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7549 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7550 | `				"TypeError",` |
|         - | 7551 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7552 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7553 | `				zFunc` |
|         - | 7554 | `				);` |
|         - | 7555 | `		}` |
|         3 | 7556 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7557 | `			"TypeError",` |
|         - | 7558 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7559 | `			"no array or string given"` |
|         - | 7560 | `			);` |
|         - | 7561 | `	}` |
|         - | 7562 | `	/* Every remaining argument must be an array */` |
|       121 | 7563 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        69 | 7564 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7565 | `			if( i == 1 ){` |
|         4 | 7566 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7567 | `					"TypeError",` |
|         - | 7568 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7569 | `					ph7_type_name(apArg[1])` |
|         - | 7570 | `					);` |
|         - | 7571 | `			}` |
|       ! 0 | 7572 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7573 | `				"TypeError",` |
|         - | 7574 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7575 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7576 | `				);` |
|         - | 7577 | `		}` |
|        34 | 7578 | `	}` |
|        54 | 7579 | `	pVm = pCtx->pVm;` |
|         - | 7580 | `	/* Create a new array */` |
|        54 | 7581 | `	pArray = ph7_context_new_array(pCtx);` |
|        54 | 7582 | `	if( pArray == 0 ){` |
|       ! 0 | 7583 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7584 | `		return PH7_OK;` |
|         - | 7585 | `	}` |
|        54 | 7586 | `	PH7_MemObjInit(pVm,&sResult);` |
|        54 | 7587 | `	PH7_MemObjInit(pVm,&sKey);` |
|        54 | 7588 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7589 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7590 | `	if( nArg == 2 ){` |
|         - | 7591 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        44 | 7592 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        44 | 7593 | `		pEntry = pMap->pFirst;` |
|       134 | 7594 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7595 | `			/* Extract the node value */` |
|        96 | 7596 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        96 | 7597 | `			if( pValue ){` |
|         - | 7598 | `				/* Extract the node key */` |
|        96 | 7599 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        96 | 7600 | `				if( bNullCallback ){` |
|         - | 7601 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7602 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7603 | `				}else{` |
|         - | 7604 | `					/* Invoke the supplied callback */` |
|        86 | 7605 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        86 | 7606 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7607 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7608 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7609 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7610 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7611 | `						return PH7_EXCEPTION;` |
|         - | 7612 | `					}` |
|         - | 7613 | `					/* Insert the callback return value */` |
|        82 | 7614 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7615 | `				}` |
|        92 | 7616 | `				PH7_MemObjRelease(&sKey);` |
|        92 | 7617 | `				PH7_MemObjRelease(&sResult);` |
|        45 | 7618 | `			}` |
|         - | 7619 | `			/* Point to the next entry */` |
|        92 | 7620 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        47 | 7621 | `		}` |
|        21 | 7622 | `	}else{` |
|         - | 7623 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7624 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7625 | `		int nArrays = nArg - 1;` |
|         - | 7626 | `		ph7_hashmap_node **apCur;` |
|         - | 7627 | `		ph7_value **apCallArg;` |
|         - | 7628 | `		ph7_value sNull;` |
|        11 | 7629 | `		sxu32 nMax = 0;` |
|        11 | 7630 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7631 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7632 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7633 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7634 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7635 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7636 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7637 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7638 | `			return PH7_OK;` |
|         - | 7639 | `		}` |
|        11 | 7640 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7641 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7642 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7643 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7644 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7645 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7646 | `				nMax = pMap->nEntry;` |
|         6 | 7647 | `			}` |
|        12 | 7648 | `		}` |
|        35 | 7649 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7650 | `			ph7_value *pZip = 0;` |
|        25 | 7651 | `			if( bNullCallback ){` |
|         - | 7652 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7653 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7654 | `			}` |
|        79 | 7655 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7656 | `				ph7_value *pv = &sNull;` |
|        55 | 7657 | `				if( apCur[i] ){` |
|        53 | 7658 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7659 | `					if( pNodeVal ){` |
|        53 | 7660 | `						pv = pNodeVal;` |
|        26 | 7661 | `					}` |
|        53 | 7662 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7663 | `				}` |
|        55 | 7664 | `				if( bNullCallback ){` |
|         9 | 7665 | `					if( pZip ){` |
|         9 | 7666 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7667 | `					}` |
|         5 | 7668 | `				}else{` |
|        47 | 7669 | `					apCallArg[i] = pv;` |
|         - | 7670 | `				}` |
|        28 | 7671 | `			}` |
|        25 | 7672 | `			if( bNullCallback ){` |
|         5 | 7673 | `				if( pZip ){` |
|         5 | 7674 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7675 | `				}` |
|         3 | 7676 | `			}else{` |
|        21 | 7677 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7678 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7679 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7680 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7681 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7682 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7683 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7684 | `					return PH7_EXCEPTION;` |
|         - | 7685 | `				}` |
|        21 | 7686 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7687 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7688 | `			}` |
|        13 | 7689 | `		}` |
|        11 | 7690 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7691 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7692 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7693 | `	}` |
|        50 | 7694 | `	PH7_MemObjRelease(&sKey);` |
|        50 | 7695 | `	PH7_MemObjRelease(&sResult);` |
|        50 | 7696 | `	ph7_result_value(pCtx,pArray);` |
|        50 | 7697 | `	return PH7_OK;` |
|        36 | 7698 | `}` |
|         - | 7699 | `/*` |
|         - | 7700 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7701 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7702 | ` * Parameters` |
|         - | 7703 | ` *  $array` |
|         - | 7704 | ` *   The input array.` |
|         - | 7705 | ` *  $callback` |
|         - | 7706 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7707 | ` *  $initial` |
|         - | 7708 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7709 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7710 | ` * Return` |
|         - | 7711 | ` *  Returns the resulting value.` |
|         - | 7712 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7713 | ` */` |
|        34 | 7714 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7715 | `{` |
|         - | 7716 | `	ph7_hashmap_node *pEntry;` |
|         - | 7717 | `	ph7_hashmap *pMap;` |
|         - | 7718 | `	ph7_value *pValue;` |
|         - | 7719 | `	ph7_value sResult;` |
|         - | 7720 | `	sxi32 rc;` |
|         - | 7721 | `	sxu32 n;` |
|        39 | 7722 | `	if( nArg < 2 ){` |
|         8 | 7723 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7724 | `			"ArgumentCountError",` |
|         - | 7725 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|         2 | 7726 | `			nArg` |
|         - | 7727 | `			);` |
|         - | 7728 | `	}` |
|        35 | 7729 | `	if( nArg > 3 ){` |
|         4 | 7730 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7731 | `			"ArgumentCountError",` |
|         - | 7732 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7733 | `			nArg` |
|         - | 7734 | `			);` |
|         - | 7735 | `	}` |
|        33 | 7736 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7737 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7738 | `			"TypeError",` |
|         - | 7739 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7740 | `			ph7_type_name(apArg[0])` |
|         - | 7741 | `			);` |
|         - | 7742 | `	}` |
|        31 | 7743 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7744 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7745 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7746 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7747 | `				"TypeError",` |
|         - | 7748 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7749 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7750 | `				zFunc` |
|         - | 7751 | `				);` |
|         - | 7752 | `		}` |
|         9 | 7753 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7754 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7755 | `				"TypeError",` |
|         - | 7756 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7757 | `				"array callback must have exactly two members"` |
|         - | 7758 | `				);` |
|         - | 7759 | `		}` |
|         6 | 7760 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7761 | `			"TypeError",` |
|         - | 7762 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7763 | `			"no array or string given"` |
|         - | 7764 | `			);` |
|         - | 7765 | `	}` |
|         - | 7766 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7767 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7768 | `	/* Assume a NULL initial value */` |
|        19 | 7769 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7770 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7771 | `	if( nArg > 2 ){` |
|         - | 7772 | `		/* Set the initial value */` |
|        13 | 7773 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7774 | `	}` |
|         - | 7775 | `	/* Perform the requested operation */` |
|        19 | 7776 | `	pEntry = pMap->pFirst;` |
|        55 | 7777 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7778 | `		/* Extract the node value */` |
|        39 | 7779 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7780 | `		/* Invoke the supplied callback */` |
|        39 | 7781 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7782 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7783 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7784 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7785 | `			return PH7_EXCEPTION;` |
|         - | 7786 | `		}` |
|         - | 7787 | `		/* Point to the next entry */` |
|        37 | 7788 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7789 | `	}` |
|        17 | 7790 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7791 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7792 | `	return PH7_OK;` |
|        22 | 7793 | `}` |
|         - | 7794 | `/*` |
|         - | 7795 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7796 | ` *  Apply a user function to every member of an array.` |
|         - | 7797 | ` * Parameters` |
|         - | 7798 | ` *  $array` |
|         - | 7799 | ` *   The input array.` |
|         - | 7800 | ` *  $funcname` |
|         - | 7801 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7802 | ` *   the first, and the key/index second.` |
|         - | 7803 | ` * Note:` |
|         - | 7804 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7805 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7806 | ` *  be made in the original array itself.` |
|         - | 7807 | ` *  $userdata` |
|         - | 7808 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7809 | ` *   to the callback funcname.` |
|         - | 7810 | ` * Return` |
|         - | 7811 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7812 | ` */` |
|        38 | 7813 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7814 | `{` |
|         - | 7815 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7816 | `	ph7_hashmap_node *pEntry;` |
|         - | 7817 | `	ph7_hashmap *pMap;` |
|         - | 7818 | `	sxu32 n;` |
|        43 | 7819 | `	if( nArg < 2 ){` |
|         8 | 7820 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7821 | `			"ArgumentCountError",` |
|         - | 7822 | `			"array_walk() expects at least 2 arguments, %d given",` |
|         2 | 7823 | `			nArg` |
|         - | 7824 | `			);` |
|         - | 7825 | `	}` |
|        39 | 7826 | `	if( nArg > 3 ){` |
|         4 | 7827 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7828 | `			"ArgumentCountError",` |
|         - | 7829 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7830 | `			nArg` |
|         - | 7831 | `			);` |
|         - | 7832 | `	}` |
|        37 | 7833 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7834 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7835 | `			"TypeError",` |
|         - | 7836 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7837 | `			ph7_type_name(apArg[0])` |
|         - | 7838 | `			);` |
|         - | 7839 | `	}` |
|        35 | 7840 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7841 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7842 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7843 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7844 | `				"TypeError",` |
|         - | 7845 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7846 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7847 | `				zFunc` |
|         - | 7848 | `				);` |
|         - | 7849 | `		}` |
|        12 | 7850 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7851 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7852 | `				"TypeError",` |
|         - | 7853 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7854 | `				"array callback must have exactly two members"` |
|         - | 7855 | `				);` |
|         - | 7856 | `		}` |
|         6 | 7857 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7858 | `			"TypeError",` |
|         - | 7859 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7860 | `			"no array or string given"` |
|         - | 7861 | `			);` |
|         - | 7862 | `	}` |
|        21 | 7863 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7864 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7865 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7866 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7867 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7868 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7869 | `	/* Perform the desired operation */` |
|        21 | 7870 | `	pEntry = pMap->pFirst;` |
|        61 | 7871 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7872 | `		/* Extract the node value */` |
|        43 | 7873 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7874 | `		if( pValue ){` |
|         - | 7875 | `			sxi32 rcW;` |
|         - | 7876 | `			/* Extract the entry key */` |
|        43 | 7877 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7878 | `			/* Invoke the supplied callback */` |
|        43 | 7879 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7880 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7881 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7882 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7883 | `				return PH7_EXCEPTION;` |
|         - | 7884 | `			}` |
|        20 | 7885 | `		}` |
|         - | 7886 | `		/* Point to the next entry */` |
|        41 | 7887 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7888 | `	}` |
|         - | 7889 | `	/* All done, return TRUE */` |
|        19 | 7890 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7891 | `	return PH7_OK;` |
|        24 | 7892 | `}` |
|         - | 7893 | `/*` |
|         - | 7894 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7895 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7896 | ` */` |
|        22 | 7897 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7898 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7899 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7900 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7901 | `	int iNest             /* Nesting level */` |
|         - | 7902 | `	)` |
|         1 | 7903 | `{` |
|         - | 7904 | `	ph7_hashmap_node *pEntry;` |
|         - | 7905 | `	ph7_value *pValue,sKey;` |
|         - | 7906 | `	sxi32 rc;` |
|         - | 7907 | `	sxu32 n;` |
|         - | 7908 | `	/* Iterate through hashmap entries */` |
|        23 | 7909 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7910 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7911 | `	pEntry = pMap->pFirst;` |
|        59 | 7912 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7913 | `		/* Extract the node value */` |
|        37 | 7914 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7915 | `		if( pValue ){` |
|        37 | 7916 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7917 | `				if( iNest < 32 ){` |
|         - | 7918 | `					/* Recurse */` |
|        11 | 7919 | `					iNest++;` |
|        11 | 7920 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 7921 | `					iNest--;` |
|        11 | 7922 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7923 | `						return PH7_EXCEPTION;` |
|         - | 7924 | `					}` |
|         5 | 7925 | `				}` |
|         6 | 7926 | `			}else{` |
|         - | 7927 | `				/* Extract the node key */` |
|        27 | 7928 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7929 | `				/* Invoke the supplied callback */` |
|        27 | 7930 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 7931 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 7932 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 7933 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7934 | `					return PH7_EXCEPTION;` |
|         - | 7935 | `				}` |
|         - | 7936 | `			}` |
|        18 | 7937 | `		}` |
|         - | 7938 | `		/* Point to the next entry */` |
|        37 | 7939 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7940 | `	}` |
|        23 | 7941 | `	return PH7_OK;` |
|        12 | 7942 | `}` |
|         - | 7943 | `/*` |
|         - | 7944 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7945 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 7946 | ` * Parameters` |
|         - | 7947 | ` *  $array` |
|         - | 7948 | ` *   The input array.` |
|         - | 7949 | ` *  $funcname` |
|         - | 7950 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7951 | ` *   the first, and the key/index second.` |
|         - | 7952 | ` * Note:` |
|         - | 7953 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7954 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7955 | ` *  be made in the original array itself.` |
|         - | 7956 | ` *  $userdata` |
|         - | 7957 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7958 | ` *   to the callback funcname.` |
|         - | 7959 | ` * Return` |
|         - | 7960 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7961 | ` */` |
|        30 | 7962 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7963 | `{` |
|         - | 7964 | `	ph7_hashmap *pMap;` |
|        35 | 7965 | `	if( nArg < 2 ){` |
|         8 | 7966 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7967 | `			"ArgumentCountError",` |
|         - | 7968 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|         2 | 7969 | `			nArg` |
|         - | 7970 | `			);` |
|         - | 7971 | `	}` |
|        31 | 7972 | `	if( nArg > 3 ){` |
|         4 | 7973 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7974 | `			"ArgumentCountError",` |
|         - | 7975 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 7976 | `			nArg` |
|         - | 7977 | `			);` |
|         - | 7978 | `	}` |
|        29 | 7979 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7980 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7981 | `			"TypeError",` |
|         - | 7982 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7983 | `			ph7_type_name(apArg[0])` |
|         - | 7984 | `			);` |
|         - | 7985 | `	}` |
|        27 | 7986 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7987 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7988 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7989 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7990 | `				"TypeError",` |
|         - | 7991 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7992 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7993 | `				zFunc` |
|         - | 7994 | `				);` |
|         - | 7995 | `		}` |
|        12 | 7996 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7997 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7998 | `				"TypeError",` |
|         - | 7999 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8000 | `				"array callback must have exactly two members"` |
|         - | 8001 | `				);` |
|         - | 8002 | `		}` |
|         6 | 8003 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8004 | `			"TypeError",` |
|         - | 8005 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8006 | `			"no array or string given"` |
|         - | 8007 | `			);` |
|         - | 8008 | `	}` |
|         - | 8009 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8010 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8011 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8012 | `	/* Perform the desired operation */` |
|        13 | 8013 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8014 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8015 | `		return PH7_EXCEPTION;` |
|         - | 8016 | `	}` |
|         - | 8017 | `	/* All done, return TRUE */` |
|        13 | 8018 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8019 | `	return PH7_OK;` |
|        20 | 8020 | `}` |
|         - | 8021 | `/*` |
|         - | 8022 | ` * bool array_is_list(array $array)` |
|         - | 8023 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8024 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8025 | ` * Return` |
|         - | 8026 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8027 | ` */` |
|         - | 8028 | `/*` |
|         - | 8029 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8030 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8031 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8032 | ` */` |
|       244 | 8033 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8034 | `{` |
|       245 | 8035 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       245 | 8036 | `	sxi64 iExpect = 0;` |
|         - | 8037 | `	sxu32 n;` |
|       547 | 8038 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       403 | 8039 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8040 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       101 | 8041 | `			return 0;` |
|         - | 8042 | `		}` |
|       303 | 8043 | `		++iExpect;` |
|       303 | 8044 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       152 | 8045 | `	}` |
|       145 | 8046 | `	return 1;` |
|       123 | 8047 | `}` |
|        12 | 8048 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8049 | `{` |
|        13 | 8050 | `	if( nArg < 1 ){` |
|       ! 0 | 8051 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8052 | `			"ArgumentCountError",` |
|         - | 8053 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8054 | `			);` |
|         - | 8055 | `	}` |
|        13 | 8056 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8057 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8058 | `			"TypeError",` |
|         - | 8059 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8060 | `			ph7_type_name(apArg[0])` |
|         - | 8061 | `			);` |
|         - | 8062 | `	}` |
|        13 | 8063 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8064 | `	return PH7_OK;` |
|         7 | 8065 | `}` |
|         - | 8066 | `/*` |
|         - | 8067 | ` * mixed array_first(array $array)` |
|         - | 8068 | ` * mixed array_last(array $array)` |
|         - | 8069 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8070 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8071 | ` *  untouched (unlike reset()/end()).` |
|         - | 8072 | ` */` |
|        20 | 8073 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8074 | `{` |
|         - | 8075 | `	ph7_hashmap *pMap;` |
|         - | 8076 | `	ph7_hashmap_node *pNode;` |
|         - | 8077 | `	ph7_value *pVal;` |
|        21 | 8078 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        21 | 8079 | `	if( nArg < 1 ){` |
|         4 | 8080 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8081 | `			"ArgumentCountError",` |
|         - | 8082 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8083 | `			zName` |
|         - | 8084 | `			);` |
|         - | 8085 | `	}` |
|        19 | 8086 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8087 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8088 | `			"TypeError",` |
|         - | 8089 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8090 | `			zName,` |
|         1 | 8091 | `			ph7_type_name(apArg[0])` |
|         - | 8092 | `			);` |
|         - | 8093 | `	}` |
|        17 | 8094 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8095 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8096 | `	if( pNode == 0 ){` |
|         - | 8097 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8098 | `		ph7_result_null(pCtx);` |
|         5 | 8099 | `		return PH7_OK;` |
|         - | 8100 | `	}` |
|        13 | 8101 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8102 | `	if( pVal ){` |
|        13 | 8103 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8104 | `	}else{` |
|       ! 0 | 8105 | `		ph7_result_null(pCtx);` |
|         - | 8106 | `	}` |
|        13 | 8107 | `	return PH7_OK;` |
|        11 | 8108 | `}` |
|        10 | 8109 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8110 | `{` |
|        11 | 8111 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8112 | `}` |
|        10 | 8113 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8114 | `{` |
|        11 | 8115 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8116 | `}` |
|         - | 8117 | `/*` |
|         - | 8118 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8119 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8120 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8121 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8122 | ` *  untouched.` |
|         - | 8123 | ` */` |
|        24 | 8124 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8125 | `{` |
|         - | 8126 | `	ph7_hashmap *pMap;` |
|         - | 8127 | `	ph7_hashmap_node *pNode;` |
|        25 | 8128 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        25 | 8129 | `	if( nArg < 1 ){` |
|         4 | 8130 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8131 | `			"ArgumentCountError",` |
|         - | 8132 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8133 | `			zName` |
|         - | 8134 | `			);` |
|         - | 8135 | `	}` |
|        23 | 8136 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8137 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8138 | `			"TypeError",` |
|         - | 8139 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8140 | `			zName,` |
|         1 | 8141 | `			ph7_type_name(apArg[0])` |
|         - | 8142 | `			);` |
|         - | 8143 | `	}` |
|        21 | 8144 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8145 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8146 | `	if( pNode == 0 ){` |
|         - | 8147 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8148 | `		ph7_result_null(pCtx);` |
|         5 | 8149 | `		return PH7_OK;` |
|         - | 8150 | `	}` |
|        17 | 8151 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8152 | `	return PH7_OK;` |
|        13 | 8153 | `}` |
|        12 | 8154 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8155 | `{` |
|        13 | 8156 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8157 | `}` |
|        12 | 8158 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8159 | `{` |
|        13 | 8160 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8161 | `}` |
|         - | 8162 | `/*` |
|         - | 8163 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8164 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8165 | ` * array_column() for both the column value and the index key.` |
|         - | 8166 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8167 | ` * container or the key is absent.` |
|         - | 8168 | ` */` |
|        32 | 8169 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8170 | `{` |
|        33 | 8171 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8172 | `		ph7_hashmap_node *pNode;` |
|        25 | 8173 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8174 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8175 | `		}` |
|        11 | 8176 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8177 | `		ph7_value sName;` |
|         - | 8178 | `		const char *zName;` |
|         - | 8179 | `		ph7_value *pAttr;` |
|         - | 8180 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8181 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8182 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8183 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8184 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8185 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8186 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8187 | `		return pAttr;` |
|         - | 8188 | `	}` |
|         5 | 8189 | `	return 0;` |
|        17 | 8190 | `}` |
|         - | 8191 | `/*` |
|         - | 8192 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8193 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8194 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8195 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8196 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8197 | ` *  Each row may be an array or an object.` |
|         - | 8198 | ` */` |
|        12 | 8199 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8200 | `{` |
|         - | 8201 | `	ph7_hashmap_node *pNode;` |
|         - | 8202 | `	ph7_hashmap *pMap;` |
|         - | 8203 | `	ph7_value *pArray;` |
|         - | 8204 | `	ph7_value *pRow;` |
|         - | 8205 | `	ph7_value *pCol;` |
|         - | 8206 | `	ph7_value *pIdx;` |
|         - | 8207 | `	int bWantCol;` |
|         - | 8208 | `	int bWantIdx;` |
|         - | 8209 | `	sxu32 n;` |
|        13 | 8210 | `	if( nArg < 2 ){` |
|       ! 0 | 8211 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8212 | `			"ArgumentCountError",` |
|         - | 8213 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8214 | `			nArg` |
|         - | 8215 | `			);` |
|         - | 8216 | `	}` |
|        13 | 8217 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8218 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8219 | `			"TypeError",` |
|         - | 8220 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8221 | `			ph7_type_name(apArg[0])` |
|         - | 8222 | `			);` |
|         - | 8223 | `	}` |
|        13 | 8224 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8225 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8226 | `	if( pArray == 0 ){` |
|       ! 0 | 8227 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8228 | `		return PH7_OK;` |
|         - | 8229 | `	}` |
|         - | 8230 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8231 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8232 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8233 | `	pNode = pMap->pFirst;` |
|        33 | 8234 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8235 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8236 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8237 | `		if( pRow == 0 ){` |
|       ! 0 | 8238 | `			continue;` |
|         - | 8239 | `		}` |
|        21 | 8240 | `		if( bWantCol ){` |
|        19 | 8241 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8242 | `			if( pCol == 0 ){` |
|         - | 8243 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8244 | `				continue;` |
|         - | 8245 | `			}` |
|         9 | 8246 | `		}else{` |
|         3 | 8247 | `			pCol = pRow;` |
|         - | 8248 | `		}` |
|        19 | 8249 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8250 | `		if( pIdx ){` |
|        13 | 8251 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8252 | `		}else{` |
|         7 | 8253 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8254 | `		}` |
|        10 | 8255 | `	}` |
|        13 | 8256 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8257 | `	return PH7_OK;` |
|         7 | 8258 | `}` |
|         - | 8259 | `/*` |
|         - | 8260 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8261 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8262 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8263 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8264 | ` */` |
|        28 | 8265 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8266 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8267 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8268 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8269 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8270 | `	)` |
|         1 | 8271 | `{` |
|         - | 8272 | `	ph7_hashmap_node *pEntry;` |
|         - | 8273 | `	ph7_hashmap *pMap;` |
|         - | 8274 | `	ph7_value *pValue;` |
|         - | 8275 | `	ph7_value *apCbArg[2];` |
|         - | 8276 | `	ph7_value sKey;` |
|         - | 8277 | `	ph7_value sResult;` |
|         - | 8278 | `	sxi32 rc;` |
|         - | 8279 | `	sxu32 n;` |
|        29 | 8280 | `	*ppMatch = 0;` |
|        29 | 8281 | `	if( nArg < 2 ){` |
|       ! 0 | 8282 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8283 | `			"ArgumentCountError",` |
|         - | 8284 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8285 | `			zName,nArg` |
|         - | 8286 | `			);` |
|         - | 8287 | `	}` |
|        29 | 8288 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8289 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8290 | `			"TypeError",` |
|         - | 8291 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8292 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8293 | `			);` |
|         - | 8294 | `	}` |
|        29 | 8295 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8296 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8297 | `			"TypeError",` |
|         - | 8298 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8299 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8300 | `			);` |
|         - | 8301 | `	}` |
|        29 | 8302 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8303 | `	pEntry = pMap->pFirst;` |
|        29 | 8304 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8305 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8306 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8307 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8308 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8309 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8310 | `		if( pValue ){` |
|         - | 8311 | `			/* The callback receives ($value, $key). */` |
|        59 | 8312 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8313 | `			apCbArg[0] = pValue;` |
|        59 | 8314 | `			apCbArg[1] = &sKey;` |
|        59 | 8315 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8316 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8317 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8318 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8319 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8320 | `				return PH7_EXCEPTION;` |
|         - | 8321 | `			}` |
|        59 | 8322 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8323 | `				*ppMatch = pEntry;` |
|        15 | 8324 | `				break;` |
|         - | 8325 | `			}` |
|        22 | 8326 | `		}` |
|        45 | 8327 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8328 | `	}` |
|        29 | 8329 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8330 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8331 | `	return PH7_OK;` |
|        15 | 8332 | `}` |
|         - | 8333 | `/*` |
|         - | 8334 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8335 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8336 | ` *  is truthy, or NULL if none match.` |
|         - | 8337 | ` */` |
|         6 | 8338 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8339 | `{` |
|         - | 8340 | `	ph7_hashmap_node *pMatch;` |
|         - | 8341 | `	ph7_value *pVal;` |
|         - | 8342 | `	sxi32 rc;` |
|         7 | 8343 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8344 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8345 | `		return rc;` |
|         - | 8346 | `	}` |
|         7 | 8347 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8348 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8349 | `	}else{` |
|         3 | 8350 | `		ph7_result_null(pCtx);` |
|         - | 8351 | `	}` |
|         7 | 8352 | `	return PH7_OK;` |
|         4 | 8353 | `}` |
|         - | 8354 | `/*` |
|         - | 8355 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8356 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8357 | ` *  is truthy, or NULL if none match.` |
|         - | 8358 | ` */` |
|         6 | 8359 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8360 | `{` |
|         - | 8361 | `	ph7_hashmap_node *pMatch;` |
|         - | 8362 | `	sxi32 rc;` |
|         7 | 8363 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8364 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8365 | `		return rc;` |
|         - | 8366 | `	}` |
|         7 | 8367 | `	if( pMatch == 0 ){` |
|         3 | 8368 | `		ph7_result_null(pCtx);` |
|         6 | 8369 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8370 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8371 | `	}else{` |
|         4 | 8372 | `		ph7_result_string(pCtx,` |
|         2 | 8373 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8374 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8375 | `	}` |
|         7 | 8376 | `	return PH7_OK;` |
|         4 | 8377 | `}` |
|         - | 8378 | `/*` |
|         - | 8379 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8380 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8381 | ` *  FALSE for an empty array.` |
|         - | 8382 | ` */` |
|         8 | 8383 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8384 | `{` |
|         - | 8385 | `	ph7_hashmap_node *pMatch;` |
|         - | 8386 | `	sxi32 rc;` |
|         9 | 8387 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8388 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8389 | `		return rc;` |
|         - | 8390 | `	}` |
|         9 | 8391 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8392 | `	return PH7_OK;` |
|         5 | 8393 | `}` |
|         - | 8394 | `/*` |
|         - | 8395 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8396 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8397 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8398 | ` */` |
|         8 | 8399 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8400 | `{` |
|         - | 8401 | `	ph7_hashmap_node *pMatch;` |
|         - | 8402 | `	sxi32 rc;` |
|         9 | 8403 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8404 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8405 | `		return rc;` |
|         - | 8406 | `	}` |
|         9 | 8407 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8408 | `	return PH7_OK;` |
|         5 | 8409 | `}` |
|         - | 8410 | `/*` |
|         - | 8411 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8412 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8413 | ` */` |
|         - | 8414 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8415 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        70 | 8416 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8417 | `{` |
|        74 | 8418 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        35 | 8419 | `	(void)pVm;` |
|        74 | 8420 | `	p->nCount++;` |
|        74 | 8421 | `	if( p->pArray ){` |
|         - | 8422 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8423 | `		 * otherwise append with an auto-assigned int index. */` |
|        66 | 8424 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        31 | 8425 | `	}` |
|        74 | 8426 | `	return SXRET_OK;` |
|         4 | 8427 | `}` |
|         - | 8428 | `/*` |
|         - | 8429 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8430 | ` */` |
|        26 | 8431 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8432 | `{` |
|         - | 8433 | `	struct IterCollect sCol;` |
|         - | 8434 | `	ph7_value *pArray;` |
|         - | 8435 | `	sxi32 rc;` |
|        30 | 8436 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8437 | `	pArray = ph7_context_new_array(pCtx);` |
|        30 | 8438 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8439 | `	sCol.pArray = pArray;` |
|        30 | 8440 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        30 | 8441 | `	sCol.nCount = 0;` |
|        30 | 8442 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8443 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8444 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8445 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8446 | `		sxu32 n;` |
|         9 | 8447 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8448 | `			ph7_value sKey, *pVal;` |
|         7 | 8449 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8450 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8451 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8452 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8453 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8454 | `			pEntry = pEntry->pPrev;` |
|         4 | 8455 | `		}` |
|         3 | 8456 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8457 | `		return PH7_OK;` |
|         - | 8458 | `	}` |
|        28 | 8459 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        28 | 8460 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        26 | 8461 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8462 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8463 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8464 | `			ph7_type_name(apArg[0]));` |
|         - | 8465 | `	}` |
|        26 | 8466 | `	ph7_result_value(pCtx,pArray);` |
|        26 | 8467 | `	return PH7_OK;` |
|        17 | 8468 | `}` |
|         - | 8469 | `/*` |
|         - | 8470 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8471 | ` */` |
|         6 | 8472 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8473 | `{` |
|         - | 8474 | `	struct IterCollect sCol;` |
|         - | 8475 | `	sxi32 rc;` |
|         7 | 8476 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         7 | 8477 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8478 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8479 | `		return PH7_OK;` |
|         - | 8480 | `	}` |
|         5 | 8481 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         5 | 8482 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         5 | 8483 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         5 | 8484 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8485 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8486 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8487 | `			ph7_type_name(apArg[0]));` |
|         - | 8488 | `	}` |
|         5 | 8489 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         5 | 8490 | `	return PH7_OK;` |
|         4 | 8491 | `}` |
|         - | 8492 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8493 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8494 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8495 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        24 | 8496 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8497 | `{` |
|        25 | 8498 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8499 | `	ph7_value sResult;` |
|         - | 8500 | `	SySet aArg;` |
|         - | 8501 | `	sxi32 rc;` |
|         - | 8502 | `	int bContinue;` |
|        12 | 8503 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        25 | 8504 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        25 | 8505 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8506 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8507 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8508 | `		sxu32 n;` |
|        17 | 8509 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8510 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8511 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8512 | `			pEntry = pEntry->pPrev;` |
|         5 | 8513 | `		}` |
|         4 | 8514 | `	}` |
|        25 | 8515 | `	PH7_MemObjInit(pVm,&sResult);` |
|        37 | 8516 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        24 | 8517 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        25 | 8518 | `	SySetRelease(&aArg);` |
|        25 | 8519 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        23 | 8520 | `	p->nCount++;` |
|        23 | 8521 | `	PH7_MemObjToBool(&sResult);` |
|        23 | 8522 | `	bContinue = (sResult.x.iVal != 0);` |
|        23 | 8523 | `	PH7_MemObjRelease(&sResult);` |
|        23 | 8524 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        13 | 8525 | `}` |
|         - | 8526 | `/*` |
|         - | 8527 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8528 | ` */` |
|         8 | 8529 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8530 | `{` |
|         - | 8531 | `	struct IterApply sApp;` |
|         - | 8532 | `	sxi32 rc;` |
|         9 | 8533 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8534 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8535 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8536 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8537 | `	}` |
|         9 | 8538 | `	sApp.pCallback = apArg[1];` |
|         9 | 8539 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|         9 | 8540 | `	sApp.nCount = 0;` |
|         9 | 8541 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|         9 | 8542 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8543 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8544 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8545 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8546 | `			ph7_type_name(apArg[0]));` |
|         - | 8547 | `	}` |
|         7 | 8548 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|         7 | 8549 | `	return PH7_OK;` |
|         5 | 8550 | `}` |
|         - | 8551 | `/*` |
|         - | 8552 | ` * Table of hashmap functions.` |
|         - | 8553 | ` */` |
|         - | 8554 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8555 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8556 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8557 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8558 | `	{"count",             ph7_hashmap_count },` |
|         - | 8559 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8560 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8561 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8562 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8563 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8564 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8565 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8566 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8567 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8568 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8569 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8570 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8571 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8572 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8573 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8574 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8575 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8576 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8577 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8578 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8579 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8580 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8581 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8582 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8583 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8584 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8585 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8586 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8587 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8588 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8589 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8590 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8591 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8592 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8593 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8594 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8595 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8596 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8597 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8598 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8599 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8600 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8601 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8602 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8603 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8604 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8605 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8606 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8607 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8608 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8609 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8610 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8611 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8612 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8613 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8614 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8615 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8616 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8617 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8618 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8619 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8620 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8621 | `	{"current",           ph7_hashmap_current },` |
|         - | 8622 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8623 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8624 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8625 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8626 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8627 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8628 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8629 | `};` |
|         - | 8630 | `/*` |
|         - | 8631 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8632 | ` */` |
|      3488 | 8633 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8634 | `{` |
|         - | 8635 | `	sxu32 n;` |
|    261605 | 8636 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    258117 | 8637 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    129061 | 8638 | `	}` |
|      3493 | 8639 | `}` |
|         - | 8640 | `/*` |
|         - | 8641 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8642 | ` * the BLOB given as the first argument.` |
|         - | 8643 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8644 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8645 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8646 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8647 | ` */` |
|         - | 8648 | `/*` |
|         - | 8649 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8650 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8651 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8652 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8653 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8654 | ` */` |
|        84 | 8655 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         3 | 8656 | `{` |
|        87 | 8657 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8658 | `	ph7_value *pObj;` |
|        87 | 8659 | `	sxu32 n = 0;` |
|         - | 8660 | `	int isRef;` |
|        87 | 8661 | `	sxi32 rc = SXRET_OK;` |
|         - | 8662 | `	int i;` |
|       134 | 8663 | `	for(;;){` |
|       271 | 8664 | `		if( n >= pMap->nEntry ){` |
|        87 | 8665 | `			break;` |
|         - | 8666 | `		}` |
|       187 | 8667 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       187 | 8668 | `		isRef = (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0;` |
|       187 | 8669 | `		if( ShowType ){` |
|         - | 8670 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8671 | `			 * on the next line at the same indent (php). */` |
|       105 | 8672 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        71 | 8673 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        37 | 8674 | `			}` |
|        37 | 8675 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        23 | 8676 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        12 | 8677 | `			}else{` |
|        21 | 8678 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8679 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8680 | `			}` |
|        37 | 8681 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        37 | 8682 | `			if( pObj ){` |
|        37 | 8683 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        37 | 8684 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8685 | `					break;` |
|         - | 8686 | `				}` |
|        17 | 8687 | `			}` |
|        20 | 8688 | `		}else{` |
|         - | 8689 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8690 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8691 | `			 * php's extra blank line. References carry no marker. */` |
|       864 | 8692 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|       714 | 8693 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       358 | 8694 | `			}` |
|       152 | 8695 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        51 | 8696 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        26 | 8697 | `			}else{` |
|       152 | 8698 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        50 | 8699 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8700 | `			}` |
|       150 | 8701 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|        89 | 8702 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8703 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8704 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8705 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8706 | `					break;` |
|         - | 8707 | `				}` |
|        13 | 8708 | `			}else{` |
|       128 | 8709 | `				if( pObj ){` |
|       128 | 8710 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|        63 | 8711 | `				}` |
|       128 | 8712 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8713 | `			}` |
|         - | 8714 | `		}` |
|         - | 8715 | `		/* Point to the next entry */` |
|       187 | 8716 | `		n++;` |
|       187 | 8717 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 8718 | `	}` |
|        87 | 8719 | `	return rc;` |
|         3 | 8720 | `}` |
|        80 | 8721 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8722 | `{` |
|         - | 8723 | `	sxi32 rc;` |
|         - | 8724 | `	int i;` |
|        82 | 8725 | `	if( nDepth > 31 ){` |
|         - | 8726 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8727 | `		/* Nesting limit reached */` |
|       ! 0 | 8728 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8729 | `		return SXERR_LIMIT;` |
|         - | 8730 | `	}` |
|        82 | 8731 | `	if( ShowType ){` |
|         - | 8732 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8733 | `		 * newline (a nested array is itself an entry value line). */` |
|        14 | 8734 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        14 | 8735 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        14 | 8736 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        14 | 8737 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8738 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8739 | `		}` |
|        14 | 8740 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        14 | 8741 | `		return rc;` |
|         - | 8742 | `	}` |
|         - | 8743 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|        69 | 8744 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       261 | 8745 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8746 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8747 | `	}` |
|        69 | 8748 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        69 | 8749 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       261 | 8750 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8751 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8752 | `	}` |
|        69 | 8753 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        69 | 8754 | `	return rc;` |
|        42 | 8755 | `}` |
|         - | 8756 | `/*` |
|         - | 8757 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8758 | ` * retrieved entry.` |
|         - | 8759 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8760 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8761 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8762 | ` * a value different from PH7_OK.` |
|         - | 8763 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8764 | ` */` |
|     34042 | 8765 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8766 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8767 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8768 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8769 | `	)` |
|         5 | 8770 | `{` |
|         - | 8771 | `	ph7_hashmap_node *pEntry;` |
|         - | 8772 | `	ph7_value sKey,sValue;` |
|         - | 8773 | `	sxi32 rc;` |
|         - | 8774 | `	sxu32 n;` |
|         - | 8775 | `	/* Initialize walker parameter */` |
|     34047 | 8776 | `	rc = SXRET_OK;` |
|     34047 | 8777 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     34047 | 8778 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     34047 | 8779 | `	n = pMap->nEntry;` |
|     34047 | 8780 | `	pEntry = pMap->pFirst;` |
|         - | 8781 | `	/* Start the iteration process */` |
|     91306 | 8782 | `	for(;;){` |
|    182617 | 8783 | `		if( n < 1 ){` |
|     34047 | 8784 | `			break;` |
|         - | 8785 | `		}` |
|         - | 8786 | `		/* Extract a copy of the key and a copy the current value */` |
|    148575 | 8787 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    148575 | 8788 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8789 | `		/* Invoke the user callback */` |
|    148575 | 8790 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8791 | `		/* Release the copy of the key and the value */` |
|    148575 | 8792 | `		PH7_MemObjRelease(&sKey);` |
|    148575 | 8793 | `		PH7_MemObjRelease(&sValue);` |
|    148575 | 8794 | `		if( rc != PH7_OK ){` |
|         - | 8795 | `			/* Callback request an operation abort */` |
|       ! 0 | 8796 | `			return SXERR_ABORT;` |
|         - | 8797 | `		}` |
|         - | 8798 | `		/* Point to the next entry */` |
|    148575 | 8799 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    148575 | 8800 | `		n--;` |
|         5 | 8801 | `	}` |
|         - | 8802 | `	/* All done */` |
|     34047 | 8803 | `	return SXRET_OK;` |
|     17026 | 8804 | `}` |
|         - | 8805 |  |
