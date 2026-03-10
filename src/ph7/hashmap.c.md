# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2779/3238 lines (85.82%)

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
| 2830464 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2830466 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  234118 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  234120 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  234120 |   29 | `	sxu32 nH = 5381;` |
|  234120 |   30 | `	zEnd = &zIn[nLen];` |
|  267383 |   31 | `	for(;;){` |
|  534768 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  478176 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  431560 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  350672 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  234120 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecurisve is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * If the nesting limit is reached,this function abort immediately.` |
|       - |   43 | ` */` |
|     848 |   44 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)` |
|       2 |   45 |  |
|     850 |   46 | `	sxi64 iCount = 0;` |
|     850 |   47 | `	if( !bRecursive ){` |
|     574 |   48 | `		iCount = pMap->nEntry;` |
|     288 |   49 | `	}else{` |
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
|     846 |   79 | `	return iCount;` |
|     426 |   80 |  |
|       - |   81 | `/*` |
|       - |   82 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   83 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   84 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   85 | ` */` |
| 2775658 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   87 |  |
|       - |   88 | `	ph7_hashmap_node *pNode;` |
|       - |   89 | `	/* Allocate a new node */` |
| 2775660 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2775660 |   91 | `	if( pNode == 0 ){` |
|     ! 0 |   92 | `		return 0;` |
|       - |   93 | `	}` |
|       - |   94 | `	/* Zero the stucture */` |
| 2775660 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |   96 | `	/* Fill in the structure */` |
| 2775660 |   97 | `	pNode->pMap  = &(*pMap);` |
| 2775660 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2775660 |   99 | `	pNode->nHash = nHash;` |
| 2775660 |  100 | `	pNode->xKey.iKey = iKey;` |
| 2775660 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 2775660 |  102 | `	return pNode;` |
| 1387831 |  103 |  |
|       - |  104 | `/*` |
|       - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  108 | ` */` |
|   81450 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  110 |  |
|       - |  111 | `	ph7_hashmap_node *pNode;` |
|       - |  112 | `	/* Allocate a new node */` |
|   81452 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   81452 |  114 | `	if( pNode == 0 ){` |
|     ! 0 |  115 | `		return 0;` |
|       - |  116 | `	}` |
|       - |  117 | `	/* Zero the stucture */` |
|   81452 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  119 | `	/* Fill in the structure */` |
|   81452 |  120 | `	pNode->pMap  = &(*pMap);` |
|   81452 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   81452 |  122 | `	pNode->nHash = nHash;` |
|   81452 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   81452 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   81452 |  125 | `	pNode->nValIdx = nValIdx;` |
|   81452 |  126 | `	return pNode;` |
|   40727 |  127 |  |
|       - |  128 | `/*` |
|       - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  130 | ` */` |
| 2857108 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  132 |  |
|       - |  133 | `	/* Link */` |
| 2857110 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2633882 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2633882 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1316940 |  137 | `	}` |
| 2857110 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  139 | `	/* Link to the map list */` |
| 2857110 |  140 | `	if( pMap->pFirst == 0 ){` |
|   38098 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  142 | `		/* Point to the first inserted node */` |
|   38098 |  143 | `		pMap->pCur = pNode;` |
|   19050 |  144 | `	}else{` |
| 2819014 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  146 | `	}` |
| 2857110 |  147 | `	++pMap->nEntry;` |
| 2857110 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Unlink a node from the hashmap.` |
|       - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  152 | ` */` |
|    5736 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  154 |  |
|    5738 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5738 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  157 | `	/* Unlink from the corresponding bucket */` |
|    5738 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|    5318 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2660 |  160 | `	}else{` |
|     421 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  162 | `	}` |
|    5738 |  163 | `	if( pNode->pNextCollide ){` |
|    4473 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2236 |  165 | `	}` |
|    5738 |  166 | `	if( pMap->pFirst == pNode ){` |
|      78 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|      38 |  168 | `	}` |
|    5738 |  169 | `	if( pMap->pCur == pNode ){` |
|       - |  170 | `		/* Advance the node cursor */` |
|      80 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      39 |  172 | `	}` |
|       - |  173 | `	/* Unlink from the map list */` |
|    5738 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5738 |  175 | `	if( bRestore ){` |
|       - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     100 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  178 | `		/* Restore to the freelist */` |
|     100 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     100 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      49 |  181 | `		}` |
|      49 |  182 | `	}` |
|    5738 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5621 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2810 |  185 | `	}` |
|    5738 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5738 |  187 | `	pMap->nEntry--;` |
|    5738 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  189 | `		/* Free the hash-bucket */` |
|      34 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      34 |  191 | `		pMap->apBucket = 0;` |
|      34 |  192 | `		pMap->nSize = 0;` |
|      34 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      16 |  194 | `	}` |
|    5738 |  195 |  |
|       - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  197 | `/*` |
|       - |  198 | ` * Grow the hash-table and rehash all entries.` |
|       - |  199 | ` */` |
| 2857108 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  201 |  |
| 2857110 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   41820 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   41820 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  206 | `		sxu32 nBucket;` |
|       - |  207 | `		sxu32 n;` |
|   41820 |  208 | `		if( nNew < 1 ){` |
|   38098 |  209 | `			nNew = 16;` |
|   19048 |  210 | `		}` |
|       - |  211 | `		/* Allocate a new bucket */` |
|   41820 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   41820 |  213 | `		if( apNew == 0 ){` |
|     ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|       - |  216 | `			}` |
|       - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  218 | `			return SXRET_OK;` |
|       - |  219 | `		}` |
|       - |  220 | `		/* Zero the table */` |
|   41820 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  222 | `		/* Reflect the change */` |
|   41820 |  223 | `		pMap->apBucket = apNew;` |
|   41820 |  224 | `		pMap->nSize = nNew;` |
|   41820 |  225 | `		if( apOld == 0 ){` |
|       - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   38098 |  227 | `			return SXRET_OK;` |
|       - |  228 | `		}` |
|       - |  229 | `		/* Rehash old entries */` |
|    3724 |  230 | `		pEntry = pMap->pFirst;` |
|    3724 |  231 | `		n = 0;` |
| 1945765 |  232 | `		for( ;; ){` |
| 3891532 |  233 | `			if( n >= pMap->nEntry ){` |
|    3724 |  234 | `				break;` |
|       - |  235 | `			}` |
|       - |  236 | `			/* Clear the old collision link */` |
| 3887810 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  238 | `			/* Link to the new bucket */` |
| 3887810 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3887810 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3421668 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3421668 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1710833 |  243 | `			}` |
| 3887810 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  245 | `			/* Point to the next entry */` |
| 3887810 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3887810 |  247 | `			n++;` |
|       2 |  248 | `		}` |
|       - |  249 | `		/* Free the old table */` |
|    3724 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1861 |  251 | `	}` |
| 2819014 |  252 | `	return SXRET_OK;` |
| 1428556 |  253 |  |
|       - |  254 | `/*` |
|       - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  256 | ` * hashmap.` |
|       - |  257 | ` */` |
| 2775658 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  259 |  |
|       - |  260 | `	ph7_hashmap_node *pNode;` |
|       - |  261 | `	sxu32 nIdx;` |
|       - |  262 | `	sxu32 nHash;` |
|       - |  263 | `	sxi32 rc;` |
| 2775660 |  264 | `	if( !isForeign ){` |
|       - |  265 | `		ph7_value *pObj;` |
|       - |  266 | `		/* Reserve a ph7_value for the value */` |
| 2775636 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2775636 |  268 | `		if( pObj == 0 ){` |
|     ! 0 |  269 | `			return SXERR_MEM;` |
|       - |  270 | `		}` |
| 2775636 |  271 | `		if( pValue ){` |
|       - |  272 | `			/* Duplicate the value */` |
| 2775636 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 1387817 |  274 | `		}` |
| 2775636 |  275 | `		nIdx = pObj->nIdx;` |
| 1387819 |  276 | `	}else{` |
|      25 |  277 | `		nIdx = nRefIdx;` |
|       - |  278 | `	}` |
|       - |  279 | `	/* Hash the key */` |
| 2775660 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  281 | `	/* Allocate a new int node */` |
| 2775660 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2775660 |  283 | `	if( pNode == 0 ){` |
|     ! 0 |  284 | `		return SXERR_MEM;` |
|       - |  285 | `	}` |
| 2775660 |  286 | `	if( isForeign ){` |
|       - |  287 | `		/* Mark as a foregin entry */` |
|      25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      12 |  289 | `	}` |
|       - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2775660 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2775660 |  292 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  294 | `		return rc;` |
|       - |  295 | `	}` |
|       - |  296 | `	/* Perform the insertion */` |
| 2775660 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  298 | `	/* Install in the reference table */` |
| 2775660 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  300 | `	/* All done */` |
| 2775660 |  301 | `	return SXRET_OK;` |
| 1387831 |  302 |  |
|       - |  303 | `/*` |
|       - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  305 | ` * hashmap.` |
|       - |  306 | ` */` |
|   81450 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  308 |  |
|       - |  309 | `	ph7_hashmap_node *pNode;` |
|       - |  310 | `	sxu32 nHash;` |
|       - |  311 | `	sxu32 nIdx;` |
|       - |  312 | `	sxi32 rc;` |
|   81452 |  313 | `	if( !isForeign ){` |
|       - |  314 | `		ph7_value *pObj;` |
|       - |  315 | `		/* Reserve a ph7_value for the value */` |
|   59732 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   59732 |  317 | `		if( pObj == 0 ){` |
|     ! 0 |  318 | `			return SXERR_MEM;` |
|       - |  319 | `		}` |
|   59732 |  320 | `		if( pValue ){` |
|       - |  321 | `			/* Duplicate the value */` |
|   59732 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|   29865 |  323 | `		}` |
|   59732 |  324 | `		nIdx = pObj->nIdx;` |
|   29867 |  325 | `	}else{` |
|   21722 |  326 | `		nIdx = nRefIdx;` |
|       - |  327 | `	}` |
|       - |  328 | `	/* Hash the key */` |
|   81452 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  330 | `	/* Allocate a new blob node */` |
|   81452 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   81452 |  332 | `	if( pNode == 0 ){` |
|     ! 0 |  333 | `		return SXERR_MEM;` |
|       - |  334 | `	}` |
|   81452 |  335 | `	if( isForeign ){` |
|       - |  336 | `		/* Mark as a foregin entry */` |
|   21722 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   10860 |  338 | `	}` |
|       - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   81452 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   81452 |  341 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  343 | `		return rc;` |
|       - |  344 | `	}` |
|       - |  345 | `	/* Perform the insertion */` |
|   81452 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  347 | `	/* Install in the reference table */` |
|   81452 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  349 | `	/* All done */` |
|   81452 |  350 | `	return SXRET_OK;` |
|   40727 |  351 |  |
|       - |  352 | `/*` |
|       - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  356 | ` */` |
|   46878 |  357 | `static sxi32 HashmapLookupIntKey(` |
|       - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  359 | `	sxi64 iKey,                /* lookup key */` |
|       - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  361 | `	)` |
|       2 |  362 |  |
|       - |  363 | `	ph7_hashmap_node *pNode;` |
|       - |  364 | `	sxu32 nHash;` |
|   46880 |  365 | `	if( pMap->nEntry < 1 ){` |
|       - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|     388 |  367 | `		return SXERR_NOTFOUND;` |
|       - |  368 | `	}` |
|       - |  369 | `	/* Hash the key first */` |
|   46494 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  371 | `	/* Point to the appropriate bucket */` |
|   46494 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  373 | `	/* Perform the lookup */` |
|  411603 |  374 | `	for(;;){` |
|  823208 |  375 | `		if( pNode == 0 ){` |
|   45802 |  376 | `			break;` |
|       - |  377 | `		}` |
|  777752 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774388 |  379 | `			&& pNode->nHash == nHash` |
|  386033 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  381 | `				/* Node found */` |
|     694 |  382 | `				if( ppNode ){` |
|     682 |  383 | `					*ppNode = pNode;` |
|     340 |  384 | `				}` |
|     694 |  385 | `				return SXRET_OK;` |
|       - |  386 | `		}` |
|       - |  387 | `		/* Follow the collision link */` |
|  776715 |  388 | `		pNode = pNode->pNextCollide;` |
|       1 |  389 | `	}` |
|       - |  390 | `	/* No such entry */` |
|   45802 |  391 | `	return SXERR_NOTFOUND;` |
|   23441 |  392 |  |
|       - |  393 | `/*` |
|       - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  397 | ` */` |
|  161328 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  400 | `	const void *pKey,           /* Lookup key */` |
|       - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  403 | `	)` |
|       2 |  404 |  |
|       - |  405 | `	ph7_hashmap_node *pNode;` |
|       - |  406 | `	sxu32 nHash;` |
|  161330 |  407 | `	if( pMap->nEntry < 1 ){` |
|       - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|    8662 |  409 | `		return SXERR_NOTFOUND;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* Hash the key first */` |
|  152670 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  413 | `	/* Point to the appropriate bucket */` |
|  152670 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  415 | `	/* Perform the lookup */` |
|  150499 |  416 | `	for(;;){` |
|  301000 |  417 | `		if( pNode == 0 ){` |
|  115862 |  418 | `			break;` |
|       - |  419 | `		}` |
|  203542 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  183638 |  421 | `			&& pNode->nHash == nHash` |
|  109473 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   36810 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  424 | `				/* Node found */` |
|   36810 |  425 | `				if( ppNode ){` |
|   36782 |  426 | `					*ppNode = pNode;` |
|   18390 |  427 | `				}` |
|   36810 |  428 | `				return SXRET_OK;` |
|       - |  429 | `		}` |
|       - |  430 | `		/* Follow the collision link */` |
|  148332 |  431 | `		pNode = pNode->pNextCollide;` |
|       2 |  432 | `	}` |
|       - |  433 | `	/* No such entry */` |
|  115862 |  434 | `	return SXERR_NOTFOUND;` |
|   80666 |  435 |  |
|       - |  436 | `/*` |
|       - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  439 | ` */` |
|  161470 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  441 |  |
|  161472 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  161472 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  161472 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  445 | `		/* Octal not decimal number */` |
|       5 |  446 | `		return FALSE;` |
|       - |  447 | `	}` |
|  161468 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  449 | `		zIn++;` |
|     ! 0 |  450 | `	}` |
|   81066 |  451 | `	for(;;){` |
|  162134 |  452 | `		if( zIn >= zEnd ){` |
|     233 |  453 | `			return TRUE;` |
|       - |  454 | `		}` |
|  161902 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   80619 |  456 | `			break;` |
|       - |  457 | `		}` |
|     667 |  458 | `		zIn++;` |
|       1 |  459 | `	}` |
|       - |  460 | `	/* Key does not look like a decimal number */` |
|  161236 |  461 | `	return FALSE;` |
|   80737 |  462 |  |
|       - |  463 | `/*` |
|       - |  464 | ` * Check if a given key exists in the given hashmap.` |
|       - |  465 | ` * Write a pointer to the target node on success.` |
|       - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  467 | ` */` |
|   80468 |  468 | `static sxi32 HashmapLookup(` |
|       - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  472 | `	)` |
|       2 |  473 |  |
|   80470 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  475 | `	sxi32 rc;` |
|   80470 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   79870 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  478 | `			/* Force a string cast */` |
|     ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  480 | `		}` |
|   79870 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  482 | `			/* Perform a blob lookup */` |
|   79854 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   79854 |  484 | `			goto result;` |
|       - |  485 | `		}` |
|       8 |  486 | `	}` |
|       - |  487 | `	/* Perform an int lookup */` |
|     618 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  489 | `		/* Force an integer cast */` |
|      27 |  490 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  491 | `	}` |
|       - |  492 | `	/* Perform an int lookup */` |
|     618 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   40234 |  494 | `result:` |
|   80470 |  495 | `	if( rc == SXRET_OK ){` |
|       - |  496 | `		/* Node found */` |
|   37330 |  497 | `		if( ppNode ){` |
|   37306 |  498 | `			*ppNode = pNode;` |
|   18652 |  499 | `		}` |
|   37330 |  500 | `		return SXRET_OK;` |
|       - |  501 | `	}` |
|       - |  502 | `	/* No such entry */` |
|   43142 |  503 | `	return SXERR_NOTFOUND;` |
|   40236 |  504 |  |
|       - |  505 | `/*` |
|       - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  507 | ` * hashmap.` |
|       - |  508 | ` * If a node with the given key already exists in the database` |
|       - |  509 | ` * then this function overwrite the old value.` |
|       - |  510 | ` */` |
| 2835162 |  511 | `static sxi32 HashmapInsert(` |
|       - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  514 | `	ph7_value *pVal    /* Node value */` |
|       - |  515 | `	)` |
|       2 |  516 |  |
| 2835164 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 2835164 |  518 | `	sxi32 rc = SXRET_OK;` |
| 2835164 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   59916 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  521 | `			/* Force a string cast */` |
|       3 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  523 | `		}` |
|   59916 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  526 | `				/* Automatic index assign */` |
|      34 |  527 | `				pKey = 0;` |
|      16 |  528 | `			}` |
|     256 |  529 | `			goto IntKey;` |
|       - |  530 | `		}` |
|   89492 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   29830 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  533 | `				/* Overwrite the old value */` |
|       - |  534 | `				ph7_value *pElem;` |
|      27 |  535 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      27 |  536 | `				if( pElem ){` |
|      27 |  537 | `					if( pVal ){` |
|      27 |  538 | `						PH7_MemObjStore(pVal,pElem);` |
|      14 |  539 | `					}else{` |
|       - |  540 | `						/* Nullify the entry */` |
|     ! 0 |  541 | `						PH7_MemObjToNull(pElem);` |
|       - |  542 | `					}` |
|      13 |  543 | `				}` |
|      27 |  544 | `				return SXRET_OK;` |
|       - |  545 | `		}` |
|   59636 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  547 | `			/* Forbidden */` |
|       3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  549 | `			return SXRET_OK;` |
|       - |  550 | `		}` |
|       - |  551 | `		/* Perform a blob-key insertion */` |
|   59634 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   59634 |  553 | `		return rc;` |
|       - |  554 | `	}` |
| 1387624 |  555 | `IntKey:` |
| 2775504 |  556 | `	if( pKey ){` |
|   23242 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  558 | `			/* Force an integer cast */` |
|     251 |  559 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  560 | `		}` |
|   23242 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  562 | `			/* Overwrite the old value */` |
|       - |  563 | `			ph7_value *pElem;` |
|      47 |  564 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      47 |  565 | `			if( pElem ){` |
|      47 |  566 | `				if( pVal ){` |
|      47 |  567 | `					PH7_MemObjStore(pVal,pElem);` |
|      24 |  568 | `				}else{` |
|       - |  569 | `					/* Nullify the entry */` |
|     ! 0 |  570 | `					PH7_MemObjToNull(pElem);` |
|       - |  571 | `				}` |
|      23 |  572 | `			}` |
|      47 |  573 | `			return SXRET_OK;` |
|       - |  574 | `		}` |
|   23196 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  576 | `			/* Forbidden */` |
|       3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  578 | `			return SXRET_OK;` |
|       - |  579 | `		}` |
|       - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|   23194 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23194 |  582 | `		if( rc == SXRET_OK ){` |
|   23194 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  584 | `				/* Increment the automatic index */` |
|   22966 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  586 | `				/* Make sure the automatic index is not reserved */` |
|   22966 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  588 | `					pMap->iNextIdx++;` |
|     ! 0 |  589 | `				}` |
|   11482 |  590 | `			}` |
|   11596 |  591 | `		}` |
|   11598 |  592 | `	}else{` |
| 2752264 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  594 | `			/* Forbidden */` |
|       3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  596 | `			return SXRET_OK;` |
|       - |  597 | `		}` |
|       - |  598 | `		/* Assign an automatic index */` |
| 2752262 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2752262 |  600 | `		if( rc == SXRET_OK ){` |
| 2752262 |  601 | `			++pMap->iNextIdx;` |
| 1376130 |  602 | `		}` |
|       - |  603 | `	}` |
|       - |  604 | `	/* Insertion result */` |
| 2775454 |  605 | `	return rc;` |
| 1417583 |  606 |  |
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
|   21750 |  634 | `static sxi32 HashmapInsertByRef(` |
|       - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  638 | `	)` |
|       2 |  639 |  |
|   21752 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|   21752 |  641 | `	sxi32 rc = SXRET_OK;` |
|   21752 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   21728 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  644 | `			/* Force a string cast */` |
|     ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  646 | `		}` |
|   21728 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  649 | `				/* Automatic index assign */` |
|     ! 0 |  650 | `				pKey = 0;` |
|     ! 0 |  651 | `			}` |
|     ! 0 |  652 | `			goto IntKey;` |
|       - |  653 | `		}` |
|   32591 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   10863 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  656 | `				/* Overwrite */` |
|       7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|       - |  659 | `				/* Install in the reference table */` |
|       7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  661 | `				return SXRET_OK;` |
|       - |  662 | `		}` |
|       - |  663 | `		/* Perform a blob-key insertion */` |
|   21722 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   21722 |  665 | `		return rc;` |
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
|   10877 |  702 |  |
|       - |  703 | `/*` |
|       - |  704 | ` * Extract node value.` |
|       - |  705 | ` */` |
|  898626 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  707 |  |
|       - |  708 | `	/* Point to the desired object */` |
|       - |  709 | `	ph7_value *pObj;` |
|  898628 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  898628 |  711 | `	return pObj;` |
|       2 |  712 |  |
|       - |  713 | `/*` |
|       - |  714 | ` * Insert a node in the given hashmap.` |
|       - |  715 | ` * If a node with the given key already exists in the database` |
|       - |  716 | ` * then this function overwrite the old value.` |
|       - |  717 | ` */` |
|     418 |  718 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       1 |  719 |  |
|       - |  720 | `	ph7_value *pObj;` |
|       - |  721 | `	sxi32 rc;` |
|       - |  722 | `	/* Extract the node value */` |
|     419 |  723 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     419 |  724 | `	if( pObj == 0 ){` |
|     ! 0 |  725 | `		return SXERR_EMPTY;` |
|       - |  726 | `	}` |
|       - |  727 | `	/* Preserve key */` |
|     419 |  728 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  729 | `		/* Int64 key */` |
|     289 |  730 | `		if( !bPreserve ){` |
|       - |  731 | `			/* Assign an automatic index */` |
|     149 |  732 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      75 |  733 | `		}else{` |
|     141 |  734 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  735 | `		}` |
|     145 |  736 | `	}else{` |
|       - |  737 | `		/* Blob key */` |
|     131 |  738 | `		if( !bPreserve ){` |
|       - |  739 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  740 | `			 * original string key entirely */` |
|      35 |  741 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      18 |  742 | `		}else{` |
|     145 |  743 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      48 |  744 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  745 | `		}` |
|       - |  746 | `	}` |
|     419 |  747 | `	return rc;` |
|     210 |  748 |  |
|       - |  749 | `/*` |
|       - |  750 | ` * Compare two node values.` |
|       - |  751 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  752 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  753 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  754 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  755 | ` * documenation.` |
|       - |  756 | ` */` |
|   38072 |  757 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  758 |  |
|       - |  759 | `	ph7_value sObj1,sObj2;` |
|       - |  760 | `	sxi32 rc;` |
|   38074 |  761 | `	if( pLeft == pRight ){` |
|       - |  762 | `		/*` |
|       - |  763 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  764 | `		 * below for more information on this sceanario.` |
|       - |  765 | `		 */` |
|     ! 0 |  766 | `		return 0;` |
|       - |  767 | `	}` |
|       - |  768 | `	/* Do the comparison */` |
|   38074 |  769 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   38074 |  770 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   38074 |  771 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   38074 |  772 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   38074 |  773 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   38074 |  774 | `	PH7_MemObjRelease(&sObj1);` |
|   38074 |  775 | `	PH7_MemObjRelease(&sObj2);` |
|   38074 |  776 | `	return rc;` |
|   19076 |  777 |  |
|       - |  778 | `/*` |
|       - |  779 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  780 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  781 | ` */` |
|    8314 |  782 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  783 |  |
|    8316 |  784 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  785 | `	sxu32 nBucket;` |
|       - |  786 | `	/* Remove old collision links */` |
|    8316 |  787 | `	if( pEntry->pPrevCollide ){` |
|    6621 |  788 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    3302 |  789 | `	}else{` |
|    1697 |  790 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  791 | `	}` |
|    8316 |  792 | `	if( pEntry->pNextCollide ){` |
|     653 |  793 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     325 |  794 | `	}` |
|    8316 |  795 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  796 | `	/* Compute the new hash */` |
|    8316 |  797 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    8316 |  798 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    8316 |  799 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  800 | `	/* Link to the new bucket */` |
|    8316 |  801 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    8316 |  802 | `	if( pMap->apBucket[nBucket] ){` |
|    6793 |  803 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3393 |  804 | `	}` |
|    8316 |  805 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    8316 |  806 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  807 | `	/* Increment the automatic index */` |
|    8316 |  808 | `	pMap->iNextIdx++;` |
|    8316 |  809 |  |
|       - |  810 | `/*` |
|       - |  811 | ` * Perform a linear search on a given hashmap.` |
|       - |  812 | ` * Write a pointer to the target node on success.` |
|       - |  813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  814 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  815 | ` * for more information.` |
|       - |  816 | ` */` |
|   21074 |  817 | `static int HashmapFindValue(` |
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
|   21076 |  830 | `	pEntry = pMap->pFirst;` |
|   21076 |  831 | `	n = pMap->nEntry;` |
|   21076 |  832 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   21076 |  833 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   50437 |  834 | `	for(;;){` |
|  100878 |  835 | `		if( n < 1 ){` |
|      99 |  836 | `			break;` |
|       - |  837 | `		}` |
|       - |  838 | `		/* Extract node value */` |
|  100780 |  839 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  100780 |  840 | `		if( pVal ){` |
|  100780 |  841 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  100780 |  853 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  100780 |  854 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  100780 |  855 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  100780 |  856 | `				PH7_MemObjRelease(&sVal);` |
|  100780 |  857 | `				PH7_MemObjRelease(&sNeedle);` |
|  100780 |  858 | `				if( rc == 0 ){` |
|   20978 |  859 | `					if( ppNode ){` |
|      23 |  860 | `						*ppNode = pEntry;` |
|      11 |  861 | `					}` |
|       - |  862 | `					/* Match found*/` |
|   20978 |  863 | `					return SXRET_OK;` |
|       - |  864 | `				}` |
|       - |  865 | `			}` |
|   39900 |  866 | `		}` |
|       - |  867 | `		/* Point to the next entry */` |
|   79804 |  868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   79804 |  869 | `		n--;` |
|       2 |  870 | `	}` |
|       - |  871 | `	/* No such entry */` |
|      99 |  872 | `	return SXERR_NOTFOUND;` |
|   10539 |  873 |  |
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
|  440660 | 1047 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1048 | `	ph7_hashmap *pDest,` |
|       - | 1049 | `	ph7_hashmap_node *pEntry,` |
|       - | 1050 | `	ph7_value *pVal,` |
|       - | 1051 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1052 | `	)` |
|       2 | 1053 |  |
|  440662 | 1054 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1055 | `	ph7_value sKey;` |
|       - | 1056 | `	sxi32 rc;` |
|       - | 1057 |  |
|  440662 | 1058 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1059 | `		/* Blob key insertion */` |
|      41 | 1060 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      41 | 1061 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      41 | 1062 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      41 | 1063 | `		PH7_MemObjRelease(&sKey);` |
|      21 | 1064 | `	}else{` |
|       - | 1065 | `		/* Int key */` |
|  440622 | 1066 | `		if( iAction == 0 ){ /* Merge */` |
|  440550 | 1067 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  220348 | 1068 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1069 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1070 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1071 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1072 | `		}else{ /* Dup */` |
|      44 | 1073 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1074 | `		}` |
|       - | 1075 | `	}` |
|  440662 | 1076 | `	return rc;` |
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
|    1732 | 1090 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1091 |  |
|       - | 1092 | `	ph7_hashmap_node *pEntry;` |
|       - | 1093 | `	ph7_value *pVal;` |
|       - | 1094 | `	sxi32 rc;` |
|       - | 1095 | `	sxu32 n;` |
|    1734 | 1096 | `	if( pSrc == pDest ){` |
|       - | 1097 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1098 | `		 * Unlike the zend engine.` |
|       - | 1099 | `		 */` |
|     ! 0 | 1100 | `		return SXRET_OK;` |
|       - | 1101 | `	}` |
|       - | 1102 | `	/* Point to the first inserted entry in the source */` |
|    1734 | 1103 | `	pEntry = pSrc->pFirst;` |
|       - | 1104 | `	/* Perform the merge */` |
|  442298 | 1105 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1106 | `		/* Extract the node value */` |
|  440566 | 1107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  440566 | 1108 | `		if( pVal ){` |
|       - | 1109 | `			/* Make a local copy of the value.` |
|       - | 1110 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1111 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1112 | `			 * to the old pool.` |
|       - | 1113 | `			 */` |
|  440566 | 1114 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  220284 | 1115 | `		}else{` |
|     ! 0 | 1116 | `			rc = SXRET_OK;` |
|       - | 1117 | `		}` |
|  440566 | 1118 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1119 | `			return rc;` |
|       - | 1120 | `		}` |
|       - | 1121 | `		/* Point to the next entry */` |
|  440566 | 1122 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  220284 | 1123 | `	}` |
|    1734 | 1124 | `	return SXRET_OK;` |
|     868 | 1125 |  |
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
|      34 | 1140 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1141 |  |
|       - | 1142 | `	ph7_hashmap_node *pEntry;` |
|       - | 1143 | `	ph7_value *pVal;` |
|       - | 1144 | `	sxi32 rc;` |
|       - | 1145 | `	sxu32 n;` |
|      36 | 1146 | `	if( pSrc == pDest ){` |
|       - | 1147 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1148 | `		 * Unlike the zend engine.` |
|       - | 1149 | `		 */` |
|     ! 0 | 1150 | `		return SXRET_OK;` |
|       - | 1151 | `	}` |
|       - | 1152 | `	/* Point to the first inserted entry in the source */` |
|      36 | 1153 | `	pEntry = pSrc->pFirst;` |
|       - | 1154 | `	/* Perform the merge */` |
|      80 | 1155 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1156 | `		/* Extract the node value */` |
|      46 | 1157 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      46 | 1158 | `		if( pVal ){` |
|      46 | 1159 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      24 | 1160 | `		}else{` |
|     ! 0 | 1161 | `			rc = SXRET_OK;` |
|       - | 1162 | `		}` |
|      46 | 1163 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1164 | `			return rc;` |
|       - | 1165 | `		}` |
|       - | 1166 | `		/* Point to the next entry */` |
|      46 | 1167 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 1168 | `	}` |
|      36 | 1169 | `	return SXRET_OK;` |
|      19 | 1170 |  |
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
|   56922 | 1297 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1298 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1299 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1300 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1301 | `	)` |
|       2 | 1302 |  |
|       - | 1303 | `	ph7_hashmap *pMap;` |
|       - | 1304 | `	/* Allocate a new instance */` |
|   56924 | 1305 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   56924 | 1306 | `	if( pMap == 0 ){` |
|     ! 0 | 1307 | `		return 0;` |
|       - | 1308 | `	}` |
|       - | 1309 | `	/* Zero the structure */` |
|   56924 | 1310 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1311 | `	/* Fill in the structure */` |
|   56924 | 1312 | `	pMap->pVm = &(*pVm);` |
|   56924 | 1313 | `	pMap->iRef = 1;` |
|       - | 1314 | `	/* Default hash functions */` |
|   56924 | 1315 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   56924 | 1316 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   56924 | 1317 | `	return pMap;` |
|   28463 | 1318 |  |
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
|    1620 | 1339 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    1622 | 1359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1622 | 1360 | `	if( pMap == 0 ){` |
|     ! 0 | 1361 | `		return SXERR_MEM;` |
|       - | 1362 | `	}` |
|    1622 | 1363 | `	pVm->pGlobal = pMap;` |
|       - | 1364 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1622 | 1365 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1622 | 1366 | `	if( pObj == 0 ){` |
|     ! 0 | 1367 | `		return SXERR_MEM;` |
|       - | 1368 | `	}` |
|    1622 | 1369 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1370 | `	/* Record object index */` |
|    1622 | 1371 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1372 | `	/* Install the special $GLOBALS array */` |
|    1622 | 1373 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1622 | 1374 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1375 | `		return rc;` |
|       - | 1376 | `	}` |
|       - | 1377 | `	/* Install superglobals now */` |
|   17822 | 1378 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1379 | `		ph7_value *pSuper;` |
|       - | 1380 | `		/* Request an empty array */` |
|   16202 | 1381 | `		pSuper = ph7_new_array(&(*pVm));` |
|   16202 | 1382 | `		if( pSuper == 0 ){` |
|     ! 0 | 1383 | `			return SXERR_MEM;` |
|       - | 1384 | `		}` |
|       - | 1385 | `		/* Install */` |
|   16202 | 1386 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   16202 | 1387 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1388 | `			return rc;` |
|       - | 1389 | `		}` |
|       - | 1390 | `		/* Release the value now it have been installed */` |
|   16202 | 1391 | `		ph7_release_value(&(*pVm),pSuper);` |
|    8102 | 1392 | `	}` |
|       - | 1393 | `	/* Set some $_SERVER entries */` |
|    1622 | 1394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1395 | `	/*` |
|       - | 1396 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1397 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1398 | `	 */` |
|    3238 | 1399 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1400 | `		"SCRIPT_FILENAME",` |
|     810 | 1401 | `		pFile ? pFile->zString : ":Memory:",` |
|    1616 | 1402 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1403 | `		);` |
|       - | 1404 | `	/* All done,all super-global are installed now */` |
|    1622 | 1405 | `	return SXRET_OK;` |
|     812 | 1406 |  |
|       - | 1407 | `/*` |
|       - | 1408 | ` * Release a hashmap.` |
|       - | 1409 | ` */` |
|   39028 | 1410 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1411 |  |
|       - | 1412 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   39030 | 1413 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1414 | `	sxu32 n;` |
|   39030 | 1415 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1416 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1417 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1418 | `		return SXRET_OK;` |
|       - | 1419 | `	}` |
|       - | 1420 | `	/* Start the release process */` |
|   39030 | 1421 | `	n = 0;` |
|   39030 | 1422 | `	pEntry = pMap->pFirst;` |
| 1433971 | 1423 | `	for(;;){` |
| 2867944 | 1424 | `		if( n >= pMap->nEntry ){` |
|   39030 | 1425 | `			break;` |
|       - | 1426 | `		}` |
| 2828916 | 1427 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1428 | `		/* Remove the reference from the foreign table */` |
| 2828916 | 1429 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2828916 | 1430 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1431 | `			/* Restore the ph7_value to the free list */` |
| 2828908 | 1432 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1414453 | 1433 | `		}` |
|       - | 1434 | `		/* Release the node */` |
| 2828916 | 1435 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   57474 | 1436 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   28736 | 1437 | `		}` |
| 2828916 | 1438 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1439 | `		/* Point to the next entry */` |
| 2828916 | 1440 | `		pEntry = pNext;` |
| 2828916 | 1441 | `		n++;` |
|       2 | 1442 | `	}` |
|   39030 | 1443 | `	if( pMap->nEntry > 0 ){` |
|       - | 1444 | `		/* Release the hash bucket */` |
|   34734 | 1445 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   17366 | 1446 | `	}` |
|   39030 | 1447 | `	if( FreeDS ){` |
|       - | 1448 | `		/* Free the whole instance */` |
|   39014 | 1449 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   19508 | 1450 | `	}else{` |
|       - | 1451 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1452 | `		pMap->apBucket = 0;` |
|      17 | 1453 | `		pMap->iNextIdx = 0;` |
|      17 | 1454 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1455 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1456 | `	}` |
|   39030 | 1457 | `	return SXRET_OK;` |
|   19516 | 1458 |  |
|       - | 1459 | `/*` |
|       - | 1460 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1461 | ` * If the count reaches zero which mean no more variables` |
|       - | 1462 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1463 | ` */` |
|  445732 | 1464 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1465 |  |
|  445734 | 1466 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1467 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  445734 | 1468 | `	pMap->iRef--;` |
|  445734 | 1469 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   39014 | 1470 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   19506 | 1471 | `	}` |
|  445734 | 1472 |  |
|       - | 1473 | `/*` |
|       - | 1474 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1475 | ` * Write a pointer to the target node on success.` |
|       - | 1476 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1477 | ` */` |
|   80476 | 1478 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1479 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1480 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1481 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1482 | `	)` |
|       2 | 1483 |  |
|       - | 1484 | `	sxi32 rc;` |
|   80478 | 1485 | `	if( pMap->nEntry < 1 ){` |
|       - | 1486 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1487 | `		 */` |
|       9 | 1488 | `		return SXERR_NOTFOUND;` |
|       - | 1489 | `	}` |
|   80470 | 1490 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   80470 | 1491 | `	return rc;` |
|   40240 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1495 | ` * hashmap.` |
|       - | 1496 | ` * If a node with the given key already exists in the database` |
|       - | 1497 | ` * then this function overwrite the old value.` |
|       - | 1498 | ` */` |
| 2394402 | 1499 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1500 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1501 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1502 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1503 | `	)` |
|       2 | 1504 |  |
|       - | 1505 | `	sxi32 rc;` |
| 2394404 | 1506 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1507 | `		/*` |
|       - | 1508 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1509 | `		 */` |
|     ! 0 | 1510 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1511 | `		return SXRET_OK;` |
|       - | 1512 | `	}` |
| 2394404 | 1513 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2394404 | 1514 | `	return rc;` |
| 1197203 | 1515 |  |
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
|   21750 | 1543 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1544 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1545 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1546 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1547 | `	)` |
|       2 | 1548 |  |
|       - | 1549 | `	sxi32 rc;` |
|   21752 | 1550 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1551 | `		/*` |
|       - | 1552 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1553 | `		 */` |
|     ! 0 | 1554 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1555 | `		return SXRET_OK;` |
|       - | 1556 | `	}` |
|   21752 | 1557 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   21752 | 1558 | `	return rc;` |
|   10877 | 1559 |  |
|       - | 1560 | `/*` |
|       - | 1561 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1562 | ` */` |
|   17272 | 1563 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1564 |  |
|       - | 1565 | `	/* Reset the loop cursor */` |
|   17274 | 1566 | `	pMap->pCur = pMap->pFirst;` |
|   17274 | 1567 |  |
|       - | 1568 | `/*` |
|       - | 1569 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1570 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1571 | ` * return NULL.` |
|       - | 1572 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1573 | ` */` |
|  140450 | 1574 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1575 |  |
|  140452 | 1576 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  140452 | 1577 | `	if( pCur == 0 ){` |
|       - | 1578 | `		/* End of the list,return null */` |
|    8658 | 1579 | `		return 0;` |
|       - | 1580 | `	}` |
|       - | 1581 | `	/* Advance the node cursor */` |
|  131796 | 1582 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  131796 | 1583 | `	return pCur;` |
|   70227 | 1584 |  |
|       - | 1585 | `/*` |
|       - | 1586 | ` * Extract a node value.` |
|       - | 1587 | ` */` |
|  333420 | 1588 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1589 |  |
|  333422 | 1590 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  333422 | 1591 | `	if( pEntry ){` |
|  333422 | 1592 | `		if( bStore ){` |
|  131824 | 1593 | `			PH7_MemObjStore(pEntry,pValue);` |
|   65913 | 1594 | `		}else{` |
|  201600 | 1595 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1596 | `		}` |
|  166788 | 1597 | `	}else{` |
|     ! 0 | 1598 | `		PH7_MemObjRelease(pValue);` |
|       - | 1599 | `	}` |
|  333422 | 1600 |  |
|       - | 1601 | `/*` |
|       - | 1602 | ` * Extract a node key.` |
|       - | 1603 | ` */` |
|   88586 | 1604 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1605 |  |
|       - | 1606 | `	/* Fill with the current key */` |
|   88588 | 1607 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   88432 | 1608 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      13 | 1609 | `			SyBlobRelease(&pKey->sBlob);` |
|       6 | 1610 | `		}` |
|   88432 | 1611 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   88432 | 1612 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   44217 | 1613 | `	}else{` |
|     157 | 1614 | `		SyBlobReset(&pKey->sBlob);` |
|     157 | 1615 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     157 | 1616 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1617 | `	}` |
|   88588 | 1618 |  |
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
|   24308 | 1666 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1667 |  |
|       - | 1668 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1669 | `    /* Prevent compiler warning */` |
|   24310 | 1670 | `	result.pNext = result.pPrev = 0;` |
|   24310 | 1671 | `	pTail = &result;` |
|   62427 | 1672 | `	while( pA && pB ){` |
|   38119 | 1673 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|   25141 | 1674 | `			pTail->pPrev = pA;` |
|   25141 | 1675 | `			pA->pNext = pTail;` |
|   25141 | 1676 | `			pTail = pA;` |
|   25141 | 1677 | `			pA = pA->pPrev;` |
|   12571 | 1678 | `		}else{` |
|   12980 | 1679 | `			pTail->pPrev = pB;` |
|   12980 | 1680 | `			pB->pNext = pTail;` |
|   12980 | 1681 | `			pTail = pB;` |
|   12980 | 1682 | `			pB = pB->pPrev;` |
|       - | 1683 | `		}` |
|       2 | 1684 | `	}` |
|   24310 | 1685 | `	if( pA ){` |
|   18017 | 1686 | `		pTail->pPrev = pA;` |
|   18017 | 1687 | `		pA->pNext = pTail;` |
|   15317 | 1688 | `	}else if( pB ){` |
|    6137 | 1689 | `		pTail->pPrev = pB;` |
|    6137 | 1690 | `		pB->pNext = pTail;` |
|    3055 | 1691 | `	}else{` |
|     160 | 1692 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1693 | `	}` |
|   24310 | 1694 | `	return result.pPrev;` |
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
|     546 | 1708 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1709 |  |
|       - | 1710 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1711 | `	sxu32 i;` |
|     548 | 1712 | `	SyZero(a,sizeof(a));` |
|       - | 1713 | `	/* Point to the first inserted entry */` |
|     548 | 1714 | `	pIn = pMap->pFirst;` |
|    8866 | 1715 | `	while( pIn ){` |
|    8320 | 1716 | `		p = pIn;` |
|    8320 | 1717 | `		pIn = p->pPrev;` |
|    8320 | 1718 | `		p->pPrev = 0;` |
|   15702 | 1719 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   15702 | 1720 | `			if( a[i]==0 ){` |
|    8320 | 1721 | `				a[i] = p;` |
|    8320 | 1722 | `				break;` |
|     ! 0 | 1723 | `			}else{` |
|    7384 | 1724 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    7384 | 1725 | `				a[i] = 0;` |
|       - | 1726 | `			}` |
|    3693 | 1727 | `		}` |
|    8320 | 1728 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1729 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1730 | `			 * But that is impossible.` |
|       - | 1731 | `			 */` |
|     ! 0 | 1732 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1733 | `		}` |
|       2 | 1734 | `	}` |
|     548 | 1735 | `	p = a[0];` |
|   17474 | 1736 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   16928 | 1737 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    8465 | 1738 | `	}` |
|     548 | 1739 | `	p->pNext = 0;` |
|       - | 1740 | `	/* Reflect the change */` |
|     548 | 1741 | `	pMap->pFirst = p;` |
|       - | 1742 | `	/* Reset the loop cursor */` |
|     548 | 1743 | `	pMap->pCur = pMap->pFirst;` |
|     548 | 1744 | `	return SXRET_OK;` |
|       2 | 1745 |  |
|       - | 1746 | `/*` |
|       - | 1747 | ` * Node comparison callback.` |
|       - | 1748 | ` * used-by: [sort(),asort(),...]` |
|       - | 1749 | ` */` |
|   38054 | 1750 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1751 |  |
|       - | 1752 | `	ph7_value sA,sB;` |
|       - | 1753 | `	sxi32 iFlags;` |
|       - | 1754 | `	int rc;` |
|   38056 | 1755 | `	if( pCmpData == 0 ){` |
|       - | 1756 | `		/* Perform a standard comparison */` |
|   38052 | 1757 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   38052 | 1758 | `		return rc;` |
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
|   19067 | 1784 |  |
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
|     530 | 2006 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2007 |  |
|       - | 2008 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2009 | `	sxu32 i;` |
|       - | 2010 | `	/* Rehash all entries */` |
|     532 | 2011 | `	pLast = p = pMap->pFirst;` |
|     532 | 2012 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     532 | 2013 | `	i = 0;` |
|    4397 | 2014 | `	for( ;; ){` |
|    8796 | 2015 | `		if( i >= pMap->nEntry ){` |
|     532 | 2016 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     532 | 2017 | `			break;` |
|       - | 2018 | `		}` |
|    8266 | 2019 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2020 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2021 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2022 | `			/* Change key type */` |
|       5 | 2023 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2024 | `		}` |
|    8266 | 2025 | `		HashmapRehashIntNode(p);` |
|       - | 2026 | `		/* Point to the next entry */` |
|    8266 | 2027 | `		i++;` |
|    8266 | 2028 | `		pLast = p;` |
|    8266 | 2029 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2030 | `	}` |
|     532 | 2031 |  |
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
|     838 | 2053 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2054 |  |
|       - | 2055 | `	ph7_hashmap *pMap;` |
|       - | 2056 | `	/* Make sure we are dealing with a valid hashmap */` |
|     840 | 2057 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2060 | `		return PH7_OK;` |
|       - | 2061 | `	}` |
|       - | 2062 | `	/* Point to the internal representation of the input hashmap */` |
|     840 | 2063 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     840 | 2064 | `	if( pMap->nEntry > 1 ){` |
|     526 | 2065 | `		sxi32 iCmpFlags = 0;` |
|     526 | 2066 | `		if( nArg > 1 ){` |
|       - | 2067 | `			/* Extract comparison flags */` |
|       3 | 2068 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2069 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2070 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2071 | `			}` |
|       1 | 2072 | `		}` |
|       - | 2073 | `		/* Do the merge sort */` |
|     526 | 2074 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2075 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     526 | 2076 | `		HashmapSortRehash(pMap);` |
|     262 | 2077 | `	}` |
|       - | 2078 | `	/* All done,return TRUE */` |
|     840 | 2079 | `	ph7_result_bool(pCtx,1);` |
|     840 | 2080 | `	return PH7_OK;` |
|     421 | 2081 |  |
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
|     604 | 2498 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2499 |  |
|     606 | 2500 | `	int bRecursive = FALSE;` |
|       - | 2501 | `	sxi64 iCount;` |
|     606 | 2502 | `	if( nArg < 1 ){` |
|       - | 2503 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 2504 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 2505 | `		return PH7_OK;` |
|       - | 2506 | `	}` |
|     606 | 2507 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2508 | `		/* TICKET 1433-19: Handle objects */` |
|       3 | 2509 | `		int res = !ph7_value_is_null(apArg[0]);` |
|       3 | 2510 | `		ph7_result_int(pCtx,res);` |
|       3 | 2511 | `		return PH7_OK;` |
|       - | 2512 | `	}` |
|     604 | 2513 | `	if( nArg > 1 ){` |
|       - | 2514 | `		/* Recursive count? */` |
|      31 | 2515 | `		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;` |
|      15 | 2516 | `	}` |
|       - | 2517 | `	/* Count */` |
|     604 | 2518 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);` |
|     604 | 2519 | `	ph7_result_int64(pCtx,iCount);` |
|     604 | 2520 | `	return PH7_OK;` |
|     304 | 2521 |  |
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
|      30 | 3111 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3112 |  |
|       - | 3113 | `	ph7_hashmap_node *pNode;` |
|       - | 3114 | `	ph7_hashmap *pMap;` |
|       - | 3115 | `	ph7_value *pArray;` |
|       - | 3116 | `	ph7_value *pObj;` |
|       - | 3117 | `	sxu32 n;` |
|      32 | 3118 | `	if( nArg != 1 ){` |
|       - | 3119 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3120 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3121 | `			"ArgumentCountError",` |
|       - | 3122 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3123 | `			nArg` |
|       - | 3124 | `			);` |
|       - | 3125 | `	}` |
|       - | 3126 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3127 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3128 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3129 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3130 | `			"TypeError",` |
|       - | 3131 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3132 | `			ph7_type_name(apArg[0])` |
|       - | 3133 | `			);` |
|       - | 3134 | `	}` |
|       - | 3135 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3136 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3137 | `	/* Create a new array */` |
|      25 | 3138 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3139 | `	if( pArray == 0 ){` |
|     ! 0 | 3140 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3141 | `		return PH7_OK;` |
|       - | 3142 | `	}` |
|       - | 3143 | `	/* Perform the requested operation */` |
|      25 | 3144 | `	pNode = pMap->pFirst;` |
|      83 | 3145 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3146 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3147 | `		if( pObj ){` |
|       - | 3148 | `			/* perform the insertion */` |
|      59 | 3149 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3150 | `		}` |
|       - | 3151 | `		/* Point to the next entry */` |
|      59 | 3152 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3153 | `	}` |
|       - | 3154 | `	/* return the new array */` |
|      25 | 3155 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3156 | `	return PH7_OK;` |
|      17 | 3157 |  |
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
|     120 | 3171 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|     122 | 3182 | `	if( nArg < 1 ){` |
|       - | 3183 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3184 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3185 | `			"ArgumentCountError",` |
|       - | 3186 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3187 | `			);` |
|       - | 3188 | `	}` |
|       - | 3189 | `	/* Make sure we are dealing with a valid hashmap */` |
|     120 | 3190 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3191 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3192 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3193 | `			"TypeError",` |
|       - | 3194 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3195 | `			ph7_type_name(apArg[0])` |
|       - | 3196 | `			);` |
|       - | 3197 | `	}` |
|       - | 3198 | `	/* Point to the internal representation of the input hashmap */` |
|     118 | 3199 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3200 | `	/* Create a new array */` |
|     118 | 3201 | `	pArray = ph7_context_new_array(pCtx);` |
|     118 | 3202 | `	if( pArray == 0 ){` |
|     ! 0 | 3203 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3204 | `		return PH7_OK;` |
|       - | 3205 | `	}` |
|     118 | 3206 | `	bStrict = FALSE;` |
|     118 | 3207 | `	if( nArg > 2 ){` |
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
|     115 | 3219 | `	pNode = pMap->pFirst;` |
|     115 | 3220 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     553 | 3221 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     439 | 3222 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     117 | 3223 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      59 | 3224 | `		}else{` |
|     323 | 3225 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3226 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3227 | `		}` |
|     439 | 3228 | `		rc = 0;` |
|     439 | 3229 | `		if( nArg > 1 ){` |
|      31 | 3230 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3231 | `			if( pValue ){` |
|      31 | 3232 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3233 | `				/* Filter key */` |
|      31 | 3234 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3235 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3236 | `			}` |
|      15 | 3237 | `		}` |
|     439 | 3238 | `		if( rc == 0 ){` |
|       - | 3239 | `			/* Perform the insertion */` |
|     421 | 3240 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     210 | 3241 | `		}` |
|     439 | 3242 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3243 | `		/* Point to the next entry */` |
|     439 | 3244 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     220 | 3245 | `	}` |
|       - | 3246 | `	/* return the new array */` |
|     115 | 3247 | `	ph7_result_value(pCtx,pArray);` |
|     115 | 3248 | `	return PH7_OK;` |
|      62 | 3249 |  |
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
|     866 | 3293 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3294 |  |
|       - | 3295 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3296 | `	ph7_value *pArray;` |
|       - | 3297 | `	int i;` |
|       - | 3298 | `	/* Create a new array */` |
|     868 | 3299 | `	pArray = ph7_context_new_array(pCtx);` |
|     868 | 3300 | `	if( pArray == 0 ){` |
|     ! 0 | 3301 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3302 | `		return PH7_OK;` |
|       - | 3303 | `	}` |
|       - | 3304 | `	/* Point to the internal representation of the hashmap */` |
|     868 | 3305 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3306 | `	/* Start merging */` |
|    2590 | 3307 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3308 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1728 | 3309 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3310 | `			/* Type mismatch -> TypeError */` |
|       7 | 3311 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3312 | `				"TypeError",` |
|       - | 3313 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3314 | `				i + 1,` |
|       4 | 3315 | `				ph7_type_name(apArg[i])` |
|       - | 3316 | `				);` |
|     ! 0 | 3317 | `		}else{` |
|    1724 | 3318 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3319 | `			/* Merge the two hashmaps */` |
|    1724 | 3320 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3321 | `		}` |
|     863 | 3322 | `	}` |
|       - | 3323 | `	/* Return the freshly created array */` |
|     864 | 3324 | `	ph7_result_value(pCtx,pArray);` |
|     864 | 3325 | `	return PH7_OK;` |
|     435 | 3326 |  |
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
|       - | 3394 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3395 | ` *  Extract a slice of the array.` |
|       - | 3396 | ` * Parameters` |
|       - | 3397 | ` *  $array` |
|       - | 3398 | ` *    The input array.` |
|       - | 3399 | ` * $offset` |
|       - | 3400 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3401 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3402 | ` * $length (optional, nullable)` |
|       - | 3403 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3404 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3405 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3406 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3407 | ` * $preserve_keys (optional)` |
|       - | 3408 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3409 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3410 | ` * Return` |
|       - | 3411 | ` *   The new slice.` |
|       - | 3412 | ` */` |
|      46 | 3413 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3414 |  |
|       - | 3415 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3416 | `	ph7_hashmap_node *pCur;` |
|       - | 3417 | `	ph7_value *pArray;` |
|       - | 3418 | `	int iLength,iOfft;` |
|       - | 3419 | `	int bPreserve;` |
|       - | 3420 | `	sxi32 rc;` |
|      48 | 3421 | `	if( nArg < 2 ){` |
|       7 | 3422 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3423 | `			"ArgumentCountError",` |
|       - | 3424 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3425 | `			nArg` |
|       - | 3426 | `			);` |
|       - | 3427 | `	}` |
|      44 | 3428 | `	if( nArg > 4 ){` |
|       4 | 3429 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3430 | `			"ArgumentCountError",` |
|       - | 3431 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3432 | `			nArg` |
|       - | 3433 | `			);` |
|       - | 3434 | `	}` |
|      42 | 3435 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3436 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3437 | `			"TypeError",` |
|       - | 3438 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3439 | `			ph7_type_name(apArg[0])` |
|       - | 3440 | `			);` |
|       - | 3441 | `	}` |
|       - | 3442 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3443 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3444 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3445 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3446 | `			"TypeError",` |
|       - | 3447 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3448 | `			ph7_type_name(apArg[1])` |
|       - | 3449 | `			);` |
|       - | 3450 | `	}` |
|       - | 3451 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3452 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3453 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3454 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3455 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3456 | `				"TypeError",` |
|       - | 3457 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3458 | `				ph7_type_name(apArg[2])` |
|       - | 3459 | `				);` |
|       - | 3460 | `		}` |
|       8 | 3461 | `	}` |
|       - | 3462 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3463 | `	if( nArg > 3 ){` |
|      10 | 3464 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3465 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3466 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3467 | `				"TypeError",` |
|       - | 3468 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3469 | `				ph7_type_name(apArg[3])` |
|       - | 3470 | `				);` |
|       - | 3471 | `		}` |
|       2 | 3472 | `	}` |
|       - | 3473 | `	/* Point the internal representation of the target array */` |
|      33 | 3474 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3475 | `	bPreserve = FALSE;` |
|       - | 3476 | `	/* Get the offset */` |
|      33 | 3477 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3478 | `	if( iOfft < 0 ){` |
|       5 | 3479 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3480 | `		if( iOfft < 0 ){` |
|       3 | 3481 | `			iOfft = 0;` |
|       1 | 3482 | `		}` |
|       2 | 3483 | `	}` |
|      33 | 3484 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3485 | `		/* Offset past end of array, return empty array */` |
|       5 | 3486 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3487 | `		if( pArray == 0 ){` |
|     ! 0 | 3488 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3489 | `			return PH7_OK;` |
|       - | 3490 | `		}` |
|       5 | 3491 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3492 | `		return PH7_OK;` |
|       - | 3493 | `	}` |
|       - | 3494 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3495 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3496 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3497 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3498 | `		if( iLength < 0 ){` |
|       5 | 3499 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3500 | `		}` |
|      15 | 3501 | `		if( iLength < 0 ){` |
|       3 | 3502 | `			iLength = 0;` |
|       1 | 3503 | `		}` |
|      15 | 3504 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3505 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3506 | `		}` |
|       7 | 3507 | `	}` |
|      29 | 3508 | `	if( nArg > 3 ){` |
|       5 | 3509 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3510 | `	}` |
|       - | 3511 | `	/* Create a new array */` |
|      29 | 3512 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3513 | `	if( pArray == 0 ){` |
|     ! 0 | 3514 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3515 | `		return PH7_OK;` |
|       - | 3516 | `	}` |
|      29 | 3517 | `	if( iLength < 1 ){` |
|       - | 3518 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3519 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3520 | `		return PH7_OK;` |
|       - | 3521 | `	}` |
|       - | 3522 | `	/* Point to the desired entry */` |
|      25 | 3523 | `	pCur = pSrc->pFirst;` |
|      24 | 3524 | `	for(;;){` |
|      49 | 3525 | `		if( iOfft < 1 ){` |
|      25 | 3526 | `			break;` |
|       - | 3527 | `		}` |
|       - | 3528 | `		/* Point to the next entry */` |
|      25 | 3529 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3530 | `		iOfft--;` |
|       1 | 3531 | `	}` |
|       - | 3532 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3533 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3534 | `	for(;;){` |
|      79 | 3535 | `		if( iLength < 1 ){` |
|      25 | 3536 | `			break;` |
|       - | 3537 | `		}` |
|       - | 3538 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3539 | `		{` |
|      55 | 3540 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3541 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3542 | `		}` |
|      55 | 3543 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3544 | `			break;` |
|       - | 3545 | `		}` |
|       - | 3546 | `		/* Point to the next entry */` |
|      55 | 3547 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3548 | `		iLength--;` |
|       1 | 3549 | `	}` |
|       - | 3550 | `	/* Return the freshly created array */` |
|      25 | 3551 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3552 | `	return PH7_OK;` |
|      25 | 3553 |  |
|       - | 3554 | `/*` |
|       - | 3555 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3556 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3557 | ` * beginning (becomes the new pFirst).` |
|       - | 3558 | ` */` |
|      30 | 3559 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3560 |  |
|       - | 3561 | `	ph7_hashmap_node *pNode;` |
|       - | 3562 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3563 | `	pNode = pMap->pLast;` |
|      31 | 3564 | `	if( pNode == 0 ){` |
|     ! 0 | 3565 | `		return;` |
|       - | 3566 | `	}` |
|      31 | 3567 | `	if( pNode->pNext == 0 ){` |
|       - | 3568 | `		/* Only node in the list, nothing to move */` |
|       5 | 3569 | `		return;` |
|       - | 3570 | `	}` |
|      27 | 3571 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3572 | `		/* Already in the correct position */` |
|       9 | 3573 | `		return;` |
|       - | 3574 | `	}` |
|       - | 3575 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3576 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3577 | `	pMap->pLast->pPrev = 0;` |
|       - | 3578 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3579 | `	if( pAfter == 0 ){` |
|       - | 3580 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3581 | `		pNode->pNext = 0;` |
|       3 | 3582 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3583 | `		if( pMap->pFirst ){` |
|       3 | 3584 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3585 | `		}` |
|       3 | 3586 | `		pMap->pFirst = pNode;` |
|       2 | 3587 | `	}else{` |
|      17 | 3588 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3589 | `		pNode->pPrev = pOldNext;` |
|      17 | 3590 | `		pNode->pNext = pAfter;` |
|      17 | 3591 | `		pAfter->pPrev = pNode;` |
|      17 | 3592 | `		if( pOldNext ){` |
|      17 | 3593 | `			pOldNext->pNext = pNode;` |
|       9 | 3594 | `		}else{` |
|     ! 0 | 3595 | `			pMap->pLast = pNode;` |
|       - | 3596 | `		}` |
|       - | 3597 | `	}` |
|      16 | 3598 |  |
|       - | 3599 | `/*` |
|       - | 3600 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3601 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3602 | ` * Parameters` |
|       - | 3603 | ` *  $array` |
|       - | 3604 | ` *    The input array.` |
|       - | 3605 | ` *  $offset` |
|       - | 3606 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3607 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3608 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3609 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3610 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3611 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3612 | ` *  $length (optional)` |
|       - | 3613 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3614 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3615 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3616 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3617 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3618 | ` *  $replacement (optional)` |
|       - | 3619 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3620 | ` *    with elements from this array.` |
|       - | 3621 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3622 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3623 | ` *    offset.` |
|       - | 3624 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3625 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3626 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3627 | ` * Return` |
|       - | 3628 | ` *   A new array consisting of the extracted elements.` |
|       - | 3629 | ` */` |
|      54 | 3630 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3631 |  |
|       - | 3632 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3633 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3634 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3635 | `	int iLength,iOfft,i;` |
|       - | 3636 | `	sxi32 rc;` |
|      56 | 3637 | `	if( nArg < 2 ){` |
|       7 | 3638 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3639 | `			"ArgumentCountError",` |
|       - | 3640 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3641 | `			nArg` |
|       - | 3642 | `			);` |
|       - | 3643 | `	}` |
|      52 | 3644 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3645 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3646 | `			"TypeError",` |
|       - | 3647 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3648 | `			ph7_type_name(apArg[0])` |
|       - | 3649 | `			);` |
|       - | 3650 | `	}` |
|       - | 3651 | `	/* Point to the internal representation of the target array */` |
|      49 | 3652 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3653 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3654 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3655 | `	if( iOfft < 0 ){` |
|       7 | 3656 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3657 | `		if( iOfft < 0 ){` |
|       3 | 3658 | `			iOfft = 0;` |
|       2 | 3659 | `		}` |
|      46 | 3660 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3661 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3662 | `	}` |
|       - | 3663 | `	/* Get the length and clamp to valid range.` |
|       - | 3664 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3665 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3666 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3667 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3668 | `		if( iLength < 0 ){` |
|       7 | 3669 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3670 | `			if( iLength < 0 ){` |
|       3 | 3671 | `				iLength = 0;` |
|       1 | 3672 | `			}` |
|       3 | 3673 | `		}` |
|      31 | 3674 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3675 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3676 | `		}` |
|      15 | 3677 | `	}` |
|       - | 3678 | `	/* Create the result array for removed elements */` |
|      49 | 3679 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3680 | `	if( pArray == 0 ){` |
|     ! 0 | 3681 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3682 | `		return PH7_OK;` |
|       - | 3683 | `	}` |
|       - | 3684 | `	/* Get replacement array if provided */` |
|      49 | 3685 | `	pRep = 0;` |
|      49 | 3686 | `	if( nArg > 3 ){` |
|      21 | 3687 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3688 | `			/* Perform an array cast */` |
|       3 | 3689 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3690 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3691 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3692 | `			}` |
|       2 | 3693 | `		}else{` |
|      19 | 3694 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3695 | `		}` |
|      21 | 3696 | `		if( pRep ){` |
|       - | 3697 | `			/* Reset the loop cursor */` |
|      21 | 3698 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3699 | `		}` |
|      10 | 3700 | `	}` |
|       - | 3701 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3702 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3703 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3704 | `		return PH7_OK;` |
|       - | 3705 | `	}` |
|       - | 3706 | `	/* Navigate to the offset position */` |
|      41 | 3707 | `	pCur = pSrc->pFirst;` |
|      85 | 3708 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3709 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3710 | `	}` |
|       - | 3711 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3712 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3713 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3714 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3715 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3716 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3717 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3718 | `		pPrev = pCur->pPrev;` |
|      71 | 3719 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3720 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3721 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3722 | `			break;` |
|       - | 3723 | `		}` |
|      71 | 3724 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3725 | `	}` |
|       - | 3726 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3727 | `	if( pRep ){` |
|       - | 3728 | `		ph7_value sSafeVal;` |
|      61 | 3729 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3730 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3731 | `			if( pRvalue ){` |
|       - | 3732 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3733 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3734 | `				 * since it points into that same pool. */` |
|      31 | 3735 | `				sSafeVal = *pRvalue;` |
|      31 | 3736 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 3737 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 3738 | `					pNewNode = pSrc->pLast;` |
|      31 | 3739 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 3740 | `					pInsertAfter = pNewNode;` |
|      15 | 3741 | `				}` |
|      15 | 3742 | `			}` |
|       1 | 3743 | `		}` |
|      10 | 3744 | `	}` |
|       - | 3745 | `	/* Return the freshly created array */` |
|      41 | 3746 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 3747 | `	return PH7_OK;` |
|      29 | 3748 |  |
|       - | 3749 | `/*` |
|       - | 3750 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3751 | ` *  Checks if a value exists in an array.` |
|       - | 3752 | ` * Parameters` |
|       - | 3753 | ` *  $needle` |
|       - | 3754 | ` *   The searched value.` |
|       - | 3755 | ` *   Note:` |
|       - | 3756 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3757 | ` * $haystack` |
|       - | 3758 | ` *  The target array.` |
|       - | 3759 | ` * $strict` |
|       - | 3760 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3761 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3762 | ` */` |
|   20882 | 3763 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3764 |  |
|       - | 3765 | `	ph7_value *pNeedle;` |
|       - | 3766 | `	int bStrict;` |
|       - | 3767 | `	int rc;` |
|   20884 | 3768 | `	if( nArg < 2 ){` |
|       - | 3769 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3770 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3771 | `		return PH7_OK;` |
|       - | 3772 | `	}` |
|   20884 | 3773 | `	pNeedle = apArg[0];` |
|   20884 | 3774 | `	bStrict = 0;` |
|   20884 | 3775 | `	if( nArg > 2 ){` |
|       5 | 3776 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3777 | `	}` |
|   20884 | 3778 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3779 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3780 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3781 | `		/* Set the comparison result */` |
|     ! 0 | 3782 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3783 | `		return PH7_OK;` |
|       - | 3784 | `	}` |
|       - | 3785 | `	/* Perform the lookup */` |
|   20884 | 3786 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3787 | `	/* Lookup result */` |
|   20884 | 3788 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   20884 | 3789 | `	return PH7_OK;` |
|   10443 | 3790 |  |
|       - | 3791 | `/*` |
|       - | 3792 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3793 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3794 | ` * Parameters` |
|       - | 3795 | ` * $needle` |
|       - | 3796 | ` *   The searched value.` |
|       - | 3797 | ` * $haystack` |
|       - | 3798 | ` *   The array.` |
|       - | 3799 | ` * $strict` |
|       - | 3800 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3801 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3802 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3803 | ` * Return` |
|       - | 3804 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3805 | ` */` |
|      28 | 3806 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3807 |  |
|       - | 3808 | `	ph7_hashmap_node *pEntry;` |
|       - | 3809 | `	ph7_value *pVal,sNeedle;` |
|       - | 3810 | `	ph7_hashmap *pMap;` |
|       - | 3811 | `	ph7_value sVal;` |
|       - | 3812 | `	int bStrict;` |
|       - | 3813 | `	sxu32 n;` |
|       - | 3814 | `	int rc;` |
|      30 | 3815 | `	if( nArg < 2 ){` |
|       - | 3816 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3817 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3818 | `			"ArgumentCountError",` |
|       - | 3819 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 3820 | `			nArg` |
|       - | 3821 | `			);` |
|       - | 3822 | `	}` |
|      26 | 3823 | `	bStrict = FALSE;` |
|      26 | 3824 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3825 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3826 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3827 | `			"TypeError",` |
|       - | 3828 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 3829 | `			ph7_type_name(apArg[1])` |
|       - | 3830 | `			);` |
|       - | 3831 | `	}` |
|      24 | 3832 | `	if( nArg > 2 ){` |
|       - | 3833 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 3834 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3835 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3836 | `				"TypeError",` |
|       - | 3837 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3838 | `				ph7_type_name(apArg[2])` |
|       - | 3839 | `				);` |
|       - | 3840 | `		}` |
|       9 | 3841 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 3842 | `	}` |
|       - | 3843 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 3844 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3845 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 3846 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 3847 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 3848 | `	pEntry = pMap->pFirst;` |
|      21 | 3849 | `	n = pMap->nEntry;` |
|      23 | 3850 | `	for(;;){` |
|      47 | 3851 | `		if( !n ){` |
|       9 | 3852 | `			break;` |
|       - | 3853 | `		}` |
|       - | 3854 | `		/* Extract node value */` |
|      39 | 3855 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 3856 | `		if( pVal ){` |
|       - | 3857 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3858 | `			 * can change their type.` |
|       - | 3859 | `			 */` |
|      39 | 3860 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 3861 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 3862 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 3863 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 3864 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 3865 | `			if( rc == 0 ){` |
|       - | 3866 | `				/* Match found,return key */` |
|      13 | 3867 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3868 | `					/* INT key */` |
|       7 | 3869 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 3870 | `				}else{` |
|       7 | 3871 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3872 | `					/* Blob key */` |
|       7 | 3873 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3874 | `				}` |
|      13 | 3875 | `				return PH7_OK;` |
|       - | 3876 | `			}` |
|      13 | 3877 | `		}` |
|       - | 3878 | `		/* Point to the next entry */` |
|      27 | 3879 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 3880 | `		n--;` |
|       1 | 3881 | `	}` |
|       - | 3882 | `	/* No such value,return FALSE */` |
|       9 | 3883 | `	ph7_result_bool(pCtx,0);` |
|       9 | 3884 | `	return PH7_OK;` |
|      16 | 3885 |  |
|       - | 3886 | `/*` |
|       - | 3887 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3888 | ` *  Computes the difference of arrays.` |
|       - | 3889 | ` * Parameters` |
|       - | 3890 | ` *  $array1` |
|       - | 3891 | ` *    The array to compare from` |
|       - | 3892 | ` *  $array2` |
|       - | 3893 | ` *    An array to compare against` |
|       - | 3894 | ` *  $...` |
|       - | 3895 | ` *   More arrays to compare against` |
|       - | 3896 | ` * Return` |
|       - | 3897 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3898 | ` *  are not present in any of the other arrays.` |
|       - | 3899 | ` */` |
|      22 | 3900 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3901 |  |
|       - | 3902 | `	ph7_hashmap_node *pEntry;` |
|       - | 3903 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3904 | `	ph7_value *pArray;` |
|       - | 3905 | `	ph7_value *pVal;` |
|       - | 3906 | `	sxi32 rc;` |
|       - | 3907 | `	sxu32 n;` |
|       - | 3908 | `	int i;` |
|       - | 3909 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3910 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3911 | `	 * debugging difficult. */` |
|      24 | 3912 | `	if( nArg < 1 ){` |
|       4 | 3913 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3914 | `			"ArgumentCountError",` |
|       - | 3915 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3916 | `			nArg` |
|       - | 3917 | `			);` |
|       - | 3918 | `	}` |
|      22 | 3919 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3920 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3921 | `			"TypeError",` |
|       - | 3922 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3923 | `			ph7_type_name(apArg[0])` |
|       - | 3924 | `			);` |
|       - | 3925 | `	}` |
|      36 | 3926 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3927 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3928 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3929 | `				"TypeError",` |
|       - | 3930 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3931 | `				i + 1,` |
|       2 | 3932 | `				ph7_type_name(apArg[i])` |
|       - | 3933 | `				);` |
|       - | 3934 | `		}` |
|       9 | 3935 | `	}` |
|      17 | 3936 | `	if( nArg == 1 ){` |
|       - | 3937 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3938 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3939 | `		return PH7_OK;` |
|       - | 3940 | `	}` |
|       - | 3941 | `	/* Create a new array */` |
|      15 | 3942 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3943 | `	if( pArray == 0 ){` |
|     ! 0 | 3944 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3945 | `		return PH7_OK;` |
|       - | 3946 | `	}` |
|       - | 3947 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 3948 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3949 | `	/* Perform the diff */` |
|      15 | 3950 | `	pEntry = pSrc->pFirst;` |
|      15 | 3951 | `	n = pSrc->nEntry;` |
|      27 | 3952 | `	for(;;){` |
|      55 | 3953 | `		if( n < 1 ){` |
|      15 | 3954 | `			break;` |
|       - | 3955 | `		}` |
|       - | 3956 | `		/* Extract the node value */` |
|      41 | 3957 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 3958 | `		if( pVal ){` |
|      69 | 3959 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3960 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 3961 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3962 | `				/* Perform the lookup */` |
|      45 | 3963 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 3964 | `				if( rc == SXRET_OK ){` |
|       - | 3965 | `					/* Value exist */` |
|      17 | 3966 | `					break;` |
|       - | 3967 | `				}` |
|      15 | 3968 | `			}` |
|      41 | 3969 | `			if( i >= nArg ){` |
|       - | 3970 | `				/* Perform the insertion */` |
|      25 | 3971 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 3972 | `			}` |
|      20 | 3973 | `		}` |
|       - | 3974 | `		/* Point to the next entry */` |
|      41 | 3975 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 3976 | `		n--;` |
|       1 | 3977 | `	}` |
|       - | 3978 | `	/* Return the freshly created array */` |
|      15 | 3979 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3980 | `	return PH7_OK;` |
|      13 | 3981 |  |
|       - | 3982 | `/*` |
|       - | 3983 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 3984 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 3985 | ` * Parameters` |
|       - | 3986 | ` *  $array1` |
|       - | 3987 | ` *    The array to compare from` |
|       - | 3988 | ` *  $array2` |
|       - | 3989 | ` *    An array to compare against` |
|       - | 3990 | ` *  $...` |
|       - | 3991 | ` *   More arrays to compare against.` |
|       - | 3992 | ` * $callback` |
|       - | 3993 | ` *  The callback comparison function.` |
|       - | 3994 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3995 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3996 | ` *  than the second.` |
|       - | 3997 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3998 | ` * Return` |
|       - | 3999 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4000 | ` *  are not present in any of the other arrays.` |
|       - | 4001 | ` */` |
|       2 | 4002 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4003 |  |
|       - | 4004 | `	ph7_hashmap_node *pEntry;` |
|       - | 4005 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4006 | `	ph7_value *pCallback;` |
|       - | 4007 | `	ph7_value *pArray;` |
|       - | 4008 | `	ph7_value *pVal;` |
|       - | 4009 | `	sxi32 rc;` |
|       - | 4010 | `	sxu32 n;` |
|       - | 4011 | `	int i;` |
|       3 | 4012 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4013 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4014 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4015 | `		return PH7_OK;` |
|       - | 4016 | `	}` |
|       - | 4017 | `	/* Point to the callback */` |
|       3 | 4018 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4019 | `	if( nArg == 2 ){` |
|       - | 4020 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4021 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4022 | `		return PH7_OK;` |
|       - | 4023 | `	}` |
|       - | 4024 | `	/* Create a new array */` |
|       3 | 4025 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4026 | `	if( pArray == 0 ){` |
|     ! 0 | 4027 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4028 | `		return PH7_OK;` |
|       - | 4029 | `	}` |
|       - | 4030 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4031 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4032 | `	/* Perform the diff */` |
|       3 | 4033 | `	pEntry = pSrc->pFirst;` |
|       3 | 4034 | `	n = pSrc->nEntry;` |
|       4 | 4035 | `	for(;;){` |
|       9 | 4036 | `		if( n < 1 ){` |
|       3 | 4037 | `			break;` |
|       - | 4038 | `		}` |
|       - | 4039 | `		/* Extract the node value */` |
|       7 | 4040 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4041 | `		if( pVal ){` |
|      11 | 4042 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4043 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4044 | `					/* ignore */` |
|     ! 0 | 4045 | `					continue;` |
|       - | 4046 | `				}` |
|       - | 4047 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4048 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4049 | `				/* Perform the lookup */` |
|       7 | 4050 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4051 | `				if( rc == SXRET_OK ){` |
|       - | 4052 | `					/* Value exist */` |
|       3 | 4053 | `					break;` |
|       - | 4054 | `				}` |
|       3 | 4055 | `			}` |
|       7 | 4056 | `			if( i >= (nArg - 1)){` |
|       - | 4057 | `				/* Perform the insertion */` |
|       5 | 4058 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4059 | `			}` |
|       3 | 4060 | `		}` |
|       - | 4061 | `		/* Point to the next entry */` |
|       7 | 4062 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4063 | `		n--;` |
|       1 | 4064 | `	}` |
|       - | 4065 | `	/* Return the freshly created array */` |
|       3 | 4066 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4067 | `	return PH7_OK;` |
|       2 | 4068 |  |
|       - | 4069 | `/*` |
|       - | 4070 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4071 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4072 | ` * Parameters` |
|       - | 4073 | ` *  $array1` |
|       - | 4074 | ` *    The array to compare from` |
|       - | 4075 | ` *  $array2` |
|       - | 4076 | ` *    An array to compare against` |
|       - | 4077 | ` *  $...` |
|       - | 4078 | ` *   More arrays to compare against` |
|       - | 4079 | ` * Return` |
|       - | 4080 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4081 | ` *  are not present in any of the other arrays.` |
|       - | 4082 | ` */` |
|      20 | 4083 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4084 |  |
|       - | 4085 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4086 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4087 | `	ph7_value *pArray;` |
|       - | 4088 | `	ph7_value *pVal;` |
|       - | 4089 | `	sxi32 rc;` |
|       - | 4090 | `	sxu32 n;` |
|       - | 4091 | `	int i;` |
|       - | 4092 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4093 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4094 | `	 * accompanying integration tests to pass. */` |
|      22 | 4095 | `	if( nArg < 1 ){` |
|       4 | 4096 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4097 | `			"ArgumentCountError",` |
|       - | 4098 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4099 | `			nArg` |
|       - | 4100 | `			);` |
|       - | 4101 | `	}` |
|      20 | 4102 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4103 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4104 | `			"TypeError",` |
|       - | 4105 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4106 | `			ph7_type_name(apArg[0])` |
|       - | 4107 | `			);` |
|       - | 4108 | `	}` |
|      32 | 4109 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4110 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4111 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4112 | `				"TypeError",` |
|       - | 4113 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4114 | `				i + 1,` |
|       4 | 4115 | `				ph7_type_name(apArg[i])` |
|       - | 4116 | `				);` |
|       - | 4117 | `		}` |
|       9 | 4118 | `	}` |
|      13 | 4119 | `	if( nArg == 1 ){` |
|       - | 4120 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4121 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4122 | `		return PH7_OK;` |
|       - | 4123 | `	}` |
|       - | 4124 | `	/* Create a new array */` |
|      11 | 4125 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4126 | `	if( pArray == 0 ){` |
|     ! 0 | 4127 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4128 | `		return PH7_OK;` |
|       - | 4129 | `	}` |
|       - | 4130 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4131 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4132 | `	/* Perform the diff */` |
|      11 | 4133 | `	pEntry = pSrc->pFirst;` |
|      11 | 4134 | `	n = pSrc->nEntry;` |
|      11 | 4135 | `	pN1 = pN2 = 0;` |
|      29 | 4136 | `	for(;;){` |
|       - | 4137 | `		int keep;` |
|      35 | 4138 | `		if( n < 1 ){` |
|      11 | 4139 | `			break;` |
|       - | 4140 | `		}` |
|       - | 4141 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4142 | `		keep = 1;` |
|      41 | 4143 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4144 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4145 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4146 | `			/* Perform a key lookup first */` |
|      29 | 4147 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4148 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4149 | `			}else{` |
|      17 | 4150 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4151 | `			}` |
|      29 | 4152 | `			if( rc != SXRET_OK ){` |
|       - | 4153 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4154 | `				continue;` |
|       - | 4155 | `			}` |
|       - | 4156 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4157 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4158 | `			if( pVal ){` |
|       - | 4159 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4160 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4161 | `				if( pVal2 ){` |
|      15 | 4162 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4163 | `					if( cmp == 0 ){` |
|       - | 4164 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4165 | `						keep = 0;` |
|      13 | 4166 | `						break;` |
|       - | 4167 | `					}` |
|       1 | 4168 | `				}` |
|       1 | 4169 | `			}` |
|       2 | 4170 | `		}` |
|      25 | 4171 | `		if( keep ){` |
|       - | 4172 | `			/* Perform the insertion */` |
|      13 | 4173 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4174 | `		}` |
|       - | 4175 | `		/* Point to the next entry */` |
|      25 | 4176 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4177 | `		n--;` |
|       1 | 4178 | `	}` |
|       - | 4179 | `	/* Return the freshly created array */` |
|      11 | 4180 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4181 | `	return PH7_OK;` |
|      12 | 4182 |  |
|       - | 4183 | `/*` |
|       - | 4184 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4185 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4186 | ` *  by a user supplied callback function.` |
|       - | 4187 | ` * Parameters` |
|       - | 4188 | ` *  $array1` |
|       - | 4189 | ` *    The array to compare from` |
|       - | 4190 | ` *  $array2` |
|       - | 4191 | ` *    An array to compare against` |
|       - | 4192 | ` *  $...` |
|       - | 4193 | ` *   More arrays to compare against.` |
|       - | 4194 | ` *  $key_compare_func` |
|       - | 4195 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4196 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4197 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4198 | ` * Return` |
|       - | 4199 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4200 | ` *  are not present in any of the other arrays.` |
|       - | 4201 | ` */` |
|      22 | 4202 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4203 |  |
|       - | 4204 | `	ph7_hashmap_node *pEntry;` |
|       - | 4205 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4206 | `	ph7_value *pCallback;` |
|       - | 4207 | `	ph7_value *pArray;` |
|       - | 4208 | `	sxi32 rc;` |
|       - | 4209 | `	sxu32 n;` |
|       - | 4210 | `	int i;` |
|       - | 4211 |  |
|       - | 4212 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4213 | `	if( nArg < 2 ){` |
|       4 | 4214 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4215 | `			"ArgumentCountError",` |
|       - | 4216 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4217 | `			nArg` |
|       - | 4218 | `			);` |
|       - | 4219 | `	}` |
|      22 | 4220 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4221 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4222 | `			"TypeError",` |
|       - | 4223 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4224 | `			ph7_type_name(apArg[0])` |
|       - | 4225 | `			);` |
|       - | 4226 | `	}` |
|       - | 4227 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4228 | `	 * expected to be a callback. */` |
|      32 | 4229 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4230 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4231 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4232 | `				"TypeError",` |
|       - | 4233 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4234 | `				i + 1,` |
|       2 | 4235 | `				ph7_type_name(apArg[i])` |
|       - | 4236 | `				);` |
|       - | 4237 | `		}` |
|       8 | 4238 | `	}` |
|       - | 4239 | `	/* Point to the callback value */` |
|      18 | 4240 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4241 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4242 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4243 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4244 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4245 | `		 * string given" which we also reproduce. */` |
|       7 | 4246 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4247 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4248 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4249 | `				"TypeError",` |
|       - | 4250 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4251 | `				nArg` |
|       - | 4252 | `				);` |
|       - | 4253 | `		}` |
|       5 | 4254 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4255 | `			/* neither array nor string */` |
|       7 | 4256 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4257 | `				"TypeError",` |
|       - | 4258 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4259 | `				nArg` |
|       - | 4260 | `				);` |
|       - | 4261 | `		}` |
|       - | 4262 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4263 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4264 | `			"TypeError",` |
|       - | 4265 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4266 | `			nArg,` |
|     ! 0 | 4267 | `			ph7_type_name(pCallback)` |
|       - | 4268 | `			);` |
|       - | 4269 | `	}` |
|      11 | 4270 | `	if( nArg == 2 ){` |
|       - | 4271 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4272 | `		 * input array. */` |
|       3 | 4273 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4274 | `		return PH7_OK;` |
|       - | 4275 | `	}` |
|       - | 4276 | `	/* Create a new array */` |
|       9 | 4277 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4278 | `	if( pArray == 0 ){` |
|     ! 0 | 4279 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4280 | `		return PH7_OK;` |
|       - | 4281 | `	}` |
|       - | 4282 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4283 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4284 | `	/* Perform the diff */` |
|       9 | 4285 | `	pEntry = pSrc->pFirst;` |
|       9 | 4286 | `	n = pSrc->nEntry;` |
|      20 | 4287 | `	for(;;){` |
|       - | 4288 | `		int keep;` |
|      25 | 4289 | `		if( n < 1 ){` |
|       9 | 4290 | `			break;` |
|       - | 4291 | `		}` |
|      17 | 4292 | `		keep = 1;` |
|      29 | 4293 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4294 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4295 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4296 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4297 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4298 | `			while( pIt ){` |
|       - | 4299 | `				/* build temporary key values for callback */` |
|       - | 4300 | `				ph7_value key1, key2, result;` |
|       - | 4301 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4302 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4303 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4304 | `				}else{` |
|       - | 4305 | `					SyString sStr;` |
|      31 | 4306 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4307 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4308 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4309 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4310 | `				}` |
|      31 | 4311 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4312 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4313 | `				}else{` |
|       - | 4314 | `					SyString sStr;` |
|      31 | 4315 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4316 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4317 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4318 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4319 | `				}` |
|      31 | 4320 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4321 | `				/* call user callback with (key1, key2) */` |
|       - | 4322 | `				{` |
|       - | 4323 | `					ph7_value *apK[2];` |
|      31 | 4324 | `					apK[0] = &key1;` |
|      31 | 4325 | `					apK[1] = &key2;` |
|      31 | 4326 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4327 | `				}` |
|      31 | 4328 | `				if( rc == SXRET_OK ){` |
|      31 | 4329 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4330 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4331 | `					}` |
|      31 | 4332 | `					if( result.x.iVal == 0 ){` |
|       - | 4333 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4334 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4335 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4336 | `						if( pVal1 && pVal2 ){` |
|      13 | 4337 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4338 | `								keep = 0;` |
|       9 | 4339 | `								PH7_MemObjRelease(&result);` |
|       - | 4340 | `								/* release keys too before breaking */` |
|       9 | 4341 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4342 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4343 | `								break;` |
|       - | 4344 | `							}` |
|       2 | 4345 | `						}` |
|       2 | 4346 | `					}` |
|      11 | 4347 | `				}` |
|      23 | 4348 | `				PH7_MemObjRelease(&result);` |
|      23 | 4349 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4350 | `				PH7_MemObjRelease(&key2);` |
|       - | 4351 | `				/* move to next node */` |
|      23 | 4352 | `				pIt = pIt->pPrev;` |
|      23 | 4353 | `				if( keep == 0 ) break;` |
|       1 | 4354 | `			}` |
|      21 | 4355 | `			if( keep == 0 ) break;` |
|       7 | 4356 | `		}` |
|      17 | 4357 | `		if( keep ){` |
|       - | 4358 | `			/* Perform the insertion */` |
|       9 | 4359 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4360 | `		}` |
|       - | 4361 | `		/* Point to the next entry */` |
|      17 | 4362 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4363 | `		n--;` |
|       1 | 4364 | `	}` |
|       - | 4365 | `	/* Return the freshly created array */` |
|       9 | 4366 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4367 | `	return PH7_OK;` |
|      13 | 4368 |  |
|       - | 4369 | `/*` |
|       - | 4370 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4371 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4372 | ` * Parameters` |
|       - | 4373 | ` *  $array1` |
|       - | 4374 | ` *    The array to compare from` |
|       - | 4375 | ` *  $array2` |
|       - | 4376 | ` *    An array to compare against` |
|       - | 4377 | ` *  $...` |
|       - | 4378 | ` *   More arrays to compare against` |
|       - | 4379 | ` * Return` |
|       - | 4380 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4381 | ` *  in any of the other arrays.` |
|       - | 4382 | ` * Note that NULL is returned on failure.` |
|       - | 4383 | ` */` |
|      14 | 4384 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4385 |  |
|       - | 4386 | `	ph7_hashmap_node *pEntry;` |
|       - | 4387 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4388 | `	ph7_value *pArray;` |
|       - | 4389 | `	sxi32 rc;` |
|       - | 4390 | `	sxu32 n;` |
|       - | 4391 | `	int i;` |
|       - | 4392 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4393 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4394 | `	 * helpers. */` |
|      16 | 4395 | `	if( nArg < 1 ){` |
|       4 | 4396 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4397 | `			"ArgumentCountError",` |
|       - | 4398 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4399 | `			nArg` |
|       - | 4400 | `			);` |
|       - | 4401 | `	}` |
|      14 | 4402 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4403 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4404 | `			"TypeError",` |
|       - | 4405 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4406 | `			ph7_type_name(apArg[0])` |
|       - | 4407 | `			);` |
|       - | 4408 | `	}` |
|      20 | 4409 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4410 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4411 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4412 | `				"TypeError",` |
|       - | 4413 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4414 | `				i + 1,` |
|       2 | 4415 | `				ph7_type_name(apArg[i])` |
|       - | 4416 | `				);` |
|       - | 4417 | `		}` |
|       5 | 4418 | `	}` |
|       9 | 4419 | `	if( nArg == 1 ){` |
|       - | 4420 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4421 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4422 | `		return PH7_OK;` |
|       - | 4423 | `	}` |
|       - | 4424 | `	/* Create a new array */` |
|       7 | 4425 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4426 | `	if( pArray == 0 ){` |
|     ! 0 | 4427 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4428 | `		return PH7_OK;` |
|       - | 4429 | `	}` |
|       - | 4430 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4431 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4432 | `	/* Perfrom the diff */` |
|       7 | 4433 | `	pEntry = pSrc->pFirst;` |
|       7 | 4434 | `	n = pSrc->nEntry;` |
|      12 | 4435 | `	for(;;){` |
|      25 | 4436 | `		if( n < 1 ){` |
|       7 | 4437 | `			break;` |
|       - | 4438 | `		}` |
|      31 | 4439 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4440 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4441 | `				/* ignore */` |
|     ! 0 | 4442 | `				continue;` |
|       - | 4443 | `			}` |
|      23 | 4444 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4445 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4446 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4447 | `				/* Blob lookup */` |
|      17 | 4448 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4449 | `			}else{` |
|       - | 4450 | `				/* Int lookup */` |
|       7 | 4451 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4452 | `			}` |
|      23 | 4453 | `			if( rc == SXRET_OK ){` |
|       - | 4454 | `				/* Key exists,break immediately */` |
|      11 | 4455 | `				break;` |
|       - | 4456 | `			}` |
|       7 | 4457 | `		}` |
|      19 | 4458 | `		if( i >= nArg ){` |
|       - | 4459 | `			/* Perform the insertion */` |
|       9 | 4460 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4461 | `		}` |
|       - | 4462 | `		/* Point to the next entry */` |
|      19 | 4463 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4464 | `		n--;` |
|       1 | 4465 | `	}` |
|       - | 4466 | `	/* Return the freshly created array */` |
|       7 | 4467 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4468 | `	return PH7_OK;` |
|       9 | 4469 |  |
|       - | 4470 | `/*` |
|       - | 4471 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4472 | ` *  Computes the intersection of arrays.` |
|       - | 4473 | ` * Parameters` |
|       - | 4474 | ` *  $array1` |
|       - | 4475 | ` *    The array to compare from` |
|       - | 4476 | ` *  $array2` |
|       - | 4477 | ` *    An array to compare against` |
|       - | 4478 | ` *  $...` |
|       - | 4479 | ` *   More arrays to compare against` |
|       - | 4480 | ` * Return` |
|       - | 4481 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4482 | ` *  in all of the parameters.` |
|       - | 4483 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4484 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4485 | ` */` |
|      22 | 4486 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4487 |  |
|       - | 4488 | `	ph7_hashmap_node *pEntry;` |
|       - | 4489 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4490 | `	ph7_value *pArray;` |
|       - | 4491 | `	ph7_value *pVal;` |
|       - | 4492 | `	sxi32 rc;` |
|       - | 4493 | `	sxu32 n;` |
|       - | 4494 | `	int i;` |
|      24 | 4495 | `	if( nArg < 1 ){` |
|       4 | 4496 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4497 | `			"ArgumentCountError",` |
|       - | 4498 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4499 | `			nArg` |
|       - | 4500 | `			);` |
|       - | 4501 | `	}` |
|      22 | 4502 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4503 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4504 | `			"TypeError",` |
|       - | 4505 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4506 | `			ph7_type_name(apArg[0])` |
|       - | 4507 | `			);` |
|       - | 4508 | `	}` |
|      36 | 4509 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4510 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4511 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4512 | `				"TypeError",` |
|       - | 4513 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4514 | `				i + 1,` |
|       2 | 4515 | `				ph7_type_name(apArg[i])` |
|       - | 4516 | `				);` |
|       - | 4517 | `		}` |
|       9 | 4518 | `	}` |
|      17 | 4519 | `	if( nArg == 1 ){` |
|       - | 4520 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4521 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4522 | `		return PH7_OK;` |
|       - | 4523 | `	}` |
|       - | 4524 | `	/* Create a new array */` |
|      15 | 4525 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4526 | `	if( pArray == 0 ){` |
|     ! 0 | 4527 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4528 | `		return PH7_OK;` |
|       - | 4529 | `	}` |
|       - | 4530 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4531 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4532 | `	/* Perform the intersection */` |
|      15 | 4533 | `	pEntry = pSrc->pFirst;` |
|      15 | 4534 | `	n = pSrc->nEntry;` |
|      31 | 4535 | `	for(;;){` |
|      63 | 4536 | `		if( n < 1 ){` |
|      15 | 4537 | `			break;` |
|       - | 4538 | `		}` |
|       - | 4539 | `		/* Extract the node value */` |
|      49 | 4540 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4541 | `		if( pVal ){` |
|      79 | 4542 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4543 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4544 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4545 | `				/* Perform the lookup */` |
|      55 | 4546 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4547 | `				if( rc != SXRET_OK ){` |
|       - | 4548 | `					/* Value does not exist */` |
|      25 | 4549 | `					break;` |
|       - | 4550 | `				}` |
|      16 | 4551 | `			}` |
|      49 | 4552 | `			if( i >= nArg ){` |
|       - | 4553 | `				/* Perform the insertion */` |
|      25 | 4554 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4555 | `			}` |
|      24 | 4556 | `		}` |
|       - | 4557 | `		/* Point to the next entry */` |
|      49 | 4558 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4559 | `		n--;` |
|       1 | 4560 | `	}` |
|       - | 4561 | `	/* Return the freshly created array */` |
|      15 | 4562 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4563 | `	return PH7_OK;` |
|      13 | 4564 |  |
|       - | 4565 | `/*` |
|       - | 4566 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4567 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4568 | ` * Parameters` |
|       - | 4569 | ` *  $array1` |
|       - | 4570 | ` *    The array to compare from` |
|       - | 4571 | ` *  $array2` |
|       - | 4572 | ` *    An array to compare against` |
|       - | 4573 | ` *  $...` |
|       - | 4574 | ` *   More arrays to compare against` |
|       - | 4575 | ` * Return` |
|       - | 4576 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4577 | ` *  in all the arguments, with matching keys.` |
|       - | 4578 | ` */` |
|      22 | 4579 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4580 |  |
|       - | 4581 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4582 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4583 | `	ph7_value *pArray;` |
|       - | 4584 | `	ph7_value *pVal;` |
|       - | 4585 | `	sxi32 rc;` |
|       - | 4586 | `	sxu32 n;` |
|       - | 4587 | `	int i;` |
|      24 | 4588 | `	if( nArg < 1 ){` |
|       4 | 4589 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4590 | `			"ArgumentCountError",` |
|       - | 4591 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4592 | `			nArg` |
|       - | 4593 | `			);` |
|       - | 4594 | `	}` |
|      22 | 4595 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4596 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4597 | `			"TypeError",` |
|       - | 4598 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4599 | `			ph7_type_name(apArg[0])` |
|       - | 4600 | `			);` |
|       - | 4601 | `	}` |
|      36 | 4602 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4603 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4604 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4605 | `				"TypeError",` |
|       - | 4606 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4607 | `				i + 1,` |
|       2 | 4608 | `				ph7_type_name(apArg[i])` |
|       - | 4609 | `				);` |
|       - | 4610 | `		}` |
|       9 | 4611 | `	}` |
|      17 | 4612 | `	if( nArg == 1 ){` |
|       - | 4613 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4614 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4615 | `		return PH7_OK;` |
|       - | 4616 | `	}` |
|       - | 4617 | `	/* Create a new array */` |
|      15 | 4618 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4619 | `	if( pArray == 0 ){` |
|     ! 0 | 4620 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4621 | `		return PH7_OK;` |
|       - | 4622 | `	}` |
|       - | 4623 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4624 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4625 | `	/* Perform the intersection */` |
|      15 | 4626 | `	pEntry = pSrc->pFirst;` |
|      15 | 4627 | `	n = pSrc->nEntry;` |
|      15 | 4628 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4629 | `	for(;;){` |
|      47 | 4630 | `		if( n < 1 ){` |
|      15 | 4631 | `			break;` |
|       - | 4632 | `		}` |
|       - | 4633 | `		/* Extract the node value */` |
|      33 | 4634 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4635 | `		if( pVal ){` |
|      53 | 4636 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4637 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4638 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4639 | `				/* Perform a key lookup first */` |
|      37 | 4640 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4641 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4642 | `				}else{` |
|      23 | 4643 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4644 | `				}` |
|      37 | 4645 | `				if( rc != SXRET_OK ){` |
|       - | 4646 | `					/* No such key,break immediately */` |
|       7 | 4647 | `					break;` |
|       - | 4648 | `				}` |
|       - | 4649 | `				/* Perform the lookup */` |
|      31 | 4650 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4651 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4652 | `					/* Value does not exist */` |
|       6 | 4653 | `					break;` |
|       - | 4654 | `				}` |
|      11 | 4655 | `			}` |
|      33 | 4656 | `			if( i >= nArg ){` |
|       - | 4657 | `				/* Perform the insertion */` |
|      17 | 4658 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4659 | `			}` |
|      16 | 4660 | `		}` |
|       - | 4661 | `		/* Point to the next entry */` |
|      33 | 4662 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4663 | `		n--;` |
|       1 | 4664 | `	}` |
|       - | 4665 | `	/* Return the freshly created array */` |
|      15 | 4666 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4667 | `	return PH7_OK;` |
|      13 | 4668 |  |
|       - | 4669 | `/*` |
|       - | 4670 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 4671 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4672 | ` * Parameters` |
|       - | 4673 | ` *  $array1` |
|       - | 4674 | ` *    The array to compare from` |
|       - | 4675 | ` *  $...` |
|       - | 4676 | ` *   More arrays to compare against` |
|       - | 4677 | ` * Return` |
|       - | 4678 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4679 | ` *  have keys that are present in all arguments.` |
|       - | 4680 | ` * Note that NULL is returned on failure.` |
|       - | 4681 | ` */` |
|      22 | 4682 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4683 |  |
|       - | 4684 | `	ph7_hashmap_node *pEntry;` |
|       - | 4685 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4686 | `	ph7_value *pArray;` |
|       - | 4687 | `	sxi32 rc;` |
|       - | 4688 | `	sxu32 n;` |
|       - | 4689 | `	int i;` |
|      24 | 4690 | `	if( nArg < 1 ){` |
|       4 | 4691 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4692 | `			"ArgumentCountError",` |
|       - | 4693 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 4694 | `			nArg` |
|       - | 4695 | `			);` |
|       - | 4696 | `	}` |
|      22 | 4697 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4698 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4699 | `			"TypeError",` |
|       - | 4700 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4701 | `			ph7_type_name(apArg[0])` |
|       - | 4702 | `			);` |
|       - | 4703 | `	}` |
|      36 | 4704 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4705 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4706 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4707 | `				"TypeError",` |
|       - | 4708 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4709 | `				i + 1,` |
|       2 | 4710 | `				ph7_type_name(apArg[i])` |
|       - | 4711 | `				);` |
|       - | 4712 | `		}` |
|       9 | 4713 | `	}` |
|      17 | 4714 | `	if( nArg == 1 ){` |
|       - | 4715 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4716 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4717 | `		return PH7_OK;` |
|       - | 4718 | `	}` |
|       - | 4719 | `	/* Create a new array */` |
|      15 | 4720 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4721 | `	if( pArray == 0 ){` |
|     ! 0 | 4722 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4723 | `		return PH7_OK;` |
|       - | 4724 | `	}` |
|       - | 4725 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 4726 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4727 | `	/* Perform the intersection */` |
|      15 | 4728 | `	pEntry = pSrc->pFirst;` |
|      15 | 4729 | `	n = pSrc->nEntry;` |
|      24 | 4730 | `	for(;;){` |
|      49 | 4731 | `		if( n < 1 ){` |
|      15 | 4732 | `			break;` |
|       - | 4733 | `		}` |
|      57 | 4734 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 4735 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 4736 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 4737 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4738 | `				/* Blob lookup */` |
|      27 | 4739 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 4740 | `			}else{` |
|       - | 4741 | `				/* Int key */` |
|      13 | 4742 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4743 | `			}` |
|      39 | 4744 | `			if( rc != SXRET_OK ){` |
|       - | 4745 | `				/* Key does not exist, break immediately */` |
|      17 | 4746 | `				break;` |
|       - | 4747 | `			}` |
|      12 | 4748 | `		}` |
|      35 | 4749 | `		if( i >= nArg ){` |
|       - | 4750 | `			/* Perform the insertion */` |
|      19 | 4751 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 4752 | `		}` |
|       - | 4753 | `		/* Point to the next entry */` |
|      35 | 4754 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 4755 | `		n--;` |
|       1 | 4756 | `	}` |
|       - | 4757 | `	/* Return the freshly created array */` |
|      15 | 4758 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4759 | `	return PH7_OK;` |
|      13 | 4760 |  |
|       - | 4761 | `/*` |
|       - | 4762 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4763 | ` *  Computes the intersection of arrays.` |
|       - | 4764 | ` * Parameters` |
|       - | 4765 | ` *  $array1` |
|       - | 4766 | ` *    The array to compare from` |
|       - | 4767 | ` *  $array2` |
|       - | 4768 | ` *    An array to compare against` |
|       - | 4769 | ` *  $...` |
|       - | 4770 | ` *   More arrays to compare against` |
|       - | 4771 | ` * $callback` |
|       - | 4772 | ` *  The callback comparison function.` |
|       - | 4773 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4774 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4775 | ` *  than the second.` |
|       - | 4776 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4777 | ` * Return` |
|       - | 4778 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4779 | ` *  in all of the parameters. .` |
|       - | 4780 | ` * Note that NULL is returned on failure.` |
|       - | 4781 | ` */` |
|       2 | 4782 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4783 |  |
|       - | 4784 | `	ph7_hashmap_node *pEntry;` |
|       - | 4785 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4786 | `	ph7_value *pCallback;` |
|       - | 4787 | `	ph7_value *pArray;` |
|       - | 4788 | `	ph7_value *pVal;` |
|       - | 4789 | `	sxi32 rc;` |
|       - | 4790 | `	sxu32 n;` |
|       - | 4791 | `	int i;` |
|       - | 4792 |  |
|       3 | 4793 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4794 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4795 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4796 | `		return PH7_OK;` |
|       - | 4797 | `	}` |
|       - | 4798 | `	/* Point to the callback */` |
|       3 | 4799 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4800 | `	if( nArg == 2 ){` |
|       - | 4801 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4802 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4803 | `		return PH7_OK;` |
|       - | 4804 | `	}` |
|       - | 4805 | `	/* Create a new array */` |
|       3 | 4806 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4807 | `	if( pArray == 0 ){` |
|     ! 0 | 4808 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4809 | `		return PH7_OK;` |
|       - | 4810 | `	}` |
|       - | 4811 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4812 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4813 | `	/* Perform the intersection */` |
|       3 | 4814 | `	pEntry = pSrc->pFirst;` |
|       3 | 4815 | `	n = pSrc->nEntry;` |
|       4 | 4816 | `	for(;;){` |
|       9 | 4817 | `		if( n < 1 ){` |
|       3 | 4818 | `			break;` |
|       - | 4819 | `		}` |
|       - | 4820 | `		/* Extract the node value */` |
|       7 | 4821 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4822 | `		if( pVal ){` |
|      11 | 4823 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4824 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4825 | `					/* ignore */` |
|     ! 0 | 4826 | `					continue;` |
|       - | 4827 | `				}` |
|       - | 4828 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4829 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4830 | `				/* Perform the lookup */` |
|       7 | 4831 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4832 | `				if( rc != SXRET_OK ){` |
|       - | 4833 | `					/* Value does not exist */` |
|       3 | 4834 | `					break;` |
|       - | 4835 | `				}` |
|       3 | 4836 | `			}` |
|       7 | 4837 | `			if( i >= (nArg-1) ){` |
|       - | 4838 | `				/* Perform the insertion */` |
|       5 | 4839 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4840 | `			}` |
|       3 | 4841 | `		}` |
|       - | 4842 | `		/* Point to the next entry */` |
|       7 | 4843 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4844 | `		n--;` |
|       1 | 4845 | `	}` |
|       - | 4846 | `	/* Return the freshly created array */` |
|       3 | 4847 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4848 | `	return PH7_OK;` |
|       2 | 4849 |  |
|       - | 4850 | `/*` |
|       - | 4851 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 4852 | ` *  Fill an array with values.` |
|       - | 4853 | ` * Parameters` |
|       - | 4854 | ` *  $start_index` |
|       - | 4855 | ` *    The first index of the returned array.` |
|       - | 4856 | ` *  $num` |
|       - | 4857 | ` *   Number of elements to insert.` |
|       - | 4858 | ` *  $value` |
|       - | 4859 | ` *    Value to use for filling.` |
|       - | 4860 | ` * Return` |
|       - | 4861 | ` *  The filled array or null on failure.` |
|       - | 4862 | ` */` |
|     238 | 4863 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4864 |  |
|       - | 4865 | `	ph7_value *pArray;` |
|       - | 4866 | `	int i,nEntry;` |
|       - | 4867 |  |
|       - | 4868 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 4869 | `	if( nArg != 3 ){` |
|       - | 4870 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 4871 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4872 | `			"ArgumentCountError",` |
|       - | 4873 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 4874 | `			nArg` |
|       - | 4875 | `			);` |
|       - | 4876 | `	}` |
|       - | 4877 |  |
|       - | 4878 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 4879 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 4880 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 4881 | `	 * and NULLs are rejected outright. */` |
|     466 | 4882 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 4883 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 4884 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4885 | `			"TypeError",` |
|       - | 4886 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 4887 | `			ph7_type_name(apArg[0])` |
|       - | 4888 | `			);` |
|       - | 4889 | `	}` |
|     234 | 4890 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 4891 | `		int len;` |
|       8 | 4892 | `		sxu8 bReal = FALSE;` |
|       8 | 4893 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 4894 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 4895 | `			/* Non‑numeric string is an error. */` |
|       3 | 4896 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4897 | `				"TypeError",` |
|       - | 4898 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 4899 | `				);` |
|       - | 4900 | `		}` |
|       5 | 4901 | `		if( bReal ){` |
|       - | 4902 | `			/* float-string -> deprecation warning */` |
|       4 | 4903 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4904 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 4905 | `				zStr` |
|       - | 4906 | `				);` |
|       1 | 4907 | `		}` |
|       2 | 4908 | `	}` |
|       - | 4909 |  |
|       - | 4910 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 4911 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 4912 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 4913 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4914 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4915 | `			"TypeError",` |
|       - | 4916 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 4917 | `			ph7_type_name(apArg[1])` |
|       - | 4918 | `			);` |
|       - | 4919 | `	}` |
|     232 | 4920 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4921 | `		int len;` |
|       3 | 4922 | `		sxu8 bReal = FALSE;` |
|       3 | 4923 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4924 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 4925 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4926 | `				"TypeError",` |
|       - | 4927 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 4928 | `				);` |
|       - | 4929 | `		}` |
|     ! 0 | 4930 | `	}` |
|       - | 4931 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 4932 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 4933 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 4934 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 4935 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 4936 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 4937 | `		if( d != (double)i64 ){` |
|       7 | 4938 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4939 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 4940 | `				d` |
|       - | 4941 | `				);` |
|       2 | 4942 | `		}` |
|       2 | 4943 | `	}` |
|       - | 4944 |  |
|       - | 4945 | `	/* Total number of entries to insert */` |
|     230 | 4946 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 4947 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 4948 | `	if( nEntry < 0 ){` |
|       3 | 4949 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4950 | `			"ValueError",` |
|       - | 4951 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 4952 | `			);` |
|       - | 4953 | `	}` |
|       - | 4954 |  |
|       - | 4955 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 4956 | `	if( nEntry == 0 ){` |
|       7 | 4957 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 4958 | `		return PH7_OK;` |
|       - | 4959 | `	}` |
|       - | 4960 |  |
|       - | 4961 | `	/* Create a new array */` |
|     221 | 4962 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 4963 | `	if( pArray == 0 ){` |
|     ! 0 | 4964 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4965 | `		return PH7_OK;` |
|       - | 4966 | `	}` |
|       - | 4967 |  |
|       - | 4968 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 4969 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 4970 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 4971 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 4972 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 4973 | `	}` |
|       - | 4974 | `	/* Return the filled array */` |
|     221 | 4975 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 4976 | `	return PH7_OK;` |
|     121 | 4977 |  |
|       - | 4978 | `/*` |
|       - | 4979 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 4980 | ` *  Fill an array with values, specifying keys.` |
|       - | 4981 | ` * Parameters` |
|       - | 4982 | ` *  $input` |
|       - | 4983 | ` *   Array of values that will be used as key.` |
|       - | 4984 | ` *  $value` |
|       - | 4985 | ` *    Value to use for filling.` |
|       - | 4986 | ` * Return` |
|       - | 4987 | ` *  The filled array.` |
|       - | 4988 | ` * Throws` |
|       - | 4989 | ` *  ValueError if $input is not an array.` |
|       - | 4990 | ` */` |
|      26 | 4991 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4992 |  |
|       - | 4993 | `	ph7_hashmap_node *pEntry;` |
|       - | 4994 | `	ph7_hashmap *pSrc;` |
|       - | 4995 | `	ph7_value *pArray;` |
|       - | 4996 | `	sxu32 n;` |
|       - | 4997 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 4998 | `	if( nArg != 2 ){` |
|      10 | 4999 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5000 | `			"ArgumentCountError",` |
|       - | 5001 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5002 | `			nArg` |
|       - | 5003 | `			);` |
|       - | 5004 | `	}` |
|       - | 5005 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5006 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5007 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5008 | `			"TypeError",` |
|       - | 5009 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5010 | `			ph7_type_name(apArg[0])` |
|       - | 5011 | `			);` |
|       - | 5012 | `	}` |
|       - | 5013 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5014 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5015 | `	/* Create a new array */` |
|      17 | 5016 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5017 | `	if( pArray == 0 ){` |
|     ! 0 | 5018 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5019 | `		return PH7_OK;` |
|       - | 5020 | `	}` |
|       - | 5021 | `	/* Perform the requested operation */` |
|      17 | 5022 | `	pEntry = pSrc->pFirst;` |
|      45 | 5023 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5024 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5025 | `		/* Point to the next entry */` |
|      29 | 5026 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5027 | `	}` |
|       - | 5028 | `	/* Return the filled array */` |
|      17 | 5029 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5030 | `	return PH7_OK;` |
|      15 | 5031 |  |
|       - | 5032 | `/*` |
|       - | 5033 | ` * array array_combine(array $keys,array $values)` |
|       - | 5034 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5035 | ` * Parameters` |
|       - | 5036 | ` *  $keys` |
|       - | 5037 | ` *    Array of keys to be used.` |
|       - | 5038 | ` * $values` |
|       - | 5039 | ` *   Array of values to be used.` |
|       - | 5040 | ` * Return` |
|       - | 5041 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5042 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5043 | ` *  not an array.` |
|       - | 5044 | ` */` |
|      18 | 5045 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5046 |  |
|       - | 5047 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5048 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5049 | `	ph7_value *pArray;` |
|       - | 5050 | `	sxu32 n;` |
|       - | 5051 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5052 | `	if( nArg != 2 ){` |
|       - | 5053 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5054 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5055 | `			"ArgumentCountError",` |
|       - | 5056 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5057 | `			nArg` |
|       - | 5058 | `			);` |
|       - | 5059 | `	}` |
|       - | 5060 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5061 | `	 * argument index in the error message. */` |
|      18 | 5062 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5063 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5064 | `			"TypeError",` |
|       - | 5065 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5066 | `			ph7_type_name(apArg[0])` |
|       - | 5067 | `			);` |
|       - | 5068 | `	}` |
|      16 | 5069 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5070 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5071 | `			"TypeError",` |
|       - | 5072 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5073 | `			ph7_type_name(apArg[1])` |
|       - | 5074 | `			);` |
|       - | 5075 | `	}` |
|       - | 5076 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5077 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5078 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5079 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5080 | `		/* Length mismatch -> ValueError */` |
|       3 | 5081 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5082 | `			"ValueError",` |
|       - | 5083 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5084 | `			);` |
|       - | 5085 | `	}` |
|       - | 5086 | `	/* Create a new array */` |
|      11 | 5087 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5088 | `	if( pArray == 0 ){` |
|     ! 0 | 5089 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5090 | `		return PH7_OK;` |
|       - | 5091 | `	}` |
|       - | 5092 | `	/* Perform the requested operation */` |
|      11 | 5093 | `	pKe = pKey->pFirst;` |
|      11 | 5094 | `	pVe = pValue->pFirst;` |
|      33 | 5095 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5096 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5097 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5098 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5099 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5100 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5101 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5102 | `		 * original array must not be mutated. */` |
|      23 | 5103 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5104 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5105 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5106 | `			if( pTmpKey ){` |
|       5 | 5107 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5108 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5109 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5110 | `				pKeyCopy = pTmpKey;` |
|       2 | 5111 | `			}` |
|       2 | 5112 | `		}` |
|      23 | 5113 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5114 | `		/* Point to the next entry */` |
|      23 | 5115 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5116 | `		pVe = pVe->pPrev;` |
|      12 | 5117 | `	}` |
|       - | 5118 | `	/* Return the filled array */` |
|      11 | 5119 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5120 | `	return PH7_OK;` |
|      11 | 5121 |  |
|       - | 5122 | `/*` |
|       - | 5123 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5124 | ` *  Return an array with elements in reverse order.` |
|       - | 5125 | ` * Parameters` |
|       - | 5126 | ` *  $array` |
|       - | 5127 | ` *   The input array.` |
|       - | 5128 | ` *  $preserve_keys (optional)` |
|       - | 5129 | ` *   If set to TRUE keys are preserved.` |
|       - | 5130 | ` * Return` |
|       - | 5131 | ` *  The reversed array.` |
|       - | 5132 | ` */` |
|      20 | 5133 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5134 |  |
|       - | 5135 | `	ph7_hashmap_node *pEntry;` |
|       - | 5136 | `	ph7_hashmap *pSrc;` |
|       - | 5137 | `	ph7_value *pArray;` |
|       - | 5138 | `	int bPreserve;` |
|       - | 5139 | `	sxu32 n;` |
|      22 | 5140 | `	if( nArg < 1 ){` |
|       4 | 5141 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5142 | `			"ArgumentCountError",` |
|       - | 5143 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5144 | `			nArg` |
|       - | 5145 | `			);` |
|       - | 5146 | `	}` |
|       - | 5147 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5148 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5149 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5150 | `			"TypeError",` |
|       - | 5151 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5152 | `			ph7_type_name(apArg[0])` |
|       - | 5153 | `			);` |
|       - | 5154 | `	}` |
|      17 | 5155 | `	bPreserve = FALSE;` |
|      17 | 5156 | `	if( nArg > 1 ){` |
|       7 | 5157 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5158 | `	}` |
|       - | 5159 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5160 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5161 | `	/* Create a new array */` |
|      17 | 5162 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5163 | `	if( pArray == 0 ){` |
|     ! 0 | 5164 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5165 | `		return PH7_OK;` |
|       - | 5166 | `	}` |
|       - | 5167 | `	/* Perform the requested operation */` |
|      17 | 5168 | `	pEntry = pSrc->pLast;` |
|      55 | 5169 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5170 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5171 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5172 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5173 | `		/* Point to the previous entry */` |
|      39 | 5174 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5175 | `	}` |
|      17 | 5176 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5177 | `	return PH7_OK;` |
|      12 | 5178 |  |
|       - | 5179 | `/*` |
|       - | 5180 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5181 | ` *  Removes duplicate values from an array.` |
|       - | 5182 | ` * Parameters` |
|       - | 5183 | ` *  $array` |
|       - | 5184 | ` *   The input array.` |
|       - | 5185 | ` *  $flags` |
|       - | 5186 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5187 | ` *   behavior using these values:` |
|       - | 5188 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5189 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5190 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5191 | ` * Return` |
|       - | 5192 | ` *  The filtered array.` |
|       - | 5193 | ` */` |
|      24 | 5194 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5195 |  |
|       - | 5196 | `	ph7_hashmap_node *pEntry;` |
|       - | 5197 | `	ph7_value *pNeedle;` |
|       - | 5198 | `	ph7_hashmap *pSrc;` |
|       - | 5199 | `	ph7_value *pArray;` |
|       - | 5200 | `	int bStrict;` |
|       - | 5201 | `	sxi32 rc;` |
|       - | 5202 | `	sxu32 n;` |
|      26 | 5203 | `	if( nArg < 1 ){` |
|       - | 5204 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5205 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5206 | `			"ArgumentCountError",` |
|       - | 5207 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5208 | `			);` |
|       - | 5209 | `	}` |
|      24 | 5210 | `	if( nArg > 2 ){` |
|       - | 5211 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5212 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5213 | `			"ArgumentCountError",` |
|       - | 5214 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5215 | `			nArg` |
|       - | 5216 | `			);` |
|       - | 5217 | `	}` |
|       - | 5218 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5219 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5220 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5221 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5222 | `			"TypeError",` |
|       - | 5223 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5224 | `			ph7_type_name(apArg[0])` |
|       - | 5225 | `			);` |
|       - | 5226 | `	}` |
|      19 | 5227 | `	bStrict = FALSE;` |
|       - | 5228 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5229 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5230 | `	/* Create a new array */` |
|      19 | 5231 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5232 | `	if( pArray == 0 ){` |
|     ! 0 | 5233 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5234 | `		return PH7_OK;` |
|       - | 5235 | `	}` |
|       - | 5236 | `	/* Perform the requested operation */` |
|      19 | 5237 | `	pEntry = pSrc->pFirst;` |
|      83 | 5238 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5239 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5240 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5241 | `		if( pNeedle ){` |
|      65 | 5242 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5243 | `		}` |
|      65 | 5244 | `		if( rc != SXRET_OK ){` |
|       - | 5245 | `			/* Perform the insertion */` |
|      37 | 5246 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5247 | `		}` |
|       - | 5248 | `		/* Point to the next entry */` |
|      65 | 5249 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5250 | `	}` |
|       - | 5251 | `	/* Return the freshly created array */` |
|      19 | 5252 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5253 | `	return PH7_OK;` |
|      14 | 5254 |  |
|       - | 5255 | `/*` |
|       - | 5256 | ` * array array_flip(array $input)` |
|       - | 5257 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5258 | ` * Parameter` |
|       - | 5259 | ` *  $input` |
|       - | 5260 | ` *   Input array.` |
|       - | 5261 | ` * Return` |
|       - | 5262 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5263 | ` */` |
|      34 | 5264 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5265 |  |
|       - | 5266 | `	ph7_hashmap_node *pEntry;` |
|       - | 5267 | `	ph7_hashmap *pSrc;` |
|       - | 5268 | `	ph7_value *pArray;` |
|       - | 5269 | `	ph7_value *pKey;` |
|       - | 5270 | `	ph7_value sVal;` |
|       - | 5271 | `	sxu32 n;` |
|       - | 5272 |  |
|       - | 5273 | `	/* PHP requires exactly one argument */` |
|      36 | 5274 | `	if( nArg != 1 ){` |
|       - | 5275 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5276 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5277 | `			"ArgumentCountError",` |
|       - | 5278 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5279 | `			nArg` |
|       - | 5280 | `			);` |
|       - | 5281 | `	}` |
|       - | 5282 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5283 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5284 | `		/* Type mismatch -> TypeError */` |
|       7 | 5285 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5286 | `			"TypeError",` |
|       - | 5287 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5288 | `			ph7_type_name(apArg[0])` |
|       - | 5289 | `			);` |
|       - | 5290 | `	}` |
|       - | 5291 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5292 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5293 | `	/* Create a new array */` |
|      27 | 5294 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5295 | `	if( pArray == 0 ){` |
|     ! 0 | 5296 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5297 | `		return PH7_OK;` |
|       - | 5298 | `	}` |
|       - | 5299 | `	/* Start processing */` |
|      27 | 5300 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5301 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5302 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5303 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5304 | `		if( pKey ){` |
|       - | 5305 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5306 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5307 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5308 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5309 | `					);` |
|   22236 | 5310 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5311 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5312 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5313 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5314 | `				}else{` |
|       - | 5315 | `					SyString sStr;` |
|    2227 | 5316 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5317 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5318 | `				}` |
|       - | 5319 | `				/* Perform the insertion */` |
|   22227 | 5320 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5321 | `				/* Safely release the value because each inserted entry` |
|       - | 5322 | `				 * has its own private copy of the value.` |
|       - | 5323 | `				 */` |
|   22227 | 5324 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5325 | `			}else{` |
|       - | 5326 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5327 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5328 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5329 | `					);` |
|       - | 5330 | `			}` |
|   11118 | 5331 | `		}` |
|       - | 5332 | `		/* Point to the next entry */` |
|   22237 | 5333 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5334 | `	}` |
|       - | 5335 | `	/* Return the freshly created array */` |
|      27 | 5336 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5337 | `	return PH7_OK;` |
|      19 | 5338 |  |
|       - | 5339 | `/*` |
|       - | 5340 | ` * number array_sum(array $array )` |
|       - | 5341 | ` *  Calculate the sum of values in an array.` |
|       - | 5342 | ` * Parameters` |
|       - | 5343 | ` *  $array: The input array.` |
|       - | 5344 | ` * Return` |
|       - | 5345 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5346 | ` */` |
|      24 | 5347 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5348 |  |
|       - | 5349 | `	ph7_hashmap_node *pEntry;` |
|       - | 5350 | `	ph7_value *pObj;` |
|      25 | 5351 | `	double dSum = 0;` |
|       - | 5352 | `	sxu32 n;` |
|      25 | 5353 | `	pEntry = pMap->pFirst;` |
|      91 | 5354 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5355 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5356 | `		if( pObj ){` |
|      67 | 5357 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5358 | `				dSum += pObj->rVal;` |
|      53 | 5359 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5360 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5361 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5362 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5363 | `					double dv = 0;` |
|      13 | 5364 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5365 | `					dSum += dv;` |
|       7 | 5366 | `				}` |
|      12 | 5367 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5368 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5369 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5370 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5371 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5372 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5373 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5374 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5375 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5376 | `			}` |
|       - | 5377 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5378 | `		}` |
|       - | 5379 | `		/* Point to the next entry */` |
|      67 | 5380 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5381 | `	}` |
|       - | 5382 | `	/* Return sum */` |
|      25 | 5383 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5384 |  |
|      18 | 5385 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5386 |  |
|       - | 5387 | `	ph7_hashmap_node *pEntry;` |
|       - | 5388 | `	ph7_value *pObj;` |
|      20 | 5389 | `	sxi64 nSum = 0;` |
|       - | 5390 | `	sxu32 n;` |
|      20 | 5391 | `	pEntry = pMap->pFirst;` |
|      80 | 5392 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5393 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5394 | `		if( pObj ){` |
|      62 | 5395 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5396 | `				nSum += pObj->x.iVal;` |
|      36 | 5397 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5398 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5399 | `					sxi64 nv = 0;` |
|       5 | 5400 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5401 | `					nSum += nv;` |
|       3 | 5402 | `				}` |
|       8 | 5403 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5404 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5405 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5406 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5407 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5408 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5409 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5410 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5411 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5412 | `			}` |
|       - | 5413 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5414 | `		}` |
|       - | 5415 | `		/* Point to the next entry */` |
|      62 | 5416 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5417 | `	}` |
|       - | 5418 | `	/* Return sum */` |
|      20 | 5419 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5420 |  |
|       - | 5421 | `/* number array_sum(array $array )` |
|       - | 5422 | ` * (See block-coment above)` |
|       - | 5423 | ` */` |
|      52 | 5424 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5425 |  |
|       - | 5426 | `	ph7_hashmap_node *pEntry;` |
|       - | 5427 | `	ph7_hashmap *pMap;` |
|       - | 5428 | `	ph7_value *pObj;` |
|      54 | 5429 | `	int useDouble = 0;` |
|       - | 5430 | `	sxu32 n;` |
|       - | 5431 | `	/* PHP requires exactly one argument */` |
|      54 | 5432 | `	if( nArg != 1 ){` |
|       7 | 5433 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5434 | `			"ArgumentCountError",` |
|       - | 5435 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5436 | `			nArg` |
|       - | 5437 | `			);` |
|       - | 5438 | `	}` |
|       - | 5439 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5440 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5441 | `		/* Type mismatch -> TypeError */` |
|       7 | 5442 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5443 | `			"TypeError",` |
|       - | 5444 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5445 | `			ph7_type_name(apArg[0])` |
|       - | 5446 | `			);` |
|       - | 5447 | `	}` |
|      46 | 5448 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5449 | `	if( pMap->nEntry < 1 ){` |
|       - | 5450 | `		/* Nothing to compute,return 0 */` |
|       3 | 5451 | `		ph7_result_int(pCtx,0);` |
|       3 | 5452 | `		return PH7_OK;` |
|       - | 5453 | `	}` |
|       - | 5454 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5455 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5456 | `	 */` |
|      44 | 5457 | `	pEntry = pMap->pFirst;` |
|     112 | 5458 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5459 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5460 | `		if( pObj ){` |
|      94 | 5461 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5462 | `				useDouble = 1;` |
|      19 | 5463 | `				break;` |
|       - | 5464 | `			}` |
|      76 | 5465 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5466 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5467 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5468 | `				sxu32 i;` |
|      23 | 5469 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5470 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5471 | `						useDouble = 1;` |
|       7 | 5472 | `						break;` |
|       - | 5473 | `					}` |
|       6 | 5474 | `				}` |
|      13 | 5475 | `				if( useDouble ){` |
|       7 | 5476 | `					break;` |
|       - | 5477 | `				}` |
|       3 | 5478 | `			}` |
|      34 | 5479 | `		}` |
|      70 | 5480 | `		pEntry = pEntry->pPrev;` |
|      36 | 5481 | `	}` |
|      44 | 5482 | `	if( useDouble ){` |
|      25 | 5483 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5484 | `	}else{` |
|      20 | 5485 | `		Int64Sum(pCtx,pMap);` |
|       - | 5486 | `	}` |
|      44 | 5487 | `	return PH7_OK;` |
|      28 | 5488 |  |
|       - | 5489 | `/*` |
|       - | 5490 | ` * number array_product(array $array )` |
|       - | 5491 | ` *  Calculate the product of values in an array.` |
|       - | 5492 | ` * Parameters` |
|       - | 5493 | ` *  $array: The input array.` |
|       - | 5494 | ` * Return` |
|       - | 5495 | ` *  Returns the product of values as an integer or float.` |
|       - | 5496 | ` */` |
|     ! 0 | 5497 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5498 |  |
|       - | 5499 | `	ph7_hashmap_node *pEntry;` |
|       - | 5500 | `	ph7_value *pObj;` |
|       - | 5501 | `	double dProd;` |
|       - | 5502 | `	sxu32 n;` |
|     ! 0 | 5503 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5504 | `	dProd = 1;` |
|     ! 0 | 5505 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5506 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5507 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5508 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5509 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5510 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5511 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5512 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5513 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5514 | `					double dv = 0;` |
|     ! 0 | 5515 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5516 | `					dProd *= dv;` |
|     ! 0 | 5517 | `				}` |
|     ! 0 | 5518 | `			}` |
|     ! 0 | 5519 | `		}` |
|       - | 5520 | `		/* Point to the next entry */` |
|     ! 0 | 5521 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5522 | `	}` |
|       - | 5523 | `	/* Return product */` |
|     ! 0 | 5524 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5525 |  |
|     ! 0 | 5526 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5527 |  |
|       - | 5528 | `	ph7_hashmap_node *pEntry;` |
|       - | 5529 | `	ph7_value *pObj;` |
|       - | 5530 | `	sxi64 nProd;` |
|       - | 5531 | `	sxu32 n;` |
|     ! 0 | 5532 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5533 | `	nProd = 1;` |
|     ! 0 | 5534 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5535 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5536 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5537 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5538 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5539 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5540 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5541 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5542 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5543 | `					sxi64 nv = 0;` |
|     ! 0 | 5544 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5545 | `					nProd *= nv;` |
|     ! 0 | 5546 | `				}` |
|     ! 0 | 5547 | `			}` |
|     ! 0 | 5548 | `		}` |
|       - | 5549 | `		/* Point to the next entry */` |
|     ! 0 | 5550 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5551 | `	}` |
|       - | 5552 | `	/* Return product */` |
|     ! 0 | 5553 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5554 |  |
|       - | 5555 | `/* number array_product(array $array )` |
|       - | 5556 | ` * (See block-block comment above)` |
|       - | 5557 | ` */` |
|     ! 0 | 5558 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5559 |  |
|       - | 5560 | `	ph7_hashmap *pMap;` |
|       - | 5561 | `	ph7_value *pObj;` |
|     ! 0 | 5562 | `	if( nArg < 1 ){` |
|       - | 5563 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5564 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5565 | `		return PH7_OK;` |
|       - | 5566 | `	}` |
|       - | 5567 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5568 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5569 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5570 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5571 | `		return PH7_OK;` |
|       - | 5572 | `	}` |
|     ! 0 | 5573 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5574 | `	if( pMap->nEntry < 1 ){` |
|       - | 5575 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5576 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5577 | `		return PH7_OK;` |
|       - | 5578 | `	}` |
|       - | 5579 | `	/* If the first element is of type float,then perform floating` |
|       - | 5580 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5581 | `	 */` |
|     ! 0 | 5582 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5583 | `	if( pObj == 0 ){` |
|     ! 0 | 5584 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5585 | `		return PH7_OK;` |
|       - | 5586 | `	}` |
|     ! 0 | 5587 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5588 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5589 | `	}else{` |
|     ! 0 | 5590 | `		Int64Prod(pCtx,pMap);` |
|       - | 5591 | `	}` |
|     ! 0 | 5592 | `	return PH7_OK;` |
|     ! 0 | 5593 |  |
|       - | 5594 | `/*` |
|       - | 5595 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5596 | ` *  Pick one or more random entries out of an array.` |
|       - | 5597 | ` * Parameters` |
|       - | 5598 | ` * $input` |
|       - | 5599 | ` *  The input array.` |
|       - | 5600 | ` * $num_req` |
|       - | 5601 | ` *  Specifies how many entries you want to pick.` |
|       - | 5602 | ` * Return` |
|       - | 5603 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5604 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5605 | ` *  NULL is returned on failure.` |
|       - | 5606 | ` */` |
|       6 | 5607 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5608 |  |
|       - | 5609 | `	ph7_hashmap_node *pNode;` |
|       - | 5610 | `	ph7_hashmap *pMap;` |
|       7 | 5611 | `	int nItem = 1;` |
|       7 | 5612 | `	if( nArg < 1 ){` |
|       - | 5613 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5614 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5615 | `		return PH7_OK;` |
|       - | 5616 | `	}` |
|       - | 5617 | `	/* Make sure we are dealing with an array */` |
|       7 | 5618 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5619 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5620 | `		return PH7_OK;` |
|       - | 5621 | `	}` |
|       - | 5622 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5623 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5624 | `	if(pMap->nEntry < 1 ){` |
|       - | 5625 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5626 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5627 | `		return PH7_OK;` |
|       - | 5628 | `	}` |
|       7 | 5629 | `	if( nArg > 1 ){` |
|       3 | 5630 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5631 | `	}` |
|       7 | 5632 | `	if( nItem < 2 ){` |
|       - | 5633 | `		sxu32 nEntry;` |
|       - | 5634 | `		/* Select a random number */` |
|       5 | 5635 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5636 | `		/* Extract the desired entry.` |
|       - | 5637 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5638 | `		 */` |
|       5 | 5639 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 5640 | `			pNode = pMap->pLast;` |
|       3 | 5641 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 5642 | `			if( nEntry > 1 ){` |
|     ! 0 | 5643 | `				for(;;){` |
|     ! 0 | 5644 | `					if( nEntry == 0 ){` |
|     ! 0 | 5645 | `						break;` |
|       - | 5646 | `					}` |
|       - | 5647 | `					/* Point to the previous entry */` |
|     ! 0 | 5648 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5649 | `					nEntry--;` |
|     ! 0 | 5650 | `				}` |
|     ! 0 | 5651 | `			}` |
|       2 | 5652 | `		}else{` |
|       3 | 5653 | `			pNode = pMap->pFirst;` |
|       1 | 5654 | `			for(;;){` |
|       3 | 5655 | `				if( nEntry == 0 ){` |
|       3 | 5656 | `					break;` |
|       - | 5657 | `				}` |
|       - | 5658 | `				/* Point to the next entry */` |
|       1 | 5659 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 5660 | `				nEntry--;` |
|       1 | 5661 | `			}` |
|       - | 5662 | `		}` |
|       5 | 5663 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5664 | `			/* Int key */` |
|       3 | 5665 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5666 | `		}else{` |
|       - | 5667 | `			/* Blob key */` |
|       3 | 5668 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5669 | `		}` |
|       3 | 5670 | `	}else{` |
|       - | 5671 | `		ph7_value sKey,*pArray;` |
|       - | 5672 | `		ph7_hashmap *pDest;` |
|       - | 5673 | `		/* Create a new array */` |
|       3 | 5674 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5675 | `		if( pArray == 0 ){` |
|     ! 0 | 5676 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5677 | `			return PH7_OK;` |
|       - | 5678 | `		}` |
|       - | 5679 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5680 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5681 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5682 | `		/* Copy the first n items */` |
|       3 | 5683 | `		pNode = pMap->pFirst;` |
|       3 | 5684 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5685 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5686 | `		}` |
|       7 | 5687 | `		while( nItem > 0){` |
|       5 | 5688 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5689 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5690 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5691 | `			/* Point to the next entry */` |
|       5 | 5692 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5693 | `			nItem--;` |
|       1 | 5694 | `		}` |
|       - | 5695 | `		/* Shuffle the array */` |
|       3 | 5696 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5697 | `		/* Rehash node */` |
|       3 | 5698 | `		HashmapSortRehash(pDest);` |
|       - | 5699 | `		/* Return the random array */` |
|       3 | 5700 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5701 | `	}` |
|       7 | 5702 | `	return PH7_OK;` |
|       4 | 5703 |  |
|       - | 5704 | `/*` |
|       - | 5705 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5706 | ` *  Split an array into chunks.` |
|       - | 5707 | ` * Parameters` |
|       - | 5708 | ` * $input` |
|       - | 5709 | ` *   The array to work on` |
|       - | 5710 | ` * $size` |
|       - | 5711 | ` *   The size of each chunk` |
|       - | 5712 | ` * $preserve_keys` |
|       - | 5713 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5714 | ` *   the chunk numerically.` |
|       - | 5715 | ` * Return` |
|       - | 5716 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5717 | ` *  zero, with each dimension containing size elements.` |
|       - | 5718 | ` */` |
|      42 | 5719 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5720 |  |
|       - | 5721 | `	ph7_value *pArray,*pChunk;` |
|       - | 5722 | `	ph7_hashmap_node *pEntry;` |
|       - | 5723 | `	ph7_hashmap *pMap;` |
|       - | 5724 | `	int bPreserve;` |
|       - | 5725 | `	sxu32 nChunk;` |
|       - | 5726 | `	sxu32 nSize;` |
|       - | 5727 | `	sxu32 n;` |
|       - | 5728 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5729 | `	if( nArg < 2 ){` |
|       - | 5730 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5731 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5732 | `			"ArgumentCountError",` |
|       - | 5733 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5734 | `			nArg` |
|       - | 5735 | `			);` |
|       - | 5736 | `	}` |
|      42 | 5737 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5738 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5739 | `			"TypeError",` |
|       - | 5740 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5741 | `			ph7_type_name(apArg[0])` |
|       - | 5742 | `			);` |
|       - | 5743 | `	}` |
|       - | 5744 | `	/* Create a new array */` |
|      40 | 5745 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5746 | `	if( pArray == 0 ){` |
|     ! 0 | 5747 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5748 | `		return PH7_OK;` |
|       - | 5749 | `	}` |
|       - | 5750 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5751 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5752 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5753 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5754 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5755 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5756 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5757 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5758 | `			"TypeError",` |
|       - | 5759 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5760 | `			ph7_type_name(apArg[1])` |
|       - | 5761 | `			);` |
|       - | 5762 | `	}` |
|       - | 5763 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5764 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5765 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5766 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5767 | `		int len;` |
|       3 | 5768 | `		sxu8 bReal = FALSE;` |
|       3 | 5769 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5770 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5771 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5772 | `				"TypeError",` |
|       - | 5773 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5774 | `				);` |
|       - | 5775 | `		}` |
|     ! 0 | 5776 | `		if( bReal ){` |
|       - | 5777 | `			/* float-string -> warn but allow */` |
|     ! 0 | 5778 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5779 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 5780 | `				zStr` |
|       - | 5781 | `				);` |
|     ! 0 | 5782 | `		}` |
|     ! 0 | 5783 | `	}` |
|       - | 5784 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 5785 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 5786 | `	 * later via ph7_value_to_int. */` |
|      38 | 5787 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5788 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5789 | `		sxi64 i = (sxi64)d;` |
|       3 | 5790 | `		if( d != (double)i ){` |
|       4 | 5791 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5792 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 5793 | `				d` |
|       - | 5794 | `				);` |
|       1 | 5795 | `		}` |
|       1 | 5796 | `	}` |
|       - | 5797 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5798 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5799 | `	{` |
|      38 | 5800 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 5801 | `		if( nSizeSigned < 1 ){` |
|       - | 5802 | `			/* size <= 0 -> ValueError */` |
|       5 | 5803 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5804 | `				"ValueError",` |
|       - | 5805 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5806 | `				);` |
|       - | 5807 | `		}` |
|      34 | 5808 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5809 | `	}` |
|      34 | 5810 | `	if( nSize >= pMap->nEntry ){` |
|       - | 5811 | `		/* Return the whole array */` |
|       3 | 5812 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 5813 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5814 | `		return PH7_OK;` |
|       - | 5815 | `	}` |
|      32 | 5816 | `	bPreserve = 0;` |
|      32 | 5817 | `	if( nArg > 2 ){` |
|       - | 5818 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5819 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5820 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5821 | `		 * normally, matching PHP behaviour. */` |
|      45 | 5822 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 5823 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 5824 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 5825 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5826 | `				"TypeError",` |
|       - | 5827 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 5828 | `				ph7_type_name(apArg[2])` |
|       - | 5829 | `				);` |
|       - | 5830 | `		}` |
|      21 | 5831 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 5832 | `	}` |
|       - | 5833 | `	/* Start processing */` |
|      27 | 5834 | `	pEntry = pMap->pFirst;` |
|      27 | 5835 | `	nChunk = 0;` |
|      27 | 5836 | `	pChunk = 0;` |
|      27 | 5837 | `	n = pMap->nEntry;` |
|      56 | 5838 | `	for( ;; ){` |
|     113 | 5839 | `		if( n < 1 ){` |
|       - | 5840 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5841 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5842 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5843 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5844 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5845 | `			 * exists. */` |
|      27 | 5846 | `			if( pChunk ){` |
|      27 | 5847 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 5848 | `			}` |
|      27 | 5849 | `			break;` |
|       - | 5850 | `		}` |
|      87 | 5851 | `		if( nChunk < 1 ){` |
|      71 | 5852 | `			if( pChunk ){` |
|       - | 5853 | `				/* Put the first chunk */` |
|      45 | 5854 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 5855 | `			}` |
|       - | 5856 | `			/* Create a new dimension */` |
|      71 | 5857 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5858 | `												   * will be automatically released as soon we return` |
|       - | 5859 | `												   * from this function */` |
|      71 | 5860 | `			if( pChunk == 0 ){` |
|     ! 0 | 5861 | `				break;` |
|       - | 5862 | `			}` |
|      71 | 5863 | `			nChunk = nSize;` |
|      35 | 5864 | `		}` |
|       - | 5865 | `		/* Insert the entry */` |
|      87 | 5866 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5867 | `		/* Point to the next entry */` |
|      87 | 5868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 5869 | `		nChunk--;` |
|      87 | 5870 | `		n--;` |
|       1 | 5871 | `	}` |
|       - | 5872 | `	/* Return the multidimensional array */` |
|      27 | 5873 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5874 | `	return PH7_OK;` |
|      23 | 5875 |  |
|       - | 5876 | `/*` |
|       - | 5877 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5878 | ` *  Pad array to the specified length with a value.` |
|       - | 5879 | ` * $input` |
|       - | 5880 | ` *   Initial array of values to pad.` |
|       - | 5881 | ` * $pad_size` |
|       - | 5882 | ` *   New size of the array.` |
|       - | 5883 | ` * $pad_value` |
|       - | 5884 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5885 | ` */` |
|      28 | 5886 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5887 |  |
|       - | 5888 | `	ph7_hashmap *pMap;` |
|       - | 5889 | `	ph7_value *pArray;` |
|       - | 5890 | `	int nEntry;` |
|      30 | 5891 | `	if( nArg != 3 ){` |
|      10 | 5892 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5893 | `			"ArgumentCountError",` |
|       - | 5894 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 5895 | `			nArg` |
|       - | 5896 | `			);` |
|       - | 5897 | `	}` |
|      24 | 5898 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5899 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5900 | `			"TypeError",` |
|       - | 5901 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5902 | `			ph7_type_name(apArg[0])` |
|       - | 5903 | `			);` |
|       - | 5904 | `	}` |
|       - | 5905 | `	/* Create a new array */` |
|      21 | 5906 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 5907 | `	if( pArray == 0 ){` |
|     ! 0 | 5908 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5909 | `		return PH7_OK;` |
|       - | 5910 | `	}` |
|       - | 5911 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 5912 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5913 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 5914 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 5915 | `	if( nEntry < 0 ){` |
|       9 | 5916 | `		nEntry = -nEntry;` |
|       9 | 5917 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 5918 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5919 | `			/* Insert given items first */` |
|      17 | 5920 | `			while( nEntry > 0 ){` |
|      13 | 5921 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 5922 | `				nEntry--;` |
|       1 | 5923 | `			}` |
|       - | 5924 | `			/* Merge the two arrays */` |
|       5 | 5925 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5926 | `		}else{` |
|       5 | 5927 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5928 | `		}` |
|      17 | 5929 | `	}else if( nEntry > 0 ){` |
|      11 | 5930 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 5931 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5932 | `			/* Merge the two arrays first */` |
|       7 | 5933 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5934 | `			/* Insert given items */` |
|      25 | 5935 | `			while( nEntry > 0 ){` |
|      19 | 5936 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 5937 | `				nEntry--;` |
|       1 | 5938 | `			}` |
|       4 | 5939 | `		}else{` |
|       5 | 5940 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5941 | `		}` |
|       6 | 5942 | `	}else{` |
|       - | 5943 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 5944 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5945 | `	}` |
|       - | 5946 | `	/* Return the new array */` |
|      21 | 5947 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 5948 | `	return PH7_OK;` |
|      16 | 5949 |  |
|       - | 5950 | `/*` |
|       - | 5951 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5952 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5953 | ` * Parameters` |
|       - | 5954 | ` * $array` |
|       - | 5955 | ` *   The array in which elements are replaced.` |
|       - | 5956 | ` * $array1` |
|       - | 5957 | ` *   The array from which elements will be extracted.` |
|       - | 5958 | ` * ....` |
|       - | 5959 | ` *  More arrays from which elements will be extracted.` |
|       - | 5960 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5961 | ` * Return` |
|       - | 5962 | ` *  Returns an array.` |
|       - | 5963 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 5964 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 5965 | ` */` |
|      22 | 5966 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5967 |  |
|       - | 5968 | `	ph7_hashmap *pMap;` |
|       - | 5969 | `	ph7_value *pArray;` |
|       - | 5970 | `	int i;` |
|      24 | 5971 | `	if( nArg < 1 ){` |
|       3 | 5972 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5973 | `			"ArgumentCountError",` |
|       - | 5974 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 5975 | `			);` |
|       - | 5976 | `	}` |
|      22 | 5977 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5978 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5979 | `			"TypeError",` |
|       - | 5980 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5981 | `			ph7_type_name(apArg[0])` |
|       - | 5982 | `			);` |
|       - | 5983 | `	}` |
|       - | 5984 | `	/* Create a new array */` |
|      20 | 5985 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 5986 | `	if( pArray == 0 ){` |
|     ! 0 | 5987 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5988 | `		return PH7_OK;` |
|       - | 5989 | `	}` |
|       - | 5990 | `	/* Overwrite from the first array */` |
|      20 | 5991 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 5992 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5993 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 5994 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5995 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 5996 | `			/* Type mismatch -> TypeError */` |
|       4 | 5997 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5998 | `				"TypeError",` |
|       - | 5999 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6000 | `				i + 1,` |
|       2 | 6001 | `				ph7_type_name(apArg[i])` |
|       - | 6002 | `				);` |
|       - | 6003 | `		}` |
|       - | 6004 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6005 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6006 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6007 | `	}` |
|       - | 6008 | `	/* Return the new array */` |
|      17 | 6009 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6010 | `	return PH7_OK;` |
|      13 | 6011 |  |
|       - | 6012 | `/*` |
|       - | 6013 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6014 | ` *  Filters elements of an array using a callback function.` |
|       - | 6015 | ` * Parameters` |
|       - | 6016 | ` *  $input` |
|       - | 6017 | ` *    The array to iterate over` |
|       - | 6018 | ` * $callback` |
|       - | 6019 | ` *    The callback function to use` |
|       - | 6020 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6021 | ` *    will be removed.` |
|       - | 6022 | ` * Return` |
|       - | 6023 | ` *  The filtered array.` |
|       - | 6024 | ` */` |
|      18 | 6025 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6026 |  |
|       - | 6027 | `	ph7_hashmap_node *pEntry;` |
|       - | 6028 | `	ph7_hashmap *pMap;` |
|       - | 6029 | `	ph7_value *pArray;` |
|       - | 6030 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6031 | `	ph7_value *pValue;` |
|       - | 6032 | `	sxi32 rc;` |
|       - | 6033 | `	int keep;` |
|       - | 6034 | `	sxu32 n;` |
|      20 | 6035 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6036 | `		/* Invalid arguments,return NULL */` |
|       5 | 6037 | `		ph7_result_null(pCtx);` |
|       5 | 6038 | `		return PH7_OK;` |
|       - | 6039 | `	}` |
|       - | 6040 | `	/* Create a new array */` |
|      16 | 6041 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 6042 | `	if( pArray == 0 ){` |
|     ! 0 | 6043 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6044 | `		return PH7_OK;` |
|       - | 6045 | `	}` |
|       - | 6046 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 6047 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 6048 | `	pEntry = pMap->pFirst;` |
|      16 | 6049 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 6050 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6051 | `	/* Perform the requested operation */` |
|      66 | 6052 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6053 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 6054 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 6055 | `		if( pValue == 0 ){` |
|       - | 6056 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6057 | `			keep = FALSE;` |
|      54 | 6058 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6059 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6060 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6061 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 6062 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6063 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6064 | `					int len;` |
|       3 | 6065 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6066 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6067 | `						"TypeError",` |
|       - | 6068 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6069 | `						zName` |
|       - | 6070 | `						);` |
|     ! 0 | 6071 | `				}else{` |
|     ! 0 | 6072 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6073 | `						"TypeError",` |
|       - | 6074 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6075 | `						ph7_type_name(apArg[1])` |
|       - | 6076 | `						);` |
|       - | 6077 | `				}` |
|       - | 6078 | `			}` |
|      23 | 6079 | `			keep = FALSE;` |
|      23 | 6080 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 6081 | `			if( rc == SXRET_OK ){` |
|       - | 6082 | `				/* Perform a boolean cast */` |
|      23 | 6083 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6084 | `			}` |
|      23 | 6085 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6086 | `		}else{` |
|       - | 6087 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6088 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6089 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6090 | `			 */` |
|      29 | 6091 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6092 | `		}` |
|      51 | 6093 | `		if( keep ){` |
|       - | 6094 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6095 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6096 | `		}` |
|       - | 6097 | `		/* Point to the next entry */` |
|      51 | 6098 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6099 | `	}` |
|      13 | 6100 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6101 | `	return PH7_OK;` |
|      11 | 6102 |  |
|       - | 6103 | `/*` |
|       - | 6104 | ` * array array_map(?callable $callback, array $array)` |
|       - | 6105 | ` *  Applies the callback to the elements of the given array.` |
|       - | 6106 | ` * Parameters` |
|       - | 6107 | ` *  $callback` |
|       - | 6108 | ` *   A callable to run for each element in the array, or NULL for the` |
|       - | 6109 | ` *   identity function (returns the array unchanged).` |
|       - | 6110 | ` *  $array` |
|       - | 6111 | ` *   An array to run through the callback function.` |
|       - | 6112 | ` * Return` |
|       - | 6113 | ` *  Returns an array containing the results of applying the callback` |
|       - | 6114 | ` *  function to each element of $array.` |
|       - | 6115 | ` */` |
|      28 | 6116 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6117 |  |
|       - | 6118 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6119 | `	ph7_hashmap_node *pEntry;` |
|       - | 6120 | `	ph7_hashmap *pMap;` |
|       - | 6121 | `	int bNullCallback;` |
|       - | 6122 | `	sxu32 n;` |
|      30 | 6123 | `	if( nArg < 2 ){` |
|       7 | 6124 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6125 | `			"ArgumentCountError",` |
|       - | 6126 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6127 | `			nArg` |
|       - | 6128 | `			);` |
|       - | 6129 | `	}` |
|      26 | 6130 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 6131 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6132 | `			"TypeError",` |
|       - | 6133 | `			"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6134 | `			ph7_type_name(apArg[1])` |
|       - | 6135 | `			);` |
|       - | 6136 | `	}` |
|      24 | 6137 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      24 | 6138 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6139 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6140 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6141 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6142 | `				"TypeError",` |
|       - | 6143 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6144 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6145 | `				zFunc` |
|       - | 6146 | `				);` |
|       - | 6147 | `		}` |
|       3 | 6148 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6149 | `			"TypeError",` |
|       - | 6150 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6151 | `			"no array or string given"` |
|       - | 6152 | `			);` |
|       - | 6153 | `	}` |
|       - | 6154 | `	/* Create a new array */` |
|      19 | 6155 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 6156 | `	if( pArray == 0 ){` |
|     ! 0 | 6157 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6158 | `		return PH7_OK;` |
|       - | 6159 | `	}` |
|       - | 6160 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6161 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      19 | 6162 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 6163 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6164 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 6165 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 6166 | `	/* Perform the requested operation */` |
|      19 | 6167 | `	pEntry = pMap->pFirst;` |
|      53 | 6168 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6169 | `		/* Extract the node value */` |
|      35 | 6170 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      35 | 6171 | `		if( pValue ){` |
|       - | 6172 | `			/* Extract the node key */` |
|      35 | 6173 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      35 | 6174 | `			if( bNullCallback ){` |
|       - | 6175 | `				/* NULL callback: identity function, keep original value */` |
|      11 | 6176 | `				ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6177 | `			}else{` |
|       - | 6178 | `				/* Invoke the supplied callback */` |
|      25 | 6179 | `				PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 6180 | `				/* Insert the callback return value */` |
|      25 | 6181 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6182 | `			}` |
|      35 | 6183 | `			PH7_MemObjRelease(&sKey);` |
|      35 | 6184 | `			PH7_MemObjRelease(&sResult);` |
|      17 | 6185 | `		}` |
|       - | 6186 | `		/* Point to the next entry */` |
|      35 | 6187 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      18 | 6188 | `	}` |
|      19 | 6189 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 6190 | `	return PH7_OK;` |
|      16 | 6191 |  |
|       - | 6192 | `/*` |
|       - | 6193 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6194 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6195 | ` * Parameters` |
|       - | 6196 | ` *  $array` |
|       - | 6197 | ` *   The input array.` |
|       - | 6198 | ` *  $callback` |
|       - | 6199 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6200 | ` *  $initial` |
|       - | 6201 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6202 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6203 | ` * Return` |
|       - | 6204 | ` *  Returns the resulting value.` |
|       - | 6205 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6206 | ` */` |
|      30 | 6207 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6208 |  |
|       - | 6209 | `	ph7_hashmap_node *pEntry;` |
|       - | 6210 | `	ph7_hashmap *pMap;` |
|       - | 6211 | `	ph7_value *pValue;` |
|       - | 6212 | `	ph7_value sResult;` |
|       - | 6213 | `	sxu32 n;` |
|      32 | 6214 | `	if( nArg < 2 ){` |
|       7 | 6215 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6216 | `			"ArgumentCountError",` |
|       - | 6217 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6218 | `			nArg` |
|       - | 6219 | `			);` |
|       - | 6220 | `	}` |
|      28 | 6221 | `	if( nArg > 3 ){` |
|       4 | 6222 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6223 | `			"ArgumentCountError",` |
|       - | 6224 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6225 | `			nArg` |
|       - | 6226 | `			);` |
|       - | 6227 | `	}` |
|      26 | 6228 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6229 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6230 | `			"TypeError",` |
|       - | 6231 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6232 | `			ph7_type_name(apArg[0])` |
|       - | 6233 | `			);` |
|       - | 6234 | `	}` |
|      24 | 6235 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6236 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6237 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6238 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6239 | `				"TypeError",` |
|       - | 6240 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6241 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6242 | `				zFunc` |
|       - | 6243 | `				);` |
|       - | 6244 | `		}` |
|       7 | 6245 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6246 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6247 | `				"TypeError",` |
|       - | 6248 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6249 | `				"array callback must have exactly two members"` |
|       - | 6250 | `				);` |
|       - | 6251 | `		}` |
|       5 | 6252 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6253 | `			"TypeError",` |
|       - | 6254 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6255 | `			"no array or string given"` |
|       - | 6256 | `			);` |
|       - | 6257 | `	}` |
|       - | 6258 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 6259 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6260 | `	/* Assume a NULL initial value */` |
|      15 | 6261 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      15 | 6262 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      15 | 6263 | `	if( nArg > 2 ){` |
|       - | 6264 | `		/* Set the initial value */` |
|      11 | 6265 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6266 | `	}` |
|       - | 6267 | `	/* Perform the requested operation */` |
|      15 | 6268 | `	pEntry = pMap->pFirst;` |
|      43 | 6269 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6270 | `		/* Extract the node value */` |
|      29 | 6271 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6272 | `		/* Invoke the supplied callback */` |
|      29 | 6273 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6274 | `		/* Point to the next entry */` |
|      29 | 6275 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6276 | `	}` |
|      15 | 6277 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6278 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6279 | `	return PH7_OK;` |
|      17 | 6280 |  |
|       - | 6281 | `/*` |
|       - | 6282 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6283 | ` *  Apply a user function to every member of an array.` |
|       - | 6284 | ` * Parameters` |
|       - | 6285 | ` *  $array` |
|       - | 6286 | ` *   The input array.` |
|       - | 6287 | ` *  $funcname` |
|       - | 6288 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6289 | ` *   the first, and the key/index second.` |
|       - | 6290 | ` * Note:` |
|       - | 6291 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6292 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6293 | ` *  be made in the original array itself.` |
|       - | 6294 | ` *  $userdata` |
|       - | 6295 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6296 | ` *   to the callback funcname.` |
|       - | 6297 | ` * Return` |
|       - | 6298 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6299 | ` */` |
|      36 | 6300 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6301 |  |
|       - | 6302 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6303 | `	ph7_hashmap_node *pEntry;` |
|       - | 6304 | `	ph7_hashmap *pMap;` |
|       - | 6305 | `	sxu32 n;` |
|      38 | 6306 | `	if( nArg < 2 ){` |
|       7 | 6307 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6308 | `			"ArgumentCountError",` |
|       - | 6309 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6310 | `			nArg` |
|       - | 6311 | `			);` |
|       - | 6312 | `	}` |
|      34 | 6313 | `	if( nArg > 3 ){` |
|       4 | 6314 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6315 | `			"ArgumentCountError",` |
|       - | 6316 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6317 | `			nArg` |
|       - | 6318 | `			);` |
|       - | 6319 | `	}` |
|      32 | 6320 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6321 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6322 | `			"TypeError",` |
|       - | 6323 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6324 | `			ph7_type_name(apArg[0])` |
|       - | 6325 | `			);` |
|       - | 6326 | `	}` |
|      30 | 6327 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6328 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6329 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6330 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6331 | `				"TypeError",` |
|       - | 6332 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6333 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6334 | `				zFunc` |
|       - | 6335 | `				);` |
|       - | 6336 | `		}` |
|       9 | 6337 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6338 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6339 | `				"TypeError",` |
|       - | 6340 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6341 | `				"array callback must have exactly two members"` |
|       - | 6342 | `				);` |
|       - | 6343 | `		}` |
|       5 | 6344 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6345 | `			"TypeError",` |
|       - | 6346 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6347 | `			"no array or string given"` |
|       - | 6348 | `			);` |
|       - | 6349 | `	}` |
|      19 | 6350 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6351 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6352 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      19 | 6353 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6354 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6355 | `	/* Perform the desired operation */` |
|      19 | 6356 | `	pEntry = pMap->pFirst;` |
|      59 | 6357 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6358 | `		/* Extract the node value */` |
|      41 | 6359 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      41 | 6360 | `		if( pValue ){` |
|       - | 6361 | `			/* Extract the entry key */` |
|      41 | 6362 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6363 | `			/* Invoke the supplied callback */` |
|      41 | 6364 | `			PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      41 | 6365 | `			PH7_MemObjRelease(&sKey);` |
|      20 | 6366 | `		}` |
|       - | 6367 | `		/* Point to the next entry */` |
|      41 | 6368 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6369 | `	}` |
|       - | 6370 | `	/* All done, return TRUE */` |
|      19 | 6371 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6372 | `	return PH7_OK;` |
|      20 | 6373 |  |
|       - | 6374 | `/*` |
|       - | 6375 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6376 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6377 | ` */` |
|      22 | 6378 | `static void HashmapWalkRecursive(` |
|       - | 6379 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6380 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6381 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6382 | `	int iNest             /* Nesting level */` |
|       - | 6383 | `	)` |
|       1 | 6384 |  |
|       - | 6385 | `	ph7_hashmap_node *pEntry;` |
|       - | 6386 | `	ph7_value *pValue,sKey;` |
|       - | 6387 | `	sxu32 n;` |
|       - | 6388 | `	/* Iterate through hashmap entries */` |
|      23 | 6389 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6390 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6391 | `	pEntry = pMap->pFirst;` |
|      59 | 6392 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6393 | `		/* Extract the node value */` |
|      37 | 6394 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6395 | `		if( pValue ){` |
|      37 | 6396 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6397 | `				if( iNest < 32 ){` |
|       - | 6398 | `					/* Recurse */` |
|      11 | 6399 | `					iNest++;` |
|      11 | 6400 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6401 | `					iNest--;` |
|       5 | 6402 | `				}` |
|       6 | 6403 | `			}else{` |
|       - | 6404 | `				/* Extract the node key */` |
|      27 | 6405 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6406 | `				/* Invoke the supplied callback */` |
|      27 | 6407 | `				PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6408 | `				PH7_MemObjRelease(&sKey);` |
|       - | 6409 | `			}` |
|      18 | 6410 | `		}` |
|       - | 6411 | `		/* Point to the next entry */` |
|      37 | 6412 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6413 | `	}` |
|      23 | 6414 |  |
|       - | 6415 | `/*` |
|       - | 6416 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6417 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6418 | ` * Parameters` |
|       - | 6419 | ` *  $array` |
|       - | 6420 | ` *   The input array.` |
|       - | 6421 | ` *  $funcname` |
|       - | 6422 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6423 | ` *   the first, and the key/index second.` |
|       - | 6424 | ` * Note:` |
|       - | 6425 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6426 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6427 | ` *  be made in the original array itself.` |
|       - | 6428 | ` *  $userdata` |
|       - | 6429 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6430 | ` *   to the callback funcname.` |
|       - | 6431 | ` * Return` |
|       - | 6432 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6433 | ` */` |
|      30 | 6434 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6435 |  |
|       - | 6436 | `	ph7_hashmap *pMap;` |
|      32 | 6437 | `	if( nArg < 2 ){` |
|       7 | 6438 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6439 | `			"ArgumentCountError",` |
|       - | 6440 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 6441 | `			nArg` |
|       - | 6442 | `			);` |
|       - | 6443 | `	}` |
|      28 | 6444 | `	if( nArg > 3 ){` |
|       4 | 6445 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6446 | `			"ArgumentCountError",` |
|       - | 6447 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 6448 | `			nArg` |
|       - | 6449 | `			);` |
|       - | 6450 | `	}` |
|      26 | 6451 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6452 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6453 | `			"TypeError",` |
|       - | 6454 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6455 | `			ph7_type_name(apArg[0])` |
|       - | 6456 | `			);` |
|       - | 6457 | `	}` |
|      24 | 6458 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6459 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6460 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6461 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6462 | `				"TypeError",` |
|       - | 6463 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6464 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6465 | `				zFunc` |
|       - | 6466 | `				);` |
|       - | 6467 | `		}` |
|       9 | 6468 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6469 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6470 | `				"TypeError",` |
|       - | 6471 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6472 | `				"array callback must have exactly two members"` |
|       - | 6473 | `				);` |
|       - | 6474 | `		}` |
|       5 | 6475 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6476 | `			"TypeError",` |
|       - | 6477 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6478 | `			"no array or string given"` |
|       - | 6479 | `			);` |
|       - | 6480 | `	}` |
|       - | 6481 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6482 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6483 | `	/* Perform the desired operation */` |
|      13 | 6484 | `	HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6485 | `	/* All done, return TRUE */` |
|      13 | 6486 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6487 | `	return PH7_OK;` |
|      17 | 6488 |  |
|       - | 6489 | `/*` |
|       - | 6490 | ` * Table of hashmap functions.` |
|       - | 6491 | ` */` |
|       - | 6492 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6493 | `	{"count",             ph7_hashmap_count },` |
|       - | 6494 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6495 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6496 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6497 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6498 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6499 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6500 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6501 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6502 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6503 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6504 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6505 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6506 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6507 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6508 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6509 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6510 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6511 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6512 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6513 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6514 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6515 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6516 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6517 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6518 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6519 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6520 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6521 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6522 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6523 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6524 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6525 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6526 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6527 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6528 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6529 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6530 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6531 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6532 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6533 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6534 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6535 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6536 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6537 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6538 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6539 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6540 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6541 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6542 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6543 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6544 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6545 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6546 | `	{"current",           ph7_hashmap_current },` |
|       - | 6547 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6548 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6549 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6550 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6551 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6552 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6553 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6554 | `};` |
|       - | 6555 | `/*` |
|       - | 6556 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6557 | ` */` |
|    1620 | 6558 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6559 |  |
|       - | 6560 | `	sxu32 n;` |
|  100442 | 6561 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|   98822 | 6562 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   49412 | 6563 | `	}` |
|    1622 | 6564 |  |
|       - | 6565 | `/*` |
|       - | 6566 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6567 | ` * the BLOB given as the first argument.` |
|       - | 6568 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6569 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6570 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6571 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6572 | ` */` |
|      26 | 6573 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6574 |  |
|       - | 6575 | `	ph7_hashmap_node *pEntry;` |
|       - | 6576 | `	ph7_value *pObj;` |
|      28 | 6577 | `	sxu32 n = 0;` |
|       - | 6578 | `	int isRef;` |
|       - | 6579 | `	sxi32 rc;` |
|       - | 6580 | `	int i;` |
|      28 | 6581 | `	if( nDepth > 31 ){` |
|       - | 6582 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6583 | `		/* Nesting limit reached */` |
|     ! 0 | 6584 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6585 | `		if( ShowType ){` |
|     ! 0 | 6586 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6587 | `		}` |
|     ! 0 | 6588 | `		return SXERR_LIMIT;` |
|       - | 6589 | `	}` |
|       - | 6590 | `	/* Point to the first inserted entry */` |
|      28 | 6591 | `	pEntry = pMap->pFirst;` |
|      28 | 6592 | `	rc = SXRET_OK;` |
|      28 | 6593 | `	if( !ShowType ){` |
|      15 | 6594 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6595 | `	}` |
|       - | 6596 | `	/* Total entries */` |
|      28 | 6597 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6598 | `#ifdef __WINNT__` |
|       2 | 6599 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6600 | `#else` |
|      26 | 6601 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6602 | `#endif` |
|      62 | 6603 | `	for(;;){` |
|     126 | 6604 | `		if( n >= pMap->nEntry ){` |
|      28 | 6605 | `			break;` |
|       - | 6606 | `		}` |
|     198 | 6607 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 6608 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 6609 | `		}` |
|       - | 6610 | `		/* Dump key */` |
|     100 | 6611 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 6612 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 6613 | `		}else{` |
|     101 | 6614 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6615 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6616 | `		}` |
|       - | 6617 | `#ifdef __WINNT__` |
|       2 | 6618 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6619 | `#else` |
|      98 | 6620 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6621 | `#endif` |
|       - | 6622 | `		/* Dump node value */` |
|     100 | 6623 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 6624 | `		isRef = 0;` |
|     100 | 6625 | `		if( pObj ){` |
|     100 | 6626 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6627 | `				/* Referenced object */` |
|     ! 0 | 6628 | `				isRef = 1;` |
|     ! 0 | 6629 | `			}` |
|     100 | 6630 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 6631 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6632 | `				break;` |
|       - | 6633 | `			}` |
|      49 | 6634 | `		}` |
|       - | 6635 | `		/* Point to the next entry */` |
|     100 | 6636 | `		n++;` |
|     100 | 6637 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6638 | `	}` |
|      54 | 6639 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 6640 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 6641 | `	}` |
|      28 | 6642 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 6643 | `	return rc;` |
|      15 | 6644 |  |
|       - | 6645 | `/*` |
|       - | 6646 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6647 | ` * retrieved entry.` |
|       - | 6648 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6649 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6650 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6651 | ` * a value different from PH7_OK.` |
|       - | 6652 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6653 | ` */` |
|   21146 | 6654 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6655 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6656 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6657 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6658 | `	)` |
|       2 | 6659 |  |
|       - | 6660 | `	ph7_hashmap_node *pEntry;` |
|       - | 6661 | `	ph7_value sKey,sValue;` |
|       - | 6662 | `	sxi32 rc;` |
|       - | 6663 | `	sxu32 n;` |
|       - | 6664 | `	/* Initialize walker parameter */` |
|   21148 | 6665 | `	rc = SXRET_OK;` |
|   21148 | 6666 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   21148 | 6667 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   21148 | 6668 | `	n = pMap->nEntry;` |
|   21148 | 6669 | `	pEntry = pMap->pFirst;` |
|       - | 6670 | `	/* Start the iteration process */` |
|   54679 | 6671 | `	for(;;){` |
|  109360 | 6672 | `		if( n < 1 ){` |
|   21148 | 6673 | `			break;` |
|       - | 6674 | `		}` |
|       - | 6675 | `		/* Extract a copy of the key and a copy the current value */` |
|   88214 | 6676 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   88214 | 6677 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6678 | `		/* Invoke the user callback */` |
|   88214 | 6679 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6680 | `		/* Release the copy of the key and the value */` |
|   88214 | 6681 | `		PH7_MemObjRelease(&sKey);` |
|   88214 | 6682 | `		PH7_MemObjRelease(&sValue);` |
|   88214 | 6683 | `		if( rc != PH7_OK ){` |
|       - | 6684 | `			/* Callback request an operation abort */` |
|     ! 0 | 6685 | `			return SXERR_ABORT;` |
|       - | 6686 | `		}` |
|       - | 6687 | `		/* Point to the next entry */` |
|   88214 | 6688 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   88214 | 6689 | `		n--;` |
|       2 | 6690 | `	}` |
|       - | 6691 | `	/* All done */` |
|   21148 | 6692 | `	return SXRET_OK;` |
|   10575 | 6693 |  |
|       - | 6694 |  |
