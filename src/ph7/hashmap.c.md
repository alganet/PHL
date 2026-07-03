# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3348/3838 lines (87.23%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/* This file implement generic hashmaps known as 'array' in the PHP world */` |
|       - |    8 | `/* Allowed node types */` |
|       - |    9 | `#define HASHMAP_INT_NODE   1  /* Node with an int [i.e: 64-bit integer] key */` |
|       - |   10 | `#define HASHMAP_BLOB_NODE  2  /* Node with a string/BLOB key */` |
|       - |   11 | `/* Node control flags */` |
|       - |   12 | `#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node hold a reference to a foreign ph7_value` |
|       - |   13 | `                                        * [i.e: array(&var)/$a[] =& $var ]` |
|       - |   14 | `										*/` |
|       - |   15 | `/*` |
|       - |   16 | ` * Default hash function for int [i.e; 64-bit integer] keys.` |
|       - |   17 | ` */` |
| 3125500 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       5 |   19 | `{` |
| 3125505 |   20 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
| 3125505 |   21 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|       5 |   22 | `}` |
|       - |   23 | `/*` |
|       - |   24 | ` * Default hash function for string/BLOB keys.` |
|       - |   25 | ` */` |
|  392520 |   26 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       5 |   27 | `{` |
|  392525 |   28 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   29 | `	unsigned char *zEnd;` |
|  392525 |   30 | `	sxu32 nH = 5381;` |
|  392525 |   31 | `	zEnd = &zIn[nLen];` |
|  445671 |   32 | `	for(;;){` |
|  891347 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  768161 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  691433 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  590245 |   36 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       5 |   37 | `	}` |
|  392525 |   38 | `	return nH;` |
|       5 |   39 | `}` |
|       - |   40 | `/*` |
|       - |   41 | ` * Return the total number of entries in a given hashmap.` |
|       - |   42 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   43 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   44 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   45 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   46 | ` */` |
|     948 |   47 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       5 |   48 | `{` |
|     953 |   49 | `	sxi64 iCount = 0;` |
|     953 |   50 | `	if( !bRecursive ){` |
|     779 |   51 | `		iCount = pMap->nEntry;` |
|     392 |   52 | `	}else{` |
|       - |   53 | `		/* Recursive hashmap walk */` |
|     175 |   54 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|       - |   55 | `		ph7_value *pElem;` |
|     175 |   56 | `		sxu32 n = 0;` |
|       - |   57 | `		/* Mark this map as being counted */` |
|     175 |   58 | `		pMap->iFlags \|= HASHMAP_COUNTING;` |
|     209 |   59 | `		for(;;){` |
|     419 |   60 | `			if( n >= pMap->nEntry ){` |
|     175 |   61 | `				break;` |
|       - |   62 | `			}` |
|       - |   63 | `			/* Point to the element value */` |
|     245 |   64 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|     245 |   65 | `			if( pElem ){` |
|     245 |   66 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|     151 |   67 | `					ph7_hashmap *pSub = (ph7_hashmap *)pElem->x.pOther;` |
|     151 |   68 | `					if( pSub->iFlags & HASHMAP_COUNTING ){` |
|       - |   69 | `						/* Cycle detected — skip this entry */` |
|       3 |   70 | `						if( pCycleDetected ){` |
|       3 |   71 | `							*pCycleDetected = TRUE;` |
|       1 |   72 | `						}` |
|       2 |   73 | `					}else{` |
|     149 |   74 | `						iCount += HashmapCount(pSub,TRUE,pCycleDetected);` |
|       - |   75 | `					}` |
|      75 |   76 | `				}` |
|     122 |   77 | `			}` |
|       - |   78 | `			/* Point to the next entry */` |
|     245 |   79 | `			pEntry = pEntry->pNext;` |
|     245 |   80 | `			++n;` |
|       1 |   81 | `		}` |
|       - |   82 | `		/* Clear the counting flag */` |
|     175 |   83 | `		pMap->iFlags &= ~HASHMAP_COUNTING;` |
|       - |   84 | `		/* Update count */` |
|     175 |   85 | `		iCount += pMap->nEntry;` |
|       - |   86 | `	}` |
|     953 |   87 | `	return iCount;` |
|       5 |   88 | `}` |
|       - |   89 | `/*` |
|       - |   90 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   91 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   92 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   93 | ` */` |
| 3064456 |   94 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       5 |   95 | `{` |
|       - |   96 | `	ph7_hashmap_node *pNode;` |
|       - |   97 | `	/* Allocate a new node */` |
| 3064461 |   98 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 3064461 |   99 | `	if( pNode == 0 ){` |
|     ! 0 |  100 | `		return 0;` |
|       - |  101 | `	}` |
|       - |  102 | `	/* Zero the stucture */` |
| 3064461 |  103 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  104 | `	/* Fill in the structure */` |
| 3064461 |  105 | `	pNode->pMap  = &(*pMap);` |
| 3064461 |  106 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 3064461 |  107 | `	pNode->nHash = nHash;` |
| 3064461 |  108 | `	pNode->xKey.iKey = iKey;` |
| 3064461 |  109 | `	pNode->nValIdx  = nValIdx;` |
| 3064461 |  110 | `	return pNode;` |
| 1532233 |  111 | `}` |
|       - |  112 | `/*` |
|       - |  113 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  114 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  115 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  116 | ` */` |
|  142298 |  117 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       5 |  118 | `{` |
|       - |  119 | `	ph7_hashmap_node *pNode;` |
|       - |  120 | `	/* Allocate a new node */` |
|  142303 |  121 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  142303 |  122 | `	if( pNode == 0 ){` |
|     ! 0 |  123 | `		return 0;` |
|       - |  124 | `	}` |
|       - |  125 | `	/* Zero the stucture */` |
|  142303 |  126 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  127 | `	/* Fill in the structure */` |
|  142303 |  128 | `	pNode->pMap  = &(*pMap);` |
|  142303 |  129 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  142303 |  130 | `	pNode->nHash = nHash;` |
|  142303 |  131 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  142303 |  132 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  142303 |  133 | `	pNode->nValIdx = nValIdx;` |
|  142303 |  134 | `	return pNode;` |
|   71154 |  135 | `}` |
|       - |  136 | `/*` |
|       - |  137 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  138 | ` */` |
| 3206754 |  139 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       5 |  140 | `{` |
|       - |  141 | `	/* Link */` |
| 3206759 |  142 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2845233 |  143 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2845233 |  144 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1422614 |  145 | `	}` |
| 3206759 |  146 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  147 | `	/* Link to the map list */` |
| 3206759 |  148 | `	if( pMap->pFirst == 0 ){` |
|   65357 |  149 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  150 | `		/* Point to the first inserted node */` |
|   65357 |  151 | `		pMap->pCur = pNode;` |
|   32681 |  152 | `	}else{` |
| 3141407 |  153 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  154 | `	}` |
| 3206759 |  155 | `	++pMap->nEntry;` |
| 3206759 |  156 | `}` |
|       - |  157 | `/*` |
|       - |  158 | ` * Unlink a node from the hashmap.` |
|       - |  159 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  160 | ` */` |
|    7180 |  161 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       5 |  162 | `{` |
|    7185 |  163 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    7185 |  164 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  165 | `	/* Unlink from the corresponding bucket */` |
|    7185 |  166 | `	if( pNode->pPrevCollide == 0 ){` |
|    6735 |  167 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3370 |  168 | `	}else{` |
|     451 |  169 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  170 | `	}` |
|    7185 |  171 | `	if( pNode->pNextCollide ){` |
|    5581 |  172 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2790 |  173 | `	}` |
|    7185 |  174 | `	if( pMap->pFirst == pNode ){` |
|     131 |  175 | `		pMap->pFirst = pNode->pPrev;` |
|      63 |  176 | `	}` |
|    7185 |  177 | `	if( pMap->pCur == pNode ){` |
|       - |  178 | `		/* Advance the node cursor */` |
|     133 |  179 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      64 |  180 | `	}` |
|       - |  181 | `	/* Unlink from the map list */` |
|    7185 |  182 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    7185 |  183 | `	if( bRestore ){` |
|       - |  184 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     107 |  185 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  186 | `		/* Restore to the freelist */` |
|     107 |  187 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     107 |  188 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  189 | `		}` |
|      51 |  190 | `	}` |
|    7185 |  191 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    7050 |  192 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3523 |  193 | `	}` |
|    7185 |  194 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    7185 |  195 | `	pMap->nEntry--;` |
|    7185 |  196 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  197 | `		/* Free the hash-bucket */` |
|      75 |  198 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      75 |  199 | `		pMap->apBucket = 0;` |
|      75 |  200 | `		pMap->nSize = 0;` |
|      75 |  201 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      35 |  202 | `	}` |
|    7185 |  203 | `}` |
|       - |  204 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  205 | `/*` |
|       - |  206 | ` * Grow the hash-table and rehash all entries.` |
|       - |  207 | ` */` |
| 3206754 |  208 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       5 |  209 | `{` |
| 3206759 |  210 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   69991 |  211 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  212 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   69991 |  213 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  214 | `		sxu32 nBucket;` |
|       - |  215 | `		sxu32 n;` |
|   69991 |  216 | `		if( nNew < 1 ){` |
|   65357 |  217 | `			nNew = 16;` |
|   32676 |  218 | `		}` |
|       - |  219 | `		/* Allocate a new bucket */` |
|   69991 |  220 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   69991 |  221 | `		if( apNew == 0 ){` |
|     ! 0 |  222 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  223 | `				return SXERR_MEM; /* Fatal */` |
|       - |  224 | `			}` |
|       - |  225 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  226 | `			return SXRET_OK;` |
|       - |  227 | `		}` |
|       - |  228 | `		/* Zero the table */` |
|   69991 |  229 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  230 | `		/* Reflect the change */` |
|   69991 |  231 | `		pMap->apBucket = apNew;` |
|   69991 |  232 | `		pMap->nSize = nNew;` |
|   69991 |  233 | `		if( apOld == 0 ){` |
|       - |  234 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   65357 |  235 | `			return SXRET_OK;` |
|       - |  236 | `		}` |
|       - |  237 | `		/* Rehash old entries */` |
|    4639 |  238 | `		pEntry = pMap->pFirst;` |
|    4639 |  239 | `		n = 0;` |
| 2074669 |  240 | `		for( ;; ){` |
| 4149343 |  241 | `			if( n >= pMap->nEntry ){` |
|    4639 |  242 | `				break;` |
|       - |  243 | `			}` |
|       - |  244 | `			/* Clear the old collision link */` |
| 4144709 |  245 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  246 | `			/* Link to the new bucket */` |
| 4144709 |  247 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4144709 |  248 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3559121 |  249 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3559121 |  250 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1779558 |  251 | `			}` |
| 4144709 |  252 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  253 | `			/* Point to the next entry */` |
| 4144709 |  254 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4144709 |  255 | `			n++;` |
|       5 |  256 | `		}` |
|       - |  257 | `		/* Free the old table */` |
|    4639 |  258 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2317 |  259 | `	}` |
| 3141407 |  260 | `	return SXRET_OK;` |
| 1603382 |  261 | `}` |
|       - |  262 | `/*` |
|       - |  263 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  264 | ` * hashmap.` |
|       - |  265 | ` */` |
| 3064456 |  266 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  267 | `{` |
|       - |  268 | `	ph7_hashmap_node *pNode;` |
|       - |  269 | `	sxu32 nIdx;` |
|       - |  270 | `	sxu32 nHash;` |
|       - |  271 | `	sxi32 rc;` |
| 3064461 |  272 | `	if( !isForeign ){` |
|       - |  273 | `		ph7_value *pObj;` |
|       - |  274 | `		ph7_value sSafeVal;` |
|       - |  275 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  276 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  277 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  278 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  279 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  280 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
| 3064425 |  281 | `		if( pValue ){` |
| 3064423 |  282 | `			sSafeVal = *pValue;` |
| 3064423 |  283 | `			pValue = &sSafeVal;` |
| 1532209 |  284 | `		}` |
|       - |  285 | `		/* Reserve a ph7_value for the value */` |
| 3064425 |  286 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 3064425 |  287 | `		if( pObj == 0 ){` |
|     ! 0 |  288 | `			return SXERR_MEM;` |
|       - |  289 | `		}` |
| 3064425 |  290 | `		if( pValue ){` |
|       - |  291 | `			/* Duplicate the value */` |
| 3064423 |  292 | `			PH7_MemObjStore(pValue,pObj);` |
| 1532209 |  293 | `		}` |
| 3064425 |  294 | `		nIdx = pObj->nIdx;` |
| 1532215 |  295 | `	}else{` |
|      37 |  296 | `		nIdx = nRefIdx;` |
|       - |  297 | `	}` |
|       - |  298 | `	/* Hash the key */` |
| 3064461 |  299 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  300 | `	/* Allocate a new int node */` |
| 3064461 |  301 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 3064461 |  302 | `	if( pNode == 0 ){` |
|     ! 0 |  303 | `		return SXERR_MEM;` |
|       - |  304 | `	}` |
| 3064461 |  305 | `	if( isForeign ){` |
|       - |  306 | `		/* Mark as a foregin entry */` |
|      37 |  307 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      18 |  308 | `	}` |
|       - |  309 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 3064461 |  310 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 3064461 |  311 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  312 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  313 | `		return rc;` |
|       - |  314 | `	}` |
|       - |  315 | `	/* Perform the insertion */` |
| 3064461 |  316 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  317 | `	/* Install in the reference table */` |
| 3064461 |  318 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  319 | `	/* All done */` |
| 3064461 |  320 | `	return SXRET_OK;` |
| 1532233 |  321 | `}` |
|       - |  322 | `/*` |
|       - |  323 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  324 | ` * hashmap.` |
|       - |  325 | ` */` |
|  142298 |  326 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  327 | `{` |
|       - |  328 | `	ph7_hashmap_node *pNode;` |
|       - |  329 | `	sxu32 nHash;` |
|       - |  330 | `	sxu32 nIdx;` |
|       - |  331 | `	sxi32 rc;` |
|  142303 |  332 | `	if( !isForeign ){` |
|       - |  333 | `		ph7_value *pObj;` |
|       - |  334 | `		ph7_value sSafeVal;` |
|       - |  335 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  336 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  337 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  338 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  339 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  340 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   96929 |  341 | `		if( pValue ){` |
|   96639 |  342 | `			sSafeVal = *pValue;` |
|   96639 |  343 | `			pValue = &sSafeVal;` |
|   48317 |  344 | `		}` |
|       - |  345 | `		/* Reserve a ph7_value for the value */` |
|   96929 |  346 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   96929 |  347 | `		if( pObj == 0 ){` |
|     ! 0 |  348 | `			return SXERR_MEM;` |
|       - |  349 | `		}` |
|   96929 |  350 | `		if( pValue ){` |
|       - |  351 | `			/* Duplicate the value */` |
|   96639 |  352 | `			PH7_MemObjStore(pValue,pObj);` |
|   48317 |  353 | `		}` |
|   96929 |  354 | `		nIdx = pObj->nIdx;` |
|   48467 |  355 | `	}else{` |
|   45379 |  356 | `		nIdx = nRefIdx;` |
|       - |  357 | `	}` |
|       - |  358 | `	/* Hash the key */` |
|  142303 |  359 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  360 | `	/* Allocate a new blob node */` |
|  142303 |  361 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  142303 |  362 | `	if( pNode == 0 ){` |
|     ! 0 |  363 | `		return SXERR_MEM;` |
|       - |  364 | `	}` |
|  142303 |  365 | `	if( isForeign ){` |
|       - |  366 | `		/* Mark as a foregin entry */` |
|   45379 |  367 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   22687 |  368 | `	}` |
|       - |  369 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  142303 |  370 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  142303 |  371 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  372 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  373 | `		return rc;` |
|       - |  374 | `	}` |
|       - |  375 | `	/* Perform the insertion */` |
|  142303 |  376 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  377 | `	/* Install in the reference table */` |
|  142303 |  378 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  379 | `	/* All done */` |
|  142303 |  380 | `	return SXRET_OK;` |
|   71154 |  381 | `}` |
|       - |  382 | `/*` |
|       - |  383 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  384 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  385 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  386 | ` */` |
|   48534 |  387 | `static sxi32 HashmapLookupIntKey(` |
|       - |  388 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  389 | `	sxi64 iKey,                /* lookup key */` |
|       - |  390 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  391 | `	)` |
|       5 |  392 | `{` |
|       - |  393 | `	ph7_hashmap_node *pNode;` |
|       - |  394 | `	sxu32 nHash;` |
|   48539 |  395 | `	if( pMap->nEntry < 1 ){` |
|       - |  396 | `		/* Don't bother hashing,there is no entry anyway */` |
|     553 |  397 | `		return SXERR_NOTFOUND;` |
|       - |  398 | `	}` |
|       - |  399 | `	/* Hash the key first */` |
|   47991 |  400 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  401 | `	/* Point to the appropriate bucket */` |
|   47991 |  402 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  403 | `	/* Perform the lookup */` |
|  412350 |  404 | `	for(;;){` |
|  824705 |  405 | `		if( pNode == 0 ){` |
|   46305 |  406 | `			break;` |
|       - |  407 | `		}` |
|  778400 |  408 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  775384 |  409 | `			&& pNode->nHash == nHash` |
|  387032 |  410 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  411 | `				/* Node found */` |
|    1691 |  412 | `				if( ppNode ){` |
|    1677 |  413 | `					*ppNode = pNode;` |
|     836 |  414 | `				}` |
|    1691 |  415 | `				return SXRET_OK;` |
|       - |  416 | `		}` |
|       - |  417 | `		/* Follow the collision link */` |
|  776715 |  418 | `		pNode = pNode->pNextCollide;` |
|       1 |  419 | `	}` |
|       - |  420 | `	/* No such entry */` |
|   46305 |  421 | `	return SXERR_NOTFOUND;` |
|   24272 |  422 | `}` |
|       - |  423 | `/*` |
|       - |  424 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  425 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  426 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  427 | ` */` |
|  266260 |  428 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  429 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  430 | `	const void *pKey,           /* Lookup key */` |
|       - |  431 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  432 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  433 | `	)` |
|       5 |  434 | `{` |
|       - |  435 | `	ph7_hashmap_node *pNode;` |
|       - |  436 | `	sxu32 nHash;` |
|  266265 |  437 | `	if( pMap->nEntry < 1 ){` |
|       - |  438 | `		/* Don't bother hashing,there is no entry anyway */` |
|   16043 |  439 | `		return SXERR_NOTFOUND;` |
|       - |  440 | `	}` |
|       - |  441 | `	/* Hash the key first */` |
|  250227 |  442 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  443 | `	/* Point to the appropriate bucket */` |
|  250227 |  444 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  445 | `	/* Perform the lookup */` |
|  214340 |  446 | `	for(;;){` |
|  428685 |  447 | `		if( pNode == 0 ){` |
|  197023 |  448 | `			break;` |
|       - |  449 | `		}` |
|  231662 |  450 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  230159 |  451 | `			&& pNode->nHash == nHash` |
|  140976 |  452 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   53301 |  453 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  454 | `				/* Node found */` |
|   53209 |  455 | `				if( ppNode ){` |
|   53181 |  456 | `					*ppNode = pNode;` |
|   26588 |  457 | `				}` |
|   53209 |  458 | `				return SXRET_OK;` |
|       - |  459 | `		}` |
|       - |  460 | `		/* Follow the collision link */` |
|  178463 |  461 | `		pNode = pNode->pNextCollide;` |
|       5 |  462 | `	}` |
|       - |  463 | `	/* No such entry */` |
|  197023 |  464 | `	return SXERR_NOTFOUND;` |
|  133135 |  465 | `}` |
|       - |  466 | `/*` |
|       - |  467 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  468 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  469 | ` */` |
|  266398 |  470 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       5 |  471 | `{` |
|  266403 |  472 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  266403 |  473 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  266403 |  474 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  475 | `		/* Octal not decimal number */` |
|       5 |  476 | `		return FALSE;` |
|       - |  477 | `	}` |
|  266399 |  478 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  479 | `		zIn++;` |
|     ! 0 |  480 | `	}` |
|  133530 |  481 | `	for(;;){` |
|  267065 |  482 | `		if( zIn >= zEnd ){` |
|     233 |  483 | `			return TRUE;` |
|       - |  484 | `		}` |
|  266833 |  485 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  133086 |  486 | `			break;` |
|       - |  487 | `		}` |
|     667 |  488 | `		zIn++;` |
|       1 |  489 | `	}` |
|       - |  490 | `	/* Key does not look like a decimal number */` |
|  266167 |  491 | `	return FALSE;` |
|  133204 |  492 | `}` |
|       - |  493 | `/*` |
|       - |  494 | ` * Check if a given key exists in the given hashmap.` |
|       - |  495 | ` * Write a pointer to the target node on success.` |
|       - |  496 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  497 | ` */` |
|  125428 |  498 | `static sxi32 HashmapLookup(` |
|       - |  499 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  500 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  501 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  502 | `	)` |
|       5 |  503 | `{` |
|  125433 |  504 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  505 | `	sxi32 rc;` |
|  125433 |  506 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  123901 |  507 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  508 | `			/* Force a string cast */` |
|     ! 0 |  509 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  510 | `		}` |
|  123901 |  511 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  512 | `			/* Perform a blob lookup */` |
|  123885 |  513 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  123885 |  514 | `			goto result;` |
|       - |  515 | `		}` |
|       8 |  516 | `	}` |
|       - |  517 | `	/* Perform an int lookup */` |
|    1553 |  518 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  519 | `		/* Force an integer cast */` |
|      27 |  520 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  521 | `	}` |
|       - |  522 | `	/* Perform an int lookup */` |
|    1553 |  523 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   62714 |  524 | `result:` |
|  125433 |  525 | `	if( rc == SXRET_OK ){` |
|       - |  526 | `		/* Node found */` |
|   54605 |  527 | `		if( ppNode ){` |
|   54561 |  528 | `			*ppNode = pNode;` |
|   27278 |  529 | `		}` |
|   54605 |  530 | `		return SXRET_OK;` |
|       - |  531 | `	}` |
|       - |  532 | `	/* No such entry */` |
|   70833 |  533 | `	return SXERR_NOTFOUND;` |
|   62719 |  534 | `}` |
|       - |  535 | `/*` |
|       - |  536 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  537 | ` * hashmap.` |
|       - |  538 | ` * If a node with the given key already exists in the database` |
|       - |  539 | ` * then this function overwrite the old value.` |
|       - |  540 | ` */` |
| 3161084 |  541 | `static sxi32 HashmapInsert(` |
|       - |  542 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  543 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  544 | `	ph7_value *pVal    /* Node value */` |
|       - |  545 | `	)` |
|       5 |  546 | `{` |
| 3161089 |  547 | `	ph7_hashmap_node *pNode = 0;` |
| 3161089 |  548 | `	sxi32 rc = SXRET_OK;` |
| 3161089 |  549 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  100569 |  550 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  551 | `			/* Force a string cast */` |
|       3 |  552 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  553 | `		}` |
|  100569 |  554 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    3663 |  555 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  556 | `				/* Automatic index assign */` |
|    3441 |  557 | `				pKey = 0;` |
|    1718 |  558 | `			}` |
|    3663 |  559 | `			goto IntKey;` |
|       - |  560 | `		}` |
|  145364 |  561 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   48453 |  562 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  563 | `				/* Overwrite the old value */` |
|       - |  564 | `				ph7_value *pElem;` |
|      81 |  565 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      81 |  566 | `				if( pElem ){` |
|      81 |  567 | `					if( pVal ){` |
|      81 |  568 | `						PH7_MemObjStore(pVal,pElem);` |
|      42 |  569 | `					}else{` |
|       - |  570 | `						/* Nullify the entry */` |
|     ! 0 |  571 | `						PH7_MemObjToNull(pElem);` |
|       - |  572 | `					}` |
|      39 |  573 | `				}` |
|      81 |  574 | `				return SXRET_OK;` |
|       - |  575 | `		}` |
|   96833 |  576 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  577 | `			/* Forbidden */` |
|       3 |  578 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  579 | `			return SXRET_OK;` |
|       - |  580 | `		}` |
|       - |  581 | `		/* Perform a blob-key insertion */` |
|   96831 |  582 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   96831 |  583 | `		return rc;` |
|       - |  584 | `	}` |
| 1530260 |  585 | `IntKey:` |
| 3064183 |  586 | `	if( pKey ){` |
|   23617 |  587 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  588 | `			/* Force an integer cast */` |
|     251 |  589 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  590 | `		}` |
|   23617 |  591 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  592 | `			/* Overwrite the old value */` |
|       - |  593 | `			ph7_value *pElem;` |
|      87 |  594 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      87 |  595 | `			if( pElem ){` |
|      87 |  596 | `				if( pVal ){` |
|      87 |  597 | `					PH7_MemObjStore(pVal,pElem);` |
|      44 |  598 | `				}else{` |
|       - |  599 | `					/* Nullify the entry */` |
|     ! 0 |  600 | `					PH7_MemObjToNull(pElem);` |
|       - |  601 | `				}` |
|      43 |  602 | `			}` |
|      87 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|   23531 |  605 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  606 | `			/* Forbidden */` |
|       3 |  607 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  608 | `			return SXRET_OK;` |
|       - |  609 | `		}` |
|       - |  610 | `		/* Perform a 64-bit-int-key insertion */` |
|   23529 |  611 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23529 |  612 | `		if( rc == SXRET_OK ){` |
|   23529 |  613 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  614 | `				/* Increment the automatic index. Like Zend's nNextFreeElement, the` |
|       - |  615 | `				 * index saturates at PHP_INT_MAX (incrementing past it is signed` |
|       - |  616 | `				 * overflow); the occupied-slot case errors at append time below. */` |
|   23287 |  617 | `				pMap->iNextIdx = pKey->x.iVal < SXI64_HIGH ? pKey->x.iVal + 1 : SXI64_HIGH;` |
|       - |  618 | `				/* Make sure the automatic index is not reserved */` |
|   23287 |  619 | `				while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  620 | `					pMap->iNextIdx++;` |
|     ! 0 |  621 | `				}` |
|   11641 |  622 | `			}` |
|   11762 |  623 | `		}` |
|   11767 |  624 | `	}else{` |
| 3040571 |  625 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  626 | `			/* Forbidden */` |
|       3 |  627 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  628 | `			return SXRET_OK;` |
|       - |  629 | `		}` |
| 3040569 |  630 | `		if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),SXI64_HIGH,0) ){` |
|       - |  631 | `			/* The saturated automatic index is taken: php throws a catchable Error` |
|       - |  632 | `			 * here; PHL reports it as a runtime error (divergence recorded in` |
|       - |  633 | `			 * PLAN.md §3 — catchability needs exception plumbing in the store` |
|       - |  634 | `			 * opcodes). */` |
|       3 |  635 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,` |
|       - |  636 | `				"Cannot add element to the array as the next element is already occupied");` |
|       3 |  637 | `			return SXRET_OK;` |
|       - |  638 | `		}` |
|       - |  639 | `		/* Assign an automatic index */` |
| 3040567 |  640 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 3040567 |  641 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
| 3040565 |  642 | `			++pMap->iNextIdx;` |
| 1520280 |  643 | `		}` |
|       - |  644 | `	}` |
|       - |  645 | `	/* Insertion result */` |
| 3064091 |  646 | `	return rc;` |
| 1580547 |  647 | `}` |
|       - |  648 | `/*` |
|       - |  649 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - |  650 | ` * hashmap.` |
|       - |  651 | ` * This is insertion by reference so be careful to mark the node` |
|       - |  652 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - |  653 | ` * The insertion by reference is triggered when the following` |
|       - |  654 | ` * expression is encountered.` |
|       - |  655 | ` * $var = 10;` |
|       - |  656 | ` *  $a = array(&var);` |
|       - |  657 | ` * OR` |
|       - |  658 | ` *  $a[] =& $var;` |
|       - |  659 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - |  660 | ` * over it's contents.` |
|       - |  661 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - |  662 | ` * removed when the foreign ph7_value is unset.` |
|       - |  663 | ` * Example:` |
|       - |  664 | ` *  $var = 10;` |
|       - |  665 | ` *  $a[] =& $var;` |
|       - |  666 | ` *  echo count($a).PHP_EOL; //1` |
|       - |  667 | ` *  //Unset the foreign ph7_value now` |
|       - |  668 | ` *  unset($var);` |
|       - |  669 | ` *  echo count($a); //0` |
|       - |  670 | ` * Note that this is a PH7 eXtension.` |
|       - |  671 | ` * Refer to the official documentation for more information.` |
|       - |  672 | ` * If a node with the given key already exists in the database` |
|       - |  673 | ` * then this function overwrite the old value.` |
|       - |  674 | ` */` |
|   45416 |  675 | `static sxi32 HashmapInsertByRef(` |
|       - |  676 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  677 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  678 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  679 | `	)` |
|       5 |  680 | `{` |
|   45421 |  681 | `	ph7_hashmap_node *pNode = 0;` |
|   45421 |  682 | `	sxi32 rc = SXRET_OK;` |
|   45421 |  683 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   45385 |  684 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  685 | `			/* Force a string cast */` |
|     ! 0 |  686 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  687 | `		}` |
|   45385 |  688 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  689 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  690 | `				/* Automatic index assign */` |
|     ! 0 |  691 | `				pKey = 0;` |
|     ! 0 |  692 | `			}` |
|     ! 0 |  693 | `			goto IntKey;` |
|       - |  694 | `		}` |
|   68075 |  695 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   22690 |  696 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  697 | `				/* Overwrite */` |
|       7 |  698 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  699 | `				pNode->nValIdx = nRefIdx;` |
|       - |  700 | `				/* Install in the reference table */` |
|       7 |  701 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  702 | `				return SXRET_OK;` |
|       - |  703 | `		}` |
|       - |  704 | `		/* Perform a blob-key insertion */` |
|   45379 |  705 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   45379 |  706 | `		return rc;` |
|       - |  707 | `	}` |
|      18 |  708 | `IntKey:` |
|      37 |  709 | `	if( pKey ){` |
|       5 |  710 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  711 | `			/* Force an integer cast */` |
|     ! 0 |  712 | `			PH7_MemObjToInteger(pKey);` |
|     ! 0 |  713 | `		}` |
|       5 |  714 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  715 | `			/* Overwrite */` |
|     ! 0 |  716 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|     ! 0 |  717 | `			pNode->nValIdx = nRefIdx;` |
|       - |  718 | `			/* Install in the reference table */` |
|     ! 0 |  719 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|     ! 0 |  720 | `			return SXRET_OK;` |
|       - |  721 | `		}` |
|       - |  722 | `		/* Perform a 64-bit-int-key insertion */` |
|       5 |  723 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|       5 |  724 | `		if( rc == SXRET_OK ){` |
|       5 |  725 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  726 | `				/* Increment the automatic index (saturating — see PH7_HashmapInsert) */` |
|       5 |  727 | `				pMap->iNextIdx = pKey->x.iVal < SXI64_HIGH ? pKey->x.iVal + 1 : SXI64_HIGH;` |
|       - |  728 | `				/* Make sure the automatic index is not reserved */` |
|       5 |  729 | `				while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  730 | `					pMap->iNextIdx++;` |
|     ! 0 |  731 | `				}` |
|       2 |  732 | `			}` |
|       2 |  733 | `		}` |
|       3 |  734 | `	}else{` |
|      33 |  735 | `		if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),SXI64_HIGH,0) ){` |
|       - |  736 | `			/* Saturated automatic index taken (see PH7_HashmapInsert) */` |
|     ! 0 |  737 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,` |
|       - |  738 | `				"Cannot add element to the array as the next element is already occupied");` |
|     ! 0 |  739 | `			return SXRET_OK;` |
|       - |  740 | `		}` |
|       - |  741 | `		/* Assign an automatic index */` |
|      33 |  742 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|      33 |  743 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|      33 |  744 | `			++pMap->iNextIdx;` |
|      16 |  745 | `		}` |
|       - |  746 | `	}` |
|       - |  747 | `	/* Insertion result */` |
|      37 |  748 | `	return rc;` |
|   22713 |  749 | `}` |
|       - |  750 | `/*` |
|       - |  751 | ` * Extract node value.` |
|       - |  752 | ` */` |
| 1330722 |  753 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       5 |  754 | `{` |
|       - |  755 | `	/* Point to the desired object */` |
|       - |  756 | `	ph7_value *pObj;` |
| 1330727 |  757 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1330727 |  758 | `	return pObj;` |
|       5 |  759 | `}` |
|       - |  760 | `/*` |
|       - |  761 | ` * Insert a node in the given hashmap.` |
|       - |  762 | ` * If a node with the given key already exists in the database` |
|       - |  763 | ` * then this function overwrite the old value.` |
|       - |  764 | ` */` |
|     446 |  765 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       5 |  766 | `{` |
|       - |  767 | `	ph7_value *pObj;` |
|       - |  768 | `	sxi32 rc;` |
|       - |  769 | `	/* Extract the node value */` |
|     451 |  770 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     451 |  771 | `	if( pObj == 0 ){` |
|     ! 0 |  772 | `		return SXERR_EMPTY;` |
|       - |  773 | `	}` |
|       - |  774 | `	/* Preserve key */` |
|     451 |  775 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  776 | `		/* Int64 key */` |
|     321 |  777 | `		if( !bPreserve ){` |
|       - |  778 | `			/* Assign an automatic index */` |
|     173 |  779 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      89 |  780 | `		}else{` |
|     149 |  781 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  782 | `		}` |
|     163 |  783 | `	}else{` |
|       - |  784 | `		/* Blob key */` |
|     131 |  785 | `		if( !bPreserve ){` |
|       - |  786 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  787 | `			 * original string key entirely */` |
|      35 |  788 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      18 |  789 | `		}else{` |
|     145 |  790 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      48 |  791 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  792 | `		}` |
|       - |  793 | `	}` |
|     451 |  794 | `	return rc;` |
|     228 |  795 | `}` |
|       - |  796 | `/*` |
|       - |  797 | ` * Compare two node values.` |
|       - |  798 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  799 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  800 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  801 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  802 | ` * documenation.` |
|       - |  803 | ` */` |
|   67781 |  804 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       5 |  805 | `{` |
|       - |  806 | `	ph7_value sObj1,sObj2;` |
|       - |  807 | `	sxi32 rc;` |
|   67786 |  808 | `	if( pLeft == pRight ){` |
|       - |  809 | `		/*` |
|       - |  810 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  811 | `		 * below for more information on this sceanario.` |
|       - |  812 | `		 */` |
|     ! 0 |  813 | `		return 0;` |
|       - |  814 | `	}` |
|       - |  815 | `	/* Do the comparison */` |
|   67786 |  816 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   67786 |  817 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   67786 |  818 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   67786 |  819 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   67786 |  820 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   67786 |  821 | `	PH7_MemObjRelease(&sObj1);` |
|   67786 |  822 | `	PH7_MemObjRelease(&sObj2);` |
|   67786 |  823 | `	return rc;` |
|   33925 |  824 | `}` |
|       - |  825 | `/*` |
|       - |  826 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  827 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  828 | ` */` |
|   13058 |  829 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       5 |  830 | `{` |
|   13063 |  831 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  832 | `	sxu32 nBucket;` |
|       - |  833 | `	/* Remove old collision links */` |
|   13063 |  834 | `	if( pEntry->pPrevCollide ){` |
|   10683 |  835 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    5345 |  836 | `	}else{` |
|    2385 |  837 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  838 | `	}` |
|   13063 |  839 | `	if( pEntry->pNextCollide ){` |
|    1050 |  840 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     528 |  841 | `	}` |
|   13063 |  842 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  843 | `	/* Compute the new hash */` |
|   13063 |  844 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   13063 |  845 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   13063 |  846 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  847 | `	/* Link to the new bucket */` |
|   13063 |  848 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   13063 |  849 | `	if( pMap->apBucket[nBucket] ){` |
|   11005 |  850 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    5498 |  851 | `	}` |
|   13063 |  852 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   13063 |  853 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  854 | `	/* Increment the automatic index */` |
|   13063 |  855 | `	pMap->iNextIdx++;` |
|   13063 |  856 | `}` |
|       - |  857 | `/*` |
|       - |  858 | ` * Perform a linear search on a given hashmap.` |
|       - |  859 | ` * Write a pointer to the target node on success.` |
|       - |  860 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  861 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  862 | ` * for more information.` |
|       - |  863 | ` */` |
|   32492 |  864 | `static int HashmapFindValue(` |
|       - |  865 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  866 | `	ph7_value *pNeedle,  /* Lookup key */` |
|       - |  867 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|       - |  868 | `	int bStrict      /* TRUE for strict comparison */` |
|       - |  869 | `	)` |
|       5 |  870 | `{` |
|       - |  871 | `	ph7_hashmap_node *pEntry;` |
|       - |  872 | `	ph7_value sVal,*pVal;` |
|       - |  873 | `	ph7_value sNeedle;` |
|       - |  874 | `	sxi32 rc;` |
|       - |  875 | `	sxu32 n;` |
|       - |  876 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|   32497 |  877 | `	pEntry = pMap->pFirst;` |
|   32497 |  878 | `	n = pMap->nEntry;` |
|   32497 |  879 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   32497 |  880 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   77668 |  881 | `	for(;;){` |
|  155343 |  882 | `		if( n < 1 ){` |
|      99 |  883 | `			break;` |
|       - |  884 | `		}` |
|       - |  885 | `		/* Extract node value */` |
|  155245 |  886 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  155245 |  887 | `		if( pVal ){` |
|  155245 |  888 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|     ! 0 |  889 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  890 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  891 | `				if( iF1 == iF2 ){` |
|       - |  892 | `					/* NULL values are equals */` |
|     ! 0 |  893 | `					if( ppNode ){` |
|     ! 0 |  894 | `						*ppNode = pEntry;` |
|     ! 0 |  895 | `					}` |
|     ! 0 |  896 | `					return SXRET_OK;` |
|       - |  897 | `				}` |
|     ! 0 |  898 | `			}else{` |
|       - |  899 | `				/* Duplicate value */` |
|  155245 |  900 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  155245 |  901 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  155245 |  902 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  155245 |  903 | `				PH7_MemObjRelease(&sVal);` |
|  155245 |  904 | `				PH7_MemObjRelease(&sNeedle);` |
|  155245 |  905 | `				if( rc == 0 ){` |
|   32399 |  906 | `					if( ppNode ){` |
|      23 |  907 | `						*ppNode = pEntry;` |
|      11 |  908 | `					}` |
|       - |  909 | `					/* Match found*/` |
|   32399 |  910 | `					return SXRET_OK;` |
|       - |  911 | `				}` |
|       - |  912 | `			}` |
|   61422 |  913 | `		}` |
|       - |  914 | `		/* Point to the next entry */` |
|  122851 |  915 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  122851 |  916 | `		n--;` |
|       5 |  917 | `	}` |
|       - |  918 | `	/* No such entry */` |
|      99 |  919 | `	return SXERR_NOTFOUND;` |
|   16251 |  920 | `}` |
|       - |  921 | `/*` |
|       - |  922 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  923 | ` * for values comparison.` |
|       - |  924 | ` * Write a pointer to the target node on success.` |
|       - |  925 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  926 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  927 | ` * for more information.` |
|       - |  928 | ` */` |
|      22 |  929 | `static int HashmapFindValueByCallback(` |
|       - |  930 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|       - |  931 | `	ph7_value *pNeedle,    /* Lookup key */` |
|       - |  932 | `	ph7_value *pCallback,  /* User defined callback */` |
|       - |  933 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|       - |  934 | `	)` |
|       1 |  935 | `{` |
|       - |  936 | `	ph7_hashmap_node *pEntry;` |
|       - |  937 | `	ph7_value sResult,*pVal;` |
|       - |  938 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|       - |  939 | `	sxi32 rc;` |
|       - |  940 | `	sxu32 n;` |
|      23 |  941 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|       - |  942 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|       - |  943 | `		 * exception is not thrown again, and let the caller wind down. */` |
|     ! 0 |  944 | `		return SXERR_NOTFOUND;` |
|       - |  945 | `	}` |
|       - |  946 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|      23 |  947 | `	pEntry = pMap->pFirst;` |
|      23 |  948 | `	n = pMap->nEntry;` |
|       - |  949 | `	/* Store callback result here */` |
|      23 |  950 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  951 | `	/* First argument to the callback */` |
|      23 |  952 | `	apArg[0] = pNeedle;` |
|      25 |  953 | `	for(;;){` |
|      51 |  954 | `		if( n < 1 ){` |
|       9 |  955 | `			break;` |
|       - |  956 | `		}` |
|       - |  957 | `		/* Extract node value */` |
|      43 |  958 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      43 |  959 | `		if( pVal ){` |
|       - |  960 | `			/* Invoke the user callback */` |
|      43 |  961 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      43 |  962 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      43 |  963 | `			if( rc == PH7_EXCEPTION ){` |
|       - |  964 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|       - |  965 | `				 * and report no match for the rest of the run. */` |
|       5 |  966 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|       5 |  967 | `				PH7_MemObjRelease(&sResult);` |
|       5 |  968 | `				return SXERR_NOTFOUND;` |
|       - |  969 | `			}` |
|      39 |  970 | `			if( rc == SXRET_OK ){` |
|       - |  971 | `				/* Extract callback result */` |
|      39 |  972 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  973 | `					/* Perform an int cast */` |
|     ! 0 |  974 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  975 | `				}` |
|      39 |  976 | `				rc = (sxi32)sResult.x.iVal;` |
|      39 |  977 | `				PH7_MemObjRelease(&sResult);` |
|      39 |  978 | `				if( rc == 0 ){` |
|       - |  979 | `					/* Match found*/` |
|      11 |  980 | `					if( ppNode ){` |
|     ! 0 |  981 | `						*ppNode = pEntry;` |
|     ! 0 |  982 | `					}` |
|      11 |  983 | `					return SXRET_OK;` |
|       - |  984 | `				}` |
|      14 |  985 | `			}` |
|      14 |  986 | `		}` |
|       - |  987 | `		/* Point to the next entry */` |
|      29 |  988 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 |  989 | `		n--;` |
|       1 |  990 | `	}` |
|       - |  991 | `	/* No such entry */` |
|       9 |  992 | `	return SXERR_NOTFOUND;` |
|      12 |  993 | `}` |
|       - |  994 | `/*` |
|       - |  995 | ` * Compare two hashmaps.` |
|       - |  996 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|       - |  997 | ` * Note on array comparison operators.` |
|       - |  998 | ` *  According to the PHP language reference manual.` |
|       - |  999 | ` *  Array Operators Example 	Name 	Result` |
|       - | 1000 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|       - | 1001 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|       - | 1002 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|       - | 1003 | ` *                          order and of the same types.` |
|       - | 1004 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - | 1005 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - | 1006 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|       - | 1007 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1008 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1009 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1010 | ` * <?php` |
|       - | 1011 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1012 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1013 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1014 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1015 | ` * var_dump($c);` |
|       - | 1016 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1017 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1018 | ` * var_dump($c);` |
|       - | 1019 | ` * ?>` |
|       - | 1020 | ` * When executed, this script will print the following:` |
|       - | 1021 | ` * Union of $a and $b:` |
|       - | 1022 | ` * array(3) {` |
|       - | 1023 | ` *  ["a"]=>` |
|       - | 1024 | ` *  string(5) "apple"` |
|       - | 1025 | ` *  ["b"]=>` |
|       - | 1026 | ` * string(6) "banana"` |
|       - | 1027 | ` *  ["c"]=>` |
|       - | 1028 | ` * string(6) "cherry"` |
|       - | 1029 | ` * }` |
|       - | 1030 | ` * Union of $b and $a:` |
|       - | 1031 | ` * array(3) {` |
|       - | 1032 | ` * ["a"]=>` |
|       - | 1033 | ` * string(4) "pear"` |
|       - | 1034 | ` * ["b"]=>` |
|       - | 1035 | ` * string(10) "strawberry"` |
|       - | 1036 | ` * ["c"]=>` |
|       - | 1037 | ` * string(6) "cherry"` |
|       - | 1038 | ` * }` |
|       - | 1039 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|       - | 1040 | ` */` |
|      26 | 1041 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - | 1042 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - | 1043 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - | 1044 | `	int bStrict          /* TRUE for strict comparison */` |
|       - | 1045 | `	)` |
|       1 | 1046 | `{` |
|       - | 1047 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - | 1048 | `	sxi32 rc;` |
|       - | 1049 | `	sxu32 n;` |
|      27 | 1050 | `	if( pLeft == pRight ){` |
|       - | 1051 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - | 1052 | `		 * Unlike the zend engine.` |
|       - | 1053 | `		 */` |
|     ! 0 | 1054 | `		return 0;` |
|       - | 1055 | `	}` |
|      27 | 1056 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - | 1057 | `		/* Must have the same number of entries */` |
|       5 | 1058 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1059 | `	}` |
|       - | 1060 | `	/* Point to the first inserted entry of the left hashmap */` |
|      23 | 1061 | `	pLe = pLeft->pFirst;` |
|      23 | 1062 | `	pRe = 0; /* cc warning */` |
|       - | 1063 | `	/* Perform the comparison */` |
|      23 | 1064 | `	n = pLeft->nEntry;` |
|      27 | 1065 | `	for(;;){` |
|      55 | 1066 | `		if( n < 1 ){` |
|      21 | 1067 | `			break;` |
|       - | 1068 | `		}` |
|      35 | 1069 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1070 | `			/* Int key */` |
|      27 | 1071 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|      14 | 1072 | `		}else{` |
|       9 | 1073 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1074 | `			/* Blob key */` |
|       9 | 1075 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1076 | `		}` |
|      35 | 1077 | `		if( rc != SXRET_OK ){` |
|       - | 1078 | `			/* No such entry in the right side */` |
|     ! 0 | 1079 | `			return 1;` |
|       - | 1080 | `		}` |
|      35 | 1081 | `		rc = 0;` |
|      35 | 1082 | `		if( bStrict ){` |
|       - | 1083 | `			/* Make sure,the keys are of the same type */` |
|      19 | 1084 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1085 | `				rc = 1;` |
|     ! 0 | 1086 | `			}` |
|       9 | 1087 | `		}` |
|      35 | 1088 | `		if( !rc ){` |
|       - | 1089 | `			/* Compare nodes */` |
|      35 | 1090 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|      17 | 1091 | `		}` |
|      35 | 1092 | `		if( rc != 0 ){` |
|       - | 1093 | `			/* Nodes key/value differ */` |
|       3 | 1094 | `			return rc;` |
|       - | 1095 | `		}` |
|       - | 1096 | `		/* Point to the next entry */` |
|      33 | 1097 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      33 | 1098 | `		n--;` |
|       1 | 1099 | `	}` |
|      21 | 1100 | `	return 0; /* Hashmaps are equals */` |
|      14 | 1101 | `}` |
|       - | 1102 | `/*` |
|       - | 1103 | ` * Duplicate a hashmap node.` |
|       - | 1104 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1105 | ` */` |
|  613558 | 1106 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1107 | `	ph7_hashmap *pDest,` |
|       - | 1108 | `	ph7_hashmap_node *pEntry,` |
|       - | 1109 | `	ph7_value *pVal,` |
|       - | 1110 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1111 | `	)` |
|       5 | 1112 | `{` |
|       - | 1113 | `	ph7_value sSafeVal;` |
|       - | 1114 | `	ph7_value sKey;` |
|       - | 1115 | `	sxi32 rc;` |
|       - | 1116 |  |
|  613563 | 1117 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 1118 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|       - | 1119 | `		 * Re-insert it by reference so the reference survives the duplication` |
|       - | 1120 | `		 * instead of being flattened to a value copy. This keeps spread` |
|       - | 1121 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|       - | 1122 | `		 * with PHP semantics. */` |
|       7 | 1123 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|       7 | 1124 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       3 | 1125 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|       3 | 1126 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|       3 | 1127 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       3 | 1128 | `			PH7_MemObjRelease(&sKey);` |
|       2 | 1129 | `		}else{` |
|       5 | 1130 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|       5 | 1131 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|       2 | 1132 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|     ! 0 | 1133 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|     ! 0 | 1134 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|     ! 0 | 1135 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 1136 | `			}else{ /* Dup: preserve the int key */` |
|     ! 0 | 1137 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|       - | 1138 | `			}` |
|       - | 1139 | `		}` |
|       7 | 1140 | `		return rc;` |
|       - | 1141 | `	}` |
|  613557 | 1142 | `	sSafeVal = *pVal;` |
|       - | 1143 |  |
|  613557 | 1144 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1145 | `		/* Blob key insertion */` |
|     101 | 1146 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|     101 | 1147 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|     101 | 1148 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|     101 | 1149 | `		PH7_MemObjRelease(&sKey);` |
|      51 | 1150 | `	}else{` |
|       - | 1151 | `		/* Int key */` |
|  613457 | 1152 | `		if( iAction == 0 ){ /* Merge */` |
|  613251 | 1153 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  306832 | 1154 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1155 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1156 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1157 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1158 | `		}else{ /* Dup */` |
|     178 | 1159 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1160 | `		}` |
|       - | 1161 | `	}` |
|  613557 | 1162 | `	return rc;` |
|  306784 | 1163 | `}` |
|       - | 1164 | `/*` |
|       - | 1165 | ` * Merge two hashmaps.` |
|       - | 1166 | ` * Note on the merge process` |
|       - | 1167 | ` * According to the PHP language reference manual.` |
|       - | 1168 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|       - | 1169 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|       - | 1170 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|       - | 1171 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|       - | 1172 | ` *  the later value will not overwrite the original value, but will be appended.` |
|       - | 1173 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|       - | 1174 | ` *  keys starting from zero in the result array.` |
|       - | 1175 | ` */` |
|    2104 | 1176 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       5 | 1177 | `{` |
|       - | 1178 | `	ph7_hashmap_node *pEntry;` |
|       - | 1179 | `	ph7_value *pVal;` |
|       - | 1180 | `	sxi32 rc;` |
|       - | 1181 | `	sxu32 n;` |
|    2109 | 1182 | `	if( pSrc == pDest ){` |
|       - | 1183 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1184 | `		 * Unlike the zend engine.` |
|       - | 1185 | `		 */` |
|     ! 0 | 1186 | `		return SXRET_OK;` |
|       - | 1187 | `	}` |
|       - | 1188 | `	/* Point to the first inserted entry in the source */` |
|    2109 | 1189 | `	pEntry = pSrc->pFirst;` |
|       - | 1190 | `	/* Perform the merge */` |
|  615413 | 1191 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1192 | `		/* Extract the node value */` |
|  613309 | 1193 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  613309 | 1194 | `		if( pVal ){` |
|       - | 1195 | `			/* Make a local copy of the value.` |
|       - | 1196 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1197 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1198 | `			 * to the old pool.` |
|       - | 1199 | `			 */` |
|  613309 | 1200 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  306657 | 1201 | `		}else{` |
|     ! 0 | 1202 | `			rc = SXRET_OK;` |
|       - | 1203 | `		}` |
|  613309 | 1204 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1205 | `			return rc;` |
|       - | 1206 | `		}` |
|       - | 1207 | `		/* Point to the next entry */` |
|  613309 | 1208 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  306657 | 1209 | `	}` |
|    2109 | 1210 | `	return SXRET_OK;` |
|    1057 | 1211 | `}` |
|       - | 1212 | `/*` |
|       - | 1213 | ` * Overwrite entries with the same key.` |
|       - | 1214 | ` * Refer to the [array_replace()] implementation for more information.` |
|       - | 1215 | ` *  According to the PHP language reference manual.` |
|       - | 1216 | ` *  array_replace() replaces the values of the first array with the same values` |
|       - | 1217 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|       - | 1218 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|       - | 1219 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|       - | 1220 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|       - | 1221 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|       - | 1222 | ` *  overwriting the previous values.` |
|       - | 1223 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|       - | 1224 | ` *  by whatever type is in the second array.` |
|       - | 1225 | ` */` |
|      34 | 1226 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1227 | `{` |
|       - | 1228 | `	ph7_hashmap_node *pEntry;` |
|       - | 1229 | `	ph7_value *pVal;` |
|       - | 1230 | `	sxi32 rc;` |
|       - | 1231 | `	sxu32 n;` |
|      36 | 1232 | `	if( pSrc == pDest ){` |
|       - | 1233 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1234 | `		 * Unlike the zend engine.` |
|       - | 1235 | `		 */` |
|     ! 0 | 1236 | `		return SXRET_OK;` |
|       - | 1237 | `	}` |
|       - | 1238 | `	/* Point to the first inserted entry in the source */` |
|      36 | 1239 | `	pEntry = pSrc->pFirst;` |
|       - | 1240 | `	/* Perform the merge */` |
|      80 | 1241 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1242 | `		/* Extract the node value */` |
|      46 | 1243 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      46 | 1244 | `		if( pVal ){` |
|      46 | 1245 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      24 | 1246 | `		}else{` |
|     ! 0 | 1247 | `			rc = SXRET_OK;` |
|       - | 1248 | `		}` |
|      46 | 1249 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1250 | `			return rc;` |
|       - | 1251 | `		}` |
|       - | 1252 | `		/* Point to the next entry */` |
|      46 | 1253 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 1254 | `	}` |
|      36 | 1255 | `	return SXRET_OK;` |
|      19 | 1256 | `}` |
|       - | 1257 | `/*` |
|       - | 1258 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|       - | 1259 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|       - | 1260 | ` */` |
|     108 | 1261 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1262 | `{` |
|       - | 1263 | `	ph7_hashmap_node *pEntry;` |
|       - | 1264 | `	ph7_value *pVal;` |
|       - | 1265 | `	sxi32 rc;` |
|       - | 1266 | `	sxu32 n;` |
|     110 | 1267 | `	if( pSrc == pDest ){` |
|       - | 1268 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1269 | `		 * Unlike the zend engine.` |
|       - | 1270 | `		 */` |
|     ! 0 | 1271 | `		return SXRET_OK;` |
|       - | 1272 | `	}` |
|       - | 1273 | `	/* Point to the first inserted entry in the source */` |
|     110 | 1274 | `	pEntry = pSrc->pFirst;` |
|       - | 1275 | `	/* Perform the duplication */` |
|     320 | 1276 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1277 | `		/* Extract the node value */` |
|     212 | 1278 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     212 | 1279 | `		if( pVal ){` |
|     212 | 1280 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|     107 | 1281 | `		}else{` |
|     ! 0 | 1282 | `			rc = SXRET_OK;` |
|       - | 1283 | `		}` |
|     212 | 1284 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1285 | `			return rc;` |
|       - | 1286 | `		}` |
|       - | 1287 | `		/* Point to the next entry */` |
|     212 | 1288 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     107 | 1289 | `	}` |
|     110 | 1290 | `	return SXRET_OK;` |
|      56 | 1291 | `}` |
|       - | 1292 | `/*` |
|       - | 1293 | ` * Copy-on-write separation for arrays.` |
|       - | 1294 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|       - | 1295 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|       - | 1296 | ` * Returns the (possibly new) hashmap pointer.` |
|       - | 1297 | ` */` |
|  213856 | 1298 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1299 | `{` |
|  213861 | 1300 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1301 | `	ph7_hashmap *pNew;` |
|       - | 1302 | `	ph7_value *pBacking;` |
|       - | 1303 | `	sxu32 nValIdx;` |
|       - | 1304 | `	int bValueInPool;` |
|  213861 | 1305 | `	if( pMap->iRef < 2 ){` |
|       - | 1306 | `		/* Sole owner, no separation needed */` |
|  211691 | 1307 | `		return pMap;` |
|       - | 1308 | `	}` |
|    2175 | 1309 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1310 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1311 | `		return pMap;` |
|       - | 1312 | `	}` |
|       - | 1313 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1314 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1315 | `	 * frame is popped. */` |
|    2175 | 1316 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2175 | 1317 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    2170 | 1318 | `		if( pBacking && pBacking != pValue` |
|    2152 | 1319 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2139 | 1320 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1321 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2139 | 1322 | `			pMap->iRef--;` |
|    2139 | 1323 | `			if( pMap->iRef < 2 ){` |
|       - | 1324 | `				/* After undoing stack ref, sole owner — no separation */` |
|    2101 | 1325 | `				pMap->iRef++;` |
|    2101 | 1326 | `				return pMap;` |
|       - | 1327 | `			}` |
|      39 | 1328 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|      39 | 1329 | `			if( pNew == 0 ){` |
|     ! 0 | 1330 | `				pMap->iRef++;` |
|     ! 0 | 1331 | `				return pMap;` |
|       - | 1332 | `			}` |
|      39 | 1333 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1334 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|     ! 0 | 1335 | `				PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1336 | `				pMap->iRef++;` |
|     ! 0 | 1337 | `				return pMap;` |
|       - | 1338 | `			}` |
|      39 | 1339 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|      39 | 1340 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|       - | 1341 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|       - | 1342 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|       - | 1343 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|       - | 1344 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|       - | 1345 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|       - | 1346 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|       - | 1347 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|      39 | 1348 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      39 | 1349 | `			if( pBacking ){` |
|      39 | 1350 | `				pBacking->x.pOther = pNew;` |
|      19 | 1351 | `			}` |
|       - | 1352 | `			/* Update the stack value to match */` |
|      39 | 1353 | `			pValue->x.pOther = pNew;` |
|      39 | 1354 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|      39 | 1355 | `			return pNew;` |
|       - | 1356 | `		}` |
|      18 | 1357 | `	}` |
|       - | 1358 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|       - | 1359 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|       - | 1360 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|       - | 1361 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|       - | 1362 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|       - | 1363 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|       - | 1364 | `	 * pBacking in the backing-variable branch above). */` |
|      37 | 1365 | `	nValIdx = pValue->nIdx;` |
|      55 | 1366 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|      36 | 1367 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|      37 | 1368 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      37 | 1369 | `	if( pNew == 0 ){` |
|       - | 1370 | `		/* Allocation failure — fall through with shared map */` |
|     ! 0 | 1371 | `		return pMap;` |
|       - | 1372 | `	}` |
|      37 | 1373 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1374 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|     ! 0 | 1375 | `		PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1376 | `		return pMap;` |
|       - | 1377 | `	}` |
|      37 | 1378 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      37 | 1379 | `	pMap->iRef--;` |
|      37 | 1380 | `	if( bValueInPool ){` |
|       - | 1381 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|      37 | 1382 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|      37 | 1383 | `		if( pValue == 0 ){` |
|     ! 0 | 1384 | `			return pNew;` |
|       - | 1385 | `		}` |
|      18 | 1386 | `	}` |
|      37 | 1387 | `	pValue->x.pOther = pNew;` |
|      37 | 1388 | `	return pNew;` |
|  106933 | 1389 | `}` |
|       - | 1390 | `/*` |
|       - | 1391 | ` * Perform the union of two hashmaps.` |
|       - | 1392 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1393 | ` * with a variable holding an array as follows:` |
|       - | 1394 | ` * <?php` |
|       - | 1395 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1396 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1397 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1398 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1399 | ` * var_dump($c);` |
|       - | 1400 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1401 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1402 | ` * var_dump($c);` |
|       - | 1403 | ` * ?>` |
|       - | 1404 | ` * When executed, this script will print the following:` |
|       - | 1405 | ` * Union of $a and $b:` |
|       - | 1406 | ` * array(3) {` |
|       - | 1407 | ` *  ["a"]=>` |
|       - | 1408 | ` *  string(5) "apple"` |
|       - | 1409 | ` *  ["b"]=>` |
|       - | 1410 | ` * string(6) "banana"` |
|       - | 1411 | ` *  ["c"]=>` |
|       - | 1412 | ` * string(6) "cherry"` |
|       - | 1413 | ` * }` |
|       - | 1414 | ` * Union of $b and $a:` |
|       - | 1415 | ` * array(3) {` |
|       - | 1416 | ` * ["a"]=>` |
|       - | 1417 | ` * string(4) "pear"` |
|       - | 1418 | ` * ["b"]=>` |
|       - | 1419 | ` * string(10) "strawberry"` |
|       - | 1420 | ` * ["c"]=>` |
|       - | 1421 | ` * string(6) "cherry"` |
|       - | 1422 | ` * }` |
|       - | 1423 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1424 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1425 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1426 | ` */` |
|      10 | 1427 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       2 | 1428 | `{` |
|       - | 1429 | `	ph7_hashmap_node *pEntry;` |
|      12 | 1430 | `	sxi32 rc = SXRET_OK;` |
|       - | 1431 | `	ph7_value *pObj;` |
|       - | 1432 | `	sxu32 n;` |
|      12 | 1433 | `	if( pLeft == pRight ){` |
|       - | 1434 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1435 | `		 * Unlike the zend engine.` |
|       - | 1436 | `		 */` |
|     ! 0 | 1437 | `		return SXRET_OK;` |
|       - | 1438 | `	}` |
|       - | 1439 | `	/* Perform the union */` |
|      12 | 1440 | `	pEntry = pRight->pFirst;` |
|      32 | 1441 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1442 | `		/* Make sure the given key does not exists in the left array */` |
|      22 | 1443 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1444 | `			/* BLOB key */` |
|       7 | 1445 | `			if( SXRET_OK !=` |
|       6 | 1446 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|       3 | 1447 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|       3 | 1448 | `					if( pObj ){` |
|       3 | 1449 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1450 | `						/* Perform the insertion */` |
|       3 | 1451 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1452 | `							&sSafeVal,0,FALSE);` |
|       3 | 1453 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1454 | `							return rc;` |
|       - | 1455 | `						}` |
|       1 | 1456 | `					}` |
|       1 | 1457 | `			}` |
|       4 | 1458 | `		}else{` |
|       - | 1459 | `			/* INT key */` |
|      16 | 1460 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|      11 | 1461 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|      11 | 1462 | `				if( pObj ){` |
|      11 | 1463 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1464 | `					/* Perform the insertion */` |
|      11 | 1465 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      11 | 1466 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1467 | `						return rc;` |
|       - | 1468 | `					}` |
|       5 | 1469 | `				}` |
|       5 | 1470 | `			}` |
|       - | 1471 | `		}` |
|       - | 1472 | `		/* Point to the next entry */` |
|      22 | 1473 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      12 | 1474 | `	}` |
|      12 | 1475 | `	return SXRET_OK;` |
|       7 | 1476 | `}` |
|       - | 1477 | `/*` |
|       - | 1478 | ` * Allocate a new hashmap.` |
|       - | 1479 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1480 | ` */` |
|   99496 | 1481 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1482 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1483 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1484 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1485 | `	)` |
|       5 | 1486 | `{` |
|       - | 1487 | `	ph7_hashmap *pMap;` |
|       - | 1488 | `	/* Allocate a new instance */` |
|   99501 | 1489 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   99501 | 1490 | `	if( pMap == 0 ){` |
|     ! 0 | 1491 | `		return 0;` |
|       - | 1492 | `	}` |
|       - | 1493 | `	/* Zero the structure */` |
|   99501 | 1494 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1495 | `	/* Fill in the structure */` |
|   99501 | 1496 | `	pMap->pVm = &(*pVm);` |
|   99501 | 1497 | `	pMap->iRef = 1;` |
|       - | 1498 | `	/* Default hash functions */` |
|   99501 | 1499 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   99501 | 1500 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   99501 | 1501 | `	return pMap;` |
|   49753 | 1502 | `}` |
|       - | 1503 | `/*` |
|       - | 1504 | ` * Install superglobals in the given virtual machine.` |
|       - | 1505 | ` * Note on superglobals.` |
|       - | 1506 | ` *  According to the PHP language reference manual.` |
|       - | 1507 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1508 | `*   Description` |
|       - | 1509 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1510 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1511 | `*   global $variable; to access them within functions or methods.` |
|       - | 1512 | `*   These superglobal variables are:` |
|       - | 1513 | `*    $GLOBALS` |
|       - | 1514 | `*    $_SERVER` |
|       - | 1515 | `*    $_GET` |
|       - | 1516 | `*    $_POST` |
|       - | 1517 | `*    $_FILES` |
|       - | 1518 | `*    $_COOKIE` |
|       - | 1519 | `*    $_SESSION` |
|       - | 1520 | `*    $_REQUEST` |
|       - | 1521 | `*    $_ENV` |
|       - | 1522 | `*/` |
|    3426 | 1523 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       5 | 1524 | `{` |
|       - | 1525 | `	static const char * azSuper[] = {` |
|       - | 1526 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1527 | `		"_GET",      /* $_GET */` |
|       - | 1528 | `		"_POST",     /* $_POST */` |
|       - | 1529 | `		"_FILES",    /* $_FILES */` |
|       - | 1530 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1531 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1532 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1533 | `		"_ENV",      /* $_ENV */` |
|       - | 1534 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1535 | `		"argv"       /* $argv */` |
|       - | 1536 | `	};` |
|       - | 1537 | `	ph7_hashmap *pMap;` |
|       - | 1538 | `	ph7_value *pObj;` |
|       - | 1539 | `	SyString *pFile;` |
|       - | 1540 | `	sxi32 rc;` |
|       - | 1541 | `	sxu32 n;` |
|       - | 1542 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    3431 | 1543 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    3431 | 1544 | `	if( pMap == 0 ){` |
|     ! 0 | 1545 | `		return SXERR_MEM;` |
|       - | 1546 | `	}` |
|    3431 | 1547 | `	pVm->pGlobal = pMap;` |
|       - | 1548 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    3431 | 1549 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    3431 | 1550 | `	if( pObj == 0 ){` |
|     ! 0 | 1551 | `		return SXERR_MEM;` |
|       - | 1552 | `	}` |
|    3431 | 1553 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1554 | `	/* Record object index */` |
|    3431 | 1555 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1556 | `	/* Install the special $GLOBALS array */` |
|    3431 | 1557 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    3431 | 1558 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1559 | `		return rc;` |
|       - | 1560 | `	}` |
|       - | 1561 | `	/* Install superglobals now */` |
|   37691 | 1562 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1563 | `		ph7_value *pSuper;` |
|       - | 1564 | `		/* Request an empty array */` |
|   34265 | 1565 | `		pSuper = ph7_new_array(&(*pVm));` |
|   34265 | 1566 | `		if( pSuper == 0 ){` |
|     ! 0 | 1567 | `			return SXERR_MEM;` |
|       - | 1568 | `		}` |
|       - | 1569 | `		/* Install */` |
|   34265 | 1570 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   34265 | 1571 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1572 | `			return rc;` |
|       - | 1573 | `		}` |
|       - | 1574 | `		/* Release the value now it have been installed */` |
|   34265 | 1575 | `		ph7_release_value(&(*pVm),pSuper);` |
|   17135 | 1576 | `	}` |
|       - | 1577 | `	/* Set some $_SERVER entries */` |
|    3431 | 1578 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1579 | `	/*` |
|       - | 1580 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1581 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1582 | `	 */` |
|    6853 | 1583 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1584 | `		"SCRIPT_FILENAME",` |
|    1713 | 1585 | `		pFile ? pFile->zString : ":Memory:",` |
|    3422 | 1586 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1587 | `		);` |
|       - | 1588 | `	/* All done,all super-global are installed now */` |
|    3431 | 1589 | `	return SXRET_OK;` |
|    1718 | 1590 | `}` |
|       - | 1591 | `/*` |
|       - | 1592 | ` * Release a hashmap.` |
|       - | 1593 | ` */` |
|   61204 | 1594 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       5 | 1595 | `{` |
|       - | 1596 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   61209 | 1597 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1598 | `	sxu32 n;` |
|   61209 | 1599 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1600 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1601 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1602 | `		return SXRET_OK;` |
|       - | 1603 | `	}` |
|       - | 1604 | `	/* Start the release process */` |
|   61209 | 1605 | `	n = 0;` |
|   61209 | 1606 | `	pEntry = pMap->pFirst;` |
| 1594388 | 1607 | `	for(;;){` |
| 3188781 | 1608 | `		if( n >= pMap->nEntry ){` |
|   61209 | 1609 | `			break;` |
|       - | 1610 | `		}` |
| 3127577 | 1611 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1612 | `		/* Remove the reference from the foreign table */` |
| 3127577 | 1613 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3127577 | 1614 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1615 | `			/* Restore the ph7_value to the free list */` |
| 3127567 | 1616 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1563781 | 1617 | `		}` |
|       - | 1618 | `		/* Release the node */` |
| 3127577 | 1619 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   74067 | 1620 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   37031 | 1621 | `		}` |
| 3127577 | 1622 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1623 | `		/* Point to the next entry */` |
| 3127577 | 1624 | `		pEntry = pNext;` |
| 3127577 | 1625 | `		n++;` |
|       5 | 1626 | `	}` |
|   61209 | 1627 | `	if( pMap->nEntry > 0 ){` |
|       - | 1628 | `		/* Release the hash bucket */` |
|   54355 | 1629 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   27175 | 1630 | `	}` |
|   61209 | 1631 | `	if( FreeDS ){` |
|       - | 1632 | `		/* Free the whole instance */` |
|   61193 | 1633 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   30599 | 1634 | `	}else{` |
|       - | 1635 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1636 | `		pMap->apBucket = 0;` |
|      17 | 1637 | `		pMap->iNextIdx = 0;` |
|      17 | 1638 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1639 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1640 | `	}` |
|   61209 | 1641 | `	return SXRET_OK;` |
|   30607 | 1642 | `}` |
|       - | 1643 | `/*` |
|       - | 1644 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1645 | ` * If the count reaches zero which mean no more variables` |
|       - | 1646 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1647 | ` */` |
|  667258 | 1648 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       5 | 1649 | `{` |
|  667263 | 1650 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1651 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  667263 | 1652 | `	pMap->iRef--;` |
|  667263 | 1653 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   61173 | 1654 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   30584 | 1655 | `	}` |
|  667263 | 1656 | `}` |
|       - | 1657 | `/*` |
|       - | 1658 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1659 | ` * Write a pointer to the target node on success.` |
|       - | 1660 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1661 | ` */` |
|  125488 | 1662 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1663 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1664 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1665 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1666 | `	)` |
|       5 | 1667 | `{` |
|       - | 1668 | `	sxi32 rc;` |
|  125493 | 1669 | `	if( pMap->nEntry < 1 ){` |
|       - | 1670 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1671 | `		 */` |
|      64 | 1672 | `		return SXERR_NOTFOUND;` |
|       - | 1673 | `	}` |
|  125433 | 1674 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  125433 | 1675 | `	return rc;` |
|   62749 | 1676 | `}` |
|       - | 1677 | `/*` |
|       - | 1678 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1679 | ` * hashmap.` |
|       - | 1680 | ` * If a node with the given key already exists in the database` |
|       - | 1681 | ` * then this function overwrite the old value.` |
|       - | 1682 | ` */` |
| 2547606 | 1683 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1684 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1685 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1686 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1687 | `	)` |
|       5 | 1688 | `{` |
|       - | 1689 | `	sxi32 rc;` |
| 2547611 | 1690 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1691 | `		/*` |
|       - | 1692 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1693 | `		 */` |
|     ! 0 | 1694 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1695 | `		return SXRET_OK;` |
|       - | 1696 | `	}` |
| 2547611 | 1697 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2547611 | 1698 | `	return rc;` |
| 1273808 | 1699 | `}` |
|       - | 1700 | `/*` |
|       - | 1701 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|       - | 1702 | ` *   - String keys overwrite same-key entries in pDest.` |
|       - | 1703 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|       - | 1704 | ` * This is the same routine that backs array_merge().` |
|       - | 1705 | ` */` |
|      52 | 1706 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1707 | `{` |
|      53 | 1708 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|       1 | 1709 | `}` |
|       - | 1710 | `/*` |
|       - | 1711 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1712 | ` * hashmap.` |
|       - | 1713 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1714 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1715 | ` * The insertion by reference is triggered when the following` |
|       - | 1716 | ` * expression is encountered.` |
|       - | 1717 | ` * $var = 10;` |
|       - | 1718 | ` *  $a = array(&var);` |
|       - | 1719 | ` * OR` |
|       - | 1720 | ` *  $a[] =& $var;` |
|       - | 1721 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1722 | ` * over it's contents.` |
|       - | 1723 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1724 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1725 | ` * Example:` |
|       - | 1726 | ` *  $var = 10;` |
|       - | 1727 | ` *  $a[] =& $var;` |
|       - | 1728 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1729 | ` *  //Unset the foreign ph7_value now` |
|       - | 1730 | ` *  unset($var);` |
|       - | 1731 | ` *  echo count($a); //0` |
|       - | 1732 | ` * Note that this is a PH7 eXtension.` |
|       - | 1733 | ` * Refer to the official documentation for more information.` |
|       - | 1734 | ` * If a node with the given key already exists in the database` |
|       - | 1735 | ` * then this function overwrite the old value.` |
|       - | 1736 | ` */` |
|   45410 | 1737 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1738 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1739 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1740 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1741 | `	)` |
|       5 | 1742 | `{` |
|       - | 1743 | `	sxi32 rc;` |
|   45415 | 1744 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1745 | `		/*` |
|       - | 1746 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1747 | `		 */` |
|     ! 0 | 1748 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1749 | `		return SXRET_OK;` |
|       - | 1750 | `	}` |
|   45415 | 1751 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   45415 | 1752 | `	return rc;` |
|   22710 | 1753 | `}` |
|       - | 1754 | `/*` |
|       - | 1755 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1756 | ` */` |
|   27198 | 1757 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       5 | 1758 | `{` |
|       - | 1759 | `	/* Reset the loop cursor */` |
|   27203 | 1760 | `	pMap->pCur = pMap->pFirst;` |
|   27203 | 1761 | `}` |
|       - | 1762 | `/*` |
|       - | 1763 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1764 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1765 | ` * return NULL.` |
|       - | 1766 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1767 | ` */` |
|  227530 | 1768 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       5 | 1769 | `{` |
|  227535 | 1770 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  227535 | 1771 | `	if( pCur == 0 ){` |
|       - | 1772 | `		/* End of the list,return null */` |
|   13623 | 1773 | `		return 0;` |
|       - | 1774 | `	}` |
|       - | 1775 | `	/* Advance the node cursor */` |
|  213917 | 1776 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  213917 | 1777 | `	return pCur;` |
|  113770 | 1778 | `}` |
|       - | 1779 | `/*` |
|       - | 1780 | ` * Extract a node value.` |
|       - | 1781 | ` */` |
|  537670 | 1782 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       5 | 1783 | `{` |
|  537675 | 1784 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  537675 | 1785 | `	if( pEntry ){` |
|  537675 | 1786 | `		if( bStore ){` |
|  214255 | 1787 | `			PH7_MemObjStore(pEntry,pValue);` |
|  107130 | 1788 | `		}else{` |
|  323425 | 1789 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1790 | `		}` |
|  268899 | 1791 | `	}else{` |
|     ! 0 | 1792 | `		PH7_MemObjRelease(pValue);` |
|       - | 1793 | `	}` |
|  537675 | 1794 | `}` |
|       - | 1795 | `/*` |
|       - | 1796 | ` * Extract a node key.` |
|       - | 1797 | ` */` |
|  134146 | 1798 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       5 | 1799 | `{` |
|       - | 1800 | `	/* Fill with the current key */` |
|  134151 | 1801 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  133669 | 1802 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1803 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1804 | `		}` |
|  133669 | 1805 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  133669 | 1806 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   66837 | 1807 | `	}else{` |
|     485 | 1808 | `		SyBlobReset(&pKey->sBlob);` |
|     485 | 1809 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     485 | 1810 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1811 | `	}` |
|  134151 | 1812 | `}` |
|       - | 1813 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1814 | `/*` |
|       - | 1815 | ` * Store the address of nodes value in the given container.` |
|       - | 1816 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1817 | ` * defined in 'builtin.c' for more information.` |
|       - | 1818 | ` */` |
|      10 | 1819 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1820 | `{` |
|      11 | 1821 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1822 | `	ph7_value *pValue;` |
|       - | 1823 | `	sxu32 n;` |
|       - | 1824 | `	/* Initialize the container */` |
|      11 | 1825 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1826 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1827 | `		/* Extract node value */` |
|      17 | 1828 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1829 | `		if( pValue ){` |
|      17 | 1830 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1831 | `		}` |
|       - | 1832 | `		/* Point to the next entry */` |
|      17 | 1833 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1834 | `	}` |
|       - | 1835 | `	/* Total inserted entries */` |
|      11 | 1836 | `	return (int)SySetUsed(pOut);` |
|       1 | 1837 | `}` |
|       - | 1838 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1839 | `/* SPDX-SnippetBegin */` |
|       - | 1840 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - | 1841 | `/* SPDX-License-Identifier: blessing */` |
|       - | 1842 | `/*` |
|       - | 1843 | ` * Merge sort.` |
|       - | 1844 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1845 | ` * Status: Public domain` |
|       - | 1846 | ` */` |
|       - | 1847 | `/* Node comparison callback signature */` |
|       - | 1848 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1849 | `/*` |
|       - | 1850 | `** Inputs:` |
|       - | 1851 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1852 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1853 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1854 | `**` |
|       - | 1855 | `** Return Value:` |
|       - | 1856 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1857 | `**   of both a and b.` |
|       - | 1858 | `**` |
|       - | 1859 | `** Side effects:` |
|       - | 1860 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1861 | `**   changed.` |
|       - | 1862 | `*/` |
|   33224 | 1863 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1864 | `{` |
|       - | 1865 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1866 | `    /* Prevent compiler warning */` |
|   33229 | 1867 | `	result.pNext = result.pPrev = 0;` |
|   33229 | 1868 | `	pTail = &result;` |
|  101145 | 1869 | `	while( pA && pB ){` |
|   67921 | 1870 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   45269 | 1871 | `			pTail->pPrev = pA;` |
|   45269 | 1872 | `			pA->pNext = pTail;` |
|   45269 | 1873 | `			pTail = pA;` |
|   45269 | 1874 | `			pA = pA->pPrev;` |
|   22617 | 1875 | `		}else{` |
|   22657 | 1876 | `			pTail->pPrev = pB;` |
|   22657 | 1877 | `			pB->pNext = pTail;` |
|   22657 | 1878 | `			pTail = pB;` |
|   22657 | 1879 | `			pB = pB->pPrev;` |
|       - | 1880 | `		}` |
|       5 | 1881 | `	}` |
|   33229 | 1882 | `	if( pA ){` |
|   23251 | 1883 | `		pTail->pPrev = pA;` |
|   23251 | 1884 | `		pA->pNext = pTail;` |
|   21622 | 1885 | `	}else if( pB ){` |
|    9755 | 1886 | `		pTail->pPrev = pB;` |
|    9755 | 1887 | `		pB->pNext = pTail;` |
|    4864 | 1888 | `	}else{` |
|     233 | 1889 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1890 | `	}` |
|   33229 | 1891 | `	return result.pPrev;` |
|       5 | 1892 | `}` |
|       - | 1893 | `/*` |
|       - | 1894 | `** Inputs:` |
|       - | 1895 | `**   Map:       Input hashmap` |
|       - | 1896 | `**   cmp:       A comparison function.` |
|       - | 1897 | `**` |
|       - | 1898 | `** Return Value:` |
|       - | 1899 | `**   Sorted hashmap.` |
|       - | 1900 | `**` |
|       - | 1901 | `** Side effects:` |
|       - | 1902 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1903 | `*/` |
|       - | 1904 | `#define N_SORT_BUCKET  32` |
|     686 | 1905 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1906 | `{` |
|       - | 1907 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1908 | `	sxu32 i;` |
|     691 | 1909 | `	SyZero(a,sizeof(a));` |
|       - | 1910 | `	/* Point to the first inserted entry */` |
|     691 | 1911 | `	pIn = pMap->pFirst;` |
|   13865 | 1912 | `	while( pIn ){` |
|   13179 | 1913 | `		p = pIn;` |
|   13179 | 1914 | `		pIn = p->pPrev;` |
|   13179 | 1915 | `		p->pPrev = 0;` |
|   25137 | 1916 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   25137 | 1917 | `			if( a[i]==0 ){` |
|   13179 | 1918 | `				a[i] = p;` |
|   13179 | 1919 | `				break;` |
|     ! 0 | 1920 | `			}else{` |
|   11963 | 1921 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   11963 | 1922 | `				a[i] = 0;` |
|       - | 1923 | `			}` |
|    5984 | 1924 | `		}` |
|   13179 | 1925 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1926 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1927 | `			 * But that is impossible.` |
|       - | 1928 | `			 */` |
|     ! 0 | 1929 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1930 | `		}` |
|       5 | 1931 | `	}` |
|     691 | 1932 | `	p = a[0];` |
|   21957 | 1933 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   21271 | 1934 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   10638 | 1935 | `	}` |
|     691 | 1936 | `	p->pNext = 0;` |
|       - | 1937 | `	/* Reflect the change */` |
|     691 | 1938 | `	pMap->pFirst = p;` |
|       - | 1939 | `	/* Reset the loop cursor */` |
|     691 | 1940 | `	pMap->pCur = pMap->pFirst;` |
|     691 | 1941 | `	return SXRET_OK;` |
|       5 | 1942 | `}` |
|       - | 1943 | `/* SPDX-SnippetEnd */` |
|       - | 1944 | `/*` |
|       - | 1945 | ` * Node comparison callback.` |
|       - | 1946 | ` * used-by: [sort(),asort(),...]` |
|       - | 1947 | ` */` |
|   67713 | 1948 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 | 1949 | `{` |
|       - | 1950 | `	ph7_value sA,sB;` |
|       - | 1951 | `	sxi32 iFlags;` |
|       - | 1952 | `	int rc;` |
|   67718 | 1953 | `	if( pCmpData == 0 ){` |
|       - | 1954 | `		/* Perform a standard comparison */` |
|   67694 | 1955 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   67694 | 1956 | `		return rc;` |
|       - | 1957 | `	}` |
|      25 | 1958 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1959 | `	/* Duplicate node values */` |
|      25 | 1960 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 1961 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 1962 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 1963 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 1964 | `	if( iFlags == 5 ){` |
|       - | 1965 | `		/* String cast */` |
|       - | 1966 | `		const char *zA,*zB;` |
|       - | 1967 | `		sxu32 nA,nB,nMin;` |
|      15 | 1968 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1969 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1970 | `		}` |
|      15 | 1971 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1972 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1973 | `		}` |
|       - | 1974 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 1975 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 1976 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 1977 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 1978 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 1979 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 1980 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 1981 | `		if( rc == 0 ){` |
|       5 | 1982 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1983 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1984 | `		}` |
|       8 | 1985 | `	}else{` |
|       - | 1986 | `		/* Numeric cast */` |
|      11 | 1987 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1988 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1989 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1990 | `	}` |
|      25 | 1991 | `	PH7_MemObjRelease(&sA);` |
|      25 | 1992 | `	PH7_MemObjRelease(&sB);` |
|      25 | 1993 | `	return rc;` |
|   33891 | 1994 | `}` |
|       - | 1995 | `/*` |
|       - | 1996 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1997 | ` * used-by: [ksort()]` |
|       - | 1998 | ` */` |
|      14 | 1999 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2000 | `{` |
|       - | 2001 | `	sxi32 rc;` |
|       7 | 2002 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 2003 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2004 | `		/* Perform a string comparison */` |
|       5 | 2005 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2006 | `	}else{` |
|       - | 2007 | `		SyString sStr;` |
|       - | 2008 | `		sxi64 iA,iB;` |
|       - | 2009 | `		/* Perform a numeric comparison */` |
|      11 | 2010 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2011 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2012 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2013 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2014 | `				iA = 0;` |
|     ! 0 | 2015 | `			}else{` |
|     ! 0 | 2016 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2017 | `			}` |
|     ! 0 | 2018 | `		}else{` |
|      11 | 2019 | `			iA = pA->xKey.iKey;` |
|       - | 2020 | `		}` |
|      11 | 2021 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2022 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2023 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2024 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2025 | `				iB = 0;` |
|     ! 0 | 2026 | `			}else{` |
|     ! 0 | 2027 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2028 | `			}` |
|     ! 0 | 2029 | `		}else{` |
|      11 | 2030 | `			iB = pB->xKey.iKey;` |
|       - | 2031 | `		}` |
|      11 | 2032 | `		rc = (sxi32)(iA-iB);` |
|       - | 2033 | `	}` |
|       - | 2034 | `	/* Comparison result */` |
|      15 | 2035 | `	return rc;` |
|       1 | 2036 | `}` |
|       - | 2037 | `/*` |
|       - | 2038 | ` * Node comparison callback.` |
|       - | 2039 | ` * Used by: [rsort(),arsort()];` |
|       - | 2040 | ` */` |
|      78 | 2041 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2042 | `{` |
|       - | 2043 | `	ph7_value sA,sB;` |
|       - | 2044 | `	sxi32 iFlags;` |
|       - | 2045 | `	int rc;` |
|      79 | 2046 | `	if( pCmpData == 0 ){` |
|       - | 2047 | `		/* Perform a standard comparison */` |
|      59 | 2048 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 2049 | `		return -rc;` |
|       - | 2050 | `	}` |
|      21 | 2051 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 2052 | `	/* Duplicate node values */` |
|      21 | 2053 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 2054 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 2055 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 2056 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 2057 | `	if( iFlags == 5 ){` |
|       - | 2058 | `		/* String cast */` |
|       - | 2059 | `		const char *zA,*zB;` |
|       - | 2060 | `		sxu32 nA,nB,nMin;` |
|      11 | 2061 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2062 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 2063 | `		}` |
|      11 | 2064 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2065 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 2066 | `		}` |
|       - | 2067 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 2068 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 2069 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 2070 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 2071 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 2072 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 2073 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 2074 | `		if( rc == 0 ){` |
|       3 | 2075 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 2076 | `			else if( nA > nB ) rc = 1;` |
|       1 | 2077 | `		}` |
|       6 | 2078 | `	}else{` |
|       - | 2079 | `		/* Numeric cast */` |
|      11 | 2080 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 2081 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 2082 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 2083 | `	}` |
|      21 | 2084 | `	PH7_MemObjRelease(&sA);` |
|      21 | 2085 | `	PH7_MemObjRelease(&sB);` |
|      21 | 2086 | `	return -rc;` |
|      40 | 2087 | `}` |
|       - | 2088 | `/*` |
|       - | 2089 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2090 | ` * used-by: [usort(),uasort()]` |
|       - | 2091 | ` */` |
|      88 | 2092 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 2093 | `{` |
|       - | 2094 | `	ph7_value sResult,*pCallback;` |
|       - | 2095 | `	ph7_value *pV1,*pV2;` |
|       - | 2096 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2097 | `	sxi32 rc;` |
|       - | 2098 | `	/* Point to the desired callback */` |
|      90 | 2099 | `	pCallback = (ph7_value *)pCmpData;` |
|      90 | 2100 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2101 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2102 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       3 | 2103 | `		return 0;` |
|       - | 2104 | `	}` |
|       - | 2105 | `	/* initialize the result value */` |
|      88 | 2106 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 2107 | `	/* Extract nodes values */` |
|      88 | 2108 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      88 | 2109 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      88 | 2110 | `	apArg[0] = pV1;` |
|      88 | 2111 | `	apArg[1] = pV2;` |
|       - | 2112 | `	/* Invoke the callback */` |
|      88 | 2113 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      88 | 2114 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2115 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2116 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       3 | 2117 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       3 | 2118 | `		rc = 0;` |
|      87 | 2119 | `	}else if( rc != SXRET_OK ){` |
|       - | 2120 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       5 | 2121 | `		rc = -1; /* Set a dummy result */` |
|       3 | 2122 | `	}else{` |
|       - | 2123 | `		/* Extract callback result */` |
|      82 | 2124 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2125 | `			/* Perform an int cast */` |
|     ! 0 | 2126 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2127 | `		}` |
|      82 | 2128 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2129 | `	}` |
|      88 | 2130 | `	PH7_MemObjRelease(&sResult);` |
|       - | 2131 | `	/* Callback result */` |
|      88 | 2132 | `	return rc;` |
|      46 | 2133 | `}` |
|       - | 2134 | `/*` |
|       - | 2135 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2136 | ` * used-by: [krsort()]` |
|       - | 2137 | ` */` |
|       4 | 2138 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2139 | `{` |
|       - | 2140 | `	sxi32 rc;` |
|       2 | 2141 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2142 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2143 | `		/* Perform a string comparison */` |
|       5 | 2144 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2145 | `	}else{` |
|       - | 2146 | `		SyString sStr;` |
|       - | 2147 | `		sxi64 iA,iB;` |
|       - | 2148 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2149 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2150 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2151 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2152 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2153 | `				iA = 0;` |
|     ! 0 | 2154 | `			}else{` |
|     ! 0 | 2155 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2156 | `			}` |
|     ! 0 | 2157 | `		}else{` |
|     ! 0 | 2158 | `			iA = pA->xKey.iKey;` |
|       - | 2159 | `		}` |
|     ! 0 | 2160 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2161 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2162 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2163 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2164 | `				iB = 0;` |
|     ! 0 | 2165 | `			}else{` |
|     ! 0 | 2166 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2167 | `			}` |
|     ! 0 | 2168 | `		}else{` |
|     ! 0 | 2169 | `			iB = pB->xKey.iKey;` |
|       - | 2170 | `		}` |
|     ! 0 | 2171 | `		rc = (sxi32)(iA-iB);` |
|       - | 2172 | `	}` |
|       5 | 2173 | `	return -rc; /* Reverse result */` |
|       1 | 2174 | `}` |
|       - | 2175 | `/*` |
|       - | 2176 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2177 | ` * used-by: [uksort()]` |
|       - | 2178 | ` */` |
|       6 | 2179 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2180 | `{` |
|       - | 2181 | `	ph7_value sResult,*pCallback;` |
|       - | 2182 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2183 | `	ph7_value sK1,sK2;` |
|       - | 2184 | `	sxi32 rc;` |
|       - | 2185 | `	/* Point to the desired callback */` |
|       7 | 2186 | `	pCallback = (ph7_value *)pCmpData;` |
|       7 | 2187 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2188 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2189 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     ! 0 | 2190 | `		return 0;` |
|       - | 2191 | `	}` |
|       - | 2192 | `	/* initialize the result value */` |
|       7 | 2193 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2194 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2195 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2196 | `	/* Extract nodes keys */` |
|       7 | 2197 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2198 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2199 | `	apArg[0] = &sK1;` |
|       7 | 2200 | `	apArg[1] = &sK2;` |
|       - | 2201 | `	/* Mark keys as constants */` |
|       7 | 2202 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2203 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2204 | `	/* Invoke the callback */` |
|       7 | 2205 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2206 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2207 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2208 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     ! 0 | 2209 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     ! 0 | 2210 | `		rc = 0;` |
|       7 | 2211 | `	}else if( rc != SXRET_OK ){` |
|       - | 2212 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2213 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2214 | `	}else{` |
|       - | 2215 | `		/* Extract callback result */` |
|       7 | 2216 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2217 | `			/* Perform an int cast */` |
|     ! 0 | 2218 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2219 | `		}` |
|       7 | 2220 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2221 | `	}` |
|       7 | 2222 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2223 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2224 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2225 | `	/* Callback result */` |
|       7 | 2226 | `	return rc;` |
|       4 | 2227 | `}` |
|       - | 2228 | `/*` |
|       - | 2229 | ` * Node comparison callback: Random node comparison.` |
|       - | 2230 | ` * used-by: [shuffle()]` |
|       - | 2231 | ` */` |
|      13 | 2232 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2233 | `{` |
|       - | 2234 | `	sxu32 n;` |
|       6 | 2235 | `	SXUNUSED(pB); /* cc warning */` |
|       6 | 2236 | `	SXUNUSED(pCmpData);` |
|       - | 2237 | `	/* Grab a random number */` |
|      14 | 2238 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2239 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2240 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2241 | `	 */` |
|      14 | 2242 | `	return n&1 ? 1 : -1;` |
|       1 | 2243 | `}` |
|       - | 2244 | `/*` |
|       - | 2245 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2246 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2247 | ` */` |
|     638 | 2248 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       5 | 2249 | `{` |
|       - | 2250 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2251 | `	sxu32 i;` |
|       - | 2252 | `	/* Rehash all entries */` |
|     643 | 2253 | `	pLast = p = pMap->pFirst;` |
|     643 | 2254 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     643 | 2255 | `	i = 0;` |
|    6821 | 2256 | `	for( ;; ){` |
|   13647 | 2257 | `		if( i >= pMap->nEntry ){` |
|     643 | 2258 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     643 | 2259 | `			break;` |
|       - | 2260 | `		}` |
|   13009 | 2261 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2262 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2263 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2264 | `			/* Change key type */` |
|       5 | 2265 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2266 | `		}` |
|   13009 | 2267 | `		HashmapRehashIntNode(p);` |
|       - | 2268 | `		/* Point to the next entry */` |
|   13009 | 2269 | `		i++;` |
|   13009 | 2270 | `		pLast = p;` |
|   13009 | 2271 | `		p = p->pPrev; /* Reverse link */` |
|       5 | 2272 | `	}` |
|     643 | 2273 | `}` |
|       - | 2274 | `/*` |
|       - | 2275 | ` * Array functions implementation.` |
|       - | 2276 | ` * Status:` |
|       - | 2277 | ` *  Stable.` |
|       - | 2278 | ` */` |
|       - | 2279 | `/*` |
|       - | 2280 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2281 | ` * Sort an array.` |
|       - | 2282 | ` * Parameters` |
|       - | 2283 | ` *  $array` |
|       - | 2284 | ` *   The input array.` |
|       - | 2285 | ` * $sort_flags` |
|       - | 2286 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2287 | ` *  Sorting type flags:` |
|       - | 2288 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2289 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2290 | ` *   SORT_STRING - compare items as strings` |
|       - | 2291 | ` * Return` |
|       - | 2292 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2293 | ` *` |
|       - | 2294 | ` */` |
|     982 | 2295 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2296 | `{` |
|       - | 2297 | `	ph7_hashmap *pMap;` |
|       - | 2298 | `	/* Make sure we are dealing with a valid hashmap */` |
|     987 | 2299 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2300 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2301 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2302 | `		return PH7_OK;` |
|       - | 2303 | `	}` |
|       - | 2304 | `	/* Point to the internal representation of the input hashmap */` |
|     987 | 2305 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     987 | 2306 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     987 | 2307 | `	if( pMap->nEntry > 1 ){` |
|     627 | 2308 | `		sxi32 iCmpFlags = 0;` |
|     627 | 2309 | `		if( nArg > 1 ){` |
|       - | 2310 | `			/* Extract comparison flags */` |
|       3 | 2311 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2312 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2313 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2314 | `			}` |
|       1 | 2315 | `		}` |
|       - | 2316 | `		/* Do the merge sort */` |
|     627 | 2317 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2318 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     627 | 2319 | `		HashmapSortRehash(pMap);` |
|     311 | 2320 | `	}` |
|       - | 2321 | `	/* All done,return TRUE */` |
|     987 | 2322 | `	ph7_result_bool(pCtx,1);` |
|     987 | 2323 | `	return PH7_OK;` |
|     496 | 2324 | `}` |
|       - | 2325 | `/*` |
|       - | 2326 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2327 | ` *  Sort an array and maintain index association.` |
|       - | 2328 | ` * Parameters` |
|       - | 2329 | ` *  $array` |
|       - | 2330 | ` *   The input array.` |
|       - | 2331 | ` * $sort_flags` |
|       - | 2332 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2333 | ` *  Sorting type flags:` |
|       - | 2334 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2335 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2336 | ` *   SORT_STRING - compare items as strings` |
|       - | 2337 | ` * Return` |
|       - | 2338 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2339 | ` */` |
|      32 | 2340 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2341 | `{` |
|       - | 2342 | `	ph7_hashmap *pMap;` |
|       - | 2343 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2344 | `	if( nArg < 1 ){` |
|       3 | 2345 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2346 | `			"ArgumentCountError",` |
|       - | 2347 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2348 | `			);` |
|       - | 2349 | `	}` |
|       - | 2350 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2351 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2352 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2353 | `			"TypeError",` |
|       - | 2354 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2355 | `			ph7_type_name(apArg[0])` |
|       - | 2356 | `			);` |
|       - | 2357 | `	}` |
|       - | 2358 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2359 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2360 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2361 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2362 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2363 | `		if( nArg > 1 ){` |
|       - | 2364 | `			/* Extract comparison flags */` |
|       5 | 2365 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2366 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2367 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2368 | `			}` |
|       2 | 2369 | `		}` |
|       - | 2370 | `		/* Do the merge sort */` |
|      19 | 2371 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2372 | `		/* Fix the last link broken by the merge */` |
|      45 | 2373 | `		while(pMap->pLast->pPrev){` |
|      27 | 2374 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2375 | `		}` |
|       9 | 2376 | `	}` |
|       - | 2377 | `	/* All done,return TRUE */` |
|      23 | 2378 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2379 | `	return PH7_OK;` |
|      21 | 2380 | `}` |
|       - | 2381 | `/*` |
|       - | 2382 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2383 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2384 | ` * Parameters` |
|       - | 2385 | ` *  $array` |
|       - | 2386 | ` *   The input array.` |
|       - | 2387 | ` * $sort_flags` |
|       - | 2388 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2389 | ` *  Sorting type flags:` |
|       - | 2390 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2391 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2392 | ` *   SORT_STRING - compare items as strings` |
|       - | 2393 | ` * Return` |
|       - | 2394 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2395 | ` */` |
|      32 | 2396 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2397 | `{` |
|       - | 2398 | `	ph7_hashmap *pMap;` |
|       - | 2399 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2400 | `	if( nArg < 1 ){` |
|       3 | 2401 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2402 | `			"ArgumentCountError",` |
|       - | 2403 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2404 | `			);` |
|       - | 2405 | `	}` |
|       - | 2406 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2407 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2408 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2409 | `			"TypeError",` |
|       - | 2410 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2411 | `			ph7_type_name(apArg[0])` |
|       - | 2412 | `			);` |
|       - | 2413 | `	}` |
|       - | 2414 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2415 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2416 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2417 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2418 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2419 | `		if( nArg > 1 ){` |
|       - | 2420 | `			/* Extract comparison flags */` |
|       5 | 2421 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2422 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2423 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2424 | `			}` |
|       2 | 2425 | `		}` |
|       - | 2426 | `		/* Do the merge sort */` |
|      19 | 2427 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2428 | `		/* Fix the last link broken by the merge */` |
|      35 | 2429 | `		while(pMap->pLast->pPrev){` |
|      17 | 2430 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2431 | `		}` |
|       9 | 2432 | `	}` |
|       - | 2433 | `	/* All done,return TRUE */` |
|      23 | 2434 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2435 | `	return PH7_OK;` |
|      21 | 2436 | `}` |
|       - | 2437 | `/*` |
|       - | 2438 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2439 | ` *  Sort an array by key.` |
|       - | 2440 | ` * Parameters` |
|       - | 2441 | ` *  $array` |
|       - | 2442 | ` *   The input array.` |
|       - | 2443 | ` * $sort_flags` |
|       - | 2444 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2445 | ` *  Sorting type flags:` |
|       - | 2446 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2447 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2448 | ` *   SORT_STRING - compare items as strings` |
|       - | 2449 | ` * Return` |
|       - | 2450 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2451 | ` */` |
|       4 | 2452 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2453 | `{` |
|       - | 2454 | `	ph7_hashmap *pMap;` |
|       - | 2455 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2456 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2457 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2458 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2459 | `		return PH7_OK;` |
|       - | 2460 | `	}` |
|       - | 2461 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2462 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2463 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2464 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2465 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2466 | `		if( nArg > 1 ){` |
|       - | 2467 | `			/* Extract comparison flags */` |
|     ! 0 | 2468 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2469 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2470 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2471 | `			}` |
|     ! 0 | 2472 | `		}` |
|       - | 2473 | `		/* Do the merge sort */` |
|       5 | 2474 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2475 | `		/* Fix the last link broken by the merge */` |
|      15 | 2476 | `		while(pMap->pLast->pPrev){` |
|      11 | 2477 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2478 | `		}` |
|       2 | 2479 | `	}` |
|       - | 2480 | `	/* All done,return TRUE */` |
|       5 | 2481 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2482 | `	return PH7_OK;` |
|       3 | 2483 | `}` |
|       - | 2484 | `/*` |
|       - | 2485 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2486 | ` *  Sort an array by key in reverse order.` |
|       - | 2487 | ` * Parameters` |
|       - | 2488 | ` *  $array` |
|       - | 2489 | ` *   The input array.` |
|       - | 2490 | ` * $sort_flags` |
|       - | 2491 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2492 | ` *  Sorting type flags:` |
|       - | 2493 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2494 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2495 | ` *   SORT_STRING - compare items as strings` |
|       - | 2496 | ` * Return` |
|       - | 2497 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2498 | ` */` |
|       2 | 2499 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2500 | `{` |
|       - | 2501 | `	ph7_hashmap *pMap;` |
|       - | 2502 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2503 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2504 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2505 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2506 | `		return PH7_OK;` |
|       - | 2507 | `	}` |
|       - | 2508 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2509 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2510 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2511 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2512 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2513 | `		if( nArg > 1 ){` |
|       - | 2514 | `			/* Extract comparison flags */` |
|     ! 0 | 2515 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2516 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2517 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2518 | `			}` |
|     ! 0 | 2519 | `		}` |
|       - | 2520 | `		/* Do the merge sort */` |
|       3 | 2521 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2522 | `		/* Fix the last link broken by the merge */` |
|       7 | 2523 | `		while(pMap->pLast->pPrev){` |
|       5 | 2524 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2525 | `		}` |
|       1 | 2526 | `	}` |
|       - | 2527 | `	/* All done,return TRUE */` |
|       3 | 2528 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2529 | `	return PH7_OK;` |
|       2 | 2530 | `}` |
|       - | 2531 | `/*` |
|       - | 2532 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2533 | ` * Sort an array in reverse order.` |
|       - | 2534 | ` * Parameters` |
|       - | 2535 | ` *  $array` |
|       - | 2536 | ` *   The input array.` |
|       - | 2537 | ` * $sort_flags` |
|       - | 2538 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2539 | ` *  Sorting type flags:` |
|       - | 2540 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2541 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2542 | ` *   SORT_STRING - compare items as strings` |
|       - | 2543 | ` * Return` |
|       - | 2544 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2545 | ` */` |
|       2 | 2546 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2547 | `{` |
|       - | 2548 | `	ph7_hashmap *pMap;` |
|       - | 2549 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2550 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2551 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2552 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2553 | `		return PH7_OK;` |
|       - | 2554 | `	}` |
|       - | 2555 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2556 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2557 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2558 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2559 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2560 | `		if( nArg > 1 ){` |
|       - | 2561 | `			/* Extract comparison flags */` |
|     ! 0 | 2562 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2563 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2564 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2565 | `			}` |
|     ! 0 | 2566 | `		}` |
|       - | 2567 | `		/* Do the merge sort */` |
|       3 | 2568 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2569 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2570 | `		HashmapSortRehash(pMap);` |
|       1 | 2571 | `	}` |
|       - | 2572 | `	/* All done,return TRUE */` |
|       3 | 2573 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2574 | `	return PH7_OK;` |
|       2 | 2575 | `}` |
|       - | 2576 | `/*` |
|       - | 2577 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2578 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2579 | ` * Parameters` |
|       - | 2580 | ` *  $array` |
|       - | 2581 | ` *   The input array.` |
|       - | 2582 | ` * $cmp_function` |
|       - | 2583 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2584 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2585 | ` *  to, or greater than the second.` |
|       - | 2586 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2587 | ` * Return` |
|       - | 2588 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2589 | ` */` |
|      12 | 2590 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2591 | `{` |
|       - | 2592 | `	ph7_hashmap *pMap;` |
|       - | 2593 | `	/* Make sure we are dealing with a valid hashmap */` |
|      14 | 2594 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2595 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2596 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2597 | `		return PH7_OK;` |
|       - | 2598 | `	}` |
|       - | 2599 | `	/* Point to the internal representation of the input hashmap */` |
|      14 | 2600 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      14 | 2601 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 2602 | `	if( pMap->nEntry > 1 ){` |
|      14 | 2603 | `		ph7_value *pCallback = 0;` |
|       - | 2604 | `		ProcNodeCmp xCmp;` |
|      14 | 2605 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      14 | 2606 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2607 | `			/* Point to the desired callback */` |
|      14 | 2608 | `			pCallback = apArg[1];` |
|       8 | 2609 | `		}else{` |
|       - | 2610 | `			/* Use the default comparison function */` |
|     ! 0 | 2611 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2612 | `		}` |
|       - | 2613 | `		/* Do the merge sort */` |
|      14 | 2614 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      14 | 2615 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2616 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      14 | 2617 | `		HashmapSortRehash(pMap);` |
|      14 | 2618 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2619 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 2620 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2621 | `			return PH7_EXCEPTION;` |
|       - | 2622 | `		}` |
|       5 | 2623 | `	}` |
|       - | 2624 | `	/* All done,return TRUE */` |
|      12 | 2625 | `	ph7_result_bool(pCtx,1);` |
|      12 | 2626 | `	return PH7_OK;` |
|       8 | 2627 | `}` |
|       - | 2628 | `/*` |
|       - | 2629 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2630 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2631 | ` *  and maintain index association.` |
|       - | 2632 | ` * Parameters` |
|       - | 2633 | ` *  $array` |
|       - | 2634 | ` *   The input array.` |
|       - | 2635 | ` * $cmp_function` |
|       - | 2636 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2637 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2638 | ` *  to, or greater than the second.` |
|       - | 2639 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2640 | ` * Return` |
|       - | 2641 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2642 | ` */` |
|       2 | 2643 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2644 | `{` |
|       - | 2645 | `	ph7_hashmap *pMap;` |
|       - | 2646 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2647 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2648 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2649 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2650 | `		return PH7_OK;` |
|       - | 2651 | `	}` |
|       - | 2652 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2653 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2654 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2655 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2656 | `		ph7_value *pCallback = 0;` |
|       - | 2657 | `		ProcNodeCmp xCmp;` |
|       3 | 2658 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2659 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2660 | `			/* Point to the desired callback */` |
|       3 | 2661 | `			pCallback = apArg[1];` |
|       2 | 2662 | `		}else{` |
|       - | 2663 | `			/* Use the default comparison function */` |
|     ! 0 | 2664 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2665 | `		}` |
|       - | 2666 | `		/* Do the merge sort */` |
|       3 | 2667 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2668 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2669 | `		/* Fix the last link broken by the merge */` |
|       5 | 2670 | `		while(pMap->pLast->pPrev){` |
|       3 | 2671 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2672 | `		}` |
|       3 | 2673 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2674 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2675 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2676 | `			return PH7_EXCEPTION;` |
|       - | 2677 | `		}` |
|       1 | 2678 | `	}` |
|       - | 2679 | `	/* All done,return TRUE */` |
|       3 | 2680 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2681 | `	return PH7_OK;` |
|       2 | 2682 | `}` |
|       - | 2683 | `/*` |
|       - | 2684 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2685 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2686 | ` *  function and maintain index association.` |
|       - | 2687 | ` * Parameters` |
|       - | 2688 | ` *  $array` |
|       - | 2689 | ` *   The input array.` |
|       - | 2690 | ` * $cmp_function` |
|       - | 2691 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2692 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2693 | ` *  to, or greater than the second.` |
|       - | 2694 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2695 | ` * Return` |
|       - | 2696 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2697 | ` */` |
|       2 | 2698 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2699 | `{` |
|       - | 2700 | `	ph7_hashmap *pMap;` |
|       - | 2701 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2702 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2703 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2704 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2705 | `		return PH7_OK;` |
|       - | 2706 | `	}` |
|       - | 2707 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2708 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2709 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2710 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2711 | `		ph7_value *pCallback = 0;` |
|       - | 2712 | `		ProcNodeCmp xCmp;` |
|       3 | 2713 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2714 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2715 | `			/* Point to the desired callback */` |
|       3 | 2716 | `			pCallback = apArg[1];` |
|       2 | 2717 | `		}else{` |
|       - | 2718 | `			/* Use the default comparison function */` |
|     ! 0 | 2719 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2720 | `		}` |
|       - | 2721 | `		/* Do the merge sort */` |
|       3 | 2722 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2723 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2724 | `		/* Fix the last link broken by the merge */` |
|       3 | 2725 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2726 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2727 | `		}` |
|       3 | 2728 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2729 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2730 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2731 | `			return PH7_EXCEPTION;` |
|       - | 2732 | `		}` |
|       1 | 2733 | `	}` |
|       - | 2734 | `	/* All done,return TRUE */` |
|       3 | 2735 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2736 | `	return PH7_OK;` |
|       2 | 2737 | `}` |
|       - | 2738 | `/*` |
|       - | 2739 | ` * bool shuffle(array &$array)` |
|       - | 2740 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2741 | ` * Parameters` |
|       - | 2742 | ` *  $array` |
|       - | 2743 | ` *   The input array.` |
|       - | 2744 | ` * Return` |
|       - | 2745 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2746 | ` *` |
|       - | 2747 | ` */` |
|       2 | 2748 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2749 | `{` |
|       - | 2750 | `	ph7_hashmap *pMap;` |
|       - | 2751 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2752 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2753 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2754 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2755 | `		return PH7_OK;` |
|       - | 2756 | `	}` |
|       - | 2757 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2758 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2759 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2760 | `	if( pMap->nEntry > 1 ){` |
|       - | 2761 | `		/* Do the merge sort */` |
|       3 | 2762 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2763 | `		/* Fix the last link broken by the merge */` |
|      10 | 2764 | `		while(pMap->pLast->pPrev){` |
|       8 | 2765 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2766 | `		}` |
|       1 | 2767 | `	}` |
|       - | 2768 | `	/* All done,return TRUE */` |
|       3 | 2769 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2770 | `	return PH7_OK;` |
|       2 | 2771 | `}` |
|       - | 2772 | `/*` |
|       - | 2773 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2774 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2775 | ` * Parameters` |
|       - | 2776 | ` *  $var` |
|       - | 2777 | ` *   The array or the object.` |
|       - | 2778 | ` * $mode` |
|       - | 2779 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2780 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2781 | ` *  all the elements of a multidimensional array.` |
|       - | 2782 | ` * Return` |
|       - | 2783 | ` *  Returns the number of elements in the array.` |
|       - | 2784 | ` */` |
|     842 | 2785 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2786 | `{` |
|     847 | 2787 | `	int bRecursive = FALSE;` |
|     847 | 2788 | `	int bCycleDetected = FALSE;` |
|       - | 2789 | `	sxi64 iCount;` |
|     847 | 2790 | `	if( nArg < 1 ){` |
|       3 | 2791 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2792 | `			"ArgumentCountError",` |
|       - | 2793 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2794 | `			);` |
|       - | 2795 | `	}` |
|     845 | 2796 | `	if( nArg > 2 ){` |
|       4 | 2797 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2798 | `			"ArgumentCountError",` |
|       - | 2799 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2800 | `			nArg` |
|       - | 2801 | `			);` |
|       - | 2802 | `	}` |
|       - | 2803 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - | 2804 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - | 2805 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|     843 | 2806 | `	if( nArg > 1 ){` |
|      45 | 2807 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      45 | 2808 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|      12 | 2809 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2810 | `				"ValueError",` |
|       - | 2811 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2812 | `				);` |
|       - | 2813 | `		}` |
|      34 | 2814 | `		bRecursive = iMode == 1;` |
|      16 | 2815 | `	}` |
|     835 | 2816 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2817 | `		/* Countable object: dispatch to ->count() */` |
|      35 | 2818 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      23 | 2819 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      23 | 2820 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      23 | 2821 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      20 | 2822 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2823 | `					"count",sizeof("count")-1);` |
|      20 | 2824 | `				if( pMeth ){` |
|       - | 2825 | `					ph7_value sResult;` |
|      20 | 2826 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      20 | 2827 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      20 | 2828 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      20 | 2829 | `					PH7_MemObjRelease(&sResult);` |
|      20 | 2830 | `					return PH7_OK;` |
|       - | 2831 | `				}` |
|     ! 0 | 2832 | `			}` |
|       1 | 2833 | `		}` |
|      22 | 2834 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2835 | `			"TypeError",` |
|       - | 2836 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       6 | 2837 | `			ph7_type_name(apArg[0])` |
|       - | 2838 | `			);` |
|       - | 2839 | `	}` |
|       - | 2840 | `	/* Count */` |
|     805 | 2841 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     805 | 2842 | `	if( bCycleDetected ){` |
|       3 | 2843 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2844 | `	}` |
|     805 | 2845 | `	ph7_result_int64(pCtx,iCount);` |
|     805 | 2846 | `	return PH7_OK;` |
|     426 | 2847 | `}` |
|       - | 2848 | `/*` |
|       - | 2849 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2850 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2851 | ` * Parameters` |
|       - | 2852 | ` * $key` |
|       - | 2853 | ` *   Value to check.` |
|       - | 2854 | ` * $search` |
|       - | 2855 | ` *  An array with keys to check.` |
|       - | 2856 | ` * Return` |
|       - | 2857 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2858 | ` */` |
|      84 | 2859 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2860 | `{` |
|       - | 2861 | `	sxi32 rc;` |
|      89 | 2862 | `	if( nArg != 2 ){` |
|       - | 2863 | `		/* PHP requires exactly two arguments */` |
|      12 | 2864 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2865 | `			"ArgumentCountError",` |
|       - | 2866 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2867 | `			nArg` |
|       - | 2868 | `			);` |
|       - | 2869 | `	}` |
|       - | 2870 | `	/* Make sure we are dealing with a valid hashmap */` |
|      83 | 2871 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2872 | `		/* Type mismatch -> TypeError */` |
|       8 | 2873 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2874 | `			"TypeError",` |
|       - | 2875 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2876 | `			ph7_type_name(apArg[1])` |
|       - | 2877 | `			);` |
|       - | 2878 | `	}` |
|       - | 2879 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      78 | 2880 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2881 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2882 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2883 | `			"use an empty string instead"` |
|       - | 2884 | `			);` |
|      77 | 2885 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2886 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2887 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2888 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2889 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2890 | `				,rVal` |
|       - | 2891 | `				);` |
|       1 | 2892 | `		}` |
|       1 | 2893 | `	}` |
|       - | 2894 | `	/* Perform the lookup */` |
|      78 | 2895 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2896 | `	/* lookup result */` |
|      78 | 2897 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      78 | 2898 | `	return PH7_OK;` |
|      47 | 2899 | `}` |
|       - | 2900 | `/*` |
|       - | 2901 | ` * value array_pop(array $array)` |
|       - | 2902 | ` *   POP the last inserted element from the array.` |
|       - | 2903 | ` * Parameter` |
|       - | 2904 | ` *  The array to get the value from.` |
|       - | 2905 | ` * Return` |
|       - | 2906 | ` *  Poped value or NULL on failure.` |
|       - | 2907 | ` */` |
|      18 | 2908 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2909 | `{` |
|       - | 2910 | `	ph7_hashmap *pMap;` |
|       - | 2911 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      23 | 2912 | `	if( nArg != 1 ){` |
|       8 | 2913 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2914 | `			"ArgumentCountError",` |
|       - | 2915 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2916 | `			nArg` |
|       - | 2917 | `			);` |
|       - | 2918 | `	}` |
|       - | 2919 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2920 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      18 | 2921 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 2922 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2923 | `			"Error",` |
|       - | 2924 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2925 | `			);` |
|       - | 2926 | `	}` |
|       - | 2927 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2928 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2929 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2930 | `			"TypeError",` |
|       - | 2931 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2932 | `			ph7_type_name(apArg[0])` |
|       - | 2933 | `			);` |
|       - | 2934 | `	}` |
|       9 | 2935 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2936 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2937 | `	if( pMap->nEntry < 1 ){` |
|       - | 2938 | `		/* Nothing to pop,return NULL */` |
|       3 | 2939 | `		ph7_result_null(pCtx);` |
|       2 | 2940 | `	}else{` |
|       7 | 2941 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2942 | `		ph7_value *pObj;` |
|       7 | 2943 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2944 | `		if( pObj ){` |
|       - | 2945 | `			/* Node value */` |
|       7 | 2946 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2947 | `			/* Unlink the node */` |
|       7 | 2948 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2949 | `		}else{` |
|     ! 0 | 2950 | `			ph7_result_null(pCtx);` |
|       - | 2951 | `		}` |
|       - | 2952 | `		/* Reset the cursor */` |
|       7 | 2953 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2954 | `	}` |
|       9 | 2955 | `	return PH7_OK;` |
|      14 | 2956 | `}` |
|       - | 2957 | `/*` |
|       - | 2958 | ` * int array_push($array,$var,...)` |
|       - | 2959 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2960 | ` * Parameters` |
|       - | 2961 | ` *  array` |
|       - | 2962 | ` *    The input array.` |
|       - | 2963 | ` *  var` |
|       - | 2964 | ` *   On or more value to push.` |
|       - | 2965 | ` * Return` |
|       - | 2966 | ` *  New array count (including old items).` |
|       - | 2967 | ` */` |
|      22 | 2968 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2969 | `{` |
|       - | 2970 | `	ph7_hashmap *pMap;` |
|       - | 2971 | `	sxi32 rc;` |
|       - | 2972 | `	int i;` |
|      27 | 2973 | `	if( nArg < 1 ){` |
|       4 | 2974 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2975 | `			"ArgumentCountError",` |
|       - | 2976 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2977 | `			nArg` |
|       - | 2978 | `			);` |
|       - | 2979 | `	}` |
|       - | 2980 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2981 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      24 | 2982 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 2983 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2984 | `			"Error",` |
|       - | 2985 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2986 | `			);` |
|       - | 2987 | `	}` |
|       - | 2988 | `	/* Make sure we are dealing with a valid hashmap */` |
|      18 | 2989 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2990 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2991 | `			"TypeError",` |
|       - | 2992 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2993 | `			ph7_type_name(apArg[0])` |
|       - | 2994 | `			);` |
|       - | 2995 | `	}` |
|       - | 2996 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2997 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2998 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2999 | `	/* Start pushing given values */` |
|      31 | 3000 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      17 | 3001 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      17 | 3002 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3003 | `			break;` |
|       - | 3004 | `		}` |
|       9 | 3005 | `	}` |
|       - | 3006 | `	/* Return the new count */` |
|      15 | 3007 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 3008 | `	return PH7_OK;` |
|      16 | 3009 | `}` |
|       - | 3010 | `/*` |
|       - | 3011 | ` * value array_shift(array $array)` |
|       - | 3012 | ` *   Shift an element off the beginning of array.` |
|       - | 3013 | ` * Parameter` |
|       - | 3014 | ` *  The array to get the value from.` |
|       - | 3015 | ` * Return` |
|       - | 3016 | ` *  Shifted value or NULL on failure.` |
|       - | 3017 | ` */` |
|      38 | 3018 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3019 | `{` |
|       - | 3020 | `	ph7_hashmap *pMap;` |
|       - | 3021 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      43 | 3022 | `	if( nArg != 1 ){` |
|       8 | 3023 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3024 | `			"ArgumentCountError",` |
|       - | 3025 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 3026 | `			nArg` |
|       - | 3027 | `			);` |
|       - | 3028 | `	}` |
|       - | 3029 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      39 | 3030 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 3031 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3032 | `			"Error",` |
|       - | 3033 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 3034 | `			);` |
|       - | 3035 | `	}` |
|       - | 3036 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 3037 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3038 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3039 | `			"TypeError",` |
|       - | 3040 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3041 | `			ph7_type_name(apArg[0])` |
|       - | 3042 | `			);` |
|       - | 3043 | `	}` |
|       - | 3044 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 3045 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      33 | 3046 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3047 | `	if( pMap->nEntry < 1 ){` |
|       - | 3048 | `		/* Empty hashmap,return NULL */` |
|       3 | 3049 | `		ph7_result_null(pCtx);` |
|       2 | 3050 | `	}else{` |
|      31 | 3051 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 3052 | `		ph7_value *pObj;` |
|       - | 3053 | `		sxu32 n;` |
|      31 | 3054 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      31 | 3055 | `		if( pObj ){` |
|       - | 3056 | `			/* Node value */` |
|      31 | 3057 | `			ph7_result_value(pCtx,pObj);` |
|       - | 3058 | `			/* Unlink the first node */` |
|      31 | 3059 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      18 | 3060 | `		}else{` |
|     ! 0 | 3061 | `			ph7_result_null(pCtx);` |
|       - | 3062 | `		}` |
|       - | 3063 | `		/* Rehash all int keys */` |
|      31 | 3064 | `		n = pMap->nEntry;` |
|      31 | 3065 | `		pEntry = pMap->pFirst;` |
|      31 | 3066 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 3067 | `		for(;;){` |
|      85 | 3068 | `			if( n < 1 ){` |
|      31 | 3069 | `				break;` |
|       - | 3070 | `			}` |
|      59 | 3071 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      59 | 3072 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 3073 | `			}` |
|       - | 3074 | `			/* Point to the next entry */` |
|      59 | 3075 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      59 | 3076 | `			n--;` |
|       5 | 3077 | `		}` |
|       - | 3078 | `		/* Reset the cursor */` |
|      31 | 3079 | `		pMap->pCur = pMap->pFirst;` |
|       - | 3080 | `	}` |
|      33 | 3081 | `	return PH7_OK;` |
|      24 | 3082 | `}` |
|       - | 3083 | `/*` |
|       - | 3084 | ` * Extract the node cursor value.` |
|       - | 3085 | ` */` |
|      24 | 3086 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 3087 | `{` |
|      25 | 3088 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 3089 | `	ph7_value *pVal;` |
|      25 | 3090 | `	if( pCur == 0 ){` |
|       - | 3091 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 3092 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3093 | `		return PH7_OK;` |
|       - | 3094 | `	}` |
|      25 | 3095 | `	if( iDirection != 0 ){` |
|       9 | 3096 | `		if( iDirection > 0 ){` |
|       - | 3097 | `			/* Point to the next entry */` |
|       7 | 3098 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 3099 | `			pCur = pMap->pCur;` |
|       4 | 3100 | `		}else{` |
|       - | 3101 | `			/* Point to the previous entry */` |
|       3 | 3102 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 3103 | `			pCur = pMap->pCur;` |
|       - | 3104 | `		}` |
|       9 | 3105 | `		if( pCur == 0 ){` |
|       - | 3106 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 3107 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3108 | `			return PH7_OK;` |
|       - | 3109 | `		}` |
|       4 | 3110 | `	}` |
|       - | 3111 | `	/* Point to the desired element */` |
|      25 | 3112 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 3113 | `	if( pVal ){` |
|      25 | 3114 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 3115 | `	}else{` |
|     ! 0 | 3116 | `		ph7_result_bool(pCtx,0);` |
|       - | 3117 | `	}` |
|      25 | 3118 | `	return PH7_OK;` |
|      13 | 3119 | `}` |
|       - | 3120 | `/*` |
|       - | 3121 | ` * value current(array $array)` |
|       - | 3122 | ` *  Return the current element in an array.` |
|       - | 3123 | ` * Parameter` |
|       - | 3124 | ` *  $input: The input array.` |
|       - | 3125 | ` * Return` |
|       - | 3126 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 3127 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3128 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3129 | ` *  is empty, current() returns FALSE.` |
|       - | 3130 | ` */` |
|      10 | 3131 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3132 | `{` |
|      11 | 3133 | `	if( nArg < 1 ){` |
|       - | 3134 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3135 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3136 | `		return PH7_OK;` |
|       - | 3137 | `	}` |
|       - | 3138 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 3139 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3140 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3141 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3142 | `		return PH7_OK;` |
|       - | 3143 | `	}` |
|      11 | 3144 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 3145 | `	return PH7_OK;` |
|       6 | 3146 | `}` |
|       - | 3147 | `/*` |
|       - | 3148 | ` * value next(array $input)` |
|       - | 3149 | ` *  Advance the internal array pointer of an array.` |
|       - | 3150 | ` * Parameter` |
|       - | 3151 | ` *  $input: The input array.` |
|       - | 3152 | ` * Return` |
|       - | 3153 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 3154 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 3155 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 3156 | ` */` |
|       6 | 3157 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3158 | `{` |
|       7 | 3159 | `	if( nArg < 1 ){` |
|       - | 3160 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3161 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3162 | `		return PH7_OK;` |
|       - | 3163 | `	}` |
|       - | 3164 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 3165 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3166 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3167 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3168 | `		return PH7_OK;` |
|       - | 3169 | `	}` |
|       7 | 3170 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 3171 | `	return PH7_OK;` |
|       4 | 3172 | `}` |
|       - | 3173 | `/*` |
|       - | 3174 | ` * value prev(array $input)` |
|       - | 3175 | ` *  Rewind the internal array pointer.` |
|       - | 3176 | ` * Parameter` |
|       - | 3177 | ` *  $input: The input array.` |
|       - | 3178 | ` * Return` |
|       - | 3179 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3180 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3181 | ` *  elements.` |
|       - | 3182 | ` */` |
|       2 | 3183 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3184 | `{` |
|       3 | 3185 | `	if( nArg < 1 ){` |
|       - | 3186 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3187 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3188 | `		return PH7_OK;` |
|       - | 3189 | `	}` |
|       - | 3190 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3191 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3192 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3193 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3194 | `		return PH7_OK;` |
|       - | 3195 | `	}` |
|       3 | 3196 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3197 | `	return PH7_OK;` |
|       2 | 3198 | `}` |
|       - | 3199 | `/*` |
|       - | 3200 | ` * value end(array $input)` |
|       - | 3201 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3202 | ` * Parameter` |
|       - | 3203 | ` *  $input: The input array.` |
|       - | 3204 | ` * Return` |
|       - | 3205 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3206 | ` */` |
|       2 | 3207 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3208 | `{` |
|       - | 3209 | `	ph7_hashmap *pMap;` |
|       3 | 3210 | `	if( nArg < 1 ){` |
|       - | 3211 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3212 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3213 | `		return PH7_OK;` |
|       - | 3214 | `	}` |
|       - | 3215 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3216 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3217 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3218 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3219 | `		return PH7_OK;` |
|       - | 3220 | `	}` |
|       - | 3221 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3222 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3223 | `	/* Point to the last node */` |
|       3 | 3224 | `	pMap->pCur = pMap->pLast;` |
|       - | 3225 | `	/* Return the last node value */` |
|       3 | 3226 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3227 | `	return PH7_OK;` |
|       2 | 3228 | `}` |
|       - | 3229 | `/*` |
|       - | 3230 | ` * value reset(array $array )` |
|       - | 3231 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3232 | ` * Parameter` |
|       - | 3233 | ` *  $input: The input array.` |
|       - | 3234 | ` * Return` |
|       - | 3235 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3236 | ` */` |
|       4 | 3237 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3238 | `{` |
|       - | 3239 | `	ph7_hashmap *pMap;` |
|       5 | 3240 | `	if( nArg < 1 ){` |
|       - | 3241 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3242 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3243 | `		return PH7_OK;` |
|       - | 3244 | `	}` |
|       - | 3245 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3246 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3247 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3248 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3249 | `		return PH7_OK;` |
|       - | 3250 | `	}` |
|       - | 3251 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3252 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3253 | `	/* Point to the first node */` |
|       5 | 3254 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3255 | `	/* Return the last node value if available */` |
|       5 | 3256 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3257 | `	return PH7_OK;` |
|       3 | 3258 | `}` |
|       - | 3259 | `/*` |
|       - | 3260 | ` * value key(array $array)` |
|       - | 3261 | ` *   Fetch a key from an array` |
|       - | 3262 | ` * Parameter` |
|       - | 3263 | ` *  $input` |
|       - | 3264 | ` *   The input array.` |
|       - | 3265 | ` * Return` |
|       - | 3266 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3267 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3268 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3269 | ` *  is empty, key() returns NULL.` |
|       - | 3270 | ` */` |
|       4 | 3271 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3272 | `{` |
|       - | 3273 | `	ph7_hashmap_node *pCur;` |
|       - | 3274 | `	ph7_hashmap *pMap;` |
|       5 | 3275 | `	if( nArg < 1 ){` |
|       - | 3276 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3277 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3278 | `		return PH7_OK;` |
|       - | 3279 | `	}` |
|       - | 3280 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3281 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3282 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3283 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3284 | `		return PH7_OK;` |
|       - | 3285 | `	}` |
|       5 | 3286 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3287 | `	pCur = pMap->pCur;` |
|       5 | 3288 | `	if( pCur == 0 ){` |
|       - | 3289 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3290 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3291 | `		return PH7_OK;` |
|       - | 3292 | `	}` |
|       5 | 3293 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3294 | `		/* Key is integer */` |
|     ! 0 | 3295 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3296 | `	}else{` |
|       - | 3297 | `		/* Key is blob */` |
|       7 | 3298 | `		ph7_result_string(pCtx,` |
|       4 | 3299 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3300 | `	}` |
|       5 | 3301 | `	return PH7_OK;` |
|       3 | 3302 | `}` |
|       - | 3303 | `/*` |
|       - | 3304 | ` * array each(array $input)` |
|       - | 3305 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3306 | ` * Parameter` |
|       - | 3307 | ` *  $input` |
|       - | 3308 | ` *    The input array.` |
|       - | 3309 | ` * Return` |
|       - | 3310 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3311 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3312 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3313 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3314 | ` *  each() returns FALSE.` |
|       - | 3315 | ` */` |
|      22 | 3316 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3317 | `{` |
|       - | 3318 | `	ph7_hashmap_node *pCur;` |
|       - | 3319 | `	ph7_hashmap *pMap;` |
|       - | 3320 | `	ph7_value *pArray;` |
|       - | 3321 | `	ph7_value *pVal;` |
|       - | 3322 | `	ph7_value sKey;` |
|      23 | 3323 | `	if( nArg < 1 ){` |
|       - | 3324 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3325 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3326 | `		return PH7_OK;` |
|       - | 3327 | `	}` |
|       - | 3328 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3329 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3330 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3331 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3332 | `		return PH7_OK;` |
|       - | 3333 | `	}` |
|       - | 3334 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3335 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3336 | `	if( pMap->pCur == 0 ){` |
|       - | 3337 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3338 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3339 | `		return PH7_OK;` |
|       - | 3340 | `	}` |
|      15 | 3341 | `	pCur = pMap->pCur;` |
|       - | 3342 | `	/* Create a new array */` |
|      15 | 3343 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3344 | `	if( pArray == 0 ){` |
|     ! 0 | 3345 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3346 | `		return PH7_OK;` |
|       - | 3347 | `	}` |
|      15 | 3348 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3349 | `	/* Insert the current value */` |
|      15 | 3350 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3351 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3352 | `	/* Make the key */` |
|      15 | 3353 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3354 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3355 | `	}else{` |
|       9 | 3356 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3357 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3358 | `	}` |
|       - | 3359 | `	/* Insert the current key */` |
|      15 | 3360 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3361 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3362 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3363 | `	/* Advance the cursor */` |
|      15 | 3364 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3365 | `	/* Return the current entry */` |
|      15 | 3366 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3367 | `	return PH7_OK;` |
|      12 | 3368 | `}` |
|       - | 3369 | `/*` |
|       - | 3370 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3371 | ` *  Create an array containing a range of elements` |
|       - | 3372 | ` * Parameter` |
|       - | 3373 | ` *  start` |
|       - | 3374 | ` *   First value of the sequence.` |
|       - | 3375 | ` *  limit` |
|       - | 3376 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3377 | ` *  step` |
|       - | 3378 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3379 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3380 | ` * Return` |
|       - | 3381 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3382 | ` * NOTE:` |
|       - | 3383 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3384 | ` */` |
|       2 | 3385 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3386 | `{` |
|       - | 3387 | `	ph7_value *pValue,*pArray;` |
|       - | 3388 | `	sxi64 iOfft,iLimit;` |
|       3 | 3389 | `	int iStep = 1;` |
|       - | 3390 |  |
|       3 | 3391 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3392 | `	if( nArg > 0 ){` |
|       - | 3393 | `		/* Extract the offset */` |
|       3 | 3394 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3395 | `		if( nArg > 1 ){` |
|       - | 3396 | `			/* Extract the limit */` |
|       3 | 3397 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3398 | `			if( nArg > 2 ){` |
|       - | 3399 | `				/* Extract the increment */` |
|       3 | 3400 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3401 | `				if( iStep < 1 ){` |
|       - | 3402 | `					/* Only positive number are allowed */` |
|       3 | 3403 | `					iStep = 1;` |
|       1 | 3404 | `				}` |
|       1 | 3405 | `			}` |
|       1 | 3406 | `		}` |
|       1 | 3407 | `	}` |
|       - | 3408 | `	/* Element container */` |
|       3 | 3409 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3410 | `	/* Create the new array */` |
|       3 | 3411 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3412 | `	if( pArray == 0 ){` |
|     ! 0 | 3413 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3414 | `	}` |
|       - | 3415 | `	/* Start filling */` |
|       3 | 3416 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3417 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3418 | `		/* Perform the insertion */` |
|     ! 0 | 3419 | `		if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       - | 3420 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3421 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3422 | `		}` |
|       - | 3423 | `		/* Increment */` |
|     ! 0 | 3424 | `		iOfft += iStep;` |
|     ! 0 | 3425 | `	}` |
|       - | 3426 | `	/* Return the new array */` |
|       3 | 3427 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3428 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3429 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3430 | `	 */` |
|       3 | 3431 | `	return PH7_OK;` |
|       2 | 3432 | `}` |
|       - | 3433 | `/*` |
|       - | 3434 | ` * array array_values(array $array)` |
|       - | 3435 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3436 | ` * Parameters` |
|       - | 3437 | ` *  $array` |
|       - | 3438 | ` *   The input array.` |
|       - | 3439 | ` * Return` |
|       - | 3440 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3441 | ` */` |
|      36 | 3442 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3443 | `{` |
|       - | 3444 | `	ph7_hashmap_node *pNode;` |
|       - | 3445 | `	ph7_hashmap *pMap;` |
|       - | 3446 | `	ph7_value *pArray;` |
|       - | 3447 | `	ph7_value *pObj;` |
|       - | 3448 | `	sxu32 n;` |
|      40 | 3449 | `	if( nArg != 1 ){` |
|       - | 3450 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       8 | 3451 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3452 | `			"ArgumentCountError",` |
|       - | 3453 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3454 | `			nArg` |
|       - | 3455 | `			);` |
|       - | 3456 | `	}` |
|       - | 3457 | `	/* Make sure we are dealing with a valid hashmap */` |
|      34 | 3458 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3459 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3460 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3461 | `			"TypeError",` |
|       - | 3462 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3463 | `			ph7_type_name(apArg[0])` |
|       - | 3464 | `			);` |
|       - | 3465 | `	}` |
|       - | 3466 | `	/* Point to the internal representation that describe the input hashmap */` |
|      32 | 3467 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3468 | `	/* Create a new array */` |
|      32 | 3469 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 3470 | `	if( pArray == 0 ){` |
|     ! 0 | 3471 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3472 | `		return PH7_OK;` |
|       - | 3473 | `	}` |
|       - | 3474 | `	/* Perform the requested operation */` |
|      32 | 3475 | `	pNode = pMap->pFirst;` |
|     104 | 3476 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      74 | 3477 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      74 | 3478 | `		if( pObj ){` |
|       - | 3479 | `			/* perform the insertion */` |
|      74 | 3480 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      36 | 3481 | `		}` |
|       - | 3482 | `		/* Point to the next entry */` |
|      74 | 3483 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      38 | 3484 | `	}` |
|       - | 3485 | `	/* return the new array */` |
|      32 | 3486 | `	ph7_result_value(pCtx,pArray);` |
|      32 | 3487 | `	return PH7_OK;` |
|      22 | 3488 | `}` |
|       - | 3489 | `/*` |
|       - | 3490 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3491 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3492 | ` * Parameters` |
|       - | 3493 | ` *  $input` |
|       - | 3494 | ` *   An array containing keys to return.` |
|       - | 3495 | ` * $search_value` |
|       - | 3496 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3497 | ` * $strict` |
|       - | 3498 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3499 | ` * Return` |
|       - | 3500 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3501 | ` */` |
|     136 | 3502 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3503 | `{` |
|       - | 3504 | `	ph7_hashmap_node *pNode;` |
|       - | 3505 | `	ph7_hashmap *pMap;` |
|       - | 3506 | `	ph7_value *pArray;` |
|       - | 3507 | `	ph7_value sObj;` |
|       - | 3508 | `	ph7_value sVal;` |
|       - | 3509 | `	SyString sKey;` |
|       - | 3510 | `	int bStrict;` |
|       - | 3511 | `	sxi32 rc;` |
|       - | 3512 | `	sxu32 n;` |
|     141 | 3513 | `	if( nArg < 1 ){` |
|       - | 3514 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3515 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3516 | `			"ArgumentCountError",` |
|       - | 3517 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3518 | `			);` |
|       - | 3519 | `	}` |
|       - | 3520 | `	/* Make sure we are dealing with a valid hashmap */` |
|     138 | 3521 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3522 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3523 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3524 | `			"TypeError",` |
|       - | 3525 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3526 | `			ph7_type_name(apArg[0])` |
|       - | 3527 | `			);` |
|       - | 3528 | `	}` |
|       - | 3529 | `	/* Point to the internal representation of the input hashmap */` |
|     135 | 3530 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3531 | `	/* Create a new array */` |
|     135 | 3532 | `	pArray = ph7_context_new_array(pCtx);` |
|     135 | 3533 | `	if( pArray == 0 ){` |
|     ! 0 | 3534 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3535 | `		return PH7_OK;` |
|       - | 3536 | `	}` |
|     135 | 3537 | `	bStrict = FALSE;` |
|     135 | 3538 | `	if( nArg > 2 ){` |
|       - | 3539 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3540 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3541 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3542 | `				"TypeError",` |
|       - | 3543 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3544 | `				ph7_type_name(apArg[2])` |
|       - | 3545 | `				);` |
|       - | 3546 | `		}` |
|       5 | 3547 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3548 | `	}` |
|       - | 3549 | `	/* Perform the requested operation */` |
|     132 | 3550 | `	pNode = pMap->pFirst;` |
|     132 | 3551 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    1108 | 3552 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     978 | 3553 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     126 | 3554 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      64 | 3555 | `		}else{` |
|     854 | 3556 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     854 | 3557 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3558 | `		}` |
|     978 | 3559 | `		rc = 0;` |
|     978 | 3560 | `		if( nArg > 1 ){` |
|      31 | 3561 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3562 | `			if( pValue ){` |
|      31 | 3563 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3564 | `				/* Filter key */` |
|      31 | 3565 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3566 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3567 | `			}` |
|      15 | 3568 | `		}` |
|     978 | 3569 | `		if( rc == 0 ){` |
|       - | 3570 | `			/* Perform the insertion */` |
|     960 | 3571 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     479 | 3572 | `		}` |
|     978 | 3573 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3574 | `		/* Point to the next entry */` |
|     978 | 3575 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     490 | 3576 | `	}` |
|       - | 3577 | `	/* return the new array */` |
|     132 | 3578 | `	ph7_result_value(pCtx,pArray);` |
|     132 | 3579 | `	return PH7_OK;` |
|      73 | 3580 | `}` |
|       - | 3581 | `/*` |
|       - | 3582 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3583 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3584 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3585 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3586 | ` * Parameters` |
|       - | 3587 | ` *  $arr1` |
|       - | 3588 | ` *   First array` |
|       - | 3589 | ` *  $arr2` |
|       - | 3590 | ` *   Second array` |
|       - | 3591 | ` * Return` |
|       - | 3592 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3593 | ` * Note` |
|       - | 3594 | ` *  This function is a symisc eXtension.` |
|       - | 3595 | ` */` |
|       4 | 3596 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3597 | `{` |
|       - | 3598 | `	ph7_hashmap *p1,*p2;` |
|       - | 3599 | `	int rc;` |
|       5 | 3600 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3601 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3602 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3603 | `		return PH7_OK;` |
|       - | 3604 | `	}` |
|       - | 3605 | `	/* Point to the hashmaps */` |
|       5 | 3606 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3607 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3608 | `	rc = (p1 == p2);` |
|       - | 3609 | `	/* Same instance? */` |
|       5 | 3610 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3611 | `	return PH7_OK;` |
|       3 | 3612 | `}` |
|       - | 3613 | `/*` |
|       - | 3614 | ` * array array_merge(array ...$arrays)` |
|       - | 3615 | ` *  Merge one or more arrays.` |
|       - | 3616 | ` * Parameters` |
|       - | 3617 | ` *  ...$arrays` |
|       - | 3618 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3619 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3620 | ` * Return` |
|       - | 3621 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3622 | ` *  with no arguments.` |
|       - | 3623 | ` */` |
|    1026 | 3624 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3625 | `{` |
|       - | 3626 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3627 | `	ph7_value *pArray;` |
|       - | 3628 | `	int i;` |
|       - | 3629 | `	/* Create a new array */` |
|    1031 | 3630 | `	pArray = ph7_context_new_array(pCtx);` |
|    1031 | 3631 | `	if( pArray == 0 ){` |
|     ! 0 | 3632 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3633 | `		return PH7_OK;` |
|       - | 3634 | `	}` |
|       - | 3635 | `	/* Point to the internal representation of the hashmap */` |
|    1031 | 3636 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3637 | `	/* Start merging */` |
|    3073 | 3638 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3639 | `		/* Make sure we are dealing with a valid hashmap */` |
|    2051 | 3640 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3641 | `			/* Type mismatch -> TypeError */` |
|       8 | 3642 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3643 | `				"TypeError",` |
|       - | 3644 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3645 | `				i + 1,` |
|       4 | 3646 | `				ph7_type_name(apArg[i])` |
|       - | 3647 | `				);` |
|     ! 0 | 3648 | `		}else{` |
|    2047 | 3649 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3650 | `			/* Merge the two hashmaps */` |
|    2047 | 3651 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3652 | `		}` |
|    1026 | 3653 | `	}` |
|       - | 3654 | `	/* Return the freshly created array */` |
|    1027 | 3655 | `	ph7_result_value(pCtx,pArray);` |
|    1027 | 3656 | `	return PH7_OK;` |
|     518 | 3657 | `}` |
|       - | 3658 | `/*` |
|       - | 3659 | ` * array array_copy(array $source)` |
|       - | 3660 | ` *  Make a blind copy of the target array.` |
|       - | 3661 | ` * Parameters` |
|       - | 3662 | ` *  $source` |
|       - | 3663 | ` *   Target array` |
|       - | 3664 | ` * Return` |
|       - | 3665 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3666 | ` * Note` |
|       - | 3667 | ` *  This function is a symisc eXtension.` |
|       - | 3668 | ` */` |
|      16 | 3669 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3670 | `{` |
|       - | 3671 | `	ph7_hashmap *pMap;` |
|       - | 3672 | `	ph7_value *pArray;` |
|      17 | 3673 | `	if( nArg < 1 ){` |
|       - | 3674 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3675 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3676 | `		return PH7_OK;` |
|       - | 3677 | `	}` |
|       - | 3678 | `	/* Create a new array */` |
|      17 | 3679 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3680 | `	if( pArray == 0 ){` |
|     ! 0 | 3681 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3682 | `		return PH7_OK;` |
|       - | 3683 | `	}` |
|       - | 3684 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3685 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3686 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3687 | `		/* Point to the internal representation of the source */` |
|      17 | 3688 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3689 | `		/* Perform the copy */` |
|      17 | 3690 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3691 | `	}else{` |
|       - | 3692 | `		/* Simple insertion */` |
|     ! 0 | 3693 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3694 | `	}` |
|       - | 3695 | `	/* Return the duplicated array */` |
|      17 | 3696 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3697 | `	return PH7_OK;` |
|       9 | 3698 | `}` |
|       - | 3699 | `/*` |
|       - | 3700 | ` * bool array_erase(array $source)` |
|       - | 3701 | ` *  Remove all elements from a given array.` |
|       - | 3702 | ` * Parameters` |
|       - | 3703 | ` *  $source` |
|       - | 3704 | ` *   Target array` |
|       - | 3705 | ` * Return` |
|       - | 3706 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3707 | ` * Note` |
|       - | 3708 | ` *  This function is a symisc eXtension.` |
|       - | 3709 | ` */` |
|      16 | 3710 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3711 | `{` |
|       - | 3712 | `	ph7_hashmap *pMap;` |
|      17 | 3713 | `	if( nArg < 1 ){` |
|       - | 3714 | `		/* Missing arguments */` |
|     ! 0 | 3715 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3716 | `		return PH7_OK;` |
|       - | 3717 | `	}` |
|       - | 3718 | `	/* Point to the target hashmap */` |
|      17 | 3719 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3720 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3721 | `	/* Erase */` |
|      17 | 3722 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3723 | `	return PH7_OK;` |
|       9 | 3724 | `}` |
|       - | 3725 | `/*` |
|       - | 3726 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3727 | ` *  Extract a slice of the array.` |
|       - | 3728 | ` * Parameters` |
|       - | 3729 | ` *  $array` |
|       - | 3730 | ` *    The input array.` |
|       - | 3731 | ` * $offset` |
|       - | 3732 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3733 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3734 | ` * $length (optional, nullable)` |
|       - | 3735 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3736 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3737 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3738 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3739 | ` * $preserve_keys (optional)` |
|       - | 3740 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3741 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3742 | ` * Return` |
|       - | 3743 | ` *   The new slice.` |
|       - | 3744 | ` */` |
|      50 | 3745 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3746 | `{` |
|       - | 3747 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3748 | `	ph7_hashmap_node *pCur;` |
|       - | 3749 | `	ph7_value *pArray;` |
|       - | 3750 | `	int iLength,iOfft;` |
|       - | 3751 | `	int bPreserve;` |
|       - | 3752 | `	sxi32 rc;` |
|      55 | 3753 | `	if( nArg < 2 ){` |
|       8 | 3754 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3755 | `			"ArgumentCountError",` |
|       - | 3756 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3757 | `			nArg` |
|       - | 3758 | `			);` |
|       - | 3759 | `	}` |
|      51 | 3760 | `	if( nArg > 4 ){` |
|       4 | 3761 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3762 | `			"ArgumentCountError",` |
|       - | 3763 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3764 | `			nArg` |
|       - | 3765 | `			);` |
|       - | 3766 | `	}` |
|      49 | 3767 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3768 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3769 | `			"TypeError",` |
|       - | 3770 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3771 | `			ph7_type_name(apArg[0])` |
|       - | 3772 | `			);` |
|       - | 3773 | `	}` |
|       - | 3774 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      62 | 3775 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      65 | 3776 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3777 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3778 | `			"TypeError",` |
|       - | 3779 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3780 | `			ph7_type_name(apArg[1])` |
|       - | 3781 | `			);` |
|       - | 3782 | `	}` |
|       - | 3783 | `	/* Validate $length type if provided: nullable int */` |
|      45 | 3784 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      26 | 3785 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3786 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3787 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3788 | `				"TypeError",` |
|       - | 3789 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3790 | `				ph7_type_name(apArg[2])` |
|       - | 3791 | `				);` |
|       - | 3792 | `		}` |
|       8 | 3793 | `	}` |
|       - | 3794 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      43 | 3795 | `	if( nArg > 3 ){` |
|      10 | 3796 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3797 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3798 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3799 | `				"TypeError",` |
|       - | 3800 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3801 | `				ph7_type_name(apArg[3])` |
|       - | 3802 | `				);` |
|       - | 3803 | `		}` |
|       2 | 3804 | `	}` |
|       - | 3805 | `	/* Point the internal representation of the target array */` |
|      41 | 3806 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      41 | 3807 | `	bPreserve = FALSE;` |
|       - | 3808 | `	/* Get the offset */` |
|      41 | 3809 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      41 | 3810 | `	if( iOfft < 0 ){` |
|       5 | 3811 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3812 | `		if( iOfft < 0 ){` |
|       3 | 3813 | `			iOfft = 0;` |
|       1 | 3814 | `		}` |
|       2 | 3815 | `	}` |
|      41 | 3816 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3817 | `		/* Offset past end of array, return empty array */` |
|       5 | 3818 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3819 | `		if( pArray == 0 ){` |
|     ! 0 | 3820 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3821 | `			return PH7_OK;` |
|       - | 3822 | `		}` |
|       5 | 3823 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3824 | `		return PH7_OK;` |
|       - | 3825 | `	}` |
|       - | 3826 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      37 | 3827 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      37 | 3828 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3829 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3830 | `		if( iLength < 0 ){` |
|       5 | 3831 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3832 | `		}` |
|      15 | 3833 | `		if( iLength < 0 ){` |
|       3 | 3834 | `			iLength = 0;` |
|       1 | 3835 | `		}` |
|      15 | 3836 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3837 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3838 | `		}` |
|       7 | 3839 | `	}` |
|      37 | 3840 | `	if( nArg > 3 ){` |
|       5 | 3841 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3842 | `	}` |
|       - | 3843 | `	/* Create a new array */` |
|      37 | 3844 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 3845 | `	if( pArray == 0 ){` |
|     ! 0 | 3846 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3847 | `		return PH7_OK;` |
|       - | 3848 | `	}` |
|      37 | 3849 | `	if( iLength < 1 ){` |
|       - | 3850 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3851 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3852 | `		return PH7_OK;` |
|       - | 3853 | `	}` |
|       - | 3854 | `	/* Point to the desired entry */` |
|      33 | 3855 | `	pCur = pSrc->pFirst;` |
|      28 | 3856 | `	for(;;){` |
|      61 | 3857 | `		if( iOfft < 1 ){` |
|      33 | 3858 | `			break;` |
|       - | 3859 | `		}` |
|       - | 3860 | `		/* Point to the next entry */` |
|      33 | 3861 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      33 | 3862 | `		iOfft--;` |
|       5 | 3863 | `	}` |
|       - | 3864 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 3865 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      51 | 3866 | `	for(;;){` |
|     107 | 3867 | `		if( iLength < 1 ){` |
|      33 | 3868 | `			break;` |
|       - | 3869 | `		}` |
|       - | 3870 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3871 | `		{` |
|      79 | 3872 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      79 | 3873 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3874 | `		}` |
|      79 | 3875 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3876 | `			break;` |
|       - | 3877 | `		}` |
|       - | 3878 | `		/* Point to the next entry */` |
|      79 | 3879 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      79 | 3880 | `		iLength--;` |
|       5 | 3881 | `	}` |
|       - | 3882 | `	/* Return the freshly created array */` |
|      33 | 3883 | `	ph7_result_value(pCtx,pArray);` |
|      33 | 3884 | `	return PH7_OK;` |
|      30 | 3885 | `}` |
|       - | 3886 | `/*` |
|       - | 3887 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3888 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3889 | ` * beginning (becomes the new pFirst).` |
|       - | 3890 | ` */` |
|      30 | 3891 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3892 | `{` |
|       - | 3893 | `	ph7_hashmap_node *pNode;` |
|       - | 3894 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3895 | `	pNode = pMap->pLast;` |
|      31 | 3896 | `	if( pNode == 0 ){` |
|     ! 0 | 3897 | `		return;` |
|       - | 3898 | `	}` |
|      31 | 3899 | `	if( pNode->pNext == 0 ){` |
|       - | 3900 | `		/* Only node in the list, nothing to move */` |
|       5 | 3901 | `		return;` |
|       - | 3902 | `	}` |
|      27 | 3903 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3904 | `		/* Already in the correct position */` |
|       9 | 3905 | `		return;` |
|       - | 3906 | `	}` |
|       - | 3907 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3908 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3909 | `	pMap->pLast->pPrev = 0;` |
|       - | 3910 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3911 | `	if( pAfter == 0 ){` |
|       - | 3912 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3913 | `		pNode->pNext = 0;` |
|       3 | 3914 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3915 | `		if( pMap->pFirst ){` |
|       3 | 3916 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3917 | `		}` |
|       3 | 3918 | `		pMap->pFirst = pNode;` |
|       2 | 3919 | `	}else{` |
|      17 | 3920 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3921 | `		pNode->pPrev = pOldNext;` |
|      17 | 3922 | `		pNode->pNext = pAfter;` |
|      17 | 3923 | `		pAfter->pPrev = pNode;` |
|      17 | 3924 | `		if( pOldNext ){` |
|      17 | 3925 | `			pOldNext->pNext = pNode;` |
|       9 | 3926 | `		}else{` |
|     ! 0 | 3927 | `			pMap->pLast = pNode;` |
|       - | 3928 | `		}` |
|       - | 3929 | `	}` |
|      16 | 3930 | `}` |
|       - | 3931 | `/*` |
|       - | 3932 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3933 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3934 | ` * Parameters` |
|       - | 3935 | ` *  $array` |
|       - | 3936 | ` *    The input array.` |
|       - | 3937 | ` *  $offset` |
|       - | 3938 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3939 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3940 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3941 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3942 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3943 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3944 | ` *  $length (optional)` |
|       - | 3945 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3946 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3947 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3948 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3949 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3950 | ` *  $replacement (optional)` |
|       - | 3951 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3952 | ` *    with elements from this array.` |
|       - | 3953 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3954 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3955 | ` *    offset.` |
|       - | 3956 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3957 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3958 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3959 | ` * Return` |
|       - | 3960 | ` *   A new array consisting of the extracted elements.` |
|       - | 3961 | ` */` |
|      54 | 3962 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3963 | `{` |
|       - | 3964 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3965 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3966 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3967 | `	int iLength,iOfft,i;` |
|       - | 3968 | `	sxi32 rc;` |
|      58 | 3969 | `	if( nArg < 2 ){` |
|       8 | 3970 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3971 | `			"ArgumentCountError",` |
|       - | 3972 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3973 | `			nArg` |
|       - | 3974 | `			);` |
|       - | 3975 | `	}` |
|      52 | 3976 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3977 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3978 | `			"TypeError",` |
|       - | 3979 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3980 | `			ph7_type_name(apArg[0])` |
|       - | 3981 | `			);` |
|       - | 3982 | `	}` |
|       - | 3983 | `	/* Point to the internal representation of the target array */` |
|      49 | 3984 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3985 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3986 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3987 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3988 | `	if( iOfft < 0 ){` |
|       7 | 3989 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3990 | `		if( iOfft < 0 ){` |
|       3 | 3991 | `			iOfft = 0;` |
|       2 | 3992 | `		}` |
|      46 | 3993 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3994 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3995 | `	}` |
|       - | 3996 | `	/* Get the length and clamp to valid range.` |
|       - | 3997 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3998 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3999 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 4000 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 4001 | `		if( iLength < 0 ){` |
|       7 | 4002 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 4003 | `			if( iLength < 0 ){` |
|       3 | 4004 | `				iLength = 0;` |
|       1 | 4005 | `			}` |
|       3 | 4006 | `		}` |
|      31 | 4007 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 4008 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 4009 | `		}` |
|      15 | 4010 | `	}` |
|       - | 4011 | `	/* Create the result array for removed elements */` |
|      49 | 4012 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 4013 | `	if( pArray == 0 ){` |
|     ! 0 | 4014 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4015 | `		return PH7_OK;` |
|       - | 4016 | `	}` |
|       - | 4017 | `	/* Get replacement array if provided */` |
|      49 | 4018 | `	pRep = 0;` |
|      49 | 4019 | `	if( nArg > 3 ){` |
|      21 | 4020 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 4021 | `			/* Perform an array cast */` |
|       3 | 4022 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 4023 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 4024 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 4025 | `			}` |
|       2 | 4026 | `		}else{` |
|      19 | 4027 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 4028 | `		}` |
|      21 | 4029 | `		if( pRep ){` |
|       - | 4030 | `			/* Reset the loop cursor */` |
|      21 | 4031 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 4032 | `		}` |
|      10 | 4033 | `	}` |
|       - | 4034 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 4035 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 4036 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 4037 | `		return PH7_OK;` |
|       - | 4038 | `	}` |
|       - | 4039 | `	/* Navigate to the offset position */` |
|      41 | 4040 | `	pCur = pSrc->pFirst;` |
|      85 | 4041 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 4042 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 4043 | `	}` |
|       - | 4044 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 4045 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 4046 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 4047 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 4048 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 4049 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 4050 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 4051 | `		pPrev = pCur->pPrev;` |
|      71 | 4052 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 4053 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 4054 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 4055 | `			break;` |
|       - | 4056 | `		}` |
|      71 | 4057 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 4058 | `	}` |
|       - | 4059 | `	/* Insert replacement elements at the correct position */` |
|      41 | 4060 | `	if( pRep ){` |
|       - | 4061 | `		ph7_value sSafeVal;` |
|      61 | 4062 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 4063 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 4064 | `			if( pRvalue ){` |
|       - | 4065 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 4066 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 4067 | `				 * since it points into that same pool. */` |
|      31 | 4068 | `				sSafeVal = *pRvalue;` |
|      31 | 4069 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 4070 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 4071 | `					pNewNode = pSrc->pLast;` |
|      31 | 4072 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 4073 | `					pInsertAfter = pNewNode;` |
|      15 | 4074 | `				}` |
|      15 | 4075 | `			}` |
|       1 | 4076 | `		}` |
|      10 | 4077 | `	}` |
|       - | 4078 | `	/* Return the freshly created array */` |
|      41 | 4079 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 4080 | `	return PH7_OK;` |
|      31 | 4081 | `}` |
|       - | 4082 | `/*` |
|       - | 4083 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 4084 | ` *  Checks if a value exists in an array.` |
|       - | 4085 | ` * Parameters` |
|       - | 4086 | ` *  $needle` |
|       - | 4087 | ` *   The searched value.` |
|       - | 4088 | ` *   Note:` |
|       - | 4089 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 4090 | ` * $haystack` |
|       - | 4091 | ` *  The target array.` |
|       - | 4092 | ` * $strict` |
|       - | 4093 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 4094 | ` *  will also check the types of the needle in the haystack.` |
|       - | 4095 | ` */` |
|   32300 | 4096 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4097 | `{` |
|       - | 4098 | `	ph7_value *pNeedle;` |
|       - | 4099 | `	int bStrict;` |
|       - | 4100 | `	int rc;` |
|   32305 | 4101 | `	if( nArg < 2 ){` |
|       - | 4102 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 4103 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4104 | `		return PH7_OK;` |
|       - | 4105 | `	}` |
|   32305 | 4106 | `	pNeedle = apArg[0];` |
|   32305 | 4107 | `	bStrict = 0;` |
|   32305 | 4108 | `	if( nArg > 2 ){` |
|      17 | 4109 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       8 | 4110 | `	}` |
|   32305 | 4111 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4112 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 4113 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 4114 | `		/* Set the comparison result */` |
|     ! 0 | 4115 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 4116 | `		return PH7_OK;` |
|       - | 4117 | `	}` |
|       - | 4118 | `	/* Perform the lookup */` |
|   32305 | 4119 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 4120 | `	/* Lookup result */` |
|   32305 | 4121 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   32305 | 4122 | `	return PH7_OK;` |
|   16155 | 4123 | `}` |
|       - | 4124 | `/*` |
|       - | 4125 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 4126 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 4127 | ` * Parameters` |
|       - | 4128 | ` * $needle` |
|       - | 4129 | ` *   The searched value.` |
|       - | 4130 | ` * $haystack` |
|       - | 4131 | ` *   The array.` |
|       - | 4132 | ` * $strict` |
|       - | 4133 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 4134 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 4135 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 4136 | ` * Return` |
|       - | 4137 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 4138 | ` */` |
|      28 | 4139 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4140 | `{` |
|       - | 4141 | `	ph7_hashmap_node *pEntry;` |
|       - | 4142 | `	ph7_value *pVal,sNeedle;` |
|       - | 4143 | `	ph7_hashmap *pMap;` |
|       - | 4144 | `	ph7_value sVal;` |
|       - | 4145 | `	int bStrict;` |
|       - | 4146 | `	sxu32 n;` |
|       - | 4147 | `	int rc;` |
|      33 | 4148 | `	if( nArg < 2 ){` |
|       - | 4149 | `		/* Missing argument,throw ArgumentCountError */` |
|       8 | 4150 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4151 | `			"ArgumentCountError",` |
|       - | 4152 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4153 | `			nArg` |
|       - | 4154 | `			);` |
|       - | 4155 | `	}` |
|      27 | 4156 | `	bStrict = FALSE;` |
|      27 | 4157 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4158 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4159 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4160 | `			"TypeError",` |
|       - | 4161 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4162 | `			ph7_type_name(apArg[1])` |
|       - | 4163 | `			);` |
|       - | 4164 | `	}` |
|      24 | 4165 | `	if( nArg > 2 ){` |
|       - | 4166 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4167 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4168 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4169 | `				"TypeError",` |
|       - | 4170 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4171 | `				ph7_type_name(apArg[2])` |
|       - | 4172 | `				);` |
|       - | 4173 | `		}` |
|       9 | 4174 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4175 | `	}` |
|       - | 4176 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4177 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4178 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4179 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4180 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4181 | `	pEntry = pMap->pFirst;` |
|      21 | 4182 | `	n = pMap->nEntry;` |
|      23 | 4183 | `	for(;;){` |
|      47 | 4184 | `		if( !n ){` |
|       9 | 4185 | `			break;` |
|       - | 4186 | `		}` |
|       - | 4187 | `		/* Extract node value */` |
|      39 | 4188 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4189 | `		if( pVal ){` |
|       - | 4190 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4191 | `			 * can change their type.` |
|       - | 4192 | `			 */` |
|      39 | 4193 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4194 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4195 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4196 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4197 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4198 | `			if( rc == 0 ){` |
|       - | 4199 | `				/* Match found,return key */` |
|      13 | 4200 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4201 | `					/* INT key */` |
|       7 | 4202 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4203 | `				}else{` |
|       7 | 4204 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4205 | `					/* Blob key */` |
|       7 | 4206 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4207 | `				}` |
|      13 | 4208 | `				return PH7_OK;` |
|       - | 4209 | `			}` |
|      13 | 4210 | `		}` |
|       - | 4211 | `		/* Point to the next entry */` |
|      27 | 4212 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4213 | `		n--;` |
|       1 | 4214 | `	}` |
|       - | 4215 | `	/* No such value,return FALSE */` |
|       9 | 4216 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4217 | `	return PH7_OK;` |
|      19 | 4218 | `}` |
|       - | 4219 | `/*` |
|       - | 4220 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4221 | ` *  Computes the difference of arrays.` |
|       - | 4222 | ` * Parameters` |
|       - | 4223 | ` *  $array1` |
|       - | 4224 | ` *    The array to compare from` |
|       - | 4225 | ` *  $array2` |
|       - | 4226 | ` *    An array to compare against` |
|       - | 4227 | ` *  $...` |
|       - | 4228 | ` *   More arrays to compare against` |
|       - | 4229 | ` * Return` |
|       - | 4230 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4231 | ` *  are not present in any of the other arrays.` |
|       - | 4232 | ` */` |
|      22 | 4233 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4234 | `{` |
|       - | 4235 | `	ph7_hashmap_node *pEntry;` |
|       - | 4236 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4237 | `	ph7_value *pArray;` |
|       - | 4238 | `	ph7_value *pVal;` |
|       - | 4239 | `	sxi32 rc;` |
|       - | 4240 | `	sxu32 n;` |
|       - | 4241 | `	int i;` |
|       - | 4242 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4243 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4244 | `	 * debugging difficult. */` |
|      26 | 4245 | `	if( nArg < 1 ){` |
|       4 | 4246 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4247 | `			"ArgumentCountError",` |
|       - | 4248 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4249 | `			nArg` |
|       - | 4250 | `			);` |
|       - | 4251 | `	}` |
|      23 | 4252 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4253 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4254 | `			"TypeError",` |
|       - | 4255 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4256 | `			ph7_type_name(apArg[0])` |
|       - | 4257 | `			);` |
|       - | 4258 | `	}` |
|      36 | 4259 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4260 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4261 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4262 | `				"TypeError",` |
|       - | 4263 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4264 | `				i + 1,` |
|       2 | 4265 | `				ph7_type_name(apArg[i])` |
|       - | 4266 | `				);` |
|       - | 4267 | `		}` |
|       9 | 4268 | `	}` |
|      17 | 4269 | `	if( nArg == 1 ){` |
|       - | 4270 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4271 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4272 | `		return PH7_OK;` |
|       - | 4273 | `	}` |
|       - | 4274 | `	/* Create a new array */` |
|      15 | 4275 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4276 | `	if( pArray == 0 ){` |
|     ! 0 | 4277 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4278 | `		return PH7_OK;` |
|       - | 4279 | `	}` |
|       - | 4280 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4281 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4282 | `	/* Perform the diff */` |
|      15 | 4283 | `	pEntry = pSrc->pFirst;` |
|      15 | 4284 | `	n = pSrc->nEntry;` |
|      27 | 4285 | `	for(;;){` |
|      55 | 4286 | `		if( n < 1 ){` |
|      15 | 4287 | `			break;` |
|       - | 4288 | `		}` |
|       - | 4289 | `		/* Extract the node value */` |
|      41 | 4290 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4291 | `		if( pVal ){` |
|      69 | 4292 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4293 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4294 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4295 | `				/* Perform the lookup */` |
|      45 | 4296 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4297 | `				if( rc == SXRET_OK ){` |
|       - | 4298 | `					/* Value exist */` |
|      17 | 4299 | `					break;` |
|       - | 4300 | `				}` |
|      15 | 4301 | `			}` |
|      41 | 4302 | `			if( i >= nArg ){` |
|       - | 4303 | `				/* Perform the insertion */` |
|      25 | 4304 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4305 | `			}` |
|      20 | 4306 | `		}` |
|       - | 4307 | `		/* Point to the next entry */` |
|      41 | 4308 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4309 | `		n--;` |
|       1 | 4310 | `	}` |
|       - | 4311 | `	/* Return the freshly created array */` |
|      15 | 4312 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4313 | `	return PH7_OK;` |
|      15 | 4314 | `}` |
|       - | 4315 | `/*` |
|       - | 4316 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4317 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4318 | ` * Parameters` |
|       - | 4319 | ` *  $array1` |
|       - | 4320 | ` *    The array to compare from` |
|       - | 4321 | ` *  $array2` |
|       - | 4322 | ` *    An array to compare against` |
|       - | 4323 | ` *  $...` |
|       - | 4324 | ` *   More arrays to compare against.` |
|       - | 4325 | ` * $callback` |
|       - | 4326 | ` *  The callback comparison function.` |
|       - | 4327 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4328 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4329 | ` *  than the second.` |
|       - | 4330 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4331 | ` * Return` |
|       - | 4332 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4333 | ` *  are not present in any of the other arrays.` |
|       - | 4334 | ` */` |
|      22 | 4335 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4336 | `{` |
|       - | 4337 | `	ph7_hashmap_node *pEntry;` |
|       - | 4338 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4339 | `	ph7_value *pCallback;` |
|       - | 4340 | `	ph7_value *pArray;` |
|       - | 4341 | `	ph7_value *pVal;` |
|       - | 4342 | `	sxi32 rc;` |
|       - | 4343 | `	sxu32 n;` |
|       - | 4344 | `	int i;` |
|       - | 4345 |  |
|       - | 4346 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      27 | 4347 | `	if( nArg < 2 ){` |
|       4 | 4348 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4349 | `			"ArgumentCountError",` |
|       - | 4350 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4351 | `			nArg` |
|       - | 4352 | `			);` |
|       - | 4353 | `	}` |
|      25 | 4354 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4355 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4356 | `			"TypeError",` |
|       - | 4357 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4358 | `			ph7_type_name(apArg[0])` |
|       - | 4359 | `			);` |
|       - | 4360 | `	}` |
|       - | 4361 |  |
|      23 | 4362 | `	if( nArg == 2 ){` |
|       - | 4363 | `		/* Only the original array and the callback were provided. */` |
|       - | 4364 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4365 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4366 | `		 * validation order.` |
|       - | 4367 | `		 */` |
|       4 | 4368 | `	} else {` |
|       - | 4369 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      27 | 4370 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      19 | 4371 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      11 | 4372 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4373 | `					"TypeError",` |
|       - | 4374 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4375 | `					i + 1,` |
|       6 | 4376 | `					ph7_type_name(apArg[i])` |
|       - | 4377 | `					);` |
|       - | 4378 | `			}` |
|       7 | 4379 | `		}` |
|       - | 4380 | `	}` |
|       - | 4381 |  |
|       - | 4382 | `	/* Identify the callback (always expected as the last argument). */` |
|      16 | 4383 | `	pCallback = apArg[nArg - 1];` |
|       - | 4384 | `	/* Validate the callback to match PHP's error messages. */` |
|      16 | 4385 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       9 | 4386 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4387 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4388 | `				"TypeError",` |
|       - | 4389 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4390 | `				nArg` |
|       - | 4391 | `				);` |
|       - | 4392 | `		}` |
|       6 | 4393 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4394 | `			int len;` |
|       3 | 4395 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4396 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4397 | `				"TypeError",` |
|       - | 4398 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4399 | `				nArg,` |
|       1 | 4400 | `				zName` |
|       - | 4401 | `				);` |
|       - | 4402 | `		}` |
|       4 | 4403 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4404 | `			"TypeError",` |
|       - | 4405 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4406 | `			nArg` |
|       - | 4407 | `			);` |
|       - | 4408 | `	}` |
|       - | 4409 |  |
|       7 | 4410 | `	if( nArg == 2 ){` |
|       - | 4411 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4412 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4413 | `		return PH7_OK;` |
|       - | 4414 | `	}` |
|       - | 4415 |  |
|       - | 4416 | `	/* Create a new array */` |
|       5 | 4417 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4418 | `	if( pArray == 0 ){` |
|     ! 0 | 4419 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4420 | `		return PH7_OK;` |
|       - | 4421 | `	}` |
|       - | 4422 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 4423 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4424 | `	/* Perform the diff */` |
|       5 | 4425 | `	pEntry = pSrc->pFirst;` |
|       5 | 4426 | `	n = pSrc->nEntry;` |
|       5 | 4427 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 4428 | `	for(;;){` |
|      11 | 4429 | `		if( n < 1 ){` |
|       3 | 4430 | `			break;` |
|       - | 4431 | `		}` |
|       - | 4432 | `		/* Extract the node value */` |
|       9 | 4433 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4434 | `		if( pVal ){` |
|      15 | 4435 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4436 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4437 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4438 | `				/* Perform the lookup */` |
|       9 | 4439 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       9 | 4440 | `				if( rc == SXRET_OK ){` |
|       - | 4441 | `					/* Value exist */` |
|       3 | 4442 | `					break;` |
|       - | 4443 | `				}` |
|       4 | 4444 | `			}` |
|       9 | 4445 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 4446 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 4447 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 4448 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 4449 | `				return PH7_EXCEPTION;` |
|       - | 4450 | `			}` |
|       7 | 4451 | `			if( i >= (nArg - 1)){` |
|       - | 4452 | `				/* Perform the insertion */` |
|       5 | 4453 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4454 | `			}` |
|       3 | 4455 | `		}` |
|       - | 4456 | `		/* Point to the next entry */` |
|       7 | 4457 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4458 | `		n--;` |
|       1 | 4459 | `	}` |
|       - | 4460 | `	/* Return the freshly created array */` |
|       3 | 4461 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4462 | `	return PH7_OK;` |
|      16 | 4463 | `}` |
|       - | 4464 | `/*` |
|       - | 4465 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4466 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4467 | ` * Parameters` |
|       - | 4468 | ` *  $array1` |
|       - | 4469 | ` *    The array to compare from` |
|       - | 4470 | ` *  $array2` |
|       - | 4471 | ` *    An array to compare against` |
|       - | 4472 | ` *  $...` |
|       - | 4473 | ` *   More arrays to compare against` |
|       - | 4474 | ` * Return` |
|       - | 4475 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4476 | ` *  are not present in any of the other arrays.` |
|       - | 4477 | ` */` |
|      20 | 4478 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4479 | `{` |
|       - | 4480 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4481 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4482 | `	ph7_value *pArray;` |
|       - | 4483 | `	ph7_value *pVal;` |
|       - | 4484 | `	sxi32 rc;` |
|       - | 4485 | `	sxu32 n;` |
|       - | 4486 | `	int i;` |
|       - | 4487 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4488 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4489 | `	 * accompanying integration tests to pass. */` |
|      25 | 4490 | `	if( nArg < 1 ){` |
|       4 | 4491 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4492 | `			"ArgumentCountError",` |
|       - | 4493 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4494 | `			nArg` |
|       - | 4495 | `			);` |
|       - | 4496 | `	}` |
|      22 | 4497 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4498 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4499 | `			"TypeError",` |
|       - | 4500 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4501 | `			ph7_type_name(apArg[0])` |
|       - | 4502 | `			);` |
|       - | 4503 | `	}` |
|      33 | 4504 | `	for(i = 1 ; i < nArg ; i++){` |
|      21 | 4505 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 4506 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4507 | `				"TypeError",` |
|       - | 4508 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4509 | `				i + 1,` |
|       4 | 4510 | `				ph7_type_name(apArg[i])` |
|       - | 4511 | `				);` |
|       - | 4512 | `		}` |
|       9 | 4513 | `	}` |
|      13 | 4514 | `	if( nArg == 1 ){` |
|       - | 4515 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4516 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4517 | `		return PH7_OK;` |
|       - | 4518 | `	}` |
|       - | 4519 | `	/* Create a new array */` |
|      11 | 4520 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4521 | `	if( pArray == 0 ){` |
|     ! 0 | 4522 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4523 | `		return PH7_OK;` |
|       - | 4524 | `	}` |
|       - | 4525 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4526 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4527 | `	/* Perform the diff */` |
|      11 | 4528 | `	pEntry = pSrc->pFirst;` |
|      11 | 4529 | `	n = pSrc->nEntry;` |
|      11 | 4530 | `	pN1 = pN2 = 0;` |
|      29 | 4531 | `	for(;;){` |
|       - | 4532 | `		int keep;` |
|      35 | 4533 | `		if( n < 1 ){` |
|      11 | 4534 | `			break;` |
|       - | 4535 | `		}` |
|       - | 4536 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4537 | `		keep = 1;` |
|      41 | 4538 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4539 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4540 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4541 | `			/* Perform a key lookup first */` |
|      29 | 4542 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4543 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4544 | `			}else{` |
|      17 | 4545 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4546 | `			}` |
|      29 | 4547 | `			if( rc != SXRET_OK ){` |
|       - | 4548 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4549 | `				continue;` |
|       - | 4550 | `			}` |
|       - | 4551 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4552 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4553 | `			if( pVal ){` |
|       - | 4554 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4555 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4556 | `				if( pVal2 ){` |
|      15 | 4557 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4558 | `					if( cmp == 0 ){` |
|       - | 4559 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4560 | `						keep = 0;` |
|      13 | 4561 | `						break;` |
|       - | 4562 | `					}` |
|       1 | 4563 | `				}` |
|       1 | 4564 | `			}` |
|       2 | 4565 | `		}` |
|      25 | 4566 | `		if( keep ){` |
|       - | 4567 | `			/* Perform the insertion */` |
|      13 | 4568 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4569 | `		}` |
|       - | 4570 | `		/* Point to the next entry */` |
|      25 | 4571 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4572 | `		n--;` |
|       1 | 4573 | `	}` |
|       - | 4574 | `	/* Return the freshly created array */` |
|      11 | 4575 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4576 | `	return PH7_OK;` |
|      15 | 4577 | `}` |
|       - | 4578 | `/*` |
|       - | 4579 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4580 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4581 | ` *  by a user supplied callback function.` |
|       - | 4582 | ` * Parameters` |
|       - | 4583 | ` *  $array1` |
|       - | 4584 | ` *    The array to compare from` |
|       - | 4585 | ` *  $array2` |
|       - | 4586 | ` *    An array to compare against` |
|       - | 4587 | ` *  $...` |
|       - | 4588 | ` *   More arrays to compare against.` |
|       - | 4589 | ` *  $key_compare_func` |
|       - | 4590 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4591 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4592 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4593 | ` * Return` |
|       - | 4594 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4595 | ` *  are not present in any of the other arrays.` |
|       - | 4596 | ` */` |
|      24 | 4597 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4598 | `{` |
|       - | 4599 | `	ph7_hashmap_node *pEntry;` |
|       - | 4600 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4601 | `	ph7_value *pCallback;` |
|       - | 4602 | `	ph7_value *pArray;` |
|       - | 4603 | `	sxi32 rc;` |
|       - | 4604 | `	sxu32 n;` |
|       - | 4605 | `	int i;` |
|       - | 4606 |  |
|       - | 4607 | `	/* Argument validation mimicking PHP errors. */` |
|      29 | 4608 | `	if( nArg < 2 ){` |
|       4 | 4609 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4610 | `			"ArgumentCountError",` |
|       - | 4611 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4612 | `			nArg` |
|       - | 4613 | `			);` |
|       - | 4614 | `	}` |
|      26 | 4615 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4616 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4617 | `			"TypeError",` |
|       - | 4618 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4619 | `			ph7_type_name(apArg[0])` |
|       - | 4620 | `			);` |
|       - | 4621 | `	}` |
|       - | 4622 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4623 | `	 * expected to be a callback. */` |
|      38 | 4624 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      19 | 4625 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4626 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4627 | `				"TypeError",` |
|       - | 4628 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4629 | `				i + 1,` |
|       2 | 4630 | `				ph7_type_name(apArg[i])` |
|       - | 4631 | `				);` |
|       - | 4632 | `		}` |
|       9 | 4633 | `	}` |
|       - | 4634 | `	/* Point to the callback value */` |
|      22 | 4635 | `	pCallback = apArg[nArg - 1];` |
|      22 | 4636 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4637 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4638 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4639 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4640 | `		 * string given" which we also reproduce. */` |
|       9 | 4641 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4642 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4643 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4644 | `				"TypeError",` |
|       - | 4645 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4646 | `				nArg` |
|       - | 4647 | `				);` |
|       - | 4648 | `		}` |
|       6 | 4649 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4650 | `			/* neither array nor string */` |
|       8 | 4651 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4652 | `				"TypeError",` |
|       - | 4653 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4654 | `				nArg` |
|       - | 4655 | `				);` |
|       - | 4656 | `		}` |
|       - | 4657 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4658 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4659 | `			"TypeError",` |
|       - | 4660 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4661 | `			nArg,` |
|     ! 0 | 4662 | `			ph7_type_name(pCallback)` |
|       - | 4663 | `			);` |
|       - | 4664 | `	}` |
|      13 | 4665 | `	if( nArg == 2 ){` |
|       - | 4666 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4667 | `		 * input array. */` |
|       3 | 4668 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4669 | `		return PH7_OK;` |
|       - | 4670 | `	}` |
|       - | 4671 | `	/* Create a new array */` |
|      11 | 4672 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4673 | `	if( pArray == 0 ){` |
|     ! 0 | 4674 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4675 | `		return PH7_OK;` |
|       - | 4676 | `	}` |
|       - | 4677 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4678 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4679 | `	/* Perform the diff */` |
|      11 | 4680 | `	pEntry = pSrc->pFirst;` |
|      11 | 4681 | `	n = pSrc->nEntry;` |
|      21 | 4682 | `	for(;;){` |
|       - | 4683 | `		int keep;` |
|      27 | 4684 | `		if( n < 1 ){` |
|       9 | 4685 | `			break;` |
|       - | 4686 | `		}` |
|      19 | 4687 | `		keep = 1;` |
|      31 | 4688 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4689 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 4690 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4691 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 4692 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 4693 | `			while( pIt ){` |
|       - | 4694 | `				/* build temporary key values for callback */` |
|       - | 4695 | `				ph7_value key1, key2, result;` |
|       - | 4696 | `				/* initialise only once using the appropriate helper */` |
|      33 | 4697 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4698 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4699 | `				}else{` |
|       - | 4700 | `					SyString sStr;` |
|      33 | 4701 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4702 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4703 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 4704 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4705 | `				}` |
|      33 | 4706 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4707 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4708 | `				}else{` |
|       - | 4709 | `					SyString sStr;` |
|      33 | 4710 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4711 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4712 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 4713 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4714 | `				}` |
|      33 | 4715 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4716 | `				/* call user callback with (key1, key2) */` |
|       - | 4717 | `				{` |
|       - | 4718 | `					ph7_value *apK[2];` |
|      33 | 4719 | `					apK[0] = &key1;` |
|      33 | 4720 | `					apK[1] = &key2;` |
|      33 | 4721 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4722 | `				}` |
|      33 | 4723 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 4724 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 4725 | `					 * array_uintersect (which signal back from` |
|       - | 4726 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 4727 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 4728 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 4729 | `					PH7_MemObjRelease(&result);` |
|       3 | 4730 | `					PH7_MemObjRelease(&key1);` |
|       3 | 4731 | `					PH7_MemObjRelease(&key2);` |
|       3 | 4732 | `					return PH7_EXCEPTION;` |
|       - | 4733 | `				}` |
|      31 | 4734 | `				if( rc == SXRET_OK ){` |
|      31 | 4735 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4736 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4737 | `					}` |
|      31 | 4738 | `					if( result.x.iVal == 0 ){` |
|       - | 4739 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4740 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4741 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4742 | `						if( pVal1 && pVal2 ){` |
|      13 | 4743 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4744 | `								keep = 0;` |
|       9 | 4745 | `								PH7_MemObjRelease(&result);` |
|       - | 4746 | `								/* release keys too before breaking */` |
|       9 | 4747 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4748 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4749 | `								break;` |
|       - | 4750 | `							}` |
|       2 | 4751 | `						}` |
|       2 | 4752 | `					}` |
|      11 | 4753 | `				}` |
|      23 | 4754 | `				PH7_MemObjRelease(&result);` |
|      23 | 4755 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4756 | `				PH7_MemObjRelease(&key2);` |
|       - | 4757 | `				/* move to next node */` |
|      23 | 4758 | `				pIt = pIt->pPrev;` |
|      23 | 4759 | `				if( keep == 0 ) break;` |
|       1 | 4760 | `			}` |
|      21 | 4761 | `			if( keep == 0 ) break;` |
|       7 | 4762 | `		}` |
|      17 | 4763 | `		if( keep ){` |
|       - | 4764 | `			/* Perform the insertion */` |
|       9 | 4765 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4766 | `		}` |
|       - | 4767 | `		/* Point to the next entry */` |
|      17 | 4768 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4769 | `		n--;` |
|       1 | 4770 | `	}` |
|       - | 4771 | `	/* Return the freshly created array */` |
|       9 | 4772 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4773 | `	return PH7_OK;` |
|      17 | 4774 | `}` |
|       - | 4775 | `/*` |
|       - | 4776 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4777 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4778 | ` * Parameters` |
|       - | 4779 | ` *  $array1` |
|       - | 4780 | ` *    The array to compare from` |
|       - | 4781 | ` *  $array2` |
|       - | 4782 | ` *    An array to compare against` |
|       - | 4783 | ` *  $...` |
|       - | 4784 | ` *   More arrays to compare against` |
|       - | 4785 | ` * Return` |
|       - | 4786 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4787 | ` *  in any of the other arrays.` |
|       - | 4788 | ` * Note that NULL is returned on failure.` |
|       - | 4789 | ` */` |
|      14 | 4790 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4791 | `{` |
|       - | 4792 | `	ph7_hashmap_node *pEntry;` |
|       - | 4793 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4794 | `	ph7_value *pArray;` |
|       - | 4795 | `	sxi32 rc;` |
|       - | 4796 | `	sxu32 n;` |
|       - | 4797 | `	int i;` |
|       - | 4798 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4799 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4800 | `	 * helpers. */` |
|      18 | 4801 | `	if( nArg < 1 ){` |
|       4 | 4802 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4803 | `			"ArgumentCountError",` |
|       - | 4804 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4805 | `			nArg` |
|       - | 4806 | `			);` |
|       - | 4807 | `	}` |
|      15 | 4808 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4809 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4810 | `			"TypeError",` |
|       - | 4811 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4812 | `			ph7_type_name(apArg[0])` |
|       - | 4813 | `			);` |
|       - | 4814 | `	}` |
|      20 | 4815 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4816 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4817 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4818 | `				"TypeError",` |
|       - | 4819 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4820 | `				i + 1,` |
|       2 | 4821 | `				ph7_type_name(apArg[i])` |
|       - | 4822 | `				);` |
|       - | 4823 | `		}` |
|       5 | 4824 | `	}` |
|       9 | 4825 | `	if( nArg == 1 ){` |
|       - | 4826 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4827 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4828 | `		return PH7_OK;` |
|       - | 4829 | `	}` |
|       - | 4830 | `	/* Create a new array */` |
|       7 | 4831 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4832 | `	if( pArray == 0 ){` |
|     ! 0 | 4833 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4834 | `		return PH7_OK;` |
|       - | 4835 | `	}` |
|       - | 4836 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4837 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4838 | `	/* Perfrom the diff */` |
|       7 | 4839 | `	pEntry = pSrc->pFirst;` |
|       7 | 4840 | `	n = pSrc->nEntry;` |
|      12 | 4841 | `	for(;;){` |
|      25 | 4842 | `		if( n < 1 ){` |
|       7 | 4843 | `			break;` |
|       - | 4844 | `		}` |
|      31 | 4845 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4846 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4847 | `				/* ignore */` |
|     ! 0 | 4848 | `				continue;` |
|       - | 4849 | `			}` |
|      23 | 4850 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4851 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4852 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4853 | `				/* Blob lookup */` |
|      17 | 4854 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4855 | `			}else{` |
|       - | 4856 | `				/* Int lookup */` |
|       7 | 4857 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4858 | `			}` |
|      23 | 4859 | `			if( rc == SXRET_OK ){` |
|       - | 4860 | `				/* Key exists,break immediately */` |
|      11 | 4861 | `				break;` |
|       - | 4862 | `			}` |
|       7 | 4863 | `		}` |
|      19 | 4864 | `		if( i >= nArg ){` |
|       - | 4865 | `			/* Perform the insertion */` |
|       9 | 4866 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4867 | `		}` |
|       - | 4868 | `		/* Point to the next entry */` |
|      19 | 4869 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4870 | `		n--;` |
|       1 | 4871 | `	}` |
|       - | 4872 | `	/* Return the freshly created array */` |
|       7 | 4873 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4874 | `	return PH7_OK;` |
|      11 | 4875 | `}` |
|       - | 4876 | `/*` |
|       - | 4877 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4878 | ` *  Computes the intersection of arrays.` |
|       - | 4879 | ` * Parameters` |
|       - | 4880 | ` *  $array1` |
|       - | 4881 | ` *    The array to compare from` |
|       - | 4882 | ` *  $array2` |
|       - | 4883 | ` *    An array to compare against` |
|       - | 4884 | ` *  $...` |
|       - | 4885 | ` *   More arrays to compare against` |
|       - | 4886 | ` * Return` |
|       - | 4887 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4888 | ` *  in all of the parameters.` |
|       - | 4889 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4890 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4891 | ` */` |
|      22 | 4892 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4893 | `{` |
|       - | 4894 | `	ph7_hashmap_node *pEntry;` |
|       - | 4895 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4896 | `	ph7_value *pArray;` |
|       - | 4897 | `	ph7_value *pVal;` |
|       - | 4898 | `	sxi32 rc;` |
|       - | 4899 | `	sxu32 n;` |
|       - | 4900 | `	int i;` |
|      26 | 4901 | `	if( nArg < 1 ){` |
|       4 | 4902 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4903 | `			"ArgumentCountError",` |
|       - | 4904 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4905 | `			nArg` |
|       - | 4906 | `			);` |
|       - | 4907 | `	}` |
|      23 | 4908 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4909 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4910 | `			"TypeError",` |
|       - | 4911 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4912 | `			ph7_type_name(apArg[0])` |
|       - | 4913 | `			);` |
|       - | 4914 | `	}` |
|      36 | 4915 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4916 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4917 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4918 | `				"TypeError",` |
|       - | 4919 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4920 | `				i + 1,` |
|       2 | 4921 | `				ph7_type_name(apArg[i])` |
|       - | 4922 | `				);` |
|       - | 4923 | `		}` |
|       9 | 4924 | `	}` |
|      17 | 4925 | `	if( nArg == 1 ){` |
|       - | 4926 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4927 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4928 | `		return PH7_OK;` |
|       - | 4929 | `	}` |
|       - | 4930 | `	/* Create a new array */` |
|      15 | 4931 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4932 | `	if( pArray == 0 ){` |
|     ! 0 | 4933 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4934 | `		return PH7_OK;` |
|       - | 4935 | `	}` |
|       - | 4936 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4937 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4938 | `	/* Perform the intersection */` |
|      15 | 4939 | `	pEntry = pSrc->pFirst;` |
|      15 | 4940 | `	n = pSrc->nEntry;` |
|      31 | 4941 | `	for(;;){` |
|      63 | 4942 | `		if( n < 1 ){` |
|      15 | 4943 | `			break;` |
|       - | 4944 | `		}` |
|       - | 4945 | `		/* Extract the node value */` |
|      49 | 4946 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4947 | `		if( pVal ){` |
|      79 | 4948 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4949 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4950 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4951 | `				/* Perform the lookup */` |
|      55 | 4952 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4953 | `				if( rc != SXRET_OK ){` |
|       - | 4954 | `					/* Value does not exist */` |
|      25 | 4955 | `					break;` |
|       - | 4956 | `				}` |
|      16 | 4957 | `			}` |
|      49 | 4958 | `			if( i >= nArg ){` |
|       - | 4959 | `				/* Perform the insertion */` |
|      25 | 4960 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4961 | `			}` |
|      24 | 4962 | `		}` |
|       - | 4963 | `		/* Point to the next entry */` |
|      49 | 4964 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4965 | `		n--;` |
|       1 | 4966 | `	}` |
|       - | 4967 | `	/* Return the freshly created array */` |
|      15 | 4968 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4969 | `	return PH7_OK;` |
|      15 | 4970 | `}` |
|       - | 4971 | `/*` |
|       - | 4972 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4973 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4974 | ` * Parameters` |
|       - | 4975 | ` *  $array1` |
|       - | 4976 | ` *    The array to compare from` |
|       - | 4977 | ` *  $array2` |
|       - | 4978 | ` *    An array to compare against` |
|       - | 4979 | ` *  $...` |
|       - | 4980 | ` *   More arrays to compare against` |
|       - | 4981 | ` * Return` |
|       - | 4982 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4983 | ` *  in all the arguments, with matching keys.` |
|       - | 4984 | ` */` |
|      22 | 4985 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4986 | `{` |
|       - | 4987 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4988 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4989 | `	ph7_value *pArray;` |
|       - | 4990 | `	ph7_value *pVal;` |
|       - | 4991 | `	sxi32 rc;` |
|       - | 4992 | `	sxu32 n;` |
|       - | 4993 | `	int i;` |
|      26 | 4994 | `	if( nArg < 1 ){` |
|       4 | 4995 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4996 | `			"ArgumentCountError",` |
|       - | 4997 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4998 | `			nArg` |
|       - | 4999 | `			);` |
|       - | 5000 | `	}` |
|      23 | 5001 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5002 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5003 | `			"TypeError",` |
|       - | 5004 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5005 | `			ph7_type_name(apArg[0])` |
|       - | 5006 | `			);` |
|       - | 5007 | `	}` |
|      36 | 5008 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5009 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5010 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5011 | `				"TypeError",` |
|       - | 5012 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 5013 | `				i + 1,` |
|       2 | 5014 | `				ph7_type_name(apArg[i])` |
|       - | 5015 | `				);` |
|       - | 5016 | `		}` |
|       9 | 5017 | `	}` |
|      17 | 5018 | `	if( nArg == 1 ){` |
|       - | 5019 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5020 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5021 | `		return PH7_OK;` |
|       - | 5022 | `	}` |
|       - | 5023 | `	/* Create a new array */` |
|      15 | 5024 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5025 | `	if( pArray == 0 ){` |
|     ! 0 | 5026 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5027 | `		return PH7_OK;` |
|       - | 5028 | `	}` |
|       - | 5029 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 5030 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5031 | `	/* Perform the intersection */` |
|      15 | 5032 | `	pEntry = pSrc->pFirst;` |
|      15 | 5033 | `	n = pSrc->nEntry;` |
|      15 | 5034 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 5035 | `	for(;;){` |
|      47 | 5036 | `		if( n < 1 ){` |
|      15 | 5037 | `			break;` |
|       - | 5038 | `		}` |
|       - | 5039 | `		/* Extract the node value */` |
|      33 | 5040 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 5041 | `		if( pVal ){` |
|      53 | 5042 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 5043 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 5044 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5045 | `				/* Perform a key lookup first */` |
|      37 | 5046 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 5047 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 5048 | `				}else{` |
|      23 | 5049 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 5050 | `				}` |
|      37 | 5051 | `				if( rc != SXRET_OK ){` |
|       - | 5052 | `					/* No such key,break immediately */` |
|       7 | 5053 | `					break;` |
|       - | 5054 | `				}` |
|       - | 5055 | `				/* Perform the lookup */` |
|      31 | 5056 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 5057 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 5058 | `					/* Value does not exist */` |
|       6 | 5059 | `					break;` |
|       - | 5060 | `				}` |
|      11 | 5061 | `			}` |
|      33 | 5062 | `			if( i >= nArg ){` |
|       - | 5063 | `				/* Perform the insertion */` |
|      17 | 5064 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5065 | `			}` |
|      16 | 5066 | `		}` |
|       - | 5067 | `		/* Point to the next entry */` |
|      33 | 5068 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5069 | `		n--;` |
|       1 | 5070 | `	}` |
|       - | 5071 | `	/* Return the freshly created array */` |
|      15 | 5072 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5073 | `	return PH7_OK;` |
|      15 | 5074 | `}` |
|       - | 5075 | `/*` |
|       - | 5076 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 5077 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 5078 | ` * Parameters` |
|       - | 5079 | ` *  $array1` |
|       - | 5080 | ` *    The array to compare from` |
|       - | 5081 | ` *  $...` |
|       - | 5082 | ` *   More arrays to compare against` |
|       - | 5083 | ` * Return` |
|       - | 5084 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 5085 | ` *  have keys that are present in all arguments.` |
|       - | 5086 | ` * Note that NULL is returned on failure.` |
|       - | 5087 | ` */` |
|      22 | 5088 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5089 | `{` |
|       - | 5090 | `	ph7_hashmap_node *pEntry;` |
|       - | 5091 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5092 | `	ph7_value *pArray;` |
|       - | 5093 | `	sxi32 rc;` |
|       - | 5094 | `	sxu32 n;` |
|       - | 5095 | `	int i;` |
|      26 | 5096 | `	if( nArg < 1 ){` |
|       4 | 5097 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5098 | `			"ArgumentCountError",` |
|       - | 5099 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 5100 | `			nArg` |
|       - | 5101 | `			);` |
|       - | 5102 | `	}` |
|      23 | 5103 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5104 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5105 | `			"TypeError",` |
|       - | 5106 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5107 | `			ph7_type_name(apArg[0])` |
|       - | 5108 | `			);` |
|       - | 5109 | `	}` |
|      36 | 5110 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5111 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5112 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5113 | `				"TypeError",` |
|       - | 5114 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5115 | `				i + 1,` |
|       2 | 5116 | `				ph7_type_name(apArg[i])` |
|       - | 5117 | `				);` |
|       - | 5118 | `		}` |
|       9 | 5119 | `	}` |
|      17 | 5120 | `	if( nArg == 1 ){` |
|       - | 5121 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5122 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5123 | `		return PH7_OK;` |
|       - | 5124 | `	}` |
|       - | 5125 | `	/* Create a new array */` |
|      15 | 5126 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5127 | `	if( pArray == 0 ){` |
|     ! 0 | 5128 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5129 | `		return PH7_OK;` |
|       - | 5130 | `	}` |
|       - | 5131 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 5132 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5133 | `	/* Perform the intersection */` |
|      15 | 5134 | `	pEntry = pSrc->pFirst;` |
|      15 | 5135 | `	n = pSrc->nEntry;` |
|      24 | 5136 | `	for(;;){` |
|      49 | 5137 | `		if( n < 1 ){` |
|      15 | 5138 | `			break;` |
|       - | 5139 | `		}` |
|      57 | 5140 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 5141 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 5142 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 5143 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5144 | `				/* Blob lookup */` |
|      27 | 5145 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 5146 | `			}else{` |
|       - | 5147 | `				/* Int key */` |
|      13 | 5148 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5149 | `			}` |
|      39 | 5150 | `			if( rc != SXRET_OK ){` |
|       - | 5151 | `				/* Key does not exist, break immediately */` |
|      17 | 5152 | `				break;` |
|       - | 5153 | `			}` |
|      12 | 5154 | `		}` |
|      35 | 5155 | `		if( i >= nArg ){` |
|       - | 5156 | `			/* Perform the insertion */` |
|      19 | 5157 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 5158 | `		}` |
|       - | 5159 | `		/* Point to the next entry */` |
|      35 | 5160 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 5161 | `		n--;` |
|       1 | 5162 | `	}` |
|       - | 5163 | `	/* Return the freshly created array */` |
|      15 | 5164 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5165 | `	return PH7_OK;` |
|      15 | 5166 | `}` |
|       - | 5167 | `/*` |
|       - | 5168 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 5169 | ` *  Computes the intersection of arrays.` |
|       - | 5170 | ` * Parameters` |
|       - | 5171 | ` *  $array1` |
|       - | 5172 | ` *    The array to compare from` |
|       - | 5173 | ` *  $array2` |
|       - | 5174 | ` *    An array to compare against` |
|       - | 5175 | ` *  $...` |
|       - | 5176 | ` *   More arrays to compare against` |
|       - | 5177 | ` * $callback` |
|       - | 5178 | ` *  The callback comparison function.` |
|       - | 5179 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5180 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5181 | ` *  than the second.` |
|       - | 5182 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5183 | ` * Return` |
|       - | 5184 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5185 | ` *  in all of the parameters. .` |
|       - | 5186 | ` * Note that NULL is returned on failure.` |
|       - | 5187 | ` */` |
|      26 | 5188 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5189 | `{` |
|       - | 5190 | `	ph7_hashmap_node *pEntry;` |
|       - | 5191 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5192 | `	ph7_value *pCallback;` |
|       - | 5193 | `	ph7_value *pArray;` |
|       - | 5194 | `	ph7_value *pVal;` |
|       - | 5195 | `	sxi32 rc;` |
|       - | 5196 | `	sxu32 n;` |
|       - | 5197 | `	int i;` |
|       - | 5198 |  |
|       - | 5199 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      31 | 5200 | `	if( nArg < 2 ){` |
|       4 | 5201 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5202 | `			"ArgumentCountError",` |
|       - | 5203 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5204 | `			nArg` |
|       - | 5205 | `			);` |
|       - | 5206 | `	}` |
|      29 | 5207 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5208 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5209 | `			"TypeError",` |
|       - | 5210 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5211 | `			ph7_type_name(apArg[0])` |
|       - | 5212 | `			);` |
|       - | 5213 | `	}` |
|       - | 5214 |  |
|      27 | 5215 | `	if( nArg == 2 ){` |
|       - | 5216 | `		/* Only the original array and the callback were provided. */` |
|       - | 5217 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5218 | `		 * validation ordering. */` |
|       3 | 5219 | `	} else {` |
|       - | 5220 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      39 | 5221 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      23 | 5222 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5223 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5224 | `					"TypeError",` |
|       - | 5225 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5226 | `					i + 1,` |
|       2 | 5227 | `					ph7_type_name(apArg[i])` |
|       - | 5228 | `					);` |
|       - | 5229 | `			}` |
|      13 | 5230 | `		}` |
|       - | 5231 | `	}` |
|       - | 5232 |  |
|       - | 5233 | `	/* Identify the callback (always expected as the last argument). */` |
|      25 | 5234 | `	pCallback = apArg[nArg - 1];` |
|       - | 5235 | `	/* Validate the callback to match PHP's error messages. */` |
|      25 | 5236 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      14 | 5237 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5238 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5239 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5240 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5241 | `			 */` |
|       9 | 5242 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       9 | 5243 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5244 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5245 | `					"TypeError",` |
|       - | 5246 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5247 | `					nArg` |
|       - | 5248 | `					);` |
|       - | 5249 | `			}` |
|       - | 5250 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5251 | `			{` |
|       6 | 5252 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       6 | 5253 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       6 | 5254 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5255 | `					int nMethodLen;` |
|       6 | 5256 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       6 | 5257 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       6 | 5258 | `					if( pClass ){` |
|       - | 5259 | `						/* Class exists but method is missing. */` |
|       4 | 5260 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5261 | `							"TypeError",` |
|       - | 5262 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5263 | `							nArg,` |
|       1 | 5264 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5265 | `							zMethod` |
|       - | 5266 | `							);` |
|       - | 5267 | `					}` |
|       - | 5268 | `					/* Class not found */` |
|       - | 5269 | `					{` |
|       - | 5270 | `						int nName;` |
|       3 | 5271 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5272 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5273 | `							"TypeError",` |
|       - | 5274 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5275 | `							nArg,` |
|       1 | 5276 | `							zName` |
|       - | 5277 | `							);` |
|       - | 5278 | `					}` |
|       - | 5279 | `				}` |
|       - | 5280 | `			}` |
|       - | 5281 | `			/* Fallback message */` |
|     ! 0 | 5282 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5283 | `				"TypeError",` |
|       - | 5284 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5285 | `				nArg` |
|       - | 5286 | `				);` |
|       - | 5287 | `		}` |
|       6 | 5288 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5289 | `			int len;` |
|       3 | 5290 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5291 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5292 | `				"TypeError",` |
|       - | 5293 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5294 | `				nArg,` |
|       1 | 5295 | `				zName` |
|       - | 5296 | `				);` |
|       - | 5297 | `		}` |
|       4 | 5298 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5299 | `			"TypeError",` |
|       - | 5300 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5301 | `			nArg` |
|       - | 5302 | `			);` |
|       - | 5303 | `	}` |
|       - | 5304 |  |
|      11 | 5305 | `	if( nArg == 2 ){` |
|       - | 5306 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5307 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5308 | `		return PH7_OK;` |
|       - | 5309 | `	}` |
|       - | 5310 |  |
|       - | 5311 | `	/* Create a new array */` |
|       7 | 5312 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5313 | `	if( pArray == 0 ){` |
|     ! 0 | 5314 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5315 | `		return PH7_OK;` |
|       - | 5316 | `	}` |
|       - | 5317 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 5318 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5319 | `	/* Perform the intersection */` |
|       7 | 5320 | `	pEntry = pSrc->pFirst;` |
|       7 | 5321 | `	n = pSrc->nEntry;` |
|       7 | 5322 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 5323 | `	for(;;){` |
|      19 | 5324 | `		if( n < 1 ){` |
|       5 | 5325 | `			break;` |
|       - | 5326 | `		}` |
|       - | 5327 | `		/* Extract the node value */` |
|      15 | 5328 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5329 | `		if( pVal ){` |
|      23 | 5330 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 5331 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5332 | `					/* ignore */` |
|     ! 0 | 5333 | `					continue;` |
|       - | 5334 | `				}` |
|       - | 5335 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 5336 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5337 | `				/* Perform the lookup */` |
|      15 | 5338 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 5339 | `				if( rc != SXRET_OK ){` |
|       - | 5340 | `					/* Value does not exist */` |
|       7 | 5341 | `					break;` |
|       - | 5342 | `				}` |
|       5 | 5343 | `			}` |
|      15 | 5344 | `			if( i >= (nArg-1) ){` |
|       - | 5345 | `				/* Perform the insertion */` |
|       9 | 5346 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5347 | `			}` |
|       7 | 5348 | `		}` |
|      15 | 5349 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 5350 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5351 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 5352 | `			return PH7_EXCEPTION;` |
|       - | 5353 | `		}` |
|       - | 5354 | `		/* Point to the next entry */` |
|      13 | 5355 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5356 | `		n--;` |
|       1 | 5357 | `	}` |
|       - | 5358 | `	/* Return the freshly created array */` |
|       5 | 5359 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5360 | `	return PH7_OK;` |
|      18 | 5361 | `}` |
|       - | 5362 | `/*` |
|       - | 5363 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5364 | ` *  Fill an array with values.` |
|       - | 5365 | ` * Parameters` |
|       - | 5366 | ` *  $start_index` |
|       - | 5367 | ` *    The first index of the returned array.` |
|       - | 5368 | ` *  $num` |
|       - | 5369 | ` *   Number of elements to insert.` |
|       - | 5370 | ` *  $value` |
|       - | 5371 | ` *    Value to use for filling.` |
|       - | 5372 | ` * Return` |
|       - | 5373 | ` *  The filled array or null on failure.` |
|       - | 5374 | ` */` |
|     238 | 5375 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5376 | `{` |
|       - | 5377 | `	ph7_value *pArray;` |
|       - | 5378 | `	int i,nEntry;` |
|       - | 5379 |  |
|       - | 5380 | `	/* PHP enforces argument count and type checks. */` |
|     243 | 5381 | `	if( nArg != 3 ){` |
|       - | 5382 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       8 | 5383 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5384 | `			"ArgumentCountError",` |
|       - | 5385 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5386 | `			nArg` |
|       - | 5387 | `			);` |
|       - | 5388 | `	}` |
|       - | 5389 |  |
|       - | 5390 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5391 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5392 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5393 | `	 * and NULLs are rejected outright. */` |
|     350 | 5394 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     353 | 5395 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5396 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5397 | `			"TypeError",` |
|       - | 5398 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5399 | `			ph7_type_name(apArg[0])` |
|       - | 5400 | `			);` |
|       - | 5401 | `	}` |
|     236 | 5402 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5403 | `		int len;` |
|       8 | 5404 | `		sxu8 bReal = FALSE;` |
|       8 | 5405 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5406 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5407 | `			/* Non‑numeric string is an error. */` |
|       3 | 5408 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5409 | `				"TypeError",` |
|       - | 5410 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5411 | `				);` |
|       - | 5412 | `		}` |
|       5 | 5413 | `		if( bReal ){` |
|       - | 5414 | `			/* float-string -> deprecation warning */` |
|       4 | 5415 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5416 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5417 | `				zStr` |
|       - | 5418 | `				);` |
|       1 | 5419 | `		}` |
|       2 | 5420 | `	}` |
|       - | 5421 |  |
|       - | 5422 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5423 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     345 | 5424 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     348 | 5425 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5426 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5427 | `			"TypeError",` |
|       - | 5428 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5429 | `			ph7_type_name(apArg[1])` |
|       - | 5430 | `			);` |
|       - | 5431 | `	}` |
|     233 | 5432 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5433 | `		int len;` |
|       3 | 5434 | `		sxu8 bReal = FALSE;` |
|       3 | 5435 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5436 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5437 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5438 | `				"TypeError",` |
|       - | 5439 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5440 | `				);` |
|       - | 5441 | `		}` |
|     ! 0 | 5442 | `	}` |
|       - | 5443 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5444 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5445 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5446 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5447 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5448 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5449 | `		if( d != (double)i64 ){` |
|       7 | 5450 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5451 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5452 | `				d` |
|       - | 5453 | `				);` |
|       2 | 5454 | `		}` |
|       2 | 5455 | `	}` |
|       - | 5456 |  |
|       - | 5457 | `	/* Total number of entries to insert */` |
|     230 | 5458 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5459 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5460 | `	if( nEntry < 0 ){` |
|       3 | 5461 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5462 | `			"ValueError",` |
|       - | 5463 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5464 | `			);` |
|       - | 5465 | `	}` |
|       - | 5466 |  |
|       - | 5467 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5468 | `	if( nEntry == 0 ){` |
|       7 | 5469 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5470 | `		return PH7_OK;` |
|       - | 5471 | `	}` |
|       - | 5472 |  |
|       - | 5473 | `	/* Create a new array */` |
|     221 | 5474 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5475 | `	if( pArray == 0 ){` |
|     ! 0 | 5476 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5477 | `	}` |
|       - | 5478 |  |
|       - | 5479 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5480 | `	if( ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]) != SXRET_OK ){` |
|     ! 0 | 5481 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5482 | `	}` |
|       - | 5483 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5484 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5485 | `		if( ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]) != SXRET_OK ){` |
|       - | 5486 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 5487 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 5488 | `		}` |
| 1058682 | 5489 | `	}` |
|       - | 5490 | `	/* Return the filled array */` |
|     221 | 5491 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5492 | `	return PH7_OK;` |
|     124 | 5493 | `}` |
|       - | 5494 | `/*` |
|       - | 5495 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5496 | ` *  Fill an array with values, specifying keys.` |
|       - | 5497 | ` * Parameters` |
|       - | 5498 | ` *  $input` |
|       - | 5499 | ` *   Array of values that will be used as key.` |
|       - | 5500 | ` *  $value` |
|       - | 5501 | ` *    Value to use for filling.` |
|       - | 5502 | ` * Return` |
|       - | 5503 | ` *  The filled array.` |
|       - | 5504 | ` * Throws` |
|       - | 5505 | ` *  ValueError if $input is not an array.` |
|       - | 5506 | ` */` |
|      26 | 5507 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5508 | `{` |
|       - | 5509 | `	ph7_hashmap_node *pEntry;` |
|       - | 5510 | `	ph7_hashmap *pSrc;` |
|       - | 5511 | `	ph7_value *pArray;` |
|       - | 5512 | `	sxu32 n;` |
|       - | 5513 | `	/* PHP enforces exactly 2 arguments. */` |
|      31 | 5514 | `	if( nArg != 2 ){` |
|      12 | 5515 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5516 | `			"ArgumentCountError",` |
|       - | 5517 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5518 | `			nArg` |
|       - | 5519 | `			);` |
|       - | 5520 | `	}` |
|       - | 5521 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 5522 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 5523 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5524 | `			"TypeError",` |
|       - | 5525 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5526 | `			ph7_type_name(apArg[0])` |
|       - | 5527 | `			);` |
|       - | 5528 | `	}` |
|       - | 5529 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5530 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5531 | `	/* Create a new array */` |
|      17 | 5532 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5533 | `	if( pArray == 0 ){` |
|     ! 0 | 5534 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5535 | `		return PH7_OK;` |
|       - | 5536 | `	}` |
|       - | 5537 | `	/* Perform the requested operation */` |
|      17 | 5538 | `	pEntry = pSrc->pFirst;` |
|      45 | 5539 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5540 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5541 | `		/* Point to the next entry */` |
|      29 | 5542 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5543 | `	}` |
|       - | 5544 | `	/* Return the filled array */` |
|      17 | 5545 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5546 | `	return PH7_OK;` |
|      18 | 5547 | `}` |
|       - | 5548 | `/*` |
|       - | 5549 | ` * array array_combine(array $keys,array $values)` |
|       - | 5550 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5551 | ` * Parameters` |
|       - | 5552 | ` *  $keys` |
|       - | 5553 | ` *    Array of keys to be used.` |
|       - | 5554 | ` * $values` |
|       - | 5555 | ` *   Array of values to be used.` |
|       - | 5556 | ` * Return` |
|       - | 5557 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5558 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5559 | ` *  not an array.` |
|       - | 5560 | ` */` |
|      18 | 5561 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5562 | `{` |
|       - | 5563 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5564 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5565 | `	ph7_value *pArray;` |
|       - | 5566 | `	sxu32 n;` |
|       - | 5567 | `	/* PHP enforces argument count and type checks. */` |
|      23 | 5568 | `	if( nArg != 2 ){` |
|       - | 5569 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5570 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5571 | `			"ArgumentCountError",` |
|       - | 5572 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5573 | `			nArg` |
|       - | 5574 | `			);` |
|       - | 5575 | `	}` |
|       - | 5576 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5577 | `	 * argument index in the error message. */` |
|      20 | 5578 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5579 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5580 | `			"TypeError",` |
|       - | 5581 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5582 | `			ph7_type_name(apArg[0])` |
|       - | 5583 | `			);` |
|       - | 5584 | `	}` |
|      17 | 5585 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5586 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5587 | `			"TypeError",` |
|       - | 5588 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5589 | `			ph7_type_name(apArg[1])` |
|       - | 5590 | `			);` |
|       - | 5591 | `	}` |
|       - | 5592 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5593 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5594 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5595 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5596 | `		/* Length mismatch -> ValueError */` |
|       3 | 5597 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5598 | `			"ValueError",` |
|       - | 5599 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5600 | `			);` |
|       - | 5601 | `	}` |
|       - | 5602 | `	/* Create a new array */` |
|      11 | 5603 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5604 | `	if( pArray == 0 ){` |
|     ! 0 | 5605 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5606 | `		return PH7_OK;` |
|       - | 5607 | `	}` |
|       - | 5608 | `	/* Perform the requested operation */` |
|      11 | 5609 | `	pKe = pKey->pFirst;` |
|      11 | 5610 | `	pVe = pValue->pFirst;` |
|      33 | 5611 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5612 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5613 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5614 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5615 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5616 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5617 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5618 | `		 * original array must not be mutated. */` |
|      23 | 5619 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5620 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5621 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5622 | `			if( pTmpKey ){` |
|       5 | 5623 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5624 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5625 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5626 | `				pKeyCopy = pTmpKey;` |
|       2 | 5627 | `			}` |
|       2 | 5628 | `		}` |
|      23 | 5629 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5630 | `		/* Point to the next entry */` |
|      23 | 5631 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5632 | `		pVe = pVe->pPrev;` |
|      12 | 5633 | `	}` |
|       - | 5634 | `	/* Return the filled array */` |
|      11 | 5635 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5636 | `	return PH7_OK;` |
|      14 | 5637 | `}` |
|       - | 5638 | `/*` |
|       - | 5639 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5640 | ` *  Return an array with elements in reverse order.` |
|       - | 5641 | ` * Parameters` |
|       - | 5642 | ` *  $array` |
|       - | 5643 | ` *   The input array.` |
|       - | 5644 | ` *  $preserve_keys (optional)` |
|       - | 5645 | ` *   If set to TRUE keys are preserved.` |
|       - | 5646 | ` * Return` |
|       - | 5647 | ` *  The reversed array.` |
|       - | 5648 | ` */` |
|      20 | 5649 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 5650 | `{` |
|       - | 5651 | `	ph7_hashmap_node *pEntry;` |
|       - | 5652 | `	ph7_hashmap *pSrc;` |
|       - | 5653 | `	ph7_value *pArray;` |
|       - | 5654 | `	int bPreserve;` |
|       - | 5655 | `	sxu32 n;` |
|      23 | 5656 | `	if( nArg < 1 ){` |
|       4 | 5657 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5658 | `			"ArgumentCountError",` |
|       - | 5659 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5660 | `			nArg` |
|       - | 5661 | `			);` |
|       - | 5662 | `	}` |
|       - | 5663 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5664 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5665 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5666 | `			"TypeError",` |
|       - | 5667 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5668 | `			ph7_type_name(apArg[0])` |
|       - | 5669 | `			);` |
|       - | 5670 | `	}` |
|      17 | 5671 | `	bPreserve = FALSE;` |
|      17 | 5672 | `	if( nArg > 1 ){` |
|       7 | 5673 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5674 | `	}` |
|       - | 5675 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5676 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5677 | `	/* Create a new array */` |
|      17 | 5678 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5679 | `	if( pArray == 0 ){` |
|     ! 0 | 5680 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5681 | `		return PH7_OK;` |
|       - | 5682 | `	}` |
|       - | 5683 | `	/* Perform the requested operation */` |
|      17 | 5684 | `	pEntry = pSrc->pLast;` |
|      55 | 5685 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5686 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5687 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5688 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5689 | `		/* Point to the previous entry */` |
|      39 | 5690 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5691 | `	}` |
|      17 | 5692 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5693 | `	return PH7_OK;` |
|      13 | 5694 | `}` |
|       - | 5695 | `/*` |
|       - | 5696 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5697 | ` *  Removes duplicate values from an array.` |
|       - | 5698 | ` * Parameters` |
|       - | 5699 | ` *  $array` |
|       - | 5700 | ` *   The input array.` |
|       - | 5701 | ` *  $flags` |
|       - | 5702 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5703 | ` *   behavior using these values:` |
|       - | 5704 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5705 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5706 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5707 | ` * Return` |
|       - | 5708 | ` *  The filtered array.` |
|       - | 5709 | ` */` |
|      24 | 5710 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5711 | `{` |
|       - | 5712 | `	ph7_hashmap_node *pEntry;` |
|       - | 5713 | `	ph7_value *pNeedle;` |
|       - | 5714 | `	ph7_hashmap *pSrc;` |
|       - | 5715 | `	ph7_value *pArray;` |
|       - | 5716 | `	int bStrict;` |
|       - | 5717 | `	sxi32 rc;` |
|       - | 5718 | `	sxu32 n;` |
|      28 | 5719 | `	if( nArg < 1 ){` |
|       - | 5720 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5721 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5722 | `			"ArgumentCountError",` |
|       - | 5723 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5724 | `			);` |
|       - | 5725 | `	}` |
|      25 | 5726 | `	if( nArg > 2 ){` |
|       - | 5727 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5728 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5729 | `			"ArgumentCountError",` |
|       - | 5730 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5731 | `			nArg` |
|       - | 5732 | `			);` |
|       - | 5733 | `	}` |
|       - | 5734 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5735 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5736 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5737 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5738 | `			"TypeError",` |
|       - | 5739 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5740 | `			ph7_type_name(apArg[0])` |
|       - | 5741 | `			);` |
|       - | 5742 | `	}` |
|      19 | 5743 | `	bStrict = FALSE;` |
|       - | 5744 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5745 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5746 | `	/* Create a new array */` |
|      19 | 5747 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5748 | `	if( pArray == 0 ){` |
|     ! 0 | 5749 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5750 | `		return PH7_OK;` |
|       - | 5751 | `	}` |
|       - | 5752 | `	/* Perform the requested operation */` |
|      19 | 5753 | `	pEntry = pSrc->pFirst;` |
|      83 | 5754 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5755 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5756 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5757 | `		if( pNeedle ){` |
|      65 | 5758 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5759 | `		}` |
|      65 | 5760 | `		if( rc != SXRET_OK ){` |
|       - | 5761 | `			/* Perform the insertion */` |
|      37 | 5762 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5763 | `		}` |
|       - | 5764 | `		/* Point to the next entry */` |
|      65 | 5765 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5766 | `	}` |
|       - | 5767 | `	/* Return the freshly created array */` |
|      19 | 5768 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5769 | `	return PH7_OK;` |
|      16 | 5770 | `}` |
|       - | 5771 | `/*` |
|       - | 5772 | ` * array array_flip(array $input)` |
|       - | 5773 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5774 | ` * Parameter` |
|       - | 5775 | ` *  $input` |
|       - | 5776 | ` *   Input array.` |
|       - | 5777 | ` * Return` |
|       - | 5778 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5779 | ` */` |
|      34 | 5780 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5781 | `{` |
|       - | 5782 | `	ph7_hashmap_node *pEntry;` |
|       - | 5783 | `	ph7_hashmap *pSrc;` |
|       - | 5784 | `	ph7_value *pArray;` |
|       - | 5785 | `	ph7_value *pKey;` |
|       - | 5786 | `	ph7_value sVal;` |
|       - | 5787 | `	sxu32 n;` |
|       - | 5788 |  |
|       - | 5789 | `	/* PHP requires exactly one argument */` |
|      39 | 5790 | `	if( nArg != 1 ){` |
|       - | 5791 | `		/* Use ArgumentCountError like other array helpers */` |
|       8 | 5792 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5793 | `			"ArgumentCountError",` |
|       - | 5794 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5795 | `			nArg` |
|       - | 5796 | `			);` |
|       - | 5797 | `	}` |
|       - | 5798 | `	/* Make sure we are dealing with a valid hashmap */` |
|      33 | 5799 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5800 | `		/* Type mismatch -> TypeError */` |
|       8 | 5801 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5802 | `			"TypeError",` |
|       - | 5803 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5804 | `			ph7_type_name(apArg[0])` |
|       - | 5805 | `			);` |
|       - | 5806 | `	}` |
|       - | 5807 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5808 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5809 | `	/* Create a new array */` |
|      27 | 5810 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5811 | `	if( pArray == 0 ){` |
|     ! 0 | 5812 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5813 | `		return PH7_OK;` |
|       - | 5814 | `	}` |
|       - | 5815 | `	/* Start processing */` |
|      27 | 5816 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5817 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5818 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5819 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5820 | `		if( pKey ){` |
|       - | 5821 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5822 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5823 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5824 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5825 | `					);` |
|   22236 | 5826 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5827 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5828 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5829 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5830 | `				}else{` |
|       - | 5831 | `					SyString sStr;` |
|    2227 | 5832 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5833 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5834 | `				}` |
|       - | 5835 | `				/* Perform the insertion */` |
|   22227 | 5836 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5837 | `				/* Safely release the value because each inserted entry` |
|       - | 5838 | `				 * has its own private copy of the value.` |
|       - | 5839 | `				 */` |
|   22227 | 5840 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5841 | `			}else{` |
|       - | 5842 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5843 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5844 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5845 | `					);` |
|       - | 5846 | `			}` |
|   11118 | 5847 | `		}` |
|       - | 5848 | `		/* Point to the next entry */` |
|   22237 | 5849 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5850 | `	}` |
|       - | 5851 | `	/* Return the freshly created array */` |
|      27 | 5852 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5853 | `	return PH7_OK;` |
|      22 | 5854 | `}` |
|       - | 5855 | `/*` |
|       - | 5856 | ` * number array_sum(array $array )` |
|       - | 5857 | ` *  Calculate the sum of values in an array.` |
|       - | 5858 | ` * Parameters` |
|       - | 5859 | ` *  $array: The input array.` |
|       - | 5860 | ` * Return` |
|       - | 5861 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5862 | ` */` |
|      24 | 5863 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5864 | `{` |
|       - | 5865 | `	ph7_hashmap_node *pEntry;` |
|       - | 5866 | `	ph7_value *pObj;` |
|      25 | 5867 | `	double dSum = 0;` |
|       - | 5868 | `	sxu32 n;` |
|      25 | 5869 | `	pEntry = pMap->pFirst;` |
|      91 | 5870 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5871 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5872 | `		if( pObj ){` |
|      67 | 5873 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5874 | `				dSum += pObj->rVal;` |
|      53 | 5875 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5876 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5877 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5878 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5879 | `					double dv = 0;` |
|      13 | 5880 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5881 | `					dSum += dv;` |
|       7 | 5882 | `				}` |
|      12 | 5883 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5884 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5885 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5886 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5887 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5888 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5889 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5890 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5891 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5892 | `			}` |
|       - | 5893 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5894 | `		}` |
|       - | 5895 | `		/* Point to the next entry */` |
|      67 | 5896 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5897 | `	}` |
|       - | 5898 | `	/* Return sum */` |
|      25 | 5899 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5900 | `}` |
|      30 | 5901 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5902 | `{` |
|       - | 5903 | `	ph7_hashmap_node *pEntry;` |
|       - | 5904 | `	ph7_value *pObj;` |
|      32 | 5905 | `	sxi64 nSum = 0;` |
|       - | 5906 | `	sxu32 n;` |
|      32 | 5907 | `	pEntry = pMap->pFirst;` |
|     128 | 5908 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      98 | 5909 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      98 | 5910 | `		if( pObj ){` |
|      98 | 5911 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      88 | 5912 | `				nSum += pObj->x.iVal;` |
|      54 | 5913 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5914 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5915 | `					sxi64 nv = 0;` |
|       5 | 5916 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5917 | `					nSum += nv;` |
|       3 | 5918 | `				}` |
|       8 | 5919 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5920 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5921 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5922 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5923 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5924 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5925 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5926 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5927 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5928 | `			}` |
|       - | 5929 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      48 | 5930 | `		}` |
|       - | 5931 | `		/* Point to the next entry */` |
|      98 | 5932 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      50 | 5933 | `	}` |
|       - | 5934 | `	/* Return sum */` |
|      32 | 5935 | `	ph7_result_int64(pCtx,nSum);` |
|      32 | 5936 | `}` |
|       - | 5937 | `/* number array_sum(array $array )` |
|       - | 5938 | ` * (See block-coment above)` |
|       - | 5939 | ` */` |
|      68 | 5940 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5941 | `{` |
|       - | 5942 | `	ph7_hashmap_node *pEntry;` |
|       - | 5943 | `	ph7_hashmap *pMap;` |
|       - | 5944 | `	ph7_value *pObj;` |
|      73 | 5945 | `	int useDouble = 0;` |
|       - | 5946 | `	sxu32 n;` |
|       - | 5947 | `	/* PHP requires exactly one argument */` |
|      73 | 5948 | `	if( nArg != 1 ){` |
|       8 | 5949 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5950 | `			"ArgumentCountError",` |
|       - | 5951 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5952 | `			nArg` |
|       - | 5953 | `			);` |
|       - | 5954 | `	}` |
|       - | 5955 | `	/* Make sure we are dealing with a valid hashmap */` |
|      68 | 5956 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5957 | `		/* Type mismatch -> TypeError */` |
|       8 | 5958 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5959 | `			"TypeError",` |
|       - | 5960 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5961 | `			ph7_type_name(apArg[0])` |
|       - | 5962 | `			);` |
|       - | 5963 | `	}` |
|      62 | 5964 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      62 | 5965 | `	if( pMap->nEntry < 1 ){` |
|       - | 5966 | `		/* Nothing to compute,return 0 */` |
|       7 | 5967 | `		ph7_result_int(pCtx,0);` |
|       7 | 5968 | `		return PH7_OK;` |
|       - | 5969 | `	}` |
|       - | 5970 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5971 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5972 | `	 */` |
|      56 | 5973 | `	pEntry = pMap->pFirst;` |
|     160 | 5974 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     130 | 5975 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     130 | 5976 | `		if( pObj ){` |
|     130 | 5977 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5978 | `				useDouble = 1;` |
|      19 | 5979 | `				break;` |
|       - | 5980 | `			}` |
|     112 | 5981 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5982 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5983 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5984 | `				sxu32 i;` |
|      23 | 5985 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5986 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5987 | `						useDouble = 1;` |
|       7 | 5988 | `						break;` |
|       - | 5989 | `					}` |
|       6 | 5990 | `				}` |
|      13 | 5991 | `				if( useDouble ){` |
|       7 | 5992 | `					break;` |
|       - | 5993 | `				}` |
|       3 | 5994 | `			}` |
|      52 | 5995 | `		}` |
|     106 | 5996 | `		pEntry = pEntry->pPrev;` |
|      54 | 5997 | `	}` |
|      56 | 5998 | `	if( useDouble ){` |
|      25 | 5999 | `		DoubleSum(pCtx,pMap);` |
|      13 | 6000 | `	}else{` |
|      32 | 6001 | `		Int64Sum(pCtx,pMap);` |
|       - | 6002 | `	}` |
|      56 | 6003 | `	return PH7_OK;` |
|      39 | 6004 | `}` |
|       - | 6005 | `/*` |
|       - | 6006 | ` * number array_product(array $array )` |
|       - | 6007 | ` *  Calculate the product of values in an array.` |
|       - | 6008 | ` * Parameters` |
|       - | 6009 | ` *  $array: The input array.` |
|       - | 6010 | ` * Return` |
|       - | 6011 | ` *  Returns the product of values as an integer or float.` |
|       - | 6012 | ` */` |
|     ! 0 | 6013 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 6014 | `{` |
|       - | 6015 | `	ph7_hashmap_node *pEntry;` |
|       - | 6016 | `	ph7_value *pObj;` |
|       - | 6017 | `	double dProd;` |
|       - | 6018 | `	sxu32 n;` |
|     ! 0 | 6019 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6020 | `	dProd = 1;` |
|     ! 0 | 6021 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6022 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6023 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6024 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6025 | `				dProd *= pObj->rVal;` |
|     ! 0 | 6026 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6027 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 6028 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6029 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6030 | `					double dv = 0;` |
|     ! 0 | 6031 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 6032 | `					dProd *= dv;` |
|     ! 0 | 6033 | `				}` |
|     ! 0 | 6034 | `			}` |
|     ! 0 | 6035 | `		}` |
|       - | 6036 | `		/* Point to the next entry */` |
|     ! 0 | 6037 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6038 | `	}` |
|       - | 6039 | `	/* Return product */` |
|     ! 0 | 6040 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 6041 | `}` |
|     ! 0 | 6042 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 6043 | `{` |
|       - | 6044 | `	ph7_hashmap_node *pEntry;` |
|       - | 6045 | `	ph7_value *pObj;` |
|       - | 6046 | `	sxi64 nProd;` |
|       - | 6047 | `	sxu32 n;` |
|     ! 0 | 6048 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6049 | `	nProd = 1;` |
|     ! 0 | 6050 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6051 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6052 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6053 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6054 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 6055 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6056 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 6057 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6058 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6059 | `					sxi64 nv = 0;` |
|     ! 0 | 6060 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 6061 | `					nProd *= nv;` |
|     ! 0 | 6062 | `				}` |
|     ! 0 | 6063 | `			}` |
|     ! 0 | 6064 | `		}` |
|       - | 6065 | `		/* Point to the next entry */` |
|     ! 0 | 6066 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6067 | `	}` |
|       - | 6068 | `	/* Return product */` |
|     ! 0 | 6069 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 6070 | `}` |
|       - | 6071 | `/* number array_product(array $array )` |
|       - | 6072 | ` * (See block-block comment above)` |
|       - | 6073 | ` */` |
|     ! 0 | 6074 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 6075 | `{` |
|       - | 6076 | `	ph7_hashmap *pMap;` |
|       - | 6077 | `	ph7_value *pObj;` |
|     ! 0 | 6078 | `	if( nArg < 1 ){` |
|       - | 6079 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 6080 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6081 | `		return PH7_OK;` |
|       - | 6082 | `	}` |
|       - | 6083 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 6084 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6085 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 6086 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6087 | `		return PH7_OK;` |
|       - | 6088 | `	}` |
|     ! 0 | 6089 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 6090 | `	if( pMap->nEntry < 1 ){` |
|       - | 6091 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 6092 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6093 | `		return PH7_OK;` |
|       - | 6094 | `	}` |
|       - | 6095 | `	/* If the first element is of type float,then perform floating` |
|       - | 6096 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 6097 | `	 */` |
|     ! 0 | 6098 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 6099 | `	if( pObj == 0 ){` |
|     ! 0 | 6100 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6101 | `		return PH7_OK;` |
|       - | 6102 | `	}` |
|     ! 0 | 6103 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6104 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 6105 | `	}else{` |
|     ! 0 | 6106 | `		Int64Prod(pCtx,pMap);` |
|       - | 6107 | `	}` |
|     ! 0 | 6108 | `	return PH7_OK;` |
|     ! 0 | 6109 | `}` |
|       - | 6110 | `/*` |
|       - | 6111 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 6112 | ` *  Pick one or more random entries out of an array.` |
|       - | 6113 | ` * Parameters` |
|       - | 6114 | ` * $input` |
|       - | 6115 | ` *  The input array.` |
|       - | 6116 | ` * $num_req` |
|       - | 6117 | ` *  Specifies how many entries you want to pick.` |
|       - | 6118 | ` * Return` |
|       - | 6119 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 6120 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 6121 | ` *  NULL is returned on failure.` |
|       - | 6122 | ` */` |
|       6 | 6123 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6124 | `{` |
|       - | 6125 | `	ph7_hashmap_node *pNode;` |
|       - | 6126 | `	ph7_hashmap *pMap;` |
|       7 | 6127 | `	int nItem = 1;` |
|       7 | 6128 | `	if( nArg < 1 ){` |
|       - | 6129 | `		/* Missing argument,return NULL */` |
|     ! 0 | 6130 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6131 | `		return PH7_OK;` |
|       - | 6132 | `	}` |
|       - | 6133 | `	/* Make sure we are dealing with an array */` |
|       7 | 6134 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6135 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6136 | `		return PH7_OK;` |
|       - | 6137 | `	}` |
|       - | 6138 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 6139 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 6140 | `	if(pMap->nEntry < 1 ){` |
|       - | 6141 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 6142 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6143 | `		return PH7_OK;` |
|       - | 6144 | `	}` |
|       7 | 6145 | `	if( nArg > 1 ){` |
|       3 | 6146 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 6147 | `	}` |
|       7 | 6148 | `	if( nItem < 2 ){` |
|       - | 6149 | `		sxu32 nEntry;` |
|       - | 6150 | `		/* Select a random number */` |
|       5 | 6151 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 6152 | `		/* Extract the desired entry.` |
|       - | 6153 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 6154 | `		 */` |
|       5 | 6155 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 6156 | `			pNode = pMap->pLast;` |
|       3 | 6157 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 6158 | `			if( nEntry > 1 ){` |
|     ! 0 | 6159 | `				for(;;){` |
|     ! 0 | 6160 | `					if( nEntry == 0 ){` |
|     ! 0 | 6161 | `						break;` |
|       - | 6162 | `					}` |
|       - | 6163 | `					/* Point to the previous entry */` |
|     ! 0 | 6164 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 6165 | `					nEntry--;` |
|     ! 0 | 6166 | `				}` |
|     ! 0 | 6167 | `			}` |
|       2 | 6168 | `		}else{` |
|       3 | 6169 | `			pNode = pMap->pFirst;` |
|       1 | 6170 | `			for(;;){` |
|       4 | 6171 | `				if( nEntry == 0 ){` |
|       3 | 6172 | `					break;` |
|       - | 6173 | `				}` |
|       - | 6174 | `				/* Point to the next entry */` |
|       2 | 6175 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       2 | 6176 | `				nEntry--;` |
|       1 | 6177 | `			}` |
|       - | 6178 | `		}` |
|       5 | 6179 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6180 | `			/* Int key */` |
|       3 | 6181 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6182 | `		}else{` |
|       - | 6183 | `			/* Blob key */` |
|       3 | 6184 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6185 | `		}` |
|       3 | 6186 | `	}else{` |
|       - | 6187 | `		ph7_value sKey,*pArray;` |
|       - | 6188 | `		ph7_hashmap *pDest;` |
|       - | 6189 | `		/* Create a new array */` |
|       3 | 6190 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6191 | `		if( pArray == 0 ){` |
|     ! 0 | 6192 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6193 | `			return PH7_OK;` |
|       - | 6194 | `		}` |
|       - | 6195 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6196 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6197 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6198 | `		/* Copy the first n items */` |
|       3 | 6199 | `		pNode = pMap->pFirst;` |
|       3 | 6200 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6201 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6202 | `		}` |
|       7 | 6203 | `		while( nItem > 0){` |
|       5 | 6204 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6205 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6206 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6207 | `			/* Point to the next entry */` |
|       5 | 6208 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6209 | `			nItem--;` |
|       1 | 6210 | `		}` |
|       - | 6211 | `		/* Shuffle the array */` |
|       3 | 6212 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6213 | `		/* Rehash node */` |
|       3 | 6214 | `		HashmapSortRehash(pDest);` |
|       - | 6215 | `		/* Return the random array */` |
|       3 | 6216 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6217 | `	}` |
|       7 | 6218 | `	return PH7_OK;` |
|       4 | 6219 | `}` |
|       - | 6220 | `/*` |
|       - | 6221 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6222 | ` *  Split an array into chunks.` |
|       - | 6223 | ` * Parameters` |
|       - | 6224 | ` * $input` |
|       - | 6225 | ` *   The array to work on` |
|       - | 6226 | ` * $size` |
|       - | 6227 | ` *   The size of each chunk` |
|       - | 6228 | ` * $preserve_keys` |
|       - | 6229 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6230 | ` *   the chunk numerically.` |
|       - | 6231 | ` * Return` |
|       - | 6232 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6233 | ` *  zero, with each dimension containing size elements.` |
|       - | 6234 | ` */` |
|      42 | 6235 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6236 | `{` |
|       - | 6237 | `	ph7_value *pArray,*pChunk;` |
|       - | 6238 | `	ph7_hashmap_node *pEntry;` |
|       - | 6239 | `	ph7_hashmap *pMap;` |
|       - | 6240 | `	int bPreserve;` |
|       - | 6241 | `	sxu32 nChunk;` |
|       - | 6242 | `	sxu32 nSize;` |
|       - | 6243 | `	sxu32 n;` |
|       - | 6244 | `	/* Argument count and types follow PHP semantics. */` |
|      47 | 6245 | `	if( nArg < 2 ){` |
|       - | 6246 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6247 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6248 | `			"ArgumentCountError",` |
|       - | 6249 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6250 | `			nArg` |
|       - | 6251 | `			);` |
|       - | 6252 | `	}` |
|      45 | 6253 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6254 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6255 | `			"TypeError",` |
|       - | 6256 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6257 | `			ph7_type_name(apArg[0])` |
|       - | 6258 | `			);` |
|       - | 6259 | `	}` |
|       - | 6260 | `	/* Create a new array */` |
|      43 | 6261 | `	pArray = ph7_context_new_array(pCtx);` |
|      43 | 6262 | `	if( pArray == 0 ){` |
|     ! 0 | 6263 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6264 | `		return PH7_OK;` |
|       - | 6265 | `	}` |
|       - | 6266 | `	/* Point to the internal representation of the input hashmap */` |
|      43 | 6267 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6268 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6269 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      57 | 6270 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      81 | 6271 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6272 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6273 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6274 | `			"TypeError",` |
|       - | 6275 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6276 | `			ph7_type_name(apArg[1])` |
|       - | 6277 | `			);` |
|       - | 6278 | `	}` |
|       - | 6279 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6280 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6281 | `	 * precision and PHP emits a deprecation warning. */` |
|      43 | 6282 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6283 | `		int len;` |
|       3 | 6284 | `		sxu8 bReal = FALSE;` |
|       3 | 6285 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6286 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6287 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6288 | `				"TypeError",` |
|       - | 6289 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6290 | `				);` |
|       - | 6291 | `		}` |
|     ! 0 | 6292 | `		if( bReal ){` |
|       - | 6293 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6294 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6295 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6296 | `				zStr` |
|       - | 6297 | `				);` |
|     ! 0 | 6298 | `		}` |
|     ! 0 | 6299 | `	}` |
|       - | 6300 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6301 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6302 | `	 * later via ph7_value_to_int. */` |
|      40 | 6303 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6304 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6305 | `		sxi64 i = (sxi64)d;` |
|       3 | 6306 | `		if( d != (double)i ){` |
|       4 | 6307 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6308 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6309 | `				d` |
|       - | 6310 | `				);` |
|       1 | 6311 | `		}` |
|       1 | 6312 | `	}` |
|       - | 6313 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6314 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6315 | `	{` |
|      40 | 6316 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      40 | 6317 | `		if( nSizeSigned < 1 ){` |
|       - | 6318 | `			/* size <= 0 -> ValueError */` |
|       6 | 6319 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6320 | `				"ValueError",` |
|       - | 6321 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6322 | `				);` |
|       - | 6323 | `		}` |
|      35 | 6324 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6325 | `	}` |
|      35 | 6326 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6327 | `		/* Return the whole array */` |
|       3 | 6328 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6329 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6330 | `		return PH7_OK;` |
|       - | 6331 | `	}` |
|      33 | 6332 | `	bPreserve = 0;` |
|      33 | 6333 | `	if( nArg > 2 ){` |
|       - | 6334 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6335 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6336 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6337 | `		 * normally, matching PHP behaviour. */` |
|      35 | 6338 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      35 | 6339 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6340 | `			ph7_value_is_resource(apArg[2]) ){` |
|       8 | 6341 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6342 | `				"TypeError",` |
|       - | 6343 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6344 | `				ph7_type_name(apArg[2])` |
|       - | 6345 | `				);` |
|       - | 6346 | `		}` |
|      21 | 6347 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6348 | `	}` |
|       - | 6349 | `	/* Start processing */` |
|      27 | 6350 | `	pEntry = pMap->pFirst;` |
|      27 | 6351 | `	nChunk = 0;` |
|      27 | 6352 | `	pChunk = 0;` |
|      27 | 6353 | `	n = pMap->nEntry;` |
|      56 | 6354 | `	for( ;; ){` |
|     113 | 6355 | `		if( n < 1 ){` |
|       - | 6356 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6357 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6358 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6359 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6360 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6361 | `			 * exists. */` |
|      27 | 6362 | `			if( pChunk ){` |
|      27 | 6363 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6364 | `			}` |
|      27 | 6365 | `			break;` |
|       - | 6366 | `		}` |
|      87 | 6367 | `		if( nChunk < 1 ){` |
|      71 | 6368 | `			if( pChunk ){` |
|       - | 6369 | `				/* Put the first chunk */` |
|      45 | 6370 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6371 | `			}` |
|       - | 6372 | `			/* Create a new dimension */` |
|      71 | 6373 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6374 | `												   * will be automatically released as soon we return` |
|       - | 6375 | `												   * from this function */` |
|      71 | 6376 | `			if( pChunk == 0 ){` |
|     ! 0 | 6377 | `				break;` |
|       - | 6378 | `			}` |
|      71 | 6379 | `			nChunk = nSize;` |
|      35 | 6380 | `		}` |
|       - | 6381 | `		/* Insert the entry */` |
|      87 | 6382 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6383 | `		/* Point to the next entry */` |
|      87 | 6384 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6385 | `		nChunk--;` |
|      87 | 6386 | `		n--;` |
|       1 | 6387 | `	}` |
|       - | 6388 | `	/* Return the multidimensional array */` |
|      27 | 6389 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6390 | `	return PH7_OK;` |
|      26 | 6391 | `}` |
|       - | 6392 | `/*` |
|       - | 6393 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6394 | ` *  Pad array to the specified length with a value.` |
|       - | 6395 | ` * $input` |
|       - | 6396 | ` *   Initial array of values to pad.` |
|       - | 6397 | ` * $pad_size` |
|       - | 6398 | ` *   New size of the array.` |
|       - | 6399 | ` * $pad_value` |
|       - | 6400 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6401 | ` */` |
|      28 | 6402 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6403 | `{` |
|       - | 6404 | `	ph7_hashmap *pMap;` |
|       - | 6405 | `	ph7_value *pArray;` |
|       - | 6406 | `	int nEntry;` |
|      33 | 6407 | `	if( nArg != 3 ){` |
|      12 | 6408 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6409 | `			"ArgumentCountError",` |
|       - | 6410 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6411 | `			nArg` |
|       - | 6412 | `			);` |
|       - | 6413 | `	}` |
|      24 | 6414 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6415 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6416 | `			"TypeError",` |
|       - | 6417 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6418 | `			ph7_type_name(apArg[0])` |
|       - | 6419 | `			);` |
|       - | 6420 | `	}` |
|       - | 6421 | `	/* Create a new array */` |
|      21 | 6422 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6423 | `	if( pArray == 0 ){` |
|     ! 0 | 6424 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6425 | `	}` |
|       - | 6426 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6427 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6428 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6429 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6430 | `	if( nEntry < 0 ){` |
|       9 | 6431 | `		nEntry = -nEntry;` |
|       9 | 6432 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6433 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6434 | `			/* Insert given items first */` |
|      17 | 6435 | `			while( nEntry > 0 ){` |
|      13 | 6436 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6437 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6438 | `				}` |
|      13 | 6439 | `				nEntry--;` |
|       1 | 6440 | `			}` |
|       - | 6441 | `			/* Merge the two arrays */` |
|       5 | 6442 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6443 | `		}else{` |
|       5 | 6444 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6445 | `		}` |
|      17 | 6446 | `	}else if( nEntry > 0 ){` |
|      11 | 6447 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6448 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6449 | `			/* Merge the two arrays first */` |
|       7 | 6450 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6451 | `			/* Insert given items */` |
|      25 | 6452 | `			while( nEntry > 0 ){` |
|      19 | 6453 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6454 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6455 | `				}` |
|      19 | 6456 | `				nEntry--;` |
|       1 | 6457 | `			}` |
|       4 | 6458 | `		}else{` |
|       5 | 6459 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6460 | `		}` |
|       6 | 6461 | `	}else{` |
|       - | 6462 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6463 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6464 | `	}` |
|       - | 6465 | `	/* Return the new array */` |
|      21 | 6466 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6467 | `	return PH7_OK;` |
|      19 | 6468 | `}` |
|       - | 6469 | `/*` |
|       - | 6470 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6471 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6472 | ` * Parameters` |
|       - | 6473 | ` * $array` |
|       - | 6474 | ` *   The array in which elements are replaced.` |
|       - | 6475 | ` * $array1` |
|       - | 6476 | ` *   The array from which elements will be extracted.` |
|       - | 6477 | ` * ....` |
|       - | 6478 | ` *  More arrays from which elements will be extracted.` |
|       - | 6479 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6480 | ` * Return` |
|       - | 6481 | ` *  Returns an array.` |
|       - | 6482 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6483 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6484 | ` */` |
|      22 | 6485 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 6486 | `{` |
|       - | 6487 | `	ph7_hashmap *pMap;` |
|       - | 6488 | `	ph7_value *pArray;` |
|       - | 6489 | `	int i;` |
|      26 | 6490 | `	if( nArg < 1 ){` |
|       3 | 6491 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6492 | `			"ArgumentCountError",` |
|       - | 6493 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6494 | `			);` |
|       - | 6495 | `	}` |
|      23 | 6496 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6497 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6498 | `			"TypeError",` |
|       - | 6499 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6500 | `			ph7_type_name(apArg[0])` |
|       - | 6501 | `			);` |
|       - | 6502 | `	}` |
|       - | 6503 | `	/* Create a new array */` |
|      20 | 6504 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6505 | `	if( pArray == 0 ){` |
|     ! 0 | 6506 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6507 | `		return PH7_OK;` |
|       - | 6508 | `	}` |
|       - | 6509 | `	/* Overwrite from the first array */` |
|      20 | 6510 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6511 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6512 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6513 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6514 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6515 | `			/* Type mismatch -> TypeError */` |
|       4 | 6516 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6517 | `				"TypeError",` |
|       - | 6518 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6519 | `				i + 1,` |
|       2 | 6520 | `				ph7_type_name(apArg[i])` |
|       - | 6521 | `				);` |
|       - | 6522 | `		}` |
|       - | 6523 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6524 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6525 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6526 | `	}` |
|       - | 6527 | `	/* Return the new array */` |
|      17 | 6528 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6529 | `	return PH7_OK;` |
|      15 | 6530 | `}` |
|       - | 6531 | `/*` |
|       - | 6532 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6533 | ` *  Filters elements of an array using a callback function.` |
|       - | 6534 | ` * Parameters` |
|       - | 6535 | ` *  $input` |
|       - | 6536 | ` *    The array to iterate over` |
|       - | 6537 | ` * $callback` |
|       - | 6538 | ` *    The callback function to use` |
|       - | 6539 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6540 | ` *    will be removed.` |
|       - | 6541 | ` * Return` |
|       - | 6542 | ` *  The filtered array.` |
|       - | 6543 | ` */` |
|      22 | 6544 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6545 | `{` |
|       - | 6546 | `	ph7_hashmap_node *pEntry;` |
|       - | 6547 | `	ph7_hashmap *pMap;` |
|       - | 6548 | `	ph7_value *pArray;` |
|       - | 6549 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6550 | `	ph7_value *pValue;` |
|       - | 6551 | `	sxi32 rc;` |
|       - | 6552 | `	int keep;` |
|       - | 6553 | `	sxu32 n;` |
|      24 | 6554 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6555 | `		/* Invalid arguments,return NULL */` |
|       5 | 6556 | `		ph7_result_null(pCtx);` |
|       5 | 6557 | `		return PH7_OK;` |
|       - | 6558 | `	}` |
|       - | 6559 | `	/* Create a new array */` |
|      20 | 6560 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6561 | `	if( pArray == 0 ){` |
|     ! 0 | 6562 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6563 | `		return PH7_OK;` |
|       - | 6564 | `	}` |
|       - | 6565 | `	/* Point to the internal representation of the input hashmap */` |
|      20 | 6566 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6567 | `	pEntry = pMap->pFirst;` |
|      20 | 6568 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      20 | 6569 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6570 | `	/* Perform the requested operation */` |
|      78 | 6571 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6572 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      64 | 6573 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      64 | 6574 | `		if( pValue == 0 ){` |
|       - | 6575 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6576 | `			keep = FALSE;` |
|      64 | 6577 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6578 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6579 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6580 | `				* silently dropped the element.  Emit similar message. */` |
|      36 | 6581 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6582 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6583 | `					int len;` |
|       3 | 6584 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6585 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6586 | `						"TypeError",` |
|       - | 6587 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6588 | `						zName` |
|       - | 6589 | `						);` |
|     ! 0 | 6590 | `				}else{` |
|     ! 0 | 6591 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6592 | `						"TypeError",` |
|       - | 6593 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6594 | `						ph7_type_name(apArg[1])` |
|       - | 6595 | `						);` |
|       - | 6596 | `				}` |
|       - | 6597 | `			}` |
|      33 | 6598 | `			keep = FALSE;` |
|      33 | 6599 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      33 | 6600 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 6601 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6602 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 6603 | `				return PH7_EXCEPTION;` |
|       - | 6604 | `			}` |
|      31 | 6605 | `			if( rc == SXRET_OK ){` |
|       - | 6606 | `				/* Perform a boolean cast */` |
|      31 | 6607 | `				keep = ph7_value_to_bool(&sResult);` |
|      15 | 6608 | `			}` |
|      31 | 6609 | `			PH7_MemObjRelease(&sResult);` |
|      16 | 6610 | `		}else{` |
|       - | 6611 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6612 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6613 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6614 | `			 */` |
|      29 | 6615 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6616 | `		}` |
|      59 | 6617 | `		if( keep ){` |
|       - | 6618 | `			/* Perform the insertion,now the callback returned true */` |
|      21 | 6619 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 6620 | `		}` |
|       - | 6621 | `		/* Point to the next entry */` |
|      59 | 6622 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      30 | 6623 | `	}` |
|      15 | 6624 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 6625 | `	return PH7_OK;` |
|      13 | 6626 | `}` |
|       - | 6627 | `/*` |
|       - | 6628 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 6629 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 6630 | ` * Parameters` |
|       - | 6631 | ` *  $callback` |
|       - | 6632 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 6633 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 6634 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 6635 | ` *   are zipped together.` |
|       - | 6636 | ` *  $array` |
|       - | 6637 | ` *   The first array to run through the callback function.` |
|       - | 6638 | ` *  $arrays` |
|       - | 6639 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 6640 | ` * Return` |
|       - | 6641 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 6642 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 6643 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 6644 | ` *  padding shorter arrays with NULL.` |
|       - | 6645 | ` */` |
|      54 | 6646 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6647 | `{` |
|       - | 6648 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6649 | `	ph7_hashmap_node *pEntry;` |
|       - | 6650 | `	ph7_hashmap *pMap;` |
|       - | 6651 | `	ph7_vm *pVm;` |
|       - | 6652 | `	int bNullCallback;` |
|       - | 6653 | `	sxi32 rc;` |
|       - | 6654 | `	int i;` |
|       - | 6655 | `	sxu32 n;` |
|      59 | 6656 | `	if( nArg < 2 ){` |
|       8 | 6657 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6658 | `			"ArgumentCountError",` |
|       - | 6659 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6660 | `			nArg` |
|       - | 6661 | `			);` |
|       - | 6662 | `	}` |
|      54 | 6663 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      54 | 6664 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6665 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6666 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6667 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6668 | `				"TypeError",` |
|       - | 6669 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6670 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6671 | `				zFunc` |
|       - | 6672 | `				);` |
|       - | 6673 | `		}` |
|       3 | 6674 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6675 | `			"TypeError",` |
|       - | 6676 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6677 | `			"no array or string given"` |
|       - | 6678 | `			);` |
|       - | 6679 | `	}` |
|       - | 6680 | `	/* Every remaining argument must be an array */` |
|     105 | 6681 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      61 | 6682 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 6683 | `			if( i == 1 ){` |
|       4 | 6684 | `				return PH7_VmThrowException(pCtx,` |
|       - | 6685 | `					"TypeError",` |
|       - | 6686 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6687 | `					ph7_type_name(apArg[1])` |
|       - | 6688 | `					);` |
|       - | 6689 | `			}` |
|     ! 0 | 6690 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6691 | `				"TypeError",` |
|       - | 6692 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 6693 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 6694 | `				);` |
|       - | 6695 | `		}` |
|      30 | 6696 | `	}` |
|      46 | 6697 | `	pVm = pCtx->pVm;` |
|       - | 6698 | `	/* Create a new array */` |
|      46 | 6699 | `	pArray = ph7_context_new_array(pCtx);` |
|      46 | 6700 | `	if( pArray == 0 ){` |
|     ! 0 | 6701 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6702 | `		return PH7_OK;` |
|       - | 6703 | `	}` |
|      46 | 6704 | `	PH7_MemObjInit(pVm,&sResult);` |
|      46 | 6705 | `	PH7_MemObjInit(pVm,&sKey);` |
|      46 | 6706 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      46 | 6707 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      46 | 6708 | `	if( nArg == 2 ){` |
|       - | 6709 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      36 | 6710 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      36 | 6711 | `		pEntry = pMap->pFirst;` |
|     110 | 6712 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6713 | `			/* Extract the node value */` |
|      78 | 6714 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|      78 | 6715 | `			if( pValue ){` |
|       - | 6716 | `				/* Extract the node key */` |
|      78 | 6717 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      78 | 6718 | `				if( bNullCallback ){` |
|       - | 6719 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 6720 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6721 | `				}else{` |
|       - | 6722 | `					/* Invoke the supplied callback */` |
|      68 | 6723 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|      68 | 6724 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 6725 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 6726 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       3 | 6727 | `						PH7_MemObjRelease(&sKey);` |
|       3 | 6728 | `						PH7_MemObjRelease(&sResult);` |
|       3 | 6729 | `						return PH7_EXCEPTION;` |
|       - | 6730 | `					}` |
|       - | 6731 | `					/* Insert the callback return value */` |
|      66 | 6732 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6733 | `				}` |
|      76 | 6734 | `				PH7_MemObjRelease(&sKey);` |
|      76 | 6735 | `				PH7_MemObjRelease(&sResult);` |
|      37 | 6736 | `			}` |
|       - | 6737 | `			/* Point to the next entry */` |
|      76 | 6738 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      39 | 6739 | `		}` |
|      18 | 6740 | `	}else{` |
|       - | 6741 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 6742 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 6743 | `		int nArrays = nArg - 1;` |
|       - | 6744 | `		ph7_hashmap_node **apCur;` |
|       - | 6745 | `		ph7_value **apCallArg;` |
|       - | 6746 | `		ph7_value sNull;` |
|      11 | 6747 | `		sxu32 nMax = 0;` |
|      11 | 6748 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 6749 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 6750 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 6751 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 6752 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 6753 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6754 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6755 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 6756 | `			return PH7_OK;` |
|       - | 6757 | `		}` |
|      11 | 6758 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 6759 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 6760 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 6761 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 6762 | `			apCur[i] = pMap->pFirst;` |
|      23 | 6763 | `			if( pMap->nEntry > nMax ){` |
|      13 | 6764 | `				nMax = pMap->nEntry;` |
|       6 | 6765 | `			}` |
|      12 | 6766 | `		}` |
|      35 | 6767 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 6768 | `			ph7_value *pZip = 0;` |
|      25 | 6769 | `			if( bNullCallback ){` |
|       - | 6770 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 6771 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 6772 | `			}` |
|      79 | 6773 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 6774 | `				ph7_value *pv = &sNull;` |
|      55 | 6775 | `				if( apCur[i] ){` |
|      53 | 6776 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 6777 | `					if( pNodeVal ){` |
|      53 | 6778 | `						pv = pNodeVal;` |
|      26 | 6779 | `					}` |
|      53 | 6780 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 6781 | `				}` |
|      55 | 6782 | `				if( bNullCallback ){` |
|       9 | 6783 | `					if( pZip ){` |
|       9 | 6784 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 6785 | `					}` |
|       5 | 6786 | `				}else{` |
|      47 | 6787 | `					apCallArg[i] = pv;` |
|       - | 6788 | `				}` |
|      28 | 6789 | `			}` |
|      25 | 6790 | `			if( bNullCallback ){` |
|       5 | 6791 | `				if( pZip ){` |
|       5 | 6792 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 6793 | `				}` |
|       3 | 6794 | `			}else{` |
|      21 | 6795 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 6796 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6797 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 6798 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 6799 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 6800 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6801 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6802 | `					return PH7_EXCEPTION;` |
|       - | 6803 | `				}` |
|      21 | 6804 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 6805 | `				PH7_MemObjRelease(&sResult);` |
|       - | 6806 | `			}` |
|      13 | 6807 | `		}` |
|      11 | 6808 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 6809 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 6810 | `		PH7_MemObjRelease(&sNull);` |
|       - | 6811 | `	}` |
|      44 | 6812 | `	PH7_MemObjRelease(&sKey);` |
|      44 | 6813 | `	PH7_MemObjRelease(&sResult);` |
|      44 | 6814 | `	ph7_result_value(pCtx,pArray);` |
|      44 | 6815 | `	return PH7_OK;` |
|      32 | 6816 | `}` |
|       - | 6817 | `/*` |
|       - | 6818 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6819 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6820 | ` * Parameters` |
|       - | 6821 | ` *  $array` |
|       - | 6822 | ` *   The input array.` |
|       - | 6823 | ` *  $callback` |
|       - | 6824 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6825 | ` *  $initial` |
|       - | 6826 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6827 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6828 | ` * Return` |
|       - | 6829 | ` *  Returns the resulting value.` |
|       - | 6830 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6831 | ` */` |
|      34 | 6832 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6833 | `{` |
|       - | 6834 | `	ph7_hashmap_node *pEntry;` |
|       - | 6835 | `	ph7_hashmap *pMap;` |
|       - | 6836 | `	ph7_value *pValue;` |
|       - | 6837 | `	ph7_value sResult;` |
|       - | 6838 | `	sxi32 rc;` |
|       - | 6839 | `	sxu32 n;` |
|      39 | 6840 | `	if( nArg < 2 ){` |
|       8 | 6841 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6842 | `			"ArgumentCountError",` |
|       - | 6843 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6844 | `			nArg` |
|       - | 6845 | `			);` |
|       - | 6846 | `	}` |
|      35 | 6847 | `	if( nArg > 3 ){` |
|       4 | 6848 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6849 | `			"ArgumentCountError",` |
|       - | 6850 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6851 | `			nArg` |
|       - | 6852 | `			);` |
|       - | 6853 | `	}` |
|      33 | 6854 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6855 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6856 | `			"TypeError",` |
|       - | 6857 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6858 | `			ph7_type_name(apArg[0])` |
|       - | 6859 | `			);` |
|       - | 6860 | `	}` |
|      31 | 6861 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      12 | 6862 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6863 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6864 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6865 | `				"TypeError",` |
|       - | 6866 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6867 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6868 | `				zFunc` |
|       - | 6869 | `				);` |
|       - | 6870 | `		}` |
|       9 | 6871 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6872 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6873 | `				"TypeError",` |
|       - | 6874 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6875 | `				"array callback must have exactly two members"` |
|       - | 6876 | `				);` |
|       - | 6877 | `		}` |
|       6 | 6878 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6879 | `			"TypeError",` |
|       - | 6880 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6881 | `			"no array or string given"` |
|       - | 6882 | `			);` |
|       - | 6883 | `	}` |
|       - | 6884 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6885 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6886 | `	/* Assume a NULL initial value */` |
|      19 | 6887 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 6888 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 6889 | `	if( nArg > 2 ){` |
|       - | 6890 | `		/* Set the initial value */` |
|      13 | 6891 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 6892 | `	}` |
|       - | 6893 | `	/* Perform the requested operation */` |
|      19 | 6894 | `	pEntry = pMap->pFirst;` |
|      55 | 6895 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6896 | `		/* Extract the node value */` |
|      39 | 6897 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6898 | `		/* Invoke the supplied callback */` |
|      39 | 6899 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      39 | 6900 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 6901 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6902 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 6903 | `			return PH7_EXCEPTION;` |
|       - | 6904 | `		}` |
|       - | 6905 | `		/* Point to the next entry */` |
|      37 | 6906 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6907 | `	}` |
|      17 | 6908 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      17 | 6909 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 6910 | `	return PH7_OK;` |
|      22 | 6911 | `}` |
|       - | 6912 | `/*` |
|       - | 6913 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6914 | ` *  Apply a user function to every member of an array.` |
|       - | 6915 | ` * Parameters` |
|       - | 6916 | ` *  $array` |
|       - | 6917 | ` *   The input array.` |
|       - | 6918 | ` *  $funcname` |
|       - | 6919 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6920 | ` *   the first, and the key/index second.` |
|       - | 6921 | ` * Note:` |
|       - | 6922 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6923 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6924 | ` *  be made in the original array itself.` |
|       - | 6925 | ` *  $userdata` |
|       - | 6926 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6927 | ` *   to the callback funcname.` |
|       - | 6928 | ` * Return` |
|       - | 6929 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6930 | ` */` |
|      38 | 6931 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6932 | `{` |
|       - | 6933 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6934 | `	ph7_hashmap_node *pEntry;` |
|       - | 6935 | `	ph7_hashmap *pMap;` |
|       - | 6936 | `	sxu32 n;` |
|      43 | 6937 | `	if( nArg < 2 ){` |
|       8 | 6938 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6939 | `			"ArgumentCountError",` |
|       - | 6940 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6941 | `			nArg` |
|       - | 6942 | `			);` |
|       - | 6943 | `	}` |
|      39 | 6944 | `	if( nArg > 3 ){` |
|       4 | 6945 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6946 | `			"ArgumentCountError",` |
|       - | 6947 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6948 | `			nArg` |
|       - | 6949 | `			);` |
|       - | 6950 | `	}` |
|      37 | 6951 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6952 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6953 | `			"TypeError",` |
|       - | 6954 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6955 | `			ph7_type_name(apArg[0])` |
|       - | 6956 | `			);` |
|       - | 6957 | `	}` |
|      35 | 6958 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 6959 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6960 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6961 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6962 | `				"TypeError",` |
|       - | 6963 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6964 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6965 | `				zFunc` |
|       - | 6966 | `				);` |
|       - | 6967 | `		}` |
|      12 | 6968 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 6969 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6970 | `				"TypeError",` |
|       - | 6971 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6972 | `				"array callback must have exactly two members"` |
|       - | 6973 | `				);` |
|       - | 6974 | `		}` |
|       6 | 6975 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6976 | `			"TypeError",` |
|       - | 6977 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6978 | `			"no array or string given"` |
|       - | 6979 | `			);` |
|       - | 6980 | `	}` |
|      21 | 6981 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6982 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6983 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 6984 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 6985 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 6986 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6987 | `	/* Perform the desired operation */` |
|      21 | 6988 | `	pEntry = pMap->pFirst;` |
|      61 | 6989 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6990 | `		/* Extract the node value */` |
|      43 | 6991 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 6992 | `		if( pValue ){` |
|       - | 6993 | `			sxi32 rcW;` |
|       - | 6994 | `			/* Extract the entry key */` |
|      43 | 6995 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6996 | `			/* Invoke the supplied callback */` |
|      43 | 6997 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 6998 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 6999 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 7000 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 7001 | `				return PH7_EXCEPTION;` |
|       - | 7002 | `			}` |
|      20 | 7003 | `		}` |
|       - | 7004 | `		/* Point to the next entry */` |
|      41 | 7005 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 7006 | `	}` |
|       - | 7007 | `	/* All done, return TRUE */` |
|      19 | 7008 | `	ph7_result_bool(pCtx,1);` |
|      19 | 7009 | `	return PH7_OK;` |
|      24 | 7010 | `}` |
|       - | 7011 | `/*` |
|       - | 7012 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 7013 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 7014 | ` */` |
|      22 | 7015 | `static sxi32 HashmapWalkRecursive(` |
|       - | 7016 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 7017 | `	ph7_value *pCallback, /* User callback */` |
|       - | 7018 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 7019 | `	int iNest             /* Nesting level */` |
|       - | 7020 | `	)` |
|       1 | 7021 | `{` |
|       - | 7022 | `	ph7_hashmap_node *pEntry;` |
|       - | 7023 | `	ph7_value *pValue,sKey;` |
|       - | 7024 | `	sxi32 rc;` |
|       - | 7025 | `	sxu32 n;` |
|       - | 7026 | `	/* Iterate through hashmap entries */` |
|      23 | 7027 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 7028 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 7029 | `	pEntry = pMap->pFirst;` |
|      59 | 7030 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7031 | `		/* Extract the node value */` |
|      37 | 7032 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 7033 | `		if( pValue ){` |
|      37 | 7034 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 7035 | `				if( iNest < 32 ){` |
|       - | 7036 | `					/* Recurse */` |
|      11 | 7037 | `					iNest++;` |
|      11 | 7038 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 7039 | `					iNest--;` |
|      11 | 7040 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 7041 | `						return PH7_EXCEPTION;` |
|       - | 7042 | `					}` |
|       5 | 7043 | `				}` |
|       6 | 7044 | `			}else{` |
|       - | 7045 | `				/* Extract the node key */` |
|      27 | 7046 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 7047 | `				/* Invoke the supplied callback */` |
|      27 | 7048 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 7049 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 7050 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 7051 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7052 | `					return PH7_EXCEPTION;` |
|       - | 7053 | `				}` |
|       - | 7054 | `			}` |
|      18 | 7055 | `		}` |
|       - | 7056 | `		/* Point to the next entry */` |
|      37 | 7057 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 7058 | `	}` |
|      23 | 7059 | `	return PH7_OK;` |
|      12 | 7060 | `}` |
|       - | 7061 | `/*` |
|       - | 7062 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 7063 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 7064 | ` * Parameters` |
|       - | 7065 | ` *  $array` |
|       - | 7066 | ` *   The input array.` |
|       - | 7067 | ` *  $funcname` |
|       - | 7068 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 7069 | ` *   the first, and the key/index second.` |
|       - | 7070 | ` * Note:` |
|       - | 7071 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 7072 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 7073 | ` *  be made in the original array itself.` |
|       - | 7074 | ` *  $userdata` |
|       - | 7075 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 7076 | ` *   to the callback funcname.` |
|       - | 7077 | ` * Return` |
|       - | 7078 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 7079 | ` */` |
|      30 | 7080 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7081 | `{` |
|       - | 7082 | `	ph7_hashmap *pMap;` |
|      35 | 7083 | `	if( nArg < 2 ){` |
|       8 | 7084 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7085 | `			"ArgumentCountError",` |
|       - | 7086 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 7087 | `			nArg` |
|       - | 7088 | `			);` |
|       - | 7089 | `	}` |
|      31 | 7090 | `	if( nArg > 3 ){` |
|       4 | 7091 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7092 | `			"ArgumentCountError",` |
|       - | 7093 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 7094 | `			nArg` |
|       - | 7095 | `			);` |
|       - | 7096 | `	}` |
|      29 | 7097 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7098 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7099 | `			"TypeError",` |
|       - | 7100 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7101 | `			ph7_type_name(apArg[0])` |
|       - | 7102 | `			);` |
|       - | 7103 | `	}` |
|      27 | 7104 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 7105 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7106 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7107 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7108 | `				"TypeError",` |
|       - | 7109 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7110 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7111 | `				zFunc` |
|       - | 7112 | `				);` |
|       - | 7113 | `		}` |
|      12 | 7114 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 7115 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7116 | `				"TypeError",` |
|       - | 7117 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7118 | `				"array callback must have exactly two members"` |
|       - | 7119 | `				);` |
|       - | 7120 | `		}` |
|       6 | 7121 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7122 | `			"TypeError",` |
|       - | 7123 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7124 | `			"no array or string given"` |
|       - | 7125 | `			);` |
|       - | 7126 | `	}` |
|       - | 7127 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 7128 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 7129 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7130 | `	/* Perform the desired operation */` |
|      13 | 7131 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 7132 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7133 | `		return PH7_EXCEPTION;` |
|       - | 7134 | `	}` |
|       - | 7135 | `	/* All done, return TRUE */` |
|      13 | 7136 | `	ph7_result_bool(pCtx,1);` |
|      13 | 7137 | `	return PH7_OK;` |
|      20 | 7138 | `}` |
|       - | 7139 | `/*` |
|       - | 7140 | ` * bool array_is_list(array $array)` |
|       - | 7141 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 7142 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 7143 | ` * Return` |
|       - | 7144 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 7145 | ` */` |
|       - | 7146 | `/*` |
|       - | 7147 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 7148 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 7149 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 7150 | ` */` |
|     114 | 7151 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       1 | 7152 | `{` |
|     115 | 7153 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|     115 | 7154 | `	sxi64 iExpect = 0;` |
|       - | 7155 | `	sxu32 n;` |
|     233 | 7156 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     169 | 7157 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 7158 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|      51 | 7159 | `			return 0;` |
|       - | 7160 | `		}` |
|     119 | 7161 | `		++iExpect;` |
|     119 | 7162 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      60 | 7163 | `	}` |
|      65 | 7164 | `	return 1;` |
|      58 | 7165 | `}` |
|      12 | 7166 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7167 | `{` |
|      13 | 7168 | `	if( nArg < 1 ){` |
|     ! 0 | 7169 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7170 | `			"ArgumentCountError",` |
|       - | 7171 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 7172 | `			);` |
|       - | 7173 | `	}` |
|      13 | 7174 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7175 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7176 | `			"TypeError",` |
|       - | 7177 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7178 | `			ph7_type_name(apArg[0])` |
|       - | 7179 | `			);` |
|       - | 7180 | `	}` |
|      13 | 7181 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 7182 | `	return PH7_OK;` |
|       7 | 7183 | `}` |
|       - | 7184 | `/*` |
|       - | 7185 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 7186 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 7187 | ` * array_column() for both the column value and the index key.` |
|       - | 7188 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 7189 | ` * container or the key is absent.` |
|       - | 7190 | ` */` |
|      32 | 7191 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 7192 | `{` |
|      33 | 7193 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 7194 | `		ph7_hashmap_node *pNode;` |
|      25 | 7195 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 7196 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 7197 | `		}` |
|      11 | 7198 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 7199 | `		ph7_value sName;` |
|       - | 7200 | `		const char *zName;` |
|       - | 7201 | `		ph7_value *pAttr;` |
|       - | 7202 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 7203 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 7204 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 7205 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 7206 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 7207 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 7208 | `		PH7_MemObjRelease(&sName);` |
|       9 | 7209 | `		return pAttr;` |
|       - | 7210 | `	}` |
|       5 | 7211 | `	return 0;` |
|      17 | 7212 | `}` |
|       - | 7213 | `/*` |
|       - | 7214 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 7215 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 7216 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 7217 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 7218 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 7219 | ` *  Each row may be an array or an object.` |
|       - | 7220 | ` */` |
|      12 | 7221 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7222 | `{` |
|       - | 7223 | `	ph7_hashmap_node *pNode;` |
|       - | 7224 | `	ph7_hashmap *pMap;` |
|       - | 7225 | `	ph7_value *pArray;` |
|       - | 7226 | `	ph7_value *pRow;` |
|       - | 7227 | `	ph7_value *pCol;` |
|       - | 7228 | `	ph7_value *pIdx;` |
|       - | 7229 | `	int bWantCol;` |
|       - | 7230 | `	int bWantIdx;` |
|       - | 7231 | `	sxu32 n;` |
|      13 | 7232 | `	if( nArg < 2 ){` |
|     ! 0 | 7233 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7234 | `			"ArgumentCountError",` |
|       - | 7235 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 7236 | `			nArg` |
|       - | 7237 | `			);` |
|       - | 7238 | `	}` |
|      13 | 7239 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7240 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7241 | `			"TypeError",` |
|       - | 7242 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7243 | `			ph7_type_name(apArg[0])` |
|       - | 7244 | `			);` |
|       - | 7245 | `	}` |
|      13 | 7246 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 7247 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 7248 | `	if( pArray == 0 ){` |
|     ! 0 | 7249 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7250 | `		return PH7_OK;` |
|       - | 7251 | `	}` |
|       - | 7252 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 7253 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 7254 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 7255 | `	pNode = pMap->pFirst;` |
|      33 | 7256 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 7257 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 7258 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 7259 | `		if( pRow == 0 ){` |
|     ! 0 | 7260 | `			continue;` |
|       - | 7261 | `		}` |
|      21 | 7262 | `		if( bWantCol ){` |
|      19 | 7263 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 7264 | `			if( pCol == 0 ){` |
|       - | 7265 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 7266 | `				continue;` |
|       - | 7267 | `			}` |
|       9 | 7268 | `		}else{` |
|       3 | 7269 | `			pCol = pRow;` |
|       - | 7270 | `		}` |
|      19 | 7271 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 7272 | `		if( pIdx ){` |
|      13 | 7273 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 7274 | `		}else{` |
|       7 | 7275 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 7276 | `		}` |
|      10 | 7277 | `	}` |
|      13 | 7278 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 7279 | `	return PH7_OK;` |
|       7 | 7280 | `}` |
|       - | 7281 | `/*` |
|       - | 7282 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 7283 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 7284 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 7285 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 7286 | ` */` |
|      28 | 7287 | `static sxi32 HashmapCallbackSearch(` |
|       - | 7288 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 7289 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 7290 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 7291 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 7292 | `	)` |
|       1 | 7293 | `{` |
|       - | 7294 | `	ph7_hashmap_node *pEntry;` |
|       - | 7295 | `	ph7_hashmap *pMap;` |
|       - | 7296 | `	ph7_value *pValue;` |
|       - | 7297 | `	ph7_value *apCbArg[2];` |
|       - | 7298 | `	ph7_value sKey;` |
|       - | 7299 | `	ph7_value sResult;` |
|       - | 7300 | `	sxi32 rc;` |
|       - | 7301 | `	sxu32 n;` |
|      29 | 7302 | `	*ppMatch = 0;` |
|      29 | 7303 | `	if( nArg < 2 ){` |
|     ! 0 | 7304 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7305 | `			"ArgumentCountError",` |
|       - | 7306 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 7307 | `			zName,nArg` |
|       - | 7308 | `			);` |
|       - | 7309 | `	}` |
|      29 | 7310 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7311 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7312 | `			"TypeError",` |
|       - | 7313 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7314 | `			zName,ph7_type_name(apArg[0])` |
|       - | 7315 | `			);` |
|       - | 7316 | `	}` |
|      29 | 7317 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7318 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7319 | `			"TypeError",` |
|       - | 7320 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 7321 | `			zName,ph7_type_name(apArg[1])` |
|       - | 7322 | `			);` |
|       - | 7323 | `	}` |
|      29 | 7324 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 7325 | `	pEntry = pMap->pFirst;` |
|      29 | 7326 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 7327 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 7328 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 7329 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 7330 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 7331 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 7332 | `		if( pValue ){` |
|       - | 7333 | `			/* The callback receives ($value, $key). */` |
|      59 | 7334 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 7335 | `			apCbArg[0] = pValue;` |
|      59 | 7336 | `			apCbArg[1] = &sKey;` |
|      59 | 7337 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 7338 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 7339 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7340 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7341 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7342 | `				return PH7_EXCEPTION;` |
|       - | 7343 | `			}` |
|      59 | 7344 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 7345 | `				*ppMatch = pEntry;` |
|      15 | 7346 | `				break;` |
|       - | 7347 | `			}` |
|      22 | 7348 | `		}` |
|      45 | 7349 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 7350 | `	}` |
|      29 | 7351 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 7352 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 7353 | `	return PH7_OK;` |
|      15 | 7354 | `}` |
|       - | 7355 | `/*` |
|       - | 7356 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 7357 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 7358 | ` *  is truthy, or NULL if none match.` |
|       - | 7359 | ` */` |
|       6 | 7360 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7361 | `{` |
|       - | 7362 | `	ph7_hashmap_node *pMatch;` |
|       - | 7363 | `	ph7_value *pVal;` |
|       - | 7364 | `	sxi32 rc;` |
|       7 | 7365 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 7366 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7367 | `		return rc;` |
|       - | 7368 | `	}` |
|       7 | 7369 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 7370 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 7371 | `	}else{` |
|       3 | 7372 | `		ph7_result_null(pCtx);` |
|       - | 7373 | `	}` |
|       7 | 7374 | `	return PH7_OK;` |
|       4 | 7375 | `}` |
|       - | 7376 | `/*` |
|       - | 7377 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 7378 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 7379 | ` *  is truthy, or NULL if none match.` |
|       - | 7380 | ` */` |
|       6 | 7381 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7382 | `{` |
|       - | 7383 | `	ph7_hashmap_node *pMatch;` |
|       - | 7384 | `	sxi32 rc;` |
|       7 | 7385 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 7386 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7387 | `		return rc;` |
|       - | 7388 | `	}` |
|       7 | 7389 | `	if( pMatch == 0 ){` |
|       3 | 7390 | `		ph7_result_null(pCtx);` |
|       6 | 7391 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 7392 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 7393 | `	}else{` |
|       4 | 7394 | `		ph7_result_string(pCtx,` |
|       2 | 7395 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 7396 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 7397 | `	}` |
|       7 | 7398 | `	return PH7_OK;` |
|       4 | 7399 | `}` |
|       - | 7400 | `/*` |
|       - | 7401 | ` * bool array_any(array $array, callable $callback)` |
|       - | 7402 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 7403 | ` *  FALSE for an empty array.` |
|       - | 7404 | ` */` |
|       8 | 7405 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7406 | `{` |
|       - | 7407 | `	ph7_hashmap_node *pMatch;` |
|       - | 7408 | `	sxi32 rc;` |
|       9 | 7409 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 7410 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7411 | `		return rc;` |
|       - | 7412 | `	}` |
|       9 | 7413 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 7414 | `	return PH7_OK;` |
|       5 | 7415 | `}` |
|       - | 7416 | `/*` |
|       - | 7417 | ` * bool array_all(array $array, callable $callback)` |
|       - | 7418 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 7419 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 7420 | ` */` |
|       8 | 7421 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7422 | `{` |
|       - | 7423 | `	ph7_hashmap_node *pMatch;` |
|       - | 7424 | `	sxi32 rc;` |
|       9 | 7425 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 7426 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7427 | `		return rc;` |
|       - | 7428 | `	}` |
|       9 | 7429 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 7430 | `	return PH7_OK;` |
|       5 | 7431 | `}` |
|       - | 7432 | `/*` |
|       - | 7433 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 7434 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 7435 | ` */` |
|       - | 7436 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 7437 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|      70 | 7438 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       5 | 7439 | `{` |
|      75 | 7440 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|      35 | 7441 | `	(void)pVm;` |
|      75 | 7442 | `	p->nCount++;` |
|      75 | 7443 | `	if( p->pArray ){` |
|       - | 7444 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 7445 | `		 * otherwise append with an auto-assigned int index. */` |
|      67 | 7446 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|      31 | 7447 | `	}` |
|      75 | 7448 | `	return SXRET_OK;` |
|       5 | 7449 | `}` |
|       - | 7450 | `/*` |
|       - | 7451 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 7452 | ` */` |
|      26 | 7453 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 7454 | `{` |
|       - | 7455 | `	struct IterCollect sCol;` |
|       - | 7456 | `	ph7_value *pArray;` |
|       - | 7457 | `	sxi32 rc;` |
|      31 | 7458 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      31 | 7459 | `	pArray = ph7_context_new_array(pCtx);` |
|      31 | 7460 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      31 | 7461 | `	sCol.pArray = pArray;` |
|      31 | 7462 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|      31 | 7463 | `	sCol.nCount = 0;` |
|      31 | 7464 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 7465 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 7466 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 7467 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7468 | `		sxu32 n;` |
|       9 | 7469 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7470 | `			ph7_value sKey, *pVal;` |
|       7 | 7471 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 7472 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 7473 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 7474 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 7475 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 7476 | `			pEntry = pEntry->pPrev;` |
|       4 | 7477 | `		}` |
|       3 | 7478 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 7479 | `		return PH7_OK;` |
|       - | 7480 | `	}` |
|      29 | 7481 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      29 | 7482 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      27 | 7483 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7484 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7485 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 7486 | `			ph7_type_name(apArg[0]));` |
|       - | 7487 | `	}` |
|      27 | 7488 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 7489 | `	return PH7_OK;` |
|      18 | 7490 | `}` |
|       - | 7491 | `/*` |
|       - | 7492 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 7493 | ` */` |
|       6 | 7494 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 7495 | `{` |
|       - | 7496 | `	struct IterCollect sCol;` |
|       - | 7497 | `	sxi32 rc;` |
|       7 | 7498 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       7 | 7499 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 7500 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 7501 | `		return PH7_OK;` |
|       - | 7502 | `	}` |
|       5 | 7503 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|       5 | 7504 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|       5 | 7505 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       5 | 7506 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7507 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7508 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 7509 | `			ph7_type_name(apArg[0]));` |
|       - | 7510 | `	}` |
|       5 | 7511 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|       5 | 7512 | `	return PH7_OK;` |
|       4 | 7513 | `}` |
|       - | 7514 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 7515 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 7516 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 7517 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      24 | 7518 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       1 | 7519 | `{` |
|      25 | 7520 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 7521 | `	ph7_value sResult;` |
|       - | 7522 | `	SySet aArg;` |
|       - | 7523 | `	sxi32 rc;` |
|       - | 7524 | `	int bContinue;` |
|      12 | 7525 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      25 | 7526 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      25 | 7527 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 7528 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|       9 | 7529 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7530 | `		sxu32 n;` |
|      17 | 7531 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       9 | 7532 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       9 | 7533 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|       9 | 7534 | `			pEntry = pEntry->pPrev;` |
|       5 | 7535 | `		}` |
|       4 | 7536 | `	}` |
|      25 | 7537 | `	PH7_MemObjInit(pVm,&sResult);` |
|      37 | 7538 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      24 | 7539 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|      25 | 7540 | `	SySetRelease(&aArg);` |
|      25 | 7541 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      23 | 7542 | `	p->nCount++;` |
|      23 | 7543 | `	PH7_MemObjToBool(&sResult);` |
|      23 | 7544 | `	bContinue = (sResult.x.iVal != 0);` |
|      23 | 7545 | `	PH7_MemObjRelease(&sResult);` |
|      23 | 7546 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      13 | 7547 | `}` |
|       - | 7548 | `/*` |
|       - | 7549 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 7550 | ` */` |
|       8 | 7551 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 7552 | `{` |
|       - | 7553 | `	struct IterApply sApp;` |
|       - | 7554 | `	sxi32 rc;` |
|       9 | 7555 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       9 | 7556 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7557 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7558 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|       - | 7559 | `	}` |
|       9 | 7560 | `	sApp.pCallback = apArg[1];` |
|       9 | 7561 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|       9 | 7562 | `	sApp.nCount = 0;` |
|       9 | 7563 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|       9 | 7564 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       7 | 7565 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7566 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7567 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 7568 | `			ph7_type_name(apArg[0]));` |
|       - | 7569 | `	}` |
|       7 | 7570 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|       7 | 7571 | `	return PH7_OK;` |
|       5 | 7572 | `}` |
|       - | 7573 | `/*` |
|       - | 7574 | ` * Table of hashmap functions.` |
|       - | 7575 | ` */` |
|       - | 7576 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 7577 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|       - | 7578 | `	{"iterator_count",     ph7_iterator_count },` |
|       - | 7579 | `	{"iterator_apply",     ph7_iterator_apply },` |
|       - | 7580 | `	{"count",             ph7_hashmap_count },` |
|       - | 7581 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 7582 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 7583 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 7584 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 7585 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 7586 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 7587 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 7588 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 7589 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 7590 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 7591 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 7592 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 7593 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 7594 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 7595 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 7596 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 7597 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 7598 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 7599 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 7600 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 7601 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 7602 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 7603 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 7604 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 7605 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 7606 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 7607 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 7608 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 7609 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 7610 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 7611 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 7612 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 7613 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 7614 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 7615 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 7616 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 7617 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 7618 | `	{"array_column",      ph7_hashmap_column  },` |
|       - | 7619 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|       - | 7620 | `	{"array_find",        ph7_hashmap_find    },` |
|       - | 7621 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|       - | 7622 | `	{"array_any",         ph7_hashmap_any     },` |
|       - | 7623 | `	{"array_all",         ph7_hashmap_all     },` |
|       - | 7624 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 7625 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 7626 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 7627 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 7628 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 7629 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 7630 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 7631 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 7632 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 7633 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 7634 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 7635 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 7636 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 7637 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 7638 | `	{"range",             ph7_hashmap_range   },` |
|       - | 7639 | `	{"current",           ph7_hashmap_current },` |
|       - | 7640 | `	{"each",              ph7_hashmap_each    },` |
|       - | 7641 | `	{"pos",               ph7_hashmap_current },` |
|       - | 7642 | `	{"next",              ph7_hashmap_next    },` |
|       - | 7643 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 7644 | `	{"end",               ph7_hashmap_end     },` |
|       - | 7645 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 7646 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 7647 | `};` |
|       - | 7648 | `/*` |
|       - | 7649 | ` * Register the built-in hashmap functions defined above.` |
|       - | 7650 | ` */` |
|    3420 | 7651 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       5 | 7652 | `{` |
|       - | 7653 | `	sxu32 n;` |
|  242825 | 7654 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  239405 | 7655 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|  119705 | 7656 | `	}` |
|    3425 | 7657 | `}` |
|       - | 7658 | `/*` |
|       - | 7659 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 7660 | ` * the BLOB given as the first argument.` |
|       - | 7661 | ` * This function is typically invoked when the user issue a call to` |
|       - | 7662 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 7663 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 7664 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 7665 | ` */` |
|       - | 7666 | `/*` |
|       - | 7667 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|       - | 7668 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|       - | 7669 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|       - | 7670 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|       - | 7671 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|       - | 7672 | ` */` |
|      26 | 7673 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       3 | 7674 | `{` |
|      29 | 7675 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7676 | `	ph7_value *pObj;` |
|      29 | 7677 | `	sxu32 n = 0;` |
|       - | 7678 | `	int isRef;` |
|      29 | 7679 | `	sxi32 rc = SXRET_OK;` |
|       - | 7680 | `	int i;` |
|      44 | 7681 | `	for(;;){` |
|      91 | 7682 | `		if( n >= pMap->nEntry ){` |
|      29 | 7683 | `			break;` |
|       - | 7684 | `		}` |
|     127 | 7685 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      65 | 7686 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      34 | 7687 | `		}` |
|       - | 7688 | `		/* Dump key */` |
|      65 | 7689 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 7690 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 7691 | `		}else{` |
|      48 | 7692 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      15 | 7693 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 7694 | `		}` |
|       - | 7695 | `#ifdef __WINNT__` |
|       3 | 7696 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7697 | `#else` |
|      62 | 7698 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7699 | `#endif` |
|       - | 7700 | `		/* Dump node value */` |
|      65 | 7701 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      65 | 7702 | `		isRef = 0;` |
|      65 | 7703 | `		if( pObj ){` |
|      65 | 7704 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 7705 | `				/* Referenced object */` |
|     ! 0 | 7706 | `				isRef = 1;` |
|     ! 0 | 7707 | `			}` |
|      65 | 7708 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|      65 | 7709 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 7710 | `				break;` |
|       - | 7711 | `			}` |
|      31 | 7712 | `		}` |
|       - | 7713 | `		/* Point to the next entry */` |
|      65 | 7714 | `		n++;` |
|      65 | 7715 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       3 | 7716 | `	}` |
|      29 | 7717 | `	return rc;` |
|       3 | 7718 | `}` |
|      22 | 7719 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 7720 | `{` |
|       - | 7721 | `	sxi32 rc;` |
|       - | 7722 | `	int i;` |
|      24 | 7723 | `	if( nDepth > 31 ){` |
|       - | 7724 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 7725 | `		/* Nesting limit reached */` |
|     ! 0 | 7726 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 7727 | `		if( ShowType ){` |
|     ! 0 | 7728 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 7729 | `		}` |
|     ! 0 | 7730 | `		return SXERR_LIMIT;` |
|       - | 7731 | `	}` |
|      24 | 7732 | `	if( !ShowType ){` |
|      11 | 7733 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       5 | 7734 | `	}` |
|       - | 7735 | `	/* Total entries */` |
|      24 | 7736 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 7737 | `#ifdef __WINNT__` |
|       2 | 7738 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7739 | `#else` |
|      22 | 7740 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7741 | `#endif` |
|      24 | 7742 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|      46 | 7743 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      24 | 7744 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      13 | 7745 | `	}` |
|      24 | 7746 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      24 | 7747 | `	return rc;` |
|      13 | 7748 | `}` |
|       - | 7749 | `/*` |
|       - | 7750 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 7751 | ` * retrieved entry.` |
|       - | 7752 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 7753 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 7754 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 7755 | ` * a value different from PH7_OK.` |
|       - | 7756 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 7757 | ` */` |
|   32880 | 7758 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 7759 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 7760 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 7761 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 7762 | `	)` |
|       5 | 7763 | `{` |
|       - | 7764 | `	ph7_hashmap_node *pEntry;` |
|       - | 7765 | `	ph7_value sKey,sValue;` |
|       - | 7766 | `	sxi32 rc;` |
|       - | 7767 | `	sxu32 n;` |
|       - | 7768 | `	/* Initialize walker parameter */` |
|   32885 | 7769 | `	rc = SXRET_OK;` |
|   32885 | 7770 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   32885 | 7771 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   32885 | 7772 | `	n = pMap->nEntry;` |
|   32885 | 7773 | `	pEntry = pMap->pFirst;` |
|       - | 7774 | `	/* Start the iteration process */` |
|   83105 | 7775 | `	for(;;){` |
|  166215 | 7776 | `		if( n < 1 ){` |
|   32885 | 7777 | `			break;` |
|       - | 7778 | `		}` |
|       - | 7779 | `		/* Extract a copy of the key and a copy the current value */` |
|  133335 | 7780 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  133335 | 7781 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 7782 | `		/* Invoke the user callback */` |
|  133335 | 7783 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 7784 | `		/* Release the copy of the key and the value */` |
|  133335 | 7785 | `		PH7_MemObjRelease(&sKey);` |
|  133335 | 7786 | `		PH7_MemObjRelease(&sValue);` |
|  133335 | 7787 | `		if( rc != PH7_OK ){` |
|       - | 7788 | `			/* Callback request an operation abort */` |
|     ! 0 | 7789 | `			return SXERR_ABORT;` |
|       - | 7790 | `		}` |
|       - | 7791 | `		/* Point to the next entry */` |
|  133335 | 7792 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  133335 | 7793 | `		n--;` |
|       5 | 7794 | `	}` |
|       - | 7795 | `	/* All done */` |
|   32885 | 7796 | `	return SXRET_OK;` |
|   16445 | 7797 | `}` |
|       - | 7798 |  |
