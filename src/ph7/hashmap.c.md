# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2793/3250 lines (85.94%)

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
| 2840306 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2840308 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  236892 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  236894 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  236894 |   29 | `	sxu32 nH = 5381;` |
|  236894 |   30 | `	zEnd = &zIn[nLen];` |
|  270285 |   31 | `	for(;;){` |
|  540572 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  482762 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  435742 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  354480 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  236894 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     748 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       2 |   47 |  |
|     750 |   48 | `	sxi64 iCount = 0;` |
|     750 |   49 | `	if( !bRecursive ){` |
|     576 |   50 | `		iCount = pMap->nEntry;` |
|     289 |   51 | `	}else{` |
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
|     750 |   86 | `	return iCount;` |
|       2 |   87 |  |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 2785394 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 2785396 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2785396 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 2785396 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 2785396 |  104 | `	pNode->pMap  = &(*pMap);` |
| 2785396 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2785396 |  106 | `	pNode->nHash = nHash;` |
| 2785396 |  107 | `	pNode->xKey.iKey = iKey;` |
| 2785396 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 2785396 |  109 | `	return pNode;` |
| 1392699 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|   82320 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|   82322 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   82322 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|   82322 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|   82322 |  127 | `	pNode->pMap  = &(*pMap);` |
|   82322 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   82322 |  129 | `	pNode->nHash = nHash;` |
|   82322 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   82322 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   82322 |  132 | `	pNode->nValIdx = nValIdx;` |
|   82322 |  133 | `	return pNode;` |
|   41162 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 2867714 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  139 |  |
|       - |  140 | `	/* Link */` |
| 2867716 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2642110 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2642110 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1321054 |  144 | `	}` |
| 2867716 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 2867716 |  147 | `	if( pMap->pFirst == 0 ){` |
|   38590 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   38590 |  150 | `		pMap->pCur = pNode;` |
|   19296 |  151 | `	}else{` |
| 2829128 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 2867716 |  154 | `	++pMap->nEntry;` |
| 2867716 |  155 |  |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    5692 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  161 |  |
|    5694 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5694 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    5694 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    5276 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2639 |  167 | `	}else{` |
|     419 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    5694 |  170 | `	if( pNode->pNextCollide ){` |
|    4437 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2218 |  172 | `	}` |
|    5694 |  173 | `	if( pMap->pFirst == pNode ){` |
|      78 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      38 |  175 | `	}` |
|    5694 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|      80 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      39 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    5694 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5694 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     100 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     100 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     100 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      49 |  188 | `		}` |
|      49 |  189 | `	}` |
|    5694 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5575 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2787 |  192 | `	}` |
|    5694 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5694 |  194 | `	pMap->nEntry--;` |
|    5694 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      34 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      34 |  198 | `		pMap->apBucket = 0;` |
|      34 |  199 | `		pMap->nSize = 0;` |
|      34 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      16 |  201 | `	}` |
|    5694 |  202 |  |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 2867714 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  208 |  |
| 2867716 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   42370 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   42370 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   42370 |  215 | `		if( nNew < 1 ){` |
|   38590 |  216 | `			nNew = 16;` |
|   19294 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   42370 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   42370 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   42370 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   42370 |  230 | `		pMap->apBucket = apNew;` |
|   42370 |  231 | `		pMap->nSize = nNew;` |
|   42370 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   38590 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    3782 |  237 | `		pEntry = pMap->pFirst;` |
|    3782 |  238 | `		n = 0;` |
| 1957698 |  239 | `		for( ;; ){` |
| 3915398 |  240 | `			if( n >= pMap->nEntry ){` |
|    3782 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 3911618 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 3911618 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3911618 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3435750 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3435750 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1717874 |  250 | `			}` |
| 3911618 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 3911618 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3911618 |  254 | `			n++;` |
|       2 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    3782 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1890 |  258 | `	}` |
| 2829128 |  259 | `	return SXRET_OK;` |
| 1433859 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 2785394 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 2785396 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		/* Reserve a ph7_value for the value */` |
| 2785370 |  274 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2785370 |  275 | `		if( pObj == 0 ){` |
|     ! 0 |  276 | `			return SXERR_MEM;` |
|       - |  277 | `		}` |
| 2785370 |  278 | `		if( pValue ){` |
|       - |  279 | `			/* Duplicate the value */` |
| 2785370 |  280 | `			PH7_MemObjStore(pValue,pObj);` |
| 1392684 |  281 | `		}` |
| 2785370 |  282 | `		nIdx = pObj->nIdx;` |
| 1392686 |  283 | `	}else{` |
|      27 |  284 | `		nIdx = nRefIdx;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Hash the key */` |
| 2785396 |  287 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  288 | `	/* Allocate a new int node */` |
| 2785396 |  289 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2785396 |  290 | `	if( pNode == 0 ){` |
|     ! 0 |  291 | `		return SXERR_MEM;` |
|       - |  292 | `	}` |
| 2785396 |  293 | `	if( isForeign ){` |
|       - |  294 | `		/* Mark as a foregin entry */` |
|      27 |  295 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      13 |  296 | `	}` |
|       - |  297 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2785396 |  298 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2785396 |  299 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  300 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  301 | `		return rc;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Perform the insertion */` |
| 2785396 |  304 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  305 | `	/* Install in the reference table */` |
| 2785396 |  306 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  307 | `	/* All done */` |
| 2785396 |  308 | `	return SXRET_OK;` |
| 1392699 |  309 |  |
|       - |  310 | `/*` |
|       - |  311 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  312 | ` * hashmap.` |
|       - |  313 | ` */` |
|   82320 |  314 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  315 |  |
|       - |  316 | `	ph7_hashmap_node *pNode;` |
|       - |  317 | `	sxu32 nHash;` |
|       - |  318 | `	sxu32 nIdx;` |
|       - |  319 | `	sxi32 rc;` |
|   82322 |  320 | `	if( !isForeign ){` |
|       - |  321 | `		ph7_value *pObj;` |
|       - |  322 | `		/* Reserve a ph7_value for the value */` |
|   60128 |  323 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   60128 |  324 | `		if( pObj == 0 ){` |
|     ! 0 |  325 | `			return SXERR_MEM;` |
|       - |  326 | `		}` |
|   60128 |  327 | `		if( pValue ){` |
|       - |  328 | `			/* Duplicate the value */` |
|   60128 |  329 | `			PH7_MemObjStore(pValue,pObj);` |
|   30063 |  330 | `		}` |
|   60128 |  331 | `		nIdx = pObj->nIdx;` |
|   30065 |  332 | `	}else{` |
|   22196 |  333 | `		nIdx = nRefIdx;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Hash the key */` |
|   82322 |  336 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  337 | `	/* Allocate a new blob node */` |
|   82322 |  338 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   82322 |  339 | `	if( pNode == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|   82322 |  342 | `	if( isForeign ){` |
|       - |  343 | `		/* Mark as a foregin entry */` |
|   22196 |  344 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   11097 |  345 | `	}` |
|       - |  346 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   82322 |  347 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   82322 |  348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  349 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  350 | `		return rc;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Perform the insertion */` |
|   82322 |  353 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  354 | `	/* Install in the reference table */` |
|   82322 |  355 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  356 | `	/* All done */` |
|   82322 |  357 | `	return SXRET_OK;` |
|   41162 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  361 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  362 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  363 | ` */` |
|   46858 |  364 | `static sxi32 HashmapLookupIntKey(` |
|       - |  365 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  366 | `	sxi64 iKey,                /* lookup key */` |
|       - |  367 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  368 | `	)` |
|       2 |  369 |  |
|       - |  370 | `	ph7_hashmap_node *pNode;` |
|       - |  371 | `	sxu32 nHash;` |
|   46860 |  372 | `	if( pMap->nEntry < 1 ){` |
|       - |  373 | `		/* Don't bother hashing,there is no entry anyway */` |
|     388 |  374 | `		return SXERR_NOTFOUND;` |
|       - |  375 | `	}` |
|       - |  376 | `	/* Hash the key first */` |
|   46474 |  377 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  378 | `	/* Point to the appropriate bucket */` |
|   46474 |  379 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  380 | `	/* Perform the lookup */` |
|  411589 |  381 | `	for(;;){` |
|  823180 |  382 | `		if( pNode == 0 ){` |
|   45782 |  383 | `			break;` |
|       - |  384 | `		}` |
|  777744 |  385 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774384 |  386 | `			&& pNode->nHash == nHash` |
|  386033 |  387 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  388 | `				/* Node found */` |
|     694 |  389 | `				if( ppNode ){` |
|     682 |  390 | `					*ppNode = pNode;` |
|     340 |  391 | `				}` |
|     694 |  392 | `				return SXRET_OK;` |
|       - |  393 | `		}` |
|       - |  394 | `		/* Follow the collision link */` |
|  776707 |  395 | `		pNode = pNode->pNextCollide;` |
|       1 |  396 | `	}` |
|       - |  397 | `	/* No such entry */` |
|   45782 |  398 | `	return SXERR_NOTFOUND;` |
|   23431 |  399 |  |
|       - |  400 | `/*` |
|       - |  401 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  402 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  403 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  404 | ` */` |
|  163342 |  405 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  406 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  407 | `	const void *pKey,           /* Lookup key */` |
|       - |  408 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  409 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  410 | `	)` |
|       2 |  411 |  |
|       - |  412 | `	ph7_hashmap_node *pNode;` |
|       - |  413 | `	sxu32 nHash;` |
|  163344 |  414 | `	if( pMap->nEntry < 1 ){` |
|       - |  415 | `		/* Don't bother hashing,there is no entry anyway */` |
|    8772 |  416 | `		return SXERR_NOTFOUND;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Hash the key first */` |
|  154574 |  419 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  420 | `	/* Point to the appropriate bucket */` |
|  154574 |  421 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  422 | `	/* Perform the lookup */` |
|  151685 |  423 | `	for(;;){` |
|  303372 |  424 | `		if( pNode == 0 ){` |
|  117334 |  425 | `			break;` |
|       - |  426 | `		}` |
|  204658 |  427 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  184540 |  428 | `			&& pNode->nHash == nHash` |
|  110141 |  429 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   37242 |  430 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  431 | `				/* Node found */` |
|   37242 |  432 | `				if( ppNode ){` |
|   37214 |  433 | `					*ppNode = pNode;` |
|   18606 |  434 | `				}` |
|   37242 |  435 | `				return SXRET_OK;` |
|       - |  436 | `		}` |
|       - |  437 | `		/* Follow the collision link */` |
|  148800 |  438 | `		pNode = pNode->pNextCollide;` |
|       2 |  439 | `	}` |
|       - |  440 | `	/* No such entry */` |
|  117334 |  441 | `	return SXERR_NOTFOUND;` |
|   81673 |  442 |  |
|       - |  443 | `/*` |
|       - |  444 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  445 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  446 | ` */` |
|  163484 |  447 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  448 |  |
|  163486 |  449 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  163486 |  450 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  163486 |  451 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  452 | `		/* Octal not decimal number */` |
|       5 |  453 | `		return FALSE;` |
|       - |  454 | `	}` |
|  163482 |  455 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  456 | `		zIn++;` |
|     ! 0 |  457 | `	}` |
|   82073 |  458 | `	for(;;){` |
|  164148 |  459 | `		if( zIn >= zEnd ){` |
|     233 |  460 | `			return TRUE;` |
|       - |  461 | `		}` |
|  163916 |  462 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   81626 |  463 | `			break;` |
|       - |  464 | `		}` |
|     667 |  465 | `		zIn++;` |
|       1 |  466 | `	}` |
|       - |  467 | `	/* Key does not look like a decimal number */` |
|  163250 |  468 | `	return FALSE;` |
|   81744 |  469 |  |
|       - |  470 | `/*` |
|       - |  471 | ` * Check if a given key exists in the given hashmap.` |
|       - |  472 | ` * Write a pointer to the target node on success.` |
|       - |  473 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  474 | ` */` |
|   81612 |  475 | `static sxi32 HashmapLookup(` |
|       - |  476 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  477 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  478 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  479 | `	)` |
|       2 |  480 |  |
|   81614 |  481 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  482 | `	sxi32 rc;` |
|   81614 |  483 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   81014 |  484 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  485 | `			/* Force a string cast */` |
|     ! 0 |  486 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  487 | `		}` |
|   81014 |  488 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  489 | `			/* Perform a blob lookup */` |
|   80998 |  490 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   80998 |  491 | `			goto result;` |
|       - |  492 | `		}` |
|       8 |  493 | `	}` |
|       - |  494 | `	/* Perform an int lookup */` |
|     618 |  495 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  496 | `		/* Force an integer cast */` |
|      27 |  497 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  498 | `	}` |
|       - |  499 | `	/* Perform an int lookup */` |
|     618 |  500 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   40806 |  501 | `result:` |
|   81614 |  502 | `	if( rc == SXRET_OK ){` |
|       - |  503 | `		/* Node found */` |
|   37762 |  504 | `		if( ppNode ){` |
|   37738 |  505 | `			*ppNode = pNode;` |
|   18868 |  506 | `		}` |
|   37762 |  507 | `		return SXRET_OK;` |
|       - |  508 | `	}` |
|       - |  509 | `	/* No such entry */` |
|   43854 |  510 | `	return SXERR_NOTFOUND;` |
|   40808 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  514 | ` * hashmap.` |
|       - |  515 | ` * If a node with the given key already exists in the database` |
|       - |  516 | ` * then this function overwrite the old value.` |
|       - |  517 | ` */` |
| 2845292 |  518 | `static sxi32 HashmapInsert(` |
|       - |  519 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  520 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  521 | `	ph7_value *pVal    /* Node value */` |
|       - |  522 | `	)` |
|       2 |  523 |  |
| 2845294 |  524 | `	ph7_hashmap_node *pNode = 0;` |
| 2845294 |  525 | `	sxi32 rc = SXRET_OK;` |
| 2845294 |  526 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   60312 |  527 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  528 | `			/* Force a string cast */` |
|       3 |  529 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  530 | `		}` |
|   60312 |  531 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  532 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  533 | `				/* Automatic index assign */` |
|      34 |  534 | `				pKey = 0;` |
|      16 |  535 | `			}` |
|     256 |  536 | `			goto IntKey;` |
|       - |  537 | `		}` |
|   90086 |  538 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   30028 |  539 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  540 | `				/* Overwrite the old value */` |
|       - |  541 | `				ph7_value *pElem;` |
|      27 |  542 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      27 |  543 | `				if( pElem ){` |
|      27 |  544 | `					if( pVal ){` |
|      27 |  545 | `						PH7_MemObjStore(pVal,pElem);` |
|      14 |  546 | `					}else{` |
|       - |  547 | `						/* Nullify the entry */` |
|     ! 0 |  548 | `						PH7_MemObjToNull(pElem);` |
|       - |  549 | `					}` |
|      13 |  550 | `				}` |
|      27 |  551 | `				return SXRET_OK;` |
|       - |  552 | `		}` |
|   60032 |  553 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  554 | `			/* Forbidden */` |
|       3 |  555 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Perform a blob-key insertion */` |
|   60030 |  559 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   60030 |  560 | `		return rc;` |
|       - |  561 | `	}` |
| 1392491 |  562 | `IntKey:` |
| 2785238 |  563 | `	if( pKey ){` |
|   23232 |  564 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  565 | `			/* Force an integer cast */` |
|     251 |  566 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  567 | `		}` |
|   23232 |  568 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   23186 |  582 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  583 | `			/* Forbidden */` |
|       3 |  584 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  585 | `			return SXRET_OK;` |
|       - |  586 | `		}` |
|       - |  587 | `		/* Perform a 64-bit-int-key insertion */` |
|   23184 |  588 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23184 |  589 | `		if( rc == SXRET_OK ){` |
|   23184 |  590 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  591 | `				/* Increment the automatic index */` |
|   22956 |  592 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  593 | `				/* Make sure the automatic index is not reserved */` |
|   22956 |  594 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  595 | `					pMap->iNextIdx++;` |
|     ! 0 |  596 | `				}` |
|   11477 |  597 | `			}` |
|   11591 |  598 | `		}` |
|   11593 |  599 | `	}else{` |
| 2762008 |  600 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  601 | `			/* Forbidden */` |
|       3 |  602 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|       - |  605 | `		/* Assign an automatic index */` |
| 2762006 |  606 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2762006 |  607 | `		if( rc == SXRET_OK ){` |
| 2762006 |  608 | `			++pMap->iNextIdx;` |
| 1381002 |  609 | `		}` |
|       - |  610 | `	}` |
|       - |  611 | `	/* Insertion result */` |
| 2785188 |  612 | `	return rc;` |
| 1422648 |  613 |  |
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
|   22226 |  641 | `static sxi32 HashmapInsertByRef(` |
|       - |  642 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  643 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  644 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  645 | `	)` |
|       2 |  646 |  |
|   22228 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|   22228 |  648 | `	sxi32 rc = SXRET_OK;` |
|   22228 |  649 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   22202 |  650 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  651 | `			/* Force a string cast */` |
|     ! 0 |  652 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  653 | `		}` |
|   22202 |  654 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  655 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  656 | `				/* Automatic index assign */` |
|     ! 0 |  657 | `				pKey = 0;` |
|     ! 0 |  658 | `			}` |
|     ! 0 |  659 | `			goto IntKey;` |
|       - |  660 | `		}` |
|   33302 |  661 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   11100 |  662 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  663 | `				/* Overwrite */` |
|       7 |  664 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  665 | `				pNode->nValIdx = nRefIdx;` |
|       - |  666 | `				/* Install in the reference table */` |
|       7 |  667 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  668 | `				return SXRET_OK;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Perform a blob-key insertion */` |
|   22196 |  671 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   22196 |  672 | `		return rc;` |
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
|   11115 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * Extract node value.` |
|       - |  712 | ` */` |
|  911556 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
|  911558 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  911558 |  718 | `	return pObj;` |
|       2 |  719 |  |
|       - |  720 | `/*` |
|       - |  721 | ` * Insert a node in the given hashmap.` |
|       - |  722 | ` * If a node with the given key already exists in the database` |
|       - |  723 | ` * then this function overwrite the old value.` |
|       - |  724 | ` */` |
|     418 |  725 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       1 |  726 |  |
|       - |  727 | `	ph7_value *pObj;` |
|       - |  728 | `	sxi32 rc;` |
|       - |  729 | `	/* Extract the node value */` |
|     419 |  730 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     419 |  731 | `	if( pObj == 0 ){` |
|     ! 0 |  732 | `		return SXERR_EMPTY;` |
|       - |  733 | `	}` |
|       - |  734 | `	/* Preserve key */` |
|     419 |  735 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  736 | `		/* Int64 key */` |
|     289 |  737 | `		if( !bPreserve ){` |
|       - |  738 | `			/* Assign an automatic index */` |
|     149 |  739 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      75 |  740 | `		}else{` |
|     141 |  741 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  742 | `		}` |
|     145 |  743 | `	}else{` |
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
|     419 |  754 | `	return rc;` |
|     210 |  755 |  |
|       - |  756 | `/*` |
|       - |  757 | ` * Compare two node values.` |
|       - |  758 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  759 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  760 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  761 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  762 | ` * documenation.` |
|       - |  763 | ` */` |
|   38466 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   38468 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   38468 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   38468 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   38468 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   38468 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   38468 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   38468 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   38468 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   38468 |  783 | `	return rc;` |
|   19277 |  784 |  |
|       - |  785 | `/*` |
|       - |  786 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  787 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  788 | ` */` |
|    8440 |  789 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  790 |  |
|    8442 |  791 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  792 | `	sxu32 nBucket;` |
|       - |  793 | `	/* Remove old collision links */` |
|    8442 |  794 | `	if( pEntry->pPrevCollide ){` |
|    6719 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    3355 |  796 | `	}else{` |
|    1725 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  798 | `	}` |
|    8442 |  799 | `	if( pEntry->pNextCollide ){` |
|     657 |  800 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     328 |  801 | `	}` |
|    8442 |  802 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  803 | `	/* Compute the new hash */` |
|    8442 |  804 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    8442 |  805 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    8442 |  806 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  807 | `	/* Link to the new bucket */` |
|    8442 |  808 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    8442 |  809 | `	if( pMap->apBucket[nBucket] ){` |
|    6891 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3444 |  811 | `	}` |
|    8442 |  812 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    8442 |  813 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  814 | `	/* Increment the automatic index */` |
|    8442 |  815 | `	pMap->iNextIdx++;` |
|    8442 |  816 |  |
|       - |  817 | `/*` |
|       - |  818 | ` * Perform a linear search on a given hashmap.` |
|       - |  819 | ` * Write a pointer to the target node on success.` |
|       - |  820 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  821 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  822 | ` * for more information.` |
|       - |  823 | ` */` |
|   21378 |  824 | `static int HashmapFindValue(` |
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
|   21380 |  837 | `	pEntry = pMap->pFirst;` |
|   21380 |  838 | `	n = pMap->nEntry;` |
|   21380 |  839 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   21380 |  840 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   51187 |  841 | `	for(;;){` |
|  102378 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  102280 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  102280 |  847 | `		if( pVal ){` |
|  102280 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  102280 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  102280 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  102280 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  102280 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  102280 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  102280 |  865 | `				if( rc == 0 ){` |
|   21282 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   21282 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   40498 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|   81000 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   81000 |  876 | `		n--;` |
|       2 |  877 | `	}` |
|       - |  878 | `	/* No such entry */` |
|      99 |  879 | `	return SXERR_NOTFOUND;` |
|   10691 |  880 |  |
|       - |  881 | `/*` |
|       - |  882 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  883 | ` * for values comparison.` |
|       - |  884 | ` * Write a pointer to the target node on success.` |
|       - |  885 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  886 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  887 | ` * for more information.` |
|       - |  888 | ` */` |
|      12 |  889 | `static int HashmapFindValueByCallback(` |
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
|      13 |  902 | `	pEntry = pMap->pFirst;` |
|      13 |  903 | `	n = pMap->nEntry;` |
|       - |  904 | `	/* Store callback result here */` |
|      13 |  905 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  906 | `	/* First argument to the callback */` |
|      13 |  907 | `	apArg[0] = pNeedle;` |
|      15 |  908 | `	for(;;){` |
|      31 |  909 | `		if( n < 1 ){` |
|       7 |  910 | `			break;` |
|       - |  911 | `		}` |
|       - |  912 | `		/* Extract node value */` |
|      25 |  913 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      25 |  914 | `		if( pVal ){` |
|       - |  915 | `			/* Invoke the user callback */` |
|      25 |  916 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      25 |  917 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      25 |  918 | `			if( rc == SXRET_OK ){` |
|       - |  919 | `				/* Extract callback result */` |
|      25 |  920 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  921 | `					/* Perform an int cast */` |
|     ! 0 |  922 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  923 | `				}` |
|      25 |  924 | `				rc = (sxi32)sResult.x.iVal;` |
|      25 |  925 | `				PH7_MemObjRelease(&sResult);` |
|      25 |  926 | `				if( rc == 0 ){` |
|       - |  927 | `					/* Match found*/` |
|       7 |  928 | `					if( ppNode ){` |
|     ! 0 |  929 | `						*ppNode = pEntry;` |
|     ! 0 |  930 | `					}` |
|       7 |  931 | `					return SXRET_OK;` |
|       - |  932 | `				}` |
|       9 |  933 | `			}` |
|       9 |  934 | `		}` |
|       - |  935 | `		/* Point to the next entry */` |
|      19 |  936 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 |  937 | `		n--;` |
|       1 |  938 | `	}` |
|       - |  939 | `	/* No such entry */` |
|       7 |  940 | `	return SXERR_NOTFOUND;` |
|       7 |  941 |  |
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
|  449044 | 1054 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1055 | `	ph7_hashmap *pDest,` |
|       - | 1056 | `	ph7_hashmap_node *pEntry,` |
|       - | 1057 | `	ph7_value *pVal,` |
|       - | 1058 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1059 | `	)` |
|       2 | 1060 |  |
|  449046 | 1061 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1062 | `	ph7_value sKey;` |
|       - | 1063 | `	sxi32 rc;` |
|       - | 1064 |  |
|  449046 | 1065 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1066 | `		/* Blob key insertion */` |
|      41 | 1067 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      41 | 1068 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      41 | 1069 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      41 | 1070 | `		PH7_MemObjRelease(&sKey);` |
|      21 | 1071 | `	}else{` |
|       - | 1072 | `		/* Int key */` |
|  449006 | 1073 | `		if( iAction == 0 ){ /* Merge */` |
|  448934 | 1074 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  224540 | 1075 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1076 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1077 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1078 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1079 | `		}else{ /* Dup */` |
|      44 | 1080 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1081 | `		}` |
|       - | 1082 | `	}` |
|  449046 | 1083 | `	return rc;` |
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
|    1744 | 1097 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1098 |  |
|       - | 1099 | `	ph7_hashmap_node *pEntry;` |
|       - | 1100 | `	ph7_value *pVal;` |
|       - | 1101 | `	sxi32 rc;` |
|       - | 1102 | `	sxu32 n;` |
|    1746 | 1103 | `	if( pSrc == pDest ){` |
|       - | 1104 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1105 | `		 * Unlike the zend engine.` |
|       - | 1106 | `		 */` |
|     ! 0 | 1107 | `		return SXRET_OK;` |
|       - | 1108 | `	}` |
|       - | 1109 | `	/* Point to the first inserted entry in the source */` |
|    1746 | 1110 | `	pEntry = pSrc->pFirst;` |
|       - | 1111 | `	/* Perform the merge */` |
|  450694 | 1112 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1113 | `		/* Extract the node value */` |
|  448950 | 1114 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  448950 | 1115 | `		if( pVal ){` |
|       - | 1116 | `			/* Make a local copy of the value.` |
|       - | 1117 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1118 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1119 | `			 * to the old pool.` |
|       - | 1120 | `			 */` |
|  448950 | 1121 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  224476 | 1122 | `		}else{` |
|     ! 0 | 1123 | `			rc = SXRET_OK;` |
|       - | 1124 | `		}` |
|  448950 | 1125 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1126 | `			return rc;` |
|       - | 1127 | `		}` |
|       - | 1128 | `		/* Point to the next entry */` |
|  448950 | 1129 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  224476 | 1130 | `	}` |
|    1746 | 1131 | `	return SXRET_OK;` |
|     874 | 1132 |  |
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
|   57950 | 1304 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1305 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1306 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1307 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1308 | `	)` |
|       2 | 1309 |  |
|       - | 1310 | `	ph7_hashmap *pMap;` |
|       - | 1311 | `	/* Allocate a new instance */` |
|   57952 | 1312 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   57952 | 1313 | `	if( pMap == 0 ){` |
|     ! 0 | 1314 | `		return 0;` |
|       - | 1315 | `	}` |
|       - | 1316 | `	/* Zero the structure */` |
|   57952 | 1317 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1318 | `	/* Fill in the structure */` |
|   57952 | 1319 | `	pMap->pVm = &(*pVm);` |
|   57952 | 1320 | `	pMap->iRef = 1;` |
|       - | 1321 | `	/* Default hash functions */` |
|   57952 | 1322 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   57952 | 1323 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   57952 | 1324 | `	return pMap;` |
|   28977 | 1325 |  |
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
|    1672 | 1346 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    1674 | 1366 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1674 | 1367 | `	if( pMap == 0 ){` |
|     ! 0 | 1368 | `		return SXERR_MEM;` |
|       - | 1369 | `	}` |
|    1674 | 1370 | `	pVm->pGlobal = pMap;` |
|       - | 1371 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1674 | 1372 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1674 | 1373 | `	if( pObj == 0 ){` |
|     ! 0 | 1374 | `		return SXERR_MEM;` |
|       - | 1375 | `	}` |
|    1674 | 1376 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1377 | `	/* Record object index */` |
|    1674 | 1378 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1379 | `	/* Install the special $GLOBALS array */` |
|    1674 | 1380 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1674 | 1381 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1382 | `		return rc;` |
|       - | 1383 | `	}` |
|       - | 1384 | `	/* Install superglobals now */` |
|   18394 | 1385 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1386 | `		ph7_value *pSuper;` |
|       - | 1387 | `		/* Request an empty array */` |
|   16722 | 1388 | `		pSuper = ph7_new_array(&(*pVm));` |
|   16722 | 1389 | `		if( pSuper == 0 ){` |
|     ! 0 | 1390 | `			return SXERR_MEM;` |
|       - | 1391 | `		}` |
|       - | 1392 | `		/* Install */` |
|   16722 | 1393 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   16722 | 1394 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1395 | `			return rc;` |
|       - | 1396 | `		}` |
|       - | 1397 | `		/* Release the value now it have been installed */` |
|   16722 | 1398 | `		ph7_release_value(&(*pVm),pSuper);` |
|    8362 | 1399 | `	}` |
|       - | 1400 | `	/* Set some $_SERVER entries */` |
|    1674 | 1401 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1402 | `	/*` |
|       - | 1403 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1404 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1405 | `	 */` |
|    3342 | 1406 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1407 | `		"SCRIPT_FILENAME",` |
|     836 | 1408 | `		pFile ? pFile->zString : ":Memory:",` |
|    1668 | 1409 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1410 | `		);` |
|       - | 1411 | `	/* All done,all super-global are installed now */` |
|    1674 | 1412 | `	return SXRET_OK;` |
|     838 | 1413 |  |
|       - | 1414 | `/*` |
|       - | 1415 | ` * Release a hashmap.` |
|       - | 1416 | ` */` |
|   39484 | 1417 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1418 |  |
|       - | 1419 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   39486 | 1420 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1421 | `	sxu32 n;` |
|   39486 | 1422 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1423 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1424 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1425 | `		return SXRET_OK;` |
|       - | 1426 | `	}` |
|       - | 1427 | `	/* Start the release process */` |
|   39486 | 1428 | `	n = 0;` |
|   39486 | 1429 | `	pEntry = pMap->pFirst;` |
| 1439208 | 1430 | `	for(;;){` |
| 2878418 | 1431 | `		if( n >= pMap->nEntry ){` |
|   39486 | 1432 | `			break;` |
|       - | 1433 | `		}` |
| 2838934 | 1434 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1435 | `		/* Remove the reference from the foreign table */` |
| 2838934 | 1436 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2838934 | 1437 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1438 | `			/* Restore the ph7_value to the free list */` |
| 2838926 | 1439 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1419462 | 1440 | `		}` |
|       - | 1441 | `		/* Release the node */` |
| 2838934 | 1442 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   57818 | 1443 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   28908 | 1444 | `		}` |
| 2838934 | 1445 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1446 | `		/* Point to the next entry */` |
| 2838934 | 1447 | `		pEntry = pNext;` |
| 2838934 | 1448 | `		n++;` |
|       2 | 1449 | `	}` |
|   39486 | 1450 | `	if( pMap->nEntry > 0 ){` |
|       - | 1451 | `		/* Release the hash bucket */` |
|   35122 | 1452 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   17560 | 1453 | `	}` |
|   39486 | 1454 | `	if( FreeDS ){` |
|       - | 1455 | `		/* Free the whole instance */` |
|   39470 | 1456 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   19736 | 1457 | `	}else{` |
|       - | 1458 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1459 | `		pMap->apBucket = 0;` |
|      17 | 1460 | `		pMap->iNextIdx = 0;` |
|      17 | 1461 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1462 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1463 | `	}` |
|   39486 | 1464 | `	return SXRET_OK;` |
|   19744 | 1465 |  |
|       - | 1466 | `/*` |
|       - | 1467 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1468 | ` * If the count reaches zero which mean no more variables` |
|       - | 1469 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1470 | ` */` |
|  450320 | 1471 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1472 |  |
|  450322 | 1473 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1474 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  450322 | 1475 | `	pMap->iRef--;` |
|  450322 | 1476 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   39470 | 1477 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   19734 | 1478 | `	}` |
|  450322 | 1479 |  |
|       - | 1480 | `/*` |
|       - | 1481 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1482 | ` * Write a pointer to the target node on success.` |
|       - | 1483 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1484 | ` */` |
|   81620 | 1485 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1486 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1487 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1488 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1489 | `	)` |
|       2 | 1490 |  |
|       - | 1491 | `	sxi32 rc;` |
|   81622 | 1492 | `	if( pMap->nEntry < 1 ){` |
|       - | 1493 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1494 | `		 */` |
|       9 | 1495 | `		return SXERR_NOTFOUND;` |
|       - | 1496 | `	}` |
|   81614 | 1497 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   81614 | 1498 | `	return rc;` |
|   40812 | 1499 |  |
|       - | 1500 | `/*` |
|       - | 1501 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1502 | ` * hashmap.` |
|       - | 1503 | ` * If a node with the given key already exists in the database` |
|       - | 1504 | ` * then this function overwrite the old value.` |
|       - | 1505 | ` */` |
| 2396148 | 1506 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1507 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1508 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1509 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1510 | `	)` |
|       2 | 1511 |  |
|       - | 1512 | `	sxi32 rc;` |
| 2396150 | 1513 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1514 | `		/*` |
|       - | 1515 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1516 | `		 */` |
|     ! 0 | 1517 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1518 | `		return SXRET_OK;` |
|       - | 1519 | `	}` |
| 2396150 | 1520 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2396150 | 1521 | `	return rc;` |
| 1198076 | 1522 |  |
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
|   22226 | 1550 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1551 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1552 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1553 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1554 | `	)` |
|       2 | 1555 |  |
|       - | 1556 | `	sxi32 rc;` |
|   22228 | 1557 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1558 | `		/*` |
|       - | 1559 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1560 | `		 */` |
|     ! 0 | 1561 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1562 | `		return SXRET_OK;` |
|       - | 1563 | `	}` |
|   22228 | 1564 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   22228 | 1565 | `	return rc;` |
|   11115 | 1566 |  |
|       - | 1567 | `/*` |
|       - | 1568 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1569 | ` */` |
|   17524 | 1570 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1571 |  |
|       - | 1572 | `	/* Reset the loop cursor */` |
|   17526 | 1573 | `	pMap->pCur = pMap->pFirst;` |
|   17526 | 1574 |  |
|       - | 1575 | `/*` |
|       - | 1576 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1577 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1578 | ` * return NULL.` |
|       - | 1579 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1580 | ` */` |
|  141974 | 1581 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1582 |  |
|  141976 | 1583 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  141976 | 1584 | `	if( pCur == 0 ){` |
|       - | 1585 | `		/* End of the list,return null */` |
|    8784 | 1586 | `		return 0;` |
|       - | 1587 | `	}` |
|       - | 1588 | `	/* Advance the node cursor */` |
|  133194 | 1589 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  133194 | 1590 | `	return pCur;` |
|   70989 | 1591 |  |
|       - | 1592 | `/*` |
|       - | 1593 | ` * Extract a node value.` |
|       - | 1594 | ` */` |
|  336466 | 1595 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1596 |  |
|  336468 | 1597 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  336468 | 1598 | `	if( pEntry ){` |
|  336468 | 1599 | `		if( bStore ){` |
|  133222 | 1600 | `			PH7_MemObjStore(pEntry,pValue);` |
|   66612 | 1601 | `		}else{` |
|  203248 | 1602 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1603 | `		}` |
|  168319 | 1604 | `	}else{` |
|     ! 0 | 1605 | `		PH7_MemObjRelease(pValue);` |
|       - | 1606 | `	}` |
|  336468 | 1607 |  |
|       - | 1608 | `/*` |
|       - | 1609 | ` * Extract a node key.` |
|       - | 1610 | ` */` |
|   89014 | 1611 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1612 |  |
|       - | 1613 | `	/* Fill with the current key */` |
|   89016 | 1614 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   88860 | 1615 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      13 | 1616 | `			SyBlobRelease(&pKey->sBlob);` |
|       6 | 1617 | `		}` |
|   88860 | 1618 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   88860 | 1619 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   44431 | 1620 | `	}else{` |
|     157 | 1621 | `		SyBlobReset(&pKey->sBlob);` |
|     157 | 1622 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     157 | 1623 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1624 | `	}` |
|   89016 | 1625 |  |
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
|   24616 | 1673 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1674 |  |
|       - | 1675 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1676 | `    /* Prevent compiler warning */` |
|   24618 | 1677 | `	result.pNext = result.pPrev = 0;` |
|   24618 | 1678 | `	pTail = &result;` |
|   63127 | 1679 | `	while( pA && pB ){` |
|   38511 | 1680 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|   25385 | 1681 | `			pTail->pPrev = pA;` |
|   25385 | 1682 | `			pA->pNext = pTail;` |
|   25385 | 1683 | `			pTail = pA;` |
|   25385 | 1684 | `			pA = pA->pPrev;` |
|   12697 | 1685 | `		}else{` |
|   13128 | 1686 | `			pTail->pPrev = pB;` |
|   13128 | 1687 | `			pB->pNext = pTail;` |
|   13128 | 1688 | `			pTail = pB;` |
|   13128 | 1689 | `			pB = pB->pPrev;` |
|       - | 1690 | `		}` |
|       2 | 1691 | `	}` |
|   24618 | 1692 | `	if( pA ){` |
|   18206 | 1693 | `		pTail->pPrev = pA;` |
|   18206 | 1694 | `		pA->pNext = pTail;` |
|   15526 | 1695 | `	}else if( pB ){` |
|    6250 | 1696 | `		pTail->pPrev = pB;` |
|    6250 | 1697 | `		pB->pNext = pTail;` |
|    3116 | 1698 | `	}else{` |
|     166 | 1699 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1700 | `	}` |
|   24618 | 1701 | `	return result.pPrev;` |
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
|     552 | 1715 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1716 |  |
|       - | 1717 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1718 | `	sxu32 i;` |
|     554 | 1719 | `	SyZero(a,sizeof(a));` |
|       - | 1720 | `	/* Point to the first inserted entry */` |
|     554 | 1721 | `	pIn = pMap->pFirst;` |
|    8998 | 1722 | `	while( pIn ){` |
|    8446 | 1723 | `		p = pIn;` |
|    8446 | 1724 | `		pIn = p->pPrev;` |
|    8446 | 1725 | `		p->pPrev = 0;` |
|   15950 | 1726 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   15950 | 1727 | `			if( a[i]==0 ){` |
|    8446 | 1728 | `				a[i] = p;` |
|    8446 | 1729 | `				break;` |
|     ! 0 | 1730 | `			}else{` |
|    7506 | 1731 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    7506 | 1732 | `				a[i] = 0;` |
|       - | 1733 | `			}` |
|    3754 | 1734 | `		}` |
|    8446 | 1735 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1736 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1737 | `			 * But that is impossible.` |
|       - | 1738 | `			 */` |
|     ! 0 | 1739 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1740 | `		}` |
|       2 | 1741 | `	}` |
|     554 | 1742 | `	p = a[0];` |
|   17666 | 1743 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   17114 | 1744 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    8558 | 1745 | `	}` |
|     554 | 1746 | `	p->pNext = 0;` |
|       - | 1747 | `	/* Reflect the change */` |
|     554 | 1748 | `	pMap->pFirst = p;` |
|       - | 1749 | `	/* Reset the loop cursor */` |
|     554 | 1750 | `	pMap->pCur = pMap->pFirst;` |
|     554 | 1751 | `	return SXRET_OK;` |
|       2 | 1752 |  |
|       - | 1753 | `/*` |
|       - | 1754 | ` * Node comparison callback.` |
|       - | 1755 | ` * used-by: [sort(),asort(),...]` |
|       - | 1756 | ` */` |
|   38448 | 1757 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1758 |  |
|       - | 1759 | `	ph7_value sA,sB;` |
|       - | 1760 | `	sxi32 iFlags;` |
|       - | 1761 | `	int rc;` |
|   38450 | 1762 | `	if( pCmpData == 0 ){` |
|       - | 1763 | `		/* Perform a standard comparison */` |
|   38446 | 1764 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   38446 | 1765 | `		return rc;` |
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
|   19268 | 1791 |  |
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
|      12 | 1838 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1839 |  |
|       - | 1840 | `	ph7_value sA,sB;` |
|       - | 1841 | `	sxi32 iFlags;` |
|       - | 1842 | `	int rc;` |
|      13 | 1843 | `	if( pCmpData == 0 ){` |
|       - | 1844 | `		/* Perform a standard comparison */` |
|      13 | 1845 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      13 | 1846 | `		return -rc;` |
|       - | 1847 | `	}` |
|     ! 0 | 1848 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1849 | `	/* Duplicate node values */` |
|     ! 0 | 1850 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|     ! 0 | 1851 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|     ! 0 | 1852 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|     ! 0 | 1853 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|     ! 0 | 1854 | `	if( iFlags == 5 ){` |
|       - | 1855 | `		/* String cast */` |
|     ! 0 | 1856 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1857 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1858 | `		}` |
|     ! 0 | 1859 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1860 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1861 | `		}` |
|     ! 0 | 1862 | `	}else{` |
|       - | 1863 | `		/* Numeric cast */` |
|     ! 0 | 1864 | `		PH7_MemObjToNumeric(&sA);` |
|     ! 0 | 1865 | `		PH7_MemObjToNumeric(&sB);` |
|       - | 1866 | `	}` |
|       - | 1867 | `	/* Perform the comparison */` |
|     ! 0 | 1868 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|     ! 0 | 1869 | `	PH7_MemObjRelease(&sA);` |
|     ! 0 | 1870 | `	PH7_MemObjRelease(&sB);` |
|     ! 0 | 1871 | `	return -rc;` |
|       7 | 1872 |  |
|       - | 1873 | `/*` |
|       - | 1874 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1875 | ` * used-by: [usort(),uasort()]` |
|       - | 1876 | ` */` |
|      12 | 1877 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1878 |  |
|       - | 1879 | `	ph7_value sResult,*pCallback;` |
|       - | 1880 | `	ph7_value *pV1,*pV2;` |
|       - | 1881 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1882 | `	sxi32 rc;` |
|       - | 1883 | `	/* Point to the desired callback */` |
|      13 | 1884 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1885 | `	/* initialize the result value */` |
|      13 | 1886 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 1887 | `	/* Extract nodes values */` |
|      13 | 1888 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      13 | 1889 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      13 | 1890 | `	apArg[0] = pV1;` |
|      13 | 1891 | `	apArg[1] = pV2;` |
|       - | 1892 | `	/* Invoke the callback */` |
|      13 | 1893 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      13 | 1894 | `	if( rc != SXRET_OK ){` |
|       - | 1895 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1896 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1897 | `	}else{` |
|       - | 1898 | `		/* Extract callback result */` |
|      13 | 1899 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1900 | `			/* Perform an int cast */` |
|     ! 0 | 1901 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1902 | `		}` |
|      13 | 1903 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1904 | `	}` |
|      13 | 1905 | `	PH7_MemObjRelease(&sResult);` |
|       - | 1906 | `	/* Callback result */` |
|      13 | 1907 | `	return rc;` |
|       1 | 1908 |  |
|       - | 1909 | `/*` |
|       - | 1910 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1911 | ` * used-by: [krsort()]` |
|       - | 1912 | ` */` |
|       4 | 1913 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1914 |  |
|       - | 1915 | `	sxi32 rc;` |
|       2 | 1916 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 1917 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1918 | `		/* Perform a string comparison */` |
|       5 | 1919 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1920 | `	}else{` |
|       - | 1921 | `		SyString sStr;` |
|       - | 1922 | `		sxi64 iA,iB;` |
|       - | 1923 | `		/* Perform a numeric comparison */` |
|     ! 0 | 1924 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1925 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1926 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1927 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1928 | `				iA = 0;` |
|     ! 0 | 1929 | `			}else{` |
|     ! 0 | 1930 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1931 | `			}` |
|     ! 0 | 1932 | `		}else{` |
|     ! 0 | 1933 | `			iA = pA->xKey.iKey;` |
|       - | 1934 | `		}` |
|     ! 0 | 1935 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1936 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1937 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1938 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1939 | `				iB = 0;` |
|     ! 0 | 1940 | `			}else{` |
|     ! 0 | 1941 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1942 | `			}` |
|     ! 0 | 1943 | `		}else{` |
|     ! 0 | 1944 | `			iB = pB->xKey.iKey;` |
|       - | 1945 | `		}` |
|     ! 0 | 1946 | `		rc = (sxi32)(iA-iB);` |
|       - | 1947 | `	}` |
|       5 | 1948 | `	return -rc; /* Reverse result */` |
|       1 | 1949 |  |
|       - | 1950 | `/*` |
|       - | 1951 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1952 | ` * used-by: [uksort()]` |
|       - | 1953 | ` */` |
|       6 | 1954 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1955 |  |
|       - | 1956 | `	ph7_value sResult,*pCallback;` |
|       - | 1957 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1958 | `	ph7_value sK1,sK2;` |
|       - | 1959 | `	sxi32 rc;` |
|       - | 1960 | `	/* Point to the desired callback */` |
|       7 | 1961 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1962 | `	/* initialize the result value */` |
|       7 | 1963 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 1964 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 1965 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 1966 | `	/* Extract nodes keys */` |
|       7 | 1967 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 1968 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 1969 | `	apArg[0] = &sK1;` |
|       7 | 1970 | `	apArg[1] = &sK2;` |
|       - | 1971 | `	/* Mark keys as constants */` |
|       7 | 1972 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 1973 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 1974 | `	/* Invoke the callback */` |
|       7 | 1975 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 1976 | `	if( rc != SXRET_OK ){` |
|       - | 1977 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1978 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1979 | `	}else{` |
|       - | 1980 | `		/* Extract callback result */` |
|       7 | 1981 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1982 | `			/* Perform an int cast */` |
|     ! 0 | 1983 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1984 | `		}` |
|       7 | 1985 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1986 | `	}` |
|       7 | 1987 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 1988 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 1989 | `	PH7_MemObjRelease(&sK2);` |
|       - | 1990 | `	/* Callback result */` |
|       7 | 1991 | `	return rc;` |
|       1 | 1992 |  |
|       - | 1993 | `/*` |
|       - | 1994 | ` * Node comparison callback: Random node comparison.` |
|       - | 1995 | ` * used-by: [shuffle()]` |
|       - | 1996 | ` */` |
|      13 | 1997 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1998 |  |
|       - | 1999 | `	sxu32 n;` |
|       6 | 2000 | `	SXUNUSED(pB); /* cc warning */` |
|       6 | 2001 | `	SXUNUSED(pCmpData);` |
|       - | 2002 | `	/* Grab a random number */` |
|      14 | 2003 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2004 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2005 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2006 | `	 */` |
|      14 | 2007 | `	return n&1 ? 1 : -1;` |
|       1 | 2008 |  |
|       - | 2009 | `/*` |
|       - | 2010 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2011 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2012 | ` */` |
|     536 | 2013 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2014 |  |
|       - | 2015 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2016 | `	sxu32 i;` |
|       - | 2017 | `	/* Rehash all entries */` |
|     538 | 2018 | `	pLast = p = pMap->pFirst;` |
|     538 | 2019 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     538 | 2020 | `	i = 0;` |
|    4463 | 2021 | `	for( ;; ){` |
|    8928 | 2022 | `		if( i >= pMap->nEntry ){` |
|     538 | 2023 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     538 | 2024 | `			break;` |
|       - | 2025 | `		}` |
|    8392 | 2026 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2027 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2028 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2029 | `			/* Change key type */` |
|       5 | 2030 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2031 | `		}` |
|    8392 | 2032 | `		HashmapRehashIntNode(p);` |
|       - | 2033 | `		/* Point to the next entry */` |
|    8392 | 2034 | `		i++;` |
|    8392 | 2035 | `		pLast = p;` |
|    8392 | 2036 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2037 | `	}` |
|     538 | 2038 |  |
|       - | 2039 | `/*` |
|       - | 2040 | ` * Array functions implementation.` |
|       - | 2041 | ` * Status:` |
|       - | 2042 | ` *  Stable.` |
|       - | 2043 | ` */` |
|       - | 2044 | `/*` |
|       - | 2045 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2046 | ` * Sort an array.` |
|       - | 2047 | ` * Parameters` |
|       - | 2048 | ` *  $array` |
|       - | 2049 | ` *   The input array.` |
|       - | 2050 | ` * $sort_flags` |
|       - | 2051 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2052 | ` *  Sorting type flags:` |
|       - | 2053 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2054 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2055 | ` *   SORT_STRING - compare items as strings` |
|       - | 2056 | ` * Return` |
|       - | 2057 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2058 | ` *` |
|       - | 2059 | ` */` |
|     844 | 2060 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2061 |  |
|       - | 2062 | `	ph7_hashmap *pMap;` |
|       - | 2063 | `	/* Make sure we are dealing with a valid hashmap */` |
|     846 | 2064 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2065 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2066 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2067 | `		return PH7_OK;` |
|       - | 2068 | `	}` |
|       - | 2069 | `	/* Point to the internal representation of the input hashmap */` |
|     846 | 2070 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     846 | 2071 | `	if( pMap->nEntry > 1 ){` |
|     532 | 2072 | `		sxi32 iCmpFlags = 0;` |
|     532 | 2073 | `		if( nArg > 1 ){` |
|       - | 2074 | `			/* Extract comparison flags */` |
|       3 | 2075 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2076 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2077 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2078 | `			}` |
|       1 | 2079 | `		}` |
|       - | 2080 | `		/* Do the merge sort */` |
|     532 | 2081 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2082 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     532 | 2083 | `		HashmapSortRehash(pMap);` |
|     265 | 2084 | `	}` |
|       - | 2085 | `	/* All done,return TRUE */` |
|     846 | 2086 | `	ph7_result_bool(pCtx,1);` |
|     846 | 2087 | `	return PH7_OK;` |
|     424 | 2088 |  |
|       - | 2089 | `/*` |
|       - | 2090 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2091 | ` *  Sort an array and maintain index association.` |
|       - | 2092 | ` * Parameters` |
|       - | 2093 | ` *  $array` |
|       - | 2094 | ` *   The input array.` |
|       - | 2095 | ` * $sort_flags` |
|       - | 2096 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2097 | ` *  Sorting type flags:` |
|       - | 2098 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2099 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2100 | ` *   SORT_STRING - compare items as strings` |
|       - | 2101 | ` * Return` |
|       - | 2102 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2103 | ` */` |
|       2 | 2104 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2105 |  |
|       - | 2106 | `	ph7_hashmap *pMap;` |
|       - | 2107 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2108 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2109 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2110 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2111 | `		return PH7_OK;` |
|       - | 2112 | `	}` |
|       - | 2113 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2114 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2115 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2116 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2117 | `		if( nArg > 1 ){` |
|       - | 2118 | `			/* Extract comparison flags */` |
|     ! 0 | 2119 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2120 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2121 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2122 | `			}` |
|     ! 0 | 2123 | `		}` |
|       - | 2124 | `		/* Do the merge sort */` |
|       3 | 2125 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2126 | `		/* Fix the last link broken by the merge */` |
|       5 | 2127 | `		while(pMap->pLast->pPrev){` |
|       3 | 2128 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2129 | `		}` |
|       1 | 2130 | `	}` |
|       - | 2131 | `	/* All done,return TRUE */` |
|       3 | 2132 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2133 | `	return PH7_OK;` |
|       2 | 2134 |  |
|       - | 2135 | `/*` |
|       - | 2136 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2137 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2138 | ` * Parameters` |
|       - | 2139 | ` *  $array` |
|       - | 2140 | ` *   The input array.` |
|       - | 2141 | ` * $sort_flags` |
|       - | 2142 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2143 | ` *  Sorting type flags:` |
|       - | 2144 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2145 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2146 | ` *   SORT_STRING - compare items as strings` |
|       - | 2147 | ` * Return` |
|       - | 2148 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2149 | ` */` |
|       2 | 2150 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2151 |  |
|       - | 2152 | `	ph7_hashmap *pMap;` |
|       - | 2153 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2154 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2155 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2156 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2157 | `		return PH7_OK;` |
|       - | 2158 | `	}` |
|       - | 2159 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2160 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2161 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2162 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2163 | `		if( nArg > 1 ){` |
|       - | 2164 | `			/* Extract comparison flags */` |
|     ! 0 | 2165 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2166 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2167 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2168 | `			}` |
|     ! 0 | 2169 | `		}` |
|       - | 2170 | `		/* Do the merge sort */` |
|       3 | 2171 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2172 | `		/* Fix the last link broken by the merge */` |
|       5 | 2173 | `		while(pMap->pLast->pPrev){` |
|       3 | 2174 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2175 | `		}` |
|       1 | 2176 | `	}` |
|       - | 2177 | `	/* All done,return TRUE */` |
|       3 | 2178 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2179 | `	return PH7_OK;` |
|       2 | 2180 |  |
|       - | 2181 | `/*` |
|       - | 2182 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2183 | ` *  Sort an array by key.` |
|       - | 2184 | ` * Parameters` |
|       - | 2185 | ` *  $array` |
|       - | 2186 | ` *   The input array.` |
|       - | 2187 | ` * $sort_flags` |
|       - | 2188 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2189 | ` *  Sorting type flags:` |
|       - | 2190 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2191 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2192 | ` *   SORT_STRING - compare items as strings` |
|       - | 2193 | ` * Return` |
|       - | 2194 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2195 | ` */` |
|       4 | 2196 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2197 |  |
|       - | 2198 | `	ph7_hashmap *pMap;` |
|       - | 2199 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2200 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2201 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2202 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2203 | `		return PH7_OK;` |
|       - | 2204 | `	}` |
|       - | 2205 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2206 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2207 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2208 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2209 | `		if( nArg > 1 ){` |
|       - | 2210 | `			/* Extract comparison flags */` |
|     ! 0 | 2211 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2212 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2213 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2214 | `			}` |
|     ! 0 | 2215 | `		}` |
|       - | 2216 | `		/* Do the merge sort */` |
|       5 | 2217 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2218 | `		/* Fix the last link broken by the merge */` |
|      15 | 2219 | `		while(pMap->pLast->pPrev){` |
|      11 | 2220 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2221 | `		}` |
|       2 | 2222 | `	}` |
|       - | 2223 | `	/* All done,return TRUE */` |
|       5 | 2224 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2225 | `	return PH7_OK;` |
|       3 | 2226 |  |
|       - | 2227 | `/*` |
|       - | 2228 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2229 | ` *  Sort an array by key in reverse order.` |
|       - | 2230 | ` * Parameters` |
|       - | 2231 | ` *  $array` |
|       - | 2232 | ` *   The input array.` |
|       - | 2233 | ` * $sort_flags` |
|       - | 2234 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2235 | ` *  Sorting type flags:` |
|       - | 2236 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2237 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2238 | ` *   SORT_STRING - compare items as strings` |
|       - | 2239 | ` * Return` |
|       - | 2240 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2241 | ` */` |
|       2 | 2242 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2243 |  |
|       - | 2244 | `	ph7_hashmap *pMap;` |
|       - | 2245 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2246 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2247 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2248 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2249 | `		return PH7_OK;` |
|       - | 2250 | `	}` |
|       - | 2251 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2252 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2253 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2254 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2255 | `		if( nArg > 1 ){` |
|       - | 2256 | `			/* Extract comparison flags */` |
|     ! 0 | 2257 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2258 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2259 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2260 | `			}` |
|     ! 0 | 2261 | `		}` |
|       - | 2262 | `		/* Do the merge sort */` |
|       3 | 2263 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2264 | `		/* Fix the last link broken by the merge */` |
|       7 | 2265 | `		while(pMap->pLast->pPrev){` |
|       5 | 2266 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2267 | `		}` |
|       1 | 2268 | `	}` |
|       - | 2269 | `	/* All done,return TRUE */` |
|       3 | 2270 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2271 | `	return PH7_OK;` |
|       2 | 2272 |  |
|       - | 2273 | `/*` |
|       - | 2274 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2275 | ` * Sort an array in reverse order.` |
|       - | 2276 | ` * Parameters` |
|       - | 2277 | ` *  $array` |
|       - | 2278 | ` *   The input array.` |
|       - | 2279 | ` * $sort_flags` |
|       - | 2280 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2281 | ` *  Sorting type flags:` |
|       - | 2282 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2283 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2284 | ` *   SORT_STRING - compare items as strings` |
|       - | 2285 | ` * Return` |
|       - | 2286 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2287 | ` */` |
|       2 | 2288 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2289 |  |
|       - | 2290 | `	ph7_hashmap *pMap;` |
|       - | 2291 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2292 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2293 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2294 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2295 | `		return PH7_OK;` |
|       - | 2296 | `	}` |
|       - | 2297 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2298 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2299 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2300 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2301 | `		if( nArg > 1 ){` |
|       - | 2302 | `			/* Extract comparison flags */` |
|     ! 0 | 2303 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2304 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2305 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2306 | `			}` |
|     ! 0 | 2307 | `		}` |
|       - | 2308 | `		/* Do the merge sort */` |
|       3 | 2309 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2310 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2311 | `		HashmapSortRehash(pMap);` |
|       1 | 2312 | `	}` |
|       - | 2313 | `	/* All done,return TRUE */` |
|       3 | 2314 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2315 | `	return PH7_OK;` |
|       2 | 2316 |  |
|       - | 2317 | `/*` |
|       - | 2318 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2319 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2320 | ` * Parameters` |
|       - | 2321 | ` *  $array` |
|       - | 2322 | ` *   The input array.` |
|       - | 2323 | ` * $cmp_function` |
|       - | 2324 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2325 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2326 | ` *  to, or greater than the second.` |
|       - | 2327 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2328 | ` * Return` |
|       - | 2329 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2330 | ` */` |
|       2 | 2331 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2332 |  |
|       - | 2333 | `	ph7_hashmap *pMap;` |
|       - | 2334 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2335 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2336 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2337 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2338 | `		return PH7_OK;` |
|       - | 2339 | `	}` |
|       - | 2340 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2341 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2342 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2343 | `		ph7_value *pCallback = 0;` |
|       - | 2344 | `		ProcNodeCmp xCmp;` |
|       3 | 2345 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2346 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2347 | `			/* Point to the desired callback */` |
|       3 | 2348 | `			pCallback = apArg[1];` |
|       2 | 2349 | `		}else{` |
|       - | 2350 | `			/* Use the default comparison function */` |
|     ! 0 | 2351 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2352 | `		}` |
|       - | 2353 | `		/* Do the merge sort */` |
|       3 | 2354 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2355 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2356 | `		HashmapSortRehash(pMap);` |
|       1 | 2357 | `	}` |
|       - | 2358 | `	/* All done,return TRUE */` |
|       3 | 2359 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2360 | `	return PH7_OK;` |
|       2 | 2361 |  |
|       - | 2362 | `/*` |
|       - | 2363 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2364 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2365 | ` *  and maintain index association.` |
|       - | 2366 | ` * Parameters` |
|       - | 2367 | ` *  $array` |
|       - | 2368 | ` *   The input array.` |
|       - | 2369 | ` * $cmp_function` |
|       - | 2370 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2371 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2372 | ` *  to, or greater than the second.` |
|       - | 2373 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2374 | ` * Return` |
|       - | 2375 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2376 | ` */` |
|       2 | 2377 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2378 |  |
|       - | 2379 | `	ph7_hashmap *pMap;` |
|       - | 2380 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2381 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2382 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2383 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2384 | `		return PH7_OK;` |
|       - | 2385 | `	}` |
|       - | 2386 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2387 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2388 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2389 | `		ph7_value *pCallback = 0;` |
|       - | 2390 | `		ProcNodeCmp xCmp;` |
|       3 | 2391 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2392 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2393 | `			/* Point to the desired callback */` |
|       3 | 2394 | `			pCallback = apArg[1];` |
|       2 | 2395 | `		}else{` |
|       - | 2396 | `			/* Use the default comparison function */` |
|     ! 0 | 2397 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2398 | `		}` |
|       - | 2399 | `		/* Do the merge sort */` |
|       3 | 2400 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2401 | `		/* Fix the last link broken by the merge */` |
|       5 | 2402 | `		while(pMap->pLast->pPrev){` |
|       3 | 2403 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2404 | `		}` |
|       1 | 2405 | `	}` |
|       - | 2406 | `	/* All done,return TRUE */` |
|       3 | 2407 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2408 | `	return PH7_OK;` |
|       2 | 2409 |  |
|       - | 2410 | `/*` |
|       - | 2411 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2412 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2413 | ` *  function and maintain index association.` |
|       - | 2414 | ` * Parameters` |
|       - | 2415 | ` *  $array` |
|       - | 2416 | ` *   The input array.` |
|       - | 2417 | ` * $cmp_function` |
|       - | 2418 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2419 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2420 | ` *  to, or greater than the second.` |
|       - | 2421 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2422 | ` * Return` |
|       - | 2423 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2424 | ` */` |
|       2 | 2425 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2426 |  |
|       - | 2427 | `	ph7_hashmap *pMap;` |
|       - | 2428 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2429 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2430 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2431 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2432 | `		return PH7_OK;` |
|       - | 2433 | `	}` |
|       - | 2434 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2435 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2436 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2437 | `		ph7_value *pCallback = 0;` |
|       - | 2438 | `		ProcNodeCmp xCmp;` |
|       3 | 2439 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2440 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2441 | `			/* Point to the desired callback */` |
|       3 | 2442 | `			pCallback = apArg[1];` |
|       2 | 2443 | `		}else{` |
|       - | 2444 | `			/* Use the default comparison function */` |
|     ! 0 | 2445 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2446 | `		}` |
|       - | 2447 | `		/* Do the merge sort */` |
|       3 | 2448 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2449 | `		/* Fix the last link broken by the merge */` |
|       3 | 2450 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2451 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2452 | `		}` |
|       1 | 2453 | `	}` |
|       - | 2454 | `	/* All done,return TRUE */` |
|       3 | 2455 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2456 | `	return PH7_OK;` |
|       2 | 2457 |  |
|       - | 2458 | `/*` |
|       - | 2459 | ` * bool shuffle(array &$array)` |
|       - | 2460 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2461 | ` * Parameters` |
|       - | 2462 | ` *  $array` |
|       - | 2463 | ` *   The input array.` |
|       - | 2464 | ` * Return` |
|       - | 2465 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2466 | ` *` |
|       - | 2467 | ` */` |
|       2 | 2468 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2469 |  |
|       - | 2470 | `	ph7_hashmap *pMap;` |
|       - | 2471 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2472 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2473 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2474 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2475 | `		return PH7_OK;` |
|       - | 2476 | `	}` |
|       - | 2477 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2478 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2479 | `	if( pMap->nEntry > 1 ){` |
|       - | 2480 | `		/* Do the merge sort */` |
|       3 | 2481 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2482 | `		/* Fix the last link broken by the merge */` |
|      11 | 2483 | `		while(pMap->pLast->pPrev){` |
|       9 | 2484 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2485 | `		}` |
|       1 | 2486 | `	}` |
|       - | 2487 | `	/* All done,return TRUE */` |
|       3 | 2488 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2489 | `	return PH7_OK;` |
|       2 | 2490 |  |
|       - | 2491 | `/*` |
|       - | 2492 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2493 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2494 | ` * Parameters` |
|       - | 2495 | ` *  $var` |
|       - | 2496 | ` *   The array or the object.` |
|       - | 2497 | ` * $mode` |
|       - | 2498 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2499 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2500 | ` *  all the elements of a multidimensional array.` |
|       - | 2501 | ` * Return` |
|       - | 2502 | ` *  Returns the number of elements in the array.` |
|       - | 2503 | ` */` |
|     618 | 2504 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2505 |  |
|     620 | 2506 | `	int bRecursive = FALSE;` |
|     620 | 2507 | `	int bCycleDetected = FALSE;` |
|       - | 2508 | `	sxi64 iCount;` |
|     620 | 2509 | `	if( nArg < 1 ){` |
|       3 | 2510 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2511 | `			"ArgumentCountError",` |
|       - | 2512 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2513 | `			);` |
|       - | 2514 | `	}` |
|     618 | 2515 | `	if( nArg > 2 ){` |
|       4 | 2516 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2517 | `			"ArgumentCountError",` |
|       - | 2518 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2519 | `			nArg` |
|       - | 2520 | `			);` |
|       - | 2521 | `	}` |
|     616 | 2522 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2523 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2524 | `			"TypeError",` |
|       - | 2525 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       5 | 2526 | `			ph7_type_name(apArg[0])` |
|       - | 2527 | `			);` |
|       - | 2528 | `	}` |
|     606 | 2529 | `	if( nArg > 1 ){` |
|      34 | 2530 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      34 | 2531 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       5 | 2532 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2533 | `				"ValueError",` |
|       - | 2534 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2535 | `				);` |
|       - | 2536 | `		}` |
|      29 | 2537 | `		bRecursive = iMode == 1;` |
|      14 | 2538 | `	}` |
|       - | 2539 | `	/* Count */` |
|     602 | 2540 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     602 | 2541 | `	if( bCycleDetected ){` |
|       3 | 2542 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2543 | `	}` |
|     602 | 2544 | `	ph7_result_int64(pCtx,iCount);` |
|     602 | 2545 | `	return PH7_OK;` |
|     311 | 2546 |  |
|       - | 2547 | `/*` |
|       - | 2548 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2549 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2550 | ` * Parameters` |
|       - | 2551 | ` * $key` |
|       - | 2552 | ` *   Value to check.` |
|       - | 2553 | ` * $search` |
|       - | 2554 | ` *  An array with keys to check.` |
|       - | 2555 | ` * Return` |
|       - | 2556 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2557 | ` */` |
|      46 | 2558 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2559 |  |
|       - | 2560 | `	sxi32 rc;` |
|      48 | 2561 | `	if( nArg != 2 ){` |
|       - | 2562 | `		/* PHP requires exactly two arguments */` |
|      10 | 2563 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2564 | `			"ArgumentCountError",` |
|       - | 2565 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2566 | `			nArg` |
|       - | 2567 | `			);` |
|       - | 2568 | `	}` |
|       - | 2569 | `	/* Make sure we are dealing with a valid hashmap */` |
|      42 | 2570 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2571 | `		/* Type mismatch -> TypeError */` |
|       7 | 2572 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2573 | `			"TypeError",` |
|       - | 2574 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2575 | `			ph7_type_name(apArg[1])` |
|       - | 2576 | `			);` |
|       - | 2577 | `	}` |
|       - | 2578 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      37 | 2579 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2580 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2581 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2582 | `			"use an empty string instead"` |
|       - | 2583 | `			);` |
|      36 | 2584 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2585 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2586 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2587 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2588 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2589 | `				,rVal` |
|       - | 2590 | `				);` |
|       1 | 2591 | `		}` |
|       1 | 2592 | `	}` |
|       - | 2593 | `	/* Perform the lookup */` |
|      37 | 2594 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2595 | `	/* lookup result */` |
|      37 | 2596 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      37 | 2597 | `	return PH7_OK;` |
|      25 | 2598 |  |
|       - | 2599 | `/*` |
|       - | 2600 | ` * value array_pop(array $array)` |
|       - | 2601 | ` *   POP the last inserted element from the array.` |
|       - | 2602 | ` * Parameter` |
|       - | 2603 | ` *  The array to get the value from.` |
|       - | 2604 | ` * Return` |
|       - | 2605 | ` *  Poped value or NULL on failure.` |
|       - | 2606 | ` */` |
|      16 | 2607 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2608 |  |
|       - | 2609 | `	ph7_hashmap *pMap;` |
|       - | 2610 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      18 | 2611 | `	if( nArg != 1 ){` |
|       7 | 2612 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2613 | `			"ArgumentCountError",` |
|       - | 2614 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2615 | `			nArg` |
|       - | 2616 | `			);` |
|       - | 2617 | `	}` |
|       - | 2618 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2619 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      14 | 2620 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2621 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2622 | `			"Error",` |
|       - | 2623 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2624 | `			);` |
|       - | 2625 | `	}` |
|       - | 2626 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 | 2627 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2628 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2629 | `			"TypeError",` |
|       - | 2630 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2631 | `			ph7_type_name(apArg[0])` |
|       - | 2632 | `			);` |
|       - | 2633 | `	}` |
|       7 | 2634 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 2635 | `	if( pMap->nEntry < 1 ){` |
|       - | 2636 | `		/* Nothing to pop,return NULL */` |
|       3 | 2637 | `		ph7_result_null(pCtx);` |
|       2 | 2638 | `	}else{` |
|       5 | 2639 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2640 | `		ph7_value *pObj;` |
|       5 | 2641 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       5 | 2642 | `		if( pObj ){` |
|       - | 2643 | `			/* Node value */` |
|       5 | 2644 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2645 | `			/* Unlink the node */` |
|       5 | 2646 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       3 | 2647 | `		}else{` |
|     ! 0 | 2648 | `			ph7_result_null(pCtx);` |
|       - | 2649 | `		}` |
|       - | 2650 | `		/* Reset the cursor */` |
|       5 | 2651 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2652 | `	}` |
|       7 | 2653 | `	return PH7_OK;` |
|      10 | 2654 |  |
|       - | 2655 | `/*` |
|       - | 2656 | ` * int array_push($array,$var,...)` |
|       - | 2657 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2658 | ` * Parameters` |
|       - | 2659 | ` *  array` |
|       - | 2660 | ` *    The input array.` |
|       - | 2661 | ` *  var` |
|       - | 2662 | ` *   On or more value to push.` |
|       - | 2663 | ` * Return` |
|       - | 2664 | ` *  New array count (including old items).` |
|       - | 2665 | ` */` |
|      20 | 2666 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2667 |  |
|       - | 2668 | `	ph7_hashmap *pMap;` |
|       - | 2669 | `	sxi32 rc;` |
|       - | 2670 | `	int i;` |
|      22 | 2671 | `	if( nArg < 1 ){` |
|       4 | 2672 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2673 | `			"ArgumentCountError",` |
|       - | 2674 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2675 | `			nArg` |
|       - | 2676 | `			);` |
|       - | 2677 | `	}` |
|       - | 2678 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2679 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      20 | 2680 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2681 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2682 | `			"Error",` |
|       - | 2683 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2684 | `			);` |
|       - | 2685 | `	}` |
|       - | 2686 | `	/* Make sure we are dealing with a valid hashmap */` |
|      16 | 2687 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2688 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2689 | `			"TypeError",` |
|       - | 2690 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2691 | `			ph7_type_name(apArg[0])` |
|       - | 2692 | `			);` |
|       - | 2693 | `	}` |
|       - | 2694 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 2695 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2696 | `	/* Start pushing given values */` |
|      27 | 2697 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      15 | 2698 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      15 | 2699 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2700 | `			break;` |
|       - | 2701 | `		}` |
|       8 | 2702 | `	}` |
|       - | 2703 | `	/* Return the new count */` |
|      13 | 2704 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      13 | 2705 | `	return PH7_OK;` |
|      12 | 2706 |  |
|       - | 2707 | `/*` |
|       - | 2708 | ` * value array_shift(array $array)` |
|       - | 2709 | ` *   Shift an element off the beginning of array.` |
|       - | 2710 | ` * Parameter` |
|       - | 2711 | ` *  The array to get the value from.` |
|       - | 2712 | ` * Return` |
|       - | 2713 | ` *  Shifted value or NULL on failure.` |
|       - | 2714 | ` */` |
|      36 | 2715 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2716 |  |
|       - | 2717 | `	ph7_hashmap *pMap;` |
|       - | 2718 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      38 | 2719 | `	if( nArg != 1 ){` |
|       7 | 2720 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2721 | `			"ArgumentCountError",` |
|       - | 2722 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2723 | `			nArg` |
|       - | 2724 | `			);` |
|       - | 2725 | `	}` |
|       - | 2726 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      34 | 2727 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2728 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2729 | `			"Error",` |
|       - | 2730 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2731 | `			);` |
|       - | 2732 | `	}` |
|       - | 2733 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 | 2734 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2735 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2736 | `			"TypeError",` |
|       - | 2737 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2738 | `			ph7_type_name(apArg[0])` |
|       - | 2739 | `			);` |
|       - | 2740 | `	}` |
|       - | 2741 | `	/* Point to the internal representation of the hashmap */` |
|      28 | 2742 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      28 | 2743 | `	if( pMap->nEntry < 1 ){` |
|       - | 2744 | `		/* Empty hashmap,return NULL */` |
|       3 | 2745 | `		ph7_result_null(pCtx);` |
|       2 | 2746 | `	}else{` |
|      26 | 2747 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2748 | `		ph7_value *pObj;` |
|       - | 2749 | `		sxu32 n;` |
|      26 | 2750 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      26 | 2751 | `		if( pObj ){` |
|       - | 2752 | `			/* Node value */` |
|      26 | 2753 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2754 | `			/* Unlink the first node */` |
|      26 | 2755 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      14 | 2756 | `		}else{` |
|     ! 0 | 2757 | `			ph7_result_null(pCtx);` |
|       - | 2758 | `		}` |
|       - | 2759 | `		/* Rehash all int keys */` |
|      26 | 2760 | `		n = pMap->nEntry;` |
|      26 | 2761 | `		pEntry = pMap->pFirst;` |
|      26 | 2762 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      37 | 2763 | `		for(;;){` |
|      76 | 2764 | `			if( n < 1 ){` |
|      26 | 2765 | `				break;` |
|       - | 2766 | `			}` |
|      52 | 2767 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      52 | 2768 | `				HashmapRehashIntNode(pEntry);` |
|      25 | 2769 | `			}` |
|       - | 2770 | `			/* Point to the next entry */` |
|      52 | 2771 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      52 | 2772 | `			n--;` |
|       2 | 2773 | `		}` |
|       - | 2774 | `		/* Reset the cursor */` |
|      26 | 2775 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2776 | `	}` |
|      28 | 2777 | `	return PH7_OK;` |
|      20 | 2778 |  |
|       - | 2779 | `/*` |
|       - | 2780 | ` * Extract the node cursor value.` |
|       - | 2781 | ` */` |
|      24 | 2782 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 2783 |  |
|      25 | 2784 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 2785 | `	ph7_value *pVal;` |
|      25 | 2786 | `	if( pCur == 0 ){` |
|       - | 2787 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 2788 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2789 | `		return PH7_OK;` |
|       - | 2790 | `	}` |
|      25 | 2791 | `	if( iDirection != 0 ){` |
|       9 | 2792 | `		if( iDirection > 0 ){` |
|       - | 2793 | `			/* Point to the next entry */` |
|       7 | 2794 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 2795 | `			pCur = pMap->pCur;` |
|       4 | 2796 | `		}else{` |
|       - | 2797 | `			/* Point to the previous entry */` |
|       3 | 2798 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 2799 | `			pCur = pMap->pCur;` |
|       - | 2800 | `		}` |
|       9 | 2801 | `		if( pCur == 0 ){` |
|       - | 2802 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 2803 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2804 | `			return PH7_OK;` |
|       - | 2805 | `		}` |
|       4 | 2806 | `	}` |
|       - | 2807 | `	/* Point to the desired element */` |
|      25 | 2808 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 2809 | `	if( pVal ){` |
|      25 | 2810 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 2811 | `	}else{` |
|     ! 0 | 2812 | `		ph7_result_bool(pCtx,0);` |
|       - | 2813 | `	}` |
|      25 | 2814 | `	return PH7_OK;` |
|      13 | 2815 |  |
|       - | 2816 | `/*` |
|       - | 2817 | ` * value current(array $array)` |
|       - | 2818 | ` *  Return the current element in an array.` |
|       - | 2819 | ` * Parameter` |
|       - | 2820 | ` *  $input: The input array.` |
|       - | 2821 | ` * Return` |
|       - | 2822 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 2823 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2824 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2825 | ` *  is empty, current() returns FALSE.` |
|       - | 2826 | ` */` |
|      10 | 2827 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2828 |  |
|      11 | 2829 | `	if( nArg < 1 ){` |
|       - | 2830 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2831 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2832 | `		return PH7_OK;` |
|       - | 2833 | `	}` |
|       - | 2834 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 2835 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2836 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2837 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2838 | `		return PH7_OK;` |
|       - | 2839 | `	}` |
|      11 | 2840 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 2841 | `	return PH7_OK;` |
|       6 | 2842 |  |
|       - | 2843 | `/*` |
|       - | 2844 | ` * value next(array $input)` |
|       - | 2845 | ` *  Advance the internal array pointer of an array.` |
|       - | 2846 | ` * Parameter` |
|       - | 2847 | ` *  $input: The input array.` |
|       - | 2848 | ` * Return` |
|       - | 2849 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 2850 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 2851 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 2852 | ` */` |
|       6 | 2853 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2854 |  |
|       7 | 2855 | `	if( nArg < 1 ){` |
|       - | 2856 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2857 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2858 | `		return PH7_OK;` |
|       - | 2859 | `	}` |
|       - | 2860 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 2861 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2862 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2863 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2864 | `		return PH7_OK;` |
|       - | 2865 | `	}` |
|       7 | 2866 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 2867 | `	return PH7_OK;` |
|       4 | 2868 |  |
|       - | 2869 | `/*` |
|       - | 2870 | ` * value prev(array $input)` |
|       - | 2871 | ` *  Rewind the internal array pointer.` |
|       - | 2872 | ` * Parameter` |
|       - | 2873 | ` *  $input: The input array.` |
|       - | 2874 | ` * Return` |
|       - | 2875 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 2876 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 2877 | ` *  elements.` |
|       - | 2878 | ` */` |
|       2 | 2879 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2880 |  |
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
|       3 | 2892 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 2893 | `	return PH7_OK;` |
|       2 | 2894 |  |
|       - | 2895 | `/*` |
|       - | 2896 | ` * value end(array $input)` |
|       - | 2897 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 2898 | ` * Parameter` |
|       - | 2899 | ` *  $input: The input array.` |
|       - | 2900 | ` * Return` |
|       - | 2901 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 2902 | ` */` |
|       2 | 2903 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2904 |  |
|       - | 2905 | `	ph7_hashmap *pMap;` |
|       3 | 2906 | `	if( nArg < 1 ){` |
|       - | 2907 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2908 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2909 | `		return PH7_OK;` |
|       - | 2910 | `	}` |
|       - | 2911 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2912 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2913 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2914 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2915 | `		return PH7_OK;` |
|       - | 2916 | `	}` |
|       - | 2917 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2918 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2919 | `	/* Point to the last node */` |
|       3 | 2920 | `	pMap->pCur = pMap->pLast;` |
|       - | 2921 | `	/* Return the last node value */` |
|       3 | 2922 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 2923 | `	return PH7_OK;` |
|       2 | 2924 |  |
|       - | 2925 | `/*` |
|       - | 2926 | ` * value reset(array $array )` |
|       - | 2927 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 2928 | ` * Parameter` |
|       - | 2929 | ` *  $input: The input array.` |
|       - | 2930 | ` * Return` |
|       - | 2931 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 2932 | ` */` |
|       4 | 2933 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2934 |  |
|       - | 2935 | `	ph7_hashmap *pMap;` |
|       5 | 2936 | `	if( nArg < 1 ){` |
|       - | 2937 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2938 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2939 | `		return PH7_OK;` |
|       - | 2940 | `	}` |
|       - | 2941 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2942 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2943 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2944 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2945 | `		return PH7_OK;` |
|       - | 2946 | `	}` |
|       - | 2947 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2948 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2949 | `	/* Point to the first node */` |
|       5 | 2950 | `	pMap->pCur = pMap->pFirst;` |
|       - | 2951 | `	/* Return the last node value if available */` |
|       5 | 2952 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 2953 | `	return PH7_OK;` |
|       3 | 2954 |  |
|       - | 2955 | `/*` |
|       - | 2956 | ` * value key(array $array)` |
|       - | 2957 | ` *   Fetch a key from an array` |
|       - | 2958 | ` * Parameter` |
|       - | 2959 | ` *  $input` |
|       - | 2960 | ` *   The input array.` |
|       - | 2961 | ` * Return` |
|       - | 2962 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 2963 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2964 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2965 | ` *  is empty, key() returns NULL.` |
|       - | 2966 | ` */` |
|       4 | 2967 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2968 |  |
|       - | 2969 | `	ph7_hashmap_node *pCur;` |
|       - | 2970 | `	ph7_hashmap *pMap;` |
|       5 | 2971 | `	if( nArg < 1 ){` |
|       - | 2972 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 2973 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2974 | `		return PH7_OK;` |
|       - | 2975 | `	}` |
|       - | 2976 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2977 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2978 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 2979 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2980 | `		return PH7_OK;` |
|       - | 2981 | `	}` |
|       5 | 2982 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2983 | `	pCur = pMap->pCur;` |
|       5 | 2984 | `	if( pCur == 0 ){` |
|       - | 2985 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 2986 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2987 | `		return PH7_OK;` |
|       - | 2988 | `	}` |
|       5 | 2989 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 2990 | `		/* Key is integer */` |
|     ! 0 | 2991 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 2992 | `	}else{` |
|       - | 2993 | `		/* Key is blob */` |
|       7 | 2994 | `		ph7_result_string(pCtx,` |
|       4 | 2995 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 2996 | `	}` |
|       5 | 2997 | `	return PH7_OK;` |
|       3 | 2998 |  |
|       - | 2999 | `/*` |
|       - | 3000 | ` * array each(array $input)` |
|       - | 3001 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3002 | ` * Parameter` |
|       - | 3003 | ` *  $input` |
|       - | 3004 | ` *    The input array.` |
|       - | 3005 | ` * Return` |
|       - | 3006 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3007 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3008 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3009 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3010 | ` *  each() returns FALSE.` |
|       - | 3011 | ` */` |
|      22 | 3012 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3013 |  |
|       - | 3014 | `	ph7_hashmap_node *pCur;` |
|       - | 3015 | `	ph7_hashmap *pMap;` |
|       - | 3016 | `	ph7_value *pArray;` |
|       - | 3017 | `	ph7_value *pVal;` |
|       - | 3018 | `	ph7_value sKey;` |
|      23 | 3019 | `	if( nArg < 1 ){` |
|       - | 3020 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3021 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3022 | `		return PH7_OK;` |
|       - | 3023 | `	}` |
|       - | 3024 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3025 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3026 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3027 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3028 | `		return PH7_OK;` |
|       - | 3029 | `	}` |
|       - | 3030 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3031 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3032 | `	if( pMap->pCur == 0 ){` |
|       - | 3033 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3034 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3035 | `		return PH7_OK;` |
|       - | 3036 | `	}` |
|      15 | 3037 | `	pCur = pMap->pCur;` |
|       - | 3038 | `	/* Create a new array */` |
|      15 | 3039 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3040 | `	if( pArray == 0 ){` |
|     ! 0 | 3041 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3042 | `		return PH7_OK;` |
|       - | 3043 | `	}` |
|      15 | 3044 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3045 | `	/* Insert the current value */` |
|      15 | 3046 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3047 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3048 | `	/* Make the key */` |
|      15 | 3049 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3050 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3051 | `	}else{` |
|       9 | 3052 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3053 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3054 | `	}` |
|       - | 3055 | `	/* Insert the current key */` |
|      15 | 3056 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3057 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3058 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3059 | `	/* Advance the cursor */` |
|      15 | 3060 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3061 | `	/* Return the current entry */` |
|      15 | 3062 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3063 | `	return PH7_OK;` |
|      12 | 3064 |  |
|       - | 3065 | `/*` |
|       - | 3066 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3067 | ` *  Create an array containing a range of elements` |
|       - | 3068 | ` * Parameter` |
|       - | 3069 | ` *  start` |
|       - | 3070 | ` *   First value of the sequence.` |
|       - | 3071 | ` *  limit` |
|       - | 3072 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3073 | ` *  step` |
|       - | 3074 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3075 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3076 | ` * Return` |
|       - | 3077 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3078 | ` * NOTE:` |
|       - | 3079 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3080 | ` */` |
|       2 | 3081 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3082 |  |
|       - | 3083 | `	ph7_value *pValue,*pArray;` |
|       - | 3084 | `	sxi64 iOfft,iLimit;` |
|       3 | 3085 | `	int iStep = 1;` |
|       - | 3086 |  |
|       3 | 3087 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3088 | `	if( nArg > 0 ){` |
|       - | 3089 | `		/* Extract the offset */` |
|       3 | 3090 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3091 | `		if( nArg > 1 ){` |
|       - | 3092 | `			/* Extract the limit */` |
|       3 | 3093 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3094 | `			if( nArg > 2 ){` |
|       - | 3095 | `				/* Extract the increment */` |
|       3 | 3096 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3097 | `				if( iStep < 1 ){` |
|       - | 3098 | `					/* Only positive number are allowed */` |
|       3 | 3099 | `					iStep = 1;` |
|       1 | 3100 | `				}` |
|       1 | 3101 | `			}` |
|       1 | 3102 | `		}` |
|       1 | 3103 | `	}` |
|       - | 3104 | `	/* Element container */` |
|       3 | 3105 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3106 | `	/* Create the new array */` |
|       3 | 3107 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3108 | `	if( pArray == 0 ){` |
|     ! 0 | 3109 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3110 | `		return PH7_OK;` |
|       - | 3111 | `	}` |
|       - | 3112 | `	/* Start filling */` |
|       3 | 3113 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3114 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3115 | `		/* Perform the insertion */` |
|     ! 0 | 3116 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3117 | `		/* Increment */` |
|     ! 0 | 3118 | `		iOfft += iStep;` |
|     ! 0 | 3119 | `	}` |
|       - | 3120 | `	/* Return the new array */` |
|       3 | 3121 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3122 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3123 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3124 | `	 */` |
|       3 | 3125 | `	return PH7_OK;` |
|       2 | 3126 |  |
|       - | 3127 | `/*` |
|       - | 3128 | ` * array array_values(array $array)` |
|       - | 3129 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3130 | ` * Parameters` |
|       - | 3131 | ` *  $array` |
|       - | 3132 | ` *   The input array.` |
|       - | 3133 | ` * Return` |
|       - | 3134 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3135 | ` */` |
|      30 | 3136 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3137 |  |
|       - | 3138 | `	ph7_hashmap_node *pNode;` |
|       - | 3139 | `	ph7_hashmap *pMap;` |
|       - | 3140 | `	ph7_value *pArray;` |
|       - | 3141 | `	ph7_value *pObj;` |
|       - | 3142 | `	sxu32 n;` |
|      32 | 3143 | `	if( nArg != 1 ){` |
|       - | 3144 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3145 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3146 | `			"ArgumentCountError",` |
|       - | 3147 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3148 | `			nArg` |
|       - | 3149 | `			);` |
|       - | 3150 | `	}` |
|       - | 3151 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3152 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3153 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3154 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3155 | `			"TypeError",` |
|       - | 3156 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3157 | `			ph7_type_name(apArg[0])` |
|       - | 3158 | `			);` |
|       - | 3159 | `	}` |
|       - | 3160 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3161 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3162 | `	/* Create a new array */` |
|      25 | 3163 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3164 | `	if( pArray == 0 ){` |
|     ! 0 | 3165 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3166 | `		return PH7_OK;` |
|       - | 3167 | `	}` |
|       - | 3168 | `	/* Perform the requested operation */` |
|      25 | 3169 | `	pNode = pMap->pFirst;` |
|      83 | 3170 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3171 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3172 | `		if( pObj ){` |
|       - | 3173 | `			/* perform the insertion */` |
|      59 | 3174 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3175 | `		}` |
|       - | 3176 | `		/* Point to the next entry */` |
|      59 | 3177 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3178 | `	}` |
|       - | 3179 | `	/* return the new array */` |
|      25 | 3180 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3181 | `	return PH7_OK;` |
|      17 | 3182 |  |
|       - | 3183 | `/*` |
|       - | 3184 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3185 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3186 | ` * Parameters` |
|       - | 3187 | ` *  $input` |
|       - | 3188 | ` *   An array containing keys to return.` |
|       - | 3189 | ` * $search_value` |
|       - | 3190 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3191 | ` * $strict` |
|       - | 3192 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3193 | ` * Return` |
|       - | 3194 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3195 | ` */` |
|     120 | 3196 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3197 |  |
|       - | 3198 | `	ph7_hashmap_node *pNode;` |
|       - | 3199 | `	ph7_hashmap *pMap;` |
|       - | 3200 | `	ph7_value *pArray;` |
|       - | 3201 | `	ph7_value sObj;` |
|       - | 3202 | `	ph7_value sVal;` |
|       - | 3203 | `	SyString sKey;` |
|       - | 3204 | `	int bStrict;` |
|       - | 3205 | `	sxi32 rc;` |
|       - | 3206 | `	sxu32 n;` |
|     122 | 3207 | `	if( nArg < 1 ){` |
|       - | 3208 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3209 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3210 | `			"ArgumentCountError",` |
|       - | 3211 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3212 | `			);` |
|       - | 3213 | `	}` |
|       - | 3214 | `	/* Make sure we are dealing with a valid hashmap */` |
|     120 | 3215 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3216 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3217 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3218 | `			"TypeError",` |
|       - | 3219 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3220 | `			ph7_type_name(apArg[0])` |
|       - | 3221 | `			);` |
|       - | 3222 | `	}` |
|       - | 3223 | `	/* Point to the internal representation of the input hashmap */` |
|     118 | 3224 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3225 | `	/* Create a new array */` |
|     118 | 3226 | `	pArray = ph7_context_new_array(pCtx);` |
|     118 | 3227 | `	if( pArray == 0 ){` |
|     ! 0 | 3228 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3229 | `		return PH7_OK;` |
|       - | 3230 | `	}` |
|     118 | 3231 | `	bStrict = FALSE;` |
|     118 | 3232 | `	if( nArg > 2 ){` |
|       - | 3233 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3234 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3235 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3236 | `				"TypeError",` |
|       - | 3237 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3238 | `				ph7_type_name(apArg[2])` |
|       - | 3239 | `				);` |
|       - | 3240 | `		}` |
|       5 | 3241 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3242 | `	}` |
|       - | 3243 | `	/* Perform the requested operation */` |
|     115 | 3244 | `	pNode = pMap->pFirst;` |
|     115 | 3245 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     553 | 3246 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     439 | 3247 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     117 | 3248 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      59 | 3249 | `		}else{` |
|     323 | 3250 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3251 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3252 | `		}` |
|     439 | 3253 | `		rc = 0;` |
|     439 | 3254 | `		if( nArg > 1 ){` |
|      31 | 3255 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3256 | `			if( pValue ){` |
|      31 | 3257 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3258 | `				/* Filter key */` |
|      31 | 3259 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3260 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3261 | `			}` |
|      15 | 3262 | `		}` |
|     439 | 3263 | `		if( rc == 0 ){` |
|       - | 3264 | `			/* Perform the insertion */` |
|     421 | 3265 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     210 | 3266 | `		}` |
|     439 | 3267 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3268 | `		/* Point to the next entry */` |
|     439 | 3269 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     220 | 3270 | `	}` |
|       - | 3271 | `	/* return the new array */` |
|     115 | 3272 | `	ph7_result_value(pCtx,pArray);` |
|     115 | 3273 | `	return PH7_OK;` |
|      62 | 3274 |  |
|       - | 3275 | `/*` |
|       - | 3276 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3277 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3278 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3279 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3280 | ` * Parameters` |
|       - | 3281 | ` *  $arr1` |
|       - | 3282 | ` *   First array` |
|       - | 3283 | ` *  $arr2` |
|       - | 3284 | ` *   Second array` |
|       - | 3285 | ` * Return` |
|       - | 3286 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3287 | ` * Note` |
|       - | 3288 | ` *  This function is a symisc eXtension.` |
|       - | 3289 | ` */` |
|       4 | 3290 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3291 |  |
|       - | 3292 | `	ph7_hashmap *p1,*p2;` |
|       - | 3293 | `	int rc;` |
|       5 | 3294 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3295 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3296 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3297 | `		return PH7_OK;` |
|       - | 3298 | `	}` |
|       - | 3299 | `	/* Point to the hashmaps */` |
|       5 | 3300 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3301 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3302 | `	rc = (p1 == p2);` |
|       - | 3303 | `	/* Same instance? */` |
|       5 | 3304 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3305 | `	return PH7_OK;` |
|       3 | 3306 |  |
|       - | 3307 | `/*` |
|       - | 3308 | ` * array array_merge(array ...$arrays)` |
|       - | 3309 | ` *  Merge one or more arrays.` |
|       - | 3310 | ` * Parameters` |
|       - | 3311 | ` *  ...$arrays` |
|       - | 3312 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3313 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3314 | ` * Return` |
|       - | 3315 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3316 | ` *  with no arguments.` |
|       - | 3317 | ` */` |
|     872 | 3318 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3319 |  |
|       - | 3320 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3321 | `	ph7_value *pArray;` |
|       - | 3322 | `	int i;` |
|       - | 3323 | `	/* Create a new array */` |
|     874 | 3324 | `	pArray = ph7_context_new_array(pCtx);` |
|     874 | 3325 | `	if( pArray == 0 ){` |
|     ! 0 | 3326 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3327 | `		return PH7_OK;` |
|       - | 3328 | `	}` |
|       - | 3329 | `	/* Point to the internal representation of the hashmap */` |
|     874 | 3330 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3331 | `	/* Start merging */` |
|    2608 | 3332 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3333 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1740 | 3334 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3335 | `			/* Type mismatch -> TypeError */` |
|       7 | 3336 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3337 | `				"TypeError",` |
|       - | 3338 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3339 | `				i + 1,` |
|       4 | 3340 | `				ph7_type_name(apArg[i])` |
|       - | 3341 | `				);` |
|     ! 0 | 3342 | `		}else{` |
|    1736 | 3343 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3344 | `			/* Merge the two hashmaps */` |
|    1736 | 3345 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3346 | `		}` |
|     869 | 3347 | `	}` |
|       - | 3348 | `	/* Return the freshly created array */` |
|     870 | 3349 | `	ph7_result_value(pCtx,pArray);` |
|     870 | 3350 | `	return PH7_OK;` |
|     438 | 3351 |  |
|       - | 3352 | `/*` |
|       - | 3353 | ` * array array_copy(array $source)` |
|       - | 3354 | ` *  Make a blind copy of the target array.` |
|       - | 3355 | ` * Parameters` |
|       - | 3356 | ` *  $source` |
|       - | 3357 | ` *   Target array` |
|       - | 3358 | ` * Return` |
|       - | 3359 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3360 | ` * Note` |
|       - | 3361 | ` *  This function is a symisc eXtension.` |
|       - | 3362 | ` */` |
|      16 | 3363 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3364 |  |
|       - | 3365 | `	ph7_hashmap *pMap;` |
|       - | 3366 | `	ph7_value *pArray;` |
|      17 | 3367 | `	if( nArg < 1 ){` |
|       - | 3368 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3369 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3370 | `		return PH7_OK;` |
|       - | 3371 | `	}` |
|       - | 3372 | `	/* Create a new array */` |
|      17 | 3373 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3374 | `	if( pArray == 0 ){` |
|     ! 0 | 3375 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3376 | `		return PH7_OK;` |
|       - | 3377 | `	}` |
|       - | 3378 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3379 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3380 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3381 | `		/* Point to the internal representation of the source */` |
|      17 | 3382 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3383 | `		/* Perform the copy */` |
|      17 | 3384 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3385 | `	}else{` |
|       - | 3386 | `		/* Simple insertion */` |
|     ! 0 | 3387 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3388 | `	}` |
|       - | 3389 | `	/* Return the duplicated array */` |
|      17 | 3390 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3391 | `	return PH7_OK;` |
|       9 | 3392 |  |
|       - | 3393 | `/*` |
|       - | 3394 | ` * bool array_erase(array $source)` |
|       - | 3395 | ` *  Remove all elements from a given array.` |
|       - | 3396 | ` * Parameters` |
|       - | 3397 | ` *  $source` |
|       - | 3398 | ` *   Target array` |
|       - | 3399 | ` * Return` |
|       - | 3400 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3401 | ` * Note` |
|       - | 3402 | ` *  This function is a symisc eXtension.` |
|       - | 3403 | ` */` |
|      16 | 3404 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3405 |  |
|       - | 3406 | `	ph7_hashmap *pMap;` |
|      17 | 3407 | `	if( nArg < 1 ){` |
|       - | 3408 | `		/* Missing arguments */` |
|     ! 0 | 3409 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3410 | `		return PH7_OK;` |
|       - | 3411 | `	}` |
|       - | 3412 | `	/* Point to the target hashmap */` |
|      17 | 3413 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3414 | `	/* Erase */` |
|      17 | 3415 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3416 | `	return PH7_OK;` |
|       9 | 3417 |  |
|       - | 3418 | `/*` |
|       - | 3419 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3420 | ` *  Extract a slice of the array.` |
|       - | 3421 | ` * Parameters` |
|       - | 3422 | ` *  $array` |
|       - | 3423 | ` *    The input array.` |
|       - | 3424 | ` * $offset` |
|       - | 3425 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3426 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3427 | ` * $length (optional, nullable)` |
|       - | 3428 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3429 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3430 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3431 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3432 | ` * $preserve_keys (optional)` |
|       - | 3433 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3434 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3435 | ` * Return` |
|       - | 3436 | ` *   The new slice.` |
|       - | 3437 | ` */` |
|      46 | 3438 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3439 |  |
|       - | 3440 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3441 | `	ph7_hashmap_node *pCur;` |
|       - | 3442 | `	ph7_value *pArray;` |
|       - | 3443 | `	int iLength,iOfft;` |
|       - | 3444 | `	int bPreserve;` |
|       - | 3445 | `	sxi32 rc;` |
|      48 | 3446 | `	if( nArg < 2 ){` |
|       7 | 3447 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3448 | `			"ArgumentCountError",` |
|       - | 3449 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3450 | `			nArg` |
|       - | 3451 | `			);` |
|       - | 3452 | `	}` |
|      44 | 3453 | `	if( nArg > 4 ){` |
|       4 | 3454 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3455 | `			"ArgumentCountError",` |
|       - | 3456 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3457 | `			nArg` |
|       - | 3458 | `			);` |
|       - | 3459 | `	}` |
|      42 | 3460 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3461 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3462 | `			"TypeError",` |
|       - | 3463 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3464 | `			ph7_type_name(apArg[0])` |
|       - | 3465 | `			);` |
|       - | 3466 | `	}` |
|       - | 3467 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3468 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3469 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3470 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3471 | `			"TypeError",` |
|       - | 3472 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3473 | `			ph7_type_name(apArg[1])` |
|       - | 3474 | `			);` |
|       - | 3475 | `	}` |
|       - | 3476 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3477 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3478 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3479 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3480 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3481 | `				"TypeError",` |
|       - | 3482 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3483 | `				ph7_type_name(apArg[2])` |
|       - | 3484 | `				);` |
|       - | 3485 | `		}` |
|       8 | 3486 | `	}` |
|       - | 3487 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3488 | `	if( nArg > 3 ){` |
|      10 | 3489 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3490 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3491 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3492 | `				"TypeError",` |
|       - | 3493 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3494 | `				ph7_type_name(apArg[3])` |
|       - | 3495 | `				);` |
|       - | 3496 | `		}` |
|       2 | 3497 | `	}` |
|       - | 3498 | `	/* Point the internal representation of the target array */` |
|      33 | 3499 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3500 | `	bPreserve = FALSE;` |
|       - | 3501 | `	/* Get the offset */` |
|      33 | 3502 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3503 | `	if( iOfft < 0 ){` |
|       5 | 3504 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3505 | `		if( iOfft < 0 ){` |
|       3 | 3506 | `			iOfft = 0;` |
|       1 | 3507 | `		}` |
|       2 | 3508 | `	}` |
|      33 | 3509 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3510 | `		/* Offset past end of array, return empty array */` |
|       5 | 3511 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3512 | `		if( pArray == 0 ){` |
|     ! 0 | 3513 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3514 | `			return PH7_OK;` |
|       - | 3515 | `		}` |
|       5 | 3516 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3517 | `		return PH7_OK;` |
|       - | 3518 | `	}` |
|       - | 3519 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3520 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3521 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3522 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3523 | `		if( iLength < 0 ){` |
|       5 | 3524 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3525 | `		}` |
|      15 | 3526 | `		if( iLength < 0 ){` |
|       3 | 3527 | `			iLength = 0;` |
|       1 | 3528 | `		}` |
|      15 | 3529 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3530 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3531 | `		}` |
|       7 | 3532 | `	}` |
|      29 | 3533 | `	if( nArg > 3 ){` |
|       5 | 3534 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3535 | `	}` |
|       - | 3536 | `	/* Create a new array */` |
|      29 | 3537 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3538 | `	if( pArray == 0 ){` |
|     ! 0 | 3539 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3540 | `		return PH7_OK;` |
|       - | 3541 | `	}` |
|      29 | 3542 | `	if( iLength < 1 ){` |
|       - | 3543 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3544 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3545 | `		return PH7_OK;` |
|       - | 3546 | `	}` |
|       - | 3547 | `	/* Point to the desired entry */` |
|      25 | 3548 | `	pCur = pSrc->pFirst;` |
|      24 | 3549 | `	for(;;){` |
|      49 | 3550 | `		if( iOfft < 1 ){` |
|      25 | 3551 | `			break;` |
|       - | 3552 | `		}` |
|       - | 3553 | `		/* Point to the next entry */` |
|      25 | 3554 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3555 | `		iOfft--;` |
|       1 | 3556 | `	}` |
|       - | 3557 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3558 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3559 | `	for(;;){` |
|      79 | 3560 | `		if( iLength < 1 ){` |
|      25 | 3561 | `			break;` |
|       - | 3562 | `		}` |
|       - | 3563 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3564 | `		{` |
|      55 | 3565 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3566 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3567 | `		}` |
|      55 | 3568 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3569 | `			break;` |
|       - | 3570 | `		}` |
|       - | 3571 | `		/* Point to the next entry */` |
|      55 | 3572 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3573 | `		iLength--;` |
|       1 | 3574 | `	}` |
|       - | 3575 | `	/* Return the freshly created array */` |
|      25 | 3576 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3577 | `	return PH7_OK;` |
|      25 | 3578 |  |
|       - | 3579 | `/*` |
|       - | 3580 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3581 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3582 | ` * beginning (becomes the new pFirst).` |
|       - | 3583 | ` */` |
|      30 | 3584 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3585 |  |
|       - | 3586 | `	ph7_hashmap_node *pNode;` |
|       - | 3587 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3588 | `	pNode = pMap->pLast;` |
|      31 | 3589 | `	if( pNode == 0 ){` |
|     ! 0 | 3590 | `		return;` |
|       - | 3591 | `	}` |
|      31 | 3592 | `	if( pNode->pNext == 0 ){` |
|       - | 3593 | `		/* Only node in the list, nothing to move */` |
|       5 | 3594 | `		return;` |
|       - | 3595 | `	}` |
|      27 | 3596 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3597 | `		/* Already in the correct position */` |
|       9 | 3598 | `		return;` |
|       - | 3599 | `	}` |
|       - | 3600 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3601 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3602 | `	pMap->pLast->pPrev = 0;` |
|       - | 3603 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3604 | `	if( pAfter == 0 ){` |
|       - | 3605 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3606 | `		pNode->pNext = 0;` |
|       3 | 3607 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3608 | `		if( pMap->pFirst ){` |
|       3 | 3609 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3610 | `		}` |
|       3 | 3611 | `		pMap->pFirst = pNode;` |
|       2 | 3612 | `	}else{` |
|      17 | 3613 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3614 | `		pNode->pPrev = pOldNext;` |
|      17 | 3615 | `		pNode->pNext = pAfter;` |
|      17 | 3616 | `		pAfter->pPrev = pNode;` |
|      17 | 3617 | `		if( pOldNext ){` |
|      17 | 3618 | `			pOldNext->pNext = pNode;` |
|       9 | 3619 | `		}else{` |
|     ! 0 | 3620 | `			pMap->pLast = pNode;` |
|       - | 3621 | `		}` |
|       - | 3622 | `	}` |
|      16 | 3623 |  |
|       - | 3624 | `/*` |
|       - | 3625 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3626 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3627 | ` * Parameters` |
|       - | 3628 | ` *  $array` |
|       - | 3629 | ` *    The input array.` |
|       - | 3630 | ` *  $offset` |
|       - | 3631 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3632 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3633 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3634 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3635 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3636 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3637 | ` *  $length (optional)` |
|       - | 3638 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3639 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3640 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3641 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3642 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3643 | ` *  $replacement (optional)` |
|       - | 3644 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3645 | ` *    with elements from this array.` |
|       - | 3646 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3647 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3648 | ` *    offset.` |
|       - | 3649 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3650 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3651 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3652 | ` * Return` |
|       - | 3653 | ` *   A new array consisting of the extracted elements.` |
|       - | 3654 | ` */` |
|      54 | 3655 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3656 |  |
|       - | 3657 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3658 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3659 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3660 | `	int iLength,iOfft,i;` |
|       - | 3661 | `	sxi32 rc;` |
|      56 | 3662 | `	if( nArg < 2 ){` |
|       7 | 3663 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3664 | `			"ArgumentCountError",` |
|       - | 3665 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3666 | `			nArg` |
|       - | 3667 | `			);` |
|       - | 3668 | `	}` |
|      52 | 3669 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3670 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3671 | `			"TypeError",` |
|       - | 3672 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3673 | `			ph7_type_name(apArg[0])` |
|       - | 3674 | `			);` |
|       - | 3675 | `	}` |
|       - | 3676 | `	/* Point to the internal representation of the target array */` |
|      49 | 3677 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3678 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3679 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3680 | `	if( iOfft < 0 ){` |
|       7 | 3681 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3682 | `		if( iOfft < 0 ){` |
|       3 | 3683 | `			iOfft = 0;` |
|       2 | 3684 | `		}` |
|      46 | 3685 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3686 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3687 | `	}` |
|       - | 3688 | `	/* Get the length and clamp to valid range.` |
|       - | 3689 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3690 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3691 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3692 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3693 | `		if( iLength < 0 ){` |
|       7 | 3694 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3695 | `			if( iLength < 0 ){` |
|       3 | 3696 | `				iLength = 0;` |
|       1 | 3697 | `			}` |
|       3 | 3698 | `		}` |
|      31 | 3699 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3700 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3701 | `		}` |
|      15 | 3702 | `	}` |
|       - | 3703 | `	/* Create the result array for removed elements */` |
|      49 | 3704 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3705 | `	if( pArray == 0 ){` |
|     ! 0 | 3706 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3707 | `		return PH7_OK;` |
|       - | 3708 | `	}` |
|       - | 3709 | `	/* Get replacement array if provided */` |
|      49 | 3710 | `	pRep = 0;` |
|      49 | 3711 | `	if( nArg > 3 ){` |
|      21 | 3712 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3713 | `			/* Perform an array cast */` |
|       3 | 3714 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3715 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3716 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3717 | `			}` |
|       2 | 3718 | `		}else{` |
|      19 | 3719 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3720 | `		}` |
|      21 | 3721 | `		if( pRep ){` |
|       - | 3722 | `			/* Reset the loop cursor */` |
|      21 | 3723 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3724 | `		}` |
|      10 | 3725 | `	}` |
|       - | 3726 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3727 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3728 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3729 | `		return PH7_OK;` |
|       - | 3730 | `	}` |
|       - | 3731 | `	/* Navigate to the offset position */` |
|      41 | 3732 | `	pCur = pSrc->pFirst;` |
|      85 | 3733 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3734 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3735 | `	}` |
|       - | 3736 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3737 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3738 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3739 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3740 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3741 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3742 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3743 | `		pPrev = pCur->pPrev;` |
|      71 | 3744 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3745 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3746 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3747 | `			break;` |
|       - | 3748 | `		}` |
|      71 | 3749 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3750 | `	}` |
|       - | 3751 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3752 | `	if( pRep ){` |
|       - | 3753 | `		ph7_value sSafeVal;` |
|      61 | 3754 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3755 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3756 | `			if( pRvalue ){` |
|       - | 3757 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3758 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3759 | `				 * since it points into that same pool. */` |
|      31 | 3760 | `				sSafeVal = *pRvalue;` |
|      31 | 3761 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 3762 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 3763 | `					pNewNode = pSrc->pLast;` |
|      31 | 3764 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 3765 | `					pInsertAfter = pNewNode;` |
|      15 | 3766 | `				}` |
|      15 | 3767 | `			}` |
|       1 | 3768 | `		}` |
|      10 | 3769 | `	}` |
|       - | 3770 | `	/* Return the freshly created array */` |
|      41 | 3771 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 3772 | `	return PH7_OK;` |
|      29 | 3773 |  |
|       - | 3774 | `/*` |
|       - | 3775 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3776 | ` *  Checks if a value exists in an array.` |
|       - | 3777 | ` * Parameters` |
|       - | 3778 | ` *  $needle` |
|       - | 3779 | ` *   The searched value.` |
|       - | 3780 | ` *   Note:` |
|       - | 3781 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3782 | ` * $haystack` |
|       - | 3783 | ` *  The target array.` |
|       - | 3784 | ` * $strict` |
|       - | 3785 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3786 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3787 | ` */` |
|   21186 | 3788 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3789 |  |
|       - | 3790 | `	ph7_value *pNeedle;` |
|       - | 3791 | `	int bStrict;` |
|       - | 3792 | `	int rc;` |
|   21188 | 3793 | `	if( nArg < 2 ){` |
|       - | 3794 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3795 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3796 | `		return PH7_OK;` |
|       - | 3797 | `	}` |
|   21188 | 3798 | `	pNeedle = apArg[0];` |
|   21188 | 3799 | `	bStrict = 0;` |
|   21188 | 3800 | `	if( nArg > 2 ){` |
|       5 | 3801 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3802 | `	}` |
|   21188 | 3803 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3804 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3805 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3806 | `		/* Set the comparison result */` |
|     ! 0 | 3807 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3808 | `		return PH7_OK;` |
|       - | 3809 | `	}` |
|       - | 3810 | `	/* Perform the lookup */` |
|   21188 | 3811 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3812 | `	/* Lookup result */` |
|   21188 | 3813 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   21188 | 3814 | `	return PH7_OK;` |
|   10595 | 3815 |  |
|       - | 3816 | `/*` |
|       - | 3817 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3818 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3819 | ` * Parameters` |
|       - | 3820 | ` * $needle` |
|       - | 3821 | ` *   The searched value.` |
|       - | 3822 | ` * $haystack` |
|       - | 3823 | ` *   The array.` |
|       - | 3824 | ` * $strict` |
|       - | 3825 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3826 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3827 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3828 | ` * Return` |
|       - | 3829 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3830 | ` */` |
|      28 | 3831 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3832 |  |
|       - | 3833 | `	ph7_hashmap_node *pEntry;` |
|       - | 3834 | `	ph7_value *pVal,sNeedle;` |
|       - | 3835 | `	ph7_hashmap *pMap;` |
|       - | 3836 | `	ph7_value sVal;` |
|       - | 3837 | `	int bStrict;` |
|       - | 3838 | `	sxu32 n;` |
|       - | 3839 | `	int rc;` |
|      30 | 3840 | `	if( nArg < 2 ){` |
|       - | 3841 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3842 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3843 | `			"ArgumentCountError",` |
|       - | 3844 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 3845 | `			nArg` |
|       - | 3846 | `			);` |
|       - | 3847 | `	}` |
|      26 | 3848 | `	bStrict = FALSE;` |
|      26 | 3849 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3850 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3851 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3852 | `			"TypeError",` |
|       - | 3853 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 3854 | `			ph7_type_name(apArg[1])` |
|       - | 3855 | `			);` |
|       - | 3856 | `	}` |
|      24 | 3857 | `	if( nArg > 2 ){` |
|       - | 3858 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 3859 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3860 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3861 | `				"TypeError",` |
|       - | 3862 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3863 | `				ph7_type_name(apArg[2])` |
|       - | 3864 | `				);` |
|       - | 3865 | `		}` |
|       9 | 3866 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 3867 | `	}` |
|       - | 3868 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 3869 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3870 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 3871 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 3872 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 3873 | `	pEntry = pMap->pFirst;` |
|      21 | 3874 | `	n = pMap->nEntry;` |
|      23 | 3875 | `	for(;;){` |
|      47 | 3876 | `		if( !n ){` |
|       9 | 3877 | `			break;` |
|       - | 3878 | `		}` |
|       - | 3879 | `		/* Extract node value */` |
|      39 | 3880 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 3881 | `		if( pVal ){` |
|       - | 3882 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3883 | `			 * can change their type.` |
|       - | 3884 | `			 */` |
|      39 | 3885 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 3886 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 3887 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 3888 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 3889 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 3890 | `			if( rc == 0 ){` |
|       - | 3891 | `				/* Match found,return key */` |
|      13 | 3892 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3893 | `					/* INT key */` |
|       7 | 3894 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 3895 | `				}else{` |
|       7 | 3896 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3897 | `					/* Blob key */` |
|       7 | 3898 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3899 | `				}` |
|      13 | 3900 | `				return PH7_OK;` |
|       - | 3901 | `			}` |
|      13 | 3902 | `		}` |
|       - | 3903 | `		/* Point to the next entry */` |
|      27 | 3904 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 3905 | `		n--;` |
|       1 | 3906 | `	}` |
|       - | 3907 | `	/* No such value,return FALSE */` |
|       9 | 3908 | `	ph7_result_bool(pCtx,0);` |
|       9 | 3909 | `	return PH7_OK;` |
|      16 | 3910 |  |
|       - | 3911 | `/*` |
|       - | 3912 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3913 | ` *  Computes the difference of arrays.` |
|       - | 3914 | ` * Parameters` |
|       - | 3915 | ` *  $array1` |
|       - | 3916 | ` *    The array to compare from` |
|       - | 3917 | ` *  $array2` |
|       - | 3918 | ` *    An array to compare against` |
|       - | 3919 | ` *  $...` |
|       - | 3920 | ` *   More arrays to compare against` |
|       - | 3921 | ` * Return` |
|       - | 3922 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3923 | ` *  are not present in any of the other arrays.` |
|       - | 3924 | ` */` |
|      22 | 3925 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3926 |  |
|       - | 3927 | `	ph7_hashmap_node *pEntry;` |
|       - | 3928 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3929 | `	ph7_value *pArray;` |
|       - | 3930 | `	ph7_value *pVal;` |
|       - | 3931 | `	sxi32 rc;` |
|       - | 3932 | `	sxu32 n;` |
|       - | 3933 | `	int i;` |
|       - | 3934 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3935 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3936 | `	 * debugging difficult. */` |
|      24 | 3937 | `	if( nArg < 1 ){` |
|       4 | 3938 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3939 | `			"ArgumentCountError",` |
|       - | 3940 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3941 | `			nArg` |
|       - | 3942 | `			);` |
|       - | 3943 | `	}` |
|      22 | 3944 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3945 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3946 | `			"TypeError",` |
|       - | 3947 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3948 | `			ph7_type_name(apArg[0])` |
|       - | 3949 | `			);` |
|       - | 3950 | `	}` |
|      36 | 3951 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3952 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3953 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3954 | `				"TypeError",` |
|       - | 3955 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3956 | `				i + 1,` |
|       2 | 3957 | `				ph7_type_name(apArg[i])` |
|       - | 3958 | `				);` |
|       - | 3959 | `		}` |
|       9 | 3960 | `	}` |
|      17 | 3961 | `	if( nArg == 1 ){` |
|       - | 3962 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3963 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3964 | `		return PH7_OK;` |
|       - | 3965 | `	}` |
|       - | 3966 | `	/* Create a new array */` |
|      15 | 3967 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3968 | `	if( pArray == 0 ){` |
|     ! 0 | 3969 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3970 | `		return PH7_OK;` |
|       - | 3971 | `	}` |
|       - | 3972 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 3973 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3974 | `	/* Perform the diff */` |
|      15 | 3975 | `	pEntry = pSrc->pFirst;` |
|      15 | 3976 | `	n = pSrc->nEntry;` |
|      27 | 3977 | `	for(;;){` |
|      55 | 3978 | `		if( n < 1 ){` |
|      15 | 3979 | `			break;` |
|       - | 3980 | `		}` |
|       - | 3981 | `		/* Extract the node value */` |
|      41 | 3982 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 3983 | `		if( pVal ){` |
|      69 | 3984 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3985 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 3986 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3987 | `				/* Perform the lookup */` |
|      45 | 3988 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 3989 | `				if( rc == SXRET_OK ){` |
|       - | 3990 | `					/* Value exist */` |
|      17 | 3991 | `					break;` |
|       - | 3992 | `				}` |
|      15 | 3993 | `			}` |
|      41 | 3994 | `			if( i >= nArg ){` |
|       - | 3995 | `				/* Perform the insertion */` |
|      25 | 3996 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 3997 | `			}` |
|      20 | 3998 | `		}` |
|       - | 3999 | `		/* Point to the next entry */` |
|      41 | 4000 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4001 | `		n--;` |
|       1 | 4002 | `	}` |
|       - | 4003 | `	/* Return the freshly created array */` |
|      15 | 4004 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4005 | `	return PH7_OK;` |
|      13 | 4006 |  |
|       - | 4007 | `/*` |
|       - | 4008 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4009 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4010 | ` * Parameters` |
|       - | 4011 | ` *  $array1` |
|       - | 4012 | ` *    The array to compare from` |
|       - | 4013 | ` *  $array2` |
|       - | 4014 | ` *    An array to compare against` |
|       - | 4015 | ` *  $...` |
|       - | 4016 | ` *   More arrays to compare against.` |
|       - | 4017 | ` * $callback` |
|       - | 4018 | ` *  The callback comparison function.` |
|       - | 4019 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4020 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4021 | ` *  than the second.` |
|       - | 4022 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4023 | ` * Return` |
|       - | 4024 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4025 | ` *  are not present in any of the other arrays.` |
|       - | 4026 | ` */` |
|       2 | 4027 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4028 |  |
|       - | 4029 | `	ph7_hashmap_node *pEntry;` |
|       - | 4030 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4031 | `	ph7_value *pCallback;` |
|       - | 4032 | `	ph7_value *pArray;` |
|       - | 4033 | `	ph7_value *pVal;` |
|       - | 4034 | `	sxi32 rc;` |
|       - | 4035 | `	sxu32 n;` |
|       - | 4036 | `	int i;` |
|       3 | 4037 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4038 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4039 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4040 | `		return PH7_OK;` |
|       - | 4041 | `	}` |
|       - | 4042 | `	/* Point to the callback */` |
|       3 | 4043 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4044 | `	if( nArg == 2 ){` |
|       - | 4045 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4046 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4047 | `		return PH7_OK;` |
|       - | 4048 | `	}` |
|       - | 4049 | `	/* Create a new array */` |
|       3 | 4050 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4051 | `	if( pArray == 0 ){` |
|     ! 0 | 4052 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4053 | `		return PH7_OK;` |
|       - | 4054 | `	}` |
|       - | 4055 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4056 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4057 | `	/* Perform the diff */` |
|       3 | 4058 | `	pEntry = pSrc->pFirst;` |
|       3 | 4059 | `	n = pSrc->nEntry;` |
|       4 | 4060 | `	for(;;){` |
|       9 | 4061 | `		if( n < 1 ){` |
|       3 | 4062 | `			break;` |
|       - | 4063 | `		}` |
|       - | 4064 | `		/* Extract the node value */` |
|       7 | 4065 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4066 | `		if( pVal ){` |
|      11 | 4067 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4068 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4069 | `					/* ignore */` |
|     ! 0 | 4070 | `					continue;` |
|       - | 4071 | `				}` |
|       - | 4072 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4073 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4074 | `				/* Perform the lookup */` |
|       7 | 4075 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4076 | `				if( rc == SXRET_OK ){` |
|       - | 4077 | `					/* Value exist */` |
|       3 | 4078 | `					break;` |
|       - | 4079 | `				}` |
|       3 | 4080 | `			}` |
|       7 | 4081 | `			if( i >= (nArg - 1)){` |
|       - | 4082 | `				/* Perform the insertion */` |
|       5 | 4083 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4084 | `			}` |
|       3 | 4085 | `		}` |
|       - | 4086 | `		/* Point to the next entry */` |
|       7 | 4087 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4088 | `		n--;` |
|       1 | 4089 | `	}` |
|       - | 4090 | `	/* Return the freshly created array */` |
|       3 | 4091 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4092 | `	return PH7_OK;` |
|       2 | 4093 |  |
|       - | 4094 | `/*` |
|       - | 4095 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4096 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4097 | ` * Parameters` |
|       - | 4098 | ` *  $array1` |
|       - | 4099 | ` *    The array to compare from` |
|       - | 4100 | ` *  $array2` |
|       - | 4101 | ` *    An array to compare against` |
|       - | 4102 | ` *  $...` |
|       - | 4103 | ` *   More arrays to compare against` |
|       - | 4104 | ` * Return` |
|       - | 4105 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4106 | ` *  are not present in any of the other arrays.` |
|       - | 4107 | ` */` |
|      20 | 4108 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4109 |  |
|       - | 4110 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4111 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4112 | `	ph7_value *pArray;` |
|       - | 4113 | `	ph7_value *pVal;` |
|       - | 4114 | `	sxi32 rc;` |
|       - | 4115 | `	sxu32 n;` |
|       - | 4116 | `	int i;` |
|       - | 4117 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4118 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4119 | `	 * accompanying integration tests to pass. */` |
|      22 | 4120 | `	if( nArg < 1 ){` |
|       4 | 4121 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4122 | `			"ArgumentCountError",` |
|       - | 4123 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4124 | `			nArg` |
|       - | 4125 | `			);` |
|       - | 4126 | `	}` |
|      20 | 4127 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4128 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4129 | `			"TypeError",` |
|       - | 4130 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4131 | `			ph7_type_name(apArg[0])` |
|       - | 4132 | `			);` |
|       - | 4133 | `	}` |
|      32 | 4134 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4135 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4136 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4137 | `				"TypeError",` |
|       - | 4138 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4139 | `				i + 1,` |
|       4 | 4140 | `				ph7_type_name(apArg[i])` |
|       - | 4141 | `				);` |
|       - | 4142 | `		}` |
|       9 | 4143 | `	}` |
|      13 | 4144 | `	if( nArg == 1 ){` |
|       - | 4145 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4146 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4147 | `		return PH7_OK;` |
|       - | 4148 | `	}` |
|       - | 4149 | `	/* Create a new array */` |
|      11 | 4150 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4151 | `	if( pArray == 0 ){` |
|     ! 0 | 4152 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4153 | `		return PH7_OK;` |
|       - | 4154 | `	}` |
|       - | 4155 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4156 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4157 | `	/* Perform the diff */` |
|      11 | 4158 | `	pEntry = pSrc->pFirst;` |
|      11 | 4159 | `	n = pSrc->nEntry;` |
|      11 | 4160 | `	pN1 = pN2 = 0;` |
|      29 | 4161 | `	for(;;){` |
|       - | 4162 | `		int keep;` |
|      35 | 4163 | `		if( n < 1 ){` |
|      11 | 4164 | `			break;` |
|       - | 4165 | `		}` |
|       - | 4166 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4167 | `		keep = 1;` |
|      41 | 4168 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4169 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4170 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4171 | `			/* Perform a key lookup first */` |
|      29 | 4172 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4173 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4174 | `			}else{` |
|      17 | 4175 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4176 | `			}` |
|      29 | 4177 | `			if( rc != SXRET_OK ){` |
|       - | 4178 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4179 | `				continue;` |
|       - | 4180 | `			}` |
|       - | 4181 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4182 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4183 | `			if( pVal ){` |
|       - | 4184 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4185 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4186 | `				if( pVal2 ){` |
|      15 | 4187 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4188 | `					if( cmp == 0 ){` |
|       - | 4189 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4190 | `						keep = 0;` |
|      13 | 4191 | `						break;` |
|       - | 4192 | `					}` |
|       1 | 4193 | `				}` |
|       1 | 4194 | `			}` |
|       2 | 4195 | `		}` |
|      25 | 4196 | `		if( keep ){` |
|       - | 4197 | `			/* Perform the insertion */` |
|      13 | 4198 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4199 | `		}` |
|       - | 4200 | `		/* Point to the next entry */` |
|      25 | 4201 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4202 | `		n--;` |
|       1 | 4203 | `	}` |
|       - | 4204 | `	/* Return the freshly created array */` |
|      11 | 4205 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4206 | `	return PH7_OK;` |
|      12 | 4207 |  |
|       - | 4208 | `/*` |
|       - | 4209 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4210 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4211 | ` *  by a user supplied callback function.` |
|       - | 4212 | ` * Parameters` |
|       - | 4213 | ` *  $array1` |
|       - | 4214 | ` *    The array to compare from` |
|       - | 4215 | ` *  $array2` |
|       - | 4216 | ` *    An array to compare against` |
|       - | 4217 | ` *  $...` |
|       - | 4218 | ` *   More arrays to compare against.` |
|       - | 4219 | ` *  $key_compare_func` |
|       - | 4220 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4221 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4222 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4223 | ` * Return` |
|       - | 4224 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4225 | ` *  are not present in any of the other arrays.` |
|       - | 4226 | ` */` |
|      22 | 4227 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4228 |  |
|       - | 4229 | `	ph7_hashmap_node *pEntry;` |
|       - | 4230 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4231 | `	ph7_value *pCallback;` |
|       - | 4232 | `	ph7_value *pArray;` |
|       - | 4233 | `	sxi32 rc;` |
|       - | 4234 | `	sxu32 n;` |
|       - | 4235 | `	int i;` |
|       - | 4236 |  |
|       - | 4237 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4238 | `	if( nArg < 2 ){` |
|       4 | 4239 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4240 | `			"ArgumentCountError",` |
|       - | 4241 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4242 | `			nArg` |
|       - | 4243 | `			);` |
|       - | 4244 | `	}` |
|      22 | 4245 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4246 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4247 | `			"TypeError",` |
|       - | 4248 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4249 | `			ph7_type_name(apArg[0])` |
|       - | 4250 | `			);` |
|       - | 4251 | `	}` |
|       - | 4252 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4253 | `	 * expected to be a callback. */` |
|      32 | 4254 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4255 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4256 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4257 | `				"TypeError",` |
|       - | 4258 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4259 | `				i + 1,` |
|       2 | 4260 | `				ph7_type_name(apArg[i])` |
|       - | 4261 | `				);` |
|       - | 4262 | `		}` |
|       8 | 4263 | `	}` |
|       - | 4264 | `	/* Point to the callback value */` |
|      18 | 4265 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4266 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4267 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4268 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4269 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4270 | `		 * string given" which we also reproduce. */` |
|       7 | 4271 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4272 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4273 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4274 | `				"TypeError",` |
|       - | 4275 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4276 | `				nArg` |
|       - | 4277 | `				);` |
|       - | 4278 | `		}` |
|       5 | 4279 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4280 | `			/* neither array nor string */` |
|       7 | 4281 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4282 | `				"TypeError",` |
|       - | 4283 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4284 | `				nArg` |
|       - | 4285 | `				);` |
|       - | 4286 | `		}` |
|       - | 4287 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4288 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4289 | `			"TypeError",` |
|       - | 4290 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4291 | `			nArg,` |
|     ! 0 | 4292 | `			ph7_type_name(pCallback)` |
|       - | 4293 | `			);` |
|       - | 4294 | `	}` |
|      11 | 4295 | `	if( nArg == 2 ){` |
|       - | 4296 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4297 | `		 * input array. */` |
|       3 | 4298 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4299 | `		return PH7_OK;` |
|       - | 4300 | `	}` |
|       - | 4301 | `	/* Create a new array */` |
|       9 | 4302 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4303 | `	if( pArray == 0 ){` |
|     ! 0 | 4304 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4305 | `		return PH7_OK;` |
|       - | 4306 | `	}` |
|       - | 4307 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4308 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4309 | `	/* Perform the diff */` |
|       9 | 4310 | `	pEntry = pSrc->pFirst;` |
|       9 | 4311 | `	n = pSrc->nEntry;` |
|      20 | 4312 | `	for(;;){` |
|       - | 4313 | `		int keep;` |
|      25 | 4314 | `		if( n < 1 ){` |
|       9 | 4315 | `			break;` |
|       - | 4316 | `		}` |
|      17 | 4317 | `		keep = 1;` |
|      29 | 4318 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4319 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4320 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4321 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4322 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4323 | `			while( pIt ){` |
|       - | 4324 | `				/* build temporary key values for callback */` |
|       - | 4325 | `				ph7_value key1, key2, result;` |
|       - | 4326 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4327 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4328 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4329 | `				}else{` |
|       - | 4330 | `					SyString sStr;` |
|      31 | 4331 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4332 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4333 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4334 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4335 | `				}` |
|      31 | 4336 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4337 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4338 | `				}else{` |
|       - | 4339 | `					SyString sStr;` |
|      31 | 4340 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4341 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4342 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4343 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4344 | `				}` |
|      31 | 4345 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4346 | `				/* call user callback with (key1, key2) */` |
|       - | 4347 | `				{` |
|       - | 4348 | `					ph7_value *apK[2];` |
|      31 | 4349 | `					apK[0] = &key1;` |
|      31 | 4350 | `					apK[1] = &key2;` |
|      31 | 4351 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4352 | `				}` |
|      31 | 4353 | `				if( rc == SXRET_OK ){` |
|      31 | 4354 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4355 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4356 | `					}` |
|      31 | 4357 | `					if( result.x.iVal == 0 ){` |
|       - | 4358 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4359 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4360 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4361 | `						if( pVal1 && pVal2 ){` |
|      13 | 4362 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4363 | `								keep = 0;` |
|       9 | 4364 | `								PH7_MemObjRelease(&result);` |
|       - | 4365 | `								/* release keys too before breaking */` |
|       9 | 4366 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4367 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4368 | `								break;` |
|       - | 4369 | `							}` |
|       2 | 4370 | `						}` |
|       2 | 4371 | `					}` |
|      11 | 4372 | `				}` |
|      23 | 4373 | `				PH7_MemObjRelease(&result);` |
|      23 | 4374 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4375 | `				PH7_MemObjRelease(&key2);` |
|       - | 4376 | `				/* move to next node */` |
|      23 | 4377 | `				pIt = pIt->pPrev;` |
|      23 | 4378 | `				if( keep == 0 ) break;` |
|       1 | 4379 | `			}` |
|      21 | 4380 | `			if( keep == 0 ) break;` |
|       7 | 4381 | `		}` |
|      17 | 4382 | `		if( keep ){` |
|       - | 4383 | `			/* Perform the insertion */` |
|       9 | 4384 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4385 | `		}` |
|       - | 4386 | `		/* Point to the next entry */` |
|      17 | 4387 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4388 | `		n--;` |
|       1 | 4389 | `	}` |
|       - | 4390 | `	/* Return the freshly created array */` |
|       9 | 4391 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4392 | `	return PH7_OK;` |
|      13 | 4393 |  |
|       - | 4394 | `/*` |
|       - | 4395 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4396 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4397 | ` * Parameters` |
|       - | 4398 | ` *  $array1` |
|       - | 4399 | ` *    The array to compare from` |
|       - | 4400 | ` *  $array2` |
|       - | 4401 | ` *    An array to compare against` |
|       - | 4402 | ` *  $...` |
|       - | 4403 | ` *   More arrays to compare against` |
|       - | 4404 | ` * Return` |
|       - | 4405 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4406 | ` *  in any of the other arrays.` |
|       - | 4407 | ` * Note that NULL is returned on failure.` |
|       - | 4408 | ` */` |
|      14 | 4409 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4410 |  |
|       - | 4411 | `	ph7_hashmap_node *pEntry;` |
|       - | 4412 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4413 | `	ph7_value *pArray;` |
|       - | 4414 | `	sxi32 rc;` |
|       - | 4415 | `	sxu32 n;` |
|       - | 4416 | `	int i;` |
|       - | 4417 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4418 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4419 | `	 * helpers. */` |
|      16 | 4420 | `	if( nArg < 1 ){` |
|       4 | 4421 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4422 | `			"ArgumentCountError",` |
|       - | 4423 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4424 | `			nArg` |
|       - | 4425 | `			);` |
|       - | 4426 | `	}` |
|      14 | 4427 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4428 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4429 | `			"TypeError",` |
|       - | 4430 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4431 | `			ph7_type_name(apArg[0])` |
|       - | 4432 | `			);` |
|       - | 4433 | `	}` |
|      20 | 4434 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4435 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4436 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4437 | `				"TypeError",` |
|       - | 4438 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4439 | `				i + 1,` |
|       2 | 4440 | `				ph7_type_name(apArg[i])` |
|       - | 4441 | `				);` |
|       - | 4442 | `		}` |
|       5 | 4443 | `	}` |
|       9 | 4444 | `	if( nArg == 1 ){` |
|       - | 4445 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4446 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4447 | `		return PH7_OK;` |
|       - | 4448 | `	}` |
|       - | 4449 | `	/* Create a new array */` |
|       7 | 4450 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4451 | `	if( pArray == 0 ){` |
|     ! 0 | 4452 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4453 | `		return PH7_OK;` |
|       - | 4454 | `	}` |
|       - | 4455 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4456 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4457 | `	/* Perfrom the diff */` |
|       7 | 4458 | `	pEntry = pSrc->pFirst;` |
|       7 | 4459 | `	n = pSrc->nEntry;` |
|      12 | 4460 | `	for(;;){` |
|      25 | 4461 | `		if( n < 1 ){` |
|       7 | 4462 | `			break;` |
|       - | 4463 | `		}` |
|      31 | 4464 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4465 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4466 | `				/* ignore */` |
|     ! 0 | 4467 | `				continue;` |
|       - | 4468 | `			}` |
|      23 | 4469 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4470 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4471 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4472 | `				/* Blob lookup */` |
|      17 | 4473 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4474 | `			}else{` |
|       - | 4475 | `				/* Int lookup */` |
|       7 | 4476 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4477 | `			}` |
|      23 | 4478 | `			if( rc == SXRET_OK ){` |
|       - | 4479 | `				/* Key exists,break immediately */` |
|      11 | 4480 | `				break;` |
|       - | 4481 | `			}` |
|       7 | 4482 | `		}` |
|      19 | 4483 | `		if( i >= nArg ){` |
|       - | 4484 | `			/* Perform the insertion */` |
|       9 | 4485 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4486 | `		}` |
|       - | 4487 | `		/* Point to the next entry */` |
|      19 | 4488 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4489 | `		n--;` |
|       1 | 4490 | `	}` |
|       - | 4491 | `	/* Return the freshly created array */` |
|       7 | 4492 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4493 | `	return PH7_OK;` |
|       9 | 4494 |  |
|       - | 4495 | `/*` |
|       - | 4496 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4497 | ` *  Computes the intersection of arrays.` |
|       - | 4498 | ` * Parameters` |
|       - | 4499 | ` *  $array1` |
|       - | 4500 | ` *    The array to compare from` |
|       - | 4501 | ` *  $array2` |
|       - | 4502 | ` *    An array to compare against` |
|       - | 4503 | ` *  $...` |
|       - | 4504 | ` *   More arrays to compare against` |
|       - | 4505 | ` * Return` |
|       - | 4506 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4507 | ` *  in all of the parameters.` |
|       - | 4508 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4509 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4510 | ` */` |
|      22 | 4511 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4512 |  |
|       - | 4513 | `	ph7_hashmap_node *pEntry;` |
|       - | 4514 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4515 | `	ph7_value *pArray;` |
|       - | 4516 | `	ph7_value *pVal;` |
|       - | 4517 | `	sxi32 rc;` |
|       - | 4518 | `	sxu32 n;` |
|       - | 4519 | `	int i;` |
|      24 | 4520 | `	if( nArg < 1 ){` |
|       4 | 4521 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4522 | `			"ArgumentCountError",` |
|       - | 4523 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4524 | `			nArg` |
|       - | 4525 | `			);` |
|       - | 4526 | `	}` |
|      22 | 4527 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4528 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4529 | `			"TypeError",` |
|       - | 4530 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4531 | `			ph7_type_name(apArg[0])` |
|       - | 4532 | `			);` |
|       - | 4533 | `	}` |
|      36 | 4534 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4535 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4536 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4537 | `				"TypeError",` |
|       - | 4538 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4539 | `				i + 1,` |
|       2 | 4540 | `				ph7_type_name(apArg[i])` |
|       - | 4541 | `				);` |
|       - | 4542 | `		}` |
|       9 | 4543 | `	}` |
|      17 | 4544 | `	if( nArg == 1 ){` |
|       - | 4545 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4546 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4547 | `		return PH7_OK;` |
|       - | 4548 | `	}` |
|       - | 4549 | `	/* Create a new array */` |
|      15 | 4550 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4551 | `	if( pArray == 0 ){` |
|     ! 0 | 4552 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4553 | `		return PH7_OK;` |
|       - | 4554 | `	}` |
|       - | 4555 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4556 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4557 | `	/* Perform the intersection */` |
|      15 | 4558 | `	pEntry = pSrc->pFirst;` |
|      15 | 4559 | `	n = pSrc->nEntry;` |
|      31 | 4560 | `	for(;;){` |
|      63 | 4561 | `		if( n < 1 ){` |
|      15 | 4562 | `			break;` |
|       - | 4563 | `		}` |
|       - | 4564 | `		/* Extract the node value */` |
|      49 | 4565 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4566 | `		if( pVal ){` |
|      79 | 4567 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4568 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4569 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4570 | `				/* Perform the lookup */` |
|      55 | 4571 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4572 | `				if( rc != SXRET_OK ){` |
|       - | 4573 | `					/* Value does not exist */` |
|      25 | 4574 | `					break;` |
|       - | 4575 | `				}` |
|      16 | 4576 | `			}` |
|      49 | 4577 | `			if( i >= nArg ){` |
|       - | 4578 | `				/* Perform the insertion */` |
|      25 | 4579 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4580 | `			}` |
|      24 | 4581 | `		}` |
|       - | 4582 | `		/* Point to the next entry */` |
|      49 | 4583 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4584 | `		n--;` |
|       1 | 4585 | `	}` |
|       - | 4586 | `	/* Return the freshly created array */` |
|      15 | 4587 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4588 | `	return PH7_OK;` |
|      13 | 4589 |  |
|       - | 4590 | `/*` |
|       - | 4591 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4592 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4593 | ` * Parameters` |
|       - | 4594 | ` *  $array1` |
|       - | 4595 | ` *    The array to compare from` |
|       - | 4596 | ` *  $array2` |
|       - | 4597 | ` *    An array to compare against` |
|       - | 4598 | ` *  $...` |
|       - | 4599 | ` *   More arrays to compare against` |
|       - | 4600 | ` * Return` |
|       - | 4601 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4602 | ` *  in all the arguments, with matching keys.` |
|       - | 4603 | ` */` |
|      22 | 4604 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4605 |  |
|       - | 4606 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4607 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4608 | `	ph7_value *pArray;` |
|       - | 4609 | `	ph7_value *pVal;` |
|       - | 4610 | `	sxi32 rc;` |
|       - | 4611 | `	sxu32 n;` |
|       - | 4612 | `	int i;` |
|      24 | 4613 | `	if( nArg < 1 ){` |
|       4 | 4614 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4615 | `			"ArgumentCountError",` |
|       - | 4616 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4617 | `			nArg` |
|       - | 4618 | `			);` |
|       - | 4619 | `	}` |
|      22 | 4620 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4621 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4622 | `			"TypeError",` |
|       - | 4623 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4624 | `			ph7_type_name(apArg[0])` |
|       - | 4625 | `			);` |
|       - | 4626 | `	}` |
|      36 | 4627 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4628 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4629 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4630 | `				"TypeError",` |
|       - | 4631 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4632 | `				i + 1,` |
|       2 | 4633 | `				ph7_type_name(apArg[i])` |
|       - | 4634 | `				);` |
|       - | 4635 | `		}` |
|       9 | 4636 | `	}` |
|      17 | 4637 | `	if( nArg == 1 ){` |
|       - | 4638 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4639 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4640 | `		return PH7_OK;` |
|       - | 4641 | `	}` |
|       - | 4642 | `	/* Create a new array */` |
|      15 | 4643 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4644 | `	if( pArray == 0 ){` |
|     ! 0 | 4645 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4646 | `		return PH7_OK;` |
|       - | 4647 | `	}` |
|       - | 4648 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4649 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4650 | `	/* Perform the intersection */` |
|      15 | 4651 | `	pEntry = pSrc->pFirst;` |
|      15 | 4652 | `	n = pSrc->nEntry;` |
|      15 | 4653 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4654 | `	for(;;){` |
|      47 | 4655 | `		if( n < 1 ){` |
|      15 | 4656 | `			break;` |
|       - | 4657 | `		}` |
|       - | 4658 | `		/* Extract the node value */` |
|      33 | 4659 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4660 | `		if( pVal ){` |
|      53 | 4661 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4662 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4663 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4664 | `				/* Perform a key lookup first */` |
|      37 | 4665 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4666 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4667 | `				}else{` |
|      23 | 4668 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4669 | `				}` |
|      37 | 4670 | `				if( rc != SXRET_OK ){` |
|       - | 4671 | `					/* No such key,break immediately */` |
|       7 | 4672 | `					break;` |
|       - | 4673 | `				}` |
|       - | 4674 | `				/* Perform the lookup */` |
|      31 | 4675 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4676 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4677 | `					/* Value does not exist */` |
|       6 | 4678 | `					break;` |
|       - | 4679 | `				}` |
|      11 | 4680 | `			}` |
|      33 | 4681 | `			if( i >= nArg ){` |
|       - | 4682 | `				/* Perform the insertion */` |
|      17 | 4683 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4684 | `			}` |
|      16 | 4685 | `		}` |
|       - | 4686 | `		/* Point to the next entry */` |
|      33 | 4687 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4688 | `		n--;` |
|       1 | 4689 | `	}` |
|       - | 4690 | `	/* Return the freshly created array */` |
|      15 | 4691 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4692 | `	return PH7_OK;` |
|      13 | 4693 |  |
|       - | 4694 | `/*` |
|       - | 4695 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 4696 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4697 | ` * Parameters` |
|       - | 4698 | ` *  $array1` |
|       - | 4699 | ` *    The array to compare from` |
|       - | 4700 | ` *  $...` |
|       - | 4701 | ` *   More arrays to compare against` |
|       - | 4702 | ` * Return` |
|       - | 4703 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4704 | ` *  have keys that are present in all arguments.` |
|       - | 4705 | ` * Note that NULL is returned on failure.` |
|       - | 4706 | ` */` |
|      22 | 4707 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4708 |  |
|       - | 4709 | `	ph7_hashmap_node *pEntry;` |
|       - | 4710 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4711 | `	ph7_value *pArray;` |
|       - | 4712 | `	sxi32 rc;` |
|       - | 4713 | `	sxu32 n;` |
|       - | 4714 | `	int i;` |
|      24 | 4715 | `	if( nArg < 1 ){` |
|       4 | 4716 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4717 | `			"ArgumentCountError",` |
|       - | 4718 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 4719 | `			nArg` |
|       - | 4720 | `			);` |
|       - | 4721 | `	}` |
|      22 | 4722 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4723 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4724 | `			"TypeError",` |
|       - | 4725 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4726 | `			ph7_type_name(apArg[0])` |
|       - | 4727 | `			);` |
|       - | 4728 | `	}` |
|      36 | 4729 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4730 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4731 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4732 | `				"TypeError",` |
|       - | 4733 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4734 | `				i + 1,` |
|       2 | 4735 | `				ph7_type_name(apArg[i])` |
|       - | 4736 | `				);` |
|       - | 4737 | `		}` |
|       9 | 4738 | `	}` |
|      17 | 4739 | `	if( nArg == 1 ){` |
|       - | 4740 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4741 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4742 | `		return PH7_OK;` |
|       - | 4743 | `	}` |
|       - | 4744 | `	/* Create a new array */` |
|      15 | 4745 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4746 | `	if( pArray == 0 ){` |
|     ! 0 | 4747 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4748 | `		return PH7_OK;` |
|       - | 4749 | `	}` |
|       - | 4750 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 4751 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4752 | `	/* Perform the intersection */` |
|      15 | 4753 | `	pEntry = pSrc->pFirst;` |
|      15 | 4754 | `	n = pSrc->nEntry;` |
|      24 | 4755 | `	for(;;){` |
|      49 | 4756 | `		if( n < 1 ){` |
|      15 | 4757 | `			break;` |
|       - | 4758 | `		}` |
|      57 | 4759 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 4760 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 4761 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 4762 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4763 | `				/* Blob lookup */` |
|      27 | 4764 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 4765 | `			}else{` |
|       - | 4766 | `				/* Int key */` |
|      13 | 4767 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4768 | `			}` |
|      39 | 4769 | `			if( rc != SXRET_OK ){` |
|       - | 4770 | `				/* Key does not exist, break immediately */` |
|      17 | 4771 | `				break;` |
|       - | 4772 | `			}` |
|      12 | 4773 | `		}` |
|      35 | 4774 | `		if( i >= nArg ){` |
|       - | 4775 | `			/* Perform the insertion */` |
|      19 | 4776 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 4777 | `		}` |
|       - | 4778 | `		/* Point to the next entry */` |
|      35 | 4779 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 4780 | `		n--;` |
|       1 | 4781 | `	}` |
|       - | 4782 | `	/* Return the freshly created array */` |
|      15 | 4783 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4784 | `	return PH7_OK;` |
|      13 | 4785 |  |
|       - | 4786 | `/*` |
|       - | 4787 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4788 | ` *  Computes the intersection of arrays.` |
|       - | 4789 | ` * Parameters` |
|       - | 4790 | ` *  $array1` |
|       - | 4791 | ` *    The array to compare from` |
|       - | 4792 | ` *  $array2` |
|       - | 4793 | ` *    An array to compare against` |
|       - | 4794 | ` *  $...` |
|       - | 4795 | ` *   More arrays to compare against` |
|       - | 4796 | ` * $callback` |
|       - | 4797 | ` *  The callback comparison function.` |
|       - | 4798 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4799 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4800 | ` *  than the second.` |
|       - | 4801 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4802 | ` * Return` |
|       - | 4803 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4804 | ` *  in all of the parameters. .` |
|       - | 4805 | ` * Note that NULL is returned on failure.` |
|       - | 4806 | ` */` |
|       2 | 4807 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4808 |  |
|       - | 4809 | `	ph7_hashmap_node *pEntry;` |
|       - | 4810 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4811 | `	ph7_value *pCallback;` |
|       - | 4812 | `	ph7_value *pArray;` |
|       - | 4813 | `	ph7_value *pVal;` |
|       - | 4814 | `	sxi32 rc;` |
|       - | 4815 | `	sxu32 n;` |
|       - | 4816 | `	int i;` |
|       - | 4817 |  |
|       3 | 4818 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4819 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4820 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4821 | `		return PH7_OK;` |
|       - | 4822 | `	}` |
|       - | 4823 | `	/* Point to the callback */` |
|       3 | 4824 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4825 | `	if( nArg == 2 ){` |
|       - | 4826 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4827 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4828 | `		return PH7_OK;` |
|       - | 4829 | `	}` |
|       - | 4830 | `	/* Create a new array */` |
|       3 | 4831 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4832 | `	if( pArray == 0 ){` |
|     ! 0 | 4833 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4834 | `		return PH7_OK;` |
|       - | 4835 | `	}` |
|       - | 4836 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4837 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4838 | `	/* Perform the intersection */` |
|       3 | 4839 | `	pEntry = pSrc->pFirst;` |
|       3 | 4840 | `	n = pSrc->nEntry;` |
|       4 | 4841 | `	for(;;){` |
|       9 | 4842 | `		if( n < 1 ){` |
|       3 | 4843 | `			break;` |
|       - | 4844 | `		}` |
|       - | 4845 | `		/* Extract the node value */` |
|       7 | 4846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4847 | `		if( pVal ){` |
|      11 | 4848 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4849 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4850 | `					/* ignore */` |
|     ! 0 | 4851 | `					continue;` |
|       - | 4852 | `				}` |
|       - | 4853 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4854 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4855 | `				/* Perform the lookup */` |
|       7 | 4856 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4857 | `				if( rc != SXRET_OK ){` |
|       - | 4858 | `					/* Value does not exist */` |
|       3 | 4859 | `					break;` |
|       - | 4860 | `				}` |
|       3 | 4861 | `			}` |
|       7 | 4862 | `			if( i >= (nArg-1) ){` |
|       - | 4863 | `				/* Perform the insertion */` |
|       5 | 4864 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4865 | `			}` |
|       3 | 4866 | `		}` |
|       - | 4867 | `		/* Point to the next entry */` |
|       7 | 4868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4869 | `		n--;` |
|       1 | 4870 | `	}` |
|       - | 4871 | `	/* Return the freshly created array */` |
|       3 | 4872 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4873 | `	return PH7_OK;` |
|       2 | 4874 |  |
|       - | 4875 | `/*` |
|       - | 4876 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 4877 | ` *  Fill an array with values.` |
|       - | 4878 | ` * Parameters` |
|       - | 4879 | ` *  $start_index` |
|       - | 4880 | ` *    The first index of the returned array.` |
|       - | 4881 | ` *  $num` |
|       - | 4882 | ` *   Number of elements to insert.` |
|       - | 4883 | ` *  $value` |
|       - | 4884 | ` *    Value to use for filling.` |
|       - | 4885 | ` * Return` |
|       - | 4886 | ` *  The filled array or null on failure.` |
|       - | 4887 | ` */` |
|     238 | 4888 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4889 |  |
|       - | 4890 | `	ph7_value *pArray;` |
|       - | 4891 | `	int i,nEntry;` |
|       - | 4892 |  |
|       - | 4893 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 4894 | `	if( nArg != 3 ){` |
|       - | 4895 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 4896 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4897 | `			"ArgumentCountError",` |
|       - | 4898 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 4899 | `			nArg` |
|       - | 4900 | `			);` |
|       - | 4901 | `	}` |
|       - | 4902 |  |
|       - | 4903 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 4904 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 4905 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 4906 | `	 * and NULLs are rejected outright. */` |
|     466 | 4907 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 4908 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 4909 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4910 | `			"TypeError",` |
|       - | 4911 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 4912 | `			ph7_type_name(apArg[0])` |
|       - | 4913 | `			);` |
|       - | 4914 | `	}` |
|     234 | 4915 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 4916 | `		int len;` |
|       8 | 4917 | `		sxu8 bReal = FALSE;` |
|       8 | 4918 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 4919 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 4920 | `			/* Non‑numeric string is an error. */` |
|       3 | 4921 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4922 | `				"TypeError",` |
|       - | 4923 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 4924 | `				);` |
|       - | 4925 | `		}` |
|       5 | 4926 | `		if( bReal ){` |
|       - | 4927 | `			/* float-string -> deprecation warning */` |
|       4 | 4928 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4929 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 4930 | `				zStr` |
|       - | 4931 | `				);` |
|       1 | 4932 | `		}` |
|       2 | 4933 | `	}` |
|       - | 4934 |  |
|       - | 4935 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 4936 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 4937 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 4938 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4939 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4940 | `			"TypeError",` |
|       - | 4941 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 4942 | `			ph7_type_name(apArg[1])` |
|       - | 4943 | `			);` |
|       - | 4944 | `	}` |
|     232 | 4945 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4946 | `		int len;` |
|       3 | 4947 | `		sxu8 bReal = FALSE;` |
|       3 | 4948 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4949 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 4950 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4951 | `				"TypeError",` |
|       - | 4952 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 4953 | `				);` |
|       - | 4954 | `		}` |
|     ! 0 | 4955 | `	}` |
|       - | 4956 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 4957 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 4958 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 4959 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 4960 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 4961 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 4962 | `		if( d != (double)i64 ){` |
|       7 | 4963 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4964 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 4965 | `				d` |
|       - | 4966 | `				);` |
|       2 | 4967 | `		}` |
|       2 | 4968 | `	}` |
|       - | 4969 |  |
|       - | 4970 | `	/* Total number of entries to insert */` |
|     230 | 4971 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 4972 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 4973 | `	if( nEntry < 0 ){` |
|       3 | 4974 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4975 | `			"ValueError",` |
|       - | 4976 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 4977 | `			);` |
|       - | 4978 | `	}` |
|       - | 4979 |  |
|       - | 4980 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 4981 | `	if( nEntry == 0 ){` |
|       7 | 4982 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 4983 | `		return PH7_OK;` |
|       - | 4984 | `	}` |
|       - | 4985 |  |
|       - | 4986 | `	/* Create a new array */` |
|     221 | 4987 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 4988 | `	if( pArray == 0 ){` |
|     ! 0 | 4989 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4990 | `		return PH7_OK;` |
|       - | 4991 | `	}` |
|       - | 4992 |  |
|       - | 4993 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 4994 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 4995 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 4996 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 4997 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 4998 | `	}` |
|       - | 4999 | `	/* Return the filled array */` |
|     221 | 5000 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5001 | `	return PH7_OK;` |
|     121 | 5002 |  |
|       - | 5003 | `/*` |
|       - | 5004 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5005 | ` *  Fill an array with values, specifying keys.` |
|       - | 5006 | ` * Parameters` |
|       - | 5007 | ` *  $input` |
|       - | 5008 | ` *   Array of values that will be used as key.` |
|       - | 5009 | ` *  $value` |
|       - | 5010 | ` *    Value to use for filling.` |
|       - | 5011 | ` * Return` |
|       - | 5012 | ` *  The filled array.` |
|       - | 5013 | ` * Throws` |
|       - | 5014 | ` *  ValueError if $input is not an array.` |
|       - | 5015 | ` */` |
|      26 | 5016 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5017 |  |
|       - | 5018 | `	ph7_hashmap_node *pEntry;` |
|       - | 5019 | `	ph7_hashmap *pSrc;` |
|       - | 5020 | `	ph7_value *pArray;` |
|       - | 5021 | `	sxu32 n;` |
|       - | 5022 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5023 | `	if( nArg != 2 ){` |
|      10 | 5024 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5025 | `			"ArgumentCountError",` |
|       - | 5026 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5027 | `			nArg` |
|       - | 5028 | `			);` |
|       - | 5029 | `	}` |
|       - | 5030 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5031 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5032 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5033 | `			"TypeError",` |
|       - | 5034 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5035 | `			ph7_type_name(apArg[0])` |
|       - | 5036 | `			);` |
|       - | 5037 | `	}` |
|       - | 5038 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5039 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5040 | `	/* Create a new array */` |
|      17 | 5041 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5042 | `	if( pArray == 0 ){` |
|     ! 0 | 5043 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5044 | `		return PH7_OK;` |
|       - | 5045 | `	}` |
|       - | 5046 | `	/* Perform the requested operation */` |
|      17 | 5047 | `	pEntry = pSrc->pFirst;` |
|      45 | 5048 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5049 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5050 | `		/* Point to the next entry */` |
|      29 | 5051 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5052 | `	}` |
|       - | 5053 | `	/* Return the filled array */` |
|      17 | 5054 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5055 | `	return PH7_OK;` |
|      15 | 5056 |  |
|       - | 5057 | `/*` |
|       - | 5058 | ` * array array_combine(array $keys,array $values)` |
|       - | 5059 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5060 | ` * Parameters` |
|       - | 5061 | ` *  $keys` |
|       - | 5062 | ` *    Array of keys to be used.` |
|       - | 5063 | ` * $values` |
|       - | 5064 | ` *   Array of values to be used.` |
|       - | 5065 | ` * Return` |
|       - | 5066 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5067 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5068 | ` *  not an array.` |
|       - | 5069 | ` */` |
|      18 | 5070 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5071 |  |
|       - | 5072 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5073 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5074 | `	ph7_value *pArray;` |
|       - | 5075 | `	sxu32 n;` |
|       - | 5076 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5077 | `	if( nArg != 2 ){` |
|       - | 5078 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5079 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5080 | `			"ArgumentCountError",` |
|       - | 5081 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5082 | `			nArg` |
|       - | 5083 | `			);` |
|       - | 5084 | `	}` |
|       - | 5085 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5086 | `	 * argument index in the error message. */` |
|      18 | 5087 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5088 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5089 | `			"TypeError",` |
|       - | 5090 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5091 | `			ph7_type_name(apArg[0])` |
|       - | 5092 | `			);` |
|       - | 5093 | `	}` |
|      16 | 5094 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5095 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5096 | `			"TypeError",` |
|       - | 5097 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5098 | `			ph7_type_name(apArg[1])` |
|       - | 5099 | `			);` |
|       - | 5100 | `	}` |
|       - | 5101 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5102 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5103 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5104 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5105 | `		/* Length mismatch -> ValueError */` |
|       3 | 5106 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5107 | `			"ValueError",` |
|       - | 5108 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5109 | `			);` |
|       - | 5110 | `	}` |
|       - | 5111 | `	/* Create a new array */` |
|      11 | 5112 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5113 | `	if( pArray == 0 ){` |
|     ! 0 | 5114 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5115 | `		return PH7_OK;` |
|       - | 5116 | `	}` |
|       - | 5117 | `	/* Perform the requested operation */` |
|      11 | 5118 | `	pKe = pKey->pFirst;` |
|      11 | 5119 | `	pVe = pValue->pFirst;` |
|      33 | 5120 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5121 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5122 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5123 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5124 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5125 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5126 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5127 | `		 * original array must not be mutated. */` |
|      23 | 5128 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5129 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5130 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5131 | `			if( pTmpKey ){` |
|       5 | 5132 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5133 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5134 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5135 | `				pKeyCopy = pTmpKey;` |
|       2 | 5136 | `			}` |
|       2 | 5137 | `		}` |
|      23 | 5138 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5139 | `		/* Point to the next entry */` |
|      23 | 5140 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5141 | `		pVe = pVe->pPrev;` |
|      12 | 5142 | `	}` |
|       - | 5143 | `	/* Return the filled array */` |
|      11 | 5144 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5145 | `	return PH7_OK;` |
|      11 | 5146 |  |
|       - | 5147 | `/*` |
|       - | 5148 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5149 | ` *  Return an array with elements in reverse order.` |
|       - | 5150 | ` * Parameters` |
|       - | 5151 | ` *  $array` |
|       - | 5152 | ` *   The input array.` |
|       - | 5153 | ` *  $preserve_keys (optional)` |
|       - | 5154 | ` *   If set to TRUE keys are preserved.` |
|       - | 5155 | ` * Return` |
|       - | 5156 | ` *  The reversed array.` |
|       - | 5157 | ` */` |
|      20 | 5158 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5159 |  |
|       - | 5160 | `	ph7_hashmap_node *pEntry;` |
|       - | 5161 | `	ph7_hashmap *pSrc;` |
|       - | 5162 | `	ph7_value *pArray;` |
|       - | 5163 | `	int bPreserve;` |
|       - | 5164 | `	sxu32 n;` |
|      22 | 5165 | `	if( nArg < 1 ){` |
|       4 | 5166 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5167 | `			"ArgumentCountError",` |
|       - | 5168 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5169 | `			nArg` |
|       - | 5170 | `			);` |
|       - | 5171 | `	}` |
|       - | 5172 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5173 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5174 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5175 | `			"TypeError",` |
|       - | 5176 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5177 | `			ph7_type_name(apArg[0])` |
|       - | 5178 | `			);` |
|       - | 5179 | `	}` |
|      17 | 5180 | `	bPreserve = FALSE;` |
|      17 | 5181 | `	if( nArg > 1 ){` |
|       7 | 5182 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5183 | `	}` |
|       - | 5184 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5185 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5186 | `	/* Create a new array */` |
|      17 | 5187 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5188 | `	if( pArray == 0 ){` |
|     ! 0 | 5189 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5190 | `		return PH7_OK;` |
|       - | 5191 | `	}` |
|       - | 5192 | `	/* Perform the requested operation */` |
|      17 | 5193 | `	pEntry = pSrc->pLast;` |
|      55 | 5194 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5195 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5196 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5197 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5198 | `		/* Point to the previous entry */` |
|      39 | 5199 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5200 | `	}` |
|      17 | 5201 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5202 | `	return PH7_OK;` |
|      12 | 5203 |  |
|       - | 5204 | `/*` |
|       - | 5205 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5206 | ` *  Removes duplicate values from an array.` |
|       - | 5207 | ` * Parameters` |
|       - | 5208 | ` *  $array` |
|       - | 5209 | ` *   The input array.` |
|       - | 5210 | ` *  $flags` |
|       - | 5211 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5212 | ` *   behavior using these values:` |
|       - | 5213 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5214 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5215 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5216 | ` * Return` |
|       - | 5217 | ` *  The filtered array.` |
|       - | 5218 | ` */` |
|      24 | 5219 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5220 |  |
|       - | 5221 | `	ph7_hashmap_node *pEntry;` |
|       - | 5222 | `	ph7_value *pNeedle;` |
|       - | 5223 | `	ph7_hashmap *pSrc;` |
|       - | 5224 | `	ph7_value *pArray;` |
|       - | 5225 | `	int bStrict;` |
|       - | 5226 | `	sxi32 rc;` |
|       - | 5227 | `	sxu32 n;` |
|      26 | 5228 | `	if( nArg < 1 ){` |
|       - | 5229 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5230 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5231 | `			"ArgumentCountError",` |
|       - | 5232 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5233 | `			);` |
|       - | 5234 | `	}` |
|      24 | 5235 | `	if( nArg > 2 ){` |
|       - | 5236 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5237 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5238 | `			"ArgumentCountError",` |
|       - | 5239 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5240 | `			nArg` |
|       - | 5241 | `			);` |
|       - | 5242 | `	}` |
|       - | 5243 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5244 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5245 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5246 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5247 | `			"TypeError",` |
|       - | 5248 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5249 | `			ph7_type_name(apArg[0])` |
|       - | 5250 | `			);` |
|       - | 5251 | `	}` |
|      19 | 5252 | `	bStrict = FALSE;` |
|       - | 5253 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5254 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5255 | `	/* Create a new array */` |
|      19 | 5256 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5257 | `	if( pArray == 0 ){` |
|     ! 0 | 5258 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5259 | `		return PH7_OK;` |
|       - | 5260 | `	}` |
|       - | 5261 | `	/* Perform the requested operation */` |
|      19 | 5262 | `	pEntry = pSrc->pFirst;` |
|      83 | 5263 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5264 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5265 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5266 | `		if( pNeedle ){` |
|      65 | 5267 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5268 | `		}` |
|      65 | 5269 | `		if( rc != SXRET_OK ){` |
|       - | 5270 | `			/* Perform the insertion */` |
|      37 | 5271 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5272 | `		}` |
|       - | 5273 | `		/* Point to the next entry */` |
|      65 | 5274 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5275 | `	}` |
|       - | 5276 | `	/* Return the freshly created array */` |
|      19 | 5277 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5278 | `	return PH7_OK;` |
|      14 | 5279 |  |
|       - | 5280 | `/*` |
|       - | 5281 | ` * array array_flip(array $input)` |
|       - | 5282 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5283 | ` * Parameter` |
|       - | 5284 | ` *  $input` |
|       - | 5285 | ` *   Input array.` |
|       - | 5286 | ` * Return` |
|       - | 5287 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5288 | ` */` |
|      34 | 5289 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5290 |  |
|       - | 5291 | `	ph7_hashmap_node *pEntry;` |
|       - | 5292 | `	ph7_hashmap *pSrc;` |
|       - | 5293 | `	ph7_value *pArray;` |
|       - | 5294 | `	ph7_value *pKey;` |
|       - | 5295 | `	ph7_value sVal;` |
|       - | 5296 | `	sxu32 n;` |
|       - | 5297 |  |
|       - | 5298 | `	/* PHP requires exactly one argument */` |
|      36 | 5299 | `	if( nArg != 1 ){` |
|       - | 5300 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5301 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5302 | `			"ArgumentCountError",` |
|       - | 5303 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5304 | `			nArg` |
|       - | 5305 | `			);` |
|       - | 5306 | `	}` |
|       - | 5307 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5308 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5309 | `		/* Type mismatch -> TypeError */` |
|       7 | 5310 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5311 | `			"TypeError",` |
|       - | 5312 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5313 | `			ph7_type_name(apArg[0])` |
|       - | 5314 | `			);` |
|       - | 5315 | `	}` |
|       - | 5316 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5317 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5318 | `	/* Create a new array */` |
|      27 | 5319 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5320 | `	if( pArray == 0 ){` |
|     ! 0 | 5321 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5322 | `		return PH7_OK;` |
|       - | 5323 | `	}` |
|       - | 5324 | `	/* Start processing */` |
|      27 | 5325 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5326 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5327 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5328 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5329 | `		if( pKey ){` |
|       - | 5330 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5331 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5332 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5333 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5334 | `					);` |
|   22236 | 5335 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5336 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5337 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5338 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5339 | `				}else{` |
|       - | 5340 | `					SyString sStr;` |
|    2227 | 5341 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5342 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5343 | `				}` |
|       - | 5344 | `				/* Perform the insertion */` |
|   22227 | 5345 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5346 | `				/* Safely release the value because each inserted entry` |
|       - | 5347 | `				 * has its own private copy of the value.` |
|       - | 5348 | `				 */` |
|   22227 | 5349 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5350 | `			}else{` |
|       - | 5351 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5352 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5353 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5354 | `					);` |
|       - | 5355 | `			}` |
|   11118 | 5356 | `		}` |
|       - | 5357 | `		/* Point to the next entry */` |
|   22237 | 5358 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5359 | `	}` |
|       - | 5360 | `	/* Return the freshly created array */` |
|      27 | 5361 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5362 | `	return PH7_OK;` |
|      19 | 5363 |  |
|       - | 5364 | `/*` |
|       - | 5365 | ` * number array_sum(array $array )` |
|       - | 5366 | ` *  Calculate the sum of values in an array.` |
|       - | 5367 | ` * Parameters` |
|       - | 5368 | ` *  $array: The input array.` |
|       - | 5369 | ` * Return` |
|       - | 5370 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5371 | ` */` |
|      24 | 5372 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5373 |  |
|       - | 5374 | `	ph7_hashmap_node *pEntry;` |
|       - | 5375 | `	ph7_value *pObj;` |
|      25 | 5376 | `	double dSum = 0;` |
|       - | 5377 | `	sxu32 n;` |
|      25 | 5378 | `	pEntry = pMap->pFirst;` |
|      91 | 5379 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5380 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5381 | `		if( pObj ){` |
|      67 | 5382 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5383 | `				dSum += pObj->rVal;` |
|      53 | 5384 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5385 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5386 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5387 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5388 | `					double dv = 0;` |
|      13 | 5389 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5390 | `					dSum += dv;` |
|       7 | 5391 | `				}` |
|      12 | 5392 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5393 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5394 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5395 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5396 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5397 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5398 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5399 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5400 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5401 | `			}` |
|       - | 5402 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5403 | `		}` |
|       - | 5404 | `		/* Point to the next entry */` |
|      67 | 5405 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5406 | `	}` |
|       - | 5407 | `	/* Return sum */` |
|      25 | 5408 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5409 |  |
|      18 | 5410 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5411 |  |
|       - | 5412 | `	ph7_hashmap_node *pEntry;` |
|       - | 5413 | `	ph7_value *pObj;` |
|      20 | 5414 | `	sxi64 nSum = 0;` |
|       - | 5415 | `	sxu32 n;` |
|      20 | 5416 | `	pEntry = pMap->pFirst;` |
|      80 | 5417 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5418 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5419 | `		if( pObj ){` |
|      62 | 5420 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5421 | `				nSum += pObj->x.iVal;` |
|      36 | 5422 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5423 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5424 | `					sxi64 nv = 0;` |
|       5 | 5425 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5426 | `					nSum += nv;` |
|       3 | 5427 | `				}` |
|       8 | 5428 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5429 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5430 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5431 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5432 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5433 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5434 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5435 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5436 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5437 | `			}` |
|       - | 5438 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5439 | `		}` |
|       - | 5440 | `		/* Point to the next entry */` |
|      62 | 5441 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5442 | `	}` |
|       - | 5443 | `	/* Return sum */` |
|      20 | 5444 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5445 |  |
|       - | 5446 | `/* number array_sum(array $array )` |
|       - | 5447 | ` * (See block-coment above)` |
|       - | 5448 | ` */` |
|      52 | 5449 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5450 |  |
|       - | 5451 | `	ph7_hashmap_node *pEntry;` |
|       - | 5452 | `	ph7_hashmap *pMap;` |
|       - | 5453 | `	ph7_value *pObj;` |
|      54 | 5454 | `	int useDouble = 0;` |
|       - | 5455 | `	sxu32 n;` |
|       - | 5456 | `	/* PHP requires exactly one argument */` |
|      54 | 5457 | `	if( nArg != 1 ){` |
|       7 | 5458 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5459 | `			"ArgumentCountError",` |
|       - | 5460 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5461 | `			nArg` |
|       - | 5462 | `			);` |
|       - | 5463 | `	}` |
|       - | 5464 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5465 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5466 | `		/* Type mismatch -> TypeError */` |
|       7 | 5467 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5468 | `			"TypeError",` |
|       - | 5469 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5470 | `			ph7_type_name(apArg[0])` |
|       - | 5471 | `			);` |
|       - | 5472 | `	}` |
|      46 | 5473 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5474 | `	if( pMap->nEntry < 1 ){` |
|       - | 5475 | `		/* Nothing to compute,return 0 */` |
|       3 | 5476 | `		ph7_result_int(pCtx,0);` |
|       3 | 5477 | `		return PH7_OK;` |
|       - | 5478 | `	}` |
|       - | 5479 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5480 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5481 | `	 */` |
|      44 | 5482 | `	pEntry = pMap->pFirst;` |
|     112 | 5483 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5484 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5485 | `		if( pObj ){` |
|      94 | 5486 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5487 | `				useDouble = 1;` |
|      19 | 5488 | `				break;` |
|       - | 5489 | `			}` |
|      76 | 5490 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5491 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5492 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5493 | `				sxu32 i;` |
|      23 | 5494 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5495 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5496 | `						useDouble = 1;` |
|       7 | 5497 | `						break;` |
|       - | 5498 | `					}` |
|       6 | 5499 | `				}` |
|      13 | 5500 | `				if( useDouble ){` |
|       7 | 5501 | `					break;` |
|       - | 5502 | `				}` |
|       3 | 5503 | `			}` |
|      34 | 5504 | `		}` |
|      70 | 5505 | `		pEntry = pEntry->pPrev;` |
|      36 | 5506 | `	}` |
|      44 | 5507 | `	if( useDouble ){` |
|      25 | 5508 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5509 | `	}else{` |
|      20 | 5510 | `		Int64Sum(pCtx,pMap);` |
|       - | 5511 | `	}` |
|      44 | 5512 | `	return PH7_OK;` |
|      28 | 5513 |  |
|       - | 5514 | `/*` |
|       - | 5515 | ` * number array_product(array $array )` |
|       - | 5516 | ` *  Calculate the product of values in an array.` |
|       - | 5517 | ` * Parameters` |
|       - | 5518 | ` *  $array: The input array.` |
|       - | 5519 | ` * Return` |
|       - | 5520 | ` *  Returns the product of values as an integer or float.` |
|       - | 5521 | ` */` |
|     ! 0 | 5522 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5523 |  |
|       - | 5524 | `	ph7_hashmap_node *pEntry;` |
|       - | 5525 | `	ph7_value *pObj;` |
|       - | 5526 | `	double dProd;` |
|       - | 5527 | `	sxu32 n;` |
|     ! 0 | 5528 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5529 | `	dProd = 1;` |
|     ! 0 | 5530 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5531 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5532 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5533 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5534 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5535 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5536 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5537 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5538 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5539 | `					double dv = 0;` |
|     ! 0 | 5540 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5541 | `					dProd *= dv;` |
|     ! 0 | 5542 | `				}` |
|     ! 0 | 5543 | `			}` |
|     ! 0 | 5544 | `		}` |
|       - | 5545 | `		/* Point to the next entry */` |
|     ! 0 | 5546 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5547 | `	}` |
|       - | 5548 | `	/* Return product */` |
|     ! 0 | 5549 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5550 |  |
|     ! 0 | 5551 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5552 |  |
|       - | 5553 | `	ph7_hashmap_node *pEntry;` |
|       - | 5554 | `	ph7_value *pObj;` |
|       - | 5555 | `	sxi64 nProd;` |
|       - | 5556 | `	sxu32 n;` |
|     ! 0 | 5557 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5558 | `	nProd = 1;` |
|     ! 0 | 5559 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5560 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5561 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5562 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5563 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5564 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5565 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5566 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5567 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5568 | `					sxi64 nv = 0;` |
|     ! 0 | 5569 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5570 | `					nProd *= nv;` |
|     ! 0 | 5571 | `				}` |
|     ! 0 | 5572 | `			}` |
|     ! 0 | 5573 | `		}` |
|       - | 5574 | `		/* Point to the next entry */` |
|     ! 0 | 5575 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5576 | `	}` |
|       - | 5577 | `	/* Return product */` |
|     ! 0 | 5578 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5579 |  |
|       - | 5580 | `/* number array_product(array $array )` |
|       - | 5581 | ` * (See block-block comment above)` |
|       - | 5582 | ` */` |
|     ! 0 | 5583 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5584 |  |
|       - | 5585 | `	ph7_hashmap *pMap;` |
|       - | 5586 | `	ph7_value *pObj;` |
|     ! 0 | 5587 | `	if( nArg < 1 ){` |
|       - | 5588 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5589 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5590 | `		return PH7_OK;` |
|       - | 5591 | `	}` |
|       - | 5592 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5593 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5594 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5595 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5596 | `		return PH7_OK;` |
|       - | 5597 | `	}` |
|     ! 0 | 5598 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5599 | `	if( pMap->nEntry < 1 ){` |
|       - | 5600 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5601 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5602 | `		return PH7_OK;` |
|       - | 5603 | `	}` |
|       - | 5604 | `	/* If the first element is of type float,then perform floating` |
|       - | 5605 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5606 | `	 */` |
|     ! 0 | 5607 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5608 | `	if( pObj == 0 ){` |
|     ! 0 | 5609 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5610 | `		return PH7_OK;` |
|       - | 5611 | `	}` |
|     ! 0 | 5612 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5613 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5614 | `	}else{` |
|     ! 0 | 5615 | `		Int64Prod(pCtx,pMap);` |
|       - | 5616 | `	}` |
|     ! 0 | 5617 | `	return PH7_OK;` |
|     ! 0 | 5618 |  |
|       - | 5619 | `/*` |
|       - | 5620 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5621 | ` *  Pick one or more random entries out of an array.` |
|       - | 5622 | ` * Parameters` |
|       - | 5623 | ` * $input` |
|       - | 5624 | ` *  The input array.` |
|       - | 5625 | ` * $num_req` |
|       - | 5626 | ` *  Specifies how many entries you want to pick.` |
|       - | 5627 | ` * Return` |
|       - | 5628 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5629 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5630 | ` *  NULL is returned on failure.` |
|       - | 5631 | ` */` |
|       6 | 5632 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5633 |  |
|       - | 5634 | `	ph7_hashmap_node *pNode;` |
|       - | 5635 | `	ph7_hashmap *pMap;` |
|       7 | 5636 | `	int nItem = 1;` |
|       7 | 5637 | `	if( nArg < 1 ){` |
|       - | 5638 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5639 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5640 | `		return PH7_OK;` |
|       - | 5641 | `	}` |
|       - | 5642 | `	/* Make sure we are dealing with an array */` |
|       7 | 5643 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5644 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5645 | `		return PH7_OK;` |
|       - | 5646 | `	}` |
|       - | 5647 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5648 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5649 | `	if(pMap->nEntry < 1 ){` |
|       - | 5650 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5651 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5652 | `		return PH7_OK;` |
|       - | 5653 | `	}` |
|       7 | 5654 | `	if( nArg > 1 ){` |
|       3 | 5655 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5656 | `	}` |
|       7 | 5657 | `	if( nItem < 2 ){` |
|       - | 5658 | `		sxu32 nEntry;` |
|       - | 5659 | `		/* Select a random number */` |
|       5 | 5660 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5661 | `		/* Extract the desired entry.` |
|       - | 5662 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5663 | `		 */` |
|       5 | 5664 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       4 | 5665 | `			pNode = pMap->pLast;` |
|       4 | 5666 | `			nEntry = pMap->nEntry - nEntry;` |
|       4 | 5667 | `			if( nEntry > 1 ){` |
|     ! 0 | 5668 | `				for(;;){` |
|     ! 0 | 5669 | `					if( nEntry == 0 ){` |
|     ! 0 | 5670 | `						break;` |
|       - | 5671 | `					}` |
|       - | 5672 | `					/* Point to the previous entry */` |
|     ! 0 | 5673 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5674 | `					nEntry--;` |
|     ! 0 | 5675 | `				}` |
|     ! 0 | 5676 | `			}` |
|       2 | 5677 | `		}else{` |
|       2 | 5678 | `			pNode = pMap->pFirst;` |
|       1 | 5679 | `			for(;;){` |
|       2 | 5680 | `				if( nEntry == 0 ){` |
|       2 | 5681 | `					break;` |
|       - | 5682 | `				}` |
|       - | 5683 | `				/* Point to the next entry */` |
|       1 | 5684 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 5685 | `				nEntry--;` |
|       1 | 5686 | `			}` |
|       - | 5687 | `		}` |
|       5 | 5688 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5689 | `			/* Int key */` |
|       3 | 5690 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5691 | `		}else{` |
|       - | 5692 | `			/* Blob key */` |
|       3 | 5693 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5694 | `		}` |
|       3 | 5695 | `	}else{` |
|       - | 5696 | `		ph7_value sKey,*pArray;` |
|       - | 5697 | `		ph7_hashmap *pDest;` |
|       - | 5698 | `		/* Create a new array */` |
|       3 | 5699 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5700 | `		if( pArray == 0 ){` |
|     ! 0 | 5701 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5702 | `			return PH7_OK;` |
|       - | 5703 | `		}` |
|       - | 5704 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5705 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5706 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5707 | `		/* Copy the first n items */` |
|       3 | 5708 | `		pNode = pMap->pFirst;` |
|       3 | 5709 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5710 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5711 | `		}` |
|       7 | 5712 | `		while( nItem > 0){` |
|       5 | 5713 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5714 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5715 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5716 | `			/* Point to the next entry */` |
|       5 | 5717 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5718 | `			nItem--;` |
|       1 | 5719 | `		}` |
|       - | 5720 | `		/* Shuffle the array */` |
|       3 | 5721 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5722 | `		/* Rehash node */` |
|       3 | 5723 | `		HashmapSortRehash(pDest);` |
|       - | 5724 | `		/* Return the random array */` |
|       3 | 5725 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5726 | `	}` |
|       7 | 5727 | `	return PH7_OK;` |
|       4 | 5728 |  |
|       - | 5729 | `/*` |
|       - | 5730 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5731 | ` *  Split an array into chunks.` |
|       - | 5732 | ` * Parameters` |
|       - | 5733 | ` * $input` |
|       - | 5734 | ` *   The array to work on` |
|       - | 5735 | ` * $size` |
|       - | 5736 | ` *   The size of each chunk` |
|       - | 5737 | ` * $preserve_keys` |
|       - | 5738 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5739 | ` *   the chunk numerically.` |
|       - | 5740 | ` * Return` |
|       - | 5741 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5742 | ` *  zero, with each dimension containing size elements.` |
|       - | 5743 | ` */` |
|      42 | 5744 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5745 |  |
|       - | 5746 | `	ph7_value *pArray,*pChunk;` |
|       - | 5747 | `	ph7_hashmap_node *pEntry;` |
|       - | 5748 | `	ph7_hashmap *pMap;` |
|       - | 5749 | `	int bPreserve;` |
|       - | 5750 | `	sxu32 nChunk;` |
|       - | 5751 | `	sxu32 nSize;` |
|       - | 5752 | `	sxu32 n;` |
|       - | 5753 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5754 | `	if( nArg < 2 ){` |
|       - | 5755 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5756 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5757 | `			"ArgumentCountError",` |
|       - | 5758 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5759 | `			nArg` |
|       - | 5760 | `			);` |
|       - | 5761 | `	}` |
|      42 | 5762 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5763 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5764 | `			"TypeError",` |
|       - | 5765 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5766 | `			ph7_type_name(apArg[0])` |
|       - | 5767 | `			);` |
|       - | 5768 | `	}` |
|       - | 5769 | `	/* Create a new array */` |
|      40 | 5770 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5771 | `	if( pArray == 0 ){` |
|     ! 0 | 5772 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5773 | `		return PH7_OK;` |
|       - | 5774 | `	}` |
|       - | 5775 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5776 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5777 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5778 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5779 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5780 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5781 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5782 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5783 | `			"TypeError",` |
|       - | 5784 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5785 | `			ph7_type_name(apArg[1])` |
|       - | 5786 | `			);` |
|       - | 5787 | `	}` |
|       - | 5788 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5789 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5790 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5791 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5792 | `		int len;` |
|       3 | 5793 | `		sxu8 bReal = FALSE;` |
|       3 | 5794 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5795 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5796 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5797 | `				"TypeError",` |
|       - | 5798 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5799 | `				);` |
|       - | 5800 | `		}` |
|     ! 0 | 5801 | `		if( bReal ){` |
|       - | 5802 | `			/* float-string -> warn but allow */` |
|     ! 0 | 5803 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5804 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 5805 | `				zStr` |
|       - | 5806 | `				);` |
|     ! 0 | 5807 | `		}` |
|     ! 0 | 5808 | `	}` |
|       - | 5809 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 5810 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 5811 | `	 * later via ph7_value_to_int. */` |
|      38 | 5812 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5813 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5814 | `		sxi64 i = (sxi64)d;` |
|       3 | 5815 | `		if( d != (double)i ){` |
|       4 | 5816 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5817 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 5818 | `				d` |
|       - | 5819 | `				);` |
|       1 | 5820 | `		}` |
|       1 | 5821 | `	}` |
|       - | 5822 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5823 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5824 | `	{` |
|      38 | 5825 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 5826 | `		if( nSizeSigned < 1 ){` |
|       - | 5827 | `			/* size <= 0 -> ValueError */` |
|       5 | 5828 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5829 | `				"ValueError",` |
|       - | 5830 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5831 | `				);` |
|       - | 5832 | `		}` |
|      34 | 5833 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5834 | `	}` |
|      34 | 5835 | `	if( nSize >= pMap->nEntry ){` |
|       - | 5836 | `		/* Return the whole array */` |
|       3 | 5837 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 5838 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5839 | `		return PH7_OK;` |
|       - | 5840 | `	}` |
|      32 | 5841 | `	bPreserve = 0;` |
|      32 | 5842 | `	if( nArg > 2 ){` |
|       - | 5843 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5844 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5845 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5846 | `		 * normally, matching PHP behaviour. */` |
|      45 | 5847 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 5848 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 5849 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 5850 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5851 | `				"TypeError",` |
|       - | 5852 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 5853 | `				ph7_type_name(apArg[2])` |
|       - | 5854 | `				);` |
|       - | 5855 | `		}` |
|      21 | 5856 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 5857 | `	}` |
|       - | 5858 | `	/* Start processing */` |
|      27 | 5859 | `	pEntry = pMap->pFirst;` |
|      27 | 5860 | `	nChunk = 0;` |
|      27 | 5861 | `	pChunk = 0;` |
|      27 | 5862 | `	n = pMap->nEntry;` |
|      56 | 5863 | `	for( ;; ){` |
|     113 | 5864 | `		if( n < 1 ){` |
|       - | 5865 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5866 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5867 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5868 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5869 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5870 | `			 * exists. */` |
|      27 | 5871 | `			if( pChunk ){` |
|      27 | 5872 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 5873 | `			}` |
|      27 | 5874 | `			break;` |
|       - | 5875 | `		}` |
|      87 | 5876 | `		if( nChunk < 1 ){` |
|      71 | 5877 | `			if( pChunk ){` |
|       - | 5878 | `				/* Put the first chunk */` |
|      45 | 5879 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 5880 | `			}` |
|       - | 5881 | `			/* Create a new dimension */` |
|      71 | 5882 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5883 | `												   * will be automatically released as soon we return` |
|       - | 5884 | `												   * from this function */` |
|      71 | 5885 | `			if( pChunk == 0 ){` |
|     ! 0 | 5886 | `				break;` |
|       - | 5887 | `			}` |
|      71 | 5888 | `			nChunk = nSize;` |
|      35 | 5889 | `		}` |
|       - | 5890 | `		/* Insert the entry */` |
|      87 | 5891 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5892 | `		/* Point to the next entry */` |
|      87 | 5893 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 5894 | `		nChunk--;` |
|      87 | 5895 | `		n--;` |
|       1 | 5896 | `	}` |
|       - | 5897 | `	/* Return the multidimensional array */` |
|      27 | 5898 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5899 | `	return PH7_OK;` |
|      23 | 5900 |  |
|       - | 5901 | `/*` |
|       - | 5902 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5903 | ` *  Pad array to the specified length with a value.` |
|       - | 5904 | ` * $input` |
|       - | 5905 | ` *   Initial array of values to pad.` |
|       - | 5906 | ` * $pad_size` |
|       - | 5907 | ` *   New size of the array.` |
|       - | 5908 | ` * $pad_value` |
|       - | 5909 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5910 | ` */` |
|      28 | 5911 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5912 |  |
|       - | 5913 | `	ph7_hashmap *pMap;` |
|       - | 5914 | `	ph7_value *pArray;` |
|       - | 5915 | `	int nEntry;` |
|      30 | 5916 | `	if( nArg != 3 ){` |
|      10 | 5917 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5918 | `			"ArgumentCountError",` |
|       - | 5919 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 5920 | `			nArg` |
|       - | 5921 | `			);` |
|       - | 5922 | `	}` |
|      24 | 5923 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5924 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5925 | `			"TypeError",` |
|       - | 5926 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5927 | `			ph7_type_name(apArg[0])` |
|       - | 5928 | `			);` |
|       - | 5929 | `	}` |
|       - | 5930 | `	/* Create a new array */` |
|      21 | 5931 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 5932 | `	if( pArray == 0 ){` |
|     ! 0 | 5933 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5934 | `		return PH7_OK;` |
|       - | 5935 | `	}` |
|       - | 5936 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 5937 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5938 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 5939 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 5940 | `	if( nEntry < 0 ){` |
|       9 | 5941 | `		nEntry = -nEntry;` |
|       9 | 5942 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 5943 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5944 | `			/* Insert given items first */` |
|      17 | 5945 | `			while( nEntry > 0 ){` |
|      13 | 5946 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 5947 | `				nEntry--;` |
|       1 | 5948 | `			}` |
|       - | 5949 | `			/* Merge the two arrays */` |
|       5 | 5950 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5951 | `		}else{` |
|       5 | 5952 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5953 | `		}` |
|      17 | 5954 | `	}else if( nEntry > 0 ){` |
|      11 | 5955 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 5956 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5957 | `			/* Merge the two arrays first */` |
|       7 | 5958 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5959 | `			/* Insert given items */` |
|      25 | 5960 | `			while( nEntry > 0 ){` |
|      19 | 5961 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 5962 | `				nEntry--;` |
|       1 | 5963 | `			}` |
|       4 | 5964 | `		}else{` |
|       5 | 5965 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5966 | `		}` |
|       6 | 5967 | `	}else{` |
|       - | 5968 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 5969 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5970 | `	}` |
|       - | 5971 | `	/* Return the new array */` |
|      21 | 5972 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 5973 | `	return PH7_OK;` |
|      16 | 5974 |  |
|       - | 5975 | `/*` |
|       - | 5976 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5977 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5978 | ` * Parameters` |
|       - | 5979 | ` * $array` |
|       - | 5980 | ` *   The array in which elements are replaced.` |
|       - | 5981 | ` * $array1` |
|       - | 5982 | ` *   The array from which elements will be extracted.` |
|       - | 5983 | ` * ....` |
|       - | 5984 | ` *  More arrays from which elements will be extracted.` |
|       - | 5985 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5986 | ` * Return` |
|       - | 5987 | ` *  Returns an array.` |
|       - | 5988 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 5989 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 5990 | ` */` |
|      22 | 5991 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5992 |  |
|       - | 5993 | `	ph7_hashmap *pMap;` |
|       - | 5994 | `	ph7_value *pArray;` |
|       - | 5995 | `	int i;` |
|      24 | 5996 | `	if( nArg < 1 ){` |
|       3 | 5997 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5998 | `			"ArgumentCountError",` |
|       - | 5999 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6000 | `			);` |
|       - | 6001 | `	}` |
|      22 | 6002 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6003 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6004 | `			"TypeError",` |
|       - | 6005 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6006 | `			ph7_type_name(apArg[0])` |
|       - | 6007 | `			);` |
|       - | 6008 | `	}` |
|       - | 6009 | `	/* Create a new array */` |
|      20 | 6010 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6011 | `	if( pArray == 0 ){` |
|     ! 0 | 6012 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6013 | `		return PH7_OK;` |
|       - | 6014 | `	}` |
|       - | 6015 | `	/* Overwrite from the first array */` |
|      20 | 6016 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6017 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6018 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6019 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6020 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6021 | `			/* Type mismatch -> TypeError */` |
|       4 | 6022 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6023 | `				"TypeError",` |
|       - | 6024 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6025 | `				i + 1,` |
|       2 | 6026 | `				ph7_type_name(apArg[i])` |
|       - | 6027 | `				);` |
|       - | 6028 | `		}` |
|       - | 6029 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6030 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6031 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6032 | `	}` |
|       - | 6033 | `	/* Return the new array */` |
|      17 | 6034 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6035 | `	return PH7_OK;` |
|      13 | 6036 |  |
|       - | 6037 | `/*` |
|       - | 6038 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6039 | ` *  Filters elements of an array using a callback function.` |
|       - | 6040 | ` * Parameters` |
|       - | 6041 | ` *  $input` |
|       - | 6042 | ` *    The array to iterate over` |
|       - | 6043 | ` * $callback` |
|       - | 6044 | ` *    The callback function to use` |
|       - | 6045 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6046 | ` *    will be removed.` |
|       - | 6047 | ` * Return` |
|       - | 6048 | ` *  The filtered array.` |
|       - | 6049 | ` */` |
|      18 | 6050 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6051 |  |
|       - | 6052 | `	ph7_hashmap_node *pEntry;` |
|       - | 6053 | `	ph7_hashmap *pMap;` |
|       - | 6054 | `	ph7_value *pArray;` |
|       - | 6055 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6056 | `	ph7_value *pValue;` |
|       - | 6057 | `	sxi32 rc;` |
|       - | 6058 | `	int keep;` |
|       - | 6059 | `	sxu32 n;` |
|      20 | 6060 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6061 | `		/* Invalid arguments,return NULL */` |
|       5 | 6062 | `		ph7_result_null(pCtx);` |
|       5 | 6063 | `		return PH7_OK;` |
|       - | 6064 | `	}` |
|       - | 6065 | `	/* Create a new array */` |
|      16 | 6066 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 6067 | `	if( pArray == 0 ){` |
|     ! 0 | 6068 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6069 | `		return PH7_OK;` |
|       - | 6070 | `	}` |
|       - | 6071 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 6072 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 6073 | `	pEntry = pMap->pFirst;` |
|      16 | 6074 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 6075 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6076 | `	/* Perform the requested operation */` |
|      66 | 6077 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6078 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 6079 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 6080 | `		if( pValue == 0 ){` |
|       - | 6081 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6082 | `			keep = FALSE;` |
|      54 | 6083 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6084 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6085 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6086 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 6087 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6088 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6089 | `					int len;` |
|       3 | 6090 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6091 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6092 | `						"TypeError",` |
|       - | 6093 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6094 | `						zName` |
|       - | 6095 | `						);` |
|     ! 0 | 6096 | `				}else{` |
|     ! 0 | 6097 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6098 | `						"TypeError",` |
|       - | 6099 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6100 | `						ph7_type_name(apArg[1])` |
|       - | 6101 | `						);` |
|       - | 6102 | `				}` |
|       - | 6103 | `			}` |
|      23 | 6104 | `			keep = FALSE;` |
|      23 | 6105 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 6106 | `			if( rc == SXRET_OK ){` |
|       - | 6107 | `				/* Perform a boolean cast */` |
|      23 | 6108 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6109 | `			}` |
|      23 | 6110 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6111 | `		}else{` |
|       - | 6112 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6113 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6114 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6115 | `			 */` |
|      29 | 6116 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6117 | `		}` |
|      51 | 6118 | `		if( keep ){` |
|       - | 6119 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6120 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6121 | `		}` |
|       - | 6122 | `		/* Point to the next entry */` |
|      51 | 6123 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6124 | `	}` |
|      13 | 6125 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6126 | `	return PH7_OK;` |
|      11 | 6127 |  |
|       - | 6128 | `/*` |
|       - | 6129 | ` * array array_map(?callable $callback, array $array)` |
|       - | 6130 | ` *  Applies the callback to the elements of the given array.` |
|       - | 6131 | ` * Parameters` |
|       - | 6132 | ` *  $callback` |
|       - | 6133 | ` *   A callable to run for each element in the array, or NULL for the` |
|       - | 6134 | ` *   identity function (returns the array unchanged).` |
|       - | 6135 | ` *  $array` |
|       - | 6136 | ` *   An array to run through the callback function.` |
|       - | 6137 | ` * Return` |
|       - | 6138 | ` *  Returns an array containing the results of applying the callback` |
|       - | 6139 | ` *  function to each element of $array.` |
|       - | 6140 | ` */` |
|      28 | 6141 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6142 |  |
|       - | 6143 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6144 | `	ph7_hashmap_node *pEntry;` |
|       - | 6145 | `	ph7_hashmap *pMap;` |
|       - | 6146 | `	int bNullCallback;` |
|       - | 6147 | `	sxu32 n;` |
|      30 | 6148 | `	if( nArg < 2 ){` |
|       7 | 6149 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6150 | `			"ArgumentCountError",` |
|       - | 6151 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6152 | `			nArg` |
|       - | 6153 | `			);` |
|       - | 6154 | `	}` |
|      26 | 6155 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 6156 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6157 | `			"TypeError",` |
|       - | 6158 | `			"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6159 | `			ph7_type_name(apArg[1])` |
|       - | 6160 | `			);` |
|       - | 6161 | `	}` |
|      24 | 6162 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      24 | 6163 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6164 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6165 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6166 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6167 | `				"TypeError",` |
|       - | 6168 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6169 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6170 | `				zFunc` |
|       - | 6171 | `				);` |
|       - | 6172 | `		}` |
|       3 | 6173 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6174 | `			"TypeError",` |
|       - | 6175 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6176 | `			"no array or string given"` |
|       - | 6177 | `			);` |
|       - | 6178 | `	}` |
|       - | 6179 | `	/* Create a new array */` |
|      19 | 6180 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 6181 | `	if( pArray == 0 ){` |
|     ! 0 | 6182 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6183 | `		return PH7_OK;` |
|       - | 6184 | `	}` |
|       - | 6185 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6186 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      19 | 6187 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 6188 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6189 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 6190 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 6191 | `	/* Perform the requested operation */` |
|      19 | 6192 | `	pEntry = pMap->pFirst;` |
|      53 | 6193 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6194 | `		/* Extract the node value */` |
|      35 | 6195 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      35 | 6196 | `		if( pValue ){` |
|       - | 6197 | `			/* Extract the node key */` |
|      35 | 6198 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      35 | 6199 | `			if( bNullCallback ){` |
|       - | 6200 | `				/* NULL callback: identity function, keep original value */` |
|      11 | 6201 | `				ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6202 | `			}else{` |
|       - | 6203 | `				/* Invoke the supplied callback */` |
|      25 | 6204 | `				PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 6205 | `				/* Insert the callback return value */` |
|      25 | 6206 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6207 | `			}` |
|      35 | 6208 | `			PH7_MemObjRelease(&sKey);` |
|      35 | 6209 | `			PH7_MemObjRelease(&sResult);` |
|      17 | 6210 | `		}` |
|       - | 6211 | `		/* Point to the next entry */` |
|      35 | 6212 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      18 | 6213 | `	}` |
|      19 | 6214 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 6215 | `	return PH7_OK;` |
|      16 | 6216 |  |
|       - | 6217 | `/*` |
|       - | 6218 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6219 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6220 | ` * Parameters` |
|       - | 6221 | ` *  $array` |
|       - | 6222 | ` *   The input array.` |
|       - | 6223 | ` *  $callback` |
|       - | 6224 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6225 | ` *  $initial` |
|       - | 6226 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6227 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6228 | ` * Return` |
|       - | 6229 | ` *  Returns the resulting value.` |
|       - | 6230 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6231 | ` */` |
|      30 | 6232 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6233 |  |
|       - | 6234 | `	ph7_hashmap_node *pEntry;` |
|       - | 6235 | `	ph7_hashmap *pMap;` |
|       - | 6236 | `	ph7_value *pValue;` |
|       - | 6237 | `	ph7_value sResult;` |
|       - | 6238 | `	sxu32 n;` |
|      32 | 6239 | `	if( nArg < 2 ){` |
|       7 | 6240 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6241 | `			"ArgumentCountError",` |
|       - | 6242 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6243 | `			nArg` |
|       - | 6244 | `			);` |
|       - | 6245 | `	}` |
|      28 | 6246 | `	if( nArg > 3 ){` |
|       4 | 6247 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6248 | `			"ArgumentCountError",` |
|       - | 6249 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6250 | `			nArg` |
|       - | 6251 | `			);` |
|       - | 6252 | `	}` |
|      26 | 6253 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6254 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6255 | `			"TypeError",` |
|       - | 6256 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6257 | `			ph7_type_name(apArg[0])` |
|       - | 6258 | `			);` |
|       - | 6259 | `	}` |
|      24 | 6260 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6261 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6262 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6263 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6264 | `				"TypeError",` |
|       - | 6265 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6266 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6267 | `				zFunc` |
|       - | 6268 | `				);` |
|       - | 6269 | `		}` |
|       7 | 6270 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6271 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6272 | `				"TypeError",` |
|       - | 6273 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6274 | `				"array callback must have exactly two members"` |
|       - | 6275 | `				);` |
|       - | 6276 | `		}` |
|       5 | 6277 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6278 | `			"TypeError",` |
|       - | 6279 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6280 | `			"no array or string given"` |
|       - | 6281 | `			);` |
|       - | 6282 | `	}` |
|       - | 6283 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 6284 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6285 | `	/* Assume a NULL initial value */` |
|      15 | 6286 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      15 | 6287 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      15 | 6288 | `	if( nArg > 2 ){` |
|       - | 6289 | `		/* Set the initial value */` |
|      11 | 6290 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6291 | `	}` |
|       - | 6292 | `	/* Perform the requested operation */` |
|      15 | 6293 | `	pEntry = pMap->pFirst;` |
|      43 | 6294 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6295 | `		/* Extract the node value */` |
|      29 | 6296 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6297 | `		/* Invoke the supplied callback */` |
|      29 | 6298 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6299 | `		/* Point to the next entry */` |
|      29 | 6300 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6301 | `	}` |
|      15 | 6302 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6303 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6304 | `	return PH7_OK;` |
|      17 | 6305 |  |
|       - | 6306 | `/*` |
|       - | 6307 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6308 | ` *  Apply a user function to every member of an array.` |
|       - | 6309 | ` * Parameters` |
|       - | 6310 | ` *  $array` |
|       - | 6311 | ` *   The input array.` |
|       - | 6312 | ` *  $funcname` |
|       - | 6313 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6314 | ` *   the first, and the key/index second.` |
|       - | 6315 | ` * Note:` |
|       - | 6316 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6317 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6318 | ` *  be made in the original array itself.` |
|       - | 6319 | ` *  $userdata` |
|       - | 6320 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6321 | ` *   to the callback funcname.` |
|       - | 6322 | ` * Return` |
|       - | 6323 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6324 | ` */` |
|      36 | 6325 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6326 |  |
|       - | 6327 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6328 | `	ph7_hashmap_node *pEntry;` |
|       - | 6329 | `	ph7_hashmap *pMap;` |
|       - | 6330 | `	sxu32 n;` |
|      38 | 6331 | `	if( nArg < 2 ){` |
|       7 | 6332 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6333 | `			"ArgumentCountError",` |
|       - | 6334 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6335 | `			nArg` |
|       - | 6336 | `			);` |
|       - | 6337 | `	}` |
|      34 | 6338 | `	if( nArg > 3 ){` |
|       4 | 6339 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6340 | `			"ArgumentCountError",` |
|       - | 6341 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6342 | `			nArg` |
|       - | 6343 | `			);` |
|       - | 6344 | `	}` |
|      32 | 6345 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6346 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6347 | `			"TypeError",` |
|       - | 6348 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6349 | `			ph7_type_name(apArg[0])` |
|       - | 6350 | `			);` |
|       - | 6351 | `	}` |
|      30 | 6352 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6353 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6354 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6355 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6356 | `				"TypeError",` |
|       - | 6357 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6358 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6359 | `				zFunc` |
|       - | 6360 | `				);` |
|       - | 6361 | `		}` |
|       9 | 6362 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6363 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6364 | `				"TypeError",` |
|       - | 6365 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6366 | `				"array callback must have exactly two members"` |
|       - | 6367 | `				);` |
|       - | 6368 | `		}` |
|       5 | 6369 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6370 | `			"TypeError",` |
|       - | 6371 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6372 | `			"no array or string given"` |
|       - | 6373 | `			);` |
|       - | 6374 | `	}` |
|      19 | 6375 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6376 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6377 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      19 | 6378 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6379 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6380 | `	/* Perform the desired operation */` |
|      19 | 6381 | `	pEntry = pMap->pFirst;` |
|      59 | 6382 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6383 | `		/* Extract the node value */` |
|      41 | 6384 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      41 | 6385 | `		if( pValue ){` |
|       - | 6386 | `			/* Extract the entry key */` |
|      41 | 6387 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6388 | `			/* Invoke the supplied callback */` |
|      41 | 6389 | `			PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      41 | 6390 | `			PH7_MemObjRelease(&sKey);` |
|      20 | 6391 | `		}` |
|       - | 6392 | `		/* Point to the next entry */` |
|      41 | 6393 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6394 | `	}` |
|       - | 6395 | `	/* All done, return TRUE */` |
|      19 | 6396 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6397 | `	return PH7_OK;` |
|      20 | 6398 |  |
|       - | 6399 | `/*` |
|       - | 6400 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6401 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6402 | ` */` |
|      22 | 6403 | `static void HashmapWalkRecursive(` |
|       - | 6404 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6405 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6406 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6407 | `	int iNest             /* Nesting level */` |
|       - | 6408 | `	)` |
|       1 | 6409 |  |
|       - | 6410 | `	ph7_hashmap_node *pEntry;` |
|       - | 6411 | `	ph7_value *pValue,sKey;` |
|       - | 6412 | `	sxu32 n;` |
|       - | 6413 | `	/* Iterate through hashmap entries */` |
|      23 | 6414 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6415 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6416 | `	pEntry = pMap->pFirst;` |
|      59 | 6417 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6418 | `		/* Extract the node value */` |
|      37 | 6419 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6420 | `		if( pValue ){` |
|      37 | 6421 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6422 | `				if( iNest < 32 ){` |
|       - | 6423 | `					/* Recurse */` |
|      11 | 6424 | `					iNest++;` |
|      11 | 6425 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6426 | `					iNest--;` |
|       5 | 6427 | `				}` |
|       6 | 6428 | `			}else{` |
|       - | 6429 | `				/* Extract the node key */` |
|      27 | 6430 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6431 | `				/* Invoke the supplied callback */` |
|      27 | 6432 | `				PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6433 | `				PH7_MemObjRelease(&sKey);` |
|       - | 6434 | `			}` |
|      18 | 6435 | `		}` |
|       - | 6436 | `		/* Point to the next entry */` |
|      37 | 6437 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6438 | `	}` |
|      23 | 6439 |  |
|       - | 6440 | `/*` |
|       - | 6441 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6442 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6443 | ` * Parameters` |
|       - | 6444 | ` *  $array` |
|       - | 6445 | ` *   The input array.` |
|       - | 6446 | ` *  $funcname` |
|       - | 6447 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6448 | ` *   the first, and the key/index second.` |
|       - | 6449 | ` * Note:` |
|       - | 6450 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6451 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6452 | ` *  be made in the original array itself.` |
|       - | 6453 | ` *  $userdata` |
|       - | 6454 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6455 | ` *   to the callback funcname.` |
|       - | 6456 | ` * Return` |
|       - | 6457 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6458 | ` */` |
|      30 | 6459 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6460 |  |
|       - | 6461 | `	ph7_hashmap *pMap;` |
|      32 | 6462 | `	if( nArg < 2 ){` |
|       7 | 6463 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6464 | `			"ArgumentCountError",` |
|       - | 6465 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 6466 | `			nArg` |
|       - | 6467 | `			);` |
|       - | 6468 | `	}` |
|      28 | 6469 | `	if( nArg > 3 ){` |
|       4 | 6470 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6471 | `			"ArgumentCountError",` |
|       - | 6472 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 6473 | `			nArg` |
|       - | 6474 | `			);` |
|       - | 6475 | `	}` |
|      26 | 6476 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6477 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6478 | `			"TypeError",` |
|       - | 6479 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6480 | `			ph7_type_name(apArg[0])` |
|       - | 6481 | `			);` |
|       - | 6482 | `	}` |
|      24 | 6483 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6484 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6485 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6486 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6487 | `				"TypeError",` |
|       - | 6488 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6489 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6490 | `				zFunc` |
|       - | 6491 | `				);` |
|       - | 6492 | `		}` |
|       9 | 6493 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6494 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6495 | `				"TypeError",` |
|       - | 6496 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6497 | `				"array callback must have exactly two members"` |
|       - | 6498 | `				);` |
|       - | 6499 | `		}` |
|       5 | 6500 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6501 | `			"TypeError",` |
|       - | 6502 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6503 | `			"no array or string given"` |
|       - | 6504 | `			);` |
|       - | 6505 | `	}` |
|       - | 6506 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6507 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6508 | `	/* Perform the desired operation */` |
|      13 | 6509 | `	HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6510 | `	/* All done, return TRUE */` |
|      13 | 6511 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6512 | `	return PH7_OK;` |
|      17 | 6513 |  |
|       - | 6514 | `/*` |
|       - | 6515 | ` * Table of hashmap functions.` |
|       - | 6516 | ` */` |
|       - | 6517 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6518 | `	{"count",             ph7_hashmap_count },` |
|       - | 6519 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6520 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6521 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6522 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6523 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6524 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6525 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6526 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6527 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6528 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6529 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6530 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6531 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6532 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6533 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6534 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6535 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6536 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6537 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6538 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6539 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6540 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6541 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6542 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6543 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6544 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6545 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6546 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6547 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6548 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6549 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6550 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6551 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6552 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6553 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6554 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6555 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6556 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6557 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6558 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6559 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6560 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6561 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6562 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6563 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6564 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6565 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6566 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6567 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6568 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6569 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6570 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6571 | `	{"current",           ph7_hashmap_current },` |
|       - | 6572 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6573 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6574 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6575 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6576 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6577 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6578 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6579 | `};` |
|       - | 6580 | `/*` |
|       - | 6581 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6582 | ` */` |
|    1672 | 6583 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6584 |  |
|       - | 6585 | `	sxu32 n;` |
|  103666 | 6586 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  101994 | 6587 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   50998 | 6588 | `	}` |
|    1674 | 6589 |  |
|       - | 6590 | `/*` |
|       - | 6591 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6592 | ` * the BLOB given as the first argument.` |
|       - | 6593 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6594 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6595 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6596 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6597 | ` */` |
|      26 | 6598 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6599 |  |
|       - | 6600 | `	ph7_hashmap_node *pEntry;` |
|       - | 6601 | `	ph7_value *pObj;` |
|      28 | 6602 | `	sxu32 n = 0;` |
|       - | 6603 | `	int isRef;` |
|       - | 6604 | `	sxi32 rc;` |
|       - | 6605 | `	int i;` |
|      28 | 6606 | `	if( nDepth > 31 ){` |
|       - | 6607 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6608 | `		/* Nesting limit reached */` |
|     ! 0 | 6609 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6610 | `		if( ShowType ){` |
|     ! 0 | 6611 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6612 | `		}` |
|     ! 0 | 6613 | `		return SXERR_LIMIT;` |
|       - | 6614 | `	}` |
|       - | 6615 | `	/* Point to the first inserted entry */` |
|      28 | 6616 | `	pEntry = pMap->pFirst;` |
|      28 | 6617 | `	rc = SXRET_OK;` |
|      28 | 6618 | `	if( !ShowType ){` |
|      15 | 6619 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6620 | `	}` |
|       - | 6621 | `	/* Total entries */` |
|      28 | 6622 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6623 | `#ifdef __WINNT__` |
|       2 | 6624 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6625 | `#else` |
|      26 | 6626 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6627 | `#endif` |
|      62 | 6628 | `	for(;;){` |
|     126 | 6629 | `		if( n >= pMap->nEntry ){` |
|      28 | 6630 | `			break;` |
|       - | 6631 | `		}` |
|     198 | 6632 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 6633 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 6634 | `		}` |
|       - | 6635 | `		/* Dump key */` |
|     100 | 6636 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 6637 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 6638 | `		}else{` |
|     101 | 6639 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6640 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6641 | `		}` |
|       - | 6642 | `#ifdef __WINNT__` |
|       2 | 6643 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6644 | `#else` |
|      98 | 6645 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6646 | `#endif` |
|       - | 6647 | `		/* Dump node value */` |
|     100 | 6648 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 6649 | `		isRef = 0;` |
|     100 | 6650 | `		if( pObj ){` |
|     100 | 6651 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6652 | `				/* Referenced object */` |
|     ! 0 | 6653 | `				isRef = 1;` |
|     ! 0 | 6654 | `			}` |
|     100 | 6655 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 6656 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6657 | `				break;` |
|       - | 6658 | `			}` |
|      49 | 6659 | `		}` |
|       - | 6660 | `		/* Point to the next entry */` |
|     100 | 6661 | `		n++;` |
|     100 | 6662 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6663 | `	}` |
|      54 | 6664 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 6665 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 6666 | `	}` |
|      28 | 6667 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 6668 | `	return rc;` |
|      15 | 6669 |  |
|       - | 6670 | `/*` |
|       - | 6671 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6672 | ` * retrieved entry.` |
|       - | 6673 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6674 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6675 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6676 | ` * a value different from PH7_OK.` |
|       - | 6677 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6678 | ` */` |
|   21450 | 6679 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6680 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6681 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6682 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6683 | `	)` |
|       2 | 6684 |  |
|       - | 6685 | `	ph7_hashmap_node *pEntry;` |
|       - | 6686 | `	ph7_value sKey,sValue;` |
|       - | 6687 | `	sxi32 rc;` |
|       - | 6688 | `	sxu32 n;` |
|       - | 6689 | `	/* Initialize walker parameter */` |
|   21452 | 6690 | `	rc = SXRET_OK;` |
|   21452 | 6691 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   21452 | 6692 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   21452 | 6693 | `	n = pMap->nEntry;` |
|   21452 | 6694 | `	pEntry = pMap->pFirst;` |
|       - | 6695 | `	/* Start the iteration process */` |
|   55045 | 6696 | `	for(;;){` |
|  110092 | 6697 | `		if( n < 1 ){` |
|   21452 | 6698 | `			break;` |
|       - | 6699 | `		}` |
|       - | 6700 | `		/* Extract a copy of the key and a copy the current value */` |
|   88642 | 6701 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   88642 | 6702 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6703 | `		/* Invoke the user callback */` |
|   88642 | 6704 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6705 | `		/* Release the copy of the key and the value */` |
|   88642 | 6706 | `		PH7_MemObjRelease(&sKey);` |
|   88642 | 6707 | `		PH7_MemObjRelease(&sValue);` |
|   88642 | 6708 | `		if( rc != PH7_OK ){` |
|       - | 6709 | `			/* Callback request an operation abort */` |
|     ! 0 | 6710 | `			return SXERR_ABORT;` |
|       - | 6711 | `		}` |
|       - | 6712 | `		/* Point to the next entry */` |
|   88642 | 6713 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   88642 | 6714 | `		n--;` |
|       2 | 6715 | `	}` |
|       - | 6716 | `	/* All done */` |
|   21452 | 6717 | `	return SXRET_OK;` |
|   10727 | 6718 |  |
|       - | 6719 |  |
