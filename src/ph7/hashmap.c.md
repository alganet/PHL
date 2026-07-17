# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3884/4332 lines (89.66%)

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
|   7418380 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7418385 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7418385 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    577482 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    577487 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    577487 |   35 | `	sxu32 nH = 5381;` |
|    577487 |   36 | `	zEnd = &zIn[nLen];` |
|    658817 |   37 | `	for(;;){` |
|   1317639 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1118805 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1001351 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    864373 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    577487 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      1278 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      1283 |   54 | `	sxi64 iCount = 0;` |
|      1283 |   55 | `	if( !bRecursive ){` |
|      1109 |   56 | `		iCount = pMap->nEntry;` |
|       557 |   57 | `	}else{` |
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
|      1283 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3121070 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3121075 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3121075 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3121075 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3121075 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3121075 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3121075 |  112 | `	pNode->nHash = nHash;` |
|   3121075 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3121075 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3121075 |  115 | `	return pNode;` |
|   1560540 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    239254 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    239259 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    239259 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    239259 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    239259 |  133 | `	pNode->pMap  = &(*pMap);` |
|    239259 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    239259 |  135 | `	pNode->nHash = nHash;` |
|    239259 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    239259 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    239259 |  138 | `	pNode->nValIdx = nValIdx;` |
|    239259 |  139 | `	return pNode;` |
|    119632 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3360324 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3360329 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2910027 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2910027 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1455011 |  150 | `	}` |
|   3360329 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3360329 |  153 | `	if( pMap->pFirst == 0 ){` |
|     86703 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     86703 |  156 | `		pMap->pCur = pNode;` |
|     43354 |  157 | `	}else{` |
|   3273631 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3360329 |  160 | `	++pMap->nEntry;` |
|   3360329 |  161 | `}` |
|         - |  162 | `/*` |
|         - |  163 | ` * Unlink a node from the hashmap.` |
|         - |  164 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  165 | ` */` |
|      7424 |  166 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  167 | `{` |
|      7429 |  168 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7429 |  169 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  170 | `	/* Unlink from the corresponding bucket */` |
|      7429 |  171 | `	if( pNode->pPrevCollide == 0 ){` |
|      6957 |  172 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3481 |  173 | `	}else{` |
|       474 |  174 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  175 | `	}` |
|      7429 |  176 | `	if( pNode->pNextCollide ){` |
|      4517 |  177 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2257 |  178 | `	}` |
|      7429 |  179 | `	if( pMap->pFirst == pNode ){` |
|       131 |  180 | `		pMap->pFirst = pNode->pPrev;` |
|        63 |  181 | `	}` |
|      7429 |  182 | `	if( pMap->pCur == pNode ){` |
|         - |  183 | `		/* Advance the node cursor */` |
|       133 |  184 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|        64 |  185 | `	}` |
|         - |  186 | `	/* Unlink from the map list */` |
|      7429 |  187 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7429 |  188 | `	if( bRestore ){` |
|         - |  189 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       107 |  190 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  191 | `		/* Restore to the freelist */` |
|       107 |  192 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       107 |  193 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|        51 |  194 | `		}` |
|        51 |  195 | `	}` |
|      7429 |  196 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7292 |  197 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3644 |  198 | `	}` |
|      7429 |  199 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7429 |  200 | `	pMap->nEntry--;` |
|      7429 |  201 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  202 | `		/* Free the hash-bucket */` |
|        75 |  203 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|        75 |  204 | `		pMap->apBucket = 0;` |
|        75 |  205 | `		pMap->nSize = 0;` |
|        75 |  206 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        35 |  207 | `	}` |
|      7429 |  208 | `}` |
|         - |  209 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  210 | `/*` |
|         - |  211 | ` * Grow the hash-table and rehash all entries.` |
|         - |  212 | ` */` |
|   3360324 |  213 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  214 | `{` |
|   3360329 |  215 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     91569 |  216 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  217 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     91569 |  218 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  219 | `		sxu32 nBucket;` |
|         - |  220 | `		sxu32 n;` |
|     91569 |  221 | `		if( nNew < 1 ){` |
|     86703 |  222 | `			nNew = 16;` |
|     43349 |  223 | `		}` |
|         - |  224 | `		/* Allocate a new bucket */` |
|     91569 |  225 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     91569 |  226 | `		if( apNew == 0 ){` |
|       ! 0 |  227 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  228 | `				return SXERR_MEM; /* Fatal */` |
|         - |  229 | `			}` |
|         - |  230 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  231 | `			return SXRET_OK;` |
|         - |  232 | `		}` |
|         - |  233 | `		/* Zero the table */` |
|     91569 |  234 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  235 | `		/* Reflect the change */` |
|     91569 |  236 | `		pMap->apBucket = apNew;` |
|     91569 |  237 | `		pMap->nSize = nNew;` |
|     91569 |  238 | `		if( apOld == 0 ){` |
|         - |  239 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     86703 |  240 | `			return SXRET_OK;` |
|         - |  241 | `		}` |
|         - |  242 | `		/* Rehash old entries */` |
|      4871 |  243 | `		pEntry = pMap->pFirst;` |
|      4871 |  244 | `		n = 0;` |
|   2097585 |  245 | `		for( ;; ){` |
|   4195175 |  246 | `			if( n >= pMap->nEntry ){` |
|      4871 |  247 | `				break;` |
|         - |  248 | `			}` |
|         - |  249 | `			/* Clear the old collision link */` |
|   4190309 |  250 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  251 | `			/* Link to the new bucket */` |
|   4190309 |  252 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4190309 |  253 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3584713 |  254 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3584713 |  255 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1792354 |  256 | `			}` |
|   4190309 |  257 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  258 | `			/* Point to the next entry */` |
|   4190309 |  259 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4190309 |  260 | `			n++;` |
|         5 |  261 | `		}` |
|         - |  262 | `		/* Free the old table */` |
|      4871 |  263 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2433 |  264 | `	}` |
|   3273631 |  265 | `	return SXRET_OK;` |
|   1680167 |  266 | `}` |
|         - |  267 | `/*` |
|         - |  268 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  269 | ` * hashmap.` |
|         - |  270 | ` */` |
|   3121070 |  271 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  272 | `{` |
|         - |  273 | `	ph7_hashmap_node *pNode;` |
|         - |  274 | `	sxu32 nIdx;` |
|         - |  275 | `	sxu32 nHash;` |
|         - |  276 | `	sxi32 rc;` |
|   3121075 |  277 | `	if( !isForeign ){` |
|         - |  278 | `		ph7_value *pObj;` |
|         - |  279 | `		ph7_value sSafeVal;` |
|         - |  280 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  281 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  282 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  283 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  284 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  285 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3121037 |  286 | `		if( pValue ){` |
|   3121035 |  287 | `			sSafeVal = *pValue;` |
|   3121035 |  288 | `			pValue = &sSafeVal;` |
|   1560515 |  289 | `		}` |
|         - |  290 | `		/* Reserve a ph7_value for the value */` |
|   3121037 |  291 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3121037 |  292 | `		if( pObj == 0 ){` |
|       ! 0 |  293 | `			return SXERR_MEM;` |
|         - |  294 | `		}` |
|   3121037 |  295 | `		if( pValue ){` |
|         - |  296 | `			/* Duplicate the value */` |
|   3121035 |  297 | `			PH7_MemObjStore(pValue,pObj);` |
|   1560515 |  298 | `		}` |
|   3121037 |  299 | `		nIdx = pObj->nIdx;` |
|   1560521 |  300 | `	}else{` |
|        39 |  301 | `		nIdx = nRefIdx;` |
|         - |  302 | `	}` |
|         - |  303 | `	/* Hash the key */` |
|   3121075 |  304 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  305 | `	/* Allocate a new int node */` |
|   3121075 |  306 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3121075 |  307 | `	if( pNode == 0 ){` |
|       ! 0 |  308 | `		return SXERR_MEM;` |
|         - |  309 | `	}` |
|   3121075 |  310 | `	if( isForeign ){` |
|         - |  311 | `		/* Mark as a foregin entry */` |
|        39 |  312 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        19 |  313 | `	}` |
|         - |  314 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3121075 |  315 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3121075 |  316 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  317 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  318 | `		return rc;` |
|         - |  319 | `	}` |
|         - |  320 | `	/* Perform the insertion */` |
|   3121075 |  321 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  322 | `	/* Install in the reference table */` |
|   3121075 |  323 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  324 | `	/* All done */` |
|   3121075 |  325 | `	return SXRET_OK;` |
|   1560540 |  326 | `}` |
|         - |  327 | `/*` |
|         - |  328 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  329 | ` * hashmap.` |
|         - |  330 | ` */` |
|    239254 |  331 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  332 | `{` |
|         - |  333 | `	ph7_hashmap_node *pNode;` |
|         - |  334 | `	sxu32 nHash;` |
|         - |  335 | `	sxu32 nIdx;` |
|         - |  336 | `	sxi32 rc;` |
|    239259 |  337 | `	if( !isForeign ){` |
|         - |  338 | `		ph7_value *pObj;` |
|         - |  339 | `		ph7_value sSafeVal;` |
|         - |  340 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  341 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  342 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  343 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  344 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  345 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    192799 |  346 | `		if( pValue ){` |
|    192509 |  347 | `			sSafeVal = *pValue;` |
|    192509 |  348 | `			pValue = &sSafeVal;` |
|     96252 |  349 | `		}` |
|         - |  350 | `		/* Reserve a ph7_value for the value */` |
|    192799 |  351 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    192799 |  352 | `		if( pObj == 0 ){` |
|       ! 0 |  353 | `			return SXERR_MEM;` |
|         - |  354 | `		}` |
|    192799 |  355 | `		if( pValue ){` |
|         - |  356 | `			/* Duplicate the value */` |
|    192509 |  357 | `			PH7_MemObjStore(pValue,pObj);` |
|     96252 |  358 | `		}` |
|    192799 |  359 | `		nIdx = pObj->nIdx;` |
|     96402 |  360 | `	}else{` |
|     46465 |  361 | `		nIdx = nRefIdx;` |
|         - |  362 | `	}` |
|         - |  363 | `	/* Hash the key */` |
|    239259 |  364 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  365 | `	/* Allocate a new blob node */` |
|    239259 |  366 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    239259 |  367 | `	if( pNode == 0 ){` |
|       ! 0 |  368 | `		return SXERR_MEM;` |
|         - |  369 | `	}` |
|    239259 |  370 | `	if( isForeign ){` |
|         - |  371 | `		/* Mark as a foregin entry */` |
|     46465 |  372 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     23230 |  373 | `	}` |
|         - |  374 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    239259 |  375 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    239259 |  376 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  377 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  378 | `		return rc;` |
|         - |  379 | `	}` |
|         - |  380 | `	/* Perform the insertion */` |
|    239259 |  381 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  382 | `	/* Install in the reference table */` |
|    239259 |  383 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  384 | `	/* All done */` |
|    239259 |  385 | `	return SXRET_OK;` |
|    119632 |  386 | `}` |
|         - |  387 | `/*` |
|         - |  388 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  389 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  390 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  391 | ` */` |
|   4284338 |  392 | `static sxi32 HashmapLookupIntKey(` |
|         - |  393 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  394 | `	sxi64 iKey,                /* lookup key */` |
|         - |  395 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  396 | `	)` |
|         5 |  397 | `{` |
|         - |  398 | `	ph7_hashmap_node *pNode;` |
|         - |  399 | `	sxu32 nHash;` |
|   4284343 |  400 | `	if( pMap->nEntry < 1 ){` |
|         - |  401 | `		/* Don't bother hashing,there is no entry anyway */` |
|       587 |  402 | `		return SXERR_NOTFOUND;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Hash the key first */` |
|   4283761 |  405 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  406 | `	/* Point to the appropriate bucket */` |
|   4283761 |  407 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  408 | `	/* Perform the lookup */` |
| 110562409 |  409 | `	for(;;){` |
| 221124823 |  410 | `		if( pNode == 0 ){` |
|   4281163 |  411 | `			break;` |
|         - |  412 | `		}` |
| 216843660 |  413 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216840650 |  414 | `			&& pNode->nHash == nHash` |
| 108420124 |  415 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  416 | `				/* Node found */` |
|      2603 |  417 | `				if( ppNode ){` |
|      2585 |  418 | `					*ppNode = pNode;` |
|      1290 |  419 | `				}` |
|      2603 |  420 | `				return SXRET_OK;` |
|         - |  421 | `		}` |
|         - |  422 | `		/* Follow the collision link */` |
| 216841063 |  423 | `		pNode = pNode->pNextCollide;` |
|         1 |  424 | `	}` |
|         - |  425 | `	/* No such entry */` |
|   4281163 |  426 | `	return SXERR_NOTFOUND;` |
|   2142174 |  427 | `}` |
|         - |  428 | `/*` |
|         - |  429 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  430 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  431 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  432 | ` */` |
|    371976 |  433 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  434 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  435 | `	const void *pKey,           /* Lookup key */` |
|         - |  436 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  437 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  438 | `	)` |
|         5 |  439 | `{` |
|         - |  440 | `	ph7_hashmap_node *pNode;` |
|         - |  441 | `	sxu32 nHash;` |
|    371981 |  442 | `	if( pMap->nEntry < 1 ){` |
|         - |  443 | `		/* Don't bother hashing,there is no entry anyway */` |
|     33753 |  444 | `		return SXERR_NOTFOUND;` |
|         - |  445 | `	}` |
|         - |  446 | `	/* Hash the key first */` |
|    338233 |  447 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  448 | `	/* Point to the appropriate bucket */` |
|    338233 |  449 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  450 | `	/* Perform the lookup */` |
|    277788 |  451 | `	for(;;){` |
|    555581 |  452 | `		if( pNode == 0 ){` |
|    280671 |  453 | `			break;` |
|         - |  454 | `		}` |
|    274910 |  455 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    273405 |  456 | `			&& pNode->nHash == nHash` |
|    164779 |  457 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     57663 |  458 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  459 | `				/* Node found */` |
|     57567 |  460 | `				if( ppNode ){` |
|     57539 |  461 | `					*ppNode = pNode;` |
|     28767 |  462 | `				}` |
|     57567 |  463 | `				return SXRET_OK;` |
|         - |  464 | `		}` |
|         - |  465 | `		/* Follow the collision link */` |
|    217353 |  466 | `		pNode = pNode->pNextCollide;` |
|         5 |  467 | `	}` |
|         - |  468 | `	/* No such entry */` |
|    280671 |  469 | `	return SXERR_NOTFOUND;` |
|    185993 |  470 | `}` |
|         - |  471 | `/*` |
|         - |  472 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  473 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  474 | ` */` |
|    372106 |  475 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  476 | `{` |
|    372111 |  477 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    372111 |  478 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  479 | `	const char *zDigit;` |
|    372111 |  480 | `	int isNeg = FALSE, nDigit;` |
|    372111 |  481 | `	if( zIn >= zEnd ){` |
|       ! 0 |  482 | `		return FALSE;` |
|         - |  483 | `	}` |
|    372111 |  484 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  485 | `		/* Octal not decimal number */` |
|         5 |  486 | `		return FALSE;` |
|         - |  487 | `	}` |
|    372107 |  488 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  489 | `		isNeg = (zIn[0] == '-');` |
|         5 |  490 | `		zIn++;` |
|         2 |  491 | `	}` |
|    372107 |  492 | `	zDigit = zIn;` |
|    186483 |  493 | `	for(;;){` |
|    372971 |  494 | `		if( zIn >= zEnd ){` |
|       249 |  495 | `			break;` |
|         - |  496 | `		}` |
|    372723 |  497 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  498 | `			/* Key does not look like a decimal number */` |
|    371859 |  499 | `			return FALSE;` |
|         - |  500 | `		}` |
|       865 |  501 | `		zIn++;` |
|         1 |  502 | `	}` |
|         - |  503 | `	/* An all-digit key that overflows the signed 64-bit range is NOT an integer` |
|         - |  504 | `	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip` |
|         - |  505 | `	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to` |
|         - |  506 | `	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */` |
|       249 |  507 | `	nDigit = (int)(zEnd - zDigit);` |
|       249 |  508 | `	if( nDigit < 1 ){` |
|         - |  509 | `		/* A lone sign ("-"/"+") */` |
|       ! 0 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|       253 |  512 | `	if( nDigit > 19 \|\|` |
|       127 |  513 | `		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){` |
|         7 |  514 | `		return FALSE;` |
|         - |  515 | `	}` |
|       243 |  516 | `	return TRUE;` |
|    186058 |  517 | `}` |
|         - |  518 | `/*` |
|         - |  519 | ` * Check if a given key exists in the given hashmap.` |
|         - |  520 | ` * Write a pointer to the target node on success.` |
|         - |  521 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  522 | ` */` |
|    135330 |  523 | `static sxi32 HashmapLookup(` |
|         - |  524 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  525 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  526 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  527 | `	)` |
|         5 |  528 | `{` |
|    135335 |  529 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  530 | `	sxi32 rc;` |
|    135335 |  531 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    132963 |  532 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  533 | `			/* Force a string cast */` |
|       ! 0 |  534 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  535 | `		}` |
|    132963 |  536 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  537 | `			/* Perform a blob lookup */` |
|    132943 |  538 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    132943 |  539 | `			goto result;` |
|         - |  540 | `		}` |
|        10 |  541 | `	}` |
|         - |  542 | `	/* Perform an int lookup */` |
|      2397 |  543 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  544 | `		/* Force an integer cast */` |
|        35 |  545 | `		PH7_MemObjToInteger(pKey);` |
|        17 |  546 | `	}` |
|         - |  547 | `	/* Perform an int lookup */` |
|      2397 |  548 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     67665 |  549 | `result:` |
|    135335 |  550 | `	if( rc == SXRET_OK ){` |
|         - |  551 | `		/* Node found */` |
|     59517 |  552 | `		if( ppNode ){` |
|     59471 |  553 | `			*ppNode = pNode;` |
|     29733 |  554 | `		}` |
|     59517 |  555 | `		return SXRET_OK;` |
|         - |  556 | `	}` |
|         - |  557 | `	/* No such entry */` |
|     75823 |  558 | `	return SXERR_NOTFOUND;` |
|     67670 |  559 | `}` |
|         - |  560 | `/*` |
|         - |  561 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  562 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  563 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  564 | ` * via HashmapAppendIndexBusy.` |
|         - |  565 | ` */` |
|   2140986 |  566 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  567 | `{` |
|   2140991 |  568 | `	if( iKey >= pMap->iNextIdx ){` |
|   2140729 |  569 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  570 | `		/* Make sure the automatic index is not reserved */` |
|   2140729 |  571 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  572 | `			pMap->iNextIdx++;` |
|       ! 0 |  573 | `		}` |
|   1070362 |  574 | `	}` |
|   2140991 |  575 | `}` |
|         - |  576 | `/*` |
|         - |  577 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  578 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  579 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  580 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  581 | ` */` |
|    979738 |  582 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  583 | `{` |
|    979743 |  584 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  585 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  586 | `		return TRUE;` |
|         - |  587 | `	}` |
|    979737 |  588 | `	return FALSE;` |
|    489874 |  589 | `}` |
|         - |  590 | `/*` |
|         - |  591 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  592 | ` * hashmap.` |
|         - |  593 | ` * If a node with the given key already exists in the database` |
|         - |  594 | ` * then this function overwrite the old value.` |
|         - |  595 | ` */` |
|   3313236 |  596 | `static sxi32 HashmapInsert(` |
|         - |  597 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  598 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  599 | `	ph7_value *pVal    /* Node value */` |
|         - |  600 | `	)` |
|         5 |  601 | `{` |
|   3313241 |  602 | `	ph7_hashmap_node *pNode = 0;` |
|   3313241 |  603 | `	sxi32 rc = SXRET_OK;` |
|   3313241 |  604 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    196181 |  605 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  606 | `			/* Force a string cast */` |
|         3 |  607 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  608 | `		}` |
|    196181 |  609 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|      3721 |  610 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  611 | `				/* Automatic index assign */` |
|      3495 |  612 | `				pKey = 0;` |
|      1745 |  613 | `			}` |
|      3721 |  614 | `			goto IntKey;` |
|         - |  615 | `		}` |
|    288695 |  616 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     96230 |  617 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  618 | `				/* Overwrite the old value */` |
|         - |  619 | `				ph7_value *pElem;` |
|       371 |  620 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       371 |  621 | `				if( pElem ){` |
|       371 |  622 | `					if( pVal ){` |
|       371 |  623 | `						PH7_MemObjStore(pVal,pElem);` |
|       187 |  624 | `					}else{` |
|         - |  625 | `						/* Nullify the entry */` |
|       ! 0 |  626 | `						PH7_MemObjToNull(pElem);` |
|         - |  627 | `					}` |
|       184 |  628 | `				}` |
|       371 |  629 | `				return SXRET_OK;` |
|         - |  630 | `		}` |
|    192097 |  631 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  632 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  633 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       131 |  634 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  635 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  636 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  637 | `				return SXRET_OK;` |
|         - |  638 | `			}` |
|       196 |  639 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       130 |  640 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        65 |  641 | `				pVal,SXU32_HIGH);` |
|         - |  642 | `		}` |
|         - |  643 | `		/* Perform a blob-key insertion */` |
|    191967 |  644 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    191967 |  645 | `		return rc;` |
|         - |  646 | `	}` |
|   1558530 |  647 | `IntKey:` |
|   3120781 |  648 | `	if( pKey ){` |
|   2141073 |  649 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  650 | `			/* Force an integer cast */` |
|       259 |  651 | `			PH7_MemObjToInteger(pKey);` |
|       129 |  652 | `		}` |
|   2141073 |  653 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  654 | `			/* Overwrite the old value */` |
|         - |  655 | `			ph7_value *pElem;` |
|        87 |  656 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|        87 |  657 | `			if( pElem ){` |
|        87 |  658 | `				if( pVal ){` |
|        87 |  659 | `					PH7_MemObjStore(pVal,pElem);` |
|        44 |  660 | `				}else{` |
|         - |  661 | `					/* Nullify the entry */` |
|       ! 0 |  662 | `					PH7_MemObjToNull(pElem);` |
|         - |  663 | `				}` |
|        43 |  664 | `			}` |
|        87 |  665 | `			return SXRET_OK;` |
|         - |  666 | `		}` |
|   2140987 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  668 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  669 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  670 | `			char zKey[24];` |
|         3 |  671 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  672 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  673 | `		}` |
|         - |  674 | `		/* Perform a 64-bit-int-key insertion */` |
|   2140985 |  675 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2140985 |  676 | `		if( rc == SXRET_OK ){` |
|   2140985 |  677 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1070490 |  678 | `		}` |
|   1070495 |  679 | `	}else{` |
|    979713 |  680 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  681 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  682 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  683 | `		}` |
|    979711 |  684 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  685 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  686 | `		}` |
|         - |  687 | `		/* Assign an automatic index */` |
|    979705 |  688 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|    979705 |  689 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|    979703 |  690 | `			++pMap->iNextIdx;` |
|    489849 |  691 | `		}` |
|         - |  692 | `	}` |
|         - |  693 | `	/* Insertion result */` |
|   3120685 |  694 | `	return rc;` |
|   1656623 |  695 | `}` |
|         - |  696 | `/*` |
|         - |  697 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - |  698 | ` * hashmap.` |
|         - |  699 | ` * This is insertion by reference so be careful to mark the node` |
|         - |  700 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - |  701 | ` * The insertion by reference is triggered when the following` |
|         - |  702 | ` * expression is encountered.` |
|         - |  703 | ` * $var = 10;` |
|         - |  704 | ` *  $a = array(&var);` |
|         - |  705 | ` * OR` |
|         - |  706 | ` *  $a[] =& $var;` |
|         - |  707 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - |  708 | ` * over it's contents.` |
|         - |  709 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - |  710 | ` * removed when the foreign ph7_value is unset.` |
|         - |  711 | ` * Example:` |
|         - |  712 | ` *  $var = 10;` |
|         - |  713 | ` *  $a[] =& $var;` |
|         - |  714 | ` *  echo count($a).PHP_EOL; //1` |
|         - |  715 | ` *  //Unset the foreign ph7_value now` |
|         - |  716 | ` *  unset($var);` |
|         - |  717 | ` *  echo count($a); //0` |
|         - |  718 | ` * Note that this is a PH7 eXtension.` |
|         - |  719 | ` * Refer to the official documentation for more information.` |
|         - |  720 | ` * If a node with the given key already exists in the database` |
|         - |  721 | ` * then this function overwrite the old value.` |
|         - |  722 | ` */` |
|     46504 |  723 | `static sxi32 HashmapInsertByRef(` |
|         - |  724 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  725 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  726 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  727 | `	)` |
|         5 |  728 | `{` |
|     46509 |  729 | `	ph7_hashmap_node *pNode = 0;` |
|     46509 |  730 | `	sxi32 rc = SXRET_OK;` |
|     46509 |  731 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     46473 |  732 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  733 | `			/* Force a string cast */` |
|       ! 0 |  734 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  735 | `		}` |
|     46473 |  736 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  737 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  738 | `				/* Automatic index assign */` |
|       ! 0 |  739 | `				pKey = 0;` |
|       ! 0 |  740 | `			}` |
|         3 |  741 | `			goto IntKey;` |
|         - |  742 | `		}` |
|     69704 |  743 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     23233 |  744 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  745 | `				/* Overwrite */` |
|         7 |  746 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|         7 |  747 | `				pNode->nValIdx = nRefIdx;` |
|         - |  748 | `				/* Install in the reference table */` |
|         7 |  749 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|         7 |  750 | `				return SXRET_OK;` |
|         - |  751 | `		}` |
|         - |  752 | `		/* Perform a blob-key insertion */` |
|     46465 |  753 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     46465 |  754 | `		return rc;` |
|         - |  755 | `	}` |
|        18 |  756 | `IntKey:` |
|        39 |  757 | `	if( pKey ){` |
|         7 |  758 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  759 | `			/* Force an integer cast */` |
|         3 |  760 | `			PH7_MemObjToInteger(pKey);` |
|         1 |  761 | `		}` |
|         7 |  762 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  763 | `			/* Overwrite */` |
|       ! 0 |  764 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       ! 0 |  765 | `			pNode->nValIdx = nRefIdx;` |
|         - |  766 | `			/* Install in the reference table */` |
|       ! 0 |  767 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       ! 0 |  768 | `			return SXRET_OK;` |
|         - |  769 | `		}` |
|         - |  770 | `		/* Perform a 64-bit-int-key insertion */` |
|         7 |  771 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|         7 |  772 | `		if( rc == SXRET_OK ){` |
|         7 |  773 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|         3 |  774 | `		}` |
|         4 |  775 | `	}else{` |
|        33 |  776 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  777 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  778 | `		}` |
|         - |  779 | `		/* Assign an automatic index */` |
|        33 |  780 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|        33 |  781 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|        33 |  782 | `			++pMap->iNextIdx;` |
|        16 |  783 | `		}` |
|         - |  784 | `	}` |
|         - |  785 | `	/* Insertion result */` |
|        39 |  786 | `	return rc;` |
|     23257 |  787 | `}` |
|         - |  788 | `/*` |
|         - |  789 | ` * Extract node value.` |
|         - |  790 | ` */` |
|   1395935 |  791 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  792 | `{` |
|         - |  793 | `	/* Point to the desired object */` |
|         - |  794 | `	ph7_value *pObj;` |
|   1395940 |  795 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1395940 |  796 | `	return pObj;` |
|         5 |  797 | `}` |
|         - |  798 | `/*` |
|         - |  799 | ` * Insert a node in the given hashmap.` |
|         - |  800 | ` * If a node with the given key already exists in the database` |
|         - |  801 | ` * then this function overwrite the old value.` |
|         - |  802 | ` */` |
|       448 |  803 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  804 | `{` |
|         - |  805 | `	ph7_value *pObj;` |
|         - |  806 | `	sxi32 rc;` |
|         - |  807 | `	/* Extract the node value */` |
|       453 |  808 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|       453 |  809 | `	if( pObj == 0 ){` |
|       ! 0 |  810 | `		return SXERR_EMPTY;` |
|         - |  811 | `	}` |
|         - |  812 | `	/* Preserve key */` |
|       453 |  813 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  814 | `		/* Int64 key */` |
|       321 |  815 | `		if( !bPreserve ){` |
|         - |  816 | `			/* Assign an automatic index */` |
|       173 |  817 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        89 |  818 | `		}else{` |
|       149 |  819 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  820 | `		}` |
|       163 |  821 | `	}else{` |
|         - |  822 | `		/* Blob key */` |
|       133 |  823 | `		if( !bPreserve ){` |
|         - |  824 | `			/* treat it like an automatically-indexed element, drop the` |
|         - |  825 | `			 * original string key entirely */` |
|        35 |  826 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        18 |  827 | `		}else{` |
|       148 |  828 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|        49 |  829 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - |  830 | `		}` |
|         - |  831 | `	}` |
|       453 |  832 | `	return rc;` |
|       229 |  833 | `}` |
|         - |  834 | `/*` |
|         - |  835 | ` * Compare two node values.` |
|         - |  836 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  837 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  838 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  839 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  840 | ` * documenation.` |
|         - |  841 | ` */` |
|     70470 |  842 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  843 | `{` |
|         - |  844 | `	ph7_value sObj1,sObj2;` |
|         - |  845 | `	sxi32 rc;` |
|     70475 |  846 | `	if( pLeft == pRight ){` |
|         - |  847 | `		/*` |
|         - |  848 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  849 | `		 * below for more information on this sceanario.` |
|         - |  850 | `		 */` |
|       ! 0 |  851 | `		return 0;` |
|         - |  852 | `	}` |
|         - |  853 | `	/* Do the comparison */` |
|     70475 |  854 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     70475 |  855 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     70475 |  856 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     70475 |  857 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     70475 |  858 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     70475 |  859 | `	PH7_MemObjRelease(&sObj1);` |
|     70475 |  860 | `	PH7_MemObjRelease(&sObj2);` |
|     70475 |  861 | `	return rc;` |
|     35282 |  862 | `}` |
|         - |  863 | `/*` |
|         - |  864 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  865 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  866 | ` */` |
|     13554 |  867 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  868 | `{` |
|     13559 |  869 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  870 | `	sxu32 nBucket;` |
|         - |  871 | `	/* Remove old collision links */` |
|     13559 |  872 | `	if( pEntry->pPrevCollide ){` |
|     11115 |  873 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5558 |  874 | `	}else{` |
|      2449 |  875 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  876 | `	}` |
|     13559 |  877 | `	if( pEntry->pNextCollide ){` |
|      1120 |  878 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       554 |  879 | `	}` |
|     13559 |  880 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  881 | `	/* Compute the new hash */` |
|     13559 |  882 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     13559 |  883 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     13559 |  884 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  885 | `	/* Link to the new bucket */` |
|     13559 |  886 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13559 |  887 | `	if( pMap->apBucket[nBucket] ){` |
|     11455 |  888 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5718 |  889 | `	}` |
|     13559 |  890 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13559 |  891 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  892 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  893 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  894 | `	 * the no-overflow invariant uniform). */` |
|     13559 |  895 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     13559 |  896 | `		pMap->iNextIdx++;` |
|      6777 |  897 | `	}` |
|     13559 |  898 | `}` |
|         - |  899 | `/*` |
|         - |  900 | ` * Perform a linear search on a given hashmap.` |
|         - |  901 | ` * Write a pointer to the target node on success.` |
|         - |  902 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  903 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  904 | ` * for more information.` |
|         - |  905 | ` */` |
|     32914 |  906 | `static int HashmapFindValue(` |
|         - |  907 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  908 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - |  909 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - |  910 | `	int bStrict      /* TRUE for strict comparison */` |
|         - |  911 | `	)` |
|         5 |  912 | `{` |
|         - |  913 | `	ph7_hashmap_node *pEntry;` |
|         - |  914 | `	ph7_value sVal,*pVal;` |
|         - |  915 | `	ph7_value sNeedle;` |
|         - |  916 | `	sxi32 rc;` |
|         - |  917 | `	sxu32 n;` |
|         - |  918 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     32919 |  919 | `	pEntry = pMap->pFirst;` |
|     32919 |  920 | `	n = pMap->nEntry;` |
|     32919 |  921 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     32919 |  922 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     78112 |  923 | `	for(;;){` |
|    156230 |  924 | `		if( n < 1 ){` |
|       111 |  925 | `			break;` |
|         - |  926 | `		}` |
|         - |  927 | `		/* Extract node value */` |
|    156120 |  928 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    156120 |  929 | `		if( pVal ){` |
|         - |  930 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  931 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  932 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  933 | `			 * so null needles/values take the same path as everything else` |
|         - |  934 | `			 * (the historical null-to-null shortcut here made` |
|         - |  935 | `			 * in_array(null, [""]) false where php says true). */` |
|    156120 |  936 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    156120 |  937 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    156120 |  938 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    156120 |  939 | `			PH7_MemObjRelease(&sVal);` |
|    156120 |  940 | `			PH7_MemObjRelease(&sNeedle);` |
|    156120 |  941 | `			if( rc == 0 ){` |
|     32809 |  942 | `				if( ppNode ){` |
|        23 |  943 | `					*ppNode = pEntry;` |
|        11 |  944 | `				}` |
|         - |  945 | `				/* Match found*/` |
|     32809 |  946 | `				return SXRET_OK;` |
|         - |  947 | `			}` |
|     61655 |  948 | `		}` |
|         - |  949 | `		/* Point to the next entry */` |
|    123316 |  950 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    123316 |  951 | `		n--;` |
|         5 |  952 | `	}` |
|         - |  953 | `	/* No such entry */` |
|       111 |  954 | `	return SXERR_NOTFOUND;` |
|     16462 |  955 | `}` |
|         - |  956 | `/*` |
|         - |  957 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|         - |  958 | ` * for values comparison.` |
|         - |  959 | ` * Write a pointer to the target node on success.` |
|         - |  960 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  961 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|         - |  962 | ` * for more information.` |
|         - |  963 | ` */` |
|        22 |  964 | `static int HashmapFindValueByCallback(` |
|         - |  965 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|         - |  966 | `	ph7_value *pNeedle,    /* Lookup key */` |
|         - |  967 | `	ph7_value *pCallback,  /* User defined callback */` |
|         - |  968 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|         - |  969 | `	)` |
|         1 |  970 | `{` |
|         - |  971 | `	ph7_hashmap_node *pEntry;` |
|         - |  972 | `	ph7_value sResult,*pVal;` |
|         - |  973 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|         - |  974 | `	sxi32 rc;` |
|         - |  975 | `	sxu32 n;` |
|        23 |  976 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|         - |  977 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|         - |  978 | `		 * exception is not thrown again, and let the caller wind down. */` |
|       ! 0 |  979 | `		return SXERR_NOTFOUND;` |
|         - |  980 | `	}` |
|         - |  981 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|        23 |  982 | `	pEntry = pMap->pFirst;` |
|        23 |  983 | `	n = pMap->nEntry;` |
|         - |  984 | `	/* Store callback result here */` |
|        23 |  985 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|         - |  986 | `	/* First argument to the callback */` |
|        23 |  987 | `	apArg[0] = pNeedle;` |
|        25 |  988 | `	for(;;){` |
|        51 |  989 | `		if( n < 1 ){` |
|         9 |  990 | `			break;` |
|         - |  991 | `		}` |
|         - |  992 | `		/* Extract node value */` |
|        43 |  993 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        43 |  994 | `		if( pVal ){` |
|         - |  995 | `			/* Invoke the user callback */` |
|        43 |  996 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|        43 |  997 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|        43 |  998 | `			if( rc == PH7_EXCEPTION ){` |
|         - |  999 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|         - | 1000 | `				 * and report no match for the rest of the run. */` |
|         5 | 1001 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|         5 | 1002 | `				PH7_MemObjRelease(&sResult);` |
|         5 | 1003 | `				return SXERR_NOTFOUND;` |
|         - | 1004 | `			}` |
|        39 | 1005 | `			if( rc == SXRET_OK ){` |
|         - | 1006 | `				/* Extract callback result */` |
|        39 | 1007 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 1008 | `					/* Perform an int cast */` |
|       ! 0 | 1009 | `					PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 1010 | `				}` |
|        39 | 1011 | `				rc = (sxi32)sResult.x.iVal;` |
|        39 | 1012 | `				PH7_MemObjRelease(&sResult);` |
|        39 | 1013 | `				if( rc == 0 ){` |
|         - | 1014 | `					/* Match found*/` |
|        11 | 1015 | `					if( ppNode ){` |
|       ! 0 | 1016 | `						*ppNode = pEntry;` |
|       ! 0 | 1017 | `					}` |
|        11 | 1018 | `					return SXRET_OK;` |
|         - | 1019 | `				}` |
|        14 | 1020 | `			}` |
|        14 | 1021 | `		}` |
|         - | 1022 | `		/* Point to the next entry */` |
|        29 | 1023 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 1024 | `		n--;` |
|         1 | 1025 | `	}` |
|         - | 1026 | `	/* No such entry */` |
|         9 | 1027 | `	return SXERR_NOTFOUND;` |
|        12 | 1028 | `}` |
|         - | 1029 | `/*` |
|         - | 1030 | ` * Compare two hashmaps.` |
|         - | 1031 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1032 | ` * Note on array comparison operators.` |
|         - | 1033 | ` *  According to the PHP language reference manual.` |
|         - | 1034 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1035 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1036 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1037 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1038 | ` *                          order and of the same types.` |
|         - | 1039 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1040 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1041 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1042 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1043 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1044 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1045 | ` * <?php` |
|         - | 1046 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1047 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1048 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1049 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1050 | ` * var_dump($c);` |
|         - | 1051 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1052 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1053 | ` * var_dump($c);` |
|         - | 1054 | ` * ?>` |
|         - | 1055 | ` * When executed, this script will print the following:` |
|         - | 1056 | ` * Union of $a and $b:` |
|         - | 1057 | ` * array(3) {` |
|         - | 1058 | ` *  ["a"]=>` |
|         - | 1059 | ` *  string(5) "apple"` |
|         - | 1060 | ` *  ["b"]=>` |
|         - | 1061 | ` * string(6) "banana"` |
|         - | 1062 | ` *  ["c"]=>` |
|         - | 1063 | ` * string(6) "cherry"` |
|         - | 1064 | ` * }` |
|         - | 1065 | ` * Union of $b and $a:` |
|         - | 1066 | ` * array(3) {` |
|         - | 1067 | ` * ["a"]=>` |
|         - | 1068 | ` * string(4) "pear"` |
|         - | 1069 | ` * ["b"]=>` |
|         - | 1070 | ` * string(10) "strawberry"` |
|         - | 1071 | ` * ["c"]=>` |
|         - | 1072 | ` * string(6) "cherry"` |
|         - | 1073 | ` * }` |
|         - | 1074 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1075 | ` */` |
|        30 | 1076 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1077 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1078 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1079 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1080 | `	)` |
|         1 | 1081 | `{` |
|         - | 1082 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1083 | `	sxi32 rc;` |
|         - | 1084 | `	sxu32 n;` |
|        31 | 1085 | `	if( pLeft == pRight ){` |
|         - | 1086 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1087 | `		 * Unlike the zend engine.` |
|         - | 1088 | `		 */` |
|         3 | 1089 | `		return 0;` |
|         - | 1090 | `	}` |
|        29 | 1091 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1092 | `		/* Must have the same number of entries */` |
|         5 | 1093 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1094 | `	}` |
|         - | 1095 | `	/* Point to the first inserted entry of the left hashmap */` |
|        25 | 1096 | `	pLe = pLeft->pFirst;` |
|        25 | 1097 | `	pRe = 0; /* cc warning */` |
|         - | 1098 | `	/* Perform the comparison */` |
|        25 | 1099 | `	n = pLeft->nEntry;` |
|        59 | 1100 | `	for(;;){` |
|       119 | 1101 | `		if( n < 1 ){` |
|        23 | 1102 | `			break;` |
|         - | 1103 | `		}` |
|        97 | 1104 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1105 | `			/* Int key */` |
|        89 | 1106 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|        45 | 1107 | `		}else{` |
|         9 | 1108 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1109 | `			/* Blob key */` |
|         9 | 1110 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1111 | `		}` |
|        97 | 1112 | `		if( rc != SXRET_OK ){` |
|         - | 1113 | `			/* No such entry in the right side */` |
|       ! 0 | 1114 | `			return 1;` |
|         - | 1115 | `		}` |
|        97 | 1116 | `		rc = 0;` |
|        97 | 1117 | `		if( bStrict ){` |
|         - | 1118 | `			/* Make sure,the keys are of the same type */` |
|        81 | 1119 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1120 | `				rc = 1;` |
|       ! 0 | 1121 | `			}` |
|        40 | 1122 | `		}` |
|        97 | 1123 | `		if( !rc ){` |
|         - | 1124 | `			/* Compare nodes */` |
|        97 | 1125 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|        48 | 1126 | `		}` |
|        97 | 1127 | `		if( rc != 0 ){` |
|         - | 1128 | `			/* Nodes key/value differ */` |
|         3 | 1129 | `			return rc;` |
|         - | 1130 | `		}` |
|         - | 1131 | `		/* Point to the next entry */` |
|        95 | 1132 | `		pLe = pLe->pPrev; /* Reverse link */` |
|        95 | 1133 | `		n--;` |
|         1 | 1134 | `	}` |
|        23 | 1135 | `	return 0; /* Hashmaps are equals */` |
|        16 | 1136 | `}` |
|         - | 1137 | `/*` |
|         - | 1138 | ` * Duplicate a hashmap node.` |
|         - | 1139 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1140 | ` */` |
|    647162 | 1141 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1142 | `	ph7_hashmap *pDest,` |
|         - | 1143 | `	ph7_hashmap_node *pEntry,` |
|         - | 1144 | `	ph7_value *pVal,` |
|         - | 1145 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1146 | `	)` |
|         5 | 1147 | `{` |
|         - | 1148 | `	ph7_value sSafeVal;` |
|         - | 1149 | `	ph7_value sKey;` |
|         - | 1150 | `	sxi32 rc;` |
|         - | 1151 |  |
|    647167 | 1152 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|         - | 1153 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|         - | 1154 | `		 * Re-insert it by reference so the reference survives the duplication` |
|         - | 1155 | `		 * instead of being flattened to a value copy. This keeps spread` |
|         - | 1156 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|         - | 1157 | `		 * with PHP semantics. */` |
|         7 | 1158 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|         7 | 1159 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         3 | 1160 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|         3 | 1161 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|         3 | 1162 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|         3 | 1163 | `			PH7_MemObjRelease(&sKey);` |
|         2 | 1164 | `		}else{` |
|         5 | 1165 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         5 | 1166 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|         2 | 1167 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1168 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1169 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1170 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1171 | `			}else{ /* Dup: preserve the int key */` |
|       ! 0 | 1172 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1173 | `			}` |
|         - | 1174 | `		}` |
|         7 | 1175 | `		return rc;` |
|         - | 1176 | `	}` |
|    647161 | 1177 | `	sSafeVal = *pVal;` |
|         - | 1178 |  |
|    647161 | 1179 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1180 | `		/* Blob key insertion */` |
|      4033 | 1181 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4033 | 1182 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4033 | 1183 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4033 | 1184 | `		PH7_MemObjRelease(&sKey);` |
|      2019 | 1185 | `	}else{` |
|         - | 1186 | `		/* Int key */` |
|    643133 | 1187 | `		if( iAction == 0 ){ /* Merge */` |
|    642913 | 1188 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    321677 | 1189 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1190 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1191 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1192 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1193 | `		}else{ /* Dup */` |
|       193 | 1194 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1195 | `		}` |
|         - | 1196 | `	}` |
|    647161 | 1197 | `	return rc;` |
|    323586 | 1198 | `}` |
|         - | 1199 | `/*` |
|         - | 1200 | ` * Merge two hashmaps.` |
|         - | 1201 | ` * Note on the merge process` |
|         - | 1202 | ` * According to the PHP language reference manual.` |
|         - | 1203 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1204 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1205 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1206 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1207 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1208 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1209 | ` *  keys starting from zero in the result array.` |
|         - | 1210 | ` */` |
|      2142 | 1211 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1212 | `{` |
|         - | 1213 | `	ph7_hashmap_node *pEntry;` |
|         - | 1214 | `	ph7_value *pVal;` |
|         - | 1215 | `	sxi32 rc;` |
|         - | 1216 | `	sxu32 n;` |
|      2147 | 1217 | `	if( pSrc == pDest ){` |
|         - | 1218 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1219 | `		 * Unlike the zend engine.` |
|         - | 1220 | `		 */` |
|       ! 0 | 1221 | `		return SXRET_OK;` |
|         - | 1222 | `	}` |
|         - | 1223 | `	/* Point to the first inserted entry in the source */` |
|      2147 | 1224 | `	pEntry = pSrc->pFirst;` |
|         - | 1225 | `	/* Perform the merge */` |
|    645113 | 1226 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1227 | `		/* Extract the node value */` |
|    642971 | 1228 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    642971 | 1229 | `		if( pVal ){` |
|         - | 1230 | `			/* Make a local copy of the value.` |
|         - | 1231 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1232 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1233 | `			 * to the old pool.` |
|         - | 1234 | `			 */` |
|    642971 | 1235 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    321488 | 1236 | `		}else{` |
|       ! 0 | 1237 | `			rc = SXRET_OK;` |
|         - | 1238 | `		}` |
|    642971 | 1239 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1240 | `			return rc;` |
|         - | 1241 | `		}` |
|         - | 1242 | `		/* Point to the next entry */` |
|    642971 | 1243 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    321488 | 1244 | `	}` |
|      2147 | 1245 | `	return SXRET_OK;` |
|      1076 | 1246 | `}` |
|         - | 1247 | `/*` |
|         - | 1248 | ` * Overwrite entries with the same key.` |
|         - | 1249 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1250 | ` *  According to the PHP language reference manual.` |
|         - | 1251 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1252 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1253 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1254 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1255 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1256 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1257 | ` *  overwriting the previous values.` |
|         - | 1258 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1259 | ` *  by whatever type is in the second array.` |
|         - | 1260 | ` */` |
|        34 | 1261 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1262 | `{` |
|         - | 1263 | `	ph7_hashmap_node *pEntry;` |
|         - | 1264 | `	ph7_value *pVal;` |
|         - | 1265 | `	sxi32 rc;` |
|         - | 1266 | `	sxu32 n;` |
|        36 | 1267 | `	if( pSrc == pDest ){` |
|         - | 1268 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1269 | `		 * Unlike the zend engine.` |
|         - | 1270 | `		 */` |
|       ! 0 | 1271 | `		return SXRET_OK;` |
|         - | 1272 | `	}` |
|         - | 1273 | `	/* Point to the first inserted entry in the source */` |
|        36 | 1274 | `	pEntry = pSrc->pFirst;` |
|         - | 1275 | `	/* Perform the merge */` |
|        80 | 1276 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1277 | `		/* Extract the node value */` |
|        46 | 1278 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        46 | 1279 | `		if( pVal ){` |
|        46 | 1280 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        24 | 1281 | `		}else{` |
|       ! 0 | 1282 | `			rc = SXRET_OK;` |
|         - | 1283 | `		}` |
|        46 | 1284 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1285 | `			return rc;` |
|         - | 1286 | `		}` |
|         - | 1287 | `		/* Point to the next entry */` |
|        46 | 1288 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1289 | `	}` |
|        36 | 1290 | `	return SXRET_OK;` |
|        19 | 1291 | `}` |
|         - | 1292 | `/*` |
|         - | 1293 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1294 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1295 | ` */` |
|      3924 | 1296 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1297 | `{` |
|         - | 1298 | `	ph7_hashmap_node *pEntry;` |
|         - | 1299 | `	ph7_value *pVal;` |
|         - | 1300 | `	sxi32 rc;` |
|         - | 1301 | `	sxu32 n;` |
|      3929 | 1302 | `	if( pSrc == pDest ){` |
|         - | 1303 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1304 | `		 * Unlike the zend engine.` |
|         - | 1305 | `		 */` |
|       ! 0 | 1306 | `		return SXRET_OK;` |
|         - | 1307 | `	}` |
|         - | 1308 | `	/* Point to the first inserted entry in the source */` |
|      3929 | 1309 | `	pEntry = pSrc->pFirst;` |
|         - | 1310 | `	/* Perform the duplication */` |
|      8081 | 1311 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1312 | `		/* Extract the node value */` |
|      4157 | 1313 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4157 | 1314 | `		if( pVal ){` |
|      4157 | 1315 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2081 | 1316 | `		}else{` |
|       ! 0 | 1317 | `			rc = SXRET_OK;` |
|         - | 1318 | `		}` |
|      4157 | 1319 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1320 | `			return rc;` |
|         - | 1321 | `		}` |
|         - | 1322 | `		/* Point to the next entry */` |
|      4157 | 1323 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2081 | 1324 | `	}` |
|      3929 | 1325 | `	return SXRET_OK;` |
|      1967 | 1326 | `}` |
|         - | 1327 | `/*` |
|         - | 1328 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1329 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1330 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1331 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1332 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1333 | ` * this instead of PH7_HashmapDup.` |
|         - | 1334 | ` */` |
|        12 | 1335 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1336 | `{` |
|         - | 1337 | `	ph7_hashmap_node *pEntry;` |
|         - | 1338 | `	ph7_value *pVal;` |
|         - | 1339 | `	sxi32 rc;` |
|         - | 1340 | `	sxu32 n;` |
|        13 | 1341 | `	if( pSrc == pDest ){` |
|       ! 0 | 1342 | `		return SXRET_OK;` |
|         - | 1343 | `	}` |
|        13 | 1344 | `	pEntry = pSrc->pFirst;` |
|       739 | 1345 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1346 | `		/* Extract the node value (resolves foreign references) */` |
|       727 | 1347 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       726 | 1348 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       485 | 1349 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1350 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1351 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1352 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1353 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1354 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1355 | `			pVal = 0;` |
|         2 | 1356 | `		}` |
|       727 | 1357 | `		if( pVal ){` |
|       723 | 1358 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1078 | 1359 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       359 | 1360 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       360 | 1361 | `			}else{` |
|         5 | 1362 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1363 | `			}` |
|       723 | 1364 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1365 | `				return rc;` |
|         - | 1366 | `			}` |
|       361 | 1367 | `		}` |
|         - | 1368 | `		/* Point to the next entry */` |
|       727 | 1369 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       364 | 1370 | `	}` |
|        13 | 1371 | `	return SXRET_OK;` |
|         7 | 1372 | `}` |
|         - | 1373 | `/*` |
|         - | 1374 | ` * Copy-on-write separation for arrays.` |
|         - | 1375 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1376 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1377 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1378 | ` */` |
|    224426 | 1379 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1380 | `{` |
|    224431 | 1381 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1382 | `	ph7_hashmap *pNew;` |
|         - | 1383 | `	ph7_value *pBacking;` |
|         - | 1384 | `	sxu32 nValIdx;` |
|         - | 1385 | `	int bValueInPool;` |
|    224431 | 1386 | `	if( pMap->iRef < 2 ){` |
|         - | 1387 | `		/* Sole owner, no separation needed */` |
|    222099 | 1388 | `		return pMap;` |
|         - | 1389 | `	}` |
|      2337 | 1390 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1391 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1392 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1393 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1394 | `		return pMap;` |
|         - | 1395 | `	}` |
|         - | 1396 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1397 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1398 | `	 * frame is popped. */` |
|      2211 | 1399 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2211 | 1400 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2206 | 1401 | `		if( pBacking && pBacking != pValue` |
|      2186 | 1402 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2171 | 1403 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1404 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2171 | 1405 | `			pMap->iRef--;` |
|      2171 | 1406 | `			if( pMap->iRef < 2 ){` |
|         - | 1407 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2129 | 1408 | `				pMap->iRef++;` |
|      2129 | 1409 | `				return pMap;` |
|         - | 1410 | `			}` |
|        44 | 1411 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|        44 | 1412 | `			if( pNew == 0 ){` |
|       ! 0 | 1413 | `				pMap->iRef++;` |
|       ! 0 | 1414 | `				return pMap;` |
|         - | 1415 | `			}` |
|        44 | 1416 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1417 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1418 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1419 | `				pMap->iRef++;` |
|       ! 0 | 1420 | `				return pMap;` |
|         - | 1421 | `			}` |
|        44 | 1422 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|        44 | 1423 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1424 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1425 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1426 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1427 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1428 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1429 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1430 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|        44 | 1431 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|        44 | 1432 | `			if( pBacking ){` |
|        44 | 1433 | `				pBacking->x.pOther = pNew;` |
|        21 | 1434 | `			}` |
|         - | 1435 | `			/* Update the stack value to match */` |
|        44 | 1436 | `			pValue->x.pOther = pNew;` |
|        44 | 1437 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|        44 | 1438 | `			return pNew;` |
|         - | 1439 | `		}` |
|        20 | 1440 | `	}` |
|         - | 1441 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1442 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1443 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1444 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1445 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1446 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1447 | `	 * pBacking in the backing-variable branch above). */` |
|        41 | 1448 | `	nValIdx = pValue->nIdx;` |
|        61 | 1449 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        40 | 1450 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        41 | 1451 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        41 | 1452 | `	if( pNew == 0 ){` |
|         - | 1453 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1454 | `		return pMap;` |
|         - | 1455 | `	}` |
|        41 | 1456 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1457 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1458 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1459 | `		return pMap;` |
|         - | 1460 | `	}` |
|        41 | 1461 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        41 | 1462 | `	pMap->iRef--;` |
|        41 | 1463 | `	if( bValueInPool ){` |
|         - | 1464 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        41 | 1465 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        41 | 1466 | `		if( pValue == 0 ){` |
|       ! 0 | 1467 | `			return pNew;` |
|         - | 1468 | `		}` |
|        20 | 1469 | `	}` |
|        41 | 1470 | `	pValue->x.pOther = pNew;` |
|        41 | 1471 | `	return pNew;` |
|    112218 | 1472 | `}` |
|         - | 1473 | `/*` |
|         - | 1474 | ` * Perform the union of two hashmaps.` |
|         - | 1475 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1476 | ` * with a variable holding an array as follows:` |
|         - | 1477 | ` * <?php` |
|         - | 1478 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1479 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1480 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1481 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1482 | ` * var_dump($c);` |
|         - | 1483 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1484 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1485 | ` * var_dump($c);` |
|         - | 1486 | ` * ?>` |
|         - | 1487 | ` * When executed, this script will print the following:` |
|         - | 1488 | ` * Union of $a and $b:` |
|         - | 1489 | ` * array(3) {` |
|         - | 1490 | ` *  ["a"]=>` |
|         - | 1491 | ` *  string(5) "apple"` |
|         - | 1492 | ` *  ["b"]=>` |
|         - | 1493 | ` * string(6) "banana"` |
|         - | 1494 | ` *  ["c"]=>` |
|         - | 1495 | ` * string(6) "cherry"` |
|         - | 1496 | ` * }` |
|         - | 1497 | ` * Union of $b and $a:` |
|         - | 1498 | ` * array(3) {` |
|         - | 1499 | ` * ["a"]=>` |
|         - | 1500 | ` * string(4) "pear"` |
|         - | 1501 | ` * ["b"]=>` |
|         - | 1502 | ` * string(10) "strawberry"` |
|         - | 1503 | ` * ["c"]=>` |
|         - | 1504 | ` * string(6) "cherry"` |
|         - | 1505 | ` * }` |
|         - | 1506 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1507 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1508 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1509 | ` */` |
|      3816 | 1510 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1511 | `{` |
|         - | 1512 | `	ph7_hashmap_node *pEntry;` |
|      3821 | 1513 | `	sxi32 rc = SXRET_OK;` |
|         - | 1514 | `	ph7_value *pObj;` |
|         - | 1515 | `	sxu32 n;` |
|      3821 | 1516 | `	if( pLeft == pRight ){` |
|         - | 1517 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1518 | `		 * Unlike the zend engine.` |
|         - | 1519 | `		 */` |
|       ! 0 | 1520 | `		return SXRET_OK;` |
|         - | 1521 | `	}` |
|         - | 1522 | `	/* Perform the union */` |
|      3821 | 1523 | `	pEntry = pRight->pFirst;` |
|      3855 | 1524 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1525 | `		/* Make sure the given key does not exists in the left array */` |
|        38 | 1526 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1527 | `			/* BLOB key */` |
|        24 | 1528 | `			if( SXRET_OK !=` |
|        20 | 1529 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        20 | 1530 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        20 | 1531 | `					if( pObj ){` |
|        20 | 1532 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1533 | `						/* Perform the insertion */` |
|        20 | 1534 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1535 | `							&sSafeVal,0,FALSE);` |
|        20 | 1536 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1537 | `							return rc;` |
|         - | 1538 | `						}` |
|         8 | 1539 | `					}` |
|         8 | 1540 | `			}` |
|        14 | 1541 | `		}else{` |
|         - | 1542 | `			/* INT key */` |
|        16 | 1543 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        11 | 1544 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        11 | 1545 | `				if( pObj ){` |
|        11 | 1546 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1547 | `					/* Perform the insertion */` |
|        11 | 1548 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        11 | 1549 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1550 | `						return rc;` |
|         - | 1551 | `					}` |
|         5 | 1552 | `				}` |
|         5 | 1553 | `			}` |
|         - | 1554 | `		}` |
|         - | 1555 | `		/* Point to the next entry */` |
|        38 | 1556 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 1557 | `	}` |
|      3821 | 1558 | `	return SXRET_OK;` |
|      1913 | 1559 | `}` |
|         - | 1560 | `/*` |
|         - | 1561 | ` * Allocate a new hashmap.` |
|         - | 1562 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1563 | ` */` |
|    136728 | 1564 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1565 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1566 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1567 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1568 | `	)` |
|         5 | 1569 | `{` |
|         - | 1570 | `	ph7_hashmap *pMap;` |
|         - | 1571 | `	/* Allocate a new instance */` |
|    136733 | 1572 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    136733 | 1573 | `	if( pMap == 0 ){` |
|       ! 0 | 1574 | `		return 0;` |
|         - | 1575 | `	}` |
|         - | 1576 | `	/* Zero the structure */` |
|    136733 | 1577 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1578 | `	/* Fill in the structure */` |
|    136733 | 1579 | `	pMap->pVm = &(*pVm);` |
|    136733 | 1580 | `	pMap->iRef = 1;` |
|         - | 1581 | `	/* Default hash functions */` |
|    136733 | 1582 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    136733 | 1583 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    136733 | 1584 | `	return pMap;` |
|     68369 | 1585 | `}` |
|         - | 1586 | `/*` |
|         - | 1587 | ` * Install superglobals in the given virtual machine.` |
|         - | 1588 | ` * Note on superglobals.` |
|         - | 1589 | ` *  According to the PHP language reference manual.` |
|         - | 1590 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1591 | `*   Description` |
|         - | 1592 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1593 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1594 | `*   global $variable; to access them within functions or methods.` |
|         - | 1595 | `*   These superglobal variables are:` |
|         - | 1596 | `*    $GLOBALS` |
|         - | 1597 | `*    $_SERVER` |
|         - | 1598 | `*    $_GET` |
|         - | 1599 | `*    $_POST` |
|         - | 1600 | `*    $_FILES` |
|         - | 1601 | `*    $_COOKIE` |
|         - | 1602 | `*    $_SESSION` |
|         - | 1603 | `*    $_REQUEST` |
|         - | 1604 | `*    $_ENV` |
|         - | 1605 | `*/` |
|      3480 | 1606 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1607 | `{` |
|         - | 1608 | `	static const char * azSuper[] = {` |
|         - | 1609 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1610 | `		"_GET",      /* $_GET */` |
|         - | 1611 | `		"_POST",     /* $_POST */` |
|         - | 1612 | `		"_FILES",    /* $_FILES */` |
|         - | 1613 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1614 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1615 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1616 | `		"_ENV",      /* $_ENV */` |
|         - | 1617 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1618 | `		"argv"       /* $argv */` |
|         - | 1619 | `	};` |
|         - | 1620 | `	ph7_hashmap *pMap;` |
|         - | 1621 | `	ph7_value *pObj;` |
|         - | 1622 | `	SyString *pFile;` |
|         - | 1623 | `	sxi32 rc;` |
|         - | 1624 | `	sxu32 n;` |
|         - | 1625 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      3485 | 1626 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3485 | 1627 | `	if( pMap == 0 ){` |
|       ! 0 | 1628 | `		return SXERR_MEM;` |
|         - | 1629 | `	}` |
|      3485 | 1630 | `	pVm->pGlobal = pMap;` |
|         - | 1631 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3485 | 1632 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3485 | 1633 | `	if( pObj == 0 ){` |
|       ! 0 | 1634 | `		return SXERR_MEM;` |
|         - | 1635 | `	}` |
|      3485 | 1636 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1637 | `	/* Record object index */` |
|      3485 | 1638 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1639 | `	/* Install the special $GLOBALS array */` |
|      3485 | 1640 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3485 | 1641 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1642 | `		return rc;` |
|         - | 1643 | `	}` |
|         - | 1644 | `	/* Install superglobals now */` |
|     38285 | 1645 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1646 | `		ph7_value *pSuper;` |
|         - | 1647 | `		/* Request an empty array */` |
|     34805 | 1648 | `		pSuper = ph7_new_array(&(*pVm));` |
|     34805 | 1649 | `		if( pSuper == 0 ){` |
|       ! 0 | 1650 | `			return SXERR_MEM;` |
|         - | 1651 | `		}` |
|         - | 1652 | `		/* Install */` |
|     34805 | 1653 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     34805 | 1654 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1655 | `			return rc;` |
|         - | 1656 | `		}` |
|         - | 1657 | `		/* Release the value now it have been installed */` |
|     34805 | 1658 | `		ph7_release_value(&(*pVm),pSuper);` |
|     17405 | 1659 | `	}` |
|         - | 1660 | `	/* Set some $_SERVER entries */` |
|      3485 | 1661 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1662 | `	/*` |
|         - | 1663 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1664 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1665 | `	 */` |
|      6961 | 1666 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1667 | `		"SCRIPT_FILENAME",` |
|      1740 | 1668 | `		pFile ? pFile->zString : ":Memory:",` |
|      3476 | 1669 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1670 | `		);` |
|         - | 1671 | `	/* All done,all super-global are installed now */` |
|      3485 | 1672 | `	return SXRET_OK;` |
|      1745 | 1673 | `}` |
|         - | 1674 | `/*` |
|         - | 1675 | ` * Release a hashmap.` |
|         - | 1676 | ` */` |
|     93776 | 1677 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1678 | `{` |
|         - | 1679 | `	ph7_hashmap_node *pEntry,*pNext;` |
|     93781 | 1680 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1681 | `	sxu32 n;` |
|     93781 | 1682 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1683 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1684 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1685 | `		return SXRET_OK;` |
|         - | 1686 | `	}` |
|         - | 1687 | `	/* Start the release process */` |
|     93781 | 1688 | `	n = 0;` |
|     93781 | 1689 | `	pEntry = pMap->pFirst;` |
|   1684159 | 1690 | `	for(;;){` |
|   3368323 | 1691 | `		if( n >= pMap->nEntry ){` |
|     93781 | 1692 | `			break;` |
|         - | 1693 | `		}` |
|   3274547 | 1694 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1695 | `		/* Remove the reference from the foreign table */` |
|   3274547 | 1696 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3274547 | 1697 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1698 | `			/* Restore the ph7_value to the free list */` |
|   3274537 | 1699 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1637266 | 1700 | `		}` |
|         - | 1701 | `		/* Release the node */` |
|   3274547 | 1702 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    164923 | 1703 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     82459 | 1704 | `		}` |
|   3274547 | 1705 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1706 | `		/* Point to the next entry */` |
|   3274547 | 1707 | `		pEntry = pNext;` |
|   3274547 | 1708 | `		n++;` |
|         5 | 1709 | `	}` |
|     93781 | 1710 | `	if( pMap->nEntry > 0 ){` |
|         - | 1711 | `		/* Release the hash bucket */` |
|     71497 | 1712 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     35746 | 1713 | `	}` |
|     93781 | 1714 | `	if( FreeDS ){` |
|         - | 1715 | `		/* Free the whole instance */` |
|     93765 | 1716 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     46885 | 1717 | `	}else{` |
|         - | 1718 | `		/* Keep the instance but reset it's fields */` |
|        17 | 1719 | `		pMap->apBucket = 0;` |
|        17 | 1720 | `		pMap->iNextIdx = 0;` |
|        17 | 1721 | `		pMap->nEntry = pMap->nSize = 0;` |
|        17 | 1722 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1723 | `	}` |
|     93781 | 1724 | `	return SXRET_OK;` |
|     46893 | 1725 | `}` |
|         - | 1726 | `/*` |
|         - | 1727 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1728 | ` * If the count reaches zero which mean no more variables` |
|         - | 1729 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1730 | ` */` |
|    803156 | 1731 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1732 | `{` |
|    803161 | 1733 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1734 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    803161 | 1735 | `	pMap->iRef--;` |
|    803161 | 1736 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|     93745 | 1737 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     46870 | 1738 | `	}` |
|    803161 | 1739 | `}` |
|         - | 1740 | `/*` |
|         - | 1741 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1742 | ` * Write a pointer to the target node on success.` |
|         - | 1743 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1744 | ` */` |
|    135446 | 1745 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1746 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1747 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1748 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1749 | `	)` |
|         5 | 1750 | `{` |
|         - | 1751 | `	sxi32 rc;` |
|    135451 | 1752 | `	if( pMap->nEntry < 1 ){` |
|         - | 1753 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1754 | `		 */` |
|       120 | 1755 | `		return SXERR_NOTFOUND;` |
|         - | 1756 | `	}` |
|    135335 | 1757 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    135335 | 1758 | `	return rc;` |
|     67728 | 1759 | `}` |
|         - | 1760 | `/*` |
|         - | 1761 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1762 | ` * hashmap.` |
|         - | 1763 | ` * If a node with the given key already exists in the database` |
|         - | 1764 | ` * then this function overwrite the old value.` |
|         - | 1765 | ` */` |
|   2670096 | 1766 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 1767 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1768 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1769 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 1770 | `	)` |
|         5 | 1771 | `{` |
|         - | 1772 | `	sxi32 rc;` |
|         - | 1773 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 1774 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 1775 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 1776 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|   2670101 | 1777 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2670101 | 1778 | `	return rc;` |
|         5 | 1779 | `}` |
|         - | 1780 | `/*` |
|         - | 1781 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 1782 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 1783 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 1784 | ` * This is the same routine that backs array_merge().` |
|         - | 1785 | ` */` |
|        52 | 1786 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1787 | `{` |
|        53 | 1788 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         1 | 1789 | `}` |
|         - | 1790 | `/*` |
|         - | 1791 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 1792 | ` * hashmap.` |
|         - | 1793 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 1794 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 1795 | ` * The insertion by reference is triggered when the following` |
|         - | 1796 | ` * expression is encountered.` |
|         - | 1797 | ` * $var = 10;` |
|         - | 1798 | ` *  $a = array(&var);` |
|         - | 1799 | ` * OR` |
|         - | 1800 | ` *  $a[] =& $var;` |
|         - | 1801 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 1802 | ` * over it's contents.` |
|         - | 1803 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 1804 | ` * removed when the foreign ph7_value is unset.` |
|         - | 1805 | ` * Example:` |
|         - | 1806 | ` *  $var = 10;` |
|         - | 1807 | ` *  $a[] =& $var;` |
|         - | 1808 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 1809 | ` *  //Unset the foreign ph7_value now` |
|         - | 1810 | ` *  unset($var);` |
|         - | 1811 | ` *  echo count($a); //0` |
|         - | 1812 | ` * Note that this is a PH7 eXtension.` |
|         - | 1813 | ` * Refer to the official documentation for more information.` |
|         - | 1814 | ` * If a node with the given key already exists in the database` |
|         - | 1815 | ` * then this function overwrite the old value.` |
|         - | 1816 | ` */` |
|     46498 | 1817 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1818 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1819 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1820 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1821 | `	)` |
|         5 | 1822 | `{` |
|         - | 1823 | `	sxi32 rc;` |
|     46503 | 1824 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1825 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1826 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1827 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1828 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1829 | `		return PH7_ABORT;` |
|         - | 1830 | `	}` |
|     46503 | 1831 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     46503 | 1832 | `	return rc;` |
|     23254 | 1833 | `}` |
|         - | 1834 | `/*` |
|         - | 1835 | ` * Reset the node cursor of a given hashmap.` |
|         - | 1836 | ` */` |
|     36800 | 1837 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|         5 | 1838 | `{` |
|         - | 1839 | `	/* Reset the loop cursor */` |
|     36805 | 1840 | `	pMap->pCur = pMap->pFirst;` |
|     36805 | 1841 | `}` |
|         - | 1842 | `/*` |
|         - | 1843 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 1844 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 1845 | ` * return NULL.` |
|         - | 1846 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 1847 | ` */` |
|    242846 | 1848 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         5 | 1849 | `{` |
|    242851 | 1850 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|    242851 | 1851 | `	if( pCur == 0 ){` |
|         - | 1852 | `		/* End of the list,return null */` |
|     18393 | 1853 | `		return 0;` |
|         - | 1854 | `	}` |
|         - | 1855 | `	/* Advance the node cursor */` |
|    224463 | 1856 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|    224463 | 1857 | `	return pCur;` |
|    121428 | 1858 | `}` |
|         - | 1859 | `/*` |
|         - | 1860 | ` * Extract a node value.` |
|         - | 1861 | ` */` |
|    567546 | 1862 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1863 | `{` |
|    567551 | 1864 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    567551 | 1865 | `	if( pEntry ){` |
|    567551 | 1866 | `		if( bStore ){` |
|    224875 | 1867 | `			PH7_MemObjStore(pEntry,pValue);` |
|    112440 | 1868 | `		}else{` |
|    342681 | 1869 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1870 | `		}` |
|    283862 | 1871 | `	}else{` |
|       ! 0 | 1872 | `		PH7_MemObjRelease(pValue);` |
|         - | 1873 | `	}` |
|    567551 | 1874 | `}` |
|         - | 1875 | `/*` |
|         - | 1876 | ` * Extract a node key.` |
|         - | 1877 | ` */` |
|    147510 | 1878 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 1879 | `{` |
|         - | 1880 | `	/* Fill with the current key */` |
|    147515 | 1881 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    142665 | 1882 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        31 | 1883 | `			SyBlobRelease(&pKey->sBlob);` |
|        15 | 1884 | `		}` |
|    142665 | 1885 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    142665 | 1886 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     71335 | 1887 | `	}else{` |
|      4855 | 1888 | `		SyBlobReset(&pKey->sBlob);` |
|      4855 | 1889 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      4855 | 1890 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 1891 | `	}` |
|    147515 | 1892 | `}` |
|         - | 1893 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 1894 | `/*` |
|         - | 1895 | ` * Store the address of nodes value in the given container.` |
|         - | 1896 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 1897 | ` * defined in 'builtin.c' for more information.` |
|         - | 1898 | ` */` |
|        12 | 1899 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         1 | 1900 | `{` |
|        13 | 1901 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 1902 | `	ph7_value *pValue;` |
|         - | 1903 | `	sxu32 n;` |
|         - | 1904 | `	/* Initialize the container */` |
|        13 | 1905 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 1906 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 1907 | `		/* Extract node value */` |
|        21 | 1908 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        21 | 1909 | `		if( pValue ){` |
|        21 | 1910 | `			SySetPut(pOut,(const void *)&pValue);` |
|        10 | 1911 | `		}` |
|         - | 1912 | `		/* Point to the next entry */` |
|        21 | 1913 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        11 | 1914 | `	}` |
|         - | 1915 | `	/* Total inserted entries */` |
|        13 | 1916 | `	return (int)SySetUsed(pOut);` |
|         1 | 1917 | `}` |
|         - | 1918 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 1919 | `/* SPDX-SnippetBegin */` |
|         - | 1920 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|         - | 1921 | `/* SPDX-License-Identifier: blessing */` |
|         - | 1922 | `/*` |
|         - | 1923 | ` * Merge sort.` |
|         - | 1924 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|         - | 1925 | ` * Status: Public domain` |
|         - | 1926 | ` */` |
|         - | 1927 | `/* Node comparison callback signature */` |
|         - | 1928 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|         - | 1929 | `/*` |
|         - | 1930 | `** Inputs:` |
|         - | 1931 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 1932 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 1933 | `**   cmp:     A pointer to the comparison function.` |
|         - | 1934 | `**` |
|         - | 1935 | `** Return Value:` |
|         - | 1936 | `**   A pointer to the head of a sorted list containing the elements` |
|         - | 1937 | `**   of both a and b.` |
|         - | 1938 | `**` |
|         - | 1939 | `** Side effects:` |
|         - | 1940 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|         - | 1941 | `**   changed.` |
|         - | 1942 | `*/` |
|     34780 | 1943 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 1944 | `{` |
|         - | 1945 | `	ph7_hashmap_node result,*pTail;` |
|         - | 1946 | `    /* Prevent compiler warning */` |
|     34785 | 1947 | `	result.pNext = result.pPrev = 0;` |
|     34785 | 1948 | `	pTail = &result;` |
|    105345 | 1949 | `	while( pA && pB ){` |
|     70565 | 1950 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     46758 | 1951 | `			pTail->pPrev = pA;` |
|     46758 | 1952 | `			pA->pNext = pTail;` |
|     46758 | 1953 | `			pTail = pA;` |
|     46758 | 1954 | `			pA = pA->pPrev;` |
|     23389 | 1955 | `		}else{` |
|     23812 | 1956 | `			pTail->pPrev = pB;` |
|     23812 | 1957 | `			pB->pNext = pTail;` |
|     23812 | 1958 | `			pTail = pB;` |
|     23812 | 1959 | `			pB = pB->pPrev;` |
|         - | 1960 | `		}` |
|         5 | 1961 | `	}` |
|     34785 | 1962 | `	if( pA ){` |
|     24475 | 1963 | `		pTail->pPrev = pA;` |
|     24475 | 1964 | `		pA->pNext = pTail;` |
|     22569 | 1965 | `	}else if( pB ){` |
|     10091 | 1966 | `		pTail->pPrev = pB;` |
|     10091 | 1967 | `		pB->pNext = pTail;` |
|      5029 | 1968 | `	}else{` |
|       229 | 1969 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 1970 | `	}` |
|     34785 | 1971 | `	return result.pPrev;` |
|         5 | 1972 | `}` |
|         - | 1973 | `/*` |
|         - | 1974 | `** Inputs:` |
|         - | 1975 | `**   Map:       Input hashmap` |
|         - | 1976 | `**   cmp:       A comparison function.` |
|         - | 1977 | `**` |
|         - | 1978 | `** Return Value:` |
|         - | 1979 | `**   Sorted hashmap.` |
|         - | 1980 | `**` |
|         - | 1981 | `** Side effects:` |
|         - | 1982 | `**   The "next" pointers for elements in list are changed.` |
|         - | 1983 | `*/` |
|         - | 1984 | `#define N_SORT_BUCKET  32` |
|       722 | 1985 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 1986 | `{` |
|         - | 1987 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 1988 | `	sxu32 i;` |
|       727 | 1989 | `	SyZero(a,sizeof(a));` |
|         - | 1990 | `	/* Point to the first inserted entry */` |
|       727 | 1991 | `	pIn = pMap->pFirst;` |
|     14401 | 1992 | `	while( pIn ){` |
|     13679 | 1993 | `		p = pIn;` |
|     13679 | 1994 | `		pIn = p->pPrev;` |
|     13679 | 1995 | `		p->pPrev = 0;` |
|     26077 | 1996 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26077 | 1997 | `			if( a[i]==0 ){` |
|     13679 | 1998 | `				a[i] = p;` |
|     13679 | 1999 | `				break;` |
|       ! 0 | 2000 | `			}else{` |
|     12403 | 2001 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12403 | 2002 | `				a[i] = 0;` |
|         - | 2003 | `			}` |
|      6204 | 2004 | `		}` |
|     13679 | 2005 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2006 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2007 | `			 * But that is impossible.` |
|         - | 2008 | `			 */` |
|       ! 0 | 2009 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2010 | `		}` |
|         5 | 2011 | `	}` |
|       727 | 2012 | `	p = a[0];` |
|     23109 | 2013 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     22387 | 2014 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     11196 | 2015 | `	}` |
|       727 | 2016 | `	p->pNext = 0;` |
|         - | 2017 | `	/* Reflect the change */` |
|       727 | 2018 | `	pMap->pFirst = p;` |
|         - | 2019 | `	/* Reset the loop cursor */` |
|       727 | 2020 | `	pMap->pCur = pMap->pFirst;` |
|       727 | 2021 | `	return SXRET_OK;` |
|         5 | 2022 | `}` |
|         - | 2023 | `/* SPDX-SnippetEnd */` |
|         - | 2024 | `/*` |
|         - | 2025 | ` * Node comparison callback.` |
|         - | 2026 | ` * used-by: [sort(),asort(),...]` |
|         - | 2027 | ` */` |
|     70340 | 2028 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2029 | `{` |
|         - | 2030 | `	ph7_value sA,sB;` |
|         - | 2031 | `	sxi32 iFlags;` |
|         - | 2032 | `	int rc;` |
|     70345 | 2033 | `	if( pCmpData == 0 ){` |
|         - | 2034 | `		/* Perform a standard comparison */` |
|     70321 | 2035 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     70321 | 2036 | `		return rc;` |
|         - | 2037 | `	}` |
|        25 | 2038 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2039 | `	/* Duplicate node values */` |
|        25 | 2040 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        25 | 2041 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        25 | 2042 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        25 | 2043 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        25 | 2044 | `	if( iFlags == 5 ){` |
|         - | 2045 | `		/* String cast */` |
|         - | 2046 | `		const char *zA,*zB;` |
|         - | 2047 | `		sxu32 nA,nB,nMin;` |
|        15 | 2048 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2049 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2050 | `		}` |
|        15 | 2051 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2052 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2053 | `		}` |
|         - | 2054 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        15 | 2055 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        15 | 2056 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        15 | 2057 | `		nA = SyBlobLength(&sA.sBlob);` |
|        15 | 2058 | `		nB = SyBlobLength(&sB.sBlob);` |
|        15 | 2059 | `		nMin = nA < nB ? nA : nB;` |
|        15 | 2060 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        15 | 2061 | `		if( rc == 0 ){` |
|         5 | 2062 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2063 | `			else if( nA > nB ) rc = 1;` |
|         2 | 2064 | `		}` |
|         8 | 2065 | `	}else{` |
|         - | 2066 | `		/* Numeric cast */` |
|        11 | 2067 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2068 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2069 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2070 | `	}` |
|        25 | 2071 | `	PH7_MemObjRelease(&sA);` |
|        25 | 2072 | `	PH7_MemObjRelease(&sB);` |
|        25 | 2073 | `	return rc;` |
|     35217 | 2074 | `}` |
|         - | 2075 | `/*` |
|         - | 2076 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2077 | ` * used-by: [ksort()]` |
|         - | 2078 | ` */` |
|        16 | 2079 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2080 | `{` |
|         - | 2081 | `	sxi32 rc;` |
|         8 | 2082 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        17 | 2083 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2084 | `		/* Perform a string comparison */` |
|         7 | 2085 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|         4 | 2086 | `	}else{` |
|         - | 2087 | `		SyString sStr;` |
|         - | 2088 | `		sxi64 iA,iB;` |
|         - | 2089 | `		/* Perform a numeric comparison */` |
|        11 | 2090 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2091 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2092 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|       ! 0 | 2093 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2094 | `				iA = 0;` |
|       ! 0 | 2095 | `			}else{` |
|       ! 0 | 2096 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2097 | `			}` |
|       ! 0 | 2098 | `		}else{` |
|        11 | 2099 | `			iA = pA->xKey.iKey;` |
|         - | 2100 | `		}` |
|        11 | 2101 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2102 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2103 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|       ! 0 | 2104 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2105 | `				iB = 0;` |
|       ! 0 | 2106 | `			}else{` |
|       ! 0 | 2107 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2108 | `			}` |
|       ! 0 | 2109 | `		}else{` |
|        11 | 2110 | `			iB = pB->xKey.iKey;` |
|         - | 2111 | `		}` |
|        11 | 2112 | `		rc = (sxi32)(iA-iB);` |
|         - | 2113 | `	}` |
|         - | 2114 | `	/* Comparison result */` |
|        17 | 2115 | `	return rc;` |
|         1 | 2116 | `}` |
|         - | 2117 | `/*` |
|         - | 2118 | ` * Node comparison callback.` |
|         - | 2119 | ` * Used by: [rsort(),arsort()];` |
|         - | 2120 | ` */` |
|        78 | 2121 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2122 | `{` |
|         - | 2123 | `	ph7_value sA,sB;` |
|         - | 2124 | `	sxi32 iFlags;` |
|         - | 2125 | `	int rc;` |
|        79 | 2126 | `	if( pCmpData == 0 ){` |
|         - | 2127 | `		/* Perform a standard comparison */` |
|        59 | 2128 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|        59 | 2129 | `		return -rc;` |
|         - | 2130 | `	}` |
|        21 | 2131 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2132 | `	/* Duplicate node values */` |
|        21 | 2133 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        21 | 2134 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        21 | 2135 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        21 | 2136 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        21 | 2137 | `	if( iFlags == 5 ){` |
|         - | 2138 | `		/* String cast */` |
|         - | 2139 | `		const char *zA,*zB;` |
|         - | 2140 | `		sxu32 nA,nB,nMin;` |
|        11 | 2141 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2142 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2143 | `		}` |
|        11 | 2144 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2145 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2146 | `		}` |
|         - | 2147 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        11 | 2148 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        11 | 2149 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        11 | 2150 | `		nA = SyBlobLength(&sA.sBlob);` |
|        11 | 2151 | `		nB = SyBlobLength(&sB.sBlob);` |
|        11 | 2152 | `		nMin = nA < nB ? nA : nB;` |
|        11 | 2153 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        11 | 2154 | `		if( rc == 0 ){` |
|         3 | 2155 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2156 | `			else if( nA > nB ) rc = 1;` |
|         1 | 2157 | `		}` |
|         6 | 2158 | `	}else{` |
|         - | 2159 | `		/* Numeric cast */` |
|        11 | 2160 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2161 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2162 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2163 | `	}` |
|        21 | 2164 | `	PH7_MemObjRelease(&sA);` |
|        21 | 2165 | `	PH7_MemObjRelease(&sB);` |
|        21 | 2166 | `	return -rc;` |
|        40 | 2167 | `}` |
|         - | 2168 | `/*` |
|         - | 2169 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2170 | ` * used-by: [usort(),uasort()]` |
|         - | 2171 | ` */` |
|        94 | 2172 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         3 | 2173 | `{` |
|         - | 2174 | `	ph7_value sResult,*pCallback;` |
|         - | 2175 | `	ph7_value *pV1,*pV2;` |
|         - | 2176 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2177 | `	sxi32 rc;` |
|         - | 2178 | `	/* Point to the desired callback */` |
|        97 | 2179 | `	pCallback = (ph7_value *)pCmpData;` |
|        97 | 2180 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2181 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2182 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2183 | `		return 0;` |
|         - | 2184 | `	}` |
|         - | 2185 | `	/* initialize the result value */` |
|        91 | 2186 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2187 | `	/* Extract nodes values */` |
|        91 | 2188 | `	pV1 = HashmapExtractNodeValue(pA);` |
|        91 | 2189 | `	pV2 = HashmapExtractNodeValue(pB);` |
|        91 | 2190 | `	apArg[0] = pV1;` |
|        91 | 2191 | `	apArg[1] = pV2;` |
|         - | 2192 | `	/* Invoke the callback */` |
|        91 | 2193 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|        91 | 2194 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2195 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2196 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2197 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2198 | `		rc = 0;` |
|        86 | 2199 | `	}else if( rc != SXRET_OK ){` |
|         - | 2200 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2201 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2202 | `	}else{` |
|         - | 2203 | `		/* Extract callback result */` |
|        82 | 2204 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2205 | `			/* Perform an int cast */` |
|       ! 0 | 2206 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2207 | `		}` |
|        82 | 2208 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2209 | `	}` |
|        91 | 2210 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2211 | `	/* Callback result */` |
|        91 | 2212 | `	return rc;` |
|        50 | 2213 | `}` |
|         - | 2214 | `/*` |
|         - | 2215 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2216 | ` * used-by: [krsort()]` |
|         - | 2217 | ` */` |
|         4 | 2218 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2219 | `{` |
|         - | 2220 | `	sxi32 rc;` |
|         2 | 2221 | `	SXUNUSED(pCmpData); /* cc warning */` |
|         5 | 2222 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2223 | `		/* Perform a string comparison */` |
|         5 | 2224 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|         3 | 2225 | `	}else{` |
|         - | 2226 | `		SyString sStr;` |
|         - | 2227 | `		sxi64 iA,iB;` |
|         - | 2228 | `		/* Perform a numeric comparison */` |
|       ! 0 | 2229 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2230 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2231 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|       ! 0 | 2232 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2233 | `				iA = 0;` |
|       ! 0 | 2234 | `			}else{` |
|       ! 0 | 2235 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2236 | `			}` |
|       ! 0 | 2237 | `		}else{` |
|       ! 0 | 2238 | `			iA = pA->xKey.iKey;` |
|         - | 2239 | `		}` |
|       ! 0 | 2240 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2241 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2242 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|       ! 0 | 2243 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2244 | `				iB = 0;` |
|       ! 0 | 2245 | `			}else{` |
|       ! 0 | 2246 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2247 | `			}` |
|       ! 0 | 2248 | `		}else{` |
|       ! 0 | 2249 | `			iB = pB->xKey.iKey;` |
|         - | 2250 | `		}` |
|       ! 0 | 2251 | `		rc = (sxi32)(iA-iB);` |
|         - | 2252 | `	}` |
|         5 | 2253 | `	return -rc; /* Reverse result */` |
|         1 | 2254 | `}` |
|         - | 2255 | `/*` |
|         - | 2256 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2257 | ` * used-by: [uksort()]` |
|         - | 2258 | ` */` |
|         6 | 2259 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2260 | `{` |
|         - | 2261 | `	ph7_value sResult,*pCallback;` |
|         - | 2262 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2263 | `	ph7_value sK1,sK2;` |
|         - | 2264 | `	sxi32 rc;` |
|         - | 2265 | `	/* Point to the desired callback */` |
|         7 | 2266 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2267 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2268 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2269 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2270 | `		return 0;` |
|         - | 2271 | `	}` |
|         - | 2272 | `	/* initialize the result value */` |
|         7 | 2273 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2274 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2275 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2276 | `	/* Extract nodes keys */` |
|         7 | 2277 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2278 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2279 | `	apArg[0] = &sK1;` |
|         7 | 2280 | `	apArg[1] = &sK2;` |
|         - | 2281 | `	/* Mark keys as constants */` |
|         7 | 2282 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2283 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2284 | `	/* Invoke the callback */` |
|         7 | 2285 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2286 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2287 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2288 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2289 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2290 | `		rc = 0;` |
|         7 | 2291 | `	}else if( rc != SXRET_OK ){` |
|         - | 2292 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2293 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2294 | `	}else{` |
|         - | 2295 | `		/* Extract callback result */` |
|         7 | 2296 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2297 | `			/* Perform an int cast */` |
|       ! 0 | 2298 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2299 | `		}` |
|         7 | 2300 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2301 | `	}` |
|         7 | 2302 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2303 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2304 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2305 | `	/* Callback result */` |
|         7 | 2306 | `	return rc;` |
|         4 | 2307 | `}` |
|         - | 2308 | `/*` |
|         - | 2309 | ` * Node comparison callback: Random node comparison.` |
|         - | 2310 | ` * used-by: [shuffle()]` |
|         - | 2311 | ` */` |
|        22 | 2312 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2313 | `{` |
|         - | 2314 | `	sxu32 n;` |
|         9 | 2315 | `	SXUNUSED(pB); /* cc warning */` |
|         9 | 2316 | `	SXUNUSED(pCmpData);` |
|         - | 2317 | `	/* Grab a random number */` |
|        23 | 2318 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2319 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2320 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2321 | `	 */` |
|        23 | 2322 | `	return n&1 ? 1 : -1;` |
|         1 | 2323 | `}` |
|         - | 2324 | `/*` |
|         - | 2325 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2326 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2327 | ` */` |
|       672 | 2328 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2329 | `{` |
|         - | 2330 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2331 | `	sxu32 i;` |
|         - | 2332 | `	/* Rehash all entries */` |
|       677 | 2333 | `	pLast = p = pMap->pFirst;` |
|       677 | 2334 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|       677 | 2335 | `	i = 0;` |
|      7086 | 2336 | `	for( ;; ){` |
|     14177 | 2337 | `		if( i >= pMap->nEntry ){` |
|       677 | 2338 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       677 | 2339 | `			break;` |
|         - | 2340 | `		}` |
|     13505 | 2341 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2342 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2343 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2344 | `			/* Change key type */` |
|         5 | 2345 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2346 | `		}` |
|     13505 | 2347 | `		HashmapRehashIntNode(p);` |
|         - | 2348 | `		/* Point to the next entry */` |
|     13505 | 2349 | `		i++;` |
|     13505 | 2350 | `		pLast = p;` |
|     13505 | 2351 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2352 | `	}` |
|       677 | 2353 | `}` |
|         - | 2354 | `/*` |
|         - | 2355 | ` * Array functions implementation.` |
|         - | 2356 | ` * Status:` |
|         - | 2357 | ` *  Stable.` |
|         - | 2358 | ` */` |
|         - | 2359 | `/*` |
|         - | 2360 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2361 | ` * Sort an array.` |
|         - | 2362 | ` * Parameters` |
|         - | 2363 | ` *  $array` |
|         - | 2364 | ` *   The input array.` |
|         - | 2365 | ` * $sort_flags` |
|         - | 2366 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2367 | ` *  Sorting type flags:` |
|         - | 2368 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2369 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2370 | ` *   SORT_STRING - compare items as strings` |
|         - | 2371 | ` * Return` |
|         - | 2372 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2373 | ` *` |
|         - | 2374 | ` */` |
|      1000 | 2375 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2376 | `{` |
|         - | 2377 | `	ph7_hashmap *pMap;` |
|         - | 2378 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1005 | 2379 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2380 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2381 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2382 | `		return PH7_OK;` |
|         - | 2383 | `	}` |
|         - | 2384 | `	/* Point to the internal representation of the input hashmap */` |
|      1005 | 2385 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1005 | 2386 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1005 | 2387 | `	if( pMap->nEntry > 1 ){` |
|       655 | 2388 | `		sxi32 iCmpFlags = 0;` |
|       655 | 2389 | `		if( nArg > 1 ){` |
|         - | 2390 | `			/* Extract comparison flags */` |
|         3 | 2391 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2392 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2393 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2394 | `			}` |
|         1 | 2395 | `		}` |
|         - | 2396 | `		/* Do the merge sort */` |
|       655 | 2397 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2398 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       655 | 2399 | `		HashmapSortRehash(pMap);` |
|       325 | 2400 | `	}` |
|         - | 2401 | `	/* All done,return TRUE */` |
|      1005 | 2402 | `	ph7_result_bool(pCtx,1);` |
|      1005 | 2403 | `	return PH7_OK;` |
|       505 | 2404 | `}` |
|         - | 2405 | `/*` |
|         - | 2406 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2407 | ` *  Sort an array and maintain index association.` |
|         - | 2408 | ` * Parameters` |
|         - | 2409 | ` *  $array` |
|         - | 2410 | ` *   The input array.` |
|         - | 2411 | ` * $sort_flags` |
|         - | 2412 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2413 | ` *  Sorting type flags:` |
|         - | 2414 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2415 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2416 | ` *   SORT_STRING - compare items as strings` |
|         - | 2417 | ` * Return` |
|         - | 2418 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2419 | ` */` |
|        32 | 2420 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2421 | `{` |
|         - | 2422 | `	ph7_hashmap *pMap;` |
|         - | 2423 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2424 | `	if( nArg < 1 ){` |
|         3 | 2425 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2426 | `			"ArgumentCountError",` |
|         - | 2427 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2428 | `			);` |
|         - | 2429 | `	}` |
|         - | 2430 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2431 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2432 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2433 | `			"TypeError",` |
|         - | 2434 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2435 | `			ph7_type_name(apArg[0])` |
|         - | 2436 | `			);` |
|         - | 2437 | `	}` |
|         - | 2438 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2439 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2440 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2441 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2442 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2443 | `		if( nArg > 1 ){` |
|         - | 2444 | `			/* Extract comparison flags */` |
|         5 | 2445 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2446 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2447 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2448 | `			}` |
|         2 | 2449 | `		}` |
|         - | 2450 | `		/* Do the merge sort */` |
|        19 | 2451 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2452 | `		/* Fix the last link broken by the merge */` |
|        45 | 2453 | `		while(pMap->pLast->pPrev){` |
|        27 | 2454 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2455 | `		}` |
|         9 | 2456 | `	}` |
|         - | 2457 | `	/* All done,return TRUE */` |
|        23 | 2458 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2459 | `	return PH7_OK;` |
|        21 | 2460 | `}` |
|         - | 2461 | `/*` |
|         - | 2462 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2463 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2464 | ` * Parameters` |
|         - | 2465 | ` *  $array` |
|         - | 2466 | ` *   The input array.` |
|         - | 2467 | ` * $sort_flags` |
|         - | 2468 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2469 | ` *  Sorting type flags:` |
|         - | 2470 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2471 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2472 | ` *   SORT_STRING - compare items as strings` |
|         - | 2473 | ` * Return` |
|         - | 2474 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2475 | ` */` |
|        32 | 2476 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2477 | `{` |
|         - | 2478 | `	ph7_hashmap *pMap;` |
|         - | 2479 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2480 | `	if( nArg < 1 ){` |
|         3 | 2481 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2482 | `			"ArgumentCountError",` |
|         - | 2483 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2484 | `			);` |
|         - | 2485 | `	}` |
|         - | 2486 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2487 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2488 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2489 | `			"TypeError",` |
|         - | 2490 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2491 | `			ph7_type_name(apArg[0])` |
|         - | 2492 | `			);` |
|         - | 2493 | `	}` |
|         - | 2494 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2495 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2496 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2497 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2498 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2499 | `		if( nArg > 1 ){` |
|         - | 2500 | `			/* Extract comparison flags */` |
|         5 | 2501 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2502 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2503 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2504 | `			}` |
|         2 | 2505 | `		}` |
|         - | 2506 | `		/* Do the merge sort */` |
|        19 | 2507 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2508 | `		/* Fix the last link broken by the merge */` |
|        35 | 2509 | `		while(pMap->pLast->pPrev){` |
|        17 | 2510 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2511 | `		}` |
|         9 | 2512 | `	}` |
|         - | 2513 | `	/* All done,return TRUE */` |
|        23 | 2514 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2515 | `	return PH7_OK;` |
|        21 | 2516 | `}` |
|         - | 2517 | `/*` |
|         - | 2518 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2519 | ` *  Sort an array by key.` |
|         - | 2520 | ` * Parameters` |
|         - | 2521 | ` *  $array` |
|         - | 2522 | ` *   The input array.` |
|         - | 2523 | ` * $sort_flags` |
|         - | 2524 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2525 | ` *  Sorting type flags:` |
|         - | 2526 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2527 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2528 | ` *   SORT_STRING - compare items as strings` |
|         - | 2529 | ` * Return` |
|         - | 2530 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2531 | ` */` |
|         6 | 2532 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2533 | `{` |
|         - | 2534 | `	ph7_hashmap *pMap;` |
|         - | 2535 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2536 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2537 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2538 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2539 | `		return PH7_OK;` |
|         - | 2540 | `	}` |
|         - | 2541 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2542 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2543 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2544 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2545 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2546 | `		if( nArg > 1 ){` |
|         - | 2547 | `			/* Extract comparison flags */` |
|       ! 0 | 2548 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2549 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2550 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2551 | `			}` |
|       ! 0 | 2552 | `		}` |
|         - | 2553 | `		/* Do the merge sort */` |
|         7 | 2554 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2555 | `		/* Fix the last link broken by the merge */` |
|        17 | 2556 | `		while(pMap->pLast->pPrev){` |
|        11 | 2557 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2558 | `		}` |
|         3 | 2559 | `	}` |
|         - | 2560 | `	/* All done,return TRUE */` |
|         7 | 2561 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2562 | `	return PH7_OK;` |
|         4 | 2563 | `}` |
|         - | 2564 | `/*` |
|         - | 2565 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2566 | ` *  Sort an array by key in reverse order.` |
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
|         2 | 2579 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2580 | `{` |
|         - | 2581 | `	ph7_hashmap *pMap;` |
|         - | 2582 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2583 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2584 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2585 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2586 | `		return PH7_OK;` |
|         - | 2587 | `	}` |
|         - | 2588 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2589 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2590 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2591 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2592 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2593 | `		if( nArg > 1 ){` |
|         - | 2594 | `			/* Extract comparison flags */` |
|       ! 0 | 2595 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2596 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2597 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2598 | `			}` |
|       ! 0 | 2599 | `		}` |
|         - | 2600 | `		/* Do the merge sort */` |
|         3 | 2601 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2602 | `		/* Fix the last link broken by the merge */` |
|         7 | 2603 | `		while(pMap->pLast->pPrev){` |
|         5 | 2604 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2605 | `		}` |
|         1 | 2606 | `	}` |
|         - | 2607 | `	/* All done,return TRUE */` |
|         3 | 2608 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2609 | `	return PH7_OK;` |
|         2 | 2610 | `}` |
|         - | 2611 | `/*` |
|         - | 2612 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2613 | ` * Sort an array in reverse order.` |
|         - | 2614 | ` * Parameters` |
|         - | 2615 | ` *  $array` |
|         - | 2616 | ` *   The input array.` |
|         - | 2617 | ` * $sort_flags` |
|         - | 2618 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2619 | ` *  Sorting type flags:` |
|         - | 2620 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2621 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2622 | ` *   SORT_STRING - compare items as strings` |
|         - | 2623 | ` * Return` |
|         - | 2624 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2625 | ` */` |
|         2 | 2626 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2627 | `{` |
|         - | 2628 | `	ph7_hashmap *pMap;` |
|         - | 2629 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2630 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2631 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2632 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2633 | `		return PH7_OK;` |
|         - | 2634 | `	}` |
|         - | 2635 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2636 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2637 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2638 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2639 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2640 | `		if( nArg > 1 ){` |
|         - | 2641 | `			/* Extract comparison flags */` |
|       ! 0 | 2642 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2643 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2644 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2645 | `			}` |
|       ! 0 | 2646 | `		}` |
|         - | 2647 | `		/* Do the merge sort */` |
|         3 | 2648 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2649 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         3 | 2650 | `		HashmapSortRehash(pMap);` |
|         1 | 2651 | `	}` |
|         - | 2652 | `	/* All done,return TRUE */` |
|         3 | 2653 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2654 | `	return PH7_OK;` |
|         2 | 2655 | `}` |
|         - | 2656 | `/*` |
|         - | 2657 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2658 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2659 | ` * Parameters` |
|         - | 2660 | ` *  $array` |
|         - | 2661 | ` *   The input array.` |
|         - | 2662 | ` * $cmp_function` |
|         - | 2663 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2664 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2665 | ` *  to, or greater than the second.` |
|         - | 2666 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2667 | ` * Return` |
|         - | 2668 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2669 | ` */` |
|        16 | 2670 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2671 | `{` |
|         - | 2672 | `	ph7_hashmap *pMap;` |
|         - | 2673 | `	/* Make sure we are dealing with a valid hashmap */` |
|        19 | 2674 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2675 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2676 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2677 | `		return PH7_OK;` |
|         - | 2678 | `	}` |
|         - | 2679 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 2680 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        19 | 2681 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        19 | 2682 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2683 | `		ph7_value *pCallback = 0;` |
|         - | 2684 | `		ProcNodeCmp xCmp;` |
|        19 | 2685 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        19 | 2686 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2687 | `			/* Point to the desired callback */` |
|        19 | 2688 | `			pCallback = apArg[1];` |
|        11 | 2689 | `		}else{` |
|         - | 2690 | `			/* Use the default comparison function */` |
|       ! 0 | 2691 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2692 | `		}` |
|         - | 2693 | `		/* Do the merge sort */` |
|        19 | 2694 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        19 | 2695 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2696 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        19 | 2697 | `		HashmapSortRehash(pMap);` |
|        19 | 2698 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2699 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2700 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2701 | `			return PH7_EXCEPTION;` |
|         - | 2702 | `		}` |
|         4 | 2703 | `	}` |
|         - | 2704 | `	/* All done,return TRUE */` |
|        10 | 2705 | `	ph7_result_bool(pCtx,1);` |
|        10 | 2706 | `	return PH7_OK;` |
|        11 | 2707 | `}` |
|         - | 2708 | `/*` |
|         - | 2709 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2710 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2711 | ` *  and maintain index association.` |
|         - | 2712 | ` * Parameters` |
|         - | 2713 | ` *  $array` |
|         - | 2714 | ` *   The input array.` |
|         - | 2715 | ` * $cmp_function` |
|         - | 2716 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2717 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2718 | ` *  to, or greater than the second.` |
|         - | 2719 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2720 | ` * Return` |
|         - | 2721 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2722 | ` */` |
|         2 | 2723 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2724 | `{` |
|         - | 2725 | `	ph7_hashmap *pMap;` |
|         - | 2726 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2727 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2728 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2729 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2730 | `		return PH7_OK;` |
|         - | 2731 | `	}` |
|         - | 2732 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2733 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2734 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2735 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2736 | `		ph7_value *pCallback = 0;` |
|         - | 2737 | `		ProcNodeCmp xCmp;` |
|         3 | 2738 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|         3 | 2739 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2740 | `			/* Point to the desired callback */` |
|         3 | 2741 | `			pCallback = apArg[1];` |
|         2 | 2742 | `		}else{` |
|         - | 2743 | `			/* Use the default comparison function */` |
|       ! 0 | 2744 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2745 | `		}` |
|         - | 2746 | `		/* Do the merge sort */` |
|         3 | 2747 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2748 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2749 | `		/* Fix the last link broken by the merge */` |
|         5 | 2750 | `		while(pMap->pLast->pPrev){` |
|         3 | 2751 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2752 | `		}` |
|         3 | 2753 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2754 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2755 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2756 | `			return PH7_EXCEPTION;` |
|         - | 2757 | `		}` |
|         1 | 2758 | `	}` |
|         - | 2759 | `	/* All done,return TRUE */` |
|         3 | 2760 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2761 | `	return PH7_OK;` |
|         2 | 2762 | `}` |
|         - | 2763 | `/*` |
|         - | 2764 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2765 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2766 | ` *  function and maintain index association.` |
|         - | 2767 | ` * Parameters` |
|         - | 2768 | ` *  $array` |
|         - | 2769 | ` *   The input array.` |
|         - | 2770 | ` * $cmp_function` |
|         - | 2771 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2772 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2773 | ` *  to, or greater than the second.` |
|         - | 2774 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2775 | ` * Return` |
|         - | 2776 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2777 | ` */` |
|         2 | 2778 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2779 | `{` |
|         - | 2780 | `	ph7_hashmap *pMap;` |
|         - | 2781 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2782 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2783 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2784 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2785 | `		return PH7_OK;` |
|         - | 2786 | `	}` |
|         - | 2787 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2788 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2789 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2790 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2791 | `		ph7_value *pCallback = 0;` |
|         - | 2792 | `		ProcNodeCmp xCmp;` |
|         3 | 2793 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2794 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2795 | `			/* Point to the desired callback */` |
|         3 | 2796 | `			pCallback = apArg[1];` |
|         2 | 2797 | `		}else{` |
|         - | 2798 | `			/* Use the default comparison function */` |
|       ! 0 | 2799 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2800 | `		}` |
|         - | 2801 | `		/* Do the merge sort */` |
|         3 | 2802 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2803 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2804 | `		/* Fix the last link broken by the merge */` |
|         3 | 2805 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2806 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2807 | `		}` |
|         3 | 2808 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2809 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2810 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2811 | `			return PH7_EXCEPTION;` |
|         - | 2812 | `		}` |
|         1 | 2813 | `	}` |
|         - | 2814 | `	/* All done,return TRUE */` |
|         3 | 2815 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2816 | `	return PH7_OK;` |
|         2 | 2817 | `}` |
|         - | 2818 | `/*` |
|         - | 2819 | ` * bool shuffle(array &$array)` |
|         - | 2820 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2821 | ` * Parameters` |
|         - | 2822 | ` *  $array` |
|         - | 2823 | ` *   The input array.` |
|         - | 2824 | ` * Return` |
|         - | 2825 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2826 | ` *` |
|         - | 2827 | ` */` |
|         2 | 2828 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2829 | `{` |
|         - | 2830 | `	ph7_hashmap *pMap;` |
|         - | 2831 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2832 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2833 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2834 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2835 | `		return PH7_OK;` |
|         - | 2836 | `	}` |
|         - | 2837 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2838 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2839 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2840 | `	if( pMap->nEntry > 1 ){` |
|         - | 2841 | `		/* Do the merge sort */` |
|         3 | 2842 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 2843 | `		/* Fix the last link broken by the merge */` |
|         8 | 2844 | `		while(pMap->pLast->pPrev){` |
|         6 | 2845 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2846 | `		}` |
|         1 | 2847 | `	}` |
|         - | 2848 | `	/* All done,return TRUE */` |
|         3 | 2849 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2850 | `	return PH7_OK;` |
|         2 | 2851 | `}` |
|         - | 2852 | `/*` |
|         - | 2853 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 2854 | ` *   Count all elements in an array, or something in an object.` |
|         - | 2855 | ` * Parameters` |
|         - | 2856 | ` *  $var` |
|         - | 2857 | ` *   The array or the object.` |
|         - | 2858 | ` * $mode` |
|         - | 2859 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 2860 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 2861 | ` *  all the elements of a multidimensional array.` |
|         - | 2862 | ` * Return` |
|         - | 2863 | ` *  Returns the number of elements in the array.` |
|         - | 2864 | ` */` |
|      1174 | 2865 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2866 | `{` |
|      1179 | 2867 | `	int bRecursive = FALSE;` |
|      1179 | 2868 | `	int bCycleDetected = FALSE;` |
|         - | 2869 | `	sxi64 iCount;` |
|      1179 | 2870 | `	if( nArg < 1 ){` |
|         3 | 2871 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2872 | `			"ArgumentCountError",` |
|         - | 2873 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2874 | `			);` |
|         - | 2875 | `	}` |
|      1177 | 2876 | `	if( nArg > 2 ){` |
|         4 | 2877 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2878 | `			"ArgumentCountError",` |
|         - | 2879 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 2880 | `			nArg` |
|         - | 2881 | `			);` |
|         - | 2882 | `	}` |
|         - | 2883 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 2884 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 2885 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1175 | 2886 | `	if( nArg > 1 ){` |
|        45 | 2887 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        45 | 2888 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        12 | 2889 | `			return PH7_VmThrowException(pCtx,` |
|         - | 2890 | `				"ValueError",` |
|         - | 2891 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 2892 | `				);` |
|         - | 2893 | `		}` |
|        34 | 2894 | `		bRecursive = iMode == 1;` |
|        16 | 2895 | `	}` |
|      1167 | 2896 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 2897 | `		/* Countable object: dispatch to ->count() */` |
|        37 | 2898 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        26 | 2899 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        26 | 2900 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        26 | 2901 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        23 | 2902 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 2903 | `					"count",sizeof("count")-1);` |
|        23 | 2904 | `				if( pMeth ){` |
|         - | 2905 | `					ph7_value sResult;` |
|        23 | 2906 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        23 | 2907 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        23 | 2908 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        23 | 2909 | `					PH7_MemObjRelease(&sResult);` |
|        23 | 2910 | `					return PH7_OK;` |
|         - | 2911 | `				}` |
|       ! 0 | 2912 | `			}` |
|         1 | 2913 | `		}` |
|        22 | 2914 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2915 | `			"TypeError",` |
|         - | 2916 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 2917 | `			ph7_type_name(apArg[0])` |
|         - | 2918 | `			);` |
|         - | 2919 | `	}` |
|         - | 2920 | `	/* Count */` |
|      1135 | 2921 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1135 | 2922 | `	if( bCycleDetected ){` |
|         3 | 2923 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 2924 | `	}` |
|      1135 | 2925 | `	ph7_result_int64(pCtx,iCount);` |
|      1135 | 2926 | `	return PH7_OK;` |
|       592 | 2927 | `}` |
|         - | 2928 | `/*` |
|         - | 2929 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 2930 | ` *  Checks if the given key or index exists in the array.` |
|         - | 2931 | ` * Parameters` |
|         - | 2932 | ` * $key` |
|         - | 2933 | ` *   Value to check.` |
|         - | 2934 | ` * $search` |
|         - | 2935 | ` *  An array with keys to check.` |
|         - | 2936 | ` * Return` |
|         - | 2937 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2938 | ` */` |
|        86 | 2939 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2940 | `{` |
|         - | 2941 | `	sxi32 rc;` |
|        91 | 2942 | `	if( nArg != 2 ){` |
|         - | 2943 | `		/* PHP requires exactly two arguments */` |
|        12 | 2944 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2945 | `			"ArgumentCountError",` |
|         - | 2946 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         3 | 2947 | `			nArg` |
|         - | 2948 | `			);` |
|         - | 2949 | `	}` |
|         - | 2950 | `	/* Make sure we are dealing with a valid hashmap */` |
|        85 | 2951 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 2952 | `		/* Type mismatch -> TypeError */` |
|         8 | 2953 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2954 | `			"TypeError",` |
|         - | 2955 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 2956 | `			ph7_type_name(apArg[1])` |
|         - | 2957 | `			);` |
|         - | 2958 | `	}` |
|         - | 2959 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        80 | 2960 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         3 | 2961 | `		ph7_context_throw_error_format(pCtx,8192,` |
|         - | 2962 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 2963 | `			"use an empty string instead"` |
|         - | 2964 | `			);` |
|        79 | 2965 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 2966 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 2967 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 2968 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 2969 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 2970 | `				,rVal` |
|         - | 2971 | `				);` |
|         1 | 2972 | `		}` |
|         1 | 2973 | `	}` |
|         - | 2974 | `	/* Perform the lookup */` |
|        80 | 2975 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 2976 | `	/* lookup result */` |
|        80 | 2977 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        80 | 2978 | `	return PH7_OK;` |
|        48 | 2979 | `}` |
|         - | 2980 | `/*` |
|         - | 2981 | ` * value array_pop(array $array)` |
|         - | 2982 | ` *   POP the last inserted element from the array.` |
|         - | 2983 | ` * Parameter` |
|         - | 2984 | ` *  The array to get the value from.` |
|         - | 2985 | ` * Return` |
|         - | 2986 | ` *  Poped value or NULL on failure.` |
|         - | 2987 | ` */` |
|        18 | 2988 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2989 | `{` |
|         - | 2990 | `	ph7_hashmap *pMap;` |
|         - | 2991 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        23 | 2992 | `	if( nArg != 1 ){` |
|         8 | 2993 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2994 | `			"ArgumentCountError",` |
|         - | 2995 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         2 | 2996 | `			nArg` |
|         - | 2997 | `			);` |
|         - | 2998 | `	}` |
|         - | 2999 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3000 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        18 | 3001 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3002 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3003 | `			"Error",` |
|         - | 3004 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3005 | `			);` |
|         - | 3006 | `	}` |
|         - | 3007 | `	/* Make sure we are dealing with a valid hashmap */` |
|        12 | 3008 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3009 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3010 | `			"TypeError",` |
|         - | 3011 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3012 | `			ph7_type_name(apArg[0])` |
|         - | 3013 | `			);` |
|         - | 3014 | `	}` |
|         9 | 3015 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         9 | 3016 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         9 | 3017 | `	if( pMap->nEntry < 1 ){` |
|         - | 3018 | `		/* Nothing to pop,return NULL */` |
|         3 | 3019 | `		ph7_result_null(pCtx);` |
|         2 | 3020 | `	}else{` |
|         7 | 3021 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3022 | `		ph7_value *pObj;` |
|         7 | 3023 | `		pObj = HashmapExtractNodeValue(pLast);` |
|         7 | 3024 | `		if( pObj ){` |
|         - | 3025 | `			/* Node value */` |
|         7 | 3026 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3027 | `			/* Unlink the node */` |
|         7 | 3028 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|         4 | 3029 | `		}else{` |
|       ! 0 | 3030 | `			ph7_result_null(pCtx);` |
|         - | 3031 | `		}` |
|         - | 3032 | `		/* Reset the cursor */` |
|         7 | 3033 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3034 | `	}` |
|         9 | 3035 | `	return PH7_OK;` |
|        14 | 3036 | `}` |
|         - | 3037 | `/*` |
|         - | 3038 | ` * int array_push($array,$var,...)` |
|         - | 3039 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3040 | ` * Parameters` |
|         - | 3041 | ` *  array` |
|         - | 3042 | ` *    The input array.` |
|         - | 3043 | ` *  var` |
|         - | 3044 | ` *   On or more value to push.` |
|         - | 3045 | ` * Return` |
|         - | 3046 | ` *  New array count (including old items).` |
|         - | 3047 | ` */` |
|        24 | 3048 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3049 | `{` |
|         - | 3050 | `	ph7_hashmap *pMap;` |
|         - | 3051 | `	sxi32 rc;` |
|         - | 3052 | `	int i;` |
|        29 | 3053 | `	if( nArg < 1 ){` |
|         4 | 3054 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3055 | `			"ArgumentCountError",` |
|         - | 3056 | `			"array_push() expects at least 1 argument, %d given",` |
|         1 | 3057 | `			nArg` |
|         - | 3058 | `			);` |
|         - | 3059 | `	}` |
|         - | 3060 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3061 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        26 | 3062 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3063 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3064 | `			"Error",` |
|         - | 3065 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3066 | `			);` |
|         - | 3067 | `	}` |
|         - | 3068 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 3069 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3070 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3071 | `			"TypeError",` |
|         - | 3072 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3073 | `			ph7_type_name(apArg[0])` |
|         - | 3074 | `			);` |
|         - | 3075 | `	}` |
|         - | 3076 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3077 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3078 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3079 | `	/* Start pushing given values */` |
|        34 | 3080 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3081 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3082 | `		if( rc != SXRET_OK ){` |
|         3 | 3083 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3084 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3085 | `				return rc;` |
|         - | 3086 | `			}` |
|       ! 0 | 3087 | `			break;` |
|         - | 3088 | `		}` |
|         9 | 3089 | `	}` |
|         - | 3090 | `	/* Return the new count */` |
|        15 | 3091 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3092 | `	return PH7_OK;` |
|        17 | 3093 | `}` |
|         - | 3094 | `/*` |
|         - | 3095 | ` * value array_shift(array $array)` |
|         - | 3096 | ` *   Shift an element off the beginning of array.` |
|         - | 3097 | ` * Parameter` |
|         - | 3098 | ` *  The array to get the value from.` |
|         - | 3099 | ` * Return` |
|         - | 3100 | ` *  Shifted value or NULL on failure.` |
|         - | 3101 | ` */` |
|        38 | 3102 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3103 | `{` |
|         - | 3104 | `	ph7_hashmap *pMap;` |
|         - | 3105 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        43 | 3106 | `	if( nArg != 1 ){` |
|         8 | 3107 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3108 | `			"ArgumentCountError",` |
|         - | 3109 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         2 | 3110 | `			nArg` |
|         - | 3111 | `			);` |
|         - | 3112 | `	}` |
|         - | 3113 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        39 | 3114 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3115 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3116 | `			"Error",` |
|         - | 3117 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3118 | `			);` |
|         - | 3119 | `	}` |
|         - | 3120 | `	/* Make sure we are dealing with a valid hashmap */` |
|        35 | 3121 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3122 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3123 | `			"TypeError",` |
|         - | 3124 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3125 | `			ph7_type_name(apArg[0])` |
|         - | 3126 | `			);` |
|         - | 3127 | `	}` |
|         - | 3128 | `	/* Point to the internal representation of the hashmap */` |
|        33 | 3129 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        33 | 3130 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        33 | 3131 | `	if( pMap->nEntry < 1 ){` |
|         - | 3132 | `		/* Empty hashmap,return NULL */` |
|         3 | 3133 | `		ph7_result_null(pCtx);` |
|         2 | 3134 | `	}else{` |
|        31 | 3135 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3136 | `		ph7_value *pObj;` |
|         - | 3137 | `		sxu32 n;` |
|        31 | 3138 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        31 | 3139 | `		if( pObj ){` |
|         - | 3140 | `			/* Node value */` |
|        31 | 3141 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3142 | `			/* Unlink the first node */` |
|        31 | 3143 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        18 | 3144 | `		}else{` |
|       ! 0 | 3145 | `			ph7_result_null(pCtx);` |
|         - | 3146 | `		}` |
|         - | 3147 | `		/* Rehash all int keys */` |
|        31 | 3148 | `		n = pMap->nEntry;` |
|        31 | 3149 | `		pEntry = pMap->pFirst;` |
|        31 | 3150 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|        40 | 3151 | `		for(;;){` |
|        85 | 3152 | `			if( n < 1 ){` |
|        31 | 3153 | `				break;` |
|         - | 3154 | `			}` |
|        59 | 3155 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        59 | 3156 | `				HashmapRehashIntNode(pEntry);` |
|        27 | 3157 | `			}` |
|         - | 3158 | `			/* Point to the next entry */` |
|        59 | 3159 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        59 | 3160 | `			n--;` |
|         5 | 3161 | `		}` |
|         - | 3162 | `		/* Reset the cursor */` |
|        31 | 3163 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3164 | `	}` |
|        33 | 3165 | `	return PH7_OK;` |
|        24 | 3166 | `}` |
|         - | 3167 | `/*` |
|         - | 3168 | ` * Extract the node cursor value.` |
|         - | 3169 | ` */` |
|        32 | 3170 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3171 | `{` |
|        33 | 3172 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3173 | `	ph7_value *pVal;` |
|        33 | 3174 | `	if( pCur == 0 ){` |
|         - | 3175 | `		/* Cursor does not point to anything,return FALSE */` |
|       ! 0 | 3176 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3177 | `		return PH7_OK;` |
|         - | 3178 | `	}` |
|        33 | 3179 | `	if( iDirection != 0 ){` |
|        13 | 3180 | `		if( iDirection > 0 ){` |
|         - | 3181 | `			/* Point to the next entry */` |
|        11 | 3182 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        11 | 3183 | `			pCur = pMap->pCur;` |
|         6 | 3184 | `		}else{` |
|         - | 3185 | `			/* Point to the previous entry */` |
|         3 | 3186 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3187 | `			pCur = pMap->pCur;` |
|         - | 3188 | `		}` |
|        13 | 3189 | `		if( pCur == 0 ){` |
|         - | 3190 | `			/* End of input reached,return FALSE */` |
|       ! 0 | 3191 | `			ph7_result_bool(pCtx,0);` |
|       ! 0 | 3192 | `			return PH7_OK;` |
|         - | 3193 | `		}` |
|         6 | 3194 | `	}` |
|         - | 3195 | `	/* Point to the desired element */` |
|        33 | 3196 | `	pVal = HashmapExtractNodeValue(pCur);` |
|        33 | 3197 | `	if( pVal ){` |
|        33 | 3198 | `		ph7_result_value(pCtx,pVal);` |
|        17 | 3199 | `	}else{` |
|       ! 0 | 3200 | `		ph7_result_bool(pCtx,0);` |
|         - | 3201 | `	}` |
|        33 | 3202 | `	return PH7_OK;` |
|        17 | 3203 | `}` |
|         - | 3204 | `/*` |
|         - | 3205 | ` * value current(array $array)` |
|         - | 3206 | ` *  Return the current element in an array.` |
|         - | 3207 | ` * Parameter` |
|         - | 3208 | ` *  $input: The input array.` |
|         - | 3209 | ` * Return` |
|         - | 3210 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3211 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3212 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3213 | ` *  is empty, current() returns FALSE.` |
|         - | 3214 | ` */` |
|        14 | 3215 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3216 | `{` |
|        15 | 3217 | `	if( nArg < 1 ){` |
|         - | 3218 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3219 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3220 | `		return PH7_OK;` |
|         - | 3221 | `	}` |
|         - | 3222 | `	/* Make sure we are dealing with a valid hashmap */` |
|        15 | 3223 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3224 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3225 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3226 | `		return PH7_OK;` |
|         - | 3227 | `	}` |
|        15 | 3228 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|        15 | 3229 | `	return PH7_OK;` |
|         8 | 3230 | `}` |
|         - | 3231 | `/*` |
|         - | 3232 | ` * value next(array $input)` |
|         - | 3233 | ` *  Advance the internal array pointer of an array.` |
|         - | 3234 | ` * Parameter` |
|         - | 3235 | ` *  $input: The input array.` |
|         - | 3236 | ` * Return` |
|         - | 3237 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3238 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3239 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3240 | ` */` |
|        10 | 3241 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3242 | `{` |
|        11 | 3243 | `	if( nArg < 1 ){` |
|         - | 3244 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3245 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3246 | `		return PH7_OK;` |
|         - | 3247 | `	}` |
|         - | 3248 | `	/* Make sure we are dealing with a valid hashmap */` |
|        11 | 3249 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3250 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3251 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3252 | `		return PH7_OK;` |
|         - | 3253 | `	}` |
|        11 | 3254 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|        11 | 3255 | `	return PH7_OK;` |
|         6 | 3256 | `}` |
|         - | 3257 | `/*` |
|         - | 3258 | ` * value prev(array $input)` |
|         - | 3259 | ` *  Rewind the internal array pointer.` |
|         - | 3260 | ` * Parameter` |
|         - | 3261 | ` *  $input: The input array.` |
|         - | 3262 | ` * Return` |
|         - | 3263 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3264 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3265 | ` *  elements.` |
|         - | 3266 | ` */` |
|         2 | 3267 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3268 | `{` |
|         3 | 3269 | `	if( nArg < 1 ){` |
|         - | 3270 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3271 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3272 | `		return PH7_OK;` |
|         - | 3273 | `	}` |
|         - | 3274 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3275 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3276 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3277 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3278 | `		return PH7_OK;` |
|         - | 3279 | `	}` |
|         3 | 3280 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3281 | `	return PH7_OK;` |
|         2 | 3282 | `}` |
|         - | 3283 | `/*` |
|         - | 3284 | ` * value end(array $input)` |
|         - | 3285 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3286 | ` * Parameter` |
|         - | 3287 | ` *  $input: The input array.` |
|         - | 3288 | ` * Return` |
|         - | 3289 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3290 | ` */` |
|         2 | 3291 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3292 | `{` |
|         - | 3293 | `	ph7_hashmap *pMap;` |
|         3 | 3294 | `	if( nArg < 1 ){` |
|         - | 3295 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3296 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3297 | `		return PH7_OK;` |
|         - | 3298 | `	}` |
|         - | 3299 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3300 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3301 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3302 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3303 | `		return PH7_OK;` |
|         - | 3304 | `	}` |
|         - | 3305 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3306 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3307 | `	/* Point to the last node */` |
|         3 | 3308 | `	pMap->pCur = pMap->pLast;` |
|         - | 3309 | `	/* Return the last node value */` |
|         3 | 3310 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|         3 | 3311 | `	return PH7_OK;` |
|         2 | 3312 | `}` |
|         - | 3313 | `/*` |
|         - | 3314 | ` * value reset(array $array )` |
|         - | 3315 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3316 | ` * Parameter` |
|         - | 3317 | ` *  $input: The input array.` |
|         - | 3318 | ` * Return` |
|         - | 3319 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3320 | ` */` |
|         4 | 3321 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3322 | `{` |
|         - | 3323 | `	ph7_hashmap *pMap;` |
|         5 | 3324 | `	if( nArg < 1 ){` |
|         - | 3325 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3326 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3327 | `		return PH7_OK;` |
|         - | 3328 | `	}` |
|         - | 3329 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3330 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3331 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3332 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3333 | `		return PH7_OK;` |
|         - | 3334 | `	}` |
|         - | 3335 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 3336 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3337 | `	/* Point to the first node */` |
|         5 | 3338 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3339 | `	/* Return the last node value if available */` |
|         5 | 3340 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|         5 | 3341 | `	return PH7_OK;` |
|         3 | 3342 | `}` |
|         - | 3343 | `/*` |
|         - | 3344 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3345 | ` * array_key_first() and array_key_last().` |
|         - | 3346 | ` */` |
|        20 | 3347 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3348 | `{` |
|        21 | 3349 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3350 | `		/* Key is integer */` |
|        15 | 3351 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         8 | 3352 | `	}else{` |
|         - | 3353 | `		/* Key is blob */` |
|        10 | 3354 | `		ph7_result_string(pCtx,` |
|         6 | 3355 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3356 | `	}` |
|        21 | 3357 | `}` |
|         - | 3358 | `/*` |
|         - | 3359 | ` * value key(array $array)` |
|         - | 3360 | ` *   Fetch a key from an array` |
|         - | 3361 | ` * Parameter` |
|         - | 3362 | ` *  $input` |
|         - | 3363 | ` *   The input array.` |
|         - | 3364 | ` * Return` |
|         - | 3365 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3366 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3367 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3368 | ` *  is empty, key() returns NULL.` |
|         - | 3369 | ` */` |
|         4 | 3370 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3371 | `{` |
|         - | 3372 | `	ph7_hashmap_node *pCur;` |
|         - | 3373 | `	ph7_hashmap *pMap;` |
|         5 | 3374 | `	if( nArg < 1 ){` |
|         - | 3375 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3376 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3377 | `		return PH7_OK;` |
|         - | 3378 | `	}` |
|         - | 3379 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3380 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3381 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3382 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3383 | `		return PH7_OK;` |
|         - | 3384 | `	}` |
|         5 | 3385 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 3386 | `	pCur = pMap->pCur;` |
|         5 | 3387 | `	if( pCur == 0 ){` |
|         - | 3388 | `		/* Cursor does not point to anything,return NULL */` |
|       ! 0 | 3389 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3390 | `		return PH7_OK;` |
|         - | 3391 | `	}` |
|         5 | 3392 | `	HashmapResultNodeKey(pCtx,pCur);` |
|         5 | 3393 | `	return PH7_OK;` |
|         3 | 3394 | `}` |
|         - | 3395 | `/*` |
|         - | 3396 | ` * array each(array $input)` |
|         - | 3397 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3398 | ` * Parameter` |
|         - | 3399 | ` *  $input` |
|         - | 3400 | ` *    The input array.` |
|         - | 3401 | ` * Return` |
|         - | 3402 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3403 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3404 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3405 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3406 | ` *  each() returns FALSE.` |
|         - | 3407 | ` */` |
|        22 | 3408 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3409 | `{` |
|         - | 3410 | `	ph7_hashmap_node *pCur;` |
|         - | 3411 | `	ph7_hashmap *pMap;` |
|         - | 3412 | `	ph7_value *pArray;` |
|         - | 3413 | `	ph7_value *pVal;` |
|         - | 3414 | `	ph7_value sKey;` |
|        23 | 3415 | `	if( nArg < 1 ){` |
|         - | 3416 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3417 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3418 | `		return PH7_OK;` |
|         - | 3419 | `	}` |
|         - | 3420 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3421 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3422 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3423 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3424 | `		return PH7_OK;` |
|         - | 3425 | `	}` |
|         - | 3426 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3427 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3428 | `	if( pMap->pCur == 0 ){` |
|         - | 3429 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3430 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3431 | `		return PH7_OK;` |
|         - | 3432 | `	}` |
|        15 | 3433 | `	pCur = pMap->pCur;` |
|         - | 3434 | `	/* Create a new array */` |
|        15 | 3435 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3436 | `	if( pArray == 0 ){` |
|       ! 0 | 3437 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3438 | `		return PH7_OK;` |
|         - | 3439 | `	}` |
|        15 | 3440 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3441 | `	/* Insert the current value */` |
|        15 | 3442 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3443 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3444 | `	/* Make the key */` |
|        15 | 3445 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3446 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3447 | `	}else{` |
|         9 | 3448 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3449 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3450 | `	}` |
|         - | 3451 | `	/* Insert the current key */` |
|        15 | 3452 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3453 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3454 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3455 | `	/* Advance the cursor */` |
|        15 | 3456 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3457 | `	/* Return the current entry */` |
|        15 | 3458 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3459 | `	return PH7_OK;` |
|        12 | 3460 | `}` |
|         - | 3461 | `/*` |
|         - | 3462 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3463 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3464 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3465 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3466 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3467 | ` */` |
|         - | 3468 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3469 | `/*` |
|         - | 3470 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3471 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3472 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3473 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3474 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3475 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3476 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3477 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3478 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3479 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3480 | ` */` |
|         - | 3481 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3482 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3483 | `/*` |
|         - | 3484 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3485 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3486 | ` */` |
|         8 | 3487 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|         1 | 3488 | `{` |
|         9 | 3489 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|         3 | 3490 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|         3 | 3491 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|         3 | 3492 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|         3 | 3493 | `		zBuf[n] = 0;` |
|         3 | 3494 | `		return zBuf;` |
|         - | 3495 | `	}` |
|         7 | 3496 | `	return ph7_type_name(pVal);` |
|         5 | 3497 | `}` |
|         - | 3498 | `/*` |
|         - | 3499 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3500 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3501 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3502 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3503 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3504 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3505 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3506 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3507 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3508 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3509 | ` */` |
|       156 | 3510 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3511 | `{` |
|       157 | 3512 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3513 | `	sxu64 uVal = 0;` |
|       157 | 3514 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3515 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3516 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3517 | `		bNeg = (z[0] == '-');` |
|         3 | 3518 | `		z++;` |
|         1 | 3519 | `	}` |
|       237 | 3520 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3521 | `		int d = z[0] - '0';` |
|         - | 3522 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3523 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3524 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3525 | `			bOverflow = 1;` |
|       ! 0 | 3526 | `		}else{` |
|        81 | 3527 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3528 | `		}` |
|        81 | 3529 | `		bDigit = 1;` |
|        81 | 3530 | `		z++;` |
|         1 | 3531 | `	}` |
|       157 | 3532 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3533 | `		bReal = 1;` |
|         3 | 3534 | `		z++;` |
|         5 | 3535 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3536 | `			bDigit = 1;` |
|         3 | 3537 | `			z++;` |
|         1 | 3538 | `		}` |
|         1 | 3539 | `	}` |
|         - | 3540 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3541 | `	if( !bDigit ){` |
|        61 | 3542 | `		return RANGE_IN_ERROR;` |
|         - | 3543 | `	}` |
|         - | 3544 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3545 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3546 | `		z++;` |
|         9 | 3547 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3548 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3549 | `			return RANGE_IN_ERROR;` |
|         - | 3550 | `		}` |
|         9 | 3551 | `		bReal = 1;` |
|        17 | 3552 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3553 | `	}` |
|         - | 3554 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3555 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3556 | `	if( z != zEnd ){` |
|        13 | 3557 | `		return RANGE_IN_ERROR;` |
|         - | 3558 | `	}` |
|        84 | 3559 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3560 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3561 | `		bReal = 1;` |
|        84 | 3562 | `	}` |
|        43 | 3563 | `	if( bReal ){` |
|        11 | 3564 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3565 | `		return RANGE_IN_DOUBLE;` |
|         - | 3566 | `	}` |
|         - | 3567 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3568 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3569 | `	return RANGE_IN_LONG;` |
|        58 | 3570 | `}` |
|         - | 3571 | `/*` |
|         - | 3572 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3573 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3574 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3575 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3576 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3577 | ` */` |
|       266 | 3578 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3579 | `{` |
|         - | 3580 | `	char zMsg[160];` |
|       267 | 3581 | `	*pRc = PH7_OK;` |
|       267 | 3582 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3583 | `		char zType[80];` |
|        10 | 3584 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3585 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|         3 | 3586 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         7 | 3587 | `		return FALSE;` |
|         - | 3588 | `	}` |
|       261 | 3589 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3590 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3591 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3592 | `			iArg,zName);` |
|         5 | 3593 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3594 | `		*pbNullCoerced = TRUE;` |
|         2 | 3595 | `	}` |
|       261 | 3596 | `	return TRUE;` |
|       134 | 3597 | `}` |
|         - | 3598 | `/*` |
|         - | 3599 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3600 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3601 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3602 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3603 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3604 | ` */` |
|        62 | 3605 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3606 | `{` |
|        63 | 3607 | `	*pRc = PH7_OK;` |
|        63 | 3608 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3609 | `		char zType[80];` |
|         4 | 3610 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3611 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|         1 | 3612 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         3 | 3613 | `		return RANGE_IN_ERROR;` |
|         - | 3614 | `	}` |
|        61 | 3615 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3616 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3617 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3618 | `		*pLong = 0;` |
|         3 | 3619 | `		return RANGE_IN_LONG;` |
|         - | 3620 | `	}` |
|        59 | 3621 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3622 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3623 | `		return RANGE_IN_DOUBLE;` |
|         - | 3624 | `	}` |
|        35 | 3625 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3626 | `		const char *zStr;` |
|         - | 3627 | `		int nLen;` |
|         - | 3628 | `		sxu8 iKind;` |
|         3 | 3629 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3630 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3631 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3632 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3633 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3634 | `		}` |
|         3 | 3635 | `		return iKind;` |
|         - | 3636 | `	}` |
|         - | 3637 | `	/* int / bool */` |
|        33 | 3638 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3639 | `	return RANGE_IN_LONG;` |
|        32 | 3640 | `}` |
|         - | 3641 | `/*` |
|         - | 3642 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3643 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3644 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3645 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3646 | ` */` |
|       224 | 3647 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3648 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3649 | `{` |
|         - | 3650 | `	char zMsg[160];` |
|         - | 3651 | `	double r;` |
|       225 | 3652 | `	*pRc = PH7_OK;` |
|       225 | 3653 | `	if( bNullCoerced ){` |
|         - | 3654 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3655 | `		*pLong = 0;` |
|         5 | 3656 | `		*pDouble = 0.0;` |
|         5 | 3657 | `		return RANGE_IN_LONG;` |
|         - | 3658 | `	}` |
|       221 | 3659 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3660 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3661 | `check_dval:` |
|        25 | 3662 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3663 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3664 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3665 | `			return RANGE_IN_ERROR;` |
|         - | 3666 | `		}` |
|        21 | 3667 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3668 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3669 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3670 | `			return RANGE_IN_ERROR;` |
|         - | 3671 | `		}` |
|        17 | 3672 | `		*pDouble = r;` |
|        17 | 3673 | `		return RANGE_IN_DOUBLE;` |
|         - | 3674 | `	}` |
|       201 | 3675 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3676 | `		const char *zStr;` |
|         - | 3677 | `		int nLen;` |
|         - | 3678 | `		sxu8 iKind;` |
|        81 | 3679 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3680 | `		if( nLen == 0 ){` |
|         7 | 3681 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3682 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3683 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3684 | `			*pLong = 0;` |
|         5 | 3685 | `			*pDouble = 0.0;` |
|        41 | 3686 | `			return RANGE_IN_LONG;` |
|         - | 3687 | `		}` |
|        77 | 3688 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3689 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3690 | `			r = *pDouble;` |
|         5 | 3691 | `			goto check_dval;` |
|         - | 3692 | `		}` |
|        73 | 3693 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3694 | `			*pDouble = (double)*pLong;` |
|        23 | 3695 | `			if( nLen == 1 ){` |
|         - | 3696 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3697 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3698 | `				return RANGE_IN_DIGIT;` |
|         - | 3699 | `			}` |
|        15 | 3700 | `			return RANGE_IN_LONG;` |
|         - | 3701 | `		}` |
|        51 | 3702 | `		if( nLen != 1 ){` |
|        10 | 3703 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3704 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3705 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3706 | `		}` |
|        51 | 3707 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3708 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3709 | `		*pLong = 0;` |
|        51 | 3710 | `		*pDouble = 0.0;` |
|        51 | 3711 | `		return RANGE_IN_STRING;` |
|         - | 3712 | `	}` |
|         - | 3713 | `	/* int / bool */` |
|       121 | 3714 | `	*pLong = ph7_value_to_int64(pIn);` |
|       121 | 3715 | `	*pDouble = (double)*pLong;` |
|       121 | 3716 | `	return RANGE_IN_LONG;` |
|       113 | 3717 | `}` |
|         - | 3718 | `/*` |
|         - | 3719 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3720 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3721 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3722 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3723 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3724 | ` * exactly like php's two macros.` |
|         - | 3725 | ` */` |
|         6 | 3726 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3727 | `{` |
|        10 | 3728 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3729 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3730 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3731 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3732 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3733 | `}` |
|         6 | 3734 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3735 | `{` |
|         - | 3736 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3737 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3738 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3739 | `	const unsigned int nBuf = 1500;` |
|         7 | 3740 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3741 | `	if( zMsg == 0 ){` |
|       ! 0 | 3742 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3743 | `	}` |
|         7 | 3744 | `	snprintf(zMsg,nBuf,` |
|         - | 3745 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3746 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3747 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3748 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3749 | `}` |
|         - | 3750 | `/*` |
|         - | 3751 | ` * Set the element container to the next range element and append it to the` |
|         - | 3752 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3753 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3754 | ` * below stay one line per iteration.` |
|         - | 3755 | ` */` |
|       374 | 3756 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3757 | `{` |
|       375 | 3758 | `	ph7_value_int64(pValue,iVal);` |
|       375 | 3759 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3760 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3761 | `	}` |
|       375 | 3762 | `	return PH7_OK;` |
|       188 | 3763 | `}` |
|        70 | 3764 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3765 | `{` |
|        71 | 3766 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3767 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3768 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3769 | `	}` |
|        71 | 3770 | `	return PH7_OK;` |
|        36 | 3771 | `}` |
|       168 | 3772 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3773 | `{` |
|       169 | 3774 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3775 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3776 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3777 | `	}` |
|       169 | 3778 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3779 | `	return PH7_OK;` |
|        85 | 3780 | `}` |
|         - | 3781 | `/*` |
|         - | 3782 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3783 | ` *  Create an array containing a range of elements.` |
|         - | 3784 | ` * Return` |
|         - | 3785 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3786 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3787 | ` */` |
|       138 | 3788 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3789 | `{` |
|         - | 3790 | `	ph7_value *pValue,*pArray;` |
|       139 | 3791 | `	sxi32 rc = PH7_OK;` |
|       139 | 3792 | `	int is_step_double = 0,is_step_negative = 0;` |
|       139 | 3793 | `	double step_double = 1.0;` |
|       139 | 3794 | `	sxi64 step = 1;` |
|         - | 3795 | `	sxu8 start_type,end_type;` |
|       139 | 3796 | `	sxi64 start_long = 0,end_long = 0;` |
|       139 | 3797 | `	double start_double = 0.0,end_double = 0.0;` |
|       139 | 3798 | `	unsigned char cStart = 0,cEnd = 0;` |
|       139 | 3799 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3800 | `	sxu32 i,size;` |
|         - | 3801 |  |
|         - | 3802 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       139 | 3803 | `	if( nArg > 3 ){` |
|         4 | 3804 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3805 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3806 | `	}` |
|       137 | 3807 | `	if( nArg < 2 ){` |
|         - | 3808 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3809 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3810 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3811 | `	}` |
|         - | 3812 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3813 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       137 | 3814 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|         7 | 3815 | `		return rc;` |
|         - | 3816 | `	}` |
|       131 | 3817 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3818 | `		return rc;` |
|         - | 3819 | `	}` |
|       131 | 3820 | `	if( nArg > 2 ){` |
|        63 | 3821 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        63 | 3822 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         5 | 3823 | `			return rc;` |
|         - | 3824 | `		}` |
|        59 | 3825 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3826 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3827 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3828 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3829 | `			}` |
|        23 | 3830 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3831 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3832 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3833 | `			}` |
|         - | 3834 | `			/* We only want positive step values. */` |
|        21 | 3835 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3836 | `				is_step_negative = 1;` |
|       ! 0 | 3837 | `				step_double *= -1;` |
|       ! 0 | 3838 | `			}` |
|         - | 3839 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 3840 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 3841 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 3842 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 3843 | `				step = (sxi64)step_double;` |
|        19 | 3844 | `				if( (double)step != step_double ){` |
|        17 | 3845 | `					is_step_double = 1;` |
|         8 | 3846 | `				}` |
|        10 | 3847 | `			}else{` |
|         - | 3848 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 3849 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 3850 | `				is_step_double = 1;` |
|         - | 3851 | `			}` |
|        11 | 3852 | `		}else{` |
|         - | 3853 | `			/* We only want positive step values. */` |
|        35 | 3854 | `			if( step < 0 ){` |
|        11 | 3855 | `				if( step == SMALLEST_INT64 ){` |
|         - | 3856 | `					/* -step would overflow */` |
|         4 | 3857 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 3858 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 3859 | `				}` |
|         9 | 3860 | `				is_step_negative = 1;` |
|         9 | 3861 | `				step = -step;` |
|         4 | 3862 | `			}` |
|        33 | 3863 | `			step_double = (double)step;` |
|         - | 3864 | `		}` |
|        53 | 3865 | `		if( step_double == 0.0 ){` |
|         7 | 3866 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3867 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 3868 | `		}` |
|        23 | 3869 | `	}` |
|       115 | 3870 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       115 | 3871 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 3872 | `		return rc;` |
|         - | 3873 | `	}` |
|       111 | 3874 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       111 | 3875 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 3876 | `		return rc;` |
|         - | 3877 | `	}` |
|         - | 3878 | `	/* Element container + result array */` |
|       107 | 3879 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       107 | 3880 | `	pArray = ph7_context_new_array(pCtx);` |
|       107 | 3881 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 3882 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3883 | `	}` |
|         - | 3884 | `	/* If the range is given as strings, generate an array of characters. */` |
|       107 | 3885 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 3886 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 3887 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 3888 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 3889 | `			 * and the range is numeric. */` |
|        15 | 3890 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 3891 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 3892 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3893 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 3894 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 3895 | `				}` |
|         7 | 3896 | `				end_type = RANGE_IN_LONG;` |
|         4 | 3897 | `			}else{` |
|         9 | 3898 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 3899 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3900 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 3901 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 3902 | `				}` |
|         9 | 3903 | `				start_type = RANGE_IN_LONG;` |
|         - | 3904 | `			}` |
|        15 | 3905 | `			goto handle_numeric_inputs;` |
|         - | 3906 | `		}` |
|        23 | 3907 | `		if( is_step_double ){` |
|         - | 3908 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 3909 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 3910 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3911 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 3912 | `					" of characters, inputs converted to 0");` |
|         1 | 3913 | `			}` |
|         5 | 3914 | `			start_type = RANGE_IN_LONG;` |
|         5 | 3915 | `			end_type = RANGE_IN_LONG;` |
|         5 | 3916 | `			goto handle_numeric_inputs;` |
|         - | 3917 | `		}` |
|         - | 3918 | `		/* Generate an array of characters */` |
|        19 | 3919 | `		if( cStart > cEnd ){` |
|         - | 3920 | `			/* Decreasing char range */` |
|         - | 3921 | `			int iCur;` |
|         3 | 3922 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 3923 | `				goto boundary_error;` |
|         - | 3924 | `			}` |
|        17 | 3925 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 3926 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 3927 | `					return rc;` |
|         - | 3928 | `				}` |
|         8 | 3929 | `			}` |
|        18 | 3930 | `		}else if( cEnd > cStart ){` |
|         - | 3931 | `			/* Increasing char range */` |
|         - | 3932 | `			int iCur;` |
|        15 | 3933 | `			if( is_step_negative ){` |
|         3 | 3934 | `				goto negative_step_error;` |
|         - | 3935 | `			}` |
|        13 | 3936 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 3937 | `				goto boundary_error;` |
|         - | 3938 | `			}` |
|       163 | 3939 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 3940 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 3941 | `					return rc;` |
|         - | 3942 | `				}` |
|        77 | 3943 | `			}` |
|         6 | 3944 | `		}else{` |
|         3 | 3945 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 3946 | `				return rc;` |
|         - | 3947 | `			}` |
|         - | 3948 | `		}` |
|        15 | 3949 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 3950 | `		return PH7_OK;` |
|         - | 3951 | `	}` |
|        35 | 3952 | `handle_numeric_inputs:` |
|        97 | 3953 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 3954 | `		/* Float range */` |
|         - | 3955 | `		double elem,calc;` |
|        25 | 3956 | `		if( start_double > end_double ){` |
|         - | 3957 | `			/* Decreasing float range */` |
|         7 | 3958 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 3959 | `				goto boundary_error;` |
|         - | 3960 | `			}` |
|         7 | 3961 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 3962 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 3963 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 3964 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 3965 | `			}` |
|         5 | 3966 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 3967 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 3968 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 3969 | `					return rc;` |
|         - | 3970 | `				}` |
|         8 | 3971 | `			}` |
|        21 | 3972 | `		}else if( end_double > start_double ){` |
|         - | 3973 | `			/* Increasing float range */` |
|        17 | 3974 | `			if( is_step_negative ){` |
|       ! 0 | 3975 | `				goto negative_step_error;` |
|         - | 3976 | `			}` |
|        17 | 3977 | `			if( end_double - start_double < step_double ){` |
|         3 | 3978 | `				goto boundary_error;` |
|         - | 3979 | `			}` |
|        15 | 3980 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 3981 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 3982 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 3983 | `			}` |
|        11 | 3984 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 3985 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 3986 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 3987 | `					return rc;` |
|         - | 3988 | `				}` |
|        28 | 3989 | `			}` |
|         6 | 3990 | `		}else{` |
|         3 | 3991 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 3992 | `				return rc;` |
|         - | 3993 | `			}` |
|         - | 3994 | `		}` |
|         9 | 3995 | `	}else{` |
|         - | 3996 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 3997 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 3998 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|        65 | 3999 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4000 | `		sxu64 calc;` |
|        65 | 4001 | `		if( start_long > end_long ){` |
|         - | 4002 | `			/* Decreasing int range */` |
|        19 | 4003 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4004 | `				goto boundary_error;` |
|         - | 4005 | `			}` |
|        17 | 4006 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4007 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4008 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4009 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4010 | `			}` |
|        15 | 4011 | `			size = (sxu32)(calc + 1);` |
|       101 | 4012 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4013 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4014 | `					return rc;` |
|         - | 4015 | `				}` |
|        44 | 4016 | `			}` |
|        54 | 4017 | `		}else if( end_long > start_long ){` |
|         - | 4018 | `			/* Increasing int range */` |
|        41 | 4019 | `			if( is_step_negative ){` |
|         3 | 4020 | `				goto negative_step_error;` |
|         - | 4021 | `			}` |
|        39 | 4022 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4023 | `				goto boundary_error;` |
|         - | 4024 | `			}` |
|        37 | 4025 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        37 | 4026 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4027 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4028 | `			}` |
|        33 | 4029 | `			size = (sxu32)(calc + 1);` |
|       315 | 4030 | `			for( i = 0 ; i < size ; ++i ){` |
|       283 | 4031 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4032 | `					return rc;` |
|         - | 4033 | `				}` |
|       142 | 4034 | `			}` |
|        17 | 4035 | `		}else{` |
|         7 | 4036 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4037 | `				return rc;` |
|         - | 4038 | `			}` |
|         - | 4039 | `		}` |
|         - | 4040 | `	}` |
|         - | 4041 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4042 | `	 * virtual machine as soon as we return from this foreign function. */` |
|        69 | 4043 | `	ph7_result_value(pCtx,pArray);` |
|        69 | 4044 | `	return PH7_OK;` |
|         2 | 4045 | `negative_step_error:` |
|         5 | 4046 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4047 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4048 | `boundary_error:` |
|         9 | 4049 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4050 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        70 | 4051 | `}` |
|         - | 4052 | `/*` |
|         - | 4053 | ` * array array_values(array $array)` |
|         - | 4054 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4055 | ` * Parameters` |
|         - | 4056 | ` *  $array` |
|         - | 4057 | ` *   The input array.` |
|         - | 4058 | ` * Return` |
|         - | 4059 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4060 | ` */` |
|        36 | 4061 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4062 | `{` |
|         - | 4063 | `	ph7_hashmap_node *pNode;` |
|         - | 4064 | `	ph7_hashmap *pMap;` |
|         - | 4065 | `	ph7_value *pArray;` |
|         - | 4066 | `	ph7_value *pObj;` |
|         - | 4067 | `	sxu32 n;` |
|        40 | 4068 | `	if( nArg != 1 ){` |
|         - | 4069 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         8 | 4070 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4071 | `			"ArgumentCountError",` |
|         - | 4072 | `			"array_values() expects exactly 1 argument, %d given",` |
|         2 | 4073 | `			nArg` |
|         - | 4074 | `			);` |
|         - | 4075 | `	}` |
|         - | 4076 | `	/* Make sure we are dealing with a valid hashmap */` |
|        35 | 4077 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4078 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4079 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4080 | `			"TypeError",` |
|         - | 4081 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4082 | `			ph7_type_name(apArg[0])` |
|         - | 4083 | `			);` |
|         - | 4084 | `	}` |
|         - | 4085 | `	/* Point to the internal representation that describe the input hashmap */` |
|        32 | 4086 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4087 | `	/* Create a new array */` |
|        32 | 4088 | `	pArray = ph7_context_new_array(pCtx);` |
|        32 | 4089 | `	if( pArray == 0 ){` |
|       ! 0 | 4090 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4091 | `		return PH7_OK;` |
|         - | 4092 | `	}` |
|         - | 4093 | `	/* Perform the requested operation */` |
|        32 | 4094 | `	pNode = pMap->pFirst;` |
|       104 | 4095 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        74 | 4096 | `		pObj = HashmapExtractNodeValue(pNode);` |
|        74 | 4097 | `		if( pObj ){` |
|         - | 4098 | `			/* perform the insertion */` |
|        74 | 4099 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        36 | 4100 | `		}` |
|         - | 4101 | `		/* Point to the next entry */` |
|        74 | 4102 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        38 | 4103 | `	}` |
|         - | 4104 | `	/* return the new array */` |
|        32 | 4105 | `	ph7_result_value(pCtx,pArray);` |
|        32 | 4106 | `	return PH7_OK;` |
|        22 | 4107 | `}` |
|         - | 4108 | `/*` |
|         - | 4109 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4110 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4111 | ` * Parameters` |
|         - | 4112 | ` *  $input` |
|         - | 4113 | ` *   An array containing keys to return.` |
|         - | 4114 | ` * $search_value` |
|         - | 4115 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4116 | ` * $strict` |
|         - | 4117 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4118 | ` * Return` |
|         - | 4119 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4120 | ` */` |
|       150 | 4121 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4122 | `{` |
|         - | 4123 | `	ph7_hashmap_node *pNode;` |
|         - | 4124 | `	ph7_hashmap *pMap;` |
|         - | 4125 | `	ph7_value *pArray;` |
|         - | 4126 | `	ph7_value sObj;` |
|         - | 4127 | `	ph7_value sVal;` |
|         - | 4128 | `	SyString sKey;` |
|         - | 4129 | `	int bStrict;` |
|         - | 4130 | `	sxi32 rc;` |
|         - | 4131 | `	sxu32 n;` |
|       155 | 4132 | `	if( nArg < 1 ){` |
|         - | 4133 | `		/* Missing argument,throw ArgumentCountError */` |
|         3 | 4134 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4135 | `			"ArgumentCountError",` |
|         - | 4136 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4137 | `			);` |
|         - | 4138 | `	}` |
|         - | 4139 | `	/* Make sure we are dealing with a valid hashmap */` |
|       152 | 4140 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4141 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4142 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4143 | `			"TypeError",` |
|         - | 4144 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4145 | `			ph7_type_name(apArg[0])` |
|         - | 4146 | `			);` |
|         - | 4147 | `	}` |
|         - | 4148 | `	/* Point to the internal representation of the input hashmap */` |
|       150 | 4149 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4150 | `	/* Create a new array */` |
|       150 | 4151 | `	pArray = ph7_context_new_array(pCtx);` |
|       150 | 4152 | `	if( pArray == 0 ){` |
|       ! 0 | 4153 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4154 | `		return PH7_OK;` |
|         - | 4155 | `	}` |
|       150 | 4156 | `	bStrict = FALSE;` |
|       150 | 4157 | `	if( nArg > 2 ){` |
|         - | 4158 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        12 | 4159 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4160 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4161 | `				"TypeError",` |
|         - | 4162 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4163 | `				ph7_type_name(apArg[2])` |
|         - | 4164 | `				);` |
|         - | 4165 | `		}` |
|         9 | 4166 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4167 | `	}` |
|         - | 4168 | `	/* Perform the requested operation */` |
|       147 | 4169 | `	pNode = pMap->pFirst;` |
|       147 | 4170 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1411 | 4171 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1267 | 4172 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       163 | 4173 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        83 | 4174 | `		}else{` |
|      1106 | 4175 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1106 | 4176 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4177 | `		}` |
|      1267 | 4178 | `		rc = 0;` |
|      1267 | 4179 | `		if( nArg > 1 ){` |
|        65 | 4180 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4181 | `			if( pValue ){` |
|         - | 4182 | `				ph7_value sNeedle;` |
|        65 | 4183 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4184 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4185 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4186 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4187 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4188 | `				 * corrupt every later comparison. */` |
|        65 | 4189 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4190 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4191 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4192 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4193 | `			}` |
|        32 | 4194 | `		}` |
|      1267 | 4195 | `		if( rc == 0 ){` |
|         - | 4196 | `			/* Perform the insertion */` |
|      1235 | 4197 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       616 | 4198 | `		}` |
|      1267 | 4199 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4200 | `		/* Point to the next entry */` |
|      1267 | 4201 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       635 | 4202 | `	}` |
|         - | 4203 | `	/* return the new array */` |
|       147 | 4204 | `	ph7_result_value(pCtx,pArray);` |
|       147 | 4205 | `	return PH7_OK;` |
|        80 | 4206 | `}` |
|         - | 4207 | `/*` |
|         - | 4208 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4209 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4210 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4211 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4212 | ` * Parameters` |
|         - | 4213 | ` *  $arr1` |
|         - | 4214 | ` *   First array` |
|         - | 4215 | ` *  $arr2` |
|         - | 4216 | ` *   Second array` |
|         - | 4217 | ` * Return` |
|         - | 4218 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4219 | ` * Note` |
|         - | 4220 | ` *  This function is a symisc eXtension.` |
|         - | 4221 | ` */` |
|         4 | 4222 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4223 | `{` |
|         - | 4224 | `	ph7_hashmap *p1,*p2;` |
|         - | 4225 | `	int rc;` |
|         5 | 4226 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4227 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4228 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4229 | `		return PH7_OK;` |
|         - | 4230 | `	}` |
|         - | 4231 | `	/* Point to the hashmaps */` |
|         5 | 4232 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4233 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4234 | `	rc = (p1 == p2);` |
|         - | 4235 | `	/* Same instance? */` |
|         5 | 4236 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4237 | `	return PH7_OK;` |
|         3 | 4238 | `}` |
|         - | 4239 | `/*` |
|         - | 4240 | ` * array array_merge(array ...$arrays)` |
|         - | 4241 | ` *  Merge one or more arrays.` |
|         - | 4242 | ` * Parameters` |
|         - | 4243 | ` *  ...$arrays` |
|         - | 4244 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4245 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4246 | ` * Return` |
|         - | 4247 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4248 | ` *  with no arguments.` |
|         - | 4249 | ` */` |
|      1038 | 4250 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4251 | `{` |
|         - | 4252 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4253 | `	ph7_value *pArray;` |
|         - | 4254 | `	int i;` |
|         - | 4255 | `	/* Create a new array */` |
|      1043 | 4256 | `	pArray = ph7_context_new_array(pCtx);` |
|      1043 | 4257 | `	if( pArray == 0 ){` |
|       ! 0 | 4258 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4259 | `		return PH7_OK;` |
|         - | 4260 | `	}` |
|         - | 4261 | `	/* Point to the internal representation of the hashmap */` |
|      1043 | 4262 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4263 | `	/* Start merging */` |
|      3109 | 4264 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4265 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2075 | 4266 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4267 | `			/* Type mismatch -> TypeError */` |
|         8 | 4268 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4269 | `				"TypeError",` |
|         - | 4270 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4271 | `				i + 1,` |
|         4 | 4272 | `				ph7_type_name(apArg[i])` |
|         - | 4273 | `				);` |
|       ! 0 | 4274 | `		}else{` |
|      2071 | 4275 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4276 | `			/* Merge the two hashmaps */` |
|      2071 | 4277 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4278 | `		}` |
|      1038 | 4279 | `	}` |
|         - | 4280 | `	/* Return the freshly created array */` |
|      1039 | 4281 | `	ph7_result_value(pCtx,pArray);` |
|      1039 | 4282 | `	return PH7_OK;` |
|       524 | 4283 | `}` |
|         - | 4284 | `/*` |
|         - | 4285 | ` * array array_copy(array $source)` |
|         - | 4286 | ` *  Make a blind copy of the target array.` |
|         - | 4287 | ` * Parameters` |
|         - | 4288 | ` *  $source` |
|         - | 4289 | ` *   Target array` |
|         - | 4290 | ` * Return` |
|         - | 4291 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4292 | ` * Note` |
|         - | 4293 | ` *  This function is a symisc eXtension.` |
|         - | 4294 | ` */` |
|        16 | 4295 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4296 | `{` |
|         - | 4297 | `	ph7_hashmap *pMap;` |
|         - | 4298 | `	ph7_value *pArray;` |
|        17 | 4299 | `	if( nArg < 1 ){` |
|         - | 4300 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4301 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4302 | `		return PH7_OK;` |
|         - | 4303 | `	}` |
|         - | 4304 | `	/* Create a new array */` |
|        17 | 4305 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 4306 | `	if( pArray == 0 ){` |
|       ! 0 | 4307 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4308 | `		return PH7_OK;` |
|         - | 4309 | `	}` |
|         - | 4310 | `	/* Point to the internal representation of the hashmap */` |
|        17 | 4311 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        17 | 4312 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4313 | `		/* Point to the internal representation of the source */` |
|        17 | 4314 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4315 | `		/* Perform the copy */` |
|        17 | 4316 | `		PH7_HashmapDup(pSrc,pMap);` |
|         9 | 4317 | `	}else{` |
|         - | 4318 | `		/* Simple insertion */` |
|       ! 0 | 4319 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4320 | `	}` |
|         - | 4321 | `	/* Return the duplicated array */` |
|        17 | 4322 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 4323 | `	return PH7_OK;` |
|         9 | 4324 | `}` |
|         - | 4325 | `/*` |
|         - | 4326 | ` * bool array_erase(array $source)` |
|         - | 4327 | ` *  Remove all elements from a given array.` |
|         - | 4328 | ` * Parameters` |
|         - | 4329 | ` *  $source` |
|         - | 4330 | ` *   Target array` |
|         - | 4331 | ` * Return` |
|         - | 4332 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4333 | ` * Note` |
|         - | 4334 | ` *  This function is a symisc eXtension.` |
|         - | 4335 | ` */` |
|        16 | 4336 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4337 | `{` |
|         - | 4338 | `	ph7_hashmap *pMap;` |
|        17 | 4339 | `	if( nArg < 1 ){` |
|         - | 4340 | `		/* Missing arguments */` |
|       ! 0 | 4341 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4342 | `		return PH7_OK;` |
|         - | 4343 | `	}` |
|         - | 4344 | `	/* Point to the target hashmap */` |
|        17 | 4345 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        17 | 4346 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4347 | `	/* Erase */` |
|        17 | 4348 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        17 | 4349 | `	return PH7_OK;` |
|         9 | 4350 | `}` |
|         - | 4351 | `/*` |
|         - | 4352 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4353 | ` *  Extract a slice of the array.` |
|         - | 4354 | ` * Parameters` |
|         - | 4355 | ` *  $array` |
|         - | 4356 | ` *    The input array.` |
|         - | 4357 | ` * $offset` |
|         - | 4358 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4359 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4360 | ` * $length (optional, nullable)` |
|         - | 4361 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4362 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4363 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4364 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4365 | ` * $preserve_keys (optional)` |
|         - | 4366 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4367 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4368 | ` * Return` |
|         - | 4369 | ` *   The new slice.` |
|         - | 4370 | ` */` |
|        50 | 4371 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4372 | `{` |
|         - | 4373 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4374 | `	ph7_hashmap_node *pCur;` |
|         - | 4375 | `	ph7_value *pArray;` |
|         - | 4376 | `	int iLength,iOfft;` |
|         - | 4377 | `	int bPreserve;` |
|         - | 4378 | `	sxi32 rc;` |
|        55 | 4379 | `	if( nArg < 2 ){` |
|         8 | 4380 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4381 | `			"ArgumentCountError",` |
|         - | 4382 | `			"array_slice() expects at least 2 arguments, %d given",` |
|         2 | 4383 | `			nArg` |
|         - | 4384 | `			);` |
|         - | 4385 | `	}` |
|        51 | 4386 | `	if( nArg > 4 ){` |
|         4 | 4387 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4388 | `			"ArgumentCountError",` |
|         - | 4389 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4390 | `			nArg` |
|         - | 4391 | `			);` |
|         - | 4392 | `	}` |
|        49 | 4393 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4394 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4395 | `			"TypeError",` |
|         - | 4396 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4397 | `			ph7_type_name(apArg[0])` |
|         - | 4398 | `			);` |
|         - | 4399 | `	}` |
|         - | 4400 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        62 | 4401 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        65 | 4402 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4403 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4404 | `			"TypeError",` |
|         - | 4405 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4406 | `			ph7_type_name(apArg[1])` |
|         - | 4407 | `			);` |
|         - | 4408 | `	}` |
|         - | 4409 | `	/* Validate $length type if provided: nullable int */` |
|        45 | 4410 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        26 | 4411 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        26 | 4412 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4413 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4414 | `				"TypeError",` |
|         - | 4415 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4416 | `				ph7_type_name(apArg[2])` |
|         - | 4417 | `				);` |
|         - | 4418 | `		}` |
|         8 | 4419 | `	}` |
|         - | 4420 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        43 | 4421 | `	if( nArg > 3 ){` |
|        10 | 4422 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4423 | `			ph7_value_is_resource(apArg[3]) ){` |
|         4 | 4424 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4425 | `				"TypeError",` |
|         - | 4426 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|         2 | 4427 | `				ph7_type_name(apArg[3])` |
|         - | 4428 | `				);` |
|         - | 4429 | `		}` |
|         2 | 4430 | `	}` |
|         - | 4431 | `	/* Point the internal representation of the target array */` |
|        41 | 4432 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 4433 | `	bPreserve = FALSE;` |
|         - | 4434 | `	/* Get the offset */` |
|        41 | 4435 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        41 | 4436 | `	if( iOfft < 0 ){` |
|         5 | 4437 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4438 | `		if( iOfft < 0 ){` |
|         3 | 4439 | `			iOfft = 0;` |
|         1 | 4440 | `		}` |
|         2 | 4441 | `	}` |
|        41 | 4442 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4443 | `		/* Offset past end of array, return empty array */` |
|         5 | 4444 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4445 | `		if( pArray == 0 ){` |
|       ! 0 | 4446 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4447 | `			return PH7_OK;` |
|         - | 4448 | `		}` |
|         5 | 4449 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4450 | `		return PH7_OK;` |
|         - | 4451 | `	}` |
|         - | 4452 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        37 | 4453 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        37 | 4454 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        15 | 4455 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        15 | 4456 | `		if( iLength < 0 ){` |
|         5 | 4457 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4458 | `		}` |
|        15 | 4459 | `		if( iLength < 0 ){` |
|         3 | 4460 | `			iLength = 0;` |
|         1 | 4461 | `		}` |
|        15 | 4462 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4463 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4464 | `		}` |
|         7 | 4465 | `	}` |
|        37 | 4466 | `	if( nArg > 3 ){` |
|         5 | 4467 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4468 | `	}` |
|         - | 4469 | `	/* Create a new array */` |
|        37 | 4470 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 4471 | `	if( pArray == 0 ){` |
|       ! 0 | 4472 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4473 | `		return PH7_OK;` |
|         - | 4474 | `	}` |
|        37 | 4475 | `	if( iLength < 1 ){` |
|         - | 4476 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4477 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4478 | `		return PH7_OK;` |
|         - | 4479 | `	}` |
|         - | 4480 | `	/* Point to the desired entry */` |
|        33 | 4481 | `	pCur = pSrc->pFirst;` |
|        28 | 4482 | `	for(;;){` |
|        61 | 4483 | `		if( iOfft < 1 ){` |
|        33 | 4484 | `			break;` |
|         - | 4485 | `		}` |
|         - | 4486 | `		/* Point to the next entry */` |
|        33 | 4487 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4488 | `		iOfft--;` |
|         5 | 4489 | `	}` |
|         - | 4490 | `	/* Point to the internal representation of the hashmap */` |
|        33 | 4491 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        51 | 4492 | `	for(;;){` |
|       107 | 4493 | `		if( iLength < 1 ){` |
|        33 | 4494 | `			break;` |
|         - | 4495 | `		}` |
|         - | 4496 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4497 | `		{` |
|        79 | 4498 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        79 | 4499 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4500 | `		}` |
|        79 | 4501 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4502 | `			break;` |
|         - | 4503 | `		}` |
|         - | 4504 | `		/* Point to the next entry */` |
|        79 | 4505 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        79 | 4506 | `		iLength--;` |
|         5 | 4507 | `	}` |
|         - | 4508 | `	/* Return the freshly created array */` |
|        33 | 4509 | `	ph7_result_value(pCtx,pArray);` |
|        33 | 4510 | `	return PH7_OK;` |
|        30 | 4511 | `}` |
|         - | 4512 | `/*` |
|         - | 4513 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4514 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4515 | ` * beginning (becomes the new pFirst).` |
|         - | 4516 | ` */` |
|        30 | 4517 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4518 | `{` |
|         - | 4519 | `	ph7_hashmap_node *pNode;` |
|         - | 4520 | `	ph7_hashmap_node *pOldNext;` |
|        31 | 4521 | `	pNode = pMap->pLast;` |
|        31 | 4522 | `	if( pNode == 0 ){` |
|       ! 0 | 4523 | `		return;` |
|         - | 4524 | `	}` |
|        31 | 4525 | `	if( pNode->pNext == 0 ){` |
|         - | 4526 | `		/* Only node in the list, nothing to move */` |
|         5 | 4527 | `		return;` |
|         - | 4528 | `	}` |
|        27 | 4529 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4530 | `		/* Already in the correct position */` |
|         9 | 4531 | `		return;` |
|         - | 4532 | `	}` |
|         - | 4533 | `	/* Unlink pNode from the end of the list */` |
|        19 | 4534 | `	pMap->pLast = pNode->pNext;` |
|        19 | 4535 | `	pMap->pLast->pPrev = 0;` |
|         - | 4536 | `	/* Insert pNode after pAfter in iteration order */` |
|        19 | 4537 | `	if( pAfter == 0 ){` |
|         - | 4538 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4539 | `		pNode->pNext = 0;` |
|         3 | 4540 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4541 | `		if( pMap->pFirst ){` |
|         3 | 4542 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4543 | `		}` |
|         3 | 4544 | `		pMap->pFirst = pNode;` |
|         2 | 4545 | `	}else{` |
|        17 | 4546 | `		pOldNext = pAfter->pPrev;` |
|        17 | 4547 | `		pNode->pPrev = pOldNext;` |
|        17 | 4548 | `		pNode->pNext = pAfter;` |
|        17 | 4549 | `		pAfter->pPrev = pNode;` |
|        17 | 4550 | `		if( pOldNext ){` |
|        17 | 4551 | `			pOldNext->pNext = pNode;` |
|         9 | 4552 | `		}else{` |
|       ! 0 | 4553 | `			pMap->pLast = pNode;` |
|         - | 4554 | `		}` |
|         - | 4555 | `	}` |
|        16 | 4556 | `}` |
|         - | 4557 | `/*` |
|         - | 4558 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4559 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4560 | ` * Parameters` |
|         - | 4561 | ` *  $array` |
|         - | 4562 | ` *    The input array.` |
|         - | 4563 | ` *  $offset` |
|         - | 4564 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4565 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4566 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4567 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4568 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4569 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4570 | ` *  $length (optional)` |
|         - | 4571 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4572 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4573 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4574 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4575 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4576 | ` *  $replacement (optional)` |
|         - | 4577 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4578 | ` *    with elements from this array.` |
|         - | 4579 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4580 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4581 | ` *    offset.` |
|         - | 4582 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4583 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4584 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4585 | ` * Return` |
|         - | 4586 | ` *   A new array consisting of the extracted elements.` |
|         - | 4587 | ` */` |
|        54 | 4588 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4589 | `{` |
|         - | 4590 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4591 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4592 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4593 | `	int iLength,iOfft,i;` |
|         - | 4594 | `	sxi32 rc;` |
|        58 | 4595 | `	if( nArg < 2 ){` |
|         8 | 4596 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4597 | `			"ArgumentCountError",` |
|         - | 4598 | `			"array_splice() expects at least 2 arguments, %d given",` |
|         2 | 4599 | `			nArg` |
|         - | 4600 | `			);` |
|         - | 4601 | `	}` |
|        52 | 4602 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4603 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4604 | `			"TypeError",` |
|         - | 4605 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4606 | `			ph7_type_name(apArg[0])` |
|         - | 4607 | `			);` |
|         - | 4608 | `	}` |
|         - | 4609 | `	/* Point to the internal representation of the target array */` |
|        49 | 4610 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        49 | 4611 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4612 | `	/* Get the offset and clamp to valid range */` |
|        49 | 4613 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        49 | 4614 | `	if( iOfft < 0 ){` |
|         7 | 4615 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         7 | 4616 | `		if( iOfft < 0 ){` |
|         3 | 4617 | `			iOfft = 0;` |
|         2 | 4618 | `		}` |
|        46 | 4619 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4620 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4621 | `	}` |
|         - | 4622 | `	/* Get the length and clamp to valid range.` |
|         - | 4623 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        49 | 4624 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        49 | 4625 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        31 | 4626 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        31 | 4627 | `		if( iLength < 0 ){` |
|         7 | 4628 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4629 | `			if( iLength < 0 ){` |
|         3 | 4630 | `				iLength = 0;` |
|         1 | 4631 | `			}` |
|         3 | 4632 | `		}` |
|        31 | 4633 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4634 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4635 | `		}` |
|        15 | 4636 | `	}` |
|         - | 4637 | `	/* Create the result array for removed elements */` |
|        49 | 4638 | `	pArray = ph7_context_new_array(pCtx);` |
|        49 | 4639 | `	if( pArray == 0 ){` |
|       ! 0 | 4640 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4641 | `		return PH7_OK;` |
|         - | 4642 | `	}` |
|         - | 4643 | `	/* Get replacement array if provided */` |
|        49 | 4644 | `	pRep = 0;` |
|        49 | 4645 | `	if( nArg > 3 ){` |
|        21 | 4646 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4647 | `			/* Perform an array cast */` |
|         3 | 4648 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4649 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4650 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4651 | `			}` |
|         2 | 4652 | `		}else{` |
|        19 | 4653 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4654 | `		}` |
|        21 | 4655 | `		if( pRep ){` |
|         - | 4656 | `			/* Reset the loop cursor */` |
|        21 | 4657 | `			pRep->pCur = pRep->pFirst;` |
|        10 | 4658 | `		}` |
|        10 | 4659 | `	}` |
|         - | 4660 | `	/* Early return if nothing to remove and no replacement */` |
|        49 | 4661 | `	if( iLength < 1 && pRep == 0 ){` |
|         9 | 4662 | `		ph7_result_value(pCtx,pArray);` |
|         9 | 4663 | `		return PH7_OK;` |
|         - | 4664 | `	}` |
|         - | 4665 | `	/* Navigate to the offset position */` |
|        41 | 4666 | `	pCur = pSrc->pFirst;` |
|        85 | 4667 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        45 | 4668 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        23 | 4669 | `	}` |
|         - | 4670 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4671 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4672 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        41 | 4673 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4674 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        41 | 4675 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       111 | 4676 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        71 | 4677 | `		pPrev = pCur->pPrev;` |
|        71 | 4678 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        71 | 4679 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        71 | 4680 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4681 | `			break;` |
|         - | 4682 | `		}` |
|        71 | 4683 | `		pCur = pPrev; /* Reverse link */` |
|        36 | 4684 | `	}` |
|         - | 4685 | `	/* Insert replacement elements at the correct position */` |
|        41 | 4686 | `	if( pRep ){` |
|         - | 4687 | `		ph7_value sSafeVal;` |
|        61 | 4688 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        31 | 4689 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        31 | 4690 | `			if( pRvalue ){` |
|         - | 4691 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4692 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4693 | `				 * since it points into that same pool. */` |
|        31 | 4694 | `				sSafeVal = *pRvalue;` |
|        31 | 4695 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        31 | 4696 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        31 | 4697 | `					pNewNode = pSrc->pLast;` |
|        31 | 4698 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        31 | 4699 | `					pInsertAfter = pNewNode;` |
|        15 | 4700 | `				}` |
|        15 | 4701 | `			}` |
|         1 | 4702 | `		}` |
|        10 | 4703 | `	}` |
|         - | 4704 | `	/* Return the freshly created array */` |
|        41 | 4705 | `	ph7_result_value(pCtx,pArray);` |
|        41 | 4706 | `	return PH7_OK;` |
|        31 | 4707 | `}` |
|         - | 4708 | `/*` |
|         - | 4709 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4710 | ` *  Checks if a value exists in an array.` |
|         - | 4711 | ` * Parameters` |
|         - | 4712 | ` *  $needle` |
|         - | 4713 | ` *   The searched value.` |
|         - | 4714 | ` *   Note:` |
|         - | 4715 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4716 | ` * $haystack` |
|         - | 4717 | ` *  The target array.` |
|         - | 4718 | ` * $strict` |
|         - | 4719 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4720 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4721 | ` */` |
|     32722 | 4722 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4723 | `{` |
|         - | 4724 | `	ph7_value *pNeedle;` |
|         - | 4725 | `	int bStrict;` |
|         - | 4726 | `	int rc;` |
|     32727 | 4727 | `	if( nArg < 2 ){` |
|         - | 4728 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4729 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4730 | `		return PH7_OK;` |
|         - | 4731 | `	}` |
|     32727 | 4732 | `	pNeedle = apArg[0];` |
|     32727 | 4733 | `	bStrict = 0;` |
|     32727 | 4734 | `	if( nArg > 2 ){` |
|        43 | 4735 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        21 | 4736 | `	}` |
|     32727 | 4737 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4738 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4739 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4740 | `		/* Set the comparison result */` |
|       ! 0 | 4741 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4742 | `		return PH7_OK;` |
|         - | 4743 | `	}` |
|         - | 4744 | `	/* Perform the lookup */` |
|     32727 | 4745 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4746 | `	/* Lookup result */` |
|     32727 | 4747 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     32727 | 4748 | `	return PH7_OK;` |
|     16366 | 4749 | `}` |
|         - | 4750 | `/*` |
|         - | 4751 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4752 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4753 | ` * Parameters` |
|         - | 4754 | ` * $needle` |
|         - | 4755 | ` *   The searched value.` |
|         - | 4756 | ` * $haystack` |
|         - | 4757 | ` *   The array.` |
|         - | 4758 | ` * $strict` |
|         - | 4759 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4760 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4761 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4762 | ` * Return` |
|         - | 4763 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4764 | ` */` |
|        32 | 4765 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4766 | `{` |
|         - | 4767 | `	ph7_hashmap_node *pEntry;` |
|         - | 4768 | `	ph7_value *pVal,sNeedle;` |
|         - | 4769 | `	ph7_hashmap *pMap;` |
|         - | 4770 | `	ph7_value sVal;` |
|         - | 4771 | `	int bStrict;` |
|         - | 4772 | `	sxu32 n;` |
|         - | 4773 | `	int rc;` |
|        37 | 4774 | `	if( nArg < 2 ){` |
|         - | 4775 | `		/* Missing argument,throw ArgumentCountError */` |
|         8 | 4776 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4777 | `			"ArgumentCountError",` |
|         - | 4778 | `			"array_search() expects at least 2 arguments, %d given",` |
|         2 | 4779 | `			nArg` |
|         - | 4780 | `			);` |
|         - | 4781 | `	}` |
|        31 | 4782 | `	bStrict = FALSE;` |
|        31 | 4783 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4784 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4785 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4786 | `			"TypeError",` |
|         - | 4787 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4788 | `			ph7_type_name(apArg[1])` |
|         - | 4789 | `			);` |
|         - | 4790 | `	}` |
|        28 | 4791 | `	if( nArg > 2 ){` |
|         - | 4792 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        14 | 4793 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4794 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4795 | `				"TypeError",` |
|         - | 4796 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4797 | `				ph7_type_name(apArg[2])` |
|         - | 4798 | `				);` |
|         - | 4799 | `		}` |
|        11 | 4800 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4801 | `	}` |
|         - | 4802 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4803 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4804 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4805 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 4806 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 4807 | `	pEntry = pMap->pFirst;` |
|        25 | 4808 | `	n = pMap->nEntry;` |
|        28 | 4809 | `	for(;;){` |
|        57 | 4810 | `		if( !n ){` |
|         9 | 4811 | `			break;` |
|         - | 4812 | `		}` |
|         - | 4813 | `		/* Extract node value */` |
|        49 | 4814 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 4815 | `		if( pVal ){` |
|         - | 4816 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4817 | `			 * can change their type.` |
|         - | 4818 | `			 */` |
|        49 | 4819 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 4820 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 4821 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 4822 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 4823 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 4824 | `			if( rc == 0 ){` |
|         - | 4825 | `				/* Match found,return key */` |
|        17 | 4826 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4827 | `					/* INT key */` |
|        11 | 4828 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 4829 | `				}else{` |
|         7 | 4830 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4831 | `					/* Blob key */` |
|         7 | 4832 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4833 | `				}` |
|        17 | 4834 | `				return PH7_OK;` |
|         - | 4835 | `			}` |
|        16 | 4836 | `		}` |
|         - | 4837 | `		/* Point to the next entry */` |
|        33 | 4838 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 4839 | `		n--;` |
|         1 | 4840 | `	}` |
|         - | 4841 | `	/* No such value,return FALSE */` |
|         9 | 4842 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4843 | `	return PH7_OK;` |
|        21 | 4844 | `}` |
|         - | 4845 | `/*` |
|         - | 4846 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4847 | ` *  Computes the difference of arrays.` |
|         - | 4848 | ` * Parameters` |
|         - | 4849 | ` *  $array1` |
|         - | 4850 | ` *    The array to compare from` |
|         - | 4851 | ` *  $array2` |
|         - | 4852 | ` *    An array to compare against` |
|         - | 4853 | ` *  $...` |
|         - | 4854 | ` *   More arrays to compare against` |
|         - | 4855 | ` * Return` |
|         - | 4856 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4857 | ` *  are not present in any of the other arrays.` |
|         - | 4858 | ` */` |
|        22 | 4859 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4860 | `{` |
|         - | 4861 | `	ph7_hashmap_node *pEntry;` |
|         - | 4862 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4863 | `	ph7_value *pArray;` |
|         - | 4864 | `	ph7_value *pVal;` |
|         - | 4865 | `	sxi32 rc;` |
|         - | 4866 | `	sxu32 n;` |
|         - | 4867 | `	int i;` |
|         - | 4868 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 4869 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 4870 | `	 * debugging difficult. */` |
|        26 | 4871 | `	if( nArg < 1 ){` |
|         4 | 4872 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4873 | `			"ArgumentCountError",` |
|         - | 4874 | `			"array_diff() expects at least 1 argument, %d given",` |
|         1 | 4875 | `			nArg` |
|         - | 4876 | `			);` |
|         - | 4877 | `	}` |
|        23 | 4878 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4879 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4880 | `			"TypeError",` |
|         - | 4881 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4882 | `			ph7_type_name(apArg[0])` |
|         - | 4883 | `			);` |
|         - | 4884 | `	}` |
|        36 | 4885 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 4886 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 4887 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4888 | `				"TypeError",` |
|         - | 4889 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 4890 | `				i + 1,` |
|         2 | 4891 | `				ph7_type_name(apArg[i])` |
|         - | 4892 | `				);` |
|         - | 4893 | `		}` |
|         9 | 4894 | `	}` |
|        17 | 4895 | `	if( nArg == 1 ){` |
|         - | 4896 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 4897 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 4898 | `		return PH7_OK;` |
|         - | 4899 | `	}` |
|         - | 4900 | `	/* Create a new array */` |
|        15 | 4901 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 4902 | `	if( pArray == 0 ){` |
|       ! 0 | 4903 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4904 | `		return PH7_OK;` |
|         - | 4905 | `	}` |
|         - | 4906 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 4907 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4908 | `	/* Perform the diff */` |
|        15 | 4909 | `	pEntry = pSrc->pFirst;` |
|        15 | 4910 | `	n = pSrc->nEntry;` |
|        27 | 4911 | `	for(;;){` |
|        55 | 4912 | `		if( n < 1 ){` |
|        15 | 4913 | `			break;` |
|         - | 4914 | `		}` |
|         - | 4915 | `		/* Extract the node value */` |
|        41 | 4916 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 4917 | `		if( pVal ){` |
|        69 | 4918 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 4919 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 4920 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4921 | `				/* Perform the lookup */` |
|        45 | 4922 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 4923 | `				if( rc == SXRET_OK ){` |
|         - | 4924 | `					/* Value exist */` |
|        17 | 4925 | `					break;` |
|         - | 4926 | `				}` |
|        15 | 4927 | `			}` |
|        41 | 4928 | `			if( i >= nArg ){` |
|         - | 4929 | `				/* Perform the insertion */` |
|        25 | 4930 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 4931 | `			}` |
|        20 | 4932 | `		}` |
|         - | 4933 | `		/* Point to the next entry */` |
|        41 | 4934 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 4935 | `		n--;` |
|         1 | 4936 | `	}` |
|         - | 4937 | `	/* Return the freshly created array */` |
|        15 | 4938 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 4939 | `	return PH7_OK;` |
|        15 | 4940 | `}` |
|         - | 4941 | `/*` |
|         - | 4942 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 4943 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 4944 | ` * Parameters` |
|         - | 4945 | ` *  $array1` |
|         - | 4946 | ` *    The array to compare from` |
|         - | 4947 | ` *  $array2` |
|         - | 4948 | ` *    An array to compare against` |
|         - | 4949 | ` *  $...` |
|         - | 4950 | ` *   More arrays to compare against.` |
|         - | 4951 | ` * $callback` |
|         - | 4952 | ` *  The callback comparison function.` |
|         - | 4953 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 4954 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 4955 | ` *  than the second.` |
|         - | 4956 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 4957 | ` * Return` |
|         - | 4958 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4959 | ` *  are not present in any of the other arrays.` |
|         - | 4960 | ` */` |
|        22 | 4961 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4962 | `{` |
|         - | 4963 | `	ph7_hashmap_node *pEntry;` |
|         - | 4964 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4965 | `	ph7_value *pCallback;` |
|         - | 4966 | `	ph7_value *pArray;` |
|         - | 4967 | `	ph7_value *pVal;` |
|         - | 4968 | `	sxi32 rc;` |
|         - | 4969 | `	sxu32 n;` |
|         - | 4970 | `	int i;` |
|         - | 4971 |  |
|         - | 4972 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        27 | 4973 | `	if( nArg < 2 ){` |
|         4 | 4974 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4975 | `			"ArgumentCountError",` |
|         - | 4976 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|         1 | 4977 | `			nArg` |
|         - | 4978 | `			);` |
|         - | 4979 | `	}` |
|        25 | 4980 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4981 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4982 | `			"TypeError",` |
|         - | 4983 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4984 | `			ph7_type_name(apArg[0])` |
|         - | 4985 | `			);` |
|         - | 4986 | `	}` |
|         - | 4987 |  |
|        23 | 4988 | `	if( nArg == 2 ){` |
|         - | 4989 | `		/* Only the original array and the callback were provided. */` |
|         - | 4990 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 4991 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 4992 | `		 * validation order.` |
|         - | 4993 | `		 */` |
|         4 | 4994 | `	} else {` |
|         - | 4995 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 4996 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 4997 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 4998 | `				return PH7_VmThrowException(pCtx,` |
|         - | 4999 | `					"TypeError",` |
|         - | 5000 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5001 | `					i + 1,` |
|         6 | 5002 | `					ph7_type_name(apArg[i])` |
|         - | 5003 | `					);` |
|         - | 5004 | `			}` |
|         7 | 5005 | `		}` |
|         - | 5006 | `	}` |
|         - | 5007 |  |
|         - | 5008 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5009 | `	pCallback = apArg[nArg - 1];` |
|         - | 5010 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5011 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5012 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5013 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5014 | `				"TypeError",` |
|         - | 5015 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5016 | `				nArg` |
|         - | 5017 | `				);` |
|         - | 5018 | `		}` |
|         6 | 5019 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5020 | `			int len;` |
|         3 | 5021 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5022 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5023 | `				"TypeError",` |
|         - | 5024 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5025 | `				nArg,` |
|         1 | 5026 | `				zName` |
|         - | 5027 | `				);` |
|         - | 5028 | `		}` |
|         4 | 5029 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5030 | `			"TypeError",` |
|         - | 5031 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5032 | `			nArg` |
|         - | 5033 | `			);` |
|         - | 5034 | `	}` |
|         - | 5035 |  |
|         7 | 5036 | `	if( nArg == 2 ){` |
|         - | 5037 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5038 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5039 | `		return PH7_OK;` |
|         - | 5040 | `	}` |
|         - | 5041 |  |
|         - | 5042 | `	/* Create a new array */` |
|         5 | 5043 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5044 | `	if( pArray == 0 ){` |
|       ! 0 | 5045 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5046 | `		return PH7_OK;` |
|         - | 5047 | `	}` |
|         - | 5048 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5049 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5050 | `	/* Perform the diff */` |
|         5 | 5051 | `	pEntry = pSrc->pFirst;` |
|         5 | 5052 | `	n = pSrc->nEntry;` |
|         5 | 5053 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5054 | `	for(;;){` |
|        11 | 5055 | `		if( n < 1 ){` |
|         3 | 5056 | `			break;` |
|         - | 5057 | `		}` |
|         - | 5058 | `		/* Extract the node value */` |
|         9 | 5059 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5060 | `		if( pVal ){` |
|        15 | 5061 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5062 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5063 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5064 | `				/* Perform the lookup */` |
|         9 | 5065 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5066 | `				if( rc == SXRET_OK ){` |
|         - | 5067 | `					/* Value exist */` |
|         3 | 5068 | `					break;` |
|         - | 5069 | `				}` |
|         4 | 5070 | `			}` |
|         9 | 5071 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5072 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5073 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5074 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5075 | `				return PH7_EXCEPTION;` |
|         - | 5076 | `			}` |
|         7 | 5077 | `			if( i >= (nArg - 1)){` |
|         - | 5078 | `				/* Perform the insertion */` |
|         5 | 5079 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5080 | `			}` |
|         3 | 5081 | `		}` |
|         - | 5082 | `		/* Point to the next entry */` |
|         7 | 5083 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5084 | `		n--;` |
|         1 | 5085 | `	}` |
|         - | 5086 | `	/* Return the freshly created array */` |
|         3 | 5087 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5088 | `	return PH7_OK;` |
|        16 | 5089 | `}` |
|         - | 5090 | `/*` |
|         - | 5091 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5092 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5093 | ` * Parameters` |
|         - | 5094 | ` *  $array1` |
|         - | 5095 | ` *    The array to compare from` |
|         - | 5096 | ` *  $array2` |
|         - | 5097 | ` *    An array to compare against` |
|         - | 5098 | ` *  $...` |
|         - | 5099 | ` *   More arrays to compare against` |
|         - | 5100 | ` * Return` |
|         - | 5101 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5102 | ` *  are not present in any of the other arrays.` |
|         - | 5103 | ` */` |
|        22 | 5104 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5105 | `{` |
|         - | 5106 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5107 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5108 | `	ph7_value *pArray;` |
|         - | 5109 | `	ph7_value *pVal;` |
|         - | 5110 | `	sxi32 rc;` |
|         - | 5111 | `	sxu32 n;` |
|         - | 5112 | `	int i;` |
|         - | 5113 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5114 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5115 | `	 * accompanying integration tests to pass. */` |
|        27 | 5116 | `	if( nArg < 1 ){` |
|         4 | 5117 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5118 | `			"ArgumentCountError",` |
|         - | 5119 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|         1 | 5120 | `			nArg` |
|         - | 5121 | `			);` |
|         - | 5122 | `	}` |
|        24 | 5123 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5124 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5125 | `			"TypeError",` |
|         - | 5126 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5127 | `			ph7_type_name(apArg[0])` |
|         - | 5128 | `			);` |
|         - | 5129 | `	}` |
|        37 | 5130 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5131 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5132 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5133 | `				"TypeError",` |
|         - | 5134 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5135 | `				i + 1,` |
|         4 | 5136 | `				ph7_type_name(apArg[i])` |
|         - | 5137 | `				);` |
|         - | 5138 | `		}` |
|        10 | 5139 | `	}` |
|        15 | 5140 | `	if( nArg == 1 ){` |
|         - | 5141 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5142 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5143 | `		return PH7_OK;` |
|         - | 5144 | `	}` |
|         - | 5145 | `	/* Create a new array */` |
|        13 | 5146 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5147 | `	if( pArray == 0 ){` |
|       ! 0 | 5148 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5149 | `		return PH7_OK;` |
|         - | 5150 | `	}` |
|         - | 5151 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5152 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5153 | `	/* Perform the diff */` |
|        13 | 5154 | `	pEntry = pSrc->pFirst;` |
|        13 | 5155 | `	n = pSrc->nEntry;` |
|        13 | 5156 | `	pN1 = pN2 = 0;` |
|        34 | 5157 | `	for(;;){` |
|         - | 5158 | `		int keep;` |
|        41 | 5159 | `		if( n < 1 ){` |
|        13 | 5160 | `			break;` |
|         - | 5161 | `		}` |
|         - | 5162 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5163 | `		keep = 1;` |
|        47 | 5164 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5165 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5166 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5167 | `			/* Perform a key lookup first */` |
|        33 | 5168 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5169 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5170 | `			}else{` |
|        21 | 5171 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5172 | `			}` |
|        33 | 5173 | `			if( rc != SXRET_OK ){` |
|         - | 5174 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5175 | `				continue;` |
|         - | 5176 | `			}` |
|         - | 5177 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5178 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5179 | `			if( pVal ){` |
|         - | 5180 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5181 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5182 | `				if( pVal2 ){` |
|         - | 5183 | `					ph7_value sV1,sV2;` |
|         - | 5184 | `					sxi32 cmp;` |
|         - | 5185 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5186 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5187 | `					 * null element used to come back bool(false) in the` |
|         - | 5188 | `					 * caller's array). */` |
|        17 | 5189 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5190 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5191 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5192 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5193 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5194 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5195 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5196 | `					if( cmp == 0 ){` |
|         - | 5197 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5198 | `						keep = 0;` |
|        15 | 5199 | `						break;` |
|         - | 5200 | `					}` |
|         1 | 5201 | `				}` |
|         1 | 5202 | `			}` |
|         2 | 5203 | `		}` |
|        29 | 5204 | `		if( keep ){` |
|         - | 5205 | `			/* Perform the insertion */` |
|        15 | 5206 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5207 | `		}` |
|         - | 5208 | `		/* Point to the next entry */` |
|        29 | 5209 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5210 | `		n--;` |
|         1 | 5211 | `	}` |
|         - | 5212 | `	/* Return the freshly created array */` |
|        13 | 5213 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5214 | `	return PH7_OK;` |
|        16 | 5215 | `}` |
|         - | 5216 | `/*` |
|         - | 5217 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5218 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5219 | ` *  by a user supplied callback function.` |
|         - | 5220 | ` * Parameters` |
|         - | 5221 | ` *  $array1` |
|         - | 5222 | ` *    The array to compare from` |
|         - | 5223 | ` *  $array2` |
|         - | 5224 | ` *    An array to compare against` |
|         - | 5225 | ` *  $...` |
|         - | 5226 | ` *   More arrays to compare against.` |
|         - | 5227 | ` *  $key_compare_func` |
|         - | 5228 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5229 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5230 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5231 | ` * Return` |
|         - | 5232 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5233 | ` *  are not present in any of the other arrays.` |
|         - | 5234 | ` */` |
|        24 | 5235 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5236 | `{` |
|         - | 5237 | `	ph7_hashmap_node *pEntry;` |
|         - | 5238 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5239 | `	ph7_value *pCallback;` |
|         - | 5240 | `	ph7_value *pArray;` |
|         - | 5241 | `	sxi32 rc;` |
|         - | 5242 | `	sxu32 n;` |
|         - | 5243 | `	int i;` |
|         - | 5244 |  |
|         - | 5245 | `	/* Argument validation mimicking PHP errors. */` |
|        29 | 5246 | `	if( nArg < 2 ){` |
|         4 | 5247 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5248 | `			"ArgumentCountError",` |
|         - | 5249 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|         1 | 5250 | `			nArg` |
|         - | 5251 | `			);` |
|         - | 5252 | `	}` |
|        26 | 5253 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5254 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5255 | `			"TypeError",` |
|         - | 5256 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5257 | `			ph7_type_name(apArg[0])` |
|         - | 5258 | `			);` |
|         - | 5259 | `	}` |
|         - | 5260 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5261 | `	 * expected to be a callback. */` |
|        38 | 5262 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5263 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5264 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5265 | `				"TypeError",` |
|         - | 5266 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5267 | `				i + 1,` |
|         2 | 5268 | `				ph7_type_name(apArg[i])` |
|         - | 5269 | `				);` |
|         - | 5270 | `		}` |
|         9 | 5271 | `	}` |
|         - | 5272 | `	/* Point to the callback value */` |
|        22 | 5273 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5274 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5275 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5276 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5277 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5278 | `		 * string given" which we also reproduce. */` |
|         9 | 5279 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5280 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5281 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5282 | `				"TypeError",` |
|         - | 5283 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5284 | `				nArg` |
|         - | 5285 | `				);` |
|         - | 5286 | `		}` |
|         6 | 5287 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5288 | `			/* neither array nor string */` |
|         8 | 5289 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5290 | `				"TypeError",` |
|         - | 5291 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5292 | `				nArg` |
|         - | 5293 | `				);` |
|         - | 5294 | `		}` |
|         - | 5295 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5296 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5297 | `			"TypeError",` |
|         - | 5298 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5299 | `			nArg,` |
|       ! 0 | 5300 | `			ph7_type_name(pCallback)` |
|         - | 5301 | `			);` |
|         - | 5302 | `	}` |
|        13 | 5303 | `	if( nArg == 2 ){` |
|         - | 5304 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5305 | `		 * input array. */` |
|         3 | 5306 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5307 | `		return PH7_OK;` |
|         - | 5308 | `	}` |
|         - | 5309 | `	/* Create a new array */` |
|        11 | 5310 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5311 | `	if( pArray == 0 ){` |
|       ! 0 | 5312 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5313 | `		return PH7_OK;` |
|         - | 5314 | `	}` |
|         - | 5315 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5316 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5317 | `	/* Perform the diff */` |
|        11 | 5318 | `	pEntry = pSrc->pFirst;` |
|        11 | 5319 | `	n = pSrc->nEntry;` |
|        21 | 5320 | `	for(;;){` |
|         - | 5321 | `		int keep;` |
|        27 | 5322 | `		if( n < 1 ){` |
|         9 | 5323 | `			break;` |
|         - | 5324 | `		}` |
|        19 | 5325 | `		keep = 1;` |
|        31 | 5326 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5327 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5328 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5329 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5330 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5331 | `			while( pIt ){` |
|         - | 5332 | `				/* build temporary key values for callback */` |
|         - | 5333 | `				ph7_value key1, key2, result;` |
|         - | 5334 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5335 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5336 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5337 | `				}else{` |
|         - | 5338 | `					SyString sStr;` |
|        33 | 5339 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5340 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5341 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5342 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5343 | `				}` |
|        33 | 5344 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5345 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5346 | `				}else{` |
|         - | 5347 | `					SyString sStr;` |
|        33 | 5348 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5349 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5350 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5351 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5352 | `				}` |
|        33 | 5353 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5354 | `				/* call user callback with (key1, key2) */` |
|         - | 5355 | `				{` |
|         - | 5356 | `					ph7_value *apK[2];` |
|        33 | 5357 | `					apK[0] = &key1;` |
|        33 | 5358 | `					apK[1] = &key2;` |
|        33 | 5359 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5360 | `				}` |
|        33 | 5361 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5362 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5363 | `					 * array_uintersect (which signal back from` |
|         - | 5364 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5365 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5366 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5367 | `					PH7_MemObjRelease(&result);` |
|         3 | 5368 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5369 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5370 | `					return PH7_EXCEPTION;` |
|         - | 5371 | `				}` |
|        31 | 5372 | `				if( rc == SXRET_OK ){` |
|        31 | 5373 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5374 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5375 | `					}` |
|        31 | 5376 | `					if( result.x.iVal == 0 ){` |
|         - | 5377 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5378 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5379 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5380 | `						if( pVal1 && pVal2 ){` |
|         - | 5381 | `							ph7_value sV1,sV2;` |
|         - | 5382 | `							sxi32 cmp;` |
|         - | 5383 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5384 | `							 * place and these are LIVE array elements. */` |
|        13 | 5385 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5386 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5387 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5388 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5389 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5390 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5391 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5392 | `							if( cmp == 0 ){` |
|         9 | 5393 | `								keep = 0;` |
|         9 | 5394 | `								PH7_MemObjRelease(&result);` |
|         - | 5395 | `								/* release keys too before breaking */` |
|         9 | 5396 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5397 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5398 | `								break;` |
|         - | 5399 | `							}` |
|         2 | 5400 | `						}` |
|         2 | 5401 | `					}` |
|        11 | 5402 | `				}` |
|        23 | 5403 | `				PH7_MemObjRelease(&result);` |
|        23 | 5404 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5405 | `				PH7_MemObjRelease(&key2);` |
|         - | 5406 | `				/* move to next node */` |
|        23 | 5407 | `				pIt = pIt->pPrev;` |
|        23 | 5408 | `				if( keep == 0 ) break;` |
|         1 | 5409 | `			}` |
|        21 | 5410 | `			if( keep == 0 ) break;` |
|         7 | 5411 | `		}` |
|        17 | 5412 | `		if( keep ){` |
|         - | 5413 | `			/* Perform the insertion */` |
|         9 | 5414 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5415 | `		}` |
|         - | 5416 | `		/* Point to the next entry */` |
|        17 | 5417 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5418 | `		n--;` |
|         1 | 5419 | `	}` |
|         - | 5420 | `	/* Return the freshly created array */` |
|         9 | 5421 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5422 | `	return PH7_OK;` |
|        17 | 5423 | `}` |
|         - | 5424 | `/*` |
|         - | 5425 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5426 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5427 | ` * Parameters` |
|         - | 5428 | ` *  $array1` |
|         - | 5429 | ` *    The array to compare from` |
|         - | 5430 | ` *  $array2` |
|         - | 5431 | ` *    An array to compare against` |
|         - | 5432 | ` *  $...` |
|         - | 5433 | ` *   More arrays to compare against` |
|         - | 5434 | ` * Return` |
|         - | 5435 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5436 | ` *  in any of the other arrays.` |
|         - | 5437 | ` * Note that NULL is returned on failure.` |
|         - | 5438 | ` */` |
|        14 | 5439 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5440 | `{` |
|         - | 5441 | `	ph7_hashmap_node *pEntry;` |
|         - | 5442 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5443 | `	ph7_value *pArray;` |
|         - | 5444 | `	sxi32 rc;` |
|         - | 5445 | `	sxu32 n;` |
|         - | 5446 | `	int i;` |
|         - | 5447 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5448 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5449 | `	 * helpers. */` |
|        18 | 5450 | `	if( nArg < 1 ){` |
|         4 | 5451 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5452 | `			"ArgumentCountError",` |
|         - | 5453 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|         1 | 5454 | `			nArg` |
|         - | 5455 | `			);` |
|         - | 5456 | `	}` |
|        15 | 5457 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5458 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5459 | `			"TypeError",` |
|         - | 5460 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5461 | `			ph7_type_name(apArg[0])` |
|         - | 5462 | `			);` |
|         - | 5463 | `	}` |
|        20 | 5464 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5465 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5466 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5467 | `				"TypeError",` |
|         - | 5468 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5469 | `				i + 1,` |
|         2 | 5470 | `				ph7_type_name(apArg[i])` |
|         - | 5471 | `				);` |
|         - | 5472 | `		}` |
|         5 | 5473 | `	}` |
|         9 | 5474 | `	if( nArg == 1 ){` |
|         - | 5475 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5476 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5477 | `		return PH7_OK;` |
|         - | 5478 | `	}` |
|         - | 5479 | `	/* Create a new array */` |
|         7 | 5480 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5481 | `	if( pArray == 0 ){` |
|       ! 0 | 5482 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5483 | `		return PH7_OK;` |
|         - | 5484 | `	}` |
|         - | 5485 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5486 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5487 | `	/* Perfrom the diff */` |
|         7 | 5488 | `	pEntry = pSrc->pFirst;` |
|         7 | 5489 | `	n = pSrc->nEntry;` |
|        12 | 5490 | `	for(;;){` |
|        25 | 5491 | `		if( n < 1 ){` |
|         7 | 5492 | `			break;` |
|         - | 5493 | `		}` |
|        31 | 5494 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5495 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5496 | `				/* ignore */` |
|       ! 0 | 5497 | `				continue;` |
|         - | 5498 | `			}` |
|        23 | 5499 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5500 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5501 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5502 | `				/* Blob lookup */` |
|        17 | 5503 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5504 | `			}else{` |
|         - | 5505 | `				/* Int lookup */` |
|         7 | 5506 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5507 | `			}` |
|        23 | 5508 | `			if( rc == SXRET_OK ){` |
|         - | 5509 | `				/* Key exists,break immediately */` |
|        11 | 5510 | `				break;` |
|         - | 5511 | `			}` |
|         7 | 5512 | `		}` |
|        19 | 5513 | `		if( i >= nArg ){` |
|         - | 5514 | `			/* Perform the insertion */` |
|         9 | 5515 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5516 | `		}` |
|         - | 5517 | `		/* Point to the next entry */` |
|        19 | 5518 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5519 | `		n--;` |
|         1 | 5520 | `	}` |
|         - | 5521 | `	/* Return the freshly created array */` |
|         7 | 5522 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5523 | `	return PH7_OK;` |
|        11 | 5524 | `}` |
|         - | 5525 | `/*` |
|         - | 5526 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5527 | ` *  Computes the intersection of arrays.` |
|         - | 5528 | ` * Parameters` |
|         - | 5529 | ` *  $array1` |
|         - | 5530 | ` *    The array to compare from` |
|         - | 5531 | ` *  $array2` |
|         - | 5532 | ` *    An array to compare against` |
|         - | 5533 | ` *  $...` |
|         - | 5534 | ` *   More arrays to compare against` |
|         - | 5535 | ` * Return` |
|         - | 5536 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5537 | ` *  in all of the parameters.` |
|         - | 5538 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5539 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5540 | ` */` |
|        22 | 5541 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5542 | `{` |
|         - | 5543 | `	ph7_hashmap_node *pEntry;` |
|         - | 5544 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5545 | `	ph7_value *pArray;` |
|         - | 5546 | `	ph7_value *pVal;` |
|         - | 5547 | `	sxi32 rc;` |
|         - | 5548 | `	sxu32 n;` |
|         - | 5549 | `	int i;` |
|        26 | 5550 | `	if( nArg < 1 ){` |
|         4 | 5551 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5552 | `			"ArgumentCountError",` |
|         - | 5553 | `			"array_intersect() expects at least 1 argument, %d given",` |
|         1 | 5554 | `			nArg` |
|         - | 5555 | `			);` |
|         - | 5556 | `	}` |
|        23 | 5557 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5558 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5559 | `			"TypeError",` |
|         - | 5560 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5561 | `			ph7_type_name(apArg[0])` |
|         - | 5562 | `			);` |
|         - | 5563 | `	}` |
|        36 | 5564 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5565 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5566 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5567 | `				"TypeError",` |
|         - | 5568 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5569 | `				i + 1,` |
|         2 | 5570 | `				ph7_type_name(apArg[i])` |
|         - | 5571 | `				);` |
|         - | 5572 | `		}` |
|         9 | 5573 | `	}` |
|        17 | 5574 | `	if( nArg == 1 ){` |
|         - | 5575 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5576 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5577 | `		return PH7_OK;` |
|         - | 5578 | `	}` |
|         - | 5579 | `	/* Create a new array */` |
|        15 | 5580 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5581 | `	if( pArray == 0 ){` |
|       ! 0 | 5582 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5583 | `		return PH7_OK;` |
|         - | 5584 | `	}` |
|         - | 5585 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5586 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5587 | `	/* Perform the intersection */` |
|        15 | 5588 | `	pEntry = pSrc->pFirst;` |
|        15 | 5589 | `	n = pSrc->nEntry;` |
|        31 | 5590 | `	for(;;){` |
|        63 | 5591 | `		if( n < 1 ){` |
|        15 | 5592 | `			break;` |
|         - | 5593 | `		}` |
|         - | 5594 | `		/* Extract the node value */` |
|        49 | 5595 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5596 | `		if( pVal ){` |
|        79 | 5597 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5598 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5599 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5600 | `				/* Perform the lookup */` |
|        55 | 5601 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5602 | `				if( rc != SXRET_OK ){` |
|         - | 5603 | `					/* Value does not exist */` |
|        25 | 5604 | `					break;` |
|         - | 5605 | `				}` |
|        16 | 5606 | `			}` |
|        49 | 5607 | `			if( i >= nArg ){` |
|         - | 5608 | `				/* Perform the insertion */` |
|        25 | 5609 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5610 | `			}` |
|        24 | 5611 | `		}` |
|         - | 5612 | `		/* Point to the next entry */` |
|        49 | 5613 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5614 | `		n--;` |
|         1 | 5615 | `	}` |
|         - | 5616 | `	/* Return the freshly created array */` |
|        15 | 5617 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5618 | `	return PH7_OK;` |
|        15 | 5619 | `}` |
|         - | 5620 | `/*` |
|         - | 5621 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5622 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5623 | ` * Parameters` |
|         - | 5624 | ` *  $array1` |
|         - | 5625 | ` *    The array to compare from` |
|         - | 5626 | ` *  $array2` |
|         - | 5627 | ` *    An array to compare against` |
|         - | 5628 | ` *  $...` |
|         - | 5629 | ` *   More arrays to compare against` |
|         - | 5630 | ` * Return` |
|         - | 5631 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5632 | ` *  in all the arguments, with matching keys.` |
|         - | 5633 | ` */` |
|        22 | 5634 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5635 | `{` |
|         - | 5636 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5637 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5638 | `	ph7_value *pArray;` |
|         - | 5639 | `	ph7_value *pVal;` |
|         - | 5640 | `	sxi32 rc;` |
|         - | 5641 | `	sxu32 n;` |
|         - | 5642 | `	int i;` |
|        26 | 5643 | `	if( nArg < 1 ){` |
|         4 | 5644 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5645 | `			"ArgumentCountError",` |
|         - | 5646 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|         1 | 5647 | `			nArg` |
|         - | 5648 | `			);` |
|         - | 5649 | `	}` |
|        23 | 5650 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5651 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5652 | `			"TypeError",` |
|         - | 5653 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5654 | `			ph7_type_name(apArg[0])` |
|         - | 5655 | `			);` |
|         - | 5656 | `	}` |
|        36 | 5657 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5658 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5659 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5660 | `				"TypeError",` |
|         - | 5661 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5662 | `				i + 1,` |
|         2 | 5663 | `				ph7_type_name(apArg[i])` |
|         - | 5664 | `				);` |
|         - | 5665 | `		}` |
|         9 | 5666 | `	}` |
|        17 | 5667 | `	if( nArg == 1 ){` |
|         - | 5668 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5669 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5670 | `		return PH7_OK;` |
|         - | 5671 | `	}` |
|         - | 5672 | `	/* Create a new array */` |
|        15 | 5673 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5674 | `	if( pArray == 0 ){` |
|       ! 0 | 5675 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5676 | `		return PH7_OK;` |
|         - | 5677 | `	}` |
|         - | 5678 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5679 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5680 | `	/* Perform the intersection */` |
|        15 | 5681 | `	pEntry = pSrc->pFirst;` |
|        15 | 5682 | `	n = pSrc->nEntry;` |
|        15 | 5683 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5684 | `	for(;;){` |
|        47 | 5685 | `		if( n < 1 ){` |
|        15 | 5686 | `			break;` |
|         - | 5687 | `		}` |
|         - | 5688 | `		/* Extract the node value */` |
|        33 | 5689 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5690 | `		if( pVal ){` |
|        53 | 5691 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5692 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5693 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5694 | `				/* Perform a key lookup first */` |
|        37 | 5695 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5696 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5697 | `				}else{` |
|        23 | 5698 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5699 | `				}` |
|        37 | 5700 | `				if( rc != SXRET_OK ){` |
|         - | 5701 | `					/* No such key,break immediately */` |
|         7 | 5702 | `					break;` |
|         - | 5703 | `				}` |
|         - | 5704 | `				/* Perform the lookup */` |
|        31 | 5705 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5706 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5707 | `					/* Value does not exist */` |
|         6 | 5708 | `					break;` |
|         - | 5709 | `				}` |
|        11 | 5710 | `			}` |
|        33 | 5711 | `			if( i >= nArg ){` |
|         - | 5712 | `				/* Perform the insertion */` |
|        17 | 5713 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5714 | `			}` |
|        16 | 5715 | `		}` |
|         - | 5716 | `		/* Point to the next entry */` |
|        33 | 5717 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5718 | `		n--;` |
|         1 | 5719 | `	}` |
|         - | 5720 | `	/* Return the freshly created array */` |
|        15 | 5721 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5722 | `	return PH7_OK;` |
|        15 | 5723 | `}` |
|         - | 5724 | `/*` |
|         - | 5725 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5726 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5727 | ` * Parameters` |
|         - | 5728 | ` *  $array1` |
|         - | 5729 | ` *    The array to compare from` |
|         - | 5730 | ` *  $...` |
|         - | 5731 | ` *   More arrays to compare against` |
|         - | 5732 | ` * Return` |
|         - | 5733 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5734 | ` *  have keys that are present in all arguments.` |
|         - | 5735 | ` * Note that NULL is returned on failure.` |
|         - | 5736 | ` */` |
|        22 | 5737 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5738 | `{` |
|         - | 5739 | `	ph7_hashmap_node *pEntry;` |
|         - | 5740 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5741 | `	ph7_value *pArray;` |
|         - | 5742 | `	sxi32 rc;` |
|         - | 5743 | `	sxu32 n;` |
|         - | 5744 | `	int i;` |
|        26 | 5745 | `	if( nArg < 1 ){` |
|         4 | 5746 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5747 | `			"ArgumentCountError",` |
|         - | 5748 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|         1 | 5749 | `			nArg` |
|         - | 5750 | `			);` |
|         - | 5751 | `	}` |
|        23 | 5752 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5753 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5754 | `			"TypeError",` |
|         - | 5755 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5756 | `			ph7_type_name(apArg[0])` |
|         - | 5757 | `			);` |
|         - | 5758 | `	}` |
|        36 | 5759 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5760 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5761 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5762 | `				"TypeError",` |
|         - | 5763 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5764 | `				i + 1,` |
|         2 | 5765 | `				ph7_type_name(apArg[i])` |
|         - | 5766 | `				);` |
|         - | 5767 | `		}` |
|         9 | 5768 | `	}` |
|        17 | 5769 | `	if( nArg == 1 ){` |
|         - | 5770 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5771 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5772 | `		return PH7_OK;` |
|         - | 5773 | `	}` |
|         - | 5774 | `	/* Create a new array */` |
|        15 | 5775 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5776 | `	if( pArray == 0 ){` |
|       ! 0 | 5777 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5778 | `		return PH7_OK;` |
|         - | 5779 | `	}` |
|         - | 5780 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5781 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5782 | `	/* Perform the intersection */` |
|        15 | 5783 | `	pEntry = pSrc->pFirst;` |
|        15 | 5784 | `	n = pSrc->nEntry;` |
|        24 | 5785 | `	for(;;){` |
|        49 | 5786 | `		if( n < 1 ){` |
|        15 | 5787 | `			break;` |
|         - | 5788 | `		}` |
|        57 | 5789 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5790 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5791 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5792 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5793 | `				/* Blob lookup */` |
|        27 | 5794 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5795 | `			}else{` |
|         - | 5796 | `				/* Int key */` |
|        13 | 5797 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5798 | `			}` |
|        39 | 5799 | `			if( rc != SXRET_OK ){` |
|         - | 5800 | `				/* Key does not exist, break immediately */` |
|        17 | 5801 | `				break;` |
|         - | 5802 | `			}` |
|        12 | 5803 | `		}` |
|        35 | 5804 | `		if( i >= nArg ){` |
|         - | 5805 | `			/* Perform the insertion */` |
|        19 | 5806 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5807 | `		}` |
|         - | 5808 | `		/* Point to the next entry */` |
|        35 | 5809 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5810 | `		n--;` |
|         1 | 5811 | `	}` |
|         - | 5812 | `	/* Return the freshly created array */` |
|        15 | 5813 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5814 | `	return PH7_OK;` |
|        15 | 5815 | `}` |
|         - | 5816 | `/*` |
|         - | 5817 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5818 | ` *  Computes the intersection of arrays.` |
|         - | 5819 | ` * Parameters` |
|         - | 5820 | ` *  $array1` |
|         - | 5821 | ` *    The array to compare from` |
|         - | 5822 | ` *  $array2` |
|         - | 5823 | ` *    An array to compare against` |
|         - | 5824 | ` *  $...` |
|         - | 5825 | ` *   More arrays to compare against` |
|         - | 5826 | ` * $callback` |
|         - | 5827 | ` *  The callback comparison function.` |
|         - | 5828 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5829 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5830 | ` *  than the second.` |
|         - | 5831 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5832 | ` * Return` |
|         - | 5833 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5834 | ` *  in all of the parameters. .` |
|         - | 5835 | ` * Note that NULL is returned on failure.` |
|         - | 5836 | ` */` |
|        26 | 5837 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5838 | `{` |
|         - | 5839 | `	ph7_hashmap_node *pEntry;` |
|         - | 5840 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5841 | `	ph7_value *pCallback;` |
|         - | 5842 | `	ph7_value *pArray;` |
|         - | 5843 | `	ph7_value *pVal;` |
|         - | 5844 | `	sxi32 rc;` |
|         - | 5845 | `	sxu32 n;` |
|         - | 5846 | `	int i;` |
|         - | 5847 |  |
|         - | 5848 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        31 | 5849 | `	if( nArg < 2 ){` |
|         4 | 5850 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5851 | `			"ArgumentCountError",` |
|         - | 5852 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|         1 | 5853 | `			nArg` |
|         - | 5854 | `			);` |
|         - | 5855 | `	}` |
|        29 | 5856 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5857 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5858 | `			"TypeError",` |
|         - | 5859 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5860 | `			ph7_type_name(apArg[0])` |
|         - | 5861 | `			);` |
|         - | 5862 | `	}` |
|         - | 5863 |  |
|        27 | 5864 | `	if( nArg == 2 ){` |
|         - | 5865 | `		/* Only the original array and the callback were provided. */` |
|         - | 5866 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 5867 | `		 * validation ordering. */` |
|         3 | 5868 | `	} else {` |
|         - | 5869 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 5870 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 5871 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5872 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5873 | `					"TypeError",` |
|         - | 5874 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5875 | `					i + 1,` |
|         2 | 5876 | `					ph7_type_name(apArg[i])` |
|         - | 5877 | `					);` |
|         - | 5878 | `			}` |
|        13 | 5879 | `		}` |
|         - | 5880 | `	}` |
|         - | 5881 |  |
|         - | 5882 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 5883 | `	pCallback = apArg[nArg - 1];` |
|         - | 5884 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 5885 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 5886 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5887 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 5888 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 5889 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 5890 | `			 */` |
|         9 | 5891 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 5892 | `			if( pCb->nEntry != 2 ){` |
|         4 | 5893 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5894 | `					"TypeError",` |
|         - | 5895 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5896 | `					nArg` |
|         - | 5897 | `					);` |
|         - | 5898 | `			}` |
|         - | 5899 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 5900 | `			{` |
|         6 | 5901 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 5902 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 5903 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 5904 | `					int nMethodLen;` |
|         6 | 5905 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 5906 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 5907 | `					if( pClass ){` |
|         - | 5908 | `						/* Class exists but method is missing. */` |
|         4 | 5909 | `						return PH7_VmThrowException(pCtx,` |
|         - | 5910 | `							"TypeError",` |
|         - | 5911 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 5912 | `							nArg,` |
|         1 | 5913 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 5914 | `							zMethod` |
|         - | 5915 | `							);` |
|         - | 5916 | `					}` |
|         - | 5917 | `					/* Class not found */` |
|         - | 5918 | `					{` |
|         - | 5919 | `						int nName;` |
|         3 | 5920 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 5921 | `						return PH7_VmThrowException(pCtx,` |
|         - | 5922 | `							"TypeError",` |
|         - | 5923 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 5924 | `							nArg,` |
|         1 | 5925 | `							zName` |
|         - | 5926 | `							);` |
|         - | 5927 | `					}` |
|         - | 5928 | `				}` |
|         - | 5929 | `			}` |
|         - | 5930 | `			/* Fallback message */` |
|       ! 0 | 5931 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5932 | `				"TypeError",` |
|         - | 5933 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 5934 | `				nArg` |
|         - | 5935 | `				);` |
|         - | 5936 | `		}` |
|         6 | 5937 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5938 | `			int len;` |
|         3 | 5939 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5940 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5941 | `				"TypeError",` |
|         - | 5942 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5943 | `				nArg,` |
|         1 | 5944 | `				zName` |
|         - | 5945 | `				);` |
|         - | 5946 | `		}` |
|         4 | 5947 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5948 | `			"TypeError",` |
|         - | 5949 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5950 | `			nArg` |
|         - | 5951 | `			);` |
|         - | 5952 | `	}` |
|         - | 5953 |  |
|        11 | 5954 | `	if( nArg == 2 ){` |
|         - | 5955 | `		/* Only the original array and the callback were provided. */` |
|         5 | 5956 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 5957 | `		return PH7_OK;` |
|         - | 5958 | `	}` |
|         - | 5959 |  |
|         - | 5960 | `	/* Create a new array */` |
|         7 | 5961 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5962 | `	if( pArray == 0 ){` |
|       ! 0 | 5963 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5964 | `		return PH7_OK;` |
|         - | 5965 | `	}` |
|         - | 5966 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 5967 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5968 | `	/* Perform the intersection */` |
|         7 | 5969 | `	pEntry = pSrc->pFirst;` |
|         7 | 5970 | `	n = pSrc->nEntry;` |
|         7 | 5971 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 5972 | `	for(;;){` |
|        19 | 5973 | `		if( n < 1 ){` |
|         5 | 5974 | `			break;` |
|         - | 5975 | `		}` |
|         - | 5976 | `		/* Extract the node value */` |
|        15 | 5977 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 5978 | `		if( pVal ){` |
|        23 | 5979 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 5980 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5981 | `					/* ignore */` |
|       ! 0 | 5982 | `					continue;` |
|         - | 5983 | `				}` |
|         - | 5984 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 5985 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5986 | `				/* Perform the lookup */` |
|        15 | 5987 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 5988 | `				if( rc != SXRET_OK ){` |
|         - | 5989 | `					/* Value does not exist */` |
|         7 | 5990 | `					break;` |
|         - | 5991 | `				}` |
|         5 | 5992 | `			}` |
|        15 | 5993 | `			if( i >= (nArg-1) ){` |
|         - | 5994 | `				/* Perform the insertion */` |
|         9 | 5995 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5996 | `			}` |
|         7 | 5997 | `		}` |
|        15 | 5998 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5999 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6000 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6001 | `			return PH7_EXCEPTION;` |
|         - | 6002 | `		}` |
|         - | 6003 | `		/* Point to the next entry */` |
|        13 | 6004 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6005 | `		n--;` |
|         1 | 6006 | `	}` |
|         - | 6007 | `	/* Return the freshly created array */` |
|         5 | 6008 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6009 | `	return PH7_OK;` |
|        18 | 6010 | `}` |
|         - | 6011 | `/*` |
|         - | 6012 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6013 | ` *  Fill an array with values.` |
|         - | 6014 | ` * Parameters` |
|         - | 6015 | ` *  $start_index` |
|         - | 6016 | ` *    The first index of the returned array.` |
|         - | 6017 | ` *  $num` |
|         - | 6018 | ` *   Number of elements to insert.` |
|         - | 6019 | ` *  $value` |
|         - | 6020 | ` *    Value to use for filling.` |
|         - | 6021 | ` * Return` |
|         - | 6022 | ` *  The filled array or null on failure.` |
|         - | 6023 | ` */` |
|       244 | 6024 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6025 | `{` |
|         - | 6026 | `	ph7_value *pArray;` |
|         - | 6027 | `	int i,nEntry;` |
|         - | 6028 |  |
|         - | 6029 | `	/* PHP enforces argument count and type checks. */` |
|       249 | 6030 | `	if( nArg != 3 ){` |
|         - | 6031 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         8 | 6032 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6033 | `			"ArgumentCountError",` |
|         - | 6034 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         2 | 6035 | `			nArg` |
|         - | 6036 | `			);` |
|         - | 6037 | `	}` |
|         - | 6038 |  |
|         - | 6039 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6040 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6041 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6042 | `	 * and NULLs are rejected outright. */` |
|       359 | 6043 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       362 | 6044 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|         4 | 6045 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6046 | `			"TypeError",` |
|         - | 6047 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|         1 | 6048 | `			ph7_type_name(apArg[0])` |
|         - | 6049 | `			);` |
|         - | 6050 | `	}` |
|       242 | 6051 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6052 | `		int len;` |
|         8 | 6053 | `		sxu8 bReal = FALSE;` |
|         8 | 6054 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6055 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6056 | `			/* Non‑numeric string is an error. */` |
|         3 | 6057 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6058 | `				"TypeError",` |
|         - | 6059 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6060 | `				);` |
|         - | 6061 | `		}` |
|         5 | 6062 | `		if( bReal ){` |
|         - | 6063 | `			/* float-string -> deprecation warning */` |
|         4 | 6064 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6065 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6066 | `				zStr` |
|         - | 6067 | `				);` |
|         1 | 6068 | `		}` |
|         2 | 6069 | `	}` |
|         - | 6070 |  |
|         - | 6071 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6072 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6073 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6074 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6075 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6076 | `			"TypeError",` |
|         - | 6077 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6078 | `			ph7_type_name(apArg[1])` |
|         - | 6079 | `			);` |
|         - | 6080 | `	}` |
|       239 | 6081 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6082 | `		int len;` |
|         3 | 6083 | `		sxu8 bReal = FALSE;` |
|         3 | 6084 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6085 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6086 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6087 | `				"TypeError",` |
|         - | 6088 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6089 | `				);` |
|         - | 6090 | `		}` |
|       ! 0 | 6091 | `	}` |
|         - | 6092 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6093 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6094 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6095 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6096 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6097 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6098 | `		if( d != (double)i64 ){` |
|         7 | 6099 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6100 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6101 | `				d` |
|         - | 6102 | `				);` |
|         2 | 6103 | `		}` |
|         2 | 6104 | `	}` |
|         - | 6105 |  |
|         - | 6106 | `	/* Total number of entries to insert */` |
|       236 | 6107 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6108 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6109 | `	if( nEntry < 0 ){` |
|         3 | 6110 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6111 | `			"ValueError",` |
|         - | 6112 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6113 | `			);` |
|         - | 6114 | `	}` |
|         - | 6115 |  |
|         - | 6116 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6117 | `	if( nEntry == 0 ){` |
|         7 | 6118 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6119 | `		return PH7_OK;` |
|         - | 6120 | `	}` |
|         - | 6121 |  |
|         - | 6122 | `	/* Create a new array */` |
|       227 | 6123 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6124 | `	if( pArray == 0 ){` |
|       ! 0 | 6125 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6126 | `	}` |
|         - | 6127 |  |
|         - | 6128 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6129 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6130 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6131 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6132 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6133 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6134 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6135 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6136 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6137 | `		}` |
|   1058803 | 6138 | `	}` |
|         - | 6139 | `	/* Return the filled array */` |
|       227 | 6140 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6141 | `	return PH7_OK;` |
|       127 | 6142 | `}` |
|         - | 6143 | `/*` |
|         - | 6144 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6145 | ` *  Fill an array with values, specifying keys.` |
|         - | 6146 | ` * Parameters` |
|         - | 6147 | ` *  $input` |
|         - | 6148 | ` *   Array of values that will be used as key.` |
|         - | 6149 | ` *  $value` |
|         - | 6150 | ` *    Value to use for filling.` |
|         - | 6151 | ` * Return` |
|         - | 6152 | ` *  The filled array.` |
|         - | 6153 | ` * Throws` |
|         - | 6154 | ` *  ValueError if $input is not an array.` |
|         - | 6155 | ` */` |
|        26 | 6156 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6157 | `{` |
|         - | 6158 | `	ph7_hashmap_node *pEntry;` |
|         - | 6159 | `	ph7_hashmap *pSrc;` |
|         - | 6160 | `	ph7_value *pArray;` |
|         - | 6161 | `	sxu32 n;` |
|         - | 6162 | `	/* PHP enforces exactly 2 arguments. */` |
|        31 | 6163 | `	if( nArg != 2 ){` |
|        12 | 6164 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6165 | `			"ArgumentCountError",` |
|         - | 6166 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         3 | 6167 | `			nArg` |
|         - | 6168 | `			);` |
|         - | 6169 | `	}` |
|         - | 6170 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6171 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6172 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6173 | `			"TypeError",` |
|         - | 6174 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6175 | `			ph7_type_name(apArg[0])` |
|         - | 6176 | `			);` |
|         - | 6177 | `	}` |
|         - | 6178 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6179 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6180 | `	/* Create a new array */` |
|        17 | 6181 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6182 | `	if( pArray == 0 ){` |
|       ! 0 | 6183 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6184 | `		return PH7_OK;` |
|         - | 6185 | `	}` |
|         - | 6186 | `	/* Perform the requested operation */` |
|        17 | 6187 | `	pEntry = pSrc->pFirst;` |
|        45 | 6188 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6189 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6190 | `		/* Point to the next entry */` |
|        29 | 6191 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6192 | `	}` |
|         - | 6193 | `	/* Return the filled array */` |
|        17 | 6194 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6195 | `	return PH7_OK;` |
|        18 | 6196 | `}` |
|         - | 6197 | `/*` |
|         - | 6198 | ` * array array_combine(array $keys,array $values)` |
|         - | 6199 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6200 | ` * Parameters` |
|         - | 6201 | ` *  $keys` |
|         - | 6202 | ` *    Array of keys to be used.` |
|         - | 6203 | ` * $values` |
|         - | 6204 | ` *   Array of values to be used.` |
|         - | 6205 | ` * Return` |
|         - | 6206 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6207 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6208 | ` *  not an array.` |
|         - | 6209 | ` */` |
|        18 | 6210 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6211 | `{` |
|         - | 6212 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6213 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6214 | `	ph7_value *pArray;` |
|         - | 6215 | `	sxu32 n;` |
|         - | 6216 | `	/* PHP enforces argument count and type checks. */` |
|        23 | 6217 | `	if( nArg != 2 ){` |
|         - | 6218 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6219 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6220 | `			"ArgumentCountError",` |
|         - | 6221 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|         1 | 6222 | `			nArg` |
|         - | 6223 | `			);` |
|         - | 6224 | `	}` |
|         - | 6225 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6226 | `	 * argument index in the error message. */` |
|        20 | 6227 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6228 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6229 | `			"TypeError",` |
|         - | 6230 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6231 | `			ph7_type_name(apArg[0])` |
|         - | 6232 | `			);` |
|         - | 6233 | `	}` |
|        17 | 6234 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6235 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6236 | `			"TypeError",` |
|         - | 6237 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6238 | `			ph7_type_name(apArg[1])` |
|         - | 6239 | `			);` |
|         - | 6240 | `	}` |
|         - | 6241 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6242 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6243 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6244 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6245 | `		/* Length mismatch -> ValueError */` |
|         3 | 6246 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6247 | `			"ValueError",` |
|         - | 6248 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6249 | `			);` |
|         - | 6250 | `	}` |
|         - | 6251 | `	/* Create a new array */` |
|        11 | 6252 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6253 | `	if( pArray == 0 ){` |
|       ! 0 | 6254 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6255 | `		return PH7_OK;` |
|         - | 6256 | `	}` |
|         - | 6257 | `	/* Perform the requested operation */` |
|        11 | 6258 | `	pKe = pKey->pFirst;` |
|        11 | 6259 | `	pVe = pValue->pFirst;` |
|        33 | 6260 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6261 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6262 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6263 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6264 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6265 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6266 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6267 | `		 * original array must not be mutated. */` |
|        23 | 6268 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6269 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6270 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6271 | `			if( pTmpKey ){` |
|         5 | 6272 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6273 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6274 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6275 | `				pKeyCopy = pTmpKey;` |
|         2 | 6276 | `			}` |
|         2 | 6277 | `		}` |
|        23 | 6278 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6279 | `		/* Point to the next entry */` |
|        23 | 6280 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6281 | `		pVe = pVe->pPrev;` |
|        12 | 6282 | `	}` |
|         - | 6283 | `	/* Return the filled array */` |
|        11 | 6284 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6285 | `	return PH7_OK;` |
|        14 | 6286 | `}` |
|         - | 6287 | `/*` |
|         - | 6288 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6289 | ` *  Return an array with elements in reverse order.` |
|         - | 6290 | ` * Parameters` |
|         - | 6291 | ` *  $array` |
|         - | 6292 | ` *   The input array.` |
|         - | 6293 | ` *  $preserve_keys (optional)` |
|         - | 6294 | ` *   If set to TRUE keys are preserved.` |
|         - | 6295 | ` * Return` |
|         - | 6296 | ` *  The reversed array.` |
|         - | 6297 | ` */` |
|        20 | 6298 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6299 | `{` |
|         - | 6300 | `	ph7_hashmap_node *pEntry;` |
|         - | 6301 | `	ph7_hashmap *pSrc;` |
|         - | 6302 | `	ph7_value *pArray;` |
|         - | 6303 | `	int bPreserve;` |
|         - | 6304 | `	sxu32 n;` |
|        23 | 6305 | `	if( nArg < 1 ){` |
|         4 | 6306 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6307 | `			"ArgumentCountError",` |
|         - | 6308 | `			"array_reverse() expects at least 1 argument, %d given",` |
|         1 | 6309 | `			nArg` |
|         - | 6310 | `			);` |
|         - | 6311 | `	}` |
|         - | 6312 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6313 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6314 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6315 | `			"TypeError",` |
|         - | 6316 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6317 | `			ph7_type_name(apArg[0])` |
|         - | 6318 | `			);` |
|         - | 6319 | `	}` |
|        17 | 6320 | `	bPreserve = FALSE;` |
|        17 | 6321 | `	if( nArg > 1 ){` |
|         7 | 6322 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6323 | `	}` |
|         - | 6324 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6325 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6326 | `	/* Create a new array */` |
|        17 | 6327 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6328 | `	if( pArray == 0 ){` |
|       ! 0 | 6329 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6330 | `		return PH7_OK;` |
|         - | 6331 | `	}` |
|         - | 6332 | `	/* Perform the requested operation */` |
|        17 | 6333 | `	pEntry = pSrc->pLast;` |
|        55 | 6334 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6335 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6336 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6337 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6338 | `		/* Point to the previous entry */` |
|        39 | 6339 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6340 | `	}` |
|        17 | 6341 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6342 | `	return PH7_OK;` |
|        13 | 6343 | `}` |
|         - | 6344 | `/*` |
|         - | 6345 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6346 | ` *  Removes duplicate values from an array.` |
|         - | 6347 | ` * Parameters` |
|         - | 6348 | ` *  $array` |
|         - | 6349 | ` *   The input array.` |
|         - | 6350 | ` *  $flags` |
|         - | 6351 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6352 | ` *   behavior using these values:` |
|         - | 6353 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6354 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6355 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6356 | ` * Return` |
|         - | 6357 | ` *  The filtered array.` |
|         - | 6358 | ` */` |
|        24 | 6359 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6360 | `{` |
|         - | 6361 | `	ph7_hashmap_node *pEntry;` |
|         - | 6362 | `	ph7_value *pNeedle;` |
|         - | 6363 | `	ph7_hashmap *pSrc;` |
|         - | 6364 | `	ph7_value *pArray;` |
|         - | 6365 | `	int bStrict;` |
|         - | 6366 | `	sxi32 rc;` |
|         - | 6367 | `	sxu32 n;` |
|        28 | 6368 | `	if( nArg < 1 ){` |
|         - | 6369 | `		/* Missing arguments, throw ArgumentCountError */` |
|         3 | 6370 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6371 | `			"ArgumentCountError",` |
|         - | 6372 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6373 | `			);` |
|         - | 6374 | `	}` |
|        25 | 6375 | `	if( nArg > 2 ){` |
|         - | 6376 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6377 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6378 | `			"ArgumentCountError",` |
|         - | 6379 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6380 | `			nArg` |
|         - | 6381 | `			);` |
|         - | 6382 | `	}` |
|         - | 6383 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6384 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6385 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6386 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6387 | `			"TypeError",` |
|         - | 6388 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6389 | `			ph7_type_name(apArg[0])` |
|         - | 6390 | `			);` |
|         - | 6391 | `	}` |
|        19 | 6392 | `	bStrict = FALSE;` |
|         - | 6393 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6394 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6395 | `	/* Create a new array */` |
|        19 | 6396 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6397 | `	if( pArray == 0 ){` |
|       ! 0 | 6398 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6399 | `		return PH7_OK;` |
|         - | 6400 | `	}` |
|         - | 6401 | `	/* Perform the requested operation */` |
|        19 | 6402 | `	pEntry = pSrc->pFirst;` |
|        83 | 6403 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6404 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6405 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6406 | `		if( pNeedle ){` |
|        65 | 6407 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6408 | `		}` |
|        65 | 6409 | `		if( rc != SXRET_OK ){` |
|         - | 6410 | `			/* Perform the insertion */` |
|        37 | 6411 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6412 | `		}` |
|         - | 6413 | `		/* Point to the next entry */` |
|        65 | 6414 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6415 | `	}` |
|         - | 6416 | `	/* Return the freshly created array */` |
|        19 | 6417 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6418 | `	return PH7_OK;` |
|        16 | 6419 | `}` |
|         - | 6420 | `/*` |
|         - | 6421 | ` * array array_flip(array $input)` |
|         - | 6422 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6423 | ` * Parameter` |
|         - | 6424 | ` *  $input` |
|         - | 6425 | ` *   Input array.` |
|         - | 6426 | ` * Return` |
|         - | 6427 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6428 | ` */` |
|        34 | 6429 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6430 | `{` |
|         - | 6431 | `	ph7_hashmap_node *pEntry;` |
|         - | 6432 | `	ph7_hashmap *pSrc;` |
|         - | 6433 | `	ph7_value *pArray;` |
|         - | 6434 | `	ph7_value *pKey;` |
|         - | 6435 | `	ph7_value sVal;` |
|         - | 6436 | `	sxu32 n;` |
|         - | 6437 |  |
|         - | 6438 | `	/* PHP requires exactly one argument */` |
|        39 | 6439 | `	if( nArg != 1 ){` |
|         - | 6440 | `		/* Use ArgumentCountError like other array helpers */` |
|         8 | 6441 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6442 | `			"ArgumentCountError",` |
|         - | 6443 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         2 | 6444 | `			nArg` |
|         - | 6445 | `			);` |
|         - | 6446 | `	}` |
|         - | 6447 | `	/* Make sure we are dealing with a valid hashmap */` |
|        33 | 6448 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6449 | `		/* Type mismatch -> TypeError */` |
|         8 | 6450 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6451 | `			"TypeError",` |
|         - | 6452 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6453 | `			ph7_type_name(apArg[0])` |
|         - | 6454 | `			);` |
|         - | 6455 | `	}` |
|         - | 6456 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6457 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6458 | `	/* Create a new array */` |
|        27 | 6459 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6460 | `	if( pArray == 0 ){` |
|       ! 0 | 6461 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6462 | `		return PH7_OK;` |
|         - | 6463 | `	}` |
|         - | 6464 | `	/* Start processing */` |
|        27 | 6465 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6466 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6467 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6468 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6469 | `		if( pKey ){` |
|         - | 6470 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6471 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6472 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6473 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6474 | `					);` |
|     22236 | 6475 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6476 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6477 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6478 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6479 | `				}else{` |
|         - | 6480 | `					SyString sStr;` |
|      2227 | 6481 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6482 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6483 | `				}` |
|         - | 6484 | `				/* Perform the insertion */` |
|     22227 | 6485 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6486 | `				/* Safely release the value because each inserted entry` |
|         - | 6487 | `				 * has its own private copy of the value.` |
|         - | 6488 | `				 */` |
|     22227 | 6489 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6490 | `			}else{` |
|         - | 6491 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6492 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6493 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6494 | `					);` |
|         - | 6495 | `			}` |
|     11118 | 6496 | `		}` |
|         - | 6497 | `		/* Point to the next entry */` |
|     22237 | 6498 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6499 | `	}` |
|         - | 6500 | `	/* Return the freshly created array */` |
|        27 | 6501 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6502 | `	return PH7_OK;` |
|        22 | 6503 | `}` |
|         - | 6504 | `/*` |
|         - | 6505 | ` * number array_sum(array $array )` |
|         - | 6506 | ` *  Calculate the sum of values in an array.` |
|         - | 6507 | ` * Parameters` |
|         - | 6508 | ` *  $array: The input array.` |
|         - | 6509 | ` * Return` |
|         - | 6510 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6511 | ` */` |
|        24 | 6512 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6513 | `{` |
|         - | 6514 | `	ph7_hashmap_node *pEntry;` |
|         - | 6515 | `	ph7_value *pObj;` |
|        25 | 6516 | `	double dSum = 0;` |
|         - | 6517 | `	sxu32 n;` |
|        25 | 6518 | `	pEntry = pMap->pFirst;` |
|        91 | 6519 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        67 | 6520 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        67 | 6521 | `		if( pObj ){` |
|        67 | 6522 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        29 | 6523 | `				dSum += pObj->rVal;` |
|        53 | 6524 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6525 | `				dSum += (double)pObj->x.iVal;` |
|        29 | 6526 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        15 | 6527 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6528 | `					double dv = 0;` |
|        13 | 6529 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6530 | `					dSum += dv;` |
|         7 | 6531 | `				}` |
|        12 | 6532 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6533 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6534 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6535 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6536 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6537 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6538 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6539 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6540 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6541 | `			}` |
|         - | 6542 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6543 | `		}` |
|         - | 6544 | `		/* Point to the next entry */` |
|        67 | 6545 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        34 | 6546 | `	}` |
|         - | 6547 | `	/* Return sum */` |
|        25 | 6548 | `	ph7_result_double(pCtx,dSum);` |
|        25 | 6549 | `}` |
|        34 | 6550 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6551 | `{` |
|         - | 6552 | `	ph7_hashmap_node *pEntry;` |
|         - | 6553 | `	ph7_value *pObj;` |
|        36 | 6554 | `	sxi64 nSum = 0;` |
|         - | 6555 | `	sxu32 n;` |
|        36 | 6556 | `	pEntry = pMap->pFirst;` |
|       144 | 6557 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       110 | 6558 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       110 | 6559 | `		if( pObj ){` |
|       110 | 6560 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       100 | 6561 | `				nSum += pObj->x.iVal;` |
|        60 | 6562 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         7 | 6563 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         5 | 6564 | `					sxi64 nv = 0;` |
|         5 | 6565 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         5 | 6566 | `					nSum += nv;` |
|         3 | 6567 | `				}` |
|         8 | 6568 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6569 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6570 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6571 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6572 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6573 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6574 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6575 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6576 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6577 | `			}` |
|         - | 6578 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        54 | 6579 | `		}` |
|         - | 6580 | `		/* Point to the next entry */` |
|       110 | 6581 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        56 | 6582 | `	}` |
|         - | 6583 | `	/* Return sum */` |
|        36 | 6584 | `	ph7_result_int64(pCtx,nSum);` |
|        36 | 6585 | `}` |
|         - | 6586 | `/* number array_sum(array $array )` |
|         - | 6587 | ` * (See block-coment above)` |
|         - | 6588 | ` */` |
|        72 | 6589 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6590 | `{` |
|         - | 6591 | `	ph7_hashmap_node *pEntry;` |
|         - | 6592 | `	ph7_hashmap *pMap;` |
|         - | 6593 | `	ph7_value *pObj;` |
|        77 | 6594 | `	int useDouble = 0;` |
|         - | 6595 | `	sxu32 n;` |
|         - | 6596 | `	/* PHP requires exactly one argument */` |
|        77 | 6597 | `	if( nArg != 1 ){` |
|         8 | 6598 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6599 | `			"ArgumentCountError",` |
|         - | 6600 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         2 | 6601 | `			nArg` |
|         - | 6602 | `			);` |
|         - | 6603 | `	}` |
|         - | 6604 | `	/* Make sure we are dealing with a valid hashmap */` |
|        71 | 6605 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6606 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6607 | `		char zBuf[64];` |
|         8 | 6608 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6609 | `			"TypeError",` |
|         - | 6610 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6611 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6612 | `			);` |
|         - | 6613 | `	}` |
|        66 | 6614 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        66 | 6615 | `	if( pMap->nEntry < 1 ){` |
|         - | 6616 | `		/* Nothing to compute,return 0 */` |
|         7 | 6617 | `		ph7_result_int(pCtx,0);` |
|         7 | 6618 | `		return PH7_OK;` |
|         - | 6619 | `	}` |
|         - | 6620 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6621 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6622 | `	 */` |
|        60 | 6623 | `	pEntry = pMap->pFirst;` |
|       176 | 6624 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       142 | 6625 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       142 | 6626 | `		if( pObj ){` |
|       142 | 6627 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        19 | 6628 | `				useDouble = 1;` |
|        19 | 6629 | `				break;` |
|         - | 6630 | `			}` |
|       124 | 6631 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        13 | 6632 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        13 | 6633 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6634 | `				sxu32 i;` |
|        23 | 6635 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        17 | 6636 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6637 | `						useDouble = 1;` |
|         7 | 6638 | `						break;` |
|         - | 6639 | `					}` |
|         6 | 6640 | `				}` |
|        13 | 6641 | `				if( useDouble ){` |
|         7 | 6642 | `					break;` |
|         - | 6643 | `				}` |
|         3 | 6644 | `			}` |
|        58 | 6645 | `		}` |
|       118 | 6646 | `		pEntry = pEntry->pPrev;` |
|        60 | 6647 | `	}` |
|        60 | 6648 | `	if( useDouble ){` |
|        25 | 6649 | `		DoubleSum(pCtx,pMap);` |
|        13 | 6650 | `	}else{` |
|        36 | 6651 | `		Int64Sum(pCtx,pMap);` |
|         - | 6652 | `	}` |
|        60 | 6653 | `	return PH7_OK;` |
|        41 | 6654 | `}` |
|         - | 6655 | `/*` |
|         - | 6656 | ` * number array_product(array $array )` |
|         - | 6657 | ` *  Calculate the product of values in an array.` |
|         - | 6658 | ` * Parameters` |
|         - | 6659 | ` *  $array: The input array.` |
|         - | 6660 | ` * Return` |
|         - | 6661 | ` *  Returns the product of values as an integer or float.` |
|         - | 6662 | ` */` |
|         2 | 6663 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6664 | `{` |
|         - | 6665 | `	ph7_hashmap_node *pEntry;` |
|         - | 6666 | `	ph7_value *pObj;` |
|         - | 6667 | `	double dProd;` |
|         - | 6668 | `	sxu32 n;` |
|         3 | 6669 | `	pEntry = pMap->pFirst;` |
|         3 | 6670 | `	dProd = 1;` |
|         7 | 6671 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6672 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6673 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6674 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6675 | `				dProd *= pObj->rVal;` |
|         4 | 6676 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6677 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6678 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6679 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6680 | `					double dv = 0;` |
|       ! 0 | 6681 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6682 | `					dProd *= dv;` |
|       ! 0 | 6683 | `				}` |
|       ! 0 | 6684 | `			}` |
|         2 | 6685 | `		}` |
|         - | 6686 | `		/* Point to the next entry */` |
|         5 | 6687 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6688 | `	}` |
|         - | 6689 | `	/* Return product */` |
|         3 | 6690 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6691 | `}` |
|         2 | 6692 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6693 | `{` |
|         - | 6694 | `	ph7_hashmap_node *pEntry;` |
|         - | 6695 | `	ph7_value *pObj;` |
|         - | 6696 | `	sxi64 nProd;` |
|         - | 6697 | `	sxu32 n;` |
|         3 | 6698 | `	pEntry = pMap->pFirst;` |
|         3 | 6699 | `	nProd = 1;` |
|         9 | 6700 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6701 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6702 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6703 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6704 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6705 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6706 | `				nProd *= pObj->x.iVal;` |
|         3 | 6707 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6708 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6709 | `					sxi64 nv = 0;` |
|       ! 0 | 6710 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6711 | `					nProd *= nv;` |
|       ! 0 | 6712 | `				}` |
|       ! 0 | 6713 | `			}` |
|         3 | 6714 | `		}` |
|         - | 6715 | `		/* Point to the next entry */` |
|         7 | 6716 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6717 | `	}` |
|         - | 6718 | `	/* Return product */` |
|         3 | 6719 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6720 | `}` |
|         - | 6721 | `/* number array_product(array $array )` |
|         - | 6722 | ` * (See block-block comment above)` |
|         - | 6723 | ` */` |
|        18 | 6724 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6725 | `{` |
|         - | 6726 | `	ph7_hashmap *pMap;` |
|         - | 6727 | `	ph7_value *pObj;` |
|        19 | 6728 | `	if( nArg < 1 ){` |
|         - | 6729 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6730 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6731 | `		return PH7_OK;` |
|         - | 6732 | `	}` |
|         - | 6733 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        19 | 6734 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6735 | `		char zBuf[64];` |
|        19 | 6736 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6737 | `			"TypeError",` |
|         - | 6738 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 6739 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6740 | `			);` |
|         - | 6741 | `	}` |
|         7 | 6742 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6743 | `	if( pMap->nEntry < 1 ){` |
|         - | 6744 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6745 | `		ph7_result_int(pCtx,1);` |
|         3 | 6746 | `		return PH7_OK;` |
|         - | 6747 | `	}` |
|         - | 6748 | `	/* If the first element is of type float,then perform floating` |
|         - | 6749 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6750 | `	 */` |
|         5 | 6751 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6752 | `	if( pObj == 0 ){` |
|       ! 0 | 6753 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6754 | `		return PH7_OK;` |
|         - | 6755 | `	}` |
|         5 | 6756 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6757 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6758 | `	}else{` |
|         3 | 6759 | `		Int64Prod(pCtx,pMap);` |
|         - | 6760 | `	}` |
|         5 | 6761 | `	return PH7_OK;` |
|        10 | 6762 | `}` |
|         - | 6763 | `/*` |
|         - | 6764 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6765 | ` *  Pick one or more random entries out of an array.` |
|         - | 6766 | ` * Parameters` |
|         - | 6767 | ` * $input` |
|         - | 6768 | ` *  The input array.` |
|         - | 6769 | ` * $num_req` |
|         - | 6770 | ` *  Specifies how many entries you want to pick.` |
|         - | 6771 | ` * Return` |
|         - | 6772 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6773 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6774 | ` *  NULL is returned on failure.` |
|         - | 6775 | ` */` |
|        42 | 6776 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6777 | `{` |
|         - | 6778 | `	ph7_hashmap_node *pNode;` |
|         - | 6779 | `	ph7_hashmap *pMap;` |
|        43 | 6780 | `	int nItem = 1;` |
|        43 | 6781 | `	if( nArg < 1 ){` |
|         - | 6782 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6783 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6784 | `		return PH7_OK;` |
|         - | 6785 | `	}` |
|         - | 6786 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        43 | 6787 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6788 | `		char zBuf[64];` |
|        10 | 6789 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6790 | `			"TypeError",` |
|         - | 6791 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6792 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6793 | `			);` |
|         - | 6794 | `	}` |
|         - | 6795 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6796 | `	 * check, matching its ZPP-before-body ordering. */` |
|        37 | 6797 | `	if( nArg > 1 ){` |
|        29 | 6798 | `		ph7_value *pNum = apArg[1];` |
|        28 | 6799 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        24 | 6800 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6801 | `			char zBuf[64];` |
|        10 | 6802 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6803 | `				"TypeError",` |
|         - | 6804 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|         3 | 6805 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6806 | `				);` |
|         - | 6807 | `		}` |
|        23 | 6808 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6809 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6810 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6811 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6812 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6813 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6814 | `			int len;` |
|         9 | 6815 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6816 | `			sxi64 iLong; double dReal;` |
|         9 | 6817 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6818 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6819 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6820 | `					"TypeError",` |
|         - | 6821 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6822 | `					);` |
|         - | 6823 | `			}` |
|         - | 6824 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6825 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6826 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6827 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6828 | `			}` |
|         3 | 6829 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6830 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6831 | `			nItem = (int)iLong;` |
|         2 | 6832 | `		}else{` |
|        15 | 6833 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6834 | `		}` |
|         8 | 6835 | `	}` |
|         - | 6836 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6837 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6838 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6839 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6840 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6841 | `			"ValueError",` |
|         - | 6842 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 6843 | `			);` |
|         - | 6844 | `	}` |
|         - | 6845 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 6846 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 6847 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6848 | `			"ValueError",` |
|         - | 6849 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 6850 | `			);` |
|         - | 6851 | `	}` |
|        13 | 6852 | `	if( nItem < 2 ){` |
|         - | 6853 | `		sxu32 nEntry;` |
|         - | 6854 | `		/* Select a random number */` |
|         9 | 6855 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 6856 | `		/* Extract the desired entry.` |
|         - | 6857 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 6858 | `		 */` |
|         9 | 6859 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         4 | 6860 | `			pNode = pMap->pLast;` |
|         4 | 6861 | `			nEntry = pMap->nEntry - nEntry;` |
|         4 | 6862 | `			if( nEntry > 1 ){` |
|       ! 0 | 6863 | `				for(;;){` |
|       ! 0 | 6864 | `					if( nEntry == 0 ){` |
|       ! 0 | 6865 | `						break;` |
|         - | 6866 | `					}` |
|         - | 6867 | `					/* Point to the previous entry */` |
|       ! 0 | 6868 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 6869 | `					nEntry--;` |
|       ! 0 | 6870 | `				}` |
|       ! 0 | 6871 | `			}` |
|         3 | 6872 | `		}else{` |
|         5 | 6873 | `			pNode = pMap->pFirst;` |
|         1 | 6874 | `			for(;;){` |
|         8 | 6875 | `				if( nEntry == 0 ){` |
|         5 | 6876 | `					break;` |
|         - | 6877 | `				}` |
|         - | 6878 | `				/* Point to the next entry */` |
|         4 | 6879 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         4 | 6880 | `				nEntry--;` |
|         1 | 6881 | `			}` |
|         - | 6882 | `		}` |
|         9 | 6883 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 6884 | `			/* Int key */` |
|         7 | 6885 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 6886 | `		}else{` |
|         - | 6887 | `			/* Blob key */` |
|         3 | 6888 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 6889 | `		}` |
|         5 | 6890 | `	}else{` |
|         - | 6891 | `		ph7_value sKey,*pArray;` |
|         - | 6892 | `		ph7_hashmap *pDest;` |
|         - | 6893 | `		/* Create a new array */` |
|         5 | 6894 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 6895 | `		if( pArray == 0 ){` |
|       ! 0 | 6896 | `			ph7_result_null(pCtx);` |
|       ! 0 | 6897 | `			return PH7_OK;` |
|         - | 6898 | `		}` |
|         - | 6899 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 6900 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 6901 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 6902 | `		/* Copy the first n items */` |
|         5 | 6903 | `		pNode = pMap->pFirst;` |
|         5 | 6904 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 6905 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 6906 | `		}` |
|        15 | 6907 | `		while( nItem > 0){` |
|        11 | 6908 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 6909 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 6910 | `			PH7_MemObjRelease(&sKey);` |
|         - | 6911 | `			/* Point to the next entry */` |
|        11 | 6912 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 6913 | `			nItem--;` |
|         1 | 6914 | `		}` |
|         - | 6915 | `		/* Shuffle the array */` |
|         5 | 6916 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 6917 | `		/* Rehash node */` |
|         5 | 6918 | `		HashmapSortRehash(pDest);` |
|         - | 6919 | `		/* Return the random array */` |
|         5 | 6920 | `		ph7_result_value(pCtx,pArray);` |
|         - | 6921 | `	}` |
|        13 | 6922 | `	return PH7_OK;` |
|        22 | 6923 | `}` |
|         - | 6924 | `/*` |
|         - | 6925 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 6926 | ` *  Split an array into chunks.` |
|         - | 6927 | ` * Parameters` |
|         - | 6928 | ` * $input` |
|         - | 6929 | ` *   The array to work on` |
|         - | 6930 | ` * $size` |
|         - | 6931 | ` *   The size of each chunk` |
|         - | 6932 | ` * $preserve_keys` |
|         - | 6933 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 6934 | ` *   the chunk numerically.` |
|         - | 6935 | ` * Return` |
|         - | 6936 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 6937 | ` *  zero, with each dimension containing size elements.` |
|         - | 6938 | ` */` |
|        42 | 6939 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6940 | `{` |
|         - | 6941 | `	ph7_value *pArray,*pChunk;` |
|         - | 6942 | `	ph7_hashmap_node *pEntry;` |
|         - | 6943 | `	ph7_hashmap *pMap;` |
|         - | 6944 | `	int bPreserve;` |
|         - | 6945 | `	sxu32 nChunk;` |
|         - | 6946 | `	sxu32 nSize;` |
|         - | 6947 | `	sxu32 n;` |
|         - | 6948 | `	/* Argument count and types follow PHP semantics. */` |
|        47 | 6949 | `	if( nArg < 2 ){` |
|         - | 6950 | `		/* fewer than required arguments -> ArgumentCountError */` |
|         4 | 6951 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6952 | `			"ArgumentCountError",` |
|         - | 6953 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|         1 | 6954 | `			nArg` |
|         - | 6955 | `			);` |
|         - | 6956 | `	}` |
|        45 | 6957 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6958 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6959 | `			"TypeError",` |
|         - | 6960 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6961 | `			ph7_type_name(apArg[0])` |
|         - | 6962 | `			);` |
|         - | 6963 | `	}` |
|         - | 6964 | `	/* Create a new array */` |
|        43 | 6965 | `	pArray = ph7_context_new_array(pCtx);` |
|        43 | 6966 | `	if( pArray == 0 ){` |
|       ! 0 | 6967 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6968 | `		return PH7_OK;` |
|         - | 6969 | `	}` |
|         - | 6970 | `	/* Point to the internal representation of the input hashmap */` |
|        43 | 6971 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6972 | `	/* Extract and validate the chunk size argument. */` |
|         - | 6973 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        57 | 6974 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        81 | 6975 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        38 | 6976 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 6977 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6978 | `			"TypeError",` |
|         - | 6979 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 6980 | `			ph7_type_name(apArg[1])` |
|         - | 6981 | `			);` |
|         - | 6982 | `	}` |
|         - | 6983 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 6984 | `	 * strings are permitted; however those representing floats lose` |
|         - | 6985 | `	 * precision and PHP emits a deprecation warning. */` |
|        43 | 6986 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6987 | `		int len;` |
|         3 | 6988 | `		sxu8 bReal = FALSE;` |
|         3 | 6989 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6990 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6991 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6992 | `				"TypeError",` |
|         - | 6993 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 6994 | `				);` |
|         - | 6995 | `		}` |
|       ! 0 | 6996 | `		if( bReal ){` |
|         - | 6997 | `			/* float-string -> warn but allow */` |
|       ! 0 | 6998 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6999 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7000 | `				zStr` |
|         - | 7001 | `				);` |
|       ! 0 | 7002 | `		}` |
|       ! 0 | 7003 | `	}` |
|         - | 7004 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7005 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7006 | `	 * later via ph7_value_to_int. */` |
|        40 | 7007 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7008 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7009 | `		sxi64 i = (sxi64)d;` |
|         3 | 7010 | `		if( d != (double)i ){` |
|         4 | 7011 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7012 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7013 | `				d` |
|         - | 7014 | `				);` |
|         1 | 7015 | `		}` |
|         1 | 7016 | `	}` |
|         - | 7017 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7018 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7019 | `	{` |
|        40 | 7020 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        40 | 7021 | `		if( nSizeSigned < 1 ){` |
|         - | 7022 | `			/* size <= 0 -> ValueError */` |
|         6 | 7023 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7024 | `				"ValueError",` |
|         - | 7025 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7026 | `				);` |
|         - | 7027 | `		}` |
|        35 | 7028 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7029 | `	}` |
|        35 | 7030 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7031 | `		/* Return the whole array */` |
|         3 | 7032 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7033 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7034 | `		return PH7_OK;` |
|         - | 7035 | `	}` |
|        33 | 7036 | `	bPreserve = 0;` |
|        33 | 7037 | `	if( nArg > 2 ){` |
|         - | 7038 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7039 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7040 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7041 | `		 * normally, matching PHP behaviour. */` |
|        35 | 7042 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        35 | 7043 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7044 | `			ph7_value_is_resource(apArg[2]) ){` |
|         8 | 7045 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7046 | `				"TypeError",` |
|         - | 7047 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|         4 | 7048 | `				ph7_type_name(apArg[2])` |
|         - | 7049 | `				);` |
|         - | 7050 | `		}` |
|        21 | 7051 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7052 | `	}` |
|         - | 7053 | `	/* Start processing */` |
|        27 | 7054 | `	pEntry = pMap->pFirst;` |
|        27 | 7055 | `	nChunk = 0;` |
|        27 | 7056 | `	pChunk = 0;` |
|        27 | 7057 | `	n = pMap->nEntry;` |
|        56 | 7058 | `	for( ;; ){` |
|       113 | 7059 | `		if( n < 1 ){` |
|         - | 7060 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7061 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7062 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7063 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7064 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7065 | `			 * exists. */` |
|        27 | 7066 | `			if( pChunk ){` |
|        27 | 7067 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7068 | `			}` |
|        27 | 7069 | `			break;` |
|         - | 7070 | `		}` |
|        87 | 7071 | `		if( nChunk < 1 ){` |
|        71 | 7072 | `			if( pChunk ){` |
|         - | 7073 | `				/* Put the first chunk */` |
|        45 | 7074 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7075 | `			}` |
|         - | 7076 | `			/* Create a new dimension */` |
|        71 | 7077 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7078 | `												   * will be automatically released as soon we return` |
|         - | 7079 | `												   * from this function */` |
|        71 | 7080 | `			if( pChunk == 0 ){` |
|       ! 0 | 7081 | `				break;` |
|         - | 7082 | `			}` |
|        71 | 7083 | `			nChunk = nSize;` |
|        35 | 7084 | `		}` |
|         - | 7085 | `		/* Insert the entry */` |
|        87 | 7086 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7087 | `		/* Point to the next entry */` |
|        87 | 7088 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7089 | `		nChunk--;` |
|        87 | 7090 | `		n--;` |
|         1 | 7091 | `	}` |
|         - | 7092 | `	/* Return the multidimensional array */` |
|        27 | 7093 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7094 | `	return PH7_OK;` |
|        26 | 7095 | `}` |
|         - | 7096 | `/*` |
|         - | 7097 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7098 | ` *  Pad array to the specified length with a value.` |
|         - | 7099 | ` * $input` |
|         - | 7100 | ` *   Initial array of values to pad.` |
|         - | 7101 | ` * $pad_size` |
|         - | 7102 | ` *   New size of the array.` |
|         - | 7103 | ` * $pad_value` |
|         - | 7104 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7105 | ` */` |
|         - | 7106 | `/*` |
|         - | 7107 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7108 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7109 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7110 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7111 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7112 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7113 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7114 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7115 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7116 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7117 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7118 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7119 | ` */` |
|        50 | 7120 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7121 | `	ph7_context *pCtx,` |
|         - | 7122 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7123 | `	int iArg,              /* 1-based argument position */` |
|         - | 7124 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7125 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7126 | `	)` |
|         1 | 7127 | `{` |
|        51 | 7128 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7129 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7130 | `			"ValueError",` |
|         - | 7131 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7132 | `			zFunc,iArg,zParam` |
|         - | 7133 | `			);` |
|         - | 7134 | `	}` |
|        37 | 7135 | `	return SXRET_OK;` |
|        26 | 7136 | `}` |
|        72 | 7137 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7138 | `{` |
|         - | 7139 | `	ph7_hashmap *pMap;` |
|         - | 7140 | `	ph7_value *pArray;` |
|         - | 7141 | `	sxi64 iLen,iAbs;` |
|         - | 7142 | `	int nEntry;` |
|         - | 7143 | `	sxi32 rc;` |
|        77 | 7144 | `	if( nArg != 3 ){` |
|        12 | 7145 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7146 | `			"ArgumentCountError",` |
|         - | 7147 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         3 | 7148 | `			nArg` |
|         - | 7149 | `			);` |
|         - | 7150 | `	}` |
|        68 | 7151 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7152 | `		char zBuf[64];` |
|        14 | 7153 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7154 | `			"TypeError",` |
|         - | 7155 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 7156 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7157 | `			);` |
|         - | 7158 | `	}` |
|         - | 7159 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7160 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7161 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7162 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        58 | 7163 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        56 | 7164 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7165 | `		char zBuf[64];` |
|         7 | 7166 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7167 | `			"TypeError",` |
|         - | 7168 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|         4 | 7169 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7170 | `			);` |
|         - | 7171 | `	}` |
|        55 | 7172 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7173 | `		int nStr;` |
|        11 | 7174 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7175 | `		sxi64 iLong; double dReal;` |
|        11 | 7176 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7177 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7178 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7179 | `				"TypeError",` |
|         - | 7180 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7181 | `				);` |
|         - | 7182 | `		}` |
|         7 | 7183 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7184 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7185 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7186 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7187 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7188 | `					"TypeError",` |
|         - | 7189 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7190 | `					);` |
|         - | 7191 | `			}` |
|         3 | 7192 | `			iLen = (sxi64)dReal;` |
|         3 | 7193 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7194 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7195 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7196 | `					zStr` |
|         - | 7197 | `					);` |
|       ! 0 | 7198 | `			}` |
|         2 | 7199 | `		}else{` |
|         5 | 7200 | `			iLen = iLong;` |
|         - | 7201 | `		}` |
|         4 | 7202 | `	}else{` |
|        45 | 7203 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7204 | `	}` |
|         - | 7205 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7206 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7207 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7208 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7209 | `	 * overflow). */` |
|        51 | 7210 | `	iAbs = iLen;` |
|        51 | 7211 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7212 | `		iAbs = -iAbs;` |
|         7 | 7213 | `	}` |
|        51 | 7214 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7215 | `	if( rc != SXRET_OK ){` |
|        15 | 7216 | `		return rc;` |
|         - | 7217 | `	}` |
|        37 | 7218 | `	nEntry = (int)iLen;` |
|         - | 7219 | `	/* Create a new array */` |
|        37 | 7220 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7221 | `	if( pArray == 0 ){` |
|       ! 0 | 7222 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7223 | `	}` |
|        37 | 7224 | `	if( nEntry < 0 ){` |
|        11 | 7225 | `		nEntry = -nEntry;` |
|        11 | 7226 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7227 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7228 | `			/* Insert given items first */` |
|        25 | 7229 | `			while( nEntry > 0 ){` |
|        19 | 7230 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7231 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7232 | `				}` |
|        19 | 7233 | `				nEntry--;` |
|         1 | 7234 | `			}` |
|         - | 7235 | `			/* Merge the two arrays */` |
|         7 | 7236 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7237 | `		}else{` |
|         5 | 7238 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7239 | `		}` |
|        32 | 7240 | `	}else if( nEntry > 0 ){` |
|        25 | 7241 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7242 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7243 | `			/* Merge the two arrays first */` |
|        19 | 7244 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7245 | `			/* Insert given items */` |
|       275 | 7246 | `			while( nEntry > 0 ){` |
|       257 | 7247 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7248 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7249 | `				}` |
|       257 | 7250 | `				nEntry--;` |
|         1 | 7251 | `			}` |
|        10 | 7252 | `		}else{` |
|         7 | 7253 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7254 | `		}` |
|        13 | 7255 | `	}else{` |
|         - | 7256 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7257 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7258 | `	}` |
|         - | 7259 | `	/* Return the new array */` |
|        37 | 7260 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7261 | `	return PH7_OK;` |
|        41 | 7262 | `}` |
|         - | 7263 | `/*` |
|         - | 7264 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7265 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7266 | ` * Parameters` |
|         - | 7267 | ` * $array` |
|         - | 7268 | ` *   The array in which elements are replaced.` |
|         - | 7269 | ` * $array1` |
|         - | 7270 | ` *   The array from which elements will be extracted.` |
|         - | 7271 | ` * ....` |
|         - | 7272 | ` *  More arrays from which elements will be extracted.` |
|         - | 7273 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7274 | ` * Return` |
|         - | 7275 | ` *  Returns an array.` |
|         - | 7276 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7277 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7278 | ` */` |
|        22 | 7279 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7280 | `{` |
|         - | 7281 | `	ph7_hashmap *pMap;` |
|         - | 7282 | `	ph7_value *pArray;` |
|         - | 7283 | `	int i;` |
|        26 | 7284 | `	if( nArg < 1 ){` |
|         3 | 7285 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7286 | `			"ArgumentCountError",` |
|         - | 7287 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7288 | `			);` |
|         - | 7289 | `	}` |
|        23 | 7290 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7291 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7292 | `			"TypeError",` |
|         - | 7293 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7294 | `			ph7_type_name(apArg[0])` |
|         - | 7295 | `			);` |
|         - | 7296 | `	}` |
|         - | 7297 | `	/* Create a new array */` |
|        20 | 7298 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7299 | `	if( pArray == 0 ){` |
|       ! 0 | 7300 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7301 | `		return PH7_OK;` |
|         - | 7302 | `	}` |
|         - | 7303 | `	/* Overwrite from the first array */` |
|        20 | 7304 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7305 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7306 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7307 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7308 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7309 | `			/* Type mismatch -> TypeError */` |
|         4 | 7310 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7311 | `				"TypeError",` |
|         - | 7312 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7313 | `				i + 1,` |
|         2 | 7314 | `				ph7_type_name(apArg[i])` |
|         - | 7315 | `				);` |
|         - | 7316 | `		}` |
|         - | 7317 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7318 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7319 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7320 | `	}` |
|         - | 7321 | `	/* Return the new array */` |
|        17 | 7322 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7323 | `	return PH7_OK;` |
|        15 | 7324 | `}` |
|         - | 7325 | `/*` |
|         - | 7326 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7327 | ` *  Filters elements of an array using a callback function.` |
|         - | 7328 | ` * Parameters` |
|         - | 7329 | ` *  $input` |
|         - | 7330 | ` *    The array to iterate over` |
|         - | 7331 | ` * $callback` |
|         - | 7332 | ` *    The callback function to use` |
|         - | 7333 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7334 | ` *    will be removed.` |
|         - | 7335 | ` * Return` |
|         - | 7336 | ` *  The filtered array.` |
|         - | 7337 | ` */` |
|        32 | 7338 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7339 | `{` |
|         - | 7340 | `	ph7_hashmap_node *pEntry;` |
|         - | 7341 | `	ph7_hashmap *pMap;` |
|         - | 7342 | `	ph7_value *pArray;` |
|         - | 7343 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7344 | `	ph7_value *pValue;` |
|         - | 7345 | `	sxi32 rc;` |
|         - | 7346 | `	int keep;` |
|         - | 7347 | `	sxu32 n;` |
|        34 | 7348 | `	if( nArg < 1 ){` |
|         - | 7349 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7350 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7351 | `		return PH7_OK;` |
|         - | 7352 | `	}` |
|         - | 7353 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        34 | 7354 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7355 | `		char zBuf[64];` |
|        22 | 7356 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7357 | `			"TypeError",` |
|         - | 7358 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         7 | 7359 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7360 | `			);` |
|         - | 7361 | `	}` |
|         - | 7362 | `	/* Create a new array */` |
|        20 | 7363 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7364 | `	if( pArray == 0 ){` |
|       ! 0 | 7365 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7366 | `		return PH7_OK;` |
|         - | 7367 | `	}` |
|         - | 7368 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7369 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7370 | `	pEntry = pMap->pFirst;` |
|        20 | 7371 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7372 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7373 | `	/* Perform the requested operation */` |
|        78 | 7374 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7375 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7376 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7377 | `		if( pValue == 0 ){` |
|         - | 7378 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7379 | `			keep = FALSE;` |
|        64 | 7380 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7381 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7382 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7383 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7384 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7385 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7386 | `					int len;` |
|         3 | 7387 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7388 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7389 | `						"TypeError",` |
|         - | 7390 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7391 | `						zName` |
|         - | 7392 | `						);` |
|       ! 0 | 7393 | `				}else{` |
|       ! 0 | 7394 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7395 | `						"TypeError",` |
|         - | 7396 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7397 | `						ph7_type_name(apArg[1])` |
|         - | 7398 | `						);` |
|         - | 7399 | `				}` |
|         - | 7400 | `			}` |
|        33 | 7401 | `			keep = FALSE;` |
|        33 | 7402 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7403 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7404 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7405 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7406 | `				return PH7_EXCEPTION;` |
|         - | 7407 | `			}` |
|        31 | 7408 | `			if( rc == SXRET_OK ){` |
|         - | 7409 | `				/* Perform a boolean cast */` |
|        31 | 7410 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7411 | `			}` |
|        31 | 7412 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7413 | `		}else{` |
|         - | 7414 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7415 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7416 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7417 | `			 */` |
|        29 | 7418 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7419 | `		}` |
|        59 | 7420 | `		if( keep ){` |
|         - | 7421 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7422 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7423 | `		}` |
|         - | 7424 | `		/* Point to the next entry */` |
|        59 | 7425 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7426 | `	}` |
|        15 | 7427 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7428 | `	return PH7_OK;` |
|        18 | 7429 | `}` |
|         - | 7430 | `/*` |
|         - | 7431 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7432 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7433 | ` * Parameters` |
|         - | 7434 | ` *  $callback` |
|         - | 7435 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7436 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7437 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7438 | ` *   are zipped together.` |
|         - | 7439 | ` *  $array` |
|         - | 7440 | ` *   The first array to run through the callback function.` |
|         - | 7441 | ` *  $arrays` |
|         - | 7442 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7443 | ` * Return` |
|         - | 7444 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7445 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7446 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7447 | ` *  padding shorter arrays with NULL.` |
|         - | 7448 | ` */` |
|        56 | 7449 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7450 | `{` |
|         - | 7451 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7452 | `	ph7_hashmap_node *pEntry;` |
|         - | 7453 | `	ph7_hashmap *pMap;` |
|         - | 7454 | `	ph7_vm *pVm;` |
|         - | 7455 | `	int bNullCallback;` |
|         - | 7456 | `	sxi32 rc;` |
|         - | 7457 | `	int i;` |
|         - | 7458 | `	sxu32 n;` |
|        61 | 7459 | `	if( nArg < 2 ){` |
|         8 | 7460 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7461 | `			"ArgumentCountError",` |
|         - | 7462 | `			"array_map() expects at least 2 arguments, %d given",` |
|         2 | 7463 | `			nArg` |
|         - | 7464 | `			);` |
|         - | 7465 | `	}` |
|        56 | 7466 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        56 | 7467 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7468 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7469 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7470 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7471 | `				"TypeError",` |
|         - | 7472 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7473 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7474 | `				zFunc` |
|         - | 7475 | `				);` |
|         - | 7476 | `		}` |
|         3 | 7477 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7478 | `			"TypeError",` |
|         - | 7479 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7480 | `			"no array or string given"` |
|         - | 7481 | `			);` |
|         - | 7482 | `	}` |
|         - | 7483 | `	/* Every remaining argument must be an array */` |
|       109 | 7484 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        63 | 7485 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7486 | `			if( i == 1 ){` |
|         4 | 7487 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7488 | `					"TypeError",` |
|         - | 7489 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7490 | `					ph7_type_name(apArg[1])` |
|         - | 7491 | `					);` |
|         - | 7492 | `			}` |
|       ! 0 | 7493 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7494 | `				"TypeError",` |
|         - | 7495 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7496 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7497 | `				);` |
|         - | 7498 | `		}` |
|        31 | 7499 | `	}` |
|        48 | 7500 | `	pVm = pCtx->pVm;` |
|         - | 7501 | `	/* Create a new array */` |
|        48 | 7502 | `	pArray = ph7_context_new_array(pCtx);` |
|        48 | 7503 | `	if( pArray == 0 ){` |
|       ! 0 | 7504 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7505 | `		return PH7_OK;` |
|         - | 7506 | `	}` |
|        48 | 7507 | `	PH7_MemObjInit(pVm,&sResult);` |
|        48 | 7508 | `	PH7_MemObjInit(pVm,&sKey);` |
|        48 | 7509 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        48 | 7510 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        48 | 7511 | `	if( nArg == 2 ){` |
|         - | 7512 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        38 | 7513 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        38 | 7514 | `		pEntry = pMap->pFirst;` |
|       112 | 7515 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7516 | `			/* Extract the node value */` |
|        80 | 7517 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        80 | 7518 | `			if( pValue ){` |
|         - | 7519 | `				/* Extract the node key */` |
|        80 | 7520 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        80 | 7521 | `				if( bNullCallback ){` |
|         - | 7522 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7523 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7524 | `				}else{` |
|         - | 7525 | `					/* Invoke the supplied callback */` |
|        70 | 7526 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        70 | 7527 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7528 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7529 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7530 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7531 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7532 | `						return PH7_EXCEPTION;` |
|         - | 7533 | `					}` |
|         - | 7534 | `					/* Insert the callback return value */` |
|        66 | 7535 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7536 | `				}` |
|        76 | 7537 | `				PH7_MemObjRelease(&sKey);` |
|        76 | 7538 | `				PH7_MemObjRelease(&sResult);` |
|        37 | 7539 | `			}` |
|         - | 7540 | `			/* Point to the next entry */` |
|        76 | 7541 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        39 | 7542 | `		}` |
|        18 | 7543 | `	}else{` |
|         - | 7544 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7545 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7546 | `		int nArrays = nArg - 1;` |
|         - | 7547 | `		ph7_hashmap_node **apCur;` |
|         - | 7548 | `		ph7_value **apCallArg;` |
|         - | 7549 | `		ph7_value sNull;` |
|        11 | 7550 | `		sxu32 nMax = 0;` |
|        11 | 7551 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7552 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7553 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7554 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7555 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7556 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7557 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7558 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7559 | `			return PH7_OK;` |
|         - | 7560 | `		}` |
|        11 | 7561 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7562 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7563 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7564 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7565 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7566 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7567 | `				nMax = pMap->nEntry;` |
|         6 | 7568 | `			}` |
|        12 | 7569 | `		}` |
|        35 | 7570 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7571 | `			ph7_value *pZip = 0;` |
|        25 | 7572 | `			if( bNullCallback ){` |
|         - | 7573 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7574 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7575 | `			}` |
|        79 | 7576 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7577 | `				ph7_value *pv = &sNull;` |
|        55 | 7578 | `				if( apCur[i] ){` |
|        53 | 7579 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7580 | `					if( pNodeVal ){` |
|        53 | 7581 | `						pv = pNodeVal;` |
|        26 | 7582 | `					}` |
|        53 | 7583 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7584 | `				}` |
|        55 | 7585 | `				if( bNullCallback ){` |
|         9 | 7586 | `					if( pZip ){` |
|         9 | 7587 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7588 | `					}` |
|         5 | 7589 | `				}else{` |
|        47 | 7590 | `					apCallArg[i] = pv;` |
|         - | 7591 | `				}` |
|        28 | 7592 | `			}` |
|        25 | 7593 | `			if( bNullCallback ){` |
|         5 | 7594 | `				if( pZip ){` |
|         5 | 7595 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7596 | `				}` |
|         3 | 7597 | `			}else{` |
|        21 | 7598 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7599 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7600 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7601 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7602 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7603 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7604 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7605 | `					return PH7_EXCEPTION;` |
|         - | 7606 | `				}` |
|        21 | 7607 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7608 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7609 | `			}` |
|        13 | 7610 | `		}` |
|        11 | 7611 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7612 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7613 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7614 | `	}` |
|        44 | 7615 | `	PH7_MemObjRelease(&sKey);` |
|        44 | 7616 | `	PH7_MemObjRelease(&sResult);` |
|        44 | 7617 | `	ph7_result_value(pCtx,pArray);` |
|        44 | 7618 | `	return PH7_OK;` |
|        33 | 7619 | `}` |
|         - | 7620 | `/*` |
|         - | 7621 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7622 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7623 | ` * Parameters` |
|         - | 7624 | ` *  $array` |
|         - | 7625 | ` *   The input array.` |
|         - | 7626 | ` *  $callback` |
|         - | 7627 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7628 | ` *  $initial` |
|         - | 7629 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7630 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7631 | ` * Return` |
|         - | 7632 | ` *  Returns the resulting value.` |
|         - | 7633 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7634 | ` */` |
|        34 | 7635 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7636 | `{` |
|         - | 7637 | `	ph7_hashmap_node *pEntry;` |
|         - | 7638 | `	ph7_hashmap *pMap;` |
|         - | 7639 | `	ph7_value *pValue;` |
|         - | 7640 | `	ph7_value sResult;` |
|         - | 7641 | `	sxi32 rc;` |
|         - | 7642 | `	sxu32 n;` |
|        39 | 7643 | `	if( nArg < 2 ){` |
|         8 | 7644 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7645 | `			"ArgumentCountError",` |
|         - | 7646 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|         2 | 7647 | `			nArg` |
|         - | 7648 | `			);` |
|         - | 7649 | `	}` |
|        35 | 7650 | `	if( nArg > 3 ){` |
|         4 | 7651 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7652 | `			"ArgumentCountError",` |
|         - | 7653 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7654 | `			nArg` |
|         - | 7655 | `			);` |
|         - | 7656 | `	}` |
|        33 | 7657 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7658 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7659 | `			"TypeError",` |
|         - | 7660 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7661 | `			ph7_type_name(apArg[0])` |
|         - | 7662 | `			);` |
|         - | 7663 | `	}` |
|        31 | 7664 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7665 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7666 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7667 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7668 | `				"TypeError",` |
|         - | 7669 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7670 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7671 | `				zFunc` |
|         - | 7672 | `				);` |
|         - | 7673 | `		}` |
|         9 | 7674 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7675 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7676 | `				"TypeError",` |
|         - | 7677 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7678 | `				"array callback must have exactly two members"` |
|         - | 7679 | `				);` |
|         - | 7680 | `		}` |
|         6 | 7681 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7682 | `			"TypeError",` |
|         - | 7683 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7684 | `			"no array or string given"` |
|         - | 7685 | `			);` |
|         - | 7686 | `	}` |
|         - | 7687 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7688 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7689 | `	/* Assume a NULL initial value */` |
|        19 | 7690 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7691 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7692 | `	if( nArg > 2 ){` |
|         - | 7693 | `		/* Set the initial value */` |
|        13 | 7694 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7695 | `	}` |
|         - | 7696 | `	/* Perform the requested operation */` |
|        19 | 7697 | `	pEntry = pMap->pFirst;` |
|        55 | 7698 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7699 | `		/* Extract the node value */` |
|        39 | 7700 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7701 | `		/* Invoke the supplied callback */` |
|        39 | 7702 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7703 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7704 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7705 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7706 | `			return PH7_EXCEPTION;` |
|         - | 7707 | `		}` |
|         - | 7708 | `		/* Point to the next entry */` |
|        37 | 7709 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7710 | `	}` |
|        17 | 7711 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7712 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7713 | `	return PH7_OK;` |
|        22 | 7714 | `}` |
|         - | 7715 | `/*` |
|         - | 7716 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7717 | ` *  Apply a user function to every member of an array.` |
|         - | 7718 | ` * Parameters` |
|         - | 7719 | ` *  $array` |
|         - | 7720 | ` *   The input array.` |
|         - | 7721 | ` *  $funcname` |
|         - | 7722 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7723 | ` *   the first, and the key/index second.` |
|         - | 7724 | ` * Note:` |
|         - | 7725 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7726 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7727 | ` *  be made in the original array itself.` |
|         - | 7728 | ` *  $userdata` |
|         - | 7729 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7730 | ` *   to the callback funcname.` |
|         - | 7731 | ` * Return` |
|         - | 7732 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7733 | ` */` |
|        38 | 7734 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7735 | `{` |
|         - | 7736 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7737 | `	ph7_hashmap_node *pEntry;` |
|         - | 7738 | `	ph7_hashmap *pMap;` |
|         - | 7739 | `	sxu32 n;` |
|        43 | 7740 | `	if( nArg < 2 ){` |
|         8 | 7741 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7742 | `			"ArgumentCountError",` |
|         - | 7743 | `			"array_walk() expects at least 2 arguments, %d given",` |
|         2 | 7744 | `			nArg` |
|         - | 7745 | `			);` |
|         - | 7746 | `	}` |
|        39 | 7747 | `	if( nArg > 3 ){` |
|         4 | 7748 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7749 | `			"ArgumentCountError",` |
|         - | 7750 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7751 | `			nArg` |
|         - | 7752 | `			);` |
|         - | 7753 | `	}` |
|        37 | 7754 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7755 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7756 | `			"TypeError",` |
|         - | 7757 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7758 | `			ph7_type_name(apArg[0])` |
|         - | 7759 | `			);` |
|         - | 7760 | `	}` |
|        35 | 7761 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7762 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7763 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7764 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7765 | `				"TypeError",` |
|         - | 7766 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7767 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7768 | `				zFunc` |
|         - | 7769 | `				);` |
|         - | 7770 | `		}` |
|        12 | 7771 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7772 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7773 | `				"TypeError",` |
|         - | 7774 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7775 | `				"array callback must have exactly two members"` |
|         - | 7776 | `				);` |
|         - | 7777 | `		}` |
|         6 | 7778 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7779 | `			"TypeError",` |
|         - | 7780 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7781 | `			"no array or string given"` |
|         - | 7782 | `			);` |
|         - | 7783 | `	}` |
|        21 | 7784 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7785 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7786 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7787 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7788 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7789 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7790 | `	/* Perform the desired operation */` |
|        21 | 7791 | `	pEntry = pMap->pFirst;` |
|        61 | 7792 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7793 | `		/* Extract the node value */` |
|        43 | 7794 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7795 | `		if( pValue ){` |
|         - | 7796 | `			sxi32 rcW;` |
|         - | 7797 | `			/* Extract the entry key */` |
|        43 | 7798 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7799 | `			/* Invoke the supplied callback */` |
|        43 | 7800 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7801 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7802 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7803 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7804 | `				return PH7_EXCEPTION;` |
|         - | 7805 | `			}` |
|        20 | 7806 | `		}` |
|         - | 7807 | `		/* Point to the next entry */` |
|        41 | 7808 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7809 | `	}` |
|         - | 7810 | `	/* All done, return TRUE */` |
|        19 | 7811 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7812 | `	return PH7_OK;` |
|        24 | 7813 | `}` |
|         - | 7814 | `/*` |
|         - | 7815 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7816 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7817 | ` */` |
|        22 | 7818 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7819 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7820 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7821 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7822 | `	int iNest             /* Nesting level */` |
|         - | 7823 | `	)` |
|         1 | 7824 | `{` |
|         - | 7825 | `	ph7_hashmap_node *pEntry;` |
|         - | 7826 | `	ph7_value *pValue,sKey;` |
|         - | 7827 | `	sxi32 rc;` |
|         - | 7828 | `	sxu32 n;` |
|         - | 7829 | `	/* Iterate through hashmap entries */` |
|        23 | 7830 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7831 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7832 | `	pEntry = pMap->pFirst;` |
|        59 | 7833 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7834 | `		/* Extract the node value */` |
|        37 | 7835 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7836 | `		if( pValue ){` |
|        37 | 7837 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7838 | `				if( iNest < 32 ){` |
|         - | 7839 | `					/* Recurse */` |
|        11 | 7840 | `					iNest++;` |
|        11 | 7841 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 7842 | `					iNest--;` |
|        11 | 7843 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7844 | `						return PH7_EXCEPTION;` |
|         - | 7845 | `					}` |
|         5 | 7846 | `				}` |
|         6 | 7847 | `			}else{` |
|         - | 7848 | `				/* Extract the node key */` |
|        27 | 7849 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7850 | `				/* Invoke the supplied callback */` |
|        27 | 7851 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 7852 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 7853 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 7854 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7855 | `					return PH7_EXCEPTION;` |
|         - | 7856 | `				}` |
|         - | 7857 | `			}` |
|        18 | 7858 | `		}` |
|         - | 7859 | `		/* Point to the next entry */` |
|        37 | 7860 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7861 | `	}` |
|        23 | 7862 | `	return PH7_OK;` |
|        12 | 7863 | `}` |
|         - | 7864 | `/*` |
|         - | 7865 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7866 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 7867 | ` * Parameters` |
|         - | 7868 | ` *  $array` |
|         - | 7869 | ` *   The input array.` |
|         - | 7870 | ` *  $funcname` |
|         - | 7871 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7872 | ` *   the first, and the key/index second.` |
|         - | 7873 | ` * Note:` |
|         - | 7874 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7875 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7876 | ` *  be made in the original array itself.` |
|         - | 7877 | ` *  $userdata` |
|         - | 7878 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7879 | ` *   to the callback funcname.` |
|         - | 7880 | ` * Return` |
|         - | 7881 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7882 | ` */` |
|        30 | 7883 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7884 | `{` |
|         - | 7885 | `	ph7_hashmap *pMap;` |
|        35 | 7886 | `	if( nArg < 2 ){` |
|         8 | 7887 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7888 | `			"ArgumentCountError",` |
|         - | 7889 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|         2 | 7890 | `			nArg` |
|         - | 7891 | `			);` |
|         - | 7892 | `	}` |
|        31 | 7893 | `	if( nArg > 3 ){` |
|         4 | 7894 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7895 | `			"ArgumentCountError",` |
|         - | 7896 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 7897 | `			nArg` |
|         - | 7898 | `			);` |
|         - | 7899 | `	}` |
|        29 | 7900 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7901 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7902 | `			"TypeError",` |
|         - | 7903 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7904 | `			ph7_type_name(apArg[0])` |
|         - | 7905 | `			);` |
|         - | 7906 | `	}` |
|        27 | 7907 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7908 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7909 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7910 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7911 | `				"TypeError",` |
|         - | 7912 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7913 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7914 | `				zFunc` |
|         - | 7915 | `				);` |
|         - | 7916 | `		}` |
|        12 | 7917 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7918 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7919 | `				"TypeError",` |
|         - | 7920 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7921 | `				"array callback must have exactly two members"` |
|         - | 7922 | `				);` |
|         - | 7923 | `		}` |
|         6 | 7924 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7925 | `			"TypeError",` |
|         - | 7926 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7927 | `			"no array or string given"` |
|         - | 7928 | `			);` |
|         - | 7929 | `	}` |
|         - | 7930 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 7931 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 7932 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7933 | `	/* Perform the desired operation */` |
|        13 | 7934 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 7935 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7936 | `		return PH7_EXCEPTION;` |
|         - | 7937 | `	}` |
|         - | 7938 | `	/* All done, return TRUE */` |
|        13 | 7939 | `	ph7_result_bool(pCtx,1);` |
|        13 | 7940 | `	return PH7_OK;` |
|        20 | 7941 | `}` |
|         - | 7942 | `/*` |
|         - | 7943 | ` * bool array_is_list(array $array)` |
|         - | 7944 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 7945 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 7946 | ` * Return` |
|         - | 7947 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 7948 | ` */` |
|         - | 7949 | `/*` |
|         - | 7950 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 7951 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 7952 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 7953 | ` */` |
|       216 | 7954 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 7955 | `{` |
|       217 | 7956 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       217 | 7957 | `	sxi64 iExpect = 0;` |
|         - | 7958 | `	sxu32 n;` |
|       473 | 7959 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       339 | 7960 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 7961 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|        83 | 7962 | `			return 0;` |
|         - | 7963 | `		}` |
|       257 | 7964 | `		++iExpect;` |
|       257 | 7965 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       129 | 7966 | `	}` |
|       135 | 7967 | `	return 1;` |
|       109 | 7968 | `}` |
|        12 | 7969 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7970 | `{` |
|        13 | 7971 | `	if( nArg < 1 ){` |
|       ! 0 | 7972 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7973 | `			"ArgumentCountError",` |
|         - | 7974 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 7975 | `			);` |
|         - | 7976 | `	}` |
|        13 | 7977 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 7978 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7979 | `			"TypeError",` |
|         - | 7980 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 7981 | `			ph7_type_name(apArg[0])` |
|         - | 7982 | `			);` |
|         - | 7983 | `	}` |
|        13 | 7984 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 7985 | `	return PH7_OK;` |
|         7 | 7986 | `}` |
|         - | 7987 | `/*` |
|         - | 7988 | ` * mixed array_first(array $array)` |
|         - | 7989 | ` * mixed array_last(array $array)` |
|         - | 7990 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 7991 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 7992 | ` *  untouched (unlike reset()/end()).` |
|         - | 7993 | ` */` |
|        20 | 7994 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 7995 | `{` |
|         - | 7996 | `	ph7_hashmap *pMap;` |
|         - | 7997 | `	ph7_hashmap_node *pNode;` |
|         - | 7998 | `	ph7_value *pVal;` |
|        21 | 7999 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        21 | 8000 | `	if( nArg < 1 ){` |
|         4 | 8001 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8002 | `			"ArgumentCountError",` |
|         - | 8003 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8004 | `			zName` |
|         - | 8005 | `			);` |
|         - | 8006 | `	}` |
|        19 | 8007 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8008 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8009 | `			"TypeError",` |
|         - | 8010 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8011 | `			zName,` |
|         1 | 8012 | `			ph7_type_name(apArg[0])` |
|         - | 8013 | `			);` |
|         - | 8014 | `	}` |
|        17 | 8015 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8016 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8017 | `	if( pNode == 0 ){` |
|         - | 8018 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8019 | `		ph7_result_null(pCtx);` |
|         5 | 8020 | `		return PH7_OK;` |
|         - | 8021 | `	}` |
|        13 | 8022 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8023 | `	if( pVal ){` |
|        13 | 8024 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8025 | `	}else{` |
|       ! 0 | 8026 | `		ph7_result_null(pCtx);` |
|         - | 8027 | `	}` |
|        13 | 8028 | `	return PH7_OK;` |
|        11 | 8029 | `}` |
|        10 | 8030 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8031 | `{` |
|        11 | 8032 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8033 | `}` |
|        10 | 8034 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8035 | `{` |
|        11 | 8036 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8037 | `}` |
|         - | 8038 | `/*` |
|         - | 8039 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8040 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8041 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8042 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8043 | ` *  untouched.` |
|         - | 8044 | ` */` |
|        24 | 8045 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8046 | `{` |
|         - | 8047 | `	ph7_hashmap *pMap;` |
|         - | 8048 | `	ph7_hashmap_node *pNode;` |
|        25 | 8049 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        25 | 8050 | `	if( nArg < 1 ){` |
|         4 | 8051 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8052 | `			"ArgumentCountError",` |
|         - | 8053 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8054 | `			zName` |
|         - | 8055 | `			);` |
|         - | 8056 | `	}` |
|        23 | 8057 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8058 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8059 | `			"TypeError",` |
|         - | 8060 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8061 | `			zName,` |
|         1 | 8062 | `			ph7_type_name(apArg[0])` |
|         - | 8063 | `			);` |
|         - | 8064 | `	}` |
|        21 | 8065 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8066 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8067 | `	if( pNode == 0 ){` |
|         - | 8068 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8069 | `		ph7_result_null(pCtx);` |
|         5 | 8070 | `		return PH7_OK;` |
|         - | 8071 | `	}` |
|        17 | 8072 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8073 | `	return PH7_OK;` |
|        13 | 8074 | `}` |
|        12 | 8075 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8076 | `{` |
|        13 | 8077 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8078 | `}` |
|        12 | 8079 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8080 | `{` |
|        13 | 8081 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8082 | `}` |
|         - | 8083 | `/*` |
|         - | 8084 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8085 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8086 | ` * array_column() for both the column value and the index key.` |
|         - | 8087 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8088 | ` * container or the key is absent.` |
|         - | 8089 | ` */` |
|        32 | 8090 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8091 | `{` |
|        33 | 8092 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8093 | `		ph7_hashmap_node *pNode;` |
|        25 | 8094 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8095 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8096 | `		}` |
|        11 | 8097 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8098 | `		ph7_value sName;` |
|         - | 8099 | `		const char *zName;` |
|         - | 8100 | `		ph7_value *pAttr;` |
|         - | 8101 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8102 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8103 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8104 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8105 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8106 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8107 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8108 | `		return pAttr;` |
|         - | 8109 | `	}` |
|         5 | 8110 | `	return 0;` |
|        17 | 8111 | `}` |
|         - | 8112 | `/*` |
|         - | 8113 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8114 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8115 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8116 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8117 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8118 | ` *  Each row may be an array or an object.` |
|         - | 8119 | ` */` |
|        12 | 8120 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8121 | `{` |
|         - | 8122 | `	ph7_hashmap_node *pNode;` |
|         - | 8123 | `	ph7_hashmap *pMap;` |
|         - | 8124 | `	ph7_value *pArray;` |
|         - | 8125 | `	ph7_value *pRow;` |
|         - | 8126 | `	ph7_value *pCol;` |
|         - | 8127 | `	ph7_value *pIdx;` |
|         - | 8128 | `	int bWantCol;` |
|         - | 8129 | `	int bWantIdx;` |
|         - | 8130 | `	sxu32 n;` |
|        13 | 8131 | `	if( nArg < 2 ){` |
|       ! 0 | 8132 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8133 | `			"ArgumentCountError",` |
|         - | 8134 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8135 | `			nArg` |
|         - | 8136 | `			);` |
|         - | 8137 | `	}` |
|        13 | 8138 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8139 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8140 | `			"TypeError",` |
|         - | 8141 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8142 | `			ph7_type_name(apArg[0])` |
|         - | 8143 | `			);` |
|         - | 8144 | `	}` |
|        13 | 8145 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8146 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8147 | `	if( pArray == 0 ){` |
|       ! 0 | 8148 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8149 | `		return PH7_OK;` |
|         - | 8150 | `	}` |
|         - | 8151 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8152 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8153 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8154 | `	pNode = pMap->pFirst;` |
|        33 | 8155 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8156 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8157 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8158 | `		if( pRow == 0 ){` |
|       ! 0 | 8159 | `			continue;` |
|         - | 8160 | `		}` |
|        21 | 8161 | `		if( bWantCol ){` |
|        19 | 8162 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8163 | `			if( pCol == 0 ){` |
|         - | 8164 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8165 | `				continue;` |
|         - | 8166 | `			}` |
|         9 | 8167 | `		}else{` |
|         3 | 8168 | `			pCol = pRow;` |
|         - | 8169 | `		}` |
|        19 | 8170 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8171 | `		if( pIdx ){` |
|        13 | 8172 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8173 | `		}else{` |
|         7 | 8174 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8175 | `		}` |
|        10 | 8176 | `	}` |
|        13 | 8177 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8178 | `	return PH7_OK;` |
|         7 | 8179 | `}` |
|         - | 8180 | `/*` |
|         - | 8181 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8182 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8183 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8184 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8185 | ` */` |
|        28 | 8186 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8187 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8188 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8189 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8190 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8191 | `	)` |
|         1 | 8192 | `{` |
|         - | 8193 | `	ph7_hashmap_node *pEntry;` |
|         - | 8194 | `	ph7_hashmap *pMap;` |
|         - | 8195 | `	ph7_value *pValue;` |
|         - | 8196 | `	ph7_value *apCbArg[2];` |
|         - | 8197 | `	ph7_value sKey;` |
|         - | 8198 | `	ph7_value sResult;` |
|         - | 8199 | `	sxi32 rc;` |
|         - | 8200 | `	sxu32 n;` |
|        29 | 8201 | `	*ppMatch = 0;` |
|        29 | 8202 | `	if( nArg < 2 ){` |
|       ! 0 | 8203 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8204 | `			"ArgumentCountError",` |
|         - | 8205 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8206 | `			zName,nArg` |
|         - | 8207 | `			);` |
|         - | 8208 | `	}` |
|        29 | 8209 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8210 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8211 | `			"TypeError",` |
|         - | 8212 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8213 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8214 | `			);` |
|         - | 8215 | `	}` |
|        29 | 8216 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8217 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8218 | `			"TypeError",` |
|         - | 8219 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8220 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8221 | `			);` |
|         - | 8222 | `	}` |
|        29 | 8223 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8224 | `	pEntry = pMap->pFirst;` |
|        29 | 8225 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8226 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8227 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8228 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8229 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8230 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8231 | `		if( pValue ){` |
|         - | 8232 | `			/* The callback receives ($value, $key). */` |
|        59 | 8233 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8234 | `			apCbArg[0] = pValue;` |
|        59 | 8235 | `			apCbArg[1] = &sKey;` |
|        59 | 8236 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8237 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8238 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8239 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8240 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8241 | `				return PH7_EXCEPTION;` |
|         - | 8242 | `			}` |
|        59 | 8243 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8244 | `				*ppMatch = pEntry;` |
|        15 | 8245 | `				break;` |
|         - | 8246 | `			}` |
|        22 | 8247 | `		}` |
|        45 | 8248 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8249 | `	}` |
|        29 | 8250 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8251 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8252 | `	return PH7_OK;` |
|        15 | 8253 | `}` |
|         - | 8254 | `/*` |
|         - | 8255 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8256 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8257 | ` *  is truthy, or NULL if none match.` |
|         - | 8258 | ` */` |
|         6 | 8259 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8260 | `{` |
|         - | 8261 | `	ph7_hashmap_node *pMatch;` |
|         - | 8262 | `	ph7_value *pVal;` |
|         - | 8263 | `	sxi32 rc;` |
|         7 | 8264 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8265 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8266 | `		return rc;` |
|         - | 8267 | `	}` |
|         7 | 8268 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8269 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8270 | `	}else{` |
|         3 | 8271 | `		ph7_result_null(pCtx);` |
|         - | 8272 | `	}` |
|         7 | 8273 | `	return PH7_OK;` |
|         4 | 8274 | `}` |
|         - | 8275 | `/*` |
|         - | 8276 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8277 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8278 | ` *  is truthy, or NULL if none match.` |
|         - | 8279 | ` */` |
|         6 | 8280 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8281 | `{` |
|         - | 8282 | `	ph7_hashmap_node *pMatch;` |
|         - | 8283 | `	sxi32 rc;` |
|         7 | 8284 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8285 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8286 | `		return rc;` |
|         - | 8287 | `	}` |
|         7 | 8288 | `	if( pMatch == 0 ){` |
|         3 | 8289 | `		ph7_result_null(pCtx);` |
|         6 | 8290 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8291 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8292 | `	}else{` |
|         4 | 8293 | `		ph7_result_string(pCtx,` |
|         2 | 8294 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8295 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8296 | `	}` |
|         7 | 8297 | `	return PH7_OK;` |
|         4 | 8298 | `}` |
|         - | 8299 | `/*` |
|         - | 8300 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8301 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8302 | ` *  FALSE for an empty array.` |
|         - | 8303 | ` */` |
|         8 | 8304 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8305 | `{` |
|         - | 8306 | `	ph7_hashmap_node *pMatch;` |
|         - | 8307 | `	sxi32 rc;` |
|         9 | 8308 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8309 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8310 | `		return rc;` |
|         - | 8311 | `	}` |
|         9 | 8312 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8313 | `	return PH7_OK;` |
|         5 | 8314 | `}` |
|         - | 8315 | `/*` |
|         - | 8316 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8317 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8318 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8319 | ` */` |
|         8 | 8320 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8321 | `{` |
|         - | 8322 | `	ph7_hashmap_node *pMatch;` |
|         - | 8323 | `	sxi32 rc;` |
|         9 | 8324 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8325 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8326 | `		return rc;` |
|         - | 8327 | `	}` |
|         9 | 8328 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8329 | `	return PH7_OK;` |
|         5 | 8330 | `}` |
|         - | 8331 | `/*` |
|         - | 8332 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8333 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8334 | ` */` |
|         - | 8335 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8336 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        70 | 8337 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8338 | `{` |
|        74 | 8339 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        35 | 8340 | `	(void)pVm;` |
|        74 | 8341 | `	p->nCount++;` |
|        74 | 8342 | `	if( p->pArray ){` |
|         - | 8343 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8344 | `		 * otherwise append with an auto-assigned int index. */` |
|        66 | 8345 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        31 | 8346 | `	}` |
|        74 | 8347 | `	return SXRET_OK;` |
|         4 | 8348 | `}` |
|         - | 8349 | `/*` |
|         - | 8350 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8351 | ` */` |
|        26 | 8352 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8353 | `{` |
|         - | 8354 | `	struct IterCollect sCol;` |
|         - | 8355 | `	ph7_value *pArray;` |
|         - | 8356 | `	sxi32 rc;` |
|        30 | 8357 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8358 | `	pArray = ph7_context_new_array(pCtx);` |
|        30 | 8359 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8360 | `	sCol.pArray = pArray;` |
|        30 | 8361 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        30 | 8362 | `	sCol.nCount = 0;` |
|        30 | 8363 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8364 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8365 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8366 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8367 | `		sxu32 n;` |
|         9 | 8368 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8369 | `			ph7_value sKey, *pVal;` |
|         7 | 8370 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8371 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8372 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8373 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8374 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8375 | `			pEntry = pEntry->pPrev;` |
|         4 | 8376 | `		}` |
|         3 | 8377 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8378 | `		return PH7_OK;` |
|         - | 8379 | `	}` |
|        28 | 8380 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        28 | 8381 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        26 | 8382 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8383 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8384 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8385 | `			ph7_type_name(apArg[0]));` |
|         - | 8386 | `	}` |
|        26 | 8387 | `	ph7_result_value(pCtx,pArray);` |
|        26 | 8388 | `	return PH7_OK;` |
|        17 | 8389 | `}` |
|         - | 8390 | `/*` |
|         - | 8391 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8392 | ` */` |
|         6 | 8393 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8394 | `{` |
|         - | 8395 | `	struct IterCollect sCol;` |
|         - | 8396 | `	sxi32 rc;` |
|         7 | 8397 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         7 | 8398 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8399 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8400 | `		return PH7_OK;` |
|         - | 8401 | `	}` |
|         5 | 8402 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         5 | 8403 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         5 | 8404 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         5 | 8405 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8406 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8407 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8408 | `			ph7_type_name(apArg[0]));` |
|         - | 8409 | `	}` |
|         5 | 8410 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         5 | 8411 | `	return PH7_OK;` |
|         4 | 8412 | `}` |
|         - | 8413 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8414 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8415 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8416 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        24 | 8417 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8418 | `{` |
|        25 | 8419 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8420 | `	ph7_value sResult;` |
|         - | 8421 | `	SySet aArg;` |
|         - | 8422 | `	sxi32 rc;` |
|         - | 8423 | `	int bContinue;` |
|        12 | 8424 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        25 | 8425 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        25 | 8426 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8427 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8428 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8429 | `		sxu32 n;` |
|        17 | 8430 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8431 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8432 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8433 | `			pEntry = pEntry->pPrev;` |
|         5 | 8434 | `		}` |
|         4 | 8435 | `	}` |
|        25 | 8436 | `	PH7_MemObjInit(pVm,&sResult);` |
|        37 | 8437 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        24 | 8438 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        25 | 8439 | `	SySetRelease(&aArg);` |
|        25 | 8440 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        23 | 8441 | `	p->nCount++;` |
|        23 | 8442 | `	PH7_MemObjToBool(&sResult);` |
|        23 | 8443 | `	bContinue = (sResult.x.iVal != 0);` |
|        23 | 8444 | `	PH7_MemObjRelease(&sResult);` |
|        23 | 8445 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        13 | 8446 | `}` |
|         - | 8447 | `/*` |
|         - | 8448 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8449 | ` */` |
|         8 | 8450 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8451 | `{` |
|         - | 8452 | `	struct IterApply sApp;` |
|         - | 8453 | `	sxi32 rc;` |
|         9 | 8454 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8455 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8456 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8457 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8458 | `	}` |
|         9 | 8459 | `	sApp.pCallback = apArg[1];` |
|         9 | 8460 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|         9 | 8461 | `	sApp.nCount = 0;` |
|         9 | 8462 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|         9 | 8463 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8464 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8465 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8466 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8467 | `			ph7_type_name(apArg[0]));` |
|         - | 8468 | `	}` |
|         7 | 8469 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|         7 | 8470 | `	return PH7_OK;` |
|         5 | 8471 | `}` |
|         - | 8472 | `/*` |
|         - | 8473 | ` * Table of hashmap functions.` |
|         - | 8474 | ` */` |
|         - | 8475 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8476 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8477 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8478 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8479 | `	{"count",             ph7_hashmap_count },` |
|         - | 8480 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8481 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8482 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8483 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8484 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8485 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8486 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8487 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8488 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8489 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8490 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8491 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8492 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8493 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8494 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8495 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8496 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8497 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8498 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8499 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8500 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8501 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8502 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8503 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8504 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8505 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8506 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8507 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8508 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8509 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8510 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8511 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8512 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8513 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8514 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8515 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8516 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8517 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8518 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8519 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8520 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8521 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8522 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8523 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8524 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8525 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8526 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8527 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8528 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8529 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8530 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8531 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8532 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8533 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8534 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8535 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8536 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8537 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8538 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8539 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8540 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8541 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8542 | `	{"current",           ph7_hashmap_current },` |
|         - | 8543 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8544 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8545 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8546 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8547 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8548 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8549 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8550 | `};` |
|         - | 8551 | `/*` |
|         - | 8552 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8553 | ` */` |
|      3474 | 8554 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8555 | `{` |
|         - | 8556 | `	sxu32 n;` |
|    260555 | 8557 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    257081 | 8558 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    128543 | 8559 | `	}` |
|      3479 | 8560 | `}` |
|         - | 8561 | `/*` |
|         - | 8562 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8563 | ` * the BLOB given as the first argument.` |
|         - | 8564 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8565 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8566 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8567 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8568 | ` */` |
|         - | 8569 | `/*` |
|         - | 8570 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8571 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8572 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8573 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8574 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8575 | ` */` |
|        26 | 8576 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8577 | `{` |
|        28 | 8578 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8579 | `	ph7_value *pObj;` |
|        28 | 8580 | `	sxu32 n = 0;` |
|         - | 8581 | `	int isRef;` |
|        28 | 8582 | `	sxi32 rc = SXRET_OK;` |
|         - | 8583 | `	int i;` |
|        44 | 8584 | `	for(;;){` |
|        90 | 8585 | `		if( n >= pMap->nEntry ){` |
|        28 | 8586 | `			break;` |
|         - | 8587 | `		}` |
|       126 | 8588 | `		for( i = 0 ; i < nTab ; i++ ){` |
|        64 | 8589 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        33 | 8590 | `		}` |
|         - | 8591 | `		/* Dump key */` |
|        64 | 8592 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|        33 | 8593 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|        17 | 8594 | `		}else{` |
|        47 | 8595 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|        15 | 8596 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8597 | `		}` |
|         - | 8598 | `#ifdef __WINNT__` |
|         2 | 8599 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|         - | 8600 | `#else` |
|        62 | 8601 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8602 | `#endif` |
|         - | 8603 | `		/* Dump node value */` |
|        64 | 8604 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        64 | 8605 | `		isRef = 0;` |
|        64 | 8606 | `		if( pObj ){` |
|        64 | 8607 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|         - | 8608 | `				/* Referenced object */` |
|       ! 0 | 8609 | `				isRef = 1;` |
|       ! 0 | 8610 | `			}` |
|        64 | 8611 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|        64 | 8612 | `			if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8613 | `				break;` |
|         - | 8614 | `			}` |
|        31 | 8615 | `		}` |
|         - | 8616 | `		/* Point to the next entry */` |
|        64 | 8617 | `		n++;` |
|        64 | 8618 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 8619 | `	}` |
|        28 | 8620 | `	return rc;` |
|         2 | 8621 | `}` |
|        22 | 8622 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8623 | `{` |
|         - | 8624 | `	sxi32 rc;` |
|         - | 8625 | `	int i;` |
|        24 | 8626 | `	if( nDepth > 31 ){` |
|         - | 8627 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8628 | `		/* Nesting limit reached */` |
|       ! 0 | 8629 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8630 | `		if( ShowType ){` |
|       ! 0 | 8631 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|       ! 0 | 8632 | `		}` |
|       ! 0 | 8633 | `		return SXERR_LIMIT;` |
|         - | 8634 | `	}` |
|        24 | 8635 | `	if( !ShowType ){` |
|        11 | 8636 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|         5 | 8637 | `	}` |
|         - | 8638 | `	/* Total entries */` |
|        24 | 8639 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|         - | 8640 | `#ifdef __WINNT__` |
|         2 | 8641 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|         - | 8642 | `#else` |
|        22 | 8643 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8644 | `#endif` |
|        24 | 8645 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|        46 | 8646 | `	for( i = 0 ; i < nTab ; i++ ){` |
|        24 | 8647 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        13 | 8648 | `	}` |
|        24 | 8649 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        24 | 8650 | `	return rc;` |
|        13 | 8651 | `}` |
|         - | 8652 | `/*` |
|         - | 8653 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8654 | ` * retrieved entry.` |
|         - | 8655 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8656 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8657 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8658 | ` * a value different from PH7_OK.` |
|         - | 8659 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8660 | ` */` |
|     33526 | 8661 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8662 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8663 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8664 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8665 | `	)` |
|         5 | 8666 | `{` |
|         - | 8667 | `	ph7_hashmap_node *pEntry;` |
|         - | 8668 | `	ph7_value sKey,sValue;` |
|         - | 8669 | `	sxi32 rc;` |
|         - | 8670 | `	sxu32 n;` |
|         - | 8671 | `	/* Initialize walker parameter */` |
|     33531 | 8672 | `	rc = SXRET_OK;` |
|     33531 | 8673 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     33531 | 8674 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     33531 | 8675 | `	n = pMap->nEntry;` |
|     33531 | 8676 | `	pEntry = pMap->pFirst;` |
|         - | 8677 | `	/* Start the iteration process */` |
|     87949 | 8678 | `	for(;;){` |
|    175903 | 8679 | `		if( n < 1 ){` |
|     33531 | 8680 | `			break;` |
|         - | 8681 | `		}` |
|         - | 8682 | `		/* Extract a copy of the key and a copy the current value */` |
|    142377 | 8683 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    142377 | 8684 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8685 | `		/* Invoke the user callback */` |
|    142377 | 8686 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8687 | `		/* Release the copy of the key and the value */` |
|    142377 | 8688 | `		PH7_MemObjRelease(&sKey);` |
|    142377 | 8689 | `		PH7_MemObjRelease(&sValue);` |
|    142377 | 8690 | `		if( rc != PH7_OK ){` |
|         - | 8691 | `			/* Callback request an operation abort */` |
|       ! 0 | 8692 | `			return SXERR_ABORT;` |
|         - | 8693 | `		}` |
|         - | 8694 | `		/* Point to the next entry */` |
|    142377 | 8695 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    142377 | 8696 | `		n--;` |
|         5 | 8697 | `	}` |
|         - | 8698 | `	/* All done */` |
|     33531 | 8699 | `	return SXRET_OK;` |
|     16768 | 8700 | `}` |
|         - | 8701 |  |
