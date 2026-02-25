# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2344/2910 lines (80.55%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* This file implement generic hashmaps known as 'array' in the PHP world */` |
|      - |    8 | `/* Allowed node types */` |
|      - |    9 | `#define HASHMAP_INT_NODE   1  /* Node with an int [i.e: 64-bit integer] key */` |
|      - |   10 | `#define HASHMAP_BLOB_NODE  2  /* Node with a string/BLOB key */` |
|      - |   11 | `/* Node control flags */` |
|      - |   12 | `#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node hold a reference to a foreign ph7_value` |
|      - |   13 | `                                        * [i.e: array(&var)/$a[] =& $var ]` |
|      - |   14 | `										*/` |
|      - |   15 | `/*` |
|      - |   16 | ` * Default hash function for int [i.e; 64-bit integer] keys.` |
|      - |   17 | ` */` |
| 557962 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|      2 |   19 |  |
| 557964 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|      2 |   21 |  |
|      - |   22 | `/*` |
|      - |   23 | ` * Default hash function for string/BLOB keys.` |
|      - |   24 | ` */` |
| 198334 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|      2 |   26 |  |
| 198336 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|      - |   28 | `	unsigned char *zEnd;` |
| 198336 |   29 | `	sxu32 nH = 5381;` |
| 198336 |   30 | `	zEnd = &zIn[nLen];` |
| 231261 |   31 | `	for(;;){` |
| 462524 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 419906 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 381370 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 305660 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|      2 |   36 | `	}` |
| 198336 |   37 | `	return nH;` |
|      2 |   38 |  |
|      - |   39 | `/*` |
|      - |   40 | ` * Return the total number of entries in a given hashmap.` |
|      - |   41 | ` * If bRecurisve is set to TRUE then recurse on hashmap entries.` |
|      - |   42 | ` * If the nesting limit is reached,this function abort immediately.` |
|      - |   43 | ` */` |
|    680 |   44 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)` |
|      2 |   45 |  |
|    682 |   46 | `	sxi64 iCount = 0;` |
|    682 |   47 | `	if( !bRecursive ){` |
|    406 |   48 | `		iCount = pMap->nEntry;` |
|    204 |   49 | `	}else{` |
|      - |   50 | `		/* Recursive hashmap walk */` |
|    277 |   51 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|      - |   52 | `		ph7_value *pElem;` |
|    277 |   53 | `		sxu32 n = 0;` |
|    331 |   54 | `		for(;;){` |
|    663 |   55 | `			if( n >= pMap->nEntry ){` |
|    273 |   56 | `				break;` |
|      - |   57 | `			}` |
|      - |   58 | `			/* Point to the element value */` |
|    391 |   59 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|    391 |   60 | `			if( pElem ){` |
|    391 |   61 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|    251 |   62 | `					if( iRecCount > 31 ){` |
|      - |   63 | `						/* Nesting limit reached */` |
|      5 |   64 | `						return iCount;` |
|      - |   65 | `					}` |
|      - |   66 | `					/* Recurse */` |
|    247 |   67 | `					iRecCount++;` |
|    247 |   68 | `					iCount += HashmapCount((ph7_hashmap *)pElem->x.pOther,TRUE,iRecCount);` |
|    247 |   69 | `					iRecCount--;` |
|    123 |   70 | `				}` |
|    193 |   71 | `			}` |
|      - |   72 | `			/* Point to the next entry */` |
|    387 |   73 | `			pEntry = pEntry->pNext;` |
|    387 |   74 | `			++n;` |
|      1 |   75 | `		}` |
|      - |   76 | `		/* Update count */` |
|    273 |   77 | `		iCount += pMap->nEntry;` |
|      - |   78 | `	}` |
|    678 |   79 | `	return iCount;` |
|    342 |   80 |  |
|      - |   81 | `/*` |
|      - |   82 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|      - |   83 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|      - |   84 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|      - |   85 | ` */` |
| 505254 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|      2 |   87 |  |
|      - |   88 | `	ph7_hashmap_node *pNode;` |
|      - |   89 | `	/* Allocate a new node */` |
| 505256 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 505256 |   91 | `	if( pNode == 0 ){` |
|    ! 0 |   92 | `		return 0;` |
|      - |   93 | `	}` |
|      - |   94 | `	/* Zero the stucture */` |
| 505256 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|      - |   96 | `	/* Fill in the structure */` |
| 505256 |   97 | `	pNode->pMap  = &(*pMap);` |
| 505256 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 505256 |   99 | `	pNode->nHash = nHash;` |
| 505256 |  100 | `	pNode->xKey.iKey = iKey;` |
| 505256 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 505256 |  102 | `	return pNode;` |
| 252629 |  103 |  |
|      - |  104 | `/*` |
|      - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|      - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|      - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|      - |  108 | ` */` |
|  69282 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|      2 |  110 |  |
|      - |  111 | `	ph7_hashmap_node *pNode;` |
|      - |  112 | `	/* Allocate a new node */` |
|  69284 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  69284 |  114 | `	if( pNode == 0 ){` |
|    ! 0 |  115 | `		return 0;` |
|      - |  116 | `	}` |
|      - |  117 | `	/* Zero the stucture */` |
|  69284 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|      - |  119 | `	/* Fill in the structure */` |
|  69284 |  120 | `	pNode->pMap  = &(*pMap);` |
|  69284 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  69284 |  122 | `	pNode->nHash = nHash;` |
|  69284 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  69284 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  69284 |  125 | `	pNode->nValIdx = nValIdx;` |
|  69284 |  126 | `	return pNode;` |
|  34643 |  127 |  |
|      - |  128 | `/*` |
|      - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|      - |  130 | ` */` |
| 574536 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|      2 |  132 |  |
|      - |  133 | `	/* Link */` |
| 574538 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 417954 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 417954 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 208976 |  137 | `	}` |
| 574538 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|      - |  139 | `	/* Link to the map list */` |
| 574538 |  140 | `	if( pMap->pFirst == 0 ){` |
|  30334 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|      - |  142 | `		/* Point to the first inserted node */` |
|  30334 |  143 | `		pMap->pCur = pNode;` |
|  15168 |  144 | `	}else{` |
| 544206 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|      - |  146 | `	}` |
| 574538 |  147 | `	++pMap->nEntry;` |
| 574538 |  148 |  |
|      - |  149 | `/*` |
|      - |  150 | ` * Unlink a node from the hashmap.` |
|      - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|      - |  152 | ` */` |
|   4924 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|      2 |  154 |  |
|   4926 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|   4926 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|      - |  157 | `	/* Unlink from the corresponding bucket */` |
|   4926 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|   4502 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|   2252 |  160 | `	}else{` |
|    425 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|      - |  162 | `	}` |
|   4926 |  163 | `	if( pNode->pNextCollide ){` |
|   3735 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|   1867 |  165 | `	}` |
|   4926 |  166 | `	if( pMap->pFirst == pNode ){` |
|     56 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|     27 |  168 | `	}` |
|   4926 |  169 | `	if( pMap->pCur == pNode ){` |
|      - |  170 | `		/* Advance the node cursor */` |
|     58 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|     28 |  172 | `	}` |
|      - |  173 | `	/* Unlink from the map list */` |
|   4926 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|   4926 |  175 | `	if( bRestore ){` |
|      - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     26 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|      - |  178 | `		/* Restore to the freelist */` |
|     26 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     26 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|     12 |  181 | `		}` |
|     12 |  182 | `	}` |
|   4926 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|   4881 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|   2440 |  185 | `	}` |
|   4926 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|   4926 |  187 | `	pMap->nEntry--;` |
|   4926 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|      - |  189 | `		/* Free the hash-bucket */` |
|     26 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     26 |  191 | `		pMap->apBucket = 0;` |
|     26 |  192 | `		pMap->nSize = 0;` |
|     26 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|     12 |  194 | `	}` |
|   4926 |  195 |  |
|      - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|      - |  197 | `/*` |
|      - |  198 | ` * Grow the hash-table and rehash all entries.` |
|      - |  199 | ` */` |
| 574536 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|      2 |  201 |  |
| 574538 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|  33334 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|      - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|  33334 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|      - |  206 | `		sxu32 nBucket;` |
|      - |  207 | `		sxu32 n;` |
|  33334 |  208 | `		if( nNew < 1 ){` |
|  30334 |  209 | `			nNew = 16;` |
|  15166 |  210 | `		}` |
|      - |  211 | `		/* Allocate a new bucket */` |
|  33334 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|  33334 |  213 | `		if( apNew == 0 ){` |
|    ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|    ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|      - |  216 | `			}` |
|      - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|    ! 0 |  218 | `			return SXRET_OK;` |
|      - |  219 | `		}` |
|      - |  220 | `		/* Zero the table */` |
|  33334 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|      - |  222 | `		/* Reflect the change */` |
|  33334 |  223 | `		pMap->apBucket = apNew;` |
|  33334 |  224 | `		pMap->nSize = nNew;` |
|  33334 |  225 | `		if( apOld == 0 ){` |
|      - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|  30334 |  227 | `			return SXRET_OK;` |
|      - |  228 | `		}` |
|      - |  229 | `		/* Rehash old entries */` |
|   3002 |  230 | `		pEntry = pMap->pFirst;` |
|   3002 |  231 | `		n = 0;` |
| 255996 |  232 | `		for( ;; ){` |
| 511994 |  233 | `			if( n >= pMap->nEntry ){` |
|   3002 |  234 | `				break;` |
|      - |  235 | `			}` |
|      - |  236 | `			/* Clear the old collision link */` |
| 508994 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|      - |  238 | `			/* Link to the new bucket */` |
| 508994 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 508994 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 228452 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 228452 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 114225 |  243 | `			}` |
| 508994 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|      - |  245 | `			/* Point to the next entry */` |
| 508994 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 508994 |  247 | `			n++;` |
|      2 |  248 | `		}` |
|      - |  249 | `		/* Free the old table */` |
|   3002 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|   1500 |  251 | `	}` |
| 544206 |  252 | `	return SXRET_OK;` |
| 287270 |  253 |  |
|      - |  254 | `/*` |
|      - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|      - |  256 | ` * hashmap.` |
|      - |  257 | ` */` |
| 505254 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|      2 |  259 |  |
|      - |  260 | `	ph7_hashmap_node *pNode;` |
|      - |  261 | `	sxu32 nIdx;` |
|      - |  262 | `	sxu32 nHash;` |
|      - |  263 | `	sxi32 rc;` |
| 505256 |  264 | `	if( !isForeign ){` |
|      - |  265 | `		ph7_value *pObj;` |
|      - |  266 | `		/* Reserve a ph7_value for the value */` |
| 505232 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 505232 |  268 | `		if( pObj == 0 ){` |
|    ! 0 |  269 | `			return SXERR_MEM;` |
|      - |  270 | `		}` |
| 505232 |  271 | `		if( pValue ){` |
|      - |  272 | `			/* Duplicate the value */` |
| 505232 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 252615 |  274 | `		}` |
| 505232 |  275 | `		nIdx = pObj->nIdx;` |
| 252617 |  276 | `	}else{` |
|     25 |  277 | `		nIdx = nRefIdx;` |
|      - |  278 | `	}` |
|      - |  279 | `	/* Hash the key */` |
| 505256 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|      - |  281 | `	/* Allocate a new int node */` |
| 505256 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 505256 |  283 | `	if( pNode == 0 ){` |
|    ! 0 |  284 | `		return SXERR_MEM;` |
|      - |  285 | `	}` |
| 505256 |  286 | `	if( isForeign ){` |
|      - |  287 | `		/* Mark as a foregin entry */` |
|     25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     12 |  289 | `	}` |
|      - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 505256 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 505256 |  292 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|    ! 0 |  294 | `		return rc;` |
|      - |  295 | `	}` |
|      - |  296 | `	/* Perform the insertion */` |
| 505256 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|      - |  298 | `	/* Install in the reference table */` |
| 505256 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|      - |  300 | `	/* All done */` |
| 505256 |  301 | `	return SXRET_OK;` |
| 252629 |  302 |  |
|      - |  303 | `/*` |
|      - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|      - |  305 | ` * hashmap.` |
|      - |  306 | ` */` |
|  69282 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|      2 |  308 |  |
|      - |  309 | `	ph7_hashmap_node *pNode;` |
|      - |  310 | `	sxu32 nHash;` |
|      - |  311 | `	sxu32 nIdx;` |
|      - |  312 | `	sxi32 rc;` |
|  69284 |  313 | `	if( !isForeign ){` |
|      - |  314 | `		ph7_value *pObj;` |
|      - |  315 | `		/* Reserve a ph7_value for the value */` |
|  53944 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|  53944 |  317 | `		if( pObj == 0 ){` |
|    ! 0 |  318 | `			return SXERR_MEM;` |
|      - |  319 | `		}` |
|  53944 |  320 | `		if( pValue ){` |
|      - |  321 | `			/* Duplicate the value */` |
|  53944 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|  26971 |  323 | `		}` |
|  53944 |  324 | `		nIdx = pObj->nIdx;` |
|  26973 |  325 | `	}else{` |
|  15342 |  326 | `		nIdx = nRefIdx;` |
|      - |  327 | `	}` |
|      - |  328 | `	/* Hash the key */` |
|  69284 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|      - |  330 | `	/* Allocate a new blob node */` |
|  69284 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  69284 |  332 | `	if( pNode == 0 ){` |
|    ! 0 |  333 | `		return SXERR_MEM;` |
|      - |  334 | `	}` |
|  69284 |  335 | `	if( isForeign ){` |
|      - |  336 | `		/* Mark as a foregin entry */` |
|  15342 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   7670 |  338 | `	}` |
|      - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  69284 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  69284 |  341 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|    ! 0 |  343 | `		return rc;` |
|      - |  344 | `	}` |
|      - |  345 | `	/* Perform the insertion */` |
|  69284 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|      - |  347 | `	/* Install in the reference table */` |
|  69284 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|      - |  349 | `	/* All done */` |
|  69284 |  350 | `	return SXRET_OK;` |
|  34643 |  351 |  |
|      - |  352 | `/*` |
|      - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|      - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|      - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|      - |  356 | ` */` |
|  46234 |  357 | `static sxi32 HashmapLookupIntKey(` |
|      - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|      - |  359 | `	sxi64 iKey,                /* lookup key */` |
|      - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|      - |  361 | `	)` |
|      2 |  362 |  |
|      - |  363 | `	ph7_hashmap_node *pNode;` |
|      - |  364 | `	sxu32 nHash;` |
|  46236 |  365 | `	if( pMap->nEntry < 1 ){` |
|      - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|    299 |  367 | `		return SXERR_NOTFOUND;` |
|      - |  368 | `	}` |
|      - |  369 | `	/* Hash the key first */` |
|  45938 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|      - |  371 | `	/* Point to the appropriate bucket */` |
|  45938 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|      - |  373 | `	/* Perform the lookup */` |
| 411325 |  374 | `	for(;;){` |
| 822652 |  375 | `		if( pNode == 0 ){` |
|  45539 |  376 | `			break;` |
|      - |  377 | `		}` |
| 777311 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 774094 |  379 | `			&& pNode->nHash == nHash` |
| 385739 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|      - |  381 | `				/* Node found */` |
|    400 |  382 | `				if( ppNode ){` |
|    394 |  383 | `					*ppNode = pNode;` |
|    196 |  384 | `				}` |
|    400 |  385 | `				return SXRET_OK;` |
|      - |  386 | `		}` |
|      - |  387 | `		/* Follow the collision link */` |
| 776715 |  388 | `		pNode = pNode->pNextCollide;` |
|      1 |  389 | `	}` |
|      - |  390 | `	/* No such entry */` |
|  45539 |  391 | `	return SXERR_NOTFOUND;` |
|  23119 |  392 |  |
|      - |  393 | `/*` |
|      - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|      - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|      - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|      - |  397 | ` */` |
| 135444 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|      - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|      - |  400 | `	const void *pKey,           /* Lookup key */` |
|      - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|      - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|      - |  403 | `	)` |
|      2 |  404 |  |
|      - |  405 | `	ph7_hashmap_node *pNode;` |
|      - |  406 | `	sxu32 nHash;` |
| 135446 |  407 | `	if( pMap->nEntry < 1 ){` |
|      - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|   6394 |  409 | `		return SXERR_NOTFOUND;` |
|      - |  410 | `	}` |
|      - |  411 | `	/* Hash the key first */` |
| 129054 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|      - |  413 | `	/* Point to the appropriate bucket */` |
| 129054 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|      - |  415 | `	/* Perform the lookup */` |
| 134880 |  416 | `	for(;;){` |
| 269762 |  417 | `		if( pNode == 0 ){` |
|  97266 |  418 | `			break;` |
|      - |  419 | `		}` |
| 188390 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
| 170996 |  421 | `			&& pNode->nHash == nHash` |
| 100642 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|  31790 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|      - |  424 | `				/* Node found */` |
|  31790 |  425 | `				if( ppNode ){` |
|  31778 |  426 | `					*ppNode = pNode;` |
|  15888 |  427 | `				}` |
|  31790 |  428 | `				return SXRET_OK;` |
|      - |  429 | `		}` |
|      - |  430 | `		/* Follow the collision link */` |
| 140710 |  431 | `		pNode = pNode->pNextCollide;` |
|      2 |  432 | `	}` |
|      - |  433 | `	/* No such entry */` |
|  97266 |  434 | `	return SXERR_NOTFOUND;` |
|  67724 |  435 |  |
|      - |  436 | `/*` |
|      - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|      - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|      - |  439 | ` */` |
| 135636 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|      2 |  441 |  |
| 135638 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
| 135638 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
| 135638 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|      - |  445 | `		/* Octal not decimal number */` |
|      5 |  446 | `		return FALSE;` |
|      - |  447 | `	}` |
| 135634 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|    ! 0 |  449 | `		zIn++;` |
|    ! 0 |  450 | `	}` |
|  68147 |  451 | `	for(;;){` |
| 136296 |  452 | `		if( zIn >= zEnd ){` |
|    231 |  453 | `			return TRUE;` |
|      - |  454 | `		}` |
| 136066 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  67703 |  456 | `			break;` |
|      - |  457 | `		}` |
|    663 |  458 | `		zIn++;` |
|      1 |  459 | `	}` |
|      - |  460 | `	/* Key does not look like a decimal number */` |
| 135404 |  461 | `	return FALSE;` |
|  67820 |  462 |  |
|      - |  463 | `/*` |
|      - |  464 | ` * Check if a given key exists in the given hashmap.` |
|      - |  465 | ` * Write a pointer to the target node on success.` |
|      - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - |  467 | ` */` |
|  66518 |  468 | `static sxi32 HashmapLookup(` |
|      - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|      - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|      - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|      - |  472 | `	)` |
|      2 |  473 |  |
|  66520 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|      - |  475 | `	sxi32 rc;` |
|  66520 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  66180 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  478 | `			/* Force a string cast */` |
|    ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|    ! 0 |  480 | `		}` |
|  66180 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|      - |  482 | `			/* Perform a blob lookup */` |
|  66164 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  66164 |  484 | `			goto result;` |
|      - |  485 | `		}` |
|      8 |  486 | `	}` |
|      - |  487 | `	/* Perform an int lookup */` |
|    358 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  489 | `		/* Force an integer cast */` |
|     19 |  490 | `		PH7_MemObjToInteger(pKey);` |
|      9 |  491 | `	}` |
|      - |  492 | `	/* Perform an int lookup */` |
|    358 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|  33259 |  494 | `result:` |
|  66520 |  495 | `	if( rc == SXRET_OK ){` |
|      - |  496 | `		/* Node found */` |
|  32088 |  497 | `		if( ppNode ){` |
|  32072 |  498 | `			*ppNode = pNode;` |
|  16035 |  499 | `		}` |
|  32088 |  500 | `		return SXRET_OK;` |
|      - |  501 | `	}` |
|      - |  502 | `	/* No such entry */` |
|  34434 |  503 | `	return SXERR_NOTFOUND;` |
|  33261 |  504 |  |
|      - |  505 | `/*` |
|      - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|      - |  507 | ` * hashmap.` |
|      - |  508 | ` * If a node with the given key already exists in the database` |
|      - |  509 | ` * then this function overwrite the old value.` |
|      - |  510 | ` */` |
| 559134 |  511 | `static sxi32 HashmapInsert(` |
|      - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|      - |  514 | `	ph7_value *pVal    /* Node value */` |
|      - |  515 | `	)` |
|      2 |  516 |  |
| 559136 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 559136 |  518 | `	sxi32 rc = SXRET_OK;` |
| 559136 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  54150 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  521 | `			/* Force a string cast */` |
|      8 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|      3 |  523 | `		}` |
|  54150 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    252 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|      - |  526 | `				/* Automatic index assign */` |
|     32 |  527 | `				pKey = 0;` |
|     15 |  528 | `			}` |
|    252 |  529 | `			goto IntKey;` |
|      - |  530 | `		}` |
|  80849 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|  26949 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|      - |  533 | `				/* Overwrite the old value */` |
|      - |  534 | `				ph7_value *pElem;` |
|     21 |  535 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|     21 |  536 | `				if( pElem ){` |
|     21 |  537 | `					if( pVal ){` |
|     21 |  538 | `						PH7_MemObjStore(pVal,pElem);` |
|     11 |  539 | `					}else{` |
|      - |  540 | `						/* Nullify the entry */` |
|    ! 0 |  541 | `						PH7_MemObjToNull(pElem);` |
|      - |  542 | `					}` |
|     10 |  543 | `				}` |
|     21 |  544 | `				return SXRET_OK;` |
|      - |  545 | `		}` |
|  53880 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|      - |  547 | `			/* Forbidden */` |
|      3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|      3 |  549 | `			return SXRET_OK;` |
|      - |  550 | `		}` |
|      - |  551 | `		/* Perform a blob-key insertion */` |
|  53878 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|  53878 |  553 | `		return rc;` |
|      - |  554 | `	}` |
| 252493 |  555 | `IntKey:` |
| 505238 |  556 | `	if( pKey ){` |
|  23061 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  558 | `			/* Force an integer cast */` |
|    245 |  559 | `			PH7_MemObjToInteger(pKey);` |
|    122 |  560 | `		}` |
|  23061 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|      - |  562 | `			/* Overwrite the old value */` |
|      - |  563 | `			ph7_value *pElem;` |
|     37 |  564 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|     37 |  565 | `			if( pElem ){` |
|     37 |  566 | `				if( pVal ){` |
|     37 |  567 | `					PH7_MemObjStore(pVal,pElem);` |
|     19 |  568 | `				}else{` |
|      - |  569 | `					/* Nullify the entry */` |
|    ! 0 |  570 | `					PH7_MemObjToNull(pElem);` |
|      - |  571 | `				}` |
|     18 |  572 | `			}` |
|     37 |  573 | `			return SXRET_OK;` |
|      - |  574 | `		}` |
|  23025 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|      - |  576 | `			/* Forbidden */` |
|      3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|      3 |  578 | `			return SXRET_OK;` |
|      - |  579 | `		}` |
|      - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|  23023 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|  23023 |  582 | `		if( rc == SXRET_OK ){` |
|  23023 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|      - |  584 | `				/* Increment the automatic index */` |
|  22799 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|      - |  586 | `				/* Make sure the automatic index is not reserved */` |
|  22799 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|    ! 0 |  588 | `					pMap->iNextIdx++;` |
|    ! 0 |  589 | `				}` |
|  11399 |  590 | `			}` |
|  11511 |  591 | `		}` |
|  11512 |  592 | `	}else{` |
| 482178 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|      - |  594 | `			/* Forbidden */` |
|      3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|      3 |  596 | `			return SXRET_OK;` |
|      - |  597 | `		}` |
|      - |  598 | `		/* Assign an automatic index */` |
| 482176 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 482176 |  600 | `		if( rc == SXRET_OK ){` |
| 482176 |  601 | `			++pMap->iNextIdx;` |
| 241087 |  602 | `		}` |
|      - |  603 | `	}` |
|      - |  604 | `	/* Insertion result */` |
| 505198 |  605 | `	return rc;` |
| 279569 |  606 |  |
|      - |  607 | `/*` |
|      - |  608 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|      - |  609 | ` * hashmap.` |
|      - |  610 | ` * This is insertion by reference so be careful to mark the node` |
|      - |  611 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|      - |  612 | ` * The insertion by reference is triggered when the following` |
|      - |  613 | ` * expression is encountered.` |
|      - |  614 | ` * $var = 10;` |
|      - |  615 | ` *  $a = array(&var);` |
|      - |  616 | ` * OR` |
|      - |  617 | ` *  $a[] =& $var;` |
|      - |  618 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|      - |  619 | ` * over it's contents.` |
|      - |  620 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|      - |  621 | ` * removed when the foreign ph7_value is unset.` |
|      - |  622 | ` * Example:` |
|      - |  623 | ` *  $var = 10;` |
|      - |  624 | ` *  $a[] =& $var;` |
|      - |  625 | ` *  echo count($a).PHP_EOL; //1` |
|      - |  626 | ` *  //Unset the foreign ph7_value now` |
|      - |  627 | ` *  unset($var);` |
|      - |  628 | ` *  echo count($a); //0` |
|      - |  629 | ` * Note that this is a PH7 eXtension.` |
|      - |  630 | ` * Refer to the official documentation for more information.` |
|      - |  631 | ` * If a node with the given key already exists in the database` |
|      - |  632 | ` * then this function overwrite the old value.` |
|      - |  633 | ` */` |
|  15370 |  634 | `static sxi32 HashmapInsertByRef(` |
|      - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|      - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|      - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|      - |  638 | `	)` |
|      2 |  639 |  |
|  15372 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|  15372 |  641 | `	sxi32 rc = SXRET_OK;` |
|  15372 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  15348 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  644 | `			/* Force a string cast */` |
|    ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|    ! 0 |  646 | `		}` |
|  15348 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|      - |  649 | `				/* Automatic index assign */` |
|    ! 0 |  650 | `				pKey = 0;` |
|    ! 0 |  651 | `			}` |
|    ! 0 |  652 | `			goto IntKey;` |
|      - |  653 | `		}` |
|  23021 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   7673 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|      - |  656 | `				/* Overwrite */` |
|      7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|      7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|      - |  659 | `				/* Install in the reference table */` |
|      7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|      7 |  661 | `				return SXRET_OK;` |
|      - |  662 | `		}` |
|      - |  663 | `		/* Perform a blob-key insertion */` |
|  15342 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|  15342 |  665 | `		return rc;` |
|      - |  666 | `	}` |
|     12 |  667 | `IntKey:` |
|     25 |  668 | `	if( pKey ){` |
|      3 |  669 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  670 | `			/* Force an integer cast */` |
|    ! 0 |  671 | `			PH7_MemObjToInteger(pKey);` |
|    ! 0 |  672 | `		}` |
|      3 |  673 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|      - |  674 | `			/* Overwrite */` |
|    ! 0 |  675 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|    ! 0 |  676 | `			pNode->nValIdx = nRefIdx;` |
|      - |  677 | `			/* Install in the reference table */` |
|    ! 0 |  678 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|    ! 0 |  679 | `			return SXRET_OK;` |
|      - |  680 | `		}` |
|      - |  681 | `		/* Perform a 64-bit-int-key insertion */` |
|      3 |  682 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|      3 |  683 | `		if( rc == SXRET_OK ){` |
|      3 |  684 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|      - |  685 | `				/* Increment the automatic index */` |
|      3 |  686 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|      - |  687 | `				/* Make sure the automatic index is not reserved */` |
|      3 |  688 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|    ! 0 |  689 | `					pMap->iNextIdx++;` |
|    ! 0 |  690 | `				}` |
|      1 |  691 | `			}` |
|      1 |  692 | `		}` |
|      2 |  693 | `	}else{` |
|      - |  694 | `		/* Assign an automatic index */` |
|     23 |  695 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|     23 |  696 | `		if( rc == SXRET_OK ){` |
|     23 |  697 | `			++pMap->iNextIdx;` |
|     11 |  698 | `		}` |
|      - |  699 | `	}` |
|      - |  700 | `	/* Insertion result */` |
|     25 |  701 | `	return rc;` |
|   7687 |  702 |  |
|      - |  703 | `/*` |
|      - |  704 | ` * Extract node value.` |
|      - |  705 | ` */` |
| 685456 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|      2 |  707 |  |
|      - |  708 | `	/* Point to the desired object */` |
|      - |  709 | `	ph7_value *pObj;` |
| 685458 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 685458 |  711 | `	return pObj;` |
|      2 |  712 |  |
|      - |  713 | `/*` |
|      - |  714 | ` * Insert a node in the given hashmap.` |
|      - |  715 | ` * If a node with the given key already exists in the database` |
|      - |  716 | ` * then this function overwrite the old value.` |
|      - |  717 | ` */` |
|    156 |  718 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|      1 |  719 |  |
|      - |  720 | `	ph7_value *pObj;` |
|      - |  721 | `	sxi32 rc;` |
|      - |  722 | `	/* Extract the node value */` |
|    157 |  723 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|    157 |  724 | `	if( pObj == 0 ){` |
|    ! 0 |  725 | `		return SXERR_EMPTY;` |
|      - |  726 | `	}` |
|      - |  727 | `	/* Preserve key */` |
|    157 |  728 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|      - |  729 | `		/* Int64 key */` |
|     61 |  730 | `		if( !bPreserve ){` |
|      - |  731 | `			/* Assign an automatic index */` |
|     39 |  732 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|     20 |  733 | `		}else{` |
|     23 |  734 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|      - |  735 | `		}` |
|     31 |  736 | `	}else{` |
|      - |  737 | `		/* Blob key */` |
|     97 |  738 | `		if( !bPreserve ){` |
|      - |  739 | `			/* treat it like an automatically-indexed element, drop the` |
|      - |  740 | `			 * original string key entirely */` |
|     33 |  741 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|     17 |  742 | `		}else{` |
|     97 |  743 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|     32 |  744 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|      - |  745 | `		}` |
|      - |  746 | `	}` |
|    157 |  747 | `	return rc;` |
|     79 |  748 |  |
|      - |  749 | `/*` |
|      - |  750 | ` * Compare two node values.` |
|      - |  751 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|      - |  752 | ` * or < 0 if pRight is greater than pLeft.` |
|      - |  753 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|      - |  754 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|      - |  755 | ` * documenation.` |
|      - |  756 | ` */` |
|  31698 |  757 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|      2 |  758 |  |
|      - |  759 | `	ph7_value sObj1,sObj2;` |
|      - |  760 | `	sxi32 rc;` |
|  31700 |  761 | `	if( pLeft == pRight ){` |
|      - |  762 | `		/*` |
|      - |  763 | `		 * Same node.Refer to the sort() implementation defined` |
|      - |  764 | `		 * below for more information on this sceanario.` |
|      - |  765 | `		 */` |
|    ! 0 |  766 | `		return 0;` |
|      - |  767 | `	}` |
|      - |  768 | `	/* Do the comparison */` |
|  31700 |  769 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|  31700 |  770 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|  31700 |  771 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|  31700 |  772 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|  31700 |  773 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|  31700 |  774 | `	PH7_MemObjRelease(&sObj1);` |
|  31700 |  775 | `	PH7_MemObjRelease(&sObj2);` |
|  31700 |  776 | `	return rc;` |
|  15864 |  777 |  |
|      - |  778 | `/*` |
|      - |  779 | ` * Rehash a node with a 64-bit integer key.` |
|      - |  780 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|      - |  781 | ` */` |
|   6772 |  782 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|      2 |  783 |  |
|   6774 |  784 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|      - |  785 | `	sxu32 nBucket;` |
|      - |  786 | `	/* Remove old collision links */` |
|   6774 |  787 | `	if( pEntry->pPrevCollide ){` |
|   5458 |  788 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|   2720 |  789 | `	}else{` |
|   1318 |  790 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|      - |  791 | `	}` |
|   6774 |  792 | `	if( pEntry->pNextCollide ){` |
|    575 |  793 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|    310 |  794 | `	}` |
|   6774 |  795 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|      - |  796 | `	/* Compute the new hash */` |
|   6774 |  797 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   6774 |  798 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   6774 |  799 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|      - |  800 | `	/* Link to the new bucket */` |
|   6774 |  801 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   6774 |  802 | `	if( pMap->apBucket[nBucket] ){` |
|   5602 |  803 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   2799 |  804 | `	}` |
|   6774 |  805 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   6774 |  806 | `	pMap->apBucket[nBucket] = pEntry;` |
|      - |  807 | `	/* Increment the automatic index */` |
|   6774 |  808 | `	pMap->iNextIdx++;` |
|   6774 |  809 |  |
|      - |  810 | `/*` |
|      - |  811 | ` * Perform a linear search on a given hashmap.` |
|      - |  812 | ` * Write a pointer to the target node on success.` |
|      - |  813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - |  814 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|      - |  815 | ` * for more information.` |
|      - |  816 | ` */` |
|  17364 |  817 | `static int HashmapFindValue(` |
|      - |  818 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|      - |  819 | `	ph7_value *pNeedle,  /* Lookup key */` |
|      - |  820 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|      - |  821 | `	int bStrict      /* TRUE for strict comparison */` |
|      - |  822 | `	)` |
|      2 |  823 |  |
|      - |  824 | `	ph7_hashmap_node *pEntry;` |
|      - |  825 | `	ph7_value sVal,*pVal;` |
|      - |  826 | `	ph7_value sNeedle;` |
|      - |  827 | `	sxi32 rc;` |
|      - |  828 | `	sxu32 n;` |
|      - |  829 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|  17366 |  830 | `	pEntry = pMap->pFirst;` |
|  17366 |  831 | `	n = pMap->nEntry;` |
|  17366 |  832 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|  17366 |  833 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|  41573 |  834 | `	for(;;){` |
|  83146 |  835 | `		if( n < 1 ){` |
|     19 |  836 | `			break;` |
|      - |  837 | `		}` |
|      - |  838 | `		/* Extract node value */` |
|  83128 |  839 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  83128 |  840 | `		if( pVal ){` |
|  83128 |  841 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|    ! 0 |  842 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|    ! 0 |  843 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|    ! 0 |  844 | `				if( iF1 == iF2 ){` |
|      - |  845 | `					/* NULL values are equals */` |
|    ! 0 |  846 | `					if( ppNode ){` |
|    ! 0 |  847 | `						*ppNode = pEntry;` |
|    ! 0 |  848 | `					}` |
|    ! 0 |  849 | `					return SXRET_OK;` |
|      - |  850 | `				}` |
|    ! 0 |  851 | `			}else{` |
|      - |  852 | `				/* Duplicate value */` |
|  83128 |  853 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  83128 |  854 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  83128 |  855 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  83128 |  856 | `				PH7_MemObjRelease(&sVal);` |
|  83128 |  857 | `				PH7_MemObjRelease(&sNeedle);` |
|  83128 |  858 | `				if( rc == 0 ){` |
|  17348 |  859 | `					if( ppNode ){` |
|      5 |  860 | `						*ppNode = pEntry;` |
|      2 |  861 | `					}` |
|      - |  862 | `					/* Match found*/` |
|  17348 |  863 | `					return SXRET_OK;` |
|      - |  864 | `				}` |
|      - |  865 | `			}` |
|  32891 |  866 | `		}` |
|      - |  867 | `		/* Point to the next entry */` |
|  65782 |  868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  65782 |  869 | `		n--;` |
|      2 |  870 | `	}` |
|      - |  871 | `	/* No such entry */` |
|     19 |  872 | `	return SXERR_NOTFOUND;` |
|   8684 |  873 |  |
|      - |  874 | `/*` |
|      - |  875 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|      - |  876 | ` * for values comparison.` |
|      - |  877 | ` * Write a pointer to the target node on success.` |
|      - |  878 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - |  879 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|      - |  880 | ` * for more information.` |
|      - |  881 | ` */` |
|     16 |  882 | `static int HashmapFindValueByCallback(` |
|      - |  883 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|      - |  884 | `	ph7_value *pNeedle,    /* Lookup key */` |
|      - |  885 | `	ph7_value *pCallback,  /* User defined callback */` |
|      - |  886 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|      - |  887 | `	)` |
|      1 |  888 |  |
|      - |  889 | `	ph7_hashmap_node *pEntry;` |
|      - |  890 | `	ph7_value sResult,*pVal;` |
|      - |  891 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|      - |  892 | `	sxi32 rc;` |
|      - |  893 | `	sxu32 n;` |
|      - |  894 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|     17 |  895 | `	pEntry = pMap->pFirst;` |
|     17 |  896 | `	n = pMap->nEntry;` |
|      - |  897 | `	/* Store callback result here */` |
|     17 |  898 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      - |  899 | `	/* First argument to the callback */` |
|     17 |  900 | `	apArg[0] = pNeedle;` |
|     21 |  901 | `	for(;;){` |
|     43 |  902 | `		if( n < 1 ){` |
|      9 |  903 | `			break;` |
|      - |  904 | `		}` |
|      - |  905 | `		/* Extract node value */` |
|     35 |  906 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     35 |  907 | `		if( pVal ){` |
|      - |  908 | `			/* Invoke the user callback */` |
|     35 |  909 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|     35 |  910 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|     35 |  911 | `			if( rc == SXRET_OK ){` |
|      - |  912 | `				/* Extract callback result */` |
|     35 |  913 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  914 | `					/* Perform an int cast */` |
|    ! 0 |  915 | `					PH7_MemObjToInteger(&sResult);` |
|    ! 0 |  916 | `				}` |
|     35 |  917 | `				rc = (sxi32)sResult.x.iVal;` |
|     35 |  918 | `				PH7_MemObjRelease(&sResult);` |
|     35 |  919 | `				if( rc == 0 ){` |
|      - |  920 | `					/* Match found*/` |
|      9 |  921 | `					if( ppNode ){` |
|      3 |  922 | `						*ppNode = pEntry;` |
|      1 |  923 | `					}` |
|      9 |  924 | `					return SXRET_OK;` |
|      - |  925 | `				}` |
|     13 |  926 | `			}` |
|     13 |  927 | `		}` |
|      - |  928 | `		/* Point to the next entry */` |
|     27 |  929 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     27 |  930 | `		n--;` |
|      1 |  931 | `	}` |
|      - |  932 | `	/* No such entry */` |
|      9 |  933 | `	return SXERR_NOTFOUND;` |
|      9 |  934 |  |
|      - |  935 | `/*` |
|      - |  936 | ` * Compare two hashmaps.` |
|      - |  937 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|      - |  938 | ` * Note on array comparison operators.` |
|      - |  939 | ` *  According to the PHP language reference manual.` |
|      - |  940 | ` *  Array Operators Example 	Name 	Result` |
|      - |  941 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|      - |  942 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|      - |  943 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|      - |  944 | ` *                          order and of the same types.` |
|      - |  945 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|      - |  946 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|      - |  947 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|      - |  948 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|      - |  949 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|      - |  950 | ` * and the matching elements from the right-hand array will be ignored.` |
|      - |  951 | ` * <?php` |
|      - |  952 | ` * $a = array("a" => "apple", "b" => "banana");` |
|      - |  953 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|      - |  954 | ` * $c = $a + $b; // Union of $a and $b` |
|      - |  955 | ` * echo "Union of \$a and \$b: \n";` |
|      - |  956 | ` * var_dump($c);` |
|      - |  957 | ` * $c = $b + $a; // Union of $b and $a` |
|      - |  958 | ` * echo "Union of \$b and \$a: \n";` |
|      - |  959 | ` * var_dump($c);` |
|      - |  960 | ` * ?>` |
|      - |  961 | ` * When executed, this script will print the following:` |
|      - |  962 | ` * Union of $a and $b:` |
|      - |  963 | ` * array(3) {` |
|      - |  964 | ` *  ["a"]=>` |
|      - |  965 | ` *  string(5) "apple"` |
|      - |  966 | ` *  ["b"]=>` |
|      - |  967 | ` * string(6) "banana"` |
|      - |  968 | ` *  ["c"]=>` |
|      - |  969 | ` * string(6) "cherry"` |
|      - |  970 | ` * }` |
|      - |  971 | ` * Union of $b and $a:` |
|      - |  972 | ` * array(3) {` |
|      - |  973 | ` * ["a"]=>` |
|      - |  974 | ` * string(4) "pear"` |
|      - |  975 | ` * ["b"]=>` |
|      - |  976 | ` * string(10) "strawberry"` |
|      - |  977 | ` * ["c"]=>` |
|      - |  978 | ` * string(6) "cherry"` |
|      - |  979 | ` * }` |
|      - |  980 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|      - |  981 | ` */` |
|      8 |  982 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|      - |  983 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|      - |  984 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|      - |  985 | `	int bStrict          /* TRUE for strict comparison */` |
|      - |  986 | `	)` |
|      1 |  987 |  |
|      - |  988 | `	ph7_hashmap_node *pLe,*pRe;` |
|      - |  989 | `	sxi32 rc;` |
|      - |  990 | `	sxu32 n;` |
|      9 |  991 | `	if( pLeft == pRight ){` |
|      - |  992 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|      - |  993 | `		 * Unlike the zend engine.` |
|      - |  994 | `		 */` |
|    ! 0 |  995 | `		return 0;` |
|      - |  996 | `	}` |
|      9 |  997 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|      - |  998 | `		/* Must have the same number of entries */` |
|    ! 0 |  999 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|      - | 1000 | `	}` |
|      - | 1001 | `	/* Point to the first inserted entry of the left hashmap */` |
|      9 | 1002 | `	pLe = pLeft->pFirst;` |
|      9 | 1003 | `	pRe = 0; /* cc warning */` |
|      - | 1004 | `	/* Perform the comparison */` |
|      9 | 1005 | `	n = pLeft->nEntry;` |
|      8 | 1006 | `	for(;;){` |
|     17 | 1007 | `		if( n < 1 ){` |
|      7 | 1008 | `			break;` |
|      - | 1009 | `		}` |
|     11 | 1010 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|      - | 1011 | `			/* Int key */` |
|      7 | 1012 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|      4 | 1013 | `		}else{` |
|      5 | 1014 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|      - | 1015 | `			/* Blob key */` |
|      5 | 1016 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|      - | 1017 | `		}` |
|     11 | 1018 | `		if( rc != SXRET_OK ){` |
|      - | 1019 | `			/* No such entry in the right side */` |
|    ! 0 | 1020 | `			return 1;` |
|      - | 1021 | `		}` |
|     11 | 1022 | `		rc = 0;` |
|     11 | 1023 | `		if( bStrict ){` |
|      - | 1024 | `			/* Make sure,the keys are of the same type */` |
|      3 | 1025 | `			if( pLe->iType != pRe->iType ){` |
|    ! 0 | 1026 | `				rc = 1;` |
|    ! 0 | 1027 | `			}` |
|      1 | 1028 | `		}` |
|     11 | 1029 | `		if( !rc ){` |
|      - | 1030 | `			/* Compare nodes */` |
|     11 | 1031 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|      5 | 1032 | `		}` |
|     11 | 1033 | `		if( rc != 0 ){` |
|      - | 1034 | `			/* Nodes key/value differ */` |
|      3 | 1035 | `			return rc;` |
|      - | 1036 | `		}` |
|      - | 1037 | `		/* Point to the next entry */` |
|      9 | 1038 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      9 | 1039 | `		n--;` |
|      1 | 1040 | `	}` |
|      7 | 1041 | `	return 0; /* Hashmaps are equals */` |
|      5 | 1042 |  |
|      - | 1043 | `/*` |
|      - | 1044 | ` * Duplicate a hashmap node.` |
|      - | 1045 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|      - | 1046 | ` */` |
| 292046 | 1047 | `static sxi32 HashmapDuplicateNode(` |
|      - | 1048 | `	ph7_hashmap *pDest,` |
|      - | 1049 | `	ph7_hashmap_node *pEntry,` |
|      - | 1050 | `	ph7_value *pVal,` |
|      - | 1051 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|      - | 1052 | `	)` |
|      2 | 1053 |  |
| 292048 | 1054 | `	ph7_value sSafeVal = *pVal;` |
|      - | 1055 | `	ph7_value sKey;` |
|      - | 1056 | `	sxi32 rc;` |
|      - | 1057 |  |
| 292048 | 1058 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1059 | `		/* Blob key insertion */` |
|     19 | 1060 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|     19 | 1061 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|     19 | 1062 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|     19 | 1063 | `		PH7_MemObjRelease(&sKey);` |
|     10 | 1064 | `	}else{` |
|      - | 1065 | `		/* Int key */` |
| 292030 | 1066 | `		if( iAction == 0 ){ /* Merge */` |
| 292014 | 1067 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
| 146024 | 1068 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      5 | 1069 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      5 | 1070 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      5 | 1071 | `			PH7_MemObjRelease(&sKey);` |
|      3 | 1072 | `		}else{ /* Dup */` |
|     14 | 1073 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      - | 1074 | `		}` |
|      - | 1075 | `	}` |
| 292048 | 1076 | `	return rc;` |
|      2 | 1077 |  |
|      - | 1078 | `/*` |
|      - | 1079 | ` * Merge two hashmaps.` |
|      - | 1080 | ` * Note on the merge process` |
|      - | 1081 | ` * According to the PHP language reference manual.` |
|      - | 1082 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|      - | 1083 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|      - | 1084 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|      - | 1085 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|      - | 1086 | ` *  the later value will not overwrite the original value, but will be appended.` |
|      - | 1087 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|      - | 1088 | ` *  keys starting from zero in the result array.` |
|      - | 1089 | ` */` |
|   1528 | 1090 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|      2 | 1091 |  |
|      - | 1092 | `	ph7_hashmap_node *pEntry;` |
|      - | 1093 | `	ph7_value *pVal;` |
|      - | 1094 | `	sxi32 rc;` |
|      - | 1095 | `	sxu32 n;` |
|   1530 | 1096 | `	if( pSrc == pDest ){` |
|      - | 1097 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1098 | `		 * Unlike the zend engine.` |
|      - | 1099 | `		 */` |
|    ! 0 | 1100 | `		return SXRET_OK;` |
|      - | 1101 | `	}` |
|      - | 1102 | `	/* Point to the first inserted entry in the source */` |
|   1530 | 1103 | `	pEntry = pSrc->pFirst;` |
|      - | 1104 | `	/* Perform the merge */` |
| 293548 | 1105 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|      - | 1106 | `		/* Extract the node value */` |
| 292020 | 1107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
| 292020 | 1108 | `		if( pVal ){` |
|      - | 1109 | `			/* Make a local copy of the value.` |
|      - | 1110 | `			 * The insertion call below may trigger a memory pool reallocation` |
|      - | 1111 | `			 * which will invalidate the 'pVal' pointer since it points` |
|      - | 1112 | `			 * to the old pool.` |
|      - | 1113 | `			 */` |
| 292020 | 1114 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
| 146011 | 1115 | `		}else{` |
|    ! 0 | 1116 | `			rc = SXRET_OK;` |
|      - | 1117 | `		}` |
| 292020 | 1118 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1119 | `			return rc;` |
|      - | 1120 | `		}` |
|      - | 1121 | `		/* Point to the next entry */` |
| 292020 | 1122 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
| 146011 | 1123 | `	}` |
|   1530 | 1124 | `	return SXRET_OK;` |
|    766 | 1125 |  |
|      - | 1126 | `/*` |
|      - | 1127 | ` * Overwrite entries with the same key.` |
|      - | 1128 | ` * Refer to the [array_replace()] implementation for more information.` |
|      - | 1129 | ` *  According to the PHP language reference manual.` |
|      - | 1130 | ` *  array_replace() replaces the values of the first array with the same values` |
|      - | 1131 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|      - | 1132 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|      - | 1133 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|      - | 1134 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|      - | 1135 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|      - | 1136 | ` *  overwriting the previous values.` |
|      - | 1137 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|      - | 1138 | ` *  by whatever type is in the second array.` |
|      - | 1139 | ` */` |
|      4 | 1140 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|      1 | 1141 |  |
|      - | 1142 | `	ph7_hashmap_node *pEntry;` |
|      - | 1143 | `	ph7_value *pVal;` |
|      - | 1144 | `	sxi32 rc;` |
|      - | 1145 | `	sxu32 n;` |
|      5 | 1146 | `	if( pSrc == pDest ){` |
|      - | 1147 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1148 | `		 * Unlike the zend engine.` |
|      - | 1149 | `		 */` |
|    ! 0 | 1150 | `		return SXRET_OK;` |
|      - | 1151 | `	}` |
|      - | 1152 | `	/* Point to the first inserted entry in the source */` |
|      5 | 1153 | `	pEntry = pSrc->pFirst;` |
|      - | 1154 | `	/* Perform the merge */` |
|     13 | 1155 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|      - | 1156 | `		/* Extract the node value */` |
|      9 | 1157 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      9 | 1158 | `		if( pVal ){` |
|      9 | 1159 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      5 | 1160 | `		}else{` |
|    ! 0 | 1161 | `			rc = SXRET_OK;` |
|      - | 1162 | `		}` |
|      9 | 1163 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1164 | `			return rc;` |
|      - | 1165 | `		}` |
|      - | 1166 | `		/* Point to the next entry */` |
|      9 | 1167 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      5 | 1168 | `	}` |
|      5 | 1169 | `	return SXRET_OK;` |
|      3 | 1170 |  |
|      - | 1171 | `/*` |
|      - | 1172 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|      - | 1173 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|      - | 1174 | ` */` |
|     10 | 1175 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|      2 | 1176 |  |
|      - | 1177 | `	ph7_hashmap_node *pEntry;` |
|      - | 1178 | `	ph7_value *pVal;` |
|      - | 1179 | `	sxi32 rc;` |
|      - | 1180 | `	sxu32 n;` |
|     12 | 1181 | `	if( pSrc == pDest ){` |
|      - | 1182 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1183 | `		 * Unlike the zend engine.` |
|      - | 1184 | `		 */` |
|    ! 0 | 1185 | `		return SXRET_OK;` |
|      - | 1186 | `	}` |
|      - | 1187 | `	/* Point to the first inserted entry in the source */` |
|     12 | 1188 | `	pEntry = pSrc->pFirst;` |
|      - | 1189 | `	/* Perform the duplication */` |
|     32 | 1190 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|      - | 1191 | `		/* Extract the node value */` |
|     22 | 1192 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     22 | 1193 | `		if( pVal ){` |
|     22 | 1194 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|     12 | 1195 | `		}else{` |
|    ! 0 | 1196 | `			rc = SXRET_OK;` |
|      - | 1197 | `		}` |
|     22 | 1198 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1199 | `			return rc;` |
|      - | 1200 | `		}` |
|      - | 1201 | `		/* Point to the next entry */` |
|     22 | 1202 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     12 | 1203 | `	}` |
|     12 | 1204 | `	return SXRET_OK;` |
|      7 | 1205 |  |
|      - | 1206 | `/*` |
|      - | 1207 | ` * Perform the union of two hashmaps.` |
|      - | 1208 | ` * This operation is performed only if the user uses the '+' operator` |
|      - | 1209 | ` * with a variable holding an array as follows:` |
|      - | 1210 | ` * <?php` |
|      - | 1211 | ` * $a = array("a" => "apple", "b" => "banana");` |
|      - | 1212 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|      - | 1213 | ` * $c = $a + $b; // Union of $a and $b` |
|      - | 1214 | ` * echo "Union of \$a and \$b: \n";` |
|      - | 1215 | ` * var_dump($c);` |
|      - | 1216 | ` * $c = $b + $a; // Union of $b and $a` |
|      - | 1217 | ` * echo "Union of \$b and \$a: \n";` |
|      - | 1218 | ` * var_dump($c);` |
|      - | 1219 | ` * ?>` |
|      - | 1220 | ` * When executed, this script will print the following:` |
|      - | 1221 | ` * Union of $a and $b:` |
|      - | 1222 | ` * array(3) {` |
|      - | 1223 | ` *  ["a"]=>` |
|      - | 1224 | ` *  string(5) "apple"` |
|      - | 1225 | ` *  ["b"]=>` |
|      - | 1226 | ` * string(6) "banana"` |
|      - | 1227 | ` *  ["c"]=>` |
|      - | 1228 | ` * string(6) "cherry"` |
|      - | 1229 | ` * }` |
|      - | 1230 | ` * Union of $b and $a:` |
|      - | 1231 | ` * array(3) {` |
|      - | 1232 | ` * ["a"]=>` |
|      - | 1233 | ` * string(4) "pear"` |
|      - | 1234 | ` * ["b"]=>` |
|      - | 1235 | ` * string(10) "strawberry"` |
|      - | 1236 | ` * ["c"]=>` |
|      - | 1237 | ` * string(6) "cherry"` |
|      - | 1238 | ` * }` |
|      - | 1239 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|      - | 1240 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|      - | 1241 | ` * and the matching elements from the right-hand array will be ignored.` |
|      - | 1242 | ` */` |
|      4 | 1243 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|      2 | 1244 |  |
|      - | 1245 | `	ph7_hashmap_node *pEntry;` |
|      6 | 1246 | `	sxi32 rc = SXRET_OK;` |
|      - | 1247 | `	ph7_value *pObj;` |
|      - | 1248 | `	sxu32 n;` |
|      6 | 1249 | `	if( pLeft == pRight ){` |
|      - | 1250 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1251 | `		 * Unlike the zend engine.` |
|      - | 1252 | `		 */` |
|    ! 0 | 1253 | `		return SXRET_OK;` |
|      - | 1254 | `	}` |
|      - | 1255 | `	/* Perform the union */` |
|      6 | 1256 | `	pEntry = pRight->pFirst;` |
|     16 | 1257 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|      - | 1258 | `		/* Make sure the given key does not exists in the left array */` |
|     12 | 1259 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1260 | `			/* BLOB key */` |
|      7 | 1261 | `			if( SXRET_OK !=` |
|      6 | 1262 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|      3 | 1263 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|      3 | 1264 | `					if( pObj ){` |
|      3 | 1265 | `						ph7_value sSafeVal = *pObj;` |
|      - | 1266 | `						/* Perform the insertion */` |
|      3 | 1267 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|      - | 1268 | `							&sSafeVal,0,FALSE);` |
|      3 | 1269 | `						if( rc != SXRET_OK ){` |
|    ! 0 | 1270 | `							return rc;` |
|      - | 1271 | `						}` |
|      1 | 1272 | `					}` |
|      1 | 1273 | `			}` |
|      4 | 1274 | `		}else{` |
|      - | 1275 | `			/* INT key */` |
|      5 | 1276 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|    ! 0 | 1277 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|    ! 0 | 1278 | `				if( pObj ){` |
|    ! 0 | 1279 | `					ph7_value sSafeVal = *pObj;` |
|      - | 1280 | `					/* Perform the insertion */` |
|    ! 0 | 1281 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|    ! 0 | 1282 | `					if( rc != SXRET_OK ){` |
|    ! 0 | 1283 | `						return rc;` |
|      - | 1284 | `					}` |
|    ! 0 | 1285 | `				}` |
|    ! 0 | 1286 | `			}` |
|      - | 1287 | `		}` |
|      - | 1288 | `		/* Point to the next entry */` |
|     12 | 1289 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 1290 | `	}` |
|      6 | 1291 | `	return SXRET_OK;` |
|      4 | 1292 |  |
|      - | 1293 | `/*` |
|      - | 1294 | ` * Allocate a new hashmap.` |
|      - | 1295 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|      - | 1296 | ` */` |
|  43226 | 1297 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|      - | 1298 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|      - | 1299 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|      - | 1300 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|      - | 1301 | `	)` |
|      2 | 1302 |  |
|      - | 1303 | `	ph7_hashmap *pMap;` |
|      - | 1304 | `	/* Allocate a new instance */` |
|  43228 | 1305 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|  43228 | 1306 | `	if( pMap == 0 ){` |
|    ! 0 | 1307 | `		return 0;` |
|      - | 1308 | `	}` |
|      - | 1309 | `	/* Zero the structure */` |
|  43228 | 1310 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|      - | 1311 | `	/* Fill in the structure */` |
|  43228 | 1312 | `	pMap->pVm = &(*pVm);` |
|  43228 | 1313 | `	pMap->iRef = 1;` |
|      - | 1314 | `	/* Default hash functions */` |
|  43228 | 1315 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|  43228 | 1316 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|  43228 | 1317 | `	return pMap;` |
|  21615 | 1318 |  |
|      - | 1319 | `/*` |
|      - | 1320 | ` * Install superglobals in the given virtual machine.` |
|      - | 1321 | ` * Note on superglobals.` |
|      - | 1322 | ` *  According to the PHP language reference manual.` |
|      - | 1323 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|      - | 1324 | `*   Description` |
|      - | 1325 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|      - | 1326 | `*   are available in all scopes throughout a script. There is no need to do` |
|      - | 1327 | `*   global $variable; to access them within functions or methods.` |
|      - | 1328 | `*   These superglobal variables are:` |
|      - | 1329 | `*    $GLOBALS` |
|      - | 1330 | `*    $_SERVER` |
|      - | 1331 | `*    $_GET` |
|      - | 1332 | `*    $_POST` |
|      - | 1333 | `*    $_FILES` |
|      - | 1334 | `*    $_COOKIE` |
|      - | 1335 | `*    $_SESSION` |
|      - | 1336 | `*    $_REQUEST` |
|      - | 1337 | `*    $_ENV` |
|      - | 1338 | `*/` |
|   1062 | 1339 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|      2 | 1340 |  |
|      - | 1341 | `	static const char * azSuper[] = {` |
|      - | 1342 | `		"_SERVER",   /* $_SERVER */` |
|      - | 1343 | `		"_GET",      /* $_GET */` |
|      - | 1344 | `		"_POST",     /* $_POST */` |
|      - | 1345 | `		"_FILES",    /* $_FILES */` |
|      - | 1346 | `		"_COOKIE",   /* $_COOKIE */` |
|      - | 1347 | `		"_SESSION",  /* $_SESSION */` |
|      - | 1348 | `		"_REQUEST",  /* $_REQUEST */` |
|      - | 1349 | `		"_ENV",      /* $_ENV */` |
|      - | 1350 | `		"_HEADER",   /* $_HEADER */` |
|      - | 1351 | `		"argv"       /* $argv */` |
|      - | 1352 | `	};` |
|      - | 1353 | `	ph7_hashmap *pMap;` |
|      - | 1354 | `	ph7_value *pObj;` |
|      - | 1355 | `	SyString *pFile;` |
|      - | 1356 | `	sxi32 rc;` |
|      - | 1357 | `	sxu32 n;` |
|      - | 1358 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|   1064 | 1359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   1064 | 1360 | `	if( pMap == 0 ){` |
|    ! 0 | 1361 | `		return SXERR_MEM;` |
|      - | 1362 | `	}` |
|   1064 | 1363 | `	pVm->pGlobal = pMap;` |
|      - | 1364 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|   1064 | 1365 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|   1064 | 1366 | `	if( pObj == 0 ){` |
|    ! 0 | 1367 | `		return SXERR_MEM;` |
|      - | 1368 | `	}` |
|   1064 | 1369 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|      - | 1370 | `	/* Record object index */` |
|   1064 | 1371 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|      - | 1372 | `	/* Install the special $GLOBALS array */` |
|   1064 | 1373 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|   1064 | 1374 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1375 | `		return rc;` |
|      - | 1376 | `	}` |
|      - | 1377 | `	/* Install superglobals now */` |
|  11684 | 1378 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|      - | 1379 | `		ph7_value *pSuper;` |
|      - | 1380 | `		/* Request an empty array */` |
|  10622 | 1381 | `		pSuper = ph7_new_array(&(*pVm));` |
|  10622 | 1382 | `		if( pSuper == 0 ){` |
|    ! 0 | 1383 | `			return SXERR_MEM;` |
|      - | 1384 | `		}` |
|      - | 1385 | `		/* Install */` |
|  10622 | 1386 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|  10622 | 1387 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1388 | `			return rc;` |
|      - | 1389 | `		}` |
|      - | 1390 | `		/* Release the value now it have been installed */` |
|  10622 | 1391 | `		ph7_release_value(&(*pVm),pSuper);` |
|   5312 | 1392 | `	}` |
|      - | 1393 | `	/* Set some $_SERVER entries */` |
|   1064 | 1394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      - | 1395 | `	/*` |
|      - | 1396 | `	 * 'SCRIPT_FILENAME'` |
|      - | 1397 | `	 * The absolute pathname of the currently executing script.` |
|      - | 1398 | `	 */` |
|   2122 | 1399 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|      - | 1400 | `		"SCRIPT_FILENAME",` |
|    531 | 1401 | `		pFile ? pFile->zString : ":Memory:",` |
|   1058 | 1402 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|      - | 1403 | `		);` |
|      - | 1404 | `	/* All done,all super-global are installed now */` |
|   1064 | 1405 | `	return SXRET_OK;` |
|    533 | 1406 |  |
|      - | 1407 | `/*` |
|      - | 1408 | ` * Release a hashmap.` |
|      - | 1409 | ` */` |
|  31498 | 1410 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|      2 | 1411 |  |
|      - | 1412 | `	ph7_hashmap_node *pEntry,*pNext;` |
|  31500 | 1413 | `	ph7_vm *pVm = pMap->pVm;` |
|      - | 1414 | `	sxu32 n;` |
|  31500 | 1415 | `	if( pMap == pVm->pGlobal ){` |
|      - | 1416 | `		/* Cannot delete the $GLOBALS array */` |
|    ! 0 | 1417 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|    ! 0 | 1418 | `		return SXRET_OK;` |
|      - | 1419 | `	}` |
|      - | 1420 | `	/* Start the release process */` |
|  31500 | 1421 | `	n = 0;` |
|  31500 | 1422 | `	pEntry = pMap->pFirst;` |
| 292826 | 1423 | `	for(;;){` |
| 585654 | 1424 | `		if( n >= pMap->nEntry ){` |
|  31500 | 1425 | `			break;` |
|      - | 1426 | `		}` |
| 554156 | 1427 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|      - | 1428 | `		/* Remove the reference from the foreign table */` |
| 554156 | 1429 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 554156 | 1430 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|      - | 1431 | `			/* Restore the ph7_value to the free list */` |
| 554148 | 1432 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 277073 | 1433 | `		}` |
|      - | 1434 | `		/* Release the node */` |
| 554156 | 1435 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|  52248 | 1436 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|  26123 | 1437 | `		}` |
| 554156 | 1438 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|      - | 1439 | `		/* Point to the next entry */` |
| 554156 | 1440 | `		pEntry = pNext;` |
| 554156 | 1441 | `		n++;` |
|      2 | 1442 | `	}` |
|  31500 | 1443 | `	if( pMap->nEntry > 0 ){` |
|      - | 1444 | `		/* Release the hash bucket */` |
|  28136 | 1445 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|  14067 | 1446 | `	}` |
|  31500 | 1447 | `	if( FreeDS ){` |
|      - | 1448 | `		/* Free the whole instance */` |
|  31498 | 1449 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|  15750 | 1450 | `	}else{` |
|      - | 1451 | `		/* Keep the instance but reset it's fields */` |
|      3 | 1452 | `		pMap->apBucket = 0;` |
|      3 | 1453 | `		pMap->iNextIdx = 0;` |
|      3 | 1454 | `		pMap->nEntry = pMap->nSize = 0;` |
|      3 | 1455 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      - | 1456 | `	}` |
|  31500 | 1457 | `	return SXRET_OK;` |
|  15751 | 1458 |  |
|      - | 1459 | `/*` |
|      - | 1460 | ` * Decrement the reference count of a given hashmap.` |
|      - | 1461 | ` * If the count reaches zero which mean no more variables` |
|      - | 1462 | ` * are pointing to this hashmap,then release the whole instance.` |
|      - | 1463 | ` */` |
| 379554 | 1464 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|      2 | 1465 |  |
| 379556 | 1466 | `	ph7_vm *pVm = pMap->pVm;` |
|      - | 1467 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
| 379556 | 1468 | `	pMap->iRef--;` |
| 379556 | 1469 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|  31498 | 1470 | `		PH7_HashmapRelease(pMap,TRUE);` |
|  15748 | 1471 | `	}` |
| 379556 | 1472 |  |
|      - | 1473 | `/*` |
|      - | 1474 | ` * Check if a given key exists in the given hashmap.` |
|      - | 1475 | ` * Write a pointer to the target node on success.` |
|      - | 1476 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - | 1477 | ` */` |
|  66524 | 1478 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|      - | 1479 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|      - | 1480 | `	ph7_value *pKey,          /* Lookup key */` |
|      - | 1481 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|      - | 1482 | `	)` |
|      2 | 1483 |  |
|      - | 1484 | `	sxi32 rc;` |
|  66526 | 1485 | `	if( pMap->nEntry < 1 ){` |
|      - | 1486 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|      - | 1487 | `		 */` |
|      7 | 1488 | `		return SXERR_NOTFOUND;` |
|      - | 1489 | `	}` |
|  66520 | 1490 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  66520 | 1491 | `	return rc;` |
|  33264 | 1492 |  |
|      - | 1493 | `/*` |
|      - | 1494 | ` * Insert a given key and it's associated value (if any) in the given` |
|      - | 1495 | ` * hashmap.` |
|      - | 1496 | ` * If a node with the given key already exists in the database` |
|      - | 1497 | ` * then this function overwrite the old value.` |
|      - | 1498 | ` */` |
| 267052 | 1499 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|      - | 1500 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - | 1501 | `	ph7_value *pKey,   /* Lookup key */` |
|      - | 1502 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|      - | 1503 | `	)` |
|      2 | 1504 |  |
|      - | 1505 | `	sxi32 rc;` |
| 267054 | 1506 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|      - | 1507 | `		/*` |
|      - | 1508 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|      - | 1509 | `		 */` |
|    ! 0 | 1510 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 | 1511 | `		return SXRET_OK;` |
|      - | 1512 | `	}` |
| 267054 | 1513 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 267054 | 1514 | `	return rc;` |
| 133528 | 1515 |  |
|      - | 1516 | `/*` |
|      - | 1517 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|      - | 1518 | ` * hashmap.` |
|      - | 1519 | ` * This is insertion by reference so be careful to mark the node` |
|      - | 1520 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|      - | 1521 | ` * The insertion by reference is triggered when the following` |
|      - | 1522 | ` * expression is encountered.` |
|      - | 1523 | ` * $var = 10;` |
|      - | 1524 | ` *  $a = array(&var);` |
|      - | 1525 | ` * OR` |
|      - | 1526 | ` *  $a[] =& $var;` |
|      - | 1527 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|      - | 1528 | ` * over it's contents.` |
|      - | 1529 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|      - | 1530 | ` * removed when the foreign ph7_value is unset.` |
|      - | 1531 | ` * Example:` |
|      - | 1532 | ` *  $var = 10;` |
|      - | 1533 | ` *  $a[] =& $var;` |
|      - | 1534 | ` *  echo count($a).PHP_EOL; //1` |
|      - | 1535 | ` *  //Unset the foreign ph7_value now` |
|      - | 1536 | ` *  unset($var);` |
|      - | 1537 | ` *  echo count($a); //0` |
|      - | 1538 | ` * Note that this is a PH7 eXtension.` |
|      - | 1539 | ` * Refer to the official documentation for more information.` |
|      - | 1540 | ` * If a node with the given key already exists in the database` |
|      - | 1541 | ` * then this function overwrite the old value.` |
|      - | 1542 | ` */` |
|  15370 | 1543 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|      - | 1544 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - | 1545 | `	ph7_value *pKey,   /* Lookup key */` |
|      - | 1546 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|      - | 1547 | `	)` |
|      2 | 1548 |  |
|      - | 1549 | `	sxi32 rc;` |
|  15372 | 1550 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|      - | 1551 | `		/*` |
|      - | 1552 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|      - | 1553 | `		 */` |
|    ! 0 | 1554 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 | 1555 | `		return SXRET_OK;` |
|      - | 1556 | `	}` |
|  15372 | 1557 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|  15372 | 1558 | `	return rc;` |
|   7687 | 1559 |  |
|      - | 1560 | `/*` |
|      - | 1561 | ` * Reset the node cursor of a given hashmap.` |
|      - | 1562 | ` */` |
|  14156 | 1563 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|      2 | 1564 |  |
|      - | 1565 | `	/* Reset the loop cursor */` |
|  14158 | 1566 | `	pMap->pCur = pMap->pFirst;` |
|  14158 | 1567 |  |
|      - | 1568 | `/*` |
|      - | 1569 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|      - | 1570 | ` * If the cursor reaches the end of the list,then this function` |
|      - | 1571 | ` * return NULL.` |
|      - | 1572 | ` * Note that the node cursor is automatically advanced by this function.` |
|      - | 1573 | ` */` |
| 119078 | 1574 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|      2 | 1575 |  |
| 119080 | 1576 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
| 119080 | 1577 | `	if( pCur == 0 ){` |
|      - | 1578 | `		/* End of the list,return null */` |
|   7082 | 1579 | `		return 0;` |
|      - | 1580 | `	}` |
|      - | 1581 | `	/* Advance the node cursor */` |
| 112000 | 1582 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
| 112000 | 1583 | `	return pCur;` |
|  59541 | 1584 |  |
|      - | 1585 | `/*` |
|      - | 1586 | ` * Extract a node value.` |
|      - | 1587 | ` */` |
| 287326 | 1588 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|      2 | 1589 |  |
| 287328 | 1590 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
| 287328 | 1591 | `	if( pEntry ){` |
| 287328 | 1592 | `		if( bStore ){` |
| 112048 | 1593 | `			PH7_MemObjStore(pEntry,pValue);` |
|  56025 | 1594 | `		}else{` |
| 175282 | 1595 | `			PH7_MemObjLoad(pEntry,pValue);` |
|      - | 1596 | `		}` |
| 143691 | 1597 | `	}else{` |
|    ! 0 | 1598 | `		PH7_MemObjRelease(pValue);` |
|      - | 1599 | `	}` |
| 287328 | 1600 |  |
|      - | 1601 | `/*` |
|      - | 1602 | ` * Extract a node key.` |
|      - | 1603 | ` */` |
|  80068 | 1604 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|      2 | 1605 |  |
|      - | 1606 | `	/* Fill with the current key */` |
|  80070 | 1607 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  79938 | 1608 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|    ! 0 | 1609 | `			SyBlobRelease(&pKey->sBlob);` |
|    ! 0 | 1610 | `		}` |
|  79938 | 1611 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  79938 | 1612 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|  39970 | 1613 | `	}else{` |
|    133 | 1614 | `		SyBlobReset(&pKey->sBlob);` |
|    133 | 1615 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    133 | 1616 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|      - | 1617 | `	}` |
|  80070 | 1618 |  |
|      - | 1619 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 1620 | `/*` |
|      - | 1621 | ` * Store the address of nodes value in the given container.` |
|      - | 1622 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|      - | 1623 | ` * defined in 'builtin.c' for more information.` |
|      - | 1624 | ` */` |
|     10 | 1625 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|      1 | 1626 |  |
|     11 | 1627 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      - | 1628 | `	ph7_value *pValue;` |
|      - | 1629 | `	sxu32 n;` |
|      - | 1630 | `	/* Initialize the container */` |
|     11 | 1631 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|     27 | 1632 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 1633 | `		/* Extract node value */` |
|     17 | 1634 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     17 | 1635 | `		if( pValue ){` |
|     17 | 1636 | `			SySetPut(pOut,(const void *)&pValue);` |
|      8 | 1637 | `		}` |
|      - | 1638 | `		/* Point to the next entry */` |
|     17 | 1639 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      9 | 1640 | `	}` |
|      - | 1641 | `	/* Total inserted entries */` |
|     11 | 1642 | `	return (int)SySetUsed(pOut);` |
|      1 | 1643 |  |
|      - | 1644 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 1645 | `/*` |
|      - | 1646 | ` * Merge sort.` |
|      - | 1647 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|      - | 1648 | ` * Status: Public domain` |
|      - | 1649 | ` */` |
|      - | 1650 | `/* Node comparison callback signature */` |
|      - | 1651 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|      - | 1652 | `/*` |
|      - | 1653 | `** Inputs:` |
|      - | 1654 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|      - | 1655 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|      - | 1656 | `**   cmp:     A pointer to the comparison function.` |
|      - | 1657 | `**` |
|      - | 1658 | `** Return Value:` |
|      - | 1659 | `**   A pointer to the head of a sorted list containing the elements` |
|      - | 1660 | `**   of both a and b.` |
|      - | 1661 | `**` |
|      - | 1662 | `** Side effects:` |
|      - | 1663 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|      - | 1664 | `**   changed.` |
|      - | 1665 | `*/` |
|  19794 | 1666 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|      2 | 1667 |  |
|      - | 1668 | `	ph7_hashmap_node result,*pTail;` |
|      - | 1669 | `    /* Prevent compiler warning */` |
|  19796 | 1670 | `	result.pNext = result.pPrev = 0;` |
|  19796 | 1671 | `	pTail = &result;` |
|  51538 | 1672 | `	while( pA && pB ){` |
|  31744 | 1673 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|  20664 | 1674 | `			pTail->pPrev = pA;` |
|  20664 | 1675 | `			pA->pNext = pTail;` |
|  20664 | 1676 | `			pTail = pA;` |
|  20664 | 1677 | `			pA = pA->pPrev;` |
|  10339 | 1678 | `		}else{` |
|  11082 | 1679 | `			pTail->pPrev = pB;` |
|  11082 | 1680 | `			pB->pNext = pTail;` |
|  11082 | 1681 | `			pTail = pB;` |
|  11082 | 1682 | `			pB = pB->pPrev;` |
|      - | 1683 | `		}` |
|      2 | 1684 | `	}` |
|  19796 | 1685 | `	if( pA ){` |
|  14684 | 1686 | `		pTail->pPrev = pA;` |
|  14684 | 1687 | `		pA->pNext = pTail;` |
|  12458 | 1688 | `	}else if( pB ){` |
|   5006 | 1689 | `		pTail->pPrev = pB;` |
|   5006 | 1690 | `		pB->pNext = pTail;` |
|   2501 | 1691 | `	}else{` |
|    110 | 1692 | `		pTail->pPrev = pTail->pNext = 0;` |
|      - | 1693 | `	}` |
|  19796 | 1694 | `	return result.pPrev;` |
|      2 | 1695 |  |
|      - | 1696 | `/*` |
|      - | 1697 | `** Inputs:` |
|      - | 1698 | `**   Map:       Input hashmap` |
|      - | 1699 | `**   cmp:       A comparison function.` |
|      - | 1700 | `**` |
|      - | 1701 | `** Return Value:` |
|      - | 1702 | `**   Sorted hashmap.` |
|      - | 1703 | `**` |
|      - | 1704 | `** Side effects:` |
|      - | 1705 | `**   The "next" pointers for elements in list are changed.` |
|      - | 1706 | `*/` |
|      - | 1707 | `#define N_SORT_BUCKET  32` |
|    444 | 1708 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|      2 | 1709 |  |
|      - | 1710 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|      - | 1711 | `	sxu32 i;` |
|    446 | 1712 | `	SyZero(a,sizeof(a));` |
|      - | 1713 | `	/* Point to the first inserted entry */` |
|    446 | 1714 | `	pIn = pMap->pFirst;` |
|   7226 | 1715 | `	while( pIn ){` |
|   6782 | 1716 | `		p = pIn;` |
|   6782 | 1717 | `		pIn = p->pPrev;` |
|   6782 | 1718 | `		p->pPrev = 0;` |
|  12812 | 1719 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|  12812 | 1720 | `			if( a[i]==0 ){` |
|   6782 | 1721 | `				a[i] = p;` |
|   6782 | 1722 | `				break;` |
|    ! 0 | 1723 | `			}else{` |
|   6032 | 1724 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   6032 | 1725 | `				a[i] = 0;` |
|      - | 1726 | `			}` |
|   3017 | 1727 | `		}` |
|   6782 | 1728 | `		if( i==N_SORT_BUCKET-1 ){` |
|      - | 1729 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|      - | 1730 | `			 * But that is impossible.` |
|      - | 1731 | `			 */` |
|    ! 0 | 1732 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|    ! 0 | 1733 | `		}` |
|      2 | 1734 | `	}` |
|    446 | 1735 | `	p = a[0];` |
|  14210 | 1736 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|  13766 | 1737 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   6884 | 1738 | `	}` |
|    446 | 1739 | `	p->pNext = 0;` |
|      - | 1740 | `	/* Reflect the change */` |
|    446 | 1741 | `	pMap->pFirst = p;` |
|      - | 1742 | `	/* Reset the loop cursor */` |
|    446 | 1743 | `	pMap->pCur = pMap->pFirst;` |
|    446 | 1744 | `	return SXRET_OK;` |
|      2 | 1745 |  |
|      - | 1746 | `/*` |
|      - | 1747 | ` * Node comparison callback.` |
|      - | 1748 | ` * used-by: [sort(),asort(),...]` |
|      - | 1749 | ` */` |
|  31680 | 1750 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      2 | 1751 |  |
|      - | 1752 | `	ph7_value sA,sB;` |
|      - | 1753 | `	sxi32 iFlags;` |
|      - | 1754 | `	int rc;` |
|  31682 | 1755 | `	if( pCmpData == 0 ){` |
|      - | 1756 | `		/* Perform a standard comparison */` |
|  31678 | 1757 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|  31678 | 1758 | `		return rc;` |
|      - | 1759 | `	}` |
|      5 | 1760 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|      - | 1761 | `	/* Duplicate node values */` |
|      5 | 1762 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      5 | 1763 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      5 | 1764 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      5 | 1765 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      5 | 1766 | `	if( iFlags == 5 ){` |
|      - | 1767 | `		/* String cast */` |
|      5 | 1768 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1769 | `			PH7_MemObjToString(&sA);` |
|    ! 0 | 1770 | `		}` |
|      5 | 1771 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1772 | `			PH7_MemObjToString(&sB);` |
|    ! 0 | 1773 | `		}` |
|      3 | 1774 | `	}else{` |
|      - | 1775 | `		/* Numeric cast */` |
|    ! 0 | 1776 | `		PH7_MemObjToNumeric(&sA);` |
|    ! 0 | 1777 | `		PH7_MemObjToNumeric(&sB);` |
|      - | 1778 | `	}` |
|      - | 1779 | `	/* Perform the comparison */` |
|      5 | 1780 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|      5 | 1781 | `	PH7_MemObjRelease(&sA);` |
|      5 | 1782 | `	PH7_MemObjRelease(&sB);` |
|      5 | 1783 | `	return rc;` |
|  15855 | 1784 |  |
|      - | 1785 | `/*` |
|      - | 1786 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - | 1787 | ` * used-by: [ksort()]` |
|      - | 1788 | ` */` |
|     14 | 1789 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1790 |  |
|      - | 1791 | `	sxi32 rc;` |
|      7 | 1792 | `	SXUNUSED(pCmpData); /* cc warning */` |
|     15 | 1793 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1794 | `		/* Perform a string comparison */` |
|      5 | 1795 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|      3 | 1796 | `	}else{` |
|      - | 1797 | `		SyString sStr;` |
|      - | 1798 | `		sxi64 iA,iB;` |
|      - | 1799 | `		/* Perform a numeric comparison */` |
|     11 | 1800 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1801 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1802 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|    ! 0 | 1803 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1804 | `				iA = 0;` |
|    ! 0 | 1805 | `			}else{` |
|    ! 0 | 1806 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|      - | 1807 | `			}` |
|    ! 0 | 1808 | `		}else{` |
|     11 | 1809 | `			iA = pA->xKey.iKey;` |
|      - | 1810 | `		}` |
|     11 | 1811 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1812 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1813 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|    ! 0 | 1814 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1815 | `				iB = 0;` |
|    ! 0 | 1816 | `			}else{` |
|    ! 0 | 1817 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|      - | 1818 | `			}` |
|    ! 0 | 1819 | `		}else{` |
|     11 | 1820 | `			iB = pB->xKey.iKey;` |
|      - | 1821 | `		}` |
|     11 | 1822 | `		rc = (sxi32)(iA-iB);` |
|      - | 1823 | `	}` |
|      - | 1824 | `	/* Comparison result */` |
|     15 | 1825 | `	return rc;` |
|      1 | 1826 |  |
|      - | 1827 | `/*` |
|      - | 1828 | ` * Node comparison callback.` |
|      - | 1829 | ` * Used by: [rsort(),arsort()];` |
|      - | 1830 | ` */` |
|     12 | 1831 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1832 |  |
|      - | 1833 | `	ph7_value sA,sB;` |
|      - | 1834 | `	sxi32 iFlags;` |
|      - | 1835 | `	int rc;` |
|     13 | 1836 | `	if( pCmpData == 0 ){` |
|      - | 1837 | `		/* Perform a standard comparison */` |
|     13 | 1838 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     13 | 1839 | `		return -rc;` |
|      - | 1840 | `	}` |
|    ! 0 | 1841 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|      - | 1842 | `	/* Duplicate node values */` |
|    ! 0 | 1843 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|    ! 0 | 1844 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|    ! 0 | 1845 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|    ! 0 | 1846 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|    ! 0 | 1847 | `	if( iFlags == 5 ){` |
|      - | 1848 | `		/* String cast */` |
|    ! 0 | 1849 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1850 | `			PH7_MemObjToString(&sA);` |
|    ! 0 | 1851 | `		}` |
|    ! 0 | 1852 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1853 | `			PH7_MemObjToString(&sB);` |
|    ! 0 | 1854 | `		}` |
|    ! 0 | 1855 | `	}else{` |
|      - | 1856 | `		/* Numeric cast */` |
|    ! 0 | 1857 | `		PH7_MemObjToNumeric(&sA);` |
|    ! 0 | 1858 | `		PH7_MemObjToNumeric(&sB);` |
|      - | 1859 | `	}` |
|      - | 1860 | `	/* Perform the comparison */` |
|    ! 0 | 1861 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|    ! 0 | 1862 | `	PH7_MemObjRelease(&sA);` |
|    ! 0 | 1863 | `	PH7_MemObjRelease(&sB);` |
|    ! 0 | 1864 | `	return -rc;` |
|      7 | 1865 |  |
|      - | 1866 | `/*` |
|      - | 1867 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - | 1868 | ` * used-by: [usort(),uasort()]` |
|      - | 1869 | ` */` |
|     12 | 1870 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1871 |  |
|      - | 1872 | `	ph7_value sResult,*pCallback;` |
|      - | 1873 | `	ph7_value *pV1,*pV2;` |
|      - | 1874 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - | 1875 | `	sxi32 rc;` |
|      - | 1876 | `	/* Point to the desired callback */` |
|     13 | 1877 | `	pCallback = (ph7_value *)pCmpData;` |
|      - | 1878 | `	/* initialize the result value */` |
|     13 | 1879 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      - | 1880 | `	/* Extract nodes values */` |
|     13 | 1881 | `	pV1 = HashmapExtractNodeValue(pA);` |
|     13 | 1882 | `	pV2 = HashmapExtractNodeValue(pB);` |
|     13 | 1883 | `	apArg[0] = pV1;` |
|     13 | 1884 | `	apArg[1] = pV2;` |
|      - | 1885 | `	/* Invoke the callback */` |
|     13 | 1886 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|     13 | 1887 | `	if( rc != SXRET_OK ){` |
|      - | 1888 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 | 1889 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 | 1890 | `	}else{` |
|      - | 1891 | `		/* Extract callback result */` |
|     13 | 1892 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - | 1893 | `			/* Perform an int cast */` |
|    ! 0 | 1894 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 | 1895 | `		}` |
|     13 | 1896 | `		rc = (sxi32)sResult.x.iVal;` |
|      - | 1897 | `	}` |
|     13 | 1898 | `	PH7_MemObjRelease(&sResult);` |
|      - | 1899 | `	/* Callback result */` |
|     13 | 1900 | `	return rc;` |
|      1 | 1901 |  |
|      - | 1902 | `/*` |
|      - | 1903 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - | 1904 | ` * used-by: [krsort()]` |
|      - | 1905 | ` */` |
|      4 | 1906 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1907 |  |
|      - | 1908 | `	sxi32 rc;` |
|      2 | 1909 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      5 | 1910 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1911 | `		/* Perform a string comparison */` |
|      5 | 1912 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|      3 | 1913 | `	}else{` |
|      - | 1914 | `		SyString sStr;` |
|      - | 1915 | `		sxi64 iA,iB;` |
|      - | 1916 | `		/* Perform a numeric comparison */` |
|    ! 0 | 1917 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1918 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1919 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|    ! 0 | 1920 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1921 | `				iA = 0;` |
|    ! 0 | 1922 | `			}else{` |
|    ! 0 | 1923 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|      - | 1924 | `			}` |
|    ! 0 | 1925 | `		}else{` |
|    ! 0 | 1926 | `			iA = pA->xKey.iKey;` |
|      - | 1927 | `		}` |
|    ! 0 | 1928 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1929 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1930 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|    ! 0 | 1931 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1932 | `				iB = 0;` |
|    ! 0 | 1933 | `			}else{` |
|    ! 0 | 1934 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|      - | 1935 | `			}` |
|    ! 0 | 1936 | `		}else{` |
|    ! 0 | 1937 | `			iB = pB->xKey.iKey;` |
|      - | 1938 | `		}` |
|    ! 0 | 1939 | `		rc = (sxi32)(iA-iB);` |
|      - | 1940 | `	}` |
|      5 | 1941 | `	return -rc; /* Reverse result */` |
|      1 | 1942 |  |
|      - | 1943 | `/*` |
|      - | 1944 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - | 1945 | ` * used-by: [uksort()]` |
|      - | 1946 | ` */` |
|      6 | 1947 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1948 |  |
|      - | 1949 | `	ph7_value sResult,*pCallback;` |
|      - | 1950 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - | 1951 | `	ph7_value sK1,sK2;` |
|      - | 1952 | `	sxi32 rc;` |
|      - | 1953 | `	/* Point to the desired callback */` |
|      7 | 1954 | `	pCallback = (ph7_value *)pCmpData;` |
|      - | 1955 | `	/* initialize the result value */` |
|      7 | 1956 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      7 | 1957 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|      7 | 1958 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|      - | 1959 | `	/* Extract nodes keys */` |
|      7 | 1960 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|      7 | 1961 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|      7 | 1962 | `	apArg[0] = &sK1;` |
|      7 | 1963 | `	apArg[1] = &sK2;` |
|      - | 1964 | `	/* Mark keys as constants */` |
|      7 | 1965 | `	sK1.nIdx = SXU32_HIGH;` |
|      7 | 1966 | `	sK2.nIdx = SXU32_HIGH;` |
|      - | 1967 | `	/* Invoke the callback */` |
|      7 | 1968 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      7 | 1969 | `	if( rc != SXRET_OK ){` |
|      - | 1970 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 | 1971 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 | 1972 | `	}else{` |
|      - | 1973 | `		/* Extract callback result */` |
|      7 | 1974 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - | 1975 | `			/* Perform an int cast */` |
|    ! 0 | 1976 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 | 1977 | `		}` |
|      7 | 1978 | `		rc = (sxi32)sResult.x.iVal;` |
|      - | 1979 | `	}` |
|      7 | 1980 | `	PH7_MemObjRelease(&sResult);` |
|      7 | 1981 | `	PH7_MemObjRelease(&sK1);` |
|      7 | 1982 | `	PH7_MemObjRelease(&sK2);` |
|      - | 1983 | `	/* Callback result */` |
|      7 | 1984 | `	return rc;` |
|      1 | 1985 |  |
|      - | 1986 | `/*` |
|      - | 1987 | ` * Node comparison callback: Random node comparison.` |
|      - | 1988 | ` * used-by: [shuffle()]` |
|      - | 1989 | ` */` |
|     14 | 1990 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1991 |  |
|      - | 1992 | `	sxu32 n;` |
|      8 | 1993 | `	SXUNUSED(pB); /* cc warning */` |
|      8 | 1994 | `	SXUNUSED(pCmpData);` |
|      - | 1995 | `	/* Grab a random number */` |
|     15 | 1996 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|      - | 1997 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|      - | 1998 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|      - | 1999 | `	 */` |
|     15 | 2000 | `	return n&1 ? 1 : -1;` |
|      1 | 2001 |  |
|      - | 2002 | `/*` |
|      - | 2003 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|      - | 2004 | ` * Used by [sort(),usort() and rsort()].` |
|      - | 2005 | ` */` |
|    428 | 2006 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|      2 | 2007 |  |
|      - | 2008 | `	ph7_hashmap_node *p,*pLast;` |
|      - | 2009 | `	sxu32 i;` |
|      - | 2010 | `	/* Rehash all entries */` |
|    430 | 2011 | `	pLast = p = pMap->pFirst;` |
|    430 | 2012 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|    430 | 2013 | `	i = 0;` |
|   3577 | 2014 | `	for( ;; ){` |
|   7156 | 2015 | `		if( i >= pMap->nEntry ){` |
|    430 | 2016 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|    430 | 2017 | `			break;` |
|      - | 2018 | `		}` |
|   6728 | 2019 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|      - | 2020 | `			/* Do not maintain index association as requested by the PHP specification */` |
|      5 | 2021 | `			SyBlobRelease(&p->xKey.sKey);` |
|      - | 2022 | `			/* Change key type */` |
|      5 | 2023 | `			p->iType = HASHMAP_INT_NODE;` |
|      2 | 2024 | `		}` |
|   6728 | 2025 | `		HashmapRehashIntNode(p);` |
|      - | 2026 | `		/* Point to the next entry */` |
|   6728 | 2027 | `		i++;` |
|   6728 | 2028 | `		pLast = p;` |
|   6728 | 2029 | `		p = p->pPrev; /* Reverse link */` |
|      2 | 2030 | `	}` |
|    430 | 2031 |  |
|      - | 2032 | `/*` |
|      - | 2033 | ` * Array functions implementation.` |
|      - | 2034 | ` * Status:` |
|      - | 2035 | ` *  Stable.` |
|      - | 2036 | ` */` |
|      - | 2037 | `/*` |
|      - | 2038 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2039 | ` * Sort an array.` |
|      - | 2040 | ` * Parameters` |
|      - | 2041 | ` *  $array` |
|      - | 2042 | ` *   The input array.` |
|      - | 2043 | ` * $sort_flags` |
|      - | 2044 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2045 | ` *  Sorting type flags:` |
|      - | 2046 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2047 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2048 | ` *   SORT_STRING - compare items as strings` |
|      - | 2049 | ` * Return` |
|      - | 2050 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2051 | ` *` |
|      - | 2052 | ` */` |
|    768 | 2053 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2054 |  |
|      - | 2055 | `	ph7_hashmap *pMap;` |
|      - | 2056 | `	/* Make sure we are dealing with a valid hashmap */` |
|    770 | 2057 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2060 | `		return PH7_OK;` |
|      - | 2061 | `	}` |
|      - | 2062 | `	/* Point to the internal representation of the input hashmap */` |
|    770 | 2063 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    770 | 2064 | `	if( pMap->nEntry > 1 ){` |
|    424 | 2065 | `		sxi32 iCmpFlags = 0;` |
|    424 | 2066 | `		if( nArg > 1 ){` |
|      - | 2067 | `			/* Extract comparison flags */` |
|      3 | 2068 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 2069 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2070 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2071 | `			}` |
|      1 | 2072 | `		}` |
|      - | 2073 | `		/* Do the merge sort */` |
|    424 | 2074 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2075 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|    424 | 2076 | `		HashmapSortRehash(pMap);` |
|    211 | 2077 | `	}` |
|      - | 2078 | `	/* All done,return TRUE */` |
|    770 | 2079 | `	ph7_result_bool(pCtx,1);` |
|    770 | 2080 | `	return PH7_OK;` |
|    386 | 2081 |  |
|      - | 2082 | `/*` |
|      - | 2083 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2084 | ` *  Sort an array and maintain index association.` |
|      - | 2085 | ` * Parameters` |
|      - | 2086 | ` *  $array` |
|      - | 2087 | ` *   The input array.` |
|      - | 2088 | ` * $sort_flags` |
|      - | 2089 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2090 | ` *  Sorting type flags:` |
|      - | 2091 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2092 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2093 | ` *   SORT_STRING - compare items as strings` |
|      - | 2094 | ` * Return` |
|      - | 2095 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2096 | ` */` |
|      2 | 2097 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2098 |  |
|      - | 2099 | `	ph7_hashmap *pMap;` |
|      - | 2100 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2101 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2102 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2103 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2104 | `		return PH7_OK;` |
|      - | 2105 | `	}` |
|      - | 2106 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2107 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2108 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2109 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2110 | `		if( nArg > 1 ){` |
|      - | 2111 | `			/* Extract comparison flags */` |
|    ! 0 | 2112 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2113 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2114 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2115 | `			}` |
|    ! 0 | 2116 | `		}` |
|      - | 2117 | `		/* Do the merge sort */` |
|      3 | 2118 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2119 | `		/* Fix the last link broken by the merge */` |
|      5 | 2120 | `		while(pMap->pLast->pPrev){` |
|      3 | 2121 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2122 | `		}` |
|      1 | 2123 | `	}` |
|      - | 2124 | `	/* All done,return TRUE */` |
|      3 | 2125 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2126 | `	return PH7_OK;` |
|      2 | 2127 |  |
|      - | 2128 | `/*` |
|      - | 2129 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2130 | ` *  Sort an array in reverse order and maintain index association.` |
|      - | 2131 | ` * Parameters` |
|      - | 2132 | ` *  $array` |
|      - | 2133 | ` *   The input array.` |
|      - | 2134 | ` * $sort_flags` |
|      - | 2135 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2136 | ` *  Sorting type flags:` |
|      - | 2137 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2138 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2139 | ` *   SORT_STRING - compare items as strings` |
|      - | 2140 | ` * Return` |
|      - | 2141 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2142 | ` */` |
|      2 | 2143 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2144 |  |
|      - | 2145 | `	ph7_hashmap *pMap;` |
|      - | 2146 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2147 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2148 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2149 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2150 | `		return PH7_OK;` |
|      - | 2151 | `	}` |
|      - | 2152 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2153 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2154 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2155 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2156 | `		if( nArg > 1 ){` |
|      - | 2157 | `			/* Extract comparison flags */` |
|    ! 0 | 2158 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2159 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2160 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2161 | `			}` |
|    ! 0 | 2162 | `		}` |
|      - | 2163 | `		/* Do the merge sort */` |
|      3 | 2164 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2165 | `		/* Fix the last link broken by the merge */` |
|      5 | 2166 | `		while(pMap->pLast->pPrev){` |
|      3 | 2167 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2168 | `		}` |
|      1 | 2169 | `	}` |
|      - | 2170 | `	/* All done,return TRUE */` |
|      3 | 2171 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2172 | `	return PH7_OK;` |
|      2 | 2173 |  |
|      - | 2174 | `/*` |
|      - | 2175 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2176 | ` *  Sort an array by key.` |
|      - | 2177 | ` * Parameters` |
|      - | 2178 | ` *  $array` |
|      - | 2179 | ` *   The input array.` |
|      - | 2180 | ` * $sort_flags` |
|      - | 2181 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2182 | ` *  Sorting type flags:` |
|      - | 2183 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2184 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2185 | ` *   SORT_STRING - compare items as strings` |
|      - | 2186 | ` * Return` |
|      - | 2187 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2188 | ` */` |
|      4 | 2189 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2190 |  |
|      - | 2191 | `	ph7_hashmap *pMap;` |
|      - | 2192 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 2193 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2194 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2195 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2196 | `		return PH7_OK;` |
|      - | 2197 | `	}` |
|      - | 2198 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 2199 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 2200 | `	if( pMap->nEntry > 1 ){` |
|      5 | 2201 | `		sxi32 iCmpFlags = 0;` |
|      5 | 2202 | `		if( nArg > 1 ){` |
|      - | 2203 | `			/* Extract comparison flags */` |
|    ! 0 | 2204 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2205 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2206 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2207 | `			}` |
|    ! 0 | 2208 | `		}` |
|      - | 2209 | `		/* Do the merge sort */` |
|      5 | 2210 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2211 | `		/* Fix the last link broken by the merge */` |
|     15 | 2212 | `		while(pMap->pLast->pPrev){` |
|     11 | 2213 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2214 | `		}` |
|      2 | 2215 | `	}` |
|      - | 2216 | `	/* All done,return TRUE */` |
|      5 | 2217 | `	ph7_result_bool(pCtx,1);` |
|      5 | 2218 | `	return PH7_OK;` |
|      3 | 2219 |  |
|      - | 2220 | `/*` |
|      - | 2221 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2222 | ` *  Sort an array by key in reverse order.` |
|      - | 2223 | ` * Parameters` |
|      - | 2224 | ` *  $array` |
|      - | 2225 | ` *   The input array.` |
|      - | 2226 | ` * $sort_flags` |
|      - | 2227 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2228 | ` *  Sorting type flags:` |
|      - | 2229 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2230 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2231 | ` *   SORT_STRING - compare items as strings` |
|      - | 2232 | ` * Return` |
|      - | 2233 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2234 | ` */` |
|      2 | 2235 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2236 |  |
|      - | 2237 | `	ph7_hashmap *pMap;` |
|      - | 2238 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2239 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2240 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2241 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2242 | `		return PH7_OK;` |
|      - | 2243 | `	}` |
|      - | 2244 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2245 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2246 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2247 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2248 | `		if( nArg > 1 ){` |
|      - | 2249 | `			/* Extract comparison flags */` |
|    ! 0 | 2250 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2251 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2252 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2253 | `			}` |
|    ! 0 | 2254 | `		}` |
|      - | 2255 | `		/* Do the merge sort */` |
|      3 | 2256 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2257 | `		/* Fix the last link broken by the merge */` |
|      7 | 2258 | `		while(pMap->pLast->pPrev){` |
|      5 | 2259 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2260 | `		}` |
|      1 | 2261 | `	}` |
|      - | 2262 | `	/* All done,return TRUE */` |
|      3 | 2263 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2264 | `	return PH7_OK;` |
|      2 | 2265 |  |
|      - | 2266 | `/*` |
|      - | 2267 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2268 | ` * Sort an array in reverse order.` |
|      - | 2269 | ` * Parameters` |
|      - | 2270 | ` *  $array` |
|      - | 2271 | ` *   The input array.` |
|      - | 2272 | ` * $sort_flags` |
|      - | 2273 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2274 | ` *  Sorting type flags:` |
|      - | 2275 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2276 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2277 | ` *   SORT_STRING - compare items as strings` |
|      - | 2278 | ` * Return` |
|      - | 2279 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2280 | ` */` |
|      2 | 2281 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2282 |  |
|      - | 2283 | `	ph7_hashmap *pMap;` |
|      - | 2284 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2285 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2286 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2287 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2288 | `		return PH7_OK;` |
|      - | 2289 | `	}` |
|      - | 2290 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2291 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2292 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2293 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2294 | `		if( nArg > 1 ){` |
|      - | 2295 | `			/* Extract comparison flags */` |
|    ! 0 | 2296 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2297 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2298 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2299 | `			}` |
|    ! 0 | 2300 | `		}` |
|      - | 2301 | `		/* Do the merge sort */` |
|      3 | 2302 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2303 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      3 | 2304 | `		HashmapSortRehash(pMap);` |
|      1 | 2305 | `	}` |
|      - | 2306 | `	/* All done,return TRUE */` |
|      3 | 2307 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2308 | `	return PH7_OK;` |
|      2 | 2309 |  |
|      - | 2310 | `/*` |
|      - | 2311 | ` * bool usort(array &$array,callable $cmp_function)` |
|      - | 2312 | ` *  Sort an array by values using a user-defined comparison function.` |
|      - | 2313 | ` * Parameters` |
|      - | 2314 | ` *  $array` |
|      - | 2315 | ` *   The input array.` |
|      - | 2316 | ` * $cmp_function` |
|      - | 2317 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 2318 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 2319 | ` *  to, or greater than the second.` |
|      - | 2320 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 2321 | ` * Return` |
|      - | 2322 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2323 | ` */` |
|      2 | 2324 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2325 |  |
|      - | 2326 | `	ph7_hashmap *pMap;` |
|      - | 2327 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2328 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2329 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2330 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2331 | `		return PH7_OK;` |
|      - | 2332 | `	}` |
|      - | 2333 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2334 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2335 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2336 | `		ph7_value *pCallback = 0;` |
|      - | 2337 | `		ProcNodeCmp xCmp;` |
|      3 | 2338 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      3 | 2339 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 2340 | `			/* Point to the desired callback */` |
|      3 | 2341 | `			pCallback = apArg[1];` |
|      2 | 2342 | `		}else{` |
|      - | 2343 | `			/* Use the default comparison function */` |
|    ! 0 | 2344 | `			xCmp = HashmapCmpCallback1;` |
|      - | 2345 | `		}` |
|      - | 2346 | `		/* Do the merge sort */` |
|      3 | 2347 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 2348 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      3 | 2349 | `		HashmapSortRehash(pMap);` |
|      1 | 2350 | `	}` |
|      - | 2351 | `	/* All done,return TRUE */` |
|      3 | 2352 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2353 | `	return PH7_OK;` |
|      2 | 2354 |  |
|      - | 2355 | `/*` |
|      - | 2356 | ` * bool uasort(array &$array,callable $cmp_function)` |
|      - | 2357 | ` *  Sort an array by values using a user-defined comparison function` |
|      - | 2358 | ` *  and maintain index association.` |
|      - | 2359 | ` * Parameters` |
|      - | 2360 | ` *  $array` |
|      - | 2361 | ` *   The input array.` |
|      - | 2362 | ` * $cmp_function` |
|      - | 2363 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 2364 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 2365 | ` *  to, or greater than the second.` |
|      - | 2366 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 2367 | ` * Return` |
|      - | 2368 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2369 | ` */` |
|      2 | 2370 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2371 |  |
|      - | 2372 | `	ph7_hashmap *pMap;` |
|      - | 2373 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2374 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2375 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2376 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2377 | `		return PH7_OK;` |
|      - | 2378 | `	}` |
|      - | 2379 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2380 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2381 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2382 | `		ph7_value *pCallback = 0;` |
|      - | 2383 | `		ProcNodeCmp xCmp;` |
|      3 | 2384 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      3 | 2385 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 2386 | `			/* Point to the desired callback */` |
|      3 | 2387 | `			pCallback = apArg[1];` |
|      2 | 2388 | `		}else{` |
|      - | 2389 | `			/* Use the default comparison function */` |
|    ! 0 | 2390 | `			xCmp = HashmapCmpCallback1;` |
|      - | 2391 | `		}` |
|      - | 2392 | `		/* Do the merge sort */` |
|      3 | 2393 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 2394 | `		/* Fix the last link broken by the merge */` |
|      5 | 2395 | `		while(pMap->pLast->pPrev){` |
|      3 | 2396 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2397 | `		}` |
|      1 | 2398 | `	}` |
|      - | 2399 | `	/* All done,return TRUE */` |
|      3 | 2400 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2401 | `	return PH7_OK;` |
|      2 | 2402 |  |
|      - | 2403 | `/*` |
|      - | 2404 | ` * bool uksort(array &$array,callable $cmp_function)` |
|      - | 2405 | ` *  Sort an array by keys using a user-defined comparison` |
|      - | 2406 | ` *  function and maintain index association.` |
|      - | 2407 | ` * Parameters` |
|      - | 2408 | ` *  $array` |
|      - | 2409 | ` *   The input array.` |
|      - | 2410 | ` * $cmp_function` |
|      - | 2411 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 2412 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 2413 | ` *  to, or greater than the second.` |
|      - | 2414 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 2415 | ` * Return` |
|      - | 2416 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2417 | ` */` |
|      2 | 2418 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2419 |  |
|      - | 2420 | `	ph7_hashmap *pMap;` |
|      - | 2421 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2422 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2423 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2424 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2425 | `		return PH7_OK;` |
|      - | 2426 | `	}` |
|      - | 2427 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2428 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2429 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2430 | `		ph7_value *pCallback = 0;` |
|      - | 2431 | `		ProcNodeCmp xCmp;` |
|      3 | 2432 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|      3 | 2433 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 2434 | `			/* Point to the desired callback */` |
|      3 | 2435 | `			pCallback = apArg[1];` |
|      2 | 2436 | `		}else{` |
|      - | 2437 | `			/* Use the default comparison function */` |
|    ! 0 | 2438 | `			xCmp = HashmapCmpCallback2;` |
|      - | 2439 | `		}` |
|      - | 2440 | `		/* Do the merge sort */` |
|      3 | 2441 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 2442 | `		/* Fix the last link broken by the merge */` |
|      3 | 2443 | `		while(pMap->pLast->pPrev){` |
|    ! 0 | 2444 | `			pMap->pLast = pMap->pLast->pPrev;` |
|    ! 0 | 2445 | `		}` |
|      1 | 2446 | `	}` |
|      - | 2447 | `	/* All done,return TRUE */` |
|      3 | 2448 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2449 | `	return PH7_OK;` |
|      2 | 2450 |  |
|      - | 2451 | `/*` |
|      - | 2452 | ` * bool shuffle(array &$array)` |
|      - | 2453 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|      - | 2454 | ` * Parameters` |
|      - | 2455 | ` *  $array` |
|      - | 2456 | ` *   The input array.` |
|      - | 2457 | ` * Return` |
|      - | 2458 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2459 | ` *` |
|      - | 2460 | ` */` |
|      2 | 2461 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2462 |  |
|      - | 2463 | `	ph7_hashmap *pMap;` |
|      - | 2464 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2465 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2466 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2467 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2468 | `		return PH7_OK;` |
|      - | 2469 | `	}` |
|      - | 2470 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2471 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2472 | `	if( pMap->nEntry > 1 ){` |
|      - | 2473 | `		/* Do the merge sort */` |
|      3 | 2474 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|      - | 2475 | `		/* Fix the last link broken by the merge */` |
|     10 | 2476 | `		while(pMap->pLast->pPrev){` |
|      8 | 2477 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2478 | `		}` |
|      1 | 2479 | `	}` |
|      - | 2480 | `	/* All done,return TRUE */` |
|      3 | 2481 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2482 | `	return PH7_OK;` |
|      2 | 2483 |  |
|      - | 2484 | `/*` |
|      - | 2485 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|      - | 2486 | ` *   Count all elements in an array, or something in an object.` |
|      - | 2487 | ` * Parameters` |
|      - | 2488 | ` *  $var` |
|      - | 2489 | ` *   The array or the object.` |
|      - | 2490 | ` * $mode` |
|      - | 2491 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|      - | 2492 | ` *  will recursively count the array. This is particularly useful for counting` |
|      - | 2493 | ` *  all the elements of a multidimensional array. count() does not detect infinite` |
|      - | 2494 | ` *  recursion.` |
|      - | 2495 | ` * Return` |
|      - | 2496 | ` *  Returns the number of elements in the array.` |
|      - | 2497 | ` */` |
|    436 | 2498 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2499 |  |
|    438 | 2500 | `	int bRecursive = FALSE;` |
|      - | 2501 | `	sxi64 iCount;` |
|    438 | 2502 | `	if( nArg < 1 ){` |
|      - | 2503 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 2504 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2505 | `		return PH7_OK;` |
|      - | 2506 | `	}` |
|    438 | 2507 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2508 | `		/* TICKET 1433-19: Handle objects */` |
|      3 | 2509 | `		int res = !ph7_value_is_null(apArg[0]);` |
|      3 | 2510 | `		ph7_result_int(pCtx,res);` |
|      3 | 2511 | `		return PH7_OK;` |
|      - | 2512 | `	}` |
|    436 | 2513 | `	if( nArg > 1 ){` |
|      - | 2514 | `		/* Recursive count? */` |
|     31 | 2515 | `		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;` |
|     15 | 2516 | `	}` |
|      - | 2517 | `	/* Count */` |
|    436 | 2518 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);` |
|    436 | 2519 | `	ph7_result_int64(pCtx,iCount);` |
|    436 | 2520 | `	return PH7_OK;` |
|    220 | 2521 |  |
|      - | 2522 | `/*` |
|      - | 2523 | ` * bool array_key_exists(value $key,array $search)` |
|      - | 2524 | ` *  Checks if the given key or index exists in the array.` |
|      - | 2525 | ` * Parameters` |
|      - | 2526 | ` * $key` |
|      - | 2527 | ` *   Value to check.` |
|      - | 2528 | ` * $search` |
|      - | 2529 | ` *  An array with keys to check.` |
|      - | 2530 | ` * Return` |
|      - | 2531 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2532 | ` */` |
|     32 | 2533 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2534 |  |
|      - | 2535 | `	sxi32 rc;` |
|     33 | 2536 | `	if( nArg < 2 ){` |
|      - | 2537 | `		/* Missing arguments,return FALSE */` |
|      7 | 2538 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2539 | `		return PH7_OK;` |
|      - | 2540 | `	}` |
|      - | 2541 | `	/* Make sure we are dealing with a valid hashmap */` |
|     27 | 2542 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 2543 | `		/* Invalid argument,return FALSE */` |
|      3 | 2544 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2545 | `		return PH7_OK;` |
|      - | 2546 | `	}` |
|      - | 2547 | `	/* Perform the lookup */` |
|     25 | 2548 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|      - | 2549 | `	/* lookup result */` |
|     25 | 2550 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     25 | 2551 | `	return PH7_OK;` |
|     17 | 2552 |  |
|      - | 2553 | `/*` |
|      - | 2554 | ` * value array_pop(array $array)` |
|      - | 2555 | ` *   POP the last inserted element from the array.` |
|      - | 2556 | ` * Parameter` |
|      - | 2557 | ` *  The array to get the value from.` |
|      - | 2558 | ` * Return` |
|      - | 2559 | ` *  Poped value or NULL on failure.` |
|      - | 2560 | ` */` |
|      4 | 2561 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2562 |  |
|      - | 2563 | `	ph7_hashmap *pMap;` |
|      5 | 2564 | `	if( nArg < 1 ){` |
|      - | 2565 | `		/* Missing arguments,return null */` |
|    ! 0 | 2566 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2567 | `		return PH7_OK;` |
|      - | 2568 | `	}` |
|      - | 2569 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 2570 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2571 | `		/* Invalid argument,return null */` |
|    ! 0 | 2572 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2573 | `		return PH7_OK;` |
|      - | 2574 | `	}` |
|      5 | 2575 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 2576 | `	if( pMap->nEntry < 1 ){` |
|      - | 2577 | `		/* Noting to pop,return NULL */` |
|      3 | 2578 | `		ph7_result_null(pCtx);` |
|      2 | 2579 | `	}else{` |
|      3 | 2580 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|      - | 2581 | `		ph7_value *pObj;` |
|      3 | 2582 | `		pObj = HashmapExtractNodeValue(pLast);` |
|      3 | 2583 | `		if( pObj ){` |
|      - | 2584 | `			/* Node value */` |
|      3 | 2585 | `			ph7_result_value(pCtx,pObj);` |
|      - | 2586 | `			/* Unlink the node */` |
|      3 | 2587 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|      2 | 2588 | `		}else{` |
|    ! 0 | 2589 | `			ph7_result_null(pCtx);` |
|      - | 2590 | `		}` |
|      - | 2591 | `		/* Reset the cursor */` |
|      3 | 2592 | `		pMap->pCur = pMap->pFirst;` |
|      - | 2593 | `	}` |
|      5 | 2594 | `	return PH7_OK;` |
|      3 | 2595 |  |
|      - | 2596 | `/*` |
|      - | 2597 | ` * int array_push($array,$var,...)` |
|      - | 2598 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|      - | 2599 | ` * Parameters` |
|      - | 2600 | ` *  array` |
|      - | 2601 | ` *    The input array.` |
|      - | 2602 | ` *  var` |
|      - | 2603 | ` *   On or more value to push.` |
|      - | 2604 | ` * Return` |
|      - | 2605 | ` *  New array count (including old items).` |
|      - | 2606 | ` */` |
|      2 | 2607 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2608 |  |
|      - | 2609 | `	ph7_hashmap *pMap;` |
|      - | 2610 | `	sxi32 rc;` |
|      - | 2611 | `	int i;` |
|      3 | 2612 | `	if( nArg < 1 ){` |
|      - | 2613 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 2614 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2615 | `		return PH7_OK;` |
|      - | 2616 | `	}` |
|      - | 2617 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2618 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2619 | `		/* Invalid argument,return 0 */` |
|    ! 0 | 2620 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2621 | `		return PH7_OK;` |
|      - | 2622 | `	}` |
|      - | 2623 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2624 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 2625 | `	/* Start pushing given values */` |
|      7 | 2626 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      5 | 2627 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      5 | 2628 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 2629 | `			break;` |
|      - | 2630 | `		}` |
|      3 | 2631 | `	}` |
|      - | 2632 | `	/* Return the new count */` |
|      3 | 2633 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      3 | 2634 | `	return PH7_OK;` |
|      2 | 2635 |  |
|      - | 2636 | `/*` |
|      - | 2637 | ` * value array_shift(array $array)` |
|      - | 2638 | ` *   Shift an element off the beginning of array.` |
|      - | 2639 | ` * Parameter` |
|      - | 2640 | ` *  The array to get the value from.` |
|      - | 2641 | ` * Return` |
|      - | 2642 | ` *  Shifted value or NULL on failure.` |
|      - | 2643 | ` */` |
|     24 | 2644 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2645 |  |
|      - | 2646 | `	ph7_hashmap *pMap;` |
|     26 | 2647 | `	if( nArg < 1 ){` |
|      - | 2648 | `		/* Missing arguments,return null */` |
|    ! 0 | 2649 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2650 | `		return PH7_OK;` |
|      - | 2651 | `	}` |
|      - | 2652 | `	/* Make sure we are dealing with a valid hashmap */` |
|     26 | 2653 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2654 | `		/* Invalid argument,return null */` |
|    ! 0 | 2655 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2656 | `		return PH7_OK;` |
|      - | 2657 | `	}` |
|      - | 2658 | `	/* Point to the internal representation of the hashmap */` |
|     26 | 2659 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     26 | 2660 | `	if( pMap->nEntry < 1 ){` |
|      - | 2661 | `		/* Empty hashmap,return NULL */` |
|      3 | 2662 | `		ph7_result_null(pCtx);` |
|      2 | 2663 | `	}else{` |
|     24 | 2664 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      - | 2665 | `		ph7_value *pObj;` |
|      - | 2666 | `		sxu32 n;` |
|     24 | 2667 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     24 | 2668 | `		if( pObj ){` |
|      - | 2669 | `			/* Node value */` |
|     24 | 2670 | `			ph7_result_value(pCtx,pObj);` |
|      - | 2671 | `			/* Unlink the first node */` |
|     24 | 2672 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|     13 | 2673 | `		}else{` |
|    ! 0 | 2674 | `			ph7_result_null(pCtx);` |
|      - | 2675 | `		}` |
|      - | 2676 | `		/* Rehash all int keys */` |
|     24 | 2677 | `		n = pMap->nEntry;` |
|     24 | 2678 | `		pEntry = pMap->pFirst;` |
|     24 | 2679 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     34 | 2680 | `		for(;;){` |
|     70 | 2681 | `			if( n < 1 ){` |
|     24 | 2682 | `				break;` |
|      - | 2683 | `			}` |
|     48 | 2684 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     48 | 2685 | `				HashmapRehashIntNode(pEntry);` |
|     23 | 2686 | `			}` |
|      - | 2687 | `			/* Point to the next entry */` |
|     48 | 2688 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     48 | 2689 | `			n--;` |
|      2 | 2690 | `		}` |
|      - | 2691 | `		/* Reset the cursor */` |
|     24 | 2692 | `		pMap->pCur = pMap->pFirst;` |
|      - | 2693 | `	}` |
|     26 | 2694 | `	return PH7_OK;` |
|     14 | 2695 |  |
|      - | 2696 | `/*` |
|      - | 2697 | ` * Extract the node cursor value.` |
|      - | 2698 | ` */` |
|     24 | 2699 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|      1 | 2700 |  |
|     25 | 2701 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|      - | 2702 | `	ph7_value *pVal;` |
|     25 | 2703 | `	if( pCur == 0 ){` |
|      - | 2704 | `		/* Cursor does not point to anything,return FALSE */` |
|    ! 0 | 2705 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2706 | `		return PH7_OK;` |
|      - | 2707 | `	}` |
|     25 | 2708 | `	if( iDirection != 0 ){` |
|      9 | 2709 | `		if( iDirection > 0 ){` |
|      - | 2710 | `			/* Point to the next entry */` |
|      7 | 2711 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|      7 | 2712 | `			pCur = pMap->pCur;` |
|      4 | 2713 | `		}else{` |
|      - | 2714 | `			/* Point to the previous entry */` |
|      3 | 2715 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|      3 | 2716 | `			pCur = pMap->pCur;` |
|      - | 2717 | `		}` |
|      9 | 2718 | `		if( pCur == 0 ){` |
|      - | 2719 | `			/* End of input reached,return FALSE */` |
|    ! 0 | 2720 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2721 | `			return PH7_OK;` |
|      - | 2722 | `		}` |
|      4 | 2723 | `	}` |
|      - | 2724 | `	/* Point to the desired element */` |
|     25 | 2725 | `	pVal = HashmapExtractNodeValue(pCur);` |
|     25 | 2726 | `	if( pVal ){` |
|     25 | 2727 | `		ph7_result_value(pCtx,pVal);` |
|     13 | 2728 | `	}else{` |
|    ! 0 | 2729 | `		ph7_result_bool(pCtx,0);` |
|      - | 2730 | `	}` |
|     25 | 2731 | `	return PH7_OK;` |
|     13 | 2732 |  |
|      - | 2733 | `/*` |
|      - | 2734 | ` * value current(array $array)` |
|      - | 2735 | ` *  Return the current element in an array.` |
|      - | 2736 | ` * Parameter` |
|      - | 2737 | ` *  $input: The input array.` |
|      - | 2738 | ` * Return` |
|      - | 2739 | ` *  The current() function simply returns the value of the array element that's currently` |
|      - | 2740 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|      - | 2741 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|      - | 2742 | ` *  is empty, current() returns FALSE.` |
|      - | 2743 | ` */` |
|     10 | 2744 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2745 |  |
|     11 | 2746 | `	if( nArg < 1 ){` |
|      - | 2747 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2748 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2749 | `		return PH7_OK;` |
|      - | 2750 | `	}` |
|      - | 2751 | `	/* Make sure we are dealing with a valid hashmap */` |
|     11 | 2752 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2753 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2754 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2755 | `		return PH7_OK;` |
|      - | 2756 | `	}` |
|     11 | 2757 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|     11 | 2758 | `	return PH7_OK;` |
|      6 | 2759 |  |
|      - | 2760 | `/*` |
|      - | 2761 | ` * value next(array $input)` |
|      - | 2762 | ` *  Advance the internal array pointer of an array.` |
|      - | 2763 | ` * Parameter` |
|      - | 2764 | ` *  $input: The input array.` |
|      - | 2765 | ` * Return` |
|      - | 2766 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|      - | 2767 | ` *  pointer one place forward before returning the element value. That means it returns` |
|      - | 2768 | ` *  the next array value and advances the internal array pointer by one.` |
|      - | 2769 | ` */` |
|      6 | 2770 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2771 |  |
|      7 | 2772 | `	if( nArg < 1 ){` |
|      - | 2773 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2774 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2775 | `		return PH7_OK;` |
|      - | 2776 | `	}` |
|      - | 2777 | `	/* Make sure we are dealing with a valid hashmap */` |
|      7 | 2778 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2779 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2780 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2781 | `		return PH7_OK;` |
|      - | 2782 | `	}` |
|      7 | 2783 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|      7 | 2784 | `	return PH7_OK;` |
|      4 | 2785 |  |
|      - | 2786 | `/*` |
|      - | 2787 | ` * value prev(array $input)` |
|      - | 2788 | ` *  Rewind the internal array pointer.` |
|      - | 2789 | ` * Parameter` |
|      - | 2790 | ` *  $input: The input array.` |
|      - | 2791 | ` * Return` |
|      - | 2792 | ` *  Returns the array value in the previous place that's pointed` |
|      - | 2793 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|      - | 2794 | ` *  elements.` |
|      - | 2795 | ` */` |
|      2 | 2796 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2797 |  |
|      3 | 2798 | `	if( nArg < 1 ){` |
|      - | 2799 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2800 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2801 | `		return PH7_OK;` |
|      - | 2802 | `	}` |
|      - | 2803 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2804 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2805 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2806 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2807 | `		return PH7_OK;` |
|      - | 2808 | `	}` |
|      3 | 2809 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|      3 | 2810 | `	return PH7_OK;` |
|      2 | 2811 |  |
|      - | 2812 | `/*` |
|      - | 2813 | ` * value end(array $input)` |
|      - | 2814 | ` *  Set the internal pointer of an array to its last element.` |
|      - | 2815 | ` * Parameter` |
|      - | 2816 | ` *  $input: The input array.` |
|      - | 2817 | ` * Return` |
|      - | 2818 | ` *  Returns the value of the last element or FALSE for empty array.` |
|      - | 2819 | ` */` |
|      2 | 2820 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2821 |  |
|      - | 2822 | `	ph7_hashmap *pMap;` |
|      3 | 2823 | `	if( nArg < 1 ){` |
|      - | 2824 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2825 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2826 | `		return PH7_OK;` |
|      - | 2827 | `	}` |
|      - | 2828 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2829 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2830 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2831 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2832 | `		return PH7_OK;` |
|      - | 2833 | `	}` |
|      - | 2834 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2835 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 2836 | `	/* Point to the last node */` |
|      3 | 2837 | `	pMap->pCur = pMap->pLast;` |
|      - | 2838 | `	/* Return the last node value */` |
|      3 | 2839 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|      3 | 2840 | `	return PH7_OK;` |
|      2 | 2841 |  |
|      - | 2842 | `/*` |
|      - | 2843 | ` * value reset(array $array )` |
|      - | 2844 | ` *  Set the internal pointer of an array to its first element.` |
|      - | 2845 | ` * Parameter` |
|      - | 2846 | ` *  $input: The input array.` |
|      - | 2847 | ` * Return` |
|      - | 2848 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|      - | 2849 | ` */` |
|      4 | 2850 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2851 |  |
|      - | 2852 | `	ph7_hashmap *pMap;` |
|      5 | 2853 | `	if( nArg < 1 ){` |
|      - | 2854 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2855 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2856 | `		return PH7_OK;` |
|      - | 2857 | `	}` |
|      - | 2858 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 2859 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2860 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2861 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2862 | `		return PH7_OK;` |
|      - | 2863 | `	}` |
|      - | 2864 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 2865 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 2866 | `	/* Point to the first node */` |
|      5 | 2867 | `	pMap->pCur = pMap->pFirst;` |
|      - | 2868 | `	/* Return the last node value if available */` |
|      5 | 2869 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|      5 | 2870 | `	return PH7_OK;` |
|      3 | 2871 |  |
|      - | 2872 | `/*` |
|      - | 2873 | ` * value key(array $array)` |
|      - | 2874 | ` *   Fetch a key from an array` |
|      - | 2875 | ` * Parameter` |
|      - | 2876 | ` *  $input` |
|      - | 2877 | ` *   The input array.` |
|      - | 2878 | ` * Return` |
|      - | 2879 | ` *  The key() function simply returns the key of the array element that's currently` |
|      - | 2880 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|      - | 2881 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|      - | 2882 | ` *  is empty, key() returns NULL.` |
|      - | 2883 | ` */` |
|      4 | 2884 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2885 |  |
|      - | 2886 | `	ph7_hashmap_node *pCur;` |
|      - | 2887 | `	ph7_hashmap *pMap;` |
|      5 | 2888 | `	if( nArg < 1 ){` |
|      - | 2889 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 2890 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2891 | `		return PH7_OK;` |
|      - | 2892 | `	}` |
|      - | 2893 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 2894 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2895 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 2896 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2897 | `		return PH7_OK;` |
|      - | 2898 | `	}` |
|      5 | 2899 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 2900 | `	pCur = pMap->pCur;` |
|      5 | 2901 | `	if( pCur == 0 ){` |
|      - | 2902 | `		/* Cursor does not point to anything,return NULL */` |
|    ! 0 | 2903 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2904 | `		return PH7_OK;` |
|      - | 2905 | `	}` |
|      5 | 2906 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|      - | 2907 | `		/* Key is integer */` |
|    ! 0 | 2908 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|    ! 0 | 2909 | `	}else{` |
|      - | 2910 | `		/* Key is blob */` |
|      7 | 2911 | `		ph7_result_string(pCtx,` |
|      4 | 2912 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|      - | 2913 | `	}` |
|      5 | 2914 | `	return PH7_OK;` |
|      3 | 2915 |  |
|      - | 2916 | `/*` |
|      - | 2917 | ` * array each(array $input)` |
|      - | 2918 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|      - | 2919 | ` * Parameter` |
|      - | 2920 | ` *  $input` |
|      - | 2921 | ` *    The input array.` |
|      - | 2922 | ` * Return` |
|      - | 2923 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|      - | 2924 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|      - | 2925 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|      - | 2926 | ` *  If the internal pointer for the array points past the end of the array contents` |
|      - | 2927 | ` *  each() returns FALSE.` |
|      - | 2928 | ` */` |
|     22 | 2929 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2930 |  |
|      - | 2931 | `	ph7_hashmap_node *pCur;` |
|      - | 2932 | `	ph7_hashmap *pMap;` |
|      - | 2933 | `	ph7_value *pArray;` |
|      - | 2934 | `	ph7_value *pVal;` |
|      - | 2935 | `	ph7_value sKey;` |
|     23 | 2936 | `	if( nArg < 1 ){` |
|      - | 2937 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2938 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2939 | `		return PH7_OK;` |
|      - | 2940 | `	}` |
|      - | 2941 | `	/* Make sure we are dealing with a valid hashmap */` |
|     23 | 2942 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2943 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2944 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2945 | `		return PH7_OK;` |
|      - | 2946 | `	}` |
|      - | 2947 | `	/* Point to the internal representation that describe the input hashmap */` |
|     23 | 2948 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     23 | 2949 | `	if( pMap->pCur == 0 ){` |
|      - | 2950 | `		/* Cursor does not point to anything,return FALSE */` |
|      9 | 2951 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2952 | `		return PH7_OK;` |
|      - | 2953 | `	}` |
|     15 | 2954 | `	pCur = pMap->pCur;` |
|      - | 2955 | `	/* Create a new array */` |
|     15 | 2956 | `	pArray = ph7_context_new_array(pCtx);` |
|     15 | 2957 | `	if( pArray == 0 ){` |
|    ! 0 | 2958 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2959 | `		return PH7_OK;` |
|      - | 2960 | `	}` |
|     15 | 2961 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      - | 2962 | `	/* Insert the current value */` |
|     15 | 2963 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|     15 | 2964 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|      - | 2965 | `	/* Make the key */` |
|     15 | 2966 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|      7 | 2967 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|      4 | 2968 | `	}else{` |
|      9 | 2969 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|      9 | 2970 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|      - | 2971 | `	}` |
|      - | 2972 | `	/* Insert the current key */` |
|     15 | 2973 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|     15 | 2974 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|     15 | 2975 | `	PH7_MemObjRelease(&sKey);` |
|      - | 2976 | `	/* Advance the cursor */` |
|     15 | 2977 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|      - | 2978 | `	/* Return the current entry */` |
|     15 | 2979 | `	ph7_result_value(pCtx,pArray);` |
|     15 | 2980 | `	return PH7_OK;` |
|     12 | 2981 |  |
|      - | 2982 | `/*` |
|      - | 2983 | ` * array range(int $start,int $limit,int $step)` |
|      - | 2984 | ` *  Create an array containing a range of elements` |
|      - | 2985 | ` * Parameter` |
|      - | 2986 | ` *  start` |
|      - | 2987 | ` *   First value of the sequence.` |
|      - | 2988 | ` *  limit` |
|      - | 2989 | ` *   The sequence is ended upon reaching the limit value.` |
|      - | 2990 | ` *  step` |
|      - | 2991 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|      - | 2992 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|      - | 2993 | ` * Return` |
|      - | 2994 | ` *  An array of elements from start to limit, inclusive.` |
|      - | 2995 | ` * NOTE:` |
|      - | 2996 | ` *  Only 32/64 bit integer key is supported.` |
|      - | 2997 | ` */` |
|      2 | 2998 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2999 |  |
|      - | 3000 | `	ph7_value *pValue,*pArray;` |
|      - | 3001 | `	sxi64 iOfft,iLimit;` |
|      3 | 3002 | `	int iStep = 1;` |
|      - | 3003 |  |
|      3 | 3004 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|      3 | 3005 | `	if( nArg > 0 ){` |
|      - | 3006 | `		/* Extract the offset */` |
|      3 | 3007 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|      3 | 3008 | `		if( nArg > 1 ){` |
|      - | 3009 | `			/* Extract the limit */` |
|      3 | 3010 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|      3 | 3011 | `			if( nArg > 2 ){` |
|      - | 3012 | `				/* Extract the increment */` |
|      3 | 3013 | `				iStep = ph7_value_to_int(apArg[2]);` |
|      3 | 3014 | `				if( iStep < 1 ){` |
|      - | 3015 | `					/* Only positive number are allowed */` |
|      3 | 3016 | `					iStep = 1;` |
|      1 | 3017 | `				}` |
|      1 | 3018 | `			}` |
|      1 | 3019 | `		}` |
|      1 | 3020 | `	}` |
|      - | 3021 | `	/* Element container */` |
|      3 | 3022 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      - | 3023 | `	/* Create the new array */` |
|      3 | 3024 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3025 | `	if( pArray == 0 ){` |
|    ! 0 | 3026 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3027 | `		return PH7_OK;` |
|      - | 3028 | `	}` |
|      - | 3029 | `	/* Start filling */` |
|      3 | 3030 | `	while( iOfft <= iLimit ){` |
|    ! 0 | 3031 | `		ph7_value_int64(pValue,iOfft);` |
|      - | 3032 | `		/* Perform the insertion */` |
|    ! 0 | 3033 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|      - | 3034 | `		/* Increment */` |
|    ! 0 | 3035 | `		iOfft += iStep;` |
|    ! 0 | 3036 | `	}` |
|      - | 3037 | `	/* Return the new array */` |
|      3 | 3038 | `	ph7_result_value(pCtx,pArray);` |
|      - | 3039 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|      - | 3040 | `	 * by the virtual machine as soon we return from this foreign function.` |
|      - | 3041 | `	 */` |
|      3 | 3042 | `	return PH7_OK;` |
|      2 | 3043 |  |
|      - | 3044 | `/*` |
|      - | 3045 | ` * array array_values(array $input)` |
|      - | 3046 | ` *   Returns all the values from the input array and indexes numerically the array.` |
|      - | 3047 | ` * Parameters` |
|      - | 3048 | ` *   input: The input array.` |
|      - | 3049 | ` * Return` |
|      - | 3050 | ` *  An indexed array of values or NULL on failure.` |
|      - | 3051 | ` */` |
|     18 | 3052 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3053 |  |
|      - | 3054 | `	ph7_hashmap_node *pNode;` |
|      - | 3055 | `	ph7_hashmap *pMap;` |
|      - | 3056 | `	ph7_value *pArray;` |
|      - | 3057 | `	ph7_value *pObj;` |
|      - | 3058 | `	sxu32 n;` |
|     19 | 3059 | `	if( nArg < 1 ){` |
|      - | 3060 | `		/* Missing arguments,return NULL */` |
|      3 | 3061 | `		ph7_result_null(pCtx);` |
|      3 | 3062 | `		return PH7_OK;` |
|      - | 3063 | `	}` |
|      - | 3064 | `	/* Make sure we are dealing with a valid hashmap */` |
|     17 | 3065 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 3066 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 3067 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3068 | `		return PH7_OK;` |
|      - | 3069 | `	}` |
|      - | 3070 | `	/* Point to the internal representation that describe the input hashmap */` |
|     17 | 3071 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3072 | `	/* Create a new array */` |
|     17 | 3073 | `	pArray = ph7_context_new_array(pCtx);` |
|     17 | 3074 | `	if( pArray == 0 ){` |
|    ! 0 | 3075 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3076 | `		return PH7_OK;` |
|      - | 3077 | `	}` |
|      - | 3078 | `	/* Perform the requested operation */` |
|     17 | 3079 | `	pNode = pMap->pFirst;` |
|     61 | 3080 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     45 | 3081 | `		pObj = HashmapExtractNodeValue(pNode);` |
|     45 | 3082 | `		if( pObj ){` |
|      - | 3083 | `			/* perform the insertion */` |
|     45 | 3084 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|     22 | 3085 | `		}` |
|      - | 3086 | `		/* Point to the next entry */` |
|     45 | 3087 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     23 | 3088 | `	}` |
|      - | 3089 | `	/* return the new array */` |
|     17 | 3090 | `	ph7_result_value(pCtx,pArray);` |
|     17 | 3091 | `	return PH7_OK;` |
|     10 | 3092 |  |
|      - | 3093 | `/*` |
|      - | 3094 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|      - | 3095 | ` *  Return all the keys or a subset of the keys of an array.` |
|      - | 3096 | ` * Parameters` |
|      - | 3097 | ` *  $input` |
|      - | 3098 | ` *   An array containing keys to return.` |
|      - | 3099 | ` * $search_value` |
|      - | 3100 | ` *   If specified, then only keys containing these values are returned.` |
|      - | 3101 | ` * $strict` |
|      - | 3102 | ` *   Determines if strict comparison (===) should be used during the search.` |
|      - | 3103 | ` * Return` |
|      - | 3104 | ` *  An array of all the keys in input or NULL on failure.` |
|      - | 3105 | ` */` |
|     68 | 3106 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3107 |  |
|      - | 3108 | `	ph7_hashmap_node *pNode;` |
|      - | 3109 | `	ph7_hashmap *pMap;` |
|      - | 3110 | `	ph7_value *pArray;` |
|      - | 3111 | `	ph7_value sObj;` |
|      - | 3112 | `	ph7_value sVal;` |
|      - | 3113 | `	SyString sKey;` |
|      - | 3114 | `	int bStrict;` |
|      - | 3115 | `	sxi32 rc;` |
|      - | 3116 | `	sxu32 n;` |
|     69 | 3117 | `	if( nArg < 1 ){` |
|      - | 3118 | `		/* Missing arguments,return NULL */` |
|      3 | 3119 | `		ph7_result_null(pCtx);` |
|      3 | 3120 | `		return PH7_OK;` |
|      - | 3121 | `	}` |
|      - | 3122 | `	/* Make sure we are dealing with a valid hashmap */` |
|     67 | 3123 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 3124 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 3125 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3126 | `		return PH7_OK;` |
|      - | 3127 | `	}` |
|      - | 3128 | `	/* Point to the internal representation of the input hashmap */` |
|     67 | 3129 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3130 | `	/* Create a new array */` |
|     67 | 3131 | `	pArray = ph7_context_new_array(pCtx);` |
|     67 | 3132 | `	if( pArray == 0 ){` |
|    ! 0 | 3133 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3134 | `		return PH7_OK;` |
|      - | 3135 | `	}` |
|     67 | 3136 | `	bStrict = FALSE;` |
|     67 | 3137 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|    ! 0 | 3138 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|    ! 0 | 3139 | `	}` |
|      - | 3140 | `	/* Perform the requested operation */` |
|     67 | 3141 | `	pNode = pMap->pFirst;` |
|     67 | 3142 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    395 | 3143 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    329 | 3144 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     49 | 3145 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|     25 | 3146 | `		}else{` |
|    281 | 3147 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    281 | 3148 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|      - | 3149 | `		}` |
|    329 | 3150 | `		rc = 0;` |
|    329 | 3151 | `		if( nArg > 1 ){` |
|    ! 0 | 3152 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|    ! 0 | 3153 | `			if( pValue ){` |
|    ! 0 | 3154 | `				PH7_MemObjLoad(pValue,&sVal);` |
|      - | 3155 | `				/* Filter key */` |
|    ! 0 | 3156 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|    ! 0 | 3157 | `				PH7_MemObjRelease(pValue);` |
|    ! 0 | 3158 | `			}` |
|    ! 0 | 3159 | `		}` |
|    329 | 3160 | `		if( rc == 0 ){` |
|      - | 3161 | `			/* Perform the insertion */` |
|    329 | 3162 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|    164 | 3163 | `		}` |
|    329 | 3164 | `		PH7_MemObjRelease(&sObj);` |
|      - | 3165 | `		/* Point to the next entry */` |
|    329 | 3166 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    165 | 3167 | `	}` |
|      - | 3168 | `	/* return the new array */` |
|     67 | 3169 | `	ph7_result_value(pCtx,pArray);` |
|     67 | 3170 | `	return PH7_OK;` |
|     35 | 3171 |  |
|      - | 3172 | `/*` |
|      - | 3173 | ` * bool array_same(array $arr1,array $arr2)` |
|      - | 3174 | ` *  Return TRUE if the given arrays are the same instance.` |
|      - | 3175 | ` *  This function is useful under PH7 since arrays are passed` |
|      - | 3176 | ` *  by reference unlike the zend engine which use pass by values.` |
|      - | 3177 | ` * Parameters` |
|      - | 3178 | ` *  $arr1` |
|      - | 3179 | ` *   First array` |
|      - | 3180 | ` *  $arr2` |
|      - | 3181 | ` *   Second array` |
|      - | 3182 | ` * Return` |
|      - | 3183 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|      - | 3184 | ` * Note` |
|      - | 3185 | ` *  This function is a symisc eXtension.` |
|      - | 3186 | ` */` |
|      4 | 3187 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3188 |  |
|      - | 3189 | `	ph7_hashmap *p1,*p2;` |
|      - | 3190 | `	int rc;` |
|      5 | 3191 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3192 | `		/* Missing or invalid arguments,return FALSE*/` |
|    ! 0 | 3193 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3194 | `		return PH7_OK;` |
|      - | 3195 | `	}` |
|      - | 3196 | `	/* Point to the hashmaps */` |
|      5 | 3197 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 3198 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 3199 | `	rc = (p1 == p2);` |
|      - | 3200 | `	/* Same instance? */` |
|      5 | 3201 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 3202 | `	return PH7_OK;` |
|      3 | 3203 |  |
|      - | 3204 | `/*` |
|      - | 3205 | ` * array array_merge(array $array1,...)` |
|      - | 3206 | ` *  Merge one or more arrays.` |
|      - | 3207 | ` * Parameters` |
|      - | 3208 | ` *  $array1` |
|      - | 3209 | ` *    Initial array to merge.` |
|      - | 3210 | ` *  ...` |
|      - | 3211 | ` *   More array to merge.` |
|      - | 3212 | ` * Return` |
|      - | 3213 | ` *  The resulting array.` |
|      - | 3214 | ` */` |
|    764 | 3215 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3216 |  |
|      - | 3217 | `	ph7_hashmap *pMap,*pSrc;` |
|      - | 3218 | `	ph7_value *pArray;` |
|      - | 3219 | `	int i;` |
|    766 | 3220 | `	if( nArg < 1 ){` |
|      - | 3221 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3222 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3223 | `		return PH7_OK;` |
|      - | 3224 | `	}` |
|      - | 3225 | `	/* Create a new array */` |
|    766 | 3226 | `	pArray = ph7_context_new_array(pCtx);` |
|    766 | 3227 | `	if( pArray == 0 ){` |
|    ! 0 | 3228 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3229 | `		return PH7_OK;` |
|      - | 3230 | `	}` |
|      - | 3231 | `	/* Point to the internal representation of the hashmap */` |
|    766 | 3232 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      - | 3233 | `	/* Start merging */` |
|   2294 | 3234 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      - | 3235 | `		/* Make sure we are dealing with a valid hashmap */` |
|   1530 | 3236 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      - | 3237 | `			/* Insert scalar value */` |
|      5 | 3238 | `			ph7_array_add_elem(pArray,0,apArg[i]);` |
|      3 | 3239 | `		}else{` |
|   1526 | 3240 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3241 | `			/* Merge the two hashmaps */` |
|   1526 | 3242 | `			HashmapMerge(pSrc,pMap);` |
|      - | 3243 | `		}` |
|    766 | 3244 | `	}` |
|      - | 3245 | `	/* Return the freshly created array */` |
|    766 | 3246 | `	ph7_result_value(pCtx,pArray);` |
|    766 | 3247 | `	return PH7_OK;` |
|    384 | 3248 |  |
|      - | 3249 | `/*` |
|      - | 3250 | ` * array array_copy(array $source)` |
|      - | 3251 | ` *  Make a blind copy of the target array.` |
|      - | 3252 | ` * Parameters` |
|      - | 3253 | ` *  $source` |
|      - | 3254 | ` *   Target array` |
|      - | 3255 | ` * Return` |
|      - | 3256 | ` *  Copy of the target array on success.NULL otherwise.` |
|      - | 3257 | ` * Note` |
|      - | 3258 | ` *  This function is a symisc eXtension.` |
|      - | 3259 | ` */` |
|      2 | 3260 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3261 |  |
|      - | 3262 | `	ph7_hashmap *pMap;` |
|      - | 3263 | `	ph7_value *pArray;` |
|      3 | 3264 | `	if( nArg < 1 ){` |
|      - | 3265 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3266 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3267 | `		return PH7_OK;` |
|      - | 3268 | `	}` |
|      - | 3269 | `	/* Create a new array */` |
|      3 | 3270 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3271 | `	if( pArray == 0 ){` |
|    ! 0 | 3272 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3273 | `		return PH7_OK;` |
|      - | 3274 | `	}` |
|      - | 3275 | `	/* Point to the internal representation of the hashmap */` |
|      3 | 3276 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      3 | 3277 | `	if( ph7_value_is_array(apArg[0])){` |
|      - | 3278 | `		/* Point to the internal representation of the source */` |
|      3 | 3279 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3280 | `		/* Perform the copy */` |
|      3 | 3281 | `		PH7_HashmapDup(pSrc,pMap);` |
|      2 | 3282 | `	}else{` |
|      - | 3283 | `		/* Simple insertion */` |
|    ! 0 | 3284 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|      - | 3285 | `	}` |
|      - | 3286 | `	/* Return the duplicated array */` |
|      3 | 3287 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3288 | `	return PH7_OK;` |
|      2 | 3289 |  |
|      - | 3290 | `/*` |
|      - | 3291 | ` * bool array_erase(array $source)` |
|      - | 3292 | ` *  Remove all elements from a given array.` |
|      - | 3293 | ` * Parameters` |
|      - | 3294 | ` *  $source` |
|      - | 3295 | ` *   Target array` |
|      - | 3296 | ` * Return` |
|      - | 3297 | ` *  TRUE on success.FALSE otherwise.` |
|      - | 3298 | ` * Note` |
|      - | 3299 | ` *  This function is a symisc eXtension.` |
|      - | 3300 | ` */` |
|      2 | 3301 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3302 |  |
|      - | 3303 | `	ph7_hashmap *pMap;` |
|      3 | 3304 | `	if( nArg < 1 ){` |
|      - | 3305 | `		/* Missing arguments */` |
|    ! 0 | 3306 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3307 | `		return PH7_OK;` |
|      - | 3308 | `	}` |
|      - | 3309 | `	/* Point to the target hashmap */` |
|      3 | 3310 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3311 | `	/* Erase */` |
|      3 | 3312 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      3 | 3313 | `	return PH7_OK;` |
|      2 | 3314 |  |
|      - | 3315 | `/*` |
|      - | 3316 | ` * array array_slice(array $array,int $offset [,int $length [, bool $preserve_keys = false ]])` |
|      - | 3317 | ` *  Extract a slice of the array.` |
|      - | 3318 | ` * Parameters` |
|      - | 3319 | ` *  $array` |
|      - | 3320 | ` *    The input array.` |
|      - | 3321 | ` * $offset` |
|      - | 3322 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|      - | 3323 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|      - | 3324 | ` * $length (optional)` |
|      - | 3325 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|      - | 3326 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|      - | 3327 | ` *   elements from the end of the array. If it is omitted, then the sequence will have` |
|      - | 3328 | ` *   everything from offset up until the end of the array.` |
|      - | 3329 | ` * $preserve_keys (optional)` |
|      - | 3330 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|      - | 3331 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|      - | 3332 | ` * Return` |
|      - | 3333 | ` *   The new slice.` |
|      - | 3334 | ` */` |
|      8 | 3335 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3336 |  |
|      - | 3337 | `	ph7_hashmap *pMap,*pSrc;` |
|      - | 3338 | `	ph7_hashmap_node *pCur;` |
|      - | 3339 | `	ph7_value *pArray;` |
|      - | 3340 | `	int iLength,iOfft;` |
|      - | 3341 | `	int bPreserve;` |
|      - | 3342 | `	sxi32 rc;` |
|      9 | 3343 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3344 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3345 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3346 | `		return PH7_OK;` |
|      - | 3347 | `	}` |
|      - | 3348 | `	/* Point the internal representation of the target array */` |
|      9 | 3349 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      9 | 3350 | `	bPreserve = FALSE;` |
|      - | 3351 | `	/* Get the offset */` |
|      9 | 3352 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      9 | 3353 | `	if( iOfft < 0 ){` |
|      3 | 3354 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|      1 | 3355 | `	}` |
|      9 | 3356 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|      - | 3357 | `		/* Invalid offset,return the last entry */` |
|    ! 0 | 3358 | `		iOfft = (int)pSrc->nEntry - 1;` |
|    ! 0 | 3359 | `	}` |
|      - | 3360 | `	/* Get the length */` |
|      9 | 3361 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      9 | 3362 | `	if( nArg > 2 ){` |
|      7 | 3363 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      7 | 3364 | `		if( iLength < 0 ){` |
|    ! 0 | 3365 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|    ! 0 | 3366 | `		}` |
|      7 | 3367 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|    ! 0 | 3368 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|    ! 0 | 3369 | `		}` |
|      7 | 3370 | `		if( nArg > 3 && ph7_value_is_bool(apArg[3]) ){` |
|      3 | 3371 | `			bPreserve = ph7_value_to_bool(apArg[3]);` |
|      1 | 3372 | `		}` |
|      3 | 3373 | `	}` |
|      - | 3374 | `	/* Create a new array */` |
|      9 | 3375 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 3376 | `	if( pArray == 0 ){` |
|    ! 0 | 3377 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3378 | `		return PH7_OK;` |
|      - | 3379 | `	}` |
|      9 | 3380 | `	if( iLength < 1 ){` |
|      - | 3381 | `		/* Don't bother processing,return the empty array */` |
|    ! 0 | 3382 | `		ph7_result_value(pCtx,pArray);` |
|    ! 0 | 3383 | `		return PH7_OK;` |
|      - | 3384 | `	}` |
|      - | 3385 | `	/* Point to the desired entry */` |
|      9 | 3386 | `	pCur = pSrc->pFirst;` |
|      9 | 3387 | `	for(;;){` |
|     19 | 3388 | `		if( iOfft < 1 ){` |
|      9 | 3389 | `			break;` |
|      - | 3390 | `		}` |
|      - | 3391 | `		/* Point to the next entry */` |
|     11 | 3392 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     11 | 3393 | `		iOfft--;` |
|      1 | 3394 | `	}` |
|      - | 3395 | `	/* Point to the internal representation of the hashmap */` |
|      9 | 3396 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     12 | 3397 | `	for(;;){` |
|     25 | 3398 | `		if( iLength < 1 ){` |
|      9 | 3399 | `			break;` |
|      - | 3400 | `		}` |
|     17 | 3401 | `		rc = HashmapInsertNode(pMap,pCur,bPreserve);` |
|     17 | 3402 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3403 | `			break;` |
|      - | 3404 | `		}` |
|      - | 3405 | `		/* Point to the next entry */` |
|     17 | 3406 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     17 | 3407 | `		iLength--;` |
|      1 | 3408 | `	}` |
|      - | 3409 | `	/* Return the freshly created array */` |
|      9 | 3410 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 3411 | `	return PH7_OK;` |
|      5 | 3412 |  |
|      - | 3413 | `/*` |
|      - | 3414 | ` * array array_splice(array $array,int $offset [,int $length [,value $replacement ]])` |
|      - | 3415 | ` *  Remove a portion of the array and replace it with something else.` |
|      - | 3416 | ` * Parameters` |
|      - | 3417 | ` *  $array` |
|      - | 3418 | ` *    The input array.` |
|      - | 3419 | ` * $offset` |
|      - | 3420 | ` *    If offset is positive then the start of removed portion is at that offset from` |
|      - | 3421 | ` *    the beginning of the input array. If offset is negative then it starts that far` |
|      - | 3422 | ` *    from the end of the input array.` |
|      - | 3423 | ` * $length (optional)` |
|      - | 3424 | ` *    If length is omitted, removes everything from offset to the end of the array.` |
|      - | 3425 | ` *    If length is specified and is positive, then that many elements will be removed.` |
|      - | 3426 | ` *    If length is specified and is negative then the end of the removed portion will` |
|      - | 3427 | ` *    be that many elements from the end of the array.` |
|      - | 3428 | ` * $replacement (optional)` |
|      - | 3429 | ` *  If replacement array is specified, then the removed elements are replaced` |
|      - | 3430 | ` *  with elements from this array.` |
|      - | 3431 | ` *  If offset and length are such that nothing is removed, then the elements` |
|      - | 3432 | ` *  from the replacement array are inserted in the place specified by the offset.` |
|      - | 3433 | ` *  Note that keys in replacement array are not preserved.` |
|      - | 3434 | ` *  If replacement is just one element it is not necessary to put array() around` |
|      - | 3435 | ` *  it, unless the element is an array itself, an object or NULL.` |
|      - | 3436 | ` * Return` |
|      - | 3437 | ` *   A new array consisting of the extracted elements.` |
|      - | 3438 | ` */` |
|      2 | 3439 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3440 |  |
|      - | 3441 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode;` |
|      - | 3442 | `	ph7_value *pArray,*pRvalue,*pOld;` |
|      - | 3443 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|      - | 3444 | `	int iLength,iOfft;` |
|      - | 3445 | `	sxi32 rc;` |
|      3 | 3446 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3447 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3448 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3449 | `		return PH7_OK;` |
|      - | 3450 | `	}` |
|      - | 3451 | `	/* Point the internal representation of the target array */` |
|      3 | 3452 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3453 | `	/* Get the offset */` |
|      3 | 3454 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      3 | 3455 | `	if( iOfft < 0 ){` |
|    ! 0 | 3456 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|    ! 0 | 3457 | `	}` |
|      3 | 3458 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|      - | 3459 | `		/* Invalid offset,remove the last entry */` |
|    ! 0 | 3460 | `		iOfft = (int)pSrc->nEntry - 1;` |
|    ! 0 | 3461 | `	}` |
|      - | 3462 | `	/* Get the length */` |
|      3 | 3463 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      3 | 3464 | `	if( nArg > 2 ){` |
|      3 | 3465 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      3 | 3466 | `		if( iLength < 0 ){` |
|    ! 0 | 3467 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|    ! 0 | 3468 | `		}` |
|      3 | 3469 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|    ! 0 | 3470 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|    ! 0 | 3471 | `		}` |
|      1 | 3472 | `	}` |
|      - | 3473 | `	/* Create a new array */` |
|      3 | 3474 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3475 | `	if( pArray == 0 ){` |
|    ! 0 | 3476 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3477 | `		return PH7_OK;` |
|      - | 3478 | `	}` |
|      3 | 3479 | `	if( iLength < 1 ){` |
|      - | 3480 | `		/* Don't bother processing,return the empty array */` |
|    ! 0 | 3481 | `		ph7_result_value(pCtx,pArray);` |
|    ! 0 | 3482 | `		return PH7_OK;` |
|      - | 3483 | `	}` |
|      - | 3484 | `	/* Point to the desired entry */` |
|      3 | 3485 | `	pCur = pSrc->pFirst;` |
|      2 | 3486 | `	for(;;){` |
|      5 | 3487 | `		if( iOfft < 1 ){` |
|      3 | 3488 | `			break;` |
|      - | 3489 | `		}` |
|      - | 3490 | `		/* Point to the next entry */` |
|      3 | 3491 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      3 | 3492 | `		iOfft--;` |
|      1 | 3493 | `	}` |
|      3 | 3494 | `	pRep = 0;` |
|      3 | 3495 | `	if( nArg > 3 ){` |
|      3 | 3496 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|      - | 3497 | `			/* Perform an array cast */` |
|    ! 0 | 3498 | `			PH7_MemObjToHashmap(apArg[3]);` |
|    ! 0 | 3499 | `			if(ph7_value_is_array(apArg[3])){` |
|    ! 0 | 3500 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|    ! 0 | 3501 | `			}` |
|    ! 0 | 3502 | `		}else{` |
|      3 | 3503 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|      - | 3504 | `		}` |
|      3 | 3505 | `		if( pRep ){` |
|      - | 3506 | `			/* Reset the loop cursor */` |
|      3 | 3507 | `			pRep->pCur = pRep->pFirst;` |
|      1 | 3508 | `		}` |
|      1 | 3509 | `	}` |
|      - | 3510 | `	/* Point to the internal representation of the hashmap */` |
|      3 | 3511 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      3 | 3512 | `	for(;;){` |
|      7 | 3513 | `		if( iLength < 1 ){` |
|      3 | 3514 | `			break;` |
|      - | 3515 | `		}` |
|      5 | 3516 | `		pPrev = pCur->pPrev;` |
|      5 | 3517 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      5 | 3518 | `		if( pRep && (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      - | 3519 | `			/* Extract node value */` |
|      5 | 3520 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      - | 3521 | `			/* Replace the old node */` |
|      5 | 3522 | `			pOld = HashmapExtractNodeValue(pCur);` |
|      5 | 3523 | `			if( pRvalue && pOld ){` |
|      5 | 3524 | `				PH7_MemObjStore(pRvalue,pOld);` |
|      2 | 3525 | `			}` |
|      3 | 3526 | `		}else{` |
|      - | 3527 | `			/* Unlink the node from the source hashmap */` |
|    ! 0 | 3528 | `			PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      - | 3529 | `		}` |
|      5 | 3530 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3531 | `			break;` |
|      - | 3532 | `		}` |
|      - | 3533 | `		/* Point to the next entry */` |
|      5 | 3534 | `		pCur = pPrev; /* Reverse link */` |
|      5 | 3535 | `		iLength--;` |
|      1 | 3536 | `	}` |
|      3 | 3537 | `	if( pRep ){` |
|      3 | 3538 | `		while((pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|    ! 0 | 3539 | `			HashmapInsertNode(pSrc,pRnode,FALSE);` |
|    ! 0 | 3540 | `		}` |
|      1 | 3541 | `	}` |
|      - | 3542 | `	/* Return the freshly created array */` |
|      3 | 3543 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3544 | `	return PH7_OK;` |
|      2 | 3545 |  |
|      - | 3546 | `/*` |
|      - | 3547 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|      - | 3548 | ` *  Checks if a value exists in an array.` |
|      - | 3549 | ` * Parameters` |
|      - | 3550 | ` *  $needle` |
|      - | 3551 | ` *   The searched value.` |
|      - | 3552 | ` *   Note:` |
|      - | 3553 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|      - | 3554 | ` * $haystack` |
|      - | 3555 | ` *  The target array.` |
|      - | 3556 | ` * $strict` |
|      - | 3557 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|      - | 3558 | ` *  will also check the types of the needle in the haystack.` |
|      - | 3559 | ` */` |
|  17334 | 3560 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3561 |  |
|      - | 3562 | `	ph7_value *pNeedle;` |
|      - | 3563 | `	int bStrict;` |
|      - | 3564 | `	int rc;` |
|  17336 | 3565 | `	if( nArg < 2 ){` |
|      - | 3566 | `		/* Missing argument,return FALSE */` |
|    ! 0 | 3567 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3568 | `		return PH7_OK;` |
|      - | 3569 | `	}` |
|  17336 | 3570 | `	pNeedle = apArg[0];` |
|  17336 | 3571 | `	bStrict = 0;` |
|  17336 | 3572 | `	if( nArg > 2 ){` |
|      5 | 3573 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      2 | 3574 | `	}` |
|  17336 | 3575 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 3576 | `		/* haystack must be an array,perform a standard comparison */` |
|    ! 0 | 3577 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|      - | 3578 | `		/* Set the comparison result */` |
|    ! 0 | 3579 | `		ph7_result_bool(pCtx,rc == 0);` |
|    ! 0 | 3580 | `		return PH7_OK;` |
|      - | 3581 | `	}` |
|      - | 3582 | `	/* Perform the lookup */` |
|  17336 | 3583 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|      - | 3584 | `	/* Lookup result */` |
|  17336 | 3585 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|  17336 | 3586 | `	return PH7_OK;` |
|   8669 | 3587 |  |
|      - | 3588 | `/*` |
|      - | 3589 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|      - | 3590 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|      - | 3591 | ` * Parameters` |
|      - | 3592 | ` * $needle` |
|      - | 3593 | ` *   The searched value.` |
|      - | 3594 | ` * $haystack` |
|      - | 3595 | ` *   The array.` |
|      - | 3596 | ` * $strict` |
|      - | 3597 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|      - | 3598 | ` *  will search for identical elements in the haystack. This means it will also check` |
|      - | 3599 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|      - | 3600 | ` * Return` |
|      - | 3601 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|      - | 3602 | ` */` |
|     26 | 3603 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3604 |  |
|      - | 3605 | `	ph7_hashmap_node *pEntry;` |
|      - | 3606 | `	ph7_value *pVal,sNeedle;` |
|      - | 3607 | `	ph7_hashmap *pMap;` |
|      - | 3608 | `	ph7_value sVal;` |
|      - | 3609 | `	int bStrict;` |
|      - | 3610 | `	sxu32 n;` |
|      - | 3611 | `	int rc;` |
|     27 | 3612 | `	if( nArg < 2 ){` |
|      - | 3613 | `		/* Missing argument,return FALSE*/` |
|    ! 0 | 3614 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3615 | `		return PH7_OK;` |
|      - | 3616 | `	}` |
|     27 | 3617 | `	bStrict = FALSE;` |
|     27 | 3618 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 3619 | `		/* hasystack must be an array,return FALSE */` |
|      3 | 3620 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3621 | `		return PH7_OK;` |
|      - | 3622 | `	}` |
|     25 | 3623 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|     19 | 3624 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      9 | 3625 | `	}` |
|      - | 3626 | `	/* Point to the internal representation of the internal hashmap */` |
|     25 | 3627 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3628 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     25 | 3629 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     25 | 3630 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     25 | 3631 | `	pEntry = pMap->pFirst;` |
|     25 | 3632 | `	n = pMap->nEntry;` |
|     39 | 3633 | `	for(;;){` |
|     79 | 3634 | `		if( !n ){` |
|      7 | 3635 | `			break;` |
|      - | 3636 | `		}` |
|      - | 3637 | `		/* Extract node value */` |
|     73 | 3638 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     73 | 3639 | `		if( pVal ){` |
|      - | 3640 | `			/* Make a copy of the vuurent values since the comparison routine` |
|      - | 3641 | `			 * can change their type.` |
|      - | 3642 | `			 */` |
|     73 | 3643 | `			PH7_MemObjLoad(pVal,&sVal);` |
|     73 | 3644 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|     73 | 3645 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|     73 | 3646 | `			PH7_MemObjRelease(&sVal);` |
|     73 | 3647 | `			PH7_MemObjRelease(&sNeedle);` |
|     73 | 3648 | `			if( rc == 0 ){` |
|      - | 3649 | `				/* Match found,return key */` |
|     19 | 3650 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|      - | 3651 | `					/* INT key */` |
|     13 | 3652 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|      7 | 3653 | `				}else{` |
|      7 | 3654 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|      - | 3655 | `					/* Blob key */` |
|      7 | 3656 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|      - | 3657 | `				}` |
|     19 | 3658 | `				return PH7_OK;` |
|      - | 3659 | `			}` |
|     27 | 3660 | `		}` |
|      - | 3661 | `		/* Point to the next entry */` |
|     55 | 3662 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     55 | 3663 | `		n--;` |
|      1 | 3664 | `	}` |
|      - | 3665 | `	/* No such value,return FALSE */` |
|      7 | 3666 | `	ph7_result_bool(pCtx,0);` |
|      7 | 3667 | `	return PH7_OK;` |
|     14 | 3668 |  |
|      - | 3669 | `/*` |
|      - | 3670 | ` * array array_diff(array $array1,array $array2,...)` |
|      - | 3671 | ` *  Computes the difference of arrays.` |
|      - | 3672 | ` * Parameters` |
|      - | 3673 | ` *  $array1` |
|      - | 3674 | ` *    The array to compare from` |
|      - | 3675 | ` *  $array2` |
|      - | 3676 | ` *    An array to compare against` |
|      - | 3677 | ` *  $...` |
|      - | 3678 | ` *   More arrays to compare against` |
|      - | 3679 | ` * Return` |
|      - | 3680 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3681 | ` *  are not present in any of the other arrays.` |
|      - | 3682 | ` */` |
|      2 | 3683 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3684 |  |
|      - | 3685 | `	ph7_hashmap_node *pEntry;` |
|      - | 3686 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3687 | `	ph7_value *pArray;` |
|      - | 3688 | `	ph7_value *pVal;` |
|      - | 3689 | `	sxi32 rc;` |
|      - | 3690 | `	sxu32 n;` |
|      - | 3691 | `	int i;` |
|      3 | 3692 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3693 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3694 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3695 | `		return PH7_OK;` |
|      - | 3696 | `	}` |
|      3 | 3697 | `	if( nArg == 1 ){` |
|      - | 3698 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3699 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3700 | `		return PH7_OK;` |
|      - | 3701 | `	}` |
|      - | 3702 | `	/* Create a new array */` |
|      3 | 3703 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3704 | `	if( pArray == 0 ){` |
|    ! 0 | 3705 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3706 | `		return PH7_OK;` |
|      - | 3707 | `	}` |
|      - | 3708 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3709 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3710 | `	/* Perform the diff */` |
|      3 | 3711 | `	pEntry = pSrc->pFirst;` |
|      3 | 3712 | `	n = pSrc->nEntry;` |
|      4 | 3713 | `	for(;;){` |
|      9 | 3714 | `		if( n < 1 ){` |
|      3 | 3715 | `			break;` |
|      - | 3716 | `		}` |
|      - | 3717 | `		/* Extract the node value */` |
|      7 | 3718 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 3719 | `		if( pVal ){` |
|     11 | 3720 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      7 | 3721 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 3722 | `					/* ignore */` |
|    ! 0 | 3723 | `					continue;` |
|      - | 3724 | `				}` |
|      - | 3725 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 3726 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3727 | `				/* Perform the lookup */` |
|      7 | 3728 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      7 | 3729 | `				if( rc == SXRET_OK ){` |
|      - | 3730 | `					/* Value exist */` |
|      3 | 3731 | `					break;` |
|      - | 3732 | `				}` |
|      3 | 3733 | `			}` |
|      7 | 3734 | `			if( i >= nArg ){` |
|      - | 3735 | `				/* Perform the insertion */` |
|      5 | 3736 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 3737 | `			}` |
|      3 | 3738 | `		}` |
|      - | 3739 | `		/* Point to the next entry */` |
|      7 | 3740 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 3741 | `		n--;` |
|      1 | 3742 | `	}` |
|      - | 3743 | `	/* Return the freshly created array */` |
|      3 | 3744 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3745 | `	return PH7_OK;` |
|      2 | 3746 |  |
|      - | 3747 | `/*` |
|      - | 3748 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|      - | 3749 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|      - | 3750 | ` * Parameters` |
|      - | 3751 | ` *  $array1` |
|      - | 3752 | ` *    The array to compare from` |
|      - | 3753 | ` *  $array2` |
|      - | 3754 | ` *    An array to compare against` |
|      - | 3755 | ` *  $...` |
|      - | 3756 | ` *   More arrays to compare against.` |
|      - | 3757 | ` * $callback` |
|      - | 3758 | ` *  The callback comparison function.` |
|      - | 3759 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|      - | 3760 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|      - | 3761 | ` *  than the second.` |
|      - | 3762 | ` *     int callback ( mixed $a, mixed $b )` |
|      - | 3763 | ` * Return` |
|      - | 3764 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3765 | ` *  are not present in any of the other arrays.` |
|      - | 3766 | ` */` |
|      2 | 3767 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3768 |  |
|      - | 3769 | `	ph7_hashmap_node *pEntry;` |
|      - | 3770 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3771 | `	ph7_value *pCallback;` |
|      - | 3772 | `	ph7_value *pArray;` |
|      - | 3773 | `	ph7_value *pVal;` |
|      - | 3774 | `	sxi32 rc;` |
|      - | 3775 | `	sxu32 n;` |
|      - | 3776 | `	int i;` |
|      3 | 3777 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3778 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3779 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3780 | `		return PH7_OK;` |
|      - | 3781 | `	}` |
|      - | 3782 | `	/* Point to the callback */` |
|      3 | 3783 | `	pCallback = apArg[nArg - 1];` |
|      3 | 3784 | `	if( nArg == 2 ){` |
|      - | 3785 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3786 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3787 | `		return PH7_OK;` |
|      - | 3788 | `	}` |
|      - | 3789 | `	/* Create a new array */` |
|      3 | 3790 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3791 | `	if( pArray == 0 ){` |
|    ! 0 | 3792 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3793 | `		return PH7_OK;` |
|      - | 3794 | `	}` |
|      - | 3795 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3796 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3797 | `	/* Perform the diff */` |
|      3 | 3798 | `	pEntry = pSrc->pFirst;` |
|      3 | 3799 | `	n = pSrc->nEntry;` |
|      4 | 3800 | `	for(;;){` |
|      9 | 3801 | `		if( n < 1 ){` |
|      3 | 3802 | `			break;` |
|      - | 3803 | `		}` |
|      - | 3804 | `		/* Extract the node value */` |
|      7 | 3805 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 3806 | `		if( pVal ){` |
|     11 | 3807 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      7 | 3808 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 3809 | `					/* ignore */` |
|    ! 0 | 3810 | `					continue;` |
|      - | 3811 | `				}` |
|      - | 3812 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 3813 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3814 | `				/* Perform the lookup */` |
|      7 | 3815 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      7 | 3816 | `				if( rc == SXRET_OK ){` |
|      - | 3817 | `					/* Value exist */` |
|      3 | 3818 | `					break;` |
|      - | 3819 | `				}` |
|      3 | 3820 | `			}` |
|      7 | 3821 | `			if( i >= (nArg - 1)){` |
|      - | 3822 | `				/* Perform the insertion */` |
|      5 | 3823 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 3824 | `			}` |
|      3 | 3825 | `		}` |
|      - | 3826 | `		/* Point to the next entry */` |
|      7 | 3827 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 3828 | `		n--;` |
|      1 | 3829 | `	}` |
|      - | 3830 | `	/* Return the freshly created array */` |
|      3 | 3831 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3832 | `	return PH7_OK;` |
|      2 | 3833 |  |
|      - | 3834 | `/*` |
|      - | 3835 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|      - | 3836 | ` *  Computes the difference of arrays with additional index check.` |
|      - | 3837 | ` * Parameters` |
|      - | 3838 | ` *  $array1` |
|      - | 3839 | ` *    The array to compare from` |
|      - | 3840 | ` *  $array2` |
|      - | 3841 | ` *    An array to compare against` |
|      - | 3842 | ` *  $...` |
|      - | 3843 | ` *   More arrays to compare against` |
|      - | 3844 | ` * Return` |
|      - | 3845 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3846 | ` *  are not present in any of the other arrays.` |
|      - | 3847 | ` */` |
|      2 | 3848 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3849 |  |
|      - | 3850 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|      - | 3851 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3852 | `	ph7_value *pArray;` |
|      - | 3853 | `	ph7_value *pVal;` |
|      - | 3854 | `	sxi32 rc;` |
|      - | 3855 | `	sxu32 n;` |
|      - | 3856 | `	int i;` |
|      3 | 3857 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3858 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3859 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3860 | `		return PH7_OK;` |
|      - | 3861 | `	}` |
|      3 | 3862 | `	if( nArg == 1 ){` |
|      - | 3863 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3864 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3865 | `		return PH7_OK;` |
|      - | 3866 | `	}` |
|      - | 3867 | `	/* Create a new array */` |
|      3 | 3868 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3869 | `	if( pArray == 0 ){` |
|    ! 0 | 3870 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3871 | `		return PH7_OK;` |
|      - | 3872 | `	}` |
|      - | 3873 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3874 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3875 | `	/* Perform the diff */` |
|      3 | 3876 | `	pEntry = pSrc->pFirst;` |
|      3 | 3877 | `	n = pSrc->nEntry;` |
|      3 | 3878 | `	pN1 = pN2 = 0;` |
|      3 | 3879 | `	for(;;){` |
|      7 | 3880 | `		if( n < 1 ){` |
|      3 | 3881 | `			break;` |
|      - | 3882 | `		}` |
|      7 | 3883 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      5 | 3884 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 3885 | `				/* ignore */` |
|    ! 0 | 3886 | `				continue;` |
|      - | 3887 | `			}` |
|      - | 3888 | `			/* Point to the internal representation of the hashmap */` |
|      5 | 3889 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3890 | `			/* Perform a key lookup first */` |
|      5 | 3891 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|    ! 0 | 3892 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|    ! 0 | 3893 | `			}else{` |
|      5 | 3894 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|      - | 3895 | `			}` |
|      5 | 3896 | `			if( rc != SXRET_OK ){` |
|      - | 3897 | `				/* No such key,break immediately */` |
|      3 | 3898 | `				break;` |
|      - | 3899 | `			}` |
|      - | 3900 | `			/* Extract node value */` |
|      3 | 3901 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      3 | 3902 | `			if( pVal ){` |
|      - | 3903 | `				/* Perform the lookup */` |
|      3 | 3904 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      3 | 3905 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|      - | 3906 | `					/* Value does not exist */` |
|    ! 0 | 3907 | `					break;` |
|      - | 3908 | `				}` |
|      1 | 3909 | `			}` |
|      2 | 3910 | `		}` |
|      5 | 3911 | `		if( i < nArg ){` |
|      - | 3912 | `			/* Perform the insertion */` |
|      3 | 3913 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      1 | 3914 | `		}` |
|      - | 3915 | `		/* Point to the next entry */` |
|      5 | 3916 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      5 | 3917 | `		n--;` |
|      1 | 3918 | `	}` |
|      - | 3919 | `	/* Return the freshly created array */` |
|      3 | 3920 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3921 | `	return PH7_OK;` |
|      2 | 3922 |  |
|      - | 3923 | `/*` |
|      - | 3924 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|      - | 3925 | ` *  Computes the difference of arrays with additional index check which is performed` |
|      - | 3926 | ` *  by a user supplied callback function.` |
|      - | 3927 | ` * Parameters` |
|      - | 3928 | ` *  $array1` |
|      - | 3929 | ` *    The array to compare from` |
|      - | 3930 | ` *  $array2` |
|      - | 3931 | ` *    An array to compare against` |
|      - | 3932 | ` *  $...` |
|      - | 3933 | ` *   More arrays to compare against.` |
|      - | 3934 | ` *  $key_compare_func` |
|      - | 3935 | ` *   Callback function to use. The callback function must return an integer` |
|      - | 3936 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|      - | 3937 | ` *   to be respectively less than, equal to, or greater than the second.` |
|      - | 3938 | ` * Return` |
|      - | 3939 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3940 | ` *  are not present in any of the other arrays.` |
|      - | 3941 | ` */` |
|      2 | 3942 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3943 |  |
|      - | 3944 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|      - | 3945 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3946 | `	ph7_value *pCallback;` |
|      - | 3947 | `	ph7_value *pArray;` |
|      - | 3948 | `	ph7_value *pVal;` |
|      - | 3949 | `	sxi32 rc;` |
|      - | 3950 | `	sxu32 n;` |
|      - | 3951 | `	int i;` |
|      - | 3952 |  |
|      3 | 3953 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3954 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3955 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3956 | `		return PH7_OK;` |
|      - | 3957 | `	}` |
|      - | 3958 | `	/* Point to the callback */` |
|      3 | 3959 | `	pCallback = apArg[nArg - 1];` |
|      3 | 3960 | `	if( nArg == 2 ){` |
|      - | 3961 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3962 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3963 | `		return PH7_OK;` |
|      - | 3964 | `	}` |
|      - | 3965 | `	/* Create a new array */` |
|      3 | 3966 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3967 | `	if( pArray == 0 ){` |
|    ! 0 | 3968 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3969 | `		return PH7_OK;` |
|      - | 3970 | `	}` |
|      - | 3971 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3972 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3973 | `	/* Perform the diff */` |
|      3 | 3974 | `	pEntry = pSrc->pFirst;` |
|      3 | 3975 | `	n = pSrc->nEntry;` |
|      3 | 3976 | `	pN1 = pN2 = 0; /* cc warning */` |
|      4 | 3977 | `	for(;;){` |
|      9 | 3978 | `		if( n < 1 ){` |
|      3 | 3979 | `			break;` |
|      - | 3980 | `		}` |
|      9 | 3981 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      7 | 3982 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 3983 | `				/* ignore */` |
|    ! 0 | 3984 | `				continue;` |
|      - | 3985 | `			}` |
|      - | 3986 | `			/* Point to the internal representation of the hashmap */` |
|      7 | 3987 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3988 | `			/* Perform a key lookup first */` |
|      7 | 3989 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|    ! 0 | 3990 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|    ! 0 | 3991 | `			}else{` |
|      7 | 3992 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|      - | 3993 | `			}` |
|      7 | 3994 | `			if( rc != SXRET_OK ){` |
|      - | 3995 | `				/* No such key,break immediately */` |
|      3 | 3996 | `				break;` |
|      - | 3997 | `			}` |
|      - | 3998 | `			/* Extract node value */` |
|      5 | 3999 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      5 | 4000 | `			if( pVal ){` |
|      - | 4001 | `				/* Invoke the user callback */` |
|      5 | 4002 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,&pN2);` |
|      5 | 4003 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|      - | 4004 | `					/* Value does not exist */` |
|      2 | 4005 | `					break;` |
|      - | 4006 | `				}` |
|      1 | 4007 | `			}` |
|      2 | 4008 | `		}` |
|      7 | 4009 | `		if( i < (nArg-1) ){` |
|      - | 4010 | `			/* Perform the insertion */` |
|      5 | 4011 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 4012 | `		}` |
|      - | 4013 | `		/* Point to the next entry */` |
|      7 | 4014 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4015 | `		n--;` |
|      1 | 4016 | `	}` |
|      - | 4017 | `	/* Return the freshly created array */` |
|      3 | 4018 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4019 | `	return PH7_OK;` |
|      2 | 4020 |  |
|      - | 4021 | `/*` |
|      - | 4022 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|      - | 4023 | ` *  Computes the difference of arrays using keys for comparison.` |
|      - | 4024 | ` * Parameters` |
|      - | 4025 | ` *  $array1` |
|      - | 4026 | ` *    The array to compare from` |
|      - | 4027 | ` *  $array2` |
|      - | 4028 | ` *    An array to compare against` |
|      - | 4029 | ` *  $...` |
|      - | 4030 | ` *   More arrays to compare against` |
|      - | 4031 | ` * Return` |
|      - | 4032 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|      - | 4033 | ` *  in any of the other arrays.` |
|      - | 4034 | ` * Note that NULL is returned on failure.` |
|      - | 4035 | ` */` |
|      2 | 4036 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4037 |  |
|      - | 4038 | `	ph7_hashmap_node *pEntry;` |
|      - | 4039 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4040 | `	ph7_value *pArray;` |
|      - | 4041 | `	sxi32 rc;` |
|      - | 4042 | `	sxu32 n;` |
|      - | 4043 | `	int i;` |
|      3 | 4044 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4045 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4046 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4047 | `		return PH7_OK;` |
|      - | 4048 | `	}` |
|      3 | 4049 | `	if( nArg == 1 ){` |
|      - | 4050 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4051 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4052 | `		return PH7_OK;` |
|      - | 4053 | `	}` |
|      - | 4054 | `	/* Create a new array */` |
|      3 | 4055 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4056 | `	if( pArray == 0 ){` |
|    ! 0 | 4057 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4058 | `		return PH7_OK;` |
|      - | 4059 | `	}` |
|      - | 4060 | `	/* Point to the internal representation of the main hashmap */` |
|      3 | 4061 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4062 | `	/* Perfrom the diff */` |
|      3 | 4063 | `	pEntry = pSrc->pFirst;` |
|      3 | 4064 | `	n = pSrc->nEntry;` |
|      4 | 4065 | `	for(;;){` |
|      9 | 4066 | `		if( n < 1 ){` |
|      3 | 4067 | `			break;` |
|      - | 4068 | `		}` |
|      9 | 4069 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      7 | 4070 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4071 | `				/* ignore */` |
|    ! 0 | 4072 | `				continue;` |
|      - | 4073 | `			}` |
|      7 | 4074 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      7 | 4075 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      7 | 4076 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|      - | 4077 | `				/* Blob lookup */` |
|      7 | 4078 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      4 | 4079 | `			}else{` |
|      - | 4080 | `				/* Int lookup */` |
|    ! 0 | 4081 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|      - | 4082 | `			}` |
|      7 | 4083 | `			if( rc == SXRET_OK ){` |
|      - | 4084 | `				/* Key exists,break immediately */` |
|      5 | 4085 | `				break;` |
|      - | 4086 | `			}` |
|      2 | 4087 | `		}` |
|      7 | 4088 | `		if( i >= nArg ){` |
|      - | 4089 | `			/* Perform the insertion */` |
|      3 | 4090 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      1 | 4091 | `		}` |
|      - | 4092 | `		/* Point to the next entry */` |
|      7 | 4093 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4094 | `		n--;` |
|      1 | 4095 | `	}` |
|      - | 4096 | `	/* Return the freshly created array */` |
|      3 | 4097 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4098 | `	return PH7_OK;` |
|      2 | 4099 |  |
|      - | 4100 | `/*` |
|      - | 4101 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|      - | 4102 | ` *  Computes the intersection of arrays.` |
|      - | 4103 | ` * Parameters` |
|      - | 4104 | ` *  $array1` |
|      - | 4105 | ` *    The array to compare from` |
|      - | 4106 | ` *  $array2` |
|      - | 4107 | ` *    An array to compare against` |
|      - | 4108 | ` *  $...` |
|      - | 4109 | ` *   More arrays to compare against` |
|      - | 4110 | ` * Return` |
|      - | 4111 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|      - | 4112 | ` *  in all of the parameters. .` |
|      - | 4113 | ` * Note that NULL is returned on failure.` |
|      - | 4114 | ` */` |
|      2 | 4115 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4116 |  |
|      - | 4117 | `	ph7_hashmap_node *pEntry;` |
|      - | 4118 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4119 | `	ph7_value *pArray;` |
|      - | 4120 | `	ph7_value *pVal;` |
|      - | 4121 | `	sxi32 rc;` |
|      - | 4122 | `	sxu32 n;` |
|      - | 4123 | `	int i;` |
|      3 | 4124 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4125 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4126 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4127 | `		return PH7_OK;` |
|      - | 4128 | `	}` |
|      3 | 4129 | `	if( nArg == 1 ){` |
|      - | 4130 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4131 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4132 | `		return PH7_OK;` |
|      - | 4133 | `	}` |
|      - | 4134 | `	/* Create a new array */` |
|      3 | 4135 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4136 | `	if( pArray == 0 ){` |
|    ! 0 | 4137 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4138 | `		return PH7_OK;` |
|      - | 4139 | `	}` |
|      - | 4140 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 4141 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4142 | `	/* Perform the intersection */` |
|      3 | 4143 | `	pEntry = pSrc->pFirst;` |
|      3 | 4144 | `	n = pSrc->nEntry;` |
|      5 | 4145 | `	for(;;){` |
|     11 | 4146 | `		if( n < 1 ){` |
|      3 | 4147 | `			break;` |
|      - | 4148 | `		}` |
|      - | 4149 | `		/* Extract the node value */` |
|      9 | 4150 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      9 | 4151 | `		if( pVal ){` |
|     13 | 4152 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      9 | 4153 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4154 | `					/* ignore */` |
|    ! 0 | 4155 | `					continue;` |
|      - | 4156 | `				}` |
|      - | 4157 | `				/* Point to the internal representation of the hashmap */` |
|      9 | 4158 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 4159 | `				/* Perform the lookup */` |
|      9 | 4160 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      9 | 4161 | `				if( rc != SXRET_OK ){` |
|      - | 4162 | `					/* Value does not exist */` |
|      5 | 4163 | `					break;` |
|      - | 4164 | `				}` |
|      3 | 4165 | `			}` |
|      9 | 4166 | `			if( i >= nArg ){` |
|      - | 4167 | `				/* Perform the insertion */` |
|      5 | 4168 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 4169 | `			}` |
|      4 | 4170 | `		}` |
|      - | 4171 | `		/* Point to the next entry */` |
|      9 | 4172 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      9 | 4173 | `		n--;` |
|      1 | 4174 | `	}` |
|      - | 4175 | `	/* Return the freshly created array */` |
|      3 | 4176 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4177 | `	return PH7_OK;` |
|      2 | 4178 |  |
|      - | 4179 | `/*` |
|      - | 4180 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|      - | 4181 | ` *  Computes the intersection of arrays.` |
|      - | 4182 | ` * Parameters` |
|      - | 4183 | ` *  $array1` |
|      - | 4184 | ` *    The array to compare from` |
|      - | 4185 | ` *  $array2` |
|      - | 4186 | ` *    An array to compare against` |
|      - | 4187 | ` *  $...` |
|      - | 4188 | ` *   More arrays to compare against` |
|      - | 4189 | ` * Return` |
|      - | 4190 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|      - | 4191 | ` *  in all of the parameters. .` |
|      - | 4192 | ` * Note that NULL is returned on failure.` |
|      - | 4193 | ` */` |
|      2 | 4194 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4195 |  |
|      - | 4196 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|      - | 4197 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4198 | `	ph7_value *pArray;` |
|      - | 4199 | `	ph7_value *pVal;` |
|      - | 4200 | `	sxi32 rc;` |
|      - | 4201 | `	sxu32 n;` |
|      - | 4202 | `	int i;` |
|      3 | 4203 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4204 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4205 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4206 | `		return PH7_OK;` |
|      - | 4207 | `	}` |
|      3 | 4208 | `	if( nArg == 1 ){` |
|      - | 4209 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4210 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4211 | `		return PH7_OK;` |
|      - | 4212 | `	}` |
|      - | 4213 | `	/* Create a new array */` |
|      3 | 4214 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4215 | `	if( pArray == 0 ){` |
|    ! 0 | 4216 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4217 | `		return PH7_OK;` |
|      - | 4218 | `	}` |
|      - | 4219 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 4220 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4221 | `	/* Perform the intersection */` |
|      3 | 4222 | `	pEntry = pSrc->pFirst;` |
|      3 | 4223 | `	n = pSrc->nEntry;` |
|      3 | 4224 | `	pN1 = pN2 = 0; /* cc warning */` |
|      4 | 4225 | `	for(;;){` |
|      9 | 4226 | `		if( n < 1 ){` |
|      3 | 4227 | `			break;` |
|      - | 4228 | `		}` |
|      - | 4229 | `		/* Extract the node value */` |
|      7 | 4230 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 4231 | `		if( pVal ){` |
|      9 | 4232 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      7 | 4233 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4234 | `					/* ignore */` |
|    ! 0 | 4235 | `					continue;` |
|      - | 4236 | `				}` |
|      - | 4237 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 4238 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 4239 | `				/* Perform a key lookup first */` |
|      7 | 4240 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|    ! 0 | 4241 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|    ! 0 | 4242 | `				}else{` |
|      7 | 4243 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|      - | 4244 | `				}` |
|      7 | 4245 | `				if( rc != SXRET_OK ){` |
|      - | 4246 | `					/* No such key,break immediately */` |
|      3 | 4247 | `					break;` |
|      - | 4248 | `				}` |
|      - | 4249 | `				/* Perform the lookup */` |
|      5 | 4250 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      5 | 4251 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|      - | 4252 | `					/* Value does not exist */` |
|      2 | 4253 | `					break;` |
|      - | 4254 | `				}` |
|      2 | 4255 | `			}` |
|      7 | 4256 | `			if( i >= nArg ){` |
|      - | 4257 | `				/* Perform the insertion */` |
|      3 | 4258 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      1 | 4259 | `			}` |
|      3 | 4260 | `		}` |
|      - | 4261 | `		/* Point to the next entry */` |
|      7 | 4262 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4263 | `		n--;` |
|      1 | 4264 | `	}` |
|      - | 4265 | `	/* Return the freshly created array */` |
|      3 | 4266 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4267 | `	return PH7_OK;` |
|      2 | 4268 |  |
|      - | 4269 | `/*` |
|      - | 4270 | ` * array array_intersect_key(array $array1 ,array $array2,...)` |
|      - | 4271 | ` *  Computes the intersection of arrays using keys for comparison.` |
|      - | 4272 | ` * Parameters` |
|      - | 4273 | ` *  $array1` |
|      - | 4274 | ` *    The array to compare from` |
|      - | 4275 | ` *  $array2` |
|      - | 4276 | ` *    An array to compare against` |
|      - | 4277 | ` *  $...` |
|      - | 4278 | ` *   More arrays to compare against` |
|      - | 4279 | ` * Return` |
|      - | 4280 | ` *  Returns an associative array containing all the entries of array1 which` |
|      - | 4281 | ` *  have keys that are present in all arguments.` |
|      - | 4282 | ` * Note that NULL is returned on failure.` |
|      - | 4283 | ` */` |
|      4 | 4284 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4285 |  |
|      - | 4286 | `	ph7_hashmap_node *pEntry;` |
|      - | 4287 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4288 | `	ph7_value *pArray;` |
|      - | 4289 | `	sxi32 rc;` |
|      - | 4290 | `	sxu32 n;` |
|      - | 4291 | `	int i;` |
|      5 | 4292 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4293 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4294 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4295 | `		return PH7_OK;` |
|      - | 4296 | `	}` |
|      5 | 4297 | `	if( nArg == 1 ){` |
|      - | 4298 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4299 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4300 | `		return PH7_OK;` |
|      - | 4301 | `	}` |
|      - | 4302 | `	/* Create a new array */` |
|      5 | 4303 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 4304 | `	if( pArray == 0 ){` |
|    ! 0 | 4305 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4306 | `		return PH7_OK;` |
|      - | 4307 | `	}` |
|      - | 4308 | `	/* Point to the internal representation of the main hashmap */` |
|      5 | 4309 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4310 | `	/* Perfrom the intersection */` |
|      5 | 4311 | `	pEntry = pSrc->pFirst;` |
|      5 | 4312 | `	n = pSrc->nEntry;` |
|      8 | 4313 | `	for(;;){` |
|     17 | 4314 | `		if( n < 1 ){` |
|      5 | 4315 | `			break;` |
|      - | 4316 | `		}` |
|     19 | 4317 | `		for( i = 1 ; i < nArg ; i++ ){` |
|     13 | 4318 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4319 | `				/* ignore */` |
|    ! 0 | 4320 | `				continue;` |
|      - | 4321 | `			}` |
|     13 | 4322 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     13 | 4323 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      7 | 4324 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|      - | 4325 | `				/* Blob lookup */` |
|      7 | 4326 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      4 | 4327 | `			}else{` |
|      - | 4328 | `				/* Int key */` |
|      7 | 4329 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|      - | 4330 | `			}` |
|     13 | 4331 | `			if( rc != SXRET_OK ){` |
|      - | 4332 | `				/* Key does not exists,break immediately */` |
|      7 | 4333 | `				break;` |
|      - | 4334 | `			}` |
|      4 | 4335 | `		}` |
|     13 | 4336 | `		if( i >= nArg ){` |
|      - | 4337 | `			/* Perform the insertion */` |
|      7 | 4338 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      3 | 4339 | `		}` |
|      - | 4340 | `		/* Point to the next entry */` |
|     13 | 4341 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     13 | 4342 | `		n--;` |
|      1 | 4343 | `	}` |
|      - | 4344 | `	/* Return the freshly created array */` |
|      5 | 4345 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 4346 | `	return PH7_OK;` |
|      3 | 4347 |  |
|      - | 4348 | `/*` |
|      - | 4349 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|      - | 4350 | ` *  Computes the intersection of arrays.` |
|      - | 4351 | ` * Parameters` |
|      - | 4352 | ` *  $array1` |
|      - | 4353 | ` *    The array to compare from` |
|      - | 4354 | ` *  $array2` |
|      - | 4355 | ` *    An array to compare against` |
|      - | 4356 | ` *  $...` |
|      - | 4357 | ` *   More arrays to compare against` |
|      - | 4358 | ` * $callback` |
|      - | 4359 | ` *  The callback comparison function.` |
|      - | 4360 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|      - | 4361 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|      - | 4362 | ` *  than the second.` |
|      - | 4363 | ` *     int callback ( mixed $a, mixed $b )` |
|      - | 4364 | ` * Return` |
|      - | 4365 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|      - | 4366 | ` *  in all of the parameters. .` |
|      - | 4367 | ` * Note that NULL is returned on failure.` |
|      - | 4368 | ` */` |
|      2 | 4369 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4370 |  |
|      - | 4371 | `	ph7_hashmap_node *pEntry;` |
|      - | 4372 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4373 | `	ph7_value *pCallback;` |
|      - | 4374 | `	ph7_value *pArray;` |
|      - | 4375 | `	ph7_value *pVal;` |
|      - | 4376 | `	sxi32 rc;` |
|      - | 4377 | `	sxu32 n;` |
|      - | 4378 | `	int i;` |
|      - | 4379 |  |
|      3 | 4380 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4381 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 4382 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4383 | `		return PH7_OK;` |
|      - | 4384 | `	}` |
|      - | 4385 | `	/* Point to the callback */` |
|      3 | 4386 | `	pCallback = apArg[nArg - 1];` |
|      3 | 4387 | `	if( nArg == 2 ){` |
|      - | 4388 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4389 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4390 | `		return PH7_OK;` |
|      - | 4391 | `	}` |
|      - | 4392 | `	/* Create a new array */` |
|      3 | 4393 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4394 | `	if( pArray == 0 ){` |
|    ! 0 | 4395 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4396 | `		return PH7_OK;` |
|      - | 4397 | `	}` |
|      - | 4398 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 4399 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4400 | `	/* Perform the intersection */` |
|      3 | 4401 | `	pEntry = pSrc->pFirst;` |
|      3 | 4402 | `	n = pSrc->nEntry;` |
|      4 | 4403 | `	for(;;){` |
|      9 | 4404 | `		if( n < 1 ){` |
|      3 | 4405 | `			break;` |
|      - | 4406 | `		}` |
|      - | 4407 | `		/* Extract the node value */` |
|      7 | 4408 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 4409 | `		if( pVal ){` |
|     11 | 4410 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      7 | 4411 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4412 | `					/* ignore */` |
|    ! 0 | 4413 | `					continue;` |
|      - | 4414 | `				}` |
|      - | 4415 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 4416 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 4417 | `				/* Perform the lookup */` |
|      7 | 4418 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      7 | 4419 | `				if( rc != SXRET_OK ){` |
|      - | 4420 | `					/* Value does not exist */` |
|      3 | 4421 | `					break;` |
|      - | 4422 | `				}` |
|      3 | 4423 | `			}` |
|      7 | 4424 | `			if( i >= (nArg-1) ){` |
|      - | 4425 | `				/* Perform the insertion */` |
|      5 | 4426 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 4427 | `			}` |
|      3 | 4428 | `		}` |
|      - | 4429 | `		/* Point to the next entry */` |
|      7 | 4430 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4431 | `		n--;` |
|      1 | 4432 | `	}` |
|      - | 4433 | `	/* Return the freshly created array */` |
|      3 | 4434 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4435 | `	return PH7_OK;` |
|      2 | 4436 |  |
|      - | 4437 | `/*` |
|      - | 4438 | ` * array array_fill(int $start_index,int $num,var $value)` |
|      - | 4439 | ` *  Fill an array with values.` |
|      - | 4440 | ` * Parameters` |
|      - | 4441 | ` *  $start_index` |
|      - | 4442 | ` *    The first index of the returned array.` |
|      - | 4443 | ` *  $num` |
|      - | 4444 | ` *   Number of elements to insert.` |
|      - | 4445 | ` *  $value` |
|      - | 4446 | ` *    Value to use for filling.` |
|      - | 4447 | ` * Return` |
|      - | 4448 | ` *  The filled array or null on failure.` |
|      - | 4449 | ` */` |
|    208 | 4450 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4451 |  |
|      - | 4452 | `	ph7_value *pArray;` |
|      - | 4453 | `	int i,nEntry;` |
|    209 | 4454 | `	if( nArg < 3 ){` |
|      - | 4455 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4456 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4457 | `		return PH7_OK;` |
|      - | 4458 | `	}` |
|      - | 4459 | `	/* Create a new array */` |
|    209 | 4460 | `	pArray = ph7_context_new_array(pCtx);` |
|    209 | 4461 | `	if( pArray == 0 ){` |
|    ! 0 | 4462 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4463 | `		return PH7_OK;` |
|      - | 4464 | `	}` |
|      - | 4465 | `	/* Total number of entries to insert */` |
|    209 | 4466 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      - | 4467 | `	/* Insert the first entry alone because it have it's own key */` |
|    209 | 4468 | `	ph7_array_add_intkey_elem(pArray,ph7_value_to_int(apArg[0]),apArg[2]);` |
|      - | 4469 | `	/* Repeat insertion of the desired value */` |
|  20409 | 4470 | `	for( i = 1 ; i < nEntry ; i++ ){` |
|  20201 | 4471 | `		ph7_array_add_elem(pArray,0/*Automatic index assign */,apArg[2]);` |
|  10101 | 4472 | `	}` |
|      - | 4473 | `	/* Return the filled array */` |
|    209 | 4474 | `	ph7_result_value(pCtx,pArray);` |
|    209 | 4475 | `	return PH7_OK;` |
|    105 | 4476 |  |
|      - | 4477 | `/*` |
|      - | 4478 | ` * array array_fill_keys(array $input,var $value)` |
|      - | 4479 | ` *  Fill an array with values, specifying keys.` |
|      - | 4480 | ` * Parameters` |
|      - | 4481 | ` *  $input` |
|      - | 4482 | ` *   Array of values that will be used as key.` |
|      - | 4483 | ` *  $value` |
|      - | 4484 | ` *    Value to use for filling.` |
|      - | 4485 | ` * Return` |
|      - | 4486 | ` *  The filled array or null on failure.` |
|      - | 4487 | ` */` |
|      2 | 4488 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4489 |  |
|      - | 4490 | `	ph7_hashmap_node *pEntry;` |
|      - | 4491 | `	ph7_hashmap *pSrc;` |
|      - | 4492 | `	ph7_value *pArray;` |
|      - | 4493 | `	sxu32 n;` |
|      3 | 4494 | `	if( nArg < 2 ){` |
|      - | 4495 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4496 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4497 | `		return PH7_OK;` |
|      - | 4498 | `	}` |
|      - | 4499 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 4500 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4501 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 4502 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4503 | `		return PH7_OK;` |
|      - | 4504 | `	}` |
|      - | 4505 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 4506 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4507 | `	/* Create a new array */` |
|      3 | 4508 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4509 | `	if( pArray == 0 ){` |
|    ! 0 | 4510 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4511 | `		return PH7_OK;` |
|      - | 4512 | `	}` |
|      - | 4513 | `	/* Perform the requested operation */` |
|      3 | 4514 | `	pEntry = pSrc->pFirst;` |
|      7 | 4515 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      5 | 4516 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|      - | 4517 | `		/* Point to the next entry */` |
|      5 | 4518 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      3 | 4519 | `	}` |
|      - | 4520 | `	/* Return the filled array */` |
|      3 | 4521 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4522 | `	return PH7_OK;` |
|      2 | 4523 |  |
|      - | 4524 | `/*` |
|      - | 4525 | ` * array array_combine(array $keys,array $values)` |
|      - | 4526 | ` *  Creates an array by using one array for keys and another for its values.` |
|      - | 4527 | ` * Parameters` |
|      - | 4528 | ` *  $keys` |
|      - | 4529 | ` *    Array of keys to be used.` |
|      - | 4530 | ` * $values` |
|      - | 4531 | ` *   Array of values to be used.` |
|      - | 4532 | ` * Return` |
|      - | 4533 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|      - | 4534 | ` *  for each array isn't equal or if one of the given arguments is` |
|      - | 4535 | ` *  not an array.` |
|      - | 4536 | ` */` |
|      2 | 4537 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4538 |  |
|      - | 4539 | `	ph7_hashmap_node *pKe,*pVe;` |
|      - | 4540 | `	ph7_hashmap *pKey,*pValue;` |
|      - | 4541 | `	ph7_value *pArray;` |
|      - | 4542 | `	sxu32 n;` |
|      3 | 4543 | `	if( nArg < 2 ){` |
|      - | 4544 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4545 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4546 | `		return PH7_OK;` |
|      - | 4547 | `	}` |
|      - | 4548 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 4549 | `	if( !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4550 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 4551 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4552 | `		return PH7_OK;` |
|      - | 4553 | `	}` |
|      - | 4554 | `	/* Point to the internal representation of the input hashmaps */` |
|      3 | 4555 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 4556 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      3 | 4557 | `	if( pKey->nEntry != pValue->nEntry ){` |
|      - | 4558 | `		/* Array length differs,return FALSE */` |
|    ! 0 | 4559 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4560 | `		return PH7_OK;` |
|      - | 4561 | `	}` |
|      - | 4562 | `	/* Create a new array */` |
|      3 | 4563 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4564 | `	if( pArray == 0 ){` |
|    ! 0 | 4565 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4566 | `		return PH7_OK;` |
|      - | 4567 | `	}` |
|      - | 4568 | `	/* Perform the requested operation */` |
|      3 | 4569 | `	pKe = pKey->pFirst;` |
|      3 | 4570 | `	pVe = pValue->pFirst;` |
|      9 | 4571 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      7 | 4572 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pKe),HashmapExtractNodeValue(pVe));` |
|      - | 4573 | `		/* Point to the next entry */` |
|      7 | 4574 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      7 | 4575 | `		pVe = pVe->pPrev;` |
|      4 | 4576 | `	}` |
|      - | 4577 | `	/* Return the filled array */` |
|      3 | 4578 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4579 | `	return PH7_OK;` |
|      2 | 4580 |  |
|      - | 4581 | `/*` |
|      - | 4582 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|      - | 4583 | ` *  Return an array with elements in reverse order.` |
|      - | 4584 | ` * Parameters` |
|      - | 4585 | ` *  $array` |
|      - | 4586 | ` *   The input array.` |
|      - | 4587 | ` *  $preserve_keys (optional)` |
|      - | 4588 | ` *   If set to TRUE keys are preserved.` |
|      - | 4589 | ` * Return` |
|      - | 4590 | ` *  The reversed array.` |
|      - | 4591 | ` */` |
|      6 | 4592 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4593 |  |
|      - | 4594 | `	ph7_hashmap_node *pEntry;` |
|      - | 4595 | `	ph7_hashmap *pSrc;` |
|      - | 4596 | `	ph7_value *pArray;` |
|      - | 4597 | `	int bPreserve;` |
|      - | 4598 | `	sxu32 n;` |
|      7 | 4599 | `	if( nArg < 1 ){` |
|      - | 4600 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4601 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4602 | `		return PH7_OK;` |
|      - | 4603 | `	}` |
|      - | 4604 | `	/* Make sure we are dealing with a valid hashmap */` |
|      7 | 4605 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4606 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 4607 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4608 | `		return PH7_OK;` |
|      - | 4609 | `	}` |
|      7 | 4610 | `	bPreserve = FALSE;` |
|      7 | 4611 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1]) ){` |
|      3 | 4612 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|      1 | 4613 | `	}` |
|      - | 4614 | `	/* Point to the internal representation of the input hashmap */` |
|      7 | 4615 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4616 | `	/* Create a new array */` |
|      7 | 4617 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4618 | `	if( pArray == 0 ){` |
|    ! 0 | 4619 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4620 | `		return PH7_OK;` |
|      - | 4621 | `	}` |
|      - | 4622 | `	/* Perform the requested operation */` |
|      7 | 4623 | `	pEntry = pSrc->pLast;` |
|     23 | 4624 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|     17 | 4625 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bPreserve);` |
|      - | 4626 | `		/* Point to the previous entry */` |
|     17 | 4627 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      9 | 4628 | `	}` |
|      7 | 4629 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4630 | `	return PH7_OK;` |
|      4 | 4631 |  |
|      - | 4632 | `/*` |
|      - | 4633 | ` * array array_unique(array $array[,int $sort_flags = SORT_STRING ])` |
|      - | 4634 | ` *  Removes duplicate values from an array` |
|      - | 4635 | ` * Parameter` |
|      - | 4636 | ` *  $array` |
|      - | 4637 | ` *   The input array.` |
|      - | 4638 | ` *  $sort_flags` |
|      - | 4639 | ` *    The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 4640 | ` *    Sorting type flags:` |
|      - | 4641 | ` *       SORT_REGULAR - compare items normally (don't change types)` |
|      - | 4642 | ` *       SORT_NUMERIC - compare items numerically` |
|      - | 4643 | ` *       SORT_STRING - compare items as strings` |
|      - | 4644 | ` *       SORT_LOCALE_STRING - compare items as` |
|      - | 4645 | ` * Return` |
|      - | 4646 | ` *  Filtered array or NULL on failure.` |
|      - | 4647 | ` */` |
|      2 | 4648 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4649 |  |
|      - | 4650 | `	ph7_hashmap_node *pEntry;` |
|      - | 4651 | `	ph7_value *pNeedle;` |
|      - | 4652 | `	ph7_hashmap *pSrc;` |
|      - | 4653 | `	ph7_value *pArray;` |
|      - | 4654 | `	int bStrict;` |
|      - | 4655 | `	sxi32 rc;` |
|      - | 4656 | `	sxu32 n;` |
|      3 | 4657 | `	if( nArg < 1 ){` |
|      - | 4658 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4659 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4660 | `		return PH7_OK;` |
|      - | 4661 | `	}` |
|      - | 4662 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 4663 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4664 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 4665 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4666 | `		return PH7_OK;` |
|      - | 4667 | `	}` |
|      3 | 4668 | `	bStrict = FALSE;` |
|      3 | 4669 | `	if( nArg > 1 ){` |
|    ! 0 | 4670 | `		bStrict = ph7_value_to_int(apArg[1]) == 3 /* SORT_REGULAR */ ? 1 : 0;` |
|    ! 0 | 4671 | `	}` |
|      - | 4672 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 4673 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4674 | `	/* Create a new array */` |
|      3 | 4675 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4676 | `	if( pArray == 0 ){` |
|    ! 0 | 4677 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4678 | `		return PH7_OK;` |
|      - | 4679 | `	}` |
|      - | 4680 | `	/* Perform the requested operation */` |
|      3 | 4681 | `	pEntry = pSrc->pFirst;` |
|     13 | 4682 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|     11 | 4683 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|     11 | 4684 | `		rc = SXERR_NOTFOUND;` |
|     11 | 4685 | `		if( pNeedle ){` |
|     11 | 4686 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      5 | 4687 | `		}` |
|     11 | 4688 | `		if( rc != SXRET_OK ){` |
|      - | 4689 | `			/* Perform the insertion */` |
|      7 | 4690 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      3 | 4691 | `		}` |
|      - | 4692 | `		/* Point to the next entry */` |
|     11 | 4693 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      6 | 4694 | `	}` |
|      - | 4695 | `	/* Return the freshly created array */` |
|      3 | 4696 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4697 | `	return PH7_OK;` |
|      2 | 4698 |  |
|      - | 4699 | `/*` |
|      - | 4700 | ` * array array_flip(array $input)` |
|      - | 4701 | ` *  Exchanges all keys with their associated values in an array.` |
|      - | 4702 | ` * Parameter` |
|      - | 4703 | ` *  $input` |
|      - | 4704 | ` *   Input array.` |
|      - | 4705 | ` * Return` |
|      - | 4706 | ` *   The flipped array on success or NULL on failure.` |
|      - | 4707 | ` */` |
|     28 | 4708 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4709 |  |
|      - | 4710 | `	ph7_hashmap_node *pEntry;` |
|      - | 4711 | `	ph7_hashmap *pSrc;` |
|      - | 4712 | `	ph7_value *pArray;` |
|      - | 4713 | `	ph7_value *pKey;` |
|      - | 4714 | `	ph7_value sVal;` |
|      - | 4715 | `	sxu32 n;` |
|     29 | 4716 | `	if( nArg < 1 ){` |
|      - | 4717 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4718 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4719 | `		return PH7_OK;` |
|      - | 4720 | `	}` |
|      - | 4721 | `	/* Make sure we are dealing with a valid hashmap */` |
|     29 | 4722 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4723 | `		/* Invalid argument,return NULL */` |
|      5 | 4724 | `		ph7_result_null(pCtx);` |
|      5 | 4725 | `		return PH7_OK;` |
|      - | 4726 | `	}` |
|      - | 4727 | `	/* Point to the internal representation of the input hashmap */` |
|     25 | 4728 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4729 | `	/* Create a new array */` |
|     25 | 4730 | `	pArray = ph7_context_new_array(pCtx);` |
|     25 | 4731 | `	if( pArray == 0 ){` |
|    ! 0 | 4732 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4733 | `		return PH7_OK;` |
|      - | 4734 | `	}` |
|      - | 4735 | `	/* Start processing */` |
|     25 | 4736 | `	pEntry = pSrc->pFirst;` |
|  22259 | 4737 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      - | 4738 | `		/* Extract the node value */` |
|  22235 | 4739 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|  22235 | 4740 | `		if( pKey && (pKey->iFlags & MEMOBJ_NULL) == 0){` |
|      - | 4741 | `			/* Prepare the value for insertion */` |
|  22233 | 4742 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|  20001 | 4743 | `				PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|  10001 | 4744 | `			}else{` |
|      - | 4745 | `				SyString sStr;` |
|   2233 | 4746 | `				SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|   2233 | 4747 | `				PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|      - | 4748 | `			}` |
|      - | 4749 | `			/* Perform the insertion */` |
|  22233 | 4750 | `			ph7_array_add_elem(pArray,pKey,&sVal);` |
|      - | 4751 | `			/* Safely release the value because each inserted entry` |
|      - | 4752 | `			 * have it's own private copy of the value.` |
|      - | 4753 | `			 */` |
|  22233 | 4754 | `			PH7_MemObjRelease(&sVal);` |
|  11116 | 4755 | `		}` |
|      - | 4756 | `		/* Point to the next entry */` |
|  22235 | 4757 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  11118 | 4758 | `	}` |
|      - | 4759 | `	/* Return the freshly created array */` |
|     25 | 4760 | `	ph7_result_value(pCtx,pArray);` |
|     25 | 4761 | `	return PH7_OK;` |
|     15 | 4762 |  |
|      - | 4763 | `/*` |
|      - | 4764 | ` * number array_sum(array $array )` |
|      - | 4765 | ` *  Calculate the sum of values in an array.` |
|      - | 4766 | ` * Parameters` |
|      - | 4767 | ` *  $array: The input array.` |
|      - | 4768 | ` * Return` |
|      - | 4769 | ` *  Returns the sum of values as an integer or float.` |
|      - | 4770 | ` */` |
|      4 | 4771 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|      1 | 4772 |  |
|      - | 4773 | `	ph7_hashmap_node *pEntry;` |
|      - | 4774 | `	ph7_value *pObj;` |
|      5 | 4775 | `	double dSum = 0;` |
|      - | 4776 | `	sxu32 n;` |
|      5 | 4777 | `	pEntry = pMap->pFirst;` |
|     19 | 4778 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     15 | 4779 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     15 | 4780 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     15 | 4781 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     15 | 4782 | `				dSum += pObj->rVal;` |
|      7 | 4783 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    ! 0 | 4784 | `				dSum += (double)pObj->x.iVal;` |
|    ! 0 | 4785 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4786 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4787 | `					double dv = 0;` |
|    ! 0 | 4788 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|    ! 0 | 4789 | `					dSum += dv;` |
|    ! 0 | 4790 | `				}` |
|    ! 0 | 4791 | `			}` |
|      7 | 4792 | `		}` |
|      - | 4793 | `		/* Point to the next entry */` |
|     15 | 4794 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      8 | 4795 | `	}` |
|      - | 4796 | `	/* Return sum */` |
|      5 | 4797 | `	ph7_result_double(pCtx,dSum);` |
|      5 | 4798 |  |
|      6 | 4799 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|      2 | 4800 |  |
|      - | 4801 | `	ph7_hashmap_node *pEntry;` |
|      - | 4802 | `	ph7_value *pObj;` |
|      8 | 4803 | `	sxi64 nSum = 0;` |
|      - | 4804 | `	sxu32 n;` |
|      8 | 4805 | `	pEntry = pMap->pFirst;` |
|     34 | 4806 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     28 | 4807 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     28 | 4808 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     28 | 4809 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 4810 | `				nSum += (sxi64)pObj->rVal;` |
|     28 | 4811 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     28 | 4812 | `				nSum += pObj->x.iVal;` |
|     13 | 4813 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4814 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4815 | `					sxi64 nv = 0;` |
|    ! 0 | 4816 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|    ! 0 | 4817 | `					nSum += nv;` |
|    ! 0 | 4818 | `				}` |
|    ! 0 | 4819 | `			}` |
|     13 | 4820 | `		}` |
|      - | 4821 | `		/* Point to the next entry */` |
|     28 | 4822 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     15 | 4823 | `	}` |
|      - | 4824 | `	/* Return sum */` |
|      8 | 4825 | `	ph7_result_int64(pCtx,nSum);` |
|      8 | 4826 |  |
|      - | 4827 | `/* number array_sum(array $array )` |
|      - | 4828 | ` * (See block-coment above)` |
|      - | 4829 | ` */` |
|     16 | 4830 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4831 |  |
|      - | 4832 | `	ph7_hashmap *pMap;` |
|      - | 4833 | `	ph7_value *pObj;` |
|     18 | 4834 | `	if( nArg < 1 ){` |
|      - | 4835 | `		/* Missing arguments,return 0 */` |
|      3 | 4836 | `		ph7_result_int(pCtx,0);` |
|      3 | 4837 | `		return PH7_OK;` |
|      - | 4838 | `	}` |
|      - | 4839 | `	/* Make sure we are dealing with a valid hashmap */` |
|     16 | 4840 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4841 | `		/* Invalid argument,return 0 */` |
|      5 | 4842 | `		ph7_result_int(pCtx,0);` |
|      5 | 4843 | `		return PH7_OK;` |
|      - | 4844 | `	}` |
|     12 | 4845 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     12 | 4846 | `	if( pMap->nEntry < 1 ){` |
|      - | 4847 | `		/* Nothing to compute,return 0 */` |
|    ! 0 | 4848 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4849 | `		return PH7_OK;` |
|      - | 4850 | `	}` |
|      - | 4851 | `	/* If the first element is of type float,then perform floating` |
|      - | 4852 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|      - | 4853 | `	 */` |
|     12 | 4854 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     12 | 4855 | `	if( pObj == 0 ){` |
|    ! 0 | 4856 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4857 | `		return PH7_OK;` |
|      - | 4858 | `	}` |
|     12 | 4859 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|      5 | 4860 | `		DoubleSum(pCtx,pMap);` |
|      3 | 4861 | `	}else{` |
|      8 | 4862 | `		Int64Sum(pCtx,pMap);` |
|      - | 4863 | `	}` |
|     12 | 4864 | `	return PH7_OK;` |
|     10 | 4865 |  |
|      - | 4866 | `/*` |
|      - | 4867 | ` * number array_product(array $array )` |
|      - | 4868 | ` *  Calculate the product of values in an array.` |
|      - | 4869 | ` * Parameters` |
|      - | 4870 | ` *  $array: The input array.` |
|      - | 4871 | ` * Return` |
|      - | 4872 | ` *  Returns the product of values as an integer or float.` |
|      - | 4873 | ` */` |
|    ! 0 | 4874 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|    ! 0 | 4875 |  |
|      - | 4876 | `	ph7_hashmap_node *pEntry;` |
|      - | 4877 | `	ph7_value *pObj;` |
|      - | 4878 | `	double dProd;` |
|      - | 4879 | `	sxu32 n;` |
|    ! 0 | 4880 | `	pEntry = pMap->pFirst;` |
|    ! 0 | 4881 | `	dProd = 1;` |
|    ! 0 | 4882 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    ! 0 | 4883 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    ! 0 | 4884 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|    ! 0 | 4885 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 4886 | `				dProd *= pObj->rVal;` |
|    ! 0 | 4887 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    ! 0 | 4888 | `				dProd *= (double)pObj->x.iVal;` |
|    ! 0 | 4889 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4890 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4891 | `					double dv = 0;` |
|    ! 0 | 4892 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|    ! 0 | 4893 | `					dProd *= dv;` |
|    ! 0 | 4894 | `				}` |
|    ! 0 | 4895 | `			}` |
|    ! 0 | 4896 | `		}` |
|      - | 4897 | `		/* Point to the next entry */` |
|    ! 0 | 4898 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    ! 0 | 4899 | `	}` |
|      - | 4900 | `	/* Return product */` |
|    ! 0 | 4901 | `	ph7_result_double(pCtx,dProd);` |
|    ! 0 | 4902 |  |
|      2 | 4903 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|      1 | 4904 |  |
|      - | 4905 | `	ph7_hashmap_node *pEntry;` |
|      - | 4906 | `	ph7_value *pObj;` |
|      - | 4907 | `	sxi64 nProd;` |
|      - | 4908 | `	sxu32 n;` |
|      3 | 4909 | `	pEntry = pMap->pFirst;` |
|      3 | 4910 | `	nProd = 1;` |
|      9 | 4911 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      7 | 4912 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      7 | 4913 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|      7 | 4914 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 4915 | `				nProd *= (sxi64)pObj->rVal;` |
|      7 | 4916 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      7 | 4917 | `				nProd *= pObj->x.iVal;` |
|      3 | 4918 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4919 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4920 | `					sxi64 nv = 0;` |
|    ! 0 | 4921 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|    ! 0 | 4922 | `					nProd *= nv;` |
|    ! 0 | 4923 | `				}` |
|    ! 0 | 4924 | `			}` |
|      3 | 4925 | `		}` |
|      - | 4926 | `		/* Point to the next entry */` |
|      7 | 4927 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      4 | 4928 | `	}` |
|      - | 4929 | `	/* Return product */` |
|      3 | 4930 | `	ph7_result_int64(pCtx,nProd);` |
|      3 | 4931 |  |
|      - | 4932 | `/* number array_product(array $array )` |
|      - | 4933 | ` * (See block-block comment above)` |
|      - | 4934 | ` */` |
|      2 | 4935 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4936 |  |
|      - | 4937 | `	ph7_hashmap *pMap;` |
|      - | 4938 | `	ph7_value *pObj;` |
|      3 | 4939 | `	if( nArg < 1 ){` |
|      - | 4940 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4941 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4942 | `		return PH7_OK;` |
|      - | 4943 | `	}` |
|      - | 4944 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 4945 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4946 | `		/* Invalid argument,return 0 */` |
|    ! 0 | 4947 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4948 | `		return PH7_OK;` |
|      - | 4949 | `	}` |
|      3 | 4950 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 4951 | `	if( pMap->nEntry < 1 ){` |
|      - | 4952 | `		/* Nothing to compute,return 0 */` |
|    ! 0 | 4953 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4954 | `		return PH7_OK;` |
|      - | 4955 | `	}` |
|      - | 4956 | `	/* If the first element is of type float,then perform floating` |
|      - | 4957 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|      - | 4958 | `	 */` |
|      3 | 4959 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|      3 | 4960 | `	if( pObj == 0 ){` |
|    ! 0 | 4961 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4962 | `		return PH7_OK;` |
|      - | 4963 | `	}` |
|      3 | 4964 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 4965 | `		DoubleProd(pCtx,pMap);` |
|    ! 0 | 4966 | `	}else{` |
|      3 | 4967 | `		Int64Prod(pCtx,pMap);` |
|      - | 4968 | `	}` |
|      3 | 4969 | `	return PH7_OK;` |
|      2 | 4970 |  |
|      - | 4971 | `/*` |
|      - | 4972 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|      - | 4973 | ` *  Pick one or more random entries out of an array.` |
|      - | 4974 | ` * Parameters` |
|      - | 4975 | ` * $input` |
|      - | 4976 | ` *  The input array.` |
|      - | 4977 | ` * $num_req` |
|      - | 4978 | ` *  Specifies how many entries you want to pick.` |
|      - | 4979 | ` * Return` |
|      - | 4980 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|      - | 4981 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|      - | 4982 | ` *  NULL is returned on failure.` |
|      - | 4983 | ` */` |
|      6 | 4984 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4985 |  |
|      - | 4986 | `	ph7_hashmap_node *pNode;` |
|      - | 4987 | `	ph7_hashmap *pMap;` |
|      7 | 4988 | `	int nItem = 1;` |
|      7 | 4989 | `	if( nArg < 1 ){` |
|      - | 4990 | `		/* Missing argument,return NULL */` |
|    ! 0 | 4991 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4992 | `		return PH7_OK;` |
|      - | 4993 | `	}` |
|      - | 4994 | `	/* Make sure we are dealing with an array */` |
|      7 | 4995 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 | 4996 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4997 | `		return PH7_OK;` |
|      - | 4998 | `	}` |
|      - | 4999 | `	/* Point to the internal representation of the input hashmap */` |
|      7 | 5000 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      7 | 5001 | `	if(pMap->nEntry < 1 ){` |
|      - | 5002 | `		/* Empty hashmap,return NULL */` |
|    ! 0 | 5003 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5004 | `		return PH7_OK;` |
|      - | 5005 | `	}` |
|      7 | 5006 | `	if( nArg > 1 ){` |
|      3 | 5007 | `		nItem = ph7_value_to_int(apArg[1]);` |
|      1 | 5008 | `	}` |
|      7 | 5009 | `	if( nItem < 2 ){` |
|      - | 5010 | `		sxu32 nEntry;` |
|      - | 5011 | `		/* Select a random number */` |
|      5 | 5012 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|      - | 5013 | `		/* Extract the desired entry.` |
|      - | 5014 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|      - | 5015 | `		 */` |
|      5 | 5016 | `		if( nEntry > pMap->nEntry / 2 ){` |
|      1 | 5017 | `			pNode = pMap->pLast;` |
|      1 | 5018 | `			nEntry = pMap->nEntry - nEntry;` |
|      1 | 5019 | `			if( nEntry > 1 ){` |
|    ! 0 | 5020 | `				for(;;){` |
|    ! 0 | 5021 | `					if( nEntry == 0 ){` |
|    ! 0 | 5022 | `						break;` |
|      - | 5023 | `					}` |
|      - | 5024 | `					/* Point to the previous entry */` |
|    ! 0 | 5025 | `					pNode = pNode->pNext; /* Reverse link */` |
|    ! 0 | 5026 | `					nEntry--;` |
|    ! 0 | 5027 | `				}` |
|    ! 0 | 5028 | `			}` |
|      1 | 5029 | `		}else{` |
|      4 | 5030 | `			pNode = pMap->pFirst;` |
|      1 | 5031 | `			for(;;){` |
|      5 | 5032 | `				if( nEntry == 0 ){` |
|      4 | 5033 | `					break;` |
|      - | 5034 | `				}` |
|      - | 5035 | `				/* Point to the next entry */` |
|      2 | 5036 | `				pNode = pNode->pPrev; /* Reverse link */` |
|      2 | 5037 | `				nEntry--;` |
|      1 | 5038 | `			}` |
|      - | 5039 | `		}` |
|      5 | 5040 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|      - | 5041 | `			/* Int key */` |
|      3 | 5042 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|      2 | 5043 | `		}else{` |
|      - | 5044 | `			/* Blob key */` |
|      3 | 5045 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|      - | 5046 | `		}` |
|      3 | 5047 | `	}else{` |
|      - | 5048 | `		ph7_value sKey,*pArray;` |
|      - | 5049 | `		ph7_hashmap *pDest;` |
|      - | 5050 | `		/* Create a new array */` |
|      3 | 5051 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 5052 | `		if( pArray == 0 ){` |
|    ! 0 | 5053 | `			ph7_result_null(pCtx);` |
|    ! 0 | 5054 | `			return PH7_OK;` |
|      - | 5055 | `		}` |
|      - | 5056 | `		/* Point to the internal representation of the hashmap */` |
|      3 | 5057 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|      3 | 5058 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|      - | 5059 | `		/* Copy the first n items */` |
|      3 | 5060 | `		pNode = pMap->pFirst;` |
|      3 | 5061 | `		if( nItem > (int)pMap->nEntry ){` |
|    ! 0 | 5062 | `			nItem = (int)pMap->nEntry;` |
|    ! 0 | 5063 | `		}` |
|      7 | 5064 | `		while( nItem > 0){` |
|      5 | 5065 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|      5 | 5066 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|      5 | 5067 | `			PH7_MemObjRelease(&sKey);` |
|      - | 5068 | `			/* Point to the next entry */` |
|      5 | 5069 | `			pNode = pNode->pPrev; /* Reverse link */` |
|      5 | 5070 | `			nItem--;` |
|      1 | 5071 | `		}` |
|      - | 5072 | `		/* Shuffle the array */` |
|      3 | 5073 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|      - | 5074 | `		/* Rehash node */` |
|      3 | 5075 | `		HashmapSortRehash(pDest);` |
|      - | 5076 | `		/* Return the random array */` |
|      3 | 5077 | `		ph7_result_value(pCtx,pArray);` |
|      - | 5078 | `	}` |
|      7 | 5079 | `	return PH7_OK;` |
|      4 | 5080 |  |
|      - | 5081 | `/*` |
|      - | 5082 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|      - | 5083 | ` *  Split an array into chunks.` |
|      - | 5084 | ` * Parameters` |
|      - | 5085 | ` * $input` |
|      - | 5086 | ` *   The array to work on` |
|      - | 5087 | ` * $size` |
|      - | 5088 | ` *   The size of each chunk` |
|      - | 5089 | ` * $preserve_keys` |
|      - | 5090 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|      - | 5091 | ` *   the chunk numerically.` |
|      - | 5092 | ` * Return` |
|      - | 5093 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|      - | 5094 | ` *  zero, with each dimension containing size elements.` |
|      - | 5095 | ` */` |
|     42 | 5096 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5097 |  |
|      - | 5098 | `	ph7_value *pArray,*pChunk;` |
|      - | 5099 | `	ph7_hashmap_node *pEntry;` |
|      - | 5100 | `	ph7_hashmap *pMap;` |
|      - | 5101 | `	int bPreserve;` |
|      - | 5102 | `	sxu32 nChunk;` |
|      - | 5103 | `	sxu32 nSize;` |
|      - | 5104 | `	sxu32 n;` |
|      - | 5105 | `	/* Argument count and types follow PHP semantics. */` |
|     44 | 5106 | `	if( nArg < 2 ){` |
|      - | 5107 | `		/* fewer than required arguments -> ArgumentCountError */` |
|      4 | 5108 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5109 | `			"ArgumentCountError",` |
|      - | 5110 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|      1 | 5111 | `			nArg` |
|      - | 5112 | `			);` |
|      - | 5113 | `	}` |
|     42 | 5114 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      4 | 5115 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5116 | `			"TypeError",` |
|      - | 5117 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|      1 | 5118 | `			ph7_type_name(apArg[0])` |
|      - | 5119 | `			);` |
|      - | 5120 | `	}` |
|      - | 5121 | `	/* Create a new array */` |
|     40 | 5122 | `	pArray = ph7_context_new_array(pCtx);` |
|     40 | 5123 | `	if( pArray == 0 ){` |
|    ! 0 | 5124 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5125 | `		return PH7_OK;` |
|      - | 5126 | `	}` |
|      - | 5127 | `	/* Point to the internal representation of the input hashmap */` |
|     40 | 5128 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5129 | `	/* Extract and validate the chunk size argument. */` |
|      - | 5130 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|     76 | 5131 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     78 | 5132 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|     38 | 5133 | `		ph7_value_is_bool(apArg[1]) ){` |
|    ! 0 | 5134 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5135 | `			"TypeError",` |
|      - | 5136 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|    ! 0 | 5137 | `			ph7_type_name(apArg[1])` |
|      - | 5138 | `			);` |
|      - | 5139 | `	}` |
|      - | 5140 | `	/* Strings that are non-numeric also produce a TypeError. */` |
|     40 | 5141 | `	if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5142 | `		int len;` |
|      3 | 5143 | `		sxu8 bReal = FALSE;` |
|      3 | 5144 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|      3 | 5145 | `			if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK \|\| bReal ){` |
|      3 | 5146 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5147 | `				"TypeError",` |
|      - | 5148 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|      - | 5149 | `				);` |
|      - | 5150 | `		}` |
|    ! 0 | 5151 | `	}` |
|      - | 5152 | `	/* If the value is a float with a fractional component, refuse it.` |
|      - | 5153 | `	 * PHP currently warns but may become an error in the future; we` |
|      - | 5154 | `	 * enforce that policy now so PHL behaviour is strict. */` |
|     38 | 5155 | `	if( ph7_value_is_float(apArg[1]) ){` |
|      3 | 5156 | `		double d = ph7_value_to_double(apArg[1]);` |
|      3 | 5157 | `		sxi64 i = (sxi64)d;` |
|      3 | 5158 | `		if( d != (double)i ){` |
|      3 | 5159 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5160 | `				"TypeError",` |
|      - | 5161 | `				"array_chunk(): Argument #2 ($length) must be of type int, float given"` |
|      - | 5162 | `				);` |
|      - | 5163 | `		}` |
|    ! 0 | 5164 | `	}` |
|      - | 5165 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|      - | 5166 | `	 * eliminated, this will not produce a warning. */` |
|      - | 5167 | `	{` |
|     36 | 5168 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|     36 | 5169 | `		if( nSizeSigned < 1 ){` |
|      - | 5170 | `			/* size <= 0 -> ValueError */` |
|      5 | 5171 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5172 | `				"ValueError",` |
|      - | 5173 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|      - | 5174 | `				);` |
|      - | 5175 | `		}` |
|     32 | 5176 | `		nSize = (sxu32)nSizeSigned;` |
|      - | 5177 | `	}` |
|     32 | 5178 | `	if( nSize >= pMap->nEntry ){` |
|      - | 5179 | `		/* Return the whole array */` |
|      3 | 5180 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|      3 | 5181 | `		ph7_result_value(pCtx,pArray);` |
|      3 | 5182 | `		return PH7_OK;` |
|      - | 5183 | `	}` |
|     30 | 5184 | `	bPreserve = 0;` |
|     30 | 5185 | `	if( nArg > 2 ){` |
|      - | 5186 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|      - | 5187 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|      - | 5188 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|      - | 5189 | `		 * normally, matching PHP behaviour. */` |
|     45 | 5190 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|     34 | 5191 | `			ph7_value_is_object(apArg[2]) \|\|` |
|     20 | 5192 | `			ph7_value_is_resource(apArg[2]) ){` |
|      7 | 5193 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5194 | `				"TypeError",` |
|      - | 5195 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|      4 | 5196 | `				ph7_type_name(apArg[2])` |
|      - | 5197 | `				);` |
|      - | 5198 | `		}` |
|     21 | 5199 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|     10 | 5200 | `	}` |
|      - | 5201 | `	/* Start processing */` |
|     25 | 5202 | `	pEntry = pMap->pFirst;` |
|     25 | 5203 | `	nChunk = 0;` |
|     25 | 5204 | `	pChunk = 0;` |
|     25 | 5205 | `	n = pMap->nEntry;` |
|     51 | 5206 | `	for( ;; ){` |
|    103 | 5207 | `		if( n < 1 ){` |
|      - | 5208 | `			/* When the loop terminates we may still have a current chunk` |
|      - | 5209 | `			 * that hasn't been added to the result array.  The previous` |
|      - | 5210 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|      - | 5211 | `			 * final chunk when the input size was an exact multiple of` |
|      - | 5212 | `			 * the chunk length.  Always append the pending chunk if it` |
|      - | 5213 | `			 * exists. */` |
|     25 | 5214 | `			if( pChunk ){` |
|     25 | 5215 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|     12 | 5216 | `			}` |
|     25 | 5217 | `			break;` |
|      - | 5218 | `		}` |
|     79 | 5219 | `		if( nChunk < 1 ){` |
|     67 | 5220 | `			if( pChunk ){` |
|      - | 5221 | `				/* Put the first chunk */` |
|     43 | 5222 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|     21 | 5223 | `			}` |
|      - | 5224 | `			/* Create a new dimension */` |
|     67 | 5225 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|      - | 5226 | `												   * will be automatically released as soon we return` |
|      - | 5227 | `												   * from this function */` |
|     67 | 5228 | `			if( pChunk == 0 ){` |
|    ! 0 | 5229 | `				break;` |
|      - | 5230 | `			}` |
|     67 | 5231 | `			nChunk = nSize;` |
|     33 | 5232 | `		}` |
|      - | 5233 | `		/* Insert the entry */` |
|     79 | 5234 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|      - | 5235 | `		/* Point to the next entry */` |
|     79 | 5236 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     79 | 5237 | `		nChunk--;` |
|     79 | 5238 | `		n--;` |
|      1 | 5239 | `	}` |
|      - | 5240 | `	/* Return the multidimensional array */` |
|     25 | 5241 | `	ph7_result_value(pCtx,pArray);` |
|     25 | 5242 | `	return PH7_OK;` |
|     23 | 5243 |  |
|      - | 5244 | `/*` |
|      - | 5245 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|      - | 5246 | ` *  Pad array to the specified length with a value.` |
|      - | 5247 | ` * $input` |
|      - | 5248 | ` *   Initial array of values to pad.` |
|      - | 5249 | ` * $pad_size` |
|      - | 5250 | ` *   New size of the array.` |
|      - | 5251 | ` * $pad_value` |
|      - | 5252 | ` *   Value to pad if input is less than pad_size.` |
|      - | 5253 | ` */` |
|      8 | 5254 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5255 |  |
|      - | 5256 | `	ph7_hashmap *pMap;` |
|      - | 5257 | `	ph7_value *pArray;` |
|      - | 5258 | `	int nEntry;` |
|      9 | 5259 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5260 | `		/* Invalid arguments,return NULL */` |
|    ! 0 | 5261 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5262 | `		return PH7_OK;` |
|      - | 5263 | `	}` |
|      - | 5264 | `	/* Create a new array */` |
|      9 | 5265 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 5266 | `	if( pArray == 0 ){` |
|    ! 0 | 5267 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5268 | `		return PH7_OK;` |
|      - | 5269 | `	}` |
|      - | 5270 | `	/* Point to the internal representation of the input hashmap */` |
|      9 | 5271 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5272 | `	/* Extract the total number of desired entry to insert */` |
|      9 | 5273 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      9 | 5274 | `	if( nEntry < 0 ){` |
|      5 | 5275 | `		nEntry = -nEntry;` |
|      5 | 5276 | `		if( nEntry > 1048576 ){` |
|    ! 0 | 5277 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|    ! 0 | 5278 | `		}` |
|      5 | 5279 | `		if( nEntry > (int)pMap->nEntry ){` |
|      3 | 5280 | `			nEntry -= (int)pMap->nEntry;` |
|      - | 5281 | `			/* Insert given items first */` |
|      7 | 5282 | `			while( nEntry > 0 ){` |
|      5 | 5283 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      5 | 5284 | `				nEntry--;` |
|      1 | 5285 | `			}` |
|      - | 5286 | `			/* Merge the two arrays */` |
|      3 | 5287 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      2 | 5288 | `		}else{` |
|      3 | 5289 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      1 | 5290 | `		}` |
|      7 | 5291 | `	}else if( nEntry > 0 ){` |
|      5 | 5292 | `		if( nEntry > 1048576 ){` |
|    ! 0 | 5293 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|    ! 0 | 5294 | `		}` |
|      5 | 5295 | `		if( nEntry > (int)pMap->nEntry ){` |
|      3 | 5296 | `			nEntry -= (int)pMap->nEntry;` |
|      - | 5297 | `			/* Merge the two arrays first */` |
|      3 | 5298 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      - | 5299 | `			/* Insert given items */` |
|      7 | 5300 | `			while( nEntry > 0 ){` |
|      5 | 5301 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      5 | 5302 | `				nEntry--;` |
|      1 | 5303 | `			}` |
|      2 | 5304 | `		}else{` |
|      3 | 5305 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      - | 5306 | `		}` |
|      2 | 5307 | `	}` |
|      - | 5308 | `	/* Return the new array */` |
|      9 | 5309 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 5310 | `	return PH7_OK;` |
|      5 | 5311 |  |
|      - | 5312 | `/*` |
|      - | 5313 | ` * array array_replace(array &$array,array &$array1,...)` |
|      - | 5314 | ` *  Replaces elements from passed arrays into the first array.` |
|      - | 5315 | ` * Parameters` |
|      - | 5316 | ` * $array` |
|      - | 5317 | ` *   The array in which elements are replaced.` |
|      - | 5318 | ` * $array1` |
|      - | 5319 | ` *   The array from which elements will be extracted.` |
|      - | 5320 | ` * ....` |
|      - | 5321 | ` *  More arrays from which elements will be extracted.` |
|      - | 5322 | ` *  Values from later arrays overwrite the previous values.` |
|      - | 5323 | ` * Return` |
|      - | 5324 | ` *  Returns an array, or NULL if an error occurs.` |
|      - | 5325 | ` */` |
|      2 | 5326 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5327 |  |
|      - | 5328 | `	ph7_hashmap *pMap;` |
|      - | 5329 | `	ph7_value *pArray;` |
|      - | 5330 | `	int i;` |
|      3 | 5331 | `	if( nArg < 1 ){` |
|      - | 5332 | `		/* Invalid arguments,return NULL */` |
|    ! 0 | 5333 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5334 | `		return PH7_OK;` |
|      - | 5335 | `	}` |
|      - | 5336 | `	/* Create a new array */` |
|      3 | 5337 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5338 | `	if( pArray == 0 ){` |
|    ! 0 | 5339 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5340 | `		return PH7_OK;` |
|      - | 5341 | `	}` |
|      - | 5342 | `	/* Perform the requested operation */` |
|      7 | 5343 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      5 | 5344 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|    ! 0 | 5345 | `			continue;` |
|      - | 5346 | `		}` |
|      - | 5347 | `		/* Point to the internal representation of the input hashmap */` |
|      5 | 5348 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      5 | 5349 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      3 | 5350 | `	}` |
|      - | 5351 | `	/* Return the new array */` |
|      3 | 5352 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5353 | `	return PH7_OK;` |
|      2 | 5354 |  |
|      - | 5355 | `/*` |
|      - | 5356 | ` * array array_filter(array $input [,callback $callback ])` |
|      - | 5357 | ` *  Filters elements of an array using a callback function.` |
|      - | 5358 | ` * Parameters` |
|      - | 5359 | ` *  $input` |
|      - | 5360 | ` *    The array to iterate over` |
|      - | 5361 | ` * $callback` |
|      - | 5362 | ` *    The callback function to use` |
|      - | 5363 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|      - | 5364 | ` *    will be removed.` |
|      - | 5365 | ` * Return` |
|      - | 5366 | ` *  The filtered array.` |
|      - | 5367 | ` */` |
|      8 | 5368 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5369 |  |
|      - | 5370 | `	ph7_hashmap_node *pEntry;` |
|      - | 5371 | `	ph7_hashmap *pMap;` |
|      - | 5372 | `	ph7_value *pArray;` |
|      - | 5373 | `	ph7_value sResult;   /* Callback result */` |
|      - | 5374 | `	ph7_value *pValue;` |
|      - | 5375 | `	sxi32 rc;` |
|      - | 5376 | `	int keep;` |
|      - | 5377 | `	sxu32 n;` |
|      9 | 5378 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5379 | `		/* Invalid arguments,return NULL */` |
|      5 | 5380 | `		ph7_result_null(pCtx);` |
|      5 | 5381 | `		return PH7_OK;` |
|      - | 5382 | `	}` |
|      - | 5383 | `	/* Create a new array */` |
|      5 | 5384 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 5385 | `	if( pArray == 0 ){` |
|    ! 0 | 5386 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5387 | `		return PH7_OK;` |
|      - | 5388 | `	}` |
|      - | 5389 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 5390 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 5391 | `	pEntry = pMap->pFirst;` |
|      5 | 5392 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      5 | 5393 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      - | 5394 | `	/* Perform the requested operation */` |
|     21 | 5395 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5396 | `		/* Extract node value */` |
|     17 | 5397 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     17 | 5398 | `		if( nArg > 1 && pValue ){` |
|      - | 5399 | `			/* Invoke the given callback */` |
|     17 | 5400 | `			keep = FALSE;` |
|     17 | 5401 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|     17 | 5402 | `			if( rc == SXRET_OK ){` |
|      - | 5403 | `				/* Perform a boolean cast */` |
|     17 | 5404 | `				keep = ph7_value_to_bool(&sResult);` |
|      8 | 5405 | `			}` |
|     17 | 5406 | `			PH7_MemObjRelease(&sResult);` |
|      9 | 5407 | `		}else{` |
|      - | 5408 | `			/* No available callback,check for empty item */` |
|    ! 0 | 5409 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|      - | 5410 | `		}` |
|     17 | 5411 | `		if( keep ){` |
|      - | 5412 | `			/* Perform the insertion,now the callback returned true */` |
|      5 | 5413 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 5414 | `		}` |
|      - | 5415 | `		/* Point to the next entry */` |
|     17 | 5416 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      9 | 5417 | `	}` |
|      5 | 5418 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 5419 | `	return PH7_OK;` |
|      5 | 5420 |  |
|      - | 5421 | `/*` |
|      - | 5422 | ` * array array_map(callback $callback,array $arr1)` |
|      - | 5423 | ` *  Applies the callback to the elements of the given arrays.` |
|      - | 5424 | ` * Parameters` |
|      - | 5425 | ` *  $callback` |
|      - | 5426 | ` *   Callback function to run for each element in each array.` |
|      - | 5427 | ` * $arr1` |
|      - | 5428 | ` *   An array to run through the callback function.` |
|      - | 5429 | ` * Return` |
|      - | 5430 | ` *  Returns an array containing all the elements of arr1 after applying` |
|      - | 5431 | ` *  the callback function to each one.` |
|      - | 5432 | ` * NOTE:` |
|      - | 5433 | ` *  array_map() passes only a single value to the callback.` |
|      - | 5434 | ` */` |
|     10 | 5435 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5436 |  |
|      - | 5437 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|      - | 5438 | `	ph7_hashmap_node *pEntry;` |
|      - | 5439 | `	ph7_hashmap *pMap;` |
|      - | 5440 | `	sxu32 n;` |
|     11 | 5441 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 5442 | `		/* Invalid arguments,return NULL */` |
|      5 | 5443 | `		ph7_result_null(pCtx);` |
|      5 | 5444 | `		return PH7_OK;` |
|      - | 5445 | `	}` |
|      - | 5446 | `	/* Create a new array */` |
|      7 | 5447 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5448 | `	if( pArray == 0 ){` |
|    ! 0 | 5449 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5450 | `		return PH7_OK;` |
|      - | 5451 | `	}` |
|      - | 5452 | `	/* Point to the internal representation of the input hashmap */` |
|      7 | 5453 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      7 | 5454 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      7 | 5455 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      7 | 5456 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      7 | 5457 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      - | 5458 | `	/* Perform the requested operation */` |
|      7 | 5459 | `	pEntry = pMap->pFirst;` |
|     21 | 5460 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5461 | `		/* Extrcat the node value */` |
|     15 | 5462 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     15 | 5463 | `		if( pValue ){` |
|      - | 5464 | `			sxi32 rc;` |
|      - | 5465 | `			/* Invoke the supplied callback */` |
|     15 | 5466 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|      - | 5467 | `			/* Extract the node key */` |
|     15 | 5468 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     15 | 5469 | `			if( rc != SXRET_OK ){` |
|      - | 5470 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|    ! 0 | 5471 | `				ph7_array_add_elem(pArray,&sKey,pValue); /* Keep the same value */` |
|    ! 0 | 5472 | `			}else{` |
|      - | 5473 | `				/* Insert the callback return value */` |
|     15 | 5474 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|      - | 5475 | `			}` |
|     15 | 5476 | `			PH7_MemObjRelease(&sKey);` |
|     15 | 5477 | `			PH7_MemObjRelease(&sResult);` |
|      7 | 5478 | `		}` |
|      - | 5479 | `		/* Point to the next entry */` |
|     15 | 5480 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      8 | 5481 | `	}` |
|      7 | 5482 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5483 | `	return PH7_OK;` |
|      6 | 5484 |  |
|      - | 5485 | `/*` |
|      - | 5486 | ` * value array_reduce(array $input,callback $function[, value $initial = NULL ])` |
|      - | 5487 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|      - | 5488 | ` * Parameters` |
|      - | 5489 | ` *  $input` |
|      - | 5490 | ` *   The input array.` |
|      - | 5491 | ` *  $function` |
|      - | 5492 | ` *  The callback function.` |
|      - | 5493 | ` * $initial` |
|      - | 5494 | ` *  If the optional initial is available, it will be used at the beginning` |
|      - | 5495 | ` *  of the process, or as a final result in case the array is empty.` |
|      - | 5496 | ` * Return` |
|      - | 5497 | ` *  Returns the resulting value.` |
|      - | 5498 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|      - | 5499 | ` */` |
|      4 | 5500 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5501 |  |
|      - | 5502 | `	ph7_hashmap_node *pEntry;` |
|      - | 5503 | `	ph7_hashmap *pMap;` |
|      - | 5504 | `	ph7_value *pValue;` |
|      - | 5505 | `	ph7_value sResult;` |
|      - | 5506 | `	sxu32 n;` |
|      5 | 5507 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5508 | `		/* Invalid/Missing arguments,return NULL */` |
|    ! 0 | 5509 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5510 | `		return PH7_OK;` |
|      - | 5511 | `	}` |
|      - | 5512 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 5513 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5514 | `	/* Assume a NULL initial value */` |
|      5 | 5515 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      5 | 5516 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      5 | 5517 | `	if( nArg > 2 ){` |
|      - | 5518 | `		/* Set the initial value */` |
|      5 | 5519 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|      2 | 5520 | `	}` |
|      - | 5521 | `	/* Perform the requested operation */` |
|      5 | 5522 | `	pEntry = pMap->pFirst;` |
|     19 | 5523 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5524 | `		/* Extract the node value */` |
|     15 | 5525 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      - | 5526 | `		/* Invoke the supplied callback */` |
|     15 | 5527 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      - | 5528 | `		/* Point to the next entry */` |
|     15 | 5529 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      8 | 5530 | `	}` |
|      5 | 5531 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      5 | 5532 | `	PH7_MemObjRelease(&sResult);` |
|      5 | 5533 | `	return PH7_OK;` |
|      3 | 5534 |  |
|      - | 5535 | `/*` |
|      - | 5536 | ` * bool array_walk(array &$array,callback $funcname [, value $userdata ] )` |
|      - | 5537 | ` *  Apply a user function to every member of an array.` |
|      - | 5538 | ` * Parameters` |
|      - | 5539 | ` *  $array` |
|      - | 5540 | ` *   The input array.` |
|      - | 5541 | ` * $funcname` |
|      - | 5542 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|      - | 5543 | ` *  the first, and the key/index second.` |
|      - | 5544 | ` * Note:` |
|      - | 5545 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|      - | 5546 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|      - | 5547 | ` *  be made in the original array itself.` |
|      - | 5548 | ` * $userdata` |
|      - | 5549 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|      - | 5550 | ` *  to the callback funcname.` |
|      - | 5551 | ` * Return` |
|      - | 5552 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 5553 | ` */` |
|     12 | 5554 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5555 |  |
|      - | 5556 | `	ph7_value *pValue,*pUserData,sKey;` |
|      - | 5557 | `	ph7_hashmap_node *pEntry;` |
|      - | 5558 | `	ph7_hashmap *pMap;` |
|      - | 5559 | `	sxi32 rc;` |
|      - | 5560 | `	sxu32 n;` |
|     13 | 5561 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5562 | `		/* Invalid/Missing arguments,return FALSE */` |
|    ! 0 | 5563 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5564 | `		return PH7_OK;` |
|      - | 5565 | `	}` |
|     13 | 5566 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|      - | 5567 | `	/* Point to the internal representation of the input hashmap */` |
|     13 | 5568 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     13 | 5569 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     13 | 5570 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      - | 5571 | `	/* Perform the desired operation */` |
|     13 | 5572 | `	pEntry = pMap->pFirst;` |
|     41 | 5573 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5574 | `		/* Extract the node value */` |
|     29 | 5575 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     29 | 5576 | `		if( pValue ){` |
|      - | 5577 | `			/* Extract the entry key */` |
|     29 | 5578 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      - | 5579 | `			/* Invoke the supplied callback */` |
|     29 | 5580 | `			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|     29 | 5581 | `			PH7_MemObjRelease(&sKey);` |
|     29 | 5582 | `			if( rc != SXRET_OK ){` |
|      - | 5583 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|    ! 0 | 5584 | `				ph7_result_bool(pCtx,0); /* return FALSE */` |
|    ! 0 | 5585 | `				return PH7_OK;` |
|      - | 5586 | `			}` |
|     14 | 5587 | `		}` |
|      - | 5588 | `		/* Point to the next entry */` |
|     29 | 5589 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     15 | 5590 | `	}` |
|      - | 5591 | `	/* All done,return TRUE */` |
|     13 | 5592 | `	ph7_result_bool(pCtx,1);` |
|     13 | 5593 | `	return PH7_OK;` |
|      7 | 5594 |  |
|      - | 5595 | `/*` |
|      - | 5596 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|      - | 5597 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|      - | 5598 | ` */` |
|      6 | 5599 | `static int HashmapWalkRecursive(` |
|      - | 5600 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|      - | 5601 | `	ph7_value *pCallback, /* User callback */` |
|      - | 5602 | `	ph7_value *pUserData, /* Callback private data */` |
|      - | 5603 | `	int iNest             /* Nesting level */` |
|      - | 5604 | `	)` |
|      1 | 5605 |  |
|      - | 5606 | `	ph7_hashmap_node *pEntry;` |
|      - | 5607 | `	ph7_value *pValue,sKey;` |
|      - | 5608 | `	sxi32 rc;` |
|      - | 5609 | `	sxu32 n;` |
|      - | 5610 | `	/* Iterate throw hashmap entries */` |
|      7 | 5611 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      7 | 5612 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      7 | 5613 | `	pEntry = pMap->pFirst;` |
|     17 | 5614 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5615 | `		/* Extract the node value */` |
|     11 | 5616 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     11 | 5617 | `		if( pValue ){` |
|     11 | 5618 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      5 | 5619 | `				if( iNest < 32 ){` |
|      - | 5620 | `					/* Recurse */` |
|      5 | 5621 | `					iNest++;` |
|      5 | 5622 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      5 | 5623 | `					iNest--;` |
|      2 | 5624 | `				}` |
|      3 | 5625 | `			}else{` |
|      - | 5626 | `				/* Extract the node key */` |
|      7 | 5627 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      - | 5628 | `				/* Invoke the supplied callback */` |
|      7 | 5629 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      7 | 5630 | `				PH7_MemObjRelease(&sKey);` |
|      7 | 5631 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 5632 | `					return rc;` |
|      - | 5633 | `				}` |
|      - | 5634 | `			}` |
|      5 | 5635 | `		}` |
|      - | 5636 | `		/* Point to the next entry */` |
|     11 | 5637 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      6 | 5638 | `	}` |
|      7 | 5639 | `	return SXRET_OK;` |
|      4 | 5640 |  |
|      - | 5641 | `/*` |
|      - | 5642 | ` * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )` |
|      - | 5643 | ` *  Apply a user function recursively to every member of an array.` |
|      - | 5644 | ` * Parameters` |
|      - | 5645 | ` *  $array` |
|      - | 5646 | ` *   The input array.` |
|      - | 5647 | ` * $funcname` |
|      - | 5648 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|      - | 5649 | ` *  the first, and the key/index second.` |
|      - | 5650 | ` * Note:` |
|      - | 5651 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|      - | 5652 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|      - | 5653 | ` *  be made in the original array itself.` |
|      - | 5654 | ` * $userdata` |
|      - | 5655 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|      - | 5656 | ` *  to the callback funcname.` |
|      - | 5657 | ` * Return` |
|      - | 5658 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 5659 | ` */` |
|      2 | 5660 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5661 |  |
|      - | 5662 | `	ph7_hashmap *pMap;` |
|      - | 5663 | `	sxi32 rc;` |
|      3 | 5664 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5665 | `		/* Invalid/Missing arguments,return FALSE */` |
|    ! 0 | 5666 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5667 | `		return PH7_OK;` |
|      - | 5668 | `	}` |
|      - | 5669 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 5670 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5671 | `	/* Perform the desired operation */` |
|      3 | 5672 | `	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|      - | 5673 | `	/* All done */` |
|      3 | 5674 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|      3 | 5675 | `	return PH7_OK;` |
|      2 | 5676 |  |
|      - | 5677 | `/*` |
|      - | 5678 | ` * Table of hashmap functions.` |
|      - | 5679 | ` */` |
|      - | 5680 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|      - | 5681 | `	{"count",             ph7_hashmap_count },` |
|      - | 5682 | `	{"sizeof",            ph7_hashmap_count },` |
|      - | 5683 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|      - | 5684 | `	{"array_pop",         ph7_hashmap_pop     },` |
|      - | 5685 | `	{"array_push",        ph7_hashmap_push    },` |
|      - | 5686 | `	{"array_shift",       ph7_hashmap_shift   },` |
|      - | 5687 | `	{"array_product",     ph7_hashmap_product },` |
|      - | 5688 | `	{"array_sum",         ph7_hashmap_sum     },` |
|      - | 5689 | `	{"array_keys",        ph7_hashmap_keys    },` |
|      - | 5690 | `	{"array_values",      ph7_hashmap_values  },` |
|      - | 5691 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|      - | 5692 | `	{"array_merge",       ph7_hashmap_merge   },` |
|      - | 5693 | `	{"array_slice",       ph7_hashmap_slice   },` |
|      - | 5694 | `	{"array_splice",      ph7_hashmap_splice  },` |
|      - | 5695 | `	{"array_search",      ph7_hashmap_search  },` |
|      - | 5696 | `	{"array_diff",        ph7_hashmap_diff    },` |
|      - | 5697 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|      - | 5698 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|      - | 5699 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|      - | 5700 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|      - | 5701 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|      - | 5702 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|      - | 5703 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|      - | 5704 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|      - | 5705 | `	{"array_copy",        ph7_hashmap_copy    },` |
|      - | 5706 | `	{"array_erase",       ph7_hashmap_erase   },` |
|      - | 5707 | `	{"array_fill",        ph7_hashmap_fill    },` |
|      - | 5708 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|      - | 5709 | `	{"array_combine",     ph7_hashmap_combine },` |
|      - | 5710 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|      - | 5711 | `	{"array_unique",      ph7_hashmap_unique  },` |
|      - | 5712 | `	{"array_flip",        ph7_hashmap_flip    },` |
|      - | 5713 | `	{"array_rand",        ph7_hashmap_rand    },` |
|      - | 5714 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|      - | 5715 | `	{"array_pad",         ph7_hashmap_pad     },` |
|      - | 5716 | `	{"array_replace",     ph7_hashmap_replace },` |
|      - | 5717 | `	{"array_filter",      ph7_hashmap_filter  },` |
|      - | 5718 | `	{"array_map",         ph7_hashmap_map     },` |
|      - | 5719 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|      - | 5720 | `	{"array_walk",        ph7_hashmap_walk    },` |
|      - | 5721 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|      - | 5722 | `	{"in_array",          ph7_hashmap_in_array},` |
|      - | 5723 | `	{"sort",              ph7_hashmap_sort    },` |
|      - | 5724 | `	{"asort",             ph7_hashmap_asort   },` |
|      - | 5725 | `	{"arsort",            ph7_hashmap_arsort  },` |
|      - | 5726 | `	{"ksort",             ph7_hashmap_ksort   },` |
|      - | 5727 | `	{"krsort",            ph7_hashmap_krsort  },` |
|      - | 5728 | `	{"rsort",             ph7_hashmap_rsort   },` |
|      - | 5729 | `	{"usort",             ph7_hashmap_usort   },` |
|      - | 5730 | `	{"uasort",            ph7_hashmap_uasort  },` |
|      - | 5731 | `	{"uksort",            ph7_hashmap_uksort  },` |
|      - | 5732 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|      - | 5733 | `	{"range",             ph7_hashmap_range   },` |
|      - | 5734 | `	{"current",           ph7_hashmap_current },` |
|      - | 5735 | `	{"each",              ph7_hashmap_each    },` |
|      - | 5736 | `	{"pos",               ph7_hashmap_current },` |
|      - | 5737 | `	{"next",              ph7_hashmap_next    },` |
|      - | 5738 | `	{"prev",              ph7_hashmap_prev    },` |
|      - | 5739 | `	{"end",               ph7_hashmap_end     },` |
|      - | 5740 | `	{"reset",             ph7_hashmap_reset   },` |
|      - | 5741 | `	{"key",               ph7_hashmap_simple_key }` |
|      - | 5742 | `};` |
|      - | 5743 | `/*` |
|      - | 5744 | ` * Register the built-in hashmap functions defined above.` |
|      - | 5745 | ` */` |
|   1062 | 5746 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|      2 | 5747 |  |
|      - | 5748 | `	sxu32 n;` |
|  65846 | 5749 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  64784 | 5750 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|  32393 | 5751 | `	}` |
|   1064 | 5752 |  |
|      - | 5753 | `/*` |
|      - | 5754 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|      - | 5755 | ` * the BLOB given as the first argument.` |
|      - | 5756 | ` * This function is typically invoked when the user issue a call to` |
|      - | 5757 | ` * [var_dump(),var_export(),print_r(),...]` |
|      - | 5758 | ` * This function SXRET_OK on success. Any other return value including` |
|      - | 5759 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - | 5760 | ` */` |
|     28 | 5761 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|      2 | 5762 |  |
|      - | 5763 | `	ph7_hashmap_node *pEntry;` |
|      - | 5764 | `	ph7_value *pObj;` |
|     30 | 5765 | `	sxu32 n = 0;` |
|      - | 5766 | `	int isRef;` |
|      - | 5767 | `	sxi32 rc;` |
|      - | 5768 | `	int i;` |
|     30 | 5769 | `	if( nDepth > 31 ){` |
|      - | 5770 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 5771 | `		/* Nesting limit reached */` |
|    ! 0 | 5772 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|    ! 0 | 5773 | `		if( ShowType ){` |
|    ! 0 | 5774 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|    ! 0 | 5775 | `		}` |
|    ! 0 | 5776 | `		return SXERR_LIMIT;` |
|      - | 5777 | `	}` |
|      - | 5778 | `	/* Point to the first inserted entry */` |
|     30 | 5779 | `	pEntry = pMap->pFirst;` |
|     30 | 5780 | `	rc = SXRET_OK;` |
|     30 | 5781 | `	if( !ShowType ){` |
|     15 | 5782 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|      7 | 5783 | `	}` |
|      - | 5784 | `	/* Total entries */` |
|     30 | 5785 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|      - | 5786 | `#ifdef __WINNT__` |
|      2 | 5787 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 5788 | `#else` |
|     28 | 5789 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 5790 | `#endif` |
|     65 | 5791 | `	for(;;){` |
|    132 | 5792 | `		if( n >= pMap->nEntry ){` |
|     30 | 5793 | `			break;` |
|      - | 5794 | `		}` |
|    206 | 5795 | `		for( i = 0 ; i < nTab ; i++ ){` |
|    104 | 5796 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     53 | 5797 | `		}` |
|      - | 5798 | `		/* Dump key */` |
|    104 | 5799 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|     37 | 5800 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|     19 | 5801 | `		}else{` |
|    101 | 5802 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|     33 | 5803 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|      - | 5804 | `		}` |
|      - | 5805 | `#ifdef __WINNT__` |
|      2 | 5806 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 5807 | `#else` |
|    102 | 5808 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 5809 | `#endif` |
|      - | 5810 | `		/* Dump node value */` |
|    104 | 5811 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    104 | 5812 | `		isRef = 0;` |
|    104 | 5813 | `		if( pObj ){` |
|    104 | 5814 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|      - | 5815 | `				/* Referenced object */` |
|    ! 0 | 5816 | `				isRef = 1;` |
|    ! 0 | 5817 | `			}` |
|    104 | 5818 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|    104 | 5819 | `			if( rc == SXERR_LIMIT ){` |
|    ! 0 | 5820 | `				break;` |
|      - | 5821 | `			}` |
|     51 | 5822 | `		}` |
|      - | 5823 | `		/* Point to the next entry */` |
|    104 | 5824 | `		n++;` |
|    104 | 5825 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2 | 5826 | `	}` |
|     58 | 5827 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     30 | 5828 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     16 | 5829 | `	}` |
|     30 | 5830 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|     30 | 5831 | `	return rc;` |
|     16 | 5832 |  |
|      - | 5833 | `/*` |
|      - | 5834 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|      - | 5835 | ` * retrieved entry.` |
|      - | 5836 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 5837 | ` * the entry value in the callback body will not alter the real value.` |
|      - | 5838 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 5839 | ` * a value different from PH7_OK.` |
|      - | 5840 | ` * Refer to [ph7_array_walk()] for more information.` |
|      - | 5841 | ` */` |
|  17516 | 5842 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|      - | 5843 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - | 5844 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|      - | 5845 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 5846 | `	)` |
|      2 | 5847 |  |
|      - | 5848 | `	ph7_hashmap_node *pEntry;` |
|      - | 5849 | `	ph7_value sKey,sValue;` |
|      - | 5850 | `	sxi32 rc;` |
|      - | 5851 | `	sxu32 n;` |
|      - | 5852 | `	/* Initialize walker parameter */` |
|  17518 | 5853 | `	rc = SXRET_OK;` |
|  17518 | 5854 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|  17518 | 5855 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|  17518 | 5856 | `	n = pMap->nEntry;` |
|  17518 | 5857 | `	pEntry = pMap->pFirst;` |
|      - | 5858 | `	/* Start the iteration process */` |
|  48693 | 5859 | `	for(;;){` |
|  97388 | 5860 | `		if( n < 1 ){` |
|  17518 | 5861 | `			break;` |
|      - | 5862 | `		}` |
|      - | 5863 | `		/* Extract a copy of the key and a copy the current value */` |
|  79872 | 5864 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  79872 | 5865 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|      - | 5866 | `		/* Invoke the user callback */` |
|  79872 | 5867 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|      - | 5868 | `		/* Release the copy of the key and the value */` |
|  79872 | 5869 | `		PH7_MemObjRelease(&sKey);` |
|  79872 | 5870 | `		PH7_MemObjRelease(&sValue);` |
|  79872 | 5871 | `		if( rc != PH7_OK ){` |
|      - | 5872 | `			/* Callback request an operation abort */` |
|    ! 0 | 5873 | `			return SXERR_ABORT;` |
|      - | 5874 | `		}` |
|      - | 5875 | `		/* Point to the next entry */` |
|  79872 | 5876 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  79872 | 5877 | `		n--;` |
|      2 | 5878 | `	}` |
|      - | 5879 | `	/* All done */` |
|  17518 | 5880 | `	return SXRET_OK;` |
|   8760 | 5881 |  |
|      - | 5882 |  |
