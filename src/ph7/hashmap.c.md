# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3232/3715 lines (87.00%)

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
| 3039780 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 3039782 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  317022 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  317024 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  317024 |   29 | `	sxu32 nH = 5381;` |
|  317024 |   30 | `	zEnd = &zIn[nLen];` |
|  352541 |   31 | `	for(;;){` |
|  705084 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  616618 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  553262 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  457894 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  317024 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     912 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       2 |   47 |  |
|     914 |   48 | `	sxi64 iCount = 0;` |
|     914 |   49 | `	if( !bRecursive ){` |
|     740 |   50 | `		iCount = pMap->nEntry;` |
|     371 |   51 | `	}else{` |
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
|     914 |   86 | `	return iCount;` |
|       2 |   87 |  |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 2980426 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 2980428 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2980428 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 2980428 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 2980428 |  104 | `	pNode->pMap  = &(*pMap);` |
| 2980428 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2980428 |  106 | `	pNode->nHash = nHash;` |
| 2980428 |  107 | `	pNode->xKey.iKey = iKey;` |
| 2980428 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 2980428 |  109 | `	return pNode;` |
| 1490215 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|  108982 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|  108984 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  108984 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|  108984 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|  108984 |  127 | `	pNode->pMap  = &(*pMap);` |
|  108984 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  108984 |  129 | `	pNode->nHash = nHash;` |
|  108984 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  108984 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  108984 |  132 | `	pNode->nValIdx = nValIdx;` |
|  108984 |  133 | `	return pNode;` |
|   54493 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 3089408 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  139 |  |
|       - |  140 | `	/* Link */` |
| 3089410 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2782786 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2782786 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1391392 |  144 | `	}` |
| 3089410 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 3089410 |  147 | `	if( pMap->pFirst == 0 ){` |
|   55086 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   55086 |  150 | `		pMap->pCur = pNode;` |
|   27544 |  151 | `	}else{` |
| 3034326 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 3089410 |  154 | `	++pMap->nEntry;` |
| 3089410 |  155 |  |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    7142 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  161 |  |
|    7144 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    7144 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    7144 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    6684 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3343 |  167 | `	}else{` |
|     461 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    7144 |  170 | `	if( pNode->pNextCollide ){` |
|    5575 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2787 |  172 | `	}` |
|    7144 |  173 | `	if( pMap->pFirst == pNode ){` |
|     126 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      62 |  175 | `	}` |
|    7144 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|     128 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      63 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    7144 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    7144 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     104 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     104 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     104 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  188 | `		}` |
|      51 |  189 | `	}` |
|    7144 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    7010 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3504 |  192 | `	}` |
|    7144 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    7144 |  194 | `	pMap->nEntry--;` |
|    7144 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      72 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      72 |  198 | `		pMap->apBucket = 0;` |
|      72 |  199 | `		pMap->nSize = 0;` |
|      72 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      35 |  201 | `	}` |
|    7144 |  202 |  |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 3089408 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  208 |  |
| 3089410 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   59450 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   59450 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   59450 |  215 | `		if( nNew < 1 ){` |
|   55086 |  216 | `			nNew = 16;` |
|   27542 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   59450 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   59450 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   59450 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   59450 |  230 | `		pMap->apBucket = apNew;` |
|   59450 |  231 | `		pMap->nSize = nNew;` |
|   59450 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   55086 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    4366 |  237 | `		pEntry = pMap->pFirst;` |
|    4366 |  238 | `		n = 0;` |
| 2041030 |  239 | `		for( ;; ){` |
| 4082062 |  240 | `			if( n >= pMap->nEntry ){` |
|    4366 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 4077698 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 4077698 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4077698 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3523884 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3523884 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1761941 |  250 | `			}` |
| 4077698 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 4077698 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4077698 |  254 | `			n++;` |
|       2 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    4366 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2182 |  258 | `	}` |
| 3034326 |  259 | `	return SXRET_OK;` |
| 1544706 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 2980426 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 2980428 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		/* Reserve a ph7_value for the value */` |
| 2980394 |  274 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2980394 |  275 | `		if( pObj == 0 ){` |
|     ! 0 |  276 | `			return SXERR_MEM;` |
|       - |  277 | `		}` |
| 2980394 |  278 | `		if( pValue ){` |
|       - |  279 | `			/* Duplicate the value */` |
| 2980394 |  280 | `			PH7_MemObjStore(pValue,pObj);` |
| 1490196 |  281 | `		}` |
| 2980394 |  282 | `		nIdx = pObj->nIdx;` |
| 1490198 |  283 | `	}else{` |
|      35 |  284 | `		nIdx = nRefIdx;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Hash the key */` |
| 2980428 |  287 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  288 | `	/* Allocate a new int node */` |
| 2980428 |  289 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2980428 |  290 | `	if( pNode == 0 ){` |
|     ! 0 |  291 | `		return SXERR_MEM;` |
|       - |  292 | `	}` |
| 2980428 |  293 | `	if( isForeign ){` |
|       - |  294 | `		/* Mark as a foregin entry */` |
|      35 |  295 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      17 |  296 | `	}` |
|       - |  297 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2980428 |  298 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2980428 |  299 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  300 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  301 | `		return rc;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Perform the insertion */` |
| 2980428 |  304 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  305 | `	/* Install in the reference table */` |
| 2980428 |  306 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  307 | `	/* All done */` |
| 2980428 |  308 | `	return SXRET_OK;` |
| 1490215 |  309 |  |
|       - |  310 | `/*` |
|       - |  311 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  312 | ` * hashmap.` |
|       - |  313 | ` */` |
|  108982 |  314 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  315 |  |
|       - |  316 | `	ph7_hashmap_node *pNode;` |
|       - |  317 | `	sxu32 nHash;` |
|       - |  318 | `	sxu32 nIdx;` |
|       - |  319 | `	sxi32 rc;` |
|  108984 |  320 | `	if( !isForeign ){` |
|       - |  321 | `		ph7_value *pObj;` |
|       - |  322 | `		/* Reserve a ph7_value for the value */` |
|   73524 |  323 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   73524 |  324 | `		if( pObj == 0 ){` |
|     ! 0 |  325 | `			return SXERR_MEM;` |
|       - |  326 | `		}` |
|   73524 |  327 | `		if( pValue ){` |
|       - |  328 | `			/* Duplicate the value */` |
|   73252 |  329 | `			PH7_MemObjStore(pValue,pObj);` |
|   36625 |  330 | `		}` |
|   73524 |  331 | `		nIdx = pObj->nIdx;` |
|   36763 |  332 | `	}else{` |
|   35462 |  333 | `		nIdx = nRefIdx;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Hash the key */` |
|  108984 |  336 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  337 | `	/* Allocate a new blob node */` |
|  108984 |  338 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  108984 |  339 | `	if( pNode == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|  108984 |  342 | `	if( isForeign ){` |
|       - |  343 | `		/* Mark as a foregin entry */` |
|   35462 |  344 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   17730 |  345 | `	}` |
|       - |  346 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  108984 |  347 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  108984 |  348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  349 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  350 | `		return rc;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Perform the insertion */` |
|  108984 |  353 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  354 | `	/* Install in the reference table */` |
|  108984 |  355 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  356 | `	/* All done */` |
|  108984 |  357 | `	return SXRET_OK;` |
|   54493 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  361 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  362 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  363 | ` */` |
|   47972 |  364 | `static sxi32 HashmapLookupIntKey(` |
|       - |  365 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  366 | `	sxi64 iKey,                /* lookup key */` |
|       - |  367 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  368 | `	)` |
|       2 |  369 |  |
|       - |  370 | `	ph7_hashmap_node *pNode;` |
|       - |  371 | `	sxu32 nHash;` |
|   47974 |  372 | `	if( pMap->nEntry < 1 ){` |
|       - |  373 | `		/* Don't bother hashing,there is no entry anyway */` |
|     472 |  374 | `		return SXERR_NOTFOUND;` |
|       - |  375 | `	}` |
|       - |  376 | `	/* Hash the key first */` |
|   47504 |  377 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  378 | `	/* Point to the appropriate bucket */` |
|   47504 |  379 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  380 | `	/* Perform the lookup */` |
|  412106 |  381 | `	for(;;){` |
|  824214 |  382 | `		if( pNode == 0 ){` |
|   46076 |  383 | `			break;` |
|       - |  384 | `		}` |
|  778852 |  385 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  775123 |  386 | `			&& pNode->nHash == nHash` |
|  386770 |  387 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  388 | `				/* Node found */` |
|    1430 |  389 | `				if( ppNode ){` |
|    1418 |  390 | `					*ppNode = pNode;` |
|     708 |  391 | `				}` |
|    1430 |  392 | `				return SXRET_OK;` |
|       - |  393 | `		}` |
|       - |  394 | `		/* Follow the collision link */` |
|  776711 |  395 | `		pNode = pNode->pNextCollide;` |
|       1 |  396 | `	}` |
|       - |  397 | `	/* No such entry */` |
|   46076 |  398 | `	return SXERR_NOTFOUND;` |
|   23988 |  399 |  |
|       - |  400 | `/*` |
|       - |  401 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  402 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  403 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  404 | ` */` |
|  221660 |  405 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  406 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  407 | `	const void *pKey,           /* Lookup key */` |
|       - |  408 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  409 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  410 | `	)` |
|       2 |  411 |  |
|       - |  412 | `	ph7_hashmap_node *pNode;` |
|       - |  413 | `	sxu32 nHash;` |
|  221662 |  414 | `	if( pMap->nEntry < 1 ){` |
|       - |  415 | `		/* Don't bother hashing,there is no entry anyway */` |
|   13622 |  416 | `		return SXERR_NOTFOUND;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Hash the key first */` |
|  208042 |  419 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  420 | `	/* Point to the appropriate bucket */` |
|  208042 |  421 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  422 | `	/* Perform the lookup */` |
|  187418 |  423 | `	for(;;){` |
|  374838 |  424 | `		if( pNode == 0 ){` |
|  158840 |  425 | `			break;` |
|       - |  426 | `		}` |
|  240599 |  427 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  214499 |  428 | `			&& pNode->nHash == nHash` |
|  131101 |  429 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   49204 |  430 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  431 | `				/* Node found */` |
|   49204 |  432 | `				if( ppNode ){` |
|   49176 |  433 | `					*ppNode = pNode;` |
|   24587 |  434 | `				}` |
|   49204 |  435 | `				return SXRET_OK;` |
|       - |  436 | `		}` |
|       - |  437 | `		/* Follow the collision link */` |
|  166798 |  438 | `		pNode = pNode->pNextCollide;` |
|       2 |  439 | `	}` |
|       - |  440 | `	/* No such entry */` |
|  158840 |  441 | `	return SXERR_NOTFOUND;` |
|  110832 |  442 |  |
|       - |  443 | `/*` |
|       - |  444 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  445 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  446 | ` */` |
|  221800 |  447 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  448 |  |
|  221802 |  449 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  221802 |  450 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  221802 |  451 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  452 | `		/* Octal not decimal number */` |
|       5 |  453 | `		return FALSE;` |
|       - |  454 | `	}` |
|  221798 |  455 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  456 | `		zIn++;` |
|     ! 0 |  457 | `	}` |
|  111231 |  458 | `	for(;;){` |
|  222464 |  459 | `		if( zIn >= zEnd ){` |
|     233 |  460 | `			return TRUE;` |
|       - |  461 | `		}` |
|  222232 |  462 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  110784 |  463 | `			break;` |
|       - |  464 | `		}` |
|     667 |  465 | `		zIn++;` |
|       1 |  466 | `	}` |
|       - |  467 | `	/* Key does not look like a decimal number */` |
|  221566 |  468 | `	return FALSE;` |
|  110902 |  469 |  |
|       - |  470 | `/*` |
|       - |  471 | ` * Check if a given key exists in the given hashmap.` |
|       - |  472 | ` * Write a pointer to the target node on success.` |
|       - |  473 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  474 | ` */` |
|  113906 |  475 | `static sxi32 HashmapLookup(` |
|       - |  476 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  477 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  478 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  479 | `	)` |
|       2 |  480 |  |
|  113908 |  481 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  482 | `	sxi32 rc;` |
|  113908 |  483 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  112618 |  484 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  485 | `			/* Force a string cast */` |
|     ! 0 |  486 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  487 | `		}` |
|  112618 |  488 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  489 | `			/* Perform a blob lookup */` |
|  112602 |  490 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  112602 |  491 | `			goto result;` |
|       - |  492 | `		}` |
|       8 |  493 | `	}` |
|       - |  494 | `	/* Perform an int lookup */` |
|    1308 |  495 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  496 | `		/* Force an integer cast */` |
|      27 |  497 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  498 | `	}` |
|       - |  499 | `	/* Perform an int lookup */` |
|    1308 |  500 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   56953 |  501 | `result:` |
|  113908 |  502 | `	if( rc == SXRET_OK ){` |
|       - |  503 | `		/* Node found */` |
|   50362 |  504 | `		if( ppNode ){` |
|   50320 |  505 | `			*ppNode = pNode;` |
|   25159 |  506 | `		}` |
|   50362 |  507 | `		return SXRET_OK;` |
|       - |  508 | `	}` |
|       - |  509 | `	/* No such entry */` |
|   63548 |  510 | `	return SXERR_NOTFOUND;` |
|   56955 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  514 | ` * hashmap.` |
|       - |  515 | ` * If a node with the given key already exists in the database` |
|       - |  516 | ` * then this function overwrite the old value.` |
|       - |  517 | ` */` |
| 3053638 |  518 | `static sxi32 HashmapInsert(` |
|       - |  519 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  520 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  521 | `	ph7_value *pVal    /* Node value */` |
|       - |  522 | `	)` |
|       2 |  523 |  |
| 3053640 |  524 | `	ph7_hashmap_node *pNode = 0;` |
| 3053640 |  525 | `	sxi32 rc = SXRET_OK;` |
| 3053640 |  526 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   73758 |  527 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  528 | `			/* Force a string cast */` |
|       3 |  529 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  530 | `		}` |
|   73758 |  531 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  532 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  533 | `				/* Automatic index assign */` |
|      34 |  534 | `				pKey = 0;` |
|      16 |  535 | `			}` |
|     256 |  536 | `			goto IntKey;` |
|       - |  537 | `		}` |
|  110255 |  538 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   36751 |  539 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  540 | `				/* Overwrite the old value */` |
|       - |  541 | `				ph7_value *pElem;` |
|      78 |  542 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      78 |  543 | `				if( pElem ){` |
|      78 |  544 | `					if( pVal ){` |
|      78 |  545 | `						PH7_MemObjStore(pVal,pElem);` |
|      40 |  546 | `					}else{` |
|       - |  547 | `						/* Nullify the entry */` |
|     ! 0 |  548 | `						PH7_MemObjToNull(pElem);` |
|       - |  549 | `					}` |
|      38 |  550 | `				}` |
|      78 |  551 | `				return SXRET_OK;` |
|       - |  552 | `		}` |
|   73428 |  553 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  554 | `			/* Forbidden */` |
|       3 |  555 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Perform a blob-key insertion */` |
|   73426 |  559 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   73426 |  560 | `		return rc;` |
|       - |  561 | `	}` |
| 1489941 |  562 | `IntKey:` |
| 2980138 |  563 | `	if( pKey ){` |
|   23460 |  564 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  565 | `			/* Force an integer cast */` |
|     251 |  566 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  567 | `		}` |
|   23460 |  568 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  569 | `			/* Overwrite the old value */` |
|       - |  570 | `			ph7_value *pElem;` |
|      87 |  571 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      87 |  572 | `			if( pElem ){` |
|      87 |  573 | `				if( pVal ){` |
|      87 |  574 | `					PH7_MemObjStore(pVal,pElem);` |
|      44 |  575 | `				}else{` |
|       - |  576 | `					/* Nullify the entry */` |
|     ! 0 |  577 | `					PH7_MemObjToNull(pElem);` |
|       - |  578 | `				}` |
|      43 |  579 | `			}` |
|      87 |  580 | `			return SXRET_OK;` |
|       - |  581 | `		}` |
|   23374 |  582 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  583 | `			/* Forbidden */` |
|       3 |  584 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  585 | `			return SXRET_OK;` |
|       - |  586 | `		}` |
|       - |  587 | `		/* Perform a 64-bit-int-key insertion */` |
|   23372 |  588 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23372 |  589 | `		if( rc == SXRET_OK ){` |
|   23372 |  590 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  591 | `				/* Increment the automatic index */` |
|   23136 |  592 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  593 | `				/* Make sure the automatic index is not reserved */` |
|   23136 |  594 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  595 | `					pMap->iNextIdx++;` |
|     ! 0 |  596 | `				}` |
|   11567 |  597 | `			}` |
|   11685 |  598 | `		}` |
|   11687 |  599 | `	}else{` |
| 2956680 |  600 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  601 | `			/* Forbidden */` |
|       3 |  602 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|       - |  605 | `		/* Assign an automatic index */` |
| 2956678 |  606 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2956678 |  607 | `		if( rc == SXRET_OK ){` |
| 2956678 |  608 | `			++pMap->iNextIdx;` |
| 1478338 |  609 | `		}` |
|       - |  610 | `	}` |
|       - |  611 | `	/* Insertion result */` |
| 2980048 |  612 | `	return rc;` |
| 1526821 |  613 |  |
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
|   35500 |  641 | `static sxi32 HashmapInsertByRef(` |
|       - |  642 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  643 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  644 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  645 | `	)` |
|       2 |  646 |  |
|   35502 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|   35502 |  648 | `	sxi32 rc = SXRET_OK;` |
|   35502 |  649 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   35468 |  650 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  651 | `			/* Force a string cast */` |
|     ! 0 |  652 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  653 | `		}` |
|   35468 |  654 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  655 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  656 | `				/* Automatic index assign */` |
|     ! 0 |  657 | `				pKey = 0;` |
|     ! 0 |  658 | `			}` |
|     ! 0 |  659 | `			goto IntKey;` |
|       - |  660 | `		}` |
|   53201 |  661 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   17733 |  662 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  663 | `				/* Overwrite */` |
|       7 |  664 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  665 | `				pNode->nValIdx = nRefIdx;` |
|       - |  666 | `				/* Install in the reference table */` |
|       7 |  667 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  668 | `				return SXRET_OK;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Perform a blob-key insertion */` |
|   35462 |  671 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   35462 |  672 | `		return rc;` |
|       - |  673 | `	}` |
|      17 |  674 | `IntKey:` |
|      35 |  675 | `	if( pKey ){` |
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
|      33 |  702 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|      33 |  703 | `		if( rc == SXRET_OK ){` |
|      33 |  704 | `			++pMap->iNextIdx;` |
|      16 |  705 | `		}` |
|       - |  706 | `	}` |
|       - |  707 | `	/* Insertion result */` |
|      35 |  708 | `	return rc;` |
|   17752 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * Extract node value.` |
|       - |  712 | ` */` |
| 1215865 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
| 1215867 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1215867 |  718 | `	return pObj;` |
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
|   60834 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   60836 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   60836 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   60836 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   60836 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   60836 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   60836 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   60836 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   60836 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   60836 |  783 | `	return rc;` |
|   30410 |  784 |  |
|       - |  785 | `/*` |
|       - |  786 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  787 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  788 | ` */` |
|   11852 |  789 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  790 |  |
|   11854 |  791 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  792 | `	sxu32 nBucket;` |
|       - |  793 | `	/* Remove old collision links */` |
|   11854 |  794 | `	if( pEntry->pPrevCollide ){` |
|    9606 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    4791 |  796 | `	}else{` |
|    2250 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  798 | `	}` |
|   11854 |  799 | `	if( pEntry->pNextCollide ){` |
|     906 |  800 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     446 |  801 | `	}` |
|   11854 |  802 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  803 | `	/* Compute the new hash */` |
|   11854 |  804 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   11854 |  805 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   11854 |  806 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  807 | `	/* Link to the new bucket */` |
|   11854 |  808 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11854 |  809 | `	if( pMap->apBucket[nBucket] ){` |
|    9854 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    4915 |  811 | `	}` |
|   11854 |  812 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11854 |  813 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  814 | `	/* Increment the automatic index */` |
|   11854 |  815 | `	pMap->iNextIdx++;` |
|   11854 |  816 |  |
|       - |  817 | `/*` |
|       - |  818 | ` * Perform a linear search on a given hashmap.` |
|       - |  819 | ` * Write a pointer to the target node on success.` |
|       - |  820 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  821 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  822 | ` * for more information.` |
|       - |  823 | ` */` |
|   29660 |  824 | `static int HashmapFindValue(` |
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
|   29662 |  837 | `	pEntry = pMap->pFirst;` |
|   29662 |  838 | `	n = pMap->nEntry;` |
|   29662 |  839 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   29662 |  840 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   71030 |  841 | `	for(;;){` |
|  142063 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  141965 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  141965 |  847 | `		if( pVal ){` |
|  141965 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  141965 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  141965 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  141965 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  141965 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  141965 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  141965 |  865 | `				if( rc == 0 ){` |
|   29564 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   29564 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   56200 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|  112403 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  112403 |  876 | `		n--;` |
|       2 |  877 | `	}` |
|       - |  878 | `	/* No such entry */` |
|      99 |  879 | `	return SXERR_NOTFOUND;` |
|   14832 |  880 |  |
|       - |  881 | `/*` |
|       - |  882 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  883 | ` * for values comparison.` |
|       - |  884 | ` * Write a pointer to the target node on success.` |
|       - |  885 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  886 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  887 | ` * for more information.` |
|       - |  888 | ` */` |
|      22 |  889 | `static int HashmapFindValueByCallback(` |
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
|      23 |  901 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|       - |  902 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|       - |  903 | `		 * exception is not thrown again, and let the caller wind down. */` |
|     ! 0 |  904 | `		return SXERR_NOTFOUND;` |
|       - |  905 | `	}` |
|       - |  906 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|      23 |  907 | `	pEntry = pMap->pFirst;` |
|      23 |  908 | `	n = pMap->nEntry;` |
|       - |  909 | `	/* Store callback result here */` |
|      23 |  910 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  911 | `	/* First argument to the callback */` |
|      23 |  912 | `	apArg[0] = pNeedle;` |
|      25 |  913 | `	for(;;){` |
|      51 |  914 | `		if( n < 1 ){` |
|       9 |  915 | `			break;` |
|       - |  916 | `		}` |
|       - |  917 | `		/* Extract node value */` |
|      43 |  918 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      43 |  919 | `		if( pVal ){` |
|       - |  920 | `			/* Invoke the user callback */` |
|      43 |  921 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      43 |  922 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      43 |  923 | `			if( rc == PH7_EXCEPTION ){` |
|       - |  924 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|       - |  925 | `				 * and report no match for the rest of the run. */` |
|       5 |  926 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|       5 |  927 | `				PH7_MemObjRelease(&sResult);` |
|       5 |  928 | `				return SXERR_NOTFOUND;` |
|       - |  929 | `			}` |
|      39 |  930 | `			if( rc == SXRET_OK ){` |
|       - |  931 | `				/* Extract callback result */` |
|      39 |  932 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  933 | `					/* Perform an int cast */` |
|     ! 0 |  934 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  935 | `				}` |
|      39 |  936 | `				rc = (sxi32)sResult.x.iVal;` |
|      39 |  937 | `				PH7_MemObjRelease(&sResult);` |
|      39 |  938 | `				if( rc == 0 ){` |
|       - |  939 | `					/* Match found*/` |
|      11 |  940 | `					if( ppNode ){` |
|     ! 0 |  941 | `						*ppNode = pEntry;` |
|     ! 0 |  942 | `					}` |
|      11 |  943 | `					return SXRET_OK;` |
|       - |  944 | `				}` |
|      14 |  945 | `			}` |
|      14 |  946 | `		}` |
|       - |  947 | `		/* Point to the next entry */` |
|      29 |  948 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 |  949 | `		n--;` |
|       1 |  950 | `	}` |
|       - |  951 | `	/* No such entry */` |
|       9 |  952 | `	return SXERR_NOTFOUND;` |
|      12 |  953 |  |
|       - |  954 | `/*` |
|       - |  955 | ` * Compare two hashmaps.` |
|       - |  956 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|       - |  957 | ` * Note on array comparison operators.` |
|       - |  958 | ` *  According to the PHP language reference manual.` |
|       - |  959 | ` *  Array Operators Example 	Name 	Result` |
|       - |  960 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|       - |  961 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|       - |  962 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|       - |  963 | ` *                          order and of the same types.` |
|       - |  964 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - |  965 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - |  966 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|       - |  967 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - |  968 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - |  969 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - |  970 | ` * <?php` |
|       - |  971 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - |  972 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - |  973 | ` * $c = $a + $b; // Union of $a and $b` |
|       - |  974 | ` * echo "Union of \$a and \$b: \n";` |
|       - |  975 | ` * var_dump($c);` |
|       - |  976 | ` * $c = $b + $a; // Union of $b and $a` |
|       - |  977 | ` * echo "Union of \$b and \$a: \n";` |
|       - |  978 | ` * var_dump($c);` |
|       - |  979 | ` * ?>` |
|       - |  980 | ` * When executed, this script will print the following:` |
|       - |  981 | ` * Union of $a and $b:` |
|       - |  982 | ` * array(3) {` |
|       - |  983 | ` *  ["a"]=>` |
|       - |  984 | ` *  string(5) "apple"` |
|       - |  985 | ` *  ["b"]=>` |
|       - |  986 | ` * string(6) "banana"` |
|       - |  987 | ` *  ["c"]=>` |
|       - |  988 | ` * string(6) "cherry"` |
|       - |  989 | ` * }` |
|       - |  990 | ` * Union of $b and $a:` |
|       - |  991 | ` * array(3) {` |
|       - |  992 | ` * ["a"]=>` |
|       - |  993 | ` * string(4) "pear"` |
|       - |  994 | ` * ["b"]=>` |
|       - |  995 | ` * string(10) "strawberry"` |
|       - |  996 | ` * ["c"]=>` |
|       - |  997 | ` * string(6) "cherry"` |
|       - |  998 | ` * }` |
|       - |  999 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|       - | 1000 | ` */` |
|      18 | 1001 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - | 1002 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - | 1003 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - | 1004 | `	int bStrict          /* TRUE for strict comparison */` |
|       - | 1005 | `	)` |
|       1 | 1006 |  |
|       - | 1007 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - | 1008 | `	sxi32 rc;` |
|       - | 1009 | `	sxu32 n;` |
|      19 | 1010 | `	if( pLeft == pRight ){` |
|       - | 1011 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - | 1012 | `		 * Unlike the zend engine.` |
|       - | 1013 | `		 */` |
|     ! 0 | 1014 | `		return 0;` |
|       - | 1015 | `	}` |
|      19 | 1016 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - | 1017 | `		/* Must have the same number of entries */` |
|       5 | 1018 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1019 | `	}` |
|       - | 1020 | `	/* Point to the first inserted entry of the left hashmap */` |
|      15 | 1021 | `	pLe = pLeft->pFirst;` |
|      15 | 1022 | `	pRe = 0; /* cc warning */` |
|       - | 1023 | `	/* Perform the comparison */` |
|      15 | 1024 | `	n = pLeft->nEntry;` |
|      15 | 1025 | `	for(;;){` |
|      31 | 1026 | `		if( n < 1 ){` |
|      13 | 1027 | `			break;` |
|       - | 1028 | `		}` |
|      19 | 1029 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1030 | `			/* Int key */` |
|      13 | 1031 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|       7 | 1032 | `		}else{` |
|       7 | 1033 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1034 | `			/* Blob key */` |
|       7 | 1035 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1036 | `		}` |
|      19 | 1037 | `		if( rc != SXRET_OK ){` |
|       - | 1038 | `			/* No such entry in the right side */` |
|     ! 0 | 1039 | `			return 1;` |
|       - | 1040 | `		}` |
|      19 | 1041 | `		rc = 0;` |
|      19 | 1042 | `		if( bStrict ){` |
|       - | 1043 | `			/* Make sure,the keys are of the same type */` |
|       3 | 1044 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1045 | `				rc = 1;` |
|     ! 0 | 1046 | `			}` |
|       1 | 1047 | `		}` |
|      19 | 1048 | `		if( !rc ){` |
|       - | 1049 | `			/* Compare nodes */` |
|      19 | 1050 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|       9 | 1051 | `		}` |
|      19 | 1052 | `		if( rc != 0 ){` |
|       - | 1053 | `			/* Nodes key/value differ */` |
|       3 | 1054 | `			return rc;` |
|       - | 1055 | `		}` |
|       - | 1056 | `		/* Point to the next entry */` |
|      17 | 1057 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      17 | 1058 | `		n--;` |
|       1 | 1059 | `	}` |
|      13 | 1060 | `	return 0; /* Hashmaps are equals */` |
|      10 | 1061 |  |
|       - | 1062 | `/*` |
|       - | 1063 | ` * Duplicate a hashmap node.` |
|       - | 1064 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1065 | ` */` |
|  567280 | 1066 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1067 | `	ph7_hashmap *pDest,` |
|       - | 1068 | `	ph7_hashmap_node *pEntry,` |
|       - | 1069 | `	ph7_value *pVal,` |
|       - | 1070 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1071 | `	)` |
|       2 | 1072 |  |
|       - | 1073 | `	ph7_value sSafeVal;` |
|       - | 1074 | `	ph7_value sKey;` |
|       - | 1075 | `	sxi32 rc;` |
|       - | 1076 |  |
|  567282 | 1077 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 1078 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|       - | 1079 | `		 * Re-insert it by reference so the reference survives the duplication` |
|       - | 1080 | `		 * instead of being flattened to a value copy. This keeps spread` |
|       - | 1081 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|       - | 1082 | `		 * with PHP semantics. */` |
|       7 | 1083 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|       7 | 1084 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       3 | 1085 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|       3 | 1086 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|       3 | 1087 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       3 | 1088 | `			PH7_MemObjRelease(&sKey);` |
|       2 | 1089 | `		}else{` |
|       5 | 1090 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|       5 | 1091 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|       2 | 1092 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|     ! 0 | 1093 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|     ! 0 | 1094 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|     ! 0 | 1095 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 1096 | `			}else{ /* Dup: preserve the int key */` |
|     ! 0 | 1097 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|       - | 1098 | `			}` |
|       - | 1099 | `		}` |
|       7 | 1100 | `		return rc;` |
|       - | 1101 | `	}` |
|  567276 | 1102 | `	sSafeVal = *pVal;` |
|       - | 1103 |  |
|  567276 | 1104 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1105 | `		/* Blob key insertion */` |
|      95 | 1106 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      95 | 1107 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      95 | 1108 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      95 | 1109 | `		PH7_MemObjRelease(&sKey);` |
|      48 | 1110 | `	}else{` |
|       - | 1111 | `		/* Int key */` |
|  567182 | 1112 | `		if( iAction == 0 ){ /* Merge */` |
|  566960 | 1113 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  283703 | 1114 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1115 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1116 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1117 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1118 | `		}else{ /* Dup */` |
|     194 | 1119 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1120 | `		}` |
|       - | 1121 | `	}` |
|  567276 | 1122 | `	return rc;` |
|  283642 | 1123 |  |
|       - | 1124 | `/*` |
|       - | 1125 | ` * Merge two hashmaps.` |
|       - | 1126 | ` * Note on the merge process` |
|       - | 1127 | ` * According to the PHP language reference manual.` |
|       - | 1128 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|       - | 1129 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|       - | 1130 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|       - | 1131 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|       - | 1132 | ` *  the later value will not overwrite the original value, but will be appended.` |
|       - | 1133 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|       - | 1134 | ` *  keys starting from zero in the result array.` |
|       - | 1135 | ` */` |
|    2024 | 1136 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1137 |  |
|       - | 1138 | `	ph7_hashmap_node *pEntry;` |
|       - | 1139 | `	ph7_value *pVal;` |
|       - | 1140 | `	sxi32 rc;` |
|       - | 1141 | `	sxu32 n;` |
|    2026 | 1142 | `	if( pSrc == pDest ){` |
|       - | 1143 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1144 | `		 * Unlike the zend engine.` |
|       - | 1145 | `		 */` |
|     ! 0 | 1146 | `		return SXRET_OK;` |
|       - | 1147 | `	}` |
|       - | 1148 | `	/* Point to the first inserted entry in the source */` |
|    2026 | 1149 | `	pEntry = pSrc->pFirst;` |
|       - | 1150 | `	/* Perform the merge */` |
|  569042 | 1151 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1152 | `		/* Extract the node value */` |
|  567018 | 1153 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  567018 | 1154 | `		if( pVal ){` |
|       - | 1155 | `			/* Make a local copy of the value.` |
|       - | 1156 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1157 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1158 | `			 * to the old pool.` |
|       - | 1159 | `			 */` |
|  567018 | 1160 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  283510 | 1161 | `		}else{` |
|     ! 0 | 1162 | `			rc = SXRET_OK;` |
|       - | 1163 | `		}` |
|  567018 | 1164 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1165 | `			return rc;` |
|       - | 1166 | `		}` |
|       - | 1167 | `		/* Point to the next entry */` |
|  567018 | 1168 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  283510 | 1169 | `	}` |
|    2026 | 1170 | `	return SXRET_OK;` |
|    1014 | 1171 |  |
|       - | 1172 | `/*` |
|       - | 1173 | ` * Overwrite entries with the same key.` |
|       - | 1174 | ` * Refer to the [array_replace()] implementation for more information.` |
|       - | 1175 | ` *  According to the PHP language reference manual.` |
|       - | 1176 | ` *  array_replace() replaces the values of the first array with the same values` |
|       - | 1177 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|       - | 1178 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|       - | 1179 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|       - | 1180 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|       - | 1181 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|       - | 1182 | ` *  overwriting the previous values.` |
|       - | 1183 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|       - | 1184 | ` *  by whatever type is in the second array.` |
|       - | 1185 | ` */` |
|      34 | 1186 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1187 |  |
|       - | 1188 | `	ph7_hashmap_node *pEntry;` |
|       - | 1189 | `	ph7_value *pVal;` |
|       - | 1190 | `	sxi32 rc;` |
|       - | 1191 | `	sxu32 n;` |
|      36 | 1192 | `	if( pSrc == pDest ){` |
|       - | 1193 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1194 | `		 * Unlike the zend engine.` |
|       - | 1195 | `		 */` |
|     ! 0 | 1196 | `		return SXRET_OK;` |
|       - | 1197 | `	}` |
|       - | 1198 | `	/* Point to the first inserted entry in the source */` |
|      36 | 1199 | `	pEntry = pSrc->pFirst;` |
|       - | 1200 | `	/* Perform the merge */` |
|      80 | 1201 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1202 | `		/* Extract the node value */` |
|      46 | 1203 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      46 | 1204 | `		if( pVal ){` |
|      46 | 1205 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      24 | 1206 | `		}else{` |
|     ! 0 | 1207 | `			rc = SXRET_OK;` |
|       - | 1208 | `		}` |
|      46 | 1209 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1210 | `			return rc;` |
|       - | 1211 | `		}` |
|       - | 1212 | `		/* Point to the next entry */` |
|      46 | 1213 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 1214 | `	}` |
|      36 | 1215 | `	return SXRET_OK;` |
|      19 | 1216 |  |
|       - | 1217 | `/*` |
|       - | 1218 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|       - | 1219 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|       - | 1220 | ` */` |
|     104 | 1221 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1222 |  |
|       - | 1223 | `	ph7_hashmap_node *pEntry;` |
|       - | 1224 | `	ph7_value *pVal;` |
|       - | 1225 | `	sxi32 rc;` |
|       - | 1226 | `	sxu32 n;` |
|     106 | 1227 | `	if( pSrc == pDest ){` |
|       - | 1228 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1229 | `		 * Unlike the zend engine.` |
|       - | 1230 | `		 */` |
|     ! 0 | 1231 | `		return SXRET_OK;` |
|       - | 1232 | `	}` |
|       - | 1233 | `	/* Point to the first inserted entry in the source */` |
|     106 | 1234 | `	pEntry = pSrc->pFirst;` |
|       - | 1235 | `	/* Perform the duplication */` |
|     326 | 1236 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1237 | `		/* Extract the node value */` |
|     222 | 1238 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     222 | 1239 | `		if( pVal ){` |
|     222 | 1240 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|     112 | 1241 | `		}else{` |
|     ! 0 | 1242 | `			rc = SXRET_OK;` |
|       - | 1243 | `		}` |
|     222 | 1244 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1245 | `			return rc;` |
|       - | 1246 | `		}` |
|       - | 1247 | `		/* Point to the next entry */` |
|     222 | 1248 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     112 | 1249 | `	}` |
|     106 | 1250 | `	return SXRET_OK;` |
|      54 | 1251 |  |
|       - | 1252 | `/*` |
|       - | 1253 | ` * Copy-on-write separation for arrays.` |
|       - | 1254 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|       - | 1255 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|       - | 1256 | ` * Returns the (possibly new) hashmap pointer.` |
|       - | 1257 | ` */` |
|  196298 | 1258 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       2 | 1259 |  |
|  196300 | 1260 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1261 | `	ph7_hashmap *pNew;` |
|       - | 1262 | `	ph7_value *pBacking;` |
|  196300 | 1263 | `	if( pMap->iRef < 2 ){` |
|       - | 1264 | `		/* Sole owner, no separation needed */` |
|  194204 | 1265 | `		return pMap;` |
|       - | 1266 | `	}` |
|    2098 | 1267 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1268 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1269 | `		return pMap;` |
|       - | 1270 | `	}` |
|       - | 1271 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1272 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1273 | `	 * frame is popped. */` |
|    2098 | 1274 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2098 | 1275 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    3127 | 1276 | `		if( pBacking && pBacking != pValue` |
|    2079 | 1277 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2064 | 1278 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1279 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2064 | 1280 | `			pMap->iRef--;` |
|    2064 | 1281 | `			if( pMap->iRef < 2 ){` |
|       - | 1282 | `				/* After undoing stack ref, sole owner — no separation */` |
|    2028 | 1283 | `				pMap->iRef++;` |
|    2028 | 1284 | `				return pMap;` |
|       - | 1285 | `			}` |
|      38 | 1286 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|      38 | 1287 | `			if( pNew == 0 ){` |
|     ! 0 | 1288 | `				pMap->iRef++;` |
|     ! 0 | 1289 | `				return pMap;` |
|       - | 1290 | `			}` |
|      38 | 1291 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1292 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|     ! 0 | 1293 | `				PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1294 | `				pMap->iRef++;` |
|     ! 0 | 1295 | `				return pMap;` |
|       - | 1296 | `			}` |
|      38 | 1297 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|      38 | 1298 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|       - | 1299 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|       - | 1300 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|       - | 1301 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|       - | 1302 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|       - | 1303 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|       - | 1304 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|       - | 1305 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|      38 | 1306 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      38 | 1307 | `			if( pBacking ){` |
|      38 | 1308 | `				pBacking->x.pOther = pNew;` |
|      18 | 1309 | `			}` |
|       - | 1310 | `			/* Update the stack value to match */` |
|      38 | 1311 | `			pValue->x.pOther = pNew;` |
|      38 | 1312 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|      38 | 1313 | `			return pNew;` |
|       - | 1314 | `		}` |
|      17 | 1315 | `	}` |
|      35 | 1316 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      35 | 1317 | `	if( pNew == 0 ){` |
|       - | 1318 | `		/* Allocation failure — fall through with shared map */` |
|     ! 0 | 1319 | `		return pMap;` |
|       - | 1320 | `	}` |
|      35 | 1321 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1322 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|     ! 0 | 1323 | `		PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1324 | `		return pMap;` |
|       - | 1325 | `	}` |
|      35 | 1326 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      35 | 1327 | `	pMap->iRef--;` |
|      35 | 1328 | `	pValue->x.pOther = pNew;` |
|      35 | 1329 | `	return pNew;` |
|   98151 | 1330 |  |
|       - | 1331 | `/*` |
|       - | 1332 | ` * Perform the union of two hashmaps.` |
|       - | 1333 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1334 | ` * with a variable holding an array as follows:` |
|       - | 1335 | ` * <?php` |
|       - | 1336 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1337 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1338 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1339 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1340 | ` * var_dump($c);` |
|       - | 1341 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1342 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1343 | ` * var_dump($c);` |
|       - | 1344 | ` * ?>` |
|       - | 1345 | ` * When executed, this script will print the following:` |
|       - | 1346 | ` * Union of $a and $b:` |
|       - | 1347 | ` * array(3) {` |
|       - | 1348 | ` *  ["a"]=>` |
|       - | 1349 | ` *  string(5) "apple"` |
|       - | 1350 | ` *  ["b"]=>` |
|       - | 1351 | ` * string(6) "banana"` |
|       - | 1352 | ` *  ["c"]=>` |
|       - | 1353 | ` * string(6) "cherry"` |
|       - | 1354 | ` * }` |
|       - | 1355 | ` * Union of $b and $a:` |
|       - | 1356 | ` * array(3) {` |
|       - | 1357 | ` * ["a"]=>` |
|       - | 1358 | ` * string(4) "pear"` |
|       - | 1359 | ` * ["b"]=>` |
|       - | 1360 | ` * string(10) "strawberry"` |
|       - | 1361 | ` * ["c"]=>` |
|       - | 1362 | ` * string(6) "cherry"` |
|       - | 1363 | ` * }` |
|       - | 1364 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1365 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1366 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1367 | ` */` |
|      10 | 1368 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       2 | 1369 |  |
|       - | 1370 | `	ph7_hashmap_node *pEntry;` |
|      12 | 1371 | `	sxi32 rc = SXRET_OK;` |
|       - | 1372 | `	ph7_value *pObj;` |
|       - | 1373 | `	sxu32 n;` |
|      12 | 1374 | `	if( pLeft == pRight ){` |
|       - | 1375 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1376 | `		 * Unlike the zend engine.` |
|       - | 1377 | `		 */` |
|     ! 0 | 1378 | `		return SXRET_OK;` |
|       - | 1379 | `	}` |
|       - | 1380 | `	/* Perform the union */` |
|      12 | 1381 | `	pEntry = pRight->pFirst;` |
|      32 | 1382 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1383 | `		/* Make sure the given key does not exists in the left array */` |
|      22 | 1384 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1385 | `			/* BLOB key */` |
|       7 | 1386 | `			if( SXRET_OK !=` |
|       6 | 1387 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|       3 | 1388 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|       3 | 1389 | `					if( pObj ){` |
|       3 | 1390 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1391 | `						/* Perform the insertion */` |
|       3 | 1392 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1393 | `							&sSafeVal,0,FALSE);` |
|       3 | 1394 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1395 | `							return rc;` |
|       - | 1396 | `						}` |
|       1 | 1397 | `					}` |
|       1 | 1398 | `			}` |
|       4 | 1399 | `		}else{` |
|       - | 1400 | `			/* INT key */` |
|      16 | 1401 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|      11 | 1402 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|      11 | 1403 | `				if( pObj ){` |
|      11 | 1404 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1405 | `					/* Perform the insertion */` |
|      11 | 1406 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      11 | 1407 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1408 | `						return rc;` |
|       - | 1409 | `					}` |
|       5 | 1410 | `				}` |
|       5 | 1411 | `			}` |
|       - | 1412 | `		}` |
|       - | 1413 | `		/* Point to the next entry */` |
|      22 | 1414 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      12 | 1415 | `	}` |
|      12 | 1416 | `	return SXRET_OK;` |
|       7 | 1417 |  |
|       - | 1418 | `/*` |
|       - | 1419 | ` * Allocate a new hashmap.` |
|       - | 1420 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1421 | ` */` |
|   86638 | 1422 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1423 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1424 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1425 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1426 | `	)` |
|       2 | 1427 |  |
|       - | 1428 | `	ph7_hashmap *pMap;` |
|       - | 1429 | `	/* Allocate a new instance */` |
|   86640 | 1430 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   86640 | 1431 | `	if( pMap == 0 ){` |
|     ! 0 | 1432 | `		return 0;` |
|       - | 1433 | `	}` |
|       - | 1434 | `	/* Zero the structure */` |
|   86640 | 1435 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1436 | `	/* Fill in the structure */` |
|   86640 | 1437 | `	pMap->pVm = &(*pVm);` |
|   86640 | 1438 | `	pMap->iRef = 1;` |
|       - | 1439 | `	/* Default hash functions */` |
|   86640 | 1440 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   86640 | 1441 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   86640 | 1442 | `	return pMap;` |
|   43321 | 1443 |  |
|       - | 1444 | `/*` |
|       - | 1445 | ` * Install superglobals in the given virtual machine.` |
|       - | 1446 | ` * Note on superglobals.` |
|       - | 1447 | ` *  According to the PHP language reference manual.` |
|       - | 1448 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1449 | `*   Description` |
|       - | 1450 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1451 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1452 | `*   global $variable; to access them within functions or methods.` |
|       - | 1453 | `*   These superglobal variables are:` |
|       - | 1454 | `*    $GLOBALS` |
|       - | 1455 | `*    $_SERVER` |
|       - | 1456 | `*    $_GET` |
|       - | 1457 | `*    $_POST` |
|       - | 1458 | `*    $_FILES` |
|       - | 1459 | `*    $_COOKIE` |
|       - | 1460 | `*    $_SESSION` |
|       - | 1461 | `*    $_REQUEST` |
|       - | 1462 | `*    $_ENV` |
|       - | 1463 | `*/` |
|    2832 | 1464 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       2 | 1465 |  |
|       - | 1466 | `	static const char * azSuper[] = {` |
|       - | 1467 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1468 | `		"_GET",      /* $_GET */` |
|       - | 1469 | `		"_POST",     /* $_POST */` |
|       - | 1470 | `		"_FILES",    /* $_FILES */` |
|       - | 1471 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1472 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1473 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1474 | `		"_ENV",      /* $_ENV */` |
|       - | 1475 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1476 | `		"argv"       /* $argv */` |
|       - | 1477 | `	};` |
|       - | 1478 | `	ph7_hashmap *pMap;` |
|       - | 1479 | `	ph7_value *pObj;` |
|       - | 1480 | `	SyString *pFile;` |
|       - | 1481 | `	sxi32 rc;` |
|       - | 1482 | `	sxu32 n;` |
|       - | 1483 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    2834 | 1484 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    2834 | 1485 | `	if( pMap == 0 ){` |
|     ! 0 | 1486 | `		return SXERR_MEM;` |
|       - | 1487 | `	}` |
|    2834 | 1488 | `	pVm->pGlobal = pMap;` |
|       - | 1489 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    2834 | 1490 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    2834 | 1491 | `	if( pObj == 0 ){` |
|     ! 0 | 1492 | `		return SXERR_MEM;` |
|       - | 1493 | `	}` |
|    2834 | 1494 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1495 | `	/* Record object index */` |
|    2834 | 1496 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1497 | `	/* Install the special $GLOBALS array */` |
|    2834 | 1498 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    2834 | 1499 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1500 | `		return rc;` |
|       - | 1501 | `	}` |
|       - | 1502 | `	/* Install superglobals now */` |
|   31154 | 1503 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1504 | `		ph7_value *pSuper;` |
|       - | 1505 | `		/* Request an empty array */` |
|   28322 | 1506 | `		pSuper = ph7_new_array(&(*pVm));` |
|   28322 | 1507 | `		if( pSuper == 0 ){` |
|     ! 0 | 1508 | `			return SXERR_MEM;` |
|       - | 1509 | `		}` |
|       - | 1510 | `		/* Install */` |
|   28322 | 1511 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   28322 | 1512 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1513 | `			return rc;` |
|       - | 1514 | `		}` |
|       - | 1515 | `		/* Release the value now it have been installed */` |
|   28322 | 1516 | `		ph7_release_value(&(*pVm),pSuper);` |
|   14162 | 1517 | `	}` |
|       - | 1518 | `	/* Set some $_SERVER entries */` |
|    2834 | 1519 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1520 | `	/*` |
|       - | 1521 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1522 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1523 | `	 */` |
|    5662 | 1524 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1525 | `		"SCRIPT_FILENAME",` |
|    1416 | 1526 | `		pFile ? pFile->zString : ":Memory:",` |
|    2828 | 1527 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1528 | `		);` |
|       - | 1529 | `	/* All done,all super-global are installed now */` |
|    2834 | 1530 | `	return SXRET_OK;` |
|    1418 | 1531 |  |
|       - | 1532 | `/*` |
|       - | 1533 | ` * Release a hashmap.` |
|       - | 1534 | ` */` |
|   55316 | 1535 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1536 |  |
|       - | 1537 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   55318 | 1538 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1539 | `	sxu32 n;` |
|   55318 | 1540 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1541 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1542 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1543 | `		return SXRET_OK;` |
|       - | 1544 | `	}` |
|       - | 1545 | `	/* Start the release process */` |
|   55318 | 1546 | `	n = 0;` |
|   55318 | 1547 | `	pEntry = pMap->pFirst;` |
| 1549368 | 1548 | `	for(;;){` |
| 3098738 | 1549 | `		if( n >= pMap->nEntry ){` |
|   55318 | 1550 | `			break;` |
|       - | 1551 | `		}` |
| 3043422 | 1552 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1553 | `		/* Remove the reference from the foreign table */` |
| 3043422 | 1554 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3043422 | 1555 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1556 | `			/* Restore the ph7_value to the free list */` |
| 3043414 | 1557 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1521706 | 1558 | `		}` |
|       - | 1559 | `		/* Release the node */` |
| 3043422 | 1560 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   69216 | 1561 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   34607 | 1562 | `		}` |
| 3043422 | 1563 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1564 | `		/* Point to the next entry */` |
| 3043422 | 1565 | `		pEntry = pNext;` |
| 3043422 | 1566 | `		n++;` |
|       2 | 1567 | `	}` |
|   55318 | 1568 | `	if( pMap->nEntry > 0 ){` |
|       - | 1569 | `		/* Release the hash bucket */` |
|   49102 | 1570 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   24550 | 1571 | `	}` |
|   55318 | 1572 | `	if( FreeDS ){` |
|       - | 1573 | `		/* Free the whole instance */` |
|   55302 | 1574 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   27652 | 1575 | `	}else{` |
|       - | 1576 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1577 | `		pMap->apBucket = 0;` |
|      17 | 1578 | `		pMap->iNextIdx = 0;` |
|      17 | 1579 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1580 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1581 | `	}` |
|   55318 | 1582 | `	return SXRET_OK;` |
|   27660 | 1583 |  |
|       - | 1584 | `/*` |
|       - | 1585 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1586 | ` * If the count reaches zero which mean no more variables` |
|       - | 1587 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1588 | ` */` |
|  606538 | 1589 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1590 |  |
|  606540 | 1591 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1592 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  606540 | 1593 | `	pMap->iRef--;` |
|  606540 | 1594 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   55286 | 1595 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   27642 | 1596 | `	}` |
|  606540 | 1597 |  |
|       - | 1598 | `/*` |
|       - | 1599 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1600 | ` * Write a pointer to the target node on success.` |
|       - | 1601 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1602 | ` */` |
|  113946 | 1603 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1604 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1605 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1606 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1607 | `	)` |
|       2 | 1608 |  |
|       - | 1609 | `	sxi32 rc;` |
|  113948 | 1610 | `	if( pMap->nEntry < 1 ){` |
|       - | 1611 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1612 | `		 */` |
|      42 | 1613 | `		return SXERR_NOTFOUND;` |
|       - | 1614 | `	}` |
|  113908 | 1615 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  113908 | 1616 | `	return rc;` |
|   56975 | 1617 |  |
|       - | 1618 | `/*` |
|       - | 1619 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1620 | ` * hashmap.` |
|       - | 1621 | ` * If a node with the given key already exists in the database` |
|       - | 1622 | ` * then this function overwrite the old value.` |
|       - | 1623 | ` */` |
| 2486468 | 1624 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1625 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1626 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1627 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1628 | `	)` |
|       2 | 1629 |  |
|       - | 1630 | `	sxi32 rc;` |
| 2486470 | 1631 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1632 | `		/*` |
|       - | 1633 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1634 | `		 */` |
|     ! 0 | 1635 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1636 | `		return SXRET_OK;` |
|       - | 1637 | `	}` |
| 2486470 | 1638 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2486470 | 1639 | `	return rc;` |
| 1243236 | 1640 |  |
|       - | 1641 | `/*` |
|       - | 1642 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|       - | 1643 | ` *   - String keys overwrite same-key entries in pDest.` |
|       - | 1644 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|       - | 1645 | ` * This is the same routine that backs array_merge().` |
|       - | 1646 | ` */` |
|      52 | 1647 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1648 |  |
|      53 | 1649 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|       1 | 1650 |  |
|       - | 1651 | `/*` |
|       - | 1652 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1653 | ` * hashmap.` |
|       - | 1654 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1655 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1656 | ` * The insertion by reference is triggered when the following` |
|       - | 1657 | ` * expression is encountered.` |
|       - | 1658 | ` * $var = 10;` |
|       - | 1659 | ` *  $a = array(&var);` |
|       - | 1660 | ` * OR` |
|       - | 1661 | ` *  $a[] =& $var;` |
|       - | 1662 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1663 | ` * over it's contents.` |
|       - | 1664 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1665 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1666 | ` * Example:` |
|       - | 1667 | ` *  $var = 10;` |
|       - | 1668 | ` *  $a[] =& $var;` |
|       - | 1669 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1670 | ` *  //Unset the foreign ph7_value now` |
|       - | 1671 | ` *  unset($var);` |
|       - | 1672 | ` *  echo count($a); //0` |
|       - | 1673 | ` * Note that this is a PH7 eXtension.` |
|       - | 1674 | ` * Refer to the official documentation for more information.` |
|       - | 1675 | ` * If a node with the given key already exists in the database` |
|       - | 1676 | ` * then this function overwrite the old value.` |
|       - | 1677 | ` */` |
|   35494 | 1678 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1679 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1680 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1681 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1682 | `	)` |
|       2 | 1683 |  |
|       - | 1684 | `	sxi32 rc;` |
|   35496 | 1685 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1686 | `		/*` |
|       - | 1687 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1688 | `		 */` |
|     ! 0 | 1689 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1690 | `		return SXRET_OK;` |
|       - | 1691 | `	}` |
|   35496 | 1692 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   35496 | 1693 | `	return rc;` |
|   17749 | 1694 |  |
|       - | 1695 | `/*` |
|       - | 1696 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1697 | ` */` |
|   24654 | 1698 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1699 |  |
|       - | 1700 | `	/* Reset the loop cursor */` |
|   24656 | 1701 | `	pMap->pCur = pMap->pFirst;` |
|   24656 | 1702 |  |
|       - | 1703 | `/*` |
|       - | 1704 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1705 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1706 | ` * return NULL.` |
|       - | 1707 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1708 | ` */` |
|  203032 | 1709 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1710 |  |
|  203034 | 1711 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  203034 | 1712 | `	if( pCur == 0 ){` |
|       - | 1713 | `		/* End of the list,return null */` |
|   12348 | 1714 | `		return 0;` |
|       - | 1715 | `	}` |
|       - | 1716 | `	/* Advance the node cursor */` |
|  190688 | 1717 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  190688 | 1718 | `	return pCur;` |
|  101518 | 1719 |  |
|       - | 1720 | `/*` |
|       - | 1721 | ` * Extract a node value.` |
|       - | 1722 | ` */` |
|  482450 | 1723 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1724 |  |
|  482452 | 1725 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  482452 | 1726 | `	if( pEntry ){` |
|  482452 | 1727 | `		if( bStore ){` |
|  190826 | 1728 | `			PH7_MemObjStore(pEntry,pValue);` |
|   95414 | 1729 | `		}else{` |
|  291628 | 1730 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1731 | `		}` |
|  241209 | 1732 | `	}else{` |
|     ! 0 | 1733 | `		PH7_MemObjRelease(pValue);` |
|       - | 1734 | `	}` |
|  482452 | 1735 |  |
|       - | 1736 | `/*` |
|       - | 1737 | ` * Extract a node key.` |
|       - | 1738 | ` */` |
|  120218 | 1739 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1740 |  |
|       - | 1741 | `	/* Fill with the current key */` |
|  120220 | 1742 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  119842 | 1743 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1744 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1745 | `		}` |
|  119842 | 1746 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  119842 | 1747 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   59922 | 1748 | `	}else{` |
|     380 | 1749 | `		SyBlobReset(&pKey->sBlob);` |
|     380 | 1750 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     380 | 1751 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1752 | `	}` |
|  120220 | 1753 |  |
|       - | 1754 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1755 | `/*` |
|       - | 1756 | ` * Store the address of nodes value in the given container.` |
|       - | 1757 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1758 | ` * defined in 'builtin.c' for more information.` |
|       - | 1759 | ` */` |
|      10 | 1760 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1761 |  |
|      11 | 1762 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1763 | `	ph7_value *pValue;` |
|       - | 1764 | `	sxu32 n;` |
|       - | 1765 | `	/* Initialize the container */` |
|      11 | 1766 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1767 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1768 | `		/* Extract node value */` |
|      17 | 1769 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1770 | `		if( pValue ){` |
|      17 | 1771 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1772 | `		}` |
|       - | 1773 | `		/* Point to the next entry */` |
|      17 | 1774 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1775 | `	}` |
|       - | 1776 | `	/* Total inserted entries */` |
|      11 | 1777 | `	return (int)SySetUsed(pOut);` |
|       1 | 1778 |  |
|       - | 1779 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1780 | `/* SPDX-SnippetBegin */` |
|       - | 1781 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - | 1782 | `/* SPDX-License-Identifier: blessing */` |
|       - | 1783 | `/*` |
|       - | 1784 | ` * Merge sort.` |
|       - | 1785 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1786 | ` * Status: Public domain` |
|       - | 1787 | ` */` |
|       - | 1788 | `/* Node comparison callback signature */` |
|       - | 1789 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1790 | `/*` |
|       - | 1791 | `** Inputs:` |
|       - | 1792 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1793 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1794 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1795 | `**` |
|       - | 1796 | `** Return Value:` |
|       - | 1797 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1798 | `**   of both a and b.` |
|       - | 1799 | `**` |
|       - | 1800 | `** Side effects:` |
|       - | 1801 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1802 | `**   changed.` |
|       - | 1803 | `*/` |
|   31252 | 1804 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1805 |  |
|       - | 1806 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1807 | `    /* Prevent compiler warning */` |
|   31254 | 1808 | `	result.pNext = result.pPrev = 0;` |
|   31254 | 1809 | `	pTail = &result;` |
|   92229 | 1810 | `	while( pA && pB ){` |
|   60977 | 1811 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   40871 | 1812 | `			pTail->pPrev = pA;` |
|   40871 | 1813 | `			pA->pNext = pTail;` |
|   40871 | 1814 | `			pTail = pA;` |
|   40871 | 1815 | `			pA = pA->pPrev;` |
|   20423 | 1816 | `		}else{` |
|   20108 | 1817 | `			pTail->pPrev = pB;` |
|   20108 | 1818 | `			pB->pNext = pTail;` |
|   20108 | 1819 | `			pTail = pB;` |
|   20108 | 1820 | `			pB = pB->pPrev;` |
|       - | 1821 | `		}` |
|       2 | 1822 | `	}` |
|   31254 | 1823 | `	if( pA ){` |
|   22215 | 1824 | `		pTail->pPrev = pA;` |
|   22215 | 1825 | `		pA->pNext = pTail;` |
|   20150 | 1826 | `	}else if( pB ){` |
|    8831 | 1827 | `		pTail->pPrev = pB;` |
|    8831 | 1828 | `		pB->pNext = pTail;` |
|    4414 | 1829 | `	}else{` |
|     212 | 1830 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1831 | `	}` |
|   31254 | 1832 | `	return result.pPrev;` |
|       2 | 1833 |  |
|       - | 1834 | `/*` |
|       - | 1835 | `** Inputs:` |
|       - | 1836 | `**   Map:       Input hashmap` |
|       - | 1837 | `**   cmp:       A comparison function.` |
|       - | 1838 | `**` |
|       - | 1839 | `** Return Value:` |
|       - | 1840 | `**   Sorted hashmap.` |
|       - | 1841 | `**` |
|       - | 1842 | `** Side effects:` |
|       - | 1843 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1844 | `*/` |
|       - | 1845 | `#define N_SORT_BUCKET  32` |
|     660 | 1846 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1847 |  |
|       - | 1848 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1849 | `	sxu32 i;` |
|     662 | 1850 | `	SyZero(a,sizeof(a));` |
|       - | 1851 | `	/* Point to the first inserted entry */` |
|     662 | 1852 | `	pIn = pMap->pFirst;` |
|   12630 | 1853 | `	while( pIn ){` |
|   11970 | 1854 | `		p = pIn;` |
|   11970 | 1855 | `		pIn = p->pPrev;` |
|   11970 | 1856 | `		p->pPrev = 0;` |
|   22762 | 1857 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   22762 | 1858 | `			if( a[i]==0 ){` |
|   11970 | 1859 | `				a[i] = p;` |
|   11970 | 1860 | `				break;` |
|     ! 0 | 1861 | `			}else{` |
|   10794 | 1862 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   10794 | 1863 | `				a[i] = 0;` |
|       - | 1864 | `			}` |
|    5398 | 1865 | `		}` |
|   11970 | 1866 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1867 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1868 | `			 * But that is impossible.` |
|       - | 1869 | `			 */` |
|     ! 0 | 1870 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1871 | `		}` |
|       2 | 1872 | `	}` |
|     662 | 1873 | `	p = a[0];` |
|   21122 | 1874 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   20462 | 1875 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   10232 | 1876 | `	}` |
|     662 | 1877 | `	p->pNext = 0;` |
|       - | 1878 | `	/* Reflect the change */` |
|     662 | 1879 | `	pMap->pFirst = p;` |
|       - | 1880 | `	/* Reset the loop cursor */` |
|     662 | 1881 | `	pMap->pCur = pMap->pFirst;` |
|     662 | 1882 | `	return SXRET_OK;` |
|       2 | 1883 |  |
|       - | 1884 | `/* SPDX-SnippetEnd */` |
|       - | 1885 | `/*` |
|       - | 1886 | ` * Node comparison callback.` |
|       - | 1887 | ` * used-by: [sort(),asort(),...]` |
|       - | 1888 | ` */` |
|   60782 | 1889 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1890 |  |
|       - | 1891 | `	ph7_value sA,sB;` |
|       - | 1892 | `	sxi32 iFlags;` |
|       - | 1893 | `	int rc;` |
|   60784 | 1894 | `	if( pCmpData == 0 ){` |
|       - | 1895 | `		/* Perform a standard comparison */` |
|   60760 | 1896 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   60760 | 1897 | `		return rc;` |
|       - | 1898 | `	}` |
|      25 | 1899 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1900 | `	/* Duplicate node values */` |
|      25 | 1901 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 1902 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 1903 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 1904 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 1905 | `	if( iFlags == 5 ){` |
|       - | 1906 | `		/* String cast */` |
|       - | 1907 | `		const char *zA,*zB;` |
|       - | 1908 | `		sxu32 nA,nB,nMin;` |
|      15 | 1909 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1910 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1911 | `		}` |
|      15 | 1912 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1913 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1914 | `		}` |
|       - | 1915 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 1916 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 1917 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 1918 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 1919 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 1920 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 1921 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 1922 | `		if( rc == 0 ){` |
|       5 | 1923 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1924 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1925 | `		}` |
|       8 | 1926 | `	}else{` |
|       - | 1927 | `		/* Numeric cast */` |
|      11 | 1928 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1929 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1930 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1931 | `	}` |
|      25 | 1932 | `	PH7_MemObjRelease(&sA);` |
|      25 | 1933 | `	PH7_MemObjRelease(&sB);` |
|      25 | 1934 | `	return rc;` |
|   30384 | 1935 |  |
|       - | 1936 | `/*` |
|       - | 1937 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1938 | ` * used-by: [ksort()]` |
|       - | 1939 | ` */` |
|      14 | 1940 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1941 |  |
|       - | 1942 | `	sxi32 rc;` |
|       7 | 1943 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 1944 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1945 | `		/* Perform a string comparison */` |
|       5 | 1946 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1947 | `	}else{` |
|       - | 1948 | `		SyString sStr;` |
|       - | 1949 | `		sxi64 iA,iB;` |
|       - | 1950 | `		/* Perform a numeric comparison */` |
|      11 | 1951 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1952 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1953 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1954 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1955 | `				iA = 0;` |
|     ! 0 | 1956 | `			}else{` |
|     ! 0 | 1957 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1958 | `			}` |
|     ! 0 | 1959 | `		}else{` |
|      11 | 1960 | `			iA = pA->xKey.iKey;` |
|       - | 1961 | `		}` |
|      11 | 1962 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1963 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1964 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1965 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1966 | `				iB = 0;` |
|     ! 0 | 1967 | `			}else{` |
|     ! 0 | 1968 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1969 | `			}` |
|     ! 0 | 1970 | `		}else{` |
|      11 | 1971 | `			iB = pB->xKey.iKey;` |
|       - | 1972 | `		}` |
|      11 | 1973 | `		rc = (sxi32)(iA-iB);` |
|       - | 1974 | `	}` |
|       - | 1975 | `	/* Comparison result */` |
|      15 | 1976 | `	return rc;` |
|       1 | 1977 |  |
|       - | 1978 | `/*` |
|       - | 1979 | ` * Node comparison callback.` |
|       - | 1980 | ` * Used by: [rsort(),arsort()];` |
|       - | 1981 | ` */` |
|      78 | 1982 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1983 |  |
|       - | 1984 | `	ph7_value sA,sB;` |
|       - | 1985 | `	sxi32 iFlags;` |
|       - | 1986 | `	int rc;` |
|      79 | 1987 | `	if( pCmpData == 0 ){` |
|       - | 1988 | `		/* Perform a standard comparison */` |
|      59 | 1989 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 1990 | `		return -rc;` |
|       - | 1991 | `	}` |
|      21 | 1992 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1993 | `	/* Duplicate node values */` |
|      21 | 1994 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 1995 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 1996 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 1997 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 1998 | `	if( iFlags == 5 ){` |
|       - | 1999 | `		/* String cast */` |
|       - | 2000 | `		const char *zA,*zB;` |
|       - | 2001 | `		sxu32 nA,nB,nMin;` |
|      11 | 2002 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2003 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 2004 | `		}` |
|      11 | 2005 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2006 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 2007 | `		}` |
|       - | 2008 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 2009 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 2010 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 2011 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 2012 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 2013 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 2014 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 2015 | `		if( rc == 0 ){` |
|       3 | 2016 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 2017 | `			else if( nA > nB ) rc = 1;` |
|       1 | 2018 | `		}` |
|       6 | 2019 | `	}else{` |
|       - | 2020 | `		/* Numeric cast */` |
|      11 | 2021 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 2022 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 2023 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 2024 | `	}` |
|      21 | 2025 | `	PH7_MemObjRelease(&sA);` |
|      21 | 2026 | `	PH7_MemObjRelease(&sB);` |
|      21 | 2027 | `	return -rc;` |
|      40 | 2028 |  |
|       - | 2029 | `/*` |
|       - | 2030 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2031 | ` * used-by: [usort(),uasort()]` |
|       - | 2032 | ` */` |
|      78 | 2033 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 2034 |  |
|       - | 2035 | `	ph7_value sResult,*pCallback;` |
|       - | 2036 | `	ph7_value *pV1,*pV2;` |
|       - | 2037 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2038 | `	sxi32 rc;` |
|       - | 2039 | `	/* Point to the desired callback */` |
|      80 | 2040 | `	pCallback = (ph7_value *)pCmpData;` |
|      80 | 2041 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2042 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2043 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       3 | 2044 | `		return 0;` |
|       - | 2045 | `	}` |
|       - | 2046 | `	/* initialize the result value */` |
|      78 | 2047 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 2048 | `	/* Extract nodes values */` |
|      78 | 2049 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      78 | 2050 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      78 | 2051 | `	apArg[0] = pV1;` |
|      78 | 2052 | `	apArg[1] = pV2;` |
|       - | 2053 | `	/* Invoke the callback */` |
|      78 | 2054 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      78 | 2055 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2056 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2057 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       3 | 2058 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       3 | 2059 | `		rc = 0;` |
|      77 | 2060 | `	}else if( rc != SXRET_OK ){` |
|       - | 2061 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2062 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2063 | `	}else{` |
|       - | 2064 | `		/* Extract callback result */` |
|      76 | 2065 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2066 | `			/* Perform an int cast */` |
|     ! 0 | 2067 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2068 | `		}` |
|      76 | 2069 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2070 | `	}` |
|      78 | 2071 | `	PH7_MemObjRelease(&sResult);` |
|       - | 2072 | `	/* Callback result */` |
|      78 | 2073 | `	return rc;` |
|      41 | 2074 |  |
|       - | 2075 | `/*` |
|       - | 2076 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2077 | ` * used-by: [krsort()]` |
|       - | 2078 | ` */` |
|       4 | 2079 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2080 |  |
|       - | 2081 | `	sxi32 rc;` |
|       2 | 2082 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2083 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2084 | `		/* Perform a string comparison */` |
|       5 | 2085 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2086 | `	}else{` |
|       - | 2087 | `		SyString sStr;` |
|       - | 2088 | `		sxi64 iA,iB;` |
|       - | 2089 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2090 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2091 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2092 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2093 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2094 | `				iA = 0;` |
|     ! 0 | 2095 | `			}else{` |
|     ! 0 | 2096 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2097 | `			}` |
|     ! 0 | 2098 | `		}else{` |
|     ! 0 | 2099 | `			iA = pA->xKey.iKey;` |
|       - | 2100 | `		}` |
|     ! 0 | 2101 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2102 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2103 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2104 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2105 | `				iB = 0;` |
|     ! 0 | 2106 | `			}else{` |
|     ! 0 | 2107 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2108 | `			}` |
|     ! 0 | 2109 | `		}else{` |
|     ! 0 | 2110 | `			iB = pB->xKey.iKey;` |
|       - | 2111 | `		}` |
|     ! 0 | 2112 | `		rc = (sxi32)(iA-iB);` |
|       - | 2113 | `	}` |
|       5 | 2114 | `	return -rc; /* Reverse result */` |
|       1 | 2115 |  |
|       - | 2116 | `/*` |
|       - | 2117 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2118 | ` * used-by: [uksort()]` |
|       - | 2119 | ` */` |
|       6 | 2120 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2121 |  |
|       - | 2122 | `	ph7_value sResult,*pCallback;` |
|       - | 2123 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2124 | `	ph7_value sK1,sK2;` |
|       - | 2125 | `	sxi32 rc;` |
|       - | 2126 | `	/* Point to the desired callback */` |
|       7 | 2127 | `	pCallback = (ph7_value *)pCmpData;` |
|       7 | 2128 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2129 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2130 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     ! 0 | 2131 | `		return 0;` |
|       - | 2132 | `	}` |
|       - | 2133 | `	/* initialize the result value */` |
|       7 | 2134 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2135 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2136 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2137 | `	/* Extract nodes keys */` |
|       7 | 2138 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2139 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2140 | `	apArg[0] = &sK1;` |
|       7 | 2141 | `	apArg[1] = &sK2;` |
|       - | 2142 | `	/* Mark keys as constants */` |
|       7 | 2143 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2144 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2145 | `	/* Invoke the callback */` |
|       7 | 2146 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2147 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2148 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2149 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     ! 0 | 2150 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     ! 0 | 2151 | `		rc = 0;` |
|       7 | 2152 | `	}else if( rc != SXRET_OK ){` |
|       - | 2153 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2154 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2155 | `	}else{` |
|       - | 2156 | `		/* Extract callback result */` |
|       7 | 2157 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2158 | `			/* Perform an int cast */` |
|     ! 0 | 2159 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2160 | `		}` |
|       7 | 2161 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2162 | `	}` |
|       7 | 2163 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2164 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2165 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2166 | `	/* Callback result */` |
|       7 | 2167 | `	return rc;` |
|       4 | 2168 |  |
|       - | 2169 | `/*` |
|       - | 2170 | ` * Node comparison callback: Random node comparison.` |
|       - | 2171 | ` * used-by: [shuffle()]` |
|       - | 2172 | ` */` |
|      13 | 2173 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2174 |  |
|       - | 2175 | `	sxu32 n;` |
|       7 | 2176 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 2177 | `	SXUNUSED(pCmpData);` |
|       - | 2178 | `	/* Grab a random number */` |
|      14 | 2179 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2180 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2181 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2182 | `	 */` |
|      14 | 2183 | `	return n&1 ? 1 : -1;` |
|       1 | 2184 |  |
|       - | 2185 | `/*` |
|       - | 2186 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2187 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2188 | ` */` |
|     612 | 2189 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2190 |  |
|       - | 2191 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2192 | `	sxu32 i;` |
|       - | 2193 | `	/* Rehash all entries */` |
|     614 | 2194 | `	pLast = p = pMap->pFirst;` |
|     614 | 2195 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     614 | 2196 | `	i = 0;` |
|    6205 | 2197 | `	for( ;; ){` |
|   12412 | 2198 | `		if( i >= pMap->nEntry ){` |
|     614 | 2199 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     614 | 2200 | `			break;` |
|       - | 2201 | `		}` |
|   11800 | 2202 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2203 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2204 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2205 | `			/* Change key type */` |
|       5 | 2206 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2207 | `		}` |
|   11800 | 2208 | `		HashmapRehashIntNode(p);` |
|       - | 2209 | `		/* Point to the next entry */` |
|   11800 | 2210 | `		i++;` |
|   11800 | 2211 | `		pLast = p;` |
|   11800 | 2212 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2213 | `	}` |
|     614 | 2214 |  |
|       - | 2215 | `/*` |
|       - | 2216 | ` * Array functions implementation.` |
|       - | 2217 | ` * Status:` |
|       - | 2218 | ` *  Stable.` |
|       - | 2219 | ` */` |
|       - | 2220 | `/*` |
|       - | 2221 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2222 | ` * Sort an array.` |
|       - | 2223 | ` * Parameters` |
|       - | 2224 | ` *  $array` |
|       - | 2225 | ` *   The input array.` |
|       - | 2226 | ` * $sort_flags` |
|       - | 2227 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2228 | ` *  Sorting type flags:` |
|       - | 2229 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2230 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2231 | ` *   SORT_STRING - compare items as strings` |
|       - | 2232 | ` * Return` |
|       - | 2233 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2234 | ` *` |
|       - | 2235 | ` */` |
|     942 | 2236 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2237 |  |
|       - | 2238 | `	ph7_hashmap *pMap;` |
|       - | 2239 | `	/* Make sure we are dealing with a valid hashmap */` |
|     944 | 2240 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2241 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2242 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2243 | `		return PH7_OK;` |
|       - | 2244 | `	}` |
|       - | 2245 | `	/* Point to the internal representation of the input hashmap */` |
|     944 | 2246 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     944 | 2247 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     944 | 2248 | `	if( pMap->nEntry > 1 ){` |
|     602 | 2249 | `		sxi32 iCmpFlags = 0;` |
|     602 | 2250 | `		if( nArg > 1 ){` |
|       - | 2251 | `			/* Extract comparison flags */` |
|       3 | 2252 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2253 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2254 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2255 | `			}` |
|       1 | 2256 | `		}` |
|       - | 2257 | `		/* Do the merge sort */` |
|     602 | 2258 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2259 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     602 | 2260 | `		HashmapSortRehash(pMap);` |
|     300 | 2261 | `	}` |
|       - | 2262 | `	/* All done,return TRUE */` |
|     944 | 2263 | `	ph7_result_bool(pCtx,1);` |
|     944 | 2264 | `	return PH7_OK;` |
|     473 | 2265 |  |
|       - | 2266 | `/*` |
|       - | 2267 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2268 | ` *  Sort an array and maintain index association.` |
|       - | 2269 | ` * Parameters` |
|       - | 2270 | ` *  $array` |
|       - | 2271 | ` *   The input array.` |
|       - | 2272 | ` * $sort_flags` |
|       - | 2273 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2274 | ` *  Sorting type flags:` |
|       - | 2275 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2276 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2277 | ` *   SORT_STRING - compare items as strings` |
|       - | 2278 | ` * Return` |
|       - | 2279 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2280 | ` */` |
|      32 | 2281 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2282 |  |
|       - | 2283 | `	ph7_hashmap *pMap;` |
|       - | 2284 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2285 | `	if( nArg < 1 ){` |
|       3 | 2286 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2287 | `			"ArgumentCountError",` |
|       - | 2288 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2289 | `			);` |
|       - | 2290 | `	}` |
|       - | 2291 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2292 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2293 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2294 | `			"TypeError",` |
|       - | 2295 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2296 | `			ph7_type_name(apArg[0])` |
|       - | 2297 | `			);` |
|       - | 2298 | `	}` |
|       - | 2299 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2300 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2301 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2302 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2303 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2304 | `		if( nArg > 1 ){` |
|       - | 2305 | `			/* Extract comparison flags */` |
|       5 | 2306 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2307 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2308 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2309 | `			}` |
|       2 | 2310 | `		}` |
|       - | 2311 | `		/* Do the merge sort */` |
|      19 | 2312 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2313 | `		/* Fix the last link broken by the merge */` |
|      45 | 2314 | `		while(pMap->pLast->pPrev){` |
|      27 | 2315 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2316 | `		}` |
|       9 | 2317 | `	}` |
|       - | 2318 | `	/* All done,return TRUE */` |
|      23 | 2319 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2320 | `	return PH7_OK;` |
|      18 | 2321 |  |
|       - | 2322 | `/*` |
|       - | 2323 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2324 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2325 | ` * Parameters` |
|       - | 2326 | ` *  $array` |
|       - | 2327 | ` *   The input array.` |
|       - | 2328 | ` * $sort_flags` |
|       - | 2329 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2330 | ` *  Sorting type flags:` |
|       - | 2331 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2332 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2333 | ` *   SORT_STRING - compare items as strings` |
|       - | 2334 | ` * Return` |
|       - | 2335 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2336 | ` */` |
|      32 | 2337 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2338 |  |
|       - | 2339 | `	ph7_hashmap *pMap;` |
|       - | 2340 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2341 | `	if( nArg < 1 ){` |
|       3 | 2342 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2343 | `			"ArgumentCountError",` |
|       - | 2344 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2345 | `			);` |
|       - | 2346 | `	}` |
|       - | 2347 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2348 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2349 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2350 | `			"TypeError",` |
|       - | 2351 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2352 | `			ph7_type_name(apArg[0])` |
|       - | 2353 | `			);` |
|       - | 2354 | `	}` |
|       - | 2355 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2356 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2357 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2358 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2359 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2360 | `		if( nArg > 1 ){` |
|       - | 2361 | `			/* Extract comparison flags */` |
|       5 | 2362 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2363 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2364 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2365 | `			}` |
|       2 | 2366 | `		}` |
|       - | 2367 | `		/* Do the merge sort */` |
|      19 | 2368 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2369 | `		/* Fix the last link broken by the merge */` |
|      35 | 2370 | `		while(pMap->pLast->pPrev){` |
|      17 | 2371 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2372 | `		}` |
|       9 | 2373 | `	}` |
|       - | 2374 | `	/* All done,return TRUE */` |
|      23 | 2375 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2376 | `	return PH7_OK;` |
|      18 | 2377 |  |
|       - | 2378 | `/*` |
|       - | 2379 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2380 | ` *  Sort an array by key.` |
|       - | 2381 | ` * Parameters` |
|       - | 2382 | ` *  $array` |
|       - | 2383 | ` *   The input array.` |
|       - | 2384 | ` * $sort_flags` |
|       - | 2385 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2386 | ` *  Sorting type flags:` |
|       - | 2387 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2388 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2389 | ` *   SORT_STRING - compare items as strings` |
|       - | 2390 | ` * Return` |
|       - | 2391 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2392 | ` */` |
|       4 | 2393 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2394 |  |
|       - | 2395 | `	ph7_hashmap *pMap;` |
|       - | 2396 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2397 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2398 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2399 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2400 | `		return PH7_OK;` |
|       - | 2401 | `	}` |
|       - | 2402 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2403 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2404 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2405 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2406 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2407 | `		if( nArg > 1 ){` |
|       - | 2408 | `			/* Extract comparison flags */` |
|     ! 0 | 2409 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2410 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2411 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2412 | `			}` |
|     ! 0 | 2413 | `		}` |
|       - | 2414 | `		/* Do the merge sort */` |
|       5 | 2415 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2416 | `		/* Fix the last link broken by the merge */` |
|      15 | 2417 | `		while(pMap->pLast->pPrev){` |
|      11 | 2418 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2419 | `		}` |
|       2 | 2420 | `	}` |
|       - | 2421 | `	/* All done,return TRUE */` |
|       5 | 2422 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2423 | `	return PH7_OK;` |
|       3 | 2424 |  |
|       - | 2425 | `/*` |
|       - | 2426 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2427 | ` *  Sort an array by key in reverse order.` |
|       - | 2428 | ` * Parameters` |
|       - | 2429 | ` *  $array` |
|       - | 2430 | ` *   The input array.` |
|       - | 2431 | ` * $sort_flags` |
|       - | 2432 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2433 | ` *  Sorting type flags:` |
|       - | 2434 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2435 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2436 | ` *   SORT_STRING - compare items as strings` |
|       - | 2437 | ` * Return` |
|       - | 2438 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2439 | ` */` |
|       2 | 2440 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2441 |  |
|       - | 2442 | `	ph7_hashmap *pMap;` |
|       - | 2443 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2444 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2445 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2446 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2447 | `		return PH7_OK;` |
|       - | 2448 | `	}` |
|       - | 2449 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2450 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2451 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2452 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2453 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2454 | `		if( nArg > 1 ){` |
|       - | 2455 | `			/* Extract comparison flags */` |
|     ! 0 | 2456 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2457 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2458 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2459 | `			}` |
|     ! 0 | 2460 | `		}` |
|       - | 2461 | `		/* Do the merge sort */` |
|       3 | 2462 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2463 | `		/* Fix the last link broken by the merge */` |
|       7 | 2464 | `		while(pMap->pLast->pPrev){` |
|       5 | 2465 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2466 | `		}` |
|       1 | 2467 | `	}` |
|       - | 2468 | `	/* All done,return TRUE */` |
|       3 | 2469 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2470 | `	return PH7_OK;` |
|       2 | 2471 |  |
|       - | 2472 | `/*` |
|       - | 2473 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2474 | ` * Sort an array in reverse order.` |
|       - | 2475 | ` * Parameters` |
|       - | 2476 | ` *  $array` |
|       - | 2477 | ` *   The input array.` |
|       - | 2478 | ` * $sort_flags` |
|       - | 2479 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2480 | ` *  Sorting type flags:` |
|       - | 2481 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2482 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2483 | ` *   SORT_STRING - compare items as strings` |
|       - | 2484 | ` * Return` |
|       - | 2485 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2486 | ` */` |
|       2 | 2487 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2488 |  |
|       - | 2489 | `	ph7_hashmap *pMap;` |
|       - | 2490 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2491 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2492 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2493 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2494 | `		return PH7_OK;` |
|       - | 2495 | `	}` |
|       - | 2496 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2497 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2498 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2499 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2500 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2501 | `		if( nArg > 1 ){` |
|       - | 2502 | `			/* Extract comparison flags */` |
|     ! 0 | 2503 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2504 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2505 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2506 | `			}` |
|     ! 0 | 2507 | `		}` |
|       - | 2508 | `		/* Do the merge sort */` |
|       3 | 2509 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2510 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2511 | `		HashmapSortRehash(pMap);` |
|       1 | 2512 | `	}` |
|       - | 2513 | `	/* All done,return TRUE */` |
|       3 | 2514 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2515 | `	return PH7_OK;` |
|       2 | 2516 |  |
|       - | 2517 | `/*` |
|       - | 2518 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2519 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2520 | ` * Parameters` |
|       - | 2521 | ` *  $array` |
|       - | 2522 | ` *   The input array.` |
|       - | 2523 | ` * $cmp_function` |
|       - | 2524 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2525 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2526 | ` *  to, or greater than the second.` |
|       - | 2527 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2528 | ` * Return` |
|       - | 2529 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2530 | ` */` |
|       8 | 2531 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2532 |  |
|       - | 2533 | `	ph7_hashmap *pMap;` |
|       - | 2534 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 | 2535 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2536 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2537 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2538 | `		return PH7_OK;` |
|       - | 2539 | `	}` |
|       - | 2540 | `	/* Point to the internal representation of the input hashmap */` |
|      10 | 2541 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      10 | 2542 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      10 | 2543 | `	if( pMap->nEntry > 1 ){` |
|      10 | 2544 | `		ph7_value *pCallback = 0;` |
|       - | 2545 | `		ProcNodeCmp xCmp;` |
|      10 | 2546 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      10 | 2547 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2548 | `			/* Point to the desired callback */` |
|      10 | 2549 | `			pCallback = apArg[1];` |
|       6 | 2550 | `		}else{` |
|       - | 2551 | `			/* Use the default comparison function */` |
|     ! 0 | 2552 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2553 | `		}` |
|       - | 2554 | `		/* Do the merge sort */` |
|      10 | 2555 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      10 | 2556 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2557 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      10 | 2558 | `		HashmapSortRehash(pMap);` |
|      10 | 2559 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2560 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 2561 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2562 | `			return PH7_EXCEPTION;` |
|       - | 2563 | `		}` |
|       3 | 2564 | `	}` |
|       - | 2565 | `	/* All done,return TRUE */` |
|       8 | 2566 | `	ph7_result_bool(pCtx,1);` |
|       8 | 2567 | `	return PH7_OK;` |
|       6 | 2568 |  |
|       - | 2569 | `/*` |
|       - | 2570 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2571 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2572 | ` *  and maintain index association.` |
|       - | 2573 | ` * Parameters` |
|       - | 2574 | ` *  $array` |
|       - | 2575 | ` *   The input array.` |
|       - | 2576 | ` * $cmp_function` |
|       - | 2577 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2578 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2579 | ` *  to, or greater than the second.` |
|       - | 2580 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2581 | ` * Return` |
|       - | 2582 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2583 | ` */` |
|       2 | 2584 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2585 |  |
|       - | 2586 | `	ph7_hashmap *pMap;` |
|       - | 2587 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2588 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2589 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2590 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2591 | `		return PH7_OK;` |
|       - | 2592 | `	}` |
|       - | 2593 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2594 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2595 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2596 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2597 | `		ph7_value *pCallback = 0;` |
|       - | 2598 | `		ProcNodeCmp xCmp;` |
|       3 | 2599 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2600 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2601 | `			/* Point to the desired callback */` |
|       3 | 2602 | `			pCallback = apArg[1];` |
|       2 | 2603 | `		}else{` |
|       - | 2604 | `			/* Use the default comparison function */` |
|     ! 0 | 2605 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2606 | `		}` |
|       - | 2607 | `		/* Do the merge sort */` |
|       3 | 2608 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2609 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2610 | `		/* Fix the last link broken by the merge */` |
|       5 | 2611 | `		while(pMap->pLast->pPrev){` |
|       3 | 2612 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2613 | `		}` |
|       3 | 2614 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2615 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2616 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2617 | `			return PH7_EXCEPTION;` |
|       - | 2618 | `		}` |
|       1 | 2619 | `	}` |
|       - | 2620 | `	/* All done,return TRUE */` |
|       3 | 2621 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2622 | `	return PH7_OK;` |
|       2 | 2623 |  |
|       - | 2624 | `/*` |
|       - | 2625 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2626 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2627 | ` *  function and maintain index association.` |
|       - | 2628 | ` * Parameters` |
|       - | 2629 | ` *  $array` |
|       - | 2630 | ` *   The input array.` |
|       - | 2631 | ` * $cmp_function` |
|       - | 2632 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2633 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2634 | ` *  to, or greater than the second.` |
|       - | 2635 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2636 | ` * Return` |
|       - | 2637 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2638 | ` */` |
|       2 | 2639 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2640 |  |
|       - | 2641 | `	ph7_hashmap *pMap;` |
|       - | 2642 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2643 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2644 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2645 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2646 | `		return PH7_OK;` |
|       - | 2647 | `	}` |
|       - | 2648 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2649 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2650 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2651 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2652 | `		ph7_value *pCallback = 0;` |
|       - | 2653 | `		ProcNodeCmp xCmp;` |
|       3 | 2654 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2655 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2656 | `			/* Point to the desired callback */` |
|       3 | 2657 | `			pCallback = apArg[1];` |
|       2 | 2658 | `		}else{` |
|       - | 2659 | `			/* Use the default comparison function */` |
|     ! 0 | 2660 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2661 | `		}` |
|       - | 2662 | `		/* Do the merge sort */` |
|       3 | 2663 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2664 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2665 | `		/* Fix the last link broken by the merge */` |
|       3 | 2666 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2667 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2668 | `		}` |
|       3 | 2669 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2670 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2671 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2672 | `			return PH7_EXCEPTION;` |
|       - | 2673 | `		}` |
|       1 | 2674 | `	}` |
|       - | 2675 | `	/* All done,return TRUE */` |
|       3 | 2676 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2677 | `	return PH7_OK;` |
|       2 | 2678 |  |
|       - | 2679 | `/*` |
|       - | 2680 | ` * bool shuffle(array &$array)` |
|       - | 2681 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2682 | ` * Parameters` |
|       - | 2683 | ` *  $array` |
|       - | 2684 | ` *   The input array.` |
|       - | 2685 | ` * Return` |
|       - | 2686 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2687 | ` *` |
|       - | 2688 | ` */` |
|       2 | 2689 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2690 |  |
|       - | 2691 | `	ph7_hashmap *pMap;` |
|       - | 2692 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2693 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2694 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2695 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2696 | `		return PH7_OK;` |
|       - | 2697 | `	}` |
|       - | 2698 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2699 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2700 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2701 | `	if( pMap->nEntry > 1 ){` |
|       - | 2702 | `		/* Do the merge sort */` |
|       3 | 2703 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2704 | `		/* Fix the last link broken by the merge */` |
|      11 | 2705 | `		while(pMap->pLast->pPrev){` |
|       9 | 2706 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2707 | `		}` |
|       1 | 2708 | `	}` |
|       - | 2709 | `	/* All done,return TRUE */` |
|       3 | 2710 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2711 | `	return PH7_OK;` |
|       2 | 2712 |  |
|       - | 2713 | `/*` |
|       - | 2714 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2715 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2716 | ` * Parameters` |
|       - | 2717 | ` *  $var` |
|       - | 2718 | ` *   The array or the object.` |
|       - | 2719 | ` * $mode` |
|       - | 2720 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2721 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2722 | ` *  all the elements of a multidimensional array.` |
|       - | 2723 | ` * Return` |
|       - | 2724 | ` *  Returns the number of elements in the array.` |
|       - | 2725 | ` */` |
|     802 | 2726 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2727 |  |
|     804 | 2728 | `	int bRecursive = FALSE;` |
|     804 | 2729 | `	int bCycleDetected = FALSE;` |
|       - | 2730 | `	sxi64 iCount;` |
|     804 | 2731 | `	if( nArg < 1 ){` |
|       3 | 2732 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2733 | `			"ArgumentCountError",` |
|       - | 2734 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2735 | `			);` |
|       - | 2736 | `	}` |
|     802 | 2737 | `	if( nArg > 2 ){` |
|       4 | 2738 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2739 | `			"ArgumentCountError",` |
|       - | 2740 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2741 | `			nArg` |
|       - | 2742 | `			);` |
|       - | 2743 | `	}` |
|       - | 2744 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - | 2745 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - | 2746 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|     800 | 2747 | `	if( nArg > 1 ){` |
|      42 | 2748 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      42 | 2749 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       9 | 2750 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2751 | `				"ValueError",` |
|       - | 2752 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2753 | `				);` |
|       - | 2754 | `		}` |
|      34 | 2755 | `		bRecursive = iMode == 1;` |
|      16 | 2756 | `	}` |
|     792 | 2757 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2758 | `		/* Countable object: dispatch to ->count() */` |
|      28 | 2759 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      18 | 2760 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      18 | 2761 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      18 | 2762 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      16 | 2763 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2764 | `					"count",sizeof("count")-1);` |
|      16 | 2765 | `				if( pMeth ){` |
|       - | 2766 | `					ph7_value sResult;` |
|      16 | 2767 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      16 | 2768 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      16 | 2769 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      16 | 2770 | `					PH7_MemObjRelease(&sResult);` |
|      16 | 2771 | `					return PH7_OK;` |
|       - | 2772 | `				}` |
|     ! 0 | 2773 | `			}` |
|       1 | 2774 | `		}` |
|      19 | 2775 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2776 | `			"TypeError",` |
|       - | 2777 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       6 | 2778 | `			ph7_type_name(apArg[0])` |
|       - | 2779 | `			);` |
|       - | 2780 | `	}` |
|       - | 2781 | `	/* Count */` |
|     766 | 2782 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     766 | 2783 | `	if( bCycleDetected ){` |
|       3 | 2784 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2785 | `	}` |
|     766 | 2786 | `	ph7_result_int64(pCtx,iCount);` |
|     766 | 2787 | `	return PH7_OK;` |
|     403 | 2788 |  |
|       - | 2789 | `/*` |
|       - | 2790 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2791 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2792 | ` * Parameters` |
|       - | 2793 | ` * $key` |
|       - | 2794 | ` *   Value to check.` |
|       - | 2795 | ` * $search` |
|       - | 2796 | ` *  An array with keys to check.` |
|       - | 2797 | ` * Return` |
|       - | 2798 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2799 | ` */` |
|      82 | 2800 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2801 |  |
|       - | 2802 | `	sxi32 rc;` |
|      84 | 2803 | `	if( nArg != 2 ){` |
|       - | 2804 | `		/* PHP requires exactly two arguments */` |
|      10 | 2805 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2806 | `			"ArgumentCountError",` |
|       - | 2807 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2808 | `			nArg` |
|       - | 2809 | `			);` |
|       - | 2810 | `	}` |
|       - | 2811 | `	/* Make sure we are dealing with a valid hashmap */` |
|      78 | 2812 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2813 | `		/* Type mismatch -> TypeError */` |
|       7 | 2814 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2815 | `			"TypeError",` |
|       - | 2816 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2817 | `			ph7_type_name(apArg[1])` |
|       - | 2818 | `			);` |
|       - | 2819 | `	}` |
|       - | 2820 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      74 | 2821 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2822 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2823 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2824 | `			"use an empty string instead"` |
|       - | 2825 | `			);` |
|      73 | 2826 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2827 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2828 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2829 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2830 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2831 | `				,rVal` |
|       - | 2832 | `				);` |
|       1 | 2833 | `		}` |
|       1 | 2834 | `	}` |
|       - | 2835 | `	/* Perform the lookup */` |
|      74 | 2836 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2837 | `	/* lookup result */` |
|      74 | 2838 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      74 | 2839 | `	return PH7_OK;` |
|      43 | 2840 |  |
|       - | 2841 | `/*` |
|       - | 2842 | ` * value array_pop(array $array)` |
|       - | 2843 | ` *   POP the last inserted element from the array.` |
|       - | 2844 | ` * Parameter` |
|       - | 2845 | ` *  The array to get the value from.` |
|       - | 2846 | ` * Return` |
|       - | 2847 | ` *  Poped value or NULL on failure.` |
|       - | 2848 | ` */` |
|      18 | 2849 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2850 |  |
|       - | 2851 | `	ph7_hashmap *pMap;` |
|       - | 2852 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      20 | 2853 | `	if( nArg != 1 ){` |
|       7 | 2854 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2855 | `			"ArgumentCountError",` |
|       - | 2856 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2857 | `			nArg` |
|       - | 2858 | `			);` |
|       - | 2859 | `	}` |
|       - | 2860 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2861 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      16 | 2862 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2863 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2864 | `			"Error",` |
|       - | 2865 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2866 | `			);` |
|       - | 2867 | `	}` |
|       - | 2868 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2869 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2870 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2871 | `			"TypeError",` |
|       - | 2872 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2873 | `			ph7_type_name(apArg[0])` |
|       - | 2874 | `			);` |
|       - | 2875 | `	}` |
|       9 | 2876 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2877 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2878 | `	if( pMap->nEntry < 1 ){` |
|       - | 2879 | `		/* Nothing to pop,return NULL */` |
|       3 | 2880 | `		ph7_result_null(pCtx);` |
|       2 | 2881 | `	}else{` |
|       7 | 2882 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2883 | `		ph7_value *pObj;` |
|       7 | 2884 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2885 | `		if( pObj ){` |
|       - | 2886 | `			/* Node value */` |
|       7 | 2887 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2888 | `			/* Unlink the node */` |
|       7 | 2889 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2890 | `		}else{` |
|     ! 0 | 2891 | `			ph7_result_null(pCtx);` |
|       - | 2892 | `		}` |
|       - | 2893 | `		/* Reset the cursor */` |
|       7 | 2894 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2895 | `	}` |
|       9 | 2896 | `	return PH7_OK;` |
|      11 | 2897 |  |
|       - | 2898 | `/*` |
|       - | 2899 | ` * int array_push($array,$var,...)` |
|       - | 2900 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2901 | ` * Parameters` |
|       - | 2902 | ` *  array` |
|       - | 2903 | ` *    The input array.` |
|       - | 2904 | ` *  var` |
|       - | 2905 | ` *   On or more value to push.` |
|       - | 2906 | ` * Return` |
|       - | 2907 | ` *  New array count (including old items).` |
|       - | 2908 | ` */` |
|      22 | 2909 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2910 |  |
|       - | 2911 | `	ph7_hashmap *pMap;` |
|       - | 2912 | `	sxi32 rc;` |
|       - | 2913 | `	int i;` |
|      24 | 2914 | `	if( nArg < 1 ){` |
|       4 | 2915 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2916 | `			"ArgumentCountError",` |
|       - | 2917 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2918 | `			nArg` |
|       - | 2919 | `			);` |
|       - | 2920 | `	}` |
|       - | 2921 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2922 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      22 | 2923 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2924 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2925 | `			"Error",` |
|       - | 2926 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2927 | `			);` |
|       - | 2928 | `	}` |
|       - | 2929 | `	/* Make sure we are dealing with a valid hashmap */` |
|      18 | 2930 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2931 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2932 | `			"TypeError",` |
|       - | 2933 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2934 | `			ph7_type_name(apArg[0])` |
|       - | 2935 | `			);` |
|       - | 2936 | `	}` |
|       - | 2937 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2938 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2939 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2940 | `	/* Start pushing given values */` |
|      31 | 2941 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      17 | 2942 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      17 | 2943 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2944 | `			break;` |
|       - | 2945 | `		}` |
|       9 | 2946 | `	}` |
|       - | 2947 | `	/* Return the new count */` |
|      15 | 2948 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 2949 | `	return PH7_OK;` |
|      13 | 2950 |  |
|       - | 2951 | `/*` |
|       - | 2952 | ` * value array_shift(array $array)` |
|       - | 2953 | ` *   Shift an element off the beginning of array.` |
|       - | 2954 | ` * Parameter` |
|       - | 2955 | ` *  The array to get the value from.` |
|       - | 2956 | ` * Return` |
|       - | 2957 | ` *  Shifted value or NULL on failure.` |
|       - | 2958 | ` */` |
|      38 | 2959 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2960 |  |
|       - | 2961 | `	ph7_hashmap *pMap;` |
|       - | 2962 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      40 | 2963 | `	if( nArg != 1 ){` |
|       7 | 2964 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2965 | `			"ArgumentCountError",` |
|       - | 2966 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2967 | `			nArg` |
|       - | 2968 | `			);` |
|       - | 2969 | `	}` |
|       - | 2970 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      36 | 2971 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2972 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2973 | `			"Error",` |
|       - | 2974 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2975 | `			);` |
|       - | 2976 | `	}` |
|       - | 2977 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 2978 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2979 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2980 | `			"TypeError",` |
|       - | 2981 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2982 | `			ph7_type_name(apArg[0])` |
|       - | 2983 | `			);` |
|       - | 2984 | `	}` |
|       - | 2985 | `	/* Point to the internal representation of the hashmap */` |
|      30 | 2986 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      30 | 2987 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      30 | 2988 | `	if( pMap->nEntry < 1 ){` |
|       - | 2989 | `		/* Empty hashmap,return NULL */` |
|       3 | 2990 | `		ph7_result_null(pCtx);` |
|       2 | 2991 | `	}else{` |
|      28 | 2992 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2993 | `		ph7_value *pObj;` |
|       - | 2994 | `		sxu32 n;` |
|      28 | 2995 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      28 | 2996 | `		if( pObj ){` |
|       - | 2997 | `			/* Node value */` |
|      28 | 2998 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2999 | `			/* Unlink the first node */` |
|      28 | 3000 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      15 | 3001 | `		}else{` |
|     ! 0 | 3002 | `			ph7_result_null(pCtx);` |
|       - | 3003 | `		}` |
|       - | 3004 | `		/* Rehash all int keys */` |
|      28 | 3005 | `		n = pMap->nEntry;` |
|      28 | 3006 | `		pEntry = pMap->pFirst;` |
|      28 | 3007 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 3008 | `		for(;;){` |
|      82 | 3009 | `			if( n < 1 ){` |
|      28 | 3010 | `				break;` |
|       - | 3011 | `			}` |
|      56 | 3012 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      56 | 3013 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 3014 | `			}` |
|       - | 3015 | `			/* Point to the next entry */` |
|      56 | 3016 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      56 | 3017 | `			n--;` |
|       2 | 3018 | `		}` |
|       - | 3019 | `		/* Reset the cursor */` |
|      28 | 3020 | `		pMap->pCur = pMap->pFirst;` |
|       - | 3021 | `	}` |
|      30 | 3022 | `	return PH7_OK;` |
|      21 | 3023 |  |
|       - | 3024 | `/*` |
|       - | 3025 | ` * Extract the node cursor value.` |
|       - | 3026 | ` */` |
|      24 | 3027 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 3028 |  |
|      25 | 3029 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 3030 | `	ph7_value *pVal;` |
|      25 | 3031 | `	if( pCur == 0 ){` |
|       - | 3032 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 3033 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3034 | `		return PH7_OK;` |
|       - | 3035 | `	}` |
|      25 | 3036 | `	if( iDirection != 0 ){` |
|       9 | 3037 | `		if( iDirection > 0 ){` |
|       - | 3038 | `			/* Point to the next entry */` |
|       7 | 3039 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 3040 | `			pCur = pMap->pCur;` |
|       4 | 3041 | `		}else{` |
|       - | 3042 | `			/* Point to the previous entry */` |
|       3 | 3043 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 3044 | `			pCur = pMap->pCur;` |
|       - | 3045 | `		}` |
|       9 | 3046 | `		if( pCur == 0 ){` |
|       - | 3047 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 3048 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3049 | `			return PH7_OK;` |
|       - | 3050 | `		}` |
|       4 | 3051 | `	}` |
|       - | 3052 | `	/* Point to the desired element */` |
|      25 | 3053 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 3054 | `	if( pVal ){` |
|      25 | 3055 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 3056 | `	}else{` |
|     ! 0 | 3057 | `		ph7_result_bool(pCtx,0);` |
|       - | 3058 | `	}` |
|      25 | 3059 | `	return PH7_OK;` |
|      13 | 3060 |  |
|       - | 3061 | `/*` |
|       - | 3062 | ` * value current(array $array)` |
|       - | 3063 | ` *  Return the current element in an array.` |
|       - | 3064 | ` * Parameter` |
|       - | 3065 | ` *  $input: The input array.` |
|       - | 3066 | ` * Return` |
|       - | 3067 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 3068 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3069 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3070 | ` *  is empty, current() returns FALSE.` |
|       - | 3071 | ` */` |
|      10 | 3072 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3073 |  |
|      11 | 3074 | `	if( nArg < 1 ){` |
|       - | 3075 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3076 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3077 | `		return PH7_OK;` |
|       - | 3078 | `	}` |
|       - | 3079 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 3080 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3081 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3082 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3083 | `		return PH7_OK;` |
|       - | 3084 | `	}` |
|      11 | 3085 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 3086 | `	return PH7_OK;` |
|       6 | 3087 |  |
|       - | 3088 | `/*` |
|       - | 3089 | ` * value next(array $input)` |
|       - | 3090 | ` *  Advance the internal array pointer of an array.` |
|       - | 3091 | ` * Parameter` |
|       - | 3092 | ` *  $input: The input array.` |
|       - | 3093 | ` * Return` |
|       - | 3094 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 3095 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 3096 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 3097 | ` */` |
|       6 | 3098 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3099 |  |
|       7 | 3100 | `	if( nArg < 1 ){` |
|       - | 3101 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3102 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3103 | `		return PH7_OK;` |
|       - | 3104 | `	}` |
|       - | 3105 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 3106 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3107 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3108 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3109 | `		return PH7_OK;` |
|       - | 3110 | `	}` |
|       7 | 3111 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 3112 | `	return PH7_OK;` |
|       4 | 3113 |  |
|       - | 3114 | `/*` |
|       - | 3115 | ` * value prev(array $input)` |
|       - | 3116 | ` *  Rewind the internal array pointer.` |
|       - | 3117 | ` * Parameter` |
|       - | 3118 | ` *  $input: The input array.` |
|       - | 3119 | ` * Return` |
|       - | 3120 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3121 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3122 | ` *  elements.` |
|       - | 3123 | ` */` |
|       2 | 3124 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3125 |  |
|       3 | 3126 | `	if( nArg < 1 ){` |
|       - | 3127 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3128 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3129 | `		return PH7_OK;` |
|       - | 3130 | `	}` |
|       - | 3131 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3132 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3133 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3134 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3135 | `		return PH7_OK;` |
|       - | 3136 | `	}` |
|       3 | 3137 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3138 | `	return PH7_OK;` |
|       2 | 3139 |  |
|       - | 3140 | `/*` |
|       - | 3141 | ` * value end(array $input)` |
|       - | 3142 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3143 | ` * Parameter` |
|       - | 3144 | ` *  $input: The input array.` |
|       - | 3145 | ` * Return` |
|       - | 3146 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3147 | ` */` |
|       2 | 3148 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3149 |  |
|       - | 3150 | `	ph7_hashmap *pMap;` |
|       3 | 3151 | `	if( nArg < 1 ){` |
|       - | 3152 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3153 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3154 | `		return PH7_OK;` |
|       - | 3155 | `	}` |
|       - | 3156 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3157 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3158 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3159 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3160 | `		return PH7_OK;` |
|       - | 3161 | `	}` |
|       - | 3162 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3163 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3164 | `	/* Point to the last node */` |
|       3 | 3165 | `	pMap->pCur = pMap->pLast;` |
|       - | 3166 | `	/* Return the last node value */` |
|       3 | 3167 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3168 | `	return PH7_OK;` |
|       2 | 3169 |  |
|       - | 3170 | `/*` |
|       - | 3171 | ` * value reset(array $array )` |
|       - | 3172 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3173 | ` * Parameter` |
|       - | 3174 | ` *  $input: The input array.` |
|       - | 3175 | ` * Return` |
|       - | 3176 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3177 | ` */` |
|       4 | 3178 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3179 |  |
|       - | 3180 | `	ph7_hashmap *pMap;` |
|       5 | 3181 | `	if( nArg < 1 ){` |
|       - | 3182 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3183 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3184 | `		return PH7_OK;` |
|       - | 3185 | `	}` |
|       - | 3186 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3187 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3188 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3189 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3190 | `		return PH7_OK;` |
|       - | 3191 | `	}` |
|       - | 3192 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3193 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3194 | `	/* Point to the first node */` |
|       5 | 3195 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3196 | `	/* Return the last node value if available */` |
|       5 | 3197 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3198 | `	return PH7_OK;` |
|       3 | 3199 |  |
|       - | 3200 | `/*` |
|       - | 3201 | ` * value key(array $array)` |
|       - | 3202 | ` *   Fetch a key from an array` |
|       - | 3203 | ` * Parameter` |
|       - | 3204 | ` *  $input` |
|       - | 3205 | ` *   The input array.` |
|       - | 3206 | ` * Return` |
|       - | 3207 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3208 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3209 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3210 | ` *  is empty, key() returns NULL.` |
|       - | 3211 | ` */` |
|       4 | 3212 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3213 |  |
|       - | 3214 | `	ph7_hashmap_node *pCur;` |
|       - | 3215 | `	ph7_hashmap *pMap;` |
|       5 | 3216 | `	if( nArg < 1 ){` |
|       - | 3217 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3218 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3219 | `		return PH7_OK;` |
|       - | 3220 | `	}` |
|       - | 3221 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3222 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3223 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3224 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3225 | `		return PH7_OK;` |
|       - | 3226 | `	}` |
|       5 | 3227 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3228 | `	pCur = pMap->pCur;` |
|       5 | 3229 | `	if( pCur == 0 ){` |
|       - | 3230 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3231 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3232 | `		return PH7_OK;` |
|       - | 3233 | `	}` |
|       5 | 3234 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3235 | `		/* Key is integer */` |
|     ! 0 | 3236 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3237 | `	}else{` |
|       - | 3238 | `		/* Key is blob */` |
|       7 | 3239 | `		ph7_result_string(pCtx,` |
|       4 | 3240 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3241 | `	}` |
|       5 | 3242 | `	return PH7_OK;` |
|       3 | 3243 |  |
|       - | 3244 | `/*` |
|       - | 3245 | ` * array each(array $input)` |
|       - | 3246 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3247 | ` * Parameter` |
|       - | 3248 | ` *  $input` |
|       - | 3249 | ` *    The input array.` |
|       - | 3250 | ` * Return` |
|       - | 3251 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3252 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3253 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3254 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3255 | ` *  each() returns FALSE.` |
|       - | 3256 | ` */` |
|      22 | 3257 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3258 |  |
|       - | 3259 | `	ph7_hashmap_node *pCur;` |
|       - | 3260 | `	ph7_hashmap *pMap;` |
|       - | 3261 | `	ph7_value *pArray;` |
|       - | 3262 | `	ph7_value *pVal;` |
|       - | 3263 | `	ph7_value sKey;` |
|      23 | 3264 | `	if( nArg < 1 ){` |
|       - | 3265 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3266 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3267 | `		return PH7_OK;` |
|       - | 3268 | `	}` |
|       - | 3269 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3270 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3271 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3272 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3273 | `		return PH7_OK;` |
|       - | 3274 | `	}` |
|       - | 3275 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3276 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3277 | `	if( pMap->pCur == 0 ){` |
|       - | 3278 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3279 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3280 | `		return PH7_OK;` |
|       - | 3281 | `	}` |
|      15 | 3282 | `	pCur = pMap->pCur;` |
|       - | 3283 | `	/* Create a new array */` |
|      15 | 3284 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3285 | `	if( pArray == 0 ){` |
|     ! 0 | 3286 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3287 | `		return PH7_OK;` |
|       - | 3288 | `	}` |
|      15 | 3289 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3290 | `	/* Insert the current value */` |
|      15 | 3291 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3292 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3293 | `	/* Make the key */` |
|      15 | 3294 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3295 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3296 | `	}else{` |
|       9 | 3297 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3298 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3299 | `	}` |
|       - | 3300 | `	/* Insert the current key */` |
|      15 | 3301 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3302 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3303 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3304 | `	/* Advance the cursor */` |
|      15 | 3305 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3306 | `	/* Return the current entry */` |
|      15 | 3307 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3308 | `	return PH7_OK;` |
|      12 | 3309 |  |
|       - | 3310 | `/*` |
|       - | 3311 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3312 | ` *  Create an array containing a range of elements` |
|       - | 3313 | ` * Parameter` |
|       - | 3314 | ` *  start` |
|       - | 3315 | ` *   First value of the sequence.` |
|       - | 3316 | ` *  limit` |
|       - | 3317 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3318 | ` *  step` |
|       - | 3319 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3320 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3321 | ` * Return` |
|       - | 3322 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3323 | ` * NOTE:` |
|       - | 3324 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3325 | ` */` |
|       2 | 3326 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3327 |  |
|       - | 3328 | `	ph7_value *pValue,*pArray;` |
|       - | 3329 | `	sxi64 iOfft,iLimit;` |
|       3 | 3330 | `	int iStep = 1;` |
|       - | 3331 |  |
|       3 | 3332 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3333 | `	if( nArg > 0 ){` |
|       - | 3334 | `		/* Extract the offset */` |
|       3 | 3335 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3336 | `		if( nArg > 1 ){` |
|       - | 3337 | `			/* Extract the limit */` |
|       3 | 3338 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3339 | `			if( nArg > 2 ){` |
|       - | 3340 | `				/* Extract the increment */` |
|       3 | 3341 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3342 | `				if( iStep < 1 ){` |
|       - | 3343 | `					/* Only positive number are allowed */` |
|       3 | 3344 | `					iStep = 1;` |
|       1 | 3345 | `				}` |
|       1 | 3346 | `			}` |
|       1 | 3347 | `		}` |
|       1 | 3348 | `	}` |
|       - | 3349 | `	/* Element container */` |
|       3 | 3350 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3351 | `	/* Create the new array */` |
|       3 | 3352 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3353 | `	if( pArray == 0 ){` |
|     ! 0 | 3354 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3355 | `	}` |
|       - | 3356 | `	/* Start filling */` |
|       3 | 3357 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3358 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3359 | `		/* Perform the insertion */` |
|     ! 0 | 3360 | `		if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       - | 3361 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3362 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3363 | `		}` |
|       - | 3364 | `		/* Increment */` |
|     ! 0 | 3365 | `		iOfft += iStep;` |
|     ! 0 | 3366 | `	}` |
|       - | 3367 | `	/* Return the new array */` |
|       3 | 3368 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3369 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3370 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3371 | `	 */` |
|       3 | 3372 | `	return PH7_OK;` |
|       2 | 3373 |  |
|       - | 3374 | `/*` |
|       - | 3375 | ` * array array_values(array $array)` |
|       - | 3376 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3377 | ` * Parameters` |
|       - | 3378 | ` *  $array` |
|       - | 3379 | ` *   The input array.` |
|       - | 3380 | ` * Return` |
|       - | 3381 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3382 | ` */` |
|      30 | 3383 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3384 |  |
|       - | 3385 | `	ph7_hashmap_node *pNode;` |
|       - | 3386 | `	ph7_hashmap *pMap;` |
|       - | 3387 | `	ph7_value *pArray;` |
|       - | 3388 | `	ph7_value *pObj;` |
|       - | 3389 | `	sxu32 n;` |
|      32 | 3390 | `	if( nArg != 1 ){` |
|       - | 3391 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3392 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3393 | `			"ArgumentCountError",` |
|       - | 3394 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3395 | `			nArg` |
|       - | 3396 | `			);` |
|       - | 3397 | `	}` |
|       - | 3398 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3399 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3400 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3401 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3402 | `			"TypeError",` |
|       - | 3403 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3404 | `			ph7_type_name(apArg[0])` |
|       - | 3405 | `			);` |
|       - | 3406 | `	}` |
|       - | 3407 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3408 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3409 | `	/* Create a new array */` |
|      25 | 3410 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3411 | `	if( pArray == 0 ){` |
|     ! 0 | 3412 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3413 | `		return PH7_OK;` |
|       - | 3414 | `	}` |
|       - | 3415 | `	/* Perform the requested operation */` |
|      25 | 3416 | `	pNode = pMap->pFirst;` |
|      83 | 3417 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3418 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3419 | `		if( pObj ){` |
|       - | 3420 | `			/* perform the insertion */` |
|      59 | 3421 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3422 | `		}` |
|       - | 3423 | `		/* Point to the next entry */` |
|      59 | 3424 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3425 | `	}` |
|       - | 3426 | `	/* return the new array */` |
|      25 | 3427 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3428 | `	return PH7_OK;` |
|      17 | 3429 |  |
|       - | 3430 | `/*` |
|       - | 3431 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3432 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3433 | ` * Parameters` |
|       - | 3434 | ` *  $input` |
|       - | 3435 | ` *   An array containing keys to return.` |
|       - | 3436 | ` * $search_value` |
|       - | 3437 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3438 | ` * $strict` |
|       - | 3439 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3440 | ` * Return` |
|       - | 3441 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3442 | ` */` |
|     122 | 3443 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3444 |  |
|       - | 3445 | `	ph7_hashmap_node *pNode;` |
|       - | 3446 | `	ph7_hashmap *pMap;` |
|       - | 3447 | `	ph7_value *pArray;` |
|       - | 3448 | `	ph7_value sObj;` |
|       - | 3449 | `	ph7_value sVal;` |
|       - | 3450 | `	SyString sKey;` |
|       - | 3451 | `	int bStrict;` |
|       - | 3452 | `	sxi32 rc;` |
|       - | 3453 | `	sxu32 n;` |
|     124 | 3454 | `	if( nArg < 1 ){` |
|       - | 3455 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3456 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3457 | `			"ArgumentCountError",` |
|       - | 3458 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3459 | `			);` |
|       - | 3460 | `	}` |
|       - | 3461 | `	/* Make sure we are dealing with a valid hashmap */` |
|     122 | 3462 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3463 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3464 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3465 | `			"TypeError",` |
|       - | 3466 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3467 | `			ph7_type_name(apArg[0])` |
|       - | 3468 | `			);` |
|       - | 3469 | `	}` |
|       - | 3470 | `	/* Point to the internal representation of the input hashmap */` |
|     120 | 3471 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3472 | `	/* Create a new array */` |
|     120 | 3473 | `	pArray = ph7_context_new_array(pCtx);` |
|     120 | 3474 | `	if( pArray == 0 ){` |
|     ! 0 | 3475 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3476 | `		return PH7_OK;` |
|       - | 3477 | `	}` |
|     120 | 3478 | `	bStrict = FALSE;` |
|     120 | 3479 | `	if( nArg > 2 ){` |
|       - | 3480 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3481 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3482 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3483 | `				"TypeError",` |
|       - | 3484 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3485 | `				ph7_type_name(apArg[2])` |
|       - | 3486 | `				);` |
|       - | 3487 | `		}` |
|       5 | 3488 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3489 | `	}` |
|       - | 3490 | `	/* Perform the requested operation */` |
|     117 | 3491 | `	pNode = pMap->pFirst;` |
|     117 | 3492 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     559 | 3493 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     443 | 3494 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     121 | 3495 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      61 | 3496 | `		}else{` |
|     323 | 3497 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3498 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3499 | `		}` |
|     443 | 3500 | `		rc = 0;` |
|     443 | 3501 | `		if( nArg > 1 ){` |
|      31 | 3502 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3503 | `			if( pValue ){` |
|      31 | 3504 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3505 | `				/* Filter key */` |
|      31 | 3506 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3507 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3508 | `			}` |
|      15 | 3509 | `		}` |
|     443 | 3510 | `		if( rc == 0 ){` |
|       - | 3511 | `			/* Perform the insertion */` |
|     425 | 3512 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     212 | 3513 | `		}` |
|     443 | 3514 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3515 | `		/* Point to the next entry */` |
|     443 | 3516 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     222 | 3517 | `	}` |
|       - | 3518 | `	/* return the new array */` |
|     117 | 3519 | `	ph7_result_value(pCtx,pArray);` |
|     117 | 3520 | `	return PH7_OK;` |
|      63 | 3521 |  |
|       - | 3522 | `/*` |
|       - | 3523 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3524 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3525 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3526 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3527 | ` * Parameters` |
|       - | 3528 | ` *  $arr1` |
|       - | 3529 | ` *   First array` |
|       - | 3530 | ` *  $arr2` |
|       - | 3531 | ` *   Second array` |
|       - | 3532 | ` * Return` |
|       - | 3533 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3534 | ` * Note` |
|       - | 3535 | ` *  This function is a symisc eXtension.` |
|       - | 3536 | ` */` |
|       4 | 3537 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3538 |  |
|       - | 3539 | `	ph7_hashmap *p1,*p2;` |
|       - | 3540 | `	int rc;` |
|       5 | 3541 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3542 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3543 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3544 | `		return PH7_OK;` |
|       - | 3545 | `	}` |
|       - | 3546 | `	/* Point to the hashmaps */` |
|       5 | 3547 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3548 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3549 | `	rc = (p1 == p2);` |
|       - | 3550 | `	/* Same instance? */` |
|       5 | 3551 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3552 | `	return PH7_OK;` |
|       3 | 3553 |  |
|       - | 3554 | `/*` |
|       - | 3555 | ` * array array_merge(array ...$arrays)` |
|       - | 3556 | ` *  Merge one or more arrays.` |
|       - | 3557 | ` * Parameters` |
|       - | 3558 | ` *  ...$arrays` |
|       - | 3559 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3560 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3561 | ` * Return` |
|       - | 3562 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3563 | ` *  with no arguments.` |
|       - | 3564 | ` */` |
|     986 | 3565 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3566 |  |
|       - | 3567 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3568 | `	ph7_value *pArray;` |
|       - | 3569 | `	int i;` |
|       - | 3570 | `	/* Create a new array */` |
|     988 | 3571 | `	pArray = ph7_context_new_array(pCtx);` |
|     988 | 3572 | `	if( pArray == 0 ){` |
|     ! 0 | 3573 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3574 | `		return PH7_OK;` |
|       - | 3575 | `	}` |
|       - | 3576 | `	/* Point to the internal representation of the hashmap */` |
|     988 | 3577 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3578 | `	/* Start merging */` |
|    2950 | 3579 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3580 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1968 | 3581 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3582 | `			/* Type mismatch -> TypeError */` |
|       7 | 3583 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3584 | `				"TypeError",` |
|       - | 3585 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3586 | `				i + 1,` |
|       4 | 3587 | `				ph7_type_name(apArg[i])` |
|       - | 3588 | `				);` |
|     ! 0 | 3589 | `		}else{` |
|    1964 | 3590 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3591 | `			/* Merge the two hashmaps */` |
|    1964 | 3592 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3593 | `		}` |
|     983 | 3594 | `	}` |
|       - | 3595 | `	/* Return the freshly created array */` |
|     984 | 3596 | `	ph7_result_value(pCtx,pArray);` |
|     984 | 3597 | `	return PH7_OK;` |
|     495 | 3598 |  |
|       - | 3599 | `/*` |
|       - | 3600 | ` * array array_copy(array $source)` |
|       - | 3601 | ` *  Make a blind copy of the target array.` |
|       - | 3602 | ` * Parameters` |
|       - | 3603 | ` *  $source` |
|       - | 3604 | ` *   Target array` |
|       - | 3605 | ` * Return` |
|       - | 3606 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3607 | ` * Note` |
|       - | 3608 | ` *  This function is a symisc eXtension.` |
|       - | 3609 | ` */` |
|      16 | 3610 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3611 |  |
|       - | 3612 | `	ph7_hashmap *pMap;` |
|       - | 3613 | `	ph7_value *pArray;` |
|      17 | 3614 | `	if( nArg < 1 ){` |
|       - | 3615 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3616 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3617 | `		return PH7_OK;` |
|       - | 3618 | `	}` |
|       - | 3619 | `	/* Create a new array */` |
|      17 | 3620 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3621 | `	if( pArray == 0 ){` |
|     ! 0 | 3622 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3623 | `		return PH7_OK;` |
|       - | 3624 | `	}` |
|       - | 3625 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3626 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3627 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3628 | `		/* Point to the internal representation of the source */` |
|      17 | 3629 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3630 | `		/* Perform the copy */` |
|      17 | 3631 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3632 | `	}else{` |
|       - | 3633 | `		/* Simple insertion */` |
|     ! 0 | 3634 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3635 | `	}` |
|       - | 3636 | `	/* Return the duplicated array */` |
|      17 | 3637 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3638 | `	return PH7_OK;` |
|       9 | 3639 |  |
|       - | 3640 | `/*` |
|       - | 3641 | ` * bool array_erase(array $source)` |
|       - | 3642 | ` *  Remove all elements from a given array.` |
|       - | 3643 | ` * Parameters` |
|       - | 3644 | ` *  $source` |
|       - | 3645 | ` *   Target array` |
|       - | 3646 | ` * Return` |
|       - | 3647 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3648 | ` * Note` |
|       - | 3649 | ` *  This function is a symisc eXtension.` |
|       - | 3650 | ` */` |
|      16 | 3651 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3652 |  |
|       - | 3653 | `	ph7_hashmap *pMap;` |
|      17 | 3654 | `	if( nArg < 1 ){` |
|       - | 3655 | `		/* Missing arguments */` |
|     ! 0 | 3656 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3657 | `		return PH7_OK;` |
|       - | 3658 | `	}` |
|       - | 3659 | `	/* Point to the target hashmap */` |
|      17 | 3660 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3661 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3662 | `	/* Erase */` |
|      17 | 3663 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3664 | `	return PH7_OK;` |
|       9 | 3665 |  |
|       - | 3666 | `/*` |
|       - | 3667 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3668 | ` *  Extract a slice of the array.` |
|       - | 3669 | ` * Parameters` |
|       - | 3670 | ` *  $array` |
|       - | 3671 | ` *    The input array.` |
|       - | 3672 | ` * $offset` |
|       - | 3673 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3674 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3675 | ` * $length (optional, nullable)` |
|       - | 3676 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3677 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3678 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3679 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3680 | ` * $preserve_keys (optional)` |
|       - | 3681 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3682 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3683 | ` * Return` |
|       - | 3684 | ` *   The new slice.` |
|       - | 3685 | ` */` |
|      46 | 3686 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3687 |  |
|       - | 3688 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3689 | `	ph7_hashmap_node *pCur;` |
|       - | 3690 | `	ph7_value *pArray;` |
|       - | 3691 | `	int iLength,iOfft;` |
|       - | 3692 | `	int bPreserve;` |
|       - | 3693 | `	sxi32 rc;` |
|      48 | 3694 | `	if( nArg < 2 ){` |
|       7 | 3695 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3696 | `			"ArgumentCountError",` |
|       - | 3697 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3698 | `			nArg` |
|       - | 3699 | `			);` |
|       - | 3700 | `	}` |
|      44 | 3701 | `	if( nArg > 4 ){` |
|       4 | 3702 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3703 | `			"ArgumentCountError",` |
|       - | 3704 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3705 | `			nArg` |
|       - | 3706 | `			);` |
|       - | 3707 | `	}` |
|      42 | 3708 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3709 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3710 | `			"TypeError",` |
|       - | 3711 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3712 | `			ph7_type_name(apArg[0])` |
|       - | 3713 | `			);` |
|       - | 3714 | `	}` |
|       - | 3715 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3716 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3717 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3718 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3719 | `			"TypeError",` |
|       - | 3720 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3721 | `			ph7_type_name(apArg[1])` |
|       - | 3722 | `			);` |
|       - | 3723 | `	}` |
|       - | 3724 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3725 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3726 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3727 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3728 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3729 | `				"TypeError",` |
|       - | 3730 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3731 | `				ph7_type_name(apArg[2])` |
|       - | 3732 | `				);` |
|       - | 3733 | `		}` |
|       8 | 3734 | `	}` |
|       - | 3735 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3736 | `	if( nArg > 3 ){` |
|      10 | 3737 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3738 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3739 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3740 | `				"TypeError",` |
|       - | 3741 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3742 | `				ph7_type_name(apArg[3])` |
|       - | 3743 | `				);` |
|       - | 3744 | `		}` |
|       2 | 3745 | `	}` |
|       - | 3746 | `	/* Point the internal representation of the target array */` |
|      33 | 3747 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3748 | `	bPreserve = FALSE;` |
|       - | 3749 | `	/* Get the offset */` |
|      33 | 3750 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3751 | `	if( iOfft < 0 ){` |
|       5 | 3752 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3753 | `		if( iOfft < 0 ){` |
|       3 | 3754 | `			iOfft = 0;` |
|       1 | 3755 | `		}` |
|       2 | 3756 | `	}` |
|      33 | 3757 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3758 | `		/* Offset past end of array, return empty array */` |
|       5 | 3759 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3760 | `		if( pArray == 0 ){` |
|     ! 0 | 3761 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3762 | `			return PH7_OK;` |
|       - | 3763 | `		}` |
|       5 | 3764 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3765 | `		return PH7_OK;` |
|       - | 3766 | `	}` |
|       - | 3767 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3768 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3769 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3770 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3771 | `		if( iLength < 0 ){` |
|       5 | 3772 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3773 | `		}` |
|      15 | 3774 | `		if( iLength < 0 ){` |
|       3 | 3775 | `			iLength = 0;` |
|       1 | 3776 | `		}` |
|      15 | 3777 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3778 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3779 | `		}` |
|       7 | 3780 | `	}` |
|      29 | 3781 | `	if( nArg > 3 ){` |
|       5 | 3782 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3783 | `	}` |
|       - | 3784 | `	/* Create a new array */` |
|      29 | 3785 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3786 | `	if( pArray == 0 ){` |
|     ! 0 | 3787 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3788 | `		return PH7_OK;` |
|       - | 3789 | `	}` |
|      29 | 3790 | `	if( iLength < 1 ){` |
|       - | 3791 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3792 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3793 | `		return PH7_OK;` |
|       - | 3794 | `	}` |
|       - | 3795 | `	/* Point to the desired entry */` |
|      25 | 3796 | `	pCur = pSrc->pFirst;` |
|      24 | 3797 | `	for(;;){` |
|      49 | 3798 | `		if( iOfft < 1 ){` |
|      25 | 3799 | `			break;` |
|       - | 3800 | `		}` |
|       - | 3801 | `		/* Point to the next entry */` |
|      25 | 3802 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3803 | `		iOfft--;` |
|       1 | 3804 | `	}` |
|       - | 3805 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3806 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3807 | `	for(;;){` |
|      79 | 3808 | `		if( iLength < 1 ){` |
|      25 | 3809 | `			break;` |
|       - | 3810 | `		}` |
|       - | 3811 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3812 | `		{` |
|      55 | 3813 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3814 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3815 | `		}` |
|      55 | 3816 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3817 | `			break;` |
|       - | 3818 | `		}` |
|       - | 3819 | `		/* Point to the next entry */` |
|      55 | 3820 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3821 | `		iLength--;` |
|       1 | 3822 | `	}` |
|       - | 3823 | `	/* Return the freshly created array */` |
|      25 | 3824 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3825 | `	return PH7_OK;` |
|      25 | 3826 |  |
|       - | 3827 | `/*` |
|       - | 3828 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3829 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3830 | ` * beginning (becomes the new pFirst).` |
|       - | 3831 | ` */` |
|      30 | 3832 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3833 |  |
|       - | 3834 | `	ph7_hashmap_node *pNode;` |
|       - | 3835 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3836 | `	pNode = pMap->pLast;` |
|      31 | 3837 | `	if( pNode == 0 ){` |
|     ! 0 | 3838 | `		return;` |
|       - | 3839 | `	}` |
|      31 | 3840 | `	if( pNode->pNext == 0 ){` |
|       - | 3841 | `		/* Only node in the list, nothing to move */` |
|       5 | 3842 | `		return;` |
|       - | 3843 | `	}` |
|      27 | 3844 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3845 | `		/* Already in the correct position */` |
|       9 | 3846 | `		return;` |
|       - | 3847 | `	}` |
|       - | 3848 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3849 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3850 | `	pMap->pLast->pPrev = 0;` |
|       - | 3851 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3852 | `	if( pAfter == 0 ){` |
|       - | 3853 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3854 | `		pNode->pNext = 0;` |
|       3 | 3855 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3856 | `		if( pMap->pFirst ){` |
|       3 | 3857 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3858 | `		}` |
|       3 | 3859 | `		pMap->pFirst = pNode;` |
|       2 | 3860 | `	}else{` |
|      17 | 3861 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3862 | `		pNode->pPrev = pOldNext;` |
|      17 | 3863 | `		pNode->pNext = pAfter;` |
|      17 | 3864 | `		pAfter->pPrev = pNode;` |
|      17 | 3865 | `		if( pOldNext ){` |
|      17 | 3866 | `			pOldNext->pNext = pNode;` |
|       9 | 3867 | `		}else{` |
|     ! 0 | 3868 | `			pMap->pLast = pNode;` |
|       - | 3869 | `		}` |
|       - | 3870 | `	}` |
|      16 | 3871 |  |
|       - | 3872 | `/*` |
|       - | 3873 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3874 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3875 | ` * Parameters` |
|       - | 3876 | ` *  $array` |
|       - | 3877 | ` *    The input array.` |
|       - | 3878 | ` *  $offset` |
|       - | 3879 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3880 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3881 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3882 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3883 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3884 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3885 | ` *  $length (optional)` |
|       - | 3886 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3887 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3888 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3889 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3890 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3891 | ` *  $replacement (optional)` |
|       - | 3892 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3893 | ` *    with elements from this array.` |
|       - | 3894 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3895 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3896 | ` *    offset.` |
|       - | 3897 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3898 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3899 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3900 | ` * Return` |
|       - | 3901 | ` *   A new array consisting of the extracted elements.` |
|       - | 3902 | ` */` |
|      54 | 3903 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3904 |  |
|       - | 3905 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3906 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3907 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3908 | `	int iLength,iOfft,i;` |
|       - | 3909 | `	sxi32 rc;` |
|      56 | 3910 | `	if( nArg < 2 ){` |
|       7 | 3911 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3912 | `			"ArgumentCountError",` |
|       - | 3913 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3914 | `			nArg` |
|       - | 3915 | `			);` |
|       - | 3916 | `	}` |
|      52 | 3917 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3918 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3919 | `			"TypeError",` |
|       - | 3920 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3921 | `			ph7_type_name(apArg[0])` |
|       - | 3922 | `			);` |
|       - | 3923 | `	}` |
|       - | 3924 | `	/* Point to the internal representation of the target array */` |
|      49 | 3925 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3926 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3927 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3928 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3929 | `	if( iOfft < 0 ){` |
|       7 | 3930 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3931 | `		if( iOfft < 0 ){` |
|       3 | 3932 | `			iOfft = 0;` |
|       2 | 3933 | `		}` |
|      46 | 3934 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3935 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3936 | `	}` |
|       - | 3937 | `	/* Get the length and clamp to valid range.` |
|       - | 3938 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3939 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3940 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3941 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3942 | `		if( iLength < 0 ){` |
|       7 | 3943 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3944 | `			if( iLength < 0 ){` |
|       3 | 3945 | `				iLength = 0;` |
|       1 | 3946 | `			}` |
|       3 | 3947 | `		}` |
|      31 | 3948 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3949 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3950 | `		}` |
|      15 | 3951 | `	}` |
|       - | 3952 | `	/* Create the result array for removed elements */` |
|      49 | 3953 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3954 | `	if( pArray == 0 ){` |
|     ! 0 | 3955 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3956 | `		return PH7_OK;` |
|       - | 3957 | `	}` |
|       - | 3958 | `	/* Get replacement array if provided */` |
|      49 | 3959 | `	pRep = 0;` |
|      49 | 3960 | `	if( nArg > 3 ){` |
|      21 | 3961 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3962 | `			/* Perform an array cast */` |
|       3 | 3963 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3964 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3965 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3966 | `			}` |
|       2 | 3967 | `		}else{` |
|      19 | 3968 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3969 | `		}` |
|      21 | 3970 | `		if( pRep ){` |
|       - | 3971 | `			/* Reset the loop cursor */` |
|      21 | 3972 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3973 | `		}` |
|      10 | 3974 | `	}` |
|       - | 3975 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3976 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3977 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3978 | `		return PH7_OK;` |
|       - | 3979 | `	}` |
|       - | 3980 | `	/* Navigate to the offset position */` |
|      41 | 3981 | `	pCur = pSrc->pFirst;` |
|      85 | 3982 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3983 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3984 | `	}` |
|       - | 3985 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3986 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3987 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3988 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3989 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3990 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3991 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3992 | `		pPrev = pCur->pPrev;` |
|      71 | 3993 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3994 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3995 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3996 | `			break;` |
|       - | 3997 | `		}` |
|      71 | 3998 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3999 | `	}` |
|       - | 4000 | `	/* Insert replacement elements at the correct position */` |
|      41 | 4001 | `	if( pRep ){` |
|       - | 4002 | `		ph7_value sSafeVal;` |
|      61 | 4003 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 4004 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 4005 | `			if( pRvalue ){` |
|       - | 4006 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 4007 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 4008 | `				 * since it points into that same pool. */` |
|      31 | 4009 | `				sSafeVal = *pRvalue;` |
|      31 | 4010 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 4011 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 4012 | `					pNewNode = pSrc->pLast;` |
|      31 | 4013 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 4014 | `					pInsertAfter = pNewNode;` |
|      15 | 4015 | `				}` |
|      15 | 4016 | `			}` |
|       1 | 4017 | `		}` |
|      10 | 4018 | `	}` |
|       - | 4019 | `	/* Return the freshly created array */` |
|      41 | 4020 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 4021 | `	return PH7_OK;` |
|      29 | 4022 |  |
|       - | 4023 | `/*` |
|       - | 4024 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 4025 | ` *  Checks if a value exists in an array.` |
|       - | 4026 | ` * Parameters` |
|       - | 4027 | ` *  $needle` |
|       - | 4028 | ` *   The searched value.` |
|       - | 4029 | ` *   Note:` |
|       - | 4030 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 4031 | ` * $haystack` |
|       - | 4032 | ` *  The target array.` |
|       - | 4033 | ` * $strict` |
|       - | 4034 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 4035 | ` *  will also check the types of the needle in the haystack.` |
|       - | 4036 | ` */` |
|   29468 | 4037 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4038 |  |
|       - | 4039 | `	ph7_value *pNeedle;` |
|       - | 4040 | `	int bStrict;` |
|       - | 4041 | `	int rc;` |
|   29470 | 4042 | `	if( nArg < 2 ){` |
|       - | 4043 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 4044 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4045 | `		return PH7_OK;` |
|       - | 4046 | `	}` |
|   29470 | 4047 | `	pNeedle = apArg[0];` |
|   29470 | 4048 | `	bStrict = 0;` |
|   29470 | 4049 | `	if( nArg > 2 ){` |
|       5 | 4050 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 4051 | `	}` |
|   29470 | 4052 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4053 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 4054 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 4055 | `		/* Set the comparison result */` |
|     ! 0 | 4056 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 4057 | `		return PH7_OK;` |
|       - | 4058 | `	}` |
|       - | 4059 | `	/* Perform the lookup */` |
|   29470 | 4060 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 4061 | `	/* Lookup result */` |
|   29470 | 4062 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   29470 | 4063 | `	return PH7_OK;` |
|   14736 | 4064 |  |
|       - | 4065 | `/*` |
|       - | 4066 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 4067 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 4068 | ` * Parameters` |
|       - | 4069 | ` * $needle` |
|       - | 4070 | ` *   The searched value.` |
|       - | 4071 | ` * $haystack` |
|       - | 4072 | ` *   The array.` |
|       - | 4073 | ` * $strict` |
|       - | 4074 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 4075 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 4076 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 4077 | ` * Return` |
|       - | 4078 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 4079 | ` */` |
|      28 | 4080 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4081 |  |
|       - | 4082 | `	ph7_hashmap_node *pEntry;` |
|       - | 4083 | `	ph7_value *pVal,sNeedle;` |
|       - | 4084 | `	ph7_hashmap *pMap;` |
|       - | 4085 | `	ph7_value sVal;` |
|       - | 4086 | `	int bStrict;` |
|       - | 4087 | `	sxu32 n;` |
|       - | 4088 | `	int rc;` |
|      30 | 4089 | `	if( nArg < 2 ){` |
|       - | 4090 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 4091 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4092 | `			"ArgumentCountError",` |
|       - | 4093 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4094 | `			nArg` |
|       - | 4095 | `			);` |
|       - | 4096 | `	}` |
|      26 | 4097 | `	bStrict = FALSE;` |
|      26 | 4098 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4099 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4100 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4101 | `			"TypeError",` |
|       - | 4102 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4103 | `			ph7_type_name(apArg[1])` |
|       - | 4104 | `			);` |
|       - | 4105 | `	}` |
|      24 | 4106 | `	if( nArg > 2 ){` |
|       - | 4107 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4108 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4109 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4110 | `				"TypeError",` |
|       - | 4111 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4112 | `				ph7_type_name(apArg[2])` |
|       - | 4113 | `				);` |
|       - | 4114 | `		}` |
|       9 | 4115 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4116 | `	}` |
|       - | 4117 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4118 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4119 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4120 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4121 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4122 | `	pEntry = pMap->pFirst;` |
|      21 | 4123 | `	n = pMap->nEntry;` |
|      23 | 4124 | `	for(;;){` |
|      47 | 4125 | `		if( !n ){` |
|       9 | 4126 | `			break;` |
|       - | 4127 | `		}` |
|       - | 4128 | `		/* Extract node value */` |
|      39 | 4129 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4130 | `		if( pVal ){` |
|       - | 4131 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4132 | `			 * can change their type.` |
|       - | 4133 | `			 */` |
|      39 | 4134 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4135 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4136 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4137 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4138 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4139 | `			if( rc == 0 ){` |
|       - | 4140 | `				/* Match found,return key */` |
|      13 | 4141 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4142 | `					/* INT key */` |
|       7 | 4143 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4144 | `				}else{` |
|       7 | 4145 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4146 | `					/* Blob key */` |
|       7 | 4147 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4148 | `				}` |
|      13 | 4149 | `				return PH7_OK;` |
|       - | 4150 | `			}` |
|      13 | 4151 | `		}` |
|       - | 4152 | `		/* Point to the next entry */` |
|      27 | 4153 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4154 | `		n--;` |
|       1 | 4155 | `	}` |
|       - | 4156 | `	/* No such value,return FALSE */` |
|       9 | 4157 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4158 | `	return PH7_OK;` |
|      16 | 4159 |  |
|       - | 4160 | `/*` |
|       - | 4161 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4162 | ` *  Computes the difference of arrays.` |
|       - | 4163 | ` * Parameters` |
|       - | 4164 | ` *  $array1` |
|       - | 4165 | ` *    The array to compare from` |
|       - | 4166 | ` *  $array2` |
|       - | 4167 | ` *    An array to compare against` |
|       - | 4168 | ` *  $...` |
|       - | 4169 | ` *   More arrays to compare against` |
|       - | 4170 | ` * Return` |
|       - | 4171 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4172 | ` *  are not present in any of the other arrays.` |
|       - | 4173 | ` */` |
|      22 | 4174 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4175 |  |
|       - | 4176 | `	ph7_hashmap_node *pEntry;` |
|       - | 4177 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4178 | `	ph7_value *pArray;` |
|       - | 4179 | `	ph7_value *pVal;` |
|       - | 4180 | `	sxi32 rc;` |
|       - | 4181 | `	sxu32 n;` |
|       - | 4182 | `	int i;` |
|       - | 4183 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4184 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4185 | `	 * debugging difficult. */` |
|      24 | 4186 | `	if( nArg < 1 ){` |
|       4 | 4187 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4188 | `			"ArgumentCountError",` |
|       - | 4189 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4190 | `			nArg` |
|       - | 4191 | `			);` |
|       - | 4192 | `	}` |
|      22 | 4193 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4194 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4195 | `			"TypeError",` |
|       - | 4196 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4197 | `			ph7_type_name(apArg[0])` |
|       - | 4198 | `			);` |
|       - | 4199 | `	}` |
|      36 | 4200 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4201 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4202 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4203 | `				"TypeError",` |
|       - | 4204 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4205 | `				i + 1,` |
|       2 | 4206 | `				ph7_type_name(apArg[i])` |
|       - | 4207 | `				);` |
|       - | 4208 | `		}` |
|       9 | 4209 | `	}` |
|      17 | 4210 | `	if( nArg == 1 ){` |
|       - | 4211 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4212 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4213 | `		return PH7_OK;` |
|       - | 4214 | `	}` |
|       - | 4215 | `	/* Create a new array */` |
|      15 | 4216 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4217 | `	if( pArray == 0 ){` |
|     ! 0 | 4218 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4219 | `		return PH7_OK;` |
|       - | 4220 | `	}` |
|       - | 4221 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4222 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4223 | `	/* Perform the diff */` |
|      15 | 4224 | `	pEntry = pSrc->pFirst;` |
|      15 | 4225 | `	n = pSrc->nEntry;` |
|      27 | 4226 | `	for(;;){` |
|      55 | 4227 | `		if( n < 1 ){` |
|      15 | 4228 | `			break;` |
|       - | 4229 | `		}` |
|       - | 4230 | `		/* Extract the node value */` |
|      41 | 4231 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4232 | `		if( pVal ){` |
|      69 | 4233 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4234 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4235 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4236 | `				/* Perform the lookup */` |
|      45 | 4237 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4238 | `				if( rc == SXRET_OK ){` |
|       - | 4239 | `					/* Value exist */` |
|      17 | 4240 | `					break;` |
|       - | 4241 | `				}` |
|      15 | 4242 | `			}` |
|      41 | 4243 | `			if( i >= nArg ){` |
|       - | 4244 | `				/* Perform the insertion */` |
|      25 | 4245 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4246 | `			}` |
|      20 | 4247 | `		}` |
|       - | 4248 | `		/* Point to the next entry */` |
|      41 | 4249 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4250 | `		n--;` |
|       1 | 4251 | `	}` |
|       - | 4252 | `	/* Return the freshly created array */` |
|      15 | 4253 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4254 | `	return PH7_OK;` |
|      13 | 4255 |  |
|       - | 4256 | `/*` |
|       - | 4257 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4258 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4259 | ` * Parameters` |
|       - | 4260 | ` *  $array1` |
|       - | 4261 | ` *    The array to compare from` |
|       - | 4262 | ` *  $array2` |
|       - | 4263 | ` *    An array to compare against` |
|       - | 4264 | ` *  $...` |
|       - | 4265 | ` *   More arrays to compare against.` |
|       - | 4266 | ` * $callback` |
|       - | 4267 | ` *  The callback comparison function.` |
|       - | 4268 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4269 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4270 | ` *  than the second.` |
|       - | 4271 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4272 | ` * Return` |
|       - | 4273 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4274 | ` *  are not present in any of the other arrays.` |
|       - | 4275 | ` */` |
|      22 | 4276 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4277 |  |
|       - | 4278 | `	ph7_hashmap_node *pEntry;` |
|       - | 4279 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4280 | `	ph7_value *pCallback;` |
|       - | 4281 | `	ph7_value *pArray;` |
|       - | 4282 | `	ph7_value *pVal;` |
|       - | 4283 | `	sxi32 rc;` |
|       - | 4284 | `	sxu32 n;` |
|       - | 4285 | `	int i;` |
|       - | 4286 |  |
|       - | 4287 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      24 | 4288 | `	if( nArg < 2 ){` |
|       4 | 4289 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4290 | `			"ArgumentCountError",` |
|       - | 4291 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4292 | `			nArg` |
|       - | 4293 | `			);` |
|       - | 4294 | `	}` |
|      22 | 4295 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4296 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4297 | `			"TypeError",` |
|       - | 4298 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4299 | `			ph7_type_name(apArg[0])` |
|       - | 4300 | `			);` |
|       - | 4301 | `	}` |
|       - | 4302 |  |
|      20 | 4303 | `	if( nArg == 2 ){` |
|       - | 4304 | `		/* Only the original array and the callback were provided. */` |
|       - | 4305 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4306 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4307 | `		 * validation order.` |
|       - | 4308 | `		 */` |
|       4 | 4309 | `	} else {` |
|       - | 4310 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      24 | 4311 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      16 | 4312 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      10 | 4313 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4314 | `					"TypeError",` |
|       - | 4315 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4316 | `					i + 1,` |
|       6 | 4317 | `					ph7_type_name(apArg[i])` |
|       - | 4318 | `					);` |
|       - | 4319 | `			}` |
|       6 | 4320 | `		}` |
|       - | 4321 | `	}` |
|       - | 4322 |  |
|       - | 4323 | `	/* Identify the callback (always expected as the last argument). */` |
|      14 | 4324 | `	pCallback = apArg[nArg - 1];` |
|       - | 4325 | `	/* Validate the callback to match PHP's error messages. */` |
|      14 | 4326 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       7 | 4327 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4328 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4329 | `				"TypeError",` |
|       - | 4330 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4331 | `				nArg` |
|       - | 4332 | `				);` |
|       - | 4333 | `		}` |
|       5 | 4334 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4335 | `			int len;` |
|       3 | 4336 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4337 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4338 | `				"TypeError",` |
|       - | 4339 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4340 | `				nArg,` |
|       1 | 4341 | `				zName` |
|       - | 4342 | `				);` |
|       - | 4343 | `		}` |
|       4 | 4344 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4345 | `			"TypeError",` |
|       - | 4346 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4347 | `			nArg` |
|       - | 4348 | `			);` |
|       - | 4349 | `	}` |
|       - | 4350 |  |
|       7 | 4351 | `	if( nArg == 2 ){` |
|       - | 4352 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4353 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4354 | `		return PH7_OK;` |
|       - | 4355 | `	}` |
|       - | 4356 |  |
|       - | 4357 | `	/* Create a new array */` |
|       5 | 4358 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4359 | `	if( pArray == 0 ){` |
|     ! 0 | 4360 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4361 | `		return PH7_OK;` |
|       - | 4362 | `	}` |
|       - | 4363 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 4364 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4365 | `	/* Perform the diff */` |
|       5 | 4366 | `	pEntry = pSrc->pFirst;` |
|       5 | 4367 | `	n = pSrc->nEntry;` |
|       5 | 4368 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 4369 | `	for(;;){` |
|      11 | 4370 | `		if( n < 1 ){` |
|       3 | 4371 | `			break;` |
|       - | 4372 | `		}` |
|       - | 4373 | `		/* Extract the node value */` |
|       9 | 4374 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4375 | `		if( pVal ){` |
|      15 | 4376 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4377 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4378 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4379 | `				/* Perform the lookup */` |
|       9 | 4380 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       9 | 4381 | `				if( rc == SXRET_OK ){` |
|       - | 4382 | `					/* Value exist */` |
|       3 | 4383 | `					break;` |
|       - | 4384 | `				}` |
|       4 | 4385 | `			}` |
|       9 | 4386 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 4387 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 4388 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 4389 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 4390 | `				return PH7_EXCEPTION;` |
|       - | 4391 | `			}` |
|       7 | 4392 | `			if( i >= (nArg - 1)){` |
|       - | 4393 | `				/* Perform the insertion */` |
|       5 | 4394 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4395 | `			}` |
|       3 | 4396 | `		}` |
|       - | 4397 | `		/* Point to the next entry */` |
|       7 | 4398 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4399 | `		n--;` |
|       1 | 4400 | `	}` |
|       - | 4401 | `	/* Return the freshly created array */` |
|       3 | 4402 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4403 | `	return PH7_OK;` |
|      13 | 4404 |  |
|       - | 4405 | `/*` |
|       - | 4406 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4407 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4408 | ` * Parameters` |
|       - | 4409 | ` *  $array1` |
|       - | 4410 | ` *    The array to compare from` |
|       - | 4411 | ` *  $array2` |
|       - | 4412 | ` *    An array to compare against` |
|       - | 4413 | ` *  $...` |
|       - | 4414 | ` *   More arrays to compare against` |
|       - | 4415 | ` * Return` |
|       - | 4416 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4417 | ` *  are not present in any of the other arrays.` |
|       - | 4418 | ` */` |
|      20 | 4419 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4420 |  |
|       - | 4421 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4422 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4423 | `	ph7_value *pArray;` |
|       - | 4424 | `	ph7_value *pVal;` |
|       - | 4425 | `	sxi32 rc;` |
|       - | 4426 | `	sxu32 n;` |
|       - | 4427 | `	int i;` |
|       - | 4428 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4429 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4430 | `	 * accompanying integration tests to pass. */` |
|      22 | 4431 | `	if( nArg < 1 ){` |
|       4 | 4432 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4433 | `			"ArgumentCountError",` |
|       - | 4434 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4435 | `			nArg` |
|       - | 4436 | `			);` |
|       - | 4437 | `	}` |
|      20 | 4438 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4439 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4440 | `			"TypeError",` |
|       - | 4441 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4442 | `			ph7_type_name(apArg[0])` |
|       - | 4443 | `			);` |
|       - | 4444 | `	}` |
|      32 | 4445 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4446 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4447 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4448 | `				"TypeError",` |
|       - | 4449 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4450 | `				i + 1,` |
|       4 | 4451 | `				ph7_type_name(apArg[i])` |
|       - | 4452 | `				);` |
|       - | 4453 | `		}` |
|       9 | 4454 | `	}` |
|      13 | 4455 | `	if( nArg == 1 ){` |
|       - | 4456 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4457 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4458 | `		return PH7_OK;` |
|       - | 4459 | `	}` |
|       - | 4460 | `	/* Create a new array */` |
|      11 | 4461 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4462 | `	if( pArray == 0 ){` |
|     ! 0 | 4463 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4464 | `		return PH7_OK;` |
|       - | 4465 | `	}` |
|       - | 4466 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4467 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4468 | `	/* Perform the diff */` |
|      11 | 4469 | `	pEntry = pSrc->pFirst;` |
|      11 | 4470 | `	n = pSrc->nEntry;` |
|      11 | 4471 | `	pN1 = pN2 = 0;` |
|      29 | 4472 | `	for(;;){` |
|       - | 4473 | `		int keep;` |
|      35 | 4474 | `		if( n < 1 ){` |
|      11 | 4475 | `			break;` |
|       - | 4476 | `		}` |
|       - | 4477 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4478 | `		keep = 1;` |
|      41 | 4479 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4480 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4481 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4482 | `			/* Perform a key lookup first */` |
|      29 | 4483 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4484 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4485 | `			}else{` |
|      17 | 4486 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4487 | `			}` |
|      29 | 4488 | `			if( rc != SXRET_OK ){` |
|       - | 4489 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4490 | `				continue;` |
|       - | 4491 | `			}` |
|       - | 4492 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4493 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4494 | `			if( pVal ){` |
|       - | 4495 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4496 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4497 | `				if( pVal2 ){` |
|      15 | 4498 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4499 | `					if( cmp == 0 ){` |
|       - | 4500 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4501 | `						keep = 0;` |
|      13 | 4502 | `						break;` |
|       - | 4503 | `					}` |
|       1 | 4504 | `				}` |
|       1 | 4505 | `			}` |
|       2 | 4506 | `		}` |
|      25 | 4507 | `		if( keep ){` |
|       - | 4508 | `			/* Perform the insertion */` |
|      13 | 4509 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4510 | `		}` |
|       - | 4511 | `		/* Point to the next entry */` |
|      25 | 4512 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4513 | `		n--;` |
|       1 | 4514 | `	}` |
|       - | 4515 | `	/* Return the freshly created array */` |
|      11 | 4516 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4517 | `	return PH7_OK;` |
|      12 | 4518 |  |
|       - | 4519 | `/*` |
|       - | 4520 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4521 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4522 | ` *  by a user supplied callback function.` |
|       - | 4523 | ` * Parameters` |
|       - | 4524 | ` *  $array1` |
|       - | 4525 | ` *    The array to compare from` |
|       - | 4526 | ` *  $array2` |
|       - | 4527 | ` *    An array to compare against` |
|       - | 4528 | ` *  $...` |
|       - | 4529 | ` *   More arrays to compare against.` |
|       - | 4530 | ` *  $key_compare_func` |
|       - | 4531 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4532 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4533 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4534 | ` * Return` |
|       - | 4535 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4536 | ` *  are not present in any of the other arrays.` |
|       - | 4537 | ` */` |
|      24 | 4538 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4539 |  |
|       - | 4540 | `	ph7_hashmap_node *pEntry;` |
|       - | 4541 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4542 | `	ph7_value *pCallback;` |
|       - | 4543 | `	ph7_value *pArray;` |
|       - | 4544 | `	sxi32 rc;` |
|       - | 4545 | `	sxu32 n;` |
|       - | 4546 | `	int i;` |
|       - | 4547 |  |
|       - | 4548 | `	/* Argument validation mimicking PHP errors. */` |
|      26 | 4549 | `	if( nArg < 2 ){` |
|       4 | 4550 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4551 | `			"ArgumentCountError",` |
|       - | 4552 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4553 | `			nArg` |
|       - | 4554 | `			);` |
|       - | 4555 | `	}` |
|      24 | 4556 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4557 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4558 | `			"TypeError",` |
|       - | 4559 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4560 | `			ph7_type_name(apArg[0])` |
|       - | 4561 | `			);` |
|       - | 4562 | `	}` |
|       - | 4563 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4564 | `	 * expected to be a callback. */` |
|      36 | 4565 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      18 | 4566 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4567 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4568 | `				"TypeError",` |
|       - | 4569 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4570 | `				i + 1,` |
|       2 | 4571 | `				ph7_type_name(apArg[i])` |
|       - | 4572 | `				);` |
|       - | 4573 | `		}` |
|       9 | 4574 | `	}` |
|       - | 4575 | `	/* Point to the callback value */` |
|      20 | 4576 | `	pCallback = apArg[nArg - 1];` |
|      20 | 4577 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4578 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4579 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4580 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4581 | `		 * string given" which we also reproduce. */` |
|       7 | 4582 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4583 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4584 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4585 | `				"TypeError",` |
|       - | 4586 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4587 | `				nArg` |
|       - | 4588 | `				);` |
|       - | 4589 | `		}` |
|       5 | 4590 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4591 | `			/* neither array nor string */` |
|       7 | 4592 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4593 | `				"TypeError",` |
|       - | 4594 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4595 | `				nArg` |
|       - | 4596 | `				);` |
|       - | 4597 | `		}` |
|       - | 4598 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4599 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4600 | `			"TypeError",` |
|       - | 4601 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4602 | `			nArg,` |
|     ! 0 | 4603 | `			ph7_type_name(pCallback)` |
|       - | 4604 | `			);` |
|       - | 4605 | `	}` |
|      13 | 4606 | `	if( nArg == 2 ){` |
|       - | 4607 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4608 | `		 * input array. */` |
|       3 | 4609 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4610 | `		return PH7_OK;` |
|       - | 4611 | `	}` |
|       - | 4612 | `	/* Create a new array */` |
|      11 | 4613 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4614 | `	if( pArray == 0 ){` |
|     ! 0 | 4615 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4616 | `		return PH7_OK;` |
|       - | 4617 | `	}` |
|       - | 4618 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4619 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4620 | `	/* Perform the diff */` |
|      11 | 4621 | `	pEntry = pSrc->pFirst;` |
|      11 | 4622 | `	n = pSrc->nEntry;` |
|      21 | 4623 | `	for(;;){` |
|       - | 4624 | `		int keep;` |
|      27 | 4625 | `		if( n < 1 ){` |
|       9 | 4626 | `			break;` |
|       - | 4627 | `		}` |
|      19 | 4628 | `		keep = 1;` |
|      31 | 4629 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4630 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 4631 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4632 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 4633 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 4634 | `			while( pIt ){` |
|       - | 4635 | `				/* build temporary key values for callback */` |
|       - | 4636 | `				ph7_value key1, key2, result;` |
|       - | 4637 | `				/* initialise only once using the appropriate helper */` |
|      33 | 4638 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4639 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4640 | `				}else{` |
|       - | 4641 | `					SyString sStr;` |
|      33 | 4642 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4643 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4644 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 4645 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4646 | `				}` |
|      33 | 4647 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4648 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4649 | `				}else{` |
|       - | 4650 | `					SyString sStr;` |
|      33 | 4651 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4652 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4653 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 4654 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4655 | `				}` |
|      33 | 4656 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4657 | `				/* call user callback with (key1, key2) */` |
|       - | 4658 | `				{` |
|       - | 4659 | `					ph7_value *apK[2];` |
|      33 | 4660 | `					apK[0] = &key1;` |
|      33 | 4661 | `					apK[1] = &key2;` |
|      33 | 4662 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4663 | `				}` |
|      33 | 4664 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 4665 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 4666 | `					 * array_uintersect (which signal back from` |
|       - | 4667 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 4668 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 4669 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 4670 | `					PH7_MemObjRelease(&result);` |
|       3 | 4671 | `					PH7_MemObjRelease(&key1);` |
|       3 | 4672 | `					PH7_MemObjRelease(&key2);` |
|       3 | 4673 | `					return PH7_EXCEPTION;` |
|       - | 4674 | `				}` |
|      31 | 4675 | `				if( rc == SXRET_OK ){` |
|      31 | 4676 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4677 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4678 | `					}` |
|      31 | 4679 | `					if( result.x.iVal == 0 ){` |
|       - | 4680 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4681 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4682 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4683 | `						if( pVal1 && pVal2 ){` |
|      13 | 4684 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4685 | `								keep = 0;` |
|       9 | 4686 | `								PH7_MemObjRelease(&result);` |
|       - | 4687 | `								/* release keys too before breaking */` |
|       9 | 4688 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4689 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4690 | `								break;` |
|       - | 4691 | `							}` |
|       2 | 4692 | `						}` |
|       2 | 4693 | `					}` |
|      11 | 4694 | `				}` |
|      23 | 4695 | `				PH7_MemObjRelease(&result);` |
|      23 | 4696 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4697 | `				PH7_MemObjRelease(&key2);` |
|       - | 4698 | `				/* move to next node */` |
|      23 | 4699 | `				pIt = pIt->pPrev;` |
|      23 | 4700 | `				if( keep == 0 ) break;` |
|       1 | 4701 | `			}` |
|      21 | 4702 | `			if( keep == 0 ) break;` |
|       7 | 4703 | `		}` |
|      17 | 4704 | `		if( keep ){` |
|       - | 4705 | `			/* Perform the insertion */` |
|       9 | 4706 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4707 | `		}` |
|       - | 4708 | `		/* Point to the next entry */` |
|      17 | 4709 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4710 | `		n--;` |
|       1 | 4711 | `	}` |
|       - | 4712 | `	/* Return the freshly created array */` |
|       9 | 4713 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4714 | `	return PH7_OK;` |
|      14 | 4715 |  |
|       - | 4716 | `/*` |
|       - | 4717 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4718 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4719 | ` * Parameters` |
|       - | 4720 | ` *  $array1` |
|       - | 4721 | ` *    The array to compare from` |
|       - | 4722 | ` *  $array2` |
|       - | 4723 | ` *    An array to compare against` |
|       - | 4724 | ` *  $...` |
|       - | 4725 | ` *   More arrays to compare against` |
|       - | 4726 | ` * Return` |
|       - | 4727 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4728 | ` *  in any of the other arrays.` |
|       - | 4729 | ` * Note that NULL is returned on failure.` |
|       - | 4730 | ` */` |
|      14 | 4731 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4732 |  |
|       - | 4733 | `	ph7_hashmap_node *pEntry;` |
|       - | 4734 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4735 | `	ph7_value *pArray;` |
|       - | 4736 | `	sxi32 rc;` |
|       - | 4737 | `	sxu32 n;` |
|       - | 4738 | `	int i;` |
|       - | 4739 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4740 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4741 | `	 * helpers. */` |
|      16 | 4742 | `	if( nArg < 1 ){` |
|       4 | 4743 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4744 | `			"ArgumentCountError",` |
|       - | 4745 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4746 | `			nArg` |
|       - | 4747 | `			);` |
|       - | 4748 | `	}` |
|      14 | 4749 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4750 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4751 | `			"TypeError",` |
|       - | 4752 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4753 | `			ph7_type_name(apArg[0])` |
|       - | 4754 | `			);` |
|       - | 4755 | `	}` |
|      20 | 4756 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4757 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4758 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4759 | `				"TypeError",` |
|       - | 4760 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4761 | `				i + 1,` |
|       2 | 4762 | `				ph7_type_name(apArg[i])` |
|       - | 4763 | `				);` |
|       - | 4764 | `		}` |
|       5 | 4765 | `	}` |
|       9 | 4766 | `	if( nArg == 1 ){` |
|       - | 4767 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4768 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4769 | `		return PH7_OK;` |
|       - | 4770 | `	}` |
|       - | 4771 | `	/* Create a new array */` |
|       7 | 4772 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4773 | `	if( pArray == 0 ){` |
|     ! 0 | 4774 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4775 | `		return PH7_OK;` |
|       - | 4776 | `	}` |
|       - | 4777 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4778 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4779 | `	/* Perfrom the diff */` |
|       7 | 4780 | `	pEntry = pSrc->pFirst;` |
|       7 | 4781 | `	n = pSrc->nEntry;` |
|      12 | 4782 | `	for(;;){` |
|      25 | 4783 | `		if( n < 1 ){` |
|       7 | 4784 | `			break;` |
|       - | 4785 | `		}` |
|      31 | 4786 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4787 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4788 | `				/* ignore */` |
|     ! 0 | 4789 | `				continue;` |
|       - | 4790 | `			}` |
|      23 | 4791 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4792 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4793 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4794 | `				/* Blob lookup */` |
|      17 | 4795 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4796 | `			}else{` |
|       - | 4797 | `				/* Int lookup */` |
|       7 | 4798 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4799 | `			}` |
|      23 | 4800 | `			if( rc == SXRET_OK ){` |
|       - | 4801 | `				/* Key exists,break immediately */` |
|      11 | 4802 | `				break;` |
|       - | 4803 | `			}` |
|       7 | 4804 | `		}` |
|      19 | 4805 | `		if( i >= nArg ){` |
|       - | 4806 | `			/* Perform the insertion */` |
|       9 | 4807 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4808 | `		}` |
|       - | 4809 | `		/* Point to the next entry */` |
|      19 | 4810 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4811 | `		n--;` |
|       1 | 4812 | `	}` |
|       - | 4813 | `	/* Return the freshly created array */` |
|       7 | 4814 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4815 | `	return PH7_OK;` |
|       9 | 4816 |  |
|       - | 4817 | `/*` |
|       - | 4818 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4819 | ` *  Computes the intersection of arrays.` |
|       - | 4820 | ` * Parameters` |
|       - | 4821 | ` *  $array1` |
|       - | 4822 | ` *    The array to compare from` |
|       - | 4823 | ` *  $array2` |
|       - | 4824 | ` *    An array to compare against` |
|       - | 4825 | ` *  $...` |
|       - | 4826 | ` *   More arrays to compare against` |
|       - | 4827 | ` * Return` |
|       - | 4828 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4829 | ` *  in all of the parameters.` |
|       - | 4830 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4831 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4832 | ` */` |
|      22 | 4833 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4834 |  |
|       - | 4835 | `	ph7_hashmap_node *pEntry;` |
|       - | 4836 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4837 | `	ph7_value *pArray;` |
|       - | 4838 | `	ph7_value *pVal;` |
|       - | 4839 | `	sxi32 rc;` |
|       - | 4840 | `	sxu32 n;` |
|       - | 4841 | `	int i;` |
|      24 | 4842 | `	if( nArg < 1 ){` |
|       4 | 4843 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4844 | `			"ArgumentCountError",` |
|       - | 4845 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4846 | `			nArg` |
|       - | 4847 | `			);` |
|       - | 4848 | `	}` |
|      22 | 4849 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4850 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4851 | `			"TypeError",` |
|       - | 4852 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4853 | `			ph7_type_name(apArg[0])` |
|       - | 4854 | `			);` |
|       - | 4855 | `	}` |
|      36 | 4856 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4857 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4858 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4859 | `				"TypeError",` |
|       - | 4860 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4861 | `				i + 1,` |
|       2 | 4862 | `				ph7_type_name(apArg[i])` |
|       - | 4863 | `				);` |
|       - | 4864 | `		}` |
|       9 | 4865 | `	}` |
|      17 | 4866 | `	if( nArg == 1 ){` |
|       - | 4867 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4868 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4869 | `		return PH7_OK;` |
|       - | 4870 | `	}` |
|       - | 4871 | `	/* Create a new array */` |
|      15 | 4872 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4873 | `	if( pArray == 0 ){` |
|     ! 0 | 4874 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4875 | `		return PH7_OK;` |
|       - | 4876 | `	}` |
|       - | 4877 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4878 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4879 | `	/* Perform the intersection */` |
|      15 | 4880 | `	pEntry = pSrc->pFirst;` |
|      15 | 4881 | `	n = pSrc->nEntry;` |
|      31 | 4882 | `	for(;;){` |
|      63 | 4883 | `		if( n < 1 ){` |
|      15 | 4884 | `			break;` |
|       - | 4885 | `		}` |
|       - | 4886 | `		/* Extract the node value */` |
|      49 | 4887 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4888 | `		if( pVal ){` |
|      79 | 4889 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4890 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4891 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4892 | `				/* Perform the lookup */` |
|      55 | 4893 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4894 | `				if( rc != SXRET_OK ){` |
|       - | 4895 | `					/* Value does not exist */` |
|      25 | 4896 | `					break;` |
|       - | 4897 | `				}` |
|      16 | 4898 | `			}` |
|      49 | 4899 | `			if( i >= nArg ){` |
|       - | 4900 | `				/* Perform the insertion */` |
|      25 | 4901 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4902 | `			}` |
|      24 | 4903 | `		}` |
|       - | 4904 | `		/* Point to the next entry */` |
|      49 | 4905 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4906 | `		n--;` |
|       1 | 4907 | `	}` |
|       - | 4908 | `	/* Return the freshly created array */` |
|      15 | 4909 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4910 | `	return PH7_OK;` |
|      13 | 4911 |  |
|       - | 4912 | `/*` |
|       - | 4913 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4914 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4915 | ` * Parameters` |
|       - | 4916 | ` *  $array1` |
|       - | 4917 | ` *    The array to compare from` |
|       - | 4918 | ` *  $array2` |
|       - | 4919 | ` *    An array to compare against` |
|       - | 4920 | ` *  $...` |
|       - | 4921 | ` *   More arrays to compare against` |
|       - | 4922 | ` * Return` |
|       - | 4923 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4924 | ` *  in all the arguments, with matching keys.` |
|       - | 4925 | ` */` |
|      22 | 4926 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4927 |  |
|       - | 4928 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4929 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4930 | `	ph7_value *pArray;` |
|       - | 4931 | `	ph7_value *pVal;` |
|       - | 4932 | `	sxi32 rc;` |
|       - | 4933 | `	sxu32 n;` |
|       - | 4934 | `	int i;` |
|      24 | 4935 | `	if( nArg < 1 ){` |
|       4 | 4936 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4937 | `			"ArgumentCountError",` |
|       - | 4938 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4939 | `			nArg` |
|       - | 4940 | `			);` |
|       - | 4941 | `	}` |
|      22 | 4942 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4943 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4944 | `			"TypeError",` |
|       - | 4945 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4946 | `			ph7_type_name(apArg[0])` |
|       - | 4947 | `			);` |
|       - | 4948 | `	}` |
|      36 | 4949 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4950 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4951 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4952 | `				"TypeError",` |
|       - | 4953 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4954 | `				i + 1,` |
|       2 | 4955 | `				ph7_type_name(apArg[i])` |
|       - | 4956 | `				);` |
|       - | 4957 | `		}` |
|       9 | 4958 | `	}` |
|      17 | 4959 | `	if( nArg == 1 ){` |
|       - | 4960 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4961 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4962 | `		return PH7_OK;` |
|       - | 4963 | `	}` |
|       - | 4964 | `	/* Create a new array */` |
|      15 | 4965 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4966 | `	if( pArray == 0 ){` |
|     ! 0 | 4967 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4968 | `		return PH7_OK;` |
|       - | 4969 | `	}` |
|       - | 4970 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4971 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4972 | `	/* Perform the intersection */` |
|      15 | 4973 | `	pEntry = pSrc->pFirst;` |
|      15 | 4974 | `	n = pSrc->nEntry;` |
|      15 | 4975 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4976 | `	for(;;){` |
|      47 | 4977 | `		if( n < 1 ){` |
|      15 | 4978 | `			break;` |
|       - | 4979 | `		}` |
|       - | 4980 | `		/* Extract the node value */` |
|      33 | 4981 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4982 | `		if( pVal ){` |
|      53 | 4983 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4984 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4985 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4986 | `				/* Perform a key lookup first */` |
|      37 | 4987 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4988 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4989 | `				}else{` |
|      23 | 4990 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4991 | `				}` |
|      37 | 4992 | `				if( rc != SXRET_OK ){` |
|       - | 4993 | `					/* No such key,break immediately */` |
|       7 | 4994 | `					break;` |
|       - | 4995 | `				}` |
|       - | 4996 | `				/* Perform the lookup */` |
|      31 | 4997 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4998 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4999 | `					/* Value does not exist */` |
|       6 | 5000 | `					break;` |
|       - | 5001 | `				}` |
|      11 | 5002 | `			}` |
|      33 | 5003 | `			if( i >= nArg ){` |
|       - | 5004 | `				/* Perform the insertion */` |
|      17 | 5005 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5006 | `			}` |
|      16 | 5007 | `		}` |
|       - | 5008 | `		/* Point to the next entry */` |
|      33 | 5009 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5010 | `		n--;` |
|       1 | 5011 | `	}` |
|       - | 5012 | `	/* Return the freshly created array */` |
|      15 | 5013 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5014 | `	return PH7_OK;` |
|      13 | 5015 |  |
|       - | 5016 | `/*` |
|       - | 5017 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 5018 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 5019 | ` * Parameters` |
|       - | 5020 | ` *  $array1` |
|       - | 5021 | ` *    The array to compare from` |
|       - | 5022 | ` *  $...` |
|       - | 5023 | ` *   More arrays to compare against` |
|       - | 5024 | ` * Return` |
|       - | 5025 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 5026 | ` *  have keys that are present in all arguments.` |
|       - | 5027 | ` * Note that NULL is returned on failure.` |
|       - | 5028 | ` */` |
|      22 | 5029 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5030 |  |
|       - | 5031 | `	ph7_hashmap_node *pEntry;` |
|       - | 5032 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5033 | `	ph7_value *pArray;` |
|       - | 5034 | `	sxi32 rc;` |
|       - | 5035 | `	sxu32 n;` |
|       - | 5036 | `	int i;` |
|      24 | 5037 | `	if( nArg < 1 ){` |
|       4 | 5038 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5039 | `			"ArgumentCountError",` |
|       - | 5040 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 5041 | `			nArg` |
|       - | 5042 | `			);` |
|       - | 5043 | `	}` |
|      22 | 5044 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5045 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5046 | `			"TypeError",` |
|       - | 5047 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5048 | `			ph7_type_name(apArg[0])` |
|       - | 5049 | `			);` |
|       - | 5050 | `	}` |
|      36 | 5051 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5052 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5053 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5054 | `				"TypeError",` |
|       - | 5055 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5056 | `				i + 1,` |
|       2 | 5057 | `				ph7_type_name(apArg[i])` |
|       - | 5058 | `				);` |
|       - | 5059 | `		}` |
|       9 | 5060 | `	}` |
|      17 | 5061 | `	if( nArg == 1 ){` |
|       - | 5062 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5063 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5064 | `		return PH7_OK;` |
|       - | 5065 | `	}` |
|       - | 5066 | `	/* Create a new array */` |
|      15 | 5067 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5068 | `	if( pArray == 0 ){` |
|     ! 0 | 5069 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5070 | `		return PH7_OK;` |
|       - | 5071 | `	}` |
|       - | 5072 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 5073 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5074 | `	/* Perform the intersection */` |
|      15 | 5075 | `	pEntry = pSrc->pFirst;` |
|      15 | 5076 | `	n = pSrc->nEntry;` |
|      24 | 5077 | `	for(;;){` |
|      49 | 5078 | `		if( n < 1 ){` |
|      15 | 5079 | `			break;` |
|       - | 5080 | `		}` |
|      57 | 5081 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 5082 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 5083 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 5084 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5085 | `				/* Blob lookup */` |
|      27 | 5086 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 5087 | `			}else{` |
|       - | 5088 | `				/* Int key */` |
|      13 | 5089 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5090 | `			}` |
|      39 | 5091 | `			if( rc != SXRET_OK ){` |
|       - | 5092 | `				/* Key does not exist, break immediately */` |
|      17 | 5093 | `				break;` |
|       - | 5094 | `			}` |
|      12 | 5095 | `		}` |
|      35 | 5096 | `		if( i >= nArg ){` |
|       - | 5097 | `			/* Perform the insertion */` |
|      19 | 5098 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 5099 | `		}` |
|       - | 5100 | `		/* Point to the next entry */` |
|      35 | 5101 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 5102 | `		n--;` |
|       1 | 5103 | `	}` |
|       - | 5104 | `	/* Return the freshly created array */` |
|      15 | 5105 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5106 | `	return PH7_OK;` |
|      13 | 5107 |  |
|       - | 5108 | `/*` |
|       - | 5109 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 5110 | ` *  Computes the intersection of arrays.` |
|       - | 5111 | ` * Parameters` |
|       - | 5112 | ` *  $array1` |
|       - | 5113 | ` *    The array to compare from` |
|       - | 5114 | ` *  $array2` |
|       - | 5115 | ` *    An array to compare against` |
|       - | 5116 | ` *  $...` |
|       - | 5117 | ` *   More arrays to compare against` |
|       - | 5118 | ` * $callback` |
|       - | 5119 | ` *  The callback comparison function.` |
|       - | 5120 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5121 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5122 | ` *  than the second.` |
|       - | 5123 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5124 | ` * Return` |
|       - | 5125 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5126 | ` *  in all of the parameters. .` |
|       - | 5127 | ` * Note that NULL is returned on failure.` |
|       - | 5128 | ` */` |
|      26 | 5129 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5130 |  |
|       - | 5131 | `	ph7_hashmap_node *pEntry;` |
|       - | 5132 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5133 | `	ph7_value *pCallback;` |
|       - | 5134 | `	ph7_value *pArray;` |
|       - | 5135 | `	ph7_value *pVal;` |
|       - | 5136 | `	sxi32 rc;` |
|       - | 5137 | `	sxu32 n;` |
|       - | 5138 | `	int i;` |
|       - | 5139 |  |
|       - | 5140 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      28 | 5141 | `	if( nArg < 2 ){` |
|       4 | 5142 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5143 | `			"ArgumentCountError",` |
|       - | 5144 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5145 | `			nArg` |
|       - | 5146 | `			);` |
|       - | 5147 | `	}` |
|      26 | 5148 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5149 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5150 | `			"TypeError",` |
|       - | 5151 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5152 | `			ph7_type_name(apArg[0])` |
|       - | 5153 | `			);` |
|       - | 5154 | `	}` |
|       - | 5155 |  |
|      24 | 5156 | `	if( nArg == 2 ){` |
|       - | 5157 | `		/* Only the original array and the callback were provided. */` |
|       - | 5158 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5159 | `		 * validation ordering. */` |
|       3 | 5160 | `	} else {` |
|       - | 5161 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      36 | 5162 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      20 | 5163 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5164 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5165 | `					"TypeError",` |
|       - | 5166 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5167 | `					i + 1,` |
|       2 | 5168 | `					ph7_type_name(apArg[i])` |
|       - | 5169 | `					);` |
|       - | 5170 | `			}` |
|      10 | 5171 | `		}` |
|       - | 5172 | `	}` |
|       - | 5173 |  |
|       - | 5174 | `	/* Identify the callback (always expected as the last argument). */` |
|      22 | 5175 | `	pCallback = apArg[nArg - 1];` |
|       - | 5176 | `	/* Validate the callback to match PHP's error messages. */` |
|      22 | 5177 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      11 | 5178 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5179 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5180 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5181 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5182 | `			 */` |
|       7 | 5183 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       7 | 5184 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5185 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5186 | `					"TypeError",` |
|       - | 5187 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5188 | `					nArg` |
|       - | 5189 | `					);` |
|       - | 5190 | `			}` |
|       - | 5191 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5192 | `			{` |
|       5 | 5193 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       5 | 5194 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       5 | 5195 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5196 | `					int nMethodLen;` |
|       5 | 5197 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       5 | 5198 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       5 | 5199 | `					if( pClass ){` |
|       - | 5200 | `						/* Class exists but method is missing. */` |
|       4 | 5201 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5202 | `							"TypeError",` |
|       - | 5203 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5204 | `							nArg,` |
|       1 | 5205 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5206 | `							zMethod` |
|       - | 5207 | `							);` |
|       - | 5208 | `					}` |
|       - | 5209 | `					/* Class not found */` |
|       - | 5210 | `					{` |
|       - | 5211 | `						int nName;` |
|       3 | 5212 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5213 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5214 | `							"TypeError",` |
|       - | 5215 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5216 | `							nArg,` |
|       1 | 5217 | `							zName` |
|       - | 5218 | `							);` |
|       - | 5219 | `					}` |
|       - | 5220 | `				}` |
|       - | 5221 | `			}` |
|       - | 5222 | `			/* Fallback message */` |
|     ! 0 | 5223 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5224 | `				"TypeError",` |
|       - | 5225 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5226 | `				nArg` |
|       - | 5227 | `				);` |
|       - | 5228 | `		}` |
|       5 | 5229 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5230 | `			int len;` |
|       3 | 5231 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5232 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5233 | `				"TypeError",` |
|       - | 5234 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5235 | `				nArg,` |
|       1 | 5236 | `				zName` |
|       - | 5237 | `				);` |
|       - | 5238 | `		}` |
|       4 | 5239 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5240 | `			"TypeError",` |
|       - | 5241 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5242 | `			nArg` |
|       - | 5243 | `			);` |
|       - | 5244 | `	}` |
|       - | 5245 |  |
|      11 | 5246 | `	if( nArg == 2 ){` |
|       - | 5247 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5248 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5249 | `		return PH7_OK;` |
|       - | 5250 | `	}` |
|       - | 5251 |  |
|       - | 5252 | `	/* Create a new array */` |
|       7 | 5253 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5254 | `	if( pArray == 0 ){` |
|     ! 0 | 5255 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5256 | `		return PH7_OK;` |
|       - | 5257 | `	}` |
|       - | 5258 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 5259 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5260 | `	/* Perform the intersection */` |
|       7 | 5261 | `	pEntry = pSrc->pFirst;` |
|       7 | 5262 | `	n = pSrc->nEntry;` |
|       7 | 5263 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 5264 | `	for(;;){` |
|      19 | 5265 | `		if( n < 1 ){` |
|       5 | 5266 | `			break;` |
|       - | 5267 | `		}` |
|       - | 5268 | `		/* Extract the node value */` |
|      15 | 5269 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5270 | `		if( pVal ){` |
|      23 | 5271 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 5272 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5273 | `					/* ignore */` |
|     ! 0 | 5274 | `					continue;` |
|       - | 5275 | `				}` |
|       - | 5276 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 5277 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5278 | `				/* Perform the lookup */` |
|      15 | 5279 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 5280 | `				if( rc != SXRET_OK ){` |
|       - | 5281 | `					/* Value does not exist */` |
|       7 | 5282 | `					break;` |
|       - | 5283 | `				}` |
|       5 | 5284 | `			}` |
|      15 | 5285 | `			if( i >= (nArg-1) ){` |
|       - | 5286 | `				/* Perform the insertion */` |
|       9 | 5287 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5288 | `			}` |
|       7 | 5289 | `		}` |
|      15 | 5290 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 5291 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5292 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 5293 | `			return PH7_EXCEPTION;` |
|       - | 5294 | `		}` |
|       - | 5295 | `		/* Point to the next entry */` |
|      13 | 5296 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5297 | `		n--;` |
|       1 | 5298 | `	}` |
|       - | 5299 | `	/* Return the freshly created array */` |
|       5 | 5300 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5301 | `	return PH7_OK;` |
|      15 | 5302 |  |
|       - | 5303 | `/*` |
|       - | 5304 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5305 | ` *  Fill an array with values.` |
|       - | 5306 | ` * Parameters` |
|       - | 5307 | ` *  $start_index` |
|       - | 5308 | ` *    The first index of the returned array.` |
|       - | 5309 | ` *  $num` |
|       - | 5310 | ` *   Number of elements to insert.` |
|       - | 5311 | ` *  $value` |
|       - | 5312 | ` *    Value to use for filling.` |
|       - | 5313 | ` * Return` |
|       - | 5314 | ` *  The filled array or null on failure.` |
|       - | 5315 | ` */` |
|     238 | 5316 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5317 |  |
|       - | 5318 | `	ph7_value *pArray;` |
|       - | 5319 | `	int i,nEntry;` |
|       - | 5320 |  |
|       - | 5321 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 5322 | `	if( nArg != 3 ){` |
|       - | 5323 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 5324 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5325 | `			"ArgumentCountError",` |
|       - | 5326 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5327 | `			nArg` |
|       - | 5328 | `			);` |
|       - | 5329 | `	}` |
|       - | 5330 |  |
|       - | 5331 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5332 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5333 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5334 | `	 * and NULLs are rejected outright. */` |
|     466 | 5335 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 5336 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5337 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5338 | `			"TypeError",` |
|       - | 5339 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5340 | `			ph7_type_name(apArg[0])` |
|       - | 5341 | `			);` |
|       - | 5342 | `	}` |
|     234 | 5343 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5344 | `		int len;` |
|       8 | 5345 | `		sxu8 bReal = FALSE;` |
|       8 | 5346 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5347 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5348 | `			/* Non‑numeric string is an error. */` |
|       3 | 5349 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5350 | `				"TypeError",` |
|       - | 5351 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5352 | `				);` |
|       - | 5353 | `		}` |
|       5 | 5354 | `		if( bReal ){` |
|       - | 5355 | `			/* float-string -> deprecation warning */` |
|       4 | 5356 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5357 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5358 | `				zStr` |
|       - | 5359 | `				);` |
|       1 | 5360 | `		}` |
|       2 | 5361 | `	}` |
|       - | 5362 |  |
|       - | 5363 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5364 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 5365 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 5366 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5367 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5368 | `			"TypeError",` |
|       - | 5369 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5370 | `			ph7_type_name(apArg[1])` |
|       - | 5371 | `			);` |
|       - | 5372 | `	}` |
|     232 | 5373 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5374 | `		int len;` |
|       3 | 5375 | `		sxu8 bReal = FALSE;` |
|       3 | 5376 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5377 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5378 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5379 | `				"TypeError",` |
|       - | 5380 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5381 | `				);` |
|       - | 5382 | `		}` |
|     ! 0 | 5383 | `	}` |
|       - | 5384 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5385 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5386 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5387 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5388 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5389 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5390 | `		if( d != (double)i64 ){` |
|       7 | 5391 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5392 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5393 | `				d` |
|       - | 5394 | `				);` |
|       2 | 5395 | `		}` |
|       2 | 5396 | `	}` |
|       - | 5397 |  |
|       - | 5398 | `	/* Total number of entries to insert */` |
|     230 | 5399 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5400 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5401 | `	if( nEntry < 0 ){` |
|       3 | 5402 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5403 | `			"ValueError",` |
|       - | 5404 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5405 | `			);` |
|       - | 5406 | `	}` |
|       - | 5407 |  |
|       - | 5408 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5409 | `	if( nEntry == 0 ){` |
|       7 | 5410 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5411 | `		return PH7_OK;` |
|       - | 5412 | `	}` |
|       - | 5413 |  |
|       - | 5414 | `	/* Create a new array */` |
|     221 | 5415 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5416 | `	if( pArray == 0 ){` |
|     ! 0 | 5417 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5418 | `	}` |
|       - | 5419 |  |
|       - | 5420 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5421 | `	if( ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]) != SXRET_OK ){` |
|     ! 0 | 5422 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5423 | `	}` |
|       - | 5424 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5425 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5426 | `		if( ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]) != SXRET_OK ){` |
|       - | 5427 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 5428 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 5429 | `		}` |
| 1058682 | 5430 | `	}` |
|       - | 5431 | `	/* Return the filled array */` |
|     221 | 5432 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5433 | `	return PH7_OK;` |
|     121 | 5434 |  |
|       - | 5435 | `/*` |
|       - | 5436 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5437 | ` *  Fill an array with values, specifying keys.` |
|       - | 5438 | ` * Parameters` |
|       - | 5439 | ` *  $input` |
|       - | 5440 | ` *   Array of values that will be used as key.` |
|       - | 5441 | ` *  $value` |
|       - | 5442 | ` *    Value to use for filling.` |
|       - | 5443 | ` * Return` |
|       - | 5444 | ` *  The filled array.` |
|       - | 5445 | ` * Throws` |
|       - | 5446 | ` *  ValueError if $input is not an array.` |
|       - | 5447 | ` */` |
|      26 | 5448 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5449 |  |
|       - | 5450 | `	ph7_hashmap_node *pEntry;` |
|       - | 5451 | `	ph7_hashmap *pSrc;` |
|       - | 5452 | `	ph7_value *pArray;` |
|       - | 5453 | `	sxu32 n;` |
|       - | 5454 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5455 | `	if( nArg != 2 ){` |
|      10 | 5456 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5457 | `			"ArgumentCountError",` |
|       - | 5458 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5459 | `			nArg` |
|       - | 5460 | `			);` |
|       - | 5461 | `	}` |
|       - | 5462 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5463 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5464 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5465 | `			"TypeError",` |
|       - | 5466 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5467 | `			ph7_type_name(apArg[0])` |
|       - | 5468 | `			);` |
|       - | 5469 | `	}` |
|       - | 5470 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5471 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5472 | `	/* Create a new array */` |
|      17 | 5473 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5474 | `	if( pArray == 0 ){` |
|     ! 0 | 5475 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5476 | `		return PH7_OK;` |
|       - | 5477 | `	}` |
|       - | 5478 | `	/* Perform the requested operation */` |
|      17 | 5479 | `	pEntry = pSrc->pFirst;` |
|      45 | 5480 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5481 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5482 | `		/* Point to the next entry */` |
|      29 | 5483 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5484 | `	}` |
|       - | 5485 | `	/* Return the filled array */` |
|      17 | 5486 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5487 | `	return PH7_OK;` |
|      15 | 5488 |  |
|       - | 5489 | `/*` |
|       - | 5490 | ` * array array_combine(array $keys,array $values)` |
|       - | 5491 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5492 | ` * Parameters` |
|       - | 5493 | ` *  $keys` |
|       - | 5494 | ` *    Array of keys to be used.` |
|       - | 5495 | ` * $values` |
|       - | 5496 | ` *   Array of values to be used.` |
|       - | 5497 | ` * Return` |
|       - | 5498 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5499 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5500 | ` *  not an array.` |
|       - | 5501 | ` */` |
|      18 | 5502 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5503 |  |
|       - | 5504 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5505 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5506 | `	ph7_value *pArray;` |
|       - | 5507 | `	sxu32 n;` |
|       - | 5508 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5509 | `	if( nArg != 2 ){` |
|       - | 5510 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5511 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5512 | `			"ArgumentCountError",` |
|       - | 5513 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5514 | `			nArg` |
|       - | 5515 | `			);` |
|       - | 5516 | `	}` |
|       - | 5517 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5518 | `	 * argument index in the error message. */` |
|      18 | 5519 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5520 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5521 | `			"TypeError",` |
|       - | 5522 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5523 | `			ph7_type_name(apArg[0])` |
|       - | 5524 | `			);` |
|       - | 5525 | `	}` |
|      16 | 5526 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5527 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5528 | `			"TypeError",` |
|       - | 5529 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5530 | `			ph7_type_name(apArg[1])` |
|       - | 5531 | `			);` |
|       - | 5532 | `	}` |
|       - | 5533 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5534 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5535 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5536 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5537 | `		/* Length mismatch -> ValueError */` |
|       3 | 5538 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5539 | `			"ValueError",` |
|       - | 5540 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5541 | `			);` |
|       - | 5542 | `	}` |
|       - | 5543 | `	/* Create a new array */` |
|      11 | 5544 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5545 | `	if( pArray == 0 ){` |
|     ! 0 | 5546 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5547 | `		return PH7_OK;` |
|       - | 5548 | `	}` |
|       - | 5549 | `	/* Perform the requested operation */` |
|      11 | 5550 | `	pKe = pKey->pFirst;` |
|      11 | 5551 | `	pVe = pValue->pFirst;` |
|      33 | 5552 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5553 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5554 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5555 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5556 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5557 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5558 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5559 | `		 * original array must not be mutated. */` |
|      23 | 5560 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5561 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5562 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5563 | `			if( pTmpKey ){` |
|       5 | 5564 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5565 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5566 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5567 | `				pKeyCopy = pTmpKey;` |
|       2 | 5568 | `			}` |
|       2 | 5569 | `		}` |
|      23 | 5570 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5571 | `		/* Point to the next entry */` |
|      23 | 5572 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5573 | `		pVe = pVe->pPrev;` |
|      12 | 5574 | `	}` |
|       - | 5575 | `	/* Return the filled array */` |
|      11 | 5576 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5577 | `	return PH7_OK;` |
|      11 | 5578 |  |
|       - | 5579 | `/*` |
|       - | 5580 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5581 | ` *  Return an array with elements in reverse order.` |
|       - | 5582 | ` * Parameters` |
|       - | 5583 | ` *  $array` |
|       - | 5584 | ` *   The input array.` |
|       - | 5585 | ` *  $preserve_keys (optional)` |
|       - | 5586 | ` *   If set to TRUE keys are preserved.` |
|       - | 5587 | ` * Return` |
|       - | 5588 | ` *  The reversed array.` |
|       - | 5589 | ` */` |
|      20 | 5590 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5591 |  |
|       - | 5592 | `	ph7_hashmap_node *pEntry;` |
|       - | 5593 | `	ph7_hashmap *pSrc;` |
|       - | 5594 | `	ph7_value *pArray;` |
|       - | 5595 | `	int bPreserve;` |
|       - | 5596 | `	sxu32 n;` |
|      22 | 5597 | `	if( nArg < 1 ){` |
|       4 | 5598 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5599 | `			"ArgumentCountError",` |
|       - | 5600 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5601 | `			nArg` |
|       - | 5602 | `			);` |
|       - | 5603 | `	}` |
|       - | 5604 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5605 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5606 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5607 | `			"TypeError",` |
|       - | 5608 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5609 | `			ph7_type_name(apArg[0])` |
|       - | 5610 | `			);` |
|       - | 5611 | `	}` |
|      17 | 5612 | `	bPreserve = FALSE;` |
|      17 | 5613 | `	if( nArg > 1 ){` |
|       7 | 5614 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5615 | `	}` |
|       - | 5616 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5617 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5618 | `	/* Create a new array */` |
|      17 | 5619 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5620 | `	if( pArray == 0 ){` |
|     ! 0 | 5621 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5622 | `		return PH7_OK;` |
|       - | 5623 | `	}` |
|       - | 5624 | `	/* Perform the requested operation */` |
|      17 | 5625 | `	pEntry = pSrc->pLast;` |
|      55 | 5626 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5627 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5628 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5629 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5630 | `		/* Point to the previous entry */` |
|      39 | 5631 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5632 | `	}` |
|      17 | 5633 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5634 | `	return PH7_OK;` |
|      12 | 5635 |  |
|       - | 5636 | `/*` |
|       - | 5637 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5638 | ` *  Removes duplicate values from an array.` |
|       - | 5639 | ` * Parameters` |
|       - | 5640 | ` *  $array` |
|       - | 5641 | ` *   The input array.` |
|       - | 5642 | ` *  $flags` |
|       - | 5643 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5644 | ` *   behavior using these values:` |
|       - | 5645 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5646 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5647 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5648 | ` * Return` |
|       - | 5649 | ` *  The filtered array.` |
|       - | 5650 | ` */` |
|      24 | 5651 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5652 |  |
|       - | 5653 | `	ph7_hashmap_node *pEntry;` |
|       - | 5654 | `	ph7_value *pNeedle;` |
|       - | 5655 | `	ph7_hashmap *pSrc;` |
|       - | 5656 | `	ph7_value *pArray;` |
|       - | 5657 | `	int bStrict;` |
|       - | 5658 | `	sxi32 rc;` |
|       - | 5659 | `	sxu32 n;` |
|      26 | 5660 | `	if( nArg < 1 ){` |
|       - | 5661 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5662 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5663 | `			"ArgumentCountError",` |
|       - | 5664 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5665 | `			);` |
|       - | 5666 | `	}` |
|      24 | 5667 | `	if( nArg > 2 ){` |
|       - | 5668 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5669 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5670 | `			"ArgumentCountError",` |
|       - | 5671 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5672 | `			nArg` |
|       - | 5673 | `			);` |
|       - | 5674 | `	}` |
|       - | 5675 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5676 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5677 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5678 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5679 | `			"TypeError",` |
|       - | 5680 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5681 | `			ph7_type_name(apArg[0])` |
|       - | 5682 | `			);` |
|       - | 5683 | `	}` |
|      19 | 5684 | `	bStrict = FALSE;` |
|       - | 5685 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5686 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5687 | `	/* Create a new array */` |
|      19 | 5688 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5689 | `	if( pArray == 0 ){` |
|     ! 0 | 5690 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5691 | `		return PH7_OK;` |
|       - | 5692 | `	}` |
|       - | 5693 | `	/* Perform the requested operation */` |
|      19 | 5694 | `	pEntry = pSrc->pFirst;` |
|      83 | 5695 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5696 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5697 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5698 | `		if( pNeedle ){` |
|      65 | 5699 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5700 | `		}` |
|      65 | 5701 | `		if( rc != SXRET_OK ){` |
|       - | 5702 | `			/* Perform the insertion */` |
|      37 | 5703 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5704 | `		}` |
|       - | 5705 | `		/* Point to the next entry */` |
|      65 | 5706 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5707 | `	}` |
|       - | 5708 | `	/* Return the freshly created array */` |
|      19 | 5709 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5710 | `	return PH7_OK;` |
|      14 | 5711 |  |
|       - | 5712 | `/*` |
|       - | 5713 | ` * array array_flip(array $input)` |
|       - | 5714 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5715 | ` * Parameter` |
|       - | 5716 | ` *  $input` |
|       - | 5717 | ` *   Input array.` |
|       - | 5718 | ` * Return` |
|       - | 5719 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5720 | ` */` |
|      34 | 5721 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5722 |  |
|       - | 5723 | `	ph7_hashmap_node *pEntry;` |
|       - | 5724 | `	ph7_hashmap *pSrc;` |
|       - | 5725 | `	ph7_value *pArray;` |
|       - | 5726 | `	ph7_value *pKey;` |
|       - | 5727 | `	ph7_value sVal;` |
|       - | 5728 | `	sxu32 n;` |
|       - | 5729 |  |
|       - | 5730 | `	/* PHP requires exactly one argument */` |
|      36 | 5731 | `	if( nArg != 1 ){` |
|       - | 5732 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5733 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5734 | `			"ArgumentCountError",` |
|       - | 5735 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5736 | `			nArg` |
|       - | 5737 | `			);` |
|       - | 5738 | `	}` |
|       - | 5739 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5740 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5741 | `		/* Type mismatch -> TypeError */` |
|       7 | 5742 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5743 | `			"TypeError",` |
|       - | 5744 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5745 | `			ph7_type_name(apArg[0])` |
|       - | 5746 | `			);` |
|       - | 5747 | `	}` |
|       - | 5748 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5749 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5750 | `	/* Create a new array */` |
|      27 | 5751 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5752 | `	if( pArray == 0 ){` |
|     ! 0 | 5753 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5754 | `		return PH7_OK;` |
|       - | 5755 | `	}` |
|       - | 5756 | `	/* Start processing */` |
|      27 | 5757 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5758 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5759 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5760 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5761 | `		if( pKey ){` |
|       - | 5762 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5763 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5764 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5765 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5766 | `					);` |
|   22236 | 5767 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5768 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5769 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5770 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5771 | `				}else{` |
|       - | 5772 | `					SyString sStr;` |
|    2227 | 5773 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5774 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5775 | `				}` |
|       - | 5776 | `				/* Perform the insertion */` |
|   22227 | 5777 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5778 | `				/* Safely release the value because each inserted entry` |
|       - | 5779 | `				 * has its own private copy of the value.` |
|       - | 5780 | `				 */` |
|   22227 | 5781 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5782 | `			}else{` |
|       - | 5783 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5784 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5785 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5786 | `					);` |
|       - | 5787 | `			}` |
|   11118 | 5788 | `		}` |
|       - | 5789 | `		/* Point to the next entry */` |
|   22237 | 5790 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5791 | `	}` |
|       - | 5792 | `	/* Return the freshly created array */` |
|      27 | 5793 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5794 | `	return PH7_OK;` |
|      19 | 5795 |  |
|       - | 5796 | `/*` |
|       - | 5797 | ` * number array_sum(array $array )` |
|       - | 5798 | ` *  Calculate the sum of values in an array.` |
|       - | 5799 | ` * Parameters` |
|       - | 5800 | ` *  $array: The input array.` |
|       - | 5801 | ` * Return` |
|       - | 5802 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5803 | ` */` |
|      24 | 5804 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5805 |  |
|       - | 5806 | `	ph7_hashmap_node *pEntry;` |
|       - | 5807 | `	ph7_value *pObj;` |
|      25 | 5808 | `	double dSum = 0;` |
|       - | 5809 | `	sxu32 n;` |
|      25 | 5810 | `	pEntry = pMap->pFirst;` |
|      91 | 5811 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5812 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5813 | `		if( pObj ){` |
|      67 | 5814 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5815 | `				dSum += pObj->rVal;` |
|      53 | 5816 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5817 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5818 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5819 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5820 | `					double dv = 0;` |
|      13 | 5821 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5822 | `					dSum += dv;` |
|       7 | 5823 | `				}` |
|      12 | 5824 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5825 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5826 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5827 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5828 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5829 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5830 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5831 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5832 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5833 | `			}` |
|       - | 5834 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5835 | `		}` |
|       - | 5836 | `		/* Point to the next entry */` |
|      67 | 5837 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5838 | `	}` |
|       - | 5839 | `	/* Return sum */` |
|      25 | 5840 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5841 |  |
|      26 | 5842 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5843 |  |
|       - | 5844 | `	ph7_hashmap_node *pEntry;` |
|       - | 5845 | `	ph7_value *pObj;` |
|      28 | 5846 | `	sxi64 nSum = 0;` |
|       - | 5847 | `	sxu32 n;` |
|      28 | 5848 | `	pEntry = pMap->pFirst;` |
|     112 | 5849 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      86 | 5850 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      86 | 5851 | `		if( pObj ){` |
|      86 | 5852 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      76 | 5853 | `				nSum += pObj->x.iVal;` |
|      48 | 5854 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5855 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5856 | `					sxi64 nv = 0;` |
|       5 | 5857 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5858 | `					nSum += nv;` |
|       3 | 5859 | `				}` |
|       8 | 5860 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5861 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5862 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5863 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5864 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5865 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5866 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5867 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5868 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5869 | `			}` |
|       - | 5870 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      42 | 5871 | `		}` |
|       - | 5872 | `		/* Point to the next entry */` |
|      86 | 5873 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      44 | 5874 | `	}` |
|       - | 5875 | `	/* Return sum */` |
|      28 | 5876 | `	ph7_result_int64(pCtx,nSum);` |
|      28 | 5877 |  |
|       - | 5878 | `/* number array_sum(array $array )` |
|       - | 5879 | ` * (See block-coment above)` |
|       - | 5880 | ` */` |
|      64 | 5881 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5882 |  |
|       - | 5883 | `	ph7_hashmap_node *pEntry;` |
|       - | 5884 | `	ph7_hashmap *pMap;` |
|       - | 5885 | `	ph7_value *pObj;` |
|      66 | 5886 | `	int useDouble = 0;` |
|       - | 5887 | `	sxu32 n;` |
|       - | 5888 | `	/* PHP requires exactly one argument */` |
|      66 | 5889 | `	if( nArg != 1 ){` |
|       7 | 5890 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5891 | `			"ArgumentCountError",` |
|       - | 5892 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5893 | `			nArg` |
|       - | 5894 | `			);` |
|       - | 5895 | `	}` |
|       - | 5896 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 5897 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5898 | `		/* Type mismatch -> TypeError */` |
|       7 | 5899 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5900 | `			"TypeError",` |
|       - | 5901 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5902 | `			ph7_type_name(apArg[0])` |
|       - | 5903 | `			);` |
|       - | 5904 | `	}` |
|      58 | 5905 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      58 | 5906 | `	if( pMap->nEntry < 1 ){` |
|       - | 5907 | `		/* Nothing to compute,return 0 */` |
|       7 | 5908 | `		ph7_result_int(pCtx,0);` |
|       7 | 5909 | `		return PH7_OK;` |
|       - | 5910 | `	}` |
|       - | 5911 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5912 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5913 | `	 */` |
|      52 | 5914 | `	pEntry = pMap->pFirst;` |
|     144 | 5915 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     118 | 5916 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     118 | 5917 | `		if( pObj ){` |
|     118 | 5918 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5919 | `				useDouble = 1;` |
|      19 | 5920 | `				break;` |
|       - | 5921 | `			}` |
|     100 | 5922 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5923 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5924 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5925 | `				sxu32 i;` |
|      23 | 5926 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5927 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5928 | `						useDouble = 1;` |
|       7 | 5929 | `						break;` |
|       - | 5930 | `					}` |
|       6 | 5931 | `				}` |
|      13 | 5932 | `				if( useDouble ){` |
|       7 | 5933 | `					break;` |
|       - | 5934 | `				}` |
|       3 | 5935 | `			}` |
|      46 | 5936 | `		}` |
|      94 | 5937 | `		pEntry = pEntry->pPrev;` |
|      48 | 5938 | `	}` |
|      52 | 5939 | `	if( useDouble ){` |
|      25 | 5940 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5941 | `	}else{` |
|      28 | 5942 | `		Int64Sum(pCtx,pMap);` |
|       - | 5943 | `	}` |
|      52 | 5944 | `	return PH7_OK;` |
|      34 | 5945 |  |
|       - | 5946 | `/*` |
|       - | 5947 | ` * number array_product(array $array )` |
|       - | 5948 | ` *  Calculate the product of values in an array.` |
|       - | 5949 | ` * Parameters` |
|       - | 5950 | ` *  $array: The input array.` |
|       - | 5951 | ` * Return` |
|       - | 5952 | ` *  Returns the product of values as an integer or float.` |
|       - | 5953 | ` */` |
|     ! 0 | 5954 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5955 |  |
|       - | 5956 | `	ph7_hashmap_node *pEntry;` |
|       - | 5957 | `	ph7_value *pObj;` |
|       - | 5958 | `	double dProd;` |
|       - | 5959 | `	sxu32 n;` |
|     ! 0 | 5960 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5961 | `	dProd = 1;` |
|     ! 0 | 5962 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5963 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5964 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5965 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5966 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5967 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5968 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5969 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5970 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5971 | `					double dv = 0;` |
|     ! 0 | 5972 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5973 | `					dProd *= dv;` |
|     ! 0 | 5974 | `				}` |
|     ! 0 | 5975 | `			}` |
|     ! 0 | 5976 | `		}` |
|       - | 5977 | `		/* Point to the next entry */` |
|     ! 0 | 5978 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5979 | `	}` |
|       - | 5980 | `	/* Return product */` |
|     ! 0 | 5981 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5982 |  |
|     ! 0 | 5983 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5984 |  |
|       - | 5985 | `	ph7_hashmap_node *pEntry;` |
|       - | 5986 | `	ph7_value *pObj;` |
|       - | 5987 | `	sxi64 nProd;` |
|       - | 5988 | `	sxu32 n;` |
|     ! 0 | 5989 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5990 | `	nProd = 1;` |
|     ! 0 | 5991 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5992 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5993 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5994 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5995 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5996 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5997 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5998 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5999 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6000 | `					sxi64 nv = 0;` |
|     ! 0 | 6001 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 6002 | `					nProd *= nv;` |
|     ! 0 | 6003 | `				}` |
|     ! 0 | 6004 | `			}` |
|     ! 0 | 6005 | `		}` |
|       - | 6006 | `		/* Point to the next entry */` |
|     ! 0 | 6007 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6008 | `	}` |
|       - | 6009 | `	/* Return product */` |
|     ! 0 | 6010 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 6011 |  |
|       - | 6012 | `/* number array_product(array $array )` |
|       - | 6013 | ` * (See block-block comment above)` |
|       - | 6014 | ` */` |
|     ! 0 | 6015 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 6016 |  |
|       - | 6017 | `	ph7_hashmap *pMap;` |
|       - | 6018 | `	ph7_value *pObj;` |
|     ! 0 | 6019 | `	if( nArg < 1 ){` |
|       - | 6020 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 6021 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6022 | `		return PH7_OK;` |
|       - | 6023 | `	}` |
|       - | 6024 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 6025 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6026 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 6027 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6028 | `		return PH7_OK;` |
|       - | 6029 | `	}` |
|     ! 0 | 6030 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 6031 | `	if( pMap->nEntry < 1 ){` |
|       - | 6032 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 6033 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6034 | `		return PH7_OK;` |
|       - | 6035 | `	}` |
|       - | 6036 | `	/* If the first element is of type float,then perform floating` |
|       - | 6037 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 6038 | `	 */` |
|     ! 0 | 6039 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 6040 | `	if( pObj == 0 ){` |
|     ! 0 | 6041 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6042 | `		return PH7_OK;` |
|       - | 6043 | `	}` |
|     ! 0 | 6044 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6045 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 6046 | `	}else{` |
|     ! 0 | 6047 | `		Int64Prod(pCtx,pMap);` |
|       - | 6048 | `	}` |
|     ! 0 | 6049 | `	return PH7_OK;` |
|     ! 0 | 6050 |  |
|       - | 6051 | `/*` |
|       - | 6052 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 6053 | ` *  Pick one or more random entries out of an array.` |
|       - | 6054 | ` * Parameters` |
|       - | 6055 | ` * $input` |
|       - | 6056 | ` *  The input array.` |
|       - | 6057 | ` * $num_req` |
|       - | 6058 | ` *  Specifies how many entries you want to pick.` |
|       - | 6059 | ` * Return` |
|       - | 6060 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 6061 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 6062 | ` *  NULL is returned on failure.` |
|       - | 6063 | ` */` |
|       6 | 6064 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6065 |  |
|       - | 6066 | `	ph7_hashmap_node *pNode;` |
|       - | 6067 | `	ph7_hashmap *pMap;` |
|       7 | 6068 | `	int nItem = 1;` |
|       7 | 6069 | `	if( nArg < 1 ){` |
|       - | 6070 | `		/* Missing argument,return NULL */` |
|     ! 0 | 6071 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6072 | `		return PH7_OK;` |
|       - | 6073 | `	}` |
|       - | 6074 | `	/* Make sure we are dealing with an array */` |
|       7 | 6075 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6076 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6077 | `		return PH7_OK;` |
|       - | 6078 | `	}` |
|       - | 6079 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 6080 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 6081 | `	if(pMap->nEntry < 1 ){` |
|       - | 6082 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 6083 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6084 | `		return PH7_OK;` |
|       - | 6085 | `	}` |
|       7 | 6086 | `	if( nArg > 1 ){` |
|       3 | 6087 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 6088 | `	}` |
|       7 | 6089 | `	if( nItem < 2 ){` |
|       - | 6090 | `		sxu32 nEntry;` |
|       - | 6091 | `		/* Select a random number */` |
|       5 | 6092 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 6093 | `		/* Extract the desired entry.` |
|       - | 6094 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 6095 | `		 */` |
|       5 | 6096 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 6097 | `			pNode = pMap->pLast;` |
|       3 | 6098 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 6099 | `			if( nEntry > 1 ){` |
|     ! 0 | 6100 | `				for(;;){` |
|     ! 0 | 6101 | `					if( nEntry == 0 ){` |
|     ! 0 | 6102 | `						break;` |
|       - | 6103 | `					}` |
|       - | 6104 | `					/* Point to the previous entry */` |
|     ! 0 | 6105 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 6106 | `					nEntry--;` |
|     ! 0 | 6107 | `				}` |
|     ! 0 | 6108 | `			}` |
|       1 | 6109 | `		}else{` |
|       2 | 6110 | `			pNode = pMap->pFirst;` |
|       2 | 6111 | `			for(;;){` |
|       3 | 6112 | `				if( nEntry == 0 ){` |
|       2 | 6113 | `					break;` |
|       - | 6114 | `				}` |
|       - | 6115 | `				/* Point to the next entry */` |
|       1 | 6116 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 6117 | `				nEntry--;` |
|     ! 0 | 6118 | `			}` |
|       - | 6119 | `		}` |
|       5 | 6120 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6121 | `			/* Int key */` |
|       3 | 6122 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6123 | `		}else{` |
|       - | 6124 | `			/* Blob key */` |
|       3 | 6125 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6126 | `		}` |
|       3 | 6127 | `	}else{` |
|       - | 6128 | `		ph7_value sKey,*pArray;` |
|       - | 6129 | `		ph7_hashmap *pDest;` |
|       - | 6130 | `		/* Create a new array */` |
|       3 | 6131 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6132 | `		if( pArray == 0 ){` |
|     ! 0 | 6133 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6134 | `			return PH7_OK;` |
|       - | 6135 | `		}` |
|       - | 6136 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6137 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6138 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6139 | `		/* Copy the first n items */` |
|       3 | 6140 | `		pNode = pMap->pFirst;` |
|       3 | 6141 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6142 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6143 | `		}` |
|       7 | 6144 | `		while( nItem > 0){` |
|       5 | 6145 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6146 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6147 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6148 | `			/* Point to the next entry */` |
|       5 | 6149 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6150 | `			nItem--;` |
|       1 | 6151 | `		}` |
|       - | 6152 | `		/* Shuffle the array */` |
|       3 | 6153 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6154 | `		/* Rehash node */` |
|       3 | 6155 | `		HashmapSortRehash(pDest);` |
|       - | 6156 | `		/* Return the random array */` |
|       3 | 6157 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6158 | `	}` |
|       7 | 6159 | `	return PH7_OK;` |
|       4 | 6160 |  |
|       - | 6161 | `/*` |
|       - | 6162 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6163 | ` *  Split an array into chunks.` |
|       - | 6164 | ` * Parameters` |
|       - | 6165 | ` * $input` |
|       - | 6166 | ` *   The array to work on` |
|       - | 6167 | ` * $size` |
|       - | 6168 | ` *   The size of each chunk` |
|       - | 6169 | ` * $preserve_keys` |
|       - | 6170 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6171 | ` *   the chunk numerically.` |
|       - | 6172 | ` * Return` |
|       - | 6173 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6174 | ` *  zero, with each dimension containing size elements.` |
|       - | 6175 | ` */` |
|      42 | 6176 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6177 |  |
|       - | 6178 | `	ph7_value *pArray,*pChunk;` |
|       - | 6179 | `	ph7_hashmap_node *pEntry;` |
|       - | 6180 | `	ph7_hashmap *pMap;` |
|       - | 6181 | `	int bPreserve;` |
|       - | 6182 | `	sxu32 nChunk;` |
|       - | 6183 | `	sxu32 nSize;` |
|       - | 6184 | `	sxu32 n;` |
|       - | 6185 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 6186 | `	if( nArg < 2 ){` |
|       - | 6187 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6188 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6189 | `			"ArgumentCountError",` |
|       - | 6190 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6191 | `			nArg` |
|       - | 6192 | `			);` |
|       - | 6193 | `	}` |
|      42 | 6194 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6195 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6196 | `			"TypeError",` |
|       - | 6197 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6198 | `			ph7_type_name(apArg[0])` |
|       - | 6199 | `			);` |
|       - | 6200 | `	}` |
|       - | 6201 | `	/* Create a new array */` |
|      40 | 6202 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 6203 | `	if( pArray == 0 ){` |
|     ! 0 | 6204 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6205 | `		return PH7_OK;` |
|       - | 6206 | `	}` |
|       - | 6207 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 6208 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6209 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6210 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 6211 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 6212 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6213 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6214 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6215 | `			"TypeError",` |
|       - | 6216 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6217 | `			ph7_type_name(apArg[1])` |
|       - | 6218 | `			);` |
|       - | 6219 | `	}` |
|       - | 6220 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6221 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6222 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 6223 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6224 | `		int len;` |
|       3 | 6225 | `		sxu8 bReal = FALSE;` |
|       3 | 6226 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6227 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6228 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6229 | `				"TypeError",` |
|       - | 6230 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6231 | `				);` |
|       - | 6232 | `		}` |
|     ! 0 | 6233 | `		if( bReal ){` |
|       - | 6234 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6235 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6236 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6237 | `				zStr` |
|       - | 6238 | `				);` |
|     ! 0 | 6239 | `		}` |
|     ! 0 | 6240 | `	}` |
|       - | 6241 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6242 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6243 | `	 * later via ph7_value_to_int. */` |
|      38 | 6244 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6245 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6246 | `		sxi64 i = (sxi64)d;` |
|       3 | 6247 | `		if( d != (double)i ){` |
|       4 | 6248 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6249 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6250 | `				d` |
|       - | 6251 | `				);` |
|       1 | 6252 | `		}` |
|       1 | 6253 | `	}` |
|       - | 6254 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6255 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6256 | `	{` |
|      38 | 6257 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 6258 | `		if( nSizeSigned < 1 ){` |
|       - | 6259 | `			/* size <= 0 -> ValueError */` |
|       5 | 6260 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6261 | `				"ValueError",` |
|       - | 6262 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6263 | `				);` |
|       - | 6264 | `		}` |
|      34 | 6265 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6266 | `	}` |
|      34 | 6267 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6268 | `		/* Return the whole array */` |
|       3 | 6269 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6270 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6271 | `		return PH7_OK;` |
|       - | 6272 | `	}` |
|      32 | 6273 | `	bPreserve = 0;` |
|      32 | 6274 | `	if( nArg > 2 ){` |
|       - | 6275 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6276 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6277 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6278 | `		 * normally, matching PHP behaviour. */` |
|      45 | 6279 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 6280 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6281 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 6282 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6283 | `				"TypeError",` |
|       - | 6284 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6285 | `				ph7_type_name(apArg[2])` |
|       - | 6286 | `				);` |
|       - | 6287 | `		}` |
|      21 | 6288 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6289 | `	}` |
|       - | 6290 | `	/* Start processing */` |
|      27 | 6291 | `	pEntry = pMap->pFirst;` |
|      27 | 6292 | `	nChunk = 0;` |
|      27 | 6293 | `	pChunk = 0;` |
|      27 | 6294 | `	n = pMap->nEntry;` |
|      56 | 6295 | `	for( ;; ){` |
|     113 | 6296 | `		if( n < 1 ){` |
|       - | 6297 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6298 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6299 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6300 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6301 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6302 | `			 * exists. */` |
|      27 | 6303 | `			if( pChunk ){` |
|      27 | 6304 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6305 | `			}` |
|      27 | 6306 | `			break;` |
|       - | 6307 | `		}` |
|      87 | 6308 | `		if( nChunk < 1 ){` |
|      71 | 6309 | `			if( pChunk ){` |
|       - | 6310 | `				/* Put the first chunk */` |
|      45 | 6311 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6312 | `			}` |
|       - | 6313 | `			/* Create a new dimension */` |
|      71 | 6314 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6315 | `												   * will be automatically released as soon we return` |
|       - | 6316 | `												   * from this function */` |
|      71 | 6317 | `			if( pChunk == 0 ){` |
|     ! 0 | 6318 | `				break;` |
|       - | 6319 | `			}` |
|      71 | 6320 | `			nChunk = nSize;` |
|      35 | 6321 | `		}` |
|       - | 6322 | `		/* Insert the entry */` |
|      87 | 6323 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6324 | `		/* Point to the next entry */` |
|      87 | 6325 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6326 | `		nChunk--;` |
|      87 | 6327 | `		n--;` |
|       1 | 6328 | `	}` |
|       - | 6329 | `	/* Return the multidimensional array */` |
|      27 | 6330 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6331 | `	return PH7_OK;` |
|      23 | 6332 |  |
|       - | 6333 | `/*` |
|       - | 6334 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6335 | ` *  Pad array to the specified length with a value.` |
|       - | 6336 | ` * $input` |
|       - | 6337 | ` *   Initial array of values to pad.` |
|       - | 6338 | ` * $pad_size` |
|       - | 6339 | ` *   New size of the array.` |
|       - | 6340 | ` * $pad_value` |
|       - | 6341 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6342 | ` */` |
|      28 | 6343 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6344 |  |
|       - | 6345 | `	ph7_hashmap *pMap;` |
|       - | 6346 | `	ph7_value *pArray;` |
|       - | 6347 | `	int nEntry;` |
|      30 | 6348 | `	if( nArg != 3 ){` |
|      10 | 6349 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6350 | `			"ArgumentCountError",` |
|       - | 6351 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6352 | `			nArg` |
|       - | 6353 | `			);` |
|       - | 6354 | `	}` |
|      24 | 6355 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6356 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6357 | `			"TypeError",` |
|       - | 6358 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6359 | `			ph7_type_name(apArg[0])` |
|       - | 6360 | `			);` |
|       - | 6361 | `	}` |
|       - | 6362 | `	/* Create a new array */` |
|      21 | 6363 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6364 | `	if( pArray == 0 ){` |
|     ! 0 | 6365 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6366 | `	}` |
|       - | 6367 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6368 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6369 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6370 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6371 | `	if( nEntry < 0 ){` |
|       9 | 6372 | `		nEntry = -nEntry;` |
|       9 | 6373 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6374 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6375 | `			/* Insert given items first */` |
|      17 | 6376 | `			while( nEntry > 0 ){` |
|      13 | 6377 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6378 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6379 | `				}` |
|      13 | 6380 | `				nEntry--;` |
|       1 | 6381 | `			}` |
|       - | 6382 | `			/* Merge the two arrays */` |
|       5 | 6383 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6384 | `		}else{` |
|       5 | 6385 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6386 | `		}` |
|      17 | 6387 | `	}else if( nEntry > 0 ){` |
|      11 | 6388 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6389 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6390 | `			/* Merge the two arrays first */` |
|       7 | 6391 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6392 | `			/* Insert given items */` |
|      25 | 6393 | `			while( nEntry > 0 ){` |
|      19 | 6394 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6395 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6396 | `				}` |
|      19 | 6397 | `				nEntry--;` |
|       1 | 6398 | `			}` |
|       4 | 6399 | `		}else{` |
|       5 | 6400 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6401 | `		}` |
|       6 | 6402 | `	}else{` |
|       - | 6403 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6404 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6405 | `	}` |
|       - | 6406 | `	/* Return the new array */` |
|      21 | 6407 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6408 | `	return PH7_OK;` |
|      16 | 6409 |  |
|       - | 6410 | `/*` |
|       - | 6411 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6412 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6413 | ` * Parameters` |
|       - | 6414 | ` * $array` |
|       - | 6415 | ` *   The array in which elements are replaced.` |
|       - | 6416 | ` * $array1` |
|       - | 6417 | ` *   The array from which elements will be extracted.` |
|       - | 6418 | ` * ....` |
|       - | 6419 | ` *  More arrays from which elements will be extracted.` |
|       - | 6420 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6421 | ` * Return` |
|       - | 6422 | ` *  Returns an array.` |
|       - | 6423 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6424 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6425 | ` */` |
|      22 | 6426 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6427 |  |
|       - | 6428 | `	ph7_hashmap *pMap;` |
|       - | 6429 | `	ph7_value *pArray;` |
|       - | 6430 | `	int i;` |
|      24 | 6431 | `	if( nArg < 1 ){` |
|       3 | 6432 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6433 | `			"ArgumentCountError",` |
|       - | 6434 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6435 | `			);` |
|       - | 6436 | `	}` |
|      22 | 6437 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6438 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6439 | `			"TypeError",` |
|       - | 6440 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6441 | `			ph7_type_name(apArg[0])` |
|       - | 6442 | `			);` |
|       - | 6443 | `	}` |
|       - | 6444 | `	/* Create a new array */` |
|      20 | 6445 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6446 | `	if( pArray == 0 ){` |
|     ! 0 | 6447 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6448 | `		return PH7_OK;` |
|       - | 6449 | `	}` |
|       - | 6450 | `	/* Overwrite from the first array */` |
|      20 | 6451 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6452 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6453 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6454 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6455 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6456 | `			/* Type mismatch -> TypeError */` |
|       4 | 6457 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6458 | `				"TypeError",` |
|       - | 6459 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6460 | `				i + 1,` |
|       2 | 6461 | `				ph7_type_name(apArg[i])` |
|       - | 6462 | `				);` |
|       - | 6463 | `		}` |
|       - | 6464 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6465 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6466 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6467 | `	}` |
|       - | 6468 | `	/* Return the new array */` |
|      17 | 6469 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6470 | `	return PH7_OK;` |
|      13 | 6471 |  |
|       - | 6472 | `/*` |
|       - | 6473 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6474 | ` *  Filters elements of an array using a callback function.` |
|       - | 6475 | ` * Parameters` |
|       - | 6476 | ` *  $input` |
|       - | 6477 | ` *    The array to iterate over` |
|       - | 6478 | ` * $callback` |
|       - | 6479 | ` *    The callback function to use` |
|       - | 6480 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6481 | ` *    will be removed.` |
|       - | 6482 | ` * Return` |
|       - | 6483 | ` *  The filtered array.` |
|       - | 6484 | ` */` |
|      20 | 6485 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6486 |  |
|       - | 6487 | `	ph7_hashmap_node *pEntry;` |
|       - | 6488 | `	ph7_hashmap *pMap;` |
|       - | 6489 | `	ph7_value *pArray;` |
|       - | 6490 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6491 | `	ph7_value *pValue;` |
|       - | 6492 | `	sxi32 rc;` |
|       - | 6493 | `	int keep;` |
|       - | 6494 | `	sxu32 n;` |
|      22 | 6495 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6496 | `		/* Invalid arguments,return NULL */` |
|       5 | 6497 | `		ph7_result_null(pCtx);` |
|       5 | 6498 | `		return PH7_OK;` |
|       - | 6499 | `	}` |
|       - | 6500 | `	/* Create a new array */` |
|      18 | 6501 | `	pArray = ph7_context_new_array(pCtx);` |
|      18 | 6502 | `	if( pArray == 0 ){` |
|     ! 0 | 6503 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6504 | `		return PH7_OK;` |
|       - | 6505 | `	}` |
|       - | 6506 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 6507 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      18 | 6508 | `	pEntry = pMap->pFirst;` |
|      18 | 6509 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      18 | 6510 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6511 | `	/* Perform the requested operation */` |
|      68 | 6512 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6513 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      56 | 6514 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      56 | 6515 | `		if( pValue == 0 ){` |
|       - | 6516 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6517 | `			keep = FALSE;` |
|      56 | 6518 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6519 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6520 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6521 | `				* silently dropped the element.  Emit similar message. */` |
|      28 | 6522 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6523 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6524 | `					int len;` |
|       3 | 6525 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6526 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6527 | `						"TypeError",` |
|       - | 6528 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6529 | `						zName` |
|       - | 6530 | `						);` |
|     ! 0 | 6531 | `				}else{` |
|     ! 0 | 6532 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6533 | `						"TypeError",` |
|       - | 6534 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6535 | `						ph7_type_name(apArg[1])` |
|       - | 6536 | `						);` |
|       - | 6537 | `				}` |
|       - | 6538 | `			}` |
|      25 | 6539 | `			keep = FALSE;` |
|      25 | 6540 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      25 | 6541 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 6542 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6543 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 6544 | `				return PH7_EXCEPTION;` |
|       - | 6545 | `			}` |
|      23 | 6546 | `			if( rc == SXRET_OK ){` |
|       - | 6547 | `				/* Perform a boolean cast */` |
|      23 | 6548 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6549 | `			}` |
|      23 | 6550 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6551 | `		}else{` |
|       - | 6552 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6553 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6554 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6555 | `			 */` |
|      29 | 6556 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6557 | `		}` |
|      51 | 6558 | `		if( keep ){` |
|       - | 6559 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6560 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6561 | `		}` |
|       - | 6562 | `		/* Point to the next entry */` |
|      51 | 6563 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6564 | `	}` |
|      13 | 6565 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6566 | `	return PH7_OK;` |
|      12 | 6567 |  |
|       - | 6568 | `/*` |
|       - | 6569 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 6570 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 6571 | ` * Parameters` |
|       - | 6572 | ` *  $callback` |
|       - | 6573 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 6574 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 6575 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 6576 | ` *   are zipped together.` |
|       - | 6577 | ` *  $array` |
|       - | 6578 | ` *   The first array to run through the callback function.` |
|       - | 6579 | ` *  $arrays` |
|       - | 6580 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 6581 | ` * Return` |
|       - | 6582 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 6583 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 6584 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 6585 | ` *  padding shorter arrays with NULL.` |
|       - | 6586 | ` */` |
|      46 | 6587 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6588 |  |
|       - | 6589 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6590 | `	ph7_hashmap_node *pEntry;` |
|       - | 6591 | `	ph7_hashmap *pMap;` |
|       - | 6592 | `	ph7_vm *pVm;` |
|       - | 6593 | `	int bNullCallback;` |
|       - | 6594 | `	sxi32 rc;` |
|       - | 6595 | `	int i;` |
|       - | 6596 | `	sxu32 n;` |
|      48 | 6597 | `	if( nArg < 2 ){` |
|       7 | 6598 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6599 | `			"ArgumentCountError",` |
|       - | 6600 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6601 | `			nArg` |
|       - | 6602 | `			);` |
|       - | 6603 | `	}` |
|      44 | 6604 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      44 | 6605 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6606 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6607 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6608 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6609 | `				"TypeError",` |
|       - | 6610 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6611 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6612 | `				zFunc` |
|       - | 6613 | `				);` |
|       - | 6614 | `		}` |
|       3 | 6615 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6616 | `			"TypeError",` |
|       - | 6617 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6618 | `			"no array or string given"` |
|       - | 6619 | `			);` |
|       - | 6620 | `	}` |
|       - | 6621 | `	/* Every remaining argument must be an array */` |
|      88 | 6622 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      52 | 6623 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 6624 | `			if( i == 1 ){` |
|       4 | 6625 | `				return PH7_VmThrowException(pCtx,` |
|       - | 6626 | `					"TypeError",` |
|       - | 6627 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6628 | `					ph7_type_name(apArg[1])` |
|       - | 6629 | `					);` |
|       - | 6630 | `			}` |
|     ! 0 | 6631 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6632 | `				"TypeError",` |
|       - | 6633 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 6634 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 6635 | `				);` |
|       - | 6636 | `		}` |
|      26 | 6637 | `	}` |
|      38 | 6638 | `	pVm = pCtx->pVm;` |
|       - | 6639 | `	/* Create a new array */` |
|      38 | 6640 | `	pArray = ph7_context_new_array(pCtx);` |
|      38 | 6641 | `	if( pArray == 0 ){` |
|     ! 0 | 6642 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6643 | `		return PH7_OK;` |
|       - | 6644 | `	}` |
|      38 | 6645 | `	PH7_MemObjInit(pVm,&sResult);` |
|      38 | 6646 | `	PH7_MemObjInit(pVm,&sKey);` |
|      38 | 6647 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      38 | 6648 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      38 | 6649 | `	if( nArg == 2 ){` |
|       - | 6650 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      28 | 6651 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      28 | 6652 | `		pEntry = pMap->pFirst;` |
|      82 | 6653 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6654 | `			/* Extract the node value */` |
|      58 | 6655 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|      58 | 6656 | `			if( pValue ){` |
|       - | 6657 | `				/* Extract the node key */` |
|      58 | 6658 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      58 | 6659 | `				if( bNullCallback ){` |
|       - | 6660 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 6661 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6662 | `				}else{` |
|       - | 6663 | `					/* Invoke the supplied callback */` |
|      48 | 6664 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|      48 | 6665 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 6666 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 6667 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       3 | 6668 | `						PH7_MemObjRelease(&sKey);` |
|       3 | 6669 | `						PH7_MemObjRelease(&sResult);` |
|       3 | 6670 | `						return PH7_EXCEPTION;` |
|       - | 6671 | `					}` |
|       - | 6672 | `					/* Insert the callback return value */` |
|      46 | 6673 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6674 | `				}` |
|      56 | 6675 | `				PH7_MemObjRelease(&sKey);` |
|      56 | 6676 | `				PH7_MemObjRelease(&sResult);` |
|      27 | 6677 | `			}` |
|       - | 6678 | `			/* Point to the next entry */` |
|      56 | 6679 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 6680 | `		}` |
|      14 | 6681 | `	}else{` |
|       - | 6682 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 6683 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 6684 | `		int nArrays = nArg - 1;` |
|       - | 6685 | `		ph7_hashmap_node **apCur;` |
|       - | 6686 | `		ph7_value **apCallArg;` |
|       - | 6687 | `		ph7_value sNull;` |
|      11 | 6688 | `		sxu32 nMax = 0;` |
|      11 | 6689 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 6690 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 6691 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 6692 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 6693 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 6694 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6695 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6696 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 6697 | `			return PH7_OK;` |
|       - | 6698 | `		}` |
|      11 | 6699 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 6700 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 6701 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 6702 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 6703 | `			apCur[i] = pMap->pFirst;` |
|      23 | 6704 | `			if( pMap->nEntry > nMax ){` |
|      13 | 6705 | `				nMax = pMap->nEntry;` |
|       6 | 6706 | `			}` |
|      12 | 6707 | `		}` |
|      35 | 6708 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 6709 | `			ph7_value *pZip = 0;` |
|      25 | 6710 | `			if( bNullCallback ){` |
|       - | 6711 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 6712 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 6713 | `			}` |
|      79 | 6714 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 6715 | `				ph7_value *pv = &sNull;` |
|      55 | 6716 | `				if( apCur[i] ){` |
|      53 | 6717 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 6718 | `					if( pNodeVal ){` |
|      53 | 6719 | `						pv = pNodeVal;` |
|      26 | 6720 | `					}` |
|      53 | 6721 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 6722 | `				}` |
|      55 | 6723 | `				if( bNullCallback ){` |
|       9 | 6724 | `					if( pZip ){` |
|       9 | 6725 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 6726 | `					}` |
|       5 | 6727 | `				}else{` |
|      47 | 6728 | `					apCallArg[i] = pv;` |
|       - | 6729 | `				}` |
|      28 | 6730 | `			}` |
|      25 | 6731 | `			if( bNullCallback ){` |
|       5 | 6732 | `				if( pZip ){` |
|       5 | 6733 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 6734 | `				}` |
|       3 | 6735 | `			}else{` |
|      21 | 6736 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 6737 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6738 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 6739 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 6740 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 6741 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6742 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6743 | `					return PH7_EXCEPTION;` |
|       - | 6744 | `				}` |
|      21 | 6745 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 6746 | `				PH7_MemObjRelease(&sResult);` |
|       - | 6747 | `			}` |
|      13 | 6748 | `		}` |
|      11 | 6749 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 6750 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 6751 | `		PH7_MemObjRelease(&sNull);` |
|       - | 6752 | `	}` |
|      36 | 6753 | `	PH7_MemObjRelease(&sKey);` |
|      36 | 6754 | `	PH7_MemObjRelease(&sResult);` |
|      36 | 6755 | `	ph7_result_value(pCtx,pArray);` |
|      36 | 6756 | `	return PH7_OK;` |
|      25 | 6757 |  |
|       - | 6758 | `/*` |
|       - | 6759 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6760 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6761 | ` * Parameters` |
|       - | 6762 | ` *  $array` |
|       - | 6763 | ` *   The input array.` |
|       - | 6764 | ` *  $callback` |
|       - | 6765 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6766 | ` *  $initial` |
|       - | 6767 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6768 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6769 | ` * Return` |
|       - | 6770 | ` *  Returns the resulting value.` |
|       - | 6771 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6772 | ` */` |
|      32 | 6773 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6774 |  |
|       - | 6775 | `	ph7_hashmap_node *pEntry;` |
|       - | 6776 | `	ph7_hashmap *pMap;` |
|       - | 6777 | `	ph7_value *pValue;` |
|       - | 6778 | `	ph7_value sResult;` |
|       - | 6779 | `	sxi32 rc;` |
|       - | 6780 | `	sxu32 n;` |
|      34 | 6781 | `	if( nArg < 2 ){` |
|       7 | 6782 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6783 | `			"ArgumentCountError",` |
|       - | 6784 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6785 | `			nArg` |
|       - | 6786 | `			);` |
|       - | 6787 | `	}` |
|      30 | 6788 | `	if( nArg > 3 ){` |
|       4 | 6789 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6790 | `			"ArgumentCountError",` |
|       - | 6791 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6792 | `			nArg` |
|       - | 6793 | `			);` |
|       - | 6794 | `	}` |
|      28 | 6795 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6796 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6797 | `			"TypeError",` |
|       - | 6798 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6799 | `			ph7_type_name(apArg[0])` |
|       - | 6800 | `			);` |
|       - | 6801 | `	}` |
|      26 | 6802 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6803 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6804 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6805 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6806 | `				"TypeError",` |
|       - | 6807 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6808 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6809 | `				zFunc` |
|       - | 6810 | `				);` |
|       - | 6811 | `		}` |
|       7 | 6812 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6813 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6814 | `				"TypeError",` |
|       - | 6815 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6816 | `				"array callback must have exactly two members"` |
|       - | 6817 | `				);` |
|       - | 6818 | `		}` |
|       5 | 6819 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6820 | `			"TypeError",` |
|       - | 6821 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6822 | `			"no array or string given"` |
|       - | 6823 | `			);` |
|       - | 6824 | `	}` |
|       - | 6825 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 6826 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6827 | `	/* Assume a NULL initial value */` |
|      17 | 6828 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      17 | 6829 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      17 | 6830 | `	if( nArg > 2 ){` |
|       - | 6831 | `		/* Set the initial value */` |
|      11 | 6832 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6833 | `	}` |
|       - | 6834 | `	/* Perform the requested operation */` |
|      17 | 6835 | `	pEntry = pMap->pFirst;` |
|      45 | 6836 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6837 | `		/* Extract the node value */` |
|      31 | 6838 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6839 | `		/* Invoke the supplied callback */` |
|      31 | 6840 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      31 | 6841 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 6842 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6843 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 6844 | `			return PH7_EXCEPTION;` |
|       - | 6845 | `		}` |
|       - | 6846 | `		/* Point to the next entry */` |
|      29 | 6847 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6848 | `	}` |
|      15 | 6849 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6850 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6851 | `	return PH7_OK;` |
|      18 | 6852 |  |
|       - | 6853 | `/*` |
|       - | 6854 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6855 | ` *  Apply a user function to every member of an array.` |
|       - | 6856 | ` * Parameters` |
|       - | 6857 | ` *  $array` |
|       - | 6858 | ` *   The input array.` |
|       - | 6859 | ` *  $funcname` |
|       - | 6860 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6861 | ` *   the first, and the key/index second.` |
|       - | 6862 | ` * Note:` |
|       - | 6863 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6864 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6865 | ` *  be made in the original array itself.` |
|       - | 6866 | ` *  $userdata` |
|       - | 6867 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6868 | ` *   to the callback funcname.` |
|       - | 6869 | ` * Return` |
|       - | 6870 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6871 | ` */` |
|      38 | 6872 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6873 |  |
|       - | 6874 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6875 | `	ph7_hashmap_node *pEntry;` |
|       - | 6876 | `	ph7_hashmap *pMap;` |
|       - | 6877 | `	sxu32 n;` |
|      40 | 6878 | `	if( nArg < 2 ){` |
|       7 | 6879 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6880 | `			"ArgumentCountError",` |
|       - | 6881 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6882 | `			nArg` |
|       - | 6883 | `			);` |
|       - | 6884 | `	}` |
|      36 | 6885 | `	if( nArg > 3 ){` |
|       4 | 6886 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6887 | `			"ArgumentCountError",` |
|       - | 6888 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6889 | `			nArg` |
|       - | 6890 | `			);` |
|       - | 6891 | `	}` |
|      34 | 6892 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6893 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6894 | `			"TypeError",` |
|       - | 6895 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6896 | `			ph7_type_name(apArg[0])` |
|       - | 6897 | `			);` |
|       - | 6898 | `	}` |
|      32 | 6899 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6900 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6901 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6902 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6903 | `				"TypeError",` |
|       - | 6904 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6905 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6906 | `				zFunc` |
|       - | 6907 | `				);` |
|       - | 6908 | `		}` |
|       9 | 6909 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6910 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6911 | `				"TypeError",` |
|       - | 6912 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6913 | `				"array callback must have exactly two members"` |
|       - | 6914 | `				);` |
|       - | 6915 | `		}` |
|       5 | 6916 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6917 | `			"TypeError",` |
|       - | 6918 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6919 | `			"no array or string given"` |
|       - | 6920 | `			);` |
|       - | 6921 | `	}` |
|      21 | 6922 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6923 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6924 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 6925 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 6926 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 6927 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6928 | `	/* Perform the desired operation */` |
|      21 | 6929 | `	pEntry = pMap->pFirst;` |
|      61 | 6930 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6931 | `		/* Extract the node value */` |
|      43 | 6932 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 6933 | `		if( pValue ){` |
|       - | 6934 | `			sxi32 rcW;` |
|       - | 6935 | `			/* Extract the entry key */` |
|      43 | 6936 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6937 | `			/* Invoke the supplied callback */` |
|      43 | 6938 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 6939 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 6940 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 6941 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6942 | `				return PH7_EXCEPTION;` |
|       - | 6943 | `			}` |
|      20 | 6944 | `		}` |
|       - | 6945 | `		/* Point to the next entry */` |
|      41 | 6946 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6947 | `	}` |
|       - | 6948 | `	/* All done, return TRUE */` |
|      19 | 6949 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6950 | `	return PH7_OK;` |
|      21 | 6951 |  |
|       - | 6952 | `/*` |
|       - | 6953 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6954 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6955 | ` */` |
|      22 | 6956 | `static sxi32 HashmapWalkRecursive(` |
|       - | 6957 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6958 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6959 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6960 | `	int iNest             /* Nesting level */` |
|       - | 6961 | `	)` |
|       1 | 6962 |  |
|       - | 6963 | `	ph7_hashmap_node *pEntry;` |
|       - | 6964 | `	ph7_value *pValue,sKey;` |
|       - | 6965 | `	sxi32 rc;` |
|       - | 6966 | `	sxu32 n;` |
|       - | 6967 | `	/* Iterate through hashmap entries */` |
|      23 | 6968 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6969 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6970 | `	pEntry = pMap->pFirst;` |
|      59 | 6971 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6972 | `		/* Extract the node value */` |
|      37 | 6973 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6974 | `		if( pValue ){` |
|      37 | 6975 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6976 | `				if( iNest < 32 ){` |
|       - | 6977 | `					/* Recurse */` |
|      11 | 6978 | `					iNest++;` |
|      11 | 6979 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6980 | `					iNest--;` |
|      11 | 6981 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6982 | `						return PH7_EXCEPTION;` |
|       - | 6983 | `					}` |
|       5 | 6984 | `				}` |
|       6 | 6985 | `			}else{` |
|       - | 6986 | `				/* Extract the node key */` |
|      27 | 6987 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6988 | `				/* Invoke the supplied callback */` |
|      27 | 6989 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6990 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 6991 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 6992 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 6993 | `					return PH7_EXCEPTION;` |
|       - | 6994 | `				}` |
|       - | 6995 | `			}` |
|      18 | 6996 | `		}` |
|       - | 6997 | `		/* Point to the next entry */` |
|      37 | 6998 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6999 | `	}` |
|      23 | 7000 | `	return PH7_OK;` |
|      12 | 7001 |  |
|       - | 7002 | `/*` |
|       - | 7003 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 7004 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 7005 | ` * Parameters` |
|       - | 7006 | ` *  $array` |
|       - | 7007 | ` *   The input array.` |
|       - | 7008 | ` *  $funcname` |
|       - | 7009 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 7010 | ` *   the first, and the key/index second.` |
|       - | 7011 | ` * Note:` |
|       - | 7012 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 7013 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 7014 | ` *  be made in the original array itself.` |
|       - | 7015 | ` *  $userdata` |
|       - | 7016 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 7017 | ` *   to the callback funcname.` |
|       - | 7018 | ` * Return` |
|       - | 7019 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 7020 | ` */` |
|      30 | 7021 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 7022 |  |
|       - | 7023 | `	ph7_hashmap *pMap;` |
|      32 | 7024 | `	if( nArg < 2 ){` |
|       7 | 7025 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7026 | `			"ArgumentCountError",` |
|       - | 7027 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 7028 | `			nArg` |
|       - | 7029 | `			);` |
|       - | 7030 | `	}` |
|      28 | 7031 | `	if( nArg > 3 ){` |
|       4 | 7032 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7033 | `			"ArgumentCountError",` |
|       - | 7034 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 7035 | `			nArg` |
|       - | 7036 | `			);` |
|       - | 7037 | `	}` |
|      26 | 7038 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7039 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7040 | `			"TypeError",` |
|       - | 7041 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7042 | `			ph7_type_name(apArg[0])` |
|       - | 7043 | `			);` |
|       - | 7044 | `	}` |
|      24 | 7045 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 7046 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7047 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7048 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7049 | `				"TypeError",` |
|       - | 7050 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7051 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7052 | `				zFunc` |
|       - | 7053 | `				);` |
|       - | 7054 | `		}` |
|       9 | 7055 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 7056 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7057 | `				"TypeError",` |
|       - | 7058 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7059 | `				"array callback must have exactly two members"` |
|       - | 7060 | `				);` |
|       - | 7061 | `		}` |
|       5 | 7062 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7063 | `			"TypeError",` |
|       - | 7064 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7065 | `			"no array or string given"` |
|       - | 7066 | `			);` |
|       - | 7067 | `	}` |
|       - | 7068 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 7069 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 7070 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7071 | `	/* Perform the desired operation */` |
|      13 | 7072 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 7073 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7074 | `		return PH7_EXCEPTION;` |
|       - | 7075 | `	}` |
|       - | 7076 | `	/* All done, return TRUE */` |
|      13 | 7077 | `	ph7_result_bool(pCtx,1);` |
|      13 | 7078 | `	return PH7_OK;` |
|      17 | 7079 |  |
|       - | 7080 | `/*` |
|       - | 7081 | ` * bool array_is_list(array $array)` |
|       - | 7082 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 7083 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 7084 | ` * Return` |
|       - | 7085 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 7086 | ` */` |
|       - | 7087 | `/*` |
|       - | 7088 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 7089 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 7090 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 7091 | ` */` |
|      60 | 7092 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       1 | 7093 |  |
|      61 | 7094 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|      61 | 7095 | `	sxi64 iExpect = 0;` |
|       - | 7096 | `	sxu32 n;` |
|     129 | 7097 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     101 | 7098 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 7099 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|      33 | 7100 | `			return 0;` |
|       - | 7101 | `		}` |
|      69 | 7102 | `		++iExpect;` |
|      69 | 7103 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      35 | 7104 | `	}` |
|      29 | 7105 | `	return 1;` |
|      31 | 7106 |  |
|      12 | 7107 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7108 |  |
|      13 | 7109 | `	if( nArg < 1 ){` |
|     ! 0 | 7110 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7111 | `			"ArgumentCountError",` |
|       - | 7112 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 7113 | `			);` |
|       - | 7114 | `	}` |
|      13 | 7115 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7116 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7117 | `			"TypeError",` |
|       - | 7118 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7119 | `			ph7_type_name(apArg[0])` |
|       - | 7120 | `			);` |
|       - | 7121 | `	}` |
|      13 | 7122 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 7123 | `	return PH7_OK;` |
|       7 | 7124 |  |
|       - | 7125 | `/*` |
|       - | 7126 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 7127 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 7128 | ` * array_column() for both the column value and the index key.` |
|       - | 7129 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 7130 | ` * container or the key is absent.` |
|       - | 7131 | ` */` |
|      32 | 7132 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 7133 |  |
|      33 | 7134 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 7135 | `		ph7_hashmap_node *pNode;` |
|      25 | 7136 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 7137 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 7138 | `		}` |
|      11 | 7139 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 7140 | `		ph7_value sName;` |
|       - | 7141 | `		const char *zName;` |
|       - | 7142 | `		ph7_value *pAttr;` |
|       - | 7143 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 7144 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 7145 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 7146 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 7147 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 7148 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 7149 | `		PH7_MemObjRelease(&sName);` |
|       9 | 7150 | `		return pAttr;` |
|       - | 7151 | `	}` |
|       5 | 7152 | `	return 0;` |
|      17 | 7153 |  |
|       - | 7154 | `/*` |
|       - | 7155 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 7156 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 7157 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 7158 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 7159 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 7160 | ` *  Each row may be an array or an object.` |
|       - | 7161 | ` */` |
|      12 | 7162 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7163 |  |
|       - | 7164 | `	ph7_hashmap_node *pNode;` |
|       - | 7165 | `	ph7_hashmap *pMap;` |
|       - | 7166 | `	ph7_value *pArray;` |
|       - | 7167 | `	ph7_value *pRow;` |
|       - | 7168 | `	ph7_value *pCol;` |
|       - | 7169 | `	ph7_value *pIdx;` |
|       - | 7170 | `	int bWantCol;` |
|       - | 7171 | `	int bWantIdx;` |
|       - | 7172 | `	sxu32 n;` |
|      13 | 7173 | `	if( nArg < 2 ){` |
|     ! 0 | 7174 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7175 | `			"ArgumentCountError",` |
|       - | 7176 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 7177 | `			nArg` |
|       - | 7178 | `			);` |
|       - | 7179 | `	}` |
|      13 | 7180 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7181 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7182 | `			"TypeError",` |
|       - | 7183 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7184 | `			ph7_type_name(apArg[0])` |
|       - | 7185 | `			);` |
|       - | 7186 | `	}` |
|      13 | 7187 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 7188 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 7189 | `	if( pArray == 0 ){` |
|     ! 0 | 7190 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7191 | `		return PH7_OK;` |
|       - | 7192 | `	}` |
|       - | 7193 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 7194 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 7195 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 7196 | `	pNode = pMap->pFirst;` |
|      33 | 7197 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 7198 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 7199 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 7200 | `		if( pRow == 0 ){` |
|     ! 0 | 7201 | `			continue;` |
|       - | 7202 | `		}` |
|      21 | 7203 | `		if( bWantCol ){` |
|      19 | 7204 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 7205 | `			if( pCol == 0 ){` |
|       - | 7206 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 7207 | `				continue;` |
|       - | 7208 | `			}` |
|       9 | 7209 | `		}else{` |
|       3 | 7210 | `			pCol = pRow;` |
|       - | 7211 | `		}` |
|      19 | 7212 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 7213 | `		if( pIdx ){` |
|      13 | 7214 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 7215 | `		}else{` |
|       7 | 7216 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 7217 | `		}` |
|      10 | 7218 | `	}` |
|      13 | 7219 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 7220 | `	return PH7_OK;` |
|       7 | 7221 |  |
|       - | 7222 | `/*` |
|       - | 7223 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 7224 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 7225 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 7226 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 7227 | ` */` |
|      28 | 7228 | `static sxi32 HashmapCallbackSearch(` |
|       - | 7229 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 7230 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 7231 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 7232 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 7233 | `	)` |
|       1 | 7234 |  |
|       - | 7235 | `	ph7_hashmap_node *pEntry;` |
|       - | 7236 | `	ph7_hashmap *pMap;` |
|       - | 7237 | `	ph7_value *pValue;` |
|       - | 7238 | `	ph7_value *apCbArg[2];` |
|       - | 7239 | `	ph7_value sKey;` |
|       - | 7240 | `	ph7_value sResult;` |
|       - | 7241 | `	sxi32 rc;` |
|       - | 7242 | `	sxu32 n;` |
|      29 | 7243 | `	*ppMatch = 0;` |
|      29 | 7244 | `	if( nArg < 2 ){` |
|     ! 0 | 7245 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7246 | `			"ArgumentCountError",` |
|       - | 7247 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 7248 | `			zName,nArg` |
|       - | 7249 | `			);` |
|       - | 7250 | `	}` |
|      29 | 7251 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7252 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7253 | `			"TypeError",` |
|       - | 7254 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7255 | `			zName,ph7_type_name(apArg[0])` |
|       - | 7256 | `			);` |
|       - | 7257 | `	}` |
|      29 | 7258 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7259 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7260 | `			"TypeError",` |
|       - | 7261 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 7262 | `			zName,ph7_type_name(apArg[1])` |
|       - | 7263 | `			);` |
|       - | 7264 | `	}` |
|      29 | 7265 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 7266 | `	pEntry = pMap->pFirst;` |
|      29 | 7267 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 7268 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 7269 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 7270 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 7271 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 7272 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 7273 | `		if( pValue ){` |
|       - | 7274 | `			/* The callback receives ($value, $key). */` |
|      59 | 7275 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 7276 | `			apCbArg[0] = pValue;` |
|      59 | 7277 | `			apCbArg[1] = &sKey;` |
|      59 | 7278 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 7279 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 7280 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7281 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7282 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7283 | `				return PH7_EXCEPTION;` |
|       - | 7284 | `			}` |
|      59 | 7285 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 7286 | `				*ppMatch = pEntry;` |
|      15 | 7287 | `				break;` |
|       - | 7288 | `			}` |
|      22 | 7289 | `		}` |
|      45 | 7290 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 7291 | `	}` |
|      29 | 7292 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 7293 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 7294 | `	return PH7_OK;` |
|      15 | 7295 |  |
|       - | 7296 | `/*` |
|       - | 7297 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 7298 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 7299 | ` *  is truthy, or NULL if none match.` |
|       - | 7300 | ` */` |
|       6 | 7301 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7302 |  |
|       - | 7303 | `	ph7_hashmap_node *pMatch;` |
|       - | 7304 | `	ph7_value *pVal;` |
|       - | 7305 | `	sxi32 rc;` |
|       7 | 7306 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 7307 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7308 | `		return rc;` |
|       - | 7309 | `	}` |
|       7 | 7310 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 7311 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 7312 | `	}else{` |
|       3 | 7313 | `		ph7_result_null(pCtx);` |
|       - | 7314 | `	}` |
|       7 | 7315 | `	return PH7_OK;` |
|       4 | 7316 |  |
|       - | 7317 | `/*` |
|       - | 7318 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 7319 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 7320 | ` *  is truthy, or NULL if none match.` |
|       - | 7321 | ` */` |
|       6 | 7322 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7323 |  |
|       - | 7324 | `	ph7_hashmap_node *pMatch;` |
|       - | 7325 | `	sxi32 rc;` |
|       7 | 7326 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 7327 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7328 | `		return rc;` |
|       - | 7329 | `	}` |
|       7 | 7330 | `	if( pMatch == 0 ){` |
|       3 | 7331 | `		ph7_result_null(pCtx);` |
|       6 | 7332 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 7333 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 7334 | `	}else{` |
|       4 | 7335 | `		ph7_result_string(pCtx,` |
|       2 | 7336 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 7337 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 7338 | `	}` |
|       7 | 7339 | `	return PH7_OK;` |
|       4 | 7340 |  |
|       - | 7341 | `/*` |
|       - | 7342 | ` * bool array_any(array $array, callable $callback)` |
|       - | 7343 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 7344 | ` *  FALSE for an empty array.` |
|       - | 7345 | ` */` |
|       8 | 7346 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7347 |  |
|       - | 7348 | `	ph7_hashmap_node *pMatch;` |
|       - | 7349 | `	sxi32 rc;` |
|       9 | 7350 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 7351 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7352 | `		return rc;` |
|       - | 7353 | `	}` |
|       9 | 7354 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 7355 | `	return PH7_OK;` |
|       5 | 7356 |  |
|       - | 7357 | `/*` |
|       - | 7358 | ` * bool array_all(array $array, callable $callback)` |
|       - | 7359 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 7360 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 7361 | ` */` |
|       8 | 7362 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7363 |  |
|       - | 7364 | `	ph7_hashmap_node *pMatch;` |
|       - | 7365 | `	sxi32 rc;` |
|       9 | 7366 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 7367 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7368 | `		return rc;` |
|       - | 7369 | `	}` |
|       9 | 7370 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 7371 | `	return PH7_OK;` |
|       5 | 7372 |  |
|       - | 7373 | `/*` |
|       - | 7374 | ` * Table of hashmap functions.` |
|       - | 7375 | ` */` |
|       - | 7376 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 7377 | `	{"count",             ph7_hashmap_count },` |
|       - | 7378 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 7379 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 7380 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 7381 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 7382 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 7383 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 7384 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 7385 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 7386 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 7387 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 7388 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 7389 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 7390 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 7391 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 7392 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 7393 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 7394 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 7395 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 7396 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 7397 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 7398 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 7399 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 7400 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 7401 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 7402 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 7403 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 7404 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 7405 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 7406 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 7407 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 7408 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 7409 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 7410 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 7411 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 7412 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 7413 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 7414 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 7415 | `	{"array_column",      ph7_hashmap_column  },` |
|       - | 7416 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|       - | 7417 | `	{"array_find",        ph7_hashmap_find    },` |
|       - | 7418 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|       - | 7419 | `	{"array_any",         ph7_hashmap_any     },` |
|       - | 7420 | `	{"array_all",         ph7_hashmap_all     },` |
|       - | 7421 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 7422 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 7423 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 7424 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 7425 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 7426 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 7427 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 7428 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 7429 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 7430 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 7431 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 7432 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 7433 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 7434 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 7435 | `	{"range",             ph7_hashmap_range   },` |
|       - | 7436 | `	{"current",           ph7_hashmap_current },` |
|       - | 7437 | `	{"each",              ph7_hashmap_each    },` |
|       - | 7438 | `	{"pos",               ph7_hashmap_current },` |
|       - | 7439 | `	{"next",              ph7_hashmap_next    },` |
|       - | 7440 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 7441 | `	{"end",               ph7_hashmap_end     },` |
|       - | 7442 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 7443 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 7444 | `};` |
|       - | 7445 | `/*` |
|       - | 7446 | ` * Register the built-in hashmap functions defined above.` |
|       - | 7447 | ` */` |
|    2826 | 7448 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 7449 |  |
|       - | 7450 | `	sxu32 n;` |
|  192170 | 7451 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  189344 | 7452 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   94673 | 7453 | `	}` |
|    2828 | 7454 |  |
|       - | 7455 | `/*` |
|       - | 7456 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 7457 | ` * the BLOB given as the first argument.` |
|       - | 7458 | ` * This function is typically invoked when the user issue a call to` |
|       - | 7459 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 7460 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 7461 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 7462 | ` */` |
|      26 | 7463 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 7464 |  |
|       - | 7465 | `	ph7_hashmap_node *pEntry;` |
|       - | 7466 | `	ph7_value *pObj;` |
|      28 | 7467 | `	sxu32 n = 0;` |
|       - | 7468 | `	int isRef;` |
|       - | 7469 | `	sxi32 rc;` |
|       - | 7470 | `	int i;` |
|      28 | 7471 | `	if( nDepth > 31 ){` |
|       - | 7472 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 7473 | `		/* Nesting limit reached */` |
|     ! 0 | 7474 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 7475 | `		if( ShowType ){` |
|     ! 0 | 7476 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 7477 | `		}` |
|     ! 0 | 7478 | `		return SXERR_LIMIT;` |
|       - | 7479 | `	}` |
|       - | 7480 | `	/* Point to the first inserted entry */` |
|      28 | 7481 | `	pEntry = pMap->pFirst;` |
|      28 | 7482 | `	rc = SXRET_OK;` |
|      28 | 7483 | `	if( !ShowType ){` |
|      15 | 7484 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 7485 | `	}` |
|       - | 7486 | `	/* Total entries */` |
|      28 | 7487 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 7488 | `#ifdef __WINNT__` |
|       2 | 7489 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7490 | `#else` |
|      26 | 7491 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7492 | `#endif` |
|      62 | 7493 | `	for(;;){` |
|     126 | 7494 | `		if( n >= pMap->nEntry ){` |
|      28 | 7495 | `			break;` |
|       - | 7496 | `		}` |
|     198 | 7497 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 7498 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 7499 | `		}` |
|       - | 7500 | `		/* Dump key */` |
|     100 | 7501 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 7502 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 7503 | `		}else{` |
|     101 | 7504 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 7505 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 7506 | `		}` |
|       - | 7507 | `#ifdef __WINNT__` |
|       2 | 7508 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7509 | `#else` |
|      98 | 7510 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7511 | `#endif` |
|       - | 7512 | `		/* Dump node value */` |
|     100 | 7513 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 7514 | `		isRef = 0;` |
|     100 | 7515 | `		if( pObj ){` |
|     100 | 7516 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 7517 | `				/* Referenced object */` |
|     ! 0 | 7518 | `				isRef = 1;` |
|     ! 0 | 7519 | `			}` |
|     100 | 7520 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 7521 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 7522 | `				break;` |
|       - | 7523 | `			}` |
|      49 | 7524 | `		}` |
|       - | 7525 | `		/* Point to the next entry */` |
|     100 | 7526 | `		n++;` |
|     100 | 7527 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 7528 | `	}` |
|      54 | 7529 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 7530 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 7531 | `	}` |
|      28 | 7532 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 7533 | `	return rc;` |
|      15 | 7534 |  |
|       - | 7535 | `/*` |
|       - | 7536 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 7537 | ` * retrieved entry.` |
|       - | 7538 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 7539 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 7540 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 7541 | ` * a value different from PH7_OK.` |
|       - | 7542 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 7543 | ` */` |
|   29860 | 7544 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 7545 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 7546 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 7547 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 7548 | `	)` |
|       2 | 7549 |  |
|       - | 7550 | `	ph7_hashmap_node *pEntry;` |
|       - | 7551 | `	ph7_value sKey,sValue;` |
|       - | 7552 | `	sxi32 rc;` |
|       - | 7553 | `	sxu32 n;` |
|       - | 7554 | `	/* Initialize walker parameter */` |
|   29862 | 7555 | `	rc = SXRET_OK;` |
|   29862 | 7556 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   29862 | 7557 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   29862 | 7558 | `	n = pMap->nEntry;` |
|   29862 | 7559 | `	pEntry = pMap->pFirst;` |
|       - | 7560 | `	/* Start the iteration process */` |
|   74677 | 7561 | `	for(;;){` |
|  149356 | 7562 | `		if( n < 1 ){` |
|   29862 | 7563 | `			break;` |
|       - | 7564 | `		}` |
|       - | 7565 | `		/* Extract a copy of the key and a copy the current value */` |
|  119496 | 7566 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  119496 | 7567 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 7568 | `		/* Invoke the user callback */` |
|  119496 | 7569 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 7570 | `		/* Release the copy of the key and the value */` |
|  119496 | 7571 | `		PH7_MemObjRelease(&sKey);` |
|  119496 | 7572 | `		PH7_MemObjRelease(&sValue);` |
|  119496 | 7573 | `		if( rc != PH7_OK ){` |
|       - | 7574 | `			/* Callback request an operation abort */` |
|     ! 0 | 7575 | `			return SXERR_ABORT;` |
|       - | 7576 | `		}` |
|       - | 7577 | `		/* Point to the next entry */` |
|  119496 | 7578 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  119496 | 7579 | `		n--;` |
|       2 | 7580 | `	}` |
|       - | 7581 | `	/* All done */` |
|   29862 | 7582 | `	return SXRET_OK;` |
|   14932 | 7583 |  |
|       - | 7584 |  |
