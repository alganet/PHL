# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2546/3110 lines (81.86%)

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
| 2742676 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2742678 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  216970 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  216972 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  216972 |   29 | `	sxu32 nH = 5381;` |
|  216972 |   30 | `	zEnd = &zIn[nLen];` |
|  250239 |   31 | `	for(;;){` |
|  500480 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  450394 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  407906 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  329570 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  216972 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecurisve is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * If the nesting limit is reached,this function abort immediately.` |
|       - |   43 | ` */` |
|     742 |   44 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)` |
|       2 |   45 |  |
|     744 |   46 | `	sxi64 iCount = 0;` |
|     744 |   47 | `	if( !bRecursive ){` |
|     468 |   48 | `		iCount = pMap->nEntry;` |
|     235 |   49 | `	}else{` |
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
|     740 |   79 | `	return iCount;` |
|     373 |   80 |  |
|       - |   81 | `/*` |
|       - |   82 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   83 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   84 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   85 | ` */` |
| 2688928 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   87 |  |
|       - |   88 | `	ph7_hashmap_node *pNode;` |
|       - |   89 | `	/* Allocate a new node */` |
| 2688930 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2688930 |   91 | `	if( pNode == 0 ){` |
|     ! 0 |   92 | `		return 0;` |
|       - |   93 | `	}` |
|       - |   94 | `	/* Zero the stucture */` |
| 2688930 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |   96 | `	/* Fill in the structure */` |
| 2688930 |   97 | `	pNode->pMap  = &(*pMap);` |
| 2688930 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2688930 |   99 | `	pNode->nHash = nHash;` |
| 2688930 |  100 | `	pNode->xKey.iKey = iKey;` |
| 2688930 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 2688930 |  102 | `	return pNode;` |
| 1344466 |  103 |  |
|       - |  104 | `/*` |
|       - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  108 | ` */` |
|   75630 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  110 |  |
|       - |  111 | `	ph7_hashmap_node *pNode;` |
|       - |  112 | `	/* Allocate a new node */` |
|   75632 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   75632 |  114 | `	if( pNode == 0 ){` |
|     ! 0 |  115 | `		return 0;` |
|       - |  116 | `	}` |
|       - |  117 | `	/* Zero the stucture */` |
|   75632 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  119 | `	/* Fill in the structure */` |
|   75632 |  120 | `	pNode->pMap  = &(*pMap);` |
|   75632 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   75632 |  122 | `	pNode->nHash = nHash;` |
|   75632 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   75632 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   75632 |  125 | `	pNode->nValIdx = nValIdx;` |
|   75632 |  126 | `	return pNode;` |
|   37817 |  127 |  |
|       - |  128 | `/*` |
|       - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  130 | ` */` |
| 2764558 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  132 |  |
|       - |  133 | `	/* Link */` |
| 2764560 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2557836 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2557836 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1278917 |  137 | `	}` |
| 2764560 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  139 | `	/* Link to the map list */` |
| 2764560 |  140 | `	if( pMap->pFirst == 0 ){` |
|   34350 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  142 | `		/* Point to the first inserted node */` |
|   34350 |  143 | `		pMap->pCur = pNode;` |
|   17176 |  144 | `	}else{` |
| 2730212 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  146 | `	}` |
| 2764560 |  147 | `	++pMap->nEntry;` |
| 2764560 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Unlink a node from the hashmap.` |
|       - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  152 | ` */` |
|    5242 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  154 |  |
|    5244 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5244 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  157 | `	/* Unlink from the corresponding bucket */` |
|    5244 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|    4820 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2411 |  160 | `	}else{` |
|     425 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  162 | `	}` |
|    5244 |  163 | `	if( pNode->pNextCollide ){` |
|    4039 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2019 |  165 | `	}` |
|    5244 |  166 | `	if( pMap->pFirst == pNode ){` |
|      58 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|      28 |  168 | `	}` |
|    5244 |  169 | `	if( pMap->pCur == pNode ){` |
|       - |  170 | `		/* Advance the node cursor */` |
|      60 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      29 |  172 | `	}` |
|       - |  173 | `	/* Unlink from the map list */` |
|    5244 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5244 |  175 | `	if( bRestore ){` |
|       - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|      30 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  178 | `		/* Restore to the freelist */` |
|      30 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|      30 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      14 |  181 | `		}` |
|      14 |  182 | `	}` |
|    5244 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5195 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2597 |  185 | `	}` |
|    5244 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5244 |  187 | `	pMap->nEntry--;` |
|    5244 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  189 | `		/* Free the hash-bucket */` |
|      26 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      26 |  191 | `		pMap->apBucket = 0;` |
|      26 |  192 | `		pMap->nSize = 0;` |
|      26 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      12 |  194 | `	}` |
|    5244 |  195 |  |
|       - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  197 | `/*` |
|       - |  198 | ` * Grow the hash-table and rehash all entries.` |
|       - |  199 | ` */` |
| 2764558 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  201 |  |
| 2764560 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   37812 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   37812 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  206 | `		sxu32 nBucket;` |
|       - |  207 | `		sxu32 n;` |
|   37812 |  208 | `		if( nNew < 1 ){` |
|   34350 |  209 | `			nNew = 16;` |
|   17174 |  210 | `		}` |
|       - |  211 | `		/* Allocate a new bucket */` |
|   37812 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   37812 |  213 | `		if( apNew == 0 ){` |
|     ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|       - |  216 | `			}` |
|       - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  218 | `			return SXRET_OK;` |
|       - |  219 | `		}` |
|       - |  220 | `		/* Zero the table */` |
|   37812 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  222 | `		/* Reflect the change */` |
|   37812 |  223 | `		pMap->apBucket = apNew;` |
|   37812 |  224 | `		pMap->nSize = nNew;` |
|   37812 |  225 | `		if( apOld == 0 ){` |
|       - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   34350 |  227 | `			return SXRET_OK;` |
|       - |  228 | `		}` |
|       - |  229 | `		/* Rehash old entries */` |
|    3464 |  230 | `		pEntry = pMap->pFirst;` |
|    3464 |  231 | `		n = 0;` |
| 1893123 |  232 | `		for( ;; ){` |
| 3786248 |  233 | `			if( n >= pMap->nEntry ){` |
|    3464 |  234 | `				break;` |
|       - |  235 | `			}` |
|       - |  236 | `			/* Clear the old collision link */` |
| 3782786 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  238 | `			/* Link to the new bucket */` |
| 3782786 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3782786 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3366692 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3366692 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1683345 |  243 | `			}` |
| 3782786 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  245 | `			/* Point to the next entry */` |
| 3782786 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3782786 |  247 | `			n++;` |
|       2 |  248 | `		}` |
|       - |  249 | `		/* Free the old table */` |
|    3464 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1731 |  251 | `	}` |
| 2730212 |  252 | `	return SXRET_OK;` |
| 1382281 |  253 |  |
|       - |  254 | `/*` |
|       - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  256 | ` * hashmap.` |
|       - |  257 | ` */` |
| 2688928 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  259 |  |
|       - |  260 | `	ph7_hashmap_node *pNode;` |
|       - |  261 | `	sxu32 nIdx;` |
|       - |  262 | `	sxu32 nHash;` |
|       - |  263 | `	sxi32 rc;` |
| 2688930 |  264 | `	if( !isForeign ){` |
|       - |  265 | `		ph7_value *pObj;` |
|       - |  266 | `		/* Reserve a ph7_value for the value */` |
| 2688906 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2688906 |  268 | `		if( pObj == 0 ){` |
|     ! 0 |  269 | `			return SXERR_MEM;` |
|       - |  270 | `		}` |
| 2688906 |  271 | `		if( pValue ){` |
|       - |  272 | `			/* Duplicate the value */` |
| 2688906 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 1344452 |  274 | `		}` |
| 2688906 |  275 | `		nIdx = pObj->nIdx;` |
| 1344454 |  276 | `	}else{` |
|      25 |  277 | `		nIdx = nRefIdx;` |
|       - |  278 | `	}` |
|       - |  279 | `	/* Hash the key */` |
| 2688930 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  281 | `	/* Allocate a new int node */` |
| 2688930 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2688930 |  283 | `	if( pNode == 0 ){` |
|     ! 0 |  284 | `		return SXERR_MEM;` |
|       - |  285 | `	}` |
| 2688930 |  286 | `	if( isForeign ){` |
|       - |  287 | `		/* Mark as a foregin entry */` |
|      25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      12 |  289 | `	}` |
|       - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2688930 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2688930 |  292 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  294 | `		return rc;` |
|       - |  295 | `	}` |
|       - |  296 | `	/* Perform the insertion */` |
| 2688930 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  298 | `	/* Install in the reference table */` |
| 2688930 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  300 | `	/* All done */` |
| 2688930 |  301 | `	return SXRET_OK;` |
| 1344466 |  302 |  |
|       - |  303 | `/*` |
|       - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  305 | ` * hashmap.` |
|       - |  306 | ` */` |
|   75630 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  308 |  |
|       - |  309 | `	ph7_hashmap_node *pNode;` |
|       - |  310 | `	sxu32 nHash;` |
|       - |  311 | `	sxu32 nIdx;` |
|       - |  312 | `	sxi32 rc;` |
|   75632 |  313 | `	if( !isForeign ){` |
|       - |  314 | `		ph7_value *pObj;` |
|       - |  315 | `		/* Reserve a ph7_value for the value */` |
|   56978 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   56978 |  317 | `		if( pObj == 0 ){` |
|     ! 0 |  318 | `			return SXERR_MEM;` |
|       - |  319 | `		}` |
|   56978 |  320 | `		if( pValue ){` |
|       - |  321 | `			/* Duplicate the value */` |
|   56978 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|   28488 |  323 | `		}` |
|   56978 |  324 | `		nIdx = pObj->nIdx;` |
|   28490 |  325 | `	}else{` |
|   18656 |  326 | `		nIdx = nRefIdx;` |
|       - |  327 | `	}` |
|       - |  328 | `	/* Hash the key */` |
|   75632 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  330 | `	/* Allocate a new blob node */` |
|   75632 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   75632 |  332 | `	if( pNode == 0 ){` |
|     ! 0 |  333 | `		return SXERR_MEM;` |
|       - |  334 | `	}` |
|   75632 |  335 | `	if( isForeign ){` |
|       - |  336 | `		/* Mark as a foregin entry */` |
|   18656 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|    9327 |  338 | `	}` |
|       - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   75632 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   75632 |  341 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  343 | `		return rc;` |
|       - |  344 | `	}` |
|       - |  345 | `	/* Perform the insertion */` |
|   75632 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  347 | `	/* Install in the reference table */` |
|   75632 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  349 | `	/* All done */` |
|   75632 |  350 | `	return SXRET_OK;` |
|   37817 |  351 |  |
|       - |  352 | `/*` |
|       - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  356 | ` */` |
|   46502 |  357 | `static sxi32 HashmapLookupIntKey(` |
|       - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  359 | `	sxi64 iKey,                /* lookup key */` |
|       - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  361 | `	)` |
|       2 |  362 |  |
|       - |  363 | `	ph7_hashmap_node *pNode;` |
|       - |  364 | `	sxu32 nHash;` |
|   46504 |  365 | `	if( pMap->nEntry < 1 ){` |
|       - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|     341 |  367 | `		return SXERR_NOTFOUND;` |
|       - |  368 | `	}` |
|       - |  369 | `	/* Hash the key first */` |
|   46164 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  371 | `	/* Point to the appropriate bucket */` |
|   46164 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  373 | `	/* Perform the lookup */` |
|  411438 |  374 | `	for(;;){` |
|  822878 |  375 | `		if( pNode == 0 ){` |
|   45653 |  376 | `			break;` |
|       - |  377 | `		}` |
|  777479 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774206 |  379 | `			&& pNode->nHash == nHash` |
|  385851 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  381 | `				/* Node found */` |
|     512 |  382 | `				if( ppNode ){` |
|     504 |  383 | `					*ppNode = pNode;` |
|     251 |  384 | `				}` |
|     512 |  385 | `				return SXRET_OK;` |
|       - |  386 | `		}` |
|       - |  387 | `		/* Follow the collision link */` |
|  776715 |  388 | `		pNode = pNode->pNextCollide;` |
|       1 |  389 | `	}` |
|       - |  390 | `	/* No such entry */` |
|   45653 |  391 | `	return SXERR_NOTFOUND;` |
|   23253 |  392 |  |
|       - |  393 | `/*` |
|       - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  397 | ` */` |
|  148924 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  400 | `	const void *pKey,           /* Lookup key */` |
|       - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  403 | `	)` |
|       2 |  404 |  |
|       - |  405 | `	ph7_hashmap_node *pNode;` |
|       - |  406 | `	sxu32 nHash;` |
|  148926 |  407 | `	if( pMap->nEntry < 1 ){` |
|       - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|    7586 |  409 | `		return SXERR_NOTFOUND;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* Hash the key first */` |
|  141342 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  413 | `	/* Point to the appropriate bucket */` |
|  141342 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  415 | `	/* Perform the lookup */` |
|  142946 |  416 | `	for(;;){` |
|  285894 |  417 | `		if( pNode == 0 ){` |
|  106990 |  418 | `			break;` |
|       - |  419 | `		}` |
|  196080 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  177404 |  421 | `			&& pNode->nHash == nHash` |
|  105128 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   34354 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  424 | `				/* Node found */` |
|   34354 |  425 | `				if( ppNode ){` |
|   34342 |  426 | `					*ppNode = pNode;` |
|   17170 |  427 | `				}` |
|   34354 |  428 | `				return SXRET_OK;` |
|       - |  429 | `		}` |
|       - |  430 | `		/* Follow the collision link */` |
|  144554 |  431 | `		pNode = pNode->pNextCollide;` |
|       2 |  432 | `	}` |
|       - |  433 | `	/* No such entry */` |
|  106990 |  434 | `	return SXERR_NOTFOUND;` |
|   74464 |  435 |  |
|       - |  436 | `/*` |
|       - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  439 | ` */` |
|  149114 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  441 |  |
|  149116 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  149116 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  149116 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  445 | `		/* Octal not decimal number */` |
|       5 |  446 | `		return FALSE;` |
|       - |  447 | `	}` |
|  149112 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  449 | `		zIn++;` |
|     ! 0 |  450 | `	}` |
|   74888 |  451 | `	for(;;){` |
|  149778 |  452 | `		if( zIn >= zEnd ){` |
|     233 |  453 | `			return TRUE;` |
|       - |  454 | `		}` |
|  149546 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   74441 |  456 | `			break;` |
|       - |  457 | `		}` |
|     667 |  458 | `		zIn++;` |
|       1 |  459 | `	}` |
|       - |  460 | `	/* Key does not look like a decimal number */` |
|  148880 |  461 | `	return FALSE;` |
|   74559 |  462 |  |
|       - |  463 | `/*` |
|       - |  464 | ` * Check if a given key exists in the given hashmap.` |
|       - |  465 | ` * Write a pointer to the target node on success.` |
|       - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  467 | ` */` |
|   73764 |  468 | `static sxi32 HashmapLookup(` |
|       - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  472 | `	)` |
|       2 |  473 |  |
|   73766 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  475 | `	sxi32 rc;` |
|   73766 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   73320 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  478 | `			/* Force a string cast */` |
|     ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  480 | `		}` |
|   73320 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  482 | `			/* Perform a blob lookup */` |
|   73304 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   73304 |  484 | `			goto result;` |
|       - |  485 | `		}` |
|       8 |  486 | `	}` |
|       - |  487 | `	/* Perform an int lookup */` |
|     464 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  489 | `		/* Force an integer cast */` |
|      27 |  490 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  491 | `	}` |
|       - |  492 | `	/* Perform an int lookup */` |
|     464 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   36882 |  494 | `result:` |
|   73766 |  495 | `	if( rc == SXRET_OK ){` |
|       - |  496 | `		/* Node found */` |
|   34756 |  497 | `		if( ppNode ){` |
|   34732 |  498 | `			*ppNode = pNode;` |
|   17365 |  499 | `		}` |
|   34756 |  500 | `		return SXRET_OK;` |
|       - |  501 | `	}` |
|       - |  502 | `	/* No such entry */` |
|   39012 |  503 | `	return SXERR_NOTFOUND;` |
|   36884 |  504 |  |
|       - |  505 | `/*` |
|       - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  507 | ` * hashmap.` |
|       - |  508 | ` * If a node with the given key already exists in the database` |
|       - |  509 | ` * then this function overwrite the old value.` |
|       - |  510 | ` */` |
| 2745770 |  511 | `static sxi32 HashmapInsert(` |
|       - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  514 | `	ph7_value *pVal    /* Node value */` |
|       - |  515 | `	)` |
|       2 |  516 |  |
| 2745772 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 2745772 |  518 | `	sxi32 rc = SXRET_OK;` |
| 2745772 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   57176 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  521 | `			/* Force a string cast */` |
|       3 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  523 | `		}` |
|   57176 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  526 | `				/* Automatic index assign */` |
|      34 |  527 | `				pKey = 0;` |
|      16 |  528 | `			}` |
|     256 |  529 | `			goto IntKey;` |
|       - |  530 | `		}` |
|   85382 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   28460 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|   56900 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  547 | `			/* Forbidden */` |
|       3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  549 | `			return SXRET_OK;` |
|       - |  550 | `		}` |
|       - |  551 | `		/* Perform a blob-key insertion */` |
|   56898 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   56898 |  553 | `		return rc;` |
|       - |  554 | `	}` |
| 1344298 |  555 | `IntKey:` |
| 2688852 |  556 | `	if( pKey ){` |
|   23135 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  558 | `			/* Force an integer cast */` |
|     251 |  559 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  560 | `		}` |
|   23135 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   23099 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  576 | `			/* Forbidden */` |
|       3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  578 | `			return SXRET_OK;` |
|       - |  579 | `		}` |
|       - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|   23097 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23097 |  582 | `		if( rc == SXRET_OK ){` |
|   23097 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  584 | `				/* Increment the automatic index */` |
|   22869 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  586 | `				/* Make sure the automatic index is not reserved */` |
|   22869 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  588 | `					pMap->iNextIdx++;` |
|     ! 0 |  589 | `				}` |
|   11434 |  590 | `			}` |
|   11548 |  591 | `		}` |
|   11549 |  592 | `	}else{` |
| 2665718 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  594 | `			/* Forbidden */` |
|       3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  596 | `			return SXRET_OK;` |
|       - |  597 | `		}` |
|       - |  598 | `		/* Assign an automatic index */` |
| 2665716 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2665716 |  600 | `		if( rc == SXRET_OK ){` |
| 2665716 |  601 | `			++pMap->iNextIdx;` |
| 1332857 |  602 | `		}` |
|       - |  603 | `	}` |
|       - |  604 | `	/* Insertion result */` |
| 2688812 |  605 | `	return rc;` |
| 1372887 |  606 |  |
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
|   18684 |  634 | `static sxi32 HashmapInsertByRef(` |
|       - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  638 | `	)` |
|       2 |  639 |  |
|   18686 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|   18686 |  641 | `	sxi32 rc = SXRET_OK;` |
|   18686 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   18662 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  644 | `			/* Force a string cast */` |
|     ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  646 | `		}` |
|   18662 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  649 | `				/* Automatic index assign */` |
|     ! 0 |  650 | `				pKey = 0;` |
|     ! 0 |  651 | `			}` |
|     ! 0 |  652 | `			goto IntKey;` |
|       - |  653 | `		}` |
|   27992 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    9330 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  656 | `				/* Overwrite */` |
|       7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|       - |  659 | `				/* Install in the reference table */` |
|       7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  661 | `				return SXRET_OK;` |
|       - |  662 | `		}` |
|       - |  663 | `		/* Perform a blob-key insertion */` |
|   18656 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   18656 |  665 | `		return rc;` |
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
|    9344 |  702 |  |
|       - |  703 | `/*` |
|       - |  704 | ` * Extract node value.` |
|       - |  705 | ` */` |
|  792175 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  707 |  |
|       - |  708 | `	/* Point to the desired object */` |
|       - |  709 | `	ph7_value *pObj;` |
|  792177 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  792177 |  711 | `	return pObj;` |
|       2 |  712 |  |
|       - |  713 | `/*` |
|       - |  714 | ` * Insert a node in the given hashmap.` |
|       - |  715 | ` * If a node with the given key already exists in the database` |
|       - |  716 | ` * then this function overwrite the old value.` |
|       - |  717 | ` */` |
|     234 |  718 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       1 |  719 |  |
|       - |  720 | `	ph7_value *pObj;` |
|       - |  721 | `	sxi32 rc;` |
|       - |  722 | `	/* Extract the node value */` |
|     235 |  723 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     235 |  724 | `	if( pObj == 0 ){` |
|     ! 0 |  725 | `		return SXERR_EMPTY;` |
|       - |  726 | `	}` |
|       - |  727 | `	/* Preserve key */` |
|     235 |  728 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  729 | `		/* Int64 key */` |
|     125 |  730 | `		if( !bPreserve ){` |
|       - |  731 | `			/* Assign an automatic index */` |
|      55 |  732 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      28 |  733 | `		}else{` |
|      71 |  734 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  735 | `		}` |
|      63 |  736 | `	}else{` |
|       - |  737 | `		/* Blob key */` |
|     111 |  738 | `		if( !bPreserve ){` |
|       - |  739 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  740 | `			 * original string key entirely */` |
|      33 |  741 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      17 |  742 | `		}else{` |
|     118 |  743 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      39 |  744 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  745 | `		}` |
|       - |  746 | `	}` |
|     235 |  747 | `	return rc;` |
|     118 |  748 |  |
|       - |  749 | `/*` |
|       - |  750 | ` * Compare two node values.` |
|       - |  751 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  752 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  753 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  754 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  755 | ` * documenation.` |
|       - |  756 | ` */` |
|   34720 |  757 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  758 |  |
|       - |  759 | `	ph7_value sObj1,sObj2;` |
|       - |  760 | `	sxi32 rc;` |
|   34722 |  761 | `	if( pLeft == pRight ){` |
|       - |  762 | `		/*` |
|       - |  763 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  764 | `		 * below for more information on this sceanario.` |
|       - |  765 | `		 */` |
|     ! 0 |  766 | `		return 0;` |
|       - |  767 | `	}` |
|       - |  768 | `	/* Do the comparison */` |
|   34722 |  769 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   34722 |  770 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   34722 |  771 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   34722 |  772 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   34722 |  773 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   34722 |  774 | `	PH7_MemObjRelease(&sObj1);` |
|   34722 |  775 | `	PH7_MemObjRelease(&sObj2);` |
|   34722 |  776 | `	return rc;` |
|   17394 |  777 |  |
|       - |  778 | `/*` |
|       - |  779 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  780 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  781 | ` */` |
|    7586 |  782 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  783 |  |
|    7588 |  784 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  785 | `	sxu32 nBucket;` |
|       - |  786 | `	/* Remove old collision links */` |
|    7588 |  787 | `	if( pEntry->pPrevCollide ){` |
|    6075 |  788 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    3035 |  789 | `	}else{` |
|    1515 |  790 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  791 | `	}` |
|    7588 |  792 | `	if( pEntry->pNextCollide ){` |
|     633 |  793 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     313 |  794 | `	}` |
|    7588 |  795 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  796 | `	/* Compute the new hash */` |
|    7588 |  797 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    7588 |  798 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    7588 |  799 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  800 | `	/* Link to the new bucket */` |
|    7588 |  801 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7588 |  802 | `	if( pMap->apBucket[nBucket] ){` |
|    6240 |  803 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3120 |  804 | `	}` |
|    7588 |  805 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7588 |  806 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  807 | `	/* Increment the automatic index */` |
|    7588 |  808 | `	pMap->iNextIdx++;` |
|    7588 |  809 |  |
|       - |  810 | `/*` |
|       - |  811 | ` * Perform a linear search on a given hashmap.` |
|       - |  812 | ` * Write a pointer to the target node on success.` |
|       - |  813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  814 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  815 | ` * for more information.` |
|       - |  816 | ` */` |
|   19262 |  817 | `static int HashmapFindValue(` |
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
|   19264 |  830 | `	pEntry = pMap->pFirst;` |
|   19264 |  831 | `	n = pMap->nEntry;` |
|   19264 |  832 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   19264 |  833 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   46172 |  834 | `	for(;;){` |
|   92345 |  835 | `		if( n < 1 ){` |
|      43 |  836 | `			break;` |
|       - |  837 | `		}` |
|       - |  838 | `		/* Extract node value */` |
|   92303 |  839 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|   92303 |  840 | `		if( pVal ){` |
|   92303 |  841 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|   92303 |  853 | `				PH7_MemObjLoad(pVal,&sVal);` |
|   92303 |  854 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|   92303 |  855 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|   92303 |  856 | `				PH7_MemObjRelease(&sVal);` |
|   92303 |  857 | `				PH7_MemObjRelease(&sNeedle);` |
|   92303 |  858 | `				if( rc == 0 ){` |
|   19222 |  859 | `					if( ppNode ){` |
|     ! 0 |  860 | `						*ppNode = pEntry;` |
|     ! 0 |  861 | `					}` |
|       - |  862 | `					/* Match found*/` |
|   19222 |  863 | `					return SXRET_OK;` |
|       - |  864 | `				}` |
|       - |  865 | `			}` |
|   36541 |  866 | `		}` |
|       - |  867 | `		/* Point to the next entry */` |
|   73083 |  868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   73083 |  869 | `		n--;` |
|       2 |  870 | `	}` |
|       - |  871 | `	/* No such entry */` |
|      43 |  872 | `	return SXERR_NOTFOUND;` |
|    9633 |  873 |  |
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
|  366490 | 1047 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1048 | `	ph7_hashmap *pDest,` |
|       - | 1049 | `	ph7_hashmap_node *pEntry,` |
|       - | 1050 | `	ph7_value *pVal,` |
|       - | 1051 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1052 | `	)` |
|       2 | 1053 |  |
|  366492 | 1054 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1055 | `	ph7_value sKey;` |
|       - | 1056 | `	sxi32 rc;` |
|       - | 1057 |  |
|  366492 | 1058 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1059 | `		/* Blob key insertion */` |
|      19 | 1060 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      19 | 1061 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      19 | 1062 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      19 | 1063 | `		PH7_MemObjRelease(&sKey);` |
|      10 | 1064 | `	}else{` |
|       - | 1065 | `		/* Int key */` |
|  366474 | 1066 | `		if( iAction == 0 ){ /* Merge */` |
|  366446 | 1067 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  183252 | 1068 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|       5 | 1069 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       5 | 1070 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|       5 | 1071 | `			PH7_MemObjRelease(&sKey);` |
|       3 | 1072 | `		}else{ /* Dup */` |
|      26 | 1073 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1074 | `		}` |
|       - | 1075 | `	}` |
|  366492 | 1076 | `	return rc;` |
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
|    1618 | 1090 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1091 |  |
|       - | 1092 | `	ph7_hashmap_node *pEntry;` |
|       - | 1093 | `	ph7_value *pVal;` |
|       - | 1094 | `	sxi32 rc;` |
|       - | 1095 | `	sxu32 n;` |
|    1620 | 1096 | `	if( pSrc == pDest ){` |
|       - | 1097 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1098 | `		 * Unlike the zend engine.` |
|       - | 1099 | `		 */` |
|     ! 0 | 1100 | `		return SXRET_OK;` |
|       - | 1101 | `	}` |
|       - | 1102 | `	/* Point to the first inserted entry in the source */` |
|    1620 | 1103 | `	pEntry = pSrc->pFirst;` |
|       - | 1104 | `	/* Perform the merge */` |
|  368070 | 1105 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1106 | `		/* Extract the node value */` |
|  366452 | 1107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  366452 | 1108 | `		if( pVal ){` |
|       - | 1109 | `			/* Make a local copy of the value.` |
|       - | 1110 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1111 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1112 | `			 * to the old pool.` |
|       - | 1113 | `			 */` |
|  366452 | 1114 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  183227 | 1115 | `		}else{` |
|     ! 0 | 1116 | `			rc = SXRET_OK;` |
|       - | 1117 | `		}` |
|  366452 | 1118 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1119 | `			return rc;` |
|       - | 1120 | `		}` |
|       - | 1121 | `		/* Point to the next entry */` |
|  366452 | 1122 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  183227 | 1123 | `	}` |
|    1620 | 1124 | `	return SXRET_OK;` |
|     811 | 1125 |  |
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
|   50404 | 1297 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1298 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1299 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1300 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1301 | `	)` |
|       2 | 1302 |  |
|       - | 1303 | `	ph7_hashmap *pMap;` |
|       - | 1304 | `	/* Allocate a new instance */` |
|   50406 | 1305 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   50406 | 1306 | `	if( pMap == 0 ){` |
|     ! 0 | 1307 | `		return 0;` |
|       - | 1308 | `	}` |
|       - | 1309 | `	/* Zero the structure */` |
|   50406 | 1310 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1311 | `	/* Fill in the structure */` |
|   50406 | 1312 | `	pMap->pVm = &(*pVm);` |
|   50406 | 1313 | `	pMap->iRef = 1;` |
|       - | 1314 | `	/* Default hash functions */` |
|   50406 | 1315 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   50406 | 1316 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   50406 | 1317 | `	return pMap;` |
|   25204 | 1318 |  |
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
|    1360 | 1339 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    1362 | 1359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1362 | 1360 | `	if( pMap == 0 ){` |
|     ! 0 | 1361 | `		return SXERR_MEM;` |
|       - | 1362 | `	}` |
|    1362 | 1363 | `	pVm->pGlobal = pMap;` |
|       - | 1364 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1362 | 1365 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1362 | 1366 | `	if( pObj == 0 ){` |
|     ! 0 | 1367 | `		return SXERR_MEM;` |
|       - | 1368 | `	}` |
|    1362 | 1369 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1370 | `	/* Record object index */` |
|    1362 | 1371 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1372 | `	/* Install the special $GLOBALS array */` |
|    1362 | 1373 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1362 | 1374 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1375 | `		return rc;` |
|       - | 1376 | `	}` |
|       - | 1377 | `	/* Install superglobals now */` |
|   14962 | 1378 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1379 | `		ph7_value *pSuper;` |
|       - | 1380 | `		/* Request an empty array */` |
|   13602 | 1381 | `		pSuper = ph7_new_array(&(*pVm));` |
|   13602 | 1382 | `		if( pSuper == 0 ){` |
|     ! 0 | 1383 | `			return SXERR_MEM;` |
|       - | 1384 | `		}` |
|       - | 1385 | `		/* Install */` |
|   13602 | 1386 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   13602 | 1387 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1388 | `			return rc;` |
|       - | 1389 | `		}` |
|       - | 1390 | `		/* Release the value now it have been installed */` |
|   13602 | 1391 | `		ph7_release_value(&(*pVm),pSuper);` |
|    6802 | 1392 | `	}` |
|       - | 1393 | `	/* Set some $_SERVER entries */` |
|    1362 | 1394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1395 | `	/*` |
|       - | 1396 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1397 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1398 | `	 */` |
|    2718 | 1399 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1400 | `		"SCRIPT_FILENAME",` |
|     680 | 1401 | `		pFile ? pFile->zString : ":Memory:",` |
|    1356 | 1402 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1403 | `		);` |
|       - | 1404 | `	/* All done,all super-global are installed now */` |
|    1362 | 1405 | `	return SXRET_OK;` |
|     682 | 1406 |  |
|       - | 1407 | `/*` |
|       - | 1408 | ` * Release a hashmap.` |
|       - | 1409 | ` */` |
|   35386 | 1410 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1411 |  |
|       - | 1412 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   35388 | 1413 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1414 | `	sxu32 n;` |
|   35388 | 1415 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1416 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1417 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1418 | `		return SXRET_OK;` |
|       - | 1419 | `	}` |
|       - | 1420 | `	/* Start the release process */` |
|   35388 | 1421 | `	n = 0;` |
|   35388 | 1422 | `	pEntry = pMap->pFirst;` |
| 1387775 | 1423 | `	for(;;){` |
| 2775552 | 1424 | `		if( n >= pMap->nEntry ){` |
|   35388 | 1425 | `			break;` |
|       - | 1426 | `		}` |
| 2740166 | 1427 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1428 | `		/* Remove the reference from the foreign table */` |
| 2740166 | 1429 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2740166 | 1430 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1431 | `			/* Restore the ph7_value to the free list */` |
| 2740158 | 1432 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1370078 | 1433 | `		}` |
|       - | 1434 | `		/* Release the node */` |
| 2740166 | 1435 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   54982 | 1436 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   27490 | 1437 | `		}` |
| 2740166 | 1438 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1439 | `		/* Point to the next entry */` |
| 2740166 | 1440 | `		pEntry = pNext;` |
| 2740166 | 1441 | `		n++;` |
|       2 | 1442 | `	}` |
|   35388 | 1443 | `	if( pMap->nEntry > 0 ){` |
|       - | 1444 | `		/* Release the hash bucket */` |
|   31544 | 1445 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   15771 | 1446 | `	}` |
|   35388 | 1447 | `	if( FreeDS ){` |
|       - | 1448 | `		/* Free the whole instance */` |
|   35386 | 1449 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   17694 | 1450 | `	}else{` |
|       - | 1451 | `		/* Keep the instance but reset it's fields */` |
|       3 | 1452 | `		pMap->apBucket = 0;` |
|       3 | 1453 | `		pMap->iNextIdx = 0;` |
|       3 | 1454 | `		pMap->nEntry = pMap->nSize = 0;` |
|       3 | 1455 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1456 | `	}` |
|   35388 | 1457 | `	return SXRET_OK;` |
|   17695 | 1458 |  |
|       - | 1459 | `/*` |
|       - | 1460 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1461 | ` * If the count reaches zero which mean no more variables` |
|       - | 1462 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1463 | ` */` |
|  413500 | 1464 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1465 |  |
|  413502 | 1466 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1467 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  413502 | 1468 | `	pMap->iRef--;` |
|  413502 | 1469 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   35386 | 1470 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   17692 | 1471 | `	}` |
|  413502 | 1472 |  |
|       - | 1473 | `/*` |
|       - | 1474 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1475 | ` * Write a pointer to the target node on success.` |
|       - | 1476 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1477 | ` */` |
|   73772 | 1478 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1479 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1480 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1481 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1482 | `	)` |
|       2 | 1483 |  |
|       - | 1484 | `	sxi32 rc;` |
|   73774 | 1485 | `	if( pMap->nEntry < 1 ){` |
|       - | 1486 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1487 | `		 */` |
|       9 | 1488 | `		return SXERR_NOTFOUND;` |
|       - | 1489 | `	}` |
|   73766 | 1490 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   73766 | 1491 | `	return rc;` |
|   36888 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1495 | ` * hashmap.` |
|       - | 1496 | ` * If a node with the given key already exists in the database` |
|       - | 1497 | ` * then this function overwrite the old value.` |
|       - | 1498 | ` */` |
| 2379240 | 1499 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1500 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1501 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1502 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1503 | `	)` |
|       2 | 1504 |  |
|       - | 1505 | `	sxi32 rc;` |
| 2379242 | 1506 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1507 | `		/*` |
|       - | 1508 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1509 | `		 */` |
|     ! 0 | 1510 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1511 | `		return SXRET_OK;` |
|       - | 1512 | `	}` |
| 2379242 | 1513 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2379242 | 1514 | `	return rc;` |
| 1189622 | 1515 |  |
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
|   18684 | 1543 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1544 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1545 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1546 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1547 | `	)` |
|       2 | 1548 |  |
|       - | 1549 | `	sxi32 rc;` |
|   18686 | 1550 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1551 | `		/*` |
|       - | 1552 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1553 | `		 */` |
|     ! 0 | 1554 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1555 | `		return SXRET_OK;` |
|       - | 1556 | `	}` |
|   18686 | 1557 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   18686 | 1558 | `	return rc;` |
|    9344 | 1559 |  |
|       - | 1560 | `/*` |
|       - | 1561 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1562 | ` */` |
|   15768 | 1563 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1564 |  |
|       - | 1565 | `	/* Reset the loop cursor */` |
|   15770 | 1566 | `	pMap->pCur = pMap->pFirst;` |
|   15770 | 1567 |  |
|       - | 1568 | `/*` |
|       - | 1569 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1570 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1571 | ` * return NULL.` |
|       - | 1572 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1573 | ` */` |
|  129864 | 1574 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1575 |  |
|  129866 | 1576 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  129866 | 1577 | `	if( pCur == 0 ){` |
|       - | 1578 | `		/* End of the list,return null */` |
|    7888 | 1579 | `		return 0;` |
|       - | 1580 | `	}` |
|       - | 1581 | `	/* Advance the node cursor */` |
|  121980 | 1582 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  121980 | 1583 | `	return pCur;` |
|   64934 | 1584 |  |
|       - | 1585 | `/*` |
|       - | 1586 | ` * Extract a node value.` |
|       - | 1587 | ` */` |
|  310008 | 1588 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1589 |  |
|  310010 | 1590 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  310010 | 1591 | `	if( pEntry ){` |
|  310010 | 1592 | `		if( bStore ){` |
|  122034 | 1593 | `			PH7_MemObjStore(pEntry,pValue);` |
|   61018 | 1594 | `		}else{` |
|  187978 | 1595 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1596 | `		}` |
|  155070 | 1597 | `	}else{` |
|     ! 0 | 1598 | `		PH7_MemObjRelease(pValue);` |
|       - | 1599 | `	}` |
|  310010 | 1600 |  |
|       - | 1601 | `/*` |
|       - | 1602 | ` * Extract a node key.` |
|       - | 1603 | ` */` |
|   84100 | 1604 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1605 |  |
|       - | 1606 | `	/* Fill with the current key */` |
|   84102 | 1607 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   83964 | 1608 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|       7 | 1609 | `			SyBlobRelease(&pKey->sBlob);` |
|       3 | 1610 | `		}` |
|   83964 | 1611 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   83964 | 1612 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   41983 | 1613 | `	}else{` |
|     139 | 1614 | `		SyBlobReset(&pKey->sBlob);` |
|     139 | 1615 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     139 | 1616 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1617 | `	}` |
|   84102 | 1618 |  |
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
|   22310 | 1666 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1667 |  |
|       - | 1668 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1669 | `    /* Prevent compiler warning */` |
|   22312 | 1670 | `	result.pNext = result.pPrev = 0;` |
|   22312 | 1671 | `	pTail = &result;` |
|   57074 | 1672 | `	while( pA && pB ){` |
|   34764 | 1673 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|   22779 | 1674 | `			pTail->pPrev = pA;` |
|   22779 | 1675 | `			pA->pNext = pTail;` |
|   22779 | 1676 | `			pTail = pA;` |
|   22779 | 1677 | `			pA = pA->pPrev;` |
|   11402 | 1678 | `		}else{` |
|   11987 | 1679 | `			pTail->pPrev = pB;` |
|   11987 | 1680 | `			pB->pNext = pTail;` |
|   11987 | 1681 | `			pTail = pB;` |
|   11987 | 1682 | `			pB = pB->pPrev;` |
|       - | 1683 | `		}` |
|       2 | 1684 | `	}` |
|   22312 | 1685 | `	if( pA ){` |
|   16561 | 1686 | `		pTail->pPrev = pA;` |
|   16561 | 1687 | `		pA->pNext = pTail;` |
|   14039 | 1688 | `	}else if( pB ){` |
|    5611 | 1689 | `		pTail->pPrev = pB;` |
|    5611 | 1690 | `		pB->pNext = pTail;` |
|    2800 | 1691 | `	}else{` |
|     144 | 1692 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1693 | `	}` |
|   22312 | 1694 | `	return result.pPrev;` |
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
|     502 | 1708 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1709 |  |
|       - | 1710 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1711 | `	sxu32 i;` |
|     504 | 1712 | `	SyZero(a,sizeof(a));` |
|       - | 1713 | `	/* Point to the first inserted entry */` |
|     504 | 1714 | `	pIn = pMap->pFirst;` |
|    8094 | 1715 | `	while( pIn ){` |
|    7592 | 1716 | `		p = pIn;` |
|    7592 | 1717 | `		pIn = p->pPrev;` |
|    7592 | 1718 | `		p->pPrev = 0;` |
|   14340 | 1719 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   14340 | 1720 | `			if( a[i]==0 ){` |
|    7592 | 1721 | `				a[i] = p;` |
|    7592 | 1722 | `				break;` |
|     ! 0 | 1723 | `			}else{` |
|    6750 | 1724 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    6750 | 1725 | `				a[i] = 0;` |
|       - | 1726 | `			}` |
|    3376 | 1727 | `		}` |
|    7592 | 1728 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1729 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1730 | `			 * But that is impossible.` |
|       - | 1731 | `			 */` |
|     ! 0 | 1732 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1733 | `		}` |
|       2 | 1734 | `	}` |
|     504 | 1735 | `	p = a[0];` |
|   16066 | 1736 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   15564 | 1737 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    7783 | 1738 | `	}` |
|     504 | 1739 | `	p->pNext = 0;` |
|       - | 1740 | `	/* Reflect the change */` |
|     504 | 1741 | `	pMap->pFirst = p;` |
|       - | 1742 | `	/* Reset the loop cursor */` |
|     504 | 1743 | `	pMap->pCur = pMap->pFirst;` |
|     504 | 1744 | `	return SXRET_OK;` |
|       2 | 1745 |  |
|       - | 1746 | `/*` |
|       - | 1747 | ` * Node comparison callback.` |
|       - | 1748 | ` * used-by: [sort(),asort(),...]` |
|       - | 1749 | ` */` |
|   34702 | 1750 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1751 |  |
|       - | 1752 | `	ph7_value sA,sB;` |
|       - | 1753 | `	sxi32 iFlags;` |
|       - | 1754 | `	int rc;` |
|   34704 | 1755 | `	if( pCmpData == 0 ){` |
|       - | 1756 | `		/* Perform a standard comparison */` |
|   34700 | 1757 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   34700 | 1758 | `		return rc;` |
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
|   17385 | 1784 |  |
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
|      12 | 1990 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1991 |  |
|       - | 1992 | `	sxu32 n;` |
|       6 | 1993 | `	SXUNUSED(pB); /* cc warning */` |
|       6 | 1994 | `	SXUNUSED(pCmpData);` |
|       - | 1995 | `	/* Grab a random number */` |
|      13 | 1996 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 1997 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 1998 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 1999 | `	 */` |
|      13 | 2000 | `	return n&1 ? 1 : -1;` |
|       1 | 2001 |  |
|       - | 2002 | `/*` |
|       - | 2003 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2004 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2005 | ` */` |
|     486 | 2006 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2007 |  |
|       - | 2008 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2009 | `	sxu32 i;` |
|       - | 2010 | `	/* Rehash all entries */` |
|     488 | 2011 | `	pLast = p = pMap->pFirst;` |
|     488 | 2012 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     488 | 2013 | `	i = 0;` |
|    4011 | 2014 | `	for( ;; ){` |
|    8024 | 2015 | `		if( i >= pMap->nEntry ){` |
|     488 | 2016 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     488 | 2017 | `			break;` |
|       - | 2018 | `		}` |
|    7538 | 2019 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2020 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2021 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2022 | `			/* Change key type */` |
|       5 | 2023 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2024 | `		}` |
|    7538 | 2025 | `		HashmapRehashIntNode(p);` |
|       - | 2026 | `		/* Point to the next entry */` |
|    7538 | 2027 | `		i++;` |
|    7538 | 2028 | `		pLast = p;` |
|    7538 | 2029 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2030 | `	}` |
|     488 | 2031 |  |
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
|     810 | 2053 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2054 |  |
|       - | 2055 | `	ph7_hashmap *pMap;` |
|       - | 2056 | `	/* Make sure we are dealing with a valid hashmap */` |
|     812 | 2057 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2060 | `		return PH7_OK;` |
|       - | 2061 | `	}` |
|       - | 2062 | `	/* Point to the internal representation of the input hashmap */` |
|     812 | 2063 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     812 | 2064 | `	if( pMap->nEntry > 1 ){` |
|     482 | 2065 | `		sxi32 iCmpFlags = 0;` |
|     482 | 2066 | `		if( nArg > 1 ){` |
|       - | 2067 | `			/* Extract comparison flags */` |
|       3 | 2068 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2069 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2070 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2071 | `			}` |
|       1 | 2072 | `		}` |
|       - | 2073 | `		/* Do the merge sort */` |
|     482 | 2074 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2075 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     482 | 2076 | `		HashmapSortRehash(pMap);` |
|     240 | 2077 | `	}` |
|       - | 2078 | `	/* All done,return TRUE */` |
|     812 | 2079 | `	ph7_result_bool(pCtx,1);` |
|     812 | 2080 | `	return PH7_OK;` |
|     407 | 2081 |  |
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
|      11 | 2476 | `		while(pMap->pLast->pPrev){` |
|       9 | 2477 | `			pMap->pLast = pMap->pLast->pPrev;` |
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
|     498 | 2498 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2499 |  |
|     500 | 2500 | `	int bRecursive = FALSE;` |
|       - | 2501 | `	sxi64 iCount;` |
|     500 | 2502 | `	if( nArg < 1 ){` |
|       - | 2503 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 2504 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 2505 | `		return PH7_OK;` |
|       - | 2506 | `	}` |
|     500 | 2507 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2508 | `		/* TICKET 1433-19: Handle objects */` |
|       3 | 2509 | `		int res = !ph7_value_is_null(apArg[0]);` |
|       3 | 2510 | `		ph7_result_int(pCtx,res);` |
|       3 | 2511 | `		return PH7_OK;` |
|       - | 2512 | `	}` |
|     498 | 2513 | `	if( nArg > 1 ){` |
|       - | 2514 | `		/* Recursive count? */` |
|      31 | 2515 | `		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;` |
|      15 | 2516 | `	}` |
|       - | 2517 | `	/* Count */` |
|     498 | 2518 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);` |
|     498 | 2519 | `	ph7_result_int64(pCtx,iCount);` |
|     498 | 2520 | `	return PH7_OK;` |
|     251 | 2521 |  |
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
|     120 | 3164 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|     122 | 3175 | `	if( nArg < 1 ){` |
|       - | 3176 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3177 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3178 | `			"ArgumentCountError",` |
|       - | 3179 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3180 | `			);` |
|       - | 3181 | `	}` |
|       - | 3182 | `	/* Make sure we are dealing with a valid hashmap */` |
|     120 | 3183 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3184 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3185 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3186 | `			"TypeError",` |
|       - | 3187 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3188 | `			ph7_type_name(apArg[0])` |
|       - | 3189 | `			);` |
|       - | 3190 | `	}` |
|       - | 3191 | `	/* Point to the internal representation of the input hashmap */` |
|     118 | 3192 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3193 | `	/* Create a new array */` |
|     118 | 3194 | `	pArray = ph7_context_new_array(pCtx);` |
|     118 | 3195 | `	if( pArray == 0 ){` |
|     ! 0 | 3196 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3197 | `		return PH7_OK;` |
|       - | 3198 | `	}` |
|     118 | 3199 | `	bStrict = FALSE;` |
|     118 | 3200 | `	if( nArg > 2 ){` |
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
|     115 | 3212 | `	pNode = pMap->pFirst;` |
|     115 | 3213 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     555 | 3214 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     441 | 3215 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     109 | 3216 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      55 | 3217 | `		}else{` |
|     333 | 3218 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     333 | 3219 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3220 | `		}` |
|     441 | 3221 | `		rc = 0;` |
|     441 | 3222 | `		if( nArg > 1 ){` |
|      31 | 3223 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3224 | `			if( pValue ){` |
|      31 | 3225 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3226 | `				/* Filter key */` |
|      31 | 3227 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3228 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3229 | `			}` |
|      15 | 3230 | `		}` |
|     441 | 3231 | `		if( rc == 0 ){` |
|       - | 3232 | `			/* Perform the insertion */` |
|     423 | 3233 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     211 | 3234 | `		}` |
|     441 | 3235 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3236 | `		/* Point to the next entry */` |
|     441 | 3237 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     221 | 3238 | `	}` |
|       - | 3239 | `	/* return the new array */` |
|     115 | 3240 | `	ph7_result_value(pCtx,pArray);` |
|     115 | 3241 | `	return PH7_OK;` |
|      62 | 3242 |  |
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
|     806 | 3286 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3287 |  |
|       - | 3288 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3289 | `	ph7_value *pArray;` |
|       - | 3290 | `	int i;` |
|     808 | 3291 | `	if( nArg < 1 ){` |
|       - | 3292 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3293 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3294 | `		return PH7_OK;` |
|       - | 3295 | `	}` |
|       - | 3296 | `	/* Create a new array */` |
|     808 | 3297 | `	pArray = ph7_context_new_array(pCtx);` |
|     808 | 3298 | `	if( pArray == 0 ){` |
|     ! 0 | 3299 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3300 | `		return PH7_OK;` |
|       - | 3301 | `	}` |
|       - | 3302 | `	/* Point to the internal representation of the hashmap */` |
|     808 | 3303 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3304 | `	/* Start merging */` |
|    2420 | 3305 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3306 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1614 | 3307 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3308 | `			/* Insert scalar value */` |
|       5 | 3309 | `			ph7_array_add_elem(pArray,0,apArg[i]);` |
|       3 | 3310 | `		}else{` |
|    1610 | 3311 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3312 | `			/* Merge the two hashmaps */` |
|    1610 | 3313 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3314 | `		}` |
|     808 | 3315 | `	}` |
|       - | 3316 | `	/* Return the freshly created array */` |
|     808 | 3317 | `	ph7_result_value(pCtx,pArray);` |
|     808 | 3318 | `	return PH7_OK;` |
|     405 | 3319 |  |
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
|   19182 | 3631 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3632 |  |
|       - | 3633 | `	ph7_value *pNeedle;` |
|       - | 3634 | `	int bStrict;` |
|       - | 3635 | `	int rc;` |
|   19184 | 3636 | `	if( nArg < 2 ){` |
|       - | 3637 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3638 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3639 | `		return PH7_OK;` |
|       - | 3640 | `	}` |
|   19184 | 3641 | `	pNeedle = apArg[0];` |
|   19184 | 3642 | `	bStrict = 0;` |
|   19184 | 3643 | `	if( nArg > 2 ){` |
|       5 | 3644 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3645 | `	}` |
|   19184 | 3646 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3647 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3648 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3649 | `		/* Set the comparison result */` |
|     ! 0 | 3650 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3651 | `		return PH7_OK;` |
|       - | 3652 | `	}` |
|       - | 3653 | `	/* Perform the lookup */` |
|   19184 | 3654 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3655 | `	/* Lookup result */` |
|   19184 | 3656 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   19184 | 3657 | `	return PH7_OK;` |
|    9593 | 3658 |  |
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
|      10 | 3768 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|      12 | 3780 | `	if( nArg < 1 ){` |
|       4 | 3781 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3782 | `			"ArgumentCountError",` |
|       - | 3783 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3784 | `			nArg` |
|       - | 3785 | `			);` |
|       - | 3786 | `	}` |
|      10 | 3787 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3788 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3789 | `			"TypeError",` |
|       - | 3790 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3791 | `			ph7_type_name(apArg[0])` |
|       - | 3792 | `			);` |
|       - | 3793 | `	}` |
|      14 | 3794 | `	for(i = 1 ; i < nArg ; i++){` |
|      10 | 3795 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3796 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3797 | `				"TypeError",` |
|       - | 3798 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3799 | `				i + 1,` |
|       2 | 3800 | `				ph7_type_name(apArg[i])` |
|       - | 3801 | `				);` |
|       - | 3802 | `		}` |
|       4 | 3803 | `	}` |
|       5 | 3804 | `	if( nArg == 1 ){` |
|       - | 3805 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3806 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3807 | `		return PH7_OK;` |
|       - | 3808 | `	}` |
|       - | 3809 | `	/* Create a new array */` |
|       5 | 3810 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 3811 | `	if( pArray == 0 ){` |
|     ! 0 | 3812 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3813 | `		return PH7_OK;` |
|       - | 3814 | `	}` |
|       - | 3815 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 3816 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3817 | `	/* Perform the diff */` |
|       5 | 3818 | `	pEntry = pSrc->pFirst;` |
|       5 | 3819 | `	n = pSrc->nEntry;` |
|       8 | 3820 | `	for(;;){` |
|      17 | 3821 | `		if( n < 1 ){` |
|       5 | 3822 | `			break;` |
|       - | 3823 | `		}` |
|       - | 3824 | `		/* Extract the node value */` |
|      13 | 3825 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      13 | 3826 | `		if( pVal ){` |
|      23 | 3827 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      17 | 3828 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3829 | `					/* ignore */` |
|     ! 0 | 3830 | `					continue;` |
|       - | 3831 | `				}` |
|       - | 3832 | `				/* Point to the internal representation of the hashmap */` |
|      17 | 3833 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3834 | `				/* Perform the lookup */` |
|      17 | 3835 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      17 | 3836 | `				if( rc == SXRET_OK ){` |
|       - | 3837 | `					/* Value exist */` |
|       7 | 3838 | `					break;` |
|       - | 3839 | `				}` |
|       6 | 3840 | `			}` |
|      13 | 3841 | `			if( i >= nArg ){` |
|       - | 3842 | `				/* Perform the insertion */` |
|       7 | 3843 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 3844 | `			}` |
|       6 | 3845 | `		}` |
|       - | 3846 | `		/* Point to the next entry */` |
|      13 | 3847 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 3848 | `		n--;` |
|       1 | 3849 | `	}` |
|       - | 3850 | `	/* Return the freshly created array */` |
|       5 | 3851 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 3852 | `	return PH7_OK;` |
|       7 | 3853 |  |
|       - | 3854 | `/*` |
|       - | 3855 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 3856 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 3857 | ` * Parameters` |
|       - | 3858 | ` *  $array1` |
|       - | 3859 | ` *    The array to compare from` |
|       - | 3860 | ` *  $array2` |
|       - | 3861 | ` *    An array to compare against` |
|       - | 3862 | ` *  $...` |
|       - | 3863 | ` *   More arrays to compare against.` |
|       - | 3864 | ` * $callback` |
|       - | 3865 | ` *  The callback comparison function.` |
|       - | 3866 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3867 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3868 | ` *  than the second.` |
|       - | 3869 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3870 | ` * Return` |
|       - | 3871 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3872 | ` *  are not present in any of the other arrays.` |
|       - | 3873 | ` */` |
|       2 | 3874 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3875 |  |
|       - | 3876 | `	ph7_hashmap_node *pEntry;` |
|       - | 3877 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3878 | `	ph7_value *pCallback;` |
|       - | 3879 | `	ph7_value *pArray;` |
|       - | 3880 | `	ph7_value *pVal;` |
|       - | 3881 | `	sxi32 rc;` |
|       - | 3882 | `	sxu32 n;` |
|       - | 3883 | `	int i;` |
|       3 | 3884 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3885 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3886 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3887 | `		return PH7_OK;` |
|       - | 3888 | `	}` |
|       - | 3889 | `	/* Point to the callback */` |
|       3 | 3890 | `	pCallback = apArg[nArg - 1];` |
|       3 | 3891 | `	if( nArg == 2 ){` |
|       - | 3892 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3893 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3894 | `		return PH7_OK;` |
|       - | 3895 | `	}` |
|       - | 3896 | `	/* Create a new array */` |
|       3 | 3897 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3898 | `	if( pArray == 0 ){` |
|     ! 0 | 3899 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3900 | `		return PH7_OK;` |
|       - | 3901 | `	}` |
|       - | 3902 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 3903 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3904 | `	/* Perform the diff */` |
|       3 | 3905 | `	pEntry = pSrc->pFirst;` |
|       3 | 3906 | `	n = pSrc->nEntry;` |
|       4 | 3907 | `	for(;;){` |
|       9 | 3908 | `		if( n < 1 ){` |
|       3 | 3909 | `			break;` |
|       - | 3910 | `		}` |
|       - | 3911 | `		/* Extract the node value */` |
|       7 | 3912 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 3913 | `		if( pVal ){` |
|      11 | 3914 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 3915 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3916 | `					/* ignore */` |
|     ! 0 | 3917 | `					continue;` |
|       - | 3918 | `				}` |
|       - | 3919 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 3920 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3921 | `				/* Perform the lookup */` |
|       7 | 3922 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 3923 | `				if( rc == SXRET_OK ){` |
|       - | 3924 | `					/* Value exist */` |
|       3 | 3925 | `					break;` |
|       - | 3926 | `				}` |
|       3 | 3927 | `			}` |
|       7 | 3928 | `			if( i >= (nArg - 1)){` |
|       - | 3929 | `				/* Perform the insertion */` |
|       5 | 3930 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 3931 | `			}` |
|       3 | 3932 | `		}` |
|       - | 3933 | `		/* Point to the next entry */` |
|       7 | 3934 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 3935 | `		n--;` |
|       1 | 3936 | `	}` |
|       - | 3937 | `	/* Return the freshly created array */` |
|       3 | 3938 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3939 | `	return PH7_OK;` |
|       2 | 3940 |  |
|       - | 3941 | `/*` |
|       - | 3942 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 3943 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 3944 | ` * Parameters` |
|       - | 3945 | ` *  $array1` |
|       - | 3946 | ` *    The array to compare from` |
|       - | 3947 | ` *  $array2` |
|       - | 3948 | ` *    An array to compare against` |
|       - | 3949 | ` *  $...` |
|       - | 3950 | ` *   More arrays to compare against` |
|       - | 3951 | ` * Return` |
|       - | 3952 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3953 | ` *  are not present in any of the other arrays.` |
|       - | 3954 | ` */` |
|      20 | 3955 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3956 |  |
|       - | 3957 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 3958 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3959 | `	ph7_value *pArray;` |
|       - | 3960 | `	ph7_value *pVal;` |
|       - | 3961 | `	sxi32 rc;` |
|       - | 3962 | `	sxu32 n;` |
|       - | 3963 | `	int i;` |
|       - | 3964 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 3965 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 3966 | `	 * accompanying integration tests to pass. */` |
|      22 | 3967 | `	if( nArg < 1 ){` |
|       4 | 3968 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3969 | `			"ArgumentCountError",` |
|       - | 3970 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 3971 | `			nArg` |
|       - | 3972 | `			);` |
|       - | 3973 | `	}` |
|      20 | 3974 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3975 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3976 | `			"TypeError",` |
|       - | 3977 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3978 | `			ph7_type_name(apArg[0])` |
|       - | 3979 | `			);` |
|       - | 3980 | `	}` |
|      32 | 3981 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3982 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 3983 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3984 | `				"TypeError",` |
|       - | 3985 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 3986 | `				i + 1,` |
|       4 | 3987 | `				ph7_type_name(apArg[i])` |
|       - | 3988 | `				);` |
|       - | 3989 | `		}` |
|       9 | 3990 | `	}` |
|      13 | 3991 | `	if( nArg == 1 ){` |
|       - | 3992 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3993 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3994 | `		return PH7_OK;` |
|       - | 3995 | `	}` |
|       - | 3996 | `	/* Create a new array */` |
|      11 | 3997 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 3998 | `	if( pArray == 0 ){` |
|     ! 0 | 3999 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4000 | `		return PH7_OK;` |
|       - | 4001 | `	}` |
|       - | 4002 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4003 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4004 | `	/* Perform the diff */` |
|      11 | 4005 | `	pEntry = pSrc->pFirst;` |
|      11 | 4006 | `	n = pSrc->nEntry;` |
|      11 | 4007 | `	pN1 = pN2 = 0;` |
|      29 | 4008 | `	for(;;){` |
|       - | 4009 | `		int keep;` |
|      35 | 4010 | `		if( n < 1 ){` |
|      11 | 4011 | `			break;` |
|       - | 4012 | `		}` |
|       - | 4013 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4014 | `		keep = 1;` |
|      41 | 4015 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4016 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4017 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4018 | `			/* Perform a key lookup first */` |
|      29 | 4019 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4020 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4021 | `			}else{` |
|      17 | 4022 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4023 | `			}` |
|      29 | 4024 | `			if( rc != SXRET_OK ){` |
|       - | 4025 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4026 | `				continue;` |
|       - | 4027 | `			}` |
|       - | 4028 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4029 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4030 | `			if( pVal ){` |
|       - | 4031 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4032 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4033 | `				if( pVal2 ){` |
|      15 | 4034 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4035 | `					if( cmp == 0 ){` |
|       - | 4036 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4037 | `						keep = 0;` |
|      13 | 4038 | `						break;` |
|       - | 4039 | `					}` |
|       1 | 4040 | `				}` |
|       1 | 4041 | `			}` |
|       2 | 4042 | `		}` |
|      25 | 4043 | `		if( keep ){` |
|       - | 4044 | `			/* Perform the insertion */` |
|      13 | 4045 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4046 | `		}` |
|       - | 4047 | `		/* Point to the next entry */` |
|      25 | 4048 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4049 | `		n--;` |
|       1 | 4050 | `	}` |
|       - | 4051 | `	/* Return the freshly created array */` |
|      11 | 4052 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4053 | `	return PH7_OK;` |
|      12 | 4054 |  |
|       - | 4055 | `/*` |
|       - | 4056 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4057 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4058 | ` *  by a user supplied callback function.` |
|       - | 4059 | ` * Parameters` |
|       - | 4060 | ` *  $array1` |
|       - | 4061 | ` *    The array to compare from` |
|       - | 4062 | ` *  $array2` |
|       - | 4063 | ` *    An array to compare against` |
|       - | 4064 | ` *  $...` |
|       - | 4065 | ` *   More arrays to compare against.` |
|       - | 4066 | ` *  $key_compare_func` |
|       - | 4067 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4068 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4069 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4070 | ` * Return` |
|       - | 4071 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4072 | ` *  are not present in any of the other arrays.` |
|       - | 4073 | ` */` |
|      22 | 4074 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4075 |  |
|       - | 4076 | `	ph7_hashmap_node *pEntry;` |
|       - | 4077 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4078 | `	ph7_value *pCallback;` |
|       - | 4079 | `	ph7_value *pArray;` |
|       - | 4080 | `	sxi32 rc;` |
|       - | 4081 | `	sxu32 n;` |
|       - | 4082 | `	int i;` |
|       - | 4083 |  |
|       - | 4084 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4085 | `	if( nArg < 2 ){` |
|       4 | 4086 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4087 | `			"ArgumentCountError",` |
|       - | 4088 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4089 | `			nArg` |
|       - | 4090 | `			);` |
|       - | 4091 | `	}` |
|      22 | 4092 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4093 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4094 | `			"TypeError",` |
|       - | 4095 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4096 | `			ph7_type_name(apArg[0])` |
|       - | 4097 | `			);` |
|       - | 4098 | `	}` |
|       - | 4099 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4100 | `	 * expected to be a callback. */` |
|      32 | 4101 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4102 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4103 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4104 | `				"TypeError",` |
|       - | 4105 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4106 | `				i + 1,` |
|       2 | 4107 | `				ph7_type_name(apArg[i])` |
|       - | 4108 | `				);` |
|       - | 4109 | `		}` |
|       8 | 4110 | `	}` |
|       - | 4111 | `	/* Point to the callback value */` |
|      18 | 4112 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4113 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4114 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4115 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4116 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4117 | `		 * string given" which we also reproduce. */` |
|       7 | 4118 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4119 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4120 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4121 | `				"TypeError",` |
|       - | 4122 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4123 | `				nArg` |
|       - | 4124 | `				);` |
|       - | 4125 | `		}` |
|       5 | 4126 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4127 | `			/* neither array nor string */` |
|       7 | 4128 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4129 | `				"TypeError",` |
|       - | 4130 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4131 | `				nArg` |
|       - | 4132 | `				);` |
|       - | 4133 | `		}` |
|       - | 4134 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4135 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4136 | `			"TypeError",` |
|       - | 4137 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4138 | `			nArg,` |
|     ! 0 | 4139 | `			ph7_type_name(pCallback)` |
|       - | 4140 | `			);` |
|       - | 4141 | `	}` |
|      11 | 4142 | `	if( nArg == 2 ){` |
|       - | 4143 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4144 | `		 * input array. */` |
|       3 | 4145 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4146 | `		return PH7_OK;` |
|       - | 4147 | `	}` |
|       - | 4148 | `	/* Create a new array */` |
|       9 | 4149 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4150 | `	if( pArray == 0 ){` |
|     ! 0 | 4151 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4152 | `		return PH7_OK;` |
|       - | 4153 | `	}` |
|       - | 4154 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4155 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4156 | `	/* Perform the diff */` |
|       9 | 4157 | `	pEntry = pSrc->pFirst;` |
|       9 | 4158 | `	n = pSrc->nEntry;` |
|      20 | 4159 | `	for(;;){` |
|       - | 4160 | `		int keep;` |
|      25 | 4161 | `		if( n < 1 ){` |
|       9 | 4162 | `			break;` |
|       - | 4163 | `		}` |
|      17 | 4164 | `		keep = 1;` |
|      29 | 4165 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4166 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4167 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4168 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4169 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4170 | `			while( pIt ){` |
|       - | 4171 | `				/* build temporary key values for callback */` |
|       - | 4172 | `				ph7_value key1, key2, result;` |
|       - | 4173 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4174 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4175 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4176 | `				}else{` |
|       - | 4177 | `					SyString sStr;` |
|      31 | 4178 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4179 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4180 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4181 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4182 | `				}` |
|      31 | 4183 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4184 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4185 | `				}else{` |
|       - | 4186 | `					SyString sStr;` |
|      31 | 4187 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4188 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4189 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4190 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4191 | `				}` |
|      31 | 4192 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4193 | `				/* call user callback with (key1, key2) */` |
|       - | 4194 | `				{` |
|       - | 4195 | `					ph7_value *apK[2];` |
|      31 | 4196 | `					apK[0] = &key1;` |
|      31 | 4197 | `					apK[1] = &key2;` |
|      31 | 4198 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4199 | `				}` |
|      31 | 4200 | `				if( rc == SXRET_OK ){` |
|      31 | 4201 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4202 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4203 | `					}` |
|      31 | 4204 | `					if( result.x.iVal == 0 ){` |
|       - | 4205 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4206 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4207 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4208 | `						if( pVal1 && pVal2 ){` |
|      13 | 4209 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4210 | `								keep = 0;` |
|       9 | 4211 | `								PH7_MemObjRelease(&result);` |
|       - | 4212 | `								/* release keys too before breaking */` |
|       9 | 4213 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4214 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4215 | `								break;` |
|       - | 4216 | `							}` |
|       2 | 4217 | `						}` |
|       2 | 4218 | `					}` |
|      11 | 4219 | `				}` |
|      23 | 4220 | `				PH7_MemObjRelease(&result);` |
|      23 | 4221 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4222 | `				PH7_MemObjRelease(&key2);` |
|       - | 4223 | `				/* move to next node */` |
|      23 | 4224 | `				pIt = pIt->pPrev;` |
|      23 | 4225 | `				if( keep == 0 ) break;` |
|       1 | 4226 | `			}` |
|      21 | 4227 | `			if( keep == 0 ) break;` |
|       7 | 4228 | `		}` |
|      17 | 4229 | `		if( keep ){` |
|       - | 4230 | `			/* Perform the insertion */` |
|       9 | 4231 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4232 | `		}` |
|       - | 4233 | `		/* Point to the next entry */` |
|      17 | 4234 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4235 | `		n--;` |
|       1 | 4236 | `	}` |
|       - | 4237 | `	/* Return the freshly created array */` |
|       9 | 4238 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4239 | `	return PH7_OK;` |
|      13 | 4240 |  |
|       - | 4241 | `/*` |
|       - | 4242 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4243 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4244 | ` * Parameters` |
|       - | 4245 | ` *  $array1` |
|       - | 4246 | ` *    The array to compare from` |
|       - | 4247 | ` *  $array2` |
|       - | 4248 | ` *    An array to compare against` |
|       - | 4249 | ` *  $...` |
|       - | 4250 | ` *   More arrays to compare against` |
|       - | 4251 | ` * Return` |
|       - | 4252 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4253 | ` *  in any of the other arrays.` |
|       - | 4254 | ` * Note that NULL is returned on failure.` |
|       - | 4255 | ` */` |
|      14 | 4256 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4257 |  |
|       - | 4258 | `	ph7_hashmap_node *pEntry;` |
|       - | 4259 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4260 | `	ph7_value *pArray;` |
|       - | 4261 | `	sxi32 rc;` |
|       - | 4262 | `	sxu32 n;` |
|       - | 4263 | `	int i;` |
|       - | 4264 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4265 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4266 | `	 * helpers. */` |
|      16 | 4267 | `	if( nArg < 1 ){` |
|       4 | 4268 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4269 | `			"ArgumentCountError",` |
|       - | 4270 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4271 | `			nArg` |
|       - | 4272 | `			);` |
|       - | 4273 | `	}` |
|      14 | 4274 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4275 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4276 | `			"TypeError",` |
|       - | 4277 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4278 | `			ph7_type_name(apArg[0])` |
|       - | 4279 | `			);` |
|       - | 4280 | `	}` |
|      20 | 4281 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4282 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4283 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4284 | `				"TypeError",` |
|       - | 4285 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4286 | `				i + 1,` |
|       2 | 4287 | `				ph7_type_name(apArg[i])` |
|       - | 4288 | `				);` |
|       - | 4289 | `		}` |
|       5 | 4290 | `	}` |
|       9 | 4291 | `	if( nArg == 1 ){` |
|       - | 4292 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4293 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4294 | `		return PH7_OK;` |
|       - | 4295 | `	}` |
|       - | 4296 | `	/* Create a new array */` |
|       7 | 4297 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4298 | `	if( pArray == 0 ){` |
|     ! 0 | 4299 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4300 | `		return PH7_OK;` |
|       - | 4301 | `	}` |
|       - | 4302 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4303 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4304 | `	/* Perfrom the diff */` |
|       7 | 4305 | `	pEntry = pSrc->pFirst;` |
|       7 | 4306 | `	n = pSrc->nEntry;` |
|      12 | 4307 | `	for(;;){` |
|      25 | 4308 | `		if( n < 1 ){` |
|       7 | 4309 | `			break;` |
|       - | 4310 | `		}` |
|      31 | 4311 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4312 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4313 | `				/* ignore */` |
|     ! 0 | 4314 | `				continue;` |
|       - | 4315 | `			}` |
|      23 | 4316 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4317 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4318 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4319 | `				/* Blob lookup */` |
|      17 | 4320 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4321 | `			}else{` |
|       - | 4322 | `				/* Int lookup */` |
|       7 | 4323 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4324 | `			}` |
|      23 | 4325 | `			if( rc == SXRET_OK ){` |
|       - | 4326 | `				/* Key exists,break immediately */` |
|      11 | 4327 | `				break;` |
|       - | 4328 | `			}` |
|       7 | 4329 | `		}` |
|      19 | 4330 | `		if( i >= nArg ){` |
|       - | 4331 | `			/* Perform the insertion */` |
|       9 | 4332 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4333 | `		}` |
|       - | 4334 | `		/* Point to the next entry */` |
|      19 | 4335 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4336 | `		n--;` |
|       1 | 4337 | `	}` |
|       - | 4338 | `	/* Return the freshly created array */` |
|       7 | 4339 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4340 | `	return PH7_OK;` |
|       9 | 4341 |  |
|       - | 4342 | `/*` |
|       - | 4343 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4344 | ` *  Computes the intersection of arrays.` |
|       - | 4345 | ` * Parameters` |
|       - | 4346 | ` *  $array1` |
|       - | 4347 | ` *    The array to compare from` |
|       - | 4348 | ` *  $array2` |
|       - | 4349 | ` *    An array to compare against` |
|       - | 4350 | ` *  $...` |
|       - | 4351 | ` *   More arrays to compare against` |
|       - | 4352 | ` * Return` |
|       - | 4353 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4354 | ` *  in all of the parameters.` |
|       - | 4355 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4356 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4357 | ` */` |
|      22 | 4358 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4359 |  |
|       - | 4360 | `	ph7_hashmap_node *pEntry;` |
|       - | 4361 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4362 | `	ph7_value *pArray;` |
|       - | 4363 | `	ph7_value *pVal;` |
|       - | 4364 | `	sxi32 rc;` |
|       - | 4365 | `	sxu32 n;` |
|       - | 4366 | `	int i;` |
|      24 | 4367 | `	if( nArg < 1 ){` |
|       4 | 4368 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4369 | `			"ArgumentCountError",` |
|       - | 4370 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4371 | `			nArg` |
|       - | 4372 | `			);` |
|       - | 4373 | `	}` |
|      22 | 4374 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4375 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4376 | `			"TypeError",` |
|       - | 4377 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4378 | `			ph7_type_name(apArg[0])` |
|       - | 4379 | `			);` |
|       - | 4380 | `	}` |
|      36 | 4381 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4382 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4383 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4384 | `				"TypeError",` |
|       - | 4385 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4386 | `				i + 1,` |
|       2 | 4387 | `				ph7_type_name(apArg[i])` |
|       - | 4388 | `				);` |
|       - | 4389 | `		}` |
|       9 | 4390 | `	}` |
|      17 | 4391 | `	if( nArg == 1 ){` |
|       - | 4392 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4393 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4394 | `		return PH7_OK;` |
|       - | 4395 | `	}` |
|       - | 4396 | `	/* Create a new array */` |
|      15 | 4397 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4398 | `	if( pArray == 0 ){` |
|     ! 0 | 4399 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4400 | `		return PH7_OK;` |
|       - | 4401 | `	}` |
|       - | 4402 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4403 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4404 | `	/* Perform the intersection */` |
|      15 | 4405 | `	pEntry = pSrc->pFirst;` |
|      15 | 4406 | `	n = pSrc->nEntry;` |
|      31 | 4407 | `	for(;;){` |
|      63 | 4408 | `		if( n < 1 ){` |
|      15 | 4409 | `			break;` |
|       - | 4410 | `		}` |
|       - | 4411 | `		/* Extract the node value */` |
|      49 | 4412 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4413 | `		if( pVal ){` |
|      79 | 4414 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4415 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4416 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4417 | `				/* Perform the lookup */` |
|      55 | 4418 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4419 | `				if( rc != SXRET_OK ){` |
|       - | 4420 | `					/* Value does not exist */` |
|      25 | 4421 | `					break;` |
|       - | 4422 | `				}` |
|      16 | 4423 | `			}` |
|      49 | 4424 | `			if( i >= nArg ){` |
|       - | 4425 | `				/* Perform the insertion */` |
|      25 | 4426 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4427 | `			}` |
|      24 | 4428 | `		}` |
|       - | 4429 | `		/* Point to the next entry */` |
|      49 | 4430 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4431 | `		n--;` |
|       1 | 4432 | `	}` |
|       - | 4433 | `	/* Return the freshly created array */` |
|      15 | 4434 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4435 | `	return PH7_OK;` |
|      13 | 4436 |  |
|       - | 4437 | `/*` |
|       - | 4438 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4439 | ` *  Computes the intersection of arrays.` |
|       - | 4440 | ` * Parameters` |
|       - | 4441 | ` *  $array1` |
|       - | 4442 | ` *    The array to compare from` |
|       - | 4443 | ` *  $array2` |
|       - | 4444 | ` *    An array to compare against` |
|       - | 4445 | ` *  $...` |
|       - | 4446 | ` *   More arrays to compare against` |
|       - | 4447 | ` * Return` |
|       - | 4448 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4449 | ` *  in all of the parameters. .` |
|       - | 4450 | ` * Note that NULL is returned on failure.` |
|       - | 4451 | ` */` |
|     ! 0 | 4452 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 4453 |  |
|       - | 4454 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4455 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4456 | `	ph7_value *pArray;` |
|       - | 4457 | `	ph7_value *pVal;` |
|       - | 4458 | `	sxi32 rc;` |
|       - | 4459 | `	sxu32 n;` |
|       - | 4460 | `	int i;` |
|     ! 0 | 4461 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4462 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4463 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4464 | `		return PH7_OK;` |
|       - | 4465 | `	}` |
|     ! 0 | 4466 | `	if( nArg == 1 ){` |
|       - | 4467 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4468 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4469 | `		return PH7_OK;` |
|       - | 4470 | `	}` |
|       - | 4471 | `	/* Create a new array */` |
|     ! 0 | 4472 | `	pArray = ph7_context_new_array(pCtx);` |
|     ! 0 | 4473 | `	if( pArray == 0 ){` |
|     ! 0 | 4474 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4475 | `		return PH7_OK;` |
|       - | 4476 | `	}` |
|       - | 4477 | `	/* Point to the internal representation of the source hashmap */` |
|     ! 0 | 4478 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4479 | `	/* Perform the intersection */` |
|     ! 0 | 4480 | `	pEntry = pSrc->pFirst;` |
|     ! 0 | 4481 | `	n = pSrc->nEntry;` |
|     ! 0 | 4482 | `	pN1 = pN2 = 0; /* cc warning */` |
|     ! 0 | 4483 | `	for(;;){` |
|     ! 0 | 4484 | `		if( n < 1 ){` |
|     ! 0 | 4485 | `			break;` |
|       - | 4486 | `		}` |
|       - | 4487 | `		/* Extract the node value */` |
|     ! 0 | 4488 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 4489 | `		if( pVal ){` |
|     ! 0 | 4490 | `			for( i = 1 ; i < nArg ; i++ ){` |
|     ! 0 | 4491 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4492 | `					/* ignore */` |
|     ! 0 | 4493 | `					continue;` |
|       - | 4494 | `				}` |
|       - | 4495 | `				/* Point to the internal representation of the hashmap */` |
|     ! 0 | 4496 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4497 | `				/* Perform a key lookup first */` |
|     ! 0 | 4498 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4499 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|     ! 0 | 4500 | `				}else{` |
|     ! 0 | 4501 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4502 | `				}` |
|     ! 0 | 4503 | `				if( rc != SXRET_OK ){` |
|       - | 4504 | `					/* No such key,break immediately */` |
|     ! 0 | 4505 | `					break;` |
|       - | 4506 | `				}` |
|       - | 4507 | `				/* Perform the lookup */` |
|     ! 0 | 4508 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|     ! 0 | 4509 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4510 | `					/* Value does not exist */` |
|     ! 0 | 4511 | `					break;` |
|       - | 4512 | `				}` |
|     ! 0 | 4513 | `			}` |
|     ! 0 | 4514 | `			if( i >= nArg ){` |
|       - | 4515 | `				/* Perform the insertion */` |
|     ! 0 | 4516 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     ! 0 | 4517 | `			}` |
|     ! 0 | 4518 | `		}` |
|       - | 4519 | `		/* Point to the next entry */` |
|     ! 0 | 4520 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 4521 | `		n--;` |
|     ! 0 | 4522 | `	}` |
|       - | 4523 | `	/* Return the freshly created array */` |
|     ! 0 | 4524 | `	ph7_result_value(pCtx,pArray);` |
|     ! 0 | 4525 | `	return PH7_OK;` |
|     ! 0 | 4526 |  |
|       - | 4527 | `/*` |
|       - | 4528 | ` * array array_intersect_key(array $array1 ,array $array2,...)` |
|       - | 4529 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4530 | ` * Parameters` |
|       - | 4531 | ` *  $array1` |
|       - | 4532 | ` *    The array to compare from` |
|       - | 4533 | ` *  $array2` |
|       - | 4534 | ` *    An array to compare against` |
|       - | 4535 | ` *  $...` |
|       - | 4536 | ` *   More arrays to compare against` |
|       - | 4537 | ` * Return` |
|       - | 4538 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4539 | ` *  have keys that are present in all arguments.` |
|       - | 4540 | ` * Note that NULL is returned on failure.` |
|       - | 4541 | ` */` |
|       2 | 4542 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4543 |  |
|       - | 4544 | `	ph7_hashmap_node *pEntry;` |
|       - | 4545 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4546 | `	ph7_value *pArray;` |
|       - | 4547 | `	sxi32 rc;` |
|       - | 4548 | `	sxu32 n;` |
|       - | 4549 | `	int i;` |
|       3 | 4550 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4551 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4552 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4553 | `		return PH7_OK;` |
|       - | 4554 | `	}` |
|       3 | 4555 | `	if( nArg == 1 ){` |
|       - | 4556 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4557 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4558 | `		return PH7_OK;` |
|       - | 4559 | `	}` |
|       - | 4560 | `	/* Create a new array */` |
|       3 | 4561 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4562 | `	if( pArray == 0 ){` |
|     ! 0 | 4563 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4564 | `		return PH7_OK;` |
|       - | 4565 | `	}` |
|       - | 4566 | `	/* Point to the internal representation of the main hashmap */` |
|       3 | 4567 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4568 | `	/* Perfrom the intersection */` |
|       3 | 4569 | `	pEntry = pSrc->pFirst;` |
|       3 | 4570 | `	n = pSrc->nEntry;` |
|       4 | 4571 | `	for(;;){` |
|       9 | 4572 | `		if( n < 1 ){` |
|       3 | 4573 | `			break;` |
|       - | 4574 | `		}` |
|       9 | 4575 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       7 | 4576 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4577 | `				/* ignore */` |
|     ! 0 | 4578 | `				continue;` |
|       - | 4579 | `			}` |
|       7 | 4580 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       7 | 4581 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     ! 0 | 4582 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4583 | `				/* Blob lookup */` |
|     ! 0 | 4584 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|     ! 0 | 4585 | `			}else{` |
|       - | 4586 | `				/* Int key */` |
|       7 | 4587 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4588 | `			}` |
|       7 | 4589 | `			if( rc != SXRET_OK ){` |
|       - | 4590 | `				/* Key does not exists,break immediately */` |
|       5 | 4591 | `				break;` |
|       - | 4592 | `			}` |
|       2 | 4593 | `		}` |
|       7 | 4594 | `		if( i >= nArg ){` |
|       - | 4595 | `			/* Perform the insertion */` |
|       3 | 4596 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       1 | 4597 | `		}` |
|       - | 4598 | `		/* Point to the next entry */` |
|       7 | 4599 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4600 | `		n--;` |
|       1 | 4601 | `	}` |
|       - | 4602 | `	/* Return the freshly created array */` |
|       3 | 4603 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4604 | `	return PH7_OK;` |
|       2 | 4605 |  |
|       - | 4606 | `/*` |
|       - | 4607 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4608 | ` *  Computes the intersection of arrays.` |
|       - | 4609 | ` * Parameters` |
|       - | 4610 | ` *  $array1` |
|       - | 4611 | ` *    The array to compare from` |
|       - | 4612 | ` *  $array2` |
|       - | 4613 | ` *    An array to compare against` |
|       - | 4614 | ` *  $...` |
|       - | 4615 | ` *   More arrays to compare against` |
|       - | 4616 | ` * $callback` |
|       - | 4617 | ` *  The callback comparison function.` |
|       - | 4618 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4619 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4620 | ` *  than the second.` |
|       - | 4621 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4622 | ` * Return` |
|       - | 4623 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4624 | ` *  in all of the parameters. .` |
|       - | 4625 | ` * Note that NULL is returned on failure.` |
|       - | 4626 | ` */` |
|       2 | 4627 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4628 |  |
|       - | 4629 | `	ph7_hashmap_node *pEntry;` |
|       - | 4630 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4631 | `	ph7_value *pCallback;` |
|       - | 4632 | `	ph7_value *pArray;` |
|       - | 4633 | `	ph7_value *pVal;` |
|       - | 4634 | `	sxi32 rc;` |
|       - | 4635 | `	sxu32 n;` |
|       - | 4636 | `	int i;` |
|       - | 4637 |  |
|       3 | 4638 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4639 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4640 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4641 | `		return PH7_OK;` |
|       - | 4642 | `	}` |
|       - | 4643 | `	/* Point to the callback */` |
|       3 | 4644 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4645 | `	if( nArg == 2 ){` |
|       - | 4646 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4647 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4648 | `		return PH7_OK;` |
|       - | 4649 | `	}` |
|       - | 4650 | `	/* Create a new array */` |
|       3 | 4651 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4652 | `	if( pArray == 0 ){` |
|     ! 0 | 4653 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4654 | `		return PH7_OK;` |
|       - | 4655 | `	}` |
|       - | 4656 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4657 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4658 | `	/* Perform the intersection */` |
|       3 | 4659 | `	pEntry = pSrc->pFirst;` |
|       3 | 4660 | `	n = pSrc->nEntry;` |
|       4 | 4661 | `	for(;;){` |
|       9 | 4662 | `		if( n < 1 ){` |
|       3 | 4663 | `			break;` |
|       - | 4664 | `		}` |
|       - | 4665 | `		/* Extract the node value */` |
|       7 | 4666 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4667 | `		if( pVal ){` |
|      11 | 4668 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4669 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4670 | `					/* ignore */` |
|     ! 0 | 4671 | `					continue;` |
|       - | 4672 | `				}` |
|       - | 4673 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4674 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4675 | `				/* Perform the lookup */` |
|       7 | 4676 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4677 | `				if( rc != SXRET_OK ){` |
|       - | 4678 | `					/* Value does not exist */` |
|       3 | 4679 | `					break;` |
|       - | 4680 | `				}` |
|       3 | 4681 | `			}` |
|       7 | 4682 | `			if( i >= (nArg-1) ){` |
|       - | 4683 | `				/* Perform the insertion */` |
|       5 | 4684 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4685 | `			}` |
|       3 | 4686 | `		}` |
|       - | 4687 | `		/* Point to the next entry */` |
|       7 | 4688 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4689 | `		n--;` |
|       1 | 4690 | `	}` |
|       - | 4691 | `	/* Return the freshly created array */` |
|       3 | 4692 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4693 | `	return PH7_OK;` |
|       2 | 4694 |  |
|       - | 4695 | `/*` |
|       - | 4696 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 4697 | ` *  Fill an array with values.` |
|       - | 4698 | ` * Parameters` |
|       - | 4699 | ` *  $start_index` |
|       - | 4700 | ` *    The first index of the returned array.` |
|       - | 4701 | ` *  $num` |
|       - | 4702 | ` *   Number of elements to insert.` |
|       - | 4703 | ` *  $value` |
|       - | 4704 | ` *    Value to use for filling.` |
|       - | 4705 | ` * Return` |
|       - | 4706 | ` *  The filled array or null on failure.` |
|       - | 4707 | ` */` |
|     238 | 4708 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4709 |  |
|       - | 4710 | `	ph7_value *pArray;` |
|       - | 4711 | `	int i,nEntry;` |
|       - | 4712 |  |
|       - | 4713 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 4714 | `	if( nArg != 3 ){` |
|       - | 4715 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 4716 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4717 | `			"ArgumentCountError",` |
|       - | 4718 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 4719 | `			nArg` |
|       - | 4720 | `			);` |
|       - | 4721 | `	}` |
|       - | 4722 |  |
|       - | 4723 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 4724 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 4725 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 4726 | `	 * and NULLs are rejected outright. */` |
|     466 | 4727 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 4728 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 4729 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4730 | `			"TypeError",` |
|       - | 4731 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 4732 | `			ph7_type_name(apArg[0])` |
|       - | 4733 | `			);` |
|       - | 4734 | `	}` |
|     234 | 4735 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 4736 | `		int len;` |
|       8 | 4737 | `		sxu8 bReal = FALSE;` |
|       8 | 4738 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 4739 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 4740 | `			/* Non‑numeric string is an error. */` |
|       3 | 4741 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4742 | `				"TypeError",` |
|       - | 4743 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 4744 | `				);` |
|       - | 4745 | `		}` |
|       5 | 4746 | `		if( bReal ){` |
|       - | 4747 | `			/* float-string -> deprecation warning */` |
|       4 | 4748 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4749 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 4750 | `				zStr` |
|       - | 4751 | `				);` |
|       1 | 4752 | `		}` |
|       2 | 4753 | `	}` |
|       - | 4754 |  |
|       - | 4755 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 4756 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 4757 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 4758 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4759 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4760 | `			"TypeError",` |
|       - | 4761 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 4762 | `			ph7_type_name(apArg[1])` |
|       - | 4763 | `			);` |
|       - | 4764 | `	}` |
|     232 | 4765 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4766 | `		int len;` |
|       3 | 4767 | `		sxu8 bReal = FALSE;` |
|       3 | 4768 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4769 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 4770 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4771 | `				"TypeError",` |
|       - | 4772 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 4773 | `				);` |
|       - | 4774 | `		}` |
|     ! 0 | 4775 | `	}` |
|       - | 4776 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 4777 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 4778 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 4779 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 4780 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 4781 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 4782 | `		if( d != (double)i64 ){` |
|       7 | 4783 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4784 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 4785 | `				d` |
|       - | 4786 | `				);` |
|       2 | 4787 | `		}` |
|       2 | 4788 | `	}` |
|       - | 4789 |  |
|       - | 4790 | `	/* Total number of entries to insert */` |
|     230 | 4791 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 4792 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 4793 | `	if( nEntry < 0 ){` |
|       3 | 4794 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4795 | `			"ValueError",` |
|       - | 4796 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 4797 | `			);` |
|       - | 4798 | `	}` |
|       - | 4799 |  |
|       - | 4800 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 4801 | `	if( nEntry == 0 ){` |
|       7 | 4802 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 4803 | `		return PH7_OK;` |
|       - | 4804 | `	}` |
|       - | 4805 |  |
|       - | 4806 | `	/* Create a new array */` |
|     221 | 4807 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 4808 | `	if( pArray == 0 ){` |
|     ! 0 | 4809 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4810 | `		return PH7_OK;` |
|       - | 4811 | `	}` |
|       - | 4812 |  |
|       - | 4813 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 4814 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 4815 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 4816 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 4817 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 4818 | `	}` |
|       - | 4819 | `	/* Return the filled array */` |
|     221 | 4820 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 4821 | `	return PH7_OK;` |
|     121 | 4822 |  |
|       - | 4823 | `/*` |
|       - | 4824 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 4825 | ` *  Fill an array with values, specifying keys.` |
|       - | 4826 | ` * Parameters` |
|       - | 4827 | ` *  $input` |
|       - | 4828 | ` *   Array of values that will be used as key.` |
|       - | 4829 | ` *  $value` |
|       - | 4830 | ` *    Value to use for filling.` |
|       - | 4831 | ` * Return` |
|       - | 4832 | ` *  The filled array.` |
|       - | 4833 | ` * Throws` |
|       - | 4834 | ` *  ValueError if $input is not an array.` |
|       - | 4835 | ` */` |
|      26 | 4836 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4837 |  |
|       - | 4838 | `	ph7_hashmap_node *pEntry;` |
|       - | 4839 | `	ph7_hashmap *pSrc;` |
|       - | 4840 | `	ph7_value *pArray;` |
|       - | 4841 | `	sxu32 n;` |
|       - | 4842 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 4843 | `	if( nArg != 2 ){` |
|      10 | 4844 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4845 | `			"ArgumentCountError",` |
|       - | 4846 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 4847 | `			nArg` |
|       - | 4848 | `			);` |
|       - | 4849 | `	}` |
|       - | 4850 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 4851 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 4852 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4853 | `			"TypeError",` |
|       - | 4854 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 4855 | `			ph7_type_name(apArg[0])` |
|       - | 4856 | `			);` |
|       - | 4857 | `	}` |
|       - | 4858 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 4859 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4860 | `	/* Create a new array */` |
|      17 | 4861 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 4862 | `	if( pArray == 0 ){` |
|     ! 0 | 4863 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4864 | `		return PH7_OK;` |
|       - | 4865 | `	}` |
|       - | 4866 | `	/* Perform the requested operation */` |
|      17 | 4867 | `	pEntry = pSrc->pFirst;` |
|      45 | 4868 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 4869 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 4870 | `		/* Point to the next entry */` |
|      29 | 4871 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 4872 | `	}` |
|       - | 4873 | `	/* Return the filled array */` |
|      17 | 4874 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4875 | `	return PH7_OK;` |
|      15 | 4876 |  |
|       - | 4877 | `/*` |
|       - | 4878 | ` * array array_combine(array $keys,array $values)` |
|       - | 4879 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 4880 | ` * Parameters` |
|       - | 4881 | ` *  $keys` |
|       - | 4882 | ` *    Array of keys to be used.` |
|       - | 4883 | ` * $values` |
|       - | 4884 | ` *   Array of values to be used.` |
|       - | 4885 | ` * Return` |
|       - | 4886 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 4887 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 4888 | ` *  not an array.` |
|       - | 4889 | ` */` |
|      18 | 4890 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4891 |  |
|       - | 4892 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 4893 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 4894 | `	ph7_value *pArray;` |
|       - | 4895 | `	sxu32 n;` |
|       - | 4896 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 4897 | `	if( nArg != 2 ){` |
|       - | 4898 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 4899 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4900 | `			"ArgumentCountError",` |
|       - | 4901 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 4902 | `			nArg` |
|       - | 4903 | `			);` |
|       - | 4904 | `	}` |
|       - | 4905 | `	/* Validate argument types individually so we can report the correct` |
|       - | 4906 | `	 * argument index in the error message. */` |
|      18 | 4907 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4908 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4909 | `			"TypeError",` |
|       - | 4910 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 4911 | `			ph7_type_name(apArg[0])` |
|       - | 4912 | `			);` |
|       - | 4913 | `	}` |
|      16 | 4914 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 4915 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4916 | `			"TypeError",` |
|       - | 4917 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 4918 | `			ph7_type_name(apArg[1])` |
|       - | 4919 | `			);` |
|       - | 4920 | `	}` |
|       - | 4921 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 4922 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 4923 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 4924 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 4925 | `		/* Length mismatch -> ValueError */` |
|       3 | 4926 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4927 | `			"ValueError",` |
|       - | 4928 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 4929 | `			);` |
|       - | 4930 | `	}` |
|       - | 4931 | `	/* Create a new array */` |
|      11 | 4932 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4933 | `	if( pArray == 0 ){` |
|     ! 0 | 4934 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4935 | `		return PH7_OK;` |
|       - | 4936 | `	}` |
|       - | 4937 | `	/* Perform the requested operation */` |
|      11 | 4938 | `	pKe = pKey->pFirst;` |
|      11 | 4939 | `	pVe = pValue->pFirst;` |
|      33 | 4940 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 4941 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 4942 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 4943 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 4944 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 4945 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 4946 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 4947 | `		 * original array must not be mutated. */` |
|      23 | 4948 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 4949 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 4950 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 4951 | `			if( pTmpKey ){` |
|       5 | 4952 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 4953 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 4954 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 4955 | `				pKeyCopy = pTmpKey;` |
|       2 | 4956 | `			}` |
|       2 | 4957 | `		}` |
|      23 | 4958 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 4959 | `		/* Point to the next entry */` |
|      23 | 4960 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 4961 | `		pVe = pVe->pPrev;` |
|      12 | 4962 | `	}` |
|       - | 4963 | `	/* Return the filled array */` |
|      11 | 4964 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4965 | `	return PH7_OK;` |
|      11 | 4966 |  |
|       - | 4967 | `/*` |
|       - | 4968 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 4969 | ` *  Return an array with elements in reverse order.` |
|       - | 4970 | ` * Parameters` |
|       - | 4971 | ` *  $array` |
|       - | 4972 | ` *   The input array.` |
|       - | 4973 | ` *  $preserve_keys (optional)` |
|       - | 4974 | ` *   If set to TRUE keys are preserved.` |
|       - | 4975 | ` * Return` |
|       - | 4976 | ` *  The reversed array.` |
|       - | 4977 | ` */` |
|      20 | 4978 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4979 |  |
|       - | 4980 | `	ph7_hashmap_node *pEntry;` |
|       - | 4981 | `	ph7_hashmap *pSrc;` |
|       - | 4982 | `	ph7_value *pArray;` |
|       - | 4983 | `	int bPreserve;` |
|       - | 4984 | `	sxu32 n;` |
|      22 | 4985 | `	if( nArg < 1 ){` |
|       4 | 4986 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4987 | `			"ArgumentCountError",` |
|       - | 4988 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 4989 | `			nArg` |
|       - | 4990 | `			);` |
|       - | 4991 | `	}` |
|       - | 4992 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 4993 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4994 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4995 | `			"TypeError",` |
|       - | 4996 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4997 | `			ph7_type_name(apArg[0])` |
|       - | 4998 | `			);` |
|       - | 4999 | `	}` |
|      17 | 5000 | `	bPreserve = FALSE;` |
|      17 | 5001 | `	if( nArg > 1 ){` |
|       7 | 5002 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5003 | `	}` |
|       - | 5004 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5005 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5006 | `	/* Create a new array */` |
|      17 | 5007 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5008 | `	if( pArray == 0 ){` |
|     ! 0 | 5009 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5010 | `		return PH7_OK;` |
|       - | 5011 | `	}` |
|       - | 5012 | `	/* Perform the requested operation */` |
|      17 | 5013 | `	pEntry = pSrc->pLast;` |
|      55 | 5014 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5015 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5016 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5017 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5018 | `		/* Point to the previous entry */` |
|      39 | 5019 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5020 | `	}` |
|      17 | 5021 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5022 | `	return PH7_OK;` |
|      12 | 5023 |  |
|       - | 5024 | `/*` |
|       - | 5025 | ` * array array_unique(array $array[,int $sort_flags = SORT_STRING ])` |
|       - | 5026 | ` *  Removes duplicate values from an array` |
|       - | 5027 | ` * Parameter` |
|       - | 5028 | ` *  $array` |
|       - | 5029 | ` *   The input array.` |
|       - | 5030 | ` *  $sort_flags` |
|       - | 5031 | ` *    The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 5032 | ` *    Sorting type flags:` |
|       - | 5033 | ` *       SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5034 | ` *       SORT_NUMERIC - compare items numerically` |
|       - | 5035 | ` *       SORT_STRING - compare items as strings` |
|       - | 5036 | ` *       SORT_LOCALE_STRING - compare items as` |
|       - | 5037 | ` * Return` |
|       - | 5038 | ` *  Filtered array or NULL on failure.` |
|       - | 5039 | ` */` |
|       2 | 5040 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5041 |  |
|       - | 5042 | `	ph7_hashmap_node *pEntry;` |
|       - | 5043 | `	ph7_value *pNeedle;` |
|       - | 5044 | `	ph7_hashmap *pSrc;` |
|       - | 5045 | `	ph7_value *pArray;` |
|       - | 5046 | `	int bStrict;` |
|       - | 5047 | `	sxi32 rc;` |
|       - | 5048 | `	sxu32 n;` |
|       3 | 5049 | `	if( nArg < 1 ){` |
|       - | 5050 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 5051 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5052 | `		return PH7_OK;` |
|       - | 5053 | `	}` |
|       - | 5054 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 5055 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5056 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 5057 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5058 | `		return PH7_OK;` |
|       - | 5059 | `	}` |
|       3 | 5060 | `	bStrict = FALSE;` |
|       3 | 5061 | `	if( nArg > 1 ){` |
|     ! 0 | 5062 | `		bStrict = ph7_value_to_int(apArg[1]) == 3 /* SORT_REGULAR */ ? 1 : 0;` |
|     ! 0 | 5063 | `	}` |
|       - | 5064 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 5065 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5066 | `	/* Create a new array */` |
|       3 | 5067 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5068 | `	if( pArray == 0 ){` |
|     ! 0 | 5069 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5070 | `		return PH7_OK;` |
|       - | 5071 | `	}` |
|       - | 5072 | `	/* Perform the requested operation */` |
|       3 | 5073 | `	pEntry = pSrc->pFirst;` |
|      13 | 5074 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      11 | 5075 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      11 | 5076 | `		rc = SXERR_NOTFOUND;` |
|      11 | 5077 | `		if( pNeedle ){` |
|      11 | 5078 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|       5 | 5079 | `		}` |
|      11 | 5080 | `		if( rc != SXRET_OK ){` |
|       - | 5081 | `			/* Perform the insertion */` |
|       7 | 5082 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 5083 | `		}` |
|       - | 5084 | `		/* Point to the next entry */` |
|      11 | 5085 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 5086 | `	}` |
|       - | 5087 | `	/* Return the freshly created array */` |
|       3 | 5088 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5089 | `	return PH7_OK;` |
|       2 | 5090 |  |
|       - | 5091 | `/*` |
|       - | 5092 | ` * array array_flip(array $input)` |
|       - | 5093 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5094 | ` * Parameter` |
|       - | 5095 | ` *  $input` |
|       - | 5096 | ` *   Input array.` |
|       - | 5097 | ` * Return` |
|       - | 5098 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5099 | ` */` |
|      34 | 5100 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5101 |  |
|       - | 5102 | `	ph7_hashmap_node *pEntry;` |
|       - | 5103 | `	ph7_hashmap *pSrc;` |
|       - | 5104 | `	ph7_value *pArray;` |
|       - | 5105 | `	ph7_value *pKey;` |
|       - | 5106 | `	ph7_value sVal;` |
|       - | 5107 | `	sxu32 n;` |
|       - | 5108 |  |
|       - | 5109 | `	/* PHP requires exactly one argument */` |
|      36 | 5110 | `	if( nArg != 1 ){` |
|       - | 5111 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5112 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5113 | `			"ArgumentCountError",` |
|       - | 5114 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5115 | `			nArg` |
|       - | 5116 | `			);` |
|       - | 5117 | `	}` |
|       - | 5118 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5119 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5120 | `		/* Type mismatch -> TypeError */` |
|       7 | 5121 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5122 | `			"TypeError",` |
|       - | 5123 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5124 | `			ph7_type_name(apArg[0])` |
|       - | 5125 | `			);` |
|       - | 5126 | `	}` |
|       - | 5127 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5128 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5129 | `	/* Create a new array */` |
|      27 | 5130 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5131 | `	if( pArray == 0 ){` |
|     ! 0 | 5132 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5133 | `		return PH7_OK;` |
|       - | 5134 | `	}` |
|       - | 5135 | `	/* Start processing */` |
|      27 | 5136 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5137 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5138 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5139 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5140 | `		if( pKey ){` |
|       - | 5141 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5142 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5143 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5144 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5145 | `					);` |
|   22236 | 5146 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5147 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5148 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5149 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5150 | `				}else{` |
|       - | 5151 | `					SyString sStr;` |
|    2227 | 5152 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5153 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5154 | `				}` |
|       - | 5155 | `				/* Perform the insertion */` |
|   22227 | 5156 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5157 | `				/* Safely release the value because each inserted entry` |
|       - | 5158 | `				 * has its own private copy of the value.` |
|       - | 5159 | `				 */` |
|   22227 | 5160 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5161 | `			}else{` |
|       - | 5162 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5163 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5164 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5165 | `					);` |
|       - | 5166 | `			}` |
|   11118 | 5167 | `		}` |
|       - | 5168 | `		/* Point to the next entry */` |
|   22237 | 5169 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5170 | `	}` |
|       - | 5171 | `	/* Return the freshly created array */` |
|      27 | 5172 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5173 | `	return PH7_OK;` |
|      19 | 5174 |  |
|       - | 5175 | `/*` |
|       - | 5176 | ` * number array_sum(array $array )` |
|       - | 5177 | ` *  Calculate the sum of values in an array.` |
|       - | 5178 | ` * Parameters` |
|       - | 5179 | ` *  $array: The input array.` |
|       - | 5180 | ` * Return` |
|       - | 5181 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5182 | ` */` |
|      24 | 5183 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5184 |  |
|       - | 5185 | `	ph7_hashmap_node *pEntry;` |
|       - | 5186 | `	ph7_value *pObj;` |
|      25 | 5187 | `	double dSum = 0;` |
|       - | 5188 | `	sxu32 n;` |
|      25 | 5189 | `	pEntry = pMap->pFirst;` |
|      91 | 5190 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5191 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5192 | `		if( pObj ){` |
|      67 | 5193 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5194 | `				dSum += pObj->rVal;` |
|      53 | 5195 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5196 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5197 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5198 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5199 | `					double dv = 0;` |
|      13 | 5200 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5201 | `					dSum += dv;` |
|       7 | 5202 | `				}` |
|      12 | 5203 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5204 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5205 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5206 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5207 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5208 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5209 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5210 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5211 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5212 | `			}` |
|       - | 5213 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5214 | `		}` |
|       - | 5215 | `		/* Point to the next entry */` |
|      67 | 5216 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5217 | `	}` |
|       - | 5218 | `	/* Return sum */` |
|      25 | 5219 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5220 |  |
|      18 | 5221 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5222 |  |
|       - | 5223 | `	ph7_hashmap_node *pEntry;` |
|       - | 5224 | `	ph7_value *pObj;` |
|      20 | 5225 | `	sxi64 nSum = 0;` |
|       - | 5226 | `	sxu32 n;` |
|      20 | 5227 | `	pEntry = pMap->pFirst;` |
|      80 | 5228 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5229 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5230 | `		if( pObj ){` |
|      62 | 5231 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5232 | `				nSum += pObj->x.iVal;` |
|      36 | 5233 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5234 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5235 | `					sxi64 nv = 0;` |
|       5 | 5236 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5237 | `					nSum += nv;` |
|       3 | 5238 | `				}` |
|       8 | 5239 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5240 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5241 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5242 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5243 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5244 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5245 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5246 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5247 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5248 | `			}` |
|       - | 5249 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5250 | `		}` |
|       - | 5251 | `		/* Point to the next entry */` |
|      62 | 5252 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5253 | `	}` |
|       - | 5254 | `	/* Return sum */` |
|      20 | 5255 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5256 |  |
|       - | 5257 | `/* number array_sum(array $array )` |
|       - | 5258 | ` * (See block-coment above)` |
|       - | 5259 | ` */` |
|      52 | 5260 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5261 |  |
|       - | 5262 | `	ph7_hashmap_node *pEntry;` |
|       - | 5263 | `	ph7_hashmap *pMap;` |
|       - | 5264 | `	ph7_value *pObj;` |
|      54 | 5265 | `	int useDouble = 0;` |
|       - | 5266 | `	sxu32 n;` |
|       - | 5267 | `	/* PHP requires exactly one argument */` |
|      54 | 5268 | `	if( nArg != 1 ){` |
|       7 | 5269 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5270 | `			"ArgumentCountError",` |
|       - | 5271 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5272 | `			nArg` |
|       - | 5273 | `			);` |
|       - | 5274 | `	}` |
|       - | 5275 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5276 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5277 | `		/* Type mismatch -> TypeError */` |
|       7 | 5278 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5279 | `			"TypeError",` |
|       - | 5280 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5281 | `			ph7_type_name(apArg[0])` |
|       - | 5282 | `			);` |
|       - | 5283 | `	}` |
|      46 | 5284 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5285 | `	if( pMap->nEntry < 1 ){` |
|       - | 5286 | `		/* Nothing to compute,return 0 */` |
|       3 | 5287 | `		ph7_result_int(pCtx,0);` |
|       3 | 5288 | `		return PH7_OK;` |
|       - | 5289 | `	}` |
|       - | 5290 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5291 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5292 | `	 */` |
|      44 | 5293 | `	pEntry = pMap->pFirst;` |
|     112 | 5294 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5295 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5296 | `		if( pObj ){` |
|      94 | 5297 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5298 | `				useDouble = 1;` |
|      19 | 5299 | `				break;` |
|       - | 5300 | `			}` |
|      76 | 5301 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5302 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5303 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5304 | `				sxu32 i;` |
|      23 | 5305 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5306 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5307 | `						useDouble = 1;` |
|       7 | 5308 | `						break;` |
|       - | 5309 | `					}` |
|       6 | 5310 | `				}` |
|      13 | 5311 | `				if( useDouble ){` |
|       7 | 5312 | `					break;` |
|       - | 5313 | `				}` |
|       3 | 5314 | `			}` |
|      34 | 5315 | `		}` |
|      70 | 5316 | `		pEntry = pEntry->pPrev;` |
|      36 | 5317 | `	}` |
|      44 | 5318 | `	if( useDouble ){` |
|      25 | 5319 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5320 | `	}else{` |
|      20 | 5321 | `		Int64Sum(pCtx,pMap);` |
|       - | 5322 | `	}` |
|      44 | 5323 | `	return PH7_OK;` |
|      28 | 5324 |  |
|       - | 5325 | `/*` |
|       - | 5326 | ` * number array_product(array $array )` |
|       - | 5327 | ` *  Calculate the product of values in an array.` |
|       - | 5328 | ` * Parameters` |
|       - | 5329 | ` *  $array: The input array.` |
|       - | 5330 | ` * Return` |
|       - | 5331 | ` *  Returns the product of values as an integer or float.` |
|       - | 5332 | ` */` |
|     ! 0 | 5333 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5334 |  |
|       - | 5335 | `	ph7_hashmap_node *pEntry;` |
|       - | 5336 | `	ph7_value *pObj;` |
|       - | 5337 | `	double dProd;` |
|       - | 5338 | `	sxu32 n;` |
|     ! 0 | 5339 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5340 | `	dProd = 1;` |
|     ! 0 | 5341 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5342 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5343 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5344 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5345 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5346 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5347 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5348 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5349 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5350 | `					double dv = 0;` |
|     ! 0 | 5351 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5352 | `					dProd *= dv;` |
|     ! 0 | 5353 | `				}` |
|     ! 0 | 5354 | `			}` |
|     ! 0 | 5355 | `		}` |
|       - | 5356 | `		/* Point to the next entry */` |
|     ! 0 | 5357 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5358 | `	}` |
|       - | 5359 | `	/* Return product */` |
|     ! 0 | 5360 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5361 |  |
|     ! 0 | 5362 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5363 |  |
|       - | 5364 | `	ph7_hashmap_node *pEntry;` |
|       - | 5365 | `	ph7_value *pObj;` |
|       - | 5366 | `	sxi64 nProd;` |
|       - | 5367 | `	sxu32 n;` |
|     ! 0 | 5368 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5369 | `	nProd = 1;` |
|     ! 0 | 5370 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5371 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5372 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5373 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5374 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5375 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5376 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5377 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5378 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5379 | `					sxi64 nv = 0;` |
|     ! 0 | 5380 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5381 | `					nProd *= nv;` |
|     ! 0 | 5382 | `				}` |
|     ! 0 | 5383 | `			}` |
|     ! 0 | 5384 | `		}` |
|       - | 5385 | `		/* Point to the next entry */` |
|     ! 0 | 5386 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5387 | `	}` |
|       - | 5388 | `	/* Return product */` |
|     ! 0 | 5389 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5390 |  |
|       - | 5391 | `/* number array_product(array $array )` |
|       - | 5392 | ` * (See block-block comment above)` |
|       - | 5393 | ` */` |
|     ! 0 | 5394 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5395 |  |
|       - | 5396 | `	ph7_hashmap *pMap;` |
|       - | 5397 | `	ph7_value *pObj;` |
|     ! 0 | 5398 | `	if( nArg < 1 ){` |
|       - | 5399 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5400 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5401 | `		return PH7_OK;` |
|       - | 5402 | `	}` |
|       - | 5403 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5404 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5405 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5406 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5407 | `		return PH7_OK;` |
|       - | 5408 | `	}` |
|     ! 0 | 5409 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5410 | `	if( pMap->nEntry < 1 ){` |
|       - | 5411 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5412 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5413 | `		return PH7_OK;` |
|       - | 5414 | `	}` |
|       - | 5415 | `	/* If the first element is of type float,then perform floating` |
|       - | 5416 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5417 | `	 */` |
|     ! 0 | 5418 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5419 | `	if( pObj == 0 ){` |
|     ! 0 | 5420 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5421 | `		return PH7_OK;` |
|       - | 5422 | `	}` |
|     ! 0 | 5423 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5424 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5425 | `	}else{` |
|     ! 0 | 5426 | `		Int64Prod(pCtx,pMap);` |
|       - | 5427 | `	}` |
|     ! 0 | 5428 | `	return PH7_OK;` |
|     ! 0 | 5429 |  |
|       - | 5430 | `/*` |
|       - | 5431 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5432 | ` *  Pick one or more random entries out of an array.` |
|       - | 5433 | ` * Parameters` |
|       - | 5434 | ` * $input` |
|       - | 5435 | ` *  The input array.` |
|       - | 5436 | ` * $num_req` |
|       - | 5437 | ` *  Specifies how many entries you want to pick.` |
|       - | 5438 | ` * Return` |
|       - | 5439 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5440 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5441 | ` *  NULL is returned on failure.` |
|       - | 5442 | ` */` |
|       6 | 5443 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5444 |  |
|       - | 5445 | `	ph7_hashmap_node *pNode;` |
|       - | 5446 | `	ph7_hashmap *pMap;` |
|       7 | 5447 | `	int nItem = 1;` |
|       7 | 5448 | `	if( nArg < 1 ){` |
|       - | 5449 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5450 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5451 | `		return PH7_OK;` |
|       - | 5452 | `	}` |
|       - | 5453 | `	/* Make sure we are dealing with an array */` |
|       7 | 5454 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5455 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5456 | `		return PH7_OK;` |
|       - | 5457 | `	}` |
|       - | 5458 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5459 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5460 | `	if(pMap->nEntry < 1 ){` |
|       - | 5461 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5462 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5463 | `		return PH7_OK;` |
|       - | 5464 | `	}` |
|       7 | 5465 | `	if( nArg > 1 ){` |
|       3 | 5466 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5467 | `	}` |
|       7 | 5468 | `	if( nItem < 2 ){` |
|       - | 5469 | `		sxu32 nEntry;` |
|       - | 5470 | `		/* Select a random number */` |
|       5 | 5471 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5472 | `		/* Extract the desired entry.` |
|       - | 5473 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5474 | `		 */` |
|       5 | 5475 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       2 | 5476 | `			pNode = pMap->pLast;` |
|       2 | 5477 | `			nEntry = pMap->nEntry - nEntry;` |
|       2 | 5478 | `			if( nEntry > 1 ){` |
|     ! 0 | 5479 | `				for(;;){` |
|     ! 0 | 5480 | `					if( nEntry == 0 ){` |
|     ! 0 | 5481 | `						break;` |
|       - | 5482 | `					}` |
|       - | 5483 | `					/* Point to the previous entry */` |
|     ! 0 | 5484 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5485 | `					nEntry--;` |
|     ! 0 | 5486 | `				}` |
|     ! 0 | 5487 | `			}` |
|       2 | 5488 | `		}else{` |
|       4 | 5489 | `			pNode = pMap->pFirst;` |
|       2 | 5490 | `			for(;;){` |
|       6 | 5491 | `				if( nEntry == 0 ){` |
|       4 | 5492 | `					break;` |
|       - | 5493 | `				}` |
|       - | 5494 | `				/* Point to the next entry */` |
|       3 | 5495 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       3 | 5496 | `				nEntry--;` |
|       1 | 5497 | `			}` |
|       - | 5498 | `		}` |
|       5 | 5499 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5500 | `			/* Int key */` |
|       3 | 5501 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5502 | `		}else{` |
|       - | 5503 | `			/* Blob key */` |
|       3 | 5504 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5505 | `		}` |
|       3 | 5506 | `	}else{` |
|       - | 5507 | `		ph7_value sKey,*pArray;` |
|       - | 5508 | `		ph7_hashmap *pDest;` |
|       - | 5509 | `		/* Create a new array */` |
|       3 | 5510 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5511 | `		if( pArray == 0 ){` |
|     ! 0 | 5512 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5513 | `			return PH7_OK;` |
|       - | 5514 | `		}` |
|       - | 5515 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5516 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5517 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5518 | `		/* Copy the first n items */` |
|       3 | 5519 | `		pNode = pMap->pFirst;` |
|       3 | 5520 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5521 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5522 | `		}` |
|       7 | 5523 | `		while( nItem > 0){` |
|       5 | 5524 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5525 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5526 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5527 | `			/* Point to the next entry */` |
|       5 | 5528 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5529 | `			nItem--;` |
|       1 | 5530 | `		}` |
|       - | 5531 | `		/* Shuffle the array */` |
|       3 | 5532 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5533 | `		/* Rehash node */` |
|       3 | 5534 | `		HashmapSortRehash(pDest);` |
|       - | 5535 | `		/* Return the random array */` |
|       3 | 5536 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5537 | `	}` |
|       7 | 5538 | `	return PH7_OK;` |
|       4 | 5539 |  |
|       - | 5540 | `/*` |
|       - | 5541 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5542 | ` *  Split an array into chunks.` |
|       - | 5543 | ` * Parameters` |
|       - | 5544 | ` * $input` |
|       - | 5545 | ` *   The array to work on` |
|       - | 5546 | ` * $size` |
|       - | 5547 | ` *   The size of each chunk` |
|       - | 5548 | ` * $preserve_keys` |
|       - | 5549 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5550 | ` *   the chunk numerically.` |
|       - | 5551 | ` * Return` |
|       - | 5552 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5553 | ` *  zero, with each dimension containing size elements.` |
|       - | 5554 | ` */` |
|      42 | 5555 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5556 |  |
|       - | 5557 | `	ph7_value *pArray,*pChunk;` |
|       - | 5558 | `	ph7_hashmap_node *pEntry;` |
|       - | 5559 | `	ph7_hashmap *pMap;` |
|       - | 5560 | `	int bPreserve;` |
|       - | 5561 | `	sxu32 nChunk;` |
|       - | 5562 | `	sxu32 nSize;` |
|       - | 5563 | `	sxu32 n;` |
|       - | 5564 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5565 | `	if( nArg < 2 ){` |
|       - | 5566 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5567 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5568 | `			"ArgumentCountError",` |
|       - | 5569 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5570 | `			nArg` |
|       - | 5571 | `			);` |
|       - | 5572 | `	}` |
|      42 | 5573 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5574 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5575 | `			"TypeError",` |
|       - | 5576 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5577 | `			ph7_type_name(apArg[0])` |
|       - | 5578 | `			);` |
|       - | 5579 | `	}` |
|       - | 5580 | `	/* Create a new array */` |
|      40 | 5581 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5582 | `	if( pArray == 0 ){` |
|     ! 0 | 5583 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5584 | `		return PH7_OK;` |
|       - | 5585 | `	}` |
|       - | 5586 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5587 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5588 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5589 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5590 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5591 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5592 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5593 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5594 | `			"TypeError",` |
|       - | 5595 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5596 | `			ph7_type_name(apArg[1])` |
|       - | 5597 | `			);` |
|       - | 5598 | `	}` |
|       - | 5599 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5600 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5601 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5602 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5603 | `		int len;` |
|       3 | 5604 | `		sxu8 bReal = FALSE;` |
|       3 | 5605 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5606 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5607 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5608 | `				"TypeError",` |
|       - | 5609 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5610 | `				);` |
|       - | 5611 | `		}` |
|     ! 0 | 5612 | `		if( bReal ){` |
|       - | 5613 | `			/* float-string -> warn but allow */` |
|     ! 0 | 5614 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5615 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 5616 | `				zStr` |
|       - | 5617 | `				);` |
|     ! 0 | 5618 | `		}` |
|     ! 0 | 5619 | `	}` |
|       - | 5620 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 5621 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 5622 | `	 * later via ph7_value_to_int. */` |
|      38 | 5623 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5624 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5625 | `		sxi64 i = (sxi64)d;` |
|       3 | 5626 | `		if( d != (double)i ){` |
|       4 | 5627 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5628 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 5629 | `				d` |
|       - | 5630 | `				);` |
|       1 | 5631 | `		}` |
|       1 | 5632 | `	}` |
|       - | 5633 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5634 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5635 | `	{` |
|      38 | 5636 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 5637 | `		if( nSizeSigned < 1 ){` |
|       - | 5638 | `			/* size <= 0 -> ValueError */` |
|       5 | 5639 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5640 | `				"ValueError",` |
|       - | 5641 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5642 | `				);` |
|       - | 5643 | `		}` |
|      34 | 5644 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5645 | `	}` |
|      34 | 5646 | `	if( nSize >= pMap->nEntry ){` |
|       - | 5647 | `		/* Return the whole array */` |
|       3 | 5648 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 5649 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5650 | `		return PH7_OK;` |
|       - | 5651 | `	}` |
|      32 | 5652 | `	bPreserve = 0;` |
|      32 | 5653 | `	if( nArg > 2 ){` |
|       - | 5654 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5655 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5656 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5657 | `		 * normally, matching PHP behaviour. */` |
|      45 | 5658 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 5659 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 5660 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 5661 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5662 | `				"TypeError",` |
|       - | 5663 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 5664 | `				ph7_type_name(apArg[2])` |
|       - | 5665 | `				);` |
|       - | 5666 | `		}` |
|      21 | 5667 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 5668 | `	}` |
|       - | 5669 | `	/* Start processing */` |
|      27 | 5670 | `	pEntry = pMap->pFirst;` |
|      27 | 5671 | `	nChunk = 0;` |
|      27 | 5672 | `	pChunk = 0;` |
|      27 | 5673 | `	n = pMap->nEntry;` |
|      56 | 5674 | `	for( ;; ){` |
|     113 | 5675 | `		if( n < 1 ){` |
|       - | 5676 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5677 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5678 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5679 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5680 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5681 | `			 * exists. */` |
|      27 | 5682 | `			if( pChunk ){` |
|      27 | 5683 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 5684 | `			}` |
|      27 | 5685 | `			break;` |
|       - | 5686 | `		}` |
|      87 | 5687 | `		if( nChunk < 1 ){` |
|      71 | 5688 | `			if( pChunk ){` |
|       - | 5689 | `				/* Put the first chunk */` |
|      45 | 5690 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 5691 | `			}` |
|       - | 5692 | `			/* Create a new dimension */` |
|      71 | 5693 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5694 | `												   * will be automatically released as soon we return` |
|       - | 5695 | `												   * from this function */` |
|      71 | 5696 | `			if( pChunk == 0 ){` |
|     ! 0 | 5697 | `				break;` |
|       - | 5698 | `			}` |
|      71 | 5699 | `			nChunk = nSize;` |
|      35 | 5700 | `		}` |
|       - | 5701 | `		/* Insert the entry */` |
|      87 | 5702 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5703 | `		/* Point to the next entry */` |
|      87 | 5704 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 5705 | `		nChunk--;` |
|      87 | 5706 | `		n--;` |
|       1 | 5707 | `	}` |
|       - | 5708 | `	/* Return the multidimensional array */` |
|      27 | 5709 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5710 | `	return PH7_OK;` |
|      23 | 5711 |  |
|       - | 5712 | `/*` |
|       - | 5713 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5714 | ` *  Pad array to the specified length with a value.` |
|       - | 5715 | ` * $input` |
|       - | 5716 | ` *   Initial array of values to pad.` |
|       - | 5717 | ` * $pad_size` |
|       - | 5718 | ` *   New size of the array.` |
|       - | 5719 | ` * $pad_value` |
|       - | 5720 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5721 | ` */` |
|      28 | 5722 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5723 |  |
|       - | 5724 | `	ph7_hashmap *pMap;` |
|       - | 5725 | `	ph7_value *pArray;` |
|       - | 5726 | `	int nEntry;` |
|      30 | 5727 | `	if( nArg != 3 ){` |
|      10 | 5728 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5729 | `			"ArgumentCountError",` |
|       - | 5730 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 5731 | `			nArg` |
|       - | 5732 | `			);` |
|       - | 5733 | `	}` |
|      24 | 5734 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5735 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5736 | `			"TypeError",` |
|       - | 5737 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5738 | `			ph7_type_name(apArg[0])` |
|       - | 5739 | `			);` |
|       - | 5740 | `	}` |
|       - | 5741 | `	/* Create a new array */` |
|      21 | 5742 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 5743 | `	if( pArray == 0 ){` |
|     ! 0 | 5744 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5745 | `		return PH7_OK;` |
|       - | 5746 | `	}` |
|       - | 5747 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 5748 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5749 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 5750 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 5751 | `	if( nEntry < 0 ){` |
|       9 | 5752 | `		nEntry = -nEntry;` |
|       9 | 5753 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 5754 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5755 | `			/* Insert given items first */` |
|      17 | 5756 | `			while( nEntry > 0 ){` |
|      13 | 5757 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 5758 | `				nEntry--;` |
|       1 | 5759 | `			}` |
|       - | 5760 | `			/* Merge the two arrays */` |
|       5 | 5761 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5762 | `		}else{` |
|       5 | 5763 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5764 | `		}` |
|      17 | 5765 | `	}else if( nEntry > 0 ){` |
|      11 | 5766 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 5767 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5768 | `			/* Merge the two arrays first */` |
|       7 | 5769 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5770 | `			/* Insert given items */` |
|      25 | 5771 | `			while( nEntry > 0 ){` |
|      19 | 5772 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 5773 | `				nEntry--;` |
|       1 | 5774 | `			}` |
|       4 | 5775 | `		}else{` |
|       5 | 5776 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5777 | `		}` |
|       6 | 5778 | `	}else{` |
|       - | 5779 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 5780 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5781 | `	}` |
|       - | 5782 | `	/* Return the new array */` |
|      21 | 5783 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 5784 | `	return PH7_OK;` |
|      16 | 5785 |  |
|       - | 5786 | `/*` |
|       - | 5787 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5788 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5789 | ` * Parameters` |
|       - | 5790 | ` * $array` |
|       - | 5791 | ` *   The array in which elements are replaced.` |
|       - | 5792 | ` * $array1` |
|       - | 5793 | ` *   The array from which elements will be extracted.` |
|       - | 5794 | ` * ....` |
|       - | 5795 | ` *  More arrays from which elements will be extracted.` |
|       - | 5796 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5797 | ` * Return` |
|       - | 5798 | ` *  Returns an array, or NULL if an error occurs.` |
|       - | 5799 | ` */` |
|       2 | 5800 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5801 |  |
|       - | 5802 | `	ph7_hashmap *pMap;` |
|       - | 5803 | `	ph7_value *pArray;` |
|       - | 5804 | `	int i;` |
|       3 | 5805 | `	if( nArg < 1 ){` |
|       - | 5806 | `		/* Invalid arguments,return NULL */` |
|     ! 0 | 5807 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5808 | `		return PH7_OK;` |
|       - | 5809 | `	}` |
|       - | 5810 | `	/* Create a new array */` |
|       3 | 5811 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5812 | `	if( pArray == 0 ){` |
|     ! 0 | 5813 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5814 | `		return PH7_OK;` |
|       - | 5815 | `	}` |
|       - | 5816 | `	/* Perform the requested operation */` |
|       7 | 5817 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       5 | 5818 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|     ! 0 | 5819 | `			continue;` |
|       - | 5820 | `		}` |
|       - | 5821 | `		/* Point to the internal representation of the input hashmap */` |
|       5 | 5822 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       5 | 5823 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5824 | `	}` |
|       - | 5825 | `	/* Return the new array */` |
|       3 | 5826 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5827 | `	return PH7_OK;` |
|       2 | 5828 |  |
|       - | 5829 | `/*` |
|       - | 5830 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 5831 | ` *  Filters elements of an array using a callback function.` |
|       - | 5832 | ` * Parameters` |
|       - | 5833 | ` *  $input` |
|       - | 5834 | ` *    The array to iterate over` |
|       - | 5835 | ` * $callback` |
|       - | 5836 | ` *    The callback function to use` |
|       - | 5837 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 5838 | ` *    will be removed.` |
|       - | 5839 | ` * Return` |
|       - | 5840 | ` *  The filtered array.` |
|       - | 5841 | ` */` |
|      18 | 5842 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5843 |  |
|       - | 5844 | `	ph7_hashmap_node *pEntry;` |
|       - | 5845 | `	ph7_hashmap *pMap;` |
|       - | 5846 | `	ph7_value *pArray;` |
|       - | 5847 | `	ph7_value sResult;   /* Callback result */` |
|       - | 5848 | `	ph7_value *pValue;` |
|       - | 5849 | `	sxi32 rc;` |
|       - | 5850 | `	int keep;` |
|       - | 5851 | `	sxu32 n;` |
|      20 | 5852 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5853 | `		/* Invalid arguments,return NULL */` |
|       5 | 5854 | `		ph7_result_null(pCtx);` |
|       5 | 5855 | `		return PH7_OK;` |
|       - | 5856 | `	}` |
|       - | 5857 | `	/* Create a new array */` |
|      16 | 5858 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 5859 | `	if( pArray == 0 ){` |
|     ! 0 | 5860 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5861 | `		return PH7_OK;` |
|       - | 5862 | `	}` |
|       - | 5863 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 5864 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 5865 | `	pEntry = pMap->pFirst;` |
|      16 | 5866 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 5867 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5868 | `	/* Perform the requested operation */` |
|      66 | 5869 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5870 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 5871 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 5872 | `		if( pValue == 0 ){` |
|       - | 5873 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5874 | `			keep = FALSE;` |
|      54 | 5875 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5876 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 5877 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 5878 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 5879 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 5880 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5881 | `					int len;` |
|       3 | 5882 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 5883 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5884 | `						"TypeError",` |
|       - | 5885 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 5886 | `						zName` |
|       - | 5887 | `						);` |
|     ! 0 | 5888 | `				}else{` |
|     ! 0 | 5889 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5890 | `						"TypeError",` |
|       - | 5891 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 5892 | `						ph7_type_name(apArg[1])` |
|       - | 5893 | `						);` |
|       - | 5894 | `				}` |
|       - | 5895 | `			}` |
|      23 | 5896 | `			keep = FALSE;` |
|      23 | 5897 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 5898 | `			if( rc == SXRET_OK ){` |
|       - | 5899 | `				/* Perform a boolean cast */` |
|      23 | 5900 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 5901 | `			}` |
|      23 | 5902 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 5903 | `		}else{` |
|       - | 5904 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 5905 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 5906 | `			 * the case where the callback argument is missing entirely.` |
|       - | 5907 | `			 */` |
|      29 | 5908 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 5909 | `		}` |
|      51 | 5910 | `		if( keep ){` |
|       - | 5911 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 5912 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5913 | `		}` |
|       - | 5914 | `		/* Point to the next entry */` |
|      51 | 5915 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 5916 | `	}` |
|      13 | 5917 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5918 | `	return PH7_OK;` |
|      11 | 5919 |  |
|       - | 5920 | `/*` |
|       - | 5921 | ` * array array_map(callback $callback,array $arr1)` |
|       - | 5922 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5923 | ` * Parameters` |
|       - | 5924 | ` *  $callback` |
|       - | 5925 | ` *   Callback function to run for each element in each array.` |
|       - | 5926 | ` * $arr1` |
|       - | 5927 | ` *   An array to run through the callback function.` |
|       - | 5928 | ` * Return` |
|       - | 5929 | ` *  Returns an array containing all the elements of arr1 after applying` |
|       - | 5930 | ` *  the callback function to each one.` |
|       - | 5931 | ` * NOTE:` |
|       - | 5932 | ` *  array_map() passes only a single value to the callback.` |
|       - | 5933 | ` */` |
|      10 | 5934 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5935 |  |
|       - | 5936 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5937 | `	ph7_hashmap_node *pEntry;` |
|       - | 5938 | `	ph7_hashmap *pMap;` |
|       - | 5939 | `	sxu32 n;` |
|      11 | 5940 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 5941 | `		/* Invalid arguments,return NULL */` |
|       5 | 5942 | `		ph7_result_null(pCtx);` |
|       5 | 5943 | `		return PH7_OK;` |
|       - | 5944 | `	}` |
|       - | 5945 | `	/* Create a new array */` |
|       7 | 5946 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5947 | `	if( pArray == 0 ){` |
|     ! 0 | 5948 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5949 | `		return PH7_OK;` |
|       - | 5950 | `	}` |
|       - | 5951 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5952 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       7 | 5953 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       7 | 5954 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 5955 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 5956 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 5957 | `	/* Perform the requested operation */` |
|       7 | 5958 | `	pEntry = pMap->pFirst;` |
|      21 | 5959 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5960 | `		/* Extrcat the node value */` |
|      15 | 5961 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      15 | 5962 | `		if( pValue ){` |
|       - | 5963 | `			sxi32 rc;` |
|       - | 5964 | `			/* Invoke the supplied callback */` |
|      15 | 5965 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 5966 | `			/* Extract the node key */` |
|      15 | 5967 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      15 | 5968 | `			if( rc != SXRET_OK ){` |
|       - | 5969 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 5970 | `				ph7_array_add_elem(pArray,&sKey,pValue); /* Keep the same value */` |
|     ! 0 | 5971 | `			}else{` |
|       - | 5972 | `				/* Insert the callback return value */` |
|      15 | 5973 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 5974 | `			}` |
|      15 | 5975 | `			PH7_MemObjRelease(&sKey);` |
|      15 | 5976 | `			PH7_MemObjRelease(&sResult);` |
|       7 | 5977 | `		}` |
|       - | 5978 | `		/* Point to the next entry */` |
|      15 | 5979 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 5980 | `	}` |
|       7 | 5981 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 5982 | `	return PH7_OK;` |
|       6 | 5983 |  |
|       - | 5984 | `/*` |
|       - | 5985 | ` * value array_reduce(array $input,callback $function[, value $initial = NULL ])` |
|       - | 5986 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 5987 | ` * Parameters` |
|       - | 5988 | ` *  $input` |
|       - | 5989 | ` *   The input array.` |
|       - | 5990 | ` *  $function` |
|       - | 5991 | ` *  The callback function.` |
|       - | 5992 | ` * $initial` |
|       - | 5993 | ` *  If the optional initial is available, it will be used at the beginning` |
|       - | 5994 | ` *  of the process, or as a final result in case the array is empty.` |
|       - | 5995 | ` * Return` |
|       - | 5996 | ` *  Returns the resulting value.` |
|       - | 5997 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 5998 | ` */` |
|       4 | 5999 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6000 |  |
|       - | 6001 | `	ph7_hashmap_node *pEntry;` |
|       - | 6002 | `	ph7_hashmap *pMap;` |
|       - | 6003 | `	ph7_value *pValue;` |
|       - | 6004 | `	ph7_value sResult;` |
|       - | 6005 | `	sxu32 n;` |
|       5 | 6006 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6007 | `		/* Invalid/Missing arguments,return NULL */` |
|     ! 0 | 6008 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6009 | `		return PH7_OK;` |
|       - | 6010 | `	}` |
|       - | 6011 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 6012 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6013 | `	/* Assume a NULL initial value */` |
|       5 | 6014 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       5 | 6015 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       5 | 6016 | `	if( nArg > 2 ){` |
|       - | 6017 | `		/* Set the initial value */` |
|       5 | 6018 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       2 | 6019 | `	}` |
|       - | 6020 | `	/* Perform the requested operation */` |
|       5 | 6021 | `	pEntry = pMap->pFirst;` |
|      19 | 6022 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6023 | `		/* Extract the node value */` |
|      15 | 6024 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6025 | `		/* Invoke the supplied callback */` |
|      15 | 6026 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6027 | `		/* Point to the next entry */` |
|      15 | 6028 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 6029 | `	}` |
|       5 | 6030 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       5 | 6031 | `	PH7_MemObjRelease(&sResult);` |
|       5 | 6032 | `	return PH7_OK;` |
|       3 | 6033 |  |
|       - | 6034 | `/*` |
|       - | 6035 | ` * bool array_walk(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6036 | ` *  Apply a user function to every member of an array.` |
|       - | 6037 | ` * Parameters` |
|       - | 6038 | ` *  $array` |
|       - | 6039 | ` *   The input array.` |
|       - | 6040 | ` * $funcname` |
|       - | 6041 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6042 | ` *  the first, and the key/index second.` |
|       - | 6043 | ` * Note:` |
|       - | 6044 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6045 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6046 | ` *  be made in the original array itself.` |
|       - | 6047 | ` * $userdata` |
|       - | 6048 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6049 | ` *  to the callback funcname.` |
|       - | 6050 | ` * Return` |
|       - | 6051 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6052 | ` */` |
|      12 | 6053 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6054 |  |
|       - | 6055 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6056 | `	ph7_hashmap_node *pEntry;` |
|       - | 6057 | `	ph7_hashmap *pMap;` |
|       - | 6058 | `	sxi32 rc;` |
|       - | 6059 | `	sxu32 n;` |
|      13 | 6060 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6061 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6062 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6063 | `		return PH7_OK;` |
|       - | 6064 | `	}` |
|      13 | 6065 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6066 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6067 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 6068 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      13 | 6069 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6070 | `	/* Perform the desired operation */` |
|      13 | 6071 | `	pEntry = pMap->pFirst;` |
|      41 | 6072 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6073 | `		/* Extract the node value */` |
|      29 | 6074 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      29 | 6075 | `		if( pValue ){` |
|       - | 6076 | `			/* Extract the entry key */` |
|      29 | 6077 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6078 | `			/* Invoke the supplied callback */` |
|      29 | 6079 | `			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      29 | 6080 | `			PH7_MemObjRelease(&sKey);` |
|      29 | 6081 | `			if( rc != SXRET_OK ){` |
|       - | 6082 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 6083 | `				ph7_result_bool(pCtx,0); /* return FALSE */` |
|     ! 0 | 6084 | `				return PH7_OK;` |
|       - | 6085 | `			}` |
|      14 | 6086 | `		}` |
|       - | 6087 | `		/* Point to the next entry */` |
|      29 | 6088 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6089 | `	}` |
|       - | 6090 | `	/* All done,return TRUE */` |
|      13 | 6091 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6092 | `	return PH7_OK;` |
|       7 | 6093 |  |
|       - | 6094 | `/*` |
|       - | 6095 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6096 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6097 | ` */` |
|       6 | 6098 | `static int HashmapWalkRecursive(` |
|       - | 6099 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6100 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6101 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6102 | `	int iNest             /* Nesting level */` |
|       - | 6103 | `	)` |
|       1 | 6104 |  |
|       - | 6105 | `	ph7_hashmap_node *pEntry;` |
|       - | 6106 | `	ph7_value *pValue,sKey;` |
|       - | 6107 | `	sxi32 rc;` |
|       - | 6108 | `	sxu32 n;` |
|       - | 6109 | `	/* Iterate throw hashmap entries */` |
|       7 | 6110 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 6111 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 6112 | `	pEntry = pMap->pFirst;` |
|      17 | 6113 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6114 | `		/* Extract the node value */` |
|      11 | 6115 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      11 | 6116 | `		if( pValue ){` |
|      11 | 6117 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 6118 | `				if( iNest < 32 ){` |
|       - | 6119 | `					/* Recurse */` |
|       5 | 6120 | `					iNest++;` |
|       5 | 6121 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|       5 | 6122 | `					iNest--;` |
|       2 | 6123 | `				}` |
|       3 | 6124 | `			}else{` |
|       - | 6125 | `				/* Extract the node key */` |
|       7 | 6126 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6127 | `				/* Invoke the supplied callback */` |
|       7 | 6128 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|       7 | 6129 | `				PH7_MemObjRelease(&sKey);` |
|       7 | 6130 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6131 | `					return rc;` |
|       - | 6132 | `				}` |
|       - | 6133 | `			}` |
|       5 | 6134 | `		}` |
|       - | 6135 | `		/* Point to the next entry */` |
|      11 | 6136 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 6137 | `	}` |
|       7 | 6138 | `	return SXRET_OK;` |
|       4 | 6139 |  |
|       - | 6140 | `/*` |
|       - | 6141 | ` * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6142 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6143 | ` * Parameters` |
|       - | 6144 | ` *  $array` |
|       - | 6145 | ` *   The input array.` |
|       - | 6146 | ` * $funcname` |
|       - | 6147 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6148 | ` *  the first, and the key/index second.` |
|       - | 6149 | ` * Note:` |
|       - | 6150 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6151 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6152 | ` *  be made in the original array itself.` |
|       - | 6153 | ` * $userdata` |
|       - | 6154 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6155 | ` *  to the callback funcname.` |
|       - | 6156 | ` * Return` |
|       - | 6157 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6158 | ` */` |
|       2 | 6159 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6160 |  |
|       - | 6161 | `	ph7_hashmap *pMap;` |
|       - | 6162 | `	sxi32 rc;` |
|       3 | 6163 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6164 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6165 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6166 | `		return PH7_OK;` |
|       - | 6167 | `	}` |
|       - | 6168 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 6169 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6170 | `	/* Perform the desired operation */` |
|       3 | 6171 | `	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6172 | `	/* All done */` |
|       3 | 6173 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|       3 | 6174 | `	return PH7_OK;` |
|       2 | 6175 |  |
|       - | 6176 | `/*` |
|       - | 6177 | ` * Table of hashmap functions.` |
|       - | 6178 | ` */` |
|       - | 6179 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6180 | `	{"count",             ph7_hashmap_count },` |
|       - | 6181 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6182 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6183 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6184 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6185 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6186 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6187 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6188 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6189 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6190 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6191 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6192 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6193 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6194 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6195 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6196 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6197 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6198 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6199 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6200 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6201 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6202 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6203 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6204 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6205 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6206 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6207 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6208 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6209 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6210 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6211 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6212 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6213 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6214 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6215 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6216 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6217 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6218 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6219 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6220 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6221 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6222 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6223 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6224 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6225 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6226 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6227 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6228 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6229 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6230 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6231 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6232 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6233 | `	{"current",           ph7_hashmap_current },` |
|       - | 6234 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6235 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6236 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6237 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6238 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6239 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6240 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6241 | `};` |
|       - | 6242 | `/*` |
|       - | 6243 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6244 | ` */` |
|    1360 | 6245 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6246 |  |
|       - | 6247 | `	sxu32 n;` |
|   84322 | 6248 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|   82962 | 6249 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   41482 | 6250 | `	}` |
|    1362 | 6251 |  |
|       - | 6252 | `/*` |
|       - | 6253 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6254 | ` * the BLOB given as the first argument.` |
|       - | 6255 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6256 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6257 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6258 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6259 | ` */` |
|      28 | 6260 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6261 |  |
|       - | 6262 | `	ph7_hashmap_node *pEntry;` |
|       - | 6263 | `	ph7_value *pObj;` |
|      30 | 6264 | `	sxu32 n = 0;` |
|       - | 6265 | `	int isRef;` |
|       - | 6266 | `	sxi32 rc;` |
|       - | 6267 | `	int i;` |
|      30 | 6268 | `	if( nDepth > 31 ){` |
|       - | 6269 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6270 | `		/* Nesting limit reached */` |
|     ! 0 | 6271 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6272 | `		if( ShowType ){` |
|     ! 0 | 6273 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6274 | `		}` |
|     ! 0 | 6275 | `		return SXERR_LIMIT;` |
|       - | 6276 | `	}` |
|       - | 6277 | `	/* Point to the first inserted entry */` |
|      30 | 6278 | `	pEntry = pMap->pFirst;` |
|      30 | 6279 | `	rc = SXRET_OK;` |
|      30 | 6280 | `	if( !ShowType ){` |
|      15 | 6281 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6282 | `	}` |
|       - | 6283 | `	/* Total entries */` |
|      30 | 6284 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6285 | `#ifdef __WINNT__` |
|       2 | 6286 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6287 | `#else` |
|      28 | 6288 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6289 | `#endif` |
|      65 | 6290 | `	for(;;){` |
|     132 | 6291 | `		if( n >= pMap->nEntry ){` |
|      30 | 6292 | `			break;` |
|       - | 6293 | `		}` |
|     206 | 6294 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     104 | 6295 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      53 | 6296 | `		}` |
|       - | 6297 | `		/* Dump key */` |
|     104 | 6298 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      37 | 6299 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      19 | 6300 | `		}else{` |
|     101 | 6301 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6302 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6303 | `		}` |
|       - | 6304 | `#ifdef __WINNT__` |
|       2 | 6305 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6306 | `#else` |
|     102 | 6307 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6308 | `#endif` |
|       - | 6309 | `		/* Dump node value */` |
|     104 | 6310 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     104 | 6311 | `		isRef = 0;` |
|     104 | 6312 | `		if( pObj ){` |
|     104 | 6313 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6314 | `				/* Referenced object */` |
|     ! 0 | 6315 | `				isRef = 1;` |
|     ! 0 | 6316 | `			}` |
|     104 | 6317 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     104 | 6318 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6319 | `				break;` |
|       - | 6320 | `			}` |
|      51 | 6321 | `		}` |
|       - | 6322 | `		/* Point to the next entry */` |
|     104 | 6323 | `		n++;` |
|     104 | 6324 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6325 | `	}` |
|      58 | 6326 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      30 | 6327 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      16 | 6328 | `	}` |
|      30 | 6329 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      30 | 6330 | `	return rc;` |
|      16 | 6331 |  |
|       - | 6332 | `/*` |
|       - | 6333 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6334 | ` * retrieved entry.` |
|       - | 6335 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6336 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6337 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6338 | ` * a value different from PH7_OK.` |
|       - | 6339 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6340 | ` */` |
|   19400 | 6341 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6342 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6343 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6344 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6345 | `	)` |
|       2 | 6346 |  |
|       - | 6347 | `	ph7_hashmap_node *pEntry;` |
|       - | 6348 | `	ph7_value sKey,sValue;` |
|       - | 6349 | `	sxi32 rc;` |
|       - | 6350 | `	sxu32 n;` |
|       - | 6351 | `	/* Initialize walker parameter */` |
|   19402 | 6352 | `	rc = SXRET_OK;` |
|   19402 | 6353 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   19402 | 6354 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   19402 | 6355 | `	n = pMap->nEntry;` |
|   19402 | 6356 | `	pEntry = pMap->pFirst;` |
|       - | 6357 | `	/* Start the iteration process */` |
|   51634 | 6358 | `	for(;;){` |
|  103270 | 6359 | `		if( n < 1 ){` |
|   19402 | 6360 | `			break;` |
|       - | 6361 | `		}` |
|       - | 6362 | `		/* Extract a copy of the key and a copy the current value */` |
|   83870 | 6363 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   83870 | 6364 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6365 | `		/* Invoke the user callback */` |
|   83870 | 6366 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6367 | `		/* Release the copy of the key and the value */` |
|   83870 | 6368 | `		PH7_MemObjRelease(&sKey);` |
|   83870 | 6369 | `		PH7_MemObjRelease(&sValue);` |
|   83870 | 6370 | `		if( rc != PH7_OK ){` |
|       - | 6371 | `			/* Callback request an operation abort */` |
|     ! 0 | 6372 | `			return SXERR_ABORT;` |
|       - | 6373 | `		}` |
|       - | 6374 | `		/* Point to the next entry */` |
|   83870 | 6375 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   83870 | 6376 | `		n--;` |
|       2 | 6377 | `	}` |
|       - | 6378 | `	/* All done */` |
|   19402 | 6379 | `	return SXRET_OK;` |
|    9702 | 6380 |  |
|       - | 6381 |  |
