# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2908/3334 lines (87.22%)

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
| 2869248 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2869250 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  249976 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  249978 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  249978 |   29 | `	sxu32 nH = 5381;` |
|  249978 |   30 | `	zEnd = &zIn[nLen];` |
|  283875 |   31 | `	for(;;){` |
|  567752 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  504334 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  454806 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  371882 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  249978 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     772 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       2 |   47 |  |
|     774 |   48 | `	sxi64 iCount = 0;` |
|     774 |   49 | `	if( !bRecursive ){` |
|     600 |   50 | `		iCount = pMap->nEntry;` |
|     301 |   51 | `	}else{` |
|       - |   52 | `		/* Recursive hashmap walk */` |
|     175 |   53 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|       - |   54 | `		ph7_value *pElem;` |
|     175 |   55 | `		sxu32 n = 0;` |
|       - |   56 | `		/* Mark this map as being counted */` |
|     175 |   57 | `		pMap->iFlags \|= HASHMAP_COUNTING;` |
|     209 |   58 | `		for(;;){` |
|     419 |   59 | `			if( n >= pMap->nEntry ){` |
|     175 |   60 | `				break;` |
|       - |   61 | `			}` |
|       - |   62 | `			/* Point to the element value */` |
|     245 |   63 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|     245 |   64 | `			if( pElem ){` |
|     245 |   65 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|     151 |   66 | `					ph7_hashmap *pSub = (ph7_hashmap *)pElem->x.pOther;` |
|     151 |   67 | `					if( pSub->iFlags & HASHMAP_COUNTING ){` |
|       - |   68 | `						/* Cycle detected — skip this entry */` |
|       3 |   69 | `						if( pCycleDetected ){` |
|       3 |   70 | `							*pCycleDetected = TRUE;` |
|       1 |   71 | `						}` |
|       2 |   72 | `					}else{` |
|     149 |   73 | `						iCount += HashmapCount(pSub,TRUE,pCycleDetected);` |
|       - |   74 | `					}` |
|      75 |   75 | `				}` |
|     122 |   76 | `			}` |
|       - |   77 | `			/* Point to the next entry */` |
|     245 |   78 | `			pEntry = pEntry->pNext;` |
|     245 |   79 | `			++n;` |
|       1 |   80 | `		}` |
|       - |   81 | `		/* Clear the counting flag */` |
|     175 |   82 | `		pMap->iFlags &= ~HASHMAP_COUNTING;` |
|       - |   83 | `		/* Update count */` |
|     175 |   84 | `		iCount += pMap->nEntry;` |
|       - |   85 | `	}` |
|     774 |   86 | `	return iCount;` |
|       2 |   87 |  |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 2813790 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 2813792 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2813792 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 2813792 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 2813792 |  104 | `	pNode->pMap  = &(*pMap);` |
| 2813792 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2813792 |  106 | `	pNode->nHash = nHash;` |
| 2813792 |  107 | `	pNode->xKey.iKey = iKey;` |
| 2813792 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 2813792 |  109 | `	return pNode;` |
| 1406897 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|   87488 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|   87490 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   87490 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|   87490 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|   87490 |  127 | `	pNode->pMap  = &(*pMap);` |
|   87490 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   87490 |  129 | `	pNode->nHash = nHash;` |
|   87490 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   87490 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   87490 |  132 | `	pNode->nValIdx = nValIdx;` |
|   87490 |  133 | `	return pNode;` |
|   43746 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 2901278 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  139 |  |
|       - |  140 | `	/* Link */` |
| 2901280 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2664944 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2664944 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1332471 |  144 | `	}` |
| 2901280 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 2901280 |  147 | `	if( pMap->pFirst == 0 ){` |
|   41024 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   41024 |  150 | `		pMap->pCur = pNode;` |
|   20513 |  151 | `	}else{` |
| 2860258 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 2901280 |  154 | `	++pMap->nEntry;` |
| 2901280 |  155 |  |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    5726 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  161 |  |
|    5728 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5728 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    5728 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    5312 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2657 |  167 | `	}else{` |
|     417 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    5728 |  170 | `	if( pNode->pNextCollide ){` |
|    4475 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2237 |  172 | `	}` |
|    5728 |  173 | `	if( pMap->pFirst == pNode ){` |
|      78 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      38 |  175 | `	}` |
|    5728 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|      80 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      39 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    5728 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5728 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     100 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     100 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     100 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      49 |  188 | `		}` |
|      49 |  189 | `	}` |
|    5728 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5609 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2804 |  192 | `	}` |
|    5728 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5728 |  194 | `	pMap->nEntry--;` |
|    5728 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      34 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      34 |  198 | `		pMap->apBucket = 0;` |
|      34 |  199 | `		pMap->nSize = 0;` |
|      34 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      16 |  201 | `	}` |
|    5728 |  202 |  |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 2901278 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  208 |  |
| 2901280 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   44926 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   44926 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   44926 |  215 | `		if( nNew < 1 ){` |
|   41024 |  216 | `			nNew = 16;` |
|   20511 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   44926 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   44926 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   44926 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   44926 |  230 | `		pMap->apBucket = apNew;` |
|   44926 |  231 | `		pMap->nSize = nNew;` |
|   44926 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   41024 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    3904 |  237 | `		pEntry = pMap->pFirst;` |
|    3904 |  238 | `		n = 0;` |
| 1975951 |  239 | `		for( ;; ){` |
| 3951904 |  240 | `			if( n >= pMap->nEntry ){` |
|    3904 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 3948002 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 3948002 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3948002 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3454520 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3454520 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1727259 |  250 | `			}` |
| 3948002 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 3948002 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3948002 |  254 | `			n++;` |
|       2 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    3904 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1951 |  258 | `	}` |
| 2860258 |  259 | `	return SXRET_OK;` |
| 1450641 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 2813790 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 2813792 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		/* Reserve a ph7_value for the value */` |
| 2813766 |  274 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2813766 |  275 | `		if( pObj == 0 ){` |
|     ! 0 |  276 | `			return SXERR_MEM;` |
|       - |  277 | `		}` |
| 2813766 |  278 | `		if( pValue ){` |
|       - |  279 | `			/* Duplicate the value */` |
| 2813766 |  280 | `			PH7_MemObjStore(pValue,pObj);` |
| 1406882 |  281 | `		}` |
| 2813766 |  282 | `		nIdx = pObj->nIdx;` |
| 1406884 |  283 | `	}else{` |
|      27 |  284 | `		nIdx = nRefIdx;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Hash the key */` |
| 2813792 |  287 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  288 | `	/* Allocate a new int node */` |
| 2813792 |  289 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2813792 |  290 | `	if( pNode == 0 ){` |
|     ! 0 |  291 | `		return SXERR_MEM;` |
|       - |  292 | `	}` |
| 2813792 |  293 | `	if( isForeign ){` |
|       - |  294 | `		/* Mark as a foregin entry */` |
|      27 |  295 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      13 |  296 | `	}` |
|       - |  297 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2813792 |  298 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2813792 |  299 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  300 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  301 | `		return rc;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Perform the insertion */` |
| 2813792 |  304 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  305 | `	/* Install in the reference table */` |
| 2813792 |  306 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  307 | `	/* All done */` |
| 2813792 |  308 | `	return SXRET_OK;` |
| 1406897 |  309 |  |
|       - |  310 | `/*` |
|       - |  311 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  312 | ` * hashmap.` |
|       - |  313 | ` */` |
|   87488 |  314 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  315 |  |
|       - |  316 | `	ph7_hashmap_node *pNode;` |
|       - |  317 | `	sxu32 nHash;` |
|       - |  318 | `	sxu32 nIdx;` |
|       - |  319 | `	sxi32 rc;` |
|   87490 |  320 | `	if( !isForeign ){` |
|       - |  321 | `		ph7_value *pObj;` |
|       - |  322 | `		/* Reserve a ph7_value for the value */` |
|   61962 |  323 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   61962 |  324 | `		if( pObj == 0 ){` |
|     ! 0 |  325 | `			return SXERR_MEM;` |
|       - |  326 | `		}` |
|   61962 |  327 | `		if( pValue ){` |
|       - |  328 | `			/* Duplicate the value */` |
|   61962 |  329 | `			PH7_MemObjStore(pValue,pObj);` |
|   30980 |  330 | `		}` |
|   61962 |  331 | `		nIdx = pObj->nIdx;` |
|   30982 |  332 | `	}else{` |
|   25530 |  333 | `		nIdx = nRefIdx;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Hash the key */` |
|   87490 |  336 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  337 | `	/* Allocate a new blob node */` |
|   87490 |  338 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   87490 |  339 | `	if( pNode == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|   87490 |  342 | `	if( isForeign ){` |
|       - |  343 | `		/* Mark as a foregin entry */` |
|   25530 |  344 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   12764 |  345 | `	}` |
|       - |  346 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   87490 |  347 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   87490 |  348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  349 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  350 | `		return rc;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Perform the insertion */` |
|   87490 |  353 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  354 | `	/* Install in the reference table */` |
|   87490 |  355 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  356 | `	/* All done */` |
|   87490 |  357 | `	return SXRET_OK;` |
|   43746 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  361 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  362 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  363 | ` */` |
|   47004 |  364 | `static sxi32 HashmapLookupIntKey(` |
|       - |  365 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  366 | `	sxi64 iKey,                /* lookup key */` |
|       - |  367 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  368 | `	)` |
|       2 |  369 |  |
|       - |  370 | `	ph7_hashmap_node *pNode;` |
|       - |  371 | `	sxu32 nHash;` |
|   47006 |  372 | `	if( pMap->nEntry < 1 ){` |
|       - |  373 | `		/* Don't bother hashing,there is no entry anyway */` |
|     400 |  374 | `		return SXERR_NOTFOUND;` |
|       - |  375 | `	}` |
|       - |  376 | `	/* Hash the key first */` |
|   46608 |  377 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  378 | `	/* Point to the appropriate bucket */` |
|   46608 |  379 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  380 | `	/* Perform the lookup */` |
|  411656 |  381 | `	for(;;){` |
|  823314 |  382 | `		if( pNode == 0 ){` |
|   45826 |  383 | `			break;` |
|       - |  384 | `		}` |
|  777879 |  385 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774474 |  386 | `			&& pNode->nHash == nHash` |
|  386123 |  387 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  388 | `				/* Node found */` |
|     784 |  389 | `				if( ppNode ){` |
|     772 |  390 | `					*ppNode = pNode;` |
|     385 |  391 | `				}` |
|     784 |  392 | `				return SXRET_OK;` |
|       - |  393 | `		}` |
|       - |  394 | `		/* Follow the collision link */` |
|  776707 |  395 | `		pNode = pNode->pNextCollide;` |
|       1 |  396 | `	}` |
|       - |  397 | `	/* No such entry */` |
|   45826 |  398 | `	return SXERR_NOTFOUND;` |
|   23504 |  399 |  |
|       - |  400 | `/*` |
|       - |  401 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  402 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  403 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  404 | ` */` |
|  172246 |  405 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  406 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  407 | `	const void *pKey,           /* Lookup key */` |
|       - |  408 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  409 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  410 | `	)` |
|       2 |  411 |  |
|       - |  412 | `	ph7_hashmap_node *pNode;` |
|       - |  413 | `	sxu32 nHash;` |
|  172248 |  414 | `	if( pMap->nEntry < 1 ){` |
|       - |  415 | `		/* Don't bother hashing,there is no entry anyway */` |
|    9760 |  416 | `		return SXERR_NOTFOUND;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Hash the key first */` |
|  162490 |  419 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  420 | `	/* Point to the appropriate bucket */` |
|  162490 |  421 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  422 | `	/* Perform the lookup */` |
|  156728 |  423 | `	for(;;){` |
|  313458 |  424 | `		if( pNode == 0 ){` |
|  123846 |  425 | `			break;` |
|       - |  426 | `		}` |
|  208934 |  427 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  188114 |  428 | `			&& pNode->nHash == nHash` |
|  112630 |  429 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   38646 |  430 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  431 | `				/* Node found */` |
|   38646 |  432 | `				if( ppNode ){` |
|   38618 |  433 | `					*ppNode = pNode;` |
|   19308 |  434 | `				}` |
|   38646 |  435 | `				return SXRET_OK;` |
|       - |  436 | `		}` |
|       - |  437 | `		/* Follow the collision link */` |
|  150970 |  438 | `		pNode = pNode->pNextCollide;` |
|       2 |  439 | `	}` |
|       - |  440 | `	/* No such entry */` |
|  123846 |  441 | `	return SXERR_NOTFOUND;` |
|   86125 |  442 |  |
|       - |  443 | `/*` |
|       - |  444 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  445 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  446 | ` */` |
|  172388 |  447 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  448 |  |
|  172390 |  449 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  172390 |  450 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  172390 |  451 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  452 | `		/* Octal not decimal number */` |
|       5 |  453 | `		return FALSE;` |
|       - |  454 | `	}` |
|  172386 |  455 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  456 | `		zIn++;` |
|     ! 0 |  457 | `	}` |
|   86525 |  458 | `	for(;;){` |
|  173052 |  459 | `		if( zIn >= zEnd ){` |
|     233 |  460 | `			return TRUE;` |
|       - |  461 | `		}` |
|  172820 |  462 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   86078 |  463 | `			break;` |
|       - |  464 | `		}` |
|     667 |  465 | `		zIn++;` |
|       1 |  466 | `	}` |
|       - |  467 | `	/* Key does not look like a decimal number */` |
|  172154 |  468 | `	return FALSE;` |
|   86196 |  469 |  |
|       - |  470 | `/*` |
|       - |  471 | ` * Check if a given key exists in the given hashmap.` |
|       - |  472 | ` * Write a pointer to the target node on success.` |
|       - |  473 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  474 | ` */` |
|   85428 |  475 | `static sxi32 HashmapLookup(` |
|       - |  476 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  477 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  478 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  479 | `	)` |
|       2 |  480 |  |
|   85430 |  481 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  482 | `	sxi32 rc;` |
|   85430 |  483 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   84740 |  484 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  485 | `			/* Force a string cast */` |
|     ! 0 |  486 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  487 | `		}` |
|   84740 |  488 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  489 | `			/* Perform a blob lookup */` |
|   84724 |  490 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   84724 |  491 | `			goto result;` |
|       - |  492 | `		}` |
|       8 |  493 | `	}` |
|       - |  494 | `	/* Perform an int lookup */` |
|     708 |  495 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  496 | `		/* Force an integer cast */` |
|      27 |  497 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  498 | `	}` |
|       - |  499 | `	/* Perform an int lookup */` |
|     708 |  500 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   42714 |  501 | `result:` |
|   85430 |  502 | `	if( rc == SXRET_OK ){` |
|       - |  503 | `		/* Node found */` |
|   39246 |  504 | `		if( ppNode ){` |
|   39212 |  505 | `			*ppNode = pNode;` |
|   19605 |  506 | `		}` |
|   39246 |  507 | `		return SXRET_OK;` |
|       - |  508 | `	}` |
|       - |  509 | `	/* No such entry */` |
|   46186 |  510 | `	return SXERR_NOTFOUND;` |
|   42716 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  514 | ` * hashmap.` |
|       - |  515 | ` * If a node with the given key already exists in the database` |
|       - |  516 | ` * then this function overwrite the old value.` |
|       - |  517 | ` */` |
| 2875528 |  518 | `static sxi32 HashmapInsert(` |
|       - |  519 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  520 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  521 | `	ph7_value *pVal    /* Node value */` |
|       - |  522 | `	)` |
|       2 |  523 |  |
| 2875530 |  524 | `	ph7_hashmap_node *pNode = 0;` |
| 2875530 |  525 | `	sxi32 rc = SXRET_OK;` |
| 2875530 |  526 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   62156 |  527 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  528 | `			/* Force a string cast */` |
|       3 |  529 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  530 | `		}` |
|   62156 |  531 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  532 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  533 | `				/* Automatic index assign */` |
|      34 |  534 | `				pKey = 0;` |
|      16 |  535 | `			}` |
|     256 |  536 | `			goto IntKey;` |
|       - |  537 | `		}` |
|   92852 |  538 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   30950 |  539 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  540 | `				/* Overwrite the old value */` |
|       - |  541 | `				ph7_value *pElem;` |
|      37 |  542 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      37 |  543 | `				if( pElem ){` |
|      37 |  544 | `					if( pVal ){` |
|      37 |  545 | `						PH7_MemObjStore(pVal,pElem);` |
|      19 |  546 | `					}else{` |
|       - |  547 | `						/* Nullify the entry */` |
|     ! 0 |  548 | `						PH7_MemObjToNull(pElem);` |
|       - |  549 | `					}` |
|      18 |  550 | `				}` |
|      37 |  551 | `				return SXRET_OK;` |
|       - |  552 | `		}` |
|   61866 |  553 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  554 | `			/* Forbidden */` |
|       3 |  555 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Perform a blob-key insertion */` |
|   61864 |  559 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   61864 |  560 | `		return rc;` |
|       - |  561 | `	}` |
| 1406687 |  562 | `IntKey:` |
| 2813630 |  563 | `	if( pKey ){` |
|   23264 |  564 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  565 | `			/* Force an integer cast */` |
|     251 |  566 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  567 | `		}` |
|   23264 |  568 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  569 | `			/* Overwrite the old value */` |
|       - |  570 | `			ph7_value *pElem;` |
|      47 |  571 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      47 |  572 | `			if( pElem ){` |
|      47 |  573 | `				if( pVal ){` |
|      47 |  574 | `					PH7_MemObjStore(pVal,pElem);` |
|      24 |  575 | `				}else{` |
|       - |  576 | `					/* Nullify the entry */` |
|     ! 0 |  577 | `					PH7_MemObjToNull(pElem);` |
|       - |  578 | `				}` |
|      23 |  579 | `			}` |
|      47 |  580 | `			return SXRET_OK;` |
|       - |  581 | `		}` |
|   23218 |  582 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  583 | `			/* Forbidden */` |
|       3 |  584 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  585 | `			return SXRET_OK;` |
|       - |  586 | `		}` |
|       - |  587 | `		/* Perform a 64-bit-int-key insertion */` |
|   23216 |  588 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23216 |  589 | `		if( rc == SXRET_OK ){` |
|   23216 |  590 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  591 | `				/* Increment the automatic index */` |
|   22980 |  592 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  593 | `				/* Make sure the automatic index is not reserved */` |
|   22980 |  594 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  595 | `					pMap->iNextIdx++;` |
|     ! 0 |  596 | `				}` |
|   11489 |  597 | `			}` |
|   11607 |  598 | `		}` |
|   11609 |  599 | `	}else{` |
| 2790368 |  600 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  601 | `			/* Forbidden */` |
|       3 |  602 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|       - |  605 | `		/* Assign an automatic index */` |
| 2790366 |  606 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2790366 |  607 | `		if( rc == SXRET_OK ){` |
| 2790366 |  608 | `			++pMap->iNextIdx;` |
| 1395182 |  609 | `		}` |
|       - |  610 | `	}` |
|       - |  611 | `	/* Insertion result */` |
| 2813580 |  612 | `	return rc;` |
| 1437766 |  613 |  |
|       - |  614 | `/*` |
|       - |  615 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - |  616 | ` * hashmap.` |
|       - |  617 | ` * This is insertion by reference so be careful to mark the node` |
|       - |  618 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - |  619 | ` * The insertion by reference is triggered when the following` |
|       - |  620 | ` * expression is encountered.` |
|       - |  621 | ` * $var = 10;` |
|       - |  622 | ` *  $a = array(&var);` |
|       - |  623 | ` * OR` |
|       - |  624 | ` *  $a[] =& $var;` |
|       - |  625 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - |  626 | ` * over it's contents.` |
|       - |  627 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - |  628 | ` * removed when the foreign ph7_value is unset.` |
|       - |  629 | ` * Example:` |
|       - |  630 | ` *  $var = 10;` |
|       - |  631 | ` *  $a[] =& $var;` |
|       - |  632 | ` *  echo count($a).PHP_EOL; //1` |
|       - |  633 | ` *  //Unset the foreign ph7_value now` |
|       - |  634 | ` *  unset($var);` |
|       - |  635 | ` *  echo count($a); //0` |
|       - |  636 | ` * Note that this is a PH7 eXtension.` |
|       - |  637 | ` * Refer to the official documentation for more information.` |
|       - |  638 | ` * If a node with the given key already exists in the database` |
|       - |  639 | ` * then this function overwrite the old value.` |
|       - |  640 | ` */` |
|   25560 |  641 | `static sxi32 HashmapInsertByRef(` |
|       - |  642 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  643 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  644 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  645 | `	)` |
|       2 |  646 |  |
|   25562 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|   25562 |  648 | `	sxi32 rc = SXRET_OK;` |
|   25562 |  649 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   25536 |  650 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  651 | `			/* Force a string cast */` |
|     ! 0 |  652 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  653 | `		}` |
|   25536 |  654 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  655 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  656 | `				/* Automatic index assign */` |
|     ! 0 |  657 | `				pKey = 0;` |
|     ! 0 |  658 | `			}` |
|     ! 0 |  659 | `			goto IntKey;` |
|       - |  660 | `		}` |
|   38303 |  661 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   12767 |  662 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  663 | `				/* Overwrite */` |
|       7 |  664 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  665 | `				pNode->nValIdx = nRefIdx;` |
|       - |  666 | `				/* Install in the reference table */` |
|       7 |  667 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  668 | `				return SXRET_OK;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Perform a blob-key insertion */` |
|   25530 |  671 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   25530 |  672 | `		return rc;` |
|       - |  673 | `	}` |
|      13 |  674 | `IntKey:` |
|      27 |  675 | `	if( pKey ){` |
|       3 |  676 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  677 | `			/* Force an integer cast */` |
|     ! 0 |  678 | `			PH7_MemObjToInteger(pKey);` |
|     ! 0 |  679 | `		}` |
|       3 |  680 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  681 | `			/* Overwrite */` |
|     ! 0 |  682 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|     ! 0 |  683 | `			pNode->nValIdx = nRefIdx;` |
|       - |  684 | `			/* Install in the reference table */` |
|     ! 0 |  685 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|     ! 0 |  686 | `			return SXRET_OK;` |
|       - |  687 | `		}` |
|       - |  688 | `		/* Perform a 64-bit-int-key insertion */` |
|       3 |  689 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|       3 |  690 | `		if( rc == SXRET_OK ){` |
|       3 |  691 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  692 | `				/* Increment the automatic index */` |
|       3 |  693 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  694 | `				/* Make sure the automatic index is not reserved */` |
|       3 |  695 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  696 | `					pMap->iNextIdx++;` |
|     ! 0 |  697 | `				}` |
|       1 |  698 | `			}` |
|       1 |  699 | `		}` |
|       2 |  700 | `	}else{` |
|       - |  701 | `		/* Assign an automatic index */` |
|      25 |  702 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|      25 |  703 | `		if( rc == SXRET_OK ){` |
|      25 |  704 | `			++pMap->iNextIdx;` |
|      12 |  705 | `		}` |
|       - |  706 | `	}` |
|       - |  707 | `	/* Insertion result */` |
|      27 |  708 | `	return rc;` |
|   12782 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * Extract node value.` |
|       - |  712 | ` */` |
|  950814 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
|  950816 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  950816 |  718 | `	return pObj;` |
|       2 |  719 |  |
|       - |  720 | `/*` |
|       - |  721 | ` * Insert a node in the given hashmap.` |
|       - |  722 | ` * If a node with the given key already exists in the database` |
|       - |  723 | ` * then this function overwrite the old value.` |
|       - |  724 | ` */` |
|     422 |  725 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       1 |  726 |  |
|       - |  727 | `	ph7_value *pObj;` |
|       - |  728 | `	sxi32 rc;` |
|       - |  729 | `	/* Extract the node value */` |
|     423 |  730 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     423 |  731 | `	if( pObj == 0 ){` |
|     ! 0 |  732 | `		return SXERR_EMPTY;` |
|       - |  733 | `	}` |
|       - |  734 | `	/* Preserve key */` |
|     423 |  735 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  736 | `		/* Int64 key */` |
|     293 |  737 | `		if( !bPreserve ){` |
|       - |  738 | `			/* Assign an automatic index */` |
|     149 |  739 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      75 |  740 | `		}else{` |
|     145 |  741 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  742 | `		}` |
|     147 |  743 | `	}else{` |
|       - |  744 | `		/* Blob key */` |
|     131 |  745 | `		if( !bPreserve ){` |
|       - |  746 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  747 | `			 * original string key entirely */` |
|      35 |  748 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      18 |  749 | `		}else{` |
|     145 |  750 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      48 |  751 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  752 | `		}` |
|       - |  753 | `	}` |
|     423 |  754 | `	return rc;` |
|     212 |  755 |  |
|       - |  756 | `/*` |
|       - |  757 | ` * Compare two node values.` |
|       - |  758 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  759 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  760 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  761 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  762 | ` * documenation.` |
|       - |  763 | ` */` |
|   40072 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   40074 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   40074 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   40074 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   40074 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   40074 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   40074 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   40074 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   40074 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   40074 |  783 | `	return rc;` |
|   20058 |  784 |  |
|       - |  785 | `/*` |
|       - |  786 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  787 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  788 | ` */` |
|    8852 |  789 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  790 |  |
|    8854 |  791 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  792 | `	sxu32 nBucket;` |
|       - |  793 | `	/* Remove old collision links */` |
|    8854 |  794 | `	if( pEntry->pPrevCollide ){` |
|    6918 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    3453 |  796 | `	}else{` |
|    1938 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  798 | `	}` |
|    8854 |  799 | `	if( pEntry->pNextCollide ){` |
|     673 |  800 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     335 |  801 | `	}` |
|    8854 |  802 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  803 | `	/* Compute the new hash */` |
|    8854 |  804 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    8854 |  805 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    8854 |  806 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  807 | `	/* Link to the new bucket */` |
|    8854 |  808 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    8854 |  809 | `	if( pMap->apBucket[nBucket] ){` |
|    7089 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3540 |  811 | `	}` |
|    8854 |  812 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    8854 |  813 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  814 | `	/* Increment the automatic index */` |
|    8854 |  815 | `	pMap->iNextIdx++;` |
|    8854 |  816 |  |
|       - |  817 | `/*` |
|       - |  818 | ` * Perform a linear search on a given hashmap.` |
|       - |  819 | ` * Write a pointer to the target node on success.` |
|       - |  820 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  821 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  822 | ` * for more information.` |
|       - |  823 | ` */` |
|   22346 |  824 | `static int HashmapFindValue(` |
|       - |  825 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  826 | `	ph7_value *pNeedle,  /* Lookup key */` |
|       - |  827 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|       - |  828 | `	int bStrict      /* TRUE for strict comparison */` |
|       - |  829 | `	)` |
|       2 |  830 |  |
|       - |  831 | `	ph7_hashmap_node *pEntry;` |
|       - |  832 | `	ph7_value sVal,*pVal;` |
|       - |  833 | `	ph7_value sNeedle;` |
|       - |  834 | `	sxi32 rc;` |
|       - |  835 | `	sxu32 n;` |
|       - |  836 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|   22348 |  837 | `	pEntry = pMap->pFirst;` |
|   22348 |  838 | `	n = pMap->nEntry;` |
|   22348 |  839 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   22348 |  840 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   53564 |  841 | `	for(;;){` |
|  107130 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  107032 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  107032 |  847 | `		if( pVal ){` |
|  107032 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|     ! 0 |  849 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  850 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  851 | `				if( iF1 == iF2 ){` |
|       - |  852 | `					/* NULL values are equals */` |
|     ! 0 |  853 | `					if( ppNode ){` |
|     ! 0 |  854 | `						*ppNode = pEntry;` |
|     ! 0 |  855 | `					}` |
|     ! 0 |  856 | `					return SXRET_OK;` |
|       - |  857 | `				}` |
|     ! 0 |  858 | `			}else{` |
|       - |  859 | `				/* Duplicate value */` |
|  107032 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  107032 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  107032 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  107032 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  107032 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  107032 |  865 | `				if( rc == 0 ){` |
|   22250 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   22250 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   42391 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|   84784 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   84784 |  876 | `		n--;` |
|       2 |  877 | `	}` |
|       - |  878 | `	/* No such entry */` |
|      99 |  879 | `	return SXERR_NOTFOUND;` |
|   11175 |  880 |  |
|       - |  881 | `/*` |
|       - |  882 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  883 | ` * for values comparison.` |
|       - |  884 | ` * Write a pointer to the target node on success.` |
|       - |  885 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  886 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  887 | ` * for more information.` |
|       - |  888 | ` */` |
|      18 |  889 | `static int HashmapFindValueByCallback(` |
|       - |  890 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|       - |  891 | `	ph7_value *pNeedle,    /* Lookup key */` |
|       - |  892 | `	ph7_value *pCallback,  /* User defined callback */` |
|       - |  893 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|       - |  894 | `	)` |
|       1 |  895 |  |
|       - |  896 | `	ph7_hashmap_node *pEntry;` |
|       - |  897 | `	ph7_value sResult,*pVal;` |
|       - |  898 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|       - |  899 | `	sxi32 rc;` |
|       - |  900 | `	sxu32 n;` |
|       - |  901 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|      19 |  902 | `	pEntry = pMap->pFirst;` |
|      19 |  903 | `	n = pMap->nEntry;` |
|       - |  904 | `	/* Store callback result here */` |
|      19 |  905 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  906 | `	/* First argument to the callback */` |
|      19 |  907 | `	apArg[0] = pNeedle;` |
|      23 |  908 | `	for(;;){` |
|      47 |  909 | `		if( n < 1 ){` |
|       9 |  910 | `			break;` |
|       - |  911 | `		}` |
|       - |  912 | `		/* Extract node value */` |
|      39 |  913 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 |  914 | `		if( pVal ){` |
|       - |  915 | `			/* Invoke the user callback */` |
|      39 |  916 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      39 |  917 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      39 |  918 | `			if( rc == SXRET_OK ){` |
|       - |  919 | `				/* Extract callback result */` |
|      39 |  920 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  921 | `					/* Perform an int cast */` |
|     ! 0 |  922 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  923 | `				}` |
|      39 |  924 | `				rc = (sxi32)sResult.x.iVal;` |
|      39 |  925 | `				PH7_MemObjRelease(&sResult);` |
|      39 |  926 | `				if( rc == 0 ){` |
|       - |  927 | `					/* Match found*/` |
|      11 |  928 | `					if( ppNode ){` |
|     ! 0 |  929 | `						*ppNode = pEntry;` |
|     ! 0 |  930 | `					}` |
|      11 |  931 | `					return SXRET_OK;` |
|       - |  932 | `				}` |
|      14 |  933 | `			}` |
|      14 |  934 | `		}` |
|       - |  935 | `		/* Point to the next entry */` |
|      29 |  936 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 |  937 | `		n--;` |
|       1 |  938 | `	}` |
|       - |  939 | `	/* No such entry */` |
|       9 |  940 | `	return SXERR_NOTFOUND;` |
|      10 |  941 |  |
|       - |  942 | `/*` |
|       - |  943 | ` * Compare two hashmaps.` |
|       - |  944 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|       - |  945 | ` * Note on array comparison operators.` |
|       - |  946 | ` *  According to the PHP language reference manual.` |
|       - |  947 | ` *  Array Operators Example 	Name 	Result` |
|       - |  948 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|       - |  949 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|       - |  950 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|       - |  951 | ` *                          order and of the same types.` |
|       - |  952 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - |  953 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - |  954 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|       - |  955 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - |  956 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - |  957 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - |  958 | ` * <?php` |
|       - |  959 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - |  960 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - |  961 | ` * $c = $a + $b; // Union of $a and $b` |
|       - |  962 | ` * echo "Union of \$a and \$b: \n";` |
|       - |  963 | ` * var_dump($c);` |
|       - |  964 | ` * $c = $b + $a; // Union of $b and $a` |
|       - |  965 | ` * echo "Union of \$b and \$a: \n";` |
|       - |  966 | ` * var_dump($c);` |
|       - |  967 | ` * ?>` |
|       - |  968 | ` * When executed, this script will print the following:` |
|       - |  969 | ` * Union of $a and $b:` |
|       - |  970 | ` * array(3) {` |
|       - |  971 | ` *  ["a"]=>` |
|       - |  972 | ` *  string(5) "apple"` |
|       - |  973 | ` *  ["b"]=>` |
|       - |  974 | ` * string(6) "banana"` |
|       - |  975 | ` *  ["c"]=>` |
|       - |  976 | ` * string(6) "cherry"` |
|       - |  977 | ` * }` |
|       - |  978 | ` * Union of $b and $a:` |
|       - |  979 | ` * array(3) {` |
|       - |  980 | ` * ["a"]=>` |
|       - |  981 | ` * string(4) "pear"` |
|       - |  982 | ` * ["b"]=>` |
|       - |  983 | ` * string(10) "strawberry"` |
|       - |  984 | ` * ["c"]=>` |
|       - |  985 | ` * string(6) "cherry"` |
|       - |  986 | ` * }` |
|       - |  987 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|       - |  988 | ` */` |
|       8 |  989 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - |  990 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - |  991 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - |  992 | `	int bStrict          /* TRUE for strict comparison */` |
|       - |  993 | `	)` |
|       1 |  994 |  |
|       - |  995 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - |  996 | `	sxi32 rc;` |
|       - |  997 | `	sxu32 n;` |
|       9 |  998 | `	if( pLeft == pRight ){` |
|       - |  999 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - | 1000 | `		 * Unlike the zend engine.` |
|       - | 1001 | `		 */` |
|     ! 0 | 1002 | `		return 0;` |
|       - | 1003 | `	}` |
|       9 | 1004 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - | 1005 | `		/* Must have the same number of entries */` |
|     ! 0 | 1006 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1007 | `	}` |
|       - | 1008 | `	/* Point to the first inserted entry of the left hashmap */` |
|       9 | 1009 | `	pLe = pLeft->pFirst;` |
|       9 | 1010 | `	pRe = 0; /* cc warning */` |
|       - | 1011 | `	/* Perform the comparison */` |
|       9 | 1012 | `	n = pLeft->nEntry;` |
|       8 | 1013 | `	for(;;){` |
|      17 | 1014 | `		if( n < 1 ){` |
|       7 | 1015 | `			break;` |
|       - | 1016 | `		}` |
|      11 | 1017 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1018 | `			/* Int key */` |
|       7 | 1019 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|       4 | 1020 | `		}else{` |
|       5 | 1021 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1022 | `			/* Blob key */` |
|       5 | 1023 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1024 | `		}` |
|      11 | 1025 | `		if( rc != SXRET_OK ){` |
|       - | 1026 | `			/* No such entry in the right side */` |
|     ! 0 | 1027 | `			return 1;` |
|       - | 1028 | `		}` |
|      11 | 1029 | `		rc = 0;` |
|      11 | 1030 | `		if( bStrict ){` |
|       - | 1031 | `			/* Make sure,the keys are of the same type */` |
|       3 | 1032 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1033 | `				rc = 1;` |
|     ! 0 | 1034 | `			}` |
|       1 | 1035 | `		}` |
|      11 | 1036 | `		if( !rc ){` |
|       - | 1037 | `			/* Compare nodes */` |
|      11 | 1038 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|       5 | 1039 | `		}` |
|      11 | 1040 | `		if( rc != 0 ){` |
|       - | 1041 | `			/* Nodes key/value differ */` |
|       3 | 1042 | `			return rc;` |
|       - | 1043 | `		}` |
|       - | 1044 | `		/* Point to the next entry */` |
|       9 | 1045 | `		pLe = pLe->pPrev; /* Reverse link */` |
|       9 | 1046 | `		n--;` |
|       1 | 1047 | `	}` |
|       7 | 1048 | `	return 0; /* Hashmaps are equals */` |
|       5 | 1049 |  |
|       - | 1050 | `/*` |
|       - | 1051 | ` * Duplicate a hashmap node.` |
|       - | 1052 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1053 | ` */` |
|  471010 | 1054 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1055 | `	ph7_hashmap *pDest,` |
|       - | 1056 | `	ph7_hashmap_node *pEntry,` |
|       - | 1057 | `	ph7_value *pVal,` |
|       - | 1058 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1059 | `	)` |
|       2 | 1060 |  |
|  471012 | 1061 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1062 | `	ph7_value sKey;` |
|       - | 1063 | `	sxi32 rc;` |
|       - | 1064 |  |
|  471012 | 1065 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1066 | `		/* Blob key insertion */` |
|      41 | 1067 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      41 | 1068 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      41 | 1069 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      41 | 1070 | `		PH7_MemObjRelease(&sKey);` |
|      21 | 1071 | `	}else{` |
|       - | 1072 | `		/* Int key */` |
|  470972 | 1073 | `		if( iAction == 0 ){ /* Merge */` |
|  470900 | 1074 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  235523 | 1075 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1076 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1077 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1078 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1079 | `		}else{ /* Dup */` |
|      44 | 1080 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1081 | `		}` |
|       - | 1082 | `	}` |
|  471012 | 1083 | `	return rc;` |
|       2 | 1084 |  |
|       - | 1085 | `/*` |
|       - | 1086 | ` * Merge two hashmaps.` |
|       - | 1087 | ` * Note on the merge process` |
|       - | 1088 | ` * According to the PHP language reference manual.` |
|       - | 1089 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|       - | 1090 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|       - | 1091 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|       - | 1092 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|       - | 1093 | ` *  the later value will not overwrite the original value, but will be appended.` |
|       - | 1094 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|       - | 1095 | ` *  keys starting from zero in the result array.` |
|       - | 1096 | ` */` |
|    1784 | 1097 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1098 |  |
|       - | 1099 | `	ph7_hashmap_node *pEntry;` |
|       - | 1100 | `	ph7_value *pVal;` |
|       - | 1101 | `	sxi32 rc;` |
|       - | 1102 | `	sxu32 n;` |
|    1786 | 1103 | `	if( pSrc == pDest ){` |
|       - | 1104 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1105 | `		 * Unlike the zend engine.` |
|       - | 1106 | `		 */` |
|     ! 0 | 1107 | `		return SXRET_OK;` |
|       - | 1108 | `	}` |
|       - | 1109 | `	/* Point to the first inserted entry in the source */` |
|    1786 | 1110 | `	pEntry = pSrc->pFirst;` |
|       - | 1111 | `	/* Perform the merge */` |
|  472700 | 1112 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1113 | `		/* Extract the node value */` |
|  470916 | 1114 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  470916 | 1115 | `		if( pVal ){` |
|       - | 1116 | `			/* Make a local copy of the value.` |
|       - | 1117 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1118 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1119 | `			 * to the old pool.` |
|       - | 1120 | `			 */` |
|  470916 | 1121 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  235459 | 1122 | `		}else{` |
|     ! 0 | 1123 | `			rc = SXRET_OK;` |
|       - | 1124 | `		}` |
|  470916 | 1125 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1126 | `			return rc;` |
|       - | 1127 | `		}` |
|       - | 1128 | `		/* Point to the next entry */` |
|  470916 | 1129 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  235459 | 1130 | `	}` |
|    1786 | 1131 | `	return SXRET_OK;` |
|     894 | 1132 |  |
|       - | 1133 | `/*` |
|       - | 1134 | ` * Overwrite entries with the same key.` |
|       - | 1135 | ` * Refer to the [array_replace()] implementation for more information.` |
|       - | 1136 | ` *  According to the PHP language reference manual.` |
|       - | 1137 | ` *  array_replace() replaces the values of the first array with the same values` |
|       - | 1138 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|       - | 1139 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|       - | 1140 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|       - | 1141 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|       - | 1142 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|       - | 1143 | ` *  overwriting the previous values.` |
|       - | 1144 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|       - | 1145 | ` *  by whatever type is in the second array.` |
|       - | 1146 | ` */` |
|      34 | 1147 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1148 |  |
|       - | 1149 | `	ph7_hashmap_node *pEntry;` |
|       - | 1150 | `	ph7_value *pVal;` |
|       - | 1151 | `	sxi32 rc;` |
|       - | 1152 | `	sxu32 n;` |
|      36 | 1153 | `	if( pSrc == pDest ){` |
|       - | 1154 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1155 | `		 * Unlike the zend engine.` |
|       - | 1156 | `		 */` |
|     ! 0 | 1157 | `		return SXRET_OK;` |
|       - | 1158 | `	}` |
|       - | 1159 | `	/* Point to the first inserted entry in the source */` |
|      36 | 1160 | `	pEntry = pSrc->pFirst;` |
|       - | 1161 | `	/* Perform the merge */` |
|      80 | 1162 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1163 | `		/* Extract the node value */` |
|      46 | 1164 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      46 | 1165 | `		if( pVal ){` |
|      46 | 1166 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      24 | 1167 | `		}else{` |
|     ! 0 | 1168 | `			rc = SXRET_OK;` |
|       - | 1169 | `		}` |
|      46 | 1170 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1171 | `			return rc;` |
|       - | 1172 | `		}` |
|       - | 1173 | `		/* Point to the next entry */` |
|      46 | 1174 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 1175 | `	}` |
|      36 | 1176 | `	return SXRET_OK;` |
|      19 | 1177 |  |
|       - | 1178 | `/*` |
|       - | 1179 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|       - | 1180 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|       - | 1181 | ` */` |
|      30 | 1182 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1183 |  |
|       - | 1184 | `	ph7_hashmap_node *pEntry;` |
|       - | 1185 | `	ph7_value *pVal;` |
|       - | 1186 | `	sxi32 rc;` |
|       - | 1187 | `	sxu32 n;` |
|      32 | 1188 | `	if( pSrc == pDest ){` |
|       - | 1189 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1190 | `		 * Unlike the zend engine.` |
|       - | 1191 | `		 */` |
|     ! 0 | 1192 | `		return SXRET_OK;` |
|       - | 1193 | `	}` |
|       - | 1194 | `	/* Point to the first inserted entry in the source */` |
|      32 | 1195 | `	pEntry = pSrc->pFirst;` |
|       - | 1196 | `	/* Perform the duplication */` |
|      84 | 1197 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1198 | `		/* Extract the node value */` |
|      54 | 1199 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      54 | 1200 | `		if( pVal ){` |
|      54 | 1201 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      28 | 1202 | `		}else{` |
|     ! 0 | 1203 | `			rc = SXRET_OK;` |
|       - | 1204 | `		}` |
|      54 | 1205 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1206 | `			return rc;` |
|       - | 1207 | `		}` |
|       - | 1208 | `		/* Point to the next entry */` |
|      54 | 1209 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      28 | 1210 | `	}` |
|      32 | 1211 | `	return SXRET_OK;` |
|      17 | 1212 |  |
|       - | 1213 | `/*` |
|       - | 1214 | ` * Perform the union of two hashmaps.` |
|       - | 1215 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1216 | ` * with a variable holding an array as follows:` |
|       - | 1217 | ` * <?php` |
|       - | 1218 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1219 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1220 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1221 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1222 | ` * var_dump($c);` |
|       - | 1223 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1224 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1225 | ` * var_dump($c);` |
|       - | 1226 | ` * ?>` |
|       - | 1227 | ` * When executed, this script will print the following:` |
|       - | 1228 | ` * Union of $a and $b:` |
|       - | 1229 | ` * array(3) {` |
|       - | 1230 | ` *  ["a"]=>` |
|       - | 1231 | ` *  string(5) "apple"` |
|       - | 1232 | ` *  ["b"]=>` |
|       - | 1233 | ` * string(6) "banana"` |
|       - | 1234 | ` *  ["c"]=>` |
|       - | 1235 | ` * string(6) "cherry"` |
|       - | 1236 | ` * }` |
|       - | 1237 | ` * Union of $b and $a:` |
|       - | 1238 | ` * array(3) {` |
|       - | 1239 | ` * ["a"]=>` |
|       - | 1240 | ` * string(4) "pear"` |
|       - | 1241 | ` * ["b"]=>` |
|       - | 1242 | ` * string(10) "strawberry"` |
|       - | 1243 | ` * ["c"]=>` |
|       - | 1244 | ` * string(6) "cherry"` |
|       - | 1245 | ` * }` |
|       - | 1246 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1247 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1248 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1249 | ` */` |
|       4 | 1250 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       2 | 1251 |  |
|       - | 1252 | `	ph7_hashmap_node *pEntry;` |
|       6 | 1253 | `	sxi32 rc = SXRET_OK;` |
|       - | 1254 | `	ph7_value *pObj;` |
|       - | 1255 | `	sxu32 n;` |
|       6 | 1256 | `	if( pLeft == pRight ){` |
|       - | 1257 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1258 | `		 * Unlike the zend engine.` |
|       - | 1259 | `		 */` |
|     ! 0 | 1260 | `		return SXRET_OK;` |
|       - | 1261 | `	}` |
|       - | 1262 | `	/* Perform the union */` |
|       6 | 1263 | `	pEntry = pRight->pFirst;` |
|      16 | 1264 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1265 | `		/* Make sure the given key does not exists in the left array */` |
|      12 | 1266 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1267 | `			/* BLOB key */` |
|       7 | 1268 | `			if( SXRET_OK !=` |
|       6 | 1269 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|       3 | 1270 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|       3 | 1271 | `					if( pObj ){` |
|       3 | 1272 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1273 | `						/* Perform the insertion */` |
|       3 | 1274 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1275 | `							&sSafeVal,0,FALSE);` |
|       3 | 1276 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1277 | `							return rc;` |
|       - | 1278 | `						}` |
|       1 | 1279 | `					}` |
|       1 | 1280 | `			}` |
|       4 | 1281 | `		}else{` |
|       - | 1282 | `			/* INT key */` |
|       5 | 1283 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|     ! 0 | 1284 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 1285 | `				if( pObj ){` |
|     ! 0 | 1286 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1287 | `					/* Perform the insertion */` |
|     ! 0 | 1288 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|     ! 0 | 1289 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1290 | `						return rc;` |
|       - | 1291 | `					}` |
|     ! 0 | 1292 | `				}` |
|     ! 0 | 1293 | `			}` |
|       - | 1294 | `		}` |
|       - | 1295 | `		/* Point to the next entry */` |
|      12 | 1296 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 1297 | `	}` |
|       6 | 1298 | `	return SXRET_OK;` |
|       4 | 1299 |  |
|       - | 1300 | `/*` |
|       - | 1301 | ` * Allocate a new hashmap.` |
|       - | 1302 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1303 | ` */` |
|   63450 | 1304 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1305 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1306 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1307 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1308 | `	)` |
|       2 | 1309 |  |
|       - | 1310 | `	ph7_hashmap *pMap;` |
|       - | 1311 | `	/* Allocate a new instance */` |
|   63452 | 1312 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   63452 | 1313 | `	if( pMap == 0 ){` |
|     ! 0 | 1314 | `		return 0;` |
|       - | 1315 | `	}` |
|       - | 1316 | `	/* Zero the structure */` |
|   63452 | 1317 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1318 | `	/* Fill in the structure */` |
|   63452 | 1319 | `	pMap->pVm = &(*pVm);` |
|   63452 | 1320 | `	pMap->iRef = 1;` |
|       - | 1321 | `	/* Default hash functions */` |
|   63452 | 1322 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   63452 | 1323 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   63452 | 1324 | `	return pMap;` |
|   31727 | 1325 |  |
|       - | 1326 | `/*` |
|       - | 1327 | ` * Install superglobals in the given virtual machine.` |
|       - | 1328 | ` * Note on superglobals.` |
|       - | 1329 | ` *  According to the PHP language reference manual.` |
|       - | 1330 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1331 | `*   Description` |
|       - | 1332 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1333 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1334 | `*   global $variable; to access them within functions or methods.` |
|       - | 1335 | `*   These superglobal variables are:` |
|       - | 1336 | `*    $GLOBALS` |
|       - | 1337 | `*    $_SERVER` |
|       - | 1338 | `*    $_GET` |
|       - | 1339 | `*    $_POST` |
|       - | 1340 | `*    $_FILES` |
|       - | 1341 | `*    $_COOKIE` |
|       - | 1342 | `*    $_SESSION` |
|       - | 1343 | `*    $_REQUEST` |
|       - | 1344 | `*    $_ENV` |
|       - | 1345 | `*/` |
|    1990 | 1346 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       2 | 1347 |  |
|       - | 1348 | `	static const char * azSuper[] = {` |
|       - | 1349 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1350 | `		"_GET",      /* $_GET */` |
|       - | 1351 | `		"_POST",     /* $_POST */` |
|       - | 1352 | `		"_FILES",    /* $_FILES */` |
|       - | 1353 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1354 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1355 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1356 | `		"_ENV",      /* $_ENV */` |
|       - | 1357 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1358 | `		"argv"       /* $argv */` |
|       - | 1359 | `	};` |
|       - | 1360 | `	ph7_hashmap *pMap;` |
|       - | 1361 | `	ph7_value *pObj;` |
|       - | 1362 | `	SyString *pFile;` |
|       - | 1363 | `	sxi32 rc;` |
|       - | 1364 | `	sxu32 n;` |
|       - | 1365 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    1992 | 1366 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1992 | 1367 | `	if( pMap == 0 ){` |
|     ! 0 | 1368 | `		return SXERR_MEM;` |
|       - | 1369 | `	}` |
|    1992 | 1370 | `	pVm->pGlobal = pMap;` |
|       - | 1371 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1992 | 1372 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1992 | 1373 | `	if( pObj == 0 ){` |
|     ! 0 | 1374 | `		return SXERR_MEM;` |
|       - | 1375 | `	}` |
|    1992 | 1376 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1377 | `	/* Record object index */` |
|    1992 | 1378 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1379 | `	/* Install the special $GLOBALS array */` |
|    1992 | 1380 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1992 | 1381 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1382 | `		return rc;` |
|       - | 1383 | `	}` |
|       - | 1384 | `	/* Install superglobals now */` |
|   21892 | 1385 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1386 | `		ph7_value *pSuper;` |
|       - | 1387 | `		/* Request an empty array */` |
|   19902 | 1388 | `		pSuper = ph7_new_array(&(*pVm));` |
|   19902 | 1389 | `		if( pSuper == 0 ){` |
|     ! 0 | 1390 | `			return SXERR_MEM;` |
|       - | 1391 | `		}` |
|       - | 1392 | `		/* Install */` |
|   19902 | 1393 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   19902 | 1394 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1395 | `			return rc;` |
|       - | 1396 | `		}` |
|       - | 1397 | `		/* Release the value now it have been installed */` |
|   19902 | 1398 | `		ph7_release_value(&(*pVm),pSuper);` |
|    9952 | 1399 | `	}` |
|       - | 1400 | `	/* Set some $_SERVER entries */` |
|    1992 | 1401 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1402 | `	/*` |
|       - | 1403 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1404 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1405 | `	 */` |
|    3978 | 1406 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1407 | `		"SCRIPT_FILENAME",` |
|     995 | 1408 | `		pFile ? pFile->zString : ":Memory:",` |
|    1986 | 1409 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1410 | `		);` |
|       - | 1411 | `	/* All done,all super-global are installed now */` |
|    1992 | 1412 | `	return SXRET_OK;` |
|     997 | 1413 |  |
|       - | 1414 | `/*` |
|       - | 1415 | ` * Release a hashmap.` |
|       - | 1416 | ` */` |
|   41446 | 1417 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1418 |  |
|       - | 1419 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   41448 | 1420 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1421 | `	sxu32 n;` |
|   41448 | 1422 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1423 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1424 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1425 | `		return SXRET_OK;` |
|       - | 1426 | `	}` |
|       - | 1427 | `	/* Start the release process */` |
|   41448 | 1428 | `	n = 0;` |
|   41448 | 1429 | `	pEntry = pMap->pFirst;` |
| 1454988 | 1430 | `	for(;;){` |
| 2909978 | 1431 | `		if( n >= pMap->nEntry ){` |
|   41448 | 1432 | `			break;` |
|       - | 1433 | `		}` |
| 2868532 | 1434 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1435 | `		/* Remove the reference from the foreign table */` |
| 2868532 | 1436 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2868532 | 1437 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1438 | `			/* Restore the ph7_value to the free list */` |
| 2868524 | 1439 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1434261 | 1440 | `		}` |
|       - | 1441 | `		/* Release the node */` |
| 2868532 | 1442 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   59238 | 1443 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   29618 | 1444 | `		}` |
| 2868532 | 1445 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1446 | `		/* Point to the next entry */` |
| 2868532 | 1447 | `		pEntry = pNext;` |
| 2868532 | 1448 | `		n++;` |
|       2 | 1449 | `	}` |
|   41448 | 1450 | `	if( pMap->nEntry > 0 ){` |
|       - | 1451 | `		/* Release the hash bucket */` |
|   36884 | 1452 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   18441 | 1453 | `	}` |
|   41448 | 1454 | `	if( FreeDS ){` |
|       - | 1455 | `		/* Free the whole instance */` |
|   41432 | 1456 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   20717 | 1457 | `	}else{` |
|       - | 1458 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1459 | `		pMap->apBucket = 0;` |
|      17 | 1460 | `		pMap->iNextIdx = 0;` |
|      17 | 1461 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1462 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1463 | `	}` |
|   41448 | 1464 | `	return SXRET_OK;` |
|   20725 | 1465 |  |
|       - | 1466 | `/*` |
|       - | 1467 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1468 | ` * If the count reaches zero which mean no more variables` |
|       - | 1469 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1470 | ` */` |
|  469804 | 1471 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1472 |  |
|  469806 | 1473 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1474 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  469806 | 1475 | `	pMap->iRef--;` |
|  469806 | 1476 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   41432 | 1477 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   20715 | 1478 | `	}` |
|  469806 | 1479 |  |
|       - | 1480 | `/*` |
|       - | 1481 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1482 | ` * Write a pointer to the target node on success.` |
|       - | 1483 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1484 | ` */` |
|   85446 | 1485 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1486 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1487 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1488 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1489 | `	)` |
|       2 | 1490 |  |
|       - | 1491 | `	sxi32 rc;` |
|   85448 | 1492 | `	if( pMap->nEntry < 1 ){` |
|       - | 1493 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1494 | `		 */` |
|      19 | 1495 | `		return SXERR_NOTFOUND;` |
|       - | 1496 | `	}` |
|   85430 | 1497 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   85430 | 1498 | `	return rc;` |
|   42725 | 1499 |  |
|       - | 1500 | `/*` |
|       - | 1501 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1502 | ` * hashmap.` |
|       - | 1503 | ` * If a node with the given key already exists in the database` |
|       - | 1504 | ` * then this function overwrite the old value.` |
|       - | 1505 | ` */` |
| 2404418 | 1506 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1507 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1508 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1509 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1510 | `	)` |
|       2 | 1511 |  |
|       - | 1512 | `	sxi32 rc;` |
| 2404420 | 1513 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1514 | `		/*` |
|       - | 1515 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1516 | `		 */` |
|     ! 0 | 1517 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1518 | `		return SXRET_OK;` |
|       - | 1519 | `	}` |
| 2404420 | 1520 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2404420 | 1521 | `	return rc;` |
| 1202211 | 1522 |  |
|       - | 1523 | `/*` |
|       - | 1524 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1525 | ` * hashmap.` |
|       - | 1526 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1527 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1528 | ` * The insertion by reference is triggered when the following` |
|       - | 1529 | ` * expression is encountered.` |
|       - | 1530 | ` * $var = 10;` |
|       - | 1531 | ` *  $a = array(&var);` |
|       - | 1532 | ` * OR` |
|       - | 1533 | ` *  $a[] =& $var;` |
|       - | 1534 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1535 | ` * over it's contents.` |
|       - | 1536 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1537 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1538 | ` * Example:` |
|       - | 1539 | ` *  $var = 10;` |
|       - | 1540 | ` *  $a[] =& $var;` |
|       - | 1541 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1542 | ` *  //Unset the foreign ph7_value now` |
|       - | 1543 | ` *  unset($var);` |
|       - | 1544 | ` *  echo count($a); //0` |
|       - | 1545 | ` * Note that this is a PH7 eXtension.` |
|       - | 1546 | ` * Refer to the official documentation for more information.` |
|       - | 1547 | ` * If a node with the given key already exists in the database` |
|       - | 1548 | ` * then this function overwrite the old value.` |
|       - | 1549 | ` */` |
|   25560 | 1550 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1551 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1552 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1553 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1554 | `	)` |
|       2 | 1555 |  |
|       - | 1556 | `	sxi32 rc;` |
|   25562 | 1557 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1558 | `		/*` |
|       - | 1559 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1560 | `		 */` |
|     ! 0 | 1561 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1562 | `		return SXRET_OK;` |
|       - | 1563 | `	}` |
|   25562 | 1564 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   25562 | 1565 | `	return rc;` |
|   12782 | 1566 |  |
|       - | 1567 | `/*` |
|       - | 1568 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1569 | ` */` |
|   18472 | 1570 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1571 |  |
|       - | 1572 | `	/* Reset the loop cursor */` |
|   18474 | 1573 | `	pMap->pCur = pMap->pFirst;` |
|   18474 | 1574 |  |
|       - | 1575 | `/*` |
|       - | 1576 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1577 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1578 | ` * return NULL.` |
|       - | 1579 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1580 | ` */` |
|  147956 | 1581 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1582 |  |
|  147958 | 1583 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  147958 | 1584 | `	if( pCur == 0 ){` |
|       - | 1585 | `		/* End of the list,return null */` |
|    9258 | 1586 | `		return 0;` |
|       - | 1587 | `	}` |
|       - | 1588 | `	/* Advance the node cursor */` |
|  138702 | 1589 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  138702 | 1590 | `	return pCur;` |
|   73980 | 1591 |  |
|       - | 1592 | `/*` |
|       - | 1593 | ` * Extract a node value.` |
|       - | 1594 | ` */` |
|  348982 | 1595 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1596 |  |
|  348984 | 1597 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  348984 | 1598 | `	if( pEntry ){` |
|  348984 | 1599 | `		if( bStore ){` |
|  138730 | 1600 | `			PH7_MemObjStore(pEntry,pValue);` |
|   69366 | 1601 | `		}else{` |
|  210256 | 1602 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1603 | `		}` |
|  174533 | 1604 | `	}else{` |
|     ! 0 | 1605 | `		PH7_MemObjRelease(pValue);` |
|       - | 1606 | `	}` |
|  348984 | 1607 |  |
|       - | 1608 | `/*` |
|       - | 1609 | ` * Extract a node key.` |
|       - | 1610 | ` */` |
|   91404 | 1611 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1612 |  |
|       - | 1613 | `	/* Fill with the current key */` |
|   91406 | 1614 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   91134 | 1615 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      13 | 1616 | `			SyBlobRelease(&pKey->sBlob);` |
|       6 | 1617 | `		}` |
|   91134 | 1618 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   91134 | 1619 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   45568 | 1620 | `	}else{` |
|     274 | 1621 | `		SyBlobReset(&pKey->sBlob);` |
|     274 | 1622 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     274 | 1623 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1624 | `	}` |
|   91406 | 1625 |  |
|       - | 1626 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1627 | `/*` |
|       - | 1628 | ` * Store the address of nodes value in the given container.` |
|       - | 1629 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1630 | ` * defined in 'builtin.c' for more information.` |
|       - | 1631 | ` */` |
|      10 | 1632 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1633 |  |
|      11 | 1634 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1635 | `	ph7_value *pValue;` |
|       - | 1636 | `	sxu32 n;` |
|       - | 1637 | `	/* Initialize the container */` |
|      11 | 1638 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1639 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1640 | `		/* Extract node value */` |
|      17 | 1641 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1642 | `		if( pValue ){` |
|      17 | 1643 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1644 | `		}` |
|       - | 1645 | `		/* Point to the next entry */` |
|      17 | 1646 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1647 | `	}` |
|       - | 1648 | `	/* Total inserted entries */` |
|      11 | 1649 | `	return (int)SySetUsed(pOut);` |
|       1 | 1650 |  |
|       - | 1651 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1652 | `/*` |
|       - | 1653 | ` * Merge sort.` |
|       - | 1654 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1655 | ` * Status: Public domain` |
|       - | 1656 | ` */` |
|       - | 1657 | `/* Node comparison callback signature */` |
|       - | 1658 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1659 | `/*` |
|       - | 1660 | `** Inputs:` |
|       - | 1661 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1662 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1663 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1664 | `**` |
|       - | 1665 | `** Return Value:` |
|       - | 1666 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1667 | `**   of both a and b.` |
|       - | 1668 | `**` |
|       - | 1669 | `** Side effects:` |
|       - | 1670 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1671 | `**   changed.` |
|       - | 1672 | `*/` |
|   26782 | 1673 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1674 |  |
|       - | 1675 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1676 | `    /* Prevent compiler warning */` |
|   26784 | 1677 | `	result.pNext = result.pPrev = 0;` |
|   26784 | 1678 | `	pTail = &result;` |
|   66940 | 1679 | `	while( pA && pB ){` |
|   40158 | 1680 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   26438 | 1681 | `			pTail->pPrev = pA;` |
|   26438 | 1682 | `			pA->pNext = pTail;` |
|   26438 | 1683 | `			pTail = pA;` |
|   26438 | 1684 | `			pA = pA->pPrev;` |
|   13221 | 1685 | `		}else{` |
|   13722 | 1686 | `			pTail->pPrev = pB;` |
|   13722 | 1687 | `			pB->pNext = pTail;` |
|   13722 | 1688 | `			pTail = pB;` |
|   13722 | 1689 | `			pB = pB->pPrev;` |
|       - | 1690 | `		}` |
|       2 | 1691 | `	}` |
|   26784 | 1692 | `	if( pA ){` |
|   19974 | 1693 | `		pTail->pPrev = pA;` |
|   19974 | 1694 | `		pA->pNext = pTail;` |
|   16807 | 1695 | `	}else if( pB ){` |
|    6614 | 1696 | `		pTail->pPrev = pB;` |
|    6614 | 1697 | `		pB->pNext = pTail;` |
|    3299 | 1698 | `	}else{` |
|     200 | 1699 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1700 | `	}` |
|   26784 | 1701 | `	return result.pPrev;` |
|       2 | 1702 |  |
|       - | 1703 | `/*` |
|       - | 1704 | `** Inputs:` |
|       - | 1705 | `**   Map:       Input hashmap` |
|       - | 1706 | `**   cmp:       A comparison function.` |
|       - | 1707 | `**` |
|       - | 1708 | `** Return Value:` |
|       - | 1709 | `**   Sorted hashmap.` |
|       - | 1710 | `**` |
|       - | 1711 | `** Side effects:` |
|       - | 1712 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1713 | `*/` |
|       - | 1714 | `#define N_SORT_BUCKET  32` |
|     608 | 1715 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1716 |  |
|       - | 1717 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1718 | `	sxu32 i;` |
|     610 | 1719 | `	SyZero(a,sizeof(a));` |
|       - | 1720 | `	/* Point to the first inserted entry */` |
|     610 | 1721 | `	pIn = pMap->pFirst;` |
|    9582 | 1722 | `	while( pIn ){` |
|    8974 | 1723 | `		p = pIn;` |
|    8974 | 1724 | `		pIn = p->pPrev;` |
|    8974 | 1725 | `		p->pPrev = 0;` |
|   16908 | 1726 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   16908 | 1727 | `			if( a[i]==0 ){` |
|    8974 | 1728 | `				a[i] = p;` |
|    8974 | 1729 | `				break;` |
|     ! 0 | 1730 | `			}else{` |
|    7936 | 1731 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    7936 | 1732 | `				a[i] = 0;` |
|       - | 1733 | `			}` |
|    3969 | 1734 | `		}` |
|    8974 | 1735 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1736 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1737 | `			 * But that is impossible.` |
|       - | 1738 | `			 */` |
|     ! 0 | 1739 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1740 | `		}` |
|       2 | 1741 | `	}` |
|     610 | 1742 | `	p = a[0];` |
|   19458 | 1743 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   18850 | 1744 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    9426 | 1745 | `	}` |
|     610 | 1746 | `	p->pNext = 0;` |
|       - | 1747 | `	/* Reflect the change */` |
|     610 | 1748 | `	pMap->pFirst = p;` |
|       - | 1749 | `	/* Reset the loop cursor */` |
|     610 | 1750 | `	pMap->pCur = pMap->pFirst;` |
|     610 | 1751 | `	return SXRET_OK;` |
|       2 | 1752 |  |
|       - | 1753 | `/*` |
|       - | 1754 | ` * Node comparison callback.` |
|       - | 1755 | ` * used-by: [sort(),asort(),...]` |
|       - | 1756 | ` */` |
|   40028 | 1757 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1758 |  |
|       - | 1759 | `	ph7_value sA,sB;` |
|       - | 1760 | `	sxi32 iFlags;` |
|       - | 1761 | `	int rc;` |
|   40030 | 1762 | `	if( pCmpData == 0 ){` |
|       - | 1763 | `		/* Perform a standard comparison */` |
|   40006 | 1764 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   40006 | 1765 | `		return rc;` |
|       - | 1766 | `	}` |
|      26 | 1767 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1768 | `	/* Duplicate node values */` |
|      26 | 1769 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      26 | 1770 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      26 | 1771 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      26 | 1772 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      26 | 1773 | `	if( iFlags == 5 ){` |
|       - | 1774 | `		/* String cast */` |
|       - | 1775 | `		const char *zA,*zB;` |
|       - | 1776 | `		sxu32 nA,nB,nMin;` |
|      16 | 1777 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1778 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1779 | `		}` |
|      16 | 1780 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1781 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1782 | `		}` |
|       - | 1783 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      16 | 1784 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      16 | 1785 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      16 | 1786 | `		nA = SyBlobLength(&sA.sBlob);` |
|      16 | 1787 | `		nB = SyBlobLength(&sB.sBlob);` |
|      16 | 1788 | `		nMin = nA < nB ? nA : nB;` |
|      16 | 1789 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      16 | 1790 | `		if( rc == 0 ){` |
|       6 | 1791 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1792 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1793 | `		}` |
|       9 | 1794 | `	}else{` |
|       - | 1795 | `		/* Numeric cast */` |
|      11 | 1796 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1797 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1798 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1799 | `	}` |
|      26 | 1800 | `	PH7_MemObjRelease(&sA);` |
|      26 | 1801 | `	PH7_MemObjRelease(&sB);` |
|      26 | 1802 | `	return rc;` |
|   20036 | 1803 |  |
|       - | 1804 | `/*` |
|       - | 1805 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1806 | ` * used-by: [ksort()]` |
|       - | 1807 | ` */` |
|      14 | 1808 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1809 |  |
|       - | 1810 | `	sxi32 rc;` |
|       7 | 1811 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 1812 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1813 | `		/* Perform a string comparison */` |
|       5 | 1814 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1815 | `	}else{` |
|       - | 1816 | `		SyString sStr;` |
|       - | 1817 | `		sxi64 iA,iB;` |
|       - | 1818 | `		/* Perform a numeric comparison */` |
|      11 | 1819 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1820 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1821 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1822 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1823 | `				iA = 0;` |
|     ! 0 | 1824 | `			}else{` |
|     ! 0 | 1825 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1826 | `			}` |
|     ! 0 | 1827 | `		}else{` |
|      11 | 1828 | `			iA = pA->xKey.iKey;` |
|       - | 1829 | `		}` |
|      11 | 1830 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1831 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1832 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1833 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1834 | `				iB = 0;` |
|     ! 0 | 1835 | `			}else{` |
|     ! 0 | 1836 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1837 | `			}` |
|     ! 0 | 1838 | `		}else{` |
|      11 | 1839 | `			iB = pB->xKey.iKey;` |
|       - | 1840 | `		}` |
|      11 | 1841 | `		rc = (sxi32)(iA-iB);` |
|       - | 1842 | `	}` |
|       - | 1843 | `	/* Comparison result */` |
|      15 | 1844 | `	return rc;` |
|       1 | 1845 |  |
|       - | 1846 | `/*` |
|       - | 1847 | ` * Node comparison callback.` |
|       - | 1848 | ` * Used by: [rsort(),arsort()];` |
|       - | 1849 | ` */` |
|      78 | 1850 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1851 |  |
|       - | 1852 | `	ph7_value sA,sB;` |
|       - | 1853 | `	sxi32 iFlags;` |
|       - | 1854 | `	int rc;` |
|      80 | 1855 | `	if( pCmpData == 0 ){` |
|       - | 1856 | `		/* Perform a standard comparison */` |
|      60 | 1857 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      60 | 1858 | `		return -rc;` |
|       - | 1859 | `	}` |
|      21 | 1860 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1861 | `	/* Duplicate node values */` |
|      21 | 1862 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 1863 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 1864 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 1865 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 1866 | `	if( iFlags == 5 ){` |
|       - | 1867 | `		/* String cast */` |
|       - | 1868 | `		const char *zA,*zB;` |
|       - | 1869 | `		sxu32 nA,nB,nMin;` |
|      11 | 1870 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1871 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1872 | `		}` |
|      11 | 1873 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1874 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1875 | `		}` |
|       - | 1876 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 1877 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 1878 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 1879 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 1880 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 1881 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 1882 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 1883 | `		if( rc == 0 ){` |
|       3 | 1884 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1885 | `			else if( nA > nB ) rc = 1;` |
|       1 | 1886 | `		}` |
|       6 | 1887 | `	}else{` |
|       - | 1888 | `		/* Numeric cast */` |
|      11 | 1889 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1890 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1891 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1892 | `	}` |
|      21 | 1893 | `	PH7_MemObjRelease(&sA);` |
|      21 | 1894 | `	PH7_MemObjRelease(&sB);` |
|      21 | 1895 | `	return -rc;` |
|      41 | 1896 |  |
|       - | 1897 | `/*` |
|       - | 1898 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1899 | ` * used-by: [usort(),uasort()]` |
|       - | 1900 | ` */` |
|      12 | 1901 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1902 |  |
|       - | 1903 | `	ph7_value sResult,*pCallback;` |
|       - | 1904 | `	ph7_value *pV1,*pV2;` |
|       - | 1905 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1906 | `	sxi32 rc;` |
|       - | 1907 | `	/* Point to the desired callback */` |
|      13 | 1908 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1909 | `	/* initialize the result value */` |
|      13 | 1910 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 1911 | `	/* Extract nodes values */` |
|      13 | 1912 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      13 | 1913 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      13 | 1914 | `	apArg[0] = pV1;` |
|      13 | 1915 | `	apArg[1] = pV2;` |
|       - | 1916 | `	/* Invoke the callback */` |
|      13 | 1917 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      13 | 1918 | `	if( rc != SXRET_OK ){` |
|       - | 1919 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1920 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1921 | `	}else{` |
|       - | 1922 | `		/* Extract callback result */` |
|      13 | 1923 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1924 | `			/* Perform an int cast */` |
|     ! 0 | 1925 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1926 | `		}` |
|      13 | 1927 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1928 | `	}` |
|      13 | 1929 | `	PH7_MemObjRelease(&sResult);` |
|       - | 1930 | `	/* Callback result */` |
|      13 | 1931 | `	return rc;` |
|       1 | 1932 |  |
|       - | 1933 | `/*` |
|       - | 1934 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1935 | ` * used-by: [krsort()]` |
|       - | 1936 | ` */` |
|       4 | 1937 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1938 |  |
|       - | 1939 | `	sxi32 rc;` |
|       2 | 1940 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 1941 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1942 | `		/* Perform a string comparison */` |
|       5 | 1943 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1944 | `	}else{` |
|       - | 1945 | `		SyString sStr;` |
|       - | 1946 | `		sxi64 iA,iB;` |
|       - | 1947 | `		/* Perform a numeric comparison */` |
|     ! 0 | 1948 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1949 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1950 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1951 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1952 | `				iA = 0;` |
|     ! 0 | 1953 | `			}else{` |
|     ! 0 | 1954 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1955 | `			}` |
|     ! 0 | 1956 | `		}else{` |
|     ! 0 | 1957 | `			iA = pA->xKey.iKey;` |
|       - | 1958 | `		}` |
|     ! 0 | 1959 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1960 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1961 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1962 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1963 | `				iB = 0;` |
|     ! 0 | 1964 | `			}else{` |
|     ! 0 | 1965 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1966 | `			}` |
|     ! 0 | 1967 | `		}else{` |
|     ! 0 | 1968 | `			iB = pB->xKey.iKey;` |
|       - | 1969 | `		}` |
|     ! 0 | 1970 | `		rc = (sxi32)(iA-iB);` |
|       - | 1971 | `	}` |
|       5 | 1972 | `	return -rc; /* Reverse result */` |
|       1 | 1973 |  |
|       - | 1974 | `/*` |
|       - | 1975 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1976 | ` * used-by: [uksort()]` |
|       - | 1977 | ` */` |
|       6 | 1978 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1979 |  |
|       - | 1980 | `	ph7_value sResult,*pCallback;` |
|       - | 1981 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1982 | `	ph7_value sK1,sK2;` |
|       - | 1983 | `	sxi32 rc;` |
|       - | 1984 | `	/* Point to the desired callback */` |
|       7 | 1985 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1986 | `	/* initialize the result value */` |
|       7 | 1987 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 1988 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 1989 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 1990 | `	/* Extract nodes keys */` |
|       7 | 1991 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 1992 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 1993 | `	apArg[0] = &sK1;` |
|       7 | 1994 | `	apArg[1] = &sK2;` |
|       - | 1995 | `	/* Mark keys as constants */` |
|       7 | 1996 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 1997 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 1998 | `	/* Invoke the callback */` |
|       7 | 1999 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2000 | `	if( rc != SXRET_OK ){` |
|       - | 2001 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2002 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2003 | `	}else{` |
|       - | 2004 | `		/* Extract callback result */` |
|       7 | 2005 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2006 | `			/* Perform an int cast */` |
|     ! 0 | 2007 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2008 | `		}` |
|       7 | 2009 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2010 | `	}` |
|       7 | 2011 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2012 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2013 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2014 | `	/* Callback result */` |
|       7 | 2015 | `	return rc;` |
|       1 | 2016 |  |
|       - | 2017 | `/*` |
|       - | 2018 | ` * Node comparison callback: Random node comparison.` |
|       - | 2019 | ` * used-by: [shuffle()]` |
|       - | 2020 | ` */` |
|      14 | 2021 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2022 |  |
|       - | 2023 | `	sxu32 n;` |
|       7 | 2024 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 2025 | `	SXUNUSED(pCmpData);` |
|       - | 2026 | `	/* Grab a random number */` |
|      15 | 2027 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2028 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2029 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2030 | `	 */` |
|      15 | 2031 | `	return n&1 ? 1 : -1;` |
|       1 | 2032 |  |
|       - | 2033 | `/*` |
|       - | 2034 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2035 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2036 | ` */` |
|     560 | 2037 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2038 |  |
|       - | 2039 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2040 | `	sxu32 i;` |
|       - | 2041 | `	/* Rehash all entries */` |
|     562 | 2042 | `	pLast = p = pMap->pFirst;` |
|     562 | 2043 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     562 | 2044 | `	i = 0;` |
|    4681 | 2045 | `	for( ;; ){` |
|    9364 | 2046 | `		if( i >= pMap->nEntry ){` |
|     562 | 2047 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     562 | 2048 | `			break;` |
|       - | 2049 | `		}` |
|    8804 | 2050 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2051 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2052 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2053 | `			/* Change key type */` |
|       5 | 2054 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2055 | `		}` |
|    8804 | 2056 | `		HashmapRehashIntNode(p);` |
|       - | 2057 | `		/* Point to the next entry */` |
|    8804 | 2058 | `		i++;` |
|    8804 | 2059 | `		pLast = p;` |
|    8804 | 2060 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2061 | `	}` |
|     562 | 2062 |  |
|       - | 2063 | `/*` |
|       - | 2064 | ` * Array functions implementation.` |
|       - | 2065 | ` * Status:` |
|       - | 2066 | ` *  Stable.` |
|       - | 2067 | ` */` |
|       - | 2068 | `/*` |
|       - | 2069 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2070 | ` * Sort an array.` |
|       - | 2071 | ` * Parameters` |
|       - | 2072 | ` *  $array` |
|       - | 2073 | ` *   The input array.` |
|       - | 2074 | ` * $sort_flags` |
|       - | 2075 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2076 | ` *  Sorting type flags:` |
|       - | 2077 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2078 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2079 | ` *   SORT_STRING - compare items as strings` |
|       - | 2080 | ` * Return` |
|       - | 2081 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2082 | ` *` |
|       - | 2083 | ` */` |
|     864 | 2084 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2085 |  |
|       - | 2086 | `	ph7_hashmap *pMap;` |
|       - | 2087 | `	/* Make sure we are dealing with a valid hashmap */` |
|     866 | 2088 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2089 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2090 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2091 | `		return PH7_OK;` |
|       - | 2092 | `	}` |
|       - | 2093 | `	/* Point to the internal representation of the input hashmap */` |
|     866 | 2094 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     866 | 2095 | `	if( pMap->nEntry > 1 ){` |
|     556 | 2096 | `		sxi32 iCmpFlags = 0;` |
|     556 | 2097 | `		if( nArg > 1 ){` |
|       - | 2098 | `			/* Extract comparison flags */` |
|       3 | 2099 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2100 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2101 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2102 | `			}` |
|       1 | 2103 | `		}` |
|       - | 2104 | `		/* Do the merge sort */` |
|     556 | 2105 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2106 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     556 | 2107 | `		HashmapSortRehash(pMap);` |
|     277 | 2108 | `	}` |
|       - | 2109 | `	/* All done,return TRUE */` |
|     866 | 2110 | `	ph7_result_bool(pCtx,1);` |
|     866 | 2111 | `	return PH7_OK;` |
|     434 | 2112 |  |
|       - | 2113 | `/*` |
|       - | 2114 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2115 | ` *  Sort an array and maintain index association.` |
|       - | 2116 | ` * Parameters` |
|       - | 2117 | ` *  $array` |
|       - | 2118 | ` *   The input array.` |
|       - | 2119 | ` * $sort_flags` |
|       - | 2120 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2121 | ` *  Sorting type flags:` |
|       - | 2122 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2123 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2124 | ` *   SORT_STRING - compare items as strings` |
|       - | 2125 | ` * Return` |
|       - | 2126 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2127 | ` */` |
|      32 | 2128 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2129 |  |
|       - | 2130 | `	ph7_hashmap *pMap;` |
|       - | 2131 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2132 | `	if( nArg < 1 ){` |
|       3 | 2133 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2134 | `			"ArgumentCountError",` |
|       - | 2135 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2136 | `			);` |
|       - | 2137 | `	}` |
|       - | 2138 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2139 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2140 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2141 | `			"TypeError",` |
|       - | 2142 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2143 | `			ph7_type_name(apArg[0])` |
|       - | 2144 | `			);` |
|       - | 2145 | `	}` |
|       - | 2146 | `	/* Point to the internal representation of the input hashmap */` |
|      24 | 2147 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      24 | 2148 | `	if( pMap->nEntry > 1 ){` |
|      20 | 2149 | `		sxi32 iCmpFlags = 0;` |
|      20 | 2150 | `		if( nArg > 1 ){` |
|       - | 2151 | `			/* Extract comparison flags */` |
|       5 | 2152 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2153 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2154 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2155 | `			}` |
|       2 | 2156 | `		}` |
|       - | 2157 | `		/* Do the merge sort */` |
|      20 | 2158 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2159 | `		/* Fix the last link broken by the merge */` |
|      46 | 2160 | `		while(pMap->pLast->pPrev){` |
|      28 | 2161 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       2 | 2162 | `		}` |
|       9 | 2163 | `	}` |
|       - | 2164 | `	/* All done,return TRUE */` |
|      24 | 2165 | `	ph7_result_bool(pCtx,1);` |
|      24 | 2166 | `	return PH7_OK;` |
|      18 | 2167 |  |
|       - | 2168 | `/*` |
|       - | 2169 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2170 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2171 | ` * Parameters` |
|       - | 2172 | ` *  $array` |
|       - | 2173 | ` *   The input array.` |
|       - | 2174 | ` * $sort_flags` |
|       - | 2175 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2176 | ` *  Sorting type flags:` |
|       - | 2177 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2178 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2179 | ` *   SORT_STRING - compare items as strings` |
|       - | 2180 | ` * Return` |
|       - | 2181 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2182 | ` */` |
|      32 | 2183 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2184 |  |
|       - | 2185 | `	ph7_hashmap *pMap;` |
|       - | 2186 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2187 | `	if( nArg < 1 ){` |
|       3 | 2188 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2189 | `			"ArgumentCountError",` |
|       - | 2190 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2191 | `			);` |
|       - | 2192 | `	}` |
|       - | 2193 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2194 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2195 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2196 | `			"TypeError",` |
|       - | 2197 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2198 | `			ph7_type_name(apArg[0])` |
|       - | 2199 | `			);` |
|       - | 2200 | `	}` |
|       - | 2201 | `	/* Point to the internal representation of the input hashmap */` |
|      24 | 2202 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      24 | 2203 | `	if( pMap->nEntry > 1 ){` |
|      20 | 2204 | `		sxi32 iCmpFlags = 0;` |
|      20 | 2205 | `		if( nArg > 1 ){` |
|       - | 2206 | `			/* Extract comparison flags */` |
|       5 | 2207 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2208 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2209 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2210 | `			}` |
|       2 | 2211 | `		}` |
|       - | 2212 | `		/* Do the merge sort */` |
|      20 | 2213 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2214 | `		/* Fix the last link broken by the merge */` |
|      36 | 2215 | `		while(pMap->pLast->pPrev){` |
|      18 | 2216 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       2 | 2217 | `		}` |
|       9 | 2218 | `	}` |
|       - | 2219 | `	/* All done,return TRUE */` |
|      24 | 2220 | `	ph7_result_bool(pCtx,1);` |
|      24 | 2221 | `	return PH7_OK;` |
|      18 | 2222 |  |
|       - | 2223 | `/*` |
|       - | 2224 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2225 | ` *  Sort an array by key.` |
|       - | 2226 | ` * Parameters` |
|       - | 2227 | ` *  $array` |
|       - | 2228 | ` *   The input array.` |
|       - | 2229 | ` * $sort_flags` |
|       - | 2230 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2231 | ` *  Sorting type flags:` |
|       - | 2232 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2233 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2234 | ` *   SORT_STRING - compare items as strings` |
|       - | 2235 | ` * Return` |
|       - | 2236 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2237 | ` */` |
|       4 | 2238 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2239 |  |
|       - | 2240 | `	ph7_hashmap *pMap;` |
|       - | 2241 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2242 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2243 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2244 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2245 | `		return PH7_OK;` |
|       - | 2246 | `	}` |
|       - | 2247 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2248 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2249 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2250 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2251 | `		if( nArg > 1 ){` |
|       - | 2252 | `			/* Extract comparison flags */` |
|     ! 0 | 2253 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2254 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2255 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2256 | `			}` |
|     ! 0 | 2257 | `		}` |
|       - | 2258 | `		/* Do the merge sort */` |
|       5 | 2259 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2260 | `		/* Fix the last link broken by the merge */` |
|      15 | 2261 | `		while(pMap->pLast->pPrev){` |
|      11 | 2262 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2263 | `		}` |
|       2 | 2264 | `	}` |
|       - | 2265 | `	/* All done,return TRUE */` |
|       5 | 2266 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2267 | `	return PH7_OK;` |
|       3 | 2268 |  |
|       - | 2269 | `/*` |
|       - | 2270 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2271 | ` *  Sort an array by key in reverse order.` |
|       - | 2272 | ` * Parameters` |
|       - | 2273 | ` *  $array` |
|       - | 2274 | ` *   The input array.` |
|       - | 2275 | ` * $sort_flags` |
|       - | 2276 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2277 | ` *  Sorting type flags:` |
|       - | 2278 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2279 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2280 | ` *   SORT_STRING - compare items as strings` |
|       - | 2281 | ` * Return` |
|       - | 2282 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2283 | ` */` |
|       2 | 2284 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2285 |  |
|       - | 2286 | `	ph7_hashmap *pMap;` |
|       - | 2287 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2288 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2289 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2290 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2291 | `		return PH7_OK;` |
|       - | 2292 | `	}` |
|       - | 2293 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2294 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2295 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2296 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2297 | `		if( nArg > 1 ){` |
|       - | 2298 | `			/* Extract comparison flags */` |
|     ! 0 | 2299 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2300 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2301 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2302 | `			}` |
|     ! 0 | 2303 | `		}` |
|       - | 2304 | `		/* Do the merge sort */` |
|       3 | 2305 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2306 | `		/* Fix the last link broken by the merge */` |
|       7 | 2307 | `		while(pMap->pLast->pPrev){` |
|       5 | 2308 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2309 | `		}` |
|       1 | 2310 | `	}` |
|       - | 2311 | `	/* All done,return TRUE */` |
|       3 | 2312 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2313 | `	return PH7_OK;` |
|       2 | 2314 |  |
|       - | 2315 | `/*` |
|       - | 2316 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2317 | ` * Sort an array in reverse order.` |
|       - | 2318 | ` * Parameters` |
|       - | 2319 | ` *  $array` |
|       - | 2320 | ` *   The input array.` |
|       - | 2321 | ` * $sort_flags` |
|       - | 2322 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2323 | ` *  Sorting type flags:` |
|       - | 2324 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2325 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2326 | ` *   SORT_STRING - compare items as strings` |
|       - | 2327 | ` * Return` |
|       - | 2328 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2329 | ` */` |
|       2 | 2330 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2331 |  |
|       - | 2332 | `	ph7_hashmap *pMap;` |
|       - | 2333 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2334 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2335 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2336 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2337 | `		return PH7_OK;` |
|       - | 2338 | `	}` |
|       - | 2339 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2340 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2341 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2342 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2343 | `		if( nArg > 1 ){` |
|       - | 2344 | `			/* Extract comparison flags */` |
|     ! 0 | 2345 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2346 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2347 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2348 | `			}` |
|     ! 0 | 2349 | `		}` |
|       - | 2350 | `		/* Do the merge sort */` |
|       3 | 2351 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2352 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2353 | `		HashmapSortRehash(pMap);` |
|       1 | 2354 | `	}` |
|       - | 2355 | `	/* All done,return TRUE */` |
|       3 | 2356 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2357 | `	return PH7_OK;` |
|       2 | 2358 |  |
|       - | 2359 | `/*` |
|       - | 2360 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2361 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2362 | ` * Parameters` |
|       - | 2363 | ` *  $array` |
|       - | 2364 | ` *   The input array.` |
|       - | 2365 | ` * $cmp_function` |
|       - | 2366 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2367 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2368 | ` *  to, or greater than the second.` |
|       - | 2369 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2370 | ` * Return` |
|       - | 2371 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2372 | ` */` |
|       2 | 2373 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2374 |  |
|       - | 2375 | `	ph7_hashmap *pMap;` |
|       - | 2376 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2377 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2378 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2379 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2380 | `		return PH7_OK;` |
|       - | 2381 | `	}` |
|       - | 2382 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2383 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2384 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2385 | `		ph7_value *pCallback = 0;` |
|       - | 2386 | `		ProcNodeCmp xCmp;` |
|       3 | 2387 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2388 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2389 | `			/* Point to the desired callback */` |
|       3 | 2390 | `			pCallback = apArg[1];` |
|       2 | 2391 | `		}else{` |
|       - | 2392 | `			/* Use the default comparison function */` |
|     ! 0 | 2393 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2394 | `		}` |
|       - | 2395 | `		/* Do the merge sort */` |
|       3 | 2396 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2397 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2398 | `		HashmapSortRehash(pMap);` |
|       1 | 2399 | `	}` |
|       - | 2400 | `	/* All done,return TRUE */` |
|       3 | 2401 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2402 | `	return PH7_OK;` |
|       2 | 2403 |  |
|       - | 2404 | `/*` |
|       - | 2405 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2406 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2407 | ` *  and maintain index association.` |
|       - | 2408 | ` * Parameters` |
|       - | 2409 | ` *  $array` |
|       - | 2410 | ` *   The input array.` |
|       - | 2411 | ` * $cmp_function` |
|       - | 2412 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2413 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2414 | ` *  to, or greater than the second.` |
|       - | 2415 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2416 | ` * Return` |
|       - | 2417 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2418 | ` */` |
|       2 | 2419 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2420 |  |
|       - | 2421 | `	ph7_hashmap *pMap;` |
|       - | 2422 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2423 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2424 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2425 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2426 | `		return PH7_OK;` |
|       - | 2427 | `	}` |
|       - | 2428 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2429 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2430 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2431 | `		ph7_value *pCallback = 0;` |
|       - | 2432 | `		ProcNodeCmp xCmp;` |
|       3 | 2433 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2434 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2435 | `			/* Point to the desired callback */` |
|       3 | 2436 | `			pCallback = apArg[1];` |
|       2 | 2437 | `		}else{` |
|       - | 2438 | `			/* Use the default comparison function */` |
|     ! 0 | 2439 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2440 | `		}` |
|       - | 2441 | `		/* Do the merge sort */` |
|       3 | 2442 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2443 | `		/* Fix the last link broken by the merge */` |
|       5 | 2444 | `		while(pMap->pLast->pPrev){` |
|       3 | 2445 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2446 | `		}` |
|       1 | 2447 | `	}` |
|       - | 2448 | `	/* All done,return TRUE */` |
|       3 | 2449 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2450 | `	return PH7_OK;` |
|       2 | 2451 |  |
|       - | 2452 | `/*` |
|       - | 2453 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2454 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2455 | ` *  function and maintain index association.` |
|       - | 2456 | ` * Parameters` |
|       - | 2457 | ` *  $array` |
|       - | 2458 | ` *   The input array.` |
|       - | 2459 | ` * $cmp_function` |
|       - | 2460 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2461 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2462 | ` *  to, or greater than the second.` |
|       - | 2463 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2464 | ` * Return` |
|       - | 2465 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2466 | ` */` |
|       2 | 2467 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2468 |  |
|       - | 2469 | `	ph7_hashmap *pMap;` |
|       - | 2470 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2471 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2472 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2473 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2474 | `		return PH7_OK;` |
|       - | 2475 | `	}` |
|       - | 2476 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2477 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2478 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2479 | `		ph7_value *pCallback = 0;` |
|       - | 2480 | `		ProcNodeCmp xCmp;` |
|       3 | 2481 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2482 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2483 | `			/* Point to the desired callback */` |
|       3 | 2484 | `			pCallback = apArg[1];` |
|       2 | 2485 | `		}else{` |
|       - | 2486 | `			/* Use the default comparison function */` |
|     ! 0 | 2487 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2488 | `		}` |
|       - | 2489 | `		/* Do the merge sort */` |
|       3 | 2490 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2491 | `		/* Fix the last link broken by the merge */` |
|       3 | 2492 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2493 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2494 | `		}` |
|       1 | 2495 | `	}` |
|       - | 2496 | `	/* All done,return TRUE */` |
|       3 | 2497 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2498 | `	return PH7_OK;` |
|       2 | 2499 |  |
|       - | 2500 | `/*` |
|       - | 2501 | ` * bool shuffle(array &$array)` |
|       - | 2502 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2503 | ` * Parameters` |
|       - | 2504 | ` *  $array` |
|       - | 2505 | ` *   The input array.` |
|       - | 2506 | ` * Return` |
|       - | 2507 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2508 | ` *` |
|       - | 2509 | ` */` |
|       2 | 2510 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2511 |  |
|       - | 2512 | `	ph7_hashmap *pMap;` |
|       - | 2513 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2514 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2515 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2516 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2517 | `		return PH7_OK;` |
|       - | 2518 | `	}` |
|       - | 2519 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2520 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2521 | `	if( pMap->nEntry > 1 ){` |
|       - | 2522 | `		/* Do the merge sort */` |
|       3 | 2523 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2524 | `		/* Fix the last link broken by the merge */` |
|      11 | 2525 | `		while(pMap->pLast->pPrev){` |
|       9 | 2526 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2527 | `		}` |
|       1 | 2528 | `	}` |
|       - | 2529 | `	/* All done,return TRUE */` |
|       3 | 2530 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2531 | `	return PH7_OK;` |
|       2 | 2532 |  |
|       - | 2533 | `/*` |
|       - | 2534 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2535 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2536 | ` * Parameters` |
|       - | 2537 | ` *  $var` |
|       - | 2538 | ` *   The array or the object.` |
|       - | 2539 | ` * $mode` |
|       - | 2540 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2541 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2542 | ` *  all the elements of a multidimensional array.` |
|       - | 2543 | ` * Return` |
|       - | 2544 | ` *  Returns the number of elements in the array.` |
|       - | 2545 | ` */` |
|     642 | 2546 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2547 |  |
|     644 | 2548 | `	int bRecursive = FALSE;` |
|     644 | 2549 | `	int bCycleDetected = FALSE;` |
|       - | 2550 | `	sxi64 iCount;` |
|     644 | 2551 | `	if( nArg < 1 ){` |
|       3 | 2552 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2553 | `			"ArgumentCountError",` |
|       - | 2554 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2555 | `			);` |
|       - | 2556 | `	}` |
|     642 | 2557 | `	if( nArg > 2 ){` |
|       4 | 2558 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2559 | `			"ArgumentCountError",` |
|       - | 2560 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2561 | `			nArg` |
|       - | 2562 | `			);` |
|       - | 2563 | `	}` |
|     640 | 2564 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2565 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2566 | `			"TypeError",` |
|       - | 2567 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       5 | 2568 | `			ph7_type_name(apArg[0])` |
|       - | 2569 | `			);` |
|       - | 2570 | `	}` |
|     630 | 2571 | `	if( nArg > 1 ){` |
|      34 | 2572 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      34 | 2573 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       5 | 2574 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2575 | `				"ValueError",` |
|       - | 2576 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2577 | `				);` |
|       - | 2578 | `		}` |
|      29 | 2579 | `		bRecursive = iMode == 1;` |
|      14 | 2580 | `	}` |
|       - | 2581 | `	/* Count */` |
|     626 | 2582 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     626 | 2583 | `	if( bCycleDetected ){` |
|       3 | 2584 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2585 | `	}` |
|     626 | 2586 | `	ph7_result_int64(pCtx,iCount);` |
|     626 | 2587 | `	return PH7_OK;` |
|     323 | 2588 |  |
|       - | 2589 | `/*` |
|       - | 2590 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2591 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2592 | ` * Parameters` |
|       - | 2593 | ` * $key` |
|       - | 2594 | ` *   Value to check.` |
|       - | 2595 | ` * $search` |
|       - | 2596 | ` *  An array with keys to check.` |
|       - | 2597 | ` * Return` |
|       - | 2598 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2599 | ` */` |
|      66 | 2600 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2601 |  |
|       - | 2602 | `	sxi32 rc;` |
|      68 | 2603 | `	if( nArg != 2 ){` |
|       - | 2604 | `		/* PHP requires exactly two arguments */` |
|      10 | 2605 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2606 | `			"ArgumentCountError",` |
|       - | 2607 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2608 | `			nArg` |
|       - | 2609 | `			);` |
|       - | 2610 | `	}` |
|       - | 2611 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 2612 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2613 | `		/* Type mismatch -> TypeError */` |
|       7 | 2614 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2615 | `			"TypeError",` |
|       - | 2616 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2617 | `			ph7_type_name(apArg[1])` |
|       - | 2618 | `			);` |
|       - | 2619 | `	}` |
|       - | 2620 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      57 | 2621 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2622 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2623 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2624 | `			"use an empty string instead"` |
|       - | 2625 | `			);` |
|      56 | 2626 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2627 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2628 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2629 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2630 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2631 | `				,rVal` |
|       - | 2632 | `				);` |
|       1 | 2633 | `		}` |
|       1 | 2634 | `	}` |
|       - | 2635 | `	/* Perform the lookup */` |
|      57 | 2636 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2637 | `	/* lookup result */` |
|      57 | 2638 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      57 | 2639 | `	return PH7_OK;` |
|      35 | 2640 |  |
|       - | 2641 | `/*` |
|       - | 2642 | ` * value array_pop(array $array)` |
|       - | 2643 | ` *   POP the last inserted element from the array.` |
|       - | 2644 | ` * Parameter` |
|       - | 2645 | ` *  The array to get the value from.` |
|       - | 2646 | ` * Return` |
|       - | 2647 | ` *  Poped value or NULL on failure.` |
|       - | 2648 | ` */` |
|      16 | 2649 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2650 |  |
|       - | 2651 | `	ph7_hashmap *pMap;` |
|       - | 2652 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      18 | 2653 | `	if( nArg != 1 ){` |
|       7 | 2654 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2655 | `			"ArgumentCountError",` |
|       - | 2656 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2657 | `			nArg` |
|       - | 2658 | `			);` |
|       - | 2659 | `	}` |
|       - | 2660 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2661 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      14 | 2662 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2663 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2664 | `			"Error",` |
|       - | 2665 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2666 | `			);` |
|       - | 2667 | `	}` |
|       - | 2668 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 | 2669 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2670 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2671 | `			"TypeError",` |
|       - | 2672 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2673 | `			ph7_type_name(apArg[0])` |
|       - | 2674 | `			);` |
|       - | 2675 | `	}` |
|       7 | 2676 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 2677 | `	if( pMap->nEntry < 1 ){` |
|       - | 2678 | `		/* Nothing to pop,return NULL */` |
|       3 | 2679 | `		ph7_result_null(pCtx);` |
|       2 | 2680 | `	}else{` |
|       5 | 2681 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2682 | `		ph7_value *pObj;` |
|       5 | 2683 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       5 | 2684 | `		if( pObj ){` |
|       - | 2685 | `			/* Node value */` |
|       5 | 2686 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2687 | `			/* Unlink the node */` |
|       5 | 2688 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       3 | 2689 | `		}else{` |
|     ! 0 | 2690 | `			ph7_result_null(pCtx);` |
|       - | 2691 | `		}` |
|       - | 2692 | `		/* Reset the cursor */` |
|       5 | 2693 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2694 | `	}` |
|       7 | 2695 | `	return PH7_OK;` |
|      10 | 2696 |  |
|       - | 2697 | `/*` |
|       - | 2698 | ` * int array_push($array,$var,...)` |
|       - | 2699 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2700 | ` * Parameters` |
|       - | 2701 | ` *  array` |
|       - | 2702 | ` *    The input array.` |
|       - | 2703 | ` *  var` |
|       - | 2704 | ` *   On or more value to push.` |
|       - | 2705 | ` * Return` |
|       - | 2706 | ` *  New array count (including old items).` |
|       - | 2707 | ` */` |
|      20 | 2708 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2709 |  |
|       - | 2710 | `	ph7_hashmap *pMap;` |
|       - | 2711 | `	sxi32 rc;` |
|       - | 2712 | `	int i;` |
|      22 | 2713 | `	if( nArg < 1 ){` |
|       4 | 2714 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2715 | `			"ArgumentCountError",` |
|       - | 2716 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2717 | `			nArg` |
|       - | 2718 | `			);` |
|       - | 2719 | `	}` |
|       - | 2720 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2721 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      20 | 2722 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2723 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2724 | `			"Error",` |
|       - | 2725 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2726 | `			);` |
|       - | 2727 | `	}` |
|       - | 2728 | `	/* Make sure we are dealing with a valid hashmap */` |
|      16 | 2729 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2730 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2731 | `			"TypeError",` |
|       - | 2732 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2733 | `			ph7_type_name(apArg[0])` |
|       - | 2734 | `			);` |
|       - | 2735 | `	}` |
|       - | 2736 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 2737 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2738 | `	/* Start pushing given values */` |
|      27 | 2739 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      15 | 2740 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      15 | 2741 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2742 | `			break;` |
|       - | 2743 | `		}` |
|       8 | 2744 | `	}` |
|       - | 2745 | `	/* Return the new count */` |
|      13 | 2746 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      13 | 2747 | `	return PH7_OK;` |
|      12 | 2748 |  |
|       - | 2749 | `/*` |
|       - | 2750 | ` * value array_shift(array $array)` |
|       - | 2751 | ` *   Shift an element off the beginning of array.` |
|       - | 2752 | ` * Parameter` |
|       - | 2753 | ` *  The array to get the value from.` |
|       - | 2754 | ` * Return` |
|       - | 2755 | ` *  Shifted value or NULL on failure.` |
|       - | 2756 | ` */` |
|      36 | 2757 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2758 |  |
|       - | 2759 | `	ph7_hashmap *pMap;` |
|       - | 2760 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      38 | 2761 | `	if( nArg != 1 ){` |
|       7 | 2762 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2763 | `			"ArgumentCountError",` |
|       - | 2764 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2765 | `			nArg` |
|       - | 2766 | `			);` |
|       - | 2767 | `	}` |
|       - | 2768 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      34 | 2769 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2770 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2771 | `			"Error",` |
|       - | 2772 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2773 | `			);` |
|       - | 2774 | `	}` |
|       - | 2775 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 | 2776 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2777 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2778 | `			"TypeError",` |
|       - | 2779 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2780 | `			ph7_type_name(apArg[0])` |
|       - | 2781 | `			);` |
|       - | 2782 | `	}` |
|       - | 2783 | `	/* Point to the internal representation of the hashmap */` |
|      28 | 2784 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      28 | 2785 | `	if( pMap->nEntry < 1 ){` |
|       - | 2786 | `		/* Empty hashmap,return NULL */` |
|       3 | 2787 | `		ph7_result_null(pCtx);` |
|       2 | 2788 | `	}else{` |
|      26 | 2789 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2790 | `		ph7_value *pObj;` |
|       - | 2791 | `		sxu32 n;` |
|      26 | 2792 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      26 | 2793 | `		if( pObj ){` |
|       - | 2794 | `			/* Node value */` |
|      26 | 2795 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2796 | `			/* Unlink the first node */` |
|      26 | 2797 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      14 | 2798 | `		}else{` |
|     ! 0 | 2799 | `			ph7_result_null(pCtx);` |
|       - | 2800 | `		}` |
|       - | 2801 | `		/* Rehash all int keys */` |
|      26 | 2802 | `		n = pMap->nEntry;` |
|      26 | 2803 | `		pEntry = pMap->pFirst;` |
|      26 | 2804 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      37 | 2805 | `		for(;;){` |
|      76 | 2806 | `			if( n < 1 ){` |
|      26 | 2807 | `				break;` |
|       - | 2808 | `			}` |
|      52 | 2809 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      52 | 2810 | `				HashmapRehashIntNode(pEntry);` |
|      25 | 2811 | `			}` |
|       - | 2812 | `			/* Point to the next entry */` |
|      52 | 2813 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      52 | 2814 | `			n--;` |
|       2 | 2815 | `		}` |
|       - | 2816 | `		/* Reset the cursor */` |
|      26 | 2817 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2818 | `	}` |
|      28 | 2819 | `	return PH7_OK;` |
|      20 | 2820 |  |
|       - | 2821 | `/*` |
|       - | 2822 | ` * Extract the node cursor value.` |
|       - | 2823 | ` */` |
|      24 | 2824 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 2825 |  |
|      25 | 2826 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 2827 | `	ph7_value *pVal;` |
|      25 | 2828 | `	if( pCur == 0 ){` |
|       - | 2829 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 2830 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2831 | `		return PH7_OK;` |
|       - | 2832 | `	}` |
|      25 | 2833 | `	if( iDirection != 0 ){` |
|       9 | 2834 | `		if( iDirection > 0 ){` |
|       - | 2835 | `			/* Point to the next entry */` |
|       7 | 2836 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 2837 | `			pCur = pMap->pCur;` |
|       4 | 2838 | `		}else{` |
|       - | 2839 | `			/* Point to the previous entry */` |
|       3 | 2840 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 2841 | `			pCur = pMap->pCur;` |
|       - | 2842 | `		}` |
|       9 | 2843 | `		if( pCur == 0 ){` |
|       - | 2844 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 2845 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2846 | `			return PH7_OK;` |
|       - | 2847 | `		}` |
|       4 | 2848 | `	}` |
|       - | 2849 | `	/* Point to the desired element */` |
|      25 | 2850 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 2851 | `	if( pVal ){` |
|      25 | 2852 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 2853 | `	}else{` |
|     ! 0 | 2854 | `		ph7_result_bool(pCtx,0);` |
|       - | 2855 | `	}` |
|      25 | 2856 | `	return PH7_OK;` |
|      13 | 2857 |  |
|       - | 2858 | `/*` |
|       - | 2859 | ` * value current(array $array)` |
|       - | 2860 | ` *  Return the current element in an array.` |
|       - | 2861 | ` * Parameter` |
|       - | 2862 | ` *  $input: The input array.` |
|       - | 2863 | ` * Return` |
|       - | 2864 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 2865 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2866 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2867 | ` *  is empty, current() returns FALSE.` |
|       - | 2868 | ` */` |
|      10 | 2869 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2870 |  |
|      11 | 2871 | `	if( nArg < 1 ){` |
|       - | 2872 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2873 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2874 | `		return PH7_OK;` |
|       - | 2875 | `	}` |
|       - | 2876 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 2877 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2878 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2879 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2880 | `		return PH7_OK;` |
|       - | 2881 | `	}` |
|      11 | 2882 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 2883 | `	return PH7_OK;` |
|       6 | 2884 |  |
|       - | 2885 | `/*` |
|       - | 2886 | ` * value next(array $input)` |
|       - | 2887 | ` *  Advance the internal array pointer of an array.` |
|       - | 2888 | ` * Parameter` |
|       - | 2889 | ` *  $input: The input array.` |
|       - | 2890 | ` * Return` |
|       - | 2891 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 2892 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 2893 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 2894 | ` */` |
|       6 | 2895 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2896 |  |
|       7 | 2897 | `	if( nArg < 1 ){` |
|       - | 2898 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2899 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2900 | `		return PH7_OK;` |
|       - | 2901 | `	}` |
|       - | 2902 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 2903 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2904 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2905 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2906 | `		return PH7_OK;` |
|       - | 2907 | `	}` |
|       7 | 2908 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 2909 | `	return PH7_OK;` |
|       4 | 2910 |  |
|       - | 2911 | `/*` |
|       - | 2912 | ` * value prev(array $input)` |
|       - | 2913 | ` *  Rewind the internal array pointer.` |
|       - | 2914 | ` * Parameter` |
|       - | 2915 | ` *  $input: The input array.` |
|       - | 2916 | ` * Return` |
|       - | 2917 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 2918 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 2919 | ` *  elements.` |
|       - | 2920 | ` */` |
|       2 | 2921 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2922 |  |
|       3 | 2923 | `	if( nArg < 1 ){` |
|       - | 2924 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2925 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2926 | `		return PH7_OK;` |
|       - | 2927 | `	}` |
|       - | 2928 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2929 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2930 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2931 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2932 | `		return PH7_OK;` |
|       - | 2933 | `	}` |
|       3 | 2934 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 2935 | `	return PH7_OK;` |
|       2 | 2936 |  |
|       - | 2937 | `/*` |
|       - | 2938 | ` * value end(array $input)` |
|       - | 2939 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 2940 | ` * Parameter` |
|       - | 2941 | ` *  $input: The input array.` |
|       - | 2942 | ` * Return` |
|       - | 2943 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 2944 | ` */` |
|       2 | 2945 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2946 |  |
|       - | 2947 | `	ph7_hashmap *pMap;` |
|       3 | 2948 | `	if( nArg < 1 ){` |
|       - | 2949 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2950 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2951 | `		return PH7_OK;` |
|       - | 2952 | `	}` |
|       - | 2953 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2954 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2955 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2956 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2957 | `		return PH7_OK;` |
|       - | 2958 | `	}` |
|       - | 2959 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2960 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2961 | `	/* Point to the last node */` |
|       3 | 2962 | `	pMap->pCur = pMap->pLast;` |
|       - | 2963 | `	/* Return the last node value */` |
|       3 | 2964 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 2965 | `	return PH7_OK;` |
|       2 | 2966 |  |
|       - | 2967 | `/*` |
|       - | 2968 | ` * value reset(array $array )` |
|       - | 2969 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 2970 | ` * Parameter` |
|       - | 2971 | ` *  $input: The input array.` |
|       - | 2972 | ` * Return` |
|       - | 2973 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 2974 | ` */` |
|       4 | 2975 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2976 |  |
|       - | 2977 | `	ph7_hashmap *pMap;` |
|       5 | 2978 | `	if( nArg < 1 ){` |
|       - | 2979 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2980 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2981 | `		return PH7_OK;` |
|       - | 2982 | `	}` |
|       - | 2983 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2984 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2985 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2986 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2987 | `		return PH7_OK;` |
|       - | 2988 | `	}` |
|       - | 2989 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2990 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2991 | `	/* Point to the first node */` |
|       5 | 2992 | `	pMap->pCur = pMap->pFirst;` |
|       - | 2993 | `	/* Return the last node value if available */` |
|       5 | 2994 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 2995 | `	return PH7_OK;` |
|       3 | 2996 |  |
|       - | 2997 | `/*` |
|       - | 2998 | ` * value key(array $array)` |
|       - | 2999 | ` *   Fetch a key from an array` |
|       - | 3000 | ` * Parameter` |
|       - | 3001 | ` *  $input` |
|       - | 3002 | ` *   The input array.` |
|       - | 3003 | ` * Return` |
|       - | 3004 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3005 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3006 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3007 | ` *  is empty, key() returns NULL.` |
|       - | 3008 | ` */` |
|       4 | 3009 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3010 |  |
|       - | 3011 | `	ph7_hashmap_node *pCur;` |
|       - | 3012 | `	ph7_hashmap *pMap;` |
|       5 | 3013 | `	if( nArg < 1 ){` |
|       - | 3014 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3015 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3016 | `		return PH7_OK;` |
|       - | 3017 | `	}` |
|       - | 3018 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3019 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3020 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3021 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3022 | `		return PH7_OK;` |
|       - | 3023 | `	}` |
|       5 | 3024 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3025 | `	pCur = pMap->pCur;` |
|       5 | 3026 | `	if( pCur == 0 ){` |
|       - | 3027 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3028 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3029 | `		return PH7_OK;` |
|       - | 3030 | `	}` |
|       5 | 3031 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3032 | `		/* Key is integer */` |
|     ! 0 | 3033 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3034 | `	}else{` |
|       - | 3035 | `		/* Key is blob */` |
|       7 | 3036 | `		ph7_result_string(pCtx,` |
|       4 | 3037 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3038 | `	}` |
|       5 | 3039 | `	return PH7_OK;` |
|       3 | 3040 |  |
|       - | 3041 | `/*` |
|       - | 3042 | ` * array each(array $input)` |
|       - | 3043 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3044 | ` * Parameter` |
|       - | 3045 | ` *  $input` |
|       - | 3046 | ` *    The input array.` |
|       - | 3047 | ` * Return` |
|       - | 3048 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3049 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3050 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3051 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3052 | ` *  each() returns FALSE.` |
|       - | 3053 | ` */` |
|      22 | 3054 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3055 |  |
|       - | 3056 | `	ph7_hashmap_node *pCur;` |
|       - | 3057 | `	ph7_hashmap *pMap;` |
|       - | 3058 | `	ph7_value *pArray;` |
|       - | 3059 | `	ph7_value *pVal;` |
|       - | 3060 | `	ph7_value sKey;` |
|      23 | 3061 | `	if( nArg < 1 ){` |
|       - | 3062 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3063 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3064 | `		return PH7_OK;` |
|       - | 3065 | `	}` |
|       - | 3066 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3067 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3068 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3069 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3070 | `		return PH7_OK;` |
|       - | 3071 | `	}` |
|       - | 3072 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3073 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3074 | `	if( pMap->pCur == 0 ){` |
|       - | 3075 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3076 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3077 | `		return PH7_OK;` |
|       - | 3078 | `	}` |
|      15 | 3079 | `	pCur = pMap->pCur;` |
|       - | 3080 | `	/* Create a new array */` |
|      15 | 3081 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3082 | `	if( pArray == 0 ){` |
|     ! 0 | 3083 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3084 | `		return PH7_OK;` |
|       - | 3085 | `	}` |
|      15 | 3086 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3087 | `	/* Insert the current value */` |
|      15 | 3088 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3089 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3090 | `	/* Make the key */` |
|      15 | 3091 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3092 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3093 | `	}else{` |
|       9 | 3094 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3095 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3096 | `	}` |
|       - | 3097 | `	/* Insert the current key */` |
|      15 | 3098 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3099 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3100 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3101 | `	/* Advance the cursor */` |
|      15 | 3102 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3103 | `	/* Return the current entry */` |
|      15 | 3104 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3105 | `	return PH7_OK;` |
|      12 | 3106 |  |
|       - | 3107 | `/*` |
|       - | 3108 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3109 | ` *  Create an array containing a range of elements` |
|       - | 3110 | ` * Parameter` |
|       - | 3111 | ` *  start` |
|       - | 3112 | ` *   First value of the sequence.` |
|       - | 3113 | ` *  limit` |
|       - | 3114 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3115 | ` *  step` |
|       - | 3116 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3117 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3118 | ` * Return` |
|       - | 3119 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3120 | ` * NOTE:` |
|       - | 3121 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3122 | ` */` |
|       2 | 3123 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3124 |  |
|       - | 3125 | `	ph7_value *pValue,*pArray;` |
|       - | 3126 | `	sxi64 iOfft,iLimit;` |
|       3 | 3127 | `	int iStep = 1;` |
|       - | 3128 |  |
|       3 | 3129 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3130 | `	if( nArg > 0 ){` |
|       - | 3131 | `		/* Extract the offset */` |
|       3 | 3132 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3133 | `		if( nArg > 1 ){` |
|       - | 3134 | `			/* Extract the limit */` |
|       3 | 3135 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3136 | `			if( nArg > 2 ){` |
|       - | 3137 | `				/* Extract the increment */` |
|       3 | 3138 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3139 | `				if( iStep < 1 ){` |
|       - | 3140 | `					/* Only positive number are allowed */` |
|       3 | 3141 | `					iStep = 1;` |
|       1 | 3142 | `				}` |
|       1 | 3143 | `			}` |
|       1 | 3144 | `		}` |
|       1 | 3145 | `	}` |
|       - | 3146 | `	/* Element container */` |
|       3 | 3147 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3148 | `	/* Create the new array */` |
|       3 | 3149 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3150 | `	if( pArray == 0 ){` |
|     ! 0 | 3151 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3152 | `		return PH7_OK;` |
|       - | 3153 | `	}` |
|       - | 3154 | `	/* Start filling */` |
|       3 | 3155 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3156 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3157 | `		/* Perform the insertion */` |
|     ! 0 | 3158 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3159 | `		/* Increment */` |
|     ! 0 | 3160 | `		iOfft += iStep;` |
|     ! 0 | 3161 | `	}` |
|       - | 3162 | `	/* Return the new array */` |
|       3 | 3163 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3164 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3165 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3166 | `	 */` |
|       3 | 3167 | `	return PH7_OK;` |
|       2 | 3168 |  |
|       - | 3169 | `/*` |
|       - | 3170 | ` * array array_values(array $array)` |
|       - | 3171 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3172 | ` * Parameters` |
|       - | 3173 | ` *  $array` |
|       - | 3174 | ` *   The input array.` |
|       - | 3175 | ` * Return` |
|       - | 3176 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3177 | ` */` |
|      30 | 3178 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3179 |  |
|       - | 3180 | `	ph7_hashmap_node *pNode;` |
|       - | 3181 | `	ph7_hashmap *pMap;` |
|       - | 3182 | `	ph7_value *pArray;` |
|       - | 3183 | `	ph7_value *pObj;` |
|       - | 3184 | `	sxu32 n;` |
|      32 | 3185 | `	if( nArg != 1 ){` |
|       - | 3186 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3187 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3188 | `			"ArgumentCountError",` |
|       - | 3189 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3190 | `			nArg` |
|       - | 3191 | `			);` |
|       - | 3192 | `	}` |
|       - | 3193 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3194 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3195 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3196 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3197 | `			"TypeError",` |
|       - | 3198 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3199 | `			ph7_type_name(apArg[0])` |
|       - | 3200 | `			);` |
|       - | 3201 | `	}` |
|       - | 3202 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3203 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3204 | `	/* Create a new array */` |
|      25 | 3205 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3206 | `	if( pArray == 0 ){` |
|     ! 0 | 3207 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3208 | `		return PH7_OK;` |
|       - | 3209 | `	}` |
|       - | 3210 | `	/* Perform the requested operation */` |
|      25 | 3211 | `	pNode = pMap->pFirst;` |
|      83 | 3212 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3213 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3214 | `		if( pObj ){` |
|       - | 3215 | `			/* perform the insertion */` |
|      59 | 3216 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3217 | `		}` |
|       - | 3218 | `		/* Point to the next entry */` |
|      59 | 3219 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3220 | `	}` |
|       - | 3221 | `	/* return the new array */` |
|      25 | 3222 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3223 | `	return PH7_OK;` |
|      17 | 3224 |  |
|       - | 3225 | `/*` |
|       - | 3226 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3227 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3228 | ` * Parameters` |
|       - | 3229 | ` *  $input` |
|       - | 3230 | ` *   An array containing keys to return.` |
|       - | 3231 | ` * $search_value` |
|       - | 3232 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3233 | ` * $strict` |
|       - | 3234 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3235 | ` * Return` |
|       - | 3236 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3237 | ` */` |
|     120 | 3238 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3239 |  |
|       - | 3240 | `	ph7_hashmap_node *pNode;` |
|       - | 3241 | `	ph7_hashmap *pMap;` |
|       - | 3242 | `	ph7_value *pArray;` |
|       - | 3243 | `	ph7_value sObj;` |
|       - | 3244 | `	ph7_value sVal;` |
|       - | 3245 | `	SyString sKey;` |
|       - | 3246 | `	int bStrict;` |
|       - | 3247 | `	sxi32 rc;` |
|       - | 3248 | `	sxu32 n;` |
|     122 | 3249 | `	if( nArg < 1 ){` |
|       - | 3250 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3251 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3252 | `			"ArgumentCountError",` |
|       - | 3253 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3254 | `			);` |
|       - | 3255 | `	}` |
|       - | 3256 | `	/* Make sure we are dealing with a valid hashmap */` |
|     120 | 3257 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3258 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3259 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3260 | `			"TypeError",` |
|       - | 3261 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3262 | `			ph7_type_name(apArg[0])` |
|       - | 3263 | `			);` |
|       - | 3264 | `	}` |
|       - | 3265 | `	/* Point to the internal representation of the input hashmap */` |
|     118 | 3266 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3267 | `	/* Create a new array */` |
|     118 | 3268 | `	pArray = ph7_context_new_array(pCtx);` |
|     118 | 3269 | `	if( pArray == 0 ){` |
|     ! 0 | 3270 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3271 | `		return PH7_OK;` |
|       - | 3272 | `	}` |
|     118 | 3273 | `	bStrict = FALSE;` |
|     118 | 3274 | `	if( nArg > 2 ){` |
|       - | 3275 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3276 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3277 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3278 | `				"TypeError",` |
|       - | 3279 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3280 | `				ph7_type_name(apArg[2])` |
|       - | 3281 | `				);` |
|       - | 3282 | `		}` |
|       5 | 3283 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3284 | `	}` |
|       - | 3285 | `	/* Perform the requested operation */` |
|     115 | 3286 | `	pNode = pMap->pFirst;` |
|     115 | 3287 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     553 | 3288 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     439 | 3289 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     117 | 3290 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      59 | 3291 | `		}else{` |
|     323 | 3292 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3293 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3294 | `		}` |
|     439 | 3295 | `		rc = 0;` |
|     439 | 3296 | `		if( nArg > 1 ){` |
|      31 | 3297 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3298 | `			if( pValue ){` |
|      31 | 3299 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3300 | `				/* Filter key */` |
|      31 | 3301 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3302 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3303 | `			}` |
|      15 | 3304 | `		}` |
|     439 | 3305 | `		if( rc == 0 ){` |
|       - | 3306 | `			/* Perform the insertion */` |
|     421 | 3307 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     210 | 3308 | `		}` |
|     439 | 3309 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3310 | `		/* Point to the next entry */` |
|     439 | 3311 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     220 | 3312 | `	}` |
|       - | 3313 | `	/* return the new array */` |
|     115 | 3314 | `	ph7_result_value(pCtx,pArray);` |
|     115 | 3315 | `	return PH7_OK;` |
|      62 | 3316 |  |
|       - | 3317 | `/*` |
|       - | 3318 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3319 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3320 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3321 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3322 | ` * Parameters` |
|       - | 3323 | ` *  $arr1` |
|       - | 3324 | ` *   First array` |
|       - | 3325 | ` *  $arr2` |
|       - | 3326 | ` *   Second array` |
|       - | 3327 | ` * Return` |
|       - | 3328 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3329 | ` * Note` |
|       - | 3330 | ` *  This function is a symisc eXtension.` |
|       - | 3331 | ` */` |
|       4 | 3332 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3333 |  |
|       - | 3334 | `	ph7_hashmap *p1,*p2;` |
|       - | 3335 | `	int rc;` |
|       5 | 3336 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3337 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3338 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3339 | `		return PH7_OK;` |
|       - | 3340 | `	}` |
|       - | 3341 | `	/* Point to the hashmaps */` |
|       5 | 3342 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3343 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3344 | `	rc = (p1 == p2);` |
|       - | 3345 | `	/* Same instance? */` |
|       5 | 3346 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3347 | `	return PH7_OK;` |
|       3 | 3348 |  |
|       - | 3349 | `/*` |
|       - | 3350 | ` * array array_merge(array ...$arrays)` |
|       - | 3351 | ` *  Merge one or more arrays.` |
|       - | 3352 | ` * Parameters` |
|       - | 3353 | ` *  ...$arrays` |
|       - | 3354 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3355 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3356 | ` * Return` |
|       - | 3357 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3358 | ` *  with no arguments.` |
|       - | 3359 | ` */` |
|     892 | 3360 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3361 |  |
|       - | 3362 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3363 | `	ph7_value *pArray;` |
|       - | 3364 | `	int i;` |
|       - | 3365 | `	/* Create a new array */` |
|     894 | 3366 | `	pArray = ph7_context_new_array(pCtx);` |
|     894 | 3367 | `	if( pArray == 0 ){` |
|     ! 0 | 3368 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3369 | `		return PH7_OK;` |
|       - | 3370 | `	}` |
|       - | 3371 | `	/* Point to the internal representation of the hashmap */` |
|     894 | 3372 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3373 | `	/* Start merging */` |
|    2668 | 3374 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3375 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1780 | 3376 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3377 | `			/* Type mismatch -> TypeError */` |
|       7 | 3378 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3379 | `				"TypeError",` |
|       - | 3380 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3381 | `				i + 1,` |
|       4 | 3382 | `				ph7_type_name(apArg[i])` |
|       - | 3383 | `				);` |
|     ! 0 | 3384 | `		}else{` |
|    1776 | 3385 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3386 | `			/* Merge the two hashmaps */` |
|    1776 | 3387 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3388 | `		}` |
|     889 | 3389 | `	}` |
|       - | 3390 | `	/* Return the freshly created array */` |
|     890 | 3391 | `	ph7_result_value(pCtx,pArray);` |
|     890 | 3392 | `	return PH7_OK;` |
|     448 | 3393 |  |
|       - | 3394 | `/*` |
|       - | 3395 | ` * array array_copy(array $source)` |
|       - | 3396 | ` *  Make a blind copy of the target array.` |
|       - | 3397 | ` * Parameters` |
|       - | 3398 | ` *  $source` |
|       - | 3399 | ` *   Target array` |
|       - | 3400 | ` * Return` |
|       - | 3401 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3402 | ` * Note` |
|       - | 3403 | ` *  This function is a symisc eXtension.` |
|       - | 3404 | ` */` |
|      16 | 3405 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3406 |  |
|       - | 3407 | `	ph7_hashmap *pMap;` |
|       - | 3408 | `	ph7_value *pArray;` |
|      17 | 3409 | `	if( nArg < 1 ){` |
|       - | 3410 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3411 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3412 | `		return PH7_OK;` |
|       - | 3413 | `	}` |
|       - | 3414 | `	/* Create a new array */` |
|      17 | 3415 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3416 | `	if( pArray == 0 ){` |
|     ! 0 | 3417 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3418 | `		return PH7_OK;` |
|       - | 3419 | `	}` |
|       - | 3420 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3421 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3422 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3423 | `		/* Point to the internal representation of the source */` |
|      17 | 3424 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3425 | `		/* Perform the copy */` |
|      17 | 3426 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3427 | `	}else{` |
|       - | 3428 | `		/* Simple insertion */` |
|     ! 0 | 3429 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3430 | `	}` |
|       - | 3431 | `	/* Return the duplicated array */` |
|      17 | 3432 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3433 | `	return PH7_OK;` |
|       9 | 3434 |  |
|       - | 3435 | `/*` |
|       - | 3436 | ` * bool array_erase(array $source)` |
|       - | 3437 | ` *  Remove all elements from a given array.` |
|       - | 3438 | ` * Parameters` |
|       - | 3439 | ` *  $source` |
|       - | 3440 | ` *   Target array` |
|       - | 3441 | ` * Return` |
|       - | 3442 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3443 | ` * Note` |
|       - | 3444 | ` *  This function is a symisc eXtension.` |
|       - | 3445 | ` */` |
|      16 | 3446 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3447 |  |
|       - | 3448 | `	ph7_hashmap *pMap;` |
|      17 | 3449 | `	if( nArg < 1 ){` |
|       - | 3450 | `		/* Missing arguments */` |
|     ! 0 | 3451 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3452 | `		return PH7_OK;` |
|       - | 3453 | `	}` |
|       - | 3454 | `	/* Point to the target hashmap */` |
|      17 | 3455 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3456 | `	/* Erase */` |
|      17 | 3457 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3458 | `	return PH7_OK;` |
|       9 | 3459 |  |
|       - | 3460 | `/*` |
|       - | 3461 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3462 | ` *  Extract a slice of the array.` |
|       - | 3463 | ` * Parameters` |
|       - | 3464 | ` *  $array` |
|       - | 3465 | ` *    The input array.` |
|       - | 3466 | ` * $offset` |
|       - | 3467 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3468 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3469 | ` * $length (optional, nullable)` |
|       - | 3470 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3471 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3472 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3473 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3474 | ` * $preserve_keys (optional)` |
|       - | 3475 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3476 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3477 | ` * Return` |
|       - | 3478 | ` *   The new slice.` |
|       - | 3479 | ` */` |
|      46 | 3480 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3481 |  |
|       - | 3482 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3483 | `	ph7_hashmap_node *pCur;` |
|       - | 3484 | `	ph7_value *pArray;` |
|       - | 3485 | `	int iLength,iOfft;` |
|       - | 3486 | `	int bPreserve;` |
|       - | 3487 | `	sxi32 rc;` |
|      48 | 3488 | `	if( nArg < 2 ){` |
|       7 | 3489 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3490 | `			"ArgumentCountError",` |
|       - | 3491 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3492 | `			nArg` |
|       - | 3493 | `			);` |
|       - | 3494 | `	}` |
|      44 | 3495 | `	if( nArg > 4 ){` |
|       4 | 3496 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3497 | `			"ArgumentCountError",` |
|       - | 3498 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3499 | `			nArg` |
|       - | 3500 | `			);` |
|       - | 3501 | `	}` |
|      42 | 3502 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3503 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3504 | `			"TypeError",` |
|       - | 3505 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3506 | `			ph7_type_name(apArg[0])` |
|       - | 3507 | `			);` |
|       - | 3508 | `	}` |
|       - | 3509 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3510 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3511 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3512 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3513 | `			"TypeError",` |
|       - | 3514 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3515 | `			ph7_type_name(apArg[1])` |
|       - | 3516 | `			);` |
|       - | 3517 | `	}` |
|       - | 3518 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3519 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3520 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3521 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3522 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3523 | `				"TypeError",` |
|       - | 3524 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3525 | `				ph7_type_name(apArg[2])` |
|       - | 3526 | `				);` |
|       - | 3527 | `		}` |
|       8 | 3528 | `	}` |
|       - | 3529 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3530 | `	if( nArg > 3 ){` |
|      10 | 3531 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3532 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3533 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3534 | `				"TypeError",` |
|       - | 3535 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3536 | `				ph7_type_name(apArg[3])` |
|       - | 3537 | `				);` |
|       - | 3538 | `		}` |
|       2 | 3539 | `	}` |
|       - | 3540 | `	/* Point the internal representation of the target array */` |
|      33 | 3541 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3542 | `	bPreserve = FALSE;` |
|       - | 3543 | `	/* Get the offset */` |
|      33 | 3544 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3545 | `	if( iOfft < 0 ){` |
|       5 | 3546 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3547 | `		if( iOfft < 0 ){` |
|       3 | 3548 | `			iOfft = 0;` |
|       1 | 3549 | `		}` |
|       2 | 3550 | `	}` |
|      33 | 3551 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3552 | `		/* Offset past end of array, return empty array */` |
|       5 | 3553 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3554 | `		if( pArray == 0 ){` |
|     ! 0 | 3555 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3556 | `			return PH7_OK;` |
|       - | 3557 | `		}` |
|       5 | 3558 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3559 | `		return PH7_OK;` |
|       - | 3560 | `	}` |
|       - | 3561 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3562 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3563 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3564 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3565 | `		if( iLength < 0 ){` |
|       5 | 3566 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3567 | `		}` |
|      15 | 3568 | `		if( iLength < 0 ){` |
|       3 | 3569 | `			iLength = 0;` |
|       1 | 3570 | `		}` |
|      15 | 3571 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3572 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3573 | `		}` |
|       7 | 3574 | `	}` |
|      29 | 3575 | `	if( nArg > 3 ){` |
|       5 | 3576 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3577 | `	}` |
|       - | 3578 | `	/* Create a new array */` |
|      29 | 3579 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3580 | `	if( pArray == 0 ){` |
|     ! 0 | 3581 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3582 | `		return PH7_OK;` |
|       - | 3583 | `	}` |
|      29 | 3584 | `	if( iLength < 1 ){` |
|       - | 3585 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3586 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3587 | `		return PH7_OK;` |
|       - | 3588 | `	}` |
|       - | 3589 | `	/* Point to the desired entry */` |
|      25 | 3590 | `	pCur = pSrc->pFirst;` |
|      24 | 3591 | `	for(;;){` |
|      49 | 3592 | `		if( iOfft < 1 ){` |
|      25 | 3593 | `			break;` |
|       - | 3594 | `		}` |
|       - | 3595 | `		/* Point to the next entry */` |
|      25 | 3596 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3597 | `		iOfft--;` |
|       1 | 3598 | `	}` |
|       - | 3599 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3600 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3601 | `	for(;;){` |
|      79 | 3602 | `		if( iLength < 1 ){` |
|      25 | 3603 | `			break;` |
|       - | 3604 | `		}` |
|       - | 3605 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3606 | `		{` |
|      55 | 3607 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3608 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3609 | `		}` |
|      55 | 3610 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3611 | `			break;` |
|       - | 3612 | `		}` |
|       - | 3613 | `		/* Point to the next entry */` |
|      55 | 3614 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3615 | `		iLength--;` |
|       1 | 3616 | `	}` |
|       - | 3617 | `	/* Return the freshly created array */` |
|      25 | 3618 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3619 | `	return PH7_OK;` |
|      25 | 3620 |  |
|       - | 3621 | `/*` |
|       - | 3622 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3623 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3624 | ` * beginning (becomes the new pFirst).` |
|       - | 3625 | ` */` |
|      30 | 3626 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3627 |  |
|       - | 3628 | `	ph7_hashmap_node *pNode;` |
|       - | 3629 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3630 | `	pNode = pMap->pLast;` |
|      31 | 3631 | `	if( pNode == 0 ){` |
|     ! 0 | 3632 | `		return;` |
|       - | 3633 | `	}` |
|      31 | 3634 | `	if( pNode->pNext == 0 ){` |
|       - | 3635 | `		/* Only node in the list, nothing to move */` |
|       5 | 3636 | `		return;` |
|       - | 3637 | `	}` |
|      27 | 3638 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3639 | `		/* Already in the correct position */` |
|       9 | 3640 | `		return;` |
|       - | 3641 | `	}` |
|       - | 3642 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3643 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3644 | `	pMap->pLast->pPrev = 0;` |
|       - | 3645 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3646 | `	if( pAfter == 0 ){` |
|       - | 3647 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3648 | `		pNode->pNext = 0;` |
|       3 | 3649 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3650 | `		if( pMap->pFirst ){` |
|       3 | 3651 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3652 | `		}` |
|       3 | 3653 | `		pMap->pFirst = pNode;` |
|       2 | 3654 | `	}else{` |
|      17 | 3655 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3656 | `		pNode->pPrev = pOldNext;` |
|      17 | 3657 | `		pNode->pNext = pAfter;` |
|      17 | 3658 | `		pAfter->pPrev = pNode;` |
|      17 | 3659 | `		if( pOldNext ){` |
|      17 | 3660 | `			pOldNext->pNext = pNode;` |
|       9 | 3661 | `		}else{` |
|     ! 0 | 3662 | `			pMap->pLast = pNode;` |
|       - | 3663 | `		}` |
|       - | 3664 | `	}` |
|      16 | 3665 |  |
|       - | 3666 | `/*` |
|       - | 3667 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3668 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3669 | ` * Parameters` |
|       - | 3670 | ` *  $array` |
|       - | 3671 | ` *    The input array.` |
|       - | 3672 | ` *  $offset` |
|       - | 3673 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3674 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3675 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3676 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3677 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3678 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3679 | ` *  $length (optional)` |
|       - | 3680 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3681 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3682 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3683 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3684 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3685 | ` *  $replacement (optional)` |
|       - | 3686 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3687 | ` *    with elements from this array.` |
|       - | 3688 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3689 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3690 | ` *    offset.` |
|       - | 3691 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3692 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3693 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3694 | ` * Return` |
|       - | 3695 | ` *   A new array consisting of the extracted elements.` |
|       - | 3696 | ` */` |
|      54 | 3697 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3698 |  |
|       - | 3699 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3700 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3701 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3702 | `	int iLength,iOfft,i;` |
|       - | 3703 | `	sxi32 rc;` |
|      56 | 3704 | `	if( nArg < 2 ){` |
|       7 | 3705 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3706 | `			"ArgumentCountError",` |
|       - | 3707 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3708 | `			nArg` |
|       - | 3709 | `			);` |
|       - | 3710 | `	}` |
|      52 | 3711 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3712 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3713 | `			"TypeError",` |
|       - | 3714 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3715 | `			ph7_type_name(apArg[0])` |
|       - | 3716 | `			);` |
|       - | 3717 | `	}` |
|       - | 3718 | `	/* Point to the internal representation of the target array */` |
|      49 | 3719 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3720 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3721 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3722 | `	if( iOfft < 0 ){` |
|       7 | 3723 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3724 | `		if( iOfft < 0 ){` |
|       3 | 3725 | `			iOfft = 0;` |
|       2 | 3726 | `		}` |
|      46 | 3727 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3728 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3729 | `	}` |
|       - | 3730 | `	/* Get the length and clamp to valid range.` |
|       - | 3731 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3732 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3733 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3734 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3735 | `		if( iLength < 0 ){` |
|       7 | 3736 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3737 | `			if( iLength < 0 ){` |
|       3 | 3738 | `				iLength = 0;` |
|       1 | 3739 | `			}` |
|       3 | 3740 | `		}` |
|      31 | 3741 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3742 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3743 | `		}` |
|      15 | 3744 | `	}` |
|       - | 3745 | `	/* Create the result array for removed elements */` |
|      49 | 3746 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3747 | `	if( pArray == 0 ){` |
|     ! 0 | 3748 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3749 | `		return PH7_OK;` |
|       - | 3750 | `	}` |
|       - | 3751 | `	/* Get replacement array if provided */` |
|      49 | 3752 | `	pRep = 0;` |
|      49 | 3753 | `	if( nArg > 3 ){` |
|      21 | 3754 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3755 | `			/* Perform an array cast */` |
|       3 | 3756 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3757 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3758 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3759 | `			}` |
|       2 | 3760 | `		}else{` |
|      19 | 3761 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3762 | `		}` |
|      21 | 3763 | `		if( pRep ){` |
|       - | 3764 | `			/* Reset the loop cursor */` |
|      21 | 3765 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3766 | `		}` |
|      10 | 3767 | `	}` |
|       - | 3768 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3769 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3770 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3771 | `		return PH7_OK;` |
|       - | 3772 | `	}` |
|       - | 3773 | `	/* Navigate to the offset position */` |
|      41 | 3774 | `	pCur = pSrc->pFirst;` |
|      85 | 3775 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3776 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3777 | `	}` |
|       - | 3778 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3779 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3780 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3781 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3782 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3783 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3784 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3785 | `		pPrev = pCur->pPrev;` |
|      71 | 3786 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3787 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3788 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3789 | `			break;` |
|       - | 3790 | `		}` |
|      71 | 3791 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3792 | `	}` |
|       - | 3793 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3794 | `	if( pRep ){` |
|       - | 3795 | `		ph7_value sSafeVal;` |
|      61 | 3796 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3797 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3798 | `			if( pRvalue ){` |
|       - | 3799 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3800 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3801 | `				 * since it points into that same pool. */` |
|      31 | 3802 | `				sSafeVal = *pRvalue;` |
|      31 | 3803 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 3804 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 3805 | `					pNewNode = pSrc->pLast;` |
|      31 | 3806 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 3807 | `					pInsertAfter = pNewNode;` |
|      15 | 3808 | `				}` |
|      15 | 3809 | `			}` |
|       1 | 3810 | `		}` |
|      10 | 3811 | `	}` |
|       - | 3812 | `	/* Return the freshly created array */` |
|      41 | 3813 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 3814 | `	return PH7_OK;` |
|      29 | 3815 |  |
|       - | 3816 | `/*` |
|       - | 3817 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3818 | ` *  Checks if a value exists in an array.` |
|       - | 3819 | ` * Parameters` |
|       - | 3820 | ` *  $needle` |
|       - | 3821 | ` *   The searched value.` |
|       - | 3822 | ` *   Note:` |
|       - | 3823 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3824 | ` * $haystack` |
|       - | 3825 | ` *  The target array.` |
|       - | 3826 | ` * $strict` |
|       - | 3827 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3828 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3829 | ` */` |
|   22154 | 3830 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3831 |  |
|       - | 3832 | `	ph7_value *pNeedle;` |
|       - | 3833 | `	int bStrict;` |
|       - | 3834 | `	int rc;` |
|   22156 | 3835 | `	if( nArg < 2 ){` |
|       - | 3836 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3837 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3838 | `		return PH7_OK;` |
|       - | 3839 | `	}` |
|   22156 | 3840 | `	pNeedle = apArg[0];` |
|   22156 | 3841 | `	bStrict = 0;` |
|   22156 | 3842 | `	if( nArg > 2 ){` |
|       5 | 3843 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3844 | `	}` |
|   22156 | 3845 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3846 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3847 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3848 | `		/* Set the comparison result */` |
|     ! 0 | 3849 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3850 | `		return PH7_OK;` |
|       - | 3851 | `	}` |
|       - | 3852 | `	/* Perform the lookup */` |
|   22156 | 3853 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3854 | `	/* Lookup result */` |
|   22156 | 3855 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   22156 | 3856 | `	return PH7_OK;` |
|   11079 | 3857 |  |
|       - | 3858 | `/*` |
|       - | 3859 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3860 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3861 | ` * Parameters` |
|       - | 3862 | ` * $needle` |
|       - | 3863 | ` *   The searched value.` |
|       - | 3864 | ` * $haystack` |
|       - | 3865 | ` *   The array.` |
|       - | 3866 | ` * $strict` |
|       - | 3867 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3868 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3869 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3870 | ` * Return` |
|       - | 3871 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3872 | ` */` |
|      28 | 3873 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3874 |  |
|       - | 3875 | `	ph7_hashmap_node *pEntry;` |
|       - | 3876 | `	ph7_value *pVal,sNeedle;` |
|       - | 3877 | `	ph7_hashmap *pMap;` |
|       - | 3878 | `	ph7_value sVal;` |
|       - | 3879 | `	int bStrict;` |
|       - | 3880 | `	sxu32 n;` |
|       - | 3881 | `	int rc;` |
|      30 | 3882 | `	if( nArg < 2 ){` |
|       - | 3883 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3884 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3885 | `			"ArgumentCountError",` |
|       - | 3886 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 3887 | `			nArg` |
|       - | 3888 | `			);` |
|       - | 3889 | `	}` |
|      26 | 3890 | `	bStrict = FALSE;` |
|      26 | 3891 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3892 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3893 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3894 | `			"TypeError",` |
|       - | 3895 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 3896 | `			ph7_type_name(apArg[1])` |
|       - | 3897 | `			);` |
|       - | 3898 | `	}` |
|      24 | 3899 | `	if( nArg > 2 ){` |
|       - | 3900 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 3901 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3902 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3903 | `				"TypeError",` |
|       - | 3904 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3905 | `				ph7_type_name(apArg[2])` |
|       - | 3906 | `				);` |
|       - | 3907 | `		}` |
|       9 | 3908 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 3909 | `	}` |
|       - | 3910 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 3911 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3912 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 3913 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 3914 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 3915 | `	pEntry = pMap->pFirst;` |
|      21 | 3916 | `	n = pMap->nEntry;` |
|      23 | 3917 | `	for(;;){` |
|      47 | 3918 | `		if( !n ){` |
|       9 | 3919 | `			break;` |
|       - | 3920 | `		}` |
|       - | 3921 | `		/* Extract node value */` |
|      39 | 3922 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 3923 | `		if( pVal ){` |
|       - | 3924 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3925 | `			 * can change their type.` |
|       - | 3926 | `			 */` |
|      39 | 3927 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 3928 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 3929 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 3930 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 3931 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 3932 | `			if( rc == 0 ){` |
|       - | 3933 | `				/* Match found,return key */` |
|      13 | 3934 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3935 | `					/* INT key */` |
|       7 | 3936 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 3937 | `				}else{` |
|       7 | 3938 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3939 | `					/* Blob key */` |
|       7 | 3940 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3941 | `				}` |
|      13 | 3942 | `				return PH7_OK;` |
|       - | 3943 | `			}` |
|      13 | 3944 | `		}` |
|       - | 3945 | `		/* Point to the next entry */` |
|      27 | 3946 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 3947 | `		n--;` |
|       1 | 3948 | `	}` |
|       - | 3949 | `	/* No such value,return FALSE */` |
|       9 | 3950 | `	ph7_result_bool(pCtx,0);` |
|       9 | 3951 | `	return PH7_OK;` |
|      16 | 3952 |  |
|       - | 3953 | `/*` |
|       - | 3954 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3955 | ` *  Computes the difference of arrays.` |
|       - | 3956 | ` * Parameters` |
|       - | 3957 | ` *  $array1` |
|       - | 3958 | ` *    The array to compare from` |
|       - | 3959 | ` *  $array2` |
|       - | 3960 | ` *    An array to compare against` |
|       - | 3961 | ` *  $...` |
|       - | 3962 | ` *   More arrays to compare against` |
|       - | 3963 | ` * Return` |
|       - | 3964 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3965 | ` *  are not present in any of the other arrays.` |
|       - | 3966 | ` */` |
|      22 | 3967 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3968 |  |
|       - | 3969 | `	ph7_hashmap_node *pEntry;` |
|       - | 3970 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3971 | `	ph7_value *pArray;` |
|       - | 3972 | `	ph7_value *pVal;` |
|       - | 3973 | `	sxi32 rc;` |
|       - | 3974 | `	sxu32 n;` |
|       - | 3975 | `	int i;` |
|       - | 3976 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3977 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3978 | `	 * debugging difficult. */` |
|      24 | 3979 | `	if( nArg < 1 ){` |
|       4 | 3980 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3981 | `			"ArgumentCountError",` |
|       - | 3982 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3983 | `			nArg` |
|       - | 3984 | `			);` |
|       - | 3985 | `	}` |
|      22 | 3986 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3987 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3988 | `			"TypeError",` |
|       - | 3989 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3990 | `			ph7_type_name(apArg[0])` |
|       - | 3991 | `			);` |
|       - | 3992 | `	}` |
|      36 | 3993 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3994 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3995 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3996 | `				"TypeError",` |
|       - | 3997 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3998 | `				i + 1,` |
|       2 | 3999 | `				ph7_type_name(apArg[i])` |
|       - | 4000 | `				);` |
|       - | 4001 | `		}` |
|       9 | 4002 | `	}` |
|      17 | 4003 | `	if( nArg == 1 ){` |
|       - | 4004 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4005 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4006 | `		return PH7_OK;` |
|       - | 4007 | `	}` |
|       - | 4008 | `	/* Create a new array */` |
|      15 | 4009 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4010 | `	if( pArray == 0 ){` |
|     ! 0 | 4011 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4012 | `		return PH7_OK;` |
|       - | 4013 | `	}` |
|       - | 4014 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4015 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4016 | `	/* Perform the diff */` |
|      15 | 4017 | `	pEntry = pSrc->pFirst;` |
|      15 | 4018 | `	n = pSrc->nEntry;` |
|      27 | 4019 | `	for(;;){` |
|      55 | 4020 | `		if( n < 1 ){` |
|      15 | 4021 | `			break;` |
|       - | 4022 | `		}` |
|       - | 4023 | `		/* Extract the node value */` |
|      41 | 4024 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4025 | `		if( pVal ){` |
|      69 | 4026 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4027 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4028 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4029 | `				/* Perform the lookup */` |
|      45 | 4030 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4031 | `				if( rc == SXRET_OK ){` |
|       - | 4032 | `					/* Value exist */` |
|      17 | 4033 | `					break;` |
|       - | 4034 | `				}` |
|      15 | 4035 | `			}` |
|      41 | 4036 | `			if( i >= nArg ){` |
|       - | 4037 | `				/* Perform the insertion */` |
|      25 | 4038 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4039 | `			}` |
|      20 | 4040 | `		}` |
|       - | 4041 | `		/* Point to the next entry */` |
|      41 | 4042 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4043 | `		n--;` |
|       1 | 4044 | `	}` |
|       - | 4045 | `	/* Return the freshly created array */` |
|      15 | 4046 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4047 | `	return PH7_OK;` |
|      13 | 4048 |  |
|       - | 4049 | `/*` |
|       - | 4050 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4051 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4052 | ` * Parameters` |
|       - | 4053 | ` *  $array1` |
|       - | 4054 | ` *    The array to compare from` |
|       - | 4055 | ` *  $array2` |
|       - | 4056 | ` *    An array to compare against` |
|       - | 4057 | ` *  $...` |
|       - | 4058 | ` *   More arrays to compare against.` |
|       - | 4059 | ` * $callback` |
|       - | 4060 | ` *  The callback comparison function.` |
|       - | 4061 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4062 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4063 | ` *  than the second.` |
|       - | 4064 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4065 | ` * Return` |
|       - | 4066 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4067 | ` *  are not present in any of the other arrays.` |
|       - | 4068 | ` */` |
|      20 | 4069 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4070 |  |
|       - | 4071 | `	ph7_hashmap_node *pEntry;` |
|       - | 4072 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4073 | `	ph7_value *pCallback;` |
|       - | 4074 | `	ph7_value *pArray;` |
|       - | 4075 | `	ph7_value *pVal;` |
|       - | 4076 | `	sxi32 rc;` |
|       - | 4077 | `	sxu32 n;` |
|       - | 4078 | `	int i;` |
|       - | 4079 |  |
|       - | 4080 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      22 | 4081 | `	if( nArg < 2 ){` |
|       4 | 4082 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4083 | `			"ArgumentCountError",` |
|       - | 4084 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4085 | `			nArg` |
|       - | 4086 | `			);` |
|       - | 4087 | `	}` |
|      20 | 4088 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4089 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4090 | `			"TypeError",` |
|       - | 4091 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4092 | `			ph7_type_name(apArg[0])` |
|       - | 4093 | `			);` |
|       - | 4094 | `	}` |
|       - | 4095 |  |
|      18 | 4096 | `	if( nArg == 2 ){` |
|       - | 4097 | `		/* Only the original array and the callback were provided. */` |
|       - | 4098 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4099 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4100 | `		 * validation order.` |
|       - | 4101 | `		 */` |
|       4 | 4102 | `	} else {` |
|       - | 4103 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      20 | 4104 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      14 | 4105 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      10 | 4106 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4107 | `					"TypeError",` |
|       - | 4108 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4109 | `					i + 1,` |
|       6 | 4110 | `					ph7_type_name(apArg[i])` |
|       - | 4111 | `					);` |
|       - | 4112 | `			}` |
|       5 | 4113 | `		}` |
|       - | 4114 | `	}` |
|       - | 4115 |  |
|       - | 4116 | `	/* Identify the callback (always expected as the last argument). */` |
|      12 | 4117 | `	pCallback = apArg[nArg - 1];` |
|       - | 4118 | `	/* Validate the callback to match PHP's error messages. */` |
|      12 | 4119 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       7 | 4120 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4121 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4122 | `				"TypeError",` |
|       - | 4123 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4124 | `				nArg` |
|       - | 4125 | `				);` |
|       - | 4126 | `		}` |
|       5 | 4127 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4128 | `			int len;` |
|       3 | 4129 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4130 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4131 | `				"TypeError",` |
|       - | 4132 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4133 | `				nArg,` |
|       1 | 4134 | `				zName` |
|       - | 4135 | `				);` |
|       - | 4136 | `		}` |
|       4 | 4137 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4138 | `			"TypeError",` |
|       - | 4139 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4140 | `			nArg` |
|       - | 4141 | `			);` |
|       - | 4142 | `	}` |
|       - | 4143 |  |
|       5 | 4144 | `	if( nArg == 2 ){` |
|       - | 4145 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4146 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4147 | `		return PH7_OK;` |
|       - | 4148 | `	}` |
|       - | 4149 |  |
|       - | 4150 | `	/* Create a new array */` |
|       3 | 4151 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4152 | `	if( pArray == 0 ){` |
|     ! 0 | 4153 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4154 | `		return PH7_OK;` |
|       - | 4155 | `	}` |
|       - | 4156 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4157 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4158 | `	/* Perform the diff */` |
|       3 | 4159 | `	pEntry = pSrc->pFirst;` |
|       3 | 4160 | `	n = pSrc->nEntry;` |
|       4 | 4161 | `	for(;;){` |
|       9 | 4162 | `		if( n < 1 ){` |
|       3 | 4163 | `			break;` |
|       - | 4164 | `		}` |
|       - | 4165 | `		/* Extract the node value */` |
|       7 | 4166 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4167 | `		if( pVal ){` |
|      11 | 4168 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4169 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4170 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4171 | `				/* Perform the lookup */` |
|       7 | 4172 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4173 | `				if( rc == SXRET_OK ){` |
|       - | 4174 | `					/* Value exist */` |
|       3 | 4175 | `					break;` |
|       - | 4176 | `				}` |
|       3 | 4177 | `			}` |
|       7 | 4178 | `			if( i >= (nArg - 1)){` |
|       - | 4179 | `				/* Perform the insertion */` |
|       5 | 4180 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4181 | `			}` |
|       3 | 4182 | `		}` |
|       - | 4183 | `		/* Point to the next entry */` |
|       7 | 4184 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4185 | `		n--;` |
|       1 | 4186 | `	}` |
|       - | 4187 | `	/* Return the freshly created array */` |
|       3 | 4188 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4189 | `	return PH7_OK;` |
|      12 | 4190 |  |
|       - | 4191 | `/*` |
|       - | 4192 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4193 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4194 | ` * Parameters` |
|       - | 4195 | ` *  $array1` |
|       - | 4196 | ` *    The array to compare from` |
|       - | 4197 | ` *  $array2` |
|       - | 4198 | ` *    An array to compare against` |
|       - | 4199 | ` *  $...` |
|       - | 4200 | ` *   More arrays to compare against` |
|       - | 4201 | ` * Return` |
|       - | 4202 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4203 | ` *  are not present in any of the other arrays.` |
|       - | 4204 | ` */` |
|      20 | 4205 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4206 |  |
|       - | 4207 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4208 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4209 | `	ph7_value *pArray;` |
|       - | 4210 | `	ph7_value *pVal;` |
|       - | 4211 | `	sxi32 rc;` |
|       - | 4212 | `	sxu32 n;` |
|       - | 4213 | `	int i;` |
|       - | 4214 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4215 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4216 | `	 * accompanying integration tests to pass. */` |
|      22 | 4217 | `	if( nArg < 1 ){` |
|       4 | 4218 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4219 | `			"ArgumentCountError",` |
|       - | 4220 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4221 | `			nArg` |
|       - | 4222 | `			);` |
|       - | 4223 | `	}` |
|      20 | 4224 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4225 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4226 | `			"TypeError",` |
|       - | 4227 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4228 | `			ph7_type_name(apArg[0])` |
|       - | 4229 | `			);` |
|       - | 4230 | `	}` |
|      32 | 4231 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4232 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4233 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4234 | `				"TypeError",` |
|       - | 4235 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4236 | `				i + 1,` |
|       4 | 4237 | `				ph7_type_name(apArg[i])` |
|       - | 4238 | `				);` |
|       - | 4239 | `		}` |
|       9 | 4240 | `	}` |
|      13 | 4241 | `	if( nArg == 1 ){` |
|       - | 4242 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4243 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4244 | `		return PH7_OK;` |
|       - | 4245 | `	}` |
|       - | 4246 | `	/* Create a new array */` |
|      11 | 4247 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4248 | `	if( pArray == 0 ){` |
|     ! 0 | 4249 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4250 | `		return PH7_OK;` |
|       - | 4251 | `	}` |
|       - | 4252 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4253 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4254 | `	/* Perform the diff */` |
|      11 | 4255 | `	pEntry = pSrc->pFirst;` |
|      11 | 4256 | `	n = pSrc->nEntry;` |
|      11 | 4257 | `	pN1 = pN2 = 0;` |
|      29 | 4258 | `	for(;;){` |
|       - | 4259 | `		int keep;` |
|      35 | 4260 | `		if( n < 1 ){` |
|      11 | 4261 | `			break;` |
|       - | 4262 | `		}` |
|       - | 4263 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4264 | `		keep = 1;` |
|      41 | 4265 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4266 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4267 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4268 | `			/* Perform a key lookup first */` |
|      29 | 4269 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4270 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4271 | `			}else{` |
|      17 | 4272 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4273 | `			}` |
|      29 | 4274 | `			if( rc != SXRET_OK ){` |
|       - | 4275 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4276 | `				continue;` |
|       - | 4277 | `			}` |
|       - | 4278 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4279 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4280 | `			if( pVal ){` |
|       - | 4281 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4282 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4283 | `				if( pVal2 ){` |
|      15 | 4284 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4285 | `					if( cmp == 0 ){` |
|       - | 4286 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4287 | `						keep = 0;` |
|      13 | 4288 | `						break;` |
|       - | 4289 | `					}` |
|       1 | 4290 | `				}` |
|       1 | 4291 | `			}` |
|       2 | 4292 | `		}` |
|      25 | 4293 | `		if( keep ){` |
|       - | 4294 | `			/* Perform the insertion */` |
|      13 | 4295 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4296 | `		}` |
|       - | 4297 | `		/* Point to the next entry */` |
|      25 | 4298 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4299 | `		n--;` |
|       1 | 4300 | `	}` |
|       - | 4301 | `	/* Return the freshly created array */` |
|      11 | 4302 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4303 | `	return PH7_OK;` |
|      12 | 4304 |  |
|       - | 4305 | `/*` |
|       - | 4306 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4307 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4308 | ` *  by a user supplied callback function.` |
|       - | 4309 | ` * Parameters` |
|       - | 4310 | ` *  $array1` |
|       - | 4311 | ` *    The array to compare from` |
|       - | 4312 | ` *  $array2` |
|       - | 4313 | ` *    An array to compare against` |
|       - | 4314 | ` *  $...` |
|       - | 4315 | ` *   More arrays to compare against.` |
|       - | 4316 | ` *  $key_compare_func` |
|       - | 4317 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4318 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4319 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4320 | ` * Return` |
|       - | 4321 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4322 | ` *  are not present in any of the other arrays.` |
|       - | 4323 | ` */` |
|      22 | 4324 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4325 |  |
|       - | 4326 | `	ph7_hashmap_node *pEntry;` |
|       - | 4327 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4328 | `	ph7_value *pCallback;` |
|       - | 4329 | `	ph7_value *pArray;` |
|       - | 4330 | `	sxi32 rc;` |
|       - | 4331 | `	sxu32 n;` |
|       - | 4332 | `	int i;` |
|       - | 4333 |  |
|       - | 4334 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4335 | `	if( nArg < 2 ){` |
|       4 | 4336 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4337 | `			"ArgumentCountError",` |
|       - | 4338 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4339 | `			nArg` |
|       - | 4340 | `			);` |
|       - | 4341 | `	}` |
|      22 | 4342 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4343 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4344 | `			"TypeError",` |
|       - | 4345 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4346 | `			ph7_type_name(apArg[0])` |
|       - | 4347 | `			);` |
|       - | 4348 | `	}` |
|       - | 4349 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4350 | `	 * expected to be a callback. */` |
|      32 | 4351 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4352 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4353 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4354 | `				"TypeError",` |
|       - | 4355 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4356 | `				i + 1,` |
|       2 | 4357 | `				ph7_type_name(apArg[i])` |
|       - | 4358 | `				);` |
|       - | 4359 | `		}` |
|       8 | 4360 | `	}` |
|       - | 4361 | `	/* Point to the callback value */` |
|      18 | 4362 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4363 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4364 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4365 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4366 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4367 | `		 * string given" which we also reproduce. */` |
|       7 | 4368 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4369 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4370 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4371 | `				"TypeError",` |
|       - | 4372 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4373 | `				nArg` |
|       - | 4374 | `				);` |
|       - | 4375 | `		}` |
|       5 | 4376 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4377 | `			/* neither array nor string */` |
|       7 | 4378 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4379 | `				"TypeError",` |
|       - | 4380 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4381 | `				nArg` |
|       - | 4382 | `				);` |
|       - | 4383 | `		}` |
|       - | 4384 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4385 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4386 | `			"TypeError",` |
|       - | 4387 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4388 | `			nArg,` |
|     ! 0 | 4389 | `			ph7_type_name(pCallback)` |
|       - | 4390 | `			);` |
|       - | 4391 | `	}` |
|      11 | 4392 | `	if( nArg == 2 ){` |
|       - | 4393 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4394 | `		 * input array. */` |
|       3 | 4395 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4396 | `		return PH7_OK;` |
|       - | 4397 | `	}` |
|       - | 4398 | `	/* Create a new array */` |
|       9 | 4399 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4400 | `	if( pArray == 0 ){` |
|     ! 0 | 4401 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4402 | `		return PH7_OK;` |
|       - | 4403 | `	}` |
|       - | 4404 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4405 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4406 | `	/* Perform the diff */` |
|       9 | 4407 | `	pEntry = pSrc->pFirst;` |
|       9 | 4408 | `	n = pSrc->nEntry;` |
|      20 | 4409 | `	for(;;){` |
|       - | 4410 | `		int keep;` |
|      25 | 4411 | `		if( n < 1 ){` |
|       9 | 4412 | `			break;` |
|       - | 4413 | `		}` |
|      17 | 4414 | `		keep = 1;` |
|      29 | 4415 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4416 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4417 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4418 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4419 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4420 | `			while( pIt ){` |
|       - | 4421 | `				/* build temporary key values for callback */` |
|       - | 4422 | `				ph7_value key1, key2, result;` |
|       - | 4423 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4424 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4425 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4426 | `				}else{` |
|       - | 4427 | `					SyString sStr;` |
|      31 | 4428 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4429 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4430 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4431 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4432 | `				}` |
|      31 | 4433 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4434 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4435 | `				}else{` |
|       - | 4436 | `					SyString sStr;` |
|      31 | 4437 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4438 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4439 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4440 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4441 | `				}` |
|      31 | 4442 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4443 | `				/* call user callback with (key1, key2) */` |
|       - | 4444 | `				{` |
|       - | 4445 | `					ph7_value *apK[2];` |
|      31 | 4446 | `					apK[0] = &key1;` |
|      31 | 4447 | `					apK[1] = &key2;` |
|      31 | 4448 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4449 | `				}` |
|      31 | 4450 | `				if( rc == SXRET_OK ){` |
|      31 | 4451 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4452 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4453 | `					}` |
|      31 | 4454 | `					if( result.x.iVal == 0 ){` |
|       - | 4455 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4456 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4457 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4458 | `						if( pVal1 && pVal2 ){` |
|      13 | 4459 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4460 | `								keep = 0;` |
|       9 | 4461 | `								PH7_MemObjRelease(&result);` |
|       - | 4462 | `								/* release keys too before breaking */` |
|       9 | 4463 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4464 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4465 | `								break;` |
|       - | 4466 | `							}` |
|       2 | 4467 | `						}` |
|       2 | 4468 | `					}` |
|      11 | 4469 | `				}` |
|      23 | 4470 | `				PH7_MemObjRelease(&result);` |
|      23 | 4471 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4472 | `				PH7_MemObjRelease(&key2);` |
|       - | 4473 | `				/* move to next node */` |
|      23 | 4474 | `				pIt = pIt->pPrev;` |
|      23 | 4475 | `				if( keep == 0 ) break;` |
|       1 | 4476 | `			}` |
|      21 | 4477 | `			if( keep == 0 ) break;` |
|       7 | 4478 | `		}` |
|      17 | 4479 | `		if( keep ){` |
|       - | 4480 | `			/* Perform the insertion */` |
|       9 | 4481 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4482 | `		}` |
|       - | 4483 | `		/* Point to the next entry */` |
|      17 | 4484 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4485 | `		n--;` |
|       1 | 4486 | `	}` |
|       - | 4487 | `	/* Return the freshly created array */` |
|       9 | 4488 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4489 | `	return PH7_OK;` |
|      13 | 4490 |  |
|       - | 4491 | `/*` |
|       - | 4492 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4493 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4494 | ` * Parameters` |
|       - | 4495 | ` *  $array1` |
|       - | 4496 | ` *    The array to compare from` |
|       - | 4497 | ` *  $array2` |
|       - | 4498 | ` *    An array to compare against` |
|       - | 4499 | ` *  $...` |
|       - | 4500 | ` *   More arrays to compare against` |
|       - | 4501 | ` * Return` |
|       - | 4502 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4503 | ` *  in any of the other arrays.` |
|       - | 4504 | ` * Note that NULL is returned on failure.` |
|       - | 4505 | ` */` |
|      14 | 4506 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4507 |  |
|       - | 4508 | `	ph7_hashmap_node *pEntry;` |
|       - | 4509 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4510 | `	ph7_value *pArray;` |
|       - | 4511 | `	sxi32 rc;` |
|       - | 4512 | `	sxu32 n;` |
|       - | 4513 | `	int i;` |
|       - | 4514 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4515 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4516 | `	 * helpers. */` |
|      16 | 4517 | `	if( nArg < 1 ){` |
|       4 | 4518 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4519 | `			"ArgumentCountError",` |
|       - | 4520 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4521 | `			nArg` |
|       - | 4522 | `			);` |
|       - | 4523 | `	}` |
|      14 | 4524 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4525 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4526 | `			"TypeError",` |
|       - | 4527 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4528 | `			ph7_type_name(apArg[0])` |
|       - | 4529 | `			);` |
|       - | 4530 | `	}` |
|      20 | 4531 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4532 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4533 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4534 | `				"TypeError",` |
|       - | 4535 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4536 | `				i + 1,` |
|       2 | 4537 | `				ph7_type_name(apArg[i])` |
|       - | 4538 | `				);` |
|       - | 4539 | `		}` |
|       5 | 4540 | `	}` |
|       9 | 4541 | `	if( nArg == 1 ){` |
|       - | 4542 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4543 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4544 | `		return PH7_OK;` |
|       - | 4545 | `	}` |
|       - | 4546 | `	/* Create a new array */` |
|       7 | 4547 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4548 | `	if( pArray == 0 ){` |
|     ! 0 | 4549 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4550 | `		return PH7_OK;` |
|       - | 4551 | `	}` |
|       - | 4552 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4553 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4554 | `	/* Perfrom the diff */` |
|       7 | 4555 | `	pEntry = pSrc->pFirst;` |
|       7 | 4556 | `	n = pSrc->nEntry;` |
|      12 | 4557 | `	for(;;){` |
|      25 | 4558 | `		if( n < 1 ){` |
|       7 | 4559 | `			break;` |
|       - | 4560 | `		}` |
|      31 | 4561 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4562 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4563 | `				/* ignore */` |
|     ! 0 | 4564 | `				continue;` |
|       - | 4565 | `			}` |
|      23 | 4566 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4567 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4568 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4569 | `				/* Blob lookup */` |
|      17 | 4570 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4571 | `			}else{` |
|       - | 4572 | `				/* Int lookup */` |
|       7 | 4573 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4574 | `			}` |
|      23 | 4575 | `			if( rc == SXRET_OK ){` |
|       - | 4576 | `				/* Key exists,break immediately */` |
|      11 | 4577 | `				break;` |
|       - | 4578 | `			}` |
|       7 | 4579 | `		}` |
|      19 | 4580 | `		if( i >= nArg ){` |
|       - | 4581 | `			/* Perform the insertion */` |
|       9 | 4582 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4583 | `		}` |
|       - | 4584 | `		/* Point to the next entry */` |
|      19 | 4585 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4586 | `		n--;` |
|       1 | 4587 | `	}` |
|       - | 4588 | `	/* Return the freshly created array */` |
|       7 | 4589 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4590 | `	return PH7_OK;` |
|       9 | 4591 |  |
|       - | 4592 | `/*` |
|       - | 4593 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4594 | ` *  Computes the intersection of arrays.` |
|       - | 4595 | ` * Parameters` |
|       - | 4596 | ` *  $array1` |
|       - | 4597 | ` *    The array to compare from` |
|       - | 4598 | ` *  $array2` |
|       - | 4599 | ` *    An array to compare against` |
|       - | 4600 | ` *  $...` |
|       - | 4601 | ` *   More arrays to compare against` |
|       - | 4602 | ` * Return` |
|       - | 4603 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4604 | ` *  in all of the parameters.` |
|       - | 4605 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4606 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4607 | ` */` |
|      22 | 4608 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4609 |  |
|       - | 4610 | `	ph7_hashmap_node *pEntry;` |
|       - | 4611 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4612 | `	ph7_value *pArray;` |
|       - | 4613 | `	ph7_value *pVal;` |
|       - | 4614 | `	sxi32 rc;` |
|       - | 4615 | `	sxu32 n;` |
|       - | 4616 | `	int i;` |
|      24 | 4617 | `	if( nArg < 1 ){` |
|       4 | 4618 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4619 | `			"ArgumentCountError",` |
|       - | 4620 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4621 | `			nArg` |
|       - | 4622 | `			);` |
|       - | 4623 | `	}` |
|      22 | 4624 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4625 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4626 | `			"TypeError",` |
|       - | 4627 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4628 | `			ph7_type_name(apArg[0])` |
|       - | 4629 | `			);` |
|       - | 4630 | `	}` |
|      36 | 4631 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4632 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4633 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4634 | `				"TypeError",` |
|       - | 4635 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4636 | `				i + 1,` |
|       2 | 4637 | `				ph7_type_name(apArg[i])` |
|       - | 4638 | `				);` |
|       - | 4639 | `		}` |
|       9 | 4640 | `	}` |
|      17 | 4641 | `	if( nArg == 1 ){` |
|       - | 4642 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4643 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4644 | `		return PH7_OK;` |
|       - | 4645 | `	}` |
|       - | 4646 | `	/* Create a new array */` |
|      15 | 4647 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4648 | `	if( pArray == 0 ){` |
|     ! 0 | 4649 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4650 | `		return PH7_OK;` |
|       - | 4651 | `	}` |
|       - | 4652 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4653 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4654 | `	/* Perform the intersection */` |
|      15 | 4655 | `	pEntry = pSrc->pFirst;` |
|      15 | 4656 | `	n = pSrc->nEntry;` |
|      31 | 4657 | `	for(;;){` |
|      63 | 4658 | `		if( n < 1 ){` |
|      15 | 4659 | `			break;` |
|       - | 4660 | `		}` |
|       - | 4661 | `		/* Extract the node value */` |
|      49 | 4662 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4663 | `		if( pVal ){` |
|      79 | 4664 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4665 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4666 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4667 | `				/* Perform the lookup */` |
|      55 | 4668 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4669 | `				if( rc != SXRET_OK ){` |
|       - | 4670 | `					/* Value does not exist */` |
|      25 | 4671 | `					break;` |
|       - | 4672 | `				}` |
|      16 | 4673 | `			}` |
|      49 | 4674 | `			if( i >= nArg ){` |
|       - | 4675 | `				/* Perform the insertion */` |
|      25 | 4676 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4677 | `			}` |
|      24 | 4678 | `		}` |
|       - | 4679 | `		/* Point to the next entry */` |
|      49 | 4680 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4681 | `		n--;` |
|       1 | 4682 | `	}` |
|       - | 4683 | `	/* Return the freshly created array */` |
|      15 | 4684 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4685 | `	return PH7_OK;` |
|      13 | 4686 |  |
|       - | 4687 | `/*` |
|       - | 4688 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4689 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4690 | ` * Parameters` |
|       - | 4691 | ` *  $array1` |
|       - | 4692 | ` *    The array to compare from` |
|       - | 4693 | ` *  $array2` |
|       - | 4694 | ` *    An array to compare against` |
|       - | 4695 | ` *  $...` |
|       - | 4696 | ` *   More arrays to compare against` |
|       - | 4697 | ` * Return` |
|       - | 4698 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4699 | ` *  in all the arguments, with matching keys.` |
|       - | 4700 | ` */` |
|      22 | 4701 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4702 |  |
|       - | 4703 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4704 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4705 | `	ph7_value *pArray;` |
|       - | 4706 | `	ph7_value *pVal;` |
|       - | 4707 | `	sxi32 rc;` |
|       - | 4708 | `	sxu32 n;` |
|       - | 4709 | `	int i;` |
|      24 | 4710 | `	if( nArg < 1 ){` |
|       4 | 4711 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4712 | `			"ArgumentCountError",` |
|       - | 4713 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4714 | `			nArg` |
|       - | 4715 | `			);` |
|       - | 4716 | `	}` |
|      22 | 4717 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4718 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4719 | `			"TypeError",` |
|       - | 4720 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4721 | `			ph7_type_name(apArg[0])` |
|       - | 4722 | `			);` |
|       - | 4723 | `	}` |
|      36 | 4724 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4725 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4726 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4727 | `				"TypeError",` |
|       - | 4728 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4729 | `				i + 1,` |
|       2 | 4730 | `				ph7_type_name(apArg[i])` |
|       - | 4731 | `				);` |
|       - | 4732 | `		}` |
|       9 | 4733 | `	}` |
|      17 | 4734 | `	if( nArg == 1 ){` |
|       - | 4735 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4736 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4737 | `		return PH7_OK;` |
|       - | 4738 | `	}` |
|       - | 4739 | `	/* Create a new array */` |
|      15 | 4740 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4741 | `	if( pArray == 0 ){` |
|     ! 0 | 4742 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4743 | `		return PH7_OK;` |
|       - | 4744 | `	}` |
|       - | 4745 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4746 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4747 | `	/* Perform the intersection */` |
|      15 | 4748 | `	pEntry = pSrc->pFirst;` |
|      15 | 4749 | `	n = pSrc->nEntry;` |
|      15 | 4750 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4751 | `	for(;;){` |
|      47 | 4752 | `		if( n < 1 ){` |
|      15 | 4753 | `			break;` |
|       - | 4754 | `		}` |
|       - | 4755 | `		/* Extract the node value */` |
|      33 | 4756 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4757 | `		if( pVal ){` |
|      53 | 4758 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4759 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4760 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4761 | `				/* Perform a key lookup first */` |
|      37 | 4762 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4763 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4764 | `				}else{` |
|      23 | 4765 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4766 | `				}` |
|      37 | 4767 | `				if( rc != SXRET_OK ){` |
|       - | 4768 | `					/* No such key,break immediately */` |
|       7 | 4769 | `					break;` |
|       - | 4770 | `				}` |
|       - | 4771 | `				/* Perform the lookup */` |
|      31 | 4772 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4773 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4774 | `					/* Value does not exist */` |
|       6 | 4775 | `					break;` |
|       - | 4776 | `				}` |
|      11 | 4777 | `			}` |
|      33 | 4778 | `			if( i >= nArg ){` |
|       - | 4779 | `				/* Perform the insertion */` |
|      17 | 4780 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4781 | `			}` |
|      16 | 4782 | `		}` |
|       - | 4783 | `		/* Point to the next entry */` |
|      33 | 4784 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4785 | `		n--;` |
|       1 | 4786 | `	}` |
|       - | 4787 | `	/* Return the freshly created array */` |
|      15 | 4788 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4789 | `	return PH7_OK;` |
|      13 | 4790 |  |
|       - | 4791 | `/*` |
|       - | 4792 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 4793 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4794 | ` * Parameters` |
|       - | 4795 | ` *  $array1` |
|       - | 4796 | ` *    The array to compare from` |
|       - | 4797 | ` *  $...` |
|       - | 4798 | ` *   More arrays to compare against` |
|       - | 4799 | ` * Return` |
|       - | 4800 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4801 | ` *  have keys that are present in all arguments.` |
|       - | 4802 | ` * Note that NULL is returned on failure.` |
|       - | 4803 | ` */` |
|      22 | 4804 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4805 |  |
|       - | 4806 | `	ph7_hashmap_node *pEntry;` |
|       - | 4807 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4808 | `	ph7_value *pArray;` |
|       - | 4809 | `	sxi32 rc;` |
|       - | 4810 | `	sxu32 n;` |
|       - | 4811 | `	int i;` |
|      24 | 4812 | `	if( nArg < 1 ){` |
|       4 | 4813 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4814 | `			"ArgumentCountError",` |
|       - | 4815 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 4816 | `			nArg` |
|       - | 4817 | `			);` |
|       - | 4818 | `	}` |
|      22 | 4819 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4820 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4821 | `			"TypeError",` |
|       - | 4822 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4823 | `			ph7_type_name(apArg[0])` |
|       - | 4824 | `			);` |
|       - | 4825 | `	}` |
|      36 | 4826 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4827 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4828 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4829 | `				"TypeError",` |
|       - | 4830 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4831 | `				i + 1,` |
|       2 | 4832 | `				ph7_type_name(apArg[i])` |
|       - | 4833 | `				);` |
|       - | 4834 | `		}` |
|       9 | 4835 | `	}` |
|      17 | 4836 | `	if( nArg == 1 ){` |
|       - | 4837 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4838 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4839 | `		return PH7_OK;` |
|       - | 4840 | `	}` |
|       - | 4841 | `	/* Create a new array */` |
|      15 | 4842 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4843 | `	if( pArray == 0 ){` |
|     ! 0 | 4844 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4845 | `		return PH7_OK;` |
|       - | 4846 | `	}` |
|       - | 4847 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 4848 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4849 | `	/* Perform the intersection */` |
|      15 | 4850 | `	pEntry = pSrc->pFirst;` |
|      15 | 4851 | `	n = pSrc->nEntry;` |
|      24 | 4852 | `	for(;;){` |
|      49 | 4853 | `		if( n < 1 ){` |
|      15 | 4854 | `			break;` |
|       - | 4855 | `		}` |
|      57 | 4856 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 4857 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 4858 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 4859 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4860 | `				/* Blob lookup */` |
|      27 | 4861 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 4862 | `			}else{` |
|       - | 4863 | `				/* Int key */` |
|      13 | 4864 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4865 | `			}` |
|      39 | 4866 | `			if( rc != SXRET_OK ){` |
|       - | 4867 | `				/* Key does not exist, break immediately */` |
|      17 | 4868 | `				break;` |
|       - | 4869 | `			}` |
|      12 | 4870 | `		}` |
|      35 | 4871 | `		if( i >= nArg ){` |
|       - | 4872 | `			/* Perform the insertion */` |
|      19 | 4873 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 4874 | `		}` |
|       - | 4875 | `		/* Point to the next entry */` |
|      35 | 4876 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 4877 | `		n--;` |
|       1 | 4878 | `	}` |
|       - | 4879 | `	/* Return the freshly created array */` |
|      15 | 4880 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4881 | `	return PH7_OK;` |
|      13 | 4882 |  |
|       - | 4883 | `/*` |
|       - | 4884 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4885 | ` *  Computes the intersection of arrays.` |
|       - | 4886 | ` * Parameters` |
|       - | 4887 | ` *  $array1` |
|       - | 4888 | ` *    The array to compare from` |
|       - | 4889 | ` *  $array2` |
|       - | 4890 | ` *    An array to compare against` |
|       - | 4891 | ` *  $...` |
|       - | 4892 | ` *   More arrays to compare against` |
|       - | 4893 | ` * $callback` |
|       - | 4894 | ` *  The callback comparison function.` |
|       - | 4895 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4896 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4897 | ` *  than the second.` |
|       - | 4898 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4899 | ` * Return` |
|       - | 4900 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4901 | ` *  in all of the parameters. .` |
|       - | 4902 | ` * Note that NULL is returned on failure.` |
|       - | 4903 | ` */` |
|      24 | 4904 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4905 |  |
|       - | 4906 | `	ph7_hashmap_node *pEntry;` |
|       - | 4907 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4908 | `	ph7_value *pCallback;` |
|       - | 4909 | `	ph7_value *pArray;` |
|       - | 4910 | `	ph7_value *pVal;` |
|       - | 4911 | `	sxi32 rc;` |
|       - | 4912 | `	sxu32 n;` |
|       - | 4913 | `	int i;` |
|       - | 4914 |  |
|       - | 4915 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      26 | 4916 | `	if( nArg < 2 ){` |
|       4 | 4917 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4918 | `			"ArgumentCountError",` |
|       - | 4919 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 4920 | `			nArg` |
|       - | 4921 | `			);` |
|       - | 4922 | `	}` |
|      24 | 4923 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4924 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4925 | `			"TypeError",` |
|       - | 4926 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4927 | `			ph7_type_name(apArg[0])` |
|       - | 4928 | `			);` |
|       - | 4929 | `	}` |
|       - | 4930 |  |
|      22 | 4931 | `	if( nArg == 2 ){` |
|       - | 4932 | `		/* Only the original array and the callback were provided. */` |
|       - | 4933 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 4934 | `		 * validation ordering. */` |
|       3 | 4935 | `	} else {` |
|       - | 4936 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      32 | 4937 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      18 | 4938 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4939 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4940 | `					"TypeError",` |
|       - | 4941 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4942 | `					i + 1,` |
|       2 | 4943 | `					ph7_type_name(apArg[i])` |
|       - | 4944 | `					);` |
|       - | 4945 | `			}` |
|       9 | 4946 | `		}` |
|       - | 4947 | `	}` |
|       - | 4948 |  |
|       - | 4949 | `	/* Identify the callback (always expected as the last argument). */` |
|      20 | 4950 | `	pCallback = apArg[nArg - 1];` |
|       - | 4951 | `	/* Validate the callback to match PHP's error messages. */` |
|      20 | 4952 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      11 | 4953 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4954 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 4955 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 4956 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 4957 | `			 */` |
|       7 | 4958 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       7 | 4959 | `			if( pCb->nEntry != 2 ){` |
|       4 | 4960 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4961 | `					"TypeError",` |
|       - | 4962 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4963 | `					nArg` |
|       - | 4964 | `					);` |
|       - | 4965 | `			}` |
|       - | 4966 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 4967 | `			{` |
|       5 | 4968 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       5 | 4969 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       5 | 4970 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 4971 | `					int nMethodLen;` |
|       5 | 4972 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       5 | 4973 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       5 | 4974 | `					if( pClass ){` |
|       - | 4975 | `						/* Class exists but method is missing. */` |
|       4 | 4976 | `						return PH7_VmThrowException(pCtx,` |
|       - | 4977 | `							"TypeError",` |
|       - | 4978 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 4979 | `							nArg,` |
|       1 | 4980 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 4981 | `							zMethod` |
|       - | 4982 | `							);` |
|       - | 4983 | `					}` |
|       - | 4984 | `					/* Class not found */` |
|       - | 4985 | `					{` |
|       - | 4986 | `						int nName;` |
|       3 | 4987 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 4988 | `						return PH7_VmThrowException(pCtx,` |
|       - | 4989 | `							"TypeError",` |
|       - | 4990 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 4991 | `							nArg,` |
|       1 | 4992 | `							zName` |
|       - | 4993 | `							);` |
|       - | 4994 | `					}` |
|       - | 4995 | `				}` |
|       - | 4996 | `			}` |
|       - | 4997 | `			/* Fallback message */` |
|     ! 0 | 4998 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4999 | `				"TypeError",` |
|       - | 5000 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5001 | `				nArg` |
|       - | 5002 | `				);` |
|       - | 5003 | `		}` |
|       5 | 5004 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5005 | `			int len;` |
|       3 | 5006 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5007 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5008 | `				"TypeError",` |
|       - | 5009 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5010 | `				nArg,` |
|       1 | 5011 | `				zName` |
|       - | 5012 | `				);` |
|       - | 5013 | `		}` |
|       4 | 5014 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5015 | `			"TypeError",` |
|       - | 5016 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5017 | `			nArg` |
|       - | 5018 | `			);` |
|       - | 5019 | `	}` |
|       - | 5020 |  |
|       9 | 5021 | `	if( nArg == 2 ){` |
|       - | 5022 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5023 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5024 | `		return PH7_OK;` |
|       - | 5025 | `	}` |
|       - | 5026 |  |
|       - | 5027 | `	/* Create a new array */` |
|       5 | 5028 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 5029 | `	if( pArray == 0 ){` |
|     ! 0 | 5030 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5031 | `		return PH7_OK;` |
|       - | 5032 | `	}` |
|       - | 5033 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 5034 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5035 | `	/* Perform the intersection */` |
|       5 | 5036 | `	pEntry = pSrc->pFirst;` |
|       5 | 5037 | `	n = pSrc->nEntry;` |
|       8 | 5038 | `	for(;;){` |
|      17 | 5039 | `		if( n < 1 ){` |
|       5 | 5040 | `			break;` |
|       - | 5041 | `		}` |
|       - | 5042 | `		/* Extract the node value */` |
|      13 | 5043 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      13 | 5044 | `		if( pVal ){` |
|      21 | 5045 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      13 | 5046 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5047 | `					/* ignore */` |
|     ! 0 | 5048 | `					continue;` |
|       - | 5049 | `				}` |
|       - | 5050 | `				/* Point to the internal representation of the hashmap */` |
|      13 | 5051 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5052 | `				/* Perform the lookup */` |
|      13 | 5053 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      13 | 5054 | `				if( rc != SXRET_OK ){` |
|       - | 5055 | `					/* Value does not exist */` |
|       5 | 5056 | `					break;` |
|       - | 5057 | `				}` |
|       5 | 5058 | `			}` |
|      13 | 5059 | `			if( i >= (nArg-1) ){` |
|       - | 5060 | `				/* Perform the insertion */` |
|       9 | 5061 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5062 | `			}` |
|       6 | 5063 | `		}` |
|       - | 5064 | `		/* Point to the next entry */` |
|      13 | 5065 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5066 | `		n--;` |
|       1 | 5067 | `	}` |
|       - | 5068 | `	/* Return the freshly created array */` |
|       5 | 5069 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5070 | `	return PH7_OK;` |
|      14 | 5071 |  |
|       - | 5072 | `/*` |
|       - | 5073 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5074 | ` *  Fill an array with values.` |
|       - | 5075 | ` * Parameters` |
|       - | 5076 | ` *  $start_index` |
|       - | 5077 | ` *    The first index of the returned array.` |
|       - | 5078 | ` *  $num` |
|       - | 5079 | ` *   Number of elements to insert.` |
|       - | 5080 | ` *  $value` |
|       - | 5081 | ` *    Value to use for filling.` |
|       - | 5082 | ` * Return` |
|       - | 5083 | ` *  The filled array or null on failure.` |
|       - | 5084 | ` */` |
|     238 | 5085 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5086 |  |
|       - | 5087 | `	ph7_value *pArray;` |
|       - | 5088 | `	int i,nEntry;` |
|       - | 5089 |  |
|       - | 5090 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 5091 | `	if( nArg != 3 ){` |
|       - | 5092 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 5093 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5094 | `			"ArgumentCountError",` |
|       - | 5095 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5096 | `			nArg` |
|       - | 5097 | `			);` |
|       - | 5098 | `	}` |
|       - | 5099 |  |
|       - | 5100 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5101 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5102 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5103 | `	 * and NULLs are rejected outright. */` |
|     466 | 5104 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 5105 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5106 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5107 | `			"TypeError",` |
|       - | 5108 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5109 | `			ph7_type_name(apArg[0])` |
|       - | 5110 | `			);` |
|       - | 5111 | `	}` |
|     234 | 5112 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5113 | `		int len;` |
|       8 | 5114 | `		sxu8 bReal = FALSE;` |
|       8 | 5115 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5116 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5117 | `			/* Non‑numeric string is an error. */` |
|       3 | 5118 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5119 | `				"TypeError",` |
|       - | 5120 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5121 | `				);` |
|       - | 5122 | `		}` |
|       5 | 5123 | `		if( bReal ){` |
|       - | 5124 | `			/* float-string -> deprecation warning */` |
|       4 | 5125 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5126 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5127 | `				zStr` |
|       - | 5128 | `				);` |
|       1 | 5129 | `		}` |
|       2 | 5130 | `	}` |
|       - | 5131 |  |
|       - | 5132 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5133 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 5134 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 5135 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5136 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5137 | `			"TypeError",` |
|       - | 5138 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5139 | `			ph7_type_name(apArg[1])` |
|       - | 5140 | `			);` |
|       - | 5141 | `	}` |
|     232 | 5142 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5143 | `		int len;` |
|       3 | 5144 | `		sxu8 bReal = FALSE;` |
|       3 | 5145 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5146 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5147 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5148 | `				"TypeError",` |
|       - | 5149 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5150 | `				);` |
|       - | 5151 | `		}` |
|     ! 0 | 5152 | `	}` |
|       - | 5153 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5154 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5155 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5156 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5157 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5158 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5159 | `		if( d != (double)i64 ){` |
|       7 | 5160 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5161 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5162 | `				d` |
|       - | 5163 | `				);` |
|       2 | 5164 | `		}` |
|       2 | 5165 | `	}` |
|       - | 5166 |  |
|       - | 5167 | `	/* Total number of entries to insert */` |
|     230 | 5168 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5169 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5170 | `	if( nEntry < 0 ){` |
|       3 | 5171 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5172 | `			"ValueError",` |
|       - | 5173 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5174 | `			);` |
|       - | 5175 | `	}` |
|       - | 5176 |  |
|       - | 5177 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5178 | `	if( nEntry == 0 ){` |
|       7 | 5179 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5180 | `		return PH7_OK;` |
|       - | 5181 | `	}` |
|       - | 5182 |  |
|       - | 5183 | `	/* Create a new array */` |
|     221 | 5184 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5185 | `	if( pArray == 0 ){` |
|     ! 0 | 5186 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5187 | `		return PH7_OK;` |
|       - | 5188 | `	}` |
|       - | 5189 |  |
|       - | 5190 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5191 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 5192 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5193 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5194 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 5195 | `	}` |
|       - | 5196 | `	/* Return the filled array */` |
|     221 | 5197 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5198 | `	return PH7_OK;` |
|     121 | 5199 |  |
|       - | 5200 | `/*` |
|       - | 5201 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5202 | ` *  Fill an array with values, specifying keys.` |
|       - | 5203 | ` * Parameters` |
|       - | 5204 | ` *  $input` |
|       - | 5205 | ` *   Array of values that will be used as key.` |
|       - | 5206 | ` *  $value` |
|       - | 5207 | ` *    Value to use for filling.` |
|       - | 5208 | ` * Return` |
|       - | 5209 | ` *  The filled array.` |
|       - | 5210 | ` * Throws` |
|       - | 5211 | ` *  ValueError if $input is not an array.` |
|       - | 5212 | ` */` |
|      26 | 5213 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5214 |  |
|       - | 5215 | `	ph7_hashmap_node *pEntry;` |
|       - | 5216 | `	ph7_hashmap *pSrc;` |
|       - | 5217 | `	ph7_value *pArray;` |
|       - | 5218 | `	sxu32 n;` |
|       - | 5219 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5220 | `	if( nArg != 2 ){` |
|      10 | 5221 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5222 | `			"ArgumentCountError",` |
|       - | 5223 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5224 | `			nArg` |
|       - | 5225 | `			);` |
|       - | 5226 | `	}` |
|       - | 5227 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5228 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5229 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5230 | `			"TypeError",` |
|       - | 5231 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5232 | `			ph7_type_name(apArg[0])` |
|       - | 5233 | `			);` |
|       - | 5234 | `	}` |
|       - | 5235 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5236 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5237 | `	/* Create a new array */` |
|      17 | 5238 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5239 | `	if( pArray == 0 ){` |
|     ! 0 | 5240 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5241 | `		return PH7_OK;` |
|       - | 5242 | `	}` |
|       - | 5243 | `	/* Perform the requested operation */` |
|      17 | 5244 | `	pEntry = pSrc->pFirst;` |
|      45 | 5245 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5246 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5247 | `		/* Point to the next entry */` |
|      29 | 5248 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5249 | `	}` |
|       - | 5250 | `	/* Return the filled array */` |
|      17 | 5251 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5252 | `	return PH7_OK;` |
|      15 | 5253 |  |
|       - | 5254 | `/*` |
|       - | 5255 | ` * array array_combine(array $keys,array $values)` |
|       - | 5256 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5257 | ` * Parameters` |
|       - | 5258 | ` *  $keys` |
|       - | 5259 | ` *    Array of keys to be used.` |
|       - | 5260 | ` * $values` |
|       - | 5261 | ` *   Array of values to be used.` |
|       - | 5262 | ` * Return` |
|       - | 5263 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5264 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5265 | ` *  not an array.` |
|       - | 5266 | ` */` |
|      18 | 5267 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5268 |  |
|       - | 5269 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5270 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5271 | `	ph7_value *pArray;` |
|       - | 5272 | `	sxu32 n;` |
|       - | 5273 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5274 | `	if( nArg != 2 ){` |
|       - | 5275 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5276 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5277 | `			"ArgumentCountError",` |
|       - | 5278 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5279 | `			nArg` |
|       - | 5280 | `			);` |
|       - | 5281 | `	}` |
|       - | 5282 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5283 | `	 * argument index in the error message. */` |
|      18 | 5284 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5285 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5286 | `			"TypeError",` |
|       - | 5287 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5288 | `			ph7_type_name(apArg[0])` |
|       - | 5289 | `			);` |
|       - | 5290 | `	}` |
|      16 | 5291 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5292 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5293 | `			"TypeError",` |
|       - | 5294 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5295 | `			ph7_type_name(apArg[1])` |
|       - | 5296 | `			);` |
|       - | 5297 | `	}` |
|       - | 5298 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5299 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5300 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5301 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5302 | `		/* Length mismatch -> ValueError */` |
|       3 | 5303 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5304 | `			"ValueError",` |
|       - | 5305 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5306 | `			);` |
|       - | 5307 | `	}` |
|       - | 5308 | `	/* Create a new array */` |
|      11 | 5309 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5310 | `	if( pArray == 0 ){` |
|     ! 0 | 5311 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5312 | `		return PH7_OK;` |
|       - | 5313 | `	}` |
|       - | 5314 | `	/* Perform the requested operation */` |
|      11 | 5315 | `	pKe = pKey->pFirst;` |
|      11 | 5316 | `	pVe = pValue->pFirst;` |
|      33 | 5317 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5318 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5319 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5320 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5321 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5322 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5323 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5324 | `		 * original array must not be mutated. */` |
|      23 | 5325 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5326 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5327 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5328 | `			if( pTmpKey ){` |
|       5 | 5329 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5330 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5331 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5332 | `				pKeyCopy = pTmpKey;` |
|       2 | 5333 | `			}` |
|       2 | 5334 | `		}` |
|      23 | 5335 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5336 | `		/* Point to the next entry */` |
|      23 | 5337 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5338 | `		pVe = pVe->pPrev;` |
|      12 | 5339 | `	}` |
|       - | 5340 | `	/* Return the filled array */` |
|      11 | 5341 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5342 | `	return PH7_OK;` |
|      11 | 5343 |  |
|       - | 5344 | `/*` |
|       - | 5345 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5346 | ` *  Return an array with elements in reverse order.` |
|       - | 5347 | ` * Parameters` |
|       - | 5348 | ` *  $array` |
|       - | 5349 | ` *   The input array.` |
|       - | 5350 | ` *  $preserve_keys (optional)` |
|       - | 5351 | ` *   If set to TRUE keys are preserved.` |
|       - | 5352 | ` * Return` |
|       - | 5353 | ` *  The reversed array.` |
|       - | 5354 | ` */` |
|      20 | 5355 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5356 |  |
|       - | 5357 | `	ph7_hashmap_node *pEntry;` |
|       - | 5358 | `	ph7_hashmap *pSrc;` |
|       - | 5359 | `	ph7_value *pArray;` |
|       - | 5360 | `	int bPreserve;` |
|       - | 5361 | `	sxu32 n;` |
|      22 | 5362 | `	if( nArg < 1 ){` |
|       4 | 5363 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5364 | `			"ArgumentCountError",` |
|       - | 5365 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5366 | `			nArg` |
|       - | 5367 | `			);` |
|       - | 5368 | `	}` |
|       - | 5369 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5370 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5371 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5372 | `			"TypeError",` |
|       - | 5373 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5374 | `			ph7_type_name(apArg[0])` |
|       - | 5375 | `			);` |
|       - | 5376 | `	}` |
|      17 | 5377 | `	bPreserve = FALSE;` |
|      17 | 5378 | `	if( nArg > 1 ){` |
|       7 | 5379 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5380 | `	}` |
|       - | 5381 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5382 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5383 | `	/* Create a new array */` |
|      17 | 5384 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5385 | `	if( pArray == 0 ){` |
|     ! 0 | 5386 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5387 | `		return PH7_OK;` |
|       - | 5388 | `	}` |
|       - | 5389 | `	/* Perform the requested operation */` |
|      17 | 5390 | `	pEntry = pSrc->pLast;` |
|      55 | 5391 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5392 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5393 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5394 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5395 | `		/* Point to the previous entry */` |
|      39 | 5396 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5397 | `	}` |
|      17 | 5398 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5399 | `	return PH7_OK;` |
|      12 | 5400 |  |
|       - | 5401 | `/*` |
|       - | 5402 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5403 | ` *  Removes duplicate values from an array.` |
|       - | 5404 | ` * Parameters` |
|       - | 5405 | ` *  $array` |
|       - | 5406 | ` *   The input array.` |
|       - | 5407 | ` *  $flags` |
|       - | 5408 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5409 | ` *   behavior using these values:` |
|       - | 5410 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5411 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5412 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5413 | ` * Return` |
|       - | 5414 | ` *  The filtered array.` |
|       - | 5415 | ` */` |
|      24 | 5416 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5417 |  |
|       - | 5418 | `	ph7_hashmap_node *pEntry;` |
|       - | 5419 | `	ph7_value *pNeedle;` |
|       - | 5420 | `	ph7_hashmap *pSrc;` |
|       - | 5421 | `	ph7_value *pArray;` |
|       - | 5422 | `	int bStrict;` |
|       - | 5423 | `	sxi32 rc;` |
|       - | 5424 | `	sxu32 n;` |
|      26 | 5425 | `	if( nArg < 1 ){` |
|       - | 5426 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5427 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5428 | `			"ArgumentCountError",` |
|       - | 5429 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5430 | `			);` |
|       - | 5431 | `	}` |
|      24 | 5432 | `	if( nArg > 2 ){` |
|       - | 5433 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5434 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5435 | `			"ArgumentCountError",` |
|       - | 5436 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5437 | `			nArg` |
|       - | 5438 | `			);` |
|       - | 5439 | `	}` |
|       - | 5440 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5441 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5442 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5443 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5444 | `			"TypeError",` |
|       - | 5445 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5446 | `			ph7_type_name(apArg[0])` |
|       - | 5447 | `			);` |
|       - | 5448 | `	}` |
|      19 | 5449 | `	bStrict = FALSE;` |
|       - | 5450 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5451 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5452 | `	/* Create a new array */` |
|      19 | 5453 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5454 | `	if( pArray == 0 ){` |
|     ! 0 | 5455 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5456 | `		return PH7_OK;` |
|       - | 5457 | `	}` |
|       - | 5458 | `	/* Perform the requested operation */` |
|      19 | 5459 | `	pEntry = pSrc->pFirst;` |
|      83 | 5460 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5461 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5462 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5463 | `		if( pNeedle ){` |
|      65 | 5464 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5465 | `		}` |
|      65 | 5466 | `		if( rc != SXRET_OK ){` |
|       - | 5467 | `			/* Perform the insertion */` |
|      37 | 5468 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5469 | `		}` |
|       - | 5470 | `		/* Point to the next entry */` |
|      65 | 5471 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5472 | `	}` |
|       - | 5473 | `	/* Return the freshly created array */` |
|      19 | 5474 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5475 | `	return PH7_OK;` |
|      14 | 5476 |  |
|       - | 5477 | `/*` |
|       - | 5478 | ` * array array_flip(array $input)` |
|       - | 5479 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5480 | ` * Parameter` |
|       - | 5481 | ` *  $input` |
|       - | 5482 | ` *   Input array.` |
|       - | 5483 | ` * Return` |
|       - | 5484 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5485 | ` */` |
|      34 | 5486 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5487 |  |
|       - | 5488 | `	ph7_hashmap_node *pEntry;` |
|       - | 5489 | `	ph7_hashmap *pSrc;` |
|       - | 5490 | `	ph7_value *pArray;` |
|       - | 5491 | `	ph7_value *pKey;` |
|       - | 5492 | `	ph7_value sVal;` |
|       - | 5493 | `	sxu32 n;` |
|       - | 5494 |  |
|       - | 5495 | `	/* PHP requires exactly one argument */` |
|      36 | 5496 | `	if( nArg != 1 ){` |
|       - | 5497 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5498 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5499 | `			"ArgumentCountError",` |
|       - | 5500 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5501 | `			nArg` |
|       - | 5502 | `			);` |
|       - | 5503 | `	}` |
|       - | 5504 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5505 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5506 | `		/* Type mismatch -> TypeError */` |
|       7 | 5507 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5508 | `			"TypeError",` |
|       - | 5509 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5510 | `			ph7_type_name(apArg[0])` |
|       - | 5511 | `			);` |
|       - | 5512 | `	}` |
|       - | 5513 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5514 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5515 | `	/* Create a new array */` |
|      27 | 5516 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5517 | `	if( pArray == 0 ){` |
|     ! 0 | 5518 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5519 | `		return PH7_OK;` |
|       - | 5520 | `	}` |
|       - | 5521 | `	/* Start processing */` |
|      27 | 5522 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5523 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5524 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5525 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5526 | `		if( pKey ){` |
|       - | 5527 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5528 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5529 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5530 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5531 | `					);` |
|   22236 | 5532 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5533 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5534 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5535 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5536 | `				}else{` |
|       - | 5537 | `					SyString sStr;` |
|    2227 | 5538 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5539 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5540 | `				}` |
|       - | 5541 | `				/* Perform the insertion */` |
|   22227 | 5542 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5543 | `				/* Safely release the value because each inserted entry` |
|       - | 5544 | `				 * has its own private copy of the value.` |
|       - | 5545 | `				 */` |
|   22227 | 5546 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5547 | `			}else{` |
|       - | 5548 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5549 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5550 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5551 | `					);` |
|       - | 5552 | `			}` |
|   11118 | 5553 | `		}` |
|       - | 5554 | `		/* Point to the next entry */` |
|   22237 | 5555 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5556 | `	}` |
|       - | 5557 | `	/* Return the freshly created array */` |
|      27 | 5558 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5559 | `	return PH7_OK;` |
|      19 | 5560 |  |
|       - | 5561 | `/*` |
|       - | 5562 | ` * number array_sum(array $array )` |
|       - | 5563 | ` *  Calculate the sum of values in an array.` |
|       - | 5564 | ` * Parameters` |
|       - | 5565 | ` *  $array: The input array.` |
|       - | 5566 | ` * Return` |
|       - | 5567 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5568 | ` */` |
|      24 | 5569 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5570 |  |
|       - | 5571 | `	ph7_hashmap_node *pEntry;` |
|       - | 5572 | `	ph7_value *pObj;` |
|      25 | 5573 | `	double dSum = 0;` |
|       - | 5574 | `	sxu32 n;` |
|      25 | 5575 | `	pEntry = pMap->pFirst;` |
|      91 | 5576 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5577 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5578 | `		if( pObj ){` |
|      67 | 5579 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5580 | `				dSum += pObj->rVal;` |
|      53 | 5581 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5582 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5583 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5584 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5585 | `					double dv = 0;` |
|      13 | 5586 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5587 | `					dSum += dv;` |
|       7 | 5588 | `				}` |
|      12 | 5589 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5590 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5591 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5592 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5593 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5594 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5595 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5596 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5597 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5598 | `			}` |
|       - | 5599 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5600 | `		}` |
|       - | 5601 | `		/* Point to the next entry */` |
|      67 | 5602 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5603 | `	}` |
|       - | 5604 | `	/* Return sum */` |
|      25 | 5605 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5606 |  |
|      18 | 5607 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5608 |  |
|       - | 5609 | `	ph7_hashmap_node *pEntry;` |
|       - | 5610 | `	ph7_value *pObj;` |
|      20 | 5611 | `	sxi64 nSum = 0;` |
|       - | 5612 | `	sxu32 n;` |
|      20 | 5613 | `	pEntry = pMap->pFirst;` |
|      80 | 5614 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5615 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5616 | `		if( pObj ){` |
|      62 | 5617 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5618 | `				nSum += pObj->x.iVal;` |
|      36 | 5619 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5620 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5621 | `					sxi64 nv = 0;` |
|       5 | 5622 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5623 | `					nSum += nv;` |
|       3 | 5624 | `				}` |
|       8 | 5625 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5626 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5627 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5628 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5629 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5630 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5631 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5632 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5633 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5634 | `			}` |
|       - | 5635 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5636 | `		}` |
|       - | 5637 | `		/* Point to the next entry */` |
|      62 | 5638 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5639 | `	}` |
|       - | 5640 | `	/* Return sum */` |
|      20 | 5641 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5642 |  |
|       - | 5643 | `/* number array_sum(array $array )` |
|       - | 5644 | ` * (See block-coment above)` |
|       - | 5645 | ` */` |
|      52 | 5646 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5647 |  |
|       - | 5648 | `	ph7_hashmap_node *pEntry;` |
|       - | 5649 | `	ph7_hashmap *pMap;` |
|       - | 5650 | `	ph7_value *pObj;` |
|      54 | 5651 | `	int useDouble = 0;` |
|       - | 5652 | `	sxu32 n;` |
|       - | 5653 | `	/* PHP requires exactly one argument */` |
|      54 | 5654 | `	if( nArg != 1 ){` |
|       7 | 5655 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5656 | `			"ArgumentCountError",` |
|       - | 5657 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5658 | `			nArg` |
|       - | 5659 | `			);` |
|       - | 5660 | `	}` |
|       - | 5661 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5662 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5663 | `		/* Type mismatch -> TypeError */` |
|       7 | 5664 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5665 | `			"TypeError",` |
|       - | 5666 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5667 | `			ph7_type_name(apArg[0])` |
|       - | 5668 | `			);` |
|       - | 5669 | `	}` |
|      46 | 5670 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5671 | `	if( pMap->nEntry < 1 ){` |
|       - | 5672 | `		/* Nothing to compute,return 0 */` |
|       3 | 5673 | `		ph7_result_int(pCtx,0);` |
|       3 | 5674 | `		return PH7_OK;` |
|       - | 5675 | `	}` |
|       - | 5676 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5677 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5678 | `	 */` |
|      44 | 5679 | `	pEntry = pMap->pFirst;` |
|     112 | 5680 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5681 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5682 | `		if( pObj ){` |
|      94 | 5683 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5684 | `				useDouble = 1;` |
|      19 | 5685 | `				break;` |
|       - | 5686 | `			}` |
|      76 | 5687 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5688 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5689 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5690 | `				sxu32 i;` |
|      23 | 5691 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5692 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5693 | `						useDouble = 1;` |
|       7 | 5694 | `						break;` |
|       - | 5695 | `					}` |
|       6 | 5696 | `				}` |
|      13 | 5697 | `				if( useDouble ){` |
|       7 | 5698 | `					break;` |
|       - | 5699 | `				}` |
|       3 | 5700 | `			}` |
|      34 | 5701 | `		}` |
|      70 | 5702 | `		pEntry = pEntry->pPrev;` |
|      36 | 5703 | `	}` |
|      44 | 5704 | `	if( useDouble ){` |
|      25 | 5705 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5706 | `	}else{` |
|      20 | 5707 | `		Int64Sum(pCtx,pMap);` |
|       - | 5708 | `	}` |
|      44 | 5709 | `	return PH7_OK;` |
|      28 | 5710 |  |
|       - | 5711 | `/*` |
|       - | 5712 | ` * number array_product(array $array )` |
|       - | 5713 | ` *  Calculate the product of values in an array.` |
|       - | 5714 | ` * Parameters` |
|       - | 5715 | ` *  $array: The input array.` |
|       - | 5716 | ` * Return` |
|       - | 5717 | ` *  Returns the product of values as an integer or float.` |
|       - | 5718 | ` */` |
|     ! 0 | 5719 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5720 |  |
|       - | 5721 | `	ph7_hashmap_node *pEntry;` |
|       - | 5722 | `	ph7_value *pObj;` |
|       - | 5723 | `	double dProd;` |
|       - | 5724 | `	sxu32 n;` |
|     ! 0 | 5725 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5726 | `	dProd = 1;` |
|     ! 0 | 5727 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5728 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5729 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5730 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5731 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5732 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5733 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5734 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5735 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5736 | `					double dv = 0;` |
|     ! 0 | 5737 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5738 | `					dProd *= dv;` |
|     ! 0 | 5739 | `				}` |
|     ! 0 | 5740 | `			}` |
|     ! 0 | 5741 | `		}` |
|       - | 5742 | `		/* Point to the next entry */` |
|     ! 0 | 5743 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5744 | `	}` |
|       - | 5745 | `	/* Return product */` |
|     ! 0 | 5746 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5747 |  |
|     ! 0 | 5748 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5749 |  |
|       - | 5750 | `	ph7_hashmap_node *pEntry;` |
|       - | 5751 | `	ph7_value *pObj;` |
|       - | 5752 | `	sxi64 nProd;` |
|       - | 5753 | `	sxu32 n;` |
|     ! 0 | 5754 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5755 | `	nProd = 1;` |
|     ! 0 | 5756 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5757 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5758 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5759 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5760 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5761 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5762 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5763 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5764 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5765 | `					sxi64 nv = 0;` |
|     ! 0 | 5766 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5767 | `					nProd *= nv;` |
|     ! 0 | 5768 | `				}` |
|     ! 0 | 5769 | `			}` |
|     ! 0 | 5770 | `		}` |
|       - | 5771 | `		/* Point to the next entry */` |
|     ! 0 | 5772 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5773 | `	}` |
|       - | 5774 | `	/* Return product */` |
|     ! 0 | 5775 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5776 |  |
|       - | 5777 | `/* number array_product(array $array )` |
|       - | 5778 | ` * (See block-block comment above)` |
|       - | 5779 | ` */` |
|     ! 0 | 5780 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5781 |  |
|       - | 5782 | `	ph7_hashmap *pMap;` |
|       - | 5783 | `	ph7_value *pObj;` |
|     ! 0 | 5784 | `	if( nArg < 1 ){` |
|       - | 5785 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5786 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5787 | `		return PH7_OK;` |
|       - | 5788 | `	}` |
|       - | 5789 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5790 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5791 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5792 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5793 | `		return PH7_OK;` |
|       - | 5794 | `	}` |
|     ! 0 | 5795 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5796 | `	if( pMap->nEntry < 1 ){` |
|       - | 5797 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5798 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5799 | `		return PH7_OK;` |
|       - | 5800 | `	}` |
|       - | 5801 | `	/* If the first element is of type float,then perform floating` |
|       - | 5802 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5803 | `	 */` |
|     ! 0 | 5804 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5805 | `	if( pObj == 0 ){` |
|     ! 0 | 5806 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5807 | `		return PH7_OK;` |
|       - | 5808 | `	}` |
|     ! 0 | 5809 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5810 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5811 | `	}else{` |
|     ! 0 | 5812 | `		Int64Prod(pCtx,pMap);` |
|       - | 5813 | `	}` |
|     ! 0 | 5814 | `	return PH7_OK;` |
|     ! 0 | 5815 |  |
|       - | 5816 | `/*` |
|       - | 5817 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5818 | ` *  Pick one or more random entries out of an array.` |
|       - | 5819 | ` * Parameters` |
|       - | 5820 | ` * $input` |
|       - | 5821 | ` *  The input array.` |
|       - | 5822 | ` * $num_req` |
|       - | 5823 | ` *  Specifies how many entries you want to pick.` |
|       - | 5824 | ` * Return` |
|       - | 5825 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5826 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5827 | ` *  NULL is returned on failure.` |
|       - | 5828 | ` */` |
|       6 | 5829 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5830 |  |
|       - | 5831 | `	ph7_hashmap_node *pNode;` |
|       - | 5832 | `	ph7_hashmap *pMap;` |
|       7 | 5833 | `	int nItem = 1;` |
|       7 | 5834 | `	if( nArg < 1 ){` |
|       - | 5835 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5836 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5837 | `		return PH7_OK;` |
|       - | 5838 | `	}` |
|       - | 5839 | `	/* Make sure we are dealing with an array */` |
|       7 | 5840 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5841 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5842 | `		return PH7_OK;` |
|       - | 5843 | `	}` |
|       - | 5844 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5845 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5846 | `	if(pMap->nEntry < 1 ){` |
|       - | 5847 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5848 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5849 | `		return PH7_OK;` |
|       - | 5850 | `	}` |
|       7 | 5851 | `	if( nArg > 1 ){` |
|       3 | 5852 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5853 | `	}` |
|       7 | 5854 | `	if( nItem < 2 ){` |
|       - | 5855 | `		sxu32 nEntry;` |
|       - | 5856 | `		/* Select a random number */` |
|       5 | 5857 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5858 | `		/* Extract the desired entry.` |
|       - | 5859 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5860 | `		 */` |
|       5 | 5861 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       2 | 5862 | `			pNode = pMap->pLast;` |
|       2 | 5863 | `			nEntry = pMap->nEntry - nEntry;` |
|       2 | 5864 | `			if( nEntry > 1 ){` |
|     ! 0 | 5865 | `				for(;;){` |
|     ! 0 | 5866 | `					if( nEntry == 0 ){` |
|     ! 0 | 5867 | `						break;` |
|       - | 5868 | `					}` |
|       - | 5869 | `					/* Point to the previous entry */` |
|     ! 0 | 5870 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5871 | `					nEntry--;` |
|     ! 0 | 5872 | `				}` |
|     ! 0 | 5873 | `			}` |
|       1 | 5874 | `		}else{` |
|       3 | 5875 | `			pNode = pMap->pFirst;` |
|       1 | 5876 | `			for(;;){` |
|       4 | 5877 | `				if( nEntry == 0 ){` |
|       3 | 5878 | `					break;` |
|       - | 5879 | `				}` |
|       - | 5880 | `				/* Point to the next entry */` |
|       1 | 5881 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 5882 | `				nEntry--;` |
|     ! 0 | 5883 | `			}` |
|       - | 5884 | `		}` |
|       5 | 5885 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5886 | `			/* Int key */` |
|       3 | 5887 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5888 | `		}else{` |
|       - | 5889 | `			/* Blob key */` |
|       3 | 5890 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5891 | `		}` |
|       3 | 5892 | `	}else{` |
|       - | 5893 | `		ph7_value sKey,*pArray;` |
|       - | 5894 | `		ph7_hashmap *pDest;` |
|       - | 5895 | `		/* Create a new array */` |
|       3 | 5896 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5897 | `		if( pArray == 0 ){` |
|     ! 0 | 5898 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5899 | `			return PH7_OK;` |
|       - | 5900 | `		}` |
|       - | 5901 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5902 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5903 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5904 | `		/* Copy the first n items */` |
|       3 | 5905 | `		pNode = pMap->pFirst;` |
|       3 | 5906 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5907 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5908 | `		}` |
|       7 | 5909 | `		while( nItem > 0){` |
|       5 | 5910 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5911 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5912 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5913 | `			/* Point to the next entry */` |
|       5 | 5914 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5915 | `			nItem--;` |
|       1 | 5916 | `		}` |
|       - | 5917 | `		/* Shuffle the array */` |
|       3 | 5918 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5919 | `		/* Rehash node */` |
|       3 | 5920 | `		HashmapSortRehash(pDest);` |
|       - | 5921 | `		/* Return the random array */` |
|       3 | 5922 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5923 | `	}` |
|       7 | 5924 | `	return PH7_OK;` |
|       4 | 5925 |  |
|       - | 5926 | `/*` |
|       - | 5927 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5928 | ` *  Split an array into chunks.` |
|       - | 5929 | ` * Parameters` |
|       - | 5930 | ` * $input` |
|       - | 5931 | ` *   The array to work on` |
|       - | 5932 | ` * $size` |
|       - | 5933 | ` *   The size of each chunk` |
|       - | 5934 | ` * $preserve_keys` |
|       - | 5935 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5936 | ` *   the chunk numerically.` |
|       - | 5937 | ` * Return` |
|       - | 5938 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5939 | ` *  zero, with each dimension containing size elements.` |
|       - | 5940 | ` */` |
|      42 | 5941 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5942 |  |
|       - | 5943 | `	ph7_value *pArray,*pChunk;` |
|       - | 5944 | `	ph7_hashmap_node *pEntry;` |
|       - | 5945 | `	ph7_hashmap *pMap;` |
|       - | 5946 | `	int bPreserve;` |
|       - | 5947 | `	sxu32 nChunk;` |
|       - | 5948 | `	sxu32 nSize;` |
|       - | 5949 | `	sxu32 n;` |
|       - | 5950 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5951 | `	if( nArg < 2 ){` |
|       - | 5952 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5953 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5954 | `			"ArgumentCountError",` |
|       - | 5955 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5956 | `			nArg` |
|       - | 5957 | `			);` |
|       - | 5958 | `	}` |
|      42 | 5959 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5960 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5961 | `			"TypeError",` |
|       - | 5962 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5963 | `			ph7_type_name(apArg[0])` |
|       - | 5964 | `			);` |
|       - | 5965 | `	}` |
|       - | 5966 | `	/* Create a new array */` |
|      40 | 5967 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5968 | `	if( pArray == 0 ){` |
|     ! 0 | 5969 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5970 | `		return PH7_OK;` |
|       - | 5971 | `	}` |
|       - | 5972 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5973 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5974 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5975 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5976 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5977 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5978 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5979 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5980 | `			"TypeError",` |
|       - | 5981 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5982 | `			ph7_type_name(apArg[1])` |
|       - | 5983 | `			);` |
|       - | 5984 | `	}` |
|       - | 5985 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5986 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5987 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5988 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5989 | `		int len;` |
|       3 | 5990 | `		sxu8 bReal = FALSE;` |
|       3 | 5991 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5992 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5993 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5994 | `				"TypeError",` |
|       - | 5995 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5996 | `				);` |
|       - | 5997 | `		}` |
|     ! 0 | 5998 | `		if( bReal ){` |
|       - | 5999 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6000 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6001 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6002 | `				zStr` |
|       - | 6003 | `				);` |
|     ! 0 | 6004 | `		}` |
|     ! 0 | 6005 | `	}` |
|       - | 6006 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6007 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6008 | `	 * later via ph7_value_to_int. */` |
|      38 | 6009 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6010 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6011 | `		sxi64 i = (sxi64)d;` |
|       3 | 6012 | `		if( d != (double)i ){` |
|       4 | 6013 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6014 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6015 | `				d` |
|       - | 6016 | `				);` |
|       1 | 6017 | `		}` |
|       1 | 6018 | `	}` |
|       - | 6019 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6020 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6021 | `	{` |
|      38 | 6022 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 6023 | `		if( nSizeSigned < 1 ){` |
|       - | 6024 | `			/* size <= 0 -> ValueError */` |
|       5 | 6025 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6026 | `				"ValueError",` |
|       - | 6027 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6028 | `				);` |
|       - | 6029 | `		}` |
|      34 | 6030 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6031 | `	}` |
|      34 | 6032 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6033 | `		/* Return the whole array */` |
|       3 | 6034 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6035 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6036 | `		return PH7_OK;` |
|       - | 6037 | `	}` |
|      32 | 6038 | `	bPreserve = 0;` |
|      32 | 6039 | `	if( nArg > 2 ){` |
|       - | 6040 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6041 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6042 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6043 | `		 * normally, matching PHP behaviour. */` |
|      45 | 6044 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 6045 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6046 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 6047 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6048 | `				"TypeError",` |
|       - | 6049 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6050 | `				ph7_type_name(apArg[2])` |
|       - | 6051 | `				);` |
|       - | 6052 | `		}` |
|      21 | 6053 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6054 | `	}` |
|       - | 6055 | `	/* Start processing */` |
|      27 | 6056 | `	pEntry = pMap->pFirst;` |
|      27 | 6057 | `	nChunk = 0;` |
|      27 | 6058 | `	pChunk = 0;` |
|      27 | 6059 | `	n = pMap->nEntry;` |
|      56 | 6060 | `	for( ;; ){` |
|     113 | 6061 | `		if( n < 1 ){` |
|       - | 6062 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6063 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6064 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6065 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6066 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6067 | `			 * exists. */` |
|      27 | 6068 | `			if( pChunk ){` |
|      27 | 6069 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6070 | `			}` |
|      27 | 6071 | `			break;` |
|       - | 6072 | `		}` |
|      87 | 6073 | `		if( nChunk < 1 ){` |
|      71 | 6074 | `			if( pChunk ){` |
|       - | 6075 | `				/* Put the first chunk */` |
|      45 | 6076 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6077 | `			}` |
|       - | 6078 | `			/* Create a new dimension */` |
|      71 | 6079 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6080 | `												   * will be automatically released as soon we return` |
|       - | 6081 | `												   * from this function */` |
|      71 | 6082 | `			if( pChunk == 0 ){` |
|     ! 0 | 6083 | `				break;` |
|       - | 6084 | `			}` |
|      71 | 6085 | `			nChunk = nSize;` |
|      35 | 6086 | `		}` |
|       - | 6087 | `		/* Insert the entry */` |
|      87 | 6088 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6089 | `		/* Point to the next entry */` |
|      87 | 6090 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6091 | `		nChunk--;` |
|      87 | 6092 | `		n--;` |
|       1 | 6093 | `	}` |
|       - | 6094 | `	/* Return the multidimensional array */` |
|      27 | 6095 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6096 | `	return PH7_OK;` |
|      23 | 6097 |  |
|       - | 6098 | `/*` |
|       - | 6099 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6100 | ` *  Pad array to the specified length with a value.` |
|       - | 6101 | ` * $input` |
|       - | 6102 | ` *   Initial array of values to pad.` |
|       - | 6103 | ` * $pad_size` |
|       - | 6104 | ` *   New size of the array.` |
|       - | 6105 | ` * $pad_value` |
|       - | 6106 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6107 | ` */` |
|      28 | 6108 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6109 |  |
|       - | 6110 | `	ph7_hashmap *pMap;` |
|       - | 6111 | `	ph7_value *pArray;` |
|       - | 6112 | `	int nEntry;` |
|      30 | 6113 | `	if( nArg != 3 ){` |
|      10 | 6114 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6115 | `			"ArgumentCountError",` |
|       - | 6116 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6117 | `			nArg` |
|       - | 6118 | `			);` |
|       - | 6119 | `	}` |
|      24 | 6120 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6121 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6122 | `			"TypeError",` |
|       - | 6123 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6124 | `			ph7_type_name(apArg[0])` |
|       - | 6125 | `			);` |
|       - | 6126 | `	}` |
|       - | 6127 | `	/* Create a new array */` |
|      21 | 6128 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6129 | `	if( pArray == 0 ){` |
|     ! 0 | 6130 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6131 | `		return PH7_OK;` |
|       - | 6132 | `	}` |
|       - | 6133 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6134 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6135 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6136 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6137 | `	if( nEntry < 0 ){` |
|       9 | 6138 | `		nEntry = -nEntry;` |
|       9 | 6139 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6140 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6141 | `			/* Insert given items first */` |
|      17 | 6142 | `			while( nEntry > 0 ){` |
|      13 | 6143 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 6144 | `				nEntry--;` |
|       1 | 6145 | `			}` |
|       - | 6146 | `			/* Merge the two arrays */` |
|       5 | 6147 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6148 | `		}else{` |
|       5 | 6149 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6150 | `		}` |
|      17 | 6151 | `	}else if( nEntry > 0 ){` |
|      11 | 6152 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6153 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6154 | `			/* Merge the two arrays first */` |
|       7 | 6155 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6156 | `			/* Insert given items */` |
|      25 | 6157 | `			while( nEntry > 0 ){` |
|      19 | 6158 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 6159 | `				nEntry--;` |
|       1 | 6160 | `			}` |
|       4 | 6161 | `		}else{` |
|       5 | 6162 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6163 | `		}` |
|       6 | 6164 | `	}else{` |
|       - | 6165 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6166 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6167 | `	}` |
|       - | 6168 | `	/* Return the new array */` |
|      21 | 6169 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6170 | `	return PH7_OK;` |
|      16 | 6171 |  |
|       - | 6172 | `/*` |
|       - | 6173 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6174 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6175 | ` * Parameters` |
|       - | 6176 | ` * $array` |
|       - | 6177 | ` *   The array in which elements are replaced.` |
|       - | 6178 | ` * $array1` |
|       - | 6179 | ` *   The array from which elements will be extracted.` |
|       - | 6180 | ` * ....` |
|       - | 6181 | ` *  More arrays from which elements will be extracted.` |
|       - | 6182 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6183 | ` * Return` |
|       - | 6184 | ` *  Returns an array.` |
|       - | 6185 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6186 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6187 | ` */` |
|      22 | 6188 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6189 |  |
|       - | 6190 | `	ph7_hashmap *pMap;` |
|       - | 6191 | `	ph7_value *pArray;` |
|       - | 6192 | `	int i;` |
|      24 | 6193 | `	if( nArg < 1 ){` |
|       3 | 6194 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6195 | `			"ArgumentCountError",` |
|       - | 6196 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6197 | `			);` |
|       - | 6198 | `	}` |
|      22 | 6199 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6200 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6201 | `			"TypeError",` |
|       - | 6202 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6203 | `			ph7_type_name(apArg[0])` |
|       - | 6204 | `			);` |
|       - | 6205 | `	}` |
|       - | 6206 | `	/* Create a new array */` |
|      20 | 6207 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6208 | `	if( pArray == 0 ){` |
|     ! 0 | 6209 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6210 | `		return PH7_OK;` |
|       - | 6211 | `	}` |
|       - | 6212 | `	/* Overwrite from the first array */` |
|      20 | 6213 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6214 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6215 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6216 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6217 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6218 | `			/* Type mismatch -> TypeError */` |
|       4 | 6219 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6220 | `				"TypeError",` |
|       - | 6221 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6222 | `				i + 1,` |
|       2 | 6223 | `				ph7_type_name(apArg[i])` |
|       - | 6224 | `				);` |
|       - | 6225 | `		}` |
|       - | 6226 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6227 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6228 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6229 | `	}` |
|       - | 6230 | `	/* Return the new array */` |
|      17 | 6231 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6232 | `	return PH7_OK;` |
|      13 | 6233 |  |
|       - | 6234 | `/*` |
|       - | 6235 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6236 | ` *  Filters elements of an array using a callback function.` |
|       - | 6237 | ` * Parameters` |
|       - | 6238 | ` *  $input` |
|       - | 6239 | ` *    The array to iterate over` |
|       - | 6240 | ` * $callback` |
|       - | 6241 | ` *    The callback function to use` |
|       - | 6242 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6243 | ` *    will be removed.` |
|       - | 6244 | ` * Return` |
|       - | 6245 | ` *  The filtered array.` |
|       - | 6246 | ` */` |
|      18 | 6247 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6248 |  |
|       - | 6249 | `	ph7_hashmap_node *pEntry;` |
|       - | 6250 | `	ph7_hashmap *pMap;` |
|       - | 6251 | `	ph7_value *pArray;` |
|       - | 6252 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6253 | `	ph7_value *pValue;` |
|       - | 6254 | `	sxi32 rc;` |
|       - | 6255 | `	int keep;` |
|       - | 6256 | `	sxu32 n;` |
|      20 | 6257 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6258 | `		/* Invalid arguments,return NULL */` |
|       5 | 6259 | `		ph7_result_null(pCtx);` |
|       5 | 6260 | `		return PH7_OK;` |
|       - | 6261 | `	}` |
|       - | 6262 | `	/* Create a new array */` |
|      16 | 6263 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 6264 | `	if( pArray == 0 ){` |
|     ! 0 | 6265 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6266 | `		return PH7_OK;` |
|       - | 6267 | `	}` |
|       - | 6268 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 6269 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 6270 | `	pEntry = pMap->pFirst;` |
|      16 | 6271 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 6272 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6273 | `	/* Perform the requested operation */` |
|      66 | 6274 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6275 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 6276 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 6277 | `		if( pValue == 0 ){` |
|       - | 6278 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6279 | `			keep = FALSE;` |
|      54 | 6280 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6281 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6282 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6283 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 6284 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6285 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6286 | `					int len;` |
|       3 | 6287 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6288 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6289 | `						"TypeError",` |
|       - | 6290 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6291 | `						zName` |
|       - | 6292 | `						);` |
|     ! 0 | 6293 | `				}else{` |
|     ! 0 | 6294 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6295 | `						"TypeError",` |
|       - | 6296 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6297 | `						ph7_type_name(apArg[1])` |
|       - | 6298 | `						);` |
|       - | 6299 | `				}` |
|       - | 6300 | `			}` |
|      23 | 6301 | `			keep = FALSE;` |
|      23 | 6302 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 6303 | `			if( rc == SXRET_OK ){` |
|       - | 6304 | `				/* Perform a boolean cast */` |
|      23 | 6305 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6306 | `			}` |
|      23 | 6307 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6308 | `		}else{` |
|       - | 6309 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6310 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6311 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6312 | `			 */` |
|      29 | 6313 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6314 | `		}` |
|      51 | 6315 | `		if( keep ){` |
|       - | 6316 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6317 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6318 | `		}` |
|       - | 6319 | `		/* Point to the next entry */` |
|      51 | 6320 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6321 | `	}` |
|      13 | 6322 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6323 | `	return PH7_OK;` |
|      11 | 6324 |  |
|       - | 6325 | `/*` |
|       - | 6326 | ` * array array_map(?callable $callback, array $array)` |
|       - | 6327 | ` *  Applies the callback to the elements of the given array.` |
|       - | 6328 | ` * Parameters` |
|       - | 6329 | ` *  $callback` |
|       - | 6330 | ` *   A callable to run for each element in the array, or NULL for the` |
|       - | 6331 | ` *   identity function (returns the array unchanged).` |
|       - | 6332 | ` *  $array` |
|       - | 6333 | ` *   An array to run through the callback function.` |
|       - | 6334 | ` * Return` |
|       - | 6335 | ` *  Returns an array containing the results of applying the callback` |
|       - | 6336 | ` *  function to each element of $array.` |
|       - | 6337 | ` */` |
|      28 | 6338 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6339 |  |
|       - | 6340 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6341 | `	ph7_hashmap_node *pEntry;` |
|       - | 6342 | `	ph7_hashmap *pMap;` |
|       - | 6343 | `	int bNullCallback;` |
|       - | 6344 | `	sxu32 n;` |
|      30 | 6345 | `	if( nArg < 2 ){` |
|       7 | 6346 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6347 | `			"ArgumentCountError",` |
|       - | 6348 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6349 | `			nArg` |
|       - | 6350 | `			);` |
|       - | 6351 | `	}` |
|      26 | 6352 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 6353 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6354 | `			"TypeError",` |
|       - | 6355 | `			"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6356 | `			ph7_type_name(apArg[1])` |
|       - | 6357 | `			);` |
|       - | 6358 | `	}` |
|      24 | 6359 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      24 | 6360 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6361 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6362 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6363 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6364 | `				"TypeError",` |
|       - | 6365 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6366 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6367 | `				zFunc` |
|       - | 6368 | `				);` |
|       - | 6369 | `		}` |
|       3 | 6370 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6371 | `			"TypeError",` |
|       - | 6372 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6373 | `			"no array or string given"` |
|       - | 6374 | `			);` |
|       - | 6375 | `	}` |
|       - | 6376 | `	/* Create a new array */` |
|      19 | 6377 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 6378 | `	if( pArray == 0 ){` |
|     ! 0 | 6379 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6380 | `		return PH7_OK;` |
|       - | 6381 | `	}` |
|       - | 6382 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6383 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      19 | 6384 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 6385 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6386 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 6387 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 6388 | `	/* Perform the requested operation */` |
|      19 | 6389 | `	pEntry = pMap->pFirst;` |
|      53 | 6390 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6391 | `		/* Extract the node value */` |
|      35 | 6392 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      35 | 6393 | `		if( pValue ){` |
|       - | 6394 | `			/* Extract the node key */` |
|      35 | 6395 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      35 | 6396 | `			if( bNullCallback ){` |
|       - | 6397 | `				/* NULL callback: identity function, keep original value */` |
|      11 | 6398 | `				ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6399 | `			}else{` |
|       - | 6400 | `				/* Invoke the supplied callback */` |
|      25 | 6401 | `				PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 6402 | `				/* Insert the callback return value */` |
|      25 | 6403 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6404 | `			}` |
|      35 | 6405 | `			PH7_MemObjRelease(&sKey);` |
|      35 | 6406 | `			PH7_MemObjRelease(&sResult);` |
|      17 | 6407 | `		}` |
|       - | 6408 | `		/* Point to the next entry */` |
|      35 | 6409 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      18 | 6410 | `	}` |
|      19 | 6411 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 6412 | `	return PH7_OK;` |
|      16 | 6413 |  |
|       - | 6414 | `/*` |
|       - | 6415 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6416 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6417 | ` * Parameters` |
|       - | 6418 | ` *  $array` |
|       - | 6419 | ` *   The input array.` |
|       - | 6420 | ` *  $callback` |
|       - | 6421 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6422 | ` *  $initial` |
|       - | 6423 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6424 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6425 | ` * Return` |
|       - | 6426 | ` *  Returns the resulting value.` |
|       - | 6427 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6428 | ` */` |
|      30 | 6429 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6430 |  |
|       - | 6431 | `	ph7_hashmap_node *pEntry;` |
|       - | 6432 | `	ph7_hashmap *pMap;` |
|       - | 6433 | `	ph7_value *pValue;` |
|       - | 6434 | `	ph7_value sResult;` |
|       - | 6435 | `	sxu32 n;` |
|      32 | 6436 | `	if( nArg < 2 ){` |
|       7 | 6437 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6438 | `			"ArgumentCountError",` |
|       - | 6439 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6440 | `			nArg` |
|       - | 6441 | `			);` |
|       - | 6442 | `	}` |
|      28 | 6443 | `	if( nArg > 3 ){` |
|       4 | 6444 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6445 | `			"ArgumentCountError",` |
|       - | 6446 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6447 | `			nArg` |
|       - | 6448 | `			);` |
|       - | 6449 | `	}` |
|      26 | 6450 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6451 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6452 | `			"TypeError",` |
|       - | 6453 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6454 | `			ph7_type_name(apArg[0])` |
|       - | 6455 | `			);` |
|       - | 6456 | `	}` |
|      24 | 6457 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6458 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6459 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6460 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6461 | `				"TypeError",` |
|       - | 6462 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6463 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6464 | `				zFunc` |
|       - | 6465 | `				);` |
|       - | 6466 | `		}` |
|       7 | 6467 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6468 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6469 | `				"TypeError",` |
|       - | 6470 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6471 | `				"array callback must have exactly two members"` |
|       - | 6472 | `				);` |
|       - | 6473 | `		}` |
|       5 | 6474 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6475 | `			"TypeError",` |
|       - | 6476 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6477 | `			"no array or string given"` |
|       - | 6478 | `			);` |
|       - | 6479 | `	}` |
|       - | 6480 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 6481 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6482 | `	/* Assume a NULL initial value */` |
|      15 | 6483 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      15 | 6484 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      15 | 6485 | `	if( nArg > 2 ){` |
|       - | 6486 | `		/* Set the initial value */` |
|      11 | 6487 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6488 | `	}` |
|       - | 6489 | `	/* Perform the requested operation */` |
|      15 | 6490 | `	pEntry = pMap->pFirst;` |
|      43 | 6491 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6492 | `		/* Extract the node value */` |
|      29 | 6493 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6494 | `		/* Invoke the supplied callback */` |
|      29 | 6495 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6496 | `		/* Point to the next entry */` |
|      29 | 6497 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6498 | `	}` |
|      15 | 6499 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6500 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6501 | `	return PH7_OK;` |
|      17 | 6502 |  |
|       - | 6503 | `/*` |
|       - | 6504 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6505 | ` *  Apply a user function to every member of an array.` |
|       - | 6506 | ` * Parameters` |
|       - | 6507 | ` *  $array` |
|       - | 6508 | ` *   The input array.` |
|       - | 6509 | ` *  $funcname` |
|       - | 6510 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6511 | ` *   the first, and the key/index second.` |
|       - | 6512 | ` * Note:` |
|       - | 6513 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6514 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6515 | ` *  be made in the original array itself.` |
|       - | 6516 | ` *  $userdata` |
|       - | 6517 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6518 | ` *   to the callback funcname.` |
|       - | 6519 | ` * Return` |
|       - | 6520 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6521 | ` */` |
|      36 | 6522 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6523 |  |
|       - | 6524 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6525 | `	ph7_hashmap_node *pEntry;` |
|       - | 6526 | `	ph7_hashmap *pMap;` |
|       - | 6527 | `	sxu32 n;` |
|      38 | 6528 | `	if( nArg < 2 ){` |
|       7 | 6529 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6530 | `			"ArgumentCountError",` |
|       - | 6531 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6532 | `			nArg` |
|       - | 6533 | `			);` |
|       - | 6534 | `	}` |
|      34 | 6535 | `	if( nArg > 3 ){` |
|       4 | 6536 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6537 | `			"ArgumentCountError",` |
|       - | 6538 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6539 | `			nArg` |
|       - | 6540 | `			);` |
|       - | 6541 | `	}` |
|      32 | 6542 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6543 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6544 | `			"TypeError",` |
|       - | 6545 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6546 | `			ph7_type_name(apArg[0])` |
|       - | 6547 | `			);` |
|       - | 6548 | `	}` |
|      30 | 6549 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6550 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6551 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6552 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6553 | `				"TypeError",` |
|       - | 6554 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6555 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6556 | `				zFunc` |
|       - | 6557 | `				);` |
|       - | 6558 | `		}` |
|       9 | 6559 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6560 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6561 | `				"TypeError",` |
|       - | 6562 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6563 | `				"array callback must have exactly two members"` |
|       - | 6564 | `				);` |
|       - | 6565 | `		}` |
|       5 | 6566 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6567 | `			"TypeError",` |
|       - | 6568 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6569 | `			"no array or string given"` |
|       - | 6570 | `			);` |
|       - | 6571 | `	}` |
|      19 | 6572 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6573 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6574 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      19 | 6575 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6576 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6577 | `	/* Perform the desired operation */` |
|      19 | 6578 | `	pEntry = pMap->pFirst;` |
|      59 | 6579 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6580 | `		/* Extract the node value */` |
|      41 | 6581 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      41 | 6582 | `		if( pValue ){` |
|       - | 6583 | `			/* Extract the entry key */` |
|      41 | 6584 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6585 | `			/* Invoke the supplied callback */` |
|      41 | 6586 | `			PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      41 | 6587 | `			PH7_MemObjRelease(&sKey);` |
|      20 | 6588 | `		}` |
|       - | 6589 | `		/* Point to the next entry */` |
|      41 | 6590 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6591 | `	}` |
|       - | 6592 | `	/* All done, return TRUE */` |
|      19 | 6593 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6594 | `	return PH7_OK;` |
|      20 | 6595 |  |
|       - | 6596 | `/*` |
|       - | 6597 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6598 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6599 | ` */` |
|      22 | 6600 | `static void HashmapWalkRecursive(` |
|       - | 6601 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6602 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6603 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6604 | `	int iNest             /* Nesting level */` |
|       - | 6605 | `	)` |
|       1 | 6606 |  |
|       - | 6607 | `	ph7_hashmap_node *pEntry;` |
|       - | 6608 | `	ph7_value *pValue,sKey;` |
|       - | 6609 | `	sxu32 n;` |
|       - | 6610 | `	/* Iterate through hashmap entries */` |
|      23 | 6611 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6612 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6613 | `	pEntry = pMap->pFirst;` |
|      59 | 6614 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6615 | `		/* Extract the node value */` |
|      37 | 6616 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6617 | `		if( pValue ){` |
|      37 | 6618 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6619 | `				if( iNest < 32 ){` |
|       - | 6620 | `					/* Recurse */` |
|      11 | 6621 | `					iNest++;` |
|      11 | 6622 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6623 | `					iNest--;` |
|       5 | 6624 | `				}` |
|       6 | 6625 | `			}else{` |
|       - | 6626 | `				/* Extract the node key */` |
|      27 | 6627 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6628 | `				/* Invoke the supplied callback */` |
|      27 | 6629 | `				PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6630 | `				PH7_MemObjRelease(&sKey);` |
|       - | 6631 | `			}` |
|      18 | 6632 | `		}` |
|       - | 6633 | `		/* Point to the next entry */` |
|      37 | 6634 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6635 | `	}` |
|      23 | 6636 |  |
|       - | 6637 | `/*` |
|       - | 6638 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6639 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6640 | ` * Parameters` |
|       - | 6641 | ` *  $array` |
|       - | 6642 | ` *   The input array.` |
|       - | 6643 | ` *  $funcname` |
|       - | 6644 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6645 | ` *   the first, and the key/index second.` |
|       - | 6646 | ` * Note:` |
|       - | 6647 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6648 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6649 | ` *  be made in the original array itself.` |
|       - | 6650 | ` *  $userdata` |
|       - | 6651 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6652 | ` *   to the callback funcname.` |
|       - | 6653 | ` * Return` |
|       - | 6654 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6655 | ` */` |
|      30 | 6656 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6657 |  |
|       - | 6658 | `	ph7_hashmap *pMap;` |
|      32 | 6659 | `	if( nArg < 2 ){` |
|       7 | 6660 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6661 | `			"ArgumentCountError",` |
|       - | 6662 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 6663 | `			nArg` |
|       - | 6664 | `			);` |
|       - | 6665 | `	}` |
|      28 | 6666 | `	if( nArg > 3 ){` |
|       4 | 6667 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6668 | `			"ArgumentCountError",` |
|       - | 6669 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 6670 | `			nArg` |
|       - | 6671 | `			);` |
|       - | 6672 | `	}` |
|      26 | 6673 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6674 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6675 | `			"TypeError",` |
|       - | 6676 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6677 | `			ph7_type_name(apArg[0])` |
|       - | 6678 | `			);` |
|       - | 6679 | `	}` |
|      24 | 6680 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6681 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6682 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6683 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6684 | `				"TypeError",` |
|       - | 6685 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6686 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6687 | `				zFunc` |
|       - | 6688 | `				);` |
|       - | 6689 | `		}` |
|       9 | 6690 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6691 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6692 | `				"TypeError",` |
|       - | 6693 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6694 | `				"array callback must have exactly two members"` |
|       - | 6695 | `				);` |
|       - | 6696 | `		}` |
|       5 | 6697 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6698 | `			"TypeError",` |
|       - | 6699 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6700 | `			"no array or string given"` |
|       - | 6701 | `			);` |
|       - | 6702 | `	}` |
|       - | 6703 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6704 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6705 | `	/* Perform the desired operation */` |
|      13 | 6706 | `	HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6707 | `	/* All done, return TRUE */` |
|      13 | 6708 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6709 | `	return PH7_OK;` |
|      17 | 6710 |  |
|       - | 6711 | `/*` |
|       - | 6712 | ` * Table of hashmap functions.` |
|       - | 6713 | ` */` |
|       - | 6714 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6715 | `	{"count",             ph7_hashmap_count },` |
|       - | 6716 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6717 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6718 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6719 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6720 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6721 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6722 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6723 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6724 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6725 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6726 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6727 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6728 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6729 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6730 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6731 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6732 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6733 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6734 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6735 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6736 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6737 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6738 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6739 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6740 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6741 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6742 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6743 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6744 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6745 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6746 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6747 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6748 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6749 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6750 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6751 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6752 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6753 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6754 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6755 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6756 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6757 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6758 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6759 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6760 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6761 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6762 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6763 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6764 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6765 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6766 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6767 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6768 | `	{"current",           ph7_hashmap_current },` |
|       - | 6769 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6770 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6771 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6772 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6773 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6774 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6775 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6776 | `};` |
|       - | 6777 | `/*` |
|       - | 6778 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6779 | ` */` |
|    1990 | 6780 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6781 |  |
|       - | 6782 | `	sxu32 n;` |
|  123382 | 6783 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  121392 | 6784 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   60697 | 6785 | `	}` |
|    1992 | 6786 |  |
|       - | 6787 | `/*` |
|       - | 6788 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6789 | ` * the BLOB given as the first argument.` |
|       - | 6790 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6791 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6792 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6793 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6794 | ` */` |
|      26 | 6795 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6796 |  |
|       - | 6797 | `	ph7_hashmap_node *pEntry;` |
|       - | 6798 | `	ph7_value *pObj;` |
|      28 | 6799 | `	sxu32 n = 0;` |
|       - | 6800 | `	int isRef;` |
|       - | 6801 | `	sxi32 rc;` |
|       - | 6802 | `	int i;` |
|      28 | 6803 | `	if( nDepth > 31 ){` |
|       - | 6804 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6805 | `		/* Nesting limit reached */` |
|     ! 0 | 6806 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6807 | `		if( ShowType ){` |
|     ! 0 | 6808 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6809 | `		}` |
|     ! 0 | 6810 | `		return SXERR_LIMIT;` |
|       - | 6811 | `	}` |
|       - | 6812 | `	/* Point to the first inserted entry */` |
|      28 | 6813 | `	pEntry = pMap->pFirst;` |
|      28 | 6814 | `	rc = SXRET_OK;` |
|      28 | 6815 | `	if( !ShowType ){` |
|      15 | 6816 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6817 | `	}` |
|       - | 6818 | `	/* Total entries */` |
|      28 | 6819 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6820 | `#ifdef __WINNT__` |
|       2 | 6821 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6822 | `#else` |
|      26 | 6823 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6824 | `#endif` |
|      62 | 6825 | `	for(;;){` |
|     126 | 6826 | `		if( n >= pMap->nEntry ){` |
|      28 | 6827 | `			break;` |
|       - | 6828 | `		}` |
|     198 | 6829 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 6830 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 6831 | `		}` |
|       - | 6832 | `		/* Dump key */` |
|     100 | 6833 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 6834 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 6835 | `		}else{` |
|     101 | 6836 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6837 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6838 | `		}` |
|       - | 6839 | `#ifdef __WINNT__` |
|       2 | 6840 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6841 | `#else` |
|      98 | 6842 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6843 | `#endif` |
|       - | 6844 | `		/* Dump node value */` |
|     100 | 6845 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 6846 | `		isRef = 0;` |
|     100 | 6847 | `		if( pObj ){` |
|     100 | 6848 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6849 | `				/* Referenced object */` |
|     ! 0 | 6850 | `				isRef = 1;` |
|     ! 0 | 6851 | `			}` |
|     100 | 6852 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 6853 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6854 | `				break;` |
|       - | 6855 | `			}` |
|      49 | 6856 | `		}` |
|       - | 6857 | `		/* Point to the next entry */` |
|     100 | 6858 | `		n++;` |
|     100 | 6859 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6860 | `	}` |
|      54 | 6861 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 6862 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 6863 | `	}` |
|      28 | 6864 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 6865 | `	return rc;` |
|      15 | 6866 |  |
|       - | 6867 | `/*` |
|       - | 6868 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6869 | ` * retrieved entry.` |
|       - | 6870 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6871 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6872 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6873 | ` * a value different from PH7_OK.` |
|       - | 6874 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6875 | ` */` |
|   22420 | 6876 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6877 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6878 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6879 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6880 | `	)` |
|       2 | 6881 |  |
|       - | 6882 | `	ph7_hashmap_node *pEntry;` |
|       - | 6883 | `	ph7_value sKey,sValue;` |
|       - | 6884 | `	sxi32 rc;` |
|       - | 6885 | `	sxu32 n;` |
|       - | 6886 | `	/* Initialize walker parameter */` |
|   22422 | 6887 | `	rc = SXRET_OK;` |
|   22422 | 6888 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   22422 | 6889 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   22422 | 6890 | `	n = pMap->nEntry;` |
|   22422 | 6891 | `	pEntry = pMap->pFirst;` |
|       - | 6892 | `	/* Start the iteration process */` |
|   56651 | 6893 | `	for(;;){` |
|  113304 | 6894 | `		if( n < 1 ){` |
|   22422 | 6895 | `			break;` |
|       - | 6896 | `		}` |
|       - | 6897 | `		/* Extract a copy of the key and a copy the current value */` |
|   90884 | 6898 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   90884 | 6899 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6900 | `		/* Invoke the user callback */` |
|   90884 | 6901 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6902 | `		/* Release the copy of the key and the value */` |
|   90884 | 6903 | `		PH7_MemObjRelease(&sKey);` |
|   90884 | 6904 | `		PH7_MemObjRelease(&sValue);` |
|   90884 | 6905 | `		if( rc != PH7_OK ){` |
|       - | 6906 | `			/* Callback request an operation abort */` |
|     ! 0 | 6907 | `			return SXERR_ABORT;` |
|       - | 6908 | `		}` |
|       - | 6909 | `		/* Point to the next entry */` |
|   90884 | 6910 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   90884 | 6911 | `		n--;` |
|       2 | 6912 | `	}` |
|       - | 6913 | `	/* All done */` |
|   22422 | 6914 | `	return SXRET_OK;` |
|   11212 | 6915 |  |
|       - | 6916 |  |
