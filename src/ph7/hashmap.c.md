# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3098/3556 lines (87.12%)

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
| 3020328 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 3020330 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  314246 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  314248 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  314248 |   29 | `	sxu32 nH = 5381;` |
|  314248 |   30 | `	zEnd = &zIn[nLen];` |
|  349680 |   31 | `	for(;;){` |
|  699362 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  611802 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  549092 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  454380 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  314248 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     910 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       2 |   47 |  |
|     912 |   48 | `	sxi64 iCount = 0;` |
|     912 |   49 | `	if( !bRecursive ){` |
|     738 |   50 | `		iCount = pMap->nEntry;` |
|     370 |   51 | `	}else{` |
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
|     912 |   86 | `	return iCount;` |
|       2 |   87 |  |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 2961194 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 2961196 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2961196 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 2961196 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 2961196 |  104 | `	pNode->pMap  = &(*pMap);` |
| 2961196 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2961196 |  106 | `	pNode->nHash = nHash;` |
| 2961196 |  107 | `	pNode->xKey.iKey = iKey;` |
| 2961196 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 2961196 |  109 | `	return pNode;` |
| 1480599 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|  108080 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|  108082 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  108082 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|  108082 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|  108082 |  127 | `	pNode->pMap  = &(*pMap);` |
|  108082 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  108082 |  129 | `	pNode->nHash = nHash;` |
|  108082 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  108082 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  108082 |  132 | `	pNode->nValIdx = nValIdx;` |
|  108082 |  133 | `	return pNode;` |
|   54042 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 3069274 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  139 |  |
|       - |  140 | `	/* Link */` |
| 3069276 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2766214 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2766214 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1383106 |  144 | `	}` |
| 3069276 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 3069276 |  147 | `	if( pMap->pFirst == 0 ){` |
|   54322 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   54322 |  150 | `		pMap->pCur = pNode;` |
|   27162 |  151 | `	}else{` |
| 3014956 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 3069276 |  154 | `	++pMap->nEntry;` |
| 3069276 |  155 |  |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    6924 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  161 |  |
|    6926 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    6926 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    6926 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    6478 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3240 |  167 | `	}else{` |
|     449 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    6926 |  170 | `	if( pNode->pNextCollide ){` |
|    5403 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2701 |  172 | `	}` |
|    6926 |  173 | `	if( pMap->pFirst == pNode ){` |
|     100 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      49 |  175 | `	}` |
|    6926 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|     102 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      50 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    6926 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    6926 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     104 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     104 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     104 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  188 | `		}` |
|      51 |  189 | `	}` |
|    6926 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    6792 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3395 |  192 | `	}` |
|    6926 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    6926 |  194 | `	pMap->nEntry--;` |
|    6926 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      46 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      46 |  198 | `		pMap->apBucket = 0;` |
|      46 |  199 | `		pMap->nSize = 0;` |
|      46 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      22 |  201 | `	}` |
|    6926 |  202 |  |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 3069274 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  208 |  |
| 3069276 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   58606 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   58606 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   58606 |  215 | `		if( nNew < 1 ){` |
|   54322 |  216 | `			nNew = 16;` |
|   27160 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   58606 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   58606 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   58606 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   58606 |  230 | `		pMap->apBucket = apNew;` |
|   58606 |  231 | `		pMap->nSize = nNew;` |
|   58606 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   54322 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    4286 |  237 | `		pEntry = pMap->pFirst;` |
|    4286 |  238 | `		n = 0;` |
| 2029470 |  239 | `		for( ;; ){` |
| 4058942 |  240 | `			if( n >= pMap->nEntry ){` |
|    4286 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 4054658 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 4054658 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4054658 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3511594 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3511594 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1755796 |  250 | `			}` |
| 4054658 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 4054658 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4054658 |  254 | `			n++;` |
|       2 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    4286 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2142 |  258 | `	}` |
| 3014956 |  259 | `	return SXRET_OK;` |
| 1534639 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 2961194 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 2961196 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		/* Reserve a ph7_value for the value */` |
| 2961162 |  274 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2961162 |  275 | `		if( pObj == 0 ){` |
|     ! 0 |  276 | `			return SXERR_MEM;` |
|       - |  277 | `		}` |
| 2961162 |  278 | `		if( pValue ){` |
|       - |  279 | `			/* Duplicate the value */` |
| 2961162 |  280 | `			PH7_MemObjStore(pValue,pObj);` |
| 1480580 |  281 | `		}` |
| 2961162 |  282 | `		nIdx = pObj->nIdx;` |
| 1480582 |  283 | `	}else{` |
|      35 |  284 | `		nIdx = nRefIdx;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Hash the key */` |
| 2961196 |  287 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  288 | `	/* Allocate a new int node */` |
| 2961196 |  289 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2961196 |  290 | `	if( pNode == 0 ){` |
|     ! 0 |  291 | `		return SXERR_MEM;` |
|       - |  292 | `	}` |
| 2961196 |  293 | `	if( isForeign ){` |
|       - |  294 | `		/* Mark as a foregin entry */` |
|      35 |  295 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      17 |  296 | `	}` |
|       - |  297 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2961196 |  298 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2961196 |  299 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  300 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  301 | `		return rc;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Perform the insertion */` |
| 2961196 |  304 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  305 | `	/* Install in the reference table */` |
| 2961196 |  306 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  307 | `	/* All done */` |
| 2961196 |  308 | `	return SXRET_OK;` |
| 1480599 |  309 |  |
|       - |  310 | `/*` |
|       - |  311 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  312 | ` * hashmap.` |
|       - |  313 | ` */` |
|  108080 |  314 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  315 |  |
|       - |  316 | `	ph7_hashmap_node *pNode;` |
|       - |  317 | `	sxu32 nHash;` |
|       - |  318 | `	sxu32 nIdx;` |
|       - |  319 | `	sxi32 rc;` |
|  108082 |  320 | `	if( !isForeign ){` |
|       - |  321 | `		ph7_value *pObj;` |
|       - |  322 | `		/* Reserve a ph7_value for the value */` |
|   72808 |  323 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   72808 |  324 | `		if( pObj == 0 ){` |
|     ! 0 |  325 | `			return SXERR_MEM;` |
|       - |  326 | `		}` |
|   72808 |  327 | `		if( pValue ){` |
|       - |  328 | `			/* Duplicate the value */` |
|   72536 |  329 | `			PH7_MemObjStore(pValue,pObj);` |
|   36267 |  330 | `		}` |
|   72808 |  331 | `		nIdx = pObj->nIdx;` |
|   36405 |  332 | `	}else{` |
|   35276 |  333 | `		nIdx = nRefIdx;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Hash the key */` |
|  108082 |  336 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  337 | `	/* Allocate a new blob node */` |
|  108082 |  338 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  108082 |  339 | `	if( pNode == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|  108082 |  342 | `	if( isForeign ){` |
|       - |  343 | `		/* Mark as a foregin entry */` |
|   35276 |  344 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   17637 |  345 | `	}` |
|       - |  346 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  108082 |  347 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  108082 |  348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  349 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  350 | `		return rc;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Perform the insertion */` |
|  108082 |  353 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  354 | `	/* Install in the reference table */` |
|  108082 |  355 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  356 | `	/* All done */` |
|  108082 |  357 | `	return SXRET_OK;` |
|   54042 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  361 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  362 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  363 | ` */` |
|   47850 |  364 | `static sxi32 HashmapLookupIntKey(` |
|       - |  365 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  366 | `	sxi64 iKey,                /* lookup key */` |
|       - |  367 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  368 | `	)` |
|       2 |  369 |  |
|       - |  370 | `	ph7_hashmap_node *pNode;` |
|       - |  371 | `	sxu32 nHash;` |
|   47852 |  372 | `	if( pMap->nEntry < 1 ){` |
|       - |  373 | `		/* Don't bother hashing,there is no entry anyway */` |
|     446 |  374 | `		return SXERR_NOTFOUND;` |
|       - |  375 | `	}` |
|       - |  376 | `	/* Hash the key first */` |
|   47408 |  377 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  378 | `	/* Point to the appropriate bucket */` |
|   47408 |  379 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  380 | `	/* Perform the lookup */` |
|  412058 |  381 | `	for(;;){` |
|  824118 |  382 | `		if( pNode == 0 ){` |
|   45998 |  383 | `			break;` |
|       - |  384 | `		}` |
|  778825 |  385 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  775105 |  386 | `			&& pNode->nHash == nHash` |
|  386752 |  387 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  388 | `				/* Node found */` |
|    1412 |  389 | `				if( ppNode ){` |
|    1400 |  390 | `					*ppNode = pNode;` |
|     699 |  391 | `				}` |
|    1412 |  392 | `				return SXRET_OK;` |
|       - |  393 | `		}` |
|       - |  394 | `		/* Follow the collision link */` |
|  776711 |  395 | `		pNode = pNode->pNextCollide;` |
|       1 |  396 | `	}` |
|       - |  397 | `	/* No such entry */` |
|   45998 |  398 | `	return SXERR_NOTFOUND;` |
|   23927 |  399 |  |
|       - |  400 | `/*` |
|       - |  401 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  402 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  403 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  404 | ` */` |
|  219596 |  405 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  406 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  407 | `	const void *pKey,           /* Lookup key */` |
|       - |  408 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  409 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  410 | `	)` |
|       2 |  411 |  |
|       - |  412 | `	ph7_hashmap_node *pNode;` |
|       - |  413 | `	sxu32 nHash;` |
|  219598 |  414 | `	if( pMap->nEntry < 1 ){` |
|       - |  415 | `		/* Don't bother hashing,there is no entry anyway */` |
|   13432 |  416 | `		return SXERR_NOTFOUND;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Hash the key first */` |
|  206168 |  419 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  420 | `	/* Point to the appropriate bucket */` |
|  206168 |  421 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  422 | `	/* Perform the lookup */` |
|  185924 |  423 | `	for(;;){` |
|  371850 |  424 | `		if( pNode == 0 ){` |
|  157488 |  425 | `			break;` |
|       - |  426 | `		}` |
|  238702 |  427 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  212863 |  428 | `			&& pNode->nHash == nHash` |
|  130022 |  429 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   48682 |  430 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  431 | `				/* Node found */` |
|   48682 |  432 | `				if( ppNode ){` |
|   48654 |  433 | `					*ppNode = pNode;` |
|   24326 |  434 | `				}` |
|   48682 |  435 | `				return SXRET_OK;` |
|       - |  436 | `		}` |
|       - |  437 | `		/* Follow the collision link */` |
|  165684 |  438 | `		pNode = pNode->pNextCollide;` |
|       2 |  439 | `	}` |
|       - |  440 | `	/* No such entry */` |
|  157488 |  441 | `	return SXERR_NOTFOUND;` |
|  109800 |  442 |  |
|       - |  443 | `/*` |
|       - |  444 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  445 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  446 | ` */` |
|  219736 |  447 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  448 |  |
|  219738 |  449 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  219738 |  450 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  219738 |  451 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  452 | `		/* Octal not decimal number */` |
|       5 |  453 | `		return FALSE;` |
|       - |  454 | `	}` |
|  219734 |  455 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  456 | `		zIn++;` |
|     ! 0 |  457 | `	}` |
|  110199 |  458 | `	for(;;){` |
|  220400 |  459 | `		if( zIn >= zEnd ){` |
|     233 |  460 | `			return TRUE;` |
|       - |  461 | `		}` |
|  220168 |  462 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  109752 |  463 | `			break;` |
|       - |  464 | `		}` |
|     667 |  465 | `		zIn++;` |
|       1 |  466 | `	}` |
|       - |  467 | `	/* Key does not look like a decimal number */` |
|  219502 |  468 | `	return FALSE;` |
|  109870 |  469 |  |
|       - |  470 | `/*` |
|       - |  471 | ` * Check if a given key exists in the given hashmap.` |
|       - |  472 | ` * Write a pointer to the target node on success.` |
|       - |  473 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  474 | ` */` |
|  112732 |  475 | `static sxi32 HashmapLookup(` |
|       - |  476 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  477 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  478 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  479 | `	)` |
|       2 |  480 |  |
|  112734 |  481 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  482 | `	sxi32 rc;` |
|  112734 |  483 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  111462 |  484 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  485 | `			/* Force a string cast */` |
|     ! 0 |  486 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  487 | `		}` |
|  111462 |  488 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  489 | `			/* Perform a blob lookup */` |
|  111446 |  490 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  111446 |  491 | `			goto result;` |
|       - |  492 | `		}` |
|       8 |  493 | `	}` |
|       - |  494 | `	/* Perform an int lookup */` |
|    1290 |  495 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  496 | `		/* Force an integer cast */` |
|      27 |  497 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  498 | `	}` |
|       - |  499 | `	/* Perform an int lookup */` |
|    1290 |  500 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   56366 |  501 | `result:` |
|  112734 |  502 | `	if( rc == SXRET_OK ){` |
|       - |  503 | `		/* Node found */` |
|   49828 |  504 | `		if( ppNode ){` |
|   49786 |  505 | `			*ppNode = pNode;` |
|   24892 |  506 | `		}` |
|   49828 |  507 | `		return SXRET_OK;` |
|       - |  508 | `	}` |
|       - |  509 | `	/* No such entry */` |
|   62908 |  510 | `	return SXERR_NOTFOUND;` |
|   56368 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  514 | ` * hashmap.` |
|       - |  515 | ` * If a node with the given key already exists in the database` |
|       - |  516 | ` * then this function overwrite the old value.` |
|       - |  517 | ` */` |
| 3033684 |  518 | `static sxi32 HashmapInsert(` |
|       - |  519 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  520 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  521 | `	ph7_value *pVal    /* Node value */` |
|       - |  522 | `	)` |
|       2 |  523 |  |
| 3033686 |  524 | `	ph7_hashmap_node *pNode = 0;` |
| 3033686 |  525 | `	sxi32 rc = SXRET_OK;` |
| 3033686 |  526 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   73036 |  527 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  528 | `			/* Force a string cast */` |
|       3 |  529 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  530 | `		}` |
|   73036 |  531 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  532 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  533 | `				/* Automatic index assign */` |
|      34 |  534 | `				pKey = 0;` |
|      16 |  535 | `			}` |
|     256 |  536 | `			goto IntKey;` |
|       - |  537 | `		}` |
|  109172 |  538 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   36390 |  539 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|   72712 |  553 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  554 | `			/* Forbidden */` |
|       3 |  555 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Perform a blob-key insertion */` |
|   72710 |  559 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   72710 |  560 | `		return rc;` |
|       - |  561 | `	}` |
| 1480325 |  562 | `IntKey:` |
| 2960906 |  563 | `	if( pKey ){` |
|   23408 |  564 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  565 | `			/* Force an integer cast */` |
|     251 |  566 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  567 | `		}` |
|   23408 |  568 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   23322 |  582 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  583 | `			/* Forbidden */` |
|       3 |  584 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  585 | `			return SXRET_OK;` |
|       - |  586 | `		}` |
|       - |  587 | `		/* Perform a 64-bit-int-key insertion */` |
|   23320 |  588 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23320 |  589 | `		if( rc == SXRET_OK ){` |
|   23320 |  590 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  591 | `				/* Increment the automatic index */` |
|   23084 |  592 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  593 | `				/* Make sure the automatic index is not reserved */` |
|   23084 |  594 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  595 | `					pMap->iNextIdx++;` |
|     ! 0 |  596 | `				}` |
|   11541 |  597 | `			}` |
|   11659 |  598 | `		}` |
|   11661 |  599 | `	}else{` |
| 2937500 |  600 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  601 | `			/* Forbidden */` |
|       3 |  602 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|       - |  605 | `		/* Assign an automatic index */` |
| 2937498 |  606 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2937498 |  607 | `		if( rc == SXRET_OK ){` |
| 2937498 |  608 | `			++pMap->iNextIdx;` |
| 1468748 |  609 | `		}` |
|       - |  610 | `	}` |
|       - |  611 | `	/* Insertion result */` |
| 2960816 |  612 | `	return rc;` |
| 1516844 |  613 |  |
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
|   35314 |  641 | `static sxi32 HashmapInsertByRef(` |
|       - |  642 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  643 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  644 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  645 | `	)` |
|       2 |  646 |  |
|   35316 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|   35316 |  648 | `	sxi32 rc = SXRET_OK;` |
|   35316 |  649 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   35282 |  650 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  651 | `			/* Force a string cast */` |
|     ! 0 |  652 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  653 | `		}` |
|   35282 |  654 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  655 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  656 | `				/* Automatic index assign */` |
|     ! 0 |  657 | `				pKey = 0;` |
|     ! 0 |  658 | `			}` |
|     ! 0 |  659 | `			goto IntKey;` |
|       - |  660 | `		}` |
|   52922 |  661 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   17640 |  662 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  663 | `				/* Overwrite */` |
|       7 |  664 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  665 | `				pNode->nValIdx = nRefIdx;` |
|       - |  666 | `				/* Install in the reference table */` |
|       7 |  667 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  668 | `				return SXRET_OK;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Perform a blob-key insertion */` |
|   35276 |  671 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   35276 |  672 | `		return rc;` |
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
|   17659 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * Extract node value.` |
|       - |  712 | ` */` |
| 1193157 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
| 1193159 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1193159 |  718 | `	return pObj;` |
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
|   60266 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   60268 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   60268 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   60268 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   60268 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   60268 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   60268 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   60268 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   60268 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   60268 |  783 | `	return rc;` |
|   30152 |  784 |  |
|       - |  785 | `/*` |
|       - |  786 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  787 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  788 | ` */` |
|   11728 |  789 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  790 |  |
|   11730 |  791 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  792 | `	sxu32 nBucket;` |
|       - |  793 | `	/* Remove old collision links */` |
|   11730 |  794 | `	if( pEntry->pPrevCollide ){` |
|    9458 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    4738 |  796 | `	}else{` |
|    2274 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  798 | `	}` |
|   11730 |  799 | `	if( pEntry->pNextCollide ){` |
|     898 |  800 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     440 |  801 | `	}` |
|   11730 |  802 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  803 | `	/* Compute the new hash */` |
|   11730 |  804 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   11730 |  805 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   11730 |  806 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  807 | `	/* Link to the new bucket */` |
|   11730 |  808 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11730 |  809 | `	if( pMap->apBucket[nBucket] ){` |
|    9710 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    4863 |  811 | `	}` |
|   11730 |  812 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11730 |  813 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  814 | `	/* Increment the automatic index */` |
|   11730 |  815 | `	pMap->iNextIdx++;` |
|   11730 |  816 |  |
|       - |  817 | `/*` |
|       - |  818 | ` * Perform a linear search on a given hashmap.` |
|       - |  819 | ` * Write a pointer to the target node on success.` |
|       - |  820 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  821 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  822 | ` * for more information.` |
|       - |  823 | ` */` |
|   29336 |  824 | `static int HashmapFindValue(` |
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
|   29338 |  837 | `	pEntry = pMap->pFirst;` |
|   29338 |  838 | `	n = pMap->nEntry;` |
|   29338 |  839 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   29338 |  840 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   70272 |  841 | `	for(;;){` |
|  140547 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  140449 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  140449 |  847 | `		if( pVal ){` |
|  140449 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  140449 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  140449 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  140449 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  140449 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  140449 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  140449 |  865 | `				if( rc == 0 ){` |
|   29240 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   29240 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   55604 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|  111211 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  111211 |  876 | `		n--;` |
|       2 |  877 | `	}` |
|       - |  878 | `	/* No such entry */` |
|      99 |  879 | `	return SXERR_NOTFOUND;` |
|   14670 |  880 |  |
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
|  551818 | 1066 | `static sxi32 HashmapDuplicateNode(` |
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
|  551820 | 1077 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
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
|  551814 | 1102 | `	sSafeVal = *pVal;` |
|       - | 1103 |  |
|  551814 | 1104 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1105 | `		/* Blob key insertion */` |
|      95 | 1106 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      95 | 1107 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      95 | 1108 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      95 | 1109 | `		PH7_MemObjRelease(&sKey);` |
|      48 | 1110 | `	}else{` |
|       - | 1111 | `		/* Int key */` |
|  551720 | 1112 | `		if( iAction == 0 ){ /* Merge */` |
|  551498 | 1113 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  275972 | 1114 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1115 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1116 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1117 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1118 | `		}else{ /* Dup */` |
|     194 | 1119 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1120 | `		}` |
|       - | 1121 | `	}` |
|  551814 | 1122 | `	return rc;` |
|  275911 | 1123 |  |
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
|    1984 | 1136 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1137 |  |
|       - | 1138 | `	ph7_hashmap_node *pEntry;` |
|       - | 1139 | `	ph7_value *pVal;` |
|       - | 1140 | `	sxi32 rc;` |
|       - | 1141 | `	sxu32 n;` |
|    1986 | 1142 | `	if( pSrc == pDest ){` |
|       - | 1143 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1144 | `		 * Unlike the zend engine.` |
|       - | 1145 | `		 */` |
|     ! 0 | 1146 | `		return SXRET_OK;` |
|       - | 1147 | `	}` |
|       - | 1148 | `	/* Point to the first inserted entry in the source */` |
|    1986 | 1149 | `	pEntry = pSrc->pFirst;` |
|       - | 1150 | `	/* Perform the merge */` |
|  553540 | 1151 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1152 | `		/* Extract the node value */` |
|  551556 | 1153 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  551556 | 1154 | `		if( pVal ){` |
|       - | 1155 | `			/* Make a local copy of the value.` |
|       - | 1156 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1157 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1158 | `			 * to the old pool.` |
|       - | 1159 | `			 */` |
|  551556 | 1160 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  275779 | 1161 | `		}else{` |
|     ! 0 | 1162 | `			rc = SXRET_OK;` |
|       - | 1163 | `		}` |
|  551556 | 1164 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1165 | `			return rc;` |
|       - | 1166 | `		}` |
|       - | 1167 | `		/* Point to the next entry */` |
|  551556 | 1168 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  275779 | 1169 | `	}` |
|    1986 | 1170 | `	return SXRET_OK;` |
|     994 | 1171 |  |
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
|  194390 | 1258 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       2 | 1259 |  |
|  194392 | 1260 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1261 | `	ph7_hashmap *pNew;` |
|       - | 1262 | `	ph7_value *pBacking;` |
|  194392 | 1263 | `	if( pMap->iRef < 2 ){` |
|       - | 1264 | `		/* Sole owner, no separation needed */` |
|  192318 | 1265 | `		return pMap;` |
|       - | 1266 | `	}` |
|    2076 | 1267 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1268 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1269 | `		return pMap;` |
|       - | 1270 | `	}` |
|       - | 1271 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1272 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1273 | `	 * frame is popped. */` |
|    2076 | 1274 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2076 | 1275 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    3094 | 1276 | `		if( pBacking && pBacking != pValue` |
|    2057 | 1277 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2042 | 1278 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1279 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2042 | 1280 | `			pMap->iRef--;` |
|    2042 | 1281 | `			if( pMap->iRef < 2 ){` |
|       - | 1282 | `				/* After undoing stack ref, sole owner — no separation */` |
|    2006 | 1283 | `				pMap->iRef++;` |
|    2006 | 1284 | `				return pMap;` |
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
|   97197 | 1320 |  |
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
|   85710 | 1412 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1413 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1414 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1415 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1416 | `	)` |
|       2 | 1417 |  |
|       - | 1418 | `	ph7_hashmap *pMap;` |
|       - | 1419 | `	/* Allocate a new instance */` |
|   85712 | 1420 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   85712 | 1421 | `	if( pMap == 0 ){` |
|     ! 0 | 1422 | `		return 0;` |
|       - | 1423 | `	}` |
|       - | 1424 | `	/* Zero the structure */` |
|   85712 | 1425 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1426 | `	/* Fill in the structure */` |
|   85712 | 1427 | `	pMap->pVm = &(*pVm);` |
|   85712 | 1428 | `	pMap->iRef = 1;` |
|       - | 1429 | `	/* Default hash functions */` |
|   85712 | 1430 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   85712 | 1431 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   85712 | 1432 | `	return pMap;` |
|   42857 | 1433 |  |
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
|   54454 | 1525 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1526 |  |
|       - | 1527 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   54456 | 1528 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1529 | `	sxu32 n;` |
|   54456 | 1530 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1531 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1532 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1533 | `		return SXRET_OK;` |
|       - | 1534 | `	}` |
|       - | 1535 | `	/* Start the release process */` |
|   54456 | 1536 | `	n = 0;` |
|   54456 | 1537 | `	pEntry = pMap->pFirst;` |
| 1539062 | 1538 | `	for(;;){` |
| 3078126 | 1539 | `		if( n >= pMap->nEntry ){` |
|   54456 | 1540 | `			break;` |
|       - | 1541 | `		}` |
| 3023672 | 1542 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1543 | `		/* Remove the reference from the foreign table */` |
| 3023672 | 1544 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3023672 | 1545 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1546 | `			/* Restore the ph7_value to the free list */` |
| 3023664 | 1547 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1511831 | 1548 | `		}` |
|       - | 1549 | `		/* Release the node */` |
| 3023672 | 1550 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   68638 | 1551 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   34318 | 1552 | `		}` |
| 3023672 | 1553 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1554 | `		/* Point to the next entry */` |
| 3023672 | 1555 | `		pEntry = pNext;` |
| 3023672 | 1556 | `		n++;` |
|       2 | 1557 | `	}` |
|   54456 | 1558 | `	if( pMap->nEntry > 0 ){` |
|       - | 1559 | `		/* Release the hash bucket */` |
|   48380 | 1560 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   24189 | 1561 | `	}` |
|   54456 | 1562 | `	if( FreeDS ){` |
|       - | 1563 | `		/* Free the whole instance */` |
|   54440 | 1564 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   27221 | 1565 | `	}else{` |
|       - | 1566 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1567 | `		pMap->apBucket = 0;` |
|      17 | 1568 | `		pMap->iNextIdx = 0;` |
|      17 | 1569 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1570 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1571 | `	}` |
|   54456 | 1572 | `	return SXRET_OK;` |
|   27229 | 1573 |  |
|       - | 1574 | `/*` |
|       - | 1575 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1576 | ` * If the count reaches zero which mean no more variables` |
|       - | 1577 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1578 | ` */` |
|  599994 | 1579 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1580 |  |
|  599996 | 1581 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1582 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  599996 | 1583 | `	pMap->iRef--;` |
|  599996 | 1584 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   54424 | 1585 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   27211 | 1586 | `	}` |
|  599996 | 1587 |  |
|       - | 1588 | `/*` |
|       - | 1589 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1590 | ` * Write a pointer to the target node on success.` |
|       - | 1591 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1592 | ` */` |
|  112772 | 1593 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1594 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1595 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1596 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1597 | `	)` |
|       2 | 1598 |  |
|       - | 1599 | `	sxi32 rc;` |
|  112774 | 1600 | `	if( pMap->nEntry < 1 ){` |
|       - | 1601 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1602 | `		 */` |
|      42 | 1603 | `		return SXERR_NOTFOUND;` |
|       - | 1604 | `	}` |
|  112734 | 1605 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  112734 | 1606 | `	return rc;` |
|   56388 | 1607 |  |
|       - | 1608 | `/*` |
|       - | 1609 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1610 | ` * hashmap.` |
|       - | 1611 | ` * If a node with the given key already exists in the database` |
|       - | 1612 | ` * then this function overwrite the old value.` |
|       - | 1613 | ` */` |
| 2481976 | 1614 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1615 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1616 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1617 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1618 | `	)` |
|       2 | 1619 |  |
|       - | 1620 | `	sxi32 rc;` |
| 2481978 | 1621 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1622 | `		/*` |
|       - | 1623 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1624 | `		 */` |
|     ! 0 | 1625 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1626 | `		return SXRET_OK;` |
|       - | 1627 | `	}` |
| 2481978 | 1628 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2481978 | 1629 | `	return rc;` |
| 1240990 | 1630 |  |
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
|   35308 | 1668 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1669 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1670 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1671 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1672 | `	)` |
|       2 | 1673 |  |
|       - | 1674 | `	sxi32 rc;` |
|   35310 | 1675 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1676 | `		/*` |
|       - | 1677 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1678 | `		 */` |
|     ! 0 | 1679 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1680 | `		return SXRET_OK;` |
|       - | 1681 | `	}` |
|   35310 | 1682 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   35310 | 1683 | `	return rc;` |
|   17656 | 1684 |  |
|       - | 1685 | `/*` |
|       - | 1686 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1687 | ` */` |
|   24342 | 1688 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1689 |  |
|       - | 1690 | `	/* Reset the loop cursor */` |
|   24344 | 1691 | `	pMap->pCur = pMap->pFirst;` |
|   24344 | 1692 |  |
|       - | 1693 | `/*` |
|       - | 1694 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1695 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1696 | ` * return NULL.` |
|       - | 1697 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1698 | ` */` |
|  200428 | 1699 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1700 |  |
|  200430 | 1701 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  200430 | 1702 | `	if( pCur == 0 ){` |
|       - | 1703 | `		/* End of the list,return null */` |
|   12192 | 1704 | `		return 0;` |
|       - | 1705 | `	}` |
|       - | 1706 | `	/* Advance the node cursor */` |
|  188240 | 1707 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  188240 | 1708 | `	return pCur;` |
|  100216 | 1709 |  |
|       - | 1710 | `/*` |
|       - | 1711 | ` * Extract a node value.` |
|       - | 1712 | ` */` |
|  476822 | 1713 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1714 |  |
|  476824 | 1715 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  476824 | 1716 | `	if( pEntry ){` |
|  476824 | 1717 | `		if( bStore ){` |
|  188378 | 1718 | `			PH7_MemObjStore(pEntry,pValue);` |
|   94190 | 1719 | `		}else{` |
|  288448 | 1720 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1721 | `		}` |
|  238447 | 1722 | `	}else{` |
|     ! 0 | 1723 | `		PH7_MemObjRelease(pValue);` |
|       - | 1724 | `	}` |
|  476824 | 1725 |  |
|       - | 1726 | `/*` |
|       - | 1727 | ` * Extract a node key.` |
|       - | 1728 | ` */` |
|  118608 | 1729 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1730 |  |
|       - | 1731 | `	/* Fill with the current key */` |
|  118610 | 1732 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  118290 | 1733 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1734 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1735 | `		}` |
|  118290 | 1736 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  118290 | 1737 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   59146 | 1738 | `	}else{` |
|     322 | 1739 | `		SyBlobReset(&pKey->sBlob);` |
|     322 | 1740 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     322 | 1741 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1742 | `	}` |
|  118610 | 1743 |  |
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
|       - | 1770 | `/*` |
|       - | 1771 | ` * Merge sort.` |
|       - | 1772 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1773 | ` * Status: Public domain` |
|       - | 1774 | ` */` |
|       - | 1775 | `/* Node comparison callback signature */` |
|       - | 1776 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1777 | `/*` |
|       - | 1778 | `** Inputs:` |
|       - | 1779 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1780 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1781 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1782 | `**` |
|       - | 1783 | `** Return Value:` |
|       - | 1784 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1785 | `**   of both a and b.` |
|       - | 1786 | `**` |
|       - | 1787 | `** Side effects:` |
|       - | 1788 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1789 | `**   changed.` |
|       - | 1790 | `*/` |
|   31018 | 1791 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1792 |  |
|       - | 1793 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1794 | `    /* Prevent compiler warning */` |
|   31020 | 1795 | `	result.pNext = result.pPrev = 0;` |
|   31020 | 1796 | `	pTail = &result;` |
|   91428 | 1797 | `	while( pA && pB ){` |
|   60410 | 1798 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   40450 | 1799 | `			pTail->pPrev = pA;` |
|   40450 | 1800 | `			pA->pNext = pTail;` |
|   40450 | 1801 | `			pTail = pA;` |
|   40450 | 1802 | `			pA = pA->pPrev;` |
|   20201 | 1803 | `		}else{` |
|   19962 | 1804 | `			pTail->pPrev = pB;` |
|   19962 | 1805 | `			pB->pNext = pTail;` |
|   19962 | 1806 | `			pTail = pB;` |
|   19962 | 1807 | `			pB = pB->pPrev;` |
|       - | 1808 | `		}` |
|       2 | 1809 | `	}` |
|   31020 | 1810 | `	if( pA ){` |
|   22115 | 1811 | `		pTail->pPrev = pA;` |
|   22115 | 1812 | `		pA->pNext = pTail;` |
|   19985 | 1813 | `	}else if( pB ){` |
|    8693 | 1814 | `		pTail->pPrev = pB;` |
|    8693 | 1815 | `		pB->pNext = pTail;` |
|    4326 | 1816 | `	}else{` |
|     216 | 1817 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1818 | `	}` |
|   31020 | 1819 | `	return result.pPrev;` |
|       2 | 1820 |  |
|       - | 1821 | `/*` |
|       - | 1822 | `** Inputs:` |
|       - | 1823 | `**   Map:       Input hashmap` |
|       - | 1824 | `**   cmp:       A comparison function.` |
|       - | 1825 | `**` |
|       - | 1826 | `** Return Value:` |
|       - | 1827 | `**   Sorted hashmap.` |
|       - | 1828 | `**` |
|       - | 1829 | `** Side effects:` |
|       - | 1830 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1831 | `*/` |
|       - | 1832 | `#define N_SORT_BUCKET  32` |
|     656 | 1833 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1834 |  |
|       - | 1835 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1836 | `	sxu32 i;` |
|     658 | 1837 | `	SyZero(a,sizeof(a));` |
|       - | 1838 | `	/* Point to the first inserted entry */` |
|     658 | 1839 | `	pIn = pMap->pFirst;` |
|   12502 | 1840 | `	while( pIn ){` |
|   11846 | 1841 | `		p = pIn;` |
|   11846 | 1842 | `		pIn = p->pPrev;` |
|   11846 | 1843 | `		p->pPrev = 0;` |
|   22528 | 1844 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   22528 | 1845 | `			if( a[i]==0 ){` |
|   11846 | 1846 | `				a[i] = p;` |
|   11846 | 1847 | `				break;` |
|     ! 0 | 1848 | `			}else{` |
|   10684 | 1849 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   10684 | 1850 | `				a[i] = 0;` |
|       - | 1851 | `			}` |
|    5343 | 1852 | `		}` |
|   11846 | 1853 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1854 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1855 | `			 * But that is impossible.` |
|       - | 1856 | `			 */` |
|     ! 0 | 1857 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1858 | `		}` |
|       2 | 1859 | `	}` |
|     658 | 1860 | `	p = a[0];` |
|   20994 | 1861 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   20338 | 1862 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   10170 | 1863 | `	}` |
|     658 | 1864 | `	p->pNext = 0;` |
|       - | 1865 | `	/* Reflect the change */` |
|     658 | 1866 | `	pMap->pFirst = p;` |
|       - | 1867 | `	/* Reset the loop cursor */` |
|     658 | 1868 | `	pMap->pCur = pMap->pFirst;` |
|     658 | 1869 | `	return SXRET_OK;` |
|       2 | 1870 |  |
|       - | 1871 | `/*` |
|       - | 1872 | ` * Node comparison callback.` |
|       - | 1873 | ` * used-by: [sort(),asort(),...]` |
|       - | 1874 | ` */` |
|   60214 | 1875 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1876 |  |
|       - | 1877 | `	ph7_value sA,sB;` |
|       - | 1878 | `	sxi32 iFlags;` |
|       - | 1879 | `	int rc;` |
|   60216 | 1880 | `	if( pCmpData == 0 ){` |
|       - | 1881 | `		/* Perform a standard comparison */` |
|   60192 | 1882 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   60192 | 1883 | `		return rc;` |
|       - | 1884 | `	}` |
|      25 | 1885 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1886 | `	/* Duplicate node values */` |
|      25 | 1887 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 1888 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 1889 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 1890 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 1891 | `	if( iFlags == 5 ){` |
|       - | 1892 | `		/* String cast */` |
|       - | 1893 | `		const char *zA,*zB;` |
|       - | 1894 | `		sxu32 nA,nB,nMin;` |
|      15 | 1895 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1896 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1897 | `		}` |
|      15 | 1898 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1899 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1900 | `		}` |
|       - | 1901 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 1902 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 1903 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 1904 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 1905 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 1906 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 1907 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 1908 | `		if( rc == 0 ){` |
|       5 | 1909 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1910 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1911 | `		}` |
|       8 | 1912 | `	}else{` |
|       - | 1913 | `		/* Numeric cast */` |
|      11 | 1914 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1915 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1916 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1917 | `	}` |
|      25 | 1918 | `	PH7_MemObjRelease(&sA);` |
|      25 | 1919 | `	PH7_MemObjRelease(&sB);` |
|      25 | 1920 | `	return rc;` |
|   30126 | 1921 |  |
|       - | 1922 | `/*` |
|       - | 1923 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1924 | ` * used-by: [ksort()]` |
|       - | 1925 | ` */` |
|      14 | 1926 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1927 |  |
|       - | 1928 | `	sxi32 rc;` |
|       7 | 1929 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 1930 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1931 | `		/* Perform a string comparison */` |
|       5 | 1932 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1933 | `	}else{` |
|       - | 1934 | `		SyString sStr;` |
|       - | 1935 | `		sxi64 iA,iB;` |
|       - | 1936 | `		/* Perform a numeric comparison */` |
|      11 | 1937 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1938 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1939 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1940 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1941 | `				iA = 0;` |
|     ! 0 | 1942 | `			}else{` |
|     ! 0 | 1943 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1944 | `			}` |
|     ! 0 | 1945 | `		}else{` |
|      11 | 1946 | `			iA = pA->xKey.iKey;` |
|       - | 1947 | `		}` |
|      11 | 1948 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1949 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1950 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1951 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1952 | `				iB = 0;` |
|     ! 0 | 1953 | `			}else{` |
|     ! 0 | 1954 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1955 | `			}` |
|     ! 0 | 1956 | `		}else{` |
|      11 | 1957 | `			iB = pB->xKey.iKey;` |
|       - | 1958 | `		}` |
|      11 | 1959 | `		rc = (sxi32)(iA-iB);` |
|       - | 1960 | `	}` |
|       - | 1961 | `	/* Comparison result */` |
|      15 | 1962 | `	return rc;` |
|       1 | 1963 |  |
|       - | 1964 | `/*` |
|       - | 1965 | ` * Node comparison callback.` |
|       - | 1966 | ` * Used by: [rsort(),arsort()];` |
|       - | 1967 | ` */` |
|      78 | 1968 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1969 |  |
|       - | 1970 | `	ph7_value sA,sB;` |
|       - | 1971 | `	sxi32 iFlags;` |
|       - | 1972 | `	int rc;` |
|      79 | 1973 | `	if( pCmpData == 0 ){` |
|       - | 1974 | `		/* Perform a standard comparison */` |
|      59 | 1975 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 1976 | `		return -rc;` |
|       - | 1977 | `	}` |
|      21 | 1978 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1979 | `	/* Duplicate node values */` |
|      21 | 1980 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 1981 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 1982 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 1983 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 1984 | `	if( iFlags == 5 ){` |
|       - | 1985 | `		/* String cast */` |
|       - | 1986 | `		const char *zA,*zB;` |
|       - | 1987 | `		sxu32 nA,nB,nMin;` |
|      11 | 1988 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1989 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1990 | `		}` |
|      11 | 1991 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1992 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1993 | `		}` |
|       - | 1994 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 1995 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 1996 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 1997 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 1998 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 1999 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 2000 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 2001 | `		if( rc == 0 ){` |
|       3 | 2002 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 2003 | `			else if( nA > nB ) rc = 1;` |
|       1 | 2004 | `		}` |
|       6 | 2005 | `	}else{` |
|       - | 2006 | `		/* Numeric cast */` |
|      11 | 2007 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 2008 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 2009 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 2010 | `	}` |
|      21 | 2011 | `	PH7_MemObjRelease(&sA);` |
|      21 | 2012 | `	PH7_MemObjRelease(&sB);` |
|      21 | 2013 | `	return -rc;` |
|      40 | 2014 |  |
|       - | 2015 | `/*` |
|       - | 2016 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2017 | ` * used-by: [usort(),uasort()]` |
|       - | 2018 | ` */` |
|      78 | 2019 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 2020 |  |
|       - | 2021 | `	ph7_value sResult,*pCallback;` |
|       - | 2022 | `	ph7_value *pV1,*pV2;` |
|       - | 2023 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2024 | `	sxi32 rc;` |
|       - | 2025 | `	/* Point to the desired callback */` |
|      80 | 2026 | `	pCallback = (ph7_value *)pCmpData;` |
|      80 | 2027 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2028 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2029 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       3 | 2030 | `		return 0;` |
|       - | 2031 | `	}` |
|       - | 2032 | `	/* initialize the result value */` |
|      78 | 2033 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 2034 | `	/* Extract nodes values */` |
|      78 | 2035 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      78 | 2036 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      78 | 2037 | `	apArg[0] = pV1;` |
|      78 | 2038 | `	apArg[1] = pV2;` |
|       - | 2039 | `	/* Invoke the callback */` |
|      78 | 2040 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      78 | 2041 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2042 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2043 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       3 | 2044 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       3 | 2045 | `		rc = 0;` |
|      77 | 2046 | `	}else if( rc != SXRET_OK ){` |
|       - | 2047 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2048 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2049 | `	}else{` |
|       - | 2050 | `		/* Extract callback result */` |
|      76 | 2051 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2052 | `			/* Perform an int cast */` |
|     ! 0 | 2053 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2054 | `		}` |
|      76 | 2055 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2056 | `	}` |
|      78 | 2057 | `	PH7_MemObjRelease(&sResult);` |
|       - | 2058 | `	/* Callback result */` |
|      78 | 2059 | `	return rc;` |
|      41 | 2060 |  |
|       - | 2061 | `/*` |
|       - | 2062 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2063 | ` * used-by: [krsort()]` |
|       - | 2064 | ` */` |
|       4 | 2065 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2066 |  |
|       - | 2067 | `	sxi32 rc;` |
|       2 | 2068 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2069 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2070 | `		/* Perform a string comparison */` |
|       5 | 2071 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2072 | `	}else{` |
|       - | 2073 | `		SyString sStr;` |
|       - | 2074 | `		sxi64 iA,iB;` |
|       - | 2075 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2076 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2077 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2078 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2079 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2080 | `				iA = 0;` |
|     ! 0 | 2081 | `			}else{` |
|     ! 0 | 2082 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2083 | `			}` |
|     ! 0 | 2084 | `		}else{` |
|     ! 0 | 2085 | `			iA = pA->xKey.iKey;` |
|       - | 2086 | `		}` |
|     ! 0 | 2087 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2088 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2089 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2090 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2091 | `				iB = 0;` |
|     ! 0 | 2092 | `			}else{` |
|     ! 0 | 2093 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2094 | `			}` |
|     ! 0 | 2095 | `		}else{` |
|     ! 0 | 2096 | `			iB = pB->xKey.iKey;` |
|       - | 2097 | `		}` |
|     ! 0 | 2098 | `		rc = (sxi32)(iA-iB);` |
|       - | 2099 | `	}` |
|       5 | 2100 | `	return -rc; /* Reverse result */` |
|       1 | 2101 |  |
|       - | 2102 | `/*` |
|       - | 2103 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2104 | ` * used-by: [uksort()]` |
|       - | 2105 | ` */` |
|       6 | 2106 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2107 |  |
|       - | 2108 | `	ph7_value sResult,*pCallback;` |
|       - | 2109 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2110 | `	ph7_value sK1,sK2;` |
|       - | 2111 | `	sxi32 rc;` |
|       - | 2112 | `	/* Point to the desired callback */` |
|       7 | 2113 | `	pCallback = (ph7_value *)pCmpData;` |
|       7 | 2114 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2115 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2116 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     ! 0 | 2117 | `		return 0;` |
|       - | 2118 | `	}` |
|       - | 2119 | `	/* initialize the result value */` |
|       7 | 2120 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2121 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2122 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2123 | `	/* Extract nodes keys */` |
|       7 | 2124 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2125 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2126 | `	apArg[0] = &sK1;` |
|       7 | 2127 | `	apArg[1] = &sK2;` |
|       - | 2128 | `	/* Mark keys as constants */` |
|       7 | 2129 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2130 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2131 | `	/* Invoke the callback */` |
|       7 | 2132 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2133 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2134 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2135 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     ! 0 | 2136 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     ! 0 | 2137 | `		rc = 0;` |
|       7 | 2138 | `	}else if( rc != SXRET_OK ){` |
|       - | 2139 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2140 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2141 | `	}else{` |
|       - | 2142 | `		/* Extract callback result */` |
|       7 | 2143 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2144 | `			/* Perform an int cast */` |
|     ! 0 | 2145 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2146 | `		}` |
|       7 | 2147 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2148 | `	}` |
|       7 | 2149 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2150 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2151 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2152 | `	/* Callback result */` |
|       7 | 2153 | `	return rc;` |
|       4 | 2154 |  |
|       - | 2155 | `/*` |
|       - | 2156 | ` * Node comparison callback: Random node comparison.` |
|       - | 2157 | ` * used-by: [shuffle()]` |
|       - | 2158 | ` */` |
|      14 | 2159 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2160 |  |
|       - | 2161 | `	sxu32 n;` |
|       7 | 2162 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 2163 | `	SXUNUSED(pCmpData);` |
|       - | 2164 | `	/* Grab a random number */` |
|      15 | 2165 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2166 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2167 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2168 | `	 */` |
|      15 | 2169 | `	return n&1 ? 1 : -1;` |
|       1 | 2170 |  |
|       - | 2171 | `/*` |
|       - | 2172 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2173 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2174 | ` */` |
|     608 | 2175 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2176 |  |
|       - | 2177 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2178 | `	sxu32 i;` |
|       - | 2179 | `	/* Rehash all entries */` |
|     610 | 2180 | `	pLast = p = pMap->pFirst;` |
|     610 | 2181 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     610 | 2182 | `	i = 0;` |
|    6141 | 2183 | `	for( ;; ){` |
|   12284 | 2184 | `		if( i >= pMap->nEntry ){` |
|     610 | 2185 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     610 | 2186 | `			break;` |
|       - | 2187 | `		}` |
|   11676 | 2188 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2189 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2190 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2191 | `			/* Change key type */` |
|       5 | 2192 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2193 | `		}` |
|   11676 | 2194 | `		HashmapRehashIntNode(p);` |
|       - | 2195 | `		/* Point to the next entry */` |
|   11676 | 2196 | `		i++;` |
|   11676 | 2197 | `		pLast = p;` |
|   11676 | 2198 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2199 | `	}` |
|     610 | 2200 |  |
|       - | 2201 | `/*` |
|       - | 2202 | ` * Array functions implementation.` |
|       - | 2203 | ` * Status:` |
|       - | 2204 | ` *  Stable.` |
|       - | 2205 | ` */` |
|       - | 2206 | `/*` |
|       - | 2207 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2208 | ` * Sort an array.` |
|       - | 2209 | ` * Parameters` |
|       - | 2210 | ` *  $array` |
|       - | 2211 | ` *   The input array.` |
|       - | 2212 | ` * $sort_flags` |
|       - | 2213 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2214 | ` *  Sorting type flags:` |
|       - | 2215 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2216 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2217 | ` *   SORT_STRING - compare items as strings` |
|       - | 2218 | ` * Return` |
|       - | 2219 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2220 | ` *` |
|       - | 2221 | ` */` |
|     922 | 2222 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2223 |  |
|       - | 2224 | `	ph7_hashmap *pMap;` |
|       - | 2225 | `	/* Make sure we are dealing with a valid hashmap */` |
|     924 | 2226 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2227 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2228 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2229 | `		return PH7_OK;` |
|       - | 2230 | `	}` |
|       - | 2231 | `	/* Point to the internal representation of the input hashmap */` |
|     924 | 2232 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     924 | 2233 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     924 | 2234 | `	if( pMap->nEntry > 1 ){` |
|     598 | 2235 | `		sxi32 iCmpFlags = 0;` |
|     598 | 2236 | `		if( nArg > 1 ){` |
|       - | 2237 | `			/* Extract comparison flags */` |
|       3 | 2238 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2239 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2240 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2241 | `			}` |
|       1 | 2242 | `		}` |
|       - | 2243 | `		/* Do the merge sort */` |
|     598 | 2244 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2245 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     598 | 2246 | `		HashmapSortRehash(pMap);` |
|     298 | 2247 | `	}` |
|       - | 2248 | `	/* All done,return TRUE */` |
|     924 | 2249 | `	ph7_result_bool(pCtx,1);` |
|     924 | 2250 | `	return PH7_OK;` |
|     463 | 2251 |  |
|       - | 2252 | `/*` |
|       - | 2253 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2254 | ` *  Sort an array and maintain index association.` |
|       - | 2255 | ` * Parameters` |
|       - | 2256 | ` *  $array` |
|       - | 2257 | ` *   The input array.` |
|       - | 2258 | ` * $sort_flags` |
|       - | 2259 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2260 | ` *  Sorting type flags:` |
|       - | 2261 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2262 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2263 | ` *   SORT_STRING - compare items as strings` |
|       - | 2264 | ` * Return` |
|       - | 2265 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2266 | ` */` |
|      32 | 2267 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2268 |  |
|       - | 2269 | `	ph7_hashmap *pMap;` |
|       - | 2270 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2271 | `	if( nArg < 1 ){` |
|       3 | 2272 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2273 | `			"ArgumentCountError",` |
|       - | 2274 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2275 | `			);` |
|       - | 2276 | `	}` |
|       - | 2277 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2278 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2279 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2280 | `			"TypeError",` |
|       - | 2281 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2282 | `			ph7_type_name(apArg[0])` |
|       - | 2283 | `			);` |
|       - | 2284 | `	}` |
|       - | 2285 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2286 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2287 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2288 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2289 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2290 | `		if( nArg > 1 ){` |
|       - | 2291 | `			/* Extract comparison flags */` |
|       5 | 2292 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2293 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2294 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2295 | `			}` |
|       2 | 2296 | `		}` |
|       - | 2297 | `		/* Do the merge sort */` |
|      19 | 2298 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2299 | `		/* Fix the last link broken by the merge */` |
|      45 | 2300 | `		while(pMap->pLast->pPrev){` |
|      27 | 2301 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2302 | `		}` |
|       9 | 2303 | `	}` |
|       - | 2304 | `	/* All done,return TRUE */` |
|      23 | 2305 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2306 | `	return PH7_OK;` |
|      18 | 2307 |  |
|       - | 2308 | `/*` |
|       - | 2309 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2310 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2311 | ` * Parameters` |
|       - | 2312 | ` *  $array` |
|       - | 2313 | ` *   The input array.` |
|       - | 2314 | ` * $sort_flags` |
|       - | 2315 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2316 | ` *  Sorting type flags:` |
|       - | 2317 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2318 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2319 | ` *   SORT_STRING - compare items as strings` |
|       - | 2320 | ` * Return` |
|       - | 2321 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2322 | ` */` |
|      32 | 2323 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2324 |  |
|       - | 2325 | `	ph7_hashmap *pMap;` |
|       - | 2326 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2327 | `	if( nArg < 1 ){` |
|       3 | 2328 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2329 | `			"ArgumentCountError",` |
|       - | 2330 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2331 | `			);` |
|       - | 2332 | `	}` |
|       - | 2333 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2334 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2335 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2336 | `			"TypeError",` |
|       - | 2337 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2338 | `			ph7_type_name(apArg[0])` |
|       - | 2339 | `			);` |
|       - | 2340 | `	}` |
|       - | 2341 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2342 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2343 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2344 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2345 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2346 | `		if( nArg > 1 ){` |
|       - | 2347 | `			/* Extract comparison flags */` |
|       5 | 2348 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2349 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2350 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2351 | `			}` |
|       2 | 2352 | `		}` |
|       - | 2353 | `		/* Do the merge sort */` |
|      19 | 2354 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2355 | `		/* Fix the last link broken by the merge */` |
|      35 | 2356 | `		while(pMap->pLast->pPrev){` |
|      17 | 2357 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2358 | `		}` |
|       9 | 2359 | `	}` |
|       - | 2360 | `	/* All done,return TRUE */` |
|      23 | 2361 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2362 | `	return PH7_OK;` |
|      18 | 2363 |  |
|       - | 2364 | `/*` |
|       - | 2365 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2366 | ` *  Sort an array by key.` |
|       - | 2367 | ` * Parameters` |
|       - | 2368 | ` *  $array` |
|       - | 2369 | ` *   The input array.` |
|       - | 2370 | ` * $sort_flags` |
|       - | 2371 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2372 | ` *  Sorting type flags:` |
|       - | 2373 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2374 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2375 | ` *   SORT_STRING - compare items as strings` |
|       - | 2376 | ` * Return` |
|       - | 2377 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2378 | ` */` |
|       4 | 2379 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2380 |  |
|       - | 2381 | `	ph7_hashmap *pMap;` |
|       - | 2382 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2383 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2384 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2385 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2386 | `		return PH7_OK;` |
|       - | 2387 | `	}` |
|       - | 2388 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2389 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2390 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2391 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2392 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2393 | `		if( nArg > 1 ){` |
|       - | 2394 | `			/* Extract comparison flags */` |
|     ! 0 | 2395 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2396 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2397 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2398 | `			}` |
|     ! 0 | 2399 | `		}` |
|       - | 2400 | `		/* Do the merge sort */` |
|       5 | 2401 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2402 | `		/* Fix the last link broken by the merge */` |
|      15 | 2403 | `		while(pMap->pLast->pPrev){` |
|      11 | 2404 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2405 | `		}` |
|       2 | 2406 | `	}` |
|       - | 2407 | `	/* All done,return TRUE */` |
|       5 | 2408 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2409 | `	return PH7_OK;` |
|       3 | 2410 |  |
|       - | 2411 | `/*` |
|       - | 2412 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2413 | ` *  Sort an array by key in reverse order.` |
|       - | 2414 | ` * Parameters` |
|       - | 2415 | ` *  $array` |
|       - | 2416 | ` *   The input array.` |
|       - | 2417 | ` * $sort_flags` |
|       - | 2418 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2419 | ` *  Sorting type flags:` |
|       - | 2420 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2421 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2422 | ` *   SORT_STRING - compare items as strings` |
|       - | 2423 | ` * Return` |
|       - | 2424 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2425 | ` */` |
|       2 | 2426 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2427 |  |
|       - | 2428 | `	ph7_hashmap *pMap;` |
|       - | 2429 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2430 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2431 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2432 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2433 | `		return PH7_OK;` |
|       - | 2434 | `	}` |
|       - | 2435 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2436 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2437 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2438 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2439 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2440 | `		if( nArg > 1 ){` |
|       - | 2441 | `			/* Extract comparison flags */` |
|     ! 0 | 2442 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2443 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2444 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2445 | `			}` |
|     ! 0 | 2446 | `		}` |
|       - | 2447 | `		/* Do the merge sort */` |
|       3 | 2448 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2449 | `		/* Fix the last link broken by the merge */` |
|       7 | 2450 | `		while(pMap->pLast->pPrev){` |
|       5 | 2451 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2452 | `		}` |
|       1 | 2453 | `	}` |
|       - | 2454 | `	/* All done,return TRUE */` |
|       3 | 2455 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2456 | `	return PH7_OK;` |
|       2 | 2457 |  |
|       - | 2458 | `/*` |
|       - | 2459 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2460 | ` * Sort an array in reverse order.` |
|       - | 2461 | ` * Parameters` |
|       - | 2462 | ` *  $array` |
|       - | 2463 | ` *   The input array.` |
|       - | 2464 | ` * $sort_flags` |
|       - | 2465 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2466 | ` *  Sorting type flags:` |
|       - | 2467 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2468 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2469 | ` *   SORT_STRING - compare items as strings` |
|       - | 2470 | ` * Return` |
|       - | 2471 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2472 | ` */` |
|       2 | 2473 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2474 |  |
|       - | 2475 | `	ph7_hashmap *pMap;` |
|       - | 2476 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2477 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2478 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2479 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2480 | `		return PH7_OK;` |
|       - | 2481 | `	}` |
|       - | 2482 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2483 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2484 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2485 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2486 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2487 | `		if( nArg > 1 ){` |
|       - | 2488 | `			/* Extract comparison flags */` |
|     ! 0 | 2489 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2490 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2491 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2492 | `			}` |
|     ! 0 | 2493 | `		}` |
|       - | 2494 | `		/* Do the merge sort */` |
|       3 | 2495 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2496 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2497 | `		HashmapSortRehash(pMap);` |
|       1 | 2498 | `	}` |
|       - | 2499 | `	/* All done,return TRUE */` |
|       3 | 2500 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2501 | `	return PH7_OK;` |
|       2 | 2502 |  |
|       - | 2503 | `/*` |
|       - | 2504 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2505 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2506 | ` * Parameters` |
|       - | 2507 | ` *  $array` |
|       - | 2508 | ` *   The input array.` |
|       - | 2509 | ` * $cmp_function` |
|       - | 2510 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2511 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2512 | ` *  to, or greater than the second.` |
|       - | 2513 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2514 | ` * Return` |
|       - | 2515 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2516 | ` */` |
|       8 | 2517 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2518 |  |
|       - | 2519 | `	ph7_hashmap *pMap;` |
|       - | 2520 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 | 2521 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2522 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2523 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2524 | `		return PH7_OK;` |
|       - | 2525 | `	}` |
|       - | 2526 | `	/* Point to the internal representation of the input hashmap */` |
|      10 | 2527 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      10 | 2528 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      10 | 2529 | `	if( pMap->nEntry > 1 ){` |
|      10 | 2530 | `		ph7_value *pCallback = 0;` |
|       - | 2531 | `		ProcNodeCmp xCmp;` |
|      10 | 2532 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      10 | 2533 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2534 | `			/* Point to the desired callback */` |
|      10 | 2535 | `			pCallback = apArg[1];` |
|       6 | 2536 | `		}else{` |
|       - | 2537 | `			/* Use the default comparison function */` |
|     ! 0 | 2538 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2539 | `		}` |
|       - | 2540 | `		/* Do the merge sort */` |
|      10 | 2541 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      10 | 2542 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2543 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      10 | 2544 | `		HashmapSortRehash(pMap);` |
|      10 | 2545 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2546 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 2547 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2548 | `			return PH7_EXCEPTION;` |
|       - | 2549 | `		}` |
|       3 | 2550 | `	}` |
|       - | 2551 | `	/* All done,return TRUE */` |
|       8 | 2552 | `	ph7_result_bool(pCtx,1);` |
|       8 | 2553 | `	return PH7_OK;` |
|       6 | 2554 |  |
|       - | 2555 | `/*` |
|       - | 2556 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2557 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2558 | ` *  and maintain index association.` |
|       - | 2559 | ` * Parameters` |
|       - | 2560 | ` *  $array` |
|       - | 2561 | ` *   The input array.` |
|       - | 2562 | ` * $cmp_function` |
|       - | 2563 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2564 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2565 | ` *  to, or greater than the second.` |
|       - | 2566 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2567 | ` * Return` |
|       - | 2568 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2569 | ` */` |
|       2 | 2570 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2571 |  |
|       - | 2572 | `	ph7_hashmap *pMap;` |
|       - | 2573 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2574 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2575 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2576 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2577 | `		return PH7_OK;` |
|       - | 2578 | `	}` |
|       - | 2579 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2580 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2581 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2582 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2583 | `		ph7_value *pCallback = 0;` |
|       - | 2584 | `		ProcNodeCmp xCmp;` |
|       3 | 2585 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2586 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2587 | `			/* Point to the desired callback */` |
|       3 | 2588 | `			pCallback = apArg[1];` |
|       2 | 2589 | `		}else{` |
|       - | 2590 | `			/* Use the default comparison function */` |
|     ! 0 | 2591 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2592 | `		}` |
|       - | 2593 | `		/* Do the merge sort */` |
|       3 | 2594 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2595 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2596 | `		/* Fix the last link broken by the merge */` |
|       5 | 2597 | `		while(pMap->pLast->pPrev){` |
|       3 | 2598 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2599 | `		}` |
|       3 | 2600 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2601 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2602 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2603 | `			return PH7_EXCEPTION;` |
|       - | 2604 | `		}` |
|       1 | 2605 | `	}` |
|       - | 2606 | `	/* All done,return TRUE */` |
|       3 | 2607 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2608 | `	return PH7_OK;` |
|       2 | 2609 |  |
|       - | 2610 | `/*` |
|       - | 2611 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2612 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2613 | ` *  function and maintain index association.` |
|       - | 2614 | ` * Parameters` |
|       - | 2615 | ` *  $array` |
|       - | 2616 | ` *   The input array.` |
|       - | 2617 | ` * $cmp_function` |
|       - | 2618 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2619 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2620 | ` *  to, or greater than the second.` |
|       - | 2621 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2622 | ` * Return` |
|       - | 2623 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2624 | ` */` |
|       2 | 2625 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2626 |  |
|       - | 2627 | `	ph7_hashmap *pMap;` |
|       - | 2628 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2629 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2630 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2631 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2632 | `		return PH7_OK;` |
|       - | 2633 | `	}` |
|       - | 2634 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2635 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2636 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2637 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2638 | `		ph7_value *pCallback = 0;` |
|       - | 2639 | `		ProcNodeCmp xCmp;` |
|       3 | 2640 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2641 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2642 | `			/* Point to the desired callback */` |
|       3 | 2643 | `			pCallback = apArg[1];` |
|       2 | 2644 | `		}else{` |
|       - | 2645 | `			/* Use the default comparison function */` |
|     ! 0 | 2646 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2647 | `		}` |
|       - | 2648 | `		/* Do the merge sort */` |
|       3 | 2649 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2650 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2651 | `		/* Fix the last link broken by the merge */` |
|       3 | 2652 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2653 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2654 | `		}` |
|       3 | 2655 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2656 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2657 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2658 | `			return PH7_EXCEPTION;` |
|       - | 2659 | `		}` |
|       1 | 2660 | `	}` |
|       - | 2661 | `	/* All done,return TRUE */` |
|       3 | 2662 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2663 | `	return PH7_OK;` |
|       2 | 2664 |  |
|       - | 2665 | `/*` |
|       - | 2666 | ` * bool shuffle(array &$array)` |
|       - | 2667 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2668 | ` * Parameters` |
|       - | 2669 | ` *  $array` |
|       - | 2670 | ` *   The input array.` |
|       - | 2671 | ` * Return` |
|       - | 2672 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2673 | ` *` |
|       - | 2674 | ` */` |
|       2 | 2675 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2676 |  |
|       - | 2677 | `	ph7_hashmap *pMap;` |
|       - | 2678 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2679 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2680 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2681 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2682 | `		return PH7_OK;` |
|       - | 2683 | `	}` |
|       - | 2684 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2685 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2686 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2687 | `	if( pMap->nEntry > 1 ){` |
|       - | 2688 | `		/* Do the merge sort */` |
|       3 | 2689 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2690 | `		/* Fix the last link broken by the merge */` |
|      11 | 2691 | `		while(pMap->pLast->pPrev){` |
|       9 | 2692 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2693 | `		}` |
|       1 | 2694 | `	}` |
|       - | 2695 | `	/* All done,return TRUE */` |
|       3 | 2696 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2697 | `	return PH7_OK;` |
|       2 | 2698 |  |
|       - | 2699 | `/*` |
|       - | 2700 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2701 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2702 | ` * Parameters` |
|       - | 2703 | ` *  $var` |
|       - | 2704 | ` *   The array or the object.` |
|       - | 2705 | ` * $mode` |
|       - | 2706 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2707 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2708 | ` *  all the elements of a multidimensional array.` |
|       - | 2709 | ` * Return` |
|       - | 2710 | ` *  Returns the number of elements in the array.` |
|       - | 2711 | ` */` |
|     800 | 2712 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2713 |  |
|     802 | 2714 | `	int bRecursive = FALSE;` |
|     802 | 2715 | `	int bCycleDetected = FALSE;` |
|       - | 2716 | `	sxi64 iCount;` |
|     802 | 2717 | `	if( nArg < 1 ){` |
|       3 | 2718 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2719 | `			"ArgumentCountError",` |
|       - | 2720 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2721 | `			);` |
|       - | 2722 | `	}` |
|     800 | 2723 | `	if( nArg > 2 ){` |
|       4 | 2724 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2725 | `			"ArgumentCountError",` |
|       - | 2726 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2727 | `			nArg` |
|       - | 2728 | `			);` |
|       - | 2729 | `	}` |
|       - | 2730 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - | 2731 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - | 2732 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|     798 | 2733 | `	if( nArg > 1 ){` |
|      42 | 2734 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      42 | 2735 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       9 | 2736 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2737 | `				"ValueError",` |
|       - | 2738 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2739 | `				);` |
|       - | 2740 | `		}` |
|      34 | 2741 | `		bRecursive = iMode == 1;` |
|      16 | 2742 | `	}` |
|     790 | 2743 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2744 | `		/* Countable object: dispatch to ->count() */` |
|      28 | 2745 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      18 | 2746 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      18 | 2747 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      18 | 2748 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      16 | 2749 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2750 | `					"count",sizeof("count")-1);` |
|      16 | 2751 | `				if( pMeth ){` |
|       - | 2752 | `					ph7_value sResult;` |
|      16 | 2753 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      16 | 2754 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      16 | 2755 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      16 | 2756 | `					PH7_MemObjRelease(&sResult);` |
|      16 | 2757 | `					return PH7_OK;` |
|       - | 2758 | `				}` |
|     ! 0 | 2759 | `			}` |
|       1 | 2760 | `		}` |
|      19 | 2761 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2762 | `			"TypeError",` |
|       - | 2763 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       6 | 2764 | `			ph7_type_name(apArg[0])` |
|       - | 2765 | `			);` |
|       - | 2766 | `	}` |
|       - | 2767 | `	/* Count */` |
|     764 | 2768 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     764 | 2769 | `	if( bCycleDetected ){` |
|       3 | 2770 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2771 | `	}` |
|     764 | 2772 | `	ph7_result_int64(pCtx,iCount);` |
|     764 | 2773 | `	return PH7_OK;` |
|     402 | 2774 |  |
|       - | 2775 | `/*` |
|       - | 2776 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2777 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2778 | ` * Parameters` |
|       - | 2779 | ` * $key` |
|       - | 2780 | ` *   Value to check.` |
|       - | 2781 | ` * $search` |
|       - | 2782 | ` *  An array with keys to check.` |
|       - | 2783 | ` * Return` |
|       - | 2784 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2785 | ` */` |
|      82 | 2786 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2787 |  |
|       - | 2788 | `	sxi32 rc;` |
|      84 | 2789 | `	if( nArg != 2 ){` |
|       - | 2790 | `		/* PHP requires exactly two arguments */` |
|      10 | 2791 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2792 | `			"ArgumentCountError",` |
|       - | 2793 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2794 | `			nArg` |
|       - | 2795 | `			);` |
|       - | 2796 | `	}` |
|       - | 2797 | `	/* Make sure we are dealing with a valid hashmap */` |
|      78 | 2798 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2799 | `		/* Type mismatch -> TypeError */` |
|       7 | 2800 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2801 | `			"TypeError",` |
|       - | 2802 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2803 | `			ph7_type_name(apArg[1])` |
|       - | 2804 | `			);` |
|       - | 2805 | `	}` |
|       - | 2806 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      74 | 2807 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2808 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2809 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2810 | `			"use an empty string instead"` |
|       - | 2811 | `			);` |
|      73 | 2812 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2813 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2814 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2815 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2816 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2817 | `				,rVal` |
|       - | 2818 | `				);` |
|       1 | 2819 | `		}` |
|       1 | 2820 | `	}` |
|       - | 2821 | `	/* Perform the lookup */` |
|      74 | 2822 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2823 | `	/* lookup result */` |
|      74 | 2824 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      74 | 2825 | `	return PH7_OK;` |
|      43 | 2826 |  |
|       - | 2827 | `/*` |
|       - | 2828 | ` * value array_pop(array $array)` |
|       - | 2829 | ` *   POP the last inserted element from the array.` |
|       - | 2830 | ` * Parameter` |
|       - | 2831 | ` *  The array to get the value from.` |
|       - | 2832 | ` * Return` |
|       - | 2833 | ` *  Poped value or NULL on failure.` |
|       - | 2834 | ` */` |
|      18 | 2835 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2836 |  |
|       - | 2837 | `	ph7_hashmap *pMap;` |
|       - | 2838 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      20 | 2839 | `	if( nArg != 1 ){` |
|       7 | 2840 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2841 | `			"ArgumentCountError",` |
|       - | 2842 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2843 | `			nArg` |
|       - | 2844 | `			);` |
|       - | 2845 | `	}` |
|       - | 2846 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2847 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      16 | 2848 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2849 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2850 | `			"Error",` |
|       - | 2851 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2852 | `			);` |
|       - | 2853 | `	}` |
|       - | 2854 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2855 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2856 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2857 | `			"TypeError",` |
|       - | 2858 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2859 | `			ph7_type_name(apArg[0])` |
|       - | 2860 | `			);` |
|       - | 2861 | `	}` |
|       9 | 2862 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2863 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2864 | `	if( pMap->nEntry < 1 ){` |
|       - | 2865 | `		/* Nothing to pop,return NULL */` |
|       3 | 2866 | `		ph7_result_null(pCtx);` |
|       2 | 2867 | `	}else{` |
|       7 | 2868 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2869 | `		ph7_value *pObj;` |
|       7 | 2870 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2871 | `		if( pObj ){` |
|       - | 2872 | `			/* Node value */` |
|       7 | 2873 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2874 | `			/* Unlink the node */` |
|       7 | 2875 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2876 | `		}else{` |
|     ! 0 | 2877 | `			ph7_result_null(pCtx);` |
|       - | 2878 | `		}` |
|       - | 2879 | `		/* Reset the cursor */` |
|       7 | 2880 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2881 | `	}` |
|       9 | 2882 | `	return PH7_OK;` |
|      11 | 2883 |  |
|       - | 2884 | `/*` |
|       - | 2885 | ` * int array_push($array,$var,...)` |
|       - | 2886 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2887 | ` * Parameters` |
|       - | 2888 | ` *  array` |
|       - | 2889 | ` *    The input array.` |
|       - | 2890 | ` *  var` |
|       - | 2891 | ` *   On or more value to push.` |
|       - | 2892 | ` * Return` |
|       - | 2893 | ` *  New array count (including old items).` |
|       - | 2894 | ` */` |
|      22 | 2895 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2896 |  |
|       - | 2897 | `	ph7_hashmap *pMap;` |
|       - | 2898 | `	sxi32 rc;` |
|       - | 2899 | `	int i;` |
|      24 | 2900 | `	if( nArg < 1 ){` |
|       4 | 2901 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2902 | `			"ArgumentCountError",` |
|       - | 2903 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2904 | `			nArg` |
|       - | 2905 | `			);` |
|       - | 2906 | `	}` |
|       - | 2907 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2908 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      22 | 2909 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2910 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2911 | `			"Error",` |
|       - | 2912 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2913 | `			);` |
|       - | 2914 | `	}` |
|       - | 2915 | `	/* Make sure we are dealing with a valid hashmap */` |
|      18 | 2916 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2917 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2918 | `			"TypeError",` |
|       - | 2919 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2920 | `			ph7_type_name(apArg[0])` |
|       - | 2921 | `			);` |
|       - | 2922 | `	}` |
|       - | 2923 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2924 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2925 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2926 | `	/* Start pushing given values */` |
|      31 | 2927 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      17 | 2928 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      17 | 2929 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2930 | `			break;` |
|       - | 2931 | `		}` |
|       9 | 2932 | `	}` |
|       - | 2933 | `	/* Return the new count */` |
|      15 | 2934 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 2935 | `	return PH7_OK;` |
|      13 | 2936 |  |
|       - | 2937 | `/*` |
|       - | 2938 | ` * value array_shift(array $array)` |
|       - | 2939 | ` *   Shift an element off the beginning of array.` |
|       - | 2940 | ` * Parameter` |
|       - | 2941 | ` *  The array to get the value from.` |
|       - | 2942 | ` * Return` |
|       - | 2943 | ` *  Shifted value or NULL on failure.` |
|       - | 2944 | ` */` |
|      38 | 2945 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2946 |  |
|       - | 2947 | `	ph7_hashmap *pMap;` |
|       - | 2948 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      40 | 2949 | `	if( nArg != 1 ){` |
|       7 | 2950 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2951 | `			"ArgumentCountError",` |
|       - | 2952 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2953 | `			nArg` |
|       - | 2954 | `			);` |
|       - | 2955 | `	}` |
|       - | 2956 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      36 | 2957 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2958 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2959 | `			"Error",` |
|       - | 2960 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2961 | `			);` |
|       - | 2962 | `	}` |
|       - | 2963 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 2964 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2965 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2966 | `			"TypeError",` |
|       - | 2967 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2968 | `			ph7_type_name(apArg[0])` |
|       - | 2969 | `			);` |
|       - | 2970 | `	}` |
|       - | 2971 | `	/* Point to the internal representation of the hashmap */` |
|      30 | 2972 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      30 | 2973 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      30 | 2974 | `	if( pMap->nEntry < 1 ){` |
|       - | 2975 | `		/* Empty hashmap,return NULL */` |
|       3 | 2976 | `		ph7_result_null(pCtx);` |
|       2 | 2977 | `	}else{` |
|      28 | 2978 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2979 | `		ph7_value *pObj;` |
|       - | 2980 | `		sxu32 n;` |
|      28 | 2981 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      28 | 2982 | `		if( pObj ){` |
|       - | 2983 | `			/* Node value */` |
|      28 | 2984 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2985 | `			/* Unlink the first node */` |
|      28 | 2986 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      15 | 2987 | `		}else{` |
|     ! 0 | 2988 | `			ph7_result_null(pCtx);` |
|       - | 2989 | `		}` |
|       - | 2990 | `		/* Rehash all int keys */` |
|      28 | 2991 | `		n = pMap->nEntry;` |
|      28 | 2992 | `		pEntry = pMap->pFirst;` |
|      28 | 2993 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 2994 | `		for(;;){` |
|      82 | 2995 | `			if( n < 1 ){` |
|      28 | 2996 | `				break;` |
|       - | 2997 | `			}` |
|      56 | 2998 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      56 | 2999 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 3000 | `			}` |
|       - | 3001 | `			/* Point to the next entry */` |
|      56 | 3002 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      56 | 3003 | `			n--;` |
|       2 | 3004 | `		}` |
|       - | 3005 | `		/* Reset the cursor */` |
|      28 | 3006 | `		pMap->pCur = pMap->pFirst;` |
|       - | 3007 | `	}` |
|      30 | 3008 | `	return PH7_OK;` |
|      21 | 3009 |  |
|       - | 3010 | `/*` |
|       - | 3011 | ` * Extract the node cursor value.` |
|       - | 3012 | ` */` |
|      24 | 3013 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 3014 |  |
|      25 | 3015 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 3016 | `	ph7_value *pVal;` |
|      25 | 3017 | `	if( pCur == 0 ){` |
|       - | 3018 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 3019 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3020 | `		return PH7_OK;` |
|       - | 3021 | `	}` |
|      25 | 3022 | `	if( iDirection != 0 ){` |
|       9 | 3023 | `		if( iDirection > 0 ){` |
|       - | 3024 | `			/* Point to the next entry */` |
|       7 | 3025 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 3026 | `			pCur = pMap->pCur;` |
|       4 | 3027 | `		}else{` |
|       - | 3028 | `			/* Point to the previous entry */` |
|       3 | 3029 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 3030 | `			pCur = pMap->pCur;` |
|       - | 3031 | `		}` |
|       9 | 3032 | `		if( pCur == 0 ){` |
|       - | 3033 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 3034 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3035 | `			return PH7_OK;` |
|       - | 3036 | `		}` |
|       4 | 3037 | `	}` |
|       - | 3038 | `	/* Point to the desired element */` |
|      25 | 3039 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 3040 | `	if( pVal ){` |
|      25 | 3041 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 3042 | `	}else{` |
|     ! 0 | 3043 | `		ph7_result_bool(pCtx,0);` |
|       - | 3044 | `	}` |
|      25 | 3045 | `	return PH7_OK;` |
|      13 | 3046 |  |
|       - | 3047 | `/*` |
|       - | 3048 | ` * value current(array $array)` |
|       - | 3049 | ` *  Return the current element in an array.` |
|       - | 3050 | ` * Parameter` |
|       - | 3051 | ` *  $input: The input array.` |
|       - | 3052 | ` * Return` |
|       - | 3053 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 3054 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3055 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3056 | ` *  is empty, current() returns FALSE.` |
|       - | 3057 | ` */` |
|      10 | 3058 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3059 |  |
|      11 | 3060 | `	if( nArg < 1 ){` |
|       - | 3061 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3062 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3063 | `		return PH7_OK;` |
|       - | 3064 | `	}` |
|       - | 3065 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 3066 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3067 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3068 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3069 | `		return PH7_OK;` |
|       - | 3070 | `	}` |
|      11 | 3071 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 3072 | `	return PH7_OK;` |
|       6 | 3073 |  |
|       - | 3074 | `/*` |
|       - | 3075 | ` * value next(array $input)` |
|       - | 3076 | ` *  Advance the internal array pointer of an array.` |
|       - | 3077 | ` * Parameter` |
|       - | 3078 | ` *  $input: The input array.` |
|       - | 3079 | ` * Return` |
|       - | 3080 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 3081 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 3082 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 3083 | ` */` |
|       6 | 3084 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3085 |  |
|       7 | 3086 | `	if( nArg < 1 ){` |
|       - | 3087 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3088 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3089 | `		return PH7_OK;` |
|       - | 3090 | `	}` |
|       - | 3091 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 3092 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3093 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3094 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3095 | `		return PH7_OK;` |
|       - | 3096 | `	}` |
|       7 | 3097 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 3098 | `	return PH7_OK;` |
|       4 | 3099 |  |
|       - | 3100 | `/*` |
|       - | 3101 | ` * value prev(array $input)` |
|       - | 3102 | ` *  Rewind the internal array pointer.` |
|       - | 3103 | ` * Parameter` |
|       - | 3104 | ` *  $input: The input array.` |
|       - | 3105 | ` * Return` |
|       - | 3106 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3107 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3108 | ` *  elements.` |
|       - | 3109 | ` */` |
|       2 | 3110 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3111 |  |
|       3 | 3112 | `	if( nArg < 1 ){` |
|       - | 3113 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3114 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3115 | `		return PH7_OK;` |
|       - | 3116 | `	}` |
|       - | 3117 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3118 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3119 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3120 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3121 | `		return PH7_OK;` |
|       - | 3122 | `	}` |
|       3 | 3123 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3124 | `	return PH7_OK;` |
|       2 | 3125 |  |
|       - | 3126 | `/*` |
|       - | 3127 | ` * value end(array $input)` |
|       - | 3128 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3129 | ` * Parameter` |
|       - | 3130 | ` *  $input: The input array.` |
|       - | 3131 | ` * Return` |
|       - | 3132 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3133 | ` */` |
|       2 | 3134 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3135 |  |
|       - | 3136 | `	ph7_hashmap *pMap;` |
|       3 | 3137 | `	if( nArg < 1 ){` |
|       - | 3138 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3139 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3140 | `		return PH7_OK;` |
|       - | 3141 | `	}` |
|       - | 3142 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3143 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3144 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3145 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3146 | `		return PH7_OK;` |
|       - | 3147 | `	}` |
|       - | 3148 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3149 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3150 | `	/* Point to the last node */` |
|       3 | 3151 | `	pMap->pCur = pMap->pLast;` |
|       - | 3152 | `	/* Return the last node value */` |
|       3 | 3153 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3154 | `	return PH7_OK;` |
|       2 | 3155 |  |
|       - | 3156 | `/*` |
|       - | 3157 | ` * value reset(array $array )` |
|       - | 3158 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3159 | ` * Parameter` |
|       - | 3160 | ` *  $input: The input array.` |
|       - | 3161 | ` * Return` |
|       - | 3162 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3163 | ` */` |
|       4 | 3164 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3165 |  |
|       - | 3166 | `	ph7_hashmap *pMap;` |
|       5 | 3167 | `	if( nArg < 1 ){` |
|       - | 3168 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3169 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3170 | `		return PH7_OK;` |
|       - | 3171 | `	}` |
|       - | 3172 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3173 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3174 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3175 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3176 | `		return PH7_OK;` |
|       - | 3177 | `	}` |
|       - | 3178 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3179 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3180 | `	/* Point to the first node */` |
|       5 | 3181 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3182 | `	/* Return the last node value if available */` |
|       5 | 3183 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3184 | `	return PH7_OK;` |
|       3 | 3185 |  |
|       - | 3186 | `/*` |
|       - | 3187 | ` * value key(array $array)` |
|       - | 3188 | ` *   Fetch a key from an array` |
|       - | 3189 | ` * Parameter` |
|       - | 3190 | ` *  $input` |
|       - | 3191 | ` *   The input array.` |
|       - | 3192 | ` * Return` |
|       - | 3193 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3194 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3195 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3196 | ` *  is empty, key() returns NULL.` |
|       - | 3197 | ` */` |
|       4 | 3198 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3199 |  |
|       - | 3200 | `	ph7_hashmap_node *pCur;` |
|       - | 3201 | `	ph7_hashmap *pMap;` |
|       5 | 3202 | `	if( nArg < 1 ){` |
|       - | 3203 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3204 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3205 | `		return PH7_OK;` |
|       - | 3206 | `	}` |
|       - | 3207 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3208 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3209 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3210 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3211 | `		return PH7_OK;` |
|       - | 3212 | `	}` |
|       5 | 3213 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3214 | `	pCur = pMap->pCur;` |
|       5 | 3215 | `	if( pCur == 0 ){` |
|       - | 3216 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3217 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3218 | `		return PH7_OK;` |
|       - | 3219 | `	}` |
|       5 | 3220 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3221 | `		/* Key is integer */` |
|     ! 0 | 3222 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3223 | `	}else{` |
|       - | 3224 | `		/* Key is blob */` |
|       7 | 3225 | `		ph7_result_string(pCtx,` |
|       4 | 3226 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3227 | `	}` |
|       5 | 3228 | `	return PH7_OK;` |
|       3 | 3229 |  |
|       - | 3230 | `/*` |
|       - | 3231 | ` * array each(array $input)` |
|       - | 3232 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3233 | ` * Parameter` |
|       - | 3234 | ` *  $input` |
|       - | 3235 | ` *    The input array.` |
|       - | 3236 | ` * Return` |
|       - | 3237 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3238 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3239 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3240 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3241 | ` *  each() returns FALSE.` |
|       - | 3242 | ` */` |
|      22 | 3243 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3244 |  |
|       - | 3245 | `	ph7_hashmap_node *pCur;` |
|       - | 3246 | `	ph7_hashmap *pMap;` |
|       - | 3247 | `	ph7_value *pArray;` |
|       - | 3248 | `	ph7_value *pVal;` |
|       - | 3249 | `	ph7_value sKey;` |
|      23 | 3250 | `	if( nArg < 1 ){` |
|       - | 3251 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3252 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3253 | `		return PH7_OK;` |
|       - | 3254 | `	}` |
|       - | 3255 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3256 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3257 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3258 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3259 | `		return PH7_OK;` |
|       - | 3260 | `	}` |
|       - | 3261 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3262 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3263 | `	if( pMap->pCur == 0 ){` |
|       - | 3264 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3265 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3266 | `		return PH7_OK;` |
|       - | 3267 | `	}` |
|      15 | 3268 | `	pCur = pMap->pCur;` |
|       - | 3269 | `	/* Create a new array */` |
|      15 | 3270 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3271 | `	if( pArray == 0 ){` |
|     ! 0 | 3272 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3273 | `		return PH7_OK;` |
|       - | 3274 | `	}` |
|      15 | 3275 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3276 | `	/* Insert the current value */` |
|      15 | 3277 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3278 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3279 | `	/* Make the key */` |
|      15 | 3280 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3281 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3282 | `	}else{` |
|       9 | 3283 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3284 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3285 | `	}` |
|       - | 3286 | `	/* Insert the current key */` |
|      15 | 3287 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3288 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3289 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3290 | `	/* Advance the cursor */` |
|      15 | 3291 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3292 | `	/* Return the current entry */` |
|      15 | 3293 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3294 | `	return PH7_OK;` |
|      12 | 3295 |  |
|       - | 3296 | `/*` |
|       - | 3297 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3298 | ` *  Create an array containing a range of elements` |
|       - | 3299 | ` * Parameter` |
|       - | 3300 | ` *  start` |
|       - | 3301 | ` *   First value of the sequence.` |
|       - | 3302 | ` *  limit` |
|       - | 3303 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3304 | ` *  step` |
|       - | 3305 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3306 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3307 | ` * Return` |
|       - | 3308 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3309 | ` * NOTE:` |
|       - | 3310 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3311 | ` */` |
|       2 | 3312 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3313 |  |
|       - | 3314 | `	ph7_value *pValue,*pArray;` |
|       - | 3315 | `	sxi64 iOfft,iLimit;` |
|       3 | 3316 | `	int iStep = 1;` |
|       - | 3317 |  |
|       3 | 3318 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3319 | `	if( nArg > 0 ){` |
|       - | 3320 | `		/* Extract the offset */` |
|       3 | 3321 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3322 | `		if( nArg > 1 ){` |
|       - | 3323 | `			/* Extract the limit */` |
|       3 | 3324 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3325 | `			if( nArg > 2 ){` |
|       - | 3326 | `				/* Extract the increment */` |
|       3 | 3327 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3328 | `				if( iStep < 1 ){` |
|       - | 3329 | `					/* Only positive number are allowed */` |
|       3 | 3330 | `					iStep = 1;` |
|       1 | 3331 | `				}` |
|       1 | 3332 | `			}` |
|       1 | 3333 | `		}` |
|       1 | 3334 | `	}` |
|       - | 3335 | `	/* Element container */` |
|       3 | 3336 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3337 | `	/* Create the new array */` |
|       3 | 3338 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3339 | `	if( pArray == 0 ){` |
|     ! 0 | 3340 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3341 | `		return PH7_OK;` |
|       - | 3342 | `	}` |
|       - | 3343 | `	/* Start filling */` |
|       3 | 3344 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3345 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3346 | `		/* Perform the insertion */` |
|     ! 0 | 3347 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3348 | `		/* Increment */` |
|     ! 0 | 3349 | `		iOfft += iStep;` |
|     ! 0 | 3350 | `	}` |
|       - | 3351 | `	/* Return the new array */` |
|       3 | 3352 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3353 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3354 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3355 | `	 */` |
|       3 | 3356 | `	return PH7_OK;` |
|       2 | 3357 |  |
|       - | 3358 | `/*` |
|       - | 3359 | ` * array array_values(array $array)` |
|       - | 3360 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3361 | ` * Parameters` |
|       - | 3362 | ` *  $array` |
|       - | 3363 | ` *   The input array.` |
|       - | 3364 | ` * Return` |
|       - | 3365 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3366 | ` */` |
|      30 | 3367 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3368 |  |
|       - | 3369 | `	ph7_hashmap_node *pNode;` |
|       - | 3370 | `	ph7_hashmap *pMap;` |
|       - | 3371 | `	ph7_value *pArray;` |
|       - | 3372 | `	ph7_value *pObj;` |
|       - | 3373 | `	sxu32 n;` |
|      32 | 3374 | `	if( nArg != 1 ){` |
|       - | 3375 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3376 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3377 | `			"ArgumentCountError",` |
|       - | 3378 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3379 | `			nArg` |
|       - | 3380 | `			);` |
|       - | 3381 | `	}` |
|       - | 3382 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3383 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3384 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3385 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3386 | `			"TypeError",` |
|       - | 3387 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3388 | `			ph7_type_name(apArg[0])` |
|       - | 3389 | `			);` |
|       - | 3390 | `	}` |
|       - | 3391 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3392 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3393 | `	/* Create a new array */` |
|      25 | 3394 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3395 | `	if( pArray == 0 ){` |
|     ! 0 | 3396 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3397 | `		return PH7_OK;` |
|       - | 3398 | `	}` |
|       - | 3399 | `	/* Perform the requested operation */` |
|      25 | 3400 | `	pNode = pMap->pFirst;` |
|      83 | 3401 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3402 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3403 | `		if( pObj ){` |
|       - | 3404 | `			/* perform the insertion */` |
|      59 | 3405 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3406 | `		}` |
|       - | 3407 | `		/* Point to the next entry */` |
|      59 | 3408 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3409 | `	}` |
|       - | 3410 | `	/* return the new array */` |
|      25 | 3411 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3412 | `	return PH7_OK;` |
|      17 | 3413 |  |
|       - | 3414 | `/*` |
|       - | 3415 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3416 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3417 | ` * Parameters` |
|       - | 3418 | ` *  $input` |
|       - | 3419 | ` *   An array containing keys to return.` |
|       - | 3420 | ` * $search_value` |
|       - | 3421 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3422 | ` * $strict` |
|       - | 3423 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3424 | ` * Return` |
|       - | 3425 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3426 | ` */` |
|     122 | 3427 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3428 |  |
|       - | 3429 | `	ph7_hashmap_node *pNode;` |
|       - | 3430 | `	ph7_hashmap *pMap;` |
|       - | 3431 | `	ph7_value *pArray;` |
|       - | 3432 | `	ph7_value sObj;` |
|       - | 3433 | `	ph7_value sVal;` |
|       - | 3434 | `	SyString sKey;` |
|       - | 3435 | `	int bStrict;` |
|       - | 3436 | `	sxi32 rc;` |
|       - | 3437 | `	sxu32 n;` |
|     124 | 3438 | `	if( nArg < 1 ){` |
|       - | 3439 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3440 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3441 | `			"ArgumentCountError",` |
|       - | 3442 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3443 | `			);` |
|       - | 3444 | `	}` |
|       - | 3445 | `	/* Make sure we are dealing with a valid hashmap */` |
|     122 | 3446 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3447 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3448 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3449 | `			"TypeError",` |
|       - | 3450 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3451 | `			ph7_type_name(apArg[0])` |
|       - | 3452 | `			);` |
|       - | 3453 | `	}` |
|       - | 3454 | `	/* Point to the internal representation of the input hashmap */` |
|     120 | 3455 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3456 | `	/* Create a new array */` |
|     120 | 3457 | `	pArray = ph7_context_new_array(pCtx);` |
|     120 | 3458 | `	if( pArray == 0 ){` |
|     ! 0 | 3459 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3460 | `		return PH7_OK;` |
|       - | 3461 | `	}` |
|     120 | 3462 | `	bStrict = FALSE;` |
|     120 | 3463 | `	if( nArg > 2 ){` |
|       - | 3464 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3465 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3466 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3467 | `				"TypeError",` |
|       - | 3468 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3469 | `				ph7_type_name(apArg[2])` |
|       - | 3470 | `				);` |
|       - | 3471 | `		}` |
|       5 | 3472 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3473 | `	}` |
|       - | 3474 | `	/* Perform the requested operation */` |
|     117 | 3475 | `	pNode = pMap->pFirst;` |
|     117 | 3476 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     559 | 3477 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     443 | 3478 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     121 | 3479 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      61 | 3480 | `		}else{` |
|     323 | 3481 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3482 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3483 | `		}` |
|     443 | 3484 | `		rc = 0;` |
|     443 | 3485 | `		if( nArg > 1 ){` |
|      31 | 3486 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3487 | `			if( pValue ){` |
|      31 | 3488 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3489 | `				/* Filter key */` |
|      31 | 3490 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3491 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3492 | `			}` |
|      15 | 3493 | `		}` |
|     443 | 3494 | `		if( rc == 0 ){` |
|       - | 3495 | `			/* Perform the insertion */` |
|     425 | 3496 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     212 | 3497 | `		}` |
|     443 | 3498 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3499 | `		/* Point to the next entry */` |
|     443 | 3500 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     222 | 3501 | `	}` |
|       - | 3502 | `	/* return the new array */` |
|     117 | 3503 | `	ph7_result_value(pCtx,pArray);` |
|     117 | 3504 | `	return PH7_OK;` |
|      63 | 3505 |  |
|       - | 3506 | `/*` |
|       - | 3507 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3508 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3509 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3510 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3511 | ` * Parameters` |
|       - | 3512 | ` *  $arr1` |
|       - | 3513 | ` *   First array` |
|       - | 3514 | ` *  $arr2` |
|       - | 3515 | ` *   Second array` |
|       - | 3516 | ` * Return` |
|       - | 3517 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3518 | ` * Note` |
|       - | 3519 | ` *  This function is a symisc eXtension.` |
|       - | 3520 | ` */` |
|       4 | 3521 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3522 |  |
|       - | 3523 | `	ph7_hashmap *p1,*p2;` |
|       - | 3524 | `	int rc;` |
|       5 | 3525 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3526 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3527 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3528 | `		return PH7_OK;` |
|       - | 3529 | `	}` |
|       - | 3530 | `	/* Point to the hashmaps */` |
|       5 | 3531 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3532 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3533 | `	rc = (p1 == p2);` |
|       - | 3534 | `	/* Same instance? */` |
|       5 | 3535 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3536 | `	return PH7_OK;` |
|       3 | 3537 |  |
|       - | 3538 | `/*` |
|       - | 3539 | ` * array array_merge(array ...$arrays)` |
|       - | 3540 | ` *  Merge one or more arrays.` |
|       - | 3541 | ` * Parameters` |
|       - | 3542 | ` *  ...$arrays` |
|       - | 3543 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3544 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3545 | ` * Return` |
|       - | 3546 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3547 | ` *  with no arguments.` |
|       - | 3548 | ` */` |
|     966 | 3549 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3550 |  |
|       - | 3551 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3552 | `	ph7_value *pArray;` |
|       - | 3553 | `	int i;` |
|       - | 3554 | `	/* Create a new array */` |
|     968 | 3555 | `	pArray = ph7_context_new_array(pCtx);` |
|     968 | 3556 | `	if( pArray == 0 ){` |
|     ! 0 | 3557 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3558 | `		return PH7_OK;` |
|       - | 3559 | `	}` |
|       - | 3560 | `	/* Point to the internal representation of the hashmap */` |
|     968 | 3561 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3562 | `	/* Start merging */` |
|    2890 | 3563 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3564 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1928 | 3565 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3566 | `			/* Type mismatch -> TypeError */` |
|       7 | 3567 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3568 | `				"TypeError",` |
|       - | 3569 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3570 | `				i + 1,` |
|       4 | 3571 | `				ph7_type_name(apArg[i])` |
|       - | 3572 | `				);` |
|     ! 0 | 3573 | `		}else{` |
|    1924 | 3574 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3575 | `			/* Merge the two hashmaps */` |
|    1924 | 3576 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3577 | `		}` |
|     963 | 3578 | `	}` |
|       - | 3579 | `	/* Return the freshly created array */` |
|     964 | 3580 | `	ph7_result_value(pCtx,pArray);` |
|     964 | 3581 | `	return PH7_OK;` |
|     485 | 3582 |  |
|       - | 3583 | `/*` |
|       - | 3584 | ` * array array_copy(array $source)` |
|       - | 3585 | ` *  Make a blind copy of the target array.` |
|       - | 3586 | ` * Parameters` |
|       - | 3587 | ` *  $source` |
|       - | 3588 | ` *   Target array` |
|       - | 3589 | ` * Return` |
|       - | 3590 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3591 | ` * Note` |
|       - | 3592 | ` *  This function is a symisc eXtension.` |
|       - | 3593 | ` */` |
|      16 | 3594 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3595 |  |
|       - | 3596 | `	ph7_hashmap *pMap;` |
|       - | 3597 | `	ph7_value *pArray;` |
|      17 | 3598 | `	if( nArg < 1 ){` |
|       - | 3599 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3600 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3601 | `		return PH7_OK;` |
|       - | 3602 | `	}` |
|       - | 3603 | `	/* Create a new array */` |
|      17 | 3604 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3605 | `	if( pArray == 0 ){` |
|     ! 0 | 3606 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3607 | `		return PH7_OK;` |
|       - | 3608 | `	}` |
|       - | 3609 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3610 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3611 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3612 | `		/* Point to the internal representation of the source */` |
|      17 | 3613 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3614 | `		/* Perform the copy */` |
|      17 | 3615 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3616 | `	}else{` |
|       - | 3617 | `		/* Simple insertion */` |
|     ! 0 | 3618 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3619 | `	}` |
|       - | 3620 | `	/* Return the duplicated array */` |
|      17 | 3621 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3622 | `	return PH7_OK;` |
|       9 | 3623 |  |
|       - | 3624 | `/*` |
|       - | 3625 | ` * bool array_erase(array $source)` |
|       - | 3626 | ` *  Remove all elements from a given array.` |
|       - | 3627 | ` * Parameters` |
|       - | 3628 | ` *  $source` |
|       - | 3629 | ` *   Target array` |
|       - | 3630 | ` * Return` |
|       - | 3631 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3632 | ` * Note` |
|       - | 3633 | ` *  This function is a symisc eXtension.` |
|       - | 3634 | ` */` |
|      16 | 3635 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3636 |  |
|       - | 3637 | `	ph7_hashmap *pMap;` |
|      17 | 3638 | `	if( nArg < 1 ){` |
|       - | 3639 | `		/* Missing arguments */` |
|     ! 0 | 3640 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3641 | `		return PH7_OK;` |
|       - | 3642 | `	}` |
|       - | 3643 | `	/* Point to the target hashmap */` |
|      17 | 3644 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3645 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3646 | `	/* Erase */` |
|      17 | 3647 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3648 | `	return PH7_OK;` |
|       9 | 3649 |  |
|       - | 3650 | `/*` |
|       - | 3651 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3652 | ` *  Extract a slice of the array.` |
|       - | 3653 | ` * Parameters` |
|       - | 3654 | ` *  $array` |
|       - | 3655 | ` *    The input array.` |
|       - | 3656 | ` * $offset` |
|       - | 3657 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3658 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3659 | ` * $length (optional, nullable)` |
|       - | 3660 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3661 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3662 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3663 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3664 | ` * $preserve_keys (optional)` |
|       - | 3665 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3666 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3667 | ` * Return` |
|       - | 3668 | ` *   The new slice.` |
|       - | 3669 | ` */` |
|      46 | 3670 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3671 |  |
|       - | 3672 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3673 | `	ph7_hashmap_node *pCur;` |
|       - | 3674 | `	ph7_value *pArray;` |
|       - | 3675 | `	int iLength,iOfft;` |
|       - | 3676 | `	int bPreserve;` |
|       - | 3677 | `	sxi32 rc;` |
|      48 | 3678 | `	if( nArg < 2 ){` |
|       7 | 3679 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3680 | `			"ArgumentCountError",` |
|       - | 3681 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3682 | `			nArg` |
|       - | 3683 | `			);` |
|       - | 3684 | `	}` |
|      44 | 3685 | `	if( nArg > 4 ){` |
|       4 | 3686 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3687 | `			"ArgumentCountError",` |
|       - | 3688 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3689 | `			nArg` |
|       - | 3690 | `			);` |
|       - | 3691 | `	}` |
|      42 | 3692 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3693 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3694 | `			"TypeError",` |
|       - | 3695 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3696 | `			ph7_type_name(apArg[0])` |
|       - | 3697 | `			);` |
|       - | 3698 | `	}` |
|       - | 3699 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3700 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3701 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3702 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3703 | `			"TypeError",` |
|       - | 3704 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3705 | `			ph7_type_name(apArg[1])` |
|       - | 3706 | `			);` |
|       - | 3707 | `	}` |
|       - | 3708 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3709 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3710 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3711 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3712 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3713 | `				"TypeError",` |
|       - | 3714 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3715 | `				ph7_type_name(apArg[2])` |
|       - | 3716 | `				);` |
|       - | 3717 | `		}` |
|       8 | 3718 | `	}` |
|       - | 3719 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3720 | `	if( nArg > 3 ){` |
|      10 | 3721 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3722 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3723 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3724 | `				"TypeError",` |
|       - | 3725 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3726 | `				ph7_type_name(apArg[3])` |
|       - | 3727 | `				);` |
|       - | 3728 | `		}` |
|       2 | 3729 | `	}` |
|       - | 3730 | `	/* Point the internal representation of the target array */` |
|      33 | 3731 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3732 | `	bPreserve = FALSE;` |
|       - | 3733 | `	/* Get the offset */` |
|      33 | 3734 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3735 | `	if( iOfft < 0 ){` |
|       5 | 3736 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3737 | `		if( iOfft < 0 ){` |
|       3 | 3738 | `			iOfft = 0;` |
|       1 | 3739 | `		}` |
|       2 | 3740 | `	}` |
|      33 | 3741 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3742 | `		/* Offset past end of array, return empty array */` |
|       5 | 3743 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3744 | `		if( pArray == 0 ){` |
|     ! 0 | 3745 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3746 | `			return PH7_OK;` |
|       - | 3747 | `		}` |
|       5 | 3748 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3749 | `		return PH7_OK;` |
|       - | 3750 | `	}` |
|       - | 3751 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3752 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3753 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3754 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3755 | `		if( iLength < 0 ){` |
|       5 | 3756 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3757 | `		}` |
|      15 | 3758 | `		if( iLength < 0 ){` |
|       3 | 3759 | `			iLength = 0;` |
|       1 | 3760 | `		}` |
|      15 | 3761 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3762 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3763 | `		}` |
|       7 | 3764 | `	}` |
|      29 | 3765 | `	if( nArg > 3 ){` |
|       5 | 3766 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3767 | `	}` |
|       - | 3768 | `	/* Create a new array */` |
|      29 | 3769 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3770 | `	if( pArray == 0 ){` |
|     ! 0 | 3771 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3772 | `		return PH7_OK;` |
|       - | 3773 | `	}` |
|      29 | 3774 | `	if( iLength < 1 ){` |
|       - | 3775 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3776 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3777 | `		return PH7_OK;` |
|       - | 3778 | `	}` |
|       - | 3779 | `	/* Point to the desired entry */` |
|      25 | 3780 | `	pCur = pSrc->pFirst;` |
|      24 | 3781 | `	for(;;){` |
|      49 | 3782 | `		if( iOfft < 1 ){` |
|      25 | 3783 | `			break;` |
|       - | 3784 | `		}` |
|       - | 3785 | `		/* Point to the next entry */` |
|      25 | 3786 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3787 | `		iOfft--;` |
|       1 | 3788 | `	}` |
|       - | 3789 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3790 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3791 | `	for(;;){` |
|      79 | 3792 | `		if( iLength < 1 ){` |
|      25 | 3793 | `			break;` |
|       - | 3794 | `		}` |
|       - | 3795 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3796 | `		{` |
|      55 | 3797 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3798 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3799 | `		}` |
|      55 | 3800 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3801 | `			break;` |
|       - | 3802 | `		}` |
|       - | 3803 | `		/* Point to the next entry */` |
|      55 | 3804 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3805 | `		iLength--;` |
|       1 | 3806 | `	}` |
|       - | 3807 | `	/* Return the freshly created array */` |
|      25 | 3808 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3809 | `	return PH7_OK;` |
|      25 | 3810 |  |
|       - | 3811 | `/*` |
|       - | 3812 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3813 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3814 | ` * beginning (becomes the new pFirst).` |
|       - | 3815 | ` */` |
|      30 | 3816 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3817 |  |
|       - | 3818 | `	ph7_hashmap_node *pNode;` |
|       - | 3819 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3820 | `	pNode = pMap->pLast;` |
|      31 | 3821 | `	if( pNode == 0 ){` |
|     ! 0 | 3822 | `		return;` |
|       - | 3823 | `	}` |
|      31 | 3824 | `	if( pNode->pNext == 0 ){` |
|       - | 3825 | `		/* Only node in the list, nothing to move */` |
|       5 | 3826 | `		return;` |
|       - | 3827 | `	}` |
|      27 | 3828 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3829 | `		/* Already in the correct position */` |
|       9 | 3830 | `		return;` |
|       - | 3831 | `	}` |
|       - | 3832 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3833 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3834 | `	pMap->pLast->pPrev = 0;` |
|       - | 3835 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3836 | `	if( pAfter == 0 ){` |
|       - | 3837 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3838 | `		pNode->pNext = 0;` |
|       3 | 3839 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3840 | `		if( pMap->pFirst ){` |
|       3 | 3841 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3842 | `		}` |
|       3 | 3843 | `		pMap->pFirst = pNode;` |
|       2 | 3844 | `	}else{` |
|      17 | 3845 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3846 | `		pNode->pPrev = pOldNext;` |
|      17 | 3847 | `		pNode->pNext = pAfter;` |
|      17 | 3848 | `		pAfter->pPrev = pNode;` |
|      17 | 3849 | `		if( pOldNext ){` |
|      17 | 3850 | `			pOldNext->pNext = pNode;` |
|       9 | 3851 | `		}else{` |
|     ! 0 | 3852 | `			pMap->pLast = pNode;` |
|       - | 3853 | `		}` |
|       - | 3854 | `	}` |
|      16 | 3855 |  |
|       - | 3856 | `/*` |
|       - | 3857 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3858 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3859 | ` * Parameters` |
|       - | 3860 | ` *  $array` |
|       - | 3861 | ` *    The input array.` |
|       - | 3862 | ` *  $offset` |
|       - | 3863 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3864 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3865 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3866 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3867 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3868 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3869 | ` *  $length (optional)` |
|       - | 3870 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3871 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3872 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3873 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3874 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3875 | ` *  $replacement (optional)` |
|       - | 3876 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3877 | ` *    with elements from this array.` |
|       - | 3878 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3879 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3880 | ` *    offset.` |
|       - | 3881 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3882 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3883 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3884 | ` * Return` |
|       - | 3885 | ` *   A new array consisting of the extracted elements.` |
|       - | 3886 | ` */` |
|      54 | 3887 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3888 |  |
|       - | 3889 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3890 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3891 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3892 | `	int iLength,iOfft,i;` |
|       - | 3893 | `	sxi32 rc;` |
|      56 | 3894 | `	if( nArg < 2 ){` |
|       7 | 3895 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3896 | `			"ArgumentCountError",` |
|       - | 3897 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3898 | `			nArg` |
|       - | 3899 | `			);` |
|       - | 3900 | `	}` |
|      52 | 3901 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3902 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3903 | `			"TypeError",` |
|       - | 3904 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3905 | `			ph7_type_name(apArg[0])` |
|       - | 3906 | `			);` |
|       - | 3907 | `	}` |
|       - | 3908 | `	/* Point to the internal representation of the target array */` |
|      49 | 3909 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3910 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3911 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3912 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3913 | `	if( iOfft < 0 ){` |
|       7 | 3914 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3915 | `		if( iOfft < 0 ){` |
|       3 | 3916 | `			iOfft = 0;` |
|       2 | 3917 | `		}` |
|      46 | 3918 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3919 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3920 | `	}` |
|       - | 3921 | `	/* Get the length and clamp to valid range.` |
|       - | 3922 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3923 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3924 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3925 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3926 | `		if( iLength < 0 ){` |
|       7 | 3927 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3928 | `			if( iLength < 0 ){` |
|       3 | 3929 | `				iLength = 0;` |
|       1 | 3930 | `			}` |
|       3 | 3931 | `		}` |
|      31 | 3932 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3933 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3934 | `		}` |
|      15 | 3935 | `	}` |
|       - | 3936 | `	/* Create the result array for removed elements */` |
|      49 | 3937 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3938 | `	if( pArray == 0 ){` |
|     ! 0 | 3939 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3940 | `		return PH7_OK;` |
|       - | 3941 | `	}` |
|       - | 3942 | `	/* Get replacement array if provided */` |
|      49 | 3943 | `	pRep = 0;` |
|      49 | 3944 | `	if( nArg > 3 ){` |
|      21 | 3945 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3946 | `			/* Perform an array cast */` |
|       3 | 3947 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3948 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3949 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3950 | `			}` |
|       2 | 3951 | `		}else{` |
|      19 | 3952 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3953 | `		}` |
|      21 | 3954 | `		if( pRep ){` |
|       - | 3955 | `			/* Reset the loop cursor */` |
|      21 | 3956 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3957 | `		}` |
|      10 | 3958 | `	}` |
|       - | 3959 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3960 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3961 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3962 | `		return PH7_OK;` |
|       - | 3963 | `	}` |
|       - | 3964 | `	/* Navigate to the offset position */` |
|      41 | 3965 | `	pCur = pSrc->pFirst;` |
|      85 | 3966 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3967 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3968 | `	}` |
|       - | 3969 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3970 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3971 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3972 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3973 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3974 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3975 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3976 | `		pPrev = pCur->pPrev;` |
|      71 | 3977 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3978 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3979 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3980 | `			break;` |
|       - | 3981 | `		}` |
|      71 | 3982 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3983 | `	}` |
|       - | 3984 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3985 | `	if( pRep ){` |
|       - | 3986 | `		ph7_value sSafeVal;` |
|      61 | 3987 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3988 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3989 | `			if( pRvalue ){` |
|       - | 3990 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3991 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3992 | `				 * since it points into that same pool. */` |
|      31 | 3993 | `				sSafeVal = *pRvalue;` |
|      31 | 3994 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 3995 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 3996 | `					pNewNode = pSrc->pLast;` |
|      31 | 3997 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 3998 | `					pInsertAfter = pNewNode;` |
|      15 | 3999 | `				}` |
|      15 | 4000 | `			}` |
|       1 | 4001 | `		}` |
|      10 | 4002 | `	}` |
|       - | 4003 | `	/* Return the freshly created array */` |
|      41 | 4004 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 4005 | `	return PH7_OK;` |
|      29 | 4006 |  |
|       - | 4007 | `/*` |
|       - | 4008 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 4009 | ` *  Checks if a value exists in an array.` |
|       - | 4010 | ` * Parameters` |
|       - | 4011 | ` *  $needle` |
|       - | 4012 | ` *   The searched value.` |
|       - | 4013 | ` *   Note:` |
|       - | 4014 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 4015 | ` * $haystack` |
|       - | 4016 | ` *  The target array.` |
|       - | 4017 | ` * $strict` |
|       - | 4018 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 4019 | ` *  will also check the types of the needle in the haystack.` |
|       - | 4020 | ` */` |
|   29144 | 4021 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4022 |  |
|       - | 4023 | `	ph7_value *pNeedle;` |
|       - | 4024 | `	int bStrict;` |
|       - | 4025 | `	int rc;` |
|   29146 | 4026 | `	if( nArg < 2 ){` |
|       - | 4027 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 4028 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4029 | `		return PH7_OK;` |
|       - | 4030 | `	}` |
|   29146 | 4031 | `	pNeedle = apArg[0];` |
|   29146 | 4032 | `	bStrict = 0;` |
|   29146 | 4033 | `	if( nArg > 2 ){` |
|       5 | 4034 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 4035 | `	}` |
|   29146 | 4036 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4037 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 4038 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 4039 | `		/* Set the comparison result */` |
|     ! 0 | 4040 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 4041 | `		return PH7_OK;` |
|       - | 4042 | `	}` |
|       - | 4043 | `	/* Perform the lookup */` |
|   29146 | 4044 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 4045 | `	/* Lookup result */` |
|   29146 | 4046 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   29146 | 4047 | `	return PH7_OK;` |
|   14574 | 4048 |  |
|       - | 4049 | `/*` |
|       - | 4050 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 4051 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 4052 | ` * Parameters` |
|       - | 4053 | ` * $needle` |
|       - | 4054 | ` *   The searched value.` |
|       - | 4055 | ` * $haystack` |
|       - | 4056 | ` *   The array.` |
|       - | 4057 | ` * $strict` |
|       - | 4058 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 4059 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 4060 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 4061 | ` * Return` |
|       - | 4062 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 4063 | ` */` |
|      28 | 4064 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4065 |  |
|       - | 4066 | `	ph7_hashmap_node *pEntry;` |
|       - | 4067 | `	ph7_value *pVal,sNeedle;` |
|       - | 4068 | `	ph7_hashmap *pMap;` |
|       - | 4069 | `	ph7_value sVal;` |
|       - | 4070 | `	int bStrict;` |
|       - | 4071 | `	sxu32 n;` |
|       - | 4072 | `	int rc;` |
|      30 | 4073 | `	if( nArg < 2 ){` |
|       - | 4074 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 4075 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4076 | `			"ArgumentCountError",` |
|       - | 4077 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4078 | `			nArg` |
|       - | 4079 | `			);` |
|       - | 4080 | `	}` |
|      26 | 4081 | `	bStrict = FALSE;` |
|      26 | 4082 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4083 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4084 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4085 | `			"TypeError",` |
|       - | 4086 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4087 | `			ph7_type_name(apArg[1])` |
|       - | 4088 | `			);` |
|       - | 4089 | `	}` |
|      24 | 4090 | `	if( nArg > 2 ){` |
|       - | 4091 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4092 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4093 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4094 | `				"TypeError",` |
|       - | 4095 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4096 | `				ph7_type_name(apArg[2])` |
|       - | 4097 | `				);` |
|       - | 4098 | `		}` |
|       9 | 4099 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4100 | `	}` |
|       - | 4101 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4102 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4103 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4104 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4105 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4106 | `	pEntry = pMap->pFirst;` |
|      21 | 4107 | `	n = pMap->nEntry;` |
|      23 | 4108 | `	for(;;){` |
|      47 | 4109 | `		if( !n ){` |
|       9 | 4110 | `			break;` |
|       - | 4111 | `		}` |
|       - | 4112 | `		/* Extract node value */` |
|      39 | 4113 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4114 | `		if( pVal ){` |
|       - | 4115 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4116 | `			 * can change their type.` |
|       - | 4117 | `			 */` |
|      39 | 4118 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4119 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4120 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4121 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4122 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4123 | `			if( rc == 0 ){` |
|       - | 4124 | `				/* Match found,return key */` |
|      13 | 4125 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4126 | `					/* INT key */` |
|       7 | 4127 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4128 | `				}else{` |
|       7 | 4129 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4130 | `					/* Blob key */` |
|       7 | 4131 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4132 | `				}` |
|      13 | 4133 | `				return PH7_OK;` |
|       - | 4134 | `			}` |
|      13 | 4135 | `		}` |
|       - | 4136 | `		/* Point to the next entry */` |
|      27 | 4137 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4138 | `		n--;` |
|       1 | 4139 | `	}` |
|       - | 4140 | `	/* No such value,return FALSE */` |
|       9 | 4141 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4142 | `	return PH7_OK;` |
|      16 | 4143 |  |
|       - | 4144 | `/*` |
|       - | 4145 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4146 | ` *  Computes the difference of arrays.` |
|       - | 4147 | ` * Parameters` |
|       - | 4148 | ` *  $array1` |
|       - | 4149 | ` *    The array to compare from` |
|       - | 4150 | ` *  $array2` |
|       - | 4151 | ` *    An array to compare against` |
|       - | 4152 | ` *  $...` |
|       - | 4153 | ` *   More arrays to compare against` |
|       - | 4154 | ` * Return` |
|       - | 4155 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4156 | ` *  are not present in any of the other arrays.` |
|       - | 4157 | ` */` |
|      22 | 4158 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4159 |  |
|       - | 4160 | `	ph7_hashmap_node *pEntry;` |
|       - | 4161 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4162 | `	ph7_value *pArray;` |
|       - | 4163 | `	ph7_value *pVal;` |
|       - | 4164 | `	sxi32 rc;` |
|       - | 4165 | `	sxu32 n;` |
|       - | 4166 | `	int i;` |
|       - | 4167 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4168 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4169 | `	 * debugging difficult. */` |
|      24 | 4170 | `	if( nArg < 1 ){` |
|       4 | 4171 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4172 | `			"ArgumentCountError",` |
|       - | 4173 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4174 | `			nArg` |
|       - | 4175 | `			);` |
|       - | 4176 | `	}` |
|      22 | 4177 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4178 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4179 | `			"TypeError",` |
|       - | 4180 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4181 | `			ph7_type_name(apArg[0])` |
|       - | 4182 | `			);` |
|       - | 4183 | `	}` |
|      36 | 4184 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4185 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4186 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4187 | `				"TypeError",` |
|       - | 4188 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4189 | `				i + 1,` |
|       2 | 4190 | `				ph7_type_name(apArg[i])` |
|       - | 4191 | `				);` |
|       - | 4192 | `		}` |
|       9 | 4193 | `	}` |
|      17 | 4194 | `	if( nArg == 1 ){` |
|       - | 4195 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4196 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4197 | `		return PH7_OK;` |
|       - | 4198 | `	}` |
|       - | 4199 | `	/* Create a new array */` |
|      15 | 4200 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4201 | `	if( pArray == 0 ){` |
|     ! 0 | 4202 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4203 | `		return PH7_OK;` |
|       - | 4204 | `	}` |
|       - | 4205 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4206 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4207 | `	/* Perform the diff */` |
|      15 | 4208 | `	pEntry = pSrc->pFirst;` |
|      15 | 4209 | `	n = pSrc->nEntry;` |
|      27 | 4210 | `	for(;;){` |
|      55 | 4211 | `		if( n < 1 ){` |
|      15 | 4212 | `			break;` |
|       - | 4213 | `		}` |
|       - | 4214 | `		/* Extract the node value */` |
|      41 | 4215 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4216 | `		if( pVal ){` |
|      69 | 4217 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4218 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4219 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4220 | `				/* Perform the lookup */` |
|      45 | 4221 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4222 | `				if( rc == SXRET_OK ){` |
|       - | 4223 | `					/* Value exist */` |
|      17 | 4224 | `					break;` |
|       - | 4225 | `				}` |
|      15 | 4226 | `			}` |
|      41 | 4227 | `			if( i >= nArg ){` |
|       - | 4228 | `				/* Perform the insertion */` |
|      25 | 4229 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4230 | `			}` |
|      20 | 4231 | `		}` |
|       - | 4232 | `		/* Point to the next entry */` |
|      41 | 4233 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4234 | `		n--;` |
|       1 | 4235 | `	}` |
|       - | 4236 | `	/* Return the freshly created array */` |
|      15 | 4237 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4238 | `	return PH7_OK;` |
|      13 | 4239 |  |
|       - | 4240 | `/*` |
|       - | 4241 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4242 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4243 | ` * Parameters` |
|       - | 4244 | ` *  $array1` |
|       - | 4245 | ` *    The array to compare from` |
|       - | 4246 | ` *  $array2` |
|       - | 4247 | ` *    An array to compare against` |
|       - | 4248 | ` *  $...` |
|       - | 4249 | ` *   More arrays to compare against.` |
|       - | 4250 | ` * $callback` |
|       - | 4251 | ` *  The callback comparison function.` |
|       - | 4252 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4253 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4254 | ` *  than the second.` |
|       - | 4255 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4256 | ` * Return` |
|       - | 4257 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4258 | ` *  are not present in any of the other arrays.` |
|       - | 4259 | ` */` |
|      22 | 4260 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4261 |  |
|       - | 4262 | `	ph7_hashmap_node *pEntry;` |
|       - | 4263 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4264 | `	ph7_value *pCallback;` |
|       - | 4265 | `	ph7_value *pArray;` |
|       - | 4266 | `	ph7_value *pVal;` |
|       - | 4267 | `	sxi32 rc;` |
|       - | 4268 | `	sxu32 n;` |
|       - | 4269 | `	int i;` |
|       - | 4270 |  |
|       - | 4271 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      24 | 4272 | `	if( nArg < 2 ){` |
|       4 | 4273 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4274 | `			"ArgumentCountError",` |
|       - | 4275 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4276 | `			nArg` |
|       - | 4277 | `			);` |
|       - | 4278 | `	}` |
|      22 | 4279 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4280 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4281 | `			"TypeError",` |
|       - | 4282 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4283 | `			ph7_type_name(apArg[0])` |
|       - | 4284 | `			);` |
|       - | 4285 | `	}` |
|       - | 4286 |  |
|      20 | 4287 | `	if( nArg == 2 ){` |
|       - | 4288 | `		/* Only the original array and the callback were provided. */` |
|       - | 4289 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4290 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4291 | `		 * validation order.` |
|       - | 4292 | `		 */` |
|       4 | 4293 | `	} else {` |
|       - | 4294 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      24 | 4295 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      16 | 4296 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      10 | 4297 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4298 | `					"TypeError",` |
|       - | 4299 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4300 | `					i + 1,` |
|       6 | 4301 | `					ph7_type_name(apArg[i])` |
|       - | 4302 | `					);` |
|       - | 4303 | `			}` |
|       6 | 4304 | `		}` |
|       - | 4305 | `	}` |
|       - | 4306 |  |
|       - | 4307 | `	/* Identify the callback (always expected as the last argument). */` |
|      14 | 4308 | `	pCallback = apArg[nArg - 1];` |
|       - | 4309 | `	/* Validate the callback to match PHP's error messages. */` |
|      14 | 4310 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       7 | 4311 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4312 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4313 | `				"TypeError",` |
|       - | 4314 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4315 | `				nArg` |
|       - | 4316 | `				);` |
|       - | 4317 | `		}` |
|       5 | 4318 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4319 | `			int len;` |
|       3 | 4320 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4321 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4322 | `				"TypeError",` |
|       - | 4323 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4324 | `				nArg,` |
|       1 | 4325 | `				zName` |
|       - | 4326 | `				);` |
|       - | 4327 | `		}` |
|       4 | 4328 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4329 | `			"TypeError",` |
|       - | 4330 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4331 | `			nArg` |
|       - | 4332 | `			);` |
|       - | 4333 | `	}` |
|       - | 4334 |  |
|       7 | 4335 | `	if( nArg == 2 ){` |
|       - | 4336 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4337 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4338 | `		return PH7_OK;` |
|       - | 4339 | `	}` |
|       - | 4340 |  |
|       - | 4341 | `	/* Create a new array */` |
|       5 | 4342 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4343 | `	if( pArray == 0 ){` |
|     ! 0 | 4344 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4345 | `		return PH7_OK;` |
|       - | 4346 | `	}` |
|       - | 4347 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 4348 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4349 | `	/* Perform the diff */` |
|       5 | 4350 | `	pEntry = pSrc->pFirst;` |
|       5 | 4351 | `	n = pSrc->nEntry;` |
|       5 | 4352 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 4353 | `	for(;;){` |
|      11 | 4354 | `		if( n < 1 ){` |
|       3 | 4355 | `			break;` |
|       - | 4356 | `		}` |
|       - | 4357 | `		/* Extract the node value */` |
|       9 | 4358 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4359 | `		if( pVal ){` |
|      15 | 4360 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4361 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4362 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4363 | `				/* Perform the lookup */` |
|       9 | 4364 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       9 | 4365 | `				if( rc == SXRET_OK ){` |
|       - | 4366 | `					/* Value exist */` |
|       3 | 4367 | `					break;` |
|       - | 4368 | `				}` |
|       4 | 4369 | `			}` |
|       9 | 4370 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 4371 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 4372 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 4373 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 4374 | `				return PH7_EXCEPTION;` |
|       - | 4375 | `			}` |
|       7 | 4376 | `			if( i >= (nArg - 1)){` |
|       - | 4377 | `				/* Perform the insertion */` |
|       5 | 4378 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4379 | `			}` |
|       3 | 4380 | `		}` |
|       - | 4381 | `		/* Point to the next entry */` |
|       7 | 4382 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4383 | `		n--;` |
|       1 | 4384 | `	}` |
|       - | 4385 | `	/* Return the freshly created array */` |
|       3 | 4386 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4387 | `	return PH7_OK;` |
|      13 | 4388 |  |
|       - | 4389 | `/*` |
|       - | 4390 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4391 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4392 | ` * Parameters` |
|       - | 4393 | ` *  $array1` |
|       - | 4394 | ` *    The array to compare from` |
|       - | 4395 | ` *  $array2` |
|       - | 4396 | ` *    An array to compare against` |
|       - | 4397 | ` *  $...` |
|       - | 4398 | ` *   More arrays to compare against` |
|       - | 4399 | ` * Return` |
|       - | 4400 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4401 | ` *  are not present in any of the other arrays.` |
|       - | 4402 | ` */` |
|      20 | 4403 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4404 |  |
|       - | 4405 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4406 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4407 | `	ph7_value *pArray;` |
|       - | 4408 | `	ph7_value *pVal;` |
|       - | 4409 | `	sxi32 rc;` |
|       - | 4410 | `	sxu32 n;` |
|       - | 4411 | `	int i;` |
|       - | 4412 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4413 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4414 | `	 * accompanying integration tests to pass. */` |
|      22 | 4415 | `	if( nArg < 1 ){` |
|       4 | 4416 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4417 | `			"ArgumentCountError",` |
|       - | 4418 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4419 | `			nArg` |
|       - | 4420 | `			);` |
|       - | 4421 | `	}` |
|      20 | 4422 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4423 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4424 | `			"TypeError",` |
|       - | 4425 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4426 | `			ph7_type_name(apArg[0])` |
|       - | 4427 | `			);` |
|       - | 4428 | `	}` |
|      32 | 4429 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4430 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4431 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4432 | `				"TypeError",` |
|       - | 4433 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4434 | `				i + 1,` |
|       4 | 4435 | `				ph7_type_name(apArg[i])` |
|       - | 4436 | `				);` |
|       - | 4437 | `		}` |
|       9 | 4438 | `	}` |
|      13 | 4439 | `	if( nArg == 1 ){` |
|       - | 4440 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4441 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4442 | `		return PH7_OK;` |
|       - | 4443 | `	}` |
|       - | 4444 | `	/* Create a new array */` |
|      11 | 4445 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4446 | `	if( pArray == 0 ){` |
|     ! 0 | 4447 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4448 | `		return PH7_OK;` |
|       - | 4449 | `	}` |
|       - | 4450 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4451 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4452 | `	/* Perform the diff */` |
|      11 | 4453 | `	pEntry = pSrc->pFirst;` |
|      11 | 4454 | `	n = pSrc->nEntry;` |
|      11 | 4455 | `	pN1 = pN2 = 0;` |
|      29 | 4456 | `	for(;;){` |
|       - | 4457 | `		int keep;` |
|      35 | 4458 | `		if( n < 1 ){` |
|      11 | 4459 | `			break;` |
|       - | 4460 | `		}` |
|       - | 4461 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4462 | `		keep = 1;` |
|      41 | 4463 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4464 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4465 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4466 | `			/* Perform a key lookup first */` |
|      29 | 4467 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4468 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4469 | `			}else{` |
|      17 | 4470 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4471 | `			}` |
|      29 | 4472 | `			if( rc != SXRET_OK ){` |
|       - | 4473 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4474 | `				continue;` |
|       - | 4475 | `			}` |
|       - | 4476 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4477 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4478 | `			if( pVal ){` |
|       - | 4479 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4480 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4481 | `				if( pVal2 ){` |
|      15 | 4482 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4483 | `					if( cmp == 0 ){` |
|       - | 4484 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4485 | `						keep = 0;` |
|      13 | 4486 | `						break;` |
|       - | 4487 | `					}` |
|       1 | 4488 | `				}` |
|       1 | 4489 | `			}` |
|       2 | 4490 | `		}` |
|      25 | 4491 | `		if( keep ){` |
|       - | 4492 | `			/* Perform the insertion */` |
|      13 | 4493 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4494 | `		}` |
|       - | 4495 | `		/* Point to the next entry */` |
|      25 | 4496 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4497 | `		n--;` |
|       1 | 4498 | `	}` |
|       - | 4499 | `	/* Return the freshly created array */` |
|      11 | 4500 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4501 | `	return PH7_OK;` |
|      12 | 4502 |  |
|       - | 4503 | `/*` |
|       - | 4504 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4505 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4506 | ` *  by a user supplied callback function.` |
|       - | 4507 | ` * Parameters` |
|       - | 4508 | ` *  $array1` |
|       - | 4509 | ` *    The array to compare from` |
|       - | 4510 | ` *  $array2` |
|       - | 4511 | ` *    An array to compare against` |
|       - | 4512 | ` *  $...` |
|       - | 4513 | ` *   More arrays to compare against.` |
|       - | 4514 | ` *  $key_compare_func` |
|       - | 4515 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4516 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4517 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4518 | ` * Return` |
|       - | 4519 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4520 | ` *  are not present in any of the other arrays.` |
|       - | 4521 | ` */` |
|      24 | 4522 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4523 |  |
|       - | 4524 | `	ph7_hashmap_node *pEntry;` |
|       - | 4525 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4526 | `	ph7_value *pCallback;` |
|       - | 4527 | `	ph7_value *pArray;` |
|       - | 4528 | `	sxi32 rc;` |
|       - | 4529 | `	sxu32 n;` |
|       - | 4530 | `	int i;` |
|       - | 4531 |  |
|       - | 4532 | `	/* Argument validation mimicking PHP errors. */` |
|      26 | 4533 | `	if( nArg < 2 ){` |
|       4 | 4534 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4535 | `			"ArgumentCountError",` |
|       - | 4536 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4537 | `			nArg` |
|       - | 4538 | `			);` |
|       - | 4539 | `	}` |
|      24 | 4540 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4541 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4542 | `			"TypeError",` |
|       - | 4543 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4544 | `			ph7_type_name(apArg[0])` |
|       - | 4545 | `			);` |
|       - | 4546 | `	}` |
|       - | 4547 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4548 | `	 * expected to be a callback. */` |
|      36 | 4549 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      18 | 4550 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4551 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4552 | `				"TypeError",` |
|       - | 4553 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4554 | `				i + 1,` |
|       2 | 4555 | `				ph7_type_name(apArg[i])` |
|       - | 4556 | `				);` |
|       - | 4557 | `		}` |
|       9 | 4558 | `	}` |
|       - | 4559 | `	/* Point to the callback value */` |
|      20 | 4560 | `	pCallback = apArg[nArg - 1];` |
|      20 | 4561 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4562 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4563 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4564 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4565 | `		 * string given" which we also reproduce. */` |
|       7 | 4566 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4567 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4568 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4569 | `				"TypeError",` |
|       - | 4570 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4571 | `				nArg` |
|       - | 4572 | `				);` |
|       - | 4573 | `		}` |
|       5 | 4574 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4575 | `			/* neither array nor string */` |
|       7 | 4576 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4577 | `				"TypeError",` |
|       - | 4578 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4579 | `				nArg` |
|       - | 4580 | `				);` |
|       - | 4581 | `		}` |
|       - | 4582 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4583 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4584 | `			"TypeError",` |
|       - | 4585 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4586 | `			nArg,` |
|     ! 0 | 4587 | `			ph7_type_name(pCallback)` |
|       - | 4588 | `			);` |
|       - | 4589 | `	}` |
|      13 | 4590 | `	if( nArg == 2 ){` |
|       - | 4591 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4592 | `		 * input array. */` |
|       3 | 4593 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4594 | `		return PH7_OK;` |
|       - | 4595 | `	}` |
|       - | 4596 | `	/* Create a new array */` |
|      11 | 4597 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4598 | `	if( pArray == 0 ){` |
|     ! 0 | 4599 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4600 | `		return PH7_OK;` |
|       - | 4601 | `	}` |
|       - | 4602 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4603 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4604 | `	/* Perform the diff */` |
|      11 | 4605 | `	pEntry = pSrc->pFirst;` |
|      11 | 4606 | `	n = pSrc->nEntry;` |
|      21 | 4607 | `	for(;;){` |
|       - | 4608 | `		int keep;` |
|      27 | 4609 | `		if( n < 1 ){` |
|       9 | 4610 | `			break;` |
|       - | 4611 | `		}` |
|      19 | 4612 | `		keep = 1;` |
|      31 | 4613 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4614 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 4615 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4616 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 4617 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 4618 | `			while( pIt ){` |
|       - | 4619 | `				/* build temporary key values for callback */` |
|       - | 4620 | `				ph7_value key1, key2, result;` |
|       - | 4621 | `				/* initialise only once using the appropriate helper */` |
|      33 | 4622 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4623 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4624 | `				}else{` |
|       - | 4625 | `					SyString sStr;` |
|      33 | 4626 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4627 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4628 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 4629 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4630 | `				}` |
|      33 | 4631 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4632 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4633 | `				}else{` |
|       - | 4634 | `					SyString sStr;` |
|      33 | 4635 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4636 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4637 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 4638 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4639 | `				}` |
|      33 | 4640 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4641 | `				/* call user callback with (key1, key2) */` |
|       - | 4642 | `				{` |
|       - | 4643 | `					ph7_value *apK[2];` |
|      33 | 4644 | `					apK[0] = &key1;` |
|      33 | 4645 | `					apK[1] = &key2;` |
|      33 | 4646 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4647 | `				}` |
|      33 | 4648 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 4649 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 4650 | `					 * array_uintersect (which signal back from` |
|       - | 4651 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 4652 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 4653 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 4654 | `					PH7_MemObjRelease(&result);` |
|       3 | 4655 | `					PH7_MemObjRelease(&key1);` |
|       3 | 4656 | `					PH7_MemObjRelease(&key2);` |
|       3 | 4657 | `					return PH7_EXCEPTION;` |
|       - | 4658 | `				}` |
|      31 | 4659 | `				if( rc == SXRET_OK ){` |
|      31 | 4660 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4661 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4662 | `					}` |
|      31 | 4663 | `					if( result.x.iVal == 0 ){` |
|       - | 4664 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4665 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4666 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4667 | `						if( pVal1 && pVal2 ){` |
|      13 | 4668 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4669 | `								keep = 0;` |
|       9 | 4670 | `								PH7_MemObjRelease(&result);` |
|       - | 4671 | `								/* release keys too before breaking */` |
|       9 | 4672 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4673 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4674 | `								break;` |
|       - | 4675 | `							}` |
|       2 | 4676 | `						}` |
|       2 | 4677 | `					}` |
|      11 | 4678 | `				}` |
|      23 | 4679 | `				PH7_MemObjRelease(&result);` |
|      23 | 4680 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4681 | `				PH7_MemObjRelease(&key2);` |
|       - | 4682 | `				/* move to next node */` |
|      23 | 4683 | `				pIt = pIt->pPrev;` |
|      23 | 4684 | `				if( keep == 0 ) break;` |
|       1 | 4685 | `			}` |
|      21 | 4686 | `			if( keep == 0 ) break;` |
|       7 | 4687 | `		}` |
|      17 | 4688 | `		if( keep ){` |
|       - | 4689 | `			/* Perform the insertion */` |
|       9 | 4690 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4691 | `		}` |
|       - | 4692 | `		/* Point to the next entry */` |
|      17 | 4693 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4694 | `		n--;` |
|       1 | 4695 | `	}` |
|       - | 4696 | `	/* Return the freshly created array */` |
|       9 | 4697 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4698 | `	return PH7_OK;` |
|      14 | 4699 |  |
|       - | 4700 | `/*` |
|       - | 4701 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4702 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4703 | ` * Parameters` |
|       - | 4704 | ` *  $array1` |
|       - | 4705 | ` *    The array to compare from` |
|       - | 4706 | ` *  $array2` |
|       - | 4707 | ` *    An array to compare against` |
|       - | 4708 | ` *  $...` |
|       - | 4709 | ` *   More arrays to compare against` |
|       - | 4710 | ` * Return` |
|       - | 4711 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4712 | ` *  in any of the other arrays.` |
|       - | 4713 | ` * Note that NULL is returned on failure.` |
|       - | 4714 | ` */` |
|      14 | 4715 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4716 |  |
|       - | 4717 | `	ph7_hashmap_node *pEntry;` |
|       - | 4718 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4719 | `	ph7_value *pArray;` |
|       - | 4720 | `	sxi32 rc;` |
|       - | 4721 | `	sxu32 n;` |
|       - | 4722 | `	int i;` |
|       - | 4723 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4724 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4725 | `	 * helpers. */` |
|      16 | 4726 | `	if( nArg < 1 ){` |
|       4 | 4727 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4728 | `			"ArgumentCountError",` |
|       - | 4729 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4730 | `			nArg` |
|       - | 4731 | `			);` |
|       - | 4732 | `	}` |
|      14 | 4733 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4734 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4735 | `			"TypeError",` |
|       - | 4736 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4737 | `			ph7_type_name(apArg[0])` |
|       - | 4738 | `			);` |
|       - | 4739 | `	}` |
|      20 | 4740 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4741 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4742 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4743 | `				"TypeError",` |
|       - | 4744 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4745 | `				i + 1,` |
|       2 | 4746 | `				ph7_type_name(apArg[i])` |
|       - | 4747 | `				);` |
|       - | 4748 | `		}` |
|       5 | 4749 | `	}` |
|       9 | 4750 | `	if( nArg == 1 ){` |
|       - | 4751 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4752 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4753 | `		return PH7_OK;` |
|       - | 4754 | `	}` |
|       - | 4755 | `	/* Create a new array */` |
|       7 | 4756 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4757 | `	if( pArray == 0 ){` |
|     ! 0 | 4758 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4759 | `		return PH7_OK;` |
|       - | 4760 | `	}` |
|       - | 4761 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4762 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4763 | `	/* Perfrom the diff */` |
|       7 | 4764 | `	pEntry = pSrc->pFirst;` |
|       7 | 4765 | `	n = pSrc->nEntry;` |
|      12 | 4766 | `	for(;;){` |
|      25 | 4767 | `		if( n < 1 ){` |
|       7 | 4768 | `			break;` |
|       - | 4769 | `		}` |
|      31 | 4770 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4771 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4772 | `				/* ignore */` |
|     ! 0 | 4773 | `				continue;` |
|       - | 4774 | `			}` |
|      23 | 4775 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4776 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4777 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4778 | `				/* Blob lookup */` |
|      17 | 4779 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4780 | `			}else{` |
|       - | 4781 | `				/* Int lookup */` |
|       7 | 4782 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4783 | `			}` |
|      23 | 4784 | `			if( rc == SXRET_OK ){` |
|       - | 4785 | `				/* Key exists,break immediately */` |
|      11 | 4786 | `				break;` |
|       - | 4787 | `			}` |
|       7 | 4788 | `		}` |
|      19 | 4789 | `		if( i >= nArg ){` |
|       - | 4790 | `			/* Perform the insertion */` |
|       9 | 4791 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4792 | `		}` |
|       - | 4793 | `		/* Point to the next entry */` |
|      19 | 4794 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4795 | `		n--;` |
|       1 | 4796 | `	}` |
|       - | 4797 | `	/* Return the freshly created array */` |
|       7 | 4798 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4799 | `	return PH7_OK;` |
|       9 | 4800 |  |
|       - | 4801 | `/*` |
|       - | 4802 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4803 | ` *  Computes the intersection of arrays.` |
|       - | 4804 | ` * Parameters` |
|       - | 4805 | ` *  $array1` |
|       - | 4806 | ` *    The array to compare from` |
|       - | 4807 | ` *  $array2` |
|       - | 4808 | ` *    An array to compare against` |
|       - | 4809 | ` *  $...` |
|       - | 4810 | ` *   More arrays to compare against` |
|       - | 4811 | ` * Return` |
|       - | 4812 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4813 | ` *  in all of the parameters.` |
|       - | 4814 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4815 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4816 | ` */` |
|      22 | 4817 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4818 |  |
|       - | 4819 | `	ph7_hashmap_node *pEntry;` |
|       - | 4820 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4821 | `	ph7_value *pArray;` |
|       - | 4822 | `	ph7_value *pVal;` |
|       - | 4823 | `	sxi32 rc;` |
|       - | 4824 | `	sxu32 n;` |
|       - | 4825 | `	int i;` |
|      24 | 4826 | `	if( nArg < 1 ){` |
|       4 | 4827 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4828 | `			"ArgumentCountError",` |
|       - | 4829 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4830 | `			nArg` |
|       - | 4831 | `			);` |
|       - | 4832 | `	}` |
|      22 | 4833 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4834 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4835 | `			"TypeError",` |
|       - | 4836 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4837 | `			ph7_type_name(apArg[0])` |
|       - | 4838 | `			);` |
|       - | 4839 | `	}` |
|      36 | 4840 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4841 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4842 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4843 | `				"TypeError",` |
|       - | 4844 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4845 | `				i + 1,` |
|       2 | 4846 | `				ph7_type_name(apArg[i])` |
|       - | 4847 | `				);` |
|       - | 4848 | `		}` |
|       9 | 4849 | `	}` |
|      17 | 4850 | `	if( nArg == 1 ){` |
|       - | 4851 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4852 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4853 | `		return PH7_OK;` |
|       - | 4854 | `	}` |
|       - | 4855 | `	/* Create a new array */` |
|      15 | 4856 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4857 | `	if( pArray == 0 ){` |
|     ! 0 | 4858 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4859 | `		return PH7_OK;` |
|       - | 4860 | `	}` |
|       - | 4861 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4862 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4863 | `	/* Perform the intersection */` |
|      15 | 4864 | `	pEntry = pSrc->pFirst;` |
|      15 | 4865 | `	n = pSrc->nEntry;` |
|      31 | 4866 | `	for(;;){` |
|      63 | 4867 | `		if( n < 1 ){` |
|      15 | 4868 | `			break;` |
|       - | 4869 | `		}` |
|       - | 4870 | `		/* Extract the node value */` |
|      49 | 4871 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4872 | `		if( pVal ){` |
|      79 | 4873 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4874 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4875 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4876 | `				/* Perform the lookup */` |
|      55 | 4877 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4878 | `				if( rc != SXRET_OK ){` |
|       - | 4879 | `					/* Value does not exist */` |
|      25 | 4880 | `					break;` |
|       - | 4881 | `				}` |
|      16 | 4882 | `			}` |
|      49 | 4883 | `			if( i >= nArg ){` |
|       - | 4884 | `				/* Perform the insertion */` |
|      25 | 4885 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4886 | `			}` |
|      24 | 4887 | `		}` |
|       - | 4888 | `		/* Point to the next entry */` |
|      49 | 4889 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4890 | `		n--;` |
|       1 | 4891 | `	}` |
|       - | 4892 | `	/* Return the freshly created array */` |
|      15 | 4893 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4894 | `	return PH7_OK;` |
|      13 | 4895 |  |
|       - | 4896 | `/*` |
|       - | 4897 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4898 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4899 | ` * Parameters` |
|       - | 4900 | ` *  $array1` |
|       - | 4901 | ` *    The array to compare from` |
|       - | 4902 | ` *  $array2` |
|       - | 4903 | ` *    An array to compare against` |
|       - | 4904 | ` *  $...` |
|       - | 4905 | ` *   More arrays to compare against` |
|       - | 4906 | ` * Return` |
|       - | 4907 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4908 | ` *  in all the arguments, with matching keys.` |
|       - | 4909 | ` */` |
|      22 | 4910 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4911 |  |
|       - | 4912 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4913 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4914 | `	ph7_value *pArray;` |
|       - | 4915 | `	ph7_value *pVal;` |
|       - | 4916 | `	sxi32 rc;` |
|       - | 4917 | `	sxu32 n;` |
|       - | 4918 | `	int i;` |
|      24 | 4919 | `	if( nArg < 1 ){` |
|       4 | 4920 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4921 | `			"ArgumentCountError",` |
|       - | 4922 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4923 | `			nArg` |
|       - | 4924 | `			);` |
|       - | 4925 | `	}` |
|      22 | 4926 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4927 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4928 | `			"TypeError",` |
|       - | 4929 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4930 | `			ph7_type_name(apArg[0])` |
|       - | 4931 | `			);` |
|       - | 4932 | `	}` |
|      36 | 4933 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4934 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4935 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4936 | `				"TypeError",` |
|       - | 4937 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4938 | `				i + 1,` |
|       2 | 4939 | `				ph7_type_name(apArg[i])` |
|       - | 4940 | `				);` |
|       - | 4941 | `		}` |
|       9 | 4942 | `	}` |
|      17 | 4943 | `	if( nArg == 1 ){` |
|       - | 4944 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4945 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4946 | `		return PH7_OK;` |
|       - | 4947 | `	}` |
|       - | 4948 | `	/* Create a new array */` |
|      15 | 4949 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4950 | `	if( pArray == 0 ){` |
|     ! 0 | 4951 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4952 | `		return PH7_OK;` |
|       - | 4953 | `	}` |
|       - | 4954 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4955 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4956 | `	/* Perform the intersection */` |
|      15 | 4957 | `	pEntry = pSrc->pFirst;` |
|      15 | 4958 | `	n = pSrc->nEntry;` |
|      15 | 4959 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4960 | `	for(;;){` |
|      47 | 4961 | `		if( n < 1 ){` |
|      15 | 4962 | `			break;` |
|       - | 4963 | `		}` |
|       - | 4964 | `		/* Extract the node value */` |
|      33 | 4965 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4966 | `		if( pVal ){` |
|      53 | 4967 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4968 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4969 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4970 | `				/* Perform a key lookup first */` |
|      37 | 4971 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4972 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4973 | `				}else{` |
|      23 | 4974 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4975 | `				}` |
|      37 | 4976 | `				if( rc != SXRET_OK ){` |
|       - | 4977 | `					/* No such key,break immediately */` |
|       7 | 4978 | `					break;` |
|       - | 4979 | `				}` |
|       - | 4980 | `				/* Perform the lookup */` |
|      31 | 4981 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4982 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4983 | `					/* Value does not exist */` |
|       6 | 4984 | `					break;` |
|       - | 4985 | `				}` |
|      11 | 4986 | `			}` |
|      33 | 4987 | `			if( i >= nArg ){` |
|       - | 4988 | `				/* Perform the insertion */` |
|      17 | 4989 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4990 | `			}` |
|      16 | 4991 | `		}` |
|       - | 4992 | `		/* Point to the next entry */` |
|      33 | 4993 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4994 | `		n--;` |
|       1 | 4995 | `	}` |
|       - | 4996 | `	/* Return the freshly created array */` |
|      15 | 4997 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4998 | `	return PH7_OK;` |
|      13 | 4999 |  |
|       - | 5000 | `/*` |
|       - | 5001 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 5002 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 5003 | ` * Parameters` |
|       - | 5004 | ` *  $array1` |
|       - | 5005 | ` *    The array to compare from` |
|       - | 5006 | ` *  $...` |
|       - | 5007 | ` *   More arrays to compare against` |
|       - | 5008 | ` * Return` |
|       - | 5009 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 5010 | ` *  have keys that are present in all arguments.` |
|       - | 5011 | ` * Note that NULL is returned on failure.` |
|       - | 5012 | ` */` |
|      22 | 5013 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5014 |  |
|       - | 5015 | `	ph7_hashmap_node *pEntry;` |
|       - | 5016 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5017 | `	ph7_value *pArray;` |
|       - | 5018 | `	sxi32 rc;` |
|       - | 5019 | `	sxu32 n;` |
|       - | 5020 | `	int i;` |
|      24 | 5021 | `	if( nArg < 1 ){` |
|       4 | 5022 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5023 | `			"ArgumentCountError",` |
|       - | 5024 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 5025 | `			nArg` |
|       - | 5026 | `			);` |
|       - | 5027 | `	}` |
|      22 | 5028 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5029 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5030 | `			"TypeError",` |
|       - | 5031 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5032 | `			ph7_type_name(apArg[0])` |
|       - | 5033 | `			);` |
|       - | 5034 | `	}` |
|      36 | 5035 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5036 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5037 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5038 | `				"TypeError",` |
|       - | 5039 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5040 | `				i + 1,` |
|       2 | 5041 | `				ph7_type_name(apArg[i])` |
|       - | 5042 | `				);` |
|       - | 5043 | `		}` |
|       9 | 5044 | `	}` |
|      17 | 5045 | `	if( nArg == 1 ){` |
|       - | 5046 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5047 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5048 | `		return PH7_OK;` |
|       - | 5049 | `	}` |
|       - | 5050 | `	/* Create a new array */` |
|      15 | 5051 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5052 | `	if( pArray == 0 ){` |
|     ! 0 | 5053 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5054 | `		return PH7_OK;` |
|       - | 5055 | `	}` |
|       - | 5056 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 5057 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5058 | `	/* Perform the intersection */` |
|      15 | 5059 | `	pEntry = pSrc->pFirst;` |
|      15 | 5060 | `	n = pSrc->nEntry;` |
|      24 | 5061 | `	for(;;){` |
|      49 | 5062 | `		if( n < 1 ){` |
|      15 | 5063 | `			break;` |
|       - | 5064 | `		}` |
|      57 | 5065 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 5066 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 5067 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 5068 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5069 | `				/* Blob lookup */` |
|      27 | 5070 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 5071 | `			}else{` |
|       - | 5072 | `				/* Int key */` |
|      13 | 5073 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5074 | `			}` |
|      39 | 5075 | `			if( rc != SXRET_OK ){` |
|       - | 5076 | `				/* Key does not exist, break immediately */` |
|      17 | 5077 | `				break;` |
|       - | 5078 | `			}` |
|      12 | 5079 | `		}` |
|      35 | 5080 | `		if( i >= nArg ){` |
|       - | 5081 | `			/* Perform the insertion */` |
|      19 | 5082 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 5083 | `		}` |
|       - | 5084 | `		/* Point to the next entry */` |
|      35 | 5085 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 5086 | `		n--;` |
|       1 | 5087 | `	}` |
|       - | 5088 | `	/* Return the freshly created array */` |
|      15 | 5089 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5090 | `	return PH7_OK;` |
|      13 | 5091 |  |
|       - | 5092 | `/*` |
|       - | 5093 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 5094 | ` *  Computes the intersection of arrays.` |
|       - | 5095 | ` * Parameters` |
|       - | 5096 | ` *  $array1` |
|       - | 5097 | ` *    The array to compare from` |
|       - | 5098 | ` *  $array2` |
|       - | 5099 | ` *    An array to compare against` |
|       - | 5100 | ` *  $...` |
|       - | 5101 | ` *   More arrays to compare against` |
|       - | 5102 | ` * $callback` |
|       - | 5103 | ` *  The callback comparison function.` |
|       - | 5104 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5105 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5106 | ` *  than the second.` |
|       - | 5107 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5108 | ` * Return` |
|       - | 5109 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5110 | ` *  in all of the parameters. .` |
|       - | 5111 | ` * Note that NULL is returned on failure.` |
|       - | 5112 | ` */` |
|      26 | 5113 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5114 |  |
|       - | 5115 | `	ph7_hashmap_node *pEntry;` |
|       - | 5116 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5117 | `	ph7_value *pCallback;` |
|       - | 5118 | `	ph7_value *pArray;` |
|       - | 5119 | `	ph7_value *pVal;` |
|       - | 5120 | `	sxi32 rc;` |
|       - | 5121 | `	sxu32 n;` |
|       - | 5122 | `	int i;` |
|       - | 5123 |  |
|       - | 5124 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      28 | 5125 | `	if( nArg < 2 ){` |
|       4 | 5126 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5127 | `			"ArgumentCountError",` |
|       - | 5128 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5129 | `			nArg` |
|       - | 5130 | `			);` |
|       - | 5131 | `	}` |
|      26 | 5132 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5133 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5134 | `			"TypeError",` |
|       - | 5135 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5136 | `			ph7_type_name(apArg[0])` |
|       - | 5137 | `			);` |
|       - | 5138 | `	}` |
|       - | 5139 |  |
|      24 | 5140 | `	if( nArg == 2 ){` |
|       - | 5141 | `		/* Only the original array and the callback were provided. */` |
|       - | 5142 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5143 | `		 * validation ordering. */` |
|       3 | 5144 | `	} else {` |
|       - | 5145 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      36 | 5146 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      20 | 5147 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5148 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5149 | `					"TypeError",` |
|       - | 5150 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5151 | `					i + 1,` |
|       2 | 5152 | `					ph7_type_name(apArg[i])` |
|       - | 5153 | `					);` |
|       - | 5154 | `			}` |
|      10 | 5155 | `		}` |
|       - | 5156 | `	}` |
|       - | 5157 |  |
|       - | 5158 | `	/* Identify the callback (always expected as the last argument). */` |
|      22 | 5159 | `	pCallback = apArg[nArg - 1];` |
|       - | 5160 | `	/* Validate the callback to match PHP's error messages. */` |
|      22 | 5161 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      11 | 5162 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5163 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5164 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5165 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5166 | `			 */` |
|       7 | 5167 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       7 | 5168 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5169 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5170 | `					"TypeError",` |
|       - | 5171 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5172 | `					nArg` |
|       - | 5173 | `					);` |
|       - | 5174 | `			}` |
|       - | 5175 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5176 | `			{` |
|       5 | 5177 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       5 | 5178 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       5 | 5179 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5180 | `					int nMethodLen;` |
|       5 | 5181 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       5 | 5182 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       5 | 5183 | `					if( pClass ){` |
|       - | 5184 | `						/* Class exists but method is missing. */` |
|       4 | 5185 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5186 | `							"TypeError",` |
|       - | 5187 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5188 | `							nArg,` |
|       1 | 5189 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5190 | `							zMethod` |
|       - | 5191 | `							);` |
|       - | 5192 | `					}` |
|       - | 5193 | `					/* Class not found */` |
|       - | 5194 | `					{` |
|       - | 5195 | `						int nName;` |
|       3 | 5196 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5197 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5198 | `							"TypeError",` |
|       - | 5199 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5200 | `							nArg,` |
|       1 | 5201 | `							zName` |
|       - | 5202 | `							);` |
|       - | 5203 | `					}` |
|       - | 5204 | `				}` |
|       - | 5205 | `			}` |
|       - | 5206 | `			/* Fallback message */` |
|     ! 0 | 5207 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5208 | `				"TypeError",` |
|       - | 5209 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5210 | `				nArg` |
|       - | 5211 | `				);` |
|       - | 5212 | `		}` |
|       5 | 5213 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5214 | `			int len;` |
|       3 | 5215 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5216 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5217 | `				"TypeError",` |
|       - | 5218 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5219 | `				nArg,` |
|       1 | 5220 | `				zName` |
|       - | 5221 | `				);` |
|       - | 5222 | `		}` |
|       4 | 5223 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5224 | `			"TypeError",` |
|       - | 5225 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5226 | `			nArg` |
|       - | 5227 | `			);` |
|       - | 5228 | `	}` |
|       - | 5229 |  |
|      11 | 5230 | `	if( nArg == 2 ){` |
|       - | 5231 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5232 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5233 | `		return PH7_OK;` |
|       - | 5234 | `	}` |
|       - | 5235 |  |
|       - | 5236 | `	/* Create a new array */` |
|       7 | 5237 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5238 | `	if( pArray == 0 ){` |
|     ! 0 | 5239 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5240 | `		return PH7_OK;` |
|       - | 5241 | `	}` |
|       - | 5242 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 5243 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5244 | `	/* Perform the intersection */` |
|       7 | 5245 | `	pEntry = pSrc->pFirst;` |
|       7 | 5246 | `	n = pSrc->nEntry;` |
|       7 | 5247 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 5248 | `	for(;;){` |
|      19 | 5249 | `		if( n < 1 ){` |
|       5 | 5250 | `			break;` |
|       - | 5251 | `		}` |
|       - | 5252 | `		/* Extract the node value */` |
|      15 | 5253 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5254 | `		if( pVal ){` |
|      23 | 5255 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 5256 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5257 | `					/* ignore */` |
|     ! 0 | 5258 | `					continue;` |
|       - | 5259 | `				}` |
|       - | 5260 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 5261 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5262 | `				/* Perform the lookup */` |
|      15 | 5263 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 5264 | `				if( rc != SXRET_OK ){` |
|       - | 5265 | `					/* Value does not exist */` |
|       7 | 5266 | `					break;` |
|       - | 5267 | `				}` |
|       5 | 5268 | `			}` |
|      15 | 5269 | `			if( i >= (nArg-1) ){` |
|       - | 5270 | `				/* Perform the insertion */` |
|       9 | 5271 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5272 | `			}` |
|       7 | 5273 | `		}` |
|      15 | 5274 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 5275 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5276 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 5277 | `			return PH7_EXCEPTION;` |
|       - | 5278 | `		}` |
|       - | 5279 | `		/* Point to the next entry */` |
|      13 | 5280 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5281 | `		n--;` |
|       1 | 5282 | `	}` |
|       - | 5283 | `	/* Return the freshly created array */` |
|       5 | 5284 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5285 | `	return PH7_OK;` |
|      15 | 5286 |  |
|       - | 5287 | `/*` |
|       - | 5288 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5289 | ` *  Fill an array with values.` |
|       - | 5290 | ` * Parameters` |
|       - | 5291 | ` *  $start_index` |
|       - | 5292 | ` *    The first index of the returned array.` |
|       - | 5293 | ` *  $num` |
|       - | 5294 | ` *   Number of elements to insert.` |
|       - | 5295 | ` *  $value` |
|       - | 5296 | ` *    Value to use for filling.` |
|       - | 5297 | ` * Return` |
|       - | 5298 | ` *  The filled array or null on failure.` |
|       - | 5299 | ` */` |
|     238 | 5300 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5301 |  |
|       - | 5302 | `	ph7_value *pArray;` |
|       - | 5303 | `	int i,nEntry;` |
|       - | 5304 |  |
|       - | 5305 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 5306 | `	if( nArg != 3 ){` |
|       - | 5307 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 5308 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5309 | `			"ArgumentCountError",` |
|       - | 5310 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5311 | `			nArg` |
|       - | 5312 | `			);` |
|       - | 5313 | `	}` |
|       - | 5314 |  |
|       - | 5315 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5316 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5317 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5318 | `	 * and NULLs are rejected outright. */` |
|     466 | 5319 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 5320 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5321 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5322 | `			"TypeError",` |
|       - | 5323 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5324 | `			ph7_type_name(apArg[0])` |
|       - | 5325 | `			);` |
|       - | 5326 | `	}` |
|     234 | 5327 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5328 | `		int len;` |
|       8 | 5329 | `		sxu8 bReal = FALSE;` |
|       8 | 5330 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5331 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5332 | `			/* Non‑numeric string is an error. */` |
|       3 | 5333 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5334 | `				"TypeError",` |
|       - | 5335 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5336 | `				);` |
|       - | 5337 | `		}` |
|       5 | 5338 | `		if( bReal ){` |
|       - | 5339 | `			/* float-string -> deprecation warning */` |
|       4 | 5340 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5341 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5342 | `				zStr` |
|       - | 5343 | `				);` |
|       1 | 5344 | `		}` |
|       2 | 5345 | `	}` |
|       - | 5346 |  |
|       - | 5347 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5348 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 5349 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 5350 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5351 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5352 | `			"TypeError",` |
|       - | 5353 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5354 | `			ph7_type_name(apArg[1])` |
|       - | 5355 | `			);` |
|       - | 5356 | `	}` |
|     232 | 5357 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5358 | `		int len;` |
|       3 | 5359 | `		sxu8 bReal = FALSE;` |
|       3 | 5360 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5361 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5362 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5363 | `				"TypeError",` |
|       - | 5364 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5365 | `				);` |
|       - | 5366 | `		}` |
|     ! 0 | 5367 | `	}` |
|       - | 5368 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5369 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5370 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5371 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5372 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5373 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5374 | `		if( d != (double)i64 ){` |
|       7 | 5375 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5376 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5377 | `				d` |
|       - | 5378 | `				);` |
|       2 | 5379 | `		}` |
|       2 | 5380 | `	}` |
|       - | 5381 |  |
|       - | 5382 | `	/* Total number of entries to insert */` |
|     230 | 5383 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5384 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5385 | `	if( nEntry < 0 ){` |
|       3 | 5386 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5387 | `			"ValueError",` |
|       - | 5388 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5389 | `			);` |
|       - | 5390 | `	}` |
|       - | 5391 |  |
|       - | 5392 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5393 | `	if( nEntry == 0 ){` |
|       7 | 5394 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5395 | `		return PH7_OK;` |
|       - | 5396 | `	}` |
|       - | 5397 |  |
|       - | 5398 | `	/* Create a new array */` |
|     221 | 5399 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5400 | `	if( pArray == 0 ){` |
|     ! 0 | 5401 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5402 | `		return PH7_OK;` |
|       - | 5403 | `	}` |
|       - | 5404 |  |
|       - | 5405 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5406 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 5407 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5408 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5409 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 5410 | `	}` |
|       - | 5411 | `	/* Return the filled array */` |
|     221 | 5412 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5413 | `	return PH7_OK;` |
|     121 | 5414 |  |
|       - | 5415 | `/*` |
|       - | 5416 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5417 | ` *  Fill an array with values, specifying keys.` |
|       - | 5418 | ` * Parameters` |
|       - | 5419 | ` *  $input` |
|       - | 5420 | ` *   Array of values that will be used as key.` |
|       - | 5421 | ` *  $value` |
|       - | 5422 | ` *    Value to use for filling.` |
|       - | 5423 | ` * Return` |
|       - | 5424 | ` *  The filled array.` |
|       - | 5425 | ` * Throws` |
|       - | 5426 | ` *  ValueError if $input is not an array.` |
|       - | 5427 | ` */` |
|      26 | 5428 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5429 |  |
|       - | 5430 | `	ph7_hashmap_node *pEntry;` |
|       - | 5431 | `	ph7_hashmap *pSrc;` |
|       - | 5432 | `	ph7_value *pArray;` |
|       - | 5433 | `	sxu32 n;` |
|       - | 5434 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5435 | `	if( nArg != 2 ){` |
|      10 | 5436 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5437 | `			"ArgumentCountError",` |
|       - | 5438 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5439 | `			nArg` |
|       - | 5440 | `			);` |
|       - | 5441 | `	}` |
|       - | 5442 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5443 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5444 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5445 | `			"TypeError",` |
|       - | 5446 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5447 | `			ph7_type_name(apArg[0])` |
|       - | 5448 | `			);` |
|       - | 5449 | `	}` |
|       - | 5450 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5451 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5452 | `	/* Create a new array */` |
|      17 | 5453 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5454 | `	if( pArray == 0 ){` |
|     ! 0 | 5455 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5456 | `		return PH7_OK;` |
|       - | 5457 | `	}` |
|       - | 5458 | `	/* Perform the requested operation */` |
|      17 | 5459 | `	pEntry = pSrc->pFirst;` |
|      45 | 5460 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5461 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5462 | `		/* Point to the next entry */` |
|      29 | 5463 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5464 | `	}` |
|       - | 5465 | `	/* Return the filled array */` |
|      17 | 5466 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5467 | `	return PH7_OK;` |
|      15 | 5468 |  |
|       - | 5469 | `/*` |
|       - | 5470 | ` * array array_combine(array $keys,array $values)` |
|       - | 5471 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5472 | ` * Parameters` |
|       - | 5473 | ` *  $keys` |
|       - | 5474 | ` *    Array of keys to be used.` |
|       - | 5475 | ` * $values` |
|       - | 5476 | ` *   Array of values to be used.` |
|       - | 5477 | ` * Return` |
|       - | 5478 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5479 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5480 | ` *  not an array.` |
|       - | 5481 | ` */` |
|      18 | 5482 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5483 |  |
|       - | 5484 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5485 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5486 | `	ph7_value *pArray;` |
|       - | 5487 | `	sxu32 n;` |
|       - | 5488 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5489 | `	if( nArg != 2 ){` |
|       - | 5490 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5491 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5492 | `			"ArgumentCountError",` |
|       - | 5493 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5494 | `			nArg` |
|       - | 5495 | `			);` |
|       - | 5496 | `	}` |
|       - | 5497 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5498 | `	 * argument index in the error message. */` |
|      18 | 5499 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5500 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5501 | `			"TypeError",` |
|       - | 5502 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5503 | `			ph7_type_name(apArg[0])` |
|       - | 5504 | `			);` |
|       - | 5505 | `	}` |
|      16 | 5506 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5507 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5508 | `			"TypeError",` |
|       - | 5509 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5510 | `			ph7_type_name(apArg[1])` |
|       - | 5511 | `			);` |
|       - | 5512 | `	}` |
|       - | 5513 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5514 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5515 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5516 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5517 | `		/* Length mismatch -> ValueError */` |
|       3 | 5518 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5519 | `			"ValueError",` |
|       - | 5520 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5521 | `			);` |
|       - | 5522 | `	}` |
|       - | 5523 | `	/* Create a new array */` |
|      11 | 5524 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5525 | `	if( pArray == 0 ){` |
|     ! 0 | 5526 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5527 | `		return PH7_OK;` |
|       - | 5528 | `	}` |
|       - | 5529 | `	/* Perform the requested operation */` |
|      11 | 5530 | `	pKe = pKey->pFirst;` |
|      11 | 5531 | `	pVe = pValue->pFirst;` |
|      33 | 5532 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5533 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5534 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5535 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5536 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5537 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5538 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5539 | `		 * original array must not be mutated. */` |
|      23 | 5540 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5541 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5542 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5543 | `			if( pTmpKey ){` |
|       5 | 5544 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5545 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5546 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5547 | `				pKeyCopy = pTmpKey;` |
|       2 | 5548 | `			}` |
|       2 | 5549 | `		}` |
|      23 | 5550 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5551 | `		/* Point to the next entry */` |
|      23 | 5552 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5553 | `		pVe = pVe->pPrev;` |
|      12 | 5554 | `	}` |
|       - | 5555 | `	/* Return the filled array */` |
|      11 | 5556 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5557 | `	return PH7_OK;` |
|      11 | 5558 |  |
|       - | 5559 | `/*` |
|       - | 5560 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5561 | ` *  Return an array with elements in reverse order.` |
|       - | 5562 | ` * Parameters` |
|       - | 5563 | ` *  $array` |
|       - | 5564 | ` *   The input array.` |
|       - | 5565 | ` *  $preserve_keys (optional)` |
|       - | 5566 | ` *   If set to TRUE keys are preserved.` |
|       - | 5567 | ` * Return` |
|       - | 5568 | ` *  The reversed array.` |
|       - | 5569 | ` */` |
|      20 | 5570 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5571 |  |
|       - | 5572 | `	ph7_hashmap_node *pEntry;` |
|       - | 5573 | `	ph7_hashmap *pSrc;` |
|       - | 5574 | `	ph7_value *pArray;` |
|       - | 5575 | `	int bPreserve;` |
|       - | 5576 | `	sxu32 n;` |
|      22 | 5577 | `	if( nArg < 1 ){` |
|       4 | 5578 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5579 | `			"ArgumentCountError",` |
|       - | 5580 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5581 | `			nArg` |
|       - | 5582 | `			);` |
|       - | 5583 | `	}` |
|       - | 5584 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5585 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5586 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5587 | `			"TypeError",` |
|       - | 5588 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5589 | `			ph7_type_name(apArg[0])` |
|       - | 5590 | `			);` |
|       - | 5591 | `	}` |
|      17 | 5592 | `	bPreserve = FALSE;` |
|      17 | 5593 | `	if( nArg > 1 ){` |
|       7 | 5594 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5595 | `	}` |
|       - | 5596 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5597 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5598 | `	/* Create a new array */` |
|      17 | 5599 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5600 | `	if( pArray == 0 ){` |
|     ! 0 | 5601 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5602 | `		return PH7_OK;` |
|       - | 5603 | `	}` |
|       - | 5604 | `	/* Perform the requested operation */` |
|      17 | 5605 | `	pEntry = pSrc->pLast;` |
|      55 | 5606 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5607 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5608 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5609 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5610 | `		/* Point to the previous entry */` |
|      39 | 5611 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5612 | `	}` |
|      17 | 5613 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5614 | `	return PH7_OK;` |
|      12 | 5615 |  |
|       - | 5616 | `/*` |
|       - | 5617 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5618 | ` *  Removes duplicate values from an array.` |
|       - | 5619 | ` * Parameters` |
|       - | 5620 | ` *  $array` |
|       - | 5621 | ` *   The input array.` |
|       - | 5622 | ` *  $flags` |
|       - | 5623 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5624 | ` *   behavior using these values:` |
|       - | 5625 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5626 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5627 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5628 | ` * Return` |
|       - | 5629 | ` *  The filtered array.` |
|       - | 5630 | ` */` |
|      24 | 5631 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5632 |  |
|       - | 5633 | `	ph7_hashmap_node *pEntry;` |
|       - | 5634 | `	ph7_value *pNeedle;` |
|       - | 5635 | `	ph7_hashmap *pSrc;` |
|       - | 5636 | `	ph7_value *pArray;` |
|       - | 5637 | `	int bStrict;` |
|       - | 5638 | `	sxi32 rc;` |
|       - | 5639 | `	sxu32 n;` |
|      26 | 5640 | `	if( nArg < 1 ){` |
|       - | 5641 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5642 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5643 | `			"ArgumentCountError",` |
|       - | 5644 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5645 | `			);` |
|       - | 5646 | `	}` |
|      24 | 5647 | `	if( nArg > 2 ){` |
|       - | 5648 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5649 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5650 | `			"ArgumentCountError",` |
|       - | 5651 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5652 | `			nArg` |
|       - | 5653 | `			);` |
|       - | 5654 | `	}` |
|       - | 5655 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5656 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5657 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5658 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5659 | `			"TypeError",` |
|       - | 5660 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5661 | `			ph7_type_name(apArg[0])` |
|       - | 5662 | `			);` |
|       - | 5663 | `	}` |
|      19 | 5664 | `	bStrict = FALSE;` |
|       - | 5665 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5666 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5667 | `	/* Create a new array */` |
|      19 | 5668 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5669 | `	if( pArray == 0 ){` |
|     ! 0 | 5670 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5671 | `		return PH7_OK;` |
|       - | 5672 | `	}` |
|       - | 5673 | `	/* Perform the requested operation */` |
|      19 | 5674 | `	pEntry = pSrc->pFirst;` |
|      83 | 5675 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5676 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5677 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5678 | `		if( pNeedle ){` |
|      65 | 5679 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5680 | `		}` |
|      65 | 5681 | `		if( rc != SXRET_OK ){` |
|       - | 5682 | `			/* Perform the insertion */` |
|      37 | 5683 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5684 | `		}` |
|       - | 5685 | `		/* Point to the next entry */` |
|      65 | 5686 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5687 | `	}` |
|       - | 5688 | `	/* Return the freshly created array */` |
|      19 | 5689 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5690 | `	return PH7_OK;` |
|      14 | 5691 |  |
|       - | 5692 | `/*` |
|       - | 5693 | ` * array array_flip(array $input)` |
|       - | 5694 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5695 | ` * Parameter` |
|       - | 5696 | ` *  $input` |
|       - | 5697 | ` *   Input array.` |
|       - | 5698 | ` * Return` |
|       - | 5699 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5700 | ` */` |
|      34 | 5701 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5702 |  |
|       - | 5703 | `	ph7_hashmap_node *pEntry;` |
|       - | 5704 | `	ph7_hashmap *pSrc;` |
|       - | 5705 | `	ph7_value *pArray;` |
|       - | 5706 | `	ph7_value *pKey;` |
|       - | 5707 | `	ph7_value sVal;` |
|       - | 5708 | `	sxu32 n;` |
|       - | 5709 |  |
|       - | 5710 | `	/* PHP requires exactly one argument */` |
|      36 | 5711 | `	if( nArg != 1 ){` |
|       - | 5712 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5713 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5714 | `			"ArgumentCountError",` |
|       - | 5715 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5716 | `			nArg` |
|       - | 5717 | `			);` |
|       - | 5718 | `	}` |
|       - | 5719 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5720 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5721 | `		/* Type mismatch -> TypeError */` |
|       7 | 5722 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5723 | `			"TypeError",` |
|       - | 5724 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5725 | `			ph7_type_name(apArg[0])` |
|       - | 5726 | `			);` |
|       - | 5727 | `	}` |
|       - | 5728 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5729 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5730 | `	/* Create a new array */` |
|      27 | 5731 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5732 | `	if( pArray == 0 ){` |
|     ! 0 | 5733 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5734 | `		return PH7_OK;` |
|       - | 5735 | `	}` |
|       - | 5736 | `	/* Start processing */` |
|      27 | 5737 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5738 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5739 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5740 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5741 | `		if( pKey ){` |
|       - | 5742 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5743 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5744 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5745 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5746 | `					);` |
|   22236 | 5747 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5748 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5749 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5750 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5751 | `				}else{` |
|       - | 5752 | `					SyString sStr;` |
|    2227 | 5753 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5754 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5755 | `				}` |
|       - | 5756 | `				/* Perform the insertion */` |
|   22227 | 5757 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5758 | `				/* Safely release the value because each inserted entry` |
|       - | 5759 | `				 * has its own private copy of the value.` |
|       - | 5760 | `				 */` |
|   22227 | 5761 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5762 | `			}else{` |
|       - | 5763 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5764 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5765 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5766 | `					);` |
|       - | 5767 | `			}` |
|   11118 | 5768 | `		}` |
|       - | 5769 | `		/* Point to the next entry */` |
|   22237 | 5770 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5771 | `	}` |
|       - | 5772 | `	/* Return the freshly created array */` |
|      27 | 5773 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5774 | `	return PH7_OK;` |
|      19 | 5775 |  |
|       - | 5776 | `/*` |
|       - | 5777 | ` * number array_sum(array $array )` |
|       - | 5778 | ` *  Calculate the sum of values in an array.` |
|       - | 5779 | ` * Parameters` |
|       - | 5780 | ` *  $array: The input array.` |
|       - | 5781 | ` * Return` |
|       - | 5782 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5783 | ` */` |
|      24 | 5784 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5785 |  |
|       - | 5786 | `	ph7_hashmap_node *pEntry;` |
|       - | 5787 | `	ph7_value *pObj;` |
|      25 | 5788 | `	double dSum = 0;` |
|       - | 5789 | `	sxu32 n;` |
|      25 | 5790 | `	pEntry = pMap->pFirst;` |
|      91 | 5791 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5792 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5793 | `		if( pObj ){` |
|      67 | 5794 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5795 | `				dSum += pObj->rVal;` |
|      53 | 5796 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5797 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5798 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5799 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5800 | `					double dv = 0;` |
|      13 | 5801 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5802 | `					dSum += dv;` |
|       7 | 5803 | `				}` |
|      12 | 5804 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5805 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5806 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5807 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5808 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5809 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5810 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5811 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5812 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5813 | `			}` |
|       - | 5814 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5815 | `		}` |
|       - | 5816 | `		/* Point to the next entry */` |
|      67 | 5817 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5818 | `	}` |
|       - | 5819 | `	/* Return sum */` |
|      25 | 5820 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5821 |  |
|      26 | 5822 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5823 |  |
|       - | 5824 | `	ph7_hashmap_node *pEntry;` |
|       - | 5825 | `	ph7_value *pObj;` |
|      28 | 5826 | `	sxi64 nSum = 0;` |
|       - | 5827 | `	sxu32 n;` |
|      28 | 5828 | `	pEntry = pMap->pFirst;` |
|     112 | 5829 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      86 | 5830 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      86 | 5831 | `		if( pObj ){` |
|      86 | 5832 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      76 | 5833 | `				nSum += pObj->x.iVal;` |
|      48 | 5834 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5835 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5836 | `					sxi64 nv = 0;` |
|       5 | 5837 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5838 | `					nSum += nv;` |
|       3 | 5839 | `				}` |
|       8 | 5840 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5841 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5842 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5843 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5844 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5845 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5846 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5847 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5848 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5849 | `			}` |
|       - | 5850 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      42 | 5851 | `		}` |
|       - | 5852 | `		/* Point to the next entry */` |
|      86 | 5853 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      44 | 5854 | `	}` |
|       - | 5855 | `	/* Return sum */` |
|      28 | 5856 | `	ph7_result_int64(pCtx,nSum);` |
|      28 | 5857 |  |
|       - | 5858 | `/* number array_sum(array $array )` |
|       - | 5859 | ` * (See block-coment above)` |
|       - | 5860 | ` */` |
|      64 | 5861 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5862 |  |
|       - | 5863 | `	ph7_hashmap_node *pEntry;` |
|       - | 5864 | `	ph7_hashmap *pMap;` |
|       - | 5865 | `	ph7_value *pObj;` |
|      66 | 5866 | `	int useDouble = 0;` |
|       - | 5867 | `	sxu32 n;` |
|       - | 5868 | `	/* PHP requires exactly one argument */` |
|      66 | 5869 | `	if( nArg != 1 ){` |
|       7 | 5870 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5871 | `			"ArgumentCountError",` |
|       - | 5872 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5873 | `			nArg` |
|       - | 5874 | `			);` |
|       - | 5875 | `	}` |
|       - | 5876 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 5877 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5878 | `		/* Type mismatch -> TypeError */` |
|       7 | 5879 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5880 | `			"TypeError",` |
|       - | 5881 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5882 | `			ph7_type_name(apArg[0])` |
|       - | 5883 | `			);` |
|       - | 5884 | `	}` |
|      58 | 5885 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      58 | 5886 | `	if( pMap->nEntry < 1 ){` |
|       - | 5887 | `		/* Nothing to compute,return 0 */` |
|       7 | 5888 | `		ph7_result_int(pCtx,0);` |
|       7 | 5889 | `		return PH7_OK;` |
|       - | 5890 | `	}` |
|       - | 5891 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5892 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5893 | `	 */` |
|      52 | 5894 | `	pEntry = pMap->pFirst;` |
|     144 | 5895 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     118 | 5896 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     118 | 5897 | `		if( pObj ){` |
|     118 | 5898 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5899 | `				useDouble = 1;` |
|      19 | 5900 | `				break;` |
|       - | 5901 | `			}` |
|     100 | 5902 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5903 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5904 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5905 | `				sxu32 i;` |
|      23 | 5906 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5907 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5908 | `						useDouble = 1;` |
|       7 | 5909 | `						break;` |
|       - | 5910 | `					}` |
|       6 | 5911 | `				}` |
|      13 | 5912 | `				if( useDouble ){` |
|       7 | 5913 | `					break;` |
|       - | 5914 | `				}` |
|       3 | 5915 | `			}` |
|      46 | 5916 | `		}` |
|      94 | 5917 | `		pEntry = pEntry->pPrev;` |
|      48 | 5918 | `	}` |
|      52 | 5919 | `	if( useDouble ){` |
|      25 | 5920 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5921 | `	}else{` |
|      28 | 5922 | `		Int64Sum(pCtx,pMap);` |
|       - | 5923 | `	}` |
|      52 | 5924 | `	return PH7_OK;` |
|      34 | 5925 |  |
|       - | 5926 | `/*` |
|       - | 5927 | ` * number array_product(array $array )` |
|       - | 5928 | ` *  Calculate the product of values in an array.` |
|       - | 5929 | ` * Parameters` |
|       - | 5930 | ` *  $array: The input array.` |
|       - | 5931 | ` * Return` |
|       - | 5932 | ` *  Returns the product of values as an integer or float.` |
|       - | 5933 | ` */` |
|     ! 0 | 5934 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5935 |  |
|       - | 5936 | `	ph7_hashmap_node *pEntry;` |
|       - | 5937 | `	ph7_value *pObj;` |
|       - | 5938 | `	double dProd;` |
|       - | 5939 | `	sxu32 n;` |
|     ! 0 | 5940 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5941 | `	dProd = 1;` |
|     ! 0 | 5942 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5943 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5944 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5945 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5946 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5947 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5948 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5949 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5950 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5951 | `					double dv = 0;` |
|     ! 0 | 5952 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5953 | `					dProd *= dv;` |
|     ! 0 | 5954 | `				}` |
|     ! 0 | 5955 | `			}` |
|     ! 0 | 5956 | `		}` |
|       - | 5957 | `		/* Point to the next entry */` |
|     ! 0 | 5958 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5959 | `	}` |
|       - | 5960 | `	/* Return product */` |
|     ! 0 | 5961 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5962 |  |
|     ! 0 | 5963 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5964 |  |
|       - | 5965 | `	ph7_hashmap_node *pEntry;` |
|       - | 5966 | `	ph7_value *pObj;` |
|       - | 5967 | `	sxi64 nProd;` |
|       - | 5968 | `	sxu32 n;` |
|     ! 0 | 5969 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5970 | `	nProd = 1;` |
|     ! 0 | 5971 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5972 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5973 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5974 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5975 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5976 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5977 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5978 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5979 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5980 | `					sxi64 nv = 0;` |
|     ! 0 | 5981 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5982 | `					nProd *= nv;` |
|     ! 0 | 5983 | `				}` |
|     ! 0 | 5984 | `			}` |
|     ! 0 | 5985 | `		}` |
|       - | 5986 | `		/* Point to the next entry */` |
|     ! 0 | 5987 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5988 | `	}` |
|       - | 5989 | `	/* Return product */` |
|     ! 0 | 5990 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5991 |  |
|       - | 5992 | `/* number array_product(array $array )` |
|       - | 5993 | ` * (See block-block comment above)` |
|       - | 5994 | ` */` |
|     ! 0 | 5995 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5996 |  |
|       - | 5997 | `	ph7_hashmap *pMap;` |
|       - | 5998 | `	ph7_value *pObj;` |
|     ! 0 | 5999 | `	if( nArg < 1 ){` |
|       - | 6000 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 6001 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6002 | `		return PH7_OK;` |
|       - | 6003 | `	}` |
|       - | 6004 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 6005 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6006 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 6007 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6008 | `		return PH7_OK;` |
|       - | 6009 | `	}` |
|     ! 0 | 6010 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 6011 | `	if( pMap->nEntry < 1 ){` |
|       - | 6012 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 6013 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6014 | `		return PH7_OK;` |
|       - | 6015 | `	}` |
|       - | 6016 | `	/* If the first element is of type float,then perform floating` |
|       - | 6017 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 6018 | `	 */` |
|     ! 0 | 6019 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 6020 | `	if( pObj == 0 ){` |
|     ! 0 | 6021 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6022 | `		return PH7_OK;` |
|       - | 6023 | `	}` |
|     ! 0 | 6024 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6025 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 6026 | `	}else{` |
|     ! 0 | 6027 | `		Int64Prod(pCtx,pMap);` |
|       - | 6028 | `	}` |
|     ! 0 | 6029 | `	return PH7_OK;` |
|     ! 0 | 6030 |  |
|       - | 6031 | `/*` |
|       - | 6032 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 6033 | ` *  Pick one or more random entries out of an array.` |
|       - | 6034 | ` * Parameters` |
|       - | 6035 | ` * $input` |
|       - | 6036 | ` *  The input array.` |
|       - | 6037 | ` * $num_req` |
|       - | 6038 | ` *  Specifies how many entries you want to pick.` |
|       - | 6039 | ` * Return` |
|       - | 6040 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 6041 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 6042 | ` *  NULL is returned on failure.` |
|       - | 6043 | ` */` |
|       6 | 6044 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6045 |  |
|       - | 6046 | `	ph7_hashmap_node *pNode;` |
|       - | 6047 | `	ph7_hashmap *pMap;` |
|       7 | 6048 | `	int nItem = 1;` |
|       7 | 6049 | `	if( nArg < 1 ){` |
|       - | 6050 | `		/* Missing argument,return NULL */` |
|     ! 0 | 6051 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6052 | `		return PH7_OK;` |
|       - | 6053 | `	}` |
|       - | 6054 | `	/* Make sure we are dealing with an array */` |
|       7 | 6055 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6056 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6057 | `		return PH7_OK;` |
|       - | 6058 | `	}` |
|       - | 6059 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 6060 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 6061 | `	if(pMap->nEntry < 1 ){` |
|       - | 6062 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 6063 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6064 | `		return PH7_OK;` |
|       - | 6065 | `	}` |
|       7 | 6066 | `	if( nArg > 1 ){` |
|       3 | 6067 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 6068 | `	}` |
|       7 | 6069 | `	if( nItem < 2 ){` |
|       - | 6070 | `		sxu32 nEntry;` |
|       - | 6071 | `		/* Select a random number */` |
|       5 | 6072 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 6073 | `		/* Extract the desired entry.` |
|       - | 6074 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 6075 | `		 */` |
|       5 | 6076 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       2 | 6077 | `			pNode = pMap->pLast;` |
|       2 | 6078 | `			nEntry = pMap->nEntry - nEntry;` |
|       2 | 6079 | `			if( nEntry > 1 ){` |
|     ! 0 | 6080 | `				for(;;){` |
|     ! 0 | 6081 | `					if( nEntry == 0 ){` |
|     ! 0 | 6082 | `						break;` |
|       - | 6083 | `					}` |
|       - | 6084 | `					/* Point to the previous entry */` |
|     ! 0 | 6085 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 6086 | `					nEntry--;` |
|     ! 0 | 6087 | `				}` |
|     ! 0 | 6088 | `			}` |
|       1 | 6089 | `		}else{` |
|       4 | 6090 | `			pNode = pMap->pFirst;` |
|       4 | 6091 | `			for(;;){` |
|       6 | 6092 | `				if( nEntry == 0 ){` |
|       4 | 6093 | `					break;` |
|       - | 6094 | `				}` |
|       - | 6095 | `				/* Point to the next entry */` |
|       2 | 6096 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       2 | 6097 | `				nEntry--;` |
|     ! 0 | 6098 | `			}` |
|       - | 6099 | `		}` |
|       5 | 6100 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6101 | `			/* Int key */` |
|       3 | 6102 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6103 | `		}else{` |
|       - | 6104 | `			/* Blob key */` |
|       3 | 6105 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6106 | `		}` |
|       3 | 6107 | `	}else{` |
|       - | 6108 | `		ph7_value sKey,*pArray;` |
|       - | 6109 | `		ph7_hashmap *pDest;` |
|       - | 6110 | `		/* Create a new array */` |
|       3 | 6111 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6112 | `		if( pArray == 0 ){` |
|     ! 0 | 6113 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6114 | `			return PH7_OK;` |
|       - | 6115 | `		}` |
|       - | 6116 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6117 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6118 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6119 | `		/* Copy the first n items */` |
|       3 | 6120 | `		pNode = pMap->pFirst;` |
|       3 | 6121 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6122 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6123 | `		}` |
|       7 | 6124 | `		while( nItem > 0){` |
|       5 | 6125 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6126 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6127 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6128 | `			/* Point to the next entry */` |
|       5 | 6129 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6130 | `			nItem--;` |
|       1 | 6131 | `		}` |
|       - | 6132 | `		/* Shuffle the array */` |
|       3 | 6133 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6134 | `		/* Rehash node */` |
|       3 | 6135 | `		HashmapSortRehash(pDest);` |
|       - | 6136 | `		/* Return the random array */` |
|       3 | 6137 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6138 | `	}` |
|       7 | 6139 | `	return PH7_OK;` |
|       4 | 6140 |  |
|       - | 6141 | `/*` |
|       - | 6142 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6143 | ` *  Split an array into chunks.` |
|       - | 6144 | ` * Parameters` |
|       - | 6145 | ` * $input` |
|       - | 6146 | ` *   The array to work on` |
|       - | 6147 | ` * $size` |
|       - | 6148 | ` *   The size of each chunk` |
|       - | 6149 | ` * $preserve_keys` |
|       - | 6150 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6151 | ` *   the chunk numerically.` |
|       - | 6152 | ` * Return` |
|       - | 6153 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6154 | ` *  zero, with each dimension containing size elements.` |
|       - | 6155 | ` */` |
|      42 | 6156 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6157 |  |
|       - | 6158 | `	ph7_value *pArray,*pChunk;` |
|       - | 6159 | `	ph7_hashmap_node *pEntry;` |
|       - | 6160 | `	ph7_hashmap *pMap;` |
|       - | 6161 | `	int bPreserve;` |
|       - | 6162 | `	sxu32 nChunk;` |
|       - | 6163 | `	sxu32 nSize;` |
|       - | 6164 | `	sxu32 n;` |
|       - | 6165 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 6166 | `	if( nArg < 2 ){` |
|       - | 6167 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6168 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6169 | `			"ArgumentCountError",` |
|       - | 6170 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6171 | `			nArg` |
|       - | 6172 | `			);` |
|       - | 6173 | `	}` |
|      42 | 6174 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6175 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6176 | `			"TypeError",` |
|       - | 6177 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6178 | `			ph7_type_name(apArg[0])` |
|       - | 6179 | `			);` |
|       - | 6180 | `	}` |
|       - | 6181 | `	/* Create a new array */` |
|      40 | 6182 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 6183 | `	if( pArray == 0 ){` |
|     ! 0 | 6184 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6185 | `		return PH7_OK;` |
|       - | 6186 | `	}` |
|       - | 6187 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 6188 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6189 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6190 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 6191 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 6192 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6193 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6194 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6195 | `			"TypeError",` |
|       - | 6196 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6197 | `			ph7_type_name(apArg[1])` |
|       - | 6198 | `			);` |
|       - | 6199 | `	}` |
|       - | 6200 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6201 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6202 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 6203 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6204 | `		int len;` |
|       3 | 6205 | `		sxu8 bReal = FALSE;` |
|       3 | 6206 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6207 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6208 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6209 | `				"TypeError",` |
|       - | 6210 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6211 | `				);` |
|       - | 6212 | `		}` |
|     ! 0 | 6213 | `		if( bReal ){` |
|       - | 6214 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6215 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6216 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6217 | `				zStr` |
|       - | 6218 | `				);` |
|     ! 0 | 6219 | `		}` |
|     ! 0 | 6220 | `	}` |
|       - | 6221 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6222 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6223 | `	 * later via ph7_value_to_int. */` |
|      38 | 6224 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6225 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6226 | `		sxi64 i = (sxi64)d;` |
|       3 | 6227 | `		if( d != (double)i ){` |
|       4 | 6228 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6229 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6230 | `				d` |
|       - | 6231 | `				);` |
|       1 | 6232 | `		}` |
|       1 | 6233 | `	}` |
|       - | 6234 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6235 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6236 | `	{` |
|      38 | 6237 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 6238 | `		if( nSizeSigned < 1 ){` |
|       - | 6239 | `			/* size <= 0 -> ValueError */` |
|       5 | 6240 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6241 | `				"ValueError",` |
|       - | 6242 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6243 | `				);` |
|       - | 6244 | `		}` |
|      34 | 6245 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6246 | `	}` |
|      34 | 6247 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6248 | `		/* Return the whole array */` |
|       3 | 6249 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6250 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6251 | `		return PH7_OK;` |
|       - | 6252 | `	}` |
|      32 | 6253 | `	bPreserve = 0;` |
|      32 | 6254 | `	if( nArg > 2 ){` |
|       - | 6255 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6256 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6257 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6258 | `		 * normally, matching PHP behaviour. */` |
|      45 | 6259 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 6260 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6261 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 6262 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6263 | `				"TypeError",` |
|       - | 6264 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6265 | `				ph7_type_name(apArg[2])` |
|       - | 6266 | `				);` |
|       - | 6267 | `		}` |
|      21 | 6268 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6269 | `	}` |
|       - | 6270 | `	/* Start processing */` |
|      27 | 6271 | `	pEntry = pMap->pFirst;` |
|      27 | 6272 | `	nChunk = 0;` |
|      27 | 6273 | `	pChunk = 0;` |
|      27 | 6274 | `	n = pMap->nEntry;` |
|      56 | 6275 | `	for( ;; ){` |
|     113 | 6276 | `		if( n < 1 ){` |
|       - | 6277 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6278 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6279 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6280 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6281 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6282 | `			 * exists. */` |
|      27 | 6283 | `			if( pChunk ){` |
|      27 | 6284 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6285 | `			}` |
|      27 | 6286 | `			break;` |
|       - | 6287 | `		}` |
|      87 | 6288 | `		if( nChunk < 1 ){` |
|      71 | 6289 | `			if( pChunk ){` |
|       - | 6290 | `				/* Put the first chunk */` |
|      45 | 6291 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6292 | `			}` |
|       - | 6293 | `			/* Create a new dimension */` |
|      71 | 6294 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6295 | `												   * will be automatically released as soon we return` |
|       - | 6296 | `												   * from this function */` |
|      71 | 6297 | `			if( pChunk == 0 ){` |
|     ! 0 | 6298 | `				break;` |
|       - | 6299 | `			}` |
|      71 | 6300 | `			nChunk = nSize;` |
|      35 | 6301 | `		}` |
|       - | 6302 | `		/* Insert the entry */` |
|      87 | 6303 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6304 | `		/* Point to the next entry */` |
|      87 | 6305 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6306 | `		nChunk--;` |
|      87 | 6307 | `		n--;` |
|       1 | 6308 | `	}` |
|       - | 6309 | `	/* Return the multidimensional array */` |
|      27 | 6310 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6311 | `	return PH7_OK;` |
|      23 | 6312 |  |
|       - | 6313 | `/*` |
|       - | 6314 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6315 | ` *  Pad array to the specified length with a value.` |
|       - | 6316 | ` * $input` |
|       - | 6317 | ` *   Initial array of values to pad.` |
|       - | 6318 | ` * $pad_size` |
|       - | 6319 | ` *   New size of the array.` |
|       - | 6320 | ` * $pad_value` |
|       - | 6321 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6322 | ` */` |
|      28 | 6323 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6324 |  |
|       - | 6325 | `	ph7_hashmap *pMap;` |
|       - | 6326 | `	ph7_value *pArray;` |
|       - | 6327 | `	int nEntry;` |
|      30 | 6328 | `	if( nArg != 3 ){` |
|      10 | 6329 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6330 | `			"ArgumentCountError",` |
|       - | 6331 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6332 | `			nArg` |
|       - | 6333 | `			);` |
|       - | 6334 | `	}` |
|      24 | 6335 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6336 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6337 | `			"TypeError",` |
|       - | 6338 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6339 | `			ph7_type_name(apArg[0])` |
|       - | 6340 | `			);` |
|       - | 6341 | `	}` |
|       - | 6342 | `	/* Create a new array */` |
|      21 | 6343 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6344 | `	if( pArray == 0 ){` |
|     ! 0 | 6345 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6346 | `		return PH7_OK;` |
|       - | 6347 | `	}` |
|       - | 6348 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6349 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6350 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6351 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6352 | `	if( nEntry < 0 ){` |
|       9 | 6353 | `		nEntry = -nEntry;` |
|       9 | 6354 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6355 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6356 | `			/* Insert given items first */` |
|      17 | 6357 | `			while( nEntry > 0 ){` |
|      13 | 6358 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 6359 | `				nEntry--;` |
|       1 | 6360 | `			}` |
|       - | 6361 | `			/* Merge the two arrays */` |
|       5 | 6362 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6363 | `		}else{` |
|       5 | 6364 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6365 | `		}` |
|      17 | 6366 | `	}else if( nEntry > 0 ){` |
|      11 | 6367 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6368 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6369 | `			/* Merge the two arrays first */` |
|       7 | 6370 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6371 | `			/* Insert given items */` |
|      25 | 6372 | `			while( nEntry > 0 ){` |
|      19 | 6373 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 6374 | `				nEntry--;` |
|       1 | 6375 | `			}` |
|       4 | 6376 | `		}else{` |
|       5 | 6377 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6378 | `		}` |
|       6 | 6379 | `	}else{` |
|       - | 6380 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6381 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6382 | `	}` |
|       - | 6383 | `	/* Return the new array */` |
|      21 | 6384 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6385 | `	return PH7_OK;` |
|      16 | 6386 |  |
|       - | 6387 | `/*` |
|       - | 6388 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6389 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6390 | ` * Parameters` |
|       - | 6391 | ` * $array` |
|       - | 6392 | ` *   The array in which elements are replaced.` |
|       - | 6393 | ` * $array1` |
|       - | 6394 | ` *   The array from which elements will be extracted.` |
|       - | 6395 | ` * ....` |
|       - | 6396 | ` *  More arrays from which elements will be extracted.` |
|       - | 6397 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6398 | ` * Return` |
|       - | 6399 | ` *  Returns an array.` |
|       - | 6400 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6401 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6402 | ` */` |
|      22 | 6403 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6404 |  |
|       - | 6405 | `	ph7_hashmap *pMap;` |
|       - | 6406 | `	ph7_value *pArray;` |
|       - | 6407 | `	int i;` |
|      24 | 6408 | `	if( nArg < 1 ){` |
|       3 | 6409 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6410 | `			"ArgumentCountError",` |
|       - | 6411 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6412 | `			);` |
|       - | 6413 | `	}` |
|      22 | 6414 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6415 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6416 | `			"TypeError",` |
|       - | 6417 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6418 | `			ph7_type_name(apArg[0])` |
|       - | 6419 | `			);` |
|       - | 6420 | `	}` |
|       - | 6421 | `	/* Create a new array */` |
|      20 | 6422 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6423 | `	if( pArray == 0 ){` |
|     ! 0 | 6424 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6425 | `		return PH7_OK;` |
|       - | 6426 | `	}` |
|       - | 6427 | `	/* Overwrite from the first array */` |
|      20 | 6428 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6429 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6430 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6431 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6432 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6433 | `			/* Type mismatch -> TypeError */` |
|       4 | 6434 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6435 | `				"TypeError",` |
|       - | 6436 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6437 | `				i + 1,` |
|       2 | 6438 | `				ph7_type_name(apArg[i])` |
|       - | 6439 | `				);` |
|       - | 6440 | `		}` |
|       - | 6441 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6442 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6443 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6444 | `	}` |
|       - | 6445 | `	/* Return the new array */` |
|      17 | 6446 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6447 | `	return PH7_OK;` |
|      13 | 6448 |  |
|       - | 6449 | `/*` |
|       - | 6450 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6451 | ` *  Filters elements of an array using a callback function.` |
|       - | 6452 | ` * Parameters` |
|       - | 6453 | ` *  $input` |
|       - | 6454 | ` *    The array to iterate over` |
|       - | 6455 | ` * $callback` |
|       - | 6456 | ` *    The callback function to use` |
|       - | 6457 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6458 | ` *    will be removed.` |
|       - | 6459 | ` * Return` |
|       - | 6460 | ` *  The filtered array.` |
|       - | 6461 | ` */` |
|      20 | 6462 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6463 |  |
|       - | 6464 | `	ph7_hashmap_node *pEntry;` |
|       - | 6465 | `	ph7_hashmap *pMap;` |
|       - | 6466 | `	ph7_value *pArray;` |
|       - | 6467 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6468 | `	ph7_value *pValue;` |
|       - | 6469 | `	sxi32 rc;` |
|       - | 6470 | `	int keep;` |
|       - | 6471 | `	sxu32 n;` |
|      22 | 6472 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6473 | `		/* Invalid arguments,return NULL */` |
|       5 | 6474 | `		ph7_result_null(pCtx);` |
|       5 | 6475 | `		return PH7_OK;` |
|       - | 6476 | `	}` |
|       - | 6477 | `	/* Create a new array */` |
|      18 | 6478 | `	pArray = ph7_context_new_array(pCtx);` |
|      18 | 6479 | `	if( pArray == 0 ){` |
|     ! 0 | 6480 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6481 | `		return PH7_OK;` |
|       - | 6482 | `	}` |
|       - | 6483 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 6484 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      18 | 6485 | `	pEntry = pMap->pFirst;` |
|      18 | 6486 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      18 | 6487 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6488 | `	/* Perform the requested operation */` |
|      68 | 6489 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6490 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      56 | 6491 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      56 | 6492 | `		if( pValue == 0 ){` |
|       - | 6493 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6494 | `			keep = FALSE;` |
|      56 | 6495 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6496 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6497 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6498 | `				* silently dropped the element.  Emit similar message. */` |
|      28 | 6499 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6500 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6501 | `					int len;` |
|       3 | 6502 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6503 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6504 | `						"TypeError",` |
|       - | 6505 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6506 | `						zName` |
|       - | 6507 | `						);` |
|     ! 0 | 6508 | `				}else{` |
|     ! 0 | 6509 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6510 | `						"TypeError",` |
|       - | 6511 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6512 | `						ph7_type_name(apArg[1])` |
|       - | 6513 | `						);` |
|       - | 6514 | `				}` |
|       - | 6515 | `			}` |
|      25 | 6516 | `			keep = FALSE;` |
|      25 | 6517 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      25 | 6518 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 6519 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6520 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 6521 | `				return PH7_EXCEPTION;` |
|       - | 6522 | `			}` |
|      23 | 6523 | `			if( rc == SXRET_OK ){` |
|       - | 6524 | `				/* Perform a boolean cast */` |
|      23 | 6525 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6526 | `			}` |
|      23 | 6527 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6528 | `		}else{` |
|       - | 6529 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6530 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6531 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6532 | `			 */` |
|      29 | 6533 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6534 | `		}` |
|      51 | 6535 | `		if( keep ){` |
|       - | 6536 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6537 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6538 | `		}` |
|       - | 6539 | `		/* Point to the next entry */` |
|      51 | 6540 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6541 | `	}` |
|      13 | 6542 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6543 | `	return PH7_OK;` |
|      12 | 6544 |  |
|       - | 6545 | `/*` |
|       - | 6546 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 6547 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 6548 | ` * Parameters` |
|       - | 6549 | ` *  $callback` |
|       - | 6550 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 6551 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 6552 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 6553 | ` *   are zipped together.` |
|       - | 6554 | ` *  $array` |
|       - | 6555 | ` *   The first array to run through the callback function.` |
|       - | 6556 | ` *  $arrays` |
|       - | 6557 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 6558 | ` * Return` |
|       - | 6559 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 6560 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 6561 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 6562 | ` *  padding shorter arrays with NULL.` |
|       - | 6563 | ` */` |
|      46 | 6564 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6565 |  |
|       - | 6566 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6567 | `	ph7_hashmap_node *pEntry;` |
|       - | 6568 | `	ph7_hashmap *pMap;` |
|       - | 6569 | `	ph7_vm *pVm;` |
|       - | 6570 | `	int bNullCallback;` |
|       - | 6571 | `	sxi32 rc;` |
|       - | 6572 | `	int i;` |
|       - | 6573 | `	sxu32 n;` |
|      48 | 6574 | `	if( nArg < 2 ){` |
|       7 | 6575 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6576 | `			"ArgumentCountError",` |
|       - | 6577 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6578 | `			nArg` |
|       - | 6579 | `			);` |
|       - | 6580 | `	}` |
|      44 | 6581 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      44 | 6582 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6583 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6584 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6585 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6586 | `				"TypeError",` |
|       - | 6587 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6588 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6589 | `				zFunc` |
|       - | 6590 | `				);` |
|       - | 6591 | `		}` |
|       3 | 6592 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6593 | `			"TypeError",` |
|       - | 6594 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6595 | `			"no array or string given"` |
|       - | 6596 | `			);` |
|       - | 6597 | `	}` |
|       - | 6598 | `	/* Every remaining argument must be an array */` |
|      88 | 6599 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      52 | 6600 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 6601 | `			if( i == 1 ){` |
|       4 | 6602 | `				return PH7_VmThrowException(pCtx,` |
|       - | 6603 | `					"TypeError",` |
|       - | 6604 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6605 | `					ph7_type_name(apArg[1])` |
|       - | 6606 | `					);` |
|       - | 6607 | `			}` |
|     ! 0 | 6608 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6609 | `				"TypeError",` |
|       - | 6610 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 6611 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 6612 | `				);` |
|       - | 6613 | `		}` |
|      26 | 6614 | `	}` |
|      38 | 6615 | `	pVm = pCtx->pVm;` |
|       - | 6616 | `	/* Create a new array */` |
|      38 | 6617 | `	pArray = ph7_context_new_array(pCtx);` |
|      38 | 6618 | `	if( pArray == 0 ){` |
|     ! 0 | 6619 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6620 | `		return PH7_OK;` |
|       - | 6621 | `	}` |
|      38 | 6622 | `	PH7_MemObjInit(pVm,&sResult);` |
|      38 | 6623 | `	PH7_MemObjInit(pVm,&sKey);` |
|      38 | 6624 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      38 | 6625 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      38 | 6626 | `	if( nArg == 2 ){` |
|       - | 6627 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      28 | 6628 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      28 | 6629 | `		pEntry = pMap->pFirst;` |
|      82 | 6630 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6631 | `			/* Extract the node value */` |
|      58 | 6632 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|      58 | 6633 | `			if( pValue ){` |
|       - | 6634 | `				/* Extract the node key */` |
|      58 | 6635 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      58 | 6636 | `				if( bNullCallback ){` |
|       - | 6637 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 6638 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6639 | `				}else{` |
|       - | 6640 | `					/* Invoke the supplied callback */` |
|      48 | 6641 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|      48 | 6642 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 6643 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 6644 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       3 | 6645 | `						PH7_MemObjRelease(&sKey);` |
|       3 | 6646 | `						PH7_MemObjRelease(&sResult);` |
|       3 | 6647 | `						return PH7_EXCEPTION;` |
|       - | 6648 | `					}` |
|       - | 6649 | `					/* Insert the callback return value */` |
|      46 | 6650 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6651 | `				}` |
|      56 | 6652 | `				PH7_MemObjRelease(&sKey);` |
|      56 | 6653 | `				PH7_MemObjRelease(&sResult);` |
|      27 | 6654 | `			}` |
|       - | 6655 | `			/* Point to the next entry */` |
|      56 | 6656 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 6657 | `		}` |
|      14 | 6658 | `	}else{` |
|       - | 6659 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 6660 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 6661 | `		int nArrays = nArg - 1;` |
|       - | 6662 | `		ph7_hashmap_node **apCur;` |
|       - | 6663 | `		ph7_value **apCallArg;` |
|       - | 6664 | `		ph7_value sNull;` |
|      11 | 6665 | `		sxu32 nMax = 0;` |
|      11 | 6666 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 6667 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 6668 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 6669 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 6670 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 6671 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6672 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6673 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 6674 | `			return PH7_OK;` |
|       - | 6675 | `		}` |
|      11 | 6676 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 6677 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 6678 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 6679 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 6680 | `			apCur[i] = pMap->pFirst;` |
|      23 | 6681 | `			if( pMap->nEntry > nMax ){` |
|      13 | 6682 | `				nMax = pMap->nEntry;` |
|       6 | 6683 | `			}` |
|      12 | 6684 | `		}` |
|      35 | 6685 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 6686 | `			ph7_value *pZip = 0;` |
|      25 | 6687 | `			if( bNullCallback ){` |
|       - | 6688 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 6689 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 6690 | `			}` |
|      79 | 6691 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 6692 | `				ph7_value *pv = &sNull;` |
|      55 | 6693 | `				if( apCur[i] ){` |
|      53 | 6694 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 6695 | `					if( pNodeVal ){` |
|      53 | 6696 | `						pv = pNodeVal;` |
|      26 | 6697 | `					}` |
|      53 | 6698 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 6699 | `				}` |
|      55 | 6700 | `				if( bNullCallback ){` |
|       9 | 6701 | `					if( pZip ){` |
|       9 | 6702 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 6703 | `					}` |
|       5 | 6704 | `				}else{` |
|      47 | 6705 | `					apCallArg[i] = pv;` |
|       - | 6706 | `				}` |
|      28 | 6707 | `			}` |
|      25 | 6708 | `			if( bNullCallback ){` |
|       5 | 6709 | `				if( pZip ){` |
|       5 | 6710 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 6711 | `				}` |
|       3 | 6712 | `			}else{` |
|      21 | 6713 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 6714 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6715 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 6716 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 6717 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 6718 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6719 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6720 | `					return PH7_EXCEPTION;` |
|       - | 6721 | `				}` |
|      21 | 6722 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 6723 | `				PH7_MemObjRelease(&sResult);` |
|       - | 6724 | `			}` |
|      13 | 6725 | `		}` |
|      11 | 6726 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 6727 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 6728 | `		PH7_MemObjRelease(&sNull);` |
|       - | 6729 | `	}` |
|      36 | 6730 | `	PH7_MemObjRelease(&sKey);` |
|      36 | 6731 | `	PH7_MemObjRelease(&sResult);` |
|      36 | 6732 | `	ph7_result_value(pCtx,pArray);` |
|      36 | 6733 | `	return PH7_OK;` |
|      25 | 6734 |  |
|       - | 6735 | `/*` |
|       - | 6736 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6737 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6738 | ` * Parameters` |
|       - | 6739 | ` *  $array` |
|       - | 6740 | ` *   The input array.` |
|       - | 6741 | ` *  $callback` |
|       - | 6742 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6743 | ` *  $initial` |
|       - | 6744 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6745 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6746 | ` * Return` |
|       - | 6747 | ` *  Returns the resulting value.` |
|       - | 6748 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6749 | ` */` |
|      32 | 6750 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6751 |  |
|       - | 6752 | `	ph7_hashmap_node *pEntry;` |
|       - | 6753 | `	ph7_hashmap *pMap;` |
|       - | 6754 | `	ph7_value *pValue;` |
|       - | 6755 | `	ph7_value sResult;` |
|       - | 6756 | `	sxi32 rc;` |
|       - | 6757 | `	sxu32 n;` |
|      34 | 6758 | `	if( nArg < 2 ){` |
|       7 | 6759 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6760 | `			"ArgumentCountError",` |
|       - | 6761 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6762 | `			nArg` |
|       - | 6763 | `			);` |
|       - | 6764 | `	}` |
|      30 | 6765 | `	if( nArg > 3 ){` |
|       4 | 6766 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6767 | `			"ArgumentCountError",` |
|       - | 6768 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6769 | `			nArg` |
|       - | 6770 | `			);` |
|       - | 6771 | `	}` |
|      28 | 6772 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6773 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6774 | `			"TypeError",` |
|       - | 6775 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6776 | `			ph7_type_name(apArg[0])` |
|       - | 6777 | `			);` |
|       - | 6778 | `	}` |
|      26 | 6779 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6780 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6781 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6782 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6783 | `				"TypeError",` |
|       - | 6784 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6785 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6786 | `				zFunc` |
|       - | 6787 | `				);` |
|       - | 6788 | `		}` |
|       7 | 6789 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6790 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6791 | `				"TypeError",` |
|       - | 6792 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6793 | `				"array callback must have exactly two members"` |
|       - | 6794 | `				);` |
|       - | 6795 | `		}` |
|       5 | 6796 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6797 | `			"TypeError",` |
|       - | 6798 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6799 | `			"no array or string given"` |
|       - | 6800 | `			);` |
|       - | 6801 | `	}` |
|       - | 6802 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 6803 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6804 | `	/* Assume a NULL initial value */` |
|      17 | 6805 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      17 | 6806 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      17 | 6807 | `	if( nArg > 2 ){` |
|       - | 6808 | `		/* Set the initial value */` |
|      11 | 6809 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6810 | `	}` |
|       - | 6811 | `	/* Perform the requested operation */` |
|      17 | 6812 | `	pEntry = pMap->pFirst;` |
|      45 | 6813 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6814 | `		/* Extract the node value */` |
|      31 | 6815 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6816 | `		/* Invoke the supplied callback */` |
|      31 | 6817 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      31 | 6818 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 6819 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6820 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 6821 | `			return PH7_EXCEPTION;` |
|       - | 6822 | `		}` |
|       - | 6823 | `		/* Point to the next entry */` |
|      29 | 6824 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6825 | `	}` |
|      15 | 6826 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6827 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6828 | `	return PH7_OK;` |
|      18 | 6829 |  |
|       - | 6830 | `/*` |
|       - | 6831 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6832 | ` *  Apply a user function to every member of an array.` |
|       - | 6833 | ` * Parameters` |
|       - | 6834 | ` *  $array` |
|       - | 6835 | ` *   The input array.` |
|       - | 6836 | ` *  $funcname` |
|       - | 6837 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6838 | ` *   the first, and the key/index second.` |
|       - | 6839 | ` * Note:` |
|       - | 6840 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6841 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6842 | ` *  be made in the original array itself.` |
|       - | 6843 | ` *  $userdata` |
|       - | 6844 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6845 | ` *   to the callback funcname.` |
|       - | 6846 | ` * Return` |
|       - | 6847 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6848 | ` */` |
|      38 | 6849 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6850 |  |
|       - | 6851 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6852 | `	ph7_hashmap_node *pEntry;` |
|       - | 6853 | `	ph7_hashmap *pMap;` |
|       - | 6854 | `	sxu32 n;` |
|      40 | 6855 | `	if( nArg < 2 ){` |
|       7 | 6856 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6857 | `			"ArgumentCountError",` |
|       - | 6858 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6859 | `			nArg` |
|       - | 6860 | `			);` |
|       - | 6861 | `	}` |
|      36 | 6862 | `	if( nArg > 3 ){` |
|       4 | 6863 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6864 | `			"ArgumentCountError",` |
|       - | 6865 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6866 | `			nArg` |
|       - | 6867 | `			);` |
|       - | 6868 | `	}` |
|      34 | 6869 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6870 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6871 | `			"TypeError",` |
|       - | 6872 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6873 | `			ph7_type_name(apArg[0])` |
|       - | 6874 | `			);` |
|       - | 6875 | `	}` |
|      32 | 6876 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6877 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6878 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6879 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6880 | `				"TypeError",` |
|       - | 6881 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6882 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6883 | `				zFunc` |
|       - | 6884 | `				);` |
|       - | 6885 | `		}` |
|       9 | 6886 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6887 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6888 | `				"TypeError",` |
|       - | 6889 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6890 | `				"array callback must have exactly two members"` |
|       - | 6891 | `				);` |
|       - | 6892 | `		}` |
|       5 | 6893 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6894 | `			"TypeError",` |
|       - | 6895 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6896 | `			"no array or string given"` |
|       - | 6897 | `			);` |
|       - | 6898 | `	}` |
|      21 | 6899 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6900 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6901 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 6902 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 6903 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 6904 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6905 | `	/* Perform the desired operation */` |
|      21 | 6906 | `	pEntry = pMap->pFirst;` |
|      61 | 6907 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6908 | `		/* Extract the node value */` |
|      43 | 6909 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 6910 | `		if( pValue ){` |
|       - | 6911 | `			sxi32 rcW;` |
|       - | 6912 | `			/* Extract the entry key */` |
|      43 | 6913 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6914 | `			/* Invoke the supplied callback */` |
|      43 | 6915 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 6916 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 6917 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 6918 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6919 | `				return PH7_EXCEPTION;` |
|       - | 6920 | `			}` |
|      20 | 6921 | `		}` |
|       - | 6922 | `		/* Point to the next entry */` |
|      41 | 6923 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6924 | `	}` |
|       - | 6925 | `	/* All done, return TRUE */` |
|      19 | 6926 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6927 | `	return PH7_OK;` |
|      21 | 6928 |  |
|       - | 6929 | `/*` |
|       - | 6930 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6931 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6932 | ` */` |
|      22 | 6933 | `static sxi32 HashmapWalkRecursive(` |
|       - | 6934 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6935 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6936 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6937 | `	int iNest             /* Nesting level */` |
|       - | 6938 | `	)` |
|       1 | 6939 |  |
|       - | 6940 | `	ph7_hashmap_node *pEntry;` |
|       - | 6941 | `	ph7_value *pValue,sKey;` |
|       - | 6942 | `	sxi32 rc;` |
|       - | 6943 | `	sxu32 n;` |
|       - | 6944 | `	/* Iterate through hashmap entries */` |
|      23 | 6945 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6946 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6947 | `	pEntry = pMap->pFirst;` |
|      59 | 6948 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6949 | `		/* Extract the node value */` |
|      37 | 6950 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6951 | `		if( pValue ){` |
|      37 | 6952 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6953 | `				if( iNest < 32 ){` |
|       - | 6954 | `					/* Recurse */` |
|      11 | 6955 | `					iNest++;` |
|      11 | 6956 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6957 | `					iNest--;` |
|      11 | 6958 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6959 | `						return PH7_EXCEPTION;` |
|       - | 6960 | `					}` |
|       5 | 6961 | `				}` |
|       6 | 6962 | `			}else{` |
|       - | 6963 | `				/* Extract the node key */` |
|      27 | 6964 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6965 | `				/* Invoke the supplied callback */` |
|      27 | 6966 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6967 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 6968 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 6969 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 6970 | `					return PH7_EXCEPTION;` |
|       - | 6971 | `				}` |
|       - | 6972 | `			}` |
|      18 | 6973 | `		}` |
|       - | 6974 | `		/* Point to the next entry */` |
|      37 | 6975 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6976 | `	}` |
|      23 | 6977 | `	return PH7_OK;` |
|      12 | 6978 |  |
|       - | 6979 | `/*` |
|       - | 6980 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6981 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6982 | ` * Parameters` |
|       - | 6983 | ` *  $array` |
|       - | 6984 | ` *   The input array.` |
|       - | 6985 | ` *  $funcname` |
|       - | 6986 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6987 | ` *   the first, and the key/index second.` |
|       - | 6988 | ` * Note:` |
|       - | 6989 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6990 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6991 | ` *  be made in the original array itself.` |
|       - | 6992 | ` *  $userdata` |
|       - | 6993 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6994 | ` *   to the callback funcname.` |
|       - | 6995 | ` * Return` |
|       - | 6996 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6997 | ` */` |
|      30 | 6998 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6999 |  |
|       - | 7000 | `	ph7_hashmap *pMap;` |
|      32 | 7001 | `	if( nArg < 2 ){` |
|       7 | 7002 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7003 | `			"ArgumentCountError",` |
|       - | 7004 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 7005 | `			nArg` |
|       - | 7006 | `			);` |
|       - | 7007 | `	}` |
|      28 | 7008 | `	if( nArg > 3 ){` |
|       4 | 7009 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7010 | `			"ArgumentCountError",` |
|       - | 7011 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 7012 | `			nArg` |
|       - | 7013 | `			);` |
|       - | 7014 | `	}` |
|      26 | 7015 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7016 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7017 | `			"TypeError",` |
|       - | 7018 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7019 | `			ph7_type_name(apArg[0])` |
|       - | 7020 | `			);` |
|       - | 7021 | `	}` |
|      24 | 7022 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 7023 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7024 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7025 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7026 | `				"TypeError",` |
|       - | 7027 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7028 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7029 | `				zFunc` |
|       - | 7030 | `				);` |
|       - | 7031 | `		}` |
|       9 | 7032 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 7033 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7034 | `				"TypeError",` |
|       - | 7035 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7036 | `				"array callback must have exactly two members"` |
|       - | 7037 | `				);` |
|       - | 7038 | `		}` |
|       5 | 7039 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7040 | `			"TypeError",` |
|       - | 7041 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7042 | `			"no array or string given"` |
|       - | 7043 | `			);` |
|       - | 7044 | `	}` |
|       - | 7045 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 7046 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 7047 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7048 | `	/* Perform the desired operation */` |
|      13 | 7049 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 7050 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7051 | `		return PH7_EXCEPTION;` |
|       - | 7052 | `	}` |
|       - | 7053 | `	/* All done, return TRUE */` |
|      13 | 7054 | `	ph7_result_bool(pCtx,1);` |
|      13 | 7055 | `	return PH7_OK;` |
|      17 | 7056 |  |
|       - | 7057 | `/*` |
|       - | 7058 | ` * Table of hashmap functions.` |
|       - | 7059 | ` */` |
|       - | 7060 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 7061 | `	{"count",             ph7_hashmap_count },` |
|       - | 7062 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 7063 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 7064 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 7065 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 7066 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 7067 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 7068 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 7069 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 7070 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 7071 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 7072 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 7073 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 7074 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 7075 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 7076 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 7077 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 7078 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 7079 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 7080 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 7081 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 7082 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 7083 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 7084 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 7085 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 7086 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 7087 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 7088 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 7089 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 7090 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 7091 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 7092 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 7093 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 7094 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 7095 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 7096 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 7097 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 7098 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 7099 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 7100 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 7101 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 7102 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 7103 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 7104 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 7105 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 7106 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 7107 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 7108 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 7109 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 7110 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 7111 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 7112 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 7113 | `	{"range",             ph7_hashmap_range   },` |
|       - | 7114 | `	{"current",           ph7_hashmap_current },` |
|       - | 7115 | `	{"each",              ph7_hashmap_each    },` |
|       - | 7116 | `	{"pos",               ph7_hashmap_current },` |
|       - | 7117 | `	{"next",              ph7_hashmap_next    },` |
|       - | 7118 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 7119 | `	{"end",               ph7_hashmap_end     },` |
|       - | 7120 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 7121 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 7122 | `};` |
|       - | 7123 | `/*` |
|       - | 7124 | ` * Register the built-in hashmap functions defined above.` |
|       - | 7125 | ` */` |
|    2820 | 7126 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 7127 |  |
|       - | 7128 | `	sxu32 n;` |
|  174842 | 7129 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  172022 | 7130 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   86012 | 7131 | `	}` |
|    2822 | 7132 |  |
|       - | 7133 | `/*` |
|       - | 7134 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 7135 | ` * the BLOB given as the first argument.` |
|       - | 7136 | ` * This function is typically invoked when the user issue a call to` |
|       - | 7137 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 7138 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 7139 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 7140 | ` */` |
|      26 | 7141 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 7142 |  |
|       - | 7143 | `	ph7_hashmap_node *pEntry;` |
|       - | 7144 | `	ph7_value *pObj;` |
|      28 | 7145 | `	sxu32 n = 0;` |
|       - | 7146 | `	int isRef;` |
|       - | 7147 | `	sxi32 rc;` |
|       - | 7148 | `	int i;` |
|      28 | 7149 | `	if( nDepth > 31 ){` |
|       - | 7150 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 7151 | `		/* Nesting limit reached */` |
|     ! 0 | 7152 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 7153 | `		if( ShowType ){` |
|     ! 0 | 7154 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 7155 | `		}` |
|     ! 0 | 7156 | `		return SXERR_LIMIT;` |
|       - | 7157 | `	}` |
|       - | 7158 | `	/* Point to the first inserted entry */` |
|      28 | 7159 | `	pEntry = pMap->pFirst;` |
|      28 | 7160 | `	rc = SXRET_OK;` |
|      28 | 7161 | `	if( !ShowType ){` |
|      15 | 7162 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 7163 | `	}` |
|       - | 7164 | `	/* Total entries */` |
|      28 | 7165 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 7166 | `#ifdef __WINNT__` |
|       2 | 7167 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7168 | `#else` |
|      26 | 7169 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7170 | `#endif` |
|      62 | 7171 | `	for(;;){` |
|     126 | 7172 | `		if( n >= pMap->nEntry ){` |
|      28 | 7173 | `			break;` |
|       - | 7174 | `		}` |
|     198 | 7175 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 7176 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 7177 | `		}` |
|       - | 7178 | `		/* Dump key */` |
|     100 | 7179 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 7180 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 7181 | `		}else{` |
|     101 | 7182 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 7183 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 7184 | `		}` |
|       - | 7185 | `#ifdef __WINNT__` |
|       2 | 7186 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7187 | `#else` |
|      98 | 7188 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7189 | `#endif` |
|       - | 7190 | `		/* Dump node value */` |
|     100 | 7191 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 7192 | `		isRef = 0;` |
|     100 | 7193 | `		if( pObj ){` |
|     100 | 7194 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 7195 | `				/* Referenced object */` |
|     ! 0 | 7196 | `				isRef = 1;` |
|     ! 0 | 7197 | `			}` |
|     100 | 7198 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 7199 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 7200 | `				break;` |
|       - | 7201 | `			}` |
|      49 | 7202 | `		}` |
|       - | 7203 | `		/* Point to the next entry */` |
|     100 | 7204 | `		n++;` |
|     100 | 7205 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 7206 | `	}` |
|      54 | 7207 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 7208 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 7209 | `	}` |
|      28 | 7210 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 7211 | `	return rc;` |
|      15 | 7212 |  |
|       - | 7213 | `/*` |
|       - | 7214 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 7215 | ` * retrieved entry.` |
|       - | 7216 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 7217 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 7218 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 7219 | ` * a value different from PH7_OK.` |
|       - | 7220 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 7221 | ` */` |
|   29470 | 7222 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 7223 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 7224 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 7225 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 7226 | `	)` |
|       2 | 7227 |  |
|       - | 7228 | `	ph7_hashmap_node *pEntry;` |
|       - | 7229 | `	ph7_value sKey,sValue;` |
|       - | 7230 | `	sxi32 rc;` |
|       - | 7231 | `	sxu32 n;` |
|       - | 7232 | `	/* Initialize walker parameter */` |
|   29472 | 7233 | `	rc = SXRET_OK;` |
|   29472 | 7234 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   29472 | 7235 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   29472 | 7236 | `	n = pMap->nEntry;` |
|   29472 | 7237 | `	pEntry = pMap->pFirst;` |
|       - | 7238 | `	/* Start the iteration process */` |
|   73717 | 7239 | `	for(;;){` |
|  147436 | 7240 | `		if( n < 1 ){` |
|   29472 | 7241 | `			break;` |
|       - | 7242 | `		}` |
|       - | 7243 | `		/* Extract a copy of the key and a copy the current value */` |
|  117966 | 7244 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  117966 | 7245 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 7246 | `		/* Invoke the user callback */` |
|  117966 | 7247 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 7248 | `		/* Release the copy of the key and the value */` |
|  117966 | 7249 | `		PH7_MemObjRelease(&sKey);` |
|  117966 | 7250 | `		PH7_MemObjRelease(&sValue);` |
|  117966 | 7251 | `		if( rc != PH7_OK ){` |
|       - | 7252 | `			/* Callback request an operation abort */` |
|     ! 0 | 7253 | `			return SXERR_ABORT;` |
|       - | 7254 | `		}` |
|       - | 7255 | `		/* Point to the next entry */` |
|  117966 | 7256 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  117966 | 7257 | `		n--;` |
|       2 | 7258 | `	}` |
|       - | 7259 | `	/* All done */` |
|   29472 | 7260 | `	return SXRET_OK;` |
|   14737 | 7261 |  |
|       - | 7262 |  |
