# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3356/3846 lines (87.26%)

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
|       - |    8 | `/* HASHMAP_INT_NODE / HASHMAP_BLOB_NODE (node key types) are declared in ph7int.h` |
|       - |    9 | ` * alongside ph7_hashmap_node so name-forwarding builtins can classify keys. */` |
|       - |   10 | `/* Node control flags */` |
|       - |   11 | `#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node hold a reference to a foreign ph7_value` |
|       - |   12 | `                                        * [i.e: array(&var)/$a[] =& $var ]` |
|       - |   13 | `										*/` |
|       - |   14 | `/*` |
|       - |   15 | ` * Default hash function for int [i.e; 64-bit integer] keys.` |
|       - |   16 | ` */` |
| 3129614 |   17 | `static sxu32 IntHash(sxi64 iKey)` |
|       5 |   18 | `{` |
| 3129619 |   19 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
| 3129619 |   20 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|       5 |   21 | `}` |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  402936 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       5 |   26 | `{` |
|  402941 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  402941 |   29 | `	sxu32 nH = 5381;` |
|  402941 |   30 | `	zEnd = &zIn[nLen];` |
|  471377 |   31 | `	for(;;){` |
|  942759 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  818023 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  740519 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  631797 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       5 |   36 | `	}` |
|  402941 |   37 | `	return nH;` |
|       5 |   38 | `}` |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     946 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       5 |   47 | `{` |
|     951 |   48 | `	sxi64 iCount = 0;` |
|     951 |   49 | `	if( !bRecursive ){` |
|     777 |   50 | `		iCount = pMap->nEntry;` |
|     391 |   51 | `	}else{` |
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
|     951 |   86 | `	return iCount;` |
|       5 |   87 | `}` |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 3068504 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       5 |   94 | `{` |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 3068509 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 3068509 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 3068509 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 3068509 |  104 | `	pNode->pMap  = &(*pMap);` |
| 3068509 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 3068509 |  106 | `	pNode->nHash = nHash;` |
| 3068509 |  107 | `	pNode->xKey.iKey = iKey;` |
| 3068509 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 3068509 |  109 | `	return pNode;` |
| 1534257 |  110 | `}` |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|  151228 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       5 |  117 | `{` |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|  151233 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  151233 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|  151233 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|  151233 |  127 | `	pNode->pMap  = &(*pMap);` |
|  151233 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  151233 |  129 | `	pNode->nHash = nHash;` |
|  151233 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  151233 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  151233 |  132 | `	pNode->nValIdx = nValIdx;` |
|  151233 |  133 | `	return pNode;` |
|   75619 |  134 | `}` |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 3219732 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       5 |  139 | `{` |
|       - |  140 | `	/* Link */` |
| 3219737 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2847755 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2847755 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1423875 |  144 | `	}` |
| 3219737 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 3219737 |  147 | `	if( pMap->pFirst == 0 ){` |
|   73519 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   73519 |  150 | `		pMap->pCur = pNode;` |
|   36762 |  151 | `	}else{` |
| 3146223 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 3219737 |  154 | `	++pMap->nEntry;` |
| 3219737 |  155 | `}` |
|       - |  156 | `/*` |
|       - |  157 | ` * Unlink a node from the hashmap.` |
|       - |  158 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  159 | ` */` |
|    7348 |  160 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       5 |  161 | `{` |
|    7353 |  162 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    7353 |  163 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  164 | `	/* Unlink from the corresponding bucket */` |
|    7353 |  165 | `	if( pNode->pPrevCollide == 0 ){` |
|    6915 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3460 |  167 | `	}else{` |
|     440 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    7353 |  170 | `	if( pNode->pNextCollide ){` |
|    5548 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2773 |  172 | `	}` |
|    7353 |  173 | `	if( pMap->pFirst == pNode ){` |
|     131 |  174 | `		pMap->pFirst = pNode->pPrev;` |
|      63 |  175 | `	}` |
|    7353 |  176 | `	if( pMap->pCur == pNode ){` |
|       - |  177 | `		/* Advance the node cursor */` |
|     133 |  178 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      64 |  179 | `	}` |
|       - |  180 | `	/* Unlink from the map list */` |
|    7353 |  181 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    7353 |  182 | `	if( bRestore ){` |
|       - |  183 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     107 |  184 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  185 | `		/* Restore to the freelist */` |
|     107 |  186 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     107 |  187 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  188 | `		}` |
|      51 |  189 | `	}` |
|    7353 |  190 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    7218 |  191 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3607 |  192 | `	}` |
|    7353 |  193 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    7353 |  194 | `	pMap->nEntry--;` |
|    7353 |  195 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  196 | `		/* Free the hash-bucket */` |
|      75 |  197 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      75 |  198 | `		pMap->apBucket = 0;` |
|      75 |  199 | `		pMap->nSize = 0;` |
|      75 |  200 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      35 |  201 | `	}` |
|    7353 |  202 | `}` |
|       - |  203 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  204 | `/*` |
|       - |  205 | ` * Grow the hash-table and rehash all entries.` |
|       - |  206 | ` */` |
| 3219732 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       5 |  208 | `{` |
| 3219737 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   78159 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   78159 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   78159 |  215 | `		if( nNew < 1 ){` |
|   73519 |  216 | `			nNew = 16;` |
|   36757 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   78159 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   78159 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   78159 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   78159 |  230 | `		pMap->apBucket = apNew;` |
|   78159 |  231 | `		pMap->nSize = nNew;` |
|   78159 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   73519 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    4645 |  237 | `		pEntry = pMap->pFirst;` |
|    4645 |  238 | `		n = 0;` |
| 2075680 |  239 | `		for( ;; ){` |
| 4151365 |  240 | `			if( n >= pMap->nEntry ){` |
|    4645 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 4146725 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 4146725 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4146725 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3560351 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3560351 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1780173 |  250 | `			}` |
| 4146725 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 4146725 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4146725 |  254 | `			n++;` |
|       5 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    4645 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2320 |  258 | `	}` |
| 3146223 |  259 | `	return SXRET_OK;` |
| 1609871 |  260 | `}` |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 3068504 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  266 | `{` |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 3068509 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		ph7_value sSafeVal;` |
|       - |  274 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  275 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  276 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  277 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  278 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  279 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
| 3068473 |  280 | `		if( pValue ){` |
| 3068471 |  281 | `			sSafeVal = *pValue;` |
| 3068471 |  282 | `			pValue = &sSafeVal;` |
| 1534233 |  283 | `		}` |
|       - |  284 | `		/* Reserve a ph7_value for the value */` |
| 3068473 |  285 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 3068473 |  286 | `		if( pObj == 0 ){` |
|     ! 0 |  287 | `			return SXERR_MEM;` |
|       - |  288 | `		}` |
| 3068473 |  289 | `		if( pValue ){` |
|       - |  290 | `			/* Duplicate the value */` |
| 3068471 |  291 | `			PH7_MemObjStore(pValue,pObj);` |
| 1534233 |  292 | `		}` |
| 3068473 |  293 | `		nIdx = pObj->nIdx;` |
| 1534239 |  294 | `	}else{` |
|      37 |  295 | `		nIdx = nRefIdx;` |
|       - |  296 | `	}` |
|       - |  297 | `	/* Hash the key */` |
| 3068509 |  298 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  299 | `	/* Allocate a new int node */` |
| 3068509 |  300 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 3068509 |  301 | `	if( pNode == 0 ){` |
|     ! 0 |  302 | `		return SXERR_MEM;` |
|       - |  303 | `	}` |
| 3068509 |  304 | `	if( isForeign ){` |
|       - |  305 | `		/* Mark as a foregin entry */` |
|      37 |  306 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      18 |  307 | `	}` |
|       - |  308 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 3068509 |  309 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 3068509 |  310 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  311 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  312 | `		return rc;` |
|       - |  313 | `	}` |
|       - |  314 | `	/* Perform the insertion */` |
| 3068509 |  315 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  316 | `	/* Install in the reference table */` |
| 3068509 |  317 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  318 | `	/* All done */` |
| 3068509 |  319 | `	return SXRET_OK;` |
| 1534257 |  320 | `}` |
|       - |  321 | `/*` |
|       - |  322 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  323 | ` * hashmap.` |
|       - |  324 | ` */` |
|  151228 |  325 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  326 | `{` |
|       - |  327 | `	ph7_hashmap_node *pNode;` |
|       - |  328 | `	sxu32 nHash;` |
|       - |  329 | `	sxu32 nIdx;` |
|       - |  330 | `	sxi32 rc;` |
|  151233 |  331 | `	if( !isForeign ){` |
|       - |  332 | `		ph7_value *pObj;` |
|       - |  333 | `		ph7_value sSafeVal;` |
|       - |  334 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  335 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  336 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  337 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  338 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  339 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|  105275 |  340 | `		if( pValue ){` |
|  104985 |  341 | `			sSafeVal = *pValue;` |
|  104985 |  342 | `			pValue = &sSafeVal;` |
|   52490 |  343 | `		}` |
|       - |  344 | `		/* Reserve a ph7_value for the value */` |
|  105275 |  345 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|  105275 |  346 | `		if( pObj == 0 ){` |
|     ! 0 |  347 | `			return SXERR_MEM;` |
|       - |  348 | `		}` |
|  105275 |  349 | `		if( pValue ){` |
|       - |  350 | `			/* Duplicate the value */` |
|  104985 |  351 | `			PH7_MemObjStore(pValue,pObj);` |
|   52490 |  352 | `		}` |
|  105275 |  353 | `		nIdx = pObj->nIdx;` |
|   52640 |  354 | `	}else{` |
|   45963 |  355 | `		nIdx = nRefIdx;` |
|       - |  356 | `	}` |
|       - |  357 | `	/* Hash the key */` |
|  151233 |  358 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  359 | `	/* Allocate a new blob node */` |
|  151233 |  360 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  151233 |  361 | `	if( pNode == 0 ){` |
|     ! 0 |  362 | `		return SXERR_MEM;` |
|       - |  363 | `	}` |
|  151233 |  364 | `	if( isForeign ){` |
|       - |  365 | `		/* Mark as a foregin entry */` |
|   45963 |  366 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   22979 |  367 | `	}` |
|       - |  368 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  151233 |  369 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  151233 |  370 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  371 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  372 | `		return rc;` |
|       - |  373 | `	}` |
|       - |  374 | `	/* Perform the insertion */` |
|  151233 |  375 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  376 | `	/* Install in the reference table */` |
|  151233 |  377 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  378 | `	/* All done */` |
|  151233 |  379 | `	return SXRET_OK;` |
|   75619 |  380 | `}` |
|       - |  381 | `/*` |
|       - |  382 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  383 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  384 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  385 | ` */` |
|   48554 |  386 | `static sxi32 HashmapLookupIntKey(` |
|       - |  387 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  388 | `	sxi64 iKey,                /* lookup key */` |
|       - |  389 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  390 | `	)` |
|       5 |  391 | `{` |
|       - |  392 | `	ph7_hashmap_node *pNode;` |
|       - |  393 | `	sxu32 nHash;` |
|   48559 |  394 | `	if( pMap->nEntry < 1 ){` |
|       - |  395 | `		/* Don't bother hashing,there is no entry anyway */` |
|     555 |  396 | `		return SXERR_NOTFOUND;` |
|       - |  397 | `	}` |
|       - |  398 | `	/* Hash the key first */` |
|   48009 |  399 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  400 | `	/* Point to the appropriate bucket */` |
|   48009 |  401 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  402 | `	/* Perform the lookup */` |
|  412359 |  403 | `	for(;;){` |
|  824723 |  404 | `		if( pNode == 0 ){` |
|   46305 |  405 | `			break;` |
|       - |  406 | `		}` |
|  778418 |  407 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  775402 |  408 | `			&& pNode->nHash == nHash` |
|  387050 |  409 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  410 | `				/* Node found */` |
|    1709 |  411 | `				if( ppNode ){` |
|    1691 |  412 | `					*ppNode = pNode;` |
|     843 |  413 | `				}` |
|    1709 |  414 | `				return SXRET_OK;` |
|       - |  415 | `		}` |
|       - |  416 | `		/* Follow the collision link */` |
|  776715 |  417 | `		pNode = pNode->pNextCollide;` |
|       1 |  418 | `	}` |
|       - |  419 | `	/* No such entry */` |
|   46305 |  420 | `	return SXERR_NOTFOUND;` |
|   24282 |  421 | `}` |
|       - |  422 | `/*` |
|       - |  423 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  424 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  425 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  426 | ` */` |
|  275536 |  427 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  428 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  429 | `	const void *pKey,           /* Lookup key */` |
|       - |  430 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  431 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  432 | `	)` |
|       5 |  433 | `{` |
|       - |  434 | `	ph7_hashmap_node *pNode;` |
|       - |  435 | `	sxu32 nHash;` |
|  275541 |  436 | `	if( pMap->nEntry < 1 ){` |
|       - |  437 | `		/* Don't bother hashing,there is no entry anyway */` |
|   23833 |  438 | `		return SXERR_NOTFOUND;` |
|       - |  439 | `	}` |
|       - |  440 | `	/* Hash the key first */` |
|  251713 |  441 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  442 | `	/* Point to the appropriate bucket */` |
|  251713 |  443 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  444 | `	/* Perform the lookup */` |
|  215164 |  445 | `	for(;;){` |
|  430333 |  446 | `		if( pNode == 0 ){` |
|  198791 |  447 | `			break;` |
|       - |  448 | `		}` |
|  231542 |  449 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  230039 |  450 | `			&& pNode->nHash == nHash` |
|  140775 |  451 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   53019 |  452 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  453 | `				/* Node found */` |
|   52927 |  454 | `				if( ppNode ){` |
|   52899 |  455 | `					*ppNode = pNode;` |
|   26447 |  456 | `				}` |
|   52927 |  457 | `				return SXRET_OK;` |
|       - |  458 | `		}` |
|       - |  459 | `		/* Follow the collision link */` |
|  178625 |  460 | `		pNode = pNode->pNextCollide;` |
|       5 |  461 | `	}` |
|       - |  462 | `	/* No such entry */` |
|  198791 |  463 | `	return SXERR_NOTFOUND;` |
|  137773 |  464 | `}` |
|       - |  465 | `/*` |
|       - |  466 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  467 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  468 | ` */` |
|  275660 |  469 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       5 |  470 | `{` |
|  275665 |  471 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  275665 |  472 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  275665 |  473 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  474 | `		/* Octal not decimal number */` |
|       5 |  475 | `		return FALSE;` |
|       - |  476 | `	}` |
|  275661 |  477 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  478 | `		zIn++;` |
|     ! 0 |  479 | `	}` |
|  138161 |  480 | `	for(;;){` |
|  276327 |  481 | `		if( zIn >= zEnd ){` |
|     233 |  482 | `			return TRUE;` |
|       - |  483 | `		}` |
|  276095 |  484 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  137717 |  485 | `			break;` |
|       - |  486 | `		}` |
|     667 |  487 | `		zIn++;` |
|       1 |  488 | `	}` |
|       - |  489 | `	/* Key does not look like a decimal number */` |
|  275429 |  490 | `	return FALSE;` |
|  137835 |  491 | `}` |
|       - |  492 | `/*` |
|       - |  493 | ` * Check if a given key exists in the given hashmap.` |
|       - |  494 | ` * Write a pointer to the target node on success.` |
|       - |  495 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  496 | ` */` |
|  125788 |  497 | `static sxi32 HashmapLookup(` |
|       - |  498 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  499 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  500 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  501 | `	)` |
|       5 |  502 | `{` |
|  125793 |  503 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  504 | `	sxi32 rc;` |
|  125793 |  505 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  124247 |  506 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  507 | `			/* Force a string cast */` |
|     ! 0 |  508 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  509 | `		}` |
|  124247 |  510 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  511 | `			/* Perform a blob lookup */` |
|  124231 |  512 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  124231 |  513 | `			goto result;` |
|       - |  514 | `		}` |
|       8 |  515 | `	}` |
|       - |  516 | `	/* Perform an int lookup */` |
|    1567 |  517 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  518 | `		/* Force an integer cast */` |
|      27 |  519 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  520 | `	}` |
|       - |  521 | `	/* Perform an int lookup */` |
|    1567 |  522 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   62894 |  523 | `result:` |
|  125793 |  524 | `	if( rc == SXRET_OK ){` |
|       - |  525 | `		/* Node found */` |
|   54337 |  526 | `		if( ppNode ){` |
|   54293 |  527 | `			*ppNode = pNode;` |
|   27144 |  528 | `		}` |
|   54337 |  529 | `		return SXRET_OK;` |
|       - |  530 | `	}` |
|       - |  531 | `	/* No such entry */` |
|   71461 |  532 | `	return SXERR_NOTFOUND;` |
|   62899 |  533 | `}` |
|       - |  534 | `/*` |
|       - |  535 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|       - |  536 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|       - |  537 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|       - |  538 | ` * via HashmapAppendIndexBusy.` |
|       - |  539 | ` */` |
|   23530 |  540 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|       5 |  541 | `{` |
|   23535 |  542 | `	if( iKey >= pMap->iNextIdx ){` |
|   23291 |  543 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       - |  544 | `		/* Make sure the automatic index is not reserved */` |
|   23291 |  545 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  546 | `			pMap->iNextIdx++;` |
|     ! 0 |  547 | `		}` |
|   11643 |  548 | `	}` |
|   23535 |  549 | `}` |
|       - |  550 | `/*` |
|       - |  551 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|       - |  552 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|       - |  553 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|       - |  554 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|       - |  555 | ` */` |
| 3044642 |  556 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|       5 |  557 | `{` |
| 3044647 |  558 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       7 |  559 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|       7 |  560 | `		return TRUE;` |
|       - |  561 | `	}` |
| 3044641 |  562 | `	return FALSE;` |
| 1522326 |  563 | `}` |
|       - |  564 | `/*` |
|       - |  565 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  566 | ` * hashmap.` |
|       - |  567 | ` * If a node with the given key already exists in the database` |
|       - |  568 | ` * then this function overwrite the old value.` |
|       - |  569 | ` */` |
| 3173464 |  570 | `static sxi32 HashmapInsert(` |
|       - |  571 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  572 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  573 | `	ph7_value *pVal    /* Node value */` |
|       - |  574 | `	)` |
|       5 |  575 | `{` |
| 3173469 |  576 | `	ph7_hashmap_node *pNode = 0;` |
| 3173469 |  577 | `	sxi32 rc = SXRET_OK;` |
| 3173469 |  578 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  108935 |  579 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  580 | `			/* Force a string cast */` |
|       3 |  581 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  582 | `		}` |
|  108935 |  583 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    3697 |  584 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  585 | `				/* Automatic index assign */` |
|    3475 |  586 | `				pKey = 0;` |
|    1735 |  587 | `			}` |
|    3697 |  588 | `			goto IntKey;` |
|       - |  589 | `		}` |
|  157862 |  590 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   52619 |  591 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  592 | `				/* Overwrite the old value */` |
|       - |  593 | `				ph7_value *pElem;` |
|      81 |  594 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      81 |  595 | `				if( pElem ){` |
|      81 |  596 | `					if( pVal ){` |
|      81 |  597 | `						PH7_MemObjStore(pVal,pElem);` |
|      42 |  598 | `					}else{` |
|       - |  599 | `						/* Nullify the entry */` |
|     ! 0 |  600 | `						PH7_MemObjToNull(pElem);` |
|       - |  601 | `					}` |
|      39 |  602 | `				}` |
|      81 |  603 | `				return SXRET_OK;` |
|       - |  604 | `		}` |
|  105165 |  605 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  606 | `			/* Forbidden */` |
|       3 |  607 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  608 | `			return SXRET_OK;` |
|       - |  609 | `		}` |
|       - |  610 | `		/* Perform a blob-key insertion */` |
|  105163 |  611 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|  105163 |  612 | `		return rc;` |
|       - |  613 | `	}` |
| 1532267 |  614 | `IntKey:` |
| 3068231 |  615 | `	if( pKey ){` |
|   23619 |  616 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  617 | `			/* Force an integer cast */` |
|     251 |  618 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  619 | `		}` |
|   23619 |  620 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  621 | `			/* Overwrite the old value */` |
|       - |  622 | `			ph7_value *pElem;` |
|      87 |  623 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      87 |  624 | `			if( pElem ){` |
|      87 |  625 | `				if( pVal ){` |
|      87 |  626 | `					PH7_MemObjStore(pVal,pElem);` |
|      44 |  627 | `				}else{` |
|       - |  628 | `					/* Nullify the entry */` |
|     ! 0 |  629 | `					PH7_MemObjToNull(pElem);` |
|       - |  630 | `				}` |
|      43 |  631 | `			}` |
|      87 |  632 | `			return SXRET_OK;` |
|       - |  633 | `		}` |
|   23533 |  634 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  635 | `			/* Forbidden */` |
|       3 |  636 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  637 | `			return SXRET_OK;` |
|       - |  638 | `		}` |
|       - |  639 | `		/* Perform a 64-bit-int-key insertion */` |
|   23531 |  640 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23531 |  641 | `		if( rc == SXRET_OK ){` |
|   23531 |  642 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   11763 |  643 | `		}` |
|   11768 |  644 | `	}else{` |
| 3044617 |  645 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  646 | `			/* Forbidden */` |
|       3 |  647 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  648 | `			return SXRET_OK;` |
|       - |  649 | `		}` |
| 3044615 |  650 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       7 |  651 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|       - |  652 | `		}` |
|       - |  653 | `		/* Assign an automatic index */` |
| 3044609 |  654 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 3044609 |  655 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
| 3044607 |  656 | `			++pMap->iNextIdx;` |
| 1522301 |  657 | `		}` |
|       - |  658 | `	}` |
|       - |  659 | `	/* Insertion result */` |
| 3068135 |  660 | `	return rc;` |
| 1586737 |  661 | `}` |
|       - |  662 | `/*` |
|       - |  663 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - |  664 | ` * hashmap.` |
|       - |  665 | ` * This is insertion by reference so be careful to mark the node` |
|       - |  666 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - |  667 | ` * The insertion by reference is triggered when the following` |
|       - |  668 | ` * expression is encountered.` |
|       - |  669 | ` * $var = 10;` |
|       - |  670 | ` *  $a = array(&var);` |
|       - |  671 | ` * OR` |
|       - |  672 | ` *  $a[] =& $var;` |
|       - |  673 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - |  674 | ` * over it's contents.` |
|       - |  675 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - |  676 | ` * removed when the foreign ph7_value is unset.` |
|       - |  677 | ` * Example:` |
|       - |  678 | ` *  $var = 10;` |
|       - |  679 | ` *  $a[] =& $var;` |
|       - |  680 | ` *  echo count($a).PHP_EOL; //1` |
|       - |  681 | ` *  //Unset the foreign ph7_value now` |
|       - |  682 | ` *  unset($var);` |
|       - |  683 | ` *  echo count($a); //0` |
|       - |  684 | ` * Note that this is a PH7 eXtension.` |
|       - |  685 | ` * Refer to the official documentation for more information.` |
|       - |  686 | ` * If a node with the given key already exists in the database` |
|       - |  687 | ` * then this function overwrite the old value.` |
|       - |  688 | ` */` |
|   46000 |  689 | `static sxi32 HashmapInsertByRef(` |
|       - |  690 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  691 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  692 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  693 | `	)` |
|       5 |  694 | `{` |
|   46005 |  695 | `	ph7_hashmap_node *pNode = 0;` |
|   46005 |  696 | `	sxi32 rc = SXRET_OK;` |
|   46005 |  697 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   45969 |  698 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  699 | `			/* Force a string cast */` |
|     ! 0 |  700 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  701 | `		}` |
|   45969 |  702 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  703 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  704 | `				/* Automatic index assign */` |
|     ! 0 |  705 | `				pKey = 0;` |
|     ! 0 |  706 | `			}` |
|     ! 0 |  707 | `			goto IntKey;` |
|       - |  708 | `		}` |
|   68951 |  709 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   22982 |  710 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  711 | `				/* Overwrite */` |
|       7 |  712 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  713 | `				pNode->nValIdx = nRefIdx;` |
|       - |  714 | `				/* Install in the reference table */` |
|       7 |  715 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  716 | `				return SXRET_OK;` |
|       - |  717 | `		}` |
|       - |  718 | `		/* Perform a blob-key insertion */` |
|   45963 |  719 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   45963 |  720 | `		return rc;` |
|       - |  721 | `	}` |
|      18 |  722 | `IntKey:` |
|      37 |  723 | `	if( pKey ){` |
|       5 |  724 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  725 | `			/* Force an integer cast */` |
|     ! 0 |  726 | `			PH7_MemObjToInteger(pKey);` |
|     ! 0 |  727 | `		}` |
|       5 |  728 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  729 | `			/* Overwrite */` |
|     ! 0 |  730 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|     ! 0 |  731 | `			pNode->nValIdx = nRefIdx;` |
|       - |  732 | `			/* Install in the reference table */` |
|     ! 0 |  733 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|     ! 0 |  734 | `			return SXRET_OK;` |
|       - |  735 | `		}` |
|       - |  736 | `		/* Perform a 64-bit-int-key insertion */` |
|       5 |  737 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|       5 |  738 | `		if( rc == SXRET_OK ){` |
|       5 |  739 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|       2 |  740 | `		}` |
|       3 |  741 | `	}else{` |
|      33 |  742 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|     ! 0 |  743 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|       - |  744 | `		}` |
|       - |  745 | `		/* Assign an automatic index */` |
|      33 |  746 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|      33 |  747 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|      33 |  748 | `			++pMap->iNextIdx;` |
|      16 |  749 | `		}` |
|       - |  750 | `	}` |
|       - |  751 | `	/* Insertion result */` |
|      37 |  752 | `	return rc;` |
|   23005 |  753 | `}` |
|       - |  754 | `/*` |
|       - |  755 | ` * Extract node value.` |
|       - |  756 | ` */` |
| 1336389 |  757 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       5 |  758 | `{` |
|       - |  759 | `	/* Point to the desired object */` |
|       - |  760 | `	ph7_value *pObj;` |
| 1336394 |  761 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1336394 |  762 | `	return pObj;` |
|       5 |  763 | `}` |
|       - |  764 | `/*` |
|       - |  765 | ` * Insert a node in the given hashmap.` |
|       - |  766 | ` * If a node with the given key already exists in the database` |
|       - |  767 | ` * then this function overwrite the old value.` |
|       - |  768 | ` */` |
|     446 |  769 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       5 |  770 | `{` |
|       - |  771 | `	ph7_value *pObj;` |
|       - |  772 | `	sxi32 rc;` |
|       - |  773 | `	/* Extract the node value */` |
|     451 |  774 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     451 |  775 | `	if( pObj == 0 ){` |
|     ! 0 |  776 | `		return SXERR_EMPTY;` |
|       - |  777 | `	}` |
|       - |  778 | `	/* Preserve key */` |
|     451 |  779 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  780 | `		/* Int64 key */` |
|     321 |  781 | `		if( !bPreserve ){` |
|       - |  782 | `			/* Assign an automatic index */` |
|     173 |  783 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      89 |  784 | `		}else{` |
|     149 |  785 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  786 | `		}` |
|     163 |  787 | `	}else{` |
|       - |  788 | `		/* Blob key */` |
|     131 |  789 | `		if( !bPreserve ){` |
|       - |  790 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  791 | `			 * original string key entirely */` |
|      35 |  792 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      18 |  793 | `		}else{` |
|     145 |  794 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      48 |  795 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  796 | `		}` |
|       - |  797 | `	}` |
|     451 |  798 | `	return rc;` |
|     228 |  799 | `}` |
|       - |  800 | `/*` |
|       - |  801 | ` * Compare two node values.` |
|       - |  802 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  803 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  804 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  805 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  806 | ` * documenation.` |
|       - |  807 | ` */` |
|   68540 |  808 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       5 |  809 | `{` |
|       - |  810 | `	ph7_value sObj1,sObj2;` |
|       - |  811 | `	sxi32 rc;` |
|   68545 |  812 | `	if( pLeft == pRight ){` |
|       - |  813 | `		/*` |
|       - |  814 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  815 | `		 * below for more information on this sceanario.` |
|       - |  816 | `		 */` |
|     ! 0 |  817 | `		return 0;` |
|       - |  818 | `	}` |
|       - |  819 | `	/* Do the comparison */` |
|   68545 |  820 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   68545 |  821 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   68545 |  822 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   68545 |  823 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   68545 |  824 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   68545 |  825 | `	PH7_MemObjRelease(&sObj1);` |
|   68545 |  826 | `	PH7_MemObjRelease(&sObj2);` |
|   68545 |  827 | `	return rc;` |
|   34301 |  828 | `}` |
|       - |  829 | `/*` |
|       - |  830 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  831 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  832 | ` */` |
|   13106 |  833 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       5 |  834 | `{` |
|   13111 |  835 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  836 | `	sxu32 nBucket;` |
|       - |  837 | `	/* Remove old collision links */` |
|   13111 |  838 | `	if( pEntry->pPrevCollide ){` |
|   10729 |  839 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    5371 |  840 | `	}else{` |
|    2387 |  841 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  842 | `	}` |
|   13111 |  843 | `	if( pEntry->pNextCollide ){` |
|    1067 |  844 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     539 |  845 | `	}` |
|   13111 |  846 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  847 | `	/* Compute the new hash */` |
|   13111 |  848 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   13111 |  849 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   13111 |  850 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  851 | `	/* Link to the new bucket */` |
|   13111 |  852 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   13111 |  853 | `	if( pMap->apBucket[nBucket] ){` |
|   11050 |  854 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    5523 |  855 | `	}` |
|   13111 |  856 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   13111 |  857 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  858 | `	/* Increment the automatic index (saturating, like every other advance —` |
|       - |  859 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|       - |  860 | `	 * the no-overflow invariant uniform). */` |
|   13111 |  861 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|   13111 |  862 | `		pMap->iNextIdx++;` |
|    6553 |  863 | `	}` |
|   13111 |  864 | `}` |
|       - |  865 | `/*` |
|       - |  866 | ` * Perform a linear search on a given hashmap.` |
|       - |  867 | ` * Write a pointer to the target node on success.` |
|       - |  868 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  869 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  870 | ` * for more information.` |
|       - |  871 | ` */` |
|   32470 |  872 | `static int HashmapFindValue(` |
|       - |  873 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  874 | `	ph7_value *pNeedle,  /* Lookup key */` |
|       - |  875 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|       - |  876 | `	int bStrict      /* TRUE for strict comparison */` |
|       - |  877 | `	)` |
|       5 |  878 | `{` |
|       - |  879 | `	ph7_hashmap_node *pEntry;` |
|       - |  880 | `	ph7_value sVal,*pVal;` |
|       - |  881 | `	ph7_value sNeedle;` |
|       - |  882 | `	sxi32 rc;` |
|       - |  883 | `	sxu32 n;` |
|       - |  884 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|   32475 |  885 | `	pEntry = pMap->pFirst;` |
|   32475 |  886 | `	n = pMap->nEntry;` |
|   32475 |  887 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   32475 |  888 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   77517 |  889 | `	for(;;){` |
|  155040 |  890 | `		if( n < 1 ){` |
|      99 |  891 | `			break;` |
|       - |  892 | `		}` |
|       - |  893 | `		/* Extract node value */` |
|  154942 |  894 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  154942 |  895 | `		if( pVal ){` |
|  154942 |  896 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|     ! 0 |  897 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  898 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  899 | `				if( iF1 == iF2 ){` |
|       - |  900 | `					/* NULL values are equals */` |
|     ! 0 |  901 | `					if( ppNode ){` |
|     ! 0 |  902 | `						*ppNode = pEntry;` |
|     ! 0 |  903 | `					}` |
|     ! 0 |  904 | `					return SXRET_OK;` |
|       - |  905 | `				}` |
|     ! 0 |  906 | `			}else{` |
|       - |  907 | `				/* Duplicate value */` |
|  154942 |  908 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  154942 |  909 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  154942 |  910 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  154942 |  911 | `				PH7_MemObjRelease(&sVal);` |
|  154942 |  912 | `				PH7_MemObjRelease(&sNeedle);` |
|  154942 |  913 | `				if( rc == 0 ){` |
|   32377 |  914 | `					if( ppNode ){` |
|      23 |  915 | `						*ppNode = pEntry;` |
|      11 |  916 | `					}` |
|       - |  917 | `					/* Match found*/` |
|   32377 |  918 | `					return SXRET_OK;` |
|       - |  919 | `				}` |
|       - |  920 | `			}` |
|   61282 |  921 | `		}` |
|       - |  922 | `		/* Point to the next entry */` |
|  122570 |  923 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  122570 |  924 | `		n--;` |
|       5 |  925 | `	}` |
|       - |  926 | `	/* No such entry */` |
|      99 |  927 | `	return SXERR_NOTFOUND;` |
|   16240 |  928 | `}` |
|       - |  929 | `/*` |
|       - |  930 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  931 | ` * for values comparison.` |
|       - |  932 | ` * Write a pointer to the target node on success.` |
|       - |  933 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  934 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  935 | ` * for more information.` |
|       - |  936 | ` */` |
|      22 |  937 | `static int HashmapFindValueByCallback(` |
|       - |  938 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|       - |  939 | `	ph7_value *pNeedle,    /* Lookup key */` |
|       - |  940 | `	ph7_value *pCallback,  /* User defined callback */` |
|       - |  941 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|       - |  942 | `	)` |
|       1 |  943 | `{` |
|       - |  944 | `	ph7_hashmap_node *pEntry;` |
|       - |  945 | `	ph7_value sResult,*pVal;` |
|       - |  946 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|       - |  947 | `	sxi32 rc;` |
|       - |  948 | `	sxu32 n;` |
|      23 |  949 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|       - |  950 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|       - |  951 | `		 * exception is not thrown again, and let the caller wind down. */` |
|     ! 0 |  952 | `		return SXERR_NOTFOUND;` |
|       - |  953 | `	}` |
|       - |  954 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|      23 |  955 | `	pEntry = pMap->pFirst;` |
|      23 |  956 | `	n = pMap->nEntry;` |
|       - |  957 | `	/* Store callback result here */` |
|      23 |  958 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  959 | `	/* First argument to the callback */` |
|      23 |  960 | `	apArg[0] = pNeedle;` |
|      25 |  961 | `	for(;;){` |
|      51 |  962 | `		if( n < 1 ){` |
|       9 |  963 | `			break;` |
|       - |  964 | `		}` |
|       - |  965 | `		/* Extract node value */` |
|      43 |  966 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      43 |  967 | `		if( pVal ){` |
|       - |  968 | `			/* Invoke the user callback */` |
|      43 |  969 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      43 |  970 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      43 |  971 | `			if( rc == PH7_EXCEPTION ){` |
|       - |  972 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|       - |  973 | `				 * and report no match for the rest of the run. */` |
|       5 |  974 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|       5 |  975 | `				PH7_MemObjRelease(&sResult);` |
|       5 |  976 | `				return SXERR_NOTFOUND;` |
|       - |  977 | `			}` |
|      39 |  978 | `			if( rc == SXRET_OK ){` |
|       - |  979 | `				/* Extract callback result */` |
|      39 |  980 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  981 | `					/* Perform an int cast */` |
|     ! 0 |  982 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  983 | `				}` |
|      39 |  984 | `				rc = (sxi32)sResult.x.iVal;` |
|      39 |  985 | `				PH7_MemObjRelease(&sResult);` |
|      39 |  986 | `				if( rc == 0 ){` |
|       - |  987 | `					/* Match found*/` |
|      11 |  988 | `					if( ppNode ){` |
|     ! 0 |  989 | `						*ppNode = pEntry;` |
|     ! 0 |  990 | `					}` |
|      11 |  991 | `					return SXRET_OK;` |
|       - |  992 | `				}` |
|      14 |  993 | `			}` |
|      14 |  994 | `		}` |
|       - |  995 | `		/* Point to the next entry */` |
|      29 |  996 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 |  997 | `		n--;` |
|       1 |  998 | `	}` |
|       - |  999 | `	/* No such entry */` |
|       9 | 1000 | `	return SXERR_NOTFOUND;` |
|      12 | 1001 | `}` |
|       - | 1002 | `/*` |
|       - | 1003 | ` * Compare two hashmaps.` |
|       - | 1004 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|       - | 1005 | ` * Note on array comparison operators.` |
|       - | 1006 | ` *  According to the PHP language reference manual.` |
|       - | 1007 | ` *  Array Operators Example 	Name 	Result` |
|       - | 1008 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|       - | 1009 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|       - | 1010 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|       - | 1011 | ` *                          order and of the same types.` |
|       - | 1012 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - | 1013 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - | 1014 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|       - | 1015 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1016 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1017 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1018 | ` * <?php` |
|       - | 1019 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1020 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1021 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1022 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1023 | ` * var_dump($c);` |
|       - | 1024 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1025 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1026 | ` * var_dump($c);` |
|       - | 1027 | ` * ?>` |
|       - | 1028 | ` * When executed, this script will print the following:` |
|       - | 1029 | ` * Union of $a and $b:` |
|       - | 1030 | ` * array(3) {` |
|       - | 1031 | ` *  ["a"]=>` |
|       - | 1032 | ` *  string(5) "apple"` |
|       - | 1033 | ` *  ["b"]=>` |
|       - | 1034 | ` * string(6) "banana"` |
|       - | 1035 | ` *  ["c"]=>` |
|       - | 1036 | ` * string(6) "cherry"` |
|       - | 1037 | ` * }` |
|       - | 1038 | ` * Union of $b and $a:` |
|       - | 1039 | ` * array(3) {` |
|       - | 1040 | ` * ["a"]=>` |
|       - | 1041 | ` * string(4) "pear"` |
|       - | 1042 | ` * ["b"]=>` |
|       - | 1043 | ` * string(10) "strawberry"` |
|       - | 1044 | ` * ["c"]=>` |
|       - | 1045 | ` * string(6) "cherry"` |
|       - | 1046 | ` * }` |
|       - | 1047 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|       - | 1048 | ` */` |
|      26 | 1049 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - | 1050 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - | 1051 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - | 1052 | `	int bStrict          /* TRUE for strict comparison */` |
|       - | 1053 | `	)` |
|       1 | 1054 | `{` |
|       - | 1055 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - | 1056 | `	sxi32 rc;` |
|       - | 1057 | `	sxu32 n;` |
|      27 | 1058 | `	if( pLeft == pRight ){` |
|       - | 1059 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - | 1060 | `		 * Unlike the zend engine.` |
|       - | 1061 | `		 */` |
|     ! 0 | 1062 | `		return 0;` |
|       - | 1063 | `	}` |
|      27 | 1064 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - | 1065 | `		/* Must have the same number of entries */` |
|       5 | 1066 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1067 | `	}` |
|       - | 1068 | `	/* Point to the first inserted entry of the left hashmap */` |
|      23 | 1069 | `	pLe = pLeft->pFirst;` |
|      23 | 1070 | `	pRe = 0; /* cc warning */` |
|       - | 1071 | `	/* Perform the comparison */` |
|      23 | 1072 | `	n = pLeft->nEntry;` |
|      27 | 1073 | `	for(;;){` |
|      55 | 1074 | `		if( n < 1 ){` |
|      21 | 1075 | `			break;` |
|       - | 1076 | `		}` |
|      35 | 1077 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1078 | `			/* Int key */` |
|      27 | 1079 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|      14 | 1080 | `		}else{` |
|       9 | 1081 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1082 | `			/* Blob key */` |
|       9 | 1083 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1084 | `		}` |
|      35 | 1085 | `		if( rc != SXRET_OK ){` |
|       - | 1086 | `			/* No such entry in the right side */` |
|     ! 0 | 1087 | `			return 1;` |
|       - | 1088 | `		}` |
|      35 | 1089 | `		rc = 0;` |
|      35 | 1090 | `		if( bStrict ){` |
|       - | 1091 | `			/* Make sure,the keys are of the same type */` |
|      19 | 1092 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1093 | `				rc = 1;` |
|     ! 0 | 1094 | `			}` |
|       9 | 1095 | `		}` |
|      35 | 1096 | `		if( !rc ){` |
|       - | 1097 | `			/* Compare nodes */` |
|      35 | 1098 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|      17 | 1099 | `		}` |
|      35 | 1100 | `		if( rc != 0 ){` |
|       - | 1101 | `			/* Nodes key/value differ */` |
|       3 | 1102 | `			return rc;` |
|       - | 1103 | `		}` |
|       - | 1104 | `		/* Point to the next entry */` |
|      33 | 1105 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      33 | 1106 | `		n--;` |
|       1 | 1107 | `	}` |
|      21 | 1108 | `	return 0; /* Hashmaps are equals */` |
|      14 | 1109 | `}` |
|       - | 1110 | `/*` |
|       - | 1111 | ` * Duplicate a hashmap node.` |
|       - | 1112 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1113 | ` */` |
|  619262 | 1114 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1115 | `	ph7_hashmap *pDest,` |
|       - | 1116 | `	ph7_hashmap_node *pEntry,` |
|       - | 1117 | `	ph7_value *pVal,` |
|       - | 1118 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1119 | `	)` |
|       5 | 1120 | `{` |
|       - | 1121 | `	ph7_value sSafeVal;` |
|       - | 1122 | `	ph7_value sKey;` |
|       - | 1123 | `	sxi32 rc;` |
|       - | 1124 |  |
|  619267 | 1125 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 1126 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|       - | 1127 | `		 * Re-insert it by reference so the reference survives the duplication` |
|       - | 1128 | `		 * instead of being flattened to a value copy. This keeps spread` |
|       - | 1129 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|       - | 1130 | `		 * with PHP semantics. */` |
|       7 | 1131 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|       7 | 1132 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       3 | 1133 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|       3 | 1134 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|       3 | 1135 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       3 | 1136 | `			PH7_MemObjRelease(&sKey);` |
|       2 | 1137 | `		}else{` |
|       5 | 1138 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|       5 | 1139 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|       2 | 1140 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|     ! 0 | 1141 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|     ! 0 | 1142 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|     ! 0 | 1143 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 1144 | `			}else{ /* Dup: preserve the int key */` |
|     ! 0 | 1145 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|       - | 1146 | `			}` |
|       - | 1147 | `		}` |
|       7 | 1148 | `		return rc;` |
|       - | 1149 | `	}` |
|  619261 | 1150 | `	sSafeVal = *pVal;` |
|       - | 1151 |  |
|  619261 | 1152 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1153 | `		/* Blob key insertion */` |
|    3891 | 1154 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|    3891 | 1155 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    3891 | 1156 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|    3891 | 1157 | `		PH7_MemObjRelease(&sKey);` |
|    1948 | 1158 | `	}else{` |
|       - | 1159 | `		/* Int key */` |
|  615375 | 1160 | `		if( iAction == 0 ){ /* Merge */` |
|  615165 | 1161 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  307793 | 1162 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1163 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1164 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1165 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1166 | `		}else{ /* Dup */` |
|     182 | 1167 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1168 | `		}` |
|       - | 1169 | `	}` |
|  619261 | 1170 | `	return rc;` |
|  309636 | 1171 | `}` |
|       - | 1172 | `/*` |
|       - | 1173 | ` * Merge two hashmaps.` |
|       - | 1174 | ` * Note on the merge process` |
|       - | 1175 | ` * According to the PHP language reference manual.` |
|       - | 1176 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|       - | 1177 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|       - | 1178 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|       - | 1179 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|       - | 1180 | ` *  the later value will not overwrite the original value, but will be appended.` |
|       - | 1181 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|       - | 1182 | ` *  keys starting from zero in the result array.` |
|       - | 1183 | ` */` |
|    2104 | 1184 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       5 | 1185 | `{` |
|       - | 1186 | `	ph7_hashmap_node *pEntry;` |
|       - | 1187 | `	ph7_value *pVal;` |
|       - | 1188 | `	sxi32 rc;` |
|       - | 1189 | `	sxu32 n;` |
|    2109 | 1190 | `	if( pSrc == pDest ){` |
|       - | 1191 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1192 | `		 * Unlike the zend engine.` |
|       - | 1193 | `		 */` |
|     ! 0 | 1194 | `		return SXRET_OK;` |
|       - | 1195 | `	}` |
|       - | 1196 | `	/* Point to the first inserted entry in the source */` |
|    2109 | 1197 | `	pEntry = pSrc->pFirst;` |
|       - | 1198 | `	/* Perform the merge */` |
|  617327 | 1199 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1200 | `		/* Extract the node value */` |
|  615223 | 1201 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  615223 | 1202 | `		if( pVal ){` |
|       - | 1203 | `			/* Make a local copy of the value.` |
|       - | 1204 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1205 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1206 | `			 * to the old pool.` |
|       - | 1207 | `			 */` |
|  615223 | 1208 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  307614 | 1209 | `		}else{` |
|     ! 0 | 1210 | `			rc = SXRET_OK;` |
|       - | 1211 | `		}` |
|  615223 | 1212 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1213 | `			return rc;` |
|       - | 1214 | `		}` |
|       - | 1215 | `		/* Point to the next entry */` |
|  615223 | 1216 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  307614 | 1217 | `	}` |
|    2109 | 1218 | `	return SXRET_OK;` |
|    1057 | 1219 | `}` |
|       - | 1220 | `/*` |
|       - | 1221 | ` * Overwrite entries with the same key.` |
|       - | 1222 | ` * Refer to the [array_replace()] implementation for more information.` |
|       - | 1223 | ` *  According to the PHP language reference manual.` |
|       - | 1224 | ` *  array_replace() replaces the values of the first array with the same values` |
|       - | 1225 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|       - | 1226 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|       - | 1227 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|       - | 1228 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|       - | 1229 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|       - | 1230 | ` *  overwriting the previous values.` |
|       - | 1231 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|       - | 1232 | ` *  by whatever type is in the second array.` |
|       - | 1233 | ` */` |
|      34 | 1234 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1235 | `{` |
|       - | 1236 | `	ph7_hashmap_node *pEntry;` |
|       - | 1237 | `	ph7_value *pVal;` |
|       - | 1238 | `	sxi32 rc;` |
|       - | 1239 | `	sxu32 n;` |
|      36 | 1240 | `	if( pSrc == pDest ){` |
|       - | 1241 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1242 | `		 * Unlike the zend engine.` |
|       - | 1243 | `		 */` |
|     ! 0 | 1244 | `		return SXRET_OK;` |
|       - | 1245 | `	}` |
|       - | 1246 | `	/* Point to the first inserted entry in the source */` |
|      36 | 1247 | `	pEntry = pSrc->pFirst;` |
|       - | 1248 | `	/* Perform the merge */` |
|      80 | 1249 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1250 | `		/* Extract the node value */` |
|      46 | 1251 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      46 | 1252 | `		if( pVal ){` |
|      46 | 1253 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      24 | 1254 | `		}else{` |
|     ! 0 | 1255 | `			rc = SXRET_OK;` |
|       - | 1256 | `		}` |
|      46 | 1257 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1258 | `			return rc;` |
|       - | 1259 | `		}` |
|       - | 1260 | `		/* Point to the next entry */` |
|      46 | 1261 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 1262 | `	}` |
|      36 | 1263 | `	return SXRET_OK;` |
|      19 | 1264 | `}` |
|       - | 1265 | `/*` |
|       - | 1266 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|       - | 1267 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|       - | 1268 | ` */` |
|    3896 | 1269 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       5 | 1270 | `{` |
|       - | 1271 | `	ph7_hashmap_node *pEntry;` |
|       - | 1272 | `	ph7_value *pVal;` |
|       - | 1273 | `	sxi32 rc;` |
|       - | 1274 | `	sxu32 n;` |
|    3901 | 1275 | `	if( pSrc == pDest ){` |
|       - | 1276 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1277 | `		 * Unlike the zend engine.` |
|       - | 1278 | `		 */` |
|     ! 0 | 1279 | `		return SXRET_OK;` |
|       - | 1280 | `	}` |
|       - | 1281 | `	/* Point to the first inserted entry in the source */` |
|    3901 | 1282 | `	pEntry = pSrc->pFirst;` |
|       - | 1283 | `	/* Perform the duplication */` |
|    7901 | 1284 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1285 | `		/* Extract the node value */` |
|    4005 | 1286 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    4005 | 1287 | `		if( pVal ){` |
|    4005 | 1288 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|    2005 | 1289 | `		}else{` |
|     ! 0 | 1290 | `			rc = SXRET_OK;` |
|       - | 1291 | `		}` |
|    4005 | 1292 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1293 | `			return rc;` |
|       - | 1294 | `		}` |
|       - | 1295 | `		/* Point to the next entry */` |
|    4005 | 1296 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    2005 | 1297 | `	}` |
|    3901 | 1298 | `	return SXRET_OK;` |
|    1953 | 1299 | `}` |
|       - | 1300 | `/*` |
|       - | 1301 | ` * Copy-on-write separation for arrays.` |
|       - | 1302 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|       - | 1303 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|       - | 1304 | ` * Returns the (possibly new) hashmap pointer.` |
|       - | 1305 | ` */` |
|  214440 | 1306 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1307 | `{` |
|  214445 | 1308 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1309 | `	ph7_hashmap *pNew;` |
|       - | 1310 | `	ph7_value *pBacking;` |
|       - | 1311 | `	sxu32 nValIdx;` |
|       - | 1312 | `	int bValueInPool;` |
|  214445 | 1313 | `	if( pMap->iRef < 2 ){` |
|       - | 1314 | `		/* Sole owner, no separation needed */` |
|  212271 | 1315 | `		return pMap;` |
|       - | 1316 | `	}` |
|    2179 | 1317 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1318 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1319 | `		return pMap;` |
|       - | 1320 | `	}` |
|       - | 1321 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1322 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1323 | `	 * frame is popped. */` |
|    2179 | 1324 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2179 | 1325 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    2174 | 1326 | `		if( pBacking && pBacking != pValue` |
|    2156 | 1327 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2143 | 1328 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1329 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2143 | 1330 | `			pMap->iRef--;` |
|    2143 | 1331 | `			if( pMap->iRef < 2 ){` |
|       - | 1332 | `				/* After undoing stack ref, sole owner — no separation */` |
|    2103 | 1333 | `				pMap->iRef++;` |
|    2103 | 1334 | `				return pMap;` |
|       - | 1335 | `			}` |
|      42 | 1336 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|      42 | 1337 | `			if( pNew == 0 ){` |
|     ! 0 | 1338 | `				pMap->iRef++;` |
|     ! 0 | 1339 | `				return pMap;` |
|       - | 1340 | `			}` |
|      42 | 1341 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1342 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|     ! 0 | 1343 | `				PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1344 | `				pMap->iRef++;` |
|     ! 0 | 1345 | `				return pMap;` |
|       - | 1346 | `			}` |
|      42 | 1347 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|      42 | 1348 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|       - | 1349 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|       - | 1350 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|       - | 1351 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|       - | 1352 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|       - | 1353 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|       - | 1354 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|       - | 1355 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|      42 | 1356 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      42 | 1357 | `			if( pBacking ){` |
|      42 | 1358 | `				pBacking->x.pOther = pNew;` |
|      20 | 1359 | `			}` |
|       - | 1360 | `			/* Update the stack value to match */` |
|      42 | 1361 | `			pValue->x.pOther = pNew;` |
|      42 | 1362 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|      42 | 1363 | `			return pNew;` |
|       - | 1364 | `		}` |
|      18 | 1365 | `	}` |
|       - | 1366 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|       - | 1367 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|       - | 1368 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|       - | 1369 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|       - | 1370 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|       - | 1371 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|       - | 1372 | `	 * pBacking in the backing-variable branch above). */` |
|      37 | 1373 | `	nValIdx = pValue->nIdx;` |
|      55 | 1374 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|      36 | 1375 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|      37 | 1376 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      37 | 1377 | `	if( pNew == 0 ){` |
|       - | 1378 | `		/* Allocation failure — fall through with shared map */` |
|     ! 0 | 1379 | `		return pMap;` |
|       - | 1380 | `	}` |
|      37 | 1381 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1382 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|     ! 0 | 1383 | `		PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1384 | `		return pMap;` |
|       - | 1385 | `	}` |
|      37 | 1386 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      37 | 1387 | `	pMap->iRef--;` |
|      37 | 1388 | `	if( bValueInPool ){` |
|       - | 1389 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|      37 | 1390 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|      37 | 1391 | `		if( pValue == 0 ){` |
|     ! 0 | 1392 | `			return pNew;` |
|       - | 1393 | `		}` |
|      18 | 1394 | `	}` |
|      37 | 1395 | `	pValue->x.pOther = pNew;` |
|      37 | 1396 | `	return pNew;` |
|  107225 | 1397 | `}` |
|       - | 1398 | `/*` |
|       - | 1399 | ` * Perform the union of two hashmaps.` |
|       - | 1400 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1401 | ` * with a variable holding an array as follows:` |
|       - | 1402 | ` * <?php` |
|       - | 1403 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1404 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1405 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1406 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1407 | ` * var_dump($c);` |
|       - | 1408 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1409 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1410 | ` * var_dump($c);` |
|       - | 1411 | ` * ?>` |
|       - | 1412 | ` * When executed, this script will print the following:` |
|       - | 1413 | ` * Union of $a and $b:` |
|       - | 1414 | ` * array(3) {` |
|       - | 1415 | ` *  ["a"]=>` |
|       - | 1416 | ` *  string(5) "apple"` |
|       - | 1417 | ` *  ["b"]=>` |
|       - | 1418 | ` * string(6) "banana"` |
|       - | 1419 | ` *  ["c"]=>` |
|       - | 1420 | ` * string(6) "cherry"` |
|       - | 1421 | ` * }` |
|       - | 1422 | ` * Union of $b and $a:` |
|       - | 1423 | ` * array(3) {` |
|       - | 1424 | ` * ["a"]=>` |
|       - | 1425 | ` * string(4) "pear"` |
|       - | 1426 | ` * ["b"]=>` |
|       - | 1427 | ` * string(10) "strawberry"` |
|       - | 1428 | ` * ["c"]=>` |
|       - | 1429 | ` * string(6) "cherry"` |
|       - | 1430 | ` * }` |
|       - | 1431 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1432 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1433 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1434 | ` */` |
|    3796 | 1435 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       5 | 1436 | `{` |
|       - | 1437 | `	ph7_hashmap_node *pEntry;` |
|    3801 | 1438 | `	sxi32 rc = SXRET_OK;` |
|       - | 1439 | `	ph7_value *pObj;` |
|       - | 1440 | `	sxu32 n;` |
|    3801 | 1441 | `	if( pLeft == pRight ){` |
|       - | 1442 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1443 | `		 * Unlike the zend engine.` |
|       - | 1444 | `		 */` |
|     ! 0 | 1445 | `		return SXRET_OK;` |
|       - | 1446 | `	}` |
|       - | 1447 | `	/* Perform the union */` |
|    3801 | 1448 | `	pEntry = pRight->pFirst;` |
|    3835 | 1449 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1450 | `		/* Make sure the given key does not exists in the left array */` |
|      37 | 1451 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1452 | `			/* BLOB key */` |
|      23 | 1453 | `			if( SXRET_OK !=` |
|      20 | 1454 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|      19 | 1455 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|      19 | 1456 | `					if( pObj ){` |
|      19 | 1457 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1458 | `						/* Perform the insertion */` |
|      19 | 1459 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1460 | `							&sSafeVal,0,FALSE);` |
|      19 | 1461 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1462 | `							return rc;` |
|       - | 1463 | `						}` |
|       8 | 1464 | `					}` |
|       8 | 1465 | `			}` |
|      13 | 1466 | `		}else{` |
|       - | 1467 | `			/* INT key */` |
|      16 | 1468 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|      11 | 1469 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|      11 | 1470 | `				if( pObj ){` |
|      11 | 1471 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1472 | `					/* Perform the insertion */` |
|      11 | 1473 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      11 | 1474 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1475 | `						return rc;` |
|       - | 1476 | `					}` |
|       5 | 1477 | `				}` |
|       5 | 1478 | `			}` |
|       - | 1479 | `		}` |
|       - | 1480 | `		/* Point to the next entry */` |
|      37 | 1481 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      20 | 1482 | `	}` |
|    3801 | 1483 | `	return SXRET_OK;` |
|    1903 | 1484 | `}` |
|       - | 1485 | `/*` |
|       - | 1486 | ` * Allocate a new hashmap.` |
|       - | 1487 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1488 | ` */` |
|  114330 | 1489 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1490 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1491 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1492 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1493 | `	)` |
|       5 | 1494 | `{` |
|       - | 1495 | `	ph7_hashmap *pMap;` |
|       - | 1496 | `	/* Allocate a new instance */` |
|  114335 | 1497 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|  114335 | 1498 | `	if( pMap == 0 ){` |
|     ! 0 | 1499 | `		return 0;` |
|       - | 1500 | `	}` |
|       - | 1501 | `	/* Zero the structure */` |
|  114335 | 1502 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1503 | `	/* Fill in the structure */` |
|  114335 | 1504 | `	pMap->pVm = &(*pVm);` |
|  114335 | 1505 | `	pMap->iRef = 1;` |
|       - | 1506 | `	/* Default hash functions */` |
|  114335 | 1507 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|  114335 | 1508 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|  114335 | 1509 | `	return pMap;` |
|   57170 | 1510 | `}` |
|       - | 1511 | `/*` |
|       - | 1512 | ` * Install superglobals in the given virtual machine.` |
|       - | 1513 | ` * Note on superglobals.` |
|       - | 1514 | ` *  According to the PHP language reference manual.` |
|       - | 1515 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1516 | `*   Description` |
|       - | 1517 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1518 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1519 | `*   global $variable; to access them within functions or methods.` |
|       - | 1520 | `*   These superglobal variables are:` |
|       - | 1521 | `*    $GLOBALS` |
|       - | 1522 | `*    $_SERVER` |
|       - | 1523 | `*    $_GET` |
|       - | 1524 | `*    $_POST` |
|       - | 1525 | `*    $_FILES` |
|       - | 1526 | `*    $_COOKIE` |
|       - | 1527 | `*    $_SESSION` |
|       - | 1528 | `*    $_REQUEST` |
|       - | 1529 | `*    $_ENV` |
|       - | 1530 | `*/` |
|    3460 | 1531 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       5 | 1532 | `{` |
|       - | 1533 | `	static const char * azSuper[] = {` |
|       - | 1534 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1535 | `		"_GET",      /* $_GET */` |
|       - | 1536 | `		"_POST",     /* $_POST */` |
|       - | 1537 | `		"_FILES",    /* $_FILES */` |
|       - | 1538 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1539 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1540 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1541 | `		"_ENV",      /* $_ENV */` |
|       - | 1542 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1543 | `		"argv"       /* $argv */` |
|       - | 1544 | `	};` |
|       - | 1545 | `	ph7_hashmap *pMap;` |
|       - | 1546 | `	ph7_value *pObj;` |
|       - | 1547 | `	SyString *pFile;` |
|       - | 1548 | `	sxi32 rc;` |
|       - | 1549 | `	sxu32 n;` |
|       - | 1550 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    3465 | 1551 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    3465 | 1552 | `	if( pMap == 0 ){` |
|     ! 0 | 1553 | `		return SXERR_MEM;` |
|       - | 1554 | `	}` |
|    3465 | 1555 | `	pVm->pGlobal = pMap;` |
|       - | 1556 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    3465 | 1557 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    3465 | 1558 | `	if( pObj == 0 ){` |
|     ! 0 | 1559 | `		return SXERR_MEM;` |
|       - | 1560 | `	}` |
|    3465 | 1561 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1562 | `	/* Record object index */` |
|    3465 | 1563 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1564 | `	/* Install the special $GLOBALS array */` |
|    3465 | 1565 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    3465 | 1566 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1567 | `		return rc;` |
|       - | 1568 | `	}` |
|       - | 1569 | `	/* Install superglobals now */` |
|   38065 | 1570 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1571 | `		ph7_value *pSuper;` |
|       - | 1572 | `		/* Request an empty array */` |
|   34605 | 1573 | `		pSuper = ph7_new_array(&(*pVm));` |
|   34605 | 1574 | `		if( pSuper == 0 ){` |
|     ! 0 | 1575 | `			return SXERR_MEM;` |
|       - | 1576 | `		}` |
|       - | 1577 | `		/* Install */` |
|   34605 | 1578 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   34605 | 1579 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1580 | `			return rc;` |
|       - | 1581 | `		}` |
|       - | 1582 | `		/* Release the value now it have been installed */` |
|   34605 | 1583 | `		ph7_release_value(&(*pVm),pSuper);` |
|   17305 | 1584 | `	}` |
|       - | 1585 | `	/* Set some $_SERVER entries */` |
|    3465 | 1586 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1587 | `	/*` |
|       - | 1588 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1589 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1590 | `	 */` |
|    6921 | 1591 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1592 | `		"SCRIPT_FILENAME",` |
|    1730 | 1593 | `		pFile ? pFile->zString : ":Memory:",` |
|    3456 | 1594 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1595 | `		);` |
|       - | 1596 | `	/* All done,all super-global are installed now */` |
|    3465 | 1597 | `	return SXRET_OK;` |
|    1735 | 1598 | `}` |
|       - | 1599 | `/*` |
|       - | 1600 | ` * Release a hashmap.` |
|       - | 1601 | ` */` |
|   71854 | 1602 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       5 | 1603 | `{` |
|       - | 1604 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   71859 | 1605 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1606 | `	sxu32 n;` |
|   71859 | 1607 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1608 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1609 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1610 | `		return SXRET_OK;` |
|       - | 1611 | `	}` |
|       - | 1612 | `	/* Start the release process */` |
|   71859 | 1613 | `	n = 0;` |
|   71859 | 1614 | `	pEntry = pMap->pFirst;` |
| 1603851 | 1615 | `	for(;;){` |
| 3207707 | 1616 | `		if( n >= pMap->nEntry ){` |
|   71859 | 1617 | `			break;` |
|       - | 1618 | `		}` |
| 3135853 | 1619 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1620 | `		/* Remove the reference from the foreign table */` |
| 3135853 | 1621 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3135853 | 1622 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1623 | `			/* Restore the ph7_value to the free list */` |
| 3135843 | 1624 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1567919 | 1625 | `		}` |
|       - | 1626 | `		/* Release the node */` |
| 3135853 | 1627 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   78383 | 1628 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   39189 | 1629 | `		}` |
| 3135853 | 1630 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1631 | `		/* Point to the next entry */` |
| 3135853 | 1632 | `		pEntry = pNext;` |
| 3135853 | 1633 | `		n++;` |
|       5 | 1634 | `	}` |
|   71859 | 1635 | `	if( pMap->nEntry > 0 ){` |
|       - | 1636 | `		/* Release the hash bucket */` |
|   58609 | 1637 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   29302 | 1638 | `	}` |
|   71859 | 1639 | `	if( FreeDS ){` |
|       - | 1640 | `		/* Free the whole instance */` |
|   71843 | 1641 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   35924 | 1642 | `	}else{` |
|       - | 1643 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1644 | `		pMap->apBucket = 0;` |
|      17 | 1645 | `		pMap->iNextIdx = 0;` |
|      17 | 1646 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1647 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1648 | `	}` |
|   71859 | 1649 | `	return SXRET_OK;` |
|   35932 | 1650 | `}` |
|       - | 1651 | `/*` |
|       - | 1652 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1653 | ` * If the count reaches zero which mean no more variables` |
|       - | 1654 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1655 | ` */` |
|  723190 | 1656 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       5 | 1657 | `{` |
|  723195 | 1658 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1659 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  723195 | 1660 | `	pMap->iRef--;` |
|  723195 | 1661 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   71823 | 1662 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   35909 | 1663 | `	}` |
|  723195 | 1664 | `}` |
|       - | 1665 | `/*` |
|       - | 1666 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1667 | ` * Write a pointer to the target node on success.` |
|       - | 1668 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1669 | ` */` |
|  125848 | 1670 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1671 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1672 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1673 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1674 | `	)` |
|       5 | 1675 | `{` |
|       - | 1676 | `	sxi32 rc;` |
|  125853 | 1677 | `	if( pMap->nEntry < 1 ){` |
|       - | 1678 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1679 | `		 */` |
|      65 | 1680 | `		return SXERR_NOTFOUND;` |
|       - | 1681 | `	}` |
|  125793 | 1682 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  125793 | 1683 | `	return rc;` |
|   62929 | 1684 | `}` |
|       - | 1685 | `/*` |
|       - | 1686 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1687 | ` * hashmap.` |
|       - | 1688 | ` * If a node with the given key already exists in the database` |
|       - | 1689 | ` * then this function overwrite the old value.` |
|       - | 1690 | ` */` |
| 2558072 | 1691 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1692 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1693 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1694 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1695 | `	)` |
|       5 | 1696 | `{` |
|       - | 1697 | `	sxi32 rc;` |
| 2558077 | 1698 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1699 | `		/*` |
|       - | 1700 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1701 | `		 */` |
|     ! 0 | 1702 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1703 | `		return SXRET_OK;` |
|       - | 1704 | `	}` |
| 2558077 | 1705 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2558077 | 1706 | `	return rc;` |
| 1279041 | 1707 | `}` |
|       - | 1708 | `/*` |
|       - | 1709 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|       - | 1710 | ` *   - String keys overwrite same-key entries in pDest.` |
|       - | 1711 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|       - | 1712 | ` * This is the same routine that backs array_merge().` |
|       - | 1713 | ` */` |
|      52 | 1714 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1715 | `{` |
|      53 | 1716 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|       1 | 1717 | `}` |
|       - | 1718 | `/*` |
|       - | 1719 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1720 | ` * hashmap.` |
|       - | 1721 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1722 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1723 | ` * The insertion by reference is triggered when the following` |
|       - | 1724 | ` * expression is encountered.` |
|       - | 1725 | ` * $var = 10;` |
|       - | 1726 | ` *  $a = array(&var);` |
|       - | 1727 | ` * OR` |
|       - | 1728 | ` *  $a[] =& $var;` |
|       - | 1729 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1730 | ` * over it's contents.` |
|       - | 1731 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1732 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1733 | ` * Example:` |
|       - | 1734 | ` *  $var = 10;` |
|       - | 1735 | ` *  $a[] =& $var;` |
|       - | 1736 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1737 | ` *  //Unset the foreign ph7_value now` |
|       - | 1738 | ` *  unset($var);` |
|       - | 1739 | ` *  echo count($a); //0` |
|       - | 1740 | ` * Note that this is a PH7 eXtension.` |
|       - | 1741 | ` * Refer to the official documentation for more information.` |
|       - | 1742 | ` * If a node with the given key already exists in the database` |
|       - | 1743 | ` * then this function overwrite the old value.` |
|       - | 1744 | ` */` |
|   45994 | 1745 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1746 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1747 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1748 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1749 | `	)` |
|       5 | 1750 | `{` |
|       - | 1751 | `	sxi32 rc;` |
|   45999 | 1752 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1753 | `		/*` |
|       - | 1754 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1755 | `		 */` |
|     ! 0 | 1756 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1757 | `		return SXRET_OK;` |
|       - | 1758 | `	}` |
|   45999 | 1759 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   45999 | 1760 | `	return rc;` |
|   23002 | 1761 | `}` |
|       - | 1762 | `/*` |
|       - | 1763 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1764 | ` */` |
|   34878 | 1765 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       5 | 1766 | `{` |
|       - | 1767 | `	/* Reset the loop cursor */` |
|   34883 | 1768 | `	pMap->pCur = pMap->pFirst;` |
|   34883 | 1769 | `}` |
|       - | 1770 | `/*` |
|       - | 1771 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1772 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1773 | ` * return NULL.` |
|       - | 1774 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1775 | ` */` |
|  229826 | 1776 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       5 | 1777 | `{` |
|  229831 | 1778 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  229831 | 1779 | `	if( pCur == 0 ){` |
|       - | 1780 | `		/* End of the list,return null */` |
|   17463 | 1781 | `		return 0;` |
|       - | 1782 | `	}` |
|       - | 1783 | `	/* Advance the node cursor */` |
|  212373 | 1784 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  212373 | 1785 | `	return pCur;` |
|  114918 | 1786 | `}` |
|       - | 1787 | `/*` |
|       - | 1788 | ` * Extract a node value.` |
|       - | 1789 | ` */` |
|  537926 | 1790 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       5 | 1791 | `{` |
|  537931 | 1792 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  537931 | 1793 | `	if( pEntry ){` |
|  537931 | 1794 | `		if( bStore ){` |
|  212723 | 1795 | `			PH7_MemObjStore(pEntry,pValue);` |
|  106364 | 1796 | `		}else{` |
|  325213 | 1797 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1798 | `		}` |
|  269020 | 1799 | `	}else{` |
|     ! 0 | 1800 | `		PH7_MemObjRelease(pValue);` |
|       - | 1801 | `	}` |
|  537931 | 1802 | `}` |
|       - | 1803 | `/*` |
|       - | 1804 | ` * Extract a node key.` |
|       - | 1805 | ` */` |
|  138496 | 1806 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       5 | 1807 | `{` |
|       - | 1808 | `	/* Fill with the current key */` |
|  138501 | 1809 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  134219 | 1810 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1811 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1812 | `		}` |
|  134219 | 1813 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  134219 | 1814 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   67112 | 1815 | `	}else{` |
|    4287 | 1816 | `		SyBlobReset(&pKey->sBlob);` |
|    4287 | 1817 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    4287 | 1818 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1819 | `	}` |
|  138501 | 1820 | `}` |
|       - | 1821 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1822 | `/*` |
|       - | 1823 | ` * Store the address of nodes value in the given container.` |
|       - | 1824 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1825 | ` * defined in 'builtin.c' for more information.` |
|       - | 1826 | ` */` |
|      10 | 1827 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1828 | `{` |
|      11 | 1829 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1830 | `	ph7_value *pValue;` |
|       - | 1831 | `	sxu32 n;` |
|       - | 1832 | `	/* Initialize the container */` |
|      11 | 1833 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1834 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1835 | `		/* Extract node value */` |
|      17 | 1836 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1837 | `		if( pValue ){` |
|      17 | 1838 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1839 | `		}` |
|       - | 1840 | `		/* Point to the next entry */` |
|      17 | 1841 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1842 | `	}` |
|       - | 1843 | `	/* Total inserted entries */` |
|      11 | 1844 | `	return (int)SySetUsed(pOut);` |
|       1 | 1845 | `}` |
|       - | 1846 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1847 | `/* SPDX-SnippetBegin */` |
|       - | 1848 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - | 1849 | `/* SPDX-License-Identifier: blessing */` |
|       - | 1850 | `/*` |
|       - | 1851 | ` * Merge sort.` |
|       - | 1852 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1853 | ` * Status: Public domain` |
|       - | 1854 | ` */` |
|       - | 1855 | `/* Node comparison callback signature */` |
|       - | 1856 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1857 | `/*` |
|       - | 1858 | `** Inputs:` |
|       - | 1859 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1860 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1861 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1862 | `**` |
|       - | 1863 | `** Return Value:` |
|       - | 1864 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1865 | `**   of both a and b.` |
|       - | 1866 | `**` |
|       - | 1867 | `** Side effects:` |
|       - | 1868 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1869 | `**   changed.` |
|       - | 1870 | `*/` |
|   33278 | 1871 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1872 | `{` |
|       - | 1873 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1874 | `    /* Prevent compiler warning */` |
|   33283 | 1875 | `	result.pNext = result.pPrev = 0;` |
|   33283 | 1876 | `	pTail = &result;` |
|  101960 | 1877 | `	while( pA && pB ){` |
|   68682 | 1878 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   45457 | 1879 | `			pTail->pPrev = pA;` |
|   45457 | 1880 | `			pA->pNext = pTail;` |
|   45457 | 1881 | `			pTail = pA;` |
|   45457 | 1882 | `			pA = pA->pPrev;` |
|   22734 | 1883 | `		}else{` |
|   23230 | 1884 | `			pTail->pPrev = pB;` |
|   23230 | 1885 | `			pB->pNext = pTail;` |
|   23230 | 1886 | `			pTail = pB;` |
|   23230 | 1887 | `			pB = pB->pPrev;` |
|       - | 1888 | `		}` |
|       5 | 1889 | `	}` |
|   33283 | 1890 | `	if( pA ){` |
|   23291 | 1891 | `		pTail->pPrev = pA;` |
|   23291 | 1892 | `		pA->pNext = pTail;` |
|   21639 | 1893 | `	}else if( pB ){` |
|    9781 | 1894 | `		pTail->pPrev = pB;` |
|    9781 | 1895 | `		pB->pNext = pTail;` |
|    4894 | 1896 | `	}else{` |
|     221 | 1897 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1898 | `	}` |
|   33283 | 1899 | `	return result.pPrev;` |
|       5 | 1900 | `}` |
|       - | 1901 | `/*` |
|       - | 1902 | `** Inputs:` |
|       - | 1903 | `**   Map:       Input hashmap` |
|       - | 1904 | `**   cmp:       A comparison function.` |
|       - | 1905 | `**` |
|       - | 1906 | `** Return Value:` |
|       - | 1907 | `**   Sorted hashmap.` |
|       - | 1908 | `**` |
|       - | 1909 | `** Side effects:` |
|       - | 1910 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1911 | `*/` |
|       - | 1912 | `#define N_SORT_BUCKET  32` |
|     686 | 1913 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1914 | `{` |
|       - | 1915 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1916 | `	sxu32 i;` |
|     691 | 1917 | `	SyZero(a,sizeof(a));` |
|       - | 1918 | `	/* Point to the first inserted entry */` |
|     691 | 1919 | `	pIn = pMap->pFirst;` |
|   13913 | 1920 | `	while( pIn ){` |
|   13227 | 1921 | `		p = pIn;` |
|   13227 | 1922 | `		pIn = p->pPrev;` |
|   13227 | 1923 | `		p->pPrev = 0;` |
|   25239 | 1924 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   25239 | 1925 | `			if( a[i]==0 ){` |
|   13227 | 1926 | `				a[i] = p;` |
|   13227 | 1927 | `				break;` |
|     ! 0 | 1928 | `			}else{` |
|   12017 | 1929 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   12017 | 1930 | `				a[i] = 0;` |
|       - | 1931 | `			}` |
|    6011 | 1932 | `		}` |
|   13227 | 1933 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1934 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1935 | `			 * But that is impossible.` |
|       - | 1936 | `			 */` |
|     ! 0 | 1937 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1938 | `		}` |
|       5 | 1939 | `	}` |
|     691 | 1940 | `	p = a[0];` |
|   21957 | 1941 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   21271 | 1942 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   10638 | 1943 | `	}` |
|     691 | 1944 | `	p->pNext = 0;` |
|       - | 1945 | `	/* Reflect the change */` |
|     691 | 1946 | `	pMap->pFirst = p;` |
|       - | 1947 | `	/* Reset the loop cursor */` |
|     691 | 1948 | `	pMap->pCur = pMap->pFirst;` |
|     691 | 1949 | `	return SXRET_OK;` |
|       5 | 1950 | `}` |
|       - | 1951 | `/* SPDX-SnippetEnd */` |
|       - | 1952 | `/*` |
|       - | 1953 | ` * Node comparison callback.` |
|       - | 1954 | ` * used-by: [sort(),asort(),...]` |
|       - | 1955 | ` */` |
|   68472 | 1956 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 | 1957 | `{` |
|       - | 1958 | `	ph7_value sA,sB;` |
|       - | 1959 | `	sxi32 iFlags;` |
|       - | 1960 | `	int rc;` |
|   68477 | 1961 | `	if( pCmpData == 0 ){` |
|       - | 1962 | `		/* Perform a standard comparison */` |
|   68453 | 1963 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   68453 | 1964 | `		return rc;` |
|       - | 1965 | `	}` |
|      25 | 1966 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1967 | `	/* Duplicate node values */` |
|      25 | 1968 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 1969 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 1970 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 1971 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 1972 | `	if( iFlags == 5 ){` |
|       - | 1973 | `		/* String cast */` |
|       - | 1974 | `		const char *zA,*zB;` |
|       - | 1975 | `		sxu32 nA,nB,nMin;` |
|      15 | 1976 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1977 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1978 | `		}` |
|      15 | 1979 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1980 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1981 | `		}` |
|       - | 1982 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 1983 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 1984 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 1985 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 1986 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 1987 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 1988 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 1989 | `		if( rc == 0 ){` |
|       5 | 1990 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1991 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1992 | `		}` |
|       8 | 1993 | `	}else{` |
|       - | 1994 | `		/* Numeric cast */` |
|      11 | 1995 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1996 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1997 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1998 | `	}` |
|      25 | 1999 | `	PH7_MemObjRelease(&sA);` |
|      25 | 2000 | `	PH7_MemObjRelease(&sB);` |
|      25 | 2001 | `	return rc;` |
|   34267 | 2002 | `}` |
|       - | 2003 | `/*` |
|       - | 2004 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2005 | ` * used-by: [ksort()]` |
|       - | 2006 | ` */` |
|      14 | 2007 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2008 | `{` |
|       - | 2009 | `	sxi32 rc;` |
|       7 | 2010 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 2011 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2012 | `		/* Perform a string comparison */` |
|       5 | 2013 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2014 | `	}else{` |
|       - | 2015 | `		SyString sStr;` |
|       - | 2016 | `		sxi64 iA,iB;` |
|       - | 2017 | `		/* Perform a numeric comparison */` |
|      11 | 2018 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2019 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2020 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2021 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2022 | `				iA = 0;` |
|     ! 0 | 2023 | `			}else{` |
|     ! 0 | 2024 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2025 | `			}` |
|     ! 0 | 2026 | `		}else{` |
|      11 | 2027 | `			iA = pA->xKey.iKey;` |
|       - | 2028 | `		}` |
|      11 | 2029 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2030 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2031 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2032 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2033 | `				iB = 0;` |
|     ! 0 | 2034 | `			}else{` |
|     ! 0 | 2035 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2036 | `			}` |
|     ! 0 | 2037 | `		}else{` |
|      11 | 2038 | `			iB = pB->xKey.iKey;` |
|       - | 2039 | `		}` |
|      11 | 2040 | `		rc = (sxi32)(iA-iB);` |
|       - | 2041 | `	}` |
|       - | 2042 | `	/* Comparison result */` |
|      15 | 2043 | `	return rc;` |
|       1 | 2044 | `}` |
|       - | 2045 | `/*` |
|       - | 2046 | ` * Node comparison callback.` |
|       - | 2047 | ` * Used by: [rsort(),arsort()];` |
|       - | 2048 | ` */` |
|      78 | 2049 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2050 | `{` |
|       - | 2051 | `	ph7_value sA,sB;` |
|       - | 2052 | `	sxi32 iFlags;` |
|       - | 2053 | `	int rc;` |
|      79 | 2054 | `	if( pCmpData == 0 ){` |
|       - | 2055 | `		/* Perform a standard comparison */` |
|      59 | 2056 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 2057 | `		return -rc;` |
|       - | 2058 | `	}` |
|      21 | 2059 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 2060 | `	/* Duplicate node values */` |
|      21 | 2061 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 2062 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 2063 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 2064 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 2065 | `	if( iFlags == 5 ){` |
|       - | 2066 | `		/* String cast */` |
|       - | 2067 | `		const char *zA,*zB;` |
|       - | 2068 | `		sxu32 nA,nB,nMin;` |
|      11 | 2069 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2070 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 2071 | `		}` |
|      11 | 2072 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2073 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 2074 | `		}` |
|       - | 2075 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 2076 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 2077 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 2078 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 2079 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 2080 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 2081 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 2082 | `		if( rc == 0 ){` |
|       3 | 2083 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 2084 | `			else if( nA > nB ) rc = 1;` |
|       1 | 2085 | `		}` |
|       6 | 2086 | `	}else{` |
|       - | 2087 | `		/* Numeric cast */` |
|      11 | 2088 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 2089 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 2090 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 2091 | `	}` |
|      21 | 2092 | `	PH7_MemObjRelease(&sA);` |
|      21 | 2093 | `	PH7_MemObjRelease(&sB);` |
|      21 | 2094 | `	return -rc;` |
|      40 | 2095 | `}` |
|       - | 2096 | `/*` |
|       - | 2097 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2098 | ` * used-by: [usort(),uasort()]` |
|       - | 2099 | ` */` |
|      88 | 2100 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 2101 | `{` |
|       - | 2102 | `	ph7_value sResult,*pCallback;` |
|       - | 2103 | `	ph7_value *pV1,*pV2;` |
|       - | 2104 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2105 | `	sxi32 rc;` |
|       - | 2106 | `	/* Point to the desired callback */` |
|      90 | 2107 | `	pCallback = (ph7_value *)pCmpData;` |
|      90 | 2108 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2109 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2110 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       6 | 2111 | `		return 0;` |
|       - | 2112 | `	}` |
|       - | 2113 | `	/* initialize the result value */` |
|      86 | 2114 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 2115 | `	/* Extract nodes values */` |
|      86 | 2116 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      86 | 2117 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      86 | 2118 | `	apArg[0] = pV1;` |
|      86 | 2119 | `	apArg[1] = pV2;` |
|       - | 2120 | `	/* Invoke the callback */` |
|      86 | 2121 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      86 | 2122 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2123 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2124 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       6 | 2125 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       6 | 2126 | `		rc = 0;` |
|      84 | 2127 | `	}else if( rc != SXRET_OK ){` |
|       - | 2128 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2129 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2130 | `	}else{` |
|       - | 2131 | `		/* Extract callback result */` |
|      82 | 2132 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2133 | `			/* Perform an int cast */` |
|     ! 0 | 2134 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2135 | `		}` |
|      82 | 2136 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2137 | `	}` |
|      86 | 2138 | `	PH7_MemObjRelease(&sResult);` |
|       - | 2139 | `	/* Callback result */` |
|      86 | 2140 | `	return rc;` |
|      46 | 2141 | `}` |
|       - | 2142 | `/*` |
|       - | 2143 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2144 | ` * used-by: [krsort()]` |
|       - | 2145 | ` */` |
|       4 | 2146 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2147 | `{` |
|       - | 2148 | `	sxi32 rc;` |
|       2 | 2149 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2150 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2151 | `		/* Perform a string comparison */` |
|       5 | 2152 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2153 | `	}else{` |
|       - | 2154 | `		SyString sStr;` |
|       - | 2155 | `		sxi64 iA,iB;` |
|       - | 2156 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2157 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2158 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2159 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2160 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2161 | `				iA = 0;` |
|     ! 0 | 2162 | `			}else{` |
|     ! 0 | 2163 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2164 | `			}` |
|     ! 0 | 2165 | `		}else{` |
|     ! 0 | 2166 | `			iA = pA->xKey.iKey;` |
|       - | 2167 | `		}` |
|     ! 0 | 2168 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2169 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2170 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2171 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2172 | `				iB = 0;` |
|     ! 0 | 2173 | `			}else{` |
|     ! 0 | 2174 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2175 | `			}` |
|     ! 0 | 2176 | `		}else{` |
|     ! 0 | 2177 | `			iB = pB->xKey.iKey;` |
|       - | 2178 | `		}` |
|     ! 0 | 2179 | `		rc = (sxi32)(iA-iB);` |
|       - | 2180 | `	}` |
|       5 | 2181 | `	return -rc; /* Reverse result */` |
|       1 | 2182 | `}` |
|       - | 2183 | `/*` |
|       - | 2184 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2185 | ` * used-by: [uksort()]` |
|       - | 2186 | ` */` |
|       6 | 2187 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2188 | `{` |
|       - | 2189 | `	ph7_value sResult,*pCallback;` |
|       - | 2190 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2191 | `	ph7_value sK1,sK2;` |
|       - | 2192 | `	sxi32 rc;` |
|       - | 2193 | `	/* Point to the desired callback */` |
|       7 | 2194 | `	pCallback = (ph7_value *)pCmpData;` |
|       7 | 2195 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2196 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2197 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     ! 0 | 2198 | `		return 0;` |
|       - | 2199 | `	}` |
|       - | 2200 | `	/* initialize the result value */` |
|       7 | 2201 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2202 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2203 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2204 | `	/* Extract nodes keys */` |
|       7 | 2205 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2206 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2207 | `	apArg[0] = &sK1;` |
|       7 | 2208 | `	apArg[1] = &sK2;` |
|       - | 2209 | `	/* Mark keys as constants */` |
|       7 | 2210 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2211 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2212 | `	/* Invoke the callback */` |
|       7 | 2213 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2214 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2215 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2216 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     ! 0 | 2217 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     ! 0 | 2218 | `		rc = 0;` |
|       7 | 2219 | `	}else if( rc != SXRET_OK ){` |
|       - | 2220 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2221 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2222 | `	}else{` |
|       - | 2223 | `		/* Extract callback result */` |
|       7 | 2224 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2225 | `			/* Perform an int cast */` |
|     ! 0 | 2226 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2227 | `		}` |
|       7 | 2228 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2229 | `	}` |
|       7 | 2230 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2231 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2232 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2233 | `	/* Callback result */` |
|       7 | 2234 | `	return rc;` |
|       4 | 2235 | `}` |
|       - | 2236 | `/*` |
|       - | 2237 | ` * Node comparison callback: Random node comparison.` |
|       - | 2238 | ` * used-by: [shuffle()]` |
|       - | 2239 | ` */` |
|      15 | 2240 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2241 | `{` |
|       - | 2242 | `	sxu32 n;` |
|       8 | 2243 | `	SXUNUSED(pB); /* cc warning */` |
|       8 | 2244 | `	SXUNUSED(pCmpData);` |
|       - | 2245 | `	/* Grab a random number */` |
|      16 | 2246 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2247 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2248 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2249 | `	 */` |
|      16 | 2250 | `	return n&1 ? 1 : -1;` |
|       1 | 2251 | `}` |
|       - | 2252 | `/*` |
|       - | 2253 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2254 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2255 | ` */` |
|     638 | 2256 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       5 | 2257 | `{` |
|       - | 2258 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2259 | `	sxu32 i;` |
|       - | 2260 | `	/* Rehash all entries */` |
|     643 | 2261 | `	pLast = p = pMap->pFirst;` |
|     643 | 2262 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     643 | 2263 | `	i = 0;` |
|    6845 | 2264 | `	for( ;; ){` |
|   13695 | 2265 | `		if( i >= pMap->nEntry ){` |
|     643 | 2266 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     643 | 2267 | `			break;` |
|       - | 2268 | `		}` |
|   13057 | 2269 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2270 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2271 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2272 | `			/* Change key type */` |
|       5 | 2273 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2274 | `		}` |
|   13057 | 2275 | `		HashmapRehashIntNode(p);` |
|       - | 2276 | `		/* Point to the next entry */` |
|   13057 | 2277 | `		i++;` |
|   13057 | 2278 | `		pLast = p;` |
|   13057 | 2279 | `		p = p->pPrev; /* Reverse link */` |
|       5 | 2280 | `	}` |
|     643 | 2281 | `}` |
|       - | 2282 | `/*` |
|       - | 2283 | ` * Array functions implementation.` |
|       - | 2284 | ` * Status:` |
|       - | 2285 | ` *  Stable.` |
|       - | 2286 | ` */` |
|       - | 2287 | `/*` |
|       - | 2288 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2289 | ` * Sort an array.` |
|       - | 2290 | ` * Parameters` |
|       - | 2291 | ` *  $array` |
|       - | 2292 | ` *   The input array.` |
|       - | 2293 | ` * $sort_flags` |
|       - | 2294 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2295 | ` *  Sorting type flags:` |
|       - | 2296 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2297 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2298 | ` *   SORT_STRING - compare items as strings` |
|       - | 2299 | ` * Return` |
|       - | 2300 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2301 | ` *` |
|       - | 2302 | ` */` |
|     982 | 2303 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2304 | `{` |
|       - | 2305 | `	ph7_hashmap *pMap;` |
|       - | 2306 | `	/* Make sure we are dealing with a valid hashmap */` |
|     987 | 2307 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2308 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2309 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2310 | `		return PH7_OK;` |
|       - | 2311 | `	}` |
|       - | 2312 | `	/* Point to the internal representation of the input hashmap */` |
|     987 | 2313 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     987 | 2314 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     987 | 2315 | `	if( pMap->nEntry > 1 ){` |
|     627 | 2316 | `		sxi32 iCmpFlags = 0;` |
|     627 | 2317 | `		if( nArg > 1 ){` |
|       - | 2318 | `			/* Extract comparison flags */` |
|       3 | 2319 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2320 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2321 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2322 | `			}` |
|       1 | 2323 | `		}` |
|       - | 2324 | `		/* Do the merge sort */` |
|     627 | 2325 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2326 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     627 | 2327 | `		HashmapSortRehash(pMap);` |
|     311 | 2328 | `	}` |
|       - | 2329 | `	/* All done,return TRUE */` |
|     987 | 2330 | `	ph7_result_bool(pCtx,1);` |
|     987 | 2331 | `	return PH7_OK;` |
|     496 | 2332 | `}` |
|       - | 2333 | `/*` |
|       - | 2334 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2335 | ` *  Sort an array and maintain index association.` |
|       - | 2336 | ` * Parameters` |
|       - | 2337 | ` *  $array` |
|       - | 2338 | ` *   The input array.` |
|       - | 2339 | ` * $sort_flags` |
|       - | 2340 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2341 | ` *  Sorting type flags:` |
|       - | 2342 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2343 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2344 | ` *   SORT_STRING - compare items as strings` |
|       - | 2345 | ` * Return` |
|       - | 2346 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2347 | ` */` |
|      32 | 2348 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2349 | `{` |
|       - | 2350 | `	ph7_hashmap *pMap;` |
|       - | 2351 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2352 | `	if( nArg < 1 ){` |
|       3 | 2353 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2354 | `			"ArgumentCountError",` |
|       - | 2355 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2356 | `			);` |
|       - | 2357 | `	}` |
|       - | 2358 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2359 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2360 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2361 | `			"TypeError",` |
|       - | 2362 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2363 | `			ph7_type_name(apArg[0])` |
|       - | 2364 | `			);` |
|       - | 2365 | `	}` |
|       - | 2366 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2367 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2368 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2369 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2370 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2371 | `		if( nArg > 1 ){` |
|       - | 2372 | `			/* Extract comparison flags */` |
|       5 | 2373 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2374 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2375 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2376 | `			}` |
|       2 | 2377 | `		}` |
|       - | 2378 | `		/* Do the merge sort */` |
|      19 | 2379 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2380 | `		/* Fix the last link broken by the merge */` |
|      45 | 2381 | `		while(pMap->pLast->pPrev){` |
|      27 | 2382 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2383 | `		}` |
|       9 | 2384 | `	}` |
|       - | 2385 | `	/* All done,return TRUE */` |
|      23 | 2386 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2387 | `	return PH7_OK;` |
|      21 | 2388 | `}` |
|       - | 2389 | `/*` |
|       - | 2390 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2391 | ` *  Sort an array in reverse order and maintain index association.` |
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
|      32 | 2404 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2405 | `{` |
|       - | 2406 | `	ph7_hashmap *pMap;` |
|       - | 2407 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2408 | `	if( nArg < 1 ){` |
|       3 | 2409 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2410 | `			"ArgumentCountError",` |
|       - | 2411 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2412 | `			);` |
|       - | 2413 | `	}` |
|       - | 2414 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2415 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2416 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2417 | `			"TypeError",` |
|       - | 2418 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2419 | `			ph7_type_name(apArg[0])` |
|       - | 2420 | `			);` |
|       - | 2421 | `	}` |
|       - | 2422 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2423 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2424 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2425 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2426 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2427 | `		if( nArg > 1 ){` |
|       - | 2428 | `			/* Extract comparison flags */` |
|       5 | 2429 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2430 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2431 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2432 | `			}` |
|       2 | 2433 | `		}` |
|       - | 2434 | `		/* Do the merge sort */` |
|      19 | 2435 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2436 | `		/* Fix the last link broken by the merge */` |
|      35 | 2437 | `		while(pMap->pLast->pPrev){` |
|      17 | 2438 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2439 | `		}` |
|       9 | 2440 | `	}` |
|       - | 2441 | `	/* All done,return TRUE */` |
|      23 | 2442 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2443 | `	return PH7_OK;` |
|      21 | 2444 | `}` |
|       - | 2445 | `/*` |
|       - | 2446 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2447 | ` *  Sort an array by key.` |
|       - | 2448 | ` * Parameters` |
|       - | 2449 | ` *  $array` |
|       - | 2450 | ` *   The input array.` |
|       - | 2451 | ` * $sort_flags` |
|       - | 2452 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2453 | ` *  Sorting type flags:` |
|       - | 2454 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2455 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2456 | ` *   SORT_STRING - compare items as strings` |
|       - | 2457 | ` * Return` |
|       - | 2458 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2459 | ` */` |
|       4 | 2460 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2461 | `{` |
|       - | 2462 | `	ph7_hashmap *pMap;` |
|       - | 2463 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2464 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2465 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2466 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2467 | `		return PH7_OK;` |
|       - | 2468 | `	}` |
|       - | 2469 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2470 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2471 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2472 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2473 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2474 | `		if( nArg > 1 ){` |
|       - | 2475 | `			/* Extract comparison flags */` |
|     ! 0 | 2476 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2477 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2478 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2479 | `			}` |
|     ! 0 | 2480 | `		}` |
|       - | 2481 | `		/* Do the merge sort */` |
|       5 | 2482 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2483 | `		/* Fix the last link broken by the merge */` |
|      15 | 2484 | `		while(pMap->pLast->pPrev){` |
|      11 | 2485 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2486 | `		}` |
|       2 | 2487 | `	}` |
|       - | 2488 | `	/* All done,return TRUE */` |
|       5 | 2489 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2490 | `	return PH7_OK;` |
|       3 | 2491 | `}` |
|       - | 2492 | `/*` |
|       - | 2493 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2494 | ` *  Sort an array by key in reverse order.` |
|       - | 2495 | ` * Parameters` |
|       - | 2496 | ` *  $array` |
|       - | 2497 | ` *   The input array.` |
|       - | 2498 | ` * $sort_flags` |
|       - | 2499 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2500 | ` *  Sorting type flags:` |
|       - | 2501 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2502 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2503 | ` *   SORT_STRING - compare items as strings` |
|       - | 2504 | ` * Return` |
|       - | 2505 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2506 | ` */` |
|       2 | 2507 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2508 | `{` |
|       - | 2509 | `	ph7_hashmap *pMap;` |
|       - | 2510 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2511 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2512 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2513 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2514 | `		return PH7_OK;` |
|       - | 2515 | `	}` |
|       - | 2516 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2517 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2518 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2519 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2520 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2521 | `		if( nArg > 1 ){` |
|       - | 2522 | `			/* Extract comparison flags */` |
|     ! 0 | 2523 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2524 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2525 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2526 | `			}` |
|     ! 0 | 2527 | `		}` |
|       - | 2528 | `		/* Do the merge sort */` |
|       3 | 2529 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2530 | `		/* Fix the last link broken by the merge */` |
|       7 | 2531 | `		while(pMap->pLast->pPrev){` |
|       5 | 2532 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2533 | `		}` |
|       1 | 2534 | `	}` |
|       - | 2535 | `	/* All done,return TRUE */` |
|       3 | 2536 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2537 | `	return PH7_OK;` |
|       2 | 2538 | `}` |
|       - | 2539 | `/*` |
|       - | 2540 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2541 | ` * Sort an array in reverse order.` |
|       - | 2542 | ` * Parameters` |
|       - | 2543 | ` *  $array` |
|       - | 2544 | ` *   The input array.` |
|       - | 2545 | ` * $sort_flags` |
|       - | 2546 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2547 | ` *  Sorting type flags:` |
|       - | 2548 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2549 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2550 | ` *   SORT_STRING - compare items as strings` |
|       - | 2551 | ` * Return` |
|       - | 2552 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2553 | ` */` |
|       2 | 2554 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2555 | `{` |
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
|       3 | 2567 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2568 | `		if( nArg > 1 ){` |
|       - | 2569 | `			/* Extract comparison flags */` |
|     ! 0 | 2570 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2571 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2572 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2573 | `			}` |
|     ! 0 | 2574 | `		}` |
|       - | 2575 | `		/* Do the merge sort */` |
|       3 | 2576 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2577 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2578 | `		HashmapSortRehash(pMap);` |
|       1 | 2579 | `	}` |
|       - | 2580 | `	/* All done,return TRUE */` |
|       3 | 2581 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2582 | `	return PH7_OK;` |
|       2 | 2583 | `}` |
|       - | 2584 | `/*` |
|       - | 2585 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2586 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2587 | ` * Parameters` |
|       - | 2588 | ` *  $array` |
|       - | 2589 | ` *   The input array.` |
|       - | 2590 | ` * $cmp_function` |
|       - | 2591 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2592 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2593 | ` *  to, or greater than the second.` |
|       - | 2594 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2595 | ` * Return` |
|       - | 2596 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2597 | ` */` |
|      12 | 2598 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2599 | `{` |
|       - | 2600 | `	ph7_hashmap *pMap;` |
|       - | 2601 | `	/* Make sure we are dealing with a valid hashmap */` |
|      14 | 2602 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2603 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2604 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2605 | `		return PH7_OK;` |
|       - | 2606 | `	}` |
|       - | 2607 | `	/* Point to the internal representation of the input hashmap */` |
|      14 | 2608 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      14 | 2609 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 2610 | `	if( pMap->nEntry > 1 ){` |
|      14 | 2611 | `		ph7_value *pCallback = 0;` |
|       - | 2612 | `		ProcNodeCmp xCmp;` |
|      14 | 2613 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      14 | 2614 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2615 | `			/* Point to the desired callback */` |
|      14 | 2616 | `			pCallback = apArg[1];` |
|       8 | 2617 | `		}else{` |
|       - | 2618 | `			/* Use the default comparison function */` |
|     ! 0 | 2619 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2620 | `		}` |
|       - | 2621 | `		/* Do the merge sort */` |
|      14 | 2622 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      14 | 2623 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2624 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      14 | 2625 | `		HashmapSortRehash(pMap);` |
|      14 | 2626 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2627 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       6 | 2628 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       6 | 2629 | `			return PH7_EXCEPTION;` |
|       - | 2630 | `		}` |
|       4 | 2631 | `	}` |
|       - | 2632 | `	/* All done,return TRUE */` |
|      10 | 2633 | `	ph7_result_bool(pCtx,1);` |
|      10 | 2634 | `	return PH7_OK;` |
|       8 | 2635 | `}` |
|       - | 2636 | `/*` |
|       - | 2637 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2638 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2639 | ` *  and maintain index association.` |
|       - | 2640 | ` * Parameters` |
|       - | 2641 | ` *  $array` |
|       - | 2642 | ` *   The input array.` |
|       - | 2643 | ` * $cmp_function` |
|       - | 2644 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2645 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2646 | ` *  to, or greater than the second.` |
|       - | 2647 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2648 | ` * Return` |
|       - | 2649 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2650 | ` */` |
|       2 | 2651 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2652 | `{` |
|       - | 2653 | `	ph7_hashmap *pMap;` |
|       - | 2654 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2655 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2656 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2657 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2658 | `		return PH7_OK;` |
|       - | 2659 | `	}` |
|       - | 2660 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2661 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2662 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2663 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2664 | `		ph7_value *pCallback = 0;` |
|       - | 2665 | `		ProcNodeCmp xCmp;` |
|       3 | 2666 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2667 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2668 | `			/* Point to the desired callback */` |
|       3 | 2669 | `			pCallback = apArg[1];` |
|       2 | 2670 | `		}else{` |
|       - | 2671 | `			/* Use the default comparison function */` |
|     ! 0 | 2672 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2673 | `		}` |
|       - | 2674 | `		/* Do the merge sort */` |
|       3 | 2675 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2676 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2677 | `		/* Fix the last link broken by the merge */` |
|       5 | 2678 | `		while(pMap->pLast->pPrev){` |
|       3 | 2679 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2680 | `		}` |
|       3 | 2681 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2682 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2683 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2684 | `			return PH7_EXCEPTION;` |
|       - | 2685 | `		}` |
|       1 | 2686 | `	}` |
|       - | 2687 | `	/* All done,return TRUE */` |
|       3 | 2688 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2689 | `	return PH7_OK;` |
|       2 | 2690 | `}` |
|       - | 2691 | `/*` |
|       - | 2692 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2693 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2694 | ` *  function and maintain index association.` |
|       - | 2695 | ` * Parameters` |
|       - | 2696 | ` *  $array` |
|       - | 2697 | ` *   The input array.` |
|       - | 2698 | ` * $cmp_function` |
|       - | 2699 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2700 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2701 | ` *  to, or greater than the second.` |
|       - | 2702 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2703 | ` * Return` |
|       - | 2704 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2705 | ` */` |
|       2 | 2706 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2707 | `{` |
|       - | 2708 | `	ph7_hashmap *pMap;` |
|       - | 2709 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2710 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2711 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2712 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2713 | `		return PH7_OK;` |
|       - | 2714 | `	}` |
|       - | 2715 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2716 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2717 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2718 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2719 | `		ph7_value *pCallback = 0;` |
|       - | 2720 | `		ProcNodeCmp xCmp;` |
|       3 | 2721 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2722 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2723 | `			/* Point to the desired callback */` |
|       3 | 2724 | `			pCallback = apArg[1];` |
|       2 | 2725 | `		}else{` |
|       - | 2726 | `			/* Use the default comparison function */` |
|     ! 0 | 2727 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2728 | `		}` |
|       - | 2729 | `		/* Do the merge sort */` |
|       3 | 2730 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2731 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2732 | `		/* Fix the last link broken by the merge */` |
|       3 | 2733 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2734 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2735 | `		}` |
|       3 | 2736 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2737 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2738 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2739 | `			return PH7_EXCEPTION;` |
|       - | 2740 | `		}` |
|       1 | 2741 | `	}` |
|       - | 2742 | `	/* All done,return TRUE */` |
|       3 | 2743 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2744 | `	return PH7_OK;` |
|       2 | 2745 | `}` |
|       - | 2746 | `/*` |
|       - | 2747 | ` * bool shuffle(array &$array)` |
|       - | 2748 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2749 | ` * Parameters` |
|       - | 2750 | ` *  $array` |
|       - | 2751 | ` *   The input array.` |
|       - | 2752 | ` * Return` |
|       - | 2753 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2754 | ` *` |
|       - | 2755 | ` */` |
|       2 | 2756 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2757 | `{` |
|       - | 2758 | `	ph7_hashmap *pMap;` |
|       - | 2759 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2760 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2761 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2762 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2763 | `		return PH7_OK;` |
|       - | 2764 | `	}` |
|       - | 2765 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2766 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2767 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2768 | `	if( pMap->nEntry > 1 ){` |
|       - | 2769 | `		/* Do the merge sort */` |
|       3 | 2770 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2771 | `		/* Fix the last link broken by the merge */` |
|      10 | 2772 | `		while(pMap->pLast->pPrev){` |
|       7 | 2773 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2774 | `		}` |
|       1 | 2775 | `	}` |
|       - | 2776 | `	/* All done,return TRUE */` |
|       3 | 2777 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2778 | `	return PH7_OK;` |
|       2 | 2779 | `}` |
|       - | 2780 | `/*` |
|       - | 2781 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2782 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2783 | ` * Parameters` |
|       - | 2784 | ` *  $var` |
|       - | 2785 | ` *   The array or the object.` |
|       - | 2786 | ` * $mode` |
|       - | 2787 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2788 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2789 | ` *  all the elements of a multidimensional array.` |
|       - | 2790 | ` * Return` |
|       - | 2791 | ` *  Returns the number of elements in the array.` |
|       - | 2792 | ` */` |
|     840 | 2793 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2794 | `{` |
|     845 | 2795 | `	int bRecursive = FALSE;` |
|     845 | 2796 | `	int bCycleDetected = FALSE;` |
|       - | 2797 | `	sxi64 iCount;` |
|     845 | 2798 | `	if( nArg < 1 ){` |
|       3 | 2799 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2800 | `			"ArgumentCountError",` |
|       - | 2801 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2802 | `			);` |
|       - | 2803 | `	}` |
|     843 | 2804 | `	if( nArg > 2 ){` |
|       4 | 2805 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2806 | `			"ArgumentCountError",` |
|       - | 2807 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2808 | `			nArg` |
|       - | 2809 | `			);` |
|       - | 2810 | `	}` |
|       - | 2811 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - | 2812 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - | 2813 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|     841 | 2814 | `	if( nArg > 1 ){` |
|      45 | 2815 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      45 | 2816 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|      11 | 2817 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2818 | `				"ValueError",` |
|       - | 2819 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2820 | `				);` |
|       - | 2821 | `		}` |
|      34 | 2822 | `		bRecursive = iMode == 1;` |
|      16 | 2823 | `	}` |
|     833 | 2824 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2825 | `		/* Countable object: dispatch to ->count() */` |
|      35 | 2826 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      23 | 2827 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      23 | 2828 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      23 | 2829 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      20 | 2830 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2831 | `					"count",sizeof("count")-1);` |
|      20 | 2832 | `				if( pMeth ){` |
|       - | 2833 | `					ph7_value sResult;` |
|      20 | 2834 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      20 | 2835 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      20 | 2836 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      20 | 2837 | `					PH7_MemObjRelease(&sResult);` |
|      20 | 2838 | `					return PH7_OK;` |
|       - | 2839 | `				}` |
|     ! 0 | 2840 | `			}` |
|       1 | 2841 | `		}` |
|      22 | 2842 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2843 | `			"TypeError",` |
|       - | 2844 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       6 | 2845 | `			ph7_type_name(apArg[0])` |
|       - | 2846 | `			);` |
|       - | 2847 | `	}` |
|       - | 2848 | `	/* Count */` |
|     803 | 2849 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     803 | 2850 | `	if( bCycleDetected ){` |
|       3 | 2851 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2852 | `	}` |
|     803 | 2853 | `	ph7_result_int64(pCtx,iCount);` |
|     803 | 2854 | `	return PH7_OK;` |
|     425 | 2855 | `}` |
|       - | 2856 | `/*` |
|       - | 2857 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2858 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2859 | ` * Parameters` |
|       - | 2860 | ` * $key` |
|       - | 2861 | ` *   Value to check.` |
|       - | 2862 | ` * $search` |
|       - | 2863 | ` *  An array with keys to check.` |
|       - | 2864 | ` * Return` |
|       - | 2865 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2866 | ` */` |
|      84 | 2867 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2868 | `{` |
|       - | 2869 | `	sxi32 rc;` |
|      89 | 2870 | `	if( nArg != 2 ){` |
|       - | 2871 | `		/* PHP requires exactly two arguments */` |
|      12 | 2872 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2873 | `			"ArgumentCountError",` |
|       - | 2874 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2875 | `			nArg` |
|       - | 2876 | `			);` |
|       - | 2877 | `	}` |
|       - | 2878 | `	/* Make sure we are dealing with a valid hashmap */` |
|      82 | 2879 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2880 | `		/* Type mismatch -> TypeError */` |
|       8 | 2881 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2882 | `			"TypeError",` |
|       - | 2883 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2884 | `			ph7_type_name(apArg[1])` |
|       - | 2885 | `			);` |
|       - | 2886 | `	}` |
|       - | 2887 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      78 | 2888 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2889 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2890 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2891 | `			"use an empty string instead"` |
|       - | 2892 | `			);` |
|      77 | 2893 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2894 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2895 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2896 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2897 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2898 | `				,rVal` |
|       - | 2899 | `				);` |
|       1 | 2900 | `		}` |
|       1 | 2901 | `	}` |
|       - | 2902 | `	/* Perform the lookup */` |
|      78 | 2903 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2904 | `	/* lookup result */` |
|      78 | 2905 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      78 | 2906 | `	return PH7_OK;` |
|      47 | 2907 | `}` |
|       - | 2908 | `/*` |
|       - | 2909 | ` * value array_pop(array $array)` |
|       - | 2910 | ` *   POP the last inserted element from the array.` |
|       - | 2911 | ` * Parameter` |
|       - | 2912 | ` *  The array to get the value from.` |
|       - | 2913 | ` * Return` |
|       - | 2914 | ` *  Poped value or NULL on failure.` |
|       - | 2915 | ` */` |
|      18 | 2916 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2917 | `{` |
|       - | 2918 | `	ph7_hashmap *pMap;` |
|       - | 2919 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      23 | 2920 | `	if( nArg != 1 ){` |
|       8 | 2921 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2922 | `			"ArgumentCountError",` |
|       - | 2923 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2924 | `			nArg` |
|       - | 2925 | `			);` |
|       - | 2926 | `	}` |
|       - | 2927 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2928 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      18 | 2929 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 2930 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2931 | `			"Error",` |
|       - | 2932 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2933 | `			);` |
|       - | 2934 | `	}` |
|       - | 2935 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2936 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2937 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2938 | `			"TypeError",` |
|       - | 2939 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2940 | `			ph7_type_name(apArg[0])` |
|       - | 2941 | `			);` |
|       - | 2942 | `	}` |
|       9 | 2943 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2944 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2945 | `	if( pMap->nEntry < 1 ){` |
|       - | 2946 | `		/* Nothing to pop,return NULL */` |
|       3 | 2947 | `		ph7_result_null(pCtx);` |
|       2 | 2948 | `	}else{` |
|       7 | 2949 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2950 | `		ph7_value *pObj;` |
|       7 | 2951 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2952 | `		if( pObj ){` |
|       - | 2953 | `			/* Node value */` |
|       7 | 2954 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2955 | `			/* Unlink the node */` |
|       7 | 2956 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2957 | `		}else{` |
|     ! 0 | 2958 | `			ph7_result_null(pCtx);` |
|       - | 2959 | `		}` |
|       - | 2960 | `		/* Reset the cursor */` |
|       7 | 2961 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2962 | `	}` |
|       9 | 2963 | `	return PH7_OK;` |
|      14 | 2964 | `}` |
|       - | 2965 | `/*` |
|       - | 2966 | ` * int array_push($array,$var,...)` |
|       - | 2967 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2968 | ` * Parameters` |
|       - | 2969 | ` *  array` |
|       - | 2970 | ` *    The input array.` |
|       - | 2971 | ` *  var` |
|       - | 2972 | ` *   On or more value to push.` |
|       - | 2973 | ` * Return` |
|       - | 2974 | ` *  New array count (including old items).` |
|       - | 2975 | ` */` |
|      24 | 2976 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2977 | `{` |
|       - | 2978 | `	ph7_hashmap *pMap;` |
|       - | 2979 | `	sxi32 rc;` |
|       - | 2980 | `	int i;` |
|      29 | 2981 | `	if( nArg < 1 ){` |
|       4 | 2982 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2983 | `			"ArgumentCountError",` |
|       - | 2984 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2985 | `			nArg` |
|       - | 2986 | `			);` |
|       - | 2987 | `	}` |
|       - | 2988 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2989 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      26 | 2990 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 2991 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2992 | `			"Error",` |
|       - | 2993 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2994 | `			);` |
|       - | 2995 | `	}` |
|       - | 2996 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 2997 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2998 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2999 | `			"TypeError",` |
|       - | 3000 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3001 | `			ph7_type_name(apArg[0])` |
|       - | 3002 | `			);` |
|       - | 3003 | `	}` |
|       - | 3004 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 3005 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      18 | 3006 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3007 | `	/* Start pushing given values */` |
|      34 | 3008 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      20 | 3009 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      20 | 3010 | `		if( rc != SXRET_OK ){` |
|       3 | 3011 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       - | 3012 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|       3 | 3013 | `				return rc;` |
|       - | 3014 | `			}` |
|     ! 0 | 3015 | `			break;` |
|       - | 3016 | `		}` |
|       9 | 3017 | `	}` |
|       - | 3018 | `	/* Return the new count */` |
|      15 | 3019 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 3020 | `	return PH7_OK;` |
|      17 | 3021 | `}` |
|       - | 3022 | `/*` |
|       - | 3023 | ` * value array_shift(array $array)` |
|       - | 3024 | ` *   Shift an element off the beginning of array.` |
|       - | 3025 | ` * Parameter` |
|       - | 3026 | ` *  The array to get the value from.` |
|       - | 3027 | ` * Return` |
|       - | 3028 | ` *  Shifted value or NULL on failure.` |
|       - | 3029 | ` */` |
|      38 | 3030 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3031 | `{` |
|       - | 3032 | `	ph7_hashmap *pMap;` |
|       - | 3033 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      43 | 3034 | `	if( nArg != 1 ){` |
|       8 | 3035 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3036 | `			"ArgumentCountError",` |
|       - | 3037 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 3038 | `			nArg` |
|       - | 3039 | `			);` |
|       - | 3040 | `	}` |
|       - | 3041 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      39 | 3042 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 3043 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3044 | `			"Error",` |
|       - | 3045 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 3046 | `			);` |
|       - | 3047 | `	}` |
|       - | 3048 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 3049 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3050 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3051 | `			"TypeError",` |
|       - | 3052 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3053 | `			ph7_type_name(apArg[0])` |
|       - | 3054 | `			);` |
|       - | 3055 | `	}` |
|       - | 3056 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 3057 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      33 | 3058 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3059 | `	if( pMap->nEntry < 1 ){` |
|       - | 3060 | `		/* Empty hashmap,return NULL */` |
|       3 | 3061 | `		ph7_result_null(pCtx);` |
|       2 | 3062 | `	}else{` |
|      31 | 3063 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 3064 | `		ph7_value *pObj;` |
|       - | 3065 | `		sxu32 n;` |
|      31 | 3066 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      31 | 3067 | `		if( pObj ){` |
|       - | 3068 | `			/* Node value */` |
|      31 | 3069 | `			ph7_result_value(pCtx,pObj);` |
|       - | 3070 | `			/* Unlink the first node */` |
|      31 | 3071 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      18 | 3072 | `		}else{` |
|     ! 0 | 3073 | `			ph7_result_null(pCtx);` |
|       - | 3074 | `		}` |
|       - | 3075 | `		/* Rehash all int keys */` |
|      31 | 3076 | `		n = pMap->nEntry;` |
|      31 | 3077 | `		pEntry = pMap->pFirst;` |
|      31 | 3078 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 3079 | `		for(;;){` |
|      85 | 3080 | `			if( n < 1 ){` |
|      31 | 3081 | `				break;` |
|       - | 3082 | `			}` |
|      59 | 3083 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      59 | 3084 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 3085 | `			}` |
|       - | 3086 | `			/* Point to the next entry */` |
|      59 | 3087 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      59 | 3088 | `			n--;` |
|       5 | 3089 | `		}` |
|       - | 3090 | `		/* Reset the cursor */` |
|      31 | 3091 | `		pMap->pCur = pMap->pFirst;` |
|       - | 3092 | `	}` |
|      33 | 3093 | `	return PH7_OK;` |
|      24 | 3094 | `}` |
|       - | 3095 | `/*` |
|       - | 3096 | ` * Extract the node cursor value.` |
|       - | 3097 | ` */` |
|      24 | 3098 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 3099 | `{` |
|      25 | 3100 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 3101 | `	ph7_value *pVal;` |
|      25 | 3102 | `	if( pCur == 0 ){` |
|       - | 3103 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 3104 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3105 | `		return PH7_OK;` |
|       - | 3106 | `	}` |
|      25 | 3107 | `	if( iDirection != 0 ){` |
|       9 | 3108 | `		if( iDirection > 0 ){` |
|       - | 3109 | `			/* Point to the next entry */` |
|       7 | 3110 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 3111 | `			pCur = pMap->pCur;` |
|       4 | 3112 | `		}else{` |
|       - | 3113 | `			/* Point to the previous entry */` |
|       3 | 3114 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 3115 | `			pCur = pMap->pCur;` |
|       - | 3116 | `		}` |
|       9 | 3117 | `		if( pCur == 0 ){` |
|       - | 3118 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 3119 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3120 | `			return PH7_OK;` |
|       - | 3121 | `		}` |
|       4 | 3122 | `	}` |
|       - | 3123 | `	/* Point to the desired element */` |
|      25 | 3124 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 3125 | `	if( pVal ){` |
|      25 | 3126 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 3127 | `	}else{` |
|     ! 0 | 3128 | `		ph7_result_bool(pCtx,0);` |
|       - | 3129 | `	}` |
|      25 | 3130 | `	return PH7_OK;` |
|      13 | 3131 | `}` |
|       - | 3132 | `/*` |
|       - | 3133 | ` * value current(array $array)` |
|       - | 3134 | ` *  Return the current element in an array.` |
|       - | 3135 | ` * Parameter` |
|       - | 3136 | ` *  $input: The input array.` |
|       - | 3137 | ` * Return` |
|       - | 3138 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 3139 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3140 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3141 | ` *  is empty, current() returns FALSE.` |
|       - | 3142 | ` */` |
|      10 | 3143 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3144 | `{` |
|      11 | 3145 | `	if( nArg < 1 ){` |
|       - | 3146 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3147 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3148 | `		return PH7_OK;` |
|       - | 3149 | `	}` |
|       - | 3150 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 3151 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3152 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3153 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3154 | `		return PH7_OK;` |
|       - | 3155 | `	}` |
|      11 | 3156 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 3157 | `	return PH7_OK;` |
|       6 | 3158 | `}` |
|       - | 3159 | `/*` |
|       - | 3160 | ` * value next(array $input)` |
|       - | 3161 | ` *  Advance the internal array pointer of an array.` |
|       - | 3162 | ` * Parameter` |
|       - | 3163 | ` *  $input: The input array.` |
|       - | 3164 | ` * Return` |
|       - | 3165 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 3166 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 3167 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 3168 | ` */` |
|       6 | 3169 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3170 | `{` |
|       7 | 3171 | `	if( nArg < 1 ){` |
|       - | 3172 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3173 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3174 | `		return PH7_OK;` |
|       - | 3175 | `	}` |
|       - | 3176 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 3177 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3178 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3179 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3180 | `		return PH7_OK;` |
|       - | 3181 | `	}` |
|       7 | 3182 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 3183 | `	return PH7_OK;` |
|       4 | 3184 | `}` |
|       - | 3185 | `/*` |
|       - | 3186 | ` * value prev(array $input)` |
|       - | 3187 | ` *  Rewind the internal array pointer.` |
|       - | 3188 | ` * Parameter` |
|       - | 3189 | ` *  $input: The input array.` |
|       - | 3190 | ` * Return` |
|       - | 3191 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3192 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3193 | ` *  elements.` |
|       - | 3194 | ` */` |
|       2 | 3195 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3196 | `{` |
|       3 | 3197 | `	if( nArg < 1 ){` |
|       - | 3198 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3199 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3200 | `		return PH7_OK;` |
|       - | 3201 | `	}` |
|       - | 3202 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3203 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3204 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3205 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3206 | `		return PH7_OK;` |
|       - | 3207 | `	}` |
|       3 | 3208 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3209 | `	return PH7_OK;` |
|       2 | 3210 | `}` |
|       - | 3211 | `/*` |
|       - | 3212 | ` * value end(array $input)` |
|       - | 3213 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3214 | ` * Parameter` |
|       - | 3215 | ` *  $input: The input array.` |
|       - | 3216 | ` * Return` |
|       - | 3217 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3218 | ` */` |
|       2 | 3219 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3220 | `{` |
|       - | 3221 | `	ph7_hashmap *pMap;` |
|       3 | 3222 | `	if( nArg < 1 ){` |
|       - | 3223 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3224 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3225 | `		return PH7_OK;` |
|       - | 3226 | `	}` |
|       - | 3227 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3228 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3229 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3230 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3231 | `		return PH7_OK;` |
|       - | 3232 | `	}` |
|       - | 3233 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3234 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3235 | `	/* Point to the last node */` |
|       3 | 3236 | `	pMap->pCur = pMap->pLast;` |
|       - | 3237 | `	/* Return the last node value */` |
|       3 | 3238 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3239 | `	return PH7_OK;` |
|       2 | 3240 | `}` |
|       - | 3241 | `/*` |
|       - | 3242 | ` * value reset(array $array )` |
|       - | 3243 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3244 | ` * Parameter` |
|       - | 3245 | ` *  $input: The input array.` |
|       - | 3246 | ` * Return` |
|       - | 3247 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3248 | ` */` |
|       4 | 3249 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3250 | `{` |
|       - | 3251 | `	ph7_hashmap *pMap;` |
|       5 | 3252 | `	if( nArg < 1 ){` |
|       - | 3253 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3254 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3255 | `		return PH7_OK;` |
|       - | 3256 | `	}` |
|       - | 3257 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3258 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3259 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3260 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3261 | `		return PH7_OK;` |
|       - | 3262 | `	}` |
|       - | 3263 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3264 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3265 | `	/* Point to the first node */` |
|       5 | 3266 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3267 | `	/* Return the last node value if available */` |
|       5 | 3268 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3269 | `	return PH7_OK;` |
|       3 | 3270 | `}` |
|       - | 3271 | `/*` |
|       - | 3272 | ` * value key(array $array)` |
|       - | 3273 | ` *   Fetch a key from an array` |
|       - | 3274 | ` * Parameter` |
|       - | 3275 | ` *  $input` |
|       - | 3276 | ` *   The input array.` |
|       - | 3277 | ` * Return` |
|       - | 3278 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3279 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3280 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3281 | ` *  is empty, key() returns NULL.` |
|       - | 3282 | ` */` |
|       4 | 3283 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3284 | `{` |
|       - | 3285 | `	ph7_hashmap_node *pCur;` |
|       - | 3286 | `	ph7_hashmap *pMap;` |
|       5 | 3287 | `	if( nArg < 1 ){` |
|       - | 3288 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3289 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3290 | `		return PH7_OK;` |
|       - | 3291 | `	}` |
|       - | 3292 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3293 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3294 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3295 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3296 | `		return PH7_OK;` |
|       - | 3297 | `	}` |
|       5 | 3298 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3299 | `	pCur = pMap->pCur;` |
|       5 | 3300 | `	if( pCur == 0 ){` |
|       - | 3301 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3302 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3303 | `		return PH7_OK;` |
|       - | 3304 | `	}` |
|       5 | 3305 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3306 | `		/* Key is integer */` |
|     ! 0 | 3307 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3308 | `	}else{` |
|       - | 3309 | `		/* Key is blob */` |
|       7 | 3310 | `		ph7_result_string(pCtx,` |
|       4 | 3311 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3312 | `	}` |
|       5 | 3313 | `	return PH7_OK;` |
|       3 | 3314 | `}` |
|       - | 3315 | `/*` |
|       - | 3316 | ` * array each(array $input)` |
|       - | 3317 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3318 | ` * Parameter` |
|       - | 3319 | ` *  $input` |
|       - | 3320 | ` *    The input array.` |
|       - | 3321 | ` * Return` |
|       - | 3322 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3323 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3324 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3325 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3326 | ` *  each() returns FALSE.` |
|       - | 3327 | ` */` |
|      22 | 3328 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3329 | `{` |
|       - | 3330 | `	ph7_hashmap_node *pCur;` |
|       - | 3331 | `	ph7_hashmap *pMap;` |
|       - | 3332 | `	ph7_value *pArray;` |
|       - | 3333 | `	ph7_value *pVal;` |
|       - | 3334 | `	ph7_value sKey;` |
|      23 | 3335 | `	if( nArg < 1 ){` |
|       - | 3336 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3337 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3338 | `		return PH7_OK;` |
|       - | 3339 | `	}` |
|       - | 3340 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3341 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3342 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3343 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3344 | `		return PH7_OK;` |
|       - | 3345 | `	}` |
|       - | 3346 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3347 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3348 | `	if( pMap->pCur == 0 ){` |
|       - | 3349 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3350 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3351 | `		return PH7_OK;` |
|       - | 3352 | `	}` |
|      15 | 3353 | `	pCur = pMap->pCur;` |
|       - | 3354 | `	/* Create a new array */` |
|      15 | 3355 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3356 | `	if( pArray == 0 ){` |
|     ! 0 | 3357 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3358 | `		return PH7_OK;` |
|       - | 3359 | `	}` |
|      15 | 3360 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3361 | `	/* Insert the current value */` |
|      15 | 3362 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3363 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3364 | `	/* Make the key */` |
|      15 | 3365 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3366 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3367 | `	}else{` |
|       9 | 3368 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3369 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3370 | `	}` |
|       - | 3371 | `	/* Insert the current key */` |
|      15 | 3372 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3373 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3374 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3375 | `	/* Advance the cursor */` |
|      15 | 3376 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3377 | `	/* Return the current entry */` |
|      15 | 3378 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3379 | `	return PH7_OK;` |
|      12 | 3380 | `}` |
|       - | 3381 | `/*` |
|       - | 3382 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3383 | ` *  Create an array containing a range of elements` |
|       - | 3384 | ` * Parameter` |
|       - | 3385 | ` *  start` |
|       - | 3386 | ` *   First value of the sequence.` |
|       - | 3387 | ` *  limit` |
|       - | 3388 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3389 | ` *  step` |
|       - | 3390 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3391 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3392 | ` * Return` |
|       - | 3393 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3394 | ` * NOTE:` |
|       - | 3395 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3396 | ` */` |
|       2 | 3397 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3398 | `{` |
|       - | 3399 | `	ph7_value *pValue,*pArray;` |
|       - | 3400 | `	sxi64 iOfft,iLimit;` |
|       3 | 3401 | `	int iStep = 1;` |
|       - | 3402 |  |
|       3 | 3403 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3404 | `	if( nArg > 0 ){` |
|       - | 3405 | `		/* Extract the offset */` |
|       3 | 3406 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3407 | `		if( nArg > 1 ){` |
|       - | 3408 | `			/* Extract the limit */` |
|       3 | 3409 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3410 | `			if( nArg > 2 ){` |
|       - | 3411 | `				/* Extract the increment */` |
|       3 | 3412 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3413 | `				if( iStep < 1 ){` |
|       - | 3414 | `					/* Only positive number are allowed */` |
|       3 | 3415 | `					iStep = 1;` |
|       1 | 3416 | `				}` |
|       1 | 3417 | `			}` |
|       1 | 3418 | `		}` |
|       1 | 3419 | `	}` |
|       - | 3420 | `	/* Element container */` |
|       3 | 3421 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3422 | `	/* Create the new array */` |
|       3 | 3423 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3424 | `	if( pArray == 0 ){` |
|     ! 0 | 3425 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3426 | `	}` |
|       - | 3427 | `	/* Start filling */` |
|       3 | 3428 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3429 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3430 | `		/* Perform the insertion */` |
|     ! 0 | 3431 | `		if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       - | 3432 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3433 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3434 | `		}` |
|       - | 3435 | `		/* Increment */` |
|     ! 0 | 3436 | `		iOfft += iStep;` |
|     ! 0 | 3437 | `	}` |
|       - | 3438 | `	/* Return the new array */` |
|       3 | 3439 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3440 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3441 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3442 | `	 */` |
|       3 | 3443 | `	return PH7_OK;` |
|       2 | 3444 | `}` |
|       - | 3445 | `/*` |
|       - | 3446 | ` * array array_values(array $array)` |
|       - | 3447 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3448 | ` * Parameters` |
|       - | 3449 | ` *  $array` |
|       - | 3450 | ` *   The input array.` |
|       - | 3451 | ` * Return` |
|       - | 3452 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3453 | ` */` |
|      36 | 3454 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3455 | `{` |
|       - | 3456 | `	ph7_hashmap_node *pNode;` |
|       - | 3457 | `	ph7_hashmap *pMap;` |
|       - | 3458 | `	ph7_value *pArray;` |
|       - | 3459 | `	ph7_value *pObj;` |
|       - | 3460 | `	sxu32 n;` |
|      41 | 3461 | `	if( nArg != 1 ){` |
|       - | 3462 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       8 | 3463 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3464 | `			"ArgumentCountError",` |
|       - | 3465 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3466 | `			nArg` |
|       - | 3467 | `			);` |
|       - | 3468 | `	}` |
|       - | 3469 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 3470 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3471 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3472 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3473 | `			"TypeError",` |
|       - | 3474 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3475 | `			ph7_type_name(apArg[0])` |
|       - | 3476 | `			);` |
|       - | 3477 | `	}` |
|       - | 3478 | `	/* Point to the internal representation that describe the input hashmap */` |
|      32 | 3479 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3480 | `	/* Create a new array */` |
|      32 | 3481 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 3482 | `	if( pArray == 0 ){` |
|     ! 0 | 3483 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3484 | `		return PH7_OK;` |
|       - | 3485 | `	}` |
|       - | 3486 | `	/* Perform the requested operation */` |
|      32 | 3487 | `	pNode = pMap->pFirst;` |
|     104 | 3488 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      74 | 3489 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      74 | 3490 | `		if( pObj ){` |
|       - | 3491 | `			/* perform the insertion */` |
|      74 | 3492 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      36 | 3493 | `		}` |
|       - | 3494 | `		/* Point to the next entry */` |
|      74 | 3495 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      38 | 3496 | `	}` |
|       - | 3497 | `	/* return the new array */` |
|      32 | 3498 | `	ph7_result_value(pCtx,pArray);` |
|      32 | 3499 | `	return PH7_OK;` |
|      23 | 3500 | `}` |
|       - | 3501 | `/*` |
|       - | 3502 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3503 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3504 | ` * Parameters` |
|       - | 3505 | ` *  $input` |
|       - | 3506 | ` *   An array containing keys to return.` |
|       - | 3507 | ` * $search_value` |
|       - | 3508 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3509 | ` * $strict` |
|       - | 3510 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3511 | ` * Return` |
|       - | 3512 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3513 | ` */` |
|     142 | 3514 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3515 | `{` |
|       - | 3516 | `	ph7_hashmap_node *pNode;` |
|       - | 3517 | `	ph7_hashmap *pMap;` |
|       - | 3518 | `	ph7_value *pArray;` |
|       - | 3519 | `	ph7_value sObj;` |
|       - | 3520 | `	ph7_value sVal;` |
|       - | 3521 | `	SyString sKey;` |
|       - | 3522 | `	int bStrict;` |
|       - | 3523 | `	sxi32 rc;` |
|       - | 3524 | `	sxu32 n;` |
|     147 | 3525 | `	if( nArg < 1 ){` |
|       - | 3526 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3527 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3528 | `			"ArgumentCountError",` |
|       - | 3529 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3530 | `			);` |
|       - | 3531 | `	}` |
|       - | 3532 | `	/* Make sure we are dealing with a valid hashmap */` |
|     145 | 3533 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3534 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3535 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3536 | `			"TypeError",` |
|       - | 3537 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3538 | `			ph7_type_name(apArg[0])` |
|       - | 3539 | `			);` |
|       - | 3540 | `	}` |
|       - | 3541 | `	/* Point to the internal representation of the input hashmap */` |
|     143 | 3542 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3543 | `	/* Create a new array */` |
|     143 | 3544 | `	pArray = ph7_context_new_array(pCtx);` |
|     143 | 3545 | `	if( pArray == 0 ){` |
|     ! 0 | 3546 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3547 | `		return PH7_OK;` |
|       - | 3548 | `	}` |
|     143 | 3549 | `	bStrict = FALSE;` |
|     143 | 3550 | `	if( nArg > 2 ){` |
|       - | 3551 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3552 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3553 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3554 | `				"TypeError",` |
|       - | 3555 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3556 | `				ph7_type_name(apArg[2])` |
|       - | 3557 | `				);` |
|       - | 3558 | `		}` |
|       5 | 3559 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3560 | `	}` |
|       - | 3561 | `	/* Perform the requested operation */` |
|     140 | 3562 | `	pNode = pMap->pFirst;` |
|     140 | 3563 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    1130 | 3564 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     994 | 3565 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     133 | 3566 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      68 | 3567 | `		}else{` |
|     862 | 3568 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     862 | 3569 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3570 | `		}` |
|     994 | 3571 | `		rc = 0;` |
|     994 | 3572 | `		if( nArg > 1 ){` |
|      31 | 3573 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3574 | `			if( pValue ){` |
|      31 | 3575 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3576 | `				/* Filter key */` |
|      31 | 3577 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3578 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3579 | `			}` |
|      15 | 3580 | `		}` |
|     994 | 3581 | `		if( rc == 0 ){` |
|       - | 3582 | `			/* Perform the insertion */` |
|     976 | 3583 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     486 | 3584 | `		}` |
|     994 | 3585 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3586 | `		/* Point to the next entry */` |
|     994 | 3587 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     499 | 3588 | `	}` |
|       - | 3589 | `	/* return the new array */` |
|     140 | 3590 | `	ph7_result_value(pCtx,pArray);` |
|     140 | 3591 | `	return PH7_OK;` |
|      76 | 3592 | `}` |
|       - | 3593 | `/*` |
|       - | 3594 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3595 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3596 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3597 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3598 | ` * Parameters` |
|       - | 3599 | ` *  $arr1` |
|       - | 3600 | ` *   First array` |
|       - | 3601 | ` *  $arr2` |
|       - | 3602 | ` *   Second array` |
|       - | 3603 | ` * Return` |
|       - | 3604 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3605 | ` * Note` |
|       - | 3606 | ` *  This function is a symisc eXtension.` |
|       - | 3607 | ` */` |
|       4 | 3608 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3609 | `{` |
|       - | 3610 | `	ph7_hashmap *p1,*p2;` |
|       - | 3611 | `	int rc;` |
|       5 | 3612 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3613 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3614 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3615 | `		return PH7_OK;` |
|       - | 3616 | `	}` |
|       - | 3617 | `	/* Point to the hashmaps */` |
|       5 | 3618 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3619 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3620 | `	rc = (p1 == p2);` |
|       - | 3621 | `	/* Same instance? */` |
|       5 | 3622 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3623 | `	return PH7_OK;` |
|       3 | 3624 | `}` |
|       - | 3625 | `/*` |
|       - | 3626 | ` * array array_merge(array ...$arrays)` |
|       - | 3627 | ` *  Merge one or more arrays.` |
|       - | 3628 | ` * Parameters` |
|       - | 3629 | ` *  ...$arrays` |
|       - | 3630 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3631 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3632 | ` * Return` |
|       - | 3633 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3634 | ` *  with no arguments.` |
|       - | 3635 | ` */` |
|    1026 | 3636 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3637 | `{` |
|       - | 3638 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3639 | `	ph7_value *pArray;` |
|       - | 3640 | `	int i;` |
|       - | 3641 | `	/* Create a new array */` |
|    1031 | 3642 | `	pArray = ph7_context_new_array(pCtx);` |
|    1031 | 3643 | `	if( pArray == 0 ){` |
|     ! 0 | 3644 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3645 | `		return PH7_OK;` |
|       - | 3646 | `	}` |
|       - | 3647 | `	/* Point to the internal representation of the hashmap */` |
|    1031 | 3648 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3649 | `	/* Start merging */` |
|    3073 | 3650 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3651 | `		/* Make sure we are dealing with a valid hashmap */` |
|    2051 | 3652 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3653 | `			/* Type mismatch -> TypeError */` |
|       8 | 3654 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3655 | `				"TypeError",` |
|       - | 3656 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3657 | `				i + 1,` |
|       4 | 3658 | `				ph7_type_name(apArg[i])` |
|       - | 3659 | `				);` |
|     ! 0 | 3660 | `		}else{` |
|    2047 | 3661 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3662 | `			/* Merge the two hashmaps */` |
|    2047 | 3663 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3664 | `		}` |
|    1026 | 3665 | `	}` |
|       - | 3666 | `	/* Return the freshly created array */` |
|    1027 | 3667 | `	ph7_result_value(pCtx,pArray);` |
|    1027 | 3668 | `	return PH7_OK;` |
|     518 | 3669 | `}` |
|       - | 3670 | `/*` |
|       - | 3671 | ` * array array_copy(array $source)` |
|       - | 3672 | ` *  Make a blind copy of the target array.` |
|       - | 3673 | ` * Parameters` |
|       - | 3674 | ` *  $source` |
|       - | 3675 | ` *   Target array` |
|       - | 3676 | ` * Return` |
|       - | 3677 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3678 | ` * Note` |
|       - | 3679 | ` *  This function is a symisc eXtension.` |
|       - | 3680 | ` */` |
|      16 | 3681 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3682 | `{` |
|       - | 3683 | `	ph7_hashmap *pMap;` |
|       - | 3684 | `	ph7_value *pArray;` |
|      17 | 3685 | `	if( nArg < 1 ){` |
|       - | 3686 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3687 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3688 | `		return PH7_OK;` |
|       - | 3689 | `	}` |
|       - | 3690 | `	/* Create a new array */` |
|      17 | 3691 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3692 | `	if( pArray == 0 ){` |
|     ! 0 | 3693 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3694 | `		return PH7_OK;` |
|       - | 3695 | `	}` |
|       - | 3696 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3697 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3698 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3699 | `		/* Point to the internal representation of the source */` |
|      17 | 3700 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3701 | `		/* Perform the copy */` |
|      17 | 3702 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3703 | `	}else{` |
|       - | 3704 | `		/* Simple insertion */` |
|     ! 0 | 3705 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3706 | `	}` |
|       - | 3707 | `	/* Return the duplicated array */` |
|      17 | 3708 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3709 | `	return PH7_OK;` |
|       9 | 3710 | `}` |
|       - | 3711 | `/*` |
|       - | 3712 | ` * bool array_erase(array $source)` |
|       - | 3713 | ` *  Remove all elements from a given array.` |
|       - | 3714 | ` * Parameters` |
|       - | 3715 | ` *  $source` |
|       - | 3716 | ` *   Target array` |
|       - | 3717 | ` * Return` |
|       - | 3718 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3719 | ` * Note` |
|       - | 3720 | ` *  This function is a symisc eXtension.` |
|       - | 3721 | ` */` |
|      16 | 3722 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3723 | `{` |
|       - | 3724 | `	ph7_hashmap *pMap;` |
|      17 | 3725 | `	if( nArg < 1 ){` |
|       - | 3726 | `		/* Missing arguments */` |
|     ! 0 | 3727 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3728 | `		return PH7_OK;` |
|       - | 3729 | `	}` |
|       - | 3730 | `	/* Point to the target hashmap */` |
|      17 | 3731 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3732 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3733 | `	/* Erase */` |
|      17 | 3734 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3735 | `	return PH7_OK;` |
|       9 | 3736 | `}` |
|       - | 3737 | `/*` |
|       - | 3738 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3739 | ` *  Extract a slice of the array.` |
|       - | 3740 | ` * Parameters` |
|       - | 3741 | ` *  $array` |
|       - | 3742 | ` *    The input array.` |
|       - | 3743 | ` * $offset` |
|       - | 3744 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3745 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3746 | ` * $length (optional, nullable)` |
|       - | 3747 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3748 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3749 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3750 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3751 | ` * $preserve_keys (optional)` |
|       - | 3752 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3753 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3754 | ` * Return` |
|       - | 3755 | ` *   The new slice.` |
|       - | 3756 | ` */` |
|      50 | 3757 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3758 | `{` |
|       - | 3759 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3760 | `	ph7_hashmap_node *pCur;` |
|       - | 3761 | `	ph7_value *pArray;` |
|       - | 3762 | `	int iLength,iOfft;` |
|       - | 3763 | `	int bPreserve;` |
|       - | 3764 | `	sxi32 rc;` |
|      55 | 3765 | `	if( nArg < 2 ){` |
|       8 | 3766 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3767 | `			"ArgumentCountError",` |
|       - | 3768 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3769 | `			nArg` |
|       - | 3770 | `			);` |
|       - | 3771 | `	}` |
|      51 | 3772 | `	if( nArg > 4 ){` |
|       4 | 3773 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3774 | `			"ArgumentCountError",` |
|       - | 3775 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3776 | `			nArg` |
|       - | 3777 | `			);` |
|       - | 3778 | `	}` |
|      49 | 3779 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3780 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3781 | `			"TypeError",` |
|       - | 3782 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3783 | `			ph7_type_name(apArg[0])` |
|       - | 3784 | `			);` |
|       - | 3785 | `	}` |
|       - | 3786 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      62 | 3787 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      65 | 3788 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3789 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3790 | `			"TypeError",` |
|       - | 3791 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3792 | `			ph7_type_name(apArg[1])` |
|       - | 3793 | `			);` |
|       - | 3794 | `	}` |
|       - | 3795 | `	/* Validate $length type if provided: nullable int */` |
|      45 | 3796 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      26 | 3797 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3798 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3799 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3800 | `				"TypeError",` |
|       - | 3801 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3802 | `				ph7_type_name(apArg[2])` |
|       - | 3803 | `				);` |
|       - | 3804 | `		}` |
|       8 | 3805 | `	}` |
|       - | 3806 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      43 | 3807 | `	if( nArg > 3 ){` |
|      10 | 3808 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3809 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3810 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3811 | `				"TypeError",` |
|       - | 3812 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3813 | `				ph7_type_name(apArg[3])` |
|       - | 3814 | `				);` |
|       - | 3815 | `		}` |
|       2 | 3816 | `	}` |
|       - | 3817 | `	/* Point the internal representation of the target array */` |
|      41 | 3818 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      41 | 3819 | `	bPreserve = FALSE;` |
|       - | 3820 | `	/* Get the offset */` |
|      41 | 3821 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      41 | 3822 | `	if( iOfft < 0 ){` |
|       5 | 3823 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3824 | `		if( iOfft < 0 ){` |
|       3 | 3825 | `			iOfft = 0;` |
|       1 | 3826 | `		}` |
|       2 | 3827 | `	}` |
|      41 | 3828 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3829 | `		/* Offset past end of array, return empty array */` |
|       5 | 3830 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3831 | `		if( pArray == 0 ){` |
|     ! 0 | 3832 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3833 | `			return PH7_OK;` |
|       - | 3834 | `		}` |
|       5 | 3835 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3836 | `		return PH7_OK;` |
|       - | 3837 | `	}` |
|       - | 3838 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      37 | 3839 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      37 | 3840 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3841 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3842 | `		if( iLength < 0 ){` |
|       5 | 3843 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3844 | `		}` |
|      15 | 3845 | `		if( iLength < 0 ){` |
|       3 | 3846 | `			iLength = 0;` |
|       1 | 3847 | `		}` |
|      15 | 3848 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3849 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3850 | `		}` |
|       7 | 3851 | `	}` |
|      37 | 3852 | `	if( nArg > 3 ){` |
|       5 | 3853 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3854 | `	}` |
|       - | 3855 | `	/* Create a new array */` |
|      37 | 3856 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 3857 | `	if( pArray == 0 ){` |
|     ! 0 | 3858 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3859 | `		return PH7_OK;` |
|       - | 3860 | `	}` |
|      37 | 3861 | `	if( iLength < 1 ){` |
|       - | 3862 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3863 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3864 | `		return PH7_OK;` |
|       - | 3865 | `	}` |
|       - | 3866 | `	/* Point to the desired entry */` |
|      33 | 3867 | `	pCur = pSrc->pFirst;` |
|      28 | 3868 | `	for(;;){` |
|      61 | 3869 | `		if( iOfft < 1 ){` |
|      33 | 3870 | `			break;` |
|       - | 3871 | `		}` |
|       - | 3872 | `		/* Point to the next entry */` |
|      33 | 3873 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      33 | 3874 | `		iOfft--;` |
|       5 | 3875 | `	}` |
|       - | 3876 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 3877 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      51 | 3878 | `	for(;;){` |
|     107 | 3879 | `		if( iLength < 1 ){` |
|      33 | 3880 | `			break;` |
|       - | 3881 | `		}` |
|       - | 3882 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3883 | `		{` |
|      79 | 3884 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      79 | 3885 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3886 | `		}` |
|      79 | 3887 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3888 | `			break;` |
|       - | 3889 | `		}` |
|       - | 3890 | `		/* Point to the next entry */` |
|      79 | 3891 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      79 | 3892 | `		iLength--;` |
|       5 | 3893 | `	}` |
|       - | 3894 | `	/* Return the freshly created array */` |
|      33 | 3895 | `	ph7_result_value(pCtx,pArray);` |
|      33 | 3896 | `	return PH7_OK;` |
|      30 | 3897 | `}` |
|       - | 3898 | `/*` |
|       - | 3899 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3900 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3901 | ` * beginning (becomes the new pFirst).` |
|       - | 3902 | ` */` |
|      30 | 3903 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3904 | `{` |
|       - | 3905 | `	ph7_hashmap_node *pNode;` |
|       - | 3906 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3907 | `	pNode = pMap->pLast;` |
|      31 | 3908 | `	if( pNode == 0 ){` |
|     ! 0 | 3909 | `		return;` |
|       - | 3910 | `	}` |
|      31 | 3911 | `	if( pNode->pNext == 0 ){` |
|       - | 3912 | `		/* Only node in the list, nothing to move */` |
|       5 | 3913 | `		return;` |
|       - | 3914 | `	}` |
|      27 | 3915 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3916 | `		/* Already in the correct position */` |
|       9 | 3917 | `		return;` |
|       - | 3918 | `	}` |
|       - | 3919 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3920 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3921 | `	pMap->pLast->pPrev = 0;` |
|       - | 3922 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3923 | `	if( pAfter == 0 ){` |
|       - | 3924 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3925 | `		pNode->pNext = 0;` |
|       3 | 3926 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3927 | `		if( pMap->pFirst ){` |
|       3 | 3928 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3929 | `		}` |
|       3 | 3930 | `		pMap->pFirst = pNode;` |
|       2 | 3931 | `	}else{` |
|      17 | 3932 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3933 | `		pNode->pPrev = pOldNext;` |
|      17 | 3934 | `		pNode->pNext = pAfter;` |
|      17 | 3935 | `		pAfter->pPrev = pNode;` |
|      17 | 3936 | `		if( pOldNext ){` |
|      17 | 3937 | `			pOldNext->pNext = pNode;` |
|       9 | 3938 | `		}else{` |
|     ! 0 | 3939 | `			pMap->pLast = pNode;` |
|       - | 3940 | `		}` |
|       - | 3941 | `	}` |
|      16 | 3942 | `}` |
|       - | 3943 | `/*` |
|       - | 3944 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3945 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3946 | ` * Parameters` |
|       - | 3947 | ` *  $array` |
|       - | 3948 | ` *    The input array.` |
|       - | 3949 | ` *  $offset` |
|       - | 3950 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3951 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3952 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3953 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3954 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3955 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3956 | ` *  $length (optional)` |
|       - | 3957 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3958 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3959 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3960 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3961 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3962 | ` *  $replacement (optional)` |
|       - | 3963 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3964 | ` *    with elements from this array.` |
|       - | 3965 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3966 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3967 | ` *    offset.` |
|       - | 3968 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3969 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3970 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3971 | ` * Return` |
|       - | 3972 | ` *   A new array consisting of the extracted elements.` |
|       - | 3973 | ` */` |
|      54 | 3974 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3975 | `{` |
|       - | 3976 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3977 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3978 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3979 | `	int iLength,iOfft,i;` |
|       - | 3980 | `	sxi32 rc;` |
|      58 | 3981 | `	if( nArg < 2 ){` |
|       8 | 3982 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3983 | `			"ArgumentCountError",` |
|       - | 3984 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3985 | `			nArg` |
|       - | 3986 | `			);` |
|       - | 3987 | `	}` |
|      52 | 3988 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3989 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3990 | `			"TypeError",` |
|       - | 3991 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3992 | `			ph7_type_name(apArg[0])` |
|       - | 3993 | `			);` |
|       - | 3994 | `	}` |
|       - | 3995 | `	/* Point to the internal representation of the target array */` |
|      49 | 3996 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3997 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3998 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3999 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 4000 | `	if( iOfft < 0 ){` |
|       7 | 4001 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 4002 | `		if( iOfft < 0 ){` |
|       3 | 4003 | `			iOfft = 0;` |
|       2 | 4004 | `		}` |
|      46 | 4005 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 4006 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 4007 | `	}` |
|       - | 4008 | `	/* Get the length and clamp to valid range.` |
|       - | 4009 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 4010 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 4011 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 4012 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 4013 | `		if( iLength < 0 ){` |
|       7 | 4014 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 4015 | `			if( iLength < 0 ){` |
|       3 | 4016 | `				iLength = 0;` |
|       1 | 4017 | `			}` |
|       3 | 4018 | `		}` |
|      31 | 4019 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 4020 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 4021 | `		}` |
|      15 | 4022 | `	}` |
|       - | 4023 | `	/* Create the result array for removed elements */` |
|      49 | 4024 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 4025 | `	if( pArray == 0 ){` |
|     ! 0 | 4026 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4027 | `		return PH7_OK;` |
|       - | 4028 | `	}` |
|       - | 4029 | `	/* Get replacement array if provided */` |
|      49 | 4030 | `	pRep = 0;` |
|      49 | 4031 | `	if( nArg > 3 ){` |
|      21 | 4032 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 4033 | `			/* Perform an array cast */` |
|       3 | 4034 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 4035 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 4036 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 4037 | `			}` |
|       2 | 4038 | `		}else{` |
|      19 | 4039 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 4040 | `		}` |
|      21 | 4041 | `		if( pRep ){` |
|       - | 4042 | `			/* Reset the loop cursor */` |
|      21 | 4043 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 4044 | `		}` |
|      10 | 4045 | `	}` |
|       - | 4046 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 4047 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 4048 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 4049 | `		return PH7_OK;` |
|       - | 4050 | `	}` |
|       - | 4051 | `	/* Navigate to the offset position */` |
|      41 | 4052 | `	pCur = pSrc->pFirst;` |
|      85 | 4053 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 4054 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 4055 | `	}` |
|       - | 4056 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 4057 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 4058 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 4059 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 4060 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 4061 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 4062 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 4063 | `		pPrev = pCur->pPrev;` |
|      71 | 4064 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 4065 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 4066 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 4067 | `			break;` |
|       - | 4068 | `		}` |
|      71 | 4069 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 4070 | `	}` |
|       - | 4071 | `	/* Insert replacement elements at the correct position */` |
|      41 | 4072 | `	if( pRep ){` |
|       - | 4073 | `		ph7_value sSafeVal;` |
|      61 | 4074 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 4075 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 4076 | `			if( pRvalue ){` |
|       - | 4077 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 4078 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 4079 | `				 * since it points into that same pool. */` |
|      31 | 4080 | `				sSafeVal = *pRvalue;` |
|      31 | 4081 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 4082 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 4083 | `					pNewNode = pSrc->pLast;` |
|      31 | 4084 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 4085 | `					pInsertAfter = pNewNode;` |
|      15 | 4086 | `				}` |
|      15 | 4087 | `			}` |
|       1 | 4088 | `		}` |
|      10 | 4089 | `	}` |
|       - | 4090 | `	/* Return the freshly created array */` |
|      41 | 4091 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 4092 | `	return PH7_OK;` |
|      31 | 4093 | `}` |
|       - | 4094 | `/*` |
|       - | 4095 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 4096 | ` *  Checks if a value exists in an array.` |
|       - | 4097 | ` * Parameters` |
|       - | 4098 | ` *  $needle` |
|       - | 4099 | ` *   The searched value.` |
|       - | 4100 | ` *   Note:` |
|       - | 4101 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 4102 | ` * $haystack` |
|       - | 4103 | ` *  The target array.` |
|       - | 4104 | ` * $strict` |
|       - | 4105 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 4106 | ` *  will also check the types of the needle in the haystack.` |
|       - | 4107 | ` */` |
|   32278 | 4108 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4109 | `{` |
|       - | 4110 | `	ph7_value *pNeedle;` |
|       - | 4111 | `	int bStrict;` |
|       - | 4112 | `	int rc;` |
|   32283 | 4113 | `	if( nArg < 2 ){` |
|       - | 4114 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 4115 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4116 | `		return PH7_OK;` |
|       - | 4117 | `	}` |
|   32283 | 4118 | `	pNeedle = apArg[0];` |
|   32283 | 4119 | `	bStrict = 0;` |
|   32283 | 4120 | `	if( nArg > 2 ){` |
|      17 | 4121 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       8 | 4122 | `	}` |
|   32283 | 4123 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4124 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 4125 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 4126 | `		/* Set the comparison result */` |
|     ! 0 | 4127 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 4128 | `		return PH7_OK;` |
|       - | 4129 | `	}` |
|       - | 4130 | `	/* Perform the lookup */` |
|   32283 | 4131 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 4132 | `	/* Lookup result */` |
|   32283 | 4133 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   32283 | 4134 | `	return PH7_OK;` |
|   16144 | 4135 | `}` |
|       - | 4136 | `/*` |
|       - | 4137 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 4138 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 4139 | ` * Parameters` |
|       - | 4140 | ` * $needle` |
|       - | 4141 | ` *   The searched value.` |
|       - | 4142 | ` * $haystack` |
|       - | 4143 | ` *   The array.` |
|       - | 4144 | ` * $strict` |
|       - | 4145 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 4146 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 4147 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 4148 | ` * Return` |
|       - | 4149 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 4150 | ` */` |
|      28 | 4151 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4152 | `{` |
|       - | 4153 | `	ph7_hashmap_node *pEntry;` |
|       - | 4154 | `	ph7_value *pVal,sNeedle;` |
|       - | 4155 | `	ph7_hashmap *pMap;` |
|       - | 4156 | `	ph7_value sVal;` |
|       - | 4157 | `	int bStrict;` |
|       - | 4158 | `	sxu32 n;` |
|       - | 4159 | `	int rc;` |
|      33 | 4160 | `	if( nArg < 2 ){` |
|       - | 4161 | `		/* Missing argument,throw ArgumentCountError */` |
|       8 | 4162 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4163 | `			"ArgumentCountError",` |
|       - | 4164 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4165 | `			nArg` |
|       - | 4166 | `			);` |
|       - | 4167 | `	}` |
|      27 | 4168 | `	bStrict = FALSE;` |
|      27 | 4169 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4170 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4171 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4172 | `			"TypeError",` |
|       - | 4173 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4174 | `			ph7_type_name(apArg[1])` |
|       - | 4175 | `			);` |
|       - | 4176 | `	}` |
|      24 | 4177 | `	if( nArg > 2 ){` |
|       - | 4178 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4179 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4180 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4181 | `				"TypeError",` |
|       - | 4182 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4183 | `				ph7_type_name(apArg[2])` |
|       - | 4184 | `				);` |
|       - | 4185 | `		}` |
|       9 | 4186 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4187 | `	}` |
|       - | 4188 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4189 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4190 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4191 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4192 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4193 | `	pEntry = pMap->pFirst;` |
|      21 | 4194 | `	n = pMap->nEntry;` |
|      23 | 4195 | `	for(;;){` |
|      47 | 4196 | `		if( !n ){` |
|       9 | 4197 | `			break;` |
|       - | 4198 | `		}` |
|       - | 4199 | `		/* Extract node value */` |
|      39 | 4200 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4201 | `		if( pVal ){` |
|       - | 4202 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4203 | `			 * can change their type.` |
|       - | 4204 | `			 */` |
|      39 | 4205 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4206 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4207 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4208 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4209 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4210 | `			if( rc == 0 ){` |
|       - | 4211 | `				/* Match found,return key */` |
|      13 | 4212 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4213 | `					/* INT key */` |
|       7 | 4214 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4215 | `				}else{` |
|       7 | 4216 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4217 | `					/* Blob key */` |
|       7 | 4218 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4219 | `				}` |
|      13 | 4220 | `				return PH7_OK;` |
|       - | 4221 | `			}` |
|      13 | 4222 | `		}` |
|       - | 4223 | `		/* Point to the next entry */` |
|      27 | 4224 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4225 | `		n--;` |
|       1 | 4226 | `	}` |
|       - | 4227 | `	/* No such value,return FALSE */` |
|       9 | 4228 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4229 | `	return PH7_OK;` |
|      19 | 4230 | `}` |
|       - | 4231 | `/*` |
|       - | 4232 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4233 | ` *  Computes the difference of arrays.` |
|       - | 4234 | ` * Parameters` |
|       - | 4235 | ` *  $array1` |
|       - | 4236 | ` *    The array to compare from` |
|       - | 4237 | ` *  $array2` |
|       - | 4238 | ` *    An array to compare against` |
|       - | 4239 | ` *  $...` |
|       - | 4240 | ` *   More arrays to compare against` |
|       - | 4241 | ` * Return` |
|       - | 4242 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4243 | ` *  are not present in any of the other arrays.` |
|       - | 4244 | ` */` |
|      22 | 4245 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4246 | `{` |
|       - | 4247 | `	ph7_hashmap_node *pEntry;` |
|       - | 4248 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4249 | `	ph7_value *pArray;` |
|       - | 4250 | `	ph7_value *pVal;` |
|       - | 4251 | `	sxi32 rc;` |
|       - | 4252 | `	sxu32 n;` |
|       - | 4253 | `	int i;` |
|       - | 4254 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4255 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4256 | `	 * debugging difficult. */` |
|      26 | 4257 | `	if( nArg < 1 ){` |
|       4 | 4258 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4259 | `			"ArgumentCountError",` |
|       - | 4260 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4261 | `			nArg` |
|       - | 4262 | `			);` |
|       - | 4263 | `	}` |
|      23 | 4264 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4265 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4266 | `			"TypeError",` |
|       - | 4267 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4268 | `			ph7_type_name(apArg[0])` |
|       - | 4269 | `			);` |
|       - | 4270 | `	}` |
|      36 | 4271 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4272 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4273 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4274 | `				"TypeError",` |
|       - | 4275 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4276 | `				i + 1,` |
|       2 | 4277 | `				ph7_type_name(apArg[i])` |
|       - | 4278 | `				);` |
|       - | 4279 | `		}` |
|       9 | 4280 | `	}` |
|      17 | 4281 | `	if( nArg == 1 ){` |
|       - | 4282 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4283 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4284 | `		return PH7_OK;` |
|       - | 4285 | `	}` |
|       - | 4286 | `	/* Create a new array */` |
|      15 | 4287 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4288 | `	if( pArray == 0 ){` |
|     ! 0 | 4289 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4290 | `		return PH7_OK;` |
|       - | 4291 | `	}` |
|       - | 4292 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4293 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4294 | `	/* Perform the diff */` |
|      15 | 4295 | `	pEntry = pSrc->pFirst;` |
|      15 | 4296 | `	n = pSrc->nEntry;` |
|      27 | 4297 | `	for(;;){` |
|      55 | 4298 | `		if( n < 1 ){` |
|      15 | 4299 | `			break;` |
|       - | 4300 | `		}` |
|       - | 4301 | `		/* Extract the node value */` |
|      41 | 4302 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4303 | `		if( pVal ){` |
|      69 | 4304 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4305 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4306 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4307 | `				/* Perform the lookup */` |
|      45 | 4308 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4309 | `				if( rc == SXRET_OK ){` |
|       - | 4310 | `					/* Value exist */` |
|      17 | 4311 | `					break;` |
|       - | 4312 | `				}` |
|      15 | 4313 | `			}` |
|      41 | 4314 | `			if( i >= nArg ){` |
|       - | 4315 | `				/* Perform the insertion */` |
|      25 | 4316 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4317 | `			}` |
|      20 | 4318 | `		}` |
|       - | 4319 | `		/* Point to the next entry */` |
|      41 | 4320 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4321 | `		n--;` |
|       1 | 4322 | `	}` |
|       - | 4323 | `	/* Return the freshly created array */` |
|      15 | 4324 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4325 | `	return PH7_OK;` |
|      15 | 4326 | `}` |
|       - | 4327 | `/*` |
|       - | 4328 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4329 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4330 | ` * Parameters` |
|       - | 4331 | ` *  $array1` |
|       - | 4332 | ` *    The array to compare from` |
|       - | 4333 | ` *  $array2` |
|       - | 4334 | ` *    An array to compare against` |
|       - | 4335 | ` *  $...` |
|       - | 4336 | ` *   More arrays to compare against.` |
|       - | 4337 | ` * $callback` |
|       - | 4338 | ` *  The callback comparison function.` |
|       - | 4339 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4340 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4341 | ` *  than the second.` |
|       - | 4342 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4343 | ` * Return` |
|       - | 4344 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4345 | ` *  are not present in any of the other arrays.` |
|       - | 4346 | ` */` |
|      22 | 4347 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4348 | `{` |
|       - | 4349 | `	ph7_hashmap_node *pEntry;` |
|       - | 4350 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4351 | `	ph7_value *pCallback;` |
|       - | 4352 | `	ph7_value *pArray;` |
|       - | 4353 | `	ph7_value *pVal;` |
|       - | 4354 | `	sxi32 rc;` |
|       - | 4355 | `	sxu32 n;` |
|       - | 4356 | `	int i;` |
|       - | 4357 |  |
|       - | 4358 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      27 | 4359 | `	if( nArg < 2 ){` |
|       4 | 4360 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4361 | `			"ArgumentCountError",` |
|       - | 4362 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4363 | `			nArg` |
|       - | 4364 | `			);` |
|       - | 4365 | `	}` |
|      25 | 4366 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4367 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4368 | `			"TypeError",` |
|       - | 4369 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4370 | `			ph7_type_name(apArg[0])` |
|       - | 4371 | `			);` |
|       - | 4372 | `	}` |
|       - | 4373 |  |
|      23 | 4374 | `	if( nArg == 2 ){` |
|       - | 4375 | `		/* Only the original array and the callback were provided. */` |
|       - | 4376 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4377 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4378 | `		 * validation order.` |
|       - | 4379 | `		 */` |
|       4 | 4380 | `	} else {` |
|       - | 4381 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      27 | 4382 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      19 | 4383 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      11 | 4384 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4385 | `					"TypeError",` |
|       - | 4386 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4387 | `					i + 1,` |
|       6 | 4388 | `					ph7_type_name(apArg[i])` |
|       - | 4389 | `					);` |
|       - | 4390 | `			}` |
|       7 | 4391 | `		}` |
|       - | 4392 | `	}` |
|       - | 4393 |  |
|       - | 4394 | `	/* Identify the callback (always expected as the last argument). */` |
|      16 | 4395 | `	pCallback = apArg[nArg - 1];` |
|       - | 4396 | `	/* Validate the callback to match PHP's error messages. */` |
|      16 | 4397 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       9 | 4398 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4399 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4400 | `				"TypeError",` |
|       - | 4401 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4402 | `				nArg` |
|       - | 4403 | `				);` |
|       - | 4404 | `		}` |
|       6 | 4405 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4406 | `			int len;` |
|       3 | 4407 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4408 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4409 | `				"TypeError",` |
|       - | 4410 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4411 | `				nArg,` |
|       1 | 4412 | `				zName` |
|       - | 4413 | `				);` |
|       - | 4414 | `		}` |
|       4 | 4415 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4416 | `			"TypeError",` |
|       - | 4417 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4418 | `			nArg` |
|       - | 4419 | `			);` |
|       - | 4420 | `	}` |
|       - | 4421 |  |
|       7 | 4422 | `	if( nArg == 2 ){` |
|       - | 4423 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4424 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4425 | `		return PH7_OK;` |
|       - | 4426 | `	}` |
|       - | 4427 |  |
|       - | 4428 | `	/* Create a new array */` |
|       5 | 4429 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4430 | `	if( pArray == 0 ){` |
|     ! 0 | 4431 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4432 | `		return PH7_OK;` |
|       - | 4433 | `	}` |
|       - | 4434 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 4435 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4436 | `	/* Perform the diff */` |
|       5 | 4437 | `	pEntry = pSrc->pFirst;` |
|       5 | 4438 | `	n = pSrc->nEntry;` |
|       5 | 4439 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 4440 | `	for(;;){` |
|      11 | 4441 | `		if( n < 1 ){` |
|       3 | 4442 | `			break;` |
|       - | 4443 | `		}` |
|       - | 4444 | `		/* Extract the node value */` |
|       9 | 4445 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4446 | `		if( pVal ){` |
|      15 | 4447 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4448 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4449 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4450 | `				/* Perform the lookup */` |
|       9 | 4451 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       9 | 4452 | `				if( rc == SXRET_OK ){` |
|       - | 4453 | `					/* Value exist */` |
|       3 | 4454 | `					break;` |
|       - | 4455 | `				}` |
|       4 | 4456 | `			}` |
|       9 | 4457 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 4458 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 4459 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 4460 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 4461 | `				return PH7_EXCEPTION;` |
|       - | 4462 | `			}` |
|       7 | 4463 | `			if( i >= (nArg - 1)){` |
|       - | 4464 | `				/* Perform the insertion */` |
|       5 | 4465 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4466 | `			}` |
|       3 | 4467 | `		}` |
|       - | 4468 | `		/* Point to the next entry */` |
|       7 | 4469 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4470 | `		n--;` |
|       1 | 4471 | `	}` |
|       - | 4472 | `	/* Return the freshly created array */` |
|       3 | 4473 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4474 | `	return PH7_OK;` |
|      16 | 4475 | `}` |
|       - | 4476 | `/*` |
|       - | 4477 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4478 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4479 | ` * Parameters` |
|       - | 4480 | ` *  $array1` |
|       - | 4481 | ` *    The array to compare from` |
|       - | 4482 | ` *  $array2` |
|       - | 4483 | ` *    An array to compare against` |
|       - | 4484 | ` *  $...` |
|       - | 4485 | ` *   More arrays to compare against` |
|       - | 4486 | ` * Return` |
|       - | 4487 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4488 | ` *  are not present in any of the other arrays.` |
|       - | 4489 | ` */` |
|      20 | 4490 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4491 | `{` |
|       - | 4492 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4493 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4494 | `	ph7_value *pArray;` |
|       - | 4495 | `	ph7_value *pVal;` |
|       - | 4496 | `	sxi32 rc;` |
|       - | 4497 | `	sxu32 n;` |
|       - | 4498 | `	int i;` |
|       - | 4499 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4500 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4501 | `	 * accompanying integration tests to pass. */` |
|      25 | 4502 | `	if( nArg < 1 ){` |
|       4 | 4503 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4504 | `			"ArgumentCountError",` |
|       - | 4505 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4506 | `			nArg` |
|       - | 4507 | `			);` |
|       - | 4508 | `	}` |
|      22 | 4509 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4510 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4511 | `			"TypeError",` |
|       - | 4512 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4513 | `			ph7_type_name(apArg[0])` |
|       - | 4514 | `			);` |
|       - | 4515 | `	}` |
|      33 | 4516 | `	for(i = 1 ; i < nArg ; i++){` |
|      21 | 4517 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 4518 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4519 | `				"TypeError",` |
|       - | 4520 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4521 | `				i + 1,` |
|       4 | 4522 | `				ph7_type_name(apArg[i])` |
|       - | 4523 | `				);` |
|       - | 4524 | `		}` |
|       9 | 4525 | `	}` |
|      13 | 4526 | `	if( nArg == 1 ){` |
|       - | 4527 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4528 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4529 | `		return PH7_OK;` |
|       - | 4530 | `	}` |
|       - | 4531 | `	/* Create a new array */` |
|      11 | 4532 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4533 | `	if( pArray == 0 ){` |
|     ! 0 | 4534 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4535 | `		return PH7_OK;` |
|       - | 4536 | `	}` |
|       - | 4537 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4538 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4539 | `	/* Perform the diff */` |
|      11 | 4540 | `	pEntry = pSrc->pFirst;` |
|      11 | 4541 | `	n = pSrc->nEntry;` |
|      11 | 4542 | `	pN1 = pN2 = 0;` |
|      29 | 4543 | `	for(;;){` |
|       - | 4544 | `		int keep;` |
|      35 | 4545 | `		if( n < 1 ){` |
|      11 | 4546 | `			break;` |
|       - | 4547 | `		}` |
|       - | 4548 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4549 | `		keep = 1;` |
|      41 | 4550 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4551 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4552 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4553 | `			/* Perform a key lookup first */` |
|      29 | 4554 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4555 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4556 | `			}else{` |
|      17 | 4557 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4558 | `			}` |
|      29 | 4559 | `			if( rc != SXRET_OK ){` |
|       - | 4560 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4561 | `				continue;` |
|       - | 4562 | `			}` |
|       - | 4563 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4564 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4565 | `			if( pVal ){` |
|       - | 4566 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4567 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4568 | `				if( pVal2 ){` |
|      15 | 4569 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4570 | `					if( cmp == 0 ){` |
|       - | 4571 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4572 | `						keep = 0;` |
|      13 | 4573 | `						break;` |
|       - | 4574 | `					}` |
|       1 | 4575 | `				}` |
|       1 | 4576 | `			}` |
|       2 | 4577 | `		}` |
|      25 | 4578 | `		if( keep ){` |
|       - | 4579 | `			/* Perform the insertion */` |
|      13 | 4580 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4581 | `		}` |
|       - | 4582 | `		/* Point to the next entry */` |
|      25 | 4583 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4584 | `		n--;` |
|       1 | 4585 | `	}` |
|       - | 4586 | `	/* Return the freshly created array */` |
|      11 | 4587 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4588 | `	return PH7_OK;` |
|      15 | 4589 | `}` |
|       - | 4590 | `/*` |
|       - | 4591 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4592 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4593 | ` *  by a user supplied callback function.` |
|       - | 4594 | ` * Parameters` |
|       - | 4595 | ` *  $array1` |
|       - | 4596 | ` *    The array to compare from` |
|       - | 4597 | ` *  $array2` |
|       - | 4598 | ` *    An array to compare against` |
|       - | 4599 | ` *  $...` |
|       - | 4600 | ` *   More arrays to compare against.` |
|       - | 4601 | ` *  $key_compare_func` |
|       - | 4602 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4603 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4604 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4605 | ` * Return` |
|       - | 4606 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4607 | ` *  are not present in any of the other arrays.` |
|       - | 4608 | ` */` |
|      24 | 4609 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4610 | `{` |
|       - | 4611 | `	ph7_hashmap_node *pEntry;` |
|       - | 4612 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4613 | `	ph7_value *pCallback;` |
|       - | 4614 | `	ph7_value *pArray;` |
|       - | 4615 | `	sxi32 rc;` |
|       - | 4616 | `	sxu32 n;` |
|       - | 4617 | `	int i;` |
|       - | 4618 |  |
|       - | 4619 | `	/* Argument validation mimicking PHP errors. */` |
|      29 | 4620 | `	if( nArg < 2 ){` |
|       4 | 4621 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4622 | `			"ArgumentCountError",` |
|       - | 4623 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4624 | `			nArg` |
|       - | 4625 | `			);` |
|       - | 4626 | `	}` |
|      26 | 4627 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4628 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4629 | `			"TypeError",` |
|       - | 4630 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4631 | `			ph7_type_name(apArg[0])` |
|       - | 4632 | `			);` |
|       - | 4633 | `	}` |
|       - | 4634 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4635 | `	 * expected to be a callback. */` |
|      38 | 4636 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      19 | 4637 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4638 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4639 | `				"TypeError",` |
|       - | 4640 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4641 | `				i + 1,` |
|       2 | 4642 | `				ph7_type_name(apArg[i])` |
|       - | 4643 | `				);` |
|       - | 4644 | `		}` |
|       9 | 4645 | `	}` |
|       - | 4646 | `	/* Point to the callback value */` |
|      22 | 4647 | `	pCallback = apArg[nArg - 1];` |
|      22 | 4648 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4649 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4650 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4651 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4652 | `		 * string given" which we also reproduce. */` |
|       9 | 4653 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4654 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4655 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4656 | `				"TypeError",` |
|       - | 4657 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4658 | `				nArg` |
|       - | 4659 | `				);` |
|       - | 4660 | `		}` |
|       6 | 4661 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4662 | `			/* neither array nor string */` |
|       8 | 4663 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4664 | `				"TypeError",` |
|       - | 4665 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4666 | `				nArg` |
|       - | 4667 | `				);` |
|       - | 4668 | `		}` |
|       - | 4669 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4670 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4671 | `			"TypeError",` |
|       - | 4672 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4673 | `			nArg,` |
|     ! 0 | 4674 | `			ph7_type_name(pCallback)` |
|       - | 4675 | `			);` |
|       - | 4676 | `	}` |
|      13 | 4677 | `	if( nArg == 2 ){` |
|       - | 4678 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4679 | `		 * input array. */` |
|       3 | 4680 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4681 | `		return PH7_OK;` |
|       - | 4682 | `	}` |
|       - | 4683 | `	/* Create a new array */` |
|      11 | 4684 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4685 | `	if( pArray == 0 ){` |
|     ! 0 | 4686 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4687 | `		return PH7_OK;` |
|       - | 4688 | `	}` |
|       - | 4689 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4690 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4691 | `	/* Perform the diff */` |
|      11 | 4692 | `	pEntry = pSrc->pFirst;` |
|      11 | 4693 | `	n = pSrc->nEntry;` |
|      21 | 4694 | `	for(;;){` |
|       - | 4695 | `		int keep;` |
|      27 | 4696 | `		if( n < 1 ){` |
|       9 | 4697 | `			break;` |
|       - | 4698 | `		}` |
|      19 | 4699 | `		keep = 1;` |
|      31 | 4700 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4701 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 4702 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4703 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 4704 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 4705 | `			while( pIt ){` |
|       - | 4706 | `				/* build temporary key values for callback */` |
|       - | 4707 | `				ph7_value key1, key2, result;` |
|       - | 4708 | `				/* initialise only once using the appropriate helper */` |
|      33 | 4709 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4710 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4711 | `				}else{` |
|       - | 4712 | `					SyString sStr;` |
|      33 | 4713 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4714 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4715 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 4716 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4717 | `				}` |
|      33 | 4718 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4719 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4720 | `				}else{` |
|       - | 4721 | `					SyString sStr;` |
|      33 | 4722 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4723 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4724 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 4725 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4726 | `				}` |
|      33 | 4727 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4728 | `				/* call user callback with (key1, key2) */` |
|       - | 4729 | `				{` |
|       - | 4730 | `					ph7_value *apK[2];` |
|      33 | 4731 | `					apK[0] = &key1;` |
|      33 | 4732 | `					apK[1] = &key2;` |
|      33 | 4733 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4734 | `				}` |
|      33 | 4735 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 4736 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 4737 | `					 * array_uintersect (which signal back from` |
|       - | 4738 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 4739 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 4740 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 4741 | `					PH7_MemObjRelease(&result);` |
|       3 | 4742 | `					PH7_MemObjRelease(&key1);` |
|       3 | 4743 | `					PH7_MemObjRelease(&key2);` |
|       3 | 4744 | `					return PH7_EXCEPTION;` |
|       - | 4745 | `				}` |
|      31 | 4746 | `				if( rc == SXRET_OK ){` |
|      31 | 4747 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4748 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4749 | `					}` |
|      31 | 4750 | `					if( result.x.iVal == 0 ){` |
|       - | 4751 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4752 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4753 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4754 | `						if( pVal1 && pVal2 ){` |
|      13 | 4755 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4756 | `								keep = 0;` |
|       9 | 4757 | `								PH7_MemObjRelease(&result);` |
|       - | 4758 | `								/* release keys too before breaking */` |
|       9 | 4759 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4760 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4761 | `								break;` |
|       - | 4762 | `							}` |
|       2 | 4763 | `						}` |
|       2 | 4764 | `					}` |
|      11 | 4765 | `				}` |
|      23 | 4766 | `				PH7_MemObjRelease(&result);` |
|      23 | 4767 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4768 | `				PH7_MemObjRelease(&key2);` |
|       - | 4769 | `				/* move to next node */` |
|      23 | 4770 | `				pIt = pIt->pPrev;` |
|      23 | 4771 | `				if( keep == 0 ) break;` |
|       1 | 4772 | `			}` |
|      21 | 4773 | `			if( keep == 0 ) break;` |
|       7 | 4774 | `		}` |
|      17 | 4775 | `		if( keep ){` |
|       - | 4776 | `			/* Perform the insertion */` |
|       9 | 4777 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4778 | `		}` |
|       - | 4779 | `		/* Point to the next entry */` |
|      17 | 4780 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4781 | `		n--;` |
|       1 | 4782 | `	}` |
|       - | 4783 | `	/* Return the freshly created array */` |
|       9 | 4784 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4785 | `	return PH7_OK;` |
|      17 | 4786 | `}` |
|       - | 4787 | `/*` |
|       - | 4788 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4789 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4790 | ` * Parameters` |
|       - | 4791 | ` *  $array1` |
|       - | 4792 | ` *    The array to compare from` |
|       - | 4793 | ` *  $array2` |
|       - | 4794 | ` *    An array to compare against` |
|       - | 4795 | ` *  $...` |
|       - | 4796 | ` *   More arrays to compare against` |
|       - | 4797 | ` * Return` |
|       - | 4798 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4799 | ` *  in any of the other arrays.` |
|       - | 4800 | ` * Note that NULL is returned on failure.` |
|       - | 4801 | ` */` |
|      14 | 4802 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4803 | `{` |
|       - | 4804 | `	ph7_hashmap_node *pEntry;` |
|       - | 4805 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4806 | `	ph7_value *pArray;` |
|       - | 4807 | `	sxi32 rc;` |
|       - | 4808 | `	sxu32 n;` |
|       - | 4809 | `	int i;` |
|       - | 4810 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4811 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4812 | `	 * helpers. */` |
|      18 | 4813 | `	if( nArg < 1 ){` |
|       4 | 4814 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4815 | `			"ArgumentCountError",` |
|       - | 4816 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4817 | `			nArg` |
|       - | 4818 | `			);` |
|       - | 4819 | `	}` |
|      15 | 4820 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4821 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4822 | `			"TypeError",` |
|       - | 4823 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4824 | `			ph7_type_name(apArg[0])` |
|       - | 4825 | `			);` |
|       - | 4826 | `	}` |
|      20 | 4827 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4828 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4829 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4830 | `				"TypeError",` |
|       - | 4831 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4832 | `				i + 1,` |
|       2 | 4833 | `				ph7_type_name(apArg[i])` |
|       - | 4834 | `				);` |
|       - | 4835 | `		}` |
|       5 | 4836 | `	}` |
|       9 | 4837 | `	if( nArg == 1 ){` |
|       - | 4838 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4839 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4840 | `		return PH7_OK;` |
|       - | 4841 | `	}` |
|       - | 4842 | `	/* Create a new array */` |
|       7 | 4843 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4844 | `	if( pArray == 0 ){` |
|     ! 0 | 4845 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4846 | `		return PH7_OK;` |
|       - | 4847 | `	}` |
|       - | 4848 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4849 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4850 | `	/* Perfrom the diff */` |
|       7 | 4851 | `	pEntry = pSrc->pFirst;` |
|       7 | 4852 | `	n = pSrc->nEntry;` |
|      12 | 4853 | `	for(;;){` |
|      25 | 4854 | `		if( n < 1 ){` |
|       7 | 4855 | `			break;` |
|       - | 4856 | `		}` |
|      31 | 4857 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4858 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4859 | `				/* ignore */` |
|     ! 0 | 4860 | `				continue;` |
|       - | 4861 | `			}` |
|      23 | 4862 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4863 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4864 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4865 | `				/* Blob lookup */` |
|      17 | 4866 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4867 | `			}else{` |
|       - | 4868 | `				/* Int lookup */` |
|       7 | 4869 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4870 | `			}` |
|      23 | 4871 | `			if( rc == SXRET_OK ){` |
|       - | 4872 | `				/* Key exists,break immediately */` |
|      11 | 4873 | `				break;` |
|       - | 4874 | `			}` |
|       7 | 4875 | `		}` |
|      19 | 4876 | `		if( i >= nArg ){` |
|       - | 4877 | `			/* Perform the insertion */` |
|       9 | 4878 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4879 | `		}` |
|       - | 4880 | `		/* Point to the next entry */` |
|      19 | 4881 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4882 | `		n--;` |
|       1 | 4883 | `	}` |
|       - | 4884 | `	/* Return the freshly created array */` |
|       7 | 4885 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4886 | `	return PH7_OK;` |
|      11 | 4887 | `}` |
|       - | 4888 | `/*` |
|       - | 4889 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4890 | ` *  Computes the intersection of arrays.` |
|       - | 4891 | ` * Parameters` |
|       - | 4892 | ` *  $array1` |
|       - | 4893 | ` *    The array to compare from` |
|       - | 4894 | ` *  $array2` |
|       - | 4895 | ` *    An array to compare against` |
|       - | 4896 | ` *  $...` |
|       - | 4897 | ` *   More arrays to compare against` |
|       - | 4898 | ` * Return` |
|       - | 4899 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4900 | ` *  in all of the parameters.` |
|       - | 4901 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4902 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4903 | ` */` |
|      22 | 4904 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4905 | `{` |
|       - | 4906 | `	ph7_hashmap_node *pEntry;` |
|       - | 4907 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4908 | `	ph7_value *pArray;` |
|       - | 4909 | `	ph7_value *pVal;` |
|       - | 4910 | `	sxi32 rc;` |
|       - | 4911 | `	sxu32 n;` |
|       - | 4912 | `	int i;` |
|      26 | 4913 | `	if( nArg < 1 ){` |
|       4 | 4914 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4915 | `			"ArgumentCountError",` |
|       - | 4916 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4917 | `			nArg` |
|       - | 4918 | `			);` |
|       - | 4919 | `	}` |
|      23 | 4920 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4921 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4922 | `			"TypeError",` |
|       - | 4923 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4924 | `			ph7_type_name(apArg[0])` |
|       - | 4925 | `			);` |
|       - | 4926 | `	}` |
|      36 | 4927 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4928 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4929 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4930 | `				"TypeError",` |
|       - | 4931 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4932 | `				i + 1,` |
|       2 | 4933 | `				ph7_type_name(apArg[i])` |
|       - | 4934 | `				);` |
|       - | 4935 | `		}` |
|       9 | 4936 | `	}` |
|      17 | 4937 | `	if( nArg == 1 ){` |
|       - | 4938 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4939 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4940 | `		return PH7_OK;` |
|       - | 4941 | `	}` |
|       - | 4942 | `	/* Create a new array */` |
|      15 | 4943 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4944 | `	if( pArray == 0 ){` |
|     ! 0 | 4945 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4946 | `		return PH7_OK;` |
|       - | 4947 | `	}` |
|       - | 4948 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4949 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4950 | `	/* Perform the intersection */` |
|      15 | 4951 | `	pEntry = pSrc->pFirst;` |
|      15 | 4952 | `	n = pSrc->nEntry;` |
|      31 | 4953 | `	for(;;){` |
|      63 | 4954 | `		if( n < 1 ){` |
|      15 | 4955 | `			break;` |
|       - | 4956 | `		}` |
|       - | 4957 | `		/* Extract the node value */` |
|      49 | 4958 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4959 | `		if( pVal ){` |
|      79 | 4960 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4961 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4962 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4963 | `				/* Perform the lookup */` |
|      55 | 4964 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4965 | `				if( rc != SXRET_OK ){` |
|       - | 4966 | `					/* Value does not exist */` |
|      25 | 4967 | `					break;` |
|       - | 4968 | `				}` |
|      16 | 4969 | `			}` |
|      49 | 4970 | `			if( i >= nArg ){` |
|       - | 4971 | `				/* Perform the insertion */` |
|      25 | 4972 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4973 | `			}` |
|      24 | 4974 | `		}` |
|       - | 4975 | `		/* Point to the next entry */` |
|      49 | 4976 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4977 | `		n--;` |
|       1 | 4978 | `	}` |
|       - | 4979 | `	/* Return the freshly created array */` |
|      15 | 4980 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4981 | `	return PH7_OK;` |
|      15 | 4982 | `}` |
|       - | 4983 | `/*` |
|       - | 4984 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4985 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4986 | ` * Parameters` |
|       - | 4987 | ` *  $array1` |
|       - | 4988 | ` *    The array to compare from` |
|       - | 4989 | ` *  $array2` |
|       - | 4990 | ` *    An array to compare against` |
|       - | 4991 | ` *  $...` |
|       - | 4992 | ` *   More arrays to compare against` |
|       - | 4993 | ` * Return` |
|       - | 4994 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4995 | ` *  in all the arguments, with matching keys.` |
|       - | 4996 | ` */` |
|      22 | 4997 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4998 | `{` |
|       - | 4999 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 5000 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5001 | `	ph7_value *pArray;` |
|       - | 5002 | `	ph7_value *pVal;` |
|       - | 5003 | `	sxi32 rc;` |
|       - | 5004 | `	sxu32 n;` |
|       - | 5005 | `	int i;` |
|      26 | 5006 | `	if( nArg < 1 ){` |
|       4 | 5007 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5008 | `			"ArgumentCountError",` |
|       - | 5009 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 5010 | `			nArg` |
|       - | 5011 | `			);` |
|       - | 5012 | `	}` |
|      23 | 5013 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5014 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5015 | `			"TypeError",` |
|       - | 5016 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5017 | `			ph7_type_name(apArg[0])` |
|       - | 5018 | `			);` |
|       - | 5019 | `	}` |
|      36 | 5020 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5021 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5022 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5023 | `				"TypeError",` |
|       - | 5024 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 5025 | `				i + 1,` |
|       2 | 5026 | `				ph7_type_name(apArg[i])` |
|       - | 5027 | `				);` |
|       - | 5028 | `		}` |
|       9 | 5029 | `	}` |
|      17 | 5030 | `	if( nArg == 1 ){` |
|       - | 5031 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5032 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5033 | `		return PH7_OK;` |
|       - | 5034 | `	}` |
|       - | 5035 | `	/* Create a new array */` |
|      15 | 5036 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5037 | `	if( pArray == 0 ){` |
|     ! 0 | 5038 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5039 | `		return PH7_OK;` |
|       - | 5040 | `	}` |
|       - | 5041 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 5042 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5043 | `	/* Perform the intersection */` |
|      15 | 5044 | `	pEntry = pSrc->pFirst;` |
|      15 | 5045 | `	n = pSrc->nEntry;` |
|      15 | 5046 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 5047 | `	for(;;){` |
|      47 | 5048 | `		if( n < 1 ){` |
|      15 | 5049 | `			break;` |
|       - | 5050 | `		}` |
|       - | 5051 | `		/* Extract the node value */` |
|      33 | 5052 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 5053 | `		if( pVal ){` |
|      53 | 5054 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 5055 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 5056 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5057 | `				/* Perform a key lookup first */` |
|      37 | 5058 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 5059 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 5060 | `				}else{` |
|      23 | 5061 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 5062 | `				}` |
|      37 | 5063 | `				if( rc != SXRET_OK ){` |
|       - | 5064 | `					/* No such key,break immediately */` |
|       7 | 5065 | `					break;` |
|       - | 5066 | `				}` |
|       - | 5067 | `				/* Perform the lookup */` |
|      31 | 5068 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 5069 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 5070 | `					/* Value does not exist */` |
|       6 | 5071 | `					break;` |
|       - | 5072 | `				}` |
|      11 | 5073 | `			}` |
|      33 | 5074 | `			if( i >= nArg ){` |
|       - | 5075 | `				/* Perform the insertion */` |
|      17 | 5076 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5077 | `			}` |
|      16 | 5078 | `		}` |
|       - | 5079 | `		/* Point to the next entry */` |
|      33 | 5080 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5081 | `		n--;` |
|       1 | 5082 | `	}` |
|       - | 5083 | `	/* Return the freshly created array */` |
|      15 | 5084 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5085 | `	return PH7_OK;` |
|      15 | 5086 | `}` |
|       - | 5087 | `/*` |
|       - | 5088 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 5089 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 5090 | ` * Parameters` |
|       - | 5091 | ` *  $array1` |
|       - | 5092 | ` *    The array to compare from` |
|       - | 5093 | ` *  $...` |
|       - | 5094 | ` *   More arrays to compare against` |
|       - | 5095 | ` * Return` |
|       - | 5096 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 5097 | ` *  have keys that are present in all arguments.` |
|       - | 5098 | ` * Note that NULL is returned on failure.` |
|       - | 5099 | ` */` |
|      22 | 5100 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5101 | `{` |
|       - | 5102 | `	ph7_hashmap_node *pEntry;` |
|       - | 5103 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5104 | `	ph7_value *pArray;` |
|       - | 5105 | `	sxi32 rc;` |
|       - | 5106 | `	sxu32 n;` |
|       - | 5107 | `	int i;` |
|      26 | 5108 | `	if( nArg < 1 ){` |
|       4 | 5109 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5110 | `			"ArgumentCountError",` |
|       - | 5111 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 5112 | `			nArg` |
|       - | 5113 | `			);` |
|       - | 5114 | `	}` |
|      23 | 5115 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5116 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5117 | `			"TypeError",` |
|       - | 5118 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5119 | `			ph7_type_name(apArg[0])` |
|       - | 5120 | `			);` |
|       - | 5121 | `	}` |
|      36 | 5122 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5123 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5124 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5125 | `				"TypeError",` |
|       - | 5126 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5127 | `				i + 1,` |
|       2 | 5128 | `				ph7_type_name(apArg[i])` |
|       - | 5129 | `				);` |
|       - | 5130 | `		}` |
|       9 | 5131 | `	}` |
|      17 | 5132 | `	if( nArg == 1 ){` |
|       - | 5133 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5134 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5135 | `		return PH7_OK;` |
|       - | 5136 | `	}` |
|       - | 5137 | `	/* Create a new array */` |
|      15 | 5138 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5139 | `	if( pArray == 0 ){` |
|     ! 0 | 5140 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5141 | `		return PH7_OK;` |
|       - | 5142 | `	}` |
|       - | 5143 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 5144 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5145 | `	/* Perform the intersection */` |
|      15 | 5146 | `	pEntry = pSrc->pFirst;` |
|      15 | 5147 | `	n = pSrc->nEntry;` |
|      24 | 5148 | `	for(;;){` |
|      49 | 5149 | `		if( n < 1 ){` |
|      15 | 5150 | `			break;` |
|       - | 5151 | `		}` |
|      57 | 5152 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 5153 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 5154 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 5155 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5156 | `				/* Blob lookup */` |
|      27 | 5157 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 5158 | `			}else{` |
|       - | 5159 | `				/* Int key */` |
|      13 | 5160 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5161 | `			}` |
|      39 | 5162 | `			if( rc != SXRET_OK ){` |
|       - | 5163 | `				/* Key does not exist, break immediately */` |
|      17 | 5164 | `				break;` |
|       - | 5165 | `			}` |
|      12 | 5166 | `		}` |
|      35 | 5167 | `		if( i >= nArg ){` |
|       - | 5168 | `			/* Perform the insertion */` |
|      19 | 5169 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 5170 | `		}` |
|       - | 5171 | `		/* Point to the next entry */` |
|      35 | 5172 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 5173 | `		n--;` |
|       1 | 5174 | `	}` |
|       - | 5175 | `	/* Return the freshly created array */` |
|      15 | 5176 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5177 | `	return PH7_OK;` |
|      15 | 5178 | `}` |
|       - | 5179 | `/*` |
|       - | 5180 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 5181 | ` *  Computes the intersection of arrays.` |
|       - | 5182 | ` * Parameters` |
|       - | 5183 | ` *  $array1` |
|       - | 5184 | ` *    The array to compare from` |
|       - | 5185 | ` *  $array2` |
|       - | 5186 | ` *    An array to compare against` |
|       - | 5187 | ` *  $...` |
|       - | 5188 | ` *   More arrays to compare against` |
|       - | 5189 | ` * $callback` |
|       - | 5190 | ` *  The callback comparison function.` |
|       - | 5191 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5192 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5193 | ` *  than the second.` |
|       - | 5194 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5195 | ` * Return` |
|       - | 5196 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5197 | ` *  in all of the parameters. .` |
|       - | 5198 | ` * Note that NULL is returned on failure.` |
|       - | 5199 | ` */` |
|      26 | 5200 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5201 | `{` |
|       - | 5202 | `	ph7_hashmap_node *pEntry;` |
|       - | 5203 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5204 | `	ph7_value *pCallback;` |
|       - | 5205 | `	ph7_value *pArray;` |
|       - | 5206 | `	ph7_value *pVal;` |
|       - | 5207 | `	sxi32 rc;` |
|       - | 5208 | `	sxu32 n;` |
|       - | 5209 | `	int i;` |
|       - | 5210 |  |
|       - | 5211 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      31 | 5212 | `	if( nArg < 2 ){` |
|       4 | 5213 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5214 | `			"ArgumentCountError",` |
|       - | 5215 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5216 | `			nArg` |
|       - | 5217 | `			);` |
|       - | 5218 | `	}` |
|      29 | 5219 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5220 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5221 | `			"TypeError",` |
|       - | 5222 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5223 | `			ph7_type_name(apArg[0])` |
|       - | 5224 | `			);` |
|       - | 5225 | `	}` |
|       - | 5226 |  |
|      27 | 5227 | `	if( nArg == 2 ){` |
|       - | 5228 | `		/* Only the original array and the callback were provided. */` |
|       - | 5229 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5230 | `		 * validation ordering. */` |
|       3 | 5231 | `	} else {` |
|       - | 5232 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      39 | 5233 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      23 | 5234 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5235 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5236 | `					"TypeError",` |
|       - | 5237 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5238 | `					i + 1,` |
|       2 | 5239 | `					ph7_type_name(apArg[i])` |
|       - | 5240 | `					);` |
|       - | 5241 | `			}` |
|      13 | 5242 | `		}` |
|       - | 5243 | `	}` |
|       - | 5244 |  |
|       - | 5245 | `	/* Identify the callback (always expected as the last argument). */` |
|      25 | 5246 | `	pCallback = apArg[nArg - 1];` |
|       - | 5247 | `	/* Validate the callback to match PHP's error messages. */` |
|      25 | 5248 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      14 | 5249 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5250 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5251 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5252 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5253 | `			 */` |
|       9 | 5254 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       9 | 5255 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5256 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5257 | `					"TypeError",` |
|       - | 5258 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5259 | `					nArg` |
|       - | 5260 | `					);` |
|       - | 5261 | `			}` |
|       - | 5262 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5263 | `			{` |
|       6 | 5264 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       6 | 5265 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       6 | 5266 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5267 | `					int nMethodLen;` |
|       6 | 5268 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       6 | 5269 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       6 | 5270 | `					if( pClass ){` |
|       - | 5271 | `						/* Class exists but method is missing. */` |
|       4 | 5272 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5273 | `							"TypeError",` |
|       - | 5274 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5275 | `							nArg,` |
|       1 | 5276 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5277 | `							zMethod` |
|       - | 5278 | `							);` |
|       - | 5279 | `					}` |
|       - | 5280 | `					/* Class not found */` |
|       - | 5281 | `					{` |
|       - | 5282 | `						int nName;` |
|       3 | 5283 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5284 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5285 | `							"TypeError",` |
|       - | 5286 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5287 | `							nArg,` |
|       1 | 5288 | `							zName` |
|       - | 5289 | `							);` |
|       - | 5290 | `					}` |
|       - | 5291 | `				}` |
|       - | 5292 | `			}` |
|       - | 5293 | `			/* Fallback message */` |
|     ! 0 | 5294 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5295 | `				"TypeError",` |
|       - | 5296 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5297 | `				nArg` |
|       - | 5298 | `				);` |
|       - | 5299 | `		}` |
|       6 | 5300 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5301 | `			int len;` |
|       3 | 5302 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5303 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5304 | `				"TypeError",` |
|       - | 5305 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5306 | `				nArg,` |
|       1 | 5307 | `				zName` |
|       - | 5308 | `				);` |
|       - | 5309 | `		}` |
|       4 | 5310 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5311 | `			"TypeError",` |
|       - | 5312 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5313 | `			nArg` |
|       - | 5314 | `			);` |
|       - | 5315 | `	}` |
|       - | 5316 |  |
|      11 | 5317 | `	if( nArg == 2 ){` |
|       - | 5318 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5319 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5320 | `		return PH7_OK;` |
|       - | 5321 | `	}` |
|       - | 5322 |  |
|       - | 5323 | `	/* Create a new array */` |
|       7 | 5324 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5325 | `	if( pArray == 0 ){` |
|     ! 0 | 5326 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5327 | `		return PH7_OK;` |
|       - | 5328 | `	}` |
|       - | 5329 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 5330 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5331 | `	/* Perform the intersection */` |
|       7 | 5332 | `	pEntry = pSrc->pFirst;` |
|       7 | 5333 | `	n = pSrc->nEntry;` |
|       7 | 5334 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 5335 | `	for(;;){` |
|      19 | 5336 | `		if( n < 1 ){` |
|       5 | 5337 | `			break;` |
|       - | 5338 | `		}` |
|       - | 5339 | `		/* Extract the node value */` |
|      15 | 5340 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5341 | `		if( pVal ){` |
|      23 | 5342 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 5343 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5344 | `					/* ignore */` |
|     ! 0 | 5345 | `					continue;` |
|       - | 5346 | `				}` |
|       - | 5347 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 5348 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5349 | `				/* Perform the lookup */` |
|      15 | 5350 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 5351 | `				if( rc != SXRET_OK ){` |
|       - | 5352 | `					/* Value does not exist */` |
|       7 | 5353 | `					break;` |
|       - | 5354 | `				}` |
|       5 | 5355 | `			}` |
|      15 | 5356 | `			if( i >= (nArg-1) ){` |
|       - | 5357 | `				/* Perform the insertion */` |
|       9 | 5358 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5359 | `			}` |
|       7 | 5360 | `		}` |
|      15 | 5361 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 5362 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5363 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 5364 | `			return PH7_EXCEPTION;` |
|       - | 5365 | `		}` |
|       - | 5366 | `		/* Point to the next entry */` |
|      13 | 5367 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5368 | `		n--;` |
|       1 | 5369 | `	}` |
|       - | 5370 | `	/* Return the freshly created array */` |
|       5 | 5371 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5372 | `	return PH7_OK;` |
|      18 | 5373 | `}` |
|       - | 5374 | `/*` |
|       - | 5375 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5376 | ` *  Fill an array with values.` |
|       - | 5377 | ` * Parameters` |
|       - | 5378 | ` *  $start_index` |
|       - | 5379 | ` *    The first index of the returned array.` |
|       - | 5380 | ` *  $num` |
|       - | 5381 | ` *   Number of elements to insert.` |
|       - | 5382 | ` *  $value` |
|       - | 5383 | ` *    Value to use for filling.` |
|       - | 5384 | ` * Return` |
|       - | 5385 | ` *  The filled array or null on failure.` |
|       - | 5386 | ` */` |
|     238 | 5387 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5388 | `{` |
|       - | 5389 | `	ph7_value *pArray;` |
|       - | 5390 | `	int i,nEntry;` |
|       - | 5391 |  |
|       - | 5392 | `	/* PHP enforces argument count and type checks. */` |
|     243 | 5393 | `	if( nArg != 3 ){` |
|       - | 5394 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       8 | 5395 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5396 | `			"ArgumentCountError",` |
|       - | 5397 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5398 | `			nArg` |
|       - | 5399 | `			);` |
|       - | 5400 | `	}` |
|       - | 5401 |  |
|       - | 5402 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5403 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5404 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5405 | `	 * and NULLs are rejected outright. */` |
|     350 | 5406 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     353 | 5407 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5408 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5409 | `			"TypeError",` |
|       - | 5410 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5411 | `			ph7_type_name(apArg[0])` |
|       - | 5412 | `			);` |
|       - | 5413 | `	}` |
|     236 | 5414 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5415 | `		int len;` |
|       8 | 5416 | `		sxu8 bReal = FALSE;` |
|       8 | 5417 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5418 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5419 | `			/* Non‑numeric string is an error. */` |
|       3 | 5420 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5421 | `				"TypeError",` |
|       - | 5422 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5423 | `				);` |
|       - | 5424 | `		}` |
|       5 | 5425 | `		if( bReal ){` |
|       - | 5426 | `			/* float-string -> deprecation warning */` |
|       4 | 5427 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5428 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5429 | `				zStr` |
|       - | 5430 | `				);` |
|       1 | 5431 | `		}` |
|       2 | 5432 | `	}` |
|       - | 5433 |  |
|       - | 5434 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5435 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     345 | 5436 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     348 | 5437 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5438 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5439 | `			"TypeError",` |
|       - | 5440 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5441 | `			ph7_type_name(apArg[1])` |
|       - | 5442 | `			);` |
|       - | 5443 | `	}` |
|     233 | 5444 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5445 | `		int len;` |
|       3 | 5446 | `		sxu8 bReal = FALSE;` |
|       3 | 5447 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5448 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5449 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5450 | `				"TypeError",` |
|       - | 5451 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5452 | `				);` |
|       - | 5453 | `		}` |
|     ! 0 | 5454 | `	}` |
|       - | 5455 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5456 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5457 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5458 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5459 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5460 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5461 | `		if( d != (double)i64 ){` |
|       7 | 5462 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5463 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5464 | `				d` |
|       - | 5465 | `				);` |
|       2 | 5466 | `		}` |
|       2 | 5467 | `	}` |
|       - | 5468 |  |
|       - | 5469 | `	/* Total number of entries to insert */` |
|     230 | 5470 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5471 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5472 | `	if( nEntry < 0 ){` |
|       3 | 5473 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5474 | `			"ValueError",` |
|       - | 5475 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5476 | `			);` |
|       - | 5477 | `	}` |
|       - | 5478 |  |
|       - | 5479 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5480 | `	if( nEntry == 0 ){` |
|       7 | 5481 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5482 | `		return PH7_OK;` |
|       - | 5483 | `	}` |
|       - | 5484 |  |
|       - | 5485 | `	/* Create a new array */` |
|     221 | 5486 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5487 | `	if( pArray == 0 ){` |
|     ! 0 | 5488 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5489 | `	}` |
|       - | 5490 |  |
|       - | 5491 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5492 | `	if( ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]) != SXRET_OK ){` |
|     ! 0 | 5493 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5494 | `	}` |
|       - | 5495 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5496 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5497 | `		if( ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]) != SXRET_OK ){` |
|       - | 5498 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 5499 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 5500 | `		}` |
| 1058682 | 5501 | `	}` |
|       - | 5502 | `	/* Return the filled array */` |
|     221 | 5503 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5504 | `	return PH7_OK;` |
|     124 | 5505 | `}` |
|       - | 5506 | `/*` |
|       - | 5507 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5508 | ` *  Fill an array with values, specifying keys.` |
|       - | 5509 | ` * Parameters` |
|       - | 5510 | ` *  $input` |
|       - | 5511 | ` *   Array of values that will be used as key.` |
|       - | 5512 | ` *  $value` |
|       - | 5513 | ` *    Value to use for filling.` |
|       - | 5514 | ` * Return` |
|       - | 5515 | ` *  The filled array.` |
|       - | 5516 | ` * Throws` |
|       - | 5517 | ` *  ValueError if $input is not an array.` |
|       - | 5518 | ` */` |
|      26 | 5519 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5520 | `{` |
|       - | 5521 | `	ph7_hashmap_node *pEntry;` |
|       - | 5522 | `	ph7_hashmap *pSrc;` |
|       - | 5523 | `	ph7_value *pArray;` |
|       - | 5524 | `	sxu32 n;` |
|       - | 5525 | `	/* PHP enforces exactly 2 arguments. */` |
|      31 | 5526 | `	if( nArg != 2 ){` |
|      12 | 5527 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5528 | `			"ArgumentCountError",` |
|       - | 5529 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5530 | `			nArg` |
|       - | 5531 | `			);` |
|       - | 5532 | `	}` |
|       - | 5533 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 5534 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 5535 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5536 | `			"TypeError",` |
|       - | 5537 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5538 | `			ph7_type_name(apArg[0])` |
|       - | 5539 | `			);` |
|       - | 5540 | `	}` |
|       - | 5541 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5542 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5543 | `	/* Create a new array */` |
|      17 | 5544 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5545 | `	if( pArray == 0 ){` |
|     ! 0 | 5546 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5547 | `		return PH7_OK;` |
|       - | 5548 | `	}` |
|       - | 5549 | `	/* Perform the requested operation */` |
|      17 | 5550 | `	pEntry = pSrc->pFirst;` |
|      45 | 5551 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5552 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5553 | `		/* Point to the next entry */` |
|      29 | 5554 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5555 | `	}` |
|       - | 5556 | `	/* Return the filled array */` |
|      17 | 5557 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5558 | `	return PH7_OK;` |
|      18 | 5559 | `}` |
|       - | 5560 | `/*` |
|       - | 5561 | ` * array array_combine(array $keys,array $values)` |
|       - | 5562 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5563 | ` * Parameters` |
|       - | 5564 | ` *  $keys` |
|       - | 5565 | ` *    Array of keys to be used.` |
|       - | 5566 | ` * $values` |
|       - | 5567 | ` *   Array of values to be used.` |
|       - | 5568 | ` * Return` |
|       - | 5569 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5570 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5571 | ` *  not an array.` |
|       - | 5572 | ` */` |
|      18 | 5573 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5574 | `{` |
|       - | 5575 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5576 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5577 | `	ph7_value *pArray;` |
|       - | 5578 | `	sxu32 n;` |
|       - | 5579 | `	/* PHP enforces argument count and type checks. */` |
|      23 | 5580 | `	if( nArg != 2 ){` |
|       - | 5581 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5582 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5583 | `			"ArgumentCountError",` |
|       - | 5584 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5585 | `			nArg` |
|       - | 5586 | `			);` |
|       - | 5587 | `	}` |
|       - | 5588 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5589 | `	 * argument index in the error message. */` |
|      20 | 5590 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5591 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5592 | `			"TypeError",` |
|       - | 5593 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5594 | `			ph7_type_name(apArg[0])` |
|       - | 5595 | `			);` |
|       - | 5596 | `	}` |
|      17 | 5597 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5598 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5599 | `			"TypeError",` |
|       - | 5600 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5601 | `			ph7_type_name(apArg[1])` |
|       - | 5602 | `			);` |
|       - | 5603 | `	}` |
|       - | 5604 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5605 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5606 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5607 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5608 | `		/* Length mismatch -> ValueError */` |
|       3 | 5609 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5610 | `			"ValueError",` |
|       - | 5611 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5612 | `			);` |
|       - | 5613 | `	}` |
|       - | 5614 | `	/* Create a new array */` |
|      11 | 5615 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5616 | `	if( pArray == 0 ){` |
|     ! 0 | 5617 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5618 | `		return PH7_OK;` |
|       - | 5619 | `	}` |
|       - | 5620 | `	/* Perform the requested operation */` |
|      11 | 5621 | `	pKe = pKey->pFirst;` |
|      11 | 5622 | `	pVe = pValue->pFirst;` |
|      33 | 5623 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5624 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5625 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5626 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5627 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5628 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5629 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5630 | `		 * original array must not be mutated. */` |
|      23 | 5631 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5632 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5633 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5634 | `			if( pTmpKey ){` |
|       5 | 5635 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5636 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5637 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5638 | `				pKeyCopy = pTmpKey;` |
|       2 | 5639 | `			}` |
|       2 | 5640 | `		}` |
|      23 | 5641 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5642 | `		/* Point to the next entry */` |
|      23 | 5643 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5644 | `		pVe = pVe->pPrev;` |
|      12 | 5645 | `	}` |
|       - | 5646 | `	/* Return the filled array */` |
|      11 | 5647 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5648 | `	return PH7_OK;` |
|      14 | 5649 | `}` |
|       - | 5650 | `/*` |
|       - | 5651 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5652 | ` *  Return an array with elements in reverse order.` |
|       - | 5653 | ` * Parameters` |
|       - | 5654 | ` *  $array` |
|       - | 5655 | ` *   The input array.` |
|       - | 5656 | ` *  $preserve_keys (optional)` |
|       - | 5657 | ` *   If set to TRUE keys are preserved.` |
|       - | 5658 | ` * Return` |
|       - | 5659 | ` *  The reversed array.` |
|       - | 5660 | ` */` |
|      20 | 5661 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 5662 | `{` |
|       - | 5663 | `	ph7_hashmap_node *pEntry;` |
|       - | 5664 | `	ph7_hashmap *pSrc;` |
|       - | 5665 | `	ph7_value *pArray;` |
|       - | 5666 | `	int bPreserve;` |
|       - | 5667 | `	sxu32 n;` |
|      23 | 5668 | `	if( nArg < 1 ){` |
|       4 | 5669 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5670 | `			"ArgumentCountError",` |
|       - | 5671 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5672 | `			nArg` |
|       - | 5673 | `			);` |
|       - | 5674 | `	}` |
|       - | 5675 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5676 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5677 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5678 | `			"TypeError",` |
|       - | 5679 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5680 | `			ph7_type_name(apArg[0])` |
|       - | 5681 | `			);` |
|       - | 5682 | `	}` |
|      17 | 5683 | `	bPreserve = FALSE;` |
|      17 | 5684 | `	if( nArg > 1 ){` |
|       7 | 5685 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5686 | `	}` |
|       - | 5687 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5688 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5689 | `	/* Create a new array */` |
|      17 | 5690 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5691 | `	if( pArray == 0 ){` |
|     ! 0 | 5692 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5693 | `		return PH7_OK;` |
|       - | 5694 | `	}` |
|       - | 5695 | `	/* Perform the requested operation */` |
|      17 | 5696 | `	pEntry = pSrc->pLast;` |
|      55 | 5697 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5698 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5699 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5700 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5701 | `		/* Point to the previous entry */` |
|      39 | 5702 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5703 | `	}` |
|      17 | 5704 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5705 | `	return PH7_OK;` |
|      13 | 5706 | `}` |
|       - | 5707 | `/*` |
|       - | 5708 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5709 | ` *  Removes duplicate values from an array.` |
|       - | 5710 | ` * Parameters` |
|       - | 5711 | ` *  $array` |
|       - | 5712 | ` *   The input array.` |
|       - | 5713 | ` *  $flags` |
|       - | 5714 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5715 | ` *   behavior using these values:` |
|       - | 5716 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5717 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5718 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5719 | ` * Return` |
|       - | 5720 | ` *  The filtered array.` |
|       - | 5721 | ` */` |
|      24 | 5722 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5723 | `{` |
|       - | 5724 | `	ph7_hashmap_node *pEntry;` |
|       - | 5725 | `	ph7_value *pNeedle;` |
|       - | 5726 | `	ph7_hashmap *pSrc;` |
|       - | 5727 | `	ph7_value *pArray;` |
|       - | 5728 | `	int bStrict;` |
|       - | 5729 | `	sxi32 rc;` |
|       - | 5730 | `	sxu32 n;` |
|      28 | 5731 | `	if( nArg < 1 ){` |
|       - | 5732 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5733 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5734 | `			"ArgumentCountError",` |
|       - | 5735 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5736 | `			);` |
|       - | 5737 | `	}` |
|      25 | 5738 | `	if( nArg > 2 ){` |
|       - | 5739 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5740 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5741 | `			"ArgumentCountError",` |
|       - | 5742 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5743 | `			nArg` |
|       - | 5744 | `			);` |
|       - | 5745 | `	}` |
|       - | 5746 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5747 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5748 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5749 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5750 | `			"TypeError",` |
|       - | 5751 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5752 | `			ph7_type_name(apArg[0])` |
|       - | 5753 | `			);` |
|       - | 5754 | `	}` |
|      19 | 5755 | `	bStrict = FALSE;` |
|       - | 5756 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5757 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5758 | `	/* Create a new array */` |
|      19 | 5759 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5760 | `	if( pArray == 0 ){` |
|     ! 0 | 5761 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5762 | `		return PH7_OK;` |
|       - | 5763 | `	}` |
|       - | 5764 | `	/* Perform the requested operation */` |
|      19 | 5765 | `	pEntry = pSrc->pFirst;` |
|      83 | 5766 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5767 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5768 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5769 | `		if( pNeedle ){` |
|      65 | 5770 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5771 | `		}` |
|      65 | 5772 | `		if( rc != SXRET_OK ){` |
|       - | 5773 | `			/* Perform the insertion */` |
|      37 | 5774 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5775 | `		}` |
|       - | 5776 | `		/* Point to the next entry */` |
|      65 | 5777 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5778 | `	}` |
|       - | 5779 | `	/* Return the freshly created array */` |
|      19 | 5780 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5781 | `	return PH7_OK;` |
|      16 | 5782 | `}` |
|       - | 5783 | `/*` |
|       - | 5784 | ` * array array_flip(array $input)` |
|       - | 5785 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5786 | ` * Parameter` |
|       - | 5787 | ` *  $input` |
|       - | 5788 | ` *   Input array.` |
|       - | 5789 | ` * Return` |
|       - | 5790 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5791 | ` */` |
|      34 | 5792 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5793 | `{` |
|       - | 5794 | `	ph7_hashmap_node *pEntry;` |
|       - | 5795 | `	ph7_hashmap *pSrc;` |
|       - | 5796 | `	ph7_value *pArray;` |
|       - | 5797 | `	ph7_value *pKey;` |
|       - | 5798 | `	ph7_value sVal;` |
|       - | 5799 | `	sxu32 n;` |
|       - | 5800 |  |
|       - | 5801 | `	/* PHP requires exactly one argument */` |
|      39 | 5802 | `	if( nArg != 1 ){` |
|       - | 5803 | `		/* Use ArgumentCountError like other array helpers */` |
|       8 | 5804 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5805 | `			"ArgumentCountError",` |
|       - | 5806 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5807 | `			nArg` |
|       - | 5808 | `			);` |
|       - | 5809 | `	}` |
|       - | 5810 | `	/* Make sure we are dealing with a valid hashmap */` |
|      33 | 5811 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5812 | `		/* Type mismatch -> TypeError */` |
|       8 | 5813 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5814 | `			"TypeError",` |
|       - | 5815 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5816 | `			ph7_type_name(apArg[0])` |
|       - | 5817 | `			);` |
|       - | 5818 | `	}` |
|       - | 5819 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5820 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5821 | `	/* Create a new array */` |
|      27 | 5822 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5823 | `	if( pArray == 0 ){` |
|     ! 0 | 5824 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5825 | `		return PH7_OK;` |
|       - | 5826 | `	}` |
|       - | 5827 | `	/* Start processing */` |
|      27 | 5828 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5829 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5830 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5831 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5832 | `		if( pKey ){` |
|       - | 5833 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5834 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5835 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5836 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5837 | `					);` |
|   22236 | 5838 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5839 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5840 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5841 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5842 | `				}else{` |
|       - | 5843 | `					SyString sStr;` |
|    2227 | 5844 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5845 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5846 | `				}` |
|       - | 5847 | `				/* Perform the insertion */` |
|   22227 | 5848 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5849 | `				/* Safely release the value because each inserted entry` |
|       - | 5850 | `				 * has its own private copy of the value.` |
|       - | 5851 | `				 */` |
|   22227 | 5852 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5853 | `			}else{` |
|       - | 5854 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5855 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5856 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5857 | `					);` |
|       - | 5858 | `			}` |
|   11118 | 5859 | `		}` |
|       - | 5860 | `		/* Point to the next entry */` |
|   22237 | 5861 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5862 | `	}` |
|       - | 5863 | `	/* Return the freshly created array */` |
|      27 | 5864 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5865 | `	return PH7_OK;` |
|      22 | 5866 | `}` |
|       - | 5867 | `/*` |
|       - | 5868 | ` * number array_sum(array $array )` |
|       - | 5869 | ` *  Calculate the sum of values in an array.` |
|       - | 5870 | ` * Parameters` |
|       - | 5871 | ` *  $array: The input array.` |
|       - | 5872 | ` * Return` |
|       - | 5873 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5874 | ` */` |
|      24 | 5875 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5876 | `{` |
|       - | 5877 | `	ph7_hashmap_node *pEntry;` |
|       - | 5878 | `	ph7_value *pObj;` |
|      25 | 5879 | `	double dSum = 0;` |
|       - | 5880 | `	sxu32 n;` |
|      25 | 5881 | `	pEntry = pMap->pFirst;` |
|      91 | 5882 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5883 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5884 | `		if( pObj ){` |
|      67 | 5885 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5886 | `				dSum += pObj->rVal;` |
|      53 | 5887 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5888 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5889 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5890 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5891 | `					double dv = 0;` |
|      13 | 5892 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5893 | `					dSum += dv;` |
|       7 | 5894 | `				}` |
|      12 | 5895 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5896 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5897 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5898 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5899 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5900 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5901 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5902 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5903 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5904 | `			}` |
|       - | 5905 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5906 | `		}` |
|       - | 5907 | `		/* Point to the next entry */` |
|      67 | 5908 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5909 | `	}` |
|       - | 5910 | `	/* Return sum */` |
|      25 | 5911 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5912 | `}` |
|      30 | 5913 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5914 | `{` |
|       - | 5915 | `	ph7_hashmap_node *pEntry;` |
|       - | 5916 | `	ph7_value *pObj;` |
|      32 | 5917 | `	sxi64 nSum = 0;` |
|       - | 5918 | `	sxu32 n;` |
|      32 | 5919 | `	pEntry = pMap->pFirst;` |
|     128 | 5920 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      98 | 5921 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      98 | 5922 | `		if( pObj ){` |
|      98 | 5923 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      88 | 5924 | `				nSum += pObj->x.iVal;` |
|      54 | 5925 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5926 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5927 | `					sxi64 nv = 0;` |
|       5 | 5928 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5929 | `					nSum += nv;` |
|       3 | 5930 | `				}` |
|       8 | 5931 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5932 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5933 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5934 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5935 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5936 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5937 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5938 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5939 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5940 | `			}` |
|       - | 5941 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      48 | 5942 | `		}` |
|       - | 5943 | `		/* Point to the next entry */` |
|      98 | 5944 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      50 | 5945 | `	}` |
|       - | 5946 | `	/* Return sum */` |
|      32 | 5947 | `	ph7_result_int64(pCtx,nSum);` |
|      32 | 5948 | `}` |
|       - | 5949 | `/* number array_sum(array $array )` |
|       - | 5950 | ` * (See block-coment above)` |
|       - | 5951 | ` */` |
|      68 | 5952 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5953 | `{` |
|       - | 5954 | `	ph7_hashmap_node *pEntry;` |
|       - | 5955 | `	ph7_hashmap *pMap;` |
|       - | 5956 | `	ph7_value *pObj;` |
|      73 | 5957 | `	int useDouble = 0;` |
|       - | 5958 | `	sxu32 n;` |
|       - | 5959 | `	/* PHP requires exactly one argument */` |
|      73 | 5960 | `	if( nArg != 1 ){` |
|       8 | 5961 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5962 | `			"ArgumentCountError",` |
|       - | 5963 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5964 | `			nArg` |
|       - | 5965 | `			);` |
|       - | 5966 | `	}` |
|       - | 5967 | `	/* Make sure we are dealing with a valid hashmap */` |
|      68 | 5968 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5969 | `		/* Type mismatch -> TypeError */` |
|       8 | 5970 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5971 | `			"TypeError",` |
|       - | 5972 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5973 | `			ph7_type_name(apArg[0])` |
|       - | 5974 | `			);` |
|       - | 5975 | `	}` |
|      62 | 5976 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      62 | 5977 | `	if( pMap->nEntry < 1 ){` |
|       - | 5978 | `		/* Nothing to compute,return 0 */` |
|       7 | 5979 | `		ph7_result_int(pCtx,0);` |
|       7 | 5980 | `		return PH7_OK;` |
|       - | 5981 | `	}` |
|       - | 5982 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5983 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5984 | `	 */` |
|      56 | 5985 | `	pEntry = pMap->pFirst;` |
|     160 | 5986 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     130 | 5987 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     130 | 5988 | `		if( pObj ){` |
|     130 | 5989 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5990 | `				useDouble = 1;` |
|      19 | 5991 | `				break;` |
|       - | 5992 | `			}` |
|     112 | 5993 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5994 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5995 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5996 | `				sxu32 i;` |
|      23 | 5997 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5998 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5999 | `						useDouble = 1;` |
|       7 | 6000 | `						break;` |
|       - | 6001 | `					}` |
|       6 | 6002 | `				}` |
|      13 | 6003 | `				if( useDouble ){` |
|       7 | 6004 | `					break;` |
|       - | 6005 | `				}` |
|       3 | 6006 | `			}` |
|      52 | 6007 | `		}` |
|     106 | 6008 | `		pEntry = pEntry->pPrev;` |
|      54 | 6009 | `	}` |
|      56 | 6010 | `	if( useDouble ){` |
|      25 | 6011 | `		DoubleSum(pCtx,pMap);` |
|      13 | 6012 | `	}else{` |
|      32 | 6013 | `		Int64Sum(pCtx,pMap);` |
|       - | 6014 | `	}` |
|      56 | 6015 | `	return PH7_OK;` |
|      39 | 6016 | `}` |
|       - | 6017 | `/*` |
|       - | 6018 | ` * number array_product(array $array )` |
|       - | 6019 | ` *  Calculate the product of values in an array.` |
|       - | 6020 | ` * Parameters` |
|       - | 6021 | ` *  $array: The input array.` |
|       - | 6022 | ` * Return` |
|       - | 6023 | ` *  Returns the product of values as an integer or float.` |
|       - | 6024 | ` */` |
|     ! 0 | 6025 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 6026 | `{` |
|       - | 6027 | `	ph7_hashmap_node *pEntry;` |
|       - | 6028 | `	ph7_value *pObj;` |
|       - | 6029 | `	double dProd;` |
|       - | 6030 | `	sxu32 n;` |
|     ! 0 | 6031 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6032 | `	dProd = 1;` |
|     ! 0 | 6033 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6034 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6035 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6036 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6037 | `				dProd *= pObj->rVal;` |
|     ! 0 | 6038 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6039 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 6040 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6041 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6042 | `					double dv = 0;` |
|     ! 0 | 6043 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 6044 | `					dProd *= dv;` |
|     ! 0 | 6045 | `				}` |
|     ! 0 | 6046 | `			}` |
|     ! 0 | 6047 | `		}` |
|       - | 6048 | `		/* Point to the next entry */` |
|     ! 0 | 6049 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6050 | `	}` |
|       - | 6051 | `	/* Return product */` |
|     ! 0 | 6052 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 6053 | `}` |
|     ! 0 | 6054 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 6055 | `{` |
|       - | 6056 | `	ph7_hashmap_node *pEntry;` |
|       - | 6057 | `	ph7_value *pObj;` |
|       - | 6058 | `	sxi64 nProd;` |
|       - | 6059 | `	sxu32 n;` |
|     ! 0 | 6060 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6061 | `	nProd = 1;` |
|     ! 0 | 6062 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6063 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6064 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6065 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6066 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 6067 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6068 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 6069 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6070 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6071 | `					sxi64 nv = 0;` |
|     ! 0 | 6072 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 6073 | `					nProd *= nv;` |
|     ! 0 | 6074 | `				}` |
|     ! 0 | 6075 | `			}` |
|     ! 0 | 6076 | `		}` |
|       - | 6077 | `		/* Point to the next entry */` |
|     ! 0 | 6078 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6079 | `	}` |
|       - | 6080 | `	/* Return product */` |
|     ! 0 | 6081 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 6082 | `}` |
|       - | 6083 | `/* number array_product(array $array )` |
|       - | 6084 | ` * (See block-block comment above)` |
|       - | 6085 | ` */` |
|     ! 0 | 6086 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 6087 | `{` |
|       - | 6088 | `	ph7_hashmap *pMap;` |
|       - | 6089 | `	ph7_value *pObj;` |
|     ! 0 | 6090 | `	if( nArg < 1 ){` |
|       - | 6091 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 6092 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6093 | `		return PH7_OK;` |
|       - | 6094 | `	}` |
|       - | 6095 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 6096 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6097 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 6098 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6099 | `		return PH7_OK;` |
|       - | 6100 | `	}` |
|     ! 0 | 6101 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 6102 | `	if( pMap->nEntry < 1 ){` |
|       - | 6103 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 6104 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6105 | `		return PH7_OK;` |
|       - | 6106 | `	}` |
|       - | 6107 | `	/* If the first element is of type float,then perform floating` |
|       - | 6108 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 6109 | `	 */` |
|     ! 0 | 6110 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 6111 | `	if( pObj == 0 ){` |
|     ! 0 | 6112 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6113 | `		return PH7_OK;` |
|       - | 6114 | `	}` |
|     ! 0 | 6115 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6116 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 6117 | `	}else{` |
|     ! 0 | 6118 | `		Int64Prod(pCtx,pMap);` |
|       - | 6119 | `	}` |
|     ! 0 | 6120 | `	return PH7_OK;` |
|     ! 0 | 6121 | `}` |
|       - | 6122 | `/*` |
|       - | 6123 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 6124 | ` *  Pick one or more random entries out of an array.` |
|       - | 6125 | ` * Parameters` |
|       - | 6126 | ` * $input` |
|       - | 6127 | ` *  The input array.` |
|       - | 6128 | ` * $num_req` |
|       - | 6129 | ` *  Specifies how many entries you want to pick.` |
|       - | 6130 | ` * Return` |
|       - | 6131 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 6132 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 6133 | ` *  NULL is returned on failure.` |
|       - | 6134 | ` */` |
|       6 | 6135 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6136 | `{` |
|       - | 6137 | `	ph7_hashmap_node *pNode;` |
|       - | 6138 | `	ph7_hashmap *pMap;` |
|       7 | 6139 | `	int nItem = 1;` |
|       7 | 6140 | `	if( nArg < 1 ){` |
|       - | 6141 | `		/* Missing argument,return NULL */` |
|     ! 0 | 6142 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6143 | `		return PH7_OK;` |
|       - | 6144 | `	}` |
|       - | 6145 | `	/* Make sure we are dealing with an array */` |
|       7 | 6146 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6147 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6148 | `		return PH7_OK;` |
|       - | 6149 | `	}` |
|       - | 6150 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 6151 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 6152 | `	if(pMap->nEntry < 1 ){` |
|       - | 6153 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 6154 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6155 | `		return PH7_OK;` |
|       - | 6156 | `	}` |
|       7 | 6157 | `	if( nArg > 1 ){` |
|       3 | 6158 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 6159 | `	}` |
|       7 | 6160 | `	if( nItem < 2 ){` |
|       - | 6161 | `		sxu32 nEntry;` |
|       - | 6162 | `		/* Select a random number */` |
|       5 | 6163 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 6164 | `		/* Extract the desired entry.` |
|       - | 6165 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 6166 | `		 */` |
|       5 | 6167 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       2 | 6168 | `			pNode = pMap->pLast;` |
|       2 | 6169 | `			nEntry = pMap->nEntry - nEntry;` |
|       2 | 6170 | `			if( nEntry > 1 ){` |
|     ! 0 | 6171 | `				for(;;){` |
|     ! 0 | 6172 | `					if( nEntry == 0 ){` |
|     ! 0 | 6173 | `						break;` |
|       - | 6174 | `					}` |
|       - | 6175 | `					/* Point to the previous entry */` |
|     ! 0 | 6176 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 6177 | `					nEntry--;` |
|     ! 0 | 6178 | `				}` |
|     ! 0 | 6179 | `			}` |
|       1 | 6180 | `		}else{` |
|       4 | 6181 | `			pNode = pMap->pFirst;` |
|       3 | 6182 | `			for(;;){` |
|       5 | 6183 | `				if( nEntry == 0 ){` |
|       4 | 6184 | `					break;` |
|       - | 6185 | `				}` |
|       - | 6186 | `				/* Point to the next entry */` |
|       2 | 6187 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       2 | 6188 | `				nEntry--;` |
|       1 | 6189 | `			}` |
|       - | 6190 | `		}` |
|       5 | 6191 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6192 | `			/* Int key */` |
|       3 | 6193 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6194 | `		}else{` |
|       - | 6195 | `			/* Blob key */` |
|       3 | 6196 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6197 | `		}` |
|       3 | 6198 | `	}else{` |
|       - | 6199 | `		ph7_value sKey,*pArray;` |
|       - | 6200 | `		ph7_hashmap *pDest;` |
|       - | 6201 | `		/* Create a new array */` |
|       3 | 6202 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6203 | `		if( pArray == 0 ){` |
|     ! 0 | 6204 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6205 | `			return PH7_OK;` |
|       - | 6206 | `		}` |
|       - | 6207 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6208 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6209 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6210 | `		/* Copy the first n items */` |
|       3 | 6211 | `		pNode = pMap->pFirst;` |
|       3 | 6212 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6213 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6214 | `		}` |
|       7 | 6215 | `		while( nItem > 0){` |
|       5 | 6216 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6217 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6218 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6219 | `			/* Point to the next entry */` |
|       5 | 6220 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6221 | `			nItem--;` |
|       1 | 6222 | `		}` |
|       - | 6223 | `		/* Shuffle the array */` |
|       3 | 6224 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6225 | `		/* Rehash node */` |
|       3 | 6226 | `		HashmapSortRehash(pDest);` |
|       - | 6227 | `		/* Return the random array */` |
|       3 | 6228 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6229 | `	}` |
|       7 | 6230 | `	return PH7_OK;` |
|       4 | 6231 | `}` |
|       - | 6232 | `/*` |
|       - | 6233 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6234 | ` *  Split an array into chunks.` |
|       - | 6235 | ` * Parameters` |
|       - | 6236 | ` * $input` |
|       - | 6237 | ` *   The array to work on` |
|       - | 6238 | ` * $size` |
|       - | 6239 | ` *   The size of each chunk` |
|       - | 6240 | ` * $preserve_keys` |
|       - | 6241 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6242 | ` *   the chunk numerically.` |
|       - | 6243 | ` * Return` |
|       - | 6244 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6245 | ` *  zero, with each dimension containing size elements.` |
|       - | 6246 | ` */` |
|      42 | 6247 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6248 | `{` |
|       - | 6249 | `	ph7_value *pArray,*pChunk;` |
|       - | 6250 | `	ph7_hashmap_node *pEntry;` |
|       - | 6251 | `	ph7_hashmap *pMap;` |
|       - | 6252 | `	int bPreserve;` |
|       - | 6253 | `	sxu32 nChunk;` |
|       - | 6254 | `	sxu32 nSize;` |
|       - | 6255 | `	sxu32 n;` |
|       - | 6256 | `	/* Argument count and types follow PHP semantics. */` |
|      47 | 6257 | `	if( nArg < 2 ){` |
|       - | 6258 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6259 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6260 | `			"ArgumentCountError",` |
|       - | 6261 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6262 | `			nArg` |
|       - | 6263 | `			);` |
|       - | 6264 | `	}` |
|      45 | 6265 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6266 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6267 | `			"TypeError",` |
|       - | 6268 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6269 | `			ph7_type_name(apArg[0])` |
|       - | 6270 | `			);` |
|       - | 6271 | `	}` |
|       - | 6272 | `	/* Create a new array */` |
|      43 | 6273 | `	pArray = ph7_context_new_array(pCtx);` |
|      43 | 6274 | `	if( pArray == 0 ){` |
|     ! 0 | 6275 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6276 | `		return PH7_OK;` |
|       - | 6277 | `	}` |
|       - | 6278 | `	/* Point to the internal representation of the input hashmap */` |
|      43 | 6279 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6280 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6281 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      57 | 6282 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      81 | 6283 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6284 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6285 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6286 | `			"TypeError",` |
|       - | 6287 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6288 | `			ph7_type_name(apArg[1])` |
|       - | 6289 | `			);` |
|       - | 6290 | `	}` |
|       - | 6291 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6292 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6293 | `	 * precision and PHP emits a deprecation warning. */` |
|      43 | 6294 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6295 | `		int len;` |
|       3 | 6296 | `		sxu8 bReal = FALSE;` |
|       3 | 6297 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6298 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6299 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6300 | `				"TypeError",` |
|       - | 6301 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6302 | `				);` |
|       - | 6303 | `		}` |
|     ! 0 | 6304 | `		if( bReal ){` |
|       - | 6305 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6306 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6307 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6308 | `				zStr` |
|       - | 6309 | `				);` |
|     ! 0 | 6310 | `		}` |
|     ! 0 | 6311 | `	}` |
|       - | 6312 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6313 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6314 | `	 * later via ph7_value_to_int. */` |
|      40 | 6315 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6316 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6317 | `		sxi64 i = (sxi64)d;` |
|       3 | 6318 | `		if( d != (double)i ){` |
|       4 | 6319 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6320 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6321 | `				d` |
|       - | 6322 | `				);` |
|       1 | 6323 | `		}` |
|       1 | 6324 | `	}` |
|       - | 6325 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6326 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6327 | `	{` |
|      40 | 6328 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      40 | 6329 | `		if( nSizeSigned < 1 ){` |
|       - | 6330 | `			/* size <= 0 -> ValueError */` |
|       6 | 6331 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6332 | `				"ValueError",` |
|       - | 6333 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6334 | `				);` |
|       - | 6335 | `		}` |
|      35 | 6336 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6337 | `	}` |
|      35 | 6338 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6339 | `		/* Return the whole array */` |
|       3 | 6340 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6341 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6342 | `		return PH7_OK;` |
|       - | 6343 | `	}` |
|      33 | 6344 | `	bPreserve = 0;` |
|      33 | 6345 | `	if( nArg > 2 ){` |
|       - | 6346 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6347 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6348 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6349 | `		 * normally, matching PHP behaviour. */` |
|      35 | 6350 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      35 | 6351 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6352 | `			ph7_value_is_resource(apArg[2]) ){` |
|       8 | 6353 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6354 | `				"TypeError",` |
|       - | 6355 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6356 | `				ph7_type_name(apArg[2])` |
|       - | 6357 | `				);` |
|       - | 6358 | `		}` |
|      21 | 6359 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6360 | `	}` |
|       - | 6361 | `	/* Start processing */` |
|      27 | 6362 | `	pEntry = pMap->pFirst;` |
|      27 | 6363 | `	nChunk = 0;` |
|      27 | 6364 | `	pChunk = 0;` |
|      27 | 6365 | `	n = pMap->nEntry;` |
|      56 | 6366 | `	for( ;; ){` |
|     113 | 6367 | `		if( n < 1 ){` |
|       - | 6368 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6369 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6370 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6371 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6372 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6373 | `			 * exists. */` |
|      27 | 6374 | `			if( pChunk ){` |
|      27 | 6375 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6376 | `			}` |
|      27 | 6377 | `			break;` |
|       - | 6378 | `		}` |
|      87 | 6379 | `		if( nChunk < 1 ){` |
|      71 | 6380 | `			if( pChunk ){` |
|       - | 6381 | `				/* Put the first chunk */` |
|      45 | 6382 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6383 | `			}` |
|       - | 6384 | `			/* Create a new dimension */` |
|      71 | 6385 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6386 | `												   * will be automatically released as soon we return` |
|       - | 6387 | `												   * from this function */` |
|      71 | 6388 | `			if( pChunk == 0 ){` |
|     ! 0 | 6389 | `				break;` |
|       - | 6390 | `			}` |
|      71 | 6391 | `			nChunk = nSize;` |
|      35 | 6392 | `		}` |
|       - | 6393 | `		/* Insert the entry */` |
|      87 | 6394 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6395 | `		/* Point to the next entry */` |
|      87 | 6396 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6397 | `		nChunk--;` |
|      87 | 6398 | `		n--;` |
|       1 | 6399 | `	}` |
|       - | 6400 | `	/* Return the multidimensional array */` |
|      27 | 6401 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6402 | `	return PH7_OK;` |
|      26 | 6403 | `}` |
|       - | 6404 | `/*` |
|       - | 6405 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6406 | ` *  Pad array to the specified length with a value.` |
|       - | 6407 | ` * $input` |
|       - | 6408 | ` *   Initial array of values to pad.` |
|       - | 6409 | ` * $pad_size` |
|       - | 6410 | ` *   New size of the array.` |
|       - | 6411 | ` * $pad_value` |
|       - | 6412 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6413 | ` */` |
|      28 | 6414 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6415 | `{` |
|       - | 6416 | `	ph7_hashmap *pMap;` |
|       - | 6417 | `	ph7_value *pArray;` |
|       - | 6418 | `	int nEntry;` |
|      33 | 6419 | `	if( nArg != 3 ){` |
|      12 | 6420 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6421 | `			"ArgumentCountError",` |
|       - | 6422 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6423 | `			nArg` |
|       - | 6424 | `			);` |
|       - | 6425 | `	}` |
|      24 | 6426 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6427 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6428 | `			"TypeError",` |
|       - | 6429 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6430 | `			ph7_type_name(apArg[0])` |
|       - | 6431 | `			);` |
|       - | 6432 | `	}` |
|       - | 6433 | `	/* Create a new array */` |
|      21 | 6434 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6435 | `	if( pArray == 0 ){` |
|     ! 0 | 6436 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6437 | `	}` |
|       - | 6438 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6439 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6440 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6441 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6442 | `	if( nEntry < 0 ){` |
|       9 | 6443 | `		nEntry = -nEntry;` |
|       9 | 6444 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6445 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6446 | `			/* Insert given items first */` |
|      17 | 6447 | `			while( nEntry > 0 ){` |
|      13 | 6448 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6449 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6450 | `				}` |
|      13 | 6451 | `				nEntry--;` |
|       1 | 6452 | `			}` |
|       - | 6453 | `			/* Merge the two arrays */` |
|       5 | 6454 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6455 | `		}else{` |
|       5 | 6456 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6457 | `		}` |
|      17 | 6458 | `	}else if( nEntry > 0 ){` |
|      11 | 6459 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6460 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6461 | `			/* Merge the two arrays first */` |
|       7 | 6462 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6463 | `			/* Insert given items */` |
|      25 | 6464 | `			while( nEntry > 0 ){` |
|      19 | 6465 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6466 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6467 | `				}` |
|      19 | 6468 | `				nEntry--;` |
|       1 | 6469 | `			}` |
|       4 | 6470 | `		}else{` |
|       5 | 6471 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6472 | `		}` |
|       6 | 6473 | `	}else{` |
|       - | 6474 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6475 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6476 | `	}` |
|       - | 6477 | `	/* Return the new array */` |
|      21 | 6478 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6479 | `	return PH7_OK;` |
|      19 | 6480 | `}` |
|       - | 6481 | `/*` |
|       - | 6482 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6483 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6484 | ` * Parameters` |
|       - | 6485 | ` * $array` |
|       - | 6486 | ` *   The array in which elements are replaced.` |
|       - | 6487 | ` * $array1` |
|       - | 6488 | ` *   The array from which elements will be extracted.` |
|       - | 6489 | ` * ....` |
|       - | 6490 | ` *  More arrays from which elements will be extracted.` |
|       - | 6491 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6492 | ` * Return` |
|       - | 6493 | ` *  Returns an array.` |
|       - | 6494 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6495 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6496 | ` */` |
|      22 | 6497 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 6498 | `{` |
|       - | 6499 | `	ph7_hashmap *pMap;` |
|       - | 6500 | `	ph7_value *pArray;` |
|       - | 6501 | `	int i;` |
|      26 | 6502 | `	if( nArg < 1 ){` |
|       3 | 6503 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6504 | `			"ArgumentCountError",` |
|       - | 6505 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6506 | `			);` |
|       - | 6507 | `	}` |
|      23 | 6508 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6509 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6510 | `			"TypeError",` |
|       - | 6511 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6512 | `			ph7_type_name(apArg[0])` |
|       - | 6513 | `			);` |
|       - | 6514 | `	}` |
|       - | 6515 | `	/* Create a new array */` |
|      20 | 6516 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6517 | `	if( pArray == 0 ){` |
|     ! 0 | 6518 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6519 | `		return PH7_OK;` |
|       - | 6520 | `	}` |
|       - | 6521 | `	/* Overwrite from the first array */` |
|      20 | 6522 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6523 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6524 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6525 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6526 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6527 | `			/* Type mismatch -> TypeError */` |
|       4 | 6528 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6529 | `				"TypeError",` |
|       - | 6530 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6531 | `				i + 1,` |
|       2 | 6532 | `				ph7_type_name(apArg[i])` |
|       - | 6533 | `				);` |
|       - | 6534 | `		}` |
|       - | 6535 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6536 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6537 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6538 | `	}` |
|       - | 6539 | `	/* Return the new array */` |
|      17 | 6540 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6541 | `	return PH7_OK;` |
|      15 | 6542 | `}` |
|       - | 6543 | `/*` |
|       - | 6544 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6545 | ` *  Filters elements of an array using a callback function.` |
|       - | 6546 | ` * Parameters` |
|       - | 6547 | ` *  $input` |
|       - | 6548 | ` *    The array to iterate over` |
|       - | 6549 | ` * $callback` |
|       - | 6550 | ` *    The callback function to use` |
|       - | 6551 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6552 | ` *    will be removed.` |
|       - | 6553 | ` * Return` |
|       - | 6554 | ` *  The filtered array.` |
|       - | 6555 | ` */` |
|      22 | 6556 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6557 | `{` |
|       - | 6558 | `	ph7_hashmap_node *pEntry;` |
|       - | 6559 | `	ph7_hashmap *pMap;` |
|       - | 6560 | `	ph7_value *pArray;` |
|       - | 6561 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6562 | `	ph7_value *pValue;` |
|       - | 6563 | `	sxi32 rc;` |
|       - | 6564 | `	int keep;` |
|       - | 6565 | `	sxu32 n;` |
|      24 | 6566 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6567 | `		/* Invalid arguments,return NULL */` |
|       5 | 6568 | `		ph7_result_null(pCtx);` |
|       5 | 6569 | `		return PH7_OK;` |
|       - | 6570 | `	}` |
|       - | 6571 | `	/* Create a new array */` |
|      20 | 6572 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6573 | `	if( pArray == 0 ){` |
|     ! 0 | 6574 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6575 | `		return PH7_OK;` |
|       - | 6576 | `	}` |
|       - | 6577 | `	/* Point to the internal representation of the input hashmap */` |
|      20 | 6578 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6579 | `	pEntry = pMap->pFirst;` |
|      20 | 6580 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      20 | 6581 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6582 | `	/* Perform the requested operation */` |
|      78 | 6583 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6584 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      64 | 6585 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      64 | 6586 | `		if( pValue == 0 ){` |
|       - | 6587 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6588 | `			keep = FALSE;` |
|      64 | 6589 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6590 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6591 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6592 | `				* silently dropped the element.  Emit similar message. */` |
|      36 | 6593 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6594 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6595 | `					int len;` |
|       3 | 6596 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6597 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6598 | `						"TypeError",` |
|       - | 6599 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6600 | `						zName` |
|       - | 6601 | `						);` |
|     ! 0 | 6602 | `				}else{` |
|     ! 0 | 6603 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6604 | `						"TypeError",` |
|       - | 6605 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6606 | `						ph7_type_name(apArg[1])` |
|       - | 6607 | `						);` |
|       - | 6608 | `				}` |
|       - | 6609 | `			}` |
|      33 | 6610 | `			keep = FALSE;` |
|      33 | 6611 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      33 | 6612 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 6613 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6614 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 6615 | `				return PH7_EXCEPTION;` |
|       - | 6616 | `			}` |
|      31 | 6617 | `			if( rc == SXRET_OK ){` |
|       - | 6618 | `				/* Perform a boolean cast */` |
|      31 | 6619 | `				keep = ph7_value_to_bool(&sResult);` |
|      15 | 6620 | `			}` |
|      31 | 6621 | `			PH7_MemObjRelease(&sResult);` |
|      16 | 6622 | `		}else{` |
|       - | 6623 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6624 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6625 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6626 | `			 */` |
|      29 | 6627 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6628 | `		}` |
|      59 | 6629 | `		if( keep ){` |
|       - | 6630 | `			/* Perform the insertion,now the callback returned true */` |
|      21 | 6631 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 6632 | `		}` |
|       - | 6633 | `		/* Point to the next entry */` |
|      59 | 6634 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      30 | 6635 | `	}` |
|      15 | 6636 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 6637 | `	return PH7_OK;` |
|      13 | 6638 | `}` |
|       - | 6639 | `/*` |
|       - | 6640 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 6641 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 6642 | ` * Parameters` |
|       - | 6643 | ` *  $callback` |
|       - | 6644 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 6645 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 6646 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 6647 | ` *   are zipped together.` |
|       - | 6648 | ` *  $array` |
|       - | 6649 | ` *   The first array to run through the callback function.` |
|       - | 6650 | ` *  $arrays` |
|       - | 6651 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 6652 | ` * Return` |
|       - | 6653 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 6654 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 6655 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 6656 | ` *  padding shorter arrays with NULL.` |
|       - | 6657 | ` */` |
|      54 | 6658 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6659 | `{` |
|       - | 6660 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6661 | `	ph7_hashmap_node *pEntry;` |
|       - | 6662 | `	ph7_hashmap *pMap;` |
|       - | 6663 | `	ph7_vm *pVm;` |
|       - | 6664 | `	int bNullCallback;` |
|       - | 6665 | `	sxi32 rc;` |
|       - | 6666 | `	int i;` |
|       - | 6667 | `	sxu32 n;` |
|      59 | 6668 | `	if( nArg < 2 ){` |
|       8 | 6669 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6670 | `			"ArgumentCountError",` |
|       - | 6671 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6672 | `			nArg` |
|       - | 6673 | `			);` |
|       - | 6674 | `	}` |
|      53 | 6675 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      53 | 6676 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6677 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6678 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6679 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6680 | `				"TypeError",` |
|       - | 6681 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6682 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6683 | `				zFunc` |
|       - | 6684 | `				);` |
|       - | 6685 | `		}` |
|       3 | 6686 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6687 | `			"TypeError",` |
|       - | 6688 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6689 | `			"no array or string given"` |
|       - | 6690 | `			);` |
|       - | 6691 | `	}` |
|       - | 6692 | `	/* Every remaining argument must be an array */` |
|     105 | 6693 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      61 | 6694 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 6695 | `			if( i == 1 ){` |
|       4 | 6696 | `				return PH7_VmThrowException(pCtx,` |
|       - | 6697 | `					"TypeError",` |
|       - | 6698 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6699 | `					ph7_type_name(apArg[1])` |
|       - | 6700 | `					);` |
|       - | 6701 | `			}` |
|     ! 0 | 6702 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6703 | `				"TypeError",` |
|       - | 6704 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 6705 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 6706 | `				);` |
|       - | 6707 | `		}` |
|      30 | 6708 | `	}` |
|      46 | 6709 | `	pVm = pCtx->pVm;` |
|       - | 6710 | `	/* Create a new array */` |
|      46 | 6711 | `	pArray = ph7_context_new_array(pCtx);` |
|      46 | 6712 | `	if( pArray == 0 ){` |
|     ! 0 | 6713 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6714 | `		return PH7_OK;` |
|       - | 6715 | `	}` |
|      46 | 6716 | `	PH7_MemObjInit(pVm,&sResult);` |
|      46 | 6717 | `	PH7_MemObjInit(pVm,&sKey);` |
|      46 | 6718 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      46 | 6719 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      46 | 6720 | `	if( nArg == 2 ){` |
|       - | 6721 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      36 | 6722 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      36 | 6723 | `		pEntry = pMap->pFirst;` |
|     110 | 6724 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6725 | `			/* Extract the node value */` |
|      78 | 6726 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|      78 | 6727 | `			if( pValue ){` |
|       - | 6728 | `				/* Extract the node key */` |
|      78 | 6729 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      78 | 6730 | `				if( bNullCallback ){` |
|       - | 6731 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 6732 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6733 | `				}else{` |
|       - | 6734 | `					/* Invoke the supplied callback */` |
|      68 | 6735 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|      68 | 6736 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 6737 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 6738 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       3 | 6739 | `						PH7_MemObjRelease(&sKey);` |
|       3 | 6740 | `						PH7_MemObjRelease(&sResult);` |
|       3 | 6741 | `						return PH7_EXCEPTION;` |
|       - | 6742 | `					}` |
|       - | 6743 | `					/* Insert the callback return value */` |
|      66 | 6744 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6745 | `				}` |
|      76 | 6746 | `				PH7_MemObjRelease(&sKey);` |
|      76 | 6747 | `				PH7_MemObjRelease(&sResult);` |
|      37 | 6748 | `			}` |
|       - | 6749 | `			/* Point to the next entry */` |
|      76 | 6750 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      39 | 6751 | `		}` |
|      18 | 6752 | `	}else{` |
|       - | 6753 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 6754 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 6755 | `		int nArrays = nArg - 1;` |
|       - | 6756 | `		ph7_hashmap_node **apCur;` |
|       - | 6757 | `		ph7_value **apCallArg;` |
|       - | 6758 | `		ph7_value sNull;` |
|      11 | 6759 | `		sxu32 nMax = 0;` |
|      11 | 6760 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 6761 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 6762 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 6763 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 6764 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 6765 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6766 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6767 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 6768 | `			return PH7_OK;` |
|       - | 6769 | `		}` |
|      11 | 6770 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 6771 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 6772 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 6773 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 6774 | `			apCur[i] = pMap->pFirst;` |
|      23 | 6775 | `			if( pMap->nEntry > nMax ){` |
|      13 | 6776 | `				nMax = pMap->nEntry;` |
|       6 | 6777 | `			}` |
|      12 | 6778 | `		}` |
|      35 | 6779 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 6780 | `			ph7_value *pZip = 0;` |
|      25 | 6781 | `			if( bNullCallback ){` |
|       - | 6782 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 6783 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 6784 | `			}` |
|      79 | 6785 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 6786 | `				ph7_value *pv = &sNull;` |
|      55 | 6787 | `				if( apCur[i] ){` |
|      53 | 6788 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 6789 | `					if( pNodeVal ){` |
|      53 | 6790 | `						pv = pNodeVal;` |
|      26 | 6791 | `					}` |
|      53 | 6792 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 6793 | `				}` |
|      55 | 6794 | `				if( bNullCallback ){` |
|       9 | 6795 | `					if( pZip ){` |
|       9 | 6796 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 6797 | `					}` |
|       5 | 6798 | `				}else{` |
|      47 | 6799 | `					apCallArg[i] = pv;` |
|       - | 6800 | `				}` |
|      28 | 6801 | `			}` |
|      25 | 6802 | `			if( bNullCallback ){` |
|       5 | 6803 | `				if( pZip ){` |
|       5 | 6804 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 6805 | `				}` |
|       3 | 6806 | `			}else{` |
|      21 | 6807 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 6808 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6809 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 6810 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 6811 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 6812 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6813 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6814 | `					return PH7_EXCEPTION;` |
|       - | 6815 | `				}` |
|      21 | 6816 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 6817 | `				PH7_MemObjRelease(&sResult);` |
|       - | 6818 | `			}` |
|      13 | 6819 | `		}` |
|      11 | 6820 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 6821 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 6822 | `		PH7_MemObjRelease(&sNull);` |
|       - | 6823 | `	}` |
|      44 | 6824 | `	PH7_MemObjRelease(&sKey);` |
|      44 | 6825 | `	PH7_MemObjRelease(&sResult);` |
|      44 | 6826 | `	ph7_result_value(pCtx,pArray);` |
|      44 | 6827 | `	return PH7_OK;` |
|      32 | 6828 | `}` |
|       - | 6829 | `/*` |
|       - | 6830 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6831 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6832 | ` * Parameters` |
|       - | 6833 | ` *  $array` |
|       - | 6834 | ` *   The input array.` |
|       - | 6835 | ` *  $callback` |
|       - | 6836 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6837 | ` *  $initial` |
|       - | 6838 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6839 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6840 | ` * Return` |
|       - | 6841 | ` *  Returns the resulting value.` |
|       - | 6842 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6843 | ` */` |
|      34 | 6844 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6845 | `{` |
|       - | 6846 | `	ph7_hashmap_node *pEntry;` |
|       - | 6847 | `	ph7_hashmap *pMap;` |
|       - | 6848 | `	ph7_value *pValue;` |
|       - | 6849 | `	ph7_value sResult;` |
|       - | 6850 | `	sxi32 rc;` |
|       - | 6851 | `	sxu32 n;` |
|      39 | 6852 | `	if( nArg < 2 ){` |
|       8 | 6853 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6854 | `			"ArgumentCountError",` |
|       - | 6855 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6856 | `			nArg` |
|       - | 6857 | `			);` |
|       - | 6858 | `	}` |
|      35 | 6859 | `	if( nArg > 3 ){` |
|       4 | 6860 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6861 | `			"ArgumentCountError",` |
|       - | 6862 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6863 | `			nArg` |
|       - | 6864 | `			);` |
|       - | 6865 | `	}` |
|      33 | 6866 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6867 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6868 | `			"TypeError",` |
|       - | 6869 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6870 | `			ph7_type_name(apArg[0])` |
|       - | 6871 | `			);` |
|       - | 6872 | `	}` |
|      31 | 6873 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      12 | 6874 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6875 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6876 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6877 | `				"TypeError",` |
|       - | 6878 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6879 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6880 | `				zFunc` |
|       - | 6881 | `				);` |
|       - | 6882 | `		}` |
|       9 | 6883 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6884 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6885 | `				"TypeError",` |
|       - | 6886 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6887 | `				"array callback must have exactly two members"` |
|       - | 6888 | `				);` |
|       - | 6889 | `		}` |
|       6 | 6890 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6891 | `			"TypeError",` |
|       - | 6892 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6893 | `			"no array or string given"` |
|       - | 6894 | `			);` |
|       - | 6895 | `	}` |
|       - | 6896 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6897 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6898 | `	/* Assume a NULL initial value */` |
|      19 | 6899 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 6900 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 6901 | `	if( nArg > 2 ){` |
|       - | 6902 | `		/* Set the initial value */` |
|      13 | 6903 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 6904 | `	}` |
|       - | 6905 | `	/* Perform the requested operation */` |
|      19 | 6906 | `	pEntry = pMap->pFirst;` |
|      55 | 6907 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6908 | `		/* Extract the node value */` |
|      39 | 6909 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6910 | `		/* Invoke the supplied callback */` |
|      39 | 6911 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      39 | 6912 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 6913 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6914 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 6915 | `			return PH7_EXCEPTION;` |
|       - | 6916 | `		}` |
|       - | 6917 | `		/* Point to the next entry */` |
|      37 | 6918 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6919 | `	}` |
|      17 | 6920 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      17 | 6921 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 6922 | `	return PH7_OK;` |
|      22 | 6923 | `}` |
|       - | 6924 | `/*` |
|       - | 6925 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6926 | ` *  Apply a user function to every member of an array.` |
|       - | 6927 | ` * Parameters` |
|       - | 6928 | ` *  $array` |
|       - | 6929 | ` *   The input array.` |
|       - | 6930 | ` *  $funcname` |
|       - | 6931 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6932 | ` *   the first, and the key/index second.` |
|       - | 6933 | ` * Note:` |
|       - | 6934 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6935 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6936 | ` *  be made in the original array itself.` |
|       - | 6937 | ` *  $userdata` |
|       - | 6938 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6939 | ` *   to the callback funcname.` |
|       - | 6940 | ` * Return` |
|       - | 6941 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6942 | ` */` |
|      38 | 6943 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6944 | `{` |
|       - | 6945 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6946 | `	ph7_hashmap_node *pEntry;` |
|       - | 6947 | `	ph7_hashmap *pMap;` |
|       - | 6948 | `	sxu32 n;` |
|      43 | 6949 | `	if( nArg < 2 ){` |
|       8 | 6950 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6951 | `			"ArgumentCountError",` |
|       - | 6952 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6953 | `			nArg` |
|       - | 6954 | `			);` |
|       - | 6955 | `	}` |
|      39 | 6956 | `	if( nArg > 3 ){` |
|       4 | 6957 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6958 | `			"ArgumentCountError",` |
|       - | 6959 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6960 | `			nArg` |
|       - | 6961 | `			);` |
|       - | 6962 | `	}` |
|      37 | 6963 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6964 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6965 | `			"TypeError",` |
|       - | 6966 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6967 | `			ph7_type_name(apArg[0])` |
|       - | 6968 | `			);` |
|       - | 6969 | `	}` |
|      35 | 6970 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 6971 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6972 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6973 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6974 | `				"TypeError",` |
|       - | 6975 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6976 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6977 | `				zFunc` |
|       - | 6978 | `				);` |
|       - | 6979 | `		}` |
|      12 | 6980 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 6981 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6982 | `				"TypeError",` |
|       - | 6983 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6984 | `				"array callback must have exactly two members"` |
|       - | 6985 | `				);` |
|       - | 6986 | `		}` |
|       6 | 6987 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6988 | `			"TypeError",` |
|       - | 6989 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6990 | `			"no array or string given"` |
|       - | 6991 | `			);` |
|       - | 6992 | `	}` |
|      21 | 6993 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6994 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6995 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 6996 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 6997 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 6998 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6999 | `	/* Perform the desired operation */` |
|      21 | 7000 | `	pEntry = pMap->pFirst;` |
|      61 | 7001 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7002 | `		/* Extract the node value */` |
|      43 | 7003 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 7004 | `		if( pValue ){` |
|       - | 7005 | `			sxi32 rcW;` |
|       - | 7006 | `			/* Extract the entry key */` |
|      43 | 7007 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 7008 | `			/* Invoke the supplied callback */` |
|      43 | 7009 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 7010 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 7011 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 7012 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 7013 | `				return PH7_EXCEPTION;` |
|       - | 7014 | `			}` |
|      20 | 7015 | `		}` |
|       - | 7016 | `		/* Point to the next entry */` |
|      41 | 7017 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 7018 | `	}` |
|       - | 7019 | `	/* All done, return TRUE */` |
|      19 | 7020 | `	ph7_result_bool(pCtx,1);` |
|      19 | 7021 | `	return PH7_OK;` |
|      24 | 7022 | `}` |
|       - | 7023 | `/*` |
|       - | 7024 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 7025 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 7026 | ` */` |
|      22 | 7027 | `static sxi32 HashmapWalkRecursive(` |
|       - | 7028 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 7029 | `	ph7_value *pCallback, /* User callback */` |
|       - | 7030 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 7031 | `	int iNest             /* Nesting level */` |
|       - | 7032 | `	)` |
|       1 | 7033 | `{` |
|       - | 7034 | `	ph7_hashmap_node *pEntry;` |
|       - | 7035 | `	ph7_value *pValue,sKey;` |
|       - | 7036 | `	sxi32 rc;` |
|       - | 7037 | `	sxu32 n;` |
|       - | 7038 | `	/* Iterate through hashmap entries */` |
|      23 | 7039 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 7040 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 7041 | `	pEntry = pMap->pFirst;` |
|      59 | 7042 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7043 | `		/* Extract the node value */` |
|      37 | 7044 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 7045 | `		if( pValue ){` |
|      37 | 7046 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 7047 | `				if( iNest < 32 ){` |
|       - | 7048 | `					/* Recurse */` |
|      11 | 7049 | `					iNest++;` |
|      11 | 7050 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 7051 | `					iNest--;` |
|      11 | 7052 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 7053 | `						return PH7_EXCEPTION;` |
|       - | 7054 | `					}` |
|       5 | 7055 | `				}` |
|       6 | 7056 | `			}else{` |
|       - | 7057 | `				/* Extract the node key */` |
|      27 | 7058 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 7059 | `				/* Invoke the supplied callback */` |
|      27 | 7060 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 7061 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 7062 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 7063 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7064 | `					return PH7_EXCEPTION;` |
|       - | 7065 | `				}` |
|       - | 7066 | `			}` |
|      18 | 7067 | `		}` |
|       - | 7068 | `		/* Point to the next entry */` |
|      37 | 7069 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 7070 | `	}` |
|      23 | 7071 | `	return PH7_OK;` |
|      12 | 7072 | `}` |
|       - | 7073 | `/*` |
|       - | 7074 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 7075 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 7076 | ` * Parameters` |
|       - | 7077 | ` *  $array` |
|       - | 7078 | ` *   The input array.` |
|       - | 7079 | ` *  $funcname` |
|       - | 7080 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 7081 | ` *   the first, and the key/index second.` |
|       - | 7082 | ` * Note:` |
|       - | 7083 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 7084 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 7085 | ` *  be made in the original array itself.` |
|       - | 7086 | ` *  $userdata` |
|       - | 7087 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 7088 | ` *   to the callback funcname.` |
|       - | 7089 | ` * Return` |
|       - | 7090 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 7091 | ` */` |
|      30 | 7092 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7093 | `{` |
|       - | 7094 | `	ph7_hashmap *pMap;` |
|      35 | 7095 | `	if( nArg < 2 ){` |
|       8 | 7096 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7097 | `			"ArgumentCountError",` |
|       - | 7098 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 7099 | `			nArg` |
|       - | 7100 | `			);` |
|       - | 7101 | `	}` |
|      31 | 7102 | `	if( nArg > 3 ){` |
|       4 | 7103 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7104 | `			"ArgumentCountError",` |
|       - | 7105 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 7106 | `			nArg` |
|       - | 7107 | `			);` |
|       - | 7108 | `	}` |
|      29 | 7109 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7110 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7111 | `			"TypeError",` |
|       - | 7112 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7113 | `			ph7_type_name(apArg[0])` |
|       - | 7114 | `			);` |
|       - | 7115 | `	}` |
|      27 | 7116 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 7117 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7118 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7119 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7120 | `				"TypeError",` |
|       - | 7121 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7122 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7123 | `				zFunc` |
|       - | 7124 | `				);` |
|       - | 7125 | `		}` |
|      12 | 7126 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 7127 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7128 | `				"TypeError",` |
|       - | 7129 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7130 | `				"array callback must have exactly two members"` |
|       - | 7131 | `				);` |
|       - | 7132 | `		}` |
|       6 | 7133 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7134 | `			"TypeError",` |
|       - | 7135 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7136 | `			"no array or string given"` |
|       - | 7137 | `			);` |
|       - | 7138 | `	}` |
|       - | 7139 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 7140 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 7141 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7142 | `	/* Perform the desired operation */` |
|      13 | 7143 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 7144 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7145 | `		return PH7_EXCEPTION;` |
|       - | 7146 | `	}` |
|       - | 7147 | `	/* All done, return TRUE */` |
|      13 | 7148 | `	ph7_result_bool(pCtx,1);` |
|      13 | 7149 | `	return PH7_OK;` |
|      20 | 7150 | `}` |
|       - | 7151 | `/*` |
|       - | 7152 | ` * bool array_is_list(array $array)` |
|       - | 7153 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 7154 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 7155 | ` * Return` |
|       - | 7156 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 7157 | ` */` |
|       - | 7158 | `/*` |
|       - | 7159 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 7160 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 7161 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 7162 | ` */` |
|     114 | 7163 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       1 | 7164 | `{` |
|     115 | 7165 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|     115 | 7166 | `	sxi64 iExpect = 0;` |
|       - | 7167 | `	sxu32 n;` |
|     233 | 7168 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     169 | 7169 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 7170 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|      51 | 7171 | `			return 0;` |
|       - | 7172 | `		}` |
|     119 | 7173 | `		++iExpect;` |
|     119 | 7174 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      60 | 7175 | `	}` |
|      65 | 7176 | `	return 1;` |
|      58 | 7177 | `}` |
|      12 | 7178 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7179 | `{` |
|      13 | 7180 | `	if( nArg < 1 ){` |
|     ! 0 | 7181 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7182 | `			"ArgumentCountError",` |
|       - | 7183 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 7184 | `			);` |
|       - | 7185 | `	}` |
|      13 | 7186 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7187 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7188 | `			"TypeError",` |
|       - | 7189 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7190 | `			ph7_type_name(apArg[0])` |
|       - | 7191 | `			);` |
|       - | 7192 | `	}` |
|      13 | 7193 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 7194 | `	return PH7_OK;` |
|       7 | 7195 | `}` |
|       - | 7196 | `/*` |
|       - | 7197 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 7198 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 7199 | ` * array_column() for both the column value and the index key.` |
|       - | 7200 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 7201 | ` * container or the key is absent.` |
|       - | 7202 | ` */` |
|      32 | 7203 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 7204 | `{` |
|      33 | 7205 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 7206 | `		ph7_hashmap_node *pNode;` |
|      25 | 7207 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 7208 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 7209 | `		}` |
|      11 | 7210 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 7211 | `		ph7_value sName;` |
|       - | 7212 | `		const char *zName;` |
|       - | 7213 | `		ph7_value *pAttr;` |
|       - | 7214 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 7215 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 7216 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 7217 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 7218 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 7219 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 7220 | `		PH7_MemObjRelease(&sName);` |
|       9 | 7221 | `		return pAttr;` |
|       - | 7222 | `	}` |
|       5 | 7223 | `	return 0;` |
|      17 | 7224 | `}` |
|       - | 7225 | `/*` |
|       - | 7226 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 7227 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 7228 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 7229 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 7230 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 7231 | ` *  Each row may be an array or an object.` |
|       - | 7232 | ` */` |
|      12 | 7233 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7234 | `{` |
|       - | 7235 | `	ph7_hashmap_node *pNode;` |
|       - | 7236 | `	ph7_hashmap *pMap;` |
|       - | 7237 | `	ph7_value *pArray;` |
|       - | 7238 | `	ph7_value *pRow;` |
|       - | 7239 | `	ph7_value *pCol;` |
|       - | 7240 | `	ph7_value *pIdx;` |
|       - | 7241 | `	int bWantCol;` |
|       - | 7242 | `	int bWantIdx;` |
|       - | 7243 | `	sxu32 n;` |
|      13 | 7244 | `	if( nArg < 2 ){` |
|     ! 0 | 7245 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7246 | `			"ArgumentCountError",` |
|       - | 7247 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 7248 | `			nArg` |
|       - | 7249 | `			);` |
|       - | 7250 | `	}` |
|      13 | 7251 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7252 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7253 | `			"TypeError",` |
|       - | 7254 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7255 | `			ph7_type_name(apArg[0])` |
|       - | 7256 | `			);` |
|       - | 7257 | `	}` |
|      13 | 7258 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 7259 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 7260 | `	if( pArray == 0 ){` |
|     ! 0 | 7261 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7262 | `		return PH7_OK;` |
|       - | 7263 | `	}` |
|       - | 7264 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 7265 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 7266 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 7267 | `	pNode = pMap->pFirst;` |
|      33 | 7268 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 7269 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 7270 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 7271 | `		if( pRow == 0 ){` |
|     ! 0 | 7272 | `			continue;` |
|       - | 7273 | `		}` |
|      21 | 7274 | `		if( bWantCol ){` |
|      19 | 7275 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 7276 | `			if( pCol == 0 ){` |
|       - | 7277 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 7278 | `				continue;` |
|       - | 7279 | `			}` |
|       9 | 7280 | `		}else{` |
|       3 | 7281 | `			pCol = pRow;` |
|       - | 7282 | `		}` |
|      19 | 7283 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 7284 | `		if( pIdx ){` |
|      13 | 7285 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 7286 | `		}else{` |
|       7 | 7287 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 7288 | `		}` |
|      10 | 7289 | `	}` |
|      13 | 7290 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 7291 | `	return PH7_OK;` |
|       7 | 7292 | `}` |
|       - | 7293 | `/*` |
|       - | 7294 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 7295 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 7296 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 7297 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 7298 | ` */` |
|      28 | 7299 | `static sxi32 HashmapCallbackSearch(` |
|       - | 7300 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 7301 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 7302 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 7303 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 7304 | `	)` |
|       1 | 7305 | `{` |
|       - | 7306 | `	ph7_hashmap_node *pEntry;` |
|       - | 7307 | `	ph7_hashmap *pMap;` |
|       - | 7308 | `	ph7_value *pValue;` |
|       - | 7309 | `	ph7_value *apCbArg[2];` |
|       - | 7310 | `	ph7_value sKey;` |
|       - | 7311 | `	ph7_value sResult;` |
|       - | 7312 | `	sxi32 rc;` |
|       - | 7313 | `	sxu32 n;` |
|      29 | 7314 | `	*ppMatch = 0;` |
|      29 | 7315 | `	if( nArg < 2 ){` |
|     ! 0 | 7316 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7317 | `			"ArgumentCountError",` |
|       - | 7318 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 7319 | `			zName,nArg` |
|       - | 7320 | `			);` |
|       - | 7321 | `	}` |
|      29 | 7322 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7323 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7324 | `			"TypeError",` |
|       - | 7325 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7326 | `			zName,ph7_type_name(apArg[0])` |
|       - | 7327 | `			);` |
|       - | 7328 | `	}` |
|      29 | 7329 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7330 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7331 | `			"TypeError",` |
|       - | 7332 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 7333 | `			zName,ph7_type_name(apArg[1])` |
|       - | 7334 | `			);` |
|       - | 7335 | `	}` |
|      29 | 7336 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 7337 | `	pEntry = pMap->pFirst;` |
|      29 | 7338 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 7339 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 7340 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 7341 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 7342 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 7343 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 7344 | `		if( pValue ){` |
|       - | 7345 | `			/* The callback receives ($value, $key). */` |
|      59 | 7346 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 7347 | `			apCbArg[0] = pValue;` |
|      59 | 7348 | `			apCbArg[1] = &sKey;` |
|      59 | 7349 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 7350 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 7351 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7352 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7353 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7354 | `				return PH7_EXCEPTION;` |
|       - | 7355 | `			}` |
|      59 | 7356 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 7357 | `				*ppMatch = pEntry;` |
|      15 | 7358 | `				break;` |
|       - | 7359 | `			}` |
|      22 | 7360 | `		}` |
|      45 | 7361 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 7362 | `	}` |
|      29 | 7363 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 7364 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 7365 | `	return PH7_OK;` |
|      15 | 7366 | `}` |
|       - | 7367 | `/*` |
|       - | 7368 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 7369 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 7370 | ` *  is truthy, or NULL if none match.` |
|       - | 7371 | ` */` |
|       6 | 7372 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7373 | `{` |
|       - | 7374 | `	ph7_hashmap_node *pMatch;` |
|       - | 7375 | `	ph7_value *pVal;` |
|       - | 7376 | `	sxi32 rc;` |
|       7 | 7377 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 7378 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7379 | `		return rc;` |
|       - | 7380 | `	}` |
|       7 | 7381 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 7382 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 7383 | `	}else{` |
|       3 | 7384 | `		ph7_result_null(pCtx);` |
|       - | 7385 | `	}` |
|       7 | 7386 | `	return PH7_OK;` |
|       4 | 7387 | `}` |
|       - | 7388 | `/*` |
|       - | 7389 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 7390 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 7391 | ` *  is truthy, or NULL if none match.` |
|       - | 7392 | ` */` |
|       6 | 7393 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7394 | `{` |
|       - | 7395 | `	ph7_hashmap_node *pMatch;` |
|       - | 7396 | `	sxi32 rc;` |
|       7 | 7397 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 7398 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7399 | `		return rc;` |
|       - | 7400 | `	}` |
|       7 | 7401 | `	if( pMatch == 0 ){` |
|       3 | 7402 | `		ph7_result_null(pCtx);` |
|       6 | 7403 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 7404 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 7405 | `	}else{` |
|       4 | 7406 | `		ph7_result_string(pCtx,` |
|       2 | 7407 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 7408 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 7409 | `	}` |
|       7 | 7410 | `	return PH7_OK;` |
|       4 | 7411 | `}` |
|       - | 7412 | `/*` |
|       - | 7413 | ` * bool array_any(array $array, callable $callback)` |
|       - | 7414 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 7415 | ` *  FALSE for an empty array.` |
|       - | 7416 | ` */` |
|       8 | 7417 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7418 | `{` |
|       - | 7419 | `	ph7_hashmap_node *pMatch;` |
|       - | 7420 | `	sxi32 rc;` |
|       9 | 7421 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 7422 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7423 | `		return rc;` |
|       - | 7424 | `	}` |
|       9 | 7425 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 7426 | `	return PH7_OK;` |
|       5 | 7427 | `}` |
|       - | 7428 | `/*` |
|       - | 7429 | ` * bool array_all(array $array, callable $callback)` |
|       - | 7430 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 7431 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 7432 | ` */` |
|       8 | 7433 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7434 | `{` |
|       - | 7435 | `	ph7_hashmap_node *pMatch;` |
|       - | 7436 | `	sxi32 rc;` |
|       9 | 7437 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 7438 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7439 | `		return rc;` |
|       - | 7440 | `	}` |
|       9 | 7441 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 7442 | `	return PH7_OK;` |
|       5 | 7443 | `}` |
|       - | 7444 | `/*` |
|       - | 7445 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 7446 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 7447 | ` */` |
|       - | 7448 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 7449 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|      70 | 7450 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       5 | 7451 | `{` |
|      75 | 7452 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|      35 | 7453 | `	(void)pVm;` |
|      75 | 7454 | `	p->nCount++;` |
|      75 | 7455 | `	if( p->pArray ){` |
|       - | 7456 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 7457 | `		 * otherwise append with an auto-assigned int index. */` |
|      67 | 7458 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|      31 | 7459 | `	}` |
|      75 | 7460 | `	return SXRET_OK;` |
|       5 | 7461 | `}` |
|       - | 7462 | `/*` |
|       - | 7463 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 7464 | ` */` |
|      26 | 7465 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 7466 | `{` |
|       - | 7467 | `	struct IterCollect sCol;` |
|       - | 7468 | `	ph7_value *pArray;` |
|       - | 7469 | `	sxi32 rc;` |
|      31 | 7470 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      31 | 7471 | `	pArray = ph7_context_new_array(pCtx);` |
|      31 | 7472 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      31 | 7473 | `	sCol.pArray = pArray;` |
|      31 | 7474 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|      31 | 7475 | `	sCol.nCount = 0;` |
|      31 | 7476 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 7477 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 7478 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 7479 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7480 | `		sxu32 n;` |
|       9 | 7481 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7482 | `			ph7_value sKey, *pVal;` |
|       7 | 7483 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 7484 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 7485 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 7486 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 7487 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 7488 | `			pEntry = pEntry->pPrev;` |
|       4 | 7489 | `		}` |
|       3 | 7490 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 7491 | `		return PH7_OK;` |
|       - | 7492 | `	}` |
|      29 | 7493 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      29 | 7494 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      27 | 7495 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7496 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7497 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 7498 | `			ph7_type_name(apArg[0]));` |
|       - | 7499 | `	}` |
|      27 | 7500 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 7501 | `	return PH7_OK;` |
|      18 | 7502 | `}` |
|       - | 7503 | `/*` |
|       - | 7504 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 7505 | ` */` |
|       6 | 7506 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 7507 | `{` |
|       - | 7508 | `	struct IterCollect sCol;` |
|       - | 7509 | `	sxi32 rc;` |
|       7 | 7510 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       7 | 7511 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 7512 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 7513 | `		return PH7_OK;` |
|       - | 7514 | `	}` |
|       5 | 7515 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|       5 | 7516 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|       5 | 7517 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       5 | 7518 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7519 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7520 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 7521 | `			ph7_type_name(apArg[0]));` |
|       - | 7522 | `	}` |
|       5 | 7523 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|       5 | 7524 | `	return PH7_OK;` |
|       4 | 7525 | `}` |
|       - | 7526 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 7527 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 7528 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 7529 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      24 | 7530 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       1 | 7531 | `{` |
|      25 | 7532 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 7533 | `	ph7_value sResult;` |
|       - | 7534 | `	SySet aArg;` |
|       - | 7535 | `	sxi32 rc;` |
|       - | 7536 | `	int bContinue;` |
|      12 | 7537 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      25 | 7538 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      25 | 7539 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 7540 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|       9 | 7541 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7542 | `		sxu32 n;` |
|      17 | 7543 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       9 | 7544 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       9 | 7545 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|       9 | 7546 | `			pEntry = pEntry->pPrev;` |
|       5 | 7547 | `		}` |
|       4 | 7548 | `	}` |
|      25 | 7549 | `	PH7_MemObjInit(pVm,&sResult);` |
|      37 | 7550 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      24 | 7551 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|      25 | 7552 | `	SySetRelease(&aArg);` |
|      25 | 7553 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      23 | 7554 | `	p->nCount++;` |
|      23 | 7555 | `	PH7_MemObjToBool(&sResult);` |
|      23 | 7556 | `	bContinue = (sResult.x.iVal != 0);` |
|      23 | 7557 | `	PH7_MemObjRelease(&sResult);` |
|      23 | 7558 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      13 | 7559 | `}` |
|       - | 7560 | `/*` |
|       - | 7561 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 7562 | ` */` |
|       8 | 7563 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 7564 | `{` |
|       - | 7565 | `	struct IterApply sApp;` |
|       - | 7566 | `	sxi32 rc;` |
|       9 | 7567 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       9 | 7568 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7569 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7570 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|       - | 7571 | `	}` |
|       9 | 7572 | `	sApp.pCallback = apArg[1];` |
|       9 | 7573 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|       9 | 7574 | `	sApp.nCount = 0;` |
|       9 | 7575 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|       9 | 7576 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       7 | 7577 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7578 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7579 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 7580 | `			ph7_type_name(apArg[0]));` |
|       - | 7581 | `	}` |
|       7 | 7582 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|       7 | 7583 | `	return PH7_OK;` |
|       5 | 7584 | `}` |
|       - | 7585 | `/*` |
|       - | 7586 | ` * Table of hashmap functions.` |
|       - | 7587 | ` */` |
|       - | 7588 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 7589 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|       - | 7590 | `	{"iterator_count",     ph7_iterator_count },` |
|       - | 7591 | `	{"iterator_apply",     ph7_iterator_apply },` |
|       - | 7592 | `	{"count",             ph7_hashmap_count },` |
|       - | 7593 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 7594 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 7595 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 7596 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 7597 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 7598 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 7599 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 7600 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 7601 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 7602 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 7603 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 7604 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 7605 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 7606 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 7607 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 7608 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 7609 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 7610 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 7611 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 7612 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 7613 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 7614 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 7615 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 7616 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 7617 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 7618 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 7619 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 7620 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 7621 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 7622 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 7623 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 7624 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 7625 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 7626 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 7627 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 7628 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 7629 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 7630 | `	{"array_column",      ph7_hashmap_column  },` |
|       - | 7631 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|       - | 7632 | `	{"array_find",        ph7_hashmap_find    },` |
|       - | 7633 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|       - | 7634 | `	{"array_any",         ph7_hashmap_any     },` |
|       - | 7635 | `	{"array_all",         ph7_hashmap_all     },` |
|       - | 7636 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 7637 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 7638 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 7639 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 7640 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 7641 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 7642 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 7643 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 7644 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 7645 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 7646 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 7647 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 7648 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 7649 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 7650 | `	{"range",             ph7_hashmap_range   },` |
|       - | 7651 | `	{"current",           ph7_hashmap_current },` |
|       - | 7652 | `	{"each",              ph7_hashmap_each    },` |
|       - | 7653 | `	{"pos",               ph7_hashmap_current },` |
|       - | 7654 | `	{"next",              ph7_hashmap_next    },` |
|       - | 7655 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 7656 | `	{"end",               ph7_hashmap_end     },` |
|       - | 7657 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 7658 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 7659 | `};` |
|       - | 7660 | `/*` |
|       - | 7661 | ` * Register the built-in hashmap functions defined above.` |
|       - | 7662 | ` */` |
|    3454 | 7663 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       5 | 7664 | `{` |
|       - | 7665 | `	sxu32 n;` |
|  245239 | 7666 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  241785 | 7667 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|  120895 | 7668 | `	}` |
|    3459 | 7669 | `}` |
|       - | 7670 | `/*` |
|       - | 7671 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 7672 | ` * the BLOB given as the first argument.` |
|       - | 7673 | ` * This function is typically invoked when the user issue a call to` |
|       - | 7674 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 7675 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 7676 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 7677 | ` */` |
|       - | 7678 | `/*` |
|       - | 7679 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|       - | 7680 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|       - | 7681 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|       - | 7682 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|       - | 7683 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|       - | 7684 | ` */` |
|      26 | 7685 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 7686 | `{` |
|      28 | 7687 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7688 | `	ph7_value *pObj;` |
|      28 | 7689 | `	sxu32 n = 0;` |
|       - | 7690 | `	int isRef;` |
|      28 | 7691 | `	sxi32 rc = SXRET_OK;` |
|       - | 7692 | `	int i;` |
|      44 | 7693 | `	for(;;){` |
|      90 | 7694 | `		if( n >= pMap->nEntry ){` |
|      28 | 7695 | `			break;` |
|       - | 7696 | `		}` |
|     126 | 7697 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      64 | 7698 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      33 | 7699 | `		}` |
|       - | 7700 | `		/* Dump key */` |
|      64 | 7701 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 7702 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 7703 | `		}else{` |
|      47 | 7704 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      15 | 7705 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 7706 | `		}` |
|       - | 7707 | `#ifdef __WINNT__` |
|       2 | 7708 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7709 | `#else` |
|      62 | 7710 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7711 | `#endif` |
|       - | 7712 | `		/* Dump node value */` |
|      64 | 7713 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      64 | 7714 | `		isRef = 0;` |
|      64 | 7715 | `		if( pObj ){` |
|      64 | 7716 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 7717 | `				/* Referenced object */` |
|     ! 0 | 7718 | `				isRef = 1;` |
|     ! 0 | 7719 | `			}` |
|      64 | 7720 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|      64 | 7721 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 7722 | `				break;` |
|       - | 7723 | `			}` |
|      31 | 7724 | `		}` |
|       - | 7725 | `		/* Point to the next entry */` |
|      64 | 7726 | `		n++;` |
|      64 | 7727 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 7728 | `	}` |
|      28 | 7729 | `	return rc;` |
|       2 | 7730 | `}` |
|      22 | 7731 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 7732 | `{` |
|       - | 7733 | `	sxi32 rc;` |
|       - | 7734 | `	int i;` |
|      24 | 7735 | `	if( nDepth > 31 ){` |
|       - | 7736 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 7737 | `		/* Nesting limit reached */` |
|     ! 0 | 7738 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 7739 | `		if( ShowType ){` |
|     ! 0 | 7740 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 7741 | `		}` |
|     ! 0 | 7742 | `		return SXERR_LIMIT;` |
|       - | 7743 | `	}` |
|      24 | 7744 | `	if( !ShowType ){` |
|      11 | 7745 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       5 | 7746 | `	}` |
|       - | 7747 | `	/* Total entries */` |
|      24 | 7748 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 7749 | `#ifdef __WINNT__` |
|       2 | 7750 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7751 | `#else` |
|      22 | 7752 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7753 | `#endif` |
|      24 | 7754 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|      46 | 7755 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      24 | 7756 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      13 | 7757 | `	}` |
|      24 | 7758 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      24 | 7759 | `	return rc;` |
|      13 | 7760 | `}` |
|       - | 7761 | `/*` |
|       - | 7762 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 7763 | ` * retrieved entry.` |
|       - | 7764 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 7765 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 7766 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 7767 | ` * a value different from PH7_OK.` |
|       - | 7768 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 7769 | ` */` |
|   32862 | 7770 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 7771 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 7772 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 7773 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 7774 | `	)` |
|       5 | 7775 | `{` |
|       - | 7776 | `	ph7_hashmap_node *pEntry;` |
|       - | 7777 | `	ph7_value sKey,sValue;` |
|       - | 7778 | `	sxi32 rc;` |
|       - | 7779 | `	sxu32 n;` |
|       - | 7780 | `	/* Initialize walker parameter */` |
|   32867 | 7781 | `	rc = SXRET_OK;` |
|   32867 | 7782 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   32867 | 7783 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   32867 | 7784 | `	n = pMap->nEntry;` |
|   32867 | 7785 | `	pEntry = pMap->pFirst;` |
|       - | 7786 | `	/* Start the iteration process */` |
|   83371 | 7787 | `	for(;;){` |
|  166747 | 7788 | `		if( n < 1 ){` |
|   32867 | 7789 | `			break;` |
|       - | 7790 | `		}` |
|       - | 7791 | `		/* Extract a copy of the key and a copy the current value */` |
|  133885 | 7792 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  133885 | 7793 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 7794 | `		/* Invoke the user callback */` |
|  133885 | 7795 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 7796 | `		/* Release the copy of the key and the value */` |
|  133885 | 7797 | `		PH7_MemObjRelease(&sKey);` |
|  133885 | 7798 | `		PH7_MemObjRelease(&sValue);` |
|  133885 | 7799 | `		if( rc != PH7_OK ){` |
|       - | 7800 | `			/* Callback request an operation abort */` |
|     ! 0 | 7801 | `			return SXERR_ABORT;` |
|       - | 7802 | `		}` |
|       - | 7803 | `		/* Point to the next entry */` |
|  133885 | 7804 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  133885 | 7805 | `		n--;` |
|       5 | 7806 | `	}` |
|       - | 7807 | `	/* All done */` |
|   32867 | 7808 | `	return SXRET_OK;` |
|   16436 | 7809 | `}` |
|       - | 7810 |  |
