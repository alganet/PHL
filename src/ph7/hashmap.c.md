# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2562/3101 lines (82.62%)

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
| 2732414 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2732416 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  215172 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  215174 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  215174 |   29 | `	sxu32 nH = 5381;` |
|  215174 |   30 | `	zEnd = &zIn[nLen];` |
|  248458 |   31 | `	for(;;){` |
|  496918 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  447502 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  405466 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  327398 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  215174 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecurisve is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * If the nesting limit is reached,this function abort immediately.` |
|       - |   43 | ` */` |
|     734 |   44 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)` |
|       2 |   45 |  |
|     736 |   46 | `	sxi64 iCount = 0;` |
|     736 |   47 | `	if( !bRecursive ){` |
|     460 |   48 | `		iCount = pMap->nEntry;` |
|     231 |   49 | `	}else{` |
|       - |   50 | `		/* Recursive hashmap walk */` |
|     277 |   51 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|       - |   52 | `		ph7_value *pElem;` |
|     277 |   53 | `		sxu32 n = 0;` |
|     331 |   54 | `		for(;;){` |
|     663 |   55 | `			if( n >= pMap->nEntry ){` |
|     273 |   56 | `				break;` |
|       - |   57 | `			}` |
|       - |   58 | `			/* Point to the element value */` |
|     391 |   59 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|     391 |   60 | `			if( pElem ){` |
|     391 |   61 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|     251 |   62 | `					if( iRecCount > 31 ){` |
|       - |   63 | `						/* Nesting limit reached */` |
|       5 |   64 | `						return iCount;` |
|       - |   65 | `					}` |
|       - |   66 | `					/* Recurse */` |
|     247 |   67 | `					iRecCount++;` |
|     247 |   68 | `					iCount += HashmapCount((ph7_hashmap *)pElem->x.pOther,TRUE,iRecCount);` |
|     247 |   69 | `					iRecCount--;` |
|     123 |   70 | `				}` |
|     193 |   71 | `			}` |
|       - |   72 | `			/* Point to the next entry */` |
|     387 |   73 | `			pEntry = pEntry->pNext;` |
|     387 |   74 | `			++n;` |
|       1 |   75 | `		}` |
|       - |   76 | `		/* Update count */` |
|     273 |   77 | `		iCount += pMap->nEntry;` |
|       - |   78 | `	}` |
|     732 |   79 | `	return iCount;` |
|     369 |   80 |  |
|       - |   81 | `/*` |
|       - |   82 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   83 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   84 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   85 | ` */` |
| 2678776 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   87 |  |
|       - |   88 | `	ph7_hashmap_node *pNode;` |
|       - |   89 | `	/* Allocate a new node */` |
| 2678778 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2678778 |   91 | `	if( pNode == 0 ){` |
|     ! 0 |   92 | `		return 0;` |
|       - |   93 | `	}` |
|       - |   94 | `	/* Zero the stucture */` |
| 2678778 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |   96 | `	/* Fill in the structure */` |
| 2678778 |   97 | `	pNode->pMap  = &(*pMap);` |
| 2678778 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2678778 |   99 | `	pNode->nHash = nHash;` |
| 2678778 |  100 | `	pNode->xKey.iKey = iKey;` |
| 2678778 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 2678778 |  102 | `	return pNode;` |
| 1339390 |  103 |  |
|       - |  104 | `/*` |
|       - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  108 | ` */` |
|   75034 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  110 |  |
|       - |  111 | `	ph7_hashmap_node *pNode;` |
|       - |  112 | `	/* Allocate a new node */` |
|   75036 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   75036 |  114 | `	if( pNode == 0 ){` |
|     ! 0 |  115 | `		return 0;` |
|       - |  116 | `	}` |
|       - |  117 | `	/* Zero the stucture */` |
|   75036 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  119 | `	/* Fill in the structure */` |
|   75036 |  120 | `	pNode->pMap  = &(*pMap);` |
|   75036 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   75036 |  122 | `	pNode->nHash = nHash;` |
|   75036 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   75036 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   75036 |  125 | `	pNode->nValIdx = nValIdx;` |
|   75036 |  126 | `	return pNode;` |
|   37519 |  127 |  |
|       - |  128 | `/*` |
|       - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  130 | ` */` |
| 2753810 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  132 |  |
|       - |  133 | `	/* Link */` |
| 2753812 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2548844 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2548844 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1274421 |  137 | `	}` |
| 2753812 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  139 | `	/* Link to the map list */` |
| 2753812 |  140 | `	if( pMap->pFirst == 0 ){` |
|   33956 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  142 | `		/* Point to the first inserted node */` |
|   33956 |  143 | `		pMap->pCur = pNode;` |
|   16979 |  144 | `	}else{` |
| 2719858 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  146 | `	}` |
| 2753812 |  147 | `	++pMap->nEntry;` |
| 2753812 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Unlink a node from the hashmap.` |
|       - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  152 | ` */` |
|    5180 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  154 |  |
|    5182 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5182 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  157 | `	/* Unlink from the corresponding bucket */` |
|    5182 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|    4758 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2380 |  160 | `	}else{` |
|     425 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  162 | `	}` |
|    5182 |  163 | `	if( pNode->pNextCollide ){` |
|    3977 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    1988 |  165 | `	}` |
|    5182 |  166 | `	if( pMap->pFirst == pNode ){` |
|      58 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|      28 |  168 | `	}` |
|    5182 |  169 | `	if( pMap->pCur == pNode ){` |
|       - |  170 | `		/* Advance the node cursor */` |
|      60 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      29 |  172 | `	}` |
|       - |  173 | `	/* Unlink from the map list */` |
|    5182 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5182 |  175 | `	if( bRestore ){` |
|       - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|      30 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  178 | `		/* Restore to the freelist */` |
|      30 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|      30 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      14 |  181 | `		}` |
|      14 |  182 | `	}` |
|    5182 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5133 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2566 |  185 | `	}` |
|    5182 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5182 |  187 | `	pMap->nEntry--;` |
|    5182 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  189 | `		/* Free the hash-bucket */` |
|      26 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      26 |  191 | `		pMap->apBucket = 0;` |
|      26 |  192 | `		pMap->nSize = 0;` |
|      26 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      12 |  194 | `	}` |
|    5182 |  195 |  |
|       - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  197 | `/*` |
|       - |  198 | ` * Grow the hash-table and rehash all entries.` |
|       - |  199 | ` */` |
| 2753810 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  201 |  |
| 2753812 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   37382 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   37382 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  206 | `		sxu32 nBucket;` |
|       - |  207 | `		sxu32 n;` |
|   37382 |  208 | `		if( nNew < 1 ){` |
|   33956 |  209 | `			nNew = 16;` |
|   16977 |  210 | `		}` |
|       - |  211 | `		/* Allocate a new bucket */` |
|   37382 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   37382 |  213 | `		if( apNew == 0 ){` |
|     ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|       - |  216 | `			}` |
|       - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  218 | `			return SXRET_OK;` |
|       - |  219 | `		}` |
|       - |  220 | `		/* Zero the table */` |
|   37382 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  222 | `		/* Reflect the change */` |
|   37382 |  223 | `		pMap->apBucket = apNew;` |
|   37382 |  224 | `		pMap->nSize = nNew;` |
|   37382 |  225 | `		if( apOld == 0 ){` |
|       - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   33956 |  227 | `			return SXRET_OK;` |
|       - |  228 | `		}` |
|       - |  229 | `		/* Rehash old entries */` |
|    3428 |  230 | `		pEntry = pMap->pFirst;` |
|    3428 |  231 | `		n = 0;` |
| 1886289 |  232 | `		for( ;; ){` |
| 3772580 |  233 | `			if( n >= pMap->nEntry ){` |
|    3428 |  234 | `				break;` |
|       - |  235 | `			}` |
|       - |  236 | `			/* Clear the old collision link */` |
| 3769154 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  238 | `			/* Link to the new bucket */` |
| 3769154 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3769154 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3360100 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3360100 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1680049 |  243 | `			}` |
| 3769154 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  245 | `			/* Point to the next entry */` |
| 3769154 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3769154 |  247 | `			n++;` |
|       2 |  248 | `		}` |
|       - |  249 | `		/* Free the old table */` |
|    3428 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1713 |  251 | `	}` |
| 2719858 |  252 | `	return SXRET_OK;` |
| 1376907 |  253 |  |
|       - |  254 | `/*` |
|       - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  256 | ` * hashmap.` |
|       - |  257 | ` */` |
| 2678776 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  259 |  |
|       - |  260 | `	ph7_hashmap_node *pNode;` |
|       - |  261 | `	sxu32 nIdx;` |
|       - |  262 | `	sxu32 nHash;` |
|       - |  263 | `	sxi32 rc;` |
| 2678778 |  264 | `	if( !isForeign ){` |
|       - |  265 | `		ph7_value *pObj;` |
|       - |  266 | `		/* Reserve a ph7_value for the value */` |
| 2678754 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2678754 |  268 | `		if( pObj == 0 ){` |
|     ! 0 |  269 | `			return SXERR_MEM;` |
|       - |  270 | `		}` |
| 2678754 |  271 | `		if( pValue ){` |
|       - |  272 | `			/* Duplicate the value */` |
| 2678754 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 1339376 |  274 | `		}` |
| 2678754 |  275 | `		nIdx = pObj->nIdx;` |
| 1339378 |  276 | `	}else{` |
|      25 |  277 | `		nIdx = nRefIdx;` |
|       - |  278 | `	}` |
|       - |  279 | `	/* Hash the key */` |
| 2678778 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  281 | `	/* Allocate a new int node */` |
| 2678778 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2678778 |  283 | `	if( pNode == 0 ){` |
|     ! 0 |  284 | `		return SXERR_MEM;` |
|       - |  285 | `	}` |
| 2678778 |  286 | `	if( isForeign ){` |
|       - |  287 | `		/* Mark as a foregin entry */` |
|      25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      12 |  289 | `	}` |
|       - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2678778 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2678778 |  292 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  294 | `		return rc;` |
|       - |  295 | `	}` |
|       - |  296 | `	/* Perform the insertion */` |
| 2678778 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  298 | `	/* Install in the reference table */` |
| 2678778 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  300 | `	/* All done */` |
| 2678778 |  301 | `	return SXRET_OK;` |
| 1339390 |  302 |  |
|       - |  303 | `/*` |
|       - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  305 | ` * hashmap.` |
|       - |  306 | ` */` |
|   75034 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  308 |  |
|       - |  309 | `	ph7_hashmap_node *pNode;` |
|       - |  310 | `	sxu32 nHash;` |
|       - |  311 | `	sxu32 nIdx;` |
|       - |  312 | `	sxi32 rc;` |
|   75036 |  313 | `	if( !isForeign ){` |
|       - |  314 | `		ph7_value *pObj;` |
|       - |  315 | `		/* Reserve a ph7_value for the value */` |
|   56684 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   56684 |  317 | `		if( pObj == 0 ){` |
|     ! 0 |  318 | `			return SXERR_MEM;` |
|       - |  319 | `		}` |
|   56684 |  320 | `		if( pValue ){` |
|       - |  321 | `			/* Duplicate the value */` |
|   56684 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|   28341 |  323 | `		}` |
|   56684 |  324 | `		nIdx = pObj->nIdx;` |
|   28343 |  325 | `	}else{` |
|   18354 |  326 | `		nIdx = nRefIdx;` |
|       - |  327 | `	}` |
|       - |  328 | `	/* Hash the key */` |
|   75036 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  330 | `	/* Allocate a new blob node */` |
|   75036 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   75036 |  332 | `	if( pNode == 0 ){` |
|     ! 0 |  333 | `		return SXERR_MEM;` |
|       - |  334 | `	}` |
|   75036 |  335 | `	if( isForeign ){` |
|       - |  336 | `		/* Mark as a foregin entry */` |
|   18354 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|    9176 |  338 | `	}` |
|       - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   75036 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   75036 |  341 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  343 | `		return rc;` |
|       - |  344 | `	}` |
|       - |  345 | `	/* Perform the insertion */` |
|   75036 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  347 | `	/* Install in the reference table */` |
|   75036 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  349 | `	/* All done */` |
|   75036 |  350 | `	return SXRET_OK;` |
|   37519 |  351 |  |
|       - |  352 | `/*` |
|       - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  356 | ` */` |
|   46466 |  357 | `static sxi32 HashmapLookupIntKey(` |
|       - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  359 | `	sxi64 iKey,                /* lookup key */` |
|       - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  361 | `	)` |
|       2 |  362 |  |
|       - |  363 | `	ph7_hashmap_node *pNode;` |
|       - |  364 | `	sxu32 nHash;` |
|   46468 |  365 | `	if( pMap->nEntry < 1 ){` |
|       - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|     335 |  367 | `		return SXERR_NOTFOUND;` |
|       - |  368 | `	}` |
|       - |  369 | `	/* Hash the key first */` |
|   46134 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  371 | `	/* Point to the appropriate bucket */` |
|   46134 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  373 | `	/* Perform the lookup */` |
|  411423 |  374 | `	for(;;){` |
|  822848 |  375 | `		if( pNode == 0 ){` |
|   45631 |  376 | `			break;` |
|       - |  377 | `		}` |
|  777467 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774198 |  379 | `			&& pNode->nHash == nHash` |
|  385843 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  381 | `				/* Node found */` |
|     504 |  382 | `				if( ppNode ){` |
|     496 |  383 | `					*ppNode = pNode;` |
|     247 |  384 | `				}` |
|     504 |  385 | `				return SXRET_OK;` |
|       - |  386 | `		}` |
|       - |  387 | `		/* Follow the collision link */` |
|  776715 |  388 | `		pNode = pNode->pNextCollide;` |
|       1 |  389 | `	}` |
|       - |  390 | `	/* No such entry */` |
|   45631 |  391 | `	return SXERR_NOTFOUND;` |
|   23235 |  392 |  |
|       - |  393 | `/*` |
|       - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  397 | ` */` |
|  147618 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  400 | `	const void *pKey,           /* Lookup key */` |
|       - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  403 | `	)` |
|       2 |  404 |  |
|       - |  405 | `	ph7_hashmap_node *pNode;` |
|       - |  406 | `	sxu32 nHash;` |
|  147620 |  407 | `	if( pMap->nEntry < 1 ){` |
|       - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|    7482 |  409 | `		return SXERR_NOTFOUND;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* Hash the key first */` |
|  140140 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  413 | `	/* Point to the appropriate bucket */` |
|  140140 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  415 | `	/* Perform the lookup */` |
|  142148 |  416 | `	for(;;){` |
|  284298 |  417 | `		if( pNode == 0 ){` |
|  106040 |  418 | `			break;` |
|       - |  419 | `		}` |
|  195308 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  176758 |  421 | `			&& pNode->nHash == nHash` |
|  104679 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   34102 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  424 | `				/* Node found */` |
|   34102 |  425 | `				if( ppNode ){` |
|   34086 |  426 | `					*ppNode = pNode;` |
|   17042 |  427 | `				}` |
|   34102 |  428 | `				return SXRET_OK;` |
|       - |  429 | `		}` |
|       - |  430 | `		/* Follow the collision link */` |
|  144160 |  431 | `		pNode = pNode->pNextCollide;` |
|       2 |  432 | `	}` |
|       - |  433 | `	/* No such entry */` |
|  106040 |  434 | `	return SXERR_NOTFOUND;` |
|   73811 |  435 |  |
|       - |  436 | `/*` |
|       - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  439 | ` */` |
|  147796 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  441 |  |
|  147798 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  147798 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  147798 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  445 | `		/* Octal not decimal number */` |
|       5 |  446 | `		return FALSE;` |
|       - |  447 | `	}` |
|  147794 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  449 | `		zIn++;` |
|     ! 0 |  450 | `	}` |
|   74229 |  451 | `	for(;;){` |
|  148460 |  452 | `		if( zIn >= zEnd ){` |
|     233 |  453 | `			return TRUE;` |
|       - |  454 | `		}` |
|  148228 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   73782 |  456 | `			break;` |
|       - |  457 | `		}` |
|     667 |  458 | `		zIn++;` |
|       1 |  459 | `	}` |
|       - |  460 | `	/* Key does not look like a decimal number */` |
|  147562 |  461 | `	return FALSE;` |
|   73900 |  462 |  |
|       - |  463 | `/*` |
|       - |  464 | ` * Check if a given key exists in the given hashmap.` |
|       - |  465 | ` * Write a pointer to the target node on success.` |
|       - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  467 | ` */` |
|   73036 |  468 | `static sxi32 HashmapLookup(` |
|       - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  472 | `	)` |
|       2 |  473 |  |
|   73038 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  475 | `	sxi32 rc;` |
|   73038 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   72600 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  478 | `			/* Force a string cast */` |
|     ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  480 | `		}` |
|   72600 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  482 | `			/* Perform a blob lookup */` |
|   72584 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   72584 |  484 | `			goto result;` |
|       - |  485 | `		}` |
|       8 |  486 | `	}` |
|       - |  487 | `	/* Perform an int lookup */` |
|     456 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  489 | `		/* Force an integer cast */` |
|      27 |  490 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  491 | `	}` |
|       - |  492 | `	/* Perform an int lookup */` |
|     456 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   36518 |  494 | `result:` |
|   73038 |  495 | `	if( rc == SXRET_OK ){` |
|       - |  496 | `		/* Node found */` |
|   34488 |  497 | `		if( ppNode ){` |
|   34464 |  498 | `			*ppNode = pNode;` |
|   17231 |  499 | `		}` |
|   34488 |  500 | `		return SXRET_OK;` |
|       - |  501 | `	}` |
|       - |  502 | `	/* No such entry */` |
|   38552 |  503 | `	return SXERR_NOTFOUND;` |
|   36520 |  504 |  |
|       - |  505 | `/*` |
|       - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  507 | ` * hashmap.` |
|       - |  508 | ` * If a node with the given key already exists in the database` |
|       - |  509 | ` * then this function overwrite the old value.` |
|       - |  510 | ` */` |
| 2735338 |  511 | `static sxi32 HashmapInsert(` |
|       - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  514 | `	ph7_value *pVal    /* Node value */` |
|       - |  515 | `	)` |
|       2 |  516 |  |
| 2735340 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 2735340 |  518 | `	sxi32 rc = SXRET_OK;` |
| 2735340 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   56880 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  521 | `			/* Force a string cast */` |
|       3 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  523 | `		}` |
|   56880 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  526 | `				/* Automatic index assign */` |
|      34 |  527 | `				pKey = 0;` |
|      16 |  528 | `			}` |
|     256 |  529 | `			goto IntKey;` |
|       - |  530 | `		}` |
|   84938 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   28312 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  533 | `				/* Overwrite the old value */` |
|       - |  534 | `				ph7_value *pElem;` |
|      23 |  535 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      23 |  536 | `				if( pElem ){` |
|      23 |  537 | `					if( pVal ){` |
|      23 |  538 | `						PH7_MemObjStore(pVal,pElem);` |
|      12 |  539 | `					}else{` |
|       - |  540 | `						/* Nullify the entry */` |
|     ! 0 |  541 | `						PH7_MemObjToNull(pElem);` |
|       - |  542 | `					}` |
|      11 |  543 | `				}` |
|      23 |  544 | `				return SXRET_OK;` |
|       - |  545 | `		}` |
|   56604 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  547 | `			/* Forbidden */` |
|       3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  549 | `			return SXRET_OK;` |
|       - |  550 | `		}` |
|       - |  551 | `		/* Perform a blob-key insertion */` |
|   56602 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   56602 |  553 | `		return rc;` |
|       - |  554 | `	}` |
| 1339230 |  555 | `IntKey:` |
| 2678716 |  556 | `	if( pKey ){` |
|   23121 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  558 | `			/* Force an integer cast */` |
|     251 |  559 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  560 | `		}` |
|   23121 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  562 | `			/* Overwrite the old value */` |
|       - |  563 | `			ph7_value *pElem;` |
|      37 |  564 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      37 |  565 | `			if( pElem ){` |
|      37 |  566 | `				if( pVal ){` |
|      37 |  567 | `					PH7_MemObjStore(pVal,pElem);` |
|      19 |  568 | `				}else{` |
|       - |  569 | `					/* Nullify the entry */` |
|     ! 0 |  570 | `					PH7_MemObjToNull(pElem);` |
|       - |  571 | `				}` |
|      18 |  572 | `			}` |
|      37 |  573 | `			return SXRET_OK;` |
|       - |  574 | `		}` |
|   23085 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  576 | `			/* Forbidden */` |
|       3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  578 | `			return SXRET_OK;` |
|       - |  579 | `		}` |
|       - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|   23083 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23083 |  582 | `		if( rc == SXRET_OK ){` |
|   23083 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  584 | `				/* Increment the automatic index */` |
|   22855 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  586 | `				/* Make sure the automatic index is not reserved */` |
|   22855 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  588 | `					pMap->iNextIdx++;` |
|     ! 0 |  589 | `				}` |
|   11427 |  590 | `			}` |
|   11541 |  591 | `		}` |
|   11542 |  592 | `	}else{` |
| 2655596 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  594 | `			/* Forbidden */` |
|       3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  596 | `			return SXRET_OK;` |
|       - |  597 | `		}` |
|       - |  598 | `		/* Assign an automatic index */` |
| 2655594 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2655594 |  600 | `		if( rc == SXRET_OK ){` |
| 2655594 |  601 | `			++pMap->iNextIdx;` |
| 1327796 |  602 | `		}` |
|       - |  603 | `	}` |
|       - |  604 | `	/* Insertion result */` |
| 2678676 |  605 | `	return rc;` |
| 1367671 |  606 |  |
|       - |  607 | `/*` |
|       - |  608 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - |  609 | ` * hashmap.` |
|       - |  610 | ` * This is insertion by reference so be careful to mark the node` |
|       - |  611 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - |  612 | ` * The insertion by reference is triggered when the following` |
|       - |  613 | ` * expression is encountered.` |
|       - |  614 | ` * $var = 10;` |
|       - |  615 | ` *  $a = array(&var);` |
|       - |  616 | ` * OR` |
|       - |  617 | ` *  $a[] =& $var;` |
|       - |  618 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - |  619 | ` * over it's contents.` |
|       - |  620 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - |  621 | ` * removed when the foreign ph7_value is unset.` |
|       - |  622 | ` * Example:` |
|       - |  623 | ` *  $var = 10;` |
|       - |  624 | ` *  $a[] =& $var;` |
|       - |  625 | ` *  echo count($a).PHP_EOL; //1` |
|       - |  626 | ` *  //Unset the foreign ph7_value now` |
|       - |  627 | ` *  unset($var);` |
|       - |  628 | ` *  echo count($a); //0` |
|       - |  629 | ` * Note that this is a PH7 eXtension.` |
|       - |  630 | ` * Refer to the official documentation for more information.` |
|       - |  631 | ` * If a node with the given key already exists in the database` |
|       - |  632 | ` * then this function overwrite the old value.` |
|       - |  633 | ` */` |
|   18382 |  634 | `static sxi32 HashmapInsertByRef(` |
|       - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  638 | `	)` |
|       2 |  639 |  |
|   18384 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|   18384 |  641 | `	sxi32 rc = SXRET_OK;` |
|   18384 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   18360 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  644 | `			/* Force a string cast */` |
|     ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  646 | `		}` |
|   18360 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  649 | `				/* Automatic index assign */` |
|     ! 0 |  650 | `				pKey = 0;` |
|     ! 0 |  651 | `			}` |
|     ! 0 |  652 | `			goto IntKey;` |
|       - |  653 | `		}` |
|   27539 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    9179 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  656 | `				/* Overwrite */` |
|       7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|       - |  659 | `				/* Install in the reference table */` |
|       7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  661 | `				return SXRET_OK;` |
|       - |  662 | `		}` |
|       - |  663 | `		/* Perform a blob-key insertion */` |
|   18354 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   18354 |  665 | `		return rc;` |
|       - |  666 | `	}` |
|      12 |  667 | `IntKey:` |
|      25 |  668 | `	if( pKey ){` |
|       3 |  669 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  670 | `			/* Force an integer cast */` |
|     ! 0 |  671 | `			PH7_MemObjToInteger(pKey);` |
|     ! 0 |  672 | `		}` |
|       3 |  673 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  674 | `			/* Overwrite */` |
|     ! 0 |  675 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|     ! 0 |  676 | `			pNode->nValIdx = nRefIdx;` |
|       - |  677 | `			/* Install in the reference table */` |
|     ! 0 |  678 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|     ! 0 |  679 | `			return SXRET_OK;` |
|       - |  680 | `		}` |
|       - |  681 | `		/* Perform a 64-bit-int-key insertion */` |
|       3 |  682 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|       3 |  683 | `		if( rc == SXRET_OK ){` |
|       3 |  684 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  685 | `				/* Increment the automatic index */` |
|       3 |  686 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  687 | `				/* Make sure the automatic index is not reserved */` |
|       3 |  688 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  689 | `					pMap->iNextIdx++;` |
|     ! 0 |  690 | `				}` |
|       1 |  691 | `			}` |
|       1 |  692 | `		}` |
|       2 |  693 | `	}else{` |
|       - |  694 | `		/* Assign an automatic index */` |
|      23 |  695 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|      23 |  696 | `		if( rc == SXRET_OK ){` |
|      23 |  697 | `			++pMap->iNextIdx;` |
|      11 |  698 | `		}` |
|       - |  699 | `	}` |
|       - |  700 | `	/* Insertion result */` |
|      25 |  701 | `	return rc;` |
|    9193 |  702 |  |
|       - |  703 | `/*` |
|       - |  704 | ` * Extract node value.` |
|       - |  705 | ` */` |
|  779986 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  707 |  |
|       - |  708 | `	/* Point to the desired object */` |
|       - |  709 | `	ph7_value *pObj;` |
|  779988 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  779988 |  711 | `	return pObj;` |
|       2 |  712 |  |
|       - |  713 | `/*` |
|       - |  714 | ` * Insert a node in the given hashmap.` |
|       - |  715 | ` * If a node with the given key already exists in the database` |
|       - |  716 | ` * then this function overwrite the old value.` |
|       - |  717 | ` */` |
|     220 |  718 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       1 |  719 |  |
|       - |  720 | `	ph7_value *pObj;` |
|       - |  721 | `	sxi32 rc;` |
|       - |  722 | `	/* Extract the node value */` |
|     221 |  723 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     221 |  724 | `	if( pObj == 0 ){` |
|     ! 0 |  725 | `		return SXERR_EMPTY;` |
|       - |  726 | `	}` |
|       - |  727 | `	/* Preserve key */` |
|     221 |  728 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  729 | `		/* Int64 key */` |
|     109 |  730 | `		if( !bPreserve ){` |
|       - |  731 | `			/* Assign an automatic index */` |
|      55 |  732 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      28 |  733 | `		}else{` |
|      55 |  734 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  735 | `		}` |
|      55 |  736 | `	}else{` |
|       - |  737 | `		/* Blob key */` |
|     113 |  738 | `		if( !bPreserve ){` |
|       - |  739 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  740 | `			 * original string key entirely */` |
|      33 |  741 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      17 |  742 | `		}else{` |
|     121 |  743 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      40 |  744 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  745 | `		}` |
|       - |  746 | `	}` |
|     221 |  747 | `	return rc;` |
|     111 |  748 |  |
|       - |  749 | `/*` |
|       - |  750 | ` * Compare two node values.` |
|       - |  751 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  752 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  753 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  754 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  755 | ` * documenation.` |
|       - |  756 | ` */` |
|   34459 |  757 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  758 |  |
|       - |  759 | `	ph7_value sObj1,sObj2;` |
|       - |  760 | `	sxi32 rc;` |
|   34461 |  761 | `	if( pLeft == pRight ){` |
|       - |  762 | `		/*` |
|       - |  763 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  764 | `		 * below for more information on this sceanario.` |
|       - |  765 | `		 */` |
|     ! 0 |  766 | `		return 0;` |
|       - |  767 | `	}` |
|       - |  768 | `	/* Do the comparison */` |
|   34461 |  769 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   34461 |  770 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   34461 |  771 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   34461 |  772 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   34461 |  773 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   34461 |  774 | `	PH7_MemObjRelease(&sObj1);` |
|   34461 |  775 | `	PH7_MemObjRelease(&sObj2);` |
|   34461 |  776 | `	return rc;` |
|   17267 |  777 |  |
|       - |  778 | `/*` |
|       - |  779 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  780 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  781 | ` */` |
|    7506 |  782 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  783 |  |
|    7508 |  784 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  785 | `	sxu32 nBucket;` |
|       - |  786 | `	/* Remove old collision links */` |
|    7508 |  787 | `	if( pEntry->pPrevCollide ){` |
|    6014 |  788 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    3005 |  789 | `	}else{` |
|    1496 |  790 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  791 | `	}` |
|    7508 |  792 | `	if( pEntry->pNextCollide ){` |
|     633 |  793 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     313 |  794 | `	}` |
|    7508 |  795 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  796 | `	/* Compute the new hash */` |
|    7508 |  797 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    7508 |  798 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    7508 |  799 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  800 | `	/* Link to the new bucket */` |
|    7508 |  801 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7508 |  802 | `	if( pMap->apBucket[nBucket] ){` |
|    6178 |  803 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3090 |  804 | `	}` |
|    7508 |  805 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7508 |  806 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  807 | `	/* Increment the automatic index */` |
|    7508 |  808 | `	pMap->iNextIdx++;` |
|    7508 |  809 |  |
|       - |  810 | `/*` |
|       - |  811 | ` * Perform a linear search on a given hashmap.` |
|       - |  812 | ` * Write a pointer to the target node on success.` |
|       - |  813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  814 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  815 | ` * for more information.` |
|       - |  816 | ` */` |
|   19032 |  817 | `static int HashmapFindValue(` |
|       - |  818 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  819 | `	ph7_value *pNeedle,  /* Lookup key */` |
|       - |  820 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|       - |  821 | `	int bStrict      /* TRUE for strict comparison */` |
|       - |  822 | `	)` |
|       2 |  823 |  |
|       - |  824 | `	ph7_hashmap_node *pEntry;` |
|       - |  825 | `	ph7_value sVal,*pVal;` |
|       - |  826 | `	ph7_value sNeedle;` |
|       - |  827 | `	sxi32 rc;` |
|       - |  828 | `	sxu32 n;` |
|       - |  829 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|   19034 |  830 | `	pEntry = pMap->pFirst;` |
|   19034 |  831 | `	n = pMap->nEntry;` |
|   19034 |  832 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   19034 |  833 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   45658 |  834 | `	for(;;){` |
|   91320 |  835 | `		if( n < 1 ){` |
|      25 |  836 | `			break;` |
|       - |  837 | `		}` |
|       - |  838 | `		/* Extract node value */` |
|   91296 |  839 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|   91296 |  840 | `		if( pVal ){` |
|   91296 |  841 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|     ! 0 |  842 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  843 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  844 | `				if( iF1 == iF2 ){` |
|       - |  845 | `					/* NULL values are equals */` |
|     ! 0 |  846 | `					if( ppNode ){` |
|     ! 0 |  847 | `						*ppNode = pEntry;` |
|     ! 0 |  848 | `					}` |
|     ! 0 |  849 | `					return SXRET_OK;` |
|       - |  850 | `				}` |
|     ! 0 |  851 | `			}else{` |
|       - |  852 | `				/* Duplicate value */` |
|   91296 |  853 | `				PH7_MemObjLoad(pVal,&sVal);` |
|   91296 |  854 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|   91296 |  855 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|   91296 |  856 | `				PH7_MemObjRelease(&sVal);` |
|   91296 |  857 | `				PH7_MemObjRelease(&sNeedle);` |
|   91296 |  858 | `				if( rc == 0 ){` |
|   19010 |  859 | `					if( ppNode ){` |
|       3 |  860 | `						*ppNode = pEntry;` |
|       1 |  861 | `					}` |
|       - |  862 | `					/* Match found*/` |
|   19010 |  863 | `					return SXRET_OK;` |
|       - |  864 | `				}` |
|       - |  865 | `			}` |
|   36142 |  866 | `		}` |
|       - |  867 | `		/* Point to the next entry */` |
|   72288 |  868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   72288 |  869 | `		n--;` |
|       2 |  870 | `	}` |
|       - |  871 | `	/* No such entry */` |
|      25 |  872 | `	return SXERR_NOTFOUND;` |
|    9518 |  873 |  |
|       - |  874 | `/*` |
|       - |  875 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  876 | ` * for values comparison.` |
|       - |  877 | ` * Write a pointer to the target node on success.` |
|       - |  878 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  879 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  880 | ` * for more information.` |
|       - |  881 | ` */` |
|      12 |  882 | `static int HashmapFindValueByCallback(` |
|       - |  883 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|       - |  884 | `	ph7_value *pNeedle,    /* Lookup key */` |
|       - |  885 | `	ph7_value *pCallback,  /* User defined callback */` |
|       - |  886 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|       - |  887 | `	)` |
|       1 |  888 |  |
|       - |  889 | `	ph7_hashmap_node *pEntry;` |
|       - |  890 | `	ph7_value sResult,*pVal;` |
|       - |  891 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|       - |  892 | `	sxi32 rc;` |
|       - |  893 | `	sxu32 n;` |
|       - |  894 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|      13 |  895 | `	pEntry = pMap->pFirst;` |
|      13 |  896 | `	n = pMap->nEntry;` |
|       - |  897 | `	/* Store callback result here */` |
|      13 |  898 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  899 | `	/* First argument to the callback */` |
|      13 |  900 | `	apArg[0] = pNeedle;` |
|      15 |  901 | `	for(;;){` |
|      31 |  902 | `		if( n < 1 ){` |
|       7 |  903 | `			break;` |
|       - |  904 | `		}` |
|       - |  905 | `		/* Extract node value */` |
|      25 |  906 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      25 |  907 | `		if( pVal ){` |
|       - |  908 | `			/* Invoke the user callback */` |
|      25 |  909 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      25 |  910 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      25 |  911 | `			if( rc == SXRET_OK ){` |
|       - |  912 | `				/* Extract callback result */` |
|      25 |  913 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  914 | `					/* Perform an int cast */` |
|     ! 0 |  915 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  916 | `				}` |
|      25 |  917 | `				rc = (sxi32)sResult.x.iVal;` |
|      25 |  918 | `				PH7_MemObjRelease(&sResult);` |
|      25 |  919 | `				if( rc == 0 ){` |
|       - |  920 | `					/* Match found*/` |
|       7 |  921 | `					if( ppNode ){` |
|     ! 0 |  922 | `						*ppNode = pEntry;` |
|     ! 0 |  923 | `					}` |
|       7 |  924 | `					return SXRET_OK;` |
|       - |  925 | `				}` |
|       9 |  926 | `			}` |
|       9 |  927 | `		}` |
|       - |  928 | `		/* Point to the next entry */` |
|      19 |  929 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 |  930 | `		n--;` |
|       1 |  931 | `	}` |
|       - |  932 | `	/* No such entry */` |
|       7 |  933 | `	return SXERR_NOTFOUND;` |
|       7 |  934 |  |
|       - |  935 | `/*` |
|       - |  936 | ` * Compare two hashmaps.` |
|       - |  937 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|       - |  938 | ` * Note on array comparison operators.` |
|       - |  939 | ` *  According to the PHP language reference manual.` |
|       - |  940 | ` *  Array Operators Example 	Name 	Result` |
|       - |  941 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|       - |  942 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|       - |  943 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|       - |  944 | ` *                          order and of the same types.` |
|       - |  945 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - |  946 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - |  947 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|       - |  948 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - |  949 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - |  950 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - |  951 | ` * <?php` |
|       - |  952 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - |  953 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - |  954 | ` * $c = $a + $b; // Union of $a and $b` |
|       - |  955 | ` * echo "Union of \$a and \$b: \n";` |
|       - |  956 | ` * var_dump($c);` |
|       - |  957 | ` * $c = $b + $a; // Union of $b and $a` |
|       - |  958 | ` * echo "Union of \$b and \$a: \n";` |
|       - |  959 | ` * var_dump($c);` |
|       - |  960 | ` * ?>` |
|       - |  961 | ` * When executed, this script will print the following:` |
|       - |  962 | ` * Union of $a and $b:` |
|       - |  963 | ` * array(3) {` |
|       - |  964 | ` *  ["a"]=>` |
|       - |  965 | ` *  string(5) "apple"` |
|       - |  966 | ` *  ["b"]=>` |
|       - |  967 | ` * string(6) "banana"` |
|       - |  968 | ` *  ["c"]=>` |
|       - |  969 | ` * string(6) "cherry"` |
|       - |  970 | ` * }` |
|       - |  971 | ` * Union of $b and $a:` |
|       - |  972 | ` * array(3) {` |
|       - |  973 | ` * ["a"]=>` |
|       - |  974 | ` * string(4) "pear"` |
|       - |  975 | ` * ["b"]=>` |
|       - |  976 | ` * string(10) "strawberry"` |
|       - |  977 | ` * ["c"]=>` |
|       - |  978 | ` * string(6) "cherry"` |
|       - |  979 | ` * }` |
|       - |  980 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|       - |  981 | ` */` |
|       8 |  982 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - |  983 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - |  984 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - |  985 | `	int bStrict          /* TRUE for strict comparison */` |
|       - |  986 | `	)` |
|       1 |  987 |  |
|       - |  988 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - |  989 | `	sxi32 rc;` |
|       - |  990 | `	sxu32 n;` |
|       9 |  991 | `	if( pLeft == pRight ){` |
|       - |  992 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - |  993 | `		 * Unlike the zend engine.` |
|       - |  994 | `		 */` |
|     ! 0 |  995 | `		return 0;` |
|       - |  996 | `	}` |
|       9 |  997 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - |  998 | `		/* Must have the same number of entries */` |
|     ! 0 |  999 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1000 | `	}` |
|       - | 1001 | `	/* Point to the first inserted entry of the left hashmap */` |
|       9 | 1002 | `	pLe = pLeft->pFirst;` |
|       9 | 1003 | `	pRe = 0; /* cc warning */` |
|       - | 1004 | `	/* Perform the comparison */` |
|       9 | 1005 | `	n = pLeft->nEntry;` |
|       8 | 1006 | `	for(;;){` |
|      17 | 1007 | `		if( n < 1 ){` |
|       7 | 1008 | `			break;` |
|       - | 1009 | `		}` |
|      11 | 1010 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1011 | `			/* Int key */` |
|       7 | 1012 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|       4 | 1013 | `		}else{` |
|       5 | 1014 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1015 | `			/* Blob key */` |
|       5 | 1016 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1017 | `		}` |
|      11 | 1018 | `		if( rc != SXRET_OK ){` |
|       - | 1019 | `			/* No such entry in the right side */` |
|     ! 0 | 1020 | `			return 1;` |
|       - | 1021 | `		}` |
|      11 | 1022 | `		rc = 0;` |
|      11 | 1023 | `		if( bStrict ){` |
|       - | 1024 | `			/* Make sure,the keys are of the same type */` |
|       3 | 1025 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1026 | `				rc = 1;` |
|     ! 0 | 1027 | `			}` |
|       1 | 1028 | `		}` |
|      11 | 1029 | `		if( !rc ){` |
|       - | 1030 | `			/* Compare nodes */` |
|      11 | 1031 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|       5 | 1032 | `		}` |
|      11 | 1033 | `		if( rc != 0 ){` |
|       - | 1034 | `			/* Nodes key/value differ */` |
|       3 | 1035 | `			return rc;` |
|       - | 1036 | `		}` |
|       - | 1037 | `		/* Point to the next entry */` |
|       9 | 1038 | `		pLe = pLe->pPrev; /* Reverse link */` |
|       9 | 1039 | `		n--;` |
|       1 | 1040 | `	}` |
|       7 | 1041 | `	return 0; /* Hashmaps are equals */` |
|       5 | 1042 |  |
|       - | 1043 | `/*` |
|       - | 1044 | ` * Duplicate a hashmap node.` |
|       - | 1045 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1046 | ` */` |
|  357664 | 1047 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1048 | `	ph7_hashmap *pDest,` |
|       - | 1049 | `	ph7_hashmap_node *pEntry,` |
|       - | 1050 | `	ph7_value *pVal,` |
|       - | 1051 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1052 | `	)` |
|       2 | 1053 |  |
|  357666 | 1054 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1055 | `	ph7_value sKey;` |
|       - | 1056 | `	sxi32 rc;` |
|       - | 1057 |  |
|  357666 | 1058 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1059 | `		/* Blob key insertion */` |
|      19 | 1060 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      19 | 1061 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      19 | 1062 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      19 | 1063 | `		PH7_MemObjRelease(&sKey);` |
|      10 | 1064 | `	}else{` |
|       - | 1065 | `		/* Int key */` |
|  357648 | 1066 | `		if( iAction == 0 ){ /* Merge */` |
|  357620 | 1067 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  178839 | 1068 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|       5 | 1069 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       5 | 1070 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|       5 | 1071 | `			PH7_MemObjRelease(&sKey);` |
|       3 | 1072 | `		}else{ /* Dup */` |
|      26 | 1073 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1074 | `		}` |
|       - | 1075 | `	}` |
|  357666 | 1076 | `	return rc;` |
|       2 | 1077 |  |
|       - | 1078 | `/*` |
|       - | 1079 | ` * Merge two hashmaps.` |
|       - | 1080 | ` * Note on the merge process` |
|       - | 1081 | ` * According to the PHP language reference manual.` |
|       - | 1082 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|       - | 1083 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|       - | 1084 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|       - | 1085 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|       - | 1086 | ` *  the later value will not overwrite the original value, but will be appended.` |
|       - | 1087 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|       - | 1088 | ` *  keys starting from zero in the result array.` |
|       - | 1089 | ` */` |
|    1610 | 1090 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1091 |  |
|       - | 1092 | `	ph7_hashmap_node *pEntry;` |
|       - | 1093 | `	ph7_value *pVal;` |
|       - | 1094 | `	sxi32 rc;` |
|       - | 1095 | `	sxu32 n;` |
|    1612 | 1096 | `	if( pSrc == pDest ){` |
|       - | 1097 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1098 | `		 * Unlike the zend engine.` |
|       - | 1099 | `		 */` |
|     ! 0 | 1100 | `		return SXRET_OK;` |
|       - | 1101 | `	}` |
|       - | 1102 | `	/* Point to the first inserted entry in the source */` |
|    1612 | 1103 | `	pEntry = pSrc->pFirst;` |
|       - | 1104 | `	/* Perform the merge */` |
|  359236 | 1105 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1106 | `		/* Extract the node value */` |
|  357626 | 1107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  357626 | 1108 | `		if( pVal ){` |
|       - | 1109 | `			/* Make a local copy of the value.` |
|       - | 1110 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1111 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1112 | `			 * to the old pool.` |
|       - | 1113 | `			 */` |
|  357626 | 1114 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  178814 | 1115 | `		}else{` |
|     ! 0 | 1116 | `			rc = SXRET_OK;` |
|       - | 1117 | `		}` |
|  357626 | 1118 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1119 | `			return rc;` |
|       - | 1120 | `		}` |
|       - | 1121 | `		/* Point to the next entry */` |
|  357626 | 1122 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  178814 | 1123 | `	}` |
|    1612 | 1124 | `	return SXRET_OK;` |
|     807 | 1125 |  |
|       - | 1126 | `/*` |
|       - | 1127 | ` * Overwrite entries with the same key.` |
|       - | 1128 | ` * Refer to the [array_replace()] implementation for more information.` |
|       - | 1129 | ` *  According to the PHP language reference manual.` |
|       - | 1130 | ` *  array_replace() replaces the values of the first array with the same values` |
|       - | 1131 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|       - | 1132 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|       - | 1133 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|       - | 1134 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|       - | 1135 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|       - | 1136 | ` *  overwriting the previous values.` |
|       - | 1137 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|       - | 1138 | ` *  by whatever type is in the second array.` |
|       - | 1139 | ` */` |
|       4 | 1140 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1141 |  |
|       - | 1142 | `	ph7_hashmap_node *pEntry;` |
|       - | 1143 | `	ph7_value *pVal;` |
|       - | 1144 | `	sxi32 rc;` |
|       - | 1145 | `	sxu32 n;` |
|       5 | 1146 | `	if( pSrc == pDest ){` |
|       - | 1147 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1148 | `		 * Unlike the zend engine.` |
|       - | 1149 | `		 */` |
|     ! 0 | 1150 | `		return SXRET_OK;` |
|       - | 1151 | `	}` |
|       - | 1152 | `	/* Point to the first inserted entry in the source */` |
|       5 | 1153 | `	pEntry = pSrc->pFirst;` |
|       - | 1154 | `	/* Perform the merge */` |
|      13 | 1155 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1156 | `		/* Extract the node value */` |
|       9 | 1157 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 1158 | `		if( pVal ){` |
|       9 | 1159 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|       5 | 1160 | `		}else{` |
|     ! 0 | 1161 | `			rc = SXRET_OK;` |
|       - | 1162 | `		}` |
|       9 | 1163 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1164 | `			return rc;` |
|       - | 1165 | `		}` |
|       - | 1166 | `		/* Point to the next entry */` |
|       9 | 1167 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       5 | 1168 | `	}` |
|       5 | 1169 | `	return SXRET_OK;` |
|       3 | 1170 |  |
|       - | 1171 | `/*` |
|       - | 1172 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|       - | 1173 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|       - | 1174 | ` */` |
|      16 | 1175 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1176 |  |
|       - | 1177 | `	ph7_hashmap_node *pEntry;` |
|       - | 1178 | `	ph7_value *pVal;` |
|       - | 1179 | `	sxi32 rc;` |
|       - | 1180 | `	sxu32 n;` |
|      18 | 1181 | `	if( pSrc == pDest ){` |
|       - | 1182 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1183 | `		 * Unlike the zend engine.` |
|       - | 1184 | `		 */` |
|     ! 0 | 1185 | `		return SXRET_OK;` |
|       - | 1186 | `	}` |
|       - | 1187 | `	/* Point to the first inserted entry in the source */` |
|      18 | 1188 | `	pEntry = pSrc->pFirst;` |
|       - | 1189 | `	/* Perform the duplication */` |
|      50 | 1190 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1191 | `		/* Extract the node value */` |
|      34 | 1192 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      34 | 1193 | `		if( pVal ){` |
|      34 | 1194 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      18 | 1195 | `		}else{` |
|     ! 0 | 1196 | `			rc = SXRET_OK;` |
|       - | 1197 | `		}` |
|      34 | 1198 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1199 | `			return rc;` |
|       - | 1200 | `		}` |
|       - | 1201 | `		/* Point to the next entry */` |
|      34 | 1202 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      18 | 1203 | `	}` |
|      18 | 1204 | `	return SXRET_OK;` |
|      10 | 1205 |  |
|       - | 1206 | `/*` |
|       - | 1207 | ` * Perform the union of two hashmaps.` |
|       - | 1208 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1209 | ` * with a variable holding an array as follows:` |
|       - | 1210 | ` * <?php` |
|       - | 1211 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1212 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1213 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1214 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1215 | ` * var_dump($c);` |
|       - | 1216 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1217 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1218 | ` * var_dump($c);` |
|       - | 1219 | ` * ?>` |
|       - | 1220 | ` * When executed, this script will print the following:` |
|       - | 1221 | ` * Union of $a and $b:` |
|       - | 1222 | ` * array(3) {` |
|       - | 1223 | ` *  ["a"]=>` |
|       - | 1224 | ` *  string(5) "apple"` |
|       - | 1225 | ` *  ["b"]=>` |
|       - | 1226 | ` * string(6) "banana"` |
|       - | 1227 | ` *  ["c"]=>` |
|       - | 1228 | ` * string(6) "cherry"` |
|       - | 1229 | ` * }` |
|       - | 1230 | ` * Union of $b and $a:` |
|       - | 1231 | ` * array(3) {` |
|       - | 1232 | ` * ["a"]=>` |
|       - | 1233 | ` * string(4) "pear"` |
|       - | 1234 | ` * ["b"]=>` |
|       - | 1235 | ` * string(10) "strawberry"` |
|       - | 1236 | ` * ["c"]=>` |
|       - | 1237 | ` * string(6) "cherry"` |
|       - | 1238 | ` * }` |
|       - | 1239 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1240 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1241 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1242 | ` */` |
|       4 | 1243 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       2 | 1244 |  |
|       - | 1245 | `	ph7_hashmap_node *pEntry;` |
|       6 | 1246 | `	sxi32 rc = SXRET_OK;` |
|       - | 1247 | `	ph7_value *pObj;` |
|       - | 1248 | `	sxu32 n;` |
|       6 | 1249 | `	if( pLeft == pRight ){` |
|       - | 1250 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1251 | `		 * Unlike the zend engine.` |
|       - | 1252 | `		 */` |
|     ! 0 | 1253 | `		return SXRET_OK;` |
|       - | 1254 | `	}` |
|       - | 1255 | `	/* Perform the union */` |
|       6 | 1256 | `	pEntry = pRight->pFirst;` |
|      16 | 1257 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1258 | `		/* Make sure the given key does not exists in the left array */` |
|      12 | 1259 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1260 | `			/* BLOB key */` |
|       7 | 1261 | `			if( SXRET_OK !=` |
|       6 | 1262 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|       3 | 1263 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|       3 | 1264 | `					if( pObj ){` |
|       3 | 1265 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1266 | `						/* Perform the insertion */` |
|       3 | 1267 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1268 | `							&sSafeVal,0,FALSE);` |
|       3 | 1269 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1270 | `							return rc;` |
|       - | 1271 | `						}` |
|       1 | 1272 | `					}` |
|       1 | 1273 | `			}` |
|       4 | 1274 | `		}else{` |
|       - | 1275 | `			/* INT key */` |
|       5 | 1276 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|     ! 0 | 1277 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 1278 | `				if( pObj ){` |
|     ! 0 | 1279 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1280 | `					/* Perform the insertion */` |
|     ! 0 | 1281 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|     ! 0 | 1282 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1283 | `						return rc;` |
|       - | 1284 | `					}` |
|     ! 0 | 1285 | `				}` |
|     ! 0 | 1286 | `			}` |
|       - | 1287 | `		}` |
|       - | 1288 | `		/* Point to the next entry */` |
|      12 | 1289 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 1290 | `	}` |
|       6 | 1291 | `	return SXRET_OK;` |
|       4 | 1292 |  |
|       - | 1293 | `/*` |
|       - | 1294 | ` * Allocate a new hashmap.` |
|       - | 1295 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1296 | ` */` |
|   49738 | 1297 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1298 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1299 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1300 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1301 | `	)` |
|       2 | 1302 |  |
|       - | 1303 | `	ph7_hashmap *pMap;` |
|       - | 1304 | `	/* Allocate a new instance */` |
|   49740 | 1305 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   49740 | 1306 | `	if( pMap == 0 ){` |
|     ! 0 | 1307 | `		return 0;` |
|       - | 1308 | `	}` |
|       - | 1309 | `	/* Zero the structure */` |
|   49740 | 1310 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1311 | `	/* Fill in the structure */` |
|   49740 | 1312 | `	pMap->pVm = &(*pVm);` |
|   49740 | 1313 | `	pMap->iRef = 1;` |
|       - | 1314 | `	/* Default hash functions */` |
|   49740 | 1315 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   49740 | 1316 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   49740 | 1317 | `	return pMap;` |
|   24871 | 1318 |  |
|       - | 1319 | `/*` |
|       - | 1320 | ` * Install superglobals in the given virtual machine.` |
|       - | 1321 | ` * Note on superglobals.` |
|       - | 1322 | ` *  According to the PHP language reference manual.` |
|       - | 1323 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1324 | `*   Description` |
|       - | 1325 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1326 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1327 | `*   global $variable; to access them within functions or methods.` |
|       - | 1328 | `*   These superglobal variables are:` |
|       - | 1329 | `*    $GLOBALS` |
|       - | 1330 | `*    $_SERVER` |
|       - | 1331 | `*    $_GET` |
|       - | 1332 | `*    $_POST` |
|       - | 1333 | `*    $_FILES` |
|       - | 1334 | `*    $_COOKIE` |
|       - | 1335 | `*    $_SESSION` |
|       - | 1336 | `*    $_REQUEST` |
|       - | 1337 | `*    $_ENV` |
|       - | 1338 | `*/` |
|    1336 | 1339 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       2 | 1340 |  |
|       - | 1341 | `	static const char * azSuper[] = {` |
|       - | 1342 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1343 | `		"_GET",      /* $_GET */` |
|       - | 1344 | `		"_POST",     /* $_POST */` |
|       - | 1345 | `		"_FILES",    /* $_FILES */` |
|       - | 1346 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1347 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1348 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1349 | `		"_ENV",      /* $_ENV */` |
|       - | 1350 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1351 | `		"argv"       /* $argv */` |
|       - | 1352 | `	};` |
|       - | 1353 | `	ph7_hashmap *pMap;` |
|       - | 1354 | `	ph7_value *pObj;` |
|       - | 1355 | `	SyString *pFile;` |
|       - | 1356 | `	sxi32 rc;` |
|       - | 1357 | `	sxu32 n;` |
|       - | 1358 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    1338 | 1359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1338 | 1360 | `	if( pMap == 0 ){` |
|     ! 0 | 1361 | `		return SXERR_MEM;` |
|       - | 1362 | `	}` |
|    1338 | 1363 | `	pVm->pGlobal = pMap;` |
|       - | 1364 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1338 | 1365 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1338 | 1366 | `	if( pObj == 0 ){` |
|     ! 0 | 1367 | `		return SXERR_MEM;` |
|       - | 1368 | `	}` |
|    1338 | 1369 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1370 | `	/* Record object index */` |
|    1338 | 1371 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1372 | `	/* Install the special $GLOBALS array */` |
|    1338 | 1373 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1338 | 1374 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1375 | `		return rc;` |
|       - | 1376 | `	}` |
|       - | 1377 | `	/* Install superglobals now */` |
|   14698 | 1378 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1379 | `		ph7_value *pSuper;` |
|       - | 1380 | `		/* Request an empty array */` |
|   13362 | 1381 | `		pSuper = ph7_new_array(&(*pVm));` |
|   13362 | 1382 | `		if( pSuper == 0 ){` |
|     ! 0 | 1383 | `			return SXERR_MEM;` |
|       - | 1384 | `		}` |
|       - | 1385 | `		/* Install */` |
|   13362 | 1386 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   13362 | 1387 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1388 | `			return rc;` |
|       - | 1389 | `		}` |
|       - | 1390 | `		/* Release the value now it have been installed */` |
|   13362 | 1391 | `		ph7_release_value(&(*pVm),pSuper);` |
|    6682 | 1392 | `	}` |
|       - | 1393 | `	/* Set some $_SERVER entries */` |
|    1338 | 1394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1395 | `	/*` |
|       - | 1396 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1397 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1398 | `	 */` |
|    2670 | 1399 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1400 | `		"SCRIPT_FILENAME",` |
|     668 | 1401 | `		pFile ? pFile->zString : ":Memory:",` |
|    1332 | 1402 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1403 | `		);` |
|       - | 1404 | `	/* All done,all super-global are installed now */` |
|    1338 | 1405 | `	return SXRET_OK;` |
|     670 | 1406 |  |
|       - | 1407 | `/*` |
|       - | 1408 | ` * Release a hashmap.` |
|       - | 1409 | ` */` |
|   34984 | 1410 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1411 |  |
|       - | 1412 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   34986 | 1413 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1414 | `	sxu32 n;` |
|   34986 | 1415 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1416 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1417 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1418 | `		return SXRET_OK;` |
|       - | 1419 | `	}` |
|       - | 1420 | `	/* Start the release process */` |
|   34986 | 1421 | `	n = 0;` |
|   34986 | 1422 | `	pEntry = pMap->pFirst;` |
| 1382382 | 1423 | `	for(;;){` |
| 2764766 | 1424 | `		if( n >= pMap->nEntry ){` |
|   34986 | 1425 | `			break;` |
|       - | 1426 | `		}` |
| 2729782 | 1427 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1428 | `		/* Remove the reference from the foreign table */` |
| 2729782 | 1429 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2729782 | 1430 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1431 | `			/* Restore the ph7_value to the free list */` |
| 2729774 | 1432 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1364886 | 1433 | `		}` |
|       - | 1434 | `		/* Release the node */` |
| 2729782 | 1435 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   54712 | 1436 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   27355 | 1437 | `		}` |
| 2729782 | 1438 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1439 | `		/* Point to the next entry */` |
| 2729782 | 1440 | `		pEntry = pNext;` |
| 2729782 | 1441 | `		n++;` |
|       2 | 1442 | `	}` |
|   34986 | 1443 | `	if( pMap->nEntry > 0 ){` |
|       - | 1444 | `		/* Release the hash bucket */` |
|   31198 | 1445 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   15598 | 1446 | `	}` |
|   34986 | 1447 | `	if( FreeDS ){` |
|       - | 1448 | `		/* Free the whole instance */` |
|   34984 | 1449 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   17493 | 1450 | `	}else{` |
|       - | 1451 | `		/* Keep the instance but reset it's fields */` |
|       3 | 1452 | `		pMap->apBucket = 0;` |
|       3 | 1453 | `		pMap->iNextIdx = 0;` |
|       3 | 1454 | `		pMap->nEntry = pMap->nSize = 0;` |
|       3 | 1455 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1456 | `	}` |
|   34986 | 1457 | `	return SXRET_OK;` |
|   17494 | 1458 |  |
|       - | 1459 | `/*` |
|       - | 1460 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1461 | ` * If the count reaches zero which mean no more variables` |
|       - | 1462 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1463 | ` */` |
|  410002 | 1464 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1465 |  |
|  410004 | 1466 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1467 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  410004 | 1468 | `	pMap->iRef--;` |
|  410004 | 1469 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   34984 | 1470 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   17491 | 1471 | `	}` |
|  410004 | 1472 |  |
|       - | 1473 | `/*` |
|       - | 1474 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1475 | ` * Write a pointer to the target node on success.` |
|       - | 1476 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1477 | ` */` |
|   73044 | 1478 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1479 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1480 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1481 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1482 | `	)` |
|       2 | 1483 |  |
|       - | 1484 | `	sxi32 rc;` |
|   73046 | 1485 | `	if( pMap->nEntry < 1 ){` |
|       - | 1486 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1487 | `		 */` |
|       9 | 1488 | `		return SXERR_NOTFOUND;` |
|       - | 1489 | `	}` |
|   73038 | 1490 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   73038 | 1491 | `	return rc;` |
|   36524 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1495 | ` * hashmap.` |
|       - | 1496 | ` * If a node with the given key already exists in the database` |
|       - | 1497 | ` * then this function overwrite the old value.` |
|       - | 1498 | ` */` |
| 2377634 | 1499 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1500 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1501 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1502 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1503 | `	)` |
|       2 | 1504 |  |
|       - | 1505 | `	sxi32 rc;` |
| 2377636 | 1506 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1507 | `		/*` |
|       - | 1508 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1509 | `		 */` |
|     ! 0 | 1510 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1511 | `		return SXRET_OK;` |
|       - | 1512 | `	}` |
| 2377636 | 1513 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2377636 | 1514 | `	return rc;` |
| 1188819 | 1515 |  |
|       - | 1516 | `/*` |
|       - | 1517 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1518 | ` * hashmap.` |
|       - | 1519 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1520 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1521 | ` * The insertion by reference is triggered when the following` |
|       - | 1522 | ` * expression is encountered.` |
|       - | 1523 | ` * $var = 10;` |
|       - | 1524 | ` *  $a = array(&var);` |
|       - | 1525 | ` * OR` |
|       - | 1526 | ` *  $a[] =& $var;` |
|       - | 1527 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1528 | ` * over it's contents.` |
|       - | 1529 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1530 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1531 | ` * Example:` |
|       - | 1532 | ` *  $var = 10;` |
|       - | 1533 | ` *  $a[] =& $var;` |
|       - | 1534 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1535 | ` *  //Unset the foreign ph7_value now` |
|       - | 1536 | ` *  unset($var);` |
|       - | 1537 | ` *  echo count($a); //0` |
|       - | 1538 | ` * Note that this is a PH7 eXtension.` |
|       - | 1539 | ` * Refer to the official documentation for more information.` |
|       - | 1540 | ` * If a node with the given key already exists in the database` |
|       - | 1541 | ` * then this function overwrite the old value.` |
|       - | 1542 | ` */` |
|   18382 | 1543 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1544 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1545 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1546 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1547 | `	)` |
|       2 | 1548 |  |
|       - | 1549 | `	sxi32 rc;` |
|   18384 | 1550 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1551 | `		/*` |
|       - | 1552 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1553 | `		 */` |
|     ! 0 | 1554 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1555 | `		return SXRET_OK;` |
|       - | 1556 | `	}` |
|   18384 | 1557 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   18384 | 1558 | `	return rc;` |
|    9193 | 1559 |  |
|       - | 1560 | `/*` |
|       - | 1561 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1562 | ` */` |
|   15588 | 1563 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1564 |  |
|       - | 1565 | `	/* Reset the loop cursor */` |
|   15590 | 1566 | `	pMap->pCur = pMap->pFirst;` |
|   15590 | 1567 |  |
|       - | 1568 | `/*` |
|       - | 1569 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1570 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1571 | ` * return NULL.` |
|       - | 1572 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1573 | ` */` |
|  128714 | 1574 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1575 |  |
|  128716 | 1576 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  128716 | 1577 | `	if( pCur == 0 ){` |
|       - | 1578 | `		/* End of the list,return null */` |
|    7798 | 1579 | `		return 0;` |
|       - | 1580 | `	}` |
|       - | 1581 | `	/* Advance the node cursor */` |
|  120920 | 1582 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  120920 | 1583 | `	return pCur;` |
|   64359 | 1584 |  |
|       - | 1585 | `/*` |
|       - | 1586 | ` * Extract a node value.` |
|       - | 1587 | ` */` |
|  307726 | 1588 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1589 |  |
|  307728 | 1590 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  307728 | 1591 | `	if( pEntry ){` |
|  307728 | 1592 | `		if( bStore ){` |
|  120974 | 1593 | `			PH7_MemObjStore(pEntry,pValue);` |
|   60488 | 1594 | `		}else{` |
|  186756 | 1595 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1596 | `		}` |
|  153936 | 1597 | `	}else{` |
|     ! 0 | 1598 | `		PH7_MemObjRelease(pValue);` |
|       - | 1599 | `	}` |
|  307728 | 1600 |  |
|       - | 1601 | `/*` |
|       - | 1602 | ` * Extract a node key.` |
|       - | 1603 | ` */` |
|   83638 | 1604 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1605 |  |
|       - | 1606 | `	/* Fill with the current key */` |
|   83640 | 1607 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   83506 | 1608 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|       3 | 1609 | `			SyBlobRelease(&pKey->sBlob);` |
|       1 | 1610 | `		}` |
|   83506 | 1611 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   83506 | 1612 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   41754 | 1613 | `	}else{` |
|     135 | 1614 | `		SyBlobReset(&pKey->sBlob);` |
|     135 | 1615 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     135 | 1616 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1617 | `	}` |
|   83640 | 1618 |  |
|       - | 1619 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1620 | `/*` |
|       - | 1621 | ` * Store the address of nodes value in the given container.` |
|       - | 1622 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1623 | ` * defined in 'builtin.c' for more information.` |
|       - | 1624 | ` */` |
|      10 | 1625 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1626 |  |
|      11 | 1627 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1628 | `	ph7_value *pValue;` |
|       - | 1629 | `	sxu32 n;` |
|       - | 1630 | `	/* Initialize the container */` |
|      11 | 1631 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1632 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1633 | `		/* Extract node value */` |
|      17 | 1634 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1635 | `		if( pValue ){` |
|      17 | 1636 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1637 | `		}` |
|       - | 1638 | `		/* Point to the next entry */` |
|      17 | 1639 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1640 | `	}` |
|       - | 1641 | `	/* Total inserted entries */` |
|      11 | 1642 | `	return (int)SySetUsed(pOut);` |
|       1 | 1643 |  |
|       - | 1644 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1645 | `/*` |
|       - | 1646 | ` * Merge sort.` |
|       - | 1647 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1648 | ` * Status: Public domain` |
|       - | 1649 | ` */` |
|       - | 1650 | `/* Node comparison callback signature */` |
|       - | 1651 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1652 | `/*` |
|       - | 1653 | `** Inputs:` |
|       - | 1654 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1655 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1656 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1657 | `**` |
|       - | 1658 | `** Return Value:` |
|       - | 1659 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1660 | `**   of both a and b.` |
|       - | 1661 | `**` |
|       - | 1662 | `** Side effects:` |
|       - | 1663 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1664 | `**   changed.` |
|       - | 1665 | `*/` |
|   22114 | 1666 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1667 |  |
|       - | 1668 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1669 | `    /* Prevent compiler warning */` |
|   22116 | 1670 | `	result.pNext = result.pPrev = 0;` |
|   22116 | 1671 | `	pTail = &result;` |
|   56620 | 1672 | `	while( pA && pB ){` |
|   34506 | 1673 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|   22613 | 1674 | `			pTail->pPrev = pA;` |
|   22613 | 1675 | `			pA->pNext = pTail;` |
|   22613 | 1676 | `			pTail = pA;` |
|   22613 | 1677 | `			pA = pA->pPrev;` |
|   11324 | 1678 | `		}else{` |
|   11895 | 1679 | `			pTail->pPrev = pB;` |
|   11895 | 1680 | `			pB->pNext = pTail;` |
|   11895 | 1681 | `			pTail = pB;` |
|   11895 | 1682 | `			pB = pB->pPrev;` |
|       - | 1683 | `		}` |
|       2 | 1684 | `	}` |
|   22116 | 1685 | `	if( pA ){` |
|   16422 | 1686 | `		pTail->pPrev = pA;` |
|   16422 | 1687 | `		pA->pNext = pTail;` |
|   13910 | 1688 | `	}else if( pB ){` |
|    5554 | 1689 | `		pTail->pPrev = pB;` |
|    5554 | 1690 | `		pB->pNext = pTail;` |
|    2774 | 1691 | `	}else{` |
|     144 | 1692 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1693 | `	}` |
|   22116 | 1694 | `	return result.pPrev;` |
|       2 | 1695 |  |
|       - | 1696 | `/*` |
|       - | 1697 | `** Inputs:` |
|       - | 1698 | `**   Map:       Input hashmap` |
|       - | 1699 | `**   cmp:       A comparison function.` |
|       - | 1700 | `**` |
|       - | 1701 | `** Return Value:` |
|       - | 1702 | `**   Sorted hashmap.` |
|       - | 1703 | `**` |
|       - | 1704 | `** Side effects:` |
|       - | 1705 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1706 | `*/` |
|       - | 1707 | `#define N_SORT_BUCKET  32` |
|     498 | 1708 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1709 |  |
|       - | 1710 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1711 | `	sxu32 i;` |
|     500 | 1712 | `	SyZero(a,sizeof(a));` |
|       - | 1713 | `	/* Point to the first inserted entry */` |
|     500 | 1714 | `	pIn = pMap->pFirst;` |
|    8010 | 1715 | `	while( pIn ){` |
|    7512 | 1716 | `		p = pIn;` |
|    7512 | 1717 | `		pIn = p->pPrev;` |
|    7512 | 1718 | `		p->pPrev = 0;` |
|   14188 | 1719 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   14188 | 1720 | `			if( a[i]==0 ){` |
|    7512 | 1721 | `				a[i] = p;` |
|    7512 | 1722 | `				break;` |
|     ! 0 | 1723 | `			}else{` |
|    6678 | 1724 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    6678 | 1725 | `				a[i] = 0;` |
|       - | 1726 | `			}` |
|    3340 | 1727 | `		}` |
|    7512 | 1728 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1729 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1730 | `			 * But that is impossible.` |
|       - | 1731 | `			 */` |
|     ! 0 | 1732 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1733 | `		}` |
|       2 | 1734 | `	}` |
|     500 | 1735 | `	p = a[0];` |
|   15938 | 1736 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   15440 | 1737 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    7721 | 1738 | `	}` |
|     500 | 1739 | `	p->pNext = 0;` |
|       - | 1740 | `	/* Reflect the change */` |
|     500 | 1741 | `	pMap->pFirst = p;` |
|       - | 1742 | `	/* Reset the loop cursor */` |
|     500 | 1743 | `	pMap->pCur = pMap->pFirst;` |
|     500 | 1744 | `	return SXRET_OK;` |
|       2 | 1745 |  |
|       - | 1746 | `/*` |
|       - | 1747 | ` * Node comparison callback.` |
|       - | 1748 | ` * used-by: [sort(),asort(),...]` |
|       - | 1749 | ` */` |
|   34441 | 1750 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1751 |  |
|       - | 1752 | `	ph7_value sA,sB;` |
|       - | 1753 | `	sxi32 iFlags;` |
|       - | 1754 | `	int rc;` |
|   34443 | 1755 | `	if( pCmpData == 0 ){` |
|       - | 1756 | `		/* Perform a standard comparison */` |
|   34439 | 1757 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   34439 | 1758 | `		return rc;` |
|       - | 1759 | `	}` |
|       5 | 1760 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1761 | `	/* Duplicate node values */` |
|       5 | 1762 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|       5 | 1763 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|       5 | 1764 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|       5 | 1765 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|       5 | 1766 | `	if( iFlags == 5 ){` |
|       - | 1767 | `		/* String cast */` |
|       5 | 1768 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1769 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1770 | `		}` |
|       5 | 1771 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1772 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1773 | `		}` |
|       3 | 1774 | `	}else{` |
|       - | 1775 | `		/* Numeric cast */` |
|     ! 0 | 1776 | `		PH7_MemObjToNumeric(&sA);` |
|     ! 0 | 1777 | `		PH7_MemObjToNumeric(&sB);` |
|       - | 1778 | `	}` |
|       - | 1779 | `	/* Perform the comparison */` |
|       5 | 1780 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       5 | 1781 | `	PH7_MemObjRelease(&sA);` |
|       5 | 1782 | `	PH7_MemObjRelease(&sB);` |
|       5 | 1783 | `	return rc;` |
|   17258 | 1784 |  |
|       - | 1785 | `/*` |
|       - | 1786 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1787 | ` * used-by: [ksort()]` |
|       - | 1788 | ` */` |
|      14 | 1789 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1790 |  |
|       - | 1791 | `	sxi32 rc;` |
|       7 | 1792 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 1793 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1794 | `		/* Perform a string comparison */` |
|       5 | 1795 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1796 | `	}else{` |
|       - | 1797 | `		SyString sStr;` |
|       - | 1798 | `		sxi64 iA,iB;` |
|       - | 1799 | `		/* Perform a numeric comparison */` |
|      11 | 1800 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1801 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1802 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1803 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1804 | `				iA = 0;` |
|     ! 0 | 1805 | `			}else{` |
|     ! 0 | 1806 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1807 | `			}` |
|     ! 0 | 1808 | `		}else{` |
|      11 | 1809 | `			iA = pA->xKey.iKey;` |
|       - | 1810 | `		}` |
|      11 | 1811 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1812 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1813 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1814 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1815 | `				iB = 0;` |
|     ! 0 | 1816 | `			}else{` |
|     ! 0 | 1817 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1818 | `			}` |
|     ! 0 | 1819 | `		}else{` |
|      11 | 1820 | `			iB = pB->xKey.iKey;` |
|       - | 1821 | `		}` |
|      11 | 1822 | `		rc = (sxi32)(iA-iB);` |
|       - | 1823 | `	}` |
|       - | 1824 | `	/* Comparison result */` |
|      15 | 1825 | `	return rc;` |
|       1 | 1826 |  |
|       - | 1827 | `/*` |
|       - | 1828 | ` * Node comparison callback.` |
|       - | 1829 | ` * Used by: [rsort(),arsort()];` |
|       - | 1830 | ` */` |
|      12 | 1831 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1832 |  |
|       - | 1833 | `	ph7_value sA,sB;` |
|       - | 1834 | `	sxi32 iFlags;` |
|       - | 1835 | `	int rc;` |
|      13 | 1836 | `	if( pCmpData == 0 ){` |
|       - | 1837 | `		/* Perform a standard comparison */` |
|      13 | 1838 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      13 | 1839 | `		return -rc;` |
|       - | 1840 | `	}` |
|     ! 0 | 1841 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1842 | `	/* Duplicate node values */` |
|     ! 0 | 1843 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|     ! 0 | 1844 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|     ! 0 | 1845 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|     ! 0 | 1846 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|     ! 0 | 1847 | `	if( iFlags == 5 ){` |
|       - | 1848 | `		/* String cast */` |
|     ! 0 | 1849 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1850 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1851 | `		}` |
|     ! 0 | 1852 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1853 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1854 | `		}` |
|     ! 0 | 1855 | `	}else{` |
|       - | 1856 | `		/* Numeric cast */` |
|     ! 0 | 1857 | `		PH7_MemObjToNumeric(&sA);` |
|     ! 0 | 1858 | `		PH7_MemObjToNumeric(&sB);` |
|       - | 1859 | `	}` |
|       - | 1860 | `	/* Perform the comparison */` |
|     ! 0 | 1861 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|     ! 0 | 1862 | `	PH7_MemObjRelease(&sA);` |
|     ! 0 | 1863 | `	PH7_MemObjRelease(&sB);` |
|     ! 0 | 1864 | `	return -rc;` |
|       7 | 1865 |  |
|       - | 1866 | `/*` |
|       - | 1867 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1868 | ` * used-by: [usort(),uasort()]` |
|       - | 1869 | ` */` |
|      12 | 1870 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1871 |  |
|       - | 1872 | `	ph7_value sResult,*pCallback;` |
|       - | 1873 | `	ph7_value *pV1,*pV2;` |
|       - | 1874 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1875 | `	sxi32 rc;` |
|       - | 1876 | `	/* Point to the desired callback */` |
|      13 | 1877 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1878 | `	/* initialize the result value */` |
|      13 | 1879 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 1880 | `	/* Extract nodes values */` |
|      13 | 1881 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      13 | 1882 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      13 | 1883 | `	apArg[0] = pV1;` |
|      13 | 1884 | `	apArg[1] = pV2;` |
|       - | 1885 | `	/* Invoke the callback */` |
|      13 | 1886 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      13 | 1887 | `	if( rc != SXRET_OK ){` |
|       - | 1888 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1889 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1890 | `	}else{` |
|       - | 1891 | `		/* Extract callback result */` |
|      13 | 1892 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1893 | `			/* Perform an int cast */` |
|     ! 0 | 1894 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1895 | `		}` |
|      13 | 1896 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1897 | `	}` |
|      13 | 1898 | `	PH7_MemObjRelease(&sResult);` |
|       - | 1899 | `	/* Callback result */` |
|      13 | 1900 | `	return rc;` |
|       1 | 1901 |  |
|       - | 1902 | `/*` |
|       - | 1903 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1904 | ` * used-by: [krsort()]` |
|       - | 1905 | ` */` |
|       4 | 1906 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1907 |  |
|       - | 1908 | `	sxi32 rc;` |
|       2 | 1909 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 1910 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1911 | `		/* Perform a string comparison */` |
|       5 | 1912 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1913 | `	}else{` |
|       - | 1914 | `		SyString sStr;` |
|       - | 1915 | `		sxi64 iA,iB;` |
|       - | 1916 | `		/* Perform a numeric comparison */` |
|     ! 0 | 1917 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1918 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1919 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1920 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1921 | `				iA = 0;` |
|     ! 0 | 1922 | `			}else{` |
|     ! 0 | 1923 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1924 | `			}` |
|     ! 0 | 1925 | `		}else{` |
|     ! 0 | 1926 | `			iA = pA->xKey.iKey;` |
|       - | 1927 | `		}` |
|     ! 0 | 1928 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1929 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1930 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1931 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1932 | `				iB = 0;` |
|     ! 0 | 1933 | `			}else{` |
|     ! 0 | 1934 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1935 | `			}` |
|     ! 0 | 1936 | `		}else{` |
|     ! 0 | 1937 | `			iB = pB->xKey.iKey;` |
|       - | 1938 | `		}` |
|     ! 0 | 1939 | `		rc = (sxi32)(iA-iB);` |
|       - | 1940 | `	}` |
|       5 | 1941 | `	return -rc; /* Reverse result */` |
|       1 | 1942 |  |
|       - | 1943 | `/*` |
|       - | 1944 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1945 | ` * used-by: [uksort()]` |
|       - | 1946 | ` */` |
|       6 | 1947 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1948 |  |
|       - | 1949 | `	ph7_value sResult,*pCallback;` |
|       - | 1950 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1951 | `	ph7_value sK1,sK2;` |
|       - | 1952 | `	sxi32 rc;` |
|       - | 1953 | `	/* Point to the desired callback */` |
|       7 | 1954 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1955 | `	/* initialize the result value */` |
|       7 | 1956 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 1957 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 1958 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 1959 | `	/* Extract nodes keys */` |
|       7 | 1960 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 1961 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 1962 | `	apArg[0] = &sK1;` |
|       7 | 1963 | `	apArg[1] = &sK2;` |
|       - | 1964 | `	/* Mark keys as constants */` |
|       7 | 1965 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 1966 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 1967 | `	/* Invoke the callback */` |
|       7 | 1968 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 1969 | `	if( rc != SXRET_OK ){` |
|       - | 1970 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1971 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1972 | `	}else{` |
|       - | 1973 | `		/* Extract callback result */` |
|       7 | 1974 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1975 | `			/* Perform an int cast */` |
|     ! 0 | 1976 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1977 | `		}` |
|       7 | 1978 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1979 | `	}` |
|       7 | 1980 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 1981 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 1982 | `	PH7_MemObjRelease(&sK2);` |
|       - | 1983 | `	/* Callback result */` |
|       7 | 1984 | `	return rc;` |
|       1 | 1985 |  |
|       - | 1986 | `/*` |
|       - | 1987 | ` * Node comparison callback: Random node comparison.` |
|       - | 1988 | ` * used-by: [shuffle()]` |
|       - | 1989 | ` */` |
|      15 | 1990 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1991 |  |
|       - | 1992 | `	sxu32 n;` |
|       7 | 1993 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 1994 | `	SXUNUSED(pCmpData);` |
|       - | 1995 | `	/* Grab a random number */` |
|      16 | 1996 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 1997 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 1998 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 1999 | `	 */` |
|      16 | 2000 | `	return n&1 ? 1 : -1;` |
|       1 | 2001 |  |
|       - | 2002 | `/*` |
|       - | 2003 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2004 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2005 | ` */` |
|     482 | 2006 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2007 |  |
|       - | 2008 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2009 | `	sxu32 i;` |
|       - | 2010 | `	/* Rehash all entries */` |
|     484 | 2011 | `	pLast = p = pMap->pFirst;` |
|     484 | 2012 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     484 | 2013 | `	i = 0;` |
|    3969 | 2014 | `	for( ;; ){` |
|    7940 | 2015 | `		if( i >= pMap->nEntry ){` |
|     484 | 2016 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     484 | 2017 | `			break;` |
|       - | 2018 | `		}` |
|    7458 | 2019 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2020 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2021 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2022 | `			/* Change key type */` |
|       5 | 2023 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2024 | `		}` |
|    7458 | 2025 | `		HashmapRehashIntNode(p);` |
|       - | 2026 | `		/* Point to the next entry */` |
|    7458 | 2027 | `		i++;` |
|    7458 | 2028 | `		pLast = p;` |
|    7458 | 2029 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2030 | `	}` |
|     484 | 2031 |  |
|       - | 2032 | `/*` |
|       - | 2033 | ` * Array functions implementation.` |
|       - | 2034 | ` * Status:` |
|       - | 2035 | ` *  Stable.` |
|       - | 2036 | ` */` |
|       - | 2037 | `/*` |
|       - | 2038 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2039 | ` * Sort an array.` |
|       - | 2040 | ` * Parameters` |
|       - | 2041 | ` *  $array` |
|       - | 2042 | ` *   The input array.` |
|       - | 2043 | ` * $sort_flags` |
|       - | 2044 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2045 | ` *  Sorting type flags:` |
|       - | 2046 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2047 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2048 | ` *   SORT_STRING - compare items as strings` |
|       - | 2049 | ` * Return` |
|       - | 2050 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2051 | ` *` |
|       - | 2052 | ` */` |
|     806 | 2053 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2054 |  |
|       - | 2055 | `	ph7_hashmap *pMap;` |
|       - | 2056 | `	/* Make sure we are dealing with a valid hashmap */` |
|     808 | 2057 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2060 | `		return PH7_OK;` |
|       - | 2061 | `	}` |
|       - | 2062 | `	/* Point to the internal representation of the input hashmap */` |
|     808 | 2063 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     808 | 2064 | `	if( pMap->nEntry > 1 ){` |
|     478 | 2065 | `		sxi32 iCmpFlags = 0;` |
|     478 | 2066 | `		if( nArg > 1 ){` |
|       - | 2067 | `			/* Extract comparison flags */` |
|       3 | 2068 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2069 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2070 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2071 | `			}` |
|       1 | 2072 | `		}` |
|       - | 2073 | `		/* Do the merge sort */` |
|     478 | 2074 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2075 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     478 | 2076 | `		HashmapSortRehash(pMap);` |
|     238 | 2077 | `	}` |
|       - | 2078 | `	/* All done,return TRUE */` |
|     808 | 2079 | `	ph7_result_bool(pCtx,1);` |
|     808 | 2080 | `	return PH7_OK;` |
|     405 | 2081 |  |
|       - | 2082 | `/*` |
|       - | 2083 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2084 | ` *  Sort an array and maintain index association.` |
|       - | 2085 | ` * Parameters` |
|       - | 2086 | ` *  $array` |
|       - | 2087 | ` *   The input array.` |
|       - | 2088 | ` * $sort_flags` |
|       - | 2089 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2090 | ` *  Sorting type flags:` |
|       - | 2091 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2092 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2093 | ` *   SORT_STRING - compare items as strings` |
|       - | 2094 | ` * Return` |
|       - | 2095 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2096 | ` */` |
|       2 | 2097 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2098 |  |
|       - | 2099 | `	ph7_hashmap *pMap;` |
|       - | 2100 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2101 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2102 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2103 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2104 | `		return PH7_OK;` |
|       - | 2105 | `	}` |
|       - | 2106 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2107 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2108 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2109 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2110 | `		if( nArg > 1 ){` |
|       - | 2111 | `			/* Extract comparison flags */` |
|     ! 0 | 2112 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2113 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2114 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2115 | `			}` |
|     ! 0 | 2116 | `		}` |
|       - | 2117 | `		/* Do the merge sort */` |
|       3 | 2118 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2119 | `		/* Fix the last link broken by the merge */` |
|       5 | 2120 | `		while(pMap->pLast->pPrev){` |
|       3 | 2121 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2122 | `		}` |
|       1 | 2123 | `	}` |
|       - | 2124 | `	/* All done,return TRUE */` |
|       3 | 2125 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2126 | `	return PH7_OK;` |
|       2 | 2127 |  |
|       - | 2128 | `/*` |
|       - | 2129 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2130 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2131 | ` * Parameters` |
|       - | 2132 | ` *  $array` |
|       - | 2133 | ` *   The input array.` |
|       - | 2134 | ` * $sort_flags` |
|       - | 2135 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2136 | ` *  Sorting type flags:` |
|       - | 2137 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2138 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2139 | ` *   SORT_STRING - compare items as strings` |
|       - | 2140 | ` * Return` |
|       - | 2141 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2142 | ` */` |
|       2 | 2143 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2144 |  |
|       - | 2145 | `	ph7_hashmap *pMap;` |
|       - | 2146 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2147 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2148 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2149 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2150 | `		return PH7_OK;` |
|       - | 2151 | `	}` |
|       - | 2152 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2153 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2154 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2155 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2156 | `		if( nArg > 1 ){` |
|       - | 2157 | `			/* Extract comparison flags */` |
|     ! 0 | 2158 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2159 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2160 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2161 | `			}` |
|     ! 0 | 2162 | `		}` |
|       - | 2163 | `		/* Do the merge sort */` |
|       3 | 2164 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2165 | `		/* Fix the last link broken by the merge */` |
|       5 | 2166 | `		while(pMap->pLast->pPrev){` |
|       3 | 2167 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2168 | `		}` |
|       1 | 2169 | `	}` |
|       - | 2170 | `	/* All done,return TRUE */` |
|       3 | 2171 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2172 | `	return PH7_OK;` |
|       2 | 2173 |  |
|       - | 2174 | `/*` |
|       - | 2175 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2176 | ` *  Sort an array by key.` |
|       - | 2177 | ` * Parameters` |
|       - | 2178 | ` *  $array` |
|       - | 2179 | ` *   The input array.` |
|       - | 2180 | ` * $sort_flags` |
|       - | 2181 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2182 | ` *  Sorting type flags:` |
|       - | 2183 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2184 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2185 | ` *   SORT_STRING - compare items as strings` |
|       - | 2186 | ` * Return` |
|       - | 2187 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2188 | ` */` |
|       4 | 2189 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2190 |  |
|       - | 2191 | `	ph7_hashmap *pMap;` |
|       - | 2192 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2193 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2194 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2195 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2196 | `		return PH7_OK;` |
|       - | 2197 | `	}` |
|       - | 2198 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2199 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2200 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2201 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2202 | `		if( nArg > 1 ){` |
|       - | 2203 | `			/* Extract comparison flags */` |
|     ! 0 | 2204 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2205 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2206 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2207 | `			}` |
|     ! 0 | 2208 | `		}` |
|       - | 2209 | `		/* Do the merge sort */` |
|       5 | 2210 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2211 | `		/* Fix the last link broken by the merge */` |
|      15 | 2212 | `		while(pMap->pLast->pPrev){` |
|      11 | 2213 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2214 | `		}` |
|       2 | 2215 | `	}` |
|       - | 2216 | `	/* All done,return TRUE */` |
|       5 | 2217 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2218 | `	return PH7_OK;` |
|       3 | 2219 |  |
|       - | 2220 | `/*` |
|       - | 2221 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2222 | ` *  Sort an array by key in reverse order.` |
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
|       - | 2234 | ` */` |
|       2 | 2235 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2236 |  |
|       - | 2237 | `	ph7_hashmap *pMap;` |
|       - | 2238 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2239 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2240 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2241 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2242 | `		return PH7_OK;` |
|       - | 2243 | `	}` |
|       - | 2244 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2245 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2246 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2247 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2248 | `		if( nArg > 1 ){` |
|       - | 2249 | `			/* Extract comparison flags */` |
|     ! 0 | 2250 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2251 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2252 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2253 | `			}` |
|     ! 0 | 2254 | `		}` |
|       - | 2255 | `		/* Do the merge sort */` |
|       3 | 2256 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2257 | `		/* Fix the last link broken by the merge */` |
|       7 | 2258 | `		while(pMap->pLast->pPrev){` |
|       5 | 2259 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2260 | `		}` |
|       1 | 2261 | `	}` |
|       - | 2262 | `	/* All done,return TRUE */` |
|       3 | 2263 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2264 | `	return PH7_OK;` |
|       2 | 2265 |  |
|       - | 2266 | `/*` |
|       - | 2267 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2268 | ` * Sort an array in reverse order.` |
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
|       2 | 2281 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2282 |  |
|       - | 2283 | `	ph7_hashmap *pMap;` |
|       - | 2284 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2285 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2286 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2287 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2288 | `		return PH7_OK;` |
|       - | 2289 | `	}` |
|       - | 2290 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2291 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2292 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2293 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2294 | `		if( nArg > 1 ){` |
|       - | 2295 | `			/* Extract comparison flags */` |
|     ! 0 | 2296 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2297 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2298 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2299 | `			}` |
|     ! 0 | 2300 | `		}` |
|       - | 2301 | `		/* Do the merge sort */` |
|       3 | 2302 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2303 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2304 | `		HashmapSortRehash(pMap);` |
|       1 | 2305 | `	}` |
|       - | 2306 | `	/* All done,return TRUE */` |
|       3 | 2307 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2308 | `	return PH7_OK;` |
|       2 | 2309 |  |
|       - | 2310 | `/*` |
|       - | 2311 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2312 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2313 | ` * Parameters` |
|       - | 2314 | ` *  $array` |
|       - | 2315 | ` *   The input array.` |
|       - | 2316 | ` * $cmp_function` |
|       - | 2317 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2318 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2319 | ` *  to, or greater than the second.` |
|       - | 2320 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2321 | ` * Return` |
|       - | 2322 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2323 | ` */` |
|       2 | 2324 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2325 |  |
|       - | 2326 | `	ph7_hashmap *pMap;` |
|       - | 2327 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2328 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2329 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2330 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2331 | `		return PH7_OK;` |
|       - | 2332 | `	}` |
|       - | 2333 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2334 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2335 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2336 | `		ph7_value *pCallback = 0;` |
|       - | 2337 | `		ProcNodeCmp xCmp;` |
|       3 | 2338 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2339 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2340 | `			/* Point to the desired callback */` |
|       3 | 2341 | `			pCallback = apArg[1];` |
|       2 | 2342 | `		}else{` |
|       - | 2343 | `			/* Use the default comparison function */` |
|     ! 0 | 2344 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2345 | `		}` |
|       - | 2346 | `		/* Do the merge sort */` |
|       3 | 2347 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2348 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2349 | `		HashmapSortRehash(pMap);` |
|       1 | 2350 | `	}` |
|       - | 2351 | `	/* All done,return TRUE */` |
|       3 | 2352 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2353 | `	return PH7_OK;` |
|       2 | 2354 |  |
|       - | 2355 | `/*` |
|       - | 2356 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2357 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2358 | ` *  and maintain index association.` |
|       - | 2359 | ` * Parameters` |
|       - | 2360 | ` *  $array` |
|       - | 2361 | ` *   The input array.` |
|       - | 2362 | ` * $cmp_function` |
|       - | 2363 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2364 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2365 | ` *  to, or greater than the second.` |
|       - | 2366 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2367 | ` * Return` |
|       - | 2368 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2369 | ` */` |
|       2 | 2370 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2371 |  |
|       - | 2372 | `	ph7_hashmap *pMap;` |
|       - | 2373 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2374 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2375 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2376 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2377 | `		return PH7_OK;` |
|       - | 2378 | `	}` |
|       - | 2379 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2380 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2381 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2382 | `		ph7_value *pCallback = 0;` |
|       - | 2383 | `		ProcNodeCmp xCmp;` |
|       3 | 2384 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2385 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2386 | `			/* Point to the desired callback */` |
|       3 | 2387 | `			pCallback = apArg[1];` |
|       2 | 2388 | `		}else{` |
|       - | 2389 | `			/* Use the default comparison function */` |
|     ! 0 | 2390 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2391 | `		}` |
|       - | 2392 | `		/* Do the merge sort */` |
|       3 | 2393 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2394 | `		/* Fix the last link broken by the merge */` |
|       5 | 2395 | `		while(pMap->pLast->pPrev){` |
|       3 | 2396 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2397 | `		}` |
|       1 | 2398 | `	}` |
|       - | 2399 | `	/* All done,return TRUE */` |
|       3 | 2400 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2401 | `	return PH7_OK;` |
|       2 | 2402 |  |
|       - | 2403 | `/*` |
|       - | 2404 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2405 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2406 | ` *  function and maintain index association.` |
|       - | 2407 | ` * Parameters` |
|       - | 2408 | ` *  $array` |
|       - | 2409 | ` *   The input array.` |
|       - | 2410 | ` * $cmp_function` |
|       - | 2411 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2412 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2413 | ` *  to, or greater than the second.` |
|       - | 2414 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2415 | ` * Return` |
|       - | 2416 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2417 | ` */` |
|       2 | 2418 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2419 |  |
|       - | 2420 | `	ph7_hashmap *pMap;` |
|       - | 2421 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2422 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2423 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2424 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2425 | `		return PH7_OK;` |
|       - | 2426 | `	}` |
|       - | 2427 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2428 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2429 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2430 | `		ph7_value *pCallback = 0;` |
|       - | 2431 | `		ProcNodeCmp xCmp;` |
|       3 | 2432 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2433 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2434 | `			/* Point to the desired callback */` |
|       3 | 2435 | `			pCallback = apArg[1];` |
|       2 | 2436 | `		}else{` |
|       - | 2437 | `			/* Use the default comparison function */` |
|     ! 0 | 2438 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2439 | `		}` |
|       - | 2440 | `		/* Do the merge sort */` |
|       3 | 2441 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2442 | `		/* Fix the last link broken by the merge */` |
|       3 | 2443 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2444 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2445 | `		}` |
|       1 | 2446 | `	}` |
|       - | 2447 | `	/* All done,return TRUE */` |
|       3 | 2448 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2449 | `	return PH7_OK;` |
|       2 | 2450 |  |
|       - | 2451 | `/*` |
|       - | 2452 | ` * bool shuffle(array &$array)` |
|       - | 2453 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2454 | ` * Parameters` |
|       - | 2455 | ` *  $array` |
|       - | 2456 | ` *   The input array.` |
|       - | 2457 | ` * Return` |
|       - | 2458 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2459 | ` *` |
|       - | 2460 | ` */` |
|       2 | 2461 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2462 |  |
|       - | 2463 | `	ph7_hashmap *pMap;` |
|       - | 2464 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2465 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2466 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2467 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2468 | `		return PH7_OK;` |
|       - | 2469 | `	}` |
|       - | 2470 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2471 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2472 | `	if( pMap->nEntry > 1 ){` |
|       - | 2473 | `		/* Do the merge sort */` |
|       3 | 2474 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2475 | `		/* Fix the last link broken by the merge */` |
|      10 | 2476 | `		while(pMap->pLast->pPrev){` |
|       8 | 2477 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2478 | `		}` |
|       1 | 2479 | `	}` |
|       - | 2480 | `	/* All done,return TRUE */` |
|       3 | 2481 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2482 | `	return PH7_OK;` |
|       2 | 2483 |  |
|       - | 2484 | `/*` |
|       - | 2485 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2486 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2487 | ` * Parameters` |
|       - | 2488 | ` *  $var` |
|       - | 2489 | ` *   The array or the object.` |
|       - | 2490 | ` * $mode` |
|       - | 2491 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2492 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2493 | ` *  all the elements of a multidimensional array. count() does not detect infinite` |
|       - | 2494 | ` *  recursion.` |
|       - | 2495 | ` * Return` |
|       - | 2496 | ` *  Returns the number of elements in the array.` |
|       - | 2497 | ` */` |
|     490 | 2498 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2499 |  |
|     492 | 2500 | `	int bRecursive = FALSE;` |
|       - | 2501 | `	sxi64 iCount;` |
|     492 | 2502 | `	if( nArg < 1 ){` |
|       - | 2503 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 2504 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 2505 | `		return PH7_OK;` |
|       - | 2506 | `	}` |
|     492 | 2507 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2508 | `		/* TICKET 1433-19: Handle objects */` |
|       3 | 2509 | `		int res = !ph7_value_is_null(apArg[0]);` |
|       3 | 2510 | `		ph7_result_int(pCtx,res);` |
|       3 | 2511 | `		return PH7_OK;` |
|       - | 2512 | `	}` |
|     490 | 2513 | `	if( nArg > 1 ){` |
|       - | 2514 | `		/* Recursive count? */` |
|      31 | 2515 | `		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;` |
|      15 | 2516 | `	}` |
|       - | 2517 | `	/* Count */` |
|     490 | 2518 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);` |
|     490 | 2519 | `	ph7_result_int64(pCtx,iCount);` |
|     490 | 2520 | `	return PH7_OK;` |
|     247 | 2521 |  |
|       - | 2522 | `/*` |
|       - | 2523 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2524 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2525 | ` * Parameters` |
|       - | 2526 | ` * $key` |
|       - | 2527 | ` *   Value to check.` |
|       - | 2528 | ` * $search` |
|       - | 2529 | ` *  An array with keys to check.` |
|       - | 2530 | ` * Return` |
|       - | 2531 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2532 | ` */` |
|      46 | 2533 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2534 |  |
|       - | 2535 | `	sxi32 rc;` |
|      48 | 2536 | `	if( nArg != 2 ){` |
|       - | 2537 | `		/* PHP requires exactly two arguments */` |
|      10 | 2538 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2539 | `			"ArgumentCountError",` |
|       - | 2540 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2541 | `			nArg` |
|       - | 2542 | `			);` |
|       - | 2543 | `	}` |
|       - | 2544 | `	/* Make sure we are dealing with a valid hashmap */` |
|      42 | 2545 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2546 | `		/* Type mismatch -> TypeError */` |
|       7 | 2547 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2548 | `			"TypeError",` |
|       - | 2549 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2550 | `			ph7_type_name(apArg[1])` |
|       - | 2551 | `			);` |
|       - | 2552 | `	}` |
|       - | 2553 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      37 | 2554 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2555 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2556 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2557 | `			"use an empty string instead"` |
|       - | 2558 | `			);` |
|      36 | 2559 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2560 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2561 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2562 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2563 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2564 | `				,rVal` |
|       - | 2565 | `				);` |
|       1 | 2566 | `		}` |
|       1 | 2567 | `	}` |
|       - | 2568 | `	/* Perform the lookup */` |
|      37 | 2569 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2570 | `	/* lookup result */` |
|      37 | 2571 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      37 | 2572 | `	return PH7_OK;` |
|      25 | 2573 |  |
|       - | 2574 | `/*` |
|       - | 2575 | ` * value array_pop(array $array)` |
|       - | 2576 | ` *   POP the last inserted element from the array.` |
|       - | 2577 | ` * Parameter` |
|       - | 2578 | ` *  The array to get the value from.` |
|       - | 2579 | ` * Return` |
|       - | 2580 | ` *  Poped value or NULL on failure.` |
|       - | 2581 | ` */` |
|      16 | 2582 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2583 |  |
|       - | 2584 | `	ph7_hashmap *pMap;` |
|       - | 2585 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      18 | 2586 | `	if( nArg != 1 ){` |
|       7 | 2587 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2588 | `			"ArgumentCountError",` |
|       - | 2589 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2590 | `			nArg` |
|       - | 2591 | `			);` |
|       - | 2592 | `	}` |
|       - | 2593 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2594 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      14 | 2595 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2596 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2597 | `			"Error",` |
|       - | 2598 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2599 | `			);` |
|       - | 2600 | `	}` |
|       - | 2601 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 | 2602 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2603 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2604 | `			"TypeError",` |
|       - | 2605 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2606 | `			ph7_type_name(apArg[0])` |
|       - | 2607 | `			);` |
|       - | 2608 | `	}` |
|       7 | 2609 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 2610 | `	if( pMap->nEntry < 1 ){` |
|       - | 2611 | `		/* Nothing to pop,return NULL */` |
|       3 | 2612 | `		ph7_result_null(pCtx);` |
|       2 | 2613 | `	}else{` |
|       5 | 2614 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2615 | `		ph7_value *pObj;` |
|       5 | 2616 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       5 | 2617 | `		if( pObj ){` |
|       - | 2618 | `			/* Node value */` |
|       5 | 2619 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2620 | `			/* Unlink the node */` |
|       5 | 2621 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       3 | 2622 | `		}else{` |
|     ! 0 | 2623 | `			ph7_result_null(pCtx);` |
|       - | 2624 | `		}` |
|       - | 2625 | `		/* Reset the cursor */` |
|       5 | 2626 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2627 | `	}` |
|       7 | 2628 | `	return PH7_OK;` |
|      10 | 2629 |  |
|       - | 2630 | `/*` |
|       - | 2631 | ` * int array_push($array,$var,...)` |
|       - | 2632 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2633 | ` * Parameters` |
|       - | 2634 | ` *  array` |
|       - | 2635 | ` *    The input array.` |
|       - | 2636 | ` *  var` |
|       - | 2637 | ` *   On or more value to push.` |
|       - | 2638 | ` * Return` |
|       - | 2639 | ` *  New array count (including old items).` |
|       - | 2640 | ` */` |
|      20 | 2641 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2642 |  |
|       - | 2643 | `	ph7_hashmap *pMap;` |
|       - | 2644 | `	sxi32 rc;` |
|       - | 2645 | `	int i;` |
|      22 | 2646 | `	if( nArg < 1 ){` |
|       4 | 2647 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2648 | `			"ArgumentCountError",` |
|       - | 2649 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2650 | `			nArg` |
|       - | 2651 | `			);` |
|       - | 2652 | `	}` |
|       - | 2653 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2654 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      20 | 2655 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2656 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2657 | `			"Error",` |
|       - | 2658 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2659 | `			);` |
|       - | 2660 | `	}` |
|       - | 2661 | `	/* Make sure we are dealing with a valid hashmap */` |
|      16 | 2662 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2663 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2664 | `			"TypeError",` |
|       - | 2665 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2666 | `			ph7_type_name(apArg[0])` |
|       - | 2667 | `			);` |
|       - | 2668 | `	}` |
|       - | 2669 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 2670 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2671 | `	/* Start pushing given values */` |
|      27 | 2672 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      15 | 2673 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      15 | 2674 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2675 | `			break;` |
|       - | 2676 | `		}` |
|       8 | 2677 | `	}` |
|       - | 2678 | `	/* Return the new count */` |
|      13 | 2679 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      13 | 2680 | `	return PH7_OK;` |
|      12 | 2681 |  |
|       - | 2682 | `/*` |
|       - | 2683 | ` * value array_shift(array $array)` |
|       - | 2684 | ` *   Shift an element off the beginning of array.` |
|       - | 2685 | ` * Parameter` |
|       - | 2686 | ` *  The array to get the value from.` |
|       - | 2687 | ` * Return` |
|       - | 2688 | ` *  Shifted value or NULL on failure.` |
|       - | 2689 | ` */` |
|      36 | 2690 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2691 |  |
|       - | 2692 | `	ph7_hashmap *pMap;` |
|       - | 2693 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      38 | 2694 | `	if( nArg != 1 ){` |
|       7 | 2695 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2696 | `			"ArgumentCountError",` |
|       - | 2697 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2698 | `			nArg` |
|       - | 2699 | `			);` |
|       - | 2700 | `	}` |
|       - | 2701 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      34 | 2702 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2703 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2704 | `			"Error",` |
|       - | 2705 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2706 | `			);` |
|       - | 2707 | `	}` |
|       - | 2708 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 | 2709 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2710 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2711 | `			"TypeError",` |
|       - | 2712 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2713 | `			ph7_type_name(apArg[0])` |
|       - | 2714 | `			);` |
|       - | 2715 | `	}` |
|       - | 2716 | `	/* Point to the internal representation of the hashmap */` |
|      28 | 2717 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      28 | 2718 | `	if( pMap->nEntry < 1 ){` |
|       - | 2719 | `		/* Empty hashmap,return NULL */` |
|       3 | 2720 | `		ph7_result_null(pCtx);` |
|       2 | 2721 | `	}else{` |
|      26 | 2722 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2723 | `		ph7_value *pObj;` |
|       - | 2724 | `		sxu32 n;` |
|      26 | 2725 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      26 | 2726 | `		if( pObj ){` |
|       - | 2727 | `			/* Node value */` |
|      26 | 2728 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2729 | `			/* Unlink the first node */` |
|      26 | 2730 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      14 | 2731 | `		}else{` |
|     ! 0 | 2732 | `			ph7_result_null(pCtx);` |
|       - | 2733 | `		}` |
|       - | 2734 | `		/* Rehash all int keys */` |
|      26 | 2735 | `		n = pMap->nEntry;` |
|      26 | 2736 | `		pEntry = pMap->pFirst;` |
|      26 | 2737 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      37 | 2738 | `		for(;;){` |
|      76 | 2739 | `			if( n < 1 ){` |
|      26 | 2740 | `				break;` |
|       - | 2741 | `			}` |
|      52 | 2742 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      52 | 2743 | `				HashmapRehashIntNode(pEntry);` |
|      25 | 2744 | `			}` |
|       - | 2745 | `			/* Point to the next entry */` |
|      52 | 2746 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      52 | 2747 | `			n--;` |
|       2 | 2748 | `		}` |
|       - | 2749 | `		/* Reset the cursor */` |
|      26 | 2750 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2751 | `	}` |
|      28 | 2752 | `	return PH7_OK;` |
|      20 | 2753 |  |
|       - | 2754 | `/*` |
|       - | 2755 | ` * Extract the node cursor value.` |
|       - | 2756 | ` */` |
|      24 | 2757 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 2758 |  |
|      25 | 2759 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 2760 | `	ph7_value *pVal;` |
|      25 | 2761 | `	if( pCur == 0 ){` |
|       - | 2762 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 2763 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2764 | `		return PH7_OK;` |
|       - | 2765 | `	}` |
|      25 | 2766 | `	if( iDirection != 0 ){` |
|       9 | 2767 | `		if( iDirection > 0 ){` |
|       - | 2768 | `			/* Point to the next entry */` |
|       7 | 2769 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 2770 | `			pCur = pMap->pCur;` |
|       4 | 2771 | `		}else{` |
|       - | 2772 | `			/* Point to the previous entry */` |
|       3 | 2773 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 2774 | `			pCur = pMap->pCur;` |
|       - | 2775 | `		}` |
|       9 | 2776 | `		if( pCur == 0 ){` |
|       - | 2777 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 2778 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2779 | `			return PH7_OK;` |
|       - | 2780 | `		}` |
|       4 | 2781 | `	}` |
|       - | 2782 | `	/* Point to the desired element */` |
|      25 | 2783 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 2784 | `	if( pVal ){` |
|      25 | 2785 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 2786 | `	}else{` |
|     ! 0 | 2787 | `		ph7_result_bool(pCtx,0);` |
|       - | 2788 | `	}` |
|      25 | 2789 | `	return PH7_OK;` |
|      13 | 2790 |  |
|       - | 2791 | `/*` |
|       - | 2792 | ` * value current(array $array)` |
|       - | 2793 | ` *  Return the current element in an array.` |
|       - | 2794 | ` * Parameter` |
|       - | 2795 | ` *  $input: The input array.` |
|       - | 2796 | ` * Return` |
|       - | 2797 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 2798 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2799 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2800 | ` *  is empty, current() returns FALSE.` |
|       - | 2801 | ` */` |
|      10 | 2802 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2803 |  |
|      11 | 2804 | `	if( nArg < 1 ){` |
|       - | 2805 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2806 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2807 | `		return PH7_OK;` |
|       - | 2808 | `	}` |
|       - | 2809 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 2810 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2811 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2812 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2813 | `		return PH7_OK;` |
|       - | 2814 | `	}` |
|      11 | 2815 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 2816 | `	return PH7_OK;` |
|       6 | 2817 |  |
|       - | 2818 | `/*` |
|       - | 2819 | ` * value next(array $input)` |
|       - | 2820 | ` *  Advance the internal array pointer of an array.` |
|       - | 2821 | ` * Parameter` |
|       - | 2822 | ` *  $input: The input array.` |
|       - | 2823 | ` * Return` |
|       - | 2824 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 2825 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 2826 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 2827 | ` */` |
|       6 | 2828 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2829 |  |
|       7 | 2830 | `	if( nArg < 1 ){` |
|       - | 2831 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2832 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2833 | `		return PH7_OK;` |
|       - | 2834 | `	}` |
|       - | 2835 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 2836 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2837 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2838 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2839 | `		return PH7_OK;` |
|       - | 2840 | `	}` |
|       7 | 2841 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 2842 | `	return PH7_OK;` |
|       4 | 2843 |  |
|       - | 2844 | `/*` |
|       - | 2845 | ` * value prev(array $input)` |
|       - | 2846 | ` *  Rewind the internal array pointer.` |
|       - | 2847 | ` * Parameter` |
|       - | 2848 | ` *  $input: The input array.` |
|       - | 2849 | ` * Return` |
|       - | 2850 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 2851 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 2852 | ` *  elements.` |
|       - | 2853 | ` */` |
|       2 | 2854 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2855 |  |
|       3 | 2856 | `	if( nArg < 1 ){` |
|       - | 2857 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2858 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2859 | `		return PH7_OK;` |
|       - | 2860 | `	}` |
|       - | 2861 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2862 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2863 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2864 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2865 | `		return PH7_OK;` |
|       - | 2866 | `	}` |
|       3 | 2867 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 2868 | `	return PH7_OK;` |
|       2 | 2869 |  |
|       - | 2870 | `/*` |
|       - | 2871 | ` * value end(array $input)` |
|       - | 2872 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 2873 | ` * Parameter` |
|       - | 2874 | ` *  $input: The input array.` |
|       - | 2875 | ` * Return` |
|       - | 2876 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 2877 | ` */` |
|       2 | 2878 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2879 |  |
|       - | 2880 | `	ph7_hashmap *pMap;` |
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
|       - | 2892 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2893 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2894 | `	/* Point to the last node */` |
|       3 | 2895 | `	pMap->pCur = pMap->pLast;` |
|       - | 2896 | `	/* Return the last node value */` |
|       3 | 2897 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 2898 | `	return PH7_OK;` |
|       2 | 2899 |  |
|       - | 2900 | `/*` |
|       - | 2901 | ` * value reset(array $array )` |
|       - | 2902 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 2903 | ` * Parameter` |
|       - | 2904 | ` *  $input: The input array.` |
|       - | 2905 | ` * Return` |
|       - | 2906 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 2907 | ` */` |
|       4 | 2908 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2909 |  |
|       - | 2910 | `	ph7_hashmap *pMap;` |
|       5 | 2911 | `	if( nArg < 1 ){` |
|       - | 2912 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2913 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2914 | `		return PH7_OK;` |
|       - | 2915 | `	}` |
|       - | 2916 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2917 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2918 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2919 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2920 | `		return PH7_OK;` |
|       - | 2921 | `	}` |
|       - | 2922 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2923 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2924 | `	/* Point to the first node */` |
|       5 | 2925 | `	pMap->pCur = pMap->pFirst;` |
|       - | 2926 | `	/* Return the last node value if available */` |
|       5 | 2927 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 2928 | `	return PH7_OK;` |
|       3 | 2929 |  |
|       - | 2930 | `/*` |
|       - | 2931 | ` * value key(array $array)` |
|       - | 2932 | ` *   Fetch a key from an array` |
|       - | 2933 | ` * Parameter` |
|       - | 2934 | ` *  $input` |
|       - | 2935 | ` *   The input array.` |
|       - | 2936 | ` * Return` |
|       - | 2937 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 2938 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2939 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2940 | ` *  is empty, key() returns NULL.` |
|       - | 2941 | ` */` |
|       4 | 2942 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2943 |  |
|       - | 2944 | `	ph7_hashmap_node *pCur;` |
|       - | 2945 | `	ph7_hashmap *pMap;` |
|       5 | 2946 | `	if( nArg < 1 ){` |
|       - | 2947 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 2948 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2949 | `		return PH7_OK;` |
|       - | 2950 | `	}` |
|       - | 2951 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2952 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2953 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 2954 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2955 | `		return PH7_OK;` |
|       - | 2956 | `	}` |
|       5 | 2957 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2958 | `	pCur = pMap->pCur;` |
|       5 | 2959 | `	if( pCur == 0 ){` |
|       - | 2960 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 2961 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2962 | `		return PH7_OK;` |
|       - | 2963 | `	}` |
|       5 | 2964 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 2965 | `		/* Key is integer */` |
|     ! 0 | 2966 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 2967 | `	}else{` |
|       - | 2968 | `		/* Key is blob */` |
|       7 | 2969 | `		ph7_result_string(pCtx,` |
|       4 | 2970 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 2971 | `	}` |
|       5 | 2972 | `	return PH7_OK;` |
|       3 | 2973 |  |
|       - | 2974 | `/*` |
|       - | 2975 | ` * array each(array $input)` |
|       - | 2976 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 2977 | ` * Parameter` |
|       - | 2978 | ` *  $input` |
|       - | 2979 | ` *    The input array.` |
|       - | 2980 | ` * Return` |
|       - | 2981 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 2982 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 2983 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 2984 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 2985 | ` *  each() returns FALSE.` |
|       - | 2986 | ` */` |
|      22 | 2987 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2988 |  |
|       - | 2989 | `	ph7_hashmap_node *pCur;` |
|       - | 2990 | `	ph7_hashmap *pMap;` |
|       - | 2991 | `	ph7_value *pArray;` |
|       - | 2992 | `	ph7_value *pVal;` |
|       - | 2993 | `	ph7_value sKey;` |
|      23 | 2994 | `	if( nArg < 1 ){` |
|       - | 2995 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2996 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2997 | `		return PH7_OK;` |
|       - | 2998 | `	}` |
|       - | 2999 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3000 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3001 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3002 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3003 | `		return PH7_OK;` |
|       - | 3004 | `	}` |
|       - | 3005 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3006 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3007 | `	if( pMap->pCur == 0 ){` |
|       - | 3008 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3009 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3010 | `		return PH7_OK;` |
|       - | 3011 | `	}` |
|      15 | 3012 | `	pCur = pMap->pCur;` |
|       - | 3013 | `	/* Create a new array */` |
|      15 | 3014 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3015 | `	if( pArray == 0 ){` |
|     ! 0 | 3016 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3017 | `		return PH7_OK;` |
|       - | 3018 | `	}` |
|      15 | 3019 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3020 | `	/* Insert the current value */` |
|      15 | 3021 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3022 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3023 | `	/* Make the key */` |
|      15 | 3024 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3025 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3026 | `	}else{` |
|       9 | 3027 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3028 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3029 | `	}` |
|       - | 3030 | `	/* Insert the current key */` |
|      15 | 3031 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3032 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3033 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3034 | `	/* Advance the cursor */` |
|      15 | 3035 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3036 | `	/* Return the current entry */` |
|      15 | 3037 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3038 | `	return PH7_OK;` |
|      12 | 3039 |  |
|       - | 3040 | `/*` |
|       - | 3041 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3042 | ` *  Create an array containing a range of elements` |
|       - | 3043 | ` * Parameter` |
|       - | 3044 | ` *  start` |
|       - | 3045 | ` *   First value of the sequence.` |
|       - | 3046 | ` *  limit` |
|       - | 3047 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3048 | ` *  step` |
|       - | 3049 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3050 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3051 | ` * Return` |
|       - | 3052 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3053 | ` * NOTE:` |
|       - | 3054 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3055 | ` */` |
|       2 | 3056 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3057 |  |
|       - | 3058 | `	ph7_value *pValue,*pArray;` |
|       - | 3059 | `	sxi64 iOfft,iLimit;` |
|       3 | 3060 | `	int iStep = 1;` |
|       - | 3061 |  |
|       3 | 3062 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3063 | `	if( nArg > 0 ){` |
|       - | 3064 | `		/* Extract the offset */` |
|       3 | 3065 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3066 | `		if( nArg > 1 ){` |
|       - | 3067 | `			/* Extract the limit */` |
|       3 | 3068 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3069 | `			if( nArg > 2 ){` |
|       - | 3070 | `				/* Extract the increment */` |
|       3 | 3071 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3072 | `				if( iStep < 1 ){` |
|       - | 3073 | `					/* Only positive number are allowed */` |
|       3 | 3074 | `					iStep = 1;` |
|       1 | 3075 | `				}` |
|       1 | 3076 | `			}` |
|       1 | 3077 | `		}` |
|       1 | 3078 | `	}` |
|       - | 3079 | `	/* Element container */` |
|       3 | 3080 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3081 | `	/* Create the new array */` |
|       3 | 3082 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3083 | `	if( pArray == 0 ){` |
|     ! 0 | 3084 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3085 | `		return PH7_OK;` |
|       - | 3086 | `	}` |
|       - | 3087 | `	/* Start filling */` |
|       3 | 3088 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3089 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3090 | `		/* Perform the insertion */` |
|     ! 0 | 3091 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3092 | `		/* Increment */` |
|     ! 0 | 3093 | `		iOfft += iStep;` |
|     ! 0 | 3094 | `	}` |
|       - | 3095 | `	/* Return the new array */` |
|       3 | 3096 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3097 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3098 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3099 | `	 */` |
|       3 | 3100 | `	return PH7_OK;` |
|       2 | 3101 |  |
|       - | 3102 | `/*` |
|       - | 3103 | ` * array array_values(array $input)` |
|       - | 3104 | ` *   Returns all the values from the input array and indexes numerically the array.` |
|       - | 3105 | ` * Parameters` |
|       - | 3106 | ` *   input: The input array.` |
|       - | 3107 | ` * Return` |
|       - | 3108 | ` *  An indexed array of values or NULL on failure.` |
|       - | 3109 | ` */` |
|      24 | 3110 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3111 |  |
|       - | 3112 | `	ph7_hashmap_node *pNode;` |
|       - | 3113 | `	ph7_hashmap *pMap;` |
|       - | 3114 | `	ph7_value *pArray;` |
|       - | 3115 | `	ph7_value *pObj;` |
|       - | 3116 | `	sxu32 n;` |
|      25 | 3117 | `	if( nArg < 1 ){` |
|       - | 3118 | `		/* Missing arguments,return NULL */` |
|       3 | 3119 | `		ph7_result_null(pCtx);` |
|       3 | 3120 | `		return PH7_OK;` |
|       - | 3121 | `	}` |
|       - | 3122 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3123 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3124 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3125 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3126 | `		return PH7_OK;` |
|       - | 3127 | `	}` |
|       - | 3128 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3129 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3130 | `	/* Create a new array */` |
|      23 | 3131 | `	pArray = ph7_context_new_array(pCtx);` |
|      23 | 3132 | `	if( pArray == 0 ){` |
|     ! 0 | 3133 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3134 | `		return PH7_OK;` |
|       - | 3135 | `	}` |
|       - | 3136 | `	/* Perform the requested operation */` |
|      23 | 3137 | `	pNode = pMap->pFirst;` |
|      81 | 3138 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3139 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3140 | `		if( pObj ){` |
|       - | 3141 | `			/* perform the insertion */` |
|      59 | 3142 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3143 | `		}` |
|       - | 3144 | `		/* Point to the next entry */` |
|      59 | 3145 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3146 | `	}` |
|       - | 3147 | `	/* return the new array */` |
|      23 | 3148 | `	ph7_result_value(pCtx,pArray);` |
|      23 | 3149 | `	return PH7_OK;` |
|      13 | 3150 |  |
|       - | 3151 | `/*` |
|       - | 3152 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3153 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3154 | ` * Parameters` |
|       - | 3155 | ` *  $input` |
|       - | 3156 | ` *   An array containing keys to return.` |
|       - | 3157 | ` * $search_value` |
|       - | 3158 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3159 | ` * $strict` |
|       - | 3160 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3161 | ` * Return` |
|       - | 3162 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3163 | ` */` |
|     104 | 3164 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3165 |  |
|       - | 3166 | `	ph7_hashmap_node *pNode;` |
|       - | 3167 | `	ph7_hashmap *pMap;` |
|       - | 3168 | `	ph7_value *pArray;` |
|       - | 3169 | `	ph7_value sObj;` |
|       - | 3170 | `	ph7_value sVal;` |
|       - | 3171 | `	SyString sKey;` |
|       - | 3172 | `	int bStrict;` |
|       - | 3173 | `	sxi32 rc;` |
|       - | 3174 | `	sxu32 n;` |
|     105 | 3175 | `	if( nArg < 1 ){` |
|       - | 3176 | `		/* Missing arguments,return NULL */` |
|       3 | 3177 | `		ph7_result_null(pCtx);` |
|       3 | 3178 | `		return PH7_OK;` |
|       - | 3179 | `	}` |
|       - | 3180 | `	/* Make sure we are dealing with a valid hashmap */` |
|     103 | 3181 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3182 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3183 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3184 | `		return PH7_OK;` |
|       - | 3185 | `	}` |
|       - | 3186 | `	/* Point to the internal representation of the input hashmap */` |
|     103 | 3187 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3188 | `	/* Create a new array */` |
|     103 | 3189 | `	pArray = ph7_context_new_array(pCtx);` |
|     103 | 3190 | `	if( pArray == 0 ){` |
|     ! 0 | 3191 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3192 | `		return PH7_OK;` |
|       - | 3193 | `	}` |
|     103 | 3194 | `	bStrict = FALSE;` |
|     103 | 3195 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|     ! 0 | 3196 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|     ! 0 | 3197 | `	}` |
|       - | 3198 | `	/* Perform the requested operation */` |
|     103 | 3199 | `	pNode = pMap->pFirst;` |
|     103 | 3200 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     507 | 3201 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     405 | 3202 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|      99 | 3203 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      50 | 3204 | `		}else{` |
|     307 | 3205 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     307 | 3206 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3207 | `		}` |
|     405 | 3208 | `		rc = 0;` |
|     405 | 3209 | `		if( nArg > 1 ){` |
|     ! 0 | 3210 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|     ! 0 | 3211 | `			if( pValue ){` |
|     ! 0 | 3212 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3213 | `				/* Filter key */` |
|     ! 0 | 3214 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|     ! 0 | 3215 | `				PH7_MemObjRelease(pValue);` |
|     ! 0 | 3216 | `			}` |
|     ! 0 | 3217 | `		}` |
|     405 | 3218 | `		if( rc == 0 ){` |
|       - | 3219 | `			/* Perform the insertion */` |
|     405 | 3220 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     202 | 3221 | `		}` |
|     405 | 3222 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3223 | `		/* Point to the next entry */` |
|     405 | 3224 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     203 | 3225 | `	}` |
|       - | 3226 | `	/* return the new array */` |
|     103 | 3227 | `	ph7_result_value(pCtx,pArray);` |
|     103 | 3228 | `	return PH7_OK;` |
|      53 | 3229 |  |
|       - | 3230 | `/*` |
|       - | 3231 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3232 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3233 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3234 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3235 | ` * Parameters` |
|       - | 3236 | ` *  $arr1` |
|       - | 3237 | ` *   First array` |
|       - | 3238 | ` *  $arr2` |
|       - | 3239 | ` *   Second array` |
|       - | 3240 | ` * Return` |
|       - | 3241 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3242 | ` * Note` |
|       - | 3243 | ` *  This function is a symisc eXtension.` |
|       - | 3244 | ` */` |
|       4 | 3245 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3246 |  |
|       - | 3247 | `	ph7_hashmap *p1,*p2;` |
|       - | 3248 | `	int rc;` |
|       5 | 3249 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3250 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3251 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3252 | `		return PH7_OK;` |
|       - | 3253 | `	}` |
|       - | 3254 | `	/* Point to the hashmaps */` |
|       5 | 3255 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3256 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3257 | `	rc = (p1 == p2);` |
|       - | 3258 | `	/* Same instance? */` |
|       5 | 3259 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3260 | `	return PH7_OK;` |
|       3 | 3261 |  |
|       - | 3262 | `/*` |
|       - | 3263 | ` * array array_merge(array $array1,...)` |
|       - | 3264 | ` *  Merge one or more arrays.` |
|       - | 3265 | ` * Parameters` |
|       - | 3266 | ` *  $array1` |
|       - | 3267 | ` *    Initial array to merge.` |
|       - | 3268 | ` *  ...` |
|       - | 3269 | ` *   More array to merge.` |
|       - | 3270 | ` * Return` |
|       - | 3271 | ` *  The resulting array.` |
|       - | 3272 | ` */` |
|     802 | 3273 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3274 |  |
|       - | 3275 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3276 | `	ph7_value *pArray;` |
|       - | 3277 | `	int i;` |
|     804 | 3278 | `	if( nArg < 1 ){` |
|       - | 3279 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3280 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3281 | `		return PH7_OK;` |
|       - | 3282 | `	}` |
|       - | 3283 | `	/* Create a new array */` |
|     804 | 3284 | `	pArray = ph7_context_new_array(pCtx);` |
|     804 | 3285 | `	if( pArray == 0 ){` |
|     ! 0 | 3286 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3287 | `		return PH7_OK;` |
|       - | 3288 | `	}` |
|       - | 3289 | `	/* Point to the internal representation of the hashmap */` |
|     804 | 3290 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3291 | `	/* Start merging */` |
|    2408 | 3292 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3293 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1606 | 3294 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3295 | `			/* Insert scalar value */` |
|       5 | 3296 | `			ph7_array_add_elem(pArray,0,apArg[i]);` |
|       3 | 3297 | `		}else{` |
|    1602 | 3298 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3299 | `			/* Merge the two hashmaps */` |
|    1602 | 3300 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3301 | `		}` |
|     804 | 3302 | `	}` |
|       - | 3303 | `	/* Return the freshly created array */` |
|     804 | 3304 | `	ph7_result_value(pCtx,pArray);` |
|     804 | 3305 | `	return PH7_OK;` |
|     403 | 3306 |  |
|       - | 3307 | `/*` |
|       - | 3308 | ` * array array_copy(array $source)` |
|       - | 3309 | ` *  Make a blind copy of the target array.` |
|       - | 3310 | ` * Parameters` |
|       - | 3311 | ` *  $source` |
|       - | 3312 | ` *   Target array` |
|       - | 3313 | ` * Return` |
|       - | 3314 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3315 | ` * Note` |
|       - | 3316 | ` *  This function is a symisc eXtension.` |
|       - | 3317 | ` */` |
|       2 | 3318 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3319 |  |
|       - | 3320 | `	ph7_hashmap *pMap;` |
|       - | 3321 | `	ph7_value *pArray;` |
|       3 | 3322 | `	if( nArg < 1 ){` |
|       - | 3323 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3324 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3325 | `		return PH7_OK;` |
|       - | 3326 | `	}` |
|       - | 3327 | `	/* Create a new array */` |
|       3 | 3328 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3329 | `	if( pArray == 0 ){` |
|     ! 0 | 3330 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3331 | `		return PH7_OK;` |
|       - | 3332 | `	}` |
|       - | 3333 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3334 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3335 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3336 | `		/* Point to the internal representation of the source */` |
|       3 | 3337 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3338 | `		/* Perform the copy */` |
|       3 | 3339 | `		PH7_HashmapDup(pSrc,pMap);` |
|       2 | 3340 | `	}else{` |
|       - | 3341 | `		/* Simple insertion */` |
|     ! 0 | 3342 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3343 | `	}` |
|       - | 3344 | `	/* Return the duplicated array */` |
|       3 | 3345 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3346 | `	return PH7_OK;` |
|       2 | 3347 |  |
|       - | 3348 | `/*` |
|       - | 3349 | ` * bool array_erase(array $source)` |
|       - | 3350 | ` *  Remove all elements from a given array.` |
|       - | 3351 | ` * Parameters` |
|       - | 3352 | ` *  $source` |
|       - | 3353 | ` *   Target array` |
|       - | 3354 | ` * Return` |
|       - | 3355 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3356 | ` * Note` |
|       - | 3357 | ` *  This function is a symisc eXtension.` |
|       - | 3358 | ` */` |
|       2 | 3359 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3360 |  |
|       - | 3361 | `	ph7_hashmap *pMap;` |
|       3 | 3362 | `	if( nArg < 1 ){` |
|       - | 3363 | `		/* Missing arguments */` |
|     ! 0 | 3364 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3365 | `		return PH7_OK;` |
|       - | 3366 | `	}` |
|       - | 3367 | `	/* Point to the target hashmap */` |
|       3 | 3368 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3369 | `	/* Erase */` |
|       3 | 3370 | `	PH7_HashmapRelease(pMap,FALSE);` |
|       3 | 3371 | `	return PH7_OK;` |
|       2 | 3372 |  |
|       - | 3373 | `/*` |
|       - | 3374 | ` * array array_slice(array $array,int $offset [,int $length [, bool $preserve_keys = false ]])` |
|       - | 3375 | ` *  Extract a slice of the array.` |
|       - | 3376 | ` * Parameters` |
|       - | 3377 | ` *  $array` |
|       - | 3378 | ` *    The input array.` |
|       - | 3379 | ` * $offset` |
|       - | 3380 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3381 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3382 | ` * $length (optional)` |
|       - | 3383 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3384 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3385 | ` *   elements from the end of the array. If it is omitted, then the sequence will have` |
|       - | 3386 | ` *   everything from offset up until the end of the array.` |
|       - | 3387 | ` * $preserve_keys (optional)` |
|       - | 3388 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3389 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3390 | ` * Return` |
|       - | 3391 | ` *   The new slice.` |
|       - | 3392 | ` */` |
|       8 | 3393 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3394 |  |
|       - | 3395 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3396 | `	ph7_hashmap_node *pCur;` |
|       - | 3397 | `	ph7_value *pArray;` |
|       - | 3398 | `	int iLength,iOfft;` |
|       - | 3399 | `	int bPreserve;` |
|       - | 3400 | `	sxi32 rc;` |
|       9 | 3401 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3402 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3403 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3404 | `		return PH7_OK;` |
|       - | 3405 | `	}` |
|       - | 3406 | `	/* Point the internal representation of the target array */` |
|       9 | 3407 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 3408 | `	bPreserve = FALSE;` |
|       - | 3409 | `	/* Get the offset */` |
|       9 | 3410 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       9 | 3411 | `	if( iOfft < 0 ){` |
|       3 | 3412 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       1 | 3413 | `	}` |
|       9 | 3414 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3415 | `		/* Invalid offset,return the last entry */` |
|     ! 0 | 3416 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3417 | `	}` |
|       - | 3418 | `	/* Get the length */` |
|       9 | 3419 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       9 | 3420 | `	if( nArg > 2 ){` |
|       7 | 3421 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       7 | 3422 | `		if( iLength < 0 ){` |
|     ! 0 | 3423 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3424 | `		}` |
|       7 | 3425 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3426 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3427 | `		}` |
|       7 | 3428 | `		if( nArg > 3 && ph7_value_is_bool(apArg[3]) ){` |
|       3 | 3429 | `			bPreserve = ph7_value_to_bool(apArg[3]);` |
|       1 | 3430 | `		}` |
|       3 | 3431 | `	}` |
|       - | 3432 | `	/* Create a new array */` |
|       9 | 3433 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 3434 | `	if( pArray == 0 ){` |
|     ! 0 | 3435 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3436 | `		return PH7_OK;` |
|       - | 3437 | `	}` |
|       9 | 3438 | `	if( iLength < 1 ){` |
|       - | 3439 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3440 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3441 | `		return PH7_OK;` |
|       - | 3442 | `	}` |
|       - | 3443 | `	/* Point to the desired entry */` |
|       9 | 3444 | `	pCur = pSrc->pFirst;` |
|       9 | 3445 | `	for(;;){` |
|      19 | 3446 | `		if( iOfft < 1 ){` |
|       9 | 3447 | `			break;` |
|       - | 3448 | `		}` |
|       - | 3449 | `		/* Point to the next entry */` |
|      11 | 3450 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      11 | 3451 | `		iOfft--;` |
|       1 | 3452 | `	}` |
|       - | 3453 | `	/* Point to the internal representation of the hashmap */` |
|       9 | 3454 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      12 | 3455 | `	for(;;){` |
|      25 | 3456 | `		if( iLength < 1 ){` |
|       9 | 3457 | `			break;` |
|       - | 3458 | `		}` |
|      17 | 3459 | `		rc = HashmapInsertNode(pMap,pCur,bPreserve);` |
|      17 | 3460 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3461 | `			break;` |
|       - | 3462 | `		}` |
|       - | 3463 | `		/* Point to the next entry */` |
|      17 | 3464 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      17 | 3465 | `		iLength--;` |
|       1 | 3466 | `	}` |
|       - | 3467 | `	/* Return the freshly created array */` |
|       9 | 3468 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 3469 | `	return PH7_OK;` |
|       5 | 3470 |  |
|       - | 3471 | `/*` |
|       - | 3472 | ` * array array_splice(array $array,int $offset [,int $length [,value $replacement ]])` |
|       - | 3473 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3474 | ` * Parameters` |
|       - | 3475 | ` *  $array` |
|       - | 3476 | ` *    The input array.` |
|       - | 3477 | ` * $offset` |
|       - | 3478 | ` *    If offset is positive then the start of removed portion is at that offset from` |
|       - | 3479 | ` *    the beginning of the input array. If offset is negative then it starts that far` |
|       - | 3480 | ` *    from the end of the input array.` |
|       - | 3481 | ` * $length (optional)` |
|       - | 3482 | ` *    If length is omitted, removes everything from offset to the end of the array.` |
|       - | 3483 | ` *    If length is specified and is positive, then that many elements will be removed.` |
|       - | 3484 | ` *    If length is specified and is negative then the end of the removed portion will` |
|       - | 3485 | ` *    be that many elements from the end of the array.` |
|       - | 3486 | ` * $replacement (optional)` |
|       - | 3487 | ` *  If replacement array is specified, then the removed elements are replaced` |
|       - | 3488 | ` *  with elements from this array.` |
|       - | 3489 | ` *  If offset and length are such that nothing is removed, then the elements` |
|       - | 3490 | ` *  from the replacement array are inserted in the place specified by the offset.` |
|       - | 3491 | ` *  Note that keys in replacement array are not preserved.` |
|       - | 3492 | ` *  If replacement is just one element it is not necessary to put array() around` |
|       - | 3493 | ` *  it, unless the element is an array itself, an object or NULL.` |
|       - | 3494 | ` * Return` |
|       - | 3495 | ` *   A new array consisting of the extracted elements.` |
|       - | 3496 | ` */` |
|       2 | 3497 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3498 |  |
|       - | 3499 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode;` |
|       - | 3500 | `	ph7_value *pArray,*pRvalue,*pOld;` |
|       - | 3501 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3502 | `	int iLength,iOfft;` |
|       - | 3503 | `	sxi32 rc;` |
|       3 | 3504 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3505 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3506 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3507 | `		return PH7_OK;` |
|       - | 3508 | `	}` |
|       - | 3509 | `	/* Point the internal representation of the target array */` |
|       3 | 3510 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3511 | `	/* Get the offset */` |
|       3 | 3512 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       3 | 3513 | `	if( iOfft < 0 ){` |
|     ! 0 | 3514 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|     ! 0 | 3515 | `	}` |
|       3 | 3516 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3517 | `		/* Invalid offset,remove the last entry */` |
|     ! 0 | 3518 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3519 | `	}` |
|       - | 3520 | `	/* Get the length */` |
|       3 | 3521 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       3 | 3522 | `	if( nArg > 2 ){` |
|       3 | 3523 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       3 | 3524 | `		if( iLength < 0 ){` |
|     ! 0 | 3525 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3526 | `		}` |
|       3 | 3527 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3528 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3529 | `		}` |
|       1 | 3530 | `	}` |
|       - | 3531 | `	/* Create a new array */` |
|       3 | 3532 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3533 | `	if( pArray == 0 ){` |
|     ! 0 | 3534 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3535 | `		return PH7_OK;` |
|       - | 3536 | `	}` |
|       3 | 3537 | `	if( iLength < 1 ){` |
|       - | 3538 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3539 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3540 | `		return PH7_OK;` |
|       - | 3541 | `	}` |
|       - | 3542 | `	/* Point to the desired entry */` |
|       3 | 3543 | `	pCur = pSrc->pFirst;` |
|       2 | 3544 | `	for(;;){` |
|       5 | 3545 | `		if( iOfft < 1 ){` |
|       3 | 3546 | `			break;` |
|       - | 3547 | `		}` |
|       - | 3548 | `		/* Point to the next entry */` |
|       3 | 3549 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       3 | 3550 | `		iOfft--;` |
|       1 | 3551 | `	}` |
|       3 | 3552 | `	pRep = 0;` |
|       3 | 3553 | `	if( nArg > 3 ){` |
|       3 | 3554 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3555 | `			/* Perform an array cast */` |
|     ! 0 | 3556 | `			PH7_MemObjToHashmap(apArg[3]);` |
|     ! 0 | 3557 | `			if(ph7_value_is_array(apArg[3])){` |
|     ! 0 | 3558 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|     ! 0 | 3559 | `			}` |
|     ! 0 | 3560 | `		}else{` |
|       3 | 3561 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3562 | `		}` |
|       3 | 3563 | `		if( pRep ){` |
|       - | 3564 | `			/* Reset the loop cursor */` |
|       3 | 3565 | `			pRep->pCur = pRep->pFirst;` |
|       1 | 3566 | `		}` |
|       1 | 3567 | `	}` |
|       - | 3568 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3569 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3570 | `	for(;;){` |
|       7 | 3571 | `		if( iLength < 1 ){` |
|       3 | 3572 | `			break;` |
|       - | 3573 | `		}` |
|       5 | 3574 | `		pPrev = pCur->pPrev;` |
|       5 | 3575 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|       5 | 3576 | `		if( pRep && (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|       - | 3577 | `			/* Extract node value */` |
|       5 | 3578 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|       - | 3579 | `			/* Replace the old node */` |
|       5 | 3580 | `			pOld = HashmapExtractNodeValue(pCur);` |
|       5 | 3581 | `			if( pRvalue && pOld ){` |
|       5 | 3582 | `				PH7_MemObjStore(pRvalue,pOld);` |
|       2 | 3583 | `			}` |
|       3 | 3584 | `		}else{` |
|       - | 3585 | `			/* Unlink the node from the source hashmap */` |
|     ! 0 | 3586 | `			PH7_HashmapUnlinkNode(pCur,TRUE);` |
|       - | 3587 | `		}` |
|       5 | 3588 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3589 | `			break;` |
|       - | 3590 | `		}` |
|       - | 3591 | `		/* Point to the next entry */` |
|       5 | 3592 | `		pCur = pPrev; /* Reverse link */` |
|       5 | 3593 | `		iLength--;` |
|       1 | 3594 | `	}` |
|       3 | 3595 | `	if( pRep ){` |
|       3 | 3596 | `		while((pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|     ! 0 | 3597 | `			HashmapInsertNode(pSrc,pRnode,FALSE);` |
|     ! 0 | 3598 | `		}` |
|       1 | 3599 | `	}` |
|       - | 3600 | `	/* Return the freshly created array */` |
|       3 | 3601 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3602 | `	return PH7_OK;` |
|       2 | 3603 |  |
|       - | 3604 | `/*` |
|       - | 3605 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3606 | ` *  Checks if a value exists in an array.` |
|       - | 3607 | ` * Parameters` |
|       - | 3608 | ` *  $needle` |
|       - | 3609 | ` *   The searched value.` |
|       - | 3610 | ` *   Note:` |
|       - | 3611 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3612 | ` * $haystack` |
|       - | 3613 | ` *  The target array.` |
|       - | 3614 | ` * $strict` |
|       - | 3615 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3616 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3617 | ` */` |
|   18994 | 3618 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3619 |  |
|       - | 3620 | `	ph7_value *pNeedle;` |
|       - | 3621 | `	int bStrict;` |
|       - | 3622 | `	int rc;` |
|   18996 | 3623 | `	if( nArg < 2 ){` |
|       - | 3624 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3625 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3626 | `		return PH7_OK;` |
|       - | 3627 | `	}` |
|   18996 | 3628 | `	pNeedle = apArg[0];` |
|   18996 | 3629 | `	bStrict = 0;` |
|   18996 | 3630 | `	if( nArg > 2 ){` |
|       5 | 3631 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3632 | `	}` |
|   18996 | 3633 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3634 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3635 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3636 | `		/* Set the comparison result */` |
|     ! 0 | 3637 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3638 | `		return PH7_OK;` |
|       - | 3639 | `	}` |
|       - | 3640 | `	/* Perform the lookup */` |
|   18996 | 3641 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3642 | `	/* Lookup result */` |
|   18996 | 3643 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   18996 | 3644 | `	return PH7_OK;` |
|    9499 | 3645 |  |
|       - | 3646 | `/*` |
|       - | 3647 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3648 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3649 | ` * Parameters` |
|       - | 3650 | ` * $needle` |
|       - | 3651 | ` *   The searched value.` |
|       - | 3652 | ` * $haystack` |
|       - | 3653 | ` *   The array.` |
|       - | 3654 | ` * $strict` |
|       - | 3655 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3656 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3657 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3658 | ` * Return` |
|       - | 3659 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3660 | ` */` |
|      28 | 3661 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3662 |  |
|       - | 3663 | `	ph7_hashmap_node *pEntry;` |
|       - | 3664 | `	ph7_value *pVal,sNeedle;` |
|       - | 3665 | `	ph7_hashmap *pMap;` |
|       - | 3666 | `	ph7_value sVal;` |
|       - | 3667 | `	int bStrict;` |
|       - | 3668 | `	sxu32 n;` |
|       - | 3669 | `	int rc;` |
|      30 | 3670 | `	if( nArg < 2 ){` |
|       - | 3671 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3672 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3673 | `			"ArgumentCountError",` |
|       - | 3674 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 3675 | `			nArg` |
|       - | 3676 | `			);` |
|       - | 3677 | `	}` |
|      26 | 3678 | `	bStrict = FALSE;` |
|      26 | 3679 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3680 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3681 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3682 | `			"TypeError",` |
|       - | 3683 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 3684 | `			ph7_type_name(apArg[1])` |
|       - | 3685 | `			);` |
|       - | 3686 | `	}` |
|      24 | 3687 | `	if( nArg > 2 ){` |
|       - | 3688 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 3689 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3690 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3691 | `				"TypeError",` |
|       - | 3692 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3693 | `				ph7_type_name(apArg[2])` |
|       - | 3694 | `				);` |
|       - | 3695 | `		}` |
|       9 | 3696 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 3697 | `	}` |
|       - | 3698 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 3699 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3700 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 3701 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 3702 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 3703 | `	pEntry = pMap->pFirst;` |
|      21 | 3704 | `	n = pMap->nEntry;` |
|      23 | 3705 | `	for(;;){` |
|      47 | 3706 | `		if( !n ){` |
|       9 | 3707 | `			break;` |
|       - | 3708 | `		}` |
|       - | 3709 | `		/* Extract node value */` |
|      39 | 3710 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 3711 | `		if( pVal ){` |
|       - | 3712 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3713 | `			 * can change their type.` |
|       - | 3714 | `			 */` |
|      39 | 3715 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 3716 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 3717 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 3718 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 3719 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 3720 | `			if( rc == 0 ){` |
|       - | 3721 | `				/* Match found,return key */` |
|      13 | 3722 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3723 | `					/* INT key */` |
|       7 | 3724 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 3725 | `				}else{` |
|       7 | 3726 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3727 | `					/* Blob key */` |
|       7 | 3728 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3729 | `				}` |
|      13 | 3730 | `				return PH7_OK;` |
|       - | 3731 | `			}` |
|      13 | 3732 | `		}` |
|       - | 3733 | `		/* Point to the next entry */` |
|      27 | 3734 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 3735 | `		n--;` |
|       1 | 3736 | `	}` |
|       - | 3737 | `	/* No such value,return FALSE */` |
|       9 | 3738 | `	ph7_result_bool(pCtx,0);` |
|       9 | 3739 | `	return PH7_OK;` |
|      16 | 3740 |  |
|       - | 3741 | `/*` |
|       - | 3742 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3743 | ` *  Computes the difference of arrays.` |
|       - | 3744 | ` * Parameters` |
|       - | 3745 | ` *  $array1` |
|       - | 3746 | ` *    The array to compare from` |
|       - | 3747 | ` *  $array2` |
|       - | 3748 | ` *    An array to compare against` |
|       - | 3749 | ` *  $...` |
|       - | 3750 | ` *   More arrays to compare against` |
|       - | 3751 | ` * Return` |
|       - | 3752 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3753 | ` *  are not present in any of the other arrays.` |
|       - | 3754 | ` */` |
|      10 | 3755 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3756 |  |
|       - | 3757 | `	ph7_hashmap_node *pEntry;` |
|       - | 3758 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3759 | `	ph7_value *pArray;` |
|       - | 3760 | `	ph7_value *pVal;` |
|       - | 3761 | `	sxi32 rc;` |
|       - | 3762 | `	sxu32 n;` |
|       - | 3763 | `	int i;` |
|       - | 3764 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3765 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3766 | `	 * debugging difficult. */` |
|      12 | 3767 | `	if( nArg < 1 ){` |
|       4 | 3768 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3769 | `			"ArgumentCountError",` |
|       - | 3770 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3771 | `			nArg` |
|       - | 3772 | `			);` |
|       - | 3773 | `	}` |
|      10 | 3774 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3775 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3776 | `			"TypeError",` |
|       - | 3777 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3778 | `			ph7_type_name(apArg[0])` |
|       - | 3779 | `			);` |
|       - | 3780 | `	}` |
|      14 | 3781 | `	for(i = 1 ; i < nArg ; i++){` |
|      10 | 3782 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3783 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3784 | `				"TypeError",` |
|       - | 3785 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3786 | `				i + 1,` |
|       2 | 3787 | `				ph7_type_name(apArg[i])` |
|       - | 3788 | `				);` |
|       - | 3789 | `		}` |
|       4 | 3790 | `	}` |
|       5 | 3791 | `	if( nArg == 1 ){` |
|       - | 3792 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3793 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3794 | `		return PH7_OK;` |
|       - | 3795 | `	}` |
|       - | 3796 | `	/* Create a new array */` |
|       5 | 3797 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 3798 | `	if( pArray == 0 ){` |
|     ! 0 | 3799 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3800 | `		return PH7_OK;` |
|       - | 3801 | `	}` |
|       - | 3802 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 3803 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3804 | `	/* Perform the diff */` |
|       5 | 3805 | `	pEntry = pSrc->pFirst;` |
|       5 | 3806 | `	n = pSrc->nEntry;` |
|       8 | 3807 | `	for(;;){` |
|      17 | 3808 | `		if( n < 1 ){` |
|       5 | 3809 | `			break;` |
|       - | 3810 | `		}` |
|       - | 3811 | `		/* Extract the node value */` |
|      13 | 3812 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      13 | 3813 | `		if( pVal ){` |
|      23 | 3814 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      17 | 3815 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3816 | `					/* ignore */` |
|     ! 0 | 3817 | `					continue;` |
|       - | 3818 | `				}` |
|       - | 3819 | `				/* Point to the internal representation of the hashmap */` |
|      17 | 3820 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3821 | `				/* Perform the lookup */` |
|      17 | 3822 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      17 | 3823 | `				if( rc == SXRET_OK ){` |
|       - | 3824 | `					/* Value exist */` |
|       7 | 3825 | `					break;` |
|       - | 3826 | `				}` |
|       6 | 3827 | `			}` |
|      13 | 3828 | `			if( i >= nArg ){` |
|       - | 3829 | `				/* Perform the insertion */` |
|       7 | 3830 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 3831 | `			}` |
|       6 | 3832 | `		}` |
|       - | 3833 | `		/* Point to the next entry */` |
|      13 | 3834 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 3835 | `		n--;` |
|       1 | 3836 | `	}` |
|       - | 3837 | `	/* Return the freshly created array */` |
|       5 | 3838 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 3839 | `	return PH7_OK;` |
|       7 | 3840 |  |
|       - | 3841 | `/*` |
|       - | 3842 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 3843 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 3844 | ` * Parameters` |
|       - | 3845 | ` *  $array1` |
|       - | 3846 | ` *    The array to compare from` |
|       - | 3847 | ` *  $array2` |
|       - | 3848 | ` *    An array to compare against` |
|       - | 3849 | ` *  $...` |
|       - | 3850 | ` *   More arrays to compare against.` |
|       - | 3851 | ` * $callback` |
|       - | 3852 | ` *  The callback comparison function.` |
|       - | 3853 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3854 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3855 | ` *  than the second.` |
|       - | 3856 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3857 | ` * Return` |
|       - | 3858 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3859 | ` *  are not present in any of the other arrays.` |
|       - | 3860 | ` */` |
|       2 | 3861 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3862 |  |
|       - | 3863 | `	ph7_hashmap_node *pEntry;` |
|       - | 3864 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3865 | `	ph7_value *pCallback;` |
|       - | 3866 | `	ph7_value *pArray;` |
|       - | 3867 | `	ph7_value *pVal;` |
|       - | 3868 | `	sxi32 rc;` |
|       - | 3869 | `	sxu32 n;` |
|       - | 3870 | `	int i;` |
|       3 | 3871 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3872 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3873 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3874 | `		return PH7_OK;` |
|       - | 3875 | `	}` |
|       - | 3876 | `	/* Point to the callback */` |
|       3 | 3877 | `	pCallback = apArg[nArg - 1];` |
|       3 | 3878 | `	if( nArg == 2 ){` |
|       - | 3879 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3880 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3881 | `		return PH7_OK;` |
|       - | 3882 | `	}` |
|       - | 3883 | `	/* Create a new array */` |
|       3 | 3884 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3885 | `	if( pArray == 0 ){` |
|     ! 0 | 3886 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3887 | `		return PH7_OK;` |
|       - | 3888 | `	}` |
|       - | 3889 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 3890 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3891 | `	/* Perform the diff */` |
|       3 | 3892 | `	pEntry = pSrc->pFirst;` |
|       3 | 3893 | `	n = pSrc->nEntry;` |
|       4 | 3894 | `	for(;;){` |
|       9 | 3895 | `		if( n < 1 ){` |
|       3 | 3896 | `			break;` |
|       - | 3897 | `		}` |
|       - | 3898 | `		/* Extract the node value */` |
|       7 | 3899 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 3900 | `		if( pVal ){` |
|      11 | 3901 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 3902 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3903 | `					/* ignore */` |
|     ! 0 | 3904 | `					continue;` |
|       - | 3905 | `				}` |
|       - | 3906 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 3907 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3908 | `				/* Perform the lookup */` |
|       7 | 3909 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 3910 | `				if( rc == SXRET_OK ){` |
|       - | 3911 | `					/* Value exist */` |
|       3 | 3912 | `					break;` |
|       - | 3913 | `				}` |
|       3 | 3914 | `			}` |
|       7 | 3915 | `			if( i >= (nArg - 1)){` |
|       - | 3916 | `				/* Perform the insertion */` |
|       5 | 3917 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 3918 | `			}` |
|       3 | 3919 | `		}` |
|       - | 3920 | `		/* Point to the next entry */` |
|       7 | 3921 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 3922 | `		n--;` |
|       1 | 3923 | `	}` |
|       - | 3924 | `	/* Return the freshly created array */` |
|       3 | 3925 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3926 | `	return PH7_OK;` |
|       2 | 3927 |  |
|       - | 3928 | `/*` |
|       - | 3929 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 3930 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 3931 | ` * Parameters` |
|       - | 3932 | ` *  $array1` |
|       - | 3933 | ` *    The array to compare from` |
|       - | 3934 | ` *  $array2` |
|       - | 3935 | ` *    An array to compare against` |
|       - | 3936 | ` *  $...` |
|       - | 3937 | ` *   More arrays to compare against` |
|       - | 3938 | ` * Return` |
|       - | 3939 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3940 | ` *  are not present in any of the other arrays.` |
|       - | 3941 | ` */` |
|      20 | 3942 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3943 |  |
|       - | 3944 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 3945 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3946 | `	ph7_value *pArray;` |
|       - | 3947 | `	ph7_value *pVal;` |
|       - | 3948 | `	sxi32 rc;` |
|       - | 3949 | `	sxu32 n;` |
|       - | 3950 | `	int i;` |
|       - | 3951 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 3952 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 3953 | `	 * accompanying integration tests to pass. */` |
|      22 | 3954 | `	if( nArg < 1 ){` |
|       4 | 3955 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3956 | `			"ArgumentCountError",` |
|       - | 3957 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 3958 | `			nArg` |
|       - | 3959 | `			);` |
|       - | 3960 | `	}` |
|      20 | 3961 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3962 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3963 | `			"TypeError",` |
|       - | 3964 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3965 | `			ph7_type_name(apArg[0])` |
|       - | 3966 | `			);` |
|       - | 3967 | `	}` |
|      32 | 3968 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3969 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 3970 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3971 | `				"TypeError",` |
|       - | 3972 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 3973 | `				i + 1,` |
|       4 | 3974 | `				ph7_type_name(apArg[i])` |
|       - | 3975 | `				);` |
|       - | 3976 | `		}` |
|       9 | 3977 | `	}` |
|      13 | 3978 | `	if( nArg == 1 ){` |
|       - | 3979 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3980 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3981 | `		return PH7_OK;` |
|       - | 3982 | `	}` |
|       - | 3983 | `	/* Create a new array */` |
|      11 | 3984 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 3985 | `	if( pArray == 0 ){` |
|     ! 0 | 3986 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3987 | `		return PH7_OK;` |
|       - | 3988 | `	}` |
|       - | 3989 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 3990 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3991 | `	/* Perform the diff */` |
|      11 | 3992 | `	pEntry = pSrc->pFirst;` |
|      11 | 3993 | `	n = pSrc->nEntry;` |
|      11 | 3994 | `	pN1 = pN2 = 0;` |
|      29 | 3995 | `	for(;;){` |
|       - | 3996 | `		int keep;` |
|      35 | 3997 | `		if( n < 1 ){` |
|      11 | 3998 | `			break;` |
|       - | 3999 | `		}` |
|       - | 4000 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4001 | `		keep = 1;` |
|      41 | 4002 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4003 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4004 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4005 | `			/* Perform a key lookup first */` |
|      29 | 4006 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4007 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4008 | `			}else{` |
|      17 | 4009 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4010 | `			}` |
|      29 | 4011 | `			if( rc != SXRET_OK ){` |
|       - | 4012 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4013 | `				continue;` |
|       - | 4014 | `			}` |
|       - | 4015 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4016 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4017 | `			if( pVal ){` |
|       - | 4018 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4019 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4020 | `				if( pVal2 ){` |
|      15 | 4021 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4022 | `					if( cmp == 0 ){` |
|       - | 4023 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4024 | `						keep = 0;` |
|      13 | 4025 | `						break;` |
|       - | 4026 | `					}` |
|       1 | 4027 | `				}` |
|       1 | 4028 | `			}` |
|       2 | 4029 | `		}` |
|      25 | 4030 | `		if( keep ){` |
|       - | 4031 | `			/* Perform the insertion */` |
|      13 | 4032 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4033 | `		}` |
|       - | 4034 | `		/* Point to the next entry */` |
|      25 | 4035 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4036 | `		n--;` |
|       1 | 4037 | `	}` |
|       - | 4038 | `	/* Return the freshly created array */` |
|      11 | 4039 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4040 | `	return PH7_OK;` |
|      12 | 4041 |  |
|       - | 4042 | `/*` |
|       - | 4043 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4044 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4045 | ` *  by a user supplied callback function.` |
|       - | 4046 | ` * Parameters` |
|       - | 4047 | ` *  $array1` |
|       - | 4048 | ` *    The array to compare from` |
|       - | 4049 | ` *  $array2` |
|       - | 4050 | ` *    An array to compare against` |
|       - | 4051 | ` *  $...` |
|       - | 4052 | ` *   More arrays to compare against.` |
|       - | 4053 | ` *  $key_compare_func` |
|       - | 4054 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4055 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4056 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4057 | ` * Return` |
|       - | 4058 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4059 | ` *  are not present in any of the other arrays.` |
|       - | 4060 | ` */` |
|      22 | 4061 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4062 |  |
|       - | 4063 | `	ph7_hashmap_node *pEntry;` |
|       - | 4064 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4065 | `	ph7_value *pCallback;` |
|       - | 4066 | `	ph7_value *pArray;` |
|       - | 4067 | `	sxi32 rc;` |
|       - | 4068 | `	sxu32 n;` |
|       - | 4069 | `	int i;` |
|       - | 4070 |  |
|       - | 4071 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4072 | `	if( nArg < 2 ){` |
|       4 | 4073 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4074 | `			"ArgumentCountError",` |
|       - | 4075 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4076 | `			nArg` |
|       - | 4077 | `			);` |
|       - | 4078 | `	}` |
|      22 | 4079 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4080 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4081 | `			"TypeError",` |
|       - | 4082 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4083 | `			ph7_type_name(apArg[0])` |
|       - | 4084 | `			);` |
|       - | 4085 | `	}` |
|       - | 4086 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4087 | `	 * expected to be a callback. */` |
|      32 | 4088 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4089 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4090 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4091 | `				"TypeError",` |
|       - | 4092 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4093 | `				i + 1,` |
|       2 | 4094 | `				ph7_type_name(apArg[i])` |
|       - | 4095 | `				);` |
|       - | 4096 | `		}` |
|       8 | 4097 | `	}` |
|       - | 4098 | `	/* Point to the callback value */` |
|      18 | 4099 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4100 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4101 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4102 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4103 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4104 | `		 * string given" which we also reproduce. */` |
|       7 | 4105 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4106 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4107 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4108 | `				"TypeError",` |
|       - | 4109 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4110 | `				nArg` |
|       - | 4111 | `				);` |
|       - | 4112 | `		}` |
|       5 | 4113 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4114 | `			/* neither array nor string */` |
|       7 | 4115 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4116 | `				"TypeError",` |
|       - | 4117 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4118 | `				nArg` |
|       - | 4119 | `				);` |
|       - | 4120 | `		}` |
|       - | 4121 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4122 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4123 | `			"TypeError",` |
|       - | 4124 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4125 | `			nArg,` |
|     ! 0 | 4126 | `			ph7_type_name(pCallback)` |
|       - | 4127 | `			);` |
|       - | 4128 | `	}` |
|      11 | 4129 | `	if( nArg == 2 ){` |
|       - | 4130 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4131 | `		 * input array. */` |
|       3 | 4132 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4133 | `		return PH7_OK;` |
|       - | 4134 | `	}` |
|       - | 4135 | `	/* Create a new array */` |
|       9 | 4136 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4137 | `	if( pArray == 0 ){` |
|     ! 0 | 4138 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4139 | `		return PH7_OK;` |
|       - | 4140 | `	}` |
|       - | 4141 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4142 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4143 | `	/* Perform the diff */` |
|       9 | 4144 | `	pEntry = pSrc->pFirst;` |
|       9 | 4145 | `	n = pSrc->nEntry;` |
|      20 | 4146 | `	for(;;){` |
|       - | 4147 | `		int keep;` |
|      25 | 4148 | `		if( n < 1 ){` |
|       9 | 4149 | `			break;` |
|       - | 4150 | `		}` |
|      17 | 4151 | `		keep = 1;` |
|      29 | 4152 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4153 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4154 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4155 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4156 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4157 | `			while( pIt ){` |
|       - | 4158 | `				/* build temporary key values for callback */` |
|       - | 4159 | `				ph7_value key1, key2, result;` |
|       - | 4160 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4161 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4162 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4163 | `				}else{` |
|       - | 4164 | `					SyString sStr;` |
|      31 | 4165 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4166 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4167 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4168 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4169 | `				}` |
|      31 | 4170 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4171 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4172 | `				}else{` |
|       - | 4173 | `					SyString sStr;` |
|      31 | 4174 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4175 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4176 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4177 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4178 | `				}` |
|      31 | 4179 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4180 | `				/* call user callback with (key1, key2) */` |
|       - | 4181 | `				{` |
|       - | 4182 | `					ph7_value *apK[2];` |
|      31 | 4183 | `					apK[0] = &key1;` |
|      31 | 4184 | `					apK[1] = &key2;` |
|      31 | 4185 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4186 | `				}` |
|      31 | 4187 | `				if( rc == SXRET_OK ){` |
|      31 | 4188 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4189 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4190 | `					}` |
|      31 | 4191 | `					if( result.x.iVal == 0 ){` |
|       - | 4192 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4193 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4194 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4195 | `						if( pVal1 && pVal2 ){` |
|      13 | 4196 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4197 | `								keep = 0;` |
|       9 | 4198 | `								PH7_MemObjRelease(&result);` |
|       - | 4199 | `								/* release keys too before breaking */` |
|       9 | 4200 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4201 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4202 | `								break;` |
|       - | 4203 | `							}` |
|       2 | 4204 | `						}` |
|       2 | 4205 | `					}` |
|      11 | 4206 | `				}` |
|      23 | 4207 | `				PH7_MemObjRelease(&result);` |
|      23 | 4208 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4209 | `				PH7_MemObjRelease(&key2);` |
|       - | 4210 | `				/* move to next node */` |
|      23 | 4211 | `				pIt = pIt->pPrev;` |
|      23 | 4212 | `				if( keep == 0 ) break;` |
|       1 | 4213 | `			}` |
|      21 | 4214 | `			if( keep == 0 ) break;` |
|       7 | 4215 | `		}` |
|      17 | 4216 | `		if( keep ){` |
|       - | 4217 | `			/* Perform the insertion */` |
|       9 | 4218 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4219 | `		}` |
|       - | 4220 | `		/* Point to the next entry */` |
|      17 | 4221 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4222 | `		n--;` |
|       1 | 4223 | `	}` |
|       - | 4224 | `	/* Return the freshly created array */` |
|       9 | 4225 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4226 | `	return PH7_OK;` |
|      13 | 4227 |  |
|       - | 4228 | `/*` |
|       - | 4229 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4230 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4231 | ` * Parameters` |
|       - | 4232 | ` *  $array1` |
|       - | 4233 | ` *    The array to compare from` |
|       - | 4234 | ` *  $array2` |
|       - | 4235 | ` *    An array to compare against` |
|       - | 4236 | ` *  $...` |
|       - | 4237 | ` *   More arrays to compare against` |
|       - | 4238 | ` * Return` |
|       - | 4239 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4240 | ` *  in any of the other arrays.` |
|       - | 4241 | ` * Note that NULL is returned on failure.` |
|       - | 4242 | ` */` |
|      14 | 4243 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4244 |  |
|       - | 4245 | `	ph7_hashmap_node *pEntry;` |
|       - | 4246 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4247 | `	ph7_value *pArray;` |
|       - | 4248 | `	sxi32 rc;` |
|       - | 4249 | `	sxu32 n;` |
|       - | 4250 | `	int i;` |
|       - | 4251 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4252 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4253 | `	 * helpers. */` |
|      16 | 4254 | `	if( nArg < 1 ){` |
|       4 | 4255 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4256 | `			"ArgumentCountError",` |
|       - | 4257 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4258 | `			nArg` |
|       - | 4259 | `			);` |
|       - | 4260 | `	}` |
|      14 | 4261 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4262 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4263 | `			"TypeError",` |
|       - | 4264 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4265 | `			ph7_type_name(apArg[0])` |
|       - | 4266 | `			);` |
|       - | 4267 | `	}` |
|      20 | 4268 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4269 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4270 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4271 | `				"TypeError",` |
|       - | 4272 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4273 | `				i + 1,` |
|       2 | 4274 | `				ph7_type_name(apArg[i])` |
|       - | 4275 | `				);` |
|       - | 4276 | `		}` |
|       5 | 4277 | `	}` |
|       9 | 4278 | `	if( nArg == 1 ){` |
|       - | 4279 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4280 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4281 | `		return PH7_OK;` |
|       - | 4282 | `	}` |
|       - | 4283 | `	/* Create a new array */` |
|       7 | 4284 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4285 | `	if( pArray == 0 ){` |
|     ! 0 | 4286 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4287 | `		return PH7_OK;` |
|       - | 4288 | `	}` |
|       - | 4289 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4290 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4291 | `	/* Perfrom the diff */` |
|       7 | 4292 | `	pEntry = pSrc->pFirst;` |
|       7 | 4293 | `	n = pSrc->nEntry;` |
|      12 | 4294 | `	for(;;){` |
|      25 | 4295 | `		if( n < 1 ){` |
|       7 | 4296 | `			break;` |
|       - | 4297 | `		}` |
|      31 | 4298 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4299 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4300 | `				/* ignore */` |
|     ! 0 | 4301 | `				continue;` |
|       - | 4302 | `			}` |
|      23 | 4303 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4304 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4305 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4306 | `				/* Blob lookup */` |
|      17 | 4307 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4308 | `			}else{` |
|       - | 4309 | `				/* Int lookup */` |
|       7 | 4310 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4311 | `			}` |
|      23 | 4312 | `			if( rc == SXRET_OK ){` |
|       - | 4313 | `				/* Key exists,break immediately */` |
|      11 | 4314 | `				break;` |
|       - | 4315 | `			}` |
|       7 | 4316 | `		}` |
|      19 | 4317 | `		if( i >= nArg ){` |
|       - | 4318 | `			/* Perform the insertion */` |
|       9 | 4319 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4320 | `		}` |
|       - | 4321 | `		/* Point to the next entry */` |
|      19 | 4322 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4323 | `		n--;` |
|       1 | 4324 | `	}` |
|       - | 4325 | `	/* Return the freshly created array */` |
|       7 | 4326 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4327 | `	return PH7_OK;` |
|       9 | 4328 |  |
|       - | 4329 | `/*` |
|       - | 4330 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4331 | ` *  Computes the intersection of arrays.` |
|       - | 4332 | ` * Parameters` |
|       - | 4333 | ` *  $array1` |
|       - | 4334 | ` *    The array to compare from` |
|       - | 4335 | ` *  $array2` |
|       - | 4336 | ` *    An array to compare against` |
|       - | 4337 | ` *  $...` |
|       - | 4338 | ` *   More arrays to compare against` |
|       - | 4339 | ` * Return` |
|       - | 4340 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4341 | ` *  in all of the parameters. .` |
|       - | 4342 | ` * Note that NULL is returned on failure.` |
|       - | 4343 | ` */` |
|       2 | 4344 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4345 |  |
|       - | 4346 | `	ph7_hashmap_node *pEntry;` |
|       - | 4347 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4348 | `	ph7_value *pArray;` |
|       - | 4349 | `	ph7_value *pVal;` |
|       - | 4350 | `	sxi32 rc;` |
|       - | 4351 | `	sxu32 n;` |
|       - | 4352 | `	int i;` |
|       3 | 4353 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4354 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4355 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4356 | `		return PH7_OK;` |
|       - | 4357 | `	}` |
|       3 | 4358 | `	if( nArg == 1 ){` |
|       - | 4359 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4360 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4361 | `		return PH7_OK;` |
|       - | 4362 | `	}` |
|       - | 4363 | `	/* Create a new array */` |
|       3 | 4364 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4365 | `	if( pArray == 0 ){` |
|     ! 0 | 4366 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4367 | `		return PH7_OK;` |
|       - | 4368 | `	}` |
|       - | 4369 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4370 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4371 | `	/* Perform the intersection */` |
|       3 | 4372 | `	pEntry = pSrc->pFirst;` |
|       3 | 4373 | `	n = pSrc->nEntry;` |
|       5 | 4374 | `	for(;;){` |
|      11 | 4375 | `		if( n < 1 ){` |
|       3 | 4376 | `			break;` |
|       - | 4377 | `		}` |
|       - | 4378 | `		/* Extract the node value */` |
|       9 | 4379 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4380 | `		if( pVal ){` |
|      13 | 4381 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       9 | 4382 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4383 | `					/* ignore */` |
|     ! 0 | 4384 | `					continue;` |
|       - | 4385 | `				}` |
|       - | 4386 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4387 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4388 | `				/* Perform the lookup */` |
|       9 | 4389 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|       9 | 4390 | `				if( rc != SXRET_OK ){` |
|       - | 4391 | `					/* Value does not exist */` |
|       5 | 4392 | `					break;` |
|       - | 4393 | `				}` |
|       3 | 4394 | `			}` |
|       9 | 4395 | `			if( i >= nArg ){` |
|       - | 4396 | `				/* Perform the insertion */` |
|       5 | 4397 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4398 | `			}` |
|       4 | 4399 | `		}` |
|       - | 4400 | `		/* Point to the next entry */` |
|       9 | 4401 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 4402 | `		n--;` |
|       1 | 4403 | `	}` |
|       - | 4404 | `	/* Return the freshly created array */` |
|       3 | 4405 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4406 | `	return PH7_OK;` |
|       2 | 4407 |  |
|       - | 4408 | `/*` |
|       - | 4409 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4410 | ` *  Computes the intersection of arrays.` |
|       - | 4411 | ` * Parameters` |
|       - | 4412 | ` *  $array1` |
|       - | 4413 | ` *    The array to compare from` |
|       - | 4414 | ` *  $array2` |
|       - | 4415 | ` *    An array to compare against` |
|       - | 4416 | ` *  $...` |
|       - | 4417 | ` *   More arrays to compare against` |
|       - | 4418 | ` * Return` |
|       - | 4419 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4420 | ` *  in all of the parameters. .` |
|       - | 4421 | ` * Note that NULL is returned on failure.` |
|       - | 4422 | ` */` |
|       2 | 4423 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4424 |  |
|       - | 4425 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4426 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4427 | `	ph7_value *pArray;` |
|       - | 4428 | `	ph7_value *pVal;` |
|       - | 4429 | `	sxi32 rc;` |
|       - | 4430 | `	sxu32 n;` |
|       - | 4431 | `	int i;` |
|       3 | 4432 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4433 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4434 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4435 | `		return PH7_OK;` |
|       - | 4436 | `	}` |
|       3 | 4437 | `	if( nArg == 1 ){` |
|       - | 4438 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4439 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4440 | `		return PH7_OK;` |
|       - | 4441 | `	}` |
|       - | 4442 | `	/* Create a new array */` |
|       3 | 4443 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4444 | `	if( pArray == 0 ){` |
|     ! 0 | 4445 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4446 | `		return PH7_OK;` |
|       - | 4447 | `	}` |
|       - | 4448 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4449 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4450 | `	/* Perform the intersection */` |
|       3 | 4451 | `	pEntry = pSrc->pFirst;` |
|       3 | 4452 | `	n = pSrc->nEntry;` |
|       3 | 4453 | `	pN1 = pN2 = 0; /* cc warning */` |
|       4 | 4454 | `	for(;;){` |
|       9 | 4455 | `		if( n < 1 ){` |
|       3 | 4456 | `			break;` |
|       - | 4457 | `		}` |
|       - | 4458 | `		/* Extract the node value */` |
|       7 | 4459 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4460 | `		if( pVal ){` |
|       9 | 4461 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       7 | 4462 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4463 | `					/* ignore */` |
|     ! 0 | 4464 | `					continue;` |
|       - | 4465 | `				}` |
|       - | 4466 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4467 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4468 | `				/* Perform a key lookup first */` |
|       7 | 4469 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4470 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|     ! 0 | 4471 | `				}else{` |
|       7 | 4472 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4473 | `				}` |
|       7 | 4474 | `				if( rc != SXRET_OK ){` |
|       - | 4475 | `					/* No such key,break immediately */` |
|       3 | 4476 | `					break;` |
|       - | 4477 | `				}` |
|       - | 4478 | `				/* Perform the lookup */` |
|       5 | 4479 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|       5 | 4480 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4481 | `					/* Value does not exist */` |
|       2 | 4482 | `					break;` |
|       - | 4483 | `				}` |
|       2 | 4484 | `			}` |
|       7 | 4485 | `			if( i >= nArg ){` |
|       - | 4486 | `				/* Perform the insertion */` |
|       3 | 4487 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       1 | 4488 | `			}` |
|       3 | 4489 | `		}` |
|       - | 4490 | `		/* Point to the next entry */` |
|       7 | 4491 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4492 | `		n--;` |
|       1 | 4493 | `	}` |
|       - | 4494 | `	/* Return the freshly created array */` |
|       3 | 4495 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4496 | `	return PH7_OK;` |
|       2 | 4497 |  |
|       - | 4498 | `/*` |
|       - | 4499 | ` * array array_intersect_key(array $array1 ,array $array2,...)` |
|       - | 4500 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4501 | ` * Parameters` |
|       - | 4502 | ` *  $array1` |
|       - | 4503 | ` *    The array to compare from` |
|       - | 4504 | ` *  $array2` |
|       - | 4505 | ` *    An array to compare against` |
|       - | 4506 | ` *  $...` |
|       - | 4507 | ` *   More arrays to compare against` |
|       - | 4508 | ` * Return` |
|       - | 4509 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4510 | ` *  have keys that are present in all arguments.` |
|       - | 4511 | ` * Note that NULL is returned on failure.` |
|       - | 4512 | ` */` |
|       4 | 4513 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4514 |  |
|       - | 4515 | `	ph7_hashmap_node *pEntry;` |
|       - | 4516 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4517 | `	ph7_value *pArray;` |
|       - | 4518 | `	sxi32 rc;` |
|       - | 4519 | `	sxu32 n;` |
|       - | 4520 | `	int i;` |
|       5 | 4521 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4522 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4523 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4524 | `		return PH7_OK;` |
|       - | 4525 | `	}` |
|       5 | 4526 | `	if( nArg == 1 ){` |
|       - | 4527 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4528 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4529 | `		return PH7_OK;` |
|       - | 4530 | `	}` |
|       - | 4531 | `	/* Create a new array */` |
|       5 | 4532 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4533 | `	if( pArray == 0 ){` |
|     ! 0 | 4534 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4535 | `		return PH7_OK;` |
|       - | 4536 | `	}` |
|       - | 4537 | `	/* Point to the internal representation of the main hashmap */` |
|       5 | 4538 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4539 | `	/* Perfrom the intersection */` |
|       5 | 4540 | `	pEntry = pSrc->pFirst;` |
|       5 | 4541 | `	n = pSrc->nEntry;` |
|       8 | 4542 | `	for(;;){` |
|      17 | 4543 | `		if( n < 1 ){` |
|       5 | 4544 | `			break;` |
|       - | 4545 | `		}` |
|      19 | 4546 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      13 | 4547 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4548 | `				/* ignore */` |
|     ! 0 | 4549 | `				continue;` |
|       - | 4550 | `			}` |
|      13 | 4551 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      13 | 4552 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       7 | 4553 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4554 | `				/* Blob lookup */` |
|       7 | 4555 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       4 | 4556 | `			}else{` |
|       - | 4557 | `				/* Int key */` |
|       7 | 4558 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4559 | `			}` |
|      13 | 4560 | `			if( rc != SXRET_OK ){` |
|       - | 4561 | `				/* Key does not exists,break immediately */` |
|       7 | 4562 | `				break;` |
|       - | 4563 | `			}` |
|       4 | 4564 | `		}` |
|      13 | 4565 | `		if( i >= nArg ){` |
|       - | 4566 | `			/* Perform the insertion */` |
|       7 | 4567 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 4568 | `		}` |
|       - | 4569 | `		/* Point to the next entry */` |
|      13 | 4570 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 4571 | `		n--;` |
|       1 | 4572 | `	}` |
|       - | 4573 | `	/* Return the freshly created array */` |
|       5 | 4574 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 4575 | `	return PH7_OK;` |
|       3 | 4576 |  |
|       - | 4577 | `/*` |
|       - | 4578 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4579 | ` *  Computes the intersection of arrays.` |
|       - | 4580 | ` * Parameters` |
|       - | 4581 | ` *  $array1` |
|       - | 4582 | ` *    The array to compare from` |
|       - | 4583 | ` *  $array2` |
|       - | 4584 | ` *    An array to compare against` |
|       - | 4585 | ` *  $...` |
|       - | 4586 | ` *   More arrays to compare against` |
|       - | 4587 | ` * $callback` |
|       - | 4588 | ` *  The callback comparison function.` |
|       - | 4589 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4590 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4591 | ` *  than the second.` |
|       - | 4592 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4593 | ` * Return` |
|       - | 4594 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4595 | ` *  in all of the parameters. .` |
|       - | 4596 | ` * Note that NULL is returned on failure.` |
|       - | 4597 | ` */` |
|       2 | 4598 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4599 |  |
|       - | 4600 | `	ph7_hashmap_node *pEntry;` |
|       - | 4601 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4602 | `	ph7_value *pCallback;` |
|       - | 4603 | `	ph7_value *pArray;` |
|       - | 4604 | `	ph7_value *pVal;` |
|       - | 4605 | `	sxi32 rc;` |
|       - | 4606 | `	sxu32 n;` |
|       - | 4607 | `	int i;` |
|       - | 4608 |  |
|       3 | 4609 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4610 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4611 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4612 | `		return PH7_OK;` |
|       - | 4613 | `	}` |
|       - | 4614 | `	/* Point to the callback */` |
|       3 | 4615 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4616 | `	if( nArg == 2 ){` |
|       - | 4617 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4618 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4619 | `		return PH7_OK;` |
|       - | 4620 | `	}` |
|       - | 4621 | `	/* Create a new array */` |
|       3 | 4622 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4623 | `	if( pArray == 0 ){` |
|     ! 0 | 4624 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4625 | `		return PH7_OK;` |
|       - | 4626 | `	}` |
|       - | 4627 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4628 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4629 | `	/* Perform the intersection */` |
|       3 | 4630 | `	pEntry = pSrc->pFirst;` |
|       3 | 4631 | `	n = pSrc->nEntry;` |
|       4 | 4632 | `	for(;;){` |
|       9 | 4633 | `		if( n < 1 ){` |
|       3 | 4634 | `			break;` |
|       - | 4635 | `		}` |
|       - | 4636 | `		/* Extract the node value */` |
|       7 | 4637 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4638 | `		if( pVal ){` |
|      11 | 4639 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4640 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4641 | `					/* ignore */` |
|     ! 0 | 4642 | `					continue;` |
|       - | 4643 | `				}` |
|       - | 4644 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4645 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4646 | `				/* Perform the lookup */` |
|       7 | 4647 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4648 | `				if( rc != SXRET_OK ){` |
|       - | 4649 | `					/* Value does not exist */` |
|       3 | 4650 | `					break;` |
|       - | 4651 | `				}` |
|       3 | 4652 | `			}` |
|       7 | 4653 | `			if( i >= (nArg-1) ){` |
|       - | 4654 | `				/* Perform the insertion */` |
|       5 | 4655 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4656 | `			}` |
|       3 | 4657 | `		}` |
|       - | 4658 | `		/* Point to the next entry */` |
|       7 | 4659 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4660 | `		n--;` |
|       1 | 4661 | `	}` |
|       - | 4662 | `	/* Return the freshly created array */` |
|       3 | 4663 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4664 | `	return PH7_OK;` |
|       2 | 4665 |  |
|       - | 4666 | `/*` |
|       - | 4667 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 4668 | ` *  Fill an array with values.` |
|       - | 4669 | ` * Parameters` |
|       - | 4670 | ` *  $start_index` |
|       - | 4671 | ` *    The first index of the returned array.` |
|       - | 4672 | ` *  $num` |
|       - | 4673 | ` *   Number of elements to insert.` |
|       - | 4674 | ` *  $value` |
|       - | 4675 | ` *    Value to use for filling.` |
|       - | 4676 | ` * Return` |
|       - | 4677 | ` *  The filled array or null on failure.` |
|       - | 4678 | ` */` |
|     238 | 4679 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4680 |  |
|       - | 4681 | `	ph7_value *pArray;` |
|       - | 4682 | `	int i,nEntry;` |
|       - | 4683 |  |
|       - | 4684 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 4685 | `	if( nArg != 3 ){` |
|       - | 4686 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 4687 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4688 | `			"ArgumentCountError",` |
|       - | 4689 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 4690 | `			nArg` |
|       - | 4691 | `			);` |
|       - | 4692 | `	}` |
|       - | 4693 |  |
|       - | 4694 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 4695 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 4696 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 4697 | `	 * and NULLs are rejected outright. */` |
|     466 | 4698 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 4699 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 4700 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4701 | `			"TypeError",` |
|       - | 4702 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 4703 | `			ph7_type_name(apArg[0])` |
|       - | 4704 | `			);` |
|       - | 4705 | `	}` |
|     234 | 4706 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 4707 | `		int len;` |
|       8 | 4708 | `		sxu8 bReal = FALSE;` |
|       8 | 4709 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 4710 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 4711 | `			/* Non‑numeric string is an error. */` |
|       3 | 4712 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4713 | `				"TypeError",` |
|       - | 4714 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 4715 | `				);` |
|       - | 4716 | `		}` |
|       5 | 4717 | `		if( bReal ){` |
|       - | 4718 | `			/* float-string -> deprecation warning */` |
|       4 | 4719 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4720 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 4721 | `				zStr` |
|       - | 4722 | `				);` |
|       1 | 4723 | `		}` |
|       2 | 4724 | `	}` |
|       - | 4725 |  |
|       - | 4726 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 4727 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 4728 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 4729 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4730 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4731 | `			"TypeError",` |
|       - | 4732 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 4733 | `			ph7_type_name(apArg[1])` |
|       - | 4734 | `			);` |
|       - | 4735 | `	}` |
|     232 | 4736 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4737 | `		int len;` |
|       3 | 4738 | `		sxu8 bReal = FALSE;` |
|       3 | 4739 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4740 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 4741 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4742 | `				"TypeError",` |
|       - | 4743 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 4744 | `				);` |
|       - | 4745 | `		}` |
|     ! 0 | 4746 | `	}` |
|       - | 4747 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 4748 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 4749 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 4750 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 4751 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 4752 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 4753 | `		if( d != (double)i64 ){` |
|       7 | 4754 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4755 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 4756 | `				d` |
|       - | 4757 | `				);` |
|       2 | 4758 | `		}` |
|       2 | 4759 | `	}` |
|       - | 4760 |  |
|       - | 4761 | `	/* Total number of entries to insert */` |
|     230 | 4762 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 4763 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 4764 | `	if( nEntry < 0 ){` |
|       3 | 4765 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4766 | `			"ValueError",` |
|       - | 4767 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 4768 | `			);` |
|       - | 4769 | `	}` |
|       - | 4770 |  |
|       - | 4771 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 4772 | `	if( nEntry == 0 ){` |
|       7 | 4773 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 4774 | `		return PH7_OK;` |
|       - | 4775 | `	}` |
|       - | 4776 |  |
|       - | 4777 | `	/* Create a new array */` |
|     221 | 4778 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 4779 | `	if( pArray == 0 ){` |
|     ! 0 | 4780 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4781 | `		return PH7_OK;` |
|       - | 4782 | `	}` |
|       - | 4783 |  |
|       - | 4784 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 4785 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 4786 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 4787 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 4788 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 4789 | `	}` |
|       - | 4790 | `	/* Return the filled array */` |
|     221 | 4791 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 4792 | `	return PH7_OK;` |
|     121 | 4793 |  |
|       - | 4794 | `/*` |
|       - | 4795 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 4796 | ` *  Fill an array with values, specifying keys.` |
|       - | 4797 | ` * Parameters` |
|       - | 4798 | ` *  $input` |
|       - | 4799 | ` *   Array of values that will be used as key.` |
|       - | 4800 | ` *  $value` |
|       - | 4801 | ` *    Value to use for filling.` |
|       - | 4802 | ` * Return` |
|       - | 4803 | ` *  The filled array.` |
|       - | 4804 | ` * Throws` |
|       - | 4805 | ` *  ValueError if $input is not an array.` |
|       - | 4806 | ` */` |
|      26 | 4807 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4808 |  |
|       - | 4809 | `	ph7_hashmap_node *pEntry;` |
|       - | 4810 | `	ph7_hashmap *pSrc;` |
|       - | 4811 | `	ph7_value *pArray;` |
|       - | 4812 | `	sxu32 n;` |
|       - | 4813 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 4814 | `	if( nArg != 2 ){` |
|      10 | 4815 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4816 | `			"ArgumentCountError",` |
|       - | 4817 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 4818 | `			nArg` |
|       - | 4819 | `			);` |
|       - | 4820 | `	}` |
|       - | 4821 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 4822 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 4823 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4824 | `			"TypeError",` |
|       - | 4825 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 4826 | `			ph7_type_name(apArg[0])` |
|       - | 4827 | `			);` |
|       - | 4828 | `	}` |
|       - | 4829 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 4830 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4831 | `	/* Create a new array */` |
|      17 | 4832 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 4833 | `	if( pArray == 0 ){` |
|     ! 0 | 4834 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4835 | `		return PH7_OK;` |
|       - | 4836 | `	}` |
|       - | 4837 | `	/* Perform the requested operation */` |
|      17 | 4838 | `	pEntry = pSrc->pFirst;` |
|      45 | 4839 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 4840 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 4841 | `		/* Point to the next entry */` |
|      29 | 4842 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 4843 | `	}` |
|       - | 4844 | `	/* Return the filled array */` |
|      17 | 4845 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4846 | `	return PH7_OK;` |
|      15 | 4847 |  |
|       - | 4848 | `/*` |
|       - | 4849 | ` * array array_combine(array $keys,array $values)` |
|       - | 4850 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 4851 | ` * Parameters` |
|       - | 4852 | ` *  $keys` |
|       - | 4853 | ` *    Array of keys to be used.` |
|       - | 4854 | ` * $values` |
|       - | 4855 | ` *   Array of values to be used.` |
|       - | 4856 | ` * Return` |
|       - | 4857 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 4858 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 4859 | ` *  not an array.` |
|       - | 4860 | ` */` |
|      18 | 4861 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4862 |  |
|       - | 4863 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 4864 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 4865 | `	ph7_value *pArray;` |
|       - | 4866 | `	sxu32 n;` |
|       - | 4867 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 4868 | `	if( nArg != 2 ){` |
|       - | 4869 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 4870 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4871 | `			"ArgumentCountError",` |
|       - | 4872 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 4873 | `			nArg` |
|       - | 4874 | `			);` |
|       - | 4875 | `	}` |
|       - | 4876 | `	/* Validate argument types individually so we can report the correct` |
|       - | 4877 | `	 * argument index in the error message. */` |
|      18 | 4878 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4879 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4880 | `			"TypeError",` |
|       - | 4881 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 4882 | `			ph7_type_name(apArg[0])` |
|       - | 4883 | `			);` |
|       - | 4884 | `	}` |
|      16 | 4885 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 4886 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4887 | `			"TypeError",` |
|       - | 4888 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 4889 | `			ph7_type_name(apArg[1])` |
|       - | 4890 | `			);` |
|       - | 4891 | `	}` |
|       - | 4892 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 4893 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 4894 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 4895 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 4896 | `		/* Length mismatch -> ValueError */` |
|       3 | 4897 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4898 | `			"ValueError",` |
|       - | 4899 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 4900 | `			);` |
|       - | 4901 | `	}` |
|       - | 4902 | `	/* Create a new array */` |
|      11 | 4903 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4904 | `	if( pArray == 0 ){` |
|     ! 0 | 4905 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4906 | `		return PH7_OK;` |
|       - | 4907 | `	}` |
|       - | 4908 | `	/* Perform the requested operation */` |
|      11 | 4909 | `	pKe = pKey->pFirst;` |
|      11 | 4910 | `	pVe = pValue->pFirst;` |
|      33 | 4911 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 4912 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 4913 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 4914 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 4915 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 4916 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 4917 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 4918 | `		 * original array must not be mutated. */` |
|      23 | 4919 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 4920 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 4921 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 4922 | `			if( pTmpKey ){` |
|       5 | 4923 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 4924 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 4925 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 4926 | `				pKeyCopy = pTmpKey;` |
|       2 | 4927 | `			}` |
|       2 | 4928 | `		}` |
|      23 | 4929 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 4930 | `		/* Point to the next entry */` |
|      23 | 4931 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 4932 | `		pVe = pVe->pPrev;` |
|      12 | 4933 | `	}` |
|       - | 4934 | `	/* Return the filled array */` |
|      11 | 4935 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4936 | `	return PH7_OK;` |
|      11 | 4937 |  |
|       - | 4938 | `/*` |
|       - | 4939 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 4940 | ` *  Return an array with elements in reverse order.` |
|       - | 4941 | ` * Parameters` |
|       - | 4942 | ` *  $array` |
|       - | 4943 | ` *   The input array.` |
|       - | 4944 | ` *  $preserve_keys (optional)` |
|       - | 4945 | ` *   If set to TRUE keys are preserved.` |
|       - | 4946 | ` * Return` |
|       - | 4947 | ` *  The reversed array.` |
|       - | 4948 | ` */` |
|      20 | 4949 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4950 |  |
|       - | 4951 | `	ph7_hashmap_node *pEntry;` |
|       - | 4952 | `	ph7_hashmap *pSrc;` |
|       - | 4953 | `	ph7_value *pArray;` |
|       - | 4954 | `	int bPreserve;` |
|       - | 4955 | `	sxu32 n;` |
|      22 | 4956 | `	if( nArg < 1 ){` |
|       4 | 4957 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4958 | `			"ArgumentCountError",` |
|       - | 4959 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 4960 | `			nArg` |
|       - | 4961 | `			);` |
|       - | 4962 | `	}` |
|       - | 4963 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 4964 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4965 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4966 | `			"TypeError",` |
|       - | 4967 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4968 | `			ph7_type_name(apArg[0])` |
|       - | 4969 | `			);` |
|       - | 4970 | `	}` |
|      17 | 4971 | `	bPreserve = FALSE;` |
|      17 | 4972 | `	if( nArg > 1 ){` |
|       7 | 4973 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 4974 | `	}` |
|       - | 4975 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 4976 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4977 | `	/* Create a new array */` |
|      17 | 4978 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 4979 | `	if( pArray == 0 ){` |
|     ! 0 | 4980 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4981 | `		return PH7_OK;` |
|       - | 4982 | `	}` |
|       - | 4983 | `	/* Perform the requested operation */` |
|      17 | 4984 | `	pEntry = pSrc->pLast;` |
|      55 | 4985 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 4986 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 4987 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 4988 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 4989 | `		/* Point to the previous entry */` |
|      39 | 4990 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 4991 | `	}` |
|      17 | 4992 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4993 | `	return PH7_OK;` |
|      12 | 4994 |  |
|       - | 4995 | `/*` |
|       - | 4996 | ` * array array_unique(array $array[,int $sort_flags = SORT_STRING ])` |
|       - | 4997 | ` *  Removes duplicate values from an array` |
|       - | 4998 | ` * Parameter` |
|       - | 4999 | ` *  $array` |
|       - | 5000 | ` *   The input array.` |
|       - | 5001 | ` *  $sort_flags` |
|       - | 5002 | ` *    The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 5003 | ` *    Sorting type flags:` |
|       - | 5004 | ` *       SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5005 | ` *       SORT_NUMERIC - compare items numerically` |
|       - | 5006 | ` *       SORT_STRING - compare items as strings` |
|       - | 5007 | ` *       SORT_LOCALE_STRING - compare items as` |
|       - | 5008 | ` * Return` |
|       - | 5009 | ` *  Filtered array or NULL on failure.` |
|       - | 5010 | ` */` |
|       2 | 5011 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5012 |  |
|       - | 5013 | `	ph7_hashmap_node *pEntry;` |
|       - | 5014 | `	ph7_value *pNeedle;` |
|       - | 5015 | `	ph7_hashmap *pSrc;` |
|       - | 5016 | `	ph7_value *pArray;` |
|       - | 5017 | `	int bStrict;` |
|       - | 5018 | `	sxi32 rc;` |
|       - | 5019 | `	sxu32 n;` |
|       3 | 5020 | `	if( nArg < 1 ){` |
|       - | 5021 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 5022 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5023 | `		return PH7_OK;` |
|       - | 5024 | `	}` |
|       - | 5025 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 5026 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5027 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 5028 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5029 | `		return PH7_OK;` |
|       - | 5030 | `	}` |
|       3 | 5031 | `	bStrict = FALSE;` |
|       3 | 5032 | `	if( nArg > 1 ){` |
|     ! 0 | 5033 | `		bStrict = ph7_value_to_int(apArg[1]) == 3 /* SORT_REGULAR */ ? 1 : 0;` |
|     ! 0 | 5034 | `	}` |
|       - | 5035 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 5036 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5037 | `	/* Create a new array */` |
|       3 | 5038 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5039 | `	if( pArray == 0 ){` |
|     ! 0 | 5040 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5041 | `		return PH7_OK;` |
|       - | 5042 | `	}` |
|       - | 5043 | `	/* Perform the requested operation */` |
|       3 | 5044 | `	pEntry = pSrc->pFirst;` |
|      13 | 5045 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      11 | 5046 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      11 | 5047 | `		rc = SXERR_NOTFOUND;` |
|      11 | 5048 | `		if( pNeedle ){` |
|      11 | 5049 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|       5 | 5050 | `		}` |
|      11 | 5051 | `		if( rc != SXRET_OK ){` |
|       - | 5052 | `			/* Perform the insertion */` |
|       7 | 5053 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 5054 | `		}` |
|       - | 5055 | `		/* Point to the next entry */` |
|      11 | 5056 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 5057 | `	}` |
|       - | 5058 | `	/* Return the freshly created array */` |
|       3 | 5059 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5060 | `	return PH7_OK;` |
|       2 | 5061 |  |
|       - | 5062 | `/*` |
|       - | 5063 | ` * array array_flip(array $input)` |
|       - | 5064 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5065 | ` * Parameter` |
|       - | 5066 | ` *  $input` |
|       - | 5067 | ` *   Input array.` |
|       - | 5068 | ` * Return` |
|       - | 5069 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5070 | ` */` |
|      34 | 5071 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5072 |  |
|       - | 5073 | `	ph7_hashmap_node *pEntry;` |
|       - | 5074 | `	ph7_hashmap *pSrc;` |
|       - | 5075 | `	ph7_value *pArray;` |
|       - | 5076 | `	ph7_value *pKey;` |
|       - | 5077 | `	ph7_value sVal;` |
|       - | 5078 | `	sxu32 n;` |
|       - | 5079 |  |
|       - | 5080 | `	/* PHP requires exactly one argument */` |
|      36 | 5081 | `	if( nArg != 1 ){` |
|       - | 5082 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5083 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5084 | `			"ArgumentCountError",` |
|       - | 5085 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5086 | `			nArg` |
|       - | 5087 | `			);` |
|       - | 5088 | `	}` |
|       - | 5089 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5090 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5091 | `		/* Type mismatch -> TypeError */` |
|       7 | 5092 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5093 | `			"TypeError",` |
|       - | 5094 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5095 | `			ph7_type_name(apArg[0])` |
|       - | 5096 | `			);` |
|       - | 5097 | `	}` |
|       - | 5098 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5099 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5100 | `	/* Create a new array */` |
|      27 | 5101 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5102 | `	if( pArray == 0 ){` |
|     ! 0 | 5103 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5104 | `		return PH7_OK;` |
|       - | 5105 | `	}` |
|       - | 5106 | `	/* Start processing */` |
|      27 | 5107 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5108 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5109 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5110 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5111 | `		if( pKey ){` |
|       - | 5112 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5113 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5114 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5115 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5116 | `					);` |
|   22236 | 5117 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5118 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5119 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5120 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5121 | `				}else{` |
|       - | 5122 | `					SyString sStr;` |
|    2227 | 5123 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5124 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5125 | `				}` |
|       - | 5126 | `				/* Perform the insertion */` |
|   22227 | 5127 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5128 | `				/* Safely release the value because each inserted entry` |
|       - | 5129 | `				 * has its own private copy of the value.` |
|       - | 5130 | `				 */` |
|   22227 | 5131 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5132 | `			}else{` |
|       - | 5133 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5134 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5135 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5136 | `					);` |
|       - | 5137 | `			}` |
|   11118 | 5138 | `		}` |
|       - | 5139 | `		/* Point to the next entry */` |
|   22237 | 5140 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5141 | `	}` |
|       - | 5142 | `	/* Return the freshly created array */` |
|      27 | 5143 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5144 | `	return PH7_OK;` |
|      19 | 5145 |  |
|       - | 5146 | `/*` |
|       - | 5147 | ` * number array_sum(array $array )` |
|       - | 5148 | ` *  Calculate the sum of values in an array.` |
|       - | 5149 | ` * Parameters` |
|       - | 5150 | ` *  $array: The input array.` |
|       - | 5151 | ` * Return` |
|       - | 5152 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5153 | ` */` |
|      24 | 5154 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5155 |  |
|       - | 5156 | `	ph7_hashmap_node *pEntry;` |
|       - | 5157 | `	ph7_value *pObj;` |
|      25 | 5158 | `	double dSum = 0;` |
|       - | 5159 | `	sxu32 n;` |
|      25 | 5160 | `	pEntry = pMap->pFirst;` |
|      91 | 5161 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5162 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5163 | `		if( pObj ){` |
|      67 | 5164 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5165 | `				dSum += pObj->rVal;` |
|      53 | 5166 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5167 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5168 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5169 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5170 | `					double dv = 0;` |
|      13 | 5171 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5172 | `					dSum += dv;` |
|       7 | 5173 | `				}` |
|      12 | 5174 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5175 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5176 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5177 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5178 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5179 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5180 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5181 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5182 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5183 | `			}` |
|       - | 5184 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5185 | `		}` |
|       - | 5186 | `		/* Point to the next entry */` |
|      67 | 5187 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5188 | `	}` |
|       - | 5189 | `	/* Return sum */` |
|      25 | 5190 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5191 |  |
|      18 | 5192 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5193 |  |
|       - | 5194 | `	ph7_hashmap_node *pEntry;` |
|       - | 5195 | `	ph7_value *pObj;` |
|      20 | 5196 | `	sxi64 nSum = 0;` |
|       - | 5197 | `	sxu32 n;` |
|      20 | 5198 | `	pEntry = pMap->pFirst;` |
|      80 | 5199 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5200 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5201 | `		if( pObj ){` |
|      62 | 5202 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5203 | `				nSum += pObj->x.iVal;` |
|      36 | 5204 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5205 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5206 | `					sxi64 nv = 0;` |
|       5 | 5207 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5208 | `					nSum += nv;` |
|       3 | 5209 | `				}` |
|       8 | 5210 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5211 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5212 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5213 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5214 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5215 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5216 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5217 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5218 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5219 | `			}` |
|       - | 5220 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5221 | `		}` |
|       - | 5222 | `		/* Point to the next entry */` |
|      62 | 5223 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5224 | `	}` |
|       - | 5225 | `	/* Return sum */` |
|      20 | 5226 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5227 |  |
|       - | 5228 | `/* number array_sum(array $array )` |
|       - | 5229 | ` * (See block-coment above)` |
|       - | 5230 | ` */` |
|      52 | 5231 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5232 |  |
|       - | 5233 | `	ph7_hashmap_node *pEntry;` |
|       - | 5234 | `	ph7_hashmap *pMap;` |
|       - | 5235 | `	ph7_value *pObj;` |
|      54 | 5236 | `	int useDouble = 0;` |
|       - | 5237 | `	sxu32 n;` |
|       - | 5238 | `	/* PHP requires exactly one argument */` |
|      54 | 5239 | `	if( nArg != 1 ){` |
|       7 | 5240 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5241 | `			"ArgumentCountError",` |
|       - | 5242 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5243 | `			nArg` |
|       - | 5244 | `			);` |
|       - | 5245 | `	}` |
|       - | 5246 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5247 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5248 | `		/* Type mismatch -> TypeError */` |
|       7 | 5249 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5250 | `			"TypeError",` |
|       - | 5251 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5252 | `			ph7_type_name(apArg[0])` |
|       - | 5253 | `			);` |
|       - | 5254 | `	}` |
|      46 | 5255 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5256 | `	if( pMap->nEntry < 1 ){` |
|       - | 5257 | `		/* Nothing to compute,return 0 */` |
|       3 | 5258 | `		ph7_result_int(pCtx,0);` |
|       3 | 5259 | `		return PH7_OK;` |
|       - | 5260 | `	}` |
|       - | 5261 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5262 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5263 | `	 */` |
|      44 | 5264 | `	pEntry = pMap->pFirst;` |
|     112 | 5265 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5266 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5267 | `		if( pObj ){` |
|      94 | 5268 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5269 | `				useDouble = 1;` |
|      19 | 5270 | `				break;` |
|       - | 5271 | `			}` |
|      76 | 5272 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5273 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5274 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5275 | `				sxu32 i;` |
|      23 | 5276 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5277 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5278 | `						useDouble = 1;` |
|       7 | 5279 | `						break;` |
|       - | 5280 | `					}` |
|       6 | 5281 | `				}` |
|      13 | 5282 | `				if( useDouble ){` |
|       7 | 5283 | `					break;` |
|       - | 5284 | `				}` |
|       3 | 5285 | `			}` |
|      34 | 5286 | `		}` |
|      70 | 5287 | `		pEntry = pEntry->pPrev;` |
|      36 | 5288 | `	}` |
|      44 | 5289 | `	if( useDouble ){` |
|      25 | 5290 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5291 | `	}else{` |
|      20 | 5292 | `		Int64Sum(pCtx,pMap);` |
|       - | 5293 | `	}` |
|      44 | 5294 | `	return PH7_OK;` |
|      28 | 5295 |  |
|       - | 5296 | `/*` |
|       - | 5297 | ` * number array_product(array $array )` |
|       - | 5298 | ` *  Calculate the product of values in an array.` |
|       - | 5299 | ` * Parameters` |
|       - | 5300 | ` *  $array: The input array.` |
|       - | 5301 | ` * Return` |
|       - | 5302 | ` *  Returns the product of values as an integer or float.` |
|       - | 5303 | ` */` |
|     ! 0 | 5304 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5305 |  |
|       - | 5306 | `	ph7_hashmap_node *pEntry;` |
|       - | 5307 | `	ph7_value *pObj;` |
|       - | 5308 | `	double dProd;` |
|       - | 5309 | `	sxu32 n;` |
|     ! 0 | 5310 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5311 | `	dProd = 1;` |
|     ! 0 | 5312 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5313 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5314 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5315 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5316 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5317 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5318 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5319 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5320 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5321 | `					double dv = 0;` |
|     ! 0 | 5322 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5323 | `					dProd *= dv;` |
|     ! 0 | 5324 | `				}` |
|     ! 0 | 5325 | `			}` |
|     ! 0 | 5326 | `		}` |
|       - | 5327 | `		/* Point to the next entry */` |
|     ! 0 | 5328 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5329 | `	}` |
|       - | 5330 | `	/* Return product */` |
|     ! 0 | 5331 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5332 |  |
|     ! 0 | 5333 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5334 |  |
|       - | 5335 | `	ph7_hashmap_node *pEntry;` |
|       - | 5336 | `	ph7_value *pObj;` |
|       - | 5337 | `	sxi64 nProd;` |
|       - | 5338 | `	sxu32 n;` |
|     ! 0 | 5339 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5340 | `	nProd = 1;` |
|     ! 0 | 5341 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5342 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5343 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5344 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5345 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5346 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5347 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5348 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5349 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5350 | `					sxi64 nv = 0;` |
|     ! 0 | 5351 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5352 | `					nProd *= nv;` |
|     ! 0 | 5353 | `				}` |
|     ! 0 | 5354 | `			}` |
|     ! 0 | 5355 | `		}` |
|       - | 5356 | `		/* Point to the next entry */` |
|     ! 0 | 5357 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5358 | `	}` |
|       - | 5359 | `	/* Return product */` |
|     ! 0 | 5360 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5361 |  |
|       - | 5362 | `/* number array_product(array $array )` |
|       - | 5363 | ` * (See block-block comment above)` |
|       - | 5364 | ` */` |
|     ! 0 | 5365 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5366 |  |
|       - | 5367 | `	ph7_hashmap *pMap;` |
|       - | 5368 | `	ph7_value *pObj;` |
|     ! 0 | 5369 | `	if( nArg < 1 ){` |
|       - | 5370 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5371 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5372 | `		return PH7_OK;` |
|       - | 5373 | `	}` |
|       - | 5374 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5375 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5376 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5377 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5378 | `		return PH7_OK;` |
|       - | 5379 | `	}` |
|     ! 0 | 5380 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5381 | `	if( pMap->nEntry < 1 ){` |
|       - | 5382 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5383 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5384 | `		return PH7_OK;` |
|       - | 5385 | `	}` |
|       - | 5386 | `	/* If the first element is of type float,then perform floating` |
|       - | 5387 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5388 | `	 */` |
|     ! 0 | 5389 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5390 | `	if( pObj == 0 ){` |
|     ! 0 | 5391 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5392 | `		return PH7_OK;` |
|       - | 5393 | `	}` |
|     ! 0 | 5394 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5395 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5396 | `	}else{` |
|     ! 0 | 5397 | `		Int64Prod(pCtx,pMap);` |
|       - | 5398 | `	}` |
|     ! 0 | 5399 | `	return PH7_OK;` |
|     ! 0 | 5400 |  |
|       - | 5401 | `/*` |
|       - | 5402 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5403 | ` *  Pick one or more random entries out of an array.` |
|       - | 5404 | ` * Parameters` |
|       - | 5405 | ` * $input` |
|       - | 5406 | ` *  The input array.` |
|       - | 5407 | ` * $num_req` |
|       - | 5408 | ` *  Specifies how many entries you want to pick.` |
|       - | 5409 | ` * Return` |
|       - | 5410 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5411 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5412 | ` *  NULL is returned on failure.` |
|       - | 5413 | ` */` |
|       6 | 5414 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5415 |  |
|       - | 5416 | `	ph7_hashmap_node *pNode;` |
|       - | 5417 | `	ph7_hashmap *pMap;` |
|       7 | 5418 | `	int nItem = 1;` |
|       7 | 5419 | `	if( nArg < 1 ){` |
|       - | 5420 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5421 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5422 | `		return PH7_OK;` |
|       - | 5423 | `	}` |
|       - | 5424 | `	/* Make sure we are dealing with an array */` |
|       7 | 5425 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5426 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5427 | `		return PH7_OK;` |
|       - | 5428 | `	}` |
|       - | 5429 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5430 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5431 | `	if(pMap->nEntry < 1 ){` |
|       - | 5432 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5433 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5434 | `		return PH7_OK;` |
|       - | 5435 | `	}` |
|       7 | 5436 | `	if( nArg > 1 ){` |
|       3 | 5437 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5438 | `	}` |
|       7 | 5439 | `	if( nItem < 2 ){` |
|       - | 5440 | `		sxu32 nEntry;` |
|       - | 5441 | `		/* Select a random number */` |
|       5 | 5442 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5443 | `		/* Extract the desired entry.` |
|       - | 5444 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5445 | `		 */` |
|       5 | 5446 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       2 | 5447 | `			pNode = pMap->pLast;` |
|       2 | 5448 | `			nEntry = pMap->nEntry - nEntry;` |
|       2 | 5449 | `			if( nEntry > 1 ){` |
|     ! 0 | 5450 | `				for(;;){` |
|     ! 0 | 5451 | `					if( nEntry == 0 ){` |
|     ! 0 | 5452 | `						break;` |
|       - | 5453 | `					}` |
|       - | 5454 | `					/* Point to the previous entry */` |
|     ! 0 | 5455 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5456 | `					nEntry--;` |
|     ! 0 | 5457 | `				}` |
|     ! 0 | 5458 | `			}` |
|       1 | 5459 | `		}else{` |
|       4 | 5460 | `			pNode = pMap->pFirst;` |
|       3 | 5461 | `			for(;;){` |
|       5 | 5462 | `				if( nEntry == 0 ){` |
|       4 | 5463 | `					break;` |
|       - | 5464 | `				}` |
|       - | 5465 | `				/* Point to the next entry */` |
|       2 | 5466 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       2 | 5467 | `				nEntry--;` |
|       1 | 5468 | `			}` |
|       - | 5469 | `		}` |
|       5 | 5470 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5471 | `			/* Int key */` |
|       3 | 5472 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5473 | `		}else{` |
|       - | 5474 | `			/* Blob key */` |
|       3 | 5475 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5476 | `		}` |
|       3 | 5477 | `	}else{` |
|       - | 5478 | `		ph7_value sKey,*pArray;` |
|       - | 5479 | `		ph7_hashmap *pDest;` |
|       - | 5480 | `		/* Create a new array */` |
|       3 | 5481 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5482 | `		if( pArray == 0 ){` |
|     ! 0 | 5483 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5484 | `			return PH7_OK;` |
|       - | 5485 | `		}` |
|       - | 5486 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5487 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5488 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5489 | `		/* Copy the first n items */` |
|       3 | 5490 | `		pNode = pMap->pFirst;` |
|       3 | 5491 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5492 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5493 | `		}` |
|       7 | 5494 | `		while( nItem > 0){` |
|       5 | 5495 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5496 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5497 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5498 | `			/* Point to the next entry */` |
|       5 | 5499 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5500 | `			nItem--;` |
|       1 | 5501 | `		}` |
|       - | 5502 | `		/* Shuffle the array */` |
|       3 | 5503 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5504 | `		/* Rehash node */` |
|       3 | 5505 | `		HashmapSortRehash(pDest);` |
|       - | 5506 | `		/* Return the random array */` |
|       3 | 5507 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5508 | `	}` |
|       7 | 5509 | `	return PH7_OK;` |
|       4 | 5510 |  |
|       - | 5511 | `/*` |
|       - | 5512 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5513 | ` *  Split an array into chunks.` |
|       - | 5514 | ` * Parameters` |
|       - | 5515 | ` * $input` |
|       - | 5516 | ` *   The array to work on` |
|       - | 5517 | ` * $size` |
|       - | 5518 | ` *   The size of each chunk` |
|       - | 5519 | ` * $preserve_keys` |
|       - | 5520 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5521 | ` *   the chunk numerically.` |
|       - | 5522 | ` * Return` |
|       - | 5523 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5524 | ` *  zero, with each dimension containing size elements.` |
|       - | 5525 | ` */` |
|      42 | 5526 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5527 |  |
|       - | 5528 | `	ph7_value *pArray,*pChunk;` |
|       - | 5529 | `	ph7_hashmap_node *pEntry;` |
|       - | 5530 | `	ph7_hashmap *pMap;` |
|       - | 5531 | `	int bPreserve;` |
|       - | 5532 | `	sxu32 nChunk;` |
|       - | 5533 | `	sxu32 nSize;` |
|       - | 5534 | `	sxu32 n;` |
|       - | 5535 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5536 | `	if( nArg < 2 ){` |
|       - | 5537 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5538 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5539 | `			"ArgumentCountError",` |
|       - | 5540 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5541 | `			nArg` |
|       - | 5542 | `			);` |
|       - | 5543 | `	}` |
|      42 | 5544 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5545 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5546 | `			"TypeError",` |
|       - | 5547 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5548 | `			ph7_type_name(apArg[0])` |
|       - | 5549 | `			);` |
|       - | 5550 | `	}` |
|       - | 5551 | `	/* Create a new array */` |
|      40 | 5552 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5553 | `	if( pArray == 0 ){` |
|     ! 0 | 5554 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5555 | `		return PH7_OK;` |
|       - | 5556 | `	}` |
|       - | 5557 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5558 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5559 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5560 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5561 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5562 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5563 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5564 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5565 | `			"TypeError",` |
|       - | 5566 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5567 | `			ph7_type_name(apArg[1])` |
|       - | 5568 | `			);` |
|       - | 5569 | `	}` |
|       - | 5570 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5571 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5572 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5573 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5574 | `		int len;` |
|       3 | 5575 | `		sxu8 bReal = FALSE;` |
|       3 | 5576 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5577 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5578 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5579 | `				"TypeError",` |
|       - | 5580 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5581 | `				);` |
|       - | 5582 | `		}` |
|     ! 0 | 5583 | `		if( bReal ){` |
|       - | 5584 | `			/* float-string -> warn but allow */` |
|     ! 0 | 5585 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5586 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 5587 | `				zStr` |
|       - | 5588 | `				);` |
|     ! 0 | 5589 | `		}` |
|     ! 0 | 5590 | `	}` |
|       - | 5591 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 5592 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 5593 | `	 * later via ph7_value_to_int. */` |
|      38 | 5594 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5595 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5596 | `		sxi64 i = (sxi64)d;` |
|       3 | 5597 | `		if( d != (double)i ){` |
|       4 | 5598 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5599 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 5600 | `				d` |
|       - | 5601 | `				);` |
|       1 | 5602 | `		}` |
|       1 | 5603 | `	}` |
|       - | 5604 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5605 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5606 | `	{` |
|      38 | 5607 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 5608 | `		if( nSizeSigned < 1 ){` |
|       - | 5609 | `			/* size <= 0 -> ValueError */` |
|       5 | 5610 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5611 | `				"ValueError",` |
|       - | 5612 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5613 | `				);` |
|       - | 5614 | `		}` |
|      34 | 5615 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5616 | `	}` |
|      34 | 5617 | `	if( nSize >= pMap->nEntry ){` |
|       - | 5618 | `		/* Return the whole array */` |
|       3 | 5619 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 5620 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5621 | `		return PH7_OK;` |
|       - | 5622 | `	}` |
|      32 | 5623 | `	bPreserve = 0;` |
|      32 | 5624 | `	if( nArg > 2 ){` |
|       - | 5625 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5626 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5627 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5628 | `		 * normally, matching PHP behaviour. */` |
|      45 | 5629 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 5630 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 5631 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 5632 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5633 | `				"TypeError",` |
|       - | 5634 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 5635 | `				ph7_type_name(apArg[2])` |
|       - | 5636 | `				);` |
|       - | 5637 | `		}` |
|      21 | 5638 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 5639 | `	}` |
|       - | 5640 | `	/* Start processing */` |
|      27 | 5641 | `	pEntry = pMap->pFirst;` |
|      27 | 5642 | `	nChunk = 0;` |
|      27 | 5643 | `	pChunk = 0;` |
|      27 | 5644 | `	n = pMap->nEntry;` |
|      56 | 5645 | `	for( ;; ){` |
|     113 | 5646 | `		if( n < 1 ){` |
|       - | 5647 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5648 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5649 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5650 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5651 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5652 | `			 * exists. */` |
|      27 | 5653 | `			if( pChunk ){` |
|      27 | 5654 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 5655 | `			}` |
|      27 | 5656 | `			break;` |
|       - | 5657 | `		}` |
|      87 | 5658 | `		if( nChunk < 1 ){` |
|      71 | 5659 | `			if( pChunk ){` |
|       - | 5660 | `				/* Put the first chunk */` |
|      45 | 5661 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 5662 | `			}` |
|       - | 5663 | `			/* Create a new dimension */` |
|      71 | 5664 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5665 | `												   * will be automatically released as soon we return` |
|       - | 5666 | `												   * from this function */` |
|      71 | 5667 | `			if( pChunk == 0 ){` |
|     ! 0 | 5668 | `				break;` |
|       - | 5669 | `			}` |
|      71 | 5670 | `			nChunk = nSize;` |
|      35 | 5671 | `		}` |
|       - | 5672 | `		/* Insert the entry */` |
|      87 | 5673 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5674 | `		/* Point to the next entry */` |
|      87 | 5675 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 5676 | `		nChunk--;` |
|      87 | 5677 | `		n--;` |
|       1 | 5678 | `	}` |
|       - | 5679 | `	/* Return the multidimensional array */` |
|      27 | 5680 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5681 | `	return PH7_OK;` |
|      23 | 5682 |  |
|       - | 5683 | `/*` |
|       - | 5684 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5685 | ` *  Pad array to the specified length with a value.` |
|       - | 5686 | ` * $input` |
|       - | 5687 | ` *   Initial array of values to pad.` |
|       - | 5688 | ` * $pad_size` |
|       - | 5689 | ` *   New size of the array.` |
|       - | 5690 | ` * $pad_value` |
|       - | 5691 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5692 | ` */` |
|      28 | 5693 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5694 |  |
|       - | 5695 | `	ph7_hashmap *pMap;` |
|       - | 5696 | `	ph7_value *pArray;` |
|       - | 5697 | `	int nEntry;` |
|      30 | 5698 | `	if( nArg != 3 ){` |
|      10 | 5699 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5700 | `			"ArgumentCountError",` |
|       - | 5701 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 5702 | `			nArg` |
|       - | 5703 | `			);` |
|       - | 5704 | `	}` |
|      24 | 5705 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5706 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5707 | `			"TypeError",` |
|       - | 5708 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5709 | `			ph7_type_name(apArg[0])` |
|       - | 5710 | `			);` |
|       - | 5711 | `	}` |
|       - | 5712 | `	/* Create a new array */` |
|      21 | 5713 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 5714 | `	if( pArray == 0 ){` |
|     ! 0 | 5715 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5716 | `		return PH7_OK;` |
|       - | 5717 | `	}` |
|       - | 5718 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 5719 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5720 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 5721 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 5722 | `	if( nEntry < 0 ){` |
|       9 | 5723 | `		nEntry = -nEntry;` |
|       9 | 5724 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 5725 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5726 | `			/* Insert given items first */` |
|      17 | 5727 | `			while( nEntry > 0 ){` |
|      13 | 5728 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 5729 | `				nEntry--;` |
|       1 | 5730 | `			}` |
|       - | 5731 | `			/* Merge the two arrays */` |
|       5 | 5732 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5733 | `		}else{` |
|       5 | 5734 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5735 | `		}` |
|      17 | 5736 | `	}else if( nEntry > 0 ){` |
|      11 | 5737 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 5738 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5739 | `			/* Merge the two arrays first */` |
|       7 | 5740 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5741 | `			/* Insert given items */` |
|      25 | 5742 | `			while( nEntry > 0 ){` |
|      19 | 5743 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 5744 | `				nEntry--;` |
|       1 | 5745 | `			}` |
|       4 | 5746 | `		}else{` |
|       5 | 5747 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5748 | `		}` |
|       6 | 5749 | `	}else{` |
|       - | 5750 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 5751 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5752 | `	}` |
|       - | 5753 | `	/* Return the new array */` |
|      21 | 5754 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 5755 | `	return PH7_OK;` |
|      16 | 5756 |  |
|       - | 5757 | `/*` |
|       - | 5758 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5759 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5760 | ` * Parameters` |
|       - | 5761 | ` * $array` |
|       - | 5762 | ` *   The array in which elements are replaced.` |
|       - | 5763 | ` * $array1` |
|       - | 5764 | ` *   The array from which elements will be extracted.` |
|       - | 5765 | ` * ....` |
|       - | 5766 | ` *  More arrays from which elements will be extracted.` |
|       - | 5767 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5768 | ` * Return` |
|       - | 5769 | ` *  Returns an array, or NULL if an error occurs.` |
|       - | 5770 | ` */` |
|       2 | 5771 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5772 |  |
|       - | 5773 | `	ph7_hashmap *pMap;` |
|       - | 5774 | `	ph7_value *pArray;` |
|       - | 5775 | `	int i;` |
|       3 | 5776 | `	if( nArg < 1 ){` |
|       - | 5777 | `		/* Invalid arguments,return NULL */` |
|     ! 0 | 5778 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5779 | `		return PH7_OK;` |
|       - | 5780 | `	}` |
|       - | 5781 | `	/* Create a new array */` |
|       3 | 5782 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5783 | `	if( pArray == 0 ){` |
|     ! 0 | 5784 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5785 | `		return PH7_OK;` |
|       - | 5786 | `	}` |
|       - | 5787 | `	/* Perform the requested operation */` |
|       7 | 5788 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       5 | 5789 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|     ! 0 | 5790 | `			continue;` |
|       - | 5791 | `		}` |
|       - | 5792 | `		/* Point to the internal representation of the input hashmap */` |
|       5 | 5793 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       5 | 5794 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5795 | `	}` |
|       - | 5796 | `	/* Return the new array */` |
|       3 | 5797 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5798 | `	return PH7_OK;` |
|       2 | 5799 |  |
|       - | 5800 | `/*` |
|       - | 5801 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 5802 | ` *  Filters elements of an array using a callback function.` |
|       - | 5803 | ` * Parameters` |
|       - | 5804 | ` *  $input` |
|       - | 5805 | ` *    The array to iterate over` |
|       - | 5806 | ` * $callback` |
|       - | 5807 | ` *    The callback function to use` |
|       - | 5808 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 5809 | ` *    will be removed.` |
|       - | 5810 | ` * Return` |
|       - | 5811 | ` *  The filtered array.` |
|       - | 5812 | ` */` |
|      18 | 5813 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5814 |  |
|       - | 5815 | `	ph7_hashmap_node *pEntry;` |
|       - | 5816 | `	ph7_hashmap *pMap;` |
|       - | 5817 | `	ph7_value *pArray;` |
|       - | 5818 | `	ph7_value sResult;   /* Callback result */` |
|       - | 5819 | `	ph7_value *pValue;` |
|       - | 5820 | `	sxi32 rc;` |
|       - | 5821 | `	int keep;` |
|       - | 5822 | `	sxu32 n;` |
|      20 | 5823 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5824 | `		/* Invalid arguments,return NULL */` |
|       5 | 5825 | `		ph7_result_null(pCtx);` |
|       5 | 5826 | `		return PH7_OK;` |
|       - | 5827 | `	}` |
|       - | 5828 | `	/* Create a new array */` |
|      16 | 5829 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 5830 | `	if( pArray == 0 ){` |
|     ! 0 | 5831 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5832 | `		return PH7_OK;` |
|       - | 5833 | `	}` |
|       - | 5834 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 5835 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 5836 | `	pEntry = pMap->pFirst;` |
|      16 | 5837 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 5838 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5839 | `	/* Perform the requested operation */` |
|      66 | 5840 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5841 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 5842 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 5843 | `		if( pValue == 0 ){` |
|       - | 5844 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5845 | `			keep = FALSE;` |
|      54 | 5846 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5847 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 5848 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 5849 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 5850 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 5851 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5852 | `					int len;` |
|       3 | 5853 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 5854 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5855 | `						"TypeError",` |
|       - | 5856 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 5857 | `						zName` |
|       - | 5858 | `						);` |
|     ! 0 | 5859 | `				}else{` |
|     ! 0 | 5860 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5861 | `						"TypeError",` |
|       - | 5862 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 5863 | `						ph7_type_name(apArg[1])` |
|       - | 5864 | `						);` |
|       - | 5865 | `				}` |
|       - | 5866 | `			}` |
|      23 | 5867 | `			keep = FALSE;` |
|      23 | 5868 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 5869 | `			if( rc == SXRET_OK ){` |
|       - | 5870 | `				/* Perform a boolean cast */` |
|      23 | 5871 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 5872 | `			}` |
|      23 | 5873 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 5874 | `		}else{` |
|       - | 5875 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 5876 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 5877 | `			 * the case where the callback argument is missing entirely.` |
|       - | 5878 | `			 */` |
|      29 | 5879 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 5880 | `		}` |
|      51 | 5881 | `		if( keep ){` |
|       - | 5882 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 5883 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5884 | `		}` |
|       - | 5885 | `		/* Point to the next entry */` |
|      51 | 5886 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 5887 | `	}` |
|      13 | 5888 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5889 | `	return PH7_OK;` |
|      11 | 5890 |  |
|       - | 5891 | `/*` |
|       - | 5892 | ` * array array_map(callback $callback,array $arr1)` |
|       - | 5893 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5894 | ` * Parameters` |
|       - | 5895 | ` *  $callback` |
|       - | 5896 | ` *   Callback function to run for each element in each array.` |
|       - | 5897 | ` * $arr1` |
|       - | 5898 | ` *   An array to run through the callback function.` |
|       - | 5899 | ` * Return` |
|       - | 5900 | ` *  Returns an array containing all the elements of arr1 after applying` |
|       - | 5901 | ` *  the callback function to each one.` |
|       - | 5902 | ` * NOTE:` |
|       - | 5903 | ` *  array_map() passes only a single value to the callback.` |
|       - | 5904 | ` */` |
|      10 | 5905 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5906 |  |
|       - | 5907 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5908 | `	ph7_hashmap_node *pEntry;` |
|       - | 5909 | `	ph7_hashmap *pMap;` |
|       - | 5910 | `	sxu32 n;` |
|      11 | 5911 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 5912 | `		/* Invalid arguments,return NULL */` |
|       5 | 5913 | `		ph7_result_null(pCtx);` |
|       5 | 5914 | `		return PH7_OK;` |
|       - | 5915 | `	}` |
|       - | 5916 | `	/* Create a new array */` |
|       7 | 5917 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5918 | `	if( pArray == 0 ){` |
|     ! 0 | 5919 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5920 | `		return PH7_OK;` |
|       - | 5921 | `	}` |
|       - | 5922 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5923 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       7 | 5924 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       7 | 5925 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 5926 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 5927 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 5928 | `	/* Perform the requested operation */` |
|       7 | 5929 | `	pEntry = pMap->pFirst;` |
|      21 | 5930 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5931 | `		/* Extrcat the node value */` |
|      15 | 5932 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      15 | 5933 | `		if( pValue ){` |
|       - | 5934 | `			sxi32 rc;` |
|       - | 5935 | `			/* Invoke the supplied callback */` |
|      15 | 5936 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 5937 | `			/* Extract the node key */` |
|      15 | 5938 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      15 | 5939 | `			if( rc != SXRET_OK ){` |
|       - | 5940 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 5941 | `				ph7_array_add_elem(pArray,&sKey,pValue); /* Keep the same value */` |
|     ! 0 | 5942 | `			}else{` |
|       - | 5943 | `				/* Insert the callback return value */` |
|      15 | 5944 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 5945 | `			}` |
|      15 | 5946 | `			PH7_MemObjRelease(&sKey);` |
|      15 | 5947 | `			PH7_MemObjRelease(&sResult);` |
|       7 | 5948 | `		}` |
|       - | 5949 | `		/* Point to the next entry */` |
|      15 | 5950 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 5951 | `	}` |
|       7 | 5952 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 5953 | `	return PH7_OK;` |
|       6 | 5954 |  |
|       - | 5955 | `/*` |
|       - | 5956 | ` * value array_reduce(array $input,callback $function[, value $initial = NULL ])` |
|       - | 5957 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 5958 | ` * Parameters` |
|       - | 5959 | ` *  $input` |
|       - | 5960 | ` *   The input array.` |
|       - | 5961 | ` *  $function` |
|       - | 5962 | ` *  The callback function.` |
|       - | 5963 | ` * $initial` |
|       - | 5964 | ` *  If the optional initial is available, it will be used at the beginning` |
|       - | 5965 | ` *  of the process, or as a final result in case the array is empty.` |
|       - | 5966 | ` * Return` |
|       - | 5967 | ` *  Returns the resulting value.` |
|       - | 5968 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 5969 | ` */` |
|       4 | 5970 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5971 |  |
|       - | 5972 | `	ph7_hashmap_node *pEntry;` |
|       - | 5973 | `	ph7_hashmap *pMap;` |
|       - | 5974 | `	ph7_value *pValue;` |
|       - | 5975 | `	ph7_value sResult;` |
|       - | 5976 | `	sxu32 n;` |
|       5 | 5977 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5978 | `		/* Invalid/Missing arguments,return NULL */` |
|     ! 0 | 5979 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5980 | `		return PH7_OK;` |
|       - | 5981 | `	}` |
|       - | 5982 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 5983 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5984 | `	/* Assume a NULL initial value */` |
|       5 | 5985 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       5 | 5986 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       5 | 5987 | `	if( nArg > 2 ){` |
|       - | 5988 | `		/* Set the initial value */` |
|       5 | 5989 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       2 | 5990 | `	}` |
|       - | 5991 | `	/* Perform the requested operation */` |
|       5 | 5992 | `	pEntry = pMap->pFirst;` |
|      19 | 5993 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5994 | `		/* Extract the node value */` |
|      15 | 5995 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 5996 | `		/* Invoke the supplied callback */` |
|      15 | 5997 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 5998 | `		/* Point to the next entry */` |
|      15 | 5999 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 6000 | `	}` |
|       5 | 6001 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       5 | 6002 | `	PH7_MemObjRelease(&sResult);` |
|       5 | 6003 | `	return PH7_OK;` |
|       3 | 6004 |  |
|       - | 6005 | `/*` |
|       - | 6006 | ` * bool array_walk(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6007 | ` *  Apply a user function to every member of an array.` |
|       - | 6008 | ` * Parameters` |
|       - | 6009 | ` *  $array` |
|       - | 6010 | ` *   The input array.` |
|       - | 6011 | ` * $funcname` |
|       - | 6012 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6013 | ` *  the first, and the key/index second.` |
|       - | 6014 | ` * Note:` |
|       - | 6015 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6016 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6017 | ` *  be made in the original array itself.` |
|       - | 6018 | ` * $userdata` |
|       - | 6019 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6020 | ` *  to the callback funcname.` |
|       - | 6021 | ` * Return` |
|       - | 6022 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6023 | ` */` |
|      12 | 6024 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6025 |  |
|       - | 6026 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6027 | `	ph7_hashmap_node *pEntry;` |
|       - | 6028 | `	ph7_hashmap *pMap;` |
|       - | 6029 | `	sxi32 rc;` |
|       - | 6030 | `	sxu32 n;` |
|      13 | 6031 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6032 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6033 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6034 | `		return PH7_OK;` |
|       - | 6035 | `	}` |
|      13 | 6036 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6037 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6038 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 6039 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      13 | 6040 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6041 | `	/* Perform the desired operation */` |
|      13 | 6042 | `	pEntry = pMap->pFirst;` |
|      41 | 6043 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6044 | `		/* Extract the node value */` |
|      29 | 6045 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      29 | 6046 | `		if( pValue ){` |
|       - | 6047 | `			/* Extract the entry key */` |
|      29 | 6048 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6049 | `			/* Invoke the supplied callback */` |
|      29 | 6050 | `			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      29 | 6051 | `			PH7_MemObjRelease(&sKey);` |
|      29 | 6052 | `			if( rc != SXRET_OK ){` |
|       - | 6053 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 6054 | `				ph7_result_bool(pCtx,0); /* return FALSE */` |
|     ! 0 | 6055 | `				return PH7_OK;` |
|       - | 6056 | `			}` |
|      14 | 6057 | `		}` |
|       - | 6058 | `		/* Point to the next entry */` |
|      29 | 6059 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6060 | `	}` |
|       - | 6061 | `	/* All done,return TRUE */` |
|      13 | 6062 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6063 | `	return PH7_OK;` |
|       7 | 6064 |  |
|       - | 6065 | `/*` |
|       - | 6066 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6067 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6068 | ` */` |
|       6 | 6069 | `static int HashmapWalkRecursive(` |
|       - | 6070 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6071 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6072 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6073 | `	int iNest             /* Nesting level */` |
|       - | 6074 | `	)` |
|       1 | 6075 |  |
|       - | 6076 | `	ph7_hashmap_node *pEntry;` |
|       - | 6077 | `	ph7_value *pValue,sKey;` |
|       - | 6078 | `	sxi32 rc;` |
|       - | 6079 | `	sxu32 n;` |
|       - | 6080 | `	/* Iterate throw hashmap entries */` |
|       7 | 6081 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 6082 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 6083 | `	pEntry = pMap->pFirst;` |
|      17 | 6084 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6085 | `		/* Extract the node value */` |
|      11 | 6086 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      11 | 6087 | `		if( pValue ){` |
|      11 | 6088 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 6089 | `				if( iNest < 32 ){` |
|       - | 6090 | `					/* Recurse */` |
|       5 | 6091 | `					iNest++;` |
|       5 | 6092 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|       5 | 6093 | `					iNest--;` |
|       2 | 6094 | `				}` |
|       3 | 6095 | `			}else{` |
|       - | 6096 | `				/* Extract the node key */` |
|       7 | 6097 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6098 | `				/* Invoke the supplied callback */` |
|       7 | 6099 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|       7 | 6100 | `				PH7_MemObjRelease(&sKey);` |
|       7 | 6101 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6102 | `					return rc;` |
|       - | 6103 | `				}` |
|       - | 6104 | `			}` |
|       5 | 6105 | `		}` |
|       - | 6106 | `		/* Point to the next entry */` |
|      11 | 6107 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 6108 | `	}` |
|       7 | 6109 | `	return SXRET_OK;` |
|       4 | 6110 |  |
|       - | 6111 | `/*` |
|       - | 6112 | ` * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6113 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6114 | ` * Parameters` |
|       - | 6115 | ` *  $array` |
|       - | 6116 | ` *   The input array.` |
|       - | 6117 | ` * $funcname` |
|       - | 6118 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6119 | ` *  the first, and the key/index second.` |
|       - | 6120 | ` * Note:` |
|       - | 6121 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6122 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6123 | ` *  be made in the original array itself.` |
|       - | 6124 | ` * $userdata` |
|       - | 6125 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6126 | ` *  to the callback funcname.` |
|       - | 6127 | ` * Return` |
|       - | 6128 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6129 | ` */` |
|       2 | 6130 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6131 |  |
|       - | 6132 | `	ph7_hashmap *pMap;` |
|       - | 6133 | `	sxi32 rc;` |
|       3 | 6134 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6135 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6136 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6137 | `		return PH7_OK;` |
|       - | 6138 | `	}` |
|       - | 6139 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 6140 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6141 | `	/* Perform the desired operation */` |
|       3 | 6142 | `	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6143 | `	/* All done */` |
|       3 | 6144 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|       3 | 6145 | `	return PH7_OK;` |
|       2 | 6146 |  |
|       - | 6147 | `/*` |
|       - | 6148 | ` * Table of hashmap functions.` |
|       - | 6149 | ` */` |
|       - | 6150 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6151 | `	{"count",             ph7_hashmap_count },` |
|       - | 6152 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6153 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6154 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6155 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6156 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6157 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6158 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6159 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6160 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6161 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6162 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6163 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6164 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6165 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6166 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6167 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6168 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6169 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6170 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6171 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6172 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6173 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6174 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6175 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6176 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6177 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6178 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6179 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6180 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6181 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6182 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6183 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6184 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6185 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6186 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6187 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6188 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6189 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6190 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6191 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6192 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6193 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6194 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6195 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6196 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6197 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6198 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6199 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6200 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6201 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6202 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6203 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6204 | `	{"current",           ph7_hashmap_current },` |
|       - | 6205 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6206 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6207 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6208 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6209 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6210 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6211 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6212 | `};` |
|       - | 6213 | `/*` |
|       - | 6214 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6215 | ` */` |
|    1336 | 6216 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6217 |  |
|       - | 6218 | `	sxu32 n;` |
|   82834 | 6219 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|   81498 | 6220 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   40750 | 6221 | `	}` |
|    1338 | 6222 |  |
|       - | 6223 | `/*` |
|       - | 6224 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6225 | ` * the BLOB given as the first argument.` |
|       - | 6226 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6227 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6228 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6229 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6230 | ` */` |
|      28 | 6231 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6232 |  |
|       - | 6233 | `	ph7_hashmap_node *pEntry;` |
|       - | 6234 | `	ph7_value *pObj;` |
|      30 | 6235 | `	sxu32 n = 0;` |
|       - | 6236 | `	int isRef;` |
|       - | 6237 | `	sxi32 rc;` |
|       - | 6238 | `	int i;` |
|      30 | 6239 | `	if( nDepth > 31 ){` |
|       - | 6240 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6241 | `		/* Nesting limit reached */` |
|     ! 0 | 6242 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6243 | `		if( ShowType ){` |
|     ! 0 | 6244 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6245 | `		}` |
|     ! 0 | 6246 | `		return SXERR_LIMIT;` |
|       - | 6247 | `	}` |
|       - | 6248 | `	/* Point to the first inserted entry */` |
|      30 | 6249 | `	pEntry = pMap->pFirst;` |
|      30 | 6250 | `	rc = SXRET_OK;` |
|      30 | 6251 | `	if( !ShowType ){` |
|      15 | 6252 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6253 | `	}` |
|       - | 6254 | `	/* Total entries */` |
|      30 | 6255 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6256 | `#ifdef __WINNT__` |
|       2 | 6257 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6258 | `#else` |
|      28 | 6259 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6260 | `#endif` |
|      65 | 6261 | `	for(;;){` |
|     132 | 6262 | `		if( n >= pMap->nEntry ){` |
|      30 | 6263 | `			break;` |
|       - | 6264 | `		}` |
|     206 | 6265 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     104 | 6266 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      53 | 6267 | `		}` |
|       - | 6268 | `		/* Dump key */` |
|     104 | 6269 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      37 | 6270 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      19 | 6271 | `		}else{` |
|     101 | 6272 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6273 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6274 | `		}` |
|       - | 6275 | `#ifdef __WINNT__` |
|       2 | 6276 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6277 | `#else` |
|     102 | 6278 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6279 | `#endif` |
|       - | 6280 | `		/* Dump node value */` |
|     104 | 6281 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     104 | 6282 | `		isRef = 0;` |
|     104 | 6283 | `		if( pObj ){` |
|     104 | 6284 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6285 | `				/* Referenced object */` |
|     ! 0 | 6286 | `				isRef = 1;` |
|     ! 0 | 6287 | `			}` |
|     104 | 6288 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     104 | 6289 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6290 | `				break;` |
|       - | 6291 | `			}` |
|      51 | 6292 | `		}` |
|       - | 6293 | `		/* Point to the next entry */` |
|     104 | 6294 | `		n++;` |
|     104 | 6295 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6296 | `	}` |
|      58 | 6297 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      30 | 6298 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      16 | 6299 | `	}` |
|      30 | 6300 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      30 | 6301 | `	return rc;` |
|      16 | 6302 |  |
|       - | 6303 | `/*` |
|       - | 6304 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6305 | ` * retrieved entry.` |
|       - | 6306 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6307 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6308 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6309 | ` * a value different from PH7_OK.` |
|       - | 6310 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6311 | ` */` |
|   19210 | 6312 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6313 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6314 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6315 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6316 | `	)` |
|       2 | 6317 |  |
|       - | 6318 | `	ph7_hashmap_node *pEntry;` |
|       - | 6319 | `	ph7_value sKey,sValue;` |
|       - | 6320 | `	sxi32 rc;` |
|       - | 6321 | `	sxu32 n;` |
|       - | 6322 | `	/* Initialize walker parameter */` |
|   19212 | 6323 | `	rc = SXRET_OK;` |
|   19212 | 6324 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   19212 | 6325 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   19212 | 6326 | `	n = pMap->nEntry;` |
|   19212 | 6327 | `	pEntry = pMap->pFirst;` |
|       - | 6328 | `	/* Start the iteration process */` |
|   51323 | 6329 | `	for(;;){` |
|  102648 | 6330 | `		if( n < 1 ){` |
|   19212 | 6331 | `			break;` |
|       - | 6332 | `		}` |
|       - | 6333 | `		/* Extract a copy of the key and a copy the current value */` |
|   83438 | 6334 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   83438 | 6335 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6336 | `		/* Invoke the user callback */` |
|   83438 | 6337 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6338 | `		/* Release the copy of the key and the value */` |
|   83438 | 6339 | `		PH7_MemObjRelease(&sKey);` |
|   83438 | 6340 | `		PH7_MemObjRelease(&sValue);` |
|   83438 | 6341 | `		if( rc != PH7_OK ){` |
|       - | 6342 | `			/* Callback request an operation abort */` |
|     ! 0 | 6343 | `			return SXERR_ABORT;` |
|       - | 6344 | `		}` |
|       - | 6345 | `		/* Point to the next entry */` |
|   83438 | 6346 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   83438 | 6347 | `		n--;` |
|       2 | 6348 | `	}` |
|       - | 6349 | `	/* All done */` |
|   19212 | 6350 | `	return SXRET_OK;` |
|    9607 | 6351 |  |
|       - | 6352 |  |
