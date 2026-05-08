# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2969/3397 lines (87.40%)

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
| 3005468 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 3005470 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  308596 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  308598 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  308598 |   29 | `	sxu32 nH = 5381;` |
|  308598 |   30 | `	zEnd = &zIn[nLen];` |
|  343466 |   31 | `	for(;;){` |
|  686934 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  602042 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  540154 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  446364 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  308598 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     892 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       2 |   47 |  |
|     894 |   48 | `	sxi64 iCount = 0;` |
|     894 |   49 | `	if( !bRecursive ){` |
|     720 |   50 | `		iCount = pMap->nEntry;` |
|     361 |   51 | `	}else{` |
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
|     894 |   86 | `	return iCount;` |
|       2 |   87 |  |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 2946690 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 2946692 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2946692 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 2946692 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 2946692 |  104 | `	pNode->pMap  = &(*pMap);` |
| 2946692 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2946692 |  106 | `	pNode->nHash = nHash;` |
| 2946692 |  107 | `	pNode->xKey.iKey = iKey;` |
| 2946692 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 2946692 |  109 | `	return pNode;` |
| 1473347 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|  106298 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|  106300 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  106300 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|  106300 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|  106300 |  127 | `	pNode->pMap  = &(*pMap);` |
|  106300 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  106300 |  129 | `	pNode->nHash = nHash;` |
|  106300 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  106300 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  106300 |  132 | `	pNode->nValIdx = nValIdx;` |
|  106300 |  133 | `	return pNode;` |
|   53151 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 3052988 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  139 |  |
|       - |  140 | `	/* Link */` |
| 3052990 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2756820 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2756820 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1378409 |  144 | `	}` |
| 3052990 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 3052990 |  147 | `	if( pMap->pFirst == 0 ){` |
|   52866 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   52866 |  150 | `		pMap->pCur = pNode;` |
|   26434 |  151 | `	}else{` |
| 3000126 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 3052990 |  154 | `	++pMap->nEntry;` |
| 3052990 |  155 |  |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    7634 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  161 |  |
|    7636 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    7636 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    7636 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    7188 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3595 |  167 | `	}else{` |
|     449 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    7636 |  170 | `	if( pNode->pNextCollide ){` |
|    5726 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2862 |  172 | `	}` |
|    7636 |  173 | `	if( pMap->pFirst == pNode ){` |
|      82 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      40 |  175 | `	}` |
|    7636 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|      84 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      41 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    7636 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    7636 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     104 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     104 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     104 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  188 | `		}` |
|      51 |  189 | `	}` |
|    7636 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    7510 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3754 |  192 | `	}` |
|    7636 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    7636 |  194 | `	pMap->nEntry--;` |
|    7636 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      32 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      32 |  198 | `		pMap->apBucket = 0;` |
|      32 |  199 | `		pMap->nSize = 0;` |
|      32 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      15 |  201 | `	}` |
|    7636 |  202 |  |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 3052988 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  208 |  |
| 3052990 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   57124 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   57124 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   57124 |  215 | `		if( nNew < 1 ){` |
|   52866 |  216 | `			nNew = 16;` |
|   26432 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   57124 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   57124 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   57124 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   57124 |  230 | `		pMap->apBucket = apNew;` |
|   57124 |  231 | `		pMap->nSize = nNew;` |
|   57124 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   52866 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    4260 |  237 | `		pEntry = pMap->pFirst;` |
|    4260 |  238 | `		n = 0;` |
| 2024513 |  239 | `		for( ;; ){` |
| 4049028 |  240 | `			if( n >= pMap->nEntry ){` |
|    4260 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 4044770 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 4044770 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4044770 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3505738 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3505738 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1752868 |  250 | `			}` |
| 4044770 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 4044770 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4044770 |  254 | `			n++;` |
|       2 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    4260 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2129 |  258 | `	}` |
| 3000126 |  259 | `	return SXRET_OK;` |
| 1526496 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 2946690 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 2946692 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		/* Reserve a ph7_value for the value */` |
| 2946666 |  274 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2946666 |  275 | `		if( pObj == 0 ){` |
|     ! 0 |  276 | `			return SXERR_MEM;` |
|       - |  277 | `		}` |
| 2946666 |  278 | `		if( pValue ){` |
|       - |  279 | `			/* Duplicate the value */` |
| 2946666 |  280 | `			PH7_MemObjStore(pValue,pObj);` |
| 1473332 |  281 | `		}` |
| 2946666 |  282 | `		nIdx = pObj->nIdx;` |
| 1473334 |  283 | `	}else{` |
|      27 |  284 | `		nIdx = nRefIdx;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Hash the key */` |
| 2946692 |  287 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  288 | `	/* Allocate a new int node */` |
| 2946692 |  289 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2946692 |  290 | `	if( pNode == 0 ){` |
|     ! 0 |  291 | `		return SXERR_MEM;` |
|       - |  292 | `	}` |
| 2946692 |  293 | `	if( isForeign ){` |
|       - |  294 | `		/* Mark as a foregin entry */` |
|      27 |  295 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      13 |  296 | `	}` |
|       - |  297 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2946692 |  298 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2946692 |  299 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  300 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  301 | `		return rc;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Perform the insertion */` |
| 2946692 |  304 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  305 | `	/* Install in the reference table */` |
| 2946692 |  306 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  307 | `	/* All done */` |
| 2946692 |  308 | `	return SXRET_OK;` |
| 1473347 |  309 |  |
|       - |  310 | `/*` |
|       - |  311 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  312 | ` * hashmap.` |
|       - |  313 | ` */` |
|  106298 |  314 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  315 |  |
|       - |  316 | `	ph7_hashmap_node *pNode;` |
|       - |  317 | `	sxu32 nHash;` |
|       - |  318 | `	sxu32 nIdx;` |
|       - |  319 | `	sxi32 rc;` |
|  106300 |  320 | `	if( !isForeign ){` |
|       - |  321 | `		ph7_value *pObj;` |
|       - |  322 | `		/* Reserve a ph7_value for the value */` |
|   71674 |  323 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   71674 |  324 | `		if( pObj == 0 ){` |
|     ! 0 |  325 | `			return SXERR_MEM;` |
|       - |  326 | `		}` |
|   71674 |  327 | `		if( pValue ){` |
|       - |  328 | `			/* Duplicate the value */` |
|   71402 |  329 | `			PH7_MemObjStore(pValue,pObj);` |
|   35700 |  330 | `		}` |
|   71674 |  331 | `		nIdx = pObj->nIdx;` |
|   35838 |  332 | `	}else{` |
|   34628 |  333 | `		nIdx = nRefIdx;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Hash the key */` |
|  106300 |  336 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  337 | `	/* Allocate a new blob node */` |
|  106300 |  338 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  106300 |  339 | `	if( pNode == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|  106300 |  342 | `	if( isForeign ){` |
|       - |  343 | `		/* Mark as a foregin entry */` |
|   34628 |  344 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   17313 |  345 | `	}` |
|       - |  346 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  106300 |  347 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  106300 |  348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  349 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  350 | `		return rc;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Perform the insertion */` |
|  106300 |  353 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  354 | `	/* Install in the reference table */` |
|  106300 |  355 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  356 | `	/* All done */` |
|  106300 |  357 | `	return SXRET_OK;` |
|   53151 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  361 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  362 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  363 | ` */` |
|   47756 |  364 | `static sxi32 HashmapLookupIntKey(` |
|       - |  365 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  366 | `	sxi64 iKey,                /* lookup key */` |
|       - |  367 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  368 | `	)` |
|       2 |  369 |  |
|       - |  370 | `	ph7_hashmap_node *pNode;` |
|       - |  371 | `	sxu32 nHash;` |
|   47758 |  372 | `	if( pMap->nEntry < 1 ){` |
|       - |  373 | `		/* Don't bother hashing,there is no entry anyway */` |
|     446 |  374 | `		return SXERR_NOTFOUND;` |
|       - |  375 | `	}` |
|       - |  376 | `	/* Hash the key first */` |
|   47314 |  377 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  378 | `	/* Point to the appropriate bucket */` |
|   47314 |  379 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  380 | `	/* Perform the lookup */` |
|  412011 |  381 | `	for(;;){` |
|  824024 |  382 | `		if( pNode == 0 ){` |
|   45990 |  383 | `			break;` |
|       - |  384 | `		}` |
|  778696 |  385 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  775019 |  386 | `			&& pNode->nHash == nHash` |
|  386666 |  387 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  388 | `				/* Node found */` |
|    1326 |  389 | `				if( ppNode ){` |
|    1314 |  390 | `					*ppNode = pNode;` |
|     656 |  391 | `				}` |
|    1326 |  392 | `				return SXRET_OK;` |
|       - |  393 | `		}` |
|       - |  394 | `		/* Follow the collision link */` |
|  776711 |  395 | `		pNode = pNode->pNextCollide;` |
|       1 |  396 | `	}` |
|       - |  397 | `	/* No such entry */` |
|   45990 |  398 | `	return SXERR_NOTFOUND;` |
|   23880 |  399 |  |
|       - |  400 | `/*` |
|       - |  401 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  402 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  403 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  404 | ` */` |
|  215244 |  405 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  406 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  407 | `	const void *pKey,           /* Lookup key */` |
|       - |  408 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  409 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  410 | `	)` |
|       2 |  411 |  |
|       - |  412 | `	ph7_hashmap_node *pNode;` |
|       - |  413 | `	sxu32 nHash;` |
|  215246 |  414 | `	if( pMap->nEntry < 1 ){` |
|       - |  415 | `		/* Don't bother hashing,there is no entry anyway */` |
|   12948 |  416 | `		return SXERR_NOTFOUND;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Hash the key first */` |
|  202300 |  419 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  420 | `	/* Point to the appropriate bucket */` |
|  202300 |  421 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  422 | `	/* Perform the lookup */` |
|  183484 |  423 | `	for(;;){` |
|  366970 |  424 | `		if( pNode == 0 ){` |
|  154626 |  425 | `			break;` |
|       - |  426 | `		}` |
|  236181 |  427 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  210845 |  428 | `			&& pNode->nHash == nHash` |
|  128510 |  429 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   47676 |  430 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  431 | `				/* Node found */` |
|   47676 |  432 | `				if( ppNode ){` |
|   47648 |  433 | `					*ppNode = pNode;` |
|   23823 |  434 | `				}` |
|   47676 |  435 | `				return SXRET_OK;` |
|       - |  436 | `		}` |
|       - |  437 | `		/* Follow the collision link */` |
|  164672 |  438 | `		pNode = pNode->pNextCollide;` |
|       2 |  439 | `	}` |
|       - |  440 | `	/* No such entry */` |
|  154626 |  441 | `	return SXERR_NOTFOUND;` |
|  107624 |  442 |  |
|       - |  443 | `/*` |
|       - |  444 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  445 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  446 | ` */` |
|  215384 |  447 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  448 |  |
|  215386 |  449 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  215386 |  450 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  215386 |  451 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  452 | `		/* Octal not decimal number */` |
|       5 |  453 | `		return FALSE;` |
|       - |  454 | `	}` |
|  215382 |  455 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  456 | `		zIn++;` |
|     ! 0 |  457 | `	}` |
|  108023 |  458 | `	for(;;){` |
|  216048 |  459 | `		if( zIn >= zEnd ){` |
|     233 |  460 | `			return TRUE;` |
|       - |  461 | `		}` |
|  215816 |  462 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  107576 |  463 | `			break;` |
|       - |  464 | `		}` |
|     667 |  465 | `		zIn++;` |
|       1 |  466 | `	}` |
|       - |  467 | `	/* Key does not look like a decimal number */` |
|  215150 |  468 | `	return FALSE;` |
|  107694 |  469 |  |
|       - |  470 | `/*` |
|       - |  471 | ` * Check if a given key exists in the given hashmap.` |
|       - |  472 | ` * Write a pointer to the target node on success.` |
|       - |  473 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  474 | ` */` |
|  110080 |  475 | `static sxi32 HashmapLookup(` |
|       - |  476 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  477 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  478 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  479 | `	)` |
|       2 |  480 |  |
|  110082 |  481 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  482 | `	sxi32 rc;` |
|  110082 |  483 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  108896 |  484 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  485 | `			/* Force a string cast */` |
|     ! 0 |  486 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  487 | `		}` |
|  108896 |  488 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  489 | `			/* Perform a blob lookup */` |
|  108880 |  490 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  108880 |  491 | `			goto result;` |
|       - |  492 | `		}` |
|       8 |  493 | `	}` |
|       - |  494 | `	/* Perform an int lookup */` |
|    1204 |  495 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  496 | `		/* Force an integer cast */` |
|      27 |  497 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  498 | `	}` |
|       - |  499 | `	/* Perform an int lookup */` |
|    1204 |  500 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   55040 |  501 | `result:` |
|  110082 |  502 | `	if( rc == SXRET_OK ){` |
|       - |  503 | `		/* Node found */` |
|   48740 |  504 | `		if( ppNode ){` |
|   48706 |  505 | `			*ppNode = pNode;` |
|   24352 |  506 | `		}` |
|   48740 |  507 | `		return SXRET_OK;` |
|       - |  508 | `	}` |
|       - |  509 | `	/* No such entry */` |
|   61344 |  510 | `	return SXERR_NOTFOUND;` |
|   55042 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  514 | ` * hashmap.` |
|       - |  515 | ` * If a node with the given key already exists in the database` |
|       - |  516 | ` * then this function overwrite the old value.` |
|       - |  517 | ` */` |
| 3018050 |  518 | `static sxi32 HashmapInsert(` |
|       - |  519 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  520 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  521 | `	ph7_value *pVal    /* Node value */` |
|       - |  522 | `	)` |
|       2 |  523 |  |
| 3018052 |  524 | `	ph7_hashmap_node *pNode = 0;` |
| 3018052 |  525 | `	sxi32 rc = SXRET_OK;` |
| 3018052 |  526 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   71898 |  527 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  528 | `			/* Force a string cast */` |
|       3 |  529 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  530 | `		}` |
|   71898 |  531 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  532 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  533 | `				/* Automatic index assign */` |
|      34 |  534 | `				pKey = 0;` |
|      16 |  535 | `			}` |
|     256 |  536 | `			goto IntKey;` |
|       - |  537 | `		}` |
|  107465 |  538 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   35821 |  539 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  540 | `				/* Overwrite the old value */` |
|       - |  541 | `				ph7_value *pElem;` |
|      67 |  542 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      67 |  543 | `				if( pElem ){` |
|      67 |  544 | `					if( pVal ){` |
|      67 |  545 | `						PH7_MemObjStore(pVal,pElem);` |
|      34 |  546 | `					}else{` |
|       - |  547 | `						/* Nullify the entry */` |
|     ! 0 |  548 | `						PH7_MemObjToNull(pElem);` |
|       - |  549 | `					}` |
|      33 |  550 | `				}` |
|      67 |  551 | `				return SXRET_OK;` |
|       - |  552 | `		}` |
|   71578 |  553 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  554 | `			/* Forbidden */` |
|       3 |  555 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Perform a blob-key insertion */` |
|   71576 |  559 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   71576 |  560 | `		return rc;` |
|       - |  561 | `	}` |
| 1473077 |  562 | `IntKey:` |
| 2946410 |  563 | `	if( pKey ){` |
|   23404 |  564 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  565 | `			/* Force an integer cast */` |
|     251 |  566 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  567 | `		}` |
|   23404 |  568 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   23318 |  582 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  583 | `			/* Forbidden */` |
|       3 |  584 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  585 | `			return SXRET_OK;` |
|       - |  586 | `		}` |
|       - |  587 | `		/* Perform a 64-bit-int-key insertion */` |
|   23316 |  588 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23316 |  589 | `		if( rc == SXRET_OK ){` |
|   23316 |  590 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  591 | `				/* Increment the automatic index */` |
|   23080 |  592 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  593 | `				/* Make sure the automatic index is not reserved */` |
|   23080 |  594 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  595 | `					pMap->iNextIdx++;` |
|     ! 0 |  596 | `				}` |
|   11539 |  597 | `			}` |
|   11657 |  598 | `		}` |
|   11659 |  599 | `	}else{` |
| 2923008 |  600 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  601 | `			/* Forbidden */` |
|       3 |  602 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|       - |  605 | `		/* Assign an automatic index */` |
| 2923006 |  606 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2923006 |  607 | `		if( rc == SXRET_OK ){` |
| 2923006 |  608 | `			++pMap->iNextIdx;` |
| 1461502 |  609 | `		}` |
|       - |  610 | `	}` |
|       - |  611 | `	/* Insertion result */` |
| 2946320 |  612 | `	return rc;` |
| 1509027 |  613 |  |
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
|   34658 |  641 | `static sxi32 HashmapInsertByRef(` |
|       - |  642 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  643 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  644 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  645 | `	)` |
|       2 |  646 |  |
|   34660 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|   34660 |  648 | `	sxi32 rc = SXRET_OK;` |
|   34660 |  649 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   34634 |  650 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  651 | `			/* Force a string cast */` |
|     ! 0 |  652 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  653 | `		}` |
|   34634 |  654 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  655 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  656 | `				/* Automatic index assign */` |
|     ! 0 |  657 | `				pKey = 0;` |
|     ! 0 |  658 | `			}` |
|     ! 0 |  659 | `			goto IntKey;` |
|       - |  660 | `		}` |
|   51950 |  661 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   17316 |  662 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  663 | `				/* Overwrite */` |
|       7 |  664 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  665 | `				pNode->nValIdx = nRefIdx;` |
|       - |  666 | `				/* Install in the reference table */` |
|       7 |  667 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  668 | `				return SXRET_OK;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Perform a blob-key insertion */` |
|   34628 |  671 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   34628 |  672 | `		return rc;` |
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
|   17331 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * Extract node value.` |
|       - |  712 | ` */` |
| 1163256 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
| 1163258 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1163258 |  718 | `	return pObj;` |
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
|   55066 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   55068 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   55068 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   55068 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   55068 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   55068 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   55068 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   55068 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   55068 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   55068 |  783 | `	return rc;` |
|   27534 |  784 |  |
|       - |  785 | `/*` |
|       - |  786 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  787 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  788 | ` */` |
|   11466 |  789 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  790 |  |
|   11468 |  791 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  792 | `	sxu32 nBucket;` |
|       - |  793 | `	/* Remove old collision links */` |
|   11468 |  794 | `	if( pEntry->pPrevCollide ){` |
|    9242 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    4631 |  796 | `	}else{` |
|    2228 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  798 | `	}` |
|   11468 |  799 | `	if( pEntry->pNextCollide ){` |
|     900 |  800 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     445 |  801 | `	}` |
|   11468 |  802 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  803 | `	/* Compute the new hash */` |
|   11468 |  804 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   11468 |  805 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   11468 |  806 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  807 | `	/* Link to the new bucket */` |
|   11468 |  808 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11468 |  809 | `	if( pMap->apBucket[nBucket] ){` |
|    9466 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    4745 |  811 | `	}` |
|   11468 |  812 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11468 |  813 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  814 | `	/* Increment the automatic index */` |
|   11468 |  815 | `	pMap->iNextIdx++;` |
|   11468 |  816 |  |
|       - |  817 | `/*` |
|       - |  818 | ` * Perform a linear search on a given hashmap.` |
|       - |  819 | ` * Write a pointer to the target node on success.` |
|       - |  820 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  821 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  822 | ` * for more information.` |
|       - |  823 | ` */` |
|   28674 |  824 | `static int HashmapFindValue(` |
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
|   28676 |  837 | `	pEntry = pMap->pFirst;` |
|   28676 |  838 | `	n = pMap->nEntry;` |
|   28676 |  839 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   28676 |  840 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   68736 |  841 | `	for(;;){` |
|  137472 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  137374 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  137374 |  847 | `		if( pVal ){` |
|  137374 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  137374 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  137374 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  137374 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  137374 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  137374 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  137374 |  865 | `				if( rc == 0 ){` |
|   28578 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   28578 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   54399 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|  108798 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  108798 |  876 | `		n--;` |
|       2 |  877 | `	}` |
|       - |  878 | `	/* No such entry */` |
|      99 |  879 | `	return SXERR_NOTFOUND;` |
|   14339 |  880 |  |
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
|  544600 | 1054 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1055 | `	ph7_hashmap *pDest,` |
|       - | 1056 | `	ph7_hashmap_node *pEntry,` |
|       - | 1057 | `	ph7_value *pVal,` |
|       - | 1058 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1059 | `	)` |
|       2 | 1060 |  |
|  544602 | 1061 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1062 | `	ph7_value sKey;` |
|       - | 1063 | `	sxi32 rc;` |
|       - | 1064 |  |
|  544602 | 1065 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1066 | `		/* Blob key insertion */` |
|      91 | 1067 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      91 | 1068 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      91 | 1069 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      91 | 1070 | `		PH7_MemObjRelease(&sKey);` |
|      46 | 1071 | `	}else{` |
|       - | 1072 | `		/* Int key */` |
|  544512 | 1073 | `		if( iAction == 0 ){ /* Merge */` |
|  544290 | 1074 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  272368 | 1075 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1076 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1077 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1078 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1079 | `		}else{ /* Dup */` |
|     194 | 1080 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1081 | `		}` |
|       - | 1082 | `	}` |
|  544602 | 1083 | `	return rc;` |
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
|  546280 | 1112 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1113 | `		/* Extract the node value */` |
|  544338 | 1114 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  544338 | 1115 | `		if( pVal ){` |
|       - | 1116 | `			/* Make a local copy of the value.` |
|       - | 1117 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1118 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1119 | `			 * to the old pool.` |
|       - | 1120 | `			 */` |
|  544338 | 1121 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  272170 | 1122 | `		}else{` |
|     ! 0 | 1123 | `			rc = SXRET_OK;` |
|       - | 1124 | `		}` |
|  544338 | 1125 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1126 | `			return rc;` |
|       - | 1127 | `		}` |
|       - | 1128 | `		/* Point to the next entry */` |
|  544338 | 1129 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  272170 | 1130 | `	}` |
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
|  190456 | 1219 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       2 | 1220 |  |
|  190458 | 1221 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1222 | `	ph7_hashmap *pNew;` |
|       - | 1223 | `	ph7_value *pBacking;` |
|  190458 | 1224 | `	if( pMap->iRef < 2 ){` |
|       - | 1225 | `		/* Sole owner, no separation needed */` |
|  188396 | 1226 | `		return pMap;` |
|       - | 1227 | `	}` |
|    2064 | 1228 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1229 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1230 | `		return pMap;` |
|       - | 1231 | `	}` |
|       - | 1232 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1233 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1234 | `	 * frame is popped. */` |
|    2064 | 1235 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2064 | 1236 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    3076 | 1237 | `		if( pBacking && pBacking != pValue` |
|    2045 | 1238 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2030 | 1239 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1240 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2030 | 1241 | `			pMap->iRef--;` |
|    2030 | 1242 | `			if( pMap->iRef < 2 ){` |
|       - | 1243 | `				/* After undoing stack ref, sole owner — no separation */` |
|    1994 | 1244 | `				pMap->iRef++;` |
|    1994 | 1245 | `				return pMap;` |
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
|   95230 | 1281 |  |
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
|   82958 | 1373 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1374 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1375 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1376 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1377 | `	)` |
|       2 | 1378 |  |
|       - | 1379 | `	ph7_hashmap *pMap;` |
|       - | 1380 | `	/* Allocate a new instance */` |
|   82960 | 1381 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   82960 | 1382 | `	if( pMap == 0 ){` |
|     ! 0 | 1383 | `		return 0;` |
|       - | 1384 | `	}` |
|       - | 1385 | `	/* Zero the structure */` |
|   82960 | 1386 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1387 | `	/* Fill in the structure */` |
|   82960 | 1388 | `	pMap->pVm = &(*pVm);` |
|   82960 | 1389 | `	pMap->iRef = 1;` |
|       - | 1390 | `	/* Default hash functions */` |
|   82960 | 1391 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   82960 | 1392 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   82960 | 1393 | `	return pMap;` |
|   41481 | 1394 |  |
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
|    2692 | 1415 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    2694 | 1435 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    2694 | 1436 | `	if( pMap == 0 ){` |
|     ! 0 | 1437 | `		return SXERR_MEM;` |
|       - | 1438 | `	}` |
|    2694 | 1439 | `	pVm->pGlobal = pMap;` |
|       - | 1440 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    2694 | 1441 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    2694 | 1442 | `	if( pObj == 0 ){` |
|     ! 0 | 1443 | `		return SXERR_MEM;` |
|       - | 1444 | `	}` |
|    2694 | 1445 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1446 | `	/* Record object index */` |
|    2694 | 1447 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1448 | `	/* Install the special $GLOBALS array */` |
|    2694 | 1449 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    2694 | 1450 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1451 | `		return rc;` |
|       - | 1452 | `	}` |
|       - | 1453 | `	/* Install superglobals now */` |
|   29614 | 1454 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1455 | `		ph7_value *pSuper;` |
|       - | 1456 | `		/* Request an empty array */` |
|   26922 | 1457 | `		pSuper = ph7_new_array(&(*pVm));` |
|   26922 | 1458 | `		if( pSuper == 0 ){` |
|     ! 0 | 1459 | `			return SXERR_MEM;` |
|       - | 1460 | `		}` |
|       - | 1461 | `		/* Install */` |
|   26922 | 1462 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   26922 | 1463 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1464 | `			return rc;` |
|       - | 1465 | `		}` |
|       - | 1466 | `		/* Release the value now it have been installed */` |
|   26922 | 1467 | `		ph7_release_value(&(*pVm),pSuper);` |
|   13462 | 1468 | `	}` |
|       - | 1469 | `	/* Set some $_SERVER entries */` |
|    2694 | 1470 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1471 | `	/*` |
|       - | 1472 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1473 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1474 | `	 */` |
|    5382 | 1475 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1476 | `		"SCRIPT_FILENAME",` |
|    1346 | 1477 | `		pFile ? pFile->zString : ":Memory:",` |
|    2688 | 1478 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1479 | `		);` |
|       - | 1480 | `	/* All done,all super-global are installed now */` |
|    2694 | 1481 | `	return SXRET_OK;` |
|    1348 | 1482 |  |
|       - | 1483 | `/*` |
|       - | 1484 | ` * Release a hashmap.` |
|       - | 1485 | ` */` |
|   53150 | 1486 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1487 |  |
|       - | 1488 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   53152 | 1489 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1490 | `	sxu32 n;` |
|   53152 | 1491 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1492 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1493 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1494 | `		return SXRET_OK;` |
|       - | 1495 | `	}` |
|       - | 1496 | `	/* Start the release process */` |
|   53152 | 1497 | `	n = 0;` |
|   53152 | 1498 | `	pEntry = pMap->pFirst;` |
| 1530767 | 1499 | `	for(;;){` |
| 3061536 | 1500 | `		if( n >= pMap->nEntry ){` |
|   53152 | 1501 | `			break;` |
|       - | 1502 | `		}` |
| 3008386 | 1503 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1504 | `		/* Remove the reference from the foreign table */` |
| 3008386 | 1505 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3008386 | 1506 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1507 | `			/* Restore the ph7_value to the free list */` |
| 3008378 | 1508 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1504188 | 1509 | `		}` |
|       - | 1510 | `		/* Release the node */` |
| 3008386 | 1511 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   67694 | 1512 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   33846 | 1513 | `		}` |
| 3008386 | 1514 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1515 | `		/* Point to the next entry */` |
| 3008386 | 1516 | `		pEntry = pNext;` |
| 3008386 | 1517 | `		n++;` |
|       2 | 1518 | `	}` |
|   53152 | 1519 | `	if( pMap->nEntry > 0 ){` |
|       - | 1520 | `		/* Release the hash bucket */` |
|   47228 | 1521 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   23613 | 1522 | `	}` |
|   53152 | 1523 | `	if( FreeDS ){` |
|       - | 1524 | `		/* Free the whole instance */` |
|   53136 | 1525 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   26569 | 1526 | `	}else{` |
|       - | 1527 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1528 | `		pMap->apBucket = 0;` |
|      17 | 1529 | `		pMap->iNextIdx = 0;` |
|      17 | 1530 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1531 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1532 | `	}` |
|   53152 | 1533 | `	return SXRET_OK;` |
|   26577 | 1534 |  |
|       - | 1535 | `/*` |
|       - | 1536 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1537 | ` * If the count reaches zero which mean no more variables` |
|       - | 1538 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1539 | ` */` |
|  586466 | 1540 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1541 |  |
|  586468 | 1542 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1543 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  586468 | 1544 | `	pMap->iRef--;` |
|  586468 | 1545 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   53122 | 1546 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   26560 | 1547 | `	}` |
|  586468 | 1548 |  |
|       - | 1549 | `/*` |
|       - | 1550 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1551 | ` * Write a pointer to the target node on success.` |
|       - | 1552 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1553 | ` */` |
|  110110 | 1554 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1555 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1556 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1557 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1558 | `	)` |
|       2 | 1559 |  |
|       - | 1560 | `	sxi32 rc;` |
|  110112 | 1561 | `	if( pMap->nEntry < 1 ){` |
|       - | 1562 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1563 | `		 */` |
|      31 | 1564 | `		return SXERR_NOTFOUND;` |
|       - | 1565 | `	}` |
|  110082 | 1566 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  110082 | 1567 | `	return rc;` |
|   55057 | 1568 |  |
|       - | 1569 | `/*` |
|       - | 1570 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1571 | ` * hashmap.` |
|       - | 1572 | ` * If a node with the given key already exists in the database` |
|       - | 1573 | ` * then this function overwrite the old value.` |
|       - | 1574 | ` */` |
| 2473550 | 1575 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1576 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1577 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1578 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1579 | `	)` |
|       2 | 1580 |  |
|       - | 1581 | `	sxi32 rc;` |
| 2473552 | 1582 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1583 | `		/*` |
|       - | 1584 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1585 | `		 */` |
|     ! 0 | 1586 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1587 | `		return SXRET_OK;` |
|       - | 1588 | `	}` |
| 2473552 | 1589 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2473552 | 1590 | `	return rc;` |
| 1236777 | 1591 |  |
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
|   34658 | 1629 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1630 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1631 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1632 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1633 | `	)` |
|       2 | 1634 |  |
|       - | 1635 | `	sxi32 rc;` |
|   34660 | 1636 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1637 | `		/*` |
|       - | 1638 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1639 | `		 */` |
|     ! 0 | 1640 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1641 | `		return SXRET_OK;` |
|       - | 1642 | `	}` |
|   34660 | 1643 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   34660 | 1644 | `	return rc;` |
|   17331 | 1645 |  |
|       - | 1646 | `/*` |
|       - | 1647 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1648 | ` */` |
|   23826 | 1649 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1650 |  |
|       - | 1651 | `	/* Reset the loop cursor */` |
|   23828 | 1652 | `	pMap->pCur = pMap->pFirst;` |
|   23828 | 1653 |  |
|       - | 1654 | `/*` |
|       - | 1655 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1656 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1657 | ` * return NULL.` |
|       - | 1658 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1659 | ` */` |
|  195118 | 1660 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1661 |  |
|  195120 | 1662 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  195120 | 1663 | `	if( pCur == 0 ){` |
|       - | 1664 | `		/* End of the list,return null */` |
|   11934 | 1665 | `		return 0;` |
|       - | 1666 | `	}` |
|       - | 1667 | `	/* Advance the node cursor */` |
|  183188 | 1668 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  183188 | 1669 | `	return pCur;` |
|   97561 | 1670 |  |
|       - | 1671 | `/*` |
|       - | 1672 | ` * Extract a node value.` |
|       - | 1673 | ` */` |
|  457286 | 1674 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1675 |  |
|  457288 | 1676 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  457288 | 1677 | `	if( pEntry ){` |
|  457288 | 1678 | `		if( bStore ){` |
|  183326 | 1679 | `			PH7_MemObjStore(pEntry,pValue);` |
|   91664 | 1680 | `		}else{` |
|  273964 | 1681 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1682 | `		}` |
|  228643 | 1683 | `	}else{` |
|     ! 0 | 1684 | `		PH7_MemObjRelease(pValue);` |
|       - | 1685 | `	}` |
|  457288 | 1686 |  |
|       - | 1687 | `/*` |
|       - | 1688 | ` * Extract a node key.` |
|       - | 1689 | ` */` |
|  115600 | 1690 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1691 |  |
|       - | 1692 | `	/* Fill with the current key */` |
|  115602 | 1693 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  115284 | 1694 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1695 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1696 | `		}` |
|  115284 | 1697 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  115284 | 1698 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   57643 | 1699 | `	}else{` |
|     319 | 1700 | `		SyBlobReset(&pKey->sBlob);` |
|     319 | 1701 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     319 | 1702 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1703 | `	}` |
|  115602 | 1704 |  |
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
|   30718 | 1752 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1753 |  |
|       - | 1754 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1755 | `    /* Prevent compiler warning */` |
|   30720 | 1756 | `	result.pNext = result.pPrev = 0;` |
|   30720 | 1757 | `	pTail = &result;` |
|   85928 | 1758 | `	while( pA && pB ){` |
|   55210 | 1759 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   35740 | 1760 | `			pTail->pPrev = pA;` |
|   35740 | 1761 | `			pA->pNext = pTail;` |
|   35740 | 1762 | `			pTail = pA;` |
|   35740 | 1763 | `			pA = pA->pPrev;` |
|   17847 | 1764 | `		}else{` |
|   19472 | 1765 | `			pTail->pPrev = pB;` |
|   19472 | 1766 | `			pB->pNext = pTail;` |
|   19472 | 1767 | `			pTail = pB;` |
|   19472 | 1768 | `			pB = pB->pPrev;` |
|       - | 1769 | `		}` |
|       2 | 1770 | `	}` |
|   30720 | 1771 | `	if( pA ){` |
|   21976 | 1772 | `		pTail->pPrev = pA;` |
|   21976 | 1773 | `		pA->pNext = pTail;` |
|   19746 | 1774 | `	}else if( pB ){` |
|    8520 | 1775 | `		pTail->pPrev = pB;` |
|    8520 | 1776 | `		pB->pNext = pTail;` |
|    4248 | 1777 | `	}else{` |
|     228 | 1778 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1779 | `	}` |
|   30720 | 1780 | `	return result.pPrev;` |
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
|   12238 | 1801 | `	while( pIn ){` |
|   11584 | 1802 | `		p = pIn;` |
|   11584 | 1803 | `		pIn = p->pPrev;` |
|   11584 | 1804 | `		p->pPrev = 0;` |
|   22028 | 1805 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   22028 | 1806 | `			if( a[i]==0 ){` |
|   11584 | 1807 | `				a[i] = p;` |
|   11584 | 1808 | `				break;` |
|     ! 0 | 1809 | `			}else{` |
|   10446 | 1810 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   10446 | 1811 | `				a[i] = 0;` |
|       - | 1812 | `			}` |
|    5224 | 1813 | `		}` |
|   11584 | 1814 | `		if( i==N_SORT_BUCKET-1 ){` |
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
|   55014 | 1836 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1837 |  |
|       - | 1838 | `	ph7_value sA,sB;` |
|       - | 1839 | `	sxi32 iFlags;` |
|       - | 1840 | `	int rc;` |
|   55016 | 1841 | `	if( pCmpData == 0 ){` |
|       - | 1842 | `		/* Perform a standard comparison */` |
|   54992 | 1843 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   54992 | 1844 | `		return rc;` |
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
|   27508 | 1882 |  |
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
|      18 | 2100 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2101 |  |
|       - | 2102 | `	sxu32 n;` |
|      10 | 2103 | `	SXUNUSED(pB); /* cc warning */` |
|      10 | 2104 | `	SXUNUSED(pCmpData);` |
|       - | 2105 | `	/* Grab a random number */` |
|      19 | 2106 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2107 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2108 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2109 | `	 */` |
|      19 | 2110 | `	return n&1 ? 1 : -1;` |
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
|    6009 | 2124 | `	for( ;; ){` |
|   12020 | 2125 | `		if( i >= pMap->nEntry ){` |
|     608 | 2126 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     608 | 2127 | `			break;` |
|       - | 2128 | `		}` |
|   11414 | 2129 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2130 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2131 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2132 | `			/* Change key type */` |
|       5 | 2133 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2134 | `		}` |
|   11414 | 2135 | `		HashmapRehashIntNode(p);` |
|       - | 2136 | `		/* Point to the next entry */` |
|   11414 | 2137 | `		i++;` |
|   11414 | 2138 | `		pLast = p;` |
|   11414 | 2139 | `		p = p->pPrev; /* Reverse link */` |
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
|       5 | 2614 | `		while(pMap->pLast->pPrev){` |
|       3 | 2615 | `			pMap->pLast = pMap->pLast->pPrev;` |
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
|     762 | 2635 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2636 |  |
|     764 | 2637 | `	int bRecursive = FALSE;` |
|     764 | 2638 | `	int bCycleDetected = FALSE;` |
|       - | 2639 | `	sxi64 iCount;` |
|     764 | 2640 | `	if( nArg < 1 ){` |
|       3 | 2641 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2642 | `			"ArgumentCountError",` |
|       - | 2643 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2644 | `			);` |
|       - | 2645 | `	}` |
|     762 | 2646 | `	if( nArg > 2 ){` |
|       4 | 2647 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2648 | `			"ArgumentCountError",` |
|       - | 2649 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2650 | `			nArg` |
|       - | 2651 | `			);` |
|       - | 2652 | `	}` |
|     760 | 2653 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2654 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2655 | `			"TypeError",` |
|       - | 2656 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       5 | 2657 | `			ph7_type_name(apArg[0])` |
|       - | 2658 | `			);` |
|       - | 2659 | `	}` |
|     750 | 2660 | `	if( nArg > 1 ){` |
|      34 | 2661 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      34 | 2662 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       5 | 2663 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2664 | `				"ValueError",` |
|       - | 2665 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2666 | `				);` |
|       - | 2667 | `		}` |
|      29 | 2668 | `		bRecursive = iMode == 1;` |
|      14 | 2669 | `	}` |
|       - | 2670 | `	/* Count */` |
|     746 | 2671 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     746 | 2672 | `	if( bCycleDetected ){` |
|       3 | 2673 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2674 | `	}` |
|     746 | 2675 | `	ph7_result_int64(pCtx,iCount);` |
|     746 | 2676 | `	return PH7_OK;` |
|     383 | 2677 |  |
|       - | 2678 | `/*` |
|       - | 2679 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2680 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2681 | ` * Parameters` |
|       - | 2682 | ` * $key` |
|       - | 2683 | ` *   Value to check.` |
|       - | 2684 | ` * $search` |
|       - | 2685 | ` *  An array with keys to check.` |
|       - | 2686 | ` * Return` |
|       - | 2687 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2688 | ` */` |
|      66 | 2689 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2690 |  |
|       - | 2691 | `	sxi32 rc;` |
|      68 | 2692 | `	if( nArg != 2 ){` |
|       - | 2693 | `		/* PHP requires exactly two arguments */` |
|      10 | 2694 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2695 | `			"ArgumentCountError",` |
|       - | 2696 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2697 | `			nArg` |
|       - | 2698 | `			);` |
|       - | 2699 | `	}` |
|       - | 2700 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 2701 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2702 | `		/* Type mismatch -> TypeError */` |
|       7 | 2703 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2704 | `			"TypeError",` |
|       - | 2705 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2706 | `			ph7_type_name(apArg[1])` |
|       - | 2707 | `			);` |
|       - | 2708 | `	}` |
|       - | 2709 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      57 | 2710 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2711 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2712 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2713 | `			"use an empty string instead"` |
|       - | 2714 | `			);` |
|      56 | 2715 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2716 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2717 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2718 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2719 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2720 | `				,rVal` |
|       - | 2721 | `				);` |
|       1 | 2722 | `		}` |
|       1 | 2723 | `	}` |
|       - | 2724 | `	/* Perform the lookup */` |
|      57 | 2725 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2726 | `	/* lookup result */` |
|      57 | 2727 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      57 | 2728 | `	return PH7_OK;` |
|      35 | 2729 |  |
|       - | 2730 | `/*` |
|       - | 2731 | ` * value array_pop(array $array)` |
|       - | 2732 | ` *   POP the last inserted element from the array.` |
|       - | 2733 | ` * Parameter` |
|       - | 2734 | ` *  The array to get the value from.` |
|       - | 2735 | ` * Return` |
|       - | 2736 | ` *  Poped value or NULL on failure.` |
|       - | 2737 | ` */` |
|      18 | 2738 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2739 |  |
|       - | 2740 | `	ph7_hashmap *pMap;` |
|       - | 2741 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      20 | 2742 | `	if( nArg != 1 ){` |
|       7 | 2743 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2744 | `			"ArgumentCountError",` |
|       - | 2745 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2746 | `			nArg` |
|       - | 2747 | `			);` |
|       - | 2748 | `	}` |
|       - | 2749 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2750 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      16 | 2751 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2752 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2753 | `			"Error",` |
|       - | 2754 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2755 | `			);` |
|       - | 2756 | `	}` |
|       - | 2757 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2758 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2759 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2760 | `			"TypeError",` |
|       - | 2761 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2762 | `			ph7_type_name(apArg[0])` |
|       - | 2763 | `			);` |
|       - | 2764 | `	}` |
|       9 | 2765 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2766 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2767 | `	if( pMap->nEntry < 1 ){` |
|       - | 2768 | `		/* Nothing to pop,return NULL */` |
|       3 | 2769 | `		ph7_result_null(pCtx);` |
|       2 | 2770 | `	}else{` |
|       7 | 2771 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2772 | `		ph7_value *pObj;` |
|       7 | 2773 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2774 | `		if( pObj ){` |
|       - | 2775 | `			/* Node value */` |
|       7 | 2776 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2777 | `			/* Unlink the node */` |
|       7 | 2778 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2779 | `		}else{` |
|     ! 0 | 2780 | `			ph7_result_null(pCtx);` |
|       - | 2781 | `		}` |
|       - | 2782 | `		/* Reset the cursor */` |
|       7 | 2783 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2784 | `	}` |
|       9 | 2785 | `	return PH7_OK;` |
|      11 | 2786 |  |
|       - | 2787 | `/*` |
|       - | 2788 | ` * int array_push($array,$var,...)` |
|       - | 2789 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2790 | ` * Parameters` |
|       - | 2791 | ` *  array` |
|       - | 2792 | ` *    The input array.` |
|       - | 2793 | ` *  var` |
|       - | 2794 | ` *   On or more value to push.` |
|       - | 2795 | ` * Return` |
|       - | 2796 | ` *  New array count (including old items).` |
|       - | 2797 | ` */` |
|      22 | 2798 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2799 |  |
|       - | 2800 | `	ph7_hashmap *pMap;` |
|       - | 2801 | `	sxi32 rc;` |
|       - | 2802 | `	int i;` |
|      24 | 2803 | `	if( nArg < 1 ){` |
|       4 | 2804 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2805 | `			"ArgumentCountError",` |
|       - | 2806 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2807 | `			nArg` |
|       - | 2808 | `			);` |
|       - | 2809 | `	}` |
|       - | 2810 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2811 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      22 | 2812 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2813 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2814 | `			"Error",` |
|       - | 2815 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2816 | `			);` |
|       - | 2817 | `	}` |
|       - | 2818 | `	/* Make sure we are dealing with a valid hashmap */` |
|      18 | 2819 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2820 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2821 | `			"TypeError",` |
|       - | 2822 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2823 | `			ph7_type_name(apArg[0])` |
|       - | 2824 | `			);` |
|       - | 2825 | `	}` |
|       - | 2826 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2827 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2828 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2829 | `	/* Start pushing given values */` |
|      31 | 2830 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      17 | 2831 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      17 | 2832 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2833 | `			break;` |
|       - | 2834 | `		}` |
|       9 | 2835 | `	}` |
|       - | 2836 | `	/* Return the new count */` |
|      15 | 2837 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 2838 | `	return PH7_OK;` |
|      13 | 2839 |  |
|       - | 2840 | `/*` |
|       - | 2841 | ` * value array_shift(array $array)` |
|       - | 2842 | ` *   Shift an element off the beginning of array.` |
|       - | 2843 | ` * Parameter` |
|       - | 2844 | ` *  The array to get the value from.` |
|       - | 2845 | ` * Return` |
|       - | 2846 | ` *  Shifted value or NULL on failure.` |
|       - | 2847 | ` */` |
|      38 | 2848 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2849 |  |
|       - | 2850 | `	ph7_hashmap *pMap;` |
|       - | 2851 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      40 | 2852 | `	if( nArg != 1 ){` |
|       7 | 2853 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2854 | `			"ArgumentCountError",` |
|       - | 2855 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2856 | `			nArg` |
|       - | 2857 | `			);` |
|       - | 2858 | `	}` |
|       - | 2859 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      36 | 2860 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2861 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2862 | `			"Error",` |
|       - | 2863 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2864 | `			);` |
|       - | 2865 | `	}` |
|       - | 2866 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 2867 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2868 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2869 | `			"TypeError",` |
|       - | 2870 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2871 | `			ph7_type_name(apArg[0])` |
|       - | 2872 | `			);` |
|       - | 2873 | `	}` |
|       - | 2874 | `	/* Point to the internal representation of the hashmap */` |
|      30 | 2875 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      30 | 2876 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      30 | 2877 | `	if( pMap->nEntry < 1 ){` |
|       - | 2878 | `		/* Empty hashmap,return NULL */` |
|       3 | 2879 | `		ph7_result_null(pCtx);` |
|       2 | 2880 | `	}else{` |
|      28 | 2881 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2882 | `		ph7_value *pObj;` |
|       - | 2883 | `		sxu32 n;` |
|      28 | 2884 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      28 | 2885 | `		if( pObj ){` |
|       - | 2886 | `			/* Node value */` |
|      28 | 2887 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2888 | `			/* Unlink the first node */` |
|      28 | 2889 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      15 | 2890 | `		}else{` |
|     ! 0 | 2891 | `			ph7_result_null(pCtx);` |
|       - | 2892 | `		}` |
|       - | 2893 | `		/* Rehash all int keys */` |
|      28 | 2894 | `		n = pMap->nEntry;` |
|      28 | 2895 | `		pEntry = pMap->pFirst;` |
|      28 | 2896 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 2897 | `		for(;;){` |
|      82 | 2898 | `			if( n < 1 ){` |
|      28 | 2899 | `				break;` |
|       - | 2900 | `			}` |
|      56 | 2901 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      56 | 2902 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 2903 | `			}` |
|       - | 2904 | `			/* Point to the next entry */` |
|      56 | 2905 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      56 | 2906 | `			n--;` |
|       2 | 2907 | `		}` |
|       - | 2908 | `		/* Reset the cursor */` |
|      28 | 2909 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2910 | `	}` |
|      30 | 2911 | `	return PH7_OK;` |
|      21 | 2912 |  |
|       - | 2913 | `/*` |
|       - | 2914 | ` * Extract the node cursor value.` |
|       - | 2915 | ` */` |
|      24 | 2916 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 2917 |  |
|      25 | 2918 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 2919 | `	ph7_value *pVal;` |
|      25 | 2920 | `	if( pCur == 0 ){` |
|       - | 2921 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 2922 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2923 | `		return PH7_OK;` |
|       - | 2924 | `	}` |
|      25 | 2925 | `	if( iDirection != 0 ){` |
|       9 | 2926 | `		if( iDirection > 0 ){` |
|       - | 2927 | `			/* Point to the next entry */` |
|       7 | 2928 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 2929 | `			pCur = pMap->pCur;` |
|       4 | 2930 | `		}else{` |
|       - | 2931 | `			/* Point to the previous entry */` |
|       3 | 2932 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 2933 | `			pCur = pMap->pCur;` |
|       - | 2934 | `		}` |
|       9 | 2935 | `		if( pCur == 0 ){` |
|       - | 2936 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 2937 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2938 | `			return PH7_OK;` |
|       - | 2939 | `		}` |
|       4 | 2940 | `	}` |
|       - | 2941 | `	/* Point to the desired element */` |
|      25 | 2942 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 2943 | `	if( pVal ){` |
|      25 | 2944 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 2945 | `	}else{` |
|     ! 0 | 2946 | `		ph7_result_bool(pCtx,0);` |
|       - | 2947 | `	}` |
|      25 | 2948 | `	return PH7_OK;` |
|      13 | 2949 |  |
|       - | 2950 | `/*` |
|       - | 2951 | ` * value current(array $array)` |
|       - | 2952 | ` *  Return the current element in an array.` |
|       - | 2953 | ` * Parameter` |
|       - | 2954 | ` *  $input: The input array.` |
|       - | 2955 | ` * Return` |
|       - | 2956 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 2957 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2958 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2959 | ` *  is empty, current() returns FALSE.` |
|       - | 2960 | ` */` |
|      10 | 2961 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2962 |  |
|      11 | 2963 | `	if( nArg < 1 ){` |
|       - | 2964 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2965 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2966 | `		return PH7_OK;` |
|       - | 2967 | `	}` |
|       - | 2968 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 2969 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2970 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2971 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2972 | `		return PH7_OK;` |
|       - | 2973 | `	}` |
|      11 | 2974 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 2975 | `	return PH7_OK;` |
|       6 | 2976 |  |
|       - | 2977 | `/*` |
|       - | 2978 | ` * value next(array $input)` |
|       - | 2979 | ` *  Advance the internal array pointer of an array.` |
|       - | 2980 | ` * Parameter` |
|       - | 2981 | ` *  $input: The input array.` |
|       - | 2982 | ` * Return` |
|       - | 2983 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 2984 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 2985 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 2986 | ` */` |
|       6 | 2987 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2988 |  |
|       7 | 2989 | `	if( nArg < 1 ){` |
|       - | 2990 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2991 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2992 | `		return PH7_OK;` |
|       - | 2993 | `	}` |
|       - | 2994 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 2995 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2996 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2997 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2998 | `		return PH7_OK;` |
|       - | 2999 | `	}` |
|       7 | 3000 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 3001 | `	return PH7_OK;` |
|       4 | 3002 |  |
|       - | 3003 | `/*` |
|       - | 3004 | ` * value prev(array $input)` |
|       - | 3005 | ` *  Rewind the internal array pointer.` |
|       - | 3006 | ` * Parameter` |
|       - | 3007 | ` *  $input: The input array.` |
|       - | 3008 | ` * Return` |
|       - | 3009 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3010 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3011 | ` *  elements.` |
|       - | 3012 | ` */` |
|       2 | 3013 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3014 |  |
|       3 | 3015 | `	if( nArg < 1 ){` |
|       - | 3016 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3017 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3018 | `		return PH7_OK;` |
|       - | 3019 | `	}` |
|       - | 3020 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3021 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3022 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3023 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3024 | `		return PH7_OK;` |
|       - | 3025 | `	}` |
|       3 | 3026 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3027 | `	return PH7_OK;` |
|       2 | 3028 |  |
|       - | 3029 | `/*` |
|       - | 3030 | ` * value end(array $input)` |
|       - | 3031 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3032 | ` * Parameter` |
|       - | 3033 | ` *  $input: The input array.` |
|       - | 3034 | ` * Return` |
|       - | 3035 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3036 | ` */` |
|       2 | 3037 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3038 |  |
|       - | 3039 | `	ph7_hashmap *pMap;` |
|       3 | 3040 | `	if( nArg < 1 ){` |
|       - | 3041 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3042 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3043 | `		return PH7_OK;` |
|       - | 3044 | `	}` |
|       - | 3045 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3046 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3047 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3048 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3049 | `		return PH7_OK;` |
|       - | 3050 | `	}` |
|       - | 3051 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3052 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3053 | `	/* Point to the last node */` |
|       3 | 3054 | `	pMap->pCur = pMap->pLast;` |
|       - | 3055 | `	/* Return the last node value */` |
|       3 | 3056 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3057 | `	return PH7_OK;` |
|       2 | 3058 |  |
|       - | 3059 | `/*` |
|       - | 3060 | ` * value reset(array $array )` |
|       - | 3061 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3062 | ` * Parameter` |
|       - | 3063 | ` *  $input: The input array.` |
|       - | 3064 | ` * Return` |
|       - | 3065 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3066 | ` */` |
|       4 | 3067 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3068 |  |
|       - | 3069 | `	ph7_hashmap *pMap;` |
|       5 | 3070 | `	if( nArg < 1 ){` |
|       - | 3071 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3072 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3073 | `		return PH7_OK;` |
|       - | 3074 | `	}` |
|       - | 3075 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3076 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3077 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3078 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3079 | `		return PH7_OK;` |
|       - | 3080 | `	}` |
|       - | 3081 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3082 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3083 | `	/* Point to the first node */` |
|       5 | 3084 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3085 | `	/* Return the last node value if available */` |
|       5 | 3086 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3087 | `	return PH7_OK;` |
|       3 | 3088 |  |
|       - | 3089 | `/*` |
|       - | 3090 | ` * value key(array $array)` |
|       - | 3091 | ` *   Fetch a key from an array` |
|       - | 3092 | ` * Parameter` |
|       - | 3093 | ` *  $input` |
|       - | 3094 | ` *   The input array.` |
|       - | 3095 | ` * Return` |
|       - | 3096 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3097 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3098 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3099 | ` *  is empty, key() returns NULL.` |
|       - | 3100 | ` */` |
|       4 | 3101 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3102 |  |
|       - | 3103 | `	ph7_hashmap_node *pCur;` |
|       - | 3104 | `	ph7_hashmap *pMap;` |
|       5 | 3105 | `	if( nArg < 1 ){` |
|       - | 3106 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3107 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3108 | `		return PH7_OK;` |
|       - | 3109 | `	}` |
|       - | 3110 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3111 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3112 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3113 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3114 | `		return PH7_OK;` |
|       - | 3115 | `	}` |
|       5 | 3116 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3117 | `	pCur = pMap->pCur;` |
|       5 | 3118 | `	if( pCur == 0 ){` |
|       - | 3119 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3120 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3121 | `		return PH7_OK;` |
|       - | 3122 | `	}` |
|       5 | 3123 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3124 | `		/* Key is integer */` |
|     ! 0 | 3125 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3126 | `	}else{` |
|       - | 3127 | `		/* Key is blob */` |
|       7 | 3128 | `		ph7_result_string(pCtx,` |
|       4 | 3129 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3130 | `	}` |
|       5 | 3131 | `	return PH7_OK;` |
|       3 | 3132 |  |
|       - | 3133 | `/*` |
|       - | 3134 | ` * array each(array $input)` |
|       - | 3135 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3136 | ` * Parameter` |
|       - | 3137 | ` *  $input` |
|       - | 3138 | ` *    The input array.` |
|       - | 3139 | ` * Return` |
|       - | 3140 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3141 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3142 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3143 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3144 | ` *  each() returns FALSE.` |
|       - | 3145 | ` */` |
|      22 | 3146 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3147 |  |
|       - | 3148 | `	ph7_hashmap_node *pCur;` |
|       - | 3149 | `	ph7_hashmap *pMap;` |
|       - | 3150 | `	ph7_value *pArray;` |
|       - | 3151 | `	ph7_value *pVal;` |
|       - | 3152 | `	ph7_value sKey;` |
|      23 | 3153 | `	if( nArg < 1 ){` |
|       - | 3154 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3155 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3156 | `		return PH7_OK;` |
|       - | 3157 | `	}` |
|       - | 3158 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3159 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3160 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3161 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3162 | `		return PH7_OK;` |
|       - | 3163 | `	}` |
|       - | 3164 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3165 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3166 | `	if( pMap->pCur == 0 ){` |
|       - | 3167 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3168 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3169 | `		return PH7_OK;` |
|       - | 3170 | `	}` |
|      15 | 3171 | `	pCur = pMap->pCur;` |
|       - | 3172 | `	/* Create a new array */` |
|      15 | 3173 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3174 | `	if( pArray == 0 ){` |
|     ! 0 | 3175 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3176 | `		return PH7_OK;` |
|       - | 3177 | `	}` |
|      15 | 3178 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3179 | `	/* Insert the current value */` |
|      15 | 3180 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3181 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3182 | `	/* Make the key */` |
|      15 | 3183 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3184 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3185 | `	}else{` |
|       9 | 3186 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3187 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3188 | `	}` |
|       - | 3189 | `	/* Insert the current key */` |
|      15 | 3190 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3191 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3192 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3193 | `	/* Advance the cursor */` |
|      15 | 3194 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3195 | `	/* Return the current entry */` |
|      15 | 3196 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3197 | `	return PH7_OK;` |
|      12 | 3198 |  |
|       - | 3199 | `/*` |
|       - | 3200 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3201 | ` *  Create an array containing a range of elements` |
|       - | 3202 | ` * Parameter` |
|       - | 3203 | ` *  start` |
|       - | 3204 | ` *   First value of the sequence.` |
|       - | 3205 | ` *  limit` |
|       - | 3206 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3207 | ` *  step` |
|       - | 3208 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3209 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3210 | ` * Return` |
|       - | 3211 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3212 | ` * NOTE:` |
|       - | 3213 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3214 | ` */` |
|       2 | 3215 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3216 |  |
|       - | 3217 | `	ph7_value *pValue,*pArray;` |
|       - | 3218 | `	sxi64 iOfft,iLimit;` |
|       3 | 3219 | `	int iStep = 1;` |
|       - | 3220 |  |
|       3 | 3221 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3222 | `	if( nArg > 0 ){` |
|       - | 3223 | `		/* Extract the offset */` |
|       3 | 3224 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3225 | `		if( nArg > 1 ){` |
|       - | 3226 | `			/* Extract the limit */` |
|       3 | 3227 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3228 | `			if( nArg > 2 ){` |
|       - | 3229 | `				/* Extract the increment */` |
|       3 | 3230 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3231 | `				if( iStep < 1 ){` |
|       - | 3232 | `					/* Only positive number are allowed */` |
|       3 | 3233 | `					iStep = 1;` |
|       1 | 3234 | `				}` |
|       1 | 3235 | `			}` |
|       1 | 3236 | `		}` |
|       1 | 3237 | `	}` |
|       - | 3238 | `	/* Element container */` |
|       3 | 3239 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3240 | `	/* Create the new array */` |
|       3 | 3241 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3242 | `	if( pArray == 0 ){` |
|     ! 0 | 3243 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3244 | `		return PH7_OK;` |
|       - | 3245 | `	}` |
|       - | 3246 | `	/* Start filling */` |
|       3 | 3247 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3248 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3249 | `		/* Perform the insertion */` |
|     ! 0 | 3250 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3251 | `		/* Increment */` |
|     ! 0 | 3252 | `		iOfft += iStep;` |
|     ! 0 | 3253 | `	}` |
|       - | 3254 | `	/* Return the new array */` |
|       3 | 3255 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3256 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3257 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3258 | `	 */` |
|       3 | 3259 | `	return PH7_OK;` |
|       2 | 3260 |  |
|       - | 3261 | `/*` |
|       - | 3262 | ` * array array_values(array $array)` |
|       - | 3263 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3264 | ` * Parameters` |
|       - | 3265 | ` *  $array` |
|       - | 3266 | ` *   The input array.` |
|       - | 3267 | ` * Return` |
|       - | 3268 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3269 | ` */` |
|      30 | 3270 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3271 |  |
|       - | 3272 | `	ph7_hashmap_node *pNode;` |
|       - | 3273 | `	ph7_hashmap *pMap;` |
|       - | 3274 | `	ph7_value *pArray;` |
|       - | 3275 | `	ph7_value *pObj;` |
|       - | 3276 | `	sxu32 n;` |
|      32 | 3277 | `	if( nArg != 1 ){` |
|       - | 3278 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3279 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3280 | `			"ArgumentCountError",` |
|       - | 3281 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3282 | `			nArg` |
|       - | 3283 | `			);` |
|       - | 3284 | `	}` |
|       - | 3285 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3286 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3287 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3288 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3289 | `			"TypeError",` |
|       - | 3290 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3291 | `			ph7_type_name(apArg[0])` |
|       - | 3292 | `			);` |
|       - | 3293 | `	}` |
|       - | 3294 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3295 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3296 | `	/* Create a new array */` |
|      25 | 3297 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3298 | `	if( pArray == 0 ){` |
|     ! 0 | 3299 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3300 | `		return PH7_OK;` |
|       - | 3301 | `	}` |
|       - | 3302 | `	/* Perform the requested operation */` |
|      25 | 3303 | `	pNode = pMap->pFirst;` |
|      83 | 3304 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3305 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3306 | `		if( pObj ){` |
|       - | 3307 | `			/* perform the insertion */` |
|      59 | 3308 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3309 | `		}` |
|       - | 3310 | `		/* Point to the next entry */` |
|      59 | 3311 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3312 | `	}` |
|       - | 3313 | `	/* return the new array */` |
|      25 | 3314 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3315 | `	return PH7_OK;` |
|      17 | 3316 |  |
|       - | 3317 | `/*` |
|       - | 3318 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3319 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3320 | ` * Parameters` |
|       - | 3321 | ` *  $input` |
|       - | 3322 | ` *   An array containing keys to return.` |
|       - | 3323 | ` * $search_value` |
|       - | 3324 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3325 | ` * $strict` |
|       - | 3326 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3327 | ` * Return` |
|       - | 3328 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3329 | ` */` |
|     120 | 3330 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3331 |  |
|       - | 3332 | `	ph7_hashmap_node *pNode;` |
|       - | 3333 | `	ph7_hashmap *pMap;` |
|       - | 3334 | `	ph7_value *pArray;` |
|       - | 3335 | `	ph7_value sObj;` |
|       - | 3336 | `	ph7_value sVal;` |
|       - | 3337 | `	SyString sKey;` |
|       - | 3338 | `	int bStrict;` |
|       - | 3339 | `	sxi32 rc;` |
|       - | 3340 | `	sxu32 n;` |
|     122 | 3341 | `	if( nArg < 1 ){` |
|       - | 3342 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3343 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3344 | `			"ArgumentCountError",` |
|       - | 3345 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3346 | `			);` |
|       - | 3347 | `	}` |
|       - | 3348 | `	/* Make sure we are dealing with a valid hashmap */` |
|     120 | 3349 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3350 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3351 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3352 | `			"TypeError",` |
|       - | 3353 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3354 | `			ph7_type_name(apArg[0])` |
|       - | 3355 | `			);` |
|       - | 3356 | `	}` |
|       - | 3357 | `	/* Point to the internal representation of the input hashmap */` |
|     118 | 3358 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3359 | `	/* Create a new array */` |
|     118 | 3360 | `	pArray = ph7_context_new_array(pCtx);` |
|     118 | 3361 | `	if( pArray == 0 ){` |
|     ! 0 | 3362 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3363 | `		return PH7_OK;` |
|       - | 3364 | `	}` |
|     118 | 3365 | `	bStrict = FALSE;` |
|     118 | 3366 | `	if( nArg > 2 ){` |
|       - | 3367 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3368 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3369 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3370 | `				"TypeError",` |
|       - | 3371 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3372 | `				ph7_type_name(apArg[2])` |
|       - | 3373 | `				);` |
|       - | 3374 | `		}` |
|       5 | 3375 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3376 | `	}` |
|       - | 3377 | `	/* Perform the requested operation */` |
|     115 | 3378 | `	pNode = pMap->pFirst;` |
|     115 | 3379 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     553 | 3380 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     439 | 3381 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     117 | 3382 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      59 | 3383 | `		}else{` |
|     323 | 3384 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3385 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3386 | `		}` |
|     439 | 3387 | `		rc = 0;` |
|     439 | 3388 | `		if( nArg > 1 ){` |
|      31 | 3389 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3390 | `			if( pValue ){` |
|      31 | 3391 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3392 | `				/* Filter key */` |
|      31 | 3393 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3394 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3395 | `			}` |
|      15 | 3396 | `		}` |
|     439 | 3397 | `		if( rc == 0 ){` |
|       - | 3398 | `			/* Perform the insertion */` |
|     421 | 3399 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     210 | 3400 | `		}` |
|     439 | 3401 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3402 | `		/* Point to the next entry */` |
|     439 | 3403 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     220 | 3404 | `	}` |
|       - | 3405 | `	/* return the new array */` |
|     115 | 3406 | `	ph7_result_value(pCtx,pArray);` |
|     115 | 3407 | `	return PH7_OK;` |
|      62 | 3408 |  |
|       - | 3409 | `/*` |
|       - | 3410 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3411 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3412 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3413 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3414 | ` * Parameters` |
|       - | 3415 | ` *  $arr1` |
|       - | 3416 | ` *   First array` |
|       - | 3417 | ` *  $arr2` |
|       - | 3418 | ` *   Second array` |
|       - | 3419 | ` * Return` |
|       - | 3420 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3421 | ` * Note` |
|       - | 3422 | ` *  This function is a symisc eXtension.` |
|       - | 3423 | ` */` |
|       4 | 3424 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3425 |  |
|       - | 3426 | `	ph7_hashmap *p1,*p2;` |
|       - | 3427 | `	int rc;` |
|       5 | 3428 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3429 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3430 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3431 | `		return PH7_OK;` |
|       - | 3432 | `	}` |
|       - | 3433 | `	/* Point to the hashmaps */` |
|       5 | 3434 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3435 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3436 | `	rc = (p1 == p2);` |
|       - | 3437 | `	/* Same instance? */` |
|       5 | 3438 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3439 | `	return PH7_OK;` |
|       3 | 3440 |  |
|       - | 3441 | `/*` |
|       - | 3442 | ` * array array_merge(array ...$arrays)` |
|       - | 3443 | ` *  Merge one or more arrays.` |
|       - | 3444 | ` * Parameters` |
|       - | 3445 | ` *  ...$arrays` |
|       - | 3446 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3447 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3448 | ` * Return` |
|       - | 3449 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3450 | ` *  with no arguments.` |
|       - | 3451 | ` */` |
|     948 | 3452 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3453 |  |
|       - | 3454 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3455 | `	ph7_value *pArray;` |
|       - | 3456 | `	int i;` |
|       - | 3457 | `	/* Create a new array */` |
|     950 | 3458 | `	pArray = ph7_context_new_array(pCtx);` |
|     950 | 3459 | `	if( pArray == 0 ){` |
|     ! 0 | 3460 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3461 | `		return PH7_OK;` |
|       - | 3462 | `	}` |
|       - | 3463 | `	/* Point to the internal representation of the hashmap */` |
|     950 | 3464 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3465 | `	/* Start merging */` |
|    2836 | 3466 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3467 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1892 | 3468 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3469 | `			/* Type mismatch -> TypeError */` |
|       7 | 3470 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3471 | `				"TypeError",` |
|       - | 3472 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3473 | `				i + 1,` |
|       4 | 3474 | `				ph7_type_name(apArg[i])` |
|       - | 3475 | `				);` |
|     ! 0 | 3476 | `		}else{` |
|    1888 | 3477 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3478 | `			/* Merge the two hashmaps */` |
|    1888 | 3479 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3480 | `		}` |
|     945 | 3481 | `	}` |
|       - | 3482 | `	/* Return the freshly created array */` |
|     946 | 3483 | `	ph7_result_value(pCtx,pArray);` |
|     946 | 3484 | `	return PH7_OK;` |
|     476 | 3485 |  |
|       - | 3486 | `/*` |
|       - | 3487 | ` * array array_copy(array $source)` |
|       - | 3488 | ` *  Make a blind copy of the target array.` |
|       - | 3489 | ` * Parameters` |
|       - | 3490 | ` *  $source` |
|       - | 3491 | ` *   Target array` |
|       - | 3492 | ` * Return` |
|       - | 3493 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3494 | ` * Note` |
|       - | 3495 | ` *  This function is a symisc eXtension.` |
|       - | 3496 | ` */` |
|      16 | 3497 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3498 |  |
|       - | 3499 | `	ph7_hashmap *pMap;` |
|       - | 3500 | `	ph7_value *pArray;` |
|      17 | 3501 | `	if( nArg < 1 ){` |
|       - | 3502 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3503 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3504 | `		return PH7_OK;` |
|       - | 3505 | `	}` |
|       - | 3506 | `	/* Create a new array */` |
|      17 | 3507 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3508 | `	if( pArray == 0 ){` |
|     ! 0 | 3509 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3510 | `		return PH7_OK;` |
|       - | 3511 | `	}` |
|       - | 3512 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3513 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3514 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3515 | `		/* Point to the internal representation of the source */` |
|      17 | 3516 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3517 | `		/* Perform the copy */` |
|      17 | 3518 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3519 | `	}else{` |
|       - | 3520 | `		/* Simple insertion */` |
|     ! 0 | 3521 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3522 | `	}` |
|       - | 3523 | `	/* Return the duplicated array */` |
|      17 | 3524 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3525 | `	return PH7_OK;` |
|       9 | 3526 |  |
|       - | 3527 | `/*` |
|       - | 3528 | ` * bool array_erase(array $source)` |
|       - | 3529 | ` *  Remove all elements from a given array.` |
|       - | 3530 | ` * Parameters` |
|       - | 3531 | ` *  $source` |
|       - | 3532 | ` *   Target array` |
|       - | 3533 | ` * Return` |
|       - | 3534 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3535 | ` * Note` |
|       - | 3536 | ` *  This function is a symisc eXtension.` |
|       - | 3537 | ` */` |
|      16 | 3538 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3539 |  |
|       - | 3540 | `	ph7_hashmap *pMap;` |
|      17 | 3541 | `	if( nArg < 1 ){` |
|       - | 3542 | `		/* Missing arguments */` |
|     ! 0 | 3543 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3544 | `		return PH7_OK;` |
|       - | 3545 | `	}` |
|       - | 3546 | `	/* Point to the target hashmap */` |
|      17 | 3547 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3548 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3549 | `	/* Erase */` |
|      17 | 3550 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3551 | `	return PH7_OK;` |
|       9 | 3552 |  |
|       - | 3553 | `/*` |
|       - | 3554 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3555 | ` *  Extract a slice of the array.` |
|       - | 3556 | ` * Parameters` |
|       - | 3557 | ` *  $array` |
|       - | 3558 | ` *    The input array.` |
|       - | 3559 | ` * $offset` |
|       - | 3560 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3561 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3562 | ` * $length (optional, nullable)` |
|       - | 3563 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3564 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3565 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3566 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3567 | ` * $preserve_keys (optional)` |
|       - | 3568 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3569 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3570 | ` * Return` |
|       - | 3571 | ` *   The new slice.` |
|       - | 3572 | ` */` |
|      46 | 3573 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3574 |  |
|       - | 3575 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3576 | `	ph7_hashmap_node *pCur;` |
|       - | 3577 | `	ph7_value *pArray;` |
|       - | 3578 | `	int iLength,iOfft;` |
|       - | 3579 | `	int bPreserve;` |
|       - | 3580 | `	sxi32 rc;` |
|      48 | 3581 | `	if( nArg < 2 ){` |
|       7 | 3582 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3583 | `			"ArgumentCountError",` |
|       - | 3584 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3585 | `			nArg` |
|       - | 3586 | `			);` |
|       - | 3587 | `	}` |
|      44 | 3588 | `	if( nArg > 4 ){` |
|       4 | 3589 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3590 | `			"ArgumentCountError",` |
|       - | 3591 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3592 | `			nArg` |
|       - | 3593 | `			);` |
|       - | 3594 | `	}` |
|      42 | 3595 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3596 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3597 | `			"TypeError",` |
|       - | 3598 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3599 | `			ph7_type_name(apArg[0])` |
|       - | 3600 | `			);` |
|       - | 3601 | `	}` |
|       - | 3602 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3603 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3604 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3605 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3606 | `			"TypeError",` |
|       - | 3607 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3608 | `			ph7_type_name(apArg[1])` |
|       - | 3609 | `			);` |
|       - | 3610 | `	}` |
|       - | 3611 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3612 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3613 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3614 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3615 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3616 | `				"TypeError",` |
|       - | 3617 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3618 | `				ph7_type_name(apArg[2])` |
|       - | 3619 | `				);` |
|       - | 3620 | `		}` |
|       8 | 3621 | `	}` |
|       - | 3622 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3623 | `	if( nArg > 3 ){` |
|      10 | 3624 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3625 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3626 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3627 | `				"TypeError",` |
|       - | 3628 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3629 | `				ph7_type_name(apArg[3])` |
|       - | 3630 | `				);` |
|       - | 3631 | `		}` |
|       2 | 3632 | `	}` |
|       - | 3633 | `	/* Point the internal representation of the target array */` |
|      33 | 3634 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3635 | `	bPreserve = FALSE;` |
|       - | 3636 | `	/* Get the offset */` |
|      33 | 3637 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3638 | `	if( iOfft < 0 ){` |
|       5 | 3639 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3640 | `		if( iOfft < 0 ){` |
|       3 | 3641 | `			iOfft = 0;` |
|       1 | 3642 | `		}` |
|       2 | 3643 | `	}` |
|      33 | 3644 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3645 | `		/* Offset past end of array, return empty array */` |
|       5 | 3646 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3647 | `		if( pArray == 0 ){` |
|     ! 0 | 3648 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3649 | `			return PH7_OK;` |
|       - | 3650 | `		}` |
|       5 | 3651 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3652 | `		return PH7_OK;` |
|       - | 3653 | `	}` |
|       - | 3654 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3655 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3656 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3657 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3658 | `		if( iLength < 0 ){` |
|       5 | 3659 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3660 | `		}` |
|      15 | 3661 | `		if( iLength < 0 ){` |
|       3 | 3662 | `			iLength = 0;` |
|       1 | 3663 | `		}` |
|      15 | 3664 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3665 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3666 | `		}` |
|       7 | 3667 | `	}` |
|      29 | 3668 | `	if( nArg > 3 ){` |
|       5 | 3669 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3670 | `	}` |
|       - | 3671 | `	/* Create a new array */` |
|      29 | 3672 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3673 | `	if( pArray == 0 ){` |
|     ! 0 | 3674 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3675 | `		return PH7_OK;` |
|       - | 3676 | `	}` |
|      29 | 3677 | `	if( iLength < 1 ){` |
|       - | 3678 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3679 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3680 | `		return PH7_OK;` |
|       - | 3681 | `	}` |
|       - | 3682 | `	/* Point to the desired entry */` |
|      25 | 3683 | `	pCur = pSrc->pFirst;` |
|      24 | 3684 | `	for(;;){` |
|      49 | 3685 | `		if( iOfft < 1 ){` |
|      25 | 3686 | `			break;` |
|       - | 3687 | `		}` |
|       - | 3688 | `		/* Point to the next entry */` |
|      25 | 3689 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3690 | `		iOfft--;` |
|       1 | 3691 | `	}` |
|       - | 3692 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3693 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3694 | `	for(;;){` |
|      79 | 3695 | `		if( iLength < 1 ){` |
|      25 | 3696 | `			break;` |
|       - | 3697 | `		}` |
|       - | 3698 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3699 | `		{` |
|      55 | 3700 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3701 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3702 | `		}` |
|      55 | 3703 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3704 | `			break;` |
|       - | 3705 | `		}` |
|       - | 3706 | `		/* Point to the next entry */` |
|      55 | 3707 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3708 | `		iLength--;` |
|       1 | 3709 | `	}` |
|       - | 3710 | `	/* Return the freshly created array */` |
|      25 | 3711 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3712 | `	return PH7_OK;` |
|      25 | 3713 |  |
|       - | 3714 | `/*` |
|       - | 3715 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3716 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3717 | ` * beginning (becomes the new pFirst).` |
|       - | 3718 | ` */` |
|      30 | 3719 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3720 |  |
|       - | 3721 | `	ph7_hashmap_node *pNode;` |
|       - | 3722 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3723 | `	pNode = pMap->pLast;` |
|      31 | 3724 | `	if( pNode == 0 ){` |
|     ! 0 | 3725 | `		return;` |
|       - | 3726 | `	}` |
|      31 | 3727 | `	if( pNode->pNext == 0 ){` |
|       - | 3728 | `		/* Only node in the list, nothing to move */` |
|       5 | 3729 | `		return;` |
|       - | 3730 | `	}` |
|      27 | 3731 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3732 | `		/* Already in the correct position */` |
|       9 | 3733 | `		return;` |
|       - | 3734 | `	}` |
|       - | 3735 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3736 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3737 | `	pMap->pLast->pPrev = 0;` |
|       - | 3738 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3739 | `	if( pAfter == 0 ){` |
|       - | 3740 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3741 | `		pNode->pNext = 0;` |
|       3 | 3742 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3743 | `		if( pMap->pFirst ){` |
|       3 | 3744 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3745 | `		}` |
|       3 | 3746 | `		pMap->pFirst = pNode;` |
|       2 | 3747 | `	}else{` |
|      17 | 3748 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3749 | `		pNode->pPrev = pOldNext;` |
|      17 | 3750 | `		pNode->pNext = pAfter;` |
|      17 | 3751 | `		pAfter->pPrev = pNode;` |
|      17 | 3752 | `		if( pOldNext ){` |
|      17 | 3753 | `			pOldNext->pNext = pNode;` |
|       9 | 3754 | `		}else{` |
|     ! 0 | 3755 | `			pMap->pLast = pNode;` |
|       - | 3756 | `		}` |
|       - | 3757 | `	}` |
|      16 | 3758 |  |
|       - | 3759 | `/*` |
|       - | 3760 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3761 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3762 | ` * Parameters` |
|       - | 3763 | ` *  $array` |
|       - | 3764 | ` *    The input array.` |
|       - | 3765 | ` *  $offset` |
|       - | 3766 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3767 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3768 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3769 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3770 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3771 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3772 | ` *  $length (optional)` |
|       - | 3773 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3774 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3775 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3776 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3777 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3778 | ` *  $replacement (optional)` |
|       - | 3779 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3780 | ` *    with elements from this array.` |
|       - | 3781 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3782 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3783 | ` *    offset.` |
|       - | 3784 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3785 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3786 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3787 | ` * Return` |
|       - | 3788 | ` *   A new array consisting of the extracted elements.` |
|       - | 3789 | ` */` |
|      54 | 3790 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3791 |  |
|       - | 3792 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3793 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3794 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3795 | `	int iLength,iOfft,i;` |
|       - | 3796 | `	sxi32 rc;` |
|      56 | 3797 | `	if( nArg < 2 ){` |
|       7 | 3798 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3799 | `			"ArgumentCountError",` |
|       - | 3800 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3801 | `			nArg` |
|       - | 3802 | `			);` |
|       - | 3803 | `	}` |
|      52 | 3804 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3805 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3806 | `			"TypeError",` |
|       - | 3807 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3808 | `			ph7_type_name(apArg[0])` |
|       - | 3809 | `			);` |
|       - | 3810 | `	}` |
|       - | 3811 | `	/* Point to the internal representation of the target array */` |
|      49 | 3812 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3813 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3814 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3815 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3816 | `	if( iOfft < 0 ){` |
|       7 | 3817 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3818 | `		if( iOfft < 0 ){` |
|       3 | 3819 | `			iOfft = 0;` |
|       2 | 3820 | `		}` |
|      46 | 3821 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3822 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3823 | `	}` |
|       - | 3824 | `	/* Get the length and clamp to valid range.` |
|       - | 3825 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3826 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3827 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3828 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3829 | `		if( iLength < 0 ){` |
|       7 | 3830 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3831 | `			if( iLength < 0 ){` |
|       3 | 3832 | `				iLength = 0;` |
|       1 | 3833 | `			}` |
|       3 | 3834 | `		}` |
|      31 | 3835 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3836 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3837 | `		}` |
|      15 | 3838 | `	}` |
|       - | 3839 | `	/* Create the result array for removed elements */` |
|      49 | 3840 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3841 | `	if( pArray == 0 ){` |
|     ! 0 | 3842 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3843 | `		return PH7_OK;` |
|       - | 3844 | `	}` |
|       - | 3845 | `	/* Get replacement array if provided */` |
|      49 | 3846 | `	pRep = 0;` |
|      49 | 3847 | `	if( nArg > 3 ){` |
|      21 | 3848 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3849 | `			/* Perform an array cast */` |
|       3 | 3850 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3851 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3852 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3853 | `			}` |
|       2 | 3854 | `		}else{` |
|      19 | 3855 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3856 | `		}` |
|      21 | 3857 | `		if( pRep ){` |
|       - | 3858 | `			/* Reset the loop cursor */` |
|      21 | 3859 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3860 | `		}` |
|      10 | 3861 | `	}` |
|       - | 3862 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3863 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3864 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3865 | `		return PH7_OK;` |
|       - | 3866 | `	}` |
|       - | 3867 | `	/* Navigate to the offset position */` |
|      41 | 3868 | `	pCur = pSrc->pFirst;` |
|      85 | 3869 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3870 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3871 | `	}` |
|       - | 3872 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3873 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3874 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3875 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3876 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3877 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3878 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3879 | `		pPrev = pCur->pPrev;` |
|      71 | 3880 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3881 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3882 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3883 | `			break;` |
|       - | 3884 | `		}` |
|      71 | 3885 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3886 | `	}` |
|       - | 3887 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3888 | `	if( pRep ){` |
|       - | 3889 | `		ph7_value sSafeVal;` |
|      61 | 3890 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3891 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3892 | `			if( pRvalue ){` |
|       - | 3893 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3894 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3895 | `				 * since it points into that same pool. */` |
|      31 | 3896 | `				sSafeVal = *pRvalue;` |
|      31 | 3897 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 3898 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 3899 | `					pNewNode = pSrc->pLast;` |
|      31 | 3900 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 3901 | `					pInsertAfter = pNewNode;` |
|      15 | 3902 | `				}` |
|      15 | 3903 | `			}` |
|       1 | 3904 | `		}` |
|      10 | 3905 | `	}` |
|       - | 3906 | `	/* Return the freshly created array */` |
|      41 | 3907 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 3908 | `	return PH7_OK;` |
|      29 | 3909 |  |
|       - | 3910 | `/*` |
|       - | 3911 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3912 | ` *  Checks if a value exists in an array.` |
|       - | 3913 | ` * Parameters` |
|       - | 3914 | ` *  $needle` |
|       - | 3915 | ` *   The searched value.` |
|       - | 3916 | ` *   Note:` |
|       - | 3917 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3918 | ` * $haystack` |
|       - | 3919 | ` *  The target array.` |
|       - | 3920 | ` * $strict` |
|       - | 3921 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3922 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3923 | ` */` |
|   28482 | 3924 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3925 |  |
|       - | 3926 | `	ph7_value *pNeedle;` |
|       - | 3927 | `	int bStrict;` |
|       - | 3928 | `	int rc;` |
|   28484 | 3929 | `	if( nArg < 2 ){` |
|       - | 3930 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3931 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3932 | `		return PH7_OK;` |
|       - | 3933 | `	}` |
|   28484 | 3934 | `	pNeedle = apArg[0];` |
|   28484 | 3935 | `	bStrict = 0;` |
|   28484 | 3936 | `	if( nArg > 2 ){` |
|       5 | 3937 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3938 | `	}` |
|   28484 | 3939 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3940 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3941 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3942 | `		/* Set the comparison result */` |
|     ! 0 | 3943 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3944 | `		return PH7_OK;` |
|       - | 3945 | `	}` |
|       - | 3946 | `	/* Perform the lookup */` |
|   28484 | 3947 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3948 | `	/* Lookup result */` |
|   28484 | 3949 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   28484 | 3950 | `	return PH7_OK;` |
|   14243 | 3951 |  |
|       - | 3952 | `/*` |
|       - | 3953 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3954 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3955 | ` * Parameters` |
|       - | 3956 | ` * $needle` |
|       - | 3957 | ` *   The searched value.` |
|       - | 3958 | ` * $haystack` |
|       - | 3959 | ` *   The array.` |
|       - | 3960 | ` * $strict` |
|       - | 3961 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3962 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3963 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3964 | ` * Return` |
|       - | 3965 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3966 | ` */` |
|      28 | 3967 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3968 |  |
|       - | 3969 | `	ph7_hashmap_node *pEntry;` |
|       - | 3970 | `	ph7_value *pVal,sNeedle;` |
|       - | 3971 | `	ph7_hashmap *pMap;` |
|       - | 3972 | `	ph7_value sVal;` |
|       - | 3973 | `	int bStrict;` |
|       - | 3974 | `	sxu32 n;` |
|       - | 3975 | `	int rc;` |
|      30 | 3976 | `	if( nArg < 2 ){` |
|       - | 3977 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3978 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3979 | `			"ArgumentCountError",` |
|       - | 3980 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 3981 | `			nArg` |
|       - | 3982 | `			);` |
|       - | 3983 | `	}` |
|      26 | 3984 | `	bStrict = FALSE;` |
|      26 | 3985 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3986 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3987 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3988 | `			"TypeError",` |
|       - | 3989 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 3990 | `			ph7_type_name(apArg[1])` |
|       - | 3991 | `			);` |
|       - | 3992 | `	}` |
|      24 | 3993 | `	if( nArg > 2 ){` |
|       - | 3994 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 3995 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3996 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3997 | `				"TypeError",` |
|       - | 3998 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3999 | `				ph7_type_name(apArg[2])` |
|       - | 4000 | `				);` |
|       - | 4001 | `		}` |
|       9 | 4002 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4003 | `	}` |
|       - | 4004 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4005 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4006 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4007 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4008 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4009 | `	pEntry = pMap->pFirst;` |
|      21 | 4010 | `	n = pMap->nEntry;` |
|      23 | 4011 | `	for(;;){` |
|      47 | 4012 | `		if( !n ){` |
|       9 | 4013 | `			break;` |
|       - | 4014 | `		}` |
|       - | 4015 | `		/* Extract node value */` |
|      39 | 4016 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4017 | `		if( pVal ){` |
|       - | 4018 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4019 | `			 * can change their type.` |
|       - | 4020 | `			 */` |
|      39 | 4021 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4022 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4023 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4024 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4025 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4026 | `			if( rc == 0 ){` |
|       - | 4027 | `				/* Match found,return key */` |
|      13 | 4028 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4029 | `					/* INT key */` |
|       7 | 4030 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4031 | `				}else{` |
|       7 | 4032 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4033 | `					/* Blob key */` |
|       7 | 4034 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4035 | `				}` |
|      13 | 4036 | `				return PH7_OK;` |
|       - | 4037 | `			}` |
|      13 | 4038 | `		}` |
|       - | 4039 | `		/* Point to the next entry */` |
|      27 | 4040 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4041 | `		n--;` |
|       1 | 4042 | `	}` |
|       - | 4043 | `	/* No such value,return FALSE */` |
|       9 | 4044 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4045 | `	return PH7_OK;` |
|      16 | 4046 |  |
|       - | 4047 | `/*` |
|       - | 4048 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4049 | ` *  Computes the difference of arrays.` |
|       - | 4050 | ` * Parameters` |
|       - | 4051 | ` *  $array1` |
|       - | 4052 | ` *    The array to compare from` |
|       - | 4053 | ` *  $array2` |
|       - | 4054 | ` *    An array to compare against` |
|       - | 4055 | ` *  $...` |
|       - | 4056 | ` *   More arrays to compare against` |
|       - | 4057 | ` * Return` |
|       - | 4058 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4059 | ` *  are not present in any of the other arrays.` |
|       - | 4060 | ` */` |
|      22 | 4061 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4062 |  |
|       - | 4063 | `	ph7_hashmap_node *pEntry;` |
|       - | 4064 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4065 | `	ph7_value *pArray;` |
|       - | 4066 | `	ph7_value *pVal;` |
|       - | 4067 | `	sxi32 rc;` |
|       - | 4068 | `	sxu32 n;` |
|       - | 4069 | `	int i;` |
|       - | 4070 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4071 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4072 | `	 * debugging difficult. */` |
|      24 | 4073 | `	if( nArg < 1 ){` |
|       4 | 4074 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4075 | `			"ArgumentCountError",` |
|       - | 4076 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4077 | `			nArg` |
|       - | 4078 | `			);` |
|       - | 4079 | `	}` |
|      22 | 4080 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4081 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4082 | `			"TypeError",` |
|       - | 4083 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4084 | `			ph7_type_name(apArg[0])` |
|       - | 4085 | `			);` |
|       - | 4086 | `	}` |
|      36 | 4087 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4088 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4089 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4090 | `				"TypeError",` |
|       - | 4091 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4092 | `				i + 1,` |
|       2 | 4093 | `				ph7_type_name(apArg[i])` |
|       - | 4094 | `				);` |
|       - | 4095 | `		}` |
|       9 | 4096 | `	}` |
|      17 | 4097 | `	if( nArg == 1 ){` |
|       - | 4098 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4099 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4100 | `		return PH7_OK;` |
|       - | 4101 | `	}` |
|       - | 4102 | `	/* Create a new array */` |
|      15 | 4103 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4104 | `	if( pArray == 0 ){` |
|     ! 0 | 4105 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4106 | `		return PH7_OK;` |
|       - | 4107 | `	}` |
|       - | 4108 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4109 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4110 | `	/* Perform the diff */` |
|      15 | 4111 | `	pEntry = pSrc->pFirst;` |
|      15 | 4112 | `	n = pSrc->nEntry;` |
|      27 | 4113 | `	for(;;){` |
|      55 | 4114 | `		if( n < 1 ){` |
|      15 | 4115 | `			break;` |
|       - | 4116 | `		}` |
|       - | 4117 | `		/* Extract the node value */` |
|      41 | 4118 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4119 | `		if( pVal ){` |
|      69 | 4120 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4121 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4122 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4123 | `				/* Perform the lookup */` |
|      45 | 4124 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4125 | `				if( rc == SXRET_OK ){` |
|       - | 4126 | `					/* Value exist */` |
|      17 | 4127 | `					break;` |
|       - | 4128 | `				}` |
|      15 | 4129 | `			}` |
|      41 | 4130 | `			if( i >= nArg ){` |
|       - | 4131 | `				/* Perform the insertion */` |
|      25 | 4132 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4133 | `			}` |
|      20 | 4134 | `		}` |
|       - | 4135 | `		/* Point to the next entry */` |
|      41 | 4136 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4137 | `		n--;` |
|       1 | 4138 | `	}` |
|       - | 4139 | `	/* Return the freshly created array */` |
|      15 | 4140 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4141 | `	return PH7_OK;` |
|      13 | 4142 |  |
|       - | 4143 | `/*` |
|       - | 4144 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4145 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4146 | ` * Parameters` |
|       - | 4147 | ` *  $array1` |
|       - | 4148 | ` *    The array to compare from` |
|       - | 4149 | ` *  $array2` |
|       - | 4150 | ` *    An array to compare against` |
|       - | 4151 | ` *  $...` |
|       - | 4152 | ` *   More arrays to compare against.` |
|       - | 4153 | ` * $callback` |
|       - | 4154 | ` *  The callback comparison function.` |
|       - | 4155 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4156 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4157 | ` *  than the second.` |
|       - | 4158 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4159 | ` * Return` |
|       - | 4160 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4161 | ` *  are not present in any of the other arrays.` |
|       - | 4162 | ` */` |
|      20 | 4163 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4164 |  |
|       - | 4165 | `	ph7_hashmap_node *pEntry;` |
|       - | 4166 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4167 | `	ph7_value *pCallback;` |
|       - | 4168 | `	ph7_value *pArray;` |
|       - | 4169 | `	ph7_value *pVal;` |
|       - | 4170 | `	sxi32 rc;` |
|       - | 4171 | `	sxu32 n;` |
|       - | 4172 | `	int i;` |
|       - | 4173 |  |
|       - | 4174 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      22 | 4175 | `	if( nArg < 2 ){` |
|       4 | 4176 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4177 | `			"ArgumentCountError",` |
|       - | 4178 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4179 | `			nArg` |
|       - | 4180 | `			);` |
|       - | 4181 | `	}` |
|      20 | 4182 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4183 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4184 | `			"TypeError",` |
|       - | 4185 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4186 | `			ph7_type_name(apArg[0])` |
|       - | 4187 | `			);` |
|       - | 4188 | `	}` |
|       - | 4189 |  |
|      18 | 4190 | `	if( nArg == 2 ){` |
|       - | 4191 | `		/* Only the original array and the callback were provided. */` |
|       - | 4192 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4193 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4194 | `		 * validation order.` |
|       - | 4195 | `		 */` |
|       4 | 4196 | `	} else {` |
|       - | 4197 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      20 | 4198 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      14 | 4199 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      10 | 4200 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4201 | `					"TypeError",` |
|       - | 4202 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4203 | `					i + 1,` |
|       6 | 4204 | `					ph7_type_name(apArg[i])` |
|       - | 4205 | `					);` |
|       - | 4206 | `			}` |
|       5 | 4207 | `		}` |
|       - | 4208 | `	}` |
|       - | 4209 |  |
|       - | 4210 | `	/* Identify the callback (always expected as the last argument). */` |
|      12 | 4211 | `	pCallback = apArg[nArg - 1];` |
|       - | 4212 | `	/* Validate the callback to match PHP's error messages. */` |
|      12 | 4213 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       7 | 4214 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4215 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4216 | `				"TypeError",` |
|       - | 4217 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4218 | `				nArg` |
|       - | 4219 | `				);` |
|       - | 4220 | `		}` |
|       5 | 4221 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4222 | `			int len;` |
|       3 | 4223 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4224 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4225 | `				"TypeError",` |
|       - | 4226 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4227 | `				nArg,` |
|       1 | 4228 | `				zName` |
|       - | 4229 | `				);` |
|       - | 4230 | `		}` |
|       4 | 4231 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4232 | `			"TypeError",` |
|       - | 4233 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4234 | `			nArg` |
|       - | 4235 | `			);` |
|       - | 4236 | `	}` |
|       - | 4237 |  |
|       5 | 4238 | `	if( nArg == 2 ){` |
|       - | 4239 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4240 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4241 | `		return PH7_OK;` |
|       - | 4242 | `	}` |
|       - | 4243 |  |
|       - | 4244 | `	/* Create a new array */` |
|       3 | 4245 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4246 | `	if( pArray == 0 ){` |
|     ! 0 | 4247 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4248 | `		return PH7_OK;` |
|       - | 4249 | `	}` |
|       - | 4250 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4251 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4252 | `	/* Perform the diff */` |
|       3 | 4253 | `	pEntry = pSrc->pFirst;` |
|       3 | 4254 | `	n = pSrc->nEntry;` |
|       4 | 4255 | `	for(;;){` |
|       9 | 4256 | `		if( n < 1 ){` |
|       3 | 4257 | `			break;` |
|       - | 4258 | `		}` |
|       - | 4259 | `		/* Extract the node value */` |
|       7 | 4260 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4261 | `		if( pVal ){` |
|      11 | 4262 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4263 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4264 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4265 | `				/* Perform the lookup */` |
|       7 | 4266 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4267 | `				if( rc == SXRET_OK ){` |
|       - | 4268 | `					/* Value exist */` |
|       3 | 4269 | `					break;` |
|       - | 4270 | `				}` |
|       3 | 4271 | `			}` |
|       7 | 4272 | `			if( i >= (nArg - 1)){` |
|       - | 4273 | `				/* Perform the insertion */` |
|       5 | 4274 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4275 | `			}` |
|       3 | 4276 | `		}` |
|       - | 4277 | `		/* Point to the next entry */` |
|       7 | 4278 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4279 | `		n--;` |
|       1 | 4280 | `	}` |
|       - | 4281 | `	/* Return the freshly created array */` |
|       3 | 4282 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4283 | `	return PH7_OK;` |
|      12 | 4284 |  |
|       - | 4285 | `/*` |
|       - | 4286 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4287 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4288 | ` * Parameters` |
|       - | 4289 | ` *  $array1` |
|       - | 4290 | ` *    The array to compare from` |
|       - | 4291 | ` *  $array2` |
|       - | 4292 | ` *    An array to compare against` |
|       - | 4293 | ` *  $...` |
|       - | 4294 | ` *   More arrays to compare against` |
|       - | 4295 | ` * Return` |
|       - | 4296 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4297 | ` *  are not present in any of the other arrays.` |
|       - | 4298 | ` */` |
|      20 | 4299 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4300 |  |
|       - | 4301 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4302 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4303 | `	ph7_value *pArray;` |
|       - | 4304 | `	ph7_value *pVal;` |
|       - | 4305 | `	sxi32 rc;` |
|       - | 4306 | `	sxu32 n;` |
|       - | 4307 | `	int i;` |
|       - | 4308 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4309 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4310 | `	 * accompanying integration tests to pass. */` |
|      22 | 4311 | `	if( nArg < 1 ){` |
|       4 | 4312 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4313 | `			"ArgumentCountError",` |
|       - | 4314 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4315 | `			nArg` |
|       - | 4316 | `			);` |
|       - | 4317 | `	}` |
|      20 | 4318 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4319 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4320 | `			"TypeError",` |
|       - | 4321 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4322 | `			ph7_type_name(apArg[0])` |
|       - | 4323 | `			);` |
|       - | 4324 | `	}` |
|      32 | 4325 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4326 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4327 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4328 | `				"TypeError",` |
|       - | 4329 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4330 | `				i + 1,` |
|       4 | 4331 | `				ph7_type_name(apArg[i])` |
|       - | 4332 | `				);` |
|       - | 4333 | `		}` |
|       9 | 4334 | `	}` |
|      13 | 4335 | `	if( nArg == 1 ){` |
|       - | 4336 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4337 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4338 | `		return PH7_OK;` |
|       - | 4339 | `	}` |
|       - | 4340 | `	/* Create a new array */` |
|      11 | 4341 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4342 | `	if( pArray == 0 ){` |
|     ! 0 | 4343 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4344 | `		return PH7_OK;` |
|       - | 4345 | `	}` |
|       - | 4346 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4347 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4348 | `	/* Perform the diff */` |
|      11 | 4349 | `	pEntry = pSrc->pFirst;` |
|      11 | 4350 | `	n = pSrc->nEntry;` |
|      11 | 4351 | `	pN1 = pN2 = 0;` |
|      29 | 4352 | `	for(;;){` |
|       - | 4353 | `		int keep;` |
|      35 | 4354 | `		if( n < 1 ){` |
|      11 | 4355 | `			break;` |
|       - | 4356 | `		}` |
|       - | 4357 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4358 | `		keep = 1;` |
|      41 | 4359 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4360 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4361 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4362 | `			/* Perform a key lookup first */` |
|      29 | 4363 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4364 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4365 | `			}else{` |
|      17 | 4366 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4367 | `			}` |
|      29 | 4368 | `			if( rc != SXRET_OK ){` |
|       - | 4369 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4370 | `				continue;` |
|       - | 4371 | `			}` |
|       - | 4372 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4373 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4374 | `			if( pVal ){` |
|       - | 4375 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4376 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4377 | `				if( pVal2 ){` |
|      15 | 4378 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4379 | `					if( cmp == 0 ){` |
|       - | 4380 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4381 | `						keep = 0;` |
|      13 | 4382 | `						break;` |
|       - | 4383 | `					}` |
|       1 | 4384 | `				}` |
|       1 | 4385 | `			}` |
|       2 | 4386 | `		}` |
|      25 | 4387 | `		if( keep ){` |
|       - | 4388 | `			/* Perform the insertion */` |
|      13 | 4389 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4390 | `		}` |
|       - | 4391 | `		/* Point to the next entry */` |
|      25 | 4392 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4393 | `		n--;` |
|       1 | 4394 | `	}` |
|       - | 4395 | `	/* Return the freshly created array */` |
|      11 | 4396 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4397 | `	return PH7_OK;` |
|      12 | 4398 |  |
|       - | 4399 | `/*` |
|       - | 4400 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4401 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4402 | ` *  by a user supplied callback function.` |
|       - | 4403 | ` * Parameters` |
|       - | 4404 | ` *  $array1` |
|       - | 4405 | ` *    The array to compare from` |
|       - | 4406 | ` *  $array2` |
|       - | 4407 | ` *    An array to compare against` |
|       - | 4408 | ` *  $...` |
|       - | 4409 | ` *   More arrays to compare against.` |
|       - | 4410 | ` *  $key_compare_func` |
|       - | 4411 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4412 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4413 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4414 | ` * Return` |
|       - | 4415 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4416 | ` *  are not present in any of the other arrays.` |
|       - | 4417 | ` */` |
|      22 | 4418 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4419 |  |
|       - | 4420 | `	ph7_hashmap_node *pEntry;` |
|       - | 4421 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4422 | `	ph7_value *pCallback;` |
|       - | 4423 | `	ph7_value *pArray;` |
|       - | 4424 | `	sxi32 rc;` |
|       - | 4425 | `	sxu32 n;` |
|       - | 4426 | `	int i;` |
|       - | 4427 |  |
|       - | 4428 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4429 | `	if( nArg < 2 ){` |
|       4 | 4430 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4431 | `			"ArgumentCountError",` |
|       - | 4432 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4433 | `			nArg` |
|       - | 4434 | `			);` |
|       - | 4435 | `	}` |
|      22 | 4436 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4437 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4438 | `			"TypeError",` |
|       - | 4439 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4440 | `			ph7_type_name(apArg[0])` |
|       - | 4441 | `			);` |
|       - | 4442 | `	}` |
|       - | 4443 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4444 | `	 * expected to be a callback. */` |
|      32 | 4445 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4446 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4447 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4448 | `				"TypeError",` |
|       - | 4449 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4450 | `				i + 1,` |
|       2 | 4451 | `				ph7_type_name(apArg[i])` |
|       - | 4452 | `				);` |
|       - | 4453 | `		}` |
|       8 | 4454 | `	}` |
|       - | 4455 | `	/* Point to the callback value */` |
|      18 | 4456 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4457 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4458 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4459 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4460 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4461 | `		 * string given" which we also reproduce. */` |
|       7 | 4462 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4463 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4464 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4465 | `				"TypeError",` |
|       - | 4466 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4467 | `				nArg` |
|       - | 4468 | `				);` |
|       - | 4469 | `		}` |
|       5 | 4470 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4471 | `			/* neither array nor string */` |
|       7 | 4472 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4473 | `				"TypeError",` |
|       - | 4474 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4475 | `				nArg` |
|       - | 4476 | `				);` |
|       - | 4477 | `		}` |
|       - | 4478 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4479 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4480 | `			"TypeError",` |
|       - | 4481 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4482 | `			nArg,` |
|     ! 0 | 4483 | `			ph7_type_name(pCallback)` |
|       - | 4484 | `			);` |
|       - | 4485 | `	}` |
|      11 | 4486 | `	if( nArg == 2 ){` |
|       - | 4487 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4488 | `		 * input array. */` |
|       3 | 4489 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4490 | `		return PH7_OK;` |
|       - | 4491 | `	}` |
|       - | 4492 | `	/* Create a new array */` |
|       9 | 4493 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4494 | `	if( pArray == 0 ){` |
|     ! 0 | 4495 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4496 | `		return PH7_OK;` |
|       - | 4497 | `	}` |
|       - | 4498 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4499 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4500 | `	/* Perform the diff */` |
|       9 | 4501 | `	pEntry = pSrc->pFirst;` |
|       9 | 4502 | `	n = pSrc->nEntry;` |
|      20 | 4503 | `	for(;;){` |
|       - | 4504 | `		int keep;` |
|      25 | 4505 | `		if( n < 1 ){` |
|       9 | 4506 | `			break;` |
|       - | 4507 | `		}` |
|      17 | 4508 | `		keep = 1;` |
|      29 | 4509 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4510 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4511 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4512 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4513 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4514 | `			while( pIt ){` |
|       - | 4515 | `				/* build temporary key values for callback */` |
|       - | 4516 | `				ph7_value key1, key2, result;` |
|       - | 4517 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4518 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4519 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4520 | `				}else{` |
|       - | 4521 | `					SyString sStr;` |
|      31 | 4522 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4523 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4524 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4525 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4526 | `				}` |
|      31 | 4527 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4528 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4529 | `				}else{` |
|       - | 4530 | `					SyString sStr;` |
|      31 | 4531 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4532 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4533 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4534 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4535 | `				}` |
|      31 | 4536 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4537 | `				/* call user callback with (key1, key2) */` |
|       - | 4538 | `				{` |
|       - | 4539 | `					ph7_value *apK[2];` |
|      31 | 4540 | `					apK[0] = &key1;` |
|      31 | 4541 | `					apK[1] = &key2;` |
|      31 | 4542 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4543 | `				}` |
|      31 | 4544 | `				if( rc == SXRET_OK ){` |
|      31 | 4545 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4546 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4547 | `					}` |
|      31 | 4548 | `					if( result.x.iVal == 0 ){` |
|       - | 4549 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4550 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4551 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4552 | `						if( pVal1 && pVal2 ){` |
|      13 | 4553 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4554 | `								keep = 0;` |
|       9 | 4555 | `								PH7_MemObjRelease(&result);` |
|       - | 4556 | `								/* release keys too before breaking */` |
|       9 | 4557 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4558 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4559 | `								break;` |
|       - | 4560 | `							}` |
|       2 | 4561 | `						}` |
|       2 | 4562 | `					}` |
|      11 | 4563 | `				}` |
|      23 | 4564 | `				PH7_MemObjRelease(&result);` |
|      23 | 4565 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4566 | `				PH7_MemObjRelease(&key2);` |
|       - | 4567 | `				/* move to next node */` |
|      23 | 4568 | `				pIt = pIt->pPrev;` |
|      23 | 4569 | `				if( keep == 0 ) break;` |
|       1 | 4570 | `			}` |
|      21 | 4571 | `			if( keep == 0 ) break;` |
|       7 | 4572 | `		}` |
|      17 | 4573 | `		if( keep ){` |
|       - | 4574 | `			/* Perform the insertion */` |
|       9 | 4575 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4576 | `		}` |
|       - | 4577 | `		/* Point to the next entry */` |
|      17 | 4578 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4579 | `		n--;` |
|       1 | 4580 | `	}` |
|       - | 4581 | `	/* Return the freshly created array */` |
|       9 | 4582 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4583 | `	return PH7_OK;` |
|      13 | 4584 |  |
|       - | 4585 | `/*` |
|       - | 4586 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4587 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4588 | ` * Parameters` |
|       - | 4589 | ` *  $array1` |
|       - | 4590 | ` *    The array to compare from` |
|       - | 4591 | ` *  $array2` |
|       - | 4592 | ` *    An array to compare against` |
|       - | 4593 | ` *  $...` |
|       - | 4594 | ` *   More arrays to compare against` |
|       - | 4595 | ` * Return` |
|       - | 4596 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4597 | ` *  in any of the other arrays.` |
|       - | 4598 | ` * Note that NULL is returned on failure.` |
|       - | 4599 | ` */` |
|      14 | 4600 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4601 |  |
|       - | 4602 | `	ph7_hashmap_node *pEntry;` |
|       - | 4603 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4604 | `	ph7_value *pArray;` |
|       - | 4605 | `	sxi32 rc;` |
|       - | 4606 | `	sxu32 n;` |
|       - | 4607 | `	int i;` |
|       - | 4608 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4609 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4610 | `	 * helpers. */` |
|      16 | 4611 | `	if( nArg < 1 ){` |
|       4 | 4612 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4613 | `			"ArgumentCountError",` |
|       - | 4614 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4615 | `			nArg` |
|       - | 4616 | `			);` |
|       - | 4617 | `	}` |
|      14 | 4618 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4619 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4620 | `			"TypeError",` |
|       - | 4621 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4622 | `			ph7_type_name(apArg[0])` |
|       - | 4623 | `			);` |
|       - | 4624 | `	}` |
|      20 | 4625 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4626 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4627 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4628 | `				"TypeError",` |
|       - | 4629 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4630 | `				i + 1,` |
|       2 | 4631 | `				ph7_type_name(apArg[i])` |
|       - | 4632 | `				);` |
|       - | 4633 | `		}` |
|       5 | 4634 | `	}` |
|       9 | 4635 | `	if( nArg == 1 ){` |
|       - | 4636 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4637 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4638 | `		return PH7_OK;` |
|       - | 4639 | `	}` |
|       - | 4640 | `	/* Create a new array */` |
|       7 | 4641 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4642 | `	if( pArray == 0 ){` |
|     ! 0 | 4643 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4644 | `		return PH7_OK;` |
|       - | 4645 | `	}` |
|       - | 4646 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4647 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4648 | `	/* Perfrom the diff */` |
|       7 | 4649 | `	pEntry = pSrc->pFirst;` |
|       7 | 4650 | `	n = pSrc->nEntry;` |
|      12 | 4651 | `	for(;;){` |
|      25 | 4652 | `		if( n < 1 ){` |
|       7 | 4653 | `			break;` |
|       - | 4654 | `		}` |
|      31 | 4655 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4656 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4657 | `				/* ignore */` |
|     ! 0 | 4658 | `				continue;` |
|       - | 4659 | `			}` |
|      23 | 4660 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4661 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4662 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4663 | `				/* Blob lookup */` |
|      17 | 4664 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4665 | `			}else{` |
|       - | 4666 | `				/* Int lookup */` |
|       7 | 4667 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4668 | `			}` |
|      23 | 4669 | `			if( rc == SXRET_OK ){` |
|       - | 4670 | `				/* Key exists,break immediately */` |
|      11 | 4671 | `				break;` |
|       - | 4672 | `			}` |
|       7 | 4673 | `		}` |
|      19 | 4674 | `		if( i >= nArg ){` |
|       - | 4675 | `			/* Perform the insertion */` |
|       9 | 4676 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4677 | `		}` |
|       - | 4678 | `		/* Point to the next entry */` |
|      19 | 4679 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4680 | `		n--;` |
|       1 | 4681 | `	}` |
|       - | 4682 | `	/* Return the freshly created array */` |
|       7 | 4683 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4684 | `	return PH7_OK;` |
|       9 | 4685 |  |
|       - | 4686 | `/*` |
|       - | 4687 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4688 | ` *  Computes the intersection of arrays.` |
|       - | 4689 | ` * Parameters` |
|       - | 4690 | ` *  $array1` |
|       - | 4691 | ` *    The array to compare from` |
|       - | 4692 | ` *  $array2` |
|       - | 4693 | ` *    An array to compare against` |
|       - | 4694 | ` *  $...` |
|       - | 4695 | ` *   More arrays to compare against` |
|       - | 4696 | ` * Return` |
|       - | 4697 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4698 | ` *  in all of the parameters.` |
|       - | 4699 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4700 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4701 | ` */` |
|      22 | 4702 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4703 |  |
|       - | 4704 | `	ph7_hashmap_node *pEntry;` |
|       - | 4705 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4706 | `	ph7_value *pArray;` |
|       - | 4707 | `	ph7_value *pVal;` |
|       - | 4708 | `	sxi32 rc;` |
|       - | 4709 | `	sxu32 n;` |
|       - | 4710 | `	int i;` |
|      24 | 4711 | `	if( nArg < 1 ){` |
|       4 | 4712 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4713 | `			"ArgumentCountError",` |
|       - | 4714 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4715 | `			nArg` |
|       - | 4716 | `			);` |
|       - | 4717 | `	}` |
|      22 | 4718 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4719 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4720 | `			"TypeError",` |
|       - | 4721 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4722 | `			ph7_type_name(apArg[0])` |
|       - | 4723 | `			);` |
|       - | 4724 | `	}` |
|      36 | 4725 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4726 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4727 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4728 | `				"TypeError",` |
|       - | 4729 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4730 | `				i + 1,` |
|       2 | 4731 | `				ph7_type_name(apArg[i])` |
|       - | 4732 | `				);` |
|       - | 4733 | `		}` |
|       9 | 4734 | `	}` |
|      17 | 4735 | `	if( nArg == 1 ){` |
|       - | 4736 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4737 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4738 | `		return PH7_OK;` |
|       - | 4739 | `	}` |
|       - | 4740 | `	/* Create a new array */` |
|      15 | 4741 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4742 | `	if( pArray == 0 ){` |
|     ! 0 | 4743 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4744 | `		return PH7_OK;` |
|       - | 4745 | `	}` |
|       - | 4746 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4747 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4748 | `	/* Perform the intersection */` |
|      15 | 4749 | `	pEntry = pSrc->pFirst;` |
|      15 | 4750 | `	n = pSrc->nEntry;` |
|      31 | 4751 | `	for(;;){` |
|      63 | 4752 | `		if( n < 1 ){` |
|      15 | 4753 | `			break;` |
|       - | 4754 | `		}` |
|       - | 4755 | `		/* Extract the node value */` |
|      49 | 4756 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4757 | `		if( pVal ){` |
|      79 | 4758 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4759 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4760 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4761 | `				/* Perform the lookup */` |
|      55 | 4762 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4763 | `				if( rc != SXRET_OK ){` |
|       - | 4764 | `					/* Value does not exist */` |
|      25 | 4765 | `					break;` |
|       - | 4766 | `				}` |
|      16 | 4767 | `			}` |
|      49 | 4768 | `			if( i >= nArg ){` |
|       - | 4769 | `				/* Perform the insertion */` |
|      25 | 4770 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4771 | `			}` |
|      24 | 4772 | `		}` |
|       - | 4773 | `		/* Point to the next entry */` |
|      49 | 4774 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4775 | `		n--;` |
|       1 | 4776 | `	}` |
|       - | 4777 | `	/* Return the freshly created array */` |
|      15 | 4778 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4779 | `	return PH7_OK;` |
|      13 | 4780 |  |
|       - | 4781 | `/*` |
|       - | 4782 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4783 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4784 | ` * Parameters` |
|       - | 4785 | ` *  $array1` |
|       - | 4786 | ` *    The array to compare from` |
|       - | 4787 | ` *  $array2` |
|       - | 4788 | ` *    An array to compare against` |
|       - | 4789 | ` *  $...` |
|       - | 4790 | ` *   More arrays to compare against` |
|       - | 4791 | ` * Return` |
|       - | 4792 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4793 | ` *  in all the arguments, with matching keys.` |
|       - | 4794 | ` */` |
|      22 | 4795 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4796 |  |
|       - | 4797 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4798 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4799 | `	ph7_value *pArray;` |
|       - | 4800 | `	ph7_value *pVal;` |
|       - | 4801 | `	sxi32 rc;` |
|       - | 4802 | `	sxu32 n;` |
|       - | 4803 | `	int i;` |
|      24 | 4804 | `	if( nArg < 1 ){` |
|       4 | 4805 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4806 | `			"ArgumentCountError",` |
|       - | 4807 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4808 | `			nArg` |
|       - | 4809 | `			);` |
|       - | 4810 | `	}` |
|      22 | 4811 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4812 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4813 | `			"TypeError",` |
|       - | 4814 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4815 | `			ph7_type_name(apArg[0])` |
|       - | 4816 | `			);` |
|       - | 4817 | `	}` |
|      36 | 4818 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4819 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4820 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4821 | `				"TypeError",` |
|       - | 4822 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4823 | `				i + 1,` |
|       2 | 4824 | `				ph7_type_name(apArg[i])` |
|       - | 4825 | `				);` |
|       - | 4826 | `		}` |
|       9 | 4827 | `	}` |
|      17 | 4828 | `	if( nArg == 1 ){` |
|       - | 4829 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4830 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4831 | `		return PH7_OK;` |
|       - | 4832 | `	}` |
|       - | 4833 | `	/* Create a new array */` |
|      15 | 4834 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4835 | `	if( pArray == 0 ){` |
|     ! 0 | 4836 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4837 | `		return PH7_OK;` |
|       - | 4838 | `	}` |
|       - | 4839 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4840 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4841 | `	/* Perform the intersection */` |
|      15 | 4842 | `	pEntry = pSrc->pFirst;` |
|      15 | 4843 | `	n = pSrc->nEntry;` |
|      15 | 4844 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4845 | `	for(;;){` |
|      47 | 4846 | `		if( n < 1 ){` |
|      15 | 4847 | `			break;` |
|       - | 4848 | `		}` |
|       - | 4849 | `		/* Extract the node value */` |
|      33 | 4850 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4851 | `		if( pVal ){` |
|      53 | 4852 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4853 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4854 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4855 | `				/* Perform a key lookup first */` |
|      37 | 4856 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4857 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4858 | `				}else{` |
|      23 | 4859 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4860 | `				}` |
|      37 | 4861 | `				if( rc != SXRET_OK ){` |
|       - | 4862 | `					/* No such key,break immediately */` |
|       7 | 4863 | `					break;` |
|       - | 4864 | `				}` |
|       - | 4865 | `				/* Perform the lookup */` |
|      31 | 4866 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4867 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4868 | `					/* Value does not exist */` |
|       6 | 4869 | `					break;` |
|       - | 4870 | `				}` |
|      11 | 4871 | `			}` |
|      33 | 4872 | `			if( i >= nArg ){` |
|       - | 4873 | `				/* Perform the insertion */` |
|      17 | 4874 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4875 | `			}` |
|      16 | 4876 | `		}` |
|       - | 4877 | `		/* Point to the next entry */` |
|      33 | 4878 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4879 | `		n--;` |
|       1 | 4880 | `	}` |
|       - | 4881 | `	/* Return the freshly created array */` |
|      15 | 4882 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4883 | `	return PH7_OK;` |
|      13 | 4884 |  |
|       - | 4885 | `/*` |
|       - | 4886 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 4887 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4888 | ` * Parameters` |
|       - | 4889 | ` *  $array1` |
|       - | 4890 | ` *    The array to compare from` |
|       - | 4891 | ` *  $...` |
|       - | 4892 | ` *   More arrays to compare against` |
|       - | 4893 | ` * Return` |
|       - | 4894 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4895 | ` *  have keys that are present in all arguments.` |
|       - | 4896 | ` * Note that NULL is returned on failure.` |
|       - | 4897 | ` */` |
|      22 | 4898 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4899 |  |
|       - | 4900 | `	ph7_hashmap_node *pEntry;` |
|       - | 4901 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4902 | `	ph7_value *pArray;` |
|       - | 4903 | `	sxi32 rc;` |
|       - | 4904 | `	sxu32 n;` |
|       - | 4905 | `	int i;` |
|      24 | 4906 | `	if( nArg < 1 ){` |
|       4 | 4907 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4908 | `			"ArgumentCountError",` |
|       - | 4909 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 4910 | `			nArg` |
|       - | 4911 | `			);` |
|       - | 4912 | `	}` |
|      22 | 4913 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4914 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4915 | `			"TypeError",` |
|       - | 4916 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4917 | `			ph7_type_name(apArg[0])` |
|       - | 4918 | `			);` |
|       - | 4919 | `	}` |
|      36 | 4920 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4921 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4922 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4923 | `				"TypeError",` |
|       - | 4924 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4925 | `				i + 1,` |
|       2 | 4926 | `				ph7_type_name(apArg[i])` |
|       - | 4927 | `				);` |
|       - | 4928 | `		}` |
|       9 | 4929 | `	}` |
|      17 | 4930 | `	if( nArg == 1 ){` |
|       - | 4931 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4932 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4933 | `		return PH7_OK;` |
|       - | 4934 | `	}` |
|       - | 4935 | `	/* Create a new array */` |
|      15 | 4936 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4937 | `	if( pArray == 0 ){` |
|     ! 0 | 4938 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4939 | `		return PH7_OK;` |
|       - | 4940 | `	}` |
|       - | 4941 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 4942 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4943 | `	/* Perform the intersection */` |
|      15 | 4944 | `	pEntry = pSrc->pFirst;` |
|      15 | 4945 | `	n = pSrc->nEntry;` |
|      24 | 4946 | `	for(;;){` |
|      49 | 4947 | `		if( n < 1 ){` |
|      15 | 4948 | `			break;` |
|       - | 4949 | `		}` |
|      57 | 4950 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 4951 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 4952 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 4953 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4954 | `				/* Blob lookup */` |
|      27 | 4955 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 4956 | `			}else{` |
|       - | 4957 | `				/* Int key */` |
|      13 | 4958 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4959 | `			}` |
|      39 | 4960 | `			if( rc != SXRET_OK ){` |
|       - | 4961 | `				/* Key does not exist, break immediately */` |
|      17 | 4962 | `				break;` |
|       - | 4963 | `			}` |
|      12 | 4964 | `		}` |
|      35 | 4965 | `		if( i >= nArg ){` |
|       - | 4966 | `			/* Perform the insertion */` |
|      19 | 4967 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 4968 | `		}` |
|       - | 4969 | `		/* Point to the next entry */` |
|      35 | 4970 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 4971 | `		n--;` |
|       1 | 4972 | `	}` |
|       - | 4973 | `	/* Return the freshly created array */` |
|      15 | 4974 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4975 | `	return PH7_OK;` |
|      13 | 4976 |  |
|       - | 4977 | `/*` |
|       - | 4978 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4979 | ` *  Computes the intersection of arrays.` |
|       - | 4980 | ` * Parameters` |
|       - | 4981 | ` *  $array1` |
|       - | 4982 | ` *    The array to compare from` |
|       - | 4983 | ` *  $array2` |
|       - | 4984 | ` *    An array to compare against` |
|       - | 4985 | ` *  $...` |
|       - | 4986 | ` *   More arrays to compare against` |
|       - | 4987 | ` * $callback` |
|       - | 4988 | ` *  The callback comparison function.` |
|       - | 4989 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4990 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4991 | ` *  than the second.` |
|       - | 4992 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4993 | ` * Return` |
|       - | 4994 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4995 | ` *  in all of the parameters. .` |
|       - | 4996 | ` * Note that NULL is returned on failure.` |
|       - | 4997 | ` */` |
|      24 | 4998 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4999 |  |
|       - | 5000 | `	ph7_hashmap_node *pEntry;` |
|       - | 5001 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5002 | `	ph7_value *pCallback;` |
|       - | 5003 | `	ph7_value *pArray;` |
|       - | 5004 | `	ph7_value *pVal;` |
|       - | 5005 | `	sxi32 rc;` |
|       - | 5006 | `	sxu32 n;` |
|       - | 5007 | `	int i;` |
|       - | 5008 |  |
|       - | 5009 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      26 | 5010 | `	if( nArg < 2 ){` |
|       4 | 5011 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5012 | `			"ArgumentCountError",` |
|       - | 5013 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5014 | `			nArg` |
|       - | 5015 | `			);` |
|       - | 5016 | `	}` |
|      24 | 5017 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5018 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5019 | `			"TypeError",` |
|       - | 5020 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5021 | `			ph7_type_name(apArg[0])` |
|       - | 5022 | `			);` |
|       - | 5023 | `	}` |
|       - | 5024 |  |
|      22 | 5025 | `	if( nArg == 2 ){` |
|       - | 5026 | `		/* Only the original array and the callback were provided. */` |
|       - | 5027 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5028 | `		 * validation ordering. */` |
|       3 | 5029 | `	} else {` |
|       - | 5030 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      32 | 5031 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      18 | 5032 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5033 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5034 | `					"TypeError",` |
|       - | 5035 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5036 | `					i + 1,` |
|       2 | 5037 | `					ph7_type_name(apArg[i])` |
|       - | 5038 | `					);` |
|       - | 5039 | `			}` |
|       9 | 5040 | `		}` |
|       - | 5041 | `	}` |
|       - | 5042 |  |
|       - | 5043 | `	/* Identify the callback (always expected as the last argument). */` |
|      20 | 5044 | `	pCallback = apArg[nArg - 1];` |
|       - | 5045 | `	/* Validate the callback to match PHP's error messages. */` |
|      20 | 5046 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      11 | 5047 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5048 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5049 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5050 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5051 | `			 */` |
|       7 | 5052 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       7 | 5053 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5054 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5055 | `					"TypeError",` |
|       - | 5056 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5057 | `					nArg` |
|       - | 5058 | `					);` |
|       - | 5059 | `			}` |
|       - | 5060 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5061 | `			{` |
|       5 | 5062 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       5 | 5063 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       5 | 5064 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5065 | `					int nMethodLen;` |
|       5 | 5066 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       5 | 5067 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       5 | 5068 | `					if( pClass ){` |
|       - | 5069 | `						/* Class exists but method is missing. */` |
|       4 | 5070 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5071 | `							"TypeError",` |
|       - | 5072 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5073 | `							nArg,` |
|       1 | 5074 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5075 | `							zMethod` |
|       - | 5076 | `							);` |
|       - | 5077 | `					}` |
|       - | 5078 | `					/* Class not found */` |
|       - | 5079 | `					{` |
|       - | 5080 | `						int nName;` |
|       3 | 5081 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5082 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5083 | `							"TypeError",` |
|       - | 5084 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5085 | `							nArg,` |
|       1 | 5086 | `							zName` |
|       - | 5087 | `							);` |
|       - | 5088 | `					}` |
|       - | 5089 | `				}` |
|       - | 5090 | `			}` |
|       - | 5091 | `			/* Fallback message */` |
|     ! 0 | 5092 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5093 | `				"TypeError",` |
|       - | 5094 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5095 | `				nArg` |
|       - | 5096 | `				);` |
|       - | 5097 | `		}` |
|       5 | 5098 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5099 | `			int len;` |
|       3 | 5100 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5101 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5102 | `				"TypeError",` |
|       - | 5103 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5104 | `				nArg,` |
|       1 | 5105 | `				zName` |
|       - | 5106 | `				);` |
|       - | 5107 | `		}` |
|       4 | 5108 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5109 | `			"TypeError",` |
|       - | 5110 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5111 | `			nArg` |
|       - | 5112 | `			);` |
|       - | 5113 | `	}` |
|       - | 5114 |  |
|       9 | 5115 | `	if( nArg == 2 ){` |
|       - | 5116 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5117 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5118 | `		return PH7_OK;` |
|       - | 5119 | `	}` |
|       - | 5120 |  |
|       - | 5121 | `	/* Create a new array */` |
|       5 | 5122 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 5123 | `	if( pArray == 0 ){` |
|     ! 0 | 5124 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5125 | `		return PH7_OK;` |
|       - | 5126 | `	}` |
|       - | 5127 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 5128 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5129 | `	/* Perform the intersection */` |
|       5 | 5130 | `	pEntry = pSrc->pFirst;` |
|       5 | 5131 | `	n = pSrc->nEntry;` |
|       8 | 5132 | `	for(;;){` |
|      17 | 5133 | `		if( n < 1 ){` |
|       5 | 5134 | `			break;` |
|       - | 5135 | `		}` |
|       - | 5136 | `		/* Extract the node value */` |
|      13 | 5137 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      13 | 5138 | `		if( pVal ){` |
|      21 | 5139 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      13 | 5140 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5141 | `					/* ignore */` |
|     ! 0 | 5142 | `					continue;` |
|       - | 5143 | `				}` |
|       - | 5144 | `				/* Point to the internal representation of the hashmap */` |
|      13 | 5145 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5146 | `				/* Perform the lookup */` |
|      13 | 5147 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      13 | 5148 | `				if( rc != SXRET_OK ){` |
|       - | 5149 | `					/* Value does not exist */` |
|       5 | 5150 | `					break;` |
|       - | 5151 | `				}` |
|       5 | 5152 | `			}` |
|      13 | 5153 | `			if( i >= (nArg-1) ){` |
|       - | 5154 | `				/* Perform the insertion */` |
|       9 | 5155 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5156 | `			}` |
|       6 | 5157 | `		}` |
|       - | 5158 | `		/* Point to the next entry */` |
|      13 | 5159 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5160 | `		n--;` |
|       1 | 5161 | `	}` |
|       - | 5162 | `	/* Return the freshly created array */` |
|       5 | 5163 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5164 | `	return PH7_OK;` |
|      14 | 5165 |  |
|       - | 5166 | `/*` |
|       - | 5167 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5168 | ` *  Fill an array with values.` |
|       - | 5169 | ` * Parameters` |
|       - | 5170 | ` *  $start_index` |
|       - | 5171 | ` *    The first index of the returned array.` |
|       - | 5172 | ` *  $num` |
|       - | 5173 | ` *   Number of elements to insert.` |
|       - | 5174 | ` *  $value` |
|       - | 5175 | ` *    Value to use for filling.` |
|       - | 5176 | ` * Return` |
|       - | 5177 | ` *  The filled array or null on failure.` |
|       - | 5178 | ` */` |
|     238 | 5179 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5180 |  |
|       - | 5181 | `	ph7_value *pArray;` |
|       - | 5182 | `	int i,nEntry;` |
|       - | 5183 |  |
|       - | 5184 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 5185 | `	if( nArg != 3 ){` |
|       - | 5186 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 5187 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5188 | `			"ArgumentCountError",` |
|       - | 5189 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5190 | `			nArg` |
|       - | 5191 | `			);` |
|       - | 5192 | `	}` |
|       - | 5193 |  |
|       - | 5194 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5195 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5196 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5197 | `	 * and NULLs are rejected outright. */` |
|     466 | 5198 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 5199 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5200 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5201 | `			"TypeError",` |
|       - | 5202 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5203 | `			ph7_type_name(apArg[0])` |
|       - | 5204 | `			);` |
|       - | 5205 | `	}` |
|     234 | 5206 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5207 | `		int len;` |
|       8 | 5208 | `		sxu8 bReal = FALSE;` |
|       8 | 5209 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5210 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5211 | `			/* Non‑numeric string is an error. */` |
|       3 | 5212 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5213 | `				"TypeError",` |
|       - | 5214 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5215 | `				);` |
|       - | 5216 | `		}` |
|       5 | 5217 | `		if( bReal ){` |
|       - | 5218 | `			/* float-string -> deprecation warning */` |
|       4 | 5219 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5220 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5221 | `				zStr` |
|       - | 5222 | `				);` |
|       1 | 5223 | `		}` |
|       2 | 5224 | `	}` |
|       - | 5225 |  |
|       - | 5226 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5227 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 5228 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 5229 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5230 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5231 | `			"TypeError",` |
|       - | 5232 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5233 | `			ph7_type_name(apArg[1])` |
|       - | 5234 | `			);` |
|       - | 5235 | `	}` |
|     232 | 5236 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5237 | `		int len;` |
|       3 | 5238 | `		sxu8 bReal = FALSE;` |
|       3 | 5239 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5240 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5241 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5242 | `				"TypeError",` |
|       - | 5243 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5244 | `				);` |
|       - | 5245 | `		}` |
|     ! 0 | 5246 | `	}` |
|       - | 5247 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5248 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5249 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5250 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5251 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5252 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5253 | `		if( d != (double)i64 ){` |
|       7 | 5254 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5255 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5256 | `				d` |
|       - | 5257 | `				);` |
|       2 | 5258 | `		}` |
|       2 | 5259 | `	}` |
|       - | 5260 |  |
|       - | 5261 | `	/* Total number of entries to insert */` |
|     230 | 5262 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5263 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5264 | `	if( nEntry < 0 ){` |
|       3 | 5265 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5266 | `			"ValueError",` |
|       - | 5267 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5268 | `			);` |
|       - | 5269 | `	}` |
|       - | 5270 |  |
|       - | 5271 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5272 | `	if( nEntry == 0 ){` |
|       7 | 5273 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5274 | `		return PH7_OK;` |
|       - | 5275 | `	}` |
|       - | 5276 |  |
|       - | 5277 | `	/* Create a new array */` |
|     221 | 5278 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5279 | `	if( pArray == 0 ){` |
|     ! 0 | 5280 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5281 | `		return PH7_OK;` |
|       - | 5282 | `	}` |
|       - | 5283 |  |
|       - | 5284 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5285 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 5286 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5287 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5288 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 5289 | `	}` |
|       - | 5290 | `	/* Return the filled array */` |
|     221 | 5291 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5292 | `	return PH7_OK;` |
|     121 | 5293 |  |
|       - | 5294 | `/*` |
|       - | 5295 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5296 | ` *  Fill an array with values, specifying keys.` |
|       - | 5297 | ` * Parameters` |
|       - | 5298 | ` *  $input` |
|       - | 5299 | ` *   Array of values that will be used as key.` |
|       - | 5300 | ` *  $value` |
|       - | 5301 | ` *    Value to use for filling.` |
|       - | 5302 | ` * Return` |
|       - | 5303 | ` *  The filled array.` |
|       - | 5304 | ` * Throws` |
|       - | 5305 | ` *  ValueError if $input is not an array.` |
|       - | 5306 | ` */` |
|      26 | 5307 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5308 |  |
|       - | 5309 | `	ph7_hashmap_node *pEntry;` |
|       - | 5310 | `	ph7_hashmap *pSrc;` |
|       - | 5311 | `	ph7_value *pArray;` |
|       - | 5312 | `	sxu32 n;` |
|       - | 5313 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5314 | `	if( nArg != 2 ){` |
|      10 | 5315 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5316 | `			"ArgumentCountError",` |
|       - | 5317 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5318 | `			nArg` |
|       - | 5319 | `			);` |
|       - | 5320 | `	}` |
|       - | 5321 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5322 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5323 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5324 | `			"TypeError",` |
|       - | 5325 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5326 | `			ph7_type_name(apArg[0])` |
|       - | 5327 | `			);` |
|       - | 5328 | `	}` |
|       - | 5329 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5330 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5331 | `	/* Create a new array */` |
|      17 | 5332 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5333 | `	if( pArray == 0 ){` |
|     ! 0 | 5334 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5335 | `		return PH7_OK;` |
|       - | 5336 | `	}` |
|       - | 5337 | `	/* Perform the requested operation */` |
|      17 | 5338 | `	pEntry = pSrc->pFirst;` |
|      45 | 5339 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5340 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5341 | `		/* Point to the next entry */` |
|      29 | 5342 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5343 | `	}` |
|       - | 5344 | `	/* Return the filled array */` |
|      17 | 5345 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5346 | `	return PH7_OK;` |
|      15 | 5347 |  |
|       - | 5348 | `/*` |
|       - | 5349 | ` * array array_combine(array $keys,array $values)` |
|       - | 5350 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5351 | ` * Parameters` |
|       - | 5352 | ` *  $keys` |
|       - | 5353 | ` *    Array of keys to be used.` |
|       - | 5354 | ` * $values` |
|       - | 5355 | ` *   Array of values to be used.` |
|       - | 5356 | ` * Return` |
|       - | 5357 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5358 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5359 | ` *  not an array.` |
|       - | 5360 | ` */` |
|      18 | 5361 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5362 |  |
|       - | 5363 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5364 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5365 | `	ph7_value *pArray;` |
|       - | 5366 | `	sxu32 n;` |
|       - | 5367 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5368 | `	if( nArg != 2 ){` |
|       - | 5369 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5370 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5371 | `			"ArgumentCountError",` |
|       - | 5372 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5373 | `			nArg` |
|       - | 5374 | `			);` |
|       - | 5375 | `	}` |
|       - | 5376 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5377 | `	 * argument index in the error message. */` |
|      18 | 5378 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5379 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5380 | `			"TypeError",` |
|       - | 5381 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5382 | `			ph7_type_name(apArg[0])` |
|       - | 5383 | `			);` |
|       - | 5384 | `	}` |
|      16 | 5385 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5386 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5387 | `			"TypeError",` |
|       - | 5388 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5389 | `			ph7_type_name(apArg[1])` |
|       - | 5390 | `			);` |
|       - | 5391 | `	}` |
|       - | 5392 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5393 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5394 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5395 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5396 | `		/* Length mismatch -> ValueError */` |
|       3 | 5397 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5398 | `			"ValueError",` |
|       - | 5399 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5400 | `			);` |
|       - | 5401 | `	}` |
|       - | 5402 | `	/* Create a new array */` |
|      11 | 5403 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5404 | `	if( pArray == 0 ){` |
|     ! 0 | 5405 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5406 | `		return PH7_OK;` |
|       - | 5407 | `	}` |
|       - | 5408 | `	/* Perform the requested operation */` |
|      11 | 5409 | `	pKe = pKey->pFirst;` |
|      11 | 5410 | `	pVe = pValue->pFirst;` |
|      33 | 5411 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5412 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5413 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5414 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5415 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5416 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5417 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5418 | `		 * original array must not be mutated. */` |
|      23 | 5419 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5420 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5421 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5422 | `			if( pTmpKey ){` |
|       5 | 5423 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5424 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5425 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5426 | `				pKeyCopy = pTmpKey;` |
|       2 | 5427 | `			}` |
|       2 | 5428 | `		}` |
|      23 | 5429 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5430 | `		/* Point to the next entry */` |
|      23 | 5431 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5432 | `		pVe = pVe->pPrev;` |
|      12 | 5433 | `	}` |
|       - | 5434 | `	/* Return the filled array */` |
|      11 | 5435 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5436 | `	return PH7_OK;` |
|      11 | 5437 |  |
|       - | 5438 | `/*` |
|       - | 5439 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5440 | ` *  Return an array with elements in reverse order.` |
|       - | 5441 | ` * Parameters` |
|       - | 5442 | ` *  $array` |
|       - | 5443 | ` *   The input array.` |
|       - | 5444 | ` *  $preserve_keys (optional)` |
|       - | 5445 | ` *   If set to TRUE keys are preserved.` |
|       - | 5446 | ` * Return` |
|       - | 5447 | ` *  The reversed array.` |
|       - | 5448 | ` */` |
|      20 | 5449 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5450 |  |
|       - | 5451 | `	ph7_hashmap_node *pEntry;` |
|       - | 5452 | `	ph7_hashmap *pSrc;` |
|       - | 5453 | `	ph7_value *pArray;` |
|       - | 5454 | `	int bPreserve;` |
|       - | 5455 | `	sxu32 n;` |
|      22 | 5456 | `	if( nArg < 1 ){` |
|       4 | 5457 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5458 | `			"ArgumentCountError",` |
|       - | 5459 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5460 | `			nArg` |
|       - | 5461 | `			);` |
|       - | 5462 | `	}` |
|       - | 5463 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5464 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5465 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5466 | `			"TypeError",` |
|       - | 5467 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5468 | `			ph7_type_name(apArg[0])` |
|       - | 5469 | `			);` |
|       - | 5470 | `	}` |
|      17 | 5471 | `	bPreserve = FALSE;` |
|      17 | 5472 | `	if( nArg > 1 ){` |
|       7 | 5473 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5474 | `	}` |
|       - | 5475 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5476 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5477 | `	/* Create a new array */` |
|      17 | 5478 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5479 | `	if( pArray == 0 ){` |
|     ! 0 | 5480 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5481 | `		return PH7_OK;` |
|       - | 5482 | `	}` |
|       - | 5483 | `	/* Perform the requested operation */` |
|      17 | 5484 | `	pEntry = pSrc->pLast;` |
|      55 | 5485 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5486 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5487 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5488 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5489 | `		/* Point to the previous entry */` |
|      39 | 5490 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5491 | `	}` |
|      17 | 5492 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5493 | `	return PH7_OK;` |
|      12 | 5494 |  |
|       - | 5495 | `/*` |
|       - | 5496 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5497 | ` *  Removes duplicate values from an array.` |
|       - | 5498 | ` * Parameters` |
|       - | 5499 | ` *  $array` |
|       - | 5500 | ` *   The input array.` |
|       - | 5501 | ` *  $flags` |
|       - | 5502 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5503 | ` *   behavior using these values:` |
|       - | 5504 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5505 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5506 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5507 | ` * Return` |
|       - | 5508 | ` *  The filtered array.` |
|       - | 5509 | ` */` |
|      24 | 5510 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5511 |  |
|       - | 5512 | `	ph7_hashmap_node *pEntry;` |
|       - | 5513 | `	ph7_value *pNeedle;` |
|       - | 5514 | `	ph7_hashmap *pSrc;` |
|       - | 5515 | `	ph7_value *pArray;` |
|       - | 5516 | `	int bStrict;` |
|       - | 5517 | `	sxi32 rc;` |
|       - | 5518 | `	sxu32 n;` |
|      26 | 5519 | `	if( nArg < 1 ){` |
|       - | 5520 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5521 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5522 | `			"ArgumentCountError",` |
|       - | 5523 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5524 | `			);` |
|       - | 5525 | `	}` |
|      24 | 5526 | `	if( nArg > 2 ){` |
|       - | 5527 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5528 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5529 | `			"ArgumentCountError",` |
|       - | 5530 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5531 | `			nArg` |
|       - | 5532 | `			);` |
|       - | 5533 | `	}` |
|       - | 5534 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5535 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5536 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5537 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5538 | `			"TypeError",` |
|       - | 5539 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5540 | `			ph7_type_name(apArg[0])` |
|       - | 5541 | `			);` |
|       - | 5542 | `	}` |
|      19 | 5543 | `	bStrict = FALSE;` |
|       - | 5544 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5545 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5546 | `	/* Create a new array */` |
|      19 | 5547 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5548 | `	if( pArray == 0 ){` |
|     ! 0 | 5549 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5550 | `		return PH7_OK;` |
|       - | 5551 | `	}` |
|       - | 5552 | `	/* Perform the requested operation */` |
|      19 | 5553 | `	pEntry = pSrc->pFirst;` |
|      83 | 5554 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5555 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5556 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5557 | `		if( pNeedle ){` |
|      65 | 5558 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5559 | `		}` |
|      65 | 5560 | `		if( rc != SXRET_OK ){` |
|       - | 5561 | `			/* Perform the insertion */` |
|      37 | 5562 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5563 | `		}` |
|       - | 5564 | `		/* Point to the next entry */` |
|      65 | 5565 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5566 | `	}` |
|       - | 5567 | `	/* Return the freshly created array */` |
|      19 | 5568 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5569 | `	return PH7_OK;` |
|      14 | 5570 |  |
|       - | 5571 | `/*` |
|       - | 5572 | ` * array array_flip(array $input)` |
|       - | 5573 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5574 | ` * Parameter` |
|       - | 5575 | ` *  $input` |
|       - | 5576 | ` *   Input array.` |
|       - | 5577 | ` * Return` |
|       - | 5578 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5579 | ` */` |
|      34 | 5580 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5581 |  |
|       - | 5582 | `	ph7_hashmap_node *pEntry;` |
|       - | 5583 | `	ph7_hashmap *pSrc;` |
|       - | 5584 | `	ph7_value *pArray;` |
|       - | 5585 | `	ph7_value *pKey;` |
|       - | 5586 | `	ph7_value sVal;` |
|       - | 5587 | `	sxu32 n;` |
|       - | 5588 |  |
|       - | 5589 | `	/* PHP requires exactly one argument */` |
|      36 | 5590 | `	if( nArg != 1 ){` |
|       - | 5591 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5592 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5593 | `			"ArgumentCountError",` |
|       - | 5594 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5595 | `			nArg` |
|       - | 5596 | `			);` |
|       - | 5597 | `	}` |
|       - | 5598 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5599 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5600 | `		/* Type mismatch -> TypeError */` |
|       7 | 5601 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5602 | `			"TypeError",` |
|       - | 5603 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5604 | `			ph7_type_name(apArg[0])` |
|       - | 5605 | `			);` |
|       - | 5606 | `	}` |
|       - | 5607 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5608 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5609 | `	/* Create a new array */` |
|      27 | 5610 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5611 | `	if( pArray == 0 ){` |
|     ! 0 | 5612 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5613 | `		return PH7_OK;` |
|       - | 5614 | `	}` |
|       - | 5615 | `	/* Start processing */` |
|      27 | 5616 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5617 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5618 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5619 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5620 | `		if( pKey ){` |
|       - | 5621 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5622 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5623 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5624 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5625 | `					);` |
|   22236 | 5626 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5627 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5628 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5629 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5630 | `				}else{` |
|       - | 5631 | `					SyString sStr;` |
|    2227 | 5632 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5633 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5634 | `				}` |
|       - | 5635 | `				/* Perform the insertion */` |
|   22227 | 5636 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5637 | `				/* Safely release the value because each inserted entry` |
|       - | 5638 | `				 * has its own private copy of the value.` |
|       - | 5639 | `				 */` |
|   22227 | 5640 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5641 | `			}else{` |
|       - | 5642 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5643 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5644 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5645 | `					);` |
|       - | 5646 | `			}` |
|   11118 | 5647 | `		}` |
|       - | 5648 | `		/* Point to the next entry */` |
|   22237 | 5649 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5650 | `	}` |
|       - | 5651 | `	/* Return the freshly created array */` |
|      27 | 5652 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5653 | `	return PH7_OK;` |
|      19 | 5654 |  |
|       - | 5655 | `/*` |
|       - | 5656 | ` * number array_sum(array $array )` |
|       - | 5657 | ` *  Calculate the sum of values in an array.` |
|       - | 5658 | ` * Parameters` |
|       - | 5659 | ` *  $array: The input array.` |
|       - | 5660 | ` * Return` |
|       - | 5661 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5662 | ` */` |
|      24 | 5663 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5664 |  |
|       - | 5665 | `	ph7_hashmap_node *pEntry;` |
|       - | 5666 | `	ph7_value *pObj;` |
|      25 | 5667 | `	double dSum = 0;` |
|       - | 5668 | `	sxu32 n;` |
|      25 | 5669 | `	pEntry = pMap->pFirst;` |
|      91 | 5670 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5671 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5672 | `		if( pObj ){` |
|      67 | 5673 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5674 | `				dSum += pObj->rVal;` |
|      53 | 5675 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5676 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5677 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5678 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5679 | `					double dv = 0;` |
|      13 | 5680 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5681 | `					dSum += dv;` |
|       7 | 5682 | `				}` |
|      12 | 5683 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5684 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5685 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5686 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5687 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5688 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5689 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5690 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5691 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5692 | `			}` |
|       - | 5693 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5694 | `		}` |
|       - | 5695 | `		/* Point to the next entry */` |
|      67 | 5696 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5697 | `	}` |
|       - | 5698 | `	/* Return sum */` |
|      25 | 5699 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5700 |  |
|      26 | 5701 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5702 |  |
|       - | 5703 | `	ph7_hashmap_node *pEntry;` |
|       - | 5704 | `	ph7_value *pObj;` |
|      28 | 5705 | `	sxi64 nSum = 0;` |
|       - | 5706 | `	sxu32 n;` |
|      28 | 5707 | `	pEntry = pMap->pFirst;` |
|     112 | 5708 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      86 | 5709 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      86 | 5710 | `		if( pObj ){` |
|      86 | 5711 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      76 | 5712 | `				nSum += pObj->x.iVal;` |
|      48 | 5713 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5714 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5715 | `					sxi64 nv = 0;` |
|       5 | 5716 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5717 | `					nSum += nv;` |
|       3 | 5718 | `				}` |
|       8 | 5719 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5720 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5721 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5722 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5723 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5724 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5725 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5726 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5727 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5728 | `			}` |
|       - | 5729 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      42 | 5730 | `		}` |
|       - | 5731 | `		/* Point to the next entry */` |
|      86 | 5732 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      44 | 5733 | `	}` |
|       - | 5734 | `	/* Return sum */` |
|      28 | 5735 | `	ph7_result_int64(pCtx,nSum);` |
|      28 | 5736 |  |
|       - | 5737 | `/* number array_sum(array $array )` |
|       - | 5738 | ` * (See block-coment above)` |
|       - | 5739 | ` */` |
|      64 | 5740 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5741 |  |
|       - | 5742 | `	ph7_hashmap_node *pEntry;` |
|       - | 5743 | `	ph7_hashmap *pMap;` |
|       - | 5744 | `	ph7_value *pObj;` |
|      66 | 5745 | `	int useDouble = 0;` |
|       - | 5746 | `	sxu32 n;` |
|       - | 5747 | `	/* PHP requires exactly one argument */` |
|      66 | 5748 | `	if( nArg != 1 ){` |
|       7 | 5749 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5750 | `			"ArgumentCountError",` |
|       - | 5751 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5752 | `			nArg` |
|       - | 5753 | `			);` |
|       - | 5754 | `	}` |
|       - | 5755 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 5756 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5757 | `		/* Type mismatch -> TypeError */` |
|       7 | 5758 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5759 | `			"TypeError",` |
|       - | 5760 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5761 | `			ph7_type_name(apArg[0])` |
|       - | 5762 | `			);` |
|       - | 5763 | `	}` |
|      58 | 5764 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      58 | 5765 | `	if( pMap->nEntry < 1 ){` |
|       - | 5766 | `		/* Nothing to compute,return 0 */` |
|       7 | 5767 | `		ph7_result_int(pCtx,0);` |
|       7 | 5768 | `		return PH7_OK;` |
|       - | 5769 | `	}` |
|       - | 5770 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5771 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5772 | `	 */` |
|      52 | 5773 | `	pEntry = pMap->pFirst;` |
|     144 | 5774 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     118 | 5775 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     118 | 5776 | `		if( pObj ){` |
|     118 | 5777 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5778 | `				useDouble = 1;` |
|      19 | 5779 | `				break;` |
|       - | 5780 | `			}` |
|     100 | 5781 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5782 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5783 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5784 | `				sxu32 i;` |
|      23 | 5785 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5786 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5787 | `						useDouble = 1;` |
|       7 | 5788 | `						break;` |
|       - | 5789 | `					}` |
|       6 | 5790 | `				}` |
|      13 | 5791 | `				if( useDouble ){` |
|       7 | 5792 | `					break;` |
|       - | 5793 | `				}` |
|       3 | 5794 | `			}` |
|      46 | 5795 | `		}` |
|      94 | 5796 | `		pEntry = pEntry->pPrev;` |
|      48 | 5797 | `	}` |
|      52 | 5798 | `	if( useDouble ){` |
|      25 | 5799 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5800 | `	}else{` |
|      28 | 5801 | `		Int64Sum(pCtx,pMap);` |
|       - | 5802 | `	}` |
|      52 | 5803 | `	return PH7_OK;` |
|      34 | 5804 |  |
|       - | 5805 | `/*` |
|       - | 5806 | ` * number array_product(array $array )` |
|       - | 5807 | ` *  Calculate the product of values in an array.` |
|       - | 5808 | ` * Parameters` |
|       - | 5809 | ` *  $array: The input array.` |
|       - | 5810 | ` * Return` |
|       - | 5811 | ` *  Returns the product of values as an integer or float.` |
|       - | 5812 | ` */` |
|     ! 0 | 5813 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5814 |  |
|       - | 5815 | `	ph7_hashmap_node *pEntry;` |
|       - | 5816 | `	ph7_value *pObj;` |
|       - | 5817 | `	double dProd;` |
|       - | 5818 | `	sxu32 n;` |
|     ! 0 | 5819 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5820 | `	dProd = 1;` |
|     ! 0 | 5821 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5822 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5823 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5824 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5825 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5826 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5827 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5828 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5829 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5830 | `					double dv = 0;` |
|     ! 0 | 5831 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5832 | `					dProd *= dv;` |
|     ! 0 | 5833 | `				}` |
|     ! 0 | 5834 | `			}` |
|     ! 0 | 5835 | `		}` |
|       - | 5836 | `		/* Point to the next entry */` |
|     ! 0 | 5837 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5838 | `	}` |
|       - | 5839 | `	/* Return product */` |
|     ! 0 | 5840 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5841 |  |
|     ! 0 | 5842 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5843 |  |
|       - | 5844 | `	ph7_hashmap_node *pEntry;` |
|       - | 5845 | `	ph7_value *pObj;` |
|       - | 5846 | `	sxi64 nProd;` |
|       - | 5847 | `	sxu32 n;` |
|     ! 0 | 5848 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5849 | `	nProd = 1;` |
|     ! 0 | 5850 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5851 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5852 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5853 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5854 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5855 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5856 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5857 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5858 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5859 | `					sxi64 nv = 0;` |
|     ! 0 | 5860 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5861 | `					nProd *= nv;` |
|     ! 0 | 5862 | `				}` |
|     ! 0 | 5863 | `			}` |
|     ! 0 | 5864 | `		}` |
|       - | 5865 | `		/* Point to the next entry */` |
|     ! 0 | 5866 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5867 | `	}` |
|       - | 5868 | `	/* Return product */` |
|     ! 0 | 5869 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5870 |  |
|       - | 5871 | `/* number array_product(array $array )` |
|       - | 5872 | ` * (See block-block comment above)` |
|       - | 5873 | ` */` |
|     ! 0 | 5874 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5875 |  |
|       - | 5876 | `	ph7_hashmap *pMap;` |
|       - | 5877 | `	ph7_value *pObj;` |
|     ! 0 | 5878 | `	if( nArg < 1 ){` |
|       - | 5879 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5880 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5881 | `		return PH7_OK;` |
|       - | 5882 | `	}` |
|       - | 5883 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5884 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5885 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5886 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5887 | `		return PH7_OK;` |
|       - | 5888 | `	}` |
|     ! 0 | 5889 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5890 | `	if( pMap->nEntry < 1 ){` |
|       - | 5891 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5892 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5893 | `		return PH7_OK;` |
|       - | 5894 | `	}` |
|       - | 5895 | `	/* If the first element is of type float,then perform floating` |
|       - | 5896 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5897 | `	 */` |
|     ! 0 | 5898 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5899 | `	if( pObj == 0 ){` |
|     ! 0 | 5900 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5901 | `		return PH7_OK;` |
|       - | 5902 | `	}` |
|     ! 0 | 5903 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5904 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5905 | `	}else{` |
|     ! 0 | 5906 | `		Int64Prod(pCtx,pMap);` |
|       - | 5907 | `	}` |
|     ! 0 | 5908 | `	return PH7_OK;` |
|     ! 0 | 5909 |  |
|       - | 5910 | `/*` |
|       - | 5911 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5912 | ` *  Pick one or more random entries out of an array.` |
|       - | 5913 | ` * Parameters` |
|       - | 5914 | ` * $input` |
|       - | 5915 | ` *  The input array.` |
|       - | 5916 | ` * $num_req` |
|       - | 5917 | ` *  Specifies how many entries you want to pick.` |
|       - | 5918 | ` * Return` |
|       - | 5919 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5920 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5921 | ` *  NULL is returned on failure.` |
|       - | 5922 | ` */` |
|       6 | 5923 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5924 |  |
|       - | 5925 | `	ph7_hashmap_node *pNode;` |
|       - | 5926 | `	ph7_hashmap *pMap;` |
|       7 | 5927 | `	int nItem = 1;` |
|       7 | 5928 | `	if( nArg < 1 ){` |
|       - | 5929 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5930 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5931 | `		return PH7_OK;` |
|       - | 5932 | `	}` |
|       - | 5933 | `	/* Make sure we are dealing with an array */` |
|       7 | 5934 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5935 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5936 | `		return PH7_OK;` |
|       - | 5937 | `	}` |
|       - | 5938 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5939 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5940 | `	if(pMap->nEntry < 1 ){` |
|       - | 5941 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5942 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5943 | `		return PH7_OK;` |
|       - | 5944 | `	}` |
|       7 | 5945 | `	if( nArg > 1 ){` |
|       3 | 5946 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5947 | `	}` |
|       7 | 5948 | `	if( nItem < 2 ){` |
|       - | 5949 | `		sxu32 nEntry;` |
|       - | 5950 | `		/* Select a random number */` |
|       5 | 5951 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5952 | `		/* Extract the desired entry.` |
|       - | 5953 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5954 | `		 */` |
|       5 | 5955 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 5956 | `			pNode = pMap->pLast;` |
|       3 | 5957 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 5958 | `			if( nEntry > 1 ){` |
|     ! 0 | 5959 | `				for(;;){` |
|     ! 0 | 5960 | `					if( nEntry == 0 ){` |
|     ! 0 | 5961 | `						break;` |
|       - | 5962 | `					}` |
|       - | 5963 | `					/* Point to the previous entry */` |
|     ! 0 | 5964 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5965 | `					nEntry--;` |
|     ! 0 | 5966 | `				}` |
|     ! 0 | 5967 | `			}` |
|       3 | 5968 | `		}else{` |
|       3 | 5969 | `			pNode = pMap->pFirst;` |
|     ! 0 | 5970 | `			for(;;){` |
|       4 | 5971 | `				if( nEntry == 0 ){` |
|       3 | 5972 | `					break;` |
|       - | 5973 | `				}` |
|       - | 5974 | `				/* Point to the next entry */` |
|       1 | 5975 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 5976 | `				nEntry--;` |
|     ! 0 | 5977 | `			}` |
|       - | 5978 | `		}` |
|       5 | 5979 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5980 | `			/* Int key */` |
|       3 | 5981 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5982 | `		}else{` |
|       - | 5983 | `			/* Blob key */` |
|       3 | 5984 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5985 | `		}` |
|       3 | 5986 | `	}else{` |
|       - | 5987 | `		ph7_value sKey,*pArray;` |
|       - | 5988 | `		ph7_hashmap *pDest;` |
|       - | 5989 | `		/* Create a new array */` |
|       3 | 5990 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5991 | `		if( pArray == 0 ){` |
|     ! 0 | 5992 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5993 | `			return PH7_OK;` |
|       - | 5994 | `		}` |
|       - | 5995 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5996 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5997 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5998 | `		/* Copy the first n items */` |
|       3 | 5999 | `		pNode = pMap->pFirst;` |
|       3 | 6000 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6001 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6002 | `		}` |
|       7 | 6003 | `		while( nItem > 0){` |
|       5 | 6004 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6005 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6006 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6007 | `			/* Point to the next entry */` |
|       5 | 6008 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6009 | `			nItem--;` |
|       1 | 6010 | `		}` |
|       - | 6011 | `		/* Shuffle the array */` |
|       3 | 6012 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6013 | `		/* Rehash node */` |
|       3 | 6014 | `		HashmapSortRehash(pDest);` |
|       - | 6015 | `		/* Return the random array */` |
|       3 | 6016 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6017 | `	}` |
|       7 | 6018 | `	return PH7_OK;` |
|       4 | 6019 |  |
|       - | 6020 | `/*` |
|       - | 6021 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6022 | ` *  Split an array into chunks.` |
|       - | 6023 | ` * Parameters` |
|       - | 6024 | ` * $input` |
|       - | 6025 | ` *   The array to work on` |
|       - | 6026 | ` * $size` |
|       - | 6027 | ` *   The size of each chunk` |
|       - | 6028 | ` * $preserve_keys` |
|       - | 6029 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6030 | ` *   the chunk numerically.` |
|       - | 6031 | ` * Return` |
|       - | 6032 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6033 | ` *  zero, with each dimension containing size elements.` |
|       - | 6034 | ` */` |
|      42 | 6035 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6036 |  |
|       - | 6037 | `	ph7_value *pArray,*pChunk;` |
|       - | 6038 | `	ph7_hashmap_node *pEntry;` |
|       - | 6039 | `	ph7_hashmap *pMap;` |
|       - | 6040 | `	int bPreserve;` |
|       - | 6041 | `	sxu32 nChunk;` |
|       - | 6042 | `	sxu32 nSize;` |
|       - | 6043 | `	sxu32 n;` |
|       - | 6044 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 6045 | `	if( nArg < 2 ){` |
|       - | 6046 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6047 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6048 | `			"ArgumentCountError",` |
|       - | 6049 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6050 | `			nArg` |
|       - | 6051 | `			);` |
|       - | 6052 | `	}` |
|      42 | 6053 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6054 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6055 | `			"TypeError",` |
|       - | 6056 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6057 | `			ph7_type_name(apArg[0])` |
|       - | 6058 | `			);` |
|       - | 6059 | `	}` |
|       - | 6060 | `	/* Create a new array */` |
|      40 | 6061 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 6062 | `	if( pArray == 0 ){` |
|     ! 0 | 6063 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6064 | `		return PH7_OK;` |
|       - | 6065 | `	}` |
|       - | 6066 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 6067 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6068 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6069 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 6070 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 6071 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6072 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6073 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6074 | `			"TypeError",` |
|       - | 6075 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6076 | `			ph7_type_name(apArg[1])` |
|       - | 6077 | `			);` |
|       - | 6078 | `	}` |
|       - | 6079 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6080 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6081 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 6082 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6083 | `		int len;` |
|       3 | 6084 | `		sxu8 bReal = FALSE;` |
|       3 | 6085 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6086 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6087 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6088 | `				"TypeError",` |
|       - | 6089 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6090 | `				);` |
|       - | 6091 | `		}` |
|     ! 0 | 6092 | `		if( bReal ){` |
|       - | 6093 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6094 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6095 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6096 | `				zStr` |
|       - | 6097 | `				);` |
|     ! 0 | 6098 | `		}` |
|     ! 0 | 6099 | `	}` |
|       - | 6100 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6101 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6102 | `	 * later via ph7_value_to_int. */` |
|      38 | 6103 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6104 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6105 | `		sxi64 i = (sxi64)d;` |
|       3 | 6106 | `		if( d != (double)i ){` |
|       4 | 6107 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6108 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6109 | `				d` |
|       - | 6110 | `				);` |
|       1 | 6111 | `		}` |
|       1 | 6112 | `	}` |
|       - | 6113 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6114 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6115 | `	{` |
|      38 | 6116 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 6117 | `		if( nSizeSigned < 1 ){` |
|       - | 6118 | `			/* size <= 0 -> ValueError */` |
|       5 | 6119 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6120 | `				"ValueError",` |
|       - | 6121 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6122 | `				);` |
|       - | 6123 | `		}` |
|      34 | 6124 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6125 | `	}` |
|      34 | 6126 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6127 | `		/* Return the whole array */` |
|       3 | 6128 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6129 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6130 | `		return PH7_OK;` |
|       - | 6131 | `	}` |
|      32 | 6132 | `	bPreserve = 0;` |
|      32 | 6133 | `	if( nArg > 2 ){` |
|       - | 6134 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6135 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6136 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6137 | `		 * normally, matching PHP behaviour. */` |
|      45 | 6138 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 6139 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6140 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 6141 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6142 | `				"TypeError",` |
|       - | 6143 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6144 | `				ph7_type_name(apArg[2])` |
|       - | 6145 | `				);` |
|       - | 6146 | `		}` |
|      21 | 6147 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6148 | `	}` |
|       - | 6149 | `	/* Start processing */` |
|      27 | 6150 | `	pEntry = pMap->pFirst;` |
|      27 | 6151 | `	nChunk = 0;` |
|      27 | 6152 | `	pChunk = 0;` |
|      27 | 6153 | `	n = pMap->nEntry;` |
|      56 | 6154 | `	for( ;; ){` |
|     113 | 6155 | `		if( n < 1 ){` |
|       - | 6156 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6157 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6158 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6159 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6160 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6161 | `			 * exists. */` |
|      27 | 6162 | `			if( pChunk ){` |
|      27 | 6163 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6164 | `			}` |
|      27 | 6165 | `			break;` |
|       - | 6166 | `		}` |
|      87 | 6167 | `		if( nChunk < 1 ){` |
|      71 | 6168 | `			if( pChunk ){` |
|       - | 6169 | `				/* Put the first chunk */` |
|      45 | 6170 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6171 | `			}` |
|       - | 6172 | `			/* Create a new dimension */` |
|      71 | 6173 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6174 | `												   * will be automatically released as soon we return` |
|       - | 6175 | `												   * from this function */` |
|      71 | 6176 | `			if( pChunk == 0 ){` |
|     ! 0 | 6177 | `				break;` |
|       - | 6178 | `			}` |
|      71 | 6179 | `			nChunk = nSize;` |
|      35 | 6180 | `		}` |
|       - | 6181 | `		/* Insert the entry */` |
|      87 | 6182 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6183 | `		/* Point to the next entry */` |
|      87 | 6184 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6185 | `		nChunk--;` |
|      87 | 6186 | `		n--;` |
|       1 | 6187 | `	}` |
|       - | 6188 | `	/* Return the multidimensional array */` |
|      27 | 6189 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6190 | `	return PH7_OK;` |
|      23 | 6191 |  |
|       - | 6192 | `/*` |
|       - | 6193 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6194 | ` *  Pad array to the specified length with a value.` |
|       - | 6195 | ` * $input` |
|       - | 6196 | ` *   Initial array of values to pad.` |
|       - | 6197 | ` * $pad_size` |
|       - | 6198 | ` *   New size of the array.` |
|       - | 6199 | ` * $pad_value` |
|       - | 6200 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6201 | ` */` |
|      28 | 6202 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6203 |  |
|       - | 6204 | `	ph7_hashmap *pMap;` |
|       - | 6205 | `	ph7_value *pArray;` |
|       - | 6206 | `	int nEntry;` |
|      30 | 6207 | `	if( nArg != 3 ){` |
|      10 | 6208 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6209 | `			"ArgumentCountError",` |
|       - | 6210 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6211 | `			nArg` |
|       - | 6212 | `			);` |
|       - | 6213 | `	}` |
|      24 | 6214 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6215 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6216 | `			"TypeError",` |
|       - | 6217 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6218 | `			ph7_type_name(apArg[0])` |
|       - | 6219 | `			);` |
|       - | 6220 | `	}` |
|       - | 6221 | `	/* Create a new array */` |
|      21 | 6222 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6223 | `	if( pArray == 0 ){` |
|     ! 0 | 6224 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6225 | `		return PH7_OK;` |
|       - | 6226 | `	}` |
|       - | 6227 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6228 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6229 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6230 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6231 | `	if( nEntry < 0 ){` |
|       9 | 6232 | `		nEntry = -nEntry;` |
|       9 | 6233 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6234 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6235 | `			/* Insert given items first */` |
|      17 | 6236 | `			while( nEntry > 0 ){` |
|      13 | 6237 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 6238 | `				nEntry--;` |
|       1 | 6239 | `			}` |
|       - | 6240 | `			/* Merge the two arrays */` |
|       5 | 6241 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6242 | `		}else{` |
|       5 | 6243 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6244 | `		}` |
|      17 | 6245 | `	}else if( nEntry > 0 ){` |
|      11 | 6246 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6247 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6248 | `			/* Merge the two arrays first */` |
|       7 | 6249 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6250 | `			/* Insert given items */` |
|      25 | 6251 | `			while( nEntry > 0 ){` |
|      19 | 6252 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 6253 | `				nEntry--;` |
|       1 | 6254 | `			}` |
|       4 | 6255 | `		}else{` |
|       5 | 6256 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6257 | `		}` |
|       6 | 6258 | `	}else{` |
|       - | 6259 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6260 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6261 | `	}` |
|       - | 6262 | `	/* Return the new array */` |
|      21 | 6263 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6264 | `	return PH7_OK;` |
|      16 | 6265 |  |
|       - | 6266 | `/*` |
|       - | 6267 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6268 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6269 | ` * Parameters` |
|       - | 6270 | ` * $array` |
|       - | 6271 | ` *   The array in which elements are replaced.` |
|       - | 6272 | ` * $array1` |
|       - | 6273 | ` *   The array from which elements will be extracted.` |
|       - | 6274 | ` * ....` |
|       - | 6275 | ` *  More arrays from which elements will be extracted.` |
|       - | 6276 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6277 | ` * Return` |
|       - | 6278 | ` *  Returns an array.` |
|       - | 6279 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6280 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6281 | ` */` |
|      22 | 6282 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6283 |  |
|       - | 6284 | `	ph7_hashmap *pMap;` |
|       - | 6285 | `	ph7_value *pArray;` |
|       - | 6286 | `	int i;` |
|      24 | 6287 | `	if( nArg < 1 ){` |
|       3 | 6288 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6289 | `			"ArgumentCountError",` |
|       - | 6290 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6291 | `			);` |
|       - | 6292 | `	}` |
|      22 | 6293 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6294 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6295 | `			"TypeError",` |
|       - | 6296 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6297 | `			ph7_type_name(apArg[0])` |
|       - | 6298 | `			);` |
|       - | 6299 | `	}` |
|       - | 6300 | `	/* Create a new array */` |
|      20 | 6301 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6302 | `	if( pArray == 0 ){` |
|     ! 0 | 6303 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6304 | `		return PH7_OK;` |
|       - | 6305 | `	}` |
|       - | 6306 | `	/* Overwrite from the first array */` |
|      20 | 6307 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6308 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6309 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6310 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6311 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6312 | `			/* Type mismatch -> TypeError */` |
|       4 | 6313 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6314 | `				"TypeError",` |
|       - | 6315 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6316 | `				i + 1,` |
|       2 | 6317 | `				ph7_type_name(apArg[i])` |
|       - | 6318 | `				);` |
|       - | 6319 | `		}` |
|       - | 6320 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6321 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6322 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6323 | `	}` |
|       - | 6324 | `	/* Return the new array */` |
|      17 | 6325 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6326 | `	return PH7_OK;` |
|      13 | 6327 |  |
|       - | 6328 | `/*` |
|       - | 6329 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6330 | ` *  Filters elements of an array using a callback function.` |
|       - | 6331 | ` * Parameters` |
|       - | 6332 | ` *  $input` |
|       - | 6333 | ` *    The array to iterate over` |
|       - | 6334 | ` * $callback` |
|       - | 6335 | ` *    The callback function to use` |
|       - | 6336 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6337 | ` *    will be removed.` |
|       - | 6338 | ` * Return` |
|       - | 6339 | ` *  The filtered array.` |
|       - | 6340 | ` */` |
|      18 | 6341 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6342 |  |
|       - | 6343 | `	ph7_hashmap_node *pEntry;` |
|       - | 6344 | `	ph7_hashmap *pMap;` |
|       - | 6345 | `	ph7_value *pArray;` |
|       - | 6346 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6347 | `	ph7_value *pValue;` |
|       - | 6348 | `	sxi32 rc;` |
|       - | 6349 | `	int keep;` |
|       - | 6350 | `	sxu32 n;` |
|      20 | 6351 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6352 | `		/* Invalid arguments,return NULL */` |
|       5 | 6353 | `		ph7_result_null(pCtx);` |
|       5 | 6354 | `		return PH7_OK;` |
|       - | 6355 | `	}` |
|       - | 6356 | `	/* Create a new array */` |
|      16 | 6357 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 6358 | `	if( pArray == 0 ){` |
|     ! 0 | 6359 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6360 | `		return PH7_OK;` |
|       - | 6361 | `	}` |
|       - | 6362 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 6363 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 6364 | `	pEntry = pMap->pFirst;` |
|      16 | 6365 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 6366 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6367 | `	/* Perform the requested operation */` |
|      66 | 6368 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6369 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 6370 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 6371 | `		if( pValue == 0 ){` |
|       - | 6372 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6373 | `			keep = FALSE;` |
|      54 | 6374 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6375 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6376 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6377 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 6378 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6379 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6380 | `					int len;` |
|       3 | 6381 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6382 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6383 | `						"TypeError",` |
|       - | 6384 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6385 | `						zName` |
|       - | 6386 | `						);` |
|     ! 0 | 6387 | `				}else{` |
|     ! 0 | 6388 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6389 | `						"TypeError",` |
|       - | 6390 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6391 | `						ph7_type_name(apArg[1])` |
|       - | 6392 | `						);` |
|       - | 6393 | `				}` |
|       - | 6394 | `			}` |
|      23 | 6395 | `			keep = FALSE;` |
|      23 | 6396 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 6397 | `			if( rc == SXRET_OK ){` |
|       - | 6398 | `				/* Perform a boolean cast */` |
|      23 | 6399 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6400 | `			}` |
|      23 | 6401 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6402 | `		}else{` |
|       - | 6403 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6404 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6405 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6406 | `			 */` |
|      29 | 6407 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6408 | `		}` |
|      51 | 6409 | `		if( keep ){` |
|       - | 6410 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6411 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6412 | `		}` |
|       - | 6413 | `		/* Point to the next entry */` |
|      51 | 6414 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6415 | `	}` |
|      13 | 6416 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6417 | `	return PH7_OK;` |
|      11 | 6418 |  |
|       - | 6419 | `/*` |
|       - | 6420 | ` * array array_map(?callable $callback, array $array)` |
|       - | 6421 | ` *  Applies the callback to the elements of the given array.` |
|       - | 6422 | ` * Parameters` |
|       - | 6423 | ` *  $callback` |
|       - | 6424 | ` *   A callable to run for each element in the array, or NULL for the` |
|       - | 6425 | ` *   identity function (returns the array unchanged).` |
|       - | 6426 | ` *  $array` |
|       - | 6427 | ` *   An array to run through the callback function.` |
|       - | 6428 | ` * Return` |
|       - | 6429 | ` *  Returns an array containing the results of applying the callback` |
|       - | 6430 | ` *  function to each element of $array.` |
|       - | 6431 | ` */` |
|      34 | 6432 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6433 |  |
|       - | 6434 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6435 | `	ph7_hashmap_node *pEntry;` |
|       - | 6436 | `	ph7_hashmap *pMap;` |
|       - | 6437 | `	int bNullCallback;` |
|       - | 6438 | `	sxu32 n;` |
|      36 | 6439 | `	if( nArg < 2 ){` |
|       7 | 6440 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6441 | `			"ArgumentCountError",` |
|       - | 6442 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6443 | `			nArg` |
|       - | 6444 | `			);` |
|       - | 6445 | `	}` |
|      32 | 6446 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 6447 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6448 | `			"TypeError",` |
|       - | 6449 | `			"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6450 | `			ph7_type_name(apArg[1])` |
|       - | 6451 | `			);` |
|       - | 6452 | `	}` |
|      30 | 6453 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      30 | 6454 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6455 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6456 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6457 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6458 | `				"TypeError",` |
|       - | 6459 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6460 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6461 | `				zFunc` |
|       - | 6462 | `				);` |
|       - | 6463 | `		}` |
|       3 | 6464 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6465 | `			"TypeError",` |
|       - | 6466 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6467 | `			"no array or string given"` |
|       - | 6468 | `			);` |
|       - | 6469 | `	}` |
|       - | 6470 | `	/* Create a new array */` |
|      26 | 6471 | `	pArray = ph7_context_new_array(pCtx);` |
|      26 | 6472 | `	if( pArray == 0 ){` |
|     ! 0 | 6473 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6474 | `		return PH7_OK;` |
|       - | 6475 | `	}` |
|       - | 6476 | `	/* Point to the internal representation of the input hashmap */` |
|      26 | 6477 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      26 | 6478 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      26 | 6479 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      26 | 6480 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      26 | 6481 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 6482 | `	/* Perform the requested operation */` |
|      26 | 6483 | `	pEntry = pMap->pFirst;` |
|      80 | 6484 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6485 | `		/* Extract the node value */` |
|      56 | 6486 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      56 | 6487 | `		if( pValue ){` |
|       - | 6488 | `			/* Extract the node key */` |
|      56 | 6489 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      56 | 6490 | `			if( bNullCallback ){` |
|       - | 6491 | `				/* NULL callback: identity function, keep original value */` |
|      11 | 6492 | `				ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6493 | `			}else{` |
|       - | 6494 | `				/* Invoke the supplied callback */` |
|      46 | 6495 | `				PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 6496 | `				/* Insert the callback return value */` |
|      46 | 6497 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6498 | `			}` |
|      56 | 6499 | `			PH7_MemObjRelease(&sKey);` |
|      56 | 6500 | `			PH7_MemObjRelease(&sResult);` |
|      27 | 6501 | `		}` |
|       - | 6502 | `		/* Point to the next entry */` |
|      56 | 6503 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 6504 | `	}` |
|      26 | 6505 | `	ph7_result_value(pCtx,pArray);` |
|      26 | 6506 | `	return PH7_OK;` |
|      19 | 6507 |  |
|       - | 6508 | `/*` |
|       - | 6509 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6510 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6511 | ` * Parameters` |
|       - | 6512 | ` *  $array` |
|       - | 6513 | ` *   The input array.` |
|       - | 6514 | ` *  $callback` |
|       - | 6515 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6516 | ` *  $initial` |
|       - | 6517 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6518 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6519 | ` * Return` |
|       - | 6520 | ` *  Returns the resulting value.` |
|       - | 6521 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6522 | ` */` |
|      30 | 6523 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6524 |  |
|       - | 6525 | `	ph7_hashmap_node *pEntry;` |
|       - | 6526 | `	ph7_hashmap *pMap;` |
|       - | 6527 | `	ph7_value *pValue;` |
|       - | 6528 | `	ph7_value sResult;` |
|       - | 6529 | `	sxu32 n;` |
|      32 | 6530 | `	if( nArg < 2 ){` |
|       7 | 6531 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6532 | `			"ArgumentCountError",` |
|       - | 6533 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6534 | `			nArg` |
|       - | 6535 | `			);` |
|       - | 6536 | `	}` |
|      28 | 6537 | `	if( nArg > 3 ){` |
|       4 | 6538 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6539 | `			"ArgumentCountError",` |
|       - | 6540 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6541 | `			nArg` |
|       - | 6542 | `			);` |
|       - | 6543 | `	}` |
|      26 | 6544 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6545 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6546 | `			"TypeError",` |
|       - | 6547 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6548 | `			ph7_type_name(apArg[0])` |
|       - | 6549 | `			);` |
|       - | 6550 | `	}` |
|      24 | 6551 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6552 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6553 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6554 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6555 | `				"TypeError",` |
|       - | 6556 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6557 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6558 | `				zFunc` |
|       - | 6559 | `				);` |
|       - | 6560 | `		}` |
|       7 | 6561 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6562 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6563 | `				"TypeError",` |
|       - | 6564 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6565 | `				"array callback must have exactly two members"` |
|       - | 6566 | `				);` |
|       - | 6567 | `		}` |
|       5 | 6568 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6569 | `			"TypeError",` |
|       - | 6570 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6571 | `			"no array or string given"` |
|       - | 6572 | `			);` |
|       - | 6573 | `	}` |
|       - | 6574 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 6575 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6576 | `	/* Assume a NULL initial value */` |
|      15 | 6577 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      15 | 6578 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      15 | 6579 | `	if( nArg > 2 ){` |
|       - | 6580 | `		/* Set the initial value */` |
|      11 | 6581 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6582 | `	}` |
|       - | 6583 | `	/* Perform the requested operation */` |
|      15 | 6584 | `	pEntry = pMap->pFirst;` |
|      43 | 6585 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6586 | `		/* Extract the node value */` |
|      29 | 6587 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6588 | `		/* Invoke the supplied callback */` |
|      29 | 6589 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6590 | `		/* Point to the next entry */` |
|      29 | 6591 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6592 | `	}` |
|      15 | 6593 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6594 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6595 | `	return PH7_OK;` |
|      17 | 6596 |  |
|       - | 6597 | `/*` |
|       - | 6598 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6599 | ` *  Apply a user function to every member of an array.` |
|       - | 6600 | ` * Parameters` |
|       - | 6601 | ` *  $array` |
|       - | 6602 | ` *   The input array.` |
|       - | 6603 | ` *  $funcname` |
|       - | 6604 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6605 | ` *   the first, and the key/index second.` |
|       - | 6606 | ` * Note:` |
|       - | 6607 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6608 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6609 | ` *  be made in the original array itself.` |
|       - | 6610 | ` *  $userdata` |
|       - | 6611 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6612 | ` *   to the callback funcname.` |
|       - | 6613 | ` * Return` |
|       - | 6614 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6615 | ` */` |
|      36 | 6616 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6617 |  |
|       - | 6618 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6619 | `	ph7_hashmap_node *pEntry;` |
|       - | 6620 | `	ph7_hashmap *pMap;` |
|       - | 6621 | `	sxu32 n;` |
|      38 | 6622 | `	if( nArg < 2 ){` |
|       7 | 6623 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6624 | `			"ArgumentCountError",` |
|       - | 6625 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6626 | `			nArg` |
|       - | 6627 | `			);` |
|       - | 6628 | `	}` |
|      34 | 6629 | `	if( nArg > 3 ){` |
|       4 | 6630 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6631 | `			"ArgumentCountError",` |
|       - | 6632 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6633 | `			nArg` |
|       - | 6634 | `			);` |
|       - | 6635 | `	}` |
|      32 | 6636 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6637 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6638 | `			"TypeError",` |
|       - | 6639 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6640 | `			ph7_type_name(apArg[0])` |
|       - | 6641 | `			);` |
|       - | 6642 | `	}` |
|      30 | 6643 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6644 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6645 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6646 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6647 | `				"TypeError",` |
|       - | 6648 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6649 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6650 | `				zFunc` |
|       - | 6651 | `				);` |
|       - | 6652 | `		}` |
|       9 | 6653 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6654 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6655 | `				"TypeError",` |
|       - | 6656 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6657 | `				"array callback must have exactly two members"` |
|       - | 6658 | `				);` |
|       - | 6659 | `		}` |
|       5 | 6660 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6661 | `			"TypeError",` |
|       - | 6662 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6663 | `			"no array or string given"` |
|       - | 6664 | `			);` |
|       - | 6665 | `	}` |
|      19 | 6666 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6667 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6668 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      19 | 6669 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      19 | 6670 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6671 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6672 | `	/* Perform the desired operation */` |
|      19 | 6673 | `	pEntry = pMap->pFirst;` |
|      59 | 6674 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6675 | `		/* Extract the node value */` |
|      41 | 6676 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      41 | 6677 | `		if( pValue ){` |
|       - | 6678 | `			/* Extract the entry key */` |
|      41 | 6679 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6680 | `			/* Invoke the supplied callback */` |
|      41 | 6681 | `			PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      41 | 6682 | `			PH7_MemObjRelease(&sKey);` |
|      20 | 6683 | `		}` |
|       - | 6684 | `		/* Point to the next entry */` |
|      41 | 6685 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6686 | `	}` |
|       - | 6687 | `	/* All done, return TRUE */` |
|      19 | 6688 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6689 | `	return PH7_OK;` |
|      20 | 6690 |  |
|       - | 6691 | `/*` |
|       - | 6692 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6693 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6694 | ` */` |
|      22 | 6695 | `static void HashmapWalkRecursive(` |
|       - | 6696 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6697 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6698 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6699 | `	int iNest             /* Nesting level */` |
|       - | 6700 | `	)` |
|       1 | 6701 |  |
|       - | 6702 | `	ph7_hashmap_node *pEntry;` |
|       - | 6703 | `	ph7_value *pValue,sKey;` |
|       - | 6704 | `	sxu32 n;` |
|       - | 6705 | `	/* Iterate through hashmap entries */` |
|      23 | 6706 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6707 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6708 | `	pEntry = pMap->pFirst;` |
|      59 | 6709 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6710 | `		/* Extract the node value */` |
|      37 | 6711 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6712 | `		if( pValue ){` |
|      37 | 6713 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6714 | `				if( iNest < 32 ){` |
|       - | 6715 | `					/* Recurse */` |
|      11 | 6716 | `					iNest++;` |
|      11 | 6717 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6718 | `					iNest--;` |
|       5 | 6719 | `				}` |
|       6 | 6720 | `			}else{` |
|       - | 6721 | `				/* Extract the node key */` |
|      27 | 6722 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6723 | `				/* Invoke the supplied callback */` |
|      27 | 6724 | `				PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6725 | `				PH7_MemObjRelease(&sKey);` |
|       - | 6726 | `			}` |
|      18 | 6727 | `		}` |
|       - | 6728 | `		/* Point to the next entry */` |
|      37 | 6729 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6730 | `	}` |
|      23 | 6731 |  |
|       - | 6732 | `/*` |
|       - | 6733 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6734 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6735 | ` * Parameters` |
|       - | 6736 | ` *  $array` |
|       - | 6737 | ` *   The input array.` |
|       - | 6738 | ` *  $funcname` |
|       - | 6739 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6740 | ` *   the first, and the key/index second.` |
|       - | 6741 | ` * Note:` |
|       - | 6742 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6743 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6744 | ` *  be made in the original array itself.` |
|       - | 6745 | ` *  $userdata` |
|       - | 6746 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6747 | ` *   to the callback funcname.` |
|       - | 6748 | ` * Return` |
|       - | 6749 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6750 | ` */` |
|      30 | 6751 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6752 |  |
|       - | 6753 | `	ph7_hashmap *pMap;` |
|      32 | 6754 | `	if( nArg < 2 ){` |
|       7 | 6755 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6756 | `			"ArgumentCountError",` |
|       - | 6757 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 6758 | `			nArg` |
|       - | 6759 | `			);` |
|       - | 6760 | `	}` |
|      28 | 6761 | `	if( nArg > 3 ){` |
|       4 | 6762 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6763 | `			"ArgumentCountError",` |
|       - | 6764 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 6765 | `			nArg` |
|       - | 6766 | `			);` |
|       - | 6767 | `	}` |
|      26 | 6768 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6769 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6770 | `			"TypeError",` |
|       - | 6771 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6772 | `			ph7_type_name(apArg[0])` |
|       - | 6773 | `			);` |
|       - | 6774 | `	}` |
|      24 | 6775 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6776 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6777 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6778 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6779 | `				"TypeError",` |
|       - | 6780 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6781 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6782 | `				zFunc` |
|       - | 6783 | `				);` |
|       - | 6784 | `		}` |
|       9 | 6785 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6786 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6787 | `				"TypeError",` |
|       - | 6788 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6789 | `				"array callback must have exactly two members"` |
|       - | 6790 | `				);` |
|       - | 6791 | `		}` |
|       5 | 6792 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6793 | `			"TypeError",` |
|       - | 6794 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6795 | `			"no array or string given"` |
|       - | 6796 | `			);` |
|       - | 6797 | `	}` |
|       - | 6798 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6799 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 6800 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6801 | `	/* Perform the desired operation */` |
|      13 | 6802 | `	HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6803 | `	/* All done, return TRUE */` |
|      13 | 6804 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6805 | `	return PH7_OK;` |
|      17 | 6806 |  |
|       - | 6807 | `/*` |
|       - | 6808 | ` * Table of hashmap functions.` |
|       - | 6809 | ` */` |
|       - | 6810 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6811 | `	{"count",             ph7_hashmap_count },` |
|       - | 6812 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6813 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6814 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6815 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6816 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6817 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6818 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6819 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6820 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6821 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6822 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6823 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6824 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6825 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6826 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6827 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6828 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6829 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6830 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6831 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6832 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6833 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6834 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6835 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6836 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6837 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6838 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6839 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6840 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6841 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6842 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6843 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6844 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6845 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6846 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6847 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6848 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6849 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6850 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6851 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6852 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6853 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6854 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6855 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6856 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6857 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6858 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6859 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6860 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6861 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6862 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6863 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6864 | `	{"current",           ph7_hashmap_current },` |
|       - | 6865 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6866 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6867 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6868 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6869 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6870 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6871 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6872 | `};` |
|       - | 6873 | `/*` |
|       - | 6874 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6875 | ` */` |
|    2692 | 6876 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6877 |  |
|       - | 6878 | `	sxu32 n;` |
|  166906 | 6879 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  164214 | 6880 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   82108 | 6881 | `	}` |
|    2694 | 6882 |  |
|       - | 6883 | `/*` |
|       - | 6884 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6885 | ` * the BLOB given as the first argument.` |
|       - | 6886 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6887 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6888 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6889 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6890 | ` */` |
|      26 | 6891 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6892 |  |
|       - | 6893 | `	ph7_hashmap_node *pEntry;` |
|       - | 6894 | `	ph7_value *pObj;` |
|      28 | 6895 | `	sxu32 n = 0;` |
|       - | 6896 | `	int isRef;` |
|       - | 6897 | `	sxi32 rc;` |
|       - | 6898 | `	int i;` |
|      28 | 6899 | `	if( nDepth > 31 ){` |
|       - | 6900 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6901 | `		/* Nesting limit reached */` |
|     ! 0 | 6902 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6903 | `		if( ShowType ){` |
|     ! 0 | 6904 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6905 | `		}` |
|     ! 0 | 6906 | `		return SXERR_LIMIT;` |
|       - | 6907 | `	}` |
|       - | 6908 | `	/* Point to the first inserted entry */` |
|      28 | 6909 | `	pEntry = pMap->pFirst;` |
|      28 | 6910 | `	rc = SXRET_OK;` |
|      28 | 6911 | `	if( !ShowType ){` |
|      15 | 6912 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6913 | `	}` |
|       - | 6914 | `	/* Total entries */` |
|      28 | 6915 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6916 | `#ifdef __WINNT__` |
|       2 | 6917 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6918 | `#else` |
|      26 | 6919 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6920 | `#endif` |
|      62 | 6921 | `	for(;;){` |
|     126 | 6922 | `		if( n >= pMap->nEntry ){` |
|      28 | 6923 | `			break;` |
|       - | 6924 | `		}` |
|     198 | 6925 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 6926 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 6927 | `		}` |
|       - | 6928 | `		/* Dump key */` |
|     100 | 6929 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 6930 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 6931 | `		}else{` |
|     101 | 6932 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6933 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6934 | `		}` |
|       - | 6935 | `#ifdef __WINNT__` |
|       2 | 6936 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6937 | `#else` |
|      98 | 6938 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6939 | `#endif` |
|       - | 6940 | `		/* Dump node value */` |
|     100 | 6941 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 6942 | `		isRef = 0;` |
|     100 | 6943 | `		if( pObj ){` |
|     100 | 6944 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6945 | `				/* Referenced object */` |
|     ! 0 | 6946 | `				isRef = 1;` |
|     ! 0 | 6947 | `			}` |
|     100 | 6948 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 6949 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6950 | `				break;` |
|       - | 6951 | `			}` |
|      49 | 6952 | `		}` |
|       - | 6953 | `		/* Point to the next entry */` |
|     100 | 6954 | `		n++;` |
|     100 | 6955 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6956 | `	}` |
|      54 | 6957 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 6958 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 6959 | `	}` |
|      28 | 6960 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 6961 | `	return rc;` |
|      15 | 6962 |  |
|       - | 6963 | `/*` |
|       - | 6964 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6965 | ` * retrieved entry.` |
|       - | 6966 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6967 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6968 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6969 | ` * a value different from PH7_OK.` |
|       - | 6970 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6971 | ` */` |
|   28792 | 6972 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6973 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6974 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6975 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6976 | `	)` |
|       2 | 6977 |  |
|       - | 6978 | `	ph7_hashmap_node *pEntry;` |
|       - | 6979 | `	ph7_value sKey,sValue;` |
|       - | 6980 | `	sxi32 rc;` |
|       - | 6981 | `	sxu32 n;` |
|       - | 6982 | `	/* Initialize walker parameter */` |
|   28794 | 6983 | `	rc = SXRET_OK;` |
|   28794 | 6984 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   28794 | 6985 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   28794 | 6986 | `	n = pMap->nEntry;` |
|   28794 | 6987 | `	pEntry = pMap->pFirst;` |
|       - | 6988 | `	/* Start the iteration process */` |
|   71876 | 6989 | `	for(;;){` |
|  143754 | 6990 | `		if( n < 1 ){` |
|   28794 | 6991 | `			break;` |
|       - | 6992 | `		}` |
|       - | 6993 | `		/* Extract a copy of the key and a copy the current value */` |
|  114962 | 6994 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  114962 | 6995 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6996 | `		/* Invoke the user callback */` |
|  114962 | 6997 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6998 | `		/* Release the copy of the key and the value */` |
|  114962 | 6999 | `		PH7_MemObjRelease(&sKey);` |
|  114962 | 7000 | `		PH7_MemObjRelease(&sValue);` |
|  114962 | 7001 | `		if( rc != PH7_OK ){` |
|       - | 7002 | `			/* Callback request an operation abort */` |
|     ! 0 | 7003 | `			return SXERR_ABORT;` |
|       - | 7004 | `		}` |
|       - | 7005 | `		/* Point to the next entry */` |
|  114962 | 7006 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  114962 | 7007 | `		n--;` |
|       2 | 7008 | `	}` |
|       - | 7009 | `	/* All done */` |
|   28794 | 7010 | `	return SXRET_OK;` |
|   14398 | 7011 |  |
|       - | 7012 |  |
