# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3357/3846 lines (87.29%)

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
| 3127104 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       5 |   19 | `{` |
| 3127109 |   20 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
| 3127109 |   21 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|       5 |   22 | `}` |
|       - |   23 | `/*` |
|       - |   24 | ` * Default hash function for string/BLOB keys.` |
|       - |   25 | ` */` |
|  394280 |   26 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       5 |   27 | `{` |
|  394285 |   28 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   29 | `	unsigned char *zEnd;` |
|  394285 |   30 | `	sxu32 nH = 5381;` |
|  394285 |   31 | `	zEnd = &zIn[nLen];` |
|  447413 |   32 | `	for(;;){` |
|  894831 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  771033 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  693649 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  592307 |   36 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       5 |   37 | `	}` |
|  394285 |   38 | `	return nH;` |
|       5 |   39 | `}` |
|       - |   40 | `/*` |
|       - |   41 | ` * Return the total number of entries in a given hashmap.` |
|       - |   42 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   43 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   44 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   45 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   46 | ` */` |
|     946 |   47 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       5 |   48 | `{` |
|     951 |   49 | `	sxi64 iCount = 0;` |
|     951 |   50 | `	if( !bRecursive ){` |
|     777 |   51 | `		iCount = pMap->nEntry;` |
|     391 |   52 | `	}else{` |
|       - |   53 | `		/* Recursive hashmap walk */` |
|     175 |   54 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|       - |   55 | `		ph7_value *pElem;` |
|     175 |   56 | `		sxu32 n = 0;` |
|       - |   57 | `		/* Mark this map as being counted */` |
|     175 |   58 | `		pMap->iFlags \|= HASHMAP_COUNTING;` |
|     209 |   59 | `		for(;;){` |
|     419 |   60 | `			if( n >= pMap->nEntry ){` |
|     175 |   61 | `				break;` |
|       - |   62 | `			}` |
|       - |   63 | `			/* Point to the element value */` |
|     245 |   64 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|     245 |   65 | `			if( pElem ){` |
|     245 |   66 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|     151 |   67 | `					ph7_hashmap *pSub = (ph7_hashmap *)pElem->x.pOther;` |
|     151 |   68 | `					if( pSub->iFlags & HASHMAP_COUNTING ){` |
|       - |   69 | `						/* Cycle detected — skip this entry */` |
|       3 |   70 | `						if( pCycleDetected ){` |
|       3 |   71 | `							*pCycleDetected = TRUE;` |
|       1 |   72 | `						}` |
|       2 |   73 | `					}else{` |
|     149 |   74 | `						iCount += HashmapCount(pSub,TRUE,pCycleDetected);` |
|       - |   75 | `					}` |
|      75 |   76 | `				}` |
|     122 |   77 | `			}` |
|       - |   78 | `			/* Point to the next entry */` |
|     245 |   79 | `			pEntry = pEntry->pNext;` |
|     245 |   80 | `			++n;` |
|       1 |   81 | `		}` |
|       - |   82 | `		/* Clear the counting flag */` |
|     175 |   83 | `		pMap->iFlags &= ~HASHMAP_COUNTING;` |
|       - |   84 | `		/* Update count */` |
|     175 |   85 | `		iCount += pMap->nEntry;` |
|       - |   86 | `	}` |
|     951 |   87 | `	return iCount;` |
|       5 |   88 | `}` |
|       - |   89 | `/*` |
|       - |   90 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   91 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   92 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   93 | ` */` |
| 3066026 |   94 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       5 |   95 | `{` |
|       - |   96 | `	ph7_hashmap_node *pNode;` |
|       - |   97 | `	/* Allocate a new node */` |
| 3066031 |   98 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 3066031 |   99 | `	if( pNode == 0 ){` |
|     ! 0 |  100 | `		return 0;` |
|       - |  101 | `	}` |
|       - |  102 | `	/* Zero the stucture */` |
| 3066031 |  103 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  104 | `	/* Fill in the structure */` |
| 3066031 |  105 | `	pNode->pMap  = &(*pMap);` |
| 3066031 |  106 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 3066031 |  107 | `	pNode->nHash = nHash;` |
| 3066031 |  108 | `	pNode->xKey.iKey = iKey;` |
| 3066031 |  109 | `	pNode->nValIdx  = nValIdx;` |
| 3066031 |  110 | `	return pNode;` |
| 1533018 |  111 | `}` |
|       - |  112 | `/*` |
|       - |  113 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  114 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  115 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  116 | ` */` |
|  143084 |  117 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       5 |  118 | `{` |
|       - |  119 | `	ph7_hashmap_node *pNode;` |
|       - |  120 | `	/* Allocate a new node */` |
|  143089 |  121 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  143089 |  122 | `	if( pNode == 0 ){` |
|     ! 0 |  123 | `		return 0;` |
|       - |  124 | `	}` |
|       - |  125 | `	/* Zero the stucture */` |
|  143089 |  126 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  127 | `	/* Fill in the structure */` |
|  143089 |  128 | `	pNode->pMap  = &(*pMap);` |
|  143089 |  129 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  143089 |  130 | `	pNode->nHash = nHash;` |
|  143089 |  131 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  143089 |  132 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  143089 |  133 | `	pNode->nValIdx = nValIdx;` |
|  143089 |  134 | `	return pNode;` |
|   71547 |  135 | `}` |
|       - |  136 | `/*` |
|       - |  137 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  138 | ` */` |
| 3209110 |  139 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       5 |  140 | `{` |
|       - |  141 | `	/* Link */` |
| 3209115 |  142 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2845927 |  143 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2845927 |  144 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1422961 |  145 | `	}` |
| 3209115 |  146 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  147 | `	/* Link to the map list */` |
| 3209115 |  148 | `	if( pMap->pFirst == 0 ){` |
|   65757 |  149 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  150 | `		/* Point to the first inserted node */` |
|   65757 |  151 | `		pMap->pCur = pNode;` |
|   32881 |  152 | `	}else{` |
| 3143363 |  153 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  154 | `	}` |
| 3209115 |  155 | `	++pMap->nEntry;` |
| 3209115 |  156 | `}` |
|       - |  157 | `/*` |
|       - |  158 | ` * Unlink a node from the hashmap.` |
|       - |  159 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  160 | ` */` |
|    7380 |  161 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       5 |  162 | `{` |
|    7385 |  163 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    7385 |  164 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  165 | `	/* Unlink from the corresponding bucket */` |
|    7385 |  166 | `	if( pNode->pPrevCollide == 0 ){` |
|    6933 |  167 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3469 |  168 | `	}else{` |
|     454 |  169 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  170 | `	}` |
|    7385 |  171 | `	if( pNode->pNextCollide ){` |
|    5782 |  172 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2890 |  173 | `	}` |
|    7385 |  174 | `	if( pMap->pFirst == pNode ){` |
|     131 |  175 | `		pMap->pFirst = pNode->pPrev;` |
|      63 |  176 | `	}` |
|    7385 |  177 | `	if( pMap->pCur == pNode ){` |
|       - |  178 | `		/* Advance the node cursor */` |
|     133 |  179 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      64 |  180 | `	}` |
|       - |  181 | `	/* Unlink from the map list */` |
|    7385 |  182 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    7385 |  183 | `	if( bRestore ){` |
|       - |  184 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     107 |  185 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  186 | `		/* Restore to the freelist */` |
|     107 |  187 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     107 |  188 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  189 | `		}` |
|      51 |  190 | `	}` |
|    7385 |  191 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    7250 |  192 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3623 |  193 | `	}` |
|    7385 |  194 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    7385 |  195 | `	pMap->nEntry--;` |
|    7385 |  196 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  197 | `		/* Free the hash-bucket */` |
|      75 |  198 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      75 |  199 | `		pMap->apBucket = 0;` |
|      75 |  200 | `		pMap->nSize = 0;` |
|      75 |  201 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      35 |  202 | `	}` |
|    7385 |  203 | `}` |
|       - |  204 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  205 | `/*` |
|       - |  206 | ` * Grow the hash-table and rehash all entries.` |
|       - |  207 | ` */` |
| 3209110 |  208 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       5 |  209 | `{` |
| 3209115 |  210 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   70391 |  211 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  212 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   70391 |  213 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  214 | `		sxu32 nBucket;` |
|       - |  215 | `		sxu32 n;` |
|   70391 |  216 | `		if( nNew < 1 ){` |
|   65757 |  217 | `			nNew = 16;` |
|   32876 |  218 | `		}` |
|       - |  219 | `		/* Allocate a new bucket */` |
|   70391 |  220 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   70391 |  221 | `		if( apNew == 0 ){` |
|     ! 0 |  222 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  223 | `				return SXERR_MEM; /* Fatal */` |
|       - |  224 | `			}` |
|       - |  225 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  226 | `			return SXRET_OK;` |
|       - |  227 | `		}` |
|       - |  228 | `		/* Zero the table */` |
|   70391 |  229 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  230 | `		/* Reflect the change */` |
|   70391 |  231 | `		pMap->apBucket = apNew;` |
|   70391 |  232 | `		pMap->nSize = nNew;` |
|   70391 |  233 | `		if( apOld == 0 ){` |
|       - |  234 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   65757 |  235 | `			return SXRET_OK;` |
|       - |  236 | `		}` |
|       - |  237 | `		/* Rehash old entries */` |
|    4639 |  238 | `		pEntry = pMap->pFirst;` |
|    4639 |  239 | `		n = 0;` |
| 2074669 |  240 | `		for( ;; ){` |
| 4149343 |  241 | `			if( n >= pMap->nEntry ){` |
|    4639 |  242 | `				break;` |
|       - |  243 | `			}` |
|       - |  244 | `			/* Clear the old collision link */` |
| 4144709 |  245 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  246 | `			/* Link to the new bucket */` |
| 4144709 |  247 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4144709 |  248 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3559121 |  249 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3559121 |  250 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1779558 |  251 | `			}` |
| 4144709 |  252 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  253 | `			/* Point to the next entry */` |
| 4144709 |  254 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4144709 |  255 | `			n++;` |
|       5 |  256 | `		}` |
|       - |  257 | `		/* Free the old table */` |
|    4639 |  258 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2317 |  259 | `	}` |
| 3143363 |  260 | `	return SXRET_OK;` |
| 1604560 |  261 | `}` |
|       - |  262 | `/*` |
|       - |  263 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  264 | ` * hashmap.` |
|       - |  265 | ` */` |
| 3066026 |  266 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  267 | `{` |
|       - |  268 | `	ph7_hashmap_node *pNode;` |
|       - |  269 | `	sxu32 nIdx;` |
|       - |  270 | `	sxu32 nHash;` |
|       - |  271 | `	sxi32 rc;` |
| 3066031 |  272 | `	if( !isForeign ){` |
|       - |  273 | `		ph7_value *pObj;` |
|       - |  274 | `		ph7_value sSafeVal;` |
|       - |  275 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  276 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  277 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  278 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  279 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  280 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
| 3065995 |  281 | `		if( pValue ){` |
| 3065993 |  282 | `			sSafeVal = *pValue;` |
| 3065993 |  283 | `			pValue = &sSafeVal;` |
| 1532994 |  284 | `		}` |
|       - |  285 | `		/* Reserve a ph7_value for the value */` |
| 3065995 |  286 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 3065995 |  287 | `		if( pObj == 0 ){` |
|     ! 0 |  288 | `			return SXERR_MEM;` |
|       - |  289 | `		}` |
| 3065995 |  290 | `		if( pValue ){` |
|       - |  291 | `			/* Duplicate the value */` |
| 3065993 |  292 | `			PH7_MemObjStore(pValue,pObj);` |
| 1532994 |  293 | `		}` |
| 3065995 |  294 | `		nIdx = pObj->nIdx;` |
| 1533000 |  295 | `	}else{` |
|      37 |  296 | `		nIdx = nRefIdx;` |
|       - |  297 | `	}` |
|       - |  298 | `	/* Hash the key */` |
| 3066031 |  299 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  300 | `	/* Allocate a new int node */` |
| 3066031 |  301 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 3066031 |  302 | `	if( pNode == 0 ){` |
|     ! 0 |  303 | `		return SXERR_MEM;` |
|       - |  304 | `	}` |
| 3066031 |  305 | `	if( isForeign ){` |
|       - |  306 | `		/* Mark as a foregin entry */` |
|      37 |  307 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      18 |  308 | `	}` |
|       - |  309 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 3066031 |  310 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 3066031 |  311 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  312 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  313 | `		return rc;` |
|       - |  314 | `	}` |
|       - |  315 | `	/* Perform the insertion */` |
| 3066031 |  316 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  317 | `	/* Install in the reference table */` |
| 3066031 |  318 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  319 | `	/* All done */` |
| 3066031 |  320 | `	return SXRET_OK;` |
| 1533018 |  321 | `}` |
|       - |  322 | `/*` |
|       - |  323 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  324 | ` * hashmap.` |
|       - |  325 | ` */` |
|  143084 |  326 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  327 | `{` |
|       - |  328 | `	ph7_hashmap_node *pNode;` |
|       - |  329 | `	sxu32 nHash;` |
|       - |  330 | `	sxu32 nIdx;` |
|       - |  331 | `	sxi32 rc;` |
|  143089 |  332 | `	if( !isForeign ){` |
|       - |  333 | `		ph7_value *pObj;` |
|       - |  334 | `		ph7_value sSafeVal;` |
|       - |  335 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  336 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  337 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  338 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  339 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  340 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   97203 |  341 | `		if( pValue ){` |
|   96913 |  342 | `			sSafeVal = *pValue;` |
|   96913 |  343 | `			pValue = &sSafeVal;` |
|   48454 |  344 | `		}` |
|       - |  345 | `		/* Reserve a ph7_value for the value */` |
|   97203 |  346 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   97203 |  347 | `		if( pObj == 0 ){` |
|     ! 0 |  348 | `			return SXERR_MEM;` |
|       - |  349 | `		}` |
|   97203 |  350 | `		if( pValue ){` |
|       - |  351 | `			/* Duplicate the value */` |
|   96913 |  352 | `			PH7_MemObjStore(pValue,pObj);` |
|   48454 |  353 | `		}` |
|   97203 |  354 | `		nIdx = pObj->nIdx;` |
|   48604 |  355 | `	}else{` |
|   45891 |  356 | `		nIdx = nRefIdx;` |
|       - |  357 | `	}` |
|       - |  358 | `	/* Hash the key */` |
|  143089 |  359 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  360 | `	/* Allocate a new blob node */` |
|  143089 |  361 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  143089 |  362 | `	if( pNode == 0 ){` |
|     ! 0 |  363 | `		return SXERR_MEM;` |
|       - |  364 | `	}` |
|  143089 |  365 | `	if( isForeign ){` |
|       - |  366 | `		/* Mark as a foregin entry */` |
|   45891 |  367 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   22943 |  368 | `	}` |
|       - |  369 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  143089 |  370 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  143089 |  371 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  372 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  373 | `		return rc;` |
|       - |  374 | `	}` |
|       - |  375 | `	/* Perform the insertion */` |
|  143089 |  376 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  377 | `	/* Install in the reference table */` |
|  143089 |  378 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  379 | `	/* All done */` |
|  143089 |  380 | `	return SXRET_OK;` |
|   71547 |  381 | `}` |
|       - |  382 | `/*` |
|       - |  383 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  384 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  385 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  386 | ` */` |
|   48542 |  387 | `static sxi32 HashmapLookupIntKey(` |
|       - |  388 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  389 | `	sxi64 iKey,                /* lookup key */` |
|       - |  390 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  391 | `	)` |
|       5 |  392 | `{` |
|       - |  393 | `	ph7_hashmap_node *pNode;` |
|       - |  394 | `	sxu32 nHash;` |
|   48547 |  395 | `	if( pMap->nEntry < 1 ){` |
|       - |  396 | `		/* Don't bother hashing,there is no entry anyway */` |
|     555 |  397 | `		return SXERR_NOTFOUND;` |
|       - |  398 | `	}` |
|       - |  399 | `	/* Hash the key first */` |
|   47997 |  400 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  401 | `	/* Point to the appropriate bucket */` |
|   47997 |  402 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  403 | `	/* Perform the lookup */` |
|  412353 |  404 | `	for(;;){` |
|  824711 |  405 | `		if( pNode == 0 ){` |
|   46305 |  406 | `			break;` |
|       - |  407 | `		}` |
|  779252 |  408 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  775390 |  409 | `			&& pNode->nHash == nHash` |
|  387038 |  410 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  411 | `				/* Node found */` |
|    1697 |  412 | `				if( ppNode ){` |
|    1679 |  413 | `					*ppNode = pNode;` |
|     837 |  414 | `				}` |
|    1697 |  415 | `				return SXRET_OK;` |
|       - |  416 | `		}` |
|       - |  417 | `		/* Follow the collision link */` |
|  776715 |  418 | `		pNode = pNode->pNextCollide;` |
|       1 |  419 | `	}` |
|       - |  420 | `	/* No such entry */` |
|   46305 |  421 | `	return SXERR_NOTFOUND;` |
|   24276 |  422 | `}` |
|       - |  423 | `/*` |
|       - |  424 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  425 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  426 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  427 | ` */` |
|  267310 |  428 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  429 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  430 | `	const void *pKey,           /* Lookup key */` |
|       - |  431 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  432 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  433 | `	)` |
|       5 |  434 | `{` |
|       - |  435 | `	ph7_hashmap_node *pNode;` |
|       - |  436 | `	sxu32 nHash;` |
|  267315 |  437 | `	if( pMap->nEntry < 1 ){` |
|       - |  438 | `		/* Don't bother hashing,there is no entry anyway */` |
|   16119 |  439 | `		return SXERR_NOTFOUND;` |
|       - |  440 | `	}` |
|       - |  441 | `	/* Hash the key first */` |
|  251201 |  442 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  443 | `	/* Point to the appropriate bucket */` |
|  251201 |  444 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  445 | `	/* Perform the lookup */` |
|  215139 |  446 | `	for(;;){` |
|  430283 |  447 | `		if( pNode == 0 ){` |
|  197905 |  448 | `			break;` |
|       - |  449 | `		}` |
|  259072 |  450 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  230875 |  451 | `			&& pNode->nHash == nHash` |
|  141380 |  452 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   53393 |  453 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  454 | `				/* Node found */` |
|   53301 |  455 | `				if( ppNode ){` |
|   53273 |  456 | `					*ppNode = pNode;` |
|   26634 |  457 | `				}` |
|   53301 |  458 | `				return SXRET_OK;` |
|       - |  459 | `		}` |
|       - |  460 | `		/* Follow the collision link */` |
|  179087 |  461 | `		pNode = pNode->pNextCollide;` |
|       5 |  462 | `	}` |
|       - |  463 | `	/* No such entry */` |
|  197905 |  464 | `	return SXERR_NOTFOUND;` |
|  133660 |  465 | `}` |
|       - |  466 | `/*` |
|       - |  467 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  468 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  469 | ` */` |
|  267448 |  470 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       5 |  471 | `{` |
|  267453 |  472 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  267453 |  473 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  267453 |  474 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  475 | `		/* Octal not decimal number */` |
|       5 |  476 | `		return FALSE;` |
|       - |  477 | `	}` |
|  267449 |  478 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  479 | `		zIn++;` |
|     ! 0 |  480 | `	}` |
|  134055 |  481 | `	for(;;){` |
|  268115 |  482 | `		if( zIn >= zEnd ){` |
|     233 |  483 | `			return TRUE;` |
|       - |  484 | `		}` |
|  267883 |  485 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  133611 |  486 | `			break;` |
|       - |  487 | `		}` |
|     667 |  488 | `		zIn++;` |
|       1 |  489 | `	}` |
|       - |  490 | `	/* Key does not look like a decimal number */` |
|  267217 |  491 | `	return FALSE;` |
|  133729 |  492 | `}` |
|       - |  493 | `/*` |
|       - |  494 | ` * Check if a given key exists in the given hashmap.` |
|       - |  495 | ` * Write a pointer to the target node on success.` |
|       - |  496 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  497 | ` */` |
|  125694 |  498 | `static sxi32 HashmapLookup(` |
|       - |  499 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  500 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  501 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  502 | `	)` |
|       5 |  503 | `{` |
|  125699 |  504 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  505 | `	sxi32 rc;` |
|  125699 |  506 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  124165 |  507 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  508 | `			/* Force a string cast */` |
|     ! 0 |  509 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  510 | `		}` |
|  124165 |  511 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  512 | `			/* Perform a blob lookup */` |
|  124149 |  513 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  124149 |  514 | `			goto result;` |
|       - |  515 | `		}` |
|       8 |  516 | `	}` |
|       - |  517 | `	/* Perform an int lookup */` |
|    1555 |  518 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  519 | `		/* Force an integer cast */` |
|      27 |  520 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  521 | `	}` |
|       - |  522 | `	/* Perform an int lookup */` |
|    1555 |  523 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   62847 |  524 | `result:` |
|  125699 |  525 | `	if( rc == SXRET_OK ){` |
|       - |  526 | `		/* Node found */` |
|   54699 |  527 | `		if( ppNode ){` |
|   54655 |  528 | `			*ppNode = pNode;` |
|   27325 |  529 | `		}` |
|   54699 |  530 | `		return SXRET_OK;` |
|       - |  531 | `	}` |
|       - |  532 | `	/* No such entry */` |
|   71005 |  533 | `	return SXERR_NOTFOUND;` |
|   62852 |  534 | `}` |
|       - |  535 | `/*` |
|       - |  536 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|       - |  537 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|       - |  538 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|       - |  539 | ` * via HashmapAppendIndexBusy.` |
|       - |  540 | ` */` |
|   23530 |  541 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|       5 |  542 | `{` |
|   23535 |  543 | `	if( iKey >= pMap->iNextIdx ){` |
|   23291 |  544 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       - |  545 | `		/* Make sure the automatic index is not reserved */` |
|   23291 |  546 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  547 | `			pMap->iNextIdx++;` |
|     ! 0 |  548 | `		}` |
|   11643 |  549 | `	}` |
|   23535 |  550 | `}` |
|       - |  551 | `/*` |
|       - |  552 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|       - |  553 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|       - |  554 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|       - |  555 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|       - |  556 | ` */` |
| 3042164 |  557 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|       5 |  558 | `{` |
| 3042169 |  559 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       7 |  560 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|       7 |  561 | `		return TRUE;` |
|       - |  562 | `	}` |
| 3042163 |  563 | `	return FALSE;` |
| 1521087 |  564 | `}` |
|       - |  565 | `/*` |
|       - |  566 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  567 | ` * hashmap.` |
|       - |  568 | ` * If a node with the given key already exists in the database` |
|       - |  569 | ` * then this function overwrite the old value.` |
|       - |  570 | ` */` |
| 3162928 |  571 | `static sxi32 HashmapInsert(` |
|       - |  572 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  573 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  574 | `	ph7_value *pVal    /* Node value */` |
|       - |  575 | `	)` |
|       5 |  576 | `{` |
| 3162933 |  577 | `	ph7_hashmap_node *pNode = 0;` |
| 3162933 |  578 | `	sxi32 rc = SXRET_OK;` |
| 3162933 |  579 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  100869 |  580 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  581 | `			/* Force a string cast */` |
|       3 |  582 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  583 | `		}` |
|  100869 |  584 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    3689 |  585 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  586 | `				/* Automatic index assign */` |
|    3467 |  587 | `				pKey = 0;` |
|    1731 |  588 | `			}` |
|    3689 |  589 | `			goto IntKey;` |
|       - |  590 | `		}` |
|  145775 |  591 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   48590 |  592 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  593 | `				/* Overwrite the old value */` |
|       - |  594 | `				ph7_value *pElem;` |
|      81 |  595 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      81 |  596 | `				if( pElem ){` |
|      81 |  597 | `					if( pVal ){` |
|      81 |  598 | `						PH7_MemObjStore(pVal,pElem);` |
|      42 |  599 | `					}else{` |
|       - |  600 | `						/* Nullify the entry */` |
|     ! 0 |  601 | `						PH7_MemObjToNull(pElem);` |
|       - |  602 | `					}` |
|      39 |  603 | `				}` |
|      81 |  604 | `				return SXRET_OK;` |
|       - |  605 | `		}` |
|   97107 |  606 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  607 | `			/* Forbidden */` |
|       3 |  608 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  609 | `			return SXRET_OK;` |
|       - |  610 | `		}` |
|       - |  611 | `		/* Perform a blob-key insertion */` |
|   97105 |  612 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   97105 |  613 | `		return rc;` |
|       - |  614 | `	}` |
| 1531032 |  615 | `IntKey:` |
| 3065753 |  616 | `	if( pKey ){` |
|   23619 |  617 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  618 | `			/* Force an integer cast */` |
|     251 |  619 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  620 | `		}` |
|   23619 |  621 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  622 | `			/* Overwrite the old value */` |
|       - |  623 | `			ph7_value *pElem;` |
|      87 |  624 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      87 |  625 | `			if( pElem ){` |
|      87 |  626 | `				if( pVal ){` |
|      87 |  627 | `					PH7_MemObjStore(pVal,pElem);` |
|      44 |  628 | `				}else{` |
|       - |  629 | `					/* Nullify the entry */` |
|     ! 0 |  630 | `					PH7_MemObjToNull(pElem);` |
|       - |  631 | `				}` |
|      43 |  632 | `			}` |
|      87 |  633 | `			return SXRET_OK;` |
|       - |  634 | `		}` |
|   23533 |  635 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  636 | `			/* Forbidden */` |
|       3 |  637 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  638 | `			return SXRET_OK;` |
|       - |  639 | `		}` |
|       - |  640 | `		/* Perform a 64-bit-int-key insertion */` |
|   23531 |  641 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23531 |  642 | `		if( rc == SXRET_OK ){` |
|   23531 |  643 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   11763 |  644 | `		}` |
|   11768 |  645 | `	}else{` |
| 3042139 |  646 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  647 | `			/* Forbidden */` |
|       3 |  648 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  649 | `			return SXRET_OK;` |
|       - |  650 | `		}` |
| 3042137 |  651 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       7 |  652 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|       - |  653 | `		}` |
|       - |  654 | `		/* Assign an automatic index */` |
| 3042131 |  655 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 3042131 |  656 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
| 3042129 |  657 | `			++pMap->iNextIdx;` |
| 1521062 |  658 | `		}` |
|       - |  659 | `	}` |
|       - |  660 | `	/* Insertion result */` |
| 3065657 |  661 | `	return rc;` |
| 1581469 |  662 | `}` |
|       - |  663 | `/*` |
|       - |  664 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - |  665 | ` * hashmap.` |
|       - |  666 | ` * This is insertion by reference so be careful to mark the node` |
|       - |  667 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - |  668 | ` * The insertion by reference is triggered when the following` |
|       - |  669 | ` * expression is encountered.` |
|       - |  670 | ` * $var = 10;` |
|       - |  671 | ` *  $a = array(&var);` |
|       - |  672 | ` * OR` |
|       - |  673 | ` *  $a[] =& $var;` |
|       - |  674 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - |  675 | ` * over it's contents.` |
|       - |  676 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - |  677 | ` * removed when the foreign ph7_value is unset.` |
|       - |  678 | ` * Example:` |
|       - |  679 | ` *  $var = 10;` |
|       - |  680 | ` *  $a[] =& $var;` |
|       - |  681 | ` *  echo count($a).PHP_EOL; //1` |
|       - |  682 | ` *  //Unset the foreign ph7_value now` |
|       - |  683 | ` *  unset($var);` |
|       - |  684 | ` *  echo count($a); //0` |
|       - |  685 | ` * Note that this is a PH7 eXtension.` |
|       - |  686 | ` * Refer to the official documentation for more information.` |
|       - |  687 | ` * If a node with the given key already exists in the database` |
|       - |  688 | ` * then this function overwrite the old value.` |
|       - |  689 | ` */` |
|   45928 |  690 | `static sxi32 HashmapInsertByRef(` |
|       - |  691 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  692 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  693 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  694 | `	)` |
|       5 |  695 | `{` |
|   45933 |  696 | `	ph7_hashmap_node *pNode = 0;` |
|   45933 |  697 | `	sxi32 rc = SXRET_OK;` |
|   45933 |  698 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   45897 |  699 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  700 | `			/* Force a string cast */` |
|     ! 0 |  701 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  702 | `		}` |
|   45897 |  703 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  704 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  705 | `				/* Automatic index assign */` |
|     ! 0 |  706 | `				pKey = 0;` |
|     ! 0 |  707 | `			}` |
|     ! 0 |  708 | `			goto IntKey;` |
|       - |  709 | `		}` |
|   68843 |  710 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   22946 |  711 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  712 | `				/* Overwrite */` |
|       7 |  713 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  714 | `				pNode->nValIdx = nRefIdx;` |
|       - |  715 | `				/* Install in the reference table */` |
|       7 |  716 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  717 | `				return SXRET_OK;` |
|       - |  718 | `		}` |
|       - |  719 | `		/* Perform a blob-key insertion */` |
|   45891 |  720 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   45891 |  721 | `		return rc;` |
|       - |  722 | `	}` |
|      18 |  723 | `IntKey:` |
|      37 |  724 | `	if( pKey ){` |
|       5 |  725 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  726 | `			/* Force an integer cast */` |
|     ! 0 |  727 | `			PH7_MemObjToInteger(pKey);` |
|     ! 0 |  728 | `		}` |
|       5 |  729 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  730 | `			/* Overwrite */` |
|     ! 0 |  731 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|     ! 0 |  732 | `			pNode->nValIdx = nRefIdx;` |
|       - |  733 | `			/* Install in the reference table */` |
|     ! 0 |  734 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|     ! 0 |  735 | `			return SXRET_OK;` |
|       - |  736 | `		}` |
|       - |  737 | `		/* Perform a 64-bit-int-key insertion */` |
|       5 |  738 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|       5 |  739 | `		if( rc == SXRET_OK ){` |
|       5 |  740 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|       2 |  741 | `		}` |
|       3 |  742 | `	}else{` |
|      33 |  743 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|     ! 0 |  744 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|       - |  745 | `		}` |
|       - |  746 | `		/* Assign an automatic index */` |
|      33 |  747 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|      33 |  748 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|      33 |  749 | `			++pMap->iNextIdx;` |
|      16 |  750 | `		}` |
|       - |  751 | `	}` |
|       - |  752 | `	/* Insertion result */` |
|      37 |  753 | `	return rc;` |
|   22969 |  754 | `}` |
|       - |  755 | `/*` |
|       - |  756 | ` * Extract node value.` |
|       - |  757 | ` */` |
| 1333306 |  758 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       5 |  759 | `{` |
|       - |  760 | `	/* Point to the desired object */` |
|       - |  761 | `	ph7_value *pObj;` |
| 1333311 |  762 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1333311 |  763 | `	return pObj;` |
|       5 |  764 | `}` |
|       - |  765 | `/*` |
|       - |  766 | ` * Insert a node in the given hashmap.` |
|       - |  767 | ` * If a node with the given key already exists in the database` |
|       - |  768 | ` * then this function overwrite the old value.` |
|       - |  769 | ` */` |
|     446 |  770 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       5 |  771 | `{` |
|       - |  772 | `	ph7_value *pObj;` |
|       - |  773 | `	sxi32 rc;` |
|       - |  774 | `	/* Extract the node value */` |
|     451 |  775 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     451 |  776 | `	if( pObj == 0 ){` |
|     ! 0 |  777 | `		return SXERR_EMPTY;` |
|       - |  778 | `	}` |
|       - |  779 | `	/* Preserve key */` |
|     451 |  780 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  781 | `		/* Int64 key */` |
|     321 |  782 | `		if( !bPreserve ){` |
|       - |  783 | `			/* Assign an automatic index */` |
|     173 |  784 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      89 |  785 | `		}else{` |
|     149 |  786 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  787 | `		}` |
|     163 |  788 | `	}else{` |
|       - |  789 | `		/* Blob key */` |
|     131 |  790 | `		if( !bPreserve ){` |
|       - |  791 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  792 | `			 * original string key entirely */` |
|      35 |  793 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      18 |  794 | `		}else{` |
|     145 |  795 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      48 |  796 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  797 | `		}` |
|       - |  798 | `	}` |
|     451 |  799 | `	return rc;` |
|     228 |  800 | `}` |
|       - |  801 | `/*` |
|       - |  802 | ` * Compare two node values.` |
|       - |  803 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  804 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  805 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  806 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  807 | ` * documenation.` |
|       - |  808 | ` */` |
|   68351 |  809 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       5 |  810 | `{` |
|       - |  811 | `	ph7_value sObj1,sObj2;` |
|       - |  812 | `	sxi32 rc;` |
|   68356 |  813 | `	if( pLeft == pRight ){` |
|       - |  814 | `		/*` |
|       - |  815 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  816 | `		 * below for more information on this sceanario.` |
|       - |  817 | `		 */` |
|     ! 0 |  818 | `		return 0;` |
|       - |  819 | `	}` |
|       - |  820 | `	/* Do the comparison */` |
|   68356 |  821 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   68356 |  822 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   68356 |  823 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   68356 |  824 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   68356 |  825 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   68356 |  826 | `	PH7_MemObjRelease(&sObj1);` |
|   68356 |  827 | `	PH7_MemObjRelease(&sObj2);` |
|   68356 |  828 | `	return rc;` |
|   34206 |  829 | `}` |
|       - |  830 | `/*` |
|       - |  831 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  832 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  833 | ` */` |
|   13086 |  834 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       5 |  835 | `{` |
|   13091 |  836 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  837 | `	sxu32 nBucket;` |
|       - |  838 | `	/* Remove old collision links */` |
|   13091 |  839 | `	if( pEntry->pPrevCollide ){` |
|   10714 |  840 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    5360 |  841 | `	}else{` |
|    2382 |  842 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  843 | `	}` |
|   13091 |  844 | `	if( pEntry->pNextCollide ){` |
|    1056 |  845 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     532 |  846 | `	}` |
|   13091 |  847 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  848 | `	/* Compute the new hash */` |
|   13091 |  849 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   13091 |  850 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   13091 |  851 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  852 | `	/* Link to the new bucket */` |
|   13091 |  853 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   13091 |  854 | `	if( pMap->apBucket[nBucket] ){` |
|   11031 |  855 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    5511 |  856 | `	}` |
|   13091 |  857 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   13091 |  858 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  859 | `	/* Increment the automatic index (saturating, like every other advance —` |
|       - |  860 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|       - |  861 | `	 * the no-overflow invariant uniform). */` |
|   13091 |  862 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|   13091 |  863 | `		pMap->iNextIdx++;` |
|    6543 |  864 | `	}` |
|   13091 |  865 | `}` |
|       - |  866 | `/*` |
|       - |  867 | ` * Perform a linear search on a given hashmap.` |
|       - |  868 | ` * Write a pointer to the target node on success.` |
|       - |  869 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  870 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  871 | ` * for more information.` |
|       - |  872 | ` */` |
|   32560 |  873 | `static int HashmapFindValue(` |
|       - |  874 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  875 | `	ph7_value *pNeedle,  /* Lookup key */` |
|       - |  876 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|       - |  877 | `	int bStrict      /* TRUE for strict comparison */` |
|       - |  878 | `	)` |
|       5 |  879 | `{` |
|       - |  880 | `	ph7_hashmap_node *pEntry;` |
|       - |  881 | `	ph7_value sVal,*pVal;` |
|       - |  882 | `	ph7_value sNeedle;` |
|       - |  883 | `	sxi32 rc;` |
|       - |  884 | `	sxu32 n;` |
|       - |  885 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|   32565 |  886 | `	pEntry = pMap->pFirst;` |
|   32565 |  887 | `	n = pMap->nEntry;` |
|   32565 |  888 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   32565 |  889 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   77832 |  890 | `	for(;;){` |
|  155669 |  891 | `		if( n < 1 ){` |
|      99 |  892 | `			break;` |
|       - |  893 | `		}` |
|       - |  894 | `		/* Extract node value */` |
|  155571 |  895 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  155571 |  896 | `		if( pVal ){` |
|  155571 |  897 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|     ! 0 |  898 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  899 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  900 | `				if( iF1 == iF2 ){` |
|       - |  901 | `					/* NULL values are equals */` |
|     ! 0 |  902 | `					if( ppNode ){` |
|     ! 0 |  903 | `						*ppNode = pEntry;` |
|     ! 0 |  904 | `					}` |
|     ! 0 |  905 | `					return SXRET_OK;` |
|       - |  906 | `				}` |
|     ! 0 |  907 | `			}else{` |
|       - |  908 | `				/* Duplicate value */` |
|  155571 |  909 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  155571 |  910 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  155571 |  911 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  155571 |  912 | `				PH7_MemObjRelease(&sVal);` |
|  155571 |  913 | `				PH7_MemObjRelease(&sNeedle);` |
|  155571 |  914 | `				if( rc == 0 ){` |
|   32467 |  915 | `					if( ppNode ){` |
|      23 |  916 | `						*ppNode = pEntry;` |
|      11 |  917 | `					}` |
|       - |  918 | `					/* Match found*/` |
|   32467 |  919 | `					return SXRET_OK;` |
|       - |  920 | `				}` |
|       - |  921 | `			}` |
|   61552 |  922 | `		}` |
|       - |  923 | `		/* Point to the next entry */` |
|  123109 |  924 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  123109 |  925 | `		n--;` |
|       5 |  926 | `	}` |
|       - |  927 | `	/* No such entry */` |
|      99 |  928 | `	return SXERR_NOTFOUND;` |
|   16285 |  929 | `}` |
|       - |  930 | `/*` |
|       - |  931 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  932 | ` * for values comparison.` |
|       - |  933 | ` * Write a pointer to the target node on success.` |
|       - |  934 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  935 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  936 | ` * for more information.` |
|       - |  937 | ` */` |
|      22 |  938 | `static int HashmapFindValueByCallback(` |
|       - |  939 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|       - |  940 | `	ph7_value *pNeedle,    /* Lookup key */` |
|       - |  941 | `	ph7_value *pCallback,  /* User defined callback */` |
|       - |  942 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|       - |  943 | `	)` |
|       1 |  944 | `{` |
|       - |  945 | `	ph7_hashmap_node *pEntry;` |
|       - |  946 | `	ph7_value sResult,*pVal;` |
|       - |  947 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|       - |  948 | `	sxi32 rc;` |
|       - |  949 | `	sxu32 n;` |
|      23 |  950 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|       - |  951 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|       - |  952 | `		 * exception is not thrown again, and let the caller wind down. */` |
|     ! 0 |  953 | `		return SXERR_NOTFOUND;` |
|       - |  954 | `	}` |
|       - |  955 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|      23 |  956 | `	pEntry = pMap->pFirst;` |
|      23 |  957 | `	n = pMap->nEntry;` |
|       - |  958 | `	/* Store callback result here */` |
|      23 |  959 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  960 | `	/* First argument to the callback */` |
|      23 |  961 | `	apArg[0] = pNeedle;` |
|      25 |  962 | `	for(;;){` |
|      51 |  963 | `		if( n < 1 ){` |
|       9 |  964 | `			break;` |
|       - |  965 | `		}` |
|       - |  966 | `		/* Extract node value */` |
|      43 |  967 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      43 |  968 | `		if( pVal ){` |
|       - |  969 | `			/* Invoke the user callback */` |
|      43 |  970 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      43 |  971 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      43 |  972 | `			if( rc == PH7_EXCEPTION ){` |
|       - |  973 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|       - |  974 | `				 * and report no match for the rest of the run. */` |
|       5 |  975 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|       5 |  976 | `				PH7_MemObjRelease(&sResult);` |
|       5 |  977 | `				return SXERR_NOTFOUND;` |
|       - |  978 | `			}` |
|      39 |  979 | `			if( rc == SXRET_OK ){` |
|       - |  980 | `				/* Extract callback result */` |
|      39 |  981 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  982 | `					/* Perform an int cast */` |
|     ! 0 |  983 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  984 | `				}` |
|      39 |  985 | `				rc = (sxi32)sResult.x.iVal;` |
|      39 |  986 | `				PH7_MemObjRelease(&sResult);` |
|      39 |  987 | `				if( rc == 0 ){` |
|       - |  988 | `					/* Match found*/` |
|      11 |  989 | `					if( ppNode ){` |
|     ! 0 |  990 | `						*ppNode = pEntry;` |
|     ! 0 |  991 | `					}` |
|      11 |  992 | `					return SXRET_OK;` |
|       - |  993 | `				}` |
|      14 |  994 | `			}` |
|      14 |  995 | `		}` |
|       - |  996 | `		/* Point to the next entry */` |
|      29 |  997 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 |  998 | `		n--;` |
|       1 |  999 | `	}` |
|       - | 1000 | `	/* No such entry */` |
|       9 | 1001 | `	return SXERR_NOTFOUND;` |
|      12 | 1002 | `}` |
|       - | 1003 | `/*` |
|       - | 1004 | ` * Compare two hashmaps.` |
|       - | 1005 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|       - | 1006 | ` * Note on array comparison operators.` |
|       - | 1007 | ` *  According to the PHP language reference manual.` |
|       - | 1008 | ` *  Array Operators Example 	Name 	Result` |
|       - | 1009 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|       - | 1010 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|       - | 1011 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|       - | 1012 | ` *                          order and of the same types.` |
|       - | 1013 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - | 1014 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - | 1015 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|       - | 1016 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1017 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1018 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1019 | ` * <?php` |
|       - | 1020 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1021 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1022 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1023 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1024 | ` * var_dump($c);` |
|       - | 1025 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1026 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1027 | ` * var_dump($c);` |
|       - | 1028 | ` * ?>` |
|       - | 1029 | ` * When executed, this script will print the following:` |
|       - | 1030 | ` * Union of $a and $b:` |
|       - | 1031 | ` * array(3) {` |
|       - | 1032 | ` *  ["a"]=>` |
|       - | 1033 | ` *  string(5) "apple"` |
|       - | 1034 | ` *  ["b"]=>` |
|       - | 1035 | ` * string(6) "banana"` |
|       - | 1036 | ` *  ["c"]=>` |
|       - | 1037 | ` * string(6) "cherry"` |
|       - | 1038 | ` * }` |
|       - | 1039 | ` * Union of $b and $a:` |
|       - | 1040 | ` * array(3) {` |
|       - | 1041 | ` * ["a"]=>` |
|       - | 1042 | ` * string(4) "pear"` |
|       - | 1043 | ` * ["b"]=>` |
|       - | 1044 | ` * string(10) "strawberry"` |
|       - | 1045 | ` * ["c"]=>` |
|       - | 1046 | ` * string(6) "cherry"` |
|       - | 1047 | ` * }` |
|       - | 1048 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|       - | 1049 | ` */` |
|      26 | 1050 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - | 1051 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - | 1052 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - | 1053 | `	int bStrict          /* TRUE for strict comparison */` |
|       - | 1054 | `	)` |
|       1 | 1055 | `{` |
|       - | 1056 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - | 1057 | `	sxi32 rc;` |
|       - | 1058 | `	sxu32 n;` |
|      27 | 1059 | `	if( pLeft == pRight ){` |
|       - | 1060 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - | 1061 | `		 * Unlike the zend engine.` |
|       - | 1062 | `		 */` |
|     ! 0 | 1063 | `		return 0;` |
|       - | 1064 | `	}` |
|      27 | 1065 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - | 1066 | `		/* Must have the same number of entries */` |
|       5 | 1067 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1068 | `	}` |
|       - | 1069 | `	/* Point to the first inserted entry of the left hashmap */` |
|      23 | 1070 | `	pLe = pLeft->pFirst;` |
|      23 | 1071 | `	pRe = 0; /* cc warning */` |
|       - | 1072 | `	/* Perform the comparison */` |
|      23 | 1073 | `	n = pLeft->nEntry;` |
|      27 | 1074 | `	for(;;){` |
|      55 | 1075 | `		if( n < 1 ){` |
|      21 | 1076 | `			break;` |
|       - | 1077 | `		}` |
|      35 | 1078 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1079 | `			/* Int key */` |
|      27 | 1080 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|      14 | 1081 | `		}else{` |
|       9 | 1082 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1083 | `			/* Blob key */` |
|       9 | 1084 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1085 | `		}` |
|      35 | 1086 | `		if( rc != SXRET_OK ){` |
|       - | 1087 | `			/* No such entry in the right side */` |
|     ! 0 | 1088 | `			return 1;` |
|       - | 1089 | `		}` |
|      35 | 1090 | `		rc = 0;` |
|      35 | 1091 | `		if( bStrict ){` |
|       - | 1092 | `			/* Make sure,the keys are of the same type */` |
|      19 | 1093 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1094 | `				rc = 1;` |
|     ! 0 | 1095 | `			}` |
|       9 | 1096 | `		}` |
|      35 | 1097 | `		if( !rc ){` |
|       - | 1098 | `			/* Compare nodes */` |
|      35 | 1099 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|      17 | 1100 | `		}` |
|      35 | 1101 | `		if( rc != 0 ){` |
|       - | 1102 | `			/* Nodes key/value differ */` |
|       3 | 1103 | `			return rc;` |
|       - | 1104 | `		}` |
|       - | 1105 | `		/* Point to the next entry */` |
|      33 | 1106 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      33 | 1107 | `		n--;` |
|       1 | 1108 | `	}` |
|      21 | 1109 | `	return 0; /* Hashmaps are equals */` |
|      14 | 1110 | `}` |
|       - | 1111 | `/*` |
|       - | 1112 | ` * Duplicate a hashmap node.` |
|       - | 1113 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1114 | ` */` |
|  613632 | 1115 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1116 | `	ph7_hashmap *pDest,` |
|       - | 1117 | `	ph7_hashmap_node *pEntry,` |
|       - | 1118 | `	ph7_value *pVal,` |
|       - | 1119 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1120 | `	)` |
|       5 | 1121 | `{` |
|       - | 1122 | `	ph7_value sSafeVal;` |
|       - | 1123 | `	ph7_value sKey;` |
|       - | 1124 | `	sxi32 rc;` |
|       - | 1125 |  |
|  613637 | 1126 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 1127 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|       - | 1128 | `		 * Re-insert it by reference so the reference survives the duplication` |
|       - | 1129 | `		 * instead of being flattened to a value copy. This keeps spread` |
|       - | 1130 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|       - | 1131 | `		 * with PHP semantics. */` |
|       7 | 1132 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|       7 | 1133 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       3 | 1134 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|       3 | 1135 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|       3 | 1136 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       3 | 1137 | `			PH7_MemObjRelease(&sKey);` |
|       2 | 1138 | `		}else{` |
|       5 | 1139 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|       5 | 1140 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|       2 | 1141 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|     ! 0 | 1142 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|     ! 0 | 1143 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|     ! 0 | 1144 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 1145 | `			}else{ /* Dup: preserve the int key */` |
|     ! 0 | 1146 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|       - | 1147 | `			}` |
|       - | 1148 | `		}` |
|       7 | 1149 | `		return rc;` |
|       - | 1150 | `	}` |
|  613631 | 1151 | `	sSafeVal = *pVal;` |
|       - | 1152 |  |
|  613631 | 1153 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1154 | `		/* Blob key insertion */` |
|     101 | 1155 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|     101 | 1156 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|     101 | 1157 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|     101 | 1158 | `		PH7_MemObjRelease(&sKey);` |
|      51 | 1159 | `	}else{` |
|       - | 1160 | `		/* Int key */` |
|  613531 | 1161 | `		if( iAction == 0 ){ /* Merge */` |
|  613321 | 1162 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  306871 | 1163 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1164 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1165 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1166 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1167 | `		}else{ /* Dup */` |
|     182 | 1168 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1169 | `		}` |
|       - | 1170 | `	}` |
|  613631 | 1171 | `	return rc;` |
|  306821 | 1172 | `}` |
|       - | 1173 | `/*` |
|       - | 1174 | ` * Merge two hashmaps.` |
|       - | 1175 | ` * Note on the merge process` |
|       - | 1176 | ` * According to the PHP language reference manual.` |
|       - | 1177 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|       - | 1178 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|       - | 1179 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|       - | 1180 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|       - | 1181 | ` *  the later value will not overwrite the original value, but will be appended.` |
|       - | 1182 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|       - | 1183 | ` *  keys starting from zero in the result array.` |
|       - | 1184 | ` */` |
|    2104 | 1185 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       5 | 1186 | `{` |
|       - | 1187 | `	ph7_hashmap_node *pEntry;` |
|       - | 1188 | `	ph7_value *pVal;` |
|       - | 1189 | `	sxi32 rc;` |
|       - | 1190 | `	sxu32 n;` |
|    2109 | 1191 | `	if( pSrc == pDest ){` |
|       - | 1192 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1193 | `		 * Unlike the zend engine.` |
|       - | 1194 | `		 */` |
|     ! 0 | 1195 | `		return SXRET_OK;` |
|       - | 1196 | `	}` |
|       - | 1197 | `	/* Point to the first inserted entry in the source */` |
|    2109 | 1198 | `	pEntry = pSrc->pFirst;` |
|       - | 1199 | `	/* Perform the merge */` |
|  615483 | 1200 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1201 | `		/* Extract the node value */` |
|  613379 | 1202 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  613379 | 1203 | `		if( pVal ){` |
|       - | 1204 | `			/* Make a local copy of the value.` |
|       - | 1205 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1206 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1207 | `			 * to the old pool.` |
|       - | 1208 | `			 */` |
|  613379 | 1209 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  306692 | 1210 | `		}else{` |
|     ! 0 | 1211 | `			rc = SXRET_OK;` |
|       - | 1212 | `		}` |
|  613379 | 1213 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1214 | `			return rc;` |
|       - | 1215 | `		}` |
|       - | 1216 | `		/* Point to the next entry */` |
|  613379 | 1217 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  306692 | 1218 | `	}` |
|    2109 | 1219 | `	return SXRET_OK;` |
|    1057 | 1220 | `}` |
|       - | 1221 | `/*` |
|       - | 1222 | ` * Overwrite entries with the same key.` |
|       - | 1223 | ` * Refer to the [array_replace()] implementation for more information.` |
|       - | 1224 | ` *  According to the PHP language reference manual.` |
|       - | 1225 | ` *  array_replace() replaces the values of the first array with the same values` |
|       - | 1226 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|       - | 1227 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|       - | 1228 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|       - | 1229 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|       - | 1230 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|       - | 1231 | ` *  overwriting the previous values.` |
|       - | 1232 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|       - | 1233 | ` *  by whatever type is in the second array.` |
|       - | 1234 | ` */` |
|      34 | 1235 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1236 | `{` |
|       - | 1237 | `	ph7_hashmap_node *pEntry;` |
|       - | 1238 | `	ph7_value *pVal;` |
|       - | 1239 | `	sxi32 rc;` |
|       - | 1240 | `	sxu32 n;` |
|      36 | 1241 | `	if( pSrc == pDest ){` |
|       - | 1242 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1243 | `		 * Unlike the zend engine.` |
|       - | 1244 | `		 */` |
|     ! 0 | 1245 | `		return SXRET_OK;` |
|       - | 1246 | `	}` |
|       - | 1247 | `	/* Point to the first inserted entry in the source */` |
|      36 | 1248 | `	pEntry = pSrc->pFirst;` |
|       - | 1249 | `	/* Perform the merge */` |
|      80 | 1250 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1251 | `		/* Extract the node value */` |
|      46 | 1252 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      46 | 1253 | `		if( pVal ){` |
|      46 | 1254 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      24 | 1255 | `		}else{` |
|     ! 0 | 1256 | `			rc = SXRET_OK;` |
|       - | 1257 | `		}` |
|      46 | 1258 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1259 | `			return rc;` |
|       - | 1260 | `		}` |
|       - | 1261 | `		/* Point to the next entry */` |
|      46 | 1262 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 1263 | `	}` |
|      36 | 1264 | `	return SXRET_OK;` |
|      19 | 1265 | `}` |
|       - | 1266 | `/*` |
|       - | 1267 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|       - | 1268 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|       - | 1269 | ` */` |
|     110 | 1270 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1271 | `{` |
|       - | 1272 | `	ph7_hashmap_node *pEntry;` |
|       - | 1273 | `	ph7_value *pVal;` |
|       - | 1274 | `	sxi32 rc;` |
|       - | 1275 | `	sxu32 n;` |
|     112 | 1276 | `	if( pSrc == pDest ){` |
|       - | 1277 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1278 | `		 * Unlike the zend engine.` |
|       - | 1279 | `		 */` |
|     ! 0 | 1280 | `		return SXRET_OK;` |
|       - | 1281 | `	}` |
|       - | 1282 | `	/* Point to the first inserted entry in the source */` |
|     112 | 1283 | `	pEntry = pSrc->pFirst;` |
|       - | 1284 | `	/* Perform the duplication */` |
|     326 | 1285 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1286 | `		/* Extract the node value */` |
|     216 | 1287 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     216 | 1288 | `		if( pVal ){` |
|     216 | 1289 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|     109 | 1290 | `		}else{` |
|     ! 0 | 1291 | `			rc = SXRET_OK;` |
|       - | 1292 | `		}` |
|     216 | 1293 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1294 | `			return rc;` |
|       - | 1295 | `		}` |
|       - | 1296 | `		/* Point to the next entry */` |
|     216 | 1297 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     109 | 1298 | `	}` |
|     112 | 1299 | `	return SXRET_OK;` |
|      57 | 1300 | `}` |
|       - | 1301 | `/*` |
|       - | 1302 | ` * Copy-on-write separation for arrays.` |
|       - | 1303 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|       - | 1304 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|       - | 1305 | ` * Returns the (possibly new) hashmap pointer.` |
|       - | 1306 | ` */` |
|  214328 | 1307 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1308 | `{` |
|  214333 | 1309 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1310 | `	ph7_hashmap *pNew;` |
|       - | 1311 | `	ph7_value *pBacking;` |
|       - | 1312 | `	sxu32 nValIdx;` |
|       - | 1313 | `	int bValueInPool;` |
|  214333 | 1314 | `	if( pMap->iRef < 2 ){` |
|       - | 1315 | `		/* Sole owner, no separation needed */` |
|  212159 | 1316 | `		return pMap;` |
|       - | 1317 | `	}` |
|    2179 | 1318 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1319 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1320 | `		return pMap;` |
|       - | 1321 | `	}` |
|       - | 1322 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1323 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1324 | `	 * frame is popped. */` |
|    2179 | 1325 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2179 | 1326 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    3243 | 1327 | `		if( pBacking && pBacking != pValue` |
|    2156 | 1328 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2143 | 1329 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1330 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2143 | 1331 | `			pMap->iRef--;` |
|    2143 | 1332 | `			if( pMap->iRef < 2 ){` |
|       - | 1333 | `				/* After undoing stack ref, sole owner — no separation */` |
|    2103 | 1334 | `				pMap->iRef++;` |
|    2103 | 1335 | `				return pMap;` |
|       - | 1336 | `			}` |
|      42 | 1337 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|      42 | 1338 | `			if( pNew == 0 ){` |
|     ! 0 | 1339 | `				pMap->iRef++;` |
|     ! 0 | 1340 | `				return pMap;` |
|       - | 1341 | `			}` |
|      42 | 1342 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1343 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|     ! 0 | 1344 | `				PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1345 | `				pMap->iRef++;` |
|     ! 0 | 1346 | `				return pMap;` |
|       - | 1347 | `			}` |
|      42 | 1348 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|      42 | 1349 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|       - | 1350 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|       - | 1351 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|       - | 1352 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|       - | 1353 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|       - | 1354 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|       - | 1355 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|       - | 1356 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|      42 | 1357 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      42 | 1358 | `			if( pBacking ){` |
|      42 | 1359 | `				pBacking->x.pOther = pNew;` |
|      20 | 1360 | `			}` |
|       - | 1361 | `			/* Update the stack value to match */` |
|      42 | 1362 | `			pValue->x.pOther = pNew;` |
|      42 | 1363 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|      42 | 1364 | `			return pNew;` |
|       - | 1365 | `		}` |
|      18 | 1366 | `	}` |
|       - | 1367 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|       - | 1368 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|       - | 1369 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|       - | 1370 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|       - | 1371 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|       - | 1372 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|       - | 1373 | `	 * pBacking in the backing-variable branch above). */` |
|      37 | 1374 | `	nValIdx = pValue->nIdx;` |
|      55 | 1375 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|      36 | 1376 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|      37 | 1377 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      37 | 1378 | `	if( pNew == 0 ){` |
|       - | 1379 | `		/* Allocation failure — fall through with shared map */` |
|     ! 0 | 1380 | `		return pMap;` |
|       - | 1381 | `	}` |
|      37 | 1382 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1383 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|     ! 0 | 1384 | `		PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1385 | `		return pMap;` |
|       - | 1386 | `	}` |
|      37 | 1387 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      37 | 1388 | `	pMap->iRef--;` |
|      37 | 1389 | `	if( bValueInPool ){` |
|       - | 1390 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|      37 | 1391 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|      37 | 1392 | `		if( pValue == 0 ){` |
|     ! 0 | 1393 | `			return pNew;` |
|       - | 1394 | `		}` |
|      18 | 1395 | `	}` |
|      37 | 1396 | `	pValue->x.pOther = pNew;` |
|      37 | 1397 | `	return pNew;` |
|  107169 | 1398 | `}` |
|       - | 1399 | `/*` |
|       - | 1400 | ` * Perform the union of two hashmaps.` |
|       - | 1401 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1402 | ` * with a variable holding an array as follows:` |
|       - | 1403 | ` * <?php` |
|       - | 1404 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1405 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1406 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1407 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1408 | ` * var_dump($c);` |
|       - | 1409 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1410 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1411 | ` * var_dump($c);` |
|       - | 1412 | ` * ?>` |
|       - | 1413 | ` * When executed, this script will print the following:` |
|       - | 1414 | ` * Union of $a and $b:` |
|       - | 1415 | ` * array(3) {` |
|       - | 1416 | ` *  ["a"]=>` |
|       - | 1417 | ` *  string(5) "apple"` |
|       - | 1418 | ` *  ["b"]=>` |
|       - | 1419 | ` * string(6) "banana"` |
|       - | 1420 | ` *  ["c"]=>` |
|       - | 1421 | ` * string(6) "cherry"` |
|       - | 1422 | ` * }` |
|       - | 1423 | ` * Union of $b and $a:` |
|       - | 1424 | ` * array(3) {` |
|       - | 1425 | ` * ["a"]=>` |
|       - | 1426 | ` * string(4) "pear"` |
|       - | 1427 | ` * ["b"]=>` |
|       - | 1428 | ` * string(10) "strawberry"` |
|       - | 1429 | ` * ["c"]=>` |
|       - | 1430 | ` * string(6) "cherry"` |
|       - | 1431 | ` * }` |
|       - | 1432 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1433 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1434 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1435 | ` */` |
|      10 | 1436 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       2 | 1437 | `{` |
|       - | 1438 | `	ph7_hashmap_node *pEntry;` |
|      12 | 1439 | `	sxi32 rc = SXRET_OK;` |
|       - | 1440 | `	ph7_value *pObj;` |
|       - | 1441 | `	sxu32 n;` |
|      12 | 1442 | `	if( pLeft == pRight ){` |
|       - | 1443 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1444 | `		 * Unlike the zend engine.` |
|       - | 1445 | `		 */` |
|     ! 0 | 1446 | `		return SXRET_OK;` |
|       - | 1447 | `	}` |
|       - | 1448 | `	/* Perform the union */` |
|      12 | 1449 | `	pEntry = pRight->pFirst;` |
|      32 | 1450 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1451 | `		/* Make sure the given key does not exists in the left array */` |
|      22 | 1452 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1453 | `			/* BLOB key */` |
|       7 | 1454 | `			if( SXRET_OK !=` |
|       6 | 1455 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|       3 | 1456 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|       3 | 1457 | `					if( pObj ){` |
|       3 | 1458 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1459 | `						/* Perform the insertion */` |
|       3 | 1460 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1461 | `							&sSafeVal,0,FALSE);` |
|       3 | 1462 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1463 | `							return rc;` |
|       - | 1464 | `						}` |
|       1 | 1465 | `					}` |
|       1 | 1466 | `			}` |
|       4 | 1467 | `		}else{` |
|       - | 1468 | `			/* INT key */` |
|      16 | 1469 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|      11 | 1470 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|      11 | 1471 | `				if( pObj ){` |
|      11 | 1472 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1473 | `					/* Perform the insertion */` |
|      11 | 1474 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      11 | 1475 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1476 | `						return rc;` |
|       - | 1477 | `					}` |
|       5 | 1478 | `				}` |
|       5 | 1479 | `			}` |
|       - | 1480 | `		}` |
|       - | 1481 | `		/* Point to the next entry */` |
|      22 | 1482 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      12 | 1483 | `	}` |
|      12 | 1484 | `	return SXRET_OK;` |
|       7 | 1485 | `}` |
|       - | 1486 | `/*` |
|       - | 1487 | ` * Allocate a new hashmap.` |
|       - | 1488 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1489 | ` */` |
|  100328 | 1490 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1491 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1492 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1493 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1494 | `	)` |
|       5 | 1495 | `{` |
|       - | 1496 | `	ph7_hashmap *pMap;` |
|       - | 1497 | `	/* Allocate a new instance */` |
|  100333 | 1498 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|  100333 | 1499 | `	if( pMap == 0 ){` |
|     ! 0 | 1500 | `		return 0;` |
|       - | 1501 | `	}` |
|       - | 1502 | `	/* Zero the structure */` |
|  100333 | 1503 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1504 | `	/* Fill in the structure */` |
|  100333 | 1505 | `	pMap->pVm = &(*pVm);` |
|  100333 | 1506 | `	pMap->iRef = 1;` |
|       - | 1507 | `	/* Default hash functions */` |
|  100333 | 1508 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|  100333 | 1509 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|  100333 | 1510 | `	return pMap;` |
|   50169 | 1511 | `}` |
|       - | 1512 | `/*` |
|       - | 1513 | ` * Install superglobals in the given virtual machine.` |
|       - | 1514 | ` * Note on superglobals.` |
|       - | 1515 | ` *  According to the PHP language reference manual.` |
|       - | 1516 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1517 | `*   Description` |
|       - | 1518 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1519 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1520 | `*   global $variable; to access them within functions or methods.` |
|       - | 1521 | `*   These superglobal variables are:` |
|       - | 1522 | `*    $GLOBALS` |
|       - | 1523 | `*    $_SERVER` |
|       - | 1524 | `*    $_GET` |
|       - | 1525 | `*    $_POST` |
|       - | 1526 | `*    $_FILES` |
|       - | 1527 | `*    $_COOKIE` |
|       - | 1528 | `*    $_SESSION` |
|       - | 1529 | `*    $_REQUEST` |
|       - | 1530 | `*    $_ENV` |
|       - | 1531 | `*/` |
|    3452 | 1532 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       5 | 1533 | `{` |
|       - | 1534 | `	static const char * azSuper[] = {` |
|       - | 1535 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1536 | `		"_GET",      /* $_GET */` |
|       - | 1537 | `		"_POST",     /* $_POST */` |
|       - | 1538 | `		"_FILES",    /* $_FILES */` |
|       - | 1539 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1540 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1541 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1542 | `		"_ENV",      /* $_ENV */` |
|       - | 1543 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1544 | `		"argv"       /* $argv */` |
|       - | 1545 | `	};` |
|       - | 1546 | `	ph7_hashmap *pMap;` |
|       - | 1547 | `	ph7_value *pObj;` |
|       - | 1548 | `	SyString *pFile;` |
|       - | 1549 | `	sxi32 rc;` |
|       - | 1550 | `	sxu32 n;` |
|       - | 1551 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    3457 | 1552 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    3457 | 1553 | `	if( pMap == 0 ){` |
|     ! 0 | 1554 | `		return SXERR_MEM;` |
|       - | 1555 | `	}` |
|    3457 | 1556 | `	pVm->pGlobal = pMap;` |
|       - | 1557 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    3457 | 1558 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    3457 | 1559 | `	if( pObj == 0 ){` |
|     ! 0 | 1560 | `		return SXERR_MEM;` |
|       - | 1561 | `	}` |
|    3457 | 1562 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1563 | `	/* Record object index */` |
|    3457 | 1564 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1565 | `	/* Install the special $GLOBALS array */` |
|    3457 | 1566 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    3457 | 1567 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1568 | `		return rc;` |
|       - | 1569 | `	}` |
|       - | 1570 | `	/* Install superglobals now */` |
|   37977 | 1571 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1572 | `		ph7_value *pSuper;` |
|       - | 1573 | `		/* Request an empty array */` |
|   34525 | 1574 | `		pSuper = ph7_new_array(&(*pVm));` |
|   34525 | 1575 | `		if( pSuper == 0 ){` |
|     ! 0 | 1576 | `			return SXERR_MEM;` |
|       - | 1577 | `		}` |
|       - | 1578 | `		/* Install */` |
|   34525 | 1579 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   34525 | 1580 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1581 | `			return rc;` |
|       - | 1582 | `		}` |
|       - | 1583 | `		/* Release the value now it have been installed */` |
|   34525 | 1584 | `		ph7_release_value(&(*pVm),pSuper);` |
|   17265 | 1585 | `	}` |
|       - | 1586 | `	/* Set some $_SERVER entries */` |
|    3457 | 1587 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1588 | `	/*` |
|       - | 1589 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1590 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1591 | `	 */` |
|    6905 | 1592 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1593 | `		"SCRIPT_FILENAME",` |
|    1726 | 1594 | `		pFile ? pFile->zString : ":Memory:",` |
|    3448 | 1595 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1596 | `		);` |
|       - | 1597 | `	/* All done,all super-global are installed now */` |
|    3457 | 1598 | `	return SXRET_OK;` |
|    1731 | 1599 | `}` |
|       - | 1600 | `/*` |
|       - | 1601 | ` * Release a hashmap.` |
|       - | 1602 | ` */` |
|   61734 | 1603 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       5 | 1604 | `{` |
|       - | 1605 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   61739 | 1606 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1607 | `	sxu32 n;` |
|   61739 | 1608 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1609 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1610 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1611 | `		return SXRET_OK;` |
|       - | 1612 | `	}` |
|       - | 1613 | `	/* Start the release process */` |
|   61739 | 1614 | `	n = 0;` |
|   61739 | 1615 | `	pEntry = pMap->pFirst;` |
| 1595448 | 1616 | `	for(;;){` |
| 3190901 | 1617 | `		if( n >= pMap->nEntry ){` |
|   61739 | 1618 | `			break;` |
|       - | 1619 | `		}` |
| 3129167 | 1620 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1621 | `		/* Remove the reference from the foreign table */` |
| 3129167 | 1622 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3129167 | 1623 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1624 | `			/* Restore the ph7_value to the free list */` |
| 3129157 | 1625 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1564576 | 1626 | `		}` |
|       - | 1627 | `		/* Release the node */` |
| 3129167 | 1628 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   74155 | 1629 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   37075 | 1630 | `		}` |
| 3129167 | 1631 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1632 | `		/* Point to the next entry */` |
| 3129167 | 1633 | `		pEntry = pNext;` |
| 3129167 | 1634 | `		n++;` |
|       5 | 1635 | `	}` |
|   61739 | 1636 | `	if( pMap->nEntry > 0 ){` |
|       - | 1637 | `		/* Release the hash bucket */` |
|   54661 | 1638 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   27328 | 1639 | `	}` |
|   61739 | 1640 | `	if( FreeDS ){` |
|       - | 1641 | `		/* Free the whole instance */` |
|   61723 | 1642 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   30864 | 1643 | `	}else{` |
|       - | 1644 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1645 | `		pMap->apBucket = 0;` |
|      17 | 1646 | `		pMap->iNextIdx = 0;` |
|      17 | 1647 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1648 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1649 | `	}` |
|   61739 | 1650 | `	return SXRET_OK;` |
|   30872 | 1651 | `}` |
|       - | 1652 | `/*` |
|       - | 1653 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1654 | ` * If the count reaches zero which mean no more variables` |
|       - | 1655 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1656 | ` */` |
|  669848 | 1657 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       5 | 1658 | `{` |
|  669853 | 1659 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1660 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  669853 | 1661 | `	pMap->iRef--;` |
|  669853 | 1662 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   61703 | 1663 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   30849 | 1664 | `	}` |
|  669853 | 1665 | `}` |
|       - | 1666 | `/*` |
|       - | 1667 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1668 | ` * Write a pointer to the target node on success.` |
|       - | 1669 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1670 | ` */` |
|  125754 | 1671 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1672 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1673 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1674 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1675 | `	)` |
|       5 | 1676 | `{` |
|       - | 1677 | `	sxi32 rc;` |
|  125759 | 1678 | `	if( pMap->nEntry < 1 ){` |
|       - | 1679 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1680 | `		 */` |
|      64 | 1681 | `		return SXERR_NOTFOUND;` |
|       - | 1682 | `	}` |
|  125699 | 1683 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  125699 | 1684 | `	return rc;` |
|   62882 | 1685 | `}` |
|       - | 1686 | `/*` |
|       - | 1687 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1688 | ` * hashmap.` |
|       - | 1689 | ` * If a node with the given key already exists in the database` |
|       - | 1690 | ` * then this function overwrite the old value.` |
|       - | 1691 | ` */` |
| 2549380 | 1692 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1693 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1694 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1695 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1696 | `	)` |
|       5 | 1697 | `{` |
|       - | 1698 | `	sxi32 rc;` |
| 2549385 | 1699 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1700 | `		/*` |
|       - | 1701 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1702 | `		 */` |
|     ! 0 | 1703 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1704 | `		return SXRET_OK;` |
|       - | 1705 | `	}` |
| 2549385 | 1706 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2549385 | 1707 | `	return rc;` |
| 1274695 | 1708 | `}` |
|       - | 1709 | `/*` |
|       - | 1710 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|       - | 1711 | ` *   - String keys overwrite same-key entries in pDest.` |
|       - | 1712 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|       - | 1713 | ` * This is the same routine that backs array_merge().` |
|       - | 1714 | ` */` |
|      52 | 1715 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1716 | `{` |
|      53 | 1717 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|       1 | 1718 | `}` |
|       - | 1719 | `/*` |
|       - | 1720 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1721 | ` * hashmap.` |
|       - | 1722 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1723 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1724 | ` * The insertion by reference is triggered when the following` |
|       - | 1725 | ` * expression is encountered.` |
|       - | 1726 | ` * $var = 10;` |
|       - | 1727 | ` *  $a = array(&var);` |
|       - | 1728 | ` * OR` |
|       - | 1729 | ` *  $a[] =& $var;` |
|       - | 1730 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1731 | ` * over it's contents.` |
|       - | 1732 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1733 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1734 | ` * Example:` |
|       - | 1735 | ` *  $var = 10;` |
|       - | 1736 | ` *  $a[] =& $var;` |
|       - | 1737 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1738 | ` *  //Unset the foreign ph7_value now` |
|       - | 1739 | ` *  unset($var);` |
|       - | 1740 | ` *  echo count($a); //0` |
|       - | 1741 | ` * Note that this is a PH7 eXtension.` |
|       - | 1742 | ` * Refer to the official documentation for more information.` |
|       - | 1743 | ` * If a node with the given key already exists in the database` |
|       - | 1744 | ` * then this function overwrite the old value.` |
|       - | 1745 | ` */` |
|   45922 | 1746 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1747 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1748 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1749 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1750 | `	)` |
|       5 | 1751 | `{` |
|       - | 1752 | `	sxi32 rc;` |
|   45927 | 1753 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1754 | `		/*` |
|       - | 1755 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1756 | `		 */` |
|     ! 0 | 1757 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1758 | `		return SXRET_OK;` |
|       - | 1759 | `	}` |
|   45927 | 1760 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   45927 | 1761 | `	return rc;` |
|   22966 | 1762 | `}` |
|       - | 1763 | `/*` |
|       - | 1764 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1765 | ` */` |
|   27254 | 1766 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       5 | 1767 | `{` |
|       - | 1768 | `	/* Reset the loop cursor */` |
|   27259 | 1769 | `	pMap->pCur = pMap->pFirst;` |
|   27259 | 1770 | `}` |
|       - | 1771 | `/*` |
|       - | 1772 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1773 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1774 | ` * return NULL.` |
|       - | 1775 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1776 | ` */` |
|  228146 | 1777 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       5 | 1778 | `{` |
|  228151 | 1779 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  228151 | 1780 | `	if( pCur == 0 ){` |
|       - | 1781 | `		/* End of the list,return null */` |
|   13651 | 1782 | `		return 0;` |
|       - | 1783 | `	}` |
|       - | 1784 | `	/* Advance the node cursor */` |
|  214505 | 1785 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  214505 | 1786 | `	return pCur;` |
|  114078 | 1787 | `}` |
|       - | 1788 | `/*` |
|       - | 1789 | ` * Extract a node value.` |
|       - | 1790 | ` */` |
|  539858 | 1791 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       5 | 1792 | `{` |
|  539863 | 1793 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  539863 | 1794 | `	if( pEntry ){` |
|  539863 | 1795 | `		if( bStore ){` |
|  214843 | 1796 | `			PH7_MemObjStore(pEntry,pValue);` |
|  107424 | 1797 | `		}else{` |
|  325025 | 1798 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1799 | `		}` |
|  269985 | 1800 | `	}else{` |
|     ! 0 | 1801 | `		PH7_MemObjRelease(pValue);` |
|       - | 1802 | `	}` |
|  539863 | 1803 | `}` |
|       - | 1804 | `/*` |
|       - | 1805 | ` * Extract a node key.` |
|       - | 1806 | ` */` |
|  134512 | 1807 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       5 | 1808 | `{` |
|       - | 1809 | `	/* Fill with the current key */` |
|  134517 | 1810 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  134035 | 1811 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1812 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1813 | `		}` |
|  134035 | 1814 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  134035 | 1815 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   67020 | 1816 | `	}else{` |
|     485 | 1817 | `		SyBlobReset(&pKey->sBlob);` |
|     485 | 1818 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     485 | 1819 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1820 | `	}` |
|  134517 | 1821 | `}` |
|       - | 1822 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1823 | `/*` |
|       - | 1824 | ` * Store the address of nodes value in the given container.` |
|       - | 1825 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1826 | ` * defined in 'builtin.c' for more information.` |
|       - | 1827 | ` */` |
|      10 | 1828 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1829 | `{` |
|      11 | 1830 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1831 | `	ph7_value *pValue;` |
|       - | 1832 | `	sxu32 n;` |
|       - | 1833 | `	/* Initialize the container */` |
|      11 | 1834 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1835 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1836 | `		/* Extract node value */` |
|      17 | 1837 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1838 | `		if( pValue ){` |
|      17 | 1839 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1840 | `		}` |
|       - | 1841 | `		/* Point to the next entry */` |
|      17 | 1842 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1843 | `	}` |
|       - | 1844 | `	/* Total inserted entries */` |
|      11 | 1845 | `	return (int)SySetUsed(pOut);` |
|       1 | 1846 | `}` |
|       - | 1847 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1848 | `/* SPDX-SnippetBegin */` |
|       - | 1849 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - | 1850 | `/* SPDX-License-Identifier: blessing */` |
|       - | 1851 | `/*` |
|       - | 1852 | ` * Merge sort.` |
|       - | 1853 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1854 | ` * Status: Public domain` |
|       - | 1855 | ` */` |
|       - | 1856 | `/* Node comparison callback signature */` |
|       - | 1857 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1858 | `/*` |
|       - | 1859 | `** Inputs:` |
|       - | 1860 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1861 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1862 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1863 | `**` |
|       - | 1864 | `** Return Value:` |
|       - | 1865 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1866 | `**   of both a and b.` |
|       - | 1867 | `**` |
|       - | 1868 | `** Side effects:` |
|       - | 1869 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1870 | `**   changed.` |
|       - | 1871 | `*/` |
|   33246 | 1872 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1873 | `{` |
|       - | 1874 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1875 | `    /* Prevent compiler warning */` |
|   33251 | 1876 | `	result.pNext = result.pPrev = 0;` |
|   33251 | 1877 | `	pTail = &result;` |
|  101741 | 1878 | `	while( pA && pB ){` |
|   68495 | 1879 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   45345 | 1880 | `			pTail->pPrev = pA;` |
|   45345 | 1881 | `			pA->pNext = pTail;` |
|   45345 | 1882 | `			pTail = pA;` |
|   45345 | 1883 | `			pA = pA->pPrev;` |
|   22674 | 1884 | `		}else{` |
|   23155 | 1885 | `			pTail->pPrev = pB;` |
|   23155 | 1886 | `			pB->pNext = pTail;` |
|   23155 | 1887 | `			pTail = pB;` |
|   23155 | 1888 | `			pB = pB->pPrev;` |
|       - | 1889 | `		}` |
|       5 | 1890 | `	}` |
|   33251 | 1891 | `	if( pA ){` |
|   23252 | 1892 | `		pTail->pPrev = pA;` |
|   23252 | 1893 | `		pA->pNext = pTail;` |
|   21630 | 1894 | `	}else if( pB ){` |
|    9790 | 1895 | `		pTail->pPrev = pB;` |
|    9790 | 1896 | `		pB->pNext = pTail;` |
|    4895 | 1897 | `	}else{` |
|     219 | 1898 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1899 | `	}` |
|   33251 | 1900 | `	return result.pPrev;` |
|       5 | 1901 | `}` |
|       - | 1902 | `/*` |
|       - | 1903 | `** Inputs:` |
|       - | 1904 | `**   Map:       Input hashmap` |
|       - | 1905 | `**   cmp:       A comparison function.` |
|       - | 1906 | `**` |
|       - | 1907 | `** Return Value:` |
|       - | 1908 | `**   Sorted hashmap.` |
|       - | 1909 | `**` |
|       - | 1910 | `** Side effects:` |
|       - | 1911 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1912 | `*/` |
|       - | 1913 | `#define N_SORT_BUCKET  32` |
|     686 | 1914 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1915 | `{` |
|       - | 1916 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1917 | `	sxu32 i;` |
|     691 | 1918 | `	SyZero(a,sizeof(a));` |
|       - | 1919 | `	/* Point to the first inserted entry */` |
|     691 | 1920 | `	pIn = pMap->pFirst;` |
|   13893 | 1921 | `	while( pIn ){` |
|   13207 | 1922 | `		p = pIn;` |
|   13207 | 1923 | `		pIn = p->pPrev;` |
|   13207 | 1924 | `		p->pPrev = 0;` |
|   25187 | 1925 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   25187 | 1926 | `			if( a[i]==0 ){` |
|   13207 | 1927 | `				a[i] = p;` |
|   13207 | 1928 | `				break;` |
|     ! 0 | 1929 | `			}else{` |
|   11985 | 1930 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   11985 | 1931 | `				a[i] = 0;` |
|       - | 1932 | `			}` |
|    5995 | 1933 | `		}` |
|   13207 | 1934 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1935 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1936 | `			 * But that is impossible.` |
|       - | 1937 | `			 */` |
|     ! 0 | 1938 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1939 | `		}` |
|       5 | 1940 | `	}` |
|     691 | 1941 | `	p = a[0];` |
|   21957 | 1942 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   21271 | 1943 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   10638 | 1944 | `	}` |
|     691 | 1945 | `	p->pNext = 0;` |
|       - | 1946 | `	/* Reflect the change */` |
|     691 | 1947 | `	pMap->pFirst = p;` |
|       - | 1948 | `	/* Reset the loop cursor */` |
|     691 | 1949 | `	pMap->pCur = pMap->pFirst;` |
|     691 | 1950 | `	return SXRET_OK;` |
|       5 | 1951 | `}` |
|       - | 1952 | `/* SPDX-SnippetEnd */` |
|       - | 1953 | `/*` |
|       - | 1954 | ` * Node comparison callback.` |
|       - | 1955 | ` * used-by: [sort(),asort(),...]` |
|       - | 1956 | ` */` |
|   68283 | 1957 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 | 1958 | `{` |
|       - | 1959 | `	ph7_value sA,sB;` |
|       - | 1960 | `	sxi32 iFlags;` |
|       - | 1961 | `	int rc;` |
|   68288 | 1962 | `	if( pCmpData == 0 ){` |
|       - | 1963 | `		/* Perform a standard comparison */` |
|   68264 | 1964 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   68264 | 1965 | `		return rc;` |
|       - | 1966 | `	}` |
|      25 | 1967 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1968 | `	/* Duplicate node values */` |
|      25 | 1969 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 1970 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 1971 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 1972 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 1973 | `	if( iFlags == 5 ){` |
|       - | 1974 | `		/* String cast */` |
|       - | 1975 | `		const char *zA,*zB;` |
|       - | 1976 | `		sxu32 nA,nB,nMin;` |
|      15 | 1977 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1978 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1979 | `		}` |
|      15 | 1980 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1981 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1982 | `		}` |
|       - | 1983 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 1984 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 1985 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 1986 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 1987 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 1988 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 1989 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 1990 | `		if( rc == 0 ){` |
|       5 | 1991 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1992 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1993 | `		}` |
|       8 | 1994 | `	}else{` |
|       - | 1995 | `		/* Numeric cast */` |
|      11 | 1996 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1997 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1998 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1999 | `	}` |
|      25 | 2000 | `	PH7_MemObjRelease(&sA);` |
|      25 | 2001 | `	PH7_MemObjRelease(&sB);` |
|      25 | 2002 | `	return rc;` |
|   34172 | 2003 | `}` |
|       - | 2004 | `/*` |
|       - | 2005 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2006 | ` * used-by: [ksort()]` |
|       - | 2007 | ` */` |
|      14 | 2008 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2009 | `{` |
|       - | 2010 | `	sxi32 rc;` |
|       7 | 2011 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 2012 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2013 | `		/* Perform a string comparison */` |
|       5 | 2014 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2015 | `	}else{` |
|       - | 2016 | `		SyString sStr;` |
|       - | 2017 | `		sxi64 iA,iB;` |
|       - | 2018 | `		/* Perform a numeric comparison */` |
|      11 | 2019 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2020 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2021 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2022 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2023 | `				iA = 0;` |
|     ! 0 | 2024 | `			}else{` |
|     ! 0 | 2025 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2026 | `			}` |
|     ! 0 | 2027 | `		}else{` |
|      11 | 2028 | `			iA = pA->xKey.iKey;` |
|       - | 2029 | `		}` |
|      11 | 2030 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2031 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2032 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2033 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2034 | `				iB = 0;` |
|     ! 0 | 2035 | `			}else{` |
|     ! 0 | 2036 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2037 | `			}` |
|     ! 0 | 2038 | `		}else{` |
|      11 | 2039 | `			iB = pB->xKey.iKey;` |
|       - | 2040 | `		}` |
|      11 | 2041 | `		rc = (sxi32)(iA-iB);` |
|       - | 2042 | `	}` |
|       - | 2043 | `	/* Comparison result */` |
|      15 | 2044 | `	return rc;` |
|       1 | 2045 | `}` |
|       - | 2046 | `/*` |
|       - | 2047 | ` * Node comparison callback.` |
|       - | 2048 | ` * Used by: [rsort(),arsort()];` |
|       - | 2049 | ` */` |
|      78 | 2050 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2051 | `{` |
|       - | 2052 | `	ph7_value sA,sB;` |
|       - | 2053 | `	sxi32 iFlags;` |
|       - | 2054 | `	int rc;` |
|      79 | 2055 | `	if( pCmpData == 0 ){` |
|       - | 2056 | `		/* Perform a standard comparison */` |
|      59 | 2057 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 2058 | `		return -rc;` |
|       - | 2059 | `	}` |
|      21 | 2060 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 2061 | `	/* Duplicate node values */` |
|      21 | 2062 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 2063 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 2064 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 2065 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 2066 | `	if( iFlags == 5 ){` |
|       - | 2067 | `		/* String cast */` |
|       - | 2068 | `		const char *zA,*zB;` |
|       - | 2069 | `		sxu32 nA,nB,nMin;` |
|      11 | 2070 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2071 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 2072 | `		}` |
|      11 | 2073 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2074 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 2075 | `		}` |
|       - | 2076 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 2077 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 2078 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 2079 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 2080 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 2081 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 2082 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 2083 | `		if( rc == 0 ){` |
|       3 | 2084 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 2085 | `			else if( nA > nB ) rc = 1;` |
|       1 | 2086 | `		}` |
|       6 | 2087 | `	}else{` |
|       - | 2088 | `		/* Numeric cast */` |
|      11 | 2089 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 2090 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 2091 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 2092 | `	}` |
|      21 | 2093 | `	PH7_MemObjRelease(&sA);` |
|      21 | 2094 | `	PH7_MemObjRelease(&sB);` |
|      21 | 2095 | `	return -rc;` |
|      40 | 2096 | `}` |
|       - | 2097 | `/*` |
|       - | 2098 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2099 | ` * used-by: [usort(),uasort()]` |
|       - | 2100 | ` */` |
|      88 | 2101 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       3 | 2102 | `{` |
|       - | 2103 | `	ph7_value sResult,*pCallback;` |
|       - | 2104 | `	ph7_value *pV1,*pV2;` |
|       - | 2105 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2106 | `	sxi32 rc;` |
|       - | 2107 | `	/* Point to the desired callback */` |
|      91 | 2108 | `	pCallback = (ph7_value *)pCmpData;` |
|      91 | 2109 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2110 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2111 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       6 | 2112 | `		return 0;` |
|       - | 2113 | `	}` |
|       - | 2114 | `	/* initialize the result value */` |
|      87 | 2115 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 2116 | `	/* Extract nodes values */` |
|      87 | 2117 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      87 | 2118 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      87 | 2119 | `	apArg[0] = pV1;` |
|      87 | 2120 | `	apArg[1] = pV2;` |
|       - | 2121 | `	/* Invoke the callback */` |
|      87 | 2122 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      87 | 2123 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2124 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2125 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       6 | 2126 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       6 | 2127 | `		rc = 0;` |
|      84 | 2128 | `	}else if( rc != SXRET_OK ){` |
|       - | 2129 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2130 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2131 | `	}else{` |
|       - | 2132 | `		/* Extract callback result */` |
|      82 | 2133 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2134 | `			/* Perform an int cast */` |
|     ! 0 | 2135 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2136 | `		}` |
|      82 | 2137 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2138 | `	}` |
|      87 | 2139 | `	PH7_MemObjRelease(&sResult);` |
|       - | 2140 | `	/* Callback result */` |
|      87 | 2141 | `	return rc;` |
|      47 | 2142 | `}` |
|       - | 2143 | `/*` |
|       - | 2144 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2145 | ` * used-by: [krsort()]` |
|       - | 2146 | ` */` |
|       4 | 2147 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2148 | `{` |
|       - | 2149 | `	sxi32 rc;` |
|       2 | 2150 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2151 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2152 | `		/* Perform a string comparison */` |
|       5 | 2153 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2154 | `	}else{` |
|       - | 2155 | `		SyString sStr;` |
|       - | 2156 | `		sxi64 iA,iB;` |
|       - | 2157 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2158 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2159 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2160 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2161 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2162 | `				iA = 0;` |
|     ! 0 | 2163 | `			}else{` |
|     ! 0 | 2164 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2165 | `			}` |
|     ! 0 | 2166 | `		}else{` |
|     ! 0 | 2167 | `			iA = pA->xKey.iKey;` |
|       - | 2168 | `		}` |
|     ! 0 | 2169 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2170 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2171 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2172 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2173 | `				iB = 0;` |
|     ! 0 | 2174 | `			}else{` |
|     ! 0 | 2175 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2176 | `			}` |
|     ! 0 | 2177 | `		}else{` |
|     ! 0 | 2178 | `			iB = pB->xKey.iKey;` |
|       - | 2179 | `		}` |
|     ! 0 | 2180 | `		rc = (sxi32)(iA-iB);` |
|       - | 2181 | `	}` |
|       5 | 2182 | `	return -rc; /* Reverse result */` |
|       1 | 2183 | `}` |
|       - | 2184 | `/*` |
|       - | 2185 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2186 | ` * used-by: [uksort()]` |
|       - | 2187 | ` */` |
|       6 | 2188 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2189 | `{` |
|       - | 2190 | `	ph7_value sResult,*pCallback;` |
|       - | 2191 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2192 | `	ph7_value sK1,sK2;` |
|       - | 2193 | `	sxi32 rc;` |
|       - | 2194 | `	/* Point to the desired callback */` |
|       7 | 2195 | `	pCallback = (ph7_value *)pCmpData;` |
|       7 | 2196 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2197 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2198 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     ! 0 | 2199 | `		return 0;` |
|       - | 2200 | `	}` |
|       - | 2201 | `	/* initialize the result value */` |
|       7 | 2202 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2203 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2204 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2205 | `	/* Extract nodes keys */` |
|       7 | 2206 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2207 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2208 | `	apArg[0] = &sK1;` |
|       7 | 2209 | `	apArg[1] = &sK2;` |
|       - | 2210 | `	/* Mark keys as constants */` |
|       7 | 2211 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2212 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2213 | `	/* Invoke the callback */` |
|       7 | 2214 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2215 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2216 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2217 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     ! 0 | 2218 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     ! 0 | 2219 | `		rc = 0;` |
|       7 | 2220 | `	}else if( rc != SXRET_OK ){` |
|       - | 2221 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2222 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2223 | `	}else{` |
|       - | 2224 | `		/* Extract callback result */` |
|       7 | 2225 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2226 | `			/* Perform an int cast */` |
|     ! 0 | 2227 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2228 | `		}` |
|       7 | 2229 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2230 | `	}` |
|       7 | 2231 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2232 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2233 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2234 | `	/* Callback result */` |
|       7 | 2235 | `	return rc;` |
|       4 | 2236 | `}` |
|       - | 2237 | `/*` |
|       - | 2238 | ` * Node comparison callback: Random node comparison.` |
|       - | 2239 | ` * used-by: [shuffle()]` |
|       - | 2240 | ` */` |
|      17 | 2241 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2242 | `{` |
|       - | 2243 | `	sxu32 n;` |
|       7 | 2244 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 2245 | `	SXUNUSED(pCmpData);` |
|       - | 2246 | `	/* Grab a random number */` |
|      18 | 2247 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2248 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2249 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2250 | `	 */` |
|      18 | 2251 | `	return n&1 ? 1 : -1;` |
|       1 | 2252 | `}` |
|       - | 2253 | `/*` |
|       - | 2254 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2255 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2256 | ` */` |
|     638 | 2257 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       5 | 2258 | `{` |
|       - | 2259 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2260 | `	sxu32 i;` |
|       - | 2261 | `	/* Rehash all entries */` |
|     643 | 2262 | `	pLast = p = pMap->pFirst;` |
|     643 | 2263 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     643 | 2264 | `	i = 0;` |
|    6835 | 2265 | `	for( ;; ){` |
|   13675 | 2266 | `		if( i >= pMap->nEntry ){` |
|     643 | 2267 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     643 | 2268 | `			break;` |
|       - | 2269 | `		}` |
|   13037 | 2270 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2271 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2272 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2273 | `			/* Change key type */` |
|       5 | 2274 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2275 | `		}` |
|   13037 | 2276 | `		HashmapRehashIntNode(p);` |
|       - | 2277 | `		/* Point to the next entry */` |
|   13037 | 2278 | `		i++;` |
|   13037 | 2279 | `		pLast = p;` |
|   13037 | 2280 | `		p = p->pPrev; /* Reverse link */` |
|       5 | 2281 | `	}` |
|     643 | 2282 | `}` |
|       - | 2283 | `/*` |
|       - | 2284 | ` * Array functions implementation.` |
|       - | 2285 | ` * Status:` |
|       - | 2286 | ` *  Stable.` |
|       - | 2287 | ` */` |
|       - | 2288 | `/*` |
|       - | 2289 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2290 | ` * Sort an array.` |
|       - | 2291 | ` * Parameters` |
|       - | 2292 | ` *  $array` |
|       - | 2293 | ` *   The input array.` |
|       - | 2294 | ` * $sort_flags` |
|       - | 2295 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2296 | ` *  Sorting type flags:` |
|       - | 2297 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2298 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2299 | ` *   SORT_STRING - compare items as strings` |
|       - | 2300 | ` * Return` |
|       - | 2301 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2302 | ` *` |
|       - | 2303 | ` */` |
|     982 | 2304 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2305 | `{` |
|       - | 2306 | `	ph7_hashmap *pMap;` |
|       - | 2307 | `	/* Make sure we are dealing with a valid hashmap */` |
|     987 | 2308 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2309 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2310 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2311 | `		return PH7_OK;` |
|       - | 2312 | `	}` |
|       - | 2313 | `	/* Point to the internal representation of the input hashmap */` |
|     987 | 2314 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     987 | 2315 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     987 | 2316 | `	if( pMap->nEntry > 1 ){` |
|     627 | 2317 | `		sxi32 iCmpFlags = 0;` |
|     627 | 2318 | `		if( nArg > 1 ){` |
|       - | 2319 | `			/* Extract comparison flags */` |
|       3 | 2320 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2321 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2322 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2323 | `			}` |
|       1 | 2324 | `		}` |
|       - | 2325 | `		/* Do the merge sort */` |
|     627 | 2326 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2327 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     627 | 2328 | `		HashmapSortRehash(pMap);` |
|     311 | 2329 | `	}` |
|       - | 2330 | `	/* All done,return TRUE */` |
|     987 | 2331 | `	ph7_result_bool(pCtx,1);` |
|     987 | 2332 | `	return PH7_OK;` |
|     496 | 2333 | `}` |
|       - | 2334 | `/*` |
|       - | 2335 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2336 | ` *  Sort an array and maintain index association.` |
|       - | 2337 | ` * Parameters` |
|       - | 2338 | ` *  $array` |
|       - | 2339 | ` *   The input array.` |
|       - | 2340 | ` * $sort_flags` |
|       - | 2341 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2342 | ` *  Sorting type flags:` |
|       - | 2343 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2344 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2345 | ` *   SORT_STRING - compare items as strings` |
|       - | 2346 | ` * Return` |
|       - | 2347 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2348 | ` */` |
|      32 | 2349 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2350 | `{` |
|       - | 2351 | `	ph7_hashmap *pMap;` |
|       - | 2352 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2353 | `	if( nArg < 1 ){` |
|       3 | 2354 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2355 | `			"ArgumentCountError",` |
|       - | 2356 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2357 | `			);` |
|       - | 2358 | `	}` |
|       - | 2359 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2360 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2361 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2362 | `			"TypeError",` |
|       - | 2363 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2364 | `			ph7_type_name(apArg[0])` |
|       - | 2365 | `			);` |
|       - | 2366 | `	}` |
|       - | 2367 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2368 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2369 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2370 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2371 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2372 | `		if( nArg > 1 ){` |
|       - | 2373 | `			/* Extract comparison flags */` |
|       5 | 2374 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2375 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2376 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2377 | `			}` |
|       2 | 2378 | `		}` |
|       - | 2379 | `		/* Do the merge sort */` |
|      19 | 2380 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2381 | `		/* Fix the last link broken by the merge */` |
|      45 | 2382 | `		while(pMap->pLast->pPrev){` |
|      27 | 2383 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2384 | `		}` |
|       9 | 2385 | `	}` |
|       - | 2386 | `	/* All done,return TRUE */` |
|      23 | 2387 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2388 | `	return PH7_OK;` |
|      21 | 2389 | `}` |
|       - | 2390 | `/*` |
|       - | 2391 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2392 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2393 | ` * Parameters` |
|       - | 2394 | ` *  $array` |
|       - | 2395 | ` *   The input array.` |
|       - | 2396 | ` * $sort_flags` |
|       - | 2397 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2398 | ` *  Sorting type flags:` |
|       - | 2399 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2400 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2401 | ` *   SORT_STRING - compare items as strings` |
|       - | 2402 | ` * Return` |
|       - | 2403 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2404 | ` */` |
|      32 | 2405 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2406 | `{` |
|       - | 2407 | `	ph7_hashmap *pMap;` |
|       - | 2408 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2409 | `	if( nArg < 1 ){` |
|       3 | 2410 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2411 | `			"ArgumentCountError",` |
|       - | 2412 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2413 | `			);` |
|       - | 2414 | `	}` |
|       - | 2415 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2416 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2417 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2418 | `			"TypeError",` |
|       - | 2419 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2420 | `			ph7_type_name(apArg[0])` |
|       - | 2421 | `			);` |
|       - | 2422 | `	}` |
|       - | 2423 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2424 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2425 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2426 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2427 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2428 | `		if( nArg > 1 ){` |
|       - | 2429 | `			/* Extract comparison flags */` |
|       5 | 2430 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2431 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2432 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2433 | `			}` |
|       2 | 2434 | `		}` |
|       - | 2435 | `		/* Do the merge sort */` |
|      19 | 2436 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2437 | `		/* Fix the last link broken by the merge */` |
|      35 | 2438 | `		while(pMap->pLast->pPrev){` |
|      17 | 2439 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2440 | `		}` |
|       9 | 2441 | `	}` |
|       - | 2442 | `	/* All done,return TRUE */` |
|      23 | 2443 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2444 | `	return PH7_OK;` |
|      21 | 2445 | `}` |
|       - | 2446 | `/*` |
|       - | 2447 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2448 | ` *  Sort an array by key.` |
|       - | 2449 | ` * Parameters` |
|       - | 2450 | ` *  $array` |
|       - | 2451 | ` *   The input array.` |
|       - | 2452 | ` * $sort_flags` |
|       - | 2453 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2454 | ` *  Sorting type flags:` |
|       - | 2455 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2456 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2457 | ` *   SORT_STRING - compare items as strings` |
|       - | 2458 | ` * Return` |
|       - | 2459 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2460 | ` */` |
|       4 | 2461 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2462 | `{` |
|       - | 2463 | `	ph7_hashmap *pMap;` |
|       - | 2464 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2465 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2466 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2467 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2468 | `		return PH7_OK;` |
|       - | 2469 | `	}` |
|       - | 2470 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2471 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2472 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2473 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2474 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2475 | `		if( nArg > 1 ){` |
|       - | 2476 | `			/* Extract comparison flags */` |
|     ! 0 | 2477 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2478 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2479 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2480 | `			}` |
|     ! 0 | 2481 | `		}` |
|       - | 2482 | `		/* Do the merge sort */` |
|       5 | 2483 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2484 | `		/* Fix the last link broken by the merge */` |
|      15 | 2485 | `		while(pMap->pLast->pPrev){` |
|      11 | 2486 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2487 | `		}` |
|       2 | 2488 | `	}` |
|       - | 2489 | `	/* All done,return TRUE */` |
|       5 | 2490 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2491 | `	return PH7_OK;` |
|       3 | 2492 | `}` |
|       - | 2493 | `/*` |
|       - | 2494 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2495 | ` *  Sort an array by key in reverse order.` |
|       - | 2496 | ` * Parameters` |
|       - | 2497 | ` *  $array` |
|       - | 2498 | ` *   The input array.` |
|       - | 2499 | ` * $sort_flags` |
|       - | 2500 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2501 | ` *  Sorting type flags:` |
|       - | 2502 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2503 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2504 | ` *   SORT_STRING - compare items as strings` |
|       - | 2505 | ` * Return` |
|       - | 2506 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2507 | ` */` |
|       2 | 2508 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2509 | `{` |
|       - | 2510 | `	ph7_hashmap *pMap;` |
|       - | 2511 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2512 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2513 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2514 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2515 | `		return PH7_OK;` |
|       - | 2516 | `	}` |
|       - | 2517 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2518 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2519 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2520 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2521 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2522 | `		if( nArg > 1 ){` |
|       - | 2523 | `			/* Extract comparison flags */` |
|     ! 0 | 2524 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2525 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2526 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2527 | `			}` |
|     ! 0 | 2528 | `		}` |
|       - | 2529 | `		/* Do the merge sort */` |
|       3 | 2530 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2531 | `		/* Fix the last link broken by the merge */` |
|       7 | 2532 | `		while(pMap->pLast->pPrev){` |
|       5 | 2533 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2534 | `		}` |
|       1 | 2535 | `	}` |
|       - | 2536 | `	/* All done,return TRUE */` |
|       3 | 2537 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2538 | `	return PH7_OK;` |
|       2 | 2539 | `}` |
|       - | 2540 | `/*` |
|       - | 2541 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2542 | ` * Sort an array in reverse order.` |
|       - | 2543 | ` * Parameters` |
|       - | 2544 | ` *  $array` |
|       - | 2545 | ` *   The input array.` |
|       - | 2546 | ` * $sort_flags` |
|       - | 2547 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2548 | ` *  Sorting type flags:` |
|       - | 2549 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2550 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2551 | ` *   SORT_STRING - compare items as strings` |
|       - | 2552 | ` * Return` |
|       - | 2553 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2554 | ` */` |
|       2 | 2555 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2556 | `{` |
|       - | 2557 | `	ph7_hashmap *pMap;` |
|       - | 2558 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2559 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2560 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2561 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2562 | `		return PH7_OK;` |
|       - | 2563 | `	}` |
|       - | 2564 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2565 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2566 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2567 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2568 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2569 | `		if( nArg > 1 ){` |
|       - | 2570 | `			/* Extract comparison flags */` |
|     ! 0 | 2571 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2572 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2573 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2574 | `			}` |
|     ! 0 | 2575 | `		}` |
|       - | 2576 | `		/* Do the merge sort */` |
|       3 | 2577 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2578 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2579 | `		HashmapSortRehash(pMap);` |
|       1 | 2580 | `	}` |
|       - | 2581 | `	/* All done,return TRUE */` |
|       3 | 2582 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2583 | `	return PH7_OK;` |
|       2 | 2584 | `}` |
|       - | 2585 | `/*` |
|       - | 2586 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2587 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2588 | ` * Parameters` |
|       - | 2589 | ` *  $array` |
|       - | 2590 | ` *   The input array.` |
|       - | 2591 | ` * $cmp_function` |
|       - | 2592 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2593 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2594 | ` *  to, or greater than the second.` |
|       - | 2595 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2596 | ` * Return` |
|       - | 2597 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2598 | ` */` |
|      12 | 2599 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2600 | `{` |
|       - | 2601 | `	ph7_hashmap *pMap;` |
|       - | 2602 | `	/* Make sure we are dealing with a valid hashmap */` |
|      15 | 2603 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2604 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2605 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2606 | `		return PH7_OK;` |
|       - | 2607 | `	}` |
|       - | 2608 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2609 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2610 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      15 | 2611 | `	if( pMap->nEntry > 1 ){` |
|      15 | 2612 | `		ph7_value *pCallback = 0;` |
|       - | 2613 | `		ProcNodeCmp xCmp;` |
|      15 | 2614 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      15 | 2615 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2616 | `			/* Point to the desired callback */` |
|      15 | 2617 | `			pCallback = apArg[1];` |
|       9 | 2618 | `		}else{` |
|       - | 2619 | `			/* Use the default comparison function */` |
|     ! 0 | 2620 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2621 | `		}` |
|       - | 2622 | `		/* Do the merge sort */` |
|      15 | 2623 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      15 | 2624 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2625 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      15 | 2626 | `		HashmapSortRehash(pMap);` |
|      15 | 2627 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2628 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       6 | 2629 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       6 | 2630 | `			return PH7_EXCEPTION;` |
|       - | 2631 | `		}` |
|       4 | 2632 | `	}` |
|       - | 2633 | `	/* All done,return TRUE */` |
|      10 | 2634 | `	ph7_result_bool(pCtx,1);` |
|      10 | 2635 | `	return PH7_OK;` |
|       9 | 2636 | `}` |
|       - | 2637 | `/*` |
|       - | 2638 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2639 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2640 | ` *  and maintain index association.` |
|       - | 2641 | ` * Parameters` |
|       - | 2642 | ` *  $array` |
|       - | 2643 | ` *   The input array.` |
|       - | 2644 | ` * $cmp_function` |
|       - | 2645 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2646 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2647 | ` *  to, or greater than the second.` |
|       - | 2648 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2649 | ` * Return` |
|       - | 2650 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2651 | ` */` |
|       2 | 2652 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2653 | `{` |
|       - | 2654 | `	ph7_hashmap *pMap;` |
|       - | 2655 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2656 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2657 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2658 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2659 | `		return PH7_OK;` |
|       - | 2660 | `	}` |
|       - | 2661 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2662 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2663 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2664 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2665 | `		ph7_value *pCallback = 0;` |
|       - | 2666 | `		ProcNodeCmp xCmp;` |
|       3 | 2667 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2668 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2669 | `			/* Point to the desired callback */` |
|       3 | 2670 | `			pCallback = apArg[1];` |
|       2 | 2671 | `		}else{` |
|       - | 2672 | `			/* Use the default comparison function */` |
|     ! 0 | 2673 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2674 | `		}` |
|       - | 2675 | `		/* Do the merge sort */` |
|       3 | 2676 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2677 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2678 | `		/* Fix the last link broken by the merge */` |
|       5 | 2679 | `		while(pMap->pLast->pPrev){` |
|       3 | 2680 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2681 | `		}` |
|       3 | 2682 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2683 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2684 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2685 | `			return PH7_EXCEPTION;` |
|       - | 2686 | `		}` |
|       1 | 2687 | `	}` |
|       - | 2688 | `	/* All done,return TRUE */` |
|       3 | 2689 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2690 | `	return PH7_OK;` |
|       2 | 2691 | `}` |
|       - | 2692 | `/*` |
|       - | 2693 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2694 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2695 | ` *  function and maintain index association.` |
|       - | 2696 | ` * Parameters` |
|       - | 2697 | ` *  $array` |
|       - | 2698 | ` *   The input array.` |
|       - | 2699 | ` * $cmp_function` |
|       - | 2700 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2701 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2702 | ` *  to, or greater than the second.` |
|       - | 2703 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2704 | ` * Return` |
|       - | 2705 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2706 | ` */` |
|       2 | 2707 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2708 | `{` |
|       - | 2709 | `	ph7_hashmap *pMap;` |
|       - | 2710 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2711 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2712 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2713 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2714 | `		return PH7_OK;` |
|       - | 2715 | `	}` |
|       - | 2716 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2717 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2718 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2719 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2720 | `		ph7_value *pCallback = 0;` |
|       - | 2721 | `		ProcNodeCmp xCmp;` |
|       3 | 2722 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2723 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2724 | `			/* Point to the desired callback */` |
|       3 | 2725 | `			pCallback = apArg[1];` |
|       2 | 2726 | `		}else{` |
|       - | 2727 | `			/* Use the default comparison function */` |
|     ! 0 | 2728 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2729 | `		}` |
|       - | 2730 | `		/* Do the merge sort */` |
|       3 | 2731 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2732 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2733 | `		/* Fix the last link broken by the merge */` |
|       3 | 2734 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2735 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2736 | `		}` |
|       3 | 2737 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2738 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2739 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2740 | `			return PH7_EXCEPTION;` |
|       - | 2741 | `		}` |
|       1 | 2742 | `	}` |
|       - | 2743 | `	/* All done,return TRUE */` |
|       3 | 2744 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2745 | `	return PH7_OK;` |
|       2 | 2746 | `}` |
|       - | 2747 | `/*` |
|       - | 2748 | ` * bool shuffle(array &$array)` |
|       - | 2749 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2750 | ` * Parameters` |
|       - | 2751 | ` *  $array` |
|       - | 2752 | ` *   The input array.` |
|       - | 2753 | ` * Return` |
|       - | 2754 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2755 | ` *` |
|       - | 2756 | ` */` |
|       2 | 2757 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2758 | `{` |
|       - | 2759 | `	ph7_hashmap *pMap;` |
|       - | 2760 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2761 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2762 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2763 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2764 | `		return PH7_OK;` |
|       - | 2765 | `	}` |
|       - | 2766 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2767 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2768 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2769 | `	if( pMap->nEntry > 1 ){` |
|       - | 2770 | `		/* Do the merge sort */` |
|       3 | 2771 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2772 | `		/* Fix the last link broken by the merge */` |
|       7 | 2773 | `		while(pMap->pLast->pPrev){` |
|       5 | 2774 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2775 | `		}` |
|       1 | 2776 | `	}` |
|       - | 2777 | `	/* All done,return TRUE */` |
|       3 | 2778 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2779 | `	return PH7_OK;` |
|       2 | 2780 | `}` |
|       - | 2781 | `/*` |
|       - | 2782 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2783 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2784 | ` * Parameters` |
|       - | 2785 | ` *  $var` |
|       - | 2786 | ` *   The array or the object.` |
|       - | 2787 | ` * $mode` |
|       - | 2788 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2789 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2790 | ` *  all the elements of a multidimensional array.` |
|       - | 2791 | ` * Return` |
|       - | 2792 | ` *  Returns the number of elements in the array.` |
|       - | 2793 | ` */` |
|     840 | 2794 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2795 | `{` |
|     845 | 2796 | `	int bRecursive = FALSE;` |
|     845 | 2797 | `	int bCycleDetected = FALSE;` |
|       - | 2798 | `	sxi64 iCount;` |
|     845 | 2799 | `	if( nArg < 1 ){` |
|       3 | 2800 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2801 | `			"ArgumentCountError",` |
|       - | 2802 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2803 | `			);` |
|       - | 2804 | `	}` |
|     843 | 2805 | `	if( nArg > 2 ){` |
|       4 | 2806 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2807 | `			"ArgumentCountError",` |
|       - | 2808 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2809 | `			nArg` |
|       - | 2810 | `			);` |
|       - | 2811 | `	}` |
|       - | 2812 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - | 2813 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - | 2814 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|     841 | 2815 | `	if( nArg > 1 ){` |
|      44 | 2816 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      44 | 2817 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|      11 | 2818 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2819 | `				"ValueError",` |
|       - | 2820 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2821 | `				);` |
|       - | 2822 | `		}` |
|      34 | 2823 | `		bRecursive = iMode == 1;` |
|      16 | 2824 | `	}` |
|     833 | 2825 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2826 | `		/* Countable object: dispatch to ->count() */` |
|      35 | 2827 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      24 | 2828 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      24 | 2829 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      24 | 2830 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      21 | 2831 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2832 | `					"count",sizeof("count")-1);` |
|      21 | 2833 | `				if( pMeth ){` |
|       - | 2834 | `					ph7_value sResult;` |
|      21 | 2835 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      21 | 2836 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      21 | 2837 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      21 | 2838 | `					PH7_MemObjRelease(&sResult);` |
|      21 | 2839 | `					return PH7_OK;` |
|       - | 2840 | `				}` |
|     ! 0 | 2841 | `			}` |
|       1 | 2842 | `		}` |
|      22 | 2843 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2844 | `			"TypeError",` |
|       - | 2845 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       6 | 2846 | `			ph7_type_name(apArg[0])` |
|       - | 2847 | `			);` |
|       - | 2848 | `	}` |
|       - | 2849 | `	/* Count */` |
|     803 | 2850 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     803 | 2851 | `	if( bCycleDetected ){` |
|       3 | 2852 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2853 | `	}` |
|     803 | 2854 | `	ph7_result_int64(pCtx,iCount);` |
|     803 | 2855 | `	return PH7_OK;` |
|     425 | 2856 | `}` |
|       - | 2857 | `/*` |
|       - | 2858 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2859 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2860 | ` * Parameters` |
|       - | 2861 | ` * $key` |
|       - | 2862 | ` *   Value to check.` |
|       - | 2863 | ` * $search` |
|       - | 2864 | ` *  An array with keys to check.` |
|       - | 2865 | ` * Return` |
|       - | 2866 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2867 | ` */` |
|      84 | 2868 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2869 | `{` |
|       - | 2870 | `	sxi32 rc;` |
|      89 | 2871 | `	if( nArg != 2 ){` |
|       - | 2872 | `		/* PHP requires exactly two arguments */` |
|      12 | 2873 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2874 | `			"ArgumentCountError",` |
|       - | 2875 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2876 | `			nArg` |
|       - | 2877 | `			);` |
|       - | 2878 | `	}` |
|       - | 2879 | `	/* Make sure we are dealing with a valid hashmap */` |
|      83 | 2880 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2881 | `		/* Type mismatch -> TypeError */` |
|       8 | 2882 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2883 | `			"TypeError",` |
|       - | 2884 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2885 | `			ph7_type_name(apArg[1])` |
|       - | 2886 | `			);` |
|       - | 2887 | `	}` |
|       - | 2888 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      78 | 2889 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2890 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2891 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2892 | `			"use an empty string instead"` |
|       - | 2893 | `			);` |
|      77 | 2894 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2895 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2896 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2897 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2898 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2899 | `				,rVal` |
|       - | 2900 | `				);` |
|       1 | 2901 | `		}` |
|       1 | 2902 | `	}` |
|       - | 2903 | `	/* Perform the lookup */` |
|      78 | 2904 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2905 | `	/* lookup result */` |
|      78 | 2906 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      78 | 2907 | `	return PH7_OK;` |
|      47 | 2908 | `}` |
|       - | 2909 | `/*` |
|       - | 2910 | ` * value array_pop(array $array)` |
|       - | 2911 | ` *   POP the last inserted element from the array.` |
|       - | 2912 | ` * Parameter` |
|       - | 2913 | ` *  The array to get the value from.` |
|       - | 2914 | ` * Return` |
|       - | 2915 | ` *  Poped value or NULL on failure.` |
|       - | 2916 | ` */` |
|      18 | 2917 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2918 | `{` |
|       - | 2919 | `	ph7_hashmap *pMap;` |
|       - | 2920 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      23 | 2921 | `	if( nArg != 1 ){` |
|       8 | 2922 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2923 | `			"ArgumentCountError",` |
|       - | 2924 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2925 | `			nArg` |
|       - | 2926 | `			);` |
|       - | 2927 | `	}` |
|       - | 2928 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2929 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      18 | 2930 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 2931 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2932 | `			"Error",` |
|       - | 2933 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2934 | `			);` |
|       - | 2935 | `	}` |
|       - | 2936 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2937 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2938 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2939 | `			"TypeError",` |
|       - | 2940 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2941 | `			ph7_type_name(apArg[0])` |
|       - | 2942 | `			);` |
|       - | 2943 | `	}` |
|       9 | 2944 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2945 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2946 | `	if( pMap->nEntry < 1 ){` |
|       - | 2947 | `		/* Nothing to pop,return NULL */` |
|       3 | 2948 | `		ph7_result_null(pCtx);` |
|       2 | 2949 | `	}else{` |
|       7 | 2950 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2951 | `		ph7_value *pObj;` |
|       7 | 2952 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2953 | `		if( pObj ){` |
|       - | 2954 | `			/* Node value */` |
|       7 | 2955 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2956 | `			/* Unlink the node */` |
|       7 | 2957 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2958 | `		}else{` |
|     ! 0 | 2959 | `			ph7_result_null(pCtx);` |
|       - | 2960 | `		}` |
|       - | 2961 | `		/* Reset the cursor */` |
|       7 | 2962 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2963 | `	}` |
|       9 | 2964 | `	return PH7_OK;` |
|      14 | 2965 | `}` |
|       - | 2966 | `/*` |
|       - | 2967 | ` * int array_push($array,$var,...)` |
|       - | 2968 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2969 | ` * Parameters` |
|       - | 2970 | ` *  array` |
|       - | 2971 | ` *    The input array.` |
|       - | 2972 | ` *  var` |
|       - | 2973 | ` *   On or more value to push.` |
|       - | 2974 | ` * Return` |
|       - | 2975 | ` *  New array count (including old items).` |
|       - | 2976 | ` */` |
|      24 | 2977 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2978 | `{` |
|       - | 2979 | `	ph7_hashmap *pMap;` |
|       - | 2980 | `	sxi32 rc;` |
|       - | 2981 | `	int i;` |
|      29 | 2982 | `	if( nArg < 1 ){` |
|       4 | 2983 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2984 | `			"ArgumentCountError",` |
|       - | 2985 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2986 | `			nArg` |
|       - | 2987 | `			);` |
|       - | 2988 | `	}` |
|       - | 2989 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2990 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      26 | 2991 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 2992 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2993 | `			"Error",` |
|       - | 2994 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2995 | `			);` |
|       - | 2996 | `	}` |
|       - | 2997 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 2998 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2999 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3000 | `			"TypeError",` |
|       - | 3001 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3002 | `			ph7_type_name(apArg[0])` |
|       - | 3003 | `			);` |
|       - | 3004 | `	}` |
|       - | 3005 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 3006 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      18 | 3007 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3008 | `	/* Start pushing given values */` |
|      34 | 3009 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      20 | 3010 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      20 | 3011 | `		if( rc != SXRET_OK ){` |
|       3 | 3012 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       - | 3013 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|       3 | 3014 | `				return rc;` |
|       - | 3015 | `			}` |
|     ! 0 | 3016 | `			break;` |
|       - | 3017 | `		}` |
|       9 | 3018 | `	}` |
|       - | 3019 | `	/* Return the new count */` |
|      15 | 3020 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 3021 | `	return PH7_OK;` |
|      17 | 3022 | `}` |
|       - | 3023 | `/*` |
|       - | 3024 | ` * value array_shift(array $array)` |
|       - | 3025 | ` *   Shift an element off the beginning of array.` |
|       - | 3026 | ` * Parameter` |
|       - | 3027 | ` *  The array to get the value from.` |
|       - | 3028 | ` * Return` |
|       - | 3029 | ` *  Shifted value or NULL on failure.` |
|       - | 3030 | ` */` |
|      38 | 3031 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3032 | `{` |
|       - | 3033 | `	ph7_hashmap *pMap;` |
|       - | 3034 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      43 | 3035 | `	if( nArg != 1 ){` |
|       8 | 3036 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3037 | `			"ArgumentCountError",` |
|       - | 3038 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 3039 | `			nArg` |
|       - | 3040 | `			);` |
|       - | 3041 | `	}` |
|       - | 3042 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      39 | 3043 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 3044 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3045 | `			"Error",` |
|       - | 3046 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 3047 | `			);` |
|       - | 3048 | `	}` |
|       - | 3049 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 3050 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3051 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3052 | `			"TypeError",` |
|       - | 3053 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3054 | `			ph7_type_name(apArg[0])` |
|       - | 3055 | `			);` |
|       - | 3056 | `	}` |
|       - | 3057 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 3058 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      33 | 3059 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3060 | `	if( pMap->nEntry < 1 ){` |
|       - | 3061 | `		/* Empty hashmap,return NULL */` |
|       3 | 3062 | `		ph7_result_null(pCtx);` |
|       2 | 3063 | `	}else{` |
|      31 | 3064 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 3065 | `		ph7_value *pObj;` |
|       - | 3066 | `		sxu32 n;` |
|      31 | 3067 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      31 | 3068 | `		if( pObj ){` |
|       - | 3069 | `			/* Node value */` |
|      31 | 3070 | `			ph7_result_value(pCtx,pObj);` |
|       - | 3071 | `			/* Unlink the first node */` |
|      31 | 3072 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      18 | 3073 | `		}else{` |
|     ! 0 | 3074 | `			ph7_result_null(pCtx);` |
|       - | 3075 | `		}` |
|       - | 3076 | `		/* Rehash all int keys */` |
|      31 | 3077 | `		n = pMap->nEntry;` |
|      31 | 3078 | `		pEntry = pMap->pFirst;` |
|      31 | 3079 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 3080 | `		for(;;){` |
|      85 | 3081 | `			if( n < 1 ){` |
|      31 | 3082 | `				break;` |
|       - | 3083 | `			}` |
|      59 | 3084 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      59 | 3085 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 3086 | `			}` |
|       - | 3087 | `			/* Point to the next entry */` |
|      59 | 3088 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      59 | 3089 | `			n--;` |
|       5 | 3090 | `		}` |
|       - | 3091 | `		/* Reset the cursor */` |
|      31 | 3092 | `		pMap->pCur = pMap->pFirst;` |
|       - | 3093 | `	}` |
|      33 | 3094 | `	return PH7_OK;` |
|      24 | 3095 | `}` |
|       - | 3096 | `/*` |
|       - | 3097 | ` * Extract the node cursor value.` |
|       - | 3098 | ` */` |
|      24 | 3099 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 3100 | `{` |
|      25 | 3101 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 3102 | `	ph7_value *pVal;` |
|      25 | 3103 | `	if( pCur == 0 ){` |
|       - | 3104 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 3105 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3106 | `		return PH7_OK;` |
|       - | 3107 | `	}` |
|      25 | 3108 | `	if( iDirection != 0 ){` |
|       9 | 3109 | `		if( iDirection > 0 ){` |
|       - | 3110 | `			/* Point to the next entry */` |
|       7 | 3111 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 3112 | `			pCur = pMap->pCur;` |
|       4 | 3113 | `		}else{` |
|       - | 3114 | `			/* Point to the previous entry */` |
|       3 | 3115 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 3116 | `			pCur = pMap->pCur;` |
|       - | 3117 | `		}` |
|       9 | 3118 | `		if( pCur == 0 ){` |
|       - | 3119 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 3120 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3121 | `			return PH7_OK;` |
|       - | 3122 | `		}` |
|       4 | 3123 | `	}` |
|       - | 3124 | `	/* Point to the desired element */` |
|      25 | 3125 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 3126 | `	if( pVal ){` |
|      25 | 3127 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 3128 | `	}else{` |
|     ! 0 | 3129 | `		ph7_result_bool(pCtx,0);` |
|       - | 3130 | `	}` |
|      25 | 3131 | `	return PH7_OK;` |
|      13 | 3132 | `}` |
|       - | 3133 | `/*` |
|       - | 3134 | ` * value current(array $array)` |
|       - | 3135 | ` *  Return the current element in an array.` |
|       - | 3136 | ` * Parameter` |
|       - | 3137 | ` *  $input: The input array.` |
|       - | 3138 | ` * Return` |
|       - | 3139 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 3140 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3141 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3142 | ` *  is empty, current() returns FALSE.` |
|       - | 3143 | ` */` |
|      10 | 3144 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3145 | `{` |
|      11 | 3146 | `	if( nArg < 1 ){` |
|       - | 3147 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3148 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3149 | `		return PH7_OK;` |
|       - | 3150 | `	}` |
|       - | 3151 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 3152 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3153 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3154 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3155 | `		return PH7_OK;` |
|       - | 3156 | `	}` |
|      11 | 3157 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 3158 | `	return PH7_OK;` |
|       6 | 3159 | `}` |
|       - | 3160 | `/*` |
|       - | 3161 | ` * value next(array $input)` |
|       - | 3162 | ` *  Advance the internal array pointer of an array.` |
|       - | 3163 | ` * Parameter` |
|       - | 3164 | ` *  $input: The input array.` |
|       - | 3165 | ` * Return` |
|       - | 3166 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 3167 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 3168 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 3169 | ` */` |
|       6 | 3170 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3171 | `{` |
|       7 | 3172 | `	if( nArg < 1 ){` |
|       - | 3173 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3174 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3175 | `		return PH7_OK;` |
|       - | 3176 | `	}` |
|       - | 3177 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 3178 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3179 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3180 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3181 | `		return PH7_OK;` |
|       - | 3182 | `	}` |
|       7 | 3183 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 3184 | `	return PH7_OK;` |
|       4 | 3185 | `}` |
|       - | 3186 | `/*` |
|       - | 3187 | ` * value prev(array $input)` |
|       - | 3188 | ` *  Rewind the internal array pointer.` |
|       - | 3189 | ` * Parameter` |
|       - | 3190 | ` *  $input: The input array.` |
|       - | 3191 | ` * Return` |
|       - | 3192 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3193 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3194 | ` *  elements.` |
|       - | 3195 | ` */` |
|       2 | 3196 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3197 | `{` |
|       3 | 3198 | `	if( nArg < 1 ){` |
|       - | 3199 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3200 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3201 | `		return PH7_OK;` |
|       - | 3202 | `	}` |
|       - | 3203 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3204 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3205 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3206 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3207 | `		return PH7_OK;` |
|       - | 3208 | `	}` |
|       3 | 3209 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3210 | `	return PH7_OK;` |
|       2 | 3211 | `}` |
|       - | 3212 | `/*` |
|       - | 3213 | ` * value end(array $input)` |
|       - | 3214 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3215 | ` * Parameter` |
|       - | 3216 | ` *  $input: The input array.` |
|       - | 3217 | ` * Return` |
|       - | 3218 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3219 | ` */` |
|       2 | 3220 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3221 | `{` |
|       - | 3222 | `	ph7_hashmap *pMap;` |
|       3 | 3223 | `	if( nArg < 1 ){` |
|       - | 3224 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3225 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3226 | `		return PH7_OK;` |
|       - | 3227 | `	}` |
|       - | 3228 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3229 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3230 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3231 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3232 | `		return PH7_OK;` |
|       - | 3233 | `	}` |
|       - | 3234 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3235 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3236 | `	/* Point to the last node */` |
|       3 | 3237 | `	pMap->pCur = pMap->pLast;` |
|       - | 3238 | `	/* Return the last node value */` |
|       3 | 3239 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3240 | `	return PH7_OK;` |
|       2 | 3241 | `}` |
|       - | 3242 | `/*` |
|       - | 3243 | ` * value reset(array $array )` |
|       - | 3244 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3245 | ` * Parameter` |
|       - | 3246 | ` *  $input: The input array.` |
|       - | 3247 | ` * Return` |
|       - | 3248 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3249 | ` */` |
|       4 | 3250 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3251 | `{` |
|       - | 3252 | `	ph7_hashmap *pMap;` |
|       5 | 3253 | `	if( nArg < 1 ){` |
|       - | 3254 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3255 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3256 | `		return PH7_OK;` |
|       - | 3257 | `	}` |
|       - | 3258 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3259 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3260 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3261 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3262 | `		return PH7_OK;` |
|       - | 3263 | `	}` |
|       - | 3264 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3265 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3266 | `	/* Point to the first node */` |
|       5 | 3267 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3268 | `	/* Return the last node value if available */` |
|       5 | 3269 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3270 | `	return PH7_OK;` |
|       3 | 3271 | `}` |
|       - | 3272 | `/*` |
|       - | 3273 | ` * value key(array $array)` |
|       - | 3274 | ` *   Fetch a key from an array` |
|       - | 3275 | ` * Parameter` |
|       - | 3276 | ` *  $input` |
|       - | 3277 | ` *   The input array.` |
|       - | 3278 | ` * Return` |
|       - | 3279 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3280 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3281 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3282 | ` *  is empty, key() returns NULL.` |
|       - | 3283 | ` */` |
|       4 | 3284 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3285 | `{` |
|       - | 3286 | `	ph7_hashmap_node *pCur;` |
|       - | 3287 | `	ph7_hashmap *pMap;` |
|       5 | 3288 | `	if( nArg < 1 ){` |
|       - | 3289 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3290 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3291 | `		return PH7_OK;` |
|       - | 3292 | `	}` |
|       - | 3293 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3294 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3295 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3296 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3297 | `		return PH7_OK;` |
|       - | 3298 | `	}` |
|       5 | 3299 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3300 | `	pCur = pMap->pCur;` |
|       5 | 3301 | `	if( pCur == 0 ){` |
|       - | 3302 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3303 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3304 | `		return PH7_OK;` |
|       - | 3305 | `	}` |
|       5 | 3306 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3307 | `		/* Key is integer */` |
|     ! 0 | 3308 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3309 | `	}else{` |
|       - | 3310 | `		/* Key is blob */` |
|       7 | 3311 | `		ph7_result_string(pCtx,` |
|       4 | 3312 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3313 | `	}` |
|       5 | 3314 | `	return PH7_OK;` |
|       3 | 3315 | `}` |
|       - | 3316 | `/*` |
|       - | 3317 | ` * array each(array $input)` |
|       - | 3318 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3319 | ` * Parameter` |
|       - | 3320 | ` *  $input` |
|       - | 3321 | ` *    The input array.` |
|       - | 3322 | ` * Return` |
|       - | 3323 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3324 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3325 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3326 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3327 | ` *  each() returns FALSE.` |
|       - | 3328 | ` */` |
|      22 | 3329 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3330 | `{` |
|       - | 3331 | `	ph7_hashmap_node *pCur;` |
|       - | 3332 | `	ph7_hashmap *pMap;` |
|       - | 3333 | `	ph7_value *pArray;` |
|       - | 3334 | `	ph7_value *pVal;` |
|       - | 3335 | `	ph7_value sKey;` |
|      23 | 3336 | `	if( nArg < 1 ){` |
|       - | 3337 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3338 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3339 | `		return PH7_OK;` |
|       - | 3340 | `	}` |
|       - | 3341 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3342 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3343 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3344 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3345 | `		return PH7_OK;` |
|       - | 3346 | `	}` |
|       - | 3347 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3348 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3349 | `	if( pMap->pCur == 0 ){` |
|       - | 3350 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3351 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3352 | `		return PH7_OK;` |
|       - | 3353 | `	}` |
|      15 | 3354 | `	pCur = pMap->pCur;` |
|       - | 3355 | `	/* Create a new array */` |
|      15 | 3356 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3357 | `	if( pArray == 0 ){` |
|     ! 0 | 3358 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3359 | `		return PH7_OK;` |
|       - | 3360 | `	}` |
|      15 | 3361 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3362 | `	/* Insert the current value */` |
|      15 | 3363 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3364 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3365 | `	/* Make the key */` |
|      15 | 3366 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3367 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3368 | `	}else{` |
|       9 | 3369 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3370 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3371 | `	}` |
|       - | 3372 | `	/* Insert the current key */` |
|      15 | 3373 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3374 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3375 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3376 | `	/* Advance the cursor */` |
|      15 | 3377 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3378 | `	/* Return the current entry */` |
|      15 | 3379 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3380 | `	return PH7_OK;` |
|      12 | 3381 | `}` |
|       - | 3382 | `/*` |
|       - | 3383 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3384 | ` *  Create an array containing a range of elements` |
|       - | 3385 | ` * Parameter` |
|       - | 3386 | ` *  start` |
|       - | 3387 | ` *   First value of the sequence.` |
|       - | 3388 | ` *  limit` |
|       - | 3389 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3390 | ` *  step` |
|       - | 3391 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3392 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3393 | ` * Return` |
|       - | 3394 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3395 | ` * NOTE:` |
|       - | 3396 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3397 | ` */` |
|       2 | 3398 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3399 | `{` |
|       - | 3400 | `	ph7_value *pValue,*pArray;` |
|       - | 3401 | `	sxi64 iOfft,iLimit;` |
|       3 | 3402 | `	int iStep = 1;` |
|       - | 3403 |  |
|       3 | 3404 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3405 | `	if( nArg > 0 ){` |
|       - | 3406 | `		/* Extract the offset */` |
|       3 | 3407 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3408 | `		if( nArg > 1 ){` |
|       - | 3409 | `			/* Extract the limit */` |
|       3 | 3410 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3411 | `			if( nArg > 2 ){` |
|       - | 3412 | `				/* Extract the increment */` |
|       3 | 3413 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3414 | `				if( iStep < 1 ){` |
|       - | 3415 | `					/* Only positive number are allowed */` |
|       3 | 3416 | `					iStep = 1;` |
|       1 | 3417 | `				}` |
|       1 | 3418 | `			}` |
|       1 | 3419 | `		}` |
|       1 | 3420 | `	}` |
|       - | 3421 | `	/* Element container */` |
|       3 | 3422 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3423 | `	/* Create the new array */` |
|       3 | 3424 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3425 | `	if( pArray == 0 ){` |
|     ! 0 | 3426 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3427 | `	}` |
|       - | 3428 | `	/* Start filling */` |
|       3 | 3429 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3430 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3431 | `		/* Perform the insertion */` |
|     ! 0 | 3432 | `		if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       - | 3433 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3434 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3435 | `		}` |
|       - | 3436 | `		/* Increment */` |
|     ! 0 | 3437 | `		iOfft += iStep;` |
|     ! 0 | 3438 | `	}` |
|       - | 3439 | `	/* Return the new array */` |
|       3 | 3440 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3441 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3442 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3443 | `	 */` |
|       3 | 3444 | `	return PH7_OK;` |
|       2 | 3445 | `}` |
|       - | 3446 | `/*` |
|       - | 3447 | ` * array array_values(array $array)` |
|       - | 3448 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3449 | ` * Parameters` |
|       - | 3450 | ` *  $array` |
|       - | 3451 | ` *   The input array.` |
|       - | 3452 | ` * Return` |
|       - | 3453 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3454 | ` */` |
|      36 | 3455 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3456 | `{` |
|       - | 3457 | `	ph7_hashmap_node *pNode;` |
|       - | 3458 | `	ph7_hashmap *pMap;` |
|       - | 3459 | `	ph7_value *pArray;` |
|       - | 3460 | `	ph7_value *pObj;` |
|       - | 3461 | `	sxu32 n;` |
|      40 | 3462 | `	if( nArg != 1 ){` |
|       - | 3463 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       8 | 3464 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3465 | `			"ArgumentCountError",` |
|       - | 3466 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3467 | `			nArg` |
|       - | 3468 | `			);` |
|       - | 3469 | `	}` |
|       - | 3470 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 3471 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3472 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3473 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3474 | `			"TypeError",` |
|       - | 3475 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3476 | `			ph7_type_name(apArg[0])` |
|       - | 3477 | `			);` |
|       - | 3478 | `	}` |
|       - | 3479 | `	/* Point to the internal representation that describe the input hashmap */` |
|      32 | 3480 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3481 | `	/* Create a new array */` |
|      32 | 3482 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 3483 | `	if( pArray == 0 ){` |
|     ! 0 | 3484 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3485 | `		return PH7_OK;` |
|       - | 3486 | `	}` |
|       - | 3487 | `	/* Perform the requested operation */` |
|      32 | 3488 | `	pNode = pMap->pFirst;` |
|     104 | 3489 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      74 | 3490 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      74 | 3491 | `		if( pObj ){` |
|       - | 3492 | `			/* perform the insertion */` |
|      74 | 3493 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      36 | 3494 | `		}` |
|       - | 3495 | `		/* Point to the next entry */` |
|      74 | 3496 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      38 | 3497 | `	}` |
|       - | 3498 | `	/* return the new array */` |
|      32 | 3499 | `	ph7_result_value(pCtx,pArray);` |
|      32 | 3500 | `	return PH7_OK;` |
|      22 | 3501 | `}` |
|       - | 3502 | `/*` |
|       - | 3503 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3504 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3505 | ` * Parameters` |
|       - | 3506 | ` *  $input` |
|       - | 3507 | ` *   An array containing keys to return.` |
|       - | 3508 | ` * $search_value` |
|       - | 3509 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3510 | ` * $strict` |
|       - | 3511 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3512 | ` * Return` |
|       - | 3513 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3514 | ` */` |
|     138 | 3515 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3516 | `{` |
|       - | 3517 | `	ph7_hashmap_node *pNode;` |
|       - | 3518 | `	ph7_hashmap *pMap;` |
|       - | 3519 | `	ph7_value *pArray;` |
|       - | 3520 | `	ph7_value sObj;` |
|       - | 3521 | `	ph7_value sVal;` |
|       - | 3522 | `	SyString sKey;` |
|       - | 3523 | `	int bStrict;` |
|       - | 3524 | `	sxi32 rc;` |
|       - | 3525 | `	sxu32 n;` |
|     143 | 3526 | `	if( nArg < 1 ){` |
|       - | 3527 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3528 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3529 | `			"ArgumentCountError",` |
|       - | 3530 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3531 | `			);` |
|       - | 3532 | `	}` |
|       - | 3533 | `	/* Make sure we are dealing with a valid hashmap */` |
|     140 | 3534 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3535 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3536 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3537 | `			"TypeError",` |
|       - | 3538 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3539 | `			ph7_type_name(apArg[0])` |
|       - | 3540 | `			);` |
|       - | 3541 | `	}` |
|       - | 3542 | `	/* Point to the internal representation of the input hashmap */` |
|     138 | 3543 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3544 | `	/* Create a new array */` |
|     138 | 3545 | `	pArray = ph7_context_new_array(pCtx);` |
|     138 | 3546 | `	if( pArray == 0 ){` |
|     ! 0 | 3547 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3548 | `		return PH7_OK;` |
|       - | 3549 | `	}` |
|     138 | 3550 | `	bStrict = FALSE;` |
|     138 | 3551 | `	if( nArg > 2 ){` |
|       - | 3552 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3553 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3554 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3555 | `				"TypeError",` |
|       - | 3556 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3557 | `				ph7_type_name(apArg[2])` |
|       - | 3558 | `				);` |
|       - | 3559 | `		}` |
|       5 | 3560 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3561 | `	}` |
|       - | 3562 | `	/* Perform the requested operation */` |
|     135 | 3563 | `	pNode = pMap->pFirst;` |
|     135 | 3564 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    1117 | 3565 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     985 | 3566 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     133 | 3567 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      68 | 3568 | `		}else{` |
|     854 | 3569 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     854 | 3570 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3571 | `		}` |
|     985 | 3572 | `		rc = 0;` |
|     985 | 3573 | `		if( nArg > 1 ){` |
|      31 | 3574 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3575 | `			if( pValue ){` |
|      31 | 3576 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3577 | `				/* Filter key */` |
|      31 | 3578 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3579 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3580 | `			}` |
|      15 | 3581 | `		}` |
|     985 | 3582 | `		if( rc == 0 ){` |
|       - | 3583 | `			/* Perform the insertion */` |
|     967 | 3584 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     482 | 3585 | `		}` |
|     985 | 3586 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3587 | `		/* Point to the next entry */` |
|     985 | 3588 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     494 | 3589 | `	}` |
|       - | 3590 | `	/* return the new array */` |
|     135 | 3591 | `	ph7_result_value(pCtx,pArray);` |
|     135 | 3592 | `	return PH7_OK;` |
|      74 | 3593 | `}` |
|       - | 3594 | `/*` |
|       - | 3595 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3596 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3597 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3598 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3599 | ` * Parameters` |
|       - | 3600 | ` *  $arr1` |
|       - | 3601 | ` *   First array` |
|       - | 3602 | ` *  $arr2` |
|       - | 3603 | ` *   Second array` |
|       - | 3604 | ` * Return` |
|       - | 3605 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3606 | ` * Note` |
|       - | 3607 | ` *  This function is a symisc eXtension.` |
|       - | 3608 | ` */` |
|       4 | 3609 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3610 | `{` |
|       - | 3611 | `	ph7_hashmap *p1,*p2;` |
|       - | 3612 | `	int rc;` |
|       5 | 3613 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3614 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3615 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3616 | `		return PH7_OK;` |
|       - | 3617 | `	}` |
|       - | 3618 | `	/* Point to the hashmaps */` |
|       5 | 3619 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3620 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3621 | `	rc = (p1 == p2);` |
|       - | 3622 | `	/* Same instance? */` |
|       5 | 3623 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3624 | `	return PH7_OK;` |
|       3 | 3625 | `}` |
|       - | 3626 | `/*` |
|       - | 3627 | ` * array array_merge(array ...$arrays)` |
|       - | 3628 | ` *  Merge one or more arrays.` |
|       - | 3629 | ` * Parameters` |
|       - | 3630 | ` *  ...$arrays` |
|       - | 3631 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3632 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3633 | ` * Return` |
|       - | 3634 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3635 | ` *  with no arguments.` |
|       - | 3636 | ` */` |
|    1026 | 3637 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3638 | `{` |
|       - | 3639 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3640 | `	ph7_value *pArray;` |
|       - | 3641 | `	int i;` |
|       - | 3642 | `	/* Create a new array */` |
|    1031 | 3643 | `	pArray = ph7_context_new_array(pCtx);` |
|    1031 | 3644 | `	if( pArray == 0 ){` |
|     ! 0 | 3645 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3646 | `		return PH7_OK;` |
|       - | 3647 | `	}` |
|       - | 3648 | `	/* Point to the internal representation of the hashmap */` |
|    1031 | 3649 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3650 | `	/* Start merging */` |
|    3073 | 3651 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3652 | `		/* Make sure we are dealing with a valid hashmap */` |
|    2051 | 3653 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3654 | `			/* Type mismatch -> TypeError */` |
|       8 | 3655 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3656 | `				"TypeError",` |
|       - | 3657 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3658 | `				i + 1,` |
|       4 | 3659 | `				ph7_type_name(apArg[i])` |
|       - | 3660 | `				);` |
|     ! 0 | 3661 | `		}else{` |
|    2047 | 3662 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3663 | `			/* Merge the two hashmaps */` |
|    2047 | 3664 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3665 | `		}` |
|    1026 | 3666 | `	}` |
|       - | 3667 | `	/* Return the freshly created array */` |
|    1027 | 3668 | `	ph7_result_value(pCtx,pArray);` |
|    1027 | 3669 | `	return PH7_OK;` |
|     518 | 3670 | `}` |
|       - | 3671 | `/*` |
|       - | 3672 | ` * array array_copy(array $source)` |
|       - | 3673 | ` *  Make a blind copy of the target array.` |
|       - | 3674 | ` * Parameters` |
|       - | 3675 | ` *  $source` |
|       - | 3676 | ` *   Target array` |
|       - | 3677 | ` * Return` |
|       - | 3678 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3679 | ` * Note` |
|       - | 3680 | ` *  This function is a symisc eXtension.` |
|       - | 3681 | ` */` |
|      16 | 3682 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3683 | `{` |
|       - | 3684 | `	ph7_hashmap *pMap;` |
|       - | 3685 | `	ph7_value *pArray;` |
|      17 | 3686 | `	if( nArg < 1 ){` |
|       - | 3687 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3688 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3689 | `		return PH7_OK;` |
|       - | 3690 | `	}` |
|       - | 3691 | `	/* Create a new array */` |
|      17 | 3692 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3693 | `	if( pArray == 0 ){` |
|     ! 0 | 3694 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3695 | `		return PH7_OK;` |
|       - | 3696 | `	}` |
|       - | 3697 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3698 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3699 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3700 | `		/* Point to the internal representation of the source */` |
|      17 | 3701 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3702 | `		/* Perform the copy */` |
|      17 | 3703 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3704 | `	}else{` |
|       - | 3705 | `		/* Simple insertion */` |
|     ! 0 | 3706 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3707 | `	}` |
|       - | 3708 | `	/* Return the duplicated array */` |
|      17 | 3709 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3710 | `	return PH7_OK;` |
|       9 | 3711 | `}` |
|       - | 3712 | `/*` |
|       - | 3713 | ` * bool array_erase(array $source)` |
|       - | 3714 | ` *  Remove all elements from a given array.` |
|       - | 3715 | ` * Parameters` |
|       - | 3716 | ` *  $source` |
|       - | 3717 | ` *   Target array` |
|       - | 3718 | ` * Return` |
|       - | 3719 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3720 | ` * Note` |
|       - | 3721 | ` *  This function is a symisc eXtension.` |
|       - | 3722 | ` */` |
|      16 | 3723 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3724 | `{` |
|       - | 3725 | `	ph7_hashmap *pMap;` |
|      17 | 3726 | `	if( nArg < 1 ){` |
|       - | 3727 | `		/* Missing arguments */` |
|     ! 0 | 3728 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3729 | `		return PH7_OK;` |
|       - | 3730 | `	}` |
|       - | 3731 | `	/* Point to the target hashmap */` |
|      17 | 3732 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3733 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3734 | `	/* Erase */` |
|      17 | 3735 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3736 | `	return PH7_OK;` |
|       9 | 3737 | `}` |
|       - | 3738 | `/*` |
|       - | 3739 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3740 | ` *  Extract a slice of the array.` |
|       - | 3741 | ` * Parameters` |
|       - | 3742 | ` *  $array` |
|       - | 3743 | ` *    The input array.` |
|       - | 3744 | ` * $offset` |
|       - | 3745 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3746 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3747 | ` * $length (optional, nullable)` |
|       - | 3748 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3749 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3750 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3751 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3752 | ` * $preserve_keys (optional)` |
|       - | 3753 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3754 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3755 | ` * Return` |
|       - | 3756 | ` *   The new slice.` |
|       - | 3757 | ` */` |
|      50 | 3758 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3759 | `{` |
|       - | 3760 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3761 | `	ph7_hashmap_node *pCur;` |
|       - | 3762 | `	ph7_value *pArray;` |
|       - | 3763 | `	int iLength,iOfft;` |
|       - | 3764 | `	int bPreserve;` |
|       - | 3765 | `	sxi32 rc;` |
|      55 | 3766 | `	if( nArg < 2 ){` |
|       8 | 3767 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3768 | `			"ArgumentCountError",` |
|       - | 3769 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3770 | `			nArg` |
|       - | 3771 | `			);` |
|       - | 3772 | `	}` |
|      51 | 3773 | `	if( nArg > 4 ){` |
|       4 | 3774 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3775 | `			"ArgumentCountError",` |
|       - | 3776 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3777 | `			nArg` |
|       - | 3778 | `			);` |
|       - | 3779 | `	}` |
|      49 | 3780 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3781 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3782 | `			"TypeError",` |
|       - | 3783 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3784 | `			ph7_type_name(apArg[0])` |
|       - | 3785 | `			);` |
|       - | 3786 | `	}` |
|       - | 3787 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      82 | 3788 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      65 | 3789 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3790 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3791 | `			"TypeError",` |
|       - | 3792 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3793 | `			ph7_type_name(apArg[1])` |
|       - | 3794 | `			);` |
|       - | 3795 | `	}` |
|       - | 3796 | `	/* Validate $length type if provided: nullable int */` |
|      45 | 3797 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3798 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3799 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3800 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3801 | `				"TypeError",` |
|       - | 3802 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3803 | `				ph7_type_name(apArg[2])` |
|       - | 3804 | `				);` |
|       - | 3805 | `		}` |
|       8 | 3806 | `	}` |
|       - | 3807 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      43 | 3808 | `	if( nArg > 3 ){` |
|      10 | 3809 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3810 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3811 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3812 | `				"TypeError",` |
|       - | 3813 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3814 | `				ph7_type_name(apArg[3])` |
|       - | 3815 | `				);` |
|       - | 3816 | `		}` |
|       2 | 3817 | `	}` |
|       - | 3818 | `	/* Point the internal representation of the target array */` |
|      41 | 3819 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      41 | 3820 | `	bPreserve = FALSE;` |
|       - | 3821 | `	/* Get the offset */` |
|      41 | 3822 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      41 | 3823 | `	if( iOfft < 0 ){` |
|       5 | 3824 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3825 | `		if( iOfft < 0 ){` |
|       3 | 3826 | `			iOfft = 0;` |
|       1 | 3827 | `		}` |
|       2 | 3828 | `	}` |
|      41 | 3829 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3830 | `		/* Offset past end of array, return empty array */` |
|       5 | 3831 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3832 | `		if( pArray == 0 ){` |
|     ! 0 | 3833 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3834 | `			return PH7_OK;` |
|       - | 3835 | `		}` |
|       5 | 3836 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3837 | `		return PH7_OK;` |
|       - | 3838 | `	}` |
|       - | 3839 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      37 | 3840 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      37 | 3841 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3842 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3843 | `		if( iLength < 0 ){` |
|       5 | 3844 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3845 | `		}` |
|      15 | 3846 | `		if( iLength < 0 ){` |
|       3 | 3847 | `			iLength = 0;` |
|       1 | 3848 | `		}` |
|      15 | 3849 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3850 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3851 | `		}` |
|       7 | 3852 | `	}` |
|      37 | 3853 | `	if( nArg > 3 ){` |
|       5 | 3854 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3855 | `	}` |
|       - | 3856 | `	/* Create a new array */` |
|      37 | 3857 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 3858 | `	if( pArray == 0 ){` |
|     ! 0 | 3859 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3860 | `		return PH7_OK;` |
|       - | 3861 | `	}` |
|      37 | 3862 | `	if( iLength < 1 ){` |
|       - | 3863 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3864 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3865 | `		return PH7_OK;` |
|       - | 3866 | `	}` |
|       - | 3867 | `	/* Point to the desired entry */` |
|      33 | 3868 | `	pCur = pSrc->pFirst;` |
|      28 | 3869 | `	for(;;){` |
|      61 | 3870 | `		if( iOfft < 1 ){` |
|      33 | 3871 | `			break;` |
|       - | 3872 | `		}` |
|       - | 3873 | `		/* Point to the next entry */` |
|      33 | 3874 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      33 | 3875 | `		iOfft--;` |
|       5 | 3876 | `	}` |
|       - | 3877 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 3878 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      51 | 3879 | `	for(;;){` |
|     107 | 3880 | `		if( iLength < 1 ){` |
|      33 | 3881 | `			break;` |
|       - | 3882 | `		}` |
|       - | 3883 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3884 | `		{` |
|      79 | 3885 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      79 | 3886 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3887 | `		}` |
|      79 | 3888 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3889 | `			break;` |
|       - | 3890 | `		}` |
|       - | 3891 | `		/* Point to the next entry */` |
|      79 | 3892 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      79 | 3893 | `		iLength--;` |
|       5 | 3894 | `	}` |
|       - | 3895 | `	/* Return the freshly created array */` |
|      33 | 3896 | `	ph7_result_value(pCtx,pArray);` |
|      33 | 3897 | `	return PH7_OK;` |
|      30 | 3898 | `}` |
|       - | 3899 | `/*` |
|       - | 3900 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3901 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3902 | ` * beginning (becomes the new pFirst).` |
|       - | 3903 | ` */` |
|      30 | 3904 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3905 | `{` |
|       - | 3906 | `	ph7_hashmap_node *pNode;` |
|       - | 3907 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3908 | `	pNode = pMap->pLast;` |
|      31 | 3909 | `	if( pNode == 0 ){` |
|     ! 0 | 3910 | `		return;` |
|       - | 3911 | `	}` |
|      31 | 3912 | `	if( pNode->pNext == 0 ){` |
|       - | 3913 | `		/* Only node in the list, nothing to move */` |
|       5 | 3914 | `		return;` |
|       - | 3915 | `	}` |
|      27 | 3916 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3917 | `		/* Already in the correct position */` |
|       9 | 3918 | `		return;` |
|       - | 3919 | `	}` |
|       - | 3920 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3921 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3922 | `	pMap->pLast->pPrev = 0;` |
|       - | 3923 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3924 | `	if( pAfter == 0 ){` |
|       - | 3925 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3926 | `		pNode->pNext = 0;` |
|       3 | 3927 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3928 | `		if( pMap->pFirst ){` |
|       3 | 3929 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3930 | `		}` |
|       3 | 3931 | `		pMap->pFirst = pNode;` |
|       2 | 3932 | `	}else{` |
|      17 | 3933 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3934 | `		pNode->pPrev = pOldNext;` |
|      17 | 3935 | `		pNode->pNext = pAfter;` |
|      17 | 3936 | `		pAfter->pPrev = pNode;` |
|      17 | 3937 | `		if( pOldNext ){` |
|      17 | 3938 | `			pOldNext->pNext = pNode;` |
|       9 | 3939 | `		}else{` |
|     ! 0 | 3940 | `			pMap->pLast = pNode;` |
|       - | 3941 | `		}` |
|       - | 3942 | `	}` |
|      16 | 3943 | `}` |
|       - | 3944 | `/*` |
|       - | 3945 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3946 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3947 | ` * Parameters` |
|       - | 3948 | ` *  $array` |
|       - | 3949 | ` *    The input array.` |
|       - | 3950 | ` *  $offset` |
|       - | 3951 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3952 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3953 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3954 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3955 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3956 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3957 | ` *  $length (optional)` |
|       - | 3958 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3959 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3960 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3961 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3962 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3963 | ` *  $replacement (optional)` |
|       - | 3964 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3965 | ` *    with elements from this array.` |
|       - | 3966 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3967 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3968 | ` *    offset.` |
|       - | 3969 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3970 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3971 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3972 | ` * Return` |
|       - | 3973 | ` *   A new array consisting of the extracted elements.` |
|       - | 3974 | ` */` |
|      54 | 3975 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3976 | `{` |
|       - | 3977 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3978 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3979 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3980 | `	int iLength,iOfft,i;` |
|       - | 3981 | `	sxi32 rc;` |
|      58 | 3982 | `	if( nArg < 2 ){` |
|       8 | 3983 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3984 | `			"ArgumentCountError",` |
|       - | 3985 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3986 | `			nArg` |
|       - | 3987 | `			);` |
|       - | 3988 | `	}` |
|      52 | 3989 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3990 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3991 | `			"TypeError",` |
|       - | 3992 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3993 | `			ph7_type_name(apArg[0])` |
|       - | 3994 | `			);` |
|       - | 3995 | `	}` |
|       - | 3996 | `	/* Point to the internal representation of the target array */` |
|      49 | 3997 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3998 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3999 | `	/* Get the offset and clamp to valid range */` |
|      49 | 4000 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 4001 | `	if( iOfft < 0 ){` |
|       7 | 4002 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 4003 | `		if( iOfft < 0 ){` |
|       3 | 4004 | `			iOfft = 0;` |
|       2 | 4005 | `		}` |
|      46 | 4006 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 4007 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 4008 | `	}` |
|       - | 4009 | `	/* Get the length and clamp to valid range.` |
|       - | 4010 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 4011 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 4012 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 4013 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 4014 | `		if( iLength < 0 ){` |
|       7 | 4015 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 4016 | `			if( iLength < 0 ){` |
|       3 | 4017 | `				iLength = 0;` |
|       1 | 4018 | `			}` |
|       3 | 4019 | `		}` |
|      31 | 4020 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 4021 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 4022 | `		}` |
|      15 | 4023 | `	}` |
|       - | 4024 | `	/* Create the result array for removed elements */` |
|      49 | 4025 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 4026 | `	if( pArray == 0 ){` |
|     ! 0 | 4027 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4028 | `		return PH7_OK;` |
|       - | 4029 | `	}` |
|       - | 4030 | `	/* Get replacement array if provided */` |
|      49 | 4031 | `	pRep = 0;` |
|      49 | 4032 | `	if( nArg > 3 ){` |
|      21 | 4033 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 4034 | `			/* Perform an array cast */` |
|       3 | 4035 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 4036 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 4037 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 4038 | `			}` |
|       2 | 4039 | `		}else{` |
|      19 | 4040 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 4041 | `		}` |
|      21 | 4042 | `		if( pRep ){` |
|       - | 4043 | `			/* Reset the loop cursor */` |
|      21 | 4044 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 4045 | `		}` |
|      10 | 4046 | `	}` |
|       - | 4047 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 4048 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 4049 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 4050 | `		return PH7_OK;` |
|       - | 4051 | `	}` |
|       - | 4052 | `	/* Navigate to the offset position */` |
|      41 | 4053 | `	pCur = pSrc->pFirst;` |
|      85 | 4054 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 4055 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 4056 | `	}` |
|       - | 4057 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 4058 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 4059 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 4060 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 4061 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 4062 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 4063 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 4064 | `		pPrev = pCur->pPrev;` |
|      71 | 4065 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 4066 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 4067 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 4068 | `			break;` |
|       - | 4069 | `		}` |
|      71 | 4070 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 4071 | `	}` |
|       - | 4072 | `	/* Insert replacement elements at the correct position */` |
|      41 | 4073 | `	if( pRep ){` |
|       - | 4074 | `		ph7_value sSafeVal;` |
|      61 | 4075 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 4076 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 4077 | `			if( pRvalue ){` |
|       - | 4078 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 4079 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 4080 | `				 * since it points into that same pool. */` |
|      31 | 4081 | `				sSafeVal = *pRvalue;` |
|      31 | 4082 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 4083 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 4084 | `					pNewNode = pSrc->pLast;` |
|      31 | 4085 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 4086 | `					pInsertAfter = pNewNode;` |
|      15 | 4087 | `				}` |
|      15 | 4088 | `			}` |
|       1 | 4089 | `		}` |
|      10 | 4090 | `	}` |
|       - | 4091 | `	/* Return the freshly created array */` |
|      41 | 4092 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 4093 | `	return PH7_OK;` |
|      31 | 4094 | `}` |
|       - | 4095 | `/*` |
|       - | 4096 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 4097 | ` *  Checks if a value exists in an array.` |
|       - | 4098 | ` * Parameters` |
|       - | 4099 | ` *  $needle` |
|       - | 4100 | ` *   The searched value.` |
|       - | 4101 | ` *   Note:` |
|       - | 4102 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 4103 | ` * $haystack` |
|       - | 4104 | ` *  The target array.` |
|       - | 4105 | ` * $strict` |
|       - | 4106 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 4107 | ` *  will also check the types of the needle in the haystack.` |
|       - | 4108 | ` */` |
|   32368 | 4109 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4110 | `{` |
|       - | 4111 | `	ph7_value *pNeedle;` |
|       - | 4112 | `	int bStrict;` |
|       - | 4113 | `	int rc;` |
|   32373 | 4114 | `	if( nArg < 2 ){` |
|       - | 4115 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 4116 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4117 | `		return PH7_OK;` |
|       - | 4118 | `	}` |
|   32373 | 4119 | `	pNeedle = apArg[0];` |
|   32373 | 4120 | `	bStrict = 0;` |
|   32373 | 4121 | `	if( nArg > 2 ){` |
|      17 | 4122 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       8 | 4123 | `	}` |
|   32373 | 4124 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4125 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 4126 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 4127 | `		/* Set the comparison result */` |
|     ! 0 | 4128 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 4129 | `		return PH7_OK;` |
|       - | 4130 | `	}` |
|       - | 4131 | `	/* Perform the lookup */` |
|   32373 | 4132 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 4133 | `	/* Lookup result */` |
|   32373 | 4134 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   32373 | 4135 | `	return PH7_OK;` |
|   16189 | 4136 | `}` |
|       - | 4137 | `/*` |
|       - | 4138 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 4139 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 4140 | ` * Parameters` |
|       - | 4141 | ` * $needle` |
|       - | 4142 | ` *   The searched value.` |
|       - | 4143 | ` * $haystack` |
|       - | 4144 | ` *   The array.` |
|       - | 4145 | ` * $strict` |
|       - | 4146 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 4147 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 4148 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 4149 | ` * Return` |
|       - | 4150 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 4151 | ` */` |
|      28 | 4152 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4153 | `{` |
|       - | 4154 | `	ph7_hashmap_node *pEntry;` |
|       - | 4155 | `	ph7_value *pVal,sNeedle;` |
|       - | 4156 | `	ph7_hashmap *pMap;` |
|       - | 4157 | `	ph7_value sVal;` |
|       - | 4158 | `	int bStrict;` |
|       - | 4159 | `	sxu32 n;` |
|       - | 4160 | `	int rc;` |
|      33 | 4161 | `	if( nArg < 2 ){` |
|       - | 4162 | `		/* Missing argument,throw ArgumentCountError */` |
|       8 | 4163 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4164 | `			"ArgumentCountError",` |
|       - | 4165 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4166 | `			nArg` |
|       - | 4167 | `			);` |
|       - | 4168 | `	}` |
|      27 | 4169 | `	bStrict = FALSE;` |
|      27 | 4170 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4171 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4172 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4173 | `			"TypeError",` |
|       - | 4174 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4175 | `			ph7_type_name(apArg[1])` |
|       - | 4176 | `			);` |
|       - | 4177 | `	}` |
|      24 | 4178 | `	if( nArg > 2 ){` |
|       - | 4179 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4180 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4181 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4182 | `				"TypeError",` |
|       - | 4183 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4184 | `				ph7_type_name(apArg[2])` |
|       - | 4185 | `				);` |
|       - | 4186 | `		}` |
|       9 | 4187 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4188 | `	}` |
|       - | 4189 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4190 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4191 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4192 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4193 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4194 | `	pEntry = pMap->pFirst;` |
|      21 | 4195 | `	n = pMap->nEntry;` |
|      23 | 4196 | `	for(;;){` |
|      47 | 4197 | `		if( !n ){` |
|       9 | 4198 | `			break;` |
|       - | 4199 | `		}` |
|       - | 4200 | `		/* Extract node value */` |
|      39 | 4201 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4202 | `		if( pVal ){` |
|       - | 4203 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4204 | `			 * can change their type.` |
|       - | 4205 | `			 */` |
|      39 | 4206 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4207 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4208 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4209 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4210 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4211 | `			if( rc == 0 ){` |
|       - | 4212 | `				/* Match found,return key */` |
|      13 | 4213 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4214 | `					/* INT key */` |
|       7 | 4215 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4216 | `				}else{` |
|       7 | 4217 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4218 | `					/* Blob key */` |
|       7 | 4219 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4220 | `				}` |
|      13 | 4221 | `				return PH7_OK;` |
|       - | 4222 | `			}` |
|      13 | 4223 | `		}` |
|       - | 4224 | `		/* Point to the next entry */` |
|      27 | 4225 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4226 | `		n--;` |
|       1 | 4227 | `	}` |
|       - | 4228 | `	/* No such value,return FALSE */` |
|       9 | 4229 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4230 | `	return PH7_OK;` |
|      19 | 4231 | `}` |
|       - | 4232 | `/*` |
|       - | 4233 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4234 | ` *  Computes the difference of arrays.` |
|       - | 4235 | ` * Parameters` |
|       - | 4236 | ` *  $array1` |
|       - | 4237 | ` *    The array to compare from` |
|       - | 4238 | ` *  $array2` |
|       - | 4239 | ` *    An array to compare against` |
|       - | 4240 | ` *  $...` |
|       - | 4241 | ` *   More arrays to compare against` |
|       - | 4242 | ` * Return` |
|       - | 4243 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4244 | ` *  are not present in any of the other arrays.` |
|       - | 4245 | ` */` |
|      22 | 4246 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4247 | `{` |
|       - | 4248 | `	ph7_hashmap_node *pEntry;` |
|       - | 4249 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4250 | `	ph7_value *pArray;` |
|       - | 4251 | `	ph7_value *pVal;` |
|       - | 4252 | `	sxi32 rc;` |
|       - | 4253 | `	sxu32 n;` |
|       - | 4254 | `	int i;` |
|       - | 4255 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4256 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4257 | `	 * debugging difficult. */` |
|      26 | 4258 | `	if( nArg < 1 ){` |
|       4 | 4259 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4260 | `			"ArgumentCountError",` |
|       - | 4261 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4262 | `			nArg` |
|       - | 4263 | `			);` |
|       - | 4264 | `	}` |
|      23 | 4265 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4266 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4267 | `			"TypeError",` |
|       - | 4268 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4269 | `			ph7_type_name(apArg[0])` |
|       - | 4270 | `			);` |
|       - | 4271 | `	}` |
|      36 | 4272 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4273 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4274 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4275 | `				"TypeError",` |
|       - | 4276 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4277 | `				i + 1,` |
|       2 | 4278 | `				ph7_type_name(apArg[i])` |
|       - | 4279 | `				);` |
|       - | 4280 | `		}` |
|       9 | 4281 | `	}` |
|      17 | 4282 | `	if( nArg == 1 ){` |
|       - | 4283 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4284 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4285 | `		return PH7_OK;` |
|       - | 4286 | `	}` |
|       - | 4287 | `	/* Create a new array */` |
|      15 | 4288 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4289 | `	if( pArray == 0 ){` |
|     ! 0 | 4290 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4291 | `		return PH7_OK;` |
|       - | 4292 | `	}` |
|       - | 4293 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4294 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4295 | `	/* Perform the diff */` |
|      15 | 4296 | `	pEntry = pSrc->pFirst;` |
|      15 | 4297 | `	n = pSrc->nEntry;` |
|      27 | 4298 | `	for(;;){` |
|      55 | 4299 | `		if( n < 1 ){` |
|      15 | 4300 | `			break;` |
|       - | 4301 | `		}` |
|       - | 4302 | `		/* Extract the node value */` |
|      41 | 4303 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4304 | `		if( pVal ){` |
|      69 | 4305 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4306 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4307 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4308 | `				/* Perform the lookup */` |
|      45 | 4309 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4310 | `				if( rc == SXRET_OK ){` |
|       - | 4311 | `					/* Value exist */` |
|      17 | 4312 | `					break;` |
|       - | 4313 | `				}` |
|      15 | 4314 | `			}` |
|      41 | 4315 | `			if( i >= nArg ){` |
|       - | 4316 | `				/* Perform the insertion */` |
|      25 | 4317 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4318 | `			}` |
|      20 | 4319 | `		}` |
|       - | 4320 | `		/* Point to the next entry */` |
|      41 | 4321 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4322 | `		n--;` |
|       1 | 4323 | `	}` |
|       - | 4324 | `	/* Return the freshly created array */` |
|      15 | 4325 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4326 | `	return PH7_OK;` |
|      15 | 4327 | `}` |
|       - | 4328 | `/*` |
|       - | 4329 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4330 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4331 | ` * Parameters` |
|       - | 4332 | ` *  $array1` |
|       - | 4333 | ` *    The array to compare from` |
|       - | 4334 | ` *  $array2` |
|       - | 4335 | ` *    An array to compare against` |
|       - | 4336 | ` *  $...` |
|       - | 4337 | ` *   More arrays to compare against.` |
|       - | 4338 | ` * $callback` |
|       - | 4339 | ` *  The callback comparison function.` |
|       - | 4340 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4341 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4342 | ` *  than the second.` |
|       - | 4343 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4344 | ` * Return` |
|       - | 4345 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4346 | ` *  are not present in any of the other arrays.` |
|       - | 4347 | ` */` |
|      22 | 4348 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4349 | `{` |
|       - | 4350 | `	ph7_hashmap_node *pEntry;` |
|       - | 4351 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4352 | `	ph7_value *pCallback;` |
|       - | 4353 | `	ph7_value *pArray;` |
|       - | 4354 | `	ph7_value *pVal;` |
|       - | 4355 | `	sxi32 rc;` |
|       - | 4356 | `	sxu32 n;` |
|       - | 4357 | `	int i;` |
|       - | 4358 |  |
|       - | 4359 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      27 | 4360 | `	if( nArg < 2 ){` |
|       4 | 4361 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4362 | `			"ArgumentCountError",` |
|       - | 4363 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4364 | `			nArg` |
|       - | 4365 | `			);` |
|       - | 4366 | `	}` |
|      25 | 4367 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4368 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4369 | `			"TypeError",` |
|       - | 4370 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4371 | `			ph7_type_name(apArg[0])` |
|       - | 4372 | `			);` |
|       - | 4373 | `	}` |
|       - | 4374 |  |
|      23 | 4375 | `	if( nArg == 2 ){` |
|       - | 4376 | `		/* Only the original array and the callback were provided. */` |
|       - | 4377 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4378 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4379 | `		 * validation order.` |
|       - | 4380 | `		 */` |
|       4 | 4381 | `	} else {` |
|       - | 4382 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      27 | 4383 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      19 | 4384 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      11 | 4385 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4386 | `					"TypeError",` |
|       - | 4387 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4388 | `					i + 1,` |
|       6 | 4389 | `					ph7_type_name(apArg[i])` |
|       - | 4390 | `					);` |
|       - | 4391 | `			}` |
|       7 | 4392 | `		}` |
|       - | 4393 | `	}` |
|       - | 4394 |  |
|       - | 4395 | `	/* Identify the callback (always expected as the last argument). */` |
|      16 | 4396 | `	pCallback = apArg[nArg - 1];` |
|       - | 4397 | `	/* Validate the callback to match PHP's error messages. */` |
|      16 | 4398 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       9 | 4399 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4400 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4401 | `				"TypeError",` |
|       - | 4402 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4403 | `				nArg` |
|       - | 4404 | `				);` |
|       - | 4405 | `		}` |
|       6 | 4406 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4407 | `			int len;` |
|       3 | 4408 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4409 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4410 | `				"TypeError",` |
|       - | 4411 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4412 | `				nArg,` |
|       1 | 4413 | `				zName` |
|       - | 4414 | `				);` |
|       - | 4415 | `		}` |
|       4 | 4416 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4417 | `			"TypeError",` |
|       - | 4418 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4419 | `			nArg` |
|       - | 4420 | `			);` |
|       - | 4421 | `	}` |
|       - | 4422 |  |
|       7 | 4423 | `	if( nArg == 2 ){` |
|       - | 4424 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4425 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4426 | `		return PH7_OK;` |
|       - | 4427 | `	}` |
|       - | 4428 |  |
|       - | 4429 | `	/* Create a new array */` |
|       5 | 4430 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4431 | `	if( pArray == 0 ){` |
|     ! 0 | 4432 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4433 | `		return PH7_OK;` |
|       - | 4434 | `	}` |
|       - | 4435 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 4436 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4437 | `	/* Perform the diff */` |
|       5 | 4438 | `	pEntry = pSrc->pFirst;` |
|       5 | 4439 | `	n = pSrc->nEntry;` |
|       5 | 4440 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 4441 | `	for(;;){` |
|      11 | 4442 | `		if( n < 1 ){` |
|       3 | 4443 | `			break;` |
|       - | 4444 | `		}` |
|       - | 4445 | `		/* Extract the node value */` |
|       9 | 4446 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4447 | `		if( pVal ){` |
|      15 | 4448 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4449 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4450 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4451 | `				/* Perform the lookup */` |
|       9 | 4452 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       9 | 4453 | `				if( rc == SXRET_OK ){` |
|       - | 4454 | `					/* Value exist */` |
|       3 | 4455 | `					break;` |
|       - | 4456 | `				}` |
|       4 | 4457 | `			}` |
|       9 | 4458 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 4459 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 4460 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 4461 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 4462 | `				return PH7_EXCEPTION;` |
|       - | 4463 | `			}` |
|       7 | 4464 | `			if( i >= (nArg - 1)){` |
|       - | 4465 | `				/* Perform the insertion */` |
|       5 | 4466 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4467 | `			}` |
|       3 | 4468 | `		}` |
|       - | 4469 | `		/* Point to the next entry */` |
|       7 | 4470 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4471 | `		n--;` |
|       1 | 4472 | `	}` |
|       - | 4473 | `	/* Return the freshly created array */` |
|       3 | 4474 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4475 | `	return PH7_OK;` |
|      16 | 4476 | `}` |
|       - | 4477 | `/*` |
|       - | 4478 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4479 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4480 | ` * Parameters` |
|       - | 4481 | ` *  $array1` |
|       - | 4482 | ` *    The array to compare from` |
|       - | 4483 | ` *  $array2` |
|       - | 4484 | ` *    An array to compare against` |
|       - | 4485 | ` *  $...` |
|       - | 4486 | ` *   More arrays to compare against` |
|       - | 4487 | ` * Return` |
|       - | 4488 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4489 | ` *  are not present in any of the other arrays.` |
|       - | 4490 | ` */` |
|      20 | 4491 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4492 | `{` |
|       - | 4493 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4494 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4495 | `	ph7_value *pArray;` |
|       - | 4496 | `	ph7_value *pVal;` |
|       - | 4497 | `	sxi32 rc;` |
|       - | 4498 | `	sxu32 n;` |
|       - | 4499 | `	int i;` |
|       - | 4500 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4501 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4502 | `	 * accompanying integration tests to pass. */` |
|      25 | 4503 | `	if( nArg < 1 ){` |
|       4 | 4504 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4505 | `			"ArgumentCountError",` |
|       - | 4506 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4507 | `			nArg` |
|       - | 4508 | `			);` |
|       - | 4509 | `	}` |
|      22 | 4510 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4511 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4512 | `			"TypeError",` |
|       - | 4513 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4514 | `			ph7_type_name(apArg[0])` |
|       - | 4515 | `			);` |
|       - | 4516 | `	}` |
|      33 | 4517 | `	for(i = 1 ; i < nArg ; i++){` |
|      21 | 4518 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 4519 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4520 | `				"TypeError",` |
|       - | 4521 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4522 | `				i + 1,` |
|       4 | 4523 | `				ph7_type_name(apArg[i])` |
|       - | 4524 | `				);` |
|       - | 4525 | `		}` |
|       9 | 4526 | `	}` |
|      13 | 4527 | `	if( nArg == 1 ){` |
|       - | 4528 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4529 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4530 | `		return PH7_OK;` |
|       - | 4531 | `	}` |
|       - | 4532 | `	/* Create a new array */` |
|      11 | 4533 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4534 | `	if( pArray == 0 ){` |
|     ! 0 | 4535 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4536 | `		return PH7_OK;` |
|       - | 4537 | `	}` |
|       - | 4538 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4539 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4540 | `	/* Perform the diff */` |
|      11 | 4541 | `	pEntry = pSrc->pFirst;` |
|      11 | 4542 | `	n = pSrc->nEntry;` |
|      11 | 4543 | `	pN1 = pN2 = 0;` |
|      29 | 4544 | `	for(;;){` |
|       - | 4545 | `		int keep;` |
|      35 | 4546 | `		if( n < 1 ){` |
|      11 | 4547 | `			break;` |
|       - | 4548 | `		}` |
|       - | 4549 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4550 | `		keep = 1;` |
|      41 | 4551 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4552 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4553 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4554 | `			/* Perform a key lookup first */` |
|      29 | 4555 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4556 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4557 | `			}else{` |
|      17 | 4558 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4559 | `			}` |
|      29 | 4560 | `			if( rc != SXRET_OK ){` |
|       - | 4561 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4562 | `				continue;` |
|       - | 4563 | `			}` |
|       - | 4564 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4565 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4566 | `			if( pVal ){` |
|       - | 4567 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4568 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4569 | `				if( pVal2 ){` |
|      15 | 4570 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4571 | `					if( cmp == 0 ){` |
|       - | 4572 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4573 | `						keep = 0;` |
|      13 | 4574 | `						break;` |
|       - | 4575 | `					}` |
|       1 | 4576 | `				}` |
|       1 | 4577 | `			}` |
|       2 | 4578 | `		}` |
|      25 | 4579 | `		if( keep ){` |
|       - | 4580 | `			/* Perform the insertion */` |
|      13 | 4581 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4582 | `		}` |
|       - | 4583 | `		/* Point to the next entry */` |
|      25 | 4584 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4585 | `		n--;` |
|       1 | 4586 | `	}` |
|       - | 4587 | `	/* Return the freshly created array */` |
|      11 | 4588 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4589 | `	return PH7_OK;` |
|      15 | 4590 | `}` |
|       - | 4591 | `/*` |
|       - | 4592 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4593 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4594 | ` *  by a user supplied callback function.` |
|       - | 4595 | ` * Parameters` |
|       - | 4596 | ` *  $array1` |
|       - | 4597 | ` *    The array to compare from` |
|       - | 4598 | ` *  $array2` |
|       - | 4599 | ` *    An array to compare against` |
|       - | 4600 | ` *  $...` |
|       - | 4601 | ` *   More arrays to compare against.` |
|       - | 4602 | ` *  $key_compare_func` |
|       - | 4603 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4604 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4605 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4606 | ` * Return` |
|       - | 4607 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4608 | ` *  are not present in any of the other arrays.` |
|       - | 4609 | ` */` |
|      24 | 4610 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4611 | `{` |
|       - | 4612 | `	ph7_hashmap_node *pEntry;` |
|       - | 4613 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4614 | `	ph7_value *pCallback;` |
|       - | 4615 | `	ph7_value *pArray;` |
|       - | 4616 | `	sxi32 rc;` |
|       - | 4617 | `	sxu32 n;` |
|       - | 4618 | `	int i;` |
|       - | 4619 |  |
|       - | 4620 | `	/* Argument validation mimicking PHP errors. */` |
|      29 | 4621 | `	if( nArg < 2 ){` |
|       4 | 4622 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4623 | `			"ArgumentCountError",` |
|       - | 4624 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4625 | `			nArg` |
|       - | 4626 | `			);` |
|       - | 4627 | `	}` |
|      26 | 4628 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4629 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4630 | `			"TypeError",` |
|       - | 4631 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4632 | `			ph7_type_name(apArg[0])` |
|       - | 4633 | `			);` |
|       - | 4634 | `	}` |
|       - | 4635 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4636 | `	 * expected to be a callback. */` |
|      38 | 4637 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      19 | 4638 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4639 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4640 | `				"TypeError",` |
|       - | 4641 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4642 | `				i + 1,` |
|       2 | 4643 | `				ph7_type_name(apArg[i])` |
|       - | 4644 | `				);` |
|       - | 4645 | `		}` |
|       9 | 4646 | `	}` |
|       - | 4647 | `	/* Point to the callback value */` |
|      22 | 4648 | `	pCallback = apArg[nArg - 1];` |
|      22 | 4649 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4650 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4651 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4652 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4653 | `		 * string given" which we also reproduce. */` |
|       9 | 4654 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4655 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4656 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4657 | `				"TypeError",` |
|       - | 4658 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4659 | `				nArg` |
|       - | 4660 | `				);` |
|       - | 4661 | `		}` |
|       6 | 4662 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4663 | `			/* neither array nor string */` |
|       8 | 4664 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4665 | `				"TypeError",` |
|       - | 4666 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4667 | `				nArg` |
|       - | 4668 | `				);` |
|       - | 4669 | `		}` |
|       - | 4670 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4671 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4672 | `			"TypeError",` |
|       - | 4673 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4674 | `			nArg,` |
|     ! 0 | 4675 | `			ph7_type_name(pCallback)` |
|       - | 4676 | `			);` |
|       - | 4677 | `	}` |
|      13 | 4678 | `	if( nArg == 2 ){` |
|       - | 4679 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4680 | `		 * input array. */` |
|       3 | 4681 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4682 | `		return PH7_OK;` |
|       - | 4683 | `	}` |
|       - | 4684 | `	/* Create a new array */` |
|      11 | 4685 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4686 | `	if( pArray == 0 ){` |
|     ! 0 | 4687 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4688 | `		return PH7_OK;` |
|       - | 4689 | `	}` |
|       - | 4690 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4691 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4692 | `	/* Perform the diff */` |
|      11 | 4693 | `	pEntry = pSrc->pFirst;` |
|      11 | 4694 | `	n = pSrc->nEntry;` |
|      21 | 4695 | `	for(;;){` |
|       - | 4696 | `		int keep;` |
|      27 | 4697 | `		if( n < 1 ){` |
|       9 | 4698 | `			break;` |
|       - | 4699 | `		}` |
|      19 | 4700 | `		keep = 1;` |
|      31 | 4701 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4702 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 4703 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4704 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 4705 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 4706 | `			while( pIt ){` |
|       - | 4707 | `				/* build temporary key values for callback */` |
|       - | 4708 | `				ph7_value key1, key2, result;` |
|       - | 4709 | `				/* initialise only once using the appropriate helper */` |
|      33 | 4710 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4711 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4712 | `				}else{` |
|       - | 4713 | `					SyString sStr;` |
|      33 | 4714 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4715 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4716 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 4717 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4718 | `				}` |
|      33 | 4719 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4720 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4721 | `				}else{` |
|       - | 4722 | `					SyString sStr;` |
|      33 | 4723 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4724 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4725 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 4726 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4727 | `				}` |
|      33 | 4728 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4729 | `				/* call user callback with (key1, key2) */` |
|       - | 4730 | `				{` |
|       - | 4731 | `					ph7_value *apK[2];` |
|      33 | 4732 | `					apK[0] = &key1;` |
|      33 | 4733 | `					apK[1] = &key2;` |
|      33 | 4734 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4735 | `				}` |
|      33 | 4736 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 4737 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 4738 | `					 * array_uintersect (which signal back from` |
|       - | 4739 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 4740 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 4741 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 4742 | `					PH7_MemObjRelease(&result);` |
|       3 | 4743 | `					PH7_MemObjRelease(&key1);` |
|       3 | 4744 | `					PH7_MemObjRelease(&key2);` |
|       3 | 4745 | `					return PH7_EXCEPTION;` |
|       - | 4746 | `				}` |
|      31 | 4747 | `				if( rc == SXRET_OK ){` |
|      31 | 4748 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4749 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4750 | `					}` |
|      31 | 4751 | `					if( result.x.iVal == 0 ){` |
|       - | 4752 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4753 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4754 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4755 | `						if( pVal1 && pVal2 ){` |
|      13 | 4756 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4757 | `								keep = 0;` |
|       9 | 4758 | `								PH7_MemObjRelease(&result);` |
|       - | 4759 | `								/* release keys too before breaking */` |
|       9 | 4760 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4761 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4762 | `								break;` |
|       - | 4763 | `							}` |
|       2 | 4764 | `						}` |
|       2 | 4765 | `					}` |
|      11 | 4766 | `				}` |
|      23 | 4767 | `				PH7_MemObjRelease(&result);` |
|      23 | 4768 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4769 | `				PH7_MemObjRelease(&key2);` |
|       - | 4770 | `				/* move to next node */` |
|      23 | 4771 | `				pIt = pIt->pPrev;` |
|      23 | 4772 | `				if( keep == 0 ) break;` |
|       1 | 4773 | `			}` |
|      21 | 4774 | `			if( keep == 0 ) break;` |
|       7 | 4775 | `		}` |
|      17 | 4776 | `		if( keep ){` |
|       - | 4777 | `			/* Perform the insertion */` |
|       9 | 4778 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4779 | `		}` |
|       - | 4780 | `		/* Point to the next entry */` |
|      17 | 4781 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4782 | `		n--;` |
|       1 | 4783 | `	}` |
|       - | 4784 | `	/* Return the freshly created array */` |
|       9 | 4785 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4786 | `	return PH7_OK;` |
|      17 | 4787 | `}` |
|       - | 4788 | `/*` |
|       - | 4789 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4790 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4791 | ` * Parameters` |
|       - | 4792 | ` *  $array1` |
|       - | 4793 | ` *    The array to compare from` |
|       - | 4794 | ` *  $array2` |
|       - | 4795 | ` *    An array to compare against` |
|       - | 4796 | ` *  $...` |
|       - | 4797 | ` *   More arrays to compare against` |
|       - | 4798 | ` * Return` |
|       - | 4799 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4800 | ` *  in any of the other arrays.` |
|       - | 4801 | ` * Note that NULL is returned on failure.` |
|       - | 4802 | ` */` |
|      14 | 4803 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4804 | `{` |
|       - | 4805 | `	ph7_hashmap_node *pEntry;` |
|       - | 4806 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4807 | `	ph7_value *pArray;` |
|       - | 4808 | `	sxi32 rc;` |
|       - | 4809 | `	sxu32 n;` |
|       - | 4810 | `	int i;` |
|       - | 4811 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4812 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4813 | `	 * helpers. */` |
|      18 | 4814 | `	if( nArg < 1 ){` |
|       4 | 4815 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4816 | `			"ArgumentCountError",` |
|       - | 4817 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4818 | `			nArg` |
|       - | 4819 | `			);` |
|       - | 4820 | `	}` |
|      15 | 4821 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4822 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4823 | `			"TypeError",` |
|       - | 4824 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4825 | `			ph7_type_name(apArg[0])` |
|       - | 4826 | `			);` |
|       - | 4827 | `	}` |
|      20 | 4828 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4829 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4830 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4831 | `				"TypeError",` |
|       - | 4832 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4833 | `				i + 1,` |
|       2 | 4834 | `				ph7_type_name(apArg[i])` |
|       - | 4835 | `				);` |
|       - | 4836 | `		}` |
|       5 | 4837 | `	}` |
|       9 | 4838 | `	if( nArg == 1 ){` |
|       - | 4839 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4840 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4841 | `		return PH7_OK;` |
|       - | 4842 | `	}` |
|       - | 4843 | `	/* Create a new array */` |
|       7 | 4844 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4845 | `	if( pArray == 0 ){` |
|     ! 0 | 4846 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4847 | `		return PH7_OK;` |
|       - | 4848 | `	}` |
|       - | 4849 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4850 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4851 | `	/* Perfrom the diff */` |
|       7 | 4852 | `	pEntry = pSrc->pFirst;` |
|       7 | 4853 | `	n = pSrc->nEntry;` |
|      12 | 4854 | `	for(;;){` |
|      25 | 4855 | `		if( n < 1 ){` |
|       7 | 4856 | `			break;` |
|       - | 4857 | `		}` |
|      31 | 4858 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4859 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4860 | `				/* ignore */` |
|     ! 0 | 4861 | `				continue;` |
|       - | 4862 | `			}` |
|      23 | 4863 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4864 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4865 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4866 | `				/* Blob lookup */` |
|      17 | 4867 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4868 | `			}else{` |
|       - | 4869 | `				/* Int lookup */` |
|       7 | 4870 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4871 | `			}` |
|      23 | 4872 | `			if( rc == SXRET_OK ){` |
|       - | 4873 | `				/* Key exists,break immediately */` |
|      11 | 4874 | `				break;` |
|       - | 4875 | `			}` |
|       7 | 4876 | `		}` |
|      19 | 4877 | `		if( i >= nArg ){` |
|       - | 4878 | `			/* Perform the insertion */` |
|       9 | 4879 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4880 | `		}` |
|       - | 4881 | `		/* Point to the next entry */` |
|      19 | 4882 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4883 | `		n--;` |
|       1 | 4884 | `	}` |
|       - | 4885 | `	/* Return the freshly created array */` |
|       7 | 4886 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4887 | `	return PH7_OK;` |
|      11 | 4888 | `}` |
|       - | 4889 | `/*` |
|       - | 4890 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4891 | ` *  Computes the intersection of arrays.` |
|       - | 4892 | ` * Parameters` |
|       - | 4893 | ` *  $array1` |
|       - | 4894 | ` *    The array to compare from` |
|       - | 4895 | ` *  $array2` |
|       - | 4896 | ` *    An array to compare against` |
|       - | 4897 | ` *  $...` |
|       - | 4898 | ` *   More arrays to compare against` |
|       - | 4899 | ` * Return` |
|       - | 4900 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4901 | ` *  in all of the parameters.` |
|       - | 4902 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4903 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4904 | ` */` |
|      22 | 4905 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4906 | `{` |
|       - | 4907 | `	ph7_hashmap_node *pEntry;` |
|       - | 4908 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4909 | `	ph7_value *pArray;` |
|       - | 4910 | `	ph7_value *pVal;` |
|       - | 4911 | `	sxi32 rc;` |
|       - | 4912 | `	sxu32 n;` |
|       - | 4913 | `	int i;` |
|      26 | 4914 | `	if( nArg < 1 ){` |
|       4 | 4915 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4916 | `			"ArgumentCountError",` |
|       - | 4917 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4918 | `			nArg` |
|       - | 4919 | `			);` |
|       - | 4920 | `	}` |
|      23 | 4921 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4922 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4923 | `			"TypeError",` |
|       - | 4924 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4925 | `			ph7_type_name(apArg[0])` |
|       - | 4926 | `			);` |
|       - | 4927 | `	}` |
|      36 | 4928 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4929 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4930 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4931 | `				"TypeError",` |
|       - | 4932 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4933 | `				i + 1,` |
|       2 | 4934 | `				ph7_type_name(apArg[i])` |
|       - | 4935 | `				);` |
|       - | 4936 | `		}` |
|       9 | 4937 | `	}` |
|      17 | 4938 | `	if( nArg == 1 ){` |
|       - | 4939 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4940 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4941 | `		return PH7_OK;` |
|       - | 4942 | `	}` |
|       - | 4943 | `	/* Create a new array */` |
|      15 | 4944 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4945 | `	if( pArray == 0 ){` |
|     ! 0 | 4946 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4947 | `		return PH7_OK;` |
|       - | 4948 | `	}` |
|       - | 4949 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4950 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4951 | `	/* Perform the intersection */` |
|      15 | 4952 | `	pEntry = pSrc->pFirst;` |
|      15 | 4953 | `	n = pSrc->nEntry;` |
|      31 | 4954 | `	for(;;){` |
|      63 | 4955 | `		if( n < 1 ){` |
|      15 | 4956 | `			break;` |
|       - | 4957 | `		}` |
|       - | 4958 | `		/* Extract the node value */` |
|      49 | 4959 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4960 | `		if( pVal ){` |
|      79 | 4961 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4962 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4963 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4964 | `				/* Perform the lookup */` |
|      55 | 4965 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4966 | `				if( rc != SXRET_OK ){` |
|       - | 4967 | `					/* Value does not exist */` |
|      25 | 4968 | `					break;` |
|       - | 4969 | `				}` |
|      16 | 4970 | `			}` |
|      49 | 4971 | `			if( i >= nArg ){` |
|       - | 4972 | `				/* Perform the insertion */` |
|      25 | 4973 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4974 | `			}` |
|      24 | 4975 | `		}` |
|       - | 4976 | `		/* Point to the next entry */` |
|      49 | 4977 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4978 | `		n--;` |
|       1 | 4979 | `	}` |
|       - | 4980 | `	/* Return the freshly created array */` |
|      15 | 4981 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4982 | `	return PH7_OK;` |
|      15 | 4983 | `}` |
|       - | 4984 | `/*` |
|       - | 4985 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4986 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4987 | ` * Parameters` |
|       - | 4988 | ` *  $array1` |
|       - | 4989 | ` *    The array to compare from` |
|       - | 4990 | ` *  $array2` |
|       - | 4991 | ` *    An array to compare against` |
|       - | 4992 | ` *  $...` |
|       - | 4993 | ` *   More arrays to compare against` |
|       - | 4994 | ` * Return` |
|       - | 4995 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4996 | ` *  in all the arguments, with matching keys.` |
|       - | 4997 | ` */` |
|      22 | 4998 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4999 | `{` |
|       - | 5000 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 5001 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5002 | `	ph7_value *pArray;` |
|       - | 5003 | `	ph7_value *pVal;` |
|       - | 5004 | `	sxi32 rc;` |
|       - | 5005 | `	sxu32 n;` |
|       - | 5006 | `	int i;` |
|      26 | 5007 | `	if( nArg < 1 ){` |
|       4 | 5008 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5009 | `			"ArgumentCountError",` |
|       - | 5010 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 5011 | `			nArg` |
|       - | 5012 | `			);` |
|       - | 5013 | `	}` |
|      23 | 5014 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5015 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5016 | `			"TypeError",` |
|       - | 5017 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5018 | `			ph7_type_name(apArg[0])` |
|       - | 5019 | `			);` |
|       - | 5020 | `	}` |
|      36 | 5021 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5022 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5023 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5024 | `				"TypeError",` |
|       - | 5025 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 5026 | `				i + 1,` |
|       2 | 5027 | `				ph7_type_name(apArg[i])` |
|       - | 5028 | `				);` |
|       - | 5029 | `		}` |
|       9 | 5030 | `	}` |
|      17 | 5031 | `	if( nArg == 1 ){` |
|       - | 5032 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5033 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5034 | `		return PH7_OK;` |
|       - | 5035 | `	}` |
|       - | 5036 | `	/* Create a new array */` |
|      15 | 5037 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5038 | `	if( pArray == 0 ){` |
|     ! 0 | 5039 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5040 | `		return PH7_OK;` |
|       - | 5041 | `	}` |
|       - | 5042 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 5043 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5044 | `	/* Perform the intersection */` |
|      15 | 5045 | `	pEntry = pSrc->pFirst;` |
|      15 | 5046 | `	n = pSrc->nEntry;` |
|      15 | 5047 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 5048 | `	for(;;){` |
|      47 | 5049 | `		if( n < 1 ){` |
|      15 | 5050 | `			break;` |
|       - | 5051 | `		}` |
|       - | 5052 | `		/* Extract the node value */` |
|      33 | 5053 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 5054 | `		if( pVal ){` |
|      53 | 5055 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 5056 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 5057 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5058 | `				/* Perform a key lookup first */` |
|      37 | 5059 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 5060 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 5061 | `				}else{` |
|      23 | 5062 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 5063 | `				}` |
|      37 | 5064 | `				if( rc != SXRET_OK ){` |
|       - | 5065 | `					/* No such key,break immediately */` |
|       7 | 5066 | `					break;` |
|       - | 5067 | `				}` |
|       - | 5068 | `				/* Perform the lookup */` |
|      31 | 5069 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 5070 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 5071 | `					/* Value does not exist */` |
|       6 | 5072 | `					break;` |
|       - | 5073 | `				}` |
|      11 | 5074 | `			}` |
|      33 | 5075 | `			if( i >= nArg ){` |
|       - | 5076 | `				/* Perform the insertion */` |
|      17 | 5077 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5078 | `			}` |
|      16 | 5079 | `		}` |
|       - | 5080 | `		/* Point to the next entry */` |
|      33 | 5081 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5082 | `		n--;` |
|       1 | 5083 | `	}` |
|       - | 5084 | `	/* Return the freshly created array */` |
|      15 | 5085 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5086 | `	return PH7_OK;` |
|      15 | 5087 | `}` |
|       - | 5088 | `/*` |
|       - | 5089 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 5090 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 5091 | ` * Parameters` |
|       - | 5092 | ` *  $array1` |
|       - | 5093 | ` *    The array to compare from` |
|       - | 5094 | ` *  $...` |
|       - | 5095 | ` *   More arrays to compare against` |
|       - | 5096 | ` * Return` |
|       - | 5097 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 5098 | ` *  have keys that are present in all arguments.` |
|       - | 5099 | ` * Note that NULL is returned on failure.` |
|       - | 5100 | ` */` |
|      22 | 5101 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5102 | `{` |
|       - | 5103 | `	ph7_hashmap_node *pEntry;` |
|       - | 5104 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5105 | `	ph7_value *pArray;` |
|       - | 5106 | `	sxi32 rc;` |
|       - | 5107 | `	sxu32 n;` |
|       - | 5108 | `	int i;` |
|      26 | 5109 | `	if( nArg < 1 ){` |
|       4 | 5110 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5111 | `			"ArgumentCountError",` |
|       - | 5112 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 5113 | `			nArg` |
|       - | 5114 | `			);` |
|       - | 5115 | `	}` |
|      23 | 5116 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5117 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5118 | `			"TypeError",` |
|       - | 5119 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5120 | `			ph7_type_name(apArg[0])` |
|       - | 5121 | `			);` |
|       - | 5122 | `	}` |
|      36 | 5123 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5124 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5125 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5126 | `				"TypeError",` |
|       - | 5127 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5128 | `				i + 1,` |
|       2 | 5129 | `				ph7_type_name(apArg[i])` |
|       - | 5130 | `				);` |
|       - | 5131 | `		}` |
|       9 | 5132 | `	}` |
|      17 | 5133 | `	if( nArg == 1 ){` |
|       - | 5134 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5135 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5136 | `		return PH7_OK;` |
|       - | 5137 | `	}` |
|       - | 5138 | `	/* Create a new array */` |
|      15 | 5139 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5140 | `	if( pArray == 0 ){` |
|     ! 0 | 5141 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5142 | `		return PH7_OK;` |
|       - | 5143 | `	}` |
|       - | 5144 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 5145 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5146 | `	/* Perform the intersection */` |
|      15 | 5147 | `	pEntry = pSrc->pFirst;` |
|      15 | 5148 | `	n = pSrc->nEntry;` |
|      24 | 5149 | `	for(;;){` |
|      49 | 5150 | `		if( n < 1 ){` |
|      15 | 5151 | `			break;` |
|       - | 5152 | `		}` |
|      57 | 5153 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 5154 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 5155 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 5156 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5157 | `				/* Blob lookup */` |
|      27 | 5158 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 5159 | `			}else{` |
|       - | 5160 | `				/* Int key */` |
|      13 | 5161 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5162 | `			}` |
|      39 | 5163 | `			if( rc != SXRET_OK ){` |
|       - | 5164 | `				/* Key does not exist, break immediately */` |
|      17 | 5165 | `				break;` |
|       - | 5166 | `			}` |
|      12 | 5167 | `		}` |
|      35 | 5168 | `		if( i >= nArg ){` |
|       - | 5169 | `			/* Perform the insertion */` |
|      19 | 5170 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 5171 | `		}` |
|       - | 5172 | `		/* Point to the next entry */` |
|      35 | 5173 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 5174 | `		n--;` |
|       1 | 5175 | `	}` |
|       - | 5176 | `	/* Return the freshly created array */` |
|      15 | 5177 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5178 | `	return PH7_OK;` |
|      15 | 5179 | `}` |
|       - | 5180 | `/*` |
|       - | 5181 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 5182 | ` *  Computes the intersection of arrays.` |
|       - | 5183 | ` * Parameters` |
|       - | 5184 | ` *  $array1` |
|       - | 5185 | ` *    The array to compare from` |
|       - | 5186 | ` *  $array2` |
|       - | 5187 | ` *    An array to compare against` |
|       - | 5188 | ` *  $...` |
|       - | 5189 | ` *   More arrays to compare against` |
|       - | 5190 | ` * $callback` |
|       - | 5191 | ` *  The callback comparison function.` |
|       - | 5192 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5193 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5194 | ` *  than the second.` |
|       - | 5195 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5196 | ` * Return` |
|       - | 5197 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5198 | ` *  in all of the parameters. .` |
|       - | 5199 | ` * Note that NULL is returned on failure.` |
|       - | 5200 | ` */` |
|      26 | 5201 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5202 | `{` |
|       - | 5203 | `	ph7_hashmap_node *pEntry;` |
|       - | 5204 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5205 | `	ph7_value *pCallback;` |
|       - | 5206 | `	ph7_value *pArray;` |
|       - | 5207 | `	ph7_value *pVal;` |
|       - | 5208 | `	sxi32 rc;` |
|       - | 5209 | `	sxu32 n;` |
|       - | 5210 | `	int i;` |
|       - | 5211 |  |
|       - | 5212 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      31 | 5213 | `	if( nArg < 2 ){` |
|       4 | 5214 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5215 | `			"ArgumentCountError",` |
|       - | 5216 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5217 | `			nArg` |
|       - | 5218 | `			);` |
|       - | 5219 | `	}` |
|      29 | 5220 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5221 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5222 | `			"TypeError",` |
|       - | 5223 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5224 | `			ph7_type_name(apArg[0])` |
|       - | 5225 | `			);` |
|       - | 5226 | `	}` |
|       - | 5227 |  |
|      27 | 5228 | `	if( nArg == 2 ){` |
|       - | 5229 | `		/* Only the original array and the callback were provided. */` |
|       - | 5230 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5231 | `		 * validation ordering. */` |
|       3 | 5232 | `	} else {` |
|       - | 5233 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      39 | 5234 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      23 | 5235 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5236 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5237 | `					"TypeError",` |
|       - | 5238 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5239 | `					i + 1,` |
|       2 | 5240 | `					ph7_type_name(apArg[i])` |
|       - | 5241 | `					);` |
|       - | 5242 | `			}` |
|      13 | 5243 | `		}` |
|       - | 5244 | `	}` |
|       - | 5245 |  |
|       - | 5246 | `	/* Identify the callback (always expected as the last argument). */` |
|      25 | 5247 | `	pCallback = apArg[nArg - 1];` |
|       - | 5248 | `	/* Validate the callback to match PHP's error messages. */` |
|      25 | 5249 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      14 | 5250 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5251 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5252 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5253 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5254 | `			 */` |
|       9 | 5255 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       9 | 5256 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5257 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5258 | `					"TypeError",` |
|       - | 5259 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5260 | `					nArg` |
|       - | 5261 | `					);` |
|       - | 5262 | `			}` |
|       - | 5263 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5264 | `			{` |
|       6 | 5265 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       6 | 5266 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       6 | 5267 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5268 | `					int nMethodLen;` |
|       6 | 5269 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       6 | 5270 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       6 | 5271 | `					if( pClass ){` |
|       - | 5272 | `						/* Class exists but method is missing. */` |
|       4 | 5273 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5274 | `							"TypeError",` |
|       - | 5275 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5276 | `							nArg,` |
|       1 | 5277 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5278 | `							zMethod` |
|       - | 5279 | `							);` |
|       - | 5280 | `					}` |
|       - | 5281 | `					/* Class not found */` |
|       - | 5282 | `					{` |
|       - | 5283 | `						int nName;` |
|       3 | 5284 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5285 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5286 | `							"TypeError",` |
|       - | 5287 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5288 | `							nArg,` |
|       1 | 5289 | `							zName` |
|       - | 5290 | `							);` |
|       - | 5291 | `					}` |
|       - | 5292 | `				}` |
|       - | 5293 | `			}` |
|       - | 5294 | `			/* Fallback message */` |
|     ! 0 | 5295 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5296 | `				"TypeError",` |
|       - | 5297 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5298 | `				nArg` |
|       - | 5299 | `				);` |
|       - | 5300 | `		}` |
|       6 | 5301 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5302 | `			int len;` |
|       3 | 5303 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5304 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5305 | `				"TypeError",` |
|       - | 5306 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5307 | `				nArg,` |
|       1 | 5308 | `				zName` |
|       - | 5309 | `				);` |
|       - | 5310 | `		}` |
|       4 | 5311 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5312 | `			"TypeError",` |
|       - | 5313 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5314 | `			nArg` |
|       - | 5315 | `			);` |
|       - | 5316 | `	}` |
|       - | 5317 |  |
|      11 | 5318 | `	if( nArg == 2 ){` |
|       - | 5319 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5320 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5321 | `		return PH7_OK;` |
|       - | 5322 | `	}` |
|       - | 5323 |  |
|       - | 5324 | `	/* Create a new array */` |
|       7 | 5325 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5326 | `	if( pArray == 0 ){` |
|     ! 0 | 5327 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5328 | `		return PH7_OK;` |
|       - | 5329 | `	}` |
|       - | 5330 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 5331 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5332 | `	/* Perform the intersection */` |
|       7 | 5333 | `	pEntry = pSrc->pFirst;` |
|       7 | 5334 | `	n = pSrc->nEntry;` |
|       7 | 5335 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 5336 | `	for(;;){` |
|      19 | 5337 | `		if( n < 1 ){` |
|       5 | 5338 | `			break;` |
|       - | 5339 | `		}` |
|       - | 5340 | `		/* Extract the node value */` |
|      15 | 5341 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5342 | `		if( pVal ){` |
|      23 | 5343 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 5344 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5345 | `					/* ignore */` |
|     ! 0 | 5346 | `					continue;` |
|       - | 5347 | `				}` |
|       - | 5348 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 5349 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5350 | `				/* Perform the lookup */` |
|      15 | 5351 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 5352 | `				if( rc != SXRET_OK ){` |
|       - | 5353 | `					/* Value does not exist */` |
|       7 | 5354 | `					break;` |
|       - | 5355 | `				}` |
|       5 | 5356 | `			}` |
|      15 | 5357 | `			if( i >= (nArg-1) ){` |
|       - | 5358 | `				/* Perform the insertion */` |
|       9 | 5359 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5360 | `			}` |
|       7 | 5361 | `		}` |
|      15 | 5362 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 5363 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5364 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 5365 | `			return PH7_EXCEPTION;` |
|       - | 5366 | `		}` |
|       - | 5367 | `		/* Point to the next entry */` |
|      13 | 5368 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5369 | `		n--;` |
|       1 | 5370 | `	}` |
|       - | 5371 | `	/* Return the freshly created array */` |
|       5 | 5372 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5373 | `	return PH7_OK;` |
|      18 | 5374 | `}` |
|       - | 5375 | `/*` |
|       - | 5376 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5377 | ` *  Fill an array with values.` |
|       - | 5378 | ` * Parameters` |
|       - | 5379 | ` *  $start_index` |
|       - | 5380 | ` *    The first index of the returned array.` |
|       - | 5381 | ` *  $num` |
|       - | 5382 | ` *   Number of elements to insert.` |
|       - | 5383 | ` *  $value` |
|       - | 5384 | ` *    Value to use for filling.` |
|       - | 5385 | ` * Return` |
|       - | 5386 | ` *  The filled array or null on failure.` |
|       - | 5387 | ` */` |
|     238 | 5388 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5389 | `{` |
|       - | 5390 | `	ph7_value *pArray;` |
|       - | 5391 | `	int i,nEntry;` |
|       - | 5392 |  |
|       - | 5393 | `	/* PHP enforces argument count and type checks. */` |
|     243 | 5394 | `	if( nArg != 3 ){` |
|       - | 5395 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       8 | 5396 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5397 | `			"ArgumentCountError",` |
|       - | 5398 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5399 | `			nArg` |
|       - | 5400 | `			);` |
|       - | 5401 | `	}` |
|       - | 5402 |  |
|       - | 5403 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5404 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5405 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5406 | `	 * and NULLs are rejected outright. */` |
|     466 | 5407 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     353 | 5408 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5409 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5410 | `			"TypeError",` |
|       - | 5411 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5412 | `			ph7_type_name(apArg[0])` |
|       - | 5413 | `			);` |
|       - | 5414 | `	}` |
|     236 | 5415 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5416 | `		int len;` |
|       8 | 5417 | `		sxu8 bReal = FALSE;` |
|       8 | 5418 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5419 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5420 | `			/* Non‑numeric string is an error. */` |
|       3 | 5421 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5422 | `				"TypeError",` |
|       - | 5423 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5424 | `				);` |
|       - | 5425 | `		}` |
|       5 | 5426 | `		if( bReal ){` |
|       - | 5427 | `			/* float-string -> deprecation warning */` |
|       4 | 5428 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5429 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5430 | `				zStr` |
|       - | 5431 | `				);` |
|       1 | 5432 | `		}` |
|       2 | 5433 | `	}` |
|       - | 5434 |  |
|       - | 5435 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5436 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 5437 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     348 | 5438 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5439 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5440 | `			"TypeError",` |
|       - | 5441 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5442 | `			ph7_type_name(apArg[1])` |
|       - | 5443 | `			);` |
|       - | 5444 | `	}` |
|     233 | 5445 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5446 | `		int len;` |
|       3 | 5447 | `		sxu8 bReal = FALSE;` |
|       3 | 5448 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5449 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5450 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5451 | `				"TypeError",` |
|       - | 5452 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5453 | `				);` |
|       - | 5454 | `		}` |
|     ! 0 | 5455 | `	}` |
|       - | 5456 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5457 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5458 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5459 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5460 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5461 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5462 | `		if( d != (double)i64 ){` |
|       7 | 5463 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5464 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5465 | `				d` |
|       - | 5466 | `				);` |
|       2 | 5467 | `		}` |
|       2 | 5468 | `	}` |
|       - | 5469 |  |
|       - | 5470 | `	/* Total number of entries to insert */` |
|     230 | 5471 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5472 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5473 | `	if( nEntry < 0 ){` |
|       3 | 5474 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5475 | `			"ValueError",` |
|       - | 5476 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5477 | `			);` |
|       - | 5478 | `	}` |
|       - | 5479 |  |
|       - | 5480 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5481 | `	if( nEntry == 0 ){` |
|       7 | 5482 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5483 | `		return PH7_OK;` |
|       - | 5484 | `	}` |
|       - | 5485 |  |
|       - | 5486 | `	/* Create a new array */` |
|     221 | 5487 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5488 | `	if( pArray == 0 ){` |
|     ! 0 | 5489 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5490 | `	}` |
|       - | 5491 |  |
|       - | 5492 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5493 | `	if( ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]) != SXRET_OK ){` |
|     ! 0 | 5494 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5495 | `	}` |
|       - | 5496 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5497 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5498 | `		if( ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]) != SXRET_OK ){` |
|       - | 5499 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 5500 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 5501 | `		}` |
| 1058682 | 5502 | `	}` |
|       - | 5503 | `	/* Return the filled array */` |
|     221 | 5504 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5505 | `	return PH7_OK;` |
|     124 | 5506 | `}` |
|       - | 5507 | `/*` |
|       - | 5508 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5509 | ` *  Fill an array with values, specifying keys.` |
|       - | 5510 | ` * Parameters` |
|       - | 5511 | ` *  $input` |
|       - | 5512 | ` *   Array of values that will be used as key.` |
|       - | 5513 | ` *  $value` |
|       - | 5514 | ` *    Value to use for filling.` |
|       - | 5515 | ` * Return` |
|       - | 5516 | ` *  The filled array.` |
|       - | 5517 | ` * Throws` |
|       - | 5518 | ` *  ValueError if $input is not an array.` |
|       - | 5519 | ` */` |
|      26 | 5520 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5521 | `{` |
|       - | 5522 | `	ph7_hashmap_node *pEntry;` |
|       - | 5523 | `	ph7_hashmap *pSrc;` |
|       - | 5524 | `	ph7_value *pArray;` |
|       - | 5525 | `	sxu32 n;` |
|       - | 5526 | `	/* PHP enforces exactly 2 arguments. */` |
|      31 | 5527 | `	if( nArg != 2 ){` |
|      12 | 5528 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5529 | `			"ArgumentCountError",` |
|       - | 5530 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5531 | `			nArg` |
|       - | 5532 | `			);` |
|       - | 5533 | `	}` |
|       - | 5534 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 5535 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 5536 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5537 | `			"TypeError",` |
|       - | 5538 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5539 | `			ph7_type_name(apArg[0])` |
|       - | 5540 | `			);` |
|       - | 5541 | `	}` |
|       - | 5542 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5543 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5544 | `	/* Create a new array */` |
|      17 | 5545 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5546 | `	if( pArray == 0 ){` |
|     ! 0 | 5547 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5548 | `		return PH7_OK;` |
|       - | 5549 | `	}` |
|       - | 5550 | `	/* Perform the requested operation */` |
|      17 | 5551 | `	pEntry = pSrc->pFirst;` |
|      45 | 5552 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5553 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5554 | `		/* Point to the next entry */` |
|      29 | 5555 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5556 | `	}` |
|       - | 5557 | `	/* Return the filled array */` |
|      17 | 5558 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5559 | `	return PH7_OK;` |
|      18 | 5560 | `}` |
|       - | 5561 | `/*` |
|       - | 5562 | ` * array array_combine(array $keys,array $values)` |
|       - | 5563 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5564 | ` * Parameters` |
|       - | 5565 | ` *  $keys` |
|       - | 5566 | ` *    Array of keys to be used.` |
|       - | 5567 | ` * $values` |
|       - | 5568 | ` *   Array of values to be used.` |
|       - | 5569 | ` * Return` |
|       - | 5570 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5571 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5572 | ` *  not an array.` |
|       - | 5573 | ` */` |
|      18 | 5574 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5575 | `{` |
|       - | 5576 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5577 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5578 | `	ph7_value *pArray;` |
|       - | 5579 | `	sxu32 n;` |
|       - | 5580 | `	/* PHP enforces argument count and type checks. */` |
|      23 | 5581 | `	if( nArg != 2 ){` |
|       - | 5582 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5583 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5584 | `			"ArgumentCountError",` |
|       - | 5585 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5586 | `			nArg` |
|       - | 5587 | `			);` |
|       - | 5588 | `	}` |
|       - | 5589 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5590 | `	 * argument index in the error message. */` |
|      20 | 5591 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5592 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5593 | `			"TypeError",` |
|       - | 5594 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5595 | `			ph7_type_name(apArg[0])` |
|       - | 5596 | `			);` |
|       - | 5597 | `	}` |
|      17 | 5598 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5599 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5600 | `			"TypeError",` |
|       - | 5601 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5602 | `			ph7_type_name(apArg[1])` |
|       - | 5603 | `			);` |
|       - | 5604 | `	}` |
|       - | 5605 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5606 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5607 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5608 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5609 | `		/* Length mismatch -> ValueError */` |
|       3 | 5610 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5611 | `			"ValueError",` |
|       - | 5612 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5613 | `			);` |
|       - | 5614 | `	}` |
|       - | 5615 | `	/* Create a new array */` |
|      11 | 5616 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5617 | `	if( pArray == 0 ){` |
|     ! 0 | 5618 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5619 | `		return PH7_OK;` |
|       - | 5620 | `	}` |
|       - | 5621 | `	/* Perform the requested operation */` |
|      11 | 5622 | `	pKe = pKey->pFirst;` |
|      11 | 5623 | `	pVe = pValue->pFirst;` |
|      33 | 5624 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5625 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5626 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5627 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5628 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5629 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5630 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5631 | `		 * original array must not be mutated. */` |
|      23 | 5632 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5633 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5634 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5635 | `			if( pTmpKey ){` |
|       5 | 5636 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5637 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5638 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5639 | `				pKeyCopy = pTmpKey;` |
|       2 | 5640 | `			}` |
|       2 | 5641 | `		}` |
|      23 | 5642 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5643 | `		/* Point to the next entry */` |
|      23 | 5644 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5645 | `		pVe = pVe->pPrev;` |
|      12 | 5646 | `	}` |
|       - | 5647 | `	/* Return the filled array */` |
|      11 | 5648 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5649 | `	return PH7_OK;` |
|      14 | 5650 | `}` |
|       - | 5651 | `/*` |
|       - | 5652 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5653 | ` *  Return an array with elements in reverse order.` |
|       - | 5654 | ` * Parameters` |
|       - | 5655 | ` *  $array` |
|       - | 5656 | ` *   The input array.` |
|       - | 5657 | ` *  $preserve_keys (optional)` |
|       - | 5658 | ` *   If set to TRUE keys are preserved.` |
|       - | 5659 | ` * Return` |
|       - | 5660 | ` *  The reversed array.` |
|       - | 5661 | ` */` |
|      20 | 5662 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 5663 | `{` |
|       - | 5664 | `	ph7_hashmap_node *pEntry;` |
|       - | 5665 | `	ph7_hashmap *pSrc;` |
|       - | 5666 | `	ph7_value *pArray;` |
|       - | 5667 | `	int bPreserve;` |
|       - | 5668 | `	sxu32 n;` |
|      23 | 5669 | `	if( nArg < 1 ){` |
|       4 | 5670 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5671 | `			"ArgumentCountError",` |
|       - | 5672 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5673 | `			nArg` |
|       - | 5674 | `			);` |
|       - | 5675 | `	}` |
|       - | 5676 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5677 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5678 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5679 | `			"TypeError",` |
|       - | 5680 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5681 | `			ph7_type_name(apArg[0])` |
|       - | 5682 | `			);` |
|       - | 5683 | `	}` |
|      17 | 5684 | `	bPreserve = FALSE;` |
|      17 | 5685 | `	if( nArg > 1 ){` |
|       7 | 5686 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5687 | `	}` |
|       - | 5688 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5689 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5690 | `	/* Create a new array */` |
|      17 | 5691 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5692 | `	if( pArray == 0 ){` |
|     ! 0 | 5693 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5694 | `		return PH7_OK;` |
|       - | 5695 | `	}` |
|       - | 5696 | `	/* Perform the requested operation */` |
|      17 | 5697 | `	pEntry = pSrc->pLast;` |
|      55 | 5698 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5699 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5700 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5701 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5702 | `		/* Point to the previous entry */` |
|      39 | 5703 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5704 | `	}` |
|      17 | 5705 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5706 | `	return PH7_OK;` |
|      13 | 5707 | `}` |
|       - | 5708 | `/*` |
|       - | 5709 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5710 | ` *  Removes duplicate values from an array.` |
|       - | 5711 | ` * Parameters` |
|       - | 5712 | ` *  $array` |
|       - | 5713 | ` *   The input array.` |
|       - | 5714 | ` *  $flags` |
|       - | 5715 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5716 | ` *   behavior using these values:` |
|       - | 5717 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5718 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5719 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5720 | ` * Return` |
|       - | 5721 | ` *  The filtered array.` |
|       - | 5722 | ` */` |
|      24 | 5723 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5724 | `{` |
|       - | 5725 | `	ph7_hashmap_node *pEntry;` |
|       - | 5726 | `	ph7_value *pNeedle;` |
|       - | 5727 | `	ph7_hashmap *pSrc;` |
|       - | 5728 | `	ph7_value *pArray;` |
|       - | 5729 | `	int bStrict;` |
|       - | 5730 | `	sxi32 rc;` |
|       - | 5731 | `	sxu32 n;` |
|      28 | 5732 | `	if( nArg < 1 ){` |
|       - | 5733 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5734 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5735 | `			"ArgumentCountError",` |
|       - | 5736 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5737 | `			);` |
|       - | 5738 | `	}` |
|      25 | 5739 | `	if( nArg > 2 ){` |
|       - | 5740 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5741 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5742 | `			"ArgumentCountError",` |
|       - | 5743 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5744 | `			nArg` |
|       - | 5745 | `			);` |
|       - | 5746 | `	}` |
|       - | 5747 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5748 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5749 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5750 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5751 | `			"TypeError",` |
|       - | 5752 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5753 | `			ph7_type_name(apArg[0])` |
|       - | 5754 | `			);` |
|       - | 5755 | `	}` |
|      19 | 5756 | `	bStrict = FALSE;` |
|       - | 5757 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5758 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5759 | `	/* Create a new array */` |
|      19 | 5760 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5761 | `	if( pArray == 0 ){` |
|     ! 0 | 5762 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5763 | `		return PH7_OK;` |
|       - | 5764 | `	}` |
|       - | 5765 | `	/* Perform the requested operation */` |
|      19 | 5766 | `	pEntry = pSrc->pFirst;` |
|      83 | 5767 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5768 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5769 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5770 | `		if( pNeedle ){` |
|      65 | 5771 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5772 | `		}` |
|      65 | 5773 | `		if( rc != SXRET_OK ){` |
|       - | 5774 | `			/* Perform the insertion */` |
|      37 | 5775 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5776 | `		}` |
|       - | 5777 | `		/* Point to the next entry */` |
|      65 | 5778 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5779 | `	}` |
|       - | 5780 | `	/* Return the freshly created array */` |
|      19 | 5781 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5782 | `	return PH7_OK;` |
|      16 | 5783 | `}` |
|       - | 5784 | `/*` |
|       - | 5785 | ` * array array_flip(array $input)` |
|       - | 5786 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5787 | ` * Parameter` |
|       - | 5788 | ` *  $input` |
|       - | 5789 | ` *   Input array.` |
|       - | 5790 | ` * Return` |
|       - | 5791 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5792 | ` */` |
|      34 | 5793 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5794 | `{` |
|       - | 5795 | `	ph7_hashmap_node *pEntry;` |
|       - | 5796 | `	ph7_hashmap *pSrc;` |
|       - | 5797 | `	ph7_value *pArray;` |
|       - | 5798 | `	ph7_value *pKey;` |
|       - | 5799 | `	ph7_value sVal;` |
|       - | 5800 | `	sxu32 n;` |
|       - | 5801 |  |
|       - | 5802 | `	/* PHP requires exactly one argument */` |
|      39 | 5803 | `	if( nArg != 1 ){` |
|       - | 5804 | `		/* Use ArgumentCountError like other array helpers */` |
|       8 | 5805 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5806 | `			"ArgumentCountError",` |
|       - | 5807 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5808 | `			nArg` |
|       - | 5809 | `			);` |
|       - | 5810 | `	}` |
|       - | 5811 | `	/* Make sure we are dealing with a valid hashmap */` |
|      33 | 5812 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5813 | `		/* Type mismatch -> TypeError */` |
|       8 | 5814 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5815 | `			"TypeError",` |
|       - | 5816 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5817 | `			ph7_type_name(apArg[0])` |
|       - | 5818 | `			);` |
|       - | 5819 | `	}` |
|       - | 5820 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5821 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5822 | `	/* Create a new array */` |
|      27 | 5823 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5824 | `	if( pArray == 0 ){` |
|     ! 0 | 5825 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5826 | `		return PH7_OK;` |
|       - | 5827 | `	}` |
|       - | 5828 | `	/* Start processing */` |
|      27 | 5829 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5830 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5831 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5832 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5833 | `		if( pKey ){` |
|       - | 5834 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5835 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5836 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5837 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5838 | `					);` |
|   22236 | 5839 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5840 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5841 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5842 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5843 | `				}else{` |
|       - | 5844 | `					SyString sStr;` |
|    2227 | 5845 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5846 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5847 | `				}` |
|       - | 5848 | `				/* Perform the insertion */` |
|   22227 | 5849 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5850 | `				/* Safely release the value because each inserted entry` |
|       - | 5851 | `				 * has its own private copy of the value.` |
|       - | 5852 | `				 */` |
|   22227 | 5853 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5854 | `			}else{` |
|       - | 5855 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5856 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5857 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5858 | `					);` |
|       - | 5859 | `			}` |
|   11118 | 5860 | `		}` |
|       - | 5861 | `		/* Point to the next entry */` |
|   22237 | 5862 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5863 | `	}` |
|       - | 5864 | `	/* Return the freshly created array */` |
|      27 | 5865 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5866 | `	return PH7_OK;` |
|      22 | 5867 | `}` |
|       - | 5868 | `/*` |
|       - | 5869 | ` * number array_sum(array $array )` |
|       - | 5870 | ` *  Calculate the sum of values in an array.` |
|       - | 5871 | ` * Parameters` |
|       - | 5872 | ` *  $array: The input array.` |
|       - | 5873 | ` * Return` |
|       - | 5874 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5875 | ` */` |
|      24 | 5876 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5877 | `{` |
|       - | 5878 | `	ph7_hashmap_node *pEntry;` |
|       - | 5879 | `	ph7_value *pObj;` |
|      25 | 5880 | `	double dSum = 0;` |
|       - | 5881 | `	sxu32 n;` |
|      25 | 5882 | `	pEntry = pMap->pFirst;` |
|      91 | 5883 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5884 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5885 | `		if( pObj ){` |
|      67 | 5886 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5887 | `				dSum += pObj->rVal;` |
|      53 | 5888 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5889 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5890 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5891 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5892 | `					double dv = 0;` |
|      13 | 5893 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5894 | `					dSum += dv;` |
|       7 | 5895 | `				}` |
|      12 | 5896 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5897 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5898 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5899 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5900 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5901 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5902 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5903 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5904 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5905 | `			}` |
|       - | 5906 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5907 | `		}` |
|       - | 5908 | `		/* Point to the next entry */` |
|      67 | 5909 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5910 | `	}` |
|       - | 5911 | `	/* Return sum */` |
|      25 | 5912 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5913 | `}` |
|      30 | 5914 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5915 | `{` |
|       - | 5916 | `	ph7_hashmap_node *pEntry;` |
|       - | 5917 | `	ph7_value *pObj;` |
|      32 | 5918 | `	sxi64 nSum = 0;` |
|       - | 5919 | `	sxu32 n;` |
|      32 | 5920 | `	pEntry = pMap->pFirst;` |
|     128 | 5921 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      98 | 5922 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      98 | 5923 | `		if( pObj ){` |
|      98 | 5924 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      88 | 5925 | `				nSum += pObj->x.iVal;` |
|      54 | 5926 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5927 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5928 | `					sxi64 nv = 0;` |
|       5 | 5929 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5930 | `					nSum += nv;` |
|       3 | 5931 | `				}` |
|       8 | 5932 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5933 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5934 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5935 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5936 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5937 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5938 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5939 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5940 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5941 | `			}` |
|       - | 5942 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      48 | 5943 | `		}` |
|       - | 5944 | `		/* Point to the next entry */` |
|      98 | 5945 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      50 | 5946 | `	}` |
|       - | 5947 | `	/* Return sum */` |
|      32 | 5948 | `	ph7_result_int64(pCtx,nSum);` |
|      32 | 5949 | `}` |
|       - | 5950 | `/* number array_sum(array $array )` |
|       - | 5951 | ` * (See block-coment above)` |
|       - | 5952 | ` */` |
|      68 | 5953 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5954 | `{` |
|       - | 5955 | `	ph7_hashmap_node *pEntry;` |
|       - | 5956 | `	ph7_hashmap *pMap;` |
|       - | 5957 | `	ph7_value *pObj;` |
|      73 | 5958 | `	int useDouble = 0;` |
|       - | 5959 | `	sxu32 n;` |
|       - | 5960 | `	/* PHP requires exactly one argument */` |
|      73 | 5961 | `	if( nArg != 1 ){` |
|       8 | 5962 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5963 | `			"ArgumentCountError",` |
|       - | 5964 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5965 | `			nArg` |
|       - | 5966 | `			);` |
|       - | 5967 | `	}` |
|       - | 5968 | `	/* Make sure we are dealing with a valid hashmap */` |
|      68 | 5969 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5970 | `		/* Type mismatch -> TypeError */` |
|       8 | 5971 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5972 | `			"TypeError",` |
|       - | 5973 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5974 | `			ph7_type_name(apArg[0])` |
|       - | 5975 | `			);` |
|       - | 5976 | `	}` |
|      62 | 5977 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      62 | 5978 | `	if( pMap->nEntry < 1 ){` |
|       - | 5979 | `		/* Nothing to compute,return 0 */` |
|       7 | 5980 | `		ph7_result_int(pCtx,0);` |
|       7 | 5981 | `		return PH7_OK;` |
|       - | 5982 | `	}` |
|       - | 5983 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5984 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5985 | `	 */` |
|      56 | 5986 | `	pEntry = pMap->pFirst;` |
|     160 | 5987 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     130 | 5988 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     130 | 5989 | `		if( pObj ){` |
|     130 | 5990 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5991 | `				useDouble = 1;` |
|      19 | 5992 | `				break;` |
|       - | 5993 | `			}` |
|     112 | 5994 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5995 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5996 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5997 | `				sxu32 i;` |
|      23 | 5998 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5999 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 6000 | `						useDouble = 1;` |
|       7 | 6001 | `						break;` |
|       - | 6002 | `					}` |
|       6 | 6003 | `				}` |
|      13 | 6004 | `				if( useDouble ){` |
|       7 | 6005 | `					break;` |
|       - | 6006 | `				}` |
|       3 | 6007 | `			}` |
|      52 | 6008 | `		}` |
|     106 | 6009 | `		pEntry = pEntry->pPrev;` |
|      54 | 6010 | `	}` |
|      56 | 6011 | `	if( useDouble ){` |
|      25 | 6012 | `		DoubleSum(pCtx,pMap);` |
|      13 | 6013 | `	}else{` |
|      32 | 6014 | `		Int64Sum(pCtx,pMap);` |
|       - | 6015 | `	}` |
|      56 | 6016 | `	return PH7_OK;` |
|      39 | 6017 | `}` |
|       - | 6018 | `/*` |
|       - | 6019 | ` * number array_product(array $array )` |
|       - | 6020 | ` *  Calculate the product of values in an array.` |
|       - | 6021 | ` * Parameters` |
|       - | 6022 | ` *  $array: The input array.` |
|       - | 6023 | ` * Return` |
|       - | 6024 | ` *  Returns the product of values as an integer or float.` |
|       - | 6025 | ` */` |
|     ! 0 | 6026 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 6027 | `{` |
|       - | 6028 | `	ph7_hashmap_node *pEntry;` |
|       - | 6029 | `	ph7_value *pObj;` |
|       - | 6030 | `	double dProd;` |
|       - | 6031 | `	sxu32 n;` |
|     ! 0 | 6032 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6033 | `	dProd = 1;` |
|     ! 0 | 6034 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6035 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6036 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6037 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6038 | `				dProd *= pObj->rVal;` |
|     ! 0 | 6039 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6040 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 6041 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6042 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6043 | `					double dv = 0;` |
|     ! 0 | 6044 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 6045 | `					dProd *= dv;` |
|     ! 0 | 6046 | `				}` |
|     ! 0 | 6047 | `			}` |
|     ! 0 | 6048 | `		}` |
|       - | 6049 | `		/* Point to the next entry */` |
|     ! 0 | 6050 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6051 | `	}` |
|       - | 6052 | `	/* Return product */` |
|     ! 0 | 6053 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 6054 | `}` |
|     ! 0 | 6055 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 6056 | `{` |
|       - | 6057 | `	ph7_hashmap_node *pEntry;` |
|       - | 6058 | `	ph7_value *pObj;` |
|       - | 6059 | `	sxi64 nProd;` |
|       - | 6060 | `	sxu32 n;` |
|     ! 0 | 6061 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6062 | `	nProd = 1;` |
|     ! 0 | 6063 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6064 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6065 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6066 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6067 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 6068 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6069 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 6070 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6071 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6072 | `					sxi64 nv = 0;` |
|     ! 0 | 6073 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 6074 | `					nProd *= nv;` |
|     ! 0 | 6075 | `				}` |
|     ! 0 | 6076 | `			}` |
|     ! 0 | 6077 | `		}` |
|       - | 6078 | `		/* Point to the next entry */` |
|     ! 0 | 6079 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6080 | `	}` |
|       - | 6081 | `	/* Return product */` |
|     ! 0 | 6082 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 6083 | `}` |
|       - | 6084 | `/* number array_product(array $array )` |
|       - | 6085 | ` * (See block-block comment above)` |
|       - | 6086 | ` */` |
|     ! 0 | 6087 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 6088 | `{` |
|       - | 6089 | `	ph7_hashmap *pMap;` |
|       - | 6090 | `	ph7_value *pObj;` |
|     ! 0 | 6091 | `	if( nArg < 1 ){` |
|       - | 6092 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 6093 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6094 | `		return PH7_OK;` |
|       - | 6095 | `	}` |
|       - | 6096 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 6097 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6098 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 6099 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6100 | `		return PH7_OK;` |
|       - | 6101 | `	}` |
|     ! 0 | 6102 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 6103 | `	if( pMap->nEntry < 1 ){` |
|       - | 6104 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 6105 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6106 | `		return PH7_OK;` |
|       - | 6107 | `	}` |
|       - | 6108 | `	/* If the first element is of type float,then perform floating` |
|       - | 6109 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 6110 | `	 */` |
|     ! 0 | 6111 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 6112 | `	if( pObj == 0 ){` |
|     ! 0 | 6113 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6114 | `		return PH7_OK;` |
|       - | 6115 | `	}` |
|     ! 0 | 6116 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6117 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 6118 | `	}else{` |
|     ! 0 | 6119 | `		Int64Prod(pCtx,pMap);` |
|       - | 6120 | `	}` |
|     ! 0 | 6121 | `	return PH7_OK;` |
|     ! 0 | 6122 | `}` |
|       - | 6123 | `/*` |
|       - | 6124 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 6125 | ` *  Pick one or more random entries out of an array.` |
|       - | 6126 | ` * Parameters` |
|       - | 6127 | ` * $input` |
|       - | 6128 | ` *  The input array.` |
|       - | 6129 | ` * $num_req` |
|       - | 6130 | ` *  Specifies how many entries you want to pick.` |
|       - | 6131 | ` * Return` |
|       - | 6132 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 6133 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 6134 | ` *  NULL is returned on failure.` |
|       - | 6135 | ` */` |
|       6 | 6136 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6137 | `{` |
|       - | 6138 | `	ph7_hashmap_node *pNode;` |
|       - | 6139 | `	ph7_hashmap *pMap;` |
|       7 | 6140 | `	int nItem = 1;` |
|       7 | 6141 | `	if( nArg < 1 ){` |
|       - | 6142 | `		/* Missing argument,return NULL */` |
|     ! 0 | 6143 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6144 | `		return PH7_OK;` |
|       - | 6145 | `	}` |
|       - | 6146 | `	/* Make sure we are dealing with an array */` |
|       7 | 6147 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6148 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6149 | `		return PH7_OK;` |
|       - | 6150 | `	}` |
|       - | 6151 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 6152 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 6153 | `	if(pMap->nEntry < 1 ){` |
|       - | 6154 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 6155 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6156 | `		return PH7_OK;` |
|       - | 6157 | `	}` |
|       7 | 6158 | `	if( nArg > 1 ){` |
|       3 | 6159 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 6160 | `	}` |
|       7 | 6161 | `	if( nItem < 2 ){` |
|       - | 6162 | `		sxu32 nEntry;` |
|       - | 6163 | `		/* Select a random number */` |
|       5 | 6164 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 6165 | `		/* Extract the desired entry.` |
|       - | 6166 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 6167 | `		 */` |
|       5 | 6168 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 6169 | `			pNode = pMap->pLast;` |
|       3 | 6170 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 6171 | `			if( nEntry > 1 ){` |
|     ! 0 | 6172 | `				for(;;){` |
|     ! 0 | 6173 | `					if( nEntry == 0 ){` |
|     ! 0 | 6174 | `						break;` |
|       - | 6175 | `					}` |
|       - | 6176 | `					/* Point to the previous entry */` |
|     ! 0 | 6177 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 6178 | `					nEntry--;` |
|     ! 0 | 6179 | `				}` |
|     ! 0 | 6180 | `			}` |
|       2 | 6181 | `		}else{` |
|       3 | 6182 | `			pNode = pMap->pFirst;` |
|       2 | 6183 | `			for(;;){` |
|       5 | 6184 | `				if( nEntry == 0 ){` |
|       3 | 6185 | `					break;` |
|       - | 6186 | `				}` |
|       - | 6187 | `				/* Point to the next entry */` |
|       3 | 6188 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       3 | 6189 | `				nEntry--;` |
|       1 | 6190 | `			}` |
|       - | 6191 | `		}` |
|       5 | 6192 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6193 | `			/* Int key */` |
|       3 | 6194 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6195 | `		}else{` |
|       - | 6196 | `			/* Blob key */` |
|       3 | 6197 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6198 | `		}` |
|       3 | 6199 | `	}else{` |
|       - | 6200 | `		ph7_value sKey,*pArray;` |
|       - | 6201 | `		ph7_hashmap *pDest;` |
|       - | 6202 | `		/* Create a new array */` |
|       3 | 6203 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6204 | `		if( pArray == 0 ){` |
|     ! 0 | 6205 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6206 | `			return PH7_OK;` |
|       - | 6207 | `		}` |
|       - | 6208 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6209 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6210 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6211 | `		/* Copy the first n items */` |
|       3 | 6212 | `		pNode = pMap->pFirst;` |
|       3 | 6213 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6214 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6215 | `		}` |
|       7 | 6216 | `		while( nItem > 0){` |
|       5 | 6217 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6218 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6219 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6220 | `			/* Point to the next entry */` |
|       5 | 6221 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6222 | `			nItem--;` |
|       1 | 6223 | `		}` |
|       - | 6224 | `		/* Shuffle the array */` |
|       3 | 6225 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6226 | `		/* Rehash node */` |
|       3 | 6227 | `		HashmapSortRehash(pDest);` |
|       - | 6228 | `		/* Return the random array */` |
|       3 | 6229 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6230 | `	}` |
|       7 | 6231 | `	return PH7_OK;` |
|       4 | 6232 | `}` |
|       - | 6233 | `/*` |
|       - | 6234 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6235 | ` *  Split an array into chunks.` |
|       - | 6236 | ` * Parameters` |
|       - | 6237 | ` * $input` |
|       - | 6238 | ` *   The array to work on` |
|       - | 6239 | ` * $size` |
|       - | 6240 | ` *   The size of each chunk` |
|       - | 6241 | ` * $preserve_keys` |
|       - | 6242 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6243 | ` *   the chunk numerically.` |
|       - | 6244 | ` * Return` |
|       - | 6245 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6246 | ` *  zero, with each dimension containing size elements.` |
|       - | 6247 | ` */` |
|      42 | 6248 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6249 | `{` |
|       - | 6250 | `	ph7_value *pArray,*pChunk;` |
|       - | 6251 | `	ph7_hashmap_node *pEntry;` |
|       - | 6252 | `	ph7_hashmap *pMap;` |
|       - | 6253 | `	int bPreserve;` |
|       - | 6254 | `	sxu32 nChunk;` |
|       - | 6255 | `	sxu32 nSize;` |
|       - | 6256 | `	sxu32 n;` |
|       - | 6257 | `	/* Argument count and types follow PHP semantics. */` |
|      47 | 6258 | `	if( nArg < 2 ){` |
|       - | 6259 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6260 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6261 | `			"ArgumentCountError",` |
|       - | 6262 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6263 | `			nArg` |
|       - | 6264 | `			);` |
|       - | 6265 | `	}` |
|      45 | 6266 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6267 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6268 | `			"TypeError",` |
|       - | 6269 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6270 | `			ph7_type_name(apArg[0])` |
|       - | 6271 | `			);` |
|       - | 6272 | `	}` |
|       - | 6273 | `	/* Create a new array */` |
|      43 | 6274 | `	pArray = ph7_context_new_array(pCtx);` |
|      43 | 6275 | `	if( pArray == 0 ){` |
|     ! 0 | 6276 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6277 | `		return PH7_OK;` |
|       - | 6278 | `	}` |
|       - | 6279 | `	/* Point to the internal representation of the input hashmap */` |
|      43 | 6280 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6281 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6282 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 6283 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      81 | 6284 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6285 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6286 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6287 | `			"TypeError",` |
|       - | 6288 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6289 | `			ph7_type_name(apArg[1])` |
|       - | 6290 | `			);` |
|       - | 6291 | `	}` |
|       - | 6292 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6293 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6294 | `	 * precision and PHP emits a deprecation warning. */` |
|      43 | 6295 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6296 | `		int len;` |
|       3 | 6297 | `		sxu8 bReal = FALSE;` |
|       3 | 6298 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6299 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6300 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6301 | `				"TypeError",` |
|       - | 6302 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6303 | `				);` |
|       - | 6304 | `		}` |
|     ! 0 | 6305 | `		if( bReal ){` |
|       - | 6306 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6307 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6308 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6309 | `				zStr` |
|       - | 6310 | `				);` |
|     ! 0 | 6311 | `		}` |
|     ! 0 | 6312 | `	}` |
|       - | 6313 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6314 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6315 | `	 * later via ph7_value_to_int. */` |
|      40 | 6316 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6317 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6318 | `		sxi64 i = (sxi64)d;` |
|       3 | 6319 | `		if( d != (double)i ){` |
|       4 | 6320 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6321 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6322 | `				d` |
|       - | 6323 | `				);` |
|       1 | 6324 | `		}` |
|       1 | 6325 | `	}` |
|       - | 6326 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6327 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6328 | `	{` |
|      40 | 6329 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      40 | 6330 | `		if( nSizeSigned < 1 ){` |
|       - | 6331 | `			/* size <= 0 -> ValueError */` |
|       6 | 6332 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6333 | `				"ValueError",` |
|       - | 6334 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6335 | `				);` |
|       - | 6336 | `		}` |
|      35 | 6337 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6338 | `	}` |
|      35 | 6339 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6340 | `		/* Return the whole array */` |
|       3 | 6341 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6342 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6343 | `		return PH7_OK;` |
|       - | 6344 | `	}` |
|      33 | 6345 | `	bPreserve = 0;` |
|      33 | 6346 | `	if( nArg > 2 ){` |
|       - | 6347 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6348 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6349 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6350 | `		 * normally, matching PHP behaviour. */` |
|      45 | 6351 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      35 | 6352 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6353 | `			ph7_value_is_resource(apArg[2]) ){` |
|       8 | 6354 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6355 | `				"TypeError",` |
|       - | 6356 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6357 | `				ph7_type_name(apArg[2])` |
|       - | 6358 | `				);` |
|       - | 6359 | `		}` |
|      21 | 6360 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6361 | `	}` |
|       - | 6362 | `	/* Start processing */` |
|      27 | 6363 | `	pEntry = pMap->pFirst;` |
|      27 | 6364 | `	nChunk = 0;` |
|      27 | 6365 | `	pChunk = 0;` |
|      27 | 6366 | `	n = pMap->nEntry;` |
|      56 | 6367 | `	for( ;; ){` |
|     113 | 6368 | `		if( n < 1 ){` |
|       - | 6369 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6370 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6371 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6372 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6373 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6374 | `			 * exists. */` |
|      27 | 6375 | `			if( pChunk ){` |
|      27 | 6376 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6377 | `			}` |
|      27 | 6378 | `			break;` |
|       - | 6379 | `		}` |
|      87 | 6380 | `		if( nChunk < 1 ){` |
|      71 | 6381 | `			if( pChunk ){` |
|       - | 6382 | `				/* Put the first chunk */` |
|      45 | 6383 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6384 | `			}` |
|       - | 6385 | `			/* Create a new dimension */` |
|      71 | 6386 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6387 | `												   * will be automatically released as soon we return` |
|       - | 6388 | `												   * from this function */` |
|      71 | 6389 | `			if( pChunk == 0 ){` |
|     ! 0 | 6390 | `				break;` |
|       - | 6391 | `			}` |
|      71 | 6392 | `			nChunk = nSize;` |
|      35 | 6393 | `		}` |
|       - | 6394 | `		/* Insert the entry */` |
|      87 | 6395 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6396 | `		/* Point to the next entry */` |
|      87 | 6397 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6398 | `		nChunk--;` |
|      87 | 6399 | `		n--;` |
|       1 | 6400 | `	}` |
|       - | 6401 | `	/* Return the multidimensional array */` |
|      27 | 6402 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6403 | `	return PH7_OK;` |
|      26 | 6404 | `}` |
|       - | 6405 | `/*` |
|       - | 6406 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6407 | ` *  Pad array to the specified length with a value.` |
|       - | 6408 | ` * $input` |
|       - | 6409 | ` *   Initial array of values to pad.` |
|       - | 6410 | ` * $pad_size` |
|       - | 6411 | ` *   New size of the array.` |
|       - | 6412 | ` * $pad_value` |
|       - | 6413 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6414 | ` */` |
|      28 | 6415 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6416 | `{` |
|       - | 6417 | `	ph7_hashmap *pMap;` |
|       - | 6418 | `	ph7_value *pArray;` |
|       - | 6419 | `	int nEntry;` |
|      33 | 6420 | `	if( nArg != 3 ){` |
|      12 | 6421 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6422 | `			"ArgumentCountError",` |
|       - | 6423 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6424 | `			nArg` |
|       - | 6425 | `			);` |
|       - | 6426 | `	}` |
|      24 | 6427 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6428 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6429 | `			"TypeError",` |
|       - | 6430 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6431 | `			ph7_type_name(apArg[0])` |
|       - | 6432 | `			);` |
|       - | 6433 | `	}` |
|       - | 6434 | `	/* Create a new array */` |
|      21 | 6435 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6436 | `	if( pArray == 0 ){` |
|     ! 0 | 6437 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6438 | `	}` |
|       - | 6439 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6440 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6441 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6442 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6443 | `	if( nEntry < 0 ){` |
|       9 | 6444 | `		nEntry = -nEntry;` |
|       9 | 6445 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6446 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6447 | `			/* Insert given items first */` |
|      17 | 6448 | `			while( nEntry > 0 ){` |
|      13 | 6449 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6450 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6451 | `				}` |
|      13 | 6452 | `				nEntry--;` |
|       1 | 6453 | `			}` |
|       - | 6454 | `			/* Merge the two arrays */` |
|       5 | 6455 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6456 | `		}else{` |
|       5 | 6457 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6458 | `		}` |
|      17 | 6459 | `	}else if( nEntry > 0 ){` |
|      11 | 6460 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6461 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6462 | `			/* Merge the two arrays first */` |
|       7 | 6463 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6464 | `			/* Insert given items */` |
|      25 | 6465 | `			while( nEntry > 0 ){` |
|      19 | 6466 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6467 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6468 | `				}` |
|      19 | 6469 | `				nEntry--;` |
|       1 | 6470 | `			}` |
|       4 | 6471 | `		}else{` |
|       5 | 6472 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6473 | `		}` |
|       6 | 6474 | `	}else{` |
|       - | 6475 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6476 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6477 | `	}` |
|       - | 6478 | `	/* Return the new array */` |
|      21 | 6479 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6480 | `	return PH7_OK;` |
|      19 | 6481 | `}` |
|       - | 6482 | `/*` |
|       - | 6483 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6484 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6485 | ` * Parameters` |
|       - | 6486 | ` * $array` |
|       - | 6487 | ` *   The array in which elements are replaced.` |
|       - | 6488 | ` * $array1` |
|       - | 6489 | ` *   The array from which elements will be extracted.` |
|       - | 6490 | ` * ....` |
|       - | 6491 | ` *  More arrays from which elements will be extracted.` |
|       - | 6492 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6493 | ` * Return` |
|       - | 6494 | ` *  Returns an array.` |
|       - | 6495 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6496 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6497 | ` */` |
|      22 | 6498 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 6499 | `{` |
|       - | 6500 | `	ph7_hashmap *pMap;` |
|       - | 6501 | `	ph7_value *pArray;` |
|       - | 6502 | `	int i;` |
|      26 | 6503 | `	if( nArg < 1 ){` |
|       3 | 6504 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6505 | `			"ArgumentCountError",` |
|       - | 6506 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6507 | `			);` |
|       - | 6508 | `	}` |
|      23 | 6509 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6510 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6511 | `			"TypeError",` |
|       - | 6512 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6513 | `			ph7_type_name(apArg[0])` |
|       - | 6514 | `			);` |
|       - | 6515 | `	}` |
|       - | 6516 | `	/* Create a new array */` |
|      20 | 6517 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6518 | `	if( pArray == 0 ){` |
|     ! 0 | 6519 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6520 | `		return PH7_OK;` |
|       - | 6521 | `	}` |
|       - | 6522 | `	/* Overwrite from the first array */` |
|      20 | 6523 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6524 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6525 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6526 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6527 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6528 | `			/* Type mismatch -> TypeError */` |
|       4 | 6529 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6530 | `				"TypeError",` |
|       - | 6531 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6532 | `				i + 1,` |
|       2 | 6533 | `				ph7_type_name(apArg[i])` |
|       - | 6534 | `				);` |
|       - | 6535 | `		}` |
|       - | 6536 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6537 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6538 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6539 | `	}` |
|       - | 6540 | `	/* Return the new array */` |
|      17 | 6541 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6542 | `	return PH7_OK;` |
|      15 | 6543 | `}` |
|       - | 6544 | `/*` |
|       - | 6545 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6546 | ` *  Filters elements of an array using a callback function.` |
|       - | 6547 | ` * Parameters` |
|       - | 6548 | ` *  $input` |
|       - | 6549 | ` *    The array to iterate over` |
|       - | 6550 | ` * $callback` |
|       - | 6551 | ` *    The callback function to use` |
|       - | 6552 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6553 | ` *    will be removed.` |
|       - | 6554 | ` * Return` |
|       - | 6555 | ` *  The filtered array.` |
|       - | 6556 | ` */` |
|      22 | 6557 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6558 | `{` |
|       - | 6559 | `	ph7_hashmap_node *pEntry;` |
|       - | 6560 | `	ph7_hashmap *pMap;` |
|       - | 6561 | `	ph7_value *pArray;` |
|       - | 6562 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6563 | `	ph7_value *pValue;` |
|       - | 6564 | `	sxi32 rc;` |
|       - | 6565 | `	int keep;` |
|       - | 6566 | `	sxu32 n;` |
|      24 | 6567 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6568 | `		/* Invalid arguments,return NULL */` |
|       5 | 6569 | `		ph7_result_null(pCtx);` |
|       5 | 6570 | `		return PH7_OK;` |
|       - | 6571 | `	}` |
|       - | 6572 | `	/* Create a new array */` |
|      20 | 6573 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6574 | `	if( pArray == 0 ){` |
|     ! 0 | 6575 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6576 | `		return PH7_OK;` |
|       - | 6577 | `	}` |
|       - | 6578 | `	/* Point to the internal representation of the input hashmap */` |
|      20 | 6579 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6580 | `	pEntry = pMap->pFirst;` |
|      20 | 6581 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      20 | 6582 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6583 | `	/* Perform the requested operation */` |
|      78 | 6584 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6585 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      64 | 6586 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      64 | 6587 | `		if( pValue == 0 ){` |
|       - | 6588 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6589 | `			keep = FALSE;` |
|      64 | 6590 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6591 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6592 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6593 | `				* silently dropped the element.  Emit similar message. */` |
|      36 | 6594 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6595 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6596 | `					int len;` |
|       3 | 6597 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6598 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6599 | `						"TypeError",` |
|       - | 6600 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6601 | `						zName` |
|       - | 6602 | `						);` |
|     ! 0 | 6603 | `				}else{` |
|     ! 0 | 6604 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6605 | `						"TypeError",` |
|       - | 6606 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6607 | `						ph7_type_name(apArg[1])` |
|       - | 6608 | `						);` |
|       - | 6609 | `				}` |
|       - | 6610 | `			}` |
|      33 | 6611 | `			keep = FALSE;` |
|      33 | 6612 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      33 | 6613 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 6614 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6615 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 6616 | `				return PH7_EXCEPTION;` |
|       - | 6617 | `			}` |
|      31 | 6618 | `			if( rc == SXRET_OK ){` |
|       - | 6619 | `				/* Perform a boolean cast */` |
|      31 | 6620 | `				keep = ph7_value_to_bool(&sResult);` |
|      15 | 6621 | `			}` |
|      31 | 6622 | `			PH7_MemObjRelease(&sResult);` |
|      16 | 6623 | `		}else{` |
|       - | 6624 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6625 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6626 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6627 | `			 */` |
|      29 | 6628 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6629 | `		}` |
|      59 | 6630 | `		if( keep ){` |
|       - | 6631 | `			/* Perform the insertion,now the callback returned true */` |
|      21 | 6632 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 6633 | `		}` |
|       - | 6634 | `		/* Point to the next entry */` |
|      59 | 6635 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      30 | 6636 | `	}` |
|      15 | 6637 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 6638 | `	return PH7_OK;` |
|      13 | 6639 | `}` |
|       - | 6640 | `/*` |
|       - | 6641 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 6642 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 6643 | ` * Parameters` |
|       - | 6644 | ` *  $callback` |
|       - | 6645 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 6646 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 6647 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 6648 | ` *   are zipped together.` |
|       - | 6649 | ` *  $array` |
|       - | 6650 | ` *   The first array to run through the callback function.` |
|       - | 6651 | ` *  $arrays` |
|       - | 6652 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 6653 | ` * Return` |
|       - | 6654 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 6655 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 6656 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 6657 | ` *  padding shorter arrays with NULL.` |
|       - | 6658 | ` */` |
|      54 | 6659 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6660 | `{` |
|       - | 6661 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6662 | `	ph7_hashmap_node *pEntry;` |
|       - | 6663 | `	ph7_hashmap *pMap;` |
|       - | 6664 | `	ph7_vm *pVm;` |
|       - | 6665 | `	int bNullCallback;` |
|       - | 6666 | `	sxi32 rc;` |
|       - | 6667 | `	int i;` |
|       - | 6668 | `	sxu32 n;` |
|      59 | 6669 | `	if( nArg < 2 ){` |
|       8 | 6670 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6671 | `			"ArgumentCountError",` |
|       - | 6672 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6673 | `			nArg` |
|       - | 6674 | `			);` |
|       - | 6675 | `	}` |
|      53 | 6676 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      53 | 6677 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6678 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6679 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6680 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6681 | `				"TypeError",` |
|       - | 6682 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6683 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6684 | `				zFunc` |
|       - | 6685 | `				);` |
|       - | 6686 | `		}` |
|       3 | 6687 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6688 | `			"TypeError",` |
|       - | 6689 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6690 | `			"no array or string given"` |
|       - | 6691 | `			);` |
|       - | 6692 | `	}` |
|       - | 6693 | `	/* Every remaining argument must be an array */` |
|     104 | 6694 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      60 | 6695 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 6696 | `			if( i == 1 ){` |
|       4 | 6697 | `				return PH7_VmThrowException(pCtx,` |
|       - | 6698 | `					"TypeError",` |
|       - | 6699 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6700 | `					ph7_type_name(apArg[1])` |
|       - | 6701 | `					);` |
|       - | 6702 | `			}` |
|     ! 0 | 6703 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6704 | `				"TypeError",` |
|       - | 6705 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 6706 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 6707 | `				);` |
|       - | 6708 | `		}` |
|      30 | 6709 | `	}` |
|      46 | 6710 | `	pVm = pCtx->pVm;` |
|       - | 6711 | `	/* Create a new array */` |
|      46 | 6712 | `	pArray = ph7_context_new_array(pCtx);` |
|      46 | 6713 | `	if( pArray == 0 ){` |
|     ! 0 | 6714 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6715 | `		return PH7_OK;` |
|       - | 6716 | `	}` |
|      46 | 6717 | `	PH7_MemObjInit(pVm,&sResult);` |
|      46 | 6718 | `	PH7_MemObjInit(pVm,&sKey);` |
|      46 | 6719 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      46 | 6720 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      46 | 6721 | `	if( nArg == 2 ){` |
|       - | 6722 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      36 | 6723 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      36 | 6724 | `		pEntry = pMap->pFirst;` |
|     110 | 6725 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6726 | `			/* Extract the node value */` |
|      78 | 6727 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|      78 | 6728 | `			if( pValue ){` |
|       - | 6729 | `				/* Extract the node key */` |
|      78 | 6730 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      78 | 6731 | `				if( bNullCallback ){` |
|       - | 6732 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 6733 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6734 | `				}else{` |
|       - | 6735 | `					/* Invoke the supplied callback */` |
|      68 | 6736 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|      68 | 6737 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 6738 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 6739 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       3 | 6740 | `						PH7_MemObjRelease(&sKey);` |
|       3 | 6741 | `						PH7_MemObjRelease(&sResult);` |
|       3 | 6742 | `						return PH7_EXCEPTION;` |
|       - | 6743 | `					}` |
|       - | 6744 | `					/* Insert the callback return value */` |
|      66 | 6745 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6746 | `				}` |
|      76 | 6747 | `				PH7_MemObjRelease(&sKey);` |
|      76 | 6748 | `				PH7_MemObjRelease(&sResult);` |
|      37 | 6749 | `			}` |
|       - | 6750 | `			/* Point to the next entry */` |
|      76 | 6751 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      39 | 6752 | `		}` |
|      18 | 6753 | `	}else{` |
|       - | 6754 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 6755 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 6756 | `		int nArrays = nArg - 1;` |
|       - | 6757 | `		ph7_hashmap_node **apCur;` |
|       - | 6758 | `		ph7_value **apCallArg;` |
|       - | 6759 | `		ph7_value sNull;` |
|      11 | 6760 | `		sxu32 nMax = 0;` |
|      11 | 6761 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 6762 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 6763 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 6764 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 6765 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 6766 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6767 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6768 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 6769 | `			return PH7_OK;` |
|       - | 6770 | `		}` |
|      11 | 6771 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 6772 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 6773 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 6774 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 6775 | `			apCur[i] = pMap->pFirst;` |
|      23 | 6776 | `			if( pMap->nEntry > nMax ){` |
|      13 | 6777 | `				nMax = pMap->nEntry;` |
|       6 | 6778 | `			}` |
|      12 | 6779 | `		}` |
|      35 | 6780 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 6781 | `			ph7_value *pZip = 0;` |
|      25 | 6782 | `			if( bNullCallback ){` |
|       - | 6783 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 6784 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 6785 | `			}` |
|      79 | 6786 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 6787 | `				ph7_value *pv = &sNull;` |
|      55 | 6788 | `				if( apCur[i] ){` |
|      53 | 6789 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 6790 | `					if( pNodeVal ){` |
|      53 | 6791 | `						pv = pNodeVal;` |
|      26 | 6792 | `					}` |
|      53 | 6793 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 6794 | `				}` |
|      55 | 6795 | `				if( bNullCallback ){` |
|       9 | 6796 | `					if( pZip ){` |
|       9 | 6797 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 6798 | `					}` |
|       5 | 6799 | `				}else{` |
|      47 | 6800 | `					apCallArg[i] = pv;` |
|       - | 6801 | `				}` |
|      28 | 6802 | `			}` |
|      25 | 6803 | `			if( bNullCallback ){` |
|       5 | 6804 | `				if( pZip ){` |
|       5 | 6805 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 6806 | `				}` |
|       3 | 6807 | `			}else{` |
|      21 | 6808 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 6809 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6810 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 6811 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 6812 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 6813 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6814 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6815 | `					return PH7_EXCEPTION;` |
|       - | 6816 | `				}` |
|      21 | 6817 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 6818 | `				PH7_MemObjRelease(&sResult);` |
|       - | 6819 | `			}` |
|      13 | 6820 | `		}` |
|      11 | 6821 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 6822 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 6823 | `		PH7_MemObjRelease(&sNull);` |
|       - | 6824 | `	}` |
|      44 | 6825 | `	PH7_MemObjRelease(&sKey);` |
|      44 | 6826 | `	PH7_MemObjRelease(&sResult);` |
|      44 | 6827 | `	ph7_result_value(pCtx,pArray);` |
|      44 | 6828 | `	return PH7_OK;` |
|      32 | 6829 | `}` |
|       - | 6830 | `/*` |
|       - | 6831 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6832 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6833 | ` * Parameters` |
|       - | 6834 | ` *  $array` |
|       - | 6835 | ` *   The input array.` |
|       - | 6836 | ` *  $callback` |
|       - | 6837 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6838 | ` *  $initial` |
|       - | 6839 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6840 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6841 | ` * Return` |
|       - | 6842 | ` *  Returns the resulting value.` |
|       - | 6843 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6844 | ` */` |
|      34 | 6845 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6846 | `{` |
|       - | 6847 | `	ph7_hashmap_node *pEntry;` |
|       - | 6848 | `	ph7_hashmap *pMap;` |
|       - | 6849 | `	ph7_value *pValue;` |
|       - | 6850 | `	ph7_value sResult;` |
|       - | 6851 | `	sxi32 rc;` |
|       - | 6852 | `	sxu32 n;` |
|      39 | 6853 | `	if( nArg < 2 ){` |
|       8 | 6854 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6855 | `			"ArgumentCountError",` |
|       - | 6856 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6857 | `			nArg` |
|       - | 6858 | `			);` |
|       - | 6859 | `	}` |
|      35 | 6860 | `	if( nArg > 3 ){` |
|       4 | 6861 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6862 | `			"ArgumentCountError",` |
|       - | 6863 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6864 | `			nArg` |
|       - | 6865 | `			);` |
|       - | 6866 | `	}` |
|      33 | 6867 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6868 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6869 | `			"TypeError",` |
|       - | 6870 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6871 | `			ph7_type_name(apArg[0])` |
|       - | 6872 | `			);` |
|       - | 6873 | `	}` |
|      31 | 6874 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      12 | 6875 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6876 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6877 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6878 | `				"TypeError",` |
|       - | 6879 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6880 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6881 | `				zFunc` |
|       - | 6882 | `				);` |
|       - | 6883 | `		}` |
|       9 | 6884 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6885 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6886 | `				"TypeError",` |
|       - | 6887 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6888 | `				"array callback must have exactly two members"` |
|       - | 6889 | `				);` |
|       - | 6890 | `		}` |
|       6 | 6891 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6892 | `			"TypeError",` |
|       - | 6893 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6894 | `			"no array or string given"` |
|       - | 6895 | `			);` |
|       - | 6896 | `	}` |
|       - | 6897 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6898 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6899 | `	/* Assume a NULL initial value */` |
|      19 | 6900 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 6901 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 6902 | `	if( nArg > 2 ){` |
|       - | 6903 | `		/* Set the initial value */` |
|      13 | 6904 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 6905 | `	}` |
|       - | 6906 | `	/* Perform the requested operation */` |
|      19 | 6907 | `	pEntry = pMap->pFirst;` |
|      55 | 6908 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6909 | `		/* Extract the node value */` |
|      39 | 6910 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6911 | `		/* Invoke the supplied callback */` |
|      39 | 6912 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      39 | 6913 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 6914 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6915 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 6916 | `			return PH7_EXCEPTION;` |
|       - | 6917 | `		}` |
|       - | 6918 | `		/* Point to the next entry */` |
|      37 | 6919 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6920 | `	}` |
|      17 | 6921 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      17 | 6922 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 6923 | `	return PH7_OK;` |
|      22 | 6924 | `}` |
|       - | 6925 | `/*` |
|       - | 6926 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6927 | ` *  Apply a user function to every member of an array.` |
|       - | 6928 | ` * Parameters` |
|       - | 6929 | ` *  $array` |
|       - | 6930 | ` *   The input array.` |
|       - | 6931 | ` *  $funcname` |
|       - | 6932 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6933 | ` *   the first, and the key/index second.` |
|       - | 6934 | ` * Note:` |
|       - | 6935 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6936 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6937 | ` *  be made in the original array itself.` |
|       - | 6938 | ` *  $userdata` |
|       - | 6939 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6940 | ` *   to the callback funcname.` |
|       - | 6941 | ` * Return` |
|       - | 6942 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6943 | ` */` |
|      38 | 6944 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6945 | `{` |
|       - | 6946 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6947 | `	ph7_hashmap_node *pEntry;` |
|       - | 6948 | `	ph7_hashmap *pMap;` |
|       - | 6949 | `	sxu32 n;` |
|      43 | 6950 | `	if( nArg < 2 ){` |
|       8 | 6951 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6952 | `			"ArgumentCountError",` |
|       - | 6953 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6954 | `			nArg` |
|       - | 6955 | `			);` |
|       - | 6956 | `	}` |
|      39 | 6957 | `	if( nArg > 3 ){` |
|       4 | 6958 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6959 | `			"ArgumentCountError",` |
|       - | 6960 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6961 | `			nArg` |
|       - | 6962 | `			);` |
|       - | 6963 | `	}` |
|      37 | 6964 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6965 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6966 | `			"TypeError",` |
|       - | 6967 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6968 | `			ph7_type_name(apArg[0])` |
|       - | 6969 | `			);` |
|       - | 6970 | `	}` |
|      35 | 6971 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 6972 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6973 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6974 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6975 | `				"TypeError",` |
|       - | 6976 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6977 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6978 | `				zFunc` |
|       - | 6979 | `				);` |
|       - | 6980 | `		}` |
|      12 | 6981 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 6982 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6983 | `				"TypeError",` |
|       - | 6984 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6985 | `				"array callback must have exactly two members"` |
|       - | 6986 | `				);` |
|       - | 6987 | `		}` |
|       6 | 6988 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6989 | `			"TypeError",` |
|       - | 6990 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6991 | `			"no array or string given"` |
|       - | 6992 | `			);` |
|       - | 6993 | `	}` |
|      21 | 6994 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6995 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6996 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 6997 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 6998 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 6999 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 7000 | `	/* Perform the desired operation */` |
|      21 | 7001 | `	pEntry = pMap->pFirst;` |
|      61 | 7002 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7003 | `		/* Extract the node value */` |
|      43 | 7004 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 7005 | `		if( pValue ){` |
|       - | 7006 | `			sxi32 rcW;` |
|       - | 7007 | `			/* Extract the entry key */` |
|      43 | 7008 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 7009 | `			/* Invoke the supplied callback */` |
|      43 | 7010 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 7011 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 7012 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 7013 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 7014 | `				return PH7_EXCEPTION;` |
|       - | 7015 | `			}` |
|      20 | 7016 | `		}` |
|       - | 7017 | `		/* Point to the next entry */` |
|      41 | 7018 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 7019 | `	}` |
|       - | 7020 | `	/* All done, return TRUE */` |
|      19 | 7021 | `	ph7_result_bool(pCtx,1);` |
|      19 | 7022 | `	return PH7_OK;` |
|      24 | 7023 | `}` |
|       - | 7024 | `/*` |
|       - | 7025 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 7026 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 7027 | ` */` |
|      22 | 7028 | `static sxi32 HashmapWalkRecursive(` |
|       - | 7029 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 7030 | `	ph7_value *pCallback, /* User callback */` |
|       - | 7031 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 7032 | `	int iNest             /* Nesting level */` |
|       - | 7033 | `	)` |
|       1 | 7034 | `{` |
|       - | 7035 | `	ph7_hashmap_node *pEntry;` |
|       - | 7036 | `	ph7_value *pValue,sKey;` |
|       - | 7037 | `	sxi32 rc;` |
|       - | 7038 | `	sxu32 n;` |
|       - | 7039 | `	/* Iterate through hashmap entries */` |
|      23 | 7040 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 7041 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 7042 | `	pEntry = pMap->pFirst;` |
|      59 | 7043 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7044 | `		/* Extract the node value */` |
|      37 | 7045 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 7046 | `		if( pValue ){` |
|      37 | 7047 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 7048 | `				if( iNest < 32 ){` |
|       - | 7049 | `					/* Recurse */` |
|      11 | 7050 | `					iNest++;` |
|      11 | 7051 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 7052 | `					iNest--;` |
|      11 | 7053 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 7054 | `						return PH7_EXCEPTION;` |
|       - | 7055 | `					}` |
|       5 | 7056 | `				}` |
|       6 | 7057 | `			}else{` |
|       - | 7058 | `				/* Extract the node key */` |
|      27 | 7059 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 7060 | `				/* Invoke the supplied callback */` |
|      27 | 7061 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 7062 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 7063 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 7064 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7065 | `					return PH7_EXCEPTION;` |
|       - | 7066 | `				}` |
|       - | 7067 | `			}` |
|      18 | 7068 | `		}` |
|       - | 7069 | `		/* Point to the next entry */` |
|      37 | 7070 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 7071 | `	}` |
|      23 | 7072 | `	return PH7_OK;` |
|      12 | 7073 | `}` |
|       - | 7074 | `/*` |
|       - | 7075 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 7076 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 7077 | ` * Parameters` |
|       - | 7078 | ` *  $array` |
|       - | 7079 | ` *   The input array.` |
|       - | 7080 | ` *  $funcname` |
|       - | 7081 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 7082 | ` *   the first, and the key/index second.` |
|       - | 7083 | ` * Note:` |
|       - | 7084 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 7085 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 7086 | ` *  be made in the original array itself.` |
|       - | 7087 | ` *  $userdata` |
|       - | 7088 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 7089 | ` *   to the callback funcname.` |
|       - | 7090 | ` * Return` |
|       - | 7091 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 7092 | ` */` |
|      30 | 7093 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7094 | `{` |
|       - | 7095 | `	ph7_hashmap *pMap;` |
|      35 | 7096 | `	if( nArg < 2 ){` |
|       8 | 7097 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7098 | `			"ArgumentCountError",` |
|       - | 7099 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 7100 | `			nArg` |
|       - | 7101 | `			);` |
|       - | 7102 | `	}` |
|      31 | 7103 | `	if( nArg > 3 ){` |
|       4 | 7104 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7105 | `			"ArgumentCountError",` |
|       - | 7106 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 7107 | `			nArg` |
|       - | 7108 | `			);` |
|       - | 7109 | `	}` |
|      29 | 7110 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7111 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7112 | `			"TypeError",` |
|       - | 7113 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7114 | `			ph7_type_name(apArg[0])` |
|       - | 7115 | `			);` |
|       - | 7116 | `	}` |
|      27 | 7117 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 7118 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7119 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7120 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7121 | `				"TypeError",` |
|       - | 7122 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7123 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7124 | `				zFunc` |
|       - | 7125 | `				);` |
|       - | 7126 | `		}` |
|      12 | 7127 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 7128 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7129 | `				"TypeError",` |
|       - | 7130 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7131 | `				"array callback must have exactly two members"` |
|       - | 7132 | `				);` |
|       - | 7133 | `		}` |
|       6 | 7134 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7135 | `			"TypeError",` |
|       - | 7136 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7137 | `			"no array or string given"` |
|       - | 7138 | `			);` |
|       - | 7139 | `	}` |
|       - | 7140 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 7141 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 7142 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7143 | `	/* Perform the desired operation */` |
|      13 | 7144 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 7145 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7146 | `		return PH7_EXCEPTION;` |
|       - | 7147 | `	}` |
|       - | 7148 | `	/* All done, return TRUE */` |
|      13 | 7149 | `	ph7_result_bool(pCtx,1);` |
|      13 | 7150 | `	return PH7_OK;` |
|      20 | 7151 | `}` |
|       - | 7152 | `/*` |
|       - | 7153 | ` * bool array_is_list(array $array)` |
|       - | 7154 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 7155 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 7156 | ` * Return` |
|       - | 7157 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 7158 | ` */` |
|       - | 7159 | `/*` |
|       - | 7160 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 7161 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 7162 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 7163 | ` */` |
|     114 | 7164 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       1 | 7165 | `{` |
|     115 | 7166 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|     115 | 7167 | `	sxi64 iExpect = 0;` |
|       - | 7168 | `	sxu32 n;` |
|     233 | 7169 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     169 | 7170 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 7171 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|      51 | 7172 | `			return 0;` |
|       - | 7173 | `		}` |
|     119 | 7174 | `		++iExpect;` |
|     119 | 7175 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      60 | 7176 | `	}` |
|      65 | 7177 | `	return 1;` |
|      58 | 7178 | `}` |
|      12 | 7179 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7180 | `{` |
|      13 | 7181 | `	if( nArg < 1 ){` |
|     ! 0 | 7182 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7183 | `			"ArgumentCountError",` |
|       - | 7184 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 7185 | `			);` |
|       - | 7186 | `	}` |
|      13 | 7187 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7188 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7189 | `			"TypeError",` |
|       - | 7190 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7191 | `			ph7_type_name(apArg[0])` |
|       - | 7192 | `			);` |
|       - | 7193 | `	}` |
|      13 | 7194 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 7195 | `	return PH7_OK;` |
|       7 | 7196 | `}` |
|       - | 7197 | `/*` |
|       - | 7198 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 7199 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 7200 | ` * array_column() for both the column value and the index key.` |
|       - | 7201 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 7202 | ` * container or the key is absent.` |
|       - | 7203 | ` */` |
|      32 | 7204 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 7205 | `{` |
|      33 | 7206 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 7207 | `		ph7_hashmap_node *pNode;` |
|      25 | 7208 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 7209 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 7210 | `		}` |
|      11 | 7211 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 7212 | `		ph7_value sName;` |
|       - | 7213 | `		const char *zName;` |
|       - | 7214 | `		ph7_value *pAttr;` |
|       - | 7215 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 7216 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 7217 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 7218 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 7219 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 7220 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 7221 | `		PH7_MemObjRelease(&sName);` |
|       9 | 7222 | `		return pAttr;` |
|       - | 7223 | `	}` |
|       5 | 7224 | `	return 0;` |
|      17 | 7225 | `}` |
|       - | 7226 | `/*` |
|       - | 7227 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 7228 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 7229 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 7230 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 7231 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 7232 | ` *  Each row may be an array or an object.` |
|       - | 7233 | ` */` |
|      12 | 7234 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7235 | `{` |
|       - | 7236 | `	ph7_hashmap_node *pNode;` |
|       - | 7237 | `	ph7_hashmap *pMap;` |
|       - | 7238 | `	ph7_value *pArray;` |
|       - | 7239 | `	ph7_value *pRow;` |
|       - | 7240 | `	ph7_value *pCol;` |
|       - | 7241 | `	ph7_value *pIdx;` |
|       - | 7242 | `	int bWantCol;` |
|       - | 7243 | `	int bWantIdx;` |
|       - | 7244 | `	sxu32 n;` |
|      13 | 7245 | `	if( nArg < 2 ){` |
|     ! 0 | 7246 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7247 | `			"ArgumentCountError",` |
|       - | 7248 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 7249 | `			nArg` |
|       - | 7250 | `			);` |
|       - | 7251 | `	}` |
|      13 | 7252 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7253 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7254 | `			"TypeError",` |
|       - | 7255 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7256 | `			ph7_type_name(apArg[0])` |
|       - | 7257 | `			);` |
|       - | 7258 | `	}` |
|      13 | 7259 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 7260 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 7261 | `	if( pArray == 0 ){` |
|     ! 0 | 7262 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7263 | `		return PH7_OK;` |
|       - | 7264 | `	}` |
|       - | 7265 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 7266 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 7267 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 7268 | `	pNode = pMap->pFirst;` |
|      33 | 7269 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 7270 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 7271 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 7272 | `		if( pRow == 0 ){` |
|     ! 0 | 7273 | `			continue;` |
|       - | 7274 | `		}` |
|      21 | 7275 | `		if( bWantCol ){` |
|      19 | 7276 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 7277 | `			if( pCol == 0 ){` |
|       - | 7278 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 7279 | `				continue;` |
|       - | 7280 | `			}` |
|       9 | 7281 | `		}else{` |
|       3 | 7282 | `			pCol = pRow;` |
|       - | 7283 | `		}` |
|      19 | 7284 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 7285 | `		if( pIdx ){` |
|      13 | 7286 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 7287 | `		}else{` |
|       7 | 7288 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 7289 | `		}` |
|      10 | 7290 | `	}` |
|      13 | 7291 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 7292 | `	return PH7_OK;` |
|       7 | 7293 | `}` |
|       - | 7294 | `/*` |
|       - | 7295 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 7296 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 7297 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 7298 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 7299 | ` */` |
|      28 | 7300 | `static sxi32 HashmapCallbackSearch(` |
|       - | 7301 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 7302 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 7303 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 7304 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 7305 | `	)` |
|       1 | 7306 | `{` |
|       - | 7307 | `	ph7_hashmap_node *pEntry;` |
|       - | 7308 | `	ph7_hashmap *pMap;` |
|       - | 7309 | `	ph7_value *pValue;` |
|       - | 7310 | `	ph7_value *apCbArg[2];` |
|       - | 7311 | `	ph7_value sKey;` |
|       - | 7312 | `	ph7_value sResult;` |
|       - | 7313 | `	sxi32 rc;` |
|       - | 7314 | `	sxu32 n;` |
|      29 | 7315 | `	*ppMatch = 0;` |
|      29 | 7316 | `	if( nArg < 2 ){` |
|     ! 0 | 7317 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7318 | `			"ArgumentCountError",` |
|       - | 7319 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 7320 | `			zName,nArg` |
|       - | 7321 | `			);` |
|       - | 7322 | `	}` |
|      29 | 7323 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7324 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7325 | `			"TypeError",` |
|       - | 7326 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7327 | `			zName,ph7_type_name(apArg[0])` |
|       - | 7328 | `			);` |
|       - | 7329 | `	}` |
|      29 | 7330 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7331 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7332 | `			"TypeError",` |
|       - | 7333 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 7334 | `			zName,ph7_type_name(apArg[1])` |
|       - | 7335 | `			);` |
|       - | 7336 | `	}` |
|      29 | 7337 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 7338 | `	pEntry = pMap->pFirst;` |
|      29 | 7339 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 7340 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 7341 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 7342 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 7343 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 7344 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 7345 | `		if( pValue ){` |
|       - | 7346 | `			/* The callback receives ($value, $key). */` |
|      59 | 7347 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 7348 | `			apCbArg[0] = pValue;` |
|      59 | 7349 | `			apCbArg[1] = &sKey;` |
|      59 | 7350 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 7351 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 7352 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7353 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7354 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7355 | `				return PH7_EXCEPTION;` |
|       - | 7356 | `			}` |
|      59 | 7357 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 7358 | `				*ppMatch = pEntry;` |
|      15 | 7359 | `				break;` |
|       - | 7360 | `			}` |
|      22 | 7361 | `		}` |
|      45 | 7362 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 7363 | `	}` |
|      29 | 7364 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 7365 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 7366 | `	return PH7_OK;` |
|      15 | 7367 | `}` |
|       - | 7368 | `/*` |
|       - | 7369 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 7370 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 7371 | ` *  is truthy, or NULL if none match.` |
|       - | 7372 | ` */` |
|       6 | 7373 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7374 | `{` |
|       - | 7375 | `	ph7_hashmap_node *pMatch;` |
|       - | 7376 | `	ph7_value *pVal;` |
|       - | 7377 | `	sxi32 rc;` |
|       7 | 7378 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 7379 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7380 | `		return rc;` |
|       - | 7381 | `	}` |
|       7 | 7382 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 7383 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 7384 | `	}else{` |
|       3 | 7385 | `		ph7_result_null(pCtx);` |
|       - | 7386 | `	}` |
|       7 | 7387 | `	return PH7_OK;` |
|       4 | 7388 | `}` |
|       - | 7389 | `/*` |
|       - | 7390 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 7391 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 7392 | ` *  is truthy, or NULL if none match.` |
|       - | 7393 | ` */` |
|       6 | 7394 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7395 | `{` |
|       - | 7396 | `	ph7_hashmap_node *pMatch;` |
|       - | 7397 | `	sxi32 rc;` |
|       7 | 7398 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 7399 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7400 | `		return rc;` |
|       - | 7401 | `	}` |
|       7 | 7402 | `	if( pMatch == 0 ){` |
|       3 | 7403 | `		ph7_result_null(pCtx);` |
|       6 | 7404 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 7405 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 7406 | `	}else{` |
|       4 | 7407 | `		ph7_result_string(pCtx,` |
|       2 | 7408 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 7409 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 7410 | `	}` |
|       7 | 7411 | `	return PH7_OK;` |
|       4 | 7412 | `}` |
|       - | 7413 | `/*` |
|       - | 7414 | ` * bool array_any(array $array, callable $callback)` |
|       - | 7415 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 7416 | ` *  FALSE for an empty array.` |
|       - | 7417 | ` */` |
|       8 | 7418 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7419 | `{` |
|       - | 7420 | `	ph7_hashmap_node *pMatch;` |
|       - | 7421 | `	sxi32 rc;` |
|       9 | 7422 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 7423 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7424 | `		return rc;` |
|       - | 7425 | `	}` |
|       9 | 7426 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 7427 | `	return PH7_OK;` |
|       5 | 7428 | `}` |
|       - | 7429 | `/*` |
|       - | 7430 | ` * bool array_all(array $array, callable $callback)` |
|       - | 7431 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 7432 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 7433 | ` */` |
|       8 | 7434 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7435 | `{` |
|       - | 7436 | `	ph7_hashmap_node *pMatch;` |
|       - | 7437 | `	sxi32 rc;` |
|       9 | 7438 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 7439 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7440 | `		return rc;` |
|       - | 7441 | `	}` |
|       9 | 7442 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 7443 | `	return PH7_OK;` |
|       5 | 7444 | `}` |
|       - | 7445 | `/*` |
|       - | 7446 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 7447 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 7448 | ` */` |
|       - | 7449 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 7450 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|      70 | 7451 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       5 | 7452 | `{` |
|      75 | 7453 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|      35 | 7454 | `	(void)pVm;` |
|      75 | 7455 | `	p->nCount++;` |
|      75 | 7456 | `	if( p->pArray ){` |
|       - | 7457 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 7458 | `		 * otherwise append with an auto-assigned int index. */` |
|      67 | 7459 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|      31 | 7460 | `	}` |
|      75 | 7461 | `	return SXRET_OK;` |
|       5 | 7462 | `}` |
|       - | 7463 | `/*` |
|       - | 7464 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 7465 | ` */` |
|      26 | 7466 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 7467 | `{` |
|       - | 7468 | `	struct IterCollect sCol;` |
|       - | 7469 | `	ph7_value *pArray;` |
|       - | 7470 | `	sxi32 rc;` |
|      31 | 7471 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      31 | 7472 | `	pArray = ph7_context_new_array(pCtx);` |
|      31 | 7473 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      31 | 7474 | `	sCol.pArray = pArray;` |
|      31 | 7475 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|      31 | 7476 | `	sCol.nCount = 0;` |
|      31 | 7477 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 7478 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 7479 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 7480 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7481 | `		sxu32 n;` |
|       9 | 7482 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7483 | `			ph7_value sKey, *pVal;` |
|       7 | 7484 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 7485 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 7486 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 7487 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 7488 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 7489 | `			pEntry = pEntry->pPrev;` |
|       4 | 7490 | `		}` |
|       3 | 7491 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 7492 | `		return PH7_OK;` |
|       - | 7493 | `	}` |
|      29 | 7494 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      29 | 7495 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      27 | 7496 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7497 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7498 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 7499 | `			ph7_type_name(apArg[0]));` |
|       - | 7500 | `	}` |
|      27 | 7501 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 7502 | `	return PH7_OK;` |
|      18 | 7503 | `}` |
|       - | 7504 | `/*` |
|       - | 7505 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 7506 | ` */` |
|       6 | 7507 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 7508 | `{` |
|       - | 7509 | `	struct IterCollect sCol;` |
|       - | 7510 | `	sxi32 rc;` |
|       7 | 7511 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       7 | 7512 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 7513 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 7514 | `		return PH7_OK;` |
|       - | 7515 | `	}` |
|       5 | 7516 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|       5 | 7517 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|       5 | 7518 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       5 | 7519 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7520 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7521 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 7522 | `			ph7_type_name(apArg[0]));` |
|       - | 7523 | `	}` |
|       5 | 7524 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|       5 | 7525 | `	return PH7_OK;` |
|       4 | 7526 | `}` |
|       - | 7527 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 7528 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 7529 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 7530 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      24 | 7531 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       1 | 7532 | `{` |
|      25 | 7533 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 7534 | `	ph7_value sResult;` |
|       - | 7535 | `	SySet aArg;` |
|       - | 7536 | `	sxi32 rc;` |
|       - | 7537 | `	int bContinue;` |
|      12 | 7538 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      25 | 7539 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      25 | 7540 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 7541 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|       9 | 7542 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7543 | `		sxu32 n;` |
|      17 | 7544 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       9 | 7545 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       9 | 7546 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|       9 | 7547 | `			pEntry = pEntry->pPrev;` |
|       5 | 7548 | `		}` |
|       4 | 7549 | `	}` |
|      25 | 7550 | `	PH7_MemObjInit(pVm,&sResult);` |
|      37 | 7551 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      24 | 7552 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|      25 | 7553 | `	SySetRelease(&aArg);` |
|      25 | 7554 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      23 | 7555 | `	p->nCount++;` |
|      23 | 7556 | `	PH7_MemObjToBool(&sResult);` |
|      23 | 7557 | `	bContinue = (sResult.x.iVal != 0);` |
|      23 | 7558 | `	PH7_MemObjRelease(&sResult);` |
|      23 | 7559 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      13 | 7560 | `}` |
|       - | 7561 | `/*` |
|       - | 7562 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 7563 | ` */` |
|       8 | 7564 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 7565 | `{` |
|       - | 7566 | `	struct IterApply sApp;` |
|       - | 7567 | `	sxi32 rc;` |
|       9 | 7568 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       9 | 7569 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7570 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7571 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|       - | 7572 | `	}` |
|       9 | 7573 | `	sApp.pCallback = apArg[1];` |
|       9 | 7574 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|       9 | 7575 | `	sApp.nCount = 0;` |
|       9 | 7576 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|       9 | 7577 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       7 | 7578 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 7579 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 7580 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 7581 | `			ph7_type_name(apArg[0]));` |
|       - | 7582 | `	}` |
|       7 | 7583 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|       7 | 7584 | `	return PH7_OK;` |
|       5 | 7585 | `}` |
|       - | 7586 | `/*` |
|       - | 7587 | ` * Table of hashmap functions.` |
|       - | 7588 | ` */` |
|       - | 7589 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 7590 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|       - | 7591 | `	{"iterator_count",     ph7_iterator_count },` |
|       - | 7592 | `	{"iterator_apply",     ph7_iterator_apply },` |
|       - | 7593 | `	{"count",             ph7_hashmap_count },` |
|       - | 7594 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 7595 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 7596 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 7597 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 7598 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 7599 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 7600 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 7601 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 7602 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 7603 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 7604 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 7605 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 7606 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 7607 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 7608 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 7609 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 7610 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 7611 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 7612 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 7613 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 7614 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 7615 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 7616 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 7617 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 7618 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 7619 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 7620 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 7621 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 7622 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 7623 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 7624 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 7625 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 7626 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 7627 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 7628 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 7629 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 7630 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 7631 | `	{"array_column",      ph7_hashmap_column  },` |
|       - | 7632 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|       - | 7633 | `	{"array_find",        ph7_hashmap_find    },` |
|       - | 7634 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|       - | 7635 | `	{"array_any",         ph7_hashmap_any     },` |
|       - | 7636 | `	{"array_all",         ph7_hashmap_all     },` |
|       - | 7637 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 7638 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 7639 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 7640 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 7641 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 7642 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 7643 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 7644 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 7645 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 7646 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 7647 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 7648 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 7649 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 7650 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 7651 | `	{"range",             ph7_hashmap_range   },` |
|       - | 7652 | `	{"current",           ph7_hashmap_current },` |
|       - | 7653 | `	{"each",              ph7_hashmap_each    },` |
|       - | 7654 | `	{"pos",               ph7_hashmap_current },` |
|       - | 7655 | `	{"next",              ph7_hashmap_next    },` |
|       - | 7656 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 7657 | `	{"end",               ph7_hashmap_end     },` |
|       - | 7658 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 7659 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 7660 | `};` |
|       - | 7661 | `/*` |
|       - | 7662 | ` * Register the built-in hashmap functions defined above.` |
|       - | 7663 | ` */` |
|    3446 | 7664 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       5 | 7665 | `{` |
|       - | 7666 | `	sxu32 n;` |
|  244671 | 7667 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  241225 | 7668 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|  120615 | 7669 | `	}` |
|    3451 | 7670 | `}` |
|       - | 7671 | `/*` |
|       - | 7672 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 7673 | ` * the BLOB given as the first argument.` |
|       - | 7674 | ` * This function is typically invoked when the user issue a call to` |
|       - | 7675 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 7676 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 7677 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 7678 | ` */` |
|       - | 7679 | `/*` |
|       - | 7680 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|       - | 7681 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|       - | 7682 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|       - | 7683 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|       - | 7684 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|       - | 7685 | ` */` |
|      26 | 7686 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       3 | 7687 | `{` |
|      29 | 7688 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 7689 | `	ph7_value *pObj;` |
|      29 | 7690 | `	sxu32 n = 0;` |
|       - | 7691 | `	int isRef;` |
|      29 | 7692 | `	sxi32 rc = SXRET_OK;` |
|       - | 7693 | `	int i;` |
|      44 | 7694 | `	for(;;){` |
|      91 | 7695 | `		if( n >= pMap->nEntry ){` |
|      29 | 7696 | `			break;` |
|       - | 7697 | `		}` |
|     127 | 7698 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      65 | 7699 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      34 | 7700 | `		}` |
|       - | 7701 | `		/* Dump key */` |
|      65 | 7702 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 7703 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 7704 | `		}else{` |
|      48 | 7705 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      15 | 7706 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 7707 | `		}` |
|       - | 7708 | `#ifdef __WINNT__` |
|       3 | 7709 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7710 | `#else` |
|      62 | 7711 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7712 | `#endif` |
|       - | 7713 | `		/* Dump node value */` |
|      65 | 7714 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      65 | 7715 | `		isRef = 0;` |
|      65 | 7716 | `		if( pObj ){` |
|      65 | 7717 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 7718 | `				/* Referenced object */` |
|     ! 0 | 7719 | `				isRef = 1;` |
|     ! 0 | 7720 | `			}` |
|      65 | 7721 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|      65 | 7722 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 7723 | `				break;` |
|       - | 7724 | `			}` |
|      31 | 7725 | `		}` |
|       - | 7726 | `		/* Point to the next entry */` |
|      65 | 7727 | `		n++;` |
|      65 | 7728 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       3 | 7729 | `	}` |
|      29 | 7730 | `	return rc;` |
|       3 | 7731 | `}` |
|      22 | 7732 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 7733 | `{` |
|       - | 7734 | `	sxi32 rc;` |
|       - | 7735 | `	int i;` |
|      24 | 7736 | `	if( nDepth > 31 ){` |
|       - | 7737 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 7738 | `		/* Nesting limit reached */` |
|     ! 0 | 7739 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 7740 | `		if( ShowType ){` |
|     ! 0 | 7741 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 7742 | `		}` |
|     ! 0 | 7743 | `		return SXERR_LIMIT;` |
|       - | 7744 | `	}` |
|      24 | 7745 | `	if( !ShowType ){` |
|      11 | 7746 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       5 | 7747 | `	}` |
|       - | 7748 | `	/* Total entries */` |
|      24 | 7749 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 7750 | `#ifdef __WINNT__` |
|       2 | 7751 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7752 | `#else` |
|      22 | 7753 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7754 | `#endif` |
|      24 | 7755 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|      46 | 7756 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      24 | 7757 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      13 | 7758 | `	}` |
|      24 | 7759 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      24 | 7760 | `	return rc;` |
|      13 | 7761 | `}` |
|       - | 7762 | `/*` |
|       - | 7763 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 7764 | ` * retrieved entry.` |
|       - | 7765 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 7766 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 7767 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 7768 | ` * a value different from PH7_OK.` |
|       - | 7769 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 7770 | ` */` |
|   32948 | 7771 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 7772 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 7773 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 7774 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 7775 | `	)` |
|       5 | 7776 | `{` |
|       - | 7777 | `	ph7_hashmap_node *pEntry;` |
|       - | 7778 | `	ph7_value sKey,sValue;` |
|       - | 7779 | `	sxi32 rc;` |
|       - | 7780 | `	sxu32 n;` |
|       - | 7781 | `	/* Initialize walker parameter */` |
|   32953 | 7782 | `	rc = SXRET_OK;` |
|   32953 | 7783 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   32953 | 7784 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   32953 | 7785 | `	n = pMap->nEntry;` |
|   32953 | 7786 | `	pEntry = pMap->pFirst;` |
|       - | 7787 | `	/* Start the iteration process */` |
|   83322 | 7788 | `	for(;;){` |
|  166649 | 7789 | `		if( n < 1 ){` |
|   32953 | 7790 | `			break;` |
|       - | 7791 | `		}` |
|       - | 7792 | `		/* Extract a copy of the key and a copy the current value */` |
|  133701 | 7793 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  133701 | 7794 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 7795 | `		/* Invoke the user callback */` |
|  133701 | 7796 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 7797 | `		/* Release the copy of the key and the value */` |
|  133701 | 7798 | `		PH7_MemObjRelease(&sKey);` |
|  133701 | 7799 | `		PH7_MemObjRelease(&sValue);` |
|  133701 | 7800 | `		if( rc != PH7_OK ){` |
|       - | 7801 | `			/* Callback request an operation abort */` |
|     ! 0 | 7802 | `			return SXERR_ABORT;` |
|       - | 7803 | `		}` |
|       - | 7804 | `		/* Point to the next entry */` |
|  133701 | 7805 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  133701 | 7806 | `		n--;` |
|       5 | 7807 | `	}` |
|       - | 7808 | `	/* All done */` |
|   32953 | 7809 | `	return SXRET_OK;` |
|   16479 | 7810 | `}` |
|       - | 7811 |  |
