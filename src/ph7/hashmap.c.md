# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3229/3712 lines (86.99%)

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
| 3038520 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 3038522 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  315810 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  315812 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  315812 |   29 | `	sxu32 nH = 5381;` |
|  315812 |   30 | `	zEnd = &zIn[nLen];` |
|  351206 |   31 | `	for(;;){` |
|  702414 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  614380 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  551280 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  456134 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  315812 |   37 | `	return nH;` |
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
| 2979190 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 2979192 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2979192 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 2979192 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 2979192 |  104 | `	pNode->pMap  = &(*pMap);` |
| 2979192 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2979192 |  106 | `	pNode->nHash = nHash;` |
| 2979192 |  107 | `	pNode->xKey.iKey = iKey;` |
| 2979192 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 2979192 |  109 | `	return pNode;` |
| 1489597 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|  108458 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|  108460 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  108460 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|  108460 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|  108460 |  127 | `	pNode->pMap  = &(*pMap);` |
|  108460 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  108460 |  129 | `	pNode->nHash = nHash;` |
|  108460 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  108460 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  108460 |  132 | `	pNode->nValIdx = nValIdx;` |
|  108460 |  133 | `	return pNode;` |
|   54231 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 3087648 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  139 |  |
|       - |  140 | `	/* Link */` |
| 3087650 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2782092 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2782092 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1391045 |  144 | `	}` |
| 3087650 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 3087650 |  147 | `	if( pMap->pFirst == 0 ){` |
|   54896 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   54896 |  150 | `		pMap->pCur = pNode;` |
|   27449 |  151 | `	}else{` |
| 3032756 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 3087650 |  154 | `	++pMap->nEntry;` |
| 3087650 |  155 |  |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    6912 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  161 |  |
|    6914 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    6914 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    6914 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    6462 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3232 |  167 | `	}else{` |
|     453 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    6914 |  170 | `	if( pNode->pNextCollide ){` |
|    5491 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2745 |  172 | `	}` |
|    6914 |  173 | `	if( pMap->pFirst == pNode ){` |
|     100 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      49 |  175 | `	}` |
|    6914 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|     102 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      50 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    6914 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    6914 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     104 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     104 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     104 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  188 | `		}` |
|      51 |  189 | `	}` |
|    6914 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    6780 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3389 |  192 | `	}` |
|    6914 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    6914 |  194 | `	pMap->nEntry--;` |
|    6914 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      46 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      46 |  198 | `		pMap->apBucket = 0;` |
|      46 |  199 | `		pMap->nSize = 0;` |
|      46 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      22 |  201 | `	}` |
|    6914 |  202 |  |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 3087648 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  208 |  |
| 3087650 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   59256 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   59256 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   59256 |  215 | `		if( nNew < 1 ){` |
|   54896 |  216 | `			nNew = 16;` |
|   27447 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   59256 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   59256 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   59256 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   59256 |  230 | `		pMap->apBucket = apNew;` |
|   59256 |  231 | `		pMap->nSize = nNew;` |
|   59256 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   54896 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    4362 |  237 | `		pEntry = pMap->pFirst;` |
|    4362 |  238 | `		n = 0;` |
| 2040932 |  239 | `		for( ;; ){` |
| 4081866 |  240 | `			if( n >= pMap->nEntry ){` |
|    4362 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 4077506 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 4077506 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4077506 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3523820 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3523820 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1761909 |  250 | `			}` |
| 4077506 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 4077506 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4077506 |  254 | `			n++;` |
|       2 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    4362 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2180 |  258 | `	}` |
| 3032756 |  259 | `	return SXRET_OK;` |
| 1543826 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 2979190 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 2979192 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		/* Reserve a ph7_value for the value */` |
| 2979158 |  274 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2979158 |  275 | `		if( pObj == 0 ){` |
|     ! 0 |  276 | `			return SXERR_MEM;` |
|       - |  277 | `		}` |
| 2979158 |  278 | `		if( pValue ){` |
|       - |  279 | `			/* Duplicate the value */` |
| 2979158 |  280 | `			PH7_MemObjStore(pValue,pObj);` |
| 1489578 |  281 | `		}` |
| 2979158 |  282 | `		nIdx = pObj->nIdx;` |
| 1489580 |  283 | `	}else{` |
|      35 |  284 | `		nIdx = nRefIdx;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Hash the key */` |
| 2979192 |  287 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  288 | `	/* Allocate a new int node */` |
| 2979192 |  289 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2979192 |  290 | `	if( pNode == 0 ){` |
|     ! 0 |  291 | `		return SXERR_MEM;` |
|       - |  292 | `	}` |
| 2979192 |  293 | `	if( isForeign ){` |
|       - |  294 | `		/* Mark as a foregin entry */` |
|      35 |  295 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      17 |  296 | `	}` |
|       - |  297 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2979192 |  298 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2979192 |  299 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  300 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  301 | `		return rc;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Perform the insertion */` |
| 2979192 |  304 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  305 | `	/* Install in the reference table */` |
| 2979192 |  306 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  307 | `	/* All done */` |
| 2979192 |  308 | `	return SXRET_OK;` |
| 1489597 |  309 |  |
|       - |  310 | `/*` |
|       - |  311 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  312 | ` * hashmap.` |
|       - |  313 | ` */` |
|  108458 |  314 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  315 |  |
|       - |  316 | `	ph7_hashmap_node *pNode;` |
|       - |  317 | `	sxu32 nHash;` |
|       - |  318 | `	sxu32 nIdx;` |
|       - |  319 | `	sxi32 rc;` |
|  108460 |  320 | `	if( !isForeign ){` |
|       - |  321 | `		ph7_value *pObj;` |
|       - |  322 | `		/* Reserve a ph7_value for the value */` |
|   73190 |  323 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   73190 |  324 | `		if( pObj == 0 ){` |
|     ! 0 |  325 | `			return SXERR_MEM;` |
|       - |  326 | `		}` |
|   73190 |  327 | `		if( pValue ){` |
|       - |  328 | `			/* Duplicate the value */` |
|   72918 |  329 | `			PH7_MemObjStore(pValue,pObj);` |
|   36458 |  330 | `		}` |
|   73190 |  331 | `		nIdx = pObj->nIdx;` |
|   36596 |  332 | `	}else{` |
|   35272 |  333 | `		nIdx = nRefIdx;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Hash the key */` |
|  108460 |  336 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  337 | `	/* Allocate a new blob node */` |
|  108460 |  338 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  108460 |  339 | `	if( pNode == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|  108460 |  342 | `	if( isForeign ){` |
|       - |  343 | `		/* Mark as a foregin entry */` |
|   35272 |  344 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   17635 |  345 | `	}` |
|       - |  346 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  108460 |  347 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  108460 |  348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  349 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  350 | `		return rc;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Perform the insertion */` |
|  108460 |  353 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  354 | `	/* Install in the reference table */` |
|  108460 |  355 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  356 | `	/* All done */` |
|  108460 |  357 | `	return SXRET_OK;` |
|   54231 |  358 |  |
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
|  220890 |  405 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  406 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  407 | `	const void *pKey,           /* Lookup key */` |
|       - |  408 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  409 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  410 | `	)` |
|       2 |  411 |  |
|       - |  412 | `	ph7_hashmap_node *pNode;` |
|       - |  413 | `	sxu32 nHash;` |
|  220892 |  414 | `	if( pMap->nEntry < 1 ){` |
|       - |  415 | `		/* Don't bother hashing,there is no entry anyway */` |
|   13540 |  416 | `		return SXERR_NOTFOUND;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Hash the key first */` |
|  207354 |  419 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  420 | `	/* Point to the appropriate bucket */` |
|  207354 |  421 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  422 | `	/* Perform the lookup */` |
|  186916 |  423 | `	for(;;){` |
|  373834 |  424 | `		if( pNode == 0 ){` |
|  158258 |  425 | `			break;` |
|       - |  426 | `		}` |
|  240124 |  427 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  214077 |  428 | `			&& pNode->nHash == nHash` |
|  130837 |  429 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   49098 |  430 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  431 | `				/* Node found */` |
|   49098 |  432 | `				if( ppNode ){` |
|   49070 |  433 | `					*ppNode = pNode;` |
|   24534 |  434 | `				}` |
|   49098 |  435 | `				return SXRET_OK;` |
|       - |  436 | `		}` |
|       - |  437 | `		/* Follow the collision link */` |
|  166482 |  438 | `		pNode = pNode->pNextCollide;` |
|       2 |  439 | `	}` |
|       - |  440 | `	/* No such entry */` |
|  158258 |  441 | `	return SXERR_NOTFOUND;` |
|  110447 |  442 |  |
|       - |  443 | `/*` |
|       - |  444 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  445 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  446 | ` */` |
|  221030 |  447 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  448 |  |
|  221032 |  449 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  221032 |  450 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  221032 |  451 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  452 | `		/* Octal not decimal number */` |
|       5 |  453 | `		return FALSE;` |
|       - |  454 | `	}` |
|  221028 |  455 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  456 | `		zIn++;` |
|     ! 0 |  457 | `	}` |
|  110846 |  458 | `	for(;;){` |
|  221694 |  459 | `		if( zIn >= zEnd ){` |
|     233 |  460 | `			return TRUE;` |
|       - |  461 | `		}` |
|  221462 |  462 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  110399 |  463 | `			break;` |
|       - |  464 | `		}` |
|     667 |  465 | `		zIn++;` |
|       1 |  466 | `	}` |
|       - |  467 | `	/* Key does not look like a decimal number */` |
|  220796 |  468 | `	return FALSE;` |
|  110517 |  469 |  |
|       - |  470 | `/*` |
|       - |  471 | ` * Check if a given key exists in the given hashmap.` |
|       - |  472 | ` * Write a pointer to the target node on success.` |
|       - |  473 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  474 | ` */` |
|  113666 |  475 | `static sxi32 HashmapLookup(` |
|       - |  476 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  477 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  478 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  479 | `	)` |
|       2 |  480 |  |
|  113668 |  481 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  482 | `	sxi32 rc;` |
|  113668 |  483 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  112378 |  484 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  485 | `			/* Force a string cast */` |
|     ! 0 |  486 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  487 | `		}` |
|  112378 |  488 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  489 | `			/* Perform a blob lookup */` |
|  112362 |  490 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  112362 |  491 | `			goto result;` |
|       - |  492 | `		}` |
|       8 |  493 | `	}` |
|       - |  494 | `	/* Perform an int lookup */` |
|    1308 |  495 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  496 | `		/* Force an integer cast */` |
|      27 |  497 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  498 | `	}` |
|       - |  499 | `	/* Perform an int lookup */` |
|    1308 |  500 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   56833 |  501 | `result:` |
|  113668 |  502 | `	if( rc == SXRET_OK ){` |
|       - |  503 | `		/* Node found */` |
|   50262 |  504 | `		if( ppNode ){` |
|   50220 |  505 | `			*ppNode = pNode;` |
|   25109 |  506 | `		}` |
|   50262 |  507 | `		return SXRET_OK;` |
|       - |  508 | `	}` |
|       - |  509 | `	/* No such entry */` |
|   63408 |  510 | `	return SXERR_NOTFOUND;` |
|   56835 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  514 | ` * hashmap.` |
|       - |  515 | ` * If a node with the given key already exists in the database` |
|       - |  516 | ` * then this function overwrite the old value.` |
|       - |  517 | ` */` |
| 3052062 |  518 | `static sxi32 HashmapInsert(` |
|       - |  519 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  520 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  521 | `	ph7_value *pVal    /* Node value */` |
|       - |  522 | `	)` |
|       2 |  523 |  |
| 3052064 |  524 | `	ph7_hashmap_node *pNode = 0;` |
| 3052064 |  525 | `	sxi32 rc = SXRET_OK;` |
| 3052064 |  526 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   73418 |  527 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  528 | `			/* Force a string cast */` |
|       3 |  529 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  530 | `		}` |
|   73418 |  531 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  532 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  533 | `				/* Automatic index assign */` |
|      34 |  534 | `				pKey = 0;` |
|      16 |  535 | `			}` |
|     256 |  536 | `			goto IntKey;` |
|       - |  537 | `		}` |
|  109745 |  538 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   36581 |  539 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  540 | `				/* Overwrite the old value */` |
|       - |  541 | `				ph7_value *pElem;` |
|      72 |  542 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      72 |  543 | `				if( pElem ){` |
|      72 |  544 | `					if( pVal ){` |
|      72 |  545 | `						PH7_MemObjStore(pVal,pElem);` |
|      37 |  546 | `					}else{` |
|       - |  547 | `						/* Nullify the entry */` |
|     ! 0 |  548 | `						PH7_MemObjToNull(pElem);` |
|       - |  549 | `					}` |
|      35 |  550 | `				}` |
|      72 |  551 | `				return SXRET_OK;` |
|       - |  552 | `		}` |
|   73094 |  553 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  554 | `			/* Forbidden */` |
|       3 |  555 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Perform a blob-key insertion */` |
|   73092 |  559 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   73092 |  560 | `		return rc;` |
|       - |  561 | `	}` |
| 1489323 |  562 | `IntKey:` |
| 2978902 |  563 | `	if( pKey ){` |
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
| 2955444 |  600 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  601 | `			/* Forbidden */` |
|       3 |  602 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|       - |  605 | `		/* Assign an automatic index */` |
| 2955442 |  606 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2955442 |  607 | `		if( rc == SXRET_OK ){` |
| 2955442 |  608 | `			++pMap->iNextIdx;` |
| 1477720 |  609 | `		}` |
|       - |  610 | `	}` |
|       - |  611 | `	/* Insertion result */` |
| 2978812 |  612 | `	return rc;` |
| 1526033 |  613 |  |
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
|   35310 |  641 | `static sxi32 HashmapInsertByRef(` |
|       - |  642 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  643 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  644 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  645 | `	)` |
|       2 |  646 |  |
|   35312 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|   35312 |  648 | `	sxi32 rc = SXRET_OK;` |
|   35312 |  649 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   35278 |  650 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  651 | `			/* Force a string cast */` |
|     ! 0 |  652 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  653 | `		}` |
|   35278 |  654 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  655 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  656 | `				/* Automatic index assign */` |
|     ! 0 |  657 | `				pKey = 0;` |
|     ! 0 |  658 | `			}` |
|     ! 0 |  659 | `			goto IntKey;` |
|       - |  660 | `		}` |
|   52916 |  661 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   17638 |  662 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  663 | `				/* Overwrite */` |
|       7 |  664 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  665 | `				pNode->nValIdx = nRefIdx;` |
|       - |  666 | `				/* Install in the reference table */` |
|       7 |  667 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  668 | `				return SXRET_OK;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Perform a blob-key insertion */` |
|   35272 |  671 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   35272 |  672 | `		return rc;` |
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
|   17657 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * Extract node value.` |
|       - |  712 | ` */` |
| 1213957 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
| 1213959 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1213959 |  718 | `	return pObj;` |
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
|   60701 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   60703 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   60703 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   60703 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   60703 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   60703 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   60703 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   60703 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   60703 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   60703 |  783 | `	return rc;` |
|   30340 |  784 |  |
|       - |  785 | `/*` |
|       - |  786 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  787 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  788 | ` */` |
|   11828 |  789 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  790 |  |
|   11830 |  791 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  792 | `	sxu32 nBucket;` |
|       - |  793 | `	/* Remove old collision links */` |
|   11830 |  794 | `	if( pEntry->pPrevCollide ){` |
|    9574 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    4777 |  796 | `	}else{` |
|    2258 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  798 | `	}` |
|   11830 |  799 | `	if( pEntry->pNextCollide ){` |
|     903 |  800 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     445 |  801 | `	}` |
|   11830 |  802 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  803 | `	/* Compute the new hash */` |
|   11830 |  804 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   11830 |  805 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   11830 |  806 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  807 | `	/* Link to the new bucket */` |
|   11830 |  808 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11830 |  809 | `	if( pMap->apBucket[nBucket] ){` |
|    9824 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    4901 |  811 | `	}` |
|   11830 |  812 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11830 |  813 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  814 | `	/* Increment the automatic index */` |
|   11830 |  815 | `	pMap->iNextIdx++;` |
|   11830 |  816 |  |
|       - |  817 | `/*` |
|       - |  818 | ` * Perform a linear search on a given hashmap.` |
|       - |  819 | ` * Write a pointer to the target node on success.` |
|       - |  820 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  821 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  822 | ` * for more information.` |
|       - |  823 | ` */` |
|   29596 |  824 | `static int HashmapFindValue(` |
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
|   29598 |  837 | `	pEntry = pMap->pFirst;` |
|   29598 |  838 | `	n = pMap->nEntry;` |
|   29598 |  839 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   29598 |  840 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   70879 |  841 | `	for(;;){` |
|  141761 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  141663 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  141663 |  847 | `		if( pVal ){` |
|  141663 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  141663 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  141663 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  141663 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  141663 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  141663 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  141663 |  865 | `				if( rc == 0 ){` |
|   29500 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   29500 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   56081 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|  112165 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  112165 |  876 | `		n--;` |
|       2 |  877 | `	}` |
|       - |  878 | `	/* No such entry */` |
|      99 |  879 | `	return SXERR_NOTFOUND;` |
|   14800 |  880 |  |
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
|  567226 | 1066 | `static sxi32 HashmapDuplicateNode(` |
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
|  567228 | 1077 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
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
|  567222 | 1102 | `	sSafeVal = *pVal;` |
|       - | 1103 |  |
|  567222 | 1104 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1105 | `		/* Blob key insertion */` |
|      95 | 1106 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      95 | 1107 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      95 | 1108 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      95 | 1109 | `		PH7_MemObjRelease(&sKey);` |
|      48 | 1110 | `	}else{` |
|       - | 1111 | `		/* Int key */` |
|  567128 | 1112 | `		if( iAction == 0 ){ /* Merge */` |
|  566906 | 1113 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  283676 | 1114 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1115 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1116 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1117 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1118 | `		}else{ /* Dup */` |
|     194 | 1119 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1120 | `		}` |
|       - | 1121 | `	}` |
|  567222 | 1122 | `	return rc;` |
|  283615 | 1123 |  |
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
|  568988 | 1151 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1152 | `		/* Extract the node value */` |
|  566964 | 1153 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  566964 | 1154 | `		if( pVal ){` |
|       - | 1155 | `			/* Make a local copy of the value.` |
|       - | 1156 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1157 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1158 | `			 * to the old pool.` |
|       - | 1159 | `			 */` |
|  566964 | 1160 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  283483 | 1161 | `		}else{` |
|     ! 0 | 1162 | `			rc = SXRET_OK;` |
|       - | 1163 | `		}` |
|  566964 | 1164 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1165 | `			return rc;` |
|       - | 1166 | `		}` |
|       - | 1167 | `		/* Point to the next entry */` |
|  566964 | 1168 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  283483 | 1169 | `	}` |
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
|  195742 | 1258 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       2 | 1259 |  |
|  195744 | 1260 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1261 | `	ph7_hashmap *pNew;` |
|       - | 1262 | `	ph7_value *pBacking;` |
|  195744 | 1263 | `	if( pMap->iRef < 2 ){` |
|       - | 1264 | `		/* Sole owner, no separation needed */` |
|  193650 | 1265 | `		return pMap;` |
|       - | 1266 | `	}` |
|    2096 | 1267 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1268 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1269 | `		return pMap;` |
|       - | 1270 | `	}` |
|       - | 1271 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1272 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1273 | `	 * frame is popped. */` |
|    2096 | 1274 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2096 | 1275 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    3124 | 1276 | `		if( pBacking && pBacking != pValue` |
|    2077 | 1277 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2062 | 1278 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1279 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2062 | 1280 | `			pMap->iRef--;` |
|    2062 | 1281 | `			if( pMap->iRef < 2 ){` |
|       - | 1282 | `				/* After undoing stack ref, sole owner — no separation */` |
|    2026 | 1283 | `				pMap->iRef++;` |
|    2026 | 1284 | `				return pMap;` |
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
|      38 | 1299 | `			pBacking->x.pOther = pNew;` |
|       - | 1300 | `			/* Update the stack value to match */` |
|      38 | 1301 | `			pValue->x.pOther = pNew;` |
|      38 | 1302 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|      38 | 1303 | `			return pNew;` |
|       - | 1304 | `		}` |
|      17 | 1305 | `	}` |
|      35 | 1306 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      35 | 1307 | `	if( pNew == 0 ){` |
|       - | 1308 | `		/* Allocation failure — fall through with shared map */` |
|     ! 0 | 1309 | `		return pMap;` |
|       - | 1310 | `	}` |
|      35 | 1311 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1312 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|     ! 0 | 1313 | `		PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1314 | `		return pMap;` |
|       - | 1315 | `	}` |
|      35 | 1316 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      35 | 1317 | `	pMap->iRef--;` |
|      35 | 1318 | `	pValue->x.pOther = pNew;` |
|      35 | 1319 | `	return pNew;` |
|   97873 | 1320 |  |
|       - | 1321 | `/*` |
|       - | 1322 | ` * Perform the union of two hashmaps.` |
|       - | 1323 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1324 | ` * with a variable holding an array as follows:` |
|       - | 1325 | ` * <?php` |
|       - | 1326 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1327 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1328 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1329 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1330 | ` * var_dump($c);` |
|       - | 1331 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1332 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1333 | ` * var_dump($c);` |
|       - | 1334 | ` * ?>` |
|       - | 1335 | ` * When executed, this script will print the following:` |
|       - | 1336 | ` * Union of $a and $b:` |
|       - | 1337 | ` * array(3) {` |
|       - | 1338 | ` *  ["a"]=>` |
|       - | 1339 | ` *  string(5) "apple"` |
|       - | 1340 | ` *  ["b"]=>` |
|       - | 1341 | ` * string(6) "banana"` |
|       - | 1342 | ` *  ["c"]=>` |
|       - | 1343 | ` * string(6) "cherry"` |
|       - | 1344 | ` * }` |
|       - | 1345 | ` * Union of $b and $a:` |
|       - | 1346 | ` * array(3) {` |
|       - | 1347 | ` * ["a"]=>` |
|       - | 1348 | ` * string(4) "pear"` |
|       - | 1349 | ` * ["b"]=>` |
|       - | 1350 | ` * string(10) "strawberry"` |
|       - | 1351 | ` * ["c"]=>` |
|       - | 1352 | ` * string(6) "cherry"` |
|       - | 1353 | ` * }` |
|       - | 1354 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1355 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1356 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1357 | ` */` |
|      10 | 1358 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       2 | 1359 |  |
|       - | 1360 | `	ph7_hashmap_node *pEntry;` |
|      12 | 1361 | `	sxi32 rc = SXRET_OK;` |
|       - | 1362 | `	ph7_value *pObj;` |
|       - | 1363 | `	sxu32 n;` |
|      12 | 1364 | `	if( pLeft == pRight ){` |
|       - | 1365 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1366 | `		 * Unlike the zend engine.` |
|       - | 1367 | `		 */` |
|     ! 0 | 1368 | `		return SXRET_OK;` |
|       - | 1369 | `	}` |
|       - | 1370 | `	/* Perform the union */` |
|      12 | 1371 | `	pEntry = pRight->pFirst;` |
|      32 | 1372 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1373 | `		/* Make sure the given key does not exists in the left array */` |
|      22 | 1374 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1375 | `			/* BLOB key */` |
|       7 | 1376 | `			if( SXRET_OK !=` |
|       6 | 1377 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|       3 | 1378 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|       3 | 1379 | `					if( pObj ){` |
|       3 | 1380 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1381 | `						/* Perform the insertion */` |
|       3 | 1382 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1383 | `							&sSafeVal,0,FALSE);` |
|       3 | 1384 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1385 | `							return rc;` |
|       - | 1386 | `						}` |
|       1 | 1387 | `					}` |
|       1 | 1388 | `			}` |
|       4 | 1389 | `		}else{` |
|       - | 1390 | `			/* INT key */` |
|      16 | 1391 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|      11 | 1392 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|      11 | 1393 | `				if( pObj ){` |
|      11 | 1394 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1395 | `					/* Perform the insertion */` |
|      11 | 1396 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      11 | 1397 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1398 | `						return rc;` |
|       - | 1399 | `					}` |
|       5 | 1400 | `				}` |
|       5 | 1401 | `			}` |
|       - | 1402 | `		}` |
|       - | 1403 | `		/* Point to the next entry */` |
|      22 | 1404 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      12 | 1405 | `	}` |
|      12 | 1406 | `	return SXRET_OK;` |
|       7 | 1407 |  |
|       - | 1408 | `/*` |
|       - | 1409 | ` * Allocate a new hashmap.` |
|       - | 1410 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1411 | ` */` |
|   86346 | 1412 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1413 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1414 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1415 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1416 | `	)` |
|       2 | 1417 |  |
|       - | 1418 | `	ph7_hashmap *pMap;` |
|       - | 1419 | `	/* Allocate a new instance */` |
|   86348 | 1420 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   86348 | 1421 | `	if( pMap == 0 ){` |
|     ! 0 | 1422 | `		return 0;` |
|       - | 1423 | `	}` |
|       - | 1424 | `	/* Zero the structure */` |
|   86348 | 1425 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1426 | `	/* Fill in the structure */` |
|   86348 | 1427 | `	pMap->pVm = &(*pVm);` |
|   86348 | 1428 | `	pMap->iRef = 1;` |
|       - | 1429 | `	/* Default hash functions */` |
|   86348 | 1430 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   86348 | 1431 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   86348 | 1432 | `	return pMap;` |
|   43175 | 1433 |  |
|       - | 1434 | `/*` |
|       - | 1435 | ` * Install superglobals in the given virtual machine.` |
|       - | 1436 | ` * Note on superglobals.` |
|       - | 1437 | ` *  According to the PHP language reference manual.` |
|       - | 1438 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1439 | `*   Description` |
|       - | 1440 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1441 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1442 | `*   global $variable; to access them within functions or methods.` |
|       - | 1443 | `*   These superglobal variables are:` |
|       - | 1444 | `*    $GLOBALS` |
|       - | 1445 | `*    $_SERVER` |
|       - | 1446 | `*    $_GET` |
|       - | 1447 | `*    $_POST` |
|       - | 1448 | `*    $_FILES` |
|       - | 1449 | `*    $_COOKIE` |
|       - | 1450 | `*    $_SESSION` |
|       - | 1451 | `*    $_REQUEST` |
|       - | 1452 | `*    $_ENV` |
|       - | 1453 | `*/` |
|    2820 | 1454 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       2 | 1455 |  |
|       - | 1456 | `	static const char * azSuper[] = {` |
|       - | 1457 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1458 | `		"_GET",      /* $_GET */` |
|       - | 1459 | `		"_POST",     /* $_POST */` |
|       - | 1460 | `		"_FILES",    /* $_FILES */` |
|       - | 1461 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1462 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1463 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1464 | `		"_ENV",      /* $_ENV */` |
|       - | 1465 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1466 | `		"argv"       /* $argv */` |
|       - | 1467 | `	};` |
|       - | 1468 | `	ph7_hashmap *pMap;` |
|       - | 1469 | `	ph7_value *pObj;` |
|       - | 1470 | `	SyString *pFile;` |
|       - | 1471 | `	sxi32 rc;` |
|       - | 1472 | `	sxu32 n;` |
|       - | 1473 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    2822 | 1474 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    2822 | 1475 | `	if( pMap == 0 ){` |
|     ! 0 | 1476 | `		return SXERR_MEM;` |
|       - | 1477 | `	}` |
|    2822 | 1478 | `	pVm->pGlobal = pMap;` |
|       - | 1479 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    2822 | 1480 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    2822 | 1481 | `	if( pObj == 0 ){` |
|     ! 0 | 1482 | `		return SXERR_MEM;` |
|       - | 1483 | `	}` |
|    2822 | 1484 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1485 | `	/* Record object index */` |
|    2822 | 1486 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1487 | `	/* Install the special $GLOBALS array */` |
|    2822 | 1488 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    2822 | 1489 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1490 | `		return rc;` |
|       - | 1491 | `	}` |
|       - | 1492 | `	/* Install superglobals now */` |
|   31022 | 1493 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1494 | `		ph7_value *pSuper;` |
|       - | 1495 | `		/* Request an empty array */` |
|   28202 | 1496 | `		pSuper = ph7_new_array(&(*pVm));` |
|   28202 | 1497 | `		if( pSuper == 0 ){` |
|     ! 0 | 1498 | `			return SXERR_MEM;` |
|       - | 1499 | `		}` |
|       - | 1500 | `		/* Install */` |
|   28202 | 1501 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   28202 | 1502 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1503 | `			return rc;` |
|       - | 1504 | `		}` |
|       - | 1505 | `		/* Release the value now it have been installed */` |
|   28202 | 1506 | `		ph7_release_value(&(*pVm),pSuper);` |
|   14102 | 1507 | `	}` |
|       - | 1508 | `	/* Set some $_SERVER entries */` |
|    2822 | 1509 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1510 | `	/*` |
|       - | 1511 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1512 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1513 | `	 */` |
|    5638 | 1514 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1515 | `		"SCRIPT_FILENAME",` |
|    1410 | 1516 | `		pFile ? pFile->zString : ":Memory:",` |
|    2816 | 1517 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1518 | `		);` |
|       - | 1519 | `	/* All done,all super-global are installed now */` |
|    2822 | 1520 | `	return SXRET_OK;` |
|    1412 | 1521 |  |
|       - | 1522 | `/*` |
|       - | 1523 | ` * Release a hashmap.` |
|       - | 1524 | ` */` |
|   55090 | 1525 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1526 |  |
|       - | 1527 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   55092 | 1528 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1529 | `	sxu32 n;` |
|   55092 | 1530 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1531 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1532 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1533 | `		return SXRET_OK;` |
|       - | 1534 | `	}` |
|       - | 1535 | `	/* Start the release process */` |
|   55092 | 1536 | `	n = 0;` |
|   55092 | 1537 | `	pEntry = pMap->pFirst;` |
| 1548545 | 1538 | `	for(;;){` |
| 3097092 | 1539 | `		if( n >= pMap->nEntry ){` |
|   55092 | 1540 | `			break;` |
|       - | 1541 | `		}` |
| 3042002 | 1542 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1543 | `		/* Remove the reference from the foreign table */` |
| 3042002 | 1544 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3042002 | 1545 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1546 | `			/* Restore the ph7_value to the free list */` |
| 3041994 | 1547 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1520996 | 1548 | `		}` |
|       - | 1549 | `		/* Release the node */` |
| 3042002 | 1550 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   69020 | 1551 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   34509 | 1552 | `		}` |
| 3042002 | 1553 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1554 | `		/* Point to the next entry */` |
| 3042002 | 1555 | `		pEntry = pNext;` |
| 3042002 | 1556 | `		n++;` |
|       2 | 1557 | `	}` |
|   55092 | 1558 | `	if( pMap->nEntry > 0 ){` |
|       - | 1559 | `		/* Release the hash bucket */` |
|   48954 | 1560 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   24476 | 1561 | `	}` |
|   55092 | 1562 | `	if( FreeDS ){` |
|       - | 1563 | `		/* Free the whole instance */` |
|   55076 | 1564 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   27539 | 1565 | `	}else{` |
|       - | 1566 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1567 | `		pMap->apBucket = 0;` |
|      17 | 1568 | `		pMap->iNextIdx = 0;` |
|      17 | 1569 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1570 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1571 | `	}` |
|   55092 | 1572 | `	return SXRET_OK;` |
|   27547 | 1573 |  |
|       - | 1574 | `/*` |
|       - | 1575 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1576 | ` * If the count reaches zero which mean no more variables` |
|       - | 1577 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1578 | ` */` |
|  604912 | 1579 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1580 |  |
|  604914 | 1581 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1582 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  604914 | 1583 | `	pMap->iRef--;` |
|  604914 | 1584 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   55060 | 1585 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   27529 | 1586 | `	}` |
|  604914 | 1587 |  |
|       - | 1588 | `/*` |
|       - | 1589 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1590 | ` * Write a pointer to the target node on success.` |
|       - | 1591 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1592 | ` */` |
|  113706 | 1593 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1594 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1595 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1596 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1597 | `	)` |
|       2 | 1598 |  |
|       - | 1599 | `	sxi32 rc;` |
|  113708 | 1600 | `	if( pMap->nEntry < 1 ){` |
|       - | 1601 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1602 | `		 */` |
|      42 | 1603 | `		return SXERR_NOTFOUND;` |
|       - | 1604 | `	}` |
|  113668 | 1605 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  113668 | 1606 | `	return rc;` |
|   56855 | 1607 |  |
|       - | 1608 | `/*` |
|       - | 1609 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1610 | ` * hashmap.` |
|       - | 1611 | ` * If a node with the given key already exists in the database` |
|       - | 1612 | ` * then this function overwrite the old value.` |
|       - | 1613 | ` */` |
| 2484946 | 1614 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1615 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1616 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1617 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1618 | `	)` |
|       2 | 1619 |  |
|       - | 1620 | `	sxi32 rc;` |
| 2484948 | 1621 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1622 | `		/*` |
|       - | 1623 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1624 | `		 */` |
|     ! 0 | 1625 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1626 | `		return SXRET_OK;` |
|       - | 1627 | `	}` |
| 2484948 | 1628 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2484948 | 1629 | `	return rc;` |
| 1242475 | 1630 |  |
|       - | 1631 | `/*` |
|       - | 1632 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|       - | 1633 | ` *   - String keys overwrite same-key entries in pDest.` |
|       - | 1634 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|       - | 1635 | ` * This is the same routine that backs array_merge().` |
|       - | 1636 | ` */` |
|      52 | 1637 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1638 |  |
|      53 | 1639 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|       1 | 1640 |  |
|       - | 1641 | `/*` |
|       - | 1642 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1643 | ` * hashmap.` |
|       - | 1644 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1645 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1646 | ` * The insertion by reference is triggered when the following` |
|       - | 1647 | ` * expression is encountered.` |
|       - | 1648 | ` * $var = 10;` |
|       - | 1649 | ` *  $a = array(&var);` |
|       - | 1650 | ` * OR` |
|       - | 1651 | ` *  $a[] =& $var;` |
|       - | 1652 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1653 | ` * over it's contents.` |
|       - | 1654 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1655 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1656 | ` * Example:` |
|       - | 1657 | ` *  $var = 10;` |
|       - | 1658 | ` *  $a[] =& $var;` |
|       - | 1659 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1660 | ` *  //Unset the foreign ph7_value now` |
|       - | 1661 | ` *  unset($var);` |
|       - | 1662 | ` *  echo count($a); //0` |
|       - | 1663 | ` * Note that this is a PH7 eXtension.` |
|       - | 1664 | ` * Refer to the official documentation for more information.` |
|       - | 1665 | ` * If a node with the given key already exists in the database` |
|       - | 1666 | ` * then this function overwrite the old value.` |
|       - | 1667 | ` */` |
|   35304 | 1668 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1669 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1670 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1671 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1672 | `	)` |
|       2 | 1673 |  |
|       - | 1674 | `	sxi32 rc;` |
|   35306 | 1675 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1676 | `		/*` |
|       - | 1677 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1678 | `		 */` |
|     ! 0 | 1679 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1680 | `		return SXRET_OK;` |
|       - | 1681 | `	}` |
|   35306 | 1682 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   35306 | 1683 | `	return rc;` |
|   17654 | 1684 |  |
|       - | 1685 | `/*` |
|       - | 1686 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1687 | ` */` |
|   24594 | 1688 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1689 |  |
|       - | 1690 | `	/* Reset the loop cursor */` |
|   24596 | 1691 | `	pMap->pCur = pMap->pFirst;` |
|   24596 | 1692 |  |
|       - | 1693 | `/*` |
|       - | 1694 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1695 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1696 | ` * return NULL.` |
|       - | 1697 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1698 | ` */` |
|  202282 | 1699 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1700 |  |
|  202284 | 1701 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  202284 | 1702 | `	if( pCur == 0 ){` |
|       - | 1703 | `		/* End of the list,return null */` |
|   12318 | 1704 | `		return 0;` |
|       - | 1705 | `	}` |
|       - | 1706 | `	/* Advance the node cursor */` |
|  189968 | 1707 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  189968 | 1708 | `	return pCur;` |
|  101143 | 1709 |  |
|       - | 1710 | `/*` |
|       - | 1711 | ` * Extract a node value.` |
|       - | 1712 | ` */` |
|  480898 | 1713 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1714 |  |
|  480900 | 1715 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  480900 | 1716 | `	if( pEntry ){` |
|  480900 | 1717 | `		if( bStore ){` |
|  190106 | 1718 | `			PH7_MemObjStore(pEntry,pValue);` |
|   95054 | 1719 | `		}else{` |
|  290796 | 1720 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1721 | `		}` |
|  240426 | 1722 | `	}else{` |
|     ! 0 | 1723 | `		PH7_MemObjRelease(pValue);` |
|       - | 1724 | `	}` |
|  480900 | 1725 |  |
|       - | 1726 | `/*` |
|       - | 1727 | ` * Extract a node key.` |
|       - | 1728 | ` */` |
|  119752 | 1729 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1730 |  |
|       - | 1731 | `	/* Fill with the current key */` |
|  119754 | 1732 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  119376 | 1733 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1734 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1735 | `		}` |
|  119376 | 1736 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  119376 | 1737 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   59689 | 1738 | `	}else{` |
|     380 | 1739 | `		SyBlobReset(&pKey->sBlob);` |
|     380 | 1740 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     380 | 1741 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1742 | `	}` |
|  119754 | 1743 |  |
|       - | 1744 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1745 | `/*` |
|       - | 1746 | ` * Store the address of nodes value in the given container.` |
|       - | 1747 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1748 | ` * defined in 'builtin.c' for more information.` |
|       - | 1749 | ` */` |
|      10 | 1750 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1751 |  |
|      11 | 1752 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1753 | `	ph7_value *pValue;` |
|       - | 1754 | `	sxu32 n;` |
|       - | 1755 | `	/* Initialize the container */` |
|      11 | 1756 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1757 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1758 | `		/* Extract node value */` |
|      17 | 1759 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1760 | `		if( pValue ){` |
|      17 | 1761 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1762 | `		}` |
|       - | 1763 | `		/* Point to the next entry */` |
|      17 | 1764 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1765 | `	}` |
|       - | 1766 | `	/* Total inserted entries */` |
|      11 | 1767 | `	return (int)SySetUsed(pOut);` |
|       1 | 1768 |  |
|       - | 1769 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1770 | `/* SPDX-SnippetBegin */` |
|       - | 1771 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - | 1772 | `/* SPDX-License-Identifier: blessing */` |
|       - | 1773 | `/*` |
|       - | 1774 | ` * Merge sort.` |
|       - | 1775 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1776 | ` * Status: Public domain` |
|       - | 1777 | ` */` |
|       - | 1778 | `/* Node comparison callback signature */` |
|       - | 1779 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1780 | `/*` |
|       - | 1781 | `** Inputs:` |
|       - | 1782 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1783 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1784 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1785 | `**` |
|       - | 1786 | `** Return Value:` |
|       - | 1787 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1788 | `**   of both a and b.` |
|       - | 1789 | `**` |
|       - | 1790 | `** Side effects:` |
|       - | 1791 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1792 | `**   changed.` |
|       - | 1793 | `*/` |
|   31230 | 1794 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1795 |  |
|       - | 1796 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1797 | `    /* Prevent compiler warning */` |
|   31232 | 1798 | `	result.pNext = result.pPrev = 0;` |
|   31232 | 1799 | `	pTail = &result;` |
|   92076 | 1800 | `	while( pA && pB ){` |
|   60846 | 1801 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   40782 | 1802 | `			pTail->pPrev = pA;` |
|   40782 | 1803 | `			pA->pNext = pTail;` |
|   40782 | 1804 | `			pTail = pA;` |
|   40782 | 1805 | `			pA = pA->pPrev;` |
|   20353 | 1806 | `		}else{` |
|   20066 | 1807 | `			pTail->pPrev = pB;` |
|   20066 | 1808 | `			pB->pNext = pTail;` |
|   20066 | 1809 | `			pTail = pB;` |
|   20066 | 1810 | `			pB = pB->pPrev;` |
|       - | 1811 | `		}` |
|       2 | 1812 | `	}` |
|   31232 | 1813 | `	if( pA ){` |
|   22217 | 1814 | `		pTail->pPrev = pA;` |
|   22217 | 1815 | `		pA->pNext = pTail;` |
|   20142 | 1816 | `	}else if( pB ){` |
|    8801 | 1817 | `		pTail->pPrev = pB;` |
|    8801 | 1818 | `		pB->pNext = pTail;` |
|    4384 | 1819 | `	}else{` |
|     218 | 1820 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1821 | `	}` |
|   31232 | 1822 | `	return result.pPrev;` |
|       2 | 1823 |  |
|       - | 1824 | `/*` |
|       - | 1825 | `** Inputs:` |
|       - | 1826 | `**   Map:       Input hashmap` |
|       - | 1827 | `**   cmp:       A comparison function.` |
|       - | 1828 | `**` |
|       - | 1829 | `** Return Value:` |
|       - | 1830 | `**   Sorted hashmap.` |
|       - | 1831 | `**` |
|       - | 1832 | `** Side effects:` |
|       - | 1833 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1834 | `*/` |
|       - | 1835 | `#define N_SORT_BUCKET  32` |
|     660 | 1836 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1837 |  |
|       - | 1838 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1839 | `	sxu32 i;` |
|     662 | 1840 | `	SyZero(a,sizeof(a));` |
|       - | 1841 | `	/* Point to the first inserted entry */` |
|     662 | 1842 | `	pIn = pMap->pFirst;` |
|   12606 | 1843 | `	while( pIn ){` |
|   11946 | 1844 | `		p = pIn;` |
|   11946 | 1845 | `		pIn = p->pPrev;` |
|   11946 | 1846 | `		p->pPrev = 0;` |
|   22716 | 1847 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   22716 | 1848 | `			if( a[i]==0 ){` |
|   11946 | 1849 | `				a[i] = p;` |
|   11946 | 1850 | `				break;` |
|     ! 0 | 1851 | `			}else{` |
|   10772 | 1852 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   10772 | 1853 | `				a[i] = 0;` |
|       - | 1854 | `			}` |
|    5387 | 1855 | `		}` |
|   11946 | 1856 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1857 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1858 | `			 * But that is impossible.` |
|       - | 1859 | `			 */` |
|     ! 0 | 1860 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1861 | `		}` |
|       2 | 1862 | `	}` |
|     662 | 1863 | `	p = a[0];` |
|   21122 | 1864 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   20462 | 1865 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   10232 | 1866 | `	}` |
|     662 | 1867 | `	p->pNext = 0;` |
|       - | 1868 | `	/* Reflect the change */` |
|     662 | 1869 | `	pMap->pFirst = p;` |
|       - | 1870 | `	/* Reset the loop cursor */` |
|     662 | 1871 | `	pMap->pCur = pMap->pFirst;` |
|     662 | 1872 | `	return SXRET_OK;` |
|       2 | 1873 |  |
|       - | 1874 | `/* SPDX-SnippetEnd */` |
|       - | 1875 | `/*` |
|       - | 1876 | ` * Node comparison callback.` |
|       - | 1877 | ` * used-by: [sort(),asort(),...]` |
|       - | 1878 | ` */` |
|   60649 | 1879 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1880 |  |
|       - | 1881 | `	ph7_value sA,sB;` |
|       - | 1882 | `	sxi32 iFlags;` |
|       - | 1883 | `	int rc;` |
|   60651 | 1884 | `	if( pCmpData == 0 ){` |
|       - | 1885 | `		/* Perform a standard comparison */` |
|   60627 | 1886 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   60627 | 1887 | `		return rc;` |
|       - | 1888 | `	}` |
|      25 | 1889 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1890 | `	/* Duplicate node values */` |
|      25 | 1891 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 1892 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 1893 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 1894 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 1895 | `	if( iFlags == 5 ){` |
|       - | 1896 | `		/* String cast */` |
|       - | 1897 | `		const char *zA,*zB;` |
|       - | 1898 | `		sxu32 nA,nB,nMin;` |
|      15 | 1899 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1900 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1901 | `		}` |
|      15 | 1902 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1903 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1904 | `		}` |
|       - | 1905 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 1906 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 1907 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 1908 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 1909 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 1910 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 1911 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 1912 | `		if( rc == 0 ){` |
|       5 | 1913 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1914 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1915 | `		}` |
|       8 | 1916 | `	}else{` |
|       - | 1917 | `		/* Numeric cast */` |
|      11 | 1918 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1919 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1920 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1921 | `	}` |
|      25 | 1922 | `	PH7_MemObjRelease(&sA);` |
|      25 | 1923 | `	PH7_MemObjRelease(&sB);` |
|      25 | 1924 | `	return rc;` |
|   30314 | 1925 |  |
|       - | 1926 | `/*` |
|       - | 1927 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1928 | ` * used-by: [ksort()]` |
|       - | 1929 | ` */` |
|      14 | 1930 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1931 |  |
|       - | 1932 | `	sxi32 rc;` |
|       7 | 1933 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 1934 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1935 | `		/* Perform a string comparison */` |
|       5 | 1936 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1937 | `	}else{` |
|       - | 1938 | `		SyString sStr;` |
|       - | 1939 | `		sxi64 iA,iB;` |
|       - | 1940 | `		/* Perform a numeric comparison */` |
|      11 | 1941 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1942 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1943 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1944 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1945 | `				iA = 0;` |
|     ! 0 | 1946 | `			}else{` |
|     ! 0 | 1947 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1948 | `			}` |
|     ! 0 | 1949 | `		}else{` |
|      11 | 1950 | `			iA = pA->xKey.iKey;` |
|       - | 1951 | `		}` |
|      11 | 1952 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1953 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1954 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1955 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1956 | `				iB = 0;` |
|     ! 0 | 1957 | `			}else{` |
|     ! 0 | 1958 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1959 | `			}` |
|     ! 0 | 1960 | `		}else{` |
|      11 | 1961 | `			iB = pB->xKey.iKey;` |
|       - | 1962 | `		}` |
|      11 | 1963 | `		rc = (sxi32)(iA-iB);` |
|       - | 1964 | `	}` |
|       - | 1965 | `	/* Comparison result */` |
|      15 | 1966 | `	return rc;` |
|       1 | 1967 |  |
|       - | 1968 | `/*` |
|       - | 1969 | ` * Node comparison callback.` |
|       - | 1970 | ` * Used by: [rsort(),arsort()];` |
|       - | 1971 | ` */` |
|      78 | 1972 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1973 |  |
|       - | 1974 | `	ph7_value sA,sB;` |
|       - | 1975 | `	sxi32 iFlags;` |
|       - | 1976 | `	int rc;` |
|      79 | 1977 | `	if( pCmpData == 0 ){` |
|       - | 1978 | `		/* Perform a standard comparison */` |
|      59 | 1979 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 1980 | `		return -rc;` |
|       - | 1981 | `	}` |
|      21 | 1982 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1983 | `	/* Duplicate node values */` |
|      21 | 1984 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 1985 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 1986 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 1987 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 1988 | `	if( iFlags == 5 ){` |
|       - | 1989 | `		/* String cast */` |
|       - | 1990 | `		const char *zA,*zB;` |
|       - | 1991 | `		sxu32 nA,nB,nMin;` |
|      11 | 1992 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1993 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1994 | `		}` |
|      11 | 1995 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1996 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1997 | `		}` |
|       - | 1998 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 1999 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 2000 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 2001 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 2002 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 2003 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 2004 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 2005 | `		if( rc == 0 ){` |
|       3 | 2006 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 2007 | `			else if( nA > nB ) rc = 1;` |
|       1 | 2008 | `		}` |
|       6 | 2009 | `	}else{` |
|       - | 2010 | `		/* Numeric cast */` |
|      11 | 2011 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 2012 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 2013 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 2014 | `	}` |
|      21 | 2015 | `	PH7_MemObjRelease(&sA);` |
|      21 | 2016 | `	PH7_MemObjRelease(&sB);` |
|      21 | 2017 | `	return -rc;` |
|      40 | 2018 |  |
|       - | 2019 | `/*` |
|       - | 2020 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2021 | ` * used-by: [usort(),uasort()]` |
|       - | 2022 | ` */` |
|      78 | 2023 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 2024 |  |
|       - | 2025 | `	ph7_value sResult,*pCallback;` |
|       - | 2026 | `	ph7_value *pV1,*pV2;` |
|       - | 2027 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2028 | `	sxi32 rc;` |
|       - | 2029 | `	/* Point to the desired callback */` |
|      80 | 2030 | `	pCallback = (ph7_value *)pCmpData;` |
|      80 | 2031 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2032 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2033 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       3 | 2034 | `		return 0;` |
|       - | 2035 | `	}` |
|       - | 2036 | `	/* initialize the result value */` |
|      78 | 2037 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 2038 | `	/* Extract nodes values */` |
|      78 | 2039 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      78 | 2040 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      78 | 2041 | `	apArg[0] = pV1;` |
|      78 | 2042 | `	apArg[1] = pV2;` |
|       - | 2043 | `	/* Invoke the callback */` |
|      78 | 2044 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      78 | 2045 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2046 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2047 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       3 | 2048 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       3 | 2049 | `		rc = 0;` |
|      77 | 2050 | `	}else if( rc != SXRET_OK ){` |
|       - | 2051 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2052 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2053 | `	}else{` |
|       - | 2054 | `		/* Extract callback result */` |
|      76 | 2055 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2056 | `			/* Perform an int cast */` |
|     ! 0 | 2057 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2058 | `		}` |
|      76 | 2059 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2060 | `	}` |
|      78 | 2061 | `	PH7_MemObjRelease(&sResult);` |
|       - | 2062 | `	/* Callback result */` |
|      78 | 2063 | `	return rc;` |
|      41 | 2064 |  |
|       - | 2065 | `/*` |
|       - | 2066 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2067 | ` * used-by: [krsort()]` |
|       - | 2068 | ` */` |
|       4 | 2069 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2070 |  |
|       - | 2071 | `	sxi32 rc;` |
|       2 | 2072 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2073 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2074 | `		/* Perform a string comparison */` |
|       5 | 2075 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2076 | `	}else{` |
|       - | 2077 | `		SyString sStr;` |
|       - | 2078 | `		sxi64 iA,iB;` |
|       - | 2079 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2080 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2081 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2082 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2083 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2084 | `				iA = 0;` |
|     ! 0 | 2085 | `			}else{` |
|     ! 0 | 2086 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2087 | `			}` |
|     ! 0 | 2088 | `		}else{` |
|     ! 0 | 2089 | `			iA = pA->xKey.iKey;` |
|       - | 2090 | `		}` |
|     ! 0 | 2091 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2092 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2093 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2094 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2095 | `				iB = 0;` |
|     ! 0 | 2096 | `			}else{` |
|     ! 0 | 2097 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2098 | `			}` |
|     ! 0 | 2099 | `		}else{` |
|     ! 0 | 2100 | `			iB = pB->xKey.iKey;` |
|       - | 2101 | `		}` |
|     ! 0 | 2102 | `		rc = (sxi32)(iA-iB);` |
|       - | 2103 | `	}` |
|       5 | 2104 | `	return -rc; /* Reverse result */` |
|       1 | 2105 |  |
|       - | 2106 | `/*` |
|       - | 2107 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2108 | ` * used-by: [uksort()]` |
|       - | 2109 | ` */` |
|       6 | 2110 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2111 |  |
|       - | 2112 | `	ph7_value sResult,*pCallback;` |
|       - | 2113 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2114 | `	ph7_value sK1,sK2;` |
|       - | 2115 | `	sxi32 rc;` |
|       - | 2116 | `	/* Point to the desired callback */` |
|       7 | 2117 | `	pCallback = (ph7_value *)pCmpData;` |
|       7 | 2118 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2119 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2120 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     ! 0 | 2121 | `		return 0;` |
|       - | 2122 | `	}` |
|       - | 2123 | `	/* initialize the result value */` |
|       7 | 2124 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2125 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2126 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2127 | `	/* Extract nodes keys */` |
|       7 | 2128 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2129 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2130 | `	apArg[0] = &sK1;` |
|       7 | 2131 | `	apArg[1] = &sK2;` |
|       - | 2132 | `	/* Mark keys as constants */` |
|       7 | 2133 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2134 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2135 | `	/* Invoke the callback */` |
|       7 | 2136 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2137 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2138 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2139 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     ! 0 | 2140 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     ! 0 | 2141 | `		rc = 0;` |
|       7 | 2142 | `	}else if( rc != SXRET_OK ){` |
|       - | 2143 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2144 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2145 | `	}else{` |
|       - | 2146 | `		/* Extract callback result */` |
|       7 | 2147 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2148 | `			/* Perform an int cast */` |
|     ! 0 | 2149 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2150 | `		}` |
|       7 | 2151 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2152 | `	}` |
|       7 | 2153 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2154 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2155 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2156 | `	/* Callback result */` |
|       7 | 2157 | `	return rc;` |
|       4 | 2158 |  |
|       - | 2159 | `/*` |
|       - | 2160 | ` * Node comparison callback: Random node comparison.` |
|       - | 2161 | ` * used-by: [shuffle()]` |
|       - | 2162 | ` */` |
|      15 | 2163 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2164 |  |
|       - | 2165 | `	sxu32 n;` |
|       8 | 2166 | `	SXUNUSED(pB); /* cc warning */` |
|       8 | 2167 | `	SXUNUSED(pCmpData);` |
|       - | 2168 | `	/* Grab a random number */` |
|      16 | 2169 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2170 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2171 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2172 | `	 */` |
|      16 | 2173 | `	return n&1 ? 1 : -1;` |
|       1 | 2174 |  |
|       - | 2175 | `/*` |
|       - | 2176 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2177 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2178 | ` */` |
|     612 | 2179 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2180 |  |
|       - | 2181 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2182 | `	sxu32 i;` |
|       - | 2183 | `	/* Rehash all entries */` |
|     614 | 2184 | `	pLast = p = pMap->pFirst;` |
|     614 | 2185 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     614 | 2186 | `	i = 0;` |
|    6193 | 2187 | `	for( ;; ){` |
|   12388 | 2188 | `		if( i >= pMap->nEntry ){` |
|     614 | 2189 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     614 | 2190 | `			break;` |
|       - | 2191 | `		}` |
|   11776 | 2192 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2193 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2194 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2195 | `			/* Change key type */` |
|       5 | 2196 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2197 | `		}` |
|   11776 | 2198 | `		HashmapRehashIntNode(p);` |
|       - | 2199 | `		/* Point to the next entry */` |
|   11776 | 2200 | `		i++;` |
|   11776 | 2201 | `		pLast = p;` |
|   11776 | 2202 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2203 | `	}` |
|     614 | 2204 |  |
|       - | 2205 | `/*` |
|       - | 2206 | ` * Array functions implementation.` |
|       - | 2207 | ` * Status:` |
|       - | 2208 | ` *  Stable.` |
|       - | 2209 | ` */` |
|       - | 2210 | `/*` |
|       - | 2211 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2212 | ` * Sort an array.` |
|       - | 2213 | ` * Parameters` |
|       - | 2214 | ` *  $array` |
|       - | 2215 | ` *   The input array.` |
|       - | 2216 | ` * $sort_flags` |
|       - | 2217 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2218 | ` *  Sorting type flags:` |
|       - | 2219 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2220 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2221 | ` *   SORT_STRING - compare items as strings` |
|       - | 2222 | ` * Return` |
|       - | 2223 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2224 | ` *` |
|       - | 2225 | ` */` |
|     942 | 2226 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2227 |  |
|       - | 2228 | `	ph7_hashmap *pMap;` |
|       - | 2229 | `	/* Make sure we are dealing with a valid hashmap */` |
|     944 | 2230 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2231 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2232 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2233 | `		return PH7_OK;` |
|       - | 2234 | `	}` |
|       - | 2235 | `	/* Point to the internal representation of the input hashmap */` |
|     944 | 2236 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     944 | 2237 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     944 | 2238 | `	if( pMap->nEntry > 1 ){` |
|     602 | 2239 | `		sxi32 iCmpFlags = 0;` |
|     602 | 2240 | `		if( nArg > 1 ){` |
|       - | 2241 | `			/* Extract comparison flags */` |
|       3 | 2242 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2243 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2244 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2245 | `			}` |
|       1 | 2246 | `		}` |
|       - | 2247 | `		/* Do the merge sort */` |
|     602 | 2248 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2249 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     602 | 2250 | `		HashmapSortRehash(pMap);` |
|     300 | 2251 | `	}` |
|       - | 2252 | `	/* All done,return TRUE */` |
|     944 | 2253 | `	ph7_result_bool(pCtx,1);` |
|     944 | 2254 | `	return PH7_OK;` |
|     473 | 2255 |  |
|       - | 2256 | `/*` |
|       - | 2257 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2258 | ` *  Sort an array and maintain index association.` |
|       - | 2259 | ` * Parameters` |
|       - | 2260 | ` *  $array` |
|       - | 2261 | ` *   The input array.` |
|       - | 2262 | ` * $sort_flags` |
|       - | 2263 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2264 | ` *  Sorting type flags:` |
|       - | 2265 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2266 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2267 | ` *   SORT_STRING - compare items as strings` |
|       - | 2268 | ` * Return` |
|       - | 2269 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2270 | ` */` |
|      32 | 2271 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2272 |  |
|       - | 2273 | `	ph7_hashmap *pMap;` |
|       - | 2274 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2275 | `	if( nArg < 1 ){` |
|       3 | 2276 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2277 | `			"ArgumentCountError",` |
|       - | 2278 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2279 | `			);` |
|       - | 2280 | `	}` |
|       - | 2281 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2282 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2283 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2284 | `			"TypeError",` |
|       - | 2285 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2286 | `			ph7_type_name(apArg[0])` |
|       - | 2287 | `			);` |
|       - | 2288 | `	}` |
|       - | 2289 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2290 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2291 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2292 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2293 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2294 | `		if( nArg > 1 ){` |
|       - | 2295 | `			/* Extract comparison flags */` |
|       5 | 2296 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2297 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2298 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2299 | `			}` |
|       2 | 2300 | `		}` |
|       - | 2301 | `		/* Do the merge sort */` |
|      19 | 2302 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2303 | `		/* Fix the last link broken by the merge */` |
|      45 | 2304 | `		while(pMap->pLast->pPrev){` |
|      27 | 2305 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2306 | `		}` |
|       9 | 2307 | `	}` |
|       - | 2308 | `	/* All done,return TRUE */` |
|      23 | 2309 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2310 | `	return PH7_OK;` |
|      18 | 2311 |  |
|       - | 2312 | `/*` |
|       - | 2313 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2314 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2315 | ` * Parameters` |
|       - | 2316 | ` *  $array` |
|       - | 2317 | ` *   The input array.` |
|       - | 2318 | ` * $sort_flags` |
|       - | 2319 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2320 | ` *  Sorting type flags:` |
|       - | 2321 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2322 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2323 | ` *   SORT_STRING - compare items as strings` |
|       - | 2324 | ` * Return` |
|       - | 2325 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2326 | ` */` |
|      32 | 2327 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2328 |  |
|       - | 2329 | `	ph7_hashmap *pMap;` |
|       - | 2330 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2331 | `	if( nArg < 1 ){` |
|       3 | 2332 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2333 | `			"ArgumentCountError",` |
|       - | 2334 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2335 | `			);` |
|       - | 2336 | `	}` |
|       - | 2337 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2338 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2339 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2340 | `			"TypeError",` |
|       - | 2341 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2342 | `			ph7_type_name(apArg[0])` |
|       - | 2343 | `			);` |
|       - | 2344 | `	}` |
|       - | 2345 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2346 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2347 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2348 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2349 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2350 | `		if( nArg > 1 ){` |
|       - | 2351 | `			/* Extract comparison flags */` |
|       5 | 2352 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2353 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2354 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2355 | `			}` |
|       2 | 2356 | `		}` |
|       - | 2357 | `		/* Do the merge sort */` |
|      19 | 2358 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2359 | `		/* Fix the last link broken by the merge */` |
|      35 | 2360 | `		while(pMap->pLast->pPrev){` |
|      17 | 2361 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2362 | `		}` |
|       9 | 2363 | `	}` |
|       - | 2364 | `	/* All done,return TRUE */` |
|      23 | 2365 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2366 | `	return PH7_OK;` |
|      18 | 2367 |  |
|       - | 2368 | `/*` |
|       - | 2369 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2370 | ` *  Sort an array by key.` |
|       - | 2371 | ` * Parameters` |
|       - | 2372 | ` *  $array` |
|       - | 2373 | ` *   The input array.` |
|       - | 2374 | ` * $sort_flags` |
|       - | 2375 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2376 | ` *  Sorting type flags:` |
|       - | 2377 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2378 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2379 | ` *   SORT_STRING - compare items as strings` |
|       - | 2380 | ` * Return` |
|       - | 2381 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2382 | ` */` |
|       4 | 2383 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2384 |  |
|       - | 2385 | `	ph7_hashmap *pMap;` |
|       - | 2386 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2387 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2388 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2389 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2390 | `		return PH7_OK;` |
|       - | 2391 | `	}` |
|       - | 2392 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2393 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2394 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2395 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2396 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2397 | `		if( nArg > 1 ){` |
|       - | 2398 | `			/* Extract comparison flags */` |
|     ! 0 | 2399 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2400 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2401 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2402 | `			}` |
|     ! 0 | 2403 | `		}` |
|       - | 2404 | `		/* Do the merge sort */` |
|       5 | 2405 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2406 | `		/* Fix the last link broken by the merge */` |
|      15 | 2407 | `		while(pMap->pLast->pPrev){` |
|      11 | 2408 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2409 | `		}` |
|       2 | 2410 | `	}` |
|       - | 2411 | `	/* All done,return TRUE */` |
|       5 | 2412 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2413 | `	return PH7_OK;` |
|       3 | 2414 |  |
|       - | 2415 | `/*` |
|       - | 2416 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2417 | ` *  Sort an array by key in reverse order.` |
|       - | 2418 | ` * Parameters` |
|       - | 2419 | ` *  $array` |
|       - | 2420 | ` *   The input array.` |
|       - | 2421 | ` * $sort_flags` |
|       - | 2422 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2423 | ` *  Sorting type flags:` |
|       - | 2424 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2425 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2426 | ` *   SORT_STRING - compare items as strings` |
|       - | 2427 | ` * Return` |
|       - | 2428 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2429 | ` */` |
|       2 | 2430 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2431 |  |
|       - | 2432 | `	ph7_hashmap *pMap;` |
|       - | 2433 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2434 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2435 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2436 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2437 | `		return PH7_OK;` |
|       - | 2438 | `	}` |
|       - | 2439 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2440 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2441 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2442 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2443 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2444 | `		if( nArg > 1 ){` |
|       - | 2445 | `			/* Extract comparison flags */` |
|     ! 0 | 2446 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2447 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2448 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2449 | `			}` |
|     ! 0 | 2450 | `		}` |
|       - | 2451 | `		/* Do the merge sort */` |
|       3 | 2452 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2453 | `		/* Fix the last link broken by the merge */` |
|       7 | 2454 | `		while(pMap->pLast->pPrev){` |
|       5 | 2455 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2456 | `		}` |
|       1 | 2457 | `	}` |
|       - | 2458 | `	/* All done,return TRUE */` |
|       3 | 2459 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2460 | `	return PH7_OK;` |
|       2 | 2461 |  |
|       - | 2462 | `/*` |
|       - | 2463 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2464 | ` * Sort an array in reverse order.` |
|       - | 2465 | ` * Parameters` |
|       - | 2466 | ` *  $array` |
|       - | 2467 | ` *   The input array.` |
|       - | 2468 | ` * $sort_flags` |
|       - | 2469 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2470 | ` *  Sorting type flags:` |
|       - | 2471 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2472 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2473 | ` *   SORT_STRING - compare items as strings` |
|       - | 2474 | ` * Return` |
|       - | 2475 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2476 | ` */` |
|       2 | 2477 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2478 |  |
|       - | 2479 | `	ph7_hashmap *pMap;` |
|       - | 2480 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2481 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2482 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2483 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2484 | `		return PH7_OK;` |
|       - | 2485 | `	}` |
|       - | 2486 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2487 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2488 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2489 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2490 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2491 | `		if( nArg > 1 ){` |
|       - | 2492 | `			/* Extract comparison flags */` |
|     ! 0 | 2493 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2494 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2495 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2496 | `			}` |
|     ! 0 | 2497 | `		}` |
|       - | 2498 | `		/* Do the merge sort */` |
|       3 | 2499 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2500 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2501 | `		HashmapSortRehash(pMap);` |
|       1 | 2502 | `	}` |
|       - | 2503 | `	/* All done,return TRUE */` |
|       3 | 2504 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2505 | `	return PH7_OK;` |
|       2 | 2506 |  |
|       - | 2507 | `/*` |
|       - | 2508 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2509 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2510 | ` * Parameters` |
|       - | 2511 | ` *  $array` |
|       - | 2512 | ` *   The input array.` |
|       - | 2513 | ` * $cmp_function` |
|       - | 2514 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2515 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2516 | ` *  to, or greater than the second.` |
|       - | 2517 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2518 | ` * Return` |
|       - | 2519 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2520 | ` */` |
|       8 | 2521 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2522 |  |
|       - | 2523 | `	ph7_hashmap *pMap;` |
|       - | 2524 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 | 2525 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2526 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2527 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2528 | `		return PH7_OK;` |
|       - | 2529 | `	}` |
|       - | 2530 | `	/* Point to the internal representation of the input hashmap */` |
|      10 | 2531 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      10 | 2532 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      10 | 2533 | `	if( pMap->nEntry > 1 ){` |
|      10 | 2534 | `		ph7_value *pCallback = 0;` |
|       - | 2535 | `		ProcNodeCmp xCmp;` |
|      10 | 2536 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      10 | 2537 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2538 | `			/* Point to the desired callback */` |
|      10 | 2539 | `			pCallback = apArg[1];` |
|       6 | 2540 | `		}else{` |
|       - | 2541 | `			/* Use the default comparison function */` |
|     ! 0 | 2542 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2543 | `		}` |
|       - | 2544 | `		/* Do the merge sort */` |
|      10 | 2545 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      10 | 2546 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2547 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      10 | 2548 | `		HashmapSortRehash(pMap);` |
|      10 | 2549 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2550 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 2551 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2552 | `			return PH7_EXCEPTION;` |
|       - | 2553 | `		}` |
|       3 | 2554 | `	}` |
|       - | 2555 | `	/* All done,return TRUE */` |
|       8 | 2556 | `	ph7_result_bool(pCtx,1);` |
|       8 | 2557 | `	return PH7_OK;` |
|       6 | 2558 |  |
|       - | 2559 | `/*` |
|       - | 2560 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2561 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2562 | ` *  and maintain index association.` |
|       - | 2563 | ` * Parameters` |
|       - | 2564 | ` *  $array` |
|       - | 2565 | ` *   The input array.` |
|       - | 2566 | ` * $cmp_function` |
|       - | 2567 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2568 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2569 | ` *  to, or greater than the second.` |
|       - | 2570 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2571 | ` * Return` |
|       - | 2572 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2573 | ` */` |
|       2 | 2574 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2575 |  |
|       - | 2576 | `	ph7_hashmap *pMap;` |
|       - | 2577 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2578 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2579 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2580 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2581 | `		return PH7_OK;` |
|       - | 2582 | `	}` |
|       - | 2583 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2584 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2585 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2586 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2587 | `		ph7_value *pCallback = 0;` |
|       - | 2588 | `		ProcNodeCmp xCmp;` |
|       3 | 2589 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2590 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2591 | `			/* Point to the desired callback */` |
|       3 | 2592 | `			pCallback = apArg[1];` |
|       2 | 2593 | `		}else{` |
|       - | 2594 | `			/* Use the default comparison function */` |
|     ! 0 | 2595 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2596 | `		}` |
|       - | 2597 | `		/* Do the merge sort */` |
|       3 | 2598 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2599 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2600 | `		/* Fix the last link broken by the merge */` |
|       5 | 2601 | `		while(pMap->pLast->pPrev){` |
|       3 | 2602 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2603 | `		}` |
|       3 | 2604 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2605 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2606 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2607 | `			return PH7_EXCEPTION;` |
|       - | 2608 | `		}` |
|       1 | 2609 | `	}` |
|       - | 2610 | `	/* All done,return TRUE */` |
|       3 | 2611 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2612 | `	return PH7_OK;` |
|       2 | 2613 |  |
|       - | 2614 | `/*` |
|       - | 2615 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2616 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2617 | ` *  function and maintain index association.` |
|       - | 2618 | ` * Parameters` |
|       - | 2619 | ` *  $array` |
|       - | 2620 | ` *   The input array.` |
|       - | 2621 | ` * $cmp_function` |
|       - | 2622 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2623 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2624 | ` *  to, or greater than the second.` |
|       - | 2625 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2626 | ` * Return` |
|       - | 2627 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2628 | ` */` |
|       2 | 2629 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2630 |  |
|       - | 2631 | `	ph7_hashmap *pMap;` |
|       - | 2632 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2633 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2634 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2635 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2636 | `		return PH7_OK;` |
|       - | 2637 | `	}` |
|       - | 2638 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2639 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2640 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2641 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2642 | `		ph7_value *pCallback = 0;` |
|       - | 2643 | `		ProcNodeCmp xCmp;` |
|       3 | 2644 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2645 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2646 | `			/* Point to the desired callback */` |
|       3 | 2647 | `			pCallback = apArg[1];` |
|       2 | 2648 | `		}else{` |
|       - | 2649 | `			/* Use the default comparison function */` |
|     ! 0 | 2650 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2651 | `		}` |
|       - | 2652 | `		/* Do the merge sort */` |
|       3 | 2653 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2654 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2655 | `		/* Fix the last link broken by the merge */` |
|       3 | 2656 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2657 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2658 | `		}` |
|       3 | 2659 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2660 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2661 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2662 | `			return PH7_EXCEPTION;` |
|       - | 2663 | `		}` |
|       1 | 2664 | `	}` |
|       - | 2665 | `	/* All done,return TRUE */` |
|       3 | 2666 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2667 | `	return PH7_OK;` |
|       2 | 2668 |  |
|       - | 2669 | `/*` |
|       - | 2670 | ` * bool shuffle(array &$array)` |
|       - | 2671 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2672 | ` * Parameters` |
|       - | 2673 | ` *  $array` |
|       - | 2674 | ` *   The input array.` |
|       - | 2675 | ` * Return` |
|       - | 2676 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2677 | ` *` |
|       - | 2678 | ` */` |
|       2 | 2679 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2680 |  |
|       - | 2681 | `	ph7_hashmap *pMap;` |
|       - | 2682 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2683 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2684 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2685 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2686 | `		return PH7_OK;` |
|       - | 2687 | `	}` |
|       - | 2688 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2689 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2690 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2691 | `	if( pMap->nEntry > 1 ){` |
|       - | 2692 | `		/* Do the merge sort */` |
|       3 | 2693 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2694 | `		/* Fix the last link broken by the merge */` |
|       9 | 2695 | `		while(pMap->pLast->pPrev){` |
|       7 | 2696 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2697 | `		}` |
|       1 | 2698 | `	}` |
|       - | 2699 | `	/* All done,return TRUE */` |
|       3 | 2700 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2701 | `	return PH7_OK;` |
|       2 | 2702 |  |
|       - | 2703 | `/*` |
|       - | 2704 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2705 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2706 | ` * Parameters` |
|       - | 2707 | ` *  $var` |
|       - | 2708 | ` *   The array or the object.` |
|       - | 2709 | ` * $mode` |
|       - | 2710 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2711 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2712 | ` *  all the elements of a multidimensional array.` |
|       - | 2713 | ` * Return` |
|       - | 2714 | ` *  Returns the number of elements in the array.` |
|       - | 2715 | ` */` |
|     802 | 2716 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2717 |  |
|     804 | 2718 | `	int bRecursive = FALSE;` |
|     804 | 2719 | `	int bCycleDetected = FALSE;` |
|       - | 2720 | `	sxi64 iCount;` |
|     804 | 2721 | `	if( nArg < 1 ){` |
|       3 | 2722 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2723 | `			"ArgumentCountError",` |
|       - | 2724 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2725 | `			);` |
|       - | 2726 | `	}` |
|     802 | 2727 | `	if( nArg > 2 ){` |
|       4 | 2728 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2729 | `			"ArgumentCountError",` |
|       - | 2730 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2731 | `			nArg` |
|       - | 2732 | `			);` |
|       - | 2733 | `	}` |
|       - | 2734 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - | 2735 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - | 2736 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|     800 | 2737 | `	if( nArg > 1 ){` |
|      42 | 2738 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      42 | 2739 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       9 | 2740 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2741 | `				"ValueError",` |
|       - | 2742 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2743 | `				);` |
|       - | 2744 | `		}` |
|      34 | 2745 | `		bRecursive = iMode == 1;` |
|      16 | 2746 | `	}` |
|     792 | 2747 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2748 | `		/* Countable object: dispatch to ->count() */` |
|      28 | 2749 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      18 | 2750 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      18 | 2751 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      18 | 2752 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      16 | 2753 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2754 | `					"count",sizeof("count")-1);` |
|      16 | 2755 | `				if( pMeth ){` |
|       - | 2756 | `					ph7_value sResult;` |
|      16 | 2757 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      16 | 2758 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      16 | 2759 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      16 | 2760 | `					PH7_MemObjRelease(&sResult);` |
|      16 | 2761 | `					return PH7_OK;` |
|       - | 2762 | `				}` |
|     ! 0 | 2763 | `			}` |
|       1 | 2764 | `		}` |
|      19 | 2765 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2766 | `			"TypeError",` |
|       - | 2767 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       6 | 2768 | `			ph7_type_name(apArg[0])` |
|       - | 2769 | `			);` |
|       - | 2770 | `	}` |
|       - | 2771 | `	/* Count */` |
|     766 | 2772 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     766 | 2773 | `	if( bCycleDetected ){` |
|       3 | 2774 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2775 | `	}` |
|     766 | 2776 | `	ph7_result_int64(pCtx,iCount);` |
|     766 | 2777 | `	return PH7_OK;` |
|     403 | 2778 |  |
|       - | 2779 | `/*` |
|       - | 2780 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2781 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2782 | ` * Parameters` |
|       - | 2783 | ` * $key` |
|       - | 2784 | ` *   Value to check.` |
|       - | 2785 | ` * $search` |
|       - | 2786 | ` *  An array with keys to check.` |
|       - | 2787 | ` * Return` |
|       - | 2788 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2789 | ` */` |
|      82 | 2790 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2791 |  |
|       - | 2792 | `	sxi32 rc;` |
|      84 | 2793 | `	if( nArg != 2 ){` |
|       - | 2794 | `		/* PHP requires exactly two arguments */` |
|      10 | 2795 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2796 | `			"ArgumentCountError",` |
|       - | 2797 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2798 | `			nArg` |
|       - | 2799 | `			);` |
|       - | 2800 | `	}` |
|       - | 2801 | `	/* Make sure we are dealing with a valid hashmap */` |
|      78 | 2802 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2803 | `		/* Type mismatch -> TypeError */` |
|       7 | 2804 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2805 | `			"TypeError",` |
|       - | 2806 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2807 | `			ph7_type_name(apArg[1])` |
|       - | 2808 | `			);` |
|       - | 2809 | `	}` |
|       - | 2810 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      74 | 2811 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2812 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2813 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2814 | `			"use an empty string instead"` |
|       - | 2815 | `			);` |
|      73 | 2816 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2817 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2818 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2819 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2820 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2821 | `				,rVal` |
|       - | 2822 | `				);` |
|       1 | 2823 | `		}` |
|       1 | 2824 | `	}` |
|       - | 2825 | `	/* Perform the lookup */` |
|      74 | 2826 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2827 | `	/* lookup result */` |
|      74 | 2828 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      74 | 2829 | `	return PH7_OK;` |
|      43 | 2830 |  |
|       - | 2831 | `/*` |
|       - | 2832 | ` * value array_pop(array $array)` |
|       - | 2833 | ` *   POP the last inserted element from the array.` |
|       - | 2834 | ` * Parameter` |
|       - | 2835 | ` *  The array to get the value from.` |
|       - | 2836 | ` * Return` |
|       - | 2837 | ` *  Poped value or NULL on failure.` |
|       - | 2838 | ` */` |
|      18 | 2839 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2840 |  |
|       - | 2841 | `	ph7_hashmap *pMap;` |
|       - | 2842 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      20 | 2843 | `	if( nArg != 1 ){` |
|       7 | 2844 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2845 | `			"ArgumentCountError",` |
|       - | 2846 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2847 | `			nArg` |
|       - | 2848 | `			);` |
|       - | 2849 | `	}` |
|       - | 2850 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2851 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      16 | 2852 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2853 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2854 | `			"Error",` |
|       - | 2855 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2856 | `			);` |
|       - | 2857 | `	}` |
|       - | 2858 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2859 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2860 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2861 | `			"TypeError",` |
|       - | 2862 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2863 | `			ph7_type_name(apArg[0])` |
|       - | 2864 | `			);` |
|       - | 2865 | `	}` |
|       9 | 2866 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2867 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2868 | `	if( pMap->nEntry < 1 ){` |
|       - | 2869 | `		/* Nothing to pop,return NULL */` |
|       3 | 2870 | `		ph7_result_null(pCtx);` |
|       2 | 2871 | `	}else{` |
|       7 | 2872 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2873 | `		ph7_value *pObj;` |
|       7 | 2874 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2875 | `		if( pObj ){` |
|       - | 2876 | `			/* Node value */` |
|       7 | 2877 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2878 | `			/* Unlink the node */` |
|       7 | 2879 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2880 | `		}else{` |
|     ! 0 | 2881 | `			ph7_result_null(pCtx);` |
|       - | 2882 | `		}` |
|       - | 2883 | `		/* Reset the cursor */` |
|       7 | 2884 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2885 | `	}` |
|       9 | 2886 | `	return PH7_OK;` |
|      11 | 2887 |  |
|       - | 2888 | `/*` |
|       - | 2889 | ` * int array_push($array,$var,...)` |
|       - | 2890 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2891 | ` * Parameters` |
|       - | 2892 | ` *  array` |
|       - | 2893 | ` *    The input array.` |
|       - | 2894 | ` *  var` |
|       - | 2895 | ` *   On or more value to push.` |
|       - | 2896 | ` * Return` |
|       - | 2897 | ` *  New array count (including old items).` |
|       - | 2898 | ` */` |
|      22 | 2899 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2900 |  |
|       - | 2901 | `	ph7_hashmap *pMap;` |
|       - | 2902 | `	sxi32 rc;` |
|       - | 2903 | `	int i;` |
|      24 | 2904 | `	if( nArg < 1 ){` |
|       4 | 2905 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2906 | `			"ArgumentCountError",` |
|       - | 2907 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2908 | `			nArg` |
|       - | 2909 | `			);` |
|       - | 2910 | `	}` |
|       - | 2911 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2912 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      22 | 2913 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2914 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2915 | `			"Error",` |
|       - | 2916 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2917 | `			);` |
|       - | 2918 | `	}` |
|       - | 2919 | `	/* Make sure we are dealing with a valid hashmap */` |
|      18 | 2920 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2921 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2922 | `			"TypeError",` |
|       - | 2923 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2924 | `			ph7_type_name(apArg[0])` |
|       - | 2925 | `			);` |
|       - | 2926 | `	}` |
|       - | 2927 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2928 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2929 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2930 | `	/* Start pushing given values */` |
|      31 | 2931 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      17 | 2932 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      17 | 2933 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2934 | `			break;` |
|       - | 2935 | `		}` |
|       9 | 2936 | `	}` |
|       - | 2937 | `	/* Return the new count */` |
|      15 | 2938 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 2939 | `	return PH7_OK;` |
|      13 | 2940 |  |
|       - | 2941 | `/*` |
|       - | 2942 | ` * value array_shift(array $array)` |
|       - | 2943 | ` *   Shift an element off the beginning of array.` |
|       - | 2944 | ` * Parameter` |
|       - | 2945 | ` *  The array to get the value from.` |
|       - | 2946 | ` * Return` |
|       - | 2947 | ` *  Shifted value or NULL on failure.` |
|       - | 2948 | ` */` |
|      38 | 2949 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2950 |  |
|       - | 2951 | `	ph7_hashmap *pMap;` |
|       - | 2952 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      40 | 2953 | `	if( nArg != 1 ){` |
|       7 | 2954 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2955 | `			"ArgumentCountError",` |
|       - | 2956 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2957 | `			nArg` |
|       - | 2958 | `			);` |
|       - | 2959 | `	}` |
|       - | 2960 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      36 | 2961 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2962 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2963 | `			"Error",` |
|       - | 2964 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2965 | `			);` |
|       - | 2966 | `	}` |
|       - | 2967 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 2968 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2969 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2970 | `			"TypeError",` |
|       - | 2971 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2972 | `			ph7_type_name(apArg[0])` |
|       - | 2973 | `			);` |
|       - | 2974 | `	}` |
|       - | 2975 | `	/* Point to the internal representation of the hashmap */` |
|      30 | 2976 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      30 | 2977 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      30 | 2978 | `	if( pMap->nEntry < 1 ){` |
|       - | 2979 | `		/* Empty hashmap,return NULL */` |
|       3 | 2980 | `		ph7_result_null(pCtx);` |
|       2 | 2981 | `	}else{` |
|      28 | 2982 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2983 | `		ph7_value *pObj;` |
|       - | 2984 | `		sxu32 n;` |
|      28 | 2985 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      28 | 2986 | `		if( pObj ){` |
|       - | 2987 | `			/* Node value */` |
|      28 | 2988 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2989 | `			/* Unlink the first node */` |
|      28 | 2990 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      15 | 2991 | `		}else{` |
|     ! 0 | 2992 | `			ph7_result_null(pCtx);` |
|       - | 2993 | `		}` |
|       - | 2994 | `		/* Rehash all int keys */` |
|      28 | 2995 | `		n = pMap->nEntry;` |
|      28 | 2996 | `		pEntry = pMap->pFirst;` |
|      28 | 2997 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 2998 | `		for(;;){` |
|      82 | 2999 | `			if( n < 1 ){` |
|      28 | 3000 | `				break;` |
|       - | 3001 | `			}` |
|      56 | 3002 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      56 | 3003 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 3004 | `			}` |
|       - | 3005 | `			/* Point to the next entry */` |
|      56 | 3006 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      56 | 3007 | `			n--;` |
|       2 | 3008 | `		}` |
|       - | 3009 | `		/* Reset the cursor */` |
|      28 | 3010 | `		pMap->pCur = pMap->pFirst;` |
|       - | 3011 | `	}` |
|      30 | 3012 | `	return PH7_OK;` |
|      21 | 3013 |  |
|       - | 3014 | `/*` |
|       - | 3015 | ` * Extract the node cursor value.` |
|       - | 3016 | ` */` |
|      24 | 3017 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 3018 |  |
|      25 | 3019 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 3020 | `	ph7_value *pVal;` |
|      25 | 3021 | `	if( pCur == 0 ){` |
|       - | 3022 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 3023 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3024 | `		return PH7_OK;` |
|       - | 3025 | `	}` |
|      25 | 3026 | `	if( iDirection != 0 ){` |
|       9 | 3027 | `		if( iDirection > 0 ){` |
|       - | 3028 | `			/* Point to the next entry */` |
|       7 | 3029 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 3030 | `			pCur = pMap->pCur;` |
|       4 | 3031 | `		}else{` |
|       - | 3032 | `			/* Point to the previous entry */` |
|       3 | 3033 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 3034 | `			pCur = pMap->pCur;` |
|       - | 3035 | `		}` |
|       9 | 3036 | `		if( pCur == 0 ){` |
|       - | 3037 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 3038 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3039 | `			return PH7_OK;` |
|       - | 3040 | `		}` |
|       4 | 3041 | `	}` |
|       - | 3042 | `	/* Point to the desired element */` |
|      25 | 3043 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 3044 | `	if( pVal ){` |
|      25 | 3045 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 3046 | `	}else{` |
|     ! 0 | 3047 | `		ph7_result_bool(pCtx,0);` |
|       - | 3048 | `	}` |
|      25 | 3049 | `	return PH7_OK;` |
|      13 | 3050 |  |
|       - | 3051 | `/*` |
|       - | 3052 | ` * value current(array $array)` |
|       - | 3053 | ` *  Return the current element in an array.` |
|       - | 3054 | ` * Parameter` |
|       - | 3055 | ` *  $input: The input array.` |
|       - | 3056 | ` * Return` |
|       - | 3057 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 3058 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3059 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3060 | ` *  is empty, current() returns FALSE.` |
|       - | 3061 | ` */` |
|      10 | 3062 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3063 |  |
|      11 | 3064 | `	if( nArg < 1 ){` |
|       - | 3065 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3066 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3067 | `		return PH7_OK;` |
|       - | 3068 | `	}` |
|       - | 3069 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 3070 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3071 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3072 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3073 | `		return PH7_OK;` |
|       - | 3074 | `	}` |
|      11 | 3075 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 3076 | `	return PH7_OK;` |
|       6 | 3077 |  |
|       - | 3078 | `/*` |
|       - | 3079 | ` * value next(array $input)` |
|       - | 3080 | ` *  Advance the internal array pointer of an array.` |
|       - | 3081 | ` * Parameter` |
|       - | 3082 | ` *  $input: The input array.` |
|       - | 3083 | ` * Return` |
|       - | 3084 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 3085 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 3086 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 3087 | ` */` |
|       6 | 3088 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3089 |  |
|       7 | 3090 | `	if( nArg < 1 ){` |
|       - | 3091 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3092 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3093 | `		return PH7_OK;` |
|       - | 3094 | `	}` |
|       - | 3095 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 3096 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3097 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3098 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3099 | `		return PH7_OK;` |
|       - | 3100 | `	}` |
|       7 | 3101 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 3102 | `	return PH7_OK;` |
|       4 | 3103 |  |
|       - | 3104 | `/*` |
|       - | 3105 | ` * value prev(array $input)` |
|       - | 3106 | ` *  Rewind the internal array pointer.` |
|       - | 3107 | ` * Parameter` |
|       - | 3108 | ` *  $input: The input array.` |
|       - | 3109 | ` * Return` |
|       - | 3110 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3111 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3112 | ` *  elements.` |
|       - | 3113 | ` */` |
|       2 | 3114 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3115 |  |
|       3 | 3116 | `	if( nArg < 1 ){` |
|       - | 3117 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3118 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3119 | `		return PH7_OK;` |
|       - | 3120 | `	}` |
|       - | 3121 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3122 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3123 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3124 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3125 | `		return PH7_OK;` |
|       - | 3126 | `	}` |
|       3 | 3127 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3128 | `	return PH7_OK;` |
|       2 | 3129 |  |
|       - | 3130 | `/*` |
|       - | 3131 | ` * value end(array $input)` |
|       - | 3132 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3133 | ` * Parameter` |
|       - | 3134 | ` *  $input: The input array.` |
|       - | 3135 | ` * Return` |
|       - | 3136 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3137 | ` */` |
|       2 | 3138 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3139 |  |
|       - | 3140 | `	ph7_hashmap *pMap;` |
|       3 | 3141 | `	if( nArg < 1 ){` |
|       - | 3142 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3143 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3144 | `		return PH7_OK;` |
|       - | 3145 | `	}` |
|       - | 3146 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3147 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3148 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3149 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3150 | `		return PH7_OK;` |
|       - | 3151 | `	}` |
|       - | 3152 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3153 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3154 | `	/* Point to the last node */` |
|       3 | 3155 | `	pMap->pCur = pMap->pLast;` |
|       - | 3156 | `	/* Return the last node value */` |
|       3 | 3157 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3158 | `	return PH7_OK;` |
|       2 | 3159 |  |
|       - | 3160 | `/*` |
|       - | 3161 | ` * value reset(array $array )` |
|       - | 3162 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3163 | ` * Parameter` |
|       - | 3164 | ` *  $input: The input array.` |
|       - | 3165 | ` * Return` |
|       - | 3166 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3167 | ` */` |
|       4 | 3168 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3169 |  |
|       - | 3170 | `	ph7_hashmap *pMap;` |
|       5 | 3171 | `	if( nArg < 1 ){` |
|       - | 3172 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3173 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3174 | `		return PH7_OK;` |
|       - | 3175 | `	}` |
|       - | 3176 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3177 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3178 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3179 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3180 | `		return PH7_OK;` |
|       - | 3181 | `	}` |
|       - | 3182 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3183 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3184 | `	/* Point to the first node */` |
|       5 | 3185 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3186 | `	/* Return the last node value if available */` |
|       5 | 3187 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3188 | `	return PH7_OK;` |
|       3 | 3189 |  |
|       - | 3190 | `/*` |
|       - | 3191 | ` * value key(array $array)` |
|       - | 3192 | ` *   Fetch a key from an array` |
|       - | 3193 | ` * Parameter` |
|       - | 3194 | ` *  $input` |
|       - | 3195 | ` *   The input array.` |
|       - | 3196 | ` * Return` |
|       - | 3197 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3198 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3199 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3200 | ` *  is empty, key() returns NULL.` |
|       - | 3201 | ` */` |
|       4 | 3202 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3203 |  |
|       - | 3204 | `	ph7_hashmap_node *pCur;` |
|       - | 3205 | `	ph7_hashmap *pMap;` |
|       5 | 3206 | `	if( nArg < 1 ){` |
|       - | 3207 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3208 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3209 | `		return PH7_OK;` |
|       - | 3210 | `	}` |
|       - | 3211 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3212 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3213 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3214 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3215 | `		return PH7_OK;` |
|       - | 3216 | `	}` |
|       5 | 3217 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3218 | `	pCur = pMap->pCur;` |
|       5 | 3219 | `	if( pCur == 0 ){` |
|       - | 3220 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3221 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3222 | `		return PH7_OK;` |
|       - | 3223 | `	}` |
|       5 | 3224 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3225 | `		/* Key is integer */` |
|     ! 0 | 3226 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3227 | `	}else{` |
|       - | 3228 | `		/* Key is blob */` |
|       7 | 3229 | `		ph7_result_string(pCtx,` |
|       4 | 3230 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3231 | `	}` |
|       5 | 3232 | `	return PH7_OK;` |
|       3 | 3233 |  |
|       - | 3234 | `/*` |
|       - | 3235 | ` * array each(array $input)` |
|       - | 3236 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3237 | ` * Parameter` |
|       - | 3238 | ` *  $input` |
|       - | 3239 | ` *    The input array.` |
|       - | 3240 | ` * Return` |
|       - | 3241 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3242 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3243 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3244 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3245 | ` *  each() returns FALSE.` |
|       - | 3246 | ` */` |
|      22 | 3247 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3248 |  |
|       - | 3249 | `	ph7_hashmap_node *pCur;` |
|       - | 3250 | `	ph7_hashmap *pMap;` |
|       - | 3251 | `	ph7_value *pArray;` |
|       - | 3252 | `	ph7_value *pVal;` |
|       - | 3253 | `	ph7_value sKey;` |
|      23 | 3254 | `	if( nArg < 1 ){` |
|       - | 3255 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3256 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3257 | `		return PH7_OK;` |
|       - | 3258 | `	}` |
|       - | 3259 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3260 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3261 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3262 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3263 | `		return PH7_OK;` |
|       - | 3264 | `	}` |
|       - | 3265 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3266 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3267 | `	if( pMap->pCur == 0 ){` |
|       - | 3268 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3269 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3270 | `		return PH7_OK;` |
|       - | 3271 | `	}` |
|      15 | 3272 | `	pCur = pMap->pCur;` |
|       - | 3273 | `	/* Create a new array */` |
|      15 | 3274 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3275 | `	if( pArray == 0 ){` |
|     ! 0 | 3276 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3277 | `		return PH7_OK;` |
|       - | 3278 | `	}` |
|      15 | 3279 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3280 | `	/* Insert the current value */` |
|      15 | 3281 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3282 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3283 | `	/* Make the key */` |
|      15 | 3284 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3285 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3286 | `	}else{` |
|       9 | 3287 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3288 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3289 | `	}` |
|       - | 3290 | `	/* Insert the current key */` |
|      15 | 3291 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3292 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3293 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3294 | `	/* Advance the cursor */` |
|      15 | 3295 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3296 | `	/* Return the current entry */` |
|      15 | 3297 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3298 | `	return PH7_OK;` |
|      12 | 3299 |  |
|       - | 3300 | `/*` |
|       - | 3301 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3302 | ` *  Create an array containing a range of elements` |
|       - | 3303 | ` * Parameter` |
|       - | 3304 | ` *  start` |
|       - | 3305 | ` *   First value of the sequence.` |
|       - | 3306 | ` *  limit` |
|       - | 3307 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3308 | ` *  step` |
|       - | 3309 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3310 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3311 | ` * Return` |
|       - | 3312 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3313 | ` * NOTE:` |
|       - | 3314 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3315 | ` */` |
|       2 | 3316 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3317 |  |
|       - | 3318 | `	ph7_value *pValue,*pArray;` |
|       - | 3319 | `	sxi64 iOfft,iLimit;` |
|       3 | 3320 | `	int iStep = 1;` |
|       - | 3321 |  |
|       3 | 3322 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3323 | `	if( nArg > 0 ){` |
|       - | 3324 | `		/* Extract the offset */` |
|       3 | 3325 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3326 | `		if( nArg > 1 ){` |
|       - | 3327 | `			/* Extract the limit */` |
|       3 | 3328 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3329 | `			if( nArg > 2 ){` |
|       - | 3330 | `				/* Extract the increment */` |
|       3 | 3331 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3332 | `				if( iStep < 1 ){` |
|       - | 3333 | `					/* Only positive number are allowed */` |
|       3 | 3334 | `					iStep = 1;` |
|       1 | 3335 | `				}` |
|       1 | 3336 | `			}` |
|       1 | 3337 | `		}` |
|       1 | 3338 | `	}` |
|       - | 3339 | `	/* Element container */` |
|       3 | 3340 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3341 | `	/* Create the new array */` |
|       3 | 3342 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3343 | `	if( pArray == 0 ){` |
|     ! 0 | 3344 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3345 | `	}` |
|       - | 3346 | `	/* Start filling */` |
|       3 | 3347 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3348 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3349 | `		/* Perform the insertion */` |
|     ! 0 | 3350 | `		if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       - | 3351 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3352 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3353 | `		}` |
|       - | 3354 | `		/* Increment */` |
|     ! 0 | 3355 | `		iOfft += iStep;` |
|     ! 0 | 3356 | `	}` |
|       - | 3357 | `	/* Return the new array */` |
|       3 | 3358 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3359 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3360 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3361 | `	 */` |
|       3 | 3362 | `	return PH7_OK;` |
|       2 | 3363 |  |
|       - | 3364 | `/*` |
|       - | 3365 | ` * array array_values(array $array)` |
|       - | 3366 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3367 | ` * Parameters` |
|       - | 3368 | ` *  $array` |
|       - | 3369 | ` *   The input array.` |
|       - | 3370 | ` * Return` |
|       - | 3371 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3372 | ` */` |
|      30 | 3373 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3374 |  |
|       - | 3375 | `	ph7_hashmap_node *pNode;` |
|       - | 3376 | `	ph7_hashmap *pMap;` |
|       - | 3377 | `	ph7_value *pArray;` |
|       - | 3378 | `	ph7_value *pObj;` |
|       - | 3379 | `	sxu32 n;` |
|      32 | 3380 | `	if( nArg != 1 ){` |
|       - | 3381 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3382 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3383 | `			"ArgumentCountError",` |
|       - | 3384 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3385 | `			nArg` |
|       - | 3386 | `			);` |
|       - | 3387 | `	}` |
|       - | 3388 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3389 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3390 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3391 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3392 | `			"TypeError",` |
|       - | 3393 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3394 | `			ph7_type_name(apArg[0])` |
|       - | 3395 | `			);` |
|       - | 3396 | `	}` |
|       - | 3397 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3398 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3399 | `	/* Create a new array */` |
|      25 | 3400 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3401 | `	if( pArray == 0 ){` |
|     ! 0 | 3402 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3403 | `		return PH7_OK;` |
|       - | 3404 | `	}` |
|       - | 3405 | `	/* Perform the requested operation */` |
|      25 | 3406 | `	pNode = pMap->pFirst;` |
|      83 | 3407 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3408 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3409 | `		if( pObj ){` |
|       - | 3410 | `			/* perform the insertion */` |
|      59 | 3411 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3412 | `		}` |
|       - | 3413 | `		/* Point to the next entry */` |
|      59 | 3414 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3415 | `	}` |
|       - | 3416 | `	/* return the new array */` |
|      25 | 3417 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3418 | `	return PH7_OK;` |
|      17 | 3419 |  |
|       - | 3420 | `/*` |
|       - | 3421 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3422 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3423 | ` * Parameters` |
|       - | 3424 | ` *  $input` |
|       - | 3425 | ` *   An array containing keys to return.` |
|       - | 3426 | ` * $search_value` |
|       - | 3427 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3428 | ` * $strict` |
|       - | 3429 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3430 | ` * Return` |
|       - | 3431 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3432 | ` */` |
|     122 | 3433 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3434 |  |
|       - | 3435 | `	ph7_hashmap_node *pNode;` |
|       - | 3436 | `	ph7_hashmap *pMap;` |
|       - | 3437 | `	ph7_value *pArray;` |
|       - | 3438 | `	ph7_value sObj;` |
|       - | 3439 | `	ph7_value sVal;` |
|       - | 3440 | `	SyString sKey;` |
|       - | 3441 | `	int bStrict;` |
|       - | 3442 | `	sxi32 rc;` |
|       - | 3443 | `	sxu32 n;` |
|     124 | 3444 | `	if( nArg < 1 ){` |
|       - | 3445 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3446 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3447 | `			"ArgumentCountError",` |
|       - | 3448 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3449 | `			);` |
|       - | 3450 | `	}` |
|       - | 3451 | `	/* Make sure we are dealing with a valid hashmap */` |
|     122 | 3452 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3453 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3454 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3455 | `			"TypeError",` |
|       - | 3456 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3457 | `			ph7_type_name(apArg[0])` |
|       - | 3458 | `			);` |
|       - | 3459 | `	}` |
|       - | 3460 | `	/* Point to the internal representation of the input hashmap */` |
|     120 | 3461 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3462 | `	/* Create a new array */` |
|     120 | 3463 | `	pArray = ph7_context_new_array(pCtx);` |
|     120 | 3464 | `	if( pArray == 0 ){` |
|     ! 0 | 3465 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3466 | `		return PH7_OK;` |
|       - | 3467 | `	}` |
|     120 | 3468 | `	bStrict = FALSE;` |
|     120 | 3469 | `	if( nArg > 2 ){` |
|       - | 3470 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3471 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3472 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3473 | `				"TypeError",` |
|       - | 3474 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3475 | `				ph7_type_name(apArg[2])` |
|       - | 3476 | `				);` |
|       - | 3477 | `		}` |
|       5 | 3478 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3479 | `	}` |
|       - | 3480 | `	/* Perform the requested operation */` |
|     117 | 3481 | `	pNode = pMap->pFirst;` |
|     117 | 3482 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     559 | 3483 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     443 | 3484 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     121 | 3485 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      61 | 3486 | `		}else{` |
|     323 | 3487 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3488 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3489 | `		}` |
|     443 | 3490 | `		rc = 0;` |
|     443 | 3491 | `		if( nArg > 1 ){` |
|      31 | 3492 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3493 | `			if( pValue ){` |
|      31 | 3494 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3495 | `				/* Filter key */` |
|      31 | 3496 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3497 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3498 | `			}` |
|      15 | 3499 | `		}` |
|     443 | 3500 | `		if( rc == 0 ){` |
|       - | 3501 | `			/* Perform the insertion */` |
|     425 | 3502 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     212 | 3503 | `		}` |
|     443 | 3504 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3505 | `		/* Point to the next entry */` |
|     443 | 3506 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     222 | 3507 | `	}` |
|       - | 3508 | `	/* return the new array */` |
|     117 | 3509 | `	ph7_result_value(pCtx,pArray);` |
|     117 | 3510 | `	return PH7_OK;` |
|      63 | 3511 |  |
|       - | 3512 | `/*` |
|       - | 3513 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3514 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3515 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3516 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3517 | ` * Parameters` |
|       - | 3518 | ` *  $arr1` |
|       - | 3519 | ` *   First array` |
|       - | 3520 | ` *  $arr2` |
|       - | 3521 | ` *   Second array` |
|       - | 3522 | ` * Return` |
|       - | 3523 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3524 | ` * Note` |
|       - | 3525 | ` *  This function is a symisc eXtension.` |
|       - | 3526 | ` */` |
|       4 | 3527 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3528 |  |
|       - | 3529 | `	ph7_hashmap *p1,*p2;` |
|       - | 3530 | `	int rc;` |
|       5 | 3531 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3532 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3533 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3534 | `		return PH7_OK;` |
|       - | 3535 | `	}` |
|       - | 3536 | `	/* Point to the hashmaps */` |
|       5 | 3537 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3538 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3539 | `	rc = (p1 == p2);` |
|       - | 3540 | `	/* Same instance? */` |
|       5 | 3541 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3542 | `	return PH7_OK;` |
|       3 | 3543 |  |
|       - | 3544 | `/*` |
|       - | 3545 | ` * array array_merge(array ...$arrays)` |
|       - | 3546 | ` *  Merge one or more arrays.` |
|       - | 3547 | ` * Parameters` |
|       - | 3548 | ` *  ...$arrays` |
|       - | 3549 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3550 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3551 | ` * Return` |
|       - | 3552 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3553 | ` *  with no arguments.` |
|       - | 3554 | ` */` |
|     986 | 3555 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3556 |  |
|       - | 3557 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3558 | `	ph7_value *pArray;` |
|       - | 3559 | `	int i;` |
|       - | 3560 | `	/* Create a new array */` |
|     988 | 3561 | `	pArray = ph7_context_new_array(pCtx);` |
|     988 | 3562 | `	if( pArray == 0 ){` |
|     ! 0 | 3563 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3564 | `		return PH7_OK;` |
|       - | 3565 | `	}` |
|       - | 3566 | `	/* Point to the internal representation of the hashmap */` |
|     988 | 3567 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3568 | `	/* Start merging */` |
|    2950 | 3569 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3570 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1968 | 3571 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3572 | `			/* Type mismatch -> TypeError */` |
|       7 | 3573 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3574 | `				"TypeError",` |
|       - | 3575 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3576 | `				i + 1,` |
|       4 | 3577 | `				ph7_type_name(apArg[i])` |
|       - | 3578 | `				);` |
|     ! 0 | 3579 | `		}else{` |
|    1964 | 3580 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3581 | `			/* Merge the two hashmaps */` |
|    1964 | 3582 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3583 | `		}` |
|     983 | 3584 | `	}` |
|       - | 3585 | `	/* Return the freshly created array */` |
|     984 | 3586 | `	ph7_result_value(pCtx,pArray);` |
|     984 | 3587 | `	return PH7_OK;` |
|     495 | 3588 |  |
|       - | 3589 | `/*` |
|       - | 3590 | ` * array array_copy(array $source)` |
|       - | 3591 | ` *  Make a blind copy of the target array.` |
|       - | 3592 | ` * Parameters` |
|       - | 3593 | ` *  $source` |
|       - | 3594 | ` *   Target array` |
|       - | 3595 | ` * Return` |
|       - | 3596 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3597 | ` * Note` |
|       - | 3598 | ` *  This function is a symisc eXtension.` |
|       - | 3599 | ` */` |
|      16 | 3600 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3601 |  |
|       - | 3602 | `	ph7_hashmap *pMap;` |
|       - | 3603 | `	ph7_value *pArray;` |
|      17 | 3604 | `	if( nArg < 1 ){` |
|       - | 3605 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3606 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3607 | `		return PH7_OK;` |
|       - | 3608 | `	}` |
|       - | 3609 | `	/* Create a new array */` |
|      17 | 3610 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3611 | `	if( pArray == 0 ){` |
|     ! 0 | 3612 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3613 | `		return PH7_OK;` |
|       - | 3614 | `	}` |
|       - | 3615 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3616 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3617 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3618 | `		/* Point to the internal representation of the source */` |
|      17 | 3619 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3620 | `		/* Perform the copy */` |
|      17 | 3621 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3622 | `	}else{` |
|       - | 3623 | `		/* Simple insertion */` |
|     ! 0 | 3624 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3625 | `	}` |
|       - | 3626 | `	/* Return the duplicated array */` |
|      17 | 3627 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3628 | `	return PH7_OK;` |
|       9 | 3629 |  |
|       - | 3630 | `/*` |
|       - | 3631 | ` * bool array_erase(array $source)` |
|       - | 3632 | ` *  Remove all elements from a given array.` |
|       - | 3633 | ` * Parameters` |
|       - | 3634 | ` *  $source` |
|       - | 3635 | ` *   Target array` |
|       - | 3636 | ` * Return` |
|       - | 3637 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3638 | ` * Note` |
|       - | 3639 | ` *  This function is a symisc eXtension.` |
|       - | 3640 | ` */` |
|      16 | 3641 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3642 |  |
|       - | 3643 | `	ph7_hashmap *pMap;` |
|      17 | 3644 | `	if( nArg < 1 ){` |
|       - | 3645 | `		/* Missing arguments */` |
|     ! 0 | 3646 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3647 | `		return PH7_OK;` |
|       - | 3648 | `	}` |
|       - | 3649 | `	/* Point to the target hashmap */` |
|      17 | 3650 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3651 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3652 | `	/* Erase */` |
|      17 | 3653 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3654 | `	return PH7_OK;` |
|       9 | 3655 |  |
|       - | 3656 | `/*` |
|       - | 3657 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3658 | ` *  Extract a slice of the array.` |
|       - | 3659 | ` * Parameters` |
|       - | 3660 | ` *  $array` |
|       - | 3661 | ` *    The input array.` |
|       - | 3662 | ` * $offset` |
|       - | 3663 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3664 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3665 | ` * $length (optional, nullable)` |
|       - | 3666 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3667 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3668 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3669 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3670 | ` * $preserve_keys (optional)` |
|       - | 3671 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3672 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3673 | ` * Return` |
|       - | 3674 | ` *   The new slice.` |
|       - | 3675 | ` */` |
|      46 | 3676 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3677 |  |
|       - | 3678 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3679 | `	ph7_hashmap_node *pCur;` |
|       - | 3680 | `	ph7_value *pArray;` |
|       - | 3681 | `	int iLength,iOfft;` |
|       - | 3682 | `	int bPreserve;` |
|       - | 3683 | `	sxi32 rc;` |
|      48 | 3684 | `	if( nArg < 2 ){` |
|       7 | 3685 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3686 | `			"ArgumentCountError",` |
|       - | 3687 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3688 | `			nArg` |
|       - | 3689 | `			);` |
|       - | 3690 | `	}` |
|      44 | 3691 | `	if( nArg > 4 ){` |
|       4 | 3692 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3693 | `			"ArgumentCountError",` |
|       - | 3694 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3695 | `			nArg` |
|       - | 3696 | `			);` |
|       - | 3697 | `	}` |
|      42 | 3698 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3699 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3700 | `			"TypeError",` |
|       - | 3701 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3702 | `			ph7_type_name(apArg[0])` |
|       - | 3703 | `			);` |
|       - | 3704 | `	}` |
|       - | 3705 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3706 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3707 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3708 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3709 | `			"TypeError",` |
|       - | 3710 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3711 | `			ph7_type_name(apArg[1])` |
|       - | 3712 | `			);` |
|       - | 3713 | `	}` |
|       - | 3714 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3715 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3716 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3717 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3718 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3719 | `				"TypeError",` |
|       - | 3720 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3721 | `				ph7_type_name(apArg[2])` |
|       - | 3722 | `				);` |
|       - | 3723 | `		}` |
|       8 | 3724 | `	}` |
|       - | 3725 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3726 | `	if( nArg > 3 ){` |
|      10 | 3727 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3728 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3729 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3730 | `				"TypeError",` |
|       - | 3731 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3732 | `				ph7_type_name(apArg[3])` |
|       - | 3733 | `				);` |
|       - | 3734 | `		}` |
|       2 | 3735 | `	}` |
|       - | 3736 | `	/* Point the internal representation of the target array */` |
|      33 | 3737 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3738 | `	bPreserve = FALSE;` |
|       - | 3739 | `	/* Get the offset */` |
|      33 | 3740 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3741 | `	if( iOfft < 0 ){` |
|       5 | 3742 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3743 | `		if( iOfft < 0 ){` |
|       3 | 3744 | `			iOfft = 0;` |
|       1 | 3745 | `		}` |
|       2 | 3746 | `	}` |
|      33 | 3747 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3748 | `		/* Offset past end of array, return empty array */` |
|       5 | 3749 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3750 | `		if( pArray == 0 ){` |
|     ! 0 | 3751 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3752 | `			return PH7_OK;` |
|       - | 3753 | `		}` |
|       5 | 3754 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3755 | `		return PH7_OK;` |
|       - | 3756 | `	}` |
|       - | 3757 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3758 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3759 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3760 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3761 | `		if( iLength < 0 ){` |
|       5 | 3762 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3763 | `		}` |
|      15 | 3764 | `		if( iLength < 0 ){` |
|       3 | 3765 | `			iLength = 0;` |
|       1 | 3766 | `		}` |
|      15 | 3767 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3768 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3769 | `		}` |
|       7 | 3770 | `	}` |
|      29 | 3771 | `	if( nArg > 3 ){` |
|       5 | 3772 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3773 | `	}` |
|       - | 3774 | `	/* Create a new array */` |
|      29 | 3775 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3776 | `	if( pArray == 0 ){` |
|     ! 0 | 3777 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3778 | `		return PH7_OK;` |
|       - | 3779 | `	}` |
|      29 | 3780 | `	if( iLength < 1 ){` |
|       - | 3781 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3782 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3783 | `		return PH7_OK;` |
|       - | 3784 | `	}` |
|       - | 3785 | `	/* Point to the desired entry */` |
|      25 | 3786 | `	pCur = pSrc->pFirst;` |
|      24 | 3787 | `	for(;;){` |
|      49 | 3788 | `		if( iOfft < 1 ){` |
|      25 | 3789 | `			break;` |
|       - | 3790 | `		}` |
|       - | 3791 | `		/* Point to the next entry */` |
|      25 | 3792 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3793 | `		iOfft--;` |
|       1 | 3794 | `	}` |
|       - | 3795 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3796 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3797 | `	for(;;){` |
|      79 | 3798 | `		if( iLength < 1 ){` |
|      25 | 3799 | `			break;` |
|       - | 3800 | `		}` |
|       - | 3801 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3802 | `		{` |
|      55 | 3803 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3804 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3805 | `		}` |
|      55 | 3806 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3807 | `			break;` |
|       - | 3808 | `		}` |
|       - | 3809 | `		/* Point to the next entry */` |
|      55 | 3810 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3811 | `		iLength--;` |
|       1 | 3812 | `	}` |
|       - | 3813 | `	/* Return the freshly created array */` |
|      25 | 3814 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3815 | `	return PH7_OK;` |
|      25 | 3816 |  |
|       - | 3817 | `/*` |
|       - | 3818 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3819 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3820 | ` * beginning (becomes the new pFirst).` |
|       - | 3821 | ` */` |
|      30 | 3822 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3823 |  |
|       - | 3824 | `	ph7_hashmap_node *pNode;` |
|       - | 3825 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3826 | `	pNode = pMap->pLast;` |
|      31 | 3827 | `	if( pNode == 0 ){` |
|     ! 0 | 3828 | `		return;` |
|       - | 3829 | `	}` |
|      31 | 3830 | `	if( pNode->pNext == 0 ){` |
|       - | 3831 | `		/* Only node in the list, nothing to move */` |
|       5 | 3832 | `		return;` |
|       - | 3833 | `	}` |
|      27 | 3834 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3835 | `		/* Already in the correct position */` |
|       9 | 3836 | `		return;` |
|       - | 3837 | `	}` |
|       - | 3838 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3839 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3840 | `	pMap->pLast->pPrev = 0;` |
|       - | 3841 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3842 | `	if( pAfter == 0 ){` |
|       - | 3843 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3844 | `		pNode->pNext = 0;` |
|       3 | 3845 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3846 | `		if( pMap->pFirst ){` |
|       3 | 3847 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3848 | `		}` |
|       3 | 3849 | `		pMap->pFirst = pNode;` |
|       2 | 3850 | `	}else{` |
|      17 | 3851 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3852 | `		pNode->pPrev = pOldNext;` |
|      17 | 3853 | `		pNode->pNext = pAfter;` |
|      17 | 3854 | `		pAfter->pPrev = pNode;` |
|      17 | 3855 | `		if( pOldNext ){` |
|      17 | 3856 | `			pOldNext->pNext = pNode;` |
|       9 | 3857 | `		}else{` |
|     ! 0 | 3858 | `			pMap->pLast = pNode;` |
|       - | 3859 | `		}` |
|       - | 3860 | `	}` |
|      16 | 3861 |  |
|       - | 3862 | `/*` |
|       - | 3863 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3864 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3865 | ` * Parameters` |
|       - | 3866 | ` *  $array` |
|       - | 3867 | ` *    The input array.` |
|       - | 3868 | ` *  $offset` |
|       - | 3869 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3870 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3871 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3872 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3873 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3874 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3875 | ` *  $length (optional)` |
|       - | 3876 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3877 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3878 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3879 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3880 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3881 | ` *  $replacement (optional)` |
|       - | 3882 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3883 | ` *    with elements from this array.` |
|       - | 3884 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3885 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3886 | ` *    offset.` |
|       - | 3887 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3888 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3889 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3890 | ` * Return` |
|       - | 3891 | ` *   A new array consisting of the extracted elements.` |
|       - | 3892 | ` */` |
|      54 | 3893 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3894 |  |
|       - | 3895 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3896 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3897 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3898 | `	int iLength,iOfft,i;` |
|       - | 3899 | `	sxi32 rc;` |
|      56 | 3900 | `	if( nArg < 2 ){` |
|       7 | 3901 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3902 | `			"ArgumentCountError",` |
|       - | 3903 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3904 | `			nArg` |
|       - | 3905 | `			);` |
|       - | 3906 | `	}` |
|      52 | 3907 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3908 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3909 | `			"TypeError",` |
|       - | 3910 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3911 | `			ph7_type_name(apArg[0])` |
|       - | 3912 | `			);` |
|       - | 3913 | `	}` |
|       - | 3914 | `	/* Point to the internal representation of the target array */` |
|      49 | 3915 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3916 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3917 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3918 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3919 | `	if( iOfft < 0 ){` |
|       7 | 3920 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3921 | `		if( iOfft < 0 ){` |
|       3 | 3922 | `			iOfft = 0;` |
|       2 | 3923 | `		}` |
|      46 | 3924 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3925 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3926 | `	}` |
|       - | 3927 | `	/* Get the length and clamp to valid range.` |
|       - | 3928 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3929 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3930 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3931 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3932 | `		if( iLength < 0 ){` |
|       7 | 3933 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3934 | `			if( iLength < 0 ){` |
|       3 | 3935 | `				iLength = 0;` |
|       1 | 3936 | `			}` |
|       3 | 3937 | `		}` |
|      31 | 3938 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3939 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3940 | `		}` |
|      15 | 3941 | `	}` |
|       - | 3942 | `	/* Create the result array for removed elements */` |
|      49 | 3943 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3944 | `	if( pArray == 0 ){` |
|     ! 0 | 3945 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3946 | `		return PH7_OK;` |
|       - | 3947 | `	}` |
|       - | 3948 | `	/* Get replacement array if provided */` |
|      49 | 3949 | `	pRep = 0;` |
|      49 | 3950 | `	if( nArg > 3 ){` |
|      21 | 3951 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3952 | `			/* Perform an array cast */` |
|       3 | 3953 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3954 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3955 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3956 | `			}` |
|       2 | 3957 | `		}else{` |
|      19 | 3958 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3959 | `		}` |
|      21 | 3960 | `		if( pRep ){` |
|       - | 3961 | `			/* Reset the loop cursor */` |
|      21 | 3962 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3963 | `		}` |
|      10 | 3964 | `	}` |
|       - | 3965 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3966 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3967 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3968 | `		return PH7_OK;` |
|       - | 3969 | `	}` |
|       - | 3970 | `	/* Navigate to the offset position */` |
|      41 | 3971 | `	pCur = pSrc->pFirst;` |
|      85 | 3972 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3973 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3974 | `	}` |
|       - | 3975 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3976 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3977 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3978 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3979 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3980 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3981 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3982 | `		pPrev = pCur->pPrev;` |
|      71 | 3983 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3984 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3985 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3986 | `			break;` |
|       - | 3987 | `		}` |
|      71 | 3988 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3989 | `	}` |
|       - | 3990 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3991 | `	if( pRep ){` |
|       - | 3992 | `		ph7_value sSafeVal;` |
|      61 | 3993 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3994 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3995 | `			if( pRvalue ){` |
|       - | 3996 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3997 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3998 | `				 * since it points into that same pool. */` |
|      31 | 3999 | `				sSafeVal = *pRvalue;` |
|      31 | 4000 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 4001 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 4002 | `					pNewNode = pSrc->pLast;` |
|      31 | 4003 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 4004 | `					pInsertAfter = pNewNode;` |
|      15 | 4005 | `				}` |
|      15 | 4006 | `			}` |
|       1 | 4007 | `		}` |
|      10 | 4008 | `	}` |
|       - | 4009 | `	/* Return the freshly created array */` |
|      41 | 4010 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 4011 | `	return PH7_OK;` |
|      29 | 4012 |  |
|       - | 4013 | `/*` |
|       - | 4014 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 4015 | ` *  Checks if a value exists in an array.` |
|       - | 4016 | ` * Parameters` |
|       - | 4017 | ` *  $needle` |
|       - | 4018 | ` *   The searched value.` |
|       - | 4019 | ` *   Note:` |
|       - | 4020 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 4021 | ` * $haystack` |
|       - | 4022 | ` *  The target array.` |
|       - | 4023 | ` * $strict` |
|       - | 4024 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 4025 | ` *  will also check the types of the needle in the haystack.` |
|       - | 4026 | ` */` |
|   29404 | 4027 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4028 |  |
|       - | 4029 | `	ph7_value *pNeedle;` |
|       - | 4030 | `	int bStrict;` |
|       - | 4031 | `	int rc;` |
|   29406 | 4032 | `	if( nArg < 2 ){` |
|       - | 4033 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 4034 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4035 | `		return PH7_OK;` |
|       - | 4036 | `	}` |
|   29406 | 4037 | `	pNeedle = apArg[0];` |
|   29406 | 4038 | `	bStrict = 0;` |
|   29406 | 4039 | `	if( nArg > 2 ){` |
|       5 | 4040 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 4041 | `	}` |
|   29406 | 4042 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4043 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 4044 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 4045 | `		/* Set the comparison result */` |
|     ! 0 | 4046 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 4047 | `		return PH7_OK;` |
|       - | 4048 | `	}` |
|       - | 4049 | `	/* Perform the lookup */` |
|   29406 | 4050 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 4051 | `	/* Lookup result */` |
|   29406 | 4052 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   29406 | 4053 | `	return PH7_OK;` |
|   14704 | 4054 |  |
|       - | 4055 | `/*` |
|       - | 4056 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 4057 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 4058 | ` * Parameters` |
|       - | 4059 | ` * $needle` |
|       - | 4060 | ` *   The searched value.` |
|       - | 4061 | ` * $haystack` |
|       - | 4062 | ` *   The array.` |
|       - | 4063 | ` * $strict` |
|       - | 4064 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 4065 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 4066 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 4067 | ` * Return` |
|       - | 4068 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 4069 | ` */` |
|      28 | 4070 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4071 |  |
|       - | 4072 | `	ph7_hashmap_node *pEntry;` |
|       - | 4073 | `	ph7_value *pVal,sNeedle;` |
|       - | 4074 | `	ph7_hashmap *pMap;` |
|       - | 4075 | `	ph7_value sVal;` |
|       - | 4076 | `	int bStrict;` |
|       - | 4077 | `	sxu32 n;` |
|       - | 4078 | `	int rc;` |
|      30 | 4079 | `	if( nArg < 2 ){` |
|       - | 4080 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 4081 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4082 | `			"ArgumentCountError",` |
|       - | 4083 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4084 | `			nArg` |
|       - | 4085 | `			);` |
|       - | 4086 | `	}` |
|      26 | 4087 | `	bStrict = FALSE;` |
|      26 | 4088 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4089 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4090 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4091 | `			"TypeError",` |
|       - | 4092 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4093 | `			ph7_type_name(apArg[1])` |
|       - | 4094 | `			);` |
|       - | 4095 | `	}` |
|      24 | 4096 | `	if( nArg > 2 ){` |
|       - | 4097 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4098 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4099 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4100 | `				"TypeError",` |
|       - | 4101 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4102 | `				ph7_type_name(apArg[2])` |
|       - | 4103 | `				);` |
|       - | 4104 | `		}` |
|       9 | 4105 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4106 | `	}` |
|       - | 4107 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4108 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4109 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4110 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4111 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4112 | `	pEntry = pMap->pFirst;` |
|      21 | 4113 | `	n = pMap->nEntry;` |
|      23 | 4114 | `	for(;;){` |
|      47 | 4115 | `		if( !n ){` |
|       9 | 4116 | `			break;` |
|       - | 4117 | `		}` |
|       - | 4118 | `		/* Extract node value */` |
|      39 | 4119 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4120 | `		if( pVal ){` |
|       - | 4121 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4122 | `			 * can change their type.` |
|       - | 4123 | `			 */` |
|      39 | 4124 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4125 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4126 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4127 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4128 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4129 | `			if( rc == 0 ){` |
|       - | 4130 | `				/* Match found,return key */` |
|      13 | 4131 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4132 | `					/* INT key */` |
|       7 | 4133 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4134 | `				}else{` |
|       7 | 4135 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4136 | `					/* Blob key */` |
|       7 | 4137 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4138 | `				}` |
|      13 | 4139 | `				return PH7_OK;` |
|       - | 4140 | `			}` |
|      13 | 4141 | `		}` |
|       - | 4142 | `		/* Point to the next entry */` |
|      27 | 4143 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4144 | `		n--;` |
|       1 | 4145 | `	}` |
|       - | 4146 | `	/* No such value,return FALSE */` |
|       9 | 4147 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4148 | `	return PH7_OK;` |
|      16 | 4149 |  |
|       - | 4150 | `/*` |
|       - | 4151 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4152 | ` *  Computes the difference of arrays.` |
|       - | 4153 | ` * Parameters` |
|       - | 4154 | ` *  $array1` |
|       - | 4155 | ` *    The array to compare from` |
|       - | 4156 | ` *  $array2` |
|       - | 4157 | ` *    An array to compare against` |
|       - | 4158 | ` *  $...` |
|       - | 4159 | ` *   More arrays to compare against` |
|       - | 4160 | ` * Return` |
|       - | 4161 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4162 | ` *  are not present in any of the other arrays.` |
|       - | 4163 | ` */` |
|      22 | 4164 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4165 |  |
|       - | 4166 | `	ph7_hashmap_node *pEntry;` |
|       - | 4167 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4168 | `	ph7_value *pArray;` |
|       - | 4169 | `	ph7_value *pVal;` |
|       - | 4170 | `	sxi32 rc;` |
|       - | 4171 | `	sxu32 n;` |
|       - | 4172 | `	int i;` |
|       - | 4173 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4174 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4175 | `	 * debugging difficult. */` |
|      24 | 4176 | `	if( nArg < 1 ){` |
|       4 | 4177 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4178 | `			"ArgumentCountError",` |
|       - | 4179 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4180 | `			nArg` |
|       - | 4181 | `			);` |
|       - | 4182 | `	}` |
|      22 | 4183 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4184 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4185 | `			"TypeError",` |
|       - | 4186 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4187 | `			ph7_type_name(apArg[0])` |
|       - | 4188 | `			);` |
|       - | 4189 | `	}` |
|      36 | 4190 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4191 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4192 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4193 | `				"TypeError",` |
|       - | 4194 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4195 | `				i + 1,` |
|       2 | 4196 | `				ph7_type_name(apArg[i])` |
|       - | 4197 | `				);` |
|       - | 4198 | `		}` |
|       9 | 4199 | `	}` |
|      17 | 4200 | `	if( nArg == 1 ){` |
|       - | 4201 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4202 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4203 | `		return PH7_OK;` |
|       - | 4204 | `	}` |
|       - | 4205 | `	/* Create a new array */` |
|      15 | 4206 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4207 | `	if( pArray == 0 ){` |
|     ! 0 | 4208 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4209 | `		return PH7_OK;` |
|       - | 4210 | `	}` |
|       - | 4211 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4212 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4213 | `	/* Perform the diff */` |
|      15 | 4214 | `	pEntry = pSrc->pFirst;` |
|      15 | 4215 | `	n = pSrc->nEntry;` |
|      27 | 4216 | `	for(;;){` |
|      55 | 4217 | `		if( n < 1 ){` |
|      15 | 4218 | `			break;` |
|       - | 4219 | `		}` |
|       - | 4220 | `		/* Extract the node value */` |
|      41 | 4221 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4222 | `		if( pVal ){` |
|      69 | 4223 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4224 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4225 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4226 | `				/* Perform the lookup */` |
|      45 | 4227 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4228 | `				if( rc == SXRET_OK ){` |
|       - | 4229 | `					/* Value exist */` |
|      17 | 4230 | `					break;` |
|       - | 4231 | `				}` |
|      15 | 4232 | `			}` |
|      41 | 4233 | `			if( i >= nArg ){` |
|       - | 4234 | `				/* Perform the insertion */` |
|      25 | 4235 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4236 | `			}` |
|      20 | 4237 | `		}` |
|       - | 4238 | `		/* Point to the next entry */` |
|      41 | 4239 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4240 | `		n--;` |
|       1 | 4241 | `	}` |
|       - | 4242 | `	/* Return the freshly created array */` |
|      15 | 4243 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4244 | `	return PH7_OK;` |
|      13 | 4245 |  |
|       - | 4246 | `/*` |
|       - | 4247 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4248 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4249 | ` * Parameters` |
|       - | 4250 | ` *  $array1` |
|       - | 4251 | ` *    The array to compare from` |
|       - | 4252 | ` *  $array2` |
|       - | 4253 | ` *    An array to compare against` |
|       - | 4254 | ` *  $...` |
|       - | 4255 | ` *   More arrays to compare against.` |
|       - | 4256 | ` * $callback` |
|       - | 4257 | ` *  The callback comparison function.` |
|       - | 4258 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4259 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4260 | ` *  than the second.` |
|       - | 4261 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4262 | ` * Return` |
|       - | 4263 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4264 | ` *  are not present in any of the other arrays.` |
|       - | 4265 | ` */` |
|      22 | 4266 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4267 |  |
|       - | 4268 | `	ph7_hashmap_node *pEntry;` |
|       - | 4269 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4270 | `	ph7_value *pCallback;` |
|       - | 4271 | `	ph7_value *pArray;` |
|       - | 4272 | `	ph7_value *pVal;` |
|       - | 4273 | `	sxi32 rc;` |
|       - | 4274 | `	sxu32 n;` |
|       - | 4275 | `	int i;` |
|       - | 4276 |  |
|       - | 4277 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      24 | 4278 | `	if( nArg < 2 ){` |
|       4 | 4279 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4280 | `			"ArgumentCountError",` |
|       - | 4281 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4282 | `			nArg` |
|       - | 4283 | `			);` |
|       - | 4284 | `	}` |
|      22 | 4285 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4286 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4287 | `			"TypeError",` |
|       - | 4288 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4289 | `			ph7_type_name(apArg[0])` |
|       - | 4290 | `			);` |
|       - | 4291 | `	}` |
|       - | 4292 |  |
|      20 | 4293 | `	if( nArg == 2 ){` |
|       - | 4294 | `		/* Only the original array and the callback were provided. */` |
|       - | 4295 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4296 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4297 | `		 * validation order.` |
|       - | 4298 | `		 */` |
|       4 | 4299 | `	} else {` |
|       - | 4300 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      24 | 4301 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      16 | 4302 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      10 | 4303 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4304 | `					"TypeError",` |
|       - | 4305 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4306 | `					i + 1,` |
|       6 | 4307 | `					ph7_type_name(apArg[i])` |
|       - | 4308 | `					);` |
|       - | 4309 | `			}` |
|       6 | 4310 | `		}` |
|       - | 4311 | `	}` |
|       - | 4312 |  |
|       - | 4313 | `	/* Identify the callback (always expected as the last argument). */` |
|      14 | 4314 | `	pCallback = apArg[nArg - 1];` |
|       - | 4315 | `	/* Validate the callback to match PHP's error messages. */` |
|      14 | 4316 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       7 | 4317 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4318 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4319 | `				"TypeError",` |
|       - | 4320 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4321 | `				nArg` |
|       - | 4322 | `				);` |
|       - | 4323 | `		}` |
|       5 | 4324 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4325 | `			int len;` |
|       3 | 4326 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4327 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4328 | `				"TypeError",` |
|       - | 4329 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4330 | `				nArg,` |
|       1 | 4331 | `				zName` |
|       - | 4332 | `				);` |
|       - | 4333 | `		}` |
|       4 | 4334 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4335 | `			"TypeError",` |
|       - | 4336 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4337 | `			nArg` |
|       - | 4338 | `			);` |
|       - | 4339 | `	}` |
|       - | 4340 |  |
|       7 | 4341 | `	if( nArg == 2 ){` |
|       - | 4342 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4343 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4344 | `		return PH7_OK;` |
|       - | 4345 | `	}` |
|       - | 4346 |  |
|       - | 4347 | `	/* Create a new array */` |
|       5 | 4348 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4349 | `	if( pArray == 0 ){` |
|     ! 0 | 4350 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4351 | `		return PH7_OK;` |
|       - | 4352 | `	}` |
|       - | 4353 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 4354 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4355 | `	/* Perform the diff */` |
|       5 | 4356 | `	pEntry = pSrc->pFirst;` |
|       5 | 4357 | `	n = pSrc->nEntry;` |
|       5 | 4358 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 4359 | `	for(;;){` |
|      11 | 4360 | `		if( n < 1 ){` |
|       3 | 4361 | `			break;` |
|       - | 4362 | `		}` |
|       - | 4363 | `		/* Extract the node value */` |
|       9 | 4364 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4365 | `		if( pVal ){` |
|      15 | 4366 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4367 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4368 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4369 | `				/* Perform the lookup */` |
|       9 | 4370 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       9 | 4371 | `				if( rc == SXRET_OK ){` |
|       - | 4372 | `					/* Value exist */` |
|       3 | 4373 | `					break;` |
|       - | 4374 | `				}` |
|       4 | 4375 | `			}` |
|       9 | 4376 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 4377 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 4378 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 4379 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 4380 | `				return PH7_EXCEPTION;` |
|       - | 4381 | `			}` |
|       7 | 4382 | `			if( i >= (nArg - 1)){` |
|       - | 4383 | `				/* Perform the insertion */` |
|       5 | 4384 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4385 | `			}` |
|       3 | 4386 | `		}` |
|       - | 4387 | `		/* Point to the next entry */` |
|       7 | 4388 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4389 | `		n--;` |
|       1 | 4390 | `	}` |
|       - | 4391 | `	/* Return the freshly created array */` |
|       3 | 4392 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4393 | `	return PH7_OK;` |
|      13 | 4394 |  |
|       - | 4395 | `/*` |
|       - | 4396 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4397 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4398 | ` * Parameters` |
|       - | 4399 | ` *  $array1` |
|       - | 4400 | ` *    The array to compare from` |
|       - | 4401 | ` *  $array2` |
|       - | 4402 | ` *    An array to compare against` |
|       - | 4403 | ` *  $...` |
|       - | 4404 | ` *   More arrays to compare against` |
|       - | 4405 | ` * Return` |
|       - | 4406 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4407 | ` *  are not present in any of the other arrays.` |
|       - | 4408 | ` */` |
|      20 | 4409 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4410 |  |
|       - | 4411 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4412 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4413 | `	ph7_value *pArray;` |
|       - | 4414 | `	ph7_value *pVal;` |
|       - | 4415 | `	sxi32 rc;` |
|       - | 4416 | `	sxu32 n;` |
|       - | 4417 | `	int i;` |
|       - | 4418 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4419 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4420 | `	 * accompanying integration tests to pass. */` |
|      22 | 4421 | `	if( nArg < 1 ){` |
|       4 | 4422 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4423 | `			"ArgumentCountError",` |
|       - | 4424 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4425 | `			nArg` |
|       - | 4426 | `			);` |
|       - | 4427 | `	}` |
|      20 | 4428 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4429 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4430 | `			"TypeError",` |
|       - | 4431 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4432 | `			ph7_type_name(apArg[0])` |
|       - | 4433 | `			);` |
|       - | 4434 | `	}` |
|      32 | 4435 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4436 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4437 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4438 | `				"TypeError",` |
|       - | 4439 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4440 | `				i + 1,` |
|       4 | 4441 | `				ph7_type_name(apArg[i])` |
|       - | 4442 | `				);` |
|       - | 4443 | `		}` |
|       9 | 4444 | `	}` |
|      13 | 4445 | `	if( nArg == 1 ){` |
|       - | 4446 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4447 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4448 | `		return PH7_OK;` |
|       - | 4449 | `	}` |
|       - | 4450 | `	/* Create a new array */` |
|      11 | 4451 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4452 | `	if( pArray == 0 ){` |
|     ! 0 | 4453 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4454 | `		return PH7_OK;` |
|       - | 4455 | `	}` |
|       - | 4456 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4457 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4458 | `	/* Perform the diff */` |
|      11 | 4459 | `	pEntry = pSrc->pFirst;` |
|      11 | 4460 | `	n = pSrc->nEntry;` |
|      11 | 4461 | `	pN1 = pN2 = 0;` |
|      29 | 4462 | `	for(;;){` |
|       - | 4463 | `		int keep;` |
|      35 | 4464 | `		if( n < 1 ){` |
|      11 | 4465 | `			break;` |
|       - | 4466 | `		}` |
|       - | 4467 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4468 | `		keep = 1;` |
|      41 | 4469 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4470 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4471 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4472 | `			/* Perform a key lookup first */` |
|      29 | 4473 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4474 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4475 | `			}else{` |
|      17 | 4476 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4477 | `			}` |
|      29 | 4478 | `			if( rc != SXRET_OK ){` |
|       - | 4479 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4480 | `				continue;` |
|       - | 4481 | `			}` |
|       - | 4482 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4483 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4484 | `			if( pVal ){` |
|       - | 4485 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4486 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4487 | `				if( pVal2 ){` |
|      15 | 4488 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4489 | `					if( cmp == 0 ){` |
|       - | 4490 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4491 | `						keep = 0;` |
|      13 | 4492 | `						break;` |
|       - | 4493 | `					}` |
|       1 | 4494 | `				}` |
|       1 | 4495 | `			}` |
|       2 | 4496 | `		}` |
|      25 | 4497 | `		if( keep ){` |
|       - | 4498 | `			/* Perform the insertion */` |
|      13 | 4499 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4500 | `		}` |
|       - | 4501 | `		/* Point to the next entry */` |
|      25 | 4502 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4503 | `		n--;` |
|       1 | 4504 | `	}` |
|       - | 4505 | `	/* Return the freshly created array */` |
|      11 | 4506 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4507 | `	return PH7_OK;` |
|      12 | 4508 |  |
|       - | 4509 | `/*` |
|       - | 4510 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4511 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4512 | ` *  by a user supplied callback function.` |
|       - | 4513 | ` * Parameters` |
|       - | 4514 | ` *  $array1` |
|       - | 4515 | ` *    The array to compare from` |
|       - | 4516 | ` *  $array2` |
|       - | 4517 | ` *    An array to compare against` |
|       - | 4518 | ` *  $...` |
|       - | 4519 | ` *   More arrays to compare against.` |
|       - | 4520 | ` *  $key_compare_func` |
|       - | 4521 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4522 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4523 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4524 | ` * Return` |
|       - | 4525 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4526 | ` *  are not present in any of the other arrays.` |
|       - | 4527 | ` */` |
|      24 | 4528 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4529 |  |
|       - | 4530 | `	ph7_hashmap_node *pEntry;` |
|       - | 4531 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4532 | `	ph7_value *pCallback;` |
|       - | 4533 | `	ph7_value *pArray;` |
|       - | 4534 | `	sxi32 rc;` |
|       - | 4535 | `	sxu32 n;` |
|       - | 4536 | `	int i;` |
|       - | 4537 |  |
|       - | 4538 | `	/* Argument validation mimicking PHP errors. */` |
|      26 | 4539 | `	if( nArg < 2 ){` |
|       4 | 4540 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4541 | `			"ArgumentCountError",` |
|       - | 4542 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4543 | `			nArg` |
|       - | 4544 | `			);` |
|       - | 4545 | `	}` |
|      24 | 4546 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4547 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4548 | `			"TypeError",` |
|       - | 4549 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4550 | `			ph7_type_name(apArg[0])` |
|       - | 4551 | `			);` |
|       - | 4552 | `	}` |
|       - | 4553 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4554 | `	 * expected to be a callback. */` |
|      36 | 4555 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      18 | 4556 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4557 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4558 | `				"TypeError",` |
|       - | 4559 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4560 | `				i + 1,` |
|       2 | 4561 | `				ph7_type_name(apArg[i])` |
|       - | 4562 | `				);` |
|       - | 4563 | `		}` |
|       9 | 4564 | `	}` |
|       - | 4565 | `	/* Point to the callback value */` |
|      20 | 4566 | `	pCallback = apArg[nArg - 1];` |
|      20 | 4567 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4568 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4569 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4570 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4571 | `		 * string given" which we also reproduce. */` |
|       7 | 4572 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4573 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4574 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4575 | `				"TypeError",` |
|       - | 4576 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4577 | `				nArg` |
|       - | 4578 | `				);` |
|       - | 4579 | `		}` |
|       5 | 4580 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4581 | `			/* neither array nor string */` |
|       7 | 4582 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4583 | `				"TypeError",` |
|       - | 4584 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4585 | `				nArg` |
|       - | 4586 | `				);` |
|       - | 4587 | `		}` |
|       - | 4588 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4589 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4590 | `			"TypeError",` |
|       - | 4591 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4592 | `			nArg,` |
|     ! 0 | 4593 | `			ph7_type_name(pCallback)` |
|       - | 4594 | `			);` |
|       - | 4595 | `	}` |
|      13 | 4596 | `	if( nArg == 2 ){` |
|       - | 4597 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4598 | `		 * input array. */` |
|       3 | 4599 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4600 | `		return PH7_OK;` |
|       - | 4601 | `	}` |
|       - | 4602 | `	/* Create a new array */` |
|      11 | 4603 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4604 | `	if( pArray == 0 ){` |
|     ! 0 | 4605 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4606 | `		return PH7_OK;` |
|       - | 4607 | `	}` |
|       - | 4608 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4609 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4610 | `	/* Perform the diff */` |
|      11 | 4611 | `	pEntry = pSrc->pFirst;` |
|      11 | 4612 | `	n = pSrc->nEntry;` |
|      21 | 4613 | `	for(;;){` |
|       - | 4614 | `		int keep;` |
|      27 | 4615 | `		if( n < 1 ){` |
|       9 | 4616 | `			break;` |
|       - | 4617 | `		}` |
|      19 | 4618 | `		keep = 1;` |
|      31 | 4619 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4620 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 4621 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4622 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 4623 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 4624 | `			while( pIt ){` |
|       - | 4625 | `				/* build temporary key values for callback */` |
|       - | 4626 | `				ph7_value key1, key2, result;` |
|       - | 4627 | `				/* initialise only once using the appropriate helper */` |
|      33 | 4628 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4629 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4630 | `				}else{` |
|       - | 4631 | `					SyString sStr;` |
|      33 | 4632 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4633 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4634 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 4635 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4636 | `				}` |
|      33 | 4637 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4638 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4639 | `				}else{` |
|       - | 4640 | `					SyString sStr;` |
|      33 | 4641 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4642 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4643 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 4644 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4645 | `				}` |
|      33 | 4646 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4647 | `				/* call user callback with (key1, key2) */` |
|       - | 4648 | `				{` |
|       - | 4649 | `					ph7_value *apK[2];` |
|      33 | 4650 | `					apK[0] = &key1;` |
|      33 | 4651 | `					apK[1] = &key2;` |
|      33 | 4652 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4653 | `				}` |
|      33 | 4654 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 4655 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 4656 | `					 * array_uintersect (which signal back from` |
|       - | 4657 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 4658 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 4659 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 4660 | `					PH7_MemObjRelease(&result);` |
|       3 | 4661 | `					PH7_MemObjRelease(&key1);` |
|       3 | 4662 | `					PH7_MemObjRelease(&key2);` |
|       3 | 4663 | `					return PH7_EXCEPTION;` |
|       - | 4664 | `				}` |
|      31 | 4665 | `				if( rc == SXRET_OK ){` |
|      31 | 4666 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4667 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4668 | `					}` |
|      31 | 4669 | `					if( result.x.iVal == 0 ){` |
|       - | 4670 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4671 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4672 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4673 | `						if( pVal1 && pVal2 ){` |
|      13 | 4674 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4675 | `								keep = 0;` |
|       9 | 4676 | `								PH7_MemObjRelease(&result);` |
|       - | 4677 | `								/* release keys too before breaking */` |
|       9 | 4678 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4679 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4680 | `								break;` |
|       - | 4681 | `							}` |
|       2 | 4682 | `						}` |
|       2 | 4683 | `					}` |
|      11 | 4684 | `				}` |
|      23 | 4685 | `				PH7_MemObjRelease(&result);` |
|      23 | 4686 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4687 | `				PH7_MemObjRelease(&key2);` |
|       - | 4688 | `				/* move to next node */` |
|      23 | 4689 | `				pIt = pIt->pPrev;` |
|      23 | 4690 | `				if( keep == 0 ) break;` |
|       1 | 4691 | `			}` |
|      21 | 4692 | `			if( keep == 0 ) break;` |
|       7 | 4693 | `		}` |
|      17 | 4694 | `		if( keep ){` |
|       - | 4695 | `			/* Perform the insertion */` |
|       9 | 4696 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4697 | `		}` |
|       - | 4698 | `		/* Point to the next entry */` |
|      17 | 4699 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4700 | `		n--;` |
|       1 | 4701 | `	}` |
|       - | 4702 | `	/* Return the freshly created array */` |
|       9 | 4703 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4704 | `	return PH7_OK;` |
|      14 | 4705 |  |
|       - | 4706 | `/*` |
|       - | 4707 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4708 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4709 | ` * Parameters` |
|       - | 4710 | ` *  $array1` |
|       - | 4711 | ` *    The array to compare from` |
|       - | 4712 | ` *  $array2` |
|       - | 4713 | ` *    An array to compare against` |
|       - | 4714 | ` *  $...` |
|       - | 4715 | ` *   More arrays to compare against` |
|       - | 4716 | ` * Return` |
|       - | 4717 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4718 | ` *  in any of the other arrays.` |
|       - | 4719 | ` * Note that NULL is returned on failure.` |
|       - | 4720 | ` */` |
|      14 | 4721 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4722 |  |
|       - | 4723 | `	ph7_hashmap_node *pEntry;` |
|       - | 4724 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4725 | `	ph7_value *pArray;` |
|       - | 4726 | `	sxi32 rc;` |
|       - | 4727 | `	sxu32 n;` |
|       - | 4728 | `	int i;` |
|       - | 4729 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4730 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4731 | `	 * helpers. */` |
|      16 | 4732 | `	if( nArg < 1 ){` |
|       4 | 4733 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4734 | `			"ArgumentCountError",` |
|       - | 4735 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4736 | `			nArg` |
|       - | 4737 | `			);` |
|       - | 4738 | `	}` |
|      14 | 4739 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4740 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4741 | `			"TypeError",` |
|       - | 4742 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4743 | `			ph7_type_name(apArg[0])` |
|       - | 4744 | `			);` |
|       - | 4745 | `	}` |
|      20 | 4746 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4747 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4748 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4749 | `				"TypeError",` |
|       - | 4750 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4751 | `				i + 1,` |
|       2 | 4752 | `				ph7_type_name(apArg[i])` |
|       - | 4753 | `				);` |
|       - | 4754 | `		}` |
|       5 | 4755 | `	}` |
|       9 | 4756 | `	if( nArg == 1 ){` |
|       - | 4757 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4758 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4759 | `		return PH7_OK;` |
|       - | 4760 | `	}` |
|       - | 4761 | `	/* Create a new array */` |
|       7 | 4762 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4763 | `	if( pArray == 0 ){` |
|     ! 0 | 4764 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4765 | `		return PH7_OK;` |
|       - | 4766 | `	}` |
|       - | 4767 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4768 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4769 | `	/* Perfrom the diff */` |
|       7 | 4770 | `	pEntry = pSrc->pFirst;` |
|       7 | 4771 | `	n = pSrc->nEntry;` |
|      12 | 4772 | `	for(;;){` |
|      25 | 4773 | `		if( n < 1 ){` |
|       7 | 4774 | `			break;` |
|       - | 4775 | `		}` |
|      31 | 4776 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4777 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4778 | `				/* ignore */` |
|     ! 0 | 4779 | `				continue;` |
|       - | 4780 | `			}` |
|      23 | 4781 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4782 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4783 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4784 | `				/* Blob lookup */` |
|      17 | 4785 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4786 | `			}else{` |
|       - | 4787 | `				/* Int lookup */` |
|       7 | 4788 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4789 | `			}` |
|      23 | 4790 | `			if( rc == SXRET_OK ){` |
|       - | 4791 | `				/* Key exists,break immediately */` |
|      11 | 4792 | `				break;` |
|       - | 4793 | `			}` |
|       7 | 4794 | `		}` |
|      19 | 4795 | `		if( i >= nArg ){` |
|       - | 4796 | `			/* Perform the insertion */` |
|       9 | 4797 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4798 | `		}` |
|       - | 4799 | `		/* Point to the next entry */` |
|      19 | 4800 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4801 | `		n--;` |
|       1 | 4802 | `	}` |
|       - | 4803 | `	/* Return the freshly created array */` |
|       7 | 4804 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4805 | `	return PH7_OK;` |
|       9 | 4806 |  |
|       - | 4807 | `/*` |
|       - | 4808 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4809 | ` *  Computes the intersection of arrays.` |
|       - | 4810 | ` * Parameters` |
|       - | 4811 | ` *  $array1` |
|       - | 4812 | ` *    The array to compare from` |
|       - | 4813 | ` *  $array2` |
|       - | 4814 | ` *    An array to compare against` |
|       - | 4815 | ` *  $...` |
|       - | 4816 | ` *   More arrays to compare against` |
|       - | 4817 | ` * Return` |
|       - | 4818 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4819 | ` *  in all of the parameters.` |
|       - | 4820 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4821 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4822 | ` */` |
|      22 | 4823 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4824 |  |
|       - | 4825 | `	ph7_hashmap_node *pEntry;` |
|       - | 4826 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4827 | `	ph7_value *pArray;` |
|       - | 4828 | `	ph7_value *pVal;` |
|       - | 4829 | `	sxi32 rc;` |
|       - | 4830 | `	sxu32 n;` |
|       - | 4831 | `	int i;` |
|      24 | 4832 | `	if( nArg < 1 ){` |
|       4 | 4833 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4834 | `			"ArgumentCountError",` |
|       - | 4835 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4836 | `			nArg` |
|       - | 4837 | `			);` |
|       - | 4838 | `	}` |
|      22 | 4839 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4840 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4841 | `			"TypeError",` |
|       - | 4842 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4843 | `			ph7_type_name(apArg[0])` |
|       - | 4844 | `			);` |
|       - | 4845 | `	}` |
|      36 | 4846 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4847 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4848 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4849 | `				"TypeError",` |
|       - | 4850 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4851 | `				i + 1,` |
|       2 | 4852 | `				ph7_type_name(apArg[i])` |
|       - | 4853 | `				);` |
|       - | 4854 | `		}` |
|       9 | 4855 | `	}` |
|      17 | 4856 | `	if( nArg == 1 ){` |
|       - | 4857 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4858 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4859 | `		return PH7_OK;` |
|       - | 4860 | `	}` |
|       - | 4861 | `	/* Create a new array */` |
|      15 | 4862 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4863 | `	if( pArray == 0 ){` |
|     ! 0 | 4864 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4865 | `		return PH7_OK;` |
|       - | 4866 | `	}` |
|       - | 4867 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4868 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4869 | `	/* Perform the intersection */` |
|      15 | 4870 | `	pEntry = pSrc->pFirst;` |
|      15 | 4871 | `	n = pSrc->nEntry;` |
|      31 | 4872 | `	for(;;){` |
|      63 | 4873 | `		if( n < 1 ){` |
|      15 | 4874 | `			break;` |
|       - | 4875 | `		}` |
|       - | 4876 | `		/* Extract the node value */` |
|      49 | 4877 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4878 | `		if( pVal ){` |
|      79 | 4879 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4880 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4881 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4882 | `				/* Perform the lookup */` |
|      55 | 4883 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4884 | `				if( rc != SXRET_OK ){` |
|       - | 4885 | `					/* Value does not exist */` |
|      25 | 4886 | `					break;` |
|       - | 4887 | `				}` |
|      16 | 4888 | `			}` |
|      49 | 4889 | `			if( i >= nArg ){` |
|       - | 4890 | `				/* Perform the insertion */` |
|      25 | 4891 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4892 | `			}` |
|      24 | 4893 | `		}` |
|       - | 4894 | `		/* Point to the next entry */` |
|      49 | 4895 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4896 | `		n--;` |
|       1 | 4897 | `	}` |
|       - | 4898 | `	/* Return the freshly created array */` |
|      15 | 4899 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4900 | `	return PH7_OK;` |
|      13 | 4901 |  |
|       - | 4902 | `/*` |
|       - | 4903 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4904 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4905 | ` * Parameters` |
|       - | 4906 | ` *  $array1` |
|       - | 4907 | ` *    The array to compare from` |
|       - | 4908 | ` *  $array2` |
|       - | 4909 | ` *    An array to compare against` |
|       - | 4910 | ` *  $...` |
|       - | 4911 | ` *   More arrays to compare against` |
|       - | 4912 | ` * Return` |
|       - | 4913 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4914 | ` *  in all the arguments, with matching keys.` |
|       - | 4915 | ` */` |
|      22 | 4916 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4917 |  |
|       - | 4918 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4919 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4920 | `	ph7_value *pArray;` |
|       - | 4921 | `	ph7_value *pVal;` |
|       - | 4922 | `	sxi32 rc;` |
|       - | 4923 | `	sxu32 n;` |
|       - | 4924 | `	int i;` |
|      24 | 4925 | `	if( nArg < 1 ){` |
|       4 | 4926 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4927 | `			"ArgumentCountError",` |
|       - | 4928 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4929 | `			nArg` |
|       - | 4930 | `			);` |
|       - | 4931 | `	}` |
|      22 | 4932 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4933 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4934 | `			"TypeError",` |
|       - | 4935 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4936 | `			ph7_type_name(apArg[0])` |
|       - | 4937 | `			);` |
|       - | 4938 | `	}` |
|      36 | 4939 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4940 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4941 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4942 | `				"TypeError",` |
|       - | 4943 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4944 | `				i + 1,` |
|       2 | 4945 | `				ph7_type_name(apArg[i])` |
|       - | 4946 | `				);` |
|       - | 4947 | `		}` |
|       9 | 4948 | `	}` |
|      17 | 4949 | `	if( nArg == 1 ){` |
|       - | 4950 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4951 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4952 | `		return PH7_OK;` |
|       - | 4953 | `	}` |
|       - | 4954 | `	/* Create a new array */` |
|      15 | 4955 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4956 | `	if( pArray == 0 ){` |
|     ! 0 | 4957 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4958 | `		return PH7_OK;` |
|       - | 4959 | `	}` |
|       - | 4960 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4961 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4962 | `	/* Perform the intersection */` |
|      15 | 4963 | `	pEntry = pSrc->pFirst;` |
|      15 | 4964 | `	n = pSrc->nEntry;` |
|      15 | 4965 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4966 | `	for(;;){` |
|      47 | 4967 | `		if( n < 1 ){` |
|      15 | 4968 | `			break;` |
|       - | 4969 | `		}` |
|       - | 4970 | `		/* Extract the node value */` |
|      33 | 4971 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4972 | `		if( pVal ){` |
|      53 | 4973 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4974 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4975 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4976 | `				/* Perform a key lookup first */` |
|      37 | 4977 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4978 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4979 | `				}else{` |
|      23 | 4980 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4981 | `				}` |
|      37 | 4982 | `				if( rc != SXRET_OK ){` |
|       - | 4983 | `					/* No such key,break immediately */` |
|       7 | 4984 | `					break;` |
|       - | 4985 | `				}` |
|       - | 4986 | `				/* Perform the lookup */` |
|      31 | 4987 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4988 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4989 | `					/* Value does not exist */` |
|       6 | 4990 | `					break;` |
|       - | 4991 | `				}` |
|      11 | 4992 | `			}` |
|      33 | 4993 | `			if( i >= nArg ){` |
|       - | 4994 | `				/* Perform the insertion */` |
|      17 | 4995 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4996 | `			}` |
|      16 | 4997 | `		}` |
|       - | 4998 | `		/* Point to the next entry */` |
|      33 | 4999 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5000 | `		n--;` |
|       1 | 5001 | `	}` |
|       - | 5002 | `	/* Return the freshly created array */` |
|      15 | 5003 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5004 | `	return PH7_OK;` |
|      13 | 5005 |  |
|       - | 5006 | `/*` |
|       - | 5007 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 5008 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 5009 | ` * Parameters` |
|       - | 5010 | ` *  $array1` |
|       - | 5011 | ` *    The array to compare from` |
|       - | 5012 | ` *  $...` |
|       - | 5013 | ` *   More arrays to compare against` |
|       - | 5014 | ` * Return` |
|       - | 5015 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 5016 | ` *  have keys that are present in all arguments.` |
|       - | 5017 | ` * Note that NULL is returned on failure.` |
|       - | 5018 | ` */` |
|      22 | 5019 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5020 |  |
|       - | 5021 | `	ph7_hashmap_node *pEntry;` |
|       - | 5022 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5023 | `	ph7_value *pArray;` |
|       - | 5024 | `	sxi32 rc;` |
|       - | 5025 | `	sxu32 n;` |
|       - | 5026 | `	int i;` |
|      24 | 5027 | `	if( nArg < 1 ){` |
|       4 | 5028 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5029 | `			"ArgumentCountError",` |
|       - | 5030 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 5031 | `			nArg` |
|       - | 5032 | `			);` |
|       - | 5033 | `	}` |
|      22 | 5034 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5035 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5036 | `			"TypeError",` |
|       - | 5037 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5038 | `			ph7_type_name(apArg[0])` |
|       - | 5039 | `			);` |
|       - | 5040 | `	}` |
|      36 | 5041 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5042 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5043 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5044 | `				"TypeError",` |
|       - | 5045 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5046 | `				i + 1,` |
|       2 | 5047 | `				ph7_type_name(apArg[i])` |
|       - | 5048 | `				);` |
|       - | 5049 | `		}` |
|       9 | 5050 | `	}` |
|      17 | 5051 | `	if( nArg == 1 ){` |
|       - | 5052 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5053 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5054 | `		return PH7_OK;` |
|       - | 5055 | `	}` |
|       - | 5056 | `	/* Create a new array */` |
|      15 | 5057 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5058 | `	if( pArray == 0 ){` |
|     ! 0 | 5059 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5060 | `		return PH7_OK;` |
|       - | 5061 | `	}` |
|       - | 5062 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 5063 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5064 | `	/* Perform the intersection */` |
|      15 | 5065 | `	pEntry = pSrc->pFirst;` |
|      15 | 5066 | `	n = pSrc->nEntry;` |
|      24 | 5067 | `	for(;;){` |
|      49 | 5068 | `		if( n < 1 ){` |
|      15 | 5069 | `			break;` |
|       - | 5070 | `		}` |
|      57 | 5071 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 5072 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 5073 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 5074 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5075 | `				/* Blob lookup */` |
|      27 | 5076 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 5077 | `			}else{` |
|       - | 5078 | `				/* Int key */` |
|      13 | 5079 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5080 | `			}` |
|      39 | 5081 | `			if( rc != SXRET_OK ){` |
|       - | 5082 | `				/* Key does not exist, break immediately */` |
|      17 | 5083 | `				break;` |
|       - | 5084 | `			}` |
|      12 | 5085 | `		}` |
|      35 | 5086 | `		if( i >= nArg ){` |
|       - | 5087 | `			/* Perform the insertion */` |
|      19 | 5088 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 5089 | `		}` |
|       - | 5090 | `		/* Point to the next entry */` |
|      35 | 5091 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 5092 | `		n--;` |
|       1 | 5093 | `	}` |
|       - | 5094 | `	/* Return the freshly created array */` |
|      15 | 5095 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5096 | `	return PH7_OK;` |
|      13 | 5097 |  |
|       - | 5098 | `/*` |
|       - | 5099 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 5100 | ` *  Computes the intersection of arrays.` |
|       - | 5101 | ` * Parameters` |
|       - | 5102 | ` *  $array1` |
|       - | 5103 | ` *    The array to compare from` |
|       - | 5104 | ` *  $array2` |
|       - | 5105 | ` *    An array to compare against` |
|       - | 5106 | ` *  $...` |
|       - | 5107 | ` *   More arrays to compare against` |
|       - | 5108 | ` * $callback` |
|       - | 5109 | ` *  The callback comparison function.` |
|       - | 5110 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5111 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5112 | ` *  than the second.` |
|       - | 5113 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5114 | ` * Return` |
|       - | 5115 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5116 | ` *  in all of the parameters. .` |
|       - | 5117 | ` * Note that NULL is returned on failure.` |
|       - | 5118 | ` */` |
|      26 | 5119 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5120 |  |
|       - | 5121 | `	ph7_hashmap_node *pEntry;` |
|       - | 5122 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5123 | `	ph7_value *pCallback;` |
|       - | 5124 | `	ph7_value *pArray;` |
|       - | 5125 | `	ph7_value *pVal;` |
|       - | 5126 | `	sxi32 rc;` |
|       - | 5127 | `	sxu32 n;` |
|       - | 5128 | `	int i;` |
|       - | 5129 |  |
|       - | 5130 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      28 | 5131 | `	if( nArg < 2 ){` |
|       4 | 5132 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5133 | `			"ArgumentCountError",` |
|       - | 5134 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5135 | `			nArg` |
|       - | 5136 | `			);` |
|       - | 5137 | `	}` |
|      26 | 5138 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5139 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5140 | `			"TypeError",` |
|       - | 5141 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5142 | `			ph7_type_name(apArg[0])` |
|       - | 5143 | `			);` |
|       - | 5144 | `	}` |
|       - | 5145 |  |
|      24 | 5146 | `	if( nArg == 2 ){` |
|       - | 5147 | `		/* Only the original array and the callback were provided. */` |
|       - | 5148 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5149 | `		 * validation ordering. */` |
|       3 | 5150 | `	} else {` |
|       - | 5151 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      36 | 5152 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      20 | 5153 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5154 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5155 | `					"TypeError",` |
|       - | 5156 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5157 | `					i + 1,` |
|       2 | 5158 | `					ph7_type_name(apArg[i])` |
|       - | 5159 | `					);` |
|       - | 5160 | `			}` |
|      10 | 5161 | `		}` |
|       - | 5162 | `	}` |
|       - | 5163 |  |
|       - | 5164 | `	/* Identify the callback (always expected as the last argument). */` |
|      22 | 5165 | `	pCallback = apArg[nArg - 1];` |
|       - | 5166 | `	/* Validate the callback to match PHP's error messages. */` |
|      22 | 5167 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      11 | 5168 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5169 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5170 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5171 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5172 | `			 */` |
|       7 | 5173 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       7 | 5174 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5175 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5176 | `					"TypeError",` |
|       - | 5177 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5178 | `					nArg` |
|       - | 5179 | `					);` |
|       - | 5180 | `			}` |
|       - | 5181 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5182 | `			{` |
|       5 | 5183 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       5 | 5184 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       5 | 5185 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5186 | `					int nMethodLen;` |
|       5 | 5187 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       5 | 5188 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       5 | 5189 | `					if( pClass ){` |
|       - | 5190 | `						/* Class exists but method is missing. */` |
|       4 | 5191 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5192 | `							"TypeError",` |
|       - | 5193 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5194 | `							nArg,` |
|       1 | 5195 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5196 | `							zMethod` |
|       - | 5197 | `							);` |
|       - | 5198 | `					}` |
|       - | 5199 | `					/* Class not found */` |
|       - | 5200 | `					{` |
|       - | 5201 | `						int nName;` |
|       3 | 5202 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5203 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5204 | `							"TypeError",` |
|       - | 5205 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5206 | `							nArg,` |
|       1 | 5207 | `							zName` |
|       - | 5208 | `							);` |
|       - | 5209 | `					}` |
|       - | 5210 | `				}` |
|       - | 5211 | `			}` |
|       - | 5212 | `			/* Fallback message */` |
|     ! 0 | 5213 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5214 | `				"TypeError",` |
|       - | 5215 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5216 | `				nArg` |
|       - | 5217 | `				);` |
|       - | 5218 | `		}` |
|       5 | 5219 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5220 | `			int len;` |
|       3 | 5221 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5222 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5223 | `				"TypeError",` |
|       - | 5224 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5225 | `				nArg,` |
|       1 | 5226 | `				zName` |
|       - | 5227 | `				);` |
|       - | 5228 | `		}` |
|       4 | 5229 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5230 | `			"TypeError",` |
|       - | 5231 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5232 | `			nArg` |
|       - | 5233 | `			);` |
|       - | 5234 | `	}` |
|       - | 5235 |  |
|      11 | 5236 | `	if( nArg == 2 ){` |
|       - | 5237 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5238 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5239 | `		return PH7_OK;` |
|       - | 5240 | `	}` |
|       - | 5241 |  |
|       - | 5242 | `	/* Create a new array */` |
|       7 | 5243 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5244 | `	if( pArray == 0 ){` |
|     ! 0 | 5245 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5246 | `		return PH7_OK;` |
|       - | 5247 | `	}` |
|       - | 5248 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 5249 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5250 | `	/* Perform the intersection */` |
|       7 | 5251 | `	pEntry = pSrc->pFirst;` |
|       7 | 5252 | `	n = pSrc->nEntry;` |
|       7 | 5253 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 5254 | `	for(;;){` |
|      19 | 5255 | `		if( n < 1 ){` |
|       5 | 5256 | `			break;` |
|       - | 5257 | `		}` |
|       - | 5258 | `		/* Extract the node value */` |
|      15 | 5259 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5260 | `		if( pVal ){` |
|      23 | 5261 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 5262 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5263 | `					/* ignore */` |
|     ! 0 | 5264 | `					continue;` |
|       - | 5265 | `				}` |
|       - | 5266 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 5267 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5268 | `				/* Perform the lookup */` |
|      15 | 5269 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 5270 | `				if( rc != SXRET_OK ){` |
|       - | 5271 | `					/* Value does not exist */` |
|       7 | 5272 | `					break;` |
|       - | 5273 | `				}` |
|       5 | 5274 | `			}` |
|      15 | 5275 | `			if( i >= (nArg-1) ){` |
|       - | 5276 | `				/* Perform the insertion */` |
|       9 | 5277 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5278 | `			}` |
|       7 | 5279 | `		}` |
|      15 | 5280 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 5281 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5282 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 5283 | `			return PH7_EXCEPTION;` |
|       - | 5284 | `		}` |
|       - | 5285 | `		/* Point to the next entry */` |
|      13 | 5286 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5287 | `		n--;` |
|       1 | 5288 | `	}` |
|       - | 5289 | `	/* Return the freshly created array */` |
|       5 | 5290 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5291 | `	return PH7_OK;` |
|      15 | 5292 |  |
|       - | 5293 | `/*` |
|       - | 5294 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5295 | ` *  Fill an array with values.` |
|       - | 5296 | ` * Parameters` |
|       - | 5297 | ` *  $start_index` |
|       - | 5298 | ` *    The first index of the returned array.` |
|       - | 5299 | ` *  $num` |
|       - | 5300 | ` *   Number of elements to insert.` |
|       - | 5301 | ` *  $value` |
|       - | 5302 | ` *    Value to use for filling.` |
|       - | 5303 | ` * Return` |
|       - | 5304 | ` *  The filled array or null on failure.` |
|       - | 5305 | ` */` |
|     238 | 5306 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5307 |  |
|       - | 5308 | `	ph7_value *pArray;` |
|       - | 5309 | `	int i,nEntry;` |
|       - | 5310 |  |
|       - | 5311 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 5312 | `	if( nArg != 3 ){` |
|       - | 5313 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 5314 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5315 | `			"ArgumentCountError",` |
|       - | 5316 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5317 | `			nArg` |
|       - | 5318 | `			);` |
|       - | 5319 | `	}` |
|       - | 5320 |  |
|       - | 5321 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5322 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5323 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5324 | `	 * and NULLs are rejected outright. */` |
|     466 | 5325 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 5326 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5327 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5328 | `			"TypeError",` |
|       - | 5329 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5330 | `			ph7_type_name(apArg[0])` |
|       - | 5331 | `			);` |
|       - | 5332 | `	}` |
|     234 | 5333 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5334 | `		int len;` |
|       8 | 5335 | `		sxu8 bReal = FALSE;` |
|       8 | 5336 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5337 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5338 | `			/* Non‑numeric string is an error. */` |
|       3 | 5339 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5340 | `				"TypeError",` |
|       - | 5341 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5342 | `				);` |
|       - | 5343 | `		}` |
|       5 | 5344 | `		if( bReal ){` |
|       - | 5345 | `			/* float-string -> deprecation warning */` |
|       4 | 5346 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5347 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5348 | `				zStr` |
|       - | 5349 | `				);` |
|       1 | 5350 | `		}` |
|       2 | 5351 | `	}` |
|       - | 5352 |  |
|       - | 5353 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5354 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 5355 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 5356 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5357 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5358 | `			"TypeError",` |
|       - | 5359 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5360 | `			ph7_type_name(apArg[1])` |
|       - | 5361 | `			);` |
|       - | 5362 | `	}` |
|     232 | 5363 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5364 | `		int len;` |
|       3 | 5365 | `		sxu8 bReal = FALSE;` |
|       3 | 5366 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5367 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5368 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5369 | `				"TypeError",` |
|       - | 5370 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5371 | `				);` |
|       - | 5372 | `		}` |
|     ! 0 | 5373 | `	}` |
|       - | 5374 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5375 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5376 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5377 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5378 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5379 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5380 | `		if( d != (double)i64 ){` |
|       7 | 5381 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5382 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5383 | `				d` |
|       - | 5384 | `				);` |
|       2 | 5385 | `		}` |
|       2 | 5386 | `	}` |
|       - | 5387 |  |
|       - | 5388 | `	/* Total number of entries to insert */` |
|     230 | 5389 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5390 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5391 | `	if( nEntry < 0 ){` |
|       3 | 5392 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5393 | `			"ValueError",` |
|       - | 5394 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5395 | `			);` |
|       - | 5396 | `	}` |
|       - | 5397 |  |
|       - | 5398 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5399 | `	if( nEntry == 0 ){` |
|       7 | 5400 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5401 | `		return PH7_OK;` |
|       - | 5402 | `	}` |
|       - | 5403 |  |
|       - | 5404 | `	/* Create a new array */` |
|     221 | 5405 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5406 | `	if( pArray == 0 ){` |
|     ! 0 | 5407 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5408 | `	}` |
|       - | 5409 |  |
|       - | 5410 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5411 | `	if( ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]) != SXRET_OK ){` |
|     ! 0 | 5412 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5413 | `	}` |
|       - | 5414 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5415 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5416 | `		if( ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]) != SXRET_OK ){` |
|       - | 5417 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 5418 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 5419 | `		}` |
| 1058682 | 5420 | `	}` |
|       - | 5421 | `	/* Return the filled array */` |
|     221 | 5422 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5423 | `	return PH7_OK;` |
|     121 | 5424 |  |
|       - | 5425 | `/*` |
|       - | 5426 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5427 | ` *  Fill an array with values, specifying keys.` |
|       - | 5428 | ` * Parameters` |
|       - | 5429 | ` *  $input` |
|       - | 5430 | ` *   Array of values that will be used as key.` |
|       - | 5431 | ` *  $value` |
|       - | 5432 | ` *    Value to use for filling.` |
|       - | 5433 | ` * Return` |
|       - | 5434 | ` *  The filled array.` |
|       - | 5435 | ` * Throws` |
|       - | 5436 | ` *  ValueError if $input is not an array.` |
|       - | 5437 | ` */` |
|      26 | 5438 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5439 |  |
|       - | 5440 | `	ph7_hashmap_node *pEntry;` |
|       - | 5441 | `	ph7_hashmap *pSrc;` |
|       - | 5442 | `	ph7_value *pArray;` |
|       - | 5443 | `	sxu32 n;` |
|       - | 5444 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5445 | `	if( nArg != 2 ){` |
|      10 | 5446 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5447 | `			"ArgumentCountError",` |
|       - | 5448 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5449 | `			nArg` |
|       - | 5450 | `			);` |
|       - | 5451 | `	}` |
|       - | 5452 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5453 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5454 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5455 | `			"TypeError",` |
|       - | 5456 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5457 | `			ph7_type_name(apArg[0])` |
|       - | 5458 | `			);` |
|       - | 5459 | `	}` |
|       - | 5460 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5461 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5462 | `	/* Create a new array */` |
|      17 | 5463 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5464 | `	if( pArray == 0 ){` |
|     ! 0 | 5465 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5466 | `		return PH7_OK;` |
|       - | 5467 | `	}` |
|       - | 5468 | `	/* Perform the requested operation */` |
|      17 | 5469 | `	pEntry = pSrc->pFirst;` |
|      45 | 5470 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5471 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5472 | `		/* Point to the next entry */` |
|      29 | 5473 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5474 | `	}` |
|       - | 5475 | `	/* Return the filled array */` |
|      17 | 5476 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5477 | `	return PH7_OK;` |
|      15 | 5478 |  |
|       - | 5479 | `/*` |
|       - | 5480 | ` * array array_combine(array $keys,array $values)` |
|       - | 5481 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5482 | ` * Parameters` |
|       - | 5483 | ` *  $keys` |
|       - | 5484 | ` *    Array of keys to be used.` |
|       - | 5485 | ` * $values` |
|       - | 5486 | ` *   Array of values to be used.` |
|       - | 5487 | ` * Return` |
|       - | 5488 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5489 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5490 | ` *  not an array.` |
|       - | 5491 | ` */` |
|      18 | 5492 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5493 |  |
|       - | 5494 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5495 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5496 | `	ph7_value *pArray;` |
|       - | 5497 | `	sxu32 n;` |
|       - | 5498 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5499 | `	if( nArg != 2 ){` |
|       - | 5500 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5501 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5502 | `			"ArgumentCountError",` |
|       - | 5503 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5504 | `			nArg` |
|       - | 5505 | `			);` |
|       - | 5506 | `	}` |
|       - | 5507 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5508 | `	 * argument index in the error message. */` |
|      18 | 5509 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5510 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5511 | `			"TypeError",` |
|       - | 5512 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5513 | `			ph7_type_name(apArg[0])` |
|       - | 5514 | `			);` |
|       - | 5515 | `	}` |
|      16 | 5516 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5517 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5518 | `			"TypeError",` |
|       - | 5519 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5520 | `			ph7_type_name(apArg[1])` |
|       - | 5521 | `			);` |
|       - | 5522 | `	}` |
|       - | 5523 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5524 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5525 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5526 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5527 | `		/* Length mismatch -> ValueError */` |
|       3 | 5528 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5529 | `			"ValueError",` |
|       - | 5530 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5531 | `			);` |
|       - | 5532 | `	}` |
|       - | 5533 | `	/* Create a new array */` |
|      11 | 5534 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5535 | `	if( pArray == 0 ){` |
|     ! 0 | 5536 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5537 | `		return PH7_OK;` |
|       - | 5538 | `	}` |
|       - | 5539 | `	/* Perform the requested operation */` |
|      11 | 5540 | `	pKe = pKey->pFirst;` |
|      11 | 5541 | `	pVe = pValue->pFirst;` |
|      33 | 5542 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5543 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5544 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5545 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5546 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5547 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5548 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5549 | `		 * original array must not be mutated. */` |
|      23 | 5550 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5551 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5552 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5553 | `			if( pTmpKey ){` |
|       5 | 5554 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5555 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5556 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5557 | `				pKeyCopy = pTmpKey;` |
|       2 | 5558 | `			}` |
|       2 | 5559 | `		}` |
|      23 | 5560 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5561 | `		/* Point to the next entry */` |
|      23 | 5562 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5563 | `		pVe = pVe->pPrev;` |
|      12 | 5564 | `	}` |
|       - | 5565 | `	/* Return the filled array */` |
|      11 | 5566 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5567 | `	return PH7_OK;` |
|      11 | 5568 |  |
|       - | 5569 | `/*` |
|       - | 5570 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5571 | ` *  Return an array with elements in reverse order.` |
|       - | 5572 | ` * Parameters` |
|       - | 5573 | ` *  $array` |
|       - | 5574 | ` *   The input array.` |
|       - | 5575 | ` *  $preserve_keys (optional)` |
|       - | 5576 | ` *   If set to TRUE keys are preserved.` |
|       - | 5577 | ` * Return` |
|       - | 5578 | ` *  The reversed array.` |
|       - | 5579 | ` */` |
|      20 | 5580 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5581 |  |
|       - | 5582 | `	ph7_hashmap_node *pEntry;` |
|       - | 5583 | `	ph7_hashmap *pSrc;` |
|       - | 5584 | `	ph7_value *pArray;` |
|       - | 5585 | `	int bPreserve;` |
|       - | 5586 | `	sxu32 n;` |
|      22 | 5587 | `	if( nArg < 1 ){` |
|       4 | 5588 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5589 | `			"ArgumentCountError",` |
|       - | 5590 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5591 | `			nArg` |
|       - | 5592 | `			);` |
|       - | 5593 | `	}` |
|       - | 5594 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5595 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5596 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5597 | `			"TypeError",` |
|       - | 5598 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5599 | `			ph7_type_name(apArg[0])` |
|       - | 5600 | `			);` |
|       - | 5601 | `	}` |
|      17 | 5602 | `	bPreserve = FALSE;` |
|      17 | 5603 | `	if( nArg > 1 ){` |
|       7 | 5604 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5605 | `	}` |
|       - | 5606 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5607 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5608 | `	/* Create a new array */` |
|      17 | 5609 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5610 | `	if( pArray == 0 ){` |
|     ! 0 | 5611 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5612 | `		return PH7_OK;` |
|       - | 5613 | `	}` |
|       - | 5614 | `	/* Perform the requested operation */` |
|      17 | 5615 | `	pEntry = pSrc->pLast;` |
|      55 | 5616 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5617 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5618 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5619 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5620 | `		/* Point to the previous entry */` |
|      39 | 5621 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5622 | `	}` |
|      17 | 5623 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5624 | `	return PH7_OK;` |
|      12 | 5625 |  |
|       - | 5626 | `/*` |
|       - | 5627 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5628 | ` *  Removes duplicate values from an array.` |
|       - | 5629 | ` * Parameters` |
|       - | 5630 | ` *  $array` |
|       - | 5631 | ` *   The input array.` |
|       - | 5632 | ` *  $flags` |
|       - | 5633 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5634 | ` *   behavior using these values:` |
|       - | 5635 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5636 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5637 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5638 | ` * Return` |
|       - | 5639 | ` *  The filtered array.` |
|       - | 5640 | ` */` |
|      24 | 5641 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5642 |  |
|       - | 5643 | `	ph7_hashmap_node *pEntry;` |
|       - | 5644 | `	ph7_value *pNeedle;` |
|       - | 5645 | `	ph7_hashmap *pSrc;` |
|       - | 5646 | `	ph7_value *pArray;` |
|       - | 5647 | `	int bStrict;` |
|       - | 5648 | `	sxi32 rc;` |
|       - | 5649 | `	sxu32 n;` |
|      26 | 5650 | `	if( nArg < 1 ){` |
|       - | 5651 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5652 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5653 | `			"ArgumentCountError",` |
|       - | 5654 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5655 | `			);` |
|       - | 5656 | `	}` |
|      24 | 5657 | `	if( nArg > 2 ){` |
|       - | 5658 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5659 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5660 | `			"ArgumentCountError",` |
|       - | 5661 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5662 | `			nArg` |
|       - | 5663 | `			);` |
|       - | 5664 | `	}` |
|       - | 5665 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5666 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5667 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5668 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5669 | `			"TypeError",` |
|       - | 5670 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5671 | `			ph7_type_name(apArg[0])` |
|       - | 5672 | `			);` |
|       - | 5673 | `	}` |
|      19 | 5674 | `	bStrict = FALSE;` |
|       - | 5675 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5676 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5677 | `	/* Create a new array */` |
|      19 | 5678 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5679 | `	if( pArray == 0 ){` |
|     ! 0 | 5680 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5681 | `		return PH7_OK;` |
|       - | 5682 | `	}` |
|       - | 5683 | `	/* Perform the requested operation */` |
|      19 | 5684 | `	pEntry = pSrc->pFirst;` |
|      83 | 5685 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5686 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5687 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5688 | `		if( pNeedle ){` |
|      65 | 5689 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5690 | `		}` |
|      65 | 5691 | `		if( rc != SXRET_OK ){` |
|       - | 5692 | `			/* Perform the insertion */` |
|      37 | 5693 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5694 | `		}` |
|       - | 5695 | `		/* Point to the next entry */` |
|      65 | 5696 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5697 | `	}` |
|       - | 5698 | `	/* Return the freshly created array */` |
|      19 | 5699 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5700 | `	return PH7_OK;` |
|      14 | 5701 |  |
|       - | 5702 | `/*` |
|       - | 5703 | ` * array array_flip(array $input)` |
|       - | 5704 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5705 | ` * Parameter` |
|       - | 5706 | ` *  $input` |
|       - | 5707 | ` *   Input array.` |
|       - | 5708 | ` * Return` |
|       - | 5709 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5710 | ` */` |
|      34 | 5711 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5712 |  |
|       - | 5713 | `	ph7_hashmap_node *pEntry;` |
|       - | 5714 | `	ph7_hashmap *pSrc;` |
|       - | 5715 | `	ph7_value *pArray;` |
|       - | 5716 | `	ph7_value *pKey;` |
|       - | 5717 | `	ph7_value sVal;` |
|       - | 5718 | `	sxu32 n;` |
|       - | 5719 |  |
|       - | 5720 | `	/* PHP requires exactly one argument */` |
|      36 | 5721 | `	if( nArg != 1 ){` |
|       - | 5722 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5723 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5724 | `			"ArgumentCountError",` |
|       - | 5725 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5726 | `			nArg` |
|       - | 5727 | `			);` |
|       - | 5728 | `	}` |
|       - | 5729 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5730 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5731 | `		/* Type mismatch -> TypeError */` |
|       7 | 5732 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5733 | `			"TypeError",` |
|       - | 5734 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5735 | `			ph7_type_name(apArg[0])` |
|       - | 5736 | `			);` |
|       - | 5737 | `	}` |
|       - | 5738 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5739 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5740 | `	/* Create a new array */` |
|      27 | 5741 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5742 | `	if( pArray == 0 ){` |
|     ! 0 | 5743 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5744 | `		return PH7_OK;` |
|       - | 5745 | `	}` |
|       - | 5746 | `	/* Start processing */` |
|      27 | 5747 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5748 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5749 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5750 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5751 | `		if( pKey ){` |
|       - | 5752 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5753 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5754 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5755 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5756 | `					);` |
|   22236 | 5757 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5758 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5759 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5760 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5761 | `				}else{` |
|       - | 5762 | `					SyString sStr;` |
|    2227 | 5763 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5764 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5765 | `				}` |
|       - | 5766 | `				/* Perform the insertion */` |
|   22227 | 5767 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5768 | `				/* Safely release the value because each inserted entry` |
|       - | 5769 | `				 * has its own private copy of the value.` |
|       - | 5770 | `				 */` |
|   22227 | 5771 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5772 | `			}else{` |
|       - | 5773 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5774 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5775 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5776 | `					);` |
|       - | 5777 | `			}` |
|   11118 | 5778 | `		}` |
|       - | 5779 | `		/* Point to the next entry */` |
|   22237 | 5780 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5781 | `	}` |
|       - | 5782 | `	/* Return the freshly created array */` |
|      27 | 5783 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5784 | `	return PH7_OK;` |
|      19 | 5785 |  |
|       - | 5786 | `/*` |
|       - | 5787 | ` * number array_sum(array $array )` |
|       - | 5788 | ` *  Calculate the sum of values in an array.` |
|       - | 5789 | ` * Parameters` |
|       - | 5790 | ` *  $array: The input array.` |
|       - | 5791 | ` * Return` |
|       - | 5792 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5793 | ` */` |
|      24 | 5794 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5795 |  |
|       - | 5796 | `	ph7_hashmap_node *pEntry;` |
|       - | 5797 | `	ph7_value *pObj;` |
|      25 | 5798 | `	double dSum = 0;` |
|       - | 5799 | `	sxu32 n;` |
|      25 | 5800 | `	pEntry = pMap->pFirst;` |
|      91 | 5801 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5802 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5803 | `		if( pObj ){` |
|      67 | 5804 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5805 | `				dSum += pObj->rVal;` |
|      53 | 5806 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5807 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5808 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5809 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5810 | `					double dv = 0;` |
|      13 | 5811 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5812 | `					dSum += dv;` |
|       7 | 5813 | `				}` |
|      12 | 5814 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5815 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5816 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5817 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5818 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5819 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5820 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5821 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5822 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5823 | `			}` |
|       - | 5824 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5825 | `		}` |
|       - | 5826 | `		/* Point to the next entry */` |
|      67 | 5827 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5828 | `	}` |
|       - | 5829 | `	/* Return sum */` |
|      25 | 5830 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5831 |  |
|      26 | 5832 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5833 |  |
|       - | 5834 | `	ph7_hashmap_node *pEntry;` |
|       - | 5835 | `	ph7_value *pObj;` |
|      28 | 5836 | `	sxi64 nSum = 0;` |
|       - | 5837 | `	sxu32 n;` |
|      28 | 5838 | `	pEntry = pMap->pFirst;` |
|     112 | 5839 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      86 | 5840 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      86 | 5841 | `		if( pObj ){` |
|      86 | 5842 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      76 | 5843 | `				nSum += pObj->x.iVal;` |
|      48 | 5844 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5845 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5846 | `					sxi64 nv = 0;` |
|       5 | 5847 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5848 | `					nSum += nv;` |
|       3 | 5849 | `				}` |
|       8 | 5850 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5851 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5852 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5853 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5854 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5855 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5856 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5857 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5858 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5859 | `			}` |
|       - | 5860 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      42 | 5861 | `		}` |
|       - | 5862 | `		/* Point to the next entry */` |
|      86 | 5863 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      44 | 5864 | `	}` |
|       - | 5865 | `	/* Return sum */` |
|      28 | 5866 | `	ph7_result_int64(pCtx,nSum);` |
|      28 | 5867 |  |
|       - | 5868 | `/* number array_sum(array $array )` |
|       - | 5869 | ` * (See block-coment above)` |
|       - | 5870 | ` */` |
|      64 | 5871 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5872 |  |
|       - | 5873 | `	ph7_hashmap_node *pEntry;` |
|       - | 5874 | `	ph7_hashmap *pMap;` |
|       - | 5875 | `	ph7_value *pObj;` |
|      66 | 5876 | `	int useDouble = 0;` |
|       - | 5877 | `	sxu32 n;` |
|       - | 5878 | `	/* PHP requires exactly one argument */` |
|      66 | 5879 | `	if( nArg != 1 ){` |
|       7 | 5880 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5881 | `			"ArgumentCountError",` |
|       - | 5882 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5883 | `			nArg` |
|       - | 5884 | `			);` |
|       - | 5885 | `	}` |
|       - | 5886 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 5887 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5888 | `		/* Type mismatch -> TypeError */` |
|       7 | 5889 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5890 | `			"TypeError",` |
|       - | 5891 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5892 | `			ph7_type_name(apArg[0])` |
|       - | 5893 | `			);` |
|       - | 5894 | `	}` |
|      58 | 5895 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      58 | 5896 | `	if( pMap->nEntry < 1 ){` |
|       - | 5897 | `		/* Nothing to compute,return 0 */` |
|       7 | 5898 | `		ph7_result_int(pCtx,0);` |
|       7 | 5899 | `		return PH7_OK;` |
|       - | 5900 | `	}` |
|       - | 5901 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5902 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5903 | `	 */` |
|      52 | 5904 | `	pEntry = pMap->pFirst;` |
|     144 | 5905 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     118 | 5906 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     118 | 5907 | `		if( pObj ){` |
|     118 | 5908 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5909 | `				useDouble = 1;` |
|      19 | 5910 | `				break;` |
|       - | 5911 | `			}` |
|     100 | 5912 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5913 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5914 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5915 | `				sxu32 i;` |
|      23 | 5916 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5917 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5918 | `						useDouble = 1;` |
|       7 | 5919 | `						break;` |
|       - | 5920 | `					}` |
|       6 | 5921 | `				}` |
|      13 | 5922 | `				if( useDouble ){` |
|       7 | 5923 | `					break;` |
|       - | 5924 | `				}` |
|       3 | 5925 | `			}` |
|      46 | 5926 | `		}` |
|      94 | 5927 | `		pEntry = pEntry->pPrev;` |
|      48 | 5928 | `	}` |
|      52 | 5929 | `	if( useDouble ){` |
|      25 | 5930 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5931 | `	}else{` |
|      28 | 5932 | `		Int64Sum(pCtx,pMap);` |
|       - | 5933 | `	}` |
|      52 | 5934 | `	return PH7_OK;` |
|      34 | 5935 |  |
|       - | 5936 | `/*` |
|       - | 5937 | ` * number array_product(array $array )` |
|       - | 5938 | ` *  Calculate the product of values in an array.` |
|       - | 5939 | ` * Parameters` |
|       - | 5940 | ` *  $array: The input array.` |
|       - | 5941 | ` * Return` |
|       - | 5942 | ` *  Returns the product of values as an integer or float.` |
|       - | 5943 | ` */` |
|     ! 0 | 5944 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5945 |  |
|       - | 5946 | `	ph7_hashmap_node *pEntry;` |
|       - | 5947 | `	ph7_value *pObj;` |
|       - | 5948 | `	double dProd;` |
|       - | 5949 | `	sxu32 n;` |
|     ! 0 | 5950 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5951 | `	dProd = 1;` |
|     ! 0 | 5952 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5953 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5954 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5955 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5956 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5957 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5958 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5959 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5960 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5961 | `					double dv = 0;` |
|     ! 0 | 5962 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5963 | `					dProd *= dv;` |
|     ! 0 | 5964 | `				}` |
|     ! 0 | 5965 | `			}` |
|     ! 0 | 5966 | `		}` |
|       - | 5967 | `		/* Point to the next entry */` |
|     ! 0 | 5968 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5969 | `	}` |
|       - | 5970 | `	/* Return product */` |
|     ! 0 | 5971 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5972 |  |
|     ! 0 | 5973 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5974 |  |
|       - | 5975 | `	ph7_hashmap_node *pEntry;` |
|       - | 5976 | `	ph7_value *pObj;` |
|       - | 5977 | `	sxi64 nProd;` |
|       - | 5978 | `	sxu32 n;` |
|     ! 0 | 5979 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5980 | `	nProd = 1;` |
|     ! 0 | 5981 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5982 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5983 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5984 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5985 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5986 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5987 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5988 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5989 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5990 | `					sxi64 nv = 0;` |
|     ! 0 | 5991 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5992 | `					nProd *= nv;` |
|     ! 0 | 5993 | `				}` |
|     ! 0 | 5994 | `			}` |
|     ! 0 | 5995 | `		}` |
|       - | 5996 | `		/* Point to the next entry */` |
|     ! 0 | 5997 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5998 | `	}` |
|       - | 5999 | `	/* Return product */` |
|     ! 0 | 6000 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 6001 |  |
|       - | 6002 | `/* number array_product(array $array )` |
|       - | 6003 | ` * (See block-block comment above)` |
|       - | 6004 | ` */` |
|     ! 0 | 6005 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 6006 |  |
|       - | 6007 | `	ph7_hashmap *pMap;` |
|       - | 6008 | `	ph7_value *pObj;` |
|     ! 0 | 6009 | `	if( nArg < 1 ){` |
|       - | 6010 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 6011 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6012 | `		return PH7_OK;` |
|       - | 6013 | `	}` |
|       - | 6014 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 6015 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6016 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 6017 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6018 | `		return PH7_OK;` |
|       - | 6019 | `	}` |
|     ! 0 | 6020 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 6021 | `	if( pMap->nEntry < 1 ){` |
|       - | 6022 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 6023 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6024 | `		return PH7_OK;` |
|       - | 6025 | `	}` |
|       - | 6026 | `	/* If the first element is of type float,then perform floating` |
|       - | 6027 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 6028 | `	 */` |
|     ! 0 | 6029 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 6030 | `	if( pObj == 0 ){` |
|     ! 0 | 6031 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6032 | `		return PH7_OK;` |
|       - | 6033 | `	}` |
|     ! 0 | 6034 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6035 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 6036 | `	}else{` |
|     ! 0 | 6037 | `		Int64Prod(pCtx,pMap);` |
|       - | 6038 | `	}` |
|     ! 0 | 6039 | `	return PH7_OK;` |
|     ! 0 | 6040 |  |
|       - | 6041 | `/*` |
|       - | 6042 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 6043 | ` *  Pick one or more random entries out of an array.` |
|       - | 6044 | ` * Parameters` |
|       - | 6045 | ` * $input` |
|       - | 6046 | ` *  The input array.` |
|       - | 6047 | ` * $num_req` |
|       - | 6048 | ` *  Specifies how many entries you want to pick.` |
|       - | 6049 | ` * Return` |
|       - | 6050 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 6051 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 6052 | ` *  NULL is returned on failure.` |
|       - | 6053 | ` */` |
|       6 | 6054 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6055 |  |
|       - | 6056 | `	ph7_hashmap_node *pNode;` |
|       - | 6057 | `	ph7_hashmap *pMap;` |
|       7 | 6058 | `	int nItem = 1;` |
|       7 | 6059 | `	if( nArg < 1 ){` |
|       - | 6060 | `		/* Missing argument,return NULL */` |
|     ! 0 | 6061 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6062 | `		return PH7_OK;` |
|       - | 6063 | `	}` |
|       - | 6064 | `	/* Make sure we are dealing with an array */` |
|       7 | 6065 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6066 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6067 | `		return PH7_OK;` |
|       - | 6068 | `	}` |
|       - | 6069 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 6070 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 6071 | `	if(pMap->nEntry < 1 ){` |
|       - | 6072 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 6073 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6074 | `		return PH7_OK;` |
|       - | 6075 | `	}` |
|       7 | 6076 | `	if( nArg > 1 ){` |
|       3 | 6077 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 6078 | `	}` |
|       7 | 6079 | `	if( nItem < 2 ){` |
|       - | 6080 | `		sxu32 nEntry;` |
|       - | 6081 | `		/* Select a random number */` |
|       5 | 6082 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 6083 | `		/* Extract the desired entry.` |
|       - | 6084 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 6085 | `		 */` |
|       5 | 6086 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 6087 | `			pNode = pMap->pLast;` |
|       3 | 6088 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 6089 | `			if( nEntry > 1 ){` |
|     ! 0 | 6090 | `				for(;;){` |
|     ! 0 | 6091 | `					if( nEntry == 0 ){` |
|     ! 0 | 6092 | `						break;` |
|       - | 6093 | `					}` |
|       - | 6094 | `					/* Point to the previous entry */` |
|     ! 0 | 6095 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 6096 | `					nEntry--;` |
|     ! 0 | 6097 | `				}` |
|     ! 0 | 6098 | `			}` |
|       2 | 6099 | `		}else{` |
|       3 | 6100 | `			pNode = pMap->pFirst;` |
|       1 | 6101 | `			for(;;){` |
|       4 | 6102 | `				if( nEntry == 0 ){` |
|       3 | 6103 | `					break;` |
|       - | 6104 | `				}` |
|       - | 6105 | `				/* Point to the next entry */` |
|       1 | 6106 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 6107 | `				nEntry--;` |
|     ! 0 | 6108 | `			}` |
|       - | 6109 | `		}` |
|       5 | 6110 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6111 | `			/* Int key */` |
|       3 | 6112 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6113 | `		}else{` |
|       - | 6114 | `			/* Blob key */` |
|       3 | 6115 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6116 | `		}` |
|       3 | 6117 | `	}else{` |
|       - | 6118 | `		ph7_value sKey,*pArray;` |
|       - | 6119 | `		ph7_hashmap *pDest;` |
|       - | 6120 | `		/* Create a new array */` |
|       3 | 6121 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6122 | `		if( pArray == 0 ){` |
|     ! 0 | 6123 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6124 | `			return PH7_OK;` |
|       - | 6125 | `		}` |
|       - | 6126 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6127 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6128 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6129 | `		/* Copy the first n items */` |
|       3 | 6130 | `		pNode = pMap->pFirst;` |
|       3 | 6131 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6132 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6133 | `		}` |
|       7 | 6134 | `		while( nItem > 0){` |
|       5 | 6135 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6136 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6137 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6138 | `			/* Point to the next entry */` |
|       5 | 6139 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6140 | `			nItem--;` |
|       1 | 6141 | `		}` |
|       - | 6142 | `		/* Shuffle the array */` |
|       3 | 6143 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6144 | `		/* Rehash node */` |
|       3 | 6145 | `		HashmapSortRehash(pDest);` |
|       - | 6146 | `		/* Return the random array */` |
|       3 | 6147 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6148 | `	}` |
|       7 | 6149 | `	return PH7_OK;` |
|       4 | 6150 |  |
|       - | 6151 | `/*` |
|       - | 6152 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6153 | ` *  Split an array into chunks.` |
|       - | 6154 | ` * Parameters` |
|       - | 6155 | ` * $input` |
|       - | 6156 | ` *   The array to work on` |
|       - | 6157 | ` * $size` |
|       - | 6158 | ` *   The size of each chunk` |
|       - | 6159 | ` * $preserve_keys` |
|       - | 6160 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6161 | ` *   the chunk numerically.` |
|       - | 6162 | ` * Return` |
|       - | 6163 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6164 | ` *  zero, with each dimension containing size elements.` |
|       - | 6165 | ` */` |
|      42 | 6166 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6167 |  |
|       - | 6168 | `	ph7_value *pArray,*pChunk;` |
|       - | 6169 | `	ph7_hashmap_node *pEntry;` |
|       - | 6170 | `	ph7_hashmap *pMap;` |
|       - | 6171 | `	int bPreserve;` |
|       - | 6172 | `	sxu32 nChunk;` |
|       - | 6173 | `	sxu32 nSize;` |
|       - | 6174 | `	sxu32 n;` |
|       - | 6175 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 6176 | `	if( nArg < 2 ){` |
|       - | 6177 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6178 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6179 | `			"ArgumentCountError",` |
|       - | 6180 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6181 | `			nArg` |
|       - | 6182 | `			);` |
|       - | 6183 | `	}` |
|      42 | 6184 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6185 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6186 | `			"TypeError",` |
|       - | 6187 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6188 | `			ph7_type_name(apArg[0])` |
|       - | 6189 | `			);` |
|       - | 6190 | `	}` |
|       - | 6191 | `	/* Create a new array */` |
|      40 | 6192 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 6193 | `	if( pArray == 0 ){` |
|     ! 0 | 6194 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6195 | `		return PH7_OK;` |
|       - | 6196 | `	}` |
|       - | 6197 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 6198 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6199 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6200 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 6201 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 6202 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6203 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6204 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6205 | `			"TypeError",` |
|       - | 6206 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6207 | `			ph7_type_name(apArg[1])` |
|       - | 6208 | `			);` |
|       - | 6209 | `	}` |
|       - | 6210 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6211 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6212 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 6213 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6214 | `		int len;` |
|       3 | 6215 | `		sxu8 bReal = FALSE;` |
|       3 | 6216 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6217 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6218 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6219 | `				"TypeError",` |
|       - | 6220 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6221 | `				);` |
|       - | 6222 | `		}` |
|     ! 0 | 6223 | `		if( bReal ){` |
|       - | 6224 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6225 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6226 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6227 | `				zStr` |
|       - | 6228 | `				);` |
|     ! 0 | 6229 | `		}` |
|     ! 0 | 6230 | `	}` |
|       - | 6231 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6232 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6233 | `	 * later via ph7_value_to_int. */` |
|      38 | 6234 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6235 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6236 | `		sxi64 i = (sxi64)d;` |
|       3 | 6237 | `		if( d != (double)i ){` |
|       4 | 6238 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6239 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6240 | `				d` |
|       - | 6241 | `				);` |
|       1 | 6242 | `		}` |
|       1 | 6243 | `	}` |
|       - | 6244 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6245 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6246 | `	{` |
|      38 | 6247 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 6248 | `		if( nSizeSigned < 1 ){` |
|       - | 6249 | `			/* size <= 0 -> ValueError */` |
|       5 | 6250 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6251 | `				"ValueError",` |
|       - | 6252 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6253 | `				);` |
|       - | 6254 | `		}` |
|      34 | 6255 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6256 | `	}` |
|      34 | 6257 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6258 | `		/* Return the whole array */` |
|       3 | 6259 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6260 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6261 | `		return PH7_OK;` |
|       - | 6262 | `	}` |
|      32 | 6263 | `	bPreserve = 0;` |
|      32 | 6264 | `	if( nArg > 2 ){` |
|       - | 6265 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6266 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6267 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6268 | `		 * normally, matching PHP behaviour. */` |
|      45 | 6269 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 6270 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6271 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 6272 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6273 | `				"TypeError",` |
|       - | 6274 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6275 | `				ph7_type_name(apArg[2])` |
|       - | 6276 | `				);` |
|       - | 6277 | `		}` |
|      21 | 6278 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6279 | `	}` |
|       - | 6280 | `	/* Start processing */` |
|      27 | 6281 | `	pEntry = pMap->pFirst;` |
|      27 | 6282 | `	nChunk = 0;` |
|      27 | 6283 | `	pChunk = 0;` |
|      27 | 6284 | `	n = pMap->nEntry;` |
|      56 | 6285 | `	for( ;; ){` |
|     113 | 6286 | `		if( n < 1 ){` |
|       - | 6287 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6288 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6289 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6290 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6291 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6292 | `			 * exists. */` |
|      27 | 6293 | `			if( pChunk ){` |
|      27 | 6294 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6295 | `			}` |
|      27 | 6296 | `			break;` |
|       - | 6297 | `		}` |
|      87 | 6298 | `		if( nChunk < 1 ){` |
|      71 | 6299 | `			if( pChunk ){` |
|       - | 6300 | `				/* Put the first chunk */` |
|      45 | 6301 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6302 | `			}` |
|       - | 6303 | `			/* Create a new dimension */` |
|      71 | 6304 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6305 | `												   * will be automatically released as soon we return` |
|       - | 6306 | `												   * from this function */` |
|      71 | 6307 | `			if( pChunk == 0 ){` |
|     ! 0 | 6308 | `				break;` |
|       - | 6309 | `			}` |
|      71 | 6310 | `			nChunk = nSize;` |
|      35 | 6311 | `		}` |
|       - | 6312 | `		/* Insert the entry */` |
|      87 | 6313 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6314 | `		/* Point to the next entry */` |
|      87 | 6315 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6316 | `		nChunk--;` |
|      87 | 6317 | `		n--;` |
|       1 | 6318 | `	}` |
|       - | 6319 | `	/* Return the multidimensional array */` |
|      27 | 6320 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6321 | `	return PH7_OK;` |
|      23 | 6322 |  |
|       - | 6323 | `/*` |
|       - | 6324 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6325 | ` *  Pad array to the specified length with a value.` |
|       - | 6326 | ` * $input` |
|       - | 6327 | ` *   Initial array of values to pad.` |
|       - | 6328 | ` * $pad_size` |
|       - | 6329 | ` *   New size of the array.` |
|       - | 6330 | ` * $pad_value` |
|       - | 6331 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6332 | ` */` |
|      28 | 6333 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6334 |  |
|       - | 6335 | `	ph7_hashmap *pMap;` |
|       - | 6336 | `	ph7_value *pArray;` |
|       - | 6337 | `	int nEntry;` |
|      30 | 6338 | `	if( nArg != 3 ){` |
|      10 | 6339 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6340 | `			"ArgumentCountError",` |
|       - | 6341 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6342 | `			nArg` |
|       - | 6343 | `			);` |
|       - | 6344 | `	}` |
|      24 | 6345 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6346 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6347 | `			"TypeError",` |
|       - | 6348 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6349 | `			ph7_type_name(apArg[0])` |
|       - | 6350 | `			);` |
|       - | 6351 | `	}` |
|       - | 6352 | `	/* Create a new array */` |
|      21 | 6353 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6354 | `	if( pArray == 0 ){` |
|     ! 0 | 6355 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6356 | `	}` |
|       - | 6357 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6358 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6359 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6360 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6361 | `	if( nEntry < 0 ){` |
|       9 | 6362 | `		nEntry = -nEntry;` |
|       9 | 6363 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6364 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6365 | `			/* Insert given items first */` |
|      17 | 6366 | `			while( nEntry > 0 ){` |
|      13 | 6367 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6368 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6369 | `				}` |
|      13 | 6370 | `				nEntry--;` |
|       1 | 6371 | `			}` |
|       - | 6372 | `			/* Merge the two arrays */` |
|       5 | 6373 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6374 | `		}else{` |
|       5 | 6375 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6376 | `		}` |
|      17 | 6377 | `	}else if( nEntry > 0 ){` |
|      11 | 6378 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6379 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6380 | `			/* Merge the two arrays first */` |
|       7 | 6381 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6382 | `			/* Insert given items */` |
|      25 | 6383 | `			while( nEntry > 0 ){` |
|      19 | 6384 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6385 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6386 | `				}` |
|      19 | 6387 | `				nEntry--;` |
|       1 | 6388 | `			}` |
|       4 | 6389 | `		}else{` |
|       5 | 6390 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6391 | `		}` |
|       6 | 6392 | `	}else{` |
|       - | 6393 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6394 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6395 | `	}` |
|       - | 6396 | `	/* Return the new array */` |
|      21 | 6397 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6398 | `	return PH7_OK;` |
|      16 | 6399 |  |
|       - | 6400 | `/*` |
|       - | 6401 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6402 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6403 | ` * Parameters` |
|       - | 6404 | ` * $array` |
|       - | 6405 | ` *   The array in which elements are replaced.` |
|       - | 6406 | ` * $array1` |
|       - | 6407 | ` *   The array from which elements will be extracted.` |
|       - | 6408 | ` * ....` |
|       - | 6409 | ` *  More arrays from which elements will be extracted.` |
|       - | 6410 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6411 | ` * Return` |
|       - | 6412 | ` *  Returns an array.` |
|       - | 6413 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6414 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6415 | ` */` |
|      22 | 6416 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6417 |  |
|       - | 6418 | `	ph7_hashmap *pMap;` |
|       - | 6419 | `	ph7_value *pArray;` |
|       - | 6420 | `	int i;` |
|      24 | 6421 | `	if( nArg < 1 ){` |
|       3 | 6422 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6423 | `			"ArgumentCountError",` |
|       - | 6424 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6425 | `			);` |
|       - | 6426 | `	}` |
|      22 | 6427 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6428 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6429 | `			"TypeError",` |
|       - | 6430 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6431 | `			ph7_type_name(apArg[0])` |
|       - | 6432 | `			);` |
|       - | 6433 | `	}` |
|       - | 6434 | `	/* Create a new array */` |
|      20 | 6435 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6436 | `	if( pArray == 0 ){` |
|     ! 0 | 6437 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6438 | `		return PH7_OK;` |
|       - | 6439 | `	}` |
|       - | 6440 | `	/* Overwrite from the first array */` |
|      20 | 6441 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6442 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6443 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6444 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6445 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6446 | `			/* Type mismatch -> TypeError */` |
|       4 | 6447 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6448 | `				"TypeError",` |
|       - | 6449 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6450 | `				i + 1,` |
|       2 | 6451 | `				ph7_type_name(apArg[i])` |
|       - | 6452 | `				);` |
|       - | 6453 | `		}` |
|       - | 6454 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6455 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6456 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6457 | `	}` |
|       - | 6458 | `	/* Return the new array */` |
|      17 | 6459 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6460 | `	return PH7_OK;` |
|      13 | 6461 |  |
|       - | 6462 | `/*` |
|       - | 6463 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6464 | ` *  Filters elements of an array using a callback function.` |
|       - | 6465 | ` * Parameters` |
|       - | 6466 | ` *  $input` |
|       - | 6467 | ` *    The array to iterate over` |
|       - | 6468 | ` * $callback` |
|       - | 6469 | ` *    The callback function to use` |
|       - | 6470 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6471 | ` *    will be removed.` |
|       - | 6472 | ` * Return` |
|       - | 6473 | ` *  The filtered array.` |
|       - | 6474 | ` */` |
|      20 | 6475 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6476 |  |
|       - | 6477 | `	ph7_hashmap_node *pEntry;` |
|       - | 6478 | `	ph7_hashmap *pMap;` |
|       - | 6479 | `	ph7_value *pArray;` |
|       - | 6480 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6481 | `	ph7_value *pValue;` |
|       - | 6482 | `	sxi32 rc;` |
|       - | 6483 | `	int keep;` |
|       - | 6484 | `	sxu32 n;` |
|      22 | 6485 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6486 | `		/* Invalid arguments,return NULL */` |
|       5 | 6487 | `		ph7_result_null(pCtx);` |
|       5 | 6488 | `		return PH7_OK;` |
|       - | 6489 | `	}` |
|       - | 6490 | `	/* Create a new array */` |
|      18 | 6491 | `	pArray = ph7_context_new_array(pCtx);` |
|      18 | 6492 | `	if( pArray == 0 ){` |
|     ! 0 | 6493 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6494 | `		return PH7_OK;` |
|       - | 6495 | `	}` |
|       - | 6496 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 6497 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      18 | 6498 | `	pEntry = pMap->pFirst;` |
|      18 | 6499 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      18 | 6500 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6501 | `	/* Perform the requested operation */` |
|      68 | 6502 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6503 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      56 | 6504 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      56 | 6505 | `		if( pValue == 0 ){` |
|       - | 6506 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6507 | `			keep = FALSE;` |
|      56 | 6508 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6509 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6510 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6511 | `				* silently dropped the element.  Emit similar message. */` |
|      28 | 6512 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6513 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6514 | `					int len;` |
|       3 | 6515 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6516 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6517 | `						"TypeError",` |
|       - | 6518 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6519 | `						zName` |
|       - | 6520 | `						);` |
|     ! 0 | 6521 | `				}else{` |
|     ! 0 | 6522 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6523 | `						"TypeError",` |
|       - | 6524 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6525 | `						ph7_type_name(apArg[1])` |
|       - | 6526 | `						);` |
|       - | 6527 | `				}` |
|       - | 6528 | `			}` |
|      25 | 6529 | `			keep = FALSE;` |
|      25 | 6530 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      25 | 6531 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 6532 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6533 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 6534 | `				return PH7_EXCEPTION;` |
|       - | 6535 | `			}` |
|      23 | 6536 | `			if( rc == SXRET_OK ){` |
|       - | 6537 | `				/* Perform a boolean cast */` |
|      23 | 6538 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6539 | `			}` |
|      23 | 6540 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6541 | `		}else{` |
|       - | 6542 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6543 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6544 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6545 | `			 */` |
|      29 | 6546 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6547 | `		}` |
|      51 | 6548 | `		if( keep ){` |
|       - | 6549 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6550 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6551 | `		}` |
|       - | 6552 | `		/* Point to the next entry */` |
|      51 | 6553 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6554 | `	}` |
|      13 | 6555 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6556 | `	return PH7_OK;` |
|      12 | 6557 |  |
|       - | 6558 | `/*` |
|       - | 6559 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 6560 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 6561 | ` * Parameters` |
|       - | 6562 | ` *  $callback` |
|       - | 6563 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 6564 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 6565 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 6566 | ` *   are zipped together.` |
|       - | 6567 | ` *  $array` |
|       - | 6568 | ` *   The first array to run through the callback function.` |
|       - | 6569 | ` *  $arrays` |
|       - | 6570 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 6571 | ` * Return` |
|       - | 6572 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 6573 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 6574 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 6575 | ` *  padding shorter arrays with NULL.` |
|       - | 6576 | ` */` |
|      46 | 6577 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6578 |  |
|       - | 6579 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6580 | `	ph7_hashmap_node *pEntry;` |
|       - | 6581 | `	ph7_hashmap *pMap;` |
|       - | 6582 | `	ph7_vm *pVm;` |
|       - | 6583 | `	int bNullCallback;` |
|       - | 6584 | `	sxi32 rc;` |
|       - | 6585 | `	int i;` |
|       - | 6586 | `	sxu32 n;` |
|      48 | 6587 | `	if( nArg < 2 ){` |
|       7 | 6588 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6589 | `			"ArgumentCountError",` |
|       - | 6590 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6591 | `			nArg` |
|       - | 6592 | `			);` |
|       - | 6593 | `	}` |
|      44 | 6594 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      44 | 6595 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6596 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6597 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6598 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6599 | `				"TypeError",` |
|       - | 6600 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6601 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6602 | `				zFunc` |
|       - | 6603 | `				);` |
|       - | 6604 | `		}` |
|       3 | 6605 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6606 | `			"TypeError",` |
|       - | 6607 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6608 | `			"no array or string given"` |
|       - | 6609 | `			);` |
|       - | 6610 | `	}` |
|       - | 6611 | `	/* Every remaining argument must be an array */` |
|      88 | 6612 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      52 | 6613 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 6614 | `			if( i == 1 ){` |
|       4 | 6615 | `				return PH7_VmThrowException(pCtx,` |
|       - | 6616 | `					"TypeError",` |
|       - | 6617 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6618 | `					ph7_type_name(apArg[1])` |
|       - | 6619 | `					);` |
|       - | 6620 | `			}` |
|     ! 0 | 6621 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6622 | `				"TypeError",` |
|       - | 6623 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 6624 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 6625 | `				);` |
|       - | 6626 | `		}` |
|      26 | 6627 | `	}` |
|      38 | 6628 | `	pVm = pCtx->pVm;` |
|       - | 6629 | `	/* Create a new array */` |
|      38 | 6630 | `	pArray = ph7_context_new_array(pCtx);` |
|      38 | 6631 | `	if( pArray == 0 ){` |
|     ! 0 | 6632 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6633 | `		return PH7_OK;` |
|       - | 6634 | `	}` |
|      38 | 6635 | `	PH7_MemObjInit(pVm,&sResult);` |
|      38 | 6636 | `	PH7_MemObjInit(pVm,&sKey);` |
|      38 | 6637 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      38 | 6638 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      38 | 6639 | `	if( nArg == 2 ){` |
|       - | 6640 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      28 | 6641 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      28 | 6642 | `		pEntry = pMap->pFirst;` |
|      82 | 6643 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6644 | `			/* Extract the node value */` |
|      58 | 6645 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|      58 | 6646 | `			if( pValue ){` |
|       - | 6647 | `				/* Extract the node key */` |
|      58 | 6648 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      58 | 6649 | `				if( bNullCallback ){` |
|       - | 6650 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 6651 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6652 | `				}else{` |
|       - | 6653 | `					/* Invoke the supplied callback */` |
|      48 | 6654 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|      48 | 6655 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 6656 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 6657 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       3 | 6658 | `						PH7_MemObjRelease(&sKey);` |
|       3 | 6659 | `						PH7_MemObjRelease(&sResult);` |
|       3 | 6660 | `						return PH7_EXCEPTION;` |
|       - | 6661 | `					}` |
|       - | 6662 | `					/* Insert the callback return value */` |
|      46 | 6663 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6664 | `				}` |
|      56 | 6665 | `				PH7_MemObjRelease(&sKey);` |
|      56 | 6666 | `				PH7_MemObjRelease(&sResult);` |
|      27 | 6667 | `			}` |
|       - | 6668 | `			/* Point to the next entry */` |
|      56 | 6669 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 6670 | `		}` |
|      14 | 6671 | `	}else{` |
|       - | 6672 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 6673 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 6674 | `		int nArrays = nArg - 1;` |
|       - | 6675 | `		ph7_hashmap_node **apCur;` |
|       - | 6676 | `		ph7_value **apCallArg;` |
|       - | 6677 | `		ph7_value sNull;` |
|      11 | 6678 | `		sxu32 nMax = 0;` |
|      11 | 6679 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 6680 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 6681 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 6682 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 6683 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 6684 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6685 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6686 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 6687 | `			return PH7_OK;` |
|       - | 6688 | `		}` |
|      11 | 6689 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 6690 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 6691 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 6692 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 6693 | `			apCur[i] = pMap->pFirst;` |
|      23 | 6694 | `			if( pMap->nEntry > nMax ){` |
|      13 | 6695 | `				nMax = pMap->nEntry;` |
|       6 | 6696 | `			}` |
|      12 | 6697 | `		}` |
|      35 | 6698 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 6699 | `			ph7_value *pZip = 0;` |
|      25 | 6700 | `			if( bNullCallback ){` |
|       - | 6701 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 6702 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 6703 | `			}` |
|      79 | 6704 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 6705 | `				ph7_value *pv = &sNull;` |
|      55 | 6706 | `				if( apCur[i] ){` |
|      53 | 6707 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 6708 | `					if( pNodeVal ){` |
|      53 | 6709 | `						pv = pNodeVal;` |
|      26 | 6710 | `					}` |
|      53 | 6711 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 6712 | `				}` |
|      55 | 6713 | `				if( bNullCallback ){` |
|       9 | 6714 | `					if( pZip ){` |
|       9 | 6715 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 6716 | `					}` |
|       5 | 6717 | `				}else{` |
|      47 | 6718 | `					apCallArg[i] = pv;` |
|       - | 6719 | `				}` |
|      28 | 6720 | `			}` |
|      25 | 6721 | `			if( bNullCallback ){` |
|       5 | 6722 | `				if( pZip ){` |
|       5 | 6723 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 6724 | `				}` |
|       3 | 6725 | `			}else{` |
|      21 | 6726 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 6727 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6728 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 6729 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 6730 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 6731 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6732 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6733 | `					return PH7_EXCEPTION;` |
|       - | 6734 | `				}` |
|      21 | 6735 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 6736 | `				PH7_MemObjRelease(&sResult);` |
|       - | 6737 | `			}` |
|      13 | 6738 | `		}` |
|      11 | 6739 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 6740 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 6741 | `		PH7_MemObjRelease(&sNull);` |
|       - | 6742 | `	}` |
|      36 | 6743 | `	PH7_MemObjRelease(&sKey);` |
|      36 | 6744 | `	PH7_MemObjRelease(&sResult);` |
|      36 | 6745 | `	ph7_result_value(pCtx,pArray);` |
|      36 | 6746 | `	return PH7_OK;` |
|      25 | 6747 |  |
|       - | 6748 | `/*` |
|       - | 6749 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6750 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6751 | ` * Parameters` |
|       - | 6752 | ` *  $array` |
|       - | 6753 | ` *   The input array.` |
|       - | 6754 | ` *  $callback` |
|       - | 6755 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6756 | ` *  $initial` |
|       - | 6757 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6758 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6759 | ` * Return` |
|       - | 6760 | ` *  Returns the resulting value.` |
|       - | 6761 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6762 | ` */` |
|      32 | 6763 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6764 |  |
|       - | 6765 | `	ph7_hashmap_node *pEntry;` |
|       - | 6766 | `	ph7_hashmap *pMap;` |
|       - | 6767 | `	ph7_value *pValue;` |
|       - | 6768 | `	ph7_value sResult;` |
|       - | 6769 | `	sxi32 rc;` |
|       - | 6770 | `	sxu32 n;` |
|      34 | 6771 | `	if( nArg < 2 ){` |
|       7 | 6772 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6773 | `			"ArgumentCountError",` |
|       - | 6774 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6775 | `			nArg` |
|       - | 6776 | `			);` |
|       - | 6777 | `	}` |
|      30 | 6778 | `	if( nArg > 3 ){` |
|       4 | 6779 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6780 | `			"ArgumentCountError",` |
|       - | 6781 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6782 | `			nArg` |
|       - | 6783 | `			);` |
|       - | 6784 | `	}` |
|      28 | 6785 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6786 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6787 | `			"TypeError",` |
|       - | 6788 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6789 | `			ph7_type_name(apArg[0])` |
|       - | 6790 | `			);` |
|       - | 6791 | `	}` |
|      26 | 6792 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6793 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6794 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6795 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6796 | `				"TypeError",` |
|       - | 6797 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6798 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6799 | `				zFunc` |
|       - | 6800 | `				);` |
|       - | 6801 | `		}` |
|       7 | 6802 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6803 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6804 | `				"TypeError",` |
|       - | 6805 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6806 | `				"array callback must have exactly two members"` |
|       - | 6807 | `				);` |
|       - | 6808 | `		}` |
|       5 | 6809 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6810 | `			"TypeError",` |
|       - | 6811 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6812 | `			"no array or string given"` |
|       - | 6813 | `			);` |
|       - | 6814 | `	}` |
|       - | 6815 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 6816 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6817 | `	/* Assume a NULL initial value */` |
|      17 | 6818 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      17 | 6819 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      17 | 6820 | `	if( nArg > 2 ){` |
|       - | 6821 | `		/* Set the initial value */` |
|      11 | 6822 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6823 | `	}` |
|       - | 6824 | `	/* Perform the requested operation */` |
|      17 | 6825 | `	pEntry = pMap->pFirst;` |
|      45 | 6826 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6827 | `		/* Extract the node value */` |
|      31 | 6828 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6829 | `		/* Invoke the supplied callback */` |
|      31 | 6830 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      31 | 6831 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 6832 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6833 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 6834 | `			return PH7_EXCEPTION;` |
|       - | 6835 | `		}` |
|       - | 6836 | `		/* Point to the next entry */` |
|      29 | 6837 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6838 | `	}` |
|      15 | 6839 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6840 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6841 | `	return PH7_OK;` |
|      18 | 6842 |  |
|       - | 6843 | `/*` |
|       - | 6844 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6845 | ` *  Apply a user function to every member of an array.` |
|       - | 6846 | ` * Parameters` |
|       - | 6847 | ` *  $array` |
|       - | 6848 | ` *   The input array.` |
|       - | 6849 | ` *  $funcname` |
|       - | 6850 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6851 | ` *   the first, and the key/index second.` |
|       - | 6852 | ` * Note:` |
|       - | 6853 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6854 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6855 | ` *  be made in the original array itself.` |
|       - | 6856 | ` *  $userdata` |
|       - | 6857 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6858 | ` *   to the callback funcname.` |
|       - | 6859 | ` * Return` |
|       - | 6860 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6861 | ` */` |
|      38 | 6862 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6863 |  |
|       - | 6864 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6865 | `	ph7_hashmap_node *pEntry;` |
|       - | 6866 | `	ph7_hashmap *pMap;` |
|       - | 6867 | `	sxu32 n;` |
|      40 | 6868 | `	if( nArg < 2 ){` |
|       7 | 6869 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6870 | `			"ArgumentCountError",` |
|       - | 6871 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6872 | `			nArg` |
|       - | 6873 | `			);` |
|       - | 6874 | `	}` |
|      36 | 6875 | `	if( nArg > 3 ){` |
|       4 | 6876 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6877 | `			"ArgumentCountError",` |
|       - | 6878 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6879 | `			nArg` |
|       - | 6880 | `			);` |
|       - | 6881 | `	}` |
|      34 | 6882 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6883 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6884 | `			"TypeError",` |
|       - | 6885 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6886 | `			ph7_type_name(apArg[0])` |
|       - | 6887 | `			);` |
|       - | 6888 | `	}` |
|      32 | 6889 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6890 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6891 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6892 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6893 | `				"TypeError",` |
|       - | 6894 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6895 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6896 | `				zFunc` |
|       - | 6897 | `				);` |
|       - | 6898 | `		}` |
|       9 | 6899 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6900 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6901 | `				"TypeError",` |
|       - | 6902 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6903 | `				"array callback must have exactly two members"` |
|       - | 6904 | `				);` |
|       - | 6905 | `		}` |
|       5 | 6906 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6907 | `			"TypeError",` |
|       - | 6908 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6909 | `			"no array or string given"` |
|       - | 6910 | `			);` |
|       - | 6911 | `	}` |
|      21 | 6912 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6913 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6914 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 6915 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 6916 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 6917 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6918 | `	/* Perform the desired operation */` |
|      21 | 6919 | `	pEntry = pMap->pFirst;` |
|      61 | 6920 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6921 | `		/* Extract the node value */` |
|      43 | 6922 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 6923 | `		if( pValue ){` |
|       - | 6924 | `			sxi32 rcW;` |
|       - | 6925 | `			/* Extract the entry key */` |
|      43 | 6926 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6927 | `			/* Invoke the supplied callback */` |
|      43 | 6928 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 6929 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 6930 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 6931 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6932 | `				return PH7_EXCEPTION;` |
|       - | 6933 | `			}` |
|      20 | 6934 | `		}` |
|       - | 6935 | `		/* Point to the next entry */` |
|      41 | 6936 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6937 | `	}` |
|       - | 6938 | `	/* All done, return TRUE */` |
|      19 | 6939 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6940 | `	return PH7_OK;` |
|      21 | 6941 |  |
|       - | 6942 | `/*` |
|       - | 6943 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6944 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6945 | ` */` |
|      22 | 6946 | `static sxi32 HashmapWalkRecursive(` |
|       - | 6947 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6948 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6949 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6950 | `	int iNest             /* Nesting level */` |
|       - | 6951 | `	)` |
|       1 | 6952 |  |
|       - | 6953 | `	ph7_hashmap_node *pEntry;` |
|       - | 6954 | `	ph7_value *pValue,sKey;` |
|       - | 6955 | `	sxi32 rc;` |
|       - | 6956 | `	sxu32 n;` |
|       - | 6957 | `	/* Iterate through hashmap entries */` |
|      23 | 6958 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6959 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6960 | `	pEntry = pMap->pFirst;` |
|      59 | 6961 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6962 | `		/* Extract the node value */` |
|      37 | 6963 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6964 | `		if( pValue ){` |
|      37 | 6965 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6966 | `				if( iNest < 32 ){` |
|       - | 6967 | `					/* Recurse */` |
|      11 | 6968 | `					iNest++;` |
|      11 | 6969 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6970 | `					iNest--;` |
|      11 | 6971 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6972 | `						return PH7_EXCEPTION;` |
|       - | 6973 | `					}` |
|       5 | 6974 | `				}` |
|       6 | 6975 | `			}else{` |
|       - | 6976 | `				/* Extract the node key */` |
|      27 | 6977 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6978 | `				/* Invoke the supplied callback */` |
|      27 | 6979 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6980 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 6981 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 6982 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 6983 | `					return PH7_EXCEPTION;` |
|       - | 6984 | `				}` |
|       - | 6985 | `			}` |
|      18 | 6986 | `		}` |
|       - | 6987 | `		/* Point to the next entry */` |
|      37 | 6988 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6989 | `	}` |
|      23 | 6990 | `	return PH7_OK;` |
|      12 | 6991 |  |
|       - | 6992 | `/*` |
|       - | 6993 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6994 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6995 | ` * Parameters` |
|       - | 6996 | ` *  $array` |
|       - | 6997 | ` *   The input array.` |
|       - | 6998 | ` *  $funcname` |
|       - | 6999 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 7000 | ` *   the first, and the key/index second.` |
|       - | 7001 | ` * Note:` |
|       - | 7002 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 7003 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 7004 | ` *  be made in the original array itself.` |
|       - | 7005 | ` *  $userdata` |
|       - | 7006 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 7007 | ` *   to the callback funcname.` |
|       - | 7008 | ` * Return` |
|       - | 7009 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 7010 | ` */` |
|      30 | 7011 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 7012 |  |
|       - | 7013 | `	ph7_hashmap *pMap;` |
|      32 | 7014 | `	if( nArg < 2 ){` |
|       7 | 7015 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7016 | `			"ArgumentCountError",` |
|       - | 7017 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 7018 | `			nArg` |
|       - | 7019 | `			);` |
|       - | 7020 | `	}` |
|      28 | 7021 | `	if( nArg > 3 ){` |
|       4 | 7022 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7023 | `			"ArgumentCountError",` |
|       - | 7024 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 7025 | `			nArg` |
|       - | 7026 | `			);` |
|       - | 7027 | `	}` |
|      26 | 7028 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7029 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7030 | `			"TypeError",` |
|       - | 7031 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7032 | `			ph7_type_name(apArg[0])` |
|       - | 7033 | `			);` |
|       - | 7034 | `	}` |
|      24 | 7035 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 7036 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7037 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7038 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7039 | `				"TypeError",` |
|       - | 7040 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7041 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7042 | `				zFunc` |
|       - | 7043 | `				);` |
|       - | 7044 | `		}` |
|       9 | 7045 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 7046 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7047 | `				"TypeError",` |
|       - | 7048 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7049 | `				"array callback must have exactly two members"` |
|       - | 7050 | `				);` |
|       - | 7051 | `		}` |
|       5 | 7052 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7053 | `			"TypeError",` |
|       - | 7054 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7055 | `			"no array or string given"` |
|       - | 7056 | `			);` |
|       - | 7057 | `	}` |
|       - | 7058 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 7059 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 7060 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7061 | `	/* Perform the desired operation */` |
|      13 | 7062 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 7063 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7064 | `		return PH7_EXCEPTION;` |
|       - | 7065 | `	}` |
|       - | 7066 | `	/* All done, return TRUE */` |
|      13 | 7067 | `	ph7_result_bool(pCtx,1);` |
|      13 | 7068 | `	return PH7_OK;` |
|      17 | 7069 |  |
|       - | 7070 | `/*` |
|       - | 7071 | ` * bool array_is_list(array $array)` |
|       - | 7072 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 7073 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 7074 | ` * Return` |
|       - | 7075 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 7076 | ` */` |
|       - | 7077 | `/*` |
|       - | 7078 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 7079 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 7080 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 7081 | ` */` |
|      60 | 7082 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       1 | 7083 |  |
|      61 | 7084 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|      61 | 7085 | `	sxi64 iExpect = 0;` |
|       - | 7086 | `	sxu32 n;` |
|     129 | 7087 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     101 | 7088 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 7089 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|      33 | 7090 | `			return 0;` |
|       - | 7091 | `		}` |
|      69 | 7092 | `		++iExpect;` |
|      69 | 7093 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      35 | 7094 | `	}` |
|      29 | 7095 | `	return 1;` |
|      31 | 7096 |  |
|      12 | 7097 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7098 |  |
|      13 | 7099 | `	if( nArg < 1 ){` |
|     ! 0 | 7100 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7101 | `			"ArgumentCountError",` |
|       - | 7102 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 7103 | `			);` |
|       - | 7104 | `	}` |
|      13 | 7105 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7106 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7107 | `			"TypeError",` |
|       - | 7108 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7109 | `			ph7_type_name(apArg[0])` |
|       - | 7110 | `			);` |
|       - | 7111 | `	}` |
|      13 | 7112 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 7113 | `	return PH7_OK;` |
|       7 | 7114 |  |
|       - | 7115 | `/*` |
|       - | 7116 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 7117 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 7118 | ` * array_column() for both the column value and the index key.` |
|       - | 7119 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 7120 | ` * container or the key is absent.` |
|       - | 7121 | ` */` |
|      32 | 7122 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 7123 |  |
|      33 | 7124 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 7125 | `		ph7_hashmap_node *pNode;` |
|      25 | 7126 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 7127 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 7128 | `		}` |
|      11 | 7129 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 7130 | `		ph7_value sName;` |
|       - | 7131 | `		const char *zName;` |
|       - | 7132 | `		ph7_value *pAttr;` |
|       - | 7133 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 7134 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 7135 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 7136 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 7137 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 7138 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 7139 | `		PH7_MemObjRelease(&sName);` |
|       9 | 7140 | `		return pAttr;` |
|       - | 7141 | `	}` |
|       5 | 7142 | `	return 0;` |
|      17 | 7143 |  |
|       - | 7144 | `/*` |
|       - | 7145 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 7146 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 7147 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 7148 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 7149 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 7150 | ` *  Each row may be an array or an object.` |
|       - | 7151 | ` */` |
|      12 | 7152 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7153 |  |
|       - | 7154 | `	ph7_hashmap_node *pNode;` |
|       - | 7155 | `	ph7_hashmap *pMap;` |
|       - | 7156 | `	ph7_value *pArray;` |
|       - | 7157 | `	ph7_value *pRow;` |
|       - | 7158 | `	ph7_value *pCol;` |
|       - | 7159 | `	ph7_value *pIdx;` |
|       - | 7160 | `	int bWantCol;` |
|       - | 7161 | `	int bWantIdx;` |
|       - | 7162 | `	sxu32 n;` |
|      13 | 7163 | `	if( nArg < 2 ){` |
|     ! 0 | 7164 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7165 | `			"ArgumentCountError",` |
|       - | 7166 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 7167 | `			nArg` |
|       - | 7168 | `			);` |
|       - | 7169 | `	}` |
|      13 | 7170 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7171 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7172 | `			"TypeError",` |
|       - | 7173 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7174 | `			ph7_type_name(apArg[0])` |
|       - | 7175 | `			);` |
|       - | 7176 | `	}` |
|      13 | 7177 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 7178 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 7179 | `	if( pArray == 0 ){` |
|     ! 0 | 7180 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7181 | `		return PH7_OK;` |
|       - | 7182 | `	}` |
|       - | 7183 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 7184 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 7185 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 7186 | `	pNode = pMap->pFirst;` |
|      33 | 7187 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 7188 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 7189 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 7190 | `		if( pRow == 0 ){` |
|     ! 0 | 7191 | `			continue;` |
|       - | 7192 | `		}` |
|      21 | 7193 | `		if( bWantCol ){` |
|      19 | 7194 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 7195 | `			if( pCol == 0 ){` |
|       - | 7196 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 7197 | `				continue;` |
|       - | 7198 | `			}` |
|       9 | 7199 | `		}else{` |
|       3 | 7200 | `			pCol = pRow;` |
|       - | 7201 | `		}` |
|      19 | 7202 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 7203 | `		if( pIdx ){` |
|      13 | 7204 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 7205 | `		}else{` |
|       7 | 7206 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 7207 | `		}` |
|      10 | 7208 | `	}` |
|      13 | 7209 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 7210 | `	return PH7_OK;` |
|       7 | 7211 |  |
|       - | 7212 | `/*` |
|       - | 7213 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 7214 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 7215 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 7216 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 7217 | ` */` |
|      28 | 7218 | `static sxi32 HashmapCallbackSearch(` |
|       - | 7219 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 7220 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 7221 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 7222 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 7223 | `	)` |
|       1 | 7224 |  |
|       - | 7225 | `	ph7_hashmap_node *pEntry;` |
|       - | 7226 | `	ph7_hashmap *pMap;` |
|       - | 7227 | `	ph7_value *pValue;` |
|       - | 7228 | `	ph7_value *apCbArg[2];` |
|       - | 7229 | `	ph7_value sKey;` |
|       - | 7230 | `	ph7_value sResult;` |
|       - | 7231 | `	sxi32 rc;` |
|       - | 7232 | `	sxu32 n;` |
|      29 | 7233 | `	*ppMatch = 0;` |
|      29 | 7234 | `	if( nArg < 2 ){` |
|     ! 0 | 7235 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7236 | `			"ArgumentCountError",` |
|       - | 7237 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 7238 | `			zName,nArg` |
|       - | 7239 | `			);` |
|       - | 7240 | `	}` |
|      29 | 7241 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7242 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7243 | `			"TypeError",` |
|       - | 7244 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7245 | `			zName,ph7_type_name(apArg[0])` |
|       - | 7246 | `			);` |
|       - | 7247 | `	}` |
|      29 | 7248 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7249 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7250 | `			"TypeError",` |
|       - | 7251 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 7252 | `			zName,ph7_type_name(apArg[1])` |
|       - | 7253 | `			);` |
|       - | 7254 | `	}` |
|      29 | 7255 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 7256 | `	pEntry = pMap->pFirst;` |
|      29 | 7257 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 7258 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 7259 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 7260 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 7261 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 7262 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 7263 | `		if( pValue ){` |
|       - | 7264 | `			/* The callback receives ($value, $key). */` |
|      59 | 7265 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 7266 | `			apCbArg[0] = pValue;` |
|      59 | 7267 | `			apCbArg[1] = &sKey;` |
|      59 | 7268 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 7269 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 7270 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7271 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7272 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7273 | `				return PH7_EXCEPTION;` |
|       - | 7274 | `			}` |
|      59 | 7275 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 7276 | `				*ppMatch = pEntry;` |
|      15 | 7277 | `				break;` |
|       - | 7278 | `			}` |
|      22 | 7279 | `		}` |
|      45 | 7280 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 7281 | `	}` |
|      29 | 7282 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 7283 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 7284 | `	return PH7_OK;` |
|      15 | 7285 |  |
|       - | 7286 | `/*` |
|       - | 7287 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 7288 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 7289 | ` *  is truthy, or NULL if none match.` |
|       - | 7290 | ` */` |
|       6 | 7291 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7292 |  |
|       - | 7293 | `	ph7_hashmap_node *pMatch;` |
|       - | 7294 | `	ph7_value *pVal;` |
|       - | 7295 | `	sxi32 rc;` |
|       7 | 7296 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 7297 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7298 | `		return rc;` |
|       - | 7299 | `	}` |
|       7 | 7300 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 7301 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 7302 | `	}else{` |
|       3 | 7303 | `		ph7_result_null(pCtx);` |
|       - | 7304 | `	}` |
|       7 | 7305 | `	return PH7_OK;` |
|       4 | 7306 |  |
|       - | 7307 | `/*` |
|       - | 7308 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 7309 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 7310 | ` *  is truthy, or NULL if none match.` |
|       - | 7311 | ` */` |
|       6 | 7312 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7313 |  |
|       - | 7314 | `	ph7_hashmap_node *pMatch;` |
|       - | 7315 | `	sxi32 rc;` |
|       7 | 7316 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 7317 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7318 | `		return rc;` |
|       - | 7319 | `	}` |
|       7 | 7320 | `	if( pMatch == 0 ){` |
|       3 | 7321 | `		ph7_result_null(pCtx);` |
|       6 | 7322 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 7323 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 7324 | `	}else{` |
|       4 | 7325 | `		ph7_result_string(pCtx,` |
|       2 | 7326 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 7327 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 7328 | `	}` |
|       7 | 7329 | `	return PH7_OK;` |
|       4 | 7330 |  |
|       - | 7331 | `/*` |
|       - | 7332 | ` * bool array_any(array $array, callable $callback)` |
|       - | 7333 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 7334 | ` *  FALSE for an empty array.` |
|       - | 7335 | ` */` |
|       8 | 7336 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7337 |  |
|       - | 7338 | `	ph7_hashmap_node *pMatch;` |
|       - | 7339 | `	sxi32 rc;` |
|       9 | 7340 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 7341 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7342 | `		return rc;` |
|       - | 7343 | `	}` |
|       9 | 7344 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 7345 | `	return PH7_OK;` |
|       5 | 7346 |  |
|       - | 7347 | `/*` |
|       - | 7348 | ` * bool array_all(array $array, callable $callback)` |
|       - | 7349 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 7350 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 7351 | ` */` |
|       8 | 7352 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7353 |  |
|       - | 7354 | `	ph7_hashmap_node *pMatch;` |
|       - | 7355 | `	sxi32 rc;` |
|       9 | 7356 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 7357 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7358 | `		return rc;` |
|       - | 7359 | `	}` |
|       9 | 7360 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 7361 | `	return PH7_OK;` |
|       5 | 7362 |  |
|       - | 7363 | `/*` |
|       - | 7364 | ` * Table of hashmap functions.` |
|       - | 7365 | ` */` |
|       - | 7366 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 7367 | `	{"count",             ph7_hashmap_count },` |
|       - | 7368 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 7369 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 7370 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 7371 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 7372 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 7373 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 7374 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 7375 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 7376 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 7377 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 7378 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 7379 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 7380 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 7381 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 7382 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 7383 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 7384 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 7385 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 7386 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 7387 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 7388 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 7389 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 7390 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 7391 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 7392 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 7393 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 7394 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 7395 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 7396 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 7397 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 7398 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 7399 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 7400 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 7401 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 7402 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 7403 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 7404 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 7405 | `	{"array_column",      ph7_hashmap_column  },` |
|       - | 7406 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|       - | 7407 | `	{"array_find",        ph7_hashmap_find    },` |
|       - | 7408 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|       - | 7409 | `	{"array_any",         ph7_hashmap_any     },` |
|       - | 7410 | `	{"array_all",         ph7_hashmap_all     },` |
|       - | 7411 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 7412 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 7413 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 7414 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 7415 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 7416 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 7417 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 7418 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 7419 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 7420 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 7421 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 7422 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 7423 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 7424 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 7425 | `	{"range",             ph7_hashmap_range   },` |
|       - | 7426 | `	{"current",           ph7_hashmap_current },` |
|       - | 7427 | `	{"each",              ph7_hashmap_each    },` |
|       - | 7428 | `	{"pos",               ph7_hashmap_current },` |
|       - | 7429 | `	{"next",              ph7_hashmap_next    },` |
|       - | 7430 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 7431 | `	{"end",               ph7_hashmap_end     },` |
|       - | 7432 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 7433 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 7434 | `};` |
|       - | 7435 | `/*` |
|       - | 7436 | ` * Register the built-in hashmap functions defined above.` |
|       - | 7437 | ` */` |
|    2820 | 7438 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 7439 |  |
|       - | 7440 | `	sxu32 n;` |
|  191762 | 7441 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  188942 | 7442 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   94472 | 7443 | `	}` |
|    2822 | 7444 |  |
|       - | 7445 | `/*` |
|       - | 7446 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 7447 | ` * the BLOB given as the first argument.` |
|       - | 7448 | ` * This function is typically invoked when the user issue a call to` |
|       - | 7449 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 7450 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 7451 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 7452 | ` */` |
|      26 | 7453 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 7454 |  |
|       - | 7455 | `	ph7_hashmap_node *pEntry;` |
|       - | 7456 | `	ph7_value *pObj;` |
|      28 | 7457 | `	sxu32 n = 0;` |
|       - | 7458 | `	int isRef;` |
|       - | 7459 | `	sxi32 rc;` |
|       - | 7460 | `	int i;` |
|      28 | 7461 | `	if( nDepth > 31 ){` |
|       - | 7462 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 7463 | `		/* Nesting limit reached */` |
|     ! 0 | 7464 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 7465 | `		if( ShowType ){` |
|     ! 0 | 7466 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 7467 | `		}` |
|     ! 0 | 7468 | `		return SXERR_LIMIT;` |
|       - | 7469 | `	}` |
|       - | 7470 | `	/* Point to the first inserted entry */` |
|      28 | 7471 | `	pEntry = pMap->pFirst;` |
|      28 | 7472 | `	rc = SXRET_OK;` |
|      28 | 7473 | `	if( !ShowType ){` |
|      15 | 7474 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 7475 | `	}` |
|       - | 7476 | `	/* Total entries */` |
|      28 | 7477 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 7478 | `#ifdef __WINNT__` |
|       2 | 7479 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7480 | `#else` |
|      26 | 7481 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7482 | `#endif` |
|      62 | 7483 | `	for(;;){` |
|     126 | 7484 | `		if( n >= pMap->nEntry ){` |
|      28 | 7485 | `			break;` |
|       - | 7486 | `		}` |
|     198 | 7487 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 7488 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 7489 | `		}` |
|       - | 7490 | `		/* Dump key */` |
|     100 | 7491 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 7492 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 7493 | `		}else{` |
|     101 | 7494 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 7495 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 7496 | `		}` |
|       - | 7497 | `#ifdef __WINNT__` |
|       2 | 7498 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7499 | `#else` |
|      98 | 7500 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7501 | `#endif` |
|       - | 7502 | `		/* Dump node value */` |
|     100 | 7503 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 7504 | `		isRef = 0;` |
|     100 | 7505 | `		if( pObj ){` |
|     100 | 7506 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 7507 | `				/* Referenced object */` |
|     ! 0 | 7508 | `				isRef = 1;` |
|     ! 0 | 7509 | `			}` |
|     100 | 7510 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 7511 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 7512 | `				break;` |
|       - | 7513 | `			}` |
|      49 | 7514 | `		}` |
|       - | 7515 | `		/* Point to the next entry */` |
|     100 | 7516 | `		n++;` |
|     100 | 7517 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 7518 | `	}` |
|      54 | 7519 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 7520 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 7521 | `	}` |
|      28 | 7522 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 7523 | `	return rc;` |
|      15 | 7524 |  |
|       - | 7525 | `/*` |
|       - | 7526 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 7527 | ` * retrieved entry.` |
|       - | 7528 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 7529 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 7530 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 7531 | ` * a value different from PH7_OK.` |
|       - | 7532 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 7533 | ` */` |
|   29796 | 7534 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 7535 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 7536 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 7537 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 7538 | `	)` |
|       2 | 7539 |  |
|       - | 7540 | `	ph7_hashmap_node *pEntry;` |
|       - | 7541 | `	ph7_value sKey,sValue;` |
|       - | 7542 | `	sxi32 rc;` |
|       - | 7543 | `	sxu32 n;` |
|       - | 7544 | `	/* Initialize walker parameter */` |
|   29798 | 7545 | `	rc = SXRET_OK;` |
|   29798 | 7546 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   29798 | 7547 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   29798 | 7548 | `	n = pMap->nEntry;` |
|   29798 | 7549 | `	pEntry = pMap->pFirst;` |
|       - | 7550 | `	/* Start the iteration process */` |
|   74412 | 7551 | `	for(;;){` |
|  148826 | 7552 | `		if( n < 1 ){` |
|   29798 | 7553 | `			break;` |
|       - | 7554 | `		}` |
|       - | 7555 | `		/* Extract a copy of the key and a copy the current value */` |
|  119030 | 7556 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  119030 | 7557 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 7558 | `		/* Invoke the user callback */` |
|  119030 | 7559 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 7560 | `		/* Release the copy of the key and the value */` |
|  119030 | 7561 | `		PH7_MemObjRelease(&sKey);` |
|  119030 | 7562 | `		PH7_MemObjRelease(&sValue);` |
|  119030 | 7563 | `		if( rc != PH7_OK ){` |
|       - | 7564 | `			/* Callback request an operation abort */` |
|     ! 0 | 7565 | `			return SXERR_ABORT;` |
|       - | 7566 | `		}` |
|       - | 7567 | `		/* Point to the next entry */` |
|  119030 | 7568 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  119030 | 7569 | `		n--;` |
|       2 | 7570 | `	}` |
|       - | 7571 | `	/* All done */` |
|   29798 | 7572 | `	return SXRET_OK;` |
|   14900 | 7573 |  |
|       - | 7574 |  |
