# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2595/3115 lines (83.31%)

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
| 2753608 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2753610 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  218454 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  218456 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  218456 |   29 | `	sxu32 nH = 5381;` |
|  218456 |   30 | `	zEnd = &zIn[nLen];` |
|  251626 |   31 | `	for(;;){` |
|  503254 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  452700 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  409734 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  331154 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  218456 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecurisve is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * If the nesting limit is reached,this function abort immediately.` |
|       - |   43 | ` */` |
|     750 |   44 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)` |
|       2 |   45 |  |
|     752 |   46 | `	sxi64 iCount = 0;` |
|     752 |   47 | `	if( !bRecursive ){` |
|     476 |   48 | `		iCount = pMap->nEntry;` |
|     239 |   49 | `	}else{` |
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
|     748 |   79 | `	return iCount;` |
|     377 |   80 |  |
|       - |   81 | `/*` |
|       - |   82 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   83 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   84 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   85 | ` */` |
| 2699722 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   87 |  |
|       - |   88 | `	ph7_hashmap_node *pNode;` |
|       - |   89 | `	/* Allocate a new node */` |
| 2699724 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2699724 |   91 | `	if( pNode == 0 ){` |
|     ! 0 |   92 | `		return 0;` |
|       - |   93 | `	}` |
|       - |   94 | `	/* Zero the stucture */` |
| 2699724 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |   96 | `	/* Fill in the structure */` |
| 2699724 |   97 | `	pNode->pMap  = &(*pMap);` |
| 2699724 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2699724 |   99 | `	pNode->nHash = nHash;` |
| 2699724 |  100 | `	pNode->xKey.iKey = iKey;` |
| 2699724 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 2699724 |  102 | `	return pNode;` |
| 1349863 |  103 |  |
|       - |  104 | `/*` |
|       - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  108 | ` */` |
|   76078 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  110 |  |
|       - |  111 | `	ph7_hashmap_node *pNode;` |
|       - |  112 | `	/* Allocate a new node */` |
|   76080 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   76080 |  114 | `	if( pNode == 0 ){` |
|     ! 0 |  115 | `		return 0;` |
|       - |  116 | `	}` |
|       - |  117 | `	/* Zero the stucture */` |
|   76080 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  119 | `	/* Fill in the structure */` |
|   76080 |  120 | `	pNode->pMap  = &(*pMap);` |
|   76080 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   76080 |  122 | `	pNode->nHash = nHash;` |
|   76080 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   76080 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   76080 |  125 | `	pNode->nValIdx = nValIdx;` |
|   76080 |  126 | `	return pNode;` |
|   38041 |  127 |  |
|       - |  128 | `/*` |
|       - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  130 | ` */` |
| 2775800 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  132 |  |
|       - |  133 | `	/* Link */` |
| 2775802 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2567494 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2567494 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1283746 |  137 | `	}` |
| 2775802 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  139 | `	/* Link to the map list */` |
| 2775802 |  140 | `	if( pMap->pFirst == 0 ){` |
|   34704 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  142 | `		/* Point to the first inserted node */` |
|   34704 |  143 | `		pMap->pCur = pNode;` |
|   17353 |  144 | `	}else{` |
| 2741100 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  146 | `	}` |
| 2775802 |  147 | `	++pMap->nEntry;` |
| 2775802 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Unlink a node from the hashmap.` |
|       - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  152 | ` */` |
|    5324 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  154 |  |
|    5326 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5326 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  157 | `	/* Unlink from the corresponding bucket */` |
|    5326 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|    4902 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2452 |  160 | `	}else{` |
|     425 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  162 | `	}` |
|    5326 |  163 | `	if( pNode->pNextCollide ){` |
|    4121 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2060 |  165 | `	}` |
|    5326 |  166 | `	if( pMap->pFirst == pNode ){` |
|      58 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|      28 |  168 | `	}` |
|    5326 |  169 | `	if( pMap->pCur == pNode ){` |
|       - |  170 | `		/* Advance the node cursor */` |
|      60 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      29 |  172 | `	}` |
|       - |  173 | `	/* Unlink from the map list */` |
|    5326 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5326 |  175 | `	if( bRestore ){` |
|       - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|      30 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  178 | `		/* Restore to the freelist */` |
|      30 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|      30 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      14 |  181 | `		}` |
|      14 |  182 | `	}` |
|    5326 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5277 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2638 |  185 | `	}` |
|    5326 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5326 |  187 | `	pMap->nEntry--;` |
|    5326 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  189 | `		/* Free the hash-bucket */` |
|      26 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      26 |  191 | `		pMap->apBucket = 0;` |
|      26 |  192 | `		pMap->nSize = 0;` |
|      26 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      12 |  194 | `	}` |
|    5326 |  195 |  |
|       - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  197 | `/*` |
|       - |  198 | ` * Grow the hash-table and rehash all entries.` |
|       - |  199 | ` */` |
| 2775800 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  201 |  |
| 2775802 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   38200 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   38200 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  206 | `		sxu32 nBucket;` |
|       - |  207 | `		sxu32 n;` |
|   38200 |  208 | `		if( nNew < 1 ){` |
|   34704 |  209 | `			nNew = 16;` |
|   17351 |  210 | `		}` |
|       - |  211 | `		/* Allocate a new bucket */` |
|   38200 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   38200 |  213 | `		if( apNew == 0 ){` |
|     ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|       - |  216 | `			}` |
|       - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  218 | `			return SXRET_OK;` |
|       - |  219 | `		}` |
|       - |  220 | `		/* Zero the table */` |
|   38200 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  222 | `		/* Reflect the change */` |
|   38200 |  223 | `		pMap->apBucket = apNew;` |
|   38200 |  224 | `		pMap->nSize = nNew;` |
|   38200 |  225 | `		if( apOld == 0 ){` |
|       - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   34704 |  227 | `			return SXRET_OK;` |
|       - |  228 | `		}` |
|       - |  229 | `		/* Rehash old entries */` |
|    3498 |  230 | `		pEntry = pMap->pFirst;` |
|    3498 |  231 | `		n = 0;` |
| 1899476 |  232 | `		for( ;; ){` |
| 3798954 |  233 | `			if( n >= pMap->nEntry ){` |
|    3498 |  234 | `				break;` |
|       - |  235 | `			}` |
|       - |  236 | `			/* Clear the old collision link */` |
| 3795458 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  238 | `			/* Link to the new bucket */` |
| 3795458 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3795458 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3373476 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3373476 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1686737 |  243 | `			}` |
| 3795458 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  245 | `			/* Point to the next entry */` |
| 3795458 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3795458 |  247 | `			n++;` |
|       2 |  248 | `		}` |
|       - |  249 | `		/* Free the old table */` |
|    3498 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1748 |  251 | `	}` |
| 2741100 |  252 | `	return SXRET_OK;` |
| 1387902 |  253 |  |
|       - |  254 | `/*` |
|       - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  256 | ` * hashmap.` |
|       - |  257 | ` */` |
| 2699722 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  259 |  |
|       - |  260 | `	ph7_hashmap_node *pNode;` |
|       - |  261 | `	sxu32 nIdx;` |
|       - |  262 | `	sxu32 nHash;` |
|       - |  263 | `	sxi32 rc;` |
| 2699724 |  264 | `	if( !isForeign ){` |
|       - |  265 | `		ph7_value *pObj;` |
|       - |  266 | `		/* Reserve a ph7_value for the value */` |
| 2699700 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2699700 |  268 | `		if( pObj == 0 ){` |
|     ! 0 |  269 | `			return SXERR_MEM;` |
|       - |  270 | `		}` |
| 2699700 |  271 | `		if( pValue ){` |
|       - |  272 | `			/* Duplicate the value */` |
| 2699700 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 1349849 |  274 | `		}` |
| 2699700 |  275 | `		nIdx = pObj->nIdx;` |
| 1349851 |  276 | `	}else{` |
|      25 |  277 | `		nIdx = nRefIdx;` |
|       - |  278 | `	}` |
|       - |  279 | `	/* Hash the key */` |
| 2699724 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  281 | `	/* Allocate a new int node */` |
| 2699724 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2699724 |  283 | `	if( pNode == 0 ){` |
|     ! 0 |  284 | `		return SXERR_MEM;` |
|       - |  285 | `	}` |
| 2699724 |  286 | `	if( isForeign ){` |
|       - |  287 | `		/* Mark as a foregin entry */` |
|      25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      12 |  289 | `	}` |
|       - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2699724 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2699724 |  292 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  294 | `		return rc;` |
|       - |  295 | `	}` |
|       - |  296 | `	/* Perform the insertion */` |
| 2699724 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  298 | `	/* Install in the reference table */` |
| 2699724 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  300 | `	/* All done */` |
| 2699724 |  301 | `	return SXRET_OK;` |
| 1349863 |  302 |  |
|       - |  303 | `/*` |
|       - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  305 | ` * hashmap.` |
|       - |  306 | ` */` |
|   76078 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  308 |  |
|       - |  309 | `	ph7_hashmap_node *pNode;` |
|       - |  310 | `	sxu32 nHash;` |
|       - |  311 | `	sxu32 nIdx;` |
|       - |  312 | `	sxi32 rc;` |
|   76080 |  313 | `	if( !isForeign ){` |
|       - |  314 | `		ph7_value *pObj;` |
|       - |  315 | `		/* Reserve a ph7_value for the value */` |
|   57224 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   57224 |  317 | `		if( pObj == 0 ){` |
|     ! 0 |  318 | `			return SXERR_MEM;` |
|       - |  319 | `		}` |
|   57224 |  320 | `		if( pValue ){` |
|       - |  321 | `			/* Duplicate the value */` |
|   57224 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|   28611 |  323 | `		}` |
|   57224 |  324 | `		nIdx = pObj->nIdx;` |
|   28613 |  325 | `	}else{` |
|   18858 |  326 | `		nIdx = nRefIdx;` |
|       - |  327 | `	}` |
|       - |  328 | `	/* Hash the key */` |
|   76080 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  330 | `	/* Allocate a new blob node */` |
|   76080 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   76080 |  332 | `	if( pNode == 0 ){` |
|     ! 0 |  333 | `		return SXERR_MEM;` |
|       - |  334 | `	}` |
|   76080 |  335 | `	if( isForeign ){` |
|       - |  336 | `		/* Mark as a foregin entry */` |
|   18858 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|    9428 |  338 | `	}` |
|       - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   76080 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   76080 |  341 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  343 | `		return rc;` |
|       - |  344 | `	}` |
|       - |  345 | `	/* Perform the insertion */` |
|   76080 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  347 | `	/* Install in the reference table */` |
|   76080 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  349 | `	/* All done */` |
|   76080 |  350 | `	return SXRET_OK;` |
|   38041 |  351 |  |
|       - |  352 | `/*` |
|       - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  356 | ` */` |
|   46580 |  357 | `static sxi32 HashmapLookupIntKey(` |
|       - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  359 | `	sxi64 iKey,                /* lookup key */` |
|       - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  361 | `	)` |
|       2 |  362 |  |
|       - |  363 | `	ph7_hashmap_node *pNode;` |
|       - |  364 | `	sxu32 nHash;` |
|   46582 |  365 | `	if( pMap->nEntry < 1 ){` |
|       - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|     353 |  367 | `		return SXERR_NOTFOUND;` |
|       - |  368 | `	}` |
|       - |  369 | `	/* Hash the key first */` |
|   46230 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  371 | `	/* Point to the appropriate bucket */` |
|   46230 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  373 | `	/* Perform the lookup */` |
|  411471 |  374 | `	for(;;){` |
|  822944 |  375 | `		if( pNode == 0 ){` |
|   45705 |  376 | `			break;` |
|       - |  377 | `		}` |
|  777500 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774220 |  379 | `			&& pNode->nHash == nHash` |
|  385865 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  381 | `				/* Node found */` |
|     526 |  382 | `				if( ppNode ){` |
|     518 |  383 | `					*ppNode = pNode;` |
|     258 |  384 | `				}` |
|     526 |  385 | `				return SXRET_OK;` |
|       - |  386 | `		}` |
|       - |  387 | `		/* Follow the collision link */` |
|  776715 |  388 | `		pNode = pNode->pNextCollide;` |
|       1 |  389 | `	}` |
|       - |  390 | `	/* No such entry */` |
|   45705 |  391 | `	return SXERR_NOTFOUND;` |
|   23292 |  392 |  |
|       - |  393 | `/*` |
|       - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  397 | ` */` |
|  150038 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  400 | `	const void *pKey,           /* Lookup key */` |
|       - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  403 | `	)` |
|       2 |  404 |  |
|       - |  405 | `	ph7_hashmap_node *pNode;` |
|       - |  406 | `	sxu32 nHash;` |
|  150040 |  407 | `	if( pMap->nEntry < 1 ){` |
|       - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|    7664 |  409 | `		return SXERR_NOTFOUND;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* Hash the key first */` |
|  142378 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  413 | `	/* Point to the appropriate bucket */` |
|  142378 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  415 | `	/* Perform the lookup */` |
|  143669 |  416 | `	for(;;){` |
|  287340 |  417 | `		if( pNode == 0 ){` |
|  107772 |  418 | `			break;` |
|       - |  419 | `		}` |
|  196871 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  178068 |  421 | `			&& pNode->nHash == nHash` |
|  105587 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   34608 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  424 | `				/* Node found */` |
|   34608 |  425 | `				if( ppNode ){` |
|   34596 |  426 | `					*ppNode = pNode;` |
|   17297 |  427 | `				}` |
|   34608 |  428 | `				return SXRET_OK;` |
|       - |  429 | `		}` |
|       - |  430 | `		/* Follow the collision link */` |
|  144964 |  431 | `		pNode = pNode->pNextCollide;` |
|       2 |  432 | `	}` |
|       - |  433 | `	/* No such entry */` |
|  107772 |  434 | `	return SXERR_NOTFOUND;` |
|   75021 |  435 |  |
|       - |  436 | `/*` |
|       - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  439 | ` */` |
|  150206 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  441 |  |
|  150208 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  150208 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  150208 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  445 | `		/* Octal not decimal number */` |
|       5 |  446 | `		return FALSE;` |
|       - |  447 | `	}` |
|  150204 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  449 | `		zIn++;` |
|     ! 0 |  450 | `	}` |
|   75434 |  451 | `	for(;;){` |
|  150870 |  452 | `		if( zIn >= zEnd ){` |
|     233 |  453 | `			return TRUE;` |
|       - |  454 | `		}` |
|  150638 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   74987 |  456 | `			break;` |
|       - |  457 | `		}` |
|     667 |  458 | `		zIn++;` |
|       1 |  459 | `	}` |
|       - |  460 | `	/* Key does not look like a decimal number */` |
|  149972 |  461 | `	return FALSE;` |
|   75105 |  462 |  |
|       - |  463 | `/*` |
|       - |  464 | ` * Check if a given key exists in the given hashmap.` |
|       - |  465 | ` * Write a pointer to the target node on success.` |
|       - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  467 | ` */` |
|   74410 |  468 | `static sxi32 HashmapLookup(` |
|       - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  472 | `	)` |
|       2 |  473 |  |
|   74412 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  475 | `	sxi32 rc;` |
|   74412 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   73966 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  478 | `			/* Force a string cast */` |
|     ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  480 | `		}` |
|   73966 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  482 | `			/* Perform a blob lookup */` |
|   73950 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   73950 |  484 | `			goto result;` |
|       - |  485 | `		}` |
|       8 |  486 | `	}` |
|       - |  487 | `	/* Perform an int lookup */` |
|     464 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  489 | `		/* Force an integer cast */` |
|      27 |  490 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  491 | `	}` |
|       - |  492 | `	/* Perform an int lookup */` |
|     464 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   37205 |  494 | `result:` |
|   74412 |  495 | `	if( rc == SXRET_OK ){` |
|       - |  496 | `		/* Node found */` |
|   34994 |  497 | `		if( ppNode ){` |
|   34970 |  498 | `			*ppNode = pNode;` |
|   17484 |  499 | `		}` |
|   34994 |  500 | `		return SXRET_OK;` |
|       - |  501 | `	}` |
|       - |  502 | `	/* No such entry */` |
|   39420 |  503 | `	return SXERR_NOTFOUND;` |
|   37207 |  504 |  |
|       - |  505 | `/*` |
|       - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  507 | ` * hashmap.` |
|       - |  508 | ` * If a node with the given key already exists in the database` |
|       - |  509 | ` * then this function overwrite the old value.` |
|       - |  510 | ` */` |
| 2756776 |  511 | `static sxi32 HashmapInsert(` |
|       - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  514 | `	ph7_value *pVal    /* Node value */` |
|       - |  515 | `	)` |
|       2 |  516 |  |
| 2756778 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 2756778 |  518 | `	sxi32 rc = SXRET_OK;` |
| 2756778 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   57420 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  521 | `			/* Force a string cast */` |
|       3 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  523 | `		}` |
|   57420 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  526 | `				/* Automatic index assign */` |
|      34 |  527 | `				pKey = 0;` |
|      16 |  528 | `			}` |
|     256 |  529 | `			goto IntKey;` |
|       - |  530 | `		}` |
|   85748 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   28582 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|   57144 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  547 | `			/* Forbidden */` |
|       3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  549 | `			return SXRET_OK;` |
|       - |  550 | `		}` |
|       - |  551 | `		/* Perform a blob-key insertion */` |
|   57142 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   57142 |  553 | `		return rc;` |
|       - |  554 | `	}` |
| 1349679 |  555 | `IntKey:` |
| 2699614 |  556 | `	if( pKey ){` |
|   23167 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  558 | `			/* Force an integer cast */` |
|     251 |  559 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  560 | `		}` |
|   23167 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   23131 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  576 | `			/* Forbidden */` |
|       3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  578 | `			return SXRET_OK;` |
|       - |  579 | `		}` |
|       - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|   23129 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23129 |  582 | `		if( rc == SXRET_OK ){` |
|   23129 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  584 | `				/* Increment the automatic index */` |
|   22901 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  586 | `				/* Make sure the automatic index is not reserved */` |
|   22901 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  588 | `					pMap->iNextIdx++;` |
|     ! 0 |  589 | `				}` |
|   11450 |  590 | `			}` |
|   11564 |  591 | `		}` |
|   11565 |  592 | `	}else{` |
| 2676448 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  594 | `			/* Forbidden */` |
|       3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  596 | `			return SXRET_OK;` |
|       - |  597 | `		}` |
|       - |  598 | `		/* Assign an automatic index */` |
| 2676446 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2676446 |  600 | `		if( rc == SXRET_OK ){` |
| 2676446 |  601 | `			++pMap->iNextIdx;` |
| 1338222 |  602 | `		}` |
|       - |  603 | `	}` |
|       - |  604 | `	/* Insertion result */` |
| 2699574 |  605 | `	return rc;` |
| 1378390 |  606 |  |
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
|   18886 |  634 | `static sxi32 HashmapInsertByRef(` |
|       - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  638 | `	)` |
|       2 |  639 |  |
|   18888 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|   18888 |  641 | `	sxi32 rc = SXRET_OK;` |
|   18888 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   18864 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  644 | `			/* Force a string cast */` |
|     ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  646 | `		}` |
|   18864 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  649 | `				/* Automatic index assign */` |
|     ! 0 |  650 | `				pKey = 0;` |
|     ! 0 |  651 | `			}` |
|     ! 0 |  652 | `			goto IntKey;` |
|       - |  653 | `		}` |
|   28295 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    9431 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  656 | `				/* Overwrite */` |
|       7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|       - |  659 | `				/* Install in the reference table */` |
|       7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  661 | `				return SXRET_OK;` |
|       - |  662 | `		}` |
|       - |  663 | `		/* Perform a blob-key insertion */` |
|   18858 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   18858 |  665 | `		return rc;` |
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
|    9445 |  702 |  |
|       - |  703 | `/*` |
|       - |  704 | ` * Extract node value.` |
|       - |  705 | ` */` |
|  804858 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  707 |  |
|       - |  708 | `	/* Point to the desired object */` |
|       - |  709 | `	ph7_value *pObj;` |
|  804860 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  804860 |  711 | `	return pObj;` |
|       2 |  712 |  |
|       - |  713 | `/*` |
|       - |  714 | ` * Insert a node in the given hashmap.` |
|       - |  715 | ` * If a node with the given key already exists in the database` |
|       - |  716 | ` * then this function overwrite the old value.` |
|       - |  717 | ` */` |
|     268 |  718 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       1 |  719 |  |
|       - |  720 | `	ph7_value *pObj;` |
|       - |  721 | `	sxi32 rc;` |
|       - |  722 | `	/* Extract the node value */` |
|     269 |  723 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     269 |  724 | `	if( pObj == 0 ){` |
|     ! 0 |  725 | `		return SXERR_EMPTY;` |
|       - |  726 | `	}` |
|       - |  727 | `	/* Preserve key */` |
|     269 |  728 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  729 | `		/* Int64 key */` |
|     157 |  730 | `		if( !bPreserve ){` |
|       - |  731 | `			/* Assign an automatic index */` |
|      55 |  732 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      28 |  733 | `		}else{` |
|     103 |  734 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  735 | `		}` |
|      79 |  736 | `	}else{` |
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
|     269 |  747 | `	return rc;` |
|     135 |  748 |  |
|       - |  749 | `/*` |
|       - |  750 | ` * Compare two node values.` |
|       - |  751 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  752 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  753 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  754 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  755 | ` * documenation.` |
|       - |  756 | ` */` |
|   34962 |  757 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  758 |  |
|       - |  759 | `	ph7_value sObj1,sObj2;` |
|       - |  760 | `	sxi32 rc;` |
|   34964 |  761 | `	if( pLeft == pRight ){` |
|       - |  762 | `		/*` |
|       - |  763 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  764 | `		 * below for more information on this sceanario.` |
|       - |  765 | `		 */` |
|     ! 0 |  766 | `		return 0;` |
|       - |  767 | `	}` |
|       - |  768 | `	/* Do the comparison */` |
|   34964 |  769 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   34964 |  770 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   34964 |  771 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   34964 |  772 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   34964 |  773 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   34964 |  774 | `	PH7_MemObjRelease(&sObj1);` |
|   34964 |  775 | `	PH7_MemObjRelease(&sObj2);` |
|   34964 |  776 | `	return rc;` |
|   17525 |  777 |  |
|       - |  778 | `/*` |
|       - |  779 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  780 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  781 | ` */` |
|    7658 |  782 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  783 |  |
|    7660 |  784 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  785 | `	sxu32 nBucket;` |
|       - |  786 | `	/* Remove old collision links */` |
|    7660 |  787 | `	if( pEntry->pPrevCollide ){` |
|    6129 |  788 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    3060 |  789 | `	}else{` |
|    1533 |  790 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  791 | `	}` |
|    7660 |  792 | `	if( pEntry->pNextCollide ){` |
|     631 |  793 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     314 |  794 | `	}` |
|    7660 |  795 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  796 | `	/* Compute the new hash */` |
|    7660 |  797 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    7660 |  798 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    7660 |  799 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  800 | `	/* Link to the new bucket */` |
|    7660 |  801 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7660 |  802 | `	if( pMap->apBucket[nBucket] ){` |
|    6297 |  803 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3147 |  804 | `	}` |
|    7660 |  805 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7660 |  806 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  807 | `	/* Increment the automatic index */` |
|    7660 |  808 | `	pMap->iNextIdx++;` |
|    7660 |  809 |  |
|       - |  810 | `/*` |
|       - |  811 | ` * Perform a linear search on a given hashmap.` |
|       - |  812 | ` * Write a pointer to the target node on success.` |
|       - |  813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  814 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  815 | ` * for more information.` |
|       - |  816 | ` */` |
|   19490 |  817 | `static int HashmapFindValue(` |
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
|   19492 |  830 | `	pEntry = pMap->pFirst;` |
|   19492 |  831 | `	n = pMap->nEntry;` |
|   19492 |  832 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   19492 |  833 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   46655 |  834 | `	for(;;){` |
|   93312 |  835 | `		if( n < 1 ){` |
|      69 |  836 | `			break;` |
|       - |  837 | `		}` |
|       - |  838 | `		/* Extract node value */` |
|   93244 |  839 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|   93244 |  840 | `		if( pVal ){` |
|   93244 |  841 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|   93244 |  853 | `				PH7_MemObjLoad(pVal,&sVal);` |
|   93244 |  854 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|   93244 |  855 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|   93244 |  856 | `				PH7_MemObjRelease(&sVal);` |
|   93244 |  857 | `				PH7_MemObjRelease(&sNeedle);` |
|   93244 |  858 | `				if( rc == 0 ){` |
|   19424 |  859 | `					if( ppNode ){` |
|      23 |  860 | `						*ppNode = pEntry;` |
|      11 |  861 | `					}` |
|       - |  862 | `					/* Match found*/` |
|   19424 |  863 | `					return SXRET_OK;` |
|       - |  864 | `				}` |
|       - |  865 | `			}` |
|   36910 |  866 | `		}` |
|       - |  867 | `		/* Point to the next entry */` |
|   73822 |  868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   73822 |  869 | `		n--;` |
|       2 |  870 | `	}` |
|       - |  871 | `	/* No such entry */` |
|      69 |  872 | `	return SXERR_NOTFOUND;` |
|    9747 |  873 |  |
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
|  375926 | 1047 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1048 | `	ph7_hashmap *pDest,` |
|       - | 1049 | `	ph7_hashmap_node *pEntry,` |
|       - | 1050 | `	ph7_value *pVal,` |
|       - | 1051 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1052 | `	)` |
|       2 | 1053 |  |
|  375928 | 1054 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1055 | `	ph7_value sKey;` |
|       - | 1056 | `	sxi32 rc;` |
|       - | 1057 |  |
|  375928 | 1058 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1059 | `		/* Blob key insertion */` |
|      19 | 1060 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      19 | 1061 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      19 | 1062 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      19 | 1063 | `		PH7_MemObjRelease(&sKey);` |
|      10 | 1064 | `	}else{` |
|       - | 1065 | `		/* Int key */` |
|  375910 | 1066 | `		if( iAction == 0 ){ /* Merge */` |
|  375882 | 1067 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  187970 | 1068 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|       5 | 1069 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       5 | 1070 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|       5 | 1071 | `			PH7_MemObjRelease(&sKey);` |
|       3 | 1072 | `		}else{ /* Dup */` |
|      26 | 1073 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1074 | `		}` |
|       - | 1075 | `	}` |
|  375928 | 1076 | `	return rc;` |
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
|    1626 | 1090 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1091 |  |
|       - | 1092 | `	ph7_hashmap_node *pEntry;` |
|       - | 1093 | `	ph7_value *pVal;` |
|       - | 1094 | `	sxi32 rc;` |
|       - | 1095 | `	sxu32 n;` |
|    1628 | 1096 | `	if( pSrc == pDest ){` |
|       - | 1097 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1098 | `		 * Unlike the zend engine.` |
|       - | 1099 | `		 */` |
|     ! 0 | 1100 | `		return SXRET_OK;` |
|       - | 1101 | `	}` |
|       - | 1102 | `	/* Point to the first inserted entry in the source */` |
|    1628 | 1103 | `	pEntry = pSrc->pFirst;` |
|       - | 1104 | `	/* Perform the merge */` |
|  377514 | 1105 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1106 | `		/* Extract the node value */` |
|  375888 | 1107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  375888 | 1108 | `		if( pVal ){` |
|       - | 1109 | `			/* Make a local copy of the value.` |
|       - | 1110 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1111 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1112 | `			 * to the old pool.` |
|       - | 1113 | `			 */` |
|  375888 | 1114 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  187945 | 1115 | `		}else{` |
|     ! 0 | 1116 | `			rc = SXRET_OK;` |
|       - | 1117 | `		}` |
|  375888 | 1118 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1119 | `			return rc;` |
|       - | 1120 | `		}` |
|       - | 1121 | `		/* Point to the next entry */` |
|  375888 | 1122 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  187945 | 1123 | `	}` |
|    1628 | 1124 | `	return SXRET_OK;` |
|     815 | 1125 |  |
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
|   50912 | 1297 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1298 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1299 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1300 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1301 | `	)` |
|       2 | 1302 |  |
|       - | 1303 | `	ph7_hashmap *pMap;` |
|       - | 1304 | `	/* Allocate a new instance */` |
|   50914 | 1305 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   50914 | 1306 | `	if( pMap == 0 ){` |
|     ! 0 | 1307 | `		return 0;` |
|       - | 1308 | `	}` |
|       - | 1309 | `	/* Zero the structure */` |
|   50914 | 1310 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1311 | `	/* Fill in the structure */` |
|   50914 | 1312 | `	pMap->pVm = &(*pVm);` |
|   50914 | 1313 | `	pMap->iRef = 1;` |
|       - | 1314 | `	/* Default hash functions */` |
|   50914 | 1315 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   50914 | 1316 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   50914 | 1317 | `	return pMap;` |
|   25458 | 1318 |  |
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
|    1372 | 1339 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    1374 | 1359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1374 | 1360 | `	if( pMap == 0 ){` |
|     ! 0 | 1361 | `		return SXERR_MEM;` |
|       - | 1362 | `	}` |
|    1374 | 1363 | `	pVm->pGlobal = pMap;` |
|       - | 1364 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1374 | 1365 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1374 | 1366 | `	if( pObj == 0 ){` |
|     ! 0 | 1367 | `		return SXERR_MEM;` |
|       - | 1368 | `	}` |
|    1374 | 1369 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1370 | `	/* Record object index */` |
|    1374 | 1371 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1372 | `	/* Install the special $GLOBALS array */` |
|    1374 | 1373 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1374 | 1374 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1375 | `		return rc;` |
|       - | 1376 | `	}` |
|       - | 1377 | `	/* Install superglobals now */` |
|   15094 | 1378 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1379 | `		ph7_value *pSuper;` |
|       - | 1380 | `		/* Request an empty array */` |
|   13722 | 1381 | `		pSuper = ph7_new_array(&(*pVm));` |
|   13722 | 1382 | `		if( pSuper == 0 ){` |
|     ! 0 | 1383 | `			return SXERR_MEM;` |
|       - | 1384 | `		}` |
|       - | 1385 | `		/* Install */` |
|   13722 | 1386 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   13722 | 1387 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1388 | `			return rc;` |
|       - | 1389 | `		}` |
|       - | 1390 | `		/* Release the value now it have been installed */` |
|   13722 | 1391 | `		ph7_release_value(&(*pVm),pSuper);` |
|    6862 | 1392 | `	}` |
|       - | 1393 | `	/* Set some $_SERVER entries */` |
|    1374 | 1394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1395 | `	/*` |
|       - | 1396 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1397 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1398 | `	 */` |
|    2742 | 1399 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1400 | `		"SCRIPT_FILENAME",` |
|     686 | 1401 | `		pFile ? pFile->zString : ":Memory:",` |
|    1368 | 1402 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1403 | `		);` |
|       - | 1404 | `	/* All done,all super-global are installed now */` |
|    1374 | 1405 | `	return SXRET_OK;` |
|     688 | 1406 |  |
|       - | 1407 | `/*` |
|       - | 1408 | ` * Release a hashmap.` |
|       - | 1409 | ` */` |
|   35762 | 1410 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1411 |  |
|       - | 1412 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   35764 | 1413 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1414 | `	sxu32 n;` |
|   35764 | 1415 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1416 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1417 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1418 | `		return SXRET_OK;` |
|       - | 1419 | `	}` |
|       - | 1420 | `	/* Start the release process */` |
|   35764 | 1421 | `	n = 0;` |
|   35764 | 1422 | `	pEntry = pMap->pFirst;` |
| 1393460 | 1423 | `	for(;;){` |
| 2786922 | 1424 | `		if( n >= pMap->nEntry ){` |
|   35764 | 1425 | `			break;` |
|       - | 1426 | `		}` |
| 2751160 | 1427 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1428 | `		/* Remove the reference from the foreign table */` |
| 2751160 | 1429 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2751160 | 1430 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1431 | `			/* Restore the ph7_value to the free list */` |
| 2751152 | 1432 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1375575 | 1433 | `		}` |
|       - | 1434 | `		/* Release the node */` |
| 2751160 | 1435 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   55216 | 1436 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   27607 | 1437 | `		}` |
| 2751160 | 1438 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1439 | `		/* Point to the next entry */` |
| 2751160 | 1440 | `		pEntry = pNext;` |
| 2751160 | 1441 | `		n++;` |
|       2 | 1442 | `	}` |
|   35764 | 1443 | `	if( pMap->nEntry > 0 ){` |
|       - | 1444 | `		/* Release the hash bucket */` |
|   31874 | 1445 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   15936 | 1446 | `	}` |
|   35764 | 1447 | `	if( FreeDS ){` |
|       - | 1448 | `		/* Free the whole instance */` |
|   35762 | 1449 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   17882 | 1450 | `	}else{` |
|       - | 1451 | `		/* Keep the instance but reset it's fields */` |
|       3 | 1452 | `		pMap->apBucket = 0;` |
|       3 | 1453 | `		pMap->iNextIdx = 0;` |
|       3 | 1454 | `		pMap->nEntry = pMap->nSize = 0;` |
|       3 | 1455 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1456 | `	}` |
|   35764 | 1457 | `	return SXRET_OK;` |
|   17883 | 1458 |  |
|       - | 1459 | `/*` |
|       - | 1460 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1461 | ` * If the count reaches zero which mean no more variables` |
|       - | 1462 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1463 | ` */` |
|  416722 | 1464 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1465 |  |
|  416724 | 1466 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1467 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  416724 | 1468 | `	pMap->iRef--;` |
|  416724 | 1469 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   35762 | 1470 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   17880 | 1471 | `	}` |
|  416724 | 1472 |  |
|       - | 1473 | `/*` |
|       - | 1474 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1475 | ` * Write a pointer to the target node on success.` |
|       - | 1476 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1477 | ` */` |
|   74418 | 1478 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1479 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1480 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1481 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1482 | `	)` |
|       2 | 1483 |  |
|       - | 1484 | `	sxi32 rc;` |
|   74420 | 1485 | `	if( pMap->nEntry < 1 ){` |
|       - | 1486 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1487 | `		 */` |
|       9 | 1488 | `		return SXERR_NOTFOUND;` |
|       - | 1489 | `	}` |
|   74412 | 1490 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   74412 | 1491 | `	return rc;` |
|   37211 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1495 | ` * hashmap.` |
|       - | 1496 | ` * If a node with the given key already exists in the database` |
|       - | 1497 | ` * then this function overwrite the old value.` |
|       - | 1498 | ` */` |
| 2380810 | 1499 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1500 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1501 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1502 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1503 | `	)` |
|       2 | 1504 |  |
|       - | 1505 | `	sxi32 rc;` |
| 2380812 | 1506 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1507 | `		/*` |
|       - | 1508 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1509 | `		 */` |
|     ! 0 | 1510 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1511 | `		return SXRET_OK;` |
|       - | 1512 | `	}` |
| 2380812 | 1513 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2380812 | 1514 | `	return rc;` |
| 1190407 | 1515 |  |
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
|   18886 | 1543 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1544 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1545 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1546 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1547 | `	)` |
|       2 | 1548 |  |
|       - | 1549 | `	sxi32 rc;` |
|   18888 | 1550 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1551 | `		/*` |
|       - | 1552 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1553 | `		 */` |
|     ! 0 | 1554 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1555 | `		return SXRET_OK;` |
|       - | 1556 | `	}` |
|   18888 | 1557 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   18888 | 1558 | `	return rc;` |
|    9445 | 1559 |  |
|       - | 1560 | `/*` |
|       - | 1561 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1562 | ` */` |
|   15956 | 1563 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1564 |  |
|       - | 1565 | `	/* Reset the loop cursor */` |
|   15958 | 1566 | `	pMap->pCur = pMap->pFirst;` |
|   15958 | 1567 |  |
|       - | 1568 | `/*` |
|       - | 1569 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1570 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1571 | ` * return NULL.` |
|       - | 1572 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1573 | ` */` |
|  131004 | 1574 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1575 |  |
|  131006 | 1576 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  131006 | 1577 | `	if( pCur == 0 ){` |
|       - | 1578 | `		/* End of the list,return null */` |
|    7982 | 1579 | `		return 0;` |
|       - | 1580 | `	}` |
|       - | 1581 | `	/* Advance the node cursor */` |
|  123026 | 1582 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  123026 | 1583 | `	return pCur;` |
|   65504 | 1584 |  |
|       - | 1585 | `/*` |
|       - | 1586 | ` * Extract a node value.` |
|       - | 1587 | ` */` |
|  312220 | 1588 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1589 |  |
|  312222 | 1590 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  312222 | 1591 | `	if( pEntry ){` |
|  312222 | 1592 | `		if( bStore ){` |
|  123080 | 1593 | `			PH7_MemObjStore(pEntry,pValue);` |
|   61541 | 1594 | `		}else{` |
|  189144 | 1595 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1596 | `		}` |
|  156196 | 1597 | `	}else{` |
|     ! 0 | 1598 | `		PH7_MemObjRelease(pValue);` |
|       - | 1599 | `	}` |
|  312222 | 1600 |  |
|       - | 1601 | `/*` |
|       - | 1602 | ` * Extract a node key.` |
|       - | 1603 | ` */` |
|   84594 | 1604 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1605 |  |
|       - | 1606 | `	/* Fill with the current key */` |
|   84596 | 1607 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   84452 | 1608 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|       9 | 1609 | `			SyBlobRelease(&pKey->sBlob);` |
|       4 | 1610 | `		}` |
|   84452 | 1611 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   84452 | 1612 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   42227 | 1613 | `	}else{` |
|     145 | 1614 | `		SyBlobReset(&pKey->sBlob);` |
|     145 | 1615 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     145 | 1616 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1617 | `	}` |
|   84596 | 1618 |  |
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
|   22496 | 1666 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1667 |  |
|       - | 1668 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1669 | `    /* Prevent compiler warning */` |
|   22498 | 1670 | `	result.pNext = result.pPrev = 0;` |
|   22498 | 1671 | `	pTail = &result;` |
|   57504 | 1672 | `	while( pA && pB ){` |
|   35008 | 1673 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|   22902 | 1674 | `			pTail->pPrev = pA;` |
|   22902 | 1675 | `			pA->pNext = pTail;` |
|   22902 | 1676 | `			pTail = pA;` |
|   22902 | 1677 | `			pA = pA->pPrev;` |
|   11459 | 1678 | `		}else{` |
|   12108 | 1679 | `			pTail->pPrev = pB;` |
|   12108 | 1680 | `			pB->pNext = pTail;` |
|   12108 | 1681 | `			pTail = pB;` |
|   12108 | 1682 | `			pB = pB->pPrev;` |
|       - | 1683 | `		}` |
|       2 | 1684 | `	}` |
|   22498 | 1685 | `	if( pA ){` |
|   16689 | 1686 | `		pTail->pPrev = pA;` |
|   16689 | 1687 | `		pA->pNext = pTail;` |
|   14165 | 1688 | `	}else if( pB ){` |
|    5667 | 1689 | `		pTail->pPrev = pB;` |
|    5667 | 1690 | `		pB->pNext = pTail;` |
|    2824 | 1691 | `	}else{` |
|     146 | 1692 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1693 | `	}` |
|   22498 | 1694 | `	return result.pPrev;` |
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
|     506 | 1708 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1709 |  |
|       - | 1710 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1711 | `	sxu32 i;` |
|     508 | 1712 | `	SyZero(a,sizeof(a));` |
|       - | 1713 | `	/* Point to the first inserted entry */` |
|     508 | 1714 | `	pIn = pMap->pFirst;` |
|    8170 | 1715 | `	while( pIn ){` |
|    7664 | 1716 | `		p = pIn;` |
|    7664 | 1717 | `		pIn = p->pPrev;` |
|    7664 | 1718 | `		p->pPrev = 0;` |
|   14474 | 1719 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   14474 | 1720 | `			if( a[i]==0 ){` |
|    7664 | 1721 | `				a[i] = p;` |
|    7664 | 1722 | `				break;` |
|     ! 0 | 1723 | `			}else{` |
|    6812 | 1724 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    6812 | 1725 | `				a[i] = 0;` |
|       - | 1726 | `			}` |
|    3407 | 1727 | `		}` |
|    7664 | 1728 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1729 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1730 | `			 * But that is impossible.` |
|       - | 1731 | `			 */` |
|     ! 0 | 1732 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1733 | `		}` |
|       2 | 1734 | `	}` |
|     508 | 1735 | `	p = a[0];` |
|   16194 | 1736 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   15688 | 1737 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    7845 | 1738 | `	}` |
|     508 | 1739 | `	p->pNext = 0;` |
|       - | 1740 | `	/* Reflect the change */` |
|     508 | 1741 | `	pMap->pFirst = p;` |
|       - | 1742 | `	/* Reset the loop cursor */` |
|     508 | 1743 | `	pMap->pCur = pMap->pFirst;` |
|     508 | 1744 | `	return SXRET_OK;` |
|       2 | 1745 |  |
|       - | 1746 | `/*` |
|       - | 1747 | ` * Node comparison callback.` |
|       - | 1748 | ` * used-by: [sort(),asort(),...]` |
|       - | 1749 | ` */` |
|   34944 | 1750 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1751 |  |
|       - | 1752 | `	ph7_value sA,sB;` |
|       - | 1753 | `	sxi32 iFlags;` |
|       - | 1754 | `	int rc;` |
|   34946 | 1755 | `	if( pCmpData == 0 ){` |
|       - | 1756 | `		/* Perform a standard comparison */` |
|   34942 | 1757 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   34942 | 1758 | `		return rc;` |
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
|   17516 | 1784 |  |
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
|      14 | 1990 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1991 |  |
|       - | 1992 | `	sxu32 n;` |
|       6 | 1993 | `	SXUNUSED(pB); /* cc warning */` |
|       6 | 1994 | `	SXUNUSED(pCmpData);` |
|       - | 1995 | `	/* Grab a random number */` |
|      15 | 1996 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 1997 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 1998 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 1999 | `	 */` |
|      15 | 2000 | `	return n&1 ? 1 : -1;` |
|       1 | 2001 |  |
|       - | 2002 | `/*` |
|       - | 2003 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2004 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2005 | ` */` |
|     490 | 2006 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2007 |  |
|       - | 2008 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2009 | `	sxu32 i;` |
|       - | 2010 | `	/* Rehash all entries */` |
|     492 | 2011 | `	pLast = p = pMap->pFirst;` |
|     492 | 2012 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     492 | 2013 | `	i = 0;` |
|    4049 | 2014 | `	for( ;; ){` |
|    8100 | 2015 | `		if( i >= pMap->nEntry ){` |
|     492 | 2016 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     492 | 2017 | `			break;` |
|       - | 2018 | `		}` |
|    7610 | 2019 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2020 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2021 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2022 | `			/* Change key type */` |
|       5 | 2023 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2024 | `		}` |
|    7610 | 2025 | `		HashmapRehashIntNode(p);` |
|       - | 2026 | `		/* Point to the next entry */` |
|    7610 | 2027 | `		i++;` |
|    7610 | 2028 | `		pLast = p;` |
|    7610 | 2029 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2030 | `	}` |
|     492 | 2031 |  |
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
|     814 | 2053 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2054 |  |
|       - | 2055 | `	ph7_hashmap *pMap;` |
|       - | 2056 | `	/* Make sure we are dealing with a valid hashmap */` |
|     816 | 2057 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2060 | `		return PH7_OK;` |
|       - | 2061 | `	}` |
|       - | 2062 | `	/* Point to the internal representation of the input hashmap */` |
|     816 | 2063 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     816 | 2064 | `	if( pMap->nEntry > 1 ){` |
|     486 | 2065 | `		sxi32 iCmpFlags = 0;` |
|     486 | 2066 | `		if( nArg > 1 ){` |
|       - | 2067 | `			/* Extract comparison flags */` |
|       3 | 2068 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2069 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2070 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2071 | `			}` |
|       1 | 2072 | `		}` |
|       - | 2073 | `		/* Do the merge sort */` |
|     486 | 2074 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2075 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     486 | 2076 | `		HashmapSortRehash(pMap);` |
|     242 | 2077 | `	}` |
|       - | 2078 | `	/* All done,return TRUE */` |
|     816 | 2079 | `	ph7_result_bool(pCtx,1);` |
|     816 | 2080 | `	return PH7_OK;` |
|     409 | 2081 |  |
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
|     506 | 2498 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2499 |  |
|     508 | 2500 | `	int bRecursive = FALSE;` |
|       - | 2501 | `	sxi64 iCount;` |
|     508 | 2502 | `	if( nArg < 1 ){` |
|       - | 2503 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 2504 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 2505 | `		return PH7_OK;` |
|       - | 2506 | `	}` |
|     508 | 2507 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2508 | `		/* TICKET 1433-19: Handle objects */` |
|       3 | 2509 | `		int res = !ph7_value_is_null(apArg[0]);` |
|       3 | 2510 | `		ph7_result_int(pCtx,res);` |
|       3 | 2511 | `		return PH7_OK;` |
|       - | 2512 | `	}` |
|     506 | 2513 | `	if( nArg > 1 ){` |
|       - | 2514 | `		/* Recursive count? */` |
|      31 | 2515 | `		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;` |
|      15 | 2516 | `	}` |
|       - | 2517 | `	/* Count */` |
|     506 | 2518 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);` |
|     506 | 2519 | `	ph7_result_int64(pCtx,iCount);` |
|     506 | 2520 | `	return PH7_OK;` |
|     255 | 2521 |  |
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
|      22 | 3110 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3111 |  |
|       - | 3112 | `	ph7_hashmap_node *pNode;` |
|       - | 3113 | `	ph7_hashmap *pMap;` |
|       - | 3114 | `	ph7_value *pArray;` |
|       - | 3115 | `	ph7_value *pObj;` |
|       - | 3116 | `	sxu32 n;` |
|      23 | 3117 | `	if( nArg < 1 ){` |
|       - | 3118 | `		/* Missing arguments,return NULL */` |
|       3 | 3119 | `		ph7_result_null(pCtx);` |
|       3 | 3120 | `		return PH7_OK;` |
|       - | 3121 | `	}` |
|       - | 3122 | `	/* Make sure we are dealing with a valid hashmap */` |
|      21 | 3123 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3124 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3125 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3126 | `		return PH7_OK;` |
|       - | 3127 | `	}` |
|       - | 3128 | `	/* Point to the internal representation that describe the input hashmap */` |
|      21 | 3129 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3130 | `	/* Create a new array */` |
|      21 | 3131 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 3132 | `	if( pArray == 0 ){` |
|     ! 0 | 3133 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3134 | `		return PH7_OK;` |
|       - | 3135 | `	}` |
|       - | 3136 | `	/* Perform the requested operation */` |
|      21 | 3137 | `	pNode = pMap->pFirst;` |
|      75 | 3138 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      55 | 3139 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      55 | 3140 | `		if( pObj ){` |
|       - | 3141 | `			/* perform the insertion */` |
|      55 | 3142 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      27 | 3143 | `		}` |
|       - | 3144 | `		/* Point to the next entry */` |
|      55 | 3145 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      28 | 3146 | `	}` |
|       - | 3147 | `	/* return the new array */` |
|      21 | 3148 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 3149 | `	return PH7_OK;` |
|      12 | 3150 |  |
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
|     118 | 3164 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3165 |  |
|       - | 3166 | `	ph7_hashmap_node *pNode;` |
|       - | 3167 | `	ph7_hashmap *pMap;` |
|       - | 3168 | `	ph7_value *pArray;` |
|       - | 3169 | `	ph7_value sObj;` |
|       - | 3170 | `	ph7_value sVal;` |
|       - | 3171 | `	SyString sKey;` |
|       - | 3172 | `	int bStrict;` |
|       - | 3173 | `	sxi32 rc;` |
|       - | 3174 | `	sxu32 n;` |
|     120 | 3175 | `	if( nArg < 1 ){` |
|       - | 3176 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3177 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3178 | `			"ArgumentCountError",` |
|       - | 3179 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3180 | `			);` |
|       - | 3181 | `	}` |
|       - | 3182 | `	/* Make sure we are dealing with a valid hashmap */` |
|     118 | 3183 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3184 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3185 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3186 | `			"TypeError",` |
|       - | 3187 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3188 | `			ph7_type_name(apArg[0])` |
|       - | 3189 | `			);` |
|       - | 3190 | `	}` |
|       - | 3191 | `	/* Point to the internal representation of the input hashmap */` |
|     116 | 3192 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3193 | `	/* Create a new array */` |
|     116 | 3194 | `	pArray = ph7_context_new_array(pCtx);` |
|     116 | 3195 | `	if( pArray == 0 ){` |
|     ! 0 | 3196 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3197 | `		return PH7_OK;` |
|       - | 3198 | `	}` |
|     116 | 3199 | `	bStrict = FALSE;` |
|     116 | 3200 | `	if( nArg > 2 ){` |
|       - | 3201 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3202 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3203 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3204 | `				"TypeError",` |
|       - | 3205 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3206 | `				ph7_type_name(apArg[2])` |
|       - | 3207 | `				);` |
|       - | 3208 | `		}` |
|       5 | 3209 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3210 | `	}` |
|       - | 3211 | `	/* Perform the requested operation */` |
|     113 | 3212 | `	pNode = pMap->pFirst;` |
|     113 | 3213 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     549 | 3214 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     437 | 3215 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     109 | 3216 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      55 | 3217 | `		}else{` |
|     329 | 3218 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     329 | 3219 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3220 | `		}` |
|     437 | 3221 | `		rc = 0;` |
|     437 | 3222 | `		if( nArg > 1 ){` |
|      31 | 3223 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3224 | `			if( pValue ){` |
|      31 | 3225 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3226 | `				/* Filter key */` |
|      31 | 3227 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3228 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3229 | `			}` |
|      15 | 3230 | `		}` |
|     437 | 3231 | `		if( rc == 0 ){` |
|       - | 3232 | `			/* Perform the insertion */` |
|     419 | 3233 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     209 | 3234 | `		}` |
|     437 | 3235 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3236 | `		/* Point to the next entry */` |
|     437 | 3237 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     219 | 3238 | `	}` |
|       - | 3239 | `	/* return the new array */` |
|     113 | 3240 | `	ph7_result_value(pCtx,pArray);` |
|     113 | 3241 | `	return PH7_OK;` |
|      61 | 3242 |  |
|       - | 3243 | `/*` |
|       - | 3244 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3245 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3246 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3247 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3248 | ` * Parameters` |
|       - | 3249 | ` *  $arr1` |
|       - | 3250 | ` *   First array` |
|       - | 3251 | ` *  $arr2` |
|       - | 3252 | ` *   Second array` |
|       - | 3253 | ` * Return` |
|       - | 3254 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3255 | ` * Note` |
|       - | 3256 | ` *  This function is a symisc eXtension.` |
|       - | 3257 | ` */` |
|       4 | 3258 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3259 |  |
|       - | 3260 | `	ph7_hashmap *p1,*p2;` |
|       - | 3261 | `	int rc;` |
|       5 | 3262 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3263 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3264 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3265 | `		return PH7_OK;` |
|       - | 3266 | `	}` |
|       - | 3267 | `	/* Point to the hashmaps */` |
|       5 | 3268 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3269 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3270 | `	rc = (p1 == p2);` |
|       - | 3271 | `	/* Same instance? */` |
|       5 | 3272 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3273 | `	return PH7_OK;` |
|       3 | 3274 |  |
|       - | 3275 | `/*` |
|       - | 3276 | ` * array array_merge(array $array1,...)` |
|       - | 3277 | ` *  Merge one or more arrays.` |
|       - | 3278 | ` * Parameters` |
|       - | 3279 | ` *  $array1` |
|       - | 3280 | ` *    Initial array to merge.` |
|       - | 3281 | ` *  ...` |
|       - | 3282 | ` *   More array to merge.` |
|       - | 3283 | ` * Return` |
|       - | 3284 | ` *  The resulting array.` |
|       - | 3285 | ` */` |
|     810 | 3286 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3287 |  |
|       - | 3288 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3289 | `	ph7_value *pArray;` |
|       - | 3290 | `	int i;` |
|     812 | 3291 | `	if( nArg < 1 ){` |
|       - | 3292 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3293 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3294 | `		return PH7_OK;` |
|       - | 3295 | `	}` |
|       - | 3296 | `	/* Create a new array */` |
|     812 | 3297 | `	pArray = ph7_context_new_array(pCtx);` |
|     812 | 3298 | `	if( pArray == 0 ){` |
|     ! 0 | 3299 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3300 | `		return PH7_OK;` |
|       - | 3301 | `	}` |
|       - | 3302 | `	/* Point to the internal representation of the hashmap */` |
|     812 | 3303 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3304 | `	/* Start merging */` |
|    2432 | 3305 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3306 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1622 | 3307 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3308 | `			/* Insert scalar value */` |
|       5 | 3309 | `			ph7_array_add_elem(pArray,0,apArg[i]);` |
|       3 | 3310 | `		}else{` |
|    1618 | 3311 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3312 | `			/* Merge the two hashmaps */` |
|    1618 | 3313 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3314 | `		}` |
|     812 | 3315 | `	}` |
|       - | 3316 | `	/* Return the freshly created array */` |
|     812 | 3317 | `	ph7_result_value(pCtx,pArray);` |
|     812 | 3318 | `	return PH7_OK;` |
|     407 | 3319 |  |
|       - | 3320 | `/*` |
|       - | 3321 | ` * array array_copy(array $source)` |
|       - | 3322 | ` *  Make a blind copy of the target array.` |
|       - | 3323 | ` * Parameters` |
|       - | 3324 | ` *  $source` |
|       - | 3325 | ` *   Target array` |
|       - | 3326 | ` * Return` |
|       - | 3327 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3328 | ` * Note` |
|       - | 3329 | ` *  This function is a symisc eXtension.` |
|       - | 3330 | ` */` |
|       2 | 3331 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3332 |  |
|       - | 3333 | `	ph7_hashmap *pMap;` |
|       - | 3334 | `	ph7_value *pArray;` |
|       3 | 3335 | `	if( nArg < 1 ){` |
|       - | 3336 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3337 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3338 | `		return PH7_OK;` |
|       - | 3339 | `	}` |
|       - | 3340 | `	/* Create a new array */` |
|       3 | 3341 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3342 | `	if( pArray == 0 ){` |
|     ! 0 | 3343 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3344 | `		return PH7_OK;` |
|       - | 3345 | `	}` |
|       - | 3346 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3347 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3348 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3349 | `		/* Point to the internal representation of the source */` |
|       3 | 3350 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3351 | `		/* Perform the copy */` |
|       3 | 3352 | `		PH7_HashmapDup(pSrc,pMap);` |
|       2 | 3353 | `	}else{` |
|       - | 3354 | `		/* Simple insertion */` |
|     ! 0 | 3355 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3356 | `	}` |
|       - | 3357 | `	/* Return the duplicated array */` |
|       3 | 3358 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3359 | `	return PH7_OK;` |
|       2 | 3360 |  |
|       - | 3361 | `/*` |
|       - | 3362 | ` * bool array_erase(array $source)` |
|       - | 3363 | ` *  Remove all elements from a given array.` |
|       - | 3364 | ` * Parameters` |
|       - | 3365 | ` *  $source` |
|       - | 3366 | ` *   Target array` |
|       - | 3367 | ` * Return` |
|       - | 3368 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3369 | ` * Note` |
|       - | 3370 | ` *  This function is a symisc eXtension.` |
|       - | 3371 | ` */` |
|       2 | 3372 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3373 |  |
|       - | 3374 | `	ph7_hashmap *pMap;` |
|       3 | 3375 | `	if( nArg < 1 ){` |
|       - | 3376 | `		/* Missing arguments */` |
|     ! 0 | 3377 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3378 | `		return PH7_OK;` |
|       - | 3379 | `	}` |
|       - | 3380 | `	/* Point to the target hashmap */` |
|       3 | 3381 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3382 | `	/* Erase */` |
|       3 | 3383 | `	PH7_HashmapRelease(pMap,FALSE);` |
|       3 | 3384 | `	return PH7_OK;` |
|       2 | 3385 |  |
|       - | 3386 | `/*` |
|       - | 3387 | ` * array array_slice(array $array,int $offset [,int $length [, bool $preserve_keys = false ]])` |
|       - | 3388 | ` *  Extract a slice of the array.` |
|       - | 3389 | ` * Parameters` |
|       - | 3390 | ` *  $array` |
|       - | 3391 | ` *    The input array.` |
|       - | 3392 | ` * $offset` |
|       - | 3393 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3394 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3395 | ` * $length (optional)` |
|       - | 3396 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3397 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3398 | ` *   elements from the end of the array. If it is omitted, then the sequence will have` |
|       - | 3399 | ` *   everything from offset up until the end of the array.` |
|       - | 3400 | ` * $preserve_keys (optional)` |
|       - | 3401 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3402 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3403 | ` * Return` |
|       - | 3404 | ` *   The new slice.` |
|       - | 3405 | ` */` |
|       8 | 3406 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3407 |  |
|       - | 3408 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3409 | `	ph7_hashmap_node *pCur;` |
|       - | 3410 | `	ph7_value *pArray;` |
|       - | 3411 | `	int iLength,iOfft;` |
|       - | 3412 | `	int bPreserve;` |
|       - | 3413 | `	sxi32 rc;` |
|       9 | 3414 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3415 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3416 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3417 | `		return PH7_OK;` |
|       - | 3418 | `	}` |
|       - | 3419 | `	/* Point the internal representation of the target array */` |
|       9 | 3420 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 3421 | `	bPreserve = FALSE;` |
|       - | 3422 | `	/* Get the offset */` |
|       9 | 3423 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       9 | 3424 | `	if( iOfft < 0 ){` |
|       3 | 3425 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       1 | 3426 | `	}` |
|       9 | 3427 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3428 | `		/* Invalid offset,return the last entry */` |
|     ! 0 | 3429 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3430 | `	}` |
|       - | 3431 | `	/* Get the length */` |
|       9 | 3432 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       9 | 3433 | `	if( nArg > 2 ){` |
|       7 | 3434 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       7 | 3435 | `		if( iLength < 0 ){` |
|     ! 0 | 3436 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3437 | `		}` |
|       7 | 3438 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3439 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3440 | `		}` |
|       7 | 3441 | `		if( nArg > 3 && ph7_value_is_bool(apArg[3]) ){` |
|       3 | 3442 | `			bPreserve = ph7_value_to_bool(apArg[3]);` |
|       1 | 3443 | `		}` |
|       3 | 3444 | `	}` |
|       - | 3445 | `	/* Create a new array */` |
|       9 | 3446 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 3447 | `	if( pArray == 0 ){` |
|     ! 0 | 3448 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3449 | `		return PH7_OK;` |
|       - | 3450 | `	}` |
|       9 | 3451 | `	if( iLength < 1 ){` |
|       - | 3452 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3453 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3454 | `		return PH7_OK;` |
|       - | 3455 | `	}` |
|       - | 3456 | `	/* Point to the desired entry */` |
|       9 | 3457 | `	pCur = pSrc->pFirst;` |
|       9 | 3458 | `	for(;;){` |
|      19 | 3459 | `		if( iOfft < 1 ){` |
|       9 | 3460 | `			break;` |
|       - | 3461 | `		}` |
|       - | 3462 | `		/* Point to the next entry */` |
|      11 | 3463 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      11 | 3464 | `		iOfft--;` |
|       1 | 3465 | `	}` |
|       - | 3466 | `	/* Point to the internal representation of the hashmap */` |
|       9 | 3467 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      12 | 3468 | `	for(;;){` |
|      25 | 3469 | `		if( iLength < 1 ){` |
|       9 | 3470 | `			break;` |
|       - | 3471 | `		}` |
|      17 | 3472 | `		rc = HashmapInsertNode(pMap,pCur,bPreserve);` |
|      17 | 3473 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3474 | `			break;` |
|       - | 3475 | `		}` |
|       - | 3476 | `		/* Point to the next entry */` |
|      17 | 3477 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      17 | 3478 | `		iLength--;` |
|       1 | 3479 | `	}` |
|       - | 3480 | `	/* Return the freshly created array */` |
|       9 | 3481 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 3482 | `	return PH7_OK;` |
|       5 | 3483 |  |
|       - | 3484 | `/*` |
|       - | 3485 | ` * array array_splice(array $array,int $offset [,int $length [,value $replacement ]])` |
|       - | 3486 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3487 | ` * Parameters` |
|       - | 3488 | ` *  $array` |
|       - | 3489 | ` *    The input array.` |
|       - | 3490 | ` * $offset` |
|       - | 3491 | ` *    If offset is positive then the start of removed portion is at that offset from` |
|       - | 3492 | ` *    the beginning of the input array. If offset is negative then it starts that far` |
|       - | 3493 | ` *    from the end of the input array.` |
|       - | 3494 | ` * $length (optional)` |
|       - | 3495 | ` *    If length is omitted, removes everything from offset to the end of the array.` |
|       - | 3496 | ` *    If length is specified and is positive, then that many elements will be removed.` |
|       - | 3497 | ` *    If length is specified and is negative then the end of the removed portion will` |
|       - | 3498 | ` *    be that many elements from the end of the array.` |
|       - | 3499 | ` * $replacement (optional)` |
|       - | 3500 | ` *  If replacement array is specified, then the removed elements are replaced` |
|       - | 3501 | ` *  with elements from this array.` |
|       - | 3502 | ` *  If offset and length are such that nothing is removed, then the elements` |
|       - | 3503 | ` *  from the replacement array are inserted in the place specified by the offset.` |
|       - | 3504 | ` *  Note that keys in replacement array are not preserved.` |
|       - | 3505 | ` *  If replacement is just one element it is not necessary to put array() around` |
|       - | 3506 | ` *  it, unless the element is an array itself, an object or NULL.` |
|       - | 3507 | ` * Return` |
|       - | 3508 | ` *   A new array consisting of the extracted elements.` |
|       - | 3509 | ` */` |
|       2 | 3510 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3511 |  |
|       - | 3512 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode;` |
|       - | 3513 | `	ph7_value *pArray,*pRvalue,*pOld;` |
|       - | 3514 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3515 | `	int iLength,iOfft;` |
|       - | 3516 | `	sxi32 rc;` |
|       3 | 3517 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3518 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3519 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3520 | `		return PH7_OK;` |
|       - | 3521 | `	}` |
|       - | 3522 | `	/* Point the internal representation of the target array */` |
|       3 | 3523 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3524 | `	/* Get the offset */` |
|       3 | 3525 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       3 | 3526 | `	if( iOfft < 0 ){` |
|     ! 0 | 3527 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|     ! 0 | 3528 | `	}` |
|       3 | 3529 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3530 | `		/* Invalid offset,remove the last entry */` |
|     ! 0 | 3531 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3532 | `	}` |
|       - | 3533 | `	/* Get the length */` |
|       3 | 3534 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       3 | 3535 | `	if( nArg > 2 ){` |
|       3 | 3536 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       3 | 3537 | `		if( iLength < 0 ){` |
|     ! 0 | 3538 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3539 | `		}` |
|       3 | 3540 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3541 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3542 | `		}` |
|       1 | 3543 | `	}` |
|       - | 3544 | `	/* Create a new array */` |
|       3 | 3545 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3546 | `	if( pArray == 0 ){` |
|     ! 0 | 3547 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3548 | `		return PH7_OK;` |
|       - | 3549 | `	}` |
|       3 | 3550 | `	if( iLength < 1 ){` |
|       - | 3551 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3552 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3553 | `		return PH7_OK;` |
|       - | 3554 | `	}` |
|       - | 3555 | `	/* Point to the desired entry */` |
|       3 | 3556 | `	pCur = pSrc->pFirst;` |
|       2 | 3557 | `	for(;;){` |
|       5 | 3558 | `		if( iOfft < 1 ){` |
|       3 | 3559 | `			break;` |
|       - | 3560 | `		}` |
|       - | 3561 | `		/* Point to the next entry */` |
|       3 | 3562 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       3 | 3563 | `		iOfft--;` |
|       1 | 3564 | `	}` |
|       3 | 3565 | `	pRep = 0;` |
|       3 | 3566 | `	if( nArg > 3 ){` |
|       3 | 3567 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3568 | `			/* Perform an array cast */` |
|     ! 0 | 3569 | `			PH7_MemObjToHashmap(apArg[3]);` |
|     ! 0 | 3570 | `			if(ph7_value_is_array(apArg[3])){` |
|     ! 0 | 3571 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|     ! 0 | 3572 | `			}` |
|     ! 0 | 3573 | `		}else{` |
|       3 | 3574 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3575 | `		}` |
|       3 | 3576 | `		if( pRep ){` |
|       - | 3577 | `			/* Reset the loop cursor */` |
|       3 | 3578 | `			pRep->pCur = pRep->pFirst;` |
|       1 | 3579 | `		}` |
|       1 | 3580 | `	}` |
|       - | 3581 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3582 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3583 | `	for(;;){` |
|       7 | 3584 | `		if( iLength < 1 ){` |
|       3 | 3585 | `			break;` |
|       - | 3586 | `		}` |
|       5 | 3587 | `		pPrev = pCur->pPrev;` |
|       5 | 3588 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|       5 | 3589 | `		if( pRep && (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|       - | 3590 | `			/* Extract node value */` |
|       5 | 3591 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|       - | 3592 | `			/* Replace the old node */` |
|       5 | 3593 | `			pOld = HashmapExtractNodeValue(pCur);` |
|       5 | 3594 | `			if( pRvalue && pOld ){` |
|       5 | 3595 | `				PH7_MemObjStore(pRvalue,pOld);` |
|       2 | 3596 | `			}` |
|       3 | 3597 | `		}else{` |
|       - | 3598 | `			/* Unlink the node from the source hashmap */` |
|     ! 0 | 3599 | `			PH7_HashmapUnlinkNode(pCur,TRUE);` |
|       - | 3600 | `		}` |
|       5 | 3601 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3602 | `			break;` |
|       - | 3603 | `		}` |
|       - | 3604 | `		/* Point to the next entry */` |
|       5 | 3605 | `		pCur = pPrev; /* Reverse link */` |
|       5 | 3606 | `		iLength--;` |
|       1 | 3607 | `	}` |
|       3 | 3608 | `	if( pRep ){` |
|       3 | 3609 | `		while((pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|     ! 0 | 3610 | `			HashmapInsertNode(pSrc,pRnode,FALSE);` |
|     ! 0 | 3611 | `		}` |
|       1 | 3612 | `	}` |
|       - | 3613 | `	/* Return the freshly created array */` |
|       3 | 3614 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3615 | `	return PH7_OK;` |
|       2 | 3616 |  |
|       - | 3617 | `/*` |
|       - | 3618 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3619 | ` *  Checks if a value exists in an array.` |
|       - | 3620 | ` * Parameters` |
|       - | 3621 | ` *  $needle` |
|       - | 3622 | ` *   The searched value.` |
|       - | 3623 | ` *   Note:` |
|       - | 3624 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3625 | ` * $haystack` |
|       - | 3626 | ` *  The target array.` |
|       - | 3627 | ` * $strict` |
|       - | 3628 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3629 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3630 | ` */` |
|   19352 | 3631 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3632 |  |
|       - | 3633 | `	ph7_value *pNeedle;` |
|       - | 3634 | `	int bStrict;` |
|       - | 3635 | `	int rc;` |
|   19354 | 3636 | `	if( nArg < 2 ){` |
|       - | 3637 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3638 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3639 | `		return PH7_OK;` |
|       - | 3640 | `	}` |
|   19354 | 3641 | `	pNeedle = apArg[0];` |
|   19354 | 3642 | `	bStrict = 0;` |
|   19354 | 3643 | `	if( nArg > 2 ){` |
|       5 | 3644 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3645 | `	}` |
|   19354 | 3646 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3647 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3648 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3649 | `		/* Set the comparison result */` |
|     ! 0 | 3650 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3651 | `		return PH7_OK;` |
|       - | 3652 | `	}` |
|       - | 3653 | `	/* Perform the lookup */` |
|   19354 | 3654 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3655 | `	/* Lookup result */` |
|   19354 | 3656 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   19354 | 3657 | `	return PH7_OK;` |
|    9678 | 3658 |  |
|       - | 3659 | `/*` |
|       - | 3660 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3661 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3662 | ` * Parameters` |
|       - | 3663 | ` * $needle` |
|       - | 3664 | ` *   The searched value.` |
|       - | 3665 | ` * $haystack` |
|       - | 3666 | ` *   The array.` |
|       - | 3667 | ` * $strict` |
|       - | 3668 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3669 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3670 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3671 | ` * Return` |
|       - | 3672 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3673 | ` */` |
|      28 | 3674 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3675 |  |
|       - | 3676 | `	ph7_hashmap_node *pEntry;` |
|       - | 3677 | `	ph7_value *pVal,sNeedle;` |
|       - | 3678 | `	ph7_hashmap *pMap;` |
|       - | 3679 | `	ph7_value sVal;` |
|       - | 3680 | `	int bStrict;` |
|       - | 3681 | `	sxu32 n;` |
|       - | 3682 | `	int rc;` |
|      30 | 3683 | `	if( nArg < 2 ){` |
|       - | 3684 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3685 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3686 | `			"ArgumentCountError",` |
|       - | 3687 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 3688 | `			nArg` |
|       - | 3689 | `			);` |
|       - | 3690 | `	}` |
|      26 | 3691 | `	bStrict = FALSE;` |
|      26 | 3692 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3693 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3694 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3695 | `			"TypeError",` |
|       - | 3696 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 3697 | `			ph7_type_name(apArg[1])` |
|       - | 3698 | `			);` |
|       - | 3699 | `	}` |
|      24 | 3700 | `	if( nArg > 2 ){` |
|       - | 3701 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 3702 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3703 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3704 | `				"TypeError",` |
|       - | 3705 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3706 | `				ph7_type_name(apArg[2])` |
|       - | 3707 | `				);` |
|       - | 3708 | `		}` |
|       9 | 3709 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 3710 | `	}` |
|       - | 3711 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 3712 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3713 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 3714 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 3715 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 3716 | `	pEntry = pMap->pFirst;` |
|      21 | 3717 | `	n = pMap->nEntry;` |
|      23 | 3718 | `	for(;;){` |
|      47 | 3719 | `		if( !n ){` |
|       9 | 3720 | `			break;` |
|       - | 3721 | `		}` |
|       - | 3722 | `		/* Extract node value */` |
|      39 | 3723 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 3724 | `		if( pVal ){` |
|       - | 3725 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3726 | `			 * can change their type.` |
|       - | 3727 | `			 */` |
|      39 | 3728 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 3729 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 3730 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 3731 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 3732 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 3733 | `			if( rc == 0 ){` |
|       - | 3734 | `				/* Match found,return key */` |
|      13 | 3735 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3736 | `					/* INT key */` |
|       7 | 3737 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 3738 | `				}else{` |
|       7 | 3739 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3740 | `					/* Blob key */` |
|       7 | 3741 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3742 | `				}` |
|      13 | 3743 | `				return PH7_OK;` |
|       - | 3744 | `			}` |
|      13 | 3745 | `		}` |
|       - | 3746 | `		/* Point to the next entry */` |
|      27 | 3747 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 3748 | `		n--;` |
|       1 | 3749 | `	}` |
|       - | 3750 | `	/* No such value,return FALSE */` |
|       9 | 3751 | `	ph7_result_bool(pCtx,0);` |
|       9 | 3752 | `	return PH7_OK;` |
|      16 | 3753 |  |
|       - | 3754 | `/*` |
|       - | 3755 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3756 | ` *  Computes the difference of arrays.` |
|       - | 3757 | ` * Parameters` |
|       - | 3758 | ` *  $array1` |
|       - | 3759 | ` *    The array to compare from` |
|       - | 3760 | ` *  $array2` |
|       - | 3761 | ` *    An array to compare against` |
|       - | 3762 | ` *  $...` |
|       - | 3763 | ` *   More arrays to compare against` |
|       - | 3764 | ` * Return` |
|       - | 3765 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3766 | ` *  are not present in any of the other arrays.` |
|       - | 3767 | ` */` |
|      22 | 3768 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3769 |  |
|       - | 3770 | `	ph7_hashmap_node *pEntry;` |
|       - | 3771 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3772 | `	ph7_value *pArray;` |
|       - | 3773 | `	ph7_value *pVal;` |
|       - | 3774 | `	sxi32 rc;` |
|       - | 3775 | `	sxu32 n;` |
|       - | 3776 | `	int i;` |
|       - | 3777 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3778 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3779 | `	 * debugging difficult. */` |
|      24 | 3780 | `	if( nArg < 1 ){` |
|       4 | 3781 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3782 | `			"ArgumentCountError",` |
|       - | 3783 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3784 | `			nArg` |
|       - | 3785 | `			);` |
|       - | 3786 | `	}` |
|      22 | 3787 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3788 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3789 | `			"TypeError",` |
|       - | 3790 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3791 | `			ph7_type_name(apArg[0])` |
|       - | 3792 | `			);` |
|       - | 3793 | `	}` |
|      36 | 3794 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3795 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3796 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3797 | `				"TypeError",` |
|       - | 3798 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3799 | `				i + 1,` |
|       2 | 3800 | `				ph7_type_name(apArg[i])` |
|       - | 3801 | `				);` |
|       - | 3802 | `		}` |
|       9 | 3803 | `	}` |
|      17 | 3804 | `	if( nArg == 1 ){` |
|       - | 3805 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3806 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3807 | `		return PH7_OK;` |
|       - | 3808 | `	}` |
|       - | 3809 | `	/* Create a new array */` |
|      15 | 3810 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3811 | `	if( pArray == 0 ){` |
|     ! 0 | 3812 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3813 | `		return PH7_OK;` |
|       - | 3814 | `	}` |
|       - | 3815 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 3816 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3817 | `	/* Perform the diff */` |
|      15 | 3818 | `	pEntry = pSrc->pFirst;` |
|      15 | 3819 | `	n = pSrc->nEntry;` |
|      27 | 3820 | `	for(;;){` |
|      55 | 3821 | `		if( n < 1 ){` |
|      15 | 3822 | `			break;` |
|       - | 3823 | `		}` |
|       - | 3824 | `		/* Extract the node value */` |
|      41 | 3825 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 3826 | `		if( pVal ){` |
|      69 | 3827 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3828 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 3829 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3830 | `				/* Perform the lookup */` |
|      45 | 3831 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 3832 | `				if( rc == SXRET_OK ){` |
|       - | 3833 | `					/* Value exist */` |
|      17 | 3834 | `					break;` |
|       - | 3835 | `				}` |
|      15 | 3836 | `			}` |
|      41 | 3837 | `			if( i >= nArg ){` |
|       - | 3838 | `				/* Perform the insertion */` |
|      25 | 3839 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 3840 | `			}` |
|      20 | 3841 | `		}` |
|       - | 3842 | `		/* Point to the next entry */` |
|      41 | 3843 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 3844 | `		n--;` |
|       1 | 3845 | `	}` |
|       - | 3846 | `	/* Return the freshly created array */` |
|      15 | 3847 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3848 | `	return PH7_OK;` |
|      13 | 3849 |  |
|       - | 3850 | `/*` |
|       - | 3851 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 3852 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 3853 | ` * Parameters` |
|       - | 3854 | ` *  $array1` |
|       - | 3855 | ` *    The array to compare from` |
|       - | 3856 | ` *  $array2` |
|       - | 3857 | ` *    An array to compare against` |
|       - | 3858 | ` *  $...` |
|       - | 3859 | ` *   More arrays to compare against.` |
|       - | 3860 | ` * $callback` |
|       - | 3861 | ` *  The callback comparison function.` |
|       - | 3862 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3863 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3864 | ` *  than the second.` |
|       - | 3865 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3866 | ` * Return` |
|       - | 3867 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3868 | ` *  are not present in any of the other arrays.` |
|       - | 3869 | ` */` |
|       2 | 3870 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3871 |  |
|       - | 3872 | `	ph7_hashmap_node *pEntry;` |
|       - | 3873 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3874 | `	ph7_value *pCallback;` |
|       - | 3875 | `	ph7_value *pArray;` |
|       - | 3876 | `	ph7_value *pVal;` |
|       - | 3877 | `	sxi32 rc;` |
|       - | 3878 | `	sxu32 n;` |
|       - | 3879 | `	int i;` |
|       3 | 3880 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3881 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3882 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3883 | `		return PH7_OK;` |
|       - | 3884 | `	}` |
|       - | 3885 | `	/* Point to the callback */` |
|       3 | 3886 | `	pCallback = apArg[nArg - 1];` |
|       3 | 3887 | `	if( nArg == 2 ){` |
|       - | 3888 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3889 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3890 | `		return PH7_OK;` |
|       - | 3891 | `	}` |
|       - | 3892 | `	/* Create a new array */` |
|       3 | 3893 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3894 | `	if( pArray == 0 ){` |
|     ! 0 | 3895 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3896 | `		return PH7_OK;` |
|       - | 3897 | `	}` |
|       - | 3898 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 3899 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3900 | `	/* Perform the diff */` |
|       3 | 3901 | `	pEntry = pSrc->pFirst;` |
|       3 | 3902 | `	n = pSrc->nEntry;` |
|       4 | 3903 | `	for(;;){` |
|       9 | 3904 | `		if( n < 1 ){` |
|       3 | 3905 | `			break;` |
|       - | 3906 | `		}` |
|       - | 3907 | `		/* Extract the node value */` |
|       7 | 3908 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 3909 | `		if( pVal ){` |
|      11 | 3910 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 3911 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3912 | `					/* ignore */` |
|     ! 0 | 3913 | `					continue;` |
|       - | 3914 | `				}` |
|       - | 3915 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 3916 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3917 | `				/* Perform the lookup */` |
|       7 | 3918 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 3919 | `				if( rc == SXRET_OK ){` |
|       - | 3920 | `					/* Value exist */` |
|       3 | 3921 | `					break;` |
|       - | 3922 | `				}` |
|       3 | 3923 | `			}` |
|       7 | 3924 | `			if( i >= (nArg - 1)){` |
|       - | 3925 | `				/* Perform the insertion */` |
|       5 | 3926 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 3927 | `			}` |
|       3 | 3928 | `		}` |
|       - | 3929 | `		/* Point to the next entry */` |
|       7 | 3930 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 3931 | `		n--;` |
|       1 | 3932 | `	}` |
|       - | 3933 | `	/* Return the freshly created array */` |
|       3 | 3934 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3935 | `	return PH7_OK;` |
|       2 | 3936 |  |
|       - | 3937 | `/*` |
|       - | 3938 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 3939 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 3940 | ` * Parameters` |
|       - | 3941 | ` *  $array1` |
|       - | 3942 | ` *    The array to compare from` |
|       - | 3943 | ` *  $array2` |
|       - | 3944 | ` *    An array to compare against` |
|       - | 3945 | ` *  $...` |
|       - | 3946 | ` *   More arrays to compare against` |
|       - | 3947 | ` * Return` |
|       - | 3948 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3949 | ` *  are not present in any of the other arrays.` |
|       - | 3950 | ` */` |
|      20 | 3951 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3952 |  |
|       - | 3953 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 3954 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3955 | `	ph7_value *pArray;` |
|       - | 3956 | `	ph7_value *pVal;` |
|       - | 3957 | `	sxi32 rc;` |
|       - | 3958 | `	sxu32 n;` |
|       - | 3959 | `	int i;` |
|       - | 3960 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 3961 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 3962 | `	 * accompanying integration tests to pass. */` |
|      22 | 3963 | `	if( nArg < 1 ){` |
|       4 | 3964 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3965 | `			"ArgumentCountError",` |
|       - | 3966 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 3967 | `			nArg` |
|       - | 3968 | `			);` |
|       - | 3969 | `	}` |
|      20 | 3970 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3971 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3972 | `			"TypeError",` |
|       - | 3973 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3974 | `			ph7_type_name(apArg[0])` |
|       - | 3975 | `			);` |
|       - | 3976 | `	}` |
|      32 | 3977 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3978 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 3979 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3980 | `				"TypeError",` |
|       - | 3981 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 3982 | `				i + 1,` |
|       4 | 3983 | `				ph7_type_name(apArg[i])` |
|       - | 3984 | `				);` |
|       - | 3985 | `		}` |
|       9 | 3986 | `	}` |
|      13 | 3987 | `	if( nArg == 1 ){` |
|       - | 3988 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3989 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3990 | `		return PH7_OK;` |
|       - | 3991 | `	}` |
|       - | 3992 | `	/* Create a new array */` |
|      11 | 3993 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 3994 | `	if( pArray == 0 ){` |
|     ! 0 | 3995 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3996 | `		return PH7_OK;` |
|       - | 3997 | `	}` |
|       - | 3998 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 3999 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4000 | `	/* Perform the diff */` |
|      11 | 4001 | `	pEntry = pSrc->pFirst;` |
|      11 | 4002 | `	n = pSrc->nEntry;` |
|      11 | 4003 | `	pN1 = pN2 = 0;` |
|      29 | 4004 | `	for(;;){` |
|       - | 4005 | `		int keep;` |
|      35 | 4006 | `		if( n < 1 ){` |
|      11 | 4007 | `			break;` |
|       - | 4008 | `		}` |
|       - | 4009 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4010 | `		keep = 1;` |
|      41 | 4011 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4012 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4013 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4014 | `			/* Perform a key lookup first */` |
|      29 | 4015 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4016 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4017 | `			}else{` |
|      17 | 4018 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4019 | `			}` |
|      29 | 4020 | `			if( rc != SXRET_OK ){` |
|       - | 4021 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4022 | `				continue;` |
|       - | 4023 | `			}` |
|       - | 4024 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4025 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4026 | `			if( pVal ){` |
|       - | 4027 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4028 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4029 | `				if( pVal2 ){` |
|      15 | 4030 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4031 | `					if( cmp == 0 ){` |
|       - | 4032 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4033 | `						keep = 0;` |
|      13 | 4034 | `						break;` |
|       - | 4035 | `					}` |
|       1 | 4036 | `				}` |
|       1 | 4037 | `			}` |
|       2 | 4038 | `		}` |
|      25 | 4039 | `		if( keep ){` |
|       - | 4040 | `			/* Perform the insertion */` |
|      13 | 4041 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4042 | `		}` |
|       - | 4043 | `		/* Point to the next entry */` |
|      25 | 4044 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4045 | `		n--;` |
|       1 | 4046 | `	}` |
|       - | 4047 | `	/* Return the freshly created array */` |
|      11 | 4048 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4049 | `	return PH7_OK;` |
|      12 | 4050 |  |
|       - | 4051 | `/*` |
|       - | 4052 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4053 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4054 | ` *  by a user supplied callback function.` |
|       - | 4055 | ` * Parameters` |
|       - | 4056 | ` *  $array1` |
|       - | 4057 | ` *    The array to compare from` |
|       - | 4058 | ` *  $array2` |
|       - | 4059 | ` *    An array to compare against` |
|       - | 4060 | ` *  $...` |
|       - | 4061 | ` *   More arrays to compare against.` |
|       - | 4062 | ` *  $key_compare_func` |
|       - | 4063 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4064 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4065 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4066 | ` * Return` |
|       - | 4067 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4068 | ` *  are not present in any of the other arrays.` |
|       - | 4069 | ` */` |
|      22 | 4070 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4071 |  |
|       - | 4072 | `	ph7_hashmap_node *pEntry;` |
|       - | 4073 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4074 | `	ph7_value *pCallback;` |
|       - | 4075 | `	ph7_value *pArray;` |
|       - | 4076 | `	sxi32 rc;` |
|       - | 4077 | `	sxu32 n;` |
|       - | 4078 | `	int i;` |
|       - | 4079 |  |
|       - | 4080 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4081 | `	if( nArg < 2 ){` |
|       4 | 4082 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4083 | `			"ArgumentCountError",` |
|       - | 4084 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4085 | `			nArg` |
|       - | 4086 | `			);` |
|       - | 4087 | `	}` |
|      22 | 4088 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4089 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4090 | `			"TypeError",` |
|       - | 4091 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4092 | `			ph7_type_name(apArg[0])` |
|       - | 4093 | `			);` |
|       - | 4094 | `	}` |
|       - | 4095 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4096 | `	 * expected to be a callback. */` |
|      32 | 4097 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4098 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4099 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4100 | `				"TypeError",` |
|       - | 4101 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4102 | `				i + 1,` |
|       2 | 4103 | `				ph7_type_name(apArg[i])` |
|       - | 4104 | `				);` |
|       - | 4105 | `		}` |
|       8 | 4106 | `	}` |
|       - | 4107 | `	/* Point to the callback value */` |
|      18 | 4108 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4109 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4110 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4111 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4112 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4113 | `		 * string given" which we also reproduce. */` |
|       7 | 4114 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4115 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4116 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4117 | `				"TypeError",` |
|       - | 4118 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4119 | `				nArg` |
|       - | 4120 | `				);` |
|       - | 4121 | `		}` |
|       5 | 4122 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4123 | `			/* neither array nor string */` |
|       7 | 4124 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4125 | `				"TypeError",` |
|       - | 4126 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4127 | `				nArg` |
|       - | 4128 | `				);` |
|       - | 4129 | `		}` |
|       - | 4130 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4131 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4132 | `			"TypeError",` |
|       - | 4133 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4134 | `			nArg,` |
|     ! 0 | 4135 | `			ph7_type_name(pCallback)` |
|       - | 4136 | `			);` |
|       - | 4137 | `	}` |
|      11 | 4138 | `	if( nArg == 2 ){` |
|       - | 4139 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4140 | `		 * input array. */` |
|       3 | 4141 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4142 | `		return PH7_OK;` |
|       - | 4143 | `	}` |
|       - | 4144 | `	/* Create a new array */` |
|       9 | 4145 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4146 | `	if( pArray == 0 ){` |
|     ! 0 | 4147 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4148 | `		return PH7_OK;` |
|       - | 4149 | `	}` |
|       - | 4150 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4151 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4152 | `	/* Perform the diff */` |
|       9 | 4153 | `	pEntry = pSrc->pFirst;` |
|       9 | 4154 | `	n = pSrc->nEntry;` |
|      20 | 4155 | `	for(;;){` |
|       - | 4156 | `		int keep;` |
|      25 | 4157 | `		if( n < 1 ){` |
|       9 | 4158 | `			break;` |
|       - | 4159 | `		}` |
|      17 | 4160 | `		keep = 1;` |
|      29 | 4161 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4162 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4163 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4164 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4165 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4166 | `			while( pIt ){` |
|       - | 4167 | `				/* build temporary key values for callback */` |
|       - | 4168 | `				ph7_value key1, key2, result;` |
|       - | 4169 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4170 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4171 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4172 | `				}else{` |
|       - | 4173 | `					SyString sStr;` |
|      31 | 4174 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4175 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4176 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4177 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4178 | `				}` |
|      31 | 4179 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4180 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4181 | `				}else{` |
|       - | 4182 | `					SyString sStr;` |
|      31 | 4183 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4184 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4185 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4186 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4187 | `				}` |
|      31 | 4188 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4189 | `				/* call user callback with (key1, key2) */` |
|       - | 4190 | `				{` |
|       - | 4191 | `					ph7_value *apK[2];` |
|      31 | 4192 | `					apK[0] = &key1;` |
|      31 | 4193 | `					apK[1] = &key2;` |
|      31 | 4194 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4195 | `				}` |
|      31 | 4196 | `				if( rc == SXRET_OK ){` |
|      31 | 4197 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4198 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4199 | `					}` |
|      31 | 4200 | `					if( result.x.iVal == 0 ){` |
|       - | 4201 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4202 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4203 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4204 | `						if( pVal1 && pVal2 ){` |
|      13 | 4205 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4206 | `								keep = 0;` |
|       9 | 4207 | `								PH7_MemObjRelease(&result);` |
|       - | 4208 | `								/* release keys too before breaking */` |
|       9 | 4209 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4210 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4211 | `								break;` |
|       - | 4212 | `							}` |
|       2 | 4213 | `						}` |
|       2 | 4214 | `					}` |
|      11 | 4215 | `				}` |
|      23 | 4216 | `				PH7_MemObjRelease(&result);` |
|      23 | 4217 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4218 | `				PH7_MemObjRelease(&key2);` |
|       - | 4219 | `				/* move to next node */` |
|      23 | 4220 | `				pIt = pIt->pPrev;` |
|      23 | 4221 | `				if( keep == 0 ) break;` |
|       1 | 4222 | `			}` |
|      21 | 4223 | `			if( keep == 0 ) break;` |
|       7 | 4224 | `		}` |
|      17 | 4225 | `		if( keep ){` |
|       - | 4226 | `			/* Perform the insertion */` |
|       9 | 4227 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4228 | `		}` |
|       - | 4229 | `		/* Point to the next entry */` |
|      17 | 4230 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4231 | `		n--;` |
|       1 | 4232 | `	}` |
|       - | 4233 | `	/* Return the freshly created array */` |
|       9 | 4234 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4235 | `	return PH7_OK;` |
|      13 | 4236 |  |
|       - | 4237 | `/*` |
|       - | 4238 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4239 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4240 | ` * Parameters` |
|       - | 4241 | ` *  $array1` |
|       - | 4242 | ` *    The array to compare from` |
|       - | 4243 | ` *  $array2` |
|       - | 4244 | ` *    An array to compare against` |
|       - | 4245 | ` *  $...` |
|       - | 4246 | ` *   More arrays to compare against` |
|       - | 4247 | ` * Return` |
|       - | 4248 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4249 | ` *  in any of the other arrays.` |
|       - | 4250 | ` * Note that NULL is returned on failure.` |
|       - | 4251 | ` */` |
|      14 | 4252 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4253 |  |
|       - | 4254 | `	ph7_hashmap_node *pEntry;` |
|       - | 4255 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4256 | `	ph7_value *pArray;` |
|       - | 4257 | `	sxi32 rc;` |
|       - | 4258 | `	sxu32 n;` |
|       - | 4259 | `	int i;` |
|       - | 4260 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4261 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4262 | `	 * helpers. */` |
|      16 | 4263 | `	if( nArg < 1 ){` |
|       4 | 4264 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4265 | `			"ArgumentCountError",` |
|       - | 4266 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4267 | `			nArg` |
|       - | 4268 | `			);` |
|       - | 4269 | `	}` |
|      14 | 4270 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4271 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4272 | `			"TypeError",` |
|       - | 4273 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4274 | `			ph7_type_name(apArg[0])` |
|       - | 4275 | `			);` |
|       - | 4276 | `	}` |
|      20 | 4277 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4278 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4279 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4280 | `				"TypeError",` |
|       - | 4281 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4282 | `				i + 1,` |
|       2 | 4283 | `				ph7_type_name(apArg[i])` |
|       - | 4284 | `				);` |
|       - | 4285 | `		}` |
|       5 | 4286 | `	}` |
|       9 | 4287 | `	if( nArg == 1 ){` |
|       - | 4288 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4289 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4290 | `		return PH7_OK;` |
|       - | 4291 | `	}` |
|       - | 4292 | `	/* Create a new array */` |
|       7 | 4293 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4294 | `	if( pArray == 0 ){` |
|     ! 0 | 4295 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4296 | `		return PH7_OK;` |
|       - | 4297 | `	}` |
|       - | 4298 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4299 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4300 | `	/* Perfrom the diff */` |
|       7 | 4301 | `	pEntry = pSrc->pFirst;` |
|       7 | 4302 | `	n = pSrc->nEntry;` |
|      12 | 4303 | `	for(;;){` |
|      25 | 4304 | `		if( n < 1 ){` |
|       7 | 4305 | `			break;` |
|       - | 4306 | `		}` |
|      31 | 4307 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4308 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4309 | `				/* ignore */` |
|     ! 0 | 4310 | `				continue;` |
|       - | 4311 | `			}` |
|      23 | 4312 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4313 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4314 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4315 | `				/* Blob lookup */` |
|      17 | 4316 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4317 | `			}else{` |
|       - | 4318 | `				/* Int lookup */` |
|       7 | 4319 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4320 | `			}` |
|      23 | 4321 | `			if( rc == SXRET_OK ){` |
|       - | 4322 | `				/* Key exists,break immediately */` |
|      11 | 4323 | `				break;` |
|       - | 4324 | `			}` |
|       7 | 4325 | `		}` |
|      19 | 4326 | `		if( i >= nArg ){` |
|       - | 4327 | `			/* Perform the insertion */` |
|       9 | 4328 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4329 | `		}` |
|       - | 4330 | `		/* Point to the next entry */` |
|      19 | 4331 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4332 | `		n--;` |
|       1 | 4333 | `	}` |
|       - | 4334 | `	/* Return the freshly created array */` |
|       7 | 4335 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4336 | `	return PH7_OK;` |
|       9 | 4337 |  |
|       - | 4338 | `/*` |
|       - | 4339 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4340 | ` *  Computes the intersection of arrays.` |
|       - | 4341 | ` * Parameters` |
|       - | 4342 | ` *  $array1` |
|       - | 4343 | ` *    The array to compare from` |
|       - | 4344 | ` *  $array2` |
|       - | 4345 | ` *    An array to compare against` |
|       - | 4346 | ` *  $...` |
|       - | 4347 | ` *   More arrays to compare against` |
|       - | 4348 | ` * Return` |
|       - | 4349 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4350 | ` *  in all of the parameters.` |
|       - | 4351 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4352 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4353 | ` */` |
|      22 | 4354 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4355 |  |
|       - | 4356 | `	ph7_hashmap_node *pEntry;` |
|       - | 4357 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4358 | `	ph7_value *pArray;` |
|       - | 4359 | `	ph7_value *pVal;` |
|       - | 4360 | `	sxi32 rc;` |
|       - | 4361 | `	sxu32 n;` |
|       - | 4362 | `	int i;` |
|      24 | 4363 | `	if( nArg < 1 ){` |
|       4 | 4364 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4365 | `			"ArgumentCountError",` |
|       - | 4366 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4367 | `			nArg` |
|       - | 4368 | `			);` |
|       - | 4369 | `	}` |
|      22 | 4370 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4371 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4372 | `			"TypeError",` |
|       - | 4373 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4374 | `			ph7_type_name(apArg[0])` |
|       - | 4375 | `			);` |
|       - | 4376 | `	}` |
|      36 | 4377 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4378 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4379 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4380 | `				"TypeError",` |
|       - | 4381 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4382 | `				i + 1,` |
|       2 | 4383 | `				ph7_type_name(apArg[i])` |
|       - | 4384 | `				);` |
|       - | 4385 | `		}` |
|       9 | 4386 | `	}` |
|      17 | 4387 | `	if( nArg == 1 ){` |
|       - | 4388 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4389 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4390 | `		return PH7_OK;` |
|       - | 4391 | `	}` |
|       - | 4392 | `	/* Create a new array */` |
|      15 | 4393 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4394 | `	if( pArray == 0 ){` |
|     ! 0 | 4395 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4396 | `		return PH7_OK;` |
|       - | 4397 | `	}` |
|       - | 4398 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4399 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4400 | `	/* Perform the intersection */` |
|      15 | 4401 | `	pEntry = pSrc->pFirst;` |
|      15 | 4402 | `	n = pSrc->nEntry;` |
|      31 | 4403 | `	for(;;){` |
|      63 | 4404 | `		if( n < 1 ){` |
|      15 | 4405 | `			break;` |
|       - | 4406 | `		}` |
|       - | 4407 | `		/* Extract the node value */` |
|      49 | 4408 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4409 | `		if( pVal ){` |
|      79 | 4410 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4411 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4412 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4413 | `				/* Perform the lookup */` |
|      55 | 4414 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4415 | `				if( rc != SXRET_OK ){` |
|       - | 4416 | `					/* Value does not exist */` |
|      25 | 4417 | `					break;` |
|       - | 4418 | `				}` |
|      16 | 4419 | `			}` |
|      49 | 4420 | `			if( i >= nArg ){` |
|       - | 4421 | `				/* Perform the insertion */` |
|      25 | 4422 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4423 | `			}` |
|      24 | 4424 | `		}` |
|       - | 4425 | `		/* Point to the next entry */` |
|      49 | 4426 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4427 | `		n--;` |
|       1 | 4428 | `	}` |
|       - | 4429 | `	/* Return the freshly created array */` |
|      15 | 4430 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4431 | `	return PH7_OK;` |
|      13 | 4432 |  |
|       - | 4433 | `/*` |
|       - | 4434 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4435 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4436 | ` * Parameters` |
|       - | 4437 | ` *  $array1` |
|       - | 4438 | ` *    The array to compare from` |
|       - | 4439 | ` *  $array2` |
|       - | 4440 | ` *    An array to compare against` |
|       - | 4441 | ` *  $...` |
|       - | 4442 | ` *   More arrays to compare against` |
|       - | 4443 | ` * Return` |
|       - | 4444 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4445 | ` *  in all the arguments, with matching keys.` |
|       - | 4446 | ` */` |
|      22 | 4447 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4448 |  |
|       - | 4449 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4450 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4451 | `	ph7_value *pArray;` |
|       - | 4452 | `	ph7_value *pVal;` |
|       - | 4453 | `	sxi32 rc;` |
|       - | 4454 | `	sxu32 n;` |
|       - | 4455 | `	int i;` |
|      24 | 4456 | `	if( nArg < 1 ){` |
|       4 | 4457 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4458 | `			"ArgumentCountError",` |
|       - | 4459 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4460 | `			nArg` |
|       - | 4461 | `			);` |
|       - | 4462 | `	}` |
|      22 | 4463 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4464 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4465 | `			"TypeError",` |
|       - | 4466 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4467 | `			ph7_type_name(apArg[0])` |
|       - | 4468 | `			);` |
|       - | 4469 | `	}` |
|      36 | 4470 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4471 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4472 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4473 | `				"TypeError",` |
|       - | 4474 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4475 | `				i + 1,` |
|       2 | 4476 | `				ph7_type_name(apArg[i])` |
|       - | 4477 | `				);` |
|       - | 4478 | `		}` |
|       9 | 4479 | `	}` |
|      17 | 4480 | `	if( nArg == 1 ){` |
|       - | 4481 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4482 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4483 | `		return PH7_OK;` |
|       - | 4484 | `	}` |
|       - | 4485 | `	/* Create a new array */` |
|      15 | 4486 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4487 | `	if( pArray == 0 ){` |
|     ! 0 | 4488 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4489 | `		return PH7_OK;` |
|       - | 4490 | `	}` |
|       - | 4491 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4492 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4493 | `	/* Perform the intersection */` |
|      15 | 4494 | `	pEntry = pSrc->pFirst;` |
|      15 | 4495 | `	n = pSrc->nEntry;` |
|      15 | 4496 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4497 | `	for(;;){` |
|      47 | 4498 | `		if( n < 1 ){` |
|      15 | 4499 | `			break;` |
|       - | 4500 | `		}` |
|       - | 4501 | `		/* Extract the node value */` |
|      33 | 4502 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4503 | `		if( pVal ){` |
|      53 | 4504 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4505 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4506 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4507 | `				/* Perform a key lookup first */` |
|      37 | 4508 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4509 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4510 | `				}else{` |
|      23 | 4511 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4512 | `				}` |
|      37 | 4513 | `				if( rc != SXRET_OK ){` |
|       - | 4514 | `					/* No such key,break immediately */` |
|       7 | 4515 | `					break;` |
|       - | 4516 | `				}` |
|       - | 4517 | `				/* Perform the lookup */` |
|      31 | 4518 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4519 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4520 | `					/* Value does not exist */` |
|       6 | 4521 | `					break;` |
|       - | 4522 | `				}` |
|      11 | 4523 | `			}` |
|      33 | 4524 | `			if( i >= nArg ){` |
|       - | 4525 | `				/* Perform the insertion */` |
|      17 | 4526 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4527 | `			}` |
|      16 | 4528 | `		}` |
|       - | 4529 | `		/* Point to the next entry */` |
|      33 | 4530 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4531 | `		n--;` |
|       1 | 4532 | `	}` |
|       - | 4533 | `	/* Return the freshly created array */` |
|      15 | 4534 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4535 | `	return PH7_OK;` |
|      13 | 4536 |  |
|       - | 4537 | `/*` |
|       - | 4538 | ` * array array_intersect_key(array $array1 ,array $array2,...)` |
|       - | 4539 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4540 | ` * Parameters` |
|       - | 4541 | ` *  $array1` |
|       - | 4542 | ` *    The array to compare from` |
|       - | 4543 | ` *  $array2` |
|       - | 4544 | ` *    An array to compare against` |
|       - | 4545 | ` *  $...` |
|       - | 4546 | ` *   More arrays to compare against` |
|       - | 4547 | ` * Return` |
|       - | 4548 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4549 | ` *  have keys that are present in all arguments.` |
|       - | 4550 | ` * Note that NULL is returned on failure.` |
|       - | 4551 | ` */` |
|       2 | 4552 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4553 |  |
|       - | 4554 | `	ph7_hashmap_node *pEntry;` |
|       - | 4555 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4556 | `	ph7_value *pArray;` |
|       - | 4557 | `	sxi32 rc;` |
|       - | 4558 | `	sxu32 n;` |
|       - | 4559 | `	int i;` |
|       3 | 4560 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4561 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4562 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4563 | `		return PH7_OK;` |
|       - | 4564 | `	}` |
|       3 | 4565 | `	if( nArg == 1 ){` |
|       - | 4566 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4567 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4568 | `		return PH7_OK;` |
|       - | 4569 | `	}` |
|       - | 4570 | `	/* Create a new array */` |
|       3 | 4571 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4572 | `	if( pArray == 0 ){` |
|     ! 0 | 4573 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4574 | `		return PH7_OK;` |
|       - | 4575 | `	}` |
|       - | 4576 | `	/* Point to the internal representation of the main hashmap */` |
|       3 | 4577 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4578 | `	/* Perfrom the intersection */` |
|       3 | 4579 | `	pEntry = pSrc->pFirst;` |
|       3 | 4580 | `	n = pSrc->nEntry;` |
|       4 | 4581 | `	for(;;){` |
|       9 | 4582 | `		if( n < 1 ){` |
|       3 | 4583 | `			break;` |
|       - | 4584 | `		}` |
|       9 | 4585 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       7 | 4586 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4587 | `				/* ignore */` |
|     ! 0 | 4588 | `				continue;` |
|       - | 4589 | `			}` |
|       7 | 4590 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       7 | 4591 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     ! 0 | 4592 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4593 | `				/* Blob lookup */` |
|     ! 0 | 4594 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|     ! 0 | 4595 | `			}else{` |
|       - | 4596 | `				/* Int key */` |
|       7 | 4597 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4598 | `			}` |
|       7 | 4599 | `			if( rc != SXRET_OK ){` |
|       - | 4600 | `				/* Key does not exists,break immediately */` |
|       5 | 4601 | `				break;` |
|       - | 4602 | `			}` |
|       2 | 4603 | `		}` |
|       7 | 4604 | `		if( i >= nArg ){` |
|       - | 4605 | `			/* Perform the insertion */` |
|       3 | 4606 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       1 | 4607 | `		}` |
|       - | 4608 | `		/* Point to the next entry */` |
|       7 | 4609 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4610 | `		n--;` |
|       1 | 4611 | `	}` |
|       - | 4612 | `	/* Return the freshly created array */` |
|       3 | 4613 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4614 | `	return PH7_OK;` |
|       2 | 4615 |  |
|       - | 4616 | `/*` |
|       - | 4617 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4618 | ` *  Computes the intersection of arrays.` |
|       - | 4619 | ` * Parameters` |
|       - | 4620 | ` *  $array1` |
|       - | 4621 | ` *    The array to compare from` |
|       - | 4622 | ` *  $array2` |
|       - | 4623 | ` *    An array to compare against` |
|       - | 4624 | ` *  $...` |
|       - | 4625 | ` *   More arrays to compare against` |
|       - | 4626 | ` * $callback` |
|       - | 4627 | ` *  The callback comparison function.` |
|       - | 4628 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4629 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4630 | ` *  than the second.` |
|       - | 4631 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4632 | ` * Return` |
|       - | 4633 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4634 | ` *  in all of the parameters. .` |
|       - | 4635 | ` * Note that NULL is returned on failure.` |
|       - | 4636 | ` */` |
|       2 | 4637 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4638 |  |
|       - | 4639 | `	ph7_hashmap_node *pEntry;` |
|       - | 4640 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4641 | `	ph7_value *pCallback;` |
|       - | 4642 | `	ph7_value *pArray;` |
|       - | 4643 | `	ph7_value *pVal;` |
|       - | 4644 | `	sxi32 rc;` |
|       - | 4645 | `	sxu32 n;` |
|       - | 4646 | `	int i;` |
|       - | 4647 |  |
|       3 | 4648 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4649 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4650 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4651 | `		return PH7_OK;` |
|       - | 4652 | `	}` |
|       - | 4653 | `	/* Point to the callback */` |
|       3 | 4654 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4655 | `	if( nArg == 2 ){` |
|       - | 4656 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4657 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4658 | `		return PH7_OK;` |
|       - | 4659 | `	}` |
|       - | 4660 | `	/* Create a new array */` |
|       3 | 4661 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4662 | `	if( pArray == 0 ){` |
|     ! 0 | 4663 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4664 | `		return PH7_OK;` |
|       - | 4665 | `	}` |
|       - | 4666 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4667 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4668 | `	/* Perform the intersection */` |
|       3 | 4669 | `	pEntry = pSrc->pFirst;` |
|       3 | 4670 | `	n = pSrc->nEntry;` |
|       4 | 4671 | `	for(;;){` |
|       9 | 4672 | `		if( n < 1 ){` |
|       3 | 4673 | `			break;` |
|       - | 4674 | `		}` |
|       - | 4675 | `		/* Extract the node value */` |
|       7 | 4676 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4677 | `		if( pVal ){` |
|      11 | 4678 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4679 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4680 | `					/* ignore */` |
|     ! 0 | 4681 | `					continue;` |
|       - | 4682 | `				}` |
|       - | 4683 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4684 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4685 | `				/* Perform the lookup */` |
|       7 | 4686 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4687 | `				if( rc != SXRET_OK ){` |
|       - | 4688 | `					/* Value does not exist */` |
|       3 | 4689 | `					break;` |
|       - | 4690 | `				}` |
|       3 | 4691 | `			}` |
|       7 | 4692 | `			if( i >= (nArg-1) ){` |
|       - | 4693 | `				/* Perform the insertion */` |
|       5 | 4694 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4695 | `			}` |
|       3 | 4696 | `		}` |
|       - | 4697 | `		/* Point to the next entry */` |
|       7 | 4698 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4699 | `		n--;` |
|       1 | 4700 | `	}` |
|       - | 4701 | `	/* Return the freshly created array */` |
|       3 | 4702 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4703 | `	return PH7_OK;` |
|       2 | 4704 |  |
|       - | 4705 | `/*` |
|       - | 4706 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 4707 | ` *  Fill an array with values.` |
|       - | 4708 | ` * Parameters` |
|       - | 4709 | ` *  $start_index` |
|       - | 4710 | ` *    The first index of the returned array.` |
|       - | 4711 | ` *  $num` |
|       - | 4712 | ` *   Number of elements to insert.` |
|       - | 4713 | ` *  $value` |
|       - | 4714 | ` *    Value to use for filling.` |
|       - | 4715 | ` * Return` |
|       - | 4716 | ` *  The filled array or null on failure.` |
|       - | 4717 | ` */` |
|     238 | 4718 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4719 |  |
|       - | 4720 | `	ph7_value *pArray;` |
|       - | 4721 | `	int i,nEntry;` |
|       - | 4722 |  |
|       - | 4723 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 4724 | `	if( nArg != 3 ){` |
|       - | 4725 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 4726 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4727 | `			"ArgumentCountError",` |
|       - | 4728 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 4729 | `			nArg` |
|       - | 4730 | `			);` |
|       - | 4731 | `	}` |
|       - | 4732 |  |
|       - | 4733 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 4734 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 4735 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 4736 | `	 * and NULLs are rejected outright. */` |
|     466 | 4737 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 4738 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 4739 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4740 | `			"TypeError",` |
|       - | 4741 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 4742 | `			ph7_type_name(apArg[0])` |
|       - | 4743 | `			);` |
|       - | 4744 | `	}` |
|     234 | 4745 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 4746 | `		int len;` |
|       8 | 4747 | `		sxu8 bReal = FALSE;` |
|       8 | 4748 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 4749 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 4750 | `			/* Non‑numeric string is an error. */` |
|       3 | 4751 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4752 | `				"TypeError",` |
|       - | 4753 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 4754 | `				);` |
|       - | 4755 | `		}` |
|       5 | 4756 | `		if( bReal ){` |
|       - | 4757 | `			/* float-string -> deprecation warning */` |
|       4 | 4758 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4759 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 4760 | `				zStr` |
|       - | 4761 | `				);` |
|       1 | 4762 | `		}` |
|       2 | 4763 | `	}` |
|       - | 4764 |  |
|       - | 4765 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 4766 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 4767 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 4768 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4769 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4770 | `			"TypeError",` |
|       - | 4771 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 4772 | `			ph7_type_name(apArg[1])` |
|       - | 4773 | `			);` |
|       - | 4774 | `	}` |
|     232 | 4775 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4776 | `		int len;` |
|       3 | 4777 | `		sxu8 bReal = FALSE;` |
|       3 | 4778 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4779 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 4780 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4781 | `				"TypeError",` |
|       - | 4782 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 4783 | `				);` |
|       - | 4784 | `		}` |
|     ! 0 | 4785 | `	}` |
|       - | 4786 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 4787 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 4788 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 4789 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 4790 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 4791 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 4792 | `		if( d != (double)i64 ){` |
|       7 | 4793 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4794 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 4795 | `				d` |
|       - | 4796 | `				);` |
|       2 | 4797 | `		}` |
|       2 | 4798 | `	}` |
|       - | 4799 |  |
|       - | 4800 | `	/* Total number of entries to insert */` |
|     230 | 4801 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 4802 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 4803 | `	if( nEntry < 0 ){` |
|       3 | 4804 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4805 | `			"ValueError",` |
|       - | 4806 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 4807 | `			);` |
|       - | 4808 | `	}` |
|       - | 4809 |  |
|       - | 4810 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 4811 | `	if( nEntry == 0 ){` |
|       7 | 4812 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 4813 | `		return PH7_OK;` |
|       - | 4814 | `	}` |
|       - | 4815 |  |
|       - | 4816 | `	/* Create a new array */` |
|     221 | 4817 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 4818 | `	if( pArray == 0 ){` |
|     ! 0 | 4819 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4820 | `		return PH7_OK;` |
|       - | 4821 | `	}` |
|       - | 4822 |  |
|       - | 4823 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 4824 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 4825 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 4826 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 4827 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 4828 | `	}` |
|       - | 4829 | `	/* Return the filled array */` |
|     221 | 4830 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 4831 | `	return PH7_OK;` |
|     121 | 4832 |  |
|       - | 4833 | `/*` |
|       - | 4834 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 4835 | ` *  Fill an array with values, specifying keys.` |
|       - | 4836 | ` * Parameters` |
|       - | 4837 | ` *  $input` |
|       - | 4838 | ` *   Array of values that will be used as key.` |
|       - | 4839 | ` *  $value` |
|       - | 4840 | ` *    Value to use for filling.` |
|       - | 4841 | ` * Return` |
|       - | 4842 | ` *  The filled array.` |
|       - | 4843 | ` * Throws` |
|       - | 4844 | ` *  ValueError if $input is not an array.` |
|       - | 4845 | ` */` |
|      26 | 4846 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4847 |  |
|       - | 4848 | `	ph7_hashmap_node *pEntry;` |
|       - | 4849 | `	ph7_hashmap *pSrc;` |
|       - | 4850 | `	ph7_value *pArray;` |
|       - | 4851 | `	sxu32 n;` |
|       - | 4852 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 4853 | `	if( nArg != 2 ){` |
|      10 | 4854 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4855 | `			"ArgumentCountError",` |
|       - | 4856 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 4857 | `			nArg` |
|       - | 4858 | `			);` |
|       - | 4859 | `	}` |
|       - | 4860 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 4861 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 4862 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4863 | `			"TypeError",` |
|       - | 4864 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 4865 | `			ph7_type_name(apArg[0])` |
|       - | 4866 | `			);` |
|       - | 4867 | `	}` |
|       - | 4868 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 4869 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4870 | `	/* Create a new array */` |
|      17 | 4871 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 4872 | `	if( pArray == 0 ){` |
|     ! 0 | 4873 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4874 | `		return PH7_OK;` |
|       - | 4875 | `	}` |
|       - | 4876 | `	/* Perform the requested operation */` |
|      17 | 4877 | `	pEntry = pSrc->pFirst;` |
|      45 | 4878 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 4879 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 4880 | `		/* Point to the next entry */` |
|      29 | 4881 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 4882 | `	}` |
|       - | 4883 | `	/* Return the filled array */` |
|      17 | 4884 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4885 | `	return PH7_OK;` |
|      15 | 4886 |  |
|       - | 4887 | `/*` |
|       - | 4888 | ` * array array_combine(array $keys,array $values)` |
|       - | 4889 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 4890 | ` * Parameters` |
|       - | 4891 | ` *  $keys` |
|       - | 4892 | ` *    Array of keys to be used.` |
|       - | 4893 | ` * $values` |
|       - | 4894 | ` *   Array of values to be used.` |
|       - | 4895 | ` * Return` |
|       - | 4896 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 4897 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 4898 | ` *  not an array.` |
|       - | 4899 | ` */` |
|      18 | 4900 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4901 |  |
|       - | 4902 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 4903 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 4904 | `	ph7_value *pArray;` |
|       - | 4905 | `	sxu32 n;` |
|       - | 4906 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 4907 | `	if( nArg != 2 ){` |
|       - | 4908 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 4909 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4910 | `			"ArgumentCountError",` |
|       - | 4911 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 4912 | `			nArg` |
|       - | 4913 | `			);` |
|       - | 4914 | `	}` |
|       - | 4915 | `	/* Validate argument types individually so we can report the correct` |
|       - | 4916 | `	 * argument index in the error message. */` |
|      18 | 4917 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4918 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4919 | `			"TypeError",` |
|       - | 4920 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 4921 | `			ph7_type_name(apArg[0])` |
|       - | 4922 | `			);` |
|       - | 4923 | `	}` |
|      16 | 4924 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 4925 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4926 | `			"TypeError",` |
|       - | 4927 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 4928 | `			ph7_type_name(apArg[1])` |
|       - | 4929 | `			);` |
|       - | 4930 | `	}` |
|       - | 4931 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 4932 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 4933 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 4934 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 4935 | `		/* Length mismatch -> ValueError */` |
|       3 | 4936 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4937 | `			"ValueError",` |
|       - | 4938 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 4939 | `			);` |
|       - | 4940 | `	}` |
|       - | 4941 | `	/* Create a new array */` |
|      11 | 4942 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4943 | `	if( pArray == 0 ){` |
|     ! 0 | 4944 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4945 | `		return PH7_OK;` |
|       - | 4946 | `	}` |
|       - | 4947 | `	/* Perform the requested operation */` |
|      11 | 4948 | `	pKe = pKey->pFirst;` |
|      11 | 4949 | `	pVe = pValue->pFirst;` |
|      33 | 4950 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 4951 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 4952 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 4953 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 4954 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 4955 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 4956 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 4957 | `		 * original array must not be mutated. */` |
|      23 | 4958 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 4959 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 4960 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 4961 | `			if( pTmpKey ){` |
|       5 | 4962 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 4963 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 4964 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 4965 | `				pKeyCopy = pTmpKey;` |
|       2 | 4966 | `			}` |
|       2 | 4967 | `		}` |
|      23 | 4968 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 4969 | `		/* Point to the next entry */` |
|      23 | 4970 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 4971 | `		pVe = pVe->pPrev;` |
|      12 | 4972 | `	}` |
|       - | 4973 | `	/* Return the filled array */` |
|      11 | 4974 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4975 | `	return PH7_OK;` |
|      11 | 4976 |  |
|       - | 4977 | `/*` |
|       - | 4978 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 4979 | ` *  Return an array with elements in reverse order.` |
|       - | 4980 | ` * Parameters` |
|       - | 4981 | ` *  $array` |
|       - | 4982 | ` *   The input array.` |
|       - | 4983 | ` *  $preserve_keys (optional)` |
|       - | 4984 | ` *   If set to TRUE keys are preserved.` |
|       - | 4985 | ` * Return` |
|       - | 4986 | ` *  The reversed array.` |
|       - | 4987 | ` */` |
|      20 | 4988 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4989 |  |
|       - | 4990 | `	ph7_hashmap_node *pEntry;` |
|       - | 4991 | `	ph7_hashmap *pSrc;` |
|       - | 4992 | `	ph7_value *pArray;` |
|       - | 4993 | `	int bPreserve;` |
|       - | 4994 | `	sxu32 n;` |
|      22 | 4995 | `	if( nArg < 1 ){` |
|       4 | 4996 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4997 | `			"ArgumentCountError",` |
|       - | 4998 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 4999 | `			nArg` |
|       - | 5000 | `			);` |
|       - | 5001 | `	}` |
|       - | 5002 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5003 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5004 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5005 | `			"TypeError",` |
|       - | 5006 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5007 | `			ph7_type_name(apArg[0])` |
|       - | 5008 | `			);` |
|       - | 5009 | `	}` |
|      17 | 5010 | `	bPreserve = FALSE;` |
|      17 | 5011 | `	if( nArg > 1 ){` |
|       7 | 5012 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5013 | `	}` |
|       - | 5014 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5015 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5016 | `	/* Create a new array */` |
|      17 | 5017 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5018 | `	if( pArray == 0 ){` |
|     ! 0 | 5019 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5020 | `		return PH7_OK;` |
|       - | 5021 | `	}` |
|       - | 5022 | `	/* Perform the requested operation */` |
|      17 | 5023 | `	pEntry = pSrc->pLast;` |
|      55 | 5024 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5025 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5026 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5027 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5028 | `		/* Point to the previous entry */` |
|      39 | 5029 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5030 | `	}` |
|      17 | 5031 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5032 | `	return PH7_OK;` |
|      12 | 5033 |  |
|       - | 5034 | `/*` |
|       - | 5035 | ` * array array_unique(array $array[,int $sort_flags = SORT_STRING ])` |
|       - | 5036 | ` *  Removes duplicate values from an array` |
|       - | 5037 | ` * Parameter` |
|       - | 5038 | ` *  $array` |
|       - | 5039 | ` *   The input array.` |
|       - | 5040 | ` *  $sort_flags` |
|       - | 5041 | ` *    The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 5042 | ` *    Sorting type flags:` |
|       - | 5043 | ` *       SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5044 | ` *       SORT_NUMERIC - compare items numerically` |
|       - | 5045 | ` *       SORT_STRING - compare items as strings` |
|       - | 5046 | ` *       SORT_LOCALE_STRING - compare items as` |
|       - | 5047 | ` * Return` |
|       - | 5048 | ` *  Filtered array or NULL on failure.` |
|       - | 5049 | ` */` |
|       2 | 5050 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5051 |  |
|       - | 5052 | `	ph7_hashmap_node *pEntry;` |
|       - | 5053 | `	ph7_value *pNeedle;` |
|       - | 5054 | `	ph7_hashmap *pSrc;` |
|       - | 5055 | `	ph7_value *pArray;` |
|       - | 5056 | `	int bStrict;` |
|       - | 5057 | `	sxi32 rc;` |
|       - | 5058 | `	sxu32 n;` |
|       3 | 5059 | `	if( nArg < 1 ){` |
|       - | 5060 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 5061 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5062 | `		return PH7_OK;` |
|       - | 5063 | `	}` |
|       - | 5064 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 5065 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5066 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 5067 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5068 | `		return PH7_OK;` |
|       - | 5069 | `	}` |
|       3 | 5070 | `	bStrict = FALSE;` |
|       3 | 5071 | `	if( nArg > 1 ){` |
|     ! 0 | 5072 | `		bStrict = ph7_value_to_int(apArg[1]) == 3 /* SORT_REGULAR */ ? 1 : 0;` |
|     ! 0 | 5073 | `	}` |
|       - | 5074 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 5075 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5076 | `	/* Create a new array */` |
|       3 | 5077 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5078 | `	if( pArray == 0 ){` |
|     ! 0 | 5079 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5080 | `		return PH7_OK;` |
|       - | 5081 | `	}` |
|       - | 5082 | `	/* Perform the requested operation */` |
|       3 | 5083 | `	pEntry = pSrc->pFirst;` |
|      13 | 5084 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      11 | 5085 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      11 | 5086 | `		rc = SXERR_NOTFOUND;` |
|      11 | 5087 | `		if( pNeedle ){` |
|      11 | 5088 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|       5 | 5089 | `		}` |
|      11 | 5090 | `		if( rc != SXRET_OK ){` |
|       - | 5091 | `			/* Perform the insertion */` |
|       7 | 5092 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 5093 | `		}` |
|       - | 5094 | `		/* Point to the next entry */` |
|      11 | 5095 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 5096 | `	}` |
|       - | 5097 | `	/* Return the freshly created array */` |
|       3 | 5098 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5099 | `	return PH7_OK;` |
|       2 | 5100 |  |
|       - | 5101 | `/*` |
|       - | 5102 | ` * array array_flip(array $input)` |
|       - | 5103 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5104 | ` * Parameter` |
|       - | 5105 | ` *  $input` |
|       - | 5106 | ` *   Input array.` |
|       - | 5107 | ` * Return` |
|       - | 5108 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5109 | ` */` |
|      34 | 5110 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5111 |  |
|       - | 5112 | `	ph7_hashmap_node *pEntry;` |
|       - | 5113 | `	ph7_hashmap *pSrc;` |
|       - | 5114 | `	ph7_value *pArray;` |
|       - | 5115 | `	ph7_value *pKey;` |
|       - | 5116 | `	ph7_value sVal;` |
|       - | 5117 | `	sxu32 n;` |
|       - | 5118 |  |
|       - | 5119 | `	/* PHP requires exactly one argument */` |
|      36 | 5120 | `	if( nArg != 1 ){` |
|       - | 5121 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5122 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5123 | `			"ArgumentCountError",` |
|       - | 5124 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5125 | `			nArg` |
|       - | 5126 | `			);` |
|       - | 5127 | `	}` |
|       - | 5128 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5129 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5130 | `		/* Type mismatch -> TypeError */` |
|       7 | 5131 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5132 | `			"TypeError",` |
|       - | 5133 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5134 | `			ph7_type_name(apArg[0])` |
|       - | 5135 | `			);` |
|       - | 5136 | `	}` |
|       - | 5137 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5138 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5139 | `	/* Create a new array */` |
|      27 | 5140 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5141 | `	if( pArray == 0 ){` |
|     ! 0 | 5142 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5143 | `		return PH7_OK;` |
|       - | 5144 | `	}` |
|       - | 5145 | `	/* Start processing */` |
|      27 | 5146 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5147 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5148 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5149 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5150 | `		if( pKey ){` |
|       - | 5151 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5152 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5153 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5154 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5155 | `					);` |
|   22236 | 5156 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5157 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5158 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5159 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5160 | `				}else{` |
|       - | 5161 | `					SyString sStr;` |
|    2227 | 5162 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5163 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5164 | `				}` |
|       - | 5165 | `				/* Perform the insertion */` |
|   22227 | 5166 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5167 | `				/* Safely release the value because each inserted entry` |
|       - | 5168 | `				 * has its own private copy of the value.` |
|       - | 5169 | `				 */` |
|   22227 | 5170 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5171 | `			}else{` |
|       - | 5172 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5173 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5174 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5175 | `					);` |
|       - | 5176 | `			}` |
|   11118 | 5177 | `		}` |
|       - | 5178 | `		/* Point to the next entry */` |
|   22237 | 5179 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5180 | `	}` |
|       - | 5181 | `	/* Return the freshly created array */` |
|      27 | 5182 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5183 | `	return PH7_OK;` |
|      19 | 5184 |  |
|       - | 5185 | `/*` |
|       - | 5186 | ` * number array_sum(array $array )` |
|       - | 5187 | ` *  Calculate the sum of values in an array.` |
|       - | 5188 | ` * Parameters` |
|       - | 5189 | ` *  $array: The input array.` |
|       - | 5190 | ` * Return` |
|       - | 5191 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5192 | ` */` |
|      24 | 5193 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5194 |  |
|       - | 5195 | `	ph7_hashmap_node *pEntry;` |
|       - | 5196 | `	ph7_value *pObj;` |
|      25 | 5197 | `	double dSum = 0;` |
|       - | 5198 | `	sxu32 n;` |
|      25 | 5199 | `	pEntry = pMap->pFirst;` |
|      91 | 5200 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5201 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5202 | `		if( pObj ){` |
|      67 | 5203 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5204 | `				dSum += pObj->rVal;` |
|      53 | 5205 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5206 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5207 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5208 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5209 | `					double dv = 0;` |
|      13 | 5210 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5211 | `					dSum += dv;` |
|       7 | 5212 | `				}` |
|      12 | 5213 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5214 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5215 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5216 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5217 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5218 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5219 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5220 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5221 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5222 | `			}` |
|       - | 5223 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5224 | `		}` |
|       - | 5225 | `		/* Point to the next entry */` |
|      67 | 5226 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5227 | `	}` |
|       - | 5228 | `	/* Return sum */` |
|      25 | 5229 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5230 |  |
|      18 | 5231 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5232 |  |
|       - | 5233 | `	ph7_hashmap_node *pEntry;` |
|       - | 5234 | `	ph7_value *pObj;` |
|      20 | 5235 | `	sxi64 nSum = 0;` |
|       - | 5236 | `	sxu32 n;` |
|      20 | 5237 | `	pEntry = pMap->pFirst;` |
|      80 | 5238 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5239 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5240 | `		if( pObj ){` |
|      62 | 5241 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5242 | `				nSum += pObj->x.iVal;` |
|      36 | 5243 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5244 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5245 | `					sxi64 nv = 0;` |
|       5 | 5246 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5247 | `					nSum += nv;` |
|       3 | 5248 | `				}` |
|       8 | 5249 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5250 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5251 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5252 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5253 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5254 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5255 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5256 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5257 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5258 | `			}` |
|       - | 5259 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5260 | `		}` |
|       - | 5261 | `		/* Point to the next entry */` |
|      62 | 5262 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5263 | `	}` |
|       - | 5264 | `	/* Return sum */` |
|      20 | 5265 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5266 |  |
|       - | 5267 | `/* number array_sum(array $array )` |
|       - | 5268 | ` * (See block-coment above)` |
|       - | 5269 | ` */` |
|      52 | 5270 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5271 |  |
|       - | 5272 | `	ph7_hashmap_node *pEntry;` |
|       - | 5273 | `	ph7_hashmap *pMap;` |
|       - | 5274 | `	ph7_value *pObj;` |
|      54 | 5275 | `	int useDouble = 0;` |
|       - | 5276 | `	sxu32 n;` |
|       - | 5277 | `	/* PHP requires exactly one argument */` |
|      54 | 5278 | `	if( nArg != 1 ){` |
|       7 | 5279 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5280 | `			"ArgumentCountError",` |
|       - | 5281 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5282 | `			nArg` |
|       - | 5283 | `			);` |
|       - | 5284 | `	}` |
|       - | 5285 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5286 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5287 | `		/* Type mismatch -> TypeError */` |
|       7 | 5288 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5289 | `			"TypeError",` |
|       - | 5290 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5291 | `			ph7_type_name(apArg[0])` |
|       - | 5292 | `			);` |
|       - | 5293 | `	}` |
|      46 | 5294 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5295 | `	if( pMap->nEntry < 1 ){` |
|       - | 5296 | `		/* Nothing to compute,return 0 */` |
|       3 | 5297 | `		ph7_result_int(pCtx,0);` |
|       3 | 5298 | `		return PH7_OK;` |
|       - | 5299 | `	}` |
|       - | 5300 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5301 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5302 | `	 */` |
|      44 | 5303 | `	pEntry = pMap->pFirst;` |
|     112 | 5304 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5305 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5306 | `		if( pObj ){` |
|      94 | 5307 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5308 | `				useDouble = 1;` |
|      19 | 5309 | `				break;` |
|       - | 5310 | `			}` |
|      76 | 5311 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5312 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5313 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5314 | `				sxu32 i;` |
|      23 | 5315 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5316 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5317 | `						useDouble = 1;` |
|       7 | 5318 | `						break;` |
|       - | 5319 | `					}` |
|       6 | 5320 | `				}` |
|      13 | 5321 | `				if( useDouble ){` |
|       7 | 5322 | `					break;` |
|       - | 5323 | `				}` |
|       3 | 5324 | `			}` |
|      34 | 5325 | `		}` |
|      70 | 5326 | `		pEntry = pEntry->pPrev;` |
|      36 | 5327 | `	}` |
|      44 | 5328 | `	if( useDouble ){` |
|      25 | 5329 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5330 | `	}else{` |
|      20 | 5331 | `		Int64Sum(pCtx,pMap);` |
|       - | 5332 | `	}` |
|      44 | 5333 | `	return PH7_OK;` |
|      28 | 5334 |  |
|       - | 5335 | `/*` |
|       - | 5336 | ` * number array_product(array $array )` |
|       - | 5337 | ` *  Calculate the product of values in an array.` |
|       - | 5338 | ` * Parameters` |
|       - | 5339 | ` *  $array: The input array.` |
|       - | 5340 | ` * Return` |
|       - | 5341 | ` *  Returns the product of values as an integer or float.` |
|       - | 5342 | ` */` |
|     ! 0 | 5343 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5344 |  |
|       - | 5345 | `	ph7_hashmap_node *pEntry;` |
|       - | 5346 | `	ph7_value *pObj;` |
|       - | 5347 | `	double dProd;` |
|       - | 5348 | `	sxu32 n;` |
|     ! 0 | 5349 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5350 | `	dProd = 1;` |
|     ! 0 | 5351 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5352 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5353 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5354 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5355 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5356 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5357 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5358 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5359 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5360 | `					double dv = 0;` |
|     ! 0 | 5361 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5362 | `					dProd *= dv;` |
|     ! 0 | 5363 | `				}` |
|     ! 0 | 5364 | `			}` |
|     ! 0 | 5365 | `		}` |
|       - | 5366 | `		/* Point to the next entry */` |
|     ! 0 | 5367 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5368 | `	}` |
|       - | 5369 | `	/* Return product */` |
|     ! 0 | 5370 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5371 |  |
|     ! 0 | 5372 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5373 |  |
|       - | 5374 | `	ph7_hashmap_node *pEntry;` |
|       - | 5375 | `	ph7_value *pObj;` |
|       - | 5376 | `	sxi64 nProd;` |
|       - | 5377 | `	sxu32 n;` |
|     ! 0 | 5378 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5379 | `	nProd = 1;` |
|     ! 0 | 5380 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5381 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5382 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5383 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5384 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5385 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5386 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5387 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5388 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5389 | `					sxi64 nv = 0;` |
|     ! 0 | 5390 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5391 | `					nProd *= nv;` |
|     ! 0 | 5392 | `				}` |
|     ! 0 | 5393 | `			}` |
|     ! 0 | 5394 | `		}` |
|       - | 5395 | `		/* Point to the next entry */` |
|     ! 0 | 5396 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5397 | `	}` |
|       - | 5398 | `	/* Return product */` |
|     ! 0 | 5399 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5400 |  |
|       - | 5401 | `/* number array_product(array $array )` |
|       - | 5402 | ` * (See block-block comment above)` |
|       - | 5403 | ` */` |
|     ! 0 | 5404 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5405 |  |
|       - | 5406 | `	ph7_hashmap *pMap;` |
|       - | 5407 | `	ph7_value *pObj;` |
|     ! 0 | 5408 | `	if( nArg < 1 ){` |
|       - | 5409 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5410 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5411 | `		return PH7_OK;` |
|       - | 5412 | `	}` |
|       - | 5413 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5414 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5415 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5416 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5417 | `		return PH7_OK;` |
|       - | 5418 | `	}` |
|     ! 0 | 5419 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5420 | `	if( pMap->nEntry < 1 ){` |
|       - | 5421 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5422 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5423 | `		return PH7_OK;` |
|       - | 5424 | `	}` |
|       - | 5425 | `	/* If the first element is of type float,then perform floating` |
|       - | 5426 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5427 | `	 */` |
|     ! 0 | 5428 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5429 | `	if( pObj == 0 ){` |
|     ! 0 | 5430 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5431 | `		return PH7_OK;` |
|       - | 5432 | `	}` |
|     ! 0 | 5433 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5434 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5435 | `	}else{` |
|     ! 0 | 5436 | `		Int64Prod(pCtx,pMap);` |
|       - | 5437 | `	}` |
|     ! 0 | 5438 | `	return PH7_OK;` |
|     ! 0 | 5439 |  |
|       - | 5440 | `/*` |
|       - | 5441 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5442 | ` *  Pick one or more random entries out of an array.` |
|       - | 5443 | ` * Parameters` |
|       - | 5444 | ` * $input` |
|       - | 5445 | ` *  The input array.` |
|       - | 5446 | ` * $num_req` |
|       - | 5447 | ` *  Specifies how many entries you want to pick.` |
|       - | 5448 | ` * Return` |
|       - | 5449 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5450 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5451 | ` *  NULL is returned on failure.` |
|       - | 5452 | ` */` |
|       6 | 5453 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5454 |  |
|       - | 5455 | `	ph7_hashmap_node *pNode;` |
|       - | 5456 | `	ph7_hashmap *pMap;` |
|       7 | 5457 | `	int nItem = 1;` |
|       7 | 5458 | `	if( nArg < 1 ){` |
|       - | 5459 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5460 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5461 | `		return PH7_OK;` |
|       - | 5462 | `	}` |
|       - | 5463 | `	/* Make sure we are dealing with an array */` |
|       7 | 5464 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5465 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5466 | `		return PH7_OK;` |
|       - | 5467 | `	}` |
|       - | 5468 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5469 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5470 | `	if(pMap->nEntry < 1 ){` |
|       - | 5471 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5472 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5473 | `		return PH7_OK;` |
|       - | 5474 | `	}` |
|       7 | 5475 | `	if( nArg > 1 ){` |
|       3 | 5476 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5477 | `	}` |
|       7 | 5478 | `	if( nItem < 2 ){` |
|       - | 5479 | `		sxu32 nEntry;` |
|       - | 5480 | `		/* Select a random number */` |
|       5 | 5481 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5482 | `		/* Extract the desired entry.` |
|       - | 5483 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5484 | `		 */` |
|       5 | 5485 | `		if( nEntry > pMap->nEntry / 2 ){` |
|     ! 0 | 5486 | `			pNode = pMap->pLast;` |
|     ! 0 | 5487 | `			nEntry = pMap->nEntry - nEntry;` |
|     ! 0 | 5488 | `			if( nEntry > 1 ){` |
|     ! 0 | 5489 | `				for(;;){` |
|     ! 0 | 5490 | `					if( nEntry == 0 ){` |
|     ! 0 | 5491 | `						break;` |
|       - | 5492 | `					}` |
|       - | 5493 | `					/* Point to the previous entry */` |
|     ! 0 | 5494 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5495 | `					nEntry--;` |
|     ! 0 | 5496 | `				}` |
|     ! 0 | 5497 | `			}` |
|     ! 0 | 5498 | `		}else{` |
|       5 | 5499 | `			pNode = pMap->pFirst;` |
|       3 | 5500 | `			for(;;){` |
|       7 | 5501 | `				if( nEntry == 0 ){` |
|       5 | 5502 | `					break;` |
|       - | 5503 | `				}` |
|       - | 5504 | `				/* Point to the next entry */` |
|       3 | 5505 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       3 | 5506 | `				nEntry--;` |
|       1 | 5507 | `			}` |
|       - | 5508 | `		}` |
|       5 | 5509 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5510 | `			/* Int key */` |
|       3 | 5511 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5512 | `		}else{` |
|       - | 5513 | `			/* Blob key */` |
|       3 | 5514 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5515 | `		}` |
|       3 | 5516 | `	}else{` |
|       - | 5517 | `		ph7_value sKey,*pArray;` |
|       - | 5518 | `		ph7_hashmap *pDest;` |
|       - | 5519 | `		/* Create a new array */` |
|       3 | 5520 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5521 | `		if( pArray == 0 ){` |
|     ! 0 | 5522 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5523 | `			return PH7_OK;` |
|       - | 5524 | `		}` |
|       - | 5525 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5526 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5527 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5528 | `		/* Copy the first n items */` |
|       3 | 5529 | `		pNode = pMap->pFirst;` |
|       3 | 5530 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5531 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5532 | `		}` |
|       7 | 5533 | `		while( nItem > 0){` |
|       5 | 5534 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5535 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5536 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5537 | `			/* Point to the next entry */` |
|       5 | 5538 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5539 | `			nItem--;` |
|       1 | 5540 | `		}` |
|       - | 5541 | `		/* Shuffle the array */` |
|       3 | 5542 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5543 | `		/* Rehash node */` |
|       3 | 5544 | `		HashmapSortRehash(pDest);` |
|       - | 5545 | `		/* Return the random array */` |
|       3 | 5546 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5547 | `	}` |
|       7 | 5548 | `	return PH7_OK;` |
|       4 | 5549 |  |
|       - | 5550 | `/*` |
|       - | 5551 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5552 | ` *  Split an array into chunks.` |
|       - | 5553 | ` * Parameters` |
|       - | 5554 | ` * $input` |
|       - | 5555 | ` *   The array to work on` |
|       - | 5556 | ` * $size` |
|       - | 5557 | ` *   The size of each chunk` |
|       - | 5558 | ` * $preserve_keys` |
|       - | 5559 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5560 | ` *   the chunk numerically.` |
|       - | 5561 | ` * Return` |
|       - | 5562 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5563 | ` *  zero, with each dimension containing size elements.` |
|       - | 5564 | ` */` |
|      42 | 5565 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5566 |  |
|       - | 5567 | `	ph7_value *pArray,*pChunk;` |
|       - | 5568 | `	ph7_hashmap_node *pEntry;` |
|       - | 5569 | `	ph7_hashmap *pMap;` |
|       - | 5570 | `	int bPreserve;` |
|       - | 5571 | `	sxu32 nChunk;` |
|       - | 5572 | `	sxu32 nSize;` |
|       - | 5573 | `	sxu32 n;` |
|       - | 5574 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5575 | `	if( nArg < 2 ){` |
|       - | 5576 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5577 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5578 | `			"ArgumentCountError",` |
|       - | 5579 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5580 | `			nArg` |
|       - | 5581 | `			);` |
|       - | 5582 | `	}` |
|      42 | 5583 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5584 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5585 | `			"TypeError",` |
|       - | 5586 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5587 | `			ph7_type_name(apArg[0])` |
|       - | 5588 | `			);` |
|       - | 5589 | `	}` |
|       - | 5590 | `	/* Create a new array */` |
|      40 | 5591 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5592 | `	if( pArray == 0 ){` |
|     ! 0 | 5593 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5594 | `		return PH7_OK;` |
|       - | 5595 | `	}` |
|       - | 5596 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5597 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5598 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5599 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5600 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5601 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5602 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5603 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5604 | `			"TypeError",` |
|       - | 5605 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5606 | `			ph7_type_name(apArg[1])` |
|       - | 5607 | `			);` |
|       - | 5608 | `	}` |
|       - | 5609 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5610 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5611 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5612 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5613 | `		int len;` |
|       3 | 5614 | `		sxu8 bReal = FALSE;` |
|       3 | 5615 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5616 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5617 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5618 | `				"TypeError",` |
|       - | 5619 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5620 | `				);` |
|       - | 5621 | `		}` |
|     ! 0 | 5622 | `		if( bReal ){` |
|       - | 5623 | `			/* float-string -> warn but allow */` |
|     ! 0 | 5624 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5625 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 5626 | `				zStr` |
|       - | 5627 | `				);` |
|     ! 0 | 5628 | `		}` |
|     ! 0 | 5629 | `	}` |
|       - | 5630 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 5631 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 5632 | `	 * later via ph7_value_to_int. */` |
|      38 | 5633 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5634 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5635 | `		sxi64 i = (sxi64)d;` |
|       3 | 5636 | `		if( d != (double)i ){` |
|       4 | 5637 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5638 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 5639 | `				d` |
|       - | 5640 | `				);` |
|       1 | 5641 | `		}` |
|       1 | 5642 | `	}` |
|       - | 5643 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5644 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5645 | `	{` |
|      38 | 5646 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 5647 | `		if( nSizeSigned < 1 ){` |
|       - | 5648 | `			/* size <= 0 -> ValueError */` |
|       5 | 5649 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5650 | `				"ValueError",` |
|       - | 5651 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5652 | `				);` |
|       - | 5653 | `		}` |
|      34 | 5654 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5655 | `	}` |
|      34 | 5656 | `	if( nSize >= pMap->nEntry ){` |
|       - | 5657 | `		/* Return the whole array */` |
|       3 | 5658 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 5659 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5660 | `		return PH7_OK;` |
|       - | 5661 | `	}` |
|      32 | 5662 | `	bPreserve = 0;` |
|      32 | 5663 | `	if( nArg > 2 ){` |
|       - | 5664 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5665 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5666 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5667 | `		 * normally, matching PHP behaviour. */` |
|      45 | 5668 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 5669 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 5670 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 5671 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5672 | `				"TypeError",` |
|       - | 5673 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 5674 | `				ph7_type_name(apArg[2])` |
|       - | 5675 | `				);` |
|       - | 5676 | `		}` |
|      21 | 5677 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 5678 | `	}` |
|       - | 5679 | `	/* Start processing */` |
|      27 | 5680 | `	pEntry = pMap->pFirst;` |
|      27 | 5681 | `	nChunk = 0;` |
|      27 | 5682 | `	pChunk = 0;` |
|      27 | 5683 | `	n = pMap->nEntry;` |
|      56 | 5684 | `	for( ;; ){` |
|     113 | 5685 | `		if( n < 1 ){` |
|       - | 5686 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5687 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5688 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5689 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5690 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5691 | `			 * exists. */` |
|      27 | 5692 | `			if( pChunk ){` |
|      27 | 5693 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 5694 | `			}` |
|      27 | 5695 | `			break;` |
|       - | 5696 | `		}` |
|      87 | 5697 | `		if( nChunk < 1 ){` |
|      71 | 5698 | `			if( pChunk ){` |
|       - | 5699 | `				/* Put the first chunk */` |
|      45 | 5700 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 5701 | `			}` |
|       - | 5702 | `			/* Create a new dimension */` |
|      71 | 5703 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5704 | `												   * will be automatically released as soon we return` |
|       - | 5705 | `												   * from this function */` |
|      71 | 5706 | `			if( pChunk == 0 ){` |
|     ! 0 | 5707 | `				break;` |
|       - | 5708 | `			}` |
|      71 | 5709 | `			nChunk = nSize;` |
|      35 | 5710 | `		}` |
|       - | 5711 | `		/* Insert the entry */` |
|      87 | 5712 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5713 | `		/* Point to the next entry */` |
|      87 | 5714 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 5715 | `		nChunk--;` |
|      87 | 5716 | `		n--;` |
|       1 | 5717 | `	}` |
|       - | 5718 | `	/* Return the multidimensional array */` |
|      27 | 5719 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5720 | `	return PH7_OK;` |
|      23 | 5721 |  |
|       - | 5722 | `/*` |
|       - | 5723 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5724 | ` *  Pad array to the specified length with a value.` |
|       - | 5725 | ` * $input` |
|       - | 5726 | ` *   Initial array of values to pad.` |
|       - | 5727 | ` * $pad_size` |
|       - | 5728 | ` *   New size of the array.` |
|       - | 5729 | ` * $pad_value` |
|       - | 5730 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5731 | ` */` |
|      28 | 5732 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5733 |  |
|       - | 5734 | `	ph7_hashmap *pMap;` |
|       - | 5735 | `	ph7_value *pArray;` |
|       - | 5736 | `	int nEntry;` |
|      30 | 5737 | `	if( nArg != 3 ){` |
|      10 | 5738 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5739 | `			"ArgumentCountError",` |
|       - | 5740 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 5741 | `			nArg` |
|       - | 5742 | `			);` |
|       - | 5743 | `	}` |
|      24 | 5744 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5745 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5746 | `			"TypeError",` |
|       - | 5747 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5748 | `			ph7_type_name(apArg[0])` |
|       - | 5749 | `			);` |
|       - | 5750 | `	}` |
|       - | 5751 | `	/* Create a new array */` |
|      21 | 5752 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 5753 | `	if( pArray == 0 ){` |
|     ! 0 | 5754 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5755 | `		return PH7_OK;` |
|       - | 5756 | `	}` |
|       - | 5757 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 5758 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5759 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 5760 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 5761 | `	if( nEntry < 0 ){` |
|       9 | 5762 | `		nEntry = -nEntry;` |
|       9 | 5763 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 5764 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5765 | `			/* Insert given items first */` |
|      17 | 5766 | `			while( nEntry > 0 ){` |
|      13 | 5767 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 5768 | `				nEntry--;` |
|       1 | 5769 | `			}` |
|       - | 5770 | `			/* Merge the two arrays */` |
|       5 | 5771 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5772 | `		}else{` |
|       5 | 5773 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5774 | `		}` |
|      17 | 5775 | `	}else if( nEntry > 0 ){` |
|      11 | 5776 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 5777 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5778 | `			/* Merge the two arrays first */` |
|       7 | 5779 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5780 | `			/* Insert given items */` |
|      25 | 5781 | `			while( nEntry > 0 ){` |
|      19 | 5782 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 5783 | `				nEntry--;` |
|       1 | 5784 | `			}` |
|       4 | 5785 | `		}else{` |
|       5 | 5786 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5787 | `		}` |
|       6 | 5788 | `	}else{` |
|       - | 5789 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 5790 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5791 | `	}` |
|       - | 5792 | `	/* Return the new array */` |
|      21 | 5793 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 5794 | `	return PH7_OK;` |
|      16 | 5795 |  |
|       - | 5796 | `/*` |
|       - | 5797 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5798 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5799 | ` * Parameters` |
|       - | 5800 | ` * $array` |
|       - | 5801 | ` *   The array in which elements are replaced.` |
|       - | 5802 | ` * $array1` |
|       - | 5803 | ` *   The array from which elements will be extracted.` |
|       - | 5804 | ` * ....` |
|       - | 5805 | ` *  More arrays from which elements will be extracted.` |
|       - | 5806 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5807 | ` * Return` |
|       - | 5808 | ` *  Returns an array, or NULL if an error occurs.` |
|       - | 5809 | ` */` |
|       2 | 5810 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5811 |  |
|       - | 5812 | `	ph7_hashmap *pMap;` |
|       - | 5813 | `	ph7_value *pArray;` |
|       - | 5814 | `	int i;` |
|       3 | 5815 | `	if( nArg < 1 ){` |
|       - | 5816 | `		/* Invalid arguments,return NULL */` |
|     ! 0 | 5817 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5818 | `		return PH7_OK;` |
|       - | 5819 | `	}` |
|       - | 5820 | `	/* Create a new array */` |
|       3 | 5821 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5822 | `	if( pArray == 0 ){` |
|     ! 0 | 5823 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5824 | `		return PH7_OK;` |
|       - | 5825 | `	}` |
|       - | 5826 | `	/* Perform the requested operation */` |
|       7 | 5827 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       5 | 5828 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|     ! 0 | 5829 | `			continue;` |
|       - | 5830 | `		}` |
|       - | 5831 | `		/* Point to the internal representation of the input hashmap */` |
|       5 | 5832 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       5 | 5833 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5834 | `	}` |
|       - | 5835 | `	/* Return the new array */` |
|       3 | 5836 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5837 | `	return PH7_OK;` |
|       2 | 5838 |  |
|       - | 5839 | `/*` |
|       - | 5840 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 5841 | ` *  Filters elements of an array using a callback function.` |
|       - | 5842 | ` * Parameters` |
|       - | 5843 | ` *  $input` |
|       - | 5844 | ` *    The array to iterate over` |
|       - | 5845 | ` * $callback` |
|       - | 5846 | ` *    The callback function to use` |
|       - | 5847 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 5848 | ` *    will be removed.` |
|       - | 5849 | ` * Return` |
|       - | 5850 | ` *  The filtered array.` |
|       - | 5851 | ` */` |
|      18 | 5852 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5853 |  |
|       - | 5854 | `	ph7_hashmap_node *pEntry;` |
|       - | 5855 | `	ph7_hashmap *pMap;` |
|       - | 5856 | `	ph7_value *pArray;` |
|       - | 5857 | `	ph7_value sResult;   /* Callback result */` |
|       - | 5858 | `	ph7_value *pValue;` |
|       - | 5859 | `	sxi32 rc;` |
|       - | 5860 | `	int keep;` |
|       - | 5861 | `	sxu32 n;` |
|      20 | 5862 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5863 | `		/* Invalid arguments,return NULL */` |
|       5 | 5864 | `		ph7_result_null(pCtx);` |
|       5 | 5865 | `		return PH7_OK;` |
|       - | 5866 | `	}` |
|       - | 5867 | `	/* Create a new array */` |
|      16 | 5868 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 5869 | `	if( pArray == 0 ){` |
|     ! 0 | 5870 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5871 | `		return PH7_OK;` |
|       - | 5872 | `	}` |
|       - | 5873 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 5874 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 5875 | `	pEntry = pMap->pFirst;` |
|      16 | 5876 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 5877 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5878 | `	/* Perform the requested operation */` |
|      66 | 5879 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5880 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 5881 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 5882 | `		if( pValue == 0 ){` |
|       - | 5883 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5884 | `			keep = FALSE;` |
|      54 | 5885 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5886 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 5887 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 5888 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 5889 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 5890 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5891 | `					int len;` |
|       3 | 5892 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 5893 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5894 | `						"TypeError",` |
|       - | 5895 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 5896 | `						zName` |
|       - | 5897 | `						);` |
|     ! 0 | 5898 | `				}else{` |
|     ! 0 | 5899 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5900 | `						"TypeError",` |
|       - | 5901 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 5902 | `						ph7_type_name(apArg[1])` |
|       - | 5903 | `						);` |
|       - | 5904 | `				}` |
|       - | 5905 | `			}` |
|      23 | 5906 | `			keep = FALSE;` |
|      23 | 5907 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 5908 | `			if( rc == SXRET_OK ){` |
|       - | 5909 | `				/* Perform a boolean cast */` |
|      23 | 5910 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 5911 | `			}` |
|      23 | 5912 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 5913 | `		}else{` |
|       - | 5914 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 5915 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 5916 | `			 * the case where the callback argument is missing entirely.` |
|       - | 5917 | `			 */` |
|      29 | 5918 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 5919 | `		}` |
|      51 | 5920 | `		if( keep ){` |
|       - | 5921 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 5922 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5923 | `		}` |
|       - | 5924 | `		/* Point to the next entry */` |
|      51 | 5925 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 5926 | `	}` |
|      13 | 5927 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5928 | `	return PH7_OK;` |
|      11 | 5929 |  |
|       - | 5930 | `/*` |
|       - | 5931 | ` * array array_map(callback $callback,array $arr1)` |
|       - | 5932 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5933 | ` * Parameters` |
|       - | 5934 | ` *  $callback` |
|       - | 5935 | ` *   Callback function to run for each element in each array.` |
|       - | 5936 | ` * $arr1` |
|       - | 5937 | ` *   An array to run through the callback function.` |
|       - | 5938 | ` * Return` |
|       - | 5939 | ` *  Returns an array containing all the elements of arr1 after applying` |
|       - | 5940 | ` *  the callback function to each one.` |
|       - | 5941 | ` * NOTE:` |
|       - | 5942 | ` *  array_map() passes only a single value to the callback.` |
|       - | 5943 | ` */` |
|      10 | 5944 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5945 |  |
|       - | 5946 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5947 | `	ph7_hashmap_node *pEntry;` |
|       - | 5948 | `	ph7_hashmap *pMap;` |
|       - | 5949 | `	sxu32 n;` |
|      11 | 5950 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 5951 | `		/* Invalid arguments,return NULL */` |
|       5 | 5952 | `		ph7_result_null(pCtx);` |
|       5 | 5953 | `		return PH7_OK;` |
|       - | 5954 | `	}` |
|       - | 5955 | `	/* Create a new array */` |
|       7 | 5956 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5957 | `	if( pArray == 0 ){` |
|     ! 0 | 5958 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5959 | `		return PH7_OK;` |
|       - | 5960 | `	}` |
|       - | 5961 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5962 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       7 | 5963 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       7 | 5964 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 5965 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 5966 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 5967 | `	/* Perform the requested operation */` |
|       7 | 5968 | `	pEntry = pMap->pFirst;` |
|      21 | 5969 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5970 | `		/* Extrcat the node value */` |
|      15 | 5971 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      15 | 5972 | `		if( pValue ){` |
|       - | 5973 | `			sxi32 rc;` |
|       - | 5974 | `			/* Invoke the supplied callback */` |
|      15 | 5975 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 5976 | `			/* Extract the node key */` |
|      15 | 5977 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      15 | 5978 | `			if( rc != SXRET_OK ){` |
|       - | 5979 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 5980 | `				ph7_array_add_elem(pArray,&sKey,pValue); /* Keep the same value */` |
|     ! 0 | 5981 | `			}else{` |
|       - | 5982 | `				/* Insert the callback return value */` |
|      15 | 5983 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 5984 | `			}` |
|      15 | 5985 | `			PH7_MemObjRelease(&sKey);` |
|      15 | 5986 | `			PH7_MemObjRelease(&sResult);` |
|       7 | 5987 | `		}` |
|       - | 5988 | `		/* Point to the next entry */` |
|      15 | 5989 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 5990 | `	}` |
|       7 | 5991 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 5992 | `	return PH7_OK;` |
|       6 | 5993 |  |
|       - | 5994 | `/*` |
|       - | 5995 | ` * value array_reduce(array $input,callback $function[, value $initial = NULL ])` |
|       - | 5996 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 5997 | ` * Parameters` |
|       - | 5998 | ` *  $input` |
|       - | 5999 | ` *   The input array.` |
|       - | 6000 | ` *  $function` |
|       - | 6001 | ` *  The callback function.` |
|       - | 6002 | ` * $initial` |
|       - | 6003 | ` *  If the optional initial is available, it will be used at the beginning` |
|       - | 6004 | ` *  of the process, or as a final result in case the array is empty.` |
|       - | 6005 | ` * Return` |
|       - | 6006 | ` *  Returns the resulting value.` |
|       - | 6007 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6008 | ` */` |
|       4 | 6009 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6010 |  |
|       - | 6011 | `	ph7_hashmap_node *pEntry;` |
|       - | 6012 | `	ph7_hashmap *pMap;` |
|       - | 6013 | `	ph7_value *pValue;` |
|       - | 6014 | `	ph7_value sResult;` |
|       - | 6015 | `	sxu32 n;` |
|       5 | 6016 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6017 | `		/* Invalid/Missing arguments,return NULL */` |
|     ! 0 | 6018 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6019 | `		return PH7_OK;` |
|       - | 6020 | `	}` |
|       - | 6021 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 6022 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6023 | `	/* Assume a NULL initial value */` |
|       5 | 6024 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       5 | 6025 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       5 | 6026 | `	if( nArg > 2 ){` |
|       - | 6027 | `		/* Set the initial value */` |
|       5 | 6028 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       2 | 6029 | `	}` |
|       - | 6030 | `	/* Perform the requested operation */` |
|       5 | 6031 | `	pEntry = pMap->pFirst;` |
|      19 | 6032 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6033 | `		/* Extract the node value */` |
|      15 | 6034 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6035 | `		/* Invoke the supplied callback */` |
|      15 | 6036 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6037 | `		/* Point to the next entry */` |
|      15 | 6038 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 6039 | `	}` |
|       5 | 6040 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       5 | 6041 | `	PH7_MemObjRelease(&sResult);` |
|       5 | 6042 | `	return PH7_OK;` |
|       3 | 6043 |  |
|       - | 6044 | `/*` |
|       - | 6045 | ` * bool array_walk(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6046 | ` *  Apply a user function to every member of an array.` |
|       - | 6047 | ` * Parameters` |
|       - | 6048 | ` *  $array` |
|       - | 6049 | ` *   The input array.` |
|       - | 6050 | ` * $funcname` |
|       - | 6051 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6052 | ` *  the first, and the key/index second.` |
|       - | 6053 | ` * Note:` |
|       - | 6054 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6055 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6056 | ` *  be made in the original array itself.` |
|       - | 6057 | ` * $userdata` |
|       - | 6058 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6059 | ` *  to the callback funcname.` |
|       - | 6060 | ` * Return` |
|       - | 6061 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6062 | ` */` |
|      12 | 6063 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6064 |  |
|       - | 6065 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6066 | `	ph7_hashmap_node *pEntry;` |
|       - | 6067 | `	ph7_hashmap *pMap;` |
|       - | 6068 | `	sxi32 rc;` |
|       - | 6069 | `	sxu32 n;` |
|      13 | 6070 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6071 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6072 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6073 | `		return PH7_OK;` |
|       - | 6074 | `	}` |
|      13 | 6075 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6076 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6077 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 6078 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      13 | 6079 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6080 | `	/* Perform the desired operation */` |
|      13 | 6081 | `	pEntry = pMap->pFirst;` |
|      41 | 6082 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6083 | `		/* Extract the node value */` |
|      29 | 6084 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      29 | 6085 | `		if( pValue ){` |
|       - | 6086 | `			/* Extract the entry key */` |
|      29 | 6087 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6088 | `			/* Invoke the supplied callback */` |
|      29 | 6089 | `			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      29 | 6090 | `			PH7_MemObjRelease(&sKey);` |
|      29 | 6091 | `			if( rc != SXRET_OK ){` |
|       - | 6092 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 6093 | `				ph7_result_bool(pCtx,0); /* return FALSE */` |
|     ! 0 | 6094 | `				return PH7_OK;` |
|       - | 6095 | `			}` |
|      14 | 6096 | `		}` |
|       - | 6097 | `		/* Point to the next entry */` |
|      29 | 6098 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6099 | `	}` |
|       - | 6100 | `	/* All done,return TRUE */` |
|      13 | 6101 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6102 | `	return PH7_OK;` |
|       7 | 6103 |  |
|       - | 6104 | `/*` |
|       - | 6105 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6106 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6107 | ` */` |
|       6 | 6108 | `static int HashmapWalkRecursive(` |
|       - | 6109 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6110 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6111 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6112 | `	int iNest             /* Nesting level */` |
|       - | 6113 | `	)` |
|       1 | 6114 |  |
|       - | 6115 | `	ph7_hashmap_node *pEntry;` |
|       - | 6116 | `	ph7_value *pValue,sKey;` |
|       - | 6117 | `	sxi32 rc;` |
|       - | 6118 | `	sxu32 n;` |
|       - | 6119 | `	/* Iterate throw hashmap entries */` |
|       7 | 6120 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 6121 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 6122 | `	pEntry = pMap->pFirst;` |
|      17 | 6123 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6124 | `		/* Extract the node value */` |
|      11 | 6125 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      11 | 6126 | `		if( pValue ){` |
|      11 | 6127 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 6128 | `				if( iNest < 32 ){` |
|       - | 6129 | `					/* Recurse */` |
|       5 | 6130 | `					iNest++;` |
|       5 | 6131 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|       5 | 6132 | `					iNest--;` |
|       2 | 6133 | `				}` |
|       3 | 6134 | `			}else{` |
|       - | 6135 | `				/* Extract the node key */` |
|       7 | 6136 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6137 | `				/* Invoke the supplied callback */` |
|       7 | 6138 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|       7 | 6139 | `				PH7_MemObjRelease(&sKey);` |
|       7 | 6140 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6141 | `					return rc;` |
|       - | 6142 | `				}` |
|       - | 6143 | `			}` |
|       5 | 6144 | `		}` |
|       - | 6145 | `		/* Point to the next entry */` |
|      11 | 6146 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 6147 | `	}` |
|       7 | 6148 | `	return SXRET_OK;` |
|       4 | 6149 |  |
|       - | 6150 | `/*` |
|       - | 6151 | ` * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6152 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6153 | ` * Parameters` |
|       - | 6154 | ` *  $array` |
|       - | 6155 | ` *   The input array.` |
|       - | 6156 | ` * $funcname` |
|       - | 6157 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6158 | ` *  the first, and the key/index second.` |
|       - | 6159 | ` * Note:` |
|       - | 6160 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6161 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6162 | ` *  be made in the original array itself.` |
|       - | 6163 | ` * $userdata` |
|       - | 6164 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6165 | ` *  to the callback funcname.` |
|       - | 6166 | ` * Return` |
|       - | 6167 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6168 | ` */` |
|       2 | 6169 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6170 |  |
|       - | 6171 | `	ph7_hashmap *pMap;` |
|       - | 6172 | `	sxi32 rc;` |
|       3 | 6173 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6174 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6175 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6176 | `		return PH7_OK;` |
|       - | 6177 | `	}` |
|       - | 6178 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 6179 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6180 | `	/* Perform the desired operation */` |
|       3 | 6181 | `	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6182 | `	/* All done */` |
|       3 | 6183 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|       3 | 6184 | `	return PH7_OK;` |
|       2 | 6185 |  |
|       - | 6186 | `/*` |
|       - | 6187 | ` * Table of hashmap functions.` |
|       - | 6188 | ` */` |
|       - | 6189 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6190 | `	{"count",             ph7_hashmap_count },` |
|       - | 6191 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6192 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6193 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6194 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6195 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6196 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6197 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6198 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6199 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6200 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6201 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6202 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6203 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6204 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6205 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6206 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6207 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6208 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6209 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6210 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6211 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6212 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6213 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6214 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6215 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6216 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6217 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6218 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6219 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6220 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6221 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6222 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6223 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6224 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6225 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6226 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6227 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6228 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6229 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6230 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6231 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6232 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6233 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6234 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6235 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6236 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6237 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6238 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6239 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6240 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6241 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6242 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6243 | `	{"current",           ph7_hashmap_current },` |
|       - | 6244 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6245 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6246 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6247 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6248 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6249 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6250 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6251 | `};` |
|       - | 6252 | `/*` |
|       - | 6253 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6254 | ` */` |
|    1372 | 6255 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6256 |  |
|       - | 6257 | `	sxu32 n;` |
|   85066 | 6258 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|   83694 | 6259 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   41848 | 6260 | `	}` |
|    1374 | 6261 |  |
|       - | 6262 | `/*` |
|       - | 6263 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6264 | ` * the BLOB given as the first argument.` |
|       - | 6265 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6266 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6267 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6268 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6269 | ` */` |
|      28 | 6270 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6271 |  |
|       - | 6272 | `	ph7_hashmap_node *pEntry;` |
|       - | 6273 | `	ph7_value *pObj;` |
|      30 | 6274 | `	sxu32 n = 0;` |
|       - | 6275 | `	int isRef;` |
|       - | 6276 | `	sxi32 rc;` |
|       - | 6277 | `	int i;` |
|      30 | 6278 | `	if( nDepth > 31 ){` |
|       - | 6279 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6280 | `		/* Nesting limit reached */` |
|     ! 0 | 6281 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6282 | `		if( ShowType ){` |
|     ! 0 | 6283 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6284 | `		}` |
|     ! 0 | 6285 | `		return SXERR_LIMIT;` |
|       - | 6286 | `	}` |
|       - | 6287 | `	/* Point to the first inserted entry */` |
|      30 | 6288 | `	pEntry = pMap->pFirst;` |
|      30 | 6289 | `	rc = SXRET_OK;` |
|      30 | 6290 | `	if( !ShowType ){` |
|      15 | 6291 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6292 | `	}` |
|       - | 6293 | `	/* Total entries */` |
|      30 | 6294 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6295 | `#ifdef __WINNT__` |
|       2 | 6296 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6297 | `#else` |
|      28 | 6298 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6299 | `#endif` |
|      65 | 6300 | `	for(;;){` |
|     132 | 6301 | `		if( n >= pMap->nEntry ){` |
|      30 | 6302 | `			break;` |
|       - | 6303 | `		}` |
|     206 | 6304 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     104 | 6305 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      53 | 6306 | `		}` |
|       - | 6307 | `		/* Dump key */` |
|     104 | 6308 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      37 | 6309 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      19 | 6310 | `		}else{` |
|     101 | 6311 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6312 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6313 | `		}` |
|       - | 6314 | `#ifdef __WINNT__` |
|       2 | 6315 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6316 | `#else` |
|     102 | 6317 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6318 | `#endif` |
|       - | 6319 | `		/* Dump node value */` |
|     104 | 6320 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     104 | 6321 | `		isRef = 0;` |
|     104 | 6322 | `		if( pObj ){` |
|     104 | 6323 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6324 | `				/* Referenced object */` |
|     ! 0 | 6325 | `				isRef = 1;` |
|     ! 0 | 6326 | `			}` |
|     104 | 6327 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     104 | 6328 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6329 | `				break;` |
|       - | 6330 | `			}` |
|      51 | 6331 | `		}` |
|       - | 6332 | `		/* Point to the next entry */` |
|     104 | 6333 | `		n++;` |
|     104 | 6334 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6335 | `	}` |
|      58 | 6336 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      30 | 6337 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      16 | 6338 | `	}` |
|      30 | 6339 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      30 | 6340 | `	return rc;` |
|      16 | 6341 |  |
|       - | 6342 | `/*` |
|       - | 6343 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6344 | ` * retrieved entry.` |
|       - | 6345 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6346 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6347 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6348 | ` * a value different from PH7_OK.` |
|       - | 6349 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6350 | ` */` |
|   19568 | 6351 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6352 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6353 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6354 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6355 | `	)` |
|       2 | 6356 |  |
|       - | 6357 | `	ph7_hashmap_node *pEntry;` |
|       - | 6358 | `	ph7_value sKey,sValue;` |
|       - | 6359 | `	sxi32 rc;` |
|       - | 6360 | `	sxu32 n;` |
|       - | 6361 | `	/* Initialize walker parameter */` |
|   19570 | 6362 | `	rc = SXRET_OK;` |
|   19570 | 6363 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   19570 | 6364 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   19570 | 6365 | `	n = pMap->nEntry;` |
|   19570 | 6366 | `	pEntry = pMap->pFirst;` |
|       - | 6367 | `	/* Start the iteration process */` |
|   51940 | 6368 | `	for(;;){` |
|  103882 | 6369 | `		if( n < 1 ){` |
|   19570 | 6370 | `			break;` |
|       - | 6371 | `		}` |
|       - | 6372 | `		/* Extract a copy of the key and a copy the current value */` |
|   84314 | 6373 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   84314 | 6374 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6375 | `		/* Invoke the user callback */` |
|   84314 | 6376 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6377 | `		/* Release the copy of the key and the value */` |
|   84314 | 6378 | `		PH7_MemObjRelease(&sKey);` |
|   84314 | 6379 | `		PH7_MemObjRelease(&sValue);` |
|   84314 | 6380 | `		if( rc != PH7_OK ){` |
|       - | 6381 | `			/* Callback request an operation abort */` |
|     ! 0 | 6382 | `			return SXERR_ABORT;` |
|       - | 6383 | `		}` |
|       - | 6384 | `		/* Point to the next entry */` |
|   84314 | 6385 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   84314 | 6386 | `		n--;` |
|       2 | 6387 | `	}` |
|       - | 6388 | `	/* All done */` |
|   19570 | 6389 | `	return SXRET_OK;` |
|    9786 | 6390 |  |
|       - | 6391 |  |
