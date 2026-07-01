# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3338/3831 lines (87.13%)

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
| 3109608 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       5 |   19 |  |
| 3109613 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       5 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  380832 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       5 |   26 |  |
|  380837 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  380837 |   29 | `	sxu32 nH = 5381;` |
|  380837 |   30 | `	zEnd = &zIn[nLen];` |
|  432934 |   31 | `	for(;;){` |
|  865873 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  747707 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  672829 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  573281 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       5 |   36 | `	}` |
|  380837 |   37 | `	return nH;` |
|       5 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     934 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       5 |   47 |  |
|     939 |   48 | `	sxi64 iCount = 0;` |
|     939 |   49 | `	if( !bRecursive ){` |
|     765 |   50 | `		iCount = pMap->nEntry;` |
|     385 |   51 | `	}else{` |
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
|     939 |   86 | `	return iCount;` |
|       5 |   87 |  |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 3048964 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       5 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 3048969 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 3048969 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 3048969 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 3048969 |  104 | `	pNode->pMap  = &(*pMap);` |
| 3048969 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 3048969 |  106 | `	pNode->nHash = nHash;` |
| 3048969 |  107 | `	pNode->xKey.iKey = iKey;` |
| 3048969 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 3048969 |  109 | `	return pNode;` |
| 1524487 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|  137192 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       5 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|  137197 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  137197 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|  137197 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|  137197 |  127 | `	pNode->pMap  = &(*pMap);` |
|  137197 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  137197 |  129 | `	pNode->nHash = nHash;` |
|  137197 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  137197 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  137197 |  132 | `	pNode->nValIdx = nValIdx;` |
|  137197 |  133 | `	return pNode;` |
|   68601 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 3186156 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       5 |  139 |  |
|       - |  140 | `	/* Link */` |
| 3186161 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2834049 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2834049 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1417022 |  144 | `	}` |
| 3186161 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 3186161 |  147 | `	if( pMap->pFirst == 0 ){` |
|   63655 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   63655 |  150 | `		pMap->pCur = pNode;` |
|   31830 |  151 | `	}else{` |
| 3122511 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 3186161 |  154 | `	++pMap->nEntry;` |
| 3186161 |  155 |  |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    7240 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       5 |  161 |  |
|    7245 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    7245 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    7245 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    6783 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3394 |  167 | `	}else{` |
|     463 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    7245 |  170 | `	if( pNode->pNextCollide ){` |
|    5735 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2867 |  172 | `	}` |
|    7245 |  173 | `	if( pMap->pFirst == pNode ){` |
|     131 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      63 |  175 | `	}` |
|    7245 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|     133 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      64 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    7245 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    7245 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     107 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     107 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     107 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  188 | `		}` |
|      51 |  189 | `	}` |
|    7245 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    7110 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3553 |  192 | `	}` |
|    7245 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    7245 |  194 | `	pMap->nEntry--;` |
|    7245 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      75 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      75 |  198 | `		pMap->apBucket = 0;` |
|      75 |  199 | `		pMap->nSize = 0;` |
|      75 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      35 |  201 | `	}` |
|    7245 |  202 |  |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 3186156 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       5 |  208 |  |
| 3186161 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   68245 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   68245 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   68245 |  215 | `		if( nNew < 1 ){` |
|   63655 |  216 | `			nNew = 16;` |
|   31825 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   68245 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   68245 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   68245 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   68245 |  230 | `		pMap->apBucket = apNew;` |
|   68245 |  231 | `		pMap->nSize = nNew;` |
|   68245 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   63655 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    4595 |  237 | `		pEntry = pMap->pFirst;` |
|    4595 |  238 | `		n = 0;` |
| 2068503 |  239 | `		for( ;; ){` |
| 4137011 |  240 | `			if( n >= pMap->nEntry ){` |
|    4595 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 4132421 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 4132421 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4132421 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3551871 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3551871 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1775933 |  250 | `			}` |
| 4132421 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 4132421 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4132421 |  254 | `			n++;` |
|       5 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    4595 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2295 |  258 | `	}` |
| 3122511 |  259 | `	return SXRET_OK;` |
| 1593083 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 3048964 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 3048969 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		ph7_value sSafeVal;` |
|       - |  274 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  275 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  276 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  277 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  278 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  279 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
| 3048933 |  280 | `		if( pValue ){` |
| 3048931 |  281 | `			sSafeVal = *pValue;` |
| 3048931 |  282 | `			pValue = &sSafeVal;` |
| 1524463 |  283 | `		}` |
|       - |  284 | `		/* Reserve a ph7_value for the value */` |
| 3048933 |  285 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 3048933 |  286 | `		if( pObj == 0 ){` |
|     ! 0 |  287 | `			return SXERR_MEM;` |
|       - |  288 | `		}` |
| 3048933 |  289 | `		if( pValue ){` |
|       - |  290 | `			/* Duplicate the value */` |
| 3048931 |  291 | `			PH7_MemObjStore(pValue,pObj);` |
| 1524463 |  292 | `		}` |
| 3048933 |  293 | `		nIdx = pObj->nIdx;` |
| 1524469 |  294 | `	}else{` |
|      37 |  295 | `		nIdx = nRefIdx;` |
|       - |  296 | `	}` |
|       - |  297 | `	/* Hash the key */` |
| 3048969 |  298 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  299 | `	/* Allocate a new int node */` |
| 3048969 |  300 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 3048969 |  301 | `	if( pNode == 0 ){` |
|     ! 0 |  302 | `		return SXERR_MEM;` |
|       - |  303 | `	}` |
| 3048969 |  304 | `	if( isForeign ){` |
|       - |  305 | `		/* Mark as a foregin entry */` |
|      37 |  306 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      18 |  307 | `	}` |
|       - |  308 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 3048969 |  309 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 3048969 |  310 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  311 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  312 | `		return rc;` |
|       - |  313 | `	}` |
|       - |  314 | `	/* Perform the insertion */` |
| 3048969 |  315 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  316 | `	/* Install in the reference table */` |
| 3048969 |  317 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  318 | `	/* All done */` |
| 3048969 |  319 | `	return SXRET_OK;` |
| 1524487 |  320 |  |
|       - |  321 | `/*` |
|       - |  322 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  323 | ` * hashmap.` |
|       - |  324 | ` */` |
|  137192 |  325 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  326 |  |
|       - |  327 | `	ph7_hashmap_node *pNode;` |
|       - |  328 | `	sxu32 nHash;` |
|       - |  329 | `	sxu32 nIdx;` |
|       - |  330 | `	sxi32 rc;` |
|  137197 |  331 | `	if( !isForeign ){` |
|       - |  332 | `		ph7_value *pObj;` |
|       - |  333 | `		ph7_value sSafeVal;` |
|       - |  334 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  335 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  336 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  337 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  338 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  339 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   94169 |  340 | `		if( pValue ){` |
|   93879 |  341 | `			sSafeVal = *pValue;` |
|   93879 |  342 | `			pValue = &sSafeVal;` |
|   46937 |  343 | `		}` |
|       - |  344 | `		/* Reserve a ph7_value for the value */` |
|   94169 |  345 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   94169 |  346 | `		if( pObj == 0 ){` |
|     ! 0 |  347 | `			return SXERR_MEM;` |
|       - |  348 | `		}` |
|   94169 |  349 | `		if( pValue ){` |
|       - |  350 | `			/* Duplicate the value */` |
|   93879 |  351 | `			PH7_MemObjStore(pValue,pObj);` |
|   46937 |  352 | `		}` |
|   94169 |  353 | `		nIdx = pObj->nIdx;` |
|   47087 |  354 | `	}else{` |
|   43033 |  355 | `		nIdx = nRefIdx;` |
|       - |  356 | `	}` |
|       - |  357 | `	/* Hash the key */` |
|  137197 |  358 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  359 | `	/* Allocate a new blob node */` |
|  137197 |  360 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  137197 |  361 | `	if( pNode == 0 ){` |
|     ! 0 |  362 | `		return SXERR_MEM;` |
|       - |  363 | `	}` |
|  137197 |  364 | `	if( isForeign ){` |
|       - |  365 | `		/* Mark as a foregin entry */` |
|   43033 |  366 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   21514 |  367 | `	}` |
|       - |  368 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  137197 |  369 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  137197 |  370 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  371 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  372 | `		return rc;` |
|       - |  373 | `	}` |
|       - |  374 | `	/* Perform the insertion */` |
|  137197 |  375 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  376 | `	/* Install in the reference table */` |
|  137197 |  377 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  378 | `	/* All done */` |
|  137197 |  379 | `	return SXRET_OK;` |
|   68601 |  380 |  |
|       - |  381 | `/*` |
|       - |  382 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  383 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  384 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  385 | ` */` |
|   48374 |  386 | `static sxi32 HashmapLookupIntKey(` |
|       - |  387 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  388 | `	sxi64 iKey,                /* lookup key */` |
|       - |  389 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  390 | `	)` |
|       5 |  391 |  |
|       - |  392 | `	ph7_hashmap_node *pNode;` |
|       - |  393 | `	sxu32 nHash;` |
|   48379 |  394 | `	if( pMap->nEntry < 1 ){` |
|       - |  395 | `		/* Don't bother hashing,there is no entry anyway */` |
|     551 |  396 | `		return SXERR_NOTFOUND;` |
|       - |  397 | `	}` |
|       - |  398 | `	/* Hash the key first */` |
|   47833 |  399 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  400 | `	/* Point to the appropriate bucket */` |
|   47833 |  401 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  402 | `	/* Perform the lookup */` |
|  412270 |  403 | `	for(;;){` |
|  824545 |  404 | `		if( pNode == 0 ){` |
|   46307 |  405 | `			break;` |
|       - |  406 | `		}` |
|  778238 |  407 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  775222 |  408 | `			&& pNode->nHash == nHash` |
|  386871 |  409 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  410 | `				/* Node found */` |
|    1531 |  411 | `				if( ppNode ){` |
|    1519 |  412 | `					*ppNode = pNode;` |
|     757 |  413 | `				}` |
|    1531 |  414 | `				return SXRET_OK;` |
|       - |  415 | `		}` |
|       - |  416 | `		/* Follow the collision link */` |
|  776713 |  417 | `		pNode = pNode->pNextCollide;` |
|       1 |  418 | `	}` |
|       - |  419 | `	/* No such entry */` |
|   46307 |  420 | `	return SXERR_NOTFOUND;` |
|   24192 |  421 |  |
|       - |  422 | `/*` |
|       - |  423 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  424 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  425 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  426 | ` */` |
|  259000 |  427 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  428 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  429 | `	const void *pKey,           /* Lookup key */` |
|       - |  430 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  431 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  432 | `	)` |
|       5 |  433 |  |
|       - |  434 | `	ph7_hashmap_node *pNode;` |
|       - |  435 | `	sxu32 nHash;` |
|  259005 |  436 | `	if( pMap->nEntry < 1 ){` |
|       - |  437 | `		/* Don't bother hashing,there is no entry anyway */` |
|   15365 |  438 | `		return SXERR_NOTFOUND;` |
|       - |  439 | `	}` |
|       - |  440 | `	/* Hash the key first */` |
|  243645 |  441 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  442 | `	/* Point to the appropriate bucket */` |
|  243645 |  443 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  444 | `	/* Perform the lookup */` |
|  210000 |  445 | `	for(;;){` |
|  420005 |  446 | `		if( pNode == 0 ){` |
|  190999 |  447 | `			break;` |
|       - |  448 | `		}` |
|  229006 |  449 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  227503 |  450 | `			&& pNode->nHash == nHash` |
|  139323 |  451 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   52651 |  452 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  453 | `				/* Node found */` |
|   52651 |  454 | `				if( ppNode ){` |
|   52623 |  455 | `					*ppNode = pNode;` |
|   26309 |  456 | `				}` |
|   52651 |  457 | `				return SXRET_OK;` |
|       - |  458 | `		}` |
|       - |  459 | `		/* Follow the collision link */` |
|  176365 |  460 | `		pNode = pNode->pNextCollide;` |
|       5 |  461 | `	}` |
|       - |  462 | `	/* No such entry */` |
|  190999 |  463 | `	return SXERR_NOTFOUND;` |
|  129505 |  464 |  |
|       - |  465 | `/*` |
|       - |  466 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  467 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  468 | ` */` |
|  259138 |  469 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       5 |  470 |  |
|  259143 |  471 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  259143 |  472 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  259143 |  473 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  474 | `		/* Octal not decimal number */` |
|       5 |  475 | `		return FALSE;` |
|       - |  476 | `	}` |
|  259139 |  477 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  478 | `		zIn++;` |
|     ! 0 |  479 | `	}` |
|  129900 |  480 | `	for(;;){` |
|  259805 |  481 | `		if( zIn >= zEnd ){` |
|     233 |  482 | `			return TRUE;` |
|       - |  483 | `		}` |
|  259573 |  484 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  129456 |  485 | `			break;` |
|       - |  486 | `		}` |
|     667 |  487 | `		zIn++;` |
|       1 |  488 | `	}` |
|       - |  489 | `	/* Key does not look like a decimal number */` |
|  258907 |  490 | `	return FALSE;` |
|  129574 |  491 |  |
|       - |  492 | `/*` |
|       - |  493 | ` * Check if a given key exists in the given hashmap.` |
|       - |  494 | ` * Write a pointer to the target node on success.` |
|       - |  495 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  496 | ` */` |
|  123116 |  497 | `static sxi32 HashmapLookup(` |
|       - |  498 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  499 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  500 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  501 | `	)` |
|       5 |  502 |  |
|  123121 |  503 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  504 | `	sxi32 rc;` |
|  123121 |  505 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  121747 |  506 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  507 | `			/* Force a string cast */` |
|     ! 0 |  508 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  509 | `		}` |
|  121747 |  510 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  511 | `			/* Perform a blob lookup */` |
|  121731 |  512 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  121731 |  513 | `			goto result;` |
|       - |  514 | `		}` |
|       8 |  515 | `	}` |
|       - |  516 | `	/* Perform an int lookup */` |
|    1395 |  517 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  518 | `		/* Force an integer cast */` |
|      27 |  519 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  520 | `	}` |
|       - |  521 | `	/* Perform an int lookup */` |
|    1395 |  522 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   61558 |  523 | `result:` |
|  123121 |  524 | `	if( rc == SXRET_OK ){` |
|       - |  525 | `		/* Node found */` |
|   53889 |  526 | `		if( ppNode ){` |
|   53845 |  527 | `			*ppNode = pNode;` |
|   26920 |  528 | `		}` |
|   53889 |  529 | `		return SXRET_OK;` |
|       - |  530 | `	}` |
|       - |  531 | `	/* No such entry */` |
|   69237 |  532 | `	return SXERR_NOTFOUND;` |
|   61563 |  533 |  |
|       - |  534 | `/*` |
|       - |  535 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  536 | ` * hashmap.` |
|       - |  537 | ` * If a node with the given key already exists in the database` |
|       - |  538 | ` * then this function overwrite the old value.` |
|       - |  539 | ` */` |
| 3142830 |  540 | `static sxi32 HashmapInsert(` |
|       - |  541 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  542 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  543 | `	ph7_value *pVal    /* Node value */` |
|       - |  544 | `	)` |
|       5 |  545 |  |
| 3142835 |  546 | `	ph7_hashmap_node *pNode = 0;` |
| 3142835 |  547 | `	sxi32 rc = SXRET_OK;` |
| 3142835 |  548 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   97605 |  549 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  550 | `			/* Force a string cast */` |
|       3 |  551 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  552 | `		}` |
|   97605 |  553 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    3459 |  554 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  555 | `				/* Automatic index assign */` |
|    3237 |  556 | `				pKey = 0;` |
|    1616 |  557 | `			}` |
|    3459 |  558 | `			goto IntKey;` |
|       - |  559 | `		}` |
|  141224 |  560 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   47073 |  561 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  562 | `				/* Overwrite the old value */` |
|       - |  563 | `				ph7_value *pElem;` |
|      81 |  564 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      81 |  565 | `				if( pElem ){` |
|      81 |  566 | `					if( pVal ){` |
|      81 |  567 | `						PH7_MemObjStore(pVal,pElem);` |
|      42 |  568 | `					}else{` |
|       - |  569 | `						/* Nullify the entry */` |
|     ! 0 |  570 | `						PH7_MemObjToNull(pElem);` |
|       - |  571 | `					}` |
|      39 |  572 | `				}` |
|      81 |  573 | `				return SXRET_OK;` |
|       - |  574 | `		}` |
|   94073 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  576 | `			/* Forbidden */` |
|       3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  578 | `			return SXRET_OK;` |
|       - |  579 | `		}` |
|       - |  580 | `		/* Perform a blob-key insertion */` |
|   94071 |  581 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   94071 |  582 | `		return rc;` |
|       - |  583 | `	}` |
| 1522615 |  584 | `IntKey:` |
| 3048689 |  585 | `	if( pKey ){` |
|   23615 |  586 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  587 | `			/* Force an integer cast */` |
|     251 |  588 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  589 | `		}` |
|   23615 |  590 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  591 | `			/* Overwrite the old value */` |
|       - |  592 | `			ph7_value *pElem;` |
|      87 |  593 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      87 |  594 | `			if( pElem ){` |
|      87 |  595 | `				if( pVal ){` |
|      87 |  596 | `					PH7_MemObjStore(pVal,pElem);` |
|      44 |  597 | `				}else{` |
|       - |  598 | `					/* Nullify the entry */` |
|     ! 0 |  599 | `					PH7_MemObjToNull(pElem);` |
|       - |  600 | `				}` |
|      43 |  601 | `			}` |
|      87 |  602 | `			return SXRET_OK;` |
|       - |  603 | `		}` |
|   23529 |  604 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  605 | `			/* Forbidden */` |
|       3 |  606 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  607 | `			return SXRET_OK;` |
|       - |  608 | `		}` |
|       - |  609 | `		/* Perform a 64-bit-int-key insertion */` |
|   23527 |  610 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23527 |  611 | `		if( rc == SXRET_OK ){` |
|   23527 |  612 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  613 | `				/* Increment the automatic index */` |
|   23287 |  614 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  615 | `				/* Make sure the automatic index is not reserved */` |
|   23287 |  616 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  617 | `					pMap->iNextIdx++;` |
|     ! 0 |  618 | `				}` |
|   11641 |  619 | `			}` |
|   11761 |  620 | `		}` |
|   11766 |  621 | `	}else{` |
| 3025079 |  622 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  623 | `			/* Forbidden */` |
|       3 |  624 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  625 | `			return SXRET_OK;` |
|       - |  626 | `		}` |
|       - |  627 | `		/* Assign an automatic index */` |
| 3025077 |  628 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 3025077 |  629 | `		if( rc == SXRET_OK ){` |
| 3025077 |  630 | `			++pMap->iNextIdx;` |
| 1512536 |  631 | `		}` |
|       - |  632 | `	}` |
|       - |  633 | `	/* Insertion result */` |
| 3048599 |  634 | `	return rc;` |
| 1571420 |  635 |  |
|       - |  636 | `/*` |
|       - |  637 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - |  638 | ` * hashmap.` |
|       - |  639 | ` * This is insertion by reference so be careful to mark the node` |
|       - |  640 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - |  641 | ` * The insertion by reference is triggered when the following` |
|       - |  642 | ` * expression is encountered.` |
|       - |  643 | ` * $var = 10;` |
|       - |  644 | ` *  $a = array(&var);` |
|       - |  645 | ` * OR` |
|       - |  646 | ` *  $a[] =& $var;` |
|       - |  647 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - |  648 | ` * over it's contents.` |
|       - |  649 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - |  650 | ` * removed when the foreign ph7_value is unset.` |
|       - |  651 | ` * Example:` |
|       - |  652 | ` *  $var = 10;` |
|       - |  653 | ` *  $a[] =& $var;` |
|       - |  654 | ` *  echo count($a).PHP_EOL; //1` |
|       - |  655 | ` *  //Unset the foreign ph7_value now` |
|       - |  656 | ` *  unset($var);` |
|       - |  657 | ` *  echo count($a); //0` |
|       - |  658 | ` * Note that this is a PH7 eXtension.` |
|       - |  659 | ` * Refer to the official documentation for more information.` |
|       - |  660 | ` * If a node with the given key already exists in the database` |
|       - |  661 | ` * then this function overwrite the old value.` |
|       - |  662 | ` */` |
|   43070 |  663 | `static sxi32 HashmapInsertByRef(` |
|       - |  664 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  665 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  666 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  667 | `	)` |
|       5 |  668 |  |
|   43075 |  669 | `	ph7_hashmap_node *pNode = 0;` |
|   43075 |  670 | `	sxi32 rc = SXRET_OK;` |
|   43075 |  671 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   43039 |  672 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  673 | `			/* Force a string cast */` |
|     ! 0 |  674 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  675 | `		}` |
|   43039 |  676 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  677 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  678 | `				/* Automatic index assign */` |
|     ! 0 |  679 | `				pKey = 0;` |
|     ! 0 |  680 | `			}` |
|     ! 0 |  681 | `			goto IntKey;` |
|       - |  682 | `		}` |
|   64556 |  683 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   21517 |  684 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  685 | `				/* Overwrite */` |
|       7 |  686 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  687 | `				pNode->nValIdx = nRefIdx;` |
|       - |  688 | `				/* Install in the reference table */` |
|       7 |  689 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  690 | `				return SXRET_OK;` |
|       - |  691 | `		}` |
|       - |  692 | `		/* Perform a blob-key insertion */` |
|   43033 |  693 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   43033 |  694 | `		return rc;` |
|       - |  695 | `	}` |
|      18 |  696 | `IntKey:` |
|      37 |  697 | `	if( pKey ){` |
|       5 |  698 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  699 | `			/* Force an integer cast */` |
|     ! 0 |  700 | `			PH7_MemObjToInteger(pKey);` |
|     ! 0 |  701 | `		}` |
|       5 |  702 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  703 | `			/* Overwrite */` |
|     ! 0 |  704 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|     ! 0 |  705 | `			pNode->nValIdx = nRefIdx;` |
|       - |  706 | `			/* Install in the reference table */` |
|     ! 0 |  707 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|     ! 0 |  708 | `			return SXRET_OK;` |
|       - |  709 | `		}` |
|       - |  710 | `		/* Perform a 64-bit-int-key insertion */` |
|       5 |  711 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|       5 |  712 | `		if( rc == SXRET_OK ){` |
|       5 |  713 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  714 | `				/* Increment the automatic index */` |
|       5 |  715 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  716 | `				/* Make sure the automatic index is not reserved */` |
|       5 |  717 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  718 | `					pMap->iNextIdx++;` |
|     ! 0 |  719 | `				}` |
|       2 |  720 | `			}` |
|       2 |  721 | `		}` |
|       3 |  722 | `	}else{` |
|       - |  723 | `		/* Assign an automatic index */` |
|      33 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|      33 |  725 | `		if( rc == SXRET_OK ){` |
|      33 |  726 | `			++pMap->iNextIdx;` |
|      16 |  727 | `		}` |
|       - |  728 | `	}` |
|       - |  729 | `	/* Insertion result */` |
|      37 |  730 | `	return rc;` |
|   21540 |  731 |  |
|       - |  732 | `/*` |
|       - |  733 | ` * Extract node value.` |
|       - |  734 | ` */` |
| 1310690 |  735 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       5 |  736 |  |
|       - |  737 | `	/* Point to the desired object */` |
|       - |  738 | `	ph7_value *pObj;` |
| 1310695 |  739 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1310695 |  740 | `	return pObj;` |
|       5 |  741 |  |
|       - |  742 | `/*` |
|       - |  743 | ` * Insert a node in the given hashmap.` |
|       - |  744 | ` * If a node with the given key already exists in the database` |
|       - |  745 | ` * then this function overwrite the old value.` |
|       - |  746 | ` */` |
|     446 |  747 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       5 |  748 |  |
|       - |  749 | `	ph7_value *pObj;` |
|       - |  750 | `	sxi32 rc;` |
|       - |  751 | `	/* Extract the node value */` |
|     451 |  752 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     451 |  753 | `	if( pObj == 0 ){` |
|     ! 0 |  754 | `		return SXERR_EMPTY;` |
|       - |  755 | `	}` |
|       - |  756 | `	/* Preserve key */` |
|     451 |  757 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  758 | `		/* Int64 key */` |
|     321 |  759 | `		if( !bPreserve ){` |
|       - |  760 | `			/* Assign an automatic index */` |
|     173 |  761 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      89 |  762 | `		}else{` |
|     149 |  763 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  764 | `		}` |
|     163 |  765 | `	}else{` |
|       - |  766 | `		/* Blob key */` |
|     131 |  767 | `		if( !bPreserve ){` |
|       - |  768 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  769 | `			 * original string key entirely */` |
|      35 |  770 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      18 |  771 | `		}else{` |
|     145 |  772 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      48 |  773 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  774 | `		}` |
|       - |  775 | `	}` |
|     451 |  776 | `	return rc;` |
|     228 |  777 |  |
|       - |  778 | `/*` |
|       - |  779 | ` * Compare two node values.` |
|       - |  780 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  781 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  782 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  783 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  784 | ` * documenation.` |
|       - |  785 | ` */` |
|   66772 |  786 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       5 |  787 |  |
|       - |  788 | `	ph7_value sObj1,sObj2;` |
|       - |  789 | `	sxi32 rc;` |
|   66777 |  790 | `	if( pLeft == pRight ){` |
|       - |  791 | `		/*` |
|       - |  792 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  793 | `		 * below for more information on this sceanario.` |
|       - |  794 | `		 */` |
|     ! 0 |  795 | `		return 0;` |
|       - |  796 | `	}` |
|       - |  797 | `	/* Do the comparison */` |
|   66777 |  798 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   66777 |  799 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   66777 |  800 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   66777 |  801 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   66777 |  802 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   66777 |  803 | `	PH7_MemObjRelease(&sObj1);` |
|   66777 |  804 | `	PH7_MemObjRelease(&sObj2);` |
|   66777 |  805 | `	return rc;` |
|   33425 |  806 |  |
|       - |  807 | `/*` |
|       - |  808 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  809 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  810 | ` */` |
|   12816 |  811 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       5 |  812 |  |
|   12821 |  813 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  814 | `	sxu32 nBucket;` |
|       - |  815 | `	/* Remove old collision links */` |
|   12821 |  816 | `	if( pEntry->pPrevCollide ){` |
|   10436 |  817 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    5213 |  818 | `	}else{` |
|    2390 |  819 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  820 | `	}` |
|   12821 |  821 | `	if( pEntry->pNextCollide ){` |
|    1023 |  822 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     503 |  823 | `	}` |
|   12821 |  824 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  825 | `	/* Compute the new hash */` |
|   12821 |  826 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   12821 |  827 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   12821 |  828 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  829 | `	/* Link to the new bucket */` |
|   12821 |  830 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   12821 |  831 | `	if( pMap->apBucket[nBucket] ){` |
|   10751 |  832 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    5364 |  833 | `	}` |
|   12821 |  834 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   12821 |  835 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  836 | `	/* Increment the automatic index */` |
|   12821 |  837 | `	pMap->iNextIdx++;` |
|   12821 |  838 |  |
|       - |  839 | `/*` |
|       - |  840 | ` * Perform a linear search on a given hashmap.` |
|       - |  841 | ` * Write a pointer to the target node on success.` |
|       - |  842 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  843 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  844 | ` * for more information.` |
|       - |  845 | ` */` |
|   32020 |  846 | `static int HashmapFindValue(` |
|       - |  847 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  848 | `	ph7_value *pNeedle,  /* Lookup key */` |
|       - |  849 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|       - |  850 | `	int bStrict      /* TRUE for strict comparison */` |
|       - |  851 | `	)` |
|       5 |  852 |  |
|       - |  853 | `	ph7_hashmap_node *pEntry;` |
|       - |  854 | `	ph7_value sVal,*pVal;` |
|       - |  855 | `	ph7_value sNeedle;` |
|       - |  856 | `	sxi32 rc;` |
|       - |  857 | `	sxu32 n;` |
|       - |  858 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|   32025 |  859 | `	pEntry = pMap->pFirst;` |
|   32025 |  860 | `	n = pMap->nEntry;` |
|   32025 |  861 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   32025 |  862 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   76590 |  863 | `	for(;;){` |
|  153183 |  864 | `		if( n < 1 ){` |
|      99 |  865 | `			break;` |
|       - |  866 | `		}` |
|       - |  867 | `		/* Extract node value */` |
|  153085 |  868 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  153085 |  869 | `		if( pVal ){` |
|  153085 |  870 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|     ! 0 |  871 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  872 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  873 | `				if( iF1 == iF2 ){` |
|       - |  874 | `					/* NULL values are equals */` |
|     ! 0 |  875 | `					if( ppNode ){` |
|     ! 0 |  876 | `						*ppNode = pEntry;` |
|     ! 0 |  877 | `					}` |
|     ! 0 |  878 | `					return SXRET_OK;` |
|       - |  879 | `				}` |
|     ! 0 |  880 | `			}else{` |
|       - |  881 | `				/* Duplicate value */` |
|  153085 |  882 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  153085 |  883 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  153085 |  884 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  153085 |  885 | `				PH7_MemObjRelease(&sVal);` |
|  153085 |  886 | `				PH7_MemObjRelease(&sNeedle);` |
|  153085 |  887 | `				if( rc == 0 ){` |
|   31927 |  888 | `					if( ppNode ){` |
|      23 |  889 | `						*ppNode = pEntry;` |
|      11 |  890 | `					}` |
|       - |  891 | `					/* Match found*/` |
|   31927 |  892 | `					return SXRET_OK;` |
|       - |  893 | `				}` |
|       - |  894 | `			}` |
|   60580 |  895 | `		}` |
|       - |  896 | `		/* Point to the next entry */` |
|  121163 |  897 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  121163 |  898 | `		n--;` |
|       5 |  899 | `	}` |
|       - |  900 | `	/* No such entry */` |
|      99 |  901 | `	return SXERR_NOTFOUND;` |
|   16015 |  902 |  |
|       - |  903 | `/*` |
|       - |  904 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  905 | ` * for values comparison.` |
|       - |  906 | ` * Write a pointer to the target node on success.` |
|       - |  907 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  908 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  909 | ` * for more information.` |
|       - |  910 | ` */` |
|      22 |  911 | `static int HashmapFindValueByCallback(` |
|       - |  912 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|       - |  913 | `	ph7_value *pNeedle,    /* Lookup key */` |
|       - |  914 | `	ph7_value *pCallback,  /* User defined callback */` |
|       - |  915 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|       - |  916 | `	)` |
|       1 |  917 |  |
|       - |  918 | `	ph7_hashmap_node *pEntry;` |
|       - |  919 | `	ph7_value sResult,*pVal;` |
|       - |  920 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|       - |  921 | `	sxi32 rc;` |
|       - |  922 | `	sxu32 n;` |
|      23 |  923 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|       - |  924 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|       - |  925 | `		 * exception is not thrown again, and let the caller wind down. */` |
|     ! 0 |  926 | `		return SXERR_NOTFOUND;` |
|       - |  927 | `	}` |
|       - |  928 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|      23 |  929 | `	pEntry = pMap->pFirst;` |
|      23 |  930 | `	n = pMap->nEntry;` |
|       - |  931 | `	/* Store callback result here */` |
|      23 |  932 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  933 | `	/* First argument to the callback */` |
|      23 |  934 | `	apArg[0] = pNeedle;` |
|      25 |  935 | `	for(;;){` |
|      51 |  936 | `		if( n < 1 ){` |
|       9 |  937 | `			break;` |
|       - |  938 | `		}` |
|       - |  939 | `		/* Extract node value */` |
|      43 |  940 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      43 |  941 | `		if( pVal ){` |
|       - |  942 | `			/* Invoke the user callback */` |
|      43 |  943 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      43 |  944 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      43 |  945 | `			if( rc == PH7_EXCEPTION ){` |
|       - |  946 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|       - |  947 | `				 * and report no match for the rest of the run. */` |
|       5 |  948 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|       5 |  949 | `				PH7_MemObjRelease(&sResult);` |
|       5 |  950 | `				return SXERR_NOTFOUND;` |
|       - |  951 | `			}` |
|      39 |  952 | `			if( rc == SXRET_OK ){` |
|       - |  953 | `				/* Extract callback result */` |
|      39 |  954 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  955 | `					/* Perform an int cast */` |
|     ! 0 |  956 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  957 | `				}` |
|      39 |  958 | `				rc = (sxi32)sResult.x.iVal;` |
|      39 |  959 | `				PH7_MemObjRelease(&sResult);` |
|      39 |  960 | `				if( rc == 0 ){` |
|       - |  961 | `					/* Match found*/` |
|      11 |  962 | `					if( ppNode ){` |
|     ! 0 |  963 | `						*ppNode = pEntry;` |
|     ! 0 |  964 | `					}` |
|      11 |  965 | `					return SXRET_OK;` |
|       - |  966 | `				}` |
|      14 |  967 | `			}` |
|      14 |  968 | `		}` |
|       - |  969 | `		/* Point to the next entry */` |
|      29 |  970 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 |  971 | `		n--;` |
|       1 |  972 | `	}` |
|       - |  973 | `	/* No such entry */` |
|       9 |  974 | `	return SXERR_NOTFOUND;` |
|      12 |  975 |  |
|       - |  976 | `/*` |
|       - |  977 | ` * Compare two hashmaps.` |
|       - |  978 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|       - |  979 | ` * Note on array comparison operators.` |
|       - |  980 | ` *  According to the PHP language reference manual.` |
|       - |  981 | ` *  Array Operators Example 	Name 	Result` |
|       - |  982 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|       - |  983 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|       - |  984 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|       - |  985 | ` *                          order and of the same types.` |
|       - |  986 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - |  987 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - |  988 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|       - |  989 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - |  990 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - |  991 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - |  992 | ` * <?php` |
|       - |  993 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - |  994 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - |  995 | ` * $c = $a + $b; // Union of $a and $b` |
|       - |  996 | ` * echo "Union of \$a and \$b: \n";` |
|       - |  997 | ` * var_dump($c);` |
|       - |  998 | ` * $c = $b + $a; // Union of $b and $a` |
|       - |  999 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1000 | ` * var_dump($c);` |
|       - | 1001 | ` * ?>` |
|       - | 1002 | ` * When executed, this script will print the following:` |
|       - | 1003 | ` * Union of $a and $b:` |
|       - | 1004 | ` * array(3) {` |
|       - | 1005 | ` *  ["a"]=>` |
|       - | 1006 | ` *  string(5) "apple"` |
|       - | 1007 | ` *  ["b"]=>` |
|       - | 1008 | ` * string(6) "banana"` |
|       - | 1009 | ` *  ["c"]=>` |
|       - | 1010 | ` * string(6) "cherry"` |
|       - | 1011 | ` * }` |
|       - | 1012 | ` * Union of $b and $a:` |
|       - | 1013 | ` * array(3) {` |
|       - | 1014 | ` * ["a"]=>` |
|       - | 1015 | ` * string(4) "pear"` |
|       - | 1016 | ` * ["b"]=>` |
|       - | 1017 | ` * string(10) "strawberry"` |
|       - | 1018 | ` * ["c"]=>` |
|       - | 1019 | ` * string(6) "cherry"` |
|       - | 1020 | ` * }` |
|       - | 1021 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|       - | 1022 | ` */` |
|      26 | 1023 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - | 1024 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - | 1025 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - | 1026 | `	int bStrict          /* TRUE for strict comparison */` |
|       - | 1027 | `	)` |
|       1 | 1028 |  |
|       - | 1029 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - | 1030 | `	sxi32 rc;` |
|       - | 1031 | `	sxu32 n;` |
|      27 | 1032 | `	if( pLeft == pRight ){` |
|       - | 1033 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - | 1034 | `		 * Unlike the zend engine.` |
|       - | 1035 | `		 */` |
|     ! 0 | 1036 | `		return 0;` |
|       - | 1037 | `	}` |
|      27 | 1038 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - | 1039 | `		/* Must have the same number of entries */` |
|       5 | 1040 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1041 | `	}` |
|       - | 1042 | `	/* Point to the first inserted entry of the left hashmap */` |
|      23 | 1043 | `	pLe = pLeft->pFirst;` |
|      23 | 1044 | `	pRe = 0; /* cc warning */` |
|       - | 1045 | `	/* Perform the comparison */` |
|      23 | 1046 | `	n = pLeft->nEntry;` |
|      27 | 1047 | `	for(;;){` |
|      55 | 1048 | `		if( n < 1 ){` |
|      21 | 1049 | `			break;` |
|       - | 1050 | `		}` |
|      35 | 1051 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1052 | `			/* Int key */` |
|      27 | 1053 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|      14 | 1054 | `		}else{` |
|       9 | 1055 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1056 | `			/* Blob key */` |
|       9 | 1057 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1058 | `		}` |
|      35 | 1059 | `		if( rc != SXRET_OK ){` |
|       - | 1060 | `			/* No such entry in the right side */` |
|     ! 0 | 1061 | `			return 1;` |
|       - | 1062 | `		}` |
|      35 | 1063 | `		rc = 0;` |
|      35 | 1064 | `		if( bStrict ){` |
|       - | 1065 | `			/* Make sure,the keys are of the same type */` |
|      19 | 1066 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1067 | `				rc = 1;` |
|     ! 0 | 1068 | `			}` |
|       9 | 1069 | `		}` |
|      35 | 1070 | `		if( !rc ){` |
|       - | 1071 | `			/* Compare nodes */` |
|      35 | 1072 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|      17 | 1073 | `		}` |
|      35 | 1074 | `		if( rc != 0 ){` |
|       - | 1075 | `			/* Nodes key/value differ */` |
|       3 | 1076 | `			return rc;` |
|       - | 1077 | `		}` |
|       - | 1078 | `		/* Point to the next entry */` |
|      33 | 1079 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      33 | 1080 | `		n--;` |
|       1 | 1081 | `	}` |
|      21 | 1082 | `	return 0; /* Hashmaps are equals */` |
|      14 | 1083 |  |
|       - | 1084 | `/*` |
|       - | 1085 | ` * Duplicate a hashmap node.` |
|       - | 1086 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1087 | ` */` |
|  606306 | 1088 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1089 | `	ph7_hashmap *pDest,` |
|       - | 1090 | `	ph7_hashmap_node *pEntry,` |
|       - | 1091 | `	ph7_value *pVal,` |
|       - | 1092 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1093 | `	)` |
|       5 | 1094 |  |
|       - | 1095 | `	ph7_value sSafeVal;` |
|       - | 1096 | `	ph7_value sKey;` |
|       - | 1097 | `	sxi32 rc;` |
|       - | 1098 |  |
|  606311 | 1099 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 1100 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|       - | 1101 | `		 * Re-insert it by reference so the reference survives the duplication` |
|       - | 1102 | `		 * instead of being flattened to a value copy. This keeps spread` |
|       - | 1103 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|       - | 1104 | `		 * with PHP semantics. */` |
|       7 | 1105 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|       7 | 1106 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       3 | 1107 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|       3 | 1108 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|       3 | 1109 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       3 | 1110 | `			PH7_MemObjRelease(&sKey);` |
|       2 | 1111 | `		}else{` |
|       5 | 1112 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|       5 | 1113 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|       2 | 1114 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|     ! 0 | 1115 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|     ! 0 | 1116 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|     ! 0 | 1117 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 1118 | `			}else{ /* Dup: preserve the int key */` |
|     ! 0 | 1119 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|       - | 1120 | `			}` |
|       - | 1121 | `		}` |
|       7 | 1122 | `		return rc;` |
|       - | 1123 | `	}` |
|  606305 | 1124 | `	sSafeVal = *pVal;` |
|       - | 1125 |  |
|  606305 | 1126 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1127 | `		/* Blob key insertion */` |
|     101 | 1128 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|     101 | 1129 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|     101 | 1130 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|     101 | 1131 | `		PH7_MemObjRelease(&sKey);` |
|      51 | 1132 | `	}else{` |
|       - | 1133 | `		/* Int key */` |
|  606205 | 1134 | `		if( iAction == 0 ){ /* Merge */` |
|  605999 | 1135 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  303206 | 1136 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1137 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1138 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1139 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1140 | `		}else{ /* Dup */` |
|     178 | 1141 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1142 | `		}` |
|       - | 1143 | `	}` |
|  606305 | 1144 | `	return rc;` |
|  303158 | 1145 |  |
|       - | 1146 | `/*` |
|       - | 1147 | ` * Merge two hashmaps.` |
|       - | 1148 | ` * Note on the merge process` |
|       - | 1149 | ` * According to the PHP language reference manual.` |
|       - | 1150 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|       - | 1151 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|       - | 1152 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|       - | 1153 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|       - | 1154 | ` *  the later value will not overwrite the original value, but will be appended.` |
|       - | 1155 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|       - | 1156 | ` *  keys starting from zero in the result array.` |
|       - | 1157 | ` */` |
|    2096 | 1158 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       5 | 1159 |  |
|       - | 1160 | `	ph7_hashmap_node *pEntry;` |
|       - | 1161 | `	ph7_value *pVal;` |
|       - | 1162 | `	sxi32 rc;` |
|       - | 1163 | `	sxu32 n;` |
|    2101 | 1164 | `	if( pSrc == pDest ){` |
|       - | 1165 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1166 | `		 * Unlike the zend engine.` |
|       - | 1167 | `		 */` |
|     ! 0 | 1168 | `		return SXRET_OK;` |
|       - | 1169 | `	}` |
|       - | 1170 | `	/* Point to the first inserted entry in the source */` |
|    2101 | 1171 | `	pEntry = pSrc->pFirst;` |
|       - | 1172 | `	/* Perform the merge */` |
|  608153 | 1173 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1174 | `		/* Extract the node value */` |
|  606057 | 1175 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  606057 | 1176 | `		if( pVal ){` |
|       - | 1177 | `			/* Make a local copy of the value.` |
|       - | 1178 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1179 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1180 | `			 * to the old pool.` |
|       - | 1181 | `			 */` |
|  606057 | 1182 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  303031 | 1183 | `		}else{` |
|     ! 0 | 1184 | `			rc = SXRET_OK;` |
|       - | 1185 | `		}` |
|  606057 | 1186 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1187 | `			return rc;` |
|       - | 1188 | `		}` |
|       - | 1189 | `		/* Point to the next entry */` |
|  606057 | 1190 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  303031 | 1191 | `	}` |
|    2101 | 1192 | `	return SXRET_OK;` |
|    1053 | 1193 |  |
|       - | 1194 | `/*` |
|       - | 1195 | ` * Overwrite entries with the same key.` |
|       - | 1196 | ` * Refer to the [array_replace()] implementation for more information.` |
|       - | 1197 | ` *  According to the PHP language reference manual.` |
|       - | 1198 | ` *  array_replace() replaces the values of the first array with the same values` |
|       - | 1199 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|       - | 1200 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|       - | 1201 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|       - | 1202 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|       - | 1203 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|       - | 1204 | ` *  overwriting the previous values.` |
|       - | 1205 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|       - | 1206 | ` *  by whatever type is in the second array.` |
|       - | 1207 | ` */` |
|      34 | 1208 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1209 |  |
|       - | 1210 | `	ph7_hashmap_node *pEntry;` |
|       - | 1211 | `	ph7_value *pVal;` |
|       - | 1212 | `	sxi32 rc;` |
|       - | 1213 | `	sxu32 n;` |
|      36 | 1214 | `	if( pSrc == pDest ){` |
|       - | 1215 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1216 | `		 * Unlike the zend engine.` |
|       - | 1217 | `		 */` |
|     ! 0 | 1218 | `		return SXRET_OK;` |
|       - | 1219 | `	}` |
|       - | 1220 | `	/* Point to the first inserted entry in the source */` |
|      36 | 1221 | `	pEntry = pSrc->pFirst;` |
|       - | 1222 | `	/* Perform the merge */` |
|      80 | 1223 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1224 | `		/* Extract the node value */` |
|      46 | 1225 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      46 | 1226 | `		if( pVal ){` |
|      46 | 1227 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      24 | 1228 | `		}else{` |
|     ! 0 | 1229 | `			rc = SXRET_OK;` |
|       - | 1230 | `		}` |
|      46 | 1231 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1232 | `			return rc;` |
|       - | 1233 | `		}` |
|       - | 1234 | `		/* Point to the next entry */` |
|      46 | 1235 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 1236 | `	}` |
|      36 | 1237 | `	return SXRET_OK;` |
|      19 | 1238 |  |
|       - | 1239 | `/*` |
|       - | 1240 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|       - | 1241 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|       - | 1242 | ` */` |
|     108 | 1243 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1244 |  |
|       - | 1245 | `	ph7_hashmap_node *pEntry;` |
|       - | 1246 | `	ph7_value *pVal;` |
|       - | 1247 | `	sxi32 rc;` |
|       - | 1248 | `	sxu32 n;` |
|     110 | 1249 | `	if( pSrc == pDest ){` |
|       - | 1250 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1251 | `		 * Unlike the zend engine.` |
|       - | 1252 | `		 */` |
|     ! 0 | 1253 | `		return SXRET_OK;` |
|       - | 1254 | `	}` |
|       - | 1255 | `	/* Point to the first inserted entry in the source */` |
|     110 | 1256 | `	pEntry = pSrc->pFirst;` |
|       - | 1257 | `	/* Perform the duplication */` |
|     320 | 1258 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1259 | `		/* Extract the node value */` |
|     212 | 1260 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     212 | 1261 | `		if( pVal ){` |
|     212 | 1262 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|     107 | 1263 | `		}else{` |
|     ! 0 | 1264 | `			rc = SXRET_OK;` |
|       - | 1265 | `		}` |
|     212 | 1266 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1267 | `			return rc;` |
|       - | 1268 | `		}` |
|       - | 1269 | `		/* Point to the next entry */` |
|     212 | 1270 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     107 | 1271 | `	}` |
|     110 | 1272 | `	return SXRET_OK;` |
|      56 | 1273 |  |
|       - | 1274 | `/*` |
|       - | 1275 | ` * Copy-on-write separation for arrays.` |
|       - | 1276 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|       - | 1277 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|       - | 1278 | ` * Returns the (possibly new) hashmap pointer.` |
|       - | 1279 | ` */` |
|  210092 | 1280 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1281 |  |
|  210097 | 1282 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1283 | `	ph7_hashmap *pNew;` |
|       - | 1284 | `	ph7_value *pBacking;` |
|       - | 1285 | `	sxu32 nValIdx;` |
|       - | 1286 | `	int bValueInPool;` |
|  210097 | 1287 | `	if( pMap->iRef < 2 ){` |
|       - | 1288 | `		/* Sole owner, no separation needed */` |
|  207933 | 1289 | `		return pMap;` |
|       - | 1290 | `	}` |
|    2169 | 1291 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1292 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1293 | `		return pMap;` |
|       - | 1294 | `	}` |
|       - | 1295 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1296 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1297 | `	 * frame is popped. */` |
|    2169 | 1298 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2169 | 1299 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    2164 | 1300 | `		if( pBacking && pBacking != pValue` |
|    2146 | 1301 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2133 | 1302 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1303 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2133 | 1304 | `			pMap->iRef--;` |
|    2133 | 1305 | `			if( pMap->iRef < 2 ){` |
|       - | 1306 | `				/* After undoing stack ref, sole owner — no separation */` |
|    2095 | 1307 | `				pMap->iRef++;` |
|    2095 | 1308 | `				return pMap;` |
|       - | 1309 | `			}` |
|      39 | 1310 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|      39 | 1311 | `			if( pNew == 0 ){` |
|     ! 0 | 1312 | `				pMap->iRef++;` |
|     ! 0 | 1313 | `				return pMap;` |
|       - | 1314 | `			}` |
|      39 | 1315 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1316 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|     ! 0 | 1317 | `				PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1318 | `				pMap->iRef++;` |
|     ! 0 | 1319 | `				return pMap;` |
|       - | 1320 | `			}` |
|      39 | 1321 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|      39 | 1322 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|       - | 1323 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|       - | 1324 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|       - | 1325 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|       - | 1326 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|       - | 1327 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|       - | 1328 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|       - | 1329 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|      39 | 1330 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      39 | 1331 | `			if( pBacking ){` |
|      39 | 1332 | `				pBacking->x.pOther = pNew;` |
|      19 | 1333 | `			}` |
|       - | 1334 | `			/* Update the stack value to match */` |
|      39 | 1335 | `			pValue->x.pOther = pNew;` |
|      39 | 1336 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|      39 | 1337 | `			return pNew;` |
|       - | 1338 | `		}` |
|      18 | 1339 | `	}` |
|       - | 1340 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|       - | 1341 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|       - | 1342 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|       - | 1343 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|       - | 1344 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|       - | 1345 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|       - | 1346 | `	 * pBacking in the backing-variable branch above). */` |
|      37 | 1347 | `	nValIdx = pValue->nIdx;` |
|      55 | 1348 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|      36 | 1349 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|      37 | 1350 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      37 | 1351 | `	if( pNew == 0 ){` |
|       - | 1352 | `		/* Allocation failure — fall through with shared map */` |
|     ! 0 | 1353 | `		return pMap;` |
|       - | 1354 | `	}` |
|      37 | 1355 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1356 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|     ! 0 | 1357 | `		PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1358 | `		return pMap;` |
|       - | 1359 | `	}` |
|      37 | 1360 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      37 | 1361 | `	pMap->iRef--;` |
|      37 | 1362 | `	if( bValueInPool ){` |
|       - | 1363 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|      37 | 1364 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|      37 | 1365 | `		if( pValue == 0 ){` |
|     ! 0 | 1366 | `			return pNew;` |
|       - | 1367 | `		}` |
|      18 | 1368 | `	}` |
|      37 | 1369 | `	pValue->x.pOther = pNew;` |
|      37 | 1370 | `	return pNew;` |
|  105051 | 1371 |  |
|       - | 1372 | `/*` |
|       - | 1373 | ` * Perform the union of two hashmaps.` |
|       - | 1374 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1375 | ` * with a variable holding an array as follows:` |
|       - | 1376 | ` * <?php` |
|       - | 1377 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1378 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1379 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1380 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1381 | ` * var_dump($c);` |
|       - | 1382 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1383 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1384 | ` * var_dump($c);` |
|       - | 1385 | ` * ?>` |
|       - | 1386 | ` * When executed, this script will print the following:` |
|       - | 1387 | ` * Union of $a and $b:` |
|       - | 1388 | ` * array(3) {` |
|       - | 1389 | ` *  ["a"]=>` |
|       - | 1390 | ` *  string(5) "apple"` |
|       - | 1391 | ` *  ["b"]=>` |
|       - | 1392 | ` * string(6) "banana"` |
|       - | 1393 | ` *  ["c"]=>` |
|       - | 1394 | ` * string(6) "cherry"` |
|       - | 1395 | ` * }` |
|       - | 1396 | ` * Union of $b and $a:` |
|       - | 1397 | ` * array(3) {` |
|       - | 1398 | ` * ["a"]=>` |
|       - | 1399 | ` * string(4) "pear"` |
|       - | 1400 | ` * ["b"]=>` |
|       - | 1401 | ` * string(10) "strawberry"` |
|       - | 1402 | ` * ["c"]=>` |
|       - | 1403 | ` * string(6) "cherry"` |
|       - | 1404 | ` * }` |
|       - | 1405 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1406 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1407 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1408 | ` */` |
|      10 | 1409 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       2 | 1410 |  |
|       - | 1411 | `	ph7_hashmap_node *pEntry;` |
|      12 | 1412 | `	sxi32 rc = SXRET_OK;` |
|       - | 1413 | `	ph7_value *pObj;` |
|       - | 1414 | `	sxu32 n;` |
|      12 | 1415 | `	if( pLeft == pRight ){` |
|       - | 1416 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1417 | `		 * Unlike the zend engine.` |
|       - | 1418 | `		 */` |
|     ! 0 | 1419 | `		return SXRET_OK;` |
|       - | 1420 | `	}` |
|       - | 1421 | `	/* Perform the union */` |
|      12 | 1422 | `	pEntry = pRight->pFirst;` |
|      32 | 1423 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1424 | `		/* Make sure the given key does not exists in the left array */` |
|      22 | 1425 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1426 | `			/* BLOB key */` |
|       7 | 1427 | `			if( SXRET_OK !=` |
|       6 | 1428 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|       3 | 1429 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|       3 | 1430 | `					if( pObj ){` |
|       3 | 1431 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1432 | `						/* Perform the insertion */` |
|       3 | 1433 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1434 | `							&sSafeVal,0,FALSE);` |
|       3 | 1435 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1436 | `							return rc;` |
|       - | 1437 | `						}` |
|       1 | 1438 | `					}` |
|       1 | 1439 | `			}` |
|       4 | 1440 | `		}else{` |
|       - | 1441 | `			/* INT key */` |
|      16 | 1442 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|      11 | 1443 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|      11 | 1444 | `				if( pObj ){` |
|      11 | 1445 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1446 | `					/* Perform the insertion */` |
|      11 | 1447 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      11 | 1448 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1449 | `						return rc;` |
|       - | 1450 | `					}` |
|       5 | 1451 | `				}` |
|       5 | 1452 | `			}` |
|       - | 1453 | `		}` |
|       - | 1454 | `		/* Point to the next entry */` |
|      22 | 1455 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      12 | 1456 | `	}` |
|      12 | 1457 | `	return SXRET_OK;` |
|       7 | 1458 |  |
|       - | 1459 | `/*` |
|       - | 1460 | ` * Allocate a new hashmap.` |
|       - | 1461 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1462 | ` */` |
|   96038 | 1463 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1464 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1465 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1466 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1467 | `	)` |
|       5 | 1468 |  |
|       - | 1469 | `	ph7_hashmap *pMap;` |
|       - | 1470 | `	/* Allocate a new instance */` |
|   96043 | 1471 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   96043 | 1472 | `	if( pMap == 0 ){` |
|     ! 0 | 1473 | `		return 0;` |
|       - | 1474 | `	}` |
|       - | 1475 | `	/* Zero the structure */` |
|   96043 | 1476 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1477 | `	/* Fill in the structure */` |
|   96043 | 1478 | `	pMap->pVm = &(*pVm);` |
|   96043 | 1479 | `	pMap->iRef = 1;` |
|       - | 1480 | `	/* Default hash functions */` |
|   96043 | 1481 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   96043 | 1482 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   96043 | 1483 | `	return pMap;` |
|   48024 | 1484 |  |
|       - | 1485 | `/*` |
|       - | 1486 | ` * Install superglobals in the given virtual machine.` |
|       - | 1487 | ` * Note on superglobals.` |
|       - | 1488 | ` *  According to the PHP language reference manual.` |
|       - | 1489 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1490 | `*   Description` |
|       - | 1491 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1492 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1493 | `*   global $variable; to access them within functions or methods.` |
|       - | 1494 | `*   These superglobal variables are:` |
|       - | 1495 | `*    $GLOBALS` |
|       - | 1496 | `*    $_SERVER` |
|       - | 1497 | `*    $_GET` |
|       - | 1498 | `*    $_POST` |
|       - | 1499 | `*    $_FILES` |
|       - | 1500 | `*    $_COOKIE` |
|       - | 1501 | `*    $_SESSION` |
|       - | 1502 | `*    $_REQUEST` |
|       - | 1503 | `*    $_ENV` |
|       - | 1504 | `*/` |
|    3222 | 1505 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       5 | 1506 |  |
|       - | 1507 | `	static const char * azSuper[] = {` |
|       - | 1508 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1509 | `		"_GET",      /* $_GET */` |
|       - | 1510 | `		"_POST",     /* $_POST */` |
|       - | 1511 | `		"_FILES",    /* $_FILES */` |
|       - | 1512 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1513 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1514 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1515 | `		"_ENV",      /* $_ENV */` |
|       - | 1516 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1517 | `		"argv"       /* $argv */` |
|       - | 1518 | `	};` |
|       - | 1519 | `	ph7_hashmap *pMap;` |
|       - | 1520 | `	ph7_value *pObj;` |
|       - | 1521 | `	SyString *pFile;` |
|       - | 1522 | `	sxi32 rc;` |
|       - | 1523 | `	sxu32 n;` |
|       - | 1524 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    3227 | 1525 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    3227 | 1526 | `	if( pMap == 0 ){` |
|     ! 0 | 1527 | `		return SXERR_MEM;` |
|       - | 1528 | `	}` |
|    3227 | 1529 | `	pVm->pGlobal = pMap;` |
|       - | 1530 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    3227 | 1531 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    3227 | 1532 | `	if( pObj == 0 ){` |
|     ! 0 | 1533 | `		return SXERR_MEM;` |
|       - | 1534 | `	}` |
|    3227 | 1535 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1536 | `	/* Record object index */` |
|    3227 | 1537 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1538 | `	/* Install the special $GLOBALS array */` |
|    3227 | 1539 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    3227 | 1540 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1541 | `		return rc;` |
|       - | 1542 | `	}` |
|       - | 1543 | `	/* Install superglobals now */` |
|   35447 | 1544 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1545 | `		ph7_value *pSuper;` |
|       - | 1546 | `		/* Request an empty array */` |
|   32225 | 1547 | `		pSuper = ph7_new_array(&(*pVm));` |
|   32225 | 1548 | `		if( pSuper == 0 ){` |
|     ! 0 | 1549 | `			return SXERR_MEM;` |
|       - | 1550 | `		}` |
|       - | 1551 | `		/* Install */` |
|   32225 | 1552 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   32225 | 1553 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1554 | `			return rc;` |
|       - | 1555 | `		}` |
|       - | 1556 | `		/* Release the value now it have been installed */` |
|   32225 | 1557 | `		ph7_release_value(&(*pVm),pSuper);` |
|   16115 | 1558 | `	}` |
|       - | 1559 | `	/* Set some $_SERVER entries */` |
|    3227 | 1560 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1561 | `	/*` |
|       - | 1562 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1563 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1564 | `	 */` |
|    6445 | 1565 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1566 | `		"SCRIPT_FILENAME",` |
|    1611 | 1567 | `		pFile ? pFile->zString : ":Memory:",` |
|    3218 | 1568 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1569 | `		);` |
|       - | 1570 | `	/* All done,all super-global are installed now */` |
|    3227 | 1571 | `	return SXRET_OK;` |
|    1616 | 1572 |  |
|       - | 1573 | `/*` |
|       - | 1574 | ` * Release a hashmap.` |
|       - | 1575 | ` */` |
|   60150 | 1576 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       5 | 1577 |  |
|       - | 1578 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   60155 | 1579 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1580 | `	sxu32 n;` |
|   60155 | 1581 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1582 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1583 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1584 | `		return SXRET_OK;` |
|       - | 1585 | `	}` |
|       - | 1586 | `	/* Start the release process */` |
|   60155 | 1587 | `	n = 0;` |
|   60155 | 1588 | `	pEntry = pMap->pFirst;` |
| 1585802 | 1589 | `	for(;;){` |
| 3171609 | 1590 | `		if( n >= pMap->nEntry ){` |
|   60155 | 1591 | `			break;` |
|       - | 1592 | `		}` |
| 3111459 | 1593 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1594 | `		/* Remove the reference from the foreign table */` |
| 3111459 | 1595 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3111459 | 1596 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1597 | `			/* Restore the ph7_value to the free list */` |
| 3111449 | 1598 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1555722 | 1599 | `		}` |
|       - | 1600 | `		/* Release the node */` |
| 3111459 | 1601 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   72811 | 1602 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   36403 | 1603 | `		}` |
| 3111459 | 1604 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1605 | `		/* Point to the next entry */` |
| 3111459 | 1606 | `		pEntry = pNext;` |
| 3111459 | 1607 | `		n++;` |
|       5 | 1608 | `	}` |
|   60155 | 1609 | `	if( pMap->nEntry > 0 ){` |
|       - | 1610 | `		/* Release the hash bucket */` |
|   53425 | 1611 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   26710 | 1612 | `	}` |
|   60155 | 1613 | `	if( FreeDS ){` |
|       - | 1614 | `		/* Free the whole instance */` |
|   60139 | 1615 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   30072 | 1616 | `	}else{` |
|       - | 1617 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1618 | `		pMap->apBucket = 0;` |
|      17 | 1619 | `		pMap->iNextIdx = 0;` |
|      17 | 1620 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1621 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1622 | `	}` |
|   60155 | 1623 | `	return SXRET_OK;` |
|   30080 | 1624 |  |
|       - | 1625 | `/*` |
|       - | 1626 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1627 | ` * If the count reaches zero which mean no more variables` |
|       - | 1628 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1629 | ` */` |
|  654418 | 1630 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       5 | 1631 |  |
|  654423 | 1632 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1633 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  654423 | 1634 | `	pMap->iRef--;` |
|  654423 | 1635 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   60119 | 1636 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   30057 | 1637 | `	}` |
|  654423 | 1638 |  |
|       - | 1639 | `/*` |
|       - | 1640 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1641 | ` * Write a pointer to the target node on success.` |
|       - | 1642 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1643 | ` */` |
|  123172 | 1644 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1645 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1646 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1647 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1648 | `	)` |
|       5 | 1649 |  |
|       - | 1650 | `	sxi32 rc;` |
|  123177 | 1651 | `	if( pMap->nEntry < 1 ){` |
|       - | 1652 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1653 | `		 */` |
|      60 | 1654 | `		return SXERR_NOTFOUND;` |
|       - | 1655 | `	}` |
|  123121 | 1656 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  123121 | 1657 | `	return rc;` |
|   61591 | 1658 |  |
|       - | 1659 | `/*` |
|       - | 1660 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1661 | ` * hashmap.` |
|       - | 1662 | ` * If a node with the given key already exists in the database` |
|       - | 1663 | ` * then this function overwrite the old value.` |
|       - | 1664 | ` */` |
| 2536604 | 1665 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1666 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1667 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1668 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1669 | `	)` |
|       5 | 1670 |  |
|       - | 1671 | `	sxi32 rc;` |
| 2536609 | 1672 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1673 | `		/*` |
|       - | 1674 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1675 | `		 */` |
|     ! 0 | 1676 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1677 | `		return SXRET_OK;` |
|       - | 1678 | `	}` |
| 2536609 | 1679 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2536609 | 1680 | `	return rc;` |
| 1268307 | 1681 |  |
|       - | 1682 | `/*` |
|       - | 1683 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|       - | 1684 | ` *   - String keys overwrite same-key entries in pDest.` |
|       - | 1685 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|       - | 1686 | ` * This is the same routine that backs array_merge().` |
|       - | 1687 | ` */` |
|      52 | 1688 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1689 |  |
|      53 | 1690 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|       1 | 1691 |  |
|       - | 1692 | `/*` |
|       - | 1693 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1694 | ` * hashmap.` |
|       - | 1695 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1696 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1697 | ` * The insertion by reference is triggered when the following` |
|       - | 1698 | ` * expression is encountered.` |
|       - | 1699 | ` * $var = 10;` |
|       - | 1700 | ` *  $a = array(&var);` |
|       - | 1701 | ` * OR` |
|       - | 1702 | ` *  $a[] =& $var;` |
|       - | 1703 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1704 | ` * over it's contents.` |
|       - | 1705 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1706 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1707 | ` * Example:` |
|       - | 1708 | ` *  $var = 10;` |
|       - | 1709 | ` *  $a[] =& $var;` |
|       - | 1710 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1711 | ` *  //Unset the foreign ph7_value now` |
|       - | 1712 | ` *  unset($var);` |
|       - | 1713 | ` *  echo count($a); //0` |
|       - | 1714 | ` * Note that this is a PH7 eXtension.` |
|       - | 1715 | ` * Refer to the official documentation for more information.` |
|       - | 1716 | ` * If a node with the given key already exists in the database` |
|       - | 1717 | ` * then this function overwrite the old value.` |
|       - | 1718 | ` */` |
|   43064 | 1719 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1720 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1721 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1722 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1723 | `	)` |
|       5 | 1724 |  |
|       - | 1725 | `	sxi32 rc;` |
|   43069 | 1726 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1727 | `		/*` |
|       - | 1728 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1729 | `		 */` |
|     ! 0 | 1730 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1731 | `		return SXRET_OK;` |
|       - | 1732 | `	}` |
|   43069 | 1733 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   43069 | 1734 | `	return rc;` |
|   21537 | 1735 |  |
|       - | 1736 | `/*` |
|       - | 1737 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1738 | ` */` |
|   26694 | 1739 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       5 | 1740 |  |
|       - | 1741 | `	/* Reset the loop cursor */` |
|   26699 | 1742 | `	pMap->pCur = pMap->pFirst;` |
|   26699 | 1743 |  |
|       - | 1744 | `/*` |
|       - | 1745 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1746 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1747 | ` * return NULL.` |
|       - | 1748 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1749 | ` */` |
|  222370 | 1750 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       5 | 1751 |  |
|  222375 | 1752 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  222375 | 1753 | `	if( pCur == 0 ){` |
|       - | 1754 | `		/* End of the list,return null */` |
|   13371 | 1755 | `		return 0;` |
|       - | 1756 | `	}` |
|       - | 1757 | `	/* Advance the node cursor */` |
|  209009 | 1758 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  209009 | 1759 | `	return pCur;` |
|  111190 | 1760 |  |
|       - | 1761 | `/*` |
|       - | 1762 | ` * Extract a node value.` |
|       - | 1763 | ` */` |
|  527018 | 1764 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       5 | 1765 |  |
|  527023 | 1766 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  527023 | 1767 | `	if( pEntry ){` |
|  527023 | 1768 | `		if( bStore ){` |
|  209193 | 1769 | `			PH7_MemObjStore(pEntry,pValue);` |
|  104599 | 1770 | `		}else{` |
|  317835 | 1771 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1772 | `		}` |
|  263582 | 1773 | `	}else{` |
|     ! 0 | 1774 | `		PH7_MemObjRelease(pValue);` |
|       - | 1775 | `	}` |
|  527023 | 1776 |  |
|       - | 1777 | `/*` |
|       - | 1778 | ` * Extract a node key.` |
|       - | 1779 | ` */` |
|  131082 | 1780 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       5 | 1781 |  |
|       - | 1782 | `	/* Fill with the current key */` |
|  131087 | 1783 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  130613 | 1784 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1785 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1786 | `		}` |
|  130613 | 1787 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  130613 | 1788 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   65309 | 1789 | `	}else{` |
|     477 | 1790 | `		SyBlobReset(&pKey->sBlob);` |
|     477 | 1791 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     477 | 1792 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1793 | `	}` |
|  131087 | 1794 |  |
|       - | 1795 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1796 | `/*` |
|       - | 1797 | ` * Store the address of nodes value in the given container.` |
|       - | 1798 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1799 | ` * defined in 'builtin.c' for more information.` |
|       - | 1800 | ` */` |
|      10 | 1801 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1802 |  |
|      11 | 1803 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1804 | `	ph7_value *pValue;` |
|       - | 1805 | `	sxu32 n;` |
|       - | 1806 | `	/* Initialize the container */` |
|      11 | 1807 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1808 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1809 | `		/* Extract node value */` |
|      17 | 1810 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1811 | `		if( pValue ){` |
|      17 | 1812 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1813 | `		}` |
|       - | 1814 | `		/* Point to the next entry */` |
|      17 | 1815 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1816 | `	}` |
|       - | 1817 | `	/* Total inserted entries */` |
|      11 | 1818 | `	return (int)SySetUsed(pOut);` |
|       1 | 1819 |  |
|       - | 1820 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1821 | `/* SPDX-SnippetBegin */` |
|       - | 1822 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - | 1823 | `/* SPDX-License-Identifier: blessing */` |
|       - | 1824 | `/*` |
|       - | 1825 | ` * Merge sort.` |
|       - | 1826 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1827 | ` * Status: Public domain` |
|       - | 1828 | ` */` |
|       - | 1829 | `/* Node comparison callback signature */` |
|       - | 1830 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1831 | `/*` |
|       - | 1832 | `** Inputs:` |
|       - | 1833 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1834 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1835 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1836 | `**` |
|       - | 1837 | `** Return Value:` |
|       - | 1838 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1839 | `**   of both a and b.` |
|       - | 1840 | `**` |
|       - | 1841 | `** Side effects:` |
|       - | 1842 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1843 | `**   changed.` |
|       - | 1844 | `*/` |
|   32818 | 1845 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1846 |  |
|       - | 1847 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1848 | `    /* Prevent compiler warning */` |
|   32823 | 1849 | `	result.pNext = result.pPrev = 0;` |
|   32823 | 1850 | `	pTail = &result;` |
|   99728 | 1851 | `	while( pA && pB ){` |
|   66910 | 1852 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   44692 | 1853 | `			pTail->pPrev = pA;` |
|   44692 | 1854 | `			pA->pNext = pTail;` |
|   44692 | 1855 | `			pTail = pA;` |
|   44692 | 1856 | `			pA = pA->pPrev;` |
|   22340 | 1857 | `		}else{` |
|   22223 | 1858 | `			pTail->pPrev = pB;` |
|   22223 | 1859 | `			pB->pNext = pTail;` |
|   22223 | 1860 | `			pTail = pB;` |
|   22223 | 1861 | `			pB = pB->pPrev;` |
|       - | 1862 | `		}` |
|       5 | 1863 | `	}` |
|   32823 | 1864 | `	if( pA ){` |
|   23006 | 1865 | `		pTail->pPrev = pA;` |
|   23006 | 1866 | `		pA->pNext = pTail;` |
|   21324 | 1867 | `	}else if( pB ){` |
|    9604 | 1868 | `		pTail->pPrev = pB;` |
|    9604 | 1869 | `		pB->pNext = pTail;` |
|    4803 | 1870 | `	}else{` |
|     223 | 1871 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1872 | `	}` |
|   32823 | 1873 | `	return result.pPrev;` |
|       5 | 1874 |  |
|       - | 1875 | `/*` |
|       - | 1876 | `** Inputs:` |
|       - | 1877 | `**   Map:       Input hashmap` |
|       - | 1878 | `**   cmp:       A comparison function.` |
|       - | 1879 | `**` |
|       - | 1880 | `** Return Value:` |
|       - | 1881 | `**   Sorted hashmap.` |
|       - | 1882 | `**` |
|       - | 1883 | `** Side effects:` |
|       - | 1884 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1885 | `*/` |
|       - | 1886 | `#define N_SORT_BUCKET  32` |
|     680 | 1887 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1888 |  |
|       - | 1889 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1890 | `	sxu32 i;` |
|     685 | 1891 | `	SyZero(a,sizeof(a));` |
|       - | 1892 | `	/* Point to the first inserted entry */` |
|     685 | 1893 | `	pIn = pMap->pFirst;` |
|   13617 | 1894 | `	while( pIn ){` |
|   12937 | 1895 | `		p = pIn;` |
|   12937 | 1896 | `		pIn = p->pPrev;` |
|   12937 | 1897 | `		p->pPrev = 0;` |
|   24675 | 1898 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   24675 | 1899 | `			if( a[i]==0 ){` |
|   12937 | 1900 | `				a[i] = p;` |
|   12937 | 1901 | `				break;` |
|     ! 0 | 1902 | `			}else{` |
|   11743 | 1903 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   11743 | 1904 | `				a[i] = 0;` |
|       - | 1905 | `			}` |
|    5874 | 1906 | `		}` |
|   12937 | 1907 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1908 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1909 | `			 * But that is impossible.` |
|       - | 1910 | `			 */` |
|     ! 0 | 1911 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1912 | `		}` |
|       5 | 1913 | `	}` |
|     685 | 1914 | `	p = a[0];` |
|   21765 | 1915 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   21085 | 1916 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   10545 | 1917 | `	}` |
|     685 | 1918 | `	p->pNext = 0;` |
|       - | 1919 | `	/* Reflect the change */` |
|     685 | 1920 | `	pMap->pFirst = p;` |
|       - | 1921 | `	/* Reset the loop cursor */` |
|     685 | 1922 | `	pMap->pCur = pMap->pFirst;` |
|     685 | 1923 | `	return SXRET_OK;` |
|       5 | 1924 |  |
|       - | 1925 | `/* SPDX-SnippetEnd */` |
|       - | 1926 | `/*` |
|       - | 1927 | ` * Node comparison callback.` |
|       - | 1928 | ` * used-by: [sort(),asort(),...]` |
|       - | 1929 | ` */` |
|   66704 | 1930 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 | 1931 |  |
|       - | 1932 | `	ph7_value sA,sB;` |
|       - | 1933 | `	sxi32 iFlags;` |
|       - | 1934 | `	int rc;` |
|   66709 | 1935 | `	if( pCmpData == 0 ){` |
|       - | 1936 | `		/* Perform a standard comparison */` |
|   66685 | 1937 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   66685 | 1938 | `		return rc;` |
|       - | 1939 | `	}` |
|      25 | 1940 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1941 | `	/* Duplicate node values */` |
|      25 | 1942 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 1943 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 1944 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 1945 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 1946 | `	if( iFlags == 5 ){` |
|       - | 1947 | `		/* String cast */` |
|       - | 1948 | `		const char *zA,*zB;` |
|       - | 1949 | `		sxu32 nA,nB,nMin;` |
|      15 | 1950 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1951 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1952 | `		}` |
|      15 | 1953 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1954 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1955 | `		}` |
|       - | 1956 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 1957 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 1958 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 1959 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 1960 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 1961 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 1962 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 1963 | `		if( rc == 0 ){` |
|       5 | 1964 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1965 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1966 | `		}` |
|       8 | 1967 | `	}else{` |
|       - | 1968 | `		/* Numeric cast */` |
|      11 | 1969 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1970 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1971 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1972 | `	}` |
|      25 | 1973 | `	PH7_MemObjRelease(&sA);` |
|      25 | 1974 | `	PH7_MemObjRelease(&sB);` |
|      25 | 1975 | `	return rc;` |
|   33391 | 1976 |  |
|       - | 1977 | `/*` |
|       - | 1978 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1979 | ` * used-by: [ksort()]` |
|       - | 1980 | ` */` |
|      14 | 1981 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1982 |  |
|       - | 1983 | `	sxi32 rc;` |
|       7 | 1984 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 1985 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1986 | `		/* Perform a string comparison */` |
|       5 | 1987 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1988 | `	}else{` |
|       - | 1989 | `		SyString sStr;` |
|       - | 1990 | `		sxi64 iA,iB;` |
|       - | 1991 | `		/* Perform a numeric comparison */` |
|      11 | 1992 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1993 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1994 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1995 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1996 | `				iA = 0;` |
|     ! 0 | 1997 | `			}else{` |
|     ! 0 | 1998 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1999 | `			}` |
|     ! 0 | 2000 | `		}else{` |
|      11 | 2001 | `			iA = pA->xKey.iKey;` |
|       - | 2002 | `		}` |
|      11 | 2003 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2004 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2005 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2006 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2007 | `				iB = 0;` |
|     ! 0 | 2008 | `			}else{` |
|     ! 0 | 2009 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2010 | `			}` |
|     ! 0 | 2011 | `		}else{` |
|      11 | 2012 | `			iB = pB->xKey.iKey;` |
|       - | 2013 | `		}` |
|      11 | 2014 | `		rc = (sxi32)(iA-iB);` |
|       - | 2015 | `	}` |
|       - | 2016 | `	/* Comparison result */` |
|      15 | 2017 | `	return rc;` |
|       1 | 2018 |  |
|       - | 2019 | `/*` |
|       - | 2020 | ` * Node comparison callback.` |
|       - | 2021 | ` * Used by: [rsort(),arsort()];` |
|       - | 2022 | ` */` |
|      78 | 2023 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2024 |  |
|       - | 2025 | `	ph7_value sA,sB;` |
|       - | 2026 | `	sxi32 iFlags;` |
|       - | 2027 | `	int rc;` |
|      79 | 2028 | `	if( pCmpData == 0 ){` |
|       - | 2029 | `		/* Perform a standard comparison */` |
|      59 | 2030 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 2031 | `		return -rc;` |
|       - | 2032 | `	}` |
|      21 | 2033 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 2034 | `	/* Duplicate node values */` |
|      21 | 2035 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 2036 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 2037 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 2038 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 2039 | `	if( iFlags == 5 ){` |
|       - | 2040 | `		/* String cast */` |
|       - | 2041 | `		const char *zA,*zB;` |
|       - | 2042 | `		sxu32 nA,nB,nMin;` |
|      11 | 2043 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2044 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 2045 | `		}` |
|      11 | 2046 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2047 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 2048 | `		}` |
|       - | 2049 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 2050 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 2051 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 2052 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 2053 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 2054 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 2055 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 2056 | `		if( rc == 0 ){` |
|       3 | 2057 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 2058 | `			else if( nA > nB ) rc = 1;` |
|       1 | 2059 | `		}` |
|       6 | 2060 | `	}else{` |
|       - | 2061 | `		/* Numeric cast */` |
|      11 | 2062 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 2063 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 2064 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 2065 | `	}` |
|      21 | 2066 | `	PH7_MemObjRelease(&sA);` |
|      21 | 2067 | `	PH7_MemObjRelease(&sB);` |
|      21 | 2068 | `	return -rc;` |
|      40 | 2069 |  |
|       - | 2070 | `/*` |
|       - | 2071 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2072 | ` * used-by: [usort(),uasort()]` |
|       - | 2073 | ` */` |
|      84 | 2074 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 2075 |  |
|       - | 2076 | `	ph7_value sResult,*pCallback;` |
|       - | 2077 | `	ph7_value *pV1,*pV2;` |
|       - | 2078 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2079 | `	sxi32 rc;` |
|       - | 2080 | `	/* Point to the desired callback */` |
|      86 | 2081 | `	pCallback = (ph7_value *)pCmpData;` |
|      86 | 2082 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2083 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2084 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       3 | 2085 | `		return 0;` |
|       - | 2086 | `	}` |
|       - | 2087 | `	/* initialize the result value */` |
|      84 | 2088 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 2089 | `	/* Extract nodes values */` |
|      84 | 2090 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      84 | 2091 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      84 | 2092 | `	apArg[0] = pV1;` |
|      84 | 2093 | `	apArg[1] = pV2;` |
|       - | 2094 | `	/* Invoke the callback */` |
|      84 | 2095 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      84 | 2096 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2097 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2098 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       3 | 2099 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       3 | 2100 | `		rc = 0;` |
|      83 | 2101 | `	}else if( rc != SXRET_OK ){` |
|       - | 2102 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2103 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2104 | `	}else{` |
|       - | 2105 | `		/* Extract callback result */` |
|      82 | 2106 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2107 | `			/* Perform an int cast */` |
|     ! 0 | 2108 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2109 | `		}` |
|      82 | 2110 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2111 | `	}` |
|      84 | 2112 | `	PH7_MemObjRelease(&sResult);` |
|       - | 2113 | `	/* Callback result */` |
|      84 | 2114 | `	return rc;` |
|      44 | 2115 |  |
|       - | 2116 | `/*` |
|       - | 2117 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2118 | ` * used-by: [krsort()]` |
|       - | 2119 | ` */` |
|       4 | 2120 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2121 |  |
|       - | 2122 | `	sxi32 rc;` |
|       2 | 2123 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2124 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2125 | `		/* Perform a string comparison */` |
|       5 | 2126 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2127 | `	}else{` |
|       - | 2128 | `		SyString sStr;` |
|       - | 2129 | `		sxi64 iA,iB;` |
|       - | 2130 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2131 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2132 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2133 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2134 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2135 | `				iA = 0;` |
|     ! 0 | 2136 | `			}else{` |
|     ! 0 | 2137 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2138 | `			}` |
|     ! 0 | 2139 | `		}else{` |
|     ! 0 | 2140 | `			iA = pA->xKey.iKey;` |
|       - | 2141 | `		}` |
|     ! 0 | 2142 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2143 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2144 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2145 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2146 | `				iB = 0;` |
|     ! 0 | 2147 | `			}else{` |
|     ! 0 | 2148 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2149 | `			}` |
|     ! 0 | 2150 | `		}else{` |
|     ! 0 | 2151 | `			iB = pB->xKey.iKey;` |
|       - | 2152 | `		}` |
|     ! 0 | 2153 | `		rc = (sxi32)(iA-iB);` |
|       - | 2154 | `	}` |
|       5 | 2155 | `	return -rc; /* Reverse result */` |
|       1 | 2156 |  |
|       - | 2157 | `/*` |
|       - | 2158 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2159 | ` * used-by: [uksort()]` |
|       - | 2160 | ` */` |
|       6 | 2161 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2162 |  |
|       - | 2163 | `	ph7_value sResult,*pCallback;` |
|       - | 2164 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2165 | `	ph7_value sK1,sK2;` |
|       - | 2166 | `	sxi32 rc;` |
|       - | 2167 | `	/* Point to the desired callback */` |
|       7 | 2168 | `	pCallback = (ph7_value *)pCmpData;` |
|       7 | 2169 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2170 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2171 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     ! 0 | 2172 | `		return 0;` |
|       - | 2173 | `	}` |
|       - | 2174 | `	/* initialize the result value */` |
|       7 | 2175 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2176 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2177 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2178 | `	/* Extract nodes keys */` |
|       7 | 2179 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2180 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2181 | `	apArg[0] = &sK1;` |
|       7 | 2182 | `	apArg[1] = &sK2;` |
|       - | 2183 | `	/* Mark keys as constants */` |
|       7 | 2184 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2185 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2186 | `	/* Invoke the callback */` |
|       7 | 2187 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2188 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2189 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2190 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     ! 0 | 2191 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     ! 0 | 2192 | `		rc = 0;` |
|       7 | 2193 | `	}else if( rc != SXRET_OK ){` |
|       - | 2194 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2195 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2196 | `	}else{` |
|       - | 2197 | `		/* Extract callback result */` |
|       7 | 2198 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2199 | `			/* Perform an int cast */` |
|     ! 0 | 2200 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2201 | `		}` |
|       7 | 2202 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2203 | `	}` |
|       7 | 2204 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2205 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2206 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2207 | `	/* Callback result */` |
|       7 | 2208 | `	return rc;` |
|       4 | 2209 |  |
|       - | 2210 | `/*` |
|       - | 2211 | ` * Node comparison callback: Random node comparison.` |
|       - | 2212 | ` * used-by: [shuffle()]` |
|       - | 2213 | ` */` |
|      15 | 2214 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2215 |  |
|       - | 2216 | `	sxu32 n;` |
|       7 | 2217 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 2218 | `	SXUNUSED(pCmpData);` |
|       - | 2219 | `	/* Grab a random number */` |
|      16 | 2220 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2221 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2222 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2223 | `	 */` |
|      16 | 2224 | `	return n&1 ? 1 : -1;` |
|       1 | 2225 |  |
|       - | 2226 | `/*` |
|       - | 2227 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2228 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2229 | ` */` |
|     632 | 2230 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       5 | 2231 |  |
|       - | 2232 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2233 | `	sxu32 i;` |
|       - | 2234 | `	/* Rehash all entries */` |
|     637 | 2235 | `	pLast = p = pMap->pFirst;` |
|     637 | 2236 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     637 | 2237 | `	i = 0;` |
|    6697 | 2238 | `	for( ;; ){` |
|   13399 | 2239 | `		if( i >= pMap->nEntry ){` |
|     637 | 2240 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     637 | 2241 | `			break;` |
|       - | 2242 | `		}` |
|   12767 | 2243 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2244 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2245 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2246 | `			/* Change key type */` |
|       5 | 2247 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2248 | `		}` |
|   12767 | 2249 | `		HashmapRehashIntNode(p);` |
|       - | 2250 | `		/* Point to the next entry */` |
|   12767 | 2251 | `		i++;` |
|   12767 | 2252 | `		pLast = p;` |
|   12767 | 2253 | `		p = p->pPrev; /* Reverse link */` |
|       5 | 2254 | `	}` |
|     637 | 2255 |  |
|       - | 2256 | `/*` |
|       - | 2257 | ` * Array functions implementation.` |
|       - | 2258 | ` * Status:` |
|       - | 2259 | ` *  Stable.` |
|       - | 2260 | ` */` |
|       - | 2261 | `/*` |
|       - | 2262 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2263 | ` * Sort an array.` |
|       - | 2264 | ` * Parameters` |
|       - | 2265 | ` *  $array` |
|       - | 2266 | ` *   The input array.` |
|       - | 2267 | ` * $sort_flags` |
|       - | 2268 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2269 | ` *  Sorting type flags:` |
|       - | 2270 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2271 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2272 | ` *   SORT_STRING - compare items as strings` |
|       - | 2273 | ` * Return` |
|       - | 2274 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2275 | ` *` |
|       - | 2276 | ` */` |
|     978 | 2277 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2278 |  |
|       - | 2279 | `	ph7_hashmap *pMap;` |
|       - | 2280 | `	/* Make sure we are dealing with a valid hashmap */` |
|     983 | 2281 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2282 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2283 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2284 | `		return PH7_OK;` |
|       - | 2285 | `	}` |
|       - | 2286 | `	/* Point to the internal representation of the input hashmap */` |
|     983 | 2287 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     983 | 2288 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     983 | 2289 | `	if( pMap->nEntry > 1 ){` |
|     623 | 2290 | `		sxi32 iCmpFlags = 0;` |
|     623 | 2291 | `		if( nArg > 1 ){` |
|       - | 2292 | `			/* Extract comparison flags */` |
|       3 | 2293 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2294 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2295 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2296 | `			}` |
|       1 | 2297 | `		}` |
|       - | 2298 | `		/* Do the merge sort */` |
|     623 | 2299 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2300 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     623 | 2301 | `		HashmapSortRehash(pMap);` |
|     309 | 2302 | `	}` |
|       - | 2303 | `	/* All done,return TRUE */` |
|     983 | 2304 | `	ph7_result_bool(pCtx,1);` |
|     983 | 2305 | `	return PH7_OK;` |
|     494 | 2306 |  |
|       - | 2307 | `/*` |
|       - | 2308 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2309 | ` *  Sort an array and maintain index association.` |
|       - | 2310 | ` * Parameters` |
|       - | 2311 | ` *  $array` |
|       - | 2312 | ` *   The input array.` |
|       - | 2313 | ` * $sort_flags` |
|       - | 2314 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2315 | ` *  Sorting type flags:` |
|       - | 2316 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2317 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2318 | ` *   SORT_STRING - compare items as strings` |
|       - | 2319 | ` * Return` |
|       - | 2320 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2321 | ` */` |
|      32 | 2322 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2323 |  |
|       - | 2324 | `	ph7_hashmap *pMap;` |
|       - | 2325 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2326 | `	if( nArg < 1 ){` |
|       3 | 2327 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2328 | `			"ArgumentCountError",` |
|       - | 2329 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2330 | `			);` |
|       - | 2331 | `	}` |
|       - | 2332 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2333 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2334 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2335 | `			"TypeError",` |
|       - | 2336 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2337 | `			ph7_type_name(apArg[0])` |
|       - | 2338 | `			);` |
|       - | 2339 | `	}` |
|       - | 2340 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2341 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2342 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2343 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2344 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2345 | `		if( nArg > 1 ){` |
|       - | 2346 | `			/* Extract comparison flags */` |
|       5 | 2347 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2348 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2349 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2350 | `			}` |
|       2 | 2351 | `		}` |
|       - | 2352 | `		/* Do the merge sort */` |
|      19 | 2353 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2354 | `		/* Fix the last link broken by the merge */` |
|      45 | 2355 | `		while(pMap->pLast->pPrev){` |
|      27 | 2356 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2357 | `		}` |
|       9 | 2358 | `	}` |
|       - | 2359 | `	/* All done,return TRUE */` |
|      23 | 2360 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2361 | `	return PH7_OK;` |
|      21 | 2362 |  |
|       - | 2363 | `/*` |
|       - | 2364 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2365 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2366 | ` * Parameters` |
|       - | 2367 | ` *  $array` |
|       - | 2368 | ` *   The input array.` |
|       - | 2369 | ` * $sort_flags` |
|       - | 2370 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2371 | ` *  Sorting type flags:` |
|       - | 2372 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2373 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2374 | ` *   SORT_STRING - compare items as strings` |
|       - | 2375 | ` * Return` |
|       - | 2376 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2377 | ` */` |
|      32 | 2378 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2379 |  |
|       - | 2380 | `	ph7_hashmap *pMap;` |
|       - | 2381 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2382 | `	if( nArg < 1 ){` |
|       3 | 2383 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2384 | `			"ArgumentCountError",` |
|       - | 2385 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2386 | `			);` |
|       - | 2387 | `	}` |
|       - | 2388 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2389 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2390 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2391 | `			"TypeError",` |
|       - | 2392 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2393 | `			ph7_type_name(apArg[0])` |
|       - | 2394 | `			);` |
|       - | 2395 | `	}` |
|       - | 2396 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2397 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2398 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2399 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2400 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2401 | `		if( nArg > 1 ){` |
|       - | 2402 | `			/* Extract comparison flags */` |
|       5 | 2403 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2404 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2405 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2406 | `			}` |
|       2 | 2407 | `		}` |
|       - | 2408 | `		/* Do the merge sort */` |
|      19 | 2409 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2410 | `		/* Fix the last link broken by the merge */` |
|      35 | 2411 | `		while(pMap->pLast->pPrev){` |
|      17 | 2412 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2413 | `		}` |
|       9 | 2414 | `	}` |
|       - | 2415 | `	/* All done,return TRUE */` |
|      23 | 2416 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2417 | `	return PH7_OK;` |
|      21 | 2418 |  |
|       - | 2419 | `/*` |
|       - | 2420 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2421 | ` *  Sort an array by key.` |
|       - | 2422 | ` * Parameters` |
|       - | 2423 | ` *  $array` |
|       - | 2424 | ` *   The input array.` |
|       - | 2425 | ` * $sort_flags` |
|       - | 2426 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2427 | ` *  Sorting type flags:` |
|       - | 2428 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2429 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2430 | ` *   SORT_STRING - compare items as strings` |
|       - | 2431 | ` * Return` |
|       - | 2432 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2433 | ` */` |
|       4 | 2434 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2435 |  |
|       - | 2436 | `	ph7_hashmap *pMap;` |
|       - | 2437 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2438 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2439 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2440 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2441 | `		return PH7_OK;` |
|       - | 2442 | `	}` |
|       - | 2443 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2444 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2445 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2446 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2447 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2448 | `		if( nArg > 1 ){` |
|       - | 2449 | `			/* Extract comparison flags */` |
|     ! 0 | 2450 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2451 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2452 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2453 | `			}` |
|     ! 0 | 2454 | `		}` |
|       - | 2455 | `		/* Do the merge sort */` |
|       5 | 2456 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2457 | `		/* Fix the last link broken by the merge */` |
|      15 | 2458 | `		while(pMap->pLast->pPrev){` |
|      11 | 2459 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2460 | `		}` |
|       2 | 2461 | `	}` |
|       - | 2462 | `	/* All done,return TRUE */` |
|       5 | 2463 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2464 | `	return PH7_OK;` |
|       3 | 2465 |  |
|       - | 2466 | `/*` |
|       - | 2467 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2468 | ` *  Sort an array by key in reverse order.` |
|       - | 2469 | ` * Parameters` |
|       - | 2470 | ` *  $array` |
|       - | 2471 | ` *   The input array.` |
|       - | 2472 | ` * $sort_flags` |
|       - | 2473 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2474 | ` *  Sorting type flags:` |
|       - | 2475 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2476 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2477 | ` *   SORT_STRING - compare items as strings` |
|       - | 2478 | ` * Return` |
|       - | 2479 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2480 | ` */` |
|       2 | 2481 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2482 |  |
|       - | 2483 | `	ph7_hashmap *pMap;` |
|       - | 2484 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2485 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2486 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2487 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2488 | `		return PH7_OK;` |
|       - | 2489 | `	}` |
|       - | 2490 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2491 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2492 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2493 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2494 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2495 | `		if( nArg > 1 ){` |
|       - | 2496 | `			/* Extract comparison flags */` |
|     ! 0 | 2497 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2498 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2499 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2500 | `			}` |
|     ! 0 | 2501 | `		}` |
|       - | 2502 | `		/* Do the merge sort */` |
|       3 | 2503 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2504 | `		/* Fix the last link broken by the merge */` |
|       7 | 2505 | `		while(pMap->pLast->pPrev){` |
|       5 | 2506 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2507 | `		}` |
|       1 | 2508 | `	}` |
|       - | 2509 | `	/* All done,return TRUE */` |
|       3 | 2510 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2511 | `	return PH7_OK;` |
|       2 | 2512 |  |
|       - | 2513 | `/*` |
|       - | 2514 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2515 | ` * Sort an array in reverse order.` |
|       - | 2516 | ` * Parameters` |
|       - | 2517 | ` *  $array` |
|       - | 2518 | ` *   The input array.` |
|       - | 2519 | ` * $sort_flags` |
|       - | 2520 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2521 | ` *  Sorting type flags:` |
|       - | 2522 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2523 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2524 | ` *   SORT_STRING - compare items as strings` |
|       - | 2525 | ` * Return` |
|       - | 2526 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2527 | ` */` |
|       2 | 2528 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2529 |  |
|       - | 2530 | `	ph7_hashmap *pMap;` |
|       - | 2531 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2532 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2533 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2534 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2535 | `		return PH7_OK;` |
|       - | 2536 | `	}` |
|       - | 2537 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2538 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2539 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2540 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2541 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2542 | `		if( nArg > 1 ){` |
|       - | 2543 | `			/* Extract comparison flags */` |
|     ! 0 | 2544 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2545 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2546 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2547 | `			}` |
|     ! 0 | 2548 | `		}` |
|       - | 2549 | `		/* Do the merge sort */` |
|       3 | 2550 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2551 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2552 | `		HashmapSortRehash(pMap);` |
|       1 | 2553 | `	}` |
|       - | 2554 | `	/* All done,return TRUE */` |
|       3 | 2555 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2556 | `	return PH7_OK;` |
|       2 | 2557 |  |
|       - | 2558 | `/*` |
|       - | 2559 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2560 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2561 | ` * Parameters` |
|       - | 2562 | ` *  $array` |
|       - | 2563 | ` *   The input array.` |
|       - | 2564 | ` * $cmp_function` |
|       - | 2565 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2566 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2567 | ` *  to, or greater than the second.` |
|       - | 2568 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2569 | ` * Return` |
|       - | 2570 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2571 | ` */` |
|      10 | 2572 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2573 |  |
|       - | 2574 | `	ph7_hashmap *pMap;` |
|       - | 2575 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2576 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2577 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2578 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2579 | `		return PH7_OK;` |
|       - | 2580 | `	}` |
|       - | 2581 | `	/* Point to the internal representation of the input hashmap */` |
|      12 | 2582 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      12 | 2583 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      12 | 2584 | `	if( pMap->nEntry > 1 ){` |
|      12 | 2585 | `		ph7_value *pCallback = 0;` |
|       - | 2586 | `		ProcNodeCmp xCmp;` |
|      12 | 2587 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      12 | 2588 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2589 | `			/* Point to the desired callback */` |
|      12 | 2590 | `			pCallback = apArg[1];` |
|       7 | 2591 | `		}else{` |
|       - | 2592 | `			/* Use the default comparison function */` |
|     ! 0 | 2593 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2594 | `		}` |
|       - | 2595 | `		/* Do the merge sort */` |
|      12 | 2596 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      12 | 2597 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2598 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      12 | 2599 | `		HashmapSortRehash(pMap);` |
|      12 | 2600 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2601 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 2602 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2603 | `			return PH7_EXCEPTION;` |
|       - | 2604 | `		}` |
|       4 | 2605 | `	}` |
|       - | 2606 | `	/* All done,return TRUE */` |
|      10 | 2607 | `	ph7_result_bool(pCtx,1);` |
|      10 | 2608 | `	return PH7_OK;` |
|       7 | 2609 |  |
|       - | 2610 | `/*` |
|       - | 2611 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2612 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2613 | ` *  and maintain index association.` |
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
|       2 | 2625 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|       3 | 2640 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2641 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2642 | `			/* Point to the desired callback */` |
|       3 | 2643 | `			pCallback = apArg[1];` |
|       2 | 2644 | `		}else{` |
|       - | 2645 | `			/* Use the default comparison function */` |
|     ! 0 | 2646 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2647 | `		}` |
|       - | 2648 | `		/* Do the merge sort */` |
|       3 | 2649 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2650 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2651 | `		/* Fix the last link broken by the merge */` |
|       5 | 2652 | `		while(pMap->pLast->pPrev){` |
|       3 | 2653 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2654 | `		}` |
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
|       - | 2666 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2667 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2668 | ` *  function and maintain index association.` |
|       - | 2669 | ` * Parameters` |
|       - | 2670 | ` *  $array` |
|       - | 2671 | ` *   The input array.` |
|       - | 2672 | ` * $cmp_function` |
|       - | 2673 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2674 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2675 | ` *  to, or greater than the second.` |
|       - | 2676 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2677 | ` * Return` |
|       - | 2678 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2679 | ` */` |
|       2 | 2680 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2681 |  |
|       - | 2682 | `	ph7_hashmap *pMap;` |
|       - | 2683 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2684 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2685 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2686 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2687 | `		return PH7_OK;` |
|       - | 2688 | `	}` |
|       - | 2689 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2690 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2691 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2692 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2693 | `		ph7_value *pCallback = 0;` |
|       - | 2694 | `		ProcNodeCmp xCmp;` |
|       3 | 2695 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2696 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2697 | `			/* Point to the desired callback */` |
|       3 | 2698 | `			pCallback = apArg[1];` |
|       2 | 2699 | `		}else{` |
|       - | 2700 | `			/* Use the default comparison function */` |
|     ! 0 | 2701 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2702 | `		}` |
|       - | 2703 | `		/* Do the merge sort */` |
|       3 | 2704 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2705 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2706 | `		/* Fix the last link broken by the merge */` |
|       3 | 2707 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2708 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2709 | `		}` |
|       3 | 2710 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2711 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2712 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2713 | `			return PH7_EXCEPTION;` |
|       - | 2714 | `		}` |
|       1 | 2715 | `	}` |
|       - | 2716 | `	/* All done,return TRUE */` |
|       3 | 2717 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2718 | `	return PH7_OK;` |
|       2 | 2719 |  |
|       - | 2720 | `/*` |
|       - | 2721 | ` * bool shuffle(array &$array)` |
|       - | 2722 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2723 | ` * Parameters` |
|       - | 2724 | ` *  $array` |
|       - | 2725 | ` *   The input array.` |
|       - | 2726 | ` * Return` |
|       - | 2727 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2728 | ` *` |
|       - | 2729 | ` */` |
|       2 | 2730 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2731 |  |
|       - | 2732 | `	ph7_hashmap *pMap;` |
|       - | 2733 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2734 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2735 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2736 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2737 | `		return PH7_OK;` |
|       - | 2738 | `	}` |
|       - | 2739 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2740 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2741 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2742 | `	if( pMap->nEntry > 1 ){` |
|       - | 2743 | `		/* Do the merge sort */` |
|       3 | 2744 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2745 | `		/* Fix the last link broken by the merge */` |
|       9 | 2746 | `		while(pMap->pLast->pPrev){` |
|       7 | 2747 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2748 | `		}` |
|       1 | 2749 | `	}` |
|       - | 2750 | `	/* All done,return TRUE */` |
|       3 | 2751 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2752 | `	return PH7_OK;` |
|       2 | 2753 |  |
|       - | 2754 | `/*` |
|       - | 2755 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2756 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2757 | ` * Parameters` |
|       - | 2758 | ` *  $var` |
|       - | 2759 | ` *   The array or the object.` |
|       - | 2760 | ` * $mode` |
|       - | 2761 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2762 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2763 | ` *  all the elements of a multidimensional array.` |
|       - | 2764 | ` * Return` |
|       - | 2765 | ` *  Returns the number of elements in the array.` |
|       - | 2766 | ` */` |
|     824 | 2767 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2768 |  |
|     829 | 2769 | `	int bRecursive = FALSE;` |
|     829 | 2770 | `	int bCycleDetected = FALSE;` |
|       - | 2771 | `	sxi64 iCount;` |
|     829 | 2772 | `	if( nArg < 1 ){` |
|       3 | 2773 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2774 | `			"ArgumentCountError",` |
|       - | 2775 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2776 | `			);` |
|       - | 2777 | `	}` |
|     827 | 2778 | `	if( nArg > 2 ){` |
|       4 | 2779 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2780 | `			"ArgumentCountError",` |
|       - | 2781 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2782 | `			nArg` |
|       - | 2783 | `			);` |
|       - | 2784 | `	}` |
|       - | 2785 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - | 2786 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - | 2787 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|     825 | 2788 | `	if( nArg > 1 ){` |
|      45 | 2789 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      45 | 2790 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|      12 | 2791 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2792 | `				"ValueError",` |
|       - | 2793 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2794 | `				);` |
|       - | 2795 | `		}` |
|      34 | 2796 | `		bRecursive = iMode == 1;` |
|      16 | 2797 | `	}` |
|     817 | 2798 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2799 | `		/* Countable object: dispatch to ->count() */` |
|      31 | 2800 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      19 | 2801 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      19 | 2802 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      19 | 2803 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      16 | 2804 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2805 | `					"count",sizeof("count")-1);` |
|      16 | 2806 | `				if( pMeth ){` |
|       - | 2807 | `					ph7_value sResult;` |
|      16 | 2808 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      16 | 2809 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      16 | 2810 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      16 | 2811 | `					PH7_MemObjRelease(&sResult);` |
|      16 | 2812 | `					return PH7_OK;` |
|       - | 2813 | `				}` |
|     ! 0 | 2814 | `			}` |
|       1 | 2815 | `		}` |
|      22 | 2816 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2817 | `			"TypeError",` |
|       - | 2818 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       6 | 2819 | `			ph7_type_name(apArg[0])` |
|       - | 2820 | `			);` |
|       - | 2821 | `	}` |
|       - | 2822 | `	/* Count */` |
|     791 | 2823 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     791 | 2824 | `	if( bCycleDetected ){` |
|       3 | 2825 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2826 | `	}` |
|     791 | 2827 | `	ph7_result_int64(pCtx,iCount);` |
|     791 | 2828 | `	return PH7_OK;` |
|     417 | 2829 |  |
|       - | 2830 | `/*` |
|       - | 2831 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2832 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2833 | ` * Parameters` |
|       - | 2834 | ` * $key` |
|       - | 2835 | ` *   Value to check.` |
|       - | 2836 | ` * $search` |
|       - | 2837 | ` *  An array with keys to check.` |
|       - | 2838 | ` * Return` |
|       - | 2839 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2840 | ` */` |
|      84 | 2841 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2842 |  |
|       - | 2843 | `	sxi32 rc;` |
|      89 | 2844 | `	if( nArg != 2 ){` |
|       - | 2845 | `		/* PHP requires exactly two arguments */` |
|      12 | 2846 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2847 | `			"ArgumentCountError",` |
|       - | 2848 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2849 | `			nArg` |
|       - | 2850 | `			);` |
|       - | 2851 | `	}` |
|       - | 2852 | `	/* Make sure we are dealing with a valid hashmap */` |
|      83 | 2853 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2854 | `		/* Type mismatch -> TypeError */` |
|       8 | 2855 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2856 | `			"TypeError",` |
|       - | 2857 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2858 | `			ph7_type_name(apArg[1])` |
|       - | 2859 | `			);` |
|       - | 2860 | `	}` |
|       - | 2861 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      78 | 2862 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2863 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2864 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2865 | `			"use an empty string instead"` |
|       - | 2866 | `			);` |
|      77 | 2867 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2868 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2869 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2870 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2871 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2872 | `				,rVal` |
|       - | 2873 | `				);` |
|       1 | 2874 | `		}` |
|       1 | 2875 | `	}` |
|       - | 2876 | `	/* Perform the lookup */` |
|      78 | 2877 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2878 | `	/* lookup result */` |
|      78 | 2879 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      78 | 2880 | `	return PH7_OK;` |
|      47 | 2881 |  |
|       - | 2882 | `/*` |
|       - | 2883 | ` * value array_pop(array $array)` |
|       - | 2884 | ` *   POP the last inserted element from the array.` |
|       - | 2885 | ` * Parameter` |
|       - | 2886 | ` *  The array to get the value from.` |
|       - | 2887 | ` * Return` |
|       - | 2888 | ` *  Poped value or NULL on failure.` |
|       - | 2889 | ` */` |
|      18 | 2890 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2891 |  |
|       - | 2892 | `	ph7_hashmap *pMap;` |
|       - | 2893 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      23 | 2894 | `	if( nArg != 1 ){` |
|       8 | 2895 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2896 | `			"ArgumentCountError",` |
|       - | 2897 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2898 | `			nArg` |
|       - | 2899 | `			);` |
|       - | 2900 | `	}` |
|       - | 2901 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2902 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      18 | 2903 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 2904 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2905 | `			"Error",` |
|       - | 2906 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2907 | `			);` |
|       - | 2908 | `	}` |
|       - | 2909 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2910 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2911 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2912 | `			"TypeError",` |
|       - | 2913 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2914 | `			ph7_type_name(apArg[0])` |
|       - | 2915 | `			);` |
|       - | 2916 | `	}` |
|       9 | 2917 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2918 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2919 | `	if( pMap->nEntry < 1 ){` |
|       - | 2920 | `		/* Nothing to pop,return NULL */` |
|       3 | 2921 | `		ph7_result_null(pCtx);` |
|       2 | 2922 | `	}else{` |
|       7 | 2923 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2924 | `		ph7_value *pObj;` |
|       7 | 2925 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2926 | `		if( pObj ){` |
|       - | 2927 | `			/* Node value */` |
|       7 | 2928 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2929 | `			/* Unlink the node */` |
|       7 | 2930 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2931 | `		}else{` |
|     ! 0 | 2932 | `			ph7_result_null(pCtx);` |
|       - | 2933 | `		}` |
|       - | 2934 | `		/* Reset the cursor */` |
|       7 | 2935 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2936 | `	}` |
|       9 | 2937 | `	return PH7_OK;` |
|      14 | 2938 |  |
|       - | 2939 | `/*` |
|       - | 2940 | ` * int array_push($array,$var,...)` |
|       - | 2941 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2942 | ` * Parameters` |
|       - | 2943 | ` *  array` |
|       - | 2944 | ` *    The input array.` |
|       - | 2945 | ` *  var` |
|       - | 2946 | ` *   On or more value to push.` |
|       - | 2947 | ` * Return` |
|       - | 2948 | ` *  New array count (including old items).` |
|       - | 2949 | ` */` |
|      22 | 2950 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2951 |  |
|       - | 2952 | `	ph7_hashmap *pMap;` |
|       - | 2953 | `	sxi32 rc;` |
|       - | 2954 | `	int i;` |
|      27 | 2955 | `	if( nArg < 1 ){` |
|       4 | 2956 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2957 | `			"ArgumentCountError",` |
|       - | 2958 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2959 | `			nArg` |
|       - | 2960 | `			);` |
|       - | 2961 | `	}` |
|       - | 2962 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2963 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      24 | 2964 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 2965 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2966 | `			"Error",` |
|       - | 2967 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2968 | `			);` |
|       - | 2969 | `	}` |
|       - | 2970 | `	/* Make sure we are dealing with a valid hashmap */` |
|      18 | 2971 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2972 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2973 | `			"TypeError",` |
|       - | 2974 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2975 | `			ph7_type_name(apArg[0])` |
|       - | 2976 | `			);` |
|       - | 2977 | `	}` |
|       - | 2978 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2979 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2980 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2981 | `	/* Start pushing given values */` |
|      31 | 2982 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      17 | 2983 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      17 | 2984 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2985 | `			break;` |
|       - | 2986 | `		}` |
|       9 | 2987 | `	}` |
|       - | 2988 | `	/* Return the new count */` |
|      15 | 2989 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 2990 | `	return PH7_OK;` |
|      16 | 2991 |  |
|       - | 2992 | `/*` |
|       - | 2993 | ` * value array_shift(array $array)` |
|       - | 2994 | ` *   Shift an element off the beginning of array.` |
|       - | 2995 | ` * Parameter` |
|       - | 2996 | ` *  The array to get the value from.` |
|       - | 2997 | ` * Return` |
|       - | 2998 | ` *  Shifted value or NULL on failure.` |
|       - | 2999 | ` */` |
|      38 | 3000 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3001 |  |
|       - | 3002 | `	ph7_hashmap *pMap;` |
|       - | 3003 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      43 | 3004 | `	if( nArg != 1 ){` |
|       8 | 3005 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3006 | `			"ArgumentCountError",` |
|       - | 3007 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 3008 | `			nArg` |
|       - | 3009 | `			);` |
|       - | 3010 | `	}` |
|       - | 3011 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      39 | 3012 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 3013 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3014 | `			"Error",` |
|       - | 3015 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 3016 | `			);` |
|       - | 3017 | `	}` |
|       - | 3018 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 3019 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3020 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3021 | `			"TypeError",` |
|       - | 3022 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3023 | `			ph7_type_name(apArg[0])` |
|       - | 3024 | `			);` |
|       - | 3025 | `	}` |
|       - | 3026 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 3027 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      33 | 3028 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3029 | `	if( pMap->nEntry < 1 ){` |
|       - | 3030 | `		/* Empty hashmap,return NULL */` |
|       3 | 3031 | `		ph7_result_null(pCtx);` |
|       2 | 3032 | `	}else{` |
|      31 | 3033 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 3034 | `		ph7_value *pObj;` |
|       - | 3035 | `		sxu32 n;` |
|      31 | 3036 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      31 | 3037 | `		if( pObj ){` |
|       - | 3038 | `			/* Node value */` |
|      31 | 3039 | `			ph7_result_value(pCtx,pObj);` |
|       - | 3040 | `			/* Unlink the first node */` |
|      31 | 3041 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      18 | 3042 | `		}else{` |
|     ! 0 | 3043 | `			ph7_result_null(pCtx);` |
|       - | 3044 | `		}` |
|       - | 3045 | `		/* Rehash all int keys */` |
|      31 | 3046 | `		n = pMap->nEntry;` |
|      31 | 3047 | `		pEntry = pMap->pFirst;` |
|      31 | 3048 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 3049 | `		for(;;){` |
|      85 | 3050 | `			if( n < 1 ){` |
|      31 | 3051 | `				break;` |
|       - | 3052 | `			}` |
|      59 | 3053 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      59 | 3054 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 3055 | `			}` |
|       - | 3056 | `			/* Point to the next entry */` |
|      59 | 3057 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      59 | 3058 | `			n--;` |
|       5 | 3059 | `		}` |
|       - | 3060 | `		/* Reset the cursor */` |
|      31 | 3061 | `		pMap->pCur = pMap->pFirst;` |
|       - | 3062 | `	}` |
|      33 | 3063 | `	return PH7_OK;` |
|      24 | 3064 |  |
|       - | 3065 | `/*` |
|       - | 3066 | ` * Extract the node cursor value.` |
|       - | 3067 | ` */` |
|      24 | 3068 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 3069 |  |
|      25 | 3070 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 3071 | `	ph7_value *pVal;` |
|      25 | 3072 | `	if( pCur == 0 ){` |
|       - | 3073 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 3074 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3075 | `		return PH7_OK;` |
|       - | 3076 | `	}` |
|      25 | 3077 | `	if( iDirection != 0 ){` |
|       9 | 3078 | `		if( iDirection > 0 ){` |
|       - | 3079 | `			/* Point to the next entry */` |
|       7 | 3080 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 3081 | `			pCur = pMap->pCur;` |
|       4 | 3082 | `		}else{` |
|       - | 3083 | `			/* Point to the previous entry */` |
|       3 | 3084 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 3085 | `			pCur = pMap->pCur;` |
|       - | 3086 | `		}` |
|       9 | 3087 | `		if( pCur == 0 ){` |
|       - | 3088 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 3089 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3090 | `			return PH7_OK;` |
|       - | 3091 | `		}` |
|       4 | 3092 | `	}` |
|       - | 3093 | `	/* Point to the desired element */` |
|      25 | 3094 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 3095 | `	if( pVal ){` |
|      25 | 3096 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 3097 | `	}else{` |
|     ! 0 | 3098 | `		ph7_result_bool(pCtx,0);` |
|       - | 3099 | `	}` |
|      25 | 3100 | `	return PH7_OK;` |
|      13 | 3101 |  |
|       - | 3102 | `/*` |
|       - | 3103 | ` * value current(array $array)` |
|       - | 3104 | ` *  Return the current element in an array.` |
|       - | 3105 | ` * Parameter` |
|       - | 3106 | ` *  $input: The input array.` |
|       - | 3107 | ` * Return` |
|       - | 3108 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 3109 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3110 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3111 | ` *  is empty, current() returns FALSE.` |
|       - | 3112 | ` */` |
|      10 | 3113 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3114 |  |
|      11 | 3115 | `	if( nArg < 1 ){` |
|       - | 3116 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3117 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3118 | `		return PH7_OK;` |
|       - | 3119 | `	}` |
|       - | 3120 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 3121 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3122 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3123 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3124 | `		return PH7_OK;` |
|       - | 3125 | `	}` |
|      11 | 3126 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 3127 | `	return PH7_OK;` |
|       6 | 3128 |  |
|       - | 3129 | `/*` |
|       - | 3130 | ` * value next(array $input)` |
|       - | 3131 | ` *  Advance the internal array pointer of an array.` |
|       - | 3132 | ` * Parameter` |
|       - | 3133 | ` *  $input: The input array.` |
|       - | 3134 | ` * Return` |
|       - | 3135 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 3136 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 3137 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 3138 | ` */` |
|       6 | 3139 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3140 |  |
|       7 | 3141 | `	if( nArg < 1 ){` |
|       - | 3142 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3143 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3144 | `		return PH7_OK;` |
|       - | 3145 | `	}` |
|       - | 3146 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 3147 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3148 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3149 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3150 | `		return PH7_OK;` |
|       - | 3151 | `	}` |
|       7 | 3152 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 3153 | `	return PH7_OK;` |
|       4 | 3154 |  |
|       - | 3155 | `/*` |
|       - | 3156 | ` * value prev(array $input)` |
|       - | 3157 | ` *  Rewind the internal array pointer.` |
|       - | 3158 | ` * Parameter` |
|       - | 3159 | ` *  $input: The input array.` |
|       - | 3160 | ` * Return` |
|       - | 3161 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3162 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3163 | ` *  elements.` |
|       - | 3164 | ` */` |
|       2 | 3165 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3166 |  |
|       3 | 3167 | `	if( nArg < 1 ){` |
|       - | 3168 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3169 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3170 | `		return PH7_OK;` |
|       - | 3171 | `	}` |
|       - | 3172 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3173 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3174 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3175 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3176 | `		return PH7_OK;` |
|       - | 3177 | `	}` |
|       3 | 3178 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3179 | `	return PH7_OK;` |
|       2 | 3180 |  |
|       - | 3181 | `/*` |
|       - | 3182 | ` * value end(array $input)` |
|       - | 3183 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3184 | ` * Parameter` |
|       - | 3185 | ` *  $input: The input array.` |
|       - | 3186 | ` * Return` |
|       - | 3187 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3188 | ` */` |
|       2 | 3189 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3190 |  |
|       - | 3191 | `	ph7_hashmap *pMap;` |
|       3 | 3192 | `	if( nArg < 1 ){` |
|       - | 3193 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3194 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3195 | `		return PH7_OK;` |
|       - | 3196 | `	}` |
|       - | 3197 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3198 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3199 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3200 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3201 | `		return PH7_OK;` |
|       - | 3202 | `	}` |
|       - | 3203 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3204 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3205 | `	/* Point to the last node */` |
|       3 | 3206 | `	pMap->pCur = pMap->pLast;` |
|       - | 3207 | `	/* Return the last node value */` |
|       3 | 3208 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3209 | `	return PH7_OK;` |
|       2 | 3210 |  |
|       - | 3211 | `/*` |
|       - | 3212 | ` * value reset(array $array )` |
|       - | 3213 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3214 | ` * Parameter` |
|       - | 3215 | ` *  $input: The input array.` |
|       - | 3216 | ` * Return` |
|       - | 3217 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3218 | ` */` |
|       4 | 3219 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3220 |  |
|       - | 3221 | `	ph7_hashmap *pMap;` |
|       5 | 3222 | `	if( nArg < 1 ){` |
|       - | 3223 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3224 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3225 | `		return PH7_OK;` |
|       - | 3226 | `	}` |
|       - | 3227 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3228 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3229 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3230 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3231 | `		return PH7_OK;` |
|       - | 3232 | `	}` |
|       - | 3233 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3234 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3235 | `	/* Point to the first node */` |
|       5 | 3236 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3237 | `	/* Return the last node value if available */` |
|       5 | 3238 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3239 | `	return PH7_OK;` |
|       3 | 3240 |  |
|       - | 3241 | `/*` |
|       - | 3242 | ` * value key(array $array)` |
|       - | 3243 | ` *   Fetch a key from an array` |
|       - | 3244 | ` * Parameter` |
|       - | 3245 | ` *  $input` |
|       - | 3246 | ` *   The input array.` |
|       - | 3247 | ` * Return` |
|       - | 3248 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3249 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3250 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3251 | ` *  is empty, key() returns NULL.` |
|       - | 3252 | ` */` |
|       4 | 3253 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3254 |  |
|       - | 3255 | `	ph7_hashmap_node *pCur;` |
|       - | 3256 | `	ph7_hashmap *pMap;` |
|       5 | 3257 | `	if( nArg < 1 ){` |
|       - | 3258 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3259 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3260 | `		return PH7_OK;` |
|       - | 3261 | `	}` |
|       - | 3262 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3263 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3264 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3265 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3266 | `		return PH7_OK;` |
|       - | 3267 | `	}` |
|       5 | 3268 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3269 | `	pCur = pMap->pCur;` |
|       5 | 3270 | `	if( pCur == 0 ){` |
|       - | 3271 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3272 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3273 | `		return PH7_OK;` |
|       - | 3274 | `	}` |
|       5 | 3275 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3276 | `		/* Key is integer */` |
|     ! 0 | 3277 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3278 | `	}else{` |
|       - | 3279 | `		/* Key is blob */` |
|       7 | 3280 | `		ph7_result_string(pCtx,` |
|       4 | 3281 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3282 | `	}` |
|       5 | 3283 | `	return PH7_OK;` |
|       3 | 3284 |  |
|       - | 3285 | `/*` |
|       - | 3286 | ` * array each(array $input)` |
|       - | 3287 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3288 | ` * Parameter` |
|       - | 3289 | ` *  $input` |
|       - | 3290 | ` *    The input array.` |
|       - | 3291 | ` * Return` |
|       - | 3292 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3293 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3294 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3295 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3296 | ` *  each() returns FALSE.` |
|       - | 3297 | ` */` |
|      22 | 3298 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3299 |  |
|       - | 3300 | `	ph7_hashmap_node *pCur;` |
|       - | 3301 | `	ph7_hashmap *pMap;` |
|       - | 3302 | `	ph7_value *pArray;` |
|       - | 3303 | `	ph7_value *pVal;` |
|       - | 3304 | `	ph7_value sKey;` |
|      23 | 3305 | `	if( nArg < 1 ){` |
|       - | 3306 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3307 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3308 | `		return PH7_OK;` |
|       - | 3309 | `	}` |
|       - | 3310 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3311 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3312 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3313 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3314 | `		return PH7_OK;` |
|       - | 3315 | `	}` |
|       - | 3316 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3317 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3318 | `	if( pMap->pCur == 0 ){` |
|       - | 3319 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3320 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3321 | `		return PH7_OK;` |
|       - | 3322 | `	}` |
|      15 | 3323 | `	pCur = pMap->pCur;` |
|       - | 3324 | `	/* Create a new array */` |
|      15 | 3325 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3326 | `	if( pArray == 0 ){` |
|     ! 0 | 3327 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3328 | `		return PH7_OK;` |
|       - | 3329 | `	}` |
|      15 | 3330 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3331 | `	/* Insert the current value */` |
|      15 | 3332 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3333 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3334 | `	/* Make the key */` |
|      15 | 3335 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3336 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3337 | `	}else{` |
|       9 | 3338 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3339 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3340 | `	}` |
|       - | 3341 | `	/* Insert the current key */` |
|      15 | 3342 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3343 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3344 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3345 | `	/* Advance the cursor */` |
|      15 | 3346 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3347 | `	/* Return the current entry */` |
|      15 | 3348 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3349 | `	return PH7_OK;` |
|      12 | 3350 |  |
|       - | 3351 | `/*` |
|       - | 3352 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3353 | ` *  Create an array containing a range of elements` |
|       - | 3354 | ` * Parameter` |
|       - | 3355 | ` *  start` |
|       - | 3356 | ` *   First value of the sequence.` |
|       - | 3357 | ` *  limit` |
|       - | 3358 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3359 | ` *  step` |
|       - | 3360 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3361 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3362 | ` * Return` |
|       - | 3363 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3364 | ` * NOTE:` |
|       - | 3365 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3366 | ` */` |
|       2 | 3367 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3368 |  |
|       - | 3369 | `	ph7_value *pValue,*pArray;` |
|       - | 3370 | `	sxi64 iOfft,iLimit;` |
|       3 | 3371 | `	int iStep = 1;` |
|       - | 3372 |  |
|       3 | 3373 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3374 | `	if( nArg > 0 ){` |
|       - | 3375 | `		/* Extract the offset */` |
|       3 | 3376 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3377 | `		if( nArg > 1 ){` |
|       - | 3378 | `			/* Extract the limit */` |
|       3 | 3379 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3380 | `			if( nArg > 2 ){` |
|       - | 3381 | `				/* Extract the increment */` |
|       3 | 3382 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3383 | `				if( iStep < 1 ){` |
|       - | 3384 | `					/* Only positive number are allowed */` |
|       3 | 3385 | `					iStep = 1;` |
|       1 | 3386 | `				}` |
|       1 | 3387 | `			}` |
|       1 | 3388 | `		}` |
|       1 | 3389 | `	}` |
|       - | 3390 | `	/* Element container */` |
|       3 | 3391 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3392 | `	/* Create the new array */` |
|       3 | 3393 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3394 | `	if( pArray == 0 ){` |
|     ! 0 | 3395 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3396 | `	}` |
|       - | 3397 | `	/* Start filling */` |
|       3 | 3398 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3399 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3400 | `		/* Perform the insertion */` |
|     ! 0 | 3401 | `		if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       - | 3402 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3403 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3404 | `		}` |
|       - | 3405 | `		/* Increment */` |
|     ! 0 | 3406 | `		iOfft += iStep;` |
|     ! 0 | 3407 | `	}` |
|       - | 3408 | `	/* Return the new array */` |
|       3 | 3409 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3410 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3411 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3412 | `	 */` |
|       3 | 3413 | `	return PH7_OK;` |
|       2 | 3414 |  |
|       - | 3415 | `/*` |
|       - | 3416 | ` * array array_values(array $array)` |
|       - | 3417 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3418 | ` * Parameters` |
|       - | 3419 | ` *  $array` |
|       - | 3420 | ` *   The input array.` |
|       - | 3421 | ` * Return` |
|       - | 3422 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3423 | ` */` |
|      36 | 3424 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3425 |  |
|       - | 3426 | `	ph7_hashmap_node *pNode;` |
|       - | 3427 | `	ph7_hashmap *pMap;` |
|       - | 3428 | `	ph7_value *pArray;` |
|       - | 3429 | `	ph7_value *pObj;` |
|       - | 3430 | `	sxu32 n;` |
|      40 | 3431 | `	if( nArg != 1 ){` |
|       - | 3432 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       8 | 3433 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3434 | `			"ArgumentCountError",` |
|       - | 3435 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3436 | `			nArg` |
|       - | 3437 | `			);` |
|       - | 3438 | `	}` |
|       - | 3439 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 3440 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3441 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3442 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3443 | `			"TypeError",` |
|       - | 3444 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3445 | `			ph7_type_name(apArg[0])` |
|       - | 3446 | `			);` |
|       - | 3447 | `	}` |
|       - | 3448 | `	/* Point to the internal representation that describe the input hashmap */` |
|      32 | 3449 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3450 | `	/* Create a new array */` |
|      32 | 3451 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 3452 | `	if( pArray == 0 ){` |
|     ! 0 | 3453 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3454 | `		return PH7_OK;` |
|       - | 3455 | `	}` |
|       - | 3456 | `	/* Perform the requested operation */` |
|      32 | 3457 | `	pNode = pMap->pFirst;` |
|     104 | 3458 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      74 | 3459 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      74 | 3460 | `		if( pObj ){` |
|       - | 3461 | `			/* perform the insertion */` |
|      74 | 3462 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      36 | 3463 | `		}` |
|       - | 3464 | `		/* Point to the next entry */` |
|      74 | 3465 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      38 | 3466 | `	}` |
|       - | 3467 | `	/* return the new array */` |
|      32 | 3468 | `	ph7_result_value(pCtx,pArray);` |
|      32 | 3469 | `	return PH7_OK;` |
|      22 | 3470 |  |
|       - | 3471 | `/*` |
|       - | 3472 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3473 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3474 | ` * Parameters` |
|       - | 3475 | ` *  $input` |
|       - | 3476 | ` *   An array containing keys to return.` |
|       - | 3477 | ` * $search_value` |
|       - | 3478 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3479 | ` * $strict` |
|       - | 3480 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3481 | ` * Return` |
|       - | 3482 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3483 | ` */` |
|     132 | 3484 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3485 |  |
|       - | 3486 | `	ph7_hashmap_node *pNode;` |
|       - | 3487 | `	ph7_hashmap *pMap;` |
|       - | 3488 | `	ph7_value *pArray;` |
|       - | 3489 | `	ph7_value sObj;` |
|       - | 3490 | `	ph7_value sVal;` |
|       - | 3491 | `	SyString sKey;` |
|       - | 3492 | `	int bStrict;` |
|       - | 3493 | `	sxi32 rc;` |
|       - | 3494 | `	sxu32 n;` |
|     136 | 3495 | `	if( nArg < 1 ){` |
|       - | 3496 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3497 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3498 | `			"ArgumentCountError",` |
|       - | 3499 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3500 | `			);` |
|       - | 3501 | `	}` |
|       - | 3502 | `	/* Make sure we are dealing with a valid hashmap */` |
|     133 | 3503 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3504 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3505 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3506 | `			"TypeError",` |
|       - | 3507 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3508 | `			ph7_type_name(apArg[0])` |
|       - | 3509 | `			);` |
|       - | 3510 | `	}` |
|       - | 3511 | `	/* Point to the internal representation of the input hashmap */` |
|     131 | 3512 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3513 | `	/* Create a new array */` |
|     131 | 3514 | `	pArray = ph7_context_new_array(pCtx);` |
|     131 | 3515 | `	if( pArray == 0 ){` |
|     ! 0 | 3516 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3517 | `		return PH7_OK;` |
|       - | 3518 | `	}` |
|     131 | 3519 | `	bStrict = FALSE;` |
|     131 | 3520 | `	if( nArg > 2 ){` |
|       - | 3521 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3522 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3523 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3524 | `				"TypeError",` |
|       - | 3525 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3526 | `				ph7_type_name(apArg[2])` |
|       - | 3527 | `				);` |
|       - | 3528 | `		}` |
|       5 | 3529 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3530 | `	}` |
|       - | 3531 | `	/* Perform the requested operation */` |
|     128 | 3532 | `	pNode = pMap->pFirst;` |
|     128 | 3533 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     594 | 3534 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     468 | 3535 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     121 | 3536 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      61 | 3537 | `		}else{` |
|     348 | 3538 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     348 | 3539 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3540 | `		}` |
|     468 | 3541 | `		rc = 0;` |
|     468 | 3542 | `		if( nArg > 1 ){` |
|      31 | 3543 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3544 | `			if( pValue ){` |
|      31 | 3545 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3546 | `				/* Filter key */` |
|      31 | 3547 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3548 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3549 | `			}` |
|      15 | 3550 | `		}` |
|     468 | 3551 | `		if( rc == 0 ){` |
|       - | 3552 | `			/* Perform the insertion */` |
|     450 | 3553 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     224 | 3554 | `		}` |
|     468 | 3555 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3556 | `		/* Point to the next entry */` |
|     468 | 3557 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     235 | 3558 | `	}` |
|       - | 3559 | `	/* return the new array */` |
|     128 | 3560 | `	ph7_result_value(pCtx,pArray);` |
|     128 | 3561 | `	return PH7_OK;` |
|      70 | 3562 |  |
|       - | 3563 | `/*` |
|       - | 3564 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3565 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3566 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3567 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3568 | ` * Parameters` |
|       - | 3569 | ` *  $arr1` |
|       - | 3570 | ` *   First array` |
|       - | 3571 | ` *  $arr2` |
|       - | 3572 | ` *   Second array` |
|       - | 3573 | ` * Return` |
|       - | 3574 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3575 | ` * Note` |
|       - | 3576 | ` *  This function is a symisc eXtension.` |
|       - | 3577 | ` */` |
|       4 | 3578 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3579 |  |
|       - | 3580 | `	ph7_hashmap *p1,*p2;` |
|       - | 3581 | `	int rc;` |
|       5 | 3582 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3583 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3584 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3585 | `		return PH7_OK;` |
|       - | 3586 | `	}` |
|       - | 3587 | `	/* Point to the hashmaps */` |
|       5 | 3588 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3589 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3590 | `	rc = (p1 == p2);` |
|       - | 3591 | `	/* Same instance? */` |
|       5 | 3592 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3593 | `	return PH7_OK;` |
|       3 | 3594 |  |
|       - | 3595 | `/*` |
|       - | 3596 | ` * array array_merge(array ...$arrays)` |
|       - | 3597 | ` *  Merge one or more arrays.` |
|       - | 3598 | ` * Parameters` |
|       - | 3599 | ` *  ...$arrays` |
|       - | 3600 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3601 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3602 | ` * Return` |
|       - | 3603 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3604 | ` *  with no arguments.` |
|       - | 3605 | ` */` |
|    1022 | 3606 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3607 |  |
|       - | 3608 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3609 | `	ph7_value *pArray;` |
|       - | 3610 | `	int i;` |
|       - | 3611 | `	/* Create a new array */` |
|    1027 | 3612 | `	pArray = ph7_context_new_array(pCtx);` |
|    1027 | 3613 | `	if( pArray == 0 ){` |
|     ! 0 | 3614 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3615 | `		return PH7_OK;` |
|       - | 3616 | `	}` |
|       - | 3617 | `	/* Point to the internal representation of the hashmap */` |
|    1027 | 3618 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3619 | `	/* Start merging */` |
|    3061 | 3620 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3621 | `		/* Make sure we are dealing with a valid hashmap */` |
|    2043 | 3622 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3623 | `			/* Type mismatch -> TypeError */` |
|       8 | 3624 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3625 | `				"TypeError",` |
|       - | 3626 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3627 | `				i + 1,` |
|       4 | 3628 | `				ph7_type_name(apArg[i])` |
|       - | 3629 | `				);` |
|     ! 0 | 3630 | `		}else{` |
|    2039 | 3631 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3632 | `			/* Merge the two hashmaps */` |
|    2039 | 3633 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3634 | `		}` |
|    1022 | 3635 | `	}` |
|       - | 3636 | `	/* Return the freshly created array */` |
|    1023 | 3637 | `	ph7_result_value(pCtx,pArray);` |
|    1023 | 3638 | `	return PH7_OK;` |
|     516 | 3639 |  |
|       - | 3640 | `/*` |
|       - | 3641 | ` * array array_copy(array $source)` |
|       - | 3642 | ` *  Make a blind copy of the target array.` |
|       - | 3643 | ` * Parameters` |
|       - | 3644 | ` *  $source` |
|       - | 3645 | ` *   Target array` |
|       - | 3646 | ` * Return` |
|       - | 3647 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3648 | ` * Note` |
|       - | 3649 | ` *  This function is a symisc eXtension.` |
|       - | 3650 | ` */` |
|      16 | 3651 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3652 |  |
|       - | 3653 | `	ph7_hashmap *pMap;` |
|       - | 3654 | `	ph7_value *pArray;` |
|      17 | 3655 | `	if( nArg < 1 ){` |
|       - | 3656 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3657 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3658 | `		return PH7_OK;` |
|       - | 3659 | `	}` |
|       - | 3660 | `	/* Create a new array */` |
|      17 | 3661 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3662 | `	if( pArray == 0 ){` |
|     ! 0 | 3663 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3664 | `		return PH7_OK;` |
|       - | 3665 | `	}` |
|       - | 3666 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3667 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3668 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3669 | `		/* Point to the internal representation of the source */` |
|      17 | 3670 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3671 | `		/* Perform the copy */` |
|      17 | 3672 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3673 | `	}else{` |
|       - | 3674 | `		/* Simple insertion */` |
|     ! 0 | 3675 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3676 | `	}` |
|       - | 3677 | `	/* Return the duplicated array */` |
|      17 | 3678 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3679 | `	return PH7_OK;` |
|       9 | 3680 |  |
|       - | 3681 | `/*` |
|       - | 3682 | ` * bool array_erase(array $source)` |
|       - | 3683 | ` *  Remove all elements from a given array.` |
|       - | 3684 | ` * Parameters` |
|       - | 3685 | ` *  $source` |
|       - | 3686 | ` *   Target array` |
|       - | 3687 | ` * Return` |
|       - | 3688 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3689 | ` * Note` |
|       - | 3690 | ` *  This function is a symisc eXtension.` |
|       - | 3691 | ` */` |
|      16 | 3692 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3693 |  |
|       - | 3694 | `	ph7_hashmap *pMap;` |
|      17 | 3695 | `	if( nArg < 1 ){` |
|       - | 3696 | `		/* Missing arguments */` |
|     ! 0 | 3697 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3698 | `		return PH7_OK;` |
|       - | 3699 | `	}` |
|       - | 3700 | `	/* Point to the target hashmap */` |
|      17 | 3701 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3702 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3703 | `	/* Erase */` |
|      17 | 3704 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3705 | `	return PH7_OK;` |
|       9 | 3706 |  |
|       - | 3707 | `/*` |
|       - | 3708 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3709 | ` *  Extract a slice of the array.` |
|       - | 3710 | ` * Parameters` |
|       - | 3711 | ` *  $array` |
|       - | 3712 | ` *    The input array.` |
|       - | 3713 | ` * $offset` |
|       - | 3714 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3715 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3716 | ` * $length (optional, nullable)` |
|       - | 3717 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3718 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3719 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3720 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3721 | ` * $preserve_keys (optional)` |
|       - | 3722 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3723 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3724 | ` * Return` |
|       - | 3725 | ` *   The new slice.` |
|       - | 3726 | ` */` |
|      50 | 3727 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3728 |  |
|       - | 3729 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3730 | `	ph7_hashmap_node *pCur;` |
|       - | 3731 | `	ph7_value *pArray;` |
|       - | 3732 | `	int iLength,iOfft;` |
|       - | 3733 | `	int bPreserve;` |
|       - | 3734 | `	sxi32 rc;` |
|      55 | 3735 | `	if( nArg < 2 ){` |
|       8 | 3736 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3737 | `			"ArgumentCountError",` |
|       - | 3738 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3739 | `			nArg` |
|       - | 3740 | `			);` |
|       - | 3741 | `	}` |
|      51 | 3742 | `	if( nArg > 4 ){` |
|       4 | 3743 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3744 | `			"ArgumentCountError",` |
|       - | 3745 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3746 | `			nArg` |
|       - | 3747 | `			);` |
|       - | 3748 | `	}` |
|      49 | 3749 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3750 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3751 | `			"TypeError",` |
|       - | 3752 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3753 | `			ph7_type_name(apArg[0])` |
|       - | 3754 | `			);` |
|       - | 3755 | `	}` |
|       - | 3756 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      62 | 3757 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      65 | 3758 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3759 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3760 | `			"TypeError",` |
|       - | 3761 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3762 | `			ph7_type_name(apArg[1])` |
|       - | 3763 | `			);` |
|       - | 3764 | `	}` |
|       - | 3765 | `	/* Validate $length type if provided: nullable int */` |
|      45 | 3766 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      26 | 3767 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3768 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3769 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3770 | `				"TypeError",` |
|       - | 3771 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3772 | `				ph7_type_name(apArg[2])` |
|       - | 3773 | `				);` |
|       - | 3774 | `		}` |
|       8 | 3775 | `	}` |
|       - | 3776 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      43 | 3777 | `	if( nArg > 3 ){` |
|      10 | 3778 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3779 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3780 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3781 | `				"TypeError",` |
|       - | 3782 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3783 | `				ph7_type_name(apArg[3])` |
|       - | 3784 | `				);` |
|       - | 3785 | `		}` |
|       2 | 3786 | `	}` |
|       - | 3787 | `	/* Point the internal representation of the target array */` |
|      41 | 3788 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      41 | 3789 | `	bPreserve = FALSE;` |
|       - | 3790 | `	/* Get the offset */` |
|      41 | 3791 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      41 | 3792 | `	if( iOfft < 0 ){` |
|       5 | 3793 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3794 | `		if( iOfft < 0 ){` |
|       3 | 3795 | `			iOfft = 0;` |
|       1 | 3796 | `		}` |
|       2 | 3797 | `	}` |
|      41 | 3798 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3799 | `		/* Offset past end of array, return empty array */` |
|       5 | 3800 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3801 | `		if( pArray == 0 ){` |
|     ! 0 | 3802 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3803 | `			return PH7_OK;` |
|       - | 3804 | `		}` |
|       5 | 3805 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3806 | `		return PH7_OK;` |
|       - | 3807 | `	}` |
|       - | 3808 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      37 | 3809 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      37 | 3810 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3811 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3812 | `		if( iLength < 0 ){` |
|       5 | 3813 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3814 | `		}` |
|      15 | 3815 | `		if( iLength < 0 ){` |
|       3 | 3816 | `			iLength = 0;` |
|       1 | 3817 | `		}` |
|      15 | 3818 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3819 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3820 | `		}` |
|       7 | 3821 | `	}` |
|      37 | 3822 | `	if( nArg > 3 ){` |
|       5 | 3823 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3824 | `	}` |
|       - | 3825 | `	/* Create a new array */` |
|      37 | 3826 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 3827 | `	if( pArray == 0 ){` |
|     ! 0 | 3828 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3829 | `		return PH7_OK;` |
|       - | 3830 | `	}` |
|      37 | 3831 | `	if( iLength < 1 ){` |
|       - | 3832 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3833 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3834 | `		return PH7_OK;` |
|       - | 3835 | `	}` |
|       - | 3836 | `	/* Point to the desired entry */` |
|      33 | 3837 | `	pCur = pSrc->pFirst;` |
|      28 | 3838 | `	for(;;){` |
|      61 | 3839 | `		if( iOfft < 1 ){` |
|      33 | 3840 | `			break;` |
|       - | 3841 | `		}` |
|       - | 3842 | `		/* Point to the next entry */` |
|      33 | 3843 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      33 | 3844 | `		iOfft--;` |
|       5 | 3845 | `	}` |
|       - | 3846 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 3847 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      51 | 3848 | `	for(;;){` |
|     107 | 3849 | `		if( iLength < 1 ){` |
|      33 | 3850 | `			break;` |
|       - | 3851 | `		}` |
|       - | 3852 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3853 | `		{` |
|      79 | 3854 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      79 | 3855 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3856 | `		}` |
|      79 | 3857 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3858 | `			break;` |
|       - | 3859 | `		}` |
|       - | 3860 | `		/* Point to the next entry */` |
|      79 | 3861 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      79 | 3862 | `		iLength--;` |
|       5 | 3863 | `	}` |
|       - | 3864 | `	/* Return the freshly created array */` |
|      33 | 3865 | `	ph7_result_value(pCtx,pArray);` |
|      33 | 3866 | `	return PH7_OK;` |
|      30 | 3867 |  |
|       - | 3868 | `/*` |
|       - | 3869 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3870 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3871 | ` * beginning (becomes the new pFirst).` |
|       - | 3872 | ` */` |
|      30 | 3873 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3874 |  |
|       - | 3875 | `	ph7_hashmap_node *pNode;` |
|       - | 3876 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3877 | `	pNode = pMap->pLast;` |
|      31 | 3878 | `	if( pNode == 0 ){` |
|     ! 0 | 3879 | `		return;` |
|       - | 3880 | `	}` |
|      31 | 3881 | `	if( pNode->pNext == 0 ){` |
|       - | 3882 | `		/* Only node in the list, nothing to move */` |
|       5 | 3883 | `		return;` |
|       - | 3884 | `	}` |
|      27 | 3885 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3886 | `		/* Already in the correct position */` |
|       9 | 3887 | `		return;` |
|       - | 3888 | `	}` |
|       - | 3889 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3890 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3891 | `	pMap->pLast->pPrev = 0;` |
|       - | 3892 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3893 | `	if( pAfter == 0 ){` |
|       - | 3894 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3895 | `		pNode->pNext = 0;` |
|       3 | 3896 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3897 | `		if( pMap->pFirst ){` |
|       3 | 3898 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3899 | `		}` |
|       3 | 3900 | `		pMap->pFirst = pNode;` |
|       2 | 3901 | `	}else{` |
|      17 | 3902 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3903 | `		pNode->pPrev = pOldNext;` |
|      17 | 3904 | `		pNode->pNext = pAfter;` |
|      17 | 3905 | `		pAfter->pPrev = pNode;` |
|      17 | 3906 | `		if( pOldNext ){` |
|      17 | 3907 | `			pOldNext->pNext = pNode;` |
|       9 | 3908 | `		}else{` |
|     ! 0 | 3909 | `			pMap->pLast = pNode;` |
|       - | 3910 | `		}` |
|       - | 3911 | `	}` |
|      16 | 3912 |  |
|       - | 3913 | `/*` |
|       - | 3914 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3915 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3916 | ` * Parameters` |
|       - | 3917 | ` *  $array` |
|       - | 3918 | ` *    The input array.` |
|       - | 3919 | ` *  $offset` |
|       - | 3920 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3921 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3922 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3923 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3924 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3925 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3926 | ` *  $length (optional)` |
|       - | 3927 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3928 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3929 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3930 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3931 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3932 | ` *  $replacement (optional)` |
|       - | 3933 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3934 | ` *    with elements from this array.` |
|       - | 3935 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3936 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3937 | ` *    offset.` |
|       - | 3938 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3939 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3940 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3941 | ` * Return` |
|       - | 3942 | ` *   A new array consisting of the extracted elements.` |
|       - | 3943 | ` */` |
|      54 | 3944 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3945 |  |
|       - | 3946 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3947 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3948 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3949 | `	int iLength,iOfft,i;` |
|       - | 3950 | `	sxi32 rc;` |
|      58 | 3951 | `	if( nArg < 2 ){` |
|       8 | 3952 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3953 | `			"ArgumentCountError",` |
|       - | 3954 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3955 | `			nArg` |
|       - | 3956 | `			);` |
|       - | 3957 | `	}` |
|      52 | 3958 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3959 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3960 | `			"TypeError",` |
|       - | 3961 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3962 | `			ph7_type_name(apArg[0])` |
|       - | 3963 | `			);` |
|       - | 3964 | `	}` |
|       - | 3965 | `	/* Point to the internal representation of the target array */` |
|      49 | 3966 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3967 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3968 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3969 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3970 | `	if( iOfft < 0 ){` |
|       7 | 3971 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3972 | `		if( iOfft < 0 ){` |
|       3 | 3973 | `			iOfft = 0;` |
|       2 | 3974 | `		}` |
|      46 | 3975 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3976 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3977 | `	}` |
|       - | 3978 | `	/* Get the length and clamp to valid range.` |
|       - | 3979 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3980 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3981 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3982 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3983 | `		if( iLength < 0 ){` |
|       7 | 3984 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3985 | `			if( iLength < 0 ){` |
|       3 | 3986 | `				iLength = 0;` |
|       1 | 3987 | `			}` |
|       3 | 3988 | `		}` |
|      31 | 3989 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3990 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3991 | `		}` |
|      15 | 3992 | `	}` |
|       - | 3993 | `	/* Create the result array for removed elements */` |
|      49 | 3994 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3995 | `	if( pArray == 0 ){` |
|     ! 0 | 3996 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3997 | `		return PH7_OK;` |
|       - | 3998 | `	}` |
|       - | 3999 | `	/* Get replacement array if provided */` |
|      49 | 4000 | `	pRep = 0;` |
|      49 | 4001 | `	if( nArg > 3 ){` |
|      21 | 4002 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 4003 | `			/* Perform an array cast */` |
|       3 | 4004 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 4005 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 4006 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 4007 | `			}` |
|       2 | 4008 | `		}else{` |
|      19 | 4009 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 4010 | `		}` |
|      21 | 4011 | `		if( pRep ){` |
|       - | 4012 | `			/* Reset the loop cursor */` |
|      21 | 4013 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 4014 | `		}` |
|      10 | 4015 | `	}` |
|       - | 4016 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 4017 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 4018 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 4019 | `		return PH7_OK;` |
|       - | 4020 | `	}` |
|       - | 4021 | `	/* Navigate to the offset position */` |
|      41 | 4022 | `	pCur = pSrc->pFirst;` |
|      85 | 4023 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 4024 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 4025 | `	}` |
|       - | 4026 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 4027 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 4028 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 4029 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 4030 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 4031 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 4032 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 4033 | `		pPrev = pCur->pPrev;` |
|      71 | 4034 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 4035 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 4036 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 4037 | `			break;` |
|       - | 4038 | `		}` |
|      71 | 4039 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 4040 | `	}` |
|       - | 4041 | `	/* Insert replacement elements at the correct position */` |
|      41 | 4042 | `	if( pRep ){` |
|       - | 4043 | `		ph7_value sSafeVal;` |
|      61 | 4044 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 4045 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 4046 | `			if( pRvalue ){` |
|       - | 4047 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 4048 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 4049 | `				 * since it points into that same pool. */` |
|      31 | 4050 | `				sSafeVal = *pRvalue;` |
|      31 | 4051 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 4052 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 4053 | `					pNewNode = pSrc->pLast;` |
|      31 | 4054 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 4055 | `					pInsertAfter = pNewNode;` |
|      15 | 4056 | `				}` |
|      15 | 4057 | `			}` |
|       1 | 4058 | `		}` |
|      10 | 4059 | `	}` |
|       - | 4060 | `	/* Return the freshly created array */` |
|      41 | 4061 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 4062 | `	return PH7_OK;` |
|      31 | 4063 |  |
|       - | 4064 | `/*` |
|       - | 4065 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 4066 | ` *  Checks if a value exists in an array.` |
|       - | 4067 | ` * Parameters` |
|       - | 4068 | ` *  $needle` |
|       - | 4069 | ` *   The searched value.` |
|       - | 4070 | ` *   Note:` |
|       - | 4071 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 4072 | ` * $haystack` |
|       - | 4073 | ` *  The target array.` |
|       - | 4074 | ` * $strict` |
|       - | 4075 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 4076 | ` *  will also check the types of the needle in the haystack.` |
|       - | 4077 | ` */` |
|   31828 | 4078 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4079 |  |
|       - | 4080 | `	ph7_value *pNeedle;` |
|       - | 4081 | `	int bStrict;` |
|       - | 4082 | `	int rc;` |
|   31833 | 4083 | `	if( nArg < 2 ){` |
|       - | 4084 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 4085 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4086 | `		return PH7_OK;` |
|       - | 4087 | `	}` |
|   31833 | 4088 | `	pNeedle = apArg[0];` |
|   31833 | 4089 | `	bStrict = 0;` |
|   31833 | 4090 | `	if( nArg > 2 ){` |
|      17 | 4091 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       8 | 4092 | `	}` |
|   31833 | 4093 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4094 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 4095 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 4096 | `		/* Set the comparison result */` |
|     ! 0 | 4097 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 4098 | `		return PH7_OK;` |
|       - | 4099 | `	}` |
|       - | 4100 | `	/* Perform the lookup */` |
|   31833 | 4101 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 4102 | `	/* Lookup result */` |
|   31833 | 4103 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   31833 | 4104 | `	return PH7_OK;` |
|   15919 | 4105 |  |
|       - | 4106 | `/*` |
|       - | 4107 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 4108 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 4109 | ` * Parameters` |
|       - | 4110 | ` * $needle` |
|       - | 4111 | ` *   The searched value.` |
|       - | 4112 | ` * $haystack` |
|       - | 4113 | ` *   The array.` |
|       - | 4114 | ` * $strict` |
|       - | 4115 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 4116 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 4117 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 4118 | ` * Return` |
|       - | 4119 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 4120 | ` */` |
|      28 | 4121 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4122 |  |
|       - | 4123 | `	ph7_hashmap_node *pEntry;` |
|       - | 4124 | `	ph7_value *pVal,sNeedle;` |
|       - | 4125 | `	ph7_hashmap *pMap;` |
|       - | 4126 | `	ph7_value sVal;` |
|       - | 4127 | `	int bStrict;` |
|       - | 4128 | `	sxu32 n;` |
|       - | 4129 | `	int rc;` |
|      33 | 4130 | `	if( nArg < 2 ){` |
|       - | 4131 | `		/* Missing argument,throw ArgumentCountError */` |
|       8 | 4132 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4133 | `			"ArgumentCountError",` |
|       - | 4134 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4135 | `			nArg` |
|       - | 4136 | `			);` |
|       - | 4137 | `	}` |
|      27 | 4138 | `	bStrict = FALSE;` |
|      27 | 4139 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4140 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4141 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4142 | `			"TypeError",` |
|       - | 4143 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4144 | `			ph7_type_name(apArg[1])` |
|       - | 4145 | `			);` |
|       - | 4146 | `	}` |
|      24 | 4147 | `	if( nArg > 2 ){` |
|       - | 4148 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4149 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4150 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4151 | `				"TypeError",` |
|       - | 4152 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4153 | `				ph7_type_name(apArg[2])` |
|       - | 4154 | `				);` |
|       - | 4155 | `		}` |
|       9 | 4156 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4157 | `	}` |
|       - | 4158 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4159 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4160 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4161 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4162 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4163 | `	pEntry = pMap->pFirst;` |
|      21 | 4164 | `	n = pMap->nEntry;` |
|      23 | 4165 | `	for(;;){` |
|      47 | 4166 | `		if( !n ){` |
|       9 | 4167 | `			break;` |
|       - | 4168 | `		}` |
|       - | 4169 | `		/* Extract node value */` |
|      39 | 4170 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4171 | `		if( pVal ){` |
|       - | 4172 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4173 | `			 * can change their type.` |
|       - | 4174 | `			 */` |
|      39 | 4175 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4176 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4177 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4178 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4179 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4180 | `			if( rc == 0 ){` |
|       - | 4181 | `				/* Match found,return key */` |
|      13 | 4182 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4183 | `					/* INT key */` |
|       7 | 4184 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4185 | `				}else{` |
|       7 | 4186 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4187 | `					/* Blob key */` |
|       7 | 4188 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4189 | `				}` |
|      13 | 4190 | `				return PH7_OK;` |
|       - | 4191 | `			}` |
|      13 | 4192 | `		}` |
|       - | 4193 | `		/* Point to the next entry */` |
|      27 | 4194 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4195 | `		n--;` |
|       1 | 4196 | `	}` |
|       - | 4197 | `	/* No such value,return FALSE */` |
|       9 | 4198 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4199 | `	return PH7_OK;` |
|      19 | 4200 |  |
|       - | 4201 | `/*` |
|       - | 4202 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4203 | ` *  Computes the difference of arrays.` |
|       - | 4204 | ` * Parameters` |
|       - | 4205 | ` *  $array1` |
|       - | 4206 | ` *    The array to compare from` |
|       - | 4207 | ` *  $array2` |
|       - | 4208 | ` *    An array to compare against` |
|       - | 4209 | ` *  $...` |
|       - | 4210 | ` *   More arrays to compare against` |
|       - | 4211 | ` * Return` |
|       - | 4212 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4213 | ` *  are not present in any of the other arrays.` |
|       - | 4214 | ` */` |
|      22 | 4215 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4216 |  |
|       - | 4217 | `	ph7_hashmap_node *pEntry;` |
|       - | 4218 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4219 | `	ph7_value *pArray;` |
|       - | 4220 | `	ph7_value *pVal;` |
|       - | 4221 | `	sxi32 rc;` |
|       - | 4222 | `	sxu32 n;` |
|       - | 4223 | `	int i;` |
|       - | 4224 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4225 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4226 | `	 * debugging difficult. */` |
|      26 | 4227 | `	if( nArg < 1 ){` |
|       4 | 4228 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4229 | `			"ArgumentCountError",` |
|       - | 4230 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4231 | `			nArg` |
|       - | 4232 | `			);` |
|       - | 4233 | `	}` |
|      23 | 4234 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4235 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4236 | `			"TypeError",` |
|       - | 4237 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4238 | `			ph7_type_name(apArg[0])` |
|       - | 4239 | `			);` |
|       - | 4240 | `	}` |
|      36 | 4241 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4242 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4243 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4244 | `				"TypeError",` |
|       - | 4245 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4246 | `				i + 1,` |
|       2 | 4247 | `				ph7_type_name(apArg[i])` |
|       - | 4248 | `				);` |
|       - | 4249 | `		}` |
|       9 | 4250 | `	}` |
|      17 | 4251 | `	if( nArg == 1 ){` |
|       - | 4252 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4253 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4254 | `		return PH7_OK;` |
|       - | 4255 | `	}` |
|       - | 4256 | `	/* Create a new array */` |
|      15 | 4257 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4258 | `	if( pArray == 0 ){` |
|     ! 0 | 4259 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4260 | `		return PH7_OK;` |
|       - | 4261 | `	}` |
|       - | 4262 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4263 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4264 | `	/* Perform the diff */` |
|      15 | 4265 | `	pEntry = pSrc->pFirst;` |
|      15 | 4266 | `	n = pSrc->nEntry;` |
|      27 | 4267 | `	for(;;){` |
|      55 | 4268 | `		if( n < 1 ){` |
|      15 | 4269 | `			break;` |
|       - | 4270 | `		}` |
|       - | 4271 | `		/* Extract the node value */` |
|      41 | 4272 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4273 | `		if( pVal ){` |
|      69 | 4274 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4275 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4276 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4277 | `				/* Perform the lookup */` |
|      45 | 4278 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4279 | `				if( rc == SXRET_OK ){` |
|       - | 4280 | `					/* Value exist */` |
|      17 | 4281 | `					break;` |
|       - | 4282 | `				}` |
|      15 | 4283 | `			}` |
|      41 | 4284 | `			if( i >= nArg ){` |
|       - | 4285 | `				/* Perform the insertion */` |
|      25 | 4286 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4287 | `			}` |
|      20 | 4288 | `		}` |
|       - | 4289 | `		/* Point to the next entry */` |
|      41 | 4290 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4291 | `		n--;` |
|       1 | 4292 | `	}` |
|       - | 4293 | `	/* Return the freshly created array */` |
|      15 | 4294 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4295 | `	return PH7_OK;` |
|      15 | 4296 |  |
|       - | 4297 | `/*` |
|       - | 4298 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4299 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4300 | ` * Parameters` |
|       - | 4301 | ` *  $array1` |
|       - | 4302 | ` *    The array to compare from` |
|       - | 4303 | ` *  $array2` |
|       - | 4304 | ` *    An array to compare against` |
|       - | 4305 | ` *  $...` |
|       - | 4306 | ` *   More arrays to compare against.` |
|       - | 4307 | ` * $callback` |
|       - | 4308 | ` *  The callback comparison function.` |
|       - | 4309 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4310 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4311 | ` *  than the second.` |
|       - | 4312 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4313 | ` * Return` |
|       - | 4314 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4315 | ` *  are not present in any of the other arrays.` |
|       - | 4316 | ` */` |
|      22 | 4317 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4318 |  |
|       - | 4319 | `	ph7_hashmap_node *pEntry;` |
|       - | 4320 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4321 | `	ph7_value *pCallback;` |
|       - | 4322 | `	ph7_value *pArray;` |
|       - | 4323 | `	ph7_value *pVal;` |
|       - | 4324 | `	sxi32 rc;` |
|       - | 4325 | `	sxu32 n;` |
|       - | 4326 | `	int i;` |
|       - | 4327 |  |
|       - | 4328 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      27 | 4329 | `	if( nArg < 2 ){` |
|       4 | 4330 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4331 | `			"ArgumentCountError",` |
|       - | 4332 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4333 | `			nArg` |
|       - | 4334 | `			);` |
|       - | 4335 | `	}` |
|      25 | 4336 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4337 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4338 | `			"TypeError",` |
|       - | 4339 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4340 | `			ph7_type_name(apArg[0])` |
|       - | 4341 | `			);` |
|       - | 4342 | `	}` |
|       - | 4343 |  |
|      23 | 4344 | `	if( nArg == 2 ){` |
|       - | 4345 | `		/* Only the original array and the callback were provided. */` |
|       - | 4346 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4347 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4348 | `		 * validation order.` |
|       - | 4349 | `		 */` |
|       4 | 4350 | `	} else {` |
|       - | 4351 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      27 | 4352 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      19 | 4353 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      11 | 4354 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4355 | `					"TypeError",` |
|       - | 4356 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4357 | `					i + 1,` |
|       6 | 4358 | `					ph7_type_name(apArg[i])` |
|       - | 4359 | `					);` |
|       - | 4360 | `			}` |
|       7 | 4361 | `		}` |
|       - | 4362 | `	}` |
|       - | 4363 |  |
|       - | 4364 | `	/* Identify the callback (always expected as the last argument). */` |
|      16 | 4365 | `	pCallback = apArg[nArg - 1];` |
|       - | 4366 | `	/* Validate the callback to match PHP's error messages. */` |
|      16 | 4367 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       9 | 4368 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4369 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4370 | `				"TypeError",` |
|       - | 4371 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4372 | `				nArg` |
|       - | 4373 | `				);` |
|       - | 4374 | `		}` |
|       6 | 4375 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4376 | `			int len;` |
|       3 | 4377 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4378 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4379 | `				"TypeError",` |
|       - | 4380 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4381 | `				nArg,` |
|       1 | 4382 | `				zName` |
|       - | 4383 | `				);` |
|       - | 4384 | `		}` |
|       4 | 4385 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4386 | `			"TypeError",` |
|       - | 4387 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4388 | `			nArg` |
|       - | 4389 | `			);` |
|       - | 4390 | `	}` |
|       - | 4391 |  |
|       7 | 4392 | `	if( nArg == 2 ){` |
|       - | 4393 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4394 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4395 | `		return PH7_OK;` |
|       - | 4396 | `	}` |
|       - | 4397 |  |
|       - | 4398 | `	/* Create a new array */` |
|       5 | 4399 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4400 | `	if( pArray == 0 ){` |
|     ! 0 | 4401 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4402 | `		return PH7_OK;` |
|       - | 4403 | `	}` |
|       - | 4404 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 4405 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4406 | `	/* Perform the diff */` |
|       5 | 4407 | `	pEntry = pSrc->pFirst;` |
|       5 | 4408 | `	n = pSrc->nEntry;` |
|       5 | 4409 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 4410 | `	for(;;){` |
|      11 | 4411 | `		if( n < 1 ){` |
|       3 | 4412 | `			break;` |
|       - | 4413 | `		}` |
|       - | 4414 | `		/* Extract the node value */` |
|       9 | 4415 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4416 | `		if( pVal ){` |
|      15 | 4417 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4418 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4419 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4420 | `				/* Perform the lookup */` |
|       9 | 4421 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       9 | 4422 | `				if( rc == SXRET_OK ){` |
|       - | 4423 | `					/* Value exist */` |
|       3 | 4424 | `					break;` |
|       - | 4425 | `				}` |
|       4 | 4426 | `			}` |
|       9 | 4427 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 4428 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 4429 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 4430 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 4431 | `				return PH7_EXCEPTION;` |
|       - | 4432 | `			}` |
|       7 | 4433 | `			if( i >= (nArg - 1)){` |
|       - | 4434 | `				/* Perform the insertion */` |
|       5 | 4435 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4436 | `			}` |
|       3 | 4437 | `		}` |
|       - | 4438 | `		/* Point to the next entry */` |
|       7 | 4439 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4440 | `		n--;` |
|       1 | 4441 | `	}` |
|       - | 4442 | `	/* Return the freshly created array */` |
|       3 | 4443 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4444 | `	return PH7_OK;` |
|      16 | 4445 |  |
|       - | 4446 | `/*` |
|       - | 4447 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4448 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4449 | ` * Parameters` |
|       - | 4450 | ` *  $array1` |
|       - | 4451 | ` *    The array to compare from` |
|       - | 4452 | ` *  $array2` |
|       - | 4453 | ` *    An array to compare against` |
|       - | 4454 | ` *  $...` |
|       - | 4455 | ` *   More arrays to compare against` |
|       - | 4456 | ` * Return` |
|       - | 4457 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4458 | ` *  are not present in any of the other arrays.` |
|       - | 4459 | ` */` |
|      20 | 4460 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4461 |  |
|       - | 4462 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4463 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4464 | `	ph7_value *pArray;` |
|       - | 4465 | `	ph7_value *pVal;` |
|       - | 4466 | `	sxi32 rc;` |
|       - | 4467 | `	sxu32 n;` |
|       - | 4468 | `	int i;` |
|       - | 4469 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4470 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4471 | `	 * accompanying integration tests to pass. */` |
|      25 | 4472 | `	if( nArg < 1 ){` |
|       4 | 4473 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4474 | `			"ArgumentCountError",` |
|       - | 4475 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4476 | `			nArg` |
|       - | 4477 | `			);` |
|       - | 4478 | `	}` |
|      22 | 4479 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4480 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4481 | `			"TypeError",` |
|       - | 4482 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4483 | `			ph7_type_name(apArg[0])` |
|       - | 4484 | `			);` |
|       - | 4485 | `	}` |
|      33 | 4486 | `	for(i = 1 ; i < nArg ; i++){` |
|      21 | 4487 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 4488 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4489 | `				"TypeError",` |
|       - | 4490 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4491 | `				i + 1,` |
|       4 | 4492 | `				ph7_type_name(apArg[i])` |
|       - | 4493 | `				);` |
|       - | 4494 | `		}` |
|       9 | 4495 | `	}` |
|      13 | 4496 | `	if( nArg == 1 ){` |
|       - | 4497 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4498 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4499 | `		return PH7_OK;` |
|       - | 4500 | `	}` |
|       - | 4501 | `	/* Create a new array */` |
|      11 | 4502 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4503 | `	if( pArray == 0 ){` |
|     ! 0 | 4504 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4505 | `		return PH7_OK;` |
|       - | 4506 | `	}` |
|       - | 4507 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4508 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4509 | `	/* Perform the diff */` |
|      11 | 4510 | `	pEntry = pSrc->pFirst;` |
|      11 | 4511 | `	n = pSrc->nEntry;` |
|      11 | 4512 | `	pN1 = pN2 = 0;` |
|      29 | 4513 | `	for(;;){` |
|       - | 4514 | `		int keep;` |
|      35 | 4515 | `		if( n < 1 ){` |
|      11 | 4516 | `			break;` |
|       - | 4517 | `		}` |
|       - | 4518 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4519 | `		keep = 1;` |
|      41 | 4520 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4521 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4522 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4523 | `			/* Perform a key lookup first */` |
|      29 | 4524 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4525 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4526 | `			}else{` |
|      17 | 4527 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4528 | `			}` |
|      29 | 4529 | `			if( rc != SXRET_OK ){` |
|       - | 4530 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4531 | `				continue;` |
|       - | 4532 | `			}` |
|       - | 4533 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4534 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4535 | `			if( pVal ){` |
|       - | 4536 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4537 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4538 | `				if( pVal2 ){` |
|      15 | 4539 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4540 | `					if( cmp == 0 ){` |
|       - | 4541 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4542 | `						keep = 0;` |
|      13 | 4543 | `						break;` |
|       - | 4544 | `					}` |
|       1 | 4545 | `				}` |
|       1 | 4546 | `			}` |
|       2 | 4547 | `		}` |
|      25 | 4548 | `		if( keep ){` |
|       - | 4549 | `			/* Perform the insertion */` |
|      13 | 4550 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4551 | `		}` |
|       - | 4552 | `		/* Point to the next entry */` |
|      25 | 4553 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4554 | `		n--;` |
|       1 | 4555 | `	}` |
|       - | 4556 | `	/* Return the freshly created array */` |
|      11 | 4557 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4558 | `	return PH7_OK;` |
|      15 | 4559 |  |
|       - | 4560 | `/*` |
|       - | 4561 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4562 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4563 | ` *  by a user supplied callback function.` |
|       - | 4564 | ` * Parameters` |
|       - | 4565 | ` *  $array1` |
|       - | 4566 | ` *    The array to compare from` |
|       - | 4567 | ` *  $array2` |
|       - | 4568 | ` *    An array to compare against` |
|       - | 4569 | ` *  $...` |
|       - | 4570 | ` *   More arrays to compare against.` |
|       - | 4571 | ` *  $key_compare_func` |
|       - | 4572 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4573 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4574 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4575 | ` * Return` |
|       - | 4576 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4577 | ` *  are not present in any of the other arrays.` |
|       - | 4578 | ` */` |
|      24 | 4579 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4580 |  |
|       - | 4581 | `	ph7_hashmap_node *pEntry;` |
|       - | 4582 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4583 | `	ph7_value *pCallback;` |
|       - | 4584 | `	ph7_value *pArray;` |
|       - | 4585 | `	sxi32 rc;` |
|       - | 4586 | `	sxu32 n;` |
|       - | 4587 | `	int i;` |
|       - | 4588 |  |
|       - | 4589 | `	/* Argument validation mimicking PHP errors. */` |
|      29 | 4590 | `	if( nArg < 2 ){` |
|       4 | 4591 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4592 | `			"ArgumentCountError",` |
|       - | 4593 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4594 | `			nArg` |
|       - | 4595 | `			);` |
|       - | 4596 | `	}` |
|      26 | 4597 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4598 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4599 | `			"TypeError",` |
|       - | 4600 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4601 | `			ph7_type_name(apArg[0])` |
|       - | 4602 | `			);` |
|       - | 4603 | `	}` |
|       - | 4604 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4605 | `	 * expected to be a callback. */` |
|      38 | 4606 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      19 | 4607 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4608 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4609 | `				"TypeError",` |
|       - | 4610 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4611 | `				i + 1,` |
|       2 | 4612 | `				ph7_type_name(apArg[i])` |
|       - | 4613 | `				);` |
|       - | 4614 | `		}` |
|       9 | 4615 | `	}` |
|       - | 4616 | `	/* Point to the callback value */` |
|      22 | 4617 | `	pCallback = apArg[nArg - 1];` |
|      22 | 4618 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4619 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4620 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4621 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4622 | `		 * string given" which we also reproduce. */` |
|       9 | 4623 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4624 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4625 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4626 | `				"TypeError",` |
|       - | 4627 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4628 | `				nArg` |
|       - | 4629 | `				);` |
|       - | 4630 | `		}` |
|       6 | 4631 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4632 | `			/* neither array nor string */` |
|       8 | 4633 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4634 | `				"TypeError",` |
|       - | 4635 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4636 | `				nArg` |
|       - | 4637 | `				);` |
|       - | 4638 | `		}` |
|       - | 4639 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4640 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4641 | `			"TypeError",` |
|       - | 4642 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4643 | `			nArg,` |
|     ! 0 | 4644 | `			ph7_type_name(pCallback)` |
|       - | 4645 | `			);` |
|       - | 4646 | `	}` |
|      13 | 4647 | `	if( nArg == 2 ){` |
|       - | 4648 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4649 | `		 * input array. */` |
|       3 | 4650 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4651 | `		return PH7_OK;` |
|       - | 4652 | `	}` |
|       - | 4653 | `	/* Create a new array */` |
|      11 | 4654 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4655 | `	if( pArray == 0 ){` |
|     ! 0 | 4656 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4657 | `		return PH7_OK;` |
|       - | 4658 | `	}` |
|       - | 4659 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4660 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4661 | `	/* Perform the diff */` |
|      11 | 4662 | `	pEntry = pSrc->pFirst;` |
|      11 | 4663 | `	n = pSrc->nEntry;` |
|      21 | 4664 | `	for(;;){` |
|       - | 4665 | `		int keep;` |
|      27 | 4666 | `		if( n < 1 ){` |
|       9 | 4667 | `			break;` |
|       - | 4668 | `		}` |
|      19 | 4669 | `		keep = 1;` |
|      31 | 4670 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4671 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 4672 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4673 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 4674 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 4675 | `			while( pIt ){` |
|       - | 4676 | `				/* build temporary key values for callback */` |
|       - | 4677 | `				ph7_value key1, key2, result;` |
|       - | 4678 | `				/* initialise only once using the appropriate helper */` |
|      33 | 4679 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4680 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4681 | `				}else{` |
|       - | 4682 | `					SyString sStr;` |
|      33 | 4683 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4684 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4685 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 4686 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4687 | `				}` |
|      33 | 4688 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4689 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4690 | `				}else{` |
|       - | 4691 | `					SyString sStr;` |
|      33 | 4692 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4693 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4694 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 4695 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4696 | `				}` |
|      33 | 4697 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4698 | `				/* call user callback with (key1, key2) */` |
|       - | 4699 | `				{` |
|       - | 4700 | `					ph7_value *apK[2];` |
|      33 | 4701 | `					apK[0] = &key1;` |
|      33 | 4702 | `					apK[1] = &key2;` |
|      33 | 4703 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4704 | `				}` |
|      33 | 4705 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 4706 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 4707 | `					 * array_uintersect (which signal back from` |
|       - | 4708 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 4709 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 4710 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 4711 | `					PH7_MemObjRelease(&result);` |
|       3 | 4712 | `					PH7_MemObjRelease(&key1);` |
|       3 | 4713 | `					PH7_MemObjRelease(&key2);` |
|       3 | 4714 | `					return PH7_EXCEPTION;` |
|       - | 4715 | `				}` |
|      31 | 4716 | `				if( rc == SXRET_OK ){` |
|      31 | 4717 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4718 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4719 | `					}` |
|      31 | 4720 | `					if( result.x.iVal == 0 ){` |
|       - | 4721 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4722 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4723 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4724 | `						if( pVal1 && pVal2 ){` |
|      13 | 4725 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4726 | `								keep = 0;` |
|       9 | 4727 | `								PH7_MemObjRelease(&result);` |
|       - | 4728 | `								/* release keys too before breaking */` |
|       9 | 4729 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4730 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4731 | `								break;` |
|       - | 4732 | `							}` |
|       2 | 4733 | `						}` |
|       2 | 4734 | `					}` |
|      11 | 4735 | `				}` |
|      23 | 4736 | `				PH7_MemObjRelease(&result);` |
|      23 | 4737 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4738 | `				PH7_MemObjRelease(&key2);` |
|       - | 4739 | `				/* move to next node */` |
|      23 | 4740 | `				pIt = pIt->pPrev;` |
|      23 | 4741 | `				if( keep == 0 ) break;` |
|       1 | 4742 | `			}` |
|      21 | 4743 | `			if( keep == 0 ) break;` |
|       7 | 4744 | `		}` |
|      17 | 4745 | `		if( keep ){` |
|       - | 4746 | `			/* Perform the insertion */` |
|       9 | 4747 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4748 | `		}` |
|       - | 4749 | `		/* Point to the next entry */` |
|      17 | 4750 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4751 | `		n--;` |
|       1 | 4752 | `	}` |
|       - | 4753 | `	/* Return the freshly created array */` |
|       9 | 4754 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4755 | `	return PH7_OK;` |
|      17 | 4756 |  |
|       - | 4757 | `/*` |
|       - | 4758 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4759 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4760 | ` * Parameters` |
|       - | 4761 | ` *  $array1` |
|       - | 4762 | ` *    The array to compare from` |
|       - | 4763 | ` *  $array2` |
|       - | 4764 | ` *    An array to compare against` |
|       - | 4765 | ` *  $...` |
|       - | 4766 | ` *   More arrays to compare against` |
|       - | 4767 | ` * Return` |
|       - | 4768 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4769 | ` *  in any of the other arrays.` |
|       - | 4770 | ` * Note that NULL is returned on failure.` |
|       - | 4771 | ` */` |
|      14 | 4772 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4773 |  |
|       - | 4774 | `	ph7_hashmap_node *pEntry;` |
|       - | 4775 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4776 | `	ph7_value *pArray;` |
|       - | 4777 | `	sxi32 rc;` |
|       - | 4778 | `	sxu32 n;` |
|       - | 4779 | `	int i;` |
|       - | 4780 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4781 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4782 | `	 * helpers. */` |
|      18 | 4783 | `	if( nArg < 1 ){` |
|       4 | 4784 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4785 | `			"ArgumentCountError",` |
|       - | 4786 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4787 | `			nArg` |
|       - | 4788 | `			);` |
|       - | 4789 | `	}` |
|      15 | 4790 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4791 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4792 | `			"TypeError",` |
|       - | 4793 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4794 | `			ph7_type_name(apArg[0])` |
|       - | 4795 | `			);` |
|       - | 4796 | `	}` |
|      20 | 4797 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4798 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4799 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4800 | `				"TypeError",` |
|       - | 4801 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4802 | `				i + 1,` |
|       2 | 4803 | `				ph7_type_name(apArg[i])` |
|       - | 4804 | `				);` |
|       - | 4805 | `		}` |
|       5 | 4806 | `	}` |
|       9 | 4807 | `	if( nArg == 1 ){` |
|       - | 4808 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4809 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4810 | `		return PH7_OK;` |
|       - | 4811 | `	}` |
|       - | 4812 | `	/* Create a new array */` |
|       7 | 4813 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4814 | `	if( pArray == 0 ){` |
|     ! 0 | 4815 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4816 | `		return PH7_OK;` |
|       - | 4817 | `	}` |
|       - | 4818 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4819 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4820 | `	/* Perfrom the diff */` |
|       7 | 4821 | `	pEntry = pSrc->pFirst;` |
|       7 | 4822 | `	n = pSrc->nEntry;` |
|      12 | 4823 | `	for(;;){` |
|      25 | 4824 | `		if( n < 1 ){` |
|       7 | 4825 | `			break;` |
|       - | 4826 | `		}` |
|      31 | 4827 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4828 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4829 | `				/* ignore */` |
|     ! 0 | 4830 | `				continue;` |
|       - | 4831 | `			}` |
|      23 | 4832 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4833 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4834 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4835 | `				/* Blob lookup */` |
|      17 | 4836 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4837 | `			}else{` |
|       - | 4838 | `				/* Int lookup */` |
|       7 | 4839 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4840 | `			}` |
|      23 | 4841 | `			if( rc == SXRET_OK ){` |
|       - | 4842 | `				/* Key exists,break immediately */` |
|      11 | 4843 | `				break;` |
|       - | 4844 | `			}` |
|       7 | 4845 | `		}` |
|      19 | 4846 | `		if( i >= nArg ){` |
|       - | 4847 | `			/* Perform the insertion */` |
|       9 | 4848 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4849 | `		}` |
|       - | 4850 | `		/* Point to the next entry */` |
|      19 | 4851 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4852 | `		n--;` |
|       1 | 4853 | `	}` |
|       - | 4854 | `	/* Return the freshly created array */` |
|       7 | 4855 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4856 | `	return PH7_OK;` |
|      11 | 4857 |  |
|       - | 4858 | `/*` |
|       - | 4859 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4860 | ` *  Computes the intersection of arrays.` |
|       - | 4861 | ` * Parameters` |
|       - | 4862 | ` *  $array1` |
|       - | 4863 | ` *    The array to compare from` |
|       - | 4864 | ` *  $array2` |
|       - | 4865 | ` *    An array to compare against` |
|       - | 4866 | ` *  $...` |
|       - | 4867 | ` *   More arrays to compare against` |
|       - | 4868 | ` * Return` |
|       - | 4869 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4870 | ` *  in all of the parameters.` |
|       - | 4871 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4872 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4873 | ` */` |
|      22 | 4874 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4875 |  |
|       - | 4876 | `	ph7_hashmap_node *pEntry;` |
|       - | 4877 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4878 | `	ph7_value *pArray;` |
|       - | 4879 | `	ph7_value *pVal;` |
|       - | 4880 | `	sxi32 rc;` |
|       - | 4881 | `	sxu32 n;` |
|       - | 4882 | `	int i;` |
|      26 | 4883 | `	if( nArg < 1 ){` |
|       4 | 4884 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4885 | `			"ArgumentCountError",` |
|       - | 4886 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4887 | `			nArg` |
|       - | 4888 | `			);` |
|       - | 4889 | `	}` |
|      23 | 4890 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4891 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4892 | `			"TypeError",` |
|       - | 4893 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4894 | `			ph7_type_name(apArg[0])` |
|       - | 4895 | `			);` |
|       - | 4896 | `	}` |
|      36 | 4897 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4898 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4899 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4900 | `				"TypeError",` |
|       - | 4901 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4902 | `				i + 1,` |
|       2 | 4903 | `				ph7_type_name(apArg[i])` |
|       - | 4904 | `				);` |
|       - | 4905 | `		}` |
|       9 | 4906 | `	}` |
|      17 | 4907 | `	if( nArg == 1 ){` |
|       - | 4908 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4909 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4910 | `		return PH7_OK;` |
|       - | 4911 | `	}` |
|       - | 4912 | `	/* Create a new array */` |
|      15 | 4913 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4914 | `	if( pArray == 0 ){` |
|     ! 0 | 4915 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4916 | `		return PH7_OK;` |
|       - | 4917 | `	}` |
|       - | 4918 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4919 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4920 | `	/* Perform the intersection */` |
|      15 | 4921 | `	pEntry = pSrc->pFirst;` |
|      15 | 4922 | `	n = pSrc->nEntry;` |
|      31 | 4923 | `	for(;;){` |
|      63 | 4924 | `		if( n < 1 ){` |
|      15 | 4925 | `			break;` |
|       - | 4926 | `		}` |
|       - | 4927 | `		/* Extract the node value */` |
|      49 | 4928 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4929 | `		if( pVal ){` |
|      79 | 4930 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4931 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4932 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4933 | `				/* Perform the lookup */` |
|      55 | 4934 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4935 | `				if( rc != SXRET_OK ){` |
|       - | 4936 | `					/* Value does not exist */` |
|      25 | 4937 | `					break;` |
|       - | 4938 | `				}` |
|      16 | 4939 | `			}` |
|      49 | 4940 | `			if( i >= nArg ){` |
|       - | 4941 | `				/* Perform the insertion */` |
|      25 | 4942 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4943 | `			}` |
|      24 | 4944 | `		}` |
|       - | 4945 | `		/* Point to the next entry */` |
|      49 | 4946 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4947 | `		n--;` |
|       1 | 4948 | `	}` |
|       - | 4949 | `	/* Return the freshly created array */` |
|      15 | 4950 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4951 | `	return PH7_OK;` |
|      15 | 4952 |  |
|       - | 4953 | `/*` |
|       - | 4954 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4955 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4956 | ` * Parameters` |
|       - | 4957 | ` *  $array1` |
|       - | 4958 | ` *    The array to compare from` |
|       - | 4959 | ` *  $array2` |
|       - | 4960 | ` *    An array to compare against` |
|       - | 4961 | ` *  $...` |
|       - | 4962 | ` *   More arrays to compare against` |
|       - | 4963 | ` * Return` |
|       - | 4964 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4965 | ` *  in all the arguments, with matching keys.` |
|       - | 4966 | ` */` |
|      22 | 4967 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4968 |  |
|       - | 4969 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4970 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4971 | `	ph7_value *pArray;` |
|       - | 4972 | `	ph7_value *pVal;` |
|       - | 4973 | `	sxi32 rc;` |
|       - | 4974 | `	sxu32 n;` |
|       - | 4975 | `	int i;` |
|      26 | 4976 | `	if( nArg < 1 ){` |
|       4 | 4977 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4978 | `			"ArgumentCountError",` |
|       - | 4979 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4980 | `			nArg` |
|       - | 4981 | `			);` |
|       - | 4982 | `	}` |
|      23 | 4983 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4984 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4985 | `			"TypeError",` |
|       - | 4986 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4987 | `			ph7_type_name(apArg[0])` |
|       - | 4988 | `			);` |
|       - | 4989 | `	}` |
|      36 | 4990 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4991 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4992 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4993 | `				"TypeError",` |
|       - | 4994 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4995 | `				i + 1,` |
|       2 | 4996 | `				ph7_type_name(apArg[i])` |
|       - | 4997 | `				);` |
|       - | 4998 | `		}` |
|       9 | 4999 | `	}` |
|      17 | 5000 | `	if( nArg == 1 ){` |
|       - | 5001 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5002 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5003 | `		return PH7_OK;` |
|       - | 5004 | `	}` |
|       - | 5005 | `	/* Create a new array */` |
|      15 | 5006 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5007 | `	if( pArray == 0 ){` |
|     ! 0 | 5008 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5009 | `		return PH7_OK;` |
|       - | 5010 | `	}` |
|       - | 5011 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 5012 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5013 | `	/* Perform the intersection */` |
|      15 | 5014 | `	pEntry = pSrc->pFirst;` |
|      15 | 5015 | `	n = pSrc->nEntry;` |
|      15 | 5016 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 5017 | `	for(;;){` |
|      47 | 5018 | `		if( n < 1 ){` |
|      15 | 5019 | `			break;` |
|       - | 5020 | `		}` |
|       - | 5021 | `		/* Extract the node value */` |
|      33 | 5022 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 5023 | `		if( pVal ){` |
|      53 | 5024 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 5025 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 5026 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5027 | `				/* Perform a key lookup first */` |
|      37 | 5028 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 5029 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 5030 | `				}else{` |
|      23 | 5031 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 5032 | `				}` |
|      37 | 5033 | `				if( rc != SXRET_OK ){` |
|       - | 5034 | `					/* No such key,break immediately */` |
|       7 | 5035 | `					break;` |
|       - | 5036 | `				}` |
|       - | 5037 | `				/* Perform the lookup */` |
|      31 | 5038 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 5039 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 5040 | `					/* Value does not exist */` |
|       6 | 5041 | `					break;` |
|       - | 5042 | `				}` |
|      11 | 5043 | `			}` |
|      33 | 5044 | `			if( i >= nArg ){` |
|       - | 5045 | `				/* Perform the insertion */` |
|      17 | 5046 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5047 | `			}` |
|      16 | 5048 | `		}` |
|       - | 5049 | `		/* Point to the next entry */` |
|      33 | 5050 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5051 | `		n--;` |
|       1 | 5052 | `	}` |
|       - | 5053 | `	/* Return the freshly created array */` |
|      15 | 5054 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5055 | `	return PH7_OK;` |
|      15 | 5056 |  |
|       - | 5057 | `/*` |
|       - | 5058 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 5059 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 5060 | ` * Parameters` |
|       - | 5061 | ` *  $array1` |
|       - | 5062 | ` *    The array to compare from` |
|       - | 5063 | ` *  $...` |
|       - | 5064 | ` *   More arrays to compare against` |
|       - | 5065 | ` * Return` |
|       - | 5066 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 5067 | ` *  have keys that are present in all arguments.` |
|       - | 5068 | ` * Note that NULL is returned on failure.` |
|       - | 5069 | ` */` |
|      22 | 5070 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5071 |  |
|       - | 5072 | `	ph7_hashmap_node *pEntry;` |
|       - | 5073 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5074 | `	ph7_value *pArray;` |
|       - | 5075 | `	sxi32 rc;` |
|       - | 5076 | `	sxu32 n;` |
|       - | 5077 | `	int i;` |
|      26 | 5078 | `	if( nArg < 1 ){` |
|       4 | 5079 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5080 | `			"ArgumentCountError",` |
|       - | 5081 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 5082 | `			nArg` |
|       - | 5083 | `			);` |
|       - | 5084 | `	}` |
|      23 | 5085 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5086 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5087 | `			"TypeError",` |
|       - | 5088 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5089 | `			ph7_type_name(apArg[0])` |
|       - | 5090 | `			);` |
|       - | 5091 | `	}` |
|      36 | 5092 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5093 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5094 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5095 | `				"TypeError",` |
|       - | 5096 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5097 | `				i + 1,` |
|       2 | 5098 | `				ph7_type_name(apArg[i])` |
|       - | 5099 | `				);` |
|       - | 5100 | `		}` |
|       9 | 5101 | `	}` |
|      17 | 5102 | `	if( nArg == 1 ){` |
|       - | 5103 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5104 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5105 | `		return PH7_OK;` |
|       - | 5106 | `	}` |
|       - | 5107 | `	/* Create a new array */` |
|      15 | 5108 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5109 | `	if( pArray == 0 ){` |
|     ! 0 | 5110 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5111 | `		return PH7_OK;` |
|       - | 5112 | `	}` |
|       - | 5113 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 5114 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5115 | `	/* Perform the intersection */` |
|      15 | 5116 | `	pEntry = pSrc->pFirst;` |
|      15 | 5117 | `	n = pSrc->nEntry;` |
|      24 | 5118 | `	for(;;){` |
|      49 | 5119 | `		if( n < 1 ){` |
|      15 | 5120 | `			break;` |
|       - | 5121 | `		}` |
|      57 | 5122 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 5123 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 5124 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 5125 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5126 | `				/* Blob lookup */` |
|      27 | 5127 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 5128 | `			}else{` |
|       - | 5129 | `				/* Int key */` |
|      13 | 5130 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5131 | `			}` |
|      39 | 5132 | `			if( rc != SXRET_OK ){` |
|       - | 5133 | `				/* Key does not exist, break immediately */` |
|      17 | 5134 | `				break;` |
|       - | 5135 | `			}` |
|      12 | 5136 | `		}` |
|      35 | 5137 | `		if( i >= nArg ){` |
|       - | 5138 | `			/* Perform the insertion */` |
|      19 | 5139 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 5140 | `		}` |
|       - | 5141 | `		/* Point to the next entry */` |
|      35 | 5142 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 5143 | `		n--;` |
|       1 | 5144 | `	}` |
|       - | 5145 | `	/* Return the freshly created array */` |
|      15 | 5146 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5147 | `	return PH7_OK;` |
|      15 | 5148 |  |
|       - | 5149 | `/*` |
|       - | 5150 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 5151 | ` *  Computes the intersection of arrays.` |
|       - | 5152 | ` * Parameters` |
|       - | 5153 | ` *  $array1` |
|       - | 5154 | ` *    The array to compare from` |
|       - | 5155 | ` *  $array2` |
|       - | 5156 | ` *    An array to compare against` |
|       - | 5157 | ` *  $...` |
|       - | 5158 | ` *   More arrays to compare against` |
|       - | 5159 | ` * $callback` |
|       - | 5160 | ` *  The callback comparison function.` |
|       - | 5161 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5162 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5163 | ` *  than the second.` |
|       - | 5164 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5165 | ` * Return` |
|       - | 5166 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5167 | ` *  in all of the parameters. .` |
|       - | 5168 | ` * Note that NULL is returned on failure.` |
|       - | 5169 | ` */` |
|      26 | 5170 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5171 |  |
|       - | 5172 | `	ph7_hashmap_node *pEntry;` |
|       - | 5173 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5174 | `	ph7_value *pCallback;` |
|       - | 5175 | `	ph7_value *pArray;` |
|       - | 5176 | `	ph7_value *pVal;` |
|       - | 5177 | `	sxi32 rc;` |
|       - | 5178 | `	sxu32 n;` |
|       - | 5179 | `	int i;` |
|       - | 5180 |  |
|       - | 5181 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      31 | 5182 | `	if( nArg < 2 ){` |
|       4 | 5183 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5184 | `			"ArgumentCountError",` |
|       - | 5185 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5186 | `			nArg` |
|       - | 5187 | `			);` |
|       - | 5188 | `	}` |
|      29 | 5189 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5190 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5191 | `			"TypeError",` |
|       - | 5192 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5193 | `			ph7_type_name(apArg[0])` |
|       - | 5194 | `			);` |
|       - | 5195 | `	}` |
|       - | 5196 |  |
|      27 | 5197 | `	if( nArg == 2 ){` |
|       - | 5198 | `		/* Only the original array and the callback were provided. */` |
|       - | 5199 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5200 | `		 * validation ordering. */` |
|       3 | 5201 | `	} else {` |
|       - | 5202 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      39 | 5203 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      23 | 5204 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5205 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5206 | `					"TypeError",` |
|       - | 5207 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5208 | `					i + 1,` |
|       2 | 5209 | `					ph7_type_name(apArg[i])` |
|       - | 5210 | `					);` |
|       - | 5211 | `			}` |
|      13 | 5212 | `		}` |
|       - | 5213 | `	}` |
|       - | 5214 |  |
|       - | 5215 | `	/* Identify the callback (always expected as the last argument). */` |
|      25 | 5216 | `	pCallback = apArg[nArg - 1];` |
|       - | 5217 | `	/* Validate the callback to match PHP's error messages. */` |
|      25 | 5218 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      14 | 5219 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5220 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5221 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5222 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5223 | `			 */` |
|       9 | 5224 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       9 | 5225 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5226 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5227 | `					"TypeError",` |
|       - | 5228 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5229 | `					nArg` |
|       - | 5230 | `					);` |
|       - | 5231 | `			}` |
|       - | 5232 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5233 | `			{` |
|       6 | 5234 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       6 | 5235 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       6 | 5236 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5237 | `					int nMethodLen;` |
|       6 | 5238 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       6 | 5239 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       6 | 5240 | `					if( pClass ){` |
|       - | 5241 | `						/* Class exists but method is missing. */` |
|       4 | 5242 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5243 | `							"TypeError",` |
|       - | 5244 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5245 | `							nArg,` |
|       1 | 5246 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5247 | `							zMethod` |
|       - | 5248 | `							);` |
|       - | 5249 | `					}` |
|       - | 5250 | `					/* Class not found */` |
|       - | 5251 | `					{` |
|       - | 5252 | `						int nName;` |
|       3 | 5253 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5254 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5255 | `							"TypeError",` |
|       - | 5256 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5257 | `							nArg,` |
|       1 | 5258 | `							zName` |
|       - | 5259 | `							);` |
|       - | 5260 | `					}` |
|       - | 5261 | `				}` |
|       - | 5262 | `			}` |
|       - | 5263 | `			/* Fallback message */` |
|     ! 0 | 5264 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5265 | `				"TypeError",` |
|       - | 5266 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5267 | `				nArg` |
|       - | 5268 | `				);` |
|       - | 5269 | `		}` |
|       6 | 5270 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5271 | `			int len;` |
|       3 | 5272 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5273 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5274 | `				"TypeError",` |
|       - | 5275 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5276 | `				nArg,` |
|       1 | 5277 | `				zName` |
|       - | 5278 | `				);` |
|       - | 5279 | `		}` |
|       4 | 5280 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5281 | `			"TypeError",` |
|       - | 5282 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5283 | `			nArg` |
|       - | 5284 | `			);` |
|       - | 5285 | `	}` |
|       - | 5286 |  |
|      11 | 5287 | `	if( nArg == 2 ){` |
|       - | 5288 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5289 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5290 | `		return PH7_OK;` |
|       - | 5291 | `	}` |
|       - | 5292 |  |
|       - | 5293 | `	/* Create a new array */` |
|       7 | 5294 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5295 | `	if( pArray == 0 ){` |
|     ! 0 | 5296 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5297 | `		return PH7_OK;` |
|       - | 5298 | `	}` |
|       - | 5299 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 5300 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5301 | `	/* Perform the intersection */` |
|       7 | 5302 | `	pEntry = pSrc->pFirst;` |
|       7 | 5303 | `	n = pSrc->nEntry;` |
|       7 | 5304 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 5305 | `	for(;;){` |
|      19 | 5306 | `		if( n < 1 ){` |
|       5 | 5307 | `			break;` |
|       - | 5308 | `		}` |
|       - | 5309 | `		/* Extract the node value */` |
|      15 | 5310 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5311 | `		if( pVal ){` |
|      23 | 5312 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 5313 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5314 | `					/* ignore */` |
|     ! 0 | 5315 | `					continue;` |
|       - | 5316 | `				}` |
|       - | 5317 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 5318 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5319 | `				/* Perform the lookup */` |
|      15 | 5320 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 5321 | `				if( rc != SXRET_OK ){` |
|       - | 5322 | `					/* Value does not exist */` |
|       7 | 5323 | `					break;` |
|       - | 5324 | `				}` |
|       5 | 5325 | `			}` |
|      15 | 5326 | `			if( i >= (nArg-1) ){` |
|       - | 5327 | `				/* Perform the insertion */` |
|       9 | 5328 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5329 | `			}` |
|       7 | 5330 | `		}` |
|      15 | 5331 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 5332 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5333 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 5334 | `			return PH7_EXCEPTION;` |
|       - | 5335 | `		}` |
|       - | 5336 | `		/* Point to the next entry */` |
|      13 | 5337 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5338 | `		n--;` |
|       1 | 5339 | `	}` |
|       - | 5340 | `	/* Return the freshly created array */` |
|       5 | 5341 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5342 | `	return PH7_OK;` |
|      18 | 5343 |  |
|       - | 5344 | `/*` |
|       - | 5345 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5346 | ` *  Fill an array with values.` |
|       - | 5347 | ` * Parameters` |
|       - | 5348 | ` *  $start_index` |
|       - | 5349 | ` *    The first index of the returned array.` |
|       - | 5350 | ` *  $num` |
|       - | 5351 | ` *   Number of elements to insert.` |
|       - | 5352 | ` *  $value` |
|       - | 5353 | ` *    Value to use for filling.` |
|       - | 5354 | ` * Return` |
|       - | 5355 | ` *  The filled array or null on failure.` |
|       - | 5356 | ` */` |
|     238 | 5357 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5358 |  |
|       - | 5359 | `	ph7_value *pArray;` |
|       - | 5360 | `	int i,nEntry;` |
|       - | 5361 |  |
|       - | 5362 | `	/* PHP enforces argument count and type checks. */` |
|     243 | 5363 | `	if( nArg != 3 ){` |
|       - | 5364 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       8 | 5365 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5366 | `			"ArgumentCountError",` |
|       - | 5367 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5368 | `			nArg` |
|       - | 5369 | `			);` |
|       - | 5370 | `	}` |
|       - | 5371 |  |
|       - | 5372 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5373 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5374 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5375 | `	 * and NULLs are rejected outright. */` |
|     350 | 5376 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     353 | 5377 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5378 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5379 | `			"TypeError",` |
|       - | 5380 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5381 | `			ph7_type_name(apArg[0])` |
|       - | 5382 | `			);` |
|       - | 5383 | `	}` |
|     236 | 5384 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5385 | `		int len;` |
|       8 | 5386 | `		sxu8 bReal = FALSE;` |
|       8 | 5387 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5388 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5389 | `			/* Non‑numeric string is an error. */` |
|       3 | 5390 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5391 | `				"TypeError",` |
|       - | 5392 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5393 | `				);` |
|       - | 5394 | `		}` |
|       5 | 5395 | `		if( bReal ){` |
|       - | 5396 | `			/* float-string -> deprecation warning */` |
|       4 | 5397 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5398 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5399 | `				zStr` |
|       - | 5400 | `				);` |
|       1 | 5401 | `		}` |
|       2 | 5402 | `	}` |
|       - | 5403 |  |
|       - | 5404 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5405 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     345 | 5406 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     348 | 5407 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5408 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5409 | `			"TypeError",` |
|       - | 5410 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5411 | `			ph7_type_name(apArg[1])` |
|       - | 5412 | `			);` |
|       - | 5413 | `	}` |
|     233 | 5414 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5415 | `		int len;` |
|       3 | 5416 | `		sxu8 bReal = FALSE;` |
|       3 | 5417 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5418 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5419 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5420 | `				"TypeError",` |
|       - | 5421 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5422 | `				);` |
|       - | 5423 | `		}` |
|     ! 0 | 5424 | `	}` |
|       - | 5425 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5426 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5427 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5428 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5429 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5430 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5431 | `		if( d != (double)i64 ){` |
|       7 | 5432 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5433 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5434 | `				d` |
|       - | 5435 | `				);` |
|       2 | 5436 | `		}` |
|       2 | 5437 | `	}` |
|       - | 5438 |  |
|       - | 5439 | `	/* Total number of entries to insert */` |
|     230 | 5440 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5441 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5442 | `	if( nEntry < 0 ){` |
|       3 | 5443 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5444 | `			"ValueError",` |
|       - | 5445 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5446 | `			);` |
|       - | 5447 | `	}` |
|       - | 5448 |  |
|       - | 5449 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5450 | `	if( nEntry == 0 ){` |
|       7 | 5451 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5452 | `		return PH7_OK;` |
|       - | 5453 | `	}` |
|       - | 5454 |  |
|       - | 5455 | `	/* Create a new array */` |
|     221 | 5456 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5457 | `	if( pArray == 0 ){` |
|     ! 0 | 5458 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5459 | `	}` |
|       - | 5460 |  |
|       - | 5461 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5462 | `	if( ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]) != SXRET_OK ){` |
|     ! 0 | 5463 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5464 | `	}` |
|       - | 5465 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5466 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5467 | `		if( ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]) != SXRET_OK ){` |
|       - | 5468 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 5469 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 5470 | `		}` |
| 1058682 | 5471 | `	}` |
|       - | 5472 | `	/* Return the filled array */` |
|     221 | 5473 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5474 | `	return PH7_OK;` |
|     124 | 5475 |  |
|       - | 5476 | `/*` |
|       - | 5477 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5478 | ` *  Fill an array with values, specifying keys.` |
|       - | 5479 | ` * Parameters` |
|       - | 5480 | ` *  $input` |
|       - | 5481 | ` *   Array of values that will be used as key.` |
|       - | 5482 | ` *  $value` |
|       - | 5483 | ` *    Value to use for filling.` |
|       - | 5484 | ` * Return` |
|       - | 5485 | ` *  The filled array.` |
|       - | 5486 | ` * Throws` |
|       - | 5487 | ` *  ValueError if $input is not an array.` |
|       - | 5488 | ` */` |
|      26 | 5489 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5490 |  |
|       - | 5491 | `	ph7_hashmap_node *pEntry;` |
|       - | 5492 | `	ph7_hashmap *pSrc;` |
|       - | 5493 | `	ph7_value *pArray;` |
|       - | 5494 | `	sxu32 n;` |
|       - | 5495 | `	/* PHP enforces exactly 2 arguments. */` |
|      31 | 5496 | `	if( nArg != 2 ){` |
|      12 | 5497 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5498 | `			"ArgumentCountError",` |
|       - | 5499 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5500 | `			nArg` |
|       - | 5501 | `			);` |
|       - | 5502 | `	}` |
|       - | 5503 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 5504 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 5505 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5506 | `			"TypeError",` |
|       - | 5507 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5508 | `			ph7_type_name(apArg[0])` |
|       - | 5509 | `			);` |
|       - | 5510 | `	}` |
|       - | 5511 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5512 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5513 | `	/* Create a new array */` |
|      17 | 5514 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5515 | `	if( pArray == 0 ){` |
|     ! 0 | 5516 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5517 | `		return PH7_OK;` |
|       - | 5518 | `	}` |
|       - | 5519 | `	/* Perform the requested operation */` |
|      17 | 5520 | `	pEntry = pSrc->pFirst;` |
|      45 | 5521 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5522 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5523 | `		/* Point to the next entry */` |
|      29 | 5524 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5525 | `	}` |
|       - | 5526 | `	/* Return the filled array */` |
|      17 | 5527 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5528 | `	return PH7_OK;` |
|      18 | 5529 |  |
|       - | 5530 | `/*` |
|       - | 5531 | ` * array array_combine(array $keys,array $values)` |
|       - | 5532 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5533 | ` * Parameters` |
|       - | 5534 | ` *  $keys` |
|       - | 5535 | ` *    Array of keys to be used.` |
|       - | 5536 | ` * $values` |
|       - | 5537 | ` *   Array of values to be used.` |
|       - | 5538 | ` * Return` |
|       - | 5539 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5540 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5541 | ` *  not an array.` |
|       - | 5542 | ` */` |
|      18 | 5543 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5544 |  |
|       - | 5545 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5546 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5547 | `	ph7_value *pArray;` |
|       - | 5548 | `	sxu32 n;` |
|       - | 5549 | `	/* PHP enforces argument count and type checks. */` |
|      23 | 5550 | `	if( nArg != 2 ){` |
|       - | 5551 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5552 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5553 | `			"ArgumentCountError",` |
|       - | 5554 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5555 | `			nArg` |
|       - | 5556 | `			);` |
|       - | 5557 | `	}` |
|       - | 5558 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5559 | `	 * argument index in the error message. */` |
|      20 | 5560 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5561 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5562 | `			"TypeError",` |
|       - | 5563 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5564 | `			ph7_type_name(apArg[0])` |
|       - | 5565 | `			);` |
|       - | 5566 | `	}` |
|      17 | 5567 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5568 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5569 | `			"TypeError",` |
|       - | 5570 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5571 | `			ph7_type_name(apArg[1])` |
|       - | 5572 | `			);` |
|       - | 5573 | `	}` |
|       - | 5574 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5575 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5576 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5577 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5578 | `		/* Length mismatch -> ValueError */` |
|       3 | 5579 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5580 | `			"ValueError",` |
|       - | 5581 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5582 | `			);` |
|       - | 5583 | `	}` |
|       - | 5584 | `	/* Create a new array */` |
|      11 | 5585 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5586 | `	if( pArray == 0 ){` |
|     ! 0 | 5587 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5588 | `		return PH7_OK;` |
|       - | 5589 | `	}` |
|       - | 5590 | `	/* Perform the requested operation */` |
|      11 | 5591 | `	pKe = pKey->pFirst;` |
|      11 | 5592 | `	pVe = pValue->pFirst;` |
|      33 | 5593 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5594 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5595 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5596 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5597 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5598 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5599 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5600 | `		 * original array must not be mutated. */` |
|      23 | 5601 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5602 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5603 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5604 | `			if( pTmpKey ){` |
|       5 | 5605 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5606 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5607 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5608 | `				pKeyCopy = pTmpKey;` |
|       2 | 5609 | `			}` |
|       2 | 5610 | `		}` |
|      23 | 5611 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5612 | `		/* Point to the next entry */` |
|      23 | 5613 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5614 | `		pVe = pVe->pPrev;` |
|      12 | 5615 | `	}` |
|       - | 5616 | `	/* Return the filled array */` |
|      11 | 5617 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5618 | `	return PH7_OK;` |
|      14 | 5619 |  |
|       - | 5620 | `/*` |
|       - | 5621 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5622 | ` *  Return an array with elements in reverse order.` |
|       - | 5623 | ` * Parameters` |
|       - | 5624 | ` *  $array` |
|       - | 5625 | ` *   The input array.` |
|       - | 5626 | ` *  $preserve_keys (optional)` |
|       - | 5627 | ` *   If set to TRUE keys are preserved.` |
|       - | 5628 | ` * Return` |
|       - | 5629 | ` *  The reversed array.` |
|       - | 5630 | ` */` |
|      20 | 5631 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 5632 |  |
|       - | 5633 | `	ph7_hashmap_node *pEntry;` |
|       - | 5634 | `	ph7_hashmap *pSrc;` |
|       - | 5635 | `	ph7_value *pArray;` |
|       - | 5636 | `	int bPreserve;` |
|       - | 5637 | `	sxu32 n;` |
|      23 | 5638 | `	if( nArg < 1 ){` |
|       4 | 5639 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5640 | `			"ArgumentCountError",` |
|       - | 5641 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5642 | `			nArg` |
|       - | 5643 | `			);` |
|       - | 5644 | `	}` |
|       - | 5645 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5646 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5647 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5648 | `			"TypeError",` |
|       - | 5649 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5650 | `			ph7_type_name(apArg[0])` |
|       - | 5651 | `			);` |
|       - | 5652 | `	}` |
|      17 | 5653 | `	bPreserve = FALSE;` |
|      17 | 5654 | `	if( nArg > 1 ){` |
|       7 | 5655 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5656 | `	}` |
|       - | 5657 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5658 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5659 | `	/* Create a new array */` |
|      17 | 5660 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5661 | `	if( pArray == 0 ){` |
|     ! 0 | 5662 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5663 | `		return PH7_OK;` |
|       - | 5664 | `	}` |
|       - | 5665 | `	/* Perform the requested operation */` |
|      17 | 5666 | `	pEntry = pSrc->pLast;` |
|      55 | 5667 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5668 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5669 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5670 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5671 | `		/* Point to the previous entry */` |
|      39 | 5672 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5673 | `	}` |
|      17 | 5674 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5675 | `	return PH7_OK;` |
|      13 | 5676 |  |
|       - | 5677 | `/*` |
|       - | 5678 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5679 | ` *  Removes duplicate values from an array.` |
|       - | 5680 | ` * Parameters` |
|       - | 5681 | ` *  $array` |
|       - | 5682 | ` *   The input array.` |
|       - | 5683 | ` *  $flags` |
|       - | 5684 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5685 | ` *   behavior using these values:` |
|       - | 5686 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5687 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5688 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5689 | ` * Return` |
|       - | 5690 | ` *  The filtered array.` |
|       - | 5691 | ` */` |
|      24 | 5692 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5693 |  |
|       - | 5694 | `	ph7_hashmap_node *pEntry;` |
|       - | 5695 | `	ph7_value *pNeedle;` |
|       - | 5696 | `	ph7_hashmap *pSrc;` |
|       - | 5697 | `	ph7_value *pArray;` |
|       - | 5698 | `	int bStrict;` |
|       - | 5699 | `	sxi32 rc;` |
|       - | 5700 | `	sxu32 n;` |
|      28 | 5701 | `	if( nArg < 1 ){` |
|       - | 5702 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5703 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5704 | `			"ArgumentCountError",` |
|       - | 5705 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5706 | `			);` |
|       - | 5707 | `	}` |
|      25 | 5708 | `	if( nArg > 2 ){` |
|       - | 5709 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5710 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5711 | `			"ArgumentCountError",` |
|       - | 5712 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5713 | `			nArg` |
|       - | 5714 | `			);` |
|       - | 5715 | `	}` |
|       - | 5716 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5717 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5718 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5719 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5720 | `			"TypeError",` |
|       - | 5721 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5722 | `			ph7_type_name(apArg[0])` |
|       - | 5723 | `			);` |
|       - | 5724 | `	}` |
|      19 | 5725 | `	bStrict = FALSE;` |
|       - | 5726 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5727 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5728 | `	/* Create a new array */` |
|      19 | 5729 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5730 | `	if( pArray == 0 ){` |
|     ! 0 | 5731 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5732 | `		return PH7_OK;` |
|       - | 5733 | `	}` |
|       - | 5734 | `	/* Perform the requested operation */` |
|      19 | 5735 | `	pEntry = pSrc->pFirst;` |
|      83 | 5736 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5737 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5738 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5739 | `		if( pNeedle ){` |
|      65 | 5740 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5741 | `		}` |
|      65 | 5742 | `		if( rc != SXRET_OK ){` |
|       - | 5743 | `			/* Perform the insertion */` |
|      37 | 5744 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5745 | `		}` |
|       - | 5746 | `		/* Point to the next entry */` |
|      65 | 5747 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5748 | `	}` |
|       - | 5749 | `	/* Return the freshly created array */` |
|      19 | 5750 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5751 | `	return PH7_OK;` |
|      16 | 5752 |  |
|       - | 5753 | `/*` |
|       - | 5754 | ` * array array_flip(array $input)` |
|       - | 5755 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5756 | ` * Parameter` |
|       - | 5757 | ` *  $input` |
|       - | 5758 | ` *   Input array.` |
|       - | 5759 | ` * Return` |
|       - | 5760 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5761 | ` */` |
|      34 | 5762 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5763 |  |
|       - | 5764 | `	ph7_hashmap_node *pEntry;` |
|       - | 5765 | `	ph7_hashmap *pSrc;` |
|       - | 5766 | `	ph7_value *pArray;` |
|       - | 5767 | `	ph7_value *pKey;` |
|       - | 5768 | `	ph7_value sVal;` |
|       - | 5769 | `	sxu32 n;` |
|       - | 5770 |  |
|       - | 5771 | `	/* PHP requires exactly one argument */` |
|      39 | 5772 | `	if( nArg != 1 ){` |
|       - | 5773 | `		/* Use ArgumentCountError like other array helpers */` |
|       8 | 5774 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5775 | `			"ArgumentCountError",` |
|       - | 5776 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5777 | `			nArg` |
|       - | 5778 | `			);` |
|       - | 5779 | `	}` |
|       - | 5780 | `	/* Make sure we are dealing with a valid hashmap */` |
|      33 | 5781 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5782 | `		/* Type mismatch -> TypeError */` |
|       8 | 5783 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5784 | `			"TypeError",` |
|       - | 5785 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5786 | `			ph7_type_name(apArg[0])` |
|       - | 5787 | `			);` |
|       - | 5788 | `	}` |
|       - | 5789 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5790 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5791 | `	/* Create a new array */` |
|      27 | 5792 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5793 | `	if( pArray == 0 ){` |
|     ! 0 | 5794 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5795 | `		return PH7_OK;` |
|       - | 5796 | `	}` |
|       - | 5797 | `	/* Start processing */` |
|      27 | 5798 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5799 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5800 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5801 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5802 | `		if( pKey ){` |
|       - | 5803 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5804 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5805 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5806 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5807 | `					);` |
|   22236 | 5808 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5809 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5810 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5811 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5812 | `				}else{` |
|       - | 5813 | `					SyString sStr;` |
|    2227 | 5814 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5815 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5816 | `				}` |
|       - | 5817 | `				/* Perform the insertion */` |
|   22227 | 5818 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5819 | `				/* Safely release the value because each inserted entry` |
|       - | 5820 | `				 * has its own private copy of the value.` |
|       - | 5821 | `				 */` |
|   22227 | 5822 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5823 | `			}else{` |
|       - | 5824 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5825 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5826 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5827 | `					);` |
|       - | 5828 | `			}` |
|   11118 | 5829 | `		}` |
|       - | 5830 | `		/* Point to the next entry */` |
|   22237 | 5831 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5832 | `	}` |
|       - | 5833 | `	/* Return the freshly created array */` |
|      27 | 5834 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5835 | `	return PH7_OK;` |
|      22 | 5836 |  |
|       - | 5837 | `/*` |
|       - | 5838 | ` * number array_sum(array $array )` |
|       - | 5839 | ` *  Calculate the sum of values in an array.` |
|       - | 5840 | ` * Parameters` |
|       - | 5841 | ` *  $array: The input array.` |
|       - | 5842 | ` * Return` |
|       - | 5843 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5844 | ` */` |
|      24 | 5845 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5846 |  |
|       - | 5847 | `	ph7_hashmap_node *pEntry;` |
|       - | 5848 | `	ph7_value *pObj;` |
|      25 | 5849 | `	double dSum = 0;` |
|       - | 5850 | `	sxu32 n;` |
|      25 | 5851 | `	pEntry = pMap->pFirst;` |
|      91 | 5852 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5853 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5854 | `		if( pObj ){` |
|      67 | 5855 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5856 | `				dSum += pObj->rVal;` |
|      53 | 5857 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5858 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5859 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5860 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5861 | `					double dv = 0;` |
|      13 | 5862 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5863 | `					dSum += dv;` |
|       7 | 5864 | `				}` |
|      12 | 5865 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5866 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5867 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5868 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5869 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5870 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5871 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5872 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5873 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5874 | `			}` |
|       - | 5875 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5876 | `		}` |
|       - | 5877 | `		/* Point to the next entry */` |
|      67 | 5878 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5879 | `	}` |
|       - | 5880 | `	/* Return sum */` |
|      25 | 5881 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5882 |  |
|      30 | 5883 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5884 |  |
|       - | 5885 | `	ph7_hashmap_node *pEntry;` |
|       - | 5886 | `	ph7_value *pObj;` |
|      32 | 5887 | `	sxi64 nSum = 0;` |
|       - | 5888 | `	sxu32 n;` |
|      32 | 5889 | `	pEntry = pMap->pFirst;` |
|     128 | 5890 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      98 | 5891 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      98 | 5892 | `		if( pObj ){` |
|      98 | 5893 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      88 | 5894 | `				nSum += pObj->x.iVal;` |
|      54 | 5895 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5896 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5897 | `					sxi64 nv = 0;` |
|       5 | 5898 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5899 | `					nSum += nv;` |
|       3 | 5900 | `				}` |
|       8 | 5901 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5902 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5903 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5904 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5905 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5906 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5907 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5908 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5909 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5910 | `			}` |
|       - | 5911 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      48 | 5912 | `		}` |
|       - | 5913 | `		/* Point to the next entry */` |
|      98 | 5914 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      50 | 5915 | `	}` |
|       - | 5916 | `	/* Return sum */` |
|      32 | 5917 | `	ph7_result_int64(pCtx,nSum);` |
|      32 | 5918 |  |
|       - | 5919 | `/* number array_sum(array $array )` |
|       - | 5920 | ` * (See block-coment above)` |
|       - | 5921 | ` */` |
|      68 | 5922 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5923 |  |
|       - | 5924 | `	ph7_hashmap_node *pEntry;` |
|       - | 5925 | `	ph7_hashmap *pMap;` |
|       - | 5926 | `	ph7_value *pObj;` |
|      73 | 5927 | `	int useDouble = 0;` |
|       - | 5928 | `	sxu32 n;` |
|       - | 5929 | `	/* PHP requires exactly one argument */` |
|      73 | 5930 | `	if( nArg != 1 ){` |
|       8 | 5931 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5932 | `			"ArgumentCountError",` |
|       - | 5933 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5934 | `			nArg` |
|       - | 5935 | `			);` |
|       - | 5936 | `	}` |
|       - | 5937 | `	/* Make sure we are dealing with a valid hashmap */` |
|      68 | 5938 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5939 | `		/* Type mismatch -> TypeError */` |
|       8 | 5940 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5941 | `			"TypeError",` |
|       - | 5942 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5943 | `			ph7_type_name(apArg[0])` |
|       - | 5944 | `			);` |
|       - | 5945 | `	}` |
|      62 | 5946 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      62 | 5947 | `	if( pMap->nEntry < 1 ){` |
|       - | 5948 | `		/* Nothing to compute,return 0 */` |
|       7 | 5949 | `		ph7_result_int(pCtx,0);` |
|       7 | 5950 | `		return PH7_OK;` |
|       - | 5951 | `	}` |
|       - | 5952 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5953 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5954 | `	 */` |
|      56 | 5955 | `	pEntry = pMap->pFirst;` |
|     160 | 5956 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     130 | 5957 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     130 | 5958 | `		if( pObj ){` |
|     130 | 5959 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5960 | `				useDouble = 1;` |
|      19 | 5961 | `				break;` |
|       - | 5962 | `			}` |
|     112 | 5963 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5964 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5965 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5966 | `				sxu32 i;` |
|      23 | 5967 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5968 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5969 | `						useDouble = 1;` |
|       7 | 5970 | `						break;` |
|       - | 5971 | `					}` |
|       6 | 5972 | `				}` |
|      13 | 5973 | `				if( useDouble ){` |
|       7 | 5974 | `					break;` |
|       - | 5975 | `				}` |
|       3 | 5976 | `			}` |
|      52 | 5977 | `		}` |
|     106 | 5978 | `		pEntry = pEntry->pPrev;` |
|      54 | 5979 | `	}` |
|      56 | 5980 | `	if( useDouble ){` |
|      25 | 5981 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5982 | `	}else{` |
|      32 | 5983 | `		Int64Sum(pCtx,pMap);` |
|       - | 5984 | `	}` |
|      56 | 5985 | `	return PH7_OK;` |
|      39 | 5986 |  |
|       - | 5987 | `/*` |
|       - | 5988 | ` * number array_product(array $array )` |
|       - | 5989 | ` *  Calculate the product of values in an array.` |
|       - | 5990 | ` * Parameters` |
|       - | 5991 | ` *  $array: The input array.` |
|       - | 5992 | ` * Return` |
|       - | 5993 | ` *  Returns the product of values as an integer or float.` |
|       - | 5994 | ` */` |
|     ! 0 | 5995 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5996 |  |
|       - | 5997 | `	ph7_hashmap_node *pEntry;` |
|       - | 5998 | `	ph7_value *pObj;` |
|       - | 5999 | `	double dProd;` |
|       - | 6000 | `	sxu32 n;` |
|     ! 0 | 6001 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6002 | `	dProd = 1;` |
|     ! 0 | 6003 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6004 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6005 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6006 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6007 | `				dProd *= pObj->rVal;` |
|     ! 0 | 6008 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6009 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 6010 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6011 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6012 | `					double dv = 0;` |
|     ! 0 | 6013 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 6014 | `					dProd *= dv;` |
|     ! 0 | 6015 | `				}` |
|     ! 0 | 6016 | `			}` |
|     ! 0 | 6017 | `		}` |
|       - | 6018 | `		/* Point to the next entry */` |
|     ! 0 | 6019 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6020 | `	}` |
|       - | 6021 | `	/* Return product */` |
|     ! 0 | 6022 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 6023 |  |
|     ! 0 | 6024 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 6025 |  |
|       - | 6026 | `	ph7_hashmap_node *pEntry;` |
|       - | 6027 | `	ph7_value *pObj;` |
|       - | 6028 | `	sxi64 nProd;` |
|       - | 6029 | `	sxu32 n;` |
|     ! 0 | 6030 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6031 | `	nProd = 1;` |
|     ! 0 | 6032 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6033 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6034 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6035 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6036 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 6037 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6038 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 6039 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6040 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6041 | `					sxi64 nv = 0;` |
|     ! 0 | 6042 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 6043 | `					nProd *= nv;` |
|     ! 0 | 6044 | `				}` |
|     ! 0 | 6045 | `			}` |
|     ! 0 | 6046 | `		}` |
|       - | 6047 | `		/* Point to the next entry */` |
|     ! 0 | 6048 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6049 | `	}` |
|       - | 6050 | `	/* Return product */` |
|     ! 0 | 6051 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 6052 |  |
|       - | 6053 | `/* number array_product(array $array )` |
|       - | 6054 | ` * (See block-block comment above)` |
|       - | 6055 | ` */` |
|     ! 0 | 6056 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 6057 |  |
|       - | 6058 | `	ph7_hashmap *pMap;` |
|       - | 6059 | `	ph7_value *pObj;` |
|     ! 0 | 6060 | `	if( nArg < 1 ){` |
|       - | 6061 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 6062 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6063 | `		return PH7_OK;` |
|       - | 6064 | `	}` |
|       - | 6065 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 6066 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6067 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 6068 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6069 | `		return PH7_OK;` |
|       - | 6070 | `	}` |
|     ! 0 | 6071 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 6072 | `	if( pMap->nEntry < 1 ){` |
|       - | 6073 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 6074 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6075 | `		return PH7_OK;` |
|       - | 6076 | `	}` |
|       - | 6077 | `	/* If the first element is of type float,then perform floating` |
|       - | 6078 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 6079 | `	 */` |
|     ! 0 | 6080 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 6081 | `	if( pObj == 0 ){` |
|     ! 0 | 6082 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6083 | `		return PH7_OK;` |
|       - | 6084 | `	}` |
|     ! 0 | 6085 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6086 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 6087 | `	}else{` |
|     ! 0 | 6088 | `		Int64Prod(pCtx,pMap);` |
|       - | 6089 | `	}` |
|     ! 0 | 6090 | `	return PH7_OK;` |
|     ! 0 | 6091 |  |
|       - | 6092 | `/*` |
|       - | 6093 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 6094 | ` *  Pick one or more random entries out of an array.` |
|       - | 6095 | ` * Parameters` |
|       - | 6096 | ` * $input` |
|       - | 6097 | ` *  The input array.` |
|       - | 6098 | ` * $num_req` |
|       - | 6099 | ` *  Specifies how many entries you want to pick.` |
|       - | 6100 | ` * Return` |
|       - | 6101 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 6102 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 6103 | ` *  NULL is returned on failure.` |
|       - | 6104 | ` */` |
|       6 | 6105 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6106 |  |
|       - | 6107 | `	ph7_hashmap_node *pNode;` |
|       - | 6108 | `	ph7_hashmap *pMap;` |
|       7 | 6109 | `	int nItem = 1;` |
|       7 | 6110 | `	if( nArg < 1 ){` |
|       - | 6111 | `		/* Missing argument,return NULL */` |
|     ! 0 | 6112 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6113 | `		return PH7_OK;` |
|       - | 6114 | `	}` |
|       - | 6115 | `	/* Make sure we are dealing with an array */` |
|       7 | 6116 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6117 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6118 | `		return PH7_OK;` |
|       - | 6119 | `	}` |
|       - | 6120 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 6121 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 6122 | `	if(pMap->nEntry < 1 ){` |
|       - | 6123 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 6124 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6125 | `		return PH7_OK;` |
|       - | 6126 | `	}` |
|       7 | 6127 | `	if( nArg > 1 ){` |
|       3 | 6128 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 6129 | `	}` |
|       7 | 6130 | `	if( nItem < 2 ){` |
|       - | 6131 | `		sxu32 nEntry;` |
|       - | 6132 | `		/* Select a random number */` |
|       5 | 6133 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 6134 | `		/* Extract the desired entry.` |
|       - | 6135 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 6136 | `		 */` |
|       5 | 6137 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 6138 | `			pNode = pMap->pLast;` |
|       3 | 6139 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 6140 | `			if( nEntry > 1 ){` |
|     ! 0 | 6141 | `				for(;;){` |
|     ! 0 | 6142 | `					if( nEntry == 0 ){` |
|     ! 0 | 6143 | `						break;` |
|       - | 6144 | `					}` |
|       - | 6145 | `					/* Point to the previous entry */` |
|     ! 0 | 6146 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 6147 | `					nEntry--;` |
|     ! 0 | 6148 | `				}` |
|     ! 0 | 6149 | `			}` |
|       2 | 6150 | `		}else{` |
|       3 | 6151 | `			pNode = pMap->pFirst;` |
|       1 | 6152 | `			for(;;){` |
|       3 | 6153 | `				if( nEntry == 0 ){` |
|       3 | 6154 | `					break;` |
|       - | 6155 | `				}` |
|       - | 6156 | `				/* Point to the next entry */` |
|     ! 0 | 6157 | `				pNode = pNode->pPrev; /* Reverse link */` |
|     ! 0 | 6158 | `				nEntry--;` |
|     ! 0 | 6159 | `			}` |
|       - | 6160 | `		}` |
|       5 | 6161 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6162 | `			/* Int key */` |
|       3 | 6163 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6164 | `		}else{` |
|       - | 6165 | `			/* Blob key */` |
|       3 | 6166 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6167 | `		}` |
|       3 | 6168 | `	}else{` |
|       - | 6169 | `		ph7_value sKey,*pArray;` |
|       - | 6170 | `		ph7_hashmap *pDest;` |
|       - | 6171 | `		/* Create a new array */` |
|       3 | 6172 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6173 | `		if( pArray == 0 ){` |
|     ! 0 | 6174 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6175 | `			return PH7_OK;` |
|       - | 6176 | `		}` |
|       - | 6177 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6178 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6179 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6180 | `		/* Copy the first n items */` |
|       3 | 6181 | `		pNode = pMap->pFirst;` |
|       3 | 6182 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6183 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6184 | `		}` |
|       7 | 6185 | `		while( nItem > 0){` |
|       5 | 6186 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6187 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6188 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6189 | `			/* Point to the next entry */` |
|       5 | 6190 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6191 | `			nItem--;` |
|       1 | 6192 | `		}` |
|       - | 6193 | `		/* Shuffle the array */` |
|       3 | 6194 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6195 | `		/* Rehash node */` |
|       3 | 6196 | `		HashmapSortRehash(pDest);` |
|       - | 6197 | `		/* Return the random array */` |
|       3 | 6198 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6199 | `	}` |
|       7 | 6200 | `	return PH7_OK;` |
|       4 | 6201 |  |
|       - | 6202 | `/*` |
|       - | 6203 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6204 | ` *  Split an array into chunks.` |
|       - | 6205 | ` * Parameters` |
|       - | 6206 | ` * $input` |
|       - | 6207 | ` *   The array to work on` |
|       - | 6208 | ` * $size` |
|       - | 6209 | ` *   The size of each chunk` |
|       - | 6210 | ` * $preserve_keys` |
|       - | 6211 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6212 | ` *   the chunk numerically.` |
|       - | 6213 | ` * Return` |
|       - | 6214 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6215 | ` *  zero, with each dimension containing size elements.` |
|       - | 6216 | ` */` |
|      42 | 6217 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6218 |  |
|       - | 6219 | `	ph7_value *pArray,*pChunk;` |
|       - | 6220 | `	ph7_hashmap_node *pEntry;` |
|       - | 6221 | `	ph7_hashmap *pMap;` |
|       - | 6222 | `	int bPreserve;` |
|       - | 6223 | `	sxu32 nChunk;` |
|       - | 6224 | `	sxu32 nSize;` |
|       - | 6225 | `	sxu32 n;` |
|       - | 6226 | `	/* Argument count and types follow PHP semantics. */` |
|      47 | 6227 | `	if( nArg < 2 ){` |
|       - | 6228 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6229 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6230 | `			"ArgumentCountError",` |
|       - | 6231 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6232 | `			nArg` |
|       - | 6233 | `			);` |
|       - | 6234 | `	}` |
|      45 | 6235 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6236 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6237 | `			"TypeError",` |
|       - | 6238 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6239 | `			ph7_type_name(apArg[0])` |
|       - | 6240 | `			);` |
|       - | 6241 | `	}` |
|       - | 6242 | `	/* Create a new array */` |
|      43 | 6243 | `	pArray = ph7_context_new_array(pCtx);` |
|      43 | 6244 | `	if( pArray == 0 ){` |
|     ! 0 | 6245 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6246 | `		return PH7_OK;` |
|       - | 6247 | `	}` |
|       - | 6248 | `	/* Point to the internal representation of the input hashmap */` |
|      43 | 6249 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6250 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6251 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      57 | 6252 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      81 | 6253 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6254 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6255 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6256 | `			"TypeError",` |
|       - | 6257 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6258 | `			ph7_type_name(apArg[1])` |
|       - | 6259 | `			);` |
|       - | 6260 | `	}` |
|       - | 6261 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6262 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6263 | `	 * precision and PHP emits a deprecation warning. */` |
|      43 | 6264 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6265 | `		int len;` |
|       3 | 6266 | `		sxu8 bReal = FALSE;` |
|       3 | 6267 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6268 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6269 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6270 | `				"TypeError",` |
|       - | 6271 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6272 | `				);` |
|       - | 6273 | `		}` |
|     ! 0 | 6274 | `		if( bReal ){` |
|       - | 6275 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6276 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6277 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6278 | `				zStr` |
|       - | 6279 | `				);` |
|     ! 0 | 6280 | `		}` |
|     ! 0 | 6281 | `	}` |
|       - | 6282 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6283 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6284 | `	 * later via ph7_value_to_int. */` |
|      40 | 6285 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6286 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6287 | `		sxi64 i = (sxi64)d;` |
|       3 | 6288 | `		if( d != (double)i ){` |
|       4 | 6289 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6290 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6291 | `				d` |
|       - | 6292 | `				);` |
|       1 | 6293 | `		}` |
|       1 | 6294 | `	}` |
|       - | 6295 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6296 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6297 | `	{` |
|      40 | 6298 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      40 | 6299 | `		if( nSizeSigned < 1 ){` |
|       - | 6300 | `			/* size <= 0 -> ValueError */` |
|       6 | 6301 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6302 | `				"ValueError",` |
|       - | 6303 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6304 | `				);` |
|       - | 6305 | `		}` |
|      35 | 6306 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6307 | `	}` |
|      35 | 6308 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6309 | `		/* Return the whole array */` |
|       3 | 6310 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6311 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6312 | `		return PH7_OK;` |
|       - | 6313 | `	}` |
|      33 | 6314 | `	bPreserve = 0;` |
|      33 | 6315 | `	if( nArg > 2 ){` |
|       - | 6316 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6317 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6318 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6319 | `		 * normally, matching PHP behaviour. */` |
|      35 | 6320 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      35 | 6321 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6322 | `			ph7_value_is_resource(apArg[2]) ){` |
|       8 | 6323 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6324 | `				"TypeError",` |
|       - | 6325 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6326 | `				ph7_type_name(apArg[2])` |
|       - | 6327 | `				);` |
|       - | 6328 | `		}` |
|      21 | 6329 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6330 | `	}` |
|       - | 6331 | `	/* Start processing */` |
|      27 | 6332 | `	pEntry = pMap->pFirst;` |
|      27 | 6333 | `	nChunk = 0;` |
|      27 | 6334 | `	pChunk = 0;` |
|      27 | 6335 | `	n = pMap->nEntry;` |
|      56 | 6336 | `	for( ;; ){` |
|     113 | 6337 | `		if( n < 1 ){` |
|       - | 6338 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6339 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6340 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6341 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6342 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6343 | `			 * exists. */` |
|      27 | 6344 | `			if( pChunk ){` |
|      27 | 6345 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6346 | `			}` |
|      27 | 6347 | `			break;` |
|       - | 6348 | `		}` |
|      87 | 6349 | `		if( nChunk < 1 ){` |
|      71 | 6350 | `			if( pChunk ){` |
|       - | 6351 | `				/* Put the first chunk */` |
|      45 | 6352 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6353 | `			}` |
|       - | 6354 | `			/* Create a new dimension */` |
|      71 | 6355 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6356 | `												   * will be automatically released as soon we return` |
|       - | 6357 | `												   * from this function */` |
|      71 | 6358 | `			if( pChunk == 0 ){` |
|     ! 0 | 6359 | `				break;` |
|       - | 6360 | `			}` |
|      71 | 6361 | `			nChunk = nSize;` |
|      35 | 6362 | `		}` |
|       - | 6363 | `		/* Insert the entry */` |
|      87 | 6364 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6365 | `		/* Point to the next entry */` |
|      87 | 6366 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6367 | `		nChunk--;` |
|      87 | 6368 | `		n--;` |
|       1 | 6369 | `	}` |
|       - | 6370 | `	/* Return the multidimensional array */` |
|      27 | 6371 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6372 | `	return PH7_OK;` |
|      26 | 6373 |  |
|       - | 6374 | `/*` |
|       - | 6375 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6376 | ` *  Pad array to the specified length with a value.` |
|       - | 6377 | ` * $input` |
|       - | 6378 | ` *   Initial array of values to pad.` |
|       - | 6379 | ` * $pad_size` |
|       - | 6380 | ` *   New size of the array.` |
|       - | 6381 | ` * $pad_value` |
|       - | 6382 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6383 | ` */` |
|      28 | 6384 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6385 |  |
|       - | 6386 | `	ph7_hashmap *pMap;` |
|       - | 6387 | `	ph7_value *pArray;` |
|       - | 6388 | `	int nEntry;` |
|      33 | 6389 | `	if( nArg != 3 ){` |
|      12 | 6390 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6391 | `			"ArgumentCountError",` |
|       - | 6392 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6393 | `			nArg` |
|       - | 6394 | `			);` |
|       - | 6395 | `	}` |
|      24 | 6396 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6397 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6398 | `			"TypeError",` |
|       - | 6399 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6400 | `			ph7_type_name(apArg[0])` |
|       - | 6401 | `			);` |
|       - | 6402 | `	}` |
|       - | 6403 | `	/* Create a new array */` |
|      21 | 6404 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6405 | `	if( pArray == 0 ){` |
|     ! 0 | 6406 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6407 | `	}` |
|       - | 6408 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6409 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6410 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6411 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6412 | `	if( nEntry < 0 ){` |
|       9 | 6413 | `		nEntry = -nEntry;` |
|       9 | 6414 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6415 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6416 | `			/* Insert given items first */` |
|      17 | 6417 | `			while( nEntry > 0 ){` |
|      13 | 6418 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6419 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6420 | `				}` |
|      13 | 6421 | `				nEntry--;` |
|       1 | 6422 | `			}` |
|       - | 6423 | `			/* Merge the two arrays */` |
|       5 | 6424 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6425 | `		}else{` |
|       5 | 6426 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6427 | `		}` |
|      17 | 6428 | `	}else if( nEntry > 0 ){` |
|      11 | 6429 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6430 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6431 | `			/* Merge the two arrays first */` |
|       7 | 6432 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6433 | `			/* Insert given items */` |
|      25 | 6434 | `			while( nEntry > 0 ){` |
|      19 | 6435 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6436 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6437 | `				}` |
|      19 | 6438 | `				nEntry--;` |
|       1 | 6439 | `			}` |
|       4 | 6440 | `		}else{` |
|       5 | 6441 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6442 | `		}` |
|       6 | 6443 | `	}else{` |
|       - | 6444 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6445 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6446 | `	}` |
|       - | 6447 | `	/* Return the new array */` |
|      21 | 6448 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6449 | `	return PH7_OK;` |
|      19 | 6450 |  |
|       - | 6451 | `/*` |
|       - | 6452 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6453 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6454 | ` * Parameters` |
|       - | 6455 | ` * $array` |
|       - | 6456 | ` *   The array in which elements are replaced.` |
|       - | 6457 | ` * $array1` |
|       - | 6458 | ` *   The array from which elements will be extracted.` |
|       - | 6459 | ` * ....` |
|       - | 6460 | ` *  More arrays from which elements will be extracted.` |
|       - | 6461 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6462 | ` * Return` |
|       - | 6463 | ` *  Returns an array.` |
|       - | 6464 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6465 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6466 | ` */` |
|      22 | 6467 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 6468 |  |
|       - | 6469 | `	ph7_hashmap *pMap;` |
|       - | 6470 | `	ph7_value *pArray;` |
|       - | 6471 | `	int i;` |
|      26 | 6472 | `	if( nArg < 1 ){` |
|       3 | 6473 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6474 | `			"ArgumentCountError",` |
|       - | 6475 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6476 | `			);` |
|       - | 6477 | `	}` |
|      23 | 6478 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6479 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6480 | `			"TypeError",` |
|       - | 6481 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6482 | `			ph7_type_name(apArg[0])` |
|       - | 6483 | `			);` |
|       - | 6484 | `	}` |
|       - | 6485 | `	/* Create a new array */` |
|      20 | 6486 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6487 | `	if( pArray == 0 ){` |
|     ! 0 | 6488 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6489 | `		return PH7_OK;` |
|       - | 6490 | `	}` |
|       - | 6491 | `	/* Overwrite from the first array */` |
|      20 | 6492 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6493 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6494 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6495 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6496 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6497 | `			/* Type mismatch -> TypeError */` |
|       4 | 6498 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6499 | `				"TypeError",` |
|       - | 6500 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6501 | `				i + 1,` |
|       2 | 6502 | `				ph7_type_name(apArg[i])` |
|       - | 6503 | `				);` |
|       - | 6504 | `		}` |
|       - | 6505 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6506 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6507 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6508 | `	}` |
|       - | 6509 | `	/* Return the new array */` |
|      17 | 6510 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6511 | `	return PH7_OK;` |
|      15 | 6512 |  |
|       - | 6513 | `/*` |
|       - | 6514 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6515 | ` *  Filters elements of an array using a callback function.` |
|       - | 6516 | ` * Parameters` |
|       - | 6517 | ` *  $input` |
|       - | 6518 | ` *    The array to iterate over` |
|       - | 6519 | ` * $callback` |
|       - | 6520 | ` *    The callback function to use` |
|       - | 6521 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6522 | ` *    will be removed.` |
|       - | 6523 | ` * Return` |
|       - | 6524 | ` *  The filtered array.` |
|       - | 6525 | ` */` |
|      22 | 6526 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6527 |  |
|       - | 6528 | `	ph7_hashmap_node *pEntry;` |
|       - | 6529 | `	ph7_hashmap *pMap;` |
|       - | 6530 | `	ph7_value *pArray;` |
|       - | 6531 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6532 | `	ph7_value *pValue;` |
|       - | 6533 | `	sxi32 rc;` |
|       - | 6534 | `	int keep;` |
|       - | 6535 | `	sxu32 n;` |
|      24 | 6536 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6537 | `		/* Invalid arguments,return NULL */` |
|       5 | 6538 | `		ph7_result_null(pCtx);` |
|       5 | 6539 | `		return PH7_OK;` |
|       - | 6540 | `	}` |
|       - | 6541 | `	/* Create a new array */` |
|      20 | 6542 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6543 | `	if( pArray == 0 ){` |
|     ! 0 | 6544 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6545 | `		return PH7_OK;` |
|       - | 6546 | `	}` |
|       - | 6547 | `	/* Point to the internal representation of the input hashmap */` |
|      20 | 6548 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6549 | `	pEntry = pMap->pFirst;` |
|      20 | 6550 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      20 | 6551 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6552 | `	/* Perform the requested operation */` |
|      78 | 6553 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6554 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      64 | 6555 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      64 | 6556 | `		if( pValue == 0 ){` |
|       - | 6557 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6558 | `			keep = FALSE;` |
|      64 | 6559 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6560 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6561 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6562 | `				* silently dropped the element.  Emit similar message. */` |
|      36 | 6563 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6564 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6565 | `					int len;` |
|       3 | 6566 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6567 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6568 | `						"TypeError",` |
|       - | 6569 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6570 | `						zName` |
|       - | 6571 | `						);` |
|     ! 0 | 6572 | `				}else{` |
|     ! 0 | 6573 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6574 | `						"TypeError",` |
|       - | 6575 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6576 | `						ph7_type_name(apArg[1])` |
|       - | 6577 | `						);` |
|       - | 6578 | `				}` |
|       - | 6579 | `			}` |
|      33 | 6580 | `			keep = FALSE;` |
|      33 | 6581 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      33 | 6582 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 6583 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6584 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 6585 | `				return PH7_EXCEPTION;` |
|       - | 6586 | `			}` |
|      31 | 6587 | `			if( rc == SXRET_OK ){` |
|       - | 6588 | `				/* Perform a boolean cast */` |
|      31 | 6589 | `				keep = ph7_value_to_bool(&sResult);` |
|      15 | 6590 | `			}` |
|      31 | 6591 | `			PH7_MemObjRelease(&sResult);` |
|      16 | 6592 | `		}else{` |
|       - | 6593 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6594 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6595 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6596 | `			 */` |
|      29 | 6597 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6598 | `		}` |
|      59 | 6599 | `		if( keep ){` |
|       - | 6600 | `			/* Perform the insertion,now the callback returned true */` |
|      21 | 6601 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 6602 | `		}` |
|       - | 6603 | `		/* Point to the next entry */` |
|      59 | 6604 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      30 | 6605 | `	}` |
|      15 | 6606 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 6607 | `	return PH7_OK;` |
|      13 | 6608 |  |
|       - | 6609 | `/*` |
|       - | 6610 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 6611 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 6612 | ` * Parameters` |
|       - | 6613 | ` *  $callback` |
|       - | 6614 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 6615 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 6616 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 6617 | ` *   are zipped together.` |
|       - | 6618 | ` *  $array` |
|       - | 6619 | ` *   The first array to run through the callback function.` |
|       - | 6620 | ` *  $arrays` |
|       - | 6621 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 6622 | ` * Return` |
|       - | 6623 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 6624 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 6625 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 6626 | ` *  padding shorter arrays with NULL.` |
|       - | 6627 | ` */` |
|      54 | 6628 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6629 |  |
|       - | 6630 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6631 | `	ph7_hashmap_node *pEntry;` |
|       - | 6632 | `	ph7_hashmap *pMap;` |
|       - | 6633 | `	ph7_vm *pVm;` |
|       - | 6634 | `	int bNullCallback;` |
|       - | 6635 | `	sxi32 rc;` |
|       - | 6636 | `	int i;` |
|       - | 6637 | `	sxu32 n;` |
|      59 | 6638 | `	if( nArg < 2 ){` |
|       8 | 6639 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6640 | `			"ArgumentCountError",` |
|       - | 6641 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6642 | `			nArg` |
|       - | 6643 | `			);` |
|       - | 6644 | `	}` |
|      53 | 6645 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      53 | 6646 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6647 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6648 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6649 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6650 | `				"TypeError",` |
|       - | 6651 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6652 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6653 | `				zFunc` |
|       - | 6654 | `				);` |
|       - | 6655 | `		}` |
|       3 | 6656 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6657 | `			"TypeError",` |
|       - | 6658 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6659 | `			"no array or string given"` |
|       - | 6660 | `			);` |
|       - | 6661 | `	}` |
|       - | 6662 | `	/* Every remaining argument must be an array */` |
|     104 | 6663 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      60 | 6664 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 6665 | `			if( i == 1 ){` |
|       4 | 6666 | `				return PH7_VmThrowException(pCtx,` |
|       - | 6667 | `					"TypeError",` |
|       - | 6668 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6669 | `					ph7_type_name(apArg[1])` |
|       - | 6670 | `					);` |
|       - | 6671 | `			}` |
|     ! 0 | 6672 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6673 | `				"TypeError",` |
|       - | 6674 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 6675 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 6676 | `				);` |
|       - | 6677 | `		}` |
|      30 | 6678 | `	}` |
|      46 | 6679 | `	pVm = pCtx->pVm;` |
|       - | 6680 | `	/* Create a new array */` |
|      46 | 6681 | `	pArray = ph7_context_new_array(pCtx);` |
|      46 | 6682 | `	if( pArray == 0 ){` |
|     ! 0 | 6683 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6684 | `		return PH7_OK;` |
|       - | 6685 | `	}` |
|      46 | 6686 | `	PH7_MemObjInit(pVm,&sResult);` |
|      46 | 6687 | `	PH7_MemObjInit(pVm,&sKey);` |
|      46 | 6688 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      46 | 6689 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      46 | 6690 | `	if( nArg == 2 ){` |
|       - | 6691 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      36 | 6692 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      36 | 6693 | `		pEntry = pMap->pFirst;` |
|     110 | 6694 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6695 | `			/* Extract the node value */` |
|      78 | 6696 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|      78 | 6697 | `			if( pValue ){` |
|       - | 6698 | `				/* Extract the node key */` |
|      78 | 6699 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      78 | 6700 | `				if( bNullCallback ){` |
|       - | 6701 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 6702 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6703 | `				}else{` |
|       - | 6704 | `					/* Invoke the supplied callback */` |
|      68 | 6705 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|      68 | 6706 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 6707 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 6708 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       3 | 6709 | `						PH7_MemObjRelease(&sKey);` |
|       3 | 6710 | `						PH7_MemObjRelease(&sResult);` |
|       3 | 6711 | `						return PH7_EXCEPTION;` |
|       - | 6712 | `					}` |
|       - | 6713 | `					/* Insert the callback return value */` |
|      66 | 6714 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6715 | `				}` |
|      76 | 6716 | `				PH7_MemObjRelease(&sKey);` |
|      76 | 6717 | `				PH7_MemObjRelease(&sResult);` |
|      37 | 6718 | `			}` |
|       - | 6719 | `			/* Point to the next entry */` |
|      76 | 6720 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      39 | 6721 | `		}` |
|      18 | 6722 | `	}else{` |
|       - | 6723 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 6724 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 6725 | `		int nArrays = nArg - 1;` |
|       - | 6726 | `		ph7_hashmap_node **apCur;` |
|       - | 6727 | `		ph7_value **apCallArg;` |
|       - | 6728 | `		ph7_value sNull;` |
|      11 | 6729 | `		sxu32 nMax = 0;` |
|      11 | 6730 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 6731 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 6732 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 6733 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 6734 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 6735 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6736 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6737 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 6738 | `			return PH7_OK;` |
|       - | 6739 | `		}` |
|      11 | 6740 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 6741 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 6742 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 6743 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 6744 | `			apCur[i] = pMap->pFirst;` |
|      23 | 6745 | `			if( pMap->nEntry > nMax ){` |
|      13 | 6746 | `				nMax = pMap->nEntry;` |
|       6 | 6747 | `			}` |
|      12 | 6748 | `		}` |
|      35 | 6749 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 6750 | `			ph7_value *pZip = 0;` |
|      25 | 6751 | `			if( bNullCallback ){` |
|       - | 6752 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 6753 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 6754 | `			}` |
|      79 | 6755 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 6756 | `				ph7_value *pv = &sNull;` |
|      55 | 6757 | `				if( apCur[i] ){` |
|      53 | 6758 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 6759 | `					if( pNodeVal ){` |
|      53 | 6760 | `						pv = pNodeVal;` |
|      26 | 6761 | `					}` |
|      53 | 6762 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 6763 | `				}` |
|      55 | 6764 | `				if( bNullCallback ){` |
|       9 | 6765 | `					if( pZip ){` |
|       9 | 6766 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 6767 | `					}` |
|       5 | 6768 | `				}else{` |
|      47 | 6769 | `					apCallArg[i] = pv;` |
|       - | 6770 | `				}` |
|      28 | 6771 | `			}` |
|      25 | 6772 | `			if( bNullCallback ){` |
|       5 | 6773 | `				if( pZip ){` |
|       5 | 6774 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 6775 | `				}` |
|       3 | 6776 | `			}else{` |
|      21 | 6777 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 6778 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6779 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 6780 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 6781 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 6782 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6783 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6784 | `					return PH7_EXCEPTION;` |
|       - | 6785 | `				}` |
|      21 | 6786 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 6787 | `				PH7_MemObjRelease(&sResult);` |
|       - | 6788 | `			}` |
|      13 | 6789 | `		}` |
|      11 | 6790 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 6791 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 6792 | `		PH7_MemObjRelease(&sNull);` |
|       - | 6793 | `	}` |
|      44 | 6794 | `	PH7_MemObjRelease(&sKey);` |
|      44 | 6795 | `	PH7_MemObjRelease(&sResult);` |
|      44 | 6796 | `	ph7_result_value(pCtx,pArray);` |
|      44 | 6797 | `	return PH7_OK;` |
|      32 | 6798 |  |
|       - | 6799 | `/*` |
|       - | 6800 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6801 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6802 | ` * Parameters` |
|       - | 6803 | ` *  $array` |
|       - | 6804 | ` *   The input array.` |
|       - | 6805 | ` *  $callback` |
|       - | 6806 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6807 | ` *  $initial` |
|       - | 6808 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6809 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6810 | ` * Return` |
|       - | 6811 | ` *  Returns the resulting value.` |
|       - | 6812 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6813 | ` */` |
|      34 | 6814 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6815 |  |
|       - | 6816 | `	ph7_hashmap_node *pEntry;` |
|       - | 6817 | `	ph7_hashmap *pMap;` |
|       - | 6818 | `	ph7_value *pValue;` |
|       - | 6819 | `	ph7_value sResult;` |
|       - | 6820 | `	sxi32 rc;` |
|       - | 6821 | `	sxu32 n;` |
|      39 | 6822 | `	if( nArg < 2 ){` |
|       8 | 6823 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6824 | `			"ArgumentCountError",` |
|       - | 6825 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6826 | `			nArg` |
|       - | 6827 | `			);` |
|       - | 6828 | `	}` |
|      35 | 6829 | `	if( nArg > 3 ){` |
|       4 | 6830 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6831 | `			"ArgumentCountError",` |
|       - | 6832 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6833 | `			nArg` |
|       - | 6834 | `			);` |
|       - | 6835 | `	}` |
|      33 | 6836 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6837 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6838 | `			"TypeError",` |
|       - | 6839 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6840 | `			ph7_type_name(apArg[0])` |
|       - | 6841 | `			);` |
|       - | 6842 | `	}` |
|      31 | 6843 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      12 | 6844 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6845 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6846 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6847 | `				"TypeError",` |
|       - | 6848 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6849 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6850 | `				zFunc` |
|       - | 6851 | `				);` |
|       - | 6852 | `		}` |
|       9 | 6853 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6854 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6855 | `				"TypeError",` |
|       - | 6856 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6857 | `				"array callback must have exactly two members"` |
|       - | 6858 | `				);` |
|       - | 6859 | `		}` |
|       6 | 6860 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6861 | `			"TypeError",` |
|       - | 6862 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6863 | `			"no array or string given"` |
|       - | 6864 | `			);` |
|       - | 6865 | `	}` |
|       - | 6866 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6867 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6868 | `	/* Assume a NULL initial value */` |
|      19 | 6869 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 6870 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 6871 | `	if( nArg > 2 ){` |
|       - | 6872 | `		/* Set the initial value */` |
|      13 | 6873 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 6874 | `	}` |
|       - | 6875 | `	/* Perform the requested operation */` |
|      19 | 6876 | `	pEntry = pMap->pFirst;` |
|      55 | 6877 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6878 | `		/* Extract the node value */` |
|      39 | 6879 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6880 | `		/* Invoke the supplied callback */` |
|      39 | 6881 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      39 | 6882 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 6883 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6884 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 6885 | `			return PH7_EXCEPTION;` |
|       - | 6886 | `		}` |
|       - | 6887 | `		/* Point to the next entry */` |
|      37 | 6888 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6889 | `	}` |
|      17 | 6890 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      17 | 6891 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 6892 | `	return PH7_OK;` |
|      22 | 6893 |  |
|       - | 6894 | `/*` |
|       - | 6895 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6896 | ` *  Apply a user function to every member of an array.` |
|       - | 6897 | ` * Parameters` |
|       - | 6898 | ` *  $array` |
|       - | 6899 | ` *   The input array.` |
|       - | 6900 | ` *  $funcname` |
|       - | 6901 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6902 | ` *   the first, and the key/index second.` |
|       - | 6903 | ` * Note:` |
|       - | 6904 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6905 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6906 | ` *  be made in the original array itself.` |
|       - | 6907 | ` *  $userdata` |
|       - | 6908 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6909 | ` *   to the callback funcname.` |
|       - | 6910 | ` * Return` |
|       - | 6911 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6912 | ` */` |
|      38 | 6913 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6914 |  |
|       - | 6915 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6916 | `	ph7_hashmap_node *pEntry;` |
|       - | 6917 | `	ph7_hashmap *pMap;` |
|       - | 6918 | `	sxu32 n;` |
|      43 | 6919 | `	if( nArg < 2 ){` |
|       8 | 6920 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6921 | `			"ArgumentCountError",` |
|       - | 6922 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6923 | `			nArg` |
|       - | 6924 | `			);` |
|       - | 6925 | `	}` |
|      39 | 6926 | `	if( nArg > 3 ){` |
|       4 | 6927 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6928 | `			"ArgumentCountError",` |
|       - | 6929 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6930 | `			nArg` |
|       - | 6931 | `			);` |
|       - | 6932 | `	}` |
|      37 | 6933 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6934 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6935 | `			"TypeError",` |
|       - | 6936 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6937 | `			ph7_type_name(apArg[0])` |
|       - | 6938 | `			);` |
|       - | 6939 | `	}` |
|      35 | 6940 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 6941 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6942 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6943 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6944 | `				"TypeError",` |
|       - | 6945 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6946 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6947 | `				zFunc` |
|       - | 6948 | `				);` |
|       - | 6949 | `		}` |
|      12 | 6950 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 6951 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6952 | `				"TypeError",` |
|       - | 6953 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6954 | `				"array callback must have exactly two members"` |
|       - | 6955 | `				);` |
|       - | 6956 | `		}` |
|       6 | 6957 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6958 | `			"TypeError",` |
|       - | 6959 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6960 | `			"no array or string given"` |
|       - | 6961 | `			);` |
|       - | 6962 | `	}` |
|      21 | 6963 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6964 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6965 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 6966 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 6967 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 6968 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6969 | `	/* Perform the desired operation */` |
|      21 | 6970 | `	pEntry = pMap->pFirst;` |
|      61 | 6971 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6972 | `		/* Extract the node value */` |
|      43 | 6973 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 6974 | `		if( pValue ){` |
|       - | 6975 | `			sxi32 rcW;` |
|       - | 6976 | `			/* Extract the entry key */` |
|      43 | 6977 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6978 | `			/* Invoke the supplied callback */` |
|      43 | 6979 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 6980 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 6981 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 6982 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6983 | `				return PH7_EXCEPTION;` |
|       - | 6984 | `			}` |
|      20 | 6985 | `		}` |
|       - | 6986 | `		/* Point to the next entry */` |
|      41 | 6987 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6988 | `	}` |
|       - | 6989 | `	/* All done, return TRUE */` |
|      19 | 6990 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6991 | `	return PH7_OK;` |
|      24 | 6992 |  |
|       - | 6993 | `/*` |
|       - | 6994 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6995 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6996 | ` */` |
|      22 | 6997 | `static sxi32 HashmapWalkRecursive(` |
|       - | 6998 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6999 | `	ph7_value *pCallback, /* User callback */` |
|       - | 7000 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 7001 | `	int iNest             /* Nesting level */` |
|       - | 7002 | `	)` |
|       1 | 7003 |  |
|       - | 7004 | `	ph7_hashmap_node *pEntry;` |
|       - | 7005 | `	ph7_value *pValue,sKey;` |
|       - | 7006 | `	sxi32 rc;` |
|       - | 7007 | `	sxu32 n;` |
|       - | 7008 | `	/* Iterate through hashmap entries */` |
|      23 | 7009 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 7010 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 7011 | `	pEntry = pMap->pFirst;` |
|      59 | 7012 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7013 | `		/* Extract the node value */` |
|      37 | 7014 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 7015 | `		if( pValue ){` |
|      37 | 7016 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 7017 | `				if( iNest < 32 ){` |
|       - | 7018 | `					/* Recurse */` |
|      11 | 7019 | `					iNest++;` |
|      11 | 7020 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 7021 | `					iNest--;` |
|      11 | 7022 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 7023 | `						return PH7_EXCEPTION;` |
|       - | 7024 | `					}` |
|       5 | 7025 | `				}` |
|       6 | 7026 | `			}else{` |
|       - | 7027 | `				/* Extract the node key */` |
|      27 | 7028 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 7029 | `				/* Invoke the supplied callback */` |
|      27 | 7030 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 7031 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 7032 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 7033 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7034 | `					return PH7_EXCEPTION;` |
|       - | 7035 | `				}` |
|       - | 7036 | `			}` |
|      18 | 7037 | `		}` |
|       - | 7038 | `		/* Point to the next entry */` |
|      37 | 7039 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 7040 | `	}` |
|      23 | 7041 | `	return PH7_OK;` |
|      12 | 7042 |  |
|       - | 7043 | `/*` |
|       - | 7044 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 7045 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 7046 | ` * Parameters` |
|       - | 7047 | ` *  $array` |
|       - | 7048 | ` *   The input array.` |
|       - | 7049 | ` *  $funcname` |
|       - | 7050 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 7051 | ` *   the first, and the key/index second.` |
|       - | 7052 | ` * Note:` |
|       - | 7053 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 7054 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 7055 | ` *  be made in the original array itself.` |
|       - | 7056 | ` *  $userdata` |
|       - | 7057 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 7058 | ` *   to the callback funcname.` |
|       - | 7059 | ` * Return` |
|       - | 7060 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 7061 | ` */` |
|      30 | 7062 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7063 |  |
|       - | 7064 | `	ph7_hashmap *pMap;` |
|      35 | 7065 | `	if( nArg < 2 ){` |
|       8 | 7066 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7067 | `			"ArgumentCountError",` |
|       - | 7068 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 7069 | `			nArg` |
|       - | 7070 | `			);` |
|       - | 7071 | `	}` |
|      31 | 7072 | `	if( nArg > 3 ){` |
|       4 | 7073 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7074 | `			"ArgumentCountError",` |
|       - | 7075 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 7076 | `			nArg` |
|       - | 7077 | `			);` |
|       - | 7078 | `	}` |
|      29 | 7079 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7080 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7081 | `			"TypeError",` |
|       - | 7082 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7083 | `			ph7_type_name(apArg[0])` |
|       - | 7084 | `			);` |
|       - | 7085 | `	}` |
|      27 | 7086 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 7087 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7088 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7089 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7090 | `				"TypeError",` |
|       - | 7091 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7092 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7093 | `				zFunc` |
|       - | 7094 | `				);` |
|       - | 7095 | `		}` |
|      12 | 7096 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 7097 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7098 | `				"TypeError",` |
|       - | 7099 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7100 | `				"array callback must have exactly two members"` |
|       - | 7101 | `				);` |
|       - | 7102 | `		}` |
|       6 | 7103 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7104 | `			"TypeError",` |
|       - | 7105 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7106 | `			"no array or string given"` |
|       - | 7107 | `			);` |
|       - | 7108 | `	}` |
|       - | 7109 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 7110 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 7111 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7112 | `	/* Perform the desired operation */` |
|      13 | 7113 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 7114 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7115 | `		return PH7_EXCEPTION;` |
|       - | 7116 | `	}` |
|       - | 7117 | `	/* All done, return TRUE */` |
|      13 | 7118 | `	ph7_result_bool(pCtx,1);` |
|      13 | 7119 | `	return PH7_OK;` |
|      20 | 7120 |  |
|       - | 7121 | `/*` |
|       - | 7122 | ` * bool array_is_list(array $array)` |
|       - | 7123 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 7124 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 7125 | ` * Return` |
|       - | 7126 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 7127 | ` */` |
|       - | 7128 | `/*` |
|       - | 7129 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 7130 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 7131 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 7132 | ` */` |
|     114 | 7133 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       1 | 7134 |  |
|     115 | 7135 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|     115 | 7136 | `	sxi64 iExpect = 0;` |
|       - | 7137 | `	sxu32 n;` |
|     233 | 7138 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     169 | 7139 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 7140 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|      51 | 7141 | `			return 0;` |
|       - | 7142 | `		}` |
|     119 | 7143 | `		++iExpect;` |
|     119 | 7144 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      60 | 7145 | `	}` |
|      65 | 7146 | `	return 1;` |
|      58 | 7147 |  |
|      12 | 7148 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7149 |  |
|      13 | 7150 | `	if( nArg < 1 ){` |
|     ! 0 | 7151 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7152 | `			"ArgumentCountError",` |
|       - | 7153 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 7154 | `			);` |
|       - | 7155 | `	}` |
|      13 | 7156 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7157 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7158 | `			"TypeError",` |
|       - | 7159 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7160 | `			ph7_type_name(apArg[0])` |
|       - | 7161 | `			);` |
|       - | 7162 | `	}` |
|      13 | 7163 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 7164 | `	return PH7_OK;` |
|       7 | 7165 |  |
|       - | 7166 | `/*` |
|       - | 7167 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 7168 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 7169 | ` * array_column() for both the column value and the index key.` |
|       - | 7170 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 7171 | ` * container or the key is absent.` |
|       - | 7172 | ` */` |
|      32 | 7173 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 7174 |  |
|      33 | 7175 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 7176 | `		ph7_hashmap_node *pNode;` |
|      25 | 7177 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 7178 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 7179 | `		}` |
|      11 | 7180 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 7181 | `		ph7_value sName;` |
|       - | 7182 | `		const char *zName;` |
|       - | 7183 | `		ph7_value *pAttr;` |
|       - | 7184 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 7185 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 7186 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 7187 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 7188 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 7189 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 7190 | `		PH7_MemObjRelease(&sName);` |
|       9 | 7191 | `		return pAttr;` |
|       - | 7192 | `	}` |
|       5 | 7193 | `	return 0;` |
|      17 | 7194 |  |
|       - | 7195 | `/*` |
|       - | 7196 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 7197 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 7198 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 7199 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 7200 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 7201 | ` *  Each row may be an array or an object.` |
|       - | 7202 | ` */` |
|      12 | 7203 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7204 |  |
|       - | 7205 | `	ph7_hashmap_node *pNode;` |
|       - | 7206 | `	ph7_hashmap *pMap;` |
|       - | 7207 | `	ph7_value *pArray;` |
|       - | 7208 | `	ph7_value *pRow;` |
|       - | 7209 | `	ph7_value *pCol;` |
|       - | 7210 | `	ph7_value *pIdx;` |
|       - | 7211 | `	int bWantCol;` |
|       - | 7212 | `	int bWantIdx;` |
|       - | 7213 | `	sxu32 n;` |
|      13 | 7214 | `	if( nArg < 2 ){` |
|     ! 0 | 7215 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7216 | `			"ArgumentCountError",` |
|       - | 7217 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 7218 | `			nArg` |
|       - | 7219 | `			);` |
|       - | 7220 | `	}` |
|      13 | 7221 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7222 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7223 | `			"TypeError",` |
|       - | 7224 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7225 | `			ph7_type_name(apArg[0])` |
|       - | 7226 | `			);` |
|       - | 7227 | `	}` |
|      13 | 7228 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 7229 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 7230 | `	if( pArray == 0 ){` |
|     ! 0 | 7231 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7232 | `		return PH7_OK;` |
|       - | 7233 | `	}` |
|       - | 7234 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 7235 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 7236 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 7237 | `	pNode = pMap->pFirst;` |
|      33 | 7238 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 7239 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 7240 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 7241 | `		if( pRow == 0 ){` |
|     ! 0 | 7242 | `			continue;` |
|       - | 7243 | `		}` |
|      21 | 7244 | `		if( bWantCol ){` |
|      19 | 7245 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 7246 | `			if( pCol == 0 ){` |
|       - | 7247 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 7248 | `				continue;` |
|       - | 7249 | `			}` |
|       9 | 7250 | `		}else{` |
|       3 | 7251 | `			pCol = pRow;` |
|       - | 7252 | `		}` |
|      19 | 7253 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 7254 | `		if( pIdx ){` |
|      13 | 7255 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 7256 | `		}else{` |
|       7 | 7257 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 7258 | `		}` |
|      10 | 7259 | `	}` |
|      13 | 7260 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 7261 | `	return PH7_OK;` |
|       7 | 7262 |  |
|       - | 7263 | `/*` |
|       - | 7264 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 7265 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 7266 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 7267 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 7268 | ` */` |
|      28 | 7269 | `static sxi32 HashmapCallbackSearch(` |
|       - | 7270 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 7271 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 7272 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 7273 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 7274 | `	)` |
|       1 | 7275 |  |
|       - | 7276 | `	ph7_hashmap_node *pEntry;` |
|       - | 7277 | `	ph7_hashmap *pMap;` |
|       - | 7278 | `	ph7_value *pValue;` |
|       - | 7279 | `	ph7_value *apCbArg[2];` |
|       - | 7280 | `	ph7_value sKey;` |
|       - | 7281 | `	ph7_value sResult;` |
|       - | 7282 | `	sxi32 rc;` |
|       - | 7283 | `	sxu32 n;` |
|      29 | 7284 | `	*ppMatch = 0;` |
|      29 | 7285 | `	if( nArg < 2 ){` |
|     ! 0 | 7286 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7287 | `			"ArgumentCountError",` |
|       - | 7288 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 7289 | `			zName,nArg` |
|       - | 7290 | `			);` |
|       - | 7291 | `	}` |
|      29 | 7292 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7293 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7294 | `			"TypeError",` |
|       - | 7295 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7296 | `			zName,ph7_type_name(apArg[0])` |
|       - | 7297 | `			);` |
|       - | 7298 | `	}` |
|      29 | 7299 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7300 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7301 | `			"TypeError",` |
|       - | 7302 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 7303 | `			zName,ph7_type_name(apArg[1])` |
|       - | 7304 | `			);` |
|       - | 7305 | `	}` |
|      29 | 7306 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 7307 | `	pEntry = pMap->pFirst;` |
|      29 | 7308 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 7309 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 7310 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 7311 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 7312 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 7313 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 7314 | `		if( pValue ){` |
|       - | 7315 | `			/* The callback receives ($value, $key). */` |
|      59 | 7316 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 7317 | `			apCbArg[0] = pValue;` |
|      59 | 7318 | `			apCbArg[1] = &sKey;` |
|      59 | 7319 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 7320 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 7321 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7322 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7323 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7324 | `				return PH7_EXCEPTION;` |
|       - | 7325 | `			}` |
|      59 | 7326 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 7327 | `				*ppMatch = pEntry;` |
|      15 | 7328 | `				break;` |
|       - | 7329 | `			}` |
|      22 | 7330 | `		}` |
|      45 | 7331 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 7332 | `	}` |
|      29 | 7333 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 7334 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 7335 | `	return PH7_OK;` |
|      15 | 7336 |  |
|       - | 7337 | `/*` |
|       - | 7338 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 7339 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 7340 | ` *  is truthy, or NULL if none match.` |
|       - | 7341 | ` */` |
|       6 | 7342 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7343 |  |
|       - | 7344 | `	ph7_hashmap_node *pMatch;` |
|       - | 7345 | `	ph7_value *pVal;` |
|       - | 7346 | `	sxi32 rc;` |
|       7 | 7347 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 7348 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7349 | `		return rc;` |
|       - | 7350 | `	}` |
|       7 | 7351 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 7352 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 7353 | `	}else{` |
|       3 | 7354 | `		ph7_result_null(pCtx);` |
|       - | 7355 | `	}` |
|       7 | 7356 | `	return PH7_OK;` |
|       4 | 7357 |  |
|       - | 7358 | `/*` |
|       - | 7359 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 7360 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 7361 | ` *  is truthy, or NULL if none match.` |
|       - | 7362 | ` */` |
|       6 | 7363 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7364 |  |
|       - | 7365 | `	ph7_hashmap_node *pMatch;` |
|       - | 7366 | `	sxi32 rc;` |
|       7 | 7367 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 7368 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7369 | `		return rc;` |
|       - | 7370 | `	}` |
|       7 | 7371 | `	if( pMatch == 0 ){` |
|       3 | 7372 | `		ph7_result_null(pCtx);` |
|       6 | 7373 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 7374 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 7375 | `	}else{` |
|       4 | 7376 | `		ph7_result_string(pCtx,` |
|       2 | 7377 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 7378 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 7379 | `	}` |
|       7 | 7380 | `	return PH7_OK;` |
|       4 | 7381 |  |
|       - | 7382 | `/*` |
|       - | 7383 | ` * bool array_any(array $array, callable $callback)` |
|       - | 7384 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 7385 | ` *  FALSE for an empty array.` |
|       - | 7386 | ` */` |
|       8 | 7387 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7388 |  |
|       - | 7389 | `	ph7_hashmap_node *pMatch;` |
|       - | 7390 | `	sxi32 rc;` |
|       9 | 7391 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 7392 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7393 | `		return rc;` |
|       - | 7394 | `	}` |
|       9 | 7395 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 7396 | `	return PH7_OK;` |
|       5 | 7397 |  |
|       - | 7398 | `/*` |
|       - | 7399 | ` * bool array_all(array $array, callable $callback)` |
|       - | 7400 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 7401 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 7402 | ` */` |
|       8 | 7403 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7404 |  |
|       - | 7405 | `	ph7_hashmap_node *pMatch;` |
|       - | 7406 | `	sxi32 rc;` |
|       9 | 7407 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 7408 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7409 | `		return rc;` |
|       - | 7410 | `	}` |
|       9 | 7411 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 7412 | `	return PH7_OK;` |
|       5 | 7413 |  |
|       - | 7414 | `/*` |
|       - | 7415 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 7416 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 7417 | ` */` |
|       - | 7418 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 7419 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|      70 | 7420 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       5 | 7421 |  |
|      75 | 7422 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|      35 | 7423 | `	(void)pVm;` |
|      75 | 7424 | `	p->nCount++;` |
|      75 | 7425 | `	if( p->pArray ){` |
|       - | 7426 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 7427 | `		 * otherwise append with an auto-assigned int index. */` |
|      67 | 7428 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|      31 | 7429 | `	}` |
|      75 | 7430 | `	return SXRET_OK;` |
|       5 | 7431 |  |
|       - | 7432 | `/*` |
|       - | 7433 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 7434 | ` */` |
|      26 | 7435 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 7436 |  |
|       - | 7437 | `	struct IterCollect sCol;` |
|       - | 7438 | `	ph7_value *pArray;` |
|       - | 7439 | `	sxi32 rc;` |
|      31 | 7440 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      31 | 7441 | `	pArray = ph7_context_new_array(pCtx);` |
|      31 | 7442 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      31 | 7443 | `	sCol.pArray = pArray;` |
|      31 | 7444 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|      31 | 7445 | `	sCol.nCount = 0;` |
|      31 | 7446 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 7447 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 7448 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 7449 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7450 | `		sxu32 n;` |
|       9 | 7451 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7452 | `			ph7_value sKey, *pVal;` |
|       7 | 7453 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 7454 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 7455 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 7456 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 7457 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 7458 | `			pEntry = pEntry->pPrev;` |
|       4 | 7459 | `		}` |
|       3 | 7460 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 7461 | `		return PH7_OK;` |
|       - | 7462 | `	}` |
|      29 | 7463 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      29 | 7464 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      27 | 7465 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7466 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7467 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 7468 | `			ph7_type_name(apArg[0]));` |
|       - | 7469 | `	}` |
|      27 | 7470 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 7471 | `	return PH7_OK;` |
|      18 | 7472 |  |
|       - | 7473 | `/*` |
|       - | 7474 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 7475 | ` */` |
|       6 | 7476 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 7477 |  |
|       - | 7478 | `	struct IterCollect sCol;` |
|       - | 7479 | `	sxi32 rc;` |
|       7 | 7480 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       7 | 7481 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 7482 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 7483 | `		return PH7_OK;` |
|       - | 7484 | `	}` |
|       5 | 7485 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|       5 | 7486 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|       5 | 7487 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       5 | 7488 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7489 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7490 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 7491 | `			ph7_type_name(apArg[0]));` |
|       - | 7492 | `	}` |
|       5 | 7493 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|       5 | 7494 | `	return PH7_OK;` |
|       4 | 7495 |  |
|       - | 7496 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 7497 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 7498 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 7499 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      24 | 7500 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       1 | 7501 |  |
|      25 | 7502 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 7503 | `	ph7_value sResult;` |
|       - | 7504 | `	SySet aArg;` |
|       - | 7505 | `	sxi32 rc;` |
|       - | 7506 | `	int bContinue;` |
|      12 | 7507 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      25 | 7508 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      25 | 7509 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 7510 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|       9 | 7511 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7512 | `		sxu32 n;` |
|      17 | 7513 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       9 | 7514 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       9 | 7515 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|       9 | 7516 | `			pEntry = pEntry->pPrev;` |
|       5 | 7517 | `		}` |
|       4 | 7518 | `	}` |
|      25 | 7519 | `	PH7_MemObjInit(pVm,&sResult);` |
|      37 | 7520 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      24 | 7521 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|      25 | 7522 | `	SySetRelease(&aArg);` |
|      25 | 7523 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      23 | 7524 | `	p->nCount++;` |
|      23 | 7525 | `	PH7_MemObjToBool(&sResult);` |
|      23 | 7526 | `	bContinue = (sResult.x.iVal != 0);` |
|      23 | 7527 | `	PH7_MemObjRelease(&sResult);` |
|      23 | 7528 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      13 | 7529 |  |
|       - | 7530 | `/*` |
|       - | 7531 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 7532 | ` */` |
|       8 | 7533 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 7534 |  |
|       - | 7535 | `	struct IterApply sApp;` |
|       - | 7536 | `	sxi32 rc;` |
|       9 | 7537 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       9 | 7538 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7539 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7540 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|       - | 7541 | `	}` |
|       9 | 7542 | `	sApp.pCallback = apArg[1];` |
|       9 | 7543 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|       9 | 7544 | `	sApp.nCount = 0;` |
|       9 | 7545 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|       9 | 7546 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       7 | 7547 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7548 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7549 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 7550 | `			ph7_type_name(apArg[0]));` |
|       - | 7551 | `	}` |
|       7 | 7552 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|       7 | 7553 | `	return PH7_OK;` |
|       5 | 7554 |  |
|       - | 7555 | `/*` |
|       - | 7556 | ` * Table of hashmap functions.` |
|       - | 7557 | ` */` |
|       - | 7558 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 7559 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|       - | 7560 | `	{"iterator_count",     ph7_iterator_count },` |
|       - | 7561 | `	{"iterator_apply",     ph7_iterator_apply },` |
|       - | 7562 | `	{"count",             ph7_hashmap_count },` |
|       - | 7563 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 7564 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 7565 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 7566 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 7567 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 7568 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 7569 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 7570 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 7571 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 7572 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 7573 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 7574 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 7575 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 7576 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 7577 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 7578 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 7579 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 7580 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 7581 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 7582 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 7583 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 7584 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 7585 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 7586 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 7587 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 7588 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 7589 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 7590 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 7591 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 7592 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 7593 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 7594 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 7595 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 7596 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 7597 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 7598 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 7599 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 7600 | `	{"array_column",      ph7_hashmap_column  },` |
|       - | 7601 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|       - | 7602 | `	{"array_find",        ph7_hashmap_find    },` |
|       - | 7603 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|       - | 7604 | `	{"array_any",         ph7_hashmap_any     },` |
|       - | 7605 | `	{"array_all",         ph7_hashmap_all     },` |
|       - | 7606 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 7607 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 7608 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 7609 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 7610 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 7611 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 7612 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 7613 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 7614 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 7615 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 7616 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 7617 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 7618 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 7619 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 7620 | `	{"range",             ph7_hashmap_range   },` |
|       - | 7621 | `	{"current",           ph7_hashmap_current },` |
|       - | 7622 | `	{"each",              ph7_hashmap_each    },` |
|       - | 7623 | `	{"pos",               ph7_hashmap_current },` |
|       - | 7624 | `	{"next",              ph7_hashmap_next    },` |
|       - | 7625 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 7626 | `	{"end",               ph7_hashmap_end     },` |
|       - | 7627 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 7628 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 7629 | `};` |
|       - | 7630 | `/*` |
|       - | 7631 | ` * Register the built-in hashmap functions defined above.` |
|       - | 7632 | ` */` |
|    3216 | 7633 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       5 | 7634 |  |
|       - | 7635 | `	sxu32 n;` |
|  228341 | 7636 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  225125 | 7637 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|  112565 | 7638 | `	}` |
|    3221 | 7639 |  |
|       - | 7640 | `/*` |
|       - | 7641 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 7642 | ` * the BLOB given as the first argument.` |
|       - | 7643 | ` * This function is typically invoked when the user issue a call to` |
|       - | 7644 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 7645 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 7646 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 7647 | ` */` |
|       - | 7648 | `/*` |
|       - | 7649 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|       - | 7650 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|       - | 7651 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|       - | 7652 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|       - | 7653 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|       - | 7654 | ` */` |
|      28 | 7655 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       3 | 7656 |  |
|      31 | 7657 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7658 | `	ph7_value *pObj;` |
|      31 | 7659 | `	sxu32 n = 0;` |
|       - | 7660 | `	int isRef;` |
|      31 | 7661 | `	sxi32 rc = SXRET_OK;` |
|       - | 7662 | `	int i;` |
|      65 | 7663 | `	for(;;){` |
|     133 | 7664 | `		if( n >= pMap->nEntry ){` |
|      31 | 7665 | `			break;` |
|       - | 7666 | `		}` |
|     207 | 7667 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     105 | 7668 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      54 | 7669 | `		}` |
|       - | 7670 | `		/* Dump key */` |
|     105 | 7671 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 7672 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 7673 | `		}else{` |
|     108 | 7674 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      35 | 7675 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 7676 | `		}` |
|       - | 7677 | `#ifdef __WINNT__` |
|       3 | 7678 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7679 | `#else` |
|     102 | 7680 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7681 | `#endif` |
|       - | 7682 | `		/* Dump node value */` |
|     105 | 7683 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     105 | 7684 | `		isRef = 0;` |
|     105 | 7685 | `		if( pObj ){` |
|     105 | 7686 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 7687 | `				/* Referenced object */` |
|     ! 0 | 7688 | `				isRef = 1;` |
|     ! 0 | 7689 | `			}` |
|     105 | 7690 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     105 | 7691 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 7692 | `				break;` |
|       - | 7693 | `			}` |
|      51 | 7694 | `		}` |
|       - | 7695 | `		/* Point to the next entry */` |
|     105 | 7696 | `		n++;` |
|     105 | 7697 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       3 | 7698 | `	}` |
|      31 | 7699 | `	return rc;` |
|       3 | 7700 |  |
|      24 | 7701 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 7702 |  |
|       - | 7703 | `	sxi32 rc;` |
|       - | 7704 | `	int i;` |
|      26 | 7705 | `	if( nDepth > 31 ){` |
|       - | 7706 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 7707 | `		/* Nesting limit reached */` |
|     ! 0 | 7708 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 7709 | `		if( ShowType ){` |
|     ! 0 | 7710 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 7711 | `		}` |
|     ! 0 | 7712 | `		return SXERR_LIMIT;` |
|       - | 7713 | `	}` |
|      26 | 7714 | `	if( !ShowType ){` |
|      13 | 7715 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       6 | 7716 | `	}` |
|       - | 7717 | `	/* Total entries */` |
|      26 | 7718 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 7719 | `#ifdef __WINNT__` |
|       2 | 7720 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7721 | `#else` |
|      24 | 7722 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7723 | `#endif` |
|      26 | 7724 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|      50 | 7725 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      26 | 7726 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      14 | 7727 | `	}` |
|      26 | 7728 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      26 | 7729 | `	return rc;` |
|      14 | 7730 |  |
|       - | 7731 | `/*` |
|       - | 7732 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 7733 | ` * retrieved entry.` |
|       - | 7734 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 7735 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 7736 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 7737 | ` * a value different from PH7_OK.` |
|       - | 7738 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 7739 | ` */` |
|   32404 | 7740 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 7741 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 7742 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 7743 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 7744 | `	)` |
|       5 | 7745 |  |
|       - | 7746 | `	ph7_hashmap_node *pEntry;` |
|       - | 7747 | `	ph7_value sKey,sValue;` |
|       - | 7748 | `	sxi32 rc;` |
|       - | 7749 | `	sxu32 n;` |
|       - | 7750 | `	/* Initialize walker parameter */` |
|   32409 | 7751 | `	rc = SXRET_OK;` |
|   32409 | 7752 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   32409 | 7753 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   32409 | 7754 | `	n = pMap->nEntry;` |
|   32409 | 7755 | `	pEntry = pMap->pFirst;` |
|       - | 7756 | `	/* Start the iteration process */` |
|   81339 | 7757 | `	for(;;){` |
|  162683 | 7758 | `		if( n < 1 ){` |
|   32409 | 7759 | `			break;` |
|       - | 7760 | `		}` |
|       - | 7761 | `		/* Extract a copy of the key and a copy the current value */` |
|  130279 | 7762 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  130279 | 7763 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 7764 | `		/* Invoke the user callback */` |
|  130279 | 7765 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 7766 | `		/* Release the copy of the key and the value */` |
|  130279 | 7767 | `		PH7_MemObjRelease(&sKey);` |
|  130279 | 7768 | `		PH7_MemObjRelease(&sValue);` |
|  130279 | 7769 | `		if( rc != PH7_OK ){` |
|       - | 7770 | `			/* Callback request an operation abort */` |
|     ! 0 | 7771 | `			return SXERR_ABORT;` |
|       - | 7772 | `		}` |
|       - | 7773 | `		/* Point to the next entry */` |
|  130279 | 7774 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  130279 | 7775 | `		n--;` |
|       5 | 7776 | `	}` |
|       - | 7777 | `	/* All done */` |
|   32409 | 7778 | `	return SXRET_OK;` |
|   16207 | 7779 |  |
|       - | 7780 |  |
