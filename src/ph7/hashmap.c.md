# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2982/3410 lines (87.45%)

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
| 3010268 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 3010270 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  311998 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  312000 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  312000 |   29 | `	sxu32 nH = 5381;` |
|  312000 |   30 | `	zEnd = &zIn[nLen];` |
|  347455 |   31 | `	for(;;){` |
|  694912 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  608176 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  546022 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  451718 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  312000 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     894 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       2 |   47 |  |
|     896 |   48 | `	sxi64 iCount = 0;` |
|     896 |   49 | `	if( !bRecursive ){` |
|     722 |   50 | `		iCount = pMap->nEntry;` |
|     362 |   51 | `	}else{` |
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
|     896 |   86 | `	return iCount;` |
|       2 |   87 |  |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 2951326 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 2951328 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2951328 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 2951328 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 2951328 |  104 | `	pNode->pMap  = &(*pMap);` |
| 2951328 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2951328 |  106 | `	pNode->nHash = nHash;` |
| 2951328 |  107 | `	pNode->xKey.iKey = iKey;` |
| 2951328 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 2951328 |  109 | `	return pNode;` |
| 1475665 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|  107406 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|  107408 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  107408 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|  107408 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|  107408 |  127 | `	pNode->pMap  = &(*pMap);` |
|  107408 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  107408 |  129 | `	pNode->nHash = nHash;` |
|  107408 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  107408 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  107408 |  132 | `	pNode->nValIdx = nValIdx;` |
|  107408 |  133 | `	return pNode;` |
|   53705 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 3058732 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  139 |  |
|       - |  140 | `	/* Link */` |
| 3058734 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2758502 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2758502 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1379250 |  144 | `	}` |
| 3058734 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 3058734 |  147 | `	if( pMap->pFirst == 0 ){` |
|   53704 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   53704 |  150 | `		pMap->pCur = pNode;` |
|   26853 |  151 | `	}else{` |
| 3005032 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 3058734 |  154 | `	++pMap->nEntry;` |
| 3058734 |  155 |  |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    6838 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  161 |  |
|    6840 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    6840 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    6840 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    6392 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3197 |  167 | `	}else{` |
|     449 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    6840 |  170 | `	if( pNode->pNextCollide ){` |
|    5343 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2671 |  172 | `	}` |
|    6840 |  173 | `	if( pMap->pFirst == pNode ){` |
|      88 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      43 |  175 | `	}` |
|    6840 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|      90 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      44 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    6840 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    6840 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     104 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     104 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     104 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  188 | `		}` |
|      51 |  189 | `	}` |
|    6840 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    6714 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3356 |  192 | `	}` |
|    6840 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    6840 |  194 | `	pMap->nEntry--;` |
|    6840 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      36 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      36 |  198 | `		pMap->apBucket = 0;` |
|      36 |  199 | `		pMap->nSize = 0;` |
|      36 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      17 |  201 | `	}` |
|    6840 |  202 |  |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 3058732 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  208 |  |
| 3058734 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   57970 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   57970 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   57970 |  215 | `		if( nNew < 1 ){` |
|   53704 |  216 | `			nNew = 16;` |
|   26851 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   57970 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   57970 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   57970 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   57970 |  230 | `		pMap->apBucket = apNew;` |
|   57970 |  231 | `		pMap->nSize = nNew;` |
|   57970 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   53704 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    4268 |  237 | `		pEntry = pMap->pFirst;` |
|    4268 |  238 | `		n = 0;` |
| 2024805 |  239 | `		for( ;; ){` |
| 4049612 |  240 | `			if( n >= pMap->nEntry ){` |
|    4268 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 4045346 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 4045346 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4045346 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3505930 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3505930 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1752964 |  250 | `			}` |
| 4045346 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 4045346 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4045346 |  254 | `			n++;` |
|       2 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    4268 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2133 |  258 | `	}` |
| 3005032 |  259 | `	return SXRET_OK;` |
| 1529368 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 2951326 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 2951328 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		/* Reserve a ph7_value for the value */` |
| 2951302 |  274 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2951302 |  275 | `		if( pObj == 0 ){` |
|     ! 0 |  276 | `			return SXERR_MEM;` |
|       - |  277 | `		}` |
| 2951302 |  278 | `		if( pValue ){` |
|       - |  279 | `			/* Duplicate the value */` |
| 2951302 |  280 | `			PH7_MemObjStore(pValue,pObj);` |
| 1475650 |  281 | `		}` |
| 2951302 |  282 | `		nIdx = pObj->nIdx;` |
| 1475652 |  283 | `	}else{` |
|      27 |  284 | `		nIdx = nRefIdx;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Hash the key */` |
| 2951328 |  287 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  288 | `	/* Allocate a new int node */` |
| 2951328 |  289 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2951328 |  290 | `	if( pNode == 0 ){` |
|     ! 0 |  291 | `		return SXERR_MEM;` |
|       - |  292 | `	}` |
| 2951328 |  293 | `	if( isForeign ){` |
|       - |  294 | `		/* Mark as a foregin entry */` |
|      27 |  295 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      13 |  296 | `	}` |
|       - |  297 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2951328 |  298 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2951328 |  299 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  300 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  301 | `		return rc;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Perform the insertion */` |
| 2951328 |  304 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  305 | `	/* Install in the reference table */` |
| 2951328 |  306 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  307 | `	/* All done */` |
| 2951328 |  308 | `	return SXRET_OK;` |
| 1475665 |  309 |  |
|       - |  310 | `/*` |
|       - |  311 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  312 | ` * hashmap.` |
|       - |  313 | ` */` |
|  107406 |  314 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  315 |  |
|       - |  316 | `	ph7_hashmap_node *pNode;` |
|       - |  317 | `	sxu32 nHash;` |
|       - |  318 | `	sxu32 nIdx;` |
|       - |  319 | `	sxi32 rc;` |
|  107408 |  320 | `	if( !isForeign ){` |
|       - |  321 | `		ph7_value *pObj;` |
|       - |  322 | `		/* Reserve a ph7_value for the value */` |
|   72338 |  323 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   72338 |  324 | `		if( pObj == 0 ){` |
|     ! 0 |  325 | `			return SXERR_MEM;` |
|       - |  326 | `		}` |
|   72338 |  327 | `		if( pValue ){` |
|       - |  328 | `			/* Duplicate the value */` |
|   72066 |  329 | `			PH7_MemObjStore(pValue,pObj);` |
|   36032 |  330 | `		}` |
|   72338 |  331 | `		nIdx = pObj->nIdx;` |
|   36170 |  332 | `	}else{` |
|   35072 |  333 | `		nIdx = nRefIdx;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Hash the key */` |
|  107408 |  336 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  337 | `	/* Allocate a new blob node */` |
|  107408 |  338 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  107408 |  339 | `	if( pNode == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|  107408 |  342 | `	if( isForeign ){` |
|       - |  343 | `		/* Mark as a foregin entry */` |
|   35072 |  344 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   17535 |  345 | `	}` |
|       - |  346 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  107408 |  347 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  107408 |  348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  349 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  350 | `		return rc;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Perform the insertion */` |
|  107408 |  353 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  354 | `	/* Install in the reference table */` |
|  107408 |  355 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  356 | `	/* All done */` |
|  107408 |  357 | `	return SXRET_OK;` |
|   53705 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  361 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  362 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  363 | ` */` |
|   47772 |  364 | `static sxi32 HashmapLookupIntKey(` |
|       - |  365 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  366 | `	sxi64 iKey,                /* lookup key */` |
|       - |  367 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  368 | `	)` |
|       2 |  369 |  |
|       - |  370 | `	ph7_hashmap_node *pNode;` |
|       - |  371 | `	sxu32 nHash;` |
|   47774 |  372 | `	if( pMap->nEntry < 1 ){` |
|       - |  373 | `		/* Don't bother hashing,there is no entry anyway */` |
|     446 |  374 | `		return SXERR_NOTFOUND;` |
|       - |  375 | `	}` |
|       - |  376 | `	/* Hash the key first */` |
|   47330 |  377 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  378 | `	/* Point to the appropriate bucket */` |
|   47330 |  379 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  380 | `	/* Perform the lookup */` |
|  412019 |  381 | `	for(;;){` |
|  824040 |  382 | `		if( pNode == 0 ){` |
|   45998 |  383 | `			break;` |
|       - |  384 | `		}` |
|  778708 |  385 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  775027 |  386 | `			&& pNode->nHash == nHash` |
|  386674 |  387 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  388 | `				/* Node found */` |
|    1334 |  389 | `				if( ppNode ){` |
|    1322 |  390 | `					*ppNode = pNode;` |
|     660 |  391 | `				}` |
|    1334 |  392 | `				return SXRET_OK;` |
|       - |  393 | `		}` |
|       - |  394 | `		/* Follow the collision link */` |
|  776711 |  395 | `		pNode = pNode->pNextCollide;` |
|       1 |  396 | `	}` |
|       - |  397 | `	/* No such entry */` |
|   45998 |  398 | `	return SXERR_NOTFOUND;` |
|   23888 |  399 |  |
|       - |  400 | `/*` |
|       - |  401 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  402 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  403 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  404 | ` */` |
|  217898 |  405 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  406 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  407 | `	const void *pKey,           /* Lookup key */` |
|       - |  408 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  409 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  410 | `	)` |
|       2 |  411 |  |
|       - |  412 | `	ph7_hashmap_node *pNode;` |
|       - |  413 | `	sxu32 nHash;` |
|  217900 |  414 | `	if( pMap->nEntry < 1 ){` |
|       - |  415 | `		/* Don't bother hashing,there is no entry anyway */` |
|   13308 |  416 | `		return SXERR_NOTFOUND;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Hash the key first */` |
|  204594 |  419 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  420 | `	/* Point to the appropriate bucket */` |
|  204594 |  421 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  422 | `	/* Perform the lookup */` |
|  184855 |  423 | `	for(;;){` |
|  369712 |  424 | `		if( pNode == 0 ){` |
|  156274 |  425 | `			break;` |
|       - |  426 | `		}` |
|  237598 |  427 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  211939 |  428 | `			&& pNode->nHash == nHash` |
|  129380 |  429 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   48322 |  430 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  431 | `				/* Node found */` |
|   48322 |  432 | `				if( ppNode ){` |
|   48294 |  433 | `					*ppNode = pNode;` |
|   24146 |  434 | `				}` |
|   48322 |  435 | `				return SXRET_OK;` |
|       - |  436 | `		}` |
|       - |  437 | `		/* Follow the collision link */` |
|  165120 |  438 | `		pNode = pNode->pNextCollide;` |
|       2 |  439 | `	}` |
|       - |  440 | `	/* No such entry */` |
|  156274 |  441 | `	return SXERR_NOTFOUND;` |
|  108951 |  442 |  |
|       - |  443 | `/*` |
|       - |  444 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  445 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  446 | ` */` |
|  218038 |  447 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  448 |  |
|  218040 |  449 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  218040 |  450 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  218040 |  451 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  452 | `		/* Octal not decimal number */` |
|       5 |  453 | `		return FALSE;` |
|       - |  454 | `	}` |
|  218036 |  455 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  456 | `		zIn++;` |
|     ! 0 |  457 | `	}` |
|  109350 |  458 | `	for(;;){` |
|  218702 |  459 | `		if( zIn >= zEnd ){` |
|     233 |  460 | `			return TRUE;` |
|       - |  461 | `		}` |
|  218470 |  462 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  108903 |  463 | `			break;` |
|       - |  464 | `		}` |
|     667 |  465 | `		zIn++;` |
|       1 |  466 | `	}` |
|       - |  467 | `	/* Key does not look like a decimal number */` |
|  217804 |  468 | `	return FALSE;` |
|  109021 |  469 |  |
|       - |  470 | `/*` |
|       - |  471 | ` * Check if a given key exists in the given hashmap.` |
|       - |  472 | ` * Write a pointer to the target node on success.` |
|       - |  473 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  474 | ` */` |
|  111630 |  475 | `static sxi32 HashmapLookup(` |
|       - |  476 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  477 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  478 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  479 | `	)` |
|       2 |  480 |  |
|  111632 |  481 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  482 | `	sxi32 rc;` |
|  111632 |  483 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  110438 |  484 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  485 | `			/* Force a string cast */` |
|     ! 0 |  486 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  487 | `		}` |
|  110438 |  488 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  489 | `			/* Perform a blob lookup */` |
|  110422 |  490 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  110422 |  491 | `			goto result;` |
|       - |  492 | `		}` |
|       8 |  493 | `	}` |
|       - |  494 | `	/* Perform an int lookup */` |
|    1212 |  495 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  496 | `		/* Force an integer cast */` |
|      27 |  497 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  498 | `	}` |
|       - |  499 | `	/* Perform an int lookup */` |
|    1212 |  500 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   55815 |  501 | `result:` |
|  111632 |  502 | `	if( rc == SXRET_OK ){` |
|       - |  503 | `		/* Node found */` |
|   49390 |  504 | `		if( ppNode ){` |
|   49348 |  505 | `			*ppNode = pNode;` |
|   24673 |  506 | `		}` |
|   49390 |  507 | `		return SXRET_OK;` |
|       - |  508 | `	}` |
|       - |  509 | `	/* No such entry */` |
|   62244 |  510 | `	return SXERR_NOTFOUND;` |
|   55817 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  514 | ` * hashmap.` |
|       - |  515 | ` * If a node with the given key already exists in the database` |
|       - |  516 | ` * then this function overwrite the old value.` |
|       - |  517 | ` */` |
| 3023354 |  518 | `static sxi32 HashmapInsert(` |
|       - |  519 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  520 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  521 | `	ph7_value *pVal    /* Node value */` |
|       - |  522 | `	)` |
|       2 |  523 |  |
| 3023356 |  524 | `	ph7_hashmap_node *pNode = 0;` |
| 3023356 |  525 | `	sxi32 rc = SXRET_OK;` |
| 3023356 |  526 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   72566 |  527 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  528 | `			/* Force a string cast */` |
|       3 |  529 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  530 | `		}` |
|   72566 |  531 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  532 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  533 | `				/* Automatic index assign */` |
|      34 |  534 | `				pKey = 0;` |
|      16 |  535 | `			}` |
|     256 |  536 | `			goto IntKey;` |
|       - |  537 | `		}` |
|  108467 |  538 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   36155 |  539 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|   72242 |  553 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  554 | `			/* Forbidden */` |
|       3 |  555 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Perform a blob-key insertion */` |
|   72240 |  559 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   72240 |  560 | `		return rc;` |
|       - |  561 | `	}` |
| 1475395 |  562 | `IntKey:` |
| 2951046 |  563 | `	if( pKey ){` |
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
| 2927640 |  600 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  601 | `			/* Forbidden */` |
|       3 |  602 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|       - |  605 | `		/* Assign an automatic index */` |
| 2927638 |  606 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2927638 |  607 | `		if( rc == SXRET_OK ){` |
| 2927638 |  608 | `			++pMap->iNextIdx;` |
| 1463818 |  609 | `		}` |
|       - |  610 | `	}` |
|       - |  611 | `	/* Insertion result */` |
| 2950956 |  612 | `	return rc;` |
| 1511679 |  613 |  |
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
|   35102 |  641 | `static sxi32 HashmapInsertByRef(` |
|       - |  642 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  643 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  644 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  645 | `	)` |
|       2 |  646 |  |
|   35104 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|   35104 |  648 | `	sxi32 rc = SXRET_OK;` |
|   35104 |  649 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   35078 |  650 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  651 | `			/* Force a string cast */` |
|     ! 0 |  652 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  653 | `		}` |
|   35078 |  654 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  655 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  656 | `				/* Automatic index assign */` |
|     ! 0 |  657 | `				pKey = 0;` |
|     ! 0 |  658 | `			}` |
|     ! 0 |  659 | `			goto IntKey;` |
|       - |  660 | `		}` |
|   52616 |  661 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   17538 |  662 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  663 | `				/* Overwrite */` |
|       7 |  664 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  665 | `				pNode->nValIdx = nRefIdx;` |
|       - |  666 | `				/* Install in the reference table */` |
|       7 |  667 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  668 | `				return SXRET_OK;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Perform a blob-key insertion */` |
|   35072 |  671 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   35072 |  672 | `		return rc;` |
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
|   17553 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * Extract node value.` |
|       - |  712 | ` */` |
| 1172264 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
| 1172266 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1172266 |  718 | `	return pObj;` |
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
|   55753 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   55755 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   55755 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   55755 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   55755 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   55755 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   55755 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   55755 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   55755 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   55755 |  783 | `	return rc;` |
|   27881 |  784 |  |
|       - |  785 | `/*` |
|       - |  786 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  787 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  788 | ` */` |
|   11614 |  789 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  790 |  |
|   11616 |  791 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  792 | `	sxu32 nBucket;` |
|       - |  793 | `	/* Remove old collision links */` |
|   11616 |  794 | `	if( pEntry->pPrevCollide ){` |
|    9368 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    4689 |  796 | `	}else{` |
|    2250 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  798 | `	}` |
|   11616 |  799 | `	if( pEntry->pNextCollide ){` |
|     882 |  800 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     435 |  801 | `	}` |
|   11616 |  802 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  803 | `	/* Compute the new hash */` |
|   11616 |  804 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   11616 |  805 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   11616 |  806 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  807 | `	/* Link to the new bucket */` |
|   11616 |  808 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11616 |  809 | `	if( pMap->apBucket[nBucket] ){` |
|    9606 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    4814 |  811 | `	}` |
|   11616 |  812 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11616 |  813 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  814 | `	/* Increment the automatic index */` |
|   11616 |  815 | `	pMap->iNextIdx++;` |
|   11616 |  816 |  |
|       - |  817 | `/*` |
|       - |  818 | ` * Perform a linear search on a given hashmap.` |
|       - |  819 | ` * Write a pointer to the target node on success.` |
|       - |  820 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  821 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  822 | ` * for more information.` |
|       - |  823 | ` */` |
|   29046 |  824 | `static int HashmapFindValue(` |
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
|   29048 |  837 | `	pEntry = pMap->pFirst;` |
|   29048 |  838 | `	n = pMap->nEntry;` |
|   29048 |  839 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   29048 |  840 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   69628 |  841 | `	for(;;){` |
|  139258 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  139160 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  139160 |  847 | `		if( pVal ){` |
|  139160 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  139160 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  139160 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  139160 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  139160 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  139160 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  139160 |  865 | `				if( rc == 0 ){` |
|   28950 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   28950 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   55105 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|  110212 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  110212 |  876 | `		n--;` |
|       2 |  877 | `	}` |
|       - |  878 | `	/* No such entry */` |
|      99 |  879 | `	return SXERR_NOTFOUND;` |
|   14525 |  880 |  |
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
|      18 |  989 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - |  990 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - |  991 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - |  992 | `	int bStrict          /* TRUE for strict comparison */` |
|       - |  993 | `	)` |
|       1 |  994 |  |
|       - |  995 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - |  996 | `	sxi32 rc;` |
|       - |  997 | `	sxu32 n;` |
|      19 |  998 | `	if( pLeft == pRight ){` |
|       - |  999 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - | 1000 | `		 * Unlike the zend engine.` |
|       - | 1001 | `		 */` |
|     ! 0 | 1002 | `		return 0;` |
|       - | 1003 | `	}` |
|      19 | 1004 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - | 1005 | `		/* Must have the same number of entries */` |
|       5 | 1006 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1007 | `	}` |
|       - | 1008 | `	/* Point to the first inserted entry of the left hashmap */` |
|      15 | 1009 | `	pLe = pLeft->pFirst;` |
|      15 | 1010 | `	pRe = 0; /* cc warning */` |
|       - | 1011 | `	/* Perform the comparison */` |
|      15 | 1012 | `	n = pLeft->nEntry;` |
|      15 | 1013 | `	for(;;){` |
|      31 | 1014 | `		if( n < 1 ){` |
|      13 | 1015 | `			break;` |
|       - | 1016 | `		}` |
|      19 | 1017 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1018 | `			/* Int key */` |
|      13 | 1019 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|       7 | 1020 | `		}else{` |
|       7 | 1021 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1022 | `			/* Blob key */` |
|       7 | 1023 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1024 | `		}` |
|      19 | 1025 | `		if( rc != SXRET_OK ){` |
|       - | 1026 | `			/* No such entry in the right side */` |
|     ! 0 | 1027 | `			return 1;` |
|       - | 1028 | `		}` |
|      19 | 1029 | `		rc = 0;` |
|      19 | 1030 | `		if( bStrict ){` |
|       - | 1031 | `			/* Make sure,the keys are of the same type */` |
|       3 | 1032 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1033 | `				rc = 1;` |
|     ! 0 | 1034 | `			}` |
|       1 | 1035 | `		}` |
|      19 | 1036 | `		if( !rc ){` |
|       - | 1037 | `			/* Compare nodes */` |
|      19 | 1038 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|       9 | 1039 | `		}` |
|      19 | 1040 | `		if( rc != 0 ){` |
|       - | 1041 | `			/* Nodes key/value differ */` |
|       3 | 1042 | `			return rc;` |
|       - | 1043 | `		}` |
|       - | 1044 | `		/* Point to the next entry */` |
|      17 | 1045 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      17 | 1046 | `		n--;` |
|       1 | 1047 | `	}` |
|      13 | 1048 | `	return 0; /* Hashmaps are equals */` |
|      10 | 1049 |  |
|       - | 1050 | `/*` |
|       - | 1051 | ` * Duplicate a hashmap node.` |
|       - | 1052 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1053 | ` */` |
|  544960 | 1054 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1055 | `	ph7_hashmap *pDest,` |
|       - | 1056 | `	ph7_hashmap_node *pEntry,` |
|       - | 1057 | `	ph7_value *pVal,` |
|       - | 1058 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1059 | `	)` |
|       2 | 1060 |  |
|  544962 | 1061 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1062 | `	ph7_value sKey;` |
|       - | 1063 | `	sxi32 rc;` |
|       - | 1064 |  |
|  544962 | 1065 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1066 | `		/* Blob key insertion */` |
|      91 | 1067 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      91 | 1068 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      91 | 1069 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      91 | 1070 | `		PH7_MemObjRelease(&sKey);` |
|      46 | 1071 | `	}else{` |
|       - | 1072 | `		/* Int key */` |
|  544872 | 1073 | `		if( iAction == 0 ){ /* Merge */` |
|  544650 | 1074 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  272548 | 1075 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1076 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1077 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1078 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1079 | `		}else{ /* Dup */` |
|     194 | 1080 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1081 | `		}` |
|       - | 1082 | `	}` |
|  544962 | 1083 | `	return rc;` |
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
|    1942 | 1097 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1098 |  |
|       - | 1099 | `	ph7_hashmap_node *pEntry;` |
|       - | 1100 | `	ph7_value *pVal;` |
|       - | 1101 | `	sxi32 rc;` |
|       - | 1102 | `	sxu32 n;` |
|    1944 | 1103 | `	if( pSrc == pDest ){` |
|       - | 1104 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1105 | `		 * Unlike the zend engine.` |
|       - | 1106 | `		 */` |
|     ! 0 | 1107 | `		return SXRET_OK;` |
|       - | 1108 | `	}` |
|       - | 1109 | `	/* Point to the first inserted entry in the source */` |
|    1944 | 1110 | `	pEntry = pSrc->pFirst;` |
|       - | 1111 | `	/* Perform the merge */` |
|  546640 | 1112 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1113 | `		/* Extract the node value */` |
|  544698 | 1114 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  544698 | 1115 | `		if( pVal ){` |
|       - | 1116 | `			/* Make a local copy of the value.` |
|       - | 1117 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1118 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1119 | `			 * to the old pool.` |
|       - | 1120 | `			 */` |
|  544698 | 1121 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  272350 | 1122 | `		}else{` |
|     ! 0 | 1123 | `			rc = SXRET_OK;` |
|       - | 1124 | `		}` |
|  544698 | 1125 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1126 | `			return rc;` |
|       - | 1127 | `		}` |
|       - | 1128 | `		/* Point to the next entry */` |
|  544698 | 1129 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  272350 | 1130 | `	}` |
|    1944 | 1131 | `	return SXRET_OK;` |
|     973 | 1132 |  |
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
|     104 | 1182 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1183 |  |
|       - | 1184 | `	ph7_hashmap_node *pEntry;` |
|       - | 1185 | `	ph7_value *pVal;` |
|       - | 1186 | `	sxi32 rc;` |
|       - | 1187 | `	sxu32 n;` |
|     106 | 1188 | `	if( pSrc == pDest ){` |
|       - | 1189 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1190 | `		 * Unlike the zend engine.` |
|       - | 1191 | `		 */` |
|     ! 0 | 1192 | `		return SXRET_OK;` |
|       - | 1193 | `	}` |
|       - | 1194 | `	/* Point to the first inserted entry in the source */` |
|     106 | 1195 | `	pEntry = pSrc->pFirst;` |
|       - | 1196 | `	/* Perform the duplication */` |
|     326 | 1197 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1198 | `		/* Extract the node value */` |
|     222 | 1199 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     222 | 1200 | `		if( pVal ){` |
|     222 | 1201 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|     112 | 1202 | `		}else{` |
|     ! 0 | 1203 | `			rc = SXRET_OK;` |
|       - | 1204 | `		}` |
|     222 | 1205 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1206 | `			return rc;` |
|       - | 1207 | `		}` |
|       - | 1208 | `		/* Point to the next entry */` |
|     222 | 1209 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     112 | 1210 | `	}` |
|     106 | 1211 | `	return SXRET_OK;` |
|      54 | 1212 |  |
|       - | 1213 | `/*` |
|       - | 1214 | ` * Copy-on-write separation for arrays.` |
|       - | 1215 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|       - | 1216 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|       - | 1217 | ` * Returns the (possibly new) hashmap pointer.` |
|       - | 1218 | ` */` |
|  192838 | 1219 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       2 | 1220 |  |
|  192840 | 1221 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1222 | `	ph7_hashmap *pNew;` |
|       - | 1223 | `	ph7_value *pBacking;` |
|  192840 | 1224 | `	if( pMap->iRef < 2 ){` |
|       - | 1225 | `		/* Sole owner, no separation needed */` |
|  190770 | 1226 | `		return pMap;` |
|       - | 1227 | `	}` |
|    2072 | 1228 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1229 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1230 | `		return pMap;` |
|       - | 1231 | `	}` |
|       - | 1232 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1233 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1234 | `	 * frame is popped. */` |
|    2072 | 1235 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2072 | 1236 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    3088 | 1237 | `		if( pBacking && pBacking != pValue` |
|    2053 | 1238 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2038 | 1239 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1240 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2038 | 1241 | `			pMap->iRef--;` |
|    2038 | 1242 | `			if( pMap->iRef < 2 ){` |
|       - | 1243 | `				/* After undoing stack ref, sole owner — no separation */` |
|    2002 | 1244 | `				pMap->iRef++;` |
|    2002 | 1245 | `				return pMap;` |
|       - | 1246 | `			}` |
|      38 | 1247 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|      38 | 1248 | `			if( pNew == 0 ){` |
|     ! 0 | 1249 | `				pMap->iRef++;` |
|     ! 0 | 1250 | `				return pMap;` |
|       - | 1251 | `			}` |
|      38 | 1252 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1253 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|     ! 0 | 1254 | `				PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1255 | `				pMap->iRef++;` |
|     ! 0 | 1256 | `				return pMap;` |
|       - | 1257 | `			}` |
|      38 | 1258 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|      38 | 1259 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|      38 | 1260 | `			pBacking->x.pOther = pNew;` |
|       - | 1261 | `			/* Update the stack value to match */` |
|      38 | 1262 | `			pValue->x.pOther = pNew;` |
|      38 | 1263 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|      38 | 1264 | `			return pNew;` |
|       - | 1265 | `		}` |
|      17 | 1266 | `	}` |
|      35 | 1267 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      35 | 1268 | `	if( pNew == 0 ){` |
|       - | 1269 | `		/* Allocation failure — fall through with shared map */` |
|     ! 0 | 1270 | `		return pMap;` |
|       - | 1271 | `	}` |
|      35 | 1272 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1273 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|     ! 0 | 1274 | `		PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1275 | `		return pMap;` |
|       - | 1276 | `	}` |
|      35 | 1277 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      35 | 1278 | `	pMap->iRef--;` |
|      35 | 1279 | `	pValue->x.pOther = pNew;` |
|      35 | 1280 | `	return pNew;` |
|   96421 | 1281 |  |
|       - | 1282 | `/*` |
|       - | 1283 | ` * Perform the union of two hashmaps.` |
|       - | 1284 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1285 | ` * with a variable holding an array as follows:` |
|       - | 1286 | ` * <?php` |
|       - | 1287 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1288 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1289 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1290 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1291 | ` * var_dump($c);` |
|       - | 1292 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1293 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1294 | ` * var_dump($c);` |
|       - | 1295 | ` * ?>` |
|       - | 1296 | ` * When executed, this script will print the following:` |
|       - | 1297 | ` * Union of $a and $b:` |
|       - | 1298 | ` * array(3) {` |
|       - | 1299 | ` *  ["a"]=>` |
|       - | 1300 | ` *  string(5) "apple"` |
|       - | 1301 | ` *  ["b"]=>` |
|       - | 1302 | ` * string(6) "banana"` |
|       - | 1303 | ` *  ["c"]=>` |
|       - | 1304 | ` * string(6) "cherry"` |
|       - | 1305 | ` * }` |
|       - | 1306 | ` * Union of $b and $a:` |
|       - | 1307 | ` * array(3) {` |
|       - | 1308 | ` * ["a"]=>` |
|       - | 1309 | ` * string(4) "pear"` |
|       - | 1310 | ` * ["b"]=>` |
|       - | 1311 | ` * string(10) "strawberry"` |
|       - | 1312 | ` * ["c"]=>` |
|       - | 1313 | ` * string(6) "cherry"` |
|       - | 1314 | ` * }` |
|       - | 1315 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1316 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1317 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1318 | ` */` |
|      10 | 1319 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       2 | 1320 |  |
|       - | 1321 | `	ph7_hashmap_node *pEntry;` |
|      12 | 1322 | `	sxi32 rc = SXRET_OK;` |
|       - | 1323 | `	ph7_value *pObj;` |
|       - | 1324 | `	sxu32 n;` |
|      12 | 1325 | `	if( pLeft == pRight ){` |
|       - | 1326 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1327 | `		 * Unlike the zend engine.` |
|       - | 1328 | `		 */` |
|     ! 0 | 1329 | `		return SXRET_OK;` |
|       - | 1330 | `	}` |
|       - | 1331 | `	/* Perform the union */` |
|      12 | 1332 | `	pEntry = pRight->pFirst;` |
|      32 | 1333 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1334 | `		/* Make sure the given key does not exists in the left array */` |
|      22 | 1335 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1336 | `			/* BLOB key */` |
|       7 | 1337 | `			if( SXRET_OK !=` |
|       6 | 1338 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|       3 | 1339 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|       3 | 1340 | `					if( pObj ){` |
|       3 | 1341 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1342 | `						/* Perform the insertion */` |
|       3 | 1343 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1344 | `							&sSafeVal,0,FALSE);` |
|       3 | 1345 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1346 | `							return rc;` |
|       - | 1347 | `						}` |
|       1 | 1348 | `					}` |
|       1 | 1349 | `			}` |
|       4 | 1350 | `		}else{` |
|       - | 1351 | `			/* INT key */` |
|      16 | 1352 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|      11 | 1353 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|      11 | 1354 | `				if( pObj ){` |
|      11 | 1355 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1356 | `					/* Perform the insertion */` |
|      11 | 1357 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      11 | 1358 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1359 | `						return rc;` |
|       - | 1360 | `					}` |
|       5 | 1361 | `				}` |
|       5 | 1362 | `			}` |
|       - | 1363 | `		}` |
|       - | 1364 | `		/* Point to the next entry */` |
|      22 | 1365 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      12 | 1366 | `	}` |
|      12 | 1367 | `	return SXRET_OK;` |
|       7 | 1368 |  |
|       - | 1369 | `/*` |
|       - | 1370 | ` * Allocate a new hashmap.` |
|       - | 1371 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1372 | ` */` |
|   84918 | 1373 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1374 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1375 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1376 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1377 | `	)` |
|       2 | 1378 |  |
|       - | 1379 | `	ph7_hashmap *pMap;` |
|       - | 1380 | `	/* Allocate a new instance */` |
|   84920 | 1381 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   84920 | 1382 | `	if( pMap == 0 ){` |
|     ! 0 | 1383 | `		return 0;` |
|       - | 1384 | `	}` |
|       - | 1385 | `	/* Zero the structure */` |
|   84920 | 1386 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1387 | `	/* Fill in the structure */` |
|   84920 | 1388 | `	pMap->pVm = &(*pVm);` |
|   84920 | 1389 | `	pMap->iRef = 1;` |
|       - | 1390 | `	/* Default hash functions */` |
|   84920 | 1391 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   84920 | 1392 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   84920 | 1393 | `	return pMap;` |
|   42461 | 1394 |  |
|       - | 1395 | `/*` |
|       - | 1396 | ` * Install superglobals in the given virtual machine.` |
|       - | 1397 | ` * Note on superglobals.` |
|       - | 1398 | ` *  According to the PHP language reference manual.` |
|       - | 1399 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1400 | `*   Description` |
|       - | 1401 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1402 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1403 | `*   global $variable; to access them within functions or methods.` |
|       - | 1404 | `*   These superglobal variables are:` |
|       - | 1405 | `*    $GLOBALS` |
|       - | 1406 | `*    $_SERVER` |
|       - | 1407 | `*    $_GET` |
|       - | 1408 | `*    $_POST` |
|       - | 1409 | `*    $_FILES` |
|       - | 1410 | `*    $_COOKIE` |
|       - | 1411 | `*    $_SESSION` |
|       - | 1412 | `*    $_REQUEST` |
|       - | 1413 | `*    $_ENV` |
|       - | 1414 | `*/` |
|    2808 | 1415 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       2 | 1416 |  |
|       - | 1417 | `	static const char * azSuper[] = {` |
|       - | 1418 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1419 | `		"_GET",      /* $_GET */` |
|       - | 1420 | `		"_POST",     /* $_POST */` |
|       - | 1421 | `		"_FILES",    /* $_FILES */` |
|       - | 1422 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1423 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1424 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1425 | `		"_ENV",      /* $_ENV */` |
|       - | 1426 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1427 | `		"argv"       /* $argv */` |
|       - | 1428 | `	};` |
|       - | 1429 | `	ph7_hashmap *pMap;` |
|       - | 1430 | `	ph7_value *pObj;` |
|       - | 1431 | `	SyString *pFile;` |
|       - | 1432 | `	sxi32 rc;` |
|       - | 1433 | `	sxu32 n;` |
|       - | 1434 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    2810 | 1435 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    2810 | 1436 | `	if( pMap == 0 ){` |
|     ! 0 | 1437 | `		return SXERR_MEM;` |
|       - | 1438 | `	}` |
|    2810 | 1439 | `	pVm->pGlobal = pMap;` |
|       - | 1440 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    2810 | 1441 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    2810 | 1442 | `	if( pObj == 0 ){` |
|     ! 0 | 1443 | `		return SXERR_MEM;` |
|       - | 1444 | `	}` |
|    2810 | 1445 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1446 | `	/* Record object index */` |
|    2810 | 1447 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1448 | `	/* Install the special $GLOBALS array */` |
|    2810 | 1449 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    2810 | 1450 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1451 | `		return rc;` |
|       - | 1452 | `	}` |
|       - | 1453 | `	/* Install superglobals now */` |
|   30890 | 1454 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1455 | `		ph7_value *pSuper;` |
|       - | 1456 | `		/* Request an empty array */` |
|   28082 | 1457 | `		pSuper = ph7_new_array(&(*pVm));` |
|   28082 | 1458 | `		if( pSuper == 0 ){` |
|     ! 0 | 1459 | `			return SXERR_MEM;` |
|       - | 1460 | `		}` |
|       - | 1461 | `		/* Install */` |
|   28082 | 1462 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   28082 | 1463 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1464 | `			return rc;` |
|       - | 1465 | `		}` |
|       - | 1466 | `		/* Release the value now it have been installed */` |
|   28082 | 1467 | `		ph7_release_value(&(*pVm),pSuper);` |
|   14042 | 1468 | `	}` |
|       - | 1469 | `	/* Set some $_SERVER entries */` |
|    2810 | 1470 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1471 | `	/*` |
|       - | 1472 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1473 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1474 | `	 */` |
|    5614 | 1475 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1476 | `		"SCRIPT_FILENAME",` |
|    1404 | 1477 | `		pFile ? pFile->zString : ":Memory:",` |
|    2804 | 1478 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1479 | `		);` |
|       - | 1480 | `	/* All done,all super-global are installed now */` |
|    2810 | 1481 | `	return SXRET_OK;` |
|    1406 | 1482 |  |
|       - | 1483 | `/*` |
|       - | 1484 | ` * Release a hashmap.` |
|       - | 1485 | ` */` |
|   53794 | 1486 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1487 |  |
|       - | 1488 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   53796 | 1489 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1490 | `	sxu32 n;` |
|   53796 | 1491 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1492 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1493 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1494 | `		return SXRET_OK;` |
|       - | 1495 | `	}` |
|       - | 1496 | `	/* Start the release process */` |
|   53796 | 1497 | `	n = 0;` |
|   53796 | 1498 | `	pEntry = pMap->pFirst;` |
| 1533600 | 1499 | `	for(;;){` |
| 3067202 | 1500 | `		if( n >= pMap->nEntry ){` |
|   53796 | 1501 | `			break;` |
|       - | 1502 | `		}` |
| 3013408 | 1503 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1504 | `		/* Remove the reference from the foreign table */` |
| 3013408 | 1505 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3013408 | 1506 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1507 | `			/* Restore the ph7_value to the free list */` |
| 3013400 | 1508 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1506699 | 1509 | `		}` |
|       - | 1510 | `		/* Release the node */` |
| 3013408 | 1511 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   68180 | 1512 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   34089 | 1513 | `		}` |
| 3013408 | 1514 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1515 | `		/* Point to the next entry */` |
| 3013408 | 1516 | `		pEntry = pNext;` |
| 3013408 | 1517 | `		n++;` |
|       2 | 1518 | `	}` |
|   53796 | 1519 | `	if( pMap->nEntry > 0 ){` |
|       - | 1520 | `		/* Release the hash bucket */` |
|   47796 | 1521 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   23897 | 1522 | `	}` |
|   53796 | 1523 | `	if( FreeDS ){` |
|       - | 1524 | `		/* Free the whole instance */` |
|   53780 | 1525 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   26891 | 1526 | `	}else{` |
|       - | 1527 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1528 | `		pMap->apBucket = 0;` |
|      17 | 1529 | `		pMap->iNextIdx = 0;` |
|      17 | 1530 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1531 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1532 | `	}` |
|   53796 | 1533 | `	return SXRET_OK;` |
|   26899 | 1534 |  |
|       - | 1535 | `/*` |
|       - | 1536 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1537 | ` * If the count reaches zero which mean no more variables` |
|       - | 1538 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1539 | ` */` |
|  594538 | 1540 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1541 |  |
|  594540 | 1542 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1543 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  594540 | 1544 | `	pMap->iRef--;` |
|  594540 | 1545 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   53764 | 1546 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   26881 | 1547 | `	}` |
|  594540 | 1548 |  |
|       - | 1549 | `/*` |
|       - | 1550 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1551 | ` * Write a pointer to the target node on success.` |
|       - | 1552 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1553 | ` */` |
|  111670 | 1554 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1555 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1556 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1557 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1558 | `	)` |
|       2 | 1559 |  |
|       - | 1560 | `	sxi32 rc;` |
|  111672 | 1561 | `	if( pMap->nEntry < 1 ){` |
|       - | 1562 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1563 | `		 */` |
|      42 | 1564 | `		return SXERR_NOTFOUND;` |
|       - | 1565 | `	}` |
|  111632 | 1566 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  111632 | 1567 | `	return rc;` |
|   55837 | 1568 |  |
|       - | 1569 | `/*` |
|       - | 1570 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1571 | ` * hashmap.` |
|       - | 1572 | ` * If a node with the given key already exists in the database` |
|       - | 1573 | ` * then this function overwrite the old value.` |
|       - | 1574 | ` */` |
| 2478494 | 1575 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1576 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1577 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1578 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1579 | `	)` |
|       2 | 1580 |  |
|       - | 1581 | `	sxi32 rc;` |
| 2478496 | 1582 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1583 | `		/*` |
|       - | 1584 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1585 | `		 */` |
|     ! 0 | 1586 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1587 | `		return SXRET_OK;` |
|       - | 1588 | `	}` |
| 2478496 | 1589 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2478496 | 1590 | `	return rc;` |
| 1239249 | 1591 |  |
|       - | 1592 | `/*` |
|       - | 1593 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|       - | 1594 | ` *   - String keys overwrite same-key entries in pDest.` |
|       - | 1595 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|       - | 1596 | ` * This is the same routine that backs array_merge().` |
|       - | 1597 | ` */` |
|      46 | 1598 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1599 |  |
|      47 | 1600 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|       1 | 1601 |  |
|       - | 1602 | `/*` |
|       - | 1603 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1604 | ` * hashmap.` |
|       - | 1605 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1606 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1607 | ` * The insertion by reference is triggered when the following` |
|       - | 1608 | ` * expression is encountered.` |
|       - | 1609 | ` * $var = 10;` |
|       - | 1610 | ` *  $a = array(&var);` |
|       - | 1611 | ` * OR` |
|       - | 1612 | ` *  $a[] =& $var;` |
|       - | 1613 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1614 | ` * over it's contents.` |
|       - | 1615 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1616 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1617 | ` * Example:` |
|       - | 1618 | ` *  $var = 10;` |
|       - | 1619 | ` *  $a[] =& $var;` |
|       - | 1620 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1621 | ` *  //Unset the foreign ph7_value now` |
|       - | 1622 | ` *  unset($var);` |
|       - | 1623 | ` *  echo count($a); //0` |
|       - | 1624 | ` * Note that this is a PH7 eXtension.` |
|       - | 1625 | ` * Refer to the official documentation for more information.` |
|       - | 1626 | ` * If a node with the given key already exists in the database` |
|       - | 1627 | ` * then this function overwrite the old value.` |
|       - | 1628 | ` */` |
|   35102 | 1629 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1630 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1631 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1632 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1633 | `	)` |
|       2 | 1634 |  |
|       - | 1635 | `	sxi32 rc;` |
|   35104 | 1636 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1637 | `		/*` |
|       - | 1638 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1639 | `		 */` |
|     ! 0 | 1640 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1641 | `		return SXRET_OK;` |
|       - | 1642 | `	}` |
|   35104 | 1643 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   35104 | 1644 | `	return rc;` |
|   17553 | 1645 |  |
|       - | 1646 | `/*` |
|       - | 1647 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1648 | ` */` |
|   24126 | 1649 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1650 |  |
|       - | 1651 | `	/* Reset the loop cursor */` |
|   24128 | 1652 | `	pMap->pCur = pMap->pFirst;` |
|   24128 | 1653 |  |
|       - | 1654 | `/*` |
|       - | 1655 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1656 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1657 | ` * return NULL.` |
|       - | 1658 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1659 | ` */` |
|  198286 | 1660 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1661 |  |
|  198288 | 1662 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  198288 | 1663 | `	if( pCur == 0 ){` |
|       - | 1664 | `		/* End of the list,return null */` |
|   12084 | 1665 | `		return 0;` |
|       - | 1666 | `	}` |
|       - | 1667 | `	/* Advance the node cursor */` |
|  186206 | 1668 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  186206 | 1669 | `	return pCur;` |
|   99145 | 1670 |  |
|       - | 1671 | `/*` |
|       - | 1672 | ` * Extract a node value.` |
|       - | 1673 | ` */` |
|  464148 | 1674 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1675 |  |
|  464150 | 1676 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  464150 | 1677 | `	if( pEntry ){` |
|  464150 | 1678 | `		if( bStore ){` |
|  186344 | 1679 | `			PH7_MemObjStore(pEntry,pValue);` |
|   93173 | 1680 | `		}else{` |
|  277808 | 1681 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1682 | `		}` |
|  232081 | 1683 | `	}else{` |
|     ! 0 | 1684 | `		PH7_MemObjRelease(pValue);` |
|       - | 1685 | `	}` |
|  464150 | 1686 |  |
|       - | 1687 | `/*` |
|       - | 1688 | ` * Extract a node key.` |
|       - | 1689 | ` */` |
|  117428 | 1690 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1691 |  |
|       - | 1692 | `	/* Fill with the current key */` |
|  117430 | 1693 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  117110 | 1694 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1695 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1696 | `		}` |
|  117110 | 1697 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  117110 | 1698 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   58556 | 1699 | `	}else{` |
|     322 | 1700 | `		SyBlobReset(&pKey->sBlob);` |
|     322 | 1701 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     322 | 1702 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1703 | `	}` |
|  117430 | 1704 |  |
|       - | 1705 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1706 | `/*` |
|       - | 1707 | ` * Store the address of nodes value in the given container.` |
|       - | 1708 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1709 | ` * defined in 'builtin.c' for more information.` |
|       - | 1710 | ` */` |
|      10 | 1711 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1712 |  |
|      11 | 1713 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1714 | `	ph7_value *pValue;` |
|       - | 1715 | `	sxu32 n;` |
|       - | 1716 | `	/* Initialize the container */` |
|      11 | 1717 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1718 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1719 | `		/* Extract node value */` |
|      17 | 1720 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1721 | `		if( pValue ){` |
|      17 | 1722 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1723 | `		}` |
|       - | 1724 | `		/* Point to the next entry */` |
|      17 | 1725 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1726 | `	}` |
|       - | 1727 | `	/* Total inserted entries */` |
|      11 | 1728 | `	return (int)SySetUsed(pOut);` |
|       1 | 1729 |  |
|       - | 1730 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1731 | `/*` |
|       - | 1732 | ` * Merge sort.` |
|       - | 1733 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1734 | ` * Status: Public domain` |
|       - | 1735 | ` */` |
|       - | 1736 | `/* Node comparison callback signature */` |
|       - | 1737 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1738 | `/*` |
|       - | 1739 | `** Inputs:` |
|       - | 1740 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1741 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1742 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1743 | `**` |
|       - | 1744 | `** Return Value:` |
|       - | 1745 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1746 | `**   of both a and b.` |
|       - | 1747 | `**` |
|       - | 1748 | `** Side effects:` |
|       - | 1749 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1750 | `**   changed.` |
|       - | 1751 | `*/` |
|   30846 | 1752 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1753 |  |
|       - | 1754 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1755 | `    /* Prevent compiler warning */` |
|   30848 | 1756 | `	result.pNext = result.pPrev = 0;` |
|   30848 | 1757 | `	pTail = &result;` |
|   86737 | 1758 | `	while( pA && pB ){` |
|   55891 | 1759 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   36153 | 1760 | `			pTail->pPrev = pA;` |
|   36153 | 1761 | `			pA->pNext = pTail;` |
|   36153 | 1762 | `			pTail = pA;` |
|   36153 | 1763 | `			pA = pA->pPrev;` |
|   18055 | 1764 | `		}else{` |
|   19740 | 1765 | `			pTail->pPrev = pB;` |
|   19740 | 1766 | `			pB->pNext = pTail;` |
|   19740 | 1767 | `			pTail = pB;` |
|   19740 | 1768 | `			pB = pB->pPrev;` |
|       - | 1769 | `		}` |
|       2 | 1770 | `	}` |
|   30848 | 1771 | `	if( pA ){` |
|   22017 | 1772 | `		pTail->pPrev = pA;` |
|   22017 | 1773 | `		pA->pNext = pTail;` |
|   19854 | 1774 | `	}else if( pB ){` |
|    8617 | 1775 | `		pTail->pPrev = pB;` |
|    8617 | 1776 | `		pB->pNext = pTail;` |
|    4296 | 1777 | `	}else{` |
|     218 | 1778 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1779 | `	}` |
|   30848 | 1780 | `	return result.pPrev;` |
|       2 | 1781 |  |
|       - | 1782 | `/*` |
|       - | 1783 | `** Inputs:` |
|       - | 1784 | `**   Map:       Input hashmap` |
|       - | 1785 | `**   cmp:       A comparison function.` |
|       - | 1786 | `**` |
|       - | 1787 | `** Return Value:` |
|       - | 1788 | `**   Sorted hashmap.` |
|       - | 1789 | `**` |
|       - | 1790 | `** Side effects:` |
|       - | 1791 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1792 | `*/` |
|       - | 1793 | `#define N_SORT_BUCKET  32` |
|     654 | 1794 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1795 |  |
|       - | 1796 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1797 | `	sxu32 i;` |
|     656 | 1798 | `	SyZero(a,sizeof(a));` |
|       - | 1799 | `	/* Point to the first inserted entry */` |
|     656 | 1800 | `	pIn = pMap->pFirst;` |
|   12386 | 1801 | `	while( pIn ){` |
|   11732 | 1802 | `		p = pIn;` |
|   11732 | 1803 | `		pIn = p->pPrev;` |
|   11732 | 1804 | `		p->pPrev = 0;` |
|   22304 | 1805 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   22304 | 1806 | `			if( a[i]==0 ){` |
|   11732 | 1807 | `				a[i] = p;` |
|   11732 | 1808 | `				break;` |
|     ! 0 | 1809 | `			}else{` |
|   10574 | 1810 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   10574 | 1811 | `				a[i] = 0;` |
|       - | 1812 | `			}` |
|    5288 | 1813 | `		}` |
|   11732 | 1814 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1815 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1816 | `			 * But that is impossible.` |
|       - | 1817 | `			 */` |
|     ! 0 | 1818 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1819 | `		}` |
|       2 | 1820 | `	}` |
|     656 | 1821 | `	p = a[0];` |
|   20930 | 1822 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   20276 | 1823 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   10139 | 1824 | `	}` |
|     656 | 1825 | `	p->pNext = 0;` |
|       - | 1826 | `	/* Reflect the change */` |
|     656 | 1827 | `	pMap->pFirst = p;` |
|       - | 1828 | `	/* Reset the loop cursor */` |
|     656 | 1829 | `	pMap->pCur = pMap->pFirst;` |
|     656 | 1830 | `	return SXRET_OK;` |
|       2 | 1831 |  |
|       - | 1832 | `/*` |
|       - | 1833 | ` * Node comparison callback.` |
|       - | 1834 | ` * used-by: [sort(),asort(),...]` |
|       - | 1835 | ` */` |
|   55701 | 1836 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1837 |  |
|       - | 1838 | `	ph7_value sA,sB;` |
|       - | 1839 | `	sxi32 iFlags;` |
|       - | 1840 | `	int rc;` |
|   55703 | 1841 | `	if( pCmpData == 0 ){` |
|       - | 1842 | `		/* Perform a standard comparison */` |
|   55679 | 1843 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   55679 | 1844 | `		return rc;` |
|       - | 1845 | `	}` |
|      25 | 1846 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1847 | `	/* Duplicate node values */` |
|      25 | 1848 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 1849 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 1850 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 1851 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 1852 | `	if( iFlags == 5 ){` |
|       - | 1853 | `		/* String cast */` |
|       - | 1854 | `		const char *zA,*zB;` |
|       - | 1855 | `		sxu32 nA,nB,nMin;` |
|      15 | 1856 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1857 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1858 | `		}` |
|      15 | 1859 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1860 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1861 | `		}` |
|       - | 1862 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 1863 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 1864 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 1865 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 1866 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 1867 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 1868 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 1869 | `		if( rc == 0 ){` |
|       5 | 1870 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1871 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1872 | `		}` |
|       8 | 1873 | `	}else{` |
|       - | 1874 | `		/* Numeric cast */` |
|      11 | 1875 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1876 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1877 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1878 | `	}` |
|      25 | 1879 | `	PH7_MemObjRelease(&sA);` |
|      25 | 1880 | `	PH7_MemObjRelease(&sB);` |
|      25 | 1881 | `	return rc;` |
|   27855 | 1882 |  |
|       - | 1883 | `/*` |
|       - | 1884 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1885 | ` * used-by: [ksort()]` |
|       - | 1886 | ` */` |
|      14 | 1887 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1888 |  |
|       - | 1889 | `	sxi32 rc;` |
|       7 | 1890 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 1891 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1892 | `		/* Perform a string comparison */` |
|       5 | 1893 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1894 | `	}else{` |
|       - | 1895 | `		SyString sStr;` |
|       - | 1896 | `		sxi64 iA,iB;` |
|       - | 1897 | `		/* Perform a numeric comparison */` |
|      11 | 1898 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1899 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1900 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1901 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1902 | `				iA = 0;` |
|     ! 0 | 1903 | `			}else{` |
|     ! 0 | 1904 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1905 | `			}` |
|     ! 0 | 1906 | `		}else{` |
|      11 | 1907 | `			iA = pA->xKey.iKey;` |
|       - | 1908 | `		}` |
|      11 | 1909 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1910 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1911 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1912 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1913 | `				iB = 0;` |
|     ! 0 | 1914 | `			}else{` |
|     ! 0 | 1915 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1916 | `			}` |
|     ! 0 | 1917 | `		}else{` |
|      11 | 1918 | `			iB = pB->xKey.iKey;` |
|       - | 1919 | `		}` |
|      11 | 1920 | `		rc = (sxi32)(iA-iB);` |
|       - | 1921 | `	}` |
|       - | 1922 | `	/* Comparison result */` |
|      15 | 1923 | `	return rc;` |
|       1 | 1924 |  |
|       - | 1925 | `/*` |
|       - | 1926 | ` * Node comparison callback.` |
|       - | 1927 | ` * Used by: [rsort(),arsort()];` |
|       - | 1928 | ` */` |
|      78 | 1929 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1930 |  |
|       - | 1931 | `	ph7_value sA,sB;` |
|       - | 1932 | `	sxi32 iFlags;` |
|       - | 1933 | `	int rc;` |
|      79 | 1934 | `	if( pCmpData == 0 ){` |
|       - | 1935 | `		/* Perform a standard comparison */` |
|      59 | 1936 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 1937 | `		return -rc;` |
|       - | 1938 | `	}` |
|      21 | 1939 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1940 | `	/* Duplicate node values */` |
|      21 | 1941 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 1942 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 1943 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 1944 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 1945 | `	if( iFlags == 5 ){` |
|       - | 1946 | `		/* String cast */` |
|       - | 1947 | `		const char *zA,*zB;` |
|       - | 1948 | `		sxu32 nA,nB,nMin;` |
|      11 | 1949 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1950 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1951 | `		}` |
|      11 | 1952 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1953 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1954 | `		}` |
|       - | 1955 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 1956 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 1957 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 1958 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 1959 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 1960 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 1961 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 1962 | `		if( rc == 0 ){` |
|       3 | 1963 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1964 | `			else if( nA > nB ) rc = 1;` |
|       1 | 1965 | `		}` |
|       6 | 1966 | `	}else{` |
|       - | 1967 | `		/* Numeric cast */` |
|      11 | 1968 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1969 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1970 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1971 | `	}` |
|      21 | 1972 | `	PH7_MemObjRelease(&sA);` |
|      21 | 1973 | `	PH7_MemObjRelease(&sB);` |
|      21 | 1974 | `	return -rc;` |
|      40 | 1975 |  |
|       - | 1976 | `/*` |
|       - | 1977 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1978 | ` * used-by: [usort(),uasort()]` |
|       - | 1979 | ` */` |
|      74 | 1980 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1981 |  |
|       - | 1982 | `	ph7_value sResult,*pCallback;` |
|       - | 1983 | `	ph7_value *pV1,*pV2;` |
|       - | 1984 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1985 | `	sxi32 rc;` |
|       - | 1986 | `	/* Point to the desired callback */` |
|      76 | 1987 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1988 | `	/* initialize the result value */` |
|      76 | 1989 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 1990 | `	/* Extract nodes values */` |
|      76 | 1991 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      76 | 1992 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      76 | 1993 | `	apArg[0] = pV1;` |
|      76 | 1994 | `	apArg[1] = pV2;` |
|       - | 1995 | `	/* Invoke the callback */` |
|      76 | 1996 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      76 | 1997 | `	if( rc != SXRET_OK ){` |
|       - | 1998 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1999 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2000 | `	}else{` |
|       - | 2001 | `		/* Extract callback result */` |
|      76 | 2002 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2003 | `			/* Perform an int cast */` |
|     ! 0 | 2004 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2005 | `		}` |
|      76 | 2006 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2007 | `	}` |
|      76 | 2008 | `	PH7_MemObjRelease(&sResult);` |
|       - | 2009 | `	/* Callback result */` |
|      76 | 2010 | `	return rc;` |
|       2 | 2011 |  |
|       - | 2012 | `/*` |
|       - | 2013 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2014 | ` * used-by: [krsort()]` |
|       - | 2015 | ` */` |
|       4 | 2016 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2017 |  |
|       - | 2018 | `	sxi32 rc;` |
|       2 | 2019 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2020 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2021 | `		/* Perform a string comparison */` |
|       5 | 2022 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2023 | `	}else{` |
|       - | 2024 | `		SyString sStr;` |
|       - | 2025 | `		sxi64 iA,iB;` |
|       - | 2026 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2027 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2028 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2029 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2030 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2031 | `				iA = 0;` |
|     ! 0 | 2032 | `			}else{` |
|     ! 0 | 2033 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2034 | `			}` |
|     ! 0 | 2035 | `		}else{` |
|     ! 0 | 2036 | `			iA = pA->xKey.iKey;` |
|       - | 2037 | `		}` |
|     ! 0 | 2038 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2039 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2040 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2041 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2042 | `				iB = 0;` |
|     ! 0 | 2043 | `			}else{` |
|     ! 0 | 2044 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2045 | `			}` |
|     ! 0 | 2046 | `		}else{` |
|     ! 0 | 2047 | `			iB = pB->xKey.iKey;` |
|       - | 2048 | `		}` |
|     ! 0 | 2049 | `		rc = (sxi32)(iA-iB);` |
|       - | 2050 | `	}` |
|       5 | 2051 | `	return -rc; /* Reverse result */` |
|       1 | 2052 |  |
|       - | 2053 | `/*` |
|       - | 2054 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2055 | ` * used-by: [uksort()]` |
|       - | 2056 | ` */` |
|       6 | 2057 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2058 |  |
|       - | 2059 | `	ph7_value sResult,*pCallback;` |
|       - | 2060 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2061 | `	ph7_value sK1,sK2;` |
|       - | 2062 | `	sxi32 rc;` |
|       - | 2063 | `	/* Point to the desired callback */` |
|       7 | 2064 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 2065 | `	/* initialize the result value */` |
|       7 | 2066 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2067 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2068 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2069 | `	/* Extract nodes keys */` |
|       7 | 2070 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2071 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2072 | `	apArg[0] = &sK1;` |
|       7 | 2073 | `	apArg[1] = &sK2;` |
|       - | 2074 | `	/* Mark keys as constants */` |
|       7 | 2075 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2076 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2077 | `	/* Invoke the callback */` |
|       7 | 2078 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2079 | `	if( rc != SXRET_OK ){` |
|       - | 2080 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2081 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2082 | `	}else{` |
|       - | 2083 | `		/* Extract callback result */` |
|       7 | 2084 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2085 | `			/* Perform an int cast */` |
|     ! 0 | 2086 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2087 | `		}` |
|       7 | 2088 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2089 | `	}` |
|       7 | 2090 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2091 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2092 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2093 | `	/* Callback result */` |
|       7 | 2094 | `	return rc;` |
|       1 | 2095 |  |
|       - | 2096 | `/*` |
|       - | 2097 | ` * Node comparison callback: Random node comparison.` |
|       - | 2098 | ` * used-by: [shuffle()]` |
|       - | 2099 | ` */` |
|      12 | 2100 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2101 |  |
|       - | 2102 | `	sxu32 n;` |
|       6 | 2103 | `	SXUNUSED(pB); /* cc warning */` |
|       6 | 2104 | `	SXUNUSED(pCmpData);` |
|       - | 2105 | `	/* Grab a random number */` |
|      13 | 2106 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2107 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2108 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2109 | `	 */` |
|      13 | 2110 | `	return n&1 ? 1 : -1;` |
|       1 | 2111 |  |
|       - | 2112 | `/*` |
|       - | 2113 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2114 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2115 | ` */` |
|     606 | 2116 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2117 |  |
|       - | 2118 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2119 | `	sxu32 i;` |
|       - | 2120 | `	/* Rehash all entries */` |
|     608 | 2121 | `	pLast = p = pMap->pFirst;` |
|     608 | 2122 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     608 | 2123 | `	i = 0;` |
|    6083 | 2124 | `	for( ;; ){` |
|   12168 | 2125 | `		if( i >= pMap->nEntry ){` |
|     608 | 2126 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     608 | 2127 | `			break;` |
|       - | 2128 | `		}` |
|   11562 | 2129 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2130 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2131 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2132 | `			/* Change key type */` |
|       5 | 2133 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2134 | `		}` |
|   11562 | 2135 | `		HashmapRehashIntNode(p);` |
|       - | 2136 | `		/* Point to the next entry */` |
|   11562 | 2137 | `		i++;` |
|   11562 | 2138 | `		pLast = p;` |
|   11562 | 2139 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2140 | `	}` |
|     608 | 2141 |  |
|       - | 2142 | `/*` |
|       - | 2143 | ` * Array functions implementation.` |
|       - | 2144 | ` * Status:` |
|       - | 2145 | ` *  Stable.` |
|       - | 2146 | ` */` |
|       - | 2147 | `/*` |
|       - | 2148 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2149 | ` * Sort an array.` |
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
|       - | 2161 | ` *` |
|       - | 2162 | ` */` |
|     922 | 2163 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2164 |  |
|       - | 2165 | `	ph7_hashmap *pMap;` |
|       - | 2166 | `	/* Make sure we are dealing with a valid hashmap */` |
|     924 | 2167 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2168 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2169 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2170 | `		return PH7_OK;` |
|       - | 2171 | `	}` |
|       - | 2172 | `	/* Point to the internal representation of the input hashmap */` |
|     924 | 2173 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     924 | 2174 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     924 | 2175 | `	if( pMap->nEntry > 1 ){` |
|     598 | 2176 | `		sxi32 iCmpFlags = 0;` |
|     598 | 2177 | `		if( nArg > 1 ){` |
|       - | 2178 | `			/* Extract comparison flags */` |
|       3 | 2179 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2180 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2181 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2182 | `			}` |
|       1 | 2183 | `		}` |
|       - | 2184 | `		/* Do the merge sort */` |
|     598 | 2185 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2186 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     598 | 2187 | `		HashmapSortRehash(pMap);` |
|     298 | 2188 | `	}` |
|       - | 2189 | `	/* All done,return TRUE */` |
|     924 | 2190 | `	ph7_result_bool(pCtx,1);` |
|     924 | 2191 | `	return PH7_OK;` |
|     463 | 2192 |  |
|       - | 2193 | `/*` |
|       - | 2194 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2195 | ` *  Sort an array and maintain index association.` |
|       - | 2196 | ` * Parameters` |
|       - | 2197 | ` *  $array` |
|       - | 2198 | ` *   The input array.` |
|       - | 2199 | ` * $sort_flags` |
|       - | 2200 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2201 | ` *  Sorting type flags:` |
|       - | 2202 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2203 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2204 | ` *   SORT_STRING - compare items as strings` |
|       - | 2205 | ` * Return` |
|       - | 2206 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2207 | ` */` |
|      32 | 2208 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2209 |  |
|       - | 2210 | `	ph7_hashmap *pMap;` |
|       - | 2211 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2212 | `	if( nArg < 1 ){` |
|       3 | 2213 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2214 | `			"ArgumentCountError",` |
|       - | 2215 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2216 | `			);` |
|       - | 2217 | `	}` |
|       - | 2218 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2219 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2220 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2221 | `			"TypeError",` |
|       - | 2222 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2223 | `			ph7_type_name(apArg[0])` |
|       - | 2224 | `			);` |
|       - | 2225 | `	}` |
|       - | 2226 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2227 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2228 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2229 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2230 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2231 | `		if( nArg > 1 ){` |
|       - | 2232 | `			/* Extract comparison flags */` |
|       5 | 2233 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2234 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2235 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2236 | `			}` |
|       2 | 2237 | `		}` |
|       - | 2238 | `		/* Do the merge sort */` |
|      19 | 2239 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2240 | `		/* Fix the last link broken by the merge */` |
|      45 | 2241 | `		while(pMap->pLast->pPrev){` |
|      27 | 2242 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2243 | `		}` |
|       9 | 2244 | `	}` |
|       - | 2245 | `	/* All done,return TRUE */` |
|      23 | 2246 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2247 | `	return PH7_OK;` |
|      18 | 2248 |  |
|       - | 2249 | `/*` |
|       - | 2250 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2251 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2252 | ` * Parameters` |
|       - | 2253 | ` *  $array` |
|       - | 2254 | ` *   The input array.` |
|       - | 2255 | ` * $sort_flags` |
|       - | 2256 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2257 | ` *  Sorting type flags:` |
|       - | 2258 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2259 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2260 | ` *   SORT_STRING - compare items as strings` |
|       - | 2261 | ` * Return` |
|       - | 2262 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2263 | ` */` |
|      32 | 2264 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2265 |  |
|       - | 2266 | `	ph7_hashmap *pMap;` |
|       - | 2267 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2268 | `	if( nArg < 1 ){` |
|       3 | 2269 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2270 | `			"ArgumentCountError",` |
|       - | 2271 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2272 | `			);` |
|       - | 2273 | `	}` |
|       - | 2274 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2275 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2276 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2277 | `			"TypeError",` |
|       - | 2278 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2279 | `			ph7_type_name(apArg[0])` |
|       - | 2280 | `			);` |
|       - | 2281 | `	}` |
|       - | 2282 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2283 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2284 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2285 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2286 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2287 | `		if( nArg > 1 ){` |
|       - | 2288 | `			/* Extract comparison flags */` |
|       5 | 2289 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2290 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2291 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2292 | `			}` |
|       2 | 2293 | `		}` |
|       - | 2294 | `		/* Do the merge sort */` |
|      19 | 2295 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2296 | `		/* Fix the last link broken by the merge */` |
|      35 | 2297 | `		while(pMap->pLast->pPrev){` |
|      17 | 2298 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2299 | `		}` |
|       9 | 2300 | `	}` |
|       - | 2301 | `	/* All done,return TRUE */` |
|      23 | 2302 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2303 | `	return PH7_OK;` |
|      18 | 2304 |  |
|       - | 2305 | `/*` |
|       - | 2306 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2307 | ` *  Sort an array by key.` |
|       - | 2308 | ` * Parameters` |
|       - | 2309 | ` *  $array` |
|       - | 2310 | ` *   The input array.` |
|       - | 2311 | ` * $sort_flags` |
|       - | 2312 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2313 | ` *  Sorting type flags:` |
|       - | 2314 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2315 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2316 | ` *   SORT_STRING - compare items as strings` |
|       - | 2317 | ` * Return` |
|       - | 2318 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2319 | ` */` |
|       4 | 2320 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2321 |  |
|       - | 2322 | `	ph7_hashmap *pMap;` |
|       - | 2323 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2324 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2325 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2326 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2327 | `		return PH7_OK;` |
|       - | 2328 | `	}` |
|       - | 2329 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2330 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2331 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2332 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2333 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2334 | `		if( nArg > 1 ){` |
|       - | 2335 | `			/* Extract comparison flags */` |
|     ! 0 | 2336 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2337 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2338 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2339 | `			}` |
|     ! 0 | 2340 | `		}` |
|       - | 2341 | `		/* Do the merge sort */` |
|       5 | 2342 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2343 | `		/* Fix the last link broken by the merge */` |
|      15 | 2344 | `		while(pMap->pLast->pPrev){` |
|      11 | 2345 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2346 | `		}` |
|       2 | 2347 | `	}` |
|       - | 2348 | `	/* All done,return TRUE */` |
|       5 | 2349 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2350 | `	return PH7_OK;` |
|       3 | 2351 |  |
|       - | 2352 | `/*` |
|       - | 2353 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2354 | ` *  Sort an array by key in reverse order.` |
|       - | 2355 | ` * Parameters` |
|       - | 2356 | ` *  $array` |
|       - | 2357 | ` *   The input array.` |
|       - | 2358 | ` * $sort_flags` |
|       - | 2359 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2360 | ` *  Sorting type flags:` |
|       - | 2361 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2362 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2363 | ` *   SORT_STRING - compare items as strings` |
|       - | 2364 | ` * Return` |
|       - | 2365 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2366 | ` */` |
|       2 | 2367 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2368 |  |
|       - | 2369 | `	ph7_hashmap *pMap;` |
|       - | 2370 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2371 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2372 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2373 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2374 | `		return PH7_OK;` |
|       - | 2375 | `	}` |
|       - | 2376 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2377 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2378 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2379 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2380 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2381 | `		if( nArg > 1 ){` |
|       - | 2382 | `			/* Extract comparison flags */` |
|     ! 0 | 2383 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2384 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2385 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2386 | `			}` |
|     ! 0 | 2387 | `		}` |
|       - | 2388 | `		/* Do the merge sort */` |
|       3 | 2389 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2390 | `		/* Fix the last link broken by the merge */` |
|       7 | 2391 | `		while(pMap->pLast->pPrev){` |
|       5 | 2392 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2393 | `		}` |
|       1 | 2394 | `	}` |
|       - | 2395 | `	/* All done,return TRUE */` |
|       3 | 2396 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2397 | `	return PH7_OK;` |
|       2 | 2398 |  |
|       - | 2399 | `/*` |
|       - | 2400 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2401 | ` * Sort an array in reverse order.` |
|       - | 2402 | ` * Parameters` |
|       - | 2403 | ` *  $array` |
|       - | 2404 | ` *   The input array.` |
|       - | 2405 | ` * $sort_flags` |
|       - | 2406 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2407 | ` *  Sorting type flags:` |
|       - | 2408 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2409 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2410 | ` *   SORT_STRING - compare items as strings` |
|       - | 2411 | ` * Return` |
|       - | 2412 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2413 | ` */` |
|       2 | 2414 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2415 |  |
|       - | 2416 | `	ph7_hashmap *pMap;` |
|       - | 2417 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2418 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2419 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2420 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2421 | `		return PH7_OK;` |
|       - | 2422 | `	}` |
|       - | 2423 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2424 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2425 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2426 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2427 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2428 | `		if( nArg > 1 ){` |
|       - | 2429 | `			/* Extract comparison flags */` |
|     ! 0 | 2430 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2431 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2432 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2433 | `			}` |
|     ! 0 | 2434 | `		}` |
|       - | 2435 | `		/* Do the merge sort */` |
|       3 | 2436 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2437 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2438 | `		HashmapSortRehash(pMap);` |
|       1 | 2439 | `	}` |
|       - | 2440 | `	/* All done,return TRUE */` |
|       3 | 2441 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2442 | `	return PH7_OK;` |
|       2 | 2443 |  |
|       - | 2444 | `/*` |
|       - | 2445 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2446 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2447 | ` * Parameters` |
|       - | 2448 | ` *  $array` |
|       - | 2449 | ` *   The input array.` |
|       - | 2450 | ` * $cmp_function` |
|       - | 2451 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2452 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2453 | ` *  to, or greater than the second.` |
|       - | 2454 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2455 | ` * Return` |
|       - | 2456 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2457 | ` */` |
|       6 | 2458 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2459 |  |
|       - | 2460 | `	ph7_hashmap *pMap;` |
|       - | 2461 | `	/* Make sure we are dealing with a valid hashmap */` |
|       8 | 2462 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2463 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2464 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2465 | `		return PH7_OK;` |
|       - | 2466 | `	}` |
|       - | 2467 | `	/* Point to the internal representation of the input hashmap */` |
|       8 | 2468 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       8 | 2469 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       8 | 2470 | `	if( pMap->nEntry > 1 ){` |
|       8 | 2471 | `		ph7_value *pCallback = 0;` |
|       - | 2472 | `		ProcNodeCmp xCmp;` |
|       8 | 2473 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       8 | 2474 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2475 | `			/* Point to the desired callback */` |
|       8 | 2476 | `			pCallback = apArg[1];` |
|       5 | 2477 | `		}else{` |
|       - | 2478 | `			/* Use the default comparison function */` |
|     ! 0 | 2479 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2480 | `		}` |
|       - | 2481 | `		/* Do the merge sort */` |
|       8 | 2482 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2483 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       8 | 2484 | `		HashmapSortRehash(pMap);` |
|       3 | 2485 | `	}` |
|       - | 2486 | `	/* All done,return TRUE */` |
|       8 | 2487 | `	ph7_result_bool(pCtx,1);` |
|       8 | 2488 | `	return PH7_OK;` |
|       5 | 2489 |  |
|       - | 2490 | `/*` |
|       - | 2491 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2492 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2493 | ` *  and maintain index association.` |
|       - | 2494 | ` * Parameters` |
|       - | 2495 | ` *  $array` |
|       - | 2496 | ` *   The input array.` |
|       - | 2497 | ` * $cmp_function` |
|       - | 2498 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2499 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2500 | ` *  to, or greater than the second.` |
|       - | 2501 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2502 | ` * Return` |
|       - | 2503 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2504 | ` */` |
|       2 | 2505 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2506 |  |
|       - | 2507 | `	ph7_hashmap *pMap;` |
|       - | 2508 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2509 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2510 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2511 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2512 | `		return PH7_OK;` |
|       - | 2513 | `	}` |
|       - | 2514 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2515 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2516 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2517 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2518 | `		ph7_value *pCallback = 0;` |
|       - | 2519 | `		ProcNodeCmp xCmp;` |
|       3 | 2520 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2521 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2522 | `			/* Point to the desired callback */` |
|       3 | 2523 | `			pCallback = apArg[1];` |
|       2 | 2524 | `		}else{` |
|       - | 2525 | `			/* Use the default comparison function */` |
|     ! 0 | 2526 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2527 | `		}` |
|       - | 2528 | `		/* Do the merge sort */` |
|       3 | 2529 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2530 | `		/* Fix the last link broken by the merge */` |
|       5 | 2531 | `		while(pMap->pLast->pPrev){` |
|       3 | 2532 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2533 | `		}` |
|       1 | 2534 | `	}` |
|       - | 2535 | `	/* All done,return TRUE */` |
|       3 | 2536 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2537 | `	return PH7_OK;` |
|       2 | 2538 |  |
|       - | 2539 | `/*` |
|       - | 2540 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2541 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2542 | ` *  function and maintain index association.` |
|       - | 2543 | ` * Parameters` |
|       - | 2544 | ` *  $array` |
|       - | 2545 | ` *   The input array.` |
|       - | 2546 | ` * $cmp_function` |
|       - | 2547 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2548 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2549 | ` *  to, or greater than the second.` |
|       - | 2550 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2551 | ` * Return` |
|       - | 2552 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2553 | ` */` |
|       2 | 2554 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2555 |  |
|       - | 2556 | `	ph7_hashmap *pMap;` |
|       - | 2557 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2558 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2559 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2560 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2561 | `		return PH7_OK;` |
|       - | 2562 | `	}` |
|       - | 2563 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2564 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2565 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2566 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2567 | `		ph7_value *pCallback = 0;` |
|       - | 2568 | `		ProcNodeCmp xCmp;` |
|       3 | 2569 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2570 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2571 | `			/* Point to the desired callback */` |
|       3 | 2572 | `			pCallback = apArg[1];` |
|       2 | 2573 | `		}else{` |
|       - | 2574 | `			/* Use the default comparison function */` |
|     ! 0 | 2575 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2576 | `		}` |
|       - | 2577 | `		/* Do the merge sort */` |
|       3 | 2578 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2579 | `		/* Fix the last link broken by the merge */` |
|       3 | 2580 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2581 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2582 | `		}` |
|       1 | 2583 | `	}` |
|       - | 2584 | `	/* All done,return TRUE */` |
|       3 | 2585 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2586 | `	return PH7_OK;` |
|       2 | 2587 |  |
|       - | 2588 | `/*` |
|       - | 2589 | ` * bool shuffle(array &$array)` |
|       - | 2590 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2591 | ` * Parameters` |
|       - | 2592 | ` *  $array` |
|       - | 2593 | ` *   The input array.` |
|       - | 2594 | ` * Return` |
|       - | 2595 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2596 | ` *` |
|       - | 2597 | ` */` |
|       2 | 2598 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2599 |  |
|       - | 2600 | `	ph7_hashmap *pMap;` |
|       - | 2601 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2602 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2603 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2604 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2605 | `		return PH7_OK;` |
|       - | 2606 | `	}` |
|       - | 2607 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2608 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2609 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2610 | `	if( pMap->nEntry > 1 ){` |
|       - | 2611 | `		/* Do the merge sort */` |
|       3 | 2612 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2613 | `		/* Fix the last link broken by the merge */` |
|      11 | 2614 | `		while(pMap->pLast->pPrev){` |
|       9 | 2615 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2616 | `		}` |
|       1 | 2617 | `	}` |
|       - | 2618 | `	/* All done,return TRUE */` |
|       3 | 2619 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2620 | `	return PH7_OK;` |
|       2 | 2621 |  |
|       - | 2622 | `/*` |
|       - | 2623 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2624 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2625 | ` * Parameters` |
|       - | 2626 | ` *  $var` |
|       - | 2627 | ` *   The array or the object.` |
|       - | 2628 | ` * $mode` |
|       - | 2629 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2630 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2631 | ` *  all the elements of a multidimensional array.` |
|       - | 2632 | ` * Return` |
|       - | 2633 | ` *  Returns the number of elements in the array.` |
|       - | 2634 | ` */` |
|     784 | 2635 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2636 |  |
|     786 | 2637 | `	int bRecursive = FALSE;` |
|     786 | 2638 | `	int bCycleDetected = FALSE;` |
|       - | 2639 | `	sxi64 iCount;` |
|     786 | 2640 | `	if( nArg < 1 ){` |
|       3 | 2641 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2642 | `			"ArgumentCountError",` |
|       - | 2643 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2644 | `			);` |
|       - | 2645 | `	}` |
|     784 | 2646 | `	if( nArg > 2 ){` |
|       4 | 2647 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2648 | `			"ArgumentCountError",` |
|       - | 2649 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2650 | `			nArg` |
|       - | 2651 | `			);` |
|       - | 2652 | `	}` |
|       - | 2653 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - | 2654 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - | 2655 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|     782 | 2656 | `	if( nArg > 1 ){` |
|      42 | 2657 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      42 | 2658 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       9 | 2659 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2660 | `				"ValueError",` |
|       - | 2661 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2662 | `				);` |
|       - | 2663 | `		}` |
|      34 | 2664 | `		bRecursive = iMode == 1;` |
|      16 | 2665 | `	}` |
|     774 | 2666 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2667 | `		/* Countable object: dispatch to ->count() */` |
|      28 | 2668 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      18 | 2669 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      18 | 2670 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      18 | 2671 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      16 | 2672 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2673 | `					"count",sizeof("count")-1);` |
|      16 | 2674 | `				if( pMeth ){` |
|       - | 2675 | `					ph7_value sResult;` |
|      16 | 2676 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      16 | 2677 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      16 | 2678 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      16 | 2679 | `					PH7_MemObjRelease(&sResult);` |
|      16 | 2680 | `					return PH7_OK;` |
|       - | 2681 | `				}` |
|     ! 0 | 2682 | `			}` |
|       1 | 2683 | `		}` |
|      19 | 2684 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2685 | `			"TypeError",` |
|       - | 2686 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       6 | 2687 | `			ph7_type_name(apArg[0])` |
|       - | 2688 | `			);` |
|       - | 2689 | `	}` |
|       - | 2690 | `	/* Count */` |
|     748 | 2691 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     748 | 2692 | `	if( bCycleDetected ){` |
|       3 | 2693 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2694 | `	}` |
|     748 | 2695 | `	ph7_result_int64(pCtx,iCount);` |
|     748 | 2696 | `	return PH7_OK;` |
|     394 | 2697 |  |
|       - | 2698 | `/*` |
|       - | 2699 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2700 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2701 | ` * Parameters` |
|       - | 2702 | ` * $key` |
|       - | 2703 | ` *   Value to check.` |
|       - | 2704 | ` * $search` |
|       - | 2705 | ` *  An array with keys to check.` |
|       - | 2706 | ` * Return` |
|       - | 2707 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2708 | ` */` |
|      82 | 2709 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2710 |  |
|       - | 2711 | `	sxi32 rc;` |
|      84 | 2712 | `	if( nArg != 2 ){` |
|       - | 2713 | `		/* PHP requires exactly two arguments */` |
|      10 | 2714 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2715 | `			"ArgumentCountError",` |
|       - | 2716 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2717 | `			nArg` |
|       - | 2718 | `			);` |
|       - | 2719 | `	}` |
|       - | 2720 | `	/* Make sure we are dealing with a valid hashmap */` |
|      78 | 2721 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2722 | `		/* Type mismatch -> TypeError */` |
|       7 | 2723 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2724 | `			"TypeError",` |
|       - | 2725 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2726 | `			ph7_type_name(apArg[1])` |
|       - | 2727 | `			);` |
|       - | 2728 | `	}` |
|       - | 2729 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      74 | 2730 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2731 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2732 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2733 | `			"use an empty string instead"` |
|       - | 2734 | `			);` |
|      73 | 2735 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2736 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2737 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2738 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2739 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2740 | `				,rVal` |
|       - | 2741 | `				);` |
|       1 | 2742 | `		}` |
|       1 | 2743 | `	}` |
|       - | 2744 | `	/* Perform the lookup */` |
|      74 | 2745 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2746 | `	/* lookup result */` |
|      74 | 2747 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      74 | 2748 | `	return PH7_OK;` |
|      43 | 2749 |  |
|       - | 2750 | `/*` |
|       - | 2751 | ` * value array_pop(array $array)` |
|       - | 2752 | ` *   POP the last inserted element from the array.` |
|       - | 2753 | ` * Parameter` |
|       - | 2754 | ` *  The array to get the value from.` |
|       - | 2755 | ` * Return` |
|       - | 2756 | ` *  Poped value or NULL on failure.` |
|       - | 2757 | ` */` |
|      18 | 2758 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2759 |  |
|       - | 2760 | `	ph7_hashmap *pMap;` |
|       - | 2761 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      20 | 2762 | `	if( nArg != 1 ){` |
|       7 | 2763 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2764 | `			"ArgumentCountError",` |
|       - | 2765 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2766 | `			nArg` |
|       - | 2767 | `			);` |
|       - | 2768 | `	}` |
|       - | 2769 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2770 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      16 | 2771 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2772 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2773 | `			"Error",` |
|       - | 2774 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2775 | `			);` |
|       - | 2776 | `	}` |
|       - | 2777 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2778 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2779 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2780 | `			"TypeError",` |
|       - | 2781 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2782 | `			ph7_type_name(apArg[0])` |
|       - | 2783 | `			);` |
|       - | 2784 | `	}` |
|       9 | 2785 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2786 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2787 | `	if( pMap->nEntry < 1 ){` |
|       - | 2788 | `		/* Nothing to pop,return NULL */` |
|       3 | 2789 | `		ph7_result_null(pCtx);` |
|       2 | 2790 | `	}else{` |
|       7 | 2791 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2792 | `		ph7_value *pObj;` |
|       7 | 2793 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2794 | `		if( pObj ){` |
|       - | 2795 | `			/* Node value */` |
|       7 | 2796 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2797 | `			/* Unlink the node */` |
|       7 | 2798 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2799 | `		}else{` |
|     ! 0 | 2800 | `			ph7_result_null(pCtx);` |
|       - | 2801 | `		}` |
|       - | 2802 | `		/* Reset the cursor */` |
|       7 | 2803 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2804 | `	}` |
|       9 | 2805 | `	return PH7_OK;` |
|      11 | 2806 |  |
|       - | 2807 | `/*` |
|       - | 2808 | ` * int array_push($array,$var,...)` |
|       - | 2809 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2810 | ` * Parameters` |
|       - | 2811 | ` *  array` |
|       - | 2812 | ` *    The input array.` |
|       - | 2813 | ` *  var` |
|       - | 2814 | ` *   On or more value to push.` |
|       - | 2815 | ` * Return` |
|       - | 2816 | ` *  New array count (including old items).` |
|       - | 2817 | ` */` |
|      22 | 2818 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2819 |  |
|       - | 2820 | `	ph7_hashmap *pMap;` |
|       - | 2821 | `	sxi32 rc;` |
|       - | 2822 | `	int i;` |
|      24 | 2823 | `	if( nArg < 1 ){` |
|       4 | 2824 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2825 | `			"ArgumentCountError",` |
|       - | 2826 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2827 | `			nArg` |
|       - | 2828 | `			);` |
|       - | 2829 | `	}` |
|       - | 2830 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2831 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      22 | 2832 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2833 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2834 | `			"Error",` |
|       - | 2835 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2836 | `			);` |
|       - | 2837 | `	}` |
|       - | 2838 | `	/* Make sure we are dealing with a valid hashmap */` |
|      18 | 2839 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2840 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2841 | `			"TypeError",` |
|       - | 2842 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2843 | `			ph7_type_name(apArg[0])` |
|       - | 2844 | `			);` |
|       - | 2845 | `	}` |
|       - | 2846 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2847 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2848 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2849 | `	/* Start pushing given values */` |
|      31 | 2850 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      17 | 2851 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      17 | 2852 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2853 | `			break;` |
|       - | 2854 | `		}` |
|       9 | 2855 | `	}` |
|       - | 2856 | `	/* Return the new count */` |
|      15 | 2857 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 2858 | `	return PH7_OK;` |
|      13 | 2859 |  |
|       - | 2860 | `/*` |
|       - | 2861 | ` * value array_shift(array $array)` |
|       - | 2862 | ` *   Shift an element off the beginning of array.` |
|       - | 2863 | ` * Parameter` |
|       - | 2864 | ` *  The array to get the value from.` |
|       - | 2865 | ` * Return` |
|       - | 2866 | ` *  Shifted value or NULL on failure.` |
|       - | 2867 | ` */` |
|      38 | 2868 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2869 |  |
|       - | 2870 | `	ph7_hashmap *pMap;` |
|       - | 2871 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      40 | 2872 | `	if( nArg != 1 ){` |
|       7 | 2873 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2874 | `			"ArgumentCountError",` |
|       - | 2875 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2876 | `			nArg` |
|       - | 2877 | `			);` |
|       - | 2878 | `	}` |
|       - | 2879 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      36 | 2880 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2881 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2882 | `			"Error",` |
|       - | 2883 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2884 | `			);` |
|       - | 2885 | `	}` |
|       - | 2886 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 2887 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2888 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2889 | `			"TypeError",` |
|       - | 2890 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2891 | `			ph7_type_name(apArg[0])` |
|       - | 2892 | `			);` |
|       - | 2893 | `	}` |
|       - | 2894 | `	/* Point to the internal representation of the hashmap */` |
|      30 | 2895 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      30 | 2896 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      30 | 2897 | `	if( pMap->nEntry < 1 ){` |
|       - | 2898 | `		/* Empty hashmap,return NULL */` |
|       3 | 2899 | `		ph7_result_null(pCtx);` |
|       2 | 2900 | `	}else{` |
|      28 | 2901 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2902 | `		ph7_value *pObj;` |
|       - | 2903 | `		sxu32 n;` |
|      28 | 2904 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      28 | 2905 | `		if( pObj ){` |
|       - | 2906 | `			/* Node value */` |
|      28 | 2907 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2908 | `			/* Unlink the first node */` |
|      28 | 2909 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      15 | 2910 | `		}else{` |
|     ! 0 | 2911 | `			ph7_result_null(pCtx);` |
|       - | 2912 | `		}` |
|       - | 2913 | `		/* Rehash all int keys */` |
|      28 | 2914 | `		n = pMap->nEntry;` |
|      28 | 2915 | `		pEntry = pMap->pFirst;` |
|      28 | 2916 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 2917 | `		for(;;){` |
|      82 | 2918 | `			if( n < 1 ){` |
|      28 | 2919 | `				break;` |
|       - | 2920 | `			}` |
|      56 | 2921 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      56 | 2922 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 2923 | `			}` |
|       - | 2924 | `			/* Point to the next entry */` |
|      56 | 2925 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      56 | 2926 | `			n--;` |
|       2 | 2927 | `		}` |
|       - | 2928 | `		/* Reset the cursor */` |
|      28 | 2929 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2930 | `	}` |
|      30 | 2931 | `	return PH7_OK;` |
|      21 | 2932 |  |
|       - | 2933 | `/*` |
|       - | 2934 | ` * Extract the node cursor value.` |
|       - | 2935 | ` */` |
|      24 | 2936 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 2937 |  |
|      25 | 2938 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 2939 | `	ph7_value *pVal;` |
|      25 | 2940 | `	if( pCur == 0 ){` |
|       - | 2941 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 2942 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2943 | `		return PH7_OK;` |
|       - | 2944 | `	}` |
|      25 | 2945 | `	if( iDirection != 0 ){` |
|       9 | 2946 | `		if( iDirection > 0 ){` |
|       - | 2947 | `			/* Point to the next entry */` |
|       7 | 2948 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 2949 | `			pCur = pMap->pCur;` |
|       4 | 2950 | `		}else{` |
|       - | 2951 | `			/* Point to the previous entry */` |
|       3 | 2952 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 2953 | `			pCur = pMap->pCur;` |
|       - | 2954 | `		}` |
|       9 | 2955 | `		if( pCur == 0 ){` |
|       - | 2956 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 2957 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2958 | `			return PH7_OK;` |
|       - | 2959 | `		}` |
|       4 | 2960 | `	}` |
|       - | 2961 | `	/* Point to the desired element */` |
|      25 | 2962 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 2963 | `	if( pVal ){` |
|      25 | 2964 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 2965 | `	}else{` |
|     ! 0 | 2966 | `		ph7_result_bool(pCtx,0);` |
|       - | 2967 | `	}` |
|      25 | 2968 | `	return PH7_OK;` |
|      13 | 2969 |  |
|       - | 2970 | `/*` |
|       - | 2971 | ` * value current(array $array)` |
|       - | 2972 | ` *  Return the current element in an array.` |
|       - | 2973 | ` * Parameter` |
|       - | 2974 | ` *  $input: The input array.` |
|       - | 2975 | ` * Return` |
|       - | 2976 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 2977 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2978 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2979 | ` *  is empty, current() returns FALSE.` |
|       - | 2980 | ` */` |
|      10 | 2981 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2982 |  |
|      11 | 2983 | `	if( nArg < 1 ){` |
|       - | 2984 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2985 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2986 | `		return PH7_OK;` |
|       - | 2987 | `	}` |
|       - | 2988 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 2989 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2990 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2991 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2992 | `		return PH7_OK;` |
|       - | 2993 | `	}` |
|      11 | 2994 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 2995 | `	return PH7_OK;` |
|       6 | 2996 |  |
|       - | 2997 | `/*` |
|       - | 2998 | ` * value next(array $input)` |
|       - | 2999 | ` *  Advance the internal array pointer of an array.` |
|       - | 3000 | ` * Parameter` |
|       - | 3001 | ` *  $input: The input array.` |
|       - | 3002 | ` * Return` |
|       - | 3003 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 3004 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 3005 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 3006 | ` */` |
|       6 | 3007 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3008 |  |
|       7 | 3009 | `	if( nArg < 1 ){` |
|       - | 3010 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3011 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3012 | `		return PH7_OK;` |
|       - | 3013 | `	}` |
|       - | 3014 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 3015 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3016 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3017 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3018 | `		return PH7_OK;` |
|       - | 3019 | `	}` |
|       7 | 3020 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 3021 | `	return PH7_OK;` |
|       4 | 3022 |  |
|       - | 3023 | `/*` |
|       - | 3024 | ` * value prev(array $input)` |
|       - | 3025 | ` *  Rewind the internal array pointer.` |
|       - | 3026 | ` * Parameter` |
|       - | 3027 | ` *  $input: The input array.` |
|       - | 3028 | ` * Return` |
|       - | 3029 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3030 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3031 | ` *  elements.` |
|       - | 3032 | ` */` |
|       2 | 3033 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3034 |  |
|       3 | 3035 | `	if( nArg < 1 ){` |
|       - | 3036 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3037 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3038 | `		return PH7_OK;` |
|       - | 3039 | `	}` |
|       - | 3040 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3041 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3042 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3043 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3044 | `		return PH7_OK;` |
|       - | 3045 | `	}` |
|       3 | 3046 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3047 | `	return PH7_OK;` |
|       2 | 3048 |  |
|       - | 3049 | `/*` |
|       - | 3050 | ` * value end(array $input)` |
|       - | 3051 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3052 | ` * Parameter` |
|       - | 3053 | ` *  $input: The input array.` |
|       - | 3054 | ` * Return` |
|       - | 3055 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3056 | ` */` |
|       2 | 3057 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3058 |  |
|       - | 3059 | `	ph7_hashmap *pMap;` |
|       3 | 3060 | `	if( nArg < 1 ){` |
|       - | 3061 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3062 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3063 | `		return PH7_OK;` |
|       - | 3064 | `	}` |
|       - | 3065 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3066 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3067 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3068 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3069 | `		return PH7_OK;` |
|       - | 3070 | `	}` |
|       - | 3071 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3072 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3073 | `	/* Point to the last node */` |
|       3 | 3074 | `	pMap->pCur = pMap->pLast;` |
|       - | 3075 | `	/* Return the last node value */` |
|       3 | 3076 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3077 | `	return PH7_OK;` |
|       2 | 3078 |  |
|       - | 3079 | `/*` |
|       - | 3080 | ` * value reset(array $array )` |
|       - | 3081 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3082 | ` * Parameter` |
|       - | 3083 | ` *  $input: The input array.` |
|       - | 3084 | ` * Return` |
|       - | 3085 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3086 | ` */` |
|       4 | 3087 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3088 |  |
|       - | 3089 | `	ph7_hashmap *pMap;` |
|       5 | 3090 | `	if( nArg < 1 ){` |
|       - | 3091 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3092 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3093 | `		return PH7_OK;` |
|       - | 3094 | `	}` |
|       - | 3095 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3096 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3097 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3098 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3099 | `		return PH7_OK;` |
|       - | 3100 | `	}` |
|       - | 3101 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3102 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3103 | `	/* Point to the first node */` |
|       5 | 3104 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3105 | `	/* Return the last node value if available */` |
|       5 | 3106 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3107 | `	return PH7_OK;` |
|       3 | 3108 |  |
|       - | 3109 | `/*` |
|       - | 3110 | ` * value key(array $array)` |
|       - | 3111 | ` *   Fetch a key from an array` |
|       - | 3112 | ` * Parameter` |
|       - | 3113 | ` *  $input` |
|       - | 3114 | ` *   The input array.` |
|       - | 3115 | ` * Return` |
|       - | 3116 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3117 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3118 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3119 | ` *  is empty, key() returns NULL.` |
|       - | 3120 | ` */` |
|       4 | 3121 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3122 |  |
|       - | 3123 | `	ph7_hashmap_node *pCur;` |
|       - | 3124 | `	ph7_hashmap *pMap;` |
|       5 | 3125 | `	if( nArg < 1 ){` |
|       - | 3126 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3127 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3128 | `		return PH7_OK;` |
|       - | 3129 | `	}` |
|       - | 3130 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3131 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3132 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3133 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3134 | `		return PH7_OK;` |
|       - | 3135 | `	}` |
|       5 | 3136 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3137 | `	pCur = pMap->pCur;` |
|       5 | 3138 | `	if( pCur == 0 ){` |
|       - | 3139 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3140 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3141 | `		return PH7_OK;` |
|       - | 3142 | `	}` |
|       5 | 3143 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3144 | `		/* Key is integer */` |
|     ! 0 | 3145 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3146 | `	}else{` |
|       - | 3147 | `		/* Key is blob */` |
|       7 | 3148 | `		ph7_result_string(pCtx,` |
|       4 | 3149 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3150 | `	}` |
|       5 | 3151 | `	return PH7_OK;` |
|       3 | 3152 |  |
|       - | 3153 | `/*` |
|       - | 3154 | ` * array each(array $input)` |
|       - | 3155 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3156 | ` * Parameter` |
|       - | 3157 | ` *  $input` |
|       - | 3158 | ` *    The input array.` |
|       - | 3159 | ` * Return` |
|       - | 3160 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3161 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3162 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3163 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3164 | ` *  each() returns FALSE.` |
|       - | 3165 | ` */` |
|      22 | 3166 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3167 |  |
|       - | 3168 | `	ph7_hashmap_node *pCur;` |
|       - | 3169 | `	ph7_hashmap *pMap;` |
|       - | 3170 | `	ph7_value *pArray;` |
|       - | 3171 | `	ph7_value *pVal;` |
|       - | 3172 | `	ph7_value sKey;` |
|      23 | 3173 | `	if( nArg < 1 ){` |
|       - | 3174 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3175 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3176 | `		return PH7_OK;` |
|       - | 3177 | `	}` |
|       - | 3178 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3179 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3180 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3181 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3182 | `		return PH7_OK;` |
|       - | 3183 | `	}` |
|       - | 3184 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3185 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3186 | `	if( pMap->pCur == 0 ){` |
|       - | 3187 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3188 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3189 | `		return PH7_OK;` |
|       - | 3190 | `	}` |
|      15 | 3191 | `	pCur = pMap->pCur;` |
|       - | 3192 | `	/* Create a new array */` |
|      15 | 3193 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3194 | `	if( pArray == 0 ){` |
|     ! 0 | 3195 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3196 | `		return PH7_OK;` |
|       - | 3197 | `	}` |
|      15 | 3198 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3199 | `	/* Insert the current value */` |
|      15 | 3200 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3201 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3202 | `	/* Make the key */` |
|      15 | 3203 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3204 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3205 | `	}else{` |
|       9 | 3206 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3207 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3208 | `	}` |
|       - | 3209 | `	/* Insert the current key */` |
|      15 | 3210 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3211 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3212 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3213 | `	/* Advance the cursor */` |
|      15 | 3214 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3215 | `	/* Return the current entry */` |
|      15 | 3216 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3217 | `	return PH7_OK;` |
|      12 | 3218 |  |
|       - | 3219 | `/*` |
|       - | 3220 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3221 | ` *  Create an array containing a range of elements` |
|       - | 3222 | ` * Parameter` |
|       - | 3223 | ` *  start` |
|       - | 3224 | ` *   First value of the sequence.` |
|       - | 3225 | ` *  limit` |
|       - | 3226 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3227 | ` *  step` |
|       - | 3228 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3229 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3230 | ` * Return` |
|       - | 3231 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3232 | ` * NOTE:` |
|       - | 3233 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3234 | ` */` |
|       2 | 3235 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3236 |  |
|       - | 3237 | `	ph7_value *pValue,*pArray;` |
|       - | 3238 | `	sxi64 iOfft,iLimit;` |
|       3 | 3239 | `	int iStep = 1;` |
|       - | 3240 |  |
|       3 | 3241 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3242 | `	if( nArg > 0 ){` |
|       - | 3243 | `		/* Extract the offset */` |
|       3 | 3244 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3245 | `		if( nArg > 1 ){` |
|       - | 3246 | `			/* Extract the limit */` |
|       3 | 3247 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3248 | `			if( nArg > 2 ){` |
|       - | 3249 | `				/* Extract the increment */` |
|       3 | 3250 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3251 | `				if( iStep < 1 ){` |
|       - | 3252 | `					/* Only positive number are allowed */` |
|       3 | 3253 | `					iStep = 1;` |
|       1 | 3254 | `				}` |
|       1 | 3255 | `			}` |
|       1 | 3256 | `		}` |
|       1 | 3257 | `	}` |
|       - | 3258 | `	/* Element container */` |
|       3 | 3259 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3260 | `	/* Create the new array */` |
|       3 | 3261 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3262 | `	if( pArray == 0 ){` |
|     ! 0 | 3263 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3264 | `		return PH7_OK;` |
|       - | 3265 | `	}` |
|       - | 3266 | `	/* Start filling */` |
|       3 | 3267 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3268 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3269 | `		/* Perform the insertion */` |
|     ! 0 | 3270 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3271 | `		/* Increment */` |
|     ! 0 | 3272 | `		iOfft += iStep;` |
|     ! 0 | 3273 | `	}` |
|       - | 3274 | `	/* Return the new array */` |
|       3 | 3275 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3276 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3277 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3278 | `	 */` |
|       3 | 3279 | `	return PH7_OK;` |
|       2 | 3280 |  |
|       - | 3281 | `/*` |
|       - | 3282 | ` * array array_values(array $array)` |
|       - | 3283 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3284 | ` * Parameters` |
|       - | 3285 | ` *  $array` |
|       - | 3286 | ` *   The input array.` |
|       - | 3287 | ` * Return` |
|       - | 3288 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3289 | ` */` |
|      30 | 3290 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3291 |  |
|       - | 3292 | `	ph7_hashmap_node *pNode;` |
|       - | 3293 | `	ph7_hashmap *pMap;` |
|       - | 3294 | `	ph7_value *pArray;` |
|       - | 3295 | `	ph7_value *pObj;` |
|       - | 3296 | `	sxu32 n;` |
|      32 | 3297 | `	if( nArg != 1 ){` |
|       - | 3298 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3299 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3300 | `			"ArgumentCountError",` |
|       - | 3301 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3302 | `			nArg` |
|       - | 3303 | `			);` |
|       - | 3304 | `	}` |
|       - | 3305 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3306 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3307 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3308 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3309 | `			"TypeError",` |
|       - | 3310 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3311 | `			ph7_type_name(apArg[0])` |
|       - | 3312 | `			);` |
|       - | 3313 | `	}` |
|       - | 3314 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3315 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3316 | `	/* Create a new array */` |
|      25 | 3317 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3318 | `	if( pArray == 0 ){` |
|     ! 0 | 3319 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3320 | `		return PH7_OK;` |
|       - | 3321 | `	}` |
|       - | 3322 | `	/* Perform the requested operation */` |
|      25 | 3323 | `	pNode = pMap->pFirst;` |
|      83 | 3324 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3325 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3326 | `		if( pObj ){` |
|       - | 3327 | `			/* perform the insertion */` |
|      59 | 3328 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3329 | `		}` |
|       - | 3330 | `		/* Point to the next entry */` |
|      59 | 3331 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3332 | `	}` |
|       - | 3333 | `	/* return the new array */` |
|      25 | 3334 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3335 | `	return PH7_OK;` |
|      17 | 3336 |  |
|       - | 3337 | `/*` |
|       - | 3338 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3339 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3340 | ` * Parameters` |
|       - | 3341 | ` *  $input` |
|       - | 3342 | ` *   An array containing keys to return.` |
|       - | 3343 | ` * $search_value` |
|       - | 3344 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3345 | ` * $strict` |
|       - | 3346 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3347 | ` * Return` |
|       - | 3348 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3349 | ` */` |
|     120 | 3350 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3351 |  |
|       - | 3352 | `	ph7_hashmap_node *pNode;` |
|       - | 3353 | `	ph7_hashmap *pMap;` |
|       - | 3354 | `	ph7_value *pArray;` |
|       - | 3355 | `	ph7_value sObj;` |
|       - | 3356 | `	ph7_value sVal;` |
|       - | 3357 | `	SyString sKey;` |
|       - | 3358 | `	int bStrict;` |
|       - | 3359 | `	sxi32 rc;` |
|       - | 3360 | `	sxu32 n;` |
|     122 | 3361 | `	if( nArg < 1 ){` |
|       - | 3362 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3363 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3364 | `			"ArgumentCountError",` |
|       - | 3365 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3366 | `			);` |
|       - | 3367 | `	}` |
|       - | 3368 | `	/* Make sure we are dealing with a valid hashmap */` |
|     120 | 3369 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3370 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3371 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3372 | `			"TypeError",` |
|       - | 3373 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3374 | `			ph7_type_name(apArg[0])` |
|       - | 3375 | `			);` |
|       - | 3376 | `	}` |
|       - | 3377 | `	/* Point to the internal representation of the input hashmap */` |
|     118 | 3378 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3379 | `	/* Create a new array */` |
|     118 | 3380 | `	pArray = ph7_context_new_array(pCtx);` |
|     118 | 3381 | `	if( pArray == 0 ){` |
|     ! 0 | 3382 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3383 | `		return PH7_OK;` |
|       - | 3384 | `	}` |
|     118 | 3385 | `	bStrict = FALSE;` |
|     118 | 3386 | `	if( nArg > 2 ){` |
|       - | 3387 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3388 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3389 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3390 | `				"TypeError",` |
|       - | 3391 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3392 | `				ph7_type_name(apArg[2])` |
|       - | 3393 | `				);` |
|       - | 3394 | `		}` |
|       5 | 3395 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3396 | `	}` |
|       - | 3397 | `	/* Perform the requested operation */` |
|     115 | 3398 | `	pNode = pMap->pFirst;` |
|     115 | 3399 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     553 | 3400 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     439 | 3401 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     117 | 3402 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      59 | 3403 | `		}else{` |
|     323 | 3404 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3405 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3406 | `		}` |
|     439 | 3407 | `		rc = 0;` |
|     439 | 3408 | `		if( nArg > 1 ){` |
|      31 | 3409 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3410 | `			if( pValue ){` |
|      31 | 3411 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3412 | `				/* Filter key */` |
|      31 | 3413 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3414 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3415 | `			}` |
|      15 | 3416 | `		}` |
|     439 | 3417 | `		if( rc == 0 ){` |
|       - | 3418 | `			/* Perform the insertion */` |
|     421 | 3419 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     210 | 3420 | `		}` |
|     439 | 3421 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3422 | `		/* Point to the next entry */` |
|     439 | 3423 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     220 | 3424 | `	}` |
|       - | 3425 | `	/* return the new array */` |
|     115 | 3426 | `	ph7_result_value(pCtx,pArray);` |
|     115 | 3427 | `	return PH7_OK;` |
|      62 | 3428 |  |
|       - | 3429 | `/*` |
|       - | 3430 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3431 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3432 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3433 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3434 | ` * Parameters` |
|       - | 3435 | ` *  $arr1` |
|       - | 3436 | ` *   First array` |
|       - | 3437 | ` *  $arr2` |
|       - | 3438 | ` *   Second array` |
|       - | 3439 | ` * Return` |
|       - | 3440 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3441 | ` * Note` |
|       - | 3442 | ` *  This function is a symisc eXtension.` |
|       - | 3443 | ` */` |
|       4 | 3444 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3445 |  |
|       - | 3446 | `	ph7_hashmap *p1,*p2;` |
|       - | 3447 | `	int rc;` |
|       5 | 3448 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3449 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3450 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3451 | `		return PH7_OK;` |
|       - | 3452 | `	}` |
|       - | 3453 | `	/* Point to the hashmaps */` |
|       5 | 3454 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3455 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3456 | `	rc = (p1 == p2);` |
|       - | 3457 | `	/* Same instance? */` |
|       5 | 3458 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3459 | `	return PH7_OK;` |
|       3 | 3460 |  |
|       - | 3461 | `/*` |
|       - | 3462 | ` * array array_merge(array ...$arrays)` |
|       - | 3463 | ` *  Merge one or more arrays.` |
|       - | 3464 | ` * Parameters` |
|       - | 3465 | ` *  ...$arrays` |
|       - | 3466 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3467 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3468 | ` * Return` |
|       - | 3469 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3470 | ` *  with no arguments.` |
|       - | 3471 | ` */` |
|     948 | 3472 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3473 |  |
|       - | 3474 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3475 | `	ph7_value *pArray;` |
|       - | 3476 | `	int i;` |
|       - | 3477 | `	/* Create a new array */` |
|     950 | 3478 | `	pArray = ph7_context_new_array(pCtx);` |
|     950 | 3479 | `	if( pArray == 0 ){` |
|     ! 0 | 3480 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3481 | `		return PH7_OK;` |
|       - | 3482 | `	}` |
|       - | 3483 | `	/* Point to the internal representation of the hashmap */` |
|     950 | 3484 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3485 | `	/* Start merging */` |
|    2836 | 3486 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3487 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1892 | 3488 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3489 | `			/* Type mismatch -> TypeError */` |
|       7 | 3490 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3491 | `				"TypeError",` |
|       - | 3492 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3493 | `				i + 1,` |
|       4 | 3494 | `				ph7_type_name(apArg[i])` |
|       - | 3495 | `				);` |
|     ! 0 | 3496 | `		}else{` |
|    1888 | 3497 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3498 | `			/* Merge the two hashmaps */` |
|    1888 | 3499 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3500 | `		}` |
|     945 | 3501 | `	}` |
|       - | 3502 | `	/* Return the freshly created array */` |
|     946 | 3503 | `	ph7_result_value(pCtx,pArray);` |
|     946 | 3504 | `	return PH7_OK;` |
|     476 | 3505 |  |
|       - | 3506 | `/*` |
|       - | 3507 | ` * array array_copy(array $source)` |
|       - | 3508 | ` *  Make a blind copy of the target array.` |
|       - | 3509 | ` * Parameters` |
|       - | 3510 | ` *  $source` |
|       - | 3511 | ` *   Target array` |
|       - | 3512 | ` * Return` |
|       - | 3513 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3514 | ` * Note` |
|       - | 3515 | ` *  This function is a symisc eXtension.` |
|       - | 3516 | ` */` |
|      16 | 3517 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3518 |  |
|       - | 3519 | `	ph7_hashmap *pMap;` |
|       - | 3520 | `	ph7_value *pArray;` |
|      17 | 3521 | `	if( nArg < 1 ){` |
|       - | 3522 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3523 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3524 | `		return PH7_OK;` |
|       - | 3525 | `	}` |
|       - | 3526 | `	/* Create a new array */` |
|      17 | 3527 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3528 | `	if( pArray == 0 ){` |
|     ! 0 | 3529 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3530 | `		return PH7_OK;` |
|       - | 3531 | `	}` |
|       - | 3532 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3533 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3534 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3535 | `		/* Point to the internal representation of the source */` |
|      17 | 3536 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3537 | `		/* Perform the copy */` |
|      17 | 3538 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3539 | `	}else{` |
|       - | 3540 | `		/* Simple insertion */` |
|     ! 0 | 3541 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3542 | `	}` |
|       - | 3543 | `	/* Return the duplicated array */` |
|      17 | 3544 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3545 | `	return PH7_OK;` |
|       9 | 3546 |  |
|       - | 3547 | `/*` |
|       - | 3548 | ` * bool array_erase(array $source)` |
|       - | 3549 | ` *  Remove all elements from a given array.` |
|       - | 3550 | ` * Parameters` |
|       - | 3551 | ` *  $source` |
|       - | 3552 | ` *   Target array` |
|       - | 3553 | ` * Return` |
|       - | 3554 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3555 | ` * Note` |
|       - | 3556 | ` *  This function is a symisc eXtension.` |
|       - | 3557 | ` */` |
|      16 | 3558 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3559 |  |
|       - | 3560 | `	ph7_hashmap *pMap;` |
|      17 | 3561 | `	if( nArg < 1 ){` |
|       - | 3562 | `		/* Missing arguments */` |
|     ! 0 | 3563 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3564 | `		return PH7_OK;` |
|       - | 3565 | `	}` |
|       - | 3566 | `	/* Point to the target hashmap */` |
|      17 | 3567 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3568 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3569 | `	/* Erase */` |
|      17 | 3570 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3571 | `	return PH7_OK;` |
|       9 | 3572 |  |
|       - | 3573 | `/*` |
|       - | 3574 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3575 | ` *  Extract a slice of the array.` |
|       - | 3576 | ` * Parameters` |
|       - | 3577 | ` *  $array` |
|       - | 3578 | ` *    The input array.` |
|       - | 3579 | ` * $offset` |
|       - | 3580 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3581 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3582 | ` * $length (optional, nullable)` |
|       - | 3583 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3584 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3585 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3586 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3587 | ` * $preserve_keys (optional)` |
|       - | 3588 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3589 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3590 | ` * Return` |
|       - | 3591 | ` *   The new slice.` |
|       - | 3592 | ` */` |
|      46 | 3593 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3594 |  |
|       - | 3595 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3596 | `	ph7_hashmap_node *pCur;` |
|       - | 3597 | `	ph7_value *pArray;` |
|       - | 3598 | `	int iLength,iOfft;` |
|       - | 3599 | `	int bPreserve;` |
|       - | 3600 | `	sxi32 rc;` |
|      48 | 3601 | `	if( nArg < 2 ){` |
|       7 | 3602 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3603 | `			"ArgumentCountError",` |
|       - | 3604 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3605 | `			nArg` |
|       - | 3606 | `			);` |
|       - | 3607 | `	}` |
|      44 | 3608 | `	if( nArg > 4 ){` |
|       4 | 3609 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3610 | `			"ArgumentCountError",` |
|       - | 3611 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3612 | `			nArg` |
|       - | 3613 | `			);` |
|       - | 3614 | `	}` |
|      42 | 3615 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3616 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3617 | `			"TypeError",` |
|       - | 3618 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3619 | `			ph7_type_name(apArg[0])` |
|       - | 3620 | `			);` |
|       - | 3621 | `	}` |
|       - | 3622 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3623 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3624 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3625 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3626 | `			"TypeError",` |
|       - | 3627 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3628 | `			ph7_type_name(apArg[1])` |
|       - | 3629 | `			);` |
|       - | 3630 | `	}` |
|       - | 3631 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3632 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3633 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3634 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3635 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3636 | `				"TypeError",` |
|       - | 3637 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3638 | `				ph7_type_name(apArg[2])` |
|       - | 3639 | `				);` |
|       - | 3640 | `		}` |
|       8 | 3641 | `	}` |
|       - | 3642 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3643 | `	if( nArg > 3 ){` |
|      10 | 3644 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3645 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3646 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3647 | `				"TypeError",` |
|       - | 3648 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3649 | `				ph7_type_name(apArg[3])` |
|       - | 3650 | `				);` |
|       - | 3651 | `		}` |
|       2 | 3652 | `	}` |
|       - | 3653 | `	/* Point the internal representation of the target array */` |
|      33 | 3654 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3655 | `	bPreserve = FALSE;` |
|       - | 3656 | `	/* Get the offset */` |
|      33 | 3657 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3658 | `	if( iOfft < 0 ){` |
|       5 | 3659 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3660 | `		if( iOfft < 0 ){` |
|       3 | 3661 | `			iOfft = 0;` |
|       1 | 3662 | `		}` |
|       2 | 3663 | `	}` |
|      33 | 3664 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3665 | `		/* Offset past end of array, return empty array */` |
|       5 | 3666 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3667 | `		if( pArray == 0 ){` |
|     ! 0 | 3668 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3669 | `			return PH7_OK;` |
|       - | 3670 | `		}` |
|       5 | 3671 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3672 | `		return PH7_OK;` |
|       - | 3673 | `	}` |
|       - | 3674 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3675 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3676 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3677 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3678 | `		if( iLength < 0 ){` |
|       5 | 3679 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3680 | `		}` |
|      15 | 3681 | `		if( iLength < 0 ){` |
|       3 | 3682 | `			iLength = 0;` |
|       1 | 3683 | `		}` |
|      15 | 3684 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3685 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3686 | `		}` |
|       7 | 3687 | `	}` |
|      29 | 3688 | `	if( nArg > 3 ){` |
|       5 | 3689 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3690 | `	}` |
|       - | 3691 | `	/* Create a new array */` |
|      29 | 3692 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3693 | `	if( pArray == 0 ){` |
|     ! 0 | 3694 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3695 | `		return PH7_OK;` |
|       - | 3696 | `	}` |
|      29 | 3697 | `	if( iLength < 1 ){` |
|       - | 3698 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3699 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3700 | `		return PH7_OK;` |
|       - | 3701 | `	}` |
|       - | 3702 | `	/* Point to the desired entry */` |
|      25 | 3703 | `	pCur = pSrc->pFirst;` |
|      24 | 3704 | `	for(;;){` |
|      49 | 3705 | `		if( iOfft < 1 ){` |
|      25 | 3706 | `			break;` |
|       - | 3707 | `		}` |
|       - | 3708 | `		/* Point to the next entry */` |
|      25 | 3709 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3710 | `		iOfft--;` |
|       1 | 3711 | `	}` |
|       - | 3712 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3713 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3714 | `	for(;;){` |
|      79 | 3715 | `		if( iLength < 1 ){` |
|      25 | 3716 | `			break;` |
|       - | 3717 | `		}` |
|       - | 3718 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3719 | `		{` |
|      55 | 3720 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3721 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3722 | `		}` |
|      55 | 3723 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3724 | `			break;` |
|       - | 3725 | `		}` |
|       - | 3726 | `		/* Point to the next entry */` |
|      55 | 3727 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3728 | `		iLength--;` |
|       1 | 3729 | `	}` |
|       - | 3730 | `	/* Return the freshly created array */` |
|      25 | 3731 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3732 | `	return PH7_OK;` |
|      25 | 3733 |  |
|       - | 3734 | `/*` |
|       - | 3735 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3736 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3737 | ` * beginning (becomes the new pFirst).` |
|       - | 3738 | ` */` |
|      30 | 3739 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3740 |  |
|       - | 3741 | `	ph7_hashmap_node *pNode;` |
|       - | 3742 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3743 | `	pNode = pMap->pLast;` |
|      31 | 3744 | `	if( pNode == 0 ){` |
|     ! 0 | 3745 | `		return;` |
|       - | 3746 | `	}` |
|      31 | 3747 | `	if( pNode->pNext == 0 ){` |
|       - | 3748 | `		/* Only node in the list, nothing to move */` |
|       5 | 3749 | `		return;` |
|       - | 3750 | `	}` |
|      27 | 3751 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3752 | `		/* Already in the correct position */` |
|       9 | 3753 | `		return;` |
|       - | 3754 | `	}` |
|       - | 3755 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3756 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3757 | `	pMap->pLast->pPrev = 0;` |
|       - | 3758 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3759 | `	if( pAfter == 0 ){` |
|       - | 3760 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3761 | `		pNode->pNext = 0;` |
|       3 | 3762 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3763 | `		if( pMap->pFirst ){` |
|       3 | 3764 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3765 | `		}` |
|       3 | 3766 | `		pMap->pFirst = pNode;` |
|       2 | 3767 | `	}else{` |
|      17 | 3768 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3769 | `		pNode->pPrev = pOldNext;` |
|      17 | 3770 | `		pNode->pNext = pAfter;` |
|      17 | 3771 | `		pAfter->pPrev = pNode;` |
|      17 | 3772 | `		if( pOldNext ){` |
|      17 | 3773 | `			pOldNext->pNext = pNode;` |
|       9 | 3774 | `		}else{` |
|     ! 0 | 3775 | `			pMap->pLast = pNode;` |
|       - | 3776 | `		}` |
|       - | 3777 | `	}` |
|      16 | 3778 |  |
|       - | 3779 | `/*` |
|       - | 3780 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3781 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3782 | ` * Parameters` |
|       - | 3783 | ` *  $array` |
|       - | 3784 | ` *    The input array.` |
|       - | 3785 | ` *  $offset` |
|       - | 3786 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3787 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3788 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3789 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3790 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3791 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3792 | ` *  $length (optional)` |
|       - | 3793 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3794 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3795 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3796 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3797 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3798 | ` *  $replacement (optional)` |
|       - | 3799 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3800 | ` *    with elements from this array.` |
|       - | 3801 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3802 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3803 | ` *    offset.` |
|       - | 3804 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3805 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3806 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3807 | ` * Return` |
|       - | 3808 | ` *   A new array consisting of the extracted elements.` |
|       - | 3809 | ` */` |
|      54 | 3810 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3811 |  |
|       - | 3812 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3813 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3814 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3815 | `	int iLength,iOfft,i;` |
|       - | 3816 | `	sxi32 rc;` |
|      56 | 3817 | `	if( nArg < 2 ){` |
|       7 | 3818 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3819 | `			"ArgumentCountError",` |
|       - | 3820 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3821 | `			nArg` |
|       - | 3822 | `			);` |
|       - | 3823 | `	}` |
|      52 | 3824 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3825 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3826 | `			"TypeError",` |
|       - | 3827 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3828 | `			ph7_type_name(apArg[0])` |
|       - | 3829 | `			);` |
|       - | 3830 | `	}` |
|       - | 3831 | `	/* Point to the internal representation of the target array */` |
|      49 | 3832 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3833 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3834 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3835 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3836 | `	if( iOfft < 0 ){` |
|       7 | 3837 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3838 | `		if( iOfft < 0 ){` |
|       3 | 3839 | `			iOfft = 0;` |
|       2 | 3840 | `		}` |
|      46 | 3841 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3842 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3843 | `	}` |
|       - | 3844 | `	/* Get the length and clamp to valid range.` |
|       - | 3845 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3846 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3847 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3848 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3849 | `		if( iLength < 0 ){` |
|       7 | 3850 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3851 | `			if( iLength < 0 ){` |
|       3 | 3852 | `				iLength = 0;` |
|       1 | 3853 | `			}` |
|       3 | 3854 | `		}` |
|      31 | 3855 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3856 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3857 | `		}` |
|      15 | 3858 | `	}` |
|       - | 3859 | `	/* Create the result array for removed elements */` |
|      49 | 3860 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3861 | `	if( pArray == 0 ){` |
|     ! 0 | 3862 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3863 | `		return PH7_OK;` |
|       - | 3864 | `	}` |
|       - | 3865 | `	/* Get replacement array if provided */` |
|      49 | 3866 | `	pRep = 0;` |
|      49 | 3867 | `	if( nArg > 3 ){` |
|      21 | 3868 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3869 | `			/* Perform an array cast */` |
|       3 | 3870 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3871 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3872 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3873 | `			}` |
|       2 | 3874 | `		}else{` |
|      19 | 3875 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3876 | `		}` |
|      21 | 3877 | `		if( pRep ){` |
|       - | 3878 | `			/* Reset the loop cursor */` |
|      21 | 3879 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3880 | `		}` |
|      10 | 3881 | `	}` |
|       - | 3882 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3883 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3884 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3885 | `		return PH7_OK;` |
|       - | 3886 | `	}` |
|       - | 3887 | `	/* Navigate to the offset position */` |
|      41 | 3888 | `	pCur = pSrc->pFirst;` |
|      85 | 3889 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3890 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3891 | `	}` |
|       - | 3892 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3893 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3894 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3895 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3896 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3897 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3898 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3899 | `		pPrev = pCur->pPrev;` |
|      71 | 3900 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3901 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3902 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3903 | `			break;` |
|       - | 3904 | `		}` |
|      71 | 3905 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3906 | `	}` |
|       - | 3907 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3908 | `	if( pRep ){` |
|       - | 3909 | `		ph7_value sSafeVal;` |
|      61 | 3910 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3911 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3912 | `			if( pRvalue ){` |
|       - | 3913 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3914 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3915 | `				 * since it points into that same pool. */` |
|      31 | 3916 | `				sSafeVal = *pRvalue;` |
|      31 | 3917 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 3918 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 3919 | `					pNewNode = pSrc->pLast;` |
|      31 | 3920 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 3921 | `					pInsertAfter = pNewNode;` |
|      15 | 3922 | `				}` |
|      15 | 3923 | `			}` |
|       1 | 3924 | `		}` |
|      10 | 3925 | `	}` |
|       - | 3926 | `	/* Return the freshly created array */` |
|      41 | 3927 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 3928 | `	return PH7_OK;` |
|      29 | 3929 |  |
|       - | 3930 | `/*` |
|       - | 3931 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3932 | ` *  Checks if a value exists in an array.` |
|       - | 3933 | ` * Parameters` |
|       - | 3934 | ` *  $needle` |
|       - | 3935 | ` *   The searched value.` |
|       - | 3936 | ` *   Note:` |
|       - | 3937 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3938 | ` * $haystack` |
|       - | 3939 | ` *  The target array.` |
|       - | 3940 | ` * $strict` |
|       - | 3941 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3942 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3943 | ` */` |
|   28854 | 3944 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3945 |  |
|       - | 3946 | `	ph7_value *pNeedle;` |
|       - | 3947 | `	int bStrict;` |
|       - | 3948 | `	int rc;` |
|   28856 | 3949 | `	if( nArg < 2 ){` |
|       - | 3950 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3951 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3952 | `		return PH7_OK;` |
|       - | 3953 | `	}` |
|   28856 | 3954 | `	pNeedle = apArg[0];` |
|   28856 | 3955 | `	bStrict = 0;` |
|   28856 | 3956 | `	if( nArg > 2 ){` |
|       5 | 3957 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3958 | `	}` |
|   28856 | 3959 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3960 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3961 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3962 | `		/* Set the comparison result */` |
|     ! 0 | 3963 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3964 | `		return PH7_OK;` |
|       - | 3965 | `	}` |
|       - | 3966 | `	/* Perform the lookup */` |
|   28856 | 3967 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3968 | `	/* Lookup result */` |
|   28856 | 3969 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   28856 | 3970 | `	return PH7_OK;` |
|   14429 | 3971 |  |
|       - | 3972 | `/*` |
|       - | 3973 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3974 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3975 | ` * Parameters` |
|       - | 3976 | ` * $needle` |
|       - | 3977 | ` *   The searched value.` |
|       - | 3978 | ` * $haystack` |
|       - | 3979 | ` *   The array.` |
|       - | 3980 | ` * $strict` |
|       - | 3981 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3982 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3983 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3984 | ` * Return` |
|       - | 3985 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3986 | ` */` |
|      28 | 3987 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3988 |  |
|       - | 3989 | `	ph7_hashmap_node *pEntry;` |
|       - | 3990 | `	ph7_value *pVal,sNeedle;` |
|       - | 3991 | `	ph7_hashmap *pMap;` |
|       - | 3992 | `	ph7_value sVal;` |
|       - | 3993 | `	int bStrict;` |
|       - | 3994 | `	sxu32 n;` |
|       - | 3995 | `	int rc;` |
|      30 | 3996 | `	if( nArg < 2 ){` |
|       - | 3997 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3998 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3999 | `			"ArgumentCountError",` |
|       - | 4000 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4001 | `			nArg` |
|       - | 4002 | `			);` |
|       - | 4003 | `	}` |
|      26 | 4004 | `	bStrict = FALSE;` |
|      26 | 4005 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4006 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4007 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4008 | `			"TypeError",` |
|       - | 4009 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4010 | `			ph7_type_name(apArg[1])` |
|       - | 4011 | `			);` |
|       - | 4012 | `	}` |
|      24 | 4013 | `	if( nArg > 2 ){` |
|       - | 4014 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4015 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4016 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4017 | `				"TypeError",` |
|       - | 4018 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4019 | `				ph7_type_name(apArg[2])` |
|       - | 4020 | `				);` |
|       - | 4021 | `		}` |
|       9 | 4022 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4023 | `	}` |
|       - | 4024 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4025 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4026 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4027 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4028 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4029 | `	pEntry = pMap->pFirst;` |
|      21 | 4030 | `	n = pMap->nEntry;` |
|      23 | 4031 | `	for(;;){` |
|      47 | 4032 | `		if( !n ){` |
|       9 | 4033 | `			break;` |
|       - | 4034 | `		}` |
|       - | 4035 | `		/* Extract node value */` |
|      39 | 4036 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4037 | `		if( pVal ){` |
|       - | 4038 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4039 | `			 * can change their type.` |
|       - | 4040 | `			 */` |
|      39 | 4041 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4042 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4043 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4044 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4045 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4046 | `			if( rc == 0 ){` |
|       - | 4047 | `				/* Match found,return key */` |
|      13 | 4048 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4049 | `					/* INT key */` |
|       7 | 4050 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4051 | `				}else{` |
|       7 | 4052 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4053 | `					/* Blob key */` |
|       7 | 4054 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4055 | `				}` |
|      13 | 4056 | `				return PH7_OK;` |
|       - | 4057 | `			}` |
|      13 | 4058 | `		}` |
|       - | 4059 | `		/* Point to the next entry */` |
|      27 | 4060 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4061 | `		n--;` |
|       1 | 4062 | `	}` |
|       - | 4063 | `	/* No such value,return FALSE */` |
|       9 | 4064 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4065 | `	return PH7_OK;` |
|      16 | 4066 |  |
|       - | 4067 | `/*` |
|       - | 4068 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4069 | ` *  Computes the difference of arrays.` |
|       - | 4070 | ` * Parameters` |
|       - | 4071 | ` *  $array1` |
|       - | 4072 | ` *    The array to compare from` |
|       - | 4073 | ` *  $array2` |
|       - | 4074 | ` *    An array to compare against` |
|       - | 4075 | ` *  $...` |
|       - | 4076 | ` *   More arrays to compare against` |
|       - | 4077 | ` * Return` |
|       - | 4078 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4079 | ` *  are not present in any of the other arrays.` |
|       - | 4080 | ` */` |
|      22 | 4081 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4082 |  |
|       - | 4083 | `	ph7_hashmap_node *pEntry;` |
|       - | 4084 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4085 | `	ph7_value *pArray;` |
|       - | 4086 | `	ph7_value *pVal;` |
|       - | 4087 | `	sxi32 rc;` |
|       - | 4088 | `	sxu32 n;` |
|       - | 4089 | `	int i;` |
|       - | 4090 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4091 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4092 | `	 * debugging difficult. */` |
|      24 | 4093 | `	if( nArg < 1 ){` |
|       4 | 4094 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4095 | `			"ArgumentCountError",` |
|       - | 4096 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4097 | `			nArg` |
|       - | 4098 | `			);` |
|       - | 4099 | `	}` |
|      22 | 4100 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4101 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4102 | `			"TypeError",` |
|       - | 4103 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4104 | `			ph7_type_name(apArg[0])` |
|       - | 4105 | `			);` |
|       - | 4106 | `	}` |
|      36 | 4107 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4108 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4109 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4110 | `				"TypeError",` |
|       - | 4111 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4112 | `				i + 1,` |
|       2 | 4113 | `				ph7_type_name(apArg[i])` |
|       - | 4114 | `				);` |
|       - | 4115 | `		}` |
|       9 | 4116 | `	}` |
|      17 | 4117 | `	if( nArg == 1 ){` |
|       - | 4118 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4119 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4120 | `		return PH7_OK;` |
|       - | 4121 | `	}` |
|       - | 4122 | `	/* Create a new array */` |
|      15 | 4123 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4124 | `	if( pArray == 0 ){` |
|     ! 0 | 4125 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4126 | `		return PH7_OK;` |
|       - | 4127 | `	}` |
|       - | 4128 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4129 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4130 | `	/* Perform the diff */` |
|      15 | 4131 | `	pEntry = pSrc->pFirst;` |
|      15 | 4132 | `	n = pSrc->nEntry;` |
|      27 | 4133 | `	for(;;){` |
|      55 | 4134 | `		if( n < 1 ){` |
|      15 | 4135 | `			break;` |
|       - | 4136 | `		}` |
|       - | 4137 | `		/* Extract the node value */` |
|      41 | 4138 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4139 | `		if( pVal ){` |
|      69 | 4140 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4141 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4142 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4143 | `				/* Perform the lookup */` |
|      45 | 4144 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4145 | `				if( rc == SXRET_OK ){` |
|       - | 4146 | `					/* Value exist */` |
|      17 | 4147 | `					break;` |
|       - | 4148 | `				}` |
|      15 | 4149 | `			}` |
|      41 | 4150 | `			if( i >= nArg ){` |
|       - | 4151 | `				/* Perform the insertion */` |
|      25 | 4152 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4153 | `			}` |
|      20 | 4154 | `		}` |
|       - | 4155 | `		/* Point to the next entry */` |
|      41 | 4156 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4157 | `		n--;` |
|       1 | 4158 | `	}` |
|       - | 4159 | `	/* Return the freshly created array */` |
|      15 | 4160 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4161 | `	return PH7_OK;` |
|      13 | 4162 |  |
|       - | 4163 | `/*` |
|       - | 4164 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4165 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4166 | ` * Parameters` |
|       - | 4167 | ` *  $array1` |
|       - | 4168 | ` *    The array to compare from` |
|       - | 4169 | ` *  $array2` |
|       - | 4170 | ` *    An array to compare against` |
|       - | 4171 | ` *  $...` |
|       - | 4172 | ` *   More arrays to compare against.` |
|       - | 4173 | ` * $callback` |
|       - | 4174 | ` *  The callback comparison function.` |
|       - | 4175 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4176 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4177 | ` *  than the second.` |
|       - | 4178 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4179 | ` * Return` |
|       - | 4180 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4181 | ` *  are not present in any of the other arrays.` |
|       - | 4182 | ` */` |
|      20 | 4183 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4184 |  |
|       - | 4185 | `	ph7_hashmap_node *pEntry;` |
|       - | 4186 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4187 | `	ph7_value *pCallback;` |
|       - | 4188 | `	ph7_value *pArray;` |
|       - | 4189 | `	ph7_value *pVal;` |
|       - | 4190 | `	sxi32 rc;` |
|       - | 4191 | `	sxu32 n;` |
|       - | 4192 | `	int i;` |
|       - | 4193 |  |
|       - | 4194 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      22 | 4195 | `	if( nArg < 2 ){` |
|       4 | 4196 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4197 | `			"ArgumentCountError",` |
|       - | 4198 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4199 | `			nArg` |
|       - | 4200 | `			);` |
|       - | 4201 | `	}` |
|      20 | 4202 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4203 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4204 | `			"TypeError",` |
|       - | 4205 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4206 | `			ph7_type_name(apArg[0])` |
|       - | 4207 | `			);` |
|       - | 4208 | `	}` |
|       - | 4209 |  |
|      18 | 4210 | `	if( nArg == 2 ){` |
|       - | 4211 | `		/* Only the original array and the callback were provided. */` |
|       - | 4212 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4213 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4214 | `		 * validation order.` |
|       - | 4215 | `		 */` |
|       4 | 4216 | `	} else {` |
|       - | 4217 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      20 | 4218 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      14 | 4219 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      10 | 4220 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4221 | `					"TypeError",` |
|       - | 4222 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4223 | `					i + 1,` |
|       6 | 4224 | `					ph7_type_name(apArg[i])` |
|       - | 4225 | `					);` |
|       - | 4226 | `			}` |
|       5 | 4227 | `		}` |
|       - | 4228 | `	}` |
|       - | 4229 |  |
|       - | 4230 | `	/* Identify the callback (always expected as the last argument). */` |
|      12 | 4231 | `	pCallback = apArg[nArg - 1];` |
|       - | 4232 | `	/* Validate the callback to match PHP's error messages. */` |
|      12 | 4233 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       7 | 4234 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4235 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4236 | `				"TypeError",` |
|       - | 4237 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4238 | `				nArg` |
|       - | 4239 | `				);` |
|       - | 4240 | `		}` |
|       5 | 4241 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4242 | `			int len;` |
|       3 | 4243 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4244 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4245 | `				"TypeError",` |
|       - | 4246 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4247 | `				nArg,` |
|       1 | 4248 | `				zName` |
|       - | 4249 | `				);` |
|       - | 4250 | `		}` |
|       4 | 4251 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4252 | `			"TypeError",` |
|       - | 4253 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4254 | `			nArg` |
|       - | 4255 | `			);` |
|       - | 4256 | `	}` |
|       - | 4257 |  |
|       5 | 4258 | `	if( nArg == 2 ){` |
|       - | 4259 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4260 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4261 | `		return PH7_OK;` |
|       - | 4262 | `	}` |
|       - | 4263 |  |
|       - | 4264 | `	/* Create a new array */` |
|       3 | 4265 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4266 | `	if( pArray == 0 ){` |
|     ! 0 | 4267 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4268 | `		return PH7_OK;` |
|       - | 4269 | `	}` |
|       - | 4270 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4271 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4272 | `	/* Perform the diff */` |
|       3 | 4273 | `	pEntry = pSrc->pFirst;` |
|       3 | 4274 | `	n = pSrc->nEntry;` |
|       4 | 4275 | `	for(;;){` |
|       9 | 4276 | `		if( n < 1 ){` |
|       3 | 4277 | `			break;` |
|       - | 4278 | `		}` |
|       - | 4279 | `		/* Extract the node value */` |
|       7 | 4280 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4281 | `		if( pVal ){` |
|      11 | 4282 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4283 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4284 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4285 | `				/* Perform the lookup */` |
|       7 | 4286 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4287 | `				if( rc == SXRET_OK ){` |
|       - | 4288 | `					/* Value exist */` |
|       3 | 4289 | `					break;` |
|       - | 4290 | `				}` |
|       3 | 4291 | `			}` |
|       7 | 4292 | `			if( i >= (nArg - 1)){` |
|       - | 4293 | `				/* Perform the insertion */` |
|       5 | 4294 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4295 | `			}` |
|       3 | 4296 | `		}` |
|       - | 4297 | `		/* Point to the next entry */` |
|       7 | 4298 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4299 | `		n--;` |
|       1 | 4300 | `	}` |
|       - | 4301 | `	/* Return the freshly created array */` |
|       3 | 4302 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4303 | `	return PH7_OK;` |
|      12 | 4304 |  |
|       - | 4305 | `/*` |
|       - | 4306 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4307 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4308 | ` * Parameters` |
|       - | 4309 | ` *  $array1` |
|       - | 4310 | ` *    The array to compare from` |
|       - | 4311 | ` *  $array2` |
|       - | 4312 | ` *    An array to compare against` |
|       - | 4313 | ` *  $...` |
|       - | 4314 | ` *   More arrays to compare against` |
|       - | 4315 | ` * Return` |
|       - | 4316 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4317 | ` *  are not present in any of the other arrays.` |
|       - | 4318 | ` */` |
|      20 | 4319 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4320 |  |
|       - | 4321 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4322 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4323 | `	ph7_value *pArray;` |
|       - | 4324 | `	ph7_value *pVal;` |
|       - | 4325 | `	sxi32 rc;` |
|       - | 4326 | `	sxu32 n;` |
|       - | 4327 | `	int i;` |
|       - | 4328 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4329 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4330 | `	 * accompanying integration tests to pass. */` |
|      22 | 4331 | `	if( nArg < 1 ){` |
|       4 | 4332 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4333 | `			"ArgumentCountError",` |
|       - | 4334 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4335 | `			nArg` |
|       - | 4336 | `			);` |
|       - | 4337 | `	}` |
|      20 | 4338 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4339 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4340 | `			"TypeError",` |
|       - | 4341 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4342 | `			ph7_type_name(apArg[0])` |
|       - | 4343 | `			);` |
|       - | 4344 | `	}` |
|      32 | 4345 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4346 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4347 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4348 | `				"TypeError",` |
|       - | 4349 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4350 | `				i + 1,` |
|       4 | 4351 | `				ph7_type_name(apArg[i])` |
|       - | 4352 | `				);` |
|       - | 4353 | `		}` |
|       9 | 4354 | `	}` |
|      13 | 4355 | `	if( nArg == 1 ){` |
|       - | 4356 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4357 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4358 | `		return PH7_OK;` |
|       - | 4359 | `	}` |
|       - | 4360 | `	/* Create a new array */` |
|      11 | 4361 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4362 | `	if( pArray == 0 ){` |
|     ! 0 | 4363 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4364 | `		return PH7_OK;` |
|       - | 4365 | `	}` |
|       - | 4366 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4367 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4368 | `	/* Perform the diff */` |
|      11 | 4369 | `	pEntry = pSrc->pFirst;` |
|      11 | 4370 | `	n = pSrc->nEntry;` |
|      11 | 4371 | `	pN1 = pN2 = 0;` |
|      29 | 4372 | `	for(;;){` |
|       - | 4373 | `		int keep;` |
|      35 | 4374 | `		if( n < 1 ){` |
|      11 | 4375 | `			break;` |
|       - | 4376 | `		}` |
|       - | 4377 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4378 | `		keep = 1;` |
|      41 | 4379 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4380 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4381 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4382 | `			/* Perform a key lookup first */` |
|      29 | 4383 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4384 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4385 | `			}else{` |
|      17 | 4386 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4387 | `			}` |
|      29 | 4388 | `			if( rc != SXRET_OK ){` |
|       - | 4389 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4390 | `				continue;` |
|       - | 4391 | `			}` |
|       - | 4392 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4393 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4394 | `			if( pVal ){` |
|       - | 4395 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4396 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4397 | `				if( pVal2 ){` |
|      15 | 4398 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4399 | `					if( cmp == 0 ){` |
|       - | 4400 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4401 | `						keep = 0;` |
|      13 | 4402 | `						break;` |
|       - | 4403 | `					}` |
|       1 | 4404 | `				}` |
|       1 | 4405 | `			}` |
|       2 | 4406 | `		}` |
|      25 | 4407 | `		if( keep ){` |
|       - | 4408 | `			/* Perform the insertion */` |
|      13 | 4409 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4410 | `		}` |
|       - | 4411 | `		/* Point to the next entry */` |
|      25 | 4412 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4413 | `		n--;` |
|       1 | 4414 | `	}` |
|       - | 4415 | `	/* Return the freshly created array */` |
|      11 | 4416 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4417 | `	return PH7_OK;` |
|      12 | 4418 |  |
|       - | 4419 | `/*` |
|       - | 4420 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4421 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4422 | ` *  by a user supplied callback function.` |
|       - | 4423 | ` * Parameters` |
|       - | 4424 | ` *  $array1` |
|       - | 4425 | ` *    The array to compare from` |
|       - | 4426 | ` *  $array2` |
|       - | 4427 | ` *    An array to compare against` |
|       - | 4428 | ` *  $...` |
|       - | 4429 | ` *   More arrays to compare against.` |
|       - | 4430 | ` *  $key_compare_func` |
|       - | 4431 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4432 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4433 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4434 | ` * Return` |
|       - | 4435 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4436 | ` *  are not present in any of the other arrays.` |
|       - | 4437 | ` */` |
|      22 | 4438 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4439 |  |
|       - | 4440 | `	ph7_hashmap_node *pEntry;` |
|       - | 4441 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4442 | `	ph7_value *pCallback;` |
|       - | 4443 | `	ph7_value *pArray;` |
|       - | 4444 | `	sxi32 rc;` |
|       - | 4445 | `	sxu32 n;` |
|       - | 4446 | `	int i;` |
|       - | 4447 |  |
|       - | 4448 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4449 | `	if( nArg < 2 ){` |
|       4 | 4450 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4451 | `			"ArgumentCountError",` |
|       - | 4452 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4453 | `			nArg` |
|       - | 4454 | `			);` |
|       - | 4455 | `	}` |
|      22 | 4456 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4457 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4458 | `			"TypeError",` |
|       - | 4459 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4460 | `			ph7_type_name(apArg[0])` |
|       - | 4461 | `			);` |
|       - | 4462 | `	}` |
|       - | 4463 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4464 | `	 * expected to be a callback. */` |
|      32 | 4465 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4466 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4467 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4468 | `				"TypeError",` |
|       - | 4469 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4470 | `				i + 1,` |
|       2 | 4471 | `				ph7_type_name(apArg[i])` |
|       - | 4472 | `				);` |
|       - | 4473 | `		}` |
|       8 | 4474 | `	}` |
|       - | 4475 | `	/* Point to the callback value */` |
|      18 | 4476 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4477 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4478 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4479 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4480 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4481 | `		 * string given" which we also reproduce. */` |
|       7 | 4482 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4483 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4484 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4485 | `				"TypeError",` |
|       - | 4486 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4487 | `				nArg` |
|       - | 4488 | `				);` |
|       - | 4489 | `		}` |
|       5 | 4490 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4491 | `			/* neither array nor string */` |
|       7 | 4492 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4493 | `				"TypeError",` |
|       - | 4494 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4495 | `				nArg` |
|       - | 4496 | `				);` |
|       - | 4497 | `		}` |
|       - | 4498 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4499 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4500 | `			"TypeError",` |
|       - | 4501 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4502 | `			nArg,` |
|     ! 0 | 4503 | `			ph7_type_name(pCallback)` |
|       - | 4504 | `			);` |
|       - | 4505 | `	}` |
|      11 | 4506 | `	if( nArg == 2 ){` |
|       - | 4507 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4508 | `		 * input array. */` |
|       3 | 4509 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4510 | `		return PH7_OK;` |
|       - | 4511 | `	}` |
|       - | 4512 | `	/* Create a new array */` |
|       9 | 4513 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4514 | `	if( pArray == 0 ){` |
|     ! 0 | 4515 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4516 | `		return PH7_OK;` |
|       - | 4517 | `	}` |
|       - | 4518 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4519 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4520 | `	/* Perform the diff */` |
|       9 | 4521 | `	pEntry = pSrc->pFirst;` |
|       9 | 4522 | `	n = pSrc->nEntry;` |
|      20 | 4523 | `	for(;;){` |
|       - | 4524 | `		int keep;` |
|      25 | 4525 | `		if( n < 1 ){` |
|       9 | 4526 | `			break;` |
|       - | 4527 | `		}` |
|      17 | 4528 | `		keep = 1;` |
|      29 | 4529 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4530 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4531 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4532 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4533 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4534 | `			while( pIt ){` |
|       - | 4535 | `				/* build temporary key values for callback */` |
|       - | 4536 | `				ph7_value key1, key2, result;` |
|       - | 4537 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4538 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4539 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4540 | `				}else{` |
|       - | 4541 | `					SyString sStr;` |
|      31 | 4542 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4543 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4544 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4545 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4546 | `				}` |
|      31 | 4547 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4548 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4549 | `				}else{` |
|       - | 4550 | `					SyString sStr;` |
|      31 | 4551 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4552 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4553 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4554 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4555 | `				}` |
|      31 | 4556 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4557 | `				/* call user callback with (key1, key2) */` |
|       - | 4558 | `				{` |
|       - | 4559 | `					ph7_value *apK[2];` |
|      31 | 4560 | `					apK[0] = &key1;` |
|      31 | 4561 | `					apK[1] = &key2;` |
|      31 | 4562 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4563 | `				}` |
|      31 | 4564 | `				if( rc == SXRET_OK ){` |
|      31 | 4565 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4566 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4567 | `					}` |
|      31 | 4568 | `					if( result.x.iVal == 0 ){` |
|       - | 4569 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4570 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4571 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4572 | `						if( pVal1 && pVal2 ){` |
|      13 | 4573 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4574 | `								keep = 0;` |
|       9 | 4575 | `								PH7_MemObjRelease(&result);` |
|       - | 4576 | `								/* release keys too before breaking */` |
|       9 | 4577 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4578 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4579 | `								break;` |
|       - | 4580 | `							}` |
|       2 | 4581 | `						}` |
|       2 | 4582 | `					}` |
|      11 | 4583 | `				}` |
|      23 | 4584 | `				PH7_MemObjRelease(&result);` |
|      23 | 4585 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4586 | `				PH7_MemObjRelease(&key2);` |
|       - | 4587 | `				/* move to next node */` |
|      23 | 4588 | `				pIt = pIt->pPrev;` |
|      23 | 4589 | `				if( keep == 0 ) break;` |
|       1 | 4590 | `			}` |
|      21 | 4591 | `			if( keep == 0 ) break;` |
|       7 | 4592 | `		}` |
|      17 | 4593 | `		if( keep ){` |
|       - | 4594 | `			/* Perform the insertion */` |
|       9 | 4595 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4596 | `		}` |
|       - | 4597 | `		/* Point to the next entry */` |
|      17 | 4598 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4599 | `		n--;` |
|       1 | 4600 | `	}` |
|       - | 4601 | `	/* Return the freshly created array */` |
|       9 | 4602 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4603 | `	return PH7_OK;` |
|      13 | 4604 |  |
|       - | 4605 | `/*` |
|       - | 4606 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4607 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4608 | ` * Parameters` |
|       - | 4609 | ` *  $array1` |
|       - | 4610 | ` *    The array to compare from` |
|       - | 4611 | ` *  $array2` |
|       - | 4612 | ` *    An array to compare against` |
|       - | 4613 | ` *  $...` |
|       - | 4614 | ` *   More arrays to compare against` |
|       - | 4615 | ` * Return` |
|       - | 4616 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4617 | ` *  in any of the other arrays.` |
|       - | 4618 | ` * Note that NULL is returned on failure.` |
|       - | 4619 | ` */` |
|      14 | 4620 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4621 |  |
|       - | 4622 | `	ph7_hashmap_node *pEntry;` |
|       - | 4623 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4624 | `	ph7_value *pArray;` |
|       - | 4625 | `	sxi32 rc;` |
|       - | 4626 | `	sxu32 n;` |
|       - | 4627 | `	int i;` |
|       - | 4628 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4629 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4630 | `	 * helpers. */` |
|      16 | 4631 | `	if( nArg < 1 ){` |
|       4 | 4632 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4633 | `			"ArgumentCountError",` |
|       - | 4634 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4635 | `			nArg` |
|       - | 4636 | `			);` |
|       - | 4637 | `	}` |
|      14 | 4638 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4639 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4640 | `			"TypeError",` |
|       - | 4641 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4642 | `			ph7_type_name(apArg[0])` |
|       - | 4643 | `			);` |
|       - | 4644 | `	}` |
|      20 | 4645 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4646 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4647 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4648 | `				"TypeError",` |
|       - | 4649 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4650 | `				i + 1,` |
|       2 | 4651 | `				ph7_type_name(apArg[i])` |
|       - | 4652 | `				);` |
|       - | 4653 | `		}` |
|       5 | 4654 | `	}` |
|       9 | 4655 | `	if( nArg == 1 ){` |
|       - | 4656 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4657 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4658 | `		return PH7_OK;` |
|       - | 4659 | `	}` |
|       - | 4660 | `	/* Create a new array */` |
|       7 | 4661 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4662 | `	if( pArray == 0 ){` |
|     ! 0 | 4663 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4664 | `		return PH7_OK;` |
|       - | 4665 | `	}` |
|       - | 4666 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4667 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4668 | `	/* Perfrom the diff */` |
|       7 | 4669 | `	pEntry = pSrc->pFirst;` |
|       7 | 4670 | `	n = pSrc->nEntry;` |
|      12 | 4671 | `	for(;;){` |
|      25 | 4672 | `		if( n < 1 ){` |
|       7 | 4673 | `			break;` |
|       - | 4674 | `		}` |
|      31 | 4675 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4676 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4677 | `				/* ignore */` |
|     ! 0 | 4678 | `				continue;` |
|       - | 4679 | `			}` |
|      23 | 4680 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4681 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4682 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4683 | `				/* Blob lookup */` |
|      17 | 4684 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4685 | `			}else{` |
|       - | 4686 | `				/* Int lookup */` |
|       7 | 4687 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4688 | `			}` |
|      23 | 4689 | `			if( rc == SXRET_OK ){` |
|       - | 4690 | `				/* Key exists,break immediately */` |
|      11 | 4691 | `				break;` |
|       - | 4692 | `			}` |
|       7 | 4693 | `		}` |
|      19 | 4694 | `		if( i >= nArg ){` |
|       - | 4695 | `			/* Perform the insertion */` |
|       9 | 4696 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4697 | `		}` |
|       - | 4698 | `		/* Point to the next entry */` |
|      19 | 4699 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4700 | `		n--;` |
|       1 | 4701 | `	}` |
|       - | 4702 | `	/* Return the freshly created array */` |
|       7 | 4703 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4704 | `	return PH7_OK;` |
|       9 | 4705 |  |
|       - | 4706 | `/*` |
|       - | 4707 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4708 | ` *  Computes the intersection of arrays.` |
|       - | 4709 | ` * Parameters` |
|       - | 4710 | ` *  $array1` |
|       - | 4711 | ` *    The array to compare from` |
|       - | 4712 | ` *  $array2` |
|       - | 4713 | ` *    An array to compare against` |
|       - | 4714 | ` *  $...` |
|       - | 4715 | ` *   More arrays to compare against` |
|       - | 4716 | ` * Return` |
|       - | 4717 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4718 | ` *  in all of the parameters.` |
|       - | 4719 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4720 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4721 | ` */` |
|      22 | 4722 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4723 |  |
|       - | 4724 | `	ph7_hashmap_node *pEntry;` |
|       - | 4725 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4726 | `	ph7_value *pArray;` |
|       - | 4727 | `	ph7_value *pVal;` |
|       - | 4728 | `	sxi32 rc;` |
|       - | 4729 | `	sxu32 n;` |
|       - | 4730 | `	int i;` |
|      24 | 4731 | `	if( nArg < 1 ){` |
|       4 | 4732 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4733 | `			"ArgumentCountError",` |
|       - | 4734 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4735 | `			nArg` |
|       - | 4736 | `			);` |
|       - | 4737 | `	}` |
|      22 | 4738 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4739 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4740 | `			"TypeError",` |
|       - | 4741 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4742 | `			ph7_type_name(apArg[0])` |
|       - | 4743 | `			);` |
|       - | 4744 | `	}` |
|      36 | 4745 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4746 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4747 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4748 | `				"TypeError",` |
|       - | 4749 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4750 | `				i + 1,` |
|       2 | 4751 | `				ph7_type_name(apArg[i])` |
|       - | 4752 | `				);` |
|       - | 4753 | `		}` |
|       9 | 4754 | `	}` |
|      17 | 4755 | `	if( nArg == 1 ){` |
|       - | 4756 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4757 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4758 | `		return PH7_OK;` |
|       - | 4759 | `	}` |
|       - | 4760 | `	/* Create a new array */` |
|      15 | 4761 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4762 | `	if( pArray == 0 ){` |
|     ! 0 | 4763 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4764 | `		return PH7_OK;` |
|       - | 4765 | `	}` |
|       - | 4766 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4767 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4768 | `	/* Perform the intersection */` |
|      15 | 4769 | `	pEntry = pSrc->pFirst;` |
|      15 | 4770 | `	n = pSrc->nEntry;` |
|      31 | 4771 | `	for(;;){` |
|      63 | 4772 | `		if( n < 1 ){` |
|      15 | 4773 | `			break;` |
|       - | 4774 | `		}` |
|       - | 4775 | `		/* Extract the node value */` |
|      49 | 4776 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4777 | `		if( pVal ){` |
|      79 | 4778 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4779 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4780 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4781 | `				/* Perform the lookup */` |
|      55 | 4782 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4783 | `				if( rc != SXRET_OK ){` |
|       - | 4784 | `					/* Value does not exist */` |
|      25 | 4785 | `					break;` |
|       - | 4786 | `				}` |
|      16 | 4787 | `			}` |
|      49 | 4788 | `			if( i >= nArg ){` |
|       - | 4789 | `				/* Perform the insertion */` |
|      25 | 4790 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4791 | `			}` |
|      24 | 4792 | `		}` |
|       - | 4793 | `		/* Point to the next entry */` |
|      49 | 4794 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4795 | `		n--;` |
|       1 | 4796 | `	}` |
|       - | 4797 | `	/* Return the freshly created array */` |
|      15 | 4798 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4799 | `	return PH7_OK;` |
|      13 | 4800 |  |
|       - | 4801 | `/*` |
|       - | 4802 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4803 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4804 | ` * Parameters` |
|       - | 4805 | ` *  $array1` |
|       - | 4806 | ` *    The array to compare from` |
|       - | 4807 | ` *  $array2` |
|       - | 4808 | ` *    An array to compare against` |
|       - | 4809 | ` *  $...` |
|       - | 4810 | ` *   More arrays to compare against` |
|       - | 4811 | ` * Return` |
|       - | 4812 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4813 | ` *  in all the arguments, with matching keys.` |
|       - | 4814 | ` */` |
|      22 | 4815 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4816 |  |
|       - | 4817 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4818 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4819 | `	ph7_value *pArray;` |
|       - | 4820 | `	ph7_value *pVal;` |
|       - | 4821 | `	sxi32 rc;` |
|       - | 4822 | `	sxu32 n;` |
|       - | 4823 | `	int i;` |
|      24 | 4824 | `	if( nArg < 1 ){` |
|       4 | 4825 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4826 | `			"ArgumentCountError",` |
|       - | 4827 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4828 | `			nArg` |
|       - | 4829 | `			);` |
|       - | 4830 | `	}` |
|      22 | 4831 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4832 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4833 | `			"TypeError",` |
|       - | 4834 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4835 | `			ph7_type_name(apArg[0])` |
|       - | 4836 | `			);` |
|       - | 4837 | `	}` |
|      36 | 4838 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4839 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4840 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4841 | `				"TypeError",` |
|       - | 4842 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4843 | `				i + 1,` |
|       2 | 4844 | `				ph7_type_name(apArg[i])` |
|       - | 4845 | `				);` |
|       - | 4846 | `		}` |
|       9 | 4847 | `	}` |
|      17 | 4848 | `	if( nArg == 1 ){` |
|       - | 4849 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4850 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4851 | `		return PH7_OK;` |
|       - | 4852 | `	}` |
|       - | 4853 | `	/* Create a new array */` |
|      15 | 4854 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4855 | `	if( pArray == 0 ){` |
|     ! 0 | 4856 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4857 | `		return PH7_OK;` |
|       - | 4858 | `	}` |
|       - | 4859 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4860 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4861 | `	/* Perform the intersection */` |
|      15 | 4862 | `	pEntry = pSrc->pFirst;` |
|      15 | 4863 | `	n = pSrc->nEntry;` |
|      15 | 4864 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4865 | `	for(;;){` |
|      47 | 4866 | `		if( n < 1 ){` |
|      15 | 4867 | `			break;` |
|       - | 4868 | `		}` |
|       - | 4869 | `		/* Extract the node value */` |
|      33 | 4870 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4871 | `		if( pVal ){` |
|      53 | 4872 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4873 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4874 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4875 | `				/* Perform a key lookup first */` |
|      37 | 4876 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4877 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4878 | `				}else{` |
|      23 | 4879 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4880 | `				}` |
|      37 | 4881 | `				if( rc != SXRET_OK ){` |
|       - | 4882 | `					/* No such key,break immediately */` |
|       7 | 4883 | `					break;` |
|       - | 4884 | `				}` |
|       - | 4885 | `				/* Perform the lookup */` |
|      31 | 4886 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4887 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4888 | `					/* Value does not exist */` |
|       6 | 4889 | `					break;` |
|       - | 4890 | `				}` |
|      11 | 4891 | `			}` |
|      33 | 4892 | `			if( i >= nArg ){` |
|       - | 4893 | `				/* Perform the insertion */` |
|      17 | 4894 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4895 | `			}` |
|      16 | 4896 | `		}` |
|       - | 4897 | `		/* Point to the next entry */` |
|      33 | 4898 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4899 | `		n--;` |
|       1 | 4900 | `	}` |
|       - | 4901 | `	/* Return the freshly created array */` |
|      15 | 4902 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4903 | `	return PH7_OK;` |
|      13 | 4904 |  |
|       - | 4905 | `/*` |
|       - | 4906 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 4907 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4908 | ` * Parameters` |
|       - | 4909 | ` *  $array1` |
|       - | 4910 | ` *    The array to compare from` |
|       - | 4911 | ` *  $...` |
|       - | 4912 | ` *   More arrays to compare against` |
|       - | 4913 | ` * Return` |
|       - | 4914 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4915 | ` *  have keys that are present in all arguments.` |
|       - | 4916 | ` * Note that NULL is returned on failure.` |
|       - | 4917 | ` */` |
|      22 | 4918 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4919 |  |
|       - | 4920 | `	ph7_hashmap_node *pEntry;` |
|       - | 4921 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4922 | `	ph7_value *pArray;` |
|       - | 4923 | `	sxi32 rc;` |
|       - | 4924 | `	sxu32 n;` |
|       - | 4925 | `	int i;` |
|      24 | 4926 | `	if( nArg < 1 ){` |
|       4 | 4927 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4928 | `			"ArgumentCountError",` |
|       - | 4929 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 4930 | `			nArg` |
|       - | 4931 | `			);` |
|       - | 4932 | `	}` |
|      22 | 4933 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4934 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4935 | `			"TypeError",` |
|       - | 4936 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4937 | `			ph7_type_name(apArg[0])` |
|       - | 4938 | `			);` |
|       - | 4939 | `	}` |
|      36 | 4940 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4941 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4942 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4943 | `				"TypeError",` |
|       - | 4944 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4945 | `				i + 1,` |
|       2 | 4946 | `				ph7_type_name(apArg[i])` |
|       - | 4947 | `				);` |
|       - | 4948 | `		}` |
|       9 | 4949 | `	}` |
|      17 | 4950 | `	if( nArg == 1 ){` |
|       - | 4951 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4952 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4953 | `		return PH7_OK;` |
|       - | 4954 | `	}` |
|       - | 4955 | `	/* Create a new array */` |
|      15 | 4956 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4957 | `	if( pArray == 0 ){` |
|     ! 0 | 4958 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4959 | `		return PH7_OK;` |
|       - | 4960 | `	}` |
|       - | 4961 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 4962 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4963 | `	/* Perform the intersection */` |
|      15 | 4964 | `	pEntry = pSrc->pFirst;` |
|      15 | 4965 | `	n = pSrc->nEntry;` |
|      24 | 4966 | `	for(;;){` |
|      49 | 4967 | `		if( n < 1 ){` |
|      15 | 4968 | `			break;` |
|       - | 4969 | `		}` |
|      57 | 4970 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 4971 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 4972 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 4973 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4974 | `				/* Blob lookup */` |
|      27 | 4975 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 4976 | `			}else{` |
|       - | 4977 | `				/* Int key */` |
|      13 | 4978 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4979 | `			}` |
|      39 | 4980 | `			if( rc != SXRET_OK ){` |
|       - | 4981 | `				/* Key does not exist, break immediately */` |
|      17 | 4982 | `				break;` |
|       - | 4983 | `			}` |
|      12 | 4984 | `		}` |
|      35 | 4985 | `		if( i >= nArg ){` |
|       - | 4986 | `			/* Perform the insertion */` |
|      19 | 4987 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 4988 | `		}` |
|       - | 4989 | `		/* Point to the next entry */` |
|      35 | 4990 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 4991 | `		n--;` |
|       1 | 4992 | `	}` |
|       - | 4993 | `	/* Return the freshly created array */` |
|      15 | 4994 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4995 | `	return PH7_OK;` |
|      13 | 4996 |  |
|       - | 4997 | `/*` |
|       - | 4998 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4999 | ` *  Computes the intersection of arrays.` |
|       - | 5000 | ` * Parameters` |
|       - | 5001 | ` *  $array1` |
|       - | 5002 | ` *    The array to compare from` |
|       - | 5003 | ` *  $array2` |
|       - | 5004 | ` *    An array to compare against` |
|       - | 5005 | ` *  $...` |
|       - | 5006 | ` *   More arrays to compare against` |
|       - | 5007 | ` * $callback` |
|       - | 5008 | ` *  The callback comparison function.` |
|       - | 5009 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5010 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5011 | ` *  than the second.` |
|       - | 5012 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5013 | ` * Return` |
|       - | 5014 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5015 | ` *  in all of the parameters. .` |
|       - | 5016 | ` * Note that NULL is returned on failure.` |
|       - | 5017 | ` */` |
|      24 | 5018 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5019 |  |
|       - | 5020 | `	ph7_hashmap_node *pEntry;` |
|       - | 5021 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5022 | `	ph7_value *pCallback;` |
|       - | 5023 | `	ph7_value *pArray;` |
|       - | 5024 | `	ph7_value *pVal;` |
|       - | 5025 | `	sxi32 rc;` |
|       - | 5026 | `	sxu32 n;` |
|       - | 5027 | `	int i;` |
|       - | 5028 |  |
|       - | 5029 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      26 | 5030 | `	if( nArg < 2 ){` |
|       4 | 5031 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5032 | `			"ArgumentCountError",` |
|       - | 5033 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5034 | `			nArg` |
|       - | 5035 | `			);` |
|       - | 5036 | `	}` |
|      24 | 5037 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5038 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5039 | `			"TypeError",` |
|       - | 5040 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5041 | `			ph7_type_name(apArg[0])` |
|       - | 5042 | `			);` |
|       - | 5043 | `	}` |
|       - | 5044 |  |
|      22 | 5045 | `	if( nArg == 2 ){` |
|       - | 5046 | `		/* Only the original array and the callback were provided. */` |
|       - | 5047 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5048 | `		 * validation ordering. */` |
|       3 | 5049 | `	} else {` |
|       - | 5050 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      32 | 5051 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      18 | 5052 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5053 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5054 | `					"TypeError",` |
|       - | 5055 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5056 | `					i + 1,` |
|       2 | 5057 | `					ph7_type_name(apArg[i])` |
|       - | 5058 | `					);` |
|       - | 5059 | `			}` |
|       9 | 5060 | `		}` |
|       - | 5061 | `	}` |
|       - | 5062 |  |
|       - | 5063 | `	/* Identify the callback (always expected as the last argument). */` |
|      20 | 5064 | `	pCallback = apArg[nArg - 1];` |
|       - | 5065 | `	/* Validate the callback to match PHP's error messages. */` |
|      20 | 5066 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      11 | 5067 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5068 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5069 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5070 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5071 | `			 */` |
|       7 | 5072 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       7 | 5073 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5074 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5075 | `					"TypeError",` |
|       - | 5076 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5077 | `					nArg` |
|       - | 5078 | `					);` |
|       - | 5079 | `			}` |
|       - | 5080 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5081 | `			{` |
|       5 | 5082 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       5 | 5083 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       5 | 5084 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5085 | `					int nMethodLen;` |
|       5 | 5086 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       5 | 5087 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       5 | 5088 | `					if( pClass ){` |
|       - | 5089 | `						/* Class exists but method is missing. */` |
|       4 | 5090 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5091 | `							"TypeError",` |
|       - | 5092 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5093 | `							nArg,` |
|       1 | 5094 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5095 | `							zMethod` |
|       - | 5096 | `							);` |
|       - | 5097 | `					}` |
|       - | 5098 | `					/* Class not found */` |
|       - | 5099 | `					{` |
|       - | 5100 | `						int nName;` |
|       3 | 5101 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5102 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5103 | `							"TypeError",` |
|       - | 5104 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5105 | `							nArg,` |
|       1 | 5106 | `							zName` |
|       - | 5107 | `							);` |
|       - | 5108 | `					}` |
|       - | 5109 | `				}` |
|       - | 5110 | `			}` |
|       - | 5111 | `			/* Fallback message */` |
|     ! 0 | 5112 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5113 | `				"TypeError",` |
|       - | 5114 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5115 | `				nArg` |
|       - | 5116 | `				);` |
|       - | 5117 | `		}` |
|       5 | 5118 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5119 | `			int len;` |
|       3 | 5120 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5121 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5122 | `				"TypeError",` |
|       - | 5123 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5124 | `				nArg,` |
|       1 | 5125 | `				zName` |
|       - | 5126 | `				);` |
|       - | 5127 | `		}` |
|       4 | 5128 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5129 | `			"TypeError",` |
|       - | 5130 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5131 | `			nArg` |
|       - | 5132 | `			);` |
|       - | 5133 | `	}` |
|       - | 5134 |  |
|       9 | 5135 | `	if( nArg == 2 ){` |
|       - | 5136 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5137 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5138 | `		return PH7_OK;` |
|       - | 5139 | `	}` |
|       - | 5140 |  |
|       - | 5141 | `	/* Create a new array */` |
|       5 | 5142 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 5143 | `	if( pArray == 0 ){` |
|     ! 0 | 5144 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5145 | `		return PH7_OK;` |
|       - | 5146 | `	}` |
|       - | 5147 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 5148 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5149 | `	/* Perform the intersection */` |
|       5 | 5150 | `	pEntry = pSrc->pFirst;` |
|       5 | 5151 | `	n = pSrc->nEntry;` |
|       8 | 5152 | `	for(;;){` |
|      17 | 5153 | `		if( n < 1 ){` |
|       5 | 5154 | `			break;` |
|       - | 5155 | `		}` |
|       - | 5156 | `		/* Extract the node value */` |
|      13 | 5157 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      13 | 5158 | `		if( pVal ){` |
|      21 | 5159 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      13 | 5160 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5161 | `					/* ignore */` |
|     ! 0 | 5162 | `					continue;` |
|       - | 5163 | `				}` |
|       - | 5164 | `				/* Point to the internal representation of the hashmap */` |
|      13 | 5165 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5166 | `				/* Perform the lookup */` |
|      13 | 5167 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      13 | 5168 | `				if( rc != SXRET_OK ){` |
|       - | 5169 | `					/* Value does not exist */` |
|       5 | 5170 | `					break;` |
|       - | 5171 | `				}` |
|       5 | 5172 | `			}` |
|      13 | 5173 | `			if( i >= (nArg-1) ){` |
|       - | 5174 | `				/* Perform the insertion */` |
|       9 | 5175 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5176 | `			}` |
|       6 | 5177 | `		}` |
|       - | 5178 | `		/* Point to the next entry */` |
|      13 | 5179 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5180 | `		n--;` |
|       1 | 5181 | `	}` |
|       - | 5182 | `	/* Return the freshly created array */` |
|       5 | 5183 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5184 | `	return PH7_OK;` |
|      14 | 5185 |  |
|       - | 5186 | `/*` |
|       - | 5187 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5188 | ` *  Fill an array with values.` |
|       - | 5189 | ` * Parameters` |
|       - | 5190 | ` *  $start_index` |
|       - | 5191 | ` *    The first index of the returned array.` |
|       - | 5192 | ` *  $num` |
|       - | 5193 | ` *   Number of elements to insert.` |
|       - | 5194 | ` *  $value` |
|       - | 5195 | ` *    Value to use for filling.` |
|       - | 5196 | ` * Return` |
|       - | 5197 | ` *  The filled array or null on failure.` |
|       - | 5198 | ` */` |
|     238 | 5199 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5200 |  |
|       - | 5201 | `	ph7_value *pArray;` |
|       - | 5202 | `	int i,nEntry;` |
|       - | 5203 |  |
|       - | 5204 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 5205 | `	if( nArg != 3 ){` |
|       - | 5206 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 5207 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5208 | `			"ArgumentCountError",` |
|       - | 5209 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5210 | `			nArg` |
|       - | 5211 | `			);` |
|       - | 5212 | `	}` |
|       - | 5213 |  |
|       - | 5214 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5215 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5216 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5217 | `	 * and NULLs are rejected outright. */` |
|     466 | 5218 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 5219 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5220 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5221 | `			"TypeError",` |
|       - | 5222 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5223 | `			ph7_type_name(apArg[0])` |
|       - | 5224 | `			);` |
|       - | 5225 | `	}` |
|     234 | 5226 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5227 | `		int len;` |
|       8 | 5228 | `		sxu8 bReal = FALSE;` |
|       8 | 5229 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5230 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5231 | `			/* Non‑numeric string is an error. */` |
|       3 | 5232 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5233 | `				"TypeError",` |
|       - | 5234 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5235 | `				);` |
|       - | 5236 | `		}` |
|       5 | 5237 | `		if( bReal ){` |
|       - | 5238 | `			/* float-string -> deprecation warning */` |
|       4 | 5239 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5240 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5241 | `				zStr` |
|       - | 5242 | `				);` |
|       1 | 5243 | `		}` |
|       2 | 5244 | `	}` |
|       - | 5245 |  |
|       - | 5246 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5247 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 5248 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 5249 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5250 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5251 | `			"TypeError",` |
|       - | 5252 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5253 | `			ph7_type_name(apArg[1])` |
|       - | 5254 | `			);` |
|       - | 5255 | `	}` |
|     232 | 5256 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5257 | `		int len;` |
|       3 | 5258 | `		sxu8 bReal = FALSE;` |
|       3 | 5259 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5260 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5261 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5262 | `				"TypeError",` |
|       - | 5263 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5264 | `				);` |
|       - | 5265 | `		}` |
|     ! 0 | 5266 | `	}` |
|       - | 5267 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5268 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5269 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5270 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5271 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5272 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5273 | `		if( d != (double)i64 ){` |
|       7 | 5274 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5275 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5276 | `				d` |
|       - | 5277 | `				);` |
|       2 | 5278 | `		}` |
|       2 | 5279 | `	}` |
|       - | 5280 |  |
|       - | 5281 | `	/* Total number of entries to insert */` |
|     230 | 5282 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5283 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5284 | `	if( nEntry < 0 ){` |
|       3 | 5285 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5286 | `			"ValueError",` |
|       - | 5287 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5288 | `			);` |
|       - | 5289 | `	}` |
|       - | 5290 |  |
|       - | 5291 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5292 | `	if( nEntry == 0 ){` |
|       7 | 5293 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5294 | `		return PH7_OK;` |
|       - | 5295 | `	}` |
|       - | 5296 |  |
|       - | 5297 | `	/* Create a new array */` |
|     221 | 5298 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5299 | `	if( pArray == 0 ){` |
|     ! 0 | 5300 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5301 | `		return PH7_OK;` |
|       - | 5302 | `	}` |
|       - | 5303 |  |
|       - | 5304 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5305 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 5306 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5307 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5308 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 5309 | `	}` |
|       - | 5310 | `	/* Return the filled array */` |
|     221 | 5311 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5312 | `	return PH7_OK;` |
|     121 | 5313 |  |
|       - | 5314 | `/*` |
|       - | 5315 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5316 | ` *  Fill an array with values, specifying keys.` |
|       - | 5317 | ` * Parameters` |
|       - | 5318 | ` *  $input` |
|       - | 5319 | ` *   Array of values that will be used as key.` |
|       - | 5320 | ` *  $value` |
|       - | 5321 | ` *    Value to use for filling.` |
|       - | 5322 | ` * Return` |
|       - | 5323 | ` *  The filled array.` |
|       - | 5324 | ` * Throws` |
|       - | 5325 | ` *  ValueError if $input is not an array.` |
|       - | 5326 | ` */` |
|      26 | 5327 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5328 |  |
|       - | 5329 | `	ph7_hashmap_node *pEntry;` |
|       - | 5330 | `	ph7_hashmap *pSrc;` |
|       - | 5331 | `	ph7_value *pArray;` |
|       - | 5332 | `	sxu32 n;` |
|       - | 5333 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5334 | `	if( nArg != 2 ){` |
|      10 | 5335 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5336 | `			"ArgumentCountError",` |
|       - | 5337 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5338 | `			nArg` |
|       - | 5339 | `			);` |
|       - | 5340 | `	}` |
|       - | 5341 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5342 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5343 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5344 | `			"TypeError",` |
|       - | 5345 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5346 | `			ph7_type_name(apArg[0])` |
|       - | 5347 | `			);` |
|       - | 5348 | `	}` |
|       - | 5349 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5350 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5351 | `	/* Create a new array */` |
|      17 | 5352 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5353 | `	if( pArray == 0 ){` |
|     ! 0 | 5354 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5355 | `		return PH7_OK;` |
|       - | 5356 | `	}` |
|       - | 5357 | `	/* Perform the requested operation */` |
|      17 | 5358 | `	pEntry = pSrc->pFirst;` |
|      45 | 5359 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5360 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5361 | `		/* Point to the next entry */` |
|      29 | 5362 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5363 | `	}` |
|       - | 5364 | `	/* Return the filled array */` |
|      17 | 5365 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5366 | `	return PH7_OK;` |
|      15 | 5367 |  |
|       - | 5368 | `/*` |
|       - | 5369 | ` * array array_combine(array $keys,array $values)` |
|       - | 5370 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5371 | ` * Parameters` |
|       - | 5372 | ` *  $keys` |
|       - | 5373 | ` *    Array of keys to be used.` |
|       - | 5374 | ` * $values` |
|       - | 5375 | ` *   Array of values to be used.` |
|       - | 5376 | ` * Return` |
|       - | 5377 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5378 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5379 | ` *  not an array.` |
|       - | 5380 | ` */` |
|      18 | 5381 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5382 |  |
|       - | 5383 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5384 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5385 | `	ph7_value *pArray;` |
|       - | 5386 | `	sxu32 n;` |
|       - | 5387 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5388 | `	if( nArg != 2 ){` |
|       - | 5389 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5390 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5391 | `			"ArgumentCountError",` |
|       - | 5392 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5393 | `			nArg` |
|       - | 5394 | `			);` |
|       - | 5395 | `	}` |
|       - | 5396 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5397 | `	 * argument index in the error message. */` |
|      18 | 5398 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5399 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5400 | `			"TypeError",` |
|       - | 5401 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5402 | `			ph7_type_name(apArg[0])` |
|       - | 5403 | `			);` |
|       - | 5404 | `	}` |
|      16 | 5405 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5406 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5407 | `			"TypeError",` |
|       - | 5408 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5409 | `			ph7_type_name(apArg[1])` |
|       - | 5410 | `			);` |
|       - | 5411 | `	}` |
|       - | 5412 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5413 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5414 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5415 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5416 | `		/* Length mismatch -> ValueError */` |
|       3 | 5417 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5418 | `			"ValueError",` |
|       - | 5419 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5420 | `			);` |
|       - | 5421 | `	}` |
|       - | 5422 | `	/* Create a new array */` |
|      11 | 5423 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5424 | `	if( pArray == 0 ){` |
|     ! 0 | 5425 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5426 | `		return PH7_OK;` |
|       - | 5427 | `	}` |
|       - | 5428 | `	/* Perform the requested operation */` |
|      11 | 5429 | `	pKe = pKey->pFirst;` |
|      11 | 5430 | `	pVe = pValue->pFirst;` |
|      33 | 5431 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5432 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5433 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5434 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5435 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5436 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5437 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5438 | `		 * original array must not be mutated. */` |
|      23 | 5439 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5440 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5441 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5442 | `			if( pTmpKey ){` |
|       5 | 5443 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5444 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5445 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5446 | `				pKeyCopy = pTmpKey;` |
|       2 | 5447 | `			}` |
|       2 | 5448 | `		}` |
|      23 | 5449 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5450 | `		/* Point to the next entry */` |
|      23 | 5451 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5452 | `		pVe = pVe->pPrev;` |
|      12 | 5453 | `	}` |
|       - | 5454 | `	/* Return the filled array */` |
|      11 | 5455 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5456 | `	return PH7_OK;` |
|      11 | 5457 |  |
|       - | 5458 | `/*` |
|       - | 5459 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5460 | ` *  Return an array with elements in reverse order.` |
|       - | 5461 | ` * Parameters` |
|       - | 5462 | ` *  $array` |
|       - | 5463 | ` *   The input array.` |
|       - | 5464 | ` *  $preserve_keys (optional)` |
|       - | 5465 | ` *   If set to TRUE keys are preserved.` |
|       - | 5466 | ` * Return` |
|       - | 5467 | ` *  The reversed array.` |
|       - | 5468 | ` */` |
|      20 | 5469 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5470 |  |
|       - | 5471 | `	ph7_hashmap_node *pEntry;` |
|       - | 5472 | `	ph7_hashmap *pSrc;` |
|       - | 5473 | `	ph7_value *pArray;` |
|       - | 5474 | `	int bPreserve;` |
|       - | 5475 | `	sxu32 n;` |
|      22 | 5476 | `	if( nArg < 1 ){` |
|       4 | 5477 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5478 | `			"ArgumentCountError",` |
|       - | 5479 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5480 | `			nArg` |
|       - | 5481 | `			);` |
|       - | 5482 | `	}` |
|       - | 5483 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5484 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5485 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5486 | `			"TypeError",` |
|       - | 5487 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5488 | `			ph7_type_name(apArg[0])` |
|       - | 5489 | `			);` |
|       - | 5490 | `	}` |
|      17 | 5491 | `	bPreserve = FALSE;` |
|      17 | 5492 | `	if( nArg > 1 ){` |
|       7 | 5493 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5494 | `	}` |
|       - | 5495 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5496 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5497 | `	/* Create a new array */` |
|      17 | 5498 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5499 | `	if( pArray == 0 ){` |
|     ! 0 | 5500 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5501 | `		return PH7_OK;` |
|       - | 5502 | `	}` |
|       - | 5503 | `	/* Perform the requested operation */` |
|      17 | 5504 | `	pEntry = pSrc->pLast;` |
|      55 | 5505 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5506 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5507 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5508 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5509 | `		/* Point to the previous entry */` |
|      39 | 5510 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5511 | `	}` |
|      17 | 5512 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5513 | `	return PH7_OK;` |
|      12 | 5514 |  |
|       - | 5515 | `/*` |
|       - | 5516 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5517 | ` *  Removes duplicate values from an array.` |
|       - | 5518 | ` * Parameters` |
|       - | 5519 | ` *  $array` |
|       - | 5520 | ` *   The input array.` |
|       - | 5521 | ` *  $flags` |
|       - | 5522 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5523 | ` *   behavior using these values:` |
|       - | 5524 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5525 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5526 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5527 | ` * Return` |
|       - | 5528 | ` *  The filtered array.` |
|       - | 5529 | ` */` |
|      24 | 5530 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5531 |  |
|       - | 5532 | `	ph7_hashmap_node *pEntry;` |
|       - | 5533 | `	ph7_value *pNeedle;` |
|       - | 5534 | `	ph7_hashmap *pSrc;` |
|       - | 5535 | `	ph7_value *pArray;` |
|       - | 5536 | `	int bStrict;` |
|       - | 5537 | `	sxi32 rc;` |
|       - | 5538 | `	sxu32 n;` |
|      26 | 5539 | `	if( nArg < 1 ){` |
|       - | 5540 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5541 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5542 | `			"ArgumentCountError",` |
|       - | 5543 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5544 | `			);` |
|       - | 5545 | `	}` |
|      24 | 5546 | `	if( nArg > 2 ){` |
|       - | 5547 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5548 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5549 | `			"ArgumentCountError",` |
|       - | 5550 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5551 | `			nArg` |
|       - | 5552 | `			);` |
|       - | 5553 | `	}` |
|       - | 5554 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5555 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5556 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5557 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5558 | `			"TypeError",` |
|       - | 5559 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5560 | `			ph7_type_name(apArg[0])` |
|       - | 5561 | `			);` |
|       - | 5562 | `	}` |
|      19 | 5563 | `	bStrict = FALSE;` |
|       - | 5564 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5565 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5566 | `	/* Create a new array */` |
|      19 | 5567 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5568 | `	if( pArray == 0 ){` |
|     ! 0 | 5569 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5570 | `		return PH7_OK;` |
|       - | 5571 | `	}` |
|       - | 5572 | `	/* Perform the requested operation */` |
|      19 | 5573 | `	pEntry = pSrc->pFirst;` |
|      83 | 5574 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5575 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5576 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5577 | `		if( pNeedle ){` |
|      65 | 5578 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5579 | `		}` |
|      65 | 5580 | `		if( rc != SXRET_OK ){` |
|       - | 5581 | `			/* Perform the insertion */` |
|      37 | 5582 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5583 | `		}` |
|       - | 5584 | `		/* Point to the next entry */` |
|      65 | 5585 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5586 | `	}` |
|       - | 5587 | `	/* Return the freshly created array */` |
|      19 | 5588 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5589 | `	return PH7_OK;` |
|      14 | 5590 |  |
|       - | 5591 | `/*` |
|       - | 5592 | ` * array array_flip(array $input)` |
|       - | 5593 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5594 | ` * Parameter` |
|       - | 5595 | ` *  $input` |
|       - | 5596 | ` *   Input array.` |
|       - | 5597 | ` * Return` |
|       - | 5598 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5599 | ` */` |
|      34 | 5600 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5601 |  |
|       - | 5602 | `	ph7_hashmap_node *pEntry;` |
|       - | 5603 | `	ph7_hashmap *pSrc;` |
|       - | 5604 | `	ph7_value *pArray;` |
|       - | 5605 | `	ph7_value *pKey;` |
|       - | 5606 | `	ph7_value sVal;` |
|       - | 5607 | `	sxu32 n;` |
|       - | 5608 |  |
|       - | 5609 | `	/* PHP requires exactly one argument */` |
|      36 | 5610 | `	if( nArg != 1 ){` |
|       - | 5611 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5612 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5613 | `			"ArgumentCountError",` |
|       - | 5614 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5615 | `			nArg` |
|       - | 5616 | `			);` |
|       - | 5617 | `	}` |
|       - | 5618 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5619 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5620 | `		/* Type mismatch -> TypeError */` |
|       7 | 5621 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5622 | `			"TypeError",` |
|       - | 5623 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5624 | `			ph7_type_name(apArg[0])` |
|       - | 5625 | `			);` |
|       - | 5626 | `	}` |
|       - | 5627 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5628 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5629 | `	/* Create a new array */` |
|      27 | 5630 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5631 | `	if( pArray == 0 ){` |
|     ! 0 | 5632 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5633 | `		return PH7_OK;` |
|       - | 5634 | `	}` |
|       - | 5635 | `	/* Start processing */` |
|      27 | 5636 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5637 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5638 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5639 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5640 | `		if( pKey ){` |
|       - | 5641 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5642 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5643 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5644 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5645 | `					);` |
|   22236 | 5646 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5647 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5648 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5649 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5650 | `				}else{` |
|       - | 5651 | `					SyString sStr;` |
|    2227 | 5652 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5653 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5654 | `				}` |
|       - | 5655 | `				/* Perform the insertion */` |
|   22227 | 5656 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5657 | `				/* Safely release the value because each inserted entry` |
|       - | 5658 | `				 * has its own private copy of the value.` |
|       - | 5659 | `				 */` |
|   22227 | 5660 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5661 | `			}else{` |
|       - | 5662 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5663 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5664 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5665 | `					);` |
|       - | 5666 | `			}` |
|   11118 | 5667 | `		}` |
|       - | 5668 | `		/* Point to the next entry */` |
|   22237 | 5669 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5670 | `	}` |
|       - | 5671 | `	/* Return the freshly created array */` |
|      27 | 5672 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5673 | `	return PH7_OK;` |
|      19 | 5674 |  |
|       - | 5675 | `/*` |
|       - | 5676 | ` * number array_sum(array $array )` |
|       - | 5677 | ` *  Calculate the sum of values in an array.` |
|       - | 5678 | ` * Parameters` |
|       - | 5679 | ` *  $array: The input array.` |
|       - | 5680 | ` * Return` |
|       - | 5681 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5682 | ` */` |
|      24 | 5683 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5684 |  |
|       - | 5685 | `	ph7_hashmap_node *pEntry;` |
|       - | 5686 | `	ph7_value *pObj;` |
|      25 | 5687 | `	double dSum = 0;` |
|       - | 5688 | `	sxu32 n;` |
|      25 | 5689 | `	pEntry = pMap->pFirst;` |
|      91 | 5690 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5691 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5692 | `		if( pObj ){` |
|      67 | 5693 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5694 | `				dSum += pObj->rVal;` |
|      53 | 5695 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5696 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5697 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5698 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5699 | `					double dv = 0;` |
|      13 | 5700 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5701 | `					dSum += dv;` |
|       7 | 5702 | `				}` |
|      12 | 5703 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5704 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5705 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5706 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5707 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5708 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5709 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5710 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5711 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5712 | `			}` |
|       - | 5713 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5714 | `		}` |
|       - | 5715 | `		/* Point to the next entry */` |
|      67 | 5716 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5717 | `	}` |
|       - | 5718 | `	/* Return sum */` |
|      25 | 5719 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5720 |  |
|      26 | 5721 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5722 |  |
|       - | 5723 | `	ph7_hashmap_node *pEntry;` |
|       - | 5724 | `	ph7_value *pObj;` |
|      28 | 5725 | `	sxi64 nSum = 0;` |
|       - | 5726 | `	sxu32 n;` |
|      28 | 5727 | `	pEntry = pMap->pFirst;` |
|     112 | 5728 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      86 | 5729 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      86 | 5730 | `		if( pObj ){` |
|      86 | 5731 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      76 | 5732 | `				nSum += pObj->x.iVal;` |
|      48 | 5733 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5734 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5735 | `					sxi64 nv = 0;` |
|       5 | 5736 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5737 | `					nSum += nv;` |
|       3 | 5738 | `				}` |
|       8 | 5739 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5740 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5741 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5742 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5743 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5744 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5745 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5746 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5747 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5748 | `			}` |
|       - | 5749 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      42 | 5750 | `		}` |
|       - | 5751 | `		/* Point to the next entry */` |
|      86 | 5752 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      44 | 5753 | `	}` |
|       - | 5754 | `	/* Return sum */` |
|      28 | 5755 | `	ph7_result_int64(pCtx,nSum);` |
|      28 | 5756 |  |
|       - | 5757 | `/* number array_sum(array $array )` |
|       - | 5758 | ` * (See block-coment above)` |
|       - | 5759 | ` */` |
|      64 | 5760 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5761 |  |
|       - | 5762 | `	ph7_hashmap_node *pEntry;` |
|       - | 5763 | `	ph7_hashmap *pMap;` |
|       - | 5764 | `	ph7_value *pObj;` |
|      66 | 5765 | `	int useDouble = 0;` |
|       - | 5766 | `	sxu32 n;` |
|       - | 5767 | `	/* PHP requires exactly one argument */` |
|      66 | 5768 | `	if( nArg != 1 ){` |
|       7 | 5769 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5770 | `			"ArgumentCountError",` |
|       - | 5771 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5772 | `			nArg` |
|       - | 5773 | `			);` |
|       - | 5774 | `	}` |
|       - | 5775 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 5776 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5777 | `		/* Type mismatch -> TypeError */` |
|       7 | 5778 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5779 | `			"TypeError",` |
|       - | 5780 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5781 | `			ph7_type_name(apArg[0])` |
|       - | 5782 | `			);` |
|       - | 5783 | `	}` |
|      58 | 5784 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      58 | 5785 | `	if( pMap->nEntry < 1 ){` |
|       - | 5786 | `		/* Nothing to compute,return 0 */` |
|       7 | 5787 | `		ph7_result_int(pCtx,0);` |
|       7 | 5788 | `		return PH7_OK;` |
|       - | 5789 | `	}` |
|       - | 5790 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5791 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5792 | `	 */` |
|      52 | 5793 | `	pEntry = pMap->pFirst;` |
|     144 | 5794 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     118 | 5795 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     118 | 5796 | `		if( pObj ){` |
|     118 | 5797 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5798 | `				useDouble = 1;` |
|      19 | 5799 | `				break;` |
|       - | 5800 | `			}` |
|     100 | 5801 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5802 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5803 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5804 | `				sxu32 i;` |
|      23 | 5805 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5806 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5807 | `						useDouble = 1;` |
|       7 | 5808 | `						break;` |
|       - | 5809 | `					}` |
|       6 | 5810 | `				}` |
|      13 | 5811 | `				if( useDouble ){` |
|       7 | 5812 | `					break;` |
|       - | 5813 | `				}` |
|       3 | 5814 | `			}` |
|      46 | 5815 | `		}` |
|      94 | 5816 | `		pEntry = pEntry->pPrev;` |
|      48 | 5817 | `	}` |
|      52 | 5818 | `	if( useDouble ){` |
|      25 | 5819 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5820 | `	}else{` |
|      28 | 5821 | `		Int64Sum(pCtx,pMap);` |
|       - | 5822 | `	}` |
|      52 | 5823 | `	return PH7_OK;` |
|      34 | 5824 |  |
|       - | 5825 | `/*` |
|       - | 5826 | ` * number array_product(array $array )` |
|       - | 5827 | ` *  Calculate the product of values in an array.` |
|       - | 5828 | ` * Parameters` |
|       - | 5829 | ` *  $array: The input array.` |
|       - | 5830 | ` * Return` |
|       - | 5831 | ` *  Returns the product of values as an integer or float.` |
|       - | 5832 | ` */` |
|     ! 0 | 5833 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5834 |  |
|       - | 5835 | `	ph7_hashmap_node *pEntry;` |
|       - | 5836 | `	ph7_value *pObj;` |
|       - | 5837 | `	double dProd;` |
|       - | 5838 | `	sxu32 n;` |
|     ! 0 | 5839 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5840 | `	dProd = 1;` |
|     ! 0 | 5841 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5842 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5843 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5844 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5845 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5846 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5847 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5848 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5849 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5850 | `					double dv = 0;` |
|     ! 0 | 5851 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5852 | `					dProd *= dv;` |
|     ! 0 | 5853 | `				}` |
|     ! 0 | 5854 | `			}` |
|     ! 0 | 5855 | `		}` |
|       - | 5856 | `		/* Point to the next entry */` |
|     ! 0 | 5857 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5858 | `	}` |
|       - | 5859 | `	/* Return product */` |
|     ! 0 | 5860 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5861 |  |
|     ! 0 | 5862 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5863 |  |
|       - | 5864 | `	ph7_hashmap_node *pEntry;` |
|       - | 5865 | `	ph7_value *pObj;` |
|       - | 5866 | `	sxi64 nProd;` |
|       - | 5867 | `	sxu32 n;` |
|     ! 0 | 5868 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5869 | `	nProd = 1;` |
|     ! 0 | 5870 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5871 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5872 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5873 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5874 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5875 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5876 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5877 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5878 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5879 | `					sxi64 nv = 0;` |
|     ! 0 | 5880 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5881 | `					nProd *= nv;` |
|     ! 0 | 5882 | `				}` |
|     ! 0 | 5883 | `			}` |
|     ! 0 | 5884 | `		}` |
|       - | 5885 | `		/* Point to the next entry */` |
|     ! 0 | 5886 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5887 | `	}` |
|       - | 5888 | `	/* Return product */` |
|     ! 0 | 5889 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5890 |  |
|       - | 5891 | `/* number array_product(array $array )` |
|       - | 5892 | ` * (See block-block comment above)` |
|       - | 5893 | ` */` |
|     ! 0 | 5894 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5895 |  |
|       - | 5896 | `	ph7_hashmap *pMap;` |
|       - | 5897 | `	ph7_value *pObj;` |
|     ! 0 | 5898 | `	if( nArg < 1 ){` |
|       - | 5899 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5900 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5901 | `		return PH7_OK;` |
|       - | 5902 | `	}` |
|       - | 5903 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5904 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5905 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5906 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5907 | `		return PH7_OK;` |
|       - | 5908 | `	}` |
|     ! 0 | 5909 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5910 | `	if( pMap->nEntry < 1 ){` |
|       - | 5911 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5912 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5913 | `		return PH7_OK;` |
|       - | 5914 | `	}` |
|       - | 5915 | `	/* If the first element is of type float,then perform floating` |
|       - | 5916 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5917 | `	 */` |
|     ! 0 | 5918 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5919 | `	if( pObj == 0 ){` |
|     ! 0 | 5920 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5921 | `		return PH7_OK;` |
|       - | 5922 | `	}` |
|     ! 0 | 5923 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5924 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5925 | `	}else{` |
|     ! 0 | 5926 | `		Int64Prod(pCtx,pMap);` |
|       - | 5927 | `	}` |
|     ! 0 | 5928 | `	return PH7_OK;` |
|     ! 0 | 5929 |  |
|       - | 5930 | `/*` |
|       - | 5931 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5932 | ` *  Pick one or more random entries out of an array.` |
|       - | 5933 | ` * Parameters` |
|       - | 5934 | ` * $input` |
|       - | 5935 | ` *  The input array.` |
|       - | 5936 | ` * $num_req` |
|       - | 5937 | ` *  Specifies how many entries you want to pick.` |
|       - | 5938 | ` * Return` |
|       - | 5939 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5940 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5941 | ` *  NULL is returned on failure.` |
|       - | 5942 | ` */` |
|       6 | 5943 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5944 |  |
|       - | 5945 | `	ph7_hashmap_node *pNode;` |
|       - | 5946 | `	ph7_hashmap *pMap;` |
|       7 | 5947 | `	int nItem = 1;` |
|       7 | 5948 | `	if( nArg < 1 ){` |
|       - | 5949 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5950 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5951 | `		return PH7_OK;` |
|       - | 5952 | `	}` |
|       - | 5953 | `	/* Make sure we are dealing with an array */` |
|       7 | 5954 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5955 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5956 | `		return PH7_OK;` |
|       - | 5957 | `	}` |
|       - | 5958 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5959 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5960 | `	if(pMap->nEntry < 1 ){` |
|       - | 5961 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5962 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5963 | `		return PH7_OK;` |
|       - | 5964 | `	}` |
|       7 | 5965 | `	if( nArg > 1 ){` |
|       3 | 5966 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5967 | `	}` |
|       7 | 5968 | `	if( nItem < 2 ){` |
|       - | 5969 | `		sxu32 nEntry;` |
|       - | 5970 | `		/* Select a random number */` |
|       5 | 5971 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5972 | `		/* Extract the desired entry.` |
|       - | 5973 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5974 | `		 */` |
|       5 | 5975 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 5976 | `			pNode = pMap->pLast;` |
|       3 | 5977 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 5978 | `			if( nEntry > 1 ){` |
|     ! 0 | 5979 | `				for(;;){` |
|     ! 0 | 5980 | `					if( nEntry == 0 ){` |
|     ! 0 | 5981 | `						break;` |
|       - | 5982 | `					}` |
|       - | 5983 | `					/* Point to the previous entry */` |
|     ! 0 | 5984 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5985 | `					nEntry--;` |
|     ! 0 | 5986 | `				}` |
|     ! 0 | 5987 | `			}` |
|       2 | 5988 | `		}else{` |
|       2 | 5989 | `			pNode = pMap->pFirst;` |
|       1 | 5990 | `			for(;;){` |
|       3 | 5991 | `				if( nEntry == 0 ){` |
|       2 | 5992 | `					break;` |
|       - | 5993 | `				}` |
|       - | 5994 | `				/* Point to the next entry */` |
|       1 | 5995 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 5996 | `				nEntry--;` |
|     ! 0 | 5997 | `			}` |
|       - | 5998 | `		}` |
|       5 | 5999 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6000 | `			/* Int key */` |
|       3 | 6001 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6002 | `		}else{` |
|       - | 6003 | `			/* Blob key */` |
|       3 | 6004 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6005 | `		}` |
|       3 | 6006 | `	}else{` |
|       - | 6007 | `		ph7_value sKey,*pArray;` |
|       - | 6008 | `		ph7_hashmap *pDest;` |
|       - | 6009 | `		/* Create a new array */` |
|       3 | 6010 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6011 | `		if( pArray == 0 ){` |
|     ! 0 | 6012 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6013 | `			return PH7_OK;` |
|       - | 6014 | `		}` |
|       - | 6015 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6016 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6017 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6018 | `		/* Copy the first n items */` |
|       3 | 6019 | `		pNode = pMap->pFirst;` |
|       3 | 6020 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6021 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6022 | `		}` |
|       7 | 6023 | `		while( nItem > 0){` |
|       5 | 6024 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6025 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6026 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6027 | `			/* Point to the next entry */` |
|       5 | 6028 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6029 | `			nItem--;` |
|       1 | 6030 | `		}` |
|       - | 6031 | `		/* Shuffle the array */` |
|       3 | 6032 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6033 | `		/* Rehash node */` |
|       3 | 6034 | `		HashmapSortRehash(pDest);` |
|       - | 6035 | `		/* Return the random array */` |
|       3 | 6036 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6037 | `	}` |
|       7 | 6038 | `	return PH7_OK;` |
|       4 | 6039 |  |
|       - | 6040 | `/*` |
|       - | 6041 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6042 | ` *  Split an array into chunks.` |
|       - | 6043 | ` * Parameters` |
|       - | 6044 | ` * $input` |
|       - | 6045 | ` *   The array to work on` |
|       - | 6046 | ` * $size` |
|       - | 6047 | ` *   The size of each chunk` |
|       - | 6048 | ` * $preserve_keys` |
|       - | 6049 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6050 | ` *   the chunk numerically.` |
|       - | 6051 | ` * Return` |
|       - | 6052 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6053 | ` *  zero, with each dimension containing size elements.` |
|       - | 6054 | ` */` |
|      42 | 6055 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6056 |  |
|       - | 6057 | `	ph7_value *pArray,*pChunk;` |
|       - | 6058 | `	ph7_hashmap_node *pEntry;` |
|       - | 6059 | `	ph7_hashmap *pMap;` |
|       - | 6060 | `	int bPreserve;` |
|       - | 6061 | `	sxu32 nChunk;` |
|       - | 6062 | `	sxu32 nSize;` |
|       - | 6063 | `	sxu32 n;` |
|       - | 6064 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 6065 | `	if( nArg < 2 ){` |
|       - | 6066 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6067 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6068 | `			"ArgumentCountError",` |
|       - | 6069 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6070 | `			nArg` |
|       - | 6071 | `			);` |
|       - | 6072 | `	}` |
|      42 | 6073 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6074 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6075 | `			"TypeError",` |
|       - | 6076 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6077 | `			ph7_type_name(apArg[0])` |
|       - | 6078 | `			);` |
|       - | 6079 | `	}` |
|       - | 6080 | `	/* Create a new array */` |
|      40 | 6081 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 6082 | `	if( pArray == 0 ){` |
|     ! 0 | 6083 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6084 | `		return PH7_OK;` |
|       - | 6085 | `	}` |
|       - | 6086 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 6087 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6088 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6089 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 6090 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 6091 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6092 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6093 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6094 | `			"TypeError",` |
|       - | 6095 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6096 | `			ph7_type_name(apArg[1])` |
|       - | 6097 | `			);` |
|       - | 6098 | `	}` |
|       - | 6099 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6100 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6101 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 6102 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6103 | `		int len;` |
|       3 | 6104 | `		sxu8 bReal = FALSE;` |
|       3 | 6105 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6106 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6107 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6108 | `				"TypeError",` |
|       - | 6109 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6110 | `				);` |
|       - | 6111 | `		}` |
|     ! 0 | 6112 | `		if( bReal ){` |
|       - | 6113 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6114 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6115 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6116 | `				zStr` |
|       - | 6117 | `				);` |
|     ! 0 | 6118 | `		}` |
|     ! 0 | 6119 | `	}` |
|       - | 6120 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6121 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6122 | `	 * later via ph7_value_to_int. */` |
|      38 | 6123 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6124 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6125 | `		sxi64 i = (sxi64)d;` |
|       3 | 6126 | `		if( d != (double)i ){` |
|       4 | 6127 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6128 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6129 | `				d` |
|       - | 6130 | `				);` |
|       1 | 6131 | `		}` |
|       1 | 6132 | `	}` |
|       - | 6133 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6134 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6135 | `	{` |
|      38 | 6136 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 6137 | `		if( nSizeSigned < 1 ){` |
|       - | 6138 | `			/* size <= 0 -> ValueError */` |
|       5 | 6139 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6140 | `				"ValueError",` |
|       - | 6141 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6142 | `				);` |
|       - | 6143 | `		}` |
|      34 | 6144 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6145 | `	}` |
|      34 | 6146 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6147 | `		/* Return the whole array */` |
|       3 | 6148 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6149 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6150 | `		return PH7_OK;` |
|       - | 6151 | `	}` |
|      32 | 6152 | `	bPreserve = 0;` |
|      32 | 6153 | `	if( nArg > 2 ){` |
|       - | 6154 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6155 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6156 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6157 | `		 * normally, matching PHP behaviour. */` |
|      45 | 6158 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 6159 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6160 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 6161 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6162 | `				"TypeError",` |
|       - | 6163 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6164 | `				ph7_type_name(apArg[2])` |
|       - | 6165 | `				);` |
|       - | 6166 | `		}` |
|      21 | 6167 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6168 | `	}` |
|       - | 6169 | `	/* Start processing */` |
|      27 | 6170 | `	pEntry = pMap->pFirst;` |
|      27 | 6171 | `	nChunk = 0;` |
|      27 | 6172 | `	pChunk = 0;` |
|      27 | 6173 | `	n = pMap->nEntry;` |
|      56 | 6174 | `	for( ;; ){` |
|     113 | 6175 | `		if( n < 1 ){` |
|       - | 6176 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6177 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6178 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6179 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6180 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6181 | `			 * exists. */` |
|      27 | 6182 | `			if( pChunk ){` |
|      27 | 6183 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6184 | `			}` |
|      27 | 6185 | `			break;` |
|       - | 6186 | `		}` |
|      87 | 6187 | `		if( nChunk < 1 ){` |
|      71 | 6188 | `			if( pChunk ){` |
|       - | 6189 | `				/* Put the first chunk */` |
|      45 | 6190 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6191 | `			}` |
|       - | 6192 | `			/* Create a new dimension */` |
|      71 | 6193 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6194 | `												   * will be automatically released as soon we return` |
|       - | 6195 | `												   * from this function */` |
|      71 | 6196 | `			if( pChunk == 0 ){` |
|     ! 0 | 6197 | `				break;` |
|       - | 6198 | `			}` |
|      71 | 6199 | `			nChunk = nSize;` |
|      35 | 6200 | `		}` |
|       - | 6201 | `		/* Insert the entry */` |
|      87 | 6202 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6203 | `		/* Point to the next entry */` |
|      87 | 6204 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6205 | `		nChunk--;` |
|      87 | 6206 | `		n--;` |
|       1 | 6207 | `	}` |
|       - | 6208 | `	/* Return the multidimensional array */` |
|      27 | 6209 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6210 | `	return PH7_OK;` |
|      23 | 6211 |  |
|       - | 6212 | `/*` |
|       - | 6213 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6214 | ` *  Pad array to the specified length with a value.` |
|       - | 6215 | ` * $input` |
|       - | 6216 | ` *   Initial array of values to pad.` |
|       - | 6217 | ` * $pad_size` |
|       - | 6218 | ` *   New size of the array.` |
|       - | 6219 | ` * $pad_value` |
|       - | 6220 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6221 | ` */` |
|      28 | 6222 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6223 |  |
|       - | 6224 | `	ph7_hashmap *pMap;` |
|       - | 6225 | `	ph7_value *pArray;` |
|       - | 6226 | `	int nEntry;` |
|      30 | 6227 | `	if( nArg != 3 ){` |
|      10 | 6228 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6229 | `			"ArgumentCountError",` |
|       - | 6230 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6231 | `			nArg` |
|       - | 6232 | `			);` |
|       - | 6233 | `	}` |
|      24 | 6234 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6235 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6236 | `			"TypeError",` |
|       - | 6237 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6238 | `			ph7_type_name(apArg[0])` |
|       - | 6239 | `			);` |
|       - | 6240 | `	}` |
|       - | 6241 | `	/* Create a new array */` |
|      21 | 6242 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6243 | `	if( pArray == 0 ){` |
|     ! 0 | 6244 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6245 | `		return PH7_OK;` |
|       - | 6246 | `	}` |
|       - | 6247 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6248 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6249 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6250 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6251 | `	if( nEntry < 0 ){` |
|       9 | 6252 | `		nEntry = -nEntry;` |
|       9 | 6253 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6254 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6255 | `			/* Insert given items first */` |
|      17 | 6256 | `			while( nEntry > 0 ){` |
|      13 | 6257 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 6258 | `				nEntry--;` |
|       1 | 6259 | `			}` |
|       - | 6260 | `			/* Merge the two arrays */` |
|       5 | 6261 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6262 | `		}else{` |
|       5 | 6263 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6264 | `		}` |
|      17 | 6265 | `	}else if( nEntry > 0 ){` |
|      11 | 6266 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6267 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6268 | `			/* Merge the two arrays first */` |
|       7 | 6269 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6270 | `			/* Insert given items */` |
|      25 | 6271 | `			while( nEntry > 0 ){` |
|      19 | 6272 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 6273 | `				nEntry--;` |
|       1 | 6274 | `			}` |
|       4 | 6275 | `		}else{` |
|       5 | 6276 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6277 | `		}` |
|       6 | 6278 | `	}else{` |
|       - | 6279 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6280 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6281 | `	}` |
|       - | 6282 | `	/* Return the new array */` |
|      21 | 6283 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6284 | `	return PH7_OK;` |
|      16 | 6285 |  |
|       - | 6286 | `/*` |
|       - | 6287 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6288 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6289 | ` * Parameters` |
|       - | 6290 | ` * $array` |
|       - | 6291 | ` *   The array in which elements are replaced.` |
|       - | 6292 | ` * $array1` |
|       - | 6293 | ` *   The array from which elements will be extracted.` |
|       - | 6294 | ` * ....` |
|       - | 6295 | ` *  More arrays from which elements will be extracted.` |
|       - | 6296 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6297 | ` * Return` |
|       - | 6298 | ` *  Returns an array.` |
|       - | 6299 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6300 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6301 | ` */` |
|      22 | 6302 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6303 |  |
|       - | 6304 | `	ph7_hashmap *pMap;` |
|       - | 6305 | `	ph7_value *pArray;` |
|       - | 6306 | `	int i;` |
|      24 | 6307 | `	if( nArg < 1 ){` |
|       3 | 6308 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6309 | `			"ArgumentCountError",` |
|       - | 6310 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6311 | `			);` |
|       - | 6312 | `	}` |
|      22 | 6313 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6314 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6315 | `			"TypeError",` |
|       - | 6316 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6317 | `			ph7_type_name(apArg[0])` |
|       - | 6318 | `			);` |
|       - | 6319 | `	}` |
|       - | 6320 | `	/* Create a new array */` |
|      20 | 6321 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6322 | `	if( pArray == 0 ){` |
|     ! 0 | 6323 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6324 | `		return PH7_OK;` |
|       - | 6325 | `	}` |
|       - | 6326 | `	/* Overwrite from the first array */` |
|      20 | 6327 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6328 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6329 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6330 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6331 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6332 | `			/* Type mismatch -> TypeError */` |
|       4 | 6333 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6334 | `				"TypeError",` |
|       - | 6335 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6336 | `				i + 1,` |
|       2 | 6337 | `				ph7_type_name(apArg[i])` |
|       - | 6338 | `				);` |
|       - | 6339 | `		}` |
|       - | 6340 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6341 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6342 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6343 | `	}` |
|       - | 6344 | `	/* Return the new array */` |
|      17 | 6345 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6346 | `	return PH7_OK;` |
|      13 | 6347 |  |
|       - | 6348 | `/*` |
|       - | 6349 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6350 | ` *  Filters elements of an array using a callback function.` |
|       - | 6351 | ` * Parameters` |
|       - | 6352 | ` *  $input` |
|       - | 6353 | ` *    The array to iterate over` |
|       - | 6354 | ` * $callback` |
|       - | 6355 | ` *    The callback function to use` |
|       - | 6356 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6357 | ` *    will be removed.` |
|       - | 6358 | ` * Return` |
|       - | 6359 | ` *  The filtered array.` |
|       - | 6360 | ` */` |
|      18 | 6361 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6362 |  |
|       - | 6363 | `	ph7_hashmap_node *pEntry;` |
|       - | 6364 | `	ph7_hashmap *pMap;` |
|       - | 6365 | `	ph7_value *pArray;` |
|       - | 6366 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6367 | `	ph7_value *pValue;` |
|       - | 6368 | `	sxi32 rc;` |
|       - | 6369 | `	int keep;` |
|       - | 6370 | `	sxu32 n;` |
|      20 | 6371 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6372 | `		/* Invalid arguments,return NULL */` |
|       5 | 6373 | `		ph7_result_null(pCtx);` |
|       5 | 6374 | `		return PH7_OK;` |
|       - | 6375 | `	}` |
|       - | 6376 | `	/* Create a new array */` |
|      16 | 6377 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 6378 | `	if( pArray == 0 ){` |
|     ! 0 | 6379 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6380 | `		return PH7_OK;` |
|       - | 6381 | `	}` |
|       - | 6382 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 6383 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 6384 | `	pEntry = pMap->pFirst;` |
|      16 | 6385 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 6386 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6387 | `	/* Perform the requested operation */` |
|      66 | 6388 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6389 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 6390 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 6391 | `		if( pValue == 0 ){` |
|       - | 6392 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6393 | `			keep = FALSE;` |
|      54 | 6394 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6395 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6396 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6397 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 6398 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6399 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6400 | `					int len;` |
|       3 | 6401 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6402 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6403 | `						"TypeError",` |
|       - | 6404 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6405 | `						zName` |
|       - | 6406 | `						);` |
|     ! 0 | 6407 | `				}else{` |
|     ! 0 | 6408 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6409 | `						"TypeError",` |
|       - | 6410 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6411 | `						ph7_type_name(apArg[1])` |
|       - | 6412 | `						);` |
|       - | 6413 | `				}` |
|       - | 6414 | `			}` |
|      23 | 6415 | `			keep = FALSE;` |
|      23 | 6416 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 6417 | `			if( rc == SXRET_OK ){` |
|       - | 6418 | `				/* Perform a boolean cast */` |
|      23 | 6419 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6420 | `			}` |
|      23 | 6421 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6422 | `		}else{` |
|       - | 6423 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6424 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6425 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6426 | `			 */` |
|      29 | 6427 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6428 | `		}` |
|      51 | 6429 | `		if( keep ){` |
|       - | 6430 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6431 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6432 | `		}` |
|       - | 6433 | `		/* Point to the next entry */` |
|      51 | 6434 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6435 | `	}` |
|      13 | 6436 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6437 | `	return PH7_OK;` |
|      11 | 6438 |  |
|       - | 6439 | `/*` |
|       - | 6440 | ` * array array_map(?callable $callback, array $array)` |
|       - | 6441 | ` *  Applies the callback to the elements of the given array.` |
|       - | 6442 | ` * Parameters` |
|       - | 6443 | ` *  $callback` |
|       - | 6444 | ` *   A callable to run for each element in the array, or NULL for the` |
|       - | 6445 | ` *   identity function (returns the array unchanged).` |
|       - | 6446 | ` *  $array` |
|       - | 6447 | ` *   An array to run through the callback function.` |
|       - | 6448 | ` * Return` |
|       - | 6449 | ` *  Returns an array containing the results of applying the callback` |
|       - | 6450 | ` *  function to each element of $array.` |
|       - | 6451 | ` */` |
|      34 | 6452 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6453 |  |
|       - | 6454 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6455 | `	ph7_hashmap_node *pEntry;` |
|       - | 6456 | `	ph7_hashmap *pMap;` |
|       - | 6457 | `	int bNullCallback;` |
|       - | 6458 | `	sxu32 n;` |
|      36 | 6459 | `	if( nArg < 2 ){` |
|       7 | 6460 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6461 | `			"ArgumentCountError",` |
|       - | 6462 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6463 | `			nArg` |
|       - | 6464 | `			);` |
|       - | 6465 | `	}` |
|      32 | 6466 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 6467 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6468 | `			"TypeError",` |
|       - | 6469 | `			"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6470 | `			ph7_type_name(apArg[1])` |
|       - | 6471 | `			);` |
|       - | 6472 | `	}` |
|      30 | 6473 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      30 | 6474 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6475 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6476 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6477 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6478 | `				"TypeError",` |
|       - | 6479 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6480 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6481 | `				zFunc` |
|       - | 6482 | `				);` |
|       - | 6483 | `		}` |
|       3 | 6484 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6485 | `			"TypeError",` |
|       - | 6486 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6487 | `			"no array or string given"` |
|       - | 6488 | `			);` |
|       - | 6489 | `	}` |
|       - | 6490 | `	/* Create a new array */` |
|      26 | 6491 | `	pArray = ph7_context_new_array(pCtx);` |
|      26 | 6492 | `	if( pArray == 0 ){` |
|     ! 0 | 6493 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6494 | `		return PH7_OK;` |
|       - | 6495 | `	}` |
|       - | 6496 | `	/* Point to the internal representation of the input hashmap */` |
|      26 | 6497 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      26 | 6498 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      26 | 6499 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      26 | 6500 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      26 | 6501 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 6502 | `	/* Perform the requested operation */` |
|      26 | 6503 | `	pEntry = pMap->pFirst;` |
|      80 | 6504 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6505 | `		/* Extract the node value */` |
|      56 | 6506 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      56 | 6507 | `		if( pValue ){` |
|       - | 6508 | `			/* Extract the node key */` |
|      56 | 6509 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      56 | 6510 | `			if( bNullCallback ){` |
|       - | 6511 | `				/* NULL callback: identity function, keep original value */` |
|      11 | 6512 | `				ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6513 | `			}else{` |
|       - | 6514 | `				/* Invoke the supplied callback */` |
|      46 | 6515 | `				PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 6516 | `				/* Insert the callback return value */` |
|      46 | 6517 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6518 | `			}` |
|      56 | 6519 | `			PH7_MemObjRelease(&sKey);` |
|      56 | 6520 | `			PH7_MemObjRelease(&sResult);` |
|      27 | 6521 | `		}` |
|       - | 6522 | `		/* Point to the next entry */` |
|      56 | 6523 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 6524 | `	}` |
|      26 | 6525 | `	ph7_result_value(pCtx,pArray);` |
|      26 | 6526 | `	return PH7_OK;` |
|      19 | 6527 |  |
|       - | 6528 | `/*` |
|       - | 6529 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6530 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6531 | ` * Parameters` |
|       - | 6532 | ` *  $array` |
|       - | 6533 | ` *   The input array.` |
|       - | 6534 | ` *  $callback` |
|       - | 6535 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6536 | ` *  $initial` |
|       - | 6537 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6538 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6539 | ` * Return` |
|       - | 6540 | ` *  Returns the resulting value.` |
|       - | 6541 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6542 | ` */` |
|      30 | 6543 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6544 |  |
|       - | 6545 | `	ph7_hashmap_node *pEntry;` |
|       - | 6546 | `	ph7_hashmap *pMap;` |
|       - | 6547 | `	ph7_value *pValue;` |
|       - | 6548 | `	ph7_value sResult;` |
|       - | 6549 | `	sxu32 n;` |
|      32 | 6550 | `	if( nArg < 2 ){` |
|       7 | 6551 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6552 | `			"ArgumentCountError",` |
|       - | 6553 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6554 | `			nArg` |
|       - | 6555 | `			);` |
|       - | 6556 | `	}` |
|      28 | 6557 | `	if( nArg > 3 ){` |
|       4 | 6558 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6559 | `			"ArgumentCountError",` |
|       - | 6560 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6561 | `			nArg` |
|       - | 6562 | `			);` |
|       - | 6563 | `	}` |
|      26 | 6564 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6565 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6566 | `			"TypeError",` |
|       - | 6567 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6568 | `			ph7_type_name(apArg[0])` |
|       - | 6569 | `			);` |
|       - | 6570 | `	}` |
|      24 | 6571 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6572 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6573 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6574 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6575 | `				"TypeError",` |
|       - | 6576 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6577 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6578 | `				zFunc` |
|       - | 6579 | `				);` |
|       - | 6580 | `		}` |
|       7 | 6581 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6582 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6583 | `				"TypeError",` |
|       - | 6584 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6585 | `				"array callback must have exactly two members"` |
|       - | 6586 | `				);` |
|       - | 6587 | `		}` |
|       5 | 6588 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6589 | `			"TypeError",` |
|       - | 6590 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6591 | `			"no array or string given"` |
|       - | 6592 | `			);` |
|       - | 6593 | `	}` |
|       - | 6594 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 6595 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6596 | `	/* Assume a NULL initial value */` |
|      15 | 6597 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      15 | 6598 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      15 | 6599 | `	if( nArg > 2 ){` |
|       - | 6600 | `		/* Set the initial value */` |
|      11 | 6601 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6602 | `	}` |
|       - | 6603 | `	/* Perform the requested operation */` |
|      15 | 6604 | `	pEntry = pMap->pFirst;` |
|      43 | 6605 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6606 | `		/* Extract the node value */` |
|      29 | 6607 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6608 | `		/* Invoke the supplied callback */` |
|      29 | 6609 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6610 | `		/* Point to the next entry */` |
|      29 | 6611 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6612 | `	}` |
|      15 | 6613 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6614 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6615 | `	return PH7_OK;` |
|      17 | 6616 |  |
|       - | 6617 | `/*` |
|       - | 6618 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6619 | ` *  Apply a user function to every member of an array.` |
|       - | 6620 | ` * Parameters` |
|       - | 6621 | ` *  $array` |
|       - | 6622 | ` *   The input array.` |
|       - | 6623 | ` *  $funcname` |
|       - | 6624 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6625 | ` *   the first, and the key/index second.` |
|       - | 6626 | ` * Note:` |
|       - | 6627 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6628 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6629 | ` *  be made in the original array itself.` |
|       - | 6630 | ` *  $userdata` |
|       - | 6631 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6632 | ` *   to the callback funcname.` |
|       - | 6633 | ` * Return` |
|       - | 6634 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6635 | ` */` |
|      36 | 6636 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6637 |  |
|       - | 6638 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6639 | `	ph7_hashmap_node *pEntry;` |
|       - | 6640 | `	ph7_hashmap *pMap;` |
|       - | 6641 | `	sxu32 n;` |
|      38 | 6642 | `	if( nArg < 2 ){` |
|       7 | 6643 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6644 | `			"ArgumentCountError",` |
|       - | 6645 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6646 | `			nArg` |
|       - | 6647 | `			);` |
|       - | 6648 | `	}` |
|      34 | 6649 | `	if( nArg > 3 ){` |
|       4 | 6650 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6651 | `			"ArgumentCountError",` |
|       - | 6652 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6653 | `			nArg` |
|       - | 6654 | `			);` |
|       - | 6655 | `	}` |
|      32 | 6656 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6657 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6658 | `			"TypeError",` |
|       - | 6659 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6660 | `			ph7_type_name(apArg[0])` |
|       - | 6661 | `			);` |
|       - | 6662 | `	}` |
|      30 | 6663 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6664 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6665 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6666 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6667 | `				"TypeError",` |
|       - | 6668 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6669 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6670 | `				zFunc` |
|       - | 6671 | `				);` |
|       - | 6672 | `		}` |
|       9 | 6673 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6674 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6675 | `				"TypeError",` |
|       - | 6676 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6677 | `				"array callback must have exactly two members"` |
|       - | 6678 | `				);` |
|       - | 6679 | `		}` |
|       5 | 6680 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6681 | `			"TypeError",` |
|       - | 6682 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6683 | `			"no array or string given"` |
|       - | 6684 | `			);` |
|       - | 6685 | `	}` |
|      19 | 6686 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6687 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6688 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      19 | 6689 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      19 | 6690 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6691 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6692 | `	/* Perform the desired operation */` |
|      19 | 6693 | `	pEntry = pMap->pFirst;` |
|      59 | 6694 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6695 | `		/* Extract the node value */` |
|      41 | 6696 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      41 | 6697 | `		if( pValue ){` |
|       - | 6698 | `			/* Extract the entry key */` |
|      41 | 6699 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6700 | `			/* Invoke the supplied callback */` |
|      41 | 6701 | `			PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      41 | 6702 | `			PH7_MemObjRelease(&sKey);` |
|      20 | 6703 | `		}` |
|       - | 6704 | `		/* Point to the next entry */` |
|      41 | 6705 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6706 | `	}` |
|       - | 6707 | `	/* All done, return TRUE */` |
|      19 | 6708 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6709 | `	return PH7_OK;` |
|      20 | 6710 |  |
|       - | 6711 | `/*` |
|       - | 6712 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6713 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6714 | ` */` |
|      22 | 6715 | `static void HashmapWalkRecursive(` |
|       - | 6716 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6717 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6718 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6719 | `	int iNest             /* Nesting level */` |
|       - | 6720 | `	)` |
|       1 | 6721 |  |
|       - | 6722 | `	ph7_hashmap_node *pEntry;` |
|       - | 6723 | `	ph7_value *pValue,sKey;` |
|       - | 6724 | `	sxu32 n;` |
|       - | 6725 | `	/* Iterate through hashmap entries */` |
|      23 | 6726 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6727 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6728 | `	pEntry = pMap->pFirst;` |
|      59 | 6729 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6730 | `		/* Extract the node value */` |
|      37 | 6731 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6732 | `		if( pValue ){` |
|      37 | 6733 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6734 | `				if( iNest < 32 ){` |
|       - | 6735 | `					/* Recurse */` |
|      11 | 6736 | `					iNest++;` |
|      11 | 6737 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6738 | `					iNest--;` |
|       5 | 6739 | `				}` |
|       6 | 6740 | `			}else{` |
|       - | 6741 | `				/* Extract the node key */` |
|      27 | 6742 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6743 | `				/* Invoke the supplied callback */` |
|      27 | 6744 | `				PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6745 | `				PH7_MemObjRelease(&sKey);` |
|       - | 6746 | `			}` |
|      18 | 6747 | `		}` |
|       - | 6748 | `		/* Point to the next entry */` |
|      37 | 6749 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6750 | `	}` |
|      23 | 6751 |  |
|       - | 6752 | `/*` |
|       - | 6753 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6754 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6755 | ` * Parameters` |
|       - | 6756 | ` *  $array` |
|       - | 6757 | ` *   The input array.` |
|       - | 6758 | ` *  $funcname` |
|       - | 6759 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6760 | ` *   the first, and the key/index second.` |
|       - | 6761 | ` * Note:` |
|       - | 6762 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6763 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6764 | ` *  be made in the original array itself.` |
|       - | 6765 | ` *  $userdata` |
|       - | 6766 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6767 | ` *   to the callback funcname.` |
|       - | 6768 | ` * Return` |
|       - | 6769 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6770 | ` */` |
|      30 | 6771 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6772 |  |
|       - | 6773 | `	ph7_hashmap *pMap;` |
|      32 | 6774 | `	if( nArg < 2 ){` |
|       7 | 6775 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6776 | `			"ArgumentCountError",` |
|       - | 6777 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 6778 | `			nArg` |
|       - | 6779 | `			);` |
|       - | 6780 | `	}` |
|      28 | 6781 | `	if( nArg > 3 ){` |
|       4 | 6782 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6783 | `			"ArgumentCountError",` |
|       - | 6784 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 6785 | `			nArg` |
|       - | 6786 | `			);` |
|       - | 6787 | `	}` |
|      26 | 6788 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6789 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6790 | `			"TypeError",` |
|       - | 6791 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6792 | `			ph7_type_name(apArg[0])` |
|       - | 6793 | `			);` |
|       - | 6794 | `	}` |
|      24 | 6795 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6796 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6797 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6798 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6799 | `				"TypeError",` |
|       - | 6800 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6801 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6802 | `				zFunc` |
|       - | 6803 | `				);` |
|       - | 6804 | `		}` |
|       9 | 6805 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6806 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6807 | `				"TypeError",` |
|       - | 6808 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6809 | `				"array callback must have exactly two members"` |
|       - | 6810 | `				);` |
|       - | 6811 | `		}` |
|       5 | 6812 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6813 | `			"TypeError",` |
|       - | 6814 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6815 | `			"no array or string given"` |
|       - | 6816 | `			);` |
|       - | 6817 | `	}` |
|       - | 6818 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6819 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 6820 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6821 | `	/* Perform the desired operation */` |
|      13 | 6822 | `	HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6823 | `	/* All done, return TRUE */` |
|      13 | 6824 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6825 | `	return PH7_OK;` |
|      17 | 6826 |  |
|       - | 6827 | `/*` |
|       - | 6828 | ` * Table of hashmap functions.` |
|       - | 6829 | ` */` |
|       - | 6830 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6831 | `	{"count",             ph7_hashmap_count },` |
|       - | 6832 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6833 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6834 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6835 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6836 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6837 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6838 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6839 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6840 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6841 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6842 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6843 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6844 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6845 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6846 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6847 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6848 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6849 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6850 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6851 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6852 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6853 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6854 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6855 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6856 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6857 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6858 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6859 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6860 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6861 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6862 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6863 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6864 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6865 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6866 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6867 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6868 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6869 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6870 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6871 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6872 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6873 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6874 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6875 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6876 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6877 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6878 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6879 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6880 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6881 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6882 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6883 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6884 | `	{"current",           ph7_hashmap_current },` |
|       - | 6885 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6886 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6887 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6888 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6889 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6890 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6891 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6892 | `};` |
|       - | 6893 | `/*` |
|       - | 6894 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6895 | ` */` |
|    2808 | 6896 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6897 |  |
|       - | 6898 | `	sxu32 n;` |
|  174098 | 6899 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  171290 | 6900 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   85646 | 6901 | `	}` |
|    2810 | 6902 |  |
|       - | 6903 | `/*` |
|       - | 6904 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6905 | ` * the BLOB given as the first argument.` |
|       - | 6906 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6907 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6908 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6909 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6910 | ` */` |
|      26 | 6911 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6912 |  |
|       - | 6913 | `	ph7_hashmap_node *pEntry;` |
|       - | 6914 | `	ph7_value *pObj;` |
|      28 | 6915 | `	sxu32 n = 0;` |
|       - | 6916 | `	int isRef;` |
|       - | 6917 | `	sxi32 rc;` |
|       - | 6918 | `	int i;` |
|      28 | 6919 | `	if( nDepth > 31 ){` |
|       - | 6920 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6921 | `		/* Nesting limit reached */` |
|     ! 0 | 6922 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6923 | `		if( ShowType ){` |
|     ! 0 | 6924 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6925 | `		}` |
|     ! 0 | 6926 | `		return SXERR_LIMIT;` |
|       - | 6927 | `	}` |
|       - | 6928 | `	/* Point to the first inserted entry */` |
|      28 | 6929 | `	pEntry = pMap->pFirst;` |
|      28 | 6930 | `	rc = SXRET_OK;` |
|      28 | 6931 | `	if( !ShowType ){` |
|      15 | 6932 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6933 | `	}` |
|       - | 6934 | `	/* Total entries */` |
|      28 | 6935 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6936 | `#ifdef __WINNT__` |
|       2 | 6937 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6938 | `#else` |
|      26 | 6939 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6940 | `#endif` |
|      62 | 6941 | `	for(;;){` |
|     126 | 6942 | `		if( n >= pMap->nEntry ){` |
|      28 | 6943 | `			break;` |
|       - | 6944 | `		}` |
|     198 | 6945 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 6946 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 6947 | `		}` |
|       - | 6948 | `		/* Dump key */` |
|     100 | 6949 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 6950 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 6951 | `		}else{` |
|     101 | 6952 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6953 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6954 | `		}` |
|       - | 6955 | `#ifdef __WINNT__` |
|       2 | 6956 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6957 | `#else` |
|      98 | 6958 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6959 | `#endif` |
|       - | 6960 | `		/* Dump node value */` |
|     100 | 6961 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 6962 | `		isRef = 0;` |
|     100 | 6963 | `		if( pObj ){` |
|     100 | 6964 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6965 | `				/* Referenced object */` |
|     ! 0 | 6966 | `				isRef = 1;` |
|     ! 0 | 6967 | `			}` |
|     100 | 6968 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 6969 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6970 | `				break;` |
|       - | 6971 | `			}` |
|      49 | 6972 | `		}` |
|       - | 6973 | `		/* Point to the next entry */` |
|     100 | 6974 | `		n++;` |
|     100 | 6975 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6976 | `	}` |
|      54 | 6977 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 6978 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 6979 | `	}` |
|      28 | 6980 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 6981 | `	return rc;` |
|      15 | 6982 |  |
|       - | 6983 | `/*` |
|       - | 6984 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6985 | ` * retrieved entry.` |
|       - | 6986 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6987 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6988 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6989 | ` * a value different from PH7_OK.` |
|       - | 6990 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6991 | ` */` |
|   29166 | 6992 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6993 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6994 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6995 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6996 | `	)` |
|       2 | 6997 |  |
|       - | 6998 | `	ph7_hashmap_node *pEntry;` |
|       - | 6999 | `	ph7_value sKey,sValue;` |
|       - | 7000 | `	sxi32 rc;` |
|       - | 7001 | `	sxu32 n;` |
|       - | 7002 | `	/* Initialize walker parameter */` |
|   29168 | 7003 | `	rc = SXRET_OK;` |
|   29168 | 7004 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   29168 | 7005 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   29168 | 7006 | `	n = pMap->nEntry;` |
|   29168 | 7007 | `	pEntry = pMap->pFirst;` |
|       - | 7008 | `	/* Start the iteration process */` |
|   72977 | 7009 | `	for(;;){` |
|  145956 | 7010 | `		if( n < 1 ){` |
|   29168 | 7011 | `			break;` |
|       - | 7012 | `		}` |
|       - | 7013 | `		/* Extract a copy of the key and a copy the current value */` |
|  116790 | 7014 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  116790 | 7015 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 7016 | `		/* Invoke the user callback */` |
|  116790 | 7017 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 7018 | `		/* Release the copy of the key and the value */` |
|  116790 | 7019 | `		PH7_MemObjRelease(&sKey);` |
|  116790 | 7020 | `		PH7_MemObjRelease(&sValue);` |
|  116790 | 7021 | `		if( rc != PH7_OK ){` |
|       - | 7022 | `			/* Callback request an operation abort */` |
|     ! 0 | 7023 | `			return SXERR_ABORT;` |
|       - | 7024 | `		}` |
|       - | 7025 | `		/* Point to the next entry */` |
|  116790 | 7026 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  116790 | 7027 | `		n--;` |
|       2 | 7028 | `	}` |
|       - | 7029 | `	/* All done */` |
|   29168 | 7030 | `	return SXRET_OK;` |
|   14585 | 7031 |  |
|       - | 7032 |  |
