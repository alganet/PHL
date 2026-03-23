# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2890/3322 lines (87.00%)

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
| 2858628 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2858630 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  244110 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  244112 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  244112 |   29 | `	sxu32 nH = 5381;` |
|  244112 |   30 | `	zEnd = &zIn[nLen];` |
|  277784 |   31 | `	for(;;){` |
|  555570 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  494728 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  446248 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  364054 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  244112 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     770 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       2 |   47 |  |
|     772 |   48 | `	sxi64 iCount = 0;` |
|     772 |   49 | `	if( !bRecursive ){` |
|     598 |   50 | `		iCount = pMap->nEntry;` |
|     300 |   51 | `	}else{` |
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
|     772 |   86 | `	return iCount;` |
|       2 |   87 |  |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 2803352 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 2803354 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2803354 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 2803354 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 2803354 |  104 | `	pNode->pMap  = &(*pMap);` |
| 2803354 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2803354 |  106 | `	pNode->nHash = nHash;` |
| 2803354 |  107 | `	pNode->xKey.iKey = iKey;` |
| 2803354 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 2803354 |  109 | `	return pNode;` |
| 1401678 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|   85128 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|   85130 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   85130 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|   85130 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|   85130 |  127 | `	pNode->pMap  = &(*pMap);` |
|   85130 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   85130 |  129 | `	pNode->nHash = nHash;` |
|   85130 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   85130 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   85130 |  132 | `	pNode->nValIdx = nValIdx;` |
|   85130 |  133 | `	return pNode;` |
|   42566 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 2888480 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  139 |  |
|       - |  140 | `	/* Link */` |
| 2888482 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2656854 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2656854 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1328426 |  144 | `	}` |
| 2888482 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 2888482 |  147 | `	if( pMap->pFirst == 0 ){` |
|   39992 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   39992 |  150 | `		pMap->pCur = pNode;` |
|   19997 |  151 | `	}else{` |
| 2848492 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 2888482 |  154 | `	++pMap->nEntry;` |
| 2888482 |  155 |  |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    5732 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  161 |  |
|    5734 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5734 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    5734 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    5318 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2660 |  167 | `	}else{` |
|     417 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    5734 |  170 | `	if( pNode->pNextCollide ){` |
|    4479 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2239 |  172 | `	}` |
|    5734 |  173 | `	if( pMap->pFirst == pNode ){` |
|      78 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      38 |  175 | `	}` |
|    5734 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|      80 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      39 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    5734 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5734 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     100 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     100 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     100 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      49 |  188 | `		}` |
|      49 |  189 | `	}` |
|    5734 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5615 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2807 |  192 | `	}` |
|    5734 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5734 |  194 | `	pMap->nEntry--;` |
|    5734 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      34 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      34 |  198 | `		pMap->apBucket = 0;` |
|      34 |  199 | `		pMap->nSize = 0;` |
|      34 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      16 |  201 | `	}` |
|    5734 |  202 |  |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 2888480 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  208 |  |
| 2888482 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   43860 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   43860 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   43860 |  215 | `		if( nNew < 1 ){` |
|   39992 |  216 | `			nNew = 16;` |
|   19995 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   43860 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   43860 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   43860 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   43860 |  230 | `		pMap->apBucket = apNew;` |
|   43860 |  231 | `		pMap->nSize = nNew;` |
|   43860 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   39992 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    3870 |  237 | `		pEntry = pMap->pFirst;` |
|    3870 |  238 | `		n = 0;` |
| 1970798 |  239 | `		for( ;; ){` |
| 3941598 |  240 | `			if( n >= pMap->nEntry ){` |
|    3870 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 3937730 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 3937730 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3937730 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3449062 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3449062 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1724530 |  250 | `			}` |
| 3937730 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 3937730 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3937730 |  254 | `			n++;` |
|       2 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    3870 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1934 |  258 | `	}` |
| 2848492 |  259 | `	return SXRET_OK;` |
| 1444242 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 2803352 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 2803354 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		/* Reserve a ph7_value for the value */` |
| 2803328 |  274 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2803328 |  275 | `		if( pObj == 0 ){` |
|     ! 0 |  276 | `			return SXERR_MEM;` |
|       - |  277 | `		}` |
| 2803328 |  278 | `		if( pValue ){` |
|       - |  279 | `			/* Duplicate the value */` |
| 2803328 |  280 | `			PH7_MemObjStore(pValue,pObj);` |
| 1401663 |  281 | `		}` |
| 2803328 |  282 | `		nIdx = pObj->nIdx;` |
| 1401665 |  283 | `	}else{` |
|      27 |  284 | `		nIdx = nRefIdx;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Hash the key */` |
| 2803354 |  287 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  288 | `	/* Allocate a new int node */` |
| 2803354 |  289 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2803354 |  290 | `	if( pNode == 0 ){` |
|     ! 0 |  291 | `		return SXERR_MEM;` |
|       - |  292 | `	}` |
| 2803354 |  293 | `	if( isForeign ){` |
|       - |  294 | `		/* Mark as a foregin entry */` |
|      27 |  295 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      13 |  296 | `	}` |
|       - |  297 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2803354 |  298 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2803354 |  299 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  300 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  301 | `		return rc;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Perform the insertion */` |
| 2803354 |  304 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  305 | `	/* Install in the reference table */` |
| 2803354 |  306 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  307 | `	/* All done */` |
| 2803354 |  308 | `	return SXRET_OK;` |
| 1401678 |  309 |  |
|       - |  310 | `/*` |
|       - |  311 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  312 | ` * hashmap.` |
|       - |  313 | ` */` |
|   85128 |  314 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  315 |  |
|       - |  316 | `	ph7_hashmap_node *pNode;` |
|       - |  317 | `	sxu32 nHash;` |
|       - |  318 | `	sxu32 nIdx;` |
|       - |  319 | `	sxi32 rc;` |
|   85130 |  320 | `	if( !isForeign ){` |
|       - |  321 | `		ph7_value *pObj;` |
|       - |  322 | `		/* Reserve a ph7_value for the value */` |
|   61212 |  323 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   61212 |  324 | `		if( pObj == 0 ){` |
|     ! 0 |  325 | `			return SXERR_MEM;` |
|       - |  326 | `		}` |
|   61212 |  327 | `		if( pValue ){` |
|       - |  328 | `			/* Duplicate the value */` |
|   61212 |  329 | `			PH7_MemObjStore(pValue,pObj);` |
|   30605 |  330 | `		}` |
|   61212 |  331 | `		nIdx = pObj->nIdx;` |
|   30607 |  332 | `	}else{` |
|   23920 |  333 | `		nIdx = nRefIdx;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Hash the key */` |
|   85130 |  336 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  337 | `	/* Allocate a new blob node */` |
|   85130 |  338 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   85130 |  339 | `	if( pNode == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|   85130 |  342 | `	if( isForeign ){` |
|       - |  343 | `		/* Mark as a foregin entry */` |
|   23920 |  344 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   11959 |  345 | `	}` |
|       - |  346 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   85130 |  347 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   85130 |  348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  349 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  350 | `		return rc;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Perform the insertion */` |
|   85130 |  353 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  354 | `	/* Install in the reference table */` |
|   85130 |  355 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  356 | `	/* All done */` |
|   85130 |  357 | `	return SXRET_OK;` |
|   42566 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  361 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  362 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  363 | ` */` |
|   46996 |  364 | `static sxi32 HashmapLookupIntKey(` |
|       - |  365 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  366 | `	sxi64 iKey,                /* lookup key */` |
|       - |  367 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  368 | `	)` |
|       2 |  369 |  |
|       - |  370 | `	ph7_hashmap_node *pNode;` |
|       - |  371 | `	sxu32 nHash;` |
|   46998 |  372 | `	if( pMap->nEntry < 1 ){` |
|       - |  373 | `		/* Don't bother hashing,there is no entry anyway */` |
|     398 |  374 | `		return SXERR_NOTFOUND;` |
|       - |  375 | `	}` |
|       - |  376 | `	/* Hash the key first */` |
|   46602 |  377 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  378 | `	/* Point to the appropriate bucket */` |
|   46602 |  379 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  380 | `	/* Perform the lookup */` |
|  411653 |  381 | `	for(;;){` |
|  823308 |  382 | `		if( pNode == 0 ){` |
|   45820 |  383 | `			break;` |
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
|   45820 |  398 | `	return SXERR_NOTFOUND;` |
|   23500 |  399 |  |
|       - |  400 | `/*` |
|       - |  401 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  402 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  403 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  404 | ` */` |
|  168288 |  405 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  406 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  407 | `	const void *pKey,           /* Lookup key */` |
|       - |  408 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  409 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  410 | `	)` |
|       2 |  411 |  |
|       - |  412 | `	ph7_hashmap_node *pNode;` |
|       - |  413 | `	sxu32 nHash;` |
|  168290 |  414 | `	if( pMap->nEntry < 1 ){` |
|       - |  415 | `		/* Don't bother hashing,there is no entry anyway */` |
|    9308 |  416 | `		return SXERR_NOTFOUND;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Hash the key first */` |
|  158984 |  419 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  420 | `	/* Point to the appropriate bucket */` |
|  158984 |  421 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  422 | `	/* Perform the lookup */` |
|  154542 |  423 | `	for(;;){` |
|  309086 |  424 | `		if( pNode == 0 ){` |
|  120934 |  425 | `			break;` |
|       - |  426 | `		}` |
|  207177 |  427 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  186654 |  428 | `			&& pNode->nHash == nHash` |
|  111603 |  429 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   38052 |  430 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  431 | `				/* Node found */` |
|   38052 |  432 | `				if( ppNode ){` |
|   38024 |  433 | `					*ppNode = pNode;` |
|   19011 |  434 | `				}` |
|   38052 |  435 | `				return SXRET_OK;` |
|       - |  436 | `		}` |
|       - |  437 | `		/* Follow the collision link */` |
|  150104 |  438 | `		pNode = pNode->pNextCollide;` |
|       2 |  439 | `	}` |
|       - |  440 | `	/* No such entry */` |
|  120934 |  441 | `	return SXERR_NOTFOUND;` |
|   84146 |  442 |  |
|       - |  443 | `/*` |
|       - |  444 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  445 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  446 | ` */` |
|  168430 |  447 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  448 |  |
|  168432 |  449 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  168432 |  450 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  168432 |  451 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  452 | `		/* Octal not decimal number */` |
|       5 |  453 | `		return FALSE;` |
|       - |  454 | `	}` |
|  168428 |  455 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  456 | `		zIn++;` |
|     ! 0 |  457 | `	}` |
|   84546 |  458 | `	for(;;){` |
|  169094 |  459 | `		if( zIn >= zEnd ){` |
|     233 |  460 | `			return TRUE;` |
|       - |  461 | `		}` |
|  168862 |  462 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   84099 |  463 | `			break;` |
|       - |  464 | `		}` |
|     667 |  465 | `		zIn++;` |
|       1 |  466 | `	}` |
|       - |  467 | `	/* Key does not look like a decimal number */` |
|  168196 |  468 | `	return FALSE;` |
|   84217 |  469 |  |
|       - |  470 | `/*` |
|       - |  471 | ` * Check if a given key exists in the given hashmap.` |
|       - |  472 | ` * Write a pointer to the target node on success.` |
|       - |  473 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  474 | ` */` |
|   83830 |  475 | `static sxi32 HashmapLookup(` |
|       - |  476 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  477 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  478 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  479 | `	)` |
|       2 |  480 |  |
|   83832 |  481 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  482 | `	sxi32 rc;` |
|   83832 |  483 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   83142 |  484 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  485 | `			/* Force a string cast */` |
|     ! 0 |  486 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  487 | `		}` |
|   83142 |  488 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  489 | `			/* Perform a blob lookup */` |
|   83126 |  490 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   83126 |  491 | `			goto result;` |
|       - |  492 | `		}` |
|       8 |  493 | `	}` |
|       - |  494 | `	/* Perform an int lookup */` |
|     708 |  495 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  496 | `		/* Force an integer cast */` |
|      27 |  497 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  498 | `	}` |
|       - |  499 | `	/* Perform an int lookup */` |
|     708 |  500 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   41915 |  501 | `result:` |
|   83832 |  502 | `	if( rc == SXRET_OK ){` |
|       - |  503 | `		/* Node found */` |
|   38652 |  504 | `		if( ppNode ){` |
|   38618 |  505 | `			*ppNode = pNode;` |
|   19308 |  506 | `		}` |
|   38652 |  507 | `		return SXRET_OK;` |
|       - |  508 | `	}` |
|       - |  509 | `	/* No such entry */` |
|   45182 |  510 | `	return SXERR_NOTFOUND;` |
|   41917 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  514 | ` * hashmap.` |
|       - |  515 | ` * If a node with the given key already exists in the database` |
|       - |  516 | ` * then this function overwrite the old value.` |
|       - |  517 | ` */` |
| 2864340 |  518 | `static sxi32 HashmapInsert(` |
|       - |  519 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  520 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  521 | `	ph7_value *pVal    /* Node value */` |
|       - |  522 | `	)` |
|       2 |  523 |  |
| 2864342 |  524 | `	ph7_hashmap_node *pNode = 0;` |
| 2864342 |  525 | `	sxi32 rc = SXRET_OK;` |
| 2864342 |  526 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   61406 |  527 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  528 | `			/* Force a string cast */` |
|       3 |  529 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  530 | `		}` |
|   61406 |  531 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  532 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  533 | `				/* Automatic index assign */` |
|      34 |  534 | `				pKey = 0;` |
|      16 |  535 | `			}` |
|     256 |  536 | `			goto IntKey;` |
|       - |  537 | `		}` |
|   91727 |  538 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   30575 |  539 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|   61116 |  553 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  554 | `			/* Forbidden */` |
|       3 |  555 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Perform a blob-key insertion */` |
|   61114 |  559 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   61114 |  560 | `		return rc;` |
|       - |  561 | `	}` |
| 1401468 |  562 | `IntKey:` |
| 2803192 |  563 | `	if( pKey ){` |
|   23258 |  564 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  565 | `			/* Force an integer cast */` |
|     251 |  566 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  567 | `		}` |
|   23258 |  568 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   23212 |  582 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  583 | `			/* Forbidden */` |
|       3 |  584 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  585 | `			return SXRET_OK;` |
|       - |  586 | `		}` |
|       - |  587 | `		/* Perform a 64-bit-int-key insertion */` |
|   23210 |  588 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23210 |  589 | `		if( rc == SXRET_OK ){` |
|   23210 |  590 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  591 | `				/* Increment the automatic index */` |
|   22978 |  592 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  593 | `				/* Make sure the automatic index is not reserved */` |
|   22978 |  594 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  595 | `					pMap->iNextIdx++;` |
|     ! 0 |  596 | `				}` |
|   11488 |  597 | `			}` |
|   11604 |  598 | `		}` |
|   11606 |  599 | `	}else{` |
| 2779936 |  600 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  601 | `			/* Forbidden */` |
|       3 |  602 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|       - |  605 | `		/* Assign an automatic index */` |
| 2779934 |  606 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2779934 |  607 | `		if( rc == SXRET_OK ){` |
| 2779934 |  608 | `			++pMap->iNextIdx;` |
| 1389966 |  609 | `		}` |
|       - |  610 | `	}` |
|       - |  611 | `	/* Insertion result */` |
| 2803142 |  612 | `	return rc;` |
| 1432172 |  613 |  |
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
|   23950 |  641 | `static sxi32 HashmapInsertByRef(` |
|       - |  642 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  643 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  644 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  645 | `	)` |
|       2 |  646 |  |
|   23952 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|   23952 |  648 | `	sxi32 rc = SXRET_OK;` |
|   23952 |  649 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   23926 |  650 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  651 | `			/* Force a string cast */` |
|     ! 0 |  652 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  653 | `		}` |
|   23926 |  654 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  655 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  656 | `				/* Automatic index assign */` |
|     ! 0 |  657 | `				pKey = 0;` |
|     ! 0 |  658 | `			}` |
|     ! 0 |  659 | `			goto IntKey;` |
|       - |  660 | `		}` |
|   35888 |  661 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   11962 |  662 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  663 | `				/* Overwrite */` |
|       7 |  664 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  665 | `				pNode->nValIdx = nRefIdx;` |
|       - |  666 | `				/* Install in the reference table */` |
|       7 |  667 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  668 | `				return SXRET_OK;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Perform a blob-key insertion */` |
|   23920 |  671 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   23920 |  672 | `		return rc;` |
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
|   11977 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * Extract node value.` |
|       - |  712 | ` */` |
|  935655 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
|  935657 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  935657 |  718 | `	return pObj;` |
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
|   39386 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   39388 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   39388 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   39388 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   39388 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   39388 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   39388 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   39388 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   39388 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   39388 |  783 | `	return rc;` |
|   19741 |  784 |  |
|       - |  785 | `/*` |
|       - |  786 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  787 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  788 | ` */` |
|    8676 |  789 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  790 |  |
|    8678 |  791 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  792 | `	sxu32 nBucket;` |
|       - |  793 | `	/* Remove old collision links */` |
|    8678 |  794 | `	if( pEntry->pPrevCollide ){` |
|    6712 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    3352 |  796 | `	}else{` |
|    1968 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  798 | `	}` |
|    8678 |  799 | `	if( pEntry->pNextCollide ){` |
|     665 |  800 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     328 |  801 | `	}` |
|    8678 |  802 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  803 | `	/* Compute the new hash */` |
|    8678 |  804 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    8678 |  805 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    8678 |  806 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  807 | `	/* Link to the new bucket */` |
|    8678 |  808 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    8678 |  809 | `	if( pMap->apBucket[nBucket] ){` |
|    6883 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3439 |  811 | `	}` |
|    8678 |  812 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    8678 |  813 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  814 | `	/* Increment the automatic index */` |
|    8678 |  815 | `	pMap->iNextIdx++;` |
|    8678 |  816 |  |
|       - |  817 | `/*` |
|       - |  818 | ` * Perform a linear search on a given hashmap.` |
|       - |  819 | ` * Write a pointer to the target node on success.` |
|       - |  820 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  821 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  822 | ` * for more information.` |
|       - |  823 | ` */` |
|   21924 |  824 | `static int HashmapFindValue(` |
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
|   21926 |  837 | `	pEntry = pMap->pFirst;` |
|   21926 |  838 | `	n = pMap->nEntry;` |
|   21926 |  839 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   21926 |  840 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   52530 |  841 | `	for(;;){` |
|  105063 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  104965 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  104965 |  847 | `		if( pVal ){` |
|  104965 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  104965 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  104965 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  104965 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  104965 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  104965 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  104965 |  865 | `				if( rc == 0 ){` |
|   21828 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   21828 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   41568 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|   83139 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   83139 |  876 | `		n--;` |
|       2 |  877 | `	}` |
|       - |  878 | `	/* No such entry */` |
|      99 |  879 | `	return SXERR_NOTFOUND;` |
|   10964 |  880 |  |
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
|  463272 | 1054 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1055 | `	ph7_hashmap *pDest,` |
|       - | 1056 | `	ph7_hashmap_node *pEntry,` |
|       - | 1057 | `	ph7_value *pVal,` |
|       - | 1058 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1059 | `	)` |
|       2 | 1060 |  |
|  463274 | 1061 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1062 | `	ph7_value sKey;` |
|       - | 1063 | `	sxi32 rc;` |
|       - | 1064 |  |
|  463274 | 1065 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1066 | `		/* Blob key insertion */` |
|      41 | 1067 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      41 | 1068 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      41 | 1069 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      41 | 1070 | `		PH7_MemObjRelease(&sKey);` |
|      21 | 1071 | `	}else{` |
|       - | 1072 | `		/* Int key */` |
|  463234 | 1073 | `		if( iAction == 0 ){ /* Merge */` |
|  463162 | 1074 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  231654 | 1075 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1076 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1077 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1078 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1079 | `		}else{ /* Dup */` |
|      44 | 1080 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1081 | `		}` |
|       - | 1082 | `	}` |
|  463274 | 1083 | `	return rc;` |
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
|    1768 | 1097 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1098 |  |
|       - | 1099 | `	ph7_hashmap_node *pEntry;` |
|       - | 1100 | `	ph7_value *pVal;` |
|       - | 1101 | `	sxi32 rc;` |
|       - | 1102 | `	sxu32 n;` |
|    1770 | 1103 | `	if( pSrc == pDest ){` |
|       - | 1104 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1105 | `		 * Unlike the zend engine.` |
|       - | 1106 | `		 */` |
|     ! 0 | 1107 | `		return SXRET_OK;` |
|       - | 1108 | `	}` |
|       - | 1109 | `	/* Point to the first inserted entry in the source */` |
|    1770 | 1110 | `	pEntry = pSrc->pFirst;` |
|       - | 1111 | `	/* Perform the merge */` |
|  464946 | 1112 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1113 | `		/* Extract the node value */` |
|  463178 | 1114 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  463178 | 1115 | `		if( pVal ){` |
|       - | 1116 | `			/* Make a local copy of the value.` |
|       - | 1117 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1118 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1119 | `			 * to the old pool.` |
|       - | 1120 | `			 */` |
|  463178 | 1121 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  231590 | 1122 | `		}else{` |
|     ! 0 | 1123 | `			rc = SXRET_OK;` |
|       - | 1124 | `		}` |
|  463178 | 1125 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1126 | `			return rc;` |
|       - | 1127 | `		}` |
|       - | 1128 | `		/* Point to the next entry */` |
|  463178 | 1129 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  231590 | 1130 | `	}` |
|    1770 | 1131 | `	return SXRET_OK;` |
|     886 | 1132 |  |
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
|   60926 | 1304 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1305 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1306 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1307 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1308 | `	)` |
|       2 | 1309 |  |
|       - | 1310 | `	ph7_hashmap *pMap;` |
|       - | 1311 | `	/* Allocate a new instance */` |
|   60928 | 1312 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   60928 | 1313 | `	if( pMap == 0 ){` |
|     ! 0 | 1314 | `		return 0;` |
|       - | 1315 | `	}` |
|       - | 1316 | `	/* Zero the structure */` |
|   60928 | 1317 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1318 | `	/* Fill in the structure */` |
|   60928 | 1319 | `	pMap->pVm = &(*pVm);` |
|   60928 | 1320 | `	pMap->iRef = 1;` |
|       - | 1321 | `	/* Default hash functions */` |
|   60928 | 1322 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   60928 | 1323 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   60928 | 1324 | `	return pMap;` |
|   30465 | 1325 |  |
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
|    1834 | 1346 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    1836 | 1366 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1836 | 1367 | `	if( pMap == 0 ){` |
|     ! 0 | 1368 | `		return SXERR_MEM;` |
|       - | 1369 | `	}` |
|    1836 | 1370 | `	pVm->pGlobal = pMap;` |
|       - | 1371 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1836 | 1372 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1836 | 1373 | `	if( pObj == 0 ){` |
|     ! 0 | 1374 | `		return SXERR_MEM;` |
|       - | 1375 | `	}` |
|    1836 | 1376 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1377 | `	/* Record object index */` |
|    1836 | 1378 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1379 | `	/* Install the special $GLOBALS array */` |
|    1836 | 1380 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1836 | 1381 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1382 | `		return rc;` |
|       - | 1383 | `	}` |
|       - | 1384 | `	/* Install superglobals now */` |
|   20176 | 1385 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1386 | `		ph7_value *pSuper;` |
|       - | 1387 | `		/* Request an empty array */` |
|   18342 | 1388 | `		pSuper = ph7_new_array(&(*pVm));` |
|   18342 | 1389 | `		if( pSuper == 0 ){` |
|     ! 0 | 1390 | `			return SXERR_MEM;` |
|       - | 1391 | `		}` |
|       - | 1392 | `		/* Install */` |
|   18342 | 1393 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   18342 | 1394 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1395 | `			return rc;` |
|       - | 1396 | `		}` |
|       - | 1397 | `		/* Release the value now it have been installed */` |
|   18342 | 1398 | `		ph7_release_value(&(*pVm),pSuper);` |
|    9172 | 1399 | `	}` |
|       - | 1400 | `	/* Set some $_SERVER entries */` |
|    1836 | 1401 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1402 | `	/*` |
|       - | 1403 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1404 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1405 | `	 */` |
|    3666 | 1406 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1407 | `		"SCRIPT_FILENAME",` |
|     917 | 1408 | `		pFile ? pFile->zString : ":Memory:",` |
|    1830 | 1409 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1410 | `		);` |
|       - | 1411 | `	/* All done,all super-global are installed now */` |
|    1836 | 1412 | `	return SXRET_OK;` |
|     919 | 1413 |  |
|       - | 1414 | `/*` |
|       - | 1415 | ` * Release a hashmap.` |
|       - | 1416 | ` */` |
|   40658 | 1417 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1418 |  |
|       - | 1419 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   40660 | 1420 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1421 | `	sxu32 n;` |
|   40660 | 1422 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1423 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1424 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1425 | `		return SXRET_OK;` |
|       - | 1426 | `	}` |
|       - | 1427 | `	/* Start the release process */` |
|   40660 | 1428 | `	n = 0;` |
|   40660 | 1429 | `	pEntry = pMap->pFirst;` |
| 1449150 | 1430 | `	for(;;){` |
| 2898302 | 1431 | `		if( n >= pMap->nEntry ){` |
|   40660 | 1432 | `			break;` |
|       - | 1433 | `		}` |
| 2857644 | 1434 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1435 | `		/* Remove the reference from the foreign table */` |
| 2857644 | 1436 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2857644 | 1437 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1438 | `			/* Restore the ph7_value to the free list */` |
| 2857636 | 1439 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1428817 | 1440 | `		}` |
|       - | 1441 | `		/* Release the node */` |
| 2857644 | 1442 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   58692 | 1443 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   29345 | 1444 | `		}` |
| 2857644 | 1445 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1446 | `		/* Point to the next entry */` |
| 2857644 | 1447 | `		pEntry = pNext;` |
| 2857644 | 1448 | `		n++;` |
|       2 | 1449 | `	}` |
|   40660 | 1450 | `	if( pMap->nEntry > 0 ){` |
|       - | 1451 | `		/* Release the hash bucket */` |
|   36182 | 1452 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   18090 | 1453 | `	}` |
|   40660 | 1454 | `	if( FreeDS ){` |
|       - | 1455 | `		/* Free the whole instance */` |
|   40644 | 1456 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   20323 | 1457 | `	}else{` |
|       - | 1458 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1459 | `		pMap->apBucket = 0;` |
|      17 | 1460 | `		pMap->iNextIdx = 0;` |
|      17 | 1461 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1462 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1463 | `	}` |
|   40660 | 1464 | `	return SXRET_OK;` |
|   20331 | 1465 |  |
|       - | 1466 | `/*` |
|       - | 1467 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1468 | ` * If the count reaches zero which mean no more variables` |
|       - | 1469 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1470 | ` */` |
|  461500 | 1471 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1472 |  |
|  461502 | 1473 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1474 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  461502 | 1475 | `	pMap->iRef--;` |
|  461502 | 1476 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   40644 | 1477 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   20321 | 1478 | `	}` |
|  461502 | 1479 |  |
|       - | 1480 | `/*` |
|       - | 1481 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1482 | ` * Write a pointer to the target node on success.` |
|       - | 1483 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1484 | ` */` |
|   83848 | 1485 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1486 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1487 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1488 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1489 | `	)` |
|       2 | 1490 |  |
|       - | 1491 | `	sxi32 rc;` |
|   83850 | 1492 | `	if( pMap->nEntry < 1 ){` |
|       - | 1493 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1494 | `		 */` |
|      19 | 1495 | `		return SXERR_NOTFOUND;` |
|       - | 1496 | `	}` |
|   83832 | 1497 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   83832 | 1498 | `	return rc;` |
|   41926 | 1499 |  |
|       - | 1500 | `/*` |
|       - | 1501 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1502 | ` * hashmap.` |
|       - | 1503 | ` * If a node with the given key already exists in the database` |
|       - | 1504 | ` * then this function overwrite the old value.` |
|       - | 1505 | ` */` |
| 2400968 | 1506 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1507 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1508 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1509 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1510 | `	)` |
|       2 | 1511 |  |
|       - | 1512 | `	sxi32 rc;` |
| 2400970 | 1513 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1514 | `		/*` |
|       - | 1515 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1516 | `		 */` |
|     ! 0 | 1517 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1518 | `		return SXRET_OK;` |
|       - | 1519 | `	}` |
| 2400970 | 1520 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2400970 | 1521 | `	return rc;` |
| 1200486 | 1522 |  |
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
|   23950 | 1550 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1551 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1552 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1553 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1554 | `	)` |
|       2 | 1555 |  |
|       - | 1556 | `	sxi32 rc;` |
|   23952 | 1557 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1558 | `		/*` |
|       - | 1559 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1560 | `		 */` |
|     ! 0 | 1561 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1562 | `		return SXRET_OK;` |
|       - | 1563 | `	}` |
|   23952 | 1564 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   23952 | 1565 | `	return rc;` |
|   11977 | 1566 |  |
|       - | 1567 | `/*` |
|       - | 1568 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1569 | ` */` |
|   18088 | 1570 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1571 |  |
|       - | 1572 | `	/* Reset the loop cursor */` |
|   18090 | 1573 | `	pMap->pCur = pMap->pFirst;` |
|   18090 | 1574 |  |
|       - | 1575 | `/*` |
|       - | 1576 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1577 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1578 | ` * return NULL.` |
|       - | 1579 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1580 | ` */` |
|  145386 | 1581 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1582 |  |
|  145388 | 1583 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  145388 | 1584 | `	if( pCur == 0 ){` |
|       - | 1585 | `		/* End of the list,return null */` |
|    9066 | 1586 | `		return 0;` |
|       - | 1587 | `	}` |
|       - | 1588 | `	/* Advance the node cursor */` |
|  136324 | 1589 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  136324 | 1590 | `	return pCur;` |
|   72695 | 1591 |  |
|       - | 1592 | `/*` |
|       - | 1593 | ` * Extract a node value.` |
|       - | 1594 | ` */` |
|  343628 | 1595 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1596 |  |
|  343630 | 1597 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  343630 | 1598 | `	if( pEntry ){` |
|  343630 | 1599 | `		if( bStore ){` |
|  136352 | 1600 | `			PH7_MemObjStore(pEntry,pValue);` |
|   68177 | 1601 | `		}else{` |
|  207280 | 1602 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1603 | `		}` |
|  171908 | 1604 | `	}else{` |
|     ! 0 | 1605 | `		PH7_MemObjRelease(pValue);` |
|       - | 1606 | `	}` |
|  343630 | 1607 |  |
|       - | 1608 | `/*` |
|       - | 1609 | ` * Extract a node key.` |
|       - | 1610 | ` */` |
|   90380 | 1611 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1612 |  |
|       - | 1613 | `	/* Fill with the current key */` |
|   90382 | 1614 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   90158 | 1615 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      13 | 1616 | `			SyBlobRelease(&pKey->sBlob);` |
|       6 | 1617 | `		}` |
|   90158 | 1618 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   90158 | 1619 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   45080 | 1620 | `	}else{` |
|     226 | 1621 | `		SyBlobReset(&pKey->sBlob);` |
|     226 | 1622 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     226 | 1623 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1624 | `	}` |
|   90382 | 1625 |  |
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
|   25852 | 1673 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1674 |  |
|       - | 1675 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1676 | `    /* Prevent compiler warning */` |
|   25854 | 1677 | `	result.pNext = result.pPrev = 0;` |
|   25854 | 1678 | `	pTail = &result;` |
|   65307 | 1679 | `	while( pA && pB ){` |
|   39455 | 1680 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   26050 | 1681 | `			pTail->pPrev = pA;` |
|   26050 | 1682 | `			pA->pNext = pTail;` |
|   26050 | 1683 | `			pTail = pA;` |
|   26050 | 1684 | `			pA = pA->pPrev;` |
|   13030 | 1685 | `		}else{` |
|   13407 | 1686 | `			pTail->pPrev = pB;` |
|   13407 | 1687 | `			pB->pNext = pTail;` |
|   13407 | 1688 | `			pTail = pB;` |
|   13407 | 1689 | `			pB = pB->pPrev;` |
|       - | 1690 | `		}` |
|       2 | 1691 | `	}` |
|   25854 | 1692 | `	if( pA ){` |
|   19207 | 1693 | `		pTail->pPrev = pA;` |
|   19207 | 1694 | `		pA->pNext = pTail;` |
|   16261 | 1695 | `	}else if( pB ){` |
|    6457 | 1696 | `		pTail->pPrev = pB;` |
|    6457 | 1697 | `		pB->pNext = pTail;` |
|    3220 | 1698 | `	}else{` |
|     194 | 1699 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1700 | `	}` |
|   25854 | 1701 | `	return result.pPrev;` |
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
|     584 | 1715 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1716 |  |
|       - | 1717 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1718 | `	sxu32 i;` |
|     586 | 1719 | `	SyZero(a,sizeof(a));` |
|       - | 1720 | `	/* Point to the first inserted entry */` |
|     586 | 1721 | `	pIn = pMap->pFirst;` |
|    9324 | 1722 | `	while( pIn ){` |
|    8740 | 1723 | `		p = pIn;` |
|    8740 | 1724 | `		pIn = p->pPrev;` |
|    8740 | 1725 | `		p->pPrev = 0;` |
|   16488 | 1726 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   16488 | 1727 | `			if( a[i]==0 ){` |
|    8740 | 1728 | `				a[i] = p;` |
|    8740 | 1729 | `				break;` |
|     ! 0 | 1730 | `			}else{` |
|    7750 | 1731 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    7750 | 1732 | `				a[i] = 0;` |
|       - | 1733 | `			}` |
|    3876 | 1734 | `		}` |
|    8740 | 1735 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1736 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1737 | `			 * But that is impossible.` |
|       - | 1738 | `			 */` |
|     ! 0 | 1739 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1740 | `		}` |
|       2 | 1741 | `	}` |
|     586 | 1742 | `	p = a[0];` |
|   18690 | 1743 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   18106 | 1744 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    9054 | 1745 | `	}` |
|     586 | 1746 | `	p->pNext = 0;` |
|       - | 1747 | `	/* Reflect the change */` |
|     586 | 1748 | `	pMap->pFirst = p;` |
|       - | 1749 | `	/* Reset the loop cursor */` |
|     586 | 1750 | `	pMap->pCur = pMap->pFirst;` |
|     586 | 1751 | `	return SXRET_OK;` |
|       2 | 1752 |  |
|       - | 1753 | `/*` |
|       - | 1754 | ` * Node comparison callback.` |
|       - | 1755 | ` * used-by: [sort(),asort(),...]` |
|       - | 1756 | ` */` |
|   39322 | 1757 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1758 |  |
|       - | 1759 | `	ph7_value sA,sB;` |
|       - | 1760 | `	sxi32 iFlags;` |
|       - | 1761 | `	int rc;` |
|   39324 | 1762 | `	if( pCmpData == 0 ){` |
|       - | 1763 | `		/* Perform a standard comparison */` |
|   39320 | 1764 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   39320 | 1765 | `		return rc;` |
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
|   19709 | 1791 |  |
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
|      78 | 1838 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1839 |  |
|       - | 1840 | `	ph7_value sA,sB;` |
|       - | 1841 | `	sxi32 iFlags;` |
|       - | 1842 | `	int rc;` |
|      80 | 1843 | `	if( pCmpData == 0 ){` |
|       - | 1844 | `		/* Perform a standard comparison */` |
|      60 | 1845 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      60 | 1846 | `		return -rc;` |
|       - | 1847 | `	}` |
|      21 | 1848 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1849 | `	/* Duplicate node values */` |
|      21 | 1850 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 1851 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 1852 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 1853 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 1854 | `	if( iFlags == 5 ){` |
|       - | 1855 | `		/* String cast */` |
|       - | 1856 | `		const char *zA,*zB;` |
|       - | 1857 | `		sxu32 nA,nB,nMin;` |
|      11 | 1858 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1859 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1860 | `		}` |
|      11 | 1861 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1862 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1863 | `		}` |
|       - | 1864 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 1865 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 1866 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 1867 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 1868 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 1869 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 1870 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 1871 | `		if( rc == 0 ){` |
|       3 | 1872 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1873 | `			else if( nA > nB ) rc = 1;` |
|       1 | 1874 | `		}` |
|       6 | 1875 | `	}else{` |
|       - | 1876 | `		/* Numeric cast */` |
|      11 | 1877 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1878 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1879 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1880 | `	}` |
|      21 | 1881 | `	PH7_MemObjRelease(&sA);` |
|      21 | 1882 | `	PH7_MemObjRelease(&sB);` |
|      21 | 1883 | `	return -rc;` |
|      41 | 1884 |  |
|       - | 1885 | `/*` |
|       - | 1886 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1887 | ` * used-by: [usort(),uasort()]` |
|       - | 1888 | ` */` |
|      12 | 1889 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1890 |  |
|       - | 1891 | `	ph7_value sResult,*pCallback;` |
|       - | 1892 | `	ph7_value *pV1,*pV2;` |
|       - | 1893 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1894 | `	sxi32 rc;` |
|       - | 1895 | `	/* Point to the desired callback */` |
|      13 | 1896 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1897 | `	/* initialize the result value */` |
|      13 | 1898 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 1899 | `	/* Extract nodes values */` |
|      13 | 1900 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      13 | 1901 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      13 | 1902 | `	apArg[0] = pV1;` |
|      13 | 1903 | `	apArg[1] = pV2;` |
|       - | 1904 | `	/* Invoke the callback */` |
|      13 | 1905 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      13 | 1906 | `	if( rc != SXRET_OK ){` |
|       - | 1907 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1908 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1909 | `	}else{` |
|       - | 1910 | `		/* Extract callback result */` |
|      13 | 1911 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1912 | `			/* Perform an int cast */` |
|     ! 0 | 1913 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1914 | `		}` |
|      13 | 1915 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1916 | `	}` |
|      13 | 1917 | `	PH7_MemObjRelease(&sResult);` |
|       - | 1918 | `	/* Callback result */` |
|      13 | 1919 | `	return rc;` |
|       1 | 1920 |  |
|       - | 1921 | `/*` |
|       - | 1922 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1923 | ` * used-by: [krsort()]` |
|       - | 1924 | ` */` |
|       4 | 1925 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1926 |  |
|       - | 1927 | `	sxi32 rc;` |
|       2 | 1928 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 1929 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1930 | `		/* Perform a string comparison */` |
|       5 | 1931 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1932 | `	}else{` |
|       - | 1933 | `		SyString sStr;` |
|       - | 1934 | `		sxi64 iA,iB;` |
|       - | 1935 | `		/* Perform a numeric comparison */` |
|     ! 0 | 1936 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1937 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1938 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1939 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1940 | `				iA = 0;` |
|     ! 0 | 1941 | `			}else{` |
|     ! 0 | 1942 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1943 | `			}` |
|     ! 0 | 1944 | `		}else{` |
|     ! 0 | 1945 | `			iA = pA->xKey.iKey;` |
|       - | 1946 | `		}` |
|     ! 0 | 1947 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1948 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1949 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1950 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1951 | `				iB = 0;` |
|     ! 0 | 1952 | `			}else{` |
|     ! 0 | 1953 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1954 | `			}` |
|     ! 0 | 1955 | `		}else{` |
|     ! 0 | 1956 | `			iB = pB->xKey.iKey;` |
|       - | 1957 | `		}` |
|     ! 0 | 1958 | `		rc = (sxi32)(iA-iB);` |
|       - | 1959 | `	}` |
|       5 | 1960 | `	return -rc; /* Reverse result */` |
|       1 | 1961 |  |
|       - | 1962 | `/*` |
|       - | 1963 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1964 | ` * used-by: [uksort()]` |
|       - | 1965 | ` */` |
|       6 | 1966 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1967 |  |
|       - | 1968 | `	ph7_value sResult,*pCallback;` |
|       - | 1969 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1970 | `	ph7_value sK1,sK2;` |
|       - | 1971 | `	sxi32 rc;` |
|       - | 1972 | `	/* Point to the desired callback */` |
|       7 | 1973 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1974 | `	/* initialize the result value */` |
|       7 | 1975 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 1976 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 1977 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 1978 | `	/* Extract nodes keys */` |
|       7 | 1979 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 1980 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 1981 | `	apArg[0] = &sK1;` |
|       7 | 1982 | `	apArg[1] = &sK2;` |
|       - | 1983 | `	/* Mark keys as constants */` |
|       7 | 1984 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 1985 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 1986 | `	/* Invoke the callback */` |
|       7 | 1987 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 1988 | `	if( rc != SXRET_OK ){` |
|       - | 1989 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1990 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1991 | `	}else{` |
|       - | 1992 | `		/* Extract callback result */` |
|       7 | 1993 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1994 | `			/* Perform an int cast */` |
|     ! 0 | 1995 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1996 | `		}` |
|       7 | 1997 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1998 | `	}` |
|       7 | 1999 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2000 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2001 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2002 | `	/* Callback result */` |
|       7 | 2003 | `	return rc;` |
|       1 | 2004 |  |
|       - | 2005 | `/*` |
|       - | 2006 | ` * Node comparison callback: Random node comparison.` |
|       - | 2007 | ` * used-by: [shuffle()]` |
|       - | 2008 | ` */` |
|      17 | 2009 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2010 |  |
|       - | 2011 | `	sxu32 n;` |
|       7 | 2012 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 2013 | `	SXUNUSED(pCmpData);` |
|       - | 2014 | `	/* Grab a random number */` |
|      18 | 2015 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2016 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2017 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2018 | `	 */` |
|      18 | 2019 | `	return n&1 ? 1 : -1;` |
|       1 | 2020 |  |
|       - | 2021 | `/*` |
|       - | 2022 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2023 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2024 | ` */` |
|     552 | 2025 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2026 |  |
|       - | 2027 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2028 | `	sxu32 i;` |
|       - | 2029 | `	/* Rehash all entries */` |
|     554 | 2030 | `	pLast = p = pMap->pFirst;` |
|     554 | 2031 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     554 | 2032 | `	i = 0;` |
|    4589 | 2033 | `	for( ;; ){` |
|    9180 | 2034 | `		if( i >= pMap->nEntry ){` |
|     554 | 2035 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     554 | 2036 | `			break;` |
|       - | 2037 | `		}` |
|    8628 | 2038 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2039 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2040 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2041 | `			/* Change key type */` |
|       5 | 2042 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2043 | `		}` |
|    8628 | 2044 | `		HashmapRehashIntNode(p);` |
|       - | 2045 | `		/* Point to the next entry */` |
|    8628 | 2046 | `		i++;` |
|    8628 | 2047 | `		pLast = p;` |
|    8628 | 2048 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2049 | `	}` |
|     554 | 2050 |  |
|       - | 2051 | `/*` |
|       - | 2052 | ` * Array functions implementation.` |
|       - | 2053 | ` * Status:` |
|       - | 2054 | ` *  Stable.` |
|       - | 2055 | ` */` |
|       - | 2056 | `/*` |
|       - | 2057 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2058 | ` * Sort an array.` |
|       - | 2059 | ` * Parameters` |
|       - | 2060 | ` *  $array` |
|       - | 2061 | ` *   The input array.` |
|       - | 2062 | ` * $sort_flags` |
|       - | 2063 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2064 | ` *  Sorting type flags:` |
|       - | 2065 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2066 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2067 | ` *   SORT_STRING - compare items as strings` |
|       - | 2068 | ` * Return` |
|       - | 2069 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2070 | ` *` |
|       - | 2071 | ` */` |
|     856 | 2072 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2073 |  |
|       - | 2074 | `	ph7_hashmap *pMap;` |
|       - | 2075 | `	/* Make sure we are dealing with a valid hashmap */` |
|     858 | 2076 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2077 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2078 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2079 | `		return PH7_OK;` |
|       - | 2080 | `	}` |
|       - | 2081 | `	/* Point to the internal representation of the input hashmap */` |
|     858 | 2082 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     858 | 2083 | `	if( pMap->nEntry > 1 ){` |
|     548 | 2084 | `		sxi32 iCmpFlags = 0;` |
|     548 | 2085 | `		if( nArg > 1 ){` |
|       - | 2086 | `			/* Extract comparison flags */` |
|       3 | 2087 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2088 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2089 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2090 | `			}` |
|       1 | 2091 | `		}` |
|       - | 2092 | `		/* Do the merge sort */` |
|     548 | 2093 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2094 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     548 | 2095 | `		HashmapSortRehash(pMap);` |
|     273 | 2096 | `	}` |
|       - | 2097 | `	/* All done,return TRUE */` |
|     858 | 2098 | `	ph7_result_bool(pCtx,1);` |
|     858 | 2099 | `	return PH7_OK;` |
|     430 | 2100 |  |
|       - | 2101 | `/*` |
|       - | 2102 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2103 | ` *  Sort an array and maintain index association.` |
|       - | 2104 | ` * Parameters` |
|       - | 2105 | ` *  $array` |
|       - | 2106 | ` *   The input array.` |
|       - | 2107 | ` * $sort_flags` |
|       - | 2108 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2109 | ` *  Sorting type flags:` |
|       - | 2110 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2111 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2112 | ` *   SORT_STRING - compare items as strings` |
|       - | 2113 | ` * Return` |
|       - | 2114 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2115 | ` */` |
|       2 | 2116 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2117 |  |
|       - | 2118 | `	ph7_hashmap *pMap;` |
|       - | 2119 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2120 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2121 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2122 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2123 | `		return PH7_OK;` |
|       - | 2124 | `	}` |
|       - | 2125 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2126 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2127 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2128 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2129 | `		if( nArg > 1 ){` |
|       - | 2130 | `			/* Extract comparison flags */` |
|     ! 0 | 2131 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2132 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2133 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2134 | `			}` |
|     ! 0 | 2135 | `		}` |
|       - | 2136 | `		/* Do the merge sort */` |
|       3 | 2137 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2138 | `		/* Fix the last link broken by the merge */` |
|       5 | 2139 | `		while(pMap->pLast->pPrev){` |
|       3 | 2140 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2141 | `		}` |
|       1 | 2142 | `	}` |
|       - | 2143 | `	/* All done,return TRUE */` |
|       3 | 2144 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2145 | `	return PH7_OK;` |
|       2 | 2146 |  |
|       - | 2147 | `/*` |
|       - | 2148 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2149 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2150 | ` * Parameters` |
|       - | 2151 | ` *  $array` |
|       - | 2152 | ` *   The input array.` |
|       - | 2153 | ` * $sort_flags` |
|       - | 2154 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2155 | ` *  Sorting type flags:` |
|       - | 2156 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2157 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2158 | ` *   SORT_STRING - compare items as strings` |
|       - | 2159 | ` * Return` |
|       - | 2160 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2161 | ` */` |
|      32 | 2162 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2163 |  |
|       - | 2164 | `	ph7_hashmap *pMap;` |
|       - | 2165 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2166 | `	if( nArg < 1 ){` |
|       3 | 2167 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2168 | `			"ArgumentCountError",` |
|       - | 2169 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2170 | `			);` |
|       - | 2171 | `	}` |
|       - | 2172 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2173 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2174 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2175 | `			"TypeError",` |
|       - | 2176 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2177 | `			ph7_type_name(apArg[0])` |
|       - | 2178 | `			);` |
|       - | 2179 | `	}` |
|       - | 2180 | `	/* Point to the internal representation of the input hashmap */` |
|      24 | 2181 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      24 | 2182 | `	if( pMap->nEntry > 1 ){` |
|      20 | 2183 | `		sxi32 iCmpFlags = 0;` |
|      20 | 2184 | `		if( nArg > 1 ){` |
|       - | 2185 | `			/* Extract comparison flags */` |
|       5 | 2186 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2187 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2188 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2189 | `			}` |
|       2 | 2190 | `		}` |
|       - | 2191 | `		/* Do the merge sort */` |
|      20 | 2192 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2193 | `		/* Fix the last link broken by the merge */` |
|      36 | 2194 | `		while(pMap->pLast->pPrev){` |
|      18 | 2195 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       2 | 2196 | `		}` |
|       9 | 2197 | `	}` |
|       - | 2198 | `	/* All done,return TRUE */` |
|      24 | 2199 | `	ph7_result_bool(pCtx,1);` |
|      24 | 2200 | `	return PH7_OK;` |
|      18 | 2201 |  |
|       - | 2202 | `/*` |
|       - | 2203 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2204 | ` *  Sort an array by key.` |
|       - | 2205 | ` * Parameters` |
|       - | 2206 | ` *  $array` |
|       - | 2207 | ` *   The input array.` |
|       - | 2208 | ` * $sort_flags` |
|       - | 2209 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2210 | ` *  Sorting type flags:` |
|       - | 2211 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2212 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2213 | ` *   SORT_STRING - compare items as strings` |
|       - | 2214 | ` * Return` |
|       - | 2215 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2216 | ` */` |
|       4 | 2217 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2218 |  |
|       - | 2219 | `	ph7_hashmap *pMap;` |
|       - | 2220 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2221 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2222 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2223 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2224 | `		return PH7_OK;` |
|       - | 2225 | `	}` |
|       - | 2226 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2227 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2228 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2229 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2230 | `		if( nArg > 1 ){` |
|       - | 2231 | `			/* Extract comparison flags */` |
|     ! 0 | 2232 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2233 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2234 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2235 | `			}` |
|     ! 0 | 2236 | `		}` |
|       - | 2237 | `		/* Do the merge sort */` |
|       5 | 2238 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2239 | `		/* Fix the last link broken by the merge */` |
|      15 | 2240 | `		while(pMap->pLast->pPrev){` |
|      11 | 2241 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2242 | `		}` |
|       2 | 2243 | `	}` |
|       - | 2244 | `	/* All done,return TRUE */` |
|       5 | 2245 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2246 | `	return PH7_OK;` |
|       3 | 2247 |  |
|       - | 2248 | `/*` |
|       - | 2249 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2250 | ` *  Sort an array by key in reverse order.` |
|       - | 2251 | ` * Parameters` |
|       - | 2252 | ` *  $array` |
|       - | 2253 | ` *   The input array.` |
|       - | 2254 | ` * $sort_flags` |
|       - | 2255 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2256 | ` *  Sorting type flags:` |
|       - | 2257 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2258 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2259 | ` *   SORT_STRING - compare items as strings` |
|       - | 2260 | ` * Return` |
|       - | 2261 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2262 | ` */` |
|       2 | 2263 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2264 |  |
|       - | 2265 | `	ph7_hashmap *pMap;` |
|       - | 2266 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2267 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2268 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2269 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2270 | `		return PH7_OK;` |
|       - | 2271 | `	}` |
|       - | 2272 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2273 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2274 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2275 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2276 | `		if( nArg > 1 ){` |
|       - | 2277 | `			/* Extract comparison flags */` |
|     ! 0 | 2278 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2279 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2280 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2281 | `			}` |
|     ! 0 | 2282 | `		}` |
|       - | 2283 | `		/* Do the merge sort */` |
|       3 | 2284 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2285 | `		/* Fix the last link broken by the merge */` |
|       7 | 2286 | `		while(pMap->pLast->pPrev){` |
|       5 | 2287 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2288 | `		}` |
|       1 | 2289 | `	}` |
|       - | 2290 | `	/* All done,return TRUE */` |
|       3 | 2291 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2292 | `	return PH7_OK;` |
|       2 | 2293 |  |
|       - | 2294 | `/*` |
|       - | 2295 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2296 | ` * Sort an array in reverse order.` |
|       - | 2297 | ` * Parameters` |
|       - | 2298 | ` *  $array` |
|       - | 2299 | ` *   The input array.` |
|       - | 2300 | ` * $sort_flags` |
|       - | 2301 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2302 | ` *  Sorting type flags:` |
|       - | 2303 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2304 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2305 | ` *   SORT_STRING - compare items as strings` |
|       - | 2306 | ` * Return` |
|       - | 2307 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2308 | ` */` |
|       2 | 2309 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2310 |  |
|       - | 2311 | `	ph7_hashmap *pMap;` |
|       - | 2312 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2313 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2314 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2315 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2316 | `		return PH7_OK;` |
|       - | 2317 | `	}` |
|       - | 2318 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2319 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2320 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2321 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2322 | `		if( nArg > 1 ){` |
|       - | 2323 | `			/* Extract comparison flags */` |
|     ! 0 | 2324 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2325 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2326 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2327 | `			}` |
|     ! 0 | 2328 | `		}` |
|       - | 2329 | `		/* Do the merge sort */` |
|       3 | 2330 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2331 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2332 | `		HashmapSortRehash(pMap);` |
|       1 | 2333 | `	}` |
|       - | 2334 | `	/* All done,return TRUE */` |
|       3 | 2335 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2336 | `	return PH7_OK;` |
|       2 | 2337 |  |
|       - | 2338 | `/*` |
|       - | 2339 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2340 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2341 | ` * Parameters` |
|       - | 2342 | ` *  $array` |
|       - | 2343 | ` *   The input array.` |
|       - | 2344 | ` * $cmp_function` |
|       - | 2345 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2346 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2347 | ` *  to, or greater than the second.` |
|       - | 2348 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2349 | ` * Return` |
|       - | 2350 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2351 | ` */` |
|       2 | 2352 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2353 |  |
|       - | 2354 | `	ph7_hashmap *pMap;` |
|       - | 2355 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2356 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2357 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2358 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2359 | `		return PH7_OK;` |
|       - | 2360 | `	}` |
|       - | 2361 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2362 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2363 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2364 | `		ph7_value *pCallback = 0;` |
|       - | 2365 | `		ProcNodeCmp xCmp;` |
|       3 | 2366 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2367 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2368 | `			/* Point to the desired callback */` |
|       3 | 2369 | `			pCallback = apArg[1];` |
|       2 | 2370 | `		}else{` |
|       - | 2371 | `			/* Use the default comparison function */` |
|     ! 0 | 2372 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2373 | `		}` |
|       - | 2374 | `		/* Do the merge sort */` |
|       3 | 2375 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2376 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2377 | `		HashmapSortRehash(pMap);` |
|       1 | 2378 | `	}` |
|       - | 2379 | `	/* All done,return TRUE */` |
|       3 | 2380 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2381 | `	return PH7_OK;` |
|       2 | 2382 |  |
|       - | 2383 | `/*` |
|       - | 2384 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2385 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2386 | ` *  and maintain index association.` |
|       - | 2387 | ` * Parameters` |
|       - | 2388 | ` *  $array` |
|       - | 2389 | ` *   The input array.` |
|       - | 2390 | ` * $cmp_function` |
|       - | 2391 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2392 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2393 | ` *  to, or greater than the second.` |
|       - | 2394 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2395 | ` * Return` |
|       - | 2396 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2397 | ` */` |
|       2 | 2398 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2399 |  |
|       - | 2400 | `	ph7_hashmap *pMap;` |
|       - | 2401 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2402 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2403 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2404 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2405 | `		return PH7_OK;` |
|       - | 2406 | `	}` |
|       - | 2407 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2408 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2409 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2410 | `		ph7_value *pCallback = 0;` |
|       - | 2411 | `		ProcNodeCmp xCmp;` |
|       3 | 2412 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2413 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2414 | `			/* Point to the desired callback */` |
|       3 | 2415 | `			pCallback = apArg[1];` |
|       2 | 2416 | `		}else{` |
|       - | 2417 | `			/* Use the default comparison function */` |
|     ! 0 | 2418 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2419 | `		}` |
|       - | 2420 | `		/* Do the merge sort */` |
|       3 | 2421 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2422 | `		/* Fix the last link broken by the merge */` |
|       5 | 2423 | `		while(pMap->pLast->pPrev){` |
|       3 | 2424 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2425 | `		}` |
|       1 | 2426 | `	}` |
|       - | 2427 | `	/* All done,return TRUE */` |
|       3 | 2428 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2429 | `	return PH7_OK;` |
|       2 | 2430 |  |
|       - | 2431 | `/*` |
|       - | 2432 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2433 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2434 | ` *  function and maintain index association.` |
|       - | 2435 | ` * Parameters` |
|       - | 2436 | ` *  $array` |
|       - | 2437 | ` *   The input array.` |
|       - | 2438 | ` * $cmp_function` |
|       - | 2439 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2440 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2441 | ` *  to, or greater than the second.` |
|       - | 2442 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2443 | ` * Return` |
|       - | 2444 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2445 | ` */` |
|       2 | 2446 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2447 |  |
|       - | 2448 | `	ph7_hashmap *pMap;` |
|       - | 2449 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2450 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2451 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2452 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2453 | `		return PH7_OK;` |
|       - | 2454 | `	}` |
|       - | 2455 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2456 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2457 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2458 | `		ph7_value *pCallback = 0;` |
|       - | 2459 | `		ProcNodeCmp xCmp;` |
|       3 | 2460 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2461 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2462 | `			/* Point to the desired callback */` |
|       3 | 2463 | `			pCallback = apArg[1];` |
|       2 | 2464 | `		}else{` |
|       - | 2465 | `			/* Use the default comparison function */` |
|     ! 0 | 2466 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2467 | `		}` |
|       - | 2468 | `		/* Do the merge sort */` |
|       3 | 2469 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2470 | `		/* Fix the last link broken by the merge */` |
|       3 | 2471 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2472 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2473 | `		}` |
|       1 | 2474 | `	}` |
|       - | 2475 | `	/* All done,return TRUE */` |
|       3 | 2476 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2477 | `	return PH7_OK;` |
|       2 | 2478 |  |
|       - | 2479 | `/*` |
|       - | 2480 | ` * bool shuffle(array &$array)` |
|       - | 2481 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2482 | ` * Parameters` |
|       - | 2483 | ` *  $array` |
|       - | 2484 | ` *   The input array.` |
|       - | 2485 | ` * Return` |
|       - | 2486 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2487 | ` *` |
|       - | 2488 | ` */` |
|       2 | 2489 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2490 |  |
|       - | 2491 | `	ph7_hashmap *pMap;` |
|       - | 2492 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2493 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2494 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2495 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2496 | `		return PH7_OK;` |
|       - | 2497 | `	}` |
|       - | 2498 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2499 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2500 | `	if( pMap->nEntry > 1 ){` |
|       - | 2501 | `		/* Do the merge sort */` |
|       3 | 2502 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2503 | `		/* Fix the last link broken by the merge */` |
|       7 | 2504 | `		while(pMap->pLast->pPrev){` |
|       5 | 2505 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2506 | `		}` |
|       1 | 2507 | `	}` |
|       - | 2508 | `	/* All done,return TRUE */` |
|       3 | 2509 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2510 | `	return PH7_OK;` |
|       2 | 2511 |  |
|       - | 2512 | `/*` |
|       - | 2513 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2514 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2515 | ` * Parameters` |
|       - | 2516 | ` *  $var` |
|       - | 2517 | ` *   The array or the object.` |
|       - | 2518 | ` * $mode` |
|       - | 2519 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2520 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2521 | ` *  all the elements of a multidimensional array.` |
|       - | 2522 | ` * Return` |
|       - | 2523 | ` *  Returns the number of elements in the array.` |
|       - | 2524 | ` */` |
|     640 | 2525 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2526 |  |
|     642 | 2527 | `	int bRecursive = FALSE;` |
|     642 | 2528 | `	int bCycleDetected = FALSE;` |
|       - | 2529 | `	sxi64 iCount;` |
|     642 | 2530 | `	if( nArg < 1 ){` |
|       3 | 2531 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2532 | `			"ArgumentCountError",` |
|       - | 2533 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2534 | `			);` |
|       - | 2535 | `	}` |
|     640 | 2536 | `	if( nArg > 2 ){` |
|       4 | 2537 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2538 | `			"ArgumentCountError",` |
|       - | 2539 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2540 | `			nArg` |
|       - | 2541 | `			);` |
|       - | 2542 | `	}` |
|     638 | 2543 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2544 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2545 | `			"TypeError",` |
|       - | 2546 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       5 | 2547 | `			ph7_type_name(apArg[0])` |
|       - | 2548 | `			);` |
|       - | 2549 | `	}` |
|     628 | 2550 | `	if( nArg > 1 ){` |
|      34 | 2551 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      34 | 2552 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       5 | 2553 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2554 | `				"ValueError",` |
|       - | 2555 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2556 | `				);` |
|       - | 2557 | `		}` |
|      29 | 2558 | `		bRecursive = iMode == 1;` |
|      14 | 2559 | `	}` |
|       - | 2560 | `	/* Count */` |
|     624 | 2561 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     624 | 2562 | `	if( bCycleDetected ){` |
|       3 | 2563 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2564 | `	}` |
|     624 | 2565 | `	ph7_result_int64(pCtx,iCount);` |
|     624 | 2566 | `	return PH7_OK;` |
|     322 | 2567 |  |
|       - | 2568 | `/*` |
|       - | 2569 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2570 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2571 | ` * Parameters` |
|       - | 2572 | ` * $key` |
|       - | 2573 | ` *   Value to check.` |
|       - | 2574 | ` * $search` |
|       - | 2575 | ` *  An array with keys to check.` |
|       - | 2576 | ` * Return` |
|       - | 2577 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2578 | ` */` |
|      66 | 2579 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2580 |  |
|       - | 2581 | `	sxi32 rc;` |
|      68 | 2582 | `	if( nArg != 2 ){` |
|       - | 2583 | `		/* PHP requires exactly two arguments */` |
|      10 | 2584 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2585 | `			"ArgumentCountError",` |
|       - | 2586 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2587 | `			nArg` |
|       - | 2588 | `			);` |
|       - | 2589 | `	}` |
|       - | 2590 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 2591 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2592 | `		/* Type mismatch -> TypeError */` |
|       7 | 2593 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2594 | `			"TypeError",` |
|       - | 2595 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2596 | `			ph7_type_name(apArg[1])` |
|       - | 2597 | `			);` |
|       - | 2598 | `	}` |
|       - | 2599 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      57 | 2600 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2601 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2602 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2603 | `			"use an empty string instead"` |
|       - | 2604 | `			);` |
|      56 | 2605 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2606 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2607 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2608 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2609 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2610 | `				,rVal` |
|       - | 2611 | `				);` |
|       1 | 2612 | `		}` |
|       1 | 2613 | `	}` |
|       - | 2614 | `	/* Perform the lookup */` |
|      57 | 2615 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2616 | `	/* lookup result */` |
|      57 | 2617 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      57 | 2618 | `	return PH7_OK;` |
|      35 | 2619 |  |
|       - | 2620 | `/*` |
|       - | 2621 | ` * value array_pop(array $array)` |
|       - | 2622 | ` *   POP the last inserted element from the array.` |
|       - | 2623 | ` * Parameter` |
|       - | 2624 | ` *  The array to get the value from.` |
|       - | 2625 | ` * Return` |
|       - | 2626 | ` *  Poped value or NULL on failure.` |
|       - | 2627 | ` */` |
|      16 | 2628 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2629 |  |
|       - | 2630 | `	ph7_hashmap *pMap;` |
|       - | 2631 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      18 | 2632 | `	if( nArg != 1 ){` |
|       7 | 2633 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2634 | `			"ArgumentCountError",` |
|       - | 2635 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2636 | `			nArg` |
|       - | 2637 | `			);` |
|       - | 2638 | `	}` |
|       - | 2639 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2640 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      14 | 2641 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2642 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2643 | `			"Error",` |
|       - | 2644 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2645 | `			);` |
|       - | 2646 | `	}` |
|       - | 2647 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 | 2648 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2649 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2650 | `			"TypeError",` |
|       - | 2651 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2652 | `			ph7_type_name(apArg[0])` |
|       - | 2653 | `			);` |
|       - | 2654 | `	}` |
|       7 | 2655 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 2656 | `	if( pMap->nEntry < 1 ){` |
|       - | 2657 | `		/* Nothing to pop,return NULL */` |
|       3 | 2658 | `		ph7_result_null(pCtx);` |
|       2 | 2659 | `	}else{` |
|       5 | 2660 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2661 | `		ph7_value *pObj;` |
|       5 | 2662 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       5 | 2663 | `		if( pObj ){` |
|       - | 2664 | `			/* Node value */` |
|       5 | 2665 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2666 | `			/* Unlink the node */` |
|       5 | 2667 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       3 | 2668 | `		}else{` |
|     ! 0 | 2669 | `			ph7_result_null(pCtx);` |
|       - | 2670 | `		}` |
|       - | 2671 | `		/* Reset the cursor */` |
|       5 | 2672 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2673 | `	}` |
|       7 | 2674 | `	return PH7_OK;` |
|      10 | 2675 |  |
|       - | 2676 | `/*` |
|       - | 2677 | ` * int array_push($array,$var,...)` |
|       - | 2678 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2679 | ` * Parameters` |
|       - | 2680 | ` *  array` |
|       - | 2681 | ` *    The input array.` |
|       - | 2682 | ` *  var` |
|       - | 2683 | ` *   On or more value to push.` |
|       - | 2684 | ` * Return` |
|       - | 2685 | ` *  New array count (including old items).` |
|       - | 2686 | ` */` |
|      20 | 2687 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2688 |  |
|       - | 2689 | `	ph7_hashmap *pMap;` |
|       - | 2690 | `	sxi32 rc;` |
|       - | 2691 | `	int i;` |
|      22 | 2692 | `	if( nArg < 1 ){` |
|       4 | 2693 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2694 | `			"ArgumentCountError",` |
|       - | 2695 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2696 | `			nArg` |
|       - | 2697 | `			);` |
|       - | 2698 | `	}` |
|       - | 2699 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2700 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      20 | 2701 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2702 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2703 | `			"Error",` |
|       - | 2704 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2705 | `			);` |
|       - | 2706 | `	}` |
|       - | 2707 | `	/* Make sure we are dealing with a valid hashmap */` |
|      16 | 2708 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2709 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2710 | `			"TypeError",` |
|       - | 2711 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2712 | `			ph7_type_name(apArg[0])` |
|       - | 2713 | `			);` |
|       - | 2714 | `	}` |
|       - | 2715 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 2716 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2717 | `	/* Start pushing given values */` |
|      27 | 2718 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      15 | 2719 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      15 | 2720 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2721 | `			break;` |
|       - | 2722 | `		}` |
|       8 | 2723 | `	}` |
|       - | 2724 | `	/* Return the new count */` |
|      13 | 2725 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      13 | 2726 | `	return PH7_OK;` |
|      12 | 2727 |  |
|       - | 2728 | `/*` |
|       - | 2729 | ` * value array_shift(array $array)` |
|       - | 2730 | ` *   Shift an element off the beginning of array.` |
|       - | 2731 | ` * Parameter` |
|       - | 2732 | ` *  The array to get the value from.` |
|       - | 2733 | ` * Return` |
|       - | 2734 | ` *  Shifted value or NULL on failure.` |
|       - | 2735 | ` */` |
|      36 | 2736 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2737 |  |
|       - | 2738 | `	ph7_hashmap *pMap;` |
|       - | 2739 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      38 | 2740 | `	if( nArg != 1 ){` |
|       7 | 2741 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2742 | `			"ArgumentCountError",` |
|       - | 2743 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2744 | `			nArg` |
|       - | 2745 | `			);` |
|       - | 2746 | `	}` |
|       - | 2747 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      34 | 2748 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2749 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2750 | `			"Error",` |
|       - | 2751 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2752 | `			);` |
|       - | 2753 | `	}` |
|       - | 2754 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 | 2755 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2756 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2757 | `			"TypeError",` |
|       - | 2758 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2759 | `			ph7_type_name(apArg[0])` |
|       - | 2760 | `			);` |
|       - | 2761 | `	}` |
|       - | 2762 | `	/* Point to the internal representation of the hashmap */` |
|      28 | 2763 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      28 | 2764 | `	if( pMap->nEntry < 1 ){` |
|       - | 2765 | `		/* Empty hashmap,return NULL */` |
|       3 | 2766 | `		ph7_result_null(pCtx);` |
|       2 | 2767 | `	}else{` |
|      26 | 2768 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2769 | `		ph7_value *pObj;` |
|       - | 2770 | `		sxu32 n;` |
|      26 | 2771 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      26 | 2772 | `		if( pObj ){` |
|       - | 2773 | `			/* Node value */` |
|      26 | 2774 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2775 | `			/* Unlink the first node */` |
|      26 | 2776 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      14 | 2777 | `		}else{` |
|     ! 0 | 2778 | `			ph7_result_null(pCtx);` |
|       - | 2779 | `		}` |
|       - | 2780 | `		/* Rehash all int keys */` |
|      26 | 2781 | `		n = pMap->nEntry;` |
|      26 | 2782 | `		pEntry = pMap->pFirst;` |
|      26 | 2783 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      37 | 2784 | `		for(;;){` |
|      76 | 2785 | `			if( n < 1 ){` |
|      26 | 2786 | `				break;` |
|       - | 2787 | `			}` |
|      52 | 2788 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      52 | 2789 | `				HashmapRehashIntNode(pEntry);` |
|      25 | 2790 | `			}` |
|       - | 2791 | `			/* Point to the next entry */` |
|      52 | 2792 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      52 | 2793 | `			n--;` |
|       2 | 2794 | `		}` |
|       - | 2795 | `		/* Reset the cursor */` |
|      26 | 2796 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2797 | `	}` |
|      28 | 2798 | `	return PH7_OK;` |
|      20 | 2799 |  |
|       - | 2800 | `/*` |
|       - | 2801 | ` * Extract the node cursor value.` |
|       - | 2802 | ` */` |
|      24 | 2803 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 2804 |  |
|      25 | 2805 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 2806 | `	ph7_value *pVal;` |
|      25 | 2807 | `	if( pCur == 0 ){` |
|       - | 2808 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 2809 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2810 | `		return PH7_OK;` |
|       - | 2811 | `	}` |
|      25 | 2812 | `	if( iDirection != 0 ){` |
|       9 | 2813 | `		if( iDirection > 0 ){` |
|       - | 2814 | `			/* Point to the next entry */` |
|       7 | 2815 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 2816 | `			pCur = pMap->pCur;` |
|       4 | 2817 | `		}else{` |
|       - | 2818 | `			/* Point to the previous entry */` |
|       3 | 2819 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 2820 | `			pCur = pMap->pCur;` |
|       - | 2821 | `		}` |
|       9 | 2822 | `		if( pCur == 0 ){` |
|       - | 2823 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 2824 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2825 | `			return PH7_OK;` |
|       - | 2826 | `		}` |
|       4 | 2827 | `	}` |
|       - | 2828 | `	/* Point to the desired element */` |
|      25 | 2829 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 2830 | `	if( pVal ){` |
|      25 | 2831 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 2832 | `	}else{` |
|     ! 0 | 2833 | `		ph7_result_bool(pCtx,0);` |
|       - | 2834 | `	}` |
|      25 | 2835 | `	return PH7_OK;` |
|      13 | 2836 |  |
|       - | 2837 | `/*` |
|       - | 2838 | ` * value current(array $array)` |
|       - | 2839 | ` *  Return the current element in an array.` |
|       - | 2840 | ` * Parameter` |
|       - | 2841 | ` *  $input: The input array.` |
|       - | 2842 | ` * Return` |
|       - | 2843 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 2844 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2845 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2846 | ` *  is empty, current() returns FALSE.` |
|       - | 2847 | ` */` |
|      10 | 2848 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2849 |  |
|      11 | 2850 | `	if( nArg < 1 ){` |
|       - | 2851 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2852 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2853 | `		return PH7_OK;` |
|       - | 2854 | `	}` |
|       - | 2855 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 2856 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2857 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2858 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2859 | `		return PH7_OK;` |
|       - | 2860 | `	}` |
|      11 | 2861 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 2862 | `	return PH7_OK;` |
|       6 | 2863 |  |
|       - | 2864 | `/*` |
|       - | 2865 | ` * value next(array $input)` |
|       - | 2866 | ` *  Advance the internal array pointer of an array.` |
|       - | 2867 | ` * Parameter` |
|       - | 2868 | ` *  $input: The input array.` |
|       - | 2869 | ` * Return` |
|       - | 2870 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 2871 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 2872 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 2873 | ` */` |
|       6 | 2874 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2875 |  |
|       7 | 2876 | `	if( nArg < 1 ){` |
|       - | 2877 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2878 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2879 | `		return PH7_OK;` |
|       - | 2880 | `	}` |
|       - | 2881 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 2882 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2883 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2884 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2885 | `		return PH7_OK;` |
|       - | 2886 | `	}` |
|       7 | 2887 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 2888 | `	return PH7_OK;` |
|       4 | 2889 |  |
|       - | 2890 | `/*` |
|       - | 2891 | ` * value prev(array $input)` |
|       - | 2892 | ` *  Rewind the internal array pointer.` |
|       - | 2893 | ` * Parameter` |
|       - | 2894 | ` *  $input: The input array.` |
|       - | 2895 | ` * Return` |
|       - | 2896 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 2897 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 2898 | ` *  elements.` |
|       - | 2899 | ` */` |
|       2 | 2900 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2901 |  |
|       3 | 2902 | `	if( nArg < 1 ){` |
|       - | 2903 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2904 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2905 | `		return PH7_OK;` |
|       - | 2906 | `	}` |
|       - | 2907 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2908 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2909 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2910 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2911 | `		return PH7_OK;` |
|       - | 2912 | `	}` |
|       3 | 2913 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 2914 | `	return PH7_OK;` |
|       2 | 2915 |  |
|       - | 2916 | `/*` |
|       - | 2917 | ` * value end(array $input)` |
|       - | 2918 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 2919 | ` * Parameter` |
|       - | 2920 | ` *  $input: The input array.` |
|       - | 2921 | ` * Return` |
|       - | 2922 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 2923 | ` */` |
|       2 | 2924 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2925 |  |
|       - | 2926 | `	ph7_hashmap *pMap;` |
|       3 | 2927 | `	if( nArg < 1 ){` |
|       - | 2928 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2929 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2930 | `		return PH7_OK;` |
|       - | 2931 | `	}` |
|       - | 2932 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2933 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2934 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2935 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2936 | `		return PH7_OK;` |
|       - | 2937 | `	}` |
|       - | 2938 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2939 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2940 | `	/* Point to the last node */` |
|       3 | 2941 | `	pMap->pCur = pMap->pLast;` |
|       - | 2942 | `	/* Return the last node value */` |
|       3 | 2943 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 2944 | `	return PH7_OK;` |
|       2 | 2945 |  |
|       - | 2946 | `/*` |
|       - | 2947 | ` * value reset(array $array )` |
|       - | 2948 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 2949 | ` * Parameter` |
|       - | 2950 | ` *  $input: The input array.` |
|       - | 2951 | ` * Return` |
|       - | 2952 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 2953 | ` */` |
|       4 | 2954 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2955 |  |
|       - | 2956 | `	ph7_hashmap *pMap;` |
|       5 | 2957 | `	if( nArg < 1 ){` |
|       - | 2958 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2959 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2960 | `		return PH7_OK;` |
|       - | 2961 | `	}` |
|       - | 2962 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2963 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2964 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2965 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2966 | `		return PH7_OK;` |
|       - | 2967 | `	}` |
|       - | 2968 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2969 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2970 | `	/* Point to the first node */` |
|       5 | 2971 | `	pMap->pCur = pMap->pFirst;` |
|       - | 2972 | `	/* Return the last node value if available */` |
|       5 | 2973 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 2974 | `	return PH7_OK;` |
|       3 | 2975 |  |
|       - | 2976 | `/*` |
|       - | 2977 | ` * value key(array $array)` |
|       - | 2978 | ` *   Fetch a key from an array` |
|       - | 2979 | ` * Parameter` |
|       - | 2980 | ` *  $input` |
|       - | 2981 | ` *   The input array.` |
|       - | 2982 | ` * Return` |
|       - | 2983 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 2984 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2985 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2986 | ` *  is empty, key() returns NULL.` |
|       - | 2987 | ` */` |
|       4 | 2988 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2989 |  |
|       - | 2990 | `	ph7_hashmap_node *pCur;` |
|       - | 2991 | `	ph7_hashmap *pMap;` |
|       5 | 2992 | `	if( nArg < 1 ){` |
|       - | 2993 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 2994 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2995 | `		return PH7_OK;` |
|       - | 2996 | `	}` |
|       - | 2997 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2998 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2999 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3000 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3001 | `		return PH7_OK;` |
|       - | 3002 | `	}` |
|       5 | 3003 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3004 | `	pCur = pMap->pCur;` |
|       5 | 3005 | `	if( pCur == 0 ){` |
|       - | 3006 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3007 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3008 | `		return PH7_OK;` |
|       - | 3009 | `	}` |
|       5 | 3010 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3011 | `		/* Key is integer */` |
|     ! 0 | 3012 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3013 | `	}else{` |
|       - | 3014 | `		/* Key is blob */` |
|       7 | 3015 | `		ph7_result_string(pCtx,` |
|       4 | 3016 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3017 | `	}` |
|       5 | 3018 | `	return PH7_OK;` |
|       3 | 3019 |  |
|       - | 3020 | `/*` |
|       - | 3021 | ` * array each(array $input)` |
|       - | 3022 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3023 | ` * Parameter` |
|       - | 3024 | ` *  $input` |
|       - | 3025 | ` *    The input array.` |
|       - | 3026 | ` * Return` |
|       - | 3027 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3028 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3029 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3030 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3031 | ` *  each() returns FALSE.` |
|       - | 3032 | ` */` |
|      22 | 3033 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3034 |  |
|       - | 3035 | `	ph7_hashmap_node *pCur;` |
|       - | 3036 | `	ph7_hashmap *pMap;` |
|       - | 3037 | `	ph7_value *pArray;` |
|       - | 3038 | `	ph7_value *pVal;` |
|       - | 3039 | `	ph7_value sKey;` |
|      23 | 3040 | `	if( nArg < 1 ){` |
|       - | 3041 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3042 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3043 | `		return PH7_OK;` |
|       - | 3044 | `	}` |
|       - | 3045 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3046 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3047 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3048 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3049 | `		return PH7_OK;` |
|       - | 3050 | `	}` |
|       - | 3051 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3052 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3053 | `	if( pMap->pCur == 0 ){` |
|       - | 3054 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3055 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3056 | `		return PH7_OK;` |
|       - | 3057 | `	}` |
|      15 | 3058 | `	pCur = pMap->pCur;` |
|       - | 3059 | `	/* Create a new array */` |
|      15 | 3060 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3061 | `	if( pArray == 0 ){` |
|     ! 0 | 3062 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3063 | `		return PH7_OK;` |
|       - | 3064 | `	}` |
|      15 | 3065 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3066 | `	/* Insert the current value */` |
|      15 | 3067 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3068 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3069 | `	/* Make the key */` |
|      15 | 3070 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3071 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3072 | `	}else{` |
|       9 | 3073 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3074 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3075 | `	}` |
|       - | 3076 | `	/* Insert the current key */` |
|      15 | 3077 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3078 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3079 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3080 | `	/* Advance the cursor */` |
|      15 | 3081 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3082 | `	/* Return the current entry */` |
|      15 | 3083 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3084 | `	return PH7_OK;` |
|      12 | 3085 |  |
|       - | 3086 | `/*` |
|       - | 3087 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3088 | ` *  Create an array containing a range of elements` |
|       - | 3089 | ` * Parameter` |
|       - | 3090 | ` *  start` |
|       - | 3091 | ` *   First value of the sequence.` |
|       - | 3092 | ` *  limit` |
|       - | 3093 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3094 | ` *  step` |
|       - | 3095 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3096 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3097 | ` * Return` |
|       - | 3098 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3099 | ` * NOTE:` |
|       - | 3100 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3101 | ` */` |
|       2 | 3102 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3103 |  |
|       - | 3104 | `	ph7_value *pValue,*pArray;` |
|       - | 3105 | `	sxi64 iOfft,iLimit;` |
|       3 | 3106 | `	int iStep = 1;` |
|       - | 3107 |  |
|       3 | 3108 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3109 | `	if( nArg > 0 ){` |
|       - | 3110 | `		/* Extract the offset */` |
|       3 | 3111 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3112 | `		if( nArg > 1 ){` |
|       - | 3113 | `			/* Extract the limit */` |
|       3 | 3114 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3115 | `			if( nArg > 2 ){` |
|       - | 3116 | `				/* Extract the increment */` |
|       3 | 3117 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3118 | `				if( iStep < 1 ){` |
|       - | 3119 | `					/* Only positive number are allowed */` |
|       3 | 3120 | `					iStep = 1;` |
|       1 | 3121 | `				}` |
|       1 | 3122 | `			}` |
|       1 | 3123 | `		}` |
|       1 | 3124 | `	}` |
|       - | 3125 | `	/* Element container */` |
|       3 | 3126 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3127 | `	/* Create the new array */` |
|       3 | 3128 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3129 | `	if( pArray == 0 ){` |
|     ! 0 | 3130 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3131 | `		return PH7_OK;` |
|       - | 3132 | `	}` |
|       - | 3133 | `	/* Start filling */` |
|       3 | 3134 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3135 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3136 | `		/* Perform the insertion */` |
|     ! 0 | 3137 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3138 | `		/* Increment */` |
|     ! 0 | 3139 | `		iOfft += iStep;` |
|     ! 0 | 3140 | `	}` |
|       - | 3141 | `	/* Return the new array */` |
|       3 | 3142 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3143 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3144 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3145 | `	 */` |
|       3 | 3146 | `	return PH7_OK;` |
|       2 | 3147 |  |
|       - | 3148 | `/*` |
|       - | 3149 | ` * array array_values(array $array)` |
|       - | 3150 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3151 | ` * Parameters` |
|       - | 3152 | ` *  $array` |
|       - | 3153 | ` *   The input array.` |
|       - | 3154 | ` * Return` |
|       - | 3155 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3156 | ` */` |
|      30 | 3157 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3158 |  |
|       - | 3159 | `	ph7_hashmap_node *pNode;` |
|       - | 3160 | `	ph7_hashmap *pMap;` |
|       - | 3161 | `	ph7_value *pArray;` |
|       - | 3162 | `	ph7_value *pObj;` |
|       - | 3163 | `	sxu32 n;` |
|      32 | 3164 | `	if( nArg != 1 ){` |
|       - | 3165 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3166 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3167 | `			"ArgumentCountError",` |
|       - | 3168 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3169 | `			nArg` |
|       - | 3170 | `			);` |
|       - | 3171 | `	}` |
|       - | 3172 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3173 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3174 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3175 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3176 | `			"TypeError",` |
|       - | 3177 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3178 | `			ph7_type_name(apArg[0])` |
|       - | 3179 | `			);` |
|       - | 3180 | `	}` |
|       - | 3181 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3182 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3183 | `	/* Create a new array */` |
|      25 | 3184 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3185 | `	if( pArray == 0 ){` |
|     ! 0 | 3186 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3187 | `		return PH7_OK;` |
|       - | 3188 | `	}` |
|       - | 3189 | `	/* Perform the requested operation */` |
|      25 | 3190 | `	pNode = pMap->pFirst;` |
|      83 | 3191 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3192 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3193 | `		if( pObj ){` |
|       - | 3194 | `			/* perform the insertion */` |
|      59 | 3195 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3196 | `		}` |
|       - | 3197 | `		/* Point to the next entry */` |
|      59 | 3198 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3199 | `	}` |
|       - | 3200 | `	/* return the new array */` |
|      25 | 3201 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3202 | `	return PH7_OK;` |
|      17 | 3203 |  |
|       - | 3204 | `/*` |
|       - | 3205 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3206 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3207 | ` * Parameters` |
|       - | 3208 | ` *  $input` |
|       - | 3209 | ` *   An array containing keys to return.` |
|       - | 3210 | ` * $search_value` |
|       - | 3211 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3212 | ` * $strict` |
|       - | 3213 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3214 | ` * Return` |
|       - | 3215 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3216 | ` */` |
|     120 | 3217 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3218 |  |
|       - | 3219 | `	ph7_hashmap_node *pNode;` |
|       - | 3220 | `	ph7_hashmap *pMap;` |
|       - | 3221 | `	ph7_value *pArray;` |
|       - | 3222 | `	ph7_value sObj;` |
|       - | 3223 | `	ph7_value sVal;` |
|       - | 3224 | `	SyString sKey;` |
|       - | 3225 | `	int bStrict;` |
|       - | 3226 | `	sxi32 rc;` |
|       - | 3227 | `	sxu32 n;` |
|     122 | 3228 | `	if( nArg < 1 ){` |
|       - | 3229 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3230 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3231 | `			"ArgumentCountError",` |
|       - | 3232 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3233 | `			);` |
|       - | 3234 | `	}` |
|       - | 3235 | `	/* Make sure we are dealing with a valid hashmap */` |
|     120 | 3236 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3237 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3238 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3239 | `			"TypeError",` |
|       - | 3240 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3241 | `			ph7_type_name(apArg[0])` |
|       - | 3242 | `			);` |
|       - | 3243 | `	}` |
|       - | 3244 | `	/* Point to the internal representation of the input hashmap */` |
|     118 | 3245 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3246 | `	/* Create a new array */` |
|     118 | 3247 | `	pArray = ph7_context_new_array(pCtx);` |
|     118 | 3248 | `	if( pArray == 0 ){` |
|     ! 0 | 3249 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3250 | `		return PH7_OK;` |
|       - | 3251 | `	}` |
|     118 | 3252 | `	bStrict = FALSE;` |
|     118 | 3253 | `	if( nArg > 2 ){` |
|       - | 3254 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3255 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3256 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3257 | `				"TypeError",` |
|       - | 3258 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3259 | `				ph7_type_name(apArg[2])` |
|       - | 3260 | `				);` |
|       - | 3261 | `		}` |
|       5 | 3262 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3263 | `	}` |
|       - | 3264 | `	/* Perform the requested operation */` |
|     115 | 3265 | `	pNode = pMap->pFirst;` |
|     115 | 3266 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     553 | 3267 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     439 | 3268 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     117 | 3269 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      59 | 3270 | `		}else{` |
|     323 | 3271 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3272 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3273 | `		}` |
|     439 | 3274 | `		rc = 0;` |
|     439 | 3275 | `		if( nArg > 1 ){` |
|      31 | 3276 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3277 | `			if( pValue ){` |
|      31 | 3278 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3279 | `				/* Filter key */` |
|      31 | 3280 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3281 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3282 | `			}` |
|      15 | 3283 | `		}` |
|     439 | 3284 | `		if( rc == 0 ){` |
|       - | 3285 | `			/* Perform the insertion */` |
|     421 | 3286 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     210 | 3287 | `		}` |
|     439 | 3288 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3289 | `		/* Point to the next entry */` |
|     439 | 3290 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     220 | 3291 | `	}` |
|       - | 3292 | `	/* return the new array */` |
|     115 | 3293 | `	ph7_result_value(pCtx,pArray);` |
|     115 | 3294 | `	return PH7_OK;` |
|      62 | 3295 |  |
|       - | 3296 | `/*` |
|       - | 3297 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3298 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3299 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3300 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3301 | ` * Parameters` |
|       - | 3302 | ` *  $arr1` |
|       - | 3303 | ` *   First array` |
|       - | 3304 | ` *  $arr2` |
|       - | 3305 | ` *   Second array` |
|       - | 3306 | ` * Return` |
|       - | 3307 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3308 | ` * Note` |
|       - | 3309 | ` *  This function is a symisc eXtension.` |
|       - | 3310 | ` */` |
|       4 | 3311 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3312 |  |
|       - | 3313 | `	ph7_hashmap *p1,*p2;` |
|       - | 3314 | `	int rc;` |
|       5 | 3315 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3316 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3317 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3318 | `		return PH7_OK;` |
|       - | 3319 | `	}` |
|       - | 3320 | `	/* Point to the hashmaps */` |
|       5 | 3321 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3322 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3323 | `	rc = (p1 == p2);` |
|       - | 3324 | `	/* Same instance? */` |
|       5 | 3325 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3326 | `	return PH7_OK;` |
|       3 | 3327 |  |
|       - | 3328 | `/*` |
|       - | 3329 | ` * array array_merge(array ...$arrays)` |
|       - | 3330 | ` *  Merge one or more arrays.` |
|       - | 3331 | ` * Parameters` |
|       - | 3332 | ` *  ...$arrays` |
|       - | 3333 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3334 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3335 | ` * Return` |
|       - | 3336 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3337 | ` *  with no arguments.` |
|       - | 3338 | ` */` |
|     884 | 3339 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3340 |  |
|       - | 3341 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3342 | `	ph7_value *pArray;` |
|       - | 3343 | `	int i;` |
|       - | 3344 | `	/* Create a new array */` |
|     886 | 3345 | `	pArray = ph7_context_new_array(pCtx);` |
|     886 | 3346 | `	if( pArray == 0 ){` |
|     ! 0 | 3347 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3348 | `		return PH7_OK;` |
|       - | 3349 | `	}` |
|       - | 3350 | `	/* Point to the internal representation of the hashmap */` |
|     886 | 3351 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3352 | `	/* Start merging */` |
|    2644 | 3353 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3354 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1764 | 3355 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3356 | `			/* Type mismatch -> TypeError */` |
|       7 | 3357 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3358 | `				"TypeError",` |
|       - | 3359 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3360 | `				i + 1,` |
|       4 | 3361 | `				ph7_type_name(apArg[i])` |
|       - | 3362 | `				);` |
|     ! 0 | 3363 | `		}else{` |
|    1760 | 3364 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3365 | `			/* Merge the two hashmaps */` |
|    1760 | 3366 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3367 | `		}` |
|     881 | 3368 | `	}` |
|       - | 3369 | `	/* Return the freshly created array */` |
|     882 | 3370 | `	ph7_result_value(pCtx,pArray);` |
|     882 | 3371 | `	return PH7_OK;` |
|     444 | 3372 |  |
|       - | 3373 | `/*` |
|       - | 3374 | ` * array array_copy(array $source)` |
|       - | 3375 | ` *  Make a blind copy of the target array.` |
|       - | 3376 | ` * Parameters` |
|       - | 3377 | ` *  $source` |
|       - | 3378 | ` *   Target array` |
|       - | 3379 | ` * Return` |
|       - | 3380 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3381 | ` * Note` |
|       - | 3382 | ` *  This function is a symisc eXtension.` |
|       - | 3383 | ` */` |
|      16 | 3384 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3385 |  |
|       - | 3386 | `	ph7_hashmap *pMap;` |
|       - | 3387 | `	ph7_value *pArray;` |
|      17 | 3388 | `	if( nArg < 1 ){` |
|       - | 3389 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3390 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3391 | `		return PH7_OK;` |
|       - | 3392 | `	}` |
|       - | 3393 | `	/* Create a new array */` |
|      17 | 3394 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3395 | `	if( pArray == 0 ){` |
|     ! 0 | 3396 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3397 | `		return PH7_OK;` |
|       - | 3398 | `	}` |
|       - | 3399 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3400 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3401 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3402 | `		/* Point to the internal representation of the source */` |
|      17 | 3403 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3404 | `		/* Perform the copy */` |
|      17 | 3405 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3406 | `	}else{` |
|       - | 3407 | `		/* Simple insertion */` |
|     ! 0 | 3408 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3409 | `	}` |
|       - | 3410 | `	/* Return the duplicated array */` |
|      17 | 3411 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3412 | `	return PH7_OK;` |
|       9 | 3413 |  |
|       - | 3414 | `/*` |
|       - | 3415 | ` * bool array_erase(array $source)` |
|       - | 3416 | ` *  Remove all elements from a given array.` |
|       - | 3417 | ` * Parameters` |
|       - | 3418 | ` *  $source` |
|       - | 3419 | ` *   Target array` |
|       - | 3420 | ` * Return` |
|       - | 3421 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3422 | ` * Note` |
|       - | 3423 | ` *  This function is a symisc eXtension.` |
|       - | 3424 | ` */` |
|      16 | 3425 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3426 |  |
|       - | 3427 | `	ph7_hashmap *pMap;` |
|      17 | 3428 | `	if( nArg < 1 ){` |
|       - | 3429 | `		/* Missing arguments */` |
|     ! 0 | 3430 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3431 | `		return PH7_OK;` |
|       - | 3432 | `	}` |
|       - | 3433 | `	/* Point to the target hashmap */` |
|      17 | 3434 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3435 | `	/* Erase */` |
|      17 | 3436 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3437 | `	return PH7_OK;` |
|       9 | 3438 |  |
|       - | 3439 | `/*` |
|       - | 3440 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3441 | ` *  Extract a slice of the array.` |
|       - | 3442 | ` * Parameters` |
|       - | 3443 | ` *  $array` |
|       - | 3444 | ` *    The input array.` |
|       - | 3445 | ` * $offset` |
|       - | 3446 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3447 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3448 | ` * $length (optional, nullable)` |
|       - | 3449 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3450 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3451 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3452 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3453 | ` * $preserve_keys (optional)` |
|       - | 3454 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3455 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3456 | ` * Return` |
|       - | 3457 | ` *   The new slice.` |
|       - | 3458 | ` */` |
|      46 | 3459 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3460 |  |
|       - | 3461 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3462 | `	ph7_hashmap_node *pCur;` |
|       - | 3463 | `	ph7_value *pArray;` |
|       - | 3464 | `	int iLength,iOfft;` |
|       - | 3465 | `	int bPreserve;` |
|       - | 3466 | `	sxi32 rc;` |
|      48 | 3467 | `	if( nArg < 2 ){` |
|       7 | 3468 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3469 | `			"ArgumentCountError",` |
|       - | 3470 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3471 | `			nArg` |
|       - | 3472 | `			);` |
|       - | 3473 | `	}` |
|      44 | 3474 | `	if( nArg > 4 ){` |
|       4 | 3475 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3476 | `			"ArgumentCountError",` |
|       - | 3477 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3478 | `			nArg` |
|       - | 3479 | `			);` |
|       - | 3480 | `	}` |
|      42 | 3481 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3482 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3483 | `			"TypeError",` |
|       - | 3484 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3485 | `			ph7_type_name(apArg[0])` |
|       - | 3486 | `			);` |
|       - | 3487 | `	}` |
|       - | 3488 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3489 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3490 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3491 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3492 | `			"TypeError",` |
|       - | 3493 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3494 | `			ph7_type_name(apArg[1])` |
|       - | 3495 | `			);` |
|       - | 3496 | `	}` |
|       - | 3497 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3498 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3499 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3500 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3501 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3502 | `				"TypeError",` |
|       - | 3503 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3504 | `				ph7_type_name(apArg[2])` |
|       - | 3505 | `				);` |
|       - | 3506 | `		}` |
|       8 | 3507 | `	}` |
|       - | 3508 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3509 | `	if( nArg > 3 ){` |
|      10 | 3510 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3511 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3512 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3513 | `				"TypeError",` |
|       - | 3514 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3515 | `				ph7_type_name(apArg[3])` |
|       - | 3516 | `				);` |
|       - | 3517 | `		}` |
|       2 | 3518 | `	}` |
|       - | 3519 | `	/* Point the internal representation of the target array */` |
|      33 | 3520 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3521 | `	bPreserve = FALSE;` |
|       - | 3522 | `	/* Get the offset */` |
|      33 | 3523 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3524 | `	if( iOfft < 0 ){` |
|       5 | 3525 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3526 | `		if( iOfft < 0 ){` |
|       3 | 3527 | `			iOfft = 0;` |
|       1 | 3528 | `		}` |
|       2 | 3529 | `	}` |
|      33 | 3530 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3531 | `		/* Offset past end of array, return empty array */` |
|       5 | 3532 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3533 | `		if( pArray == 0 ){` |
|     ! 0 | 3534 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3535 | `			return PH7_OK;` |
|       - | 3536 | `		}` |
|       5 | 3537 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3538 | `		return PH7_OK;` |
|       - | 3539 | `	}` |
|       - | 3540 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3541 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3542 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3543 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3544 | `		if( iLength < 0 ){` |
|       5 | 3545 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3546 | `		}` |
|      15 | 3547 | `		if( iLength < 0 ){` |
|       3 | 3548 | `			iLength = 0;` |
|       1 | 3549 | `		}` |
|      15 | 3550 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3551 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3552 | `		}` |
|       7 | 3553 | `	}` |
|      29 | 3554 | `	if( nArg > 3 ){` |
|       5 | 3555 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3556 | `	}` |
|       - | 3557 | `	/* Create a new array */` |
|      29 | 3558 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3559 | `	if( pArray == 0 ){` |
|     ! 0 | 3560 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3561 | `		return PH7_OK;` |
|       - | 3562 | `	}` |
|      29 | 3563 | `	if( iLength < 1 ){` |
|       - | 3564 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3565 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3566 | `		return PH7_OK;` |
|       - | 3567 | `	}` |
|       - | 3568 | `	/* Point to the desired entry */` |
|      25 | 3569 | `	pCur = pSrc->pFirst;` |
|      24 | 3570 | `	for(;;){` |
|      49 | 3571 | `		if( iOfft < 1 ){` |
|      25 | 3572 | `			break;` |
|       - | 3573 | `		}` |
|       - | 3574 | `		/* Point to the next entry */` |
|      25 | 3575 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3576 | `		iOfft--;` |
|       1 | 3577 | `	}` |
|       - | 3578 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3579 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3580 | `	for(;;){` |
|      79 | 3581 | `		if( iLength < 1 ){` |
|      25 | 3582 | `			break;` |
|       - | 3583 | `		}` |
|       - | 3584 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3585 | `		{` |
|      55 | 3586 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3587 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3588 | `		}` |
|      55 | 3589 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3590 | `			break;` |
|       - | 3591 | `		}` |
|       - | 3592 | `		/* Point to the next entry */` |
|      55 | 3593 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3594 | `		iLength--;` |
|       1 | 3595 | `	}` |
|       - | 3596 | `	/* Return the freshly created array */` |
|      25 | 3597 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3598 | `	return PH7_OK;` |
|      25 | 3599 |  |
|       - | 3600 | `/*` |
|       - | 3601 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3602 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3603 | ` * beginning (becomes the new pFirst).` |
|       - | 3604 | ` */` |
|      30 | 3605 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3606 |  |
|       - | 3607 | `	ph7_hashmap_node *pNode;` |
|       - | 3608 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3609 | `	pNode = pMap->pLast;` |
|      31 | 3610 | `	if( pNode == 0 ){` |
|     ! 0 | 3611 | `		return;` |
|       - | 3612 | `	}` |
|      31 | 3613 | `	if( pNode->pNext == 0 ){` |
|       - | 3614 | `		/* Only node in the list, nothing to move */` |
|       5 | 3615 | `		return;` |
|       - | 3616 | `	}` |
|      27 | 3617 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3618 | `		/* Already in the correct position */` |
|       9 | 3619 | `		return;` |
|       - | 3620 | `	}` |
|       - | 3621 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3622 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3623 | `	pMap->pLast->pPrev = 0;` |
|       - | 3624 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3625 | `	if( pAfter == 0 ){` |
|       - | 3626 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3627 | `		pNode->pNext = 0;` |
|       3 | 3628 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3629 | `		if( pMap->pFirst ){` |
|       3 | 3630 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3631 | `		}` |
|       3 | 3632 | `		pMap->pFirst = pNode;` |
|       2 | 3633 | `	}else{` |
|      17 | 3634 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3635 | `		pNode->pPrev = pOldNext;` |
|      17 | 3636 | `		pNode->pNext = pAfter;` |
|      17 | 3637 | `		pAfter->pPrev = pNode;` |
|      17 | 3638 | `		if( pOldNext ){` |
|      17 | 3639 | `			pOldNext->pNext = pNode;` |
|       9 | 3640 | `		}else{` |
|     ! 0 | 3641 | `			pMap->pLast = pNode;` |
|       - | 3642 | `		}` |
|       - | 3643 | `	}` |
|      16 | 3644 |  |
|       - | 3645 | `/*` |
|       - | 3646 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3647 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3648 | ` * Parameters` |
|       - | 3649 | ` *  $array` |
|       - | 3650 | ` *    The input array.` |
|       - | 3651 | ` *  $offset` |
|       - | 3652 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3653 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3654 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3655 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3656 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3657 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3658 | ` *  $length (optional)` |
|       - | 3659 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3660 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3661 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3662 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3663 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3664 | ` *  $replacement (optional)` |
|       - | 3665 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3666 | ` *    with elements from this array.` |
|       - | 3667 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3668 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3669 | ` *    offset.` |
|       - | 3670 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3671 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3672 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3673 | ` * Return` |
|       - | 3674 | ` *   A new array consisting of the extracted elements.` |
|       - | 3675 | ` */` |
|      54 | 3676 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3677 |  |
|       - | 3678 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3679 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3680 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3681 | `	int iLength,iOfft,i;` |
|       - | 3682 | `	sxi32 rc;` |
|      56 | 3683 | `	if( nArg < 2 ){` |
|       7 | 3684 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3685 | `			"ArgumentCountError",` |
|       - | 3686 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3687 | `			nArg` |
|       - | 3688 | `			);` |
|       - | 3689 | `	}` |
|      52 | 3690 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3691 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3692 | `			"TypeError",` |
|       - | 3693 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3694 | `			ph7_type_name(apArg[0])` |
|       - | 3695 | `			);` |
|       - | 3696 | `	}` |
|       - | 3697 | `	/* Point to the internal representation of the target array */` |
|      49 | 3698 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3699 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3700 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3701 | `	if( iOfft < 0 ){` |
|       7 | 3702 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3703 | `		if( iOfft < 0 ){` |
|       3 | 3704 | `			iOfft = 0;` |
|       2 | 3705 | `		}` |
|      46 | 3706 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3707 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3708 | `	}` |
|       - | 3709 | `	/* Get the length and clamp to valid range.` |
|       - | 3710 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3711 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3712 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3713 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3714 | `		if( iLength < 0 ){` |
|       7 | 3715 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3716 | `			if( iLength < 0 ){` |
|       3 | 3717 | `				iLength = 0;` |
|       1 | 3718 | `			}` |
|       3 | 3719 | `		}` |
|      31 | 3720 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3721 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3722 | `		}` |
|      15 | 3723 | `	}` |
|       - | 3724 | `	/* Create the result array for removed elements */` |
|      49 | 3725 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3726 | `	if( pArray == 0 ){` |
|     ! 0 | 3727 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3728 | `		return PH7_OK;` |
|       - | 3729 | `	}` |
|       - | 3730 | `	/* Get replacement array if provided */` |
|      49 | 3731 | `	pRep = 0;` |
|      49 | 3732 | `	if( nArg > 3 ){` |
|      21 | 3733 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3734 | `			/* Perform an array cast */` |
|       3 | 3735 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3736 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3737 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3738 | `			}` |
|       2 | 3739 | `		}else{` |
|      19 | 3740 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3741 | `		}` |
|      21 | 3742 | `		if( pRep ){` |
|       - | 3743 | `			/* Reset the loop cursor */` |
|      21 | 3744 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3745 | `		}` |
|      10 | 3746 | `	}` |
|       - | 3747 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3748 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3749 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3750 | `		return PH7_OK;` |
|       - | 3751 | `	}` |
|       - | 3752 | `	/* Navigate to the offset position */` |
|      41 | 3753 | `	pCur = pSrc->pFirst;` |
|      85 | 3754 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3755 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3756 | `	}` |
|       - | 3757 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3758 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3759 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3760 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3761 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3762 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3763 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3764 | `		pPrev = pCur->pPrev;` |
|      71 | 3765 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3766 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3767 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3768 | `			break;` |
|       - | 3769 | `		}` |
|      71 | 3770 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3771 | `	}` |
|       - | 3772 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3773 | `	if( pRep ){` |
|       - | 3774 | `		ph7_value sSafeVal;` |
|      61 | 3775 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3776 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3777 | `			if( pRvalue ){` |
|       - | 3778 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3779 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3780 | `				 * since it points into that same pool. */` |
|      31 | 3781 | `				sSafeVal = *pRvalue;` |
|      31 | 3782 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 3783 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 3784 | `					pNewNode = pSrc->pLast;` |
|      31 | 3785 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 3786 | `					pInsertAfter = pNewNode;` |
|      15 | 3787 | `				}` |
|      15 | 3788 | `			}` |
|       1 | 3789 | `		}` |
|      10 | 3790 | `	}` |
|       - | 3791 | `	/* Return the freshly created array */` |
|      41 | 3792 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 3793 | `	return PH7_OK;` |
|      29 | 3794 |  |
|       - | 3795 | `/*` |
|       - | 3796 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3797 | ` *  Checks if a value exists in an array.` |
|       - | 3798 | ` * Parameters` |
|       - | 3799 | ` *  $needle` |
|       - | 3800 | ` *   The searched value.` |
|       - | 3801 | ` *   Note:` |
|       - | 3802 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3803 | ` * $haystack` |
|       - | 3804 | ` *  The target array.` |
|       - | 3805 | ` * $strict` |
|       - | 3806 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3807 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3808 | ` */` |
|   21732 | 3809 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3810 |  |
|       - | 3811 | `	ph7_value *pNeedle;` |
|       - | 3812 | `	int bStrict;` |
|       - | 3813 | `	int rc;` |
|   21734 | 3814 | `	if( nArg < 2 ){` |
|       - | 3815 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3816 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3817 | `		return PH7_OK;` |
|       - | 3818 | `	}` |
|   21734 | 3819 | `	pNeedle = apArg[0];` |
|   21734 | 3820 | `	bStrict = 0;` |
|   21734 | 3821 | `	if( nArg > 2 ){` |
|       5 | 3822 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3823 | `	}` |
|   21734 | 3824 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3825 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3826 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3827 | `		/* Set the comparison result */` |
|     ! 0 | 3828 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3829 | `		return PH7_OK;` |
|       - | 3830 | `	}` |
|       - | 3831 | `	/* Perform the lookup */` |
|   21734 | 3832 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3833 | `	/* Lookup result */` |
|   21734 | 3834 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   21734 | 3835 | `	return PH7_OK;` |
|   10868 | 3836 |  |
|       - | 3837 | `/*` |
|       - | 3838 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3839 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3840 | ` * Parameters` |
|       - | 3841 | ` * $needle` |
|       - | 3842 | ` *   The searched value.` |
|       - | 3843 | ` * $haystack` |
|       - | 3844 | ` *   The array.` |
|       - | 3845 | ` * $strict` |
|       - | 3846 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3847 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3848 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3849 | ` * Return` |
|       - | 3850 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3851 | ` */` |
|      28 | 3852 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3853 |  |
|       - | 3854 | `	ph7_hashmap_node *pEntry;` |
|       - | 3855 | `	ph7_value *pVal,sNeedle;` |
|       - | 3856 | `	ph7_hashmap *pMap;` |
|       - | 3857 | `	ph7_value sVal;` |
|       - | 3858 | `	int bStrict;` |
|       - | 3859 | `	sxu32 n;` |
|       - | 3860 | `	int rc;` |
|      30 | 3861 | `	if( nArg < 2 ){` |
|       - | 3862 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3863 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3864 | `			"ArgumentCountError",` |
|       - | 3865 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 3866 | `			nArg` |
|       - | 3867 | `			);` |
|       - | 3868 | `	}` |
|      26 | 3869 | `	bStrict = FALSE;` |
|      26 | 3870 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3871 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3872 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3873 | `			"TypeError",` |
|       - | 3874 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 3875 | `			ph7_type_name(apArg[1])` |
|       - | 3876 | `			);` |
|       - | 3877 | `	}` |
|      24 | 3878 | `	if( nArg > 2 ){` |
|       - | 3879 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 3880 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3881 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3882 | `				"TypeError",` |
|       - | 3883 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3884 | `				ph7_type_name(apArg[2])` |
|       - | 3885 | `				);` |
|       - | 3886 | `		}` |
|       9 | 3887 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 3888 | `	}` |
|       - | 3889 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 3890 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3891 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 3892 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 3893 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 3894 | `	pEntry = pMap->pFirst;` |
|      21 | 3895 | `	n = pMap->nEntry;` |
|      23 | 3896 | `	for(;;){` |
|      47 | 3897 | `		if( !n ){` |
|       9 | 3898 | `			break;` |
|       - | 3899 | `		}` |
|       - | 3900 | `		/* Extract node value */` |
|      39 | 3901 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 3902 | `		if( pVal ){` |
|       - | 3903 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3904 | `			 * can change their type.` |
|       - | 3905 | `			 */` |
|      39 | 3906 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 3907 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 3908 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 3909 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 3910 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 3911 | `			if( rc == 0 ){` |
|       - | 3912 | `				/* Match found,return key */` |
|      13 | 3913 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3914 | `					/* INT key */` |
|       7 | 3915 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 3916 | `				}else{` |
|       7 | 3917 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3918 | `					/* Blob key */` |
|       7 | 3919 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3920 | `				}` |
|      13 | 3921 | `				return PH7_OK;` |
|       - | 3922 | `			}` |
|      13 | 3923 | `		}` |
|       - | 3924 | `		/* Point to the next entry */` |
|      27 | 3925 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 3926 | `		n--;` |
|       1 | 3927 | `	}` |
|       - | 3928 | `	/* No such value,return FALSE */` |
|       9 | 3929 | `	ph7_result_bool(pCtx,0);` |
|       9 | 3930 | `	return PH7_OK;` |
|      16 | 3931 |  |
|       - | 3932 | `/*` |
|       - | 3933 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3934 | ` *  Computes the difference of arrays.` |
|       - | 3935 | ` * Parameters` |
|       - | 3936 | ` *  $array1` |
|       - | 3937 | ` *    The array to compare from` |
|       - | 3938 | ` *  $array2` |
|       - | 3939 | ` *    An array to compare against` |
|       - | 3940 | ` *  $...` |
|       - | 3941 | ` *   More arrays to compare against` |
|       - | 3942 | ` * Return` |
|       - | 3943 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3944 | ` *  are not present in any of the other arrays.` |
|       - | 3945 | ` */` |
|      22 | 3946 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3947 |  |
|       - | 3948 | `	ph7_hashmap_node *pEntry;` |
|       - | 3949 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3950 | `	ph7_value *pArray;` |
|       - | 3951 | `	ph7_value *pVal;` |
|       - | 3952 | `	sxi32 rc;` |
|       - | 3953 | `	sxu32 n;` |
|       - | 3954 | `	int i;` |
|       - | 3955 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3956 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3957 | `	 * debugging difficult. */` |
|      24 | 3958 | `	if( nArg < 1 ){` |
|       4 | 3959 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3960 | `			"ArgumentCountError",` |
|       - | 3961 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3962 | `			nArg` |
|       - | 3963 | `			);` |
|       - | 3964 | `	}` |
|      22 | 3965 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3966 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3967 | `			"TypeError",` |
|       - | 3968 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3969 | `			ph7_type_name(apArg[0])` |
|       - | 3970 | `			);` |
|       - | 3971 | `	}` |
|      36 | 3972 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3973 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3974 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3975 | `				"TypeError",` |
|       - | 3976 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3977 | `				i + 1,` |
|       2 | 3978 | `				ph7_type_name(apArg[i])` |
|       - | 3979 | `				);` |
|       - | 3980 | `		}` |
|       9 | 3981 | `	}` |
|      17 | 3982 | `	if( nArg == 1 ){` |
|       - | 3983 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3984 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3985 | `		return PH7_OK;` |
|       - | 3986 | `	}` |
|       - | 3987 | `	/* Create a new array */` |
|      15 | 3988 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3989 | `	if( pArray == 0 ){` |
|     ! 0 | 3990 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3991 | `		return PH7_OK;` |
|       - | 3992 | `	}` |
|       - | 3993 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 3994 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3995 | `	/* Perform the diff */` |
|      15 | 3996 | `	pEntry = pSrc->pFirst;` |
|      15 | 3997 | `	n = pSrc->nEntry;` |
|      27 | 3998 | `	for(;;){` |
|      55 | 3999 | `		if( n < 1 ){` |
|      15 | 4000 | `			break;` |
|       - | 4001 | `		}` |
|       - | 4002 | `		/* Extract the node value */` |
|      41 | 4003 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4004 | `		if( pVal ){` |
|      69 | 4005 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4006 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4007 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4008 | `				/* Perform the lookup */` |
|      45 | 4009 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4010 | `				if( rc == SXRET_OK ){` |
|       - | 4011 | `					/* Value exist */` |
|      17 | 4012 | `					break;` |
|       - | 4013 | `				}` |
|      15 | 4014 | `			}` |
|      41 | 4015 | `			if( i >= nArg ){` |
|       - | 4016 | `				/* Perform the insertion */` |
|      25 | 4017 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4018 | `			}` |
|      20 | 4019 | `		}` |
|       - | 4020 | `		/* Point to the next entry */` |
|      41 | 4021 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4022 | `		n--;` |
|       1 | 4023 | `	}` |
|       - | 4024 | `	/* Return the freshly created array */` |
|      15 | 4025 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4026 | `	return PH7_OK;` |
|      13 | 4027 |  |
|       - | 4028 | `/*` |
|       - | 4029 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4030 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4031 | ` * Parameters` |
|       - | 4032 | ` *  $array1` |
|       - | 4033 | ` *    The array to compare from` |
|       - | 4034 | ` *  $array2` |
|       - | 4035 | ` *    An array to compare against` |
|       - | 4036 | ` *  $...` |
|       - | 4037 | ` *   More arrays to compare against.` |
|       - | 4038 | ` * $callback` |
|       - | 4039 | ` *  The callback comparison function.` |
|       - | 4040 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4041 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4042 | ` *  than the second.` |
|       - | 4043 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4044 | ` * Return` |
|       - | 4045 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4046 | ` *  are not present in any of the other arrays.` |
|       - | 4047 | ` */` |
|      20 | 4048 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4049 |  |
|       - | 4050 | `	ph7_hashmap_node *pEntry;` |
|       - | 4051 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4052 | `	ph7_value *pCallback;` |
|       - | 4053 | `	ph7_value *pArray;` |
|       - | 4054 | `	ph7_value *pVal;` |
|       - | 4055 | `	sxi32 rc;` |
|       - | 4056 | `	sxu32 n;` |
|       - | 4057 | `	int i;` |
|       - | 4058 |  |
|       - | 4059 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      22 | 4060 | `	if( nArg < 2 ){` |
|       4 | 4061 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4062 | `			"ArgumentCountError",` |
|       - | 4063 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4064 | `			nArg` |
|       - | 4065 | `			);` |
|       - | 4066 | `	}` |
|      20 | 4067 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4068 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4069 | `			"TypeError",` |
|       - | 4070 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4071 | `			ph7_type_name(apArg[0])` |
|       - | 4072 | `			);` |
|       - | 4073 | `	}` |
|       - | 4074 |  |
|      18 | 4075 | `	if( nArg == 2 ){` |
|       - | 4076 | `		/* Only the original array and the callback were provided. */` |
|       - | 4077 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4078 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4079 | `		 * validation order.` |
|       - | 4080 | `		 */` |
|       4 | 4081 | `	} else {` |
|       - | 4082 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      20 | 4083 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      14 | 4084 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      10 | 4085 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4086 | `					"TypeError",` |
|       - | 4087 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4088 | `					i + 1,` |
|       6 | 4089 | `					ph7_type_name(apArg[i])` |
|       - | 4090 | `					);` |
|       - | 4091 | `			}` |
|       5 | 4092 | `		}` |
|       - | 4093 | `	}` |
|       - | 4094 |  |
|       - | 4095 | `	/* Identify the callback (always expected as the last argument). */` |
|      12 | 4096 | `	pCallback = apArg[nArg - 1];` |
|       - | 4097 | `	/* Validate the callback to match PHP's error messages. */` |
|      12 | 4098 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       7 | 4099 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4100 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4101 | `				"TypeError",` |
|       - | 4102 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4103 | `				nArg` |
|       - | 4104 | `				);` |
|       - | 4105 | `		}` |
|       5 | 4106 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4107 | `			int len;` |
|       3 | 4108 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4109 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4110 | `				"TypeError",` |
|       - | 4111 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4112 | `				nArg,` |
|       1 | 4113 | `				zName` |
|       - | 4114 | `				);` |
|       - | 4115 | `		}` |
|       4 | 4116 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4117 | `			"TypeError",` |
|       - | 4118 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4119 | `			nArg` |
|       - | 4120 | `			);` |
|       - | 4121 | `	}` |
|       - | 4122 |  |
|       5 | 4123 | `	if( nArg == 2 ){` |
|       - | 4124 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4125 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4126 | `		return PH7_OK;` |
|       - | 4127 | `	}` |
|       - | 4128 |  |
|       - | 4129 | `	/* Create a new array */` |
|       3 | 4130 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4131 | `	if( pArray == 0 ){` |
|     ! 0 | 4132 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4133 | `		return PH7_OK;` |
|       - | 4134 | `	}` |
|       - | 4135 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4136 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4137 | `	/* Perform the diff */` |
|       3 | 4138 | `	pEntry = pSrc->pFirst;` |
|       3 | 4139 | `	n = pSrc->nEntry;` |
|       4 | 4140 | `	for(;;){` |
|       9 | 4141 | `		if( n < 1 ){` |
|       3 | 4142 | `			break;` |
|       - | 4143 | `		}` |
|       - | 4144 | `		/* Extract the node value */` |
|       7 | 4145 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4146 | `		if( pVal ){` |
|      11 | 4147 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4148 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4149 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4150 | `				/* Perform the lookup */` |
|       7 | 4151 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4152 | `				if( rc == SXRET_OK ){` |
|       - | 4153 | `					/* Value exist */` |
|       3 | 4154 | `					break;` |
|       - | 4155 | `				}` |
|       3 | 4156 | `			}` |
|       7 | 4157 | `			if( i >= (nArg - 1)){` |
|       - | 4158 | `				/* Perform the insertion */` |
|       5 | 4159 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4160 | `			}` |
|       3 | 4161 | `		}` |
|       - | 4162 | `		/* Point to the next entry */` |
|       7 | 4163 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4164 | `		n--;` |
|       1 | 4165 | `	}` |
|       - | 4166 | `	/* Return the freshly created array */` |
|       3 | 4167 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4168 | `	return PH7_OK;` |
|      12 | 4169 |  |
|       - | 4170 | `/*` |
|       - | 4171 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4172 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4173 | ` * Parameters` |
|       - | 4174 | ` *  $array1` |
|       - | 4175 | ` *    The array to compare from` |
|       - | 4176 | ` *  $array2` |
|       - | 4177 | ` *    An array to compare against` |
|       - | 4178 | ` *  $...` |
|       - | 4179 | ` *   More arrays to compare against` |
|       - | 4180 | ` * Return` |
|       - | 4181 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4182 | ` *  are not present in any of the other arrays.` |
|       - | 4183 | ` */` |
|      20 | 4184 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4185 |  |
|       - | 4186 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4187 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4188 | `	ph7_value *pArray;` |
|       - | 4189 | `	ph7_value *pVal;` |
|       - | 4190 | `	sxi32 rc;` |
|       - | 4191 | `	sxu32 n;` |
|       - | 4192 | `	int i;` |
|       - | 4193 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4194 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4195 | `	 * accompanying integration tests to pass. */` |
|      22 | 4196 | `	if( nArg < 1 ){` |
|       4 | 4197 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4198 | `			"ArgumentCountError",` |
|       - | 4199 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4200 | `			nArg` |
|       - | 4201 | `			);` |
|       - | 4202 | `	}` |
|      20 | 4203 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4204 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4205 | `			"TypeError",` |
|       - | 4206 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4207 | `			ph7_type_name(apArg[0])` |
|       - | 4208 | `			);` |
|       - | 4209 | `	}` |
|      32 | 4210 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4211 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4212 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4213 | `				"TypeError",` |
|       - | 4214 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4215 | `				i + 1,` |
|       4 | 4216 | `				ph7_type_name(apArg[i])` |
|       - | 4217 | `				);` |
|       - | 4218 | `		}` |
|       9 | 4219 | `	}` |
|      13 | 4220 | `	if( nArg == 1 ){` |
|       - | 4221 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4222 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4223 | `		return PH7_OK;` |
|       - | 4224 | `	}` |
|       - | 4225 | `	/* Create a new array */` |
|      11 | 4226 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4227 | `	if( pArray == 0 ){` |
|     ! 0 | 4228 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4229 | `		return PH7_OK;` |
|       - | 4230 | `	}` |
|       - | 4231 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4232 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4233 | `	/* Perform the diff */` |
|      11 | 4234 | `	pEntry = pSrc->pFirst;` |
|      11 | 4235 | `	n = pSrc->nEntry;` |
|      11 | 4236 | `	pN1 = pN2 = 0;` |
|      29 | 4237 | `	for(;;){` |
|       - | 4238 | `		int keep;` |
|      35 | 4239 | `		if( n < 1 ){` |
|      11 | 4240 | `			break;` |
|       - | 4241 | `		}` |
|       - | 4242 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4243 | `		keep = 1;` |
|      41 | 4244 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4245 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4246 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4247 | `			/* Perform a key lookup first */` |
|      29 | 4248 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4249 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4250 | `			}else{` |
|      17 | 4251 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4252 | `			}` |
|      29 | 4253 | `			if( rc != SXRET_OK ){` |
|       - | 4254 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4255 | `				continue;` |
|       - | 4256 | `			}` |
|       - | 4257 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4258 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4259 | `			if( pVal ){` |
|       - | 4260 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4261 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4262 | `				if( pVal2 ){` |
|      15 | 4263 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4264 | `					if( cmp == 0 ){` |
|       - | 4265 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4266 | `						keep = 0;` |
|      13 | 4267 | `						break;` |
|       - | 4268 | `					}` |
|       1 | 4269 | `				}` |
|       1 | 4270 | `			}` |
|       2 | 4271 | `		}` |
|      25 | 4272 | `		if( keep ){` |
|       - | 4273 | `			/* Perform the insertion */` |
|      13 | 4274 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4275 | `		}` |
|       - | 4276 | `		/* Point to the next entry */` |
|      25 | 4277 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4278 | `		n--;` |
|       1 | 4279 | `	}` |
|       - | 4280 | `	/* Return the freshly created array */` |
|      11 | 4281 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4282 | `	return PH7_OK;` |
|      12 | 4283 |  |
|       - | 4284 | `/*` |
|       - | 4285 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4286 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4287 | ` *  by a user supplied callback function.` |
|       - | 4288 | ` * Parameters` |
|       - | 4289 | ` *  $array1` |
|       - | 4290 | ` *    The array to compare from` |
|       - | 4291 | ` *  $array2` |
|       - | 4292 | ` *    An array to compare against` |
|       - | 4293 | ` *  $...` |
|       - | 4294 | ` *   More arrays to compare against.` |
|       - | 4295 | ` *  $key_compare_func` |
|       - | 4296 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4297 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4298 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4299 | ` * Return` |
|       - | 4300 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4301 | ` *  are not present in any of the other arrays.` |
|       - | 4302 | ` */` |
|      22 | 4303 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4304 |  |
|       - | 4305 | `	ph7_hashmap_node *pEntry;` |
|       - | 4306 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4307 | `	ph7_value *pCallback;` |
|       - | 4308 | `	ph7_value *pArray;` |
|       - | 4309 | `	sxi32 rc;` |
|       - | 4310 | `	sxu32 n;` |
|       - | 4311 | `	int i;` |
|       - | 4312 |  |
|       - | 4313 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4314 | `	if( nArg < 2 ){` |
|       4 | 4315 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4316 | `			"ArgumentCountError",` |
|       - | 4317 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4318 | `			nArg` |
|       - | 4319 | `			);` |
|       - | 4320 | `	}` |
|      22 | 4321 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4322 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4323 | `			"TypeError",` |
|       - | 4324 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4325 | `			ph7_type_name(apArg[0])` |
|       - | 4326 | `			);` |
|       - | 4327 | `	}` |
|       - | 4328 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4329 | `	 * expected to be a callback. */` |
|      32 | 4330 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4331 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4332 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4333 | `				"TypeError",` |
|       - | 4334 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4335 | `				i + 1,` |
|       2 | 4336 | `				ph7_type_name(apArg[i])` |
|       - | 4337 | `				);` |
|       - | 4338 | `		}` |
|       8 | 4339 | `	}` |
|       - | 4340 | `	/* Point to the callback value */` |
|      18 | 4341 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4342 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4343 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4344 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4345 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4346 | `		 * string given" which we also reproduce. */` |
|       7 | 4347 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4348 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4349 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4350 | `				"TypeError",` |
|       - | 4351 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4352 | `				nArg` |
|       - | 4353 | `				);` |
|       - | 4354 | `		}` |
|       5 | 4355 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4356 | `			/* neither array nor string */` |
|       7 | 4357 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4358 | `				"TypeError",` |
|       - | 4359 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4360 | `				nArg` |
|       - | 4361 | `				);` |
|       - | 4362 | `		}` |
|       - | 4363 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4364 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4365 | `			"TypeError",` |
|       - | 4366 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4367 | `			nArg,` |
|     ! 0 | 4368 | `			ph7_type_name(pCallback)` |
|       - | 4369 | `			);` |
|       - | 4370 | `	}` |
|      11 | 4371 | `	if( nArg == 2 ){` |
|       - | 4372 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4373 | `		 * input array. */` |
|       3 | 4374 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4375 | `		return PH7_OK;` |
|       - | 4376 | `	}` |
|       - | 4377 | `	/* Create a new array */` |
|       9 | 4378 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4379 | `	if( pArray == 0 ){` |
|     ! 0 | 4380 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4381 | `		return PH7_OK;` |
|       - | 4382 | `	}` |
|       - | 4383 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4384 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4385 | `	/* Perform the diff */` |
|       9 | 4386 | `	pEntry = pSrc->pFirst;` |
|       9 | 4387 | `	n = pSrc->nEntry;` |
|      20 | 4388 | `	for(;;){` |
|       - | 4389 | `		int keep;` |
|      25 | 4390 | `		if( n < 1 ){` |
|       9 | 4391 | `			break;` |
|       - | 4392 | `		}` |
|      17 | 4393 | `		keep = 1;` |
|      29 | 4394 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4395 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4396 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4397 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4398 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4399 | `			while( pIt ){` |
|       - | 4400 | `				/* build temporary key values for callback */` |
|       - | 4401 | `				ph7_value key1, key2, result;` |
|       - | 4402 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4403 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4404 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4405 | `				}else{` |
|       - | 4406 | `					SyString sStr;` |
|      31 | 4407 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4408 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4409 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4410 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4411 | `				}` |
|      31 | 4412 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4413 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4414 | `				}else{` |
|       - | 4415 | `					SyString sStr;` |
|      31 | 4416 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4417 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4418 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4419 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4420 | `				}` |
|      31 | 4421 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4422 | `				/* call user callback with (key1, key2) */` |
|       - | 4423 | `				{` |
|       - | 4424 | `					ph7_value *apK[2];` |
|      31 | 4425 | `					apK[0] = &key1;` |
|      31 | 4426 | `					apK[1] = &key2;` |
|      31 | 4427 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4428 | `				}` |
|      31 | 4429 | `				if( rc == SXRET_OK ){` |
|      31 | 4430 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4431 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4432 | `					}` |
|      31 | 4433 | `					if( result.x.iVal == 0 ){` |
|       - | 4434 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4435 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4436 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4437 | `						if( pVal1 && pVal2 ){` |
|      13 | 4438 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4439 | `								keep = 0;` |
|       9 | 4440 | `								PH7_MemObjRelease(&result);` |
|       - | 4441 | `								/* release keys too before breaking */` |
|       9 | 4442 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4443 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4444 | `								break;` |
|       - | 4445 | `							}` |
|       2 | 4446 | `						}` |
|       2 | 4447 | `					}` |
|      11 | 4448 | `				}` |
|      23 | 4449 | `				PH7_MemObjRelease(&result);` |
|      23 | 4450 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4451 | `				PH7_MemObjRelease(&key2);` |
|       - | 4452 | `				/* move to next node */` |
|      23 | 4453 | `				pIt = pIt->pPrev;` |
|      23 | 4454 | `				if( keep == 0 ) break;` |
|       1 | 4455 | `			}` |
|      21 | 4456 | `			if( keep == 0 ) break;` |
|       7 | 4457 | `		}` |
|      17 | 4458 | `		if( keep ){` |
|       - | 4459 | `			/* Perform the insertion */` |
|       9 | 4460 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4461 | `		}` |
|       - | 4462 | `		/* Point to the next entry */` |
|      17 | 4463 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4464 | `		n--;` |
|       1 | 4465 | `	}` |
|       - | 4466 | `	/* Return the freshly created array */` |
|       9 | 4467 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4468 | `	return PH7_OK;` |
|      13 | 4469 |  |
|       - | 4470 | `/*` |
|       - | 4471 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4472 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4473 | ` * Parameters` |
|       - | 4474 | ` *  $array1` |
|       - | 4475 | ` *    The array to compare from` |
|       - | 4476 | ` *  $array2` |
|       - | 4477 | ` *    An array to compare against` |
|       - | 4478 | ` *  $...` |
|       - | 4479 | ` *   More arrays to compare against` |
|       - | 4480 | ` * Return` |
|       - | 4481 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4482 | ` *  in any of the other arrays.` |
|       - | 4483 | ` * Note that NULL is returned on failure.` |
|       - | 4484 | ` */` |
|      14 | 4485 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4486 |  |
|       - | 4487 | `	ph7_hashmap_node *pEntry;` |
|       - | 4488 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4489 | `	ph7_value *pArray;` |
|       - | 4490 | `	sxi32 rc;` |
|       - | 4491 | `	sxu32 n;` |
|       - | 4492 | `	int i;` |
|       - | 4493 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4494 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4495 | `	 * helpers. */` |
|      16 | 4496 | `	if( nArg < 1 ){` |
|       4 | 4497 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4498 | `			"ArgumentCountError",` |
|       - | 4499 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4500 | `			nArg` |
|       - | 4501 | `			);` |
|       - | 4502 | `	}` |
|      14 | 4503 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4504 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4505 | `			"TypeError",` |
|       - | 4506 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4507 | `			ph7_type_name(apArg[0])` |
|       - | 4508 | `			);` |
|       - | 4509 | `	}` |
|      20 | 4510 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4511 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4512 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4513 | `				"TypeError",` |
|       - | 4514 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4515 | `				i + 1,` |
|       2 | 4516 | `				ph7_type_name(apArg[i])` |
|       - | 4517 | `				);` |
|       - | 4518 | `		}` |
|       5 | 4519 | `	}` |
|       9 | 4520 | `	if( nArg == 1 ){` |
|       - | 4521 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4522 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4523 | `		return PH7_OK;` |
|       - | 4524 | `	}` |
|       - | 4525 | `	/* Create a new array */` |
|       7 | 4526 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4527 | `	if( pArray == 0 ){` |
|     ! 0 | 4528 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4529 | `		return PH7_OK;` |
|       - | 4530 | `	}` |
|       - | 4531 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4532 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4533 | `	/* Perfrom the diff */` |
|       7 | 4534 | `	pEntry = pSrc->pFirst;` |
|       7 | 4535 | `	n = pSrc->nEntry;` |
|      12 | 4536 | `	for(;;){` |
|      25 | 4537 | `		if( n < 1 ){` |
|       7 | 4538 | `			break;` |
|       - | 4539 | `		}` |
|      31 | 4540 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4541 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4542 | `				/* ignore */` |
|     ! 0 | 4543 | `				continue;` |
|       - | 4544 | `			}` |
|      23 | 4545 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4546 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4547 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4548 | `				/* Blob lookup */` |
|      17 | 4549 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4550 | `			}else{` |
|       - | 4551 | `				/* Int lookup */` |
|       7 | 4552 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4553 | `			}` |
|      23 | 4554 | `			if( rc == SXRET_OK ){` |
|       - | 4555 | `				/* Key exists,break immediately */` |
|      11 | 4556 | `				break;` |
|       - | 4557 | `			}` |
|       7 | 4558 | `		}` |
|      19 | 4559 | `		if( i >= nArg ){` |
|       - | 4560 | `			/* Perform the insertion */` |
|       9 | 4561 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4562 | `		}` |
|       - | 4563 | `		/* Point to the next entry */` |
|      19 | 4564 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4565 | `		n--;` |
|       1 | 4566 | `	}` |
|       - | 4567 | `	/* Return the freshly created array */` |
|       7 | 4568 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4569 | `	return PH7_OK;` |
|       9 | 4570 |  |
|       - | 4571 | `/*` |
|       - | 4572 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4573 | ` *  Computes the intersection of arrays.` |
|       - | 4574 | ` * Parameters` |
|       - | 4575 | ` *  $array1` |
|       - | 4576 | ` *    The array to compare from` |
|       - | 4577 | ` *  $array2` |
|       - | 4578 | ` *    An array to compare against` |
|       - | 4579 | ` *  $...` |
|       - | 4580 | ` *   More arrays to compare against` |
|       - | 4581 | ` * Return` |
|       - | 4582 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4583 | ` *  in all of the parameters.` |
|       - | 4584 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4585 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4586 | ` */` |
|      22 | 4587 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4588 |  |
|       - | 4589 | `	ph7_hashmap_node *pEntry;` |
|       - | 4590 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4591 | `	ph7_value *pArray;` |
|       - | 4592 | `	ph7_value *pVal;` |
|       - | 4593 | `	sxi32 rc;` |
|       - | 4594 | `	sxu32 n;` |
|       - | 4595 | `	int i;` |
|      24 | 4596 | `	if( nArg < 1 ){` |
|       4 | 4597 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4598 | `			"ArgumentCountError",` |
|       - | 4599 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4600 | `			nArg` |
|       - | 4601 | `			);` |
|       - | 4602 | `	}` |
|      22 | 4603 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4604 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4605 | `			"TypeError",` |
|       - | 4606 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4607 | `			ph7_type_name(apArg[0])` |
|       - | 4608 | `			);` |
|       - | 4609 | `	}` |
|      36 | 4610 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4611 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4612 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4613 | `				"TypeError",` |
|       - | 4614 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4615 | `				i + 1,` |
|       2 | 4616 | `				ph7_type_name(apArg[i])` |
|       - | 4617 | `				);` |
|       - | 4618 | `		}` |
|       9 | 4619 | `	}` |
|      17 | 4620 | `	if( nArg == 1 ){` |
|       - | 4621 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4622 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4623 | `		return PH7_OK;` |
|       - | 4624 | `	}` |
|       - | 4625 | `	/* Create a new array */` |
|      15 | 4626 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4627 | `	if( pArray == 0 ){` |
|     ! 0 | 4628 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4629 | `		return PH7_OK;` |
|       - | 4630 | `	}` |
|       - | 4631 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4632 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4633 | `	/* Perform the intersection */` |
|      15 | 4634 | `	pEntry = pSrc->pFirst;` |
|      15 | 4635 | `	n = pSrc->nEntry;` |
|      31 | 4636 | `	for(;;){` |
|      63 | 4637 | `		if( n < 1 ){` |
|      15 | 4638 | `			break;` |
|       - | 4639 | `		}` |
|       - | 4640 | `		/* Extract the node value */` |
|      49 | 4641 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4642 | `		if( pVal ){` |
|      79 | 4643 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4644 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4645 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4646 | `				/* Perform the lookup */` |
|      55 | 4647 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4648 | `				if( rc != SXRET_OK ){` |
|       - | 4649 | `					/* Value does not exist */` |
|      25 | 4650 | `					break;` |
|       - | 4651 | `				}` |
|      16 | 4652 | `			}` |
|      49 | 4653 | `			if( i >= nArg ){` |
|       - | 4654 | `				/* Perform the insertion */` |
|      25 | 4655 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4656 | `			}` |
|      24 | 4657 | `		}` |
|       - | 4658 | `		/* Point to the next entry */` |
|      49 | 4659 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4660 | `		n--;` |
|       1 | 4661 | `	}` |
|       - | 4662 | `	/* Return the freshly created array */` |
|      15 | 4663 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4664 | `	return PH7_OK;` |
|      13 | 4665 |  |
|       - | 4666 | `/*` |
|       - | 4667 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4668 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4669 | ` * Parameters` |
|       - | 4670 | ` *  $array1` |
|       - | 4671 | ` *    The array to compare from` |
|       - | 4672 | ` *  $array2` |
|       - | 4673 | ` *    An array to compare against` |
|       - | 4674 | ` *  $...` |
|       - | 4675 | ` *   More arrays to compare against` |
|       - | 4676 | ` * Return` |
|       - | 4677 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4678 | ` *  in all the arguments, with matching keys.` |
|       - | 4679 | ` */` |
|      22 | 4680 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4681 |  |
|       - | 4682 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4683 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4684 | `	ph7_value *pArray;` |
|       - | 4685 | `	ph7_value *pVal;` |
|       - | 4686 | `	sxi32 rc;` |
|       - | 4687 | `	sxu32 n;` |
|       - | 4688 | `	int i;` |
|      24 | 4689 | `	if( nArg < 1 ){` |
|       4 | 4690 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4691 | `			"ArgumentCountError",` |
|       - | 4692 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4693 | `			nArg` |
|       - | 4694 | `			);` |
|       - | 4695 | `	}` |
|      22 | 4696 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4697 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4698 | `			"TypeError",` |
|       - | 4699 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4700 | `			ph7_type_name(apArg[0])` |
|       - | 4701 | `			);` |
|       - | 4702 | `	}` |
|      36 | 4703 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4704 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4705 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4706 | `				"TypeError",` |
|       - | 4707 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4708 | `				i + 1,` |
|       2 | 4709 | `				ph7_type_name(apArg[i])` |
|       - | 4710 | `				);` |
|       - | 4711 | `		}` |
|       9 | 4712 | `	}` |
|      17 | 4713 | `	if( nArg == 1 ){` |
|       - | 4714 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4715 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4716 | `		return PH7_OK;` |
|       - | 4717 | `	}` |
|       - | 4718 | `	/* Create a new array */` |
|      15 | 4719 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4720 | `	if( pArray == 0 ){` |
|     ! 0 | 4721 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4722 | `		return PH7_OK;` |
|       - | 4723 | `	}` |
|       - | 4724 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4725 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4726 | `	/* Perform the intersection */` |
|      15 | 4727 | `	pEntry = pSrc->pFirst;` |
|      15 | 4728 | `	n = pSrc->nEntry;` |
|      15 | 4729 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4730 | `	for(;;){` |
|      47 | 4731 | `		if( n < 1 ){` |
|      15 | 4732 | `			break;` |
|       - | 4733 | `		}` |
|       - | 4734 | `		/* Extract the node value */` |
|      33 | 4735 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4736 | `		if( pVal ){` |
|      53 | 4737 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4738 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4739 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4740 | `				/* Perform a key lookup first */` |
|      37 | 4741 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4742 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4743 | `				}else{` |
|      23 | 4744 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4745 | `				}` |
|      37 | 4746 | `				if( rc != SXRET_OK ){` |
|       - | 4747 | `					/* No such key,break immediately */` |
|       7 | 4748 | `					break;` |
|       - | 4749 | `				}` |
|       - | 4750 | `				/* Perform the lookup */` |
|      31 | 4751 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4752 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4753 | `					/* Value does not exist */` |
|       6 | 4754 | `					break;` |
|       - | 4755 | `				}` |
|      11 | 4756 | `			}` |
|      33 | 4757 | `			if( i >= nArg ){` |
|       - | 4758 | `				/* Perform the insertion */` |
|      17 | 4759 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4760 | `			}` |
|      16 | 4761 | `		}` |
|       - | 4762 | `		/* Point to the next entry */` |
|      33 | 4763 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4764 | `		n--;` |
|       1 | 4765 | `	}` |
|       - | 4766 | `	/* Return the freshly created array */` |
|      15 | 4767 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4768 | `	return PH7_OK;` |
|      13 | 4769 |  |
|       - | 4770 | `/*` |
|       - | 4771 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 4772 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4773 | ` * Parameters` |
|       - | 4774 | ` *  $array1` |
|       - | 4775 | ` *    The array to compare from` |
|       - | 4776 | ` *  $...` |
|       - | 4777 | ` *   More arrays to compare against` |
|       - | 4778 | ` * Return` |
|       - | 4779 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4780 | ` *  have keys that are present in all arguments.` |
|       - | 4781 | ` * Note that NULL is returned on failure.` |
|       - | 4782 | ` */` |
|      22 | 4783 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4784 |  |
|       - | 4785 | `	ph7_hashmap_node *pEntry;` |
|       - | 4786 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4787 | `	ph7_value *pArray;` |
|       - | 4788 | `	sxi32 rc;` |
|       - | 4789 | `	sxu32 n;` |
|       - | 4790 | `	int i;` |
|      24 | 4791 | `	if( nArg < 1 ){` |
|       4 | 4792 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4793 | `			"ArgumentCountError",` |
|       - | 4794 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 4795 | `			nArg` |
|       - | 4796 | `			);` |
|       - | 4797 | `	}` |
|      22 | 4798 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4799 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4800 | `			"TypeError",` |
|       - | 4801 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4802 | `			ph7_type_name(apArg[0])` |
|       - | 4803 | `			);` |
|       - | 4804 | `	}` |
|      36 | 4805 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4806 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4807 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4808 | `				"TypeError",` |
|       - | 4809 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4810 | `				i + 1,` |
|       2 | 4811 | `				ph7_type_name(apArg[i])` |
|       - | 4812 | `				);` |
|       - | 4813 | `		}` |
|       9 | 4814 | `	}` |
|      17 | 4815 | `	if( nArg == 1 ){` |
|       - | 4816 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4817 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4818 | `		return PH7_OK;` |
|       - | 4819 | `	}` |
|       - | 4820 | `	/* Create a new array */` |
|      15 | 4821 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4822 | `	if( pArray == 0 ){` |
|     ! 0 | 4823 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4824 | `		return PH7_OK;` |
|       - | 4825 | `	}` |
|       - | 4826 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 4827 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4828 | `	/* Perform the intersection */` |
|      15 | 4829 | `	pEntry = pSrc->pFirst;` |
|      15 | 4830 | `	n = pSrc->nEntry;` |
|      24 | 4831 | `	for(;;){` |
|      49 | 4832 | `		if( n < 1 ){` |
|      15 | 4833 | `			break;` |
|       - | 4834 | `		}` |
|      57 | 4835 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 4836 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 4837 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 4838 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4839 | `				/* Blob lookup */` |
|      27 | 4840 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 4841 | `			}else{` |
|       - | 4842 | `				/* Int key */` |
|      13 | 4843 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4844 | `			}` |
|      39 | 4845 | `			if( rc != SXRET_OK ){` |
|       - | 4846 | `				/* Key does not exist, break immediately */` |
|      17 | 4847 | `				break;` |
|       - | 4848 | `			}` |
|      12 | 4849 | `		}` |
|      35 | 4850 | `		if( i >= nArg ){` |
|       - | 4851 | `			/* Perform the insertion */` |
|      19 | 4852 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 4853 | `		}` |
|       - | 4854 | `		/* Point to the next entry */` |
|      35 | 4855 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 4856 | `		n--;` |
|       1 | 4857 | `	}` |
|       - | 4858 | `	/* Return the freshly created array */` |
|      15 | 4859 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4860 | `	return PH7_OK;` |
|      13 | 4861 |  |
|       - | 4862 | `/*` |
|       - | 4863 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4864 | ` *  Computes the intersection of arrays.` |
|       - | 4865 | ` * Parameters` |
|       - | 4866 | ` *  $array1` |
|       - | 4867 | ` *    The array to compare from` |
|       - | 4868 | ` *  $array2` |
|       - | 4869 | ` *    An array to compare against` |
|       - | 4870 | ` *  $...` |
|       - | 4871 | ` *   More arrays to compare against` |
|       - | 4872 | ` * $callback` |
|       - | 4873 | ` *  The callback comparison function.` |
|       - | 4874 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4875 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4876 | ` *  than the second.` |
|       - | 4877 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4878 | ` * Return` |
|       - | 4879 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4880 | ` *  in all of the parameters. .` |
|       - | 4881 | ` * Note that NULL is returned on failure.` |
|       - | 4882 | ` */` |
|      24 | 4883 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4884 |  |
|       - | 4885 | `	ph7_hashmap_node *pEntry;` |
|       - | 4886 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4887 | `	ph7_value *pCallback;` |
|       - | 4888 | `	ph7_value *pArray;` |
|       - | 4889 | `	ph7_value *pVal;` |
|       - | 4890 | `	sxi32 rc;` |
|       - | 4891 | `	sxu32 n;` |
|       - | 4892 | `	int i;` |
|       - | 4893 |  |
|       - | 4894 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      26 | 4895 | `	if( nArg < 2 ){` |
|       4 | 4896 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4897 | `			"ArgumentCountError",` |
|       - | 4898 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 4899 | `			nArg` |
|       - | 4900 | `			);` |
|       - | 4901 | `	}` |
|      24 | 4902 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4903 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4904 | `			"TypeError",` |
|       - | 4905 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4906 | `			ph7_type_name(apArg[0])` |
|       - | 4907 | `			);` |
|       - | 4908 | `	}` |
|       - | 4909 |  |
|      22 | 4910 | `	if( nArg == 2 ){` |
|       - | 4911 | `		/* Only the original array and the callback were provided. */` |
|       - | 4912 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 4913 | `		 * validation ordering. */` |
|       3 | 4914 | `	} else {` |
|       - | 4915 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      32 | 4916 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      18 | 4917 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4918 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4919 | `					"TypeError",` |
|       - | 4920 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4921 | `					i + 1,` |
|       2 | 4922 | `					ph7_type_name(apArg[i])` |
|       - | 4923 | `					);` |
|       - | 4924 | `			}` |
|       9 | 4925 | `		}` |
|       - | 4926 | `	}` |
|       - | 4927 |  |
|       - | 4928 | `	/* Identify the callback (always expected as the last argument). */` |
|      20 | 4929 | `	pCallback = apArg[nArg - 1];` |
|       - | 4930 | `	/* Validate the callback to match PHP's error messages. */` |
|      20 | 4931 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      11 | 4932 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4933 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 4934 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 4935 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 4936 | `			 */` |
|       7 | 4937 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       7 | 4938 | `			if( pCb->nEntry != 2 ){` |
|       4 | 4939 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4940 | `					"TypeError",` |
|       - | 4941 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4942 | `					nArg` |
|       - | 4943 | `					);` |
|       - | 4944 | `			}` |
|       - | 4945 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 4946 | `			{` |
|       5 | 4947 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       5 | 4948 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       5 | 4949 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 4950 | `					int nMethodLen;` |
|       5 | 4951 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       5 | 4952 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       5 | 4953 | `					if( pClass ){` |
|       - | 4954 | `						/* Class exists but method is missing. */` |
|       4 | 4955 | `						return PH7_VmThrowException(pCtx,` |
|       - | 4956 | `							"TypeError",` |
|       - | 4957 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 4958 | `							nArg,` |
|       1 | 4959 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 4960 | `							zMethod` |
|       - | 4961 | `							);` |
|       - | 4962 | `					}` |
|       - | 4963 | `					/* Class not found */` |
|       - | 4964 | `					{` |
|       - | 4965 | `						int nName;` |
|       3 | 4966 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 4967 | `						return PH7_VmThrowException(pCtx,` |
|       - | 4968 | `							"TypeError",` |
|       - | 4969 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 4970 | `							nArg,` |
|       1 | 4971 | `							zName` |
|       - | 4972 | `							);` |
|       - | 4973 | `					}` |
|       - | 4974 | `				}` |
|       - | 4975 | `			}` |
|       - | 4976 | `			/* Fallback message */` |
|     ! 0 | 4977 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4978 | `				"TypeError",` |
|       - | 4979 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 4980 | `				nArg` |
|       - | 4981 | `				);` |
|       - | 4982 | `		}` |
|       5 | 4983 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4984 | `			int len;` |
|       3 | 4985 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4986 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4987 | `				"TypeError",` |
|       - | 4988 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4989 | `				nArg,` |
|       1 | 4990 | `				zName` |
|       - | 4991 | `				);` |
|       - | 4992 | `		}` |
|       4 | 4993 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4994 | `			"TypeError",` |
|       - | 4995 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4996 | `			nArg` |
|       - | 4997 | `			);` |
|       - | 4998 | `	}` |
|       - | 4999 |  |
|       9 | 5000 | `	if( nArg == 2 ){` |
|       - | 5001 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5002 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5003 | `		return PH7_OK;` |
|       - | 5004 | `	}` |
|       - | 5005 |  |
|       - | 5006 | `	/* Create a new array */` |
|       5 | 5007 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 5008 | `	if( pArray == 0 ){` |
|     ! 0 | 5009 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5010 | `		return PH7_OK;` |
|       - | 5011 | `	}` |
|       - | 5012 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 5013 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5014 | `	/* Perform the intersection */` |
|       5 | 5015 | `	pEntry = pSrc->pFirst;` |
|       5 | 5016 | `	n = pSrc->nEntry;` |
|       8 | 5017 | `	for(;;){` |
|      17 | 5018 | `		if( n < 1 ){` |
|       5 | 5019 | `			break;` |
|       - | 5020 | `		}` |
|       - | 5021 | `		/* Extract the node value */` |
|      13 | 5022 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      13 | 5023 | `		if( pVal ){` |
|      21 | 5024 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      13 | 5025 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5026 | `					/* ignore */` |
|     ! 0 | 5027 | `					continue;` |
|       - | 5028 | `				}` |
|       - | 5029 | `				/* Point to the internal representation of the hashmap */` |
|      13 | 5030 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5031 | `				/* Perform the lookup */` |
|      13 | 5032 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      13 | 5033 | `				if( rc != SXRET_OK ){` |
|       - | 5034 | `					/* Value does not exist */` |
|       5 | 5035 | `					break;` |
|       - | 5036 | `				}` |
|       5 | 5037 | `			}` |
|      13 | 5038 | `			if( i >= (nArg-1) ){` |
|       - | 5039 | `				/* Perform the insertion */` |
|       9 | 5040 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5041 | `			}` |
|       6 | 5042 | `		}` |
|       - | 5043 | `		/* Point to the next entry */` |
|      13 | 5044 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5045 | `		n--;` |
|       1 | 5046 | `	}` |
|       - | 5047 | `	/* Return the freshly created array */` |
|       5 | 5048 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5049 | `	return PH7_OK;` |
|      14 | 5050 |  |
|       - | 5051 | `/*` |
|       - | 5052 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5053 | ` *  Fill an array with values.` |
|       - | 5054 | ` * Parameters` |
|       - | 5055 | ` *  $start_index` |
|       - | 5056 | ` *    The first index of the returned array.` |
|       - | 5057 | ` *  $num` |
|       - | 5058 | ` *   Number of elements to insert.` |
|       - | 5059 | ` *  $value` |
|       - | 5060 | ` *    Value to use for filling.` |
|       - | 5061 | ` * Return` |
|       - | 5062 | ` *  The filled array or null on failure.` |
|       - | 5063 | ` */` |
|     238 | 5064 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5065 |  |
|       - | 5066 | `	ph7_value *pArray;` |
|       - | 5067 | `	int i,nEntry;` |
|       - | 5068 |  |
|       - | 5069 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 5070 | `	if( nArg != 3 ){` |
|       - | 5071 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 5072 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5073 | `			"ArgumentCountError",` |
|       - | 5074 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5075 | `			nArg` |
|       - | 5076 | `			);` |
|       - | 5077 | `	}` |
|       - | 5078 |  |
|       - | 5079 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5080 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5081 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5082 | `	 * and NULLs are rejected outright. */` |
|     466 | 5083 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 5084 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5085 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5086 | `			"TypeError",` |
|       - | 5087 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5088 | `			ph7_type_name(apArg[0])` |
|       - | 5089 | `			);` |
|       - | 5090 | `	}` |
|     234 | 5091 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5092 | `		int len;` |
|       8 | 5093 | `		sxu8 bReal = FALSE;` |
|       8 | 5094 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5095 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5096 | `			/* Non‑numeric string is an error. */` |
|       3 | 5097 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5098 | `				"TypeError",` |
|       - | 5099 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5100 | `				);` |
|       - | 5101 | `		}` |
|       5 | 5102 | `		if( bReal ){` |
|       - | 5103 | `			/* float-string -> deprecation warning */` |
|       4 | 5104 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5105 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5106 | `				zStr` |
|       - | 5107 | `				);` |
|       1 | 5108 | `		}` |
|       2 | 5109 | `	}` |
|       - | 5110 |  |
|       - | 5111 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5112 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 5113 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 5114 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5115 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5116 | `			"TypeError",` |
|       - | 5117 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5118 | `			ph7_type_name(apArg[1])` |
|       - | 5119 | `			);` |
|       - | 5120 | `	}` |
|     232 | 5121 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5122 | `		int len;` |
|       3 | 5123 | `		sxu8 bReal = FALSE;` |
|       3 | 5124 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5125 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5126 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5127 | `				"TypeError",` |
|       - | 5128 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5129 | `				);` |
|       - | 5130 | `		}` |
|     ! 0 | 5131 | `	}` |
|       - | 5132 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5133 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5134 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5135 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5136 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5137 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5138 | `		if( d != (double)i64 ){` |
|       7 | 5139 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5140 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5141 | `				d` |
|       - | 5142 | `				);` |
|       2 | 5143 | `		}` |
|       2 | 5144 | `	}` |
|       - | 5145 |  |
|       - | 5146 | `	/* Total number of entries to insert */` |
|     230 | 5147 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5148 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5149 | `	if( nEntry < 0 ){` |
|       3 | 5150 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5151 | `			"ValueError",` |
|       - | 5152 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5153 | `			);` |
|       - | 5154 | `	}` |
|       - | 5155 |  |
|       - | 5156 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5157 | `	if( nEntry == 0 ){` |
|       7 | 5158 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5159 | `		return PH7_OK;` |
|       - | 5160 | `	}` |
|       - | 5161 |  |
|       - | 5162 | `	/* Create a new array */` |
|     221 | 5163 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5164 | `	if( pArray == 0 ){` |
|     ! 0 | 5165 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5166 | `		return PH7_OK;` |
|       - | 5167 | `	}` |
|       - | 5168 |  |
|       - | 5169 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5170 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 5171 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5172 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5173 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 5174 | `	}` |
|       - | 5175 | `	/* Return the filled array */` |
|     221 | 5176 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5177 | `	return PH7_OK;` |
|     121 | 5178 |  |
|       - | 5179 | `/*` |
|       - | 5180 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5181 | ` *  Fill an array with values, specifying keys.` |
|       - | 5182 | ` * Parameters` |
|       - | 5183 | ` *  $input` |
|       - | 5184 | ` *   Array of values that will be used as key.` |
|       - | 5185 | ` *  $value` |
|       - | 5186 | ` *    Value to use for filling.` |
|       - | 5187 | ` * Return` |
|       - | 5188 | ` *  The filled array.` |
|       - | 5189 | ` * Throws` |
|       - | 5190 | ` *  ValueError if $input is not an array.` |
|       - | 5191 | ` */` |
|      26 | 5192 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5193 |  |
|       - | 5194 | `	ph7_hashmap_node *pEntry;` |
|       - | 5195 | `	ph7_hashmap *pSrc;` |
|       - | 5196 | `	ph7_value *pArray;` |
|       - | 5197 | `	sxu32 n;` |
|       - | 5198 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5199 | `	if( nArg != 2 ){` |
|      10 | 5200 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5201 | `			"ArgumentCountError",` |
|       - | 5202 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5203 | `			nArg` |
|       - | 5204 | `			);` |
|       - | 5205 | `	}` |
|       - | 5206 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5207 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5208 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5209 | `			"TypeError",` |
|       - | 5210 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5211 | `			ph7_type_name(apArg[0])` |
|       - | 5212 | `			);` |
|       - | 5213 | `	}` |
|       - | 5214 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5215 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5216 | `	/* Create a new array */` |
|      17 | 5217 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5218 | `	if( pArray == 0 ){` |
|     ! 0 | 5219 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5220 | `		return PH7_OK;` |
|       - | 5221 | `	}` |
|       - | 5222 | `	/* Perform the requested operation */` |
|      17 | 5223 | `	pEntry = pSrc->pFirst;` |
|      45 | 5224 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5225 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5226 | `		/* Point to the next entry */` |
|      29 | 5227 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5228 | `	}` |
|       - | 5229 | `	/* Return the filled array */` |
|      17 | 5230 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5231 | `	return PH7_OK;` |
|      15 | 5232 |  |
|       - | 5233 | `/*` |
|       - | 5234 | ` * array array_combine(array $keys,array $values)` |
|       - | 5235 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5236 | ` * Parameters` |
|       - | 5237 | ` *  $keys` |
|       - | 5238 | ` *    Array of keys to be used.` |
|       - | 5239 | ` * $values` |
|       - | 5240 | ` *   Array of values to be used.` |
|       - | 5241 | ` * Return` |
|       - | 5242 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5243 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5244 | ` *  not an array.` |
|       - | 5245 | ` */` |
|      18 | 5246 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5247 |  |
|       - | 5248 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5249 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5250 | `	ph7_value *pArray;` |
|       - | 5251 | `	sxu32 n;` |
|       - | 5252 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5253 | `	if( nArg != 2 ){` |
|       - | 5254 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5255 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5256 | `			"ArgumentCountError",` |
|       - | 5257 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5258 | `			nArg` |
|       - | 5259 | `			);` |
|       - | 5260 | `	}` |
|       - | 5261 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5262 | `	 * argument index in the error message. */` |
|      18 | 5263 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5264 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5265 | `			"TypeError",` |
|       - | 5266 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5267 | `			ph7_type_name(apArg[0])` |
|       - | 5268 | `			);` |
|       - | 5269 | `	}` |
|      16 | 5270 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5271 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5272 | `			"TypeError",` |
|       - | 5273 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5274 | `			ph7_type_name(apArg[1])` |
|       - | 5275 | `			);` |
|       - | 5276 | `	}` |
|       - | 5277 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5278 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5279 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5280 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5281 | `		/* Length mismatch -> ValueError */` |
|       3 | 5282 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5283 | `			"ValueError",` |
|       - | 5284 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5285 | `			);` |
|       - | 5286 | `	}` |
|       - | 5287 | `	/* Create a new array */` |
|      11 | 5288 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5289 | `	if( pArray == 0 ){` |
|     ! 0 | 5290 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5291 | `		return PH7_OK;` |
|       - | 5292 | `	}` |
|       - | 5293 | `	/* Perform the requested operation */` |
|      11 | 5294 | `	pKe = pKey->pFirst;` |
|      11 | 5295 | `	pVe = pValue->pFirst;` |
|      33 | 5296 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5297 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5298 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5299 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5300 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5301 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5302 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5303 | `		 * original array must not be mutated. */` |
|      23 | 5304 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5305 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5306 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5307 | `			if( pTmpKey ){` |
|       5 | 5308 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5309 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5310 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5311 | `				pKeyCopy = pTmpKey;` |
|       2 | 5312 | `			}` |
|       2 | 5313 | `		}` |
|      23 | 5314 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5315 | `		/* Point to the next entry */` |
|      23 | 5316 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5317 | `		pVe = pVe->pPrev;` |
|      12 | 5318 | `	}` |
|       - | 5319 | `	/* Return the filled array */` |
|      11 | 5320 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5321 | `	return PH7_OK;` |
|      11 | 5322 |  |
|       - | 5323 | `/*` |
|       - | 5324 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5325 | ` *  Return an array with elements in reverse order.` |
|       - | 5326 | ` * Parameters` |
|       - | 5327 | ` *  $array` |
|       - | 5328 | ` *   The input array.` |
|       - | 5329 | ` *  $preserve_keys (optional)` |
|       - | 5330 | ` *   If set to TRUE keys are preserved.` |
|       - | 5331 | ` * Return` |
|       - | 5332 | ` *  The reversed array.` |
|       - | 5333 | ` */` |
|      20 | 5334 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5335 |  |
|       - | 5336 | `	ph7_hashmap_node *pEntry;` |
|       - | 5337 | `	ph7_hashmap *pSrc;` |
|       - | 5338 | `	ph7_value *pArray;` |
|       - | 5339 | `	int bPreserve;` |
|       - | 5340 | `	sxu32 n;` |
|      22 | 5341 | `	if( nArg < 1 ){` |
|       4 | 5342 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5343 | `			"ArgumentCountError",` |
|       - | 5344 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5345 | `			nArg` |
|       - | 5346 | `			);` |
|       - | 5347 | `	}` |
|       - | 5348 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5349 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5350 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5351 | `			"TypeError",` |
|       - | 5352 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5353 | `			ph7_type_name(apArg[0])` |
|       - | 5354 | `			);` |
|       - | 5355 | `	}` |
|      17 | 5356 | `	bPreserve = FALSE;` |
|      17 | 5357 | `	if( nArg > 1 ){` |
|       7 | 5358 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5359 | `	}` |
|       - | 5360 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5361 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5362 | `	/* Create a new array */` |
|      17 | 5363 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5364 | `	if( pArray == 0 ){` |
|     ! 0 | 5365 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5366 | `		return PH7_OK;` |
|       - | 5367 | `	}` |
|       - | 5368 | `	/* Perform the requested operation */` |
|      17 | 5369 | `	pEntry = pSrc->pLast;` |
|      55 | 5370 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5371 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5372 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5373 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5374 | `		/* Point to the previous entry */` |
|      39 | 5375 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5376 | `	}` |
|      17 | 5377 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5378 | `	return PH7_OK;` |
|      12 | 5379 |  |
|       - | 5380 | `/*` |
|       - | 5381 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5382 | ` *  Removes duplicate values from an array.` |
|       - | 5383 | ` * Parameters` |
|       - | 5384 | ` *  $array` |
|       - | 5385 | ` *   The input array.` |
|       - | 5386 | ` *  $flags` |
|       - | 5387 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5388 | ` *   behavior using these values:` |
|       - | 5389 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5390 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5391 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5392 | ` * Return` |
|       - | 5393 | ` *  The filtered array.` |
|       - | 5394 | ` */` |
|      24 | 5395 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5396 |  |
|       - | 5397 | `	ph7_hashmap_node *pEntry;` |
|       - | 5398 | `	ph7_value *pNeedle;` |
|       - | 5399 | `	ph7_hashmap *pSrc;` |
|       - | 5400 | `	ph7_value *pArray;` |
|       - | 5401 | `	int bStrict;` |
|       - | 5402 | `	sxi32 rc;` |
|       - | 5403 | `	sxu32 n;` |
|      26 | 5404 | `	if( nArg < 1 ){` |
|       - | 5405 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5406 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5407 | `			"ArgumentCountError",` |
|       - | 5408 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5409 | `			);` |
|       - | 5410 | `	}` |
|      24 | 5411 | `	if( nArg > 2 ){` |
|       - | 5412 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5413 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5414 | `			"ArgumentCountError",` |
|       - | 5415 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5416 | `			nArg` |
|       - | 5417 | `			);` |
|       - | 5418 | `	}` |
|       - | 5419 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5420 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5421 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5422 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5423 | `			"TypeError",` |
|       - | 5424 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5425 | `			ph7_type_name(apArg[0])` |
|       - | 5426 | `			);` |
|       - | 5427 | `	}` |
|      19 | 5428 | `	bStrict = FALSE;` |
|       - | 5429 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5430 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5431 | `	/* Create a new array */` |
|      19 | 5432 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5433 | `	if( pArray == 0 ){` |
|     ! 0 | 5434 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5435 | `		return PH7_OK;` |
|       - | 5436 | `	}` |
|       - | 5437 | `	/* Perform the requested operation */` |
|      19 | 5438 | `	pEntry = pSrc->pFirst;` |
|      83 | 5439 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5440 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5441 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5442 | `		if( pNeedle ){` |
|      65 | 5443 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5444 | `		}` |
|      65 | 5445 | `		if( rc != SXRET_OK ){` |
|       - | 5446 | `			/* Perform the insertion */` |
|      37 | 5447 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5448 | `		}` |
|       - | 5449 | `		/* Point to the next entry */` |
|      65 | 5450 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5451 | `	}` |
|       - | 5452 | `	/* Return the freshly created array */` |
|      19 | 5453 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5454 | `	return PH7_OK;` |
|      14 | 5455 |  |
|       - | 5456 | `/*` |
|       - | 5457 | ` * array array_flip(array $input)` |
|       - | 5458 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5459 | ` * Parameter` |
|       - | 5460 | ` *  $input` |
|       - | 5461 | ` *   Input array.` |
|       - | 5462 | ` * Return` |
|       - | 5463 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5464 | ` */` |
|      34 | 5465 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5466 |  |
|       - | 5467 | `	ph7_hashmap_node *pEntry;` |
|       - | 5468 | `	ph7_hashmap *pSrc;` |
|       - | 5469 | `	ph7_value *pArray;` |
|       - | 5470 | `	ph7_value *pKey;` |
|       - | 5471 | `	ph7_value sVal;` |
|       - | 5472 | `	sxu32 n;` |
|       - | 5473 |  |
|       - | 5474 | `	/* PHP requires exactly one argument */` |
|      36 | 5475 | `	if( nArg != 1 ){` |
|       - | 5476 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5477 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5478 | `			"ArgumentCountError",` |
|       - | 5479 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5480 | `			nArg` |
|       - | 5481 | `			);` |
|       - | 5482 | `	}` |
|       - | 5483 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5484 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5485 | `		/* Type mismatch -> TypeError */` |
|       7 | 5486 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5487 | `			"TypeError",` |
|       - | 5488 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5489 | `			ph7_type_name(apArg[0])` |
|       - | 5490 | `			);` |
|       - | 5491 | `	}` |
|       - | 5492 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5493 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5494 | `	/* Create a new array */` |
|      27 | 5495 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5496 | `	if( pArray == 0 ){` |
|     ! 0 | 5497 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5498 | `		return PH7_OK;` |
|       - | 5499 | `	}` |
|       - | 5500 | `	/* Start processing */` |
|      27 | 5501 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5502 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5503 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5504 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5505 | `		if( pKey ){` |
|       - | 5506 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5507 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5508 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5509 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5510 | `					);` |
|   22236 | 5511 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5512 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5513 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5514 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5515 | `				}else{` |
|       - | 5516 | `					SyString sStr;` |
|    2227 | 5517 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5518 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5519 | `				}` |
|       - | 5520 | `				/* Perform the insertion */` |
|   22227 | 5521 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5522 | `				/* Safely release the value because each inserted entry` |
|       - | 5523 | `				 * has its own private copy of the value.` |
|       - | 5524 | `				 */` |
|   22227 | 5525 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5526 | `			}else{` |
|       - | 5527 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5528 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5529 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5530 | `					);` |
|       - | 5531 | `			}` |
|   11118 | 5532 | `		}` |
|       - | 5533 | `		/* Point to the next entry */` |
|   22237 | 5534 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5535 | `	}` |
|       - | 5536 | `	/* Return the freshly created array */` |
|      27 | 5537 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5538 | `	return PH7_OK;` |
|      19 | 5539 |  |
|       - | 5540 | `/*` |
|       - | 5541 | ` * number array_sum(array $array )` |
|       - | 5542 | ` *  Calculate the sum of values in an array.` |
|       - | 5543 | ` * Parameters` |
|       - | 5544 | ` *  $array: The input array.` |
|       - | 5545 | ` * Return` |
|       - | 5546 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5547 | ` */` |
|      24 | 5548 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5549 |  |
|       - | 5550 | `	ph7_hashmap_node *pEntry;` |
|       - | 5551 | `	ph7_value *pObj;` |
|      25 | 5552 | `	double dSum = 0;` |
|       - | 5553 | `	sxu32 n;` |
|      25 | 5554 | `	pEntry = pMap->pFirst;` |
|      91 | 5555 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5556 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5557 | `		if( pObj ){` |
|      67 | 5558 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5559 | `				dSum += pObj->rVal;` |
|      53 | 5560 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5561 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5562 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5563 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5564 | `					double dv = 0;` |
|      13 | 5565 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5566 | `					dSum += dv;` |
|       7 | 5567 | `				}` |
|      12 | 5568 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5569 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5570 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5571 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5572 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5573 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5574 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5575 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5576 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5577 | `			}` |
|       - | 5578 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5579 | `		}` |
|       - | 5580 | `		/* Point to the next entry */` |
|      67 | 5581 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5582 | `	}` |
|       - | 5583 | `	/* Return sum */` |
|      25 | 5584 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5585 |  |
|      18 | 5586 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5587 |  |
|       - | 5588 | `	ph7_hashmap_node *pEntry;` |
|       - | 5589 | `	ph7_value *pObj;` |
|      20 | 5590 | `	sxi64 nSum = 0;` |
|       - | 5591 | `	sxu32 n;` |
|      20 | 5592 | `	pEntry = pMap->pFirst;` |
|      80 | 5593 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5594 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5595 | `		if( pObj ){` |
|      62 | 5596 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5597 | `				nSum += pObj->x.iVal;` |
|      36 | 5598 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5599 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5600 | `					sxi64 nv = 0;` |
|       5 | 5601 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5602 | `					nSum += nv;` |
|       3 | 5603 | `				}` |
|       8 | 5604 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5605 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5606 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5607 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5608 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5609 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5610 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5611 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5612 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5613 | `			}` |
|       - | 5614 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5615 | `		}` |
|       - | 5616 | `		/* Point to the next entry */` |
|      62 | 5617 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5618 | `	}` |
|       - | 5619 | `	/* Return sum */` |
|      20 | 5620 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5621 |  |
|       - | 5622 | `/* number array_sum(array $array )` |
|       - | 5623 | ` * (See block-coment above)` |
|       - | 5624 | ` */` |
|      52 | 5625 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5626 |  |
|       - | 5627 | `	ph7_hashmap_node *pEntry;` |
|       - | 5628 | `	ph7_hashmap *pMap;` |
|       - | 5629 | `	ph7_value *pObj;` |
|      54 | 5630 | `	int useDouble = 0;` |
|       - | 5631 | `	sxu32 n;` |
|       - | 5632 | `	/* PHP requires exactly one argument */` |
|      54 | 5633 | `	if( nArg != 1 ){` |
|       7 | 5634 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5635 | `			"ArgumentCountError",` |
|       - | 5636 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5637 | `			nArg` |
|       - | 5638 | `			);` |
|       - | 5639 | `	}` |
|       - | 5640 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5641 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5642 | `		/* Type mismatch -> TypeError */` |
|       7 | 5643 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5644 | `			"TypeError",` |
|       - | 5645 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5646 | `			ph7_type_name(apArg[0])` |
|       - | 5647 | `			);` |
|       - | 5648 | `	}` |
|      46 | 5649 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5650 | `	if( pMap->nEntry < 1 ){` |
|       - | 5651 | `		/* Nothing to compute,return 0 */` |
|       3 | 5652 | `		ph7_result_int(pCtx,0);` |
|       3 | 5653 | `		return PH7_OK;` |
|       - | 5654 | `	}` |
|       - | 5655 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5656 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5657 | `	 */` |
|      44 | 5658 | `	pEntry = pMap->pFirst;` |
|     112 | 5659 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5660 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5661 | `		if( pObj ){` |
|      94 | 5662 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5663 | `				useDouble = 1;` |
|      19 | 5664 | `				break;` |
|       - | 5665 | `			}` |
|      76 | 5666 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5667 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5668 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5669 | `				sxu32 i;` |
|      23 | 5670 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5671 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5672 | `						useDouble = 1;` |
|       7 | 5673 | `						break;` |
|       - | 5674 | `					}` |
|       6 | 5675 | `				}` |
|      13 | 5676 | `				if( useDouble ){` |
|       7 | 5677 | `					break;` |
|       - | 5678 | `				}` |
|       3 | 5679 | `			}` |
|      34 | 5680 | `		}` |
|      70 | 5681 | `		pEntry = pEntry->pPrev;` |
|      36 | 5682 | `	}` |
|      44 | 5683 | `	if( useDouble ){` |
|      25 | 5684 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5685 | `	}else{` |
|      20 | 5686 | `		Int64Sum(pCtx,pMap);` |
|       - | 5687 | `	}` |
|      44 | 5688 | `	return PH7_OK;` |
|      28 | 5689 |  |
|       - | 5690 | `/*` |
|       - | 5691 | ` * number array_product(array $array )` |
|       - | 5692 | ` *  Calculate the product of values in an array.` |
|       - | 5693 | ` * Parameters` |
|       - | 5694 | ` *  $array: The input array.` |
|       - | 5695 | ` * Return` |
|       - | 5696 | ` *  Returns the product of values as an integer or float.` |
|       - | 5697 | ` */` |
|     ! 0 | 5698 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5699 |  |
|       - | 5700 | `	ph7_hashmap_node *pEntry;` |
|       - | 5701 | `	ph7_value *pObj;` |
|       - | 5702 | `	double dProd;` |
|       - | 5703 | `	sxu32 n;` |
|     ! 0 | 5704 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5705 | `	dProd = 1;` |
|     ! 0 | 5706 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5707 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5708 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5709 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5710 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5711 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5712 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5713 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5714 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5715 | `					double dv = 0;` |
|     ! 0 | 5716 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5717 | `					dProd *= dv;` |
|     ! 0 | 5718 | `				}` |
|     ! 0 | 5719 | `			}` |
|     ! 0 | 5720 | `		}` |
|       - | 5721 | `		/* Point to the next entry */` |
|     ! 0 | 5722 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5723 | `	}` |
|       - | 5724 | `	/* Return product */` |
|     ! 0 | 5725 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5726 |  |
|     ! 0 | 5727 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5728 |  |
|       - | 5729 | `	ph7_hashmap_node *pEntry;` |
|       - | 5730 | `	ph7_value *pObj;` |
|       - | 5731 | `	sxi64 nProd;` |
|       - | 5732 | `	sxu32 n;` |
|     ! 0 | 5733 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5734 | `	nProd = 1;` |
|     ! 0 | 5735 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5736 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5737 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5738 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5739 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5740 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5741 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5742 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5743 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5744 | `					sxi64 nv = 0;` |
|     ! 0 | 5745 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5746 | `					nProd *= nv;` |
|     ! 0 | 5747 | `				}` |
|     ! 0 | 5748 | `			}` |
|     ! 0 | 5749 | `		}` |
|       - | 5750 | `		/* Point to the next entry */` |
|     ! 0 | 5751 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5752 | `	}` |
|       - | 5753 | `	/* Return product */` |
|     ! 0 | 5754 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5755 |  |
|       - | 5756 | `/* number array_product(array $array )` |
|       - | 5757 | ` * (See block-block comment above)` |
|       - | 5758 | ` */` |
|     ! 0 | 5759 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5760 |  |
|       - | 5761 | `	ph7_hashmap *pMap;` |
|       - | 5762 | `	ph7_value *pObj;` |
|     ! 0 | 5763 | `	if( nArg < 1 ){` |
|       - | 5764 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5765 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5766 | `		return PH7_OK;` |
|       - | 5767 | `	}` |
|       - | 5768 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5769 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5770 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5771 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5772 | `		return PH7_OK;` |
|       - | 5773 | `	}` |
|     ! 0 | 5774 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5775 | `	if( pMap->nEntry < 1 ){` |
|       - | 5776 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5777 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5778 | `		return PH7_OK;` |
|       - | 5779 | `	}` |
|       - | 5780 | `	/* If the first element is of type float,then perform floating` |
|       - | 5781 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5782 | `	 */` |
|     ! 0 | 5783 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5784 | `	if( pObj == 0 ){` |
|     ! 0 | 5785 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5786 | `		return PH7_OK;` |
|       - | 5787 | `	}` |
|     ! 0 | 5788 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5789 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5790 | `	}else{` |
|     ! 0 | 5791 | `		Int64Prod(pCtx,pMap);` |
|       - | 5792 | `	}` |
|     ! 0 | 5793 | `	return PH7_OK;` |
|     ! 0 | 5794 |  |
|       - | 5795 | `/*` |
|       - | 5796 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5797 | ` *  Pick one or more random entries out of an array.` |
|       - | 5798 | ` * Parameters` |
|       - | 5799 | ` * $input` |
|       - | 5800 | ` *  The input array.` |
|       - | 5801 | ` * $num_req` |
|       - | 5802 | ` *  Specifies how many entries you want to pick.` |
|       - | 5803 | ` * Return` |
|       - | 5804 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5805 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5806 | ` *  NULL is returned on failure.` |
|       - | 5807 | ` */` |
|       6 | 5808 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5809 |  |
|       - | 5810 | `	ph7_hashmap_node *pNode;` |
|       - | 5811 | `	ph7_hashmap *pMap;` |
|       7 | 5812 | `	int nItem = 1;` |
|       7 | 5813 | `	if( nArg < 1 ){` |
|       - | 5814 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5815 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5816 | `		return PH7_OK;` |
|       - | 5817 | `	}` |
|       - | 5818 | `	/* Make sure we are dealing with an array */` |
|       7 | 5819 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5820 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5821 | `		return PH7_OK;` |
|       - | 5822 | `	}` |
|       - | 5823 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5824 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5825 | `	if(pMap->nEntry < 1 ){` |
|       - | 5826 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5827 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5828 | `		return PH7_OK;` |
|       - | 5829 | `	}` |
|       7 | 5830 | `	if( nArg > 1 ){` |
|       3 | 5831 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5832 | `	}` |
|       7 | 5833 | `	if( nItem < 2 ){` |
|       - | 5834 | `		sxu32 nEntry;` |
|       - | 5835 | `		/* Select a random number */` |
|       5 | 5836 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5837 | `		/* Extract the desired entry.` |
|       - | 5838 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5839 | `		 */` |
|       5 | 5840 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       1 | 5841 | `			pNode = pMap->pLast;` |
|       1 | 5842 | `			nEntry = pMap->nEntry - nEntry;` |
|       1 | 5843 | `			if( nEntry > 1 ){` |
|     ! 0 | 5844 | `				for(;;){` |
|     ! 0 | 5845 | `					if( nEntry == 0 ){` |
|     ! 0 | 5846 | `						break;` |
|       - | 5847 | `					}` |
|       - | 5848 | `					/* Point to the previous entry */` |
|     ! 0 | 5849 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5850 | `					nEntry--;` |
|     ! 0 | 5851 | `				}` |
|     ! 0 | 5852 | `			}` |
|       1 | 5853 | `		}else{` |
|       4 | 5854 | `			pNode = pMap->pFirst;` |
|       3 | 5855 | `			for(;;){` |
|       7 | 5856 | `				if( nEntry == 0 ){` |
|       4 | 5857 | `					break;` |
|       - | 5858 | `				}` |
|       - | 5859 | `				/* Point to the next entry */` |
|       3 | 5860 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       3 | 5861 | `				nEntry--;` |
|     ! 0 | 5862 | `			}` |
|       - | 5863 | `		}` |
|       5 | 5864 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5865 | `			/* Int key */` |
|       3 | 5866 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5867 | `		}else{` |
|       - | 5868 | `			/* Blob key */` |
|       3 | 5869 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5870 | `		}` |
|       3 | 5871 | `	}else{` |
|       - | 5872 | `		ph7_value sKey,*pArray;` |
|       - | 5873 | `		ph7_hashmap *pDest;` |
|       - | 5874 | `		/* Create a new array */` |
|       3 | 5875 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5876 | `		if( pArray == 0 ){` |
|     ! 0 | 5877 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5878 | `			return PH7_OK;` |
|       - | 5879 | `		}` |
|       - | 5880 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5881 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5882 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5883 | `		/* Copy the first n items */` |
|       3 | 5884 | `		pNode = pMap->pFirst;` |
|       3 | 5885 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5886 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5887 | `		}` |
|       7 | 5888 | `		while( nItem > 0){` |
|       5 | 5889 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5890 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5891 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5892 | `			/* Point to the next entry */` |
|       5 | 5893 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5894 | `			nItem--;` |
|       1 | 5895 | `		}` |
|       - | 5896 | `		/* Shuffle the array */` |
|       3 | 5897 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5898 | `		/* Rehash node */` |
|       3 | 5899 | `		HashmapSortRehash(pDest);` |
|       - | 5900 | `		/* Return the random array */` |
|       3 | 5901 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5902 | `	}` |
|       7 | 5903 | `	return PH7_OK;` |
|       4 | 5904 |  |
|       - | 5905 | `/*` |
|       - | 5906 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5907 | ` *  Split an array into chunks.` |
|       - | 5908 | ` * Parameters` |
|       - | 5909 | ` * $input` |
|       - | 5910 | ` *   The array to work on` |
|       - | 5911 | ` * $size` |
|       - | 5912 | ` *   The size of each chunk` |
|       - | 5913 | ` * $preserve_keys` |
|       - | 5914 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5915 | ` *   the chunk numerically.` |
|       - | 5916 | ` * Return` |
|       - | 5917 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5918 | ` *  zero, with each dimension containing size elements.` |
|       - | 5919 | ` */` |
|      42 | 5920 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5921 |  |
|       - | 5922 | `	ph7_value *pArray,*pChunk;` |
|       - | 5923 | `	ph7_hashmap_node *pEntry;` |
|       - | 5924 | `	ph7_hashmap *pMap;` |
|       - | 5925 | `	int bPreserve;` |
|       - | 5926 | `	sxu32 nChunk;` |
|       - | 5927 | `	sxu32 nSize;` |
|       - | 5928 | `	sxu32 n;` |
|       - | 5929 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5930 | `	if( nArg < 2 ){` |
|       - | 5931 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5932 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5933 | `			"ArgumentCountError",` |
|       - | 5934 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5935 | `			nArg` |
|       - | 5936 | `			);` |
|       - | 5937 | `	}` |
|      42 | 5938 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5939 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5940 | `			"TypeError",` |
|       - | 5941 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5942 | `			ph7_type_name(apArg[0])` |
|       - | 5943 | `			);` |
|       - | 5944 | `	}` |
|       - | 5945 | `	/* Create a new array */` |
|      40 | 5946 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5947 | `	if( pArray == 0 ){` |
|     ! 0 | 5948 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5949 | `		return PH7_OK;` |
|       - | 5950 | `	}` |
|       - | 5951 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5952 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5953 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5954 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5955 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5956 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5957 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5958 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5959 | `			"TypeError",` |
|       - | 5960 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5961 | `			ph7_type_name(apArg[1])` |
|       - | 5962 | `			);` |
|       - | 5963 | `	}` |
|       - | 5964 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5965 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5966 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5967 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5968 | `		int len;` |
|       3 | 5969 | `		sxu8 bReal = FALSE;` |
|       3 | 5970 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5971 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5972 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5973 | `				"TypeError",` |
|       - | 5974 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5975 | `				);` |
|       - | 5976 | `		}` |
|     ! 0 | 5977 | `		if( bReal ){` |
|       - | 5978 | `			/* float-string -> warn but allow */` |
|     ! 0 | 5979 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5980 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 5981 | `				zStr` |
|       - | 5982 | `				);` |
|     ! 0 | 5983 | `		}` |
|     ! 0 | 5984 | `	}` |
|       - | 5985 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 5986 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 5987 | `	 * later via ph7_value_to_int. */` |
|      38 | 5988 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5989 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5990 | `		sxi64 i = (sxi64)d;` |
|       3 | 5991 | `		if( d != (double)i ){` |
|       4 | 5992 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5993 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 5994 | `				d` |
|       - | 5995 | `				);` |
|       1 | 5996 | `		}` |
|       1 | 5997 | `	}` |
|       - | 5998 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5999 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6000 | `	{` |
|      38 | 6001 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 6002 | `		if( nSizeSigned < 1 ){` |
|       - | 6003 | `			/* size <= 0 -> ValueError */` |
|       5 | 6004 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6005 | `				"ValueError",` |
|       - | 6006 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6007 | `				);` |
|       - | 6008 | `		}` |
|      34 | 6009 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6010 | `	}` |
|      34 | 6011 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6012 | `		/* Return the whole array */` |
|       3 | 6013 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6014 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6015 | `		return PH7_OK;` |
|       - | 6016 | `	}` |
|      32 | 6017 | `	bPreserve = 0;` |
|      32 | 6018 | `	if( nArg > 2 ){` |
|       - | 6019 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6020 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6021 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6022 | `		 * normally, matching PHP behaviour. */` |
|      45 | 6023 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 6024 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6025 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 6026 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6027 | `				"TypeError",` |
|       - | 6028 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6029 | `				ph7_type_name(apArg[2])` |
|       - | 6030 | `				);` |
|       - | 6031 | `		}` |
|      21 | 6032 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6033 | `	}` |
|       - | 6034 | `	/* Start processing */` |
|      27 | 6035 | `	pEntry = pMap->pFirst;` |
|      27 | 6036 | `	nChunk = 0;` |
|      27 | 6037 | `	pChunk = 0;` |
|      27 | 6038 | `	n = pMap->nEntry;` |
|      56 | 6039 | `	for( ;; ){` |
|     113 | 6040 | `		if( n < 1 ){` |
|       - | 6041 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6042 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6043 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6044 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6045 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6046 | `			 * exists. */` |
|      27 | 6047 | `			if( pChunk ){` |
|      27 | 6048 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6049 | `			}` |
|      27 | 6050 | `			break;` |
|       - | 6051 | `		}` |
|      87 | 6052 | `		if( nChunk < 1 ){` |
|      71 | 6053 | `			if( pChunk ){` |
|       - | 6054 | `				/* Put the first chunk */` |
|      45 | 6055 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6056 | `			}` |
|       - | 6057 | `			/* Create a new dimension */` |
|      71 | 6058 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6059 | `												   * will be automatically released as soon we return` |
|       - | 6060 | `												   * from this function */` |
|      71 | 6061 | `			if( pChunk == 0 ){` |
|     ! 0 | 6062 | `				break;` |
|       - | 6063 | `			}` |
|      71 | 6064 | `			nChunk = nSize;` |
|      35 | 6065 | `		}` |
|       - | 6066 | `		/* Insert the entry */` |
|      87 | 6067 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6068 | `		/* Point to the next entry */` |
|      87 | 6069 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6070 | `		nChunk--;` |
|      87 | 6071 | `		n--;` |
|       1 | 6072 | `	}` |
|       - | 6073 | `	/* Return the multidimensional array */` |
|      27 | 6074 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6075 | `	return PH7_OK;` |
|      23 | 6076 |  |
|       - | 6077 | `/*` |
|       - | 6078 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6079 | ` *  Pad array to the specified length with a value.` |
|       - | 6080 | ` * $input` |
|       - | 6081 | ` *   Initial array of values to pad.` |
|       - | 6082 | ` * $pad_size` |
|       - | 6083 | ` *   New size of the array.` |
|       - | 6084 | ` * $pad_value` |
|       - | 6085 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6086 | ` */` |
|      28 | 6087 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6088 |  |
|       - | 6089 | `	ph7_hashmap *pMap;` |
|       - | 6090 | `	ph7_value *pArray;` |
|       - | 6091 | `	int nEntry;` |
|      30 | 6092 | `	if( nArg != 3 ){` |
|      10 | 6093 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6094 | `			"ArgumentCountError",` |
|       - | 6095 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6096 | `			nArg` |
|       - | 6097 | `			);` |
|       - | 6098 | `	}` |
|      24 | 6099 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6100 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6101 | `			"TypeError",` |
|       - | 6102 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6103 | `			ph7_type_name(apArg[0])` |
|       - | 6104 | `			);` |
|       - | 6105 | `	}` |
|       - | 6106 | `	/* Create a new array */` |
|      21 | 6107 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6108 | `	if( pArray == 0 ){` |
|     ! 0 | 6109 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6110 | `		return PH7_OK;` |
|       - | 6111 | `	}` |
|       - | 6112 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6113 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6114 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6115 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6116 | `	if( nEntry < 0 ){` |
|       9 | 6117 | `		nEntry = -nEntry;` |
|       9 | 6118 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6119 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6120 | `			/* Insert given items first */` |
|      17 | 6121 | `			while( nEntry > 0 ){` |
|      13 | 6122 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 6123 | `				nEntry--;` |
|       1 | 6124 | `			}` |
|       - | 6125 | `			/* Merge the two arrays */` |
|       5 | 6126 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6127 | `		}else{` |
|       5 | 6128 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6129 | `		}` |
|      17 | 6130 | `	}else if( nEntry > 0 ){` |
|      11 | 6131 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6132 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6133 | `			/* Merge the two arrays first */` |
|       7 | 6134 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6135 | `			/* Insert given items */` |
|      25 | 6136 | `			while( nEntry > 0 ){` |
|      19 | 6137 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 6138 | `				nEntry--;` |
|       1 | 6139 | `			}` |
|       4 | 6140 | `		}else{` |
|       5 | 6141 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6142 | `		}` |
|       6 | 6143 | `	}else{` |
|       - | 6144 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6145 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6146 | `	}` |
|       - | 6147 | `	/* Return the new array */` |
|      21 | 6148 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6149 | `	return PH7_OK;` |
|      16 | 6150 |  |
|       - | 6151 | `/*` |
|       - | 6152 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6153 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6154 | ` * Parameters` |
|       - | 6155 | ` * $array` |
|       - | 6156 | ` *   The array in which elements are replaced.` |
|       - | 6157 | ` * $array1` |
|       - | 6158 | ` *   The array from which elements will be extracted.` |
|       - | 6159 | ` * ....` |
|       - | 6160 | ` *  More arrays from which elements will be extracted.` |
|       - | 6161 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6162 | ` * Return` |
|       - | 6163 | ` *  Returns an array.` |
|       - | 6164 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6165 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6166 | ` */` |
|      22 | 6167 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6168 |  |
|       - | 6169 | `	ph7_hashmap *pMap;` |
|       - | 6170 | `	ph7_value *pArray;` |
|       - | 6171 | `	int i;` |
|      24 | 6172 | `	if( nArg < 1 ){` |
|       3 | 6173 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6174 | `			"ArgumentCountError",` |
|       - | 6175 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6176 | `			);` |
|       - | 6177 | `	}` |
|      22 | 6178 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6179 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6180 | `			"TypeError",` |
|       - | 6181 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6182 | `			ph7_type_name(apArg[0])` |
|       - | 6183 | `			);` |
|       - | 6184 | `	}` |
|       - | 6185 | `	/* Create a new array */` |
|      20 | 6186 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6187 | `	if( pArray == 0 ){` |
|     ! 0 | 6188 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6189 | `		return PH7_OK;` |
|       - | 6190 | `	}` |
|       - | 6191 | `	/* Overwrite from the first array */` |
|      20 | 6192 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6193 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6194 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6195 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6196 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6197 | `			/* Type mismatch -> TypeError */` |
|       4 | 6198 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6199 | `				"TypeError",` |
|       - | 6200 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6201 | `				i + 1,` |
|       2 | 6202 | `				ph7_type_name(apArg[i])` |
|       - | 6203 | `				);` |
|       - | 6204 | `		}` |
|       - | 6205 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6206 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6207 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6208 | `	}` |
|       - | 6209 | `	/* Return the new array */` |
|      17 | 6210 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6211 | `	return PH7_OK;` |
|      13 | 6212 |  |
|       - | 6213 | `/*` |
|       - | 6214 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6215 | ` *  Filters elements of an array using a callback function.` |
|       - | 6216 | ` * Parameters` |
|       - | 6217 | ` *  $input` |
|       - | 6218 | ` *    The array to iterate over` |
|       - | 6219 | ` * $callback` |
|       - | 6220 | ` *    The callback function to use` |
|       - | 6221 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6222 | ` *    will be removed.` |
|       - | 6223 | ` * Return` |
|       - | 6224 | ` *  The filtered array.` |
|       - | 6225 | ` */` |
|      18 | 6226 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6227 |  |
|       - | 6228 | `	ph7_hashmap_node *pEntry;` |
|       - | 6229 | `	ph7_hashmap *pMap;` |
|       - | 6230 | `	ph7_value *pArray;` |
|       - | 6231 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6232 | `	ph7_value *pValue;` |
|       - | 6233 | `	sxi32 rc;` |
|       - | 6234 | `	int keep;` |
|       - | 6235 | `	sxu32 n;` |
|      20 | 6236 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6237 | `		/* Invalid arguments,return NULL */` |
|       5 | 6238 | `		ph7_result_null(pCtx);` |
|       5 | 6239 | `		return PH7_OK;` |
|       - | 6240 | `	}` |
|       - | 6241 | `	/* Create a new array */` |
|      16 | 6242 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 6243 | `	if( pArray == 0 ){` |
|     ! 0 | 6244 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6245 | `		return PH7_OK;` |
|       - | 6246 | `	}` |
|       - | 6247 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 6248 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 6249 | `	pEntry = pMap->pFirst;` |
|      16 | 6250 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 6251 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6252 | `	/* Perform the requested operation */` |
|      66 | 6253 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6254 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 6255 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 6256 | `		if( pValue == 0 ){` |
|       - | 6257 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6258 | `			keep = FALSE;` |
|      54 | 6259 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6260 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6261 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6262 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 6263 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6264 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6265 | `					int len;` |
|       3 | 6266 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6267 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6268 | `						"TypeError",` |
|       - | 6269 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6270 | `						zName` |
|       - | 6271 | `						);` |
|     ! 0 | 6272 | `				}else{` |
|     ! 0 | 6273 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6274 | `						"TypeError",` |
|       - | 6275 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6276 | `						ph7_type_name(apArg[1])` |
|       - | 6277 | `						);` |
|       - | 6278 | `				}` |
|       - | 6279 | `			}` |
|      23 | 6280 | `			keep = FALSE;` |
|      23 | 6281 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 6282 | `			if( rc == SXRET_OK ){` |
|       - | 6283 | `				/* Perform a boolean cast */` |
|      23 | 6284 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6285 | `			}` |
|      23 | 6286 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6287 | `		}else{` |
|       - | 6288 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6289 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6290 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6291 | `			 */` |
|      29 | 6292 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6293 | `		}` |
|      51 | 6294 | `		if( keep ){` |
|       - | 6295 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6296 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6297 | `		}` |
|       - | 6298 | `		/* Point to the next entry */` |
|      51 | 6299 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6300 | `	}` |
|      13 | 6301 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6302 | `	return PH7_OK;` |
|      11 | 6303 |  |
|       - | 6304 | `/*` |
|       - | 6305 | ` * array array_map(?callable $callback, array $array)` |
|       - | 6306 | ` *  Applies the callback to the elements of the given array.` |
|       - | 6307 | ` * Parameters` |
|       - | 6308 | ` *  $callback` |
|       - | 6309 | ` *   A callable to run for each element in the array, or NULL for the` |
|       - | 6310 | ` *   identity function (returns the array unchanged).` |
|       - | 6311 | ` *  $array` |
|       - | 6312 | ` *   An array to run through the callback function.` |
|       - | 6313 | ` * Return` |
|       - | 6314 | ` *  Returns an array containing the results of applying the callback` |
|       - | 6315 | ` *  function to each element of $array.` |
|       - | 6316 | ` */` |
|      28 | 6317 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6318 |  |
|       - | 6319 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6320 | `	ph7_hashmap_node *pEntry;` |
|       - | 6321 | `	ph7_hashmap *pMap;` |
|       - | 6322 | `	int bNullCallback;` |
|       - | 6323 | `	sxu32 n;` |
|      30 | 6324 | `	if( nArg < 2 ){` |
|       7 | 6325 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6326 | `			"ArgumentCountError",` |
|       - | 6327 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6328 | `			nArg` |
|       - | 6329 | `			);` |
|       - | 6330 | `	}` |
|      26 | 6331 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 6332 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6333 | `			"TypeError",` |
|       - | 6334 | `			"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6335 | `			ph7_type_name(apArg[1])` |
|       - | 6336 | `			);` |
|       - | 6337 | `	}` |
|      24 | 6338 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      24 | 6339 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6340 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6341 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6342 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6343 | `				"TypeError",` |
|       - | 6344 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6345 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6346 | `				zFunc` |
|       - | 6347 | `				);` |
|       - | 6348 | `		}` |
|       3 | 6349 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6350 | `			"TypeError",` |
|       - | 6351 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6352 | `			"no array or string given"` |
|       - | 6353 | `			);` |
|       - | 6354 | `	}` |
|       - | 6355 | `	/* Create a new array */` |
|      19 | 6356 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 6357 | `	if( pArray == 0 ){` |
|     ! 0 | 6358 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6359 | `		return PH7_OK;` |
|       - | 6360 | `	}` |
|       - | 6361 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6362 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      19 | 6363 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 6364 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6365 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 6366 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 6367 | `	/* Perform the requested operation */` |
|      19 | 6368 | `	pEntry = pMap->pFirst;` |
|      53 | 6369 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6370 | `		/* Extract the node value */` |
|      35 | 6371 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      35 | 6372 | `		if( pValue ){` |
|       - | 6373 | `			/* Extract the node key */` |
|      35 | 6374 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      35 | 6375 | `			if( bNullCallback ){` |
|       - | 6376 | `				/* NULL callback: identity function, keep original value */` |
|      11 | 6377 | `				ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6378 | `			}else{` |
|       - | 6379 | `				/* Invoke the supplied callback */` |
|      25 | 6380 | `				PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 6381 | `				/* Insert the callback return value */` |
|      25 | 6382 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6383 | `			}` |
|      35 | 6384 | `			PH7_MemObjRelease(&sKey);` |
|      35 | 6385 | `			PH7_MemObjRelease(&sResult);` |
|      17 | 6386 | `		}` |
|       - | 6387 | `		/* Point to the next entry */` |
|      35 | 6388 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      18 | 6389 | `	}` |
|      19 | 6390 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 6391 | `	return PH7_OK;` |
|      16 | 6392 |  |
|       - | 6393 | `/*` |
|       - | 6394 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6395 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6396 | ` * Parameters` |
|       - | 6397 | ` *  $array` |
|       - | 6398 | ` *   The input array.` |
|       - | 6399 | ` *  $callback` |
|       - | 6400 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6401 | ` *  $initial` |
|       - | 6402 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6403 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6404 | ` * Return` |
|       - | 6405 | ` *  Returns the resulting value.` |
|       - | 6406 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6407 | ` */` |
|      30 | 6408 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6409 |  |
|       - | 6410 | `	ph7_hashmap_node *pEntry;` |
|       - | 6411 | `	ph7_hashmap *pMap;` |
|       - | 6412 | `	ph7_value *pValue;` |
|       - | 6413 | `	ph7_value sResult;` |
|       - | 6414 | `	sxu32 n;` |
|      32 | 6415 | `	if( nArg < 2 ){` |
|       7 | 6416 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6417 | `			"ArgumentCountError",` |
|       - | 6418 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6419 | `			nArg` |
|       - | 6420 | `			);` |
|       - | 6421 | `	}` |
|      28 | 6422 | `	if( nArg > 3 ){` |
|       4 | 6423 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6424 | `			"ArgumentCountError",` |
|       - | 6425 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6426 | `			nArg` |
|       - | 6427 | `			);` |
|       - | 6428 | `	}` |
|      26 | 6429 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6430 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6431 | `			"TypeError",` |
|       - | 6432 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6433 | `			ph7_type_name(apArg[0])` |
|       - | 6434 | `			);` |
|       - | 6435 | `	}` |
|      24 | 6436 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6437 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6438 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6439 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6440 | `				"TypeError",` |
|       - | 6441 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6442 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6443 | `				zFunc` |
|       - | 6444 | `				);` |
|       - | 6445 | `		}` |
|       7 | 6446 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6447 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6448 | `				"TypeError",` |
|       - | 6449 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6450 | `				"array callback must have exactly two members"` |
|       - | 6451 | `				);` |
|       - | 6452 | `		}` |
|       5 | 6453 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6454 | `			"TypeError",` |
|       - | 6455 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6456 | `			"no array or string given"` |
|       - | 6457 | `			);` |
|       - | 6458 | `	}` |
|       - | 6459 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 6460 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6461 | `	/* Assume a NULL initial value */` |
|      15 | 6462 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      15 | 6463 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      15 | 6464 | `	if( nArg > 2 ){` |
|       - | 6465 | `		/* Set the initial value */` |
|      11 | 6466 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6467 | `	}` |
|       - | 6468 | `	/* Perform the requested operation */` |
|      15 | 6469 | `	pEntry = pMap->pFirst;` |
|      43 | 6470 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6471 | `		/* Extract the node value */` |
|      29 | 6472 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6473 | `		/* Invoke the supplied callback */` |
|      29 | 6474 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6475 | `		/* Point to the next entry */` |
|      29 | 6476 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6477 | `	}` |
|      15 | 6478 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6479 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6480 | `	return PH7_OK;` |
|      17 | 6481 |  |
|       - | 6482 | `/*` |
|       - | 6483 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6484 | ` *  Apply a user function to every member of an array.` |
|       - | 6485 | ` * Parameters` |
|       - | 6486 | ` *  $array` |
|       - | 6487 | ` *   The input array.` |
|       - | 6488 | ` *  $funcname` |
|       - | 6489 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6490 | ` *   the first, and the key/index second.` |
|       - | 6491 | ` * Note:` |
|       - | 6492 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6493 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6494 | ` *  be made in the original array itself.` |
|       - | 6495 | ` *  $userdata` |
|       - | 6496 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6497 | ` *   to the callback funcname.` |
|       - | 6498 | ` * Return` |
|       - | 6499 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6500 | ` */` |
|      36 | 6501 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6502 |  |
|       - | 6503 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6504 | `	ph7_hashmap_node *pEntry;` |
|       - | 6505 | `	ph7_hashmap *pMap;` |
|       - | 6506 | `	sxu32 n;` |
|      38 | 6507 | `	if( nArg < 2 ){` |
|       7 | 6508 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6509 | `			"ArgumentCountError",` |
|       - | 6510 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6511 | `			nArg` |
|       - | 6512 | `			);` |
|       - | 6513 | `	}` |
|      34 | 6514 | `	if( nArg > 3 ){` |
|       4 | 6515 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6516 | `			"ArgumentCountError",` |
|       - | 6517 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6518 | `			nArg` |
|       - | 6519 | `			);` |
|       - | 6520 | `	}` |
|      32 | 6521 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6522 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6523 | `			"TypeError",` |
|       - | 6524 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6525 | `			ph7_type_name(apArg[0])` |
|       - | 6526 | `			);` |
|       - | 6527 | `	}` |
|      30 | 6528 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6529 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6530 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6531 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6532 | `				"TypeError",` |
|       - | 6533 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6534 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6535 | `				zFunc` |
|       - | 6536 | `				);` |
|       - | 6537 | `		}` |
|       9 | 6538 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6539 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6540 | `				"TypeError",` |
|       - | 6541 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6542 | `				"array callback must have exactly two members"` |
|       - | 6543 | `				);` |
|       - | 6544 | `		}` |
|       5 | 6545 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6546 | `			"TypeError",` |
|       - | 6547 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6548 | `			"no array or string given"` |
|       - | 6549 | `			);` |
|       - | 6550 | `	}` |
|      19 | 6551 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6552 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6553 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      19 | 6554 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6555 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6556 | `	/* Perform the desired operation */` |
|      19 | 6557 | `	pEntry = pMap->pFirst;` |
|      59 | 6558 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6559 | `		/* Extract the node value */` |
|      41 | 6560 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      41 | 6561 | `		if( pValue ){` |
|       - | 6562 | `			/* Extract the entry key */` |
|      41 | 6563 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6564 | `			/* Invoke the supplied callback */` |
|      41 | 6565 | `			PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      41 | 6566 | `			PH7_MemObjRelease(&sKey);` |
|      20 | 6567 | `		}` |
|       - | 6568 | `		/* Point to the next entry */` |
|      41 | 6569 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6570 | `	}` |
|       - | 6571 | `	/* All done, return TRUE */` |
|      19 | 6572 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6573 | `	return PH7_OK;` |
|      20 | 6574 |  |
|       - | 6575 | `/*` |
|       - | 6576 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6577 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6578 | ` */` |
|      22 | 6579 | `static void HashmapWalkRecursive(` |
|       - | 6580 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6581 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6582 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6583 | `	int iNest             /* Nesting level */` |
|       - | 6584 | `	)` |
|       1 | 6585 |  |
|       - | 6586 | `	ph7_hashmap_node *pEntry;` |
|       - | 6587 | `	ph7_value *pValue,sKey;` |
|       - | 6588 | `	sxu32 n;` |
|       - | 6589 | `	/* Iterate through hashmap entries */` |
|      23 | 6590 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6591 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6592 | `	pEntry = pMap->pFirst;` |
|      59 | 6593 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6594 | `		/* Extract the node value */` |
|      37 | 6595 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6596 | `		if( pValue ){` |
|      37 | 6597 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6598 | `				if( iNest < 32 ){` |
|       - | 6599 | `					/* Recurse */` |
|      11 | 6600 | `					iNest++;` |
|      11 | 6601 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6602 | `					iNest--;` |
|       5 | 6603 | `				}` |
|       6 | 6604 | `			}else{` |
|       - | 6605 | `				/* Extract the node key */` |
|      27 | 6606 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6607 | `				/* Invoke the supplied callback */` |
|      27 | 6608 | `				PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6609 | `				PH7_MemObjRelease(&sKey);` |
|       - | 6610 | `			}` |
|      18 | 6611 | `		}` |
|       - | 6612 | `		/* Point to the next entry */` |
|      37 | 6613 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6614 | `	}` |
|      23 | 6615 |  |
|       - | 6616 | `/*` |
|       - | 6617 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6618 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6619 | ` * Parameters` |
|       - | 6620 | ` *  $array` |
|       - | 6621 | ` *   The input array.` |
|       - | 6622 | ` *  $funcname` |
|       - | 6623 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6624 | ` *   the first, and the key/index second.` |
|       - | 6625 | ` * Note:` |
|       - | 6626 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6627 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6628 | ` *  be made in the original array itself.` |
|       - | 6629 | ` *  $userdata` |
|       - | 6630 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6631 | ` *   to the callback funcname.` |
|       - | 6632 | ` * Return` |
|       - | 6633 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6634 | ` */` |
|      30 | 6635 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6636 |  |
|       - | 6637 | `	ph7_hashmap *pMap;` |
|      32 | 6638 | `	if( nArg < 2 ){` |
|       7 | 6639 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6640 | `			"ArgumentCountError",` |
|       - | 6641 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 6642 | `			nArg` |
|       - | 6643 | `			);` |
|       - | 6644 | `	}` |
|      28 | 6645 | `	if( nArg > 3 ){` |
|       4 | 6646 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6647 | `			"ArgumentCountError",` |
|       - | 6648 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 6649 | `			nArg` |
|       - | 6650 | `			);` |
|       - | 6651 | `	}` |
|      26 | 6652 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6653 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6654 | `			"TypeError",` |
|       - | 6655 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6656 | `			ph7_type_name(apArg[0])` |
|       - | 6657 | `			);` |
|       - | 6658 | `	}` |
|      24 | 6659 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6660 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6661 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6662 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6663 | `				"TypeError",` |
|       - | 6664 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6665 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6666 | `				zFunc` |
|       - | 6667 | `				);` |
|       - | 6668 | `		}` |
|       9 | 6669 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6670 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6671 | `				"TypeError",` |
|       - | 6672 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6673 | `				"array callback must have exactly two members"` |
|       - | 6674 | `				);` |
|       - | 6675 | `		}` |
|       5 | 6676 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6677 | `			"TypeError",` |
|       - | 6678 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6679 | `			"no array or string given"` |
|       - | 6680 | `			);` |
|       - | 6681 | `	}` |
|       - | 6682 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6683 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6684 | `	/* Perform the desired operation */` |
|      13 | 6685 | `	HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6686 | `	/* All done, return TRUE */` |
|      13 | 6687 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6688 | `	return PH7_OK;` |
|      17 | 6689 |  |
|       - | 6690 | `/*` |
|       - | 6691 | ` * Table of hashmap functions.` |
|       - | 6692 | ` */` |
|       - | 6693 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6694 | `	{"count",             ph7_hashmap_count },` |
|       - | 6695 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6696 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6697 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6698 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6699 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6700 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6701 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6702 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6703 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6704 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6705 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6706 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6707 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6708 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6709 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6710 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6711 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6712 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6713 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6714 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6715 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6716 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6717 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6718 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6719 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6720 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6721 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6722 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6723 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6724 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6725 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6726 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6727 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6728 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6729 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6730 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6731 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6732 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6733 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6734 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6735 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6736 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6737 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6738 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6739 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6740 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6741 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6742 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6743 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6744 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6745 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6746 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6747 | `	{"current",           ph7_hashmap_current },` |
|       - | 6748 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6749 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6750 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6751 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6752 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6753 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6754 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6755 | `};` |
|       - | 6756 | `/*` |
|       - | 6757 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6758 | ` */` |
|    1834 | 6759 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6760 |  |
|       - | 6761 | `	sxu32 n;` |
|  113710 | 6762 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  111876 | 6763 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   55939 | 6764 | `	}` |
|    1836 | 6765 |  |
|       - | 6766 | `/*` |
|       - | 6767 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6768 | ` * the BLOB given as the first argument.` |
|       - | 6769 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6770 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6771 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6772 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6773 | ` */` |
|      26 | 6774 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6775 |  |
|       - | 6776 | `	ph7_hashmap_node *pEntry;` |
|       - | 6777 | `	ph7_value *pObj;` |
|      28 | 6778 | `	sxu32 n = 0;` |
|       - | 6779 | `	int isRef;` |
|       - | 6780 | `	sxi32 rc;` |
|       - | 6781 | `	int i;` |
|      28 | 6782 | `	if( nDepth > 31 ){` |
|       - | 6783 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6784 | `		/* Nesting limit reached */` |
|     ! 0 | 6785 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6786 | `		if( ShowType ){` |
|     ! 0 | 6787 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6788 | `		}` |
|     ! 0 | 6789 | `		return SXERR_LIMIT;` |
|       - | 6790 | `	}` |
|       - | 6791 | `	/* Point to the first inserted entry */` |
|      28 | 6792 | `	pEntry = pMap->pFirst;` |
|      28 | 6793 | `	rc = SXRET_OK;` |
|      28 | 6794 | `	if( !ShowType ){` |
|      15 | 6795 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6796 | `	}` |
|       - | 6797 | `	/* Total entries */` |
|      28 | 6798 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6799 | `#ifdef __WINNT__` |
|       2 | 6800 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6801 | `#else` |
|      26 | 6802 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6803 | `#endif` |
|      62 | 6804 | `	for(;;){` |
|     126 | 6805 | `		if( n >= pMap->nEntry ){` |
|      28 | 6806 | `			break;` |
|       - | 6807 | `		}` |
|     198 | 6808 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 6809 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 6810 | `		}` |
|       - | 6811 | `		/* Dump key */` |
|     100 | 6812 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 6813 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 6814 | `		}else{` |
|     101 | 6815 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6816 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6817 | `		}` |
|       - | 6818 | `#ifdef __WINNT__` |
|       2 | 6819 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6820 | `#else` |
|      98 | 6821 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6822 | `#endif` |
|       - | 6823 | `		/* Dump node value */` |
|     100 | 6824 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 6825 | `		isRef = 0;` |
|     100 | 6826 | `		if( pObj ){` |
|     100 | 6827 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6828 | `				/* Referenced object */` |
|     ! 0 | 6829 | `				isRef = 1;` |
|     ! 0 | 6830 | `			}` |
|     100 | 6831 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 6832 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6833 | `				break;` |
|       - | 6834 | `			}` |
|      49 | 6835 | `		}` |
|       - | 6836 | `		/* Point to the next entry */` |
|     100 | 6837 | `		n++;` |
|     100 | 6838 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6839 | `	}` |
|      54 | 6840 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 6841 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 6842 | `	}` |
|      28 | 6843 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 6844 | `	return rc;` |
|      15 | 6845 |  |
|       - | 6846 | `/*` |
|       - | 6847 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6848 | ` * retrieved entry.` |
|       - | 6849 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6850 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6851 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6852 | ` * a value different from PH7_OK.` |
|       - | 6853 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6854 | ` */` |
|   21998 | 6855 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6856 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6857 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6858 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6859 | `	)` |
|       2 | 6860 |  |
|       - | 6861 | `	ph7_hashmap_node *pEntry;` |
|       - | 6862 | `	ph7_value sKey,sValue;` |
|       - | 6863 | `	sxi32 rc;` |
|       - | 6864 | `	sxu32 n;` |
|       - | 6865 | `	/* Initialize walker parameter */` |
|   22000 | 6866 | `	rc = SXRET_OK;` |
|   22000 | 6867 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   22000 | 6868 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   22000 | 6869 | `	n = pMap->nEntry;` |
|   22000 | 6870 | `	pEntry = pMap->pFirst;` |
|       - | 6871 | `	/* Start the iteration process */` |
|   55955 | 6872 | `	for(;;){` |
|  111912 | 6873 | `		if( n < 1 ){` |
|   22000 | 6874 | `			break;` |
|       - | 6875 | `		}` |
|       - | 6876 | `		/* Extract a copy of the key and a copy the current value */` |
|   89914 | 6877 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   89914 | 6878 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6879 | `		/* Invoke the user callback */` |
|   89914 | 6880 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6881 | `		/* Release the copy of the key and the value */` |
|   89914 | 6882 | `		PH7_MemObjRelease(&sKey);` |
|   89914 | 6883 | `		PH7_MemObjRelease(&sValue);` |
|   89914 | 6884 | `		if( rc != PH7_OK ){` |
|       - | 6885 | `			/* Callback request an operation abort */` |
|     ! 0 | 6886 | `			return SXERR_ABORT;` |
|       - | 6887 | `		}` |
|       - | 6888 | `		/* Point to the next entry */` |
|   89914 | 6889 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   89914 | 6890 | `		n--;` |
|       2 | 6891 | `	}` |
|       - | 6892 | `	/* All done */` |
|   22000 | 6893 | `	return SXRET_OK;` |
|   11001 | 6894 |  |
|       - | 6895 |  |
