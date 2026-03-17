# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2815/3270 lines (86.09%)

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
| 2847194 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2847196 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  238834 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  238836 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  238836 |   29 | `	sxu32 nH = 5381;` |
|  238836 |   30 | `	zEnd = &zIn[nLen];` |
|  272272 |   31 | `	for(;;){` |
|  544546 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  485942 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  438494 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  356992 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  238836 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     768 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       2 |   47 |  |
|     770 |   48 | `	sxi64 iCount = 0;` |
|     770 |   49 | `	if( !bRecursive ){` |
|     596 |   50 | `		iCount = pMap->nEntry;` |
|     299 |   51 | `	}else{` |
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
|     770 |   86 | `	return iCount;` |
|       2 |   87 |  |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 2792122 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 2792124 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2792124 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 2792124 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 2792124 |  104 | `	pNode->pMap  = &(*pMap);` |
| 2792124 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2792124 |  106 | `	pNode->nHash = nHash;` |
| 2792124 |  107 | `	pNode->xKey.iKey = iKey;` |
| 2792124 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 2792124 |  109 | `	return pNode;` |
| 1396063 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|   83038 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|   83040 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   83040 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|   83040 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|   83040 |  127 | `	pNode->pMap  = &(*pMap);` |
|   83040 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   83040 |  129 | `	pNode->nHash = nHash;` |
|   83040 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   83040 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   83040 |  132 | `	pNode->nValIdx = nValIdx;` |
|   83040 |  133 | `	return pNode;` |
|   41521 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 2875160 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  139 |  |
|       - |  140 | `	/* Link */` |
| 2875162 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2647818 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2647818 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1323908 |  144 | `	}` |
| 2875162 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 2875162 |  147 | `	if( pMap->pFirst == 0 ){` |
|   39032 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   39032 |  150 | `		pMap->pCur = pNode;` |
|   19517 |  151 | `	}else{` |
| 2836132 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 2875162 |  154 | `	++pMap->nEntry;` |
| 2875162 |  155 |  |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    5714 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  161 |  |
|    5716 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5716 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    5716 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    5298 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2650 |  167 | `	}else{` |
|     419 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    5716 |  170 | `	if( pNode->pNextCollide ){` |
|    4459 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2229 |  172 | `	}` |
|    5716 |  173 | `	if( pMap->pFirst == pNode ){` |
|      78 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      38 |  175 | `	}` |
|    5716 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|      80 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      39 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    5716 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5716 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     100 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     100 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     100 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      49 |  188 | `		}` |
|      49 |  189 | `	}` |
|    5716 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5597 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2798 |  192 | `	}` |
|    5716 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5716 |  194 | `	pMap->nEntry--;` |
|    5716 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      34 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      34 |  198 | `		pMap->apBucket = 0;` |
|      34 |  199 | `		pMap->nSize = 0;` |
|      34 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      16 |  201 | `	}` |
|    5716 |  202 |  |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 2875160 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  208 |  |
| 2875162 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   42844 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   42844 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   42844 |  215 | `		if( nNew < 1 ){` |
|   39032 |  216 | `			nNew = 16;` |
|   19515 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   42844 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   42844 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   42844 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   42844 |  230 | `		pMap->apBucket = apNew;` |
|   42844 |  231 | `		pMap->nSize = nNew;` |
|   42844 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   39032 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    3814 |  237 | `		pEntry = pMap->pFirst;` |
|    3814 |  238 | `		n = 0;` |
| 1962322 |  239 | `		for( ;; ){` |
| 3924646 |  240 | `			if( n >= pMap->nEntry ){` |
|    3814 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 3920834 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 3920834 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3920834 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3439846 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3439846 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1719922 |  250 | `			}` |
| 3920834 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 3920834 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3920834 |  254 | `			n++;` |
|       2 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    3814 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1906 |  258 | `	}` |
| 2836132 |  259 | `	return SXRET_OK;` |
| 1437582 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 2792122 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 2792124 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		/* Reserve a ph7_value for the value */` |
| 2792098 |  274 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2792098 |  275 | `		if( pObj == 0 ){` |
|     ! 0 |  276 | `			return SXERR_MEM;` |
|       - |  277 | `		}` |
| 2792098 |  278 | `		if( pValue ){` |
|       - |  279 | `			/* Duplicate the value */` |
| 2792098 |  280 | `			PH7_MemObjStore(pValue,pObj);` |
| 1396048 |  281 | `		}` |
| 2792098 |  282 | `		nIdx = pObj->nIdx;` |
| 1396050 |  283 | `	}else{` |
|      27 |  284 | `		nIdx = nRefIdx;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Hash the key */` |
| 2792124 |  287 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  288 | `	/* Allocate a new int node */` |
| 2792124 |  289 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2792124 |  290 | `	if( pNode == 0 ){` |
|     ! 0 |  291 | `		return SXERR_MEM;` |
|       - |  292 | `	}` |
| 2792124 |  293 | `	if( isForeign ){` |
|       - |  294 | `		/* Mark as a foregin entry */` |
|      27 |  295 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      13 |  296 | `	}` |
|       - |  297 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2792124 |  298 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2792124 |  299 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  300 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  301 | `		return rc;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Perform the insertion */` |
| 2792124 |  304 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  305 | `	/* Install in the reference table */` |
| 2792124 |  306 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  307 | `	/* All done */` |
| 2792124 |  308 | `	return SXRET_OK;` |
| 1396063 |  309 |  |
|       - |  310 | `/*` |
|       - |  311 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  312 | ` * hashmap.` |
|       - |  313 | ` */` |
|   83038 |  314 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  315 |  |
|       - |  316 | `	ph7_hashmap_node *pNode;` |
|       - |  317 | `	sxu32 nHash;` |
|       - |  318 | `	sxu32 nIdx;` |
|       - |  319 | `	sxi32 rc;` |
|   83040 |  320 | `	if( !isForeign ){` |
|       - |  321 | `		ph7_value *pObj;` |
|       - |  322 | `		/* Reserve a ph7_value for the value */` |
|   60444 |  323 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   60444 |  324 | `		if( pObj == 0 ){` |
|     ! 0 |  325 | `			return SXERR_MEM;` |
|       - |  326 | `		}` |
|   60444 |  327 | `		if( pValue ){` |
|       - |  328 | `			/* Duplicate the value */` |
|   60444 |  329 | `			PH7_MemObjStore(pValue,pObj);` |
|   30221 |  330 | `		}` |
|   60444 |  331 | `		nIdx = pObj->nIdx;` |
|   30223 |  332 | `	}else{` |
|   22598 |  333 | `		nIdx = nRefIdx;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Hash the key */` |
|   83040 |  336 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  337 | `	/* Allocate a new blob node */` |
|   83040 |  338 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   83040 |  339 | `	if( pNode == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|   83040 |  342 | `	if( isForeign ){` |
|       - |  343 | `		/* Mark as a foregin entry */` |
|   22598 |  344 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   11298 |  345 | `	}` |
|       - |  346 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   83040 |  347 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   83040 |  348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  349 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  350 | `		return rc;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Perform the insertion */` |
|   83040 |  353 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  354 | `	/* Install in the reference table */` |
|   83040 |  355 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  356 | `	/* All done */` |
|   83040 |  357 | `	return SXRET_OK;` |
|   41521 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  361 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  362 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  363 | ` */` |
|   46948 |  364 | `static sxi32 HashmapLookupIntKey(` |
|       - |  365 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  366 | `	sxi64 iKey,                /* lookup key */` |
|       - |  367 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  368 | `	)` |
|       2 |  369 |  |
|       - |  370 | `	ph7_hashmap_node *pNode;` |
|       - |  371 | `	sxu32 nHash;` |
|   46950 |  372 | `	if( pMap->nEntry < 1 ){` |
|       - |  373 | `		/* Don't bother hashing,there is no entry anyway */` |
|     388 |  374 | `		return SXERR_NOTFOUND;` |
|       - |  375 | `	}` |
|       - |  376 | `	/* Hash the key first */` |
|   46564 |  377 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  378 | `	/* Point to the appropriate bucket */` |
|   46564 |  379 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  380 | `	/* Perform the lookup */` |
|  411634 |  381 | `	for(;;){` |
|  823270 |  382 | `		if( pNode == 0 ){` |
|   45782 |  383 | `			break;` |
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
|   45782 |  398 | `	return SXERR_NOTFOUND;` |
|   23476 |  399 |  |
|       - |  400 | `/*` |
|       - |  401 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  402 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  403 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  404 | ` */` |
|  164720 |  405 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  406 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  407 | `	const void *pKey,           /* Lookup key */` |
|       - |  408 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  409 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  410 | `	)` |
|       2 |  411 |  |
|       - |  412 | `	ph7_hashmap_node *pNode;` |
|       - |  413 | `	sxu32 nHash;` |
|  164722 |  414 | `	if( pMap->nEntry < 1 ){` |
|       - |  415 | `		/* Don't bother hashing,there is no entry anyway */` |
|    8926 |  416 | `		return SXERR_NOTFOUND;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Hash the key first */` |
|  155798 |  419 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  420 | `	/* Point to the appropriate bucket */` |
|  155798 |  421 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  422 | `	/* Perform the lookup */` |
|  152470 |  423 | `	for(;;){` |
|  304942 |  424 | `		if( pNode == 0 ){` |
|  118282 |  425 | `			break;` |
|       - |  426 | `		}` |
|  205418 |  427 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  185162 |  428 | `			&& pNode->nHash == nHash` |
|  110590 |  429 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   37518 |  430 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  431 | `				/* Node found */` |
|   37518 |  432 | `				if( ppNode ){` |
|   37490 |  433 | `					*ppNode = pNode;` |
|   18744 |  434 | `				}` |
|   37518 |  435 | `				return SXRET_OK;` |
|       - |  436 | `		}` |
|       - |  437 | `		/* Follow the collision link */` |
|  149146 |  438 | `		pNode = pNode->pNextCollide;` |
|       2 |  439 | `	}` |
|       - |  440 | `	/* No such entry */` |
|  118282 |  441 | `	return SXERR_NOTFOUND;` |
|   82362 |  442 |  |
|       - |  443 | `/*` |
|       - |  444 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  445 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  446 | ` */` |
|  164862 |  447 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  448 |  |
|  164864 |  449 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  164864 |  450 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  164864 |  451 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  452 | `		/* Octal not decimal number */` |
|       5 |  453 | `		return FALSE;` |
|       - |  454 | `	}` |
|  164860 |  455 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  456 | `		zIn++;` |
|     ! 0 |  457 | `	}` |
|   82762 |  458 | `	for(;;){` |
|  165526 |  459 | `		if( zIn >= zEnd ){` |
|     233 |  460 | `			return TRUE;` |
|       - |  461 | `		}` |
|  165294 |  462 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   82315 |  463 | `			break;` |
|       - |  464 | `		}` |
|     667 |  465 | `		zIn++;` |
|       1 |  466 | `	}` |
|       - |  467 | `	/* Key does not look like a decimal number */` |
|  164628 |  468 | `	return FALSE;` |
|   82433 |  469 |  |
|       - |  470 | `/*` |
|       - |  471 | ` * Check if a given key exists in the given hashmap.` |
|       - |  472 | ` * Write a pointer to the target node on success.` |
|       - |  473 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  474 | ` */` |
|   82352 |  475 | `static sxi32 HashmapLookup(` |
|       - |  476 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  477 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  478 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  479 | `	)` |
|       2 |  480 |  |
|   82354 |  481 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  482 | `	sxi32 rc;` |
|   82354 |  483 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   81664 |  484 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  485 | `			/* Force a string cast */` |
|     ! 0 |  486 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  487 | `		}` |
|   81664 |  488 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  489 | `			/* Perform a blob lookup */` |
|   81648 |  490 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   81648 |  491 | `			goto result;` |
|       - |  492 | `		}` |
|       8 |  493 | `	}` |
|       - |  494 | `	/* Perform an int lookup */` |
|     708 |  495 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  496 | `		/* Force an integer cast */` |
|      27 |  497 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  498 | `	}` |
|       - |  499 | `	/* Perform an int lookup */` |
|     708 |  500 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   41176 |  501 | `result:` |
|   82354 |  502 | `	if( rc == SXRET_OK ){` |
|       - |  503 | `		/* Node found */` |
|   38118 |  504 | `		if( ppNode ){` |
|   38084 |  505 | `			*ppNode = pNode;` |
|   19041 |  506 | `		}` |
|   38118 |  507 | `		return SXRET_OK;` |
|       - |  508 | `	}` |
|       - |  509 | `	/* No such entry */` |
|   44238 |  510 | `	return SXERR_NOTFOUND;` |
|   41178 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  514 | ` * hashmap.` |
|       - |  515 | ` * If a node with the given key already exists in the database` |
|       - |  516 | ` * then this function overwrite the old value.` |
|       - |  517 | ` */` |
| 2852346 |  518 | `static sxi32 HashmapInsert(` |
|       - |  519 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  520 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  521 | `	ph7_value *pVal    /* Node value */` |
|       - |  522 | `	)` |
|       2 |  523 |  |
| 2852348 |  524 | `	ph7_hashmap_node *pNode = 0;` |
| 2852348 |  525 | `	sxi32 rc = SXRET_OK;` |
| 2852348 |  526 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   60638 |  527 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  528 | `			/* Force a string cast */` |
|       3 |  529 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  530 | `		}` |
|   60638 |  531 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  532 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  533 | `				/* Automatic index assign */` |
|      34 |  534 | `				pKey = 0;` |
|      16 |  535 | `			}` |
|     256 |  536 | `			goto IntKey;` |
|       - |  537 | `		}` |
|   90575 |  538 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   30191 |  539 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|   60348 |  553 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  554 | `			/* Forbidden */` |
|       3 |  555 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Perform a blob-key insertion */` |
|   60346 |  559 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   60346 |  560 | `		return rc;` |
|       - |  561 | `	}` |
| 1395855 |  562 | `IntKey:` |
| 2791966 |  563 | `	if( pKey ){` |
|   23232 |  564 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  565 | `			/* Force an integer cast */` |
|     251 |  566 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  567 | `		}` |
|   23232 |  568 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   23186 |  582 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  583 | `			/* Forbidden */` |
|       3 |  584 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  585 | `			return SXRET_OK;` |
|       - |  586 | `		}` |
|       - |  587 | `		/* Perform a 64-bit-int-key insertion */` |
|   23184 |  588 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23184 |  589 | `		if( rc == SXRET_OK ){` |
|   23184 |  590 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  591 | `				/* Increment the automatic index */` |
|   22956 |  592 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  593 | `				/* Make sure the automatic index is not reserved */` |
|   22956 |  594 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  595 | `					pMap->iNextIdx++;` |
|     ! 0 |  596 | `				}` |
|   11477 |  597 | `			}` |
|   11591 |  598 | `		}` |
|   11593 |  599 | `	}else{` |
| 2768736 |  600 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  601 | `			/* Forbidden */` |
|       3 |  602 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|       - |  605 | `		/* Assign an automatic index */` |
| 2768734 |  606 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2768734 |  607 | `		if( rc == SXRET_OK ){` |
| 2768734 |  608 | `			++pMap->iNextIdx;` |
| 1384366 |  609 | `		}` |
|       - |  610 | `	}` |
|       - |  611 | `	/* Insertion result */` |
| 2791916 |  612 | `	return rc;` |
| 1426175 |  613 |  |
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
|   22628 |  641 | `static sxi32 HashmapInsertByRef(` |
|       - |  642 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  643 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  644 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  645 | `	)` |
|       2 |  646 |  |
|   22630 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|   22630 |  648 | `	sxi32 rc = SXRET_OK;` |
|   22630 |  649 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   22604 |  650 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  651 | `			/* Force a string cast */` |
|     ! 0 |  652 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  653 | `		}` |
|   22604 |  654 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  655 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  656 | `				/* Automatic index assign */` |
|     ! 0 |  657 | `				pKey = 0;` |
|     ! 0 |  658 | `			}` |
|     ! 0 |  659 | `			goto IntKey;` |
|       - |  660 | `		}` |
|   33905 |  661 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   11301 |  662 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  663 | `				/* Overwrite */` |
|       7 |  664 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  665 | `				pNode->nValIdx = nRefIdx;` |
|       - |  666 | `				/* Install in the reference table */` |
|       7 |  667 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  668 | `				return SXRET_OK;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Perform a blob-key insertion */` |
|   22598 |  671 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   22598 |  672 | `		return rc;` |
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
|   11316 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * Extract node value.` |
|       - |  712 | ` */` |
|  920376 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
|  920378 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  920378 |  718 | `	return pObj;` |
|       2 |  719 |  |
|       - |  720 | `/*` |
|       - |  721 | ` * Insert a node in the given hashmap.` |
|       - |  722 | ` * If a node with the given key already exists in the database` |
|       - |  723 | ` * then this function overwrite the old value.` |
|       - |  724 | ` */` |
|     418 |  725 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       1 |  726 |  |
|       - |  727 | `	ph7_value *pObj;` |
|       - |  728 | `	sxi32 rc;` |
|       - |  729 | `	/* Extract the node value */` |
|     419 |  730 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     419 |  731 | `	if( pObj == 0 ){` |
|     ! 0 |  732 | `		return SXERR_EMPTY;` |
|       - |  733 | `	}` |
|       - |  734 | `	/* Preserve key */` |
|     419 |  735 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  736 | `		/* Int64 key */` |
|     289 |  737 | `		if( !bPreserve ){` |
|       - |  738 | `			/* Assign an automatic index */` |
|     149 |  739 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      75 |  740 | `		}else{` |
|     141 |  741 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  742 | `		}` |
|     145 |  743 | `	}else{` |
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
|     419 |  754 | `	return rc;` |
|     210 |  755 |  |
|       - |  756 | `/*` |
|       - |  757 | ` * Compare two node values.` |
|       - |  758 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  759 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  760 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  761 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  762 | ` * documenation.` |
|       - |  763 | ` */` |
|   38873 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   38875 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   38875 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   38875 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   38875 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   38875 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   38875 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   38875 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   38875 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   38875 |  783 | `	return rc;` |
|   19482 |  784 |  |
|       - |  785 | `/*` |
|       - |  786 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  787 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  788 | ` */` |
|    8510 |  789 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  790 |  |
|    8512 |  791 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  792 | `	sxu32 nBucket;` |
|       - |  793 | `	/* Remove old collision links */` |
|    8512 |  794 | `	if( pEntry->pPrevCollide ){` |
|    6524 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    3258 |  796 | `	}else{` |
|    1990 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  798 | `	}` |
|    8512 |  799 | `	if( pEntry->pNextCollide ){` |
|     660 |  800 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     330 |  801 | `	}` |
|    8512 |  802 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  803 | `	/* Compute the new hash */` |
|    8512 |  804 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    8512 |  805 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    8512 |  806 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  807 | `	/* Link to the new bucket */` |
|    8512 |  808 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    8512 |  809 | `	if( pMap->apBucket[nBucket] ){` |
|    6695 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3345 |  811 | `	}` |
|    8512 |  812 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    8512 |  813 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  814 | `	/* Increment the automatic index */` |
|    8512 |  815 | `	pMap->iNextIdx++;` |
|    8512 |  816 |  |
|       - |  817 | `/*` |
|       - |  818 | ` * Perform a linear search on a given hashmap.` |
|       - |  819 | ` * Write a pointer to the target node on success.` |
|       - |  820 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  821 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  822 | ` * for more information.` |
|       - |  823 | ` */` |
|   21538 |  824 | `static int HashmapFindValue(` |
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
|   21540 |  837 | `	pEntry = pMap->pFirst;` |
|   21540 |  838 | `	n = pMap->nEntry;` |
|   21540 |  839 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   21540 |  840 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   51583 |  841 | `	for(;;){` |
|  103168 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  103070 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  103070 |  847 | `		if( pVal ){` |
|  103070 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  103070 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  103070 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  103070 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  103070 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  103070 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  103070 |  865 | `				if( rc == 0 ){` |
|   21442 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   21442 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   40814 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|   81630 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   81630 |  876 | `		n--;` |
|       2 |  877 | `	}` |
|       - |  878 | `	/* No such entry */` |
|      99 |  879 | `	return SXERR_NOTFOUND;` |
|   10771 |  880 |  |
|       - |  881 | `/*` |
|       - |  882 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  883 | ` * for values comparison.` |
|       - |  884 | ` * Write a pointer to the target node on success.` |
|       - |  885 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  886 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  887 | ` * for more information.` |
|       - |  888 | ` */` |
|      12 |  889 | `static int HashmapFindValueByCallback(` |
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
|      13 |  902 | `	pEntry = pMap->pFirst;` |
|      13 |  903 | `	n = pMap->nEntry;` |
|       - |  904 | `	/* Store callback result here */` |
|      13 |  905 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  906 | `	/* First argument to the callback */` |
|      13 |  907 | `	apArg[0] = pNeedle;` |
|      15 |  908 | `	for(;;){` |
|      31 |  909 | `		if( n < 1 ){` |
|       7 |  910 | `			break;` |
|       - |  911 | `		}` |
|       - |  912 | `		/* Extract node value */` |
|      25 |  913 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      25 |  914 | `		if( pVal ){` |
|       - |  915 | `			/* Invoke the user callback */` |
|      25 |  916 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      25 |  917 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      25 |  918 | `			if( rc == SXRET_OK ){` |
|       - |  919 | `				/* Extract callback result */` |
|      25 |  920 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  921 | `					/* Perform an int cast */` |
|     ! 0 |  922 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  923 | `				}` |
|      25 |  924 | `				rc = (sxi32)sResult.x.iVal;` |
|      25 |  925 | `				PH7_MemObjRelease(&sResult);` |
|      25 |  926 | `				if( rc == 0 ){` |
|       - |  927 | `					/* Match found*/` |
|       7 |  928 | `					if( ppNode ){` |
|     ! 0 |  929 | `						*ppNode = pEntry;` |
|     ! 0 |  930 | `					}` |
|       7 |  931 | `					return SXRET_OK;` |
|       - |  932 | `				}` |
|       9 |  933 | `			}` |
|       9 |  934 | `		}` |
|       - |  935 | `		/* Point to the next entry */` |
|      19 |  936 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 |  937 | `		n--;` |
|       1 |  938 | `	}` |
|       - |  939 | `	/* No such entry */` |
|       7 |  940 | `	return SXERR_NOTFOUND;` |
|       7 |  941 |  |
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
|  454582 | 1054 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1055 | `	ph7_hashmap *pDest,` |
|       - | 1056 | `	ph7_hashmap_node *pEntry,` |
|       - | 1057 | `	ph7_value *pVal,` |
|       - | 1058 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1059 | `	)` |
|       2 | 1060 |  |
|  454584 | 1061 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1062 | `	ph7_value sKey;` |
|       - | 1063 | `	sxi32 rc;` |
|       - | 1064 |  |
|  454584 | 1065 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1066 | `		/* Blob key insertion */` |
|      41 | 1067 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      41 | 1068 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      41 | 1069 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      41 | 1070 | `		PH7_MemObjRelease(&sKey);` |
|      21 | 1071 | `	}else{` |
|       - | 1072 | `		/* Int key */` |
|  454544 | 1073 | `		if( iAction == 0 ){ /* Merge */` |
|  454472 | 1074 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  227309 | 1075 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1076 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1077 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1078 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1079 | `		}else{ /* Dup */` |
|      44 | 1080 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1081 | `		}` |
|       - | 1082 | `	}` |
|  454584 | 1083 | `	return rc;` |
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
|    1752 | 1097 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1098 |  |
|       - | 1099 | `	ph7_hashmap_node *pEntry;` |
|       - | 1100 | `	ph7_value *pVal;` |
|       - | 1101 | `	sxi32 rc;` |
|       - | 1102 | `	sxu32 n;` |
|    1754 | 1103 | `	if( pSrc == pDest ){` |
|       - | 1104 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1105 | `		 * Unlike the zend engine.` |
|       - | 1106 | `		 */` |
|     ! 0 | 1107 | `		return SXRET_OK;` |
|       - | 1108 | `	}` |
|       - | 1109 | `	/* Point to the first inserted entry in the source */` |
|    1754 | 1110 | `	pEntry = pSrc->pFirst;` |
|       - | 1111 | `	/* Perform the merge */` |
|  456240 | 1112 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1113 | `		/* Extract the node value */` |
|  454488 | 1114 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  454488 | 1115 | `		if( pVal ){` |
|       - | 1116 | `			/* Make a local copy of the value.` |
|       - | 1117 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1118 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1119 | `			 * to the old pool.` |
|       - | 1120 | `			 */` |
|  454488 | 1121 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  227245 | 1122 | `		}else{` |
|     ! 0 | 1123 | `			rc = SXRET_OK;` |
|       - | 1124 | `		}` |
|  454488 | 1125 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1126 | `			return rc;` |
|       - | 1127 | `		}` |
|       - | 1128 | `		/* Point to the next entry */` |
|  454488 | 1129 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  227245 | 1130 | `	}` |
|    1754 | 1131 | `	return SXRET_OK;` |
|     878 | 1132 |  |
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
|   58768 | 1304 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1305 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1306 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1307 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1308 | `	)` |
|       2 | 1309 |  |
|       - | 1310 | `	ph7_hashmap *pMap;` |
|       - | 1311 | `	/* Allocate a new instance */` |
|   58770 | 1312 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   58770 | 1313 | `	if( pMap == 0 ){` |
|     ! 0 | 1314 | `		return 0;` |
|       - | 1315 | `	}` |
|       - | 1316 | `	/* Zero the structure */` |
|   58770 | 1317 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1318 | `	/* Fill in the structure */` |
|   58770 | 1319 | `	pMap->pVm = &(*pVm);` |
|   58770 | 1320 | `	pMap->iRef = 1;` |
|       - | 1321 | `	/* Default hash functions */` |
|   58770 | 1322 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   58770 | 1323 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   58770 | 1324 | `	return pMap;` |
|   29386 | 1325 |  |
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
|    1710 | 1346 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    1712 | 1366 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1712 | 1367 | `	if( pMap == 0 ){` |
|     ! 0 | 1368 | `		return SXERR_MEM;` |
|       - | 1369 | `	}` |
|    1712 | 1370 | `	pVm->pGlobal = pMap;` |
|       - | 1371 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1712 | 1372 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1712 | 1373 | `	if( pObj == 0 ){` |
|     ! 0 | 1374 | `		return SXERR_MEM;` |
|       - | 1375 | `	}` |
|    1712 | 1376 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1377 | `	/* Record object index */` |
|    1712 | 1378 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1379 | `	/* Install the special $GLOBALS array */` |
|    1712 | 1380 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1712 | 1381 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1382 | `		return rc;` |
|       - | 1383 | `	}` |
|       - | 1384 | `	/* Install superglobals now */` |
|   18812 | 1385 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1386 | `		ph7_value *pSuper;` |
|       - | 1387 | `		/* Request an empty array */` |
|   17102 | 1388 | `		pSuper = ph7_new_array(&(*pVm));` |
|   17102 | 1389 | `		if( pSuper == 0 ){` |
|     ! 0 | 1390 | `			return SXERR_MEM;` |
|       - | 1391 | `		}` |
|       - | 1392 | `		/* Install */` |
|   17102 | 1393 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   17102 | 1394 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1395 | `			return rc;` |
|       - | 1396 | `		}` |
|       - | 1397 | `		/* Release the value now it have been installed */` |
|   17102 | 1398 | `		ph7_release_value(&(*pVm),pSuper);` |
|    8552 | 1399 | `	}` |
|       - | 1400 | `	/* Set some $_SERVER entries */` |
|    1712 | 1401 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1402 | `	/*` |
|       - | 1403 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1404 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1405 | `	 */` |
|    3418 | 1406 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1407 | `		"SCRIPT_FILENAME",` |
|     855 | 1408 | `		pFile ? pFile->zString : ":Memory:",` |
|    1706 | 1409 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1410 | `		);` |
|       - | 1411 | `	/* All done,all super-global are installed now */` |
|    1712 | 1412 | `	return SXRET_OK;` |
|     857 | 1413 |  |
|       - | 1414 | `/*` |
|       - | 1415 | ` * Release a hashmap.` |
|       - | 1416 | ` */` |
|   39884 | 1417 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1418 |  |
|       - | 1419 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   39886 | 1420 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1421 | `	sxu32 n;` |
|   39886 | 1422 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1423 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1424 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1425 | `		return SXRET_OK;` |
|       - | 1426 | `	}` |
|       - | 1427 | `	/* Start the release process */` |
|   39886 | 1428 | `	n = 0;` |
|   39886 | 1429 | `	pEntry = pMap->pFirst;` |
| 1442895 | 1430 | `	for(;;){` |
| 2885792 | 1431 | `		if( n >= pMap->nEntry ){` |
|   39886 | 1432 | `			break;` |
|       - | 1433 | `		}` |
| 2845908 | 1434 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1435 | `		/* Remove the reference from the foreign table */` |
| 2845908 | 1436 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2845908 | 1437 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1438 | `			/* Restore the ph7_value to the free list */` |
| 2845900 | 1439 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1422949 | 1440 | `		}` |
|       - | 1441 | `		/* Release the node */` |
| 2845908 | 1442 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   58096 | 1443 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   29047 | 1444 | `		}` |
| 2845908 | 1445 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1446 | `		/* Point to the next entry */` |
| 2845908 | 1447 | `		pEntry = pNext;` |
| 2845908 | 1448 | `		n++;` |
|       2 | 1449 | `	}` |
|   39886 | 1450 | `	if( pMap->nEntry > 0 ){` |
|       - | 1451 | `		/* Release the hash bucket */` |
|   35488 | 1452 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   17743 | 1453 | `	}` |
|   39886 | 1454 | `	if( FreeDS ){` |
|       - | 1455 | `		/* Free the whole instance */` |
|   39870 | 1456 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   19936 | 1457 | `	}else{` |
|       - | 1458 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1459 | `		pMap->apBucket = 0;` |
|      17 | 1460 | `		pMap->iNextIdx = 0;` |
|      17 | 1461 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1462 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1463 | `	}` |
|   39886 | 1464 | `	return SXRET_OK;` |
|   19944 | 1465 |  |
|       - | 1466 | `/*` |
|       - | 1467 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1468 | ` * If the count reaches zero which mean no more variables` |
|       - | 1469 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1470 | ` */` |
|  453914 | 1471 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1472 |  |
|  453916 | 1473 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1474 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  453916 | 1475 | `	pMap->iRef--;` |
|  453916 | 1476 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   39870 | 1477 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   19934 | 1478 | `	}` |
|  453916 | 1479 |  |
|       - | 1480 | `/*` |
|       - | 1481 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1482 | ` * Write a pointer to the target node on success.` |
|       - | 1483 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1484 | ` */` |
|   82370 | 1485 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1486 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1487 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1488 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1489 | `	)` |
|       2 | 1490 |  |
|       - | 1491 | `	sxi32 rc;` |
|   82372 | 1492 | `	if( pMap->nEntry < 1 ){` |
|       - | 1493 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1494 | `		 */` |
|      19 | 1495 | `		return SXERR_NOTFOUND;` |
|       - | 1496 | `	}` |
|   82354 | 1497 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   82354 | 1498 | `	return rc;` |
|   41187 | 1499 |  |
|       - | 1500 | `/*` |
|       - | 1501 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1502 | ` * hashmap.` |
|       - | 1503 | ` * If a node with the given key already exists in the database` |
|       - | 1504 | ` * then this function overwrite the old value.` |
|       - | 1505 | ` */` |
| 2397664 | 1506 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1507 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1508 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1509 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1510 | `	)` |
|       2 | 1511 |  |
|       - | 1512 | `	sxi32 rc;` |
| 2397666 | 1513 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1514 | `		/*` |
|       - | 1515 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1516 | `		 */` |
|     ! 0 | 1517 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1518 | `		return SXRET_OK;` |
|       - | 1519 | `	}` |
| 2397666 | 1520 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2397666 | 1521 | `	return rc;` |
| 1198834 | 1522 |  |
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
|   22628 | 1550 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1551 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1552 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1553 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1554 | `	)` |
|       2 | 1555 |  |
|       - | 1556 | `	sxi32 rc;` |
|   22630 | 1557 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1558 | `		/*` |
|       - | 1559 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1560 | `		 */` |
|     ! 0 | 1561 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1562 | `		return SXRET_OK;` |
|       - | 1563 | `	}` |
|   22630 | 1564 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   22630 | 1565 | `	return rc;` |
|   11316 | 1566 |  |
|       - | 1567 | `/*` |
|       - | 1568 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1569 | ` */` |
|   17724 | 1570 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1571 |  |
|       - | 1572 | `	/* Reset the loop cursor */` |
|   17726 | 1573 | `	pMap->pCur = pMap->pFirst;` |
|   17726 | 1574 |  |
|       - | 1575 | `/*` |
|       - | 1576 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1577 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1578 | ` * return NULL.` |
|       - | 1579 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1580 | ` */` |
|  143016 | 1581 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1582 |  |
|  143018 | 1583 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  143018 | 1584 | `	if( pCur == 0 ){` |
|       - | 1585 | `		/* End of the list,return null */` |
|    8884 | 1586 | `		return 0;` |
|       - | 1587 | `	}` |
|       - | 1588 | `	/* Advance the node cursor */` |
|  134136 | 1589 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  134136 | 1590 | `	return pCur;` |
|   71510 | 1591 |  |
|       - | 1592 | `/*` |
|       - | 1593 | ` * Extract a node value.` |
|       - | 1594 | ` */` |
|  338958 | 1595 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1596 |  |
|  338960 | 1597 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  338960 | 1598 | `	if( pEntry ){` |
|  338960 | 1599 | `		if( bStore ){` |
|  134164 | 1600 | `			PH7_MemObjStore(pEntry,pValue);` |
|   67083 | 1601 | `		}else{` |
|  204798 | 1602 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1603 | `		}` |
|  169568 | 1604 | `	}else{` |
|     ! 0 | 1605 | `		PH7_MemObjRelease(pValue);` |
|       - | 1606 | `	}` |
|  338960 | 1607 |  |
|       - | 1608 | `/*` |
|       - | 1609 | ` * Extract a node key.` |
|       - | 1610 | ` */` |
|   89440 | 1611 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1612 |  |
|       - | 1613 | `	/* Fill with the current key */` |
|   89442 | 1614 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   89266 | 1615 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      13 | 1616 | `			SyBlobRelease(&pKey->sBlob);` |
|       6 | 1617 | `		}` |
|   89266 | 1618 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   89266 | 1619 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   44634 | 1620 | `	}else{` |
|     177 | 1621 | `		SyBlobReset(&pKey->sBlob);` |
|     177 | 1622 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     177 | 1623 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1624 | `	}` |
|   89442 | 1625 |  |
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
|   24866 | 1673 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1674 |  |
|       - | 1675 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1676 | `    /* Prevent compiler warning */` |
|   24868 | 1677 | `	result.pNext = result.pPrev = 0;` |
|   24868 | 1678 | `	pTail = &result;` |
|   63785 | 1679 | `	while( pA && pB ){` |
|   38919 | 1680 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|   25705 | 1681 | `			pTail->pPrev = pA;` |
|   25705 | 1682 | `			pA->pNext = pTail;` |
|   25705 | 1683 | `			pTail = pA;` |
|   25705 | 1684 | `			pA = pA->pPrev;` |
|   12864 | 1685 | `		}else{` |
|   13216 | 1686 | `			pTail->pPrev = pB;` |
|   13216 | 1687 | `			pB->pNext = pTail;` |
|   13216 | 1688 | `			pTail = pB;` |
|   13216 | 1689 | `			pB = pB->pPrev;` |
|       - | 1690 | `		}` |
|       2 | 1691 | `	}` |
|   24868 | 1692 | `	if( pA ){` |
|   18411 | 1693 | `		pTail->pPrev = pA;` |
|   18411 | 1694 | `		pA->pNext = pTail;` |
|   15670 | 1695 | `	}else if( pB ){` |
|    6291 | 1696 | `		pTail->pPrev = pB;` |
|    6291 | 1697 | `		pB->pNext = pTail;` |
|    3140 | 1698 | `	}else{` |
|     170 | 1699 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1700 | `	}` |
|   24868 | 1701 | `	return result.pPrev;` |
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
|     558 | 1715 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1716 |  |
|       - | 1717 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1718 | `	sxu32 i;` |
|     560 | 1719 | `	SyZero(a,sizeof(a));` |
|       - | 1720 | `	/* Point to the first inserted entry */` |
|     560 | 1721 | `	pIn = pMap->pFirst;` |
|    9074 | 1722 | `	while( pIn ){` |
|    8516 | 1723 | `		p = pIn;` |
|    8516 | 1724 | `		pIn = p->pPrev;` |
|    8516 | 1725 | `		p->pPrev = 0;` |
|   16084 | 1726 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   16084 | 1727 | `			if( a[i]==0 ){` |
|    8516 | 1728 | `				a[i] = p;` |
|    8516 | 1729 | `				break;` |
|     ! 0 | 1730 | `			}else{` |
|    7570 | 1731 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    7570 | 1732 | `				a[i] = 0;` |
|       - | 1733 | `			}` |
|    3786 | 1734 | `		}` |
|    8516 | 1735 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1736 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1737 | `			 * But that is impossible.` |
|       - | 1738 | `			 */` |
|     ! 0 | 1739 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1740 | `		}` |
|       2 | 1741 | `	}` |
|     560 | 1742 | `	p = a[0];` |
|   17858 | 1743 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   17300 | 1744 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    8651 | 1745 | `	}` |
|     560 | 1746 | `	p->pNext = 0;` |
|       - | 1747 | `	/* Reflect the change */` |
|     560 | 1748 | `	pMap->pFirst = p;` |
|       - | 1749 | `	/* Reset the loop cursor */` |
|     560 | 1750 | `	pMap->pCur = pMap->pFirst;` |
|     560 | 1751 | `	return SXRET_OK;` |
|       2 | 1752 |  |
|       - | 1753 | `/*` |
|       - | 1754 | ` * Node comparison callback.` |
|       - | 1755 | ` * used-by: [sort(),asort(),...]` |
|       - | 1756 | ` */` |
|   38855 | 1757 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1758 |  |
|       - | 1759 | `	ph7_value sA,sB;` |
|       - | 1760 | `	sxi32 iFlags;` |
|       - | 1761 | `	int rc;` |
|   38857 | 1762 | `	if( pCmpData == 0 ){` |
|       - | 1763 | `		/* Perform a standard comparison */` |
|   38853 | 1764 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   38853 | 1765 | `		return rc;` |
|       - | 1766 | `	}` |
|       5 | 1767 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1768 | `	/* Duplicate node values */` |
|       5 | 1769 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|       5 | 1770 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|       5 | 1771 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|       5 | 1772 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|       5 | 1773 | `	if( iFlags == 5 ){` |
|       - | 1774 | `		/* String cast */` |
|       5 | 1775 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1776 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1777 | `		}` |
|       5 | 1778 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1779 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1780 | `		}` |
|       3 | 1781 | `	}else{` |
|       - | 1782 | `		/* Numeric cast */` |
|     ! 0 | 1783 | `		PH7_MemObjToNumeric(&sA);` |
|     ! 0 | 1784 | `		PH7_MemObjToNumeric(&sB);` |
|       - | 1785 | `	}` |
|       - | 1786 | `	/* Perform the comparison */` |
|       5 | 1787 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       5 | 1788 | `	PH7_MemObjRelease(&sA);` |
|       5 | 1789 | `	PH7_MemObjRelease(&sB);` |
|       5 | 1790 | `	return rc;` |
|   19473 | 1791 |  |
|       - | 1792 | `/*` |
|       - | 1793 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1794 | ` * used-by: [ksort()]` |
|       - | 1795 | ` */` |
|      14 | 1796 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1797 |  |
|       - | 1798 | `	sxi32 rc;` |
|       7 | 1799 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 1800 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1801 | `		/* Perform a string comparison */` |
|       5 | 1802 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1803 | `	}else{` |
|       - | 1804 | `		SyString sStr;` |
|       - | 1805 | `		sxi64 iA,iB;` |
|       - | 1806 | `		/* Perform a numeric comparison */` |
|      11 | 1807 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1808 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1809 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1810 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1811 | `				iA = 0;` |
|     ! 0 | 1812 | `			}else{` |
|     ! 0 | 1813 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1814 | `			}` |
|     ! 0 | 1815 | `		}else{` |
|      11 | 1816 | `			iA = pA->xKey.iKey;` |
|       - | 1817 | `		}` |
|      11 | 1818 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1819 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1820 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1821 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1822 | `				iB = 0;` |
|     ! 0 | 1823 | `			}else{` |
|     ! 0 | 1824 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1825 | `			}` |
|     ! 0 | 1826 | `		}else{` |
|      11 | 1827 | `			iB = pB->xKey.iKey;` |
|       - | 1828 | `		}` |
|      11 | 1829 | `		rc = (sxi32)(iA-iB);` |
|       - | 1830 | `	}` |
|       - | 1831 | `	/* Comparison result */` |
|      15 | 1832 | `	return rc;` |
|       1 | 1833 |  |
|       - | 1834 | `/*` |
|       - | 1835 | ` * Node comparison callback.` |
|       - | 1836 | ` * Used by: [rsort(),arsort()];` |
|       - | 1837 | ` */` |
|      12 | 1838 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1839 |  |
|       - | 1840 | `	ph7_value sA,sB;` |
|       - | 1841 | `	sxi32 iFlags;` |
|       - | 1842 | `	int rc;` |
|      13 | 1843 | `	if( pCmpData == 0 ){` |
|       - | 1844 | `		/* Perform a standard comparison */` |
|      13 | 1845 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      13 | 1846 | `		return -rc;` |
|       - | 1847 | `	}` |
|     ! 0 | 1848 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1849 | `	/* Duplicate node values */` |
|     ! 0 | 1850 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|     ! 0 | 1851 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|     ! 0 | 1852 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|     ! 0 | 1853 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|     ! 0 | 1854 | `	if( iFlags == 5 ){` |
|       - | 1855 | `		/* String cast */` |
|     ! 0 | 1856 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1857 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1858 | `		}` |
|     ! 0 | 1859 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1860 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1861 | `		}` |
|     ! 0 | 1862 | `	}else{` |
|       - | 1863 | `		/* Numeric cast */` |
|     ! 0 | 1864 | `		PH7_MemObjToNumeric(&sA);` |
|     ! 0 | 1865 | `		PH7_MemObjToNumeric(&sB);` |
|       - | 1866 | `	}` |
|       - | 1867 | `	/* Perform the comparison */` |
|     ! 0 | 1868 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|     ! 0 | 1869 | `	PH7_MemObjRelease(&sA);` |
|     ! 0 | 1870 | `	PH7_MemObjRelease(&sB);` |
|     ! 0 | 1871 | `	return -rc;` |
|       7 | 1872 |  |
|       - | 1873 | `/*` |
|       - | 1874 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1875 | ` * used-by: [usort(),uasort()]` |
|       - | 1876 | ` */` |
|      12 | 1877 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1878 |  |
|       - | 1879 | `	ph7_value sResult,*pCallback;` |
|       - | 1880 | `	ph7_value *pV1,*pV2;` |
|       - | 1881 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1882 | `	sxi32 rc;` |
|       - | 1883 | `	/* Point to the desired callback */` |
|      13 | 1884 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1885 | `	/* initialize the result value */` |
|      13 | 1886 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 1887 | `	/* Extract nodes values */` |
|      13 | 1888 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      13 | 1889 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      13 | 1890 | `	apArg[0] = pV1;` |
|      13 | 1891 | `	apArg[1] = pV2;` |
|       - | 1892 | `	/* Invoke the callback */` |
|      13 | 1893 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      13 | 1894 | `	if( rc != SXRET_OK ){` |
|       - | 1895 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1896 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1897 | `	}else{` |
|       - | 1898 | `		/* Extract callback result */` |
|      13 | 1899 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1900 | `			/* Perform an int cast */` |
|     ! 0 | 1901 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1902 | `		}` |
|      13 | 1903 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1904 | `	}` |
|      13 | 1905 | `	PH7_MemObjRelease(&sResult);` |
|       - | 1906 | `	/* Callback result */` |
|      13 | 1907 | `	return rc;` |
|       1 | 1908 |  |
|       - | 1909 | `/*` |
|       - | 1910 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1911 | ` * used-by: [krsort()]` |
|       - | 1912 | ` */` |
|       4 | 1913 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1914 |  |
|       - | 1915 | `	sxi32 rc;` |
|       2 | 1916 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 1917 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1918 | `		/* Perform a string comparison */` |
|       5 | 1919 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1920 | `	}else{` |
|       - | 1921 | `		SyString sStr;` |
|       - | 1922 | `		sxi64 iA,iB;` |
|       - | 1923 | `		/* Perform a numeric comparison */` |
|     ! 0 | 1924 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1925 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1926 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1927 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1928 | `				iA = 0;` |
|     ! 0 | 1929 | `			}else{` |
|     ! 0 | 1930 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1931 | `			}` |
|     ! 0 | 1932 | `		}else{` |
|     ! 0 | 1933 | `			iA = pA->xKey.iKey;` |
|       - | 1934 | `		}` |
|     ! 0 | 1935 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1936 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1937 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1938 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1939 | `				iB = 0;` |
|     ! 0 | 1940 | `			}else{` |
|     ! 0 | 1941 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1942 | `			}` |
|     ! 0 | 1943 | `		}else{` |
|     ! 0 | 1944 | `			iB = pB->xKey.iKey;` |
|       - | 1945 | `		}` |
|     ! 0 | 1946 | `		rc = (sxi32)(iA-iB);` |
|       - | 1947 | `	}` |
|       5 | 1948 | `	return -rc; /* Reverse result */` |
|       1 | 1949 |  |
|       - | 1950 | `/*` |
|       - | 1951 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1952 | ` * used-by: [uksort()]` |
|       - | 1953 | ` */` |
|       6 | 1954 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1955 |  |
|       - | 1956 | `	ph7_value sResult,*pCallback;` |
|       - | 1957 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1958 | `	ph7_value sK1,sK2;` |
|       - | 1959 | `	sxi32 rc;` |
|       - | 1960 | `	/* Point to the desired callback */` |
|       7 | 1961 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1962 | `	/* initialize the result value */` |
|       7 | 1963 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 1964 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 1965 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 1966 | `	/* Extract nodes keys */` |
|       7 | 1967 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 1968 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 1969 | `	apArg[0] = &sK1;` |
|       7 | 1970 | `	apArg[1] = &sK2;` |
|       - | 1971 | `	/* Mark keys as constants */` |
|       7 | 1972 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 1973 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 1974 | `	/* Invoke the callback */` |
|       7 | 1975 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 1976 | `	if( rc != SXRET_OK ){` |
|       - | 1977 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1978 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1979 | `	}else{` |
|       - | 1980 | `		/* Extract callback result */` |
|       7 | 1981 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1982 | `			/* Perform an int cast */` |
|     ! 0 | 1983 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1984 | `		}` |
|       7 | 1985 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1986 | `	}` |
|       7 | 1987 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 1988 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 1989 | `	PH7_MemObjRelease(&sK2);` |
|       - | 1990 | `	/* Callback result */` |
|       7 | 1991 | `	return rc;` |
|       1 | 1992 |  |
|       - | 1993 | `/*` |
|       - | 1994 | ` * Node comparison callback: Random node comparison.` |
|       - | 1995 | ` * used-by: [shuffle()]` |
|       - | 1996 | ` */` |
|      14 | 1997 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1998 |  |
|       - | 1999 | `	sxu32 n;` |
|       7 | 2000 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 2001 | `	SXUNUSED(pCmpData);` |
|       - | 2002 | `	/* Grab a random number */` |
|      15 | 2003 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2004 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2005 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2006 | `	 */` |
|      15 | 2007 | `	return n&1 ? 1 : -1;` |
|       1 | 2008 |  |
|       - | 2009 | `/*` |
|       - | 2010 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2011 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2012 | ` */` |
|     542 | 2013 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2014 |  |
|       - | 2015 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2016 | `	sxu32 i;` |
|       - | 2017 | `	/* Rehash all entries */` |
|     544 | 2018 | `	pLast = p = pMap->pFirst;` |
|     544 | 2019 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     544 | 2020 | `	i = 0;` |
|    4501 | 2021 | `	for( ;; ){` |
|    9004 | 2022 | `		if( i >= pMap->nEntry ){` |
|     544 | 2023 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     544 | 2024 | `			break;` |
|       - | 2025 | `		}` |
|    8462 | 2026 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2027 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2028 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2029 | `			/* Change key type */` |
|       5 | 2030 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2031 | `		}` |
|    8462 | 2032 | `		HashmapRehashIntNode(p);` |
|       - | 2033 | `		/* Point to the next entry */` |
|    8462 | 2034 | `		i++;` |
|    8462 | 2035 | `		pLast = p;` |
|    8462 | 2036 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2037 | `	}` |
|     544 | 2038 |  |
|       - | 2039 | `/*` |
|       - | 2040 | ` * Array functions implementation.` |
|       - | 2041 | ` * Status:` |
|       - | 2042 | ` *  Stable.` |
|       - | 2043 | ` */` |
|       - | 2044 | `/*` |
|       - | 2045 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2046 | ` * Sort an array.` |
|       - | 2047 | ` * Parameters` |
|       - | 2048 | ` *  $array` |
|       - | 2049 | ` *   The input array.` |
|       - | 2050 | ` * $sort_flags` |
|       - | 2051 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2052 | ` *  Sorting type flags:` |
|       - | 2053 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2054 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2055 | ` *   SORT_STRING - compare items as strings` |
|       - | 2056 | ` * Return` |
|       - | 2057 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2058 | ` *` |
|       - | 2059 | ` */` |
|     848 | 2060 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2061 |  |
|       - | 2062 | `	ph7_hashmap *pMap;` |
|       - | 2063 | `	/* Make sure we are dealing with a valid hashmap */` |
|     850 | 2064 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2065 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2066 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2067 | `		return PH7_OK;` |
|       - | 2068 | `	}` |
|       - | 2069 | `	/* Point to the internal representation of the input hashmap */` |
|     850 | 2070 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     850 | 2071 | `	if( pMap->nEntry > 1 ){` |
|     538 | 2072 | `		sxi32 iCmpFlags = 0;` |
|     538 | 2073 | `		if( nArg > 1 ){` |
|       - | 2074 | `			/* Extract comparison flags */` |
|       3 | 2075 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2076 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2077 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2078 | `			}` |
|       1 | 2079 | `		}` |
|       - | 2080 | `		/* Do the merge sort */` |
|     538 | 2081 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2082 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     538 | 2083 | `		HashmapSortRehash(pMap);` |
|     268 | 2084 | `	}` |
|       - | 2085 | `	/* All done,return TRUE */` |
|     850 | 2086 | `	ph7_result_bool(pCtx,1);` |
|     850 | 2087 | `	return PH7_OK;` |
|     426 | 2088 |  |
|       - | 2089 | `/*` |
|       - | 2090 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2091 | ` *  Sort an array and maintain index association.` |
|       - | 2092 | ` * Parameters` |
|       - | 2093 | ` *  $array` |
|       - | 2094 | ` *   The input array.` |
|       - | 2095 | ` * $sort_flags` |
|       - | 2096 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2097 | ` *  Sorting type flags:` |
|       - | 2098 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2099 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2100 | ` *   SORT_STRING - compare items as strings` |
|       - | 2101 | ` * Return` |
|       - | 2102 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2103 | ` */` |
|       2 | 2104 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2105 |  |
|       - | 2106 | `	ph7_hashmap *pMap;` |
|       - | 2107 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2108 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2109 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2110 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2111 | `		return PH7_OK;` |
|       - | 2112 | `	}` |
|       - | 2113 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2114 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2115 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2116 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2117 | `		if( nArg > 1 ){` |
|       - | 2118 | `			/* Extract comparison flags */` |
|     ! 0 | 2119 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2120 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2121 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2122 | `			}` |
|     ! 0 | 2123 | `		}` |
|       - | 2124 | `		/* Do the merge sort */` |
|       3 | 2125 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2126 | `		/* Fix the last link broken by the merge */` |
|       5 | 2127 | `		while(pMap->pLast->pPrev){` |
|       3 | 2128 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2129 | `		}` |
|       1 | 2130 | `	}` |
|       - | 2131 | `	/* All done,return TRUE */` |
|       3 | 2132 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2133 | `	return PH7_OK;` |
|       2 | 2134 |  |
|       - | 2135 | `/*` |
|       - | 2136 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2137 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2138 | ` * Parameters` |
|       - | 2139 | ` *  $array` |
|       - | 2140 | ` *   The input array.` |
|       - | 2141 | ` * $sort_flags` |
|       - | 2142 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2143 | ` *  Sorting type flags:` |
|       - | 2144 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2145 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2146 | ` *   SORT_STRING - compare items as strings` |
|       - | 2147 | ` * Return` |
|       - | 2148 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2149 | ` */` |
|       2 | 2150 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2151 |  |
|       - | 2152 | `	ph7_hashmap *pMap;` |
|       - | 2153 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2154 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2155 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2156 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2157 | `		return PH7_OK;` |
|       - | 2158 | `	}` |
|       - | 2159 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2160 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2161 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2162 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2163 | `		if( nArg > 1 ){` |
|       - | 2164 | `			/* Extract comparison flags */` |
|     ! 0 | 2165 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2166 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2167 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2168 | `			}` |
|     ! 0 | 2169 | `		}` |
|       - | 2170 | `		/* Do the merge sort */` |
|       3 | 2171 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2172 | `		/* Fix the last link broken by the merge */` |
|       5 | 2173 | `		while(pMap->pLast->pPrev){` |
|       3 | 2174 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2175 | `		}` |
|       1 | 2176 | `	}` |
|       - | 2177 | `	/* All done,return TRUE */` |
|       3 | 2178 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2179 | `	return PH7_OK;` |
|       2 | 2180 |  |
|       - | 2181 | `/*` |
|       - | 2182 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2183 | ` *  Sort an array by key.` |
|       - | 2184 | ` * Parameters` |
|       - | 2185 | ` *  $array` |
|       - | 2186 | ` *   The input array.` |
|       - | 2187 | ` * $sort_flags` |
|       - | 2188 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2189 | ` *  Sorting type flags:` |
|       - | 2190 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2191 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2192 | ` *   SORT_STRING - compare items as strings` |
|       - | 2193 | ` * Return` |
|       - | 2194 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2195 | ` */` |
|       4 | 2196 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2197 |  |
|       - | 2198 | `	ph7_hashmap *pMap;` |
|       - | 2199 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2200 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2201 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2202 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2203 | `		return PH7_OK;` |
|       - | 2204 | `	}` |
|       - | 2205 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2206 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2207 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2208 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2209 | `		if( nArg > 1 ){` |
|       - | 2210 | `			/* Extract comparison flags */` |
|     ! 0 | 2211 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2212 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2213 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2214 | `			}` |
|     ! 0 | 2215 | `		}` |
|       - | 2216 | `		/* Do the merge sort */` |
|       5 | 2217 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2218 | `		/* Fix the last link broken by the merge */` |
|      15 | 2219 | `		while(pMap->pLast->pPrev){` |
|      11 | 2220 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2221 | `		}` |
|       2 | 2222 | `	}` |
|       - | 2223 | `	/* All done,return TRUE */` |
|       5 | 2224 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2225 | `	return PH7_OK;` |
|       3 | 2226 |  |
|       - | 2227 | `/*` |
|       - | 2228 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2229 | ` *  Sort an array by key in reverse order.` |
|       - | 2230 | ` * Parameters` |
|       - | 2231 | ` *  $array` |
|       - | 2232 | ` *   The input array.` |
|       - | 2233 | ` * $sort_flags` |
|       - | 2234 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2235 | ` *  Sorting type flags:` |
|       - | 2236 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2237 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2238 | ` *   SORT_STRING - compare items as strings` |
|       - | 2239 | ` * Return` |
|       - | 2240 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2241 | ` */` |
|       2 | 2242 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2243 |  |
|       - | 2244 | `	ph7_hashmap *pMap;` |
|       - | 2245 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2246 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2247 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2248 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2249 | `		return PH7_OK;` |
|       - | 2250 | `	}` |
|       - | 2251 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2252 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2253 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2254 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2255 | `		if( nArg > 1 ){` |
|       - | 2256 | `			/* Extract comparison flags */` |
|     ! 0 | 2257 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2258 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2259 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2260 | `			}` |
|     ! 0 | 2261 | `		}` |
|       - | 2262 | `		/* Do the merge sort */` |
|       3 | 2263 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2264 | `		/* Fix the last link broken by the merge */` |
|       7 | 2265 | `		while(pMap->pLast->pPrev){` |
|       5 | 2266 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2267 | `		}` |
|       1 | 2268 | `	}` |
|       - | 2269 | `	/* All done,return TRUE */` |
|       3 | 2270 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2271 | `	return PH7_OK;` |
|       2 | 2272 |  |
|       - | 2273 | `/*` |
|       - | 2274 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2275 | ` * Sort an array in reverse order.` |
|       - | 2276 | ` * Parameters` |
|       - | 2277 | ` *  $array` |
|       - | 2278 | ` *   The input array.` |
|       - | 2279 | ` * $sort_flags` |
|       - | 2280 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2281 | ` *  Sorting type flags:` |
|       - | 2282 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2283 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2284 | ` *   SORT_STRING - compare items as strings` |
|       - | 2285 | ` * Return` |
|       - | 2286 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2287 | ` */` |
|       2 | 2288 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2289 |  |
|       - | 2290 | `	ph7_hashmap *pMap;` |
|       - | 2291 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2292 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2293 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2294 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2295 | `		return PH7_OK;` |
|       - | 2296 | `	}` |
|       - | 2297 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2298 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2299 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2300 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2301 | `		if( nArg > 1 ){` |
|       - | 2302 | `			/* Extract comparison flags */` |
|     ! 0 | 2303 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2304 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2305 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2306 | `			}` |
|     ! 0 | 2307 | `		}` |
|       - | 2308 | `		/* Do the merge sort */` |
|       3 | 2309 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2310 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2311 | `		HashmapSortRehash(pMap);` |
|       1 | 2312 | `	}` |
|       - | 2313 | `	/* All done,return TRUE */` |
|       3 | 2314 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2315 | `	return PH7_OK;` |
|       2 | 2316 |  |
|       - | 2317 | `/*` |
|       - | 2318 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2319 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2320 | ` * Parameters` |
|       - | 2321 | ` *  $array` |
|       - | 2322 | ` *   The input array.` |
|       - | 2323 | ` * $cmp_function` |
|       - | 2324 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2325 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2326 | ` *  to, or greater than the second.` |
|       - | 2327 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2328 | ` * Return` |
|       - | 2329 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2330 | ` */` |
|       2 | 2331 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2332 |  |
|       - | 2333 | `	ph7_hashmap *pMap;` |
|       - | 2334 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2335 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2336 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2337 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2338 | `		return PH7_OK;` |
|       - | 2339 | `	}` |
|       - | 2340 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2341 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2342 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2343 | `		ph7_value *pCallback = 0;` |
|       - | 2344 | `		ProcNodeCmp xCmp;` |
|       3 | 2345 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2346 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2347 | `			/* Point to the desired callback */` |
|       3 | 2348 | `			pCallback = apArg[1];` |
|       2 | 2349 | `		}else{` |
|       - | 2350 | `			/* Use the default comparison function */` |
|     ! 0 | 2351 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2352 | `		}` |
|       - | 2353 | `		/* Do the merge sort */` |
|       3 | 2354 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2355 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2356 | `		HashmapSortRehash(pMap);` |
|       1 | 2357 | `	}` |
|       - | 2358 | `	/* All done,return TRUE */` |
|       3 | 2359 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2360 | `	return PH7_OK;` |
|       2 | 2361 |  |
|       - | 2362 | `/*` |
|       - | 2363 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2364 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2365 | ` *  and maintain index association.` |
|       - | 2366 | ` * Parameters` |
|       - | 2367 | ` *  $array` |
|       - | 2368 | ` *   The input array.` |
|       - | 2369 | ` * $cmp_function` |
|       - | 2370 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2371 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2372 | ` *  to, or greater than the second.` |
|       - | 2373 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2374 | ` * Return` |
|       - | 2375 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2376 | ` */` |
|       2 | 2377 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2378 |  |
|       - | 2379 | `	ph7_hashmap *pMap;` |
|       - | 2380 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2381 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2382 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2383 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2384 | `		return PH7_OK;` |
|       - | 2385 | `	}` |
|       - | 2386 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2387 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2388 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2389 | `		ph7_value *pCallback = 0;` |
|       - | 2390 | `		ProcNodeCmp xCmp;` |
|       3 | 2391 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2392 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2393 | `			/* Point to the desired callback */` |
|       3 | 2394 | `			pCallback = apArg[1];` |
|       2 | 2395 | `		}else{` |
|       - | 2396 | `			/* Use the default comparison function */` |
|     ! 0 | 2397 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2398 | `		}` |
|       - | 2399 | `		/* Do the merge sort */` |
|       3 | 2400 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2401 | `		/* Fix the last link broken by the merge */` |
|       5 | 2402 | `		while(pMap->pLast->pPrev){` |
|       3 | 2403 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2404 | `		}` |
|       1 | 2405 | `	}` |
|       - | 2406 | `	/* All done,return TRUE */` |
|       3 | 2407 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2408 | `	return PH7_OK;` |
|       2 | 2409 |  |
|       - | 2410 | `/*` |
|       - | 2411 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2412 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2413 | ` *  function and maintain index association.` |
|       - | 2414 | ` * Parameters` |
|       - | 2415 | ` *  $array` |
|       - | 2416 | ` *   The input array.` |
|       - | 2417 | ` * $cmp_function` |
|       - | 2418 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2419 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2420 | ` *  to, or greater than the second.` |
|       - | 2421 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2422 | ` * Return` |
|       - | 2423 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2424 | ` */` |
|       2 | 2425 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2426 |  |
|       - | 2427 | `	ph7_hashmap *pMap;` |
|       - | 2428 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2429 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2430 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2431 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2432 | `		return PH7_OK;` |
|       - | 2433 | `	}` |
|       - | 2434 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2435 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2436 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2437 | `		ph7_value *pCallback = 0;` |
|       - | 2438 | `		ProcNodeCmp xCmp;` |
|       3 | 2439 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2440 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2441 | `			/* Point to the desired callback */` |
|       3 | 2442 | `			pCallback = apArg[1];` |
|       2 | 2443 | `		}else{` |
|       - | 2444 | `			/* Use the default comparison function */` |
|     ! 0 | 2445 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2446 | `		}` |
|       - | 2447 | `		/* Do the merge sort */` |
|       3 | 2448 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2449 | `		/* Fix the last link broken by the merge */` |
|       3 | 2450 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2451 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2452 | `		}` |
|       1 | 2453 | `	}` |
|       - | 2454 | `	/* All done,return TRUE */` |
|       3 | 2455 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2456 | `	return PH7_OK;` |
|       2 | 2457 |  |
|       - | 2458 | `/*` |
|       - | 2459 | ` * bool shuffle(array &$array)` |
|       - | 2460 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2461 | ` * Parameters` |
|       - | 2462 | ` *  $array` |
|       - | 2463 | ` *   The input array.` |
|       - | 2464 | ` * Return` |
|       - | 2465 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2466 | ` *` |
|       - | 2467 | ` */` |
|       2 | 2468 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2469 |  |
|       - | 2470 | `	ph7_hashmap *pMap;` |
|       - | 2471 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2472 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2473 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2474 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2475 | `		return PH7_OK;` |
|       - | 2476 | `	}` |
|       - | 2477 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2478 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2479 | `	if( pMap->nEntry > 1 ){` |
|       - | 2480 | `		/* Do the merge sort */` |
|       3 | 2481 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2482 | `		/* Fix the last link broken by the merge */` |
|      10 | 2483 | `		while(pMap->pLast->pPrev){` |
|       8 | 2484 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2485 | `		}` |
|       1 | 2486 | `	}` |
|       - | 2487 | `	/* All done,return TRUE */` |
|       3 | 2488 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2489 | `	return PH7_OK;` |
|       2 | 2490 |  |
|       - | 2491 | `/*` |
|       - | 2492 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2493 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2494 | ` * Parameters` |
|       - | 2495 | ` *  $var` |
|       - | 2496 | ` *   The array or the object.` |
|       - | 2497 | ` * $mode` |
|       - | 2498 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2499 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2500 | ` *  all the elements of a multidimensional array.` |
|       - | 2501 | ` * Return` |
|       - | 2502 | ` *  Returns the number of elements in the array.` |
|       - | 2503 | ` */` |
|     638 | 2504 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2505 |  |
|     640 | 2506 | `	int bRecursive = FALSE;` |
|     640 | 2507 | `	int bCycleDetected = FALSE;` |
|       - | 2508 | `	sxi64 iCount;` |
|     640 | 2509 | `	if( nArg < 1 ){` |
|       3 | 2510 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2511 | `			"ArgumentCountError",` |
|       - | 2512 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2513 | `			);` |
|       - | 2514 | `	}` |
|     638 | 2515 | `	if( nArg > 2 ){` |
|       4 | 2516 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2517 | `			"ArgumentCountError",` |
|       - | 2518 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2519 | `			nArg` |
|       - | 2520 | `			);` |
|       - | 2521 | `	}` |
|     636 | 2522 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2523 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2524 | `			"TypeError",` |
|       - | 2525 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       5 | 2526 | `			ph7_type_name(apArg[0])` |
|       - | 2527 | `			);` |
|       - | 2528 | `	}` |
|     626 | 2529 | `	if( nArg > 1 ){` |
|      34 | 2530 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      34 | 2531 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       5 | 2532 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2533 | `				"ValueError",` |
|       - | 2534 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2535 | `				);` |
|       - | 2536 | `		}` |
|      29 | 2537 | `		bRecursive = iMode == 1;` |
|      14 | 2538 | `	}` |
|       - | 2539 | `	/* Count */` |
|     622 | 2540 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     622 | 2541 | `	if( bCycleDetected ){` |
|       3 | 2542 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2543 | `	}` |
|     622 | 2544 | `	ph7_result_int64(pCtx,iCount);` |
|     622 | 2545 | `	return PH7_OK;` |
|     321 | 2546 |  |
|       - | 2547 | `/*` |
|       - | 2548 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2549 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2550 | ` * Parameters` |
|       - | 2551 | ` * $key` |
|       - | 2552 | ` *   Value to check.` |
|       - | 2553 | ` * $search` |
|       - | 2554 | ` *  An array with keys to check.` |
|       - | 2555 | ` * Return` |
|       - | 2556 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2557 | ` */` |
|      66 | 2558 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2559 |  |
|       - | 2560 | `	sxi32 rc;` |
|      68 | 2561 | `	if( nArg != 2 ){` |
|       - | 2562 | `		/* PHP requires exactly two arguments */` |
|      10 | 2563 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2564 | `			"ArgumentCountError",` |
|       - | 2565 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2566 | `			nArg` |
|       - | 2567 | `			);` |
|       - | 2568 | `	}` |
|       - | 2569 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 2570 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2571 | `		/* Type mismatch -> TypeError */` |
|       7 | 2572 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2573 | `			"TypeError",` |
|       - | 2574 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2575 | `			ph7_type_name(apArg[1])` |
|       - | 2576 | `			);` |
|       - | 2577 | `	}` |
|       - | 2578 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      57 | 2579 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2580 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2581 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2582 | `			"use an empty string instead"` |
|       - | 2583 | `			);` |
|      56 | 2584 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2585 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2586 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2587 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2588 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2589 | `				,rVal` |
|       - | 2590 | `				);` |
|       1 | 2591 | `		}` |
|       1 | 2592 | `	}` |
|       - | 2593 | `	/* Perform the lookup */` |
|      57 | 2594 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2595 | `	/* lookup result */` |
|      57 | 2596 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      57 | 2597 | `	return PH7_OK;` |
|      35 | 2598 |  |
|       - | 2599 | `/*` |
|       - | 2600 | ` * value array_pop(array $array)` |
|       - | 2601 | ` *   POP the last inserted element from the array.` |
|       - | 2602 | ` * Parameter` |
|       - | 2603 | ` *  The array to get the value from.` |
|       - | 2604 | ` * Return` |
|       - | 2605 | ` *  Poped value or NULL on failure.` |
|       - | 2606 | ` */` |
|      16 | 2607 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2608 |  |
|       - | 2609 | `	ph7_hashmap *pMap;` |
|       - | 2610 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      18 | 2611 | `	if( nArg != 1 ){` |
|       7 | 2612 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2613 | `			"ArgumentCountError",` |
|       - | 2614 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2615 | `			nArg` |
|       - | 2616 | `			);` |
|       - | 2617 | `	}` |
|       - | 2618 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2619 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      14 | 2620 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2621 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2622 | `			"Error",` |
|       - | 2623 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2624 | `			);` |
|       - | 2625 | `	}` |
|       - | 2626 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 | 2627 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2628 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2629 | `			"TypeError",` |
|       - | 2630 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2631 | `			ph7_type_name(apArg[0])` |
|       - | 2632 | `			);` |
|       - | 2633 | `	}` |
|       7 | 2634 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 2635 | `	if( pMap->nEntry < 1 ){` |
|       - | 2636 | `		/* Nothing to pop,return NULL */` |
|       3 | 2637 | `		ph7_result_null(pCtx);` |
|       2 | 2638 | `	}else{` |
|       5 | 2639 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2640 | `		ph7_value *pObj;` |
|       5 | 2641 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       5 | 2642 | `		if( pObj ){` |
|       - | 2643 | `			/* Node value */` |
|       5 | 2644 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2645 | `			/* Unlink the node */` |
|       5 | 2646 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       3 | 2647 | `		}else{` |
|     ! 0 | 2648 | `			ph7_result_null(pCtx);` |
|       - | 2649 | `		}` |
|       - | 2650 | `		/* Reset the cursor */` |
|       5 | 2651 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2652 | `	}` |
|       7 | 2653 | `	return PH7_OK;` |
|      10 | 2654 |  |
|       - | 2655 | `/*` |
|       - | 2656 | ` * int array_push($array,$var,...)` |
|       - | 2657 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2658 | ` * Parameters` |
|       - | 2659 | ` *  array` |
|       - | 2660 | ` *    The input array.` |
|       - | 2661 | ` *  var` |
|       - | 2662 | ` *   On or more value to push.` |
|       - | 2663 | ` * Return` |
|       - | 2664 | ` *  New array count (including old items).` |
|       - | 2665 | ` */` |
|      20 | 2666 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2667 |  |
|       - | 2668 | `	ph7_hashmap *pMap;` |
|       - | 2669 | `	sxi32 rc;` |
|       - | 2670 | `	int i;` |
|      22 | 2671 | `	if( nArg < 1 ){` |
|       4 | 2672 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2673 | `			"ArgumentCountError",` |
|       - | 2674 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2675 | `			nArg` |
|       - | 2676 | `			);` |
|       - | 2677 | `	}` |
|       - | 2678 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2679 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      20 | 2680 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2681 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2682 | `			"Error",` |
|       - | 2683 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2684 | `			);` |
|       - | 2685 | `	}` |
|       - | 2686 | `	/* Make sure we are dealing with a valid hashmap */` |
|      16 | 2687 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2688 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2689 | `			"TypeError",` |
|       - | 2690 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2691 | `			ph7_type_name(apArg[0])` |
|       - | 2692 | `			);` |
|       - | 2693 | `	}` |
|       - | 2694 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 2695 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2696 | `	/* Start pushing given values */` |
|      27 | 2697 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      15 | 2698 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      15 | 2699 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2700 | `			break;` |
|       - | 2701 | `		}` |
|       8 | 2702 | `	}` |
|       - | 2703 | `	/* Return the new count */` |
|      13 | 2704 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      13 | 2705 | `	return PH7_OK;` |
|      12 | 2706 |  |
|       - | 2707 | `/*` |
|       - | 2708 | ` * value array_shift(array $array)` |
|       - | 2709 | ` *   Shift an element off the beginning of array.` |
|       - | 2710 | ` * Parameter` |
|       - | 2711 | ` *  The array to get the value from.` |
|       - | 2712 | ` * Return` |
|       - | 2713 | ` *  Shifted value or NULL on failure.` |
|       - | 2714 | ` */` |
|      36 | 2715 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2716 |  |
|       - | 2717 | `	ph7_hashmap *pMap;` |
|       - | 2718 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      38 | 2719 | `	if( nArg != 1 ){` |
|       7 | 2720 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2721 | `			"ArgumentCountError",` |
|       - | 2722 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2723 | `			nArg` |
|       - | 2724 | `			);` |
|       - | 2725 | `	}` |
|       - | 2726 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      34 | 2727 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2728 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2729 | `			"Error",` |
|       - | 2730 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2731 | `			);` |
|       - | 2732 | `	}` |
|       - | 2733 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 | 2734 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2735 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2736 | `			"TypeError",` |
|       - | 2737 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2738 | `			ph7_type_name(apArg[0])` |
|       - | 2739 | `			);` |
|       - | 2740 | `	}` |
|       - | 2741 | `	/* Point to the internal representation of the hashmap */` |
|      28 | 2742 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      28 | 2743 | `	if( pMap->nEntry < 1 ){` |
|       - | 2744 | `		/* Empty hashmap,return NULL */` |
|       3 | 2745 | `		ph7_result_null(pCtx);` |
|       2 | 2746 | `	}else{` |
|      26 | 2747 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2748 | `		ph7_value *pObj;` |
|       - | 2749 | `		sxu32 n;` |
|      26 | 2750 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      26 | 2751 | `		if( pObj ){` |
|       - | 2752 | `			/* Node value */` |
|      26 | 2753 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2754 | `			/* Unlink the first node */` |
|      26 | 2755 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      14 | 2756 | `		}else{` |
|     ! 0 | 2757 | `			ph7_result_null(pCtx);` |
|       - | 2758 | `		}` |
|       - | 2759 | `		/* Rehash all int keys */` |
|      26 | 2760 | `		n = pMap->nEntry;` |
|      26 | 2761 | `		pEntry = pMap->pFirst;` |
|      26 | 2762 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      37 | 2763 | `		for(;;){` |
|      76 | 2764 | `			if( n < 1 ){` |
|      26 | 2765 | `				break;` |
|       - | 2766 | `			}` |
|      52 | 2767 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      52 | 2768 | `				HashmapRehashIntNode(pEntry);` |
|      25 | 2769 | `			}` |
|       - | 2770 | `			/* Point to the next entry */` |
|      52 | 2771 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      52 | 2772 | `			n--;` |
|       2 | 2773 | `		}` |
|       - | 2774 | `		/* Reset the cursor */` |
|      26 | 2775 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2776 | `	}` |
|      28 | 2777 | `	return PH7_OK;` |
|      20 | 2778 |  |
|       - | 2779 | `/*` |
|       - | 2780 | ` * Extract the node cursor value.` |
|       - | 2781 | ` */` |
|      24 | 2782 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 2783 |  |
|      25 | 2784 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 2785 | `	ph7_value *pVal;` |
|      25 | 2786 | `	if( pCur == 0 ){` |
|       - | 2787 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 2788 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2789 | `		return PH7_OK;` |
|       - | 2790 | `	}` |
|      25 | 2791 | `	if( iDirection != 0 ){` |
|       9 | 2792 | `		if( iDirection > 0 ){` |
|       - | 2793 | `			/* Point to the next entry */` |
|       7 | 2794 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 2795 | `			pCur = pMap->pCur;` |
|       4 | 2796 | `		}else{` |
|       - | 2797 | `			/* Point to the previous entry */` |
|       3 | 2798 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 2799 | `			pCur = pMap->pCur;` |
|       - | 2800 | `		}` |
|       9 | 2801 | `		if( pCur == 0 ){` |
|       - | 2802 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 2803 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2804 | `			return PH7_OK;` |
|       - | 2805 | `		}` |
|       4 | 2806 | `	}` |
|       - | 2807 | `	/* Point to the desired element */` |
|      25 | 2808 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 2809 | `	if( pVal ){` |
|      25 | 2810 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 2811 | `	}else{` |
|     ! 0 | 2812 | `		ph7_result_bool(pCtx,0);` |
|       - | 2813 | `	}` |
|      25 | 2814 | `	return PH7_OK;` |
|      13 | 2815 |  |
|       - | 2816 | `/*` |
|       - | 2817 | ` * value current(array $array)` |
|       - | 2818 | ` *  Return the current element in an array.` |
|       - | 2819 | ` * Parameter` |
|       - | 2820 | ` *  $input: The input array.` |
|       - | 2821 | ` * Return` |
|       - | 2822 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 2823 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2824 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2825 | ` *  is empty, current() returns FALSE.` |
|       - | 2826 | ` */` |
|      10 | 2827 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2828 |  |
|      11 | 2829 | `	if( nArg < 1 ){` |
|       - | 2830 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2831 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2832 | `		return PH7_OK;` |
|       - | 2833 | `	}` |
|       - | 2834 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 2835 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2836 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2837 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2838 | `		return PH7_OK;` |
|       - | 2839 | `	}` |
|      11 | 2840 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 2841 | `	return PH7_OK;` |
|       6 | 2842 |  |
|       - | 2843 | `/*` |
|       - | 2844 | ` * value next(array $input)` |
|       - | 2845 | ` *  Advance the internal array pointer of an array.` |
|       - | 2846 | ` * Parameter` |
|       - | 2847 | ` *  $input: The input array.` |
|       - | 2848 | ` * Return` |
|       - | 2849 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 2850 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 2851 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 2852 | ` */` |
|       6 | 2853 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2854 |  |
|       7 | 2855 | `	if( nArg < 1 ){` |
|       - | 2856 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2857 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2858 | `		return PH7_OK;` |
|       - | 2859 | `	}` |
|       - | 2860 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 2861 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2862 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2863 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2864 | `		return PH7_OK;` |
|       - | 2865 | `	}` |
|       7 | 2866 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 2867 | `	return PH7_OK;` |
|       4 | 2868 |  |
|       - | 2869 | `/*` |
|       - | 2870 | ` * value prev(array $input)` |
|       - | 2871 | ` *  Rewind the internal array pointer.` |
|       - | 2872 | ` * Parameter` |
|       - | 2873 | ` *  $input: The input array.` |
|       - | 2874 | ` * Return` |
|       - | 2875 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 2876 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 2877 | ` *  elements.` |
|       - | 2878 | ` */` |
|       2 | 2879 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2880 |  |
|       3 | 2881 | `	if( nArg < 1 ){` |
|       - | 2882 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2883 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2884 | `		return PH7_OK;` |
|       - | 2885 | `	}` |
|       - | 2886 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2887 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2888 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2889 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2890 | `		return PH7_OK;` |
|       - | 2891 | `	}` |
|       3 | 2892 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 2893 | `	return PH7_OK;` |
|       2 | 2894 |  |
|       - | 2895 | `/*` |
|       - | 2896 | ` * value end(array $input)` |
|       - | 2897 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 2898 | ` * Parameter` |
|       - | 2899 | ` *  $input: The input array.` |
|       - | 2900 | ` * Return` |
|       - | 2901 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 2902 | ` */` |
|       2 | 2903 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2904 |  |
|       - | 2905 | `	ph7_hashmap *pMap;` |
|       3 | 2906 | `	if( nArg < 1 ){` |
|       - | 2907 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2908 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2909 | `		return PH7_OK;` |
|       - | 2910 | `	}` |
|       - | 2911 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2912 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2913 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2914 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2915 | `		return PH7_OK;` |
|       - | 2916 | `	}` |
|       - | 2917 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2918 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2919 | `	/* Point to the last node */` |
|       3 | 2920 | `	pMap->pCur = pMap->pLast;` |
|       - | 2921 | `	/* Return the last node value */` |
|       3 | 2922 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 2923 | `	return PH7_OK;` |
|       2 | 2924 |  |
|       - | 2925 | `/*` |
|       - | 2926 | ` * value reset(array $array )` |
|       - | 2927 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 2928 | ` * Parameter` |
|       - | 2929 | ` *  $input: The input array.` |
|       - | 2930 | ` * Return` |
|       - | 2931 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 2932 | ` */` |
|       4 | 2933 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2934 |  |
|       - | 2935 | `	ph7_hashmap *pMap;` |
|       5 | 2936 | `	if( nArg < 1 ){` |
|       - | 2937 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2938 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2939 | `		return PH7_OK;` |
|       - | 2940 | `	}` |
|       - | 2941 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2942 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2943 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2944 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2945 | `		return PH7_OK;` |
|       - | 2946 | `	}` |
|       - | 2947 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2948 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2949 | `	/* Point to the first node */` |
|       5 | 2950 | `	pMap->pCur = pMap->pFirst;` |
|       - | 2951 | `	/* Return the last node value if available */` |
|       5 | 2952 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 2953 | `	return PH7_OK;` |
|       3 | 2954 |  |
|       - | 2955 | `/*` |
|       - | 2956 | ` * value key(array $array)` |
|       - | 2957 | ` *   Fetch a key from an array` |
|       - | 2958 | ` * Parameter` |
|       - | 2959 | ` *  $input` |
|       - | 2960 | ` *   The input array.` |
|       - | 2961 | ` * Return` |
|       - | 2962 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 2963 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2964 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2965 | ` *  is empty, key() returns NULL.` |
|       - | 2966 | ` */` |
|       4 | 2967 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2968 |  |
|       - | 2969 | `	ph7_hashmap_node *pCur;` |
|       - | 2970 | `	ph7_hashmap *pMap;` |
|       5 | 2971 | `	if( nArg < 1 ){` |
|       - | 2972 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 2973 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2974 | `		return PH7_OK;` |
|       - | 2975 | `	}` |
|       - | 2976 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2977 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2978 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 2979 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2980 | `		return PH7_OK;` |
|       - | 2981 | `	}` |
|       5 | 2982 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2983 | `	pCur = pMap->pCur;` |
|       5 | 2984 | `	if( pCur == 0 ){` |
|       - | 2985 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 2986 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2987 | `		return PH7_OK;` |
|       - | 2988 | `	}` |
|       5 | 2989 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 2990 | `		/* Key is integer */` |
|     ! 0 | 2991 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 2992 | `	}else{` |
|       - | 2993 | `		/* Key is blob */` |
|       7 | 2994 | `		ph7_result_string(pCtx,` |
|       4 | 2995 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 2996 | `	}` |
|       5 | 2997 | `	return PH7_OK;` |
|       3 | 2998 |  |
|       - | 2999 | `/*` |
|       - | 3000 | ` * array each(array $input)` |
|       - | 3001 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3002 | ` * Parameter` |
|       - | 3003 | ` *  $input` |
|       - | 3004 | ` *    The input array.` |
|       - | 3005 | ` * Return` |
|       - | 3006 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3007 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3008 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3009 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3010 | ` *  each() returns FALSE.` |
|       - | 3011 | ` */` |
|      22 | 3012 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3013 |  |
|       - | 3014 | `	ph7_hashmap_node *pCur;` |
|       - | 3015 | `	ph7_hashmap *pMap;` |
|       - | 3016 | `	ph7_value *pArray;` |
|       - | 3017 | `	ph7_value *pVal;` |
|       - | 3018 | `	ph7_value sKey;` |
|      23 | 3019 | `	if( nArg < 1 ){` |
|       - | 3020 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3021 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3022 | `		return PH7_OK;` |
|       - | 3023 | `	}` |
|       - | 3024 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3025 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3026 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3027 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3028 | `		return PH7_OK;` |
|       - | 3029 | `	}` |
|       - | 3030 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3031 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3032 | `	if( pMap->pCur == 0 ){` |
|       - | 3033 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3034 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3035 | `		return PH7_OK;` |
|       - | 3036 | `	}` |
|      15 | 3037 | `	pCur = pMap->pCur;` |
|       - | 3038 | `	/* Create a new array */` |
|      15 | 3039 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3040 | `	if( pArray == 0 ){` |
|     ! 0 | 3041 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3042 | `		return PH7_OK;` |
|       - | 3043 | `	}` |
|      15 | 3044 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3045 | `	/* Insert the current value */` |
|      15 | 3046 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3047 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3048 | `	/* Make the key */` |
|      15 | 3049 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3050 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3051 | `	}else{` |
|       9 | 3052 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3053 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3054 | `	}` |
|       - | 3055 | `	/* Insert the current key */` |
|      15 | 3056 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3057 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3058 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3059 | `	/* Advance the cursor */` |
|      15 | 3060 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3061 | `	/* Return the current entry */` |
|      15 | 3062 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3063 | `	return PH7_OK;` |
|      12 | 3064 |  |
|       - | 3065 | `/*` |
|       - | 3066 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3067 | ` *  Create an array containing a range of elements` |
|       - | 3068 | ` * Parameter` |
|       - | 3069 | ` *  start` |
|       - | 3070 | ` *   First value of the sequence.` |
|       - | 3071 | ` *  limit` |
|       - | 3072 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3073 | ` *  step` |
|       - | 3074 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3075 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3076 | ` * Return` |
|       - | 3077 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3078 | ` * NOTE:` |
|       - | 3079 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3080 | ` */` |
|       2 | 3081 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3082 |  |
|       - | 3083 | `	ph7_value *pValue,*pArray;` |
|       - | 3084 | `	sxi64 iOfft,iLimit;` |
|       3 | 3085 | `	int iStep = 1;` |
|       - | 3086 |  |
|       3 | 3087 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3088 | `	if( nArg > 0 ){` |
|       - | 3089 | `		/* Extract the offset */` |
|       3 | 3090 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3091 | `		if( nArg > 1 ){` |
|       - | 3092 | `			/* Extract the limit */` |
|       3 | 3093 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3094 | `			if( nArg > 2 ){` |
|       - | 3095 | `				/* Extract the increment */` |
|       3 | 3096 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3097 | `				if( iStep < 1 ){` |
|       - | 3098 | `					/* Only positive number are allowed */` |
|       3 | 3099 | `					iStep = 1;` |
|       1 | 3100 | `				}` |
|       1 | 3101 | `			}` |
|       1 | 3102 | `		}` |
|       1 | 3103 | `	}` |
|       - | 3104 | `	/* Element container */` |
|       3 | 3105 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3106 | `	/* Create the new array */` |
|       3 | 3107 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3108 | `	if( pArray == 0 ){` |
|     ! 0 | 3109 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3110 | `		return PH7_OK;` |
|       - | 3111 | `	}` |
|       - | 3112 | `	/* Start filling */` |
|       3 | 3113 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3114 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3115 | `		/* Perform the insertion */` |
|     ! 0 | 3116 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3117 | `		/* Increment */` |
|     ! 0 | 3118 | `		iOfft += iStep;` |
|     ! 0 | 3119 | `	}` |
|       - | 3120 | `	/* Return the new array */` |
|       3 | 3121 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3122 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3123 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3124 | `	 */` |
|       3 | 3125 | `	return PH7_OK;` |
|       2 | 3126 |  |
|       - | 3127 | `/*` |
|       - | 3128 | ` * array array_values(array $array)` |
|       - | 3129 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3130 | ` * Parameters` |
|       - | 3131 | ` *  $array` |
|       - | 3132 | ` *   The input array.` |
|       - | 3133 | ` * Return` |
|       - | 3134 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3135 | ` */` |
|      30 | 3136 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3137 |  |
|       - | 3138 | `	ph7_hashmap_node *pNode;` |
|       - | 3139 | `	ph7_hashmap *pMap;` |
|       - | 3140 | `	ph7_value *pArray;` |
|       - | 3141 | `	ph7_value *pObj;` |
|       - | 3142 | `	sxu32 n;` |
|      32 | 3143 | `	if( nArg != 1 ){` |
|       - | 3144 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3145 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3146 | `			"ArgumentCountError",` |
|       - | 3147 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3148 | `			nArg` |
|       - | 3149 | `			);` |
|       - | 3150 | `	}` |
|       - | 3151 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3152 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3153 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3154 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3155 | `			"TypeError",` |
|       - | 3156 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3157 | `			ph7_type_name(apArg[0])` |
|       - | 3158 | `			);` |
|       - | 3159 | `	}` |
|       - | 3160 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3161 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3162 | `	/* Create a new array */` |
|      25 | 3163 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3164 | `	if( pArray == 0 ){` |
|     ! 0 | 3165 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3166 | `		return PH7_OK;` |
|       - | 3167 | `	}` |
|       - | 3168 | `	/* Perform the requested operation */` |
|      25 | 3169 | `	pNode = pMap->pFirst;` |
|      83 | 3170 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3171 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3172 | `		if( pObj ){` |
|       - | 3173 | `			/* perform the insertion */` |
|      59 | 3174 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3175 | `		}` |
|       - | 3176 | `		/* Point to the next entry */` |
|      59 | 3177 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3178 | `	}` |
|       - | 3179 | `	/* return the new array */` |
|      25 | 3180 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3181 | `	return PH7_OK;` |
|      17 | 3182 |  |
|       - | 3183 | `/*` |
|       - | 3184 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3185 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3186 | ` * Parameters` |
|       - | 3187 | ` *  $input` |
|       - | 3188 | ` *   An array containing keys to return.` |
|       - | 3189 | ` * $search_value` |
|       - | 3190 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3191 | ` * $strict` |
|       - | 3192 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3193 | ` * Return` |
|       - | 3194 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3195 | ` */` |
|     120 | 3196 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3197 |  |
|       - | 3198 | `	ph7_hashmap_node *pNode;` |
|       - | 3199 | `	ph7_hashmap *pMap;` |
|       - | 3200 | `	ph7_value *pArray;` |
|       - | 3201 | `	ph7_value sObj;` |
|       - | 3202 | `	ph7_value sVal;` |
|       - | 3203 | `	SyString sKey;` |
|       - | 3204 | `	int bStrict;` |
|       - | 3205 | `	sxi32 rc;` |
|       - | 3206 | `	sxu32 n;` |
|     122 | 3207 | `	if( nArg < 1 ){` |
|       - | 3208 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3209 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3210 | `			"ArgumentCountError",` |
|       - | 3211 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3212 | `			);` |
|       - | 3213 | `	}` |
|       - | 3214 | `	/* Make sure we are dealing with a valid hashmap */` |
|     120 | 3215 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3216 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3217 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3218 | `			"TypeError",` |
|       - | 3219 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3220 | `			ph7_type_name(apArg[0])` |
|       - | 3221 | `			);` |
|       - | 3222 | `	}` |
|       - | 3223 | `	/* Point to the internal representation of the input hashmap */` |
|     118 | 3224 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3225 | `	/* Create a new array */` |
|     118 | 3226 | `	pArray = ph7_context_new_array(pCtx);` |
|     118 | 3227 | `	if( pArray == 0 ){` |
|     ! 0 | 3228 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3229 | `		return PH7_OK;` |
|       - | 3230 | `	}` |
|     118 | 3231 | `	bStrict = FALSE;` |
|     118 | 3232 | `	if( nArg > 2 ){` |
|       - | 3233 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3234 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3235 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3236 | `				"TypeError",` |
|       - | 3237 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3238 | `				ph7_type_name(apArg[2])` |
|       - | 3239 | `				);` |
|       - | 3240 | `		}` |
|       5 | 3241 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3242 | `	}` |
|       - | 3243 | `	/* Perform the requested operation */` |
|     115 | 3244 | `	pNode = pMap->pFirst;` |
|     115 | 3245 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     553 | 3246 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     439 | 3247 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     117 | 3248 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      59 | 3249 | `		}else{` |
|     323 | 3250 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3251 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3252 | `		}` |
|     439 | 3253 | `		rc = 0;` |
|     439 | 3254 | `		if( nArg > 1 ){` |
|      31 | 3255 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3256 | `			if( pValue ){` |
|      31 | 3257 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3258 | `				/* Filter key */` |
|      31 | 3259 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3260 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3261 | `			}` |
|      15 | 3262 | `		}` |
|     439 | 3263 | `		if( rc == 0 ){` |
|       - | 3264 | `			/* Perform the insertion */` |
|     421 | 3265 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     210 | 3266 | `		}` |
|     439 | 3267 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3268 | `		/* Point to the next entry */` |
|     439 | 3269 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     220 | 3270 | `	}` |
|       - | 3271 | `	/* return the new array */` |
|     115 | 3272 | `	ph7_result_value(pCtx,pArray);` |
|     115 | 3273 | `	return PH7_OK;` |
|      62 | 3274 |  |
|       - | 3275 | `/*` |
|       - | 3276 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3277 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3278 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3279 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3280 | ` * Parameters` |
|       - | 3281 | ` *  $arr1` |
|       - | 3282 | ` *   First array` |
|       - | 3283 | ` *  $arr2` |
|       - | 3284 | ` *   Second array` |
|       - | 3285 | ` * Return` |
|       - | 3286 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3287 | ` * Note` |
|       - | 3288 | ` *  This function is a symisc eXtension.` |
|       - | 3289 | ` */` |
|       4 | 3290 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3291 |  |
|       - | 3292 | `	ph7_hashmap *p1,*p2;` |
|       - | 3293 | `	int rc;` |
|       5 | 3294 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3295 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3296 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3297 | `		return PH7_OK;` |
|       - | 3298 | `	}` |
|       - | 3299 | `	/* Point to the hashmaps */` |
|       5 | 3300 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3301 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3302 | `	rc = (p1 == p2);` |
|       - | 3303 | `	/* Same instance? */` |
|       5 | 3304 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3305 | `	return PH7_OK;` |
|       3 | 3306 |  |
|       - | 3307 | `/*` |
|       - | 3308 | ` * array array_merge(array ...$arrays)` |
|       - | 3309 | ` *  Merge one or more arrays.` |
|       - | 3310 | ` * Parameters` |
|       - | 3311 | ` *  ...$arrays` |
|       - | 3312 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3313 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3314 | ` * Return` |
|       - | 3315 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3316 | ` *  with no arguments.` |
|       - | 3317 | ` */` |
|     876 | 3318 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3319 |  |
|       - | 3320 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3321 | `	ph7_value *pArray;` |
|       - | 3322 | `	int i;` |
|       - | 3323 | `	/* Create a new array */` |
|     878 | 3324 | `	pArray = ph7_context_new_array(pCtx);` |
|     878 | 3325 | `	if( pArray == 0 ){` |
|     ! 0 | 3326 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3327 | `		return PH7_OK;` |
|       - | 3328 | `	}` |
|       - | 3329 | `	/* Point to the internal representation of the hashmap */` |
|     878 | 3330 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3331 | `	/* Start merging */` |
|    2620 | 3332 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3333 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1748 | 3334 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3335 | `			/* Type mismatch -> TypeError */` |
|       7 | 3336 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3337 | `				"TypeError",` |
|       - | 3338 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3339 | `				i + 1,` |
|       4 | 3340 | `				ph7_type_name(apArg[i])` |
|       - | 3341 | `				);` |
|     ! 0 | 3342 | `		}else{` |
|    1744 | 3343 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3344 | `			/* Merge the two hashmaps */` |
|    1744 | 3345 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3346 | `		}` |
|     873 | 3347 | `	}` |
|       - | 3348 | `	/* Return the freshly created array */` |
|     874 | 3349 | `	ph7_result_value(pCtx,pArray);` |
|     874 | 3350 | `	return PH7_OK;` |
|     440 | 3351 |  |
|       - | 3352 | `/*` |
|       - | 3353 | ` * array array_copy(array $source)` |
|       - | 3354 | ` *  Make a blind copy of the target array.` |
|       - | 3355 | ` * Parameters` |
|       - | 3356 | ` *  $source` |
|       - | 3357 | ` *   Target array` |
|       - | 3358 | ` * Return` |
|       - | 3359 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3360 | ` * Note` |
|       - | 3361 | ` *  This function is a symisc eXtension.` |
|       - | 3362 | ` */` |
|      16 | 3363 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3364 |  |
|       - | 3365 | `	ph7_hashmap *pMap;` |
|       - | 3366 | `	ph7_value *pArray;` |
|      17 | 3367 | `	if( nArg < 1 ){` |
|       - | 3368 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3369 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3370 | `		return PH7_OK;` |
|       - | 3371 | `	}` |
|       - | 3372 | `	/* Create a new array */` |
|      17 | 3373 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3374 | `	if( pArray == 0 ){` |
|     ! 0 | 3375 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3376 | `		return PH7_OK;` |
|       - | 3377 | `	}` |
|       - | 3378 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3379 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3380 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3381 | `		/* Point to the internal representation of the source */` |
|      17 | 3382 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3383 | `		/* Perform the copy */` |
|      17 | 3384 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3385 | `	}else{` |
|       - | 3386 | `		/* Simple insertion */` |
|     ! 0 | 3387 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3388 | `	}` |
|       - | 3389 | `	/* Return the duplicated array */` |
|      17 | 3390 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3391 | `	return PH7_OK;` |
|       9 | 3392 |  |
|       - | 3393 | `/*` |
|       - | 3394 | ` * bool array_erase(array $source)` |
|       - | 3395 | ` *  Remove all elements from a given array.` |
|       - | 3396 | ` * Parameters` |
|       - | 3397 | ` *  $source` |
|       - | 3398 | ` *   Target array` |
|       - | 3399 | ` * Return` |
|       - | 3400 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3401 | ` * Note` |
|       - | 3402 | ` *  This function is a symisc eXtension.` |
|       - | 3403 | ` */` |
|      16 | 3404 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3405 |  |
|       - | 3406 | `	ph7_hashmap *pMap;` |
|      17 | 3407 | `	if( nArg < 1 ){` |
|       - | 3408 | `		/* Missing arguments */` |
|     ! 0 | 3409 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3410 | `		return PH7_OK;` |
|       - | 3411 | `	}` |
|       - | 3412 | `	/* Point to the target hashmap */` |
|      17 | 3413 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3414 | `	/* Erase */` |
|      17 | 3415 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3416 | `	return PH7_OK;` |
|       9 | 3417 |  |
|       - | 3418 | `/*` |
|       - | 3419 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3420 | ` *  Extract a slice of the array.` |
|       - | 3421 | ` * Parameters` |
|       - | 3422 | ` *  $array` |
|       - | 3423 | ` *    The input array.` |
|       - | 3424 | ` * $offset` |
|       - | 3425 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3426 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3427 | ` * $length (optional, nullable)` |
|       - | 3428 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3429 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3430 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3431 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3432 | ` * $preserve_keys (optional)` |
|       - | 3433 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3434 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3435 | ` * Return` |
|       - | 3436 | ` *   The new slice.` |
|       - | 3437 | ` */` |
|      46 | 3438 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3439 |  |
|       - | 3440 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3441 | `	ph7_hashmap_node *pCur;` |
|       - | 3442 | `	ph7_value *pArray;` |
|       - | 3443 | `	int iLength,iOfft;` |
|       - | 3444 | `	int bPreserve;` |
|       - | 3445 | `	sxi32 rc;` |
|      48 | 3446 | `	if( nArg < 2 ){` |
|       7 | 3447 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3448 | `			"ArgumentCountError",` |
|       - | 3449 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3450 | `			nArg` |
|       - | 3451 | `			);` |
|       - | 3452 | `	}` |
|      44 | 3453 | `	if( nArg > 4 ){` |
|       4 | 3454 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3455 | `			"ArgumentCountError",` |
|       - | 3456 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3457 | `			nArg` |
|       - | 3458 | `			);` |
|       - | 3459 | `	}` |
|      42 | 3460 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3461 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3462 | `			"TypeError",` |
|       - | 3463 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3464 | `			ph7_type_name(apArg[0])` |
|       - | 3465 | `			);` |
|       - | 3466 | `	}` |
|       - | 3467 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3468 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3469 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3470 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3471 | `			"TypeError",` |
|       - | 3472 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3473 | `			ph7_type_name(apArg[1])` |
|       - | 3474 | `			);` |
|       - | 3475 | `	}` |
|       - | 3476 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3477 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3478 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3479 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3480 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3481 | `				"TypeError",` |
|       - | 3482 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3483 | `				ph7_type_name(apArg[2])` |
|       - | 3484 | `				);` |
|       - | 3485 | `		}` |
|       8 | 3486 | `	}` |
|       - | 3487 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3488 | `	if( nArg > 3 ){` |
|      10 | 3489 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3490 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3491 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3492 | `				"TypeError",` |
|       - | 3493 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3494 | `				ph7_type_name(apArg[3])` |
|       - | 3495 | `				);` |
|       - | 3496 | `		}` |
|       2 | 3497 | `	}` |
|       - | 3498 | `	/* Point the internal representation of the target array */` |
|      33 | 3499 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3500 | `	bPreserve = FALSE;` |
|       - | 3501 | `	/* Get the offset */` |
|      33 | 3502 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3503 | `	if( iOfft < 0 ){` |
|       5 | 3504 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3505 | `		if( iOfft < 0 ){` |
|       3 | 3506 | `			iOfft = 0;` |
|       1 | 3507 | `		}` |
|       2 | 3508 | `	}` |
|      33 | 3509 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3510 | `		/* Offset past end of array, return empty array */` |
|       5 | 3511 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3512 | `		if( pArray == 0 ){` |
|     ! 0 | 3513 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3514 | `			return PH7_OK;` |
|       - | 3515 | `		}` |
|       5 | 3516 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3517 | `		return PH7_OK;` |
|       - | 3518 | `	}` |
|       - | 3519 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3520 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3521 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3522 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3523 | `		if( iLength < 0 ){` |
|       5 | 3524 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3525 | `		}` |
|      15 | 3526 | `		if( iLength < 0 ){` |
|       3 | 3527 | `			iLength = 0;` |
|       1 | 3528 | `		}` |
|      15 | 3529 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3530 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3531 | `		}` |
|       7 | 3532 | `	}` |
|      29 | 3533 | `	if( nArg > 3 ){` |
|       5 | 3534 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3535 | `	}` |
|       - | 3536 | `	/* Create a new array */` |
|      29 | 3537 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3538 | `	if( pArray == 0 ){` |
|     ! 0 | 3539 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3540 | `		return PH7_OK;` |
|       - | 3541 | `	}` |
|      29 | 3542 | `	if( iLength < 1 ){` |
|       - | 3543 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3544 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3545 | `		return PH7_OK;` |
|       - | 3546 | `	}` |
|       - | 3547 | `	/* Point to the desired entry */` |
|      25 | 3548 | `	pCur = pSrc->pFirst;` |
|      24 | 3549 | `	for(;;){` |
|      49 | 3550 | `		if( iOfft < 1 ){` |
|      25 | 3551 | `			break;` |
|       - | 3552 | `		}` |
|       - | 3553 | `		/* Point to the next entry */` |
|      25 | 3554 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3555 | `		iOfft--;` |
|       1 | 3556 | `	}` |
|       - | 3557 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3558 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3559 | `	for(;;){` |
|      79 | 3560 | `		if( iLength < 1 ){` |
|      25 | 3561 | `			break;` |
|       - | 3562 | `		}` |
|       - | 3563 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3564 | `		{` |
|      55 | 3565 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3566 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3567 | `		}` |
|      55 | 3568 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3569 | `			break;` |
|       - | 3570 | `		}` |
|       - | 3571 | `		/* Point to the next entry */` |
|      55 | 3572 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3573 | `		iLength--;` |
|       1 | 3574 | `	}` |
|       - | 3575 | `	/* Return the freshly created array */` |
|      25 | 3576 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3577 | `	return PH7_OK;` |
|      25 | 3578 |  |
|       - | 3579 | `/*` |
|       - | 3580 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3581 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3582 | ` * beginning (becomes the new pFirst).` |
|       - | 3583 | ` */` |
|      30 | 3584 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3585 |  |
|       - | 3586 | `	ph7_hashmap_node *pNode;` |
|       - | 3587 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3588 | `	pNode = pMap->pLast;` |
|      31 | 3589 | `	if( pNode == 0 ){` |
|     ! 0 | 3590 | `		return;` |
|       - | 3591 | `	}` |
|      31 | 3592 | `	if( pNode->pNext == 0 ){` |
|       - | 3593 | `		/* Only node in the list, nothing to move */` |
|       5 | 3594 | `		return;` |
|       - | 3595 | `	}` |
|      27 | 3596 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3597 | `		/* Already in the correct position */` |
|       9 | 3598 | `		return;` |
|       - | 3599 | `	}` |
|       - | 3600 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3601 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3602 | `	pMap->pLast->pPrev = 0;` |
|       - | 3603 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3604 | `	if( pAfter == 0 ){` |
|       - | 3605 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3606 | `		pNode->pNext = 0;` |
|       3 | 3607 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3608 | `		if( pMap->pFirst ){` |
|       3 | 3609 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3610 | `		}` |
|       3 | 3611 | `		pMap->pFirst = pNode;` |
|       2 | 3612 | `	}else{` |
|      17 | 3613 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3614 | `		pNode->pPrev = pOldNext;` |
|      17 | 3615 | `		pNode->pNext = pAfter;` |
|      17 | 3616 | `		pAfter->pPrev = pNode;` |
|      17 | 3617 | `		if( pOldNext ){` |
|      17 | 3618 | `			pOldNext->pNext = pNode;` |
|       9 | 3619 | `		}else{` |
|     ! 0 | 3620 | `			pMap->pLast = pNode;` |
|       - | 3621 | `		}` |
|       - | 3622 | `	}` |
|      16 | 3623 |  |
|       - | 3624 | `/*` |
|       - | 3625 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3626 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3627 | ` * Parameters` |
|       - | 3628 | ` *  $array` |
|       - | 3629 | ` *    The input array.` |
|       - | 3630 | ` *  $offset` |
|       - | 3631 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3632 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3633 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3634 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3635 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3636 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3637 | ` *  $length (optional)` |
|       - | 3638 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3639 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3640 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3641 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3642 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3643 | ` *  $replacement (optional)` |
|       - | 3644 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3645 | ` *    with elements from this array.` |
|       - | 3646 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3647 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3648 | ` *    offset.` |
|       - | 3649 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3650 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3651 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3652 | ` * Return` |
|       - | 3653 | ` *   A new array consisting of the extracted elements.` |
|       - | 3654 | ` */` |
|      54 | 3655 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3656 |  |
|       - | 3657 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3658 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3659 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3660 | `	int iLength,iOfft,i;` |
|       - | 3661 | `	sxi32 rc;` |
|      56 | 3662 | `	if( nArg < 2 ){` |
|       7 | 3663 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3664 | `			"ArgumentCountError",` |
|       - | 3665 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3666 | `			nArg` |
|       - | 3667 | `			);` |
|       - | 3668 | `	}` |
|      52 | 3669 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3670 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3671 | `			"TypeError",` |
|       - | 3672 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3673 | `			ph7_type_name(apArg[0])` |
|       - | 3674 | `			);` |
|       - | 3675 | `	}` |
|       - | 3676 | `	/* Point to the internal representation of the target array */` |
|      49 | 3677 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3678 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3679 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3680 | `	if( iOfft < 0 ){` |
|       7 | 3681 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3682 | `		if( iOfft < 0 ){` |
|       3 | 3683 | `			iOfft = 0;` |
|       2 | 3684 | `		}` |
|      46 | 3685 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3686 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3687 | `	}` |
|       - | 3688 | `	/* Get the length and clamp to valid range.` |
|       - | 3689 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3690 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3691 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3692 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3693 | `		if( iLength < 0 ){` |
|       7 | 3694 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3695 | `			if( iLength < 0 ){` |
|       3 | 3696 | `				iLength = 0;` |
|       1 | 3697 | `			}` |
|       3 | 3698 | `		}` |
|      31 | 3699 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3700 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3701 | `		}` |
|      15 | 3702 | `	}` |
|       - | 3703 | `	/* Create the result array for removed elements */` |
|      49 | 3704 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3705 | `	if( pArray == 0 ){` |
|     ! 0 | 3706 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3707 | `		return PH7_OK;` |
|       - | 3708 | `	}` |
|       - | 3709 | `	/* Get replacement array if provided */` |
|      49 | 3710 | `	pRep = 0;` |
|      49 | 3711 | `	if( nArg > 3 ){` |
|      21 | 3712 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3713 | `			/* Perform an array cast */` |
|       3 | 3714 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3715 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3716 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3717 | `			}` |
|       2 | 3718 | `		}else{` |
|      19 | 3719 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3720 | `		}` |
|      21 | 3721 | `		if( pRep ){` |
|       - | 3722 | `			/* Reset the loop cursor */` |
|      21 | 3723 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3724 | `		}` |
|      10 | 3725 | `	}` |
|       - | 3726 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3727 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3728 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3729 | `		return PH7_OK;` |
|       - | 3730 | `	}` |
|       - | 3731 | `	/* Navigate to the offset position */` |
|      41 | 3732 | `	pCur = pSrc->pFirst;` |
|      85 | 3733 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3734 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3735 | `	}` |
|       - | 3736 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3737 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3738 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3739 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3740 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3741 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3742 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3743 | `		pPrev = pCur->pPrev;` |
|      71 | 3744 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3745 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3746 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3747 | `			break;` |
|       - | 3748 | `		}` |
|      71 | 3749 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3750 | `	}` |
|       - | 3751 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3752 | `	if( pRep ){` |
|       - | 3753 | `		ph7_value sSafeVal;` |
|      61 | 3754 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3755 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3756 | `			if( pRvalue ){` |
|       - | 3757 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3758 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3759 | `				 * since it points into that same pool. */` |
|      31 | 3760 | `				sSafeVal = *pRvalue;` |
|      31 | 3761 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 3762 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 3763 | `					pNewNode = pSrc->pLast;` |
|      31 | 3764 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 3765 | `					pInsertAfter = pNewNode;` |
|      15 | 3766 | `				}` |
|      15 | 3767 | `			}` |
|       1 | 3768 | `		}` |
|      10 | 3769 | `	}` |
|       - | 3770 | `	/* Return the freshly created array */` |
|      41 | 3771 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 3772 | `	return PH7_OK;` |
|      29 | 3773 |  |
|       - | 3774 | `/*` |
|       - | 3775 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3776 | ` *  Checks if a value exists in an array.` |
|       - | 3777 | ` * Parameters` |
|       - | 3778 | ` *  $needle` |
|       - | 3779 | ` *   The searched value.` |
|       - | 3780 | ` *   Note:` |
|       - | 3781 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3782 | ` * $haystack` |
|       - | 3783 | ` *  The target array.` |
|       - | 3784 | ` * $strict` |
|       - | 3785 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3786 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3787 | ` */` |
|   21346 | 3788 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3789 |  |
|       - | 3790 | `	ph7_value *pNeedle;` |
|       - | 3791 | `	int bStrict;` |
|       - | 3792 | `	int rc;` |
|   21348 | 3793 | `	if( nArg < 2 ){` |
|       - | 3794 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3795 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3796 | `		return PH7_OK;` |
|       - | 3797 | `	}` |
|   21348 | 3798 | `	pNeedle = apArg[0];` |
|   21348 | 3799 | `	bStrict = 0;` |
|   21348 | 3800 | `	if( nArg > 2 ){` |
|       5 | 3801 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3802 | `	}` |
|   21348 | 3803 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3804 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3805 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3806 | `		/* Set the comparison result */` |
|     ! 0 | 3807 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3808 | `		return PH7_OK;` |
|       - | 3809 | `	}` |
|       - | 3810 | `	/* Perform the lookup */` |
|   21348 | 3811 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3812 | `	/* Lookup result */` |
|   21348 | 3813 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   21348 | 3814 | `	return PH7_OK;` |
|   10675 | 3815 |  |
|       - | 3816 | `/*` |
|       - | 3817 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3818 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3819 | ` * Parameters` |
|       - | 3820 | ` * $needle` |
|       - | 3821 | ` *   The searched value.` |
|       - | 3822 | ` * $haystack` |
|       - | 3823 | ` *   The array.` |
|       - | 3824 | ` * $strict` |
|       - | 3825 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3826 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3827 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3828 | ` * Return` |
|       - | 3829 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3830 | ` */` |
|      28 | 3831 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3832 |  |
|       - | 3833 | `	ph7_hashmap_node *pEntry;` |
|       - | 3834 | `	ph7_value *pVal,sNeedle;` |
|       - | 3835 | `	ph7_hashmap *pMap;` |
|       - | 3836 | `	ph7_value sVal;` |
|       - | 3837 | `	int bStrict;` |
|       - | 3838 | `	sxu32 n;` |
|       - | 3839 | `	int rc;` |
|      30 | 3840 | `	if( nArg < 2 ){` |
|       - | 3841 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3842 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3843 | `			"ArgumentCountError",` |
|       - | 3844 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 3845 | `			nArg` |
|       - | 3846 | `			);` |
|       - | 3847 | `	}` |
|      26 | 3848 | `	bStrict = FALSE;` |
|      26 | 3849 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3850 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3851 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3852 | `			"TypeError",` |
|       - | 3853 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 3854 | `			ph7_type_name(apArg[1])` |
|       - | 3855 | `			);` |
|       - | 3856 | `	}` |
|      24 | 3857 | `	if( nArg > 2 ){` |
|       - | 3858 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 3859 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3860 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3861 | `				"TypeError",` |
|       - | 3862 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3863 | `				ph7_type_name(apArg[2])` |
|       - | 3864 | `				);` |
|       - | 3865 | `		}` |
|       9 | 3866 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 3867 | `	}` |
|       - | 3868 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 3869 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3870 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 3871 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 3872 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 3873 | `	pEntry = pMap->pFirst;` |
|      21 | 3874 | `	n = pMap->nEntry;` |
|      23 | 3875 | `	for(;;){` |
|      47 | 3876 | `		if( !n ){` |
|       9 | 3877 | `			break;` |
|       - | 3878 | `		}` |
|       - | 3879 | `		/* Extract node value */` |
|      39 | 3880 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 3881 | `		if( pVal ){` |
|       - | 3882 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3883 | `			 * can change their type.` |
|       - | 3884 | `			 */` |
|      39 | 3885 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 3886 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 3887 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 3888 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 3889 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 3890 | `			if( rc == 0 ){` |
|       - | 3891 | `				/* Match found,return key */` |
|      13 | 3892 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3893 | `					/* INT key */` |
|       7 | 3894 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 3895 | `				}else{` |
|       7 | 3896 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3897 | `					/* Blob key */` |
|       7 | 3898 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3899 | `				}` |
|      13 | 3900 | `				return PH7_OK;` |
|       - | 3901 | `			}` |
|      13 | 3902 | `		}` |
|       - | 3903 | `		/* Point to the next entry */` |
|      27 | 3904 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 3905 | `		n--;` |
|       1 | 3906 | `	}` |
|       - | 3907 | `	/* No such value,return FALSE */` |
|       9 | 3908 | `	ph7_result_bool(pCtx,0);` |
|       9 | 3909 | `	return PH7_OK;` |
|      16 | 3910 |  |
|       - | 3911 | `/*` |
|       - | 3912 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3913 | ` *  Computes the difference of arrays.` |
|       - | 3914 | ` * Parameters` |
|       - | 3915 | ` *  $array1` |
|       - | 3916 | ` *    The array to compare from` |
|       - | 3917 | ` *  $array2` |
|       - | 3918 | ` *    An array to compare against` |
|       - | 3919 | ` *  $...` |
|       - | 3920 | ` *   More arrays to compare against` |
|       - | 3921 | ` * Return` |
|       - | 3922 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3923 | ` *  are not present in any of the other arrays.` |
|       - | 3924 | ` */` |
|      22 | 3925 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3926 |  |
|       - | 3927 | `	ph7_hashmap_node *pEntry;` |
|       - | 3928 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3929 | `	ph7_value *pArray;` |
|       - | 3930 | `	ph7_value *pVal;` |
|       - | 3931 | `	sxi32 rc;` |
|       - | 3932 | `	sxu32 n;` |
|       - | 3933 | `	int i;` |
|       - | 3934 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3935 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3936 | `	 * debugging difficult. */` |
|      24 | 3937 | `	if( nArg < 1 ){` |
|       4 | 3938 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3939 | `			"ArgumentCountError",` |
|       - | 3940 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3941 | `			nArg` |
|       - | 3942 | `			);` |
|       - | 3943 | `	}` |
|      22 | 3944 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3945 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3946 | `			"TypeError",` |
|       - | 3947 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3948 | `			ph7_type_name(apArg[0])` |
|       - | 3949 | `			);` |
|       - | 3950 | `	}` |
|      36 | 3951 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3952 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3953 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3954 | `				"TypeError",` |
|       - | 3955 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3956 | `				i + 1,` |
|       2 | 3957 | `				ph7_type_name(apArg[i])` |
|       - | 3958 | `				);` |
|       - | 3959 | `		}` |
|       9 | 3960 | `	}` |
|      17 | 3961 | `	if( nArg == 1 ){` |
|       - | 3962 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3963 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3964 | `		return PH7_OK;` |
|       - | 3965 | `	}` |
|       - | 3966 | `	/* Create a new array */` |
|      15 | 3967 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3968 | `	if( pArray == 0 ){` |
|     ! 0 | 3969 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3970 | `		return PH7_OK;` |
|       - | 3971 | `	}` |
|       - | 3972 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 3973 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3974 | `	/* Perform the diff */` |
|      15 | 3975 | `	pEntry = pSrc->pFirst;` |
|      15 | 3976 | `	n = pSrc->nEntry;` |
|      27 | 3977 | `	for(;;){` |
|      55 | 3978 | `		if( n < 1 ){` |
|      15 | 3979 | `			break;` |
|       - | 3980 | `		}` |
|       - | 3981 | `		/* Extract the node value */` |
|      41 | 3982 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 3983 | `		if( pVal ){` |
|      69 | 3984 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3985 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 3986 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3987 | `				/* Perform the lookup */` |
|      45 | 3988 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 3989 | `				if( rc == SXRET_OK ){` |
|       - | 3990 | `					/* Value exist */` |
|      17 | 3991 | `					break;` |
|       - | 3992 | `				}` |
|      15 | 3993 | `			}` |
|      41 | 3994 | `			if( i >= nArg ){` |
|       - | 3995 | `				/* Perform the insertion */` |
|      25 | 3996 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 3997 | `			}` |
|      20 | 3998 | `		}` |
|       - | 3999 | `		/* Point to the next entry */` |
|      41 | 4000 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4001 | `		n--;` |
|       1 | 4002 | `	}` |
|       - | 4003 | `	/* Return the freshly created array */` |
|      15 | 4004 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4005 | `	return PH7_OK;` |
|      13 | 4006 |  |
|       - | 4007 | `/*` |
|       - | 4008 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4009 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4010 | ` * Parameters` |
|       - | 4011 | ` *  $array1` |
|       - | 4012 | ` *    The array to compare from` |
|       - | 4013 | ` *  $array2` |
|       - | 4014 | ` *    An array to compare against` |
|       - | 4015 | ` *  $...` |
|       - | 4016 | ` *   More arrays to compare against.` |
|       - | 4017 | ` * $callback` |
|       - | 4018 | ` *  The callback comparison function.` |
|       - | 4019 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4020 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4021 | ` *  than the second.` |
|       - | 4022 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4023 | ` * Return` |
|       - | 4024 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4025 | ` *  are not present in any of the other arrays.` |
|       - | 4026 | ` */` |
|      20 | 4027 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4028 |  |
|       - | 4029 | `	ph7_hashmap_node *pEntry;` |
|       - | 4030 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4031 | `	ph7_value *pCallback;` |
|       - | 4032 | `	ph7_value *pArray;` |
|       - | 4033 | `	ph7_value *pVal;` |
|       - | 4034 | `	sxi32 rc;` |
|       - | 4035 | `	sxu32 n;` |
|       - | 4036 | `	int i;` |
|       - | 4037 |  |
|       - | 4038 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      22 | 4039 | `	if( nArg < 2 ){` |
|       4 | 4040 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4041 | `			"ArgumentCountError",` |
|       - | 4042 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4043 | `			nArg` |
|       - | 4044 | `			);` |
|       - | 4045 | `	}` |
|      20 | 4046 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4047 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4048 | `			"TypeError",` |
|       - | 4049 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4050 | `			ph7_type_name(apArg[0])` |
|       - | 4051 | `			);` |
|       - | 4052 | `	}` |
|       - | 4053 |  |
|      18 | 4054 | `	if( nArg == 2 ){` |
|       - | 4055 | `		/* Only the original array and the callback were provided. */` |
|       - | 4056 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4057 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4058 | `		 * validation order.` |
|       - | 4059 | `		 */` |
|       4 | 4060 | `	} else {` |
|       - | 4061 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      20 | 4062 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      14 | 4063 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      10 | 4064 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4065 | `					"TypeError",` |
|       - | 4066 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4067 | `					i + 1,` |
|       6 | 4068 | `					ph7_type_name(apArg[i])` |
|       - | 4069 | `					);` |
|       - | 4070 | `			}` |
|       5 | 4071 | `		}` |
|       - | 4072 | `	}` |
|       - | 4073 |  |
|       - | 4074 | `	/* Identify the callback (always expected as the last argument). */` |
|      12 | 4075 | `	pCallback = apArg[nArg - 1];` |
|       - | 4076 | `	/* Validate the callback to match PHP's error messages. */` |
|      12 | 4077 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       7 | 4078 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4079 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4080 | `				"TypeError",` |
|       - | 4081 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4082 | `				nArg` |
|       - | 4083 | `				);` |
|       - | 4084 | `		}` |
|       5 | 4085 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4086 | `			int len;` |
|       3 | 4087 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4088 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4089 | `				"TypeError",` |
|       - | 4090 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4091 | `				nArg,` |
|       1 | 4092 | `				zName` |
|       - | 4093 | `				);` |
|       - | 4094 | `		}` |
|       4 | 4095 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4096 | `			"TypeError",` |
|       - | 4097 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4098 | `			nArg` |
|       - | 4099 | `			);` |
|       - | 4100 | `	}` |
|       - | 4101 |  |
|       5 | 4102 | `	if( nArg == 2 ){` |
|       - | 4103 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4104 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4105 | `		return PH7_OK;` |
|       - | 4106 | `	}` |
|       - | 4107 |  |
|       - | 4108 | `	/* Create a new array */` |
|       3 | 4109 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4110 | `	if( pArray == 0 ){` |
|     ! 0 | 4111 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4112 | `		return PH7_OK;` |
|       - | 4113 | `	}` |
|       - | 4114 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4115 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4116 | `	/* Perform the diff */` |
|       3 | 4117 | `	pEntry = pSrc->pFirst;` |
|       3 | 4118 | `	n = pSrc->nEntry;` |
|       4 | 4119 | `	for(;;){` |
|       9 | 4120 | `		if( n < 1 ){` |
|       3 | 4121 | `			break;` |
|       - | 4122 | `		}` |
|       - | 4123 | `		/* Extract the node value */` |
|       7 | 4124 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4125 | `		if( pVal ){` |
|      11 | 4126 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4127 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4128 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4129 | `				/* Perform the lookup */` |
|       7 | 4130 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4131 | `				if( rc == SXRET_OK ){` |
|       - | 4132 | `					/* Value exist */` |
|       3 | 4133 | `					break;` |
|       - | 4134 | `				}` |
|       3 | 4135 | `			}` |
|       7 | 4136 | `			if( i >= (nArg - 1)){` |
|       - | 4137 | `				/* Perform the insertion */` |
|       5 | 4138 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4139 | `			}` |
|       3 | 4140 | `		}` |
|       - | 4141 | `		/* Point to the next entry */` |
|       7 | 4142 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4143 | `		n--;` |
|       1 | 4144 | `	}` |
|       - | 4145 | `	/* Return the freshly created array */` |
|       3 | 4146 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4147 | `	return PH7_OK;` |
|      12 | 4148 |  |
|       - | 4149 | `/*` |
|       - | 4150 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4151 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4152 | ` * Parameters` |
|       - | 4153 | ` *  $array1` |
|       - | 4154 | ` *    The array to compare from` |
|       - | 4155 | ` *  $array2` |
|       - | 4156 | ` *    An array to compare against` |
|       - | 4157 | ` *  $...` |
|       - | 4158 | ` *   More arrays to compare against` |
|       - | 4159 | ` * Return` |
|       - | 4160 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4161 | ` *  are not present in any of the other arrays.` |
|       - | 4162 | ` */` |
|      20 | 4163 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4164 |  |
|       - | 4165 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4166 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4167 | `	ph7_value *pArray;` |
|       - | 4168 | `	ph7_value *pVal;` |
|       - | 4169 | `	sxi32 rc;` |
|       - | 4170 | `	sxu32 n;` |
|       - | 4171 | `	int i;` |
|       - | 4172 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4173 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4174 | `	 * accompanying integration tests to pass. */` |
|      22 | 4175 | `	if( nArg < 1 ){` |
|       4 | 4176 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4177 | `			"ArgumentCountError",` |
|       - | 4178 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4179 | `			nArg` |
|       - | 4180 | `			);` |
|       - | 4181 | `	}` |
|      20 | 4182 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4183 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4184 | `			"TypeError",` |
|       - | 4185 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4186 | `			ph7_type_name(apArg[0])` |
|       - | 4187 | `			);` |
|       - | 4188 | `	}` |
|      32 | 4189 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4190 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4191 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4192 | `				"TypeError",` |
|       - | 4193 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4194 | `				i + 1,` |
|       4 | 4195 | `				ph7_type_name(apArg[i])` |
|       - | 4196 | `				);` |
|       - | 4197 | `		}` |
|       9 | 4198 | `	}` |
|      13 | 4199 | `	if( nArg == 1 ){` |
|       - | 4200 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4201 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4202 | `		return PH7_OK;` |
|       - | 4203 | `	}` |
|       - | 4204 | `	/* Create a new array */` |
|      11 | 4205 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4206 | `	if( pArray == 0 ){` |
|     ! 0 | 4207 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4208 | `		return PH7_OK;` |
|       - | 4209 | `	}` |
|       - | 4210 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4211 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4212 | `	/* Perform the diff */` |
|      11 | 4213 | `	pEntry = pSrc->pFirst;` |
|      11 | 4214 | `	n = pSrc->nEntry;` |
|      11 | 4215 | `	pN1 = pN2 = 0;` |
|      29 | 4216 | `	for(;;){` |
|       - | 4217 | `		int keep;` |
|      35 | 4218 | `		if( n < 1 ){` |
|      11 | 4219 | `			break;` |
|       - | 4220 | `		}` |
|       - | 4221 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4222 | `		keep = 1;` |
|      41 | 4223 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4224 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4225 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4226 | `			/* Perform a key lookup first */` |
|      29 | 4227 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4228 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4229 | `			}else{` |
|      17 | 4230 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4231 | `			}` |
|      29 | 4232 | `			if( rc != SXRET_OK ){` |
|       - | 4233 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4234 | `				continue;` |
|       - | 4235 | `			}` |
|       - | 4236 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4237 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4238 | `			if( pVal ){` |
|       - | 4239 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4240 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4241 | `				if( pVal2 ){` |
|      15 | 4242 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4243 | `					if( cmp == 0 ){` |
|       - | 4244 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4245 | `						keep = 0;` |
|      13 | 4246 | `						break;` |
|       - | 4247 | `					}` |
|       1 | 4248 | `				}` |
|       1 | 4249 | `			}` |
|       2 | 4250 | `		}` |
|      25 | 4251 | `		if( keep ){` |
|       - | 4252 | `			/* Perform the insertion */` |
|      13 | 4253 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4254 | `		}` |
|       - | 4255 | `		/* Point to the next entry */` |
|      25 | 4256 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4257 | `		n--;` |
|       1 | 4258 | `	}` |
|       - | 4259 | `	/* Return the freshly created array */` |
|      11 | 4260 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4261 | `	return PH7_OK;` |
|      12 | 4262 |  |
|       - | 4263 | `/*` |
|       - | 4264 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4265 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4266 | ` *  by a user supplied callback function.` |
|       - | 4267 | ` * Parameters` |
|       - | 4268 | ` *  $array1` |
|       - | 4269 | ` *    The array to compare from` |
|       - | 4270 | ` *  $array2` |
|       - | 4271 | ` *    An array to compare against` |
|       - | 4272 | ` *  $...` |
|       - | 4273 | ` *   More arrays to compare against.` |
|       - | 4274 | ` *  $key_compare_func` |
|       - | 4275 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4276 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4277 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4278 | ` * Return` |
|       - | 4279 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4280 | ` *  are not present in any of the other arrays.` |
|       - | 4281 | ` */` |
|      22 | 4282 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4283 |  |
|       - | 4284 | `	ph7_hashmap_node *pEntry;` |
|       - | 4285 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4286 | `	ph7_value *pCallback;` |
|       - | 4287 | `	ph7_value *pArray;` |
|       - | 4288 | `	sxi32 rc;` |
|       - | 4289 | `	sxu32 n;` |
|       - | 4290 | `	int i;` |
|       - | 4291 |  |
|       - | 4292 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4293 | `	if( nArg < 2 ){` |
|       4 | 4294 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4295 | `			"ArgumentCountError",` |
|       - | 4296 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4297 | `			nArg` |
|       - | 4298 | `			);` |
|       - | 4299 | `	}` |
|      22 | 4300 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4301 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4302 | `			"TypeError",` |
|       - | 4303 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4304 | `			ph7_type_name(apArg[0])` |
|       - | 4305 | `			);` |
|       - | 4306 | `	}` |
|       - | 4307 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4308 | `	 * expected to be a callback. */` |
|      32 | 4309 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4310 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4311 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4312 | `				"TypeError",` |
|       - | 4313 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4314 | `				i + 1,` |
|       2 | 4315 | `				ph7_type_name(apArg[i])` |
|       - | 4316 | `				);` |
|       - | 4317 | `		}` |
|       8 | 4318 | `	}` |
|       - | 4319 | `	/* Point to the callback value */` |
|      18 | 4320 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4321 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4322 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4323 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4324 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4325 | `		 * string given" which we also reproduce. */` |
|       7 | 4326 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4327 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4328 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4329 | `				"TypeError",` |
|       - | 4330 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4331 | `				nArg` |
|       - | 4332 | `				);` |
|       - | 4333 | `		}` |
|       5 | 4334 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4335 | `			/* neither array nor string */` |
|       7 | 4336 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4337 | `				"TypeError",` |
|       - | 4338 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4339 | `				nArg` |
|       - | 4340 | `				);` |
|       - | 4341 | `		}` |
|       - | 4342 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4343 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4344 | `			"TypeError",` |
|       - | 4345 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4346 | `			nArg,` |
|     ! 0 | 4347 | `			ph7_type_name(pCallback)` |
|       - | 4348 | `			);` |
|       - | 4349 | `	}` |
|      11 | 4350 | `	if( nArg == 2 ){` |
|       - | 4351 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4352 | `		 * input array. */` |
|       3 | 4353 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4354 | `		return PH7_OK;` |
|       - | 4355 | `	}` |
|       - | 4356 | `	/* Create a new array */` |
|       9 | 4357 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4358 | `	if( pArray == 0 ){` |
|     ! 0 | 4359 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4360 | `		return PH7_OK;` |
|       - | 4361 | `	}` |
|       - | 4362 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4363 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4364 | `	/* Perform the diff */` |
|       9 | 4365 | `	pEntry = pSrc->pFirst;` |
|       9 | 4366 | `	n = pSrc->nEntry;` |
|      20 | 4367 | `	for(;;){` |
|       - | 4368 | `		int keep;` |
|      25 | 4369 | `		if( n < 1 ){` |
|       9 | 4370 | `			break;` |
|       - | 4371 | `		}` |
|      17 | 4372 | `		keep = 1;` |
|      29 | 4373 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4374 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4375 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4376 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4377 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4378 | `			while( pIt ){` |
|       - | 4379 | `				/* build temporary key values for callback */` |
|       - | 4380 | `				ph7_value key1, key2, result;` |
|       - | 4381 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4382 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4383 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4384 | `				}else{` |
|       - | 4385 | `					SyString sStr;` |
|      31 | 4386 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4387 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4388 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4389 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4390 | `				}` |
|      31 | 4391 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4392 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4393 | `				}else{` |
|       - | 4394 | `					SyString sStr;` |
|      31 | 4395 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4396 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4397 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4398 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4399 | `				}` |
|      31 | 4400 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4401 | `				/* call user callback with (key1, key2) */` |
|       - | 4402 | `				{` |
|       - | 4403 | `					ph7_value *apK[2];` |
|      31 | 4404 | `					apK[0] = &key1;` |
|      31 | 4405 | `					apK[1] = &key2;` |
|      31 | 4406 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4407 | `				}` |
|      31 | 4408 | `				if( rc == SXRET_OK ){` |
|      31 | 4409 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4410 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4411 | `					}` |
|      31 | 4412 | `					if( result.x.iVal == 0 ){` |
|       - | 4413 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4414 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4415 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4416 | `						if( pVal1 && pVal2 ){` |
|      13 | 4417 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4418 | `								keep = 0;` |
|       9 | 4419 | `								PH7_MemObjRelease(&result);` |
|       - | 4420 | `								/* release keys too before breaking */` |
|       9 | 4421 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4422 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4423 | `								break;` |
|       - | 4424 | `							}` |
|       2 | 4425 | `						}` |
|       2 | 4426 | `					}` |
|      11 | 4427 | `				}` |
|      23 | 4428 | `				PH7_MemObjRelease(&result);` |
|      23 | 4429 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4430 | `				PH7_MemObjRelease(&key2);` |
|       - | 4431 | `				/* move to next node */` |
|      23 | 4432 | `				pIt = pIt->pPrev;` |
|      23 | 4433 | `				if( keep == 0 ) break;` |
|       1 | 4434 | `			}` |
|      21 | 4435 | `			if( keep == 0 ) break;` |
|       7 | 4436 | `		}` |
|      17 | 4437 | `		if( keep ){` |
|       - | 4438 | `			/* Perform the insertion */` |
|       9 | 4439 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4440 | `		}` |
|       - | 4441 | `		/* Point to the next entry */` |
|      17 | 4442 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4443 | `		n--;` |
|       1 | 4444 | `	}` |
|       - | 4445 | `	/* Return the freshly created array */` |
|       9 | 4446 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4447 | `	return PH7_OK;` |
|      13 | 4448 |  |
|       - | 4449 | `/*` |
|       - | 4450 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4451 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4452 | ` * Parameters` |
|       - | 4453 | ` *  $array1` |
|       - | 4454 | ` *    The array to compare from` |
|       - | 4455 | ` *  $array2` |
|       - | 4456 | ` *    An array to compare against` |
|       - | 4457 | ` *  $...` |
|       - | 4458 | ` *   More arrays to compare against` |
|       - | 4459 | ` * Return` |
|       - | 4460 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4461 | ` *  in any of the other arrays.` |
|       - | 4462 | ` * Note that NULL is returned on failure.` |
|       - | 4463 | ` */` |
|      14 | 4464 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4465 |  |
|       - | 4466 | `	ph7_hashmap_node *pEntry;` |
|       - | 4467 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4468 | `	ph7_value *pArray;` |
|       - | 4469 | `	sxi32 rc;` |
|       - | 4470 | `	sxu32 n;` |
|       - | 4471 | `	int i;` |
|       - | 4472 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4473 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4474 | `	 * helpers. */` |
|      16 | 4475 | `	if( nArg < 1 ){` |
|       4 | 4476 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4477 | `			"ArgumentCountError",` |
|       - | 4478 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4479 | `			nArg` |
|       - | 4480 | `			);` |
|       - | 4481 | `	}` |
|      14 | 4482 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4483 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4484 | `			"TypeError",` |
|       - | 4485 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4486 | `			ph7_type_name(apArg[0])` |
|       - | 4487 | `			);` |
|       - | 4488 | `	}` |
|      20 | 4489 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4490 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4491 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4492 | `				"TypeError",` |
|       - | 4493 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4494 | `				i + 1,` |
|       2 | 4495 | `				ph7_type_name(apArg[i])` |
|       - | 4496 | `				);` |
|       - | 4497 | `		}` |
|       5 | 4498 | `	}` |
|       9 | 4499 | `	if( nArg == 1 ){` |
|       - | 4500 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4501 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4502 | `		return PH7_OK;` |
|       - | 4503 | `	}` |
|       - | 4504 | `	/* Create a new array */` |
|       7 | 4505 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4506 | `	if( pArray == 0 ){` |
|     ! 0 | 4507 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4508 | `		return PH7_OK;` |
|       - | 4509 | `	}` |
|       - | 4510 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4511 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4512 | `	/* Perfrom the diff */` |
|       7 | 4513 | `	pEntry = pSrc->pFirst;` |
|       7 | 4514 | `	n = pSrc->nEntry;` |
|      12 | 4515 | `	for(;;){` |
|      25 | 4516 | `		if( n < 1 ){` |
|       7 | 4517 | `			break;` |
|       - | 4518 | `		}` |
|      31 | 4519 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4520 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4521 | `				/* ignore */` |
|     ! 0 | 4522 | `				continue;` |
|       - | 4523 | `			}` |
|      23 | 4524 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4525 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4526 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4527 | `				/* Blob lookup */` |
|      17 | 4528 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4529 | `			}else{` |
|       - | 4530 | `				/* Int lookup */` |
|       7 | 4531 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4532 | `			}` |
|      23 | 4533 | `			if( rc == SXRET_OK ){` |
|       - | 4534 | `				/* Key exists,break immediately */` |
|      11 | 4535 | `				break;` |
|       - | 4536 | `			}` |
|       7 | 4537 | `		}` |
|      19 | 4538 | `		if( i >= nArg ){` |
|       - | 4539 | `			/* Perform the insertion */` |
|       9 | 4540 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4541 | `		}` |
|       - | 4542 | `		/* Point to the next entry */` |
|      19 | 4543 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4544 | `		n--;` |
|       1 | 4545 | `	}` |
|       - | 4546 | `	/* Return the freshly created array */` |
|       7 | 4547 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4548 | `	return PH7_OK;` |
|       9 | 4549 |  |
|       - | 4550 | `/*` |
|       - | 4551 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4552 | ` *  Computes the intersection of arrays.` |
|       - | 4553 | ` * Parameters` |
|       - | 4554 | ` *  $array1` |
|       - | 4555 | ` *    The array to compare from` |
|       - | 4556 | ` *  $array2` |
|       - | 4557 | ` *    An array to compare against` |
|       - | 4558 | ` *  $...` |
|       - | 4559 | ` *   More arrays to compare against` |
|       - | 4560 | ` * Return` |
|       - | 4561 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4562 | ` *  in all of the parameters.` |
|       - | 4563 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4564 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4565 | ` */` |
|      22 | 4566 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4567 |  |
|       - | 4568 | `	ph7_hashmap_node *pEntry;` |
|       - | 4569 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4570 | `	ph7_value *pArray;` |
|       - | 4571 | `	ph7_value *pVal;` |
|       - | 4572 | `	sxi32 rc;` |
|       - | 4573 | `	sxu32 n;` |
|       - | 4574 | `	int i;` |
|      24 | 4575 | `	if( nArg < 1 ){` |
|       4 | 4576 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4577 | `			"ArgumentCountError",` |
|       - | 4578 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4579 | `			nArg` |
|       - | 4580 | `			);` |
|       - | 4581 | `	}` |
|      22 | 4582 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4583 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4584 | `			"TypeError",` |
|       - | 4585 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4586 | `			ph7_type_name(apArg[0])` |
|       - | 4587 | `			);` |
|       - | 4588 | `	}` |
|      36 | 4589 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4590 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4591 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4592 | `				"TypeError",` |
|       - | 4593 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4594 | `				i + 1,` |
|       2 | 4595 | `				ph7_type_name(apArg[i])` |
|       - | 4596 | `				);` |
|       - | 4597 | `		}` |
|       9 | 4598 | `	}` |
|      17 | 4599 | `	if( nArg == 1 ){` |
|       - | 4600 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4601 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4602 | `		return PH7_OK;` |
|       - | 4603 | `	}` |
|       - | 4604 | `	/* Create a new array */` |
|      15 | 4605 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4606 | `	if( pArray == 0 ){` |
|     ! 0 | 4607 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4608 | `		return PH7_OK;` |
|       - | 4609 | `	}` |
|       - | 4610 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4611 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4612 | `	/* Perform the intersection */` |
|      15 | 4613 | `	pEntry = pSrc->pFirst;` |
|      15 | 4614 | `	n = pSrc->nEntry;` |
|      31 | 4615 | `	for(;;){` |
|      63 | 4616 | `		if( n < 1 ){` |
|      15 | 4617 | `			break;` |
|       - | 4618 | `		}` |
|       - | 4619 | `		/* Extract the node value */` |
|      49 | 4620 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4621 | `		if( pVal ){` |
|      79 | 4622 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4623 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4624 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4625 | `				/* Perform the lookup */` |
|      55 | 4626 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4627 | `				if( rc != SXRET_OK ){` |
|       - | 4628 | `					/* Value does not exist */` |
|      25 | 4629 | `					break;` |
|       - | 4630 | `				}` |
|      16 | 4631 | `			}` |
|      49 | 4632 | `			if( i >= nArg ){` |
|       - | 4633 | `				/* Perform the insertion */` |
|      25 | 4634 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4635 | `			}` |
|      24 | 4636 | `		}` |
|       - | 4637 | `		/* Point to the next entry */` |
|      49 | 4638 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4639 | `		n--;` |
|       1 | 4640 | `	}` |
|       - | 4641 | `	/* Return the freshly created array */` |
|      15 | 4642 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4643 | `	return PH7_OK;` |
|      13 | 4644 |  |
|       - | 4645 | `/*` |
|       - | 4646 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4647 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4648 | ` * Parameters` |
|       - | 4649 | ` *  $array1` |
|       - | 4650 | ` *    The array to compare from` |
|       - | 4651 | ` *  $array2` |
|       - | 4652 | ` *    An array to compare against` |
|       - | 4653 | ` *  $...` |
|       - | 4654 | ` *   More arrays to compare against` |
|       - | 4655 | ` * Return` |
|       - | 4656 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4657 | ` *  in all the arguments, with matching keys.` |
|       - | 4658 | ` */` |
|      22 | 4659 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4660 |  |
|       - | 4661 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4662 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4663 | `	ph7_value *pArray;` |
|       - | 4664 | `	ph7_value *pVal;` |
|       - | 4665 | `	sxi32 rc;` |
|       - | 4666 | `	sxu32 n;` |
|       - | 4667 | `	int i;` |
|      24 | 4668 | `	if( nArg < 1 ){` |
|       4 | 4669 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4670 | `			"ArgumentCountError",` |
|       - | 4671 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4672 | `			nArg` |
|       - | 4673 | `			);` |
|       - | 4674 | `	}` |
|      22 | 4675 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4676 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4677 | `			"TypeError",` |
|       - | 4678 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4679 | `			ph7_type_name(apArg[0])` |
|       - | 4680 | `			);` |
|       - | 4681 | `	}` |
|      36 | 4682 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4683 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4684 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4685 | `				"TypeError",` |
|       - | 4686 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4687 | `				i + 1,` |
|       2 | 4688 | `				ph7_type_name(apArg[i])` |
|       - | 4689 | `				);` |
|       - | 4690 | `		}` |
|       9 | 4691 | `	}` |
|      17 | 4692 | `	if( nArg == 1 ){` |
|       - | 4693 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4694 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4695 | `		return PH7_OK;` |
|       - | 4696 | `	}` |
|       - | 4697 | `	/* Create a new array */` |
|      15 | 4698 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4699 | `	if( pArray == 0 ){` |
|     ! 0 | 4700 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4701 | `		return PH7_OK;` |
|       - | 4702 | `	}` |
|       - | 4703 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4704 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4705 | `	/* Perform the intersection */` |
|      15 | 4706 | `	pEntry = pSrc->pFirst;` |
|      15 | 4707 | `	n = pSrc->nEntry;` |
|      15 | 4708 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4709 | `	for(;;){` |
|      47 | 4710 | `		if( n < 1 ){` |
|      15 | 4711 | `			break;` |
|       - | 4712 | `		}` |
|       - | 4713 | `		/* Extract the node value */` |
|      33 | 4714 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4715 | `		if( pVal ){` |
|      53 | 4716 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4717 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4718 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4719 | `				/* Perform a key lookup first */` |
|      37 | 4720 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4721 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4722 | `				}else{` |
|      23 | 4723 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4724 | `				}` |
|      37 | 4725 | `				if( rc != SXRET_OK ){` |
|       - | 4726 | `					/* No such key,break immediately */` |
|       7 | 4727 | `					break;` |
|       - | 4728 | `				}` |
|       - | 4729 | `				/* Perform the lookup */` |
|      31 | 4730 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4731 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4732 | `					/* Value does not exist */` |
|       6 | 4733 | `					break;` |
|       - | 4734 | `				}` |
|      11 | 4735 | `			}` |
|      33 | 4736 | `			if( i >= nArg ){` |
|       - | 4737 | `				/* Perform the insertion */` |
|      17 | 4738 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4739 | `			}` |
|      16 | 4740 | `		}` |
|       - | 4741 | `		/* Point to the next entry */` |
|      33 | 4742 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4743 | `		n--;` |
|       1 | 4744 | `	}` |
|       - | 4745 | `	/* Return the freshly created array */` |
|      15 | 4746 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4747 | `	return PH7_OK;` |
|      13 | 4748 |  |
|       - | 4749 | `/*` |
|       - | 4750 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 4751 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4752 | ` * Parameters` |
|       - | 4753 | ` *  $array1` |
|       - | 4754 | ` *    The array to compare from` |
|       - | 4755 | ` *  $...` |
|       - | 4756 | ` *   More arrays to compare against` |
|       - | 4757 | ` * Return` |
|       - | 4758 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4759 | ` *  have keys that are present in all arguments.` |
|       - | 4760 | ` * Note that NULL is returned on failure.` |
|       - | 4761 | ` */` |
|      22 | 4762 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4763 |  |
|       - | 4764 | `	ph7_hashmap_node *pEntry;` |
|       - | 4765 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4766 | `	ph7_value *pArray;` |
|       - | 4767 | `	sxi32 rc;` |
|       - | 4768 | `	sxu32 n;` |
|       - | 4769 | `	int i;` |
|      24 | 4770 | `	if( nArg < 1 ){` |
|       4 | 4771 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4772 | `			"ArgumentCountError",` |
|       - | 4773 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 4774 | `			nArg` |
|       - | 4775 | `			);` |
|       - | 4776 | `	}` |
|      22 | 4777 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4778 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4779 | `			"TypeError",` |
|       - | 4780 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4781 | `			ph7_type_name(apArg[0])` |
|       - | 4782 | `			);` |
|       - | 4783 | `	}` |
|      36 | 4784 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4785 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4786 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4787 | `				"TypeError",` |
|       - | 4788 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4789 | `				i + 1,` |
|       2 | 4790 | `				ph7_type_name(apArg[i])` |
|       - | 4791 | `				);` |
|       - | 4792 | `		}` |
|       9 | 4793 | `	}` |
|      17 | 4794 | `	if( nArg == 1 ){` |
|       - | 4795 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4796 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4797 | `		return PH7_OK;` |
|       - | 4798 | `	}` |
|       - | 4799 | `	/* Create a new array */` |
|      15 | 4800 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4801 | `	if( pArray == 0 ){` |
|     ! 0 | 4802 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4803 | `		return PH7_OK;` |
|       - | 4804 | `	}` |
|       - | 4805 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 4806 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4807 | `	/* Perform the intersection */` |
|      15 | 4808 | `	pEntry = pSrc->pFirst;` |
|      15 | 4809 | `	n = pSrc->nEntry;` |
|      24 | 4810 | `	for(;;){` |
|      49 | 4811 | `		if( n < 1 ){` |
|      15 | 4812 | `			break;` |
|       - | 4813 | `		}` |
|      57 | 4814 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 4815 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 4816 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 4817 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4818 | `				/* Blob lookup */` |
|      27 | 4819 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 4820 | `			}else{` |
|       - | 4821 | `				/* Int key */` |
|      13 | 4822 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4823 | `			}` |
|      39 | 4824 | `			if( rc != SXRET_OK ){` |
|       - | 4825 | `				/* Key does not exist, break immediately */` |
|      17 | 4826 | `				break;` |
|       - | 4827 | `			}` |
|      12 | 4828 | `		}` |
|      35 | 4829 | `		if( i >= nArg ){` |
|       - | 4830 | `			/* Perform the insertion */` |
|      19 | 4831 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 4832 | `		}` |
|       - | 4833 | `		/* Point to the next entry */` |
|      35 | 4834 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 4835 | `		n--;` |
|       1 | 4836 | `	}` |
|       - | 4837 | `	/* Return the freshly created array */` |
|      15 | 4838 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4839 | `	return PH7_OK;` |
|      13 | 4840 |  |
|       - | 4841 | `/*` |
|       - | 4842 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4843 | ` *  Computes the intersection of arrays.` |
|       - | 4844 | ` * Parameters` |
|       - | 4845 | ` *  $array1` |
|       - | 4846 | ` *    The array to compare from` |
|       - | 4847 | ` *  $array2` |
|       - | 4848 | ` *    An array to compare against` |
|       - | 4849 | ` *  $...` |
|       - | 4850 | ` *   More arrays to compare against` |
|       - | 4851 | ` * $callback` |
|       - | 4852 | ` *  The callback comparison function.` |
|       - | 4853 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4854 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4855 | ` *  than the second.` |
|       - | 4856 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4857 | ` * Return` |
|       - | 4858 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4859 | ` *  in all of the parameters. .` |
|       - | 4860 | ` * Note that NULL is returned on failure.` |
|       - | 4861 | ` */` |
|       2 | 4862 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4863 |  |
|       - | 4864 | `	ph7_hashmap_node *pEntry;` |
|       - | 4865 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4866 | `	ph7_value *pCallback;` |
|       - | 4867 | `	ph7_value *pArray;` |
|       - | 4868 | `	ph7_value *pVal;` |
|       - | 4869 | `	sxi32 rc;` |
|       - | 4870 | `	sxu32 n;` |
|       - | 4871 | `	int i;` |
|       - | 4872 |  |
|       3 | 4873 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4874 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4875 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4876 | `		return PH7_OK;` |
|       - | 4877 | `	}` |
|       - | 4878 | `	/* Point to the callback */` |
|       3 | 4879 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4880 | `	if( nArg == 2 ){` |
|       - | 4881 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4882 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4883 | `		return PH7_OK;` |
|       - | 4884 | `	}` |
|       - | 4885 | `	/* Create a new array */` |
|       3 | 4886 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4887 | `	if( pArray == 0 ){` |
|     ! 0 | 4888 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4889 | `		return PH7_OK;` |
|       - | 4890 | `	}` |
|       - | 4891 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4892 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4893 | `	/* Perform the intersection */` |
|       3 | 4894 | `	pEntry = pSrc->pFirst;` |
|       3 | 4895 | `	n = pSrc->nEntry;` |
|       4 | 4896 | `	for(;;){` |
|       9 | 4897 | `		if( n < 1 ){` |
|       3 | 4898 | `			break;` |
|       - | 4899 | `		}` |
|       - | 4900 | `		/* Extract the node value */` |
|       7 | 4901 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4902 | `		if( pVal ){` |
|      11 | 4903 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4904 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4905 | `					/* ignore */` |
|     ! 0 | 4906 | `					continue;` |
|       - | 4907 | `				}` |
|       - | 4908 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4909 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4910 | `				/* Perform the lookup */` |
|       7 | 4911 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4912 | `				if( rc != SXRET_OK ){` |
|       - | 4913 | `					/* Value does not exist */` |
|       3 | 4914 | `					break;` |
|       - | 4915 | `				}` |
|       3 | 4916 | `			}` |
|       7 | 4917 | `			if( i >= (nArg-1) ){` |
|       - | 4918 | `				/* Perform the insertion */` |
|       5 | 4919 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4920 | `			}` |
|       3 | 4921 | `		}` |
|       - | 4922 | `		/* Point to the next entry */` |
|       7 | 4923 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4924 | `		n--;` |
|       1 | 4925 | `	}` |
|       - | 4926 | `	/* Return the freshly created array */` |
|       3 | 4927 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4928 | `	return PH7_OK;` |
|       2 | 4929 |  |
|       - | 4930 | `/*` |
|       - | 4931 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 4932 | ` *  Fill an array with values.` |
|       - | 4933 | ` * Parameters` |
|       - | 4934 | ` *  $start_index` |
|       - | 4935 | ` *    The first index of the returned array.` |
|       - | 4936 | ` *  $num` |
|       - | 4937 | ` *   Number of elements to insert.` |
|       - | 4938 | ` *  $value` |
|       - | 4939 | ` *    Value to use for filling.` |
|       - | 4940 | ` * Return` |
|       - | 4941 | ` *  The filled array or null on failure.` |
|       - | 4942 | ` */` |
|     238 | 4943 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4944 |  |
|       - | 4945 | `	ph7_value *pArray;` |
|       - | 4946 | `	int i,nEntry;` |
|       - | 4947 |  |
|       - | 4948 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 4949 | `	if( nArg != 3 ){` |
|       - | 4950 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 4951 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4952 | `			"ArgumentCountError",` |
|       - | 4953 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 4954 | `			nArg` |
|       - | 4955 | `			);` |
|       - | 4956 | `	}` |
|       - | 4957 |  |
|       - | 4958 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 4959 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 4960 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 4961 | `	 * and NULLs are rejected outright. */` |
|     466 | 4962 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 4963 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 4964 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4965 | `			"TypeError",` |
|       - | 4966 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 4967 | `			ph7_type_name(apArg[0])` |
|       - | 4968 | `			);` |
|       - | 4969 | `	}` |
|     234 | 4970 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 4971 | `		int len;` |
|       8 | 4972 | `		sxu8 bReal = FALSE;` |
|       8 | 4973 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 4974 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 4975 | `			/* Non‑numeric string is an error. */` |
|       3 | 4976 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4977 | `				"TypeError",` |
|       - | 4978 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 4979 | `				);` |
|       - | 4980 | `		}` |
|       5 | 4981 | `		if( bReal ){` |
|       - | 4982 | `			/* float-string -> deprecation warning */` |
|       4 | 4983 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4984 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 4985 | `				zStr` |
|       - | 4986 | `				);` |
|       1 | 4987 | `		}` |
|       2 | 4988 | `	}` |
|       - | 4989 |  |
|       - | 4990 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 4991 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 4992 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 4993 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4994 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4995 | `			"TypeError",` |
|       - | 4996 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 4997 | `			ph7_type_name(apArg[1])` |
|       - | 4998 | `			);` |
|       - | 4999 | `	}` |
|     232 | 5000 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5001 | `		int len;` |
|       3 | 5002 | `		sxu8 bReal = FALSE;` |
|       3 | 5003 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5004 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5005 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5006 | `				"TypeError",` |
|       - | 5007 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5008 | `				);` |
|       - | 5009 | `		}` |
|     ! 0 | 5010 | `	}` |
|       - | 5011 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5012 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5013 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5014 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5015 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5016 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5017 | `		if( d != (double)i64 ){` |
|       7 | 5018 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5019 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5020 | `				d` |
|       - | 5021 | `				);` |
|       2 | 5022 | `		}` |
|       2 | 5023 | `	}` |
|       - | 5024 |  |
|       - | 5025 | `	/* Total number of entries to insert */` |
|     230 | 5026 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5027 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5028 | `	if( nEntry < 0 ){` |
|       3 | 5029 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5030 | `			"ValueError",` |
|       - | 5031 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5032 | `			);` |
|       - | 5033 | `	}` |
|       - | 5034 |  |
|       - | 5035 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5036 | `	if( nEntry == 0 ){` |
|       7 | 5037 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5038 | `		return PH7_OK;` |
|       - | 5039 | `	}` |
|       - | 5040 |  |
|       - | 5041 | `	/* Create a new array */` |
|     221 | 5042 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5043 | `	if( pArray == 0 ){` |
|     ! 0 | 5044 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5045 | `		return PH7_OK;` |
|       - | 5046 | `	}` |
|       - | 5047 |  |
|       - | 5048 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5049 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 5050 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5051 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5052 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 5053 | `	}` |
|       - | 5054 | `	/* Return the filled array */` |
|     221 | 5055 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5056 | `	return PH7_OK;` |
|     121 | 5057 |  |
|       - | 5058 | `/*` |
|       - | 5059 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5060 | ` *  Fill an array with values, specifying keys.` |
|       - | 5061 | ` * Parameters` |
|       - | 5062 | ` *  $input` |
|       - | 5063 | ` *   Array of values that will be used as key.` |
|       - | 5064 | ` *  $value` |
|       - | 5065 | ` *    Value to use for filling.` |
|       - | 5066 | ` * Return` |
|       - | 5067 | ` *  The filled array.` |
|       - | 5068 | ` * Throws` |
|       - | 5069 | ` *  ValueError if $input is not an array.` |
|       - | 5070 | ` */` |
|      26 | 5071 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5072 |  |
|       - | 5073 | `	ph7_hashmap_node *pEntry;` |
|       - | 5074 | `	ph7_hashmap *pSrc;` |
|       - | 5075 | `	ph7_value *pArray;` |
|       - | 5076 | `	sxu32 n;` |
|       - | 5077 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5078 | `	if( nArg != 2 ){` |
|      10 | 5079 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5080 | `			"ArgumentCountError",` |
|       - | 5081 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5082 | `			nArg` |
|       - | 5083 | `			);` |
|       - | 5084 | `	}` |
|       - | 5085 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5086 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5087 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5088 | `			"TypeError",` |
|       - | 5089 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5090 | `			ph7_type_name(apArg[0])` |
|       - | 5091 | `			);` |
|       - | 5092 | `	}` |
|       - | 5093 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5094 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5095 | `	/* Create a new array */` |
|      17 | 5096 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5097 | `	if( pArray == 0 ){` |
|     ! 0 | 5098 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5099 | `		return PH7_OK;` |
|       - | 5100 | `	}` |
|       - | 5101 | `	/* Perform the requested operation */` |
|      17 | 5102 | `	pEntry = pSrc->pFirst;` |
|      45 | 5103 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5104 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5105 | `		/* Point to the next entry */` |
|      29 | 5106 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5107 | `	}` |
|       - | 5108 | `	/* Return the filled array */` |
|      17 | 5109 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5110 | `	return PH7_OK;` |
|      15 | 5111 |  |
|       - | 5112 | `/*` |
|       - | 5113 | ` * array array_combine(array $keys,array $values)` |
|       - | 5114 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5115 | ` * Parameters` |
|       - | 5116 | ` *  $keys` |
|       - | 5117 | ` *    Array of keys to be used.` |
|       - | 5118 | ` * $values` |
|       - | 5119 | ` *   Array of values to be used.` |
|       - | 5120 | ` * Return` |
|       - | 5121 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5122 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5123 | ` *  not an array.` |
|       - | 5124 | ` */` |
|      18 | 5125 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5126 |  |
|       - | 5127 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5128 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5129 | `	ph7_value *pArray;` |
|       - | 5130 | `	sxu32 n;` |
|       - | 5131 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5132 | `	if( nArg != 2 ){` |
|       - | 5133 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5134 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5135 | `			"ArgumentCountError",` |
|       - | 5136 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5137 | `			nArg` |
|       - | 5138 | `			);` |
|       - | 5139 | `	}` |
|       - | 5140 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5141 | `	 * argument index in the error message. */` |
|      18 | 5142 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5143 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5144 | `			"TypeError",` |
|       - | 5145 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5146 | `			ph7_type_name(apArg[0])` |
|       - | 5147 | `			);` |
|       - | 5148 | `	}` |
|      16 | 5149 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5150 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5151 | `			"TypeError",` |
|       - | 5152 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5153 | `			ph7_type_name(apArg[1])` |
|       - | 5154 | `			);` |
|       - | 5155 | `	}` |
|       - | 5156 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5157 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5158 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5159 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5160 | `		/* Length mismatch -> ValueError */` |
|       3 | 5161 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5162 | `			"ValueError",` |
|       - | 5163 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5164 | `			);` |
|       - | 5165 | `	}` |
|       - | 5166 | `	/* Create a new array */` |
|      11 | 5167 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5168 | `	if( pArray == 0 ){` |
|     ! 0 | 5169 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5170 | `		return PH7_OK;` |
|       - | 5171 | `	}` |
|       - | 5172 | `	/* Perform the requested operation */` |
|      11 | 5173 | `	pKe = pKey->pFirst;` |
|      11 | 5174 | `	pVe = pValue->pFirst;` |
|      33 | 5175 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5176 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5177 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5178 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5179 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5180 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5181 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5182 | `		 * original array must not be mutated. */` |
|      23 | 5183 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5184 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5185 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5186 | `			if( pTmpKey ){` |
|       5 | 5187 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5188 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5189 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5190 | `				pKeyCopy = pTmpKey;` |
|       2 | 5191 | `			}` |
|       2 | 5192 | `		}` |
|      23 | 5193 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5194 | `		/* Point to the next entry */` |
|      23 | 5195 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5196 | `		pVe = pVe->pPrev;` |
|      12 | 5197 | `	}` |
|       - | 5198 | `	/* Return the filled array */` |
|      11 | 5199 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5200 | `	return PH7_OK;` |
|      11 | 5201 |  |
|       - | 5202 | `/*` |
|       - | 5203 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5204 | ` *  Return an array with elements in reverse order.` |
|       - | 5205 | ` * Parameters` |
|       - | 5206 | ` *  $array` |
|       - | 5207 | ` *   The input array.` |
|       - | 5208 | ` *  $preserve_keys (optional)` |
|       - | 5209 | ` *   If set to TRUE keys are preserved.` |
|       - | 5210 | ` * Return` |
|       - | 5211 | ` *  The reversed array.` |
|       - | 5212 | ` */` |
|      20 | 5213 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5214 |  |
|       - | 5215 | `	ph7_hashmap_node *pEntry;` |
|       - | 5216 | `	ph7_hashmap *pSrc;` |
|       - | 5217 | `	ph7_value *pArray;` |
|       - | 5218 | `	int bPreserve;` |
|       - | 5219 | `	sxu32 n;` |
|      22 | 5220 | `	if( nArg < 1 ){` |
|       4 | 5221 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5222 | `			"ArgumentCountError",` |
|       - | 5223 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5224 | `			nArg` |
|       - | 5225 | `			);` |
|       - | 5226 | `	}` |
|       - | 5227 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5228 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5229 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5230 | `			"TypeError",` |
|       - | 5231 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5232 | `			ph7_type_name(apArg[0])` |
|       - | 5233 | `			);` |
|       - | 5234 | `	}` |
|      17 | 5235 | `	bPreserve = FALSE;` |
|      17 | 5236 | `	if( nArg > 1 ){` |
|       7 | 5237 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5238 | `	}` |
|       - | 5239 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5240 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5241 | `	/* Create a new array */` |
|      17 | 5242 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5243 | `	if( pArray == 0 ){` |
|     ! 0 | 5244 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5245 | `		return PH7_OK;` |
|       - | 5246 | `	}` |
|       - | 5247 | `	/* Perform the requested operation */` |
|      17 | 5248 | `	pEntry = pSrc->pLast;` |
|      55 | 5249 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5250 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5251 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5252 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5253 | `		/* Point to the previous entry */` |
|      39 | 5254 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5255 | `	}` |
|      17 | 5256 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5257 | `	return PH7_OK;` |
|      12 | 5258 |  |
|       - | 5259 | `/*` |
|       - | 5260 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5261 | ` *  Removes duplicate values from an array.` |
|       - | 5262 | ` * Parameters` |
|       - | 5263 | ` *  $array` |
|       - | 5264 | ` *   The input array.` |
|       - | 5265 | ` *  $flags` |
|       - | 5266 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5267 | ` *   behavior using these values:` |
|       - | 5268 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5269 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5270 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5271 | ` * Return` |
|       - | 5272 | ` *  The filtered array.` |
|       - | 5273 | ` */` |
|      24 | 5274 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5275 |  |
|       - | 5276 | `	ph7_hashmap_node *pEntry;` |
|       - | 5277 | `	ph7_value *pNeedle;` |
|       - | 5278 | `	ph7_hashmap *pSrc;` |
|       - | 5279 | `	ph7_value *pArray;` |
|       - | 5280 | `	int bStrict;` |
|       - | 5281 | `	sxi32 rc;` |
|       - | 5282 | `	sxu32 n;` |
|      26 | 5283 | `	if( nArg < 1 ){` |
|       - | 5284 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5285 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5286 | `			"ArgumentCountError",` |
|       - | 5287 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5288 | `			);` |
|       - | 5289 | `	}` |
|      24 | 5290 | `	if( nArg > 2 ){` |
|       - | 5291 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5292 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5293 | `			"ArgumentCountError",` |
|       - | 5294 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5295 | `			nArg` |
|       - | 5296 | `			);` |
|       - | 5297 | `	}` |
|       - | 5298 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5299 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5300 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5301 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5302 | `			"TypeError",` |
|       - | 5303 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5304 | `			ph7_type_name(apArg[0])` |
|       - | 5305 | `			);` |
|       - | 5306 | `	}` |
|      19 | 5307 | `	bStrict = FALSE;` |
|       - | 5308 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5309 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5310 | `	/* Create a new array */` |
|      19 | 5311 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5312 | `	if( pArray == 0 ){` |
|     ! 0 | 5313 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5314 | `		return PH7_OK;` |
|       - | 5315 | `	}` |
|       - | 5316 | `	/* Perform the requested operation */` |
|      19 | 5317 | `	pEntry = pSrc->pFirst;` |
|      83 | 5318 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5319 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5320 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5321 | `		if( pNeedle ){` |
|      65 | 5322 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5323 | `		}` |
|      65 | 5324 | `		if( rc != SXRET_OK ){` |
|       - | 5325 | `			/* Perform the insertion */` |
|      37 | 5326 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5327 | `		}` |
|       - | 5328 | `		/* Point to the next entry */` |
|      65 | 5329 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5330 | `	}` |
|       - | 5331 | `	/* Return the freshly created array */` |
|      19 | 5332 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5333 | `	return PH7_OK;` |
|      14 | 5334 |  |
|       - | 5335 | `/*` |
|       - | 5336 | ` * array array_flip(array $input)` |
|       - | 5337 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5338 | ` * Parameter` |
|       - | 5339 | ` *  $input` |
|       - | 5340 | ` *   Input array.` |
|       - | 5341 | ` * Return` |
|       - | 5342 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5343 | ` */` |
|      34 | 5344 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5345 |  |
|       - | 5346 | `	ph7_hashmap_node *pEntry;` |
|       - | 5347 | `	ph7_hashmap *pSrc;` |
|       - | 5348 | `	ph7_value *pArray;` |
|       - | 5349 | `	ph7_value *pKey;` |
|       - | 5350 | `	ph7_value sVal;` |
|       - | 5351 | `	sxu32 n;` |
|       - | 5352 |  |
|       - | 5353 | `	/* PHP requires exactly one argument */` |
|      36 | 5354 | `	if( nArg != 1 ){` |
|       - | 5355 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5356 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5357 | `			"ArgumentCountError",` |
|       - | 5358 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5359 | `			nArg` |
|       - | 5360 | `			);` |
|       - | 5361 | `	}` |
|       - | 5362 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5363 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5364 | `		/* Type mismatch -> TypeError */` |
|       7 | 5365 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5366 | `			"TypeError",` |
|       - | 5367 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5368 | `			ph7_type_name(apArg[0])` |
|       - | 5369 | `			);` |
|       - | 5370 | `	}` |
|       - | 5371 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5372 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5373 | `	/* Create a new array */` |
|      27 | 5374 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5375 | `	if( pArray == 0 ){` |
|     ! 0 | 5376 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5377 | `		return PH7_OK;` |
|       - | 5378 | `	}` |
|       - | 5379 | `	/* Start processing */` |
|      27 | 5380 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5381 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5382 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5383 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5384 | `		if( pKey ){` |
|       - | 5385 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5386 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5387 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5388 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5389 | `					);` |
|   22236 | 5390 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5391 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5392 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5393 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5394 | `				}else{` |
|       - | 5395 | `					SyString sStr;` |
|    2227 | 5396 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5397 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5398 | `				}` |
|       - | 5399 | `				/* Perform the insertion */` |
|   22227 | 5400 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5401 | `				/* Safely release the value because each inserted entry` |
|       - | 5402 | `				 * has its own private copy of the value.` |
|       - | 5403 | `				 */` |
|   22227 | 5404 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5405 | `			}else{` |
|       - | 5406 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5407 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5408 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5409 | `					);` |
|       - | 5410 | `			}` |
|   11118 | 5411 | `		}` |
|       - | 5412 | `		/* Point to the next entry */` |
|   22237 | 5413 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5414 | `	}` |
|       - | 5415 | `	/* Return the freshly created array */` |
|      27 | 5416 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5417 | `	return PH7_OK;` |
|      19 | 5418 |  |
|       - | 5419 | `/*` |
|       - | 5420 | ` * number array_sum(array $array )` |
|       - | 5421 | ` *  Calculate the sum of values in an array.` |
|       - | 5422 | ` * Parameters` |
|       - | 5423 | ` *  $array: The input array.` |
|       - | 5424 | ` * Return` |
|       - | 5425 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5426 | ` */` |
|      24 | 5427 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5428 |  |
|       - | 5429 | `	ph7_hashmap_node *pEntry;` |
|       - | 5430 | `	ph7_value *pObj;` |
|      25 | 5431 | `	double dSum = 0;` |
|       - | 5432 | `	sxu32 n;` |
|      25 | 5433 | `	pEntry = pMap->pFirst;` |
|      91 | 5434 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5435 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5436 | `		if( pObj ){` |
|      67 | 5437 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5438 | `				dSum += pObj->rVal;` |
|      53 | 5439 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5440 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5441 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5442 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5443 | `					double dv = 0;` |
|      13 | 5444 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5445 | `					dSum += dv;` |
|       7 | 5446 | `				}` |
|      12 | 5447 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5448 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5449 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5450 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5451 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5452 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5453 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5454 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5455 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5456 | `			}` |
|       - | 5457 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5458 | `		}` |
|       - | 5459 | `		/* Point to the next entry */` |
|      67 | 5460 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5461 | `	}` |
|       - | 5462 | `	/* Return sum */` |
|      25 | 5463 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5464 |  |
|      18 | 5465 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5466 |  |
|       - | 5467 | `	ph7_hashmap_node *pEntry;` |
|       - | 5468 | `	ph7_value *pObj;` |
|      20 | 5469 | `	sxi64 nSum = 0;` |
|       - | 5470 | `	sxu32 n;` |
|      20 | 5471 | `	pEntry = pMap->pFirst;` |
|      80 | 5472 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5473 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5474 | `		if( pObj ){` |
|      62 | 5475 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5476 | `				nSum += pObj->x.iVal;` |
|      36 | 5477 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5478 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5479 | `					sxi64 nv = 0;` |
|       5 | 5480 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5481 | `					nSum += nv;` |
|       3 | 5482 | `				}` |
|       8 | 5483 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5484 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5485 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5486 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5487 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5488 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5489 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5490 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5491 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5492 | `			}` |
|       - | 5493 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5494 | `		}` |
|       - | 5495 | `		/* Point to the next entry */` |
|      62 | 5496 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5497 | `	}` |
|       - | 5498 | `	/* Return sum */` |
|      20 | 5499 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5500 |  |
|       - | 5501 | `/* number array_sum(array $array )` |
|       - | 5502 | ` * (See block-coment above)` |
|       - | 5503 | ` */` |
|      52 | 5504 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5505 |  |
|       - | 5506 | `	ph7_hashmap_node *pEntry;` |
|       - | 5507 | `	ph7_hashmap *pMap;` |
|       - | 5508 | `	ph7_value *pObj;` |
|      54 | 5509 | `	int useDouble = 0;` |
|       - | 5510 | `	sxu32 n;` |
|       - | 5511 | `	/* PHP requires exactly one argument */` |
|      54 | 5512 | `	if( nArg != 1 ){` |
|       7 | 5513 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5514 | `			"ArgumentCountError",` |
|       - | 5515 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5516 | `			nArg` |
|       - | 5517 | `			);` |
|       - | 5518 | `	}` |
|       - | 5519 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5520 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5521 | `		/* Type mismatch -> TypeError */` |
|       7 | 5522 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5523 | `			"TypeError",` |
|       - | 5524 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5525 | `			ph7_type_name(apArg[0])` |
|       - | 5526 | `			);` |
|       - | 5527 | `	}` |
|      46 | 5528 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5529 | `	if( pMap->nEntry < 1 ){` |
|       - | 5530 | `		/* Nothing to compute,return 0 */` |
|       3 | 5531 | `		ph7_result_int(pCtx,0);` |
|       3 | 5532 | `		return PH7_OK;` |
|       - | 5533 | `	}` |
|       - | 5534 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5535 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5536 | `	 */` |
|      44 | 5537 | `	pEntry = pMap->pFirst;` |
|     112 | 5538 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5539 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5540 | `		if( pObj ){` |
|      94 | 5541 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5542 | `				useDouble = 1;` |
|      19 | 5543 | `				break;` |
|       - | 5544 | `			}` |
|      76 | 5545 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5546 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5547 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5548 | `				sxu32 i;` |
|      23 | 5549 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5550 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5551 | `						useDouble = 1;` |
|       7 | 5552 | `						break;` |
|       - | 5553 | `					}` |
|       6 | 5554 | `				}` |
|      13 | 5555 | `				if( useDouble ){` |
|       7 | 5556 | `					break;` |
|       - | 5557 | `				}` |
|       3 | 5558 | `			}` |
|      34 | 5559 | `		}` |
|      70 | 5560 | `		pEntry = pEntry->pPrev;` |
|      36 | 5561 | `	}` |
|      44 | 5562 | `	if( useDouble ){` |
|      25 | 5563 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5564 | `	}else{` |
|      20 | 5565 | `		Int64Sum(pCtx,pMap);` |
|       - | 5566 | `	}` |
|      44 | 5567 | `	return PH7_OK;` |
|      28 | 5568 |  |
|       - | 5569 | `/*` |
|       - | 5570 | ` * number array_product(array $array )` |
|       - | 5571 | ` *  Calculate the product of values in an array.` |
|       - | 5572 | ` * Parameters` |
|       - | 5573 | ` *  $array: The input array.` |
|       - | 5574 | ` * Return` |
|       - | 5575 | ` *  Returns the product of values as an integer or float.` |
|       - | 5576 | ` */` |
|     ! 0 | 5577 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5578 |  |
|       - | 5579 | `	ph7_hashmap_node *pEntry;` |
|       - | 5580 | `	ph7_value *pObj;` |
|       - | 5581 | `	double dProd;` |
|       - | 5582 | `	sxu32 n;` |
|     ! 0 | 5583 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5584 | `	dProd = 1;` |
|     ! 0 | 5585 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5586 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5587 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5588 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5589 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5590 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5591 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5592 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5593 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5594 | `					double dv = 0;` |
|     ! 0 | 5595 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5596 | `					dProd *= dv;` |
|     ! 0 | 5597 | `				}` |
|     ! 0 | 5598 | `			}` |
|     ! 0 | 5599 | `		}` |
|       - | 5600 | `		/* Point to the next entry */` |
|     ! 0 | 5601 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5602 | `	}` |
|       - | 5603 | `	/* Return product */` |
|     ! 0 | 5604 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5605 |  |
|     ! 0 | 5606 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5607 |  |
|       - | 5608 | `	ph7_hashmap_node *pEntry;` |
|       - | 5609 | `	ph7_value *pObj;` |
|       - | 5610 | `	sxi64 nProd;` |
|       - | 5611 | `	sxu32 n;` |
|     ! 0 | 5612 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5613 | `	nProd = 1;` |
|     ! 0 | 5614 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5615 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5616 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5617 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5618 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5619 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5620 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5621 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5622 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5623 | `					sxi64 nv = 0;` |
|     ! 0 | 5624 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5625 | `					nProd *= nv;` |
|     ! 0 | 5626 | `				}` |
|     ! 0 | 5627 | `			}` |
|     ! 0 | 5628 | `		}` |
|       - | 5629 | `		/* Point to the next entry */` |
|     ! 0 | 5630 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5631 | `	}` |
|       - | 5632 | `	/* Return product */` |
|     ! 0 | 5633 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5634 |  |
|       - | 5635 | `/* number array_product(array $array )` |
|       - | 5636 | ` * (See block-block comment above)` |
|       - | 5637 | ` */` |
|     ! 0 | 5638 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5639 |  |
|       - | 5640 | `	ph7_hashmap *pMap;` |
|       - | 5641 | `	ph7_value *pObj;` |
|     ! 0 | 5642 | `	if( nArg < 1 ){` |
|       - | 5643 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5644 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5645 | `		return PH7_OK;` |
|       - | 5646 | `	}` |
|       - | 5647 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5648 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5649 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5650 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5651 | `		return PH7_OK;` |
|       - | 5652 | `	}` |
|     ! 0 | 5653 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5654 | `	if( pMap->nEntry < 1 ){` |
|       - | 5655 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5656 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5657 | `		return PH7_OK;` |
|       - | 5658 | `	}` |
|       - | 5659 | `	/* If the first element is of type float,then perform floating` |
|       - | 5660 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5661 | `	 */` |
|     ! 0 | 5662 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5663 | `	if( pObj == 0 ){` |
|     ! 0 | 5664 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5665 | `		return PH7_OK;` |
|       - | 5666 | `	}` |
|     ! 0 | 5667 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5668 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5669 | `	}else{` |
|     ! 0 | 5670 | `		Int64Prod(pCtx,pMap);` |
|       - | 5671 | `	}` |
|     ! 0 | 5672 | `	return PH7_OK;` |
|     ! 0 | 5673 |  |
|       - | 5674 | `/*` |
|       - | 5675 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5676 | ` *  Pick one or more random entries out of an array.` |
|       - | 5677 | ` * Parameters` |
|       - | 5678 | ` * $input` |
|       - | 5679 | ` *  The input array.` |
|       - | 5680 | ` * $num_req` |
|       - | 5681 | ` *  Specifies how many entries you want to pick.` |
|       - | 5682 | ` * Return` |
|       - | 5683 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5684 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5685 | ` *  NULL is returned on failure.` |
|       - | 5686 | ` */` |
|       6 | 5687 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5688 |  |
|       - | 5689 | `	ph7_hashmap_node *pNode;` |
|       - | 5690 | `	ph7_hashmap *pMap;` |
|       7 | 5691 | `	int nItem = 1;` |
|       7 | 5692 | `	if( nArg < 1 ){` |
|       - | 5693 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5694 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5695 | `		return PH7_OK;` |
|       - | 5696 | `	}` |
|       - | 5697 | `	/* Make sure we are dealing with an array */` |
|       7 | 5698 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5699 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5700 | `		return PH7_OK;` |
|       - | 5701 | `	}` |
|       - | 5702 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5703 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5704 | `	if(pMap->nEntry < 1 ){` |
|       - | 5705 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5706 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5707 | `		return PH7_OK;` |
|       - | 5708 | `	}` |
|       7 | 5709 | `	if( nArg > 1 ){` |
|       3 | 5710 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5711 | `	}` |
|       7 | 5712 | `	if( nItem < 2 ){` |
|       - | 5713 | `		sxu32 nEntry;` |
|       - | 5714 | `		/* Select a random number */` |
|       5 | 5715 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5716 | `		/* Extract the desired entry.` |
|       - | 5717 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5718 | `		 */` |
|       5 | 5719 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       4 | 5720 | `			pNode = pMap->pLast;` |
|       4 | 5721 | `			nEntry = pMap->nEntry - nEntry;` |
|       4 | 5722 | `			if( nEntry > 1 ){` |
|     ! 0 | 5723 | `				for(;;){` |
|     ! 0 | 5724 | `					if( nEntry == 0 ){` |
|     ! 0 | 5725 | `						break;` |
|       - | 5726 | `					}` |
|       - | 5727 | `					/* Point to the previous entry */` |
|     ! 0 | 5728 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5729 | `					nEntry--;` |
|     ! 0 | 5730 | `				}` |
|     ! 0 | 5731 | `			}` |
|       2 | 5732 | `		}else{` |
|       2 | 5733 | `			pNode = pMap->pFirst;` |
|       1 | 5734 | `			for(;;){` |
|       2 | 5735 | `				if( nEntry == 0 ){` |
|       2 | 5736 | `					break;` |
|       - | 5737 | `				}` |
|       - | 5738 | `				/* Point to the next entry */` |
|     ! 0 | 5739 | `				pNode = pNode->pPrev; /* Reverse link */` |
|     ! 0 | 5740 | `				nEntry--;` |
|     ! 0 | 5741 | `			}` |
|       - | 5742 | `		}` |
|       5 | 5743 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5744 | `			/* Int key */` |
|       3 | 5745 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5746 | `		}else{` |
|       - | 5747 | `			/* Blob key */` |
|       3 | 5748 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5749 | `		}` |
|       3 | 5750 | `	}else{` |
|       - | 5751 | `		ph7_value sKey,*pArray;` |
|       - | 5752 | `		ph7_hashmap *pDest;` |
|       - | 5753 | `		/* Create a new array */` |
|       3 | 5754 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5755 | `		if( pArray == 0 ){` |
|     ! 0 | 5756 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5757 | `			return PH7_OK;` |
|       - | 5758 | `		}` |
|       - | 5759 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5760 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5761 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5762 | `		/* Copy the first n items */` |
|       3 | 5763 | `		pNode = pMap->pFirst;` |
|       3 | 5764 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5765 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5766 | `		}` |
|       7 | 5767 | `		while( nItem > 0){` |
|       5 | 5768 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5769 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5770 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5771 | `			/* Point to the next entry */` |
|       5 | 5772 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5773 | `			nItem--;` |
|       1 | 5774 | `		}` |
|       - | 5775 | `		/* Shuffle the array */` |
|       3 | 5776 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5777 | `		/* Rehash node */` |
|       3 | 5778 | `		HashmapSortRehash(pDest);` |
|       - | 5779 | `		/* Return the random array */` |
|       3 | 5780 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5781 | `	}` |
|       7 | 5782 | `	return PH7_OK;` |
|       4 | 5783 |  |
|       - | 5784 | `/*` |
|       - | 5785 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5786 | ` *  Split an array into chunks.` |
|       - | 5787 | ` * Parameters` |
|       - | 5788 | ` * $input` |
|       - | 5789 | ` *   The array to work on` |
|       - | 5790 | ` * $size` |
|       - | 5791 | ` *   The size of each chunk` |
|       - | 5792 | ` * $preserve_keys` |
|       - | 5793 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5794 | ` *   the chunk numerically.` |
|       - | 5795 | ` * Return` |
|       - | 5796 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5797 | ` *  zero, with each dimension containing size elements.` |
|       - | 5798 | ` */` |
|      42 | 5799 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5800 |  |
|       - | 5801 | `	ph7_value *pArray,*pChunk;` |
|       - | 5802 | `	ph7_hashmap_node *pEntry;` |
|       - | 5803 | `	ph7_hashmap *pMap;` |
|       - | 5804 | `	int bPreserve;` |
|       - | 5805 | `	sxu32 nChunk;` |
|       - | 5806 | `	sxu32 nSize;` |
|       - | 5807 | `	sxu32 n;` |
|       - | 5808 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5809 | `	if( nArg < 2 ){` |
|       - | 5810 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5811 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5812 | `			"ArgumentCountError",` |
|       - | 5813 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5814 | `			nArg` |
|       - | 5815 | `			);` |
|       - | 5816 | `	}` |
|      42 | 5817 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5818 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5819 | `			"TypeError",` |
|       - | 5820 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5821 | `			ph7_type_name(apArg[0])` |
|       - | 5822 | `			);` |
|       - | 5823 | `	}` |
|       - | 5824 | `	/* Create a new array */` |
|      40 | 5825 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5826 | `	if( pArray == 0 ){` |
|     ! 0 | 5827 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5828 | `		return PH7_OK;` |
|       - | 5829 | `	}` |
|       - | 5830 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5831 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5832 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5833 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5834 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5835 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5836 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5837 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5838 | `			"TypeError",` |
|       - | 5839 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5840 | `			ph7_type_name(apArg[1])` |
|       - | 5841 | `			);` |
|       - | 5842 | `	}` |
|       - | 5843 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5844 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5845 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5846 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5847 | `		int len;` |
|       3 | 5848 | `		sxu8 bReal = FALSE;` |
|       3 | 5849 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5850 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5851 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5852 | `				"TypeError",` |
|       - | 5853 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5854 | `				);` |
|       - | 5855 | `		}` |
|     ! 0 | 5856 | `		if( bReal ){` |
|       - | 5857 | `			/* float-string -> warn but allow */` |
|     ! 0 | 5858 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5859 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 5860 | `				zStr` |
|       - | 5861 | `				);` |
|     ! 0 | 5862 | `		}` |
|     ! 0 | 5863 | `	}` |
|       - | 5864 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 5865 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 5866 | `	 * later via ph7_value_to_int. */` |
|      38 | 5867 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5868 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5869 | `		sxi64 i = (sxi64)d;` |
|       3 | 5870 | `		if( d != (double)i ){` |
|       4 | 5871 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5872 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 5873 | `				d` |
|       - | 5874 | `				);` |
|       1 | 5875 | `		}` |
|       1 | 5876 | `	}` |
|       - | 5877 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5878 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5879 | `	{` |
|      38 | 5880 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 5881 | `		if( nSizeSigned < 1 ){` |
|       - | 5882 | `			/* size <= 0 -> ValueError */` |
|       5 | 5883 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5884 | `				"ValueError",` |
|       - | 5885 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5886 | `				);` |
|       - | 5887 | `		}` |
|      34 | 5888 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5889 | `	}` |
|      34 | 5890 | `	if( nSize >= pMap->nEntry ){` |
|       - | 5891 | `		/* Return the whole array */` |
|       3 | 5892 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 5893 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5894 | `		return PH7_OK;` |
|       - | 5895 | `	}` |
|      32 | 5896 | `	bPreserve = 0;` |
|      32 | 5897 | `	if( nArg > 2 ){` |
|       - | 5898 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5899 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5900 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5901 | `		 * normally, matching PHP behaviour. */` |
|      45 | 5902 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 5903 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 5904 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 5905 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5906 | `				"TypeError",` |
|       - | 5907 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 5908 | `				ph7_type_name(apArg[2])` |
|       - | 5909 | `				);` |
|       - | 5910 | `		}` |
|      21 | 5911 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 5912 | `	}` |
|       - | 5913 | `	/* Start processing */` |
|      27 | 5914 | `	pEntry = pMap->pFirst;` |
|      27 | 5915 | `	nChunk = 0;` |
|      27 | 5916 | `	pChunk = 0;` |
|      27 | 5917 | `	n = pMap->nEntry;` |
|      56 | 5918 | `	for( ;; ){` |
|     113 | 5919 | `		if( n < 1 ){` |
|       - | 5920 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5921 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5922 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5923 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5924 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5925 | `			 * exists. */` |
|      27 | 5926 | `			if( pChunk ){` |
|      27 | 5927 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 5928 | `			}` |
|      27 | 5929 | `			break;` |
|       - | 5930 | `		}` |
|      87 | 5931 | `		if( nChunk < 1 ){` |
|      71 | 5932 | `			if( pChunk ){` |
|       - | 5933 | `				/* Put the first chunk */` |
|      45 | 5934 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 5935 | `			}` |
|       - | 5936 | `			/* Create a new dimension */` |
|      71 | 5937 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5938 | `												   * will be automatically released as soon we return` |
|       - | 5939 | `												   * from this function */` |
|      71 | 5940 | `			if( pChunk == 0 ){` |
|     ! 0 | 5941 | `				break;` |
|       - | 5942 | `			}` |
|      71 | 5943 | `			nChunk = nSize;` |
|      35 | 5944 | `		}` |
|       - | 5945 | `		/* Insert the entry */` |
|      87 | 5946 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5947 | `		/* Point to the next entry */` |
|      87 | 5948 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 5949 | `		nChunk--;` |
|      87 | 5950 | `		n--;` |
|       1 | 5951 | `	}` |
|       - | 5952 | `	/* Return the multidimensional array */` |
|      27 | 5953 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5954 | `	return PH7_OK;` |
|      23 | 5955 |  |
|       - | 5956 | `/*` |
|       - | 5957 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5958 | ` *  Pad array to the specified length with a value.` |
|       - | 5959 | ` * $input` |
|       - | 5960 | ` *   Initial array of values to pad.` |
|       - | 5961 | ` * $pad_size` |
|       - | 5962 | ` *   New size of the array.` |
|       - | 5963 | ` * $pad_value` |
|       - | 5964 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5965 | ` */` |
|      28 | 5966 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5967 |  |
|       - | 5968 | `	ph7_hashmap *pMap;` |
|       - | 5969 | `	ph7_value *pArray;` |
|       - | 5970 | `	int nEntry;` |
|      30 | 5971 | `	if( nArg != 3 ){` |
|      10 | 5972 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5973 | `			"ArgumentCountError",` |
|       - | 5974 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 5975 | `			nArg` |
|       - | 5976 | `			);` |
|       - | 5977 | `	}` |
|      24 | 5978 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5979 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5980 | `			"TypeError",` |
|       - | 5981 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5982 | `			ph7_type_name(apArg[0])` |
|       - | 5983 | `			);` |
|       - | 5984 | `	}` |
|       - | 5985 | `	/* Create a new array */` |
|      21 | 5986 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 5987 | `	if( pArray == 0 ){` |
|     ! 0 | 5988 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5989 | `		return PH7_OK;` |
|       - | 5990 | `	}` |
|       - | 5991 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 5992 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5993 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 5994 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 5995 | `	if( nEntry < 0 ){` |
|       9 | 5996 | `		nEntry = -nEntry;` |
|       9 | 5997 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 5998 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5999 | `			/* Insert given items first */` |
|      17 | 6000 | `			while( nEntry > 0 ){` |
|      13 | 6001 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 6002 | `				nEntry--;` |
|       1 | 6003 | `			}` |
|       - | 6004 | `			/* Merge the two arrays */` |
|       5 | 6005 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6006 | `		}else{` |
|       5 | 6007 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6008 | `		}` |
|      17 | 6009 | `	}else if( nEntry > 0 ){` |
|      11 | 6010 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6011 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6012 | `			/* Merge the two arrays first */` |
|       7 | 6013 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6014 | `			/* Insert given items */` |
|      25 | 6015 | `			while( nEntry > 0 ){` |
|      19 | 6016 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 6017 | `				nEntry--;` |
|       1 | 6018 | `			}` |
|       4 | 6019 | `		}else{` |
|       5 | 6020 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6021 | `		}` |
|       6 | 6022 | `	}else{` |
|       - | 6023 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6024 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6025 | `	}` |
|       - | 6026 | `	/* Return the new array */` |
|      21 | 6027 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6028 | `	return PH7_OK;` |
|      16 | 6029 |  |
|       - | 6030 | `/*` |
|       - | 6031 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6032 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6033 | ` * Parameters` |
|       - | 6034 | ` * $array` |
|       - | 6035 | ` *   The array in which elements are replaced.` |
|       - | 6036 | ` * $array1` |
|       - | 6037 | ` *   The array from which elements will be extracted.` |
|       - | 6038 | ` * ....` |
|       - | 6039 | ` *  More arrays from which elements will be extracted.` |
|       - | 6040 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6041 | ` * Return` |
|       - | 6042 | ` *  Returns an array.` |
|       - | 6043 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6044 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6045 | ` */` |
|      22 | 6046 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6047 |  |
|       - | 6048 | `	ph7_hashmap *pMap;` |
|       - | 6049 | `	ph7_value *pArray;` |
|       - | 6050 | `	int i;` |
|      24 | 6051 | `	if( nArg < 1 ){` |
|       3 | 6052 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6053 | `			"ArgumentCountError",` |
|       - | 6054 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6055 | `			);` |
|       - | 6056 | `	}` |
|      22 | 6057 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6058 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6059 | `			"TypeError",` |
|       - | 6060 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6061 | `			ph7_type_name(apArg[0])` |
|       - | 6062 | `			);` |
|       - | 6063 | `	}` |
|       - | 6064 | `	/* Create a new array */` |
|      20 | 6065 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6066 | `	if( pArray == 0 ){` |
|     ! 0 | 6067 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6068 | `		return PH7_OK;` |
|       - | 6069 | `	}` |
|       - | 6070 | `	/* Overwrite from the first array */` |
|      20 | 6071 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6072 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6073 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6074 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6075 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6076 | `			/* Type mismatch -> TypeError */` |
|       4 | 6077 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6078 | `				"TypeError",` |
|       - | 6079 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6080 | `				i + 1,` |
|       2 | 6081 | `				ph7_type_name(apArg[i])` |
|       - | 6082 | `				);` |
|       - | 6083 | `		}` |
|       - | 6084 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6085 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6086 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6087 | `	}` |
|       - | 6088 | `	/* Return the new array */` |
|      17 | 6089 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6090 | `	return PH7_OK;` |
|      13 | 6091 |  |
|       - | 6092 | `/*` |
|       - | 6093 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6094 | ` *  Filters elements of an array using a callback function.` |
|       - | 6095 | ` * Parameters` |
|       - | 6096 | ` *  $input` |
|       - | 6097 | ` *    The array to iterate over` |
|       - | 6098 | ` * $callback` |
|       - | 6099 | ` *    The callback function to use` |
|       - | 6100 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6101 | ` *    will be removed.` |
|       - | 6102 | ` * Return` |
|       - | 6103 | ` *  The filtered array.` |
|       - | 6104 | ` */` |
|      18 | 6105 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6106 |  |
|       - | 6107 | `	ph7_hashmap_node *pEntry;` |
|       - | 6108 | `	ph7_hashmap *pMap;` |
|       - | 6109 | `	ph7_value *pArray;` |
|       - | 6110 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6111 | `	ph7_value *pValue;` |
|       - | 6112 | `	sxi32 rc;` |
|       - | 6113 | `	int keep;` |
|       - | 6114 | `	sxu32 n;` |
|      20 | 6115 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6116 | `		/* Invalid arguments,return NULL */` |
|       5 | 6117 | `		ph7_result_null(pCtx);` |
|       5 | 6118 | `		return PH7_OK;` |
|       - | 6119 | `	}` |
|       - | 6120 | `	/* Create a new array */` |
|      16 | 6121 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 6122 | `	if( pArray == 0 ){` |
|     ! 0 | 6123 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6124 | `		return PH7_OK;` |
|       - | 6125 | `	}` |
|       - | 6126 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 6127 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 6128 | `	pEntry = pMap->pFirst;` |
|      16 | 6129 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 6130 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6131 | `	/* Perform the requested operation */` |
|      66 | 6132 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6133 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 6134 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 6135 | `		if( pValue == 0 ){` |
|       - | 6136 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6137 | `			keep = FALSE;` |
|      54 | 6138 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6139 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6140 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6141 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 6142 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6143 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6144 | `					int len;` |
|       3 | 6145 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6146 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6147 | `						"TypeError",` |
|       - | 6148 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6149 | `						zName` |
|       - | 6150 | `						);` |
|     ! 0 | 6151 | `				}else{` |
|     ! 0 | 6152 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6153 | `						"TypeError",` |
|       - | 6154 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6155 | `						ph7_type_name(apArg[1])` |
|       - | 6156 | `						);` |
|       - | 6157 | `				}` |
|       - | 6158 | `			}` |
|      23 | 6159 | `			keep = FALSE;` |
|      23 | 6160 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 6161 | `			if( rc == SXRET_OK ){` |
|       - | 6162 | `				/* Perform a boolean cast */` |
|      23 | 6163 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6164 | `			}` |
|      23 | 6165 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6166 | `		}else{` |
|       - | 6167 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6168 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6169 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6170 | `			 */` |
|      29 | 6171 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6172 | `		}` |
|      51 | 6173 | `		if( keep ){` |
|       - | 6174 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6175 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6176 | `		}` |
|       - | 6177 | `		/* Point to the next entry */` |
|      51 | 6178 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6179 | `	}` |
|      13 | 6180 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6181 | `	return PH7_OK;` |
|      11 | 6182 |  |
|       - | 6183 | `/*` |
|       - | 6184 | ` * array array_map(?callable $callback, array $array)` |
|       - | 6185 | ` *  Applies the callback to the elements of the given array.` |
|       - | 6186 | ` * Parameters` |
|       - | 6187 | ` *  $callback` |
|       - | 6188 | ` *   A callable to run for each element in the array, or NULL for the` |
|       - | 6189 | ` *   identity function (returns the array unchanged).` |
|       - | 6190 | ` *  $array` |
|       - | 6191 | ` *   An array to run through the callback function.` |
|       - | 6192 | ` * Return` |
|       - | 6193 | ` *  Returns an array containing the results of applying the callback` |
|       - | 6194 | ` *  function to each element of $array.` |
|       - | 6195 | ` */` |
|      28 | 6196 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6197 |  |
|       - | 6198 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6199 | `	ph7_hashmap_node *pEntry;` |
|       - | 6200 | `	ph7_hashmap *pMap;` |
|       - | 6201 | `	int bNullCallback;` |
|       - | 6202 | `	sxu32 n;` |
|      30 | 6203 | `	if( nArg < 2 ){` |
|       7 | 6204 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6205 | `			"ArgumentCountError",` |
|       - | 6206 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6207 | `			nArg` |
|       - | 6208 | `			);` |
|       - | 6209 | `	}` |
|      26 | 6210 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 6211 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6212 | `			"TypeError",` |
|       - | 6213 | `			"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6214 | `			ph7_type_name(apArg[1])` |
|       - | 6215 | `			);` |
|       - | 6216 | `	}` |
|      24 | 6217 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      24 | 6218 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6219 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6220 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6221 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6222 | `				"TypeError",` |
|       - | 6223 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6224 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6225 | `				zFunc` |
|       - | 6226 | `				);` |
|       - | 6227 | `		}` |
|       3 | 6228 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6229 | `			"TypeError",` |
|       - | 6230 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6231 | `			"no array or string given"` |
|       - | 6232 | `			);` |
|       - | 6233 | `	}` |
|       - | 6234 | `	/* Create a new array */` |
|      19 | 6235 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 6236 | `	if( pArray == 0 ){` |
|     ! 0 | 6237 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6238 | `		return PH7_OK;` |
|       - | 6239 | `	}` |
|       - | 6240 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6241 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      19 | 6242 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 6243 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6244 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 6245 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 6246 | `	/* Perform the requested operation */` |
|      19 | 6247 | `	pEntry = pMap->pFirst;` |
|      53 | 6248 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6249 | `		/* Extract the node value */` |
|      35 | 6250 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      35 | 6251 | `		if( pValue ){` |
|       - | 6252 | `			/* Extract the node key */` |
|      35 | 6253 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      35 | 6254 | `			if( bNullCallback ){` |
|       - | 6255 | `				/* NULL callback: identity function, keep original value */` |
|      11 | 6256 | `				ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6257 | `			}else{` |
|       - | 6258 | `				/* Invoke the supplied callback */` |
|      25 | 6259 | `				PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 6260 | `				/* Insert the callback return value */` |
|      25 | 6261 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6262 | `			}` |
|      35 | 6263 | `			PH7_MemObjRelease(&sKey);` |
|      35 | 6264 | `			PH7_MemObjRelease(&sResult);` |
|      17 | 6265 | `		}` |
|       - | 6266 | `		/* Point to the next entry */` |
|      35 | 6267 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      18 | 6268 | `	}` |
|      19 | 6269 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 6270 | `	return PH7_OK;` |
|      16 | 6271 |  |
|       - | 6272 | `/*` |
|       - | 6273 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6274 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6275 | ` * Parameters` |
|       - | 6276 | ` *  $array` |
|       - | 6277 | ` *   The input array.` |
|       - | 6278 | ` *  $callback` |
|       - | 6279 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6280 | ` *  $initial` |
|       - | 6281 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6282 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6283 | ` * Return` |
|       - | 6284 | ` *  Returns the resulting value.` |
|       - | 6285 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6286 | ` */` |
|      30 | 6287 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6288 |  |
|       - | 6289 | `	ph7_hashmap_node *pEntry;` |
|       - | 6290 | `	ph7_hashmap *pMap;` |
|       - | 6291 | `	ph7_value *pValue;` |
|       - | 6292 | `	ph7_value sResult;` |
|       - | 6293 | `	sxu32 n;` |
|      32 | 6294 | `	if( nArg < 2 ){` |
|       7 | 6295 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6296 | `			"ArgumentCountError",` |
|       - | 6297 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6298 | `			nArg` |
|       - | 6299 | `			);` |
|       - | 6300 | `	}` |
|      28 | 6301 | `	if( nArg > 3 ){` |
|       4 | 6302 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6303 | `			"ArgumentCountError",` |
|       - | 6304 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6305 | `			nArg` |
|       - | 6306 | `			);` |
|       - | 6307 | `	}` |
|      26 | 6308 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6309 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6310 | `			"TypeError",` |
|       - | 6311 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6312 | `			ph7_type_name(apArg[0])` |
|       - | 6313 | `			);` |
|       - | 6314 | `	}` |
|      24 | 6315 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6316 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6317 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6318 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6319 | `				"TypeError",` |
|       - | 6320 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6321 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6322 | `				zFunc` |
|       - | 6323 | `				);` |
|       - | 6324 | `		}` |
|       7 | 6325 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6326 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6327 | `				"TypeError",` |
|       - | 6328 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6329 | `				"array callback must have exactly two members"` |
|       - | 6330 | `				);` |
|       - | 6331 | `		}` |
|       5 | 6332 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6333 | `			"TypeError",` |
|       - | 6334 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6335 | `			"no array or string given"` |
|       - | 6336 | `			);` |
|       - | 6337 | `	}` |
|       - | 6338 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 6339 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6340 | `	/* Assume a NULL initial value */` |
|      15 | 6341 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      15 | 6342 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      15 | 6343 | `	if( nArg > 2 ){` |
|       - | 6344 | `		/* Set the initial value */` |
|      11 | 6345 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6346 | `	}` |
|       - | 6347 | `	/* Perform the requested operation */` |
|      15 | 6348 | `	pEntry = pMap->pFirst;` |
|      43 | 6349 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6350 | `		/* Extract the node value */` |
|      29 | 6351 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6352 | `		/* Invoke the supplied callback */` |
|      29 | 6353 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6354 | `		/* Point to the next entry */` |
|      29 | 6355 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6356 | `	}` |
|      15 | 6357 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6358 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6359 | `	return PH7_OK;` |
|      17 | 6360 |  |
|       - | 6361 | `/*` |
|       - | 6362 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6363 | ` *  Apply a user function to every member of an array.` |
|       - | 6364 | ` * Parameters` |
|       - | 6365 | ` *  $array` |
|       - | 6366 | ` *   The input array.` |
|       - | 6367 | ` *  $funcname` |
|       - | 6368 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6369 | ` *   the first, and the key/index second.` |
|       - | 6370 | ` * Note:` |
|       - | 6371 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6372 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6373 | ` *  be made in the original array itself.` |
|       - | 6374 | ` *  $userdata` |
|       - | 6375 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6376 | ` *   to the callback funcname.` |
|       - | 6377 | ` * Return` |
|       - | 6378 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6379 | ` */` |
|      36 | 6380 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6381 |  |
|       - | 6382 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6383 | `	ph7_hashmap_node *pEntry;` |
|       - | 6384 | `	ph7_hashmap *pMap;` |
|       - | 6385 | `	sxu32 n;` |
|      38 | 6386 | `	if( nArg < 2 ){` |
|       7 | 6387 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6388 | `			"ArgumentCountError",` |
|       - | 6389 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6390 | `			nArg` |
|       - | 6391 | `			);` |
|       - | 6392 | `	}` |
|      34 | 6393 | `	if( nArg > 3 ){` |
|       4 | 6394 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6395 | `			"ArgumentCountError",` |
|       - | 6396 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6397 | `			nArg` |
|       - | 6398 | `			);` |
|       - | 6399 | `	}` |
|      32 | 6400 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6401 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6402 | `			"TypeError",` |
|       - | 6403 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6404 | `			ph7_type_name(apArg[0])` |
|       - | 6405 | `			);` |
|       - | 6406 | `	}` |
|      30 | 6407 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6408 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6409 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6410 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6411 | `				"TypeError",` |
|       - | 6412 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6413 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6414 | `				zFunc` |
|       - | 6415 | `				);` |
|       - | 6416 | `		}` |
|       9 | 6417 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6418 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6419 | `				"TypeError",` |
|       - | 6420 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6421 | `				"array callback must have exactly two members"` |
|       - | 6422 | `				);` |
|       - | 6423 | `		}` |
|       5 | 6424 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6425 | `			"TypeError",` |
|       - | 6426 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6427 | `			"no array or string given"` |
|       - | 6428 | `			);` |
|       - | 6429 | `	}` |
|      19 | 6430 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6431 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6432 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      19 | 6433 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6434 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6435 | `	/* Perform the desired operation */` |
|      19 | 6436 | `	pEntry = pMap->pFirst;` |
|      59 | 6437 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6438 | `		/* Extract the node value */` |
|      41 | 6439 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      41 | 6440 | `		if( pValue ){` |
|       - | 6441 | `			/* Extract the entry key */` |
|      41 | 6442 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6443 | `			/* Invoke the supplied callback */` |
|      41 | 6444 | `			PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      41 | 6445 | `			PH7_MemObjRelease(&sKey);` |
|      20 | 6446 | `		}` |
|       - | 6447 | `		/* Point to the next entry */` |
|      41 | 6448 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6449 | `	}` |
|       - | 6450 | `	/* All done, return TRUE */` |
|      19 | 6451 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6452 | `	return PH7_OK;` |
|      20 | 6453 |  |
|       - | 6454 | `/*` |
|       - | 6455 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6456 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6457 | ` */` |
|      22 | 6458 | `static void HashmapWalkRecursive(` |
|       - | 6459 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6460 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6461 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6462 | `	int iNest             /* Nesting level */` |
|       - | 6463 | `	)` |
|       1 | 6464 |  |
|       - | 6465 | `	ph7_hashmap_node *pEntry;` |
|       - | 6466 | `	ph7_value *pValue,sKey;` |
|       - | 6467 | `	sxu32 n;` |
|       - | 6468 | `	/* Iterate through hashmap entries */` |
|      23 | 6469 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6470 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6471 | `	pEntry = pMap->pFirst;` |
|      59 | 6472 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6473 | `		/* Extract the node value */` |
|      37 | 6474 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6475 | `		if( pValue ){` |
|      37 | 6476 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6477 | `				if( iNest < 32 ){` |
|       - | 6478 | `					/* Recurse */` |
|      11 | 6479 | `					iNest++;` |
|      11 | 6480 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6481 | `					iNest--;` |
|       5 | 6482 | `				}` |
|       6 | 6483 | `			}else{` |
|       - | 6484 | `				/* Extract the node key */` |
|      27 | 6485 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6486 | `				/* Invoke the supplied callback */` |
|      27 | 6487 | `				PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6488 | `				PH7_MemObjRelease(&sKey);` |
|       - | 6489 | `			}` |
|      18 | 6490 | `		}` |
|       - | 6491 | `		/* Point to the next entry */` |
|      37 | 6492 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6493 | `	}` |
|      23 | 6494 |  |
|       - | 6495 | `/*` |
|       - | 6496 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6497 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6498 | ` * Parameters` |
|       - | 6499 | ` *  $array` |
|       - | 6500 | ` *   The input array.` |
|       - | 6501 | ` *  $funcname` |
|       - | 6502 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6503 | ` *   the first, and the key/index second.` |
|       - | 6504 | ` * Note:` |
|       - | 6505 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6506 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6507 | ` *  be made in the original array itself.` |
|       - | 6508 | ` *  $userdata` |
|       - | 6509 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6510 | ` *   to the callback funcname.` |
|       - | 6511 | ` * Return` |
|       - | 6512 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6513 | ` */` |
|      30 | 6514 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6515 |  |
|       - | 6516 | `	ph7_hashmap *pMap;` |
|      32 | 6517 | `	if( nArg < 2 ){` |
|       7 | 6518 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6519 | `			"ArgumentCountError",` |
|       - | 6520 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 6521 | `			nArg` |
|       - | 6522 | `			);` |
|       - | 6523 | `	}` |
|      28 | 6524 | `	if( nArg > 3 ){` |
|       4 | 6525 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6526 | `			"ArgumentCountError",` |
|       - | 6527 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 6528 | `			nArg` |
|       - | 6529 | `			);` |
|       - | 6530 | `	}` |
|      26 | 6531 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6532 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6533 | `			"TypeError",` |
|       - | 6534 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6535 | `			ph7_type_name(apArg[0])` |
|       - | 6536 | `			);` |
|       - | 6537 | `	}` |
|      24 | 6538 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6539 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6540 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6541 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6542 | `				"TypeError",` |
|       - | 6543 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6544 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6545 | `				zFunc` |
|       - | 6546 | `				);` |
|       - | 6547 | `		}` |
|       9 | 6548 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6549 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6550 | `				"TypeError",` |
|       - | 6551 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6552 | `				"array callback must have exactly two members"` |
|       - | 6553 | `				);` |
|       - | 6554 | `		}` |
|       5 | 6555 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6556 | `			"TypeError",` |
|       - | 6557 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6558 | `			"no array or string given"` |
|       - | 6559 | `			);` |
|       - | 6560 | `	}` |
|       - | 6561 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6562 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6563 | `	/* Perform the desired operation */` |
|      13 | 6564 | `	HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6565 | `	/* All done, return TRUE */` |
|      13 | 6566 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6567 | `	return PH7_OK;` |
|      17 | 6568 |  |
|       - | 6569 | `/*` |
|       - | 6570 | ` * Table of hashmap functions.` |
|       - | 6571 | ` */` |
|       - | 6572 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6573 | `	{"count",             ph7_hashmap_count },` |
|       - | 6574 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6575 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6576 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6577 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6578 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6579 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6580 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6581 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6582 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6583 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6584 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6585 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6586 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6587 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6588 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6589 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6590 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6591 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6592 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6593 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6594 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6595 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6596 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6597 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6598 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6599 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6600 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6601 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6602 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6603 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6604 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6605 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6606 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6607 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6608 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6609 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6610 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6611 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6612 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6613 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6614 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6615 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6616 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6617 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6618 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6619 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6620 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6621 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6622 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6623 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6624 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6625 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6626 | `	{"current",           ph7_hashmap_current },` |
|       - | 6627 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6628 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6629 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6630 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6631 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6632 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6633 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6634 | `};` |
|       - | 6635 | `/*` |
|       - | 6636 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6637 | ` */` |
|    1710 | 6638 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6639 |  |
|       - | 6640 | `	sxu32 n;` |
|  106022 | 6641 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  104312 | 6642 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   52157 | 6643 | `	}` |
|    1712 | 6644 |  |
|       - | 6645 | `/*` |
|       - | 6646 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6647 | ` * the BLOB given as the first argument.` |
|       - | 6648 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6649 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6650 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6651 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6652 | ` */` |
|      26 | 6653 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6654 |  |
|       - | 6655 | `	ph7_hashmap_node *pEntry;` |
|       - | 6656 | `	ph7_value *pObj;` |
|      28 | 6657 | `	sxu32 n = 0;` |
|       - | 6658 | `	int isRef;` |
|       - | 6659 | `	sxi32 rc;` |
|       - | 6660 | `	int i;` |
|      28 | 6661 | `	if( nDepth > 31 ){` |
|       - | 6662 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6663 | `		/* Nesting limit reached */` |
|     ! 0 | 6664 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6665 | `		if( ShowType ){` |
|     ! 0 | 6666 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6667 | `		}` |
|     ! 0 | 6668 | `		return SXERR_LIMIT;` |
|       - | 6669 | `	}` |
|       - | 6670 | `	/* Point to the first inserted entry */` |
|      28 | 6671 | `	pEntry = pMap->pFirst;` |
|      28 | 6672 | `	rc = SXRET_OK;` |
|      28 | 6673 | `	if( !ShowType ){` |
|      15 | 6674 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6675 | `	}` |
|       - | 6676 | `	/* Total entries */` |
|      28 | 6677 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6678 | `#ifdef __WINNT__` |
|       2 | 6679 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6680 | `#else` |
|      26 | 6681 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6682 | `#endif` |
|      62 | 6683 | `	for(;;){` |
|     126 | 6684 | `		if( n >= pMap->nEntry ){` |
|      28 | 6685 | `			break;` |
|       - | 6686 | `		}` |
|     198 | 6687 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 6688 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 6689 | `		}` |
|       - | 6690 | `		/* Dump key */` |
|     100 | 6691 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 6692 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 6693 | `		}else{` |
|     101 | 6694 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6695 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6696 | `		}` |
|       - | 6697 | `#ifdef __WINNT__` |
|       2 | 6698 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6699 | `#else` |
|      98 | 6700 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6701 | `#endif` |
|       - | 6702 | `		/* Dump node value */` |
|     100 | 6703 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 6704 | `		isRef = 0;` |
|     100 | 6705 | `		if( pObj ){` |
|     100 | 6706 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6707 | `				/* Referenced object */` |
|     ! 0 | 6708 | `				isRef = 1;` |
|     ! 0 | 6709 | `			}` |
|     100 | 6710 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 6711 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6712 | `				break;` |
|       - | 6713 | `			}` |
|      49 | 6714 | `		}` |
|       - | 6715 | `		/* Point to the next entry */` |
|     100 | 6716 | `		n++;` |
|     100 | 6717 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6718 | `	}` |
|      54 | 6719 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 6720 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 6721 | `	}` |
|      28 | 6722 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 6723 | `	return rc;` |
|      15 | 6724 |  |
|       - | 6725 | `/*` |
|       - | 6726 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6727 | ` * retrieved entry.` |
|       - | 6728 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6729 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6730 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6731 | ` * a value different from PH7_OK.` |
|       - | 6732 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6733 | ` */` |
|   21610 | 6734 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6735 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6736 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6737 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6738 | `	)` |
|       2 | 6739 |  |
|       - | 6740 | `	ph7_hashmap_node *pEntry;` |
|       - | 6741 | `	ph7_value sKey,sValue;` |
|       - | 6742 | `	sxi32 rc;` |
|       - | 6743 | `	sxu32 n;` |
|       - | 6744 | `	/* Initialize walker parameter */` |
|   21612 | 6745 | `	rc = SXRET_OK;` |
|   21612 | 6746 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   21612 | 6747 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   21612 | 6748 | `	n = pMap->nEntry;` |
|   21612 | 6749 | `	pEntry = pMap->pFirst;` |
|       - | 6750 | `	/* Start the iteration process */` |
|   55320 | 6751 | `	for(;;){` |
|  110642 | 6752 | `		if( n < 1 ){` |
|   21612 | 6753 | `			break;` |
|       - | 6754 | `		}` |
|       - | 6755 | `		/* Extract a copy of the key and a copy the current value */` |
|   89032 | 6756 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   89032 | 6757 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6758 | `		/* Invoke the user callback */` |
|   89032 | 6759 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6760 | `		/* Release the copy of the key and the value */` |
|   89032 | 6761 | `		PH7_MemObjRelease(&sKey);` |
|   89032 | 6762 | `		PH7_MemObjRelease(&sValue);` |
|   89032 | 6763 | `		if( rc != PH7_OK ){` |
|       - | 6764 | `			/* Callback request an operation abort */` |
|     ! 0 | 6765 | `			return SXERR_ABORT;` |
|       - | 6766 | `		}` |
|       - | 6767 | `		/* Point to the next entry */` |
|   89032 | 6768 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   89032 | 6769 | `		n--;` |
|       2 | 6770 | `	}` |
|       - | 6771 | `	/* All done */` |
|   21612 | 6772 | `	return SXRET_OK;` |
|   10807 | 6773 |  |
|       - | 6774 |  |
