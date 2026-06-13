# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3227/3710 lines (86.98%)

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
| 1213954 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
| 1213956 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1213956 |  718 | `	return pObj;` |
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
|   60700 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   60702 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   60702 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   60702 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   60702 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   60702 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   60702 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   60702 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   60702 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   60702 |  783 | `	return rc;` |
|   30339 |  784 |  |
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
|    9573 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    4776 |  796 | `	}else{` |
|    2259 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
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
|    9823 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    4900 |  811 | `	}` |
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
|  141760 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  141662 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  141662 |  847 | `		if( pVal ){` |
|  141662 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  141662 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  141662 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  141662 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  141662 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  141662 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  141662 |  865 | `				if( rc == 0 ){` |
|   29500 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   29500 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   56081 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|  112164 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  112164 |  876 | `		n--;` |
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
|  480896 | 1713 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1714 |  |
|  480898 | 1715 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  480898 | 1716 | `	if( pEntry ){` |
|  480898 | 1717 | `		if( bStore ){` |
|  190106 | 1718 | `			PH7_MemObjStore(pEntry,pValue);` |
|   95054 | 1719 | `		}else{` |
|  290794 | 1720 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1721 | `		}` |
|  240424 | 1722 | `	}else{` |
|     ! 0 | 1723 | `		PH7_MemObjRelease(pValue);` |
|       - | 1724 | `	}` |
|  480898 | 1725 |  |
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
|   92073 | 1800 | `	while( pA && pB ){` |
|   60843 | 1801 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   40780 | 1802 | `			pTail->pPrev = pA;` |
|   40780 | 1803 | `			pA->pNext = pTail;` |
|   40780 | 1804 | `			pTail = pA;` |
|   40780 | 1805 | `			pA = pA->pPrev;` |
|   20353 | 1806 | `		}else{` |
|   20065 | 1807 | `			pTail->pPrev = pB;` |
|   20065 | 1808 | `			pB->pNext = pTail;` |
|   20065 | 1809 | `			pTail = pB;` |
|   20065 | 1810 | `			pB = pB->pPrev;` |
|       - | 1811 | `		}` |
|       2 | 1812 | `	}` |
|   31232 | 1813 | `	if( pA ){` |
|   22219 | 1814 | `		pTail->pPrev = pA;` |
|   22219 | 1815 | `		pA->pNext = pTail;` |
|   20140 | 1816 | `	}else if( pB ){` |
|    8799 | 1817 | `		pTail->pPrev = pB;` |
|    8799 | 1818 | `		pB->pNext = pTail;` |
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
|   60648 | 1879 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1880 |  |
|       - | 1881 | `	ph7_value sA,sB;` |
|       - | 1882 | `	sxi32 iFlags;` |
|       - | 1883 | `	int rc;` |
|   60650 | 1884 | `	if( pCmpData == 0 ){` |
|       - | 1885 | `		/* Perform a standard comparison */` |
|   60626 | 1886 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   60626 | 1887 | `		return rc;` |
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
|   30313 | 1925 |  |
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
|      13 | 2163 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2164 |  |
|       - | 2165 | `	sxu32 n;` |
|       6 | 2166 | `	SXUNUSED(pB); /* cc warning */` |
|       6 | 2167 | `	SXUNUSED(pCmpData);` |
|       - | 2168 | `	/* Grab a random number */` |
|      14 | 2169 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2170 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2171 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2172 | `	 */` |
|      14 | 2173 | `	return n&1 ? 1 : -1;` |
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
|      11 | 2695 | `		while(pMap->pLast->pPrev){` |
|       9 | 2696 | `			pMap->pLast = pMap->pLast->pPrev;` |
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
|     ! 0 | 3344 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3345 | `		return PH7_OK;` |
|       - | 3346 | `	}` |
|       - | 3347 | `	/* Start filling */` |
|       3 | 3348 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3349 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3350 | `		/* Perform the insertion */` |
|     ! 0 | 3351 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3352 | `		/* Increment */` |
|     ! 0 | 3353 | `		iOfft += iStep;` |
|     ! 0 | 3354 | `	}` |
|       - | 3355 | `	/* Return the new array */` |
|       3 | 3356 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3357 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3358 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3359 | `	 */` |
|       3 | 3360 | `	return PH7_OK;` |
|       2 | 3361 |  |
|       - | 3362 | `/*` |
|       - | 3363 | ` * array array_values(array $array)` |
|       - | 3364 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3365 | ` * Parameters` |
|       - | 3366 | ` *  $array` |
|       - | 3367 | ` *   The input array.` |
|       - | 3368 | ` * Return` |
|       - | 3369 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3370 | ` */` |
|      30 | 3371 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3372 |  |
|       - | 3373 | `	ph7_hashmap_node *pNode;` |
|       - | 3374 | `	ph7_hashmap *pMap;` |
|       - | 3375 | `	ph7_value *pArray;` |
|       - | 3376 | `	ph7_value *pObj;` |
|       - | 3377 | `	sxu32 n;` |
|      32 | 3378 | `	if( nArg != 1 ){` |
|       - | 3379 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3380 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3381 | `			"ArgumentCountError",` |
|       - | 3382 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3383 | `			nArg` |
|       - | 3384 | `			);` |
|       - | 3385 | `	}` |
|       - | 3386 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3387 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3388 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3389 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3390 | `			"TypeError",` |
|       - | 3391 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3392 | `			ph7_type_name(apArg[0])` |
|       - | 3393 | `			);` |
|       - | 3394 | `	}` |
|       - | 3395 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3396 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3397 | `	/* Create a new array */` |
|      25 | 3398 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3399 | `	if( pArray == 0 ){` |
|     ! 0 | 3400 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3401 | `		return PH7_OK;` |
|       - | 3402 | `	}` |
|       - | 3403 | `	/* Perform the requested operation */` |
|      25 | 3404 | `	pNode = pMap->pFirst;` |
|      83 | 3405 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3406 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3407 | `		if( pObj ){` |
|       - | 3408 | `			/* perform the insertion */` |
|      59 | 3409 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3410 | `		}` |
|       - | 3411 | `		/* Point to the next entry */` |
|      59 | 3412 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3413 | `	}` |
|       - | 3414 | `	/* return the new array */` |
|      25 | 3415 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3416 | `	return PH7_OK;` |
|      17 | 3417 |  |
|       - | 3418 | `/*` |
|       - | 3419 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3420 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3421 | ` * Parameters` |
|       - | 3422 | ` *  $input` |
|       - | 3423 | ` *   An array containing keys to return.` |
|       - | 3424 | ` * $search_value` |
|       - | 3425 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3426 | ` * $strict` |
|       - | 3427 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3428 | ` * Return` |
|       - | 3429 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3430 | ` */` |
|     122 | 3431 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3432 |  |
|       - | 3433 | `	ph7_hashmap_node *pNode;` |
|       - | 3434 | `	ph7_hashmap *pMap;` |
|       - | 3435 | `	ph7_value *pArray;` |
|       - | 3436 | `	ph7_value sObj;` |
|       - | 3437 | `	ph7_value sVal;` |
|       - | 3438 | `	SyString sKey;` |
|       - | 3439 | `	int bStrict;` |
|       - | 3440 | `	sxi32 rc;` |
|       - | 3441 | `	sxu32 n;` |
|     124 | 3442 | `	if( nArg < 1 ){` |
|       - | 3443 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3444 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3445 | `			"ArgumentCountError",` |
|       - | 3446 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3447 | `			);` |
|       - | 3448 | `	}` |
|       - | 3449 | `	/* Make sure we are dealing with a valid hashmap */` |
|     122 | 3450 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3451 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3452 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3453 | `			"TypeError",` |
|       - | 3454 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3455 | `			ph7_type_name(apArg[0])` |
|       - | 3456 | `			);` |
|       - | 3457 | `	}` |
|       - | 3458 | `	/* Point to the internal representation of the input hashmap */` |
|     120 | 3459 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3460 | `	/* Create a new array */` |
|     120 | 3461 | `	pArray = ph7_context_new_array(pCtx);` |
|     120 | 3462 | `	if( pArray == 0 ){` |
|     ! 0 | 3463 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3464 | `		return PH7_OK;` |
|       - | 3465 | `	}` |
|     120 | 3466 | `	bStrict = FALSE;` |
|     120 | 3467 | `	if( nArg > 2 ){` |
|       - | 3468 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3469 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3470 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3471 | `				"TypeError",` |
|       - | 3472 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3473 | `				ph7_type_name(apArg[2])` |
|       - | 3474 | `				);` |
|       - | 3475 | `		}` |
|       5 | 3476 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3477 | `	}` |
|       - | 3478 | `	/* Perform the requested operation */` |
|     117 | 3479 | `	pNode = pMap->pFirst;` |
|     117 | 3480 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     559 | 3481 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     443 | 3482 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     121 | 3483 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      61 | 3484 | `		}else{` |
|     323 | 3485 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3486 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3487 | `		}` |
|     443 | 3488 | `		rc = 0;` |
|     443 | 3489 | `		if( nArg > 1 ){` |
|      31 | 3490 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3491 | `			if( pValue ){` |
|      31 | 3492 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3493 | `				/* Filter key */` |
|      31 | 3494 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3495 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3496 | `			}` |
|      15 | 3497 | `		}` |
|     443 | 3498 | `		if( rc == 0 ){` |
|       - | 3499 | `			/* Perform the insertion */` |
|     425 | 3500 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     212 | 3501 | `		}` |
|     443 | 3502 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3503 | `		/* Point to the next entry */` |
|     443 | 3504 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     222 | 3505 | `	}` |
|       - | 3506 | `	/* return the new array */` |
|     117 | 3507 | `	ph7_result_value(pCtx,pArray);` |
|     117 | 3508 | `	return PH7_OK;` |
|      63 | 3509 |  |
|       - | 3510 | `/*` |
|       - | 3511 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3512 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3513 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3514 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3515 | ` * Parameters` |
|       - | 3516 | ` *  $arr1` |
|       - | 3517 | ` *   First array` |
|       - | 3518 | ` *  $arr2` |
|       - | 3519 | ` *   Second array` |
|       - | 3520 | ` * Return` |
|       - | 3521 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3522 | ` * Note` |
|       - | 3523 | ` *  This function is a symisc eXtension.` |
|       - | 3524 | ` */` |
|       4 | 3525 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3526 |  |
|       - | 3527 | `	ph7_hashmap *p1,*p2;` |
|       - | 3528 | `	int rc;` |
|       5 | 3529 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3530 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3531 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3532 | `		return PH7_OK;` |
|       - | 3533 | `	}` |
|       - | 3534 | `	/* Point to the hashmaps */` |
|       5 | 3535 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3536 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3537 | `	rc = (p1 == p2);` |
|       - | 3538 | `	/* Same instance? */` |
|       5 | 3539 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3540 | `	return PH7_OK;` |
|       3 | 3541 |  |
|       - | 3542 | `/*` |
|       - | 3543 | ` * array array_merge(array ...$arrays)` |
|       - | 3544 | ` *  Merge one or more arrays.` |
|       - | 3545 | ` * Parameters` |
|       - | 3546 | ` *  ...$arrays` |
|       - | 3547 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3548 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3549 | ` * Return` |
|       - | 3550 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3551 | ` *  with no arguments.` |
|       - | 3552 | ` */` |
|     986 | 3553 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3554 |  |
|       - | 3555 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3556 | `	ph7_value *pArray;` |
|       - | 3557 | `	int i;` |
|       - | 3558 | `	/* Create a new array */` |
|     988 | 3559 | `	pArray = ph7_context_new_array(pCtx);` |
|     988 | 3560 | `	if( pArray == 0 ){` |
|     ! 0 | 3561 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3562 | `		return PH7_OK;` |
|       - | 3563 | `	}` |
|       - | 3564 | `	/* Point to the internal representation of the hashmap */` |
|     988 | 3565 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3566 | `	/* Start merging */` |
|    2950 | 3567 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3568 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1968 | 3569 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3570 | `			/* Type mismatch -> TypeError */` |
|       7 | 3571 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3572 | `				"TypeError",` |
|       - | 3573 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3574 | `				i + 1,` |
|       4 | 3575 | `				ph7_type_name(apArg[i])` |
|       - | 3576 | `				);` |
|     ! 0 | 3577 | `		}else{` |
|    1964 | 3578 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3579 | `			/* Merge the two hashmaps */` |
|    1964 | 3580 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3581 | `		}` |
|     983 | 3582 | `	}` |
|       - | 3583 | `	/* Return the freshly created array */` |
|     984 | 3584 | `	ph7_result_value(pCtx,pArray);` |
|     984 | 3585 | `	return PH7_OK;` |
|     495 | 3586 |  |
|       - | 3587 | `/*` |
|       - | 3588 | ` * array array_copy(array $source)` |
|       - | 3589 | ` *  Make a blind copy of the target array.` |
|       - | 3590 | ` * Parameters` |
|       - | 3591 | ` *  $source` |
|       - | 3592 | ` *   Target array` |
|       - | 3593 | ` * Return` |
|       - | 3594 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3595 | ` * Note` |
|       - | 3596 | ` *  This function is a symisc eXtension.` |
|       - | 3597 | ` */` |
|      16 | 3598 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3599 |  |
|       - | 3600 | `	ph7_hashmap *pMap;` |
|       - | 3601 | `	ph7_value *pArray;` |
|      17 | 3602 | `	if( nArg < 1 ){` |
|       - | 3603 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3604 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3605 | `		return PH7_OK;` |
|       - | 3606 | `	}` |
|       - | 3607 | `	/* Create a new array */` |
|      17 | 3608 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3609 | `	if( pArray == 0 ){` |
|     ! 0 | 3610 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3611 | `		return PH7_OK;` |
|       - | 3612 | `	}` |
|       - | 3613 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3614 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3615 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3616 | `		/* Point to the internal representation of the source */` |
|      17 | 3617 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3618 | `		/* Perform the copy */` |
|      17 | 3619 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3620 | `	}else{` |
|       - | 3621 | `		/* Simple insertion */` |
|     ! 0 | 3622 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3623 | `	}` |
|       - | 3624 | `	/* Return the duplicated array */` |
|      17 | 3625 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3626 | `	return PH7_OK;` |
|       9 | 3627 |  |
|       - | 3628 | `/*` |
|       - | 3629 | ` * bool array_erase(array $source)` |
|       - | 3630 | ` *  Remove all elements from a given array.` |
|       - | 3631 | ` * Parameters` |
|       - | 3632 | ` *  $source` |
|       - | 3633 | ` *   Target array` |
|       - | 3634 | ` * Return` |
|       - | 3635 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3636 | ` * Note` |
|       - | 3637 | ` *  This function is a symisc eXtension.` |
|       - | 3638 | ` */` |
|      16 | 3639 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3640 |  |
|       - | 3641 | `	ph7_hashmap *pMap;` |
|      17 | 3642 | `	if( nArg < 1 ){` |
|       - | 3643 | `		/* Missing arguments */` |
|     ! 0 | 3644 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3645 | `		return PH7_OK;` |
|       - | 3646 | `	}` |
|       - | 3647 | `	/* Point to the target hashmap */` |
|      17 | 3648 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3649 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3650 | `	/* Erase */` |
|      17 | 3651 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3652 | `	return PH7_OK;` |
|       9 | 3653 |  |
|       - | 3654 | `/*` |
|       - | 3655 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3656 | ` *  Extract a slice of the array.` |
|       - | 3657 | ` * Parameters` |
|       - | 3658 | ` *  $array` |
|       - | 3659 | ` *    The input array.` |
|       - | 3660 | ` * $offset` |
|       - | 3661 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3662 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3663 | ` * $length (optional, nullable)` |
|       - | 3664 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3665 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3666 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3667 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3668 | ` * $preserve_keys (optional)` |
|       - | 3669 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3670 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3671 | ` * Return` |
|       - | 3672 | ` *   The new slice.` |
|       - | 3673 | ` */` |
|      46 | 3674 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3675 |  |
|       - | 3676 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3677 | `	ph7_hashmap_node *pCur;` |
|       - | 3678 | `	ph7_value *pArray;` |
|       - | 3679 | `	int iLength,iOfft;` |
|       - | 3680 | `	int bPreserve;` |
|       - | 3681 | `	sxi32 rc;` |
|      48 | 3682 | `	if( nArg < 2 ){` |
|       7 | 3683 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3684 | `			"ArgumentCountError",` |
|       - | 3685 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3686 | `			nArg` |
|       - | 3687 | `			);` |
|       - | 3688 | `	}` |
|      44 | 3689 | `	if( nArg > 4 ){` |
|       4 | 3690 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3691 | `			"ArgumentCountError",` |
|       - | 3692 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3693 | `			nArg` |
|       - | 3694 | `			);` |
|       - | 3695 | `	}` |
|      42 | 3696 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3697 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3698 | `			"TypeError",` |
|       - | 3699 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3700 | `			ph7_type_name(apArg[0])` |
|       - | 3701 | `			);` |
|       - | 3702 | `	}` |
|       - | 3703 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3704 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3705 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3706 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3707 | `			"TypeError",` |
|       - | 3708 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3709 | `			ph7_type_name(apArg[1])` |
|       - | 3710 | `			);` |
|       - | 3711 | `	}` |
|       - | 3712 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3713 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3714 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3715 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3716 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3717 | `				"TypeError",` |
|       - | 3718 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3719 | `				ph7_type_name(apArg[2])` |
|       - | 3720 | `				);` |
|       - | 3721 | `		}` |
|       8 | 3722 | `	}` |
|       - | 3723 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3724 | `	if( nArg > 3 ){` |
|      10 | 3725 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3726 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3727 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3728 | `				"TypeError",` |
|       - | 3729 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3730 | `				ph7_type_name(apArg[3])` |
|       - | 3731 | `				);` |
|       - | 3732 | `		}` |
|       2 | 3733 | `	}` |
|       - | 3734 | `	/* Point the internal representation of the target array */` |
|      33 | 3735 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3736 | `	bPreserve = FALSE;` |
|       - | 3737 | `	/* Get the offset */` |
|      33 | 3738 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3739 | `	if( iOfft < 0 ){` |
|       5 | 3740 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3741 | `		if( iOfft < 0 ){` |
|       3 | 3742 | `			iOfft = 0;` |
|       1 | 3743 | `		}` |
|       2 | 3744 | `	}` |
|      33 | 3745 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3746 | `		/* Offset past end of array, return empty array */` |
|       5 | 3747 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3748 | `		if( pArray == 0 ){` |
|     ! 0 | 3749 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3750 | `			return PH7_OK;` |
|       - | 3751 | `		}` |
|       5 | 3752 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3753 | `		return PH7_OK;` |
|       - | 3754 | `	}` |
|       - | 3755 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3756 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3757 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3758 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3759 | `		if( iLength < 0 ){` |
|       5 | 3760 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3761 | `		}` |
|      15 | 3762 | `		if( iLength < 0 ){` |
|       3 | 3763 | `			iLength = 0;` |
|       1 | 3764 | `		}` |
|      15 | 3765 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3766 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3767 | `		}` |
|       7 | 3768 | `	}` |
|      29 | 3769 | `	if( nArg > 3 ){` |
|       5 | 3770 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3771 | `	}` |
|       - | 3772 | `	/* Create a new array */` |
|      29 | 3773 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3774 | `	if( pArray == 0 ){` |
|     ! 0 | 3775 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3776 | `		return PH7_OK;` |
|       - | 3777 | `	}` |
|      29 | 3778 | `	if( iLength < 1 ){` |
|       - | 3779 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3780 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3781 | `		return PH7_OK;` |
|       - | 3782 | `	}` |
|       - | 3783 | `	/* Point to the desired entry */` |
|      25 | 3784 | `	pCur = pSrc->pFirst;` |
|      24 | 3785 | `	for(;;){` |
|      49 | 3786 | `		if( iOfft < 1 ){` |
|      25 | 3787 | `			break;` |
|       - | 3788 | `		}` |
|       - | 3789 | `		/* Point to the next entry */` |
|      25 | 3790 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3791 | `		iOfft--;` |
|       1 | 3792 | `	}` |
|       - | 3793 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3794 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3795 | `	for(;;){` |
|      79 | 3796 | `		if( iLength < 1 ){` |
|      25 | 3797 | `			break;` |
|       - | 3798 | `		}` |
|       - | 3799 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3800 | `		{` |
|      55 | 3801 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3802 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3803 | `		}` |
|      55 | 3804 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3805 | `			break;` |
|       - | 3806 | `		}` |
|       - | 3807 | `		/* Point to the next entry */` |
|      55 | 3808 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3809 | `		iLength--;` |
|       1 | 3810 | `	}` |
|       - | 3811 | `	/* Return the freshly created array */` |
|      25 | 3812 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3813 | `	return PH7_OK;` |
|      25 | 3814 |  |
|       - | 3815 | `/*` |
|       - | 3816 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3817 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3818 | ` * beginning (becomes the new pFirst).` |
|       - | 3819 | ` */` |
|      30 | 3820 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3821 |  |
|       - | 3822 | `	ph7_hashmap_node *pNode;` |
|       - | 3823 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3824 | `	pNode = pMap->pLast;` |
|      31 | 3825 | `	if( pNode == 0 ){` |
|     ! 0 | 3826 | `		return;` |
|       - | 3827 | `	}` |
|      31 | 3828 | `	if( pNode->pNext == 0 ){` |
|       - | 3829 | `		/* Only node in the list, nothing to move */` |
|       5 | 3830 | `		return;` |
|       - | 3831 | `	}` |
|      27 | 3832 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3833 | `		/* Already in the correct position */` |
|       9 | 3834 | `		return;` |
|       - | 3835 | `	}` |
|       - | 3836 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3837 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3838 | `	pMap->pLast->pPrev = 0;` |
|       - | 3839 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3840 | `	if( pAfter == 0 ){` |
|       - | 3841 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3842 | `		pNode->pNext = 0;` |
|       3 | 3843 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3844 | `		if( pMap->pFirst ){` |
|       3 | 3845 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3846 | `		}` |
|       3 | 3847 | `		pMap->pFirst = pNode;` |
|       2 | 3848 | `	}else{` |
|      17 | 3849 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3850 | `		pNode->pPrev = pOldNext;` |
|      17 | 3851 | `		pNode->pNext = pAfter;` |
|      17 | 3852 | `		pAfter->pPrev = pNode;` |
|      17 | 3853 | `		if( pOldNext ){` |
|      17 | 3854 | `			pOldNext->pNext = pNode;` |
|       9 | 3855 | `		}else{` |
|     ! 0 | 3856 | `			pMap->pLast = pNode;` |
|       - | 3857 | `		}` |
|       - | 3858 | `	}` |
|      16 | 3859 |  |
|       - | 3860 | `/*` |
|       - | 3861 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3862 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3863 | ` * Parameters` |
|       - | 3864 | ` *  $array` |
|       - | 3865 | ` *    The input array.` |
|       - | 3866 | ` *  $offset` |
|       - | 3867 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3868 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3869 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3870 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3871 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3872 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3873 | ` *  $length (optional)` |
|       - | 3874 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3875 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3876 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3877 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3878 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3879 | ` *  $replacement (optional)` |
|       - | 3880 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3881 | ` *    with elements from this array.` |
|       - | 3882 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3883 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3884 | ` *    offset.` |
|       - | 3885 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3886 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3887 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3888 | ` * Return` |
|       - | 3889 | ` *   A new array consisting of the extracted elements.` |
|       - | 3890 | ` */` |
|      54 | 3891 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3892 |  |
|       - | 3893 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3894 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3895 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3896 | `	int iLength,iOfft,i;` |
|       - | 3897 | `	sxi32 rc;` |
|      56 | 3898 | `	if( nArg < 2 ){` |
|       7 | 3899 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3900 | `			"ArgumentCountError",` |
|       - | 3901 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3902 | `			nArg` |
|       - | 3903 | `			);` |
|       - | 3904 | `	}` |
|      52 | 3905 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3906 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3907 | `			"TypeError",` |
|       - | 3908 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3909 | `			ph7_type_name(apArg[0])` |
|       - | 3910 | `			);` |
|       - | 3911 | `	}` |
|       - | 3912 | `	/* Point to the internal representation of the target array */` |
|      49 | 3913 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3914 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3915 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3916 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3917 | `	if( iOfft < 0 ){` |
|       7 | 3918 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3919 | `		if( iOfft < 0 ){` |
|       3 | 3920 | `			iOfft = 0;` |
|       2 | 3921 | `		}` |
|      46 | 3922 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3923 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3924 | `	}` |
|       - | 3925 | `	/* Get the length and clamp to valid range.` |
|       - | 3926 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3927 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3928 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3929 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3930 | `		if( iLength < 0 ){` |
|       7 | 3931 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3932 | `			if( iLength < 0 ){` |
|       3 | 3933 | `				iLength = 0;` |
|       1 | 3934 | `			}` |
|       3 | 3935 | `		}` |
|      31 | 3936 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3937 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3938 | `		}` |
|      15 | 3939 | `	}` |
|       - | 3940 | `	/* Create the result array for removed elements */` |
|      49 | 3941 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3942 | `	if( pArray == 0 ){` |
|     ! 0 | 3943 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3944 | `		return PH7_OK;` |
|       - | 3945 | `	}` |
|       - | 3946 | `	/* Get replacement array if provided */` |
|      49 | 3947 | `	pRep = 0;` |
|      49 | 3948 | `	if( nArg > 3 ){` |
|      21 | 3949 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3950 | `			/* Perform an array cast */` |
|       3 | 3951 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3952 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3953 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3954 | `			}` |
|       2 | 3955 | `		}else{` |
|      19 | 3956 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3957 | `		}` |
|      21 | 3958 | `		if( pRep ){` |
|       - | 3959 | `			/* Reset the loop cursor */` |
|      21 | 3960 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3961 | `		}` |
|      10 | 3962 | `	}` |
|       - | 3963 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3964 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3965 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3966 | `		return PH7_OK;` |
|       - | 3967 | `	}` |
|       - | 3968 | `	/* Navigate to the offset position */` |
|      41 | 3969 | `	pCur = pSrc->pFirst;` |
|      85 | 3970 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3971 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3972 | `	}` |
|       - | 3973 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3974 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3975 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3976 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3977 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3978 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3979 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3980 | `		pPrev = pCur->pPrev;` |
|      71 | 3981 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3982 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3983 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3984 | `			break;` |
|       - | 3985 | `		}` |
|      71 | 3986 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3987 | `	}` |
|       - | 3988 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3989 | `	if( pRep ){` |
|       - | 3990 | `		ph7_value sSafeVal;` |
|      61 | 3991 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3992 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3993 | `			if( pRvalue ){` |
|       - | 3994 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3995 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3996 | `				 * since it points into that same pool. */` |
|      31 | 3997 | `				sSafeVal = *pRvalue;` |
|      31 | 3998 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 3999 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 4000 | `					pNewNode = pSrc->pLast;` |
|      31 | 4001 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 4002 | `					pInsertAfter = pNewNode;` |
|      15 | 4003 | `				}` |
|      15 | 4004 | `			}` |
|       1 | 4005 | `		}` |
|      10 | 4006 | `	}` |
|       - | 4007 | `	/* Return the freshly created array */` |
|      41 | 4008 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 4009 | `	return PH7_OK;` |
|      29 | 4010 |  |
|       - | 4011 | `/*` |
|       - | 4012 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 4013 | ` *  Checks if a value exists in an array.` |
|       - | 4014 | ` * Parameters` |
|       - | 4015 | ` *  $needle` |
|       - | 4016 | ` *   The searched value.` |
|       - | 4017 | ` *   Note:` |
|       - | 4018 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 4019 | ` * $haystack` |
|       - | 4020 | ` *  The target array.` |
|       - | 4021 | ` * $strict` |
|       - | 4022 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 4023 | ` *  will also check the types of the needle in the haystack.` |
|       - | 4024 | ` */` |
|   29404 | 4025 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4026 |  |
|       - | 4027 | `	ph7_value *pNeedle;` |
|       - | 4028 | `	int bStrict;` |
|       - | 4029 | `	int rc;` |
|   29406 | 4030 | `	if( nArg < 2 ){` |
|       - | 4031 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 4032 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4033 | `		return PH7_OK;` |
|       - | 4034 | `	}` |
|   29406 | 4035 | `	pNeedle = apArg[0];` |
|   29406 | 4036 | `	bStrict = 0;` |
|   29406 | 4037 | `	if( nArg > 2 ){` |
|       5 | 4038 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 4039 | `	}` |
|   29406 | 4040 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4041 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 4042 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 4043 | `		/* Set the comparison result */` |
|     ! 0 | 4044 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 4045 | `		return PH7_OK;` |
|       - | 4046 | `	}` |
|       - | 4047 | `	/* Perform the lookup */` |
|   29406 | 4048 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 4049 | `	/* Lookup result */` |
|   29406 | 4050 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   29406 | 4051 | `	return PH7_OK;` |
|   14704 | 4052 |  |
|       - | 4053 | `/*` |
|       - | 4054 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 4055 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 4056 | ` * Parameters` |
|       - | 4057 | ` * $needle` |
|       - | 4058 | ` *   The searched value.` |
|       - | 4059 | ` * $haystack` |
|       - | 4060 | ` *   The array.` |
|       - | 4061 | ` * $strict` |
|       - | 4062 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 4063 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 4064 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 4065 | ` * Return` |
|       - | 4066 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 4067 | ` */` |
|      28 | 4068 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4069 |  |
|       - | 4070 | `	ph7_hashmap_node *pEntry;` |
|       - | 4071 | `	ph7_value *pVal,sNeedle;` |
|       - | 4072 | `	ph7_hashmap *pMap;` |
|       - | 4073 | `	ph7_value sVal;` |
|       - | 4074 | `	int bStrict;` |
|       - | 4075 | `	sxu32 n;` |
|       - | 4076 | `	int rc;` |
|      30 | 4077 | `	if( nArg < 2 ){` |
|       - | 4078 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 4079 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4080 | `			"ArgumentCountError",` |
|       - | 4081 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4082 | `			nArg` |
|       - | 4083 | `			);` |
|       - | 4084 | `	}` |
|      26 | 4085 | `	bStrict = FALSE;` |
|      26 | 4086 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4087 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4088 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4089 | `			"TypeError",` |
|       - | 4090 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4091 | `			ph7_type_name(apArg[1])` |
|       - | 4092 | `			);` |
|       - | 4093 | `	}` |
|      24 | 4094 | `	if( nArg > 2 ){` |
|       - | 4095 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4096 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4097 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4098 | `				"TypeError",` |
|       - | 4099 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4100 | `				ph7_type_name(apArg[2])` |
|       - | 4101 | `				);` |
|       - | 4102 | `		}` |
|       9 | 4103 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4104 | `	}` |
|       - | 4105 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4106 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4107 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4108 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4109 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4110 | `	pEntry = pMap->pFirst;` |
|      21 | 4111 | `	n = pMap->nEntry;` |
|      23 | 4112 | `	for(;;){` |
|      47 | 4113 | `		if( !n ){` |
|       9 | 4114 | `			break;` |
|       - | 4115 | `		}` |
|       - | 4116 | `		/* Extract node value */` |
|      39 | 4117 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4118 | `		if( pVal ){` |
|       - | 4119 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4120 | `			 * can change their type.` |
|       - | 4121 | `			 */` |
|      39 | 4122 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4123 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4124 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4125 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4126 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4127 | `			if( rc == 0 ){` |
|       - | 4128 | `				/* Match found,return key */` |
|      13 | 4129 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4130 | `					/* INT key */` |
|       7 | 4131 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4132 | `				}else{` |
|       7 | 4133 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4134 | `					/* Blob key */` |
|       7 | 4135 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4136 | `				}` |
|      13 | 4137 | `				return PH7_OK;` |
|       - | 4138 | `			}` |
|      13 | 4139 | `		}` |
|       - | 4140 | `		/* Point to the next entry */` |
|      27 | 4141 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4142 | `		n--;` |
|       1 | 4143 | `	}` |
|       - | 4144 | `	/* No such value,return FALSE */` |
|       9 | 4145 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4146 | `	return PH7_OK;` |
|      16 | 4147 |  |
|       - | 4148 | `/*` |
|       - | 4149 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4150 | ` *  Computes the difference of arrays.` |
|       - | 4151 | ` * Parameters` |
|       - | 4152 | ` *  $array1` |
|       - | 4153 | ` *    The array to compare from` |
|       - | 4154 | ` *  $array2` |
|       - | 4155 | ` *    An array to compare against` |
|       - | 4156 | ` *  $...` |
|       - | 4157 | ` *   More arrays to compare against` |
|       - | 4158 | ` * Return` |
|       - | 4159 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4160 | ` *  are not present in any of the other arrays.` |
|       - | 4161 | ` */` |
|      22 | 4162 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4163 |  |
|       - | 4164 | `	ph7_hashmap_node *pEntry;` |
|       - | 4165 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4166 | `	ph7_value *pArray;` |
|       - | 4167 | `	ph7_value *pVal;` |
|       - | 4168 | `	sxi32 rc;` |
|       - | 4169 | `	sxu32 n;` |
|       - | 4170 | `	int i;` |
|       - | 4171 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4172 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4173 | `	 * debugging difficult. */` |
|      24 | 4174 | `	if( nArg < 1 ){` |
|       4 | 4175 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4176 | `			"ArgumentCountError",` |
|       - | 4177 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4178 | `			nArg` |
|       - | 4179 | `			);` |
|       - | 4180 | `	}` |
|      22 | 4181 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4182 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4183 | `			"TypeError",` |
|       - | 4184 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4185 | `			ph7_type_name(apArg[0])` |
|       - | 4186 | `			);` |
|       - | 4187 | `	}` |
|      36 | 4188 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4189 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4190 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4191 | `				"TypeError",` |
|       - | 4192 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4193 | `				i + 1,` |
|       2 | 4194 | `				ph7_type_name(apArg[i])` |
|       - | 4195 | `				);` |
|       - | 4196 | `		}` |
|       9 | 4197 | `	}` |
|      17 | 4198 | `	if( nArg == 1 ){` |
|       - | 4199 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4200 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4201 | `		return PH7_OK;` |
|       - | 4202 | `	}` |
|       - | 4203 | `	/* Create a new array */` |
|      15 | 4204 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4205 | `	if( pArray == 0 ){` |
|     ! 0 | 4206 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4207 | `		return PH7_OK;` |
|       - | 4208 | `	}` |
|       - | 4209 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4210 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4211 | `	/* Perform the diff */` |
|      15 | 4212 | `	pEntry = pSrc->pFirst;` |
|      15 | 4213 | `	n = pSrc->nEntry;` |
|      27 | 4214 | `	for(;;){` |
|      55 | 4215 | `		if( n < 1 ){` |
|      15 | 4216 | `			break;` |
|       - | 4217 | `		}` |
|       - | 4218 | `		/* Extract the node value */` |
|      41 | 4219 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4220 | `		if( pVal ){` |
|      69 | 4221 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4222 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4223 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4224 | `				/* Perform the lookup */` |
|      45 | 4225 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4226 | `				if( rc == SXRET_OK ){` |
|       - | 4227 | `					/* Value exist */` |
|      17 | 4228 | `					break;` |
|       - | 4229 | `				}` |
|      15 | 4230 | `			}` |
|      41 | 4231 | `			if( i >= nArg ){` |
|       - | 4232 | `				/* Perform the insertion */` |
|      25 | 4233 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4234 | `			}` |
|      20 | 4235 | `		}` |
|       - | 4236 | `		/* Point to the next entry */` |
|      41 | 4237 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4238 | `		n--;` |
|       1 | 4239 | `	}` |
|       - | 4240 | `	/* Return the freshly created array */` |
|      15 | 4241 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4242 | `	return PH7_OK;` |
|      13 | 4243 |  |
|       - | 4244 | `/*` |
|       - | 4245 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4246 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4247 | ` * Parameters` |
|       - | 4248 | ` *  $array1` |
|       - | 4249 | ` *    The array to compare from` |
|       - | 4250 | ` *  $array2` |
|       - | 4251 | ` *    An array to compare against` |
|       - | 4252 | ` *  $...` |
|       - | 4253 | ` *   More arrays to compare against.` |
|       - | 4254 | ` * $callback` |
|       - | 4255 | ` *  The callback comparison function.` |
|       - | 4256 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4257 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4258 | ` *  than the second.` |
|       - | 4259 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4260 | ` * Return` |
|       - | 4261 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4262 | ` *  are not present in any of the other arrays.` |
|       - | 4263 | ` */` |
|      22 | 4264 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4265 |  |
|       - | 4266 | `	ph7_hashmap_node *pEntry;` |
|       - | 4267 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4268 | `	ph7_value *pCallback;` |
|       - | 4269 | `	ph7_value *pArray;` |
|       - | 4270 | `	ph7_value *pVal;` |
|       - | 4271 | `	sxi32 rc;` |
|       - | 4272 | `	sxu32 n;` |
|       - | 4273 | `	int i;` |
|       - | 4274 |  |
|       - | 4275 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      24 | 4276 | `	if( nArg < 2 ){` |
|       4 | 4277 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4278 | `			"ArgumentCountError",` |
|       - | 4279 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4280 | `			nArg` |
|       - | 4281 | `			);` |
|       - | 4282 | `	}` |
|      22 | 4283 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4284 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4285 | `			"TypeError",` |
|       - | 4286 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4287 | `			ph7_type_name(apArg[0])` |
|       - | 4288 | `			);` |
|       - | 4289 | `	}` |
|       - | 4290 |  |
|      20 | 4291 | `	if( nArg == 2 ){` |
|       - | 4292 | `		/* Only the original array and the callback were provided. */` |
|       - | 4293 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4294 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4295 | `		 * validation order.` |
|       - | 4296 | `		 */` |
|       4 | 4297 | `	} else {` |
|       - | 4298 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      24 | 4299 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      16 | 4300 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      10 | 4301 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4302 | `					"TypeError",` |
|       - | 4303 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4304 | `					i + 1,` |
|       6 | 4305 | `					ph7_type_name(apArg[i])` |
|       - | 4306 | `					);` |
|       - | 4307 | `			}` |
|       6 | 4308 | `		}` |
|       - | 4309 | `	}` |
|       - | 4310 |  |
|       - | 4311 | `	/* Identify the callback (always expected as the last argument). */` |
|      14 | 4312 | `	pCallback = apArg[nArg - 1];` |
|       - | 4313 | `	/* Validate the callback to match PHP's error messages. */` |
|      14 | 4314 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       7 | 4315 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4316 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4317 | `				"TypeError",` |
|       - | 4318 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4319 | `				nArg` |
|       - | 4320 | `				);` |
|       - | 4321 | `		}` |
|       5 | 4322 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4323 | `			int len;` |
|       3 | 4324 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4325 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4326 | `				"TypeError",` |
|       - | 4327 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4328 | `				nArg,` |
|       1 | 4329 | `				zName` |
|       - | 4330 | `				);` |
|       - | 4331 | `		}` |
|       4 | 4332 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4333 | `			"TypeError",` |
|       - | 4334 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4335 | `			nArg` |
|       - | 4336 | `			);` |
|       - | 4337 | `	}` |
|       - | 4338 |  |
|       7 | 4339 | `	if( nArg == 2 ){` |
|       - | 4340 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4341 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4342 | `		return PH7_OK;` |
|       - | 4343 | `	}` |
|       - | 4344 |  |
|       - | 4345 | `	/* Create a new array */` |
|       5 | 4346 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4347 | `	if( pArray == 0 ){` |
|     ! 0 | 4348 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4349 | `		return PH7_OK;` |
|       - | 4350 | `	}` |
|       - | 4351 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 4352 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4353 | `	/* Perform the diff */` |
|       5 | 4354 | `	pEntry = pSrc->pFirst;` |
|       5 | 4355 | `	n = pSrc->nEntry;` |
|       5 | 4356 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 4357 | `	for(;;){` |
|      11 | 4358 | `		if( n < 1 ){` |
|       3 | 4359 | `			break;` |
|       - | 4360 | `		}` |
|       - | 4361 | `		/* Extract the node value */` |
|       9 | 4362 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4363 | `		if( pVal ){` |
|      15 | 4364 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4365 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4366 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4367 | `				/* Perform the lookup */` |
|       9 | 4368 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       9 | 4369 | `				if( rc == SXRET_OK ){` |
|       - | 4370 | `					/* Value exist */` |
|       3 | 4371 | `					break;` |
|       - | 4372 | `				}` |
|       4 | 4373 | `			}` |
|       9 | 4374 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 4375 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 4376 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 4377 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 4378 | `				return PH7_EXCEPTION;` |
|       - | 4379 | `			}` |
|       7 | 4380 | `			if( i >= (nArg - 1)){` |
|       - | 4381 | `				/* Perform the insertion */` |
|       5 | 4382 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4383 | `			}` |
|       3 | 4384 | `		}` |
|       - | 4385 | `		/* Point to the next entry */` |
|       7 | 4386 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4387 | `		n--;` |
|       1 | 4388 | `	}` |
|       - | 4389 | `	/* Return the freshly created array */` |
|       3 | 4390 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4391 | `	return PH7_OK;` |
|      13 | 4392 |  |
|       - | 4393 | `/*` |
|       - | 4394 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4395 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4396 | ` * Parameters` |
|       - | 4397 | ` *  $array1` |
|       - | 4398 | ` *    The array to compare from` |
|       - | 4399 | ` *  $array2` |
|       - | 4400 | ` *    An array to compare against` |
|       - | 4401 | ` *  $...` |
|       - | 4402 | ` *   More arrays to compare against` |
|       - | 4403 | ` * Return` |
|       - | 4404 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4405 | ` *  are not present in any of the other arrays.` |
|       - | 4406 | ` */` |
|      20 | 4407 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4408 |  |
|       - | 4409 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4410 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4411 | `	ph7_value *pArray;` |
|       - | 4412 | `	ph7_value *pVal;` |
|       - | 4413 | `	sxi32 rc;` |
|       - | 4414 | `	sxu32 n;` |
|       - | 4415 | `	int i;` |
|       - | 4416 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4417 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4418 | `	 * accompanying integration tests to pass. */` |
|      22 | 4419 | `	if( nArg < 1 ){` |
|       4 | 4420 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4421 | `			"ArgumentCountError",` |
|       - | 4422 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4423 | `			nArg` |
|       - | 4424 | `			);` |
|       - | 4425 | `	}` |
|      20 | 4426 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4427 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4428 | `			"TypeError",` |
|       - | 4429 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4430 | `			ph7_type_name(apArg[0])` |
|       - | 4431 | `			);` |
|       - | 4432 | `	}` |
|      32 | 4433 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4434 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4435 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4436 | `				"TypeError",` |
|       - | 4437 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4438 | `				i + 1,` |
|       4 | 4439 | `				ph7_type_name(apArg[i])` |
|       - | 4440 | `				);` |
|       - | 4441 | `		}` |
|       9 | 4442 | `	}` |
|      13 | 4443 | `	if( nArg == 1 ){` |
|       - | 4444 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4445 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4446 | `		return PH7_OK;` |
|       - | 4447 | `	}` |
|       - | 4448 | `	/* Create a new array */` |
|      11 | 4449 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4450 | `	if( pArray == 0 ){` |
|     ! 0 | 4451 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4452 | `		return PH7_OK;` |
|       - | 4453 | `	}` |
|       - | 4454 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4455 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4456 | `	/* Perform the diff */` |
|      11 | 4457 | `	pEntry = pSrc->pFirst;` |
|      11 | 4458 | `	n = pSrc->nEntry;` |
|      11 | 4459 | `	pN1 = pN2 = 0;` |
|      29 | 4460 | `	for(;;){` |
|       - | 4461 | `		int keep;` |
|      35 | 4462 | `		if( n < 1 ){` |
|      11 | 4463 | `			break;` |
|       - | 4464 | `		}` |
|       - | 4465 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4466 | `		keep = 1;` |
|      41 | 4467 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4468 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4469 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4470 | `			/* Perform a key lookup first */` |
|      29 | 4471 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4472 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4473 | `			}else{` |
|      17 | 4474 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4475 | `			}` |
|      29 | 4476 | `			if( rc != SXRET_OK ){` |
|       - | 4477 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4478 | `				continue;` |
|       - | 4479 | `			}` |
|       - | 4480 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4481 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4482 | `			if( pVal ){` |
|       - | 4483 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4484 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4485 | `				if( pVal2 ){` |
|      15 | 4486 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4487 | `					if( cmp == 0 ){` |
|       - | 4488 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4489 | `						keep = 0;` |
|      13 | 4490 | `						break;` |
|       - | 4491 | `					}` |
|       1 | 4492 | `				}` |
|       1 | 4493 | `			}` |
|       2 | 4494 | `		}` |
|      25 | 4495 | `		if( keep ){` |
|       - | 4496 | `			/* Perform the insertion */` |
|      13 | 4497 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4498 | `		}` |
|       - | 4499 | `		/* Point to the next entry */` |
|      25 | 4500 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4501 | `		n--;` |
|       1 | 4502 | `	}` |
|       - | 4503 | `	/* Return the freshly created array */` |
|      11 | 4504 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4505 | `	return PH7_OK;` |
|      12 | 4506 |  |
|       - | 4507 | `/*` |
|       - | 4508 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4509 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4510 | ` *  by a user supplied callback function.` |
|       - | 4511 | ` * Parameters` |
|       - | 4512 | ` *  $array1` |
|       - | 4513 | ` *    The array to compare from` |
|       - | 4514 | ` *  $array2` |
|       - | 4515 | ` *    An array to compare against` |
|       - | 4516 | ` *  $...` |
|       - | 4517 | ` *   More arrays to compare against.` |
|       - | 4518 | ` *  $key_compare_func` |
|       - | 4519 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4520 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4521 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4522 | ` * Return` |
|       - | 4523 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4524 | ` *  are not present in any of the other arrays.` |
|       - | 4525 | ` */` |
|      24 | 4526 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4527 |  |
|       - | 4528 | `	ph7_hashmap_node *pEntry;` |
|       - | 4529 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4530 | `	ph7_value *pCallback;` |
|       - | 4531 | `	ph7_value *pArray;` |
|       - | 4532 | `	sxi32 rc;` |
|       - | 4533 | `	sxu32 n;` |
|       - | 4534 | `	int i;` |
|       - | 4535 |  |
|       - | 4536 | `	/* Argument validation mimicking PHP errors. */` |
|      26 | 4537 | `	if( nArg < 2 ){` |
|       4 | 4538 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4539 | `			"ArgumentCountError",` |
|       - | 4540 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4541 | `			nArg` |
|       - | 4542 | `			);` |
|       - | 4543 | `	}` |
|      24 | 4544 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4545 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4546 | `			"TypeError",` |
|       - | 4547 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4548 | `			ph7_type_name(apArg[0])` |
|       - | 4549 | `			);` |
|       - | 4550 | `	}` |
|       - | 4551 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4552 | `	 * expected to be a callback. */` |
|      36 | 4553 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      18 | 4554 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4555 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4556 | `				"TypeError",` |
|       - | 4557 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4558 | `				i + 1,` |
|       2 | 4559 | `				ph7_type_name(apArg[i])` |
|       - | 4560 | `				);` |
|       - | 4561 | `		}` |
|       9 | 4562 | `	}` |
|       - | 4563 | `	/* Point to the callback value */` |
|      20 | 4564 | `	pCallback = apArg[nArg - 1];` |
|      20 | 4565 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4566 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4567 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4568 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4569 | `		 * string given" which we also reproduce. */` |
|       7 | 4570 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4571 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4572 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4573 | `				"TypeError",` |
|       - | 4574 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4575 | `				nArg` |
|       - | 4576 | `				);` |
|       - | 4577 | `		}` |
|       5 | 4578 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4579 | `			/* neither array nor string */` |
|       7 | 4580 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4581 | `				"TypeError",` |
|       - | 4582 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4583 | `				nArg` |
|       - | 4584 | `				);` |
|       - | 4585 | `		}` |
|       - | 4586 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4587 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4588 | `			"TypeError",` |
|       - | 4589 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4590 | `			nArg,` |
|     ! 0 | 4591 | `			ph7_type_name(pCallback)` |
|       - | 4592 | `			);` |
|       - | 4593 | `	}` |
|      13 | 4594 | `	if( nArg == 2 ){` |
|       - | 4595 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4596 | `		 * input array. */` |
|       3 | 4597 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4598 | `		return PH7_OK;` |
|       - | 4599 | `	}` |
|       - | 4600 | `	/* Create a new array */` |
|      11 | 4601 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4602 | `	if( pArray == 0 ){` |
|     ! 0 | 4603 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4604 | `		return PH7_OK;` |
|       - | 4605 | `	}` |
|       - | 4606 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4607 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4608 | `	/* Perform the diff */` |
|      11 | 4609 | `	pEntry = pSrc->pFirst;` |
|      11 | 4610 | `	n = pSrc->nEntry;` |
|      21 | 4611 | `	for(;;){` |
|       - | 4612 | `		int keep;` |
|      27 | 4613 | `		if( n < 1 ){` |
|       9 | 4614 | `			break;` |
|       - | 4615 | `		}` |
|      19 | 4616 | `		keep = 1;` |
|      31 | 4617 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4618 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 4619 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4620 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 4621 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 4622 | `			while( pIt ){` |
|       - | 4623 | `				/* build temporary key values for callback */` |
|       - | 4624 | `				ph7_value key1, key2, result;` |
|       - | 4625 | `				/* initialise only once using the appropriate helper */` |
|      33 | 4626 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4627 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4628 | `				}else{` |
|       - | 4629 | `					SyString sStr;` |
|      33 | 4630 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4631 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4632 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 4633 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4634 | `				}` |
|      33 | 4635 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4636 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4637 | `				}else{` |
|       - | 4638 | `					SyString sStr;` |
|      33 | 4639 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4640 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4641 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 4642 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4643 | `				}` |
|      33 | 4644 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4645 | `				/* call user callback with (key1, key2) */` |
|       - | 4646 | `				{` |
|       - | 4647 | `					ph7_value *apK[2];` |
|      33 | 4648 | `					apK[0] = &key1;` |
|      33 | 4649 | `					apK[1] = &key2;` |
|      33 | 4650 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4651 | `				}` |
|      33 | 4652 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 4653 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 4654 | `					 * array_uintersect (which signal back from` |
|       - | 4655 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 4656 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 4657 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 4658 | `					PH7_MemObjRelease(&result);` |
|       3 | 4659 | `					PH7_MemObjRelease(&key1);` |
|       3 | 4660 | `					PH7_MemObjRelease(&key2);` |
|       3 | 4661 | `					return PH7_EXCEPTION;` |
|       - | 4662 | `				}` |
|      31 | 4663 | `				if( rc == SXRET_OK ){` |
|      31 | 4664 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4665 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4666 | `					}` |
|      31 | 4667 | `					if( result.x.iVal == 0 ){` |
|       - | 4668 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4669 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4670 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4671 | `						if( pVal1 && pVal2 ){` |
|      13 | 4672 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4673 | `								keep = 0;` |
|       9 | 4674 | `								PH7_MemObjRelease(&result);` |
|       - | 4675 | `								/* release keys too before breaking */` |
|       9 | 4676 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4677 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4678 | `								break;` |
|       - | 4679 | `							}` |
|       2 | 4680 | `						}` |
|       2 | 4681 | `					}` |
|      11 | 4682 | `				}` |
|      23 | 4683 | `				PH7_MemObjRelease(&result);` |
|      23 | 4684 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4685 | `				PH7_MemObjRelease(&key2);` |
|       - | 4686 | `				/* move to next node */` |
|      23 | 4687 | `				pIt = pIt->pPrev;` |
|      23 | 4688 | `				if( keep == 0 ) break;` |
|       1 | 4689 | `			}` |
|      21 | 4690 | `			if( keep == 0 ) break;` |
|       7 | 4691 | `		}` |
|      17 | 4692 | `		if( keep ){` |
|       - | 4693 | `			/* Perform the insertion */` |
|       9 | 4694 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4695 | `		}` |
|       - | 4696 | `		/* Point to the next entry */` |
|      17 | 4697 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4698 | `		n--;` |
|       1 | 4699 | `	}` |
|       - | 4700 | `	/* Return the freshly created array */` |
|       9 | 4701 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4702 | `	return PH7_OK;` |
|      14 | 4703 |  |
|       - | 4704 | `/*` |
|       - | 4705 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4706 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4707 | ` * Parameters` |
|       - | 4708 | ` *  $array1` |
|       - | 4709 | ` *    The array to compare from` |
|       - | 4710 | ` *  $array2` |
|       - | 4711 | ` *    An array to compare against` |
|       - | 4712 | ` *  $...` |
|       - | 4713 | ` *   More arrays to compare against` |
|       - | 4714 | ` * Return` |
|       - | 4715 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4716 | ` *  in any of the other arrays.` |
|       - | 4717 | ` * Note that NULL is returned on failure.` |
|       - | 4718 | ` */` |
|      14 | 4719 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4720 |  |
|       - | 4721 | `	ph7_hashmap_node *pEntry;` |
|       - | 4722 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4723 | `	ph7_value *pArray;` |
|       - | 4724 | `	sxi32 rc;` |
|       - | 4725 | `	sxu32 n;` |
|       - | 4726 | `	int i;` |
|       - | 4727 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4728 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4729 | `	 * helpers. */` |
|      16 | 4730 | `	if( nArg < 1 ){` |
|       4 | 4731 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4732 | `			"ArgumentCountError",` |
|       - | 4733 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4734 | `			nArg` |
|       - | 4735 | `			);` |
|       - | 4736 | `	}` |
|      14 | 4737 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4738 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4739 | `			"TypeError",` |
|       - | 4740 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4741 | `			ph7_type_name(apArg[0])` |
|       - | 4742 | `			);` |
|       - | 4743 | `	}` |
|      20 | 4744 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4745 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4746 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4747 | `				"TypeError",` |
|       - | 4748 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4749 | `				i + 1,` |
|       2 | 4750 | `				ph7_type_name(apArg[i])` |
|       - | 4751 | `				);` |
|       - | 4752 | `		}` |
|       5 | 4753 | `	}` |
|       9 | 4754 | `	if( nArg == 1 ){` |
|       - | 4755 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4756 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4757 | `		return PH7_OK;` |
|       - | 4758 | `	}` |
|       - | 4759 | `	/* Create a new array */` |
|       7 | 4760 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4761 | `	if( pArray == 0 ){` |
|     ! 0 | 4762 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4763 | `		return PH7_OK;` |
|       - | 4764 | `	}` |
|       - | 4765 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4766 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4767 | `	/* Perfrom the diff */` |
|       7 | 4768 | `	pEntry = pSrc->pFirst;` |
|       7 | 4769 | `	n = pSrc->nEntry;` |
|      12 | 4770 | `	for(;;){` |
|      25 | 4771 | `		if( n < 1 ){` |
|       7 | 4772 | `			break;` |
|       - | 4773 | `		}` |
|      31 | 4774 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4775 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4776 | `				/* ignore */` |
|     ! 0 | 4777 | `				continue;` |
|       - | 4778 | `			}` |
|      23 | 4779 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4780 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4781 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4782 | `				/* Blob lookup */` |
|      17 | 4783 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4784 | `			}else{` |
|       - | 4785 | `				/* Int lookup */` |
|       7 | 4786 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4787 | `			}` |
|      23 | 4788 | `			if( rc == SXRET_OK ){` |
|       - | 4789 | `				/* Key exists,break immediately */` |
|      11 | 4790 | `				break;` |
|       - | 4791 | `			}` |
|       7 | 4792 | `		}` |
|      19 | 4793 | `		if( i >= nArg ){` |
|       - | 4794 | `			/* Perform the insertion */` |
|       9 | 4795 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4796 | `		}` |
|       - | 4797 | `		/* Point to the next entry */` |
|      19 | 4798 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4799 | `		n--;` |
|       1 | 4800 | `	}` |
|       - | 4801 | `	/* Return the freshly created array */` |
|       7 | 4802 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4803 | `	return PH7_OK;` |
|       9 | 4804 |  |
|       - | 4805 | `/*` |
|       - | 4806 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4807 | ` *  Computes the intersection of arrays.` |
|       - | 4808 | ` * Parameters` |
|       - | 4809 | ` *  $array1` |
|       - | 4810 | ` *    The array to compare from` |
|       - | 4811 | ` *  $array2` |
|       - | 4812 | ` *    An array to compare against` |
|       - | 4813 | ` *  $...` |
|       - | 4814 | ` *   More arrays to compare against` |
|       - | 4815 | ` * Return` |
|       - | 4816 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4817 | ` *  in all of the parameters.` |
|       - | 4818 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4819 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4820 | ` */` |
|      22 | 4821 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4822 |  |
|       - | 4823 | `	ph7_hashmap_node *pEntry;` |
|       - | 4824 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4825 | `	ph7_value *pArray;` |
|       - | 4826 | `	ph7_value *pVal;` |
|       - | 4827 | `	sxi32 rc;` |
|       - | 4828 | `	sxu32 n;` |
|       - | 4829 | `	int i;` |
|      24 | 4830 | `	if( nArg < 1 ){` |
|       4 | 4831 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4832 | `			"ArgumentCountError",` |
|       - | 4833 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4834 | `			nArg` |
|       - | 4835 | `			);` |
|       - | 4836 | `	}` |
|      22 | 4837 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4838 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4839 | `			"TypeError",` |
|       - | 4840 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4841 | `			ph7_type_name(apArg[0])` |
|       - | 4842 | `			);` |
|       - | 4843 | `	}` |
|      36 | 4844 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4845 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4846 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4847 | `				"TypeError",` |
|       - | 4848 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4849 | `				i + 1,` |
|       2 | 4850 | `				ph7_type_name(apArg[i])` |
|       - | 4851 | `				);` |
|       - | 4852 | `		}` |
|       9 | 4853 | `	}` |
|      17 | 4854 | `	if( nArg == 1 ){` |
|       - | 4855 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4856 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4857 | `		return PH7_OK;` |
|       - | 4858 | `	}` |
|       - | 4859 | `	/* Create a new array */` |
|      15 | 4860 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4861 | `	if( pArray == 0 ){` |
|     ! 0 | 4862 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4863 | `		return PH7_OK;` |
|       - | 4864 | `	}` |
|       - | 4865 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4866 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4867 | `	/* Perform the intersection */` |
|      15 | 4868 | `	pEntry = pSrc->pFirst;` |
|      15 | 4869 | `	n = pSrc->nEntry;` |
|      31 | 4870 | `	for(;;){` |
|      63 | 4871 | `		if( n < 1 ){` |
|      15 | 4872 | `			break;` |
|       - | 4873 | `		}` |
|       - | 4874 | `		/* Extract the node value */` |
|      49 | 4875 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4876 | `		if( pVal ){` |
|      79 | 4877 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4878 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4879 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4880 | `				/* Perform the lookup */` |
|      55 | 4881 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4882 | `				if( rc != SXRET_OK ){` |
|       - | 4883 | `					/* Value does not exist */` |
|      25 | 4884 | `					break;` |
|       - | 4885 | `				}` |
|      16 | 4886 | `			}` |
|      49 | 4887 | `			if( i >= nArg ){` |
|       - | 4888 | `				/* Perform the insertion */` |
|      25 | 4889 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4890 | `			}` |
|      24 | 4891 | `		}` |
|       - | 4892 | `		/* Point to the next entry */` |
|      49 | 4893 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4894 | `		n--;` |
|       1 | 4895 | `	}` |
|       - | 4896 | `	/* Return the freshly created array */` |
|      15 | 4897 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4898 | `	return PH7_OK;` |
|      13 | 4899 |  |
|       - | 4900 | `/*` |
|       - | 4901 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4902 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4903 | ` * Parameters` |
|       - | 4904 | ` *  $array1` |
|       - | 4905 | ` *    The array to compare from` |
|       - | 4906 | ` *  $array2` |
|       - | 4907 | ` *    An array to compare against` |
|       - | 4908 | ` *  $...` |
|       - | 4909 | ` *   More arrays to compare against` |
|       - | 4910 | ` * Return` |
|       - | 4911 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4912 | ` *  in all the arguments, with matching keys.` |
|       - | 4913 | ` */` |
|      22 | 4914 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4915 |  |
|       - | 4916 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4917 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4918 | `	ph7_value *pArray;` |
|       - | 4919 | `	ph7_value *pVal;` |
|       - | 4920 | `	sxi32 rc;` |
|       - | 4921 | `	sxu32 n;` |
|       - | 4922 | `	int i;` |
|      24 | 4923 | `	if( nArg < 1 ){` |
|       4 | 4924 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4925 | `			"ArgumentCountError",` |
|       - | 4926 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4927 | `			nArg` |
|       - | 4928 | `			);` |
|       - | 4929 | `	}` |
|      22 | 4930 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4931 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4932 | `			"TypeError",` |
|       - | 4933 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4934 | `			ph7_type_name(apArg[0])` |
|       - | 4935 | `			);` |
|       - | 4936 | `	}` |
|      36 | 4937 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4938 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4939 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4940 | `				"TypeError",` |
|       - | 4941 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4942 | `				i + 1,` |
|       2 | 4943 | `				ph7_type_name(apArg[i])` |
|       - | 4944 | `				);` |
|       - | 4945 | `		}` |
|       9 | 4946 | `	}` |
|      17 | 4947 | `	if( nArg == 1 ){` |
|       - | 4948 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4949 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4950 | `		return PH7_OK;` |
|       - | 4951 | `	}` |
|       - | 4952 | `	/* Create a new array */` |
|      15 | 4953 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4954 | `	if( pArray == 0 ){` |
|     ! 0 | 4955 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4956 | `		return PH7_OK;` |
|       - | 4957 | `	}` |
|       - | 4958 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4959 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4960 | `	/* Perform the intersection */` |
|      15 | 4961 | `	pEntry = pSrc->pFirst;` |
|      15 | 4962 | `	n = pSrc->nEntry;` |
|      15 | 4963 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4964 | `	for(;;){` |
|      47 | 4965 | `		if( n < 1 ){` |
|      15 | 4966 | `			break;` |
|       - | 4967 | `		}` |
|       - | 4968 | `		/* Extract the node value */` |
|      33 | 4969 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4970 | `		if( pVal ){` |
|      53 | 4971 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4972 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4973 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4974 | `				/* Perform a key lookup first */` |
|      37 | 4975 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4976 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4977 | `				}else{` |
|      23 | 4978 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4979 | `				}` |
|      37 | 4980 | `				if( rc != SXRET_OK ){` |
|       - | 4981 | `					/* No such key,break immediately */` |
|       7 | 4982 | `					break;` |
|       - | 4983 | `				}` |
|       - | 4984 | `				/* Perform the lookup */` |
|      31 | 4985 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4986 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4987 | `					/* Value does not exist */` |
|       6 | 4988 | `					break;` |
|       - | 4989 | `				}` |
|      11 | 4990 | `			}` |
|      33 | 4991 | `			if( i >= nArg ){` |
|       - | 4992 | `				/* Perform the insertion */` |
|      17 | 4993 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4994 | `			}` |
|      16 | 4995 | `		}` |
|       - | 4996 | `		/* Point to the next entry */` |
|      33 | 4997 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4998 | `		n--;` |
|       1 | 4999 | `	}` |
|       - | 5000 | `	/* Return the freshly created array */` |
|      15 | 5001 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5002 | `	return PH7_OK;` |
|      13 | 5003 |  |
|       - | 5004 | `/*` |
|       - | 5005 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 5006 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 5007 | ` * Parameters` |
|       - | 5008 | ` *  $array1` |
|       - | 5009 | ` *    The array to compare from` |
|       - | 5010 | ` *  $...` |
|       - | 5011 | ` *   More arrays to compare against` |
|       - | 5012 | ` * Return` |
|       - | 5013 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 5014 | ` *  have keys that are present in all arguments.` |
|       - | 5015 | ` * Note that NULL is returned on failure.` |
|       - | 5016 | ` */` |
|      22 | 5017 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5018 |  |
|       - | 5019 | `	ph7_hashmap_node *pEntry;` |
|       - | 5020 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5021 | `	ph7_value *pArray;` |
|       - | 5022 | `	sxi32 rc;` |
|       - | 5023 | `	sxu32 n;` |
|       - | 5024 | `	int i;` |
|      24 | 5025 | `	if( nArg < 1 ){` |
|       4 | 5026 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5027 | `			"ArgumentCountError",` |
|       - | 5028 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 5029 | `			nArg` |
|       - | 5030 | `			);` |
|       - | 5031 | `	}` |
|      22 | 5032 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5033 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5034 | `			"TypeError",` |
|       - | 5035 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5036 | `			ph7_type_name(apArg[0])` |
|       - | 5037 | `			);` |
|       - | 5038 | `	}` |
|      36 | 5039 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5040 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5041 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5042 | `				"TypeError",` |
|       - | 5043 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5044 | `				i + 1,` |
|       2 | 5045 | `				ph7_type_name(apArg[i])` |
|       - | 5046 | `				);` |
|       - | 5047 | `		}` |
|       9 | 5048 | `	}` |
|      17 | 5049 | `	if( nArg == 1 ){` |
|       - | 5050 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5051 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5052 | `		return PH7_OK;` |
|       - | 5053 | `	}` |
|       - | 5054 | `	/* Create a new array */` |
|      15 | 5055 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5056 | `	if( pArray == 0 ){` |
|     ! 0 | 5057 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5058 | `		return PH7_OK;` |
|       - | 5059 | `	}` |
|       - | 5060 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 5061 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5062 | `	/* Perform the intersection */` |
|      15 | 5063 | `	pEntry = pSrc->pFirst;` |
|      15 | 5064 | `	n = pSrc->nEntry;` |
|      24 | 5065 | `	for(;;){` |
|      49 | 5066 | `		if( n < 1 ){` |
|      15 | 5067 | `			break;` |
|       - | 5068 | `		}` |
|      57 | 5069 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 5070 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 5071 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 5072 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5073 | `				/* Blob lookup */` |
|      27 | 5074 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 5075 | `			}else{` |
|       - | 5076 | `				/* Int key */` |
|      13 | 5077 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5078 | `			}` |
|      39 | 5079 | `			if( rc != SXRET_OK ){` |
|       - | 5080 | `				/* Key does not exist, break immediately */` |
|      17 | 5081 | `				break;` |
|       - | 5082 | `			}` |
|      12 | 5083 | `		}` |
|      35 | 5084 | `		if( i >= nArg ){` |
|       - | 5085 | `			/* Perform the insertion */` |
|      19 | 5086 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 5087 | `		}` |
|       - | 5088 | `		/* Point to the next entry */` |
|      35 | 5089 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 5090 | `		n--;` |
|       1 | 5091 | `	}` |
|       - | 5092 | `	/* Return the freshly created array */` |
|      15 | 5093 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5094 | `	return PH7_OK;` |
|      13 | 5095 |  |
|       - | 5096 | `/*` |
|       - | 5097 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 5098 | ` *  Computes the intersection of arrays.` |
|       - | 5099 | ` * Parameters` |
|       - | 5100 | ` *  $array1` |
|       - | 5101 | ` *    The array to compare from` |
|       - | 5102 | ` *  $array2` |
|       - | 5103 | ` *    An array to compare against` |
|       - | 5104 | ` *  $...` |
|       - | 5105 | ` *   More arrays to compare against` |
|       - | 5106 | ` * $callback` |
|       - | 5107 | ` *  The callback comparison function.` |
|       - | 5108 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5109 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5110 | ` *  than the second.` |
|       - | 5111 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5112 | ` * Return` |
|       - | 5113 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5114 | ` *  in all of the parameters. .` |
|       - | 5115 | ` * Note that NULL is returned on failure.` |
|       - | 5116 | ` */` |
|      26 | 5117 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5118 |  |
|       - | 5119 | `	ph7_hashmap_node *pEntry;` |
|       - | 5120 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5121 | `	ph7_value *pCallback;` |
|       - | 5122 | `	ph7_value *pArray;` |
|       - | 5123 | `	ph7_value *pVal;` |
|       - | 5124 | `	sxi32 rc;` |
|       - | 5125 | `	sxu32 n;` |
|       - | 5126 | `	int i;` |
|       - | 5127 |  |
|       - | 5128 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      28 | 5129 | `	if( nArg < 2 ){` |
|       4 | 5130 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5131 | `			"ArgumentCountError",` |
|       - | 5132 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5133 | `			nArg` |
|       - | 5134 | `			);` |
|       - | 5135 | `	}` |
|      26 | 5136 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5137 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5138 | `			"TypeError",` |
|       - | 5139 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5140 | `			ph7_type_name(apArg[0])` |
|       - | 5141 | `			);` |
|       - | 5142 | `	}` |
|       - | 5143 |  |
|      24 | 5144 | `	if( nArg == 2 ){` |
|       - | 5145 | `		/* Only the original array and the callback were provided. */` |
|       - | 5146 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5147 | `		 * validation ordering. */` |
|       3 | 5148 | `	} else {` |
|       - | 5149 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      36 | 5150 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      20 | 5151 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5152 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5153 | `					"TypeError",` |
|       - | 5154 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5155 | `					i + 1,` |
|       2 | 5156 | `					ph7_type_name(apArg[i])` |
|       - | 5157 | `					);` |
|       - | 5158 | `			}` |
|      10 | 5159 | `		}` |
|       - | 5160 | `	}` |
|       - | 5161 |  |
|       - | 5162 | `	/* Identify the callback (always expected as the last argument). */` |
|      22 | 5163 | `	pCallback = apArg[nArg - 1];` |
|       - | 5164 | `	/* Validate the callback to match PHP's error messages. */` |
|      22 | 5165 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      11 | 5166 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5167 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5168 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5169 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5170 | `			 */` |
|       7 | 5171 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       7 | 5172 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5173 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5174 | `					"TypeError",` |
|       - | 5175 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5176 | `					nArg` |
|       - | 5177 | `					);` |
|       - | 5178 | `			}` |
|       - | 5179 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5180 | `			{` |
|       5 | 5181 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       5 | 5182 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       5 | 5183 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5184 | `					int nMethodLen;` |
|       5 | 5185 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       5 | 5186 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       5 | 5187 | `					if( pClass ){` |
|       - | 5188 | `						/* Class exists but method is missing. */` |
|       4 | 5189 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5190 | `							"TypeError",` |
|       - | 5191 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5192 | `							nArg,` |
|       1 | 5193 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5194 | `							zMethod` |
|       - | 5195 | `							);` |
|       - | 5196 | `					}` |
|       - | 5197 | `					/* Class not found */` |
|       - | 5198 | `					{` |
|       - | 5199 | `						int nName;` |
|       3 | 5200 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5201 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5202 | `							"TypeError",` |
|       - | 5203 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5204 | `							nArg,` |
|       1 | 5205 | `							zName` |
|       - | 5206 | `							);` |
|       - | 5207 | `					}` |
|       - | 5208 | `				}` |
|       - | 5209 | `			}` |
|       - | 5210 | `			/* Fallback message */` |
|     ! 0 | 5211 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5212 | `				"TypeError",` |
|       - | 5213 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5214 | `				nArg` |
|       - | 5215 | `				);` |
|       - | 5216 | `		}` |
|       5 | 5217 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5218 | `			int len;` |
|       3 | 5219 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5220 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5221 | `				"TypeError",` |
|       - | 5222 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5223 | `				nArg,` |
|       1 | 5224 | `				zName` |
|       - | 5225 | `				);` |
|       - | 5226 | `		}` |
|       4 | 5227 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5228 | `			"TypeError",` |
|       - | 5229 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5230 | `			nArg` |
|       - | 5231 | `			);` |
|       - | 5232 | `	}` |
|       - | 5233 |  |
|      11 | 5234 | `	if( nArg == 2 ){` |
|       - | 5235 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5236 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5237 | `		return PH7_OK;` |
|       - | 5238 | `	}` |
|       - | 5239 |  |
|       - | 5240 | `	/* Create a new array */` |
|       7 | 5241 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5242 | `	if( pArray == 0 ){` |
|     ! 0 | 5243 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5244 | `		return PH7_OK;` |
|       - | 5245 | `	}` |
|       - | 5246 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 5247 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5248 | `	/* Perform the intersection */` |
|       7 | 5249 | `	pEntry = pSrc->pFirst;` |
|       7 | 5250 | `	n = pSrc->nEntry;` |
|       7 | 5251 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 5252 | `	for(;;){` |
|      19 | 5253 | `		if( n < 1 ){` |
|       5 | 5254 | `			break;` |
|       - | 5255 | `		}` |
|       - | 5256 | `		/* Extract the node value */` |
|      15 | 5257 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5258 | `		if( pVal ){` |
|      23 | 5259 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 5260 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5261 | `					/* ignore */` |
|     ! 0 | 5262 | `					continue;` |
|       - | 5263 | `				}` |
|       - | 5264 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 5265 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5266 | `				/* Perform the lookup */` |
|      15 | 5267 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 5268 | `				if( rc != SXRET_OK ){` |
|       - | 5269 | `					/* Value does not exist */` |
|       7 | 5270 | `					break;` |
|       - | 5271 | `				}` |
|       5 | 5272 | `			}` |
|      15 | 5273 | `			if( i >= (nArg-1) ){` |
|       - | 5274 | `				/* Perform the insertion */` |
|       9 | 5275 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5276 | `			}` |
|       7 | 5277 | `		}` |
|      15 | 5278 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 5279 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5280 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 5281 | `			return PH7_EXCEPTION;` |
|       - | 5282 | `		}` |
|       - | 5283 | `		/* Point to the next entry */` |
|      13 | 5284 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5285 | `		n--;` |
|       1 | 5286 | `	}` |
|       - | 5287 | `	/* Return the freshly created array */` |
|       5 | 5288 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5289 | `	return PH7_OK;` |
|      15 | 5290 |  |
|       - | 5291 | `/*` |
|       - | 5292 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5293 | ` *  Fill an array with values.` |
|       - | 5294 | ` * Parameters` |
|       - | 5295 | ` *  $start_index` |
|       - | 5296 | ` *    The first index of the returned array.` |
|       - | 5297 | ` *  $num` |
|       - | 5298 | ` *   Number of elements to insert.` |
|       - | 5299 | ` *  $value` |
|       - | 5300 | ` *    Value to use for filling.` |
|       - | 5301 | ` * Return` |
|       - | 5302 | ` *  The filled array or null on failure.` |
|       - | 5303 | ` */` |
|     238 | 5304 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5305 |  |
|       - | 5306 | `	ph7_value *pArray;` |
|       - | 5307 | `	int i,nEntry;` |
|       - | 5308 |  |
|       - | 5309 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 5310 | `	if( nArg != 3 ){` |
|       - | 5311 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 5312 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5313 | `			"ArgumentCountError",` |
|       - | 5314 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5315 | `			nArg` |
|       - | 5316 | `			);` |
|       - | 5317 | `	}` |
|       - | 5318 |  |
|       - | 5319 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5320 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5321 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5322 | `	 * and NULLs are rejected outright. */` |
|     466 | 5323 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 5324 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5325 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5326 | `			"TypeError",` |
|       - | 5327 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5328 | `			ph7_type_name(apArg[0])` |
|       - | 5329 | `			);` |
|       - | 5330 | `	}` |
|     234 | 5331 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5332 | `		int len;` |
|       8 | 5333 | `		sxu8 bReal = FALSE;` |
|       8 | 5334 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5335 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5336 | `			/* Non‑numeric string is an error. */` |
|       3 | 5337 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5338 | `				"TypeError",` |
|       - | 5339 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5340 | `				);` |
|       - | 5341 | `		}` |
|       5 | 5342 | `		if( bReal ){` |
|       - | 5343 | `			/* float-string -> deprecation warning */` |
|       4 | 5344 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5345 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5346 | `				zStr` |
|       - | 5347 | `				);` |
|       1 | 5348 | `		}` |
|       2 | 5349 | `	}` |
|       - | 5350 |  |
|       - | 5351 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5352 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 5353 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 5354 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5355 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5356 | `			"TypeError",` |
|       - | 5357 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5358 | `			ph7_type_name(apArg[1])` |
|       - | 5359 | `			);` |
|       - | 5360 | `	}` |
|     232 | 5361 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5362 | `		int len;` |
|       3 | 5363 | `		sxu8 bReal = FALSE;` |
|       3 | 5364 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5365 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5366 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5367 | `				"TypeError",` |
|       - | 5368 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5369 | `				);` |
|       - | 5370 | `		}` |
|     ! 0 | 5371 | `	}` |
|       - | 5372 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5373 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5374 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5375 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5376 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5377 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5378 | `		if( d != (double)i64 ){` |
|       7 | 5379 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5380 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5381 | `				d` |
|       - | 5382 | `				);` |
|       2 | 5383 | `		}` |
|       2 | 5384 | `	}` |
|       - | 5385 |  |
|       - | 5386 | `	/* Total number of entries to insert */` |
|     230 | 5387 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5388 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5389 | `	if( nEntry < 0 ){` |
|       3 | 5390 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5391 | `			"ValueError",` |
|       - | 5392 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5393 | `			);` |
|       - | 5394 | `	}` |
|       - | 5395 |  |
|       - | 5396 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5397 | `	if( nEntry == 0 ){` |
|       7 | 5398 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5399 | `		return PH7_OK;` |
|       - | 5400 | `	}` |
|       - | 5401 |  |
|       - | 5402 | `	/* Create a new array */` |
|     221 | 5403 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5404 | `	if( pArray == 0 ){` |
|     ! 0 | 5405 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5406 | `		return PH7_OK;` |
|       - | 5407 | `	}` |
|       - | 5408 |  |
|       - | 5409 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5410 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 5411 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5412 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5413 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 5414 | `	}` |
|       - | 5415 | `	/* Return the filled array */` |
|     221 | 5416 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5417 | `	return PH7_OK;` |
|     121 | 5418 |  |
|       - | 5419 | `/*` |
|       - | 5420 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5421 | ` *  Fill an array with values, specifying keys.` |
|       - | 5422 | ` * Parameters` |
|       - | 5423 | ` *  $input` |
|       - | 5424 | ` *   Array of values that will be used as key.` |
|       - | 5425 | ` *  $value` |
|       - | 5426 | ` *    Value to use for filling.` |
|       - | 5427 | ` * Return` |
|       - | 5428 | ` *  The filled array.` |
|       - | 5429 | ` * Throws` |
|       - | 5430 | ` *  ValueError if $input is not an array.` |
|       - | 5431 | ` */` |
|      26 | 5432 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5433 |  |
|       - | 5434 | `	ph7_hashmap_node *pEntry;` |
|       - | 5435 | `	ph7_hashmap *pSrc;` |
|       - | 5436 | `	ph7_value *pArray;` |
|       - | 5437 | `	sxu32 n;` |
|       - | 5438 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5439 | `	if( nArg != 2 ){` |
|      10 | 5440 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5441 | `			"ArgumentCountError",` |
|       - | 5442 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5443 | `			nArg` |
|       - | 5444 | `			);` |
|       - | 5445 | `	}` |
|       - | 5446 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5447 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5448 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5449 | `			"TypeError",` |
|       - | 5450 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5451 | `			ph7_type_name(apArg[0])` |
|       - | 5452 | `			);` |
|       - | 5453 | `	}` |
|       - | 5454 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5455 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5456 | `	/* Create a new array */` |
|      17 | 5457 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5458 | `	if( pArray == 0 ){` |
|     ! 0 | 5459 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5460 | `		return PH7_OK;` |
|       - | 5461 | `	}` |
|       - | 5462 | `	/* Perform the requested operation */` |
|      17 | 5463 | `	pEntry = pSrc->pFirst;` |
|      45 | 5464 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5465 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5466 | `		/* Point to the next entry */` |
|      29 | 5467 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5468 | `	}` |
|       - | 5469 | `	/* Return the filled array */` |
|      17 | 5470 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5471 | `	return PH7_OK;` |
|      15 | 5472 |  |
|       - | 5473 | `/*` |
|       - | 5474 | ` * array array_combine(array $keys,array $values)` |
|       - | 5475 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5476 | ` * Parameters` |
|       - | 5477 | ` *  $keys` |
|       - | 5478 | ` *    Array of keys to be used.` |
|       - | 5479 | ` * $values` |
|       - | 5480 | ` *   Array of values to be used.` |
|       - | 5481 | ` * Return` |
|       - | 5482 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5483 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5484 | ` *  not an array.` |
|       - | 5485 | ` */` |
|      18 | 5486 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5487 |  |
|       - | 5488 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5489 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5490 | `	ph7_value *pArray;` |
|       - | 5491 | `	sxu32 n;` |
|       - | 5492 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5493 | `	if( nArg != 2 ){` |
|       - | 5494 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5495 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5496 | `			"ArgumentCountError",` |
|       - | 5497 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5498 | `			nArg` |
|       - | 5499 | `			);` |
|       - | 5500 | `	}` |
|       - | 5501 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5502 | `	 * argument index in the error message. */` |
|      18 | 5503 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5504 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5505 | `			"TypeError",` |
|       - | 5506 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5507 | `			ph7_type_name(apArg[0])` |
|       - | 5508 | `			);` |
|       - | 5509 | `	}` |
|      16 | 5510 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5511 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5512 | `			"TypeError",` |
|       - | 5513 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5514 | `			ph7_type_name(apArg[1])` |
|       - | 5515 | `			);` |
|       - | 5516 | `	}` |
|       - | 5517 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5518 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5519 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5520 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5521 | `		/* Length mismatch -> ValueError */` |
|       3 | 5522 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5523 | `			"ValueError",` |
|       - | 5524 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5525 | `			);` |
|       - | 5526 | `	}` |
|       - | 5527 | `	/* Create a new array */` |
|      11 | 5528 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5529 | `	if( pArray == 0 ){` |
|     ! 0 | 5530 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5531 | `		return PH7_OK;` |
|       - | 5532 | `	}` |
|       - | 5533 | `	/* Perform the requested operation */` |
|      11 | 5534 | `	pKe = pKey->pFirst;` |
|      11 | 5535 | `	pVe = pValue->pFirst;` |
|      33 | 5536 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5537 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5538 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5539 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5540 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5541 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5542 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5543 | `		 * original array must not be mutated. */` |
|      23 | 5544 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5545 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5546 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5547 | `			if( pTmpKey ){` |
|       5 | 5548 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5549 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5550 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5551 | `				pKeyCopy = pTmpKey;` |
|       2 | 5552 | `			}` |
|       2 | 5553 | `		}` |
|      23 | 5554 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5555 | `		/* Point to the next entry */` |
|      23 | 5556 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5557 | `		pVe = pVe->pPrev;` |
|      12 | 5558 | `	}` |
|       - | 5559 | `	/* Return the filled array */` |
|      11 | 5560 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5561 | `	return PH7_OK;` |
|      11 | 5562 |  |
|       - | 5563 | `/*` |
|       - | 5564 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5565 | ` *  Return an array with elements in reverse order.` |
|       - | 5566 | ` * Parameters` |
|       - | 5567 | ` *  $array` |
|       - | 5568 | ` *   The input array.` |
|       - | 5569 | ` *  $preserve_keys (optional)` |
|       - | 5570 | ` *   If set to TRUE keys are preserved.` |
|       - | 5571 | ` * Return` |
|       - | 5572 | ` *  The reversed array.` |
|       - | 5573 | ` */` |
|      20 | 5574 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5575 |  |
|       - | 5576 | `	ph7_hashmap_node *pEntry;` |
|       - | 5577 | `	ph7_hashmap *pSrc;` |
|       - | 5578 | `	ph7_value *pArray;` |
|       - | 5579 | `	int bPreserve;` |
|       - | 5580 | `	sxu32 n;` |
|      22 | 5581 | `	if( nArg < 1 ){` |
|       4 | 5582 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5583 | `			"ArgumentCountError",` |
|       - | 5584 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5585 | `			nArg` |
|       - | 5586 | `			);` |
|       - | 5587 | `	}` |
|       - | 5588 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5589 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5590 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5591 | `			"TypeError",` |
|       - | 5592 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5593 | `			ph7_type_name(apArg[0])` |
|       - | 5594 | `			);` |
|       - | 5595 | `	}` |
|      17 | 5596 | `	bPreserve = FALSE;` |
|      17 | 5597 | `	if( nArg > 1 ){` |
|       7 | 5598 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5599 | `	}` |
|       - | 5600 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5601 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5602 | `	/* Create a new array */` |
|      17 | 5603 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5604 | `	if( pArray == 0 ){` |
|     ! 0 | 5605 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5606 | `		return PH7_OK;` |
|       - | 5607 | `	}` |
|       - | 5608 | `	/* Perform the requested operation */` |
|      17 | 5609 | `	pEntry = pSrc->pLast;` |
|      55 | 5610 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5611 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5612 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5613 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5614 | `		/* Point to the previous entry */` |
|      39 | 5615 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5616 | `	}` |
|      17 | 5617 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5618 | `	return PH7_OK;` |
|      12 | 5619 |  |
|       - | 5620 | `/*` |
|       - | 5621 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5622 | ` *  Removes duplicate values from an array.` |
|       - | 5623 | ` * Parameters` |
|       - | 5624 | ` *  $array` |
|       - | 5625 | ` *   The input array.` |
|       - | 5626 | ` *  $flags` |
|       - | 5627 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5628 | ` *   behavior using these values:` |
|       - | 5629 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5630 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5631 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5632 | ` * Return` |
|       - | 5633 | ` *  The filtered array.` |
|       - | 5634 | ` */` |
|      24 | 5635 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5636 |  |
|       - | 5637 | `	ph7_hashmap_node *pEntry;` |
|       - | 5638 | `	ph7_value *pNeedle;` |
|       - | 5639 | `	ph7_hashmap *pSrc;` |
|       - | 5640 | `	ph7_value *pArray;` |
|       - | 5641 | `	int bStrict;` |
|       - | 5642 | `	sxi32 rc;` |
|       - | 5643 | `	sxu32 n;` |
|      26 | 5644 | `	if( nArg < 1 ){` |
|       - | 5645 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5646 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5647 | `			"ArgumentCountError",` |
|       - | 5648 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5649 | `			);` |
|       - | 5650 | `	}` |
|      24 | 5651 | `	if( nArg > 2 ){` |
|       - | 5652 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5653 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5654 | `			"ArgumentCountError",` |
|       - | 5655 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5656 | `			nArg` |
|       - | 5657 | `			);` |
|       - | 5658 | `	}` |
|       - | 5659 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5660 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5661 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5662 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5663 | `			"TypeError",` |
|       - | 5664 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5665 | `			ph7_type_name(apArg[0])` |
|       - | 5666 | `			);` |
|       - | 5667 | `	}` |
|      19 | 5668 | `	bStrict = FALSE;` |
|       - | 5669 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5670 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5671 | `	/* Create a new array */` |
|      19 | 5672 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5673 | `	if( pArray == 0 ){` |
|     ! 0 | 5674 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5675 | `		return PH7_OK;` |
|       - | 5676 | `	}` |
|       - | 5677 | `	/* Perform the requested operation */` |
|      19 | 5678 | `	pEntry = pSrc->pFirst;` |
|      83 | 5679 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5680 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5681 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5682 | `		if( pNeedle ){` |
|      65 | 5683 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5684 | `		}` |
|      65 | 5685 | `		if( rc != SXRET_OK ){` |
|       - | 5686 | `			/* Perform the insertion */` |
|      37 | 5687 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5688 | `		}` |
|       - | 5689 | `		/* Point to the next entry */` |
|      65 | 5690 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5691 | `	}` |
|       - | 5692 | `	/* Return the freshly created array */` |
|      19 | 5693 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5694 | `	return PH7_OK;` |
|      14 | 5695 |  |
|       - | 5696 | `/*` |
|       - | 5697 | ` * array array_flip(array $input)` |
|       - | 5698 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5699 | ` * Parameter` |
|       - | 5700 | ` *  $input` |
|       - | 5701 | ` *   Input array.` |
|       - | 5702 | ` * Return` |
|       - | 5703 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5704 | ` */` |
|      34 | 5705 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5706 |  |
|       - | 5707 | `	ph7_hashmap_node *pEntry;` |
|       - | 5708 | `	ph7_hashmap *pSrc;` |
|       - | 5709 | `	ph7_value *pArray;` |
|       - | 5710 | `	ph7_value *pKey;` |
|       - | 5711 | `	ph7_value sVal;` |
|       - | 5712 | `	sxu32 n;` |
|       - | 5713 |  |
|       - | 5714 | `	/* PHP requires exactly one argument */` |
|      36 | 5715 | `	if( nArg != 1 ){` |
|       - | 5716 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5717 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5718 | `			"ArgumentCountError",` |
|       - | 5719 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5720 | `			nArg` |
|       - | 5721 | `			);` |
|       - | 5722 | `	}` |
|       - | 5723 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5724 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5725 | `		/* Type mismatch -> TypeError */` |
|       7 | 5726 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5727 | `			"TypeError",` |
|       - | 5728 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5729 | `			ph7_type_name(apArg[0])` |
|       - | 5730 | `			);` |
|       - | 5731 | `	}` |
|       - | 5732 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5733 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5734 | `	/* Create a new array */` |
|      27 | 5735 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5736 | `	if( pArray == 0 ){` |
|     ! 0 | 5737 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5738 | `		return PH7_OK;` |
|       - | 5739 | `	}` |
|       - | 5740 | `	/* Start processing */` |
|      27 | 5741 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5742 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5743 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5744 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5745 | `		if( pKey ){` |
|       - | 5746 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5747 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5748 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5749 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5750 | `					);` |
|   22236 | 5751 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5752 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5753 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5754 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5755 | `				}else{` |
|       - | 5756 | `					SyString sStr;` |
|    2227 | 5757 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5758 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5759 | `				}` |
|       - | 5760 | `				/* Perform the insertion */` |
|   22227 | 5761 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5762 | `				/* Safely release the value because each inserted entry` |
|       - | 5763 | `				 * has its own private copy of the value.` |
|       - | 5764 | `				 */` |
|   22227 | 5765 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5766 | `			}else{` |
|       - | 5767 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5768 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5769 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5770 | `					);` |
|       - | 5771 | `			}` |
|   11118 | 5772 | `		}` |
|       - | 5773 | `		/* Point to the next entry */` |
|   22237 | 5774 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5775 | `	}` |
|       - | 5776 | `	/* Return the freshly created array */` |
|      27 | 5777 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5778 | `	return PH7_OK;` |
|      19 | 5779 |  |
|       - | 5780 | `/*` |
|       - | 5781 | ` * number array_sum(array $array )` |
|       - | 5782 | ` *  Calculate the sum of values in an array.` |
|       - | 5783 | ` * Parameters` |
|       - | 5784 | ` *  $array: The input array.` |
|       - | 5785 | ` * Return` |
|       - | 5786 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5787 | ` */` |
|      24 | 5788 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5789 |  |
|       - | 5790 | `	ph7_hashmap_node *pEntry;` |
|       - | 5791 | `	ph7_value *pObj;` |
|      25 | 5792 | `	double dSum = 0;` |
|       - | 5793 | `	sxu32 n;` |
|      25 | 5794 | `	pEntry = pMap->pFirst;` |
|      91 | 5795 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5796 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5797 | `		if( pObj ){` |
|      67 | 5798 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5799 | `				dSum += pObj->rVal;` |
|      53 | 5800 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5801 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5802 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5803 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5804 | `					double dv = 0;` |
|      13 | 5805 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5806 | `					dSum += dv;` |
|       7 | 5807 | `				}` |
|      12 | 5808 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5809 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5810 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5811 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5812 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5813 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5814 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5815 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5816 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5817 | `			}` |
|       - | 5818 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5819 | `		}` |
|       - | 5820 | `		/* Point to the next entry */` |
|      67 | 5821 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5822 | `	}` |
|       - | 5823 | `	/* Return sum */` |
|      25 | 5824 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5825 |  |
|      26 | 5826 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5827 |  |
|       - | 5828 | `	ph7_hashmap_node *pEntry;` |
|       - | 5829 | `	ph7_value *pObj;` |
|      28 | 5830 | `	sxi64 nSum = 0;` |
|       - | 5831 | `	sxu32 n;` |
|      28 | 5832 | `	pEntry = pMap->pFirst;` |
|     112 | 5833 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      86 | 5834 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      86 | 5835 | `		if( pObj ){` |
|      86 | 5836 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      76 | 5837 | `				nSum += pObj->x.iVal;` |
|      48 | 5838 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5839 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5840 | `					sxi64 nv = 0;` |
|       5 | 5841 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5842 | `					nSum += nv;` |
|       3 | 5843 | `				}` |
|       8 | 5844 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5845 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5846 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5847 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5848 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5849 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5850 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5851 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5852 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5853 | `			}` |
|       - | 5854 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      42 | 5855 | `		}` |
|       - | 5856 | `		/* Point to the next entry */` |
|      86 | 5857 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      44 | 5858 | `	}` |
|       - | 5859 | `	/* Return sum */` |
|      28 | 5860 | `	ph7_result_int64(pCtx,nSum);` |
|      28 | 5861 |  |
|       - | 5862 | `/* number array_sum(array $array )` |
|       - | 5863 | ` * (See block-coment above)` |
|       - | 5864 | ` */` |
|      64 | 5865 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5866 |  |
|       - | 5867 | `	ph7_hashmap_node *pEntry;` |
|       - | 5868 | `	ph7_hashmap *pMap;` |
|       - | 5869 | `	ph7_value *pObj;` |
|      66 | 5870 | `	int useDouble = 0;` |
|       - | 5871 | `	sxu32 n;` |
|       - | 5872 | `	/* PHP requires exactly one argument */` |
|      66 | 5873 | `	if( nArg != 1 ){` |
|       7 | 5874 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5875 | `			"ArgumentCountError",` |
|       - | 5876 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5877 | `			nArg` |
|       - | 5878 | `			);` |
|       - | 5879 | `	}` |
|       - | 5880 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 5881 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5882 | `		/* Type mismatch -> TypeError */` |
|       7 | 5883 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5884 | `			"TypeError",` |
|       - | 5885 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5886 | `			ph7_type_name(apArg[0])` |
|       - | 5887 | `			);` |
|       - | 5888 | `	}` |
|      58 | 5889 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      58 | 5890 | `	if( pMap->nEntry < 1 ){` |
|       - | 5891 | `		/* Nothing to compute,return 0 */` |
|       7 | 5892 | `		ph7_result_int(pCtx,0);` |
|       7 | 5893 | `		return PH7_OK;` |
|       - | 5894 | `	}` |
|       - | 5895 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5896 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5897 | `	 */` |
|      52 | 5898 | `	pEntry = pMap->pFirst;` |
|     144 | 5899 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     118 | 5900 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     118 | 5901 | `		if( pObj ){` |
|     118 | 5902 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5903 | `				useDouble = 1;` |
|      19 | 5904 | `				break;` |
|       - | 5905 | `			}` |
|     100 | 5906 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5907 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5908 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5909 | `				sxu32 i;` |
|      23 | 5910 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5911 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5912 | `						useDouble = 1;` |
|       7 | 5913 | `						break;` |
|       - | 5914 | `					}` |
|       6 | 5915 | `				}` |
|      13 | 5916 | `				if( useDouble ){` |
|       7 | 5917 | `					break;` |
|       - | 5918 | `				}` |
|       3 | 5919 | `			}` |
|      46 | 5920 | `		}` |
|      94 | 5921 | `		pEntry = pEntry->pPrev;` |
|      48 | 5922 | `	}` |
|      52 | 5923 | `	if( useDouble ){` |
|      25 | 5924 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5925 | `	}else{` |
|      28 | 5926 | `		Int64Sum(pCtx,pMap);` |
|       - | 5927 | `	}` |
|      52 | 5928 | `	return PH7_OK;` |
|      34 | 5929 |  |
|       - | 5930 | `/*` |
|       - | 5931 | ` * number array_product(array $array )` |
|       - | 5932 | ` *  Calculate the product of values in an array.` |
|       - | 5933 | ` * Parameters` |
|       - | 5934 | ` *  $array: The input array.` |
|       - | 5935 | ` * Return` |
|       - | 5936 | ` *  Returns the product of values as an integer or float.` |
|       - | 5937 | ` */` |
|     ! 0 | 5938 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5939 |  |
|       - | 5940 | `	ph7_hashmap_node *pEntry;` |
|       - | 5941 | `	ph7_value *pObj;` |
|       - | 5942 | `	double dProd;` |
|       - | 5943 | `	sxu32 n;` |
|     ! 0 | 5944 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5945 | `	dProd = 1;` |
|     ! 0 | 5946 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5947 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5948 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5949 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5950 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5951 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5952 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5953 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5954 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5955 | `					double dv = 0;` |
|     ! 0 | 5956 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5957 | `					dProd *= dv;` |
|     ! 0 | 5958 | `				}` |
|     ! 0 | 5959 | `			}` |
|     ! 0 | 5960 | `		}` |
|       - | 5961 | `		/* Point to the next entry */` |
|     ! 0 | 5962 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5963 | `	}` |
|       - | 5964 | `	/* Return product */` |
|     ! 0 | 5965 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5966 |  |
|     ! 0 | 5967 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5968 |  |
|       - | 5969 | `	ph7_hashmap_node *pEntry;` |
|       - | 5970 | `	ph7_value *pObj;` |
|       - | 5971 | `	sxi64 nProd;` |
|       - | 5972 | `	sxu32 n;` |
|     ! 0 | 5973 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5974 | `	nProd = 1;` |
|     ! 0 | 5975 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5976 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5977 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5978 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5979 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5980 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5981 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5982 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5983 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5984 | `					sxi64 nv = 0;` |
|     ! 0 | 5985 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5986 | `					nProd *= nv;` |
|     ! 0 | 5987 | `				}` |
|     ! 0 | 5988 | `			}` |
|     ! 0 | 5989 | `		}` |
|       - | 5990 | `		/* Point to the next entry */` |
|     ! 0 | 5991 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5992 | `	}` |
|       - | 5993 | `	/* Return product */` |
|     ! 0 | 5994 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5995 |  |
|       - | 5996 | `/* number array_product(array $array )` |
|       - | 5997 | ` * (See block-block comment above)` |
|       - | 5998 | ` */` |
|     ! 0 | 5999 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 6000 |  |
|       - | 6001 | `	ph7_hashmap *pMap;` |
|       - | 6002 | `	ph7_value *pObj;` |
|     ! 0 | 6003 | `	if( nArg < 1 ){` |
|       - | 6004 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 6005 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6006 | `		return PH7_OK;` |
|       - | 6007 | `	}` |
|       - | 6008 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 6009 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6010 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 6011 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6012 | `		return PH7_OK;` |
|       - | 6013 | `	}` |
|     ! 0 | 6014 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 6015 | `	if( pMap->nEntry < 1 ){` |
|       - | 6016 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 6017 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6018 | `		return PH7_OK;` |
|       - | 6019 | `	}` |
|       - | 6020 | `	/* If the first element is of type float,then perform floating` |
|       - | 6021 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 6022 | `	 */` |
|     ! 0 | 6023 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 6024 | `	if( pObj == 0 ){` |
|     ! 0 | 6025 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6026 | `		return PH7_OK;` |
|       - | 6027 | `	}` |
|     ! 0 | 6028 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6029 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 6030 | `	}else{` |
|     ! 0 | 6031 | `		Int64Prod(pCtx,pMap);` |
|       - | 6032 | `	}` |
|     ! 0 | 6033 | `	return PH7_OK;` |
|     ! 0 | 6034 |  |
|       - | 6035 | `/*` |
|       - | 6036 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 6037 | ` *  Pick one or more random entries out of an array.` |
|       - | 6038 | ` * Parameters` |
|       - | 6039 | ` * $input` |
|       - | 6040 | ` *  The input array.` |
|       - | 6041 | ` * $num_req` |
|       - | 6042 | ` *  Specifies how many entries you want to pick.` |
|       - | 6043 | ` * Return` |
|       - | 6044 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 6045 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 6046 | ` *  NULL is returned on failure.` |
|       - | 6047 | ` */` |
|       6 | 6048 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6049 |  |
|       - | 6050 | `	ph7_hashmap_node *pNode;` |
|       - | 6051 | `	ph7_hashmap *pMap;` |
|       7 | 6052 | `	int nItem = 1;` |
|       7 | 6053 | `	if( nArg < 1 ){` |
|       - | 6054 | `		/* Missing argument,return NULL */` |
|     ! 0 | 6055 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6056 | `		return PH7_OK;` |
|       - | 6057 | `	}` |
|       - | 6058 | `	/* Make sure we are dealing with an array */` |
|       7 | 6059 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6060 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6061 | `		return PH7_OK;` |
|       - | 6062 | `	}` |
|       - | 6063 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 6064 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 6065 | `	if(pMap->nEntry < 1 ){` |
|       - | 6066 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 6067 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6068 | `		return PH7_OK;` |
|       - | 6069 | `	}` |
|       7 | 6070 | `	if( nArg > 1 ){` |
|       3 | 6071 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 6072 | `	}` |
|       7 | 6073 | `	if( nItem < 2 ){` |
|       - | 6074 | `		sxu32 nEntry;` |
|       - | 6075 | `		/* Select a random number */` |
|       5 | 6076 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 6077 | `		/* Extract the desired entry.` |
|       - | 6078 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 6079 | `		 */` |
|       5 | 6080 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 6081 | `			pNode = pMap->pLast;` |
|       3 | 6082 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 6083 | `			if( nEntry > 1 ){` |
|     ! 0 | 6084 | `				for(;;){` |
|     ! 0 | 6085 | `					if( nEntry == 0 ){` |
|     ! 0 | 6086 | `						break;` |
|       - | 6087 | `					}` |
|       - | 6088 | `					/* Point to the previous entry */` |
|     ! 0 | 6089 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 6090 | `					nEntry--;` |
|     ! 0 | 6091 | `				}` |
|     ! 0 | 6092 | `			}` |
|       2 | 6093 | `		}else{` |
|       2 | 6094 | `			pNode = pMap->pFirst;` |
|       1 | 6095 | `			for(;;){` |
|       2 | 6096 | `				if( nEntry == 0 ){` |
|       2 | 6097 | `					break;` |
|       - | 6098 | `				}` |
|       - | 6099 | `				/* Point to the next entry */` |
|     ! 0 | 6100 | `				pNode = pNode->pPrev; /* Reverse link */` |
|     ! 0 | 6101 | `				nEntry--;` |
|     ! 0 | 6102 | `			}` |
|       - | 6103 | `		}` |
|       5 | 6104 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6105 | `			/* Int key */` |
|       3 | 6106 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6107 | `		}else{` |
|       - | 6108 | `			/* Blob key */` |
|       3 | 6109 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6110 | `		}` |
|       3 | 6111 | `	}else{` |
|       - | 6112 | `		ph7_value sKey,*pArray;` |
|       - | 6113 | `		ph7_hashmap *pDest;` |
|       - | 6114 | `		/* Create a new array */` |
|       3 | 6115 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6116 | `		if( pArray == 0 ){` |
|     ! 0 | 6117 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6118 | `			return PH7_OK;` |
|       - | 6119 | `		}` |
|       - | 6120 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6121 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6122 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6123 | `		/* Copy the first n items */` |
|       3 | 6124 | `		pNode = pMap->pFirst;` |
|       3 | 6125 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6126 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6127 | `		}` |
|       7 | 6128 | `		while( nItem > 0){` |
|       5 | 6129 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6130 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6131 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6132 | `			/* Point to the next entry */` |
|       5 | 6133 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6134 | `			nItem--;` |
|       1 | 6135 | `		}` |
|       - | 6136 | `		/* Shuffle the array */` |
|       3 | 6137 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6138 | `		/* Rehash node */` |
|       3 | 6139 | `		HashmapSortRehash(pDest);` |
|       - | 6140 | `		/* Return the random array */` |
|       3 | 6141 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6142 | `	}` |
|       7 | 6143 | `	return PH7_OK;` |
|       4 | 6144 |  |
|       - | 6145 | `/*` |
|       - | 6146 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6147 | ` *  Split an array into chunks.` |
|       - | 6148 | ` * Parameters` |
|       - | 6149 | ` * $input` |
|       - | 6150 | ` *   The array to work on` |
|       - | 6151 | ` * $size` |
|       - | 6152 | ` *   The size of each chunk` |
|       - | 6153 | ` * $preserve_keys` |
|       - | 6154 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6155 | ` *   the chunk numerically.` |
|       - | 6156 | ` * Return` |
|       - | 6157 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6158 | ` *  zero, with each dimension containing size elements.` |
|       - | 6159 | ` */` |
|      42 | 6160 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6161 |  |
|       - | 6162 | `	ph7_value *pArray,*pChunk;` |
|       - | 6163 | `	ph7_hashmap_node *pEntry;` |
|       - | 6164 | `	ph7_hashmap *pMap;` |
|       - | 6165 | `	int bPreserve;` |
|       - | 6166 | `	sxu32 nChunk;` |
|       - | 6167 | `	sxu32 nSize;` |
|       - | 6168 | `	sxu32 n;` |
|       - | 6169 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 6170 | `	if( nArg < 2 ){` |
|       - | 6171 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6172 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6173 | `			"ArgumentCountError",` |
|       - | 6174 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6175 | `			nArg` |
|       - | 6176 | `			);` |
|       - | 6177 | `	}` |
|      42 | 6178 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6179 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6180 | `			"TypeError",` |
|       - | 6181 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6182 | `			ph7_type_name(apArg[0])` |
|       - | 6183 | `			);` |
|       - | 6184 | `	}` |
|       - | 6185 | `	/* Create a new array */` |
|      40 | 6186 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 6187 | `	if( pArray == 0 ){` |
|     ! 0 | 6188 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6189 | `		return PH7_OK;` |
|       - | 6190 | `	}` |
|       - | 6191 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 6192 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6193 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6194 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 6195 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 6196 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6197 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6198 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6199 | `			"TypeError",` |
|       - | 6200 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6201 | `			ph7_type_name(apArg[1])` |
|       - | 6202 | `			);` |
|       - | 6203 | `	}` |
|       - | 6204 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6205 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6206 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 6207 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6208 | `		int len;` |
|       3 | 6209 | `		sxu8 bReal = FALSE;` |
|       3 | 6210 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6211 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6212 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6213 | `				"TypeError",` |
|       - | 6214 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6215 | `				);` |
|       - | 6216 | `		}` |
|     ! 0 | 6217 | `		if( bReal ){` |
|       - | 6218 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6219 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6220 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6221 | `				zStr` |
|       - | 6222 | `				);` |
|     ! 0 | 6223 | `		}` |
|     ! 0 | 6224 | `	}` |
|       - | 6225 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6226 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6227 | `	 * later via ph7_value_to_int. */` |
|      38 | 6228 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6229 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6230 | `		sxi64 i = (sxi64)d;` |
|       3 | 6231 | `		if( d != (double)i ){` |
|       4 | 6232 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6233 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6234 | `				d` |
|       - | 6235 | `				);` |
|       1 | 6236 | `		}` |
|       1 | 6237 | `	}` |
|       - | 6238 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6239 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6240 | `	{` |
|      38 | 6241 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 6242 | `		if( nSizeSigned < 1 ){` |
|       - | 6243 | `			/* size <= 0 -> ValueError */` |
|       5 | 6244 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6245 | `				"ValueError",` |
|       - | 6246 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6247 | `				);` |
|       - | 6248 | `		}` |
|      34 | 6249 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6250 | `	}` |
|      34 | 6251 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6252 | `		/* Return the whole array */` |
|       3 | 6253 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6254 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6255 | `		return PH7_OK;` |
|       - | 6256 | `	}` |
|      32 | 6257 | `	bPreserve = 0;` |
|      32 | 6258 | `	if( nArg > 2 ){` |
|       - | 6259 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6260 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6261 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6262 | `		 * normally, matching PHP behaviour. */` |
|      45 | 6263 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 6264 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6265 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 6266 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6267 | `				"TypeError",` |
|       - | 6268 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6269 | `				ph7_type_name(apArg[2])` |
|       - | 6270 | `				);` |
|       - | 6271 | `		}` |
|      21 | 6272 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6273 | `	}` |
|       - | 6274 | `	/* Start processing */` |
|      27 | 6275 | `	pEntry = pMap->pFirst;` |
|      27 | 6276 | `	nChunk = 0;` |
|      27 | 6277 | `	pChunk = 0;` |
|      27 | 6278 | `	n = pMap->nEntry;` |
|      56 | 6279 | `	for( ;; ){` |
|     113 | 6280 | `		if( n < 1 ){` |
|       - | 6281 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6282 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6283 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6284 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6285 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6286 | `			 * exists. */` |
|      27 | 6287 | `			if( pChunk ){` |
|      27 | 6288 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6289 | `			}` |
|      27 | 6290 | `			break;` |
|       - | 6291 | `		}` |
|      87 | 6292 | `		if( nChunk < 1 ){` |
|      71 | 6293 | `			if( pChunk ){` |
|       - | 6294 | `				/* Put the first chunk */` |
|      45 | 6295 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6296 | `			}` |
|       - | 6297 | `			/* Create a new dimension */` |
|      71 | 6298 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6299 | `												   * will be automatically released as soon we return` |
|       - | 6300 | `												   * from this function */` |
|      71 | 6301 | `			if( pChunk == 0 ){` |
|     ! 0 | 6302 | `				break;` |
|       - | 6303 | `			}` |
|      71 | 6304 | `			nChunk = nSize;` |
|      35 | 6305 | `		}` |
|       - | 6306 | `		/* Insert the entry */` |
|      87 | 6307 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6308 | `		/* Point to the next entry */` |
|      87 | 6309 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6310 | `		nChunk--;` |
|      87 | 6311 | `		n--;` |
|       1 | 6312 | `	}` |
|       - | 6313 | `	/* Return the multidimensional array */` |
|      27 | 6314 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6315 | `	return PH7_OK;` |
|      23 | 6316 |  |
|       - | 6317 | `/*` |
|       - | 6318 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6319 | ` *  Pad array to the specified length with a value.` |
|       - | 6320 | ` * $input` |
|       - | 6321 | ` *   Initial array of values to pad.` |
|       - | 6322 | ` * $pad_size` |
|       - | 6323 | ` *   New size of the array.` |
|       - | 6324 | ` * $pad_value` |
|       - | 6325 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6326 | ` */` |
|      28 | 6327 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6328 |  |
|       - | 6329 | `	ph7_hashmap *pMap;` |
|       - | 6330 | `	ph7_value *pArray;` |
|       - | 6331 | `	int nEntry;` |
|      30 | 6332 | `	if( nArg != 3 ){` |
|      10 | 6333 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6334 | `			"ArgumentCountError",` |
|       - | 6335 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6336 | `			nArg` |
|       - | 6337 | `			);` |
|       - | 6338 | `	}` |
|      24 | 6339 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6340 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6341 | `			"TypeError",` |
|       - | 6342 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6343 | `			ph7_type_name(apArg[0])` |
|       - | 6344 | `			);` |
|       - | 6345 | `	}` |
|       - | 6346 | `	/* Create a new array */` |
|      21 | 6347 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6348 | `	if( pArray == 0 ){` |
|     ! 0 | 6349 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6350 | `		return PH7_OK;` |
|       - | 6351 | `	}` |
|       - | 6352 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6353 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6354 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6355 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6356 | `	if( nEntry < 0 ){` |
|       9 | 6357 | `		nEntry = -nEntry;` |
|       9 | 6358 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6359 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6360 | `			/* Insert given items first */` |
|      17 | 6361 | `			while( nEntry > 0 ){` |
|      13 | 6362 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 6363 | `				nEntry--;` |
|       1 | 6364 | `			}` |
|       - | 6365 | `			/* Merge the two arrays */` |
|       5 | 6366 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6367 | `		}else{` |
|       5 | 6368 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6369 | `		}` |
|      17 | 6370 | `	}else if( nEntry > 0 ){` |
|      11 | 6371 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6372 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6373 | `			/* Merge the two arrays first */` |
|       7 | 6374 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6375 | `			/* Insert given items */` |
|      25 | 6376 | `			while( nEntry > 0 ){` |
|      19 | 6377 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 6378 | `				nEntry--;` |
|       1 | 6379 | `			}` |
|       4 | 6380 | `		}else{` |
|       5 | 6381 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6382 | `		}` |
|       6 | 6383 | `	}else{` |
|       - | 6384 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6385 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6386 | `	}` |
|       - | 6387 | `	/* Return the new array */` |
|      21 | 6388 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6389 | `	return PH7_OK;` |
|      16 | 6390 |  |
|       - | 6391 | `/*` |
|       - | 6392 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6393 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6394 | ` * Parameters` |
|       - | 6395 | ` * $array` |
|       - | 6396 | ` *   The array in which elements are replaced.` |
|       - | 6397 | ` * $array1` |
|       - | 6398 | ` *   The array from which elements will be extracted.` |
|       - | 6399 | ` * ....` |
|       - | 6400 | ` *  More arrays from which elements will be extracted.` |
|       - | 6401 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6402 | ` * Return` |
|       - | 6403 | ` *  Returns an array.` |
|       - | 6404 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6405 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6406 | ` */` |
|      22 | 6407 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6408 |  |
|       - | 6409 | `	ph7_hashmap *pMap;` |
|       - | 6410 | `	ph7_value *pArray;` |
|       - | 6411 | `	int i;` |
|      24 | 6412 | `	if( nArg < 1 ){` |
|       3 | 6413 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6414 | `			"ArgumentCountError",` |
|       - | 6415 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6416 | `			);` |
|       - | 6417 | `	}` |
|      22 | 6418 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6419 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6420 | `			"TypeError",` |
|       - | 6421 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6422 | `			ph7_type_name(apArg[0])` |
|       - | 6423 | `			);` |
|       - | 6424 | `	}` |
|       - | 6425 | `	/* Create a new array */` |
|      20 | 6426 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6427 | `	if( pArray == 0 ){` |
|     ! 0 | 6428 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6429 | `		return PH7_OK;` |
|       - | 6430 | `	}` |
|       - | 6431 | `	/* Overwrite from the first array */` |
|      20 | 6432 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6433 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6434 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6435 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6436 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6437 | `			/* Type mismatch -> TypeError */` |
|       4 | 6438 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6439 | `				"TypeError",` |
|       - | 6440 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6441 | `				i + 1,` |
|       2 | 6442 | `				ph7_type_name(apArg[i])` |
|       - | 6443 | `				);` |
|       - | 6444 | `		}` |
|       - | 6445 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6446 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6447 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6448 | `	}` |
|       - | 6449 | `	/* Return the new array */` |
|      17 | 6450 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6451 | `	return PH7_OK;` |
|      13 | 6452 |  |
|       - | 6453 | `/*` |
|       - | 6454 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6455 | ` *  Filters elements of an array using a callback function.` |
|       - | 6456 | ` * Parameters` |
|       - | 6457 | ` *  $input` |
|       - | 6458 | ` *    The array to iterate over` |
|       - | 6459 | ` * $callback` |
|       - | 6460 | ` *    The callback function to use` |
|       - | 6461 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6462 | ` *    will be removed.` |
|       - | 6463 | ` * Return` |
|       - | 6464 | ` *  The filtered array.` |
|       - | 6465 | ` */` |
|      20 | 6466 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6467 |  |
|       - | 6468 | `	ph7_hashmap_node *pEntry;` |
|       - | 6469 | `	ph7_hashmap *pMap;` |
|       - | 6470 | `	ph7_value *pArray;` |
|       - | 6471 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6472 | `	ph7_value *pValue;` |
|       - | 6473 | `	sxi32 rc;` |
|       - | 6474 | `	int keep;` |
|       - | 6475 | `	sxu32 n;` |
|      22 | 6476 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6477 | `		/* Invalid arguments,return NULL */` |
|       5 | 6478 | `		ph7_result_null(pCtx);` |
|       5 | 6479 | `		return PH7_OK;` |
|       - | 6480 | `	}` |
|       - | 6481 | `	/* Create a new array */` |
|      18 | 6482 | `	pArray = ph7_context_new_array(pCtx);` |
|      18 | 6483 | `	if( pArray == 0 ){` |
|     ! 0 | 6484 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6485 | `		return PH7_OK;` |
|       - | 6486 | `	}` |
|       - | 6487 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 6488 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      18 | 6489 | `	pEntry = pMap->pFirst;` |
|      18 | 6490 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      18 | 6491 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6492 | `	/* Perform the requested operation */` |
|      68 | 6493 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6494 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      56 | 6495 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      56 | 6496 | `		if( pValue == 0 ){` |
|       - | 6497 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6498 | `			keep = FALSE;` |
|      56 | 6499 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6500 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6501 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6502 | `				* silently dropped the element.  Emit similar message. */` |
|      28 | 6503 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6504 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6505 | `					int len;` |
|       3 | 6506 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6507 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6508 | `						"TypeError",` |
|       - | 6509 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6510 | `						zName` |
|       - | 6511 | `						);` |
|     ! 0 | 6512 | `				}else{` |
|     ! 0 | 6513 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6514 | `						"TypeError",` |
|       - | 6515 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6516 | `						ph7_type_name(apArg[1])` |
|       - | 6517 | `						);` |
|       - | 6518 | `				}` |
|       - | 6519 | `			}` |
|      25 | 6520 | `			keep = FALSE;` |
|      25 | 6521 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      25 | 6522 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 6523 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6524 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 6525 | `				return PH7_EXCEPTION;` |
|       - | 6526 | `			}` |
|      23 | 6527 | `			if( rc == SXRET_OK ){` |
|       - | 6528 | `				/* Perform a boolean cast */` |
|      23 | 6529 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6530 | `			}` |
|      23 | 6531 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6532 | `		}else{` |
|       - | 6533 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6534 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6535 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6536 | `			 */` |
|      29 | 6537 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6538 | `		}` |
|      51 | 6539 | `		if( keep ){` |
|       - | 6540 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6541 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6542 | `		}` |
|       - | 6543 | `		/* Point to the next entry */` |
|      51 | 6544 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6545 | `	}` |
|      13 | 6546 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6547 | `	return PH7_OK;` |
|      12 | 6548 |  |
|       - | 6549 | `/*` |
|       - | 6550 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 6551 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 6552 | ` * Parameters` |
|       - | 6553 | ` *  $callback` |
|       - | 6554 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 6555 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 6556 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 6557 | ` *   are zipped together.` |
|       - | 6558 | ` *  $array` |
|       - | 6559 | ` *   The first array to run through the callback function.` |
|       - | 6560 | ` *  $arrays` |
|       - | 6561 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 6562 | ` * Return` |
|       - | 6563 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 6564 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 6565 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 6566 | ` *  padding shorter arrays with NULL.` |
|       - | 6567 | ` */` |
|      46 | 6568 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6569 |  |
|       - | 6570 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6571 | `	ph7_hashmap_node *pEntry;` |
|       - | 6572 | `	ph7_hashmap *pMap;` |
|       - | 6573 | `	ph7_vm *pVm;` |
|       - | 6574 | `	int bNullCallback;` |
|       - | 6575 | `	sxi32 rc;` |
|       - | 6576 | `	int i;` |
|       - | 6577 | `	sxu32 n;` |
|      48 | 6578 | `	if( nArg < 2 ){` |
|       7 | 6579 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6580 | `			"ArgumentCountError",` |
|       - | 6581 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6582 | `			nArg` |
|       - | 6583 | `			);` |
|       - | 6584 | `	}` |
|      44 | 6585 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      44 | 6586 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6587 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6588 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6589 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6590 | `				"TypeError",` |
|       - | 6591 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6592 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6593 | `				zFunc` |
|       - | 6594 | `				);` |
|       - | 6595 | `		}` |
|       3 | 6596 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6597 | `			"TypeError",` |
|       - | 6598 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6599 | `			"no array or string given"` |
|       - | 6600 | `			);` |
|       - | 6601 | `	}` |
|       - | 6602 | `	/* Every remaining argument must be an array */` |
|      88 | 6603 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      52 | 6604 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 6605 | `			if( i == 1 ){` |
|       4 | 6606 | `				return PH7_VmThrowException(pCtx,` |
|       - | 6607 | `					"TypeError",` |
|       - | 6608 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6609 | `					ph7_type_name(apArg[1])` |
|       - | 6610 | `					);` |
|       - | 6611 | `			}` |
|     ! 0 | 6612 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6613 | `				"TypeError",` |
|       - | 6614 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 6615 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 6616 | `				);` |
|       - | 6617 | `		}` |
|      26 | 6618 | `	}` |
|      38 | 6619 | `	pVm = pCtx->pVm;` |
|       - | 6620 | `	/* Create a new array */` |
|      38 | 6621 | `	pArray = ph7_context_new_array(pCtx);` |
|      38 | 6622 | `	if( pArray == 0 ){` |
|     ! 0 | 6623 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6624 | `		return PH7_OK;` |
|       - | 6625 | `	}` |
|      38 | 6626 | `	PH7_MemObjInit(pVm,&sResult);` |
|      38 | 6627 | `	PH7_MemObjInit(pVm,&sKey);` |
|      38 | 6628 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      38 | 6629 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      38 | 6630 | `	if( nArg == 2 ){` |
|       - | 6631 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      28 | 6632 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      28 | 6633 | `		pEntry = pMap->pFirst;` |
|      82 | 6634 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6635 | `			/* Extract the node value */` |
|      58 | 6636 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|      58 | 6637 | `			if( pValue ){` |
|       - | 6638 | `				/* Extract the node key */` |
|      58 | 6639 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      58 | 6640 | `				if( bNullCallback ){` |
|       - | 6641 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 6642 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6643 | `				}else{` |
|       - | 6644 | `					/* Invoke the supplied callback */` |
|      48 | 6645 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|      48 | 6646 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 6647 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 6648 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       3 | 6649 | `						PH7_MemObjRelease(&sKey);` |
|       3 | 6650 | `						PH7_MemObjRelease(&sResult);` |
|       3 | 6651 | `						return PH7_EXCEPTION;` |
|       - | 6652 | `					}` |
|       - | 6653 | `					/* Insert the callback return value */` |
|      46 | 6654 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6655 | `				}` |
|      56 | 6656 | `				PH7_MemObjRelease(&sKey);` |
|      56 | 6657 | `				PH7_MemObjRelease(&sResult);` |
|      27 | 6658 | `			}` |
|       - | 6659 | `			/* Point to the next entry */` |
|      56 | 6660 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 6661 | `		}` |
|      14 | 6662 | `	}else{` |
|       - | 6663 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 6664 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 6665 | `		int nArrays = nArg - 1;` |
|       - | 6666 | `		ph7_hashmap_node **apCur;` |
|       - | 6667 | `		ph7_value **apCallArg;` |
|       - | 6668 | `		ph7_value sNull;` |
|      11 | 6669 | `		sxu32 nMax = 0;` |
|      11 | 6670 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 6671 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 6672 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 6673 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 6674 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 6675 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6676 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6677 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 6678 | `			return PH7_OK;` |
|       - | 6679 | `		}` |
|      11 | 6680 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 6681 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 6682 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 6683 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 6684 | `			apCur[i] = pMap->pFirst;` |
|      23 | 6685 | `			if( pMap->nEntry > nMax ){` |
|      13 | 6686 | `				nMax = pMap->nEntry;` |
|       6 | 6687 | `			}` |
|      12 | 6688 | `		}` |
|      35 | 6689 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 6690 | `			ph7_value *pZip = 0;` |
|      25 | 6691 | `			if( bNullCallback ){` |
|       - | 6692 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 6693 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 6694 | `			}` |
|      79 | 6695 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 6696 | `				ph7_value *pv = &sNull;` |
|      55 | 6697 | `				if( apCur[i] ){` |
|      53 | 6698 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 6699 | `					if( pNodeVal ){` |
|      53 | 6700 | `						pv = pNodeVal;` |
|      26 | 6701 | `					}` |
|      53 | 6702 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 6703 | `				}` |
|      55 | 6704 | `				if( bNullCallback ){` |
|       9 | 6705 | `					if( pZip ){` |
|       9 | 6706 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 6707 | `					}` |
|       5 | 6708 | `				}else{` |
|      47 | 6709 | `					apCallArg[i] = pv;` |
|       - | 6710 | `				}` |
|      28 | 6711 | `			}` |
|      25 | 6712 | `			if( bNullCallback ){` |
|       5 | 6713 | `				if( pZip ){` |
|       5 | 6714 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 6715 | `				}` |
|       3 | 6716 | `			}else{` |
|      21 | 6717 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 6718 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6719 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 6720 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 6721 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 6722 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6723 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6724 | `					return PH7_EXCEPTION;` |
|       - | 6725 | `				}` |
|      21 | 6726 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 6727 | `				PH7_MemObjRelease(&sResult);` |
|       - | 6728 | `			}` |
|      13 | 6729 | `		}` |
|      11 | 6730 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 6731 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 6732 | `		PH7_MemObjRelease(&sNull);` |
|       - | 6733 | `	}` |
|      36 | 6734 | `	PH7_MemObjRelease(&sKey);` |
|      36 | 6735 | `	PH7_MemObjRelease(&sResult);` |
|      36 | 6736 | `	ph7_result_value(pCtx,pArray);` |
|      36 | 6737 | `	return PH7_OK;` |
|      25 | 6738 |  |
|       - | 6739 | `/*` |
|       - | 6740 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6741 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6742 | ` * Parameters` |
|       - | 6743 | ` *  $array` |
|       - | 6744 | ` *   The input array.` |
|       - | 6745 | ` *  $callback` |
|       - | 6746 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6747 | ` *  $initial` |
|       - | 6748 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6749 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6750 | ` * Return` |
|       - | 6751 | ` *  Returns the resulting value.` |
|       - | 6752 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6753 | ` */` |
|      32 | 6754 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6755 |  |
|       - | 6756 | `	ph7_hashmap_node *pEntry;` |
|       - | 6757 | `	ph7_hashmap *pMap;` |
|       - | 6758 | `	ph7_value *pValue;` |
|       - | 6759 | `	ph7_value sResult;` |
|       - | 6760 | `	sxi32 rc;` |
|       - | 6761 | `	sxu32 n;` |
|      34 | 6762 | `	if( nArg < 2 ){` |
|       7 | 6763 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6764 | `			"ArgumentCountError",` |
|       - | 6765 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6766 | `			nArg` |
|       - | 6767 | `			);` |
|       - | 6768 | `	}` |
|      30 | 6769 | `	if( nArg > 3 ){` |
|       4 | 6770 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6771 | `			"ArgumentCountError",` |
|       - | 6772 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6773 | `			nArg` |
|       - | 6774 | `			);` |
|       - | 6775 | `	}` |
|      28 | 6776 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6777 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6778 | `			"TypeError",` |
|       - | 6779 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6780 | `			ph7_type_name(apArg[0])` |
|       - | 6781 | `			);` |
|       - | 6782 | `	}` |
|      26 | 6783 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6784 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6785 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6786 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6787 | `				"TypeError",` |
|       - | 6788 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6789 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6790 | `				zFunc` |
|       - | 6791 | `				);` |
|       - | 6792 | `		}` |
|       7 | 6793 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6794 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6795 | `				"TypeError",` |
|       - | 6796 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6797 | `				"array callback must have exactly two members"` |
|       - | 6798 | `				);` |
|       - | 6799 | `		}` |
|       5 | 6800 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6801 | `			"TypeError",` |
|       - | 6802 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6803 | `			"no array or string given"` |
|       - | 6804 | `			);` |
|       - | 6805 | `	}` |
|       - | 6806 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 6807 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6808 | `	/* Assume a NULL initial value */` |
|      17 | 6809 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      17 | 6810 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      17 | 6811 | `	if( nArg > 2 ){` |
|       - | 6812 | `		/* Set the initial value */` |
|      11 | 6813 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6814 | `	}` |
|       - | 6815 | `	/* Perform the requested operation */` |
|      17 | 6816 | `	pEntry = pMap->pFirst;` |
|      45 | 6817 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6818 | `		/* Extract the node value */` |
|      31 | 6819 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6820 | `		/* Invoke the supplied callback */` |
|      31 | 6821 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      31 | 6822 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 6823 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6824 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 6825 | `			return PH7_EXCEPTION;` |
|       - | 6826 | `		}` |
|       - | 6827 | `		/* Point to the next entry */` |
|      29 | 6828 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6829 | `	}` |
|      15 | 6830 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6831 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6832 | `	return PH7_OK;` |
|      18 | 6833 |  |
|       - | 6834 | `/*` |
|       - | 6835 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6836 | ` *  Apply a user function to every member of an array.` |
|       - | 6837 | ` * Parameters` |
|       - | 6838 | ` *  $array` |
|       - | 6839 | ` *   The input array.` |
|       - | 6840 | ` *  $funcname` |
|       - | 6841 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6842 | ` *   the first, and the key/index second.` |
|       - | 6843 | ` * Note:` |
|       - | 6844 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6845 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6846 | ` *  be made in the original array itself.` |
|       - | 6847 | ` *  $userdata` |
|       - | 6848 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6849 | ` *   to the callback funcname.` |
|       - | 6850 | ` * Return` |
|       - | 6851 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6852 | ` */` |
|      38 | 6853 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6854 |  |
|       - | 6855 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6856 | `	ph7_hashmap_node *pEntry;` |
|       - | 6857 | `	ph7_hashmap *pMap;` |
|       - | 6858 | `	sxu32 n;` |
|      40 | 6859 | `	if( nArg < 2 ){` |
|       7 | 6860 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6861 | `			"ArgumentCountError",` |
|       - | 6862 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6863 | `			nArg` |
|       - | 6864 | `			);` |
|       - | 6865 | `	}` |
|      36 | 6866 | `	if( nArg > 3 ){` |
|       4 | 6867 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6868 | `			"ArgumentCountError",` |
|       - | 6869 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6870 | `			nArg` |
|       - | 6871 | `			);` |
|       - | 6872 | `	}` |
|      34 | 6873 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6874 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6875 | `			"TypeError",` |
|       - | 6876 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6877 | `			ph7_type_name(apArg[0])` |
|       - | 6878 | `			);` |
|       - | 6879 | `	}` |
|      32 | 6880 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6881 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6882 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6883 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6884 | `				"TypeError",` |
|       - | 6885 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6886 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6887 | `				zFunc` |
|       - | 6888 | `				);` |
|       - | 6889 | `		}` |
|       9 | 6890 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6891 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6892 | `				"TypeError",` |
|       - | 6893 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6894 | `				"array callback must have exactly two members"` |
|       - | 6895 | `				);` |
|       - | 6896 | `		}` |
|       5 | 6897 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6898 | `			"TypeError",` |
|       - | 6899 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6900 | `			"no array or string given"` |
|       - | 6901 | `			);` |
|       - | 6902 | `	}` |
|      21 | 6903 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6904 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6905 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 6906 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 6907 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 6908 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6909 | `	/* Perform the desired operation */` |
|      21 | 6910 | `	pEntry = pMap->pFirst;` |
|      61 | 6911 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6912 | `		/* Extract the node value */` |
|      43 | 6913 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 6914 | `		if( pValue ){` |
|       - | 6915 | `			sxi32 rcW;` |
|       - | 6916 | `			/* Extract the entry key */` |
|      43 | 6917 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6918 | `			/* Invoke the supplied callback */` |
|      43 | 6919 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 6920 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 6921 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 6922 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6923 | `				return PH7_EXCEPTION;` |
|       - | 6924 | `			}` |
|      20 | 6925 | `		}` |
|       - | 6926 | `		/* Point to the next entry */` |
|      41 | 6927 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6928 | `	}` |
|       - | 6929 | `	/* All done, return TRUE */` |
|      19 | 6930 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6931 | `	return PH7_OK;` |
|      21 | 6932 |  |
|       - | 6933 | `/*` |
|       - | 6934 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6935 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6936 | ` */` |
|      22 | 6937 | `static sxi32 HashmapWalkRecursive(` |
|       - | 6938 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6939 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6940 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6941 | `	int iNest             /* Nesting level */` |
|       - | 6942 | `	)` |
|       1 | 6943 |  |
|       - | 6944 | `	ph7_hashmap_node *pEntry;` |
|       - | 6945 | `	ph7_value *pValue,sKey;` |
|       - | 6946 | `	sxi32 rc;` |
|       - | 6947 | `	sxu32 n;` |
|       - | 6948 | `	/* Iterate through hashmap entries */` |
|      23 | 6949 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6950 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6951 | `	pEntry = pMap->pFirst;` |
|      59 | 6952 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6953 | `		/* Extract the node value */` |
|      37 | 6954 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6955 | `		if( pValue ){` |
|      37 | 6956 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6957 | `				if( iNest < 32 ){` |
|       - | 6958 | `					/* Recurse */` |
|      11 | 6959 | `					iNest++;` |
|      11 | 6960 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6961 | `					iNest--;` |
|      11 | 6962 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6963 | `						return PH7_EXCEPTION;` |
|       - | 6964 | `					}` |
|       5 | 6965 | `				}` |
|       6 | 6966 | `			}else{` |
|       - | 6967 | `				/* Extract the node key */` |
|      27 | 6968 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6969 | `				/* Invoke the supplied callback */` |
|      27 | 6970 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6971 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 6972 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 6973 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 6974 | `					return PH7_EXCEPTION;` |
|       - | 6975 | `				}` |
|       - | 6976 | `			}` |
|      18 | 6977 | `		}` |
|       - | 6978 | `		/* Point to the next entry */` |
|      37 | 6979 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6980 | `	}` |
|      23 | 6981 | `	return PH7_OK;` |
|      12 | 6982 |  |
|       - | 6983 | `/*` |
|       - | 6984 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6985 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6986 | ` * Parameters` |
|       - | 6987 | ` *  $array` |
|       - | 6988 | ` *   The input array.` |
|       - | 6989 | ` *  $funcname` |
|       - | 6990 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6991 | ` *   the first, and the key/index second.` |
|       - | 6992 | ` * Note:` |
|       - | 6993 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6994 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6995 | ` *  be made in the original array itself.` |
|       - | 6996 | ` *  $userdata` |
|       - | 6997 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6998 | ` *   to the callback funcname.` |
|       - | 6999 | ` * Return` |
|       - | 7000 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 7001 | ` */` |
|      30 | 7002 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 7003 |  |
|       - | 7004 | `	ph7_hashmap *pMap;` |
|      32 | 7005 | `	if( nArg < 2 ){` |
|       7 | 7006 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7007 | `			"ArgumentCountError",` |
|       - | 7008 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 7009 | `			nArg` |
|       - | 7010 | `			);` |
|       - | 7011 | `	}` |
|      28 | 7012 | `	if( nArg > 3 ){` |
|       4 | 7013 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7014 | `			"ArgumentCountError",` |
|       - | 7015 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 7016 | `			nArg` |
|       - | 7017 | `			);` |
|       - | 7018 | `	}` |
|      26 | 7019 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7020 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7021 | `			"TypeError",` |
|       - | 7022 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7023 | `			ph7_type_name(apArg[0])` |
|       - | 7024 | `			);` |
|       - | 7025 | `	}` |
|      24 | 7026 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 7027 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7028 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7029 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7030 | `				"TypeError",` |
|       - | 7031 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7032 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7033 | `				zFunc` |
|       - | 7034 | `				);` |
|       - | 7035 | `		}` |
|       9 | 7036 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 7037 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7038 | `				"TypeError",` |
|       - | 7039 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7040 | `				"array callback must have exactly two members"` |
|       - | 7041 | `				);` |
|       - | 7042 | `		}` |
|       5 | 7043 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7044 | `			"TypeError",` |
|       - | 7045 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7046 | `			"no array or string given"` |
|       - | 7047 | `			);` |
|       - | 7048 | `	}` |
|       - | 7049 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 7050 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 7051 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7052 | `	/* Perform the desired operation */` |
|      13 | 7053 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 7054 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7055 | `		return PH7_EXCEPTION;` |
|       - | 7056 | `	}` |
|       - | 7057 | `	/* All done, return TRUE */` |
|      13 | 7058 | `	ph7_result_bool(pCtx,1);` |
|      13 | 7059 | `	return PH7_OK;` |
|      17 | 7060 |  |
|       - | 7061 | `/*` |
|       - | 7062 | ` * bool array_is_list(array $array)` |
|       - | 7063 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 7064 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 7065 | ` * Return` |
|       - | 7066 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 7067 | ` */` |
|       - | 7068 | `/*` |
|       - | 7069 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 7070 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 7071 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 7072 | ` */` |
|      60 | 7073 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       1 | 7074 |  |
|      61 | 7075 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|      61 | 7076 | `	sxi64 iExpect = 0;` |
|       - | 7077 | `	sxu32 n;` |
|     129 | 7078 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     101 | 7079 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 7080 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|      33 | 7081 | `			return 0;` |
|       - | 7082 | `		}` |
|      69 | 7083 | `		++iExpect;` |
|      69 | 7084 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      35 | 7085 | `	}` |
|      29 | 7086 | `	return 1;` |
|      31 | 7087 |  |
|      12 | 7088 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7089 |  |
|      13 | 7090 | `	if( nArg < 1 ){` |
|     ! 0 | 7091 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7092 | `			"ArgumentCountError",` |
|       - | 7093 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 7094 | `			);` |
|       - | 7095 | `	}` |
|      13 | 7096 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7097 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7098 | `			"TypeError",` |
|       - | 7099 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7100 | `			ph7_type_name(apArg[0])` |
|       - | 7101 | `			);` |
|       - | 7102 | `	}` |
|      13 | 7103 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 7104 | `	return PH7_OK;` |
|       7 | 7105 |  |
|       - | 7106 | `/*` |
|       - | 7107 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 7108 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 7109 | ` * array_column() for both the column value and the index key.` |
|       - | 7110 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 7111 | ` * container or the key is absent.` |
|       - | 7112 | ` */` |
|      32 | 7113 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 7114 |  |
|      33 | 7115 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 7116 | `		ph7_hashmap_node *pNode;` |
|      25 | 7117 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 7118 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 7119 | `		}` |
|      11 | 7120 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 7121 | `		ph7_value sName;` |
|       - | 7122 | `		const char *zName;` |
|       - | 7123 | `		ph7_value *pAttr;` |
|       - | 7124 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 7125 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 7126 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 7127 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 7128 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 7129 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 7130 | `		PH7_MemObjRelease(&sName);` |
|       9 | 7131 | `		return pAttr;` |
|       - | 7132 | `	}` |
|       5 | 7133 | `	return 0;` |
|      17 | 7134 |  |
|       - | 7135 | `/*` |
|       - | 7136 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 7137 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 7138 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 7139 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 7140 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 7141 | ` *  Each row may be an array or an object.` |
|       - | 7142 | ` */` |
|      12 | 7143 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7144 |  |
|       - | 7145 | `	ph7_hashmap_node *pNode;` |
|       - | 7146 | `	ph7_hashmap *pMap;` |
|       - | 7147 | `	ph7_value *pArray;` |
|       - | 7148 | `	ph7_value *pRow;` |
|       - | 7149 | `	ph7_value *pCol;` |
|       - | 7150 | `	ph7_value *pIdx;` |
|       - | 7151 | `	int bWantCol;` |
|       - | 7152 | `	int bWantIdx;` |
|       - | 7153 | `	sxu32 n;` |
|      13 | 7154 | `	if( nArg < 2 ){` |
|     ! 0 | 7155 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7156 | `			"ArgumentCountError",` |
|       - | 7157 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 7158 | `			nArg` |
|       - | 7159 | `			);` |
|       - | 7160 | `	}` |
|      13 | 7161 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7162 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7163 | `			"TypeError",` |
|       - | 7164 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7165 | `			ph7_type_name(apArg[0])` |
|       - | 7166 | `			);` |
|       - | 7167 | `	}` |
|      13 | 7168 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 7169 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 7170 | `	if( pArray == 0 ){` |
|     ! 0 | 7171 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7172 | `		return PH7_OK;` |
|       - | 7173 | `	}` |
|       - | 7174 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 7175 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 7176 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 7177 | `	pNode = pMap->pFirst;` |
|      33 | 7178 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 7179 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 7180 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 7181 | `		if( pRow == 0 ){` |
|     ! 0 | 7182 | `			continue;` |
|       - | 7183 | `		}` |
|      21 | 7184 | `		if( bWantCol ){` |
|      19 | 7185 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 7186 | `			if( pCol == 0 ){` |
|       - | 7187 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 7188 | `				continue;` |
|       - | 7189 | `			}` |
|       9 | 7190 | `		}else{` |
|       3 | 7191 | `			pCol = pRow;` |
|       - | 7192 | `		}` |
|      19 | 7193 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 7194 | `		if( pIdx ){` |
|      13 | 7195 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 7196 | `		}else{` |
|       7 | 7197 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 7198 | `		}` |
|      10 | 7199 | `	}` |
|      13 | 7200 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 7201 | `	return PH7_OK;` |
|       7 | 7202 |  |
|       - | 7203 | `/*` |
|       - | 7204 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 7205 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 7206 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 7207 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 7208 | ` */` |
|      28 | 7209 | `static sxi32 HashmapCallbackSearch(` |
|       - | 7210 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 7211 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 7212 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 7213 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 7214 | `	)` |
|       1 | 7215 |  |
|       - | 7216 | `	ph7_hashmap_node *pEntry;` |
|       - | 7217 | `	ph7_hashmap *pMap;` |
|       - | 7218 | `	ph7_value *pValue;` |
|       - | 7219 | `	ph7_value *apCbArg[2];` |
|       - | 7220 | `	ph7_value sKey;` |
|       - | 7221 | `	ph7_value sResult;` |
|       - | 7222 | `	sxi32 rc;` |
|       - | 7223 | `	sxu32 n;` |
|      29 | 7224 | `	*ppMatch = 0;` |
|      29 | 7225 | `	if( nArg < 2 ){` |
|     ! 0 | 7226 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7227 | `			"ArgumentCountError",` |
|       - | 7228 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 7229 | `			zName,nArg` |
|       - | 7230 | `			);` |
|       - | 7231 | `	}` |
|      29 | 7232 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7233 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7234 | `			"TypeError",` |
|       - | 7235 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7236 | `			zName,ph7_type_name(apArg[0])` |
|       - | 7237 | `			);` |
|       - | 7238 | `	}` |
|      29 | 7239 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7240 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7241 | `			"TypeError",` |
|       - | 7242 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 7243 | `			zName,ph7_type_name(apArg[1])` |
|       - | 7244 | `			);` |
|       - | 7245 | `	}` |
|      29 | 7246 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 7247 | `	pEntry = pMap->pFirst;` |
|      29 | 7248 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 7249 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 7250 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 7251 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 7252 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 7253 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 7254 | `		if( pValue ){` |
|       - | 7255 | `			/* The callback receives ($value, $key). */` |
|      59 | 7256 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 7257 | `			apCbArg[0] = pValue;` |
|      59 | 7258 | `			apCbArg[1] = &sKey;` |
|      59 | 7259 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 7260 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 7261 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7262 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7263 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7264 | `				return PH7_EXCEPTION;` |
|       - | 7265 | `			}` |
|      59 | 7266 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 7267 | `				*ppMatch = pEntry;` |
|      15 | 7268 | `				break;` |
|       - | 7269 | `			}` |
|      22 | 7270 | `		}` |
|      45 | 7271 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 7272 | `	}` |
|      29 | 7273 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 7274 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 7275 | `	return PH7_OK;` |
|      15 | 7276 |  |
|       - | 7277 | `/*` |
|       - | 7278 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 7279 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 7280 | ` *  is truthy, or NULL if none match.` |
|       - | 7281 | ` */` |
|       6 | 7282 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7283 |  |
|       - | 7284 | `	ph7_hashmap_node *pMatch;` |
|       - | 7285 | `	ph7_value *pVal;` |
|       - | 7286 | `	sxi32 rc;` |
|       7 | 7287 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 7288 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7289 | `		return rc;` |
|       - | 7290 | `	}` |
|       7 | 7291 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 7292 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 7293 | `	}else{` |
|       3 | 7294 | `		ph7_result_null(pCtx);` |
|       - | 7295 | `	}` |
|       7 | 7296 | `	return PH7_OK;` |
|       4 | 7297 |  |
|       - | 7298 | `/*` |
|       - | 7299 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 7300 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 7301 | ` *  is truthy, or NULL if none match.` |
|       - | 7302 | ` */` |
|       6 | 7303 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7304 |  |
|       - | 7305 | `	ph7_hashmap_node *pMatch;` |
|       - | 7306 | `	sxi32 rc;` |
|       7 | 7307 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 7308 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7309 | `		return rc;` |
|       - | 7310 | `	}` |
|       7 | 7311 | `	if( pMatch == 0 ){` |
|       3 | 7312 | `		ph7_result_null(pCtx);` |
|       6 | 7313 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 7314 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 7315 | `	}else{` |
|       4 | 7316 | `		ph7_result_string(pCtx,` |
|       2 | 7317 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 7318 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 7319 | `	}` |
|       7 | 7320 | `	return PH7_OK;` |
|       4 | 7321 |  |
|       - | 7322 | `/*` |
|       - | 7323 | ` * bool array_any(array $array, callable $callback)` |
|       - | 7324 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 7325 | ` *  FALSE for an empty array.` |
|       - | 7326 | ` */` |
|       8 | 7327 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7328 |  |
|       - | 7329 | `	ph7_hashmap_node *pMatch;` |
|       - | 7330 | `	sxi32 rc;` |
|       9 | 7331 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 7332 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7333 | `		return rc;` |
|       - | 7334 | `	}` |
|       9 | 7335 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 7336 | `	return PH7_OK;` |
|       5 | 7337 |  |
|       - | 7338 | `/*` |
|       - | 7339 | ` * bool array_all(array $array, callable $callback)` |
|       - | 7340 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 7341 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 7342 | ` */` |
|       8 | 7343 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7344 |  |
|       - | 7345 | `	ph7_hashmap_node *pMatch;` |
|       - | 7346 | `	sxi32 rc;` |
|       9 | 7347 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 7348 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7349 | `		return rc;` |
|       - | 7350 | `	}` |
|       9 | 7351 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 7352 | `	return PH7_OK;` |
|       5 | 7353 |  |
|       - | 7354 | `/*` |
|       - | 7355 | ` * Table of hashmap functions.` |
|       - | 7356 | ` */` |
|       - | 7357 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 7358 | `	{"count",             ph7_hashmap_count },` |
|       - | 7359 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 7360 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 7361 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 7362 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 7363 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 7364 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 7365 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 7366 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 7367 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 7368 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 7369 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 7370 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 7371 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 7372 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 7373 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 7374 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 7375 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 7376 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 7377 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 7378 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 7379 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 7380 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 7381 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 7382 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 7383 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 7384 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 7385 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 7386 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 7387 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 7388 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 7389 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 7390 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 7391 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 7392 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 7393 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 7394 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 7395 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 7396 | `	{"array_column",      ph7_hashmap_column  },` |
|       - | 7397 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|       - | 7398 | `	{"array_find",        ph7_hashmap_find    },` |
|       - | 7399 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|       - | 7400 | `	{"array_any",         ph7_hashmap_any     },` |
|       - | 7401 | `	{"array_all",         ph7_hashmap_all     },` |
|       - | 7402 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 7403 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 7404 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 7405 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 7406 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 7407 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 7408 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 7409 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 7410 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 7411 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 7412 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 7413 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 7414 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 7415 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 7416 | `	{"range",             ph7_hashmap_range   },` |
|       - | 7417 | `	{"current",           ph7_hashmap_current },` |
|       - | 7418 | `	{"each",              ph7_hashmap_each    },` |
|       - | 7419 | `	{"pos",               ph7_hashmap_current },` |
|       - | 7420 | `	{"next",              ph7_hashmap_next    },` |
|       - | 7421 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 7422 | `	{"end",               ph7_hashmap_end     },` |
|       - | 7423 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 7424 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 7425 | `};` |
|       - | 7426 | `/*` |
|       - | 7427 | ` * Register the built-in hashmap functions defined above.` |
|       - | 7428 | ` */` |
|    2820 | 7429 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 7430 |  |
|       - | 7431 | `	sxu32 n;` |
|  191762 | 7432 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  188942 | 7433 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   94472 | 7434 | `	}` |
|    2822 | 7435 |  |
|       - | 7436 | `/*` |
|       - | 7437 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 7438 | ` * the BLOB given as the first argument.` |
|       - | 7439 | ` * This function is typically invoked when the user issue a call to` |
|       - | 7440 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 7441 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 7442 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 7443 | ` */` |
|      26 | 7444 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 7445 |  |
|       - | 7446 | `	ph7_hashmap_node *pEntry;` |
|       - | 7447 | `	ph7_value *pObj;` |
|      28 | 7448 | `	sxu32 n = 0;` |
|       - | 7449 | `	int isRef;` |
|       - | 7450 | `	sxi32 rc;` |
|       - | 7451 | `	int i;` |
|      28 | 7452 | `	if( nDepth > 31 ){` |
|       - | 7453 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 7454 | `		/* Nesting limit reached */` |
|     ! 0 | 7455 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 7456 | `		if( ShowType ){` |
|     ! 0 | 7457 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 7458 | `		}` |
|     ! 0 | 7459 | `		return SXERR_LIMIT;` |
|       - | 7460 | `	}` |
|       - | 7461 | `	/* Point to the first inserted entry */` |
|      28 | 7462 | `	pEntry = pMap->pFirst;` |
|      28 | 7463 | `	rc = SXRET_OK;` |
|      28 | 7464 | `	if( !ShowType ){` |
|      15 | 7465 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 7466 | `	}` |
|       - | 7467 | `	/* Total entries */` |
|      28 | 7468 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 7469 | `#ifdef __WINNT__` |
|       2 | 7470 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7471 | `#else` |
|      26 | 7472 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7473 | `#endif` |
|      62 | 7474 | `	for(;;){` |
|     126 | 7475 | `		if( n >= pMap->nEntry ){` |
|      28 | 7476 | `			break;` |
|       - | 7477 | `		}` |
|     198 | 7478 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 7479 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 7480 | `		}` |
|       - | 7481 | `		/* Dump key */` |
|     100 | 7482 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 7483 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 7484 | `		}else{` |
|     101 | 7485 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 7486 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 7487 | `		}` |
|       - | 7488 | `#ifdef __WINNT__` |
|       2 | 7489 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7490 | `#else` |
|      98 | 7491 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7492 | `#endif` |
|       - | 7493 | `		/* Dump node value */` |
|     100 | 7494 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 7495 | `		isRef = 0;` |
|     100 | 7496 | `		if( pObj ){` |
|     100 | 7497 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 7498 | `				/* Referenced object */` |
|     ! 0 | 7499 | `				isRef = 1;` |
|     ! 0 | 7500 | `			}` |
|     100 | 7501 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 7502 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 7503 | `				break;` |
|       - | 7504 | `			}` |
|      49 | 7505 | `		}` |
|       - | 7506 | `		/* Point to the next entry */` |
|     100 | 7507 | `		n++;` |
|     100 | 7508 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 7509 | `	}` |
|      54 | 7510 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 7511 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 7512 | `	}` |
|      28 | 7513 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 7514 | `	return rc;` |
|      15 | 7515 |  |
|       - | 7516 | `/*` |
|       - | 7517 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 7518 | ` * retrieved entry.` |
|       - | 7519 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 7520 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 7521 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 7522 | ` * a value different from PH7_OK.` |
|       - | 7523 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 7524 | ` */` |
|   29796 | 7525 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 7526 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 7527 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 7528 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 7529 | `	)` |
|       2 | 7530 |  |
|       - | 7531 | `	ph7_hashmap_node *pEntry;` |
|       - | 7532 | `	ph7_value sKey,sValue;` |
|       - | 7533 | `	sxi32 rc;` |
|       - | 7534 | `	sxu32 n;` |
|       - | 7535 | `	/* Initialize walker parameter */` |
|   29798 | 7536 | `	rc = SXRET_OK;` |
|   29798 | 7537 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   29798 | 7538 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   29798 | 7539 | `	n = pMap->nEntry;` |
|   29798 | 7540 | `	pEntry = pMap->pFirst;` |
|       - | 7541 | `	/* Start the iteration process */` |
|   74412 | 7542 | `	for(;;){` |
|  148826 | 7543 | `		if( n < 1 ){` |
|   29798 | 7544 | `			break;` |
|       - | 7545 | `		}` |
|       - | 7546 | `		/* Extract a copy of the key and a copy the current value */` |
|  119030 | 7547 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  119030 | 7548 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 7549 | `		/* Invoke the user callback */` |
|  119030 | 7550 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 7551 | `		/* Release the copy of the key and the value */` |
|  119030 | 7552 | `		PH7_MemObjRelease(&sKey);` |
|  119030 | 7553 | `		PH7_MemObjRelease(&sValue);` |
|  119030 | 7554 | `		if( rc != PH7_OK ){` |
|       - | 7555 | `			/* Callback request an operation abort */` |
|     ! 0 | 7556 | `			return SXERR_ABORT;` |
|       - | 7557 | `		}` |
|       - | 7558 | `		/* Point to the next entry */` |
|  119030 | 7559 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  119030 | 7560 | `		n--;` |
|       2 | 7561 | `	}` |
|       - | 7562 | `	/* All done */` |
|   29798 | 7563 | `	return SXRET_OK;` |
|   14900 | 7564 |  |
|       - | 7565 |  |
