# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2966/3393 lines (87.42%)

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
| 2974822 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2974824 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  299892 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  299894 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  299894 |   29 | `	sxu32 nH = 5381;` |
|  299894 |   30 | `	zEnd = &zIn[nLen];` |
|  334813 |   31 | `	for(;;){` |
|  669628 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  588072 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  528210 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  436024 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  299894 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     868 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       2 |   47 |  |
|     870 |   48 | `	sxi64 iCount = 0;` |
|     870 |   49 | `	if( !bRecursive ){` |
|     696 |   50 | `		iCount = pMap->nEntry;` |
|     349 |   51 | `	}else{` |
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
|     870 |   86 | `	return iCount;` |
|       2 |   87 |  |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 2916596 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 2916598 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2916598 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 2916598 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 2916598 |  104 | `	pNode->pMap  = &(*pMap);` |
| 2916598 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2916598 |  106 | `	pNode->nHash = nHash;` |
| 2916598 |  107 | `	pNode->xKey.iKey = iKey;` |
| 2916598 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 2916598 |  109 | `	return pNode;` |
| 1458300 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|  103664 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|  103666 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  103666 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|  103666 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|  103666 |  127 | `	pNode->pMap  = &(*pMap);` |
|  103666 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  103666 |  129 | `	pNode->nHash = nHash;` |
|  103666 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  103666 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  103666 |  132 | `	pNode->nValIdx = nValIdx;` |
|  103666 |  133 | `	return pNode;` |
|   51834 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 3020260 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  139 |  |
|       - |  140 | `	/* Link */` |
| 3020262 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2733748 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2733748 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1366873 |  144 | `	}` |
| 3020262 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 3020262 |  147 | `	if( pMap->pFirst == 0 ){` |
|   50824 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   50824 |  150 | `		pMap->pCur = pNode;` |
|   25413 |  151 | `	}else{` |
| 2969440 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 3020262 |  154 | `	++pMap->nEntry;` |
| 3020262 |  155 |  |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    7520 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  161 |  |
|    7522 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    7522 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    7522 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    7078 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3540 |  167 | `	}else{` |
|     445 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    7522 |  170 | `	if( pNode->pNextCollide ){` |
|    5608 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2803 |  172 | `	}` |
|    7522 |  173 | `	if( pMap->pFirst == pNode ){` |
|      82 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      40 |  175 | `	}` |
|    7522 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|      84 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      41 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    7522 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    7522 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     104 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     104 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     104 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  188 | `		}` |
|      51 |  189 | `	}` |
|    7522 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    7396 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3697 |  192 | `	}` |
|    7522 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    7522 |  194 | `	pMap->nEntry--;` |
|    7522 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      32 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      32 |  198 | `		pMap->apBucket = 0;` |
|      32 |  199 | `		pMap->nSize = 0;` |
|      32 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      15 |  201 | `	}` |
|    7522 |  202 |  |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 3020260 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  208 |  |
| 3020262 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   54998 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   54998 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   54998 |  215 | `		if( nNew < 1 ){` |
|   50824 |  216 | `			nNew = 16;` |
|   25411 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   54998 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   54998 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   54998 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   54998 |  230 | `		pMap->apBucket = apNew;` |
|   54998 |  231 | `		pMap->nSize = nNew;` |
|   54998 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   50824 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    4176 |  237 | `		pEntry = pMap->pFirst;` |
|    4176 |  238 | `		n = 0;` |
| 2015543 |  239 | `		for( ;; ){` |
| 4031088 |  240 | `			if( n >= pMap->nEntry ){` |
|    4176 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 4026914 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 4026914 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4026914 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3497228 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3497228 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1748613 |  250 | `			}` |
| 4026914 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 4026914 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4026914 |  254 | `			n++;` |
|       2 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    4176 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2087 |  258 | `	}` |
| 2969440 |  259 | `	return SXRET_OK;` |
| 1510132 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 2916596 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 2916598 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		/* Reserve a ph7_value for the value */` |
| 2916572 |  274 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2916572 |  275 | `		if( pObj == 0 ){` |
|     ! 0 |  276 | `			return SXERR_MEM;` |
|       - |  277 | `		}` |
| 2916572 |  278 | `		if( pValue ){` |
|       - |  279 | `			/* Duplicate the value */` |
| 2916572 |  280 | `			PH7_MemObjStore(pValue,pObj);` |
| 1458285 |  281 | `		}` |
| 2916572 |  282 | `		nIdx = pObj->nIdx;` |
| 1458287 |  283 | `	}else{` |
|      27 |  284 | `		nIdx = nRefIdx;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Hash the key */` |
| 2916598 |  287 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  288 | `	/* Allocate a new int node */` |
| 2916598 |  289 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2916598 |  290 | `	if( pNode == 0 ){` |
|     ! 0 |  291 | `		return SXERR_MEM;` |
|       - |  292 | `	}` |
| 2916598 |  293 | `	if( isForeign ){` |
|       - |  294 | `		/* Mark as a foregin entry */` |
|      27 |  295 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      13 |  296 | `	}` |
|       - |  297 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2916598 |  298 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2916598 |  299 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  300 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  301 | `		return rc;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Perform the insertion */` |
| 2916598 |  304 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  305 | `	/* Install in the reference table */` |
| 2916598 |  306 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  307 | `	/* All done */` |
| 2916598 |  308 | `	return SXRET_OK;` |
| 1458300 |  309 |  |
|       - |  310 | `/*` |
|       - |  311 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  312 | ` * hashmap.` |
|       - |  313 | ` */` |
|  103664 |  314 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  315 |  |
|       - |  316 | `	ph7_hashmap_node *pNode;` |
|       - |  317 | `	sxu32 nHash;` |
|       - |  318 | `	sxu32 nIdx;` |
|       - |  319 | `	sxi32 rc;` |
|  103666 |  320 | `	if( !isForeign ){` |
|       - |  321 | `		ph7_value *pObj;` |
|       - |  322 | `		/* Reserve a ph7_value for the value */` |
|   69886 |  323 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   69886 |  324 | `		if( pObj == 0 ){` |
|     ! 0 |  325 | `			return SXERR_MEM;` |
|       - |  326 | `		}` |
|   69886 |  327 | `		if( pValue ){` |
|       - |  328 | `			/* Duplicate the value */` |
|   69614 |  329 | `			PH7_MemObjStore(pValue,pObj);` |
|   34806 |  330 | `		}` |
|   69886 |  331 | `		nIdx = pObj->nIdx;` |
|   34944 |  332 | `	}else{` |
|   33782 |  333 | `		nIdx = nRefIdx;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Hash the key */` |
|  103666 |  336 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  337 | `	/* Allocate a new blob node */` |
|  103666 |  338 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  103666 |  339 | `	if( pNode == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|  103666 |  342 | `	if( isForeign ){` |
|       - |  343 | `		/* Mark as a foregin entry */` |
|   33782 |  344 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   16890 |  345 | `	}` |
|       - |  346 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  103666 |  347 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  103666 |  348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  349 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  350 | `		return rc;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Perform the insertion */` |
|  103666 |  353 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  354 | `	/* Install in the reference table */` |
|  103666 |  355 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  356 | `	/* All done */` |
|  103666 |  357 | `	return SXRET_OK;` |
|   51834 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  361 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  362 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  363 | ` */` |
|   47646 |  364 | `static sxi32 HashmapLookupIntKey(` |
|       - |  365 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  366 | `	sxi64 iKey,                /* lookup key */` |
|       - |  367 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  368 | `	)` |
|       2 |  369 |  |
|       - |  370 | `	ph7_hashmap_node *pNode;` |
|       - |  371 | `	sxu32 nHash;` |
|   47648 |  372 | `	if( pMap->nEntry < 1 ){` |
|       - |  373 | `		/* Don't bother hashing,there is no entry anyway */` |
|     432 |  374 | `		return SXERR_NOTFOUND;` |
|       - |  375 | `	}` |
|       - |  376 | `	/* Hash the key first */` |
|   47218 |  377 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  378 | `	/* Point to the appropriate bucket */` |
|   47218 |  379 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  380 | `	/* Perform the lookup */` |
|  411963 |  381 | `	for(;;){` |
|  823928 |  382 | `		if( pNode == 0 ){` |
|   45952 |  383 | `			break;` |
|       - |  384 | `		}` |
|  778609 |  385 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774961 |  386 | `			&& pNode->nHash == nHash` |
|  386608 |  387 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  388 | `				/* Node found */` |
|    1268 |  389 | `				if( ppNode ){` |
|    1256 |  390 | `					*ppNode = pNode;` |
|     627 |  391 | `				}` |
|    1268 |  392 | `				return SXRET_OK;` |
|       - |  393 | `		}` |
|       - |  394 | `		/* Follow the collision link */` |
|  776711 |  395 | `		pNode = pNode->pNextCollide;` |
|       1 |  396 | `	}` |
|       - |  397 | `	/* No such entry */` |
|   45952 |  398 | `	return SXERR_NOTFOUND;` |
|   23825 |  399 |  |
|       - |  400 | `/*` |
|       - |  401 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  402 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  403 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  404 | ` */` |
|  208646 |  405 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  406 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  407 | `	const void *pKey,           /* Lookup key */` |
|       - |  408 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  409 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  410 | `	)` |
|       2 |  411 |  |
|       - |  412 | `	ph7_hashmap_node *pNode;` |
|       - |  413 | `	sxu32 nHash;` |
|  208648 |  414 | `	if( pMap->nEntry < 1 ){` |
|       - |  415 | `		/* Don't bother hashing,there is no entry anyway */` |
|   12420 |  416 | `		return SXERR_NOTFOUND;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Hash the key first */` |
|  196230 |  419 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  420 | `	/* Point to the appropriate bucket */` |
|  196230 |  421 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  422 | `	/* Perform the lookup */` |
|  179492 |  423 | `	for(;;){` |
|  358986 |  424 | `		if( pNode == 0 ){` |
|  150140 |  425 | `			break;` |
|       - |  426 | `		}` |
|  231891 |  427 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  207347 |  428 | `			&& pNode->nHash == nHash` |
|  125969 |  429 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   46092 |  430 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  431 | `				/* Node found */` |
|   46092 |  432 | `				if( ppNode ){` |
|   46064 |  433 | `					*ppNode = pNode;` |
|   23031 |  434 | `				}` |
|   46092 |  435 | `				return SXRET_OK;` |
|       - |  436 | `		}` |
|       - |  437 | `		/* Follow the collision link */` |
|  162758 |  438 | `		pNode = pNode->pNextCollide;` |
|       2 |  439 | `	}` |
|       - |  440 | `	/* No such entry */` |
|  150140 |  441 | `	return SXERR_NOTFOUND;` |
|  104325 |  442 |  |
|       - |  443 | `/*` |
|       - |  444 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  445 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  446 | ` */` |
|  208788 |  447 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  448 |  |
|  208790 |  449 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  208790 |  450 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  208790 |  451 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  452 | `		/* Octal not decimal number */` |
|       5 |  453 | `		return FALSE;` |
|       - |  454 | `	}` |
|  208786 |  455 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  456 | `		zIn++;` |
|     ! 0 |  457 | `	}` |
|  104725 |  458 | `	for(;;){` |
|  209452 |  459 | `		if( zIn >= zEnd ){` |
|     233 |  460 | `			return TRUE;` |
|       - |  461 | `		}` |
|  209220 |  462 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  104278 |  463 | `			break;` |
|       - |  464 | `		}` |
|     667 |  465 | `		zIn++;` |
|       1 |  466 | `	}` |
|       - |  467 | `	/* Key does not look like a decimal number */` |
|  208554 |  468 | `	return FALSE;` |
|  104396 |  469 |  |
|       - |  470 | `/*` |
|       - |  471 | ` * Check if a given key exists in the given hashmap.` |
|       - |  472 | ` * Write a pointer to the target node on success.` |
|       - |  473 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  474 | ` */` |
|  106080 |  475 | `static sxi32 HashmapLookup(` |
|       - |  476 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  477 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  478 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  479 | `	)` |
|       2 |  480 |  |
|  106082 |  481 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  482 | `	sxi32 rc;` |
|  106082 |  483 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  104946 |  484 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  485 | `			/* Force a string cast */` |
|     ! 0 |  486 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  487 | `		}` |
|  104946 |  488 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  489 | `			/* Perform a blob lookup */` |
|  104930 |  490 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  104930 |  491 | `			goto result;` |
|       - |  492 | `		}` |
|       8 |  493 | `	}` |
|       - |  494 | `	/* Perform an int lookup */` |
|    1154 |  495 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  496 | `		/* Force an integer cast */` |
|      27 |  497 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  498 | `	}` |
|       - |  499 | `	/* Perform an int lookup */` |
|    1154 |  500 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   53040 |  501 | `result:` |
|  106082 |  502 | `	if( rc == SXRET_OK ){` |
|       - |  503 | `		/* Node found */` |
|   47120 |  504 | `		if( ppNode ){` |
|   47086 |  505 | `			*ppNode = pNode;` |
|   23542 |  506 | `		}` |
|   47120 |  507 | `		return SXRET_OK;` |
|       - |  508 | `	}` |
|       - |  509 | `	/* No such entry */` |
|   58964 |  510 | `	return SXERR_NOTFOUND;` |
|   53042 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  514 | ` * hashmap.` |
|       - |  515 | ` * If a node with the given key already exists in the database` |
|       - |  516 | ` * then this function overwrite the old value.` |
|       - |  517 | ` */` |
| 2986154 |  518 | `static sxi32 HashmapInsert(` |
|       - |  519 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  520 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  521 | `	ph7_value *pVal    /* Node value */` |
|       - |  522 | `	)` |
|       2 |  523 |  |
| 2986156 |  524 | `	ph7_hashmap_node *pNode = 0;` |
| 2986156 |  525 | `	sxi32 rc = SXRET_OK;` |
| 2986156 |  526 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   70098 |  527 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  528 | `			/* Force a string cast */` |
|       3 |  529 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  530 | `		}` |
|   70098 |  531 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  532 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  533 | `				/* Automatic index assign */` |
|      34 |  534 | `				pKey = 0;` |
|      16 |  535 | `			}` |
|     256 |  536 | `			goto IntKey;` |
|       - |  537 | `		}` |
|  104765 |  538 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   34921 |  539 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  540 | `				/* Overwrite the old value */` |
|       - |  541 | `				ph7_value *pElem;` |
|      55 |  542 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      55 |  543 | `				if( pElem ){` |
|      55 |  544 | `					if( pVal ){` |
|      55 |  545 | `						PH7_MemObjStore(pVal,pElem);` |
|      28 |  546 | `					}else{` |
|       - |  547 | `						/* Nullify the entry */` |
|     ! 0 |  548 | `						PH7_MemObjToNull(pElem);` |
|       - |  549 | `					}` |
|      27 |  550 | `				}` |
|      55 |  551 | `				return SXRET_OK;` |
|       - |  552 | `		}` |
|   69790 |  553 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  554 | `			/* Forbidden */` |
|       3 |  555 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Perform a blob-key insertion */` |
|   69788 |  559 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   69788 |  560 | `		return rc;` |
|       - |  561 | `	}` |
| 1458029 |  562 | `IntKey:` |
| 2916314 |  563 | `	if( pKey ){` |
|   23372 |  564 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  565 | `			/* Force an integer cast */` |
|     251 |  566 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  567 | `		}` |
|   23372 |  568 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  569 | `			/* Overwrite the old value */` |
|       - |  570 | `			ph7_value *pElem;` |
|      81 |  571 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      81 |  572 | `			if( pElem ){` |
|      81 |  573 | `				if( pVal ){` |
|      81 |  574 | `					PH7_MemObjStore(pVal,pElem);` |
|      41 |  575 | `				}else{` |
|       - |  576 | `					/* Nullify the entry */` |
|     ! 0 |  577 | `					PH7_MemObjToNull(pElem);` |
|       - |  578 | `				}` |
|      40 |  579 | `			}` |
|      81 |  580 | `			return SXRET_OK;` |
|       - |  581 | `		}` |
|   23292 |  582 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  583 | `			/* Forbidden */` |
|       3 |  584 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  585 | `			return SXRET_OK;` |
|       - |  586 | `		}` |
|       - |  587 | `		/* Perform a 64-bit-int-key insertion */` |
|   23290 |  588 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23290 |  589 | `		if( rc == SXRET_OK ){` |
|   23290 |  590 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  591 | `				/* Increment the automatic index */` |
|   23054 |  592 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  593 | `				/* Make sure the automatic index is not reserved */` |
|   23054 |  594 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  595 | `					pMap->iNextIdx++;` |
|     ! 0 |  596 | `				}` |
|   11526 |  597 | `			}` |
|   11644 |  598 | `		}` |
|   11646 |  599 | `	}else{` |
| 2892944 |  600 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  601 | `			/* Forbidden */` |
|       3 |  602 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|       - |  605 | `		/* Assign an automatic index */` |
| 2892942 |  606 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2892942 |  607 | `		if( rc == SXRET_OK ){` |
| 2892942 |  608 | `			++pMap->iNextIdx;` |
| 1446470 |  609 | `		}` |
|       - |  610 | `	}` |
|       - |  611 | `	/* Insertion result */` |
| 2916230 |  612 | `	return rc;` |
| 1493079 |  613 |  |
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
|   33812 |  641 | `static sxi32 HashmapInsertByRef(` |
|       - |  642 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  643 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  644 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  645 | `	)` |
|       2 |  646 |  |
|   33814 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|   33814 |  648 | `	sxi32 rc = SXRET_OK;` |
|   33814 |  649 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   33788 |  650 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  651 | `			/* Force a string cast */` |
|     ! 0 |  652 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  653 | `		}` |
|   33788 |  654 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  655 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  656 | `				/* Automatic index assign */` |
|     ! 0 |  657 | `				pKey = 0;` |
|     ! 0 |  658 | `			}` |
|     ! 0 |  659 | `			goto IntKey;` |
|       - |  660 | `		}` |
|   50681 |  661 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   16893 |  662 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  663 | `				/* Overwrite */` |
|       7 |  664 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  665 | `				pNode->nValIdx = nRefIdx;` |
|       - |  666 | `				/* Install in the reference table */` |
|       7 |  667 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  668 | `				return SXRET_OK;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Perform a blob-key insertion */` |
|   33782 |  671 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   33782 |  672 | `		return rc;` |
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
|   16908 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * Extract node value.` |
|       - |  712 | ` */` |
| 1121537 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
| 1121539 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1121539 |  718 | `	return pObj;` |
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
|   53412 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   53414 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   53414 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   53414 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   53414 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   53414 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   53414 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   53414 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   53414 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   53414 |  783 | `	return rc;` |
|   26723 |  784 |  |
|       - |  785 | `/*` |
|       - |  786 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  787 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  788 | ` */` |
|   11010 |  789 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  790 |  |
|   11012 |  791 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  792 | `	sxu32 nBucket;` |
|       - |  793 | `	/* Remove old collision links */` |
|   11012 |  794 | `	if( pEntry->pPrevCollide ){` |
|    8899 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    4454 |  796 | `	}else{` |
|    2115 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  798 | `	}` |
|   11012 |  799 | `	if( pEntry->pNextCollide ){` |
|     892 |  800 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     450 |  801 | `	}` |
|   11012 |  802 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  803 | `	/* Compute the new hash */` |
|   11012 |  804 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   11012 |  805 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   11012 |  806 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  807 | `	/* Link to the new bucket */` |
|   11012 |  808 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11012 |  809 | `	if( pMap->apBucket[nBucket] ){` |
|    9123 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    4569 |  811 | `	}` |
|   11012 |  812 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11012 |  813 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  814 | `	/* Increment the automatic index */` |
|   11012 |  815 | `	pMap->iNextIdx++;` |
|   11012 |  816 |  |
|       - |  817 | `/*` |
|       - |  818 | ` * Perform a linear search on a given hashmap.` |
|       - |  819 | ` * Write a pointer to the target node on success.` |
|       - |  820 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  821 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  822 | ` * for more information.` |
|       - |  823 | ` */` |
|   27620 |  824 | `static int HashmapFindValue(` |
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
|   27622 |  837 | `	pEntry = pMap->pFirst;` |
|   27622 |  838 | `	n = pMap->nEntry;` |
|   27622 |  839 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   27622 |  840 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   66205 |  841 | `	for(;;){` |
|  132411 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  132313 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  132313 |  847 | `		if( pVal ){` |
|  132313 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  132313 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  132313 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  132313 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  132313 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  132313 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  132313 |  865 | `				if( rc == 0 ){` |
|   27524 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   27524 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   52395 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|  104791 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  104791 |  876 | `		n--;` |
|       2 |  877 | `	}` |
|       - |  878 | `	/* No such entry */` |
|      99 |  879 | `	return SXERR_NOTFOUND;` |
|   13812 |  880 |  |
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
|      14 |  989 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - |  990 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - |  991 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - |  992 | `	int bStrict          /* TRUE for strict comparison */` |
|       - |  993 | `	)` |
|       1 |  994 |  |
|       - |  995 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - |  996 | `	sxi32 rc;` |
|       - |  997 | `	sxu32 n;` |
|      15 |  998 | `	if( pLeft == pRight ){` |
|       - |  999 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - | 1000 | `		 * Unlike the zend engine.` |
|       - | 1001 | `		 */` |
|     ! 0 | 1002 | `		return 0;` |
|       - | 1003 | `	}` |
|      15 | 1004 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - | 1005 | `		/* Must have the same number of entries */` |
|       5 | 1006 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1007 | `	}` |
|       - | 1008 | `	/* Point to the first inserted entry of the left hashmap */` |
|      11 | 1009 | `	pLe = pLeft->pFirst;` |
|      11 | 1010 | `	pRe = 0; /* cc warning */` |
|       - | 1011 | `	/* Perform the comparison */` |
|      11 | 1012 | `	n = pLeft->nEntry;` |
|      11 | 1013 | `	for(;;){` |
|      23 | 1014 | `		if( n < 1 ){` |
|       9 | 1015 | `			break;` |
|       - | 1016 | `		}` |
|      15 | 1017 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1018 | `			/* Int key */` |
|      11 | 1019 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|       6 | 1020 | `		}else{` |
|       5 | 1021 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1022 | `			/* Blob key */` |
|       5 | 1023 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1024 | `		}` |
|      15 | 1025 | `		if( rc != SXRET_OK ){` |
|       - | 1026 | `			/* No such entry in the right side */` |
|     ! 0 | 1027 | `			return 1;` |
|       - | 1028 | `		}` |
|      15 | 1029 | `		rc = 0;` |
|      15 | 1030 | `		if( bStrict ){` |
|       - | 1031 | `			/* Make sure,the keys are of the same type */` |
|       3 | 1032 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1033 | `				rc = 1;` |
|     ! 0 | 1034 | `			}` |
|       1 | 1035 | `		}` |
|      15 | 1036 | `		if( !rc ){` |
|       - | 1037 | `			/* Compare nodes */` |
|      15 | 1038 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|       7 | 1039 | `		}` |
|      15 | 1040 | `		if( rc != 0 ){` |
|       - | 1041 | `			/* Nodes key/value differ */` |
|       3 | 1042 | `			return rc;` |
|       - | 1043 | `		}` |
|       - | 1044 | `		/* Point to the next entry */` |
|      13 | 1045 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      13 | 1046 | `		n--;` |
|       1 | 1047 | `	}` |
|       9 | 1048 | `	return 0; /* Hashmaps are equals */` |
|       8 | 1049 |  |
|       - | 1050 | `/*` |
|       - | 1051 | ` * Duplicate a hashmap node.` |
|       - | 1052 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1053 | ` */` |
|  523610 | 1054 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1055 | `	ph7_hashmap *pDest,` |
|       - | 1056 | `	ph7_hashmap_node *pEntry,` |
|       - | 1057 | `	ph7_value *pVal,` |
|       - | 1058 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1059 | `	)` |
|       2 | 1060 |  |
|  523612 | 1061 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1062 | `	ph7_value sKey;` |
|       - | 1063 | `	sxi32 rc;` |
|       - | 1064 |  |
|  523612 | 1065 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1066 | `		/* Blob key insertion */` |
|      59 | 1067 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      59 | 1068 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      59 | 1069 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      59 | 1070 | `		PH7_MemObjRelease(&sKey);` |
|      30 | 1071 | `	}else{` |
|       - | 1072 | `		/* Int key */` |
|  523554 | 1073 | `		if( iAction == 0 ){ /* Merge */` |
|  523336 | 1074 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  261887 | 1075 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1076 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1077 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1078 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1079 | `		}else{ /* Dup */` |
|     190 | 1080 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1081 | `		}` |
|       - | 1082 | `	}` |
|  523612 | 1083 | `	return rc;` |
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
|    1864 | 1097 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1098 |  |
|       - | 1099 | `	ph7_hashmap_node *pEntry;` |
|       - | 1100 | `	ph7_value *pVal;` |
|       - | 1101 | `	sxi32 rc;` |
|       - | 1102 | `	sxu32 n;` |
|    1866 | 1103 | `	if( pSrc == pDest ){` |
|       - | 1104 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1105 | `		 * Unlike the zend engine.` |
|       - | 1106 | `		 */` |
|     ! 0 | 1107 | `		return SXRET_OK;` |
|       - | 1108 | `	}` |
|       - | 1109 | `	/* Point to the first inserted entry in the source */` |
|    1866 | 1110 | `	pEntry = pSrc->pFirst;` |
|       - | 1111 | `	/* Perform the merge */` |
|  525216 | 1112 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1113 | `		/* Extract the node value */` |
|  523352 | 1114 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  523352 | 1115 | `		if( pVal ){` |
|       - | 1116 | `			/* Make a local copy of the value.` |
|       - | 1117 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1118 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1119 | `			 * to the old pool.` |
|       - | 1120 | `			 */` |
|  523352 | 1121 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  261677 | 1122 | `		}else{` |
|     ! 0 | 1123 | `			rc = SXRET_OK;` |
|       - | 1124 | `		}` |
|  523352 | 1125 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1126 | `			return rc;` |
|       - | 1127 | `		}` |
|       - | 1128 | `		/* Point to the next entry */` |
|  523352 | 1129 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  261677 | 1130 | `	}` |
|    1866 | 1131 | `	return SXRET_OK;` |
|     934 | 1132 |  |
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
|     102 | 1182 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1183 |  |
|       - | 1184 | `	ph7_hashmap_node *pEntry;` |
|       - | 1185 | `	ph7_value *pVal;` |
|       - | 1186 | `	sxi32 rc;` |
|       - | 1187 | `	sxu32 n;` |
|     104 | 1188 | `	if( pSrc == pDest ){` |
|       - | 1189 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1190 | `		 * Unlike the zend engine.` |
|       - | 1191 | `		 */` |
|     ! 0 | 1192 | `		return SXRET_OK;` |
|       - | 1193 | `	}` |
|       - | 1194 | `	/* Point to the first inserted entry in the source */` |
|     104 | 1195 | `	pEntry = pSrc->pFirst;` |
|       - | 1196 | `	/* Perform the duplication */` |
|     320 | 1197 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1198 | `		/* Extract the node value */` |
|     218 | 1199 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     218 | 1200 | `		if( pVal ){` |
|     218 | 1201 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|     110 | 1202 | `		}else{` |
|     ! 0 | 1203 | `			rc = SXRET_OK;` |
|       - | 1204 | `		}` |
|     218 | 1205 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1206 | `			return rc;` |
|       - | 1207 | `		}` |
|       - | 1208 | `		/* Point to the next entry */` |
|     218 | 1209 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     110 | 1210 | `	}` |
|     104 | 1211 | `	return SXRET_OK;` |
|      53 | 1212 |  |
|       - | 1213 | `/*` |
|       - | 1214 | ` * Copy-on-write separation for arrays.` |
|       - | 1215 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|       - | 1216 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|       - | 1217 | ` * Returns the (possibly new) hashmap pointer.` |
|       - | 1218 | ` */` |
|  185394 | 1219 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       2 | 1220 |  |
|  185396 | 1221 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1222 | `	ph7_hashmap *pNew;` |
|       - | 1223 | `	ph7_value *pBacking;` |
|  185396 | 1224 | `	if( pMap->iRef < 2 ){` |
|       - | 1225 | `		/* Sole owner, no separation needed */` |
|  183358 | 1226 | `		return pMap;` |
|       - | 1227 | `	}` |
|    2040 | 1228 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1229 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1230 | `		return pMap;` |
|       - | 1231 | `	}` |
|       - | 1232 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1233 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1234 | `	 * frame is popped. */` |
|    2040 | 1235 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2040 | 1236 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    3041 | 1237 | `		if( pBacking && pBacking != pValue` |
|    2022 | 1238 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2008 | 1239 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1240 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2008 | 1241 | `			pMap->iRef--;` |
|    2008 | 1242 | `			if( pMap->iRef < 2 ){` |
|       - | 1243 | `				/* After undoing stack ref, sole owner — no separation */` |
|    1972 | 1244 | `				pMap->iRef++;` |
|    1972 | 1245 | `				return pMap;` |
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
|      16 | 1266 | `	}` |
|      33 | 1267 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      33 | 1268 | `	if( pNew == 0 ){` |
|       - | 1269 | `		/* Allocation failure — fall through with shared map */` |
|     ! 0 | 1270 | `		return pMap;` |
|       - | 1271 | `	}` |
|      33 | 1272 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1273 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|     ! 0 | 1274 | `		PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1275 | `		return pMap;` |
|       - | 1276 | `	}` |
|      33 | 1277 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      33 | 1278 | `	pMap->iRef--;` |
|      33 | 1279 | `	pValue->x.pOther = pNew;` |
|      33 | 1280 | `	return pNew;` |
|   92699 | 1281 |  |
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
|   80036 | 1373 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1374 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1375 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1376 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1377 | `	)` |
|       2 | 1378 |  |
|       - | 1379 | `	ph7_hashmap *pMap;` |
|       - | 1380 | `	/* Allocate a new instance */` |
|   80038 | 1381 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   80038 | 1382 | `	if( pMap == 0 ){` |
|     ! 0 | 1383 | `		return 0;` |
|       - | 1384 | `	}` |
|       - | 1385 | `	/* Zero the structure */` |
|   80038 | 1386 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1387 | `	/* Fill in the structure */` |
|   80038 | 1388 | `	pMap->pVm = &(*pVm);` |
|   80038 | 1389 | `	pMap->iRef = 1;` |
|       - | 1390 | `	/* Default hash functions */` |
|   80038 | 1391 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   80038 | 1392 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   80038 | 1393 | `	return pMap;` |
|   40020 | 1394 |  |
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
|    2622 | 1415 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    2624 | 1435 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    2624 | 1436 | `	if( pMap == 0 ){` |
|     ! 0 | 1437 | `		return SXERR_MEM;` |
|       - | 1438 | `	}` |
|    2624 | 1439 | `	pVm->pGlobal = pMap;` |
|       - | 1440 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    2624 | 1441 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    2624 | 1442 | `	if( pObj == 0 ){` |
|     ! 0 | 1443 | `		return SXERR_MEM;` |
|       - | 1444 | `	}` |
|    2624 | 1445 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1446 | `	/* Record object index */` |
|    2624 | 1447 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1448 | `	/* Install the special $GLOBALS array */` |
|    2624 | 1449 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    2624 | 1450 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1451 | `		return rc;` |
|       - | 1452 | `	}` |
|       - | 1453 | `	/* Install superglobals now */` |
|   28844 | 1454 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1455 | `		ph7_value *pSuper;` |
|       - | 1456 | `		/* Request an empty array */` |
|   26222 | 1457 | `		pSuper = ph7_new_array(&(*pVm));` |
|   26222 | 1458 | `		if( pSuper == 0 ){` |
|     ! 0 | 1459 | `			return SXERR_MEM;` |
|       - | 1460 | `		}` |
|       - | 1461 | `		/* Install */` |
|   26222 | 1462 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   26222 | 1463 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1464 | `			return rc;` |
|       - | 1465 | `		}` |
|       - | 1466 | `		/* Release the value now it have been installed */` |
|   26222 | 1467 | `		ph7_release_value(&(*pVm),pSuper);` |
|   13112 | 1468 | `	}` |
|       - | 1469 | `	/* Set some $_SERVER entries */` |
|    2624 | 1470 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1471 | `	/*` |
|       - | 1472 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1473 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1474 | `	 */` |
|    5242 | 1475 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1476 | `		"SCRIPT_FILENAME",` |
|    1311 | 1477 | `		pFile ? pFile->zString : ":Memory:",` |
|    2618 | 1478 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1479 | `		);` |
|       - | 1480 | `	/* All done,all super-global are installed now */` |
|    2624 | 1481 | `	return SXRET_OK;` |
|    1313 | 1482 |  |
|       - | 1483 | `/*` |
|       - | 1484 | ` * Release a hashmap.` |
|       - | 1485 | ` */` |
|   51010 | 1486 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1487 |  |
|       - | 1488 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   51012 | 1489 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1490 | `	sxu32 n;` |
|   51012 | 1491 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1492 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1493 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1494 | `		return SXRET_OK;` |
|       - | 1495 | `	}` |
|       - | 1496 | `	/* Start the release process */` |
|   51012 | 1497 | `	n = 0;` |
|   51012 | 1498 | `	pEntry = pMap->pFirst;` |
| 1513922 | 1499 | `	for(;;){` |
| 3027846 | 1500 | `		if( n >= pMap->nEntry ){` |
|   51012 | 1501 | `			break;` |
|       - | 1502 | `		}` |
| 2976836 | 1503 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1504 | `		/* Remove the reference from the foreign table */` |
| 2976836 | 1505 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2976836 | 1506 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1507 | `			/* Restore the ph7_value to the free list */` |
| 2976828 | 1508 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1488413 | 1509 | `		}` |
|       - | 1510 | `		/* Release the node */` |
| 2976836 | 1511 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   65976 | 1512 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   32987 | 1513 | `		}` |
| 2976836 | 1514 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1515 | `		/* Point to the next entry */` |
| 2976836 | 1516 | `		pEntry = pNext;` |
| 2976836 | 1517 | `		n++;` |
|       2 | 1518 | `	}` |
|   51012 | 1519 | `	if( pMap->nEntry > 0 ){` |
|       - | 1520 | `		/* Release the hash bucket */` |
|   45338 | 1521 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   22668 | 1522 | `	}` |
|   51012 | 1523 | `	if( FreeDS ){` |
|       - | 1524 | `		/* Free the whole instance */` |
|   50996 | 1525 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   25499 | 1526 | `	}else{` |
|       - | 1527 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1528 | `		pMap->apBucket = 0;` |
|      17 | 1529 | `		pMap->iNextIdx = 0;` |
|      17 | 1530 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1531 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1532 | `	}` |
|   51012 | 1533 | `	return SXRET_OK;` |
|   25507 | 1534 |  |
|       - | 1535 | `/*` |
|       - | 1536 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1537 | ` * If the count reaches zero which mean no more variables` |
|       - | 1538 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1539 | ` */` |
|  567502 | 1540 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1541 |  |
|  567504 | 1542 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1543 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  567504 | 1544 | `	pMap->iRef--;` |
|  567504 | 1545 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   50996 | 1546 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   25497 | 1547 | `	}` |
|  567504 | 1548 |  |
|       - | 1549 | `/*` |
|       - | 1550 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1551 | ` * Write a pointer to the target node on success.` |
|       - | 1552 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1553 | ` */` |
|  106110 | 1554 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1555 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1556 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1557 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1558 | `	)` |
|       2 | 1559 |  |
|       - | 1560 | `	sxi32 rc;` |
|  106112 | 1561 | `	if( pMap->nEntry < 1 ){` |
|       - | 1562 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1563 | `		 */` |
|      31 | 1564 | `		return SXERR_NOTFOUND;` |
|       - | 1565 | `	}` |
|  106082 | 1566 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  106082 | 1567 | `	return rc;` |
|   53057 | 1568 |  |
|       - | 1569 | `/*` |
|       - | 1570 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1571 | ` * hashmap.` |
|       - | 1572 | ` * If a node with the given key already exists in the database` |
|       - | 1573 | ` * then this function overwrite the old value.` |
|       - | 1574 | ` */` |
| 2462608 | 1575 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1576 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1577 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1578 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1579 | `	)` |
|       2 | 1580 |  |
|       - | 1581 | `	sxi32 rc;` |
| 2462610 | 1582 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1583 | `		/*` |
|       - | 1584 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1585 | `		 */` |
|     ! 0 | 1586 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1587 | `		return SXRET_OK;` |
|       - | 1588 | `	}` |
| 2462610 | 1589 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2462610 | 1590 | `	return rc;` |
| 1231306 | 1591 |  |
|       - | 1592 | `/*` |
|       - | 1593 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1594 | ` * hashmap.` |
|       - | 1595 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1596 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1597 | ` * The insertion by reference is triggered when the following` |
|       - | 1598 | ` * expression is encountered.` |
|       - | 1599 | ` * $var = 10;` |
|       - | 1600 | ` *  $a = array(&var);` |
|       - | 1601 | ` * OR` |
|       - | 1602 | ` *  $a[] =& $var;` |
|       - | 1603 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1604 | ` * over it's contents.` |
|       - | 1605 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1606 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1607 | ` * Example:` |
|       - | 1608 | ` *  $var = 10;` |
|       - | 1609 | ` *  $a[] =& $var;` |
|       - | 1610 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1611 | ` *  //Unset the foreign ph7_value now` |
|       - | 1612 | ` *  unset($var);` |
|       - | 1613 | ` *  echo count($a); //0` |
|       - | 1614 | ` * Note that this is a PH7 eXtension.` |
|       - | 1615 | ` * Refer to the official documentation for more information.` |
|       - | 1616 | ` * If a node with the given key already exists in the database` |
|       - | 1617 | ` * then this function overwrite the old value.` |
|       - | 1618 | ` */` |
|   33812 | 1619 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1620 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1621 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1622 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1623 | `	)` |
|       2 | 1624 |  |
|       - | 1625 | `	sxi32 rc;` |
|   33814 | 1626 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1627 | `		/*` |
|       - | 1628 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1629 | `		 */` |
|     ! 0 | 1630 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1631 | `		return SXRET_OK;` |
|       - | 1632 | `	}` |
|   33814 | 1633 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   33814 | 1634 | `	return rc;` |
|   16908 | 1635 |  |
|       - | 1636 | `/*` |
|       - | 1637 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1638 | ` */` |
|   22906 | 1639 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1640 |  |
|       - | 1641 | `	/* Reset the loop cursor */` |
|   22908 | 1642 | `	pMap->pCur = pMap->pFirst;` |
|   22908 | 1643 |  |
|       - | 1644 | `/*` |
|       - | 1645 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1646 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1647 | ` * return NULL.` |
|       - | 1648 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1649 | ` */` |
|  187676 | 1650 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1651 |  |
|  187678 | 1652 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  187678 | 1653 | `	if( pCur == 0 ){` |
|       - | 1654 | `		/* End of the list,return null */` |
|   11474 | 1655 | `		return 0;` |
|       - | 1656 | `	}` |
|       - | 1657 | `	/* Advance the node cursor */` |
|  176206 | 1658 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  176206 | 1659 | `	return pCur;` |
|   93840 | 1660 |  |
|       - | 1661 | `/*` |
|       - | 1662 | ` * Extract a node value.` |
|       - | 1663 | ` */` |
|  441776 | 1664 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1665 |  |
|  441778 | 1666 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  441778 | 1667 | `	if( pEntry ){` |
|  441778 | 1668 | `		if( bStore ){` |
|  176340 | 1669 | `			PH7_MemObjStore(pEntry,pValue);` |
|   88171 | 1670 | `		}else{` |
|  265440 | 1671 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1672 | `		}` |
|  220920 | 1673 | `	}else{` |
|     ! 0 | 1674 | `		PH7_MemObjRelease(pValue);` |
|       - | 1675 | `	}` |
|  441778 | 1676 |  |
|       - | 1677 | `/*` |
|       - | 1678 | ` * Extract a node key.` |
|       - | 1679 | ` */` |
|  111906 | 1680 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1681 |  |
|       - | 1682 | `	/* Fill with the current key */` |
|  111908 | 1683 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  111618 | 1684 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      17 | 1685 | `			SyBlobRelease(&pKey->sBlob);` |
|       8 | 1686 | `		}` |
|  111618 | 1687 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  111618 | 1688 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   55810 | 1689 | `	}else{` |
|     291 | 1690 | `		SyBlobReset(&pKey->sBlob);` |
|     291 | 1691 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     291 | 1692 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1693 | `	}` |
|  111908 | 1694 |  |
|       - | 1695 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1696 | `/*` |
|       - | 1697 | ` * Store the address of nodes value in the given container.` |
|       - | 1698 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1699 | ` * defined in 'builtin.c' for more information.` |
|       - | 1700 | ` */` |
|      10 | 1701 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1702 |  |
|      11 | 1703 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1704 | `	ph7_value *pValue;` |
|       - | 1705 | `	sxu32 n;` |
|       - | 1706 | `	/* Initialize the container */` |
|      11 | 1707 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1708 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1709 | `		/* Extract node value */` |
|      17 | 1710 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1711 | `		if( pValue ){` |
|      17 | 1712 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1713 | `		}` |
|       - | 1714 | `		/* Point to the next entry */` |
|      17 | 1715 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1716 | `	}` |
|       - | 1717 | `	/* Total inserted entries */` |
|      11 | 1718 | `	return (int)SySetUsed(pOut);` |
|       1 | 1719 |  |
|       - | 1720 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1721 | `/*` |
|       - | 1722 | ` * Merge sort.` |
|       - | 1723 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1724 | ` * Status: Public domain` |
|       - | 1725 | ` */` |
|       - | 1726 | `/* Node comparison callback signature */` |
|       - | 1727 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1728 | `/*` |
|       - | 1729 | `** Inputs:` |
|       - | 1730 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1731 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1732 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1733 | `**` |
|       - | 1734 | `** Return Value:` |
|       - | 1735 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1736 | `**   of both a and b.` |
|       - | 1737 | `**` |
|       - | 1738 | `** Side effects:` |
|       - | 1739 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1740 | `**   changed.` |
|       - | 1741 | `*/` |
|   29682 | 1742 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1743 |  |
|       - | 1744 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1745 | `    /* Prevent compiler warning */` |
|   29684 | 1746 | `	result.pNext = result.pPrev = 0;` |
|   29684 | 1747 | `	pTail = &result;` |
|   83176 | 1748 | `	while( pA && pB ){` |
|   53494 | 1749 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   34618 | 1750 | `			pTail->pPrev = pA;` |
|   34618 | 1751 | `			pA->pNext = pTail;` |
|   34618 | 1752 | `			pTail = pA;` |
|   34618 | 1753 | `			pA = pA->pPrev;` |
|   17288 | 1754 | `		}else{` |
|   18878 | 1755 | `			pTail->pPrev = pB;` |
|   18878 | 1756 | `			pB->pNext = pTail;` |
|   18878 | 1757 | `			pTail = pB;` |
|   18878 | 1758 | `			pB = pB->pPrev;` |
|       - | 1759 | `		}` |
|       2 | 1760 | `	}` |
|   29684 | 1761 | `	if( pA ){` |
|   21274 | 1762 | `		pTail->pPrev = pA;` |
|   21274 | 1763 | `		pA->pNext = pTail;` |
|   19060 | 1764 | `	}else if( pB ){` |
|    8212 | 1765 | `		pTail->pPrev = pB;` |
|    8212 | 1766 | `		pB->pNext = pTail;` |
|    4095 | 1767 | `	}else{` |
|     202 | 1768 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1769 | `	}` |
|   29684 | 1770 | `	return result.pPrev;` |
|       2 | 1771 |  |
|       - | 1772 | `/*` |
|       - | 1773 | `** Inputs:` |
|       - | 1774 | `**   Map:       Input hashmap` |
|       - | 1775 | `**   cmp:       A comparison function.` |
|       - | 1776 | `**` |
|       - | 1777 | `** Return Value:` |
|       - | 1778 | `**   Sorted hashmap.` |
|       - | 1779 | `**` |
|       - | 1780 | `** Side effects:` |
|       - | 1781 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1782 | `*/` |
|       - | 1783 | `#define N_SORT_BUCKET  32` |
|     634 | 1784 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1785 |  |
|       - | 1786 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1787 | `	sxu32 i;` |
|     636 | 1788 | `	SyZero(a,sizeof(a));` |
|       - | 1789 | `	/* Point to the first inserted entry */` |
|     636 | 1790 | `	pIn = pMap->pFirst;` |
|   11762 | 1791 | `	while( pIn ){` |
|   11128 | 1792 | `		p = pIn;` |
|   11128 | 1793 | `		pIn = p->pPrev;` |
|   11128 | 1794 | `		p->pPrev = 0;` |
|   21156 | 1795 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   21156 | 1796 | `			if( a[i]==0 ){` |
|   11128 | 1797 | `				a[i] = p;` |
|   11128 | 1798 | `				break;` |
|     ! 0 | 1799 | `			}else{` |
|   10030 | 1800 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   10030 | 1801 | `				a[i] = 0;` |
|       - | 1802 | `			}` |
|    5016 | 1803 | `		}` |
|   11128 | 1804 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1805 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1806 | `			 * But that is impossible.` |
|       - | 1807 | `			 */` |
|     ! 0 | 1808 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1809 | `		}` |
|       2 | 1810 | `	}` |
|     636 | 1811 | `	p = a[0];` |
|   20290 | 1812 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   19656 | 1813 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    9829 | 1814 | `	}` |
|     636 | 1815 | `	p->pNext = 0;` |
|       - | 1816 | `	/* Reflect the change */` |
|     636 | 1817 | `	pMap->pFirst = p;` |
|       - | 1818 | `	/* Reset the loop cursor */` |
|     636 | 1819 | `	pMap->pCur = pMap->pFirst;` |
|     636 | 1820 | `	return SXRET_OK;` |
|       2 | 1821 |  |
|       - | 1822 | `/*` |
|       - | 1823 | ` * Node comparison callback.` |
|       - | 1824 | ` * used-by: [sort(),asort(),...]` |
|       - | 1825 | ` */` |
|   53364 | 1826 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1827 |  |
|       - | 1828 | `	ph7_value sA,sB;` |
|       - | 1829 | `	sxi32 iFlags;` |
|       - | 1830 | `	int rc;` |
|   53366 | 1831 | `	if( pCmpData == 0 ){` |
|       - | 1832 | `		/* Perform a standard comparison */` |
|   53342 | 1833 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   53342 | 1834 | `		return rc;` |
|       - | 1835 | `	}` |
|      25 | 1836 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1837 | `	/* Duplicate node values */` |
|      25 | 1838 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 1839 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 1840 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 1841 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 1842 | `	if( iFlags == 5 ){` |
|       - | 1843 | `		/* String cast */` |
|       - | 1844 | `		const char *zA,*zB;` |
|       - | 1845 | `		sxu32 nA,nB,nMin;` |
|      15 | 1846 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1847 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1848 | `		}` |
|      15 | 1849 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1850 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1851 | `		}` |
|       - | 1852 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 1853 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 1854 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 1855 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 1856 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 1857 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 1858 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 1859 | `		if( rc == 0 ){` |
|       5 | 1860 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1861 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1862 | `		}` |
|       8 | 1863 | `	}else{` |
|       - | 1864 | `		/* Numeric cast */` |
|      11 | 1865 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1866 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1867 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1868 | `	}` |
|      25 | 1869 | `	PH7_MemObjRelease(&sA);` |
|      25 | 1870 | `	PH7_MemObjRelease(&sB);` |
|      25 | 1871 | `	return rc;` |
|   26699 | 1872 |  |
|       - | 1873 | `/*` |
|       - | 1874 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1875 | ` * used-by: [ksort()]` |
|       - | 1876 | ` */` |
|      14 | 1877 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1878 |  |
|       - | 1879 | `	sxi32 rc;` |
|       7 | 1880 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 1881 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1882 | `		/* Perform a string comparison */` |
|       5 | 1883 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1884 | `	}else{` |
|       - | 1885 | `		SyString sStr;` |
|       - | 1886 | `		sxi64 iA,iB;` |
|       - | 1887 | `		/* Perform a numeric comparison */` |
|      11 | 1888 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1889 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1890 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1891 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1892 | `				iA = 0;` |
|     ! 0 | 1893 | `			}else{` |
|     ! 0 | 1894 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1895 | `			}` |
|     ! 0 | 1896 | `		}else{` |
|      11 | 1897 | `			iA = pA->xKey.iKey;` |
|       - | 1898 | `		}` |
|      11 | 1899 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1900 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1901 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1902 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1903 | `				iB = 0;` |
|     ! 0 | 1904 | `			}else{` |
|     ! 0 | 1905 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1906 | `			}` |
|     ! 0 | 1907 | `		}else{` |
|      11 | 1908 | `			iB = pB->xKey.iKey;` |
|       - | 1909 | `		}` |
|      11 | 1910 | `		rc = (sxi32)(iA-iB);` |
|       - | 1911 | `	}` |
|       - | 1912 | `	/* Comparison result */` |
|      15 | 1913 | `	return rc;` |
|       1 | 1914 |  |
|       - | 1915 | `/*` |
|       - | 1916 | ` * Node comparison callback.` |
|       - | 1917 | ` * Used by: [rsort(),arsort()];` |
|       - | 1918 | ` */` |
|      78 | 1919 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1920 |  |
|       - | 1921 | `	ph7_value sA,sB;` |
|       - | 1922 | `	sxi32 iFlags;` |
|       - | 1923 | `	int rc;` |
|      79 | 1924 | `	if( pCmpData == 0 ){` |
|       - | 1925 | `		/* Perform a standard comparison */` |
|      59 | 1926 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 1927 | `		return -rc;` |
|       - | 1928 | `	}` |
|      21 | 1929 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1930 | `	/* Duplicate node values */` |
|      21 | 1931 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 1932 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 1933 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 1934 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 1935 | `	if( iFlags == 5 ){` |
|       - | 1936 | `		/* String cast */` |
|       - | 1937 | `		const char *zA,*zB;` |
|       - | 1938 | `		sxu32 nA,nB,nMin;` |
|      11 | 1939 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1940 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1941 | `		}` |
|      11 | 1942 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1943 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1944 | `		}` |
|       - | 1945 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 1946 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 1947 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 1948 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 1949 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 1950 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 1951 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 1952 | `		if( rc == 0 ){` |
|       3 | 1953 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1954 | `			else if( nA > nB ) rc = 1;` |
|       1 | 1955 | `		}` |
|       6 | 1956 | `	}else{` |
|       - | 1957 | `		/* Numeric cast */` |
|      11 | 1958 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1959 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1960 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1961 | `	}` |
|      21 | 1962 | `	PH7_MemObjRelease(&sA);` |
|      21 | 1963 | `	PH7_MemObjRelease(&sB);` |
|      21 | 1964 | `	return -rc;` |
|      40 | 1965 |  |
|       - | 1966 | `/*` |
|       - | 1967 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1968 | ` * used-by: [usort(),uasort()]` |
|       - | 1969 | ` */` |
|      12 | 1970 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1971 |  |
|       - | 1972 | `	ph7_value sResult,*pCallback;` |
|       - | 1973 | `	ph7_value *pV1,*pV2;` |
|       - | 1974 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1975 | `	sxi32 rc;` |
|       - | 1976 | `	/* Point to the desired callback */` |
|      13 | 1977 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1978 | `	/* initialize the result value */` |
|      13 | 1979 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 1980 | `	/* Extract nodes values */` |
|      13 | 1981 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      13 | 1982 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      13 | 1983 | `	apArg[0] = pV1;` |
|      13 | 1984 | `	apArg[1] = pV2;` |
|       - | 1985 | `	/* Invoke the callback */` |
|      13 | 1986 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      13 | 1987 | `	if( rc != SXRET_OK ){` |
|       - | 1988 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1989 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1990 | `	}else{` |
|       - | 1991 | `		/* Extract callback result */` |
|      13 | 1992 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1993 | `			/* Perform an int cast */` |
|     ! 0 | 1994 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1995 | `		}` |
|      13 | 1996 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1997 | `	}` |
|      13 | 1998 | `	PH7_MemObjRelease(&sResult);` |
|       - | 1999 | `	/* Callback result */` |
|      13 | 2000 | `	return rc;` |
|       1 | 2001 |  |
|       - | 2002 | `/*` |
|       - | 2003 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2004 | ` * used-by: [krsort()]` |
|       - | 2005 | ` */` |
|       4 | 2006 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2007 |  |
|       - | 2008 | `	sxi32 rc;` |
|       2 | 2009 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2010 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2011 | `		/* Perform a string comparison */` |
|       5 | 2012 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2013 | `	}else{` |
|       - | 2014 | `		SyString sStr;` |
|       - | 2015 | `		sxi64 iA,iB;` |
|       - | 2016 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2017 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2018 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2019 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2020 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2021 | `				iA = 0;` |
|     ! 0 | 2022 | `			}else{` |
|     ! 0 | 2023 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2024 | `			}` |
|     ! 0 | 2025 | `		}else{` |
|     ! 0 | 2026 | `			iA = pA->xKey.iKey;` |
|       - | 2027 | `		}` |
|     ! 0 | 2028 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2029 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2030 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2031 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2032 | `				iB = 0;` |
|     ! 0 | 2033 | `			}else{` |
|     ! 0 | 2034 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2035 | `			}` |
|     ! 0 | 2036 | `		}else{` |
|     ! 0 | 2037 | `			iB = pB->xKey.iKey;` |
|       - | 2038 | `		}` |
|     ! 0 | 2039 | `		rc = (sxi32)(iA-iB);` |
|       - | 2040 | `	}` |
|       5 | 2041 | `	return -rc; /* Reverse result */` |
|       1 | 2042 |  |
|       - | 2043 | `/*` |
|       - | 2044 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2045 | ` * used-by: [uksort()]` |
|       - | 2046 | ` */` |
|       6 | 2047 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2048 |  |
|       - | 2049 | `	ph7_value sResult,*pCallback;` |
|       - | 2050 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2051 | `	ph7_value sK1,sK2;` |
|       - | 2052 | `	sxi32 rc;` |
|       - | 2053 | `	/* Point to the desired callback */` |
|       7 | 2054 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 2055 | `	/* initialize the result value */` |
|       7 | 2056 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2057 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2058 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2059 | `	/* Extract nodes keys */` |
|       7 | 2060 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2061 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2062 | `	apArg[0] = &sK1;` |
|       7 | 2063 | `	apArg[1] = &sK2;` |
|       - | 2064 | `	/* Mark keys as constants */` |
|       7 | 2065 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2066 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2067 | `	/* Invoke the callback */` |
|       7 | 2068 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2069 | `	if( rc != SXRET_OK ){` |
|       - | 2070 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2071 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2072 | `	}else{` |
|       - | 2073 | `		/* Extract callback result */` |
|       7 | 2074 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2075 | `			/* Perform an int cast */` |
|     ! 0 | 2076 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2077 | `		}` |
|       7 | 2078 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2079 | `	}` |
|       7 | 2080 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2081 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2082 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2083 | `	/* Callback result */` |
|       7 | 2084 | `	return rc;` |
|       1 | 2085 |  |
|       - | 2086 | `/*` |
|       - | 2087 | ` * Node comparison callback: Random node comparison.` |
|       - | 2088 | ` * used-by: [shuffle()]` |
|       - | 2089 | ` */` |
|      14 | 2090 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2091 |  |
|       - | 2092 | `	sxu32 n;` |
|       7 | 2093 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 2094 | `	SXUNUSED(pCmpData);` |
|       - | 2095 | `	/* Grab a random number */` |
|      15 | 2096 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2097 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2098 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2099 | `	 */` |
|      15 | 2100 | `	return n&1 ? 1 : -1;` |
|       1 | 2101 |  |
|       - | 2102 | `/*` |
|       - | 2103 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2104 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2105 | ` */` |
|     586 | 2106 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2107 |  |
|       - | 2108 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2109 | `	sxu32 i;` |
|       - | 2110 | `	/* Rehash all entries */` |
|     588 | 2111 | `	pLast = p = pMap->pFirst;` |
|     588 | 2112 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     588 | 2113 | `	i = 0;` |
|    5771 | 2114 | `	for( ;; ){` |
|   11544 | 2115 | `		if( i >= pMap->nEntry ){` |
|     588 | 2116 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     588 | 2117 | `			break;` |
|       - | 2118 | `		}` |
|   10958 | 2119 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2120 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2121 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2122 | `			/* Change key type */` |
|       5 | 2123 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2124 | `		}` |
|   10958 | 2125 | `		HashmapRehashIntNode(p);` |
|       - | 2126 | `		/* Point to the next entry */` |
|   10958 | 2127 | `		i++;` |
|   10958 | 2128 | `		pLast = p;` |
|   10958 | 2129 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2130 | `	}` |
|     588 | 2131 |  |
|       - | 2132 | `/*` |
|       - | 2133 | ` * Array functions implementation.` |
|       - | 2134 | ` * Status:` |
|       - | 2135 | ` *  Stable.` |
|       - | 2136 | ` */` |
|       - | 2137 | `/*` |
|       - | 2138 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2139 | ` * Sort an array.` |
|       - | 2140 | ` * Parameters` |
|       - | 2141 | ` *  $array` |
|       - | 2142 | ` *   The input array.` |
|       - | 2143 | ` * $sort_flags` |
|       - | 2144 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2145 | ` *  Sorting type flags:` |
|       - | 2146 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2147 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2148 | ` *   SORT_STRING - compare items as strings` |
|       - | 2149 | ` * Return` |
|       - | 2150 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2151 | ` *` |
|       - | 2152 | ` */` |
|     906 | 2153 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2154 |  |
|       - | 2155 | `	ph7_hashmap *pMap;` |
|       - | 2156 | `	/* Make sure we are dealing with a valid hashmap */` |
|     908 | 2157 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2158 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2159 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2160 | `		return PH7_OK;` |
|       - | 2161 | `	}` |
|       - | 2162 | `	/* Point to the internal representation of the input hashmap */` |
|     908 | 2163 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     908 | 2164 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     908 | 2165 | `	if( pMap->nEntry > 1 ){` |
|     582 | 2166 | `		sxi32 iCmpFlags = 0;` |
|     582 | 2167 | `		if( nArg > 1 ){` |
|       - | 2168 | `			/* Extract comparison flags */` |
|       3 | 2169 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2170 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2171 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2172 | `			}` |
|       1 | 2173 | `		}` |
|       - | 2174 | `		/* Do the merge sort */` |
|     582 | 2175 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2176 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     582 | 2177 | `		HashmapSortRehash(pMap);` |
|     290 | 2178 | `	}` |
|       - | 2179 | `	/* All done,return TRUE */` |
|     908 | 2180 | `	ph7_result_bool(pCtx,1);` |
|     908 | 2181 | `	return PH7_OK;` |
|     455 | 2182 |  |
|       - | 2183 | `/*` |
|       - | 2184 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2185 | ` *  Sort an array and maintain index association.` |
|       - | 2186 | ` * Parameters` |
|       - | 2187 | ` *  $array` |
|       - | 2188 | ` *   The input array.` |
|       - | 2189 | ` * $sort_flags` |
|       - | 2190 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2191 | ` *  Sorting type flags:` |
|       - | 2192 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2193 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2194 | ` *   SORT_STRING - compare items as strings` |
|       - | 2195 | ` * Return` |
|       - | 2196 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2197 | ` */` |
|      32 | 2198 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2199 |  |
|       - | 2200 | `	ph7_hashmap *pMap;` |
|       - | 2201 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2202 | `	if( nArg < 1 ){` |
|       3 | 2203 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2204 | `			"ArgumentCountError",` |
|       - | 2205 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2206 | `			);` |
|       - | 2207 | `	}` |
|       - | 2208 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2209 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2210 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2211 | `			"TypeError",` |
|       - | 2212 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2213 | `			ph7_type_name(apArg[0])` |
|       - | 2214 | `			);` |
|       - | 2215 | `	}` |
|       - | 2216 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2217 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2218 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2219 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2220 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2221 | `		if( nArg > 1 ){` |
|       - | 2222 | `			/* Extract comparison flags */` |
|       5 | 2223 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2224 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2225 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2226 | `			}` |
|       2 | 2227 | `		}` |
|       - | 2228 | `		/* Do the merge sort */` |
|      19 | 2229 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2230 | `		/* Fix the last link broken by the merge */` |
|      45 | 2231 | `		while(pMap->pLast->pPrev){` |
|      27 | 2232 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2233 | `		}` |
|       9 | 2234 | `	}` |
|       - | 2235 | `	/* All done,return TRUE */` |
|      23 | 2236 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2237 | `	return PH7_OK;` |
|      18 | 2238 |  |
|       - | 2239 | `/*` |
|       - | 2240 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2241 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2242 | ` * Parameters` |
|       - | 2243 | ` *  $array` |
|       - | 2244 | ` *   The input array.` |
|       - | 2245 | ` * $sort_flags` |
|       - | 2246 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2247 | ` *  Sorting type flags:` |
|       - | 2248 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2249 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2250 | ` *   SORT_STRING - compare items as strings` |
|       - | 2251 | ` * Return` |
|       - | 2252 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2253 | ` */` |
|      32 | 2254 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2255 |  |
|       - | 2256 | `	ph7_hashmap *pMap;` |
|       - | 2257 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2258 | `	if( nArg < 1 ){` |
|       3 | 2259 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2260 | `			"ArgumentCountError",` |
|       - | 2261 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2262 | `			);` |
|       - | 2263 | `	}` |
|       - | 2264 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2265 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2266 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2267 | `			"TypeError",` |
|       - | 2268 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2269 | `			ph7_type_name(apArg[0])` |
|       - | 2270 | `			);` |
|       - | 2271 | `	}` |
|       - | 2272 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2273 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2274 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2275 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2276 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2277 | `		if( nArg > 1 ){` |
|       - | 2278 | `			/* Extract comparison flags */` |
|       5 | 2279 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2280 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2281 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2282 | `			}` |
|       2 | 2283 | `		}` |
|       - | 2284 | `		/* Do the merge sort */` |
|      19 | 2285 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2286 | `		/* Fix the last link broken by the merge */` |
|      35 | 2287 | `		while(pMap->pLast->pPrev){` |
|      17 | 2288 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2289 | `		}` |
|       9 | 2290 | `	}` |
|       - | 2291 | `	/* All done,return TRUE */` |
|      23 | 2292 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2293 | `	return PH7_OK;` |
|      18 | 2294 |  |
|       - | 2295 | `/*` |
|       - | 2296 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2297 | ` *  Sort an array by key.` |
|       - | 2298 | ` * Parameters` |
|       - | 2299 | ` *  $array` |
|       - | 2300 | ` *   The input array.` |
|       - | 2301 | ` * $sort_flags` |
|       - | 2302 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2303 | ` *  Sorting type flags:` |
|       - | 2304 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2305 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2306 | ` *   SORT_STRING - compare items as strings` |
|       - | 2307 | ` * Return` |
|       - | 2308 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2309 | ` */` |
|       4 | 2310 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2311 |  |
|       - | 2312 | `	ph7_hashmap *pMap;` |
|       - | 2313 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2314 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2315 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2316 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2317 | `		return PH7_OK;` |
|       - | 2318 | `	}` |
|       - | 2319 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2320 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2321 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2322 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2323 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2324 | `		if( nArg > 1 ){` |
|       - | 2325 | `			/* Extract comparison flags */` |
|     ! 0 | 2326 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2327 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2328 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2329 | `			}` |
|     ! 0 | 2330 | `		}` |
|       - | 2331 | `		/* Do the merge sort */` |
|       5 | 2332 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2333 | `		/* Fix the last link broken by the merge */` |
|      15 | 2334 | `		while(pMap->pLast->pPrev){` |
|      11 | 2335 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2336 | `		}` |
|       2 | 2337 | `	}` |
|       - | 2338 | `	/* All done,return TRUE */` |
|       5 | 2339 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2340 | `	return PH7_OK;` |
|       3 | 2341 |  |
|       - | 2342 | `/*` |
|       - | 2343 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2344 | ` *  Sort an array by key in reverse order.` |
|       - | 2345 | ` * Parameters` |
|       - | 2346 | ` *  $array` |
|       - | 2347 | ` *   The input array.` |
|       - | 2348 | ` * $sort_flags` |
|       - | 2349 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2350 | ` *  Sorting type flags:` |
|       - | 2351 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2352 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2353 | ` *   SORT_STRING - compare items as strings` |
|       - | 2354 | ` * Return` |
|       - | 2355 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2356 | ` */` |
|       2 | 2357 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2358 |  |
|       - | 2359 | `	ph7_hashmap *pMap;` |
|       - | 2360 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2361 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2362 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2363 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2364 | `		return PH7_OK;` |
|       - | 2365 | `	}` |
|       - | 2366 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2367 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2368 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2369 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2370 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2371 | `		if( nArg > 1 ){` |
|       - | 2372 | `			/* Extract comparison flags */` |
|     ! 0 | 2373 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2374 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2375 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2376 | `			}` |
|     ! 0 | 2377 | `		}` |
|       - | 2378 | `		/* Do the merge sort */` |
|       3 | 2379 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2380 | `		/* Fix the last link broken by the merge */` |
|       7 | 2381 | `		while(pMap->pLast->pPrev){` |
|       5 | 2382 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2383 | `		}` |
|       1 | 2384 | `	}` |
|       - | 2385 | `	/* All done,return TRUE */` |
|       3 | 2386 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2387 | `	return PH7_OK;` |
|       2 | 2388 |  |
|       - | 2389 | `/*` |
|       - | 2390 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2391 | ` * Sort an array in reverse order.` |
|       - | 2392 | ` * Parameters` |
|       - | 2393 | ` *  $array` |
|       - | 2394 | ` *   The input array.` |
|       - | 2395 | ` * $sort_flags` |
|       - | 2396 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2397 | ` *  Sorting type flags:` |
|       - | 2398 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2399 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2400 | ` *   SORT_STRING - compare items as strings` |
|       - | 2401 | ` * Return` |
|       - | 2402 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2403 | ` */` |
|       2 | 2404 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2405 |  |
|       - | 2406 | `	ph7_hashmap *pMap;` |
|       - | 2407 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2408 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2409 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2410 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2411 | `		return PH7_OK;` |
|       - | 2412 | `	}` |
|       - | 2413 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2414 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2415 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2416 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2417 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2418 | `		if( nArg > 1 ){` |
|       - | 2419 | `			/* Extract comparison flags */` |
|     ! 0 | 2420 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2421 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2422 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2423 | `			}` |
|     ! 0 | 2424 | `		}` |
|       - | 2425 | `		/* Do the merge sort */` |
|       3 | 2426 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2427 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2428 | `		HashmapSortRehash(pMap);` |
|       1 | 2429 | `	}` |
|       - | 2430 | `	/* All done,return TRUE */` |
|       3 | 2431 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2432 | `	return PH7_OK;` |
|       2 | 2433 |  |
|       - | 2434 | `/*` |
|       - | 2435 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2436 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2437 | ` * Parameters` |
|       - | 2438 | ` *  $array` |
|       - | 2439 | ` *   The input array.` |
|       - | 2440 | ` * $cmp_function` |
|       - | 2441 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2442 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2443 | ` *  to, or greater than the second.` |
|       - | 2444 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2445 | ` * Return` |
|       - | 2446 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2447 | ` */` |
|       2 | 2448 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2449 |  |
|       - | 2450 | `	ph7_hashmap *pMap;` |
|       - | 2451 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2452 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2453 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2454 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2455 | `		return PH7_OK;` |
|       - | 2456 | `	}` |
|       - | 2457 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2458 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2459 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2460 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2461 | `		ph7_value *pCallback = 0;` |
|       - | 2462 | `		ProcNodeCmp xCmp;` |
|       3 | 2463 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2464 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2465 | `			/* Point to the desired callback */` |
|       3 | 2466 | `			pCallback = apArg[1];` |
|       2 | 2467 | `		}else{` |
|       - | 2468 | `			/* Use the default comparison function */` |
|     ! 0 | 2469 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2470 | `		}` |
|       - | 2471 | `		/* Do the merge sort */` |
|       3 | 2472 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2473 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2474 | `		HashmapSortRehash(pMap);` |
|       1 | 2475 | `	}` |
|       - | 2476 | `	/* All done,return TRUE */` |
|       3 | 2477 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2478 | `	return PH7_OK;` |
|       2 | 2479 |  |
|       - | 2480 | `/*` |
|       - | 2481 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2482 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2483 | ` *  and maintain index association.` |
|       - | 2484 | ` * Parameters` |
|       - | 2485 | ` *  $array` |
|       - | 2486 | ` *   The input array.` |
|       - | 2487 | ` * $cmp_function` |
|       - | 2488 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2489 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2490 | ` *  to, or greater than the second.` |
|       - | 2491 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2492 | ` * Return` |
|       - | 2493 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2494 | ` */` |
|       2 | 2495 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2496 |  |
|       - | 2497 | `	ph7_hashmap *pMap;` |
|       - | 2498 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2499 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2500 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2501 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2502 | `		return PH7_OK;` |
|       - | 2503 | `	}` |
|       - | 2504 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2505 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2506 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2507 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2508 | `		ph7_value *pCallback = 0;` |
|       - | 2509 | `		ProcNodeCmp xCmp;` |
|       3 | 2510 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2511 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2512 | `			/* Point to the desired callback */` |
|       3 | 2513 | `			pCallback = apArg[1];` |
|       2 | 2514 | `		}else{` |
|       - | 2515 | `			/* Use the default comparison function */` |
|     ! 0 | 2516 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2517 | `		}` |
|       - | 2518 | `		/* Do the merge sort */` |
|       3 | 2519 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2520 | `		/* Fix the last link broken by the merge */` |
|       5 | 2521 | `		while(pMap->pLast->pPrev){` |
|       3 | 2522 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2523 | `		}` |
|       1 | 2524 | `	}` |
|       - | 2525 | `	/* All done,return TRUE */` |
|       3 | 2526 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2527 | `	return PH7_OK;` |
|       2 | 2528 |  |
|       - | 2529 | `/*` |
|       - | 2530 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2531 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2532 | ` *  function and maintain index association.` |
|       - | 2533 | ` * Parameters` |
|       - | 2534 | ` *  $array` |
|       - | 2535 | ` *   The input array.` |
|       - | 2536 | ` * $cmp_function` |
|       - | 2537 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2538 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2539 | ` *  to, or greater than the second.` |
|       - | 2540 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2541 | ` * Return` |
|       - | 2542 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2543 | ` */` |
|       2 | 2544 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2545 |  |
|       - | 2546 | `	ph7_hashmap *pMap;` |
|       - | 2547 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2548 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2549 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2550 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2551 | `		return PH7_OK;` |
|       - | 2552 | `	}` |
|       - | 2553 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2554 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2555 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2556 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2557 | `		ph7_value *pCallback = 0;` |
|       - | 2558 | `		ProcNodeCmp xCmp;` |
|       3 | 2559 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2560 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2561 | `			/* Point to the desired callback */` |
|       3 | 2562 | `			pCallback = apArg[1];` |
|       2 | 2563 | `		}else{` |
|       - | 2564 | `			/* Use the default comparison function */` |
|     ! 0 | 2565 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2566 | `		}` |
|       - | 2567 | `		/* Do the merge sort */` |
|       3 | 2568 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2569 | `		/* Fix the last link broken by the merge */` |
|       3 | 2570 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2571 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2572 | `		}` |
|       1 | 2573 | `	}` |
|       - | 2574 | `	/* All done,return TRUE */` |
|       3 | 2575 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2576 | `	return PH7_OK;` |
|       2 | 2577 |  |
|       - | 2578 | `/*` |
|       - | 2579 | ` * bool shuffle(array &$array)` |
|       - | 2580 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2581 | ` * Parameters` |
|       - | 2582 | ` *  $array` |
|       - | 2583 | ` *   The input array.` |
|       - | 2584 | ` * Return` |
|       - | 2585 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2586 | ` *` |
|       - | 2587 | ` */` |
|       2 | 2588 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2589 |  |
|       - | 2590 | `	ph7_hashmap *pMap;` |
|       - | 2591 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2592 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2593 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2594 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2595 | `		return PH7_OK;` |
|       - | 2596 | `	}` |
|       - | 2597 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2598 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2599 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2600 | `	if( pMap->nEntry > 1 ){` |
|       - | 2601 | `		/* Do the merge sort */` |
|       3 | 2602 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2603 | `		/* Fix the last link broken by the merge */` |
|      10 | 2604 | `		while(pMap->pLast->pPrev){` |
|       8 | 2605 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2606 | `		}` |
|       1 | 2607 | `	}` |
|       - | 2608 | `	/* All done,return TRUE */` |
|       3 | 2609 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2610 | `	return PH7_OK;` |
|       2 | 2611 |  |
|       - | 2612 | `/*` |
|       - | 2613 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2614 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2615 | ` * Parameters` |
|       - | 2616 | ` *  $var` |
|       - | 2617 | ` *   The array or the object.` |
|       - | 2618 | ` * $mode` |
|       - | 2619 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2620 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2621 | ` *  all the elements of a multidimensional array.` |
|       - | 2622 | ` * Return` |
|       - | 2623 | ` *  Returns the number of elements in the array.` |
|       - | 2624 | ` */` |
|     738 | 2625 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2626 |  |
|     740 | 2627 | `	int bRecursive = FALSE;` |
|     740 | 2628 | `	int bCycleDetected = FALSE;` |
|       - | 2629 | `	sxi64 iCount;` |
|     740 | 2630 | `	if( nArg < 1 ){` |
|       3 | 2631 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2632 | `			"ArgumentCountError",` |
|       - | 2633 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2634 | `			);` |
|       - | 2635 | `	}` |
|     738 | 2636 | `	if( nArg > 2 ){` |
|       4 | 2637 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2638 | `			"ArgumentCountError",` |
|       - | 2639 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2640 | `			nArg` |
|       - | 2641 | `			);` |
|       - | 2642 | `	}` |
|     736 | 2643 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2644 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2645 | `			"TypeError",` |
|       - | 2646 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       5 | 2647 | `			ph7_type_name(apArg[0])` |
|       - | 2648 | `			);` |
|       - | 2649 | `	}` |
|     726 | 2650 | `	if( nArg > 1 ){` |
|      34 | 2651 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      34 | 2652 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       5 | 2653 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2654 | `				"ValueError",` |
|       - | 2655 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2656 | `				);` |
|       - | 2657 | `		}` |
|      29 | 2658 | `		bRecursive = iMode == 1;` |
|      14 | 2659 | `	}` |
|       - | 2660 | `	/* Count */` |
|     722 | 2661 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     722 | 2662 | `	if( bCycleDetected ){` |
|       3 | 2663 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2664 | `	}` |
|     722 | 2665 | `	ph7_result_int64(pCtx,iCount);` |
|     722 | 2666 | `	return PH7_OK;` |
|     371 | 2667 |  |
|       - | 2668 | `/*` |
|       - | 2669 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2670 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2671 | ` * Parameters` |
|       - | 2672 | ` * $key` |
|       - | 2673 | ` *   Value to check.` |
|       - | 2674 | ` * $search` |
|       - | 2675 | ` *  An array with keys to check.` |
|       - | 2676 | ` * Return` |
|       - | 2677 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2678 | ` */` |
|      66 | 2679 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2680 |  |
|       - | 2681 | `	sxi32 rc;` |
|      68 | 2682 | `	if( nArg != 2 ){` |
|       - | 2683 | `		/* PHP requires exactly two arguments */` |
|      10 | 2684 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2685 | `			"ArgumentCountError",` |
|       - | 2686 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2687 | `			nArg` |
|       - | 2688 | `			);` |
|       - | 2689 | `	}` |
|       - | 2690 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 2691 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2692 | `		/* Type mismatch -> TypeError */` |
|       7 | 2693 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2694 | `			"TypeError",` |
|       - | 2695 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2696 | `			ph7_type_name(apArg[1])` |
|       - | 2697 | `			);` |
|       - | 2698 | `	}` |
|       - | 2699 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      57 | 2700 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2701 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2702 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2703 | `			"use an empty string instead"` |
|       - | 2704 | `			);` |
|      56 | 2705 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2706 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2707 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2708 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2709 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2710 | `				,rVal` |
|       - | 2711 | `				);` |
|       1 | 2712 | `		}` |
|       1 | 2713 | `	}` |
|       - | 2714 | `	/* Perform the lookup */` |
|      57 | 2715 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2716 | `	/* lookup result */` |
|      57 | 2717 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      57 | 2718 | `	return PH7_OK;` |
|      35 | 2719 |  |
|       - | 2720 | `/*` |
|       - | 2721 | ` * value array_pop(array $array)` |
|       - | 2722 | ` *   POP the last inserted element from the array.` |
|       - | 2723 | ` * Parameter` |
|       - | 2724 | ` *  The array to get the value from.` |
|       - | 2725 | ` * Return` |
|       - | 2726 | ` *  Poped value or NULL on failure.` |
|       - | 2727 | ` */` |
|      18 | 2728 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2729 |  |
|       - | 2730 | `	ph7_hashmap *pMap;` |
|       - | 2731 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      20 | 2732 | `	if( nArg != 1 ){` |
|       7 | 2733 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2734 | `			"ArgumentCountError",` |
|       - | 2735 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2736 | `			nArg` |
|       - | 2737 | `			);` |
|       - | 2738 | `	}` |
|       - | 2739 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2740 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      16 | 2741 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2742 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2743 | `			"Error",` |
|       - | 2744 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2745 | `			);` |
|       - | 2746 | `	}` |
|       - | 2747 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2748 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2749 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2750 | `			"TypeError",` |
|       - | 2751 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2752 | `			ph7_type_name(apArg[0])` |
|       - | 2753 | `			);` |
|       - | 2754 | `	}` |
|       9 | 2755 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2756 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2757 | `	if( pMap->nEntry < 1 ){` |
|       - | 2758 | `		/* Nothing to pop,return NULL */` |
|       3 | 2759 | `		ph7_result_null(pCtx);` |
|       2 | 2760 | `	}else{` |
|       7 | 2761 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2762 | `		ph7_value *pObj;` |
|       7 | 2763 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2764 | `		if( pObj ){` |
|       - | 2765 | `			/* Node value */` |
|       7 | 2766 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2767 | `			/* Unlink the node */` |
|       7 | 2768 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2769 | `		}else{` |
|     ! 0 | 2770 | `			ph7_result_null(pCtx);` |
|       - | 2771 | `		}` |
|       - | 2772 | `		/* Reset the cursor */` |
|       7 | 2773 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2774 | `	}` |
|       9 | 2775 | `	return PH7_OK;` |
|      11 | 2776 |  |
|       - | 2777 | `/*` |
|       - | 2778 | ` * int array_push($array,$var,...)` |
|       - | 2779 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2780 | ` * Parameters` |
|       - | 2781 | ` *  array` |
|       - | 2782 | ` *    The input array.` |
|       - | 2783 | ` *  var` |
|       - | 2784 | ` *   On or more value to push.` |
|       - | 2785 | ` * Return` |
|       - | 2786 | ` *  New array count (including old items).` |
|       - | 2787 | ` */` |
|      22 | 2788 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2789 |  |
|       - | 2790 | `	ph7_hashmap *pMap;` |
|       - | 2791 | `	sxi32 rc;` |
|       - | 2792 | `	int i;` |
|      24 | 2793 | `	if( nArg < 1 ){` |
|       4 | 2794 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2795 | `			"ArgumentCountError",` |
|       - | 2796 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2797 | `			nArg` |
|       - | 2798 | `			);` |
|       - | 2799 | `	}` |
|       - | 2800 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2801 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      22 | 2802 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2803 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2804 | `			"Error",` |
|       - | 2805 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2806 | `			);` |
|       - | 2807 | `	}` |
|       - | 2808 | `	/* Make sure we are dealing with a valid hashmap */` |
|      18 | 2809 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2810 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2811 | `			"TypeError",` |
|       - | 2812 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2813 | `			ph7_type_name(apArg[0])` |
|       - | 2814 | `			);` |
|       - | 2815 | `	}` |
|       - | 2816 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2817 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2818 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2819 | `	/* Start pushing given values */` |
|      31 | 2820 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      17 | 2821 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      17 | 2822 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2823 | `			break;` |
|       - | 2824 | `		}` |
|       9 | 2825 | `	}` |
|       - | 2826 | `	/* Return the new count */` |
|      15 | 2827 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 2828 | `	return PH7_OK;` |
|      13 | 2829 |  |
|       - | 2830 | `/*` |
|       - | 2831 | ` * value array_shift(array $array)` |
|       - | 2832 | ` *   Shift an element off the beginning of array.` |
|       - | 2833 | ` * Parameter` |
|       - | 2834 | ` *  The array to get the value from.` |
|       - | 2835 | ` * Return` |
|       - | 2836 | ` *  Shifted value or NULL on failure.` |
|       - | 2837 | ` */` |
|      38 | 2838 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2839 |  |
|       - | 2840 | `	ph7_hashmap *pMap;` |
|       - | 2841 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      40 | 2842 | `	if( nArg != 1 ){` |
|       7 | 2843 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2844 | `			"ArgumentCountError",` |
|       - | 2845 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2846 | `			nArg` |
|       - | 2847 | `			);` |
|       - | 2848 | `	}` |
|       - | 2849 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      36 | 2850 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2851 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2852 | `			"Error",` |
|       - | 2853 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2854 | `			);` |
|       - | 2855 | `	}` |
|       - | 2856 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 2857 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2858 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2859 | `			"TypeError",` |
|       - | 2860 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2861 | `			ph7_type_name(apArg[0])` |
|       - | 2862 | `			);` |
|       - | 2863 | `	}` |
|       - | 2864 | `	/* Point to the internal representation of the hashmap */` |
|      30 | 2865 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      30 | 2866 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      30 | 2867 | `	if( pMap->nEntry < 1 ){` |
|       - | 2868 | `		/* Empty hashmap,return NULL */` |
|       3 | 2869 | `		ph7_result_null(pCtx);` |
|       2 | 2870 | `	}else{` |
|      28 | 2871 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2872 | `		ph7_value *pObj;` |
|       - | 2873 | `		sxu32 n;` |
|      28 | 2874 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      28 | 2875 | `		if( pObj ){` |
|       - | 2876 | `			/* Node value */` |
|      28 | 2877 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2878 | `			/* Unlink the first node */` |
|      28 | 2879 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      15 | 2880 | `		}else{` |
|     ! 0 | 2881 | `			ph7_result_null(pCtx);` |
|       - | 2882 | `		}` |
|       - | 2883 | `		/* Rehash all int keys */` |
|      28 | 2884 | `		n = pMap->nEntry;` |
|      28 | 2885 | `		pEntry = pMap->pFirst;` |
|      28 | 2886 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 2887 | `		for(;;){` |
|      82 | 2888 | `			if( n < 1 ){` |
|      28 | 2889 | `				break;` |
|       - | 2890 | `			}` |
|      56 | 2891 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      56 | 2892 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 2893 | `			}` |
|       - | 2894 | `			/* Point to the next entry */` |
|      56 | 2895 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      56 | 2896 | `			n--;` |
|       2 | 2897 | `		}` |
|       - | 2898 | `		/* Reset the cursor */` |
|      28 | 2899 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2900 | `	}` |
|      30 | 2901 | `	return PH7_OK;` |
|      21 | 2902 |  |
|       - | 2903 | `/*` |
|       - | 2904 | ` * Extract the node cursor value.` |
|       - | 2905 | ` */` |
|      24 | 2906 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 2907 |  |
|      25 | 2908 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 2909 | `	ph7_value *pVal;` |
|      25 | 2910 | `	if( pCur == 0 ){` |
|       - | 2911 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 2912 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2913 | `		return PH7_OK;` |
|       - | 2914 | `	}` |
|      25 | 2915 | `	if( iDirection != 0 ){` |
|       9 | 2916 | `		if( iDirection > 0 ){` |
|       - | 2917 | `			/* Point to the next entry */` |
|       7 | 2918 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 2919 | `			pCur = pMap->pCur;` |
|       4 | 2920 | `		}else{` |
|       - | 2921 | `			/* Point to the previous entry */` |
|       3 | 2922 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 2923 | `			pCur = pMap->pCur;` |
|       - | 2924 | `		}` |
|       9 | 2925 | `		if( pCur == 0 ){` |
|       - | 2926 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 2927 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2928 | `			return PH7_OK;` |
|       - | 2929 | `		}` |
|       4 | 2930 | `	}` |
|       - | 2931 | `	/* Point to the desired element */` |
|      25 | 2932 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 2933 | `	if( pVal ){` |
|      25 | 2934 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 2935 | `	}else{` |
|     ! 0 | 2936 | `		ph7_result_bool(pCtx,0);` |
|       - | 2937 | `	}` |
|      25 | 2938 | `	return PH7_OK;` |
|      13 | 2939 |  |
|       - | 2940 | `/*` |
|       - | 2941 | ` * value current(array $array)` |
|       - | 2942 | ` *  Return the current element in an array.` |
|       - | 2943 | ` * Parameter` |
|       - | 2944 | ` *  $input: The input array.` |
|       - | 2945 | ` * Return` |
|       - | 2946 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 2947 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2948 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2949 | ` *  is empty, current() returns FALSE.` |
|       - | 2950 | ` */` |
|      10 | 2951 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2952 |  |
|      11 | 2953 | `	if( nArg < 1 ){` |
|       - | 2954 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2955 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2956 | `		return PH7_OK;` |
|       - | 2957 | `	}` |
|       - | 2958 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 2959 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2960 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2961 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2962 | `		return PH7_OK;` |
|       - | 2963 | `	}` |
|      11 | 2964 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 2965 | `	return PH7_OK;` |
|       6 | 2966 |  |
|       - | 2967 | `/*` |
|       - | 2968 | ` * value next(array $input)` |
|       - | 2969 | ` *  Advance the internal array pointer of an array.` |
|       - | 2970 | ` * Parameter` |
|       - | 2971 | ` *  $input: The input array.` |
|       - | 2972 | ` * Return` |
|       - | 2973 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 2974 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 2975 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 2976 | ` */` |
|       6 | 2977 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2978 |  |
|       7 | 2979 | `	if( nArg < 1 ){` |
|       - | 2980 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2981 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2982 | `		return PH7_OK;` |
|       - | 2983 | `	}` |
|       - | 2984 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 2985 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2986 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2987 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2988 | `		return PH7_OK;` |
|       - | 2989 | `	}` |
|       7 | 2990 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 2991 | `	return PH7_OK;` |
|       4 | 2992 |  |
|       - | 2993 | `/*` |
|       - | 2994 | ` * value prev(array $input)` |
|       - | 2995 | ` *  Rewind the internal array pointer.` |
|       - | 2996 | ` * Parameter` |
|       - | 2997 | ` *  $input: The input array.` |
|       - | 2998 | ` * Return` |
|       - | 2999 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3000 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3001 | ` *  elements.` |
|       - | 3002 | ` */` |
|       2 | 3003 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3004 |  |
|       3 | 3005 | `	if( nArg < 1 ){` |
|       - | 3006 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3007 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3008 | `		return PH7_OK;` |
|       - | 3009 | `	}` |
|       - | 3010 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3011 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3012 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3013 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3014 | `		return PH7_OK;` |
|       - | 3015 | `	}` |
|       3 | 3016 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3017 | `	return PH7_OK;` |
|       2 | 3018 |  |
|       - | 3019 | `/*` |
|       - | 3020 | ` * value end(array $input)` |
|       - | 3021 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3022 | ` * Parameter` |
|       - | 3023 | ` *  $input: The input array.` |
|       - | 3024 | ` * Return` |
|       - | 3025 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3026 | ` */` |
|       2 | 3027 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3028 |  |
|       - | 3029 | `	ph7_hashmap *pMap;` |
|       3 | 3030 | `	if( nArg < 1 ){` |
|       - | 3031 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3032 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3033 | `		return PH7_OK;` |
|       - | 3034 | `	}` |
|       - | 3035 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3036 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3037 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3038 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3039 | `		return PH7_OK;` |
|       - | 3040 | `	}` |
|       - | 3041 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3042 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3043 | `	/* Point to the last node */` |
|       3 | 3044 | `	pMap->pCur = pMap->pLast;` |
|       - | 3045 | `	/* Return the last node value */` |
|       3 | 3046 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3047 | `	return PH7_OK;` |
|       2 | 3048 |  |
|       - | 3049 | `/*` |
|       - | 3050 | ` * value reset(array $array )` |
|       - | 3051 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3052 | ` * Parameter` |
|       - | 3053 | ` *  $input: The input array.` |
|       - | 3054 | ` * Return` |
|       - | 3055 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3056 | ` */` |
|       4 | 3057 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3058 |  |
|       - | 3059 | `	ph7_hashmap *pMap;` |
|       5 | 3060 | `	if( nArg < 1 ){` |
|       - | 3061 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3062 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3063 | `		return PH7_OK;` |
|       - | 3064 | `	}` |
|       - | 3065 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3066 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3067 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3068 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3069 | `		return PH7_OK;` |
|       - | 3070 | `	}` |
|       - | 3071 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3072 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3073 | `	/* Point to the first node */` |
|       5 | 3074 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3075 | `	/* Return the last node value if available */` |
|       5 | 3076 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3077 | `	return PH7_OK;` |
|       3 | 3078 |  |
|       - | 3079 | `/*` |
|       - | 3080 | ` * value key(array $array)` |
|       - | 3081 | ` *   Fetch a key from an array` |
|       - | 3082 | ` * Parameter` |
|       - | 3083 | ` *  $input` |
|       - | 3084 | ` *   The input array.` |
|       - | 3085 | ` * Return` |
|       - | 3086 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3087 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3088 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3089 | ` *  is empty, key() returns NULL.` |
|       - | 3090 | ` */` |
|       4 | 3091 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3092 |  |
|       - | 3093 | `	ph7_hashmap_node *pCur;` |
|       - | 3094 | `	ph7_hashmap *pMap;` |
|       5 | 3095 | `	if( nArg < 1 ){` |
|       - | 3096 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3097 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3098 | `		return PH7_OK;` |
|       - | 3099 | `	}` |
|       - | 3100 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3101 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3102 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3103 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3104 | `		return PH7_OK;` |
|       - | 3105 | `	}` |
|       5 | 3106 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3107 | `	pCur = pMap->pCur;` |
|       5 | 3108 | `	if( pCur == 0 ){` |
|       - | 3109 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3110 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3111 | `		return PH7_OK;` |
|       - | 3112 | `	}` |
|       5 | 3113 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3114 | `		/* Key is integer */` |
|     ! 0 | 3115 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3116 | `	}else{` |
|       - | 3117 | `		/* Key is blob */` |
|       7 | 3118 | `		ph7_result_string(pCtx,` |
|       4 | 3119 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3120 | `	}` |
|       5 | 3121 | `	return PH7_OK;` |
|       3 | 3122 |  |
|       - | 3123 | `/*` |
|       - | 3124 | ` * array each(array $input)` |
|       - | 3125 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3126 | ` * Parameter` |
|       - | 3127 | ` *  $input` |
|       - | 3128 | ` *    The input array.` |
|       - | 3129 | ` * Return` |
|       - | 3130 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3131 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3132 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3133 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3134 | ` *  each() returns FALSE.` |
|       - | 3135 | ` */` |
|      22 | 3136 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3137 |  |
|       - | 3138 | `	ph7_hashmap_node *pCur;` |
|       - | 3139 | `	ph7_hashmap *pMap;` |
|       - | 3140 | `	ph7_value *pArray;` |
|       - | 3141 | `	ph7_value *pVal;` |
|       - | 3142 | `	ph7_value sKey;` |
|      23 | 3143 | `	if( nArg < 1 ){` |
|       - | 3144 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3145 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3146 | `		return PH7_OK;` |
|       - | 3147 | `	}` |
|       - | 3148 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3149 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3150 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3151 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3152 | `		return PH7_OK;` |
|       - | 3153 | `	}` |
|       - | 3154 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3155 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3156 | `	if( pMap->pCur == 0 ){` |
|       - | 3157 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3158 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3159 | `		return PH7_OK;` |
|       - | 3160 | `	}` |
|      15 | 3161 | `	pCur = pMap->pCur;` |
|       - | 3162 | `	/* Create a new array */` |
|      15 | 3163 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3164 | `	if( pArray == 0 ){` |
|     ! 0 | 3165 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3166 | `		return PH7_OK;` |
|       - | 3167 | `	}` |
|      15 | 3168 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3169 | `	/* Insert the current value */` |
|      15 | 3170 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3171 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3172 | `	/* Make the key */` |
|      15 | 3173 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3174 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3175 | `	}else{` |
|       9 | 3176 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3177 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3178 | `	}` |
|       - | 3179 | `	/* Insert the current key */` |
|      15 | 3180 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3181 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3182 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3183 | `	/* Advance the cursor */` |
|      15 | 3184 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3185 | `	/* Return the current entry */` |
|      15 | 3186 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3187 | `	return PH7_OK;` |
|      12 | 3188 |  |
|       - | 3189 | `/*` |
|       - | 3190 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3191 | ` *  Create an array containing a range of elements` |
|       - | 3192 | ` * Parameter` |
|       - | 3193 | ` *  start` |
|       - | 3194 | ` *   First value of the sequence.` |
|       - | 3195 | ` *  limit` |
|       - | 3196 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3197 | ` *  step` |
|       - | 3198 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3199 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3200 | ` * Return` |
|       - | 3201 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3202 | ` * NOTE:` |
|       - | 3203 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3204 | ` */` |
|       2 | 3205 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3206 |  |
|       - | 3207 | `	ph7_value *pValue,*pArray;` |
|       - | 3208 | `	sxi64 iOfft,iLimit;` |
|       3 | 3209 | `	int iStep = 1;` |
|       - | 3210 |  |
|       3 | 3211 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3212 | `	if( nArg > 0 ){` |
|       - | 3213 | `		/* Extract the offset */` |
|       3 | 3214 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3215 | `		if( nArg > 1 ){` |
|       - | 3216 | `			/* Extract the limit */` |
|       3 | 3217 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3218 | `			if( nArg > 2 ){` |
|       - | 3219 | `				/* Extract the increment */` |
|       3 | 3220 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3221 | `				if( iStep < 1 ){` |
|       - | 3222 | `					/* Only positive number are allowed */` |
|       3 | 3223 | `					iStep = 1;` |
|       1 | 3224 | `				}` |
|       1 | 3225 | `			}` |
|       1 | 3226 | `		}` |
|       1 | 3227 | `	}` |
|       - | 3228 | `	/* Element container */` |
|       3 | 3229 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3230 | `	/* Create the new array */` |
|       3 | 3231 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3232 | `	if( pArray == 0 ){` |
|     ! 0 | 3233 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3234 | `		return PH7_OK;` |
|       - | 3235 | `	}` |
|       - | 3236 | `	/* Start filling */` |
|       3 | 3237 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3238 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3239 | `		/* Perform the insertion */` |
|     ! 0 | 3240 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3241 | `		/* Increment */` |
|     ! 0 | 3242 | `		iOfft += iStep;` |
|     ! 0 | 3243 | `	}` |
|       - | 3244 | `	/* Return the new array */` |
|       3 | 3245 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3246 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3247 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3248 | `	 */` |
|       3 | 3249 | `	return PH7_OK;` |
|       2 | 3250 |  |
|       - | 3251 | `/*` |
|       - | 3252 | ` * array array_values(array $array)` |
|       - | 3253 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3254 | ` * Parameters` |
|       - | 3255 | ` *  $array` |
|       - | 3256 | ` *   The input array.` |
|       - | 3257 | ` * Return` |
|       - | 3258 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3259 | ` */` |
|      30 | 3260 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3261 |  |
|       - | 3262 | `	ph7_hashmap_node *pNode;` |
|       - | 3263 | `	ph7_hashmap *pMap;` |
|       - | 3264 | `	ph7_value *pArray;` |
|       - | 3265 | `	ph7_value *pObj;` |
|       - | 3266 | `	sxu32 n;` |
|      32 | 3267 | `	if( nArg != 1 ){` |
|       - | 3268 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3269 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3270 | `			"ArgumentCountError",` |
|       - | 3271 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3272 | `			nArg` |
|       - | 3273 | `			);` |
|       - | 3274 | `	}` |
|       - | 3275 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3276 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3277 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3278 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3279 | `			"TypeError",` |
|       - | 3280 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3281 | `			ph7_type_name(apArg[0])` |
|       - | 3282 | `			);` |
|       - | 3283 | `	}` |
|       - | 3284 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3285 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3286 | `	/* Create a new array */` |
|      25 | 3287 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3288 | `	if( pArray == 0 ){` |
|     ! 0 | 3289 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3290 | `		return PH7_OK;` |
|       - | 3291 | `	}` |
|       - | 3292 | `	/* Perform the requested operation */` |
|      25 | 3293 | `	pNode = pMap->pFirst;` |
|      83 | 3294 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3295 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3296 | `		if( pObj ){` |
|       - | 3297 | `			/* perform the insertion */` |
|      59 | 3298 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3299 | `		}` |
|       - | 3300 | `		/* Point to the next entry */` |
|      59 | 3301 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3302 | `	}` |
|       - | 3303 | `	/* return the new array */` |
|      25 | 3304 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3305 | `	return PH7_OK;` |
|      17 | 3306 |  |
|       - | 3307 | `/*` |
|       - | 3308 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3309 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3310 | ` * Parameters` |
|       - | 3311 | ` *  $input` |
|       - | 3312 | ` *   An array containing keys to return.` |
|       - | 3313 | ` * $search_value` |
|       - | 3314 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3315 | ` * $strict` |
|       - | 3316 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3317 | ` * Return` |
|       - | 3318 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3319 | ` */` |
|     120 | 3320 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3321 |  |
|       - | 3322 | `	ph7_hashmap_node *pNode;` |
|       - | 3323 | `	ph7_hashmap *pMap;` |
|       - | 3324 | `	ph7_value *pArray;` |
|       - | 3325 | `	ph7_value sObj;` |
|       - | 3326 | `	ph7_value sVal;` |
|       - | 3327 | `	SyString sKey;` |
|       - | 3328 | `	int bStrict;` |
|       - | 3329 | `	sxi32 rc;` |
|       - | 3330 | `	sxu32 n;` |
|     122 | 3331 | `	if( nArg < 1 ){` |
|       - | 3332 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3333 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3334 | `			"ArgumentCountError",` |
|       - | 3335 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3336 | `			);` |
|       - | 3337 | `	}` |
|       - | 3338 | `	/* Make sure we are dealing with a valid hashmap */` |
|     120 | 3339 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3340 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3341 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3342 | `			"TypeError",` |
|       - | 3343 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3344 | `			ph7_type_name(apArg[0])` |
|       - | 3345 | `			);` |
|       - | 3346 | `	}` |
|       - | 3347 | `	/* Point to the internal representation of the input hashmap */` |
|     118 | 3348 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3349 | `	/* Create a new array */` |
|     118 | 3350 | `	pArray = ph7_context_new_array(pCtx);` |
|     118 | 3351 | `	if( pArray == 0 ){` |
|     ! 0 | 3352 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3353 | `		return PH7_OK;` |
|       - | 3354 | `	}` |
|     118 | 3355 | `	bStrict = FALSE;` |
|     118 | 3356 | `	if( nArg > 2 ){` |
|       - | 3357 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3358 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3359 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3360 | `				"TypeError",` |
|       - | 3361 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3362 | `				ph7_type_name(apArg[2])` |
|       - | 3363 | `				);` |
|       - | 3364 | `		}` |
|       5 | 3365 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3366 | `	}` |
|       - | 3367 | `	/* Perform the requested operation */` |
|     115 | 3368 | `	pNode = pMap->pFirst;` |
|     115 | 3369 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     553 | 3370 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     439 | 3371 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     117 | 3372 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      59 | 3373 | `		}else{` |
|     323 | 3374 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3375 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3376 | `		}` |
|     439 | 3377 | `		rc = 0;` |
|     439 | 3378 | `		if( nArg > 1 ){` |
|      31 | 3379 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3380 | `			if( pValue ){` |
|      31 | 3381 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3382 | `				/* Filter key */` |
|      31 | 3383 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3384 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3385 | `			}` |
|      15 | 3386 | `		}` |
|     439 | 3387 | `		if( rc == 0 ){` |
|       - | 3388 | `			/* Perform the insertion */` |
|     421 | 3389 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     210 | 3390 | `		}` |
|     439 | 3391 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3392 | `		/* Point to the next entry */` |
|     439 | 3393 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     220 | 3394 | `	}` |
|       - | 3395 | `	/* return the new array */` |
|     115 | 3396 | `	ph7_result_value(pCtx,pArray);` |
|     115 | 3397 | `	return PH7_OK;` |
|      62 | 3398 |  |
|       - | 3399 | `/*` |
|       - | 3400 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3401 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3402 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3403 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3404 | ` * Parameters` |
|       - | 3405 | ` *  $arr1` |
|       - | 3406 | ` *   First array` |
|       - | 3407 | ` *  $arr2` |
|       - | 3408 | ` *   Second array` |
|       - | 3409 | ` * Return` |
|       - | 3410 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3411 | ` * Note` |
|       - | 3412 | ` *  This function is a symisc eXtension.` |
|       - | 3413 | ` */` |
|       4 | 3414 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3415 |  |
|       - | 3416 | `	ph7_hashmap *p1,*p2;` |
|       - | 3417 | `	int rc;` |
|       5 | 3418 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3419 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3420 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3421 | `		return PH7_OK;` |
|       - | 3422 | `	}` |
|       - | 3423 | `	/* Point to the hashmaps */` |
|       5 | 3424 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3425 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3426 | `	rc = (p1 == p2);` |
|       - | 3427 | `	/* Same instance? */` |
|       5 | 3428 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3429 | `	return PH7_OK;` |
|       3 | 3430 |  |
|       - | 3431 | `/*` |
|       - | 3432 | ` * array array_merge(array ...$arrays)` |
|       - | 3433 | ` *  Merge one or more arrays.` |
|       - | 3434 | ` * Parameters` |
|       - | 3435 | ` *  ...$arrays` |
|       - | 3436 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3437 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3438 | ` * Return` |
|       - | 3439 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3440 | ` *  with no arguments.` |
|       - | 3441 | ` */` |
|     932 | 3442 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3443 |  |
|       - | 3444 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3445 | `	ph7_value *pArray;` |
|       - | 3446 | `	int i;` |
|       - | 3447 | `	/* Create a new array */` |
|     934 | 3448 | `	pArray = ph7_context_new_array(pCtx);` |
|     934 | 3449 | `	if( pArray == 0 ){` |
|     ! 0 | 3450 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3451 | `		return PH7_OK;` |
|       - | 3452 | `	}` |
|       - | 3453 | `	/* Point to the internal representation of the hashmap */` |
|     934 | 3454 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3455 | `	/* Start merging */` |
|    2788 | 3456 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3457 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1860 | 3458 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3459 | `			/* Type mismatch -> TypeError */` |
|       7 | 3460 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3461 | `				"TypeError",` |
|       - | 3462 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3463 | `				i + 1,` |
|       4 | 3464 | `				ph7_type_name(apArg[i])` |
|       - | 3465 | `				);` |
|     ! 0 | 3466 | `		}else{` |
|    1856 | 3467 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3468 | `			/* Merge the two hashmaps */` |
|    1856 | 3469 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3470 | `		}` |
|     929 | 3471 | `	}` |
|       - | 3472 | `	/* Return the freshly created array */` |
|     930 | 3473 | `	ph7_result_value(pCtx,pArray);` |
|     930 | 3474 | `	return PH7_OK;` |
|     468 | 3475 |  |
|       - | 3476 | `/*` |
|       - | 3477 | ` * array array_copy(array $source)` |
|       - | 3478 | ` *  Make a blind copy of the target array.` |
|       - | 3479 | ` * Parameters` |
|       - | 3480 | ` *  $source` |
|       - | 3481 | ` *   Target array` |
|       - | 3482 | ` * Return` |
|       - | 3483 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3484 | ` * Note` |
|       - | 3485 | ` *  This function is a symisc eXtension.` |
|       - | 3486 | ` */` |
|      16 | 3487 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3488 |  |
|       - | 3489 | `	ph7_hashmap *pMap;` |
|       - | 3490 | `	ph7_value *pArray;` |
|      17 | 3491 | `	if( nArg < 1 ){` |
|       - | 3492 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3493 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3494 | `		return PH7_OK;` |
|       - | 3495 | `	}` |
|       - | 3496 | `	/* Create a new array */` |
|      17 | 3497 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3498 | `	if( pArray == 0 ){` |
|     ! 0 | 3499 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3500 | `		return PH7_OK;` |
|       - | 3501 | `	}` |
|       - | 3502 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3503 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3504 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3505 | `		/* Point to the internal representation of the source */` |
|      17 | 3506 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3507 | `		/* Perform the copy */` |
|      17 | 3508 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3509 | `	}else{` |
|       - | 3510 | `		/* Simple insertion */` |
|     ! 0 | 3511 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3512 | `	}` |
|       - | 3513 | `	/* Return the duplicated array */` |
|      17 | 3514 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3515 | `	return PH7_OK;` |
|       9 | 3516 |  |
|       - | 3517 | `/*` |
|       - | 3518 | ` * bool array_erase(array $source)` |
|       - | 3519 | ` *  Remove all elements from a given array.` |
|       - | 3520 | ` * Parameters` |
|       - | 3521 | ` *  $source` |
|       - | 3522 | ` *   Target array` |
|       - | 3523 | ` * Return` |
|       - | 3524 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3525 | ` * Note` |
|       - | 3526 | ` *  This function is a symisc eXtension.` |
|       - | 3527 | ` */` |
|      16 | 3528 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3529 |  |
|       - | 3530 | `	ph7_hashmap *pMap;` |
|      17 | 3531 | `	if( nArg < 1 ){` |
|       - | 3532 | `		/* Missing arguments */` |
|     ! 0 | 3533 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3534 | `		return PH7_OK;` |
|       - | 3535 | `	}` |
|       - | 3536 | `	/* Point to the target hashmap */` |
|      17 | 3537 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3538 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3539 | `	/* Erase */` |
|      17 | 3540 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3541 | `	return PH7_OK;` |
|       9 | 3542 |  |
|       - | 3543 | `/*` |
|       - | 3544 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3545 | ` *  Extract a slice of the array.` |
|       - | 3546 | ` * Parameters` |
|       - | 3547 | ` *  $array` |
|       - | 3548 | ` *    The input array.` |
|       - | 3549 | ` * $offset` |
|       - | 3550 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3551 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3552 | ` * $length (optional, nullable)` |
|       - | 3553 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3554 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3555 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3556 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3557 | ` * $preserve_keys (optional)` |
|       - | 3558 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3559 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3560 | ` * Return` |
|       - | 3561 | ` *   The new slice.` |
|       - | 3562 | ` */` |
|      46 | 3563 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3564 |  |
|       - | 3565 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3566 | `	ph7_hashmap_node *pCur;` |
|       - | 3567 | `	ph7_value *pArray;` |
|       - | 3568 | `	int iLength,iOfft;` |
|       - | 3569 | `	int bPreserve;` |
|       - | 3570 | `	sxi32 rc;` |
|      48 | 3571 | `	if( nArg < 2 ){` |
|       7 | 3572 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3573 | `			"ArgumentCountError",` |
|       - | 3574 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3575 | `			nArg` |
|       - | 3576 | `			);` |
|       - | 3577 | `	}` |
|      44 | 3578 | `	if( nArg > 4 ){` |
|       4 | 3579 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3580 | `			"ArgumentCountError",` |
|       - | 3581 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3582 | `			nArg` |
|       - | 3583 | `			);` |
|       - | 3584 | `	}` |
|      42 | 3585 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3586 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3587 | `			"TypeError",` |
|       - | 3588 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3589 | `			ph7_type_name(apArg[0])` |
|       - | 3590 | `			);` |
|       - | 3591 | `	}` |
|       - | 3592 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3593 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3594 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3595 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3596 | `			"TypeError",` |
|       - | 3597 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3598 | `			ph7_type_name(apArg[1])` |
|       - | 3599 | `			);` |
|       - | 3600 | `	}` |
|       - | 3601 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3602 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3603 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3604 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3605 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3606 | `				"TypeError",` |
|       - | 3607 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3608 | `				ph7_type_name(apArg[2])` |
|       - | 3609 | `				);` |
|       - | 3610 | `		}` |
|       8 | 3611 | `	}` |
|       - | 3612 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3613 | `	if( nArg > 3 ){` |
|      10 | 3614 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3615 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3616 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3617 | `				"TypeError",` |
|       - | 3618 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3619 | `				ph7_type_name(apArg[3])` |
|       - | 3620 | `				);` |
|       - | 3621 | `		}` |
|       2 | 3622 | `	}` |
|       - | 3623 | `	/* Point the internal representation of the target array */` |
|      33 | 3624 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3625 | `	bPreserve = FALSE;` |
|       - | 3626 | `	/* Get the offset */` |
|      33 | 3627 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3628 | `	if( iOfft < 0 ){` |
|       5 | 3629 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3630 | `		if( iOfft < 0 ){` |
|       3 | 3631 | `			iOfft = 0;` |
|       1 | 3632 | `		}` |
|       2 | 3633 | `	}` |
|      33 | 3634 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3635 | `		/* Offset past end of array, return empty array */` |
|       5 | 3636 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3637 | `		if( pArray == 0 ){` |
|     ! 0 | 3638 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3639 | `			return PH7_OK;` |
|       - | 3640 | `		}` |
|       5 | 3641 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3642 | `		return PH7_OK;` |
|       - | 3643 | `	}` |
|       - | 3644 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3645 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3646 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3647 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3648 | `		if( iLength < 0 ){` |
|       5 | 3649 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3650 | `		}` |
|      15 | 3651 | `		if( iLength < 0 ){` |
|       3 | 3652 | `			iLength = 0;` |
|       1 | 3653 | `		}` |
|      15 | 3654 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3655 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3656 | `		}` |
|       7 | 3657 | `	}` |
|      29 | 3658 | `	if( nArg > 3 ){` |
|       5 | 3659 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3660 | `	}` |
|       - | 3661 | `	/* Create a new array */` |
|      29 | 3662 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3663 | `	if( pArray == 0 ){` |
|     ! 0 | 3664 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3665 | `		return PH7_OK;` |
|       - | 3666 | `	}` |
|      29 | 3667 | `	if( iLength < 1 ){` |
|       - | 3668 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3669 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3670 | `		return PH7_OK;` |
|       - | 3671 | `	}` |
|       - | 3672 | `	/* Point to the desired entry */` |
|      25 | 3673 | `	pCur = pSrc->pFirst;` |
|      24 | 3674 | `	for(;;){` |
|      49 | 3675 | `		if( iOfft < 1 ){` |
|      25 | 3676 | `			break;` |
|       - | 3677 | `		}` |
|       - | 3678 | `		/* Point to the next entry */` |
|      25 | 3679 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3680 | `		iOfft--;` |
|       1 | 3681 | `	}` |
|       - | 3682 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3683 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3684 | `	for(;;){` |
|      79 | 3685 | `		if( iLength < 1 ){` |
|      25 | 3686 | `			break;` |
|       - | 3687 | `		}` |
|       - | 3688 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3689 | `		{` |
|      55 | 3690 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3691 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3692 | `		}` |
|      55 | 3693 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3694 | `			break;` |
|       - | 3695 | `		}` |
|       - | 3696 | `		/* Point to the next entry */` |
|      55 | 3697 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3698 | `		iLength--;` |
|       1 | 3699 | `	}` |
|       - | 3700 | `	/* Return the freshly created array */` |
|      25 | 3701 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3702 | `	return PH7_OK;` |
|      25 | 3703 |  |
|       - | 3704 | `/*` |
|       - | 3705 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3706 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3707 | ` * beginning (becomes the new pFirst).` |
|       - | 3708 | ` */` |
|      30 | 3709 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3710 |  |
|       - | 3711 | `	ph7_hashmap_node *pNode;` |
|       - | 3712 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3713 | `	pNode = pMap->pLast;` |
|      31 | 3714 | `	if( pNode == 0 ){` |
|     ! 0 | 3715 | `		return;` |
|       - | 3716 | `	}` |
|      31 | 3717 | `	if( pNode->pNext == 0 ){` |
|       - | 3718 | `		/* Only node in the list, nothing to move */` |
|       5 | 3719 | `		return;` |
|       - | 3720 | `	}` |
|      27 | 3721 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3722 | `		/* Already in the correct position */` |
|       9 | 3723 | `		return;` |
|       - | 3724 | `	}` |
|       - | 3725 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3726 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3727 | `	pMap->pLast->pPrev = 0;` |
|       - | 3728 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3729 | `	if( pAfter == 0 ){` |
|       - | 3730 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3731 | `		pNode->pNext = 0;` |
|       3 | 3732 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3733 | `		if( pMap->pFirst ){` |
|       3 | 3734 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3735 | `		}` |
|       3 | 3736 | `		pMap->pFirst = pNode;` |
|       2 | 3737 | `	}else{` |
|      17 | 3738 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3739 | `		pNode->pPrev = pOldNext;` |
|      17 | 3740 | `		pNode->pNext = pAfter;` |
|      17 | 3741 | `		pAfter->pPrev = pNode;` |
|      17 | 3742 | `		if( pOldNext ){` |
|      17 | 3743 | `			pOldNext->pNext = pNode;` |
|       9 | 3744 | `		}else{` |
|     ! 0 | 3745 | `			pMap->pLast = pNode;` |
|       - | 3746 | `		}` |
|       - | 3747 | `	}` |
|      16 | 3748 |  |
|       - | 3749 | `/*` |
|       - | 3750 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3751 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3752 | ` * Parameters` |
|       - | 3753 | ` *  $array` |
|       - | 3754 | ` *    The input array.` |
|       - | 3755 | ` *  $offset` |
|       - | 3756 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3757 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3758 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3759 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3760 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3761 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3762 | ` *  $length (optional)` |
|       - | 3763 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3764 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3765 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3766 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3767 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3768 | ` *  $replacement (optional)` |
|       - | 3769 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3770 | ` *    with elements from this array.` |
|       - | 3771 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3772 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3773 | ` *    offset.` |
|       - | 3774 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3775 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3776 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3777 | ` * Return` |
|       - | 3778 | ` *   A new array consisting of the extracted elements.` |
|       - | 3779 | ` */` |
|      54 | 3780 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3781 |  |
|       - | 3782 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3783 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3784 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3785 | `	int iLength,iOfft,i;` |
|       - | 3786 | `	sxi32 rc;` |
|      56 | 3787 | `	if( nArg < 2 ){` |
|       7 | 3788 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3789 | `			"ArgumentCountError",` |
|       - | 3790 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3791 | `			nArg` |
|       - | 3792 | `			);` |
|       - | 3793 | `	}` |
|      52 | 3794 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3795 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3796 | `			"TypeError",` |
|       - | 3797 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3798 | `			ph7_type_name(apArg[0])` |
|       - | 3799 | `			);` |
|       - | 3800 | `	}` |
|       - | 3801 | `	/* Point to the internal representation of the target array */` |
|      49 | 3802 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3803 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3804 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3805 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3806 | `	if( iOfft < 0 ){` |
|       7 | 3807 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3808 | `		if( iOfft < 0 ){` |
|       3 | 3809 | `			iOfft = 0;` |
|       2 | 3810 | `		}` |
|      46 | 3811 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3812 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3813 | `	}` |
|       - | 3814 | `	/* Get the length and clamp to valid range.` |
|       - | 3815 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3816 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3817 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3818 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3819 | `		if( iLength < 0 ){` |
|       7 | 3820 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3821 | `			if( iLength < 0 ){` |
|       3 | 3822 | `				iLength = 0;` |
|       1 | 3823 | `			}` |
|       3 | 3824 | `		}` |
|      31 | 3825 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3826 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3827 | `		}` |
|      15 | 3828 | `	}` |
|       - | 3829 | `	/* Create the result array for removed elements */` |
|      49 | 3830 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3831 | `	if( pArray == 0 ){` |
|     ! 0 | 3832 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3833 | `		return PH7_OK;` |
|       - | 3834 | `	}` |
|       - | 3835 | `	/* Get replacement array if provided */` |
|      49 | 3836 | `	pRep = 0;` |
|      49 | 3837 | `	if( nArg > 3 ){` |
|      21 | 3838 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3839 | `			/* Perform an array cast */` |
|       3 | 3840 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3841 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3842 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3843 | `			}` |
|       2 | 3844 | `		}else{` |
|      19 | 3845 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3846 | `		}` |
|      21 | 3847 | `		if( pRep ){` |
|       - | 3848 | `			/* Reset the loop cursor */` |
|      21 | 3849 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3850 | `		}` |
|      10 | 3851 | `	}` |
|       - | 3852 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3853 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3854 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3855 | `		return PH7_OK;` |
|       - | 3856 | `	}` |
|       - | 3857 | `	/* Navigate to the offset position */` |
|      41 | 3858 | `	pCur = pSrc->pFirst;` |
|      85 | 3859 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3860 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3861 | `	}` |
|       - | 3862 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3863 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3864 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3865 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3866 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3867 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3868 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3869 | `		pPrev = pCur->pPrev;` |
|      71 | 3870 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3871 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3872 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3873 | `			break;` |
|       - | 3874 | `		}` |
|      71 | 3875 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3876 | `	}` |
|       - | 3877 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3878 | `	if( pRep ){` |
|       - | 3879 | `		ph7_value sSafeVal;` |
|      61 | 3880 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3881 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3882 | `			if( pRvalue ){` |
|       - | 3883 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3884 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3885 | `				 * since it points into that same pool. */` |
|      31 | 3886 | `				sSafeVal = *pRvalue;` |
|      31 | 3887 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 3888 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 3889 | `					pNewNode = pSrc->pLast;` |
|      31 | 3890 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 3891 | `					pInsertAfter = pNewNode;` |
|      15 | 3892 | `				}` |
|      15 | 3893 | `			}` |
|       1 | 3894 | `		}` |
|      10 | 3895 | `	}` |
|       - | 3896 | `	/* Return the freshly created array */` |
|      41 | 3897 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 3898 | `	return PH7_OK;` |
|      29 | 3899 |  |
|       - | 3900 | `/*` |
|       - | 3901 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3902 | ` *  Checks if a value exists in an array.` |
|       - | 3903 | ` * Parameters` |
|       - | 3904 | ` *  $needle` |
|       - | 3905 | ` *   The searched value.` |
|       - | 3906 | ` *   Note:` |
|       - | 3907 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3908 | ` * $haystack` |
|       - | 3909 | ` *  The target array.` |
|       - | 3910 | ` * $strict` |
|       - | 3911 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3912 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3913 | ` */` |
|   27428 | 3914 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3915 |  |
|       - | 3916 | `	ph7_value *pNeedle;` |
|       - | 3917 | `	int bStrict;` |
|       - | 3918 | `	int rc;` |
|   27430 | 3919 | `	if( nArg < 2 ){` |
|       - | 3920 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3921 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3922 | `		return PH7_OK;` |
|       - | 3923 | `	}` |
|   27430 | 3924 | `	pNeedle = apArg[0];` |
|   27430 | 3925 | `	bStrict = 0;` |
|   27430 | 3926 | `	if( nArg > 2 ){` |
|       5 | 3927 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3928 | `	}` |
|   27430 | 3929 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3930 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3931 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3932 | `		/* Set the comparison result */` |
|     ! 0 | 3933 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3934 | `		return PH7_OK;` |
|       - | 3935 | `	}` |
|       - | 3936 | `	/* Perform the lookup */` |
|   27430 | 3937 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3938 | `	/* Lookup result */` |
|   27430 | 3939 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   27430 | 3940 | `	return PH7_OK;` |
|   13716 | 3941 |  |
|       - | 3942 | `/*` |
|       - | 3943 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3944 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3945 | ` * Parameters` |
|       - | 3946 | ` * $needle` |
|       - | 3947 | ` *   The searched value.` |
|       - | 3948 | ` * $haystack` |
|       - | 3949 | ` *   The array.` |
|       - | 3950 | ` * $strict` |
|       - | 3951 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3952 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3953 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3954 | ` * Return` |
|       - | 3955 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3956 | ` */` |
|      28 | 3957 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3958 |  |
|       - | 3959 | `	ph7_hashmap_node *pEntry;` |
|       - | 3960 | `	ph7_value *pVal,sNeedle;` |
|       - | 3961 | `	ph7_hashmap *pMap;` |
|       - | 3962 | `	ph7_value sVal;` |
|       - | 3963 | `	int bStrict;` |
|       - | 3964 | `	sxu32 n;` |
|       - | 3965 | `	int rc;` |
|      30 | 3966 | `	if( nArg < 2 ){` |
|       - | 3967 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3968 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3969 | `			"ArgumentCountError",` |
|       - | 3970 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 3971 | `			nArg` |
|       - | 3972 | `			);` |
|       - | 3973 | `	}` |
|      26 | 3974 | `	bStrict = FALSE;` |
|      26 | 3975 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3976 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3977 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3978 | `			"TypeError",` |
|       - | 3979 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 3980 | `			ph7_type_name(apArg[1])` |
|       - | 3981 | `			);` |
|       - | 3982 | `	}` |
|      24 | 3983 | `	if( nArg > 2 ){` |
|       - | 3984 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 3985 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3986 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3987 | `				"TypeError",` |
|       - | 3988 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3989 | `				ph7_type_name(apArg[2])` |
|       - | 3990 | `				);` |
|       - | 3991 | `		}` |
|       9 | 3992 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 3993 | `	}` |
|       - | 3994 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 3995 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3996 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 3997 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 3998 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 3999 | `	pEntry = pMap->pFirst;` |
|      21 | 4000 | `	n = pMap->nEntry;` |
|      23 | 4001 | `	for(;;){` |
|      47 | 4002 | `		if( !n ){` |
|       9 | 4003 | `			break;` |
|       - | 4004 | `		}` |
|       - | 4005 | `		/* Extract node value */` |
|      39 | 4006 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4007 | `		if( pVal ){` |
|       - | 4008 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4009 | `			 * can change their type.` |
|       - | 4010 | `			 */` |
|      39 | 4011 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4012 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4013 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4014 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4015 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4016 | `			if( rc == 0 ){` |
|       - | 4017 | `				/* Match found,return key */` |
|      13 | 4018 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4019 | `					/* INT key */` |
|       7 | 4020 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4021 | `				}else{` |
|       7 | 4022 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4023 | `					/* Blob key */` |
|       7 | 4024 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4025 | `				}` |
|      13 | 4026 | `				return PH7_OK;` |
|       - | 4027 | `			}` |
|      13 | 4028 | `		}` |
|       - | 4029 | `		/* Point to the next entry */` |
|      27 | 4030 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4031 | `		n--;` |
|       1 | 4032 | `	}` |
|       - | 4033 | `	/* No such value,return FALSE */` |
|       9 | 4034 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4035 | `	return PH7_OK;` |
|      16 | 4036 |  |
|       - | 4037 | `/*` |
|       - | 4038 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4039 | ` *  Computes the difference of arrays.` |
|       - | 4040 | ` * Parameters` |
|       - | 4041 | ` *  $array1` |
|       - | 4042 | ` *    The array to compare from` |
|       - | 4043 | ` *  $array2` |
|       - | 4044 | ` *    An array to compare against` |
|       - | 4045 | ` *  $...` |
|       - | 4046 | ` *   More arrays to compare against` |
|       - | 4047 | ` * Return` |
|       - | 4048 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4049 | ` *  are not present in any of the other arrays.` |
|       - | 4050 | ` */` |
|      22 | 4051 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4052 |  |
|       - | 4053 | `	ph7_hashmap_node *pEntry;` |
|       - | 4054 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4055 | `	ph7_value *pArray;` |
|       - | 4056 | `	ph7_value *pVal;` |
|       - | 4057 | `	sxi32 rc;` |
|       - | 4058 | `	sxu32 n;` |
|       - | 4059 | `	int i;` |
|       - | 4060 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4061 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4062 | `	 * debugging difficult. */` |
|      24 | 4063 | `	if( nArg < 1 ){` |
|       4 | 4064 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4065 | `			"ArgumentCountError",` |
|       - | 4066 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4067 | `			nArg` |
|       - | 4068 | `			);` |
|       - | 4069 | `	}` |
|      22 | 4070 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4071 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4072 | `			"TypeError",` |
|       - | 4073 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4074 | `			ph7_type_name(apArg[0])` |
|       - | 4075 | `			);` |
|       - | 4076 | `	}` |
|      36 | 4077 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4078 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4079 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4080 | `				"TypeError",` |
|       - | 4081 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4082 | `				i + 1,` |
|       2 | 4083 | `				ph7_type_name(apArg[i])` |
|       - | 4084 | `				);` |
|       - | 4085 | `		}` |
|       9 | 4086 | `	}` |
|      17 | 4087 | `	if( nArg == 1 ){` |
|       - | 4088 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4089 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4090 | `		return PH7_OK;` |
|       - | 4091 | `	}` |
|       - | 4092 | `	/* Create a new array */` |
|      15 | 4093 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4094 | `	if( pArray == 0 ){` |
|     ! 0 | 4095 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4096 | `		return PH7_OK;` |
|       - | 4097 | `	}` |
|       - | 4098 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4099 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4100 | `	/* Perform the diff */` |
|      15 | 4101 | `	pEntry = pSrc->pFirst;` |
|      15 | 4102 | `	n = pSrc->nEntry;` |
|      27 | 4103 | `	for(;;){` |
|      55 | 4104 | `		if( n < 1 ){` |
|      15 | 4105 | `			break;` |
|       - | 4106 | `		}` |
|       - | 4107 | `		/* Extract the node value */` |
|      41 | 4108 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4109 | `		if( pVal ){` |
|      69 | 4110 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4111 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4112 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4113 | `				/* Perform the lookup */` |
|      45 | 4114 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4115 | `				if( rc == SXRET_OK ){` |
|       - | 4116 | `					/* Value exist */` |
|      17 | 4117 | `					break;` |
|       - | 4118 | `				}` |
|      15 | 4119 | `			}` |
|      41 | 4120 | `			if( i >= nArg ){` |
|       - | 4121 | `				/* Perform the insertion */` |
|      25 | 4122 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4123 | `			}` |
|      20 | 4124 | `		}` |
|       - | 4125 | `		/* Point to the next entry */` |
|      41 | 4126 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4127 | `		n--;` |
|       1 | 4128 | `	}` |
|       - | 4129 | `	/* Return the freshly created array */` |
|      15 | 4130 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4131 | `	return PH7_OK;` |
|      13 | 4132 |  |
|       - | 4133 | `/*` |
|       - | 4134 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4135 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4136 | ` * Parameters` |
|       - | 4137 | ` *  $array1` |
|       - | 4138 | ` *    The array to compare from` |
|       - | 4139 | ` *  $array2` |
|       - | 4140 | ` *    An array to compare against` |
|       - | 4141 | ` *  $...` |
|       - | 4142 | ` *   More arrays to compare against.` |
|       - | 4143 | ` * $callback` |
|       - | 4144 | ` *  The callback comparison function.` |
|       - | 4145 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4146 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4147 | ` *  than the second.` |
|       - | 4148 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4149 | ` * Return` |
|       - | 4150 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4151 | ` *  are not present in any of the other arrays.` |
|       - | 4152 | ` */` |
|      20 | 4153 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4154 |  |
|       - | 4155 | `	ph7_hashmap_node *pEntry;` |
|       - | 4156 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4157 | `	ph7_value *pCallback;` |
|       - | 4158 | `	ph7_value *pArray;` |
|       - | 4159 | `	ph7_value *pVal;` |
|       - | 4160 | `	sxi32 rc;` |
|       - | 4161 | `	sxu32 n;` |
|       - | 4162 | `	int i;` |
|       - | 4163 |  |
|       - | 4164 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      22 | 4165 | `	if( nArg < 2 ){` |
|       4 | 4166 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4167 | `			"ArgumentCountError",` |
|       - | 4168 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4169 | `			nArg` |
|       - | 4170 | `			);` |
|       - | 4171 | `	}` |
|      20 | 4172 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4173 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4174 | `			"TypeError",` |
|       - | 4175 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4176 | `			ph7_type_name(apArg[0])` |
|       - | 4177 | `			);` |
|       - | 4178 | `	}` |
|       - | 4179 |  |
|      18 | 4180 | `	if( nArg == 2 ){` |
|       - | 4181 | `		/* Only the original array and the callback were provided. */` |
|       - | 4182 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4183 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4184 | `		 * validation order.` |
|       - | 4185 | `		 */` |
|       4 | 4186 | `	} else {` |
|       - | 4187 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      20 | 4188 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      14 | 4189 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      10 | 4190 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4191 | `					"TypeError",` |
|       - | 4192 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4193 | `					i + 1,` |
|       6 | 4194 | `					ph7_type_name(apArg[i])` |
|       - | 4195 | `					);` |
|       - | 4196 | `			}` |
|       5 | 4197 | `		}` |
|       - | 4198 | `	}` |
|       - | 4199 |  |
|       - | 4200 | `	/* Identify the callback (always expected as the last argument). */` |
|      12 | 4201 | `	pCallback = apArg[nArg - 1];` |
|       - | 4202 | `	/* Validate the callback to match PHP's error messages. */` |
|      12 | 4203 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       7 | 4204 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4205 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4206 | `				"TypeError",` |
|       - | 4207 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4208 | `				nArg` |
|       - | 4209 | `				);` |
|       - | 4210 | `		}` |
|       5 | 4211 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4212 | `			int len;` |
|       3 | 4213 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4214 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4215 | `				"TypeError",` |
|       - | 4216 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4217 | `				nArg,` |
|       1 | 4218 | `				zName` |
|       - | 4219 | `				);` |
|       - | 4220 | `		}` |
|       4 | 4221 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4222 | `			"TypeError",` |
|       - | 4223 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4224 | `			nArg` |
|       - | 4225 | `			);` |
|       - | 4226 | `	}` |
|       - | 4227 |  |
|       5 | 4228 | `	if( nArg == 2 ){` |
|       - | 4229 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4230 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4231 | `		return PH7_OK;` |
|       - | 4232 | `	}` |
|       - | 4233 |  |
|       - | 4234 | `	/* Create a new array */` |
|       3 | 4235 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4236 | `	if( pArray == 0 ){` |
|     ! 0 | 4237 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4238 | `		return PH7_OK;` |
|       - | 4239 | `	}` |
|       - | 4240 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4241 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4242 | `	/* Perform the diff */` |
|       3 | 4243 | `	pEntry = pSrc->pFirst;` |
|       3 | 4244 | `	n = pSrc->nEntry;` |
|       4 | 4245 | `	for(;;){` |
|       9 | 4246 | `		if( n < 1 ){` |
|       3 | 4247 | `			break;` |
|       - | 4248 | `		}` |
|       - | 4249 | `		/* Extract the node value */` |
|       7 | 4250 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4251 | `		if( pVal ){` |
|      11 | 4252 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4253 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4254 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4255 | `				/* Perform the lookup */` |
|       7 | 4256 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4257 | `				if( rc == SXRET_OK ){` |
|       - | 4258 | `					/* Value exist */` |
|       3 | 4259 | `					break;` |
|       - | 4260 | `				}` |
|       3 | 4261 | `			}` |
|       7 | 4262 | `			if( i >= (nArg - 1)){` |
|       - | 4263 | `				/* Perform the insertion */` |
|       5 | 4264 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4265 | `			}` |
|       3 | 4266 | `		}` |
|       - | 4267 | `		/* Point to the next entry */` |
|       7 | 4268 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4269 | `		n--;` |
|       1 | 4270 | `	}` |
|       - | 4271 | `	/* Return the freshly created array */` |
|       3 | 4272 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4273 | `	return PH7_OK;` |
|      12 | 4274 |  |
|       - | 4275 | `/*` |
|       - | 4276 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4277 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4278 | ` * Parameters` |
|       - | 4279 | ` *  $array1` |
|       - | 4280 | ` *    The array to compare from` |
|       - | 4281 | ` *  $array2` |
|       - | 4282 | ` *    An array to compare against` |
|       - | 4283 | ` *  $...` |
|       - | 4284 | ` *   More arrays to compare against` |
|       - | 4285 | ` * Return` |
|       - | 4286 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4287 | ` *  are not present in any of the other arrays.` |
|       - | 4288 | ` */` |
|      20 | 4289 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4290 |  |
|       - | 4291 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4292 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4293 | `	ph7_value *pArray;` |
|       - | 4294 | `	ph7_value *pVal;` |
|       - | 4295 | `	sxi32 rc;` |
|       - | 4296 | `	sxu32 n;` |
|       - | 4297 | `	int i;` |
|       - | 4298 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4299 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4300 | `	 * accompanying integration tests to pass. */` |
|      22 | 4301 | `	if( nArg < 1 ){` |
|       4 | 4302 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4303 | `			"ArgumentCountError",` |
|       - | 4304 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4305 | `			nArg` |
|       - | 4306 | `			);` |
|       - | 4307 | `	}` |
|      20 | 4308 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4309 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4310 | `			"TypeError",` |
|       - | 4311 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4312 | `			ph7_type_name(apArg[0])` |
|       - | 4313 | `			);` |
|       - | 4314 | `	}` |
|      32 | 4315 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4316 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4317 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4318 | `				"TypeError",` |
|       - | 4319 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4320 | `				i + 1,` |
|       4 | 4321 | `				ph7_type_name(apArg[i])` |
|       - | 4322 | `				);` |
|       - | 4323 | `		}` |
|       9 | 4324 | `	}` |
|      13 | 4325 | `	if( nArg == 1 ){` |
|       - | 4326 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4327 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4328 | `		return PH7_OK;` |
|       - | 4329 | `	}` |
|       - | 4330 | `	/* Create a new array */` |
|      11 | 4331 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4332 | `	if( pArray == 0 ){` |
|     ! 0 | 4333 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4334 | `		return PH7_OK;` |
|       - | 4335 | `	}` |
|       - | 4336 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4337 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4338 | `	/* Perform the diff */` |
|      11 | 4339 | `	pEntry = pSrc->pFirst;` |
|      11 | 4340 | `	n = pSrc->nEntry;` |
|      11 | 4341 | `	pN1 = pN2 = 0;` |
|      29 | 4342 | `	for(;;){` |
|       - | 4343 | `		int keep;` |
|      35 | 4344 | `		if( n < 1 ){` |
|      11 | 4345 | `			break;` |
|       - | 4346 | `		}` |
|       - | 4347 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4348 | `		keep = 1;` |
|      41 | 4349 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4350 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4351 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4352 | `			/* Perform a key lookup first */` |
|      29 | 4353 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4354 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4355 | `			}else{` |
|      17 | 4356 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4357 | `			}` |
|      29 | 4358 | `			if( rc != SXRET_OK ){` |
|       - | 4359 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4360 | `				continue;` |
|       - | 4361 | `			}` |
|       - | 4362 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4363 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4364 | `			if( pVal ){` |
|       - | 4365 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4366 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4367 | `				if( pVal2 ){` |
|      15 | 4368 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4369 | `					if( cmp == 0 ){` |
|       - | 4370 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4371 | `						keep = 0;` |
|      13 | 4372 | `						break;` |
|       - | 4373 | `					}` |
|       1 | 4374 | `				}` |
|       1 | 4375 | `			}` |
|       2 | 4376 | `		}` |
|      25 | 4377 | `		if( keep ){` |
|       - | 4378 | `			/* Perform the insertion */` |
|      13 | 4379 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4380 | `		}` |
|       - | 4381 | `		/* Point to the next entry */` |
|      25 | 4382 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4383 | `		n--;` |
|       1 | 4384 | `	}` |
|       - | 4385 | `	/* Return the freshly created array */` |
|      11 | 4386 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4387 | `	return PH7_OK;` |
|      12 | 4388 |  |
|       - | 4389 | `/*` |
|       - | 4390 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4391 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4392 | ` *  by a user supplied callback function.` |
|       - | 4393 | ` * Parameters` |
|       - | 4394 | ` *  $array1` |
|       - | 4395 | ` *    The array to compare from` |
|       - | 4396 | ` *  $array2` |
|       - | 4397 | ` *    An array to compare against` |
|       - | 4398 | ` *  $...` |
|       - | 4399 | ` *   More arrays to compare against.` |
|       - | 4400 | ` *  $key_compare_func` |
|       - | 4401 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4402 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4403 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4404 | ` * Return` |
|       - | 4405 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4406 | ` *  are not present in any of the other arrays.` |
|       - | 4407 | ` */` |
|      22 | 4408 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4409 |  |
|       - | 4410 | `	ph7_hashmap_node *pEntry;` |
|       - | 4411 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4412 | `	ph7_value *pCallback;` |
|       - | 4413 | `	ph7_value *pArray;` |
|       - | 4414 | `	sxi32 rc;` |
|       - | 4415 | `	sxu32 n;` |
|       - | 4416 | `	int i;` |
|       - | 4417 |  |
|       - | 4418 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4419 | `	if( nArg < 2 ){` |
|       4 | 4420 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4421 | `			"ArgumentCountError",` |
|       - | 4422 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4423 | `			nArg` |
|       - | 4424 | `			);` |
|       - | 4425 | `	}` |
|      22 | 4426 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4427 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4428 | `			"TypeError",` |
|       - | 4429 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4430 | `			ph7_type_name(apArg[0])` |
|       - | 4431 | `			);` |
|       - | 4432 | `	}` |
|       - | 4433 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4434 | `	 * expected to be a callback. */` |
|      32 | 4435 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4436 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4437 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4438 | `				"TypeError",` |
|       - | 4439 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4440 | `				i + 1,` |
|       2 | 4441 | `				ph7_type_name(apArg[i])` |
|       - | 4442 | `				);` |
|       - | 4443 | `		}` |
|       8 | 4444 | `	}` |
|       - | 4445 | `	/* Point to the callback value */` |
|      18 | 4446 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4447 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4448 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4449 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4450 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4451 | `		 * string given" which we also reproduce. */` |
|       7 | 4452 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4453 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4454 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4455 | `				"TypeError",` |
|       - | 4456 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4457 | `				nArg` |
|       - | 4458 | `				);` |
|       - | 4459 | `		}` |
|       5 | 4460 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4461 | `			/* neither array nor string */` |
|       7 | 4462 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4463 | `				"TypeError",` |
|       - | 4464 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4465 | `				nArg` |
|       - | 4466 | `				);` |
|       - | 4467 | `		}` |
|       - | 4468 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4469 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4470 | `			"TypeError",` |
|       - | 4471 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4472 | `			nArg,` |
|     ! 0 | 4473 | `			ph7_type_name(pCallback)` |
|       - | 4474 | `			);` |
|       - | 4475 | `	}` |
|      11 | 4476 | `	if( nArg == 2 ){` |
|       - | 4477 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4478 | `		 * input array. */` |
|       3 | 4479 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4480 | `		return PH7_OK;` |
|       - | 4481 | `	}` |
|       - | 4482 | `	/* Create a new array */` |
|       9 | 4483 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4484 | `	if( pArray == 0 ){` |
|     ! 0 | 4485 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4486 | `		return PH7_OK;` |
|       - | 4487 | `	}` |
|       - | 4488 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4489 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4490 | `	/* Perform the diff */` |
|       9 | 4491 | `	pEntry = pSrc->pFirst;` |
|       9 | 4492 | `	n = pSrc->nEntry;` |
|      20 | 4493 | `	for(;;){` |
|       - | 4494 | `		int keep;` |
|      25 | 4495 | `		if( n < 1 ){` |
|       9 | 4496 | `			break;` |
|       - | 4497 | `		}` |
|      17 | 4498 | `		keep = 1;` |
|      29 | 4499 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4500 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4501 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4502 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4503 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4504 | `			while( pIt ){` |
|       - | 4505 | `				/* build temporary key values for callback */` |
|       - | 4506 | `				ph7_value key1, key2, result;` |
|       - | 4507 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4508 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4509 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4510 | `				}else{` |
|       - | 4511 | `					SyString sStr;` |
|      31 | 4512 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4513 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4514 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4515 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4516 | `				}` |
|      31 | 4517 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4518 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4519 | `				}else{` |
|       - | 4520 | `					SyString sStr;` |
|      31 | 4521 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4522 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4523 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4524 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4525 | `				}` |
|      31 | 4526 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4527 | `				/* call user callback with (key1, key2) */` |
|       - | 4528 | `				{` |
|       - | 4529 | `					ph7_value *apK[2];` |
|      31 | 4530 | `					apK[0] = &key1;` |
|      31 | 4531 | `					apK[1] = &key2;` |
|      31 | 4532 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4533 | `				}` |
|      31 | 4534 | `				if( rc == SXRET_OK ){` |
|      31 | 4535 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4536 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4537 | `					}` |
|      31 | 4538 | `					if( result.x.iVal == 0 ){` |
|       - | 4539 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4540 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4541 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4542 | `						if( pVal1 && pVal2 ){` |
|      13 | 4543 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4544 | `								keep = 0;` |
|       9 | 4545 | `								PH7_MemObjRelease(&result);` |
|       - | 4546 | `								/* release keys too before breaking */` |
|       9 | 4547 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4548 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4549 | `								break;` |
|       - | 4550 | `							}` |
|       2 | 4551 | `						}` |
|       2 | 4552 | `					}` |
|      11 | 4553 | `				}` |
|      23 | 4554 | `				PH7_MemObjRelease(&result);` |
|      23 | 4555 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4556 | `				PH7_MemObjRelease(&key2);` |
|       - | 4557 | `				/* move to next node */` |
|      23 | 4558 | `				pIt = pIt->pPrev;` |
|      23 | 4559 | `				if( keep == 0 ) break;` |
|       1 | 4560 | `			}` |
|      21 | 4561 | `			if( keep == 0 ) break;` |
|       7 | 4562 | `		}` |
|      17 | 4563 | `		if( keep ){` |
|       - | 4564 | `			/* Perform the insertion */` |
|       9 | 4565 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4566 | `		}` |
|       - | 4567 | `		/* Point to the next entry */` |
|      17 | 4568 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4569 | `		n--;` |
|       1 | 4570 | `	}` |
|       - | 4571 | `	/* Return the freshly created array */` |
|       9 | 4572 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4573 | `	return PH7_OK;` |
|      13 | 4574 |  |
|       - | 4575 | `/*` |
|       - | 4576 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4577 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4578 | ` * Parameters` |
|       - | 4579 | ` *  $array1` |
|       - | 4580 | ` *    The array to compare from` |
|       - | 4581 | ` *  $array2` |
|       - | 4582 | ` *    An array to compare against` |
|       - | 4583 | ` *  $...` |
|       - | 4584 | ` *   More arrays to compare against` |
|       - | 4585 | ` * Return` |
|       - | 4586 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4587 | ` *  in any of the other arrays.` |
|       - | 4588 | ` * Note that NULL is returned on failure.` |
|       - | 4589 | ` */` |
|      14 | 4590 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4591 |  |
|       - | 4592 | `	ph7_hashmap_node *pEntry;` |
|       - | 4593 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4594 | `	ph7_value *pArray;` |
|       - | 4595 | `	sxi32 rc;` |
|       - | 4596 | `	sxu32 n;` |
|       - | 4597 | `	int i;` |
|       - | 4598 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4599 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4600 | `	 * helpers. */` |
|      16 | 4601 | `	if( nArg < 1 ){` |
|       4 | 4602 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4603 | `			"ArgumentCountError",` |
|       - | 4604 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4605 | `			nArg` |
|       - | 4606 | `			);` |
|       - | 4607 | `	}` |
|      14 | 4608 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4609 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4610 | `			"TypeError",` |
|       - | 4611 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4612 | `			ph7_type_name(apArg[0])` |
|       - | 4613 | `			);` |
|       - | 4614 | `	}` |
|      20 | 4615 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4616 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4617 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4618 | `				"TypeError",` |
|       - | 4619 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4620 | `				i + 1,` |
|       2 | 4621 | `				ph7_type_name(apArg[i])` |
|       - | 4622 | `				);` |
|       - | 4623 | `		}` |
|       5 | 4624 | `	}` |
|       9 | 4625 | `	if( nArg == 1 ){` |
|       - | 4626 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4627 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4628 | `		return PH7_OK;` |
|       - | 4629 | `	}` |
|       - | 4630 | `	/* Create a new array */` |
|       7 | 4631 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4632 | `	if( pArray == 0 ){` |
|     ! 0 | 4633 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4634 | `		return PH7_OK;` |
|       - | 4635 | `	}` |
|       - | 4636 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4637 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4638 | `	/* Perfrom the diff */` |
|       7 | 4639 | `	pEntry = pSrc->pFirst;` |
|       7 | 4640 | `	n = pSrc->nEntry;` |
|      12 | 4641 | `	for(;;){` |
|      25 | 4642 | `		if( n < 1 ){` |
|       7 | 4643 | `			break;` |
|       - | 4644 | `		}` |
|      31 | 4645 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4646 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4647 | `				/* ignore */` |
|     ! 0 | 4648 | `				continue;` |
|       - | 4649 | `			}` |
|      23 | 4650 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4651 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4652 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4653 | `				/* Blob lookup */` |
|      17 | 4654 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4655 | `			}else{` |
|       - | 4656 | `				/* Int lookup */` |
|       7 | 4657 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4658 | `			}` |
|      23 | 4659 | `			if( rc == SXRET_OK ){` |
|       - | 4660 | `				/* Key exists,break immediately */` |
|      11 | 4661 | `				break;` |
|       - | 4662 | `			}` |
|       7 | 4663 | `		}` |
|      19 | 4664 | `		if( i >= nArg ){` |
|       - | 4665 | `			/* Perform the insertion */` |
|       9 | 4666 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4667 | `		}` |
|       - | 4668 | `		/* Point to the next entry */` |
|      19 | 4669 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4670 | `		n--;` |
|       1 | 4671 | `	}` |
|       - | 4672 | `	/* Return the freshly created array */` |
|       7 | 4673 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4674 | `	return PH7_OK;` |
|       9 | 4675 |  |
|       - | 4676 | `/*` |
|       - | 4677 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4678 | ` *  Computes the intersection of arrays.` |
|       - | 4679 | ` * Parameters` |
|       - | 4680 | ` *  $array1` |
|       - | 4681 | ` *    The array to compare from` |
|       - | 4682 | ` *  $array2` |
|       - | 4683 | ` *    An array to compare against` |
|       - | 4684 | ` *  $...` |
|       - | 4685 | ` *   More arrays to compare against` |
|       - | 4686 | ` * Return` |
|       - | 4687 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4688 | ` *  in all of the parameters.` |
|       - | 4689 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4690 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4691 | ` */` |
|      22 | 4692 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4693 |  |
|       - | 4694 | `	ph7_hashmap_node *pEntry;` |
|       - | 4695 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4696 | `	ph7_value *pArray;` |
|       - | 4697 | `	ph7_value *pVal;` |
|       - | 4698 | `	sxi32 rc;` |
|       - | 4699 | `	sxu32 n;` |
|       - | 4700 | `	int i;` |
|      24 | 4701 | `	if( nArg < 1 ){` |
|       4 | 4702 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4703 | `			"ArgumentCountError",` |
|       - | 4704 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4705 | `			nArg` |
|       - | 4706 | `			);` |
|       - | 4707 | `	}` |
|      22 | 4708 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4709 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4710 | `			"TypeError",` |
|       - | 4711 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4712 | `			ph7_type_name(apArg[0])` |
|       - | 4713 | `			);` |
|       - | 4714 | `	}` |
|      36 | 4715 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4716 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4717 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4718 | `				"TypeError",` |
|       - | 4719 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4720 | `				i + 1,` |
|       2 | 4721 | `				ph7_type_name(apArg[i])` |
|       - | 4722 | `				);` |
|       - | 4723 | `		}` |
|       9 | 4724 | `	}` |
|      17 | 4725 | `	if( nArg == 1 ){` |
|       - | 4726 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4727 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4728 | `		return PH7_OK;` |
|       - | 4729 | `	}` |
|       - | 4730 | `	/* Create a new array */` |
|      15 | 4731 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4732 | `	if( pArray == 0 ){` |
|     ! 0 | 4733 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4734 | `		return PH7_OK;` |
|       - | 4735 | `	}` |
|       - | 4736 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4737 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4738 | `	/* Perform the intersection */` |
|      15 | 4739 | `	pEntry = pSrc->pFirst;` |
|      15 | 4740 | `	n = pSrc->nEntry;` |
|      31 | 4741 | `	for(;;){` |
|      63 | 4742 | `		if( n < 1 ){` |
|      15 | 4743 | `			break;` |
|       - | 4744 | `		}` |
|       - | 4745 | `		/* Extract the node value */` |
|      49 | 4746 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4747 | `		if( pVal ){` |
|      79 | 4748 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4749 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4750 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4751 | `				/* Perform the lookup */` |
|      55 | 4752 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4753 | `				if( rc != SXRET_OK ){` |
|       - | 4754 | `					/* Value does not exist */` |
|      25 | 4755 | `					break;` |
|       - | 4756 | `				}` |
|      16 | 4757 | `			}` |
|      49 | 4758 | `			if( i >= nArg ){` |
|       - | 4759 | `				/* Perform the insertion */` |
|      25 | 4760 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4761 | `			}` |
|      24 | 4762 | `		}` |
|       - | 4763 | `		/* Point to the next entry */` |
|      49 | 4764 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4765 | `		n--;` |
|       1 | 4766 | `	}` |
|       - | 4767 | `	/* Return the freshly created array */` |
|      15 | 4768 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4769 | `	return PH7_OK;` |
|      13 | 4770 |  |
|       - | 4771 | `/*` |
|       - | 4772 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4773 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4774 | ` * Parameters` |
|       - | 4775 | ` *  $array1` |
|       - | 4776 | ` *    The array to compare from` |
|       - | 4777 | ` *  $array2` |
|       - | 4778 | ` *    An array to compare against` |
|       - | 4779 | ` *  $...` |
|       - | 4780 | ` *   More arrays to compare against` |
|       - | 4781 | ` * Return` |
|       - | 4782 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4783 | ` *  in all the arguments, with matching keys.` |
|       - | 4784 | ` */` |
|      22 | 4785 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4786 |  |
|       - | 4787 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4788 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4789 | `	ph7_value *pArray;` |
|       - | 4790 | `	ph7_value *pVal;` |
|       - | 4791 | `	sxi32 rc;` |
|       - | 4792 | `	sxu32 n;` |
|       - | 4793 | `	int i;` |
|      24 | 4794 | `	if( nArg < 1 ){` |
|       4 | 4795 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4796 | `			"ArgumentCountError",` |
|       - | 4797 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4798 | `			nArg` |
|       - | 4799 | `			);` |
|       - | 4800 | `	}` |
|      22 | 4801 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4802 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4803 | `			"TypeError",` |
|       - | 4804 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4805 | `			ph7_type_name(apArg[0])` |
|       - | 4806 | `			);` |
|       - | 4807 | `	}` |
|      36 | 4808 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4809 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4810 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4811 | `				"TypeError",` |
|       - | 4812 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4813 | `				i + 1,` |
|       2 | 4814 | `				ph7_type_name(apArg[i])` |
|       - | 4815 | `				);` |
|       - | 4816 | `		}` |
|       9 | 4817 | `	}` |
|      17 | 4818 | `	if( nArg == 1 ){` |
|       - | 4819 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4820 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4821 | `		return PH7_OK;` |
|       - | 4822 | `	}` |
|       - | 4823 | `	/* Create a new array */` |
|      15 | 4824 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4825 | `	if( pArray == 0 ){` |
|     ! 0 | 4826 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4827 | `		return PH7_OK;` |
|       - | 4828 | `	}` |
|       - | 4829 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4830 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4831 | `	/* Perform the intersection */` |
|      15 | 4832 | `	pEntry = pSrc->pFirst;` |
|      15 | 4833 | `	n = pSrc->nEntry;` |
|      15 | 4834 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4835 | `	for(;;){` |
|      47 | 4836 | `		if( n < 1 ){` |
|      15 | 4837 | `			break;` |
|       - | 4838 | `		}` |
|       - | 4839 | `		/* Extract the node value */` |
|      33 | 4840 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4841 | `		if( pVal ){` |
|      53 | 4842 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4843 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4844 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4845 | `				/* Perform a key lookup first */` |
|      37 | 4846 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4847 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4848 | `				}else{` |
|      23 | 4849 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4850 | `				}` |
|      37 | 4851 | `				if( rc != SXRET_OK ){` |
|       - | 4852 | `					/* No such key,break immediately */` |
|       7 | 4853 | `					break;` |
|       - | 4854 | `				}` |
|       - | 4855 | `				/* Perform the lookup */` |
|      31 | 4856 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4857 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4858 | `					/* Value does not exist */` |
|       6 | 4859 | `					break;` |
|       - | 4860 | `				}` |
|      11 | 4861 | `			}` |
|      33 | 4862 | `			if( i >= nArg ){` |
|       - | 4863 | `				/* Perform the insertion */` |
|      17 | 4864 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4865 | `			}` |
|      16 | 4866 | `		}` |
|       - | 4867 | `		/* Point to the next entry */` |
|      33 | 4868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4869 | `		n--;` |
|       1 | 4870 | `	}` |
|       - | 4871 | `	/* Return the freshly created array */` |
|      15 | 4872 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4873 | `	return PH7_OK;` |
|      13 | 4874 |  |
|       - | 4875 | `/*` |
|       - | 4876 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 4877 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4878 | ` * Parameters` |
|       - | 4879 | ` *  $array1` |
|       - | 4880 | ` *    The array to compare from` |
|       - | 4881 | ` *  $...` |
|       - | 4882 | ` *   More arrays to compare against` |
|       - | 4883 | ` * Return` |
|       - | 4884 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4885 | ` *  have keys that are present in all arguments.` |
|       - | 4886 | ` * Note that NULL is returned on failure.` |
|       - | 4887 | ` */` |
|      22 | 4888 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4889 |  |
|       - | 4890 | `	ph7_hashmap_node *pEntry;` |
|       - | 4891 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4892 | `	ph7_value *pArray;` |
|       - | 4893 | `	sxi32 rc;` |
|       - | 4894 | `	sxu32 n;` |
|       - | 4895 | `	int i;` |
|      24 | 4896 | `	if( nArg < 1 ){` |
|       4 | 4897 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4898 | `			"ArgumentCountError",` |
|       - | 4899 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 4900 | `			nArg` |
|       - | 4901 | `			);` |
|       - | 4902 | `	}` |
|      22 | 4903 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4904 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4905 | `			"TypeError",` |
|       - | 4906 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4907 | `			ph7_type_name(apArg[0])` |
|       - | 4908 | `			);` |
|       - | 4909 | `	}` |
|      36 | 4910 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4911 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4912 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4913 | `				"TypeError",` |
|       - | 4914 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4915 | `				i + 1,` |
|       2 | 4916 | `				ph7_type_name(apArg[i])` |
|       - | 4917 | `				);` |
|       - | 4918 | `		}` |
|       9 | 4919 | `	}` |
|      17 | 4920 | `	if( nArg == 1 ){` |
|       - | 4921 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4922 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4923 | `		return PH7_OK;` |
|       - | 4924 | `	}` |
|       - | 4925 | `	/* Create a new array */` |
|      15 | 4926 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4927 | `	if( pArray == 0 ){` |
|     ! 0 | 4928 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4929 | `		return PH7_OK;` |
|       - | 4930 | `	}` |
|       - | 4931 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 4932 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4933 | `	/* Perform the intersection */` |
|      15 | 4934 | `	pEntry = pSrc->pFirst;` |
|      15 | 4935 | `	n = pSrc->nEntry;` |
|      24 | 4936 | `	for(;;){` |
|      49 | 4937 | `		if( n < 1 ){` |
|      15 | 4938 | `			break;` |
|       - | 4939 | `		}` |
|      57 | 4940 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 4941 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 4942 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 4943 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4944 | `				/* Blob lookup */` |
|      27 | 4945 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 4946 | `			}else{` |
|       - | 4947 | `				/* Int key */` |
|      13 | 4948 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4949 | `			}` |
|      39 | 4950 | `			if( rc != SXRET_OK ){` |
|       - | 4951 | `				/* Key does not exist, break immediately */` |
|      17 | 4952 | `				break;` |
|       - | 4953 | `			}` |
|      12 | 4954 | `		}` |
|      35 | 4955 | `		if( i >= nArg ){` |
|       - | 4956 | `			/* Perform the insertion */` |
|      19 | 4957 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 4958 | `		}` |
|       - | 4959 | `		/* Point to the next entry */` |
|      35 | 4960 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 4961 | `		n--;` |
|       1 | 4962 | `	}` |
|       - | 4963 | `	/* Return the freshly created array */` |
|      15 | 4964 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4965 | `	return PH7_OK;` |
|      13 | 4966 |  |
|       - | 4967 | `/*` |
|       - | 4968 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4969 | ` *  Computes the intersection of arrays.` |
|       - | 4970 | ` * Parameters` |
|       - | 4971 | ` *  $array1` |
|       - | 4972 | ` *    The array to compare from` |
|       - | 4973 | ` *  $array2` |
|       - | 4974 | ` *    An array to compare against` |
|       - | 4975 | ` *  $...` |
|       - | 4976 | ` *   More arrays to compare against` |
|       - | 4977 | ` * $callback` |
|       - | 4978 | ` *  The callback comparison function.` |
|       - | 4979 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4980 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4981 | ` *  than the second.` |
|       - | 4982 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4983 | ` * Return` |
|       - | 4984 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4985 | ` *  in all of the parameters. .` |
|       - | 4986 | ` * Note that NULL is returned on failure.` |
|       - | 4987 | ` */` |
|      24 | 4988 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4989 |  |
|       - | 4990 | `	ph7_hashmap_node *pEntry;` |
|       - | 4991 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4992 | `	ph7_value *pCallback;` |
|       - | 4993 | `	ph7_value *pArray;` |
|       - | 4994 | `	ph7_value *pVal;` |
|       - | 4995 | `	sxi32 rc;` |
|       - | 4996 | `	sxu32 n;` |
|       - | 4997 | `	int i;` |
|       - | 4998 |  |
|       - | 4999 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      26 | 5000 | `	if( nArg < 2 ){` |
|       4 | 5001 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5002 | `			"ArgumentCountError",` |
|       - | 5003 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5004 | `			nArg` |
|       - | 5005 | `			);` |
|       - | 5006 | `	}` |
|      24 | 5007 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5008 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5009 | `			"TypeError",` |
|       - | 5010 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5011 | `			ph7_type_name(apArg[0])` |
|       - | 5012 | `			);` |
|       - | 5013 | `	}` |
|       - | 5014 |  |
|      22 | 5015 | `	if( nArg == 2 ){` |
|       - | 5016 | `		/* Only the original array and the callback were provided. */` |
|       - | 5017 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5018 | `		 * validation ordering. */` |
|       3 | 5019 | `	} else {` |
|       - | 5020 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      32 | 5021 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      18 | 5022 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5023 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5024 | `					"TypeError",` |
|       - | 5025 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5026 | `					i + 1,` |
|       2 | 5027 | `					ph7_type_name(apArg[i])` |
|       - | 5028 | `					);` |
|       - | 5029 | `			}` |
|       9 | 5030 | `		}` |
|       - | 5031 | `	}` |
|       - | 5032 |  |
|       - | 5033 | `	/* Identify the callback (always expected as the last argument). */` |
|      20 | 5034 | `	pCallback = apArg[nArg - 1];` |
|       - | 5035 | `	/* Validate the callback to match PHP's error messages. */` |
|      20 | 5036 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      11 | 5037 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5038 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5039 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5040 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5041 | `			 */` |
|       7 | 5042 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       7 | 5043 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5044 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5045 | `					"TypeError",` |
|       - | 5046 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5047 | `					nArg` |
|       - | 5048 | `					);` |
|       - | 5049 | `			}` |
|       - | 5050 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5051 | `			{` |
|       5 | 5052 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       5 | 5053 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       5 | 5054 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5055 | `					int nMethodLen;` |
|       5 | 5056 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       5 | 5057 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       5 | 5058 | `					if( pClass ){` |
|       - | 5059 | `						/* Class exists but method is missing. */` |
|       4 | 5060 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5061 | `							"TypeError",` |
|       - | 5062 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5063 | `							nArg,` |
|       1 | 5064 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5065 | `							zMethod` |
|       - | 5066 | `							);` |
|       - | 5067 | `					}` |
|       - | 5068 | `					/* Class not found */` |
|       - | 5069 | `					{` |
|       - | 5070 | `						int nName;` |
|       3 | 5071 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5072 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5073 | `							"TypeError",` |
|       - | 5074 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5075 | `							nArg,` |
|       1 | 5076 | `							zName` |
|       - | 5077 | `							);` |
|       - | 5078 | `					}` |
|       - | 5079 | `				}` |
|       - | 5080 | `			}` |
|       - | 5081 | `			/* Fallback message */` |
|     ! 0 | 5082 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5083 | `				"TypeError",` |
|       - | 5084 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5085 | `				nArg` |
|       - | 5086 | `				);` |
|       - | 5087 | `		}` |
|       5 | 5088 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5089 | `			int len;` |
|       3 | 5090 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5091 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5092 | `				"TypeError",` |
|       - | 5093 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5094 | `				nArg,` |
|       1 | 5095 | `				zName` |
|       - | 5096 | `				);` |
|       - | 5097 | `		}` |
|       4 | 5098 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5099 | `			"TypeError",` |
|       - | 5100 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5101 | `			nArg` |
|       - | 5102 | `			);` |
|       - | 5103 | `	}` |
|       - | 5104 |  |
|       9 | 5105 | `	if( nArg == 2 ){` |
|       - | 5106 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5107 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5108 | `		return PH7_OK;` |
|       - | 5109 | `	}` |
|       - | 5110 |  |
|       - | 5111 | `	/* Create a new array */` |
|       5 | 5112 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 5113 | `	if( pArray == 0 ){` |
|     ! 0 | 5114 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5115 | `		return PH7_OK;` |
|       - | 5116 | `	}` |
|       - | 5117 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 5118 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5119 | `	/* Perform the intersection */` |
|       5 | 5120 | `	pEntry = pSrc->pFirst;` |
|       5 | 5121 | `	n = pSrc->nEntry;` |
|       8 | 5122 | `	for(;;){` |
|      17 | 5123 | `		if( n < 1 ){` |
|       5 | 5124 | `			break;` |
|       - | 5125 | `		}` |
|       - | 5126 | `		/* Extract the node value */` |
|      13 | 5127 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      13 | 5128 | `		if( pVal ){` |
|      21 | 5129 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      13 | 5130 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5131 | `					/* ignore */` |
|     ! 0 | 5132 | `					continue;` |
|       - | 5133 | `				}` |
|       - | 5134 | `				/* Point to the internal representation of the hashmap */` |
|      13 | 5135 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5136 | `				/* Perform the lookup */` |
|      13 | 5137 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      13 | 5138 | `				if( rc != SXRET_OK ){` |
|       - | 5139 | `					/* Value does not exist */` |
|       5 | 5140 | `					break;` |
|       - | 5141 | `				}` |
|       5 | 5142 | `			}` |
|      13 | 5143 | `			if( i >= (nArg-1) ){` |
|       - | 5144 | `				/* Perform the insertion */` |
|       9 | 5145 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5146 | `			}` |
|       6 | 5147 | `		}` |
|       - | 5148 | `		/* Point to the next entry */` |
|      13 | 5149 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5150 | `		n--;` |
|       1 | 5151 | `	}` |
|       - | 5152 | `	/* Return the freshly created array */` |
|       5 | 5153 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5154 | `	return PH7_OK;` |
|      14 | 5155 |  |
|       - | 5156 | `/*` |
|       - | 5157 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5158 | ` *  Fill an array with values.` |
|       - | 5159 | ` * Parameters` |
|       - | 5160 | ` *  $start_index` |
|       - | 5161 | ` *    The first index of the returned array.` |
|       - | 5162 | ` *  $num` |
|       - | 5163 | ` *   Number of elements to insert.` |
|       - | 5164 | ` *  $value` |
|       - | 5165 | ` *    Value to use for filling.` |
|       - | 5166 | ` * Return` |
|       - | 5167 | ` *  The filled array or null on failure.` |
|       - | 5168 | ` */` |
|     238 | 5169 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5170 |  |
|       - | 5171 | `	ph7_value *pArray;` |
|       - | 5172 | `	int i,nEntry;` |
|       - | 5173 |  |
|       - | 5174 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 5175 | `	if( nArg != 3 ){` |
|       - | 5176 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 5177 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5178 | `			"ArgumentCountError",` |
|       - | 5179 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5180 | `			nArg` |
|       - | 5181 | `			);` |
|       - | 5182 | `	}` |
|       - | 5183 |  |
|       - | 5184 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5185 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5186 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5187 | `	 * and NULLs are rejected outright. */` |
|     466 | 5188 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 5189 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5190 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5191 | `			"TypeError",` |
|       - | 5192 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5193 | `			ph7_type_name(apArg[0])` |
|       - | 5194 | `			);` |
|       - | 5195 | `	}` |
|     234 | 5196 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5197 | `		int len;` |
|       8 | 5198 | `		sxu8 bReal = FALSE;` |
|       8 | 5199 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5200 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5201 | `			/* Non‑numeric string is an error. */` |
|       3 | 5202 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5203 | `				"TypeError",` |
|       - | 5204 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5205 | `				);` |
|       - | 5206 | `		}` |
|       5 | 5207 | `		if( bReal ){` |
|       - | 5208 | `			/* float-string -> deprecation warning */` |
|       4 | 5209 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5210 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5211 | `				zStr` |
|       - | 5212 | `				);` |
|       1 | 5213 | `		}` |
|       2 | 5214 | `	}` |
|       - | 5215 |  |
|       - | 5216 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5217 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 5218 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 5219 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5220 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5221 | `			"TypeError",` |
|       - | 5222 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5223 | `			ph7_type_name(apArg[1])` |
|       - | 5224 | `			);` |
|       - | 5225 | `	}` |
|     232 | 5226 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5227 | `		int len;` |
|       3 | 5228 | `		sxu8 bReal = FALSE;` |
|       3 | 5229 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5230 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5231 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5232 | `				"TypeError",` |
|       - | 5233 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5234 | `				);` |
|       - | 5235 | `		}` |
|     ! 0 | 5236 | `	}` |
|       - | 5237 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5238 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5239 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5240 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5241 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5242 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5243 | `		if( d != (double)i64 ){` |
|       7 | 5244 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5245 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5246 | `				d` |
|       - | 5247 | `				);` |
|       2 | 5248 | `		}` |
|       2 | 5249 | `	}` |
|       - | 5250 |  |
|       - | 5251 | `	/* Total number of entries to insert */` |
|     230 | 5252 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5253 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5254 | `	if( nEntry < 0 ){` |
|       3 | 5255 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5256 | `			"ValueError",` |
|       - | 5257 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5258 | `			);` |
|       - | 5259 | `	}` |
|       - | 5260 |  |
|       - | 5261 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5262 | `	if( nEntry == 0 ){` |
|       7 | 5263 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5264 | `		return PH7_OK;` |
|       - | 5265 | `	}` |
|       - | 5266 |  |
|       - | 5267 | `	/* Create a new array */` |
|     221 | 5268 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5269 | `	if( pArray == 0 ){` |
|     ! 0 | 5270 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5271 | `		return PH7_OK;` |
|       - | 5272 | `	}` |
|       - | 5273 |  |
|       - | 5274 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5275 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 5276 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5277 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5278 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 5279 | `	}` |
|       - | 5280 | `	/* Return the filled array */` |
|     221 | 5281 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5282 | `	return PH7_OK;` |
|     121 | 5283 |  |
|       - | 5284 | `/*` |
|       - | 5285 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5286 | ` *  Fill an array with values, specifying keys.` |
|       - | 5287 | ` * Parameters` |
|       - | 5288 | ` *  $input` |
|       - | 5289 | ` *   Array of values that will be used as key.` |
|       - | 5290 | ` *  $value` |
|       - | 5291 | ` *    Value to use for filling.` |
|       - | 5292 | ` * Return` |
|       - | 5293 | ` *  The filled array.` |
|       - | 5294 | ` * Throws` |
|       - | 5295 | ` *  ValueError if $input is not an array.` |
|       - | 5296 | ` */` |
|      26 | 5297 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5298 |  |
|       - | 5299 | `	ph7_hashmap_node *pEntry;` |
|       - | 5300 | `	ph7_hashmap *pSrc;` |
|       - | 5301 | `	ph7_value *pArray;` |
|       - | 5302 | `	sxu32 n;` |
|       - | 5303 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5304 | `	if( nArg != 2 ){` |
|      10 | 5305 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5306 | `			"ArgumentCountError",` |
|       - | 5307 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5308 | `			nArg` |
|       - | 5309 | `			);` |
|       - | 5310 | `	}` |
|       - | 5311 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5312 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5313 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5314 | `			"TypeError",` |
|       - | 5315 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5316 | `			ph7_type_name(apArg[0])` |
|       - | 5317 | `			);` |
|       - | 5318 | `	}` |
|       - | 5319 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5320 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5321 | `	/* Create a new array */` |
|      17 | 5322 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5323 | `	if( pArray == 0 ){` |
|     ! 0 | 5324 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5325 | `		return PH7_OK;` |
|       - | 5326 | `	}` |
|       - | 5327 | `	/* Perform the requested operation */` |
|      17 | 5328 | `	pEntry = pSrc->pFirst;` |
|      45 | 5329 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5330 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5331 | `		/* Point to the next entry */` |
|      29 | 5332 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5333 | `	}` |
|       - | 5334 | `	/* Return the filled array */` |
|      17 | 5335 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5336 | `	return PH7_OK;` |
|      15 | 5337 |  |
|       - | 5338 | `/*` |
|       - | 5339 | ` * array array_combine(array $keys,array $values)` |
|       - | 5340 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5341 | ` * Parameters` |
|       - | 5342 | ` *  $keys` |
|       - | 5343 | ` *    Array of keys to be used.` |
|       - | 5344 | ` * $values` |
|       - | 5345 | ` *   Array of values to be used.` |
|       - | 5346 | ` * Return` |
|       - | 5347 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5348 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5349 | ` *  not an array.` |
|       - | 5350 | ` */` |
|      18 | 5351 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5352 |  |
|       - | 5353 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5354 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5355 | `	ph7_value *pArray;` |
|       - | 5356 | `	sxu32 n;` |
|       - | 5357 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5358 | `	if( nArg != 2 ){` |
|       - | 5359 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5360 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5361 | `			"ArgumentCountError",` |
|       - | 5362 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5363 | `			nArg` |
|       - | 5364 | `			);` |
|       - | 5365 | `	}` |
|       - | 5366 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5367 | `	 * argument index in the error message. */` |
|      18 | 5368 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5369 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5370 | `			"TypeError",` |
|       - | 5371 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5372 | `			ph7_type_name(apArg[0])` |
|       - | 5373 | `			);` |
|       - | 5374 | `	}` |
|      16 | 5375 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5376 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5377 | `			"TypeError",` |
|       - | 5378 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5379 | `			ph7_type_name(apArg[1])` |
|       - | 5380 | `			);` |
|       - | 5381 | `	}` |
|       - | 5382 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5383 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5384 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5385 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5386 | `		/* Length mismatch -> ValueError */` |
|       3 | 5387 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5388 | `			"ValueError",` |
|       - | 5389 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5390 | `			);` |
|       - | 5391 | `	}` |
|       - | 5392 | `	/* Create a new array */` |
|      11 | 5393 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5394 | `	if( pArray == 0 ){` |
|     ! 0 | 5395 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5396 | `		return PH7_OK;` |
|       - | 5397 | `	}` |
|       - | 5398 | `	/* Perform the requested operation */` |
|      11 | 5399 | `	pKe = pKey->pFirst;` |
|      11 | 5400 | `	pVe = pValue->pFirst;` |
|      33 | 5401 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5402 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5403 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5404 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5405 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5406 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5407 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5408 | `		 * original array must not be mutated. */` |
|      23 | 5409 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5410 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5411 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5412 | `			if( pTmpKey ){` |
|       5 | 5413 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5414 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5415 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5416 | `				pKeyCopy = pTmpKey;` |
|       2 | 5417 | `			}` |
|       2 | 5418 | `		}` |
|      23 | 5419 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5420 | `		/* Point to the next entry */` |
|      23 | 5421 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5422 | `		pVe = pVe->pPrev;` |
|      12 | 5423 | `	}` |
|       - | 5424 | `	/* Return the filled array */` |
|      11 | 5425 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5426 | `	return PH7_OK;` |
|      11 | 5427 |  |
|       - | 5428 | `/*` |
|       - | 5429 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5430 | ` *  Return an array with elements in reverse order.` |
|       - | 5431 | ` * Parameters` |
|       - | 5432 | ` *  $array` |
|       - | 5433 | ` *   The input array.` |
|       - | 5434 | ` *  $preserve_keys (optional)` |
|       - | 5435 | ` *   If set to TRUE keys are preserved.` |
|       - | 5436 | ` * Return` |
|       - | 5437 | ` *  The reversed array.` |
|       - | 5438 | ` */` |
|      20 | 5439 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5440 |  |
|       - | 5441 | `	ph7_hashmap_node *pEntry;` |
|       - | 5442 | `	ph7_hashmap *pSrc;` |
|       - | 5443 | `	ph7_value *pArray;` |
|       - | 5444 | `	int bPreserve;` |
|       - | 5445 | `	sxu32 n;` |
|      22 | 5446 | `	if( nArg < 1 ){` |
|       4 | 5447 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5448 | `			"ArgumentCountError",` |
|       - | 5449 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5450 | `			nArg` |
|       - | 5451 | `			);` |
|       - | 5452 | `	}` |
|       - | 5453 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5454 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5455 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5456 | `			"TypeError",` |
|       - | 5457 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5458 | `			ph7_type_name(apArg[0])` |
|       - | 5459 | `			);` |
|       - | 5460 | `	}` |
|      17 | 5461 | `	bPreserve = FALSE;` |
|      17 | 5462 | `	if( nArg > 1 ){` |
|       7 | 5463 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5464 | `	}` |
|       - | 5465 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5466 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5467 | `	/* Create a new array */` |
|      17 | 5468 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5469 | `	if( pArray == 0 ){` |
|     ! 0 | 5470 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5471 | `		return PH7_OK;` |
|       - | 5472 | `	}` |
|       - | 5473 | `	/* Perform the requested operation */` |
|      17 | 5474 | `	pEntry = pSrc->pLast;` |
|      55 | 5475 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5476 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5477 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5478 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5479 | `		/* Point to the previous entry */` |
|      39 | 5480 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5481 | `	}` |
|      17 | 5482 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5483 | `	return PH7_OK;` |
|      12 | 5484 |  |
|       - | 5485 | `/*` |
|       - | 5486 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5487 | ` *  Removes duplicate values from an array.` |
|       - | 5488 | ` * Parameters` |
|       - | 5489 | ` *  $array` |
|       - | 5490 | ` *   The input array.` |
|       - | 5491 | ` *  $flags` |
|       - | 5492 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5493 | ` *   behavior using these values:` |
|       - | 5494 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5495 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5496 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5497 | ` * Return` |
|       - | 5498 | ` *  The filtered array.` |
|       - | 5499 | ` */` |
|      24 | 5500 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5501 |  |
|       - | 5502 | `	ph7_hashmap_node *pEntry;` |
|       - | 5503 | `	ph7_value *pNeedle;` |
|       - | 5504 | `	ph7_hashmap *pSrc;` |
|       - | 5505 | `	ph7_value *pArray;` |
|       - | 5506 | `	int bStrict;` |
|       - | 5507 | `	sxi32 rc;` |
|       - | 5508 | `	sxu32 n;` |
|      26 | 5509 | `	if( nArg < 1 ){` |
|       - | 5510 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5511 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5512 | `			"ArgumentCountError",` |
|       - | 5513 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5514 | `			);` |
|       - | 5515 | `	}` |
|      24 | 5516 | `	if( nArg > 2 ){` |
|       - | 5517 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5518 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5519 | `			"ArgumentCountError",` |
|       - | 5520 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5521 | `			nArg` |
|       - | 5522 | `			);` |
|       - | 5523 | `	}` |
|       - | 5524 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5525 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5526 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5527 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5528 | `			"TypeError",` |
|       - | 5529 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5530 | `			ph7_type_name(apArg[0])` |
|       - | 5531 | `			);` |
|       - | 5532 | `	}` |
|      19 | 5533 | `	bStrict = FALSE;` |
|       - | 5534 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5535 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5536 | `	/* Create a new array */` |
|      19 | 5537 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5538 | `	if( pArray == 0 ){` |
|     ! 0 | 5539 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5540 | `		return PH7_OK;` |
|       - | 5541 | `	}` |
|       - | 5542 | `	/* Perform the requested operation */` |
|      19 | 5543 | `	pEntry = pSrc->pFirst;` |
|      83 | 5544 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5545 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5546 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5547 | `		if( pNeedle ){` |
|      65 | 5548 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5549 | `		}` |
|      65 | 5550 | `		if( rc != SXRET_OK ){` |
|       - | 5551 | `			/* Perform the insertion */` |
|      37 | 5552 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5553 | `		}` |
|       - | 5554 | `		/* Point to the next entry */` |
|      65 | 5555 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5556 | `	}` |
|       - | 5557 | `	/* Return the freshly created array */` |
|      19 | 5558 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5559 | `	return PH7_OK;` |
|      14 | 5560 |  |
|       - | 5561 | `/*` |
|       - | 5562 | ` * array array_flip(array $input)` |
|       - | 5563 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5564 | ` * Parameter` |
|       - | 5565 | ` *  $input` |
|       - | 5566 | ` *   Input array.` |
|       - | 5567 | ` * Return` |
|       - | 5568 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5569 | ` */` |
|      34 | 5570 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5571 |  |
|       - | 5572 | `	ph7_hashmap_node *pEntry;` |
|       - | 5573 | `	ph7_hashmap *pSrc;` |
|       - | 5574 | `	ph7_value *pArray;` |
|       - | 5575 | `	ph7_value *pKey;` |
|       - | 5576 | `	ph7_value sVal;` |
|       - | 5577 | `	sxu32 n;` |
|       - | 5578 |  |
|       - | 5579 | `	/* PHP requires exactly one argument */` |
|      36 | 5580 | `	if( nArg != 1 ){` |
|       - | 5581 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5582 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5583 | `			"ArgumentCountError",` |
|       - | 5584 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5585 | `			nArg` |
|       - | 5586 | `			);` |
|       - | 5587 | `	}` |
|       - | 5588 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5589 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5590 | `		/* Type mismatch -> TypeError */` |
|       7 | 5591 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5592 | `			"TypeError",` |
|       - | 5593 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5594 | `			ph7_type_name(apArg[0])` |
|       - | 5595 | `			);` |
|       - | 5596 | `	}` |
|       - | 5597 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5598 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5599 | `	/* Create a new array */` |
|      27 | 5600 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5601 | `	if( pArray == 0 ){` |
|     ! 0 | 5602 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5603 | `		return PH7_OK;` |
|       - | 5604 | `	}` |
|       - | 5605 | `	/* Start processing */` |
|      27 | 5606 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5607 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5608 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5609 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5610 | `		if( pKey ){` |
|       - | 5611 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5612 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5613 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5614 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5615 | `					);` |
|   22236 | 5616 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5617 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5618 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5619 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5620 | `				}else{` |
|       - | 5621 | `					SyString sStr;` |
|    2227 | 5622 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5623 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5624 | `				}` |
|       - | 5625 | `				/* Perform the insertion */` |
|   22227 | 5626 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5627 | `				/* Safely release the value because each inserted entry` |
|       - | 5628 | `				 * has its own private copy of the value.` |
|       - | 5629 | `				 */` |
|   22227 | 5630 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5631 | `			}else{` |
|       - | 5632 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5633 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5634 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5635 | `					);` |
|       - | 5636 | `			}` |
|   11118 | 5637 | `		}` |
|       - | 5638 | `		/* Point to the next entry */` |
|   22237 | 5639 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5640 | `	}` |
|       - | 5641 | `	/* Return the freshly created array */` |
|      27 | 5642 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5643 | `	return PH7_OK;` |
|      19 | 5644 |  |
|       - | 5645 | `/*` |
|       - | 5646 | ` * number array_sum(array $array )` |
|       - | 5647 | ` *  Calculate the sum of values in an array.` |
|       - | 5648 | ` * Parameters` |
|       - | 5649 | ` *  $array: The input array.` |
|       - | 5650 | ` * Return` |
|       - | 5651 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5652 | ` */` |
|      24 | 5653 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5654 |  |
|       - | 5655 | `	ph7_hashmap_node *pEntry;` |
|       - | 5656 | `	ph7_value *pObj;` |
|      25 | 5657 | `	double dSum = 0;` |
|       - | 5658 | `	sxu32 n;` |
|      25 | 5659 | `	pEntry = pMap->pFirst;` |
|      91 | 5660 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5661 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5662 | `		if( pObj ){` |
|      67 | 5663 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5664 | `				dSum += pObj->rVal;` |
|      53 | 5665 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5666 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5667 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5668 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5669 | `					double dv = 0;` |
|      13 | 5670 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5671 | `					dSum += dv;` |
|       7 | 5672 | `				}` |
|      12 | 5673 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5674 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5675 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5676 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5677 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5678 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5679 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5680 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5681 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5682 | `			}` |
|       - | 5683 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5684 | `		}` |
|       - | 5685 | `		/* Point to the next entry */` |
|      67 | 5686 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5687 | `	}` |
|       - | 5688 | `	/* Return sum */` |
|      25 | 5689 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5690 |  |
|      22 | 5691 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5692 |  |
|       - | 5693 | `	ph7_hashmap_node *pEntry;` |
|       - | 5694 | `	ph7_value *pObj;` |
|      24 | 5695 | `	sxi64 nSum = 0;` |
|       - | 5696 | `	sxu32 n;` |
|      24 | 5697 | `	pEntry = pMap->pFirst;` |
|      98 | 5698 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      76 | 5699 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      76 | 5700 | `		if( pObj ){` |
|      76 | 5701 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      66 | 5702 | `				nSum += pObj->x.iVal;` |
|      43 | 5703 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5704 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5705 | `					sxi64 nv = 0;` |
|       5 | 5706 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5707 | `					nSum += nv;` |
|       3 | 5708 | `				}` |
|       8 | 5709 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5710 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5711 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5712 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5713 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5714 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5715 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5716 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5717 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5718 | `			}` |
|       - | 5719 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      37 | 5720 | `		}` |
|       - | 5721 | `		/* Point to the next entry */` |
|      76 | 5722 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      39 | 5723 | `	}` |
|       - | 5724 | `	/* Return sum */` |
|      24 | 5725 | `	ph7_result_int64(pCtx,nSum);` |
|      24 | 5726 |  |
|       - | 5727 | `/* number array_sum(array $array )` |
|       - | 5728 | ` * (See block-coment above)` |
|       - | 5729 | ` */` |
|      58 | 5730 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5731 |  |
|       - | 5732 | `	ph7_hashmap_node *pEntry;` |
|       - | 5733 | `	ph7_hashmap *pMap;` |
|       - | 5734 | `	ph7_value *pObj;` |
|      60 | 5735 | `	int useDouble = 0;` |
|       - | 5736 | `	sxu32 n;` |
|       - | 5737 | `	/* PHP requires exactly one argument */` |
|      60 | 5738 | `	if( nArg != 1 ){` |
|       7 | 5739 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5740 | `			"ArgumentCountError",` |
|       - | 5741 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5742 | `			nArg` |
|       - | 5743 | `			);` |
|       - | 5744 | `	}` |
|       - | 5745 | `	/* Make sure we are dealing with a valid hashmap */` |
|      56 | 5746 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5747 | `		/* Type mismatch -> TypeError */` |
|       7 | 5748 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5749 | `			"TypeError",` |
|       - | 5750 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5751 | `			ph7_type_name(apArg[0])` |
|       - | 5752 | `			);` |
|       - | 5753 | `	}` |
|      52 | 5754 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      52 | 5755 | `	if( pMap->nEntry < 1 ){` |
|       - | 5756 | `		/* Nothing to compute,return 0 */` |
|       5 | 5757 | `		ph7_result_int(pCtx,0);` |
|       5 | 5758 | `		return PH7_OK;` |
|       - | 5759 | `	}` |
|       - | 5760 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5761 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5762 | `	 */` |
|      48 | 5763 | `	pEntry = pMap->pFirst;` |
|     130 | 5764 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     108 | 5765 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     108 | 5766 | `		if( pObj ){` |
|     108 | 5767 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5768 | `				useDouble = 1;` |
|      19 | 5769 | `				break;` |
|       - | 5770 | `			}` |
|      90 | 5771 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5772 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5773 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5774 | `				sxu32 i;` |
|      23 | 5775 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5776 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5777 | `						useDouble = 1;` |
|       7 | 5778 | `						break;` |
|       - | 5779 | `					}` |
|       6 | 5780 | `				}` |
|      13 | 5781 | `				if( useDouble ){` |
|       7 | 5782 | `					break;` |
|       - | 5783 | `				}` |
|       3 | 5784 | `			}` |
|      41 | 5785 | `		}` |
|      84 | 5786 | `		pEntry = pEntry->pPrev;` |
|      43 | 5787 | `	}` |
|      48 | 5788 | `	if( useDouble ){` |
|      25 | 5789 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5790 | `	}else{` |
|      24 | 5791 | `		Int64Sum(pCtx,pMap);` |
|       - | 5792 | `	}` |
|      48 | 5793 | `	return PH7_OK;` |
|      31 | 5794 |  |
|       - | 5795 | `/*` |
|       - | 5796 | ` * number array_product(array $array )` |
|       - | 5797 | ` *  Calculate the product of values in an array.` |
|       - | 5798 | ` * Parameters` |
|       - | 5799 | ` *  $array: The input array.` |
|       - | 5800 | ` * Return` |
|       - | 5801 | ` *  Returns the product of values as an integer or float.` |
|       - | 5802 | ` */` |
|     ! 0 | 5803 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5804 |  |
|       - | 5805 | `	ph7_hashmap_node *pEntry;` |
|       - | 5806 | `	ph7_value *pObj;` |
|       - | 5807 | `	double dProd;` |
|       - | 5808 | `	sxu32 n;` |
|     ! 0 | 5809 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5810 | `	dProd = 1;` |
|     ! 0 | 5811 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5812 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5813 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5814 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5815 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5816 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5817 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5818 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5819 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5820 | `					double dv = 0;` |
|     ! 0 | 5821 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5822 | `					dProd *= dv;` |
|     ! 0 | 5823 | `				}` |
|     ! 0 | 5824 | `			}` |
|     ! 0 | 5825 | `		}` |
|       - | 5826 | `		/* Point to the next entry */` |
|     ! 0 | 5827 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5828 | `	}` |
|       - | 5829 | `	/* Return product */` |
|     ! 0 | 5830 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5831 |  |
|     ! 0 | 5832 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5833 |  |
|       - | 5834 | `	ph7_hashmap_node *pEntry;` |
|       - | 5835 | `	ph7_value *pObj;` |
|       - | 5836 | `	sxi64 nProd;` |
|       - | 5837 | `	sxu32 n;` |
|     ! 0 | 5838 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5839 | `	nProd = 1;` |
|     ! 0 | 5840 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5841 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5842 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5843 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5844 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5845 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5846 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5847 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5848 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5849 | `					sxi64 nv = 0;` |
|     ! 0 | 5850 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5851 | `					nProd *= nv;` |
|     ! 0 | 5852 | `				}` |
|     ! 0 | 5853 | `			}` |
|     ! 0 | 5854 | `		}` |
|       - | 5855 | `		/* Point to the next entry */` |
|     ! 0 | 5856 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5857 | `	}` |
|       - | 5858 | `	/* Return product */` |
|     ! 0 | 5859 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5860 |  |
|       - | 5861 | `/* number array_product(array $array )` |
|       - | 5862 | ` * (See block-block comment above)` |
|       - | 5863 | ` */` |
|     ! 0 | 5864 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5865 |  |
|       - | 5866 | `	ph7_hashmap *pMap;` |
|       - | 5867 | `	ph7_value *pObj;` |
|     ! 0 | 5868 | `	if( nArg < 1 ){` |
|       - | 5869 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5870 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5871 | `		return PH7_OK;` |
|       - | 5872 | `	}` |
|       - | 5873 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5874 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5875 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5876 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5877 | `		return PH7_OK;` |
|       - | 5878 | `	}` |
|     ! 0 | 5879 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5880 | `	if( pMap->nEntry < 1 ){` |
|       - | 5881 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5882 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5883 | `		return PH7_OK;` |
|       - | 5884 | `	}` |
|       - | 5885 | `	/* If the first element is of type float,then perform floating` |
|       - | 5886 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5887 | `	 */` |
|     ! 0 | 5888 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5889 | `	if( pObj == 0 ){` |
|     ! 0 | 5890 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5891 | `		return PH7_OK;` |
|       - | 5892 | `	}` |
|     ! 0 | 5893 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5894 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5895 | `	}else{` |
|     ! 0 | 5896 | `		Int64Prod(pCtx,pMap);` |
|       - | 5897 | `	}` |
|     ! 0 | 5898 | `	return PH7_OK;` |
|     ! 0 | 5899 |  |
|       - | 5900 | `/*` |
|       - | 5901 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5902 | ` *  Pick one or more random entries out of an array.` |
|       - | 5903 | ` * Parameters` |
|       - | 5904 | ` * $input` |
|       - | 5905 | ` *  The input array.` |
|       - | 5906 | ` * $num_req` |
|       - | 5907 | ` *  Specifies how many entries you want to pick.` |
|       - | 5908 | ` * Return` |
|       - | 5909 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5910 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5911 | ` *  NULL is returned on failure.` |
|       - | 5912 | ` */` |
|       6 | 5913 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5914 |  |
|       - | 5915 | `	ph7_hashmap_node *pNode;` |
|       - | 5916 | `	ph7_hashmap *pMap;` |
|       7 | 5917 | `	int nItem = 1;` |
|       7 | 5918 | `	if( nArg < 1 ){` |
|       - | 5919 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5920 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5921 | `		return PH7_OK;` |
|       - | 5922 | `	}` |
|       - | 5923 | `	/* Make sure we are dealing with an array */` |
|       7 | 5924 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5925 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5926 | `		return PH7_OK;` |
|       - | 5927 | `	}` |
|       - | 5928 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5929 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5930 | `	if(pMap->nEntry < 1 ){` |
|       - | 5931 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5932 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5933 | `		return PH7_OK;` |
|       - | 5934 | `	}` |
|       7 | 5935 | `	if( nArg > 1 ){` |
|       3 | 5936 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5937 | `	}` |
|       7 | 5938 | `	if( nItem < 2 ){` |
|       - | 5939 | `		sxu32 nEntry;` |
|       - | 5940 | `		/* Select a random number */` |
|       5 | 5941 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5942 | `		/* Extract the desired entry.` |
|       - | 5943 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5944 | `		 */` |
|       5 | 5945 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       2 | 5946 | `			pNode = pMap->pLast;` |
|       2 | 5947 | `			nEntry = pMap->nEntry - nEntry;` |
|       2 | 5948 | `			if( nEntry > 1 ){` |
|     ! 0 | 5949 | `				for(;;){` |
|     ! 0 | 5950 | `					if( nEntry == 0 ){` |
|     ! 0 | 5951 | `						break;` |
|       - | 5952 | `					}` |
|       - | 5953 | `					/* Point to the previous entry */` |
|     ! 0 | 5954 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5955 | `					nEntry--;` |
|     ! 0 | 5956 | `				}` |
|     ! 0 | 5957 | `			}` |
|       1 | 5958 | `		}else{` |
|       4 | 5959 | `			pNode = pMap->pFirst;` |
|       3 | 5960 | `			for(;;){` |
|       5 | 5961 | `				if( nEntry == 0 ){` |
|       4 | 5962 | `					break;` |
|       - | 5963 | `				}` |
|       - | 5964 | `				/* Point to the next entry */` |
|       1 | 5965 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 5966 | `				nEntry--;` |
|     ! 0 | 5967 | `			}` |
|       - | 5968 | `		}` |
|       5 | 5969 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5970 | `			/* Int key */` |
|       3 | 5971 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5972 | `		}else{` |
|       - | 5973 | `			/* Blob key */` |
|       3 | 5974 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5975 | `		}` |
|       3 | 5976 | `	}else{` |
|       - | 5977 | `		ph7_value sKey,*pArray;` |
|       - | 5978 | `		ph7_hashmap *pDest;` |
|       - | 5979 | `		/* Create a new array */` |
|       3 | 5980 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5981 | `		if( pArray == 0 ){` |
|     ! 0 | 5982 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5983 | `			return PH7_OK;` |
|       - | 5984 | `		}` |
|       - | 5985 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5986 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5987 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5988 | `		/* Copy the first n items */` |
|       3 | 5989 | `		pNode = pMap->pFirst;` |
|       3 | 5990 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5991 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5992 | `		}` |
|       7 | 5993 | `		while( nItem > 0){` |
|       5 | 5994 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5995 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5996 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5997 | `			/* Point to the next entry */` |
|       5 | 5998 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5999 | `			nItem--;` |
|       1 | 6000 | `		}` |
|       - | 6001 | `		/* Shuffle the array */` |
|       3 | 6002 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6003 | `		/* Rehash node */` |
|       3 | 6004 | `		HashmapSortRehash(pDest);` |
|       - | 6005 | `		/* Return the random array */` |
|       3 | 6006 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6007 | `	}` |
|       7 | 6008 | `	return PH7_OK;` |
|       4 | 6009 |  |
|       - | 6010 | `/*` |
|       - | 6011 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6012 | ` *  Split an array into chunks.` |
|       - | 6013 | ` * Parameters` |
|       - | 6014 | ` * $input` |
|       - | 6015 | ` *   The array to work on` |
|       - | 6016 | ` * $size` |
|       - | 6017 | ` *   The size of each chunk` |
|       - | 6018 | ` * $preserve_keys` |
|       - | 6019 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6020 | ` *   the chunk numerically.` |
|       - | 6021 | ` * Return` |
|       - | 6022 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6023 | ` *  zero, with each dimension containing size elements.` |
|       - | 6024 | ` */` |
|      42 | 6025 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6026 |  |
|       - | 6027 | `	ph7_value *pArray,*pChunk;` |
|       - | 6028 | `	ph7_hashmap_node *pEntry;` |
|       - | 6029 | `	ph7_hashmap *pMap;` |
|       - | 6030 | `	int bPreserve;` |
|       - | 6031 | `	sxu32 nChunk;` |
|       - | 6032 | `	sxu32 nSize;` |
|       - | 6033 | `	sxu32 n;` |
|       - | 6034 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 6035 | `	if( nArg < 2 ){` |
|       - | 6036 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6037 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6038 | `			"ArgumentCountError",` |
|       - | 6039 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6040 | `			nArg` |
|       - | 6041 | `			);` |
|       - | 6042 | `	}` |
|      42 | 6043 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6044 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6045 | `			"TypeError",` |
|       - | 6046 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6047 | `			ph7_type_name(apArg[0])` |
|       - | 6048 | `			);` |
|       - | 6049 | `	}` |
|       - | 6050 | `	/* Create a new array */` |
|      40 | 6051 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 6052 | `	if( pArray == 0 ){` |
|     ! 0 | 6053 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6054 | `		return PH7_OK;` |
|       - | 6055 | `	}` |
|       - | 6056 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 6057 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6058 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6059 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 6060 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 6061 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6062 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6063 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6064 | `			"TypeError",` |
|       - | 6065 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6066 | `			ph7_type_name(apArg[1])` |
|       - | 6067 | `			);` |
|       - | 6068 | `	}` |
|       - | 6069 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6070 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6071 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 6072 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6073 | `		int len;` |
|       3 | 6074 | `		sxu8 bReal = FALSE;` |
|       3 | 6075 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6076 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6077 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6078 | `				"TypeError",` |
|       - | 6079 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6080 | `				);` |
|       - | 6081 | `		}` |
|     ! 0 | 6082 | `		if( bReal ){` |
|       - | 6083 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6084 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6085 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6086 | `				zStr` |
|       - | 6087 | `				);` |
|     ! 0 | 6088 | `		}` |
|     ! 0 | 6089 | `	}` |
|       - | 6090 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6091 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6092 | `	 * later via ph7_value_to_int. */` |
|      38 | 6093 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6094 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6095 | `		sxi64 i = (sxi64)d;` |
|       3 | 6096 | `		if( d != (double)i ){` |
|       4 | 6097 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6098 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6099 | `				d` |
|       - | 6100 | `				);` |
|       1 | 6101 | `		}` |
|       1 | 6102 | `	}` |
|       - | 6103 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6104 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6105 | `	{` |
|      38 | 6106 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 6107 | `		if( nSizeSigned < 1 ){` |
|       - | 6108 | `			/* size <= 0 -> ValueError */` |
|       5 | 6109 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6110 | `				"ValueError",` |
|       - | 6111 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6112 | `				);` |
|       - | 6113 | `		}` |
|      34 | 6114 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6115 | `	}` |
|      34 | 6116 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6117 | `		/* Return the whole array */` |
|       3 | 6118 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6119 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6120 | `		return PH7_OK;` |
|       - | 6121 | `	}` |
|      32 | 6122 | `	bPreserve = 0;` |
|      32 | 6123 | `	if( nArg > 2 ){` |
|       - | 6124 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6125 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6126 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6127 | `		 * normally, matching PHP behaviour. */` |
|      45 | 6128 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 6129 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6130 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 6131 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6132 | `				"TypeError",` |
|       - | 6133 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6134 | `				ph7_type_name(apArg[2])` |
|       - | 6135 | `				);` |
|       - | 6136 | `		}` |
|      21 | 6137 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6138 | `	}` |
|       - | 6139 | `	/* Start processing */` |
|      27 | 6140 | `	pEntry = pMap->pFirst;` |
|      27 | 6141 | `	nChunk = 0;` |
|      27 | 6142 | `	pChunk = 0;` |
|      27 | 6143 | `	n = pMap->nEntry;` |
|      56 | 6144 | `	for( ;; ){` |
|     113 | 6145 | `		if( n < 1 ){` |
|       - | 6146 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6147 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6148 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6149 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6150 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6151 | `			 * exists. */` |
|      27 | 6152 | `			if( pChunk ){` |
|      27 | 6153 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6154 | `			}` |
|      27 | 6155 | `			break;` |
|       - | 6156 | `		}` |
|      87 | 6157 | `		if( nChunk < 1 ){` |
|      71 | 6158 | `			if( pChunk ){` |
|       - | 6159 | `				/* Put the first chunk */` |
|      45 | 6160 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6161 | `			}` |
|       - | 6162 | `			/* Create a new dimension */` |
|      71 | 6163 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6164 | `												   * will be automatically released as soon we return` |
|       - | 6165 | `												   * from this function */` |
|      71 | 6166 | `			if( pChunk == 0 ){` |
|     ! 0 | 6167 | `				break;` |
|       - | 6168 | `			}` |
|      71 | 6169 | `			nChunk = nSize;` |
|      35 | 6170 | `		}` |
|       - | 6171 | `		/* Insert the entry */` |
|      87 | 6172 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6173 | `		/* Point to the next entry */` |
|      87 | 6174 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6175 | `		nChunk--;` |
|      87 | 6176 | `		n--;` |
|       1 | 6177 | `	}` |
|       - | 6178 | `	/* Return the multidimensional array */` |
|      27 | 6179 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6180 | `	return PH7_OK;` |
|      23 | 6181 |  |
|       - | 6182 | `/*` |
|       - | 6183 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6184 | ` *  Pad array to the specified length with a value.` |
|       - | 6185 | ` * $input` |
|       - | 6186 | ` *   Initial array of values to pad.` |
|       - | 6187 | ` * $pad_size` |
|       - | 6188 | ` *   New size of the array.` |
|       - | 6189 | ` * $pad_value` |
|       - | 6190 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6191 | ` */` |
|      28 | 6192 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6193 |  |
|       - | 6194 | `	ph7_hashmap *pMap;` |
|       - | 6195 | `	ph7_value *pArray;` |
|       - | 6196 | `	int nEntry;` |
|      30 | 6197 | `	if( nArg != 3 ){` |
|      10 | 6198 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6199 | `			"ArgumentCountError",` |
|       - | 6200 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6201 | `			nArg` |
|       - | 6202 | `			);` |
|       - | 6203 | `	}` |
|      24 | 6204 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6205 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6206 | `			"TypeError",` |
|       - | 6207 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6208 | `			ph7_type_name(apArg[0])` |
|       - | 6209 | `			);` |
|       - | 6210 | `	}` |
|       - | 6211 | `	/* Create a new array */` |
|      21 | 6212 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6213 | `	if( pArray == 0 ){` |
|     ! 0 | 6214 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6215 | `		return PH7_OK;` |
|       - | 6216 | `	}` |
|       - | 6217 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6218 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6219 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6220 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6221 | `	if( nEntry < 0 ){` |
|       9 | 6222 | `		nEntry = -nEntry;` |
|       9 | 6223 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6224 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6225 | `			/* Insert given items first */` |
|      17 | 6226 | `			while( nEntry > 0 ){` |
|      13 | 6227 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 6228 | `				nEntry--;` |
|       1 | 6229 | `			}` |
|       - | 6230 | `			/* Merge the two arrays */` |
|       5 | 6231 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6232 | `		}else{` |
|       5 | 6233 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6234 | `		}` |
|      17 | 6235 | `	}else if( nEntry > 0 ){` |
|      11 | 6236 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6237 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6238 | `			/* Merge the two arrays first */` |
|       7 | 6239 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6240 | `			/* Insert given items */` |
|      25 | 6241 | `			while( nEntry > 0 ){` |
|      19 | 6242 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 6243 | `				nEntry--;` |
|       1 | 6244 | `			}` |
|       4 | 6245 | `		}else{` |
|       5 | 6246 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6247 | `		}` |
|       6 | 6248 | `	}else{` |
|       - | 6249 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6250 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6251 | `	}` |
|       - | 6252 | `	/* Return the new array */` |
|      21 | 6253 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6254 | `	return PH7_OK;` |
|      16 | 6255 |  |
|       - | 6256 | `/*` |
|       - | 6257 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6258 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6259 | ` * Parameters` |
|       - | 6260 | ` * $array` |
|       - | 6261 | ` *   The array in which elements are replaced.` |
|       - | 6262 | ` * $array1` |
|       - | 6263 | ` *   The array from which elements will be extracted.` |
|       - | 6264 | ` * ....` |
|       - | 6265 | ` *  More arrays from which elements will be extracted.` |
|       - | 6266 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6267 | ` * Return` |
|       - | 6268 | ` *  Returns an array.` |
|       - | 6269 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6270 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6271 | ` */` |
|      22 | 6272 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6273 |  |
|       - | 6274 | `	ph7_hashmap *pMap;` |
|       - | 6275 | `	ph7_value *pArray;` |
|       - | 6276 | `	int i;` |
|      24 | 6277 | `	if( nArg < 1 ){` |
|       3 | 6278 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6279 | `			"ArgumentCountError",` |
|       - | 6280 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6281 | `			);` |
|       - | 6282 | `	}` |
|      22 | 6283 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6284 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6285 | `			"TypeError",` |
|       - | 6286 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6287 | `			ph7_type_name(apArg[0])` |
|       - | 6288 | `			);` |
|       - | 6289 | `	}` |
|       - | 6290 | `	/* Create a new array */` |
|      20 | 6291 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6292 | `	if( pArray == 0 ){` |
|     ! 0 | 6293 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6294 | `		return PH7_OK;` |
|       - | 6295 | `	}` |
|       - | 6296 | `	/* Overwrite from the first array */` |
|      20 | 6297 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6298 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6299 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6300 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6301 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6302 | `			/* Type mismatch -> TypeError */` |
|       4 | 6303 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6304 | `				"TypeError",` |
|       - | 6305 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6306 | `				i + 1,` |
|       2 | 6307 | `				ph7_type_name(apArg[i])` |
|       - | 6308 | `				);` |
|       - | 6309 | `		}` |
|       - | 6310 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6311 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6312 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6313 | `	}` |
|       - | 6314 | `	/* Return the new array */` |
|      17 | 6315 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6316 | `	return PH7_OK;` |
|      13 | 6317 |  |
|       - | 6318 | `/*` |
|       - | 6319 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6320 | ` *  Filters elements of an array using a callback function.` |
|       - | 6321 | ` * Parameters` |
|       - | 6322 | ` *  $input` |
|       - | 6323 | ` *    The array to iterate over` |
|       - | 6324 | ` * $callback` |
|       - | 6325 | ` *    The callback function to use` |
|       - | 6326 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6327 | ` *    will be removed.` |
|       - | 6328 | ` * Return` |
|       - | 6329 | ` *  The filtered array.` |
|       - | 6330 | ` */` |
|      18 | 6331 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6332 |  |
|       - | 6333 | `	ph7_hashmap_node *pEntry;` |
|       - | 6334 | `	ph7_hashmap *pMap;` |
|       - | 6335 | `	ph7_value *pArray;` |
|       - | 6336 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6337 | `	ph7_value *pValue;` |
|       - | 6338 | `	sxi32 rc;` |
|       - | 6339 | `	int keep;` |
|       - | 6340 | `	sxu32 n;` |
|      20 | 6341 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6342 | `		/* Invalid arguments,return NULL */` |
|       5 | 6343 | `		ph7_result_null(pCtx);` |
|       5 | 6344 | `		return PH7_OK;` |
|       - | 6345 | `	}` |
|       - | 6346 | `	/* Create a new array */` |
|      16 | 6347 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 6348 | `	if( pArray == 0 ){` |
|     ! 0 | 6349 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6350 | `		return PH7_OK;` |
|       - | 6351 | `	}` |
|       - | 6352 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 6353 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 6354 | `	pEntry = pMap->pFirst;` |
|      16 | 6355 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 6356 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6357 | `	/* Perform the requested operation */` |
|      66 | 6358 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6359 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 6360 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 6361 | `		if( pValue == 0 ){` |
|       - | 6362 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6363 | `			keep = FALSE;` |
|      54 | 6364 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6365 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6366 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6367 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 6368 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6369 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6370 | `					int len;` |
|       3 | 6371 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6372 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6373 | `						"TypeError",` |
|       - | 6374 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6375 | `						zName` |
|       - | 6376 | `						);` |
|     ! 0 | 6377 | `				}else{` |
|     ! 0 | 6378 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6379 | `						"TypeError",` |
|       - | 6380 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6381 | `						ph7_type_name(apArg[1])` |
|       - | 6382 | `						);` |
|       - | 6383 | `				}` |
|       - | 6384 | `			}` |
|      23 | 6385 | `			keep = FALSE;` |
|      23 | 6386 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 6387 | `			if( rc == SXRET_OK ){` |
|       - | 6388 | `				/* Perform a boolean cast */` |
|      23 | 6389 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6390 | `			}` |
|      23 | 6391 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6392 | `		}else{` |
|       - | 6393 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6394 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6395 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6396 | `			 */` |
|      29 | 6397 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6398 | `		}` |
|      51 | 6399 | `		if( keep ){` |
|       - | 6400 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6401 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6402 | `		}` |
|       - | 6403 | `		/* Point to the next entry */` |
|      51 | 6404 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6405 | `	}` |
|      13 | 6406 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6407 | `	return PH7_OK;` |
|      11 | 6408 |  |
|       - | 6409 | `/*` |
|       - | 6410 | ` * array array_map(?callable $callback, array $array)` |
|       - | 6411 | ` *  Applies the callback to the elements of the given array.` |
|       - | 6412 | ` * Parameters` |
|       - | 6413 | ` *  $callback` |
|       - | 6414 | ` *   A callable to run for each element in the array, or NULL for the` |
|       - | 6415 | ` *   identity function (returns the array unchanged).` |
|       - | 6416 | ` *  $array` |
|       - | 6417 | ` *   An array to run through the callback function.` |
|       - | 6418 | ` * Return` |
|       - | 6419 | ` *  Returns an array containing the results of applying the callback` |
|       - | 6420 | ` *  function to each element of $array.` |
|       - | 6421 | ` */` |
|      30 | 6422 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6423 |  |
|       - | 6424 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6425 | `	ph7_hashmap_node *pEntry;` |
|       - | 6426 | `	ph7_hashmap *pMap;` |
|       - | 6427 | `	int bNullCallback;` |
|       - | 6428 | `	sxu32 n;` |
|      32 | 6429 | `	if( nArg < 2 ){` |
|       7 | 6430 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6431 | `			"ArgumentCountError",` |
|       - | 6432 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6433 | `			nArg` |
|       - | 6434 | `			);` |
|       - | 6435 | `	}` |
|      28 | 6436 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 6437 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6438 | `			"TypeError",` |
|       - | 6439 | `			"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6440 | `			ph7_type_name(apArg[1])` |
|       - | 6441 | `			);` |
|       - | 6442 | `	}` |
|      26 | 6443 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      26 | 6444 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6445 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6446 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6447 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6448 | `				"TypeError",` |
|       - | 6449 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6450 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6451 | `				zFunc` |
|       - | 6452 | `				);` |
|       - | 6453 | `		}` |
|       3 | 6454 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6455 | `			"TypeError",` |
|       - | 6456 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6457 | `			"no array or string given"` |
|       - | 6458 | `			);` |
|       - | 6459 | `	}` |
|       - | 6460 | `	/* Create a new array */` |
|      21 | 6461 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6462 | `	if( pArray == 0 ){` |
|     ! 0 | 6463 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6464 | `		return PH7_OK;` |
|       - | 6465 | `	}` |
|       - | 6466 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6467 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      21 | 6468 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      21 | 6469 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 6470 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      21 | 6471 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 6472 | `	/* Perform the requested operation */` |
|      21 | 6473 | `	pEntry = pMap->pFirst;` |
|      61 | 6474 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6475 | `		/* Extract the node value */` |
|      41 | 6476 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      41 | 6477 | `		if( pValue ){` |
|       - | 6478 | `			/* Extract the node key */` |
|      41 | 6479 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      41 | 6480 | `			if( bNullCallback ){` |
|       - | 6481 | `				/* NULL callback: identity function, keep original value */` |
|      11 | 6482 | `				ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6483 | `			}else{` |
|       - | 6484 | `				/* Invoke the supplied callback */` |
|      31 | 6485 | `				PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 6486 | `				/* Insert the callback return value */` |
|      31 | 6487 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6488 | `			}` |
|      41 | 6489 | `			PH7_MemObjRelease(&sKey);` |
|      41 | 6490 | `			PH7_MemObjRelease(&sResult);` |
|      20 | 6491 | `		}` |
|       - | 6492 | `		/* Point to the next entry */` |
|      41 | 6493 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6494 | `	}` |
|      21 | 6495 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6496 | `	return PH7_OK;` |
|      17 | 6497 |  |
|       - | 6498 | `/*` |
|       - | 6499 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6500 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6501 | ` * Parameters` |
|       - | 6502 | ` *  $array` |
|       - | 6503 | ` *   The input array.` |
|       - | 6504 | ` *  $callback` |
|       - | 6505 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6506 | ` *  $initial` |
|       - | 6507 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6508 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6509 | ` * Return` |
|       - | 6510 | ` *  Returns the resulting value.` |
|       - | 6511 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6512 | ` */` |
|      30 | 6513 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6514 |  |
|       - | 6515 | `	ph7_hashmap_node *pEntry;` |
|       - | 6516 | `	ph7_hashmap *pMap;` |
|       - | 6517 | `	ph7_value *pValue;` |
|       - | 6518 | `	ph7_value sResult;` |
|       - | 6519 | `	sxu32 n;` |
|      32 | 6520 | `	if( nArg < 2 ){` |
|       7 | 6521 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6522 | `			"ArgumentCountError",` |
|       - | 6523 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6524 | `			nArg` |
|       - | 6525 | `			);` |
|       - | 6526 | `	}` |
|      28 | 6527 | `	if( nArg > 3 ){` |
|       4 | 6528 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6529 | `			"ArgumentCountError",` |
|       - | 6530 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6531 | `			nArg` |
|       - | 6532 | `			);` |
|       - | 6533 | `	}` |
|      26 | 6534 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6535 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6536 | `			"TypeError",` |
|       - | 6537 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6538 | `			ph7_type_name(apArg[0])` |
|       - | 6539 | `			);` |
|       - | 6540 | `	}` |
|      24 | 6541 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6542 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6543 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6544 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6545 | `				"TypeError",` |
|       - | 6546 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6547 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6548 | `				zFunc` |
|       - | 6549 | `				);` |
|       - | 6550 | `		}` |
|       7 | 6551 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6552 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6553 | `				"TypeError",` |
|       - | 6554 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6555 | `				"array callback must have exactly two members"` |
|       - | 6556 | `				);` |
|       - | 6557 | `		}` |
|       5 | 6558 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6559 | `			"TypeError",` |
|       - | 6560 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6561 | `			"no array or string given"` |
|       - | 6562 | `			);` |
|       - | 6563 | `	}` |
|       - | 6564 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 6565 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6566 | `	/* Assume a NULL initial value */` |
|      15 | 6567 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      15 | 6568 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      15 | 6569 | `	if( nArg > 2 ){` |
|       - | 6570 | `		/* Set the initial value */` |
|      11 | 6571 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6572 | `	}` |
|       - | 6573 | `	/* Perform the requested operation */` |
|      15 | 6574 | `	pEntry = pMap->pFirst;` |
|      43 | 6575 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6576 | `		/* Extract the node value */` |
|      29 | 6577 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6578 | `		/* Invoke the supplied callback */` |
|      29 | 6579 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6580 | `		/* Point to the next entry */` |
|      29 | 6581 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6582 | `	}` |
|      15 | 6583 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6584 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6585 | `	return PH7_OK;` |
|      17 | 6586 |  |
|       - | 6587 | `/*` |
|       - | 6588 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6589 | ` *  Apply a user function to every member of an array.` |
|       - | 6590 | ` * Parameters` |
|       - | 6591 | ` *  $array` |
|       - | 6592 | ` *   The input array.` |
|       - | 6593 | ` *  $funcname` |
|       - | 6594 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6595 | ` *   the first, and the key/index second.` |
|       - | 6596 | ` * Note:` |
|       - | 6597 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6598 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6599 | ` *  be made in the original array itself.` |
|       - | 6600 | ` *  $userdata` |
|       - | 6601 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6602 | ` *   to the callback funcname.` |
|       - | 6603 | ` * Return` |
|       - | 6604 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6605 | ` */` |
|      36 | 6606 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6607 |  |
|       - | 6608 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6609 | `	ph7_hashmap_node *pEntry;` |
|       - | 6610 | `	ph7_hashmap *pMap;` |
|       - | 6611 | `	sxu32 n;` |
|      38 | 6612 | `	if( nArg < 2 ){` |
|       7 | 6613 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6614 | `			"ArgumentCountError",` |
|       - | 6615 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6616 | `			nArg` |
|       - | 6617 | `			);` |
|       - | 6618 | `	}` |
|      34 | 6619 | `	if( nArg > 3 ){` |
|       4 | 6620 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6621 | `			"ArgumentCountError",` |
|       - | 6622 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6623 | `			nArg` |
|       - | 6624 | `			);` |
|       - | 6625 | `	}` |
|      32 | 6626 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6627 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6628 | `			"TypeError",` |
|       - | 6629 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6630 | `			ph7_type_name(apArg[0])` |
|       - | 6631 | `			);` |
|       - | 6632 | `	}` |
|      30 | 6633 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6634 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6635 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6636 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6637 | `				"TypeError",` |
|       - | 6638 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6639 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6640 | `				zFunc` |
|       - | 6641 | `				);` |
|       - | 6642 | `		}` |
|       9 | 6643 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6644 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6645 | `				"TypeError",` |
|       - | 6646 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6647 | `				"array callback must have exactly two members"` |
|       - | 6648 | `				);` |
|       - | 6649 | `		}` |
|       5 | 6650 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6651 | `			"TypeError",` |
|       - | 6652 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6653 | `			"no array or string given"` |
|       - | 6654 | `			);` |
|       - | 6655 | `	}` |
|      19 | 6656 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6657 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6658 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      19 | 6659 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      19 | 6660 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6661 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6662 | `	/* Perform the desired operation */` |
|      19 | 6663 | `	pEntry = pMap->pFirst;` |
|      59 | 6664 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6665 | `		/* Extract the node value */` |
|      41 | 6666 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      41 | 6667 | `		if( pValue ){` |
|       - | 6668 | `			/* Extract the entry key */` |
|      41 | 6669 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6670 | `			/* Invoke the supplied callback */` |
|      41 | 6671 | `			PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      41 | 6672 | `			PH7_MemObjRelease(&sKey);` |
|      20 | 6673 | `		}` |
|       - | 6674 | `		/* Point to the next entry */` |
|      41 | 6675 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6676 | `	}` |
|       - | 6677 | `	/* All done, return TRUE */` |
|      19 | 6678 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6679 | `	return PH7_OK;` |
|      20 | 6680 |  |
|       - | 6681 | `/*` |
|       - | 6682 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6683 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6684 | ` */` |
|      22 | 6685 | `static void HashmapWalkRecursive(` |
|       - | 6686 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6687 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6688 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6689 | `	int iNest             /* Nesting level */` |
|       - | 6690 | `	)` |
|       1 | 6691 |  |
|       - | 6692 | `	ph7_hashmap_node *pEntry;` |
|       - | 6693 | `	ph7_value *pValue,sKey;` |
|       - | 6694 | `	sxu32 n;` |
|       - | 6695 | `	/* Iterate through hashmap entries */` |
|      23 | 6696 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6697 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6698 | `	pEntry = pMap->pFirst;` |
|      59 | 6699 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6700 | `		/* Extract the node value */` |
|      37 | 6701 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6702 | `		if( pValue ){` |
|      37 | 6703 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6704 | `				if( iNest < 32 ){` |
|       - | 6705 | `					/* Recurse */` |
|      11 | 6706 | `					iNest++;` |
|      11 | 6707 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6708 | `					iNest--;` |
|       5 | 6709 | `				}` |
|       6 | 6710 | `			}else{` |
|       - | 6711 | `				/* Extract the node key */` |
|      27 | 6712 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6713 | `				/* Invoke the supplied callback */` |
|      27 | 6714 | `				PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6715 | `				PH7_MemObjRelease(&sKey);` |
|       - | 6716 | `			}` |
|      18 | 6717 | `		}` |
|       - | 6718 | `		/* Point to the next entry */` |
|      37 | 6719 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6720 | `	}` |
|      23 | 6721 |  |
|       - | 6722 | `/*` |
|       - | 6723 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6724 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6725 | ` * Parameters` |
|       - | 6726 | ` *  $array` |
|       - | 6727 | ` *   The input array.` |
|       - | 6728 | ` *  $funcname` |
|       - | 6729 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6730 | ` *   the first, and the key/index second.` |
|       - | 6731 | ` * Note:` |
|       - | 6732 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6733 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6734 | ` *  be made in the original array itself.` |
|       - | 6735 | ` *  $userdata` |
|       - | 6736 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6737 | ` *   to the callback funcname.` |
|       - | 6738 | ` * Return` |
|       - | 6739 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6740 | ` */` |
|      30 | 6741 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6742 |  |
|       - | 6743 | `	ph7_hashmap *pMap;` |
|      32 | 6744 | `	if( nArg < 2 ){` |
|       7 | 6745 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6746 | `			"ArgumentCountError",` |
|       - | 6747 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 6748 | `			nArg` |
|       - | 6749 | `			);` |
|       - | 6750 | `	}` |
|      28 | 6751 | `	if( nArg > 3 ){` |
|       4 | 6752 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6753 | `			"ArgumentCountError",` |
|       - | 6754 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 6755 | `			nArg` |
|       - | 6756 | `			);` |
|       - | 6757 | `	}` |
|      26 | 6758 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6759 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6760 | `			"TypeError",` |
|       - | 6761 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6762 | `			ph7_type_name(apArg[0])` |
|       - | 6763 | `			);` |
|       - | 6764 | `	}` |
|      24 | 6765 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6766 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6767 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6768 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6769 | `				"TypeError",` |
|       - | 6770 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6771 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6772 | `				zFunc` |
|       - | 6773 | `				);` |
|       - | 6774 | `		}` |
|       9 | 6775 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6776 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6777 | `				"TypeError",` |
|       - | 6778 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6779 | `				"array callback must have exactly two members"` |
|       - | 6780 | `				);` |
|       - | 6781 | `		}` |
|       5 | 6782 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6783 | `			"TypeError",` |
|       - | 6784 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6785 | `			"no array or string given"` |
|       - | 6786 | `			);` |
|       - | 6787 | `	}` |
|       - | 6788 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6789 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 6790 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6791 | `	/* Perform the desired operation */` |
|      13 | 6792 | `	HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6793 | `	/* All done, return TRUE */` |
|      13 | 6794 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6795 | `	return PH7_OK;` |
|      17 | 6796 |  |
|       - | 6797 | `/*` |
|       - | 6798 | ` * Table of hashmap functions.` |
|       - | 6799 | ` */` |
|       - | 6800 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6801 | `	{"count",             ph7_hashmap_count },` |
|       - | 6802 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6803 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6804 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6805 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6806 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6807 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6808 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6809 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6810 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6811 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6812 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6813 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6814 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6815 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6816 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6817 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6818 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6819 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6820 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6821 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6822 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6823 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6824 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6825 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6826 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6827 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6828 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6829 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6830 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6831 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6832 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6833 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6834 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6835 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6836 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6837 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6838 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6839 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6840 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6841 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6842 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6843 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6844 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6845 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6846 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6847 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6848 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6849 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6850 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6851 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6852 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6853 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6854 | `	{"current",           ph7_hashmap_current },` |
|       - | 6855 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6856 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6857 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6858 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6859 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6860 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6861 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6862 | `};` |
|       - | 6863 | `/*` |
|       - | 6864 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6865 | ` */` |
|    2622 | 6866 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6867 |  |
|       - | 6868 | `	sxu32 n;` |
|  162566 | 6869 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  159944 | 6870 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   79973 | 6871 | `	}` |
|    2624 | 6872 |  |
|       - | 6873 | `/*` |
|       - | 6874 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6875 | ` * the BLOB given as the first argument.` |
|       - | 6876 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6877 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6878 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6879 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6880 | ` */` |
|      26 | 6881 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6882 |  |
|       - | 6883 | `	ph7_hashmap_node *pEntry;` |
|       - | 6884 | `	ph7_value *pObj;` |
|      28 | 6885 | `	sxu32 n = 0;` |
|       - | 6886 | `	int isRef;` |
|       - | 6887 | `	sxi32 rc;` |
|       - | 6888 | `	int i;` |
|      28 | 6889 | `	if( nDepth > 31 ){` |
|       - | 6890 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6891 | `		/* Nesting limit reached */` |
|     ! 0 | 6892 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6893 | `		if( ShowType ){` |
|     ! 0 | 6894 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6895 | `		}` |
|     ! 0 | 6896 | `		return SXERR_LIMIT;` |
|       - | 6897 | `	}` |
|       - | 6898 | `	/* Point to the first inserted entry */` |
|      28 | 6899 | `	pEntry = pMap->pFirst;` |
|      28 | 6900 | `	rc = SXRET_OK;` |
|      28 | 6901 | `	if( !ShowType ){` |
|      15 | 6902 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6903 | `	}` |
|       - | 6904 | `	/* Total entries */` |
|      28 | 6905 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6906 | `#ifdef __WINNT__` |
|       2 | 6907 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6908 | `#else` |
|      26 | 6909 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6910 | `#endif` |
|      62 | 6911 | `	for(;;){` |
|     126 | 6912 | `		if( n >= pMap->nEntry ){` |
|      28 | 6913 | `			break;` |
|       - | 6914 | `		}` |
|     198 | 6915 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 6916 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 6917 | `		}` |
|       - | 6918 | `		/* Dump key */` |
|     100 | 6919 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 6920 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 6921 | `		}else{` |
|     101 | 6922 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6923 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6924 | `		}` |
|       - | 6925 | `#ifdef __WINNT__` |
|       2 | 6926 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6927 | `#else` |
|      98 | 6928 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6929 | `#endif` |
|       - | 6930 | `		/* Dump node value */` |
|     100 | 6931 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 6932 | `		isRef = 0;` |
|     100 | 6933 | `		if( pObj ){` |
|     100 | 6934 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6935 | `				/* Referenced object */` |
|     ! 0 | 6936 | `				isRef = 1;` |
|     ! 0 | 6937 | `			}` |
|     100 | 6938 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 6939 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6940 | `				break;` |
|       - | 6941 | `			}` |
|      49 | 6942 | `		}` |
|       - | 6943 | `		/* Point to the next entry */` |
|     100 | 6944 | `		n++;` |
|     100 | 6945 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6946 | `	}` |
|      54 | 6947 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 6948 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 6949 | `	}` |
|      28 | 6950 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 6951 | `	return rc;` |
|      15 | 6952 |  |
|       - | 6953 | `/*` |
|       - | 6954 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6955 | ` * retrieved entry.` |
|       - | 6956 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6957 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6958 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6959 | ` * a value different from PH7_OK.` |
|       - | 6960 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6961 | ` */` |
|   27730 | 6962 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6963 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6964 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6965 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6966 | `	)` |
|       2 | 6967 |  |
|       - | 6968 | `	ph7_hashmap_node *pEntry;` |
|       - | 6969 | `	ph7_value sKey,sValue;` |
|       - | 6970 | `	sxi32 rc;` |
|       - | 6971 | `	sxu32 n;` |
|       - | 6972 | `	/* Initialize walker parameter */` |
|   27732 | 6973 | `	rc = SXRET_OK;` |
|   27732 | 6974 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   27732 | 6975 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   27732 | 6976 | `	n = pMap->nEntry;` |
|   27732 | 6977 | `	pEntry = pMap->pFirst;` |
|       - | 6978 | `	/* Start the iteration process */` |
|   69545 | 6979 | `	for(;;){` |
|  139092 | 6980 | `		if( n < 1 ){` |
|   27732 | 6981 | `			break;` |
|       - | 6982 | `		}` |
|       - | 6983 | `		/* Extract a copy of the key and a copy the current value */` |
|  111362 | 6984 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  111362 | 6985 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6986 | `		/* Invoke the user callback */` |
|  111362 | 6987 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6988 | `		/* Release the copy of the key and the value */` |
|  111362 | 6989 | `		PH7_MemObjRelease(&sKey);` |
|  111362 | 6990 | `		PH7_MemObjRelease(&sValue);` |
|  111362 | 6991 | `		if( rc != PH7_OK ){` |
|       - | 6992 | `			/* Callback request an operation abort */` |
|     ! 0 | 6993 | `			return SXERR_ABORT;` |
|       - | 6994 | `		}` |
|       - | 6995 | `		/* Point to the next entry */` |
|  111362 | 6996 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  111362 | 6997 | `		n--;` |
|       2 | 6998 | `	}` |
|       - | 6999 | `	/* All done */` |
|   27732 | 7000 | `	return SXRET_OK;` |
|   13867 | 7001 |  |
|       - | 7002 |  |
