# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2620/3120 lines (83.97%)

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
| 2777558 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2777560 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  222598 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  222600 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  222600 |   29 | `	sxu32 nH = 5381;` |
|  222600 |   30 | `	zEnd = &zIn[nLen];` |
|  255670 |   31 | `	for(;;){` |
|  511342 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  459294 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  415204 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  336030 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  222600 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecurisve is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * If the nesting limit is reached,this function abort immediately.` |
|       - |   43 | ` */` |
|     790 |   44 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)` |
|       2 |   45 |  |
|     792 |   46 | `	sxi64 iCount = 0;` |
|     792 |   47 | `	if( !bRecursive ){` |
|     516 |   48 | `		iCount = pMap->nEntry;` |
|     259 |   49 | `	}else{` |
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
|     788 |   79 | `	return iCount;` |
|     397 |   80 |  |
|       - |   81 | `/*` |
|       - |   82 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   83 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   84 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   85 | ` */` |
| 2723370 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   87 |  |
|       - |   88 | `	ph7_hashmap_node *pNode;` |
|       - |   89 | `	/* Allocate a new node */` |
| 2723372 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2723372 |   91 | `	if( pNode == 0 ){` |
|     ! 0 |   92 | `		return 0;` |
|       - |   93 | `	}` |
|       - |   94 | `	/* Zero the stucture */` |
| 2723372 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |   96 | `	/* Fill in the structure */` |
| 2723372 |   97 | `	pNode->pMap  = &(*pMap);` |
| 2723372 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2723372 |   99 | `	pNode->nHash = nHash;` |
| 2723372 |  100 | `	pNode->xKey.iKey = iKey;` |
| 2723372 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 2723372 |  102 | `	return pNode;` |
| 1361687 |  103 |  |
|       - |  104 | `/*` |
|       - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  108 | ` */` |
|   77424 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  110 |  |
|       - |  111 | `	ph7_hashmap_node *pNode;` |
|       - |  112 | `	/* Allocate a new node */` |
|   77426 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   77426 |  114 | `	if( pNode == 0 ){` |
|     ! 0 |  115 | `		return 0;` |
|       - |  116 | `	}` |
|       - |  117 | `	/* Zero the stucture */` |
|   77426 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  119 | `	/* Fill in the structure */` |
|   77426 |  120 | `	pNode->pMap  = &(*pMap);` |
|   77426 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   77426 |  122 | `	pNode->nHash = nHash;` |
|   77426 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   77426 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   77426 |  125 | `	pNode->nValIdx = nValIdx;` |
|   77426 |  126 | `	return pNode;` |
|   38714 |  127 |  |
|       - |  128 | `/*` |
|       - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  130 | ` */` |
| 2800794 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  132 |  |
|       - |  133 | `	/* Link */` |
| 2800796 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2588338 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2588338 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1294168 |  137 | `	}` |
| 2800796 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  139 | `	/* Link to the map list */` |
| 2800796 |  140 | `	if( pMap->pFirst == 0 ){` |
|   35662 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  142 | `		/* Point to the first inserted node */` |
|   35662 |  143 | `		pMap->pCur = pNode;` |
|   17832 |  144 | `	}else{` |
| 2765136 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  146 | `	}` |
| 2800796 |  147 | `	++pMap->nEntry;` |
| 2800796 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Unlink a node from the hashmap.` |
|       - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  152 | ` */` |
|    5442 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  154 |  |
|    5444 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5444 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  157 | `	/* Unlink from the corresponding bucket */` |
|    5444 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|    5020 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2511 |  160 | `	}else{` |
|     425 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  162 | `	}` |
|    5444 |  163 | `	if( pNode->pNextCollide ){` |
|    4241 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2120 |  165 | `	}` |
|    5444 |  166 | `	if( pMap->pFirst == pNode ){` |
|      58 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|      28 |  168 | `	}` |
|    5444 |  169 | `	if( pMap->pCur == pNode ){` |
|       - |  170 | `		/* Advance the node cursor */` |
|      60 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      29 |  172 | `	}` |
|       - |  173 | `	/* Unlink from the map list */` |
|    5444 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5444 |  175 | `	if( bRestore ){` |
|       - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|      30 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  178 | `		/* Restore to the freelist */` |
|      30 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|      30 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      14 |  181 | `		}` |
|      14 |  182 | `	}` |
|    5444 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5395 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2697 |  185 | `	}` |
|    5444 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5444 |  187 | `	pMap->nEntry--;` |
|    5444 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  189 | `		/* Free the hash-bucket */` |
|      26 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      26 |  191 | `		pMap->apBucket = 0;` |
|      26 |  192 | `		pMap->nSize = 0;` |
|      26 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      12 |  194 | `	}` |
|    5444 |  195 |  |
|       - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  197 | `/*` |
|       - |  198 | ` * Grow the hash-table and rehash all entries.` |
|       - |  199 | ` */` |
| 2800794 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  201 |  |
| 2800796 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   39238 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   39238 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  206 | `		sxu32 nBucket;` |
|       - |  207 | `		sxu32 n;` |
|   39238 |  208 | `		if( nNew < 1 ){` |
|   35662 |  209 | `			nNew = 16;` |
|   17830 |  210 | `		}` |
|       - |  211 | `		/* Allocate a new bucket */` |
|   39238 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   39238 |  213 | `		if( apNew == 0 ){` |
|     ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|       - |  216 | `			}` |
|       - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  218 | `			return SXRET_OK;` |
|       - |  219 | `		}` |
|       - |  220 | `		/* Zero the table */` |
|   39238 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  222 | `		/* Reflect the change */` |
|   39238 |  223 | `		pMap->apBucket = apNew;` |
|   39238 |  224 | `		pMap->nSize = nNew;` |
|   39238 |  225 | `		if( apOld == 0 ){` |
|       - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   35662 |  227 | `			return SXRET_OK;` |
|       - |  228 | `		}` |
|       - |  229 | `		/* Rehash old entries */` |
|    3578 |  230 | `		pEntry = pMap->pFirst;` |
|    3578 |  231 | `		n = 0;` |
| 1914924 |  232 | `		for( ;; ){` |
| 3829850 |  233 | `			if( n >= pMap->nEntry ){` |
|    3578 |  234 | `				break;` |
|       - |  235 | `			}` |
|       - |  236 | `			/* Clear the old collision link */` |
| 3826274 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  238 | `			/* Link to the new bucket */` |
| 3826274 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3826274 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3388356 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3388356 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1694177 |  243 | `			}` |
| 3826274 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  245 | `			/* Point to the next entry */` |
| 3826274 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3826274 |  247 | `			n++;` |
|       2 |  248 | `		}` |
|       - |  249 | `		/* Free the old table */` |
|    3578 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1788 |  251 | `	}` |
| 2765136 |  252 | `	return SXRET_OK;` |
| 1400399 |  253 |  |
|       - |  254 | `/*` |
|       - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  256 | ` * hashmap.` |
|       - |  257 | ` */` |
| 2723370 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  259 |  |
|       - |  260 | `	ph7_hashmap_node *pNode;` |
|       - |  261 | `	sxu32 nIdx;` |
|       - |  262 | `	sxu32 nHash;` |
|       - |  263 | `	sxi32 rc;` |
| 2723372 |  264 | `	if( !isForeign ){` |
|       - |  265 | `		ph7_value *pObj;` |
|       - |  266 | `		/* Reserve a ph7_value for the value */` |
| 2723348 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2723348 |  268 | `		if( pObj == 0 ){` |
|     ! 0 |  269 | `			return SXERR_MEM;` |
|       - |  270 | `		}` |
| 2723348 |  271 | `		if( pValue ){` |
|       - |  272 | `			/* Duplicate the value */` |
| 2723348 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 1361673 |  274 | `		}` |
| 2723348 |  275 | `		nIdx = pObj->nIdx;` |
| 1361675 |  276 | `	}else{` |
|      25 |  277 | `		nIdx = nRefIdx;` |
|       - |  278 | `	}` |
|       - |  279 | `	/* Hash the key */` |
| 2723372 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  281 | `	/* Allocate a new int node */` |
| 2723372 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2723372 |  283 | `	if( pNode == 0 ){` |
|     ! 0 |  284 | `		return SXERR_MEM;` |
|       - |  285 | `	}` |
| 2723372 |  286 | `	if( isForeign ){` |
|       - |  287 | `		/* Mark as a foregin entry */` |
|      25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      12 |  289 | `	}` |
|       - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2723372 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2723372 |  292 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  294 | `		return rc;` |
|       - |  295 | `	}` |
|       - |  296 | `	/* Perform the insertion */` |
| 2723372 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  298 | `	/* Install in the reference table */` |
| 2723372 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  300 | `	/* All done */` |
| 2723372 |  301 | `	return SXRET_OK;` |
| 1361687 |  302 |  |
|       - |  303 | `/*` |
|       - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  305 | ` * hashmap.` |
|       - |  306 | ` */` |
|   77424 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  308 |  |
|       - |  309 | `	ph7_hashmap_node *pNode;` |
|       - |  310 | `	sxu32 nHash;` |
|       - |  311 | `	sxu32 nIdx;` |
|       - |  312 | `	sxi32 rc;` |
|   77426 |  313 | `	if( !isForeign ){` |
|       - |  314 | `		ph7_value *pObj;` |
|       - |  315 | `		/* Reserve a ph7_value for the value */` |
|   57930 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   57930 |  317 | `		if( pObj == 0 ){` |
|     ! 0 |  318 | `			return SXERR_MEM;` |
|       - |  319 | `		}` |
|   57930 |  320 | `		if( pValue ){` |
|       - |  321 | `			/* Duplicate the value */` |
|   57930 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|   28964 |  323 | `		}` |
|   57930 |  324 | `		nIdx = pObj->nIdx;` |
|   28966 |  325 | `	}else{` |
|   19498 |  326 | `		nIdx = nRefIdx;` |
|       - |  327 | `	}` |
|       - |  328 | `	/* Hash the key */` |
|   77426 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  330 | `	/* Allocate a new blob node */` |
|   77426 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   77426 |  332 | `	if( pNode == 0 ){` |
|     ! 0 |  333 | `		return SXERR_MEM;` |
|       - |  334 | `	}` |
|   77426 |  335 | `	if( isForeign ){` |
|       - |  336 | `		/* Mark as a foregin entry */` |
|   19498 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|    9748 |  338 | `	}` |
|       - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   77426 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   77426 |  341 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  343 | `		return rc;` |
|       - |  344 | `	}` |
|       - |  345 | `	/* Perform the insertion */` |
|   77426 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  347 | `	/* Install in the reference table */` |
|   77426 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  349 | `	/* All done */` |
|   77426 |  350 | `	return SXRET_OK;` |
|   38714 |  351 |  |
|       - |  352 | `/*` |
|       - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  356 | ` */` |
|   46702 |  357 | `static sxi32 HashmapLookupIntKey(` |
|       - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  359 | `	sxi64 iKey,                /* lookup key */` |
|       - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  361 | `	)` |
|       2 |  362 |  |
|       - |  363 | `	ph7_hashmap_node *pNode;` |
|       - |  364 | `	sxu32 nHash;` |
|   46704 |  365 | `	if( pMap->nEntry < 1 ){` |
|       - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|     365 |  367 | `		return SXERR_NOTFOUND;` |
|       - |  368 | `	}` |
|       - |  369 | `	/* Hash the key first */` |
|   46340 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  371 | `	/* Point to the appropriate bucket */` |
|   46340 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  373 | `	/* Perform the lookup */` |
|  411526 |  374 | `	for(;;){` |
|  823054 |  375 | `		if( pNode == 0 ){` |
|   45747 |  376 | `			break;` |
|       - |  377 | `		}` |
|  777602 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774288 |  379 | `			&& pNode->nHash == nHash` |
|  385933 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  381 | `				/* Node found */` |
|     594 |  382 | `				if( ppNode ){` |
|     582 |  383 | `					*ppNode = pNode;` |
|     290 |  384 | `				}` |
|     594 |  385 | `				return SXRET_OK;` |
|       - |  386 | `		}` |
|       - |  387 | `		/* Follow the collision link */` |
|  776715 |  388 | `		pNode = pNode->pNextCollide;` |
|       1 |  389 | `	}` |
|       - |  390 | `	/* No such entry */` |
|   45747 |  391 | `	return SXERR_NOTFOUND;` |
|   23353 |  392 |  |
|       - |  393 | `/*` |
|       - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  397 | ` */` |
|  153094 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  400 | `	const void *pKey,           /* Lookup key */` |
|       - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  403 | `	)` |
|       2 |  404 |  |
|       - |  405 | `	ph7_hashmap_node *pNode;` |
|       - |  406 | `	sxu32 nHash;` |
|  153096 |  407 | `	if( pMap->nEntry < 1 ){` |
|       - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|    7922 |  409 | `		return SXERR_NOTFOUND;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* Hash the key first */` |
|  145176 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  413 | `	/* Point to the appropriate bucket */` |
|  145176 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  415 | `	/* Perform the lookup */` |
|  145517 |  416 | `	for(;;){` |
|  291036 |  417 | `		if( pNode == 0 ){` |
|  109936 |  418 | `			break;` |
|       - |  419 | `		}` |
|  198720 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  179600 |  421 | `			&& pNode->nHash == nHash` |
|  106670 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   35242 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  424 | `				/* Node found */` |
|   35242 |  425 | `				if( ppNode ){` |
|   35214 |  426 | `					*ppNode = pNode;` |
|   17606 |  427 | `				}` |
|   35242 |  428 | `				return SXRET_OK;` |
|       - |  429 | `		}` |
|       - |  430 | `		/* Follow the collision link */` |
|  145862 |  431 | `		pNode = pNode->pNextCollide;` |
|       2 |  432 | `	}` |
|       - |  433 | `	/* No such entry */` |
|  109936 |  434 | `	return SXERR_NOTFOUND;` |
|   76549 |  435 |  |
|       - |  436 | `/*` |
|       - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  439 | ` */` |
|  153236 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  441 |  |
|  153238 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  153238 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  153238 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  445 | `		/* Octal not decimal number */` |
|       5 |  446 | `		return FALSE;` |
|       - |  447 | `	}` |
|  153234 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  449 | `		zIn++;` |
|     ! 0 |  450 | `	}` |
|   76949 |  451 | `	for(;;){` |
|  153900 |  452 | `		if( zIn >= zEnd ){` |
|     233 |  453 | `			return TRUE;` |
|       - |  454 | `		}` |
|  153668 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   76502 |  456 | `			break;` |
|       - |  457 | `		}` |
|     667 |  458 | `		zIn++;` |
|       1 |  459 | `	}` |
|       - |  460 | `	/* Key does not look like a decimal number */` |
|  153002 |  461 | `	return FALSE;` |
|   76620 |  462 |  |
|       - |  463 | `/*` |
|       - |  464 | ` * Check if a given key exists in the given hashmap.` |
|       - |  465 | ` * Write a pointer to the target node on success.` |
|       - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  467 | ` */` |
|   76168 |  468 | `static sxi32 HashmapLookup(` |
|       - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  472 | `	)` |
|       2 |  473 |  |
|   76170 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  475 | `	sxi32 rc;` |
|   76170 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   75660 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  478 | `			/* Force a string cast */` |
|     ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  480 | `		}` |
|   75660 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  482 | `			/* Perform a blob lookup */` |
|   75644 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   75644 |  484 | `			goto result;` |
|       - |  485 | `		}` |
|       8 |  486 | `	}` |
|       - |  487 | `	/* Perform an int lookup */` |
|     528 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  489 | `		/* Force an integer cast */` |
|      27 |  490 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  491 | `	}` |
|       - |  492 | `	/* Perform an int lookup */` |
|     528 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   38084 |  494 | `result:` |
|   76170 |  495 | `	if( rc == SXRET_OK ){` |
|       - |  496 | `		/* Node found */` |
|   35674 |  497 | `		if( ppNode ){` |
|   35650 |  498 | `			*ppNode = pNode;` |
|   17824 |  499 | `		}` |
|   35674 |  500 | `		return SXRET_OK;` |
|       - |  501 | `	}` |
|       - |  502 | `	/* No such entry */` |
|   40498 |  503 | `	return SXERR_NOTFOUND;` |
|   38086 |  504 |  |
|       - |  505 | `/*` |
|       - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  507 | ` * hashmap.` |
|       - |  508 | ` * If a node with the given key already exists in the database` |
|       - |  509 | ` * then this function overwrite the old value.` |
|       - |  510 | ` */` |
| 2781068 |  511 | `static sxi32 HashmapInsert(` |
|       - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  514 | `	ph7_value *pVal    /* Node value */` |
|       - |  515 | `	)` |
|       2 |  516 |  |
| 2781070 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 2781070 |  518 | `	sxi32 rc = SXRET_OK;` |
| 2781070 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   58116 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  521 | `			/* Force a string cast */` |
|       3 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  523 | `		}` |
|   58116 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  526 | `				/* Automatic index assign */` |
|      34 |  527 | `				pKey = 0;` |
|      16 |  528 | `			}` |
|     256 |  529 | `			goto IntKey;` |
|       - |  530 | `		}` |
|   86792 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   28930 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  533 | `				/* Overwrite the old value */` |
|       - |  534 | `				ph7_value *pElem;` |
|      25 |  535 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      25 |  536 | `				if( pElem ){` |
|      25 |  537 | `					if( pVal ){` |
|      25 |  538 | `						PH7_MemObjStore(pVal,pElem);` |
|      13 |  539 | `					}else{` |
|       - |  540 | `						/* Nullify the entry */` |
|     ! 0 |  541 | `						PH7_MemObjToNull(pElem);` |
|       - |  542 | `					}` |
|      12 |  543 | `				}` |
|      25 |  544 | `				return SXRET_OK;` |
|       - |  545 | `		}` |
|   57838 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  547 | `			/* Forbidden */` |
|       3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  549 | `			return SXRET_OK;` |
|       - |  550 | `		}` |
|       - |  551 | `		/* Perform a blob-key insertion */` |
|   57836 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   57836 |  553 | `		return rc;` |
|       - |  554 | `	}` |
| 1361477 |  555 | `IntKey:` |
| 2723210 |  556 | `	if( pKey ){` |
|   23193 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  558 | `			/* Force an integer cast */` |
|     251 |  559 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  560 | `		}` |
|   23193 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   23157 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  576 | `			/* Forbidden */` |
|       3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  578 | `			return SXRET_OK;` |
|       - |  579 | `		}` |
|       - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|   23155 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23155 |  582 | `		if( rc == SXRET_OK ){` |
|   23155 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  584 | `				/* Increment the automatic index */` |
|   22927 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  586 | `				/* Make sure the automatic index is not reserved */` |
|   22927 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  588 | `					pMap->iNextIdx++;` |
|     ! 0 |  589 | `				}` |
|   11463 |  590 | `			}` |
|   11577 |  591 | `		}` |
|   11578 |  592 | `	}else{` |
| 2700018 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  594 | `			/* Forbidden */` |
|       3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  596 | `			return SXRET_OK;` |
|       - |  597 | `		}` |
|       - |  598 | `		/* Assign an automatic index */` |
| 2700016 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2700016 |  600 | `		if( rc == SXRET_OK ){` |
| 2700016 |  601 | `			++pMap->iNextIdx;` |
| 1350007 |  602 | `		}` |
|       - |  603 | `	}` |
|       - |  604 | `	/* Insertion result */` |
| 2723170 |  605 | `	return rc;` |
| 1390536 |  606 |  |
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
|   19526 |  634 | `static sxi32 HashmapInsertByRef(` |
|       - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  638 | `	)` |
|       2 |  639 |  |
|   19528 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|   19528 |  641 | `	sxi32 rc = SXRET_OK;` |
|   19528 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   19504 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  644 | `			/* Force a string cast */` |
|     ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  646 | `		}` |
|   19504 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  649 | `				/* Automatic index assign */` |
|     ! 0 |  650 | `				pKey = 0;` |
|     ! 0 |  651 | `			}` |
|     ! 0 |  652 | `			goto IntKey;` |
|       - |  653 | `		}` |
|   29255 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    9751 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  656 | `				/* Overwrite */` |
|       7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|       - |  659 | `				/* Install in the reference table */` |
|       7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  661 | `				return SXRET_OK;` |
|       - |  662 | `		}` |
|       - |  663 | `		/* Perform a blob-key insertion */` |
|   19498 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   19498 |  665 | `		return rc;` |
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
|    9765 |  702 |  |
|       - |  703 | `/*` |
|       - |  704 | ` * Extract node value.` |
|       - |  705 | ` */` |
|  833333 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  707 |  |
|       - |  708 | `	/* Point to the desired object */` |
|       - |  709 | `	ph7_value *pObj;` |
|  833335 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  833335 |  711 | `	return pObj;` |
|       2 |  712 |  |
|       - |  713 | `/*` |
|       - |  714 | ` * Insert a node in the given hashmap.` |
|       - |  715 | ` * If a node with the given key already exists in the database` |
|       - |  716 | ` * then this function overwrite the old value.` |
|       - |  717 | ` */` |
|     314 |  718 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       1 |  719 |  |
|       - |  720 | `	ph7_value *pObj;` |
|       - |  721 | `	sxi32 rc;` |
|       - |  722 | `	/* Extract the node value */` |
|     315 |  723 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     315 |  724 | `	if( pObj == 0 ){` |
|     ! 0 |  725 | `		return SXERR_EMPTY;` |
|       - |  726 | `	}` |
|       - |  727 | `	/* Preserve key */` |
|     315 |  728 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  729 | `		/* Int64 key */` |
|     191 |  730 | `		if( !bPreserve ){` |
|       - |  731 | `			/* Assign an automatic index */` |
|      55 |  732 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      28 |  733 | `		}else{` |
|     137 |  734 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  735 | `		}` |
|      96 |  736 | `	}else{` |
|       - |  737 | `		/* Blob key */` |
|     125 |  738 | `		if( !bPreserve ){` |
|       - |  739 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  740 | `			 * original string key entirely */` |
|      33 |  741 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      17 |  742 | `		}else{` |
|     139 |  743 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      46 |  744 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  745 | `		}` |
|       - |  746 | `	}` |
|     315 |  747 | `	return rc;` |
|     158 |  748 |  |
|       - |  749 | `/*` |
|       - |  750 | ` * Compare two node values.` |
|       - |  751 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  752 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  753 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  754 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  755 | ` * documenation.` |
|       - |  756 | ` */` |
|   35664 |  757 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  758 |  |
|       - |  759 | `	ph7_value sObj1,sObj2;` |
|       - |  760 | `	sxi32 rc;` |
|   35666 |  761 | `	if( pLeft == pRight ){` |
|       - |  762 | `		/*` |
|       - |  763 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  764 | `		 * below for more information on this sceanario.` |
|       - |  765 | `		 */` |
|     ! 0 |  766 | `		return 0;` |
|       - |  767 | `	}` |
|       - |  768 | `	/* Do the comparison */` |
|   35666 |  769 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   35666 |  770 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   35666 |  771 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   35666 |  772 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   35666 |  773 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   35666 |  774 | `	PH7_MemObjRelease(&sObj1);` |
|   35666 |  775 | `	PH7_MemObjRelease(&sObj2);` |
|   35666 |  776 | `	return rc;` |
|   17884 |  777 |  |
|       - |  778 | `/*` |
|       - |  779 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  780 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  781 | ` */` |
|    7850 |  782 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  783 |  |
|    7852 |  784 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  785 | `	sxu32 nBucket;` |
|       - |  786 | `	/* Remove old collision links */` |
|    7852 |  787 | `	if( pEntry->pPrevCollide ){` |
|    6263 |  788 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    3124 |  789 | `	}else{` |
|    1591 |  790 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  791 | `	}` |
|    7852 |  792 | `	if( pEntry->pNextCollide ){` |
|     638 |  793 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     317 |  794 | `	}` |
|    7852 |  795 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  796 | `	/* Compute the new hash */` |
|    7852 |  797 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    7852 |  798 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    7852 |  799 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  800 | `	/* Link to the new bucket */` |
|    7852 |  801 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7852 |  802 | `	if( pMap->apBucket[nBucket] ){` |
|    6434 |  803 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3214 |  804 | `	}` |
|    7852 |  805 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7852 |  806 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  807 | `	/* Increment the automatic index */` |
|    7852 |  808 | `	pMap->iNextIdx++;` |
|    7852 |  809 |  |
|       - |  810 | `/*` |
|       - |  811 | ` * Perform a linear search on a given hashmap.` |
|       - |  812 | ` * Write a pointer to the target node on success.` |
|       - |  813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  814 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  815 | ` * for more information.` |
|       - |  816 | ` */` |
|   19976 |  817 | `static int HashmapFindValue(` |
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
|   19978 |  830 | `	pEntry = pMap->pFirst;` |
|   19978 |  831 | `	n = pMap->nEntry;` |
|   19978 |  832 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   19978 |  833 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   47759 |  834 | `	for(;;){` |
|   95519 |  835 | `		if( n < 1 ){` |
|      99 |  836 | `			break;` |
|       - |  837 | `		}` |
|       - |  838 | `		/* Extract node value */` |
|   95421 |  839 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|   95421 |  840 | `		if( pVal ){` |
|   95421 |  841 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|   95421 |  853 | `				PH7_MemObjLoad(pVal,&sVal);` |
|   95421 |  854 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|   95421 |  855 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|   95421 |  856 | `				PH7_MemObjRelease(&sVal);` |
|   95421 |  857 | `				PH7_MemObjRelease(&sNeedle);` |
|   95421 |  858 | `				if( rc == 0 ){` |
|   19880 |  859 | `					if( ppNode ){` |
|      23 |  860 | `						*ppNode = pEntry;` |
|      11 |  861 | `					}` |
|       - |  862 | `					/* Match found*/` |
|   19880 |  863 | `					return SXRET_OK;` |
|       - |  864 | `				}` |
|       - |  865 | `			}` |
|   37771 |  866 | `		}` |
|       - |  867 | `		/* Point to the next entry */` |
|   75543 |  868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   75543 |  869 | `		n--;` |
|       2 |  870 | `	}` |
|       - |  871 | `	/* No such entry */` |
|      99 |  872 | `	return SXERR_NOTFOUND;` |
|    9990 |  873 |  |
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
|  396414 | 1047 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1048 | `	ph7_hashmap *pDest,` |
|       - | 1049 | `	ph7_hashmap_node *pEntry,` |
|       - | 1050 | `	ph7_value *pVal,` |
|       - | 1051 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1052 | `	)` |
|       2 | 1053 |  |
|  396416 | 1054 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1055 | `	ph7_value sKey;` |
|       - | 1056 | `	sxi32 rc;` |
|       - | 1057 |  |
|  396416 | 1058 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1059 | `		/* Blob key insertion */` |
|      31 | 1060 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      31 | 1061 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 1062 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      31 | 1063 | `		PH7_MemObjRelease(&sKey);` |
|      16 | 1064 | `	}else{` |
|       - | 1065 | `		/* Int key */` |
|  396386 | 1066 | `		if( iAction == 0 ){ /* Merge */` |
|  396340 | 1067 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  198217 | 1068 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|       5 | 1069 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       5 | 1070 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|       5 | 1071 | `			PH7_MemObjRelease(&sKey);` |
|       3 | 1072 | `		}else{ /* Dup */` |
|      44 | 1073 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1074 | `		}` |
|       - | 1075 | `	}` |
|  396416 | 1076 | `	return rc;` |
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
|    1704 | 1090 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1091 |  |
|       - | 1092 | `	ph7_hashmap_node *pEntry;` |
|       - | 1093 | `	ph7_value *pVal;` |
|       - | 1094 | `	sxi32 rc;` |
|       - | 1095 | `	sxu32 n;` |
|    1706 | 1096 | `	if( pSrc == pDest ){` |
|       - | 1097 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1098 | `		 * Unlike the zend engine.` |
|       - | 1099 | `		 */` |
|     ! 0 | 1100 | `		return SXRET_OK;` |
|       - | 1101 | `	}` |
|       - | 1102 | `	/* Point to the first inserted entry in the source */` |
|    1706 | 1103 | `	pEntry = pSrc->pFirst;` |
|       - | 1104 | `	/* Perform the merge */` |
|  398060 | 1105 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1106 | `		/* Extract the node value */` |
|  396356 | 1107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  396356 | 1108 | `		if( pVal ){` |
|       - | 1109 | `			/* Make a local copy of the value.` |
|       - | 1110 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1111 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1112 | `			 * to the old pool.` |
|       - | 1113 | `			 */` |
|  396356 | 1114 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  198179 | 1115 | `		}else{` |
|     ! 0 | 1116 | `			rc = SXRET_OK;` |
|       - | 1117 | `		}` |
|  396356 | 1118 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1119 | `			return rc;` |
|       - | 1120 | `		}` |
|       - | 1121 | `		/* Point to the next entry */` |
|  396356 | 1122 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  198179 | 1123 | `	}` |
|    1706 | 1124 | `	return SXRET_OK;` |
|     854 | 1125 |  |
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
|      30 | 1175 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1176 |  |
|       - | 1177 | `	ph7_hashmap_node *pEntry;` |
|       - | 1178 | `	ph7_value *pVal;` |
|       - | 1179 | `	sxi32 rc;` |
|       - | 1180 | `	sxu32 n;` |
|      32 | 1181 | `	if( pSrc == pDest ){` |
|       - | 1182 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1183 | `		 * Unlike the zend engine.` |
|       - | 1184 | `		 */` |
|     ! 0 | 1185 | `		return SXRET_OK;` |
|       - | 1186 | `	}` |
|       - | 1187 | `	/* Point to the first inserted entry in the source */` |
|      32 | 1188 | `	pEntry = pSrc->pFirst;` |
|       - | 1189 | `	/* Perform the duplication */` |
|      84 | 1190 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1191 | `		/* Extract the node value */` |
|      54 | 1192 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      54 | 1193 | `		if( pVal ){` |
|      54 | 1194 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      28 | 1195 | `		}else{` |
|     ! 0 | 1196 | `			rc = SXRET_OK;` |
|       - | 1197 | `		}` |
|      54 | 1198 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1199 | `			return rc;` |
|       - | 1200 | `		}` |
|       - | 1201 | `		/* Point to the next entry */` |
|      54 | 1202 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      28 | 1203 | `	}` |
|      32 | 1204 | `	return SXRET_OK;` |
|      17 | 1205 |  |
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
|   52458 | 1297 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1298 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1299 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1300 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1301 | `	)` |
|       2 | 1302 |  |
|       - | 1303 | `	ph7_hashmap *pMap;` |
|       - | 1304 | `	/* Allocate a new instance */` |
|   52460 | 1305 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   52460 | 1306 | `	if( pMap == 0 ){` |
|     ! 0 | 1307 | `		return 0;` |
|       - | 1308 | `	}` |
|       - | 1309 | `	/* Zero the structure */` |
|   52460 | 1310 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1311 | `	/* Fill in the structure */` |
|   52460 | 1312 | `	pMap->pVm = &(*pVm);` |
|   52460 | 1313 | `	pMap->iRef = 1;` |
|       - | 1314 | `	/* Default hash functions */` |
|   52460 | 1315 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   52460 | 1316 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   52460 | 1317 | `	return pMap;` |
|   26231 | 1318 |  |
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
|    1424 | 1339 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    1426 | 1359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1426 | 1360 | `	if( pMap == 0 ){` |
|     ! 0 | 1361 | `		return SXERR_MEM;` |
|       - | 1362 | `	}` |
|    1426 | 1363 | `	pVm->pGlobal = pMap;` |
|       - | 1364 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1426 | 1365 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1426 | 1366 | `	if( pObj == 0 ){` |
|     ! 0 | 1367 | `		return SXERR_MEM;` |
|       - | 1368 | `	}` |
|    1426 | 1369 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1370 | `	/* Record object index */` |
|    1426 | 1371 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1372 | `	/* Install the special $GLOBALS array */` |
|    1426 | 1373 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1426 | 1374 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1375 | `		return rc;` |
|       - | 1376 | `	}` |
|       - | 1377 | `	/* Install superglobals now */` |
|   15666 | 1378 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1379 | `		ph7_value *pSuper;` |
|       - | 1380 | `		/* Request an empty array */` |
|   14242 | 1381 | `		pSuper = ph7_new_array(&(*pVm));` |
|   14242 | 1382 | `		if( pSuper == 0 ){` |
|     ! 0 | 1383 | `			return SXERR_MEM;` |
|       - | 1384 | `		}` |
|       - | 1385 | `		/* Install */` |
|   14242 | 1386 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   14242 | 1387 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1388 | `			return rc;` |
|       - | 1389 | `		}` |
|       - | 1390 | `		/* Release the value now it have been installed */` |
|   14242 | 1391 | `		ph7_release_value(&(*pVm),pSuper);` |
|    7122 | 1392 | `	}` |
|       - | 1393 | `	/* Set some $_SERVER entries */` |
|    1426 | 1394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1395 | `	/*` |
|       - | 1396 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1397 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1398 | `	 */` |
|    2846 | 1399 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1400 | `		"SCRIPT_FILENAME",` |
|     712 | 1401 | `		pFile ? pFile->zString : ":Memory:",` |
|    1420 | 1402 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1403 | `		);` |
|       - | 1404 | `	/* All done,all super-global are installed now */` |
|    1426 | 1405 | `	return SXRET_OK;` |
|     714 | 1406 |  |
|       - | 1407 | `/*` |
|       - | 1408 | ` * Release a hashmap.` |
|       - | 1409 | ` */` |
|   36750 | 1410 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1411 |  |
|       - | 1412 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   36752 | 1413 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1414 | `	sxu32 n;` |
|   36752 | 1415 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1416 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1417 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1418 | `		return SXRET_OK;` |
|       - | 1419 | `	}` |
|       - | 1420 | `	/* Start the release process */` |
|   36752 | 1421 | `	n = 0;` |
|   36752 | 1422 | `	pEntry = pMap->pFirst;` |
| 1406061 | 1423 | `	for(;;){` |
| 2812124 | 1424 | `		if( n >= pMap->nEntry ){` |
|   36752 | 1425 | `			break;` |
|       - | 1426 | `		}` |
| 2775374 | 1427 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1428 | `		/* Remove the reference from the foreign table */` |
| 2775374 | 1429 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2775374 | 1430 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1431 | `			/* Restore the ph7_value to the free list */` |
| 2775366 | 1432 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1387682 | 1433 | `		}` |
|       - | 1434 | `		/* Release the node */` |
| 2775374 | 1435 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   55870 | 1436 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   27934 | 1437 | `		}` |
| 2775374 | 1438 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1439 | `		/* Point to the next entry */` |
| 2775374 | 1440 | `		pEntry = pNext;` |
| 2775374 | 1441 | `		n++;` |
|       2 | 1442 | `	}` |
|   36752 | 1443 | `	if( pMap->nEntry > 0 ){` |
|       - | 1444 | `		/* Release the hash bucket */` |
|   32728 | 1445 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   16363 | 1446 | `	}` |
|   36752 | 1447 | `	if( FreeDS ){` |
|       - | 1448 | `		/* Free the whole instance */` |
|   36736 | 1449 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   18369 | 1450 | `	}else{` |
|       - | 1451 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1452 | `		pMap->apBucket = 0;` |
|      17 | 1453 | `		pMap->iNextIdx = 0;` |
|      17 | 1454 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1455 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1456 | `	}` |
|   36752 | 1457 | `	return SXRET_OK;` |
|   18377 | 1458 |  |
|       - | 1459 | `/*` |
|       - | 1460 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1461 | ` * If the count reaches zero which mean no more variables` |
|       - | 1462 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1463 | ` */` |
|  425134 | 1464 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1465 |  |
|  425136 | 1466 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1467 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  425136 | 1468 | `	pMap->iRef--;` |
|  425136 | 1469 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   36736 | 1470 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   18367 | 1471 | `	}` |
|  425136 | 1472 |  |
|       - | 1473 | `/*` |
|       - | 1474 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1475 | ` * Write a pointer to the target node on success.` |
|       - | 1476 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1477 | ` */` |
|   76176 | 1478 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1479 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1480 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1481 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1482 | `	)` |
|       2 | 1483 |  |
|       - | 1484 | `	sxi32 rc;` |
|   76178 | 1485 | `	if( pMap->nEntry < 1 ){` |
|       - | 1486 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1487 | `		 */` |
|       9 | 1488 | `		return SXERR_NOTFOUND;` |
|       - | 1489 | `	}` |
|   76170 | 1490 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   76170 | 1491 | `	return rc;` |
|   38090 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1495 | ` * hashmap.` |
|       - | 1496 | ` * If a node with the given key already exists in the database` |
|       - | 1497 | ` * then this function overwrite the old value.` |
|       - | 1498 | ` */` |
| 2384644 | 1499 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1500 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1501 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1502 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1503 | `	)` |
|       2 | 1504 |  |
|       - | 1505 | `	sxi32 rc;` |
| 2384646 | 1506 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1507 | `		/*` |
|       - | 1508 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1509 | `		 */` |
|     ! 0 | 1510 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1511 | `		return SXRET_OK;` |
|       - | 1512 | `	}` |
| 2384646 | 1513 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2384646 | 1514 | `	return rc;` |
| 1192324 | 1515 |  |
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
|   19526 | 1543 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1544 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1545 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1546 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1547 | `	)` |
|       2 | 1548 |  |
|       - | 1549 | `	sxi32 rc;` |
|   19528 | 1550 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1551 | `		/*` |
|       - | 1552 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1553 | `		 */` |
|     ! 0 | 1554 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1555 | `		return SXRET_OK;` |
|       - | 1556 | `	}` |
|   19528 | 1557 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   19528 | 1558 | `	return rc;` |
|    9765 | 1559 |  |
|       - | 1560 | `/*` |
|       - | 1561 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1562 | ` */` |
|   16364 | 1563 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1564 |  |
|       - | 1565 | `	/* Reset the loop cursor */` |
|   16366 | 1566 | `	pMap->pCur = pMap->pFirst;` |
|   16366 | 1567 |  |
|       - | 1568 | `/*` |
|       - | 1569 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1570 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1571 | ` * return NULL.` |
|       - | 1572 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1573 | ` */` |
|  133748 | 1574 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1575 |  |
|  133750 | 1576 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  133750 | 1577 | `	if( pCur == 0 ){` |
|       - | 1578 | `		/* End of the list,return null */` |
|    8186 | 1579 | `		return 0;` |
|       - | 1580 | `	}` |
|       - | 1581 | `	/* Advance the node cursor */` |
|  125566 | 1582 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  125566 | 1583 | `	return pCur;` |
|   66876 | 1584 |  |
|       - | 1585 | `/*` |
|       - | 1586 | ` * Extract a node value.` |
|       - | 1587 | ` */` |
|  317920 | 1588 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1589 |  |
|  317922 | 1590 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  317922 | 1591 | `	if( pEntry ){` |
|  317922 | 1592 | `		if( bStore ){` |
|  125620 | 1593 | `			PH7_MemObjStore(pEntry,pValue);` |
|   62811 | 1594 | `		}else{` |
|  192304 | 1595 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1596 | `		}` |
|  159062 | 1597 | `	}else{` |
|     ! 0 | 1598 | `		PH7_MemObjRelease(pValue);` |
|       - | 1599 | `	}` |
|  317922 | 1600 |  |
|       - | 1601 | `/*` |
|       - | 1602 | ` * Extract a node key.` |
|       - | 1603 | ` */` |
|   85710 | 1604 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1605 |  |
|       - | 1606 | `	/* Fill with the current key */` |
|   85712 | 1607 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   85562 | 1608 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      13 | 1609 | `			SyBlobRelease(&pKey->sBlob);` |
|       6 | 1610 | `		}` |
|   85562 | 1611 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   85562 | 1612 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   42782 | 1613 | `	}else{` |
|     151 | 1614 | `		SyBlobReset(&pKey->sBlob);` |
|     151 | 1615 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     151 | 1616 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1617 | `	}` |
|   85712 | 1618 |  |
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
|   23160 | 1666 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1667 |  |
|       - | 1668 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1669 | `    /* Prevent compiler warning */` |
|   23162 | 1670 | `	result.pNext = result.pPrev = 0;` |
|   23162 | 1671 | `	pTail = &result;` |
|   58872 | 1672 | `	while( pA && pB ){` |
|   35712 | 1673 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|   23384 | 1674 | `			pTail->pPrev = pA;` |
|   23384 | 1675 | `			pA->pNext = pTail;` |
|   23384 | 1676 | `			pTail = pA;` |
|   23384 | 1677 | `			pA = pA->pPrev;` |
|   11699 | 1678 | `		}else{` |
|   12330 | 1679 | `			pTail->pPrev = pB;` |
|   12330 | 1680 | `			pB->pNext = pTail;` |
|   12330 | 1681 | `			pTail = pB;` |
|   12330 | 1682 | `			pB = pB->pPrev;` |
|       - | 1683 | `		}` |
|       2 | 1684 | `	}` |
|   23162 | 1685 | `	if( pA ){` |
|   17205 | 1686 | `		pTail->pPrev = pA;` |
|   17205 | 1687 | `		pA->pNext = pTail;` |
|   14569 | 1688 | `	}else if( pB ){` |
|    5805 | 1689 | `		pTail->pPrev = pB;` |
|    5805 | 1690 | `		pB->pNext = pTail;` |
|    2895 | 1691 | `	}else{` |
|     156 | 1692 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1693 | `	}` |
|   23162 | 1694 | `	return result.pPrev;` |
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
|     522 | 1708 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1709 |  |
|       - | 1710 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1711 | `	sxu32 i;` |
|     524 | 1712 | `	SyZero(a,sizeof(a));` |
|       - | 1713 | `	/* Point to the first inserted entry */` |
|     524 | 1714 | `	pIn = pMap->pFirst;` |
|    8378 | 1715 | `	while( pIn ){` |
|    7856 | 1716 | `		p = pIn;` |
|    7856 | 1717 | `		pIn = p->pPrev;` |
|    7856 | 1718 | `		p->pPrev = 0;` |
|   14834 | 1719 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   14834 | 1720 | `			if( a[i]==0 ){` |
|    7856 | 1721 | `				a[i] = p;` |
|    7856 | 1722 | `				break;` |
|     ! 0 | 1723 | `			}else{` |
|    6980 | 1724 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    6980 | 1725 | `				a[i] = 0;` |
|       - | 1726 | `			}` |
|    3491 | 1727 | `		}` |
|    7856 | 1728 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1729 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1730 | `			 * But that is impossible.` |
|       - | 1731 | `			 */` |
|     ! 0 | 1732 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1733 | `		}` |
|       2 | 1734 | `	}` |
|     524 | 1735 | `	p = a[0];` |
|   16706 | 1736 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   16184 | 1737 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    8093 | 1738 | `	}` |
|     524 | 1739 | `	p->pNext = 0;` |
|       - | 1740 | `	/* Reflect the change */` |
|     524 | 1741 | `	pMap->pFirst = p;` |
|       - | 1742 | `	/* Reset the loop cursor */` |
|     524 | 1743 | `	pMap->pCur = pMap->pFirst;` |
|     524 | 1744 | `	return SXRET_OK;` |
|       2 | 1745 |  |
|       - | 1746 | `/*` |
|       - | 1747 | ` * Node comparison callback.` |
|       - | 1748 | ` * used-by: [sort(),asort(),...]` |
|       - | 1749 | ` */` |
|   35646 | 1750 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1751 |  |
|       - | 1752 | `	ph7_value sA,sB;` |
|       - | 1753 | `	sxi32 iFlags;` |
|       - | 1754 | `	int rc;` |
|   35648 | 1755 | `	if( pCmpData == 0 ){` |
|       - | 1756 | `		/* Perform a standard comparison */` |
|   35644 | 1757 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   35644 | 1758 | `		return rc;` |
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
|   17875 | 1784 |  |
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
|      16 | 1990 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1991 |  |
|       - | 1992 | `	sxu32 n;` |
|       8 | 1993 | `	SXUNUSED(pB); /* cc warning */` |
|       8 | 1994 | `	SXUNUSED(pCmpData);` |
|       - | 1995 | `	/* Grab a random number */` |
|      17 | 1996 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 1997 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 1998 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 1999 | `	 */` |
|      17 | 2000 | `	return n&1 ? 1 : -1;` |
|       1 | 2001 |  |
|       - | 2002 | `/*` |
|       - | 2003 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2004 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2005 | ` */` |
|     506 | 2006 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2007 |  |
|       - | 2008 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2009 | `	sxu32 i;` |
|       - | 2010 | `	/* Rehash all entries */` |
|     508 | 2011 | `	pLast = p = pMap->pFirst;` |
|     508 | 2012 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     508 | 2013 | `	i = 0;` |
|    4153 | 2014 | `	for( ;; ){` |
|    8308 | 2015 | `		if( i >= pMap->nEntry ){` |
|     508 | 2016 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     508 | 2017 | `			break;` |
|       - | 2018 | `		}` |
|    7802 | 2019 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2020 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2021 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2022 | `			/* Change key type */` |
|       5 | 2023 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2024 | `		}` |
|    7802 | 2025 | `		HashmapRehashIntNode(p);` |
|       - | 2026 | `		/* Point to the next entry */` |
|    7802 | 2027 | `		i++;` |
|    7802 | 2028 | `		pLast = p;` |
|    7802 | 2029 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2030 | `	}` |
|     508 | 2031 |  |
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
|     824 | 2053 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2054 |  |
|       - | 2055 | `	ph7_hashmap *pMap;` |
|       - | 2056 | `	/* Make sure we are dealing with a valid hashmap */` |
|     826 | 2057 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2060 | `		return PH7_OK;` |
|       - | 2061 | `	}` |
|       - | 2062 | `	/* Point to the internal representation of the input hashmap */` |
|     826 | 2063 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     826 | 2064 | `	if( pMap->nEntry > 1 ){` |
|     502 | 2065 | `		sxi32 iCmpFlags = 0;` |
|     502 | 2066 | `		if( nArg > 1 ){` |
|       - | 2067 | `			/* Extract comparison flags */` |
|       3 | 2068 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2069 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2070 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2071 | `			}` |
|       1 | 2072 | `		}` |
|       - | 2073 | `		/* Do the merge sort */` |
|     502 | 2074 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2075 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     502 | 2076 | `		HashmapSortRehash(pMap);` |
|     250 | 2077 | `	}` |
|       - | 2078 | `	/* All done,return TRUE */` |
|     826 | 2079 | `	ph7_result_bool(pCtx,1);` |
|     826 | 2080 | `	return PH7_OK;` |
|     414 | 2081 |  |
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
|       8 | 2476 | `		while(pMap->pLast->pPrev){` |
|       6 | 2477 | `			pMap->pLast = pMap->pLast->pPrev;` |
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
|     546 | 2498 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2499 |  |
|     548 | 2500 | `	int bRecursive = FALSE;` |
|       - | 2501 | `	sxi64 iCount;` |
|     548 | 2502 | `	if( nArg < 1 ){` |
|       - | 2503 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 2504 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 2505 | `		return PH7_OK;` |
|       - | 2506 | `	}` |
|     548 | 2507 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2508 | `		/* TICKET 1433-19: Handle objects */` |
|       3 | 2509 | `		int res = !ph7_value_is_null(apArg[0]);` |
|       3 | 2510 | `		ph7_result_int(pCtx,res);` |
|       3 | 2511 | `		return PH7_OK;` |
|       - | 2512 | `	}` |
|     546 | 2513 | `	if( nArg > 1 ){` |
|       - | 2514 | `		/* Recursive count? */` |
|      31 | 2515 | `		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;` |
|      15 | 2516 | `	}` |
|       - | 2517 | `	/* Count */` |
|     546 | 2518 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);` |
|     546 | 2519 | `	ph7_result_int64(pCtx,iCount);` |
|     546 | 2520 | `	return PH7_OK;` |
|     275 | 2521 |  |
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
|       - | 3103 | ` * array array_values(array $array)` |
|       - | 3104 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3105 | ` * Parameters` |
|       - | 3106 | ` *  $array` |
|       - | 3107 | ` *   The input array.` |
|       - | 3108 | ` * Return` |
|       - | 3109 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3110 | ` */` |
|      34 | 3111 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3112 |  |
|       - | 3113 | `	ph7_hashmap_node *pNode;` |
|       - | 3114 | `	ph7_hashmap *pMap;` |
|       - | 3115 | `	ph7_value *pArray;` |
|       - | 3116 | `	ph7_value *pObj;` |
|       - | 3117 | `	sxu32 n;` |
|      36 | 3118 | `	if( nArg != 1 ){` |
|       - | 3119 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3120 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3121 | `			"ArgumentCountError",` |
|       - | 3122 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3123 | `			nArg` |
|       - | 3124 | `			);` |
|       - | 3125 | `	}` |
|       - | 3126 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 3127 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3128 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3129 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3130 | `			"TypeError",` |
|       - | 3131 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3132 | `			ph7_type_name(apArg[0])` |
|       - | 3133 | `			);` |
|       - | 3134 | `	}` |
|       - | 3135 | `	/* Point to the internal representation that describe the input hashmap */` |
|      29 | 3136 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3137 | `	/* Create a new array */` |
|      29 | 3138 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3139 | `	if( pArray == 0 ){` |
|     ! 0 | 3140 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3141 | `		return PH7_OK;` |
|       - | 3142 | `	}` |
|       - | 3143 | `	/* Perform the requested operation */` |
|      29 | 3144 | `	pNode = pMap->pFirst;` |
|      97 | 3145 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      69 | 3146 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      69 | 3147 | `		if( pObj ){` |
|       - | 3148 | `			/* perform the insertion */` |
|      69 | 3149 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      34 | 3150 | `		}` |
|       - | 3151 | `		/* Point to the next entry */` |
|      69 | 3152 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      35 | 3153 | `	}` |
|       - | 3154 | `	/* return the new array */` |
|      29 | 3155 | `	ph7_result_value(pCtx,pArray);` |
|      29 | 3156 | `	return PH7_OK;` |
|      19 | 3157 |  |
|       - | 3158 | `/*` |
|       - | 3159 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3160 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3161 | ` * Parameters` |
|       - | 3162 | ` *  $input` |
|       - | 3163 | ` *   An array containing keys to return.` |
|       - | 3164 | ` * $search_value` |
|       - | 3165 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3166 | ` * $strict` |
|       - | 3167 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3168 | ` * Return` |
|       - | 3169 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3170 | ` */` |
|     116 | 3171 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3172 |  |
|       - | 3173 | `	ph7_hashmap_node *pNode;` |
|       - | 3174 | `	ph7_hashmap *pMap;` |
|       - | 3175 | `	ph7_value *pArray;` |
|       - | 3176 | `	ph7_value sObj;` |
|       - | 3177 | `	ph7_value sVal;` |
|       - | 3178 | `	SyString sKey;` |
|       - | 3179 | `	int bStrict;` |
|       - | 3180 | `	sxi32 rc;` |
|       - | 3181 | `	sxu32 n;` |
|     118 | 3182 | `	if( nArg < 1 ){` |
|       - | 3183 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3184 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3185 | `			"ArgumentCountError",` |
|       - | 3186 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3187 | `			);` |
|       - | 3188 | `	}` |
|       - | 3189 | `	/* Make sure we are dealing with a valid hashmap */` |
|     116 | 3190 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3191 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3192 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3193 | `			"TypeError",` |
|       - | 3194 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3195 | `			ph7_type_name(apArg[0])` |
|       - | 3196 | `			);` |
|       - | 3197 | `	}` |
|       - | 3198 | `	/* Point to the internal representation of the input hashmap */` |
|     114 | 3199 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3200 | `	/* Create a new array */` |
|     114 | 3201 | `	pArray = ph7_context_new_array(pCtx);` |
|     114 | 3202 | `	if( pArray == 0 ){` |
|     ! 0 | 3203 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3204 | `		return PH7_OK;` |
|       - | 3205 | `	}` |
|     114 | 3206 | `	bStrict = FALSE;` |
|     114 | 3207 | `	if( nArg > 2 ){` |
|       - | 3208 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3209 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3210 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3211 | `				"TypeError",` |
|       - | 3212 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3213 | `				ph7_type_name(apArg[2])` |
|       - | 3214 | `				);` |
|       - | 3215 | `		}` |
|       5 | 3216 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3217 | `	}` |
|       - | 3218 | `	/* Perform the requested operation */` |
|     111 | 3219 | `	pNode = pMap->pFirst;` |
|     111 | 3220 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     541 | 3221 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     431 | 3222 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     109 | 3223 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      55 | 3224 | `		}else{` |
|     323 | 3225 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3226 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3227 | `		}` |
|     431 | 3228 | `		rc = 0;` |
|     431 | 3229 | `		if( nArg > 1 ){` |
|      31 | 3230 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3231 | `			if( pValue ){` |
|      31 | 3232 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3233 | `				/* Filter key */` |
|      31 | 3234 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3235 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3236 | `			}` |
|      15 | 3237 | `		}` |
|     431 | 3238 | `		if( rc == 0 ){` |
|       - | 3239 | `			/* Perform the insertion */` |
|     413 | 3240 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     206 | 3241 | `		}` |
|     431 | 3242 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3243 | `		/* Point to the next entry */` |
|     431 | 3244 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     216 | 3245 | `	}` |
|       - | 3246 | `	/* return the new array */` |
|     111 | 3247 | `	ph7_result_value(pCtx,pArray);` |
|     111 | 3248 | `	return PH7_OK;` |
|      60 | 3249 |  |
|       - | 3250 | `/*` |
|       - | 3251 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3252 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3253 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3254 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3255 | ` * Parameters` |
|       - | 3256 | ` *  $arr1` |
|       - | 3257 | ` *   First array` |
|       - | 3258 | ` *  $arr2` |
|       - | 3259 | ` *   Second array` |
|       - | 3260 | ` * Return` |
|       - | 3261 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3262 | ` * Note` |
|       - | 3263 | ` *  This function is a symisc eXtension.` |
|       - | 3264 | ` */` |
|       4 | 3265 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3266 |  |
|       - | 3267 | `	ph7_hashmap *p1,*p2;` |
|       - | 3268 | `	int rc;` |
|       5 | 3269 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3270 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3271 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3272 | `		return PH7_OK;` |
|       - | 3273 | `	}` |
|       - | 3274 | `	/* Point to the hashmaps */` |
|       5 | 3275 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3276 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3277 | `	rc = (p1 == p2);` |
|       - | 3278 | `	/* Same instance? */` |
|       5 | 3279 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3280 | `	return PH7_OK;` |
|       3 | 3281 |  |
|       - | 3282 | `/*` |
|       - | 3283 | ` * array array_merge(array ...$arrays)` |
|       - | 3284 | ` *  Merge one or more arrays.` |
|       - | 3285 | ` * Parameters` |
|       - | 3286 | ` *  ...$arrays` |
|       - | 3287 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3288 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3289 | ` * Return` |
|       - | 3290 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3291 | ` *  with no arguments.` |
|       - | 3292 | ` */` |
|     852 | 3293 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3294 |  |
|       - | 3295 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3296 | `	ph7_value *pArray;` |
|       - | 3297 | `	int i;` |
|       - | 3298 | `	/* Create a new array */` |
|     854 | 3299 | `	pArray = ph7_context_new_array(pCtx);` |
|     854 | 3300 | `	if( pArray == 0 ){` |
|     ! 0 | 3301 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3302 | `		return PH7_OK;` |
|       - | 3303 | `	}` |
|       - | 3304 | `	/* Point to the internal representation of the hashmap */` |
|     854 | 3305 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3306 | `	/* Start merging */` |
|    2548 | 3307 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3308 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1700 | 3309 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3310 | `			/* Type mismatch -> TypeError */` |
|       7 | 3311 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3312 | `				"TypeError",` |
|       - | 3313 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3314 | `				i + 1,` |
|       4 | 3315 | `				ph7_type_name(apArg[i])` |
|       - | 3316 | `				);` |
|     ! 0 | 3317 | `		}else{` |
|    1696 | 3318 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3319 | `			/* Merge the two hashmaps */` |
|    1696 | 3320 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3321 | `		}` |
|     849 | 3322 | `	}` |
|       - | 3323 | `	/* Return the freshly created array */` |
|     850 | 3324 | `	ph7_result_value(pCtx,pArray);` |
|     850 | 3325 | `	return PH7_OK;` |
|     428 | 3326 |  |
|       - | 3327 | `/*` |
|       - | 3328 | ` * array array_copy(array $source)` |
|       - | 3329 | ` *  Make a blind copy of the target array.` |
|       - | 3330 | ` * Parameters` |
|       - | 3331 | ` *  $source` |
|       - | 3332 | ` *   Target array` |
|       - | 3333 | ` * Return` |
|       - | 3334 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3335 | ` * Note` |
|       - | 3336 | ` *  This function is a symisc eXtension.` |
|       - | 3337 | ` */` |
|      16 | 3338 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3339 |  |
|       - | 3340 | `	ph7_hashmap *pMap;` |
|       - | 3341 | `	ph7_value *pArray;` |
|      17 | 3342 | `	if( nArg < 1 ){` |
|       - | 3343 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3344 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3345 | `		return PH7_OK;` |
|       - | 3346 | `	}` |
|       - | 3347 | `	/* Create a new array */` |
|      17 | 3348 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3349 | `	if( pArray == 0 ){` |
|     ! 0 | 3350 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3351 | `		return PH7_OK;` |
|       - | 3352 | `	}` |
|       - | 3353 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3354 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3355 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3356 | `		/* Point to the internal representation of the source */` |
|      17 | 3357 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3358 | `		/* Perform the copy */` |
|      17 | 3359 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3360 | `	}else{` |
|       - | 3361 | `		/* Simple insertion */` |
|     ! 0 | 3362 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3363 | `	}` |
|       - | 3364 | `	/* Return the duplicated array */` |
|      17 | 3365 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3366 | `	return PH7_OK;` |
|       9 | 3367 |  |
|       - | 3368 | `/*` |
|       - | 3369 | ` * bool array_erase(array $source)` |
|       - | 3370 | ` *  Remove all elements from a given array.` |
|       - | 3371 | ` * Parameters` |
|       - | 3372 | ` *  $source` |
|       - | 3373 | ` *   Target array` |
|       - | 3374 | ` * Return` |
|       - | 3375 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3376 | ` * Note` |
|       - | 3377 | ` *  This function is a symisc eXtension.` |
|       - | 3378 | ` */` |
|      16 | 3379 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3380 |  |
|       - | 3381 | `	ph7_hashmap *pMap;` |
|      17 | 3382 | `	if( nArg < 1 ){` |
|       - | 3383 | `		/* Missing arguments */` |
|     ! 0 | 3384 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3385 | `		return PH7_OK;` |
|       - | 3386 | `	}` |
|       - | 3387 | `	/* Point to the target hashmap */` |
|      17 | 3388 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3389 | `	/* Erase */` |
|      17 | 3390 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3391 | `	return PH7_OK;` |
|       9 | 3392 |  |
|       - | 3393 | `/*` |
|       - | 3394 | ` * array array_slice(array $array,int $offset [,int $length [, bool $preserve_keys = false ]])` |
|       - | 3395 | ` *  Extract a slice of the array.` |
|       - | 3396 | ` * Parameters` |
|       - | 3397 | ` *  $array` |
|       - | 3398 | ` *    The input array.` |
|       - | 3399 | ` * $offset` |
|       - | 3400 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3401 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3402 | ` * $length (optional)` |
|       - | 3403 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3404 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3405 | ` *   elements from the end of the array. If it is omitted, then the sequence will have` |
|       - | 3406 | ` *   everything from offset up until the end of the array.` |
|       - | 3407 | ` * $preserve_keys (optional)` |
|       - | 3408 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3409 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3410 | ` * Return` |
|       - | 3411 | ` *   The new slice.` |
|       - | 3412 | ` */` |
|       8 | 3413 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3414 |  |
|       - | 3415 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3416 | `	ph7_hashmap_node *pCur;` |
|       - | 3417 | `	ph7_value *pArray;` |
|       - | 3418 | `	int iLength,iOfft;` |
|       - | 3419 | `	int bPreserve;` |
|       - | 3420 | `	sxi32 rc;` |
|       9 | 3421 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3422 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3423 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3424 | `		return PH7_OK;` |
|       - | 3425 | `	}` |
|       - | 3426 | `	/* Point the internal representation of the target array */` |
|       9 | 3427 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 3428 | `	bPreserve = FALSE;` |
|       - | 3429 | `	/* Get the offset */` |
|       9 | 3430 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       9 | 3431 | `	if( iOfft < 0 ){` |
|       3 | 3432 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       1 | 3433 | `	}` |
|       9 | 3434 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3435 | `		/* Invalid offset,return the last entry */` |
|     ! 0 | 3436 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3437 | `	}` |
|       - | 3438 | `	/* Get the length */` |
|       9 | 3439 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       9 | 3440 | `	if( nArg > 2 ){` |
|       7 | 3441 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       7 | 3442 | `		if( iLength < 0 ){` |
|     ! 0 | 3443 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3444 | `		}` |
|       7 | 3445 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3446 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3447 | `		}` |
|       7 | 3448 | `		if( nArg > 3 && ph7_value_is_bool(apArg[3]) ){` |
|       3 | 3449 | `			bPreserve = ph7_value_to_bool(apArg[3]);` |
|       1 | 3450 | `		}` |
|       3 | 3451 | `	}` |
|       - | 3452 | `	/* Create a new array */` |
|       9 | 3453 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 3454 | `	if( pArray == 0 ){` |
|     ! 0 | 3455 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3456 | `		return PH7_OK;` |
|       - | 3457 | `	}` |
|       9 | 3458 | `	if( iLength < 1 ){` |
|       - | 3459 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3460 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3461 | `		return PH7_OK;` |
|       - | 3462 | `	}` |
|       - | 3463 | `	/* Point to the desired entry */` |
|       9 | 3464 | `	pCur = pSrc->pFirst;` |
|       9 | 3465 | `	for(;;){` |
|      19 | 3466 | `		if( iOfft < 1 ){` |
|       9 | 3467 | `			break;` |
|       - | 3468 | `		}` |
|       - | 3469 | `		/* Point to the next entry */` |
|      11 | 3470 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      11 | 3471 | `		iOfft--;` |
|       1 | 3472 | `	}` |
|       - | 3473 | `	/* Point to the internal representation of the hashmap */` |
|       9 | 3474 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      12 | 3475 | `	for(;;){` |
|      25 | 3476 | `		if( iLength < 1 ){` |
|       9 | 3477 | `			break;` |
|       - | 3478 | `		}` |
|      17 | 3479 | `		rc = HashmapInsertNode(pMap,pCur,bPreserve);` |
|      17 | 3480 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3481 | `			break;` |
|       - | 3482 | `		}` |
|       - | 3483 | `		/* Point to the next entry */` |
|      17 | 3484 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      17 | 3485 | `		iLength--;` |
|       1 | 3486 | `	}` |
|       - | 3487 | `	/* Return the freshly created array */` |
|       9 | 3488 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 3489 | `	return PH7_OK;` |
|       5 | 3490 |  |
|       - | 3491 | `/*` |
|       - | 3492 | ` * array array_splice(array $array,int $offset [,int $length [,value $replacement ]])` |
|       - | 3493 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3494 | ` * Parameters` |
|       - | 3495 | ` *  $array` |
|       - | 3496 | ` *    The input array.` |
|       - | 3497 | ` * $offset` |
|       - | 3498 | ` *    If offset is positive then the start of removed portion is at that offset from` |
|       - | 3499 | ` *    the beginning of the input array. If offset is negative then it starts that far` |
|       - | 3500 | ` *    from the end of the input array.` |
|       - | 3501 | ` * $length (optional)` |
|       - | 3502 | ` *    If length is omitted, removes everything from offset to the end of the array.` |
|       - | 3503 | ` *    If length is specified and is positive, then that many elements will be removed.` |
|       - | 3504 | ` *    If length is specified and is negative then the end of the removed portion will` |
|       - | 3505 | ` *    be that many elements from the end of the array.` |
|       - | 3506 | ` * $replacement (optional)` |
|       - | 3507 | ` *  If replacement array is specified, then the removed elements are replaced` |
|       - | 3508 | ` *  with elements from this array.` |
|       - | 3509 | ` *  If offset and length are such that nothing is removed, then the elements` |
|       - | 3510 | ` *  from the replacement array are inserted in the place specified by the offset.` |
|       - | 3511 | ` *  Note that keys in replacement array are not preserved.` |
|       - | 3512 | ` *  If replacement is just one element it is not necessary to put array() around` |
|       - | 3513 | ` *  it, unless the element is an array itself, an object or NULL.` |
|       - | 3514 | ` * Return` |
|       - | 3515 | ` *   A new array consisting of the extracted elements.` |
|       - | 3516 | ` */` |
|       2 | 3517 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3518 |  |
|       - | 3519 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode;` |
|       - | 3520 | `	ph7_value *pArray,*pRvalue,*pOld;` |
|       - | 3521 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3522 | `	int iLength,iOfft;` |
|       - | 3523 | `	sxi32 rc;` |
|       3 | 3524 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3525 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3526 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3527 | `		return PH7_OK;` |
|       - | 3528 | `	}` |
|       - | 3529 | `	/* Point the internal representation of the target array */` |
|       3 | 3530 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3531 | `	/* Get the offset */` |
|       3 | 3532 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       3 | 3533 | `	if( iOfft < 0 ){` |
|     ! 0 | 3534 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|     ! 0 | 3535 | `	}` |
|       3 | 3536 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3537 | `		/* Invalid offset,remove the last entry */` |
|     ! 0 | 3538 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3539 | `	}` |
|       - | 3540 | `	/* Get the length */` |
|       3 | 3541 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       3 | 3542 | `	if( nArg > 2 ){` |
|       3 | 3543 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       3 | 3544 | `		if( iLength < 0 ){` |
|     ! 0 | 3545 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3546 | `		}` |
|       3 | 3547 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3548 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3549 | `		}` |
|       1 | 3550 | `	}` |
|       - | 3551 | `	/* Create a new array */` |
|       3 | 3552 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3553 | `	if( pArray == 0 ){` |
|     ! 0 | 3554 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3555 | `		return PH7_OK;` |
|       - | 3556 | `	}` |
|       3 | 3557 | `	if( iLength < 1 ){` |
|       - | 3558 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3559 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3560 | `		return PH7_OK;` |
|       - | 3561 | `	}` |
|       - | 3562 | `	/* Point to the desired entry */` |
|       3 | 3563 | `	pCur = pSrc->pFirst;` |
|       2 | 3564 | `	for(;;){` |
|       5 | 3565 | `		if( iOfft < 1 ){` |
|       3 | 3566 | `			break;` |
|       - | 3567 | `		}` |
|       - | 3568 | `		/* Point to the next entry */` |
|       3 | 3569 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       3 | 3570 | `		iOfft--;` |
|       1 | 3571 | `	}` |
|       3 | 3572 | `	pRep = 0;` |
|       3 | 3573 | `	if( nArg > 3 ){` |
|       3 | 3574 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3575 | `			/* Perform an array cast */` |
|     ! 0 | 3576 | `			PH7_MemObjToHashmap(apArg[3]);` |
|     ! 0 | 3577 | `			if(ph7_value_is_array(apArg[3])){` |
|     ! 0 | 3578 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|     ! 0 | 3579 | `			}` |
|     ! 0 | 3580 | `		}else{` |
|       3 | 3581 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3582 | `		}` |
|       3 | 3583 | `		if( pRep ){` |
|       - | 3584 | `			/* Reset the loop cursor */` |
|       3 | 3585 | `			pRep->pCur = pRep->pFirst;` |
|       1 | 3586 | `		}` |
|       1 | 3587 | `	}` |
|       - | 3588 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3589 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3590 | `	for(;;){` |
|       7 | 3591 | `		if( iLength < 1 ){` |
|       3 | 3592 | `			break;` |
|       - | 3593 | `		}` |
|       5 | 3594 | `		pPrev = pCur->pPrev;` |
|       5 | 3595 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|       5 | 3596 | `		if( pRep && (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|       - | 3597 | `			/* Extract node value */` |
|       5 | 3598 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|       - | 3599 | `			/* Replace the old node */` |
|       5 | 3600 | `			pOld = HashmapExtractNodeValue(pCur);` |
|       5 | 3601 | `			if( pRvalue && pOld ){` |
|       5 | 3602 | `				PH7_MemObjStore(pRvalue,pOld);` |
|       2 | 3603 | `			}` |
|       3 | 3604 | `		}else{` |
|       - | 3605 | `			/* Unlink the node from the source hashmap */` |
|     ! 0 | 3606 | `			PH7_HashmapUnlinkNode(pCur,TRUE);` |
|       - | 3607 | `		}` |
|       5 | 3608 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3609 | `			break;` |
|       - | 3610 | `		}` |
|       - | 3611 | `		/* Point to the next entry */` |
|       5 | 3612 | `		pCur = pPrev; /* Reverse link */` |
|       5 | 3613 | `		iLength--;` |
|       1 | 3614 | `	}` |
|       3 | 3615 | `	if( pRep ){` |
|       3 | 3616 | `		while((pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|     ! 0 | 3617 | `			HashmapInsertNode(pSrc,pRnode,FALSE);` |
|     ! 0 | 3618 | `		}` |
|       1 | 3619 | `	}` |
|       - | 3620 | `	/* Return the freshly created array */` |
|       3 | 3621 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3622 | `	return PH7_OK;` |
|       2 | 3623 |  |
|       - | 3624 | `/*` |
|       - | 3625 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3626 | ` *  Checks if a value exists in an array.` |
|       - | 3627 | ` * Parameters` |
|       - | 3628 | ` *  $needle` |
|       - | 3629 | ` *   The searched value.` |
|       - | 3630 | ` *   Note:` |
|       - | 3631 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3632 | ` * $haystack` |
|       - | 3633 | ` *  The target array.` |
|       - | 3634 | ` * $strict` |
|       - | 3635 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3636 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3637 | ` */` |
|   19784 | 3638 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3639 |  |
|       - | 3640 | `	ph7_value *pNeedle;` |
|       - | 3641 | `	int bStrict;` |
|       - | 3642 | `	int rc;` |
|   19786 | 3643 | `	if( nArg < 2 ){` |
|       - | 3644 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3645 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3646 | `		return PH7_OK;` |
|       - | 3647 | `	}` |
|   19786 | 3648 | `	pNeedle = apArg[0];` |
|   19786 | 3649 | `	bStrict = 0;` |
|   19786 | 3650 | `	if( nArg > 2 ){` |
|       5 | 3651 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3652 | `	}` |
|   19786 | 3653 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3654 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3655 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3656 | `		/* Set the comparison result */` |
|     ! 0 | 3657 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3658 | `		return PH7_OK;` |
|       - | 3659 | `	}` |
|       - | 3660 | `	/* Perform the lookup */` |
|   19786 | 3661 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3662 | `	/* Lookup result */` |
|   19786 | 3663 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   19786 | 3664 | `	return PH7_OK;` |
|    9894 | 3665 |  |
|       - | 3666 | `/*` |
|       - | 3667 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3668 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3669 | ` * Parameters` |
|       - | 3670 | ` * $needle` |
|       - | 3671 | ` *   The searched value.` |
|       - | 3672 | ` * $haystack` |
|       - | 3673 | ` *   The array.` |
|       - | 3674 | ` * $strict` |
|       - | 3675 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3676 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3677 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3678 | ` * Return` |
|       - | 3679 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3680 | ` */` |
|      28 | 3681 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3682 |  |
|       - | 3683 | `	ph7_hashmap_node *pEntry;` |
|       - | 3684 | `	ph7_value *pVal,sNeedle;` |
|       - | 3685 | `	ph7_hashmap *pMap;` |
|       - | 3686 | `	ph7_value sVal;` |
|       - | 3687 | `	int bStrict;` |
|       - | 3688 | `	sxu32 n;` |
|       - | 3689 | `	int rc;` |
|      30 | 3690 | `	if( nArg < 2 ){` |
|       - | 3691 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3692 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3693 | `			"ArgumentCountError",` |
|       - | 3694 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 3695 | `			nArg` |
|       - | 3696 | `			);` |
|       - | 3697 | `	}` |
|      26 | 3698 | `	bStrict = FALSE;` |
|      26 | 3699 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3700 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3701 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3702 | `			"TypeError",` |
|       - | 3703 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 3704 | `			ph7_type_name(apArg[1])` |
|       - | 3705 | `			);` |
|       - | 3706 | `	}` |
|      24 | 3707 | `	if( nArg > 2 ){` |
|       - | 3708 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 3709 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3710 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3711 | `				"TypeError",` |
|       - | 3712 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3713 | `				ph7_type_name(apArg[2])` |
|       - | 3714 | `				);` |
|       - | 3715 | `		}` |
|       9 | 3716 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 3717 | `	}` |
|       - | 3718 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 3719 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3720 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 3721 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 3722 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 3723 | `	pEntry = pMap->pFirst;` |
|      21 | 3724 | `	n = pMap->nEntry;` |
|      23 | 3725 | `	for(;;){` |
|      47 | 3726 | `		if( !n ){` |
|       9 | 3727 | `			break;` |
|       - | 3728 | `		}` |
|       - | 3729 | `		/* Extract node value */` |
|      39 | 3730 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 3731 | `		if( pVal ){` |
|       - | 3732 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3733 | `			 * can change their type.` |
|       - | 3734 | `			 */` |
|      39 | 3735 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 3736 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 3737 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 3738 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 3739 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 3740 | `			if( rc == 0 ){` |
|       - | 3741 | `				/* Match found,return key */` |
|      13 | 3742 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3743 | `					/* INT key */` |
|       7 | 3744 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 3745 | `				}else{` |
|       7 | 3746 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3747 | `					/* Blob key */` |
|       7 | 3748 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3749 | `				}` |
|      13 | 3750 | `				return PH7_OK;` |
|       - | 3751 | `			}` |
|      13 | 3752 | `		}` |
|       - | 3753 | `		/* Point to the next entry */` |
|      27 | 3754 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 3755 | `		n--;` |
|       1 | 3756 | `	}` |
|       - | 3757 | `	/* No such value,return FALSE */` |
|       9 | 3758 | `	ph7_result_bool(pCtx,0);` |
|       9 | 3759 | `	return PH7_OK;` |
|      16 | 3760 |  |
|       - | 3761 | `/*` |
|       - | 3762 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3763 | ` *  Computes the difference of arrays.` |
|       - | 3764 | ` * Parameters` |
|       - | 3765 | ` *  $array1` |
|       - | 3766 | ` *    The array to compare from` |
|       - | 3767 | ` *  $array2` |
|       - | 3768 | ` *    An array to compare against` |
|       - | 3769 | ` *  $...` |
|       - | 3770 | ` *   More arrays to compare against` |
|       - | 3771 | ` * Return` |
|       - | 3772 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3773 | ` *  are not present in any of the other arrays.` |
|       - | 3774 | ` */` |
|      22 | 3775 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3776 |  |
|       - | 3777 | `	ph7_hashmap_node *pEntry;` |
|       - | 3778 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3779 | `	ph7_value *pArray;` |
|       - | 3780 | `	ph7_value *pVal;` |
|       - | 3781 | `	sxi32 rc;` |
|       - | 3782 | `	sxu32 n;` |
|       - | 3783 | `	int i;` |
|       - | 3784 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3785 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3786 | `	 * debugging difficult. */` |
|      24 | 3787 | `	if( nArg < 1 ){` |
|       4 | 3788 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3789 | `			"ArgumentCountError",` |
|       - | 3790 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3791 | `			nArg` |
|       - | 3792 | `			);` |
|       - | 3793 | `	}` |
|      22 | 3794 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3795 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3796 | `			"TypeError",` |
|       - | 3797 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3798 | `			ph7_type_name(apArg[0])` |
|       - | 3799 | `			);` |
|       - | 3800 | `	}` |
|      36 | 3801 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3802 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3803 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3804 | `				"TypeError",` |
|       - | 3805 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3806 | `				i + 1,` |
|       2 | 3807 | `				ph7_type_name(apArg[i])` |
|       - | 3808 | `				);` |
|       - | 3809 | `		}` |
|       9 | 3810 | `	}` |
|      17 | 3811 | `	if( nArg == 1 ){` |
|       - | 3812 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3813 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3814 | `		return PH7_OK;` |
|       - | 3815 | `	}` |
|       - | 3816 | `	/* Create a new array */` |
|      15 | 3817 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3818 | `	if( pArray == 0 ){` |
|     ! 0 | 3819 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3820 | `		return PH7_OK;` |
|       - | 3821 | `	}` |
|       - | 3822 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 3823 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3824 | `	/* Perform the diff */` |
|      15 | 3825 | `	pEntry = pSrc->pFirst;` |
|      15 | 3826 | `	n = pSrc->nEntry;` |
|      27 | 3827 | `	for(;;){` |
|      55 | 3828 | `		if( n < 1 ){` |
|      15 | 3829 | `			break;` |
|       - | 3830 | `		}` |
|       - | 3831 | `		/* Extract the node value */` |
|      41 | 3832 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 3833 | `		if( pVal ){` |
|      69 | 3834 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3835 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 3836 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3837 | `				/* Perform the lookup */` |
|      45 | 3838 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 3839 | `				if( rc == SXRET_OK ){` |
|       - | 3840 | `					/* Value exist */` |
|      17 | 3841 | `					break;` |
|       - | 3842 | `				}` |
|      15 | 3843 | `			}` |
|      41 | 3844 | `			if( i >= nArg ){` |
|       - | 3845 | `				/* Perform the insertion */` |
|      25 | 3846 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 3847 | `			}` |
|      20 | 3848 | `		}` |
|       - | 3849 | `		/* Point to the next entry */` |
|      41 | 3850 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 3851 | `		n--;` |
|       1 | 3852 | `	}` |
|       - | 3853 | `	/* Return the freshly created array */` |
|      15 | 3854 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3855 | `	return PH7_OK;` |
|      13 | 3856 |  |
|       - | 3857 | `/*` |
|       - | 3858 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 3859 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 3860 | ` * Parameters` |
|       - | 3861 | ` *  $array1` |
|       - | 3862 | ` *    The array to compare from` |
|       - | 3863 | ` *  $array2` |
|       - | 3864 | ` *    An array to compare against` |
|       - | 3865 | ` *  $...` |
|       - | 3866 | ` *   More arrays to compare against.` |
|       - | 3867 | ` * $callback` |
|       - | 3868 | ` *  The callback comparison function.` |
|       - | 3869 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3870 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3871 | ` *  than the second.` |
|       - | 3872 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3873 | ` * Return` |
|       - | 3874 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3875 | ` *  are not present in any of the other arrays.` |
|       - | 3876 | ` */` |
|       2 | 3877 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3878 |  |
|       - | 3879 | `	ph7_hashmap_node *pEntry;` |
|       - | 3880 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3881 | `	ph7_value *pCallback;` |
|       - | 3882 | `	ph7_value *pArray;` |
|       - | 3883 | `	ph7_value *pVal;` |
|       - | 3884 | `	sxi32 rc;` |
|       - | 3885 | `	sxu32 n;` |
|       - | 3886 | `	int i;` |
|       3 | 3887 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3888 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3889 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3890 | `		return PH7_OK;` |
|       - | 3891 | `	}` |
|       - | 3892 | `	/* Point to the callback */` |
|       3 | 3893 | `	pCallback = apArg[nArg - 1];` |
|       3 | 3894 | `	if( nArg == 2 ){` |
|       - | 3895 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3896 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3897 | `		return PH7_OK;` |
|       - | 3898 | `	}` |
|       - | 3899 | `	/* Create a new array */` |
|       3 | 3900 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3901 | `	if( pArray == 0 ){` |
|     ! 0 | 3902 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3903 | `		return PH7_OK;` |
|       - | 3904 | `	}` |
|       - | 3905 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 3906 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3907 | `	/* Perform the diff */` |
|       3 | 3908 | `	pEntry = pSrc->pFirst;` |
|       3 | 3909 | `	n = pSrc->nEntry;` |
|       4 | 3910 | `	for(;;){` |
|       9 | 3911 | `		if( n < 1 ){` |
|       3 | 3912 | `			break;` |
|       - | 3913 | `		}` |
|       - | 3914 | `		/* Extract the node value */` |
|       7 | 3915 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 3916 | `		if( pVal ){` |
|      11 | 3917 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 3918 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3919 | `					/* ignore */` |
|     ! 0 | 3920 | `					continue;` |
|       - | 3921 | `				}` |
|       - | 3922 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 3923 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3924 | `				/* Perform the lookup */` |
|       7 | 3925 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 3926 | `				if( rc == SXRET_OK ){` |
|       - | 3927 | `					/* Value exist */` |
|       3 | 3928 | `					break;` |
|       - | 3929 | `				}` |
|       3 | 3930 | `			}` |
|       7 | 3931 | `			if( i >= (nArg - 1)){` |
|       - | 3932 | `				/* Perform the insertion */` |
|       5 | 3933 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 3934 | `			}` |
|       3 | 3935 | `		}` |
|       - | 3936 | `		/* Point to the next entry */` |
|       7 | 3937 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 3938 | `		n--;` |
|       1 | 3939 | `	}` |
|       - | 3940 | `	/* Return the freshly created array */` |
|       3 | 3941 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3942 | `	return PH7_OK;` |
|       2 | 3943 |  |
|       - | 3944 | `/*` |
|       - | 3945 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 3946 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 3947 | ` * Parameters` |
|       - | 3948 | ` *  $array1` |
|       - | 3949 | ` *    The array to compare from` |
|       - | 3950 | ` *  $array2` |
|       - | 3951 | ` *    An array to compare against` |
|       - | 3952 | ` *  $...` |
|       - | 3953 | ` *   More arrays to compare against` |
|       - | 3954 | ` * Return` |
|       - | 3955 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3956 | ` *  are not present in any of the other arrays.` |
|       - | 3957 | ` */` |
|      20 | 3958 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3959 |  |
|       - | 3960 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 3961 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3962 | `	ph7_value *pArray;` |
|       - | 3963 | `	ph7_value *pVal;` |
|       - | 3964 | `	sxi32 rc;` |
|       - | 3965 | `	sxu32 n;` |
|       - | 3966 | `	int i;` |
|       - | 3967 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 3968 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 3969 | `	 * accompanying integration tests to pass. */` |
|      22 | 3970 | `	if( nArg < 1 ){` |
|       4 | 3971 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3972 | `			"ArgumentCountError",` |
|       - | 3973 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 3974 | `			nArg` |
|       - | 3975 | `			);` |
|       - | 3976 | `	}` |
|      20 | 3977 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3978 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3979 | `			"TypeError",` |
|       - | 3980 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3981 | `			ph7_type_name(apArg[0])` |
|       - | 3982 | `			);` |
|       - | 3983 | `	}` |
|      32 | 3984 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3985 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 3986 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3987 | `				"TypeError",` |
|       - | 3988 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 3989 | `				i + 1,` |
|       4 | 3990 | `				ph7_type_name(apArg[i])` |
|       - | 3991 | `				);` |
|       - | 3992 | `		}` |
|       9 | 3993 | `	}` |
|      13 | 3994 | `	if( nArg == 1 ){` |
|       - | 3995 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3996 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3997 | `		return PH7_OK;` |
|       - | 3998 | `	}` |
|       - | 3999 | `	/* Create a new array */` |
|      11 | 4000 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4001 | `	if( pArray == 0 ){` |
|     ! 0 | 4002 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4003 | `		return PH7_OK;` |
|       - | 4004 | `	}` |
|       - | 4005 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4006 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4007 | `	/* Perform the diff */` |
|      11 | 4008 | `	pEntry = pSrc->pFirst;` |
|      11 | 4009 | `	n = pSrc->nEntry;` |
|      11 | 4010 | `	pN1 = pN2 = 0;` |
|      29 | 4011 | `	for(;;){` |
|       - | 4012 | `		int keep;` |
|      35 | 4013 | `		if( n < 1 ){` |
|      11 | 4014 | `			break;` |
|       - | 4015 | `		}` |
|       - | 4016 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4017 | `		keep = 1;` |
|      41 | 4018 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4019 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4020 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4021 | `			/* Perform a key lookup first */` |
|      29 | 4022 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4023 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4024 | `			}else{` |
|      17 | 4025 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4026 | `			}` |
|      29 | 4027 | `			if( rc != SXRET_OK ){` |
|       - | 4028 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4029 | `				continue;` |
|       - | 4030 | `			}` |
|       - | 4031 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4032 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4033 | `			if( pVal ){` |
|       - | 4034 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4035 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4036 | `				if( pVal2 ){` |
|      15 | 4037 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4038 | `					if( cmp == 0 ){` |
|       - | 4039 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4040 | `						keep = 0;` |
|      13 | 4041 | `						break;` |
|       - | 4042 | `					}` |
|       1 | 4043 | `				}` |
|       1 | 4044 | `			}` |
|       2 | 4045 | `		}` |
|      25 | 4046 | `		if( keep ){` |
|       - | 4047 | `			/* Perform the insertion */` |
|      13 | 4048 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4049 | `		}` |
|       - | 4050 | `		/* Point to the next entry */` |
|      25 | 4051 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4052 | `		n--;` |
|       1 | 4053 | `	}` |
|       - | 4054 | `	/* Return the freshly created array */` |
|      11 | 4055 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4056 | `	return PH7_OK;` |
|      12 | 4057 |  |
|       - | 4058 | `/*` |
|       - | 4059 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4060 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4061 | ` *  by a user supplied callback function.` |
|       - | 4062 | ` * Parameters` |
|       - | 4063 | ` *  $array1` |
|       - | 4064 | ` *    The array to compare from` |
|       - | 4065 | ` *  $array2` |
|       - | 4066 | ` *    An array to compare against` |
|       - | 4067 | ` *  $...` |
|       - | 4068 | ` *   More arrays to compare against.` |
|       - | 4069 | ` *  $key_compare_func` |
|       - | 4070 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4071 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4072 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4073 | ` * Return` |
|       - | 4074 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4075 | ` *  are not present in any of the other arrays.` |
|       - | 4076 | ` */` |
|      22 | 4077 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4078 |  |
|       - | 4079 | `	ph7_hashmap_node *pEntry;` |
|       - | 4080 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4081 | `	ph7_value *pCallback;` |
|       - | 4082 | `	ph7_value *pArray;` |
|       - | 4083 | `	sxi32 rc;` |
|       - | 4084 | `	sxu32 n;` |
|       - | 4085 | `	int i;` |
|       - | 4086 |  |
|       - | 4087 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4088 | `	if( nArg < 2 ){` |
|       4 | 4089 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4090 | `			"ArgumentCountError",` |
|       - | 4091 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4092 | `			nArg` |
|       - | 4093 | `			);` |
|       - | 4094 | `	}` |
|      22 | 4095 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4096 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4097 | `			"TypeError",` |
|       - | 4098 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4099 | `			ph7_type_name(apArg[0])` |
|       - | 4100 | `			);` |
|       - | 4101 | `	}` |
|       - | 4102 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4103 | `	 * expected to be a callback. */` |
|      32 | 4104 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4105 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4106 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4107 | `				"TypeError",` |
|       - | 4108 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4109 | `				i + 1,` |
|       2 | 4110 | `				ph7_type_name(apArg[i])` |
|       - | 4111 | `				);` |
|       - | 4112 | `		}` |
|       8 | 4113 | `	}` |
|       - | 4114 | `	/* Point to the callback value */` |
|      18 | 4115 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4116 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4117 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4118 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4119 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4120 | `		 * string given" which we also reproduce. */` |
|       7 | 4121 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4122 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4123 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4124 | `				"TypeError",` |
|       - | 4125 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4126 | `				nArg` |
|       - | 4127 | `				);` |
|       - | 4128 | `		}` |
|       5 | 4129 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4130 | `			/* neither array nor string */` |
|       7 | 4131 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4132 | `				"TypeError",` |
|       - | 4133 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4134 | `				nArg` |
|       - | 4135 | `				);` |
|       - | 4136 | `		}` |
|       - | 4137 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4138 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4139 | `			"TypeError",` |
|       - | 4140 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4141 | `			nArg,` |
|     ! 0 | 4142 | `			ph7_type_name(pCallback)` |
|       - | 4143 | `			);` |
|       - | 4144 | `	}` |
|      11 | 4145 | `	if( nArg == 2 ){` |
|       - | 4146 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4147 | `		 * input array. */` |
|       3 | 4148 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4149 | `		return PH7_OK;` |
|       - | 4150 | `	}` |
|       - | 4151 | `	/* Create a new array */` |
|       9 | 4152 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4153 | `	if( pArray == 0 ){` |
|     ! 0 | 4154 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4155 | `		return PH7_OK;` |
|       - | 4156 | `	}` |
|       - | 4157 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4158 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4159 | `	/* Perform the diff */` |
|       9 | 4160 | `	pEntry = pSrc->pFirst;` |
|       9 | 4161 | `	n = pSrc->nEntry;` |
|      20 | 4162 | `	for(;;){` |
|       - | 4163 | `		int keep;` |
|      25 | 4164 | `		if( n < 1 ){` |
|       9 | 4165 | `			break;` |
|       - | 4166 | `		}` |
|      17 | 4167 | `		keep = 1;` |
|      29 | 4168 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4169 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4170 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4171 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4172 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4173 | `			while( pIt ){` |
|       - | 4174 | `				/* build temporary key values for callback */` |
|       - | 4175 | `				ph7_value key1, key2, result;` |
|       - | 4176 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4177 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4178 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4179 | `				}else{` |
|       - | 4180 | `					SyString sStr;` |
|      31 | 4181 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4182 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4183 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4184 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4185 | `				}` |
|      31 | 4186 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4187 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4188 | `				}else{` |
|       - | 4189 | `					SyString sStr;` |
|      31 | 4190 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4191 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4192 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4193 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4194 | `				}` |
|      31 | 4195 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4196 | `				/* call user callback with (key1, key2) */` |
|       - | 4197 | `				{` |
|       - | 4198 | `					ph7_value *apK[2];` |
|      31 | 4199 | `					apK[0] = &key1;` |
|      31 | 4200 | `					apK[1] = &key2;` |
|      31 | 4201 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4202 | `				}` |
|      31 | 4203 | `				if( rc == SXRET_OK ){` |
|      31 | 4204 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4205 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4206 | `					}` |
|      31 | 4207 | `					if( result.x.iVal == 0 ){` |
|       - | 4208 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4209 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4210 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4211 | `						if( pVal1 && pVal2 ){` |
|      13 | 4212 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4213 | `								keep = 0;` |
|       9 | 4214 | `								PH7_MemObjRelease(&result);` |
|       - | 4215 | `								/* release keys too before breaking */` |
|       9 | 4216 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4217 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4218 | `								break;` |
|       - | 4219 | `							}` |
|       2 | 4220 | `						}` |
|       2 | 4221 | `					}` |
|      11 | 4222 | `				}` |
|      23 | 4223 | `				PH7_MemObjRelease(&result);` |
|      23 | 4224 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4225 | `				PH7_MemObjRelease(&key2);` |
|       - | 4226 | `				/* move to next node */` |
|      23 | 4227 | `				pIt = pIt->pPrev;` |
|      23 | 4228 | `				if( keep == 0 ) break;` |
|       1 | 4229 | `			}` |
|      21 | 4230 | `			if( keep == 0 ) break;` |
|       7 | 4231 | `		}` |
|      17 | 4232 | `		if( keep ){` |
|       - | 4233 | `			/* Perform the insertion */` |
|       9 | 4234 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4235 | `		}` |
|       - | 4236 | `		/* Point to the next entry */` |
|      17 | 4237 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4238 | `		n--;` |
|       1 | 4239 | `	}` |
|       - | 4240 | `	/* Return the freshly created array */` |
|       9 | 4241 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4242 | `	return PH7_OK;` |
|      13 | 4243 |  |
|       - | 4244 | `/*` |
|       - | 4245 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4246 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4247 | ` * Parameters` |
|       - | 4248 | ` *  $array1` |
|       - | 4249 | ` *    The array to compare from` |
|       - | 4250 | ` *  $array2` |
|       - | 4251 | ` *    An array to compare against` |
|       - | 4252 | ` *  $...` |
|       - | 4253 | ` *   More arrays to compare against` |
|       - | 4254 | ` * Return` |
|       - | 4255 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4256 | ` *  in any of the other arrays.` |
|       - | 4257 | ` * Note that NULL is returned on failure.` |
|       - | 4258 | ` */` |
|      14 | 4259 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4260 |  |
|       - | 4261 | `	ph7_hashmap_node *pEntry;` |
|       - | 4262 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4263 | `	ph7_value *pArray;` |
|       - | 4264 | `	sxi32 rc;` |
|       - | 4265 | `	sxu32 n;` |
|       - | 4266 | `	int i;` |
|       - | 4267 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4268 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4269 | `	 * helpers. */` |
|      16 | 4270 | `	if( nArg < 1 ){` |
|       4 | 4271 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4272 | `			"ArgumentCountError",` |
|       - | 4273 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4274 | `			nArg` |
|       - | 4275 | `			);` |
|       - | 4276 | `	}` |
|      14 | 4277 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4278 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4279 | `			"TypeError",` |
|       - | 4280 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4281 | `			ph7_type_name(apArg[0])` |
|       - | 4282 | `			);` |
|       - | 4283 | `	}` |
|      20 | 4284 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4285 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4286 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4287 | `				"TypeError",` |
|       - | 4288 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4289 | `				i + 1,` |
|       2 | 4290 | `				ph7_type_name(apArg[i])` |
|       - | 4291 | `				);` |
|       - | 4292 | `		}` |
|       5 | 4293 | `	}` |
|       9 | 4294 | `	if( nArg == 1 ){` |
|       - | 4295 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4296 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4297 | `		return PH7_OK;` |
|       - | 4298 | `	}` |
|       - | 4299 | `	/* Create a new array */` |
|       7 | 4300 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4301 | `	if( pArray == 0 ){` |
|     ! 0 | 4302 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4303 | `		return PH7_OK;` |
|       - | 4304 | `	}` |
|       - | 4305 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4306 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4307 | `	/* Perfrom the diff */` |
|       7 | 4308 | `	pEntry = pSrc->pFirst;` |
|       7 | 4309 | `	n = pSrc->nEntry;` |
|      12 | 4310 | `	for(;;){` |
|      25 | 4311 | `		if( n < 1 ){` |
|       7 | 4312 | `			break;` |
|       - | 4313 | `		}` |
|      31 | 4314 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4315 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4316 | `				/* ignore */` |
|     ! 0 | 4317 | `				continue;` |
|       - | 4318 | `			}` |
|      23 | 4319 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4320 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4321 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4322 | `				/* Blob lookup */` |
|      17 | 4323 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4324 | `			}else{` |
|       - | 4325 | `				/* Int lookup */` |
|       7 | 4326 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4327 | `			}` |
|      23 | 4328 | `			if( rc == SXRET_OK ){` |
|       - | 4329 | `				/* Key exists,break immediately */` |
|      11 | 4330 | `				break;` |
|       - | 4331 | `			}` |
|       7 | 4332 | `		}` |
|      19 | 4333 | `		if( i >= nArg ){` |
|       - | 4334 | `			/* Perform the insertion */` |
|       9 | 4335 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4336 | `		}` |
|       - | 4337 | `		/* Point to the next entry */` |
|      19 | 4338 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4339 | `		n--;` |
|       1 | 4340 | `	}` |
|       - | 4341 | `	/* Return the freshly created array */` |
|       7 | 4342 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4343 | `	return PH7_OK;` |
|       9 | 4344 |  |
|       - | 4345 | `/*` |
|       - | 4346 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4347 | ` *  Computes the intersection of arrays.` |
|       - | 4348 | ` * Parameters` |
|       - | 4349 | ` *  $array1` |
|       - | 4350 | ` *    The array to compare from` |
|       - | 4351 | ` *  $array2` |
|       - | 4352 | ` *    An array to compare against` |
|       - | 4353 | ` *  $...` |
|       - | 4354 | ` *   More arrays to compare against` |
|       - | 4355 | ` * Return` |
|       - | 4356 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4357 | ` *  in all of the parameters.` |
|       - | 4358 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4359 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4360 | ` */` |
|      22 | 4361 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4362 |  |
|       - | 4363 | `	ph7_hashmap_node *pEntry;` |
|       - | 4364 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4365 | `	ph7_value *pArray;` |
|       - | 4366 | `	ph7_value *pVal;` |
|       - | 4367 | `	sxi32 rc;` |
|       - | 4368 | `	sxu32 n;` |
|       - | 4369 | `	int i;` |
|      24 | 4370 | `	if( nArg < 1 ){` |
|       4 | 4371 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4372 | `			"ArgumentCountError",` |
|       - | 4373 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4374 | `			nArg` |
|       - | 4375 | `			);` |
|       - | 4376 | `	}` |
|      22 | 4377 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4378 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4379 | `			"TypeError",` |
|       - | 4380 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4381 | `			ph7_type_name(apArg[0])` |
|       - | 4382 | `			);` |
|       - | 4383 | `	}` |
|      36 | 4384 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4385 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4386 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4387 | `				"TypeError",` |
|       - | 4388 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4389 | `				i + 1,` |
|       2 | 4390 | `				ph7_type_name(apArg[i])` |
|       - | 4391 | `				);` |
|       - | 4392 | `		}` |
|       9 | 4393 | `	}` |
|      17 | 4394 | `	if( nArg == 1 ){` |
|       - | 4395 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4396 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4397 | `		return PH7_OK;` |
|       - | 4398 | `	}` |
|       - | 4399 | `	/* Create a new array */` |
|      15 | 4400 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4401 | `	if( pArray == 0 ){` |
|     ! 0 | 4402 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4403 | `		return PH7_OK;` |
|       - | 4404 | `	}` |
|       - | 4405 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4406 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4407 | `	/* Perform the intersection */` |
|      15 | 4408 | `	pEntry = pSrc->pFirst;` |
|      15 | 4409 | `	n = pSrc->nEntry;` |
|      31 | 4410 | `	for(;;){` |
|      63 | 4411 | `		if( n < 1 ){` |
|      15 | 4412 | `			break;` |
|       - | 4413 | `		}` |
|       - | 4414 | `		/* Extract the node value */` |
|      49 | 4415 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4416 | `		if( pVal ){` |
|      79 | 4417 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4418 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4419 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4420 | `				/* Perform the lookup */` |
|      55 | 4421 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4422 | `				if( rc != SXRET_OK ){` |
|       - | 4423 | `					/* Value does not exist */` |
|      25 | 4424 | `					break;` |
|       - | 4425 | `				}` |
|      16 | 4426 | `			}` |
|      49 | 4427 | `			if( i >= nArg ){` |
|       - | 4428 | `				/* Perform the insertion */` |
|      25 | 4429 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4430 | `			}` |
|      24 | 4431 | `		}` |
|       - | 4432 | `		/* Point to the next entry */` |
|      49 | 4433 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4434 | `		n--;` |
|       1 | 4435 | `	}` |
|       - | 4436 | `	/* Return the freshly created array */` |
|      15 | 4437 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4438 | `	return PH7_OK;` |
|      13 | 4439 |  |
|       - | 4440 | `/*` |
|       - | 4441 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4442 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4443 | ` * Parameters` |
|       - | 4444 | ` *  $array1` |
|       - | 4445 | ` *    The array to compare from` |
|       - | 4446 | ` *  $array2` |
|       - | 4447 | ` *    An array to compare against` |
|       - | 4448 | ` *  $...` |
|       - | 4449 | ` *   More arrays to compare against` |
|       - | 4450 | ` * Return` |
|       - | 4451 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4452 | ` *  in all the arguments, with matching keys.` |
|       - | 4453 | ` */` |
|      22 | 4454 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4455 |  |
|       - | 4456 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4457 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4458 | `	ph7_value *pArray;` |
|       - | 4459 | `	ph7_value *pVal;` |
|       - | 4460 | `	sxi32 rc;` |
|       - | 4461 | `	sxu32 n;` |
|       - | 4462 | `	int i;` |
|      24 | 4463 | `	if( nArg < 1 ){` |
|       4 | 4464 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4465 | `			"ArgumentCountError",` |
|       - | 4466 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4467 | `			nArg` |
|       - | 4468 | `			);` |
|       - | 4469 | `	}` |
|      22 | 4470 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4471 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4472 | `			"TypeError",` |
|       - | 4473 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4474 | `			ph7_type_name(apArg[0])` |
|       - | 4475 | `			);` |
|       - | 4476 | `	}` |
|      36 | 4477 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4478 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4479 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4480 | `				"TypeError",` |
|       - | 4481 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4482 | `				i + 1,` |
|       2 | 4483 | `				ph7_type_name(apArg[i])` |
|       - | 4484 | `				);` |
|       - | 4485 | `		}` |
|       9 | 4486 | `	}` |
|      17 | 4487 | `	if( nArg == 1 ){` |
|       - | 4488 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4489 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4490 | `		return PH7_OK;` |
|       - | 4491 | `	}` |
|       - | 4492 | `	/* Create a new array */` |
|      15 | 4493 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4494 | `	if( pArray == 0 ){` |
|     ! 0 | 4495 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4496 | `		return PH7_OK;` |
|       - | 4497 | `	}` |
|       - | 4498 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4499 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4500 | `	/* Perform the intersection */` |
|      15 | 4501 | `	pEntry = pSrc->pFirst;` |
|      15 | 4502 | `	n = pSrc->nEntry;` |
|      15 | 4503 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4504 | `	for(;;){` |
|      47 | 4505 | `		if( n < 1 ){` |
|      15 | 4506 | `			break;` |
|       - | 4507 | `		}` |
|       - | 4508 | `		/* Extract the node value */` |
|      33 | 4509 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4510 | `		if( pVal ){` |
|      53 | 4511 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4512 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4513 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4514 | `				/* Perform a key lookup first */` |
|      37 | 4515 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4516 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4517 | `				}else{` |
|      23 | 4518 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4519 | `				}` |
|      37 | 4520 | `				if( rc != SXRET_OK ){` |
|       - | 4521 | `					/* No such key,break immediately */` |
|       7 | 4522 | `					break;` |
|       - | 4523 | `				}` |
|       - | 4524 | `				/* Perform the lookup */` |
|      31 | 4525 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4526 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4527 | `					/* Value does not exist */` |
|       6 | 4528 | `					break;` |
|       - | 4529 | `				}` |
|      11 | 4530 | `			}` |
|      33 | 4531 | `			if( i >= nArg ){` |
|       - | 4532 | `				/* Perform the insertion */` |
|      17 | 4533 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4534 | `			}` |
|      16 | 4535 | `		}` |
|       - | 4536 | `		/* Point to the next entry */` |
|      33 | 4537 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4538 | `		n--;` |
|       1 | 4539 | `	}` |
|       - | 4540 | `	/* Return the freshly created array */` |
|      15 | 4541 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4542 | `	return PH7_OK;` |
|      13 | 4543 |  |
|       - | 4544 | `/*` |
|       - | 4545 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 4546 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4547 | ` * Parameters` |
|       - | 4548 | ` *  $array1` |
|       - | 4549 | ` *    The array to compare from` |
|       - | 4550 | ` *  $...` |
|       - | 4551 | ` *   More arrays to compare against` |
|       - | 4552 | ` * Return` |
|       - | 4553 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4554 | ` *  have keys that are present in all arguments.` |
|       - | 4555 | ` * Note that NULL is returned on failure.` |
|       - | 4556 | ` */` |
|      22 | 4557 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4558 |  |
|       - | 4559 | `	ph7_hashmap_node *pEntry;` |
|       - | 4560 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4561 | `	ph7_value *pArray;` |
|       - | 4562 | `	sxi32 rc;` |
|       - | 4563 | `	sxu32 n;` |
|       - | 4564 | `	int i;` |
|      24 | 4565 | `	if( nArg < 1 ){` |
|       4 | 4566 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4567 | `			"ArgumentCountError",` |
|       - | 4568 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 4569 | `			nArg` |
|       - | 4570 | `			);` |
|       - | 4571 | `	}` |
|      22 | 4572 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4573 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4574 | `			"TypeError",` |
|       - | 4575 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4576 | `			ph7_type_name(apArg[0])` |
|       - | 4577 | `			);` |
|       - | 4578 | `	}` |
|      36 | 4579 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4580 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4581 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4582 | `				"TypeError",` |
|       - | 4583 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4584 | `				i + 1,` |
|       2 | 4585 | `				ph7_type_name(apArg[i])` |
|       - | 4586 | `				);` |
|       - | 4587 | `		}` |
|       9 | 4588 | `	}` |
|      17 | 4589 | `	if( nArg == 1 ){` |
|       - | 4590 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4591 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4592 | `		return PH7_OK;` |
|       - | 4593 | `	}` |
|       - | 4594 | `	/* Create a new array */` |
|      15 | 4595 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4596 | `	if( pArray == 0 ){` |
|     ! 0 | 4597 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4598 | `		return PH7_OK;` |
|       - | 4599 | `	}` |
|       - | 4600 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 4601 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4602 | `	/* Perform the intersection */` |
|      15 | 4603 | `	pEntry = pSrc->pFirst;` |
|      15 | 4604 | `	n = pSrc->nEntry;` |
|      24 | 4605 | `	for(;;){` |
|      49 | 4606 | `		if( n < 1 ){` |
|      15 | 4607 | `			break;` |
|       - | 4608 | `		}` |
|      57 | 4609 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 4610 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 4611 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 4612 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4613 | `				/* Blob lookup */` |
|      27 | 4614 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 4615 | `			}else{` |
|       - | 4616 | `				/* Int key */` |
|      13 | 4617 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4618 | `			}` |
|      39 | 4619 | `			if( rc != SXRET_OK ){` |
|       - | 4620 | `				/* Key does not exist, break immediately */` |
|      17 | 4621 | `				break;` |
|       - | 4622 | `			}` |
|      12 | 4623 | `		}` |
|      35 | 4624 | `		if( i >= nArg ){` |
|       - | 4625 | `			/* Perform the insertion */` |
|      19 | 4626 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 4627 | `		}` |
|       - | 4628 | `		/* Point to the next entry */` |
|      35 | 4629 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 4630 | `		n--;` |
|       1 | 4631 | `	}` |
|       - | 4632 | `	/* Return the freshly created array */` |
|      15 | 4633 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4634 | `	return PH7_OK;` |
|      13 | 4635 |  |
|       - | 4636 | `/*` |
|       - | 4637 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4638 | ` *  Computes the intersection of arrays.` |
|       - | 4639 | ` * Parameters` |
|       - | 4640 | ` *  $array1` |
|       - | 4641 | ` *    The array to compare from` |
|       - | 4642 | ` *  $array2` |
|       - | 4643 | ` *    An array to compare against` |
|       - | 4644 | ` *  $...` |
|       - | 4645 | ` *   More arrays to compare against` |
|       - | 4646 | ` * $callback` |
|       - | 4647 | ` *  The callback comparison function.` |
|       - | 4648 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4649 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4650 | ` *  than the second.` |
|       - | 4651 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4652 | ` * Return` |
|       - | 4653 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4654 | ` *  in all of the parameters. .` |
|       - | 4655 | ` * Note that NULL is returned on failure.` |
|       - | 4656 | ` */` |
|       2 | 4657 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4658 |  |
|       - | 4659 | `	ph7_hashmap_node *pEntry;` |
|       - | 4660 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4661 | `	ph7_value *pCallback;` |
|       - | 4662 | `	ph7_value *pArray;` |
|       - | 4663 | `	ph7_value *pVal;` |
|       - | 4664 | `	sxi32 rc;` |
|       - | 4665 | `	sxu32 n;` |
|       - | 4666 | `	int i;` |
|       - | 4667 |  |
|       3 | 4668 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4669 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4670 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4671 | `		return PH7_OK;` |
|       - | 4672 | `	}` |
|       - | 4673 | `	/* Point to the callback */` |
|       3 | 4674 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4675 | `	if( nArg == 2 ){` |
|       - | 4676 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4677 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4678 | `		return PH7_OK;` |
|       - | 4679 | `	}` |
|       - | 4680 | `	/* Create a new array */` |
|       3 | 4681 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4682 | `	if( pArray == 0 ){` |
|     ! 0 | 4683 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4684 | `		return PH7_OK;` |
|       - | 4685 | `	}` |
|       - | 4686 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4687 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4688 | `	/* Perform the intersection */` |
|       3 | 4689 | `	pEntry = pSrc->pFirst;` |
|       3 | 4690 | `	n = pSrc->nEntry;` |
|       4 | 4691 | `	for(;;){` |
|       9 | 4692 | `		if( n < 1 ){` |
|       3 | 4693 | `			break;` |
|       - | 4694 | `		}` |
|       - | 4695 | `		/* Extract the node value */` |
|       7 | 4696 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4697 | `		if( pVal ){` |
|      11 | 4698 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4699 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4700 | `					/* ignore */` |
|     ! 0 | 4701 | `					continue;` |
|       - | 4702 | `				}` |
|       - | 4703 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4704 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4705 | `				/* Perform the lookup */` |
|       7 | 4706 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4707 | `				if( rc != SXRET_OK ){` |
|       - | 4708 | `					/* Value does not exist */` |
|       3 | 4709 | `					break;` |
|       - | 4710 | `				}` |
|       3 | 4711 | `			}` |
|       7 | 4712 | `			if( i >= (nArg-1) ){` |
|       - | 4713 | `				/* Perform the insertion */` |
|       5 | 4714 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4715 | `			}` |
|       3 | 4716 | `		}` |
|       - | 4717 | `		/* Point to the next entry */` |
|       7 | 4718 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4719 | `		n--;` |
|       1 | 4720 | `	}` |
|       - | 4721 | `	/* Return the freshly created array */` |
|       3 | 4722 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4723 | `	return PH7_OK;` |
|       2 | 4724 |  |
|       - | 4725 | `/*` |
|       - | 4726 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 4727 | ` *  Fill an array with values.` |
|       - | 4728 | ` * Parameters` |
|       - | 4729 | ` *  $start_index` |
|       - | 4730 | ` *    The first index of the returned array.` |
|       - | 4731 | ` *  $num` |
|       - | 4732 | ` *   Number of elements to insert.` |
|       - | 4733 | ` *  $value` |
|       - | 4734 | ` *    Value to use for filling.` |
|       - | 4735 | ` * Return` |
|       - | 4736 | ` *  The filled array or null on failure.` |
|       - | 4737 | ` */` |
|     238 | 4738 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4739 |  |
|       - | 4740 | `	ph7_value *pArray;` |
|       - | 4741 | `	int i,nEntry;` |
|       - | 4742 |  |
|       - | 4743 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 4744 | `	if( nArg != 3 ){` |
|       - | 4745 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 4746 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4747 | `			"ArgumentCountError",` |
|       - | 4748 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 4749 | `			nArg` |
|       - | 4750 | `			);` |
|       - | 4751 | `	}` |
|       - | 4752 |  |
|       - | 4753 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 4754 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 4755 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 4756 | `	 * and NULLs are rejected outright. */` |
|     466 | 4757 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 4758 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 4759 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4760 | `			"TypeError",` |
|       - | 4761 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 4762 | `			ph7_type_name(apArg[0])` |
|       - | 4763 | `			);` |
|       - | 4764 | `	}` |
|     234 | 4765 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 4766 | `		int len;` |
|       8 | 4767 | `		sxu8 bReal = FALSE;` |
|       8 | 4768 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 4769 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 4770 | `			/* Non‑numeric string is an error. */` |
|       3 | 4771 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4772 | `				"TypeError",` |
|       - | 4773 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 4774 | `				);` |
|       - | 4775 | `		}` |
|       5 | 4776 | `		if( bReal ){` |
|       - | 4777 | `			/* float-string -> deprecation warning */` |
|       4 | 4778 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4779 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 4780 | `				zStr` |
|       - | 4781 | `				);` |
|       1 | 4782 | `		}` |
|       2 | 4783 | `	}` |
|       - | 4784 |  |
|       - | 4785 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 4786 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 4787 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 4788 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4789 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4790 | `			"TypeError",` |
|       - | 4791 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 4792 | `			ph7_type_name(apArg[1])` |
|       - | 4793 | `			);` |
|       - | 4794 | `	}` |
|     232 | 4795 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4796 | `		int len;` |
|       3 | 4797 | `		sxu8 bReal = FALSE;` |
|       3 | 4798 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4799 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 4800 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4801 | `				"TypeError",` |
|       - | 4802 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 4803 | `				);` |
|       - | 4804 | `		}` |
|     ! 0 | 4805 | `	}` |
|       - | 4806 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 4807 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 4808 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 4809 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 4810 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 4811 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 4812 | `		if( d != (double)i64 ){` |
|       7 | 4813 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4814 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 4815 | `				d` |
|       - | 4816 | `				);` |
|       2 | 4817 | `		}` |
|       2 | 4818 | `	}` |
|       - | 4819 |  |
|       - | 4820 | `	/* Total number of entries to insert */` |
|     230 | 4821 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 4822 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 4823 | `	if( nEntry < 0 ){` |
|       3 | 4824 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4825 | `			"ValueError",` |
|       - | 4826 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 4827 | `			);` |
|       - | 4828 | `	}` |
|       - | 4829 |  |
|       - | 4830 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 4831 | `	if( nEntry == 0 ){` |
|       7 | 4832 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 4833 | `		return PH7_OK;` |
|       - | 4834 | `	}` |
|       - | 4835 |  |
|       - | 4836 | `	/* Create a new array */` |
|     221 | 4837 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 4838 | `	if( pArray == 0 ){` |
|     ! 0 | 4839 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4840 | `		return PH7_OK;` |
|       - | 4841 | `	}` |
|       - | 4842 |  |
|       - | 4843 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 4844 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 4845 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 4846 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 4847 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 4848 | `	}` |
|       - | 4849 | `	/* Return the filled array */` |
|     221 | 4850 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 4851 | `	return PH7_OK;` |
|     121 | 4852 |  |
|       - | 4853 | `/*` |
|       - | 4854 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 4855 | ` *  Fill an array with values, specifying keys.` |
|       - | 4856 | ` * Parameters` |
|       - | 4857 | ` *  $input` |
|       - | 4858 | ` *   Array of values that will be used as key.` |
|       - | 4859 | ` *  $value` |
|       - | 4860 | ` *    Value to use for filling.` |
|       - | 4861 | ` * Return` |
|       - | 4862 | ` *  The filled array.` |
|       - | 4863 | ` * Throws` |
|       - | 4864 | ` *  ValueError if $input is not an array.` |
|       - | 4865 | ` */` |
|      26 | 4866 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4867 |  |
|       - | 4868 | `	ph7_hashmap_node *pEntry;` |
|       - | 4869 | `	ph7_hashmap *pSrc;` |
|       - | 4870 | `	ph7_value *pArray;` |
|       - | 4871 | `	sxu32 n;` |
|       - | 4872 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 4873 | `	if( nArg != 2 ){` |
|      10 | 4874 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4875 | `			"ArgumentCountError",` |
|       - | 4876 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 4877 | `			nArg` |
|       - | 4878 | `			);` |
|       - | 4879 | `	}` |
|       - | 4880 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 4881 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 4882 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4883 | `			"TypeError",` |
|       - | 4884 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 4885 | `			ph7_type_name(apArg[0])` |
|       - | 4886 | `			);` |
|       - | 4887 | `	}` |
|       - | 4888 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 4889 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4890 | `	/* Create a new array */` |
|      17 | 4891 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 4892 | `	if( pArray == 0 ){` |
|     ! 0 | 4893 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4894 | `		return PH7_OK;` |
|       - | 4895 | `	}` |
|       - | 4896 | `	/* Perform the requested operation */` |
|      17 | 4897 | `	pEntry = pSrc->pFirst;` |
|      45 | 4898 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 4899 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 4900 | `		/* Point to the next entry */` |
|      29 | 4901 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 4902 | `	}` |
|       - | 4903 | `	/* Return the filled array */` |
|      17 | 4904 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4905 | `	return PH7_OK;` |
|      15 | 4906 |  |
|       - | 4907 | `/*` |
|       - | 4908 | ` * array array_combine(array $keys,array $values)` |
|       - | 4909 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 4910 | ` * Parameters` |
|       - | 4911 | ` *  $keys` |
|       - | 4912 | ` *    Array of keys to be used.` |
|       - | 4913 | ` * $values` |
|       - | 4914 | ` *   Array of values to be used.` |
|       - | 4915 | ` * Return` |
|       - | 4916 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 4917 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 4918 | ` *  not an array.` |
|       - | 4919 | ` */` |
|      18 | 4920 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4921 |  |
|       - | 4922 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 4923 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 4924 | `	ph7_value *pArray;` |
|       - | 4925 | `	sxu32 n;` |
|       - | 4926 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 4927 | `	if( nArg != 2 ){` |
|       - | 4928 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 4929 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4930 | `			"ArgumentCountError",` |
|       - | 4931 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 4932 | `			nArg` |
|       - | 4933 | `			);` |
|       - | 4934 | `	}` |
|       - | 4935 | `	/* Validate argument types individually so we can report the correct` |
|       - | 4936 | `	 * argument index in the error message. */` |
|      18 | 4937 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4938 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4939 | `			"TypeError",` |
|       - | 4940 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 4941 | `			ph7_type_name(apArg[0])` |
|       - | 4942 | `			);` |
|       - | 4943 | `	}` |
|      16 | 4944 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 4945 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4946 | `			"TypeError",` |
|       - | 4947 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 4948 | `			ph7_type_name(apArg[1])` |
|       - | 4949 | `			);` |
|       - | 4950 | `	}` |
|       - | 4951 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 4952 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 4953 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 4954 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 4955 | `		/* Length mismatch -> ValueError */` |
|       3 | 4956 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4957 | `			"ValueError",` |
|       - | 4958 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 4959 | `			);` |
|       - | 4960 | `	}` |
|       - | 4961 | `	/* Create a new array */` |
|      11 | 4962 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4963 | `	if( pArray == 0 ){` |
|     ! 0 | 4964 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4965 | `		return PH7_OK;` |
|       - | 4966 | `	}` |
|       - | 4967 | `	/* Perform the requested operation */` |
|      11 | 4968 | `	pKe = pKey->pFirst;` |
|      11 | 4969 | `	pVe = pValue->pFirst;` |
|      33 | 4970 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 4971 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 4972 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 4973 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 4974 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 4975 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 4976 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 4977 | `		 * original array must not be mutated. */` |
|      23 | 4978 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 4979 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 4980 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 4981 | `			if( pTmpKey ){` |
|       5 | 4982 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 4983 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 4984 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 4985 | `				pKeyCopy = pTmpKey;` |
|       2 | 4986 | `			}` |
|       2 | 4987 | `		}` |
|      23 | 4988 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 4989 | `		/* Point to the next entry */` |
|      23 | 4990 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 4991 | `		pVe = pVe->pPrev;` |
|      12 | 4992 | `	}` |
|       - | 4993 | `	/* Return the filled array */` |
|      11 | 4994 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4995 | `	return PH7_OK;` |
|      11 | 4996 |  |
|       - | 4997 | `/*` |
|       - | 4998 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 4999 | ` *  Return an array with elements in reverse order.` |
|       - | 5000 | ` * Parameters` |
|       - | 5001 | ` *  $array` |
|       - | 5002 | ` *   The input array.` |
|       - | 5003 | ` *  $preserve_keys (optional)` |
|       - | 5004 | ` *   If set to TRUE keys are preserved.` |
|       - | 5005 | ` * Return` |
|       - | 5006 | ` *  The reversed array.` |
|       - | 5007 | ` */` |
|      20 | 5008 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5009 |  |
|       - | 5010 | `	ph7_hashmap_node *pEntry;` |
|       - | 5011 | `	ph7_hashmap *pSrc;` |
|       - | 5012 | `	ph7_value *pArray;` |
|       - | 5013 | `	int bPreserve;` |
|       - | 5014 | `	sxu32 n;` |
|      22 | 5015 | `	if( nArg < 1 ){` |
|       4 | 5016 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5017 | `			"ArgumentCountError",` |
|       - | 5018 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5019 | `			nArg` |
|       - | 5020 | `			);` |
|       - | 5021 | `	}` |
|       - | 5022 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5023 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5024 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5025 | `			"TypeError",` |
|       - | 5026 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5027 | `			ph7_type_name(apArg[0])` |
|       - | 5028 | `			);` |
|       - | 5029 | `	}` |
|      17 | 5030 | `	bPreserve = FALSE;` |
|      17 | 5031 | `	if( nArg > 1 ){` |
|       7 | 5032 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5033 | `	}` |
|       - | 5034 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5035 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5036 | `	/* Create a new array */` |
|      17 | 5037 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5038 | `	if( pArray == 0 ){` |
|     ! 0 | 5039 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5040 | `		return PH7_OK;` |
|       - | 5041 | `	}` |
|       - | 5042 | `	/* Perform the requested operation */` |
|      17 | 5043 | `	pEntry = pSrc->pLast;` |
|      55 | 5044 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5045 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5046 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5047 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5048 | `		/* Point to the previous entry */` |
|      39 | 5049 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5050 | `	}` |
|      17 | 5051 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5052 | `	return PH7_OK;` |
|      12 | 5053 |  |
|       - | 5054 | `/*` |
|       - | 5055 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5056 | ` *  Removes duplicate values from an array.` |
|       - | 5057 | ` * Parameters` |
|       - | 5058 | ` *  $array` |
|       - | 5059 | ` *   The input array.` |
|       - | 5060 | ` *  $flags` |
|       - | 5061 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5062 | ` *   behavior using these values:` |
|       - | 5063 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5064 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5065 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5066 | ` * Return` |
|       - | 5067 | ` *  The filtered array.` |
|       - | 5068 | ` */` |
|      24 | 5069 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5070 |  |
|       - | 5071 | `	ph7_hashmap_node *pEntry;` |
|       - | 5072 | `	ph7_value *pNeedle;` |
|       - | 5073 | `	ph7_hashmap *pSrc;` |
|       - | 5074 | `	ph7_value *pArray;` |
|       - | 5075 | `	int bStrict;` |
|       - | 5076 | `	sxi32 rc;` |
|       - | 5077 | `	sxu32 n;` |
|      26 | 5078 | `	if( nArg < 1 ){` |
|       - | 5079 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5080 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5081 | `			"ArgumentCountError",` |
|       - | 5082 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5083 | `			);` |
|       - | 5084 | `	}` |
|      24 | 5085 | `	if( nArg > 2 ){` |
|       - | 5086 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5087 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5088 | `			"ArgumentCountError",` |
|       - | 5089 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5090 | `			nArg` |
|       - | 5091 | `			);` |
|       - | 5092 | `	}` |
|       - | 5093 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5094 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5095 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5096 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5097 | `			"TypeError",` |
|       - | 5098 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5099 | `			ph7_type_name(apArg[0])` |
|       - | 5100 | `			);` |
|       - | 5101 | `	}` |
|      19 | 5102 | `	bStrict = FALSE;` |
|       - | 5103 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5104 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5105 | `	/* Create a new array */` |
|      19 | 5106 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5107 | `	if( pArray == 0 ){` |
|     ! 0 | 5108 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5109 | `		return PH7_OK;` |
|       - | 5110 | `	}` |
|       - | 5111 | `	/* Perform the requested operation */` |
|      19 | 5112 | `	pEntry = pSrc->pFirst;` |
|      83 | 5113 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5114 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5115 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5116 | `		if( pNeedle ){` |
|      65 | 5117 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5118 | `		}` |
|      65 | 5119 | `		if( rc != SXRET_OK ){` |
|       - | 5120 | `			/* Perform the insertion */` |
|      37 | 5121 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5122 | `		}` |
|       - | 5123 | `		/* Point to the next entry */` |
|      65 | 5124 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5125 | `	}` |
|       - | 5126 | `	/* Return the freshly created array */` |
|      19 | 5127 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5128 | `	return PH7_OK;` |
|      14 | 5129 |  |
|       - | 5130 | `/*` |
|       - | 5131 | ` * array array_flip(array $input)` |
|       - | 5132 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5133 | ` * Parameter` |
|       - | 5134 | ` *  $input` |
|       - | 5135 | ` *   Input array.` |
|       - | 5136 | ` * Return` |
|       - | 5137 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5138 | ` */` |
|      34 | 5139 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5140 |  |
|       - | 5141 | `	ph7_hashmap_node *pEntry;` |
|       - | 5142 | `	ph7_hashmap *pSrc;` |
|       - | 5143 | `	ph7_value *pArray;` |
|       - | 5144 | `	ph7_value *pKey;` |
|       - | 5145 | `	ph7_value sVal;` |
|       - | 5146 | `	sxu32 n;` |
|       - | 5147 |  |
|       - | 5148 | `	/* PHP requires exactly one argument */` |
|      36 | 5149 | `	if( nArg != 1 ){` |
|       - | 5150 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5151 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5152 | `			"ArgumentCountError",` |
|       - | 5153 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5154 | `			nArg` |
|       - | 5155 | `			);` |
|       - | 5156 | `	}` |
|       - | 5157 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5158 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5159 | `		/* Type mismatch -> TypeError */` |
|       7 | 5160 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5161 | `			"TypeError",` |
|       - | 5162 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5163 | `			ph7_type_name(apArg[0])` |
|       - | 5164 | `			);` |
|       - | 5165 | `	}` |
|       - | 5166 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5167 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5168 | `	/* Create a new array */` |
|      27 | 5169 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5170 | `	if( pArray == 0 ){` |
|     ! 0 | 5171 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5172 | `		return PH7_OK;` |
|       - | 5173 | `	}` |
|       - | 5174 | `	/* Start processing */` |
|      27 | 5175 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5176 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5177 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5178 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5179 | `		if( pKey ){` |
|       - | 5180 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5181 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5182 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5183 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5184 | `					);` |
|   22236 | 5185 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5186 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5187 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5188 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5189 | `				}else{` |
|       - | 5190 | `					SyString sStr;` |
|    2227 | 5191 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5192 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5193 | `				}` |
|       - | 5194 | `				/* Perform the insertion */` |
|   22227 | 5195 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5196 | `				/* Safely release the value because each inserted entry` |
|       - | 5197 | `				 * has its own private copy of the value.` |
|       - | 5198 | `				 */` |
|   22227 | 5199 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5200 | `			}else{` |
|       - | 5201 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5202 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5203 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5204 | `					);` |
|       - | 5205 | `			}` |
|   11118 | 5206 | `		}` |
|       - | 5207 | `		/* Point to the next entry */` |
|   22237 | 5208 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5209 | `	}` |
|       - | 5210 | `	/* Return the freshly created array */` |
|      27 | 5211 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5212 | `	return PH7_OK;` |
|      19 | 5213 |  |
|       - | 5214 | `/*` |
|       - | 5215 | ` * number array_sum(array $array )` |
|       - | 5216 | ` *  Calculate the sum of values in an array.` |
|       - | 5217 | ` * Parameters` |
|       - | 5218 | ` *  $array: The input array.` |
|       - | 5219 | ` * Return` |
|       - | 5220 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5221 | ` */` |
|      24 | 5222 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5223 |  |
|       - | 5224 | `	ph7_hashmap_node *pEntry;` |
|       - | 5225 | `	ph7_value *pObj;` |
|      25 | 5226 | `	double dSum = 0;` |
|       - | 5227 | `	sxu32 n;` |
|      25 | 5228 | `	pEntry = pMap->pFirst;` |
|      91 | 5229 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5230 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5231 | `		if( pObj ){` |
|      67 | 5232 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5233 | `				dSum += pObj->rVal;` |
|      53 | 5234 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5235 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5236 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5237 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5238 | `					double dv = 0;` |
|      13 | 5239 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5240 | `					dSum += dv;` |
|       7 | 5241 | `				}` |
|      12 | 5242 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5243 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5244 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5245 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5246 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5247 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5248 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5249 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5250 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5251 | `			}` |
|       - | 5252 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5253 | `		}` |
|       - | 5254 | `		/* Point to the next entry */` |
|      67 | 5255 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5256 | `	}` |
|       - | 5257 | `	/* Return sum */` |
|      25 | 5258 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5259 |  |
|      18 | 5260 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5261 |  |
|       - | 5262 | `	ph7_hashmap_node *pEntry;` |
|       - | 5263 | `	ph7_value *pObj;` |
|      20 | 5264 | `	sxi64 nSum = 0;` |
|       - | 5265 | `	sxu32 n;` |
|      20 | 5266 | `	pEntry = pMap->pFirst;` |
|      80 | 5267 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5268 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5269 | `		if( pObj ){` |
|      62 | 5270 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5271 | `				nSum += pObj->x.iVal;` |
|      36 | 5272 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5273 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5274 | `					sxi64 nv = 0;` |
|       5 | 5275 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5276 | `					nSum += nv;` |
|       3 | 5277 | `				}` |
|       8 | 5278 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5279 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5280 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5281 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5282 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5283 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5284 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5285 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5286 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5287 | `			}` |
|       - | 5288 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5289 | `		}` |
|       - | 5290 | `		/* Point to the next entry */` |
|      62 | 5291 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5292 | `	}` |
|       - | 5293 | `	/* Return sum */` |
|      20 | 5294 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5295 |  |
|       - | 5296 | `/* number array_sum(array $array )` |
|       - | 5297 | ` * (See block-coment above)` |
|       - | 5298 | ` */` |
|      52 | 5299 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5300 |  |
|       - | 5301 | `	ph7_hashmap_node *pEntry;` |
|       - | 5302 | `	ph7_hashmap *pMap;` |
|       - | 5303 | `	ph7_value *pObj;` |
|      54 | 5304 | `	int useDouble = 0;` |
|       - | 5305 | `	sxu32 n;` |
|       - | 5306 | `	/* PHP requires exactly one argument */` |
|      54 | 5307 | `	if( nArg != 1 ){` |
|       7 | 5308 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5309 | `			"ArgumentCountError",` |
|       - | 5310 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5311 | `			nArg` |
|       - | 5312 | `			);` |
|       - | 5313 | `	}` |
|       - | 5314 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5315 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5316 | `		/* Type mismatch -> TypeError */` |
|       7 | 5317 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5318 | `			"TypeError",` |
|       - | 5319 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5320 | `			ph7_type_name(apArg[0])` |
|       - | 5321 | `			);` |
|       - | 5322 | `	}` |
|      46 | 5323 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5324 | `	if( pMap->nEntry < 1 ){` |
|       - | 5325 | `		/* Nothing to compute,return 0 */` |
|       3 | 5326 | `		ph7_result_int(pCtx,0);` |
|       3 | 5327 | `		return PH7_OK;` |
|       - | 5328 | `	}` |
|       - | 5329 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5330 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5331 | `	 */` |
|      44 | 5332 | `	pEntry = pMap->pFirst;` |
|     112 | 5333 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5334 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5335 | `		if( pObj ){` |
|      94 | 5336 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5337 | `				useDouble = 1;` |
|      19 | 5338 | `				break;` |
|       - | 5339 | `			}` |
|      76 | 5340 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5341 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5342 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5343 | `				sxu32 i;` |
|      23 | 5344 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5345 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5346 | `						useDouble = 1;` |
|       7 | 5347 | `						break;` |
|       - | 5348 | `					}` |
|       6 | 5349 | `				}` |
|      13 | 5350 | `				if( useDouble ){` |
|       7 | 5351 | `					break;` |
|       - | 5352 | `				}` |
|       3 | 5353 | `			}` |
|      34 | 5354 | `		}` |
|      70 | 5355 | `		pEntry = pEntry->pPrev;` |
|      36 | 5356 | `	}` |
|      44 | 5357 | `	if( useDouble ){` |
|      25 | 5358 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5359 | `	}else{` |
|      20 | 5360 | `		Int64Sum(pCtx,pMap);` |
|       - | 5361 | `	}` |
|      44 | 5362 | `	return PH7_OK;` |
|      28 | 5363 |  |
|       - | 5364 | `/*` |
|       - | 5365 | ` * number array_product(array $array )` |
|       - | 5366 | ` *  Calculate the product of values in an array.` |
|       - | 5367 | ` * Parameters` |
|       - | 5368 | ` *  $array: The input array.` |
|       - | 5369 | ` * Return` |
|       - | 5370 | ` *  Returns the product of values as an integer or float.` |
|       - | 5371 | ` */` |
|     ! 0 | 5372 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5373 |  |
|       - | 5374 | `	ph7_hashmap_node *pEntry;` |
|       - | 5375 | `	ph7_value *pObj;` |
|       - | 5376 | `	double dProd;` |
|       - | 5377 | `	sxu32 n;` |
|     ! 0 | 5378 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5379 | `	dProd = 1;` |
|     ! 0 | 5380 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5381 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5382 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5383 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5384 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5385 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5386 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5387 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5388 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5389 | `					double dv = 0;` |
|     ! 0 | 5390 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5391 | `					dProd *= dv;` |
|     ! 0 | 5392 | `				}` |
|     ! 0 | 5393 | `			}` |
|     ! 0 | 5394 | `		}` |
|       - | 5395 | `		/* Point to the next entry */` |
|     ! 0 | 5396 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5397 | `	}` |
|       - | 5398 | `	/* Return product */` |
|     ! 0 | 5399 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5400 |  |
|     ! 0 | 5401 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5402 |  |
|       - | 5403 | `	ph7_hashmap_node *pEntry;` |
|       - | 5404 | `	ph7_value *pObj;` |
|       - | 5405 | `	sxi64 nProd;` |
|       - | 5406 | `	sxu32 n;` |
|     ! 0 | 5407 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5408 | `	nProd = 1;` |
|     ! 0 | 5409 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5410 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5411 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5412 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5413 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5414 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5415 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5416 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5417 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5418 | `					sxi64 nv = 0;` |
|     ! 0 | 5419 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5420 | `					nProd *= nv;` |
|     ! 0 | 5421 | `				}` |
|     ! 0 | 5422 | `			}` |
|     ! 0 | 5423 | `		}` |
|       - | 5424 | `		/* Point to the next entry */` |
|     ! 0 | 5425 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5426 | `	}` |
|       - | 5427 | `	/* Return product */` |
|     ! 0 | 5428 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5429 |  |
|       - | 5430 | `/* number array_product(array $array )` |
|       - | 5431 | ` * (See block-block comment above)` |
|       - | 5432 | ` */` |
|     ! 0 | 5433 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5434 |  |
|       - | 5435 | `	ph7_hashmap *pMap;` |
|       - | 5436 | `	ph7_value *pObj;` |
|     ! 0 | 5437 | `	if( nArg < 1 ){` |
|       - | 5438 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5439 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5440 | `		return PH7_OK;` |
|       - | 5441 | `	}` |
|       - | 5442 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5443 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5444 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5445 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5446 | `		return PH7_OK;` |
|       - | 5447 | `	}` |
|     ! 0 | 5448 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5449 | `	if( pMap->nEntry < 1 ){` |
|       - | 5450 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5451 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5452 | `		return PH7_OK;` |
|       - | 5453 | `	}` |
|       - | 5454 | `	/* If the first element is of type float,then perform floating` |
|       - | 5455 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5456 | `	 */` |
|     ! 0 | 5457 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5458 | `	if( pObj == 0 ){` |
|     ! 0 | 5459 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5460 | `		return PH7_OK;` |
|       - | 5461 | `	}` |
|     ! 0 | 5462 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5463 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5464 | `	}else{` |
|     ! 0 | 5465 | `		Int64Prod(pCtx,pMap);` |
|       - | 5466 | `	}` |
|     ! 0 | 5467 | `	return PH7_OK;` |
|     ! 0 | 5468 |  |
|       - | 5469 | `/*` |
|       - | 5470 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5471 | ` *  Pick one or more random entries out of an array.` |
|       - | 5472 | ` * Parameters` |
|       - | 5473 | ` * $input` |
|       - | 5474 | ` *  The input array.` |
|       - | 5475 | ` * $num_req` |
|       - | 5476 | ` *  Specifies how many entries you want to pick.` |
|       - | 5477 | ` * Return` |
|       - | 5478 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5479 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5480 | ` *  NULL is returned on failure.` |
|       - | 5481 | ` */` |
|       6 | 5482 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5483 |  |
|       - | 5484 | `	ph7_hashmap_node *pNode;` |
|       - | 5485 | `	ph7_hashmap *pMap;` |
|       7 | 5486 | `	int nItem = 1;` |
|       7 | 5487 | `	if( nArg < 1 ){` |
|       - | 5488 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5489 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5490 | `		return PH7_OK;` |
|       - | 5491 | `	}` |
|       - | 5492 | `	/* Make sure we are dealing with an array */` |
|       7 | 5493 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5494 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5495 | `		return PH7_OK;` |
|       - | 5496 | `	}` |
|       - | 5497 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5498 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5499 | `	if(pMap->nEntry < 1 ){` |
|       - | 5500 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5501 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5502 | `		return PH7_OK;` |
|       - | 5503 | `	}` |
|       7 | 5504 | `	if( nArg > 1 ){` |
|       3 | 5505 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5506 | `	}` |
|       7 | 5507 | `	if( nItem < 2 ){` |
|       - | 5508 | `		sxu32 nEntry;` |
|       - | 5509 | `		/* Select a random number */` |
|       5 | 5510 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5511 | `		/* Extract the desired entry.` |
|       - | 5512 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5513 | `		 */` |
|       5 | 5514 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       2 | 5515 | `			pNode = pMap->pLast;` |
|       2 | 5516 | `			nEntry = pMap->nEntry - nEntry;` |
|       2 | 5517 | `			if( nEntry > 1 ){` |
|     ! 0 | 5518 | `				for(;;){` |
|     ! 0 | 5519 | `					if( nEntry == 0 ){` |
|     ! 0 | 5520 | `						break;` |
|       - | 5521 | `					}` |
|       - | 5522 | `					/* Point to the previous entry */` |
|     ! 0 | 5523 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5524 | `					nEntry--;` |
|     ! 0 | 5525 | `				}` |
|     ! 0 | 5526 | `			}` |
|       2 | 5527 | `		}else{` |
|       4 | 5528 | `			pNode = pMap->pFirst;` |
|       2 | 5529 | `			for(;;){` |
|       6 | 5530 | `				if( nEntry == 0 ){` |
|       4 | 5531 | `					break;` |
|       - | 5532 | `				}` |
|       - | 5533 | `				/* Point to the next entry */` |
|       2 | 5534 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       2 | 5535 | `				nEntry--;` |
|     ! 0 | 5536 | `			}` |
|       - | 5537 | `		}` |
|       5 | 5538 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5539 | `			/* Int key */` |
|       3 | 5540 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5541 | `		}else{` |
|       - | 5542 | `			/* Blob key */` |
|       3 | 5543 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5544 | `		}` |
|       3 | 5545 | `	}else{` |
|       - | 5546 | `		ph7_value sKey,*pArray;` |
|       - | 5547 | `		ph7_hashmap *pDest;` |
|       - | 5548 | `		/* Create a new array */` |
|       3 | 5549 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5550 | `		if( pArray == 0 ){` |
|     ! 0 | 5551 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5552 | `			return PH7_OK;` |
|       - | 5553 | `		}` |
|       - | 5554 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5555 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5556 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5557 | `		/* Copy the first n items */` |
|       3 | 5558 | `		pNode = pMap->pFirst;` |
|       3 | 5559 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5560 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5561 | `		}` |
|       7 | 5562 | `		while( nItem > 0){` |
|       5 | 5563 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5564 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5565 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5566 | `			/* Point to the next entry */` |
|       5 | 5567 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5568 | `			nItem--;` |
|       1 | 5569 | `		}` |
|       - | 5570 | `		/* Shuffle the array */` |
|       3 | 5571 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5572 | `		/* Rehash node */` |
|       3 | 5573 | `		HashmapSortRehash(pDest);` |
|       - | 5574 | `		/* Return the random array */` |
|       3 | 5575 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5576 | `	}` |
|       7 | 5577 | `	return PH7_OK;` |
|       4 | 5578 |  |
|       - | 5579 | `/*` |
|       - | 5580 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5581 | ` *  Split an array into chunks.` |
|       - | 5582 | ` * Parameters` |
|       - | 5583 | ` * $input` |
|       - | 5584 | ` *   The array to work on` |
|       - | 5585 | ` * $size` |
|       - | 5586 | ` *   The size of each chunk` |
|       - | 5587 | ` * $preserve_keys` |
|       - | 5588 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5589 | ` *   the chunk numerically.` |
|       - | 5590 | ` * Return` |
|       - | 5591 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5592 | ` *  zero, with each dimension containing size elements.` |
|       - | 5593 | ` */` |
|      42 | 5594 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5595 |  |
|       - | 5596 | `	ph7_value *pArray,*pChunk;` |
|       - | 5597 | `	ph7_hashmap_node *pEntry;` |
|       - | 5598 | `	ph7_hashmap *pMap;` |
|       - | 5599 | `	int bPreserve;` |
|       - | 5600 | `	sxu32 nChunk;` |
|       - | 5601 | `	sxu32 nSize;` |
|       - | 5602 | `	sxu32 n;` |
|       - | 5603 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5604 | `	if( nArg < 2 ){` |
|       - | 5605 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5606 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5607 | `			"ArgumentCountError",` |
|       - | 5608 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5609 | `			nArg` |
|       - | 5610 | `			);` |
|       - | 5611 | `	}` |
|      42 | 5612 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5613 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5614 | `			"TypeError",` |
|       - | 5615 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5616 | `			ph7_type_name(apArg[0])` |
|       - | 5617 | `			);` |
|       - | 5618 | `	}` |
|       - | 5619 | `	/* Create a new array */` |
|      40 | 5620 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5621 | `	if( pArray == 0 ){` |
|     ! 0 | 5622 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5623 | `		return PH7_OK;` |
|       - | 5624 | `	}` |
|       - | 5625 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5626 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5627 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5628 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5629 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5630 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5631 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5632 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5633 | `			"TypeError",` |
|       - | 5634 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5635 | `			ph7_type_name(apArg[1])` |
|       - | 5636 | `			);` |
|       - | 5637 | `	}` |
|       - | 5638 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5639 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5640 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5641 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5642 | `		int len;` |
|       3 | 5643 | `		sxu8 bReal = FALSE;` |
|       3 | 5644 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5645 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5646 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5647 | `				"TypeError",` |
|       - | 5648 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5649 | `				);` |
|       - | 5650 | `		}` |
|     ! 0 | 5651 | `		if( bReal ){` |
|       - | 5652 | `			/* float-string -> warn but allow */` |
|     ! 0 | 5653 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5654 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 5655 | `				zStr` |
|       - | 5656 | `				);` |
|     ! 0 | 5657 | `		}` |
|     ! 0 | 5658 | `	}` |
|       - | 5659 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 5660 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 5661 | `	 * later via ph7_value_to_int. */` |
|      38 | 5662 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5663 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5664 | `		sxi64 i = (sxi64)d;` |
|       3 | 5665 | `		if( d != (double)i ){` |
|       4 | 5666 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5667 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 5668 | `				d` |
|       - | 5669 | `				);` |
|       1 | 5670 | `		}` |
|       1 | 5671 | `	}` |
|       - | 5672 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5673 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5674 | `	{` |
|      38 | 5675 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 5676 | `		if( nSizeSigned < 1 ){` |
|       - | 5677 | `			/* size <= 0 -> ValueError */` |
|       5 | 5678 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5679 | `				"ValueError",` |
|       - | 5680 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5681 | `				);` |
|       - | 5682 | `		}` |
|      34 | 5683 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5684 | `	}` |
|      34 | 5685 | `	if( nSize >= pMap->nEntry ){` |
|       - | 5686 | `		/* Return the whole array */` |
|       3 | 5687 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 5688 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5689 | `		return PH7_OK;` |
|       - | 5690 | `	}` |
|      32 | 5691 | `	bPreserve = 0;` |
|      32 | 5692 | `	if( nArg > 2 ){` |
|       - | 5693 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5694 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5695 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5696 | `		 * normally, matching PHP behaviour. */` |
|      45 | 5697 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 5698 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 5699 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 5700 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5701 | `				"TypeError",` |
|       - | 5702 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 5703 | `				ph7_type_name(apArg[2])` |
|       - | 5704 | `				);` |
|       - | 5705 | `		}` |
|      21 | 5706 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 5707 | `	}` |
|       - | 5708 | `	/* Start processing */` |
|      27 | 5709 | `	pEntry = pMap->pFirst;` |
|      27 | 5710 | `	nChunk = 0;` |
|      27 | 5711 | `	pChunk = 0;` |
|      27 | 5712 | `	n = pMap->nEntry;` |
|      56 | 5713 | `	for( ;; ){` |
|     113 | 5714 | `		if( n < 1 ){` |
|       - | 5715 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5716 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5717 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5718 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5719 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5720 | `			 * exists. */` |
|      27 | 5721 | `			if( pChunk ){` |
|      27 | 5722 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 5723 | `			}` |
|      27 | 5724 | `			break;` |
|       - | 5725 | `		}` |
|      87 | 5726 | `		if( nChunk < 1 ){` |
|      71 | 5727 | `			if( pChunk ){` |
|       - | 5728 | `				/* Put the first chunk */` |
|      45 | 5729 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 5730 | `			}` |
|       - | 5731 | `			/* Create a new dimension */` |
|      71 | 5732 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5733 | `												   * will be automatically released as soon we return` |
|       - | 5734 | `												   * from this function */` |
|      71 | 5735 | `			if( pChunk == 0 ){` |
|     ! 0 | 5736 | `				break;` |
|       - | 5737 | `			}` |
|      71 | 5738 | `			nChunk = nSize;` |
|      35 | 5739 | `		}` |
|       - | 5740 | `		/* Insert the entry */` |
|      87 | 5741 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5742 | `		/* Point to the next entry */` |
|      87 | 5743 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 5744 | `		nChunk--;` |
|      87 | 5745 | `		n--;` |
|       1 | 5746 | `	}` |
|       - | 5747 | `	/* Return the multidimensional array */` |
|      27 | 5748 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5749 | `	return PH7_OK;` |
|      23 | 5750 |  |
|       - | 5751 | `/*` |
|       - | 5752 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5753 | ` *  Pad array to the specified length with a value.` |
|       - | 5754 | ` * $input` |
|       - | 5755 | ` *   Initial array of values to pad.` |
|       - | 5756 | ` * $pad_size` |
|       - | 5757 | ` *   New size of the array.` |
|       - | 5758 | ` * $pad_value` |
|       - | 5759 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5760 | ` */` |
|      28 | 5761 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5762 |  |
|       - | 5763 | `	ph7_hashmap *pMap;` |
|       - | 5764 | `	ph7_value *pArray;` |
|       - | 5765 | `	int nEntry;` |
|      30 | 5766 | `	if( nArg != 3 ){` |
|      10 | 5767 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5768 | `			"ArgumentCountError",` |
|       - | 5769 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 5770 | `			nArg` |
|       - | 5771 | `			);` |
|       - | 5772 | `	}` |
|      24 | 5773 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5774 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5775 | `			"TypeError",` |
|       - | 5776 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5777 | `			ph7_type_name(apArg[0])` |
|       - | 5778 | `			);` |
|       - | 5779 | `	}` |
|       - | 5780 | `	/* Create a new array */` |
|      21 | 5781 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 5782 | `	if( pArray == 0 ){` |
|     ! 0 | 5783 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5784 | `		return PH7_OK;` |
|       - | 5785 | `	}` |
|       - | 5786 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 5787 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5788 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 5789 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 5790 | `	if( nEntry < 0 ){` |
|       9 | 5791 | `		nEntry = -nEntry;` |
|       9 | 5792 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 5793 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5794 | `			/* Insert given items first */` |
|      17 | 5795 | `			while( nEntry > 0 ){` |
|      13 | 5796 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 5797 | `				nEntry--;` |
|       1 | 5798 | `			}` |
|       - | 5799 | `			/* Merge the two arrays */` |
|       5 | 5800 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5801 | `		}else{` |
|       5 | 5802 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5803 | `		}` |
|      17 | 5804 | `	}else if( nEntry > 0 ){` |
|      11 | 5805 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 5806 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5807 | `			/* Merge the two arrays first */` |
|       7 | 5808 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5809 | `			/* Insert given items */` |
|      25 | 5810 | `			while( nEntry > 0 ){` |
|      19 | 5811 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 5812 | `				nEntry--;` |
|       1 | 5813 | `			}` |
|       4 | 5814 | `		}else{` |
|       5 | 5815 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5816 | `		}` |
|       6 | 5817 | `	}else{` |
|       - | 5818 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 5819 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5820 | `	}` |
|       - | 5821 | `	/* Return the new array */` |
|      21 | 5822 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 5823 | `	return PH7_OK;` |
|      16 | 5824 |  |
|       - | 5825 | `/*` |
|       - | 5826 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5827 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5828 | ` * Parameters` |
|       - | 5829 | ` * $array` |
|       - | 5830 | ` *   The array in which elements are replaced.` |
|       - | 5831 | ` * $array1` |
|       - | 5832 | ` *   The array from which elements will be extracted.` |
|       - | 5833 | ` * ....` |
|       - | 5834 | ` *  More arrays from which elements will be extracted.` |
|       - | 5835 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5836 | ` * Return` |
|       - | 5837 | ` *  Returns an array, or NULL if an error occurs.` |
|       - | 5838 | ` */` |
|       2 | 5839 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5840 |  |
|       - | 5841 | `	ph7_hashmap *pMap;` |
|       - | 5842 | `	ph7_value *pArray;` |
|       - | 5843 | `	int i;` |
|       3 | 5844 | `	if( nArg < 1 ){` |
|       - | 5845 | `		/* Invalid arguments,return NULL */` |
|     ! 0 | 5846 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5847 | `		return PH7_OK;` |
|       - | 5848 | `	}` |
|       - | 5849 | `	/* Create a new array */` |
|       3 | 5850 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5851 | `	if( pArray == 0 ){` |
|     ! 0 | 5852 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5853 | `		return PH7_OK;` |
|       - | 5854 | `	}` |
|       - | 5855 | `	/* Perform the requested operation */` |
|       7 | 5856 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       5 | 5857 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|     ! 0 | 5858 | `			continue;` |
|       - | 5859 | `		}` |
|       - | 5860 | `		/* Point to the internal representation of the input hashmap */` |
|       5 | 5861 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       5 | 5862 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5863 | `	}` |
|       - | 5864 | `	/* Return the new array */` |
|       3 | 5865 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5866 | `	return PH7_OK;` |
|       2 | 5867 |  |
|       - | 5868 | `/*` |
|       - | 5869 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 5870 | ` *  Filters elements of an array using a callback function.` |
|       - | 5871 | ` * Parameters` |
|       - | 5872 | ` *  $input` |
|       - | 5873 | ` *    The array to iterate over` |
|       - | 5874 | ` * $callback` |
|       - | 5875 | ` *    The callback function to use` |
|       - | 5876 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 5877 | ` *    will be removed.` |
|       - | 5878 | ` * Return` |
|       - | 5879 | ` *  The filtered array.` |
|       - | 5880 | ` */` |
|      18 | 5881 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5882 |  |
|       - | 5883 | `	ph7_hashmap_node *pEntry;` |
|       - | 5884 | `	ph7_hashmap *pMap;` |
|       - | 5885 | `	ph7_value *pArray;` |
|       - | 5886 | `	ph7_value sResult;   /* Callback result */` |
|       - | 5887 | `	ph7_value *pValue;` |
|       - | 5888 | `	sxi32 rc;` |
|       - | 5889 | `	int keep;` |
|       - | 5890 | `	sxu32 n;` |
|      20 | 5891 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5892 | `		/* Invalid arguments,return NULL */` |
|       5 | 5893 | `		ph7_result_null(pCtx);` |
|       5 | 5894 | `		return PH7_OK;` |
|       - | 5895 | `	}` |
|       - | 5896 | `	/* Create a new array */` |
|      16 | 5897 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 5898 | `	if( pArray == 0 ){` |
|     ! 0 | 5899 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5900 | `		return PH7_OK;` |
|       - | 5901 | `	}` |
|       - | 5902 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 5903 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 5904 | `	pEntry = pMap->pFirst;` |
|      16 | 5905 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 5906 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5907 | `	/* Perform the requested operation */` |
|      66 | 5908 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5909 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 5910 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 5911 | `		if( pValue == 0 ){` |
|       - | 5912 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5913 | `			keep = FALSE;` |
|      54 | 5914 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5915 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 5916 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 5917 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 5918 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 5919 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5920 | `					int len;` |
|       3 | 5921 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 5922 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5923 | `						"TypeError",` |
|       - | 5924 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 5925 | `						zName` |
|       - | 5926 | `						);` |
|     ! 0 | 5927 | `				}else{` |
|     ! 0 | 5928 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5929 | `						"TypeError",` |
|       - | 5930 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 5931 | `						ph7_type_name(apArg[1])` |
|       - | 5932 | `						);` |
|       - | 5933 | `				}` |
|       - | 5934 | `			}` |
|      23 | 5935 | `			keep = FALSE;` |
|      23 | 5936 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 5937 | `			if( rc == SXRET_OK ){` |
|       - | 5938 | `				/* Perform a boolean cast */` |
|      23 | 5939 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 5940 | `			}` |
|      23 | 5941 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 5942 | `		}else{` |
|       - | 5943 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 5944 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 5945 | `			 * the case where the callback argument is missing entirely.` |
|       - | 5946 | `			 */` |
|      29 | 5947 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 5948 | `		}` |
|      51 | 5949 | `		if( keep ){` |
|       - | 5950 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 5951 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5952 | `		}` |
|       - | 5953 | `		/* Point to the next entry */` |
|      51 | 5954 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 5955 | `	}` |
|      13 | 5956 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5957 | `	return PH7_OK;` |
|      11 | 5958 |  |
|       - | 5959 | `/*` |
|       - | 5960 | ` * array array_map(callback $callback,array $arr1)` |
|       - | 5961 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5962 | ` * Parameters` |
|       - | 5963 | ` *  $callback` |
|       - | 5964 | ` *   Callback function to run for each element in each array.` |
|       - | 5965 | ` * $arr1` |
|       - | 5966 | ` *   An array to run through the callback function.` |
|       - | 5967 | ` * Return` |
|       - | 5968 | ` *  Returns an array containing all the elements of arr1 after applying` |
|       - | 5969 | ` *  the callback function to each one.` |
|       - | 5970 | ` * NOTE:` |
|       - | 5971 | ` *  array_map() passes only a single value to the callback.` |
|       - | 5972 | ` */` |
|      10 | 5973 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5974 |  |
|       - | 5975 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5976 | `	ph7_hashmap_node *pEntry;` |
|       - | 5977 | `	ph7_hashmap *pMap;` |
|       - | 5978 | `	sxu32 n;` |
|      11 | 5979 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 5980 | `		/* Invalid arguments,return NULL */` |
|       5 | 5981 | `		ph7_result_null(pCtx);` |
|       5 | 5982 | `		return PH7_OK;` |
|       - | 5983 | `	}` |
|       - | 5984 | `	/* Create a new array */` |
|       7 | 5985 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5986 | `	if( pArray == 0 ){` |
|     ! 0 | 5987 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5988 | `		return PH7_OK;` |
|       - | 5989 | `	}` |
|       - | 5990 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5991 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       7 | 5992 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       7 | 5993 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 5994 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 5995 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 5996 | `	/* Perform the requested operation */` |
|       7 | 5997 | `	pEntry = pMap->pFirst;` |
|      21 | 5998 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5999 | `		/* Extrcat the node value */` |
|      15 | 6000 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      15 | 6001 | `		if( pValue ){` |
|       - | 6002 | `			sxi32 rc;` |
|       - | 6003 | `			/* Invoke the supplied callback */` |
|      15 | 6004 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 6005 | `			/* Extract the node key */` |
|      15 | 6006 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      15 | 6007 | `			if( rc != SXRET_OK ){` |
|       - | 6008 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 6009 | `				ph7_array_add_elem(pArray,&sKey,pValue); /* Keep the same value */` |
|     ! 0 | 6010 | `			}else{` |
|       - | 6011 | `				/* Insert the callback return value */` |
|      15 | 6012 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6013 | `			}` |
|      15 | 6014 | `			PH7_MemObjRelease(&sKey);` |
|      15 | 6015 | `			PH7_MemObjRelease(&sResult);` |
|       7 | 6016 | `		}` |
|       - | 6017 | `		/* Point to the next entry */` |
|      15 | 6018 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 6019 | `	}` |
|       7 | 6020 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 6021 | `	return PH7_OK;` |
|       6 | 6022 |  |
|       - | 6023 | `/*` |
|       - | 6024 | ` * value array_reduce(array $input,callback $function[, value $initial = NULL ])` |
|       - | 6025 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6026 | ` * Parameters` |
|       - | 6027 | ` *  $input` |
|       - | 6028 | ` *   The input array.` |
|       - | 6029 | ` *  $function` |
|       - | 6030 | ` *  The callback function.` |
|       - | 6031 | ` * $initial` |
|       - | 6032 | ` *  If the optional initial is available, it will be used at the beginning` |
|       - | 6033 | ` *  of the process, or as a final result in case the array is empty.` |
|       - | 6034 | ` * Return` |
|       - | 6035 | ` *  Returns the resulting value.` |
|       - | 6036 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6037 | ` */` |
|       4 | 6038 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6039 |  |
|       - | 6040 | `	ph7_hashmap_node *pEntry;` |
|       - | 6041 | `	ph7_hashmap *pMap;` |
|       - | 6042 | `	ph7_value *pValue;` |
|       - | 6043 | `	ph7_value sResult;` |
|       - | 6044 | `	sxu32 n;` |
|       5 | 6045 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6046 | `		/* Invalid/Missing arguments,return NULL */` |
|     ! 0 | 6047 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6048 | `		return PH7_OK;` |
|       - | 6049 | `	}` |
|       - | 6050 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 6051 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6052 | `	/* Assume a NULL initial value */` |
|       5 | 6053 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       5 | 6054 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       5 | 6055 | `	if( nArg > 2 ){` |
|       - | 6056 | `		/* Set the initial value */` |
|       5 | 6057 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       2 | 6058 | `	}` |
|       - | 6059 | `	/* Perform the requested operation */` |
|       5 | 6060 | `	pEntry = pMap->pFirst;` |
|      19 | 6061 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6062 | `		/* Extract the node value */` |
|      15 | 6063 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6064 | `		/* Invoke the supplied callback */` |
|      15 | 6065 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6066 | `		/* Point to the next entry */` |
|      15 | 6067 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 6068 | `	}` |
|       5 | 6069 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       5 | 6070 | `	PH7_MemObjRelease(&sResult);` |
|       5 | 6071 | `	return PH7_OK;` |
|       3 | 6072 |  |
|       - | 6073 | `/*` |
|       - | 6074 | ` * bool array_walk(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6075 | ` *  Apply a user function to every member of an array.` |
|       - | 6076 | ` * Parameters` |
|       - | 6077 | ` *  $array` |
|       - | 6078 | ` *   The input array.` |
|       - | 6079 | ` * $funcname` |
|       - | 6080 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6081 | ` *  the first, and the key/index second.` |
|       - | 6082 | ` * Note:` |
|       - | 6083 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6084 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6085 | ` *  be made in the original array itself.` |
|       - | 6086 | ` * $userdata` |
|       - | 6087 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6088 | ` *  to the callback funcname.` |
|       - | 6089 | ` * Return` |
|       - | 6090 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6091 | ` */` |
|      12 | 6092 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6093 |  |
|       - | 6094 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6095 | `	ph7_hashmap_node *pEntry;` |
|       - | 6096 | `	ph7_hashmap *pMap;` |
|       - | 6097 | `	sxi32 rc;` |
|       - | 6098 | `	sxu32 n;` |
|      13 | 6099 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6100 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6101 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6102 | `		return PH7_OK;` |
|       - | 6103 | `	}` |
|      13 | 6104 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6105 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6106 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 6107 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      13 | 6108 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6109 | `	/* Perform the desired operation */` |
|      13 | 6110 | `	pEntry = pMap->pFirst;` |
|      41 | 6111 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6112 | `		/* Extract the node value */` |
|      29 | 6113 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      29 | 6114 | `		if( pValue ){` |
|       - | 6115 | `			/* Extract the entry key */` |
|      29 | 6116 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6117 | `			/* Invoke the supplied callback */` |
|      29 | 6118 | `			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      29 | 6119 | `			PH7_MemObjRelease(&sKey);` |
|      29 | 6120 | `			if( rc != SXRET_OK ){` |
|       - | 6121 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 6122 | `				ph7_result_bool(pCtx,0); /* return FALSE */` |
|     ! 0 | 6123 | `				return PH7_OK;` |
|       - | 6124 | `			}` |
|      14 | 6125 | `		}` |
|       - | 6126 | `		/* Point to the next entry */` |
|      29 | 6127 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6128 | `	}` |
|       - | 6129 | `	/* All done,return TRUE */` |
|      13 | 6130 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6131 | `	return PH7_OK;` |
|       7 | 6132 |  |
|       - | 6133 | `/*` |
|       - | 6134 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6135 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6136 | ` */` |
|       6 | 6137 | `static int HashmapWalkRecursive(` |
|       - | 6138 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6139 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6140 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6141 | `	int iNest             /* Nesting level */` |
|       - | 6142 | `	)` |
|       1 | 6143 |  |
|       - | 6144 | `	ph7_hashmap_node *pEntry;` |
|       - | 6145 | `	ph7_value *pValue,sKey;` |
|       - | 6146 | `	sxi32 rc;` |
|       - | 6147 | `	sxu32 n;` |
|       - | 6148 | `	/* Iterate throw hashmap entries */` |
|       7 | 6149 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 6150 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 6151 | `	pEntry = pMap->pFirst;` |
|      17 | 6152 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6153 | `		/* Extract the node value */` |
|      11 | 6154 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      11 | 6155 | `		if( pValue ){` |
|      11 | 6156 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 6157 | `				if( iNest < 32 ){` |
|       - | 6158 | `					/* Recurse */` |
|       5 | 6159 | `					iNest++;` |
|       5 | 6160 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|       5 | 6161 | `					iNest--;` |
|       2 | 6162 | `				}` |
|       3 | 6163 | `			}else{` |
|       - | 6164 | `				/* Extract the node key */` |
|       7 | 6165 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6166 | `				/* Invoke the supplied callback */` |
|       7 | 6167 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|       7 | 6168 | `				PH7_MemObjRelease(&sKey);` |
|       7 | 6169 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6170 | `					return rc;` |
|       - | 6171 | `				}` |
|       - | 6172 | `			}` |
|       5 | 6173 | `		}` |
|       - | 6174 | `		/* Point to the next entry */` |
|      11 | 6175 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 6176 | `	}` |
|       7 | 6177 | `	return SXRET_OK;` |
|       4 | 6178 |  |
|       - | 6179 | `/*` |
|       - | 6180 | ` * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6181 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6182 | ` * Parameters` |
|       - | 6183 | ` *  $array` |
|       - | 6184 | ` *   The input array.` |
|       - | 6185 | ` * $funcname` |
|       - | 6186 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6187 | ` *  the first, and the key/index second.` |
|       - | 6188 | ` * Note:` |
|       - | 6189 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6190 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6191 | ` *  be made in the original array itself.` |
|       - | 6192 | ` * $userdata` |
|       - | 6193 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6194 | ` *  to the callback funcname.` |
|       - | 6195 | ` * Return` |
|       - | 6196 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6197 | ` */` |
|       2 | 6198 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6199 |  |
|       - | 6200 | `	ph7_hashmap *pMap;` |
|       - | 6201 | `	sxi32 rc;` |
|       3 | 6202 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6203 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6204 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6205 | `		return PH7_OK;` |
|       - | 6206 | `	}` |
|       - | 6207 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 6208 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6209 | `	/* Perform the desired operation */` |
|       3 | 6210 | `	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6211 | `	/* All done */` |
|       3 | 6212 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|       3 | 6213 | `	return PH7_OK;` |
|       2 | 6214 |  |
|       - | 6215 | `/*` |
|       - | 6216 | ` * Table of hashmap functions.` |
|       - | 6217 | ` */` |
|       - | 6218 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6219 | `	{"count",             ph7_hashmap_count },` |
|       - | 6220 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6221 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6222 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6223 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6224 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6225 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6226 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6227 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6228 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6229 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6230 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6231 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6232 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6233 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6234 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6235 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6236 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6237 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6238 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6239 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6240 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6241 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6242 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6243 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6244 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6245 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6246 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6247 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6248 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6249 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6250 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6251 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6252 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6253 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6254 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6255 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6256 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6257 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6258 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6259 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6260 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6261 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6262 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6263 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6264 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6265 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6266 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6267 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6268 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6269 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6270 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6271 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6272 | `	{"current",           ph7_hashmap_current },` |
|       - | 6273 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6274 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6275 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6276 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6277 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6278 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6279 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6280 | `};` |
|       - | 6281 | `/*` |
|       - | 6282 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6283 | ` */` |
|    1424 | 6284 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6285 |  |
|       - | 6286 | `	sxu32 n;` |
|   88290 | 6287 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|   86866 | 6288 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   43434 | 6289 | `	}` |
|    1426 | 6290 |  |
|       - | 6291 | `/*` |
|       - | 6292 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6293 | ` * the BLOB given as the first argument.` |
|       - | 6294 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6295 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6296 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6297 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6298 | ` */` |
|      26 | 6299 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6300 |  |
|       - | 6301 | `	ph7_hashmap_node *pEntry;` |
|       - | 6302 | `	ph7_value *pObj;` |
|      28 | 6303 | `	sxu32 n = 0;` |
|       - | 6304 | `	int isRef;` |
|       - | 6305 | `	sxi32 rc;` |
|       - | 6306 | `	int i;` |
|      28 | 6307 | `	if( nDepth > 31 ){` |
|       - | 6308 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6309 | `		/* Nesting limit reached */` |
|     ! 0 | 6310 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6311 | `		if( ShowType ){` |
|     ! 0 | 6312 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6313 | `		}` |
|     ! 0 | 6314 | `		return SXERR_LIMIT;` |
|       - | 6315 | `	}` |
|       - | 6316 | `	/* Point to the first inserted entry */` |
|      28 | 6317 | `	pEntry = pMap->pFirst;` |
|      28 | 6318 | `	rc = SXRET_OK;` |
|      28 | 6319 | `	if( !ShowType ){` |
|      15 | 6320 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6321 | `	}` |
|       - | 6322 | `	/* Total entries */` |
|      28 | 6323 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6324 | `#ifdef __WINNT__` |
|       2 | 6325 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6326 | `#else` |
|      26 | 6327 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6328 | `#endif` |
|      62 | 6329 | `	for(;;){` |
|     126 | 6330 | `		if( n >= pMap->nEntry ){` |
|      28 | 6331 | `			break;` |
|       - | 6332 | `		}` |
|     198 | 6333 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 6334 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 6335 | `		}` |
|       - | 6336 | `		/* Dump key */` |
|     100 | 6337 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 6338 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 6339 | `		}else{` |
|     101 | 6340 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6341 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6342 | `		}` |
|       - | 6343 | `#ifdef __WINNT__` |
|       2 | 6344 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6345 | `#else` |
|      98 | 6346 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6347 | `#endif` |
|       - | 6348 | `		/* Dump node value */` |
|     100 | 6349 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 6350 | `		isRef = 0;` |
|     100 | 6351 | `		if( pObj ){` |
|     100 | 6352 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6353 | `				/* Referenced object */` |
|     ! 0 | 6354 | `				isRef = 1;` |
|     ! 0 | 6355 | `			}` |
|     100 | 6356 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 6357 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6358 | `				break;` |
|       - | 6359 | `			}` |
|      49 | 6360 | `		}` |
|       - | 6361 | `		/* Point to the next entry */` |
|     100 | 6362 | `		n++;` |
|     100 | 6363 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6364 | `	}` |
|      54 | 6365 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 6366 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 6367 | `	}` |
|      28 | 6368 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 6369 | `	return rc;` |
|      15 | 6370 |  |
|       - | 6371 | `/*` |
|       - | 6372 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6373 | ` * retrieved entry.` |
|       - | 6374 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6375 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6376 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6377 | ` * a value different from PH7_OK.` |
|       - | 6378 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6379 | ` */` |
|   19996 | 6380 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6381 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6382 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6383 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6384 | `	)` |
|       2 | 6385 |  |
|       - | 6386 | `	ph7_hashmap_node *pEntry;` |
|       - | 6387 | `	ph7_value sKey,sValue;` |
|       - | 6388 | `	sxi32 rc;` |
|       - | 6389 | `	sxu32 n;` |
|       - | 6390 | `	/* Initialize walker parameter */` |
|   19998 | 6391 | `	rc = SXRET_OK;` |
|   19998 | 6392 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   19998 | 6393 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   19998 | 6394 | `	n = pMap->nEntry;` |
|   19998 | 6395 | `	pEntry = pMap->pFirst;` |
|       - | 6396 | `	/* Start the iteration process */` |
|   52692 | 6397 | `	for(;;){` |
|  105386 | 6398 | `		if( n < 1 ){` |
|   19998 | 6399 | `			break;` |
|       - | 6400 | `		}` |
|       - | 6401 | `		/* Extract a copy of the key and a copy the current value */` |
|   85390 | 6402 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   85390 | 6403 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6404 | `		/* Invoke the user callback */` |
|   85390 | 6405 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6406 | `		/* Release the copy of the key and the value */` |
|   85390 | 6407 | `		PH7_MemObjRelease(&sKey);` |
|   85390 | 6408 | `		PH7_MemObjRelease(&sValue);` |
|   85390 | 6409 | `		if( rc != PH7_OK ){` |
|       - | 6410 | `			/* Callback request an operation abort */` |
|     ! 0 | 6411 | `			return SXERR_ABORT;` |
|       - | 6412 | `		}` |
|       - | 6413 | `		/* Point to the next entry */` |
|   85390 | 6414 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   85390 | 6415 | `		n--;` |
|       2 | 6416 | `	}` |
|       - | 6417 | `	/* All done */` |
|   19998 | 6418 | `	return SXRET_OK;` |
|   10000 | 6419 |  |
|       - | 6420 |  |
