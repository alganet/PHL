# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3079/3537 lines (87.05%)

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
| 3015830 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 3015832 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  313052 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  313054 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  313054 |   29 | `	sxu32 nH = 5381;` |
|  313054 |   30 | `	zEnd = &zIn[nLen];` |
|  348463 |   31 | `	for(;;){` |
|  696928 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  609808 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  547326 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  452860 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  313054 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   43 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   44 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   45 | ` */` |
|     896 |   46 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       2 |   47 |  |
|     898 |   48 | `	sxi64 iCount = 0;` |
|     898 |   49 | `	if( !bRecursive ){` |
|     724 |   50 | `		iCount = pMap->nEntry;` |
|     363 |   51 | `	}else{` |
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
|     898 |   86 | `	return iCount;` |
|       2 |   87 |  |
|       - |   88 | `/*` |
|       - |   89 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   90 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   91 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   92 | ` */` |
| 2956778 |   93 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   94 |  |
|       - |   95 | `	ph7_hashmap_node *pNode;` |
|       - |   96 | `	/* Allocate a new node */` |
| 2956780 |   97 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2956780 |   98 | `	if( pNode == 0 ){` |
|     ! 0 |   99 | `		return 0;` |
|       - |  100 | `	}` |
|       - |  101 | `	/* Zero the stucture */` |
| 2956780 |  102 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  103 | `	/* Fill in the structure */` |
| 2956780 |  104 | `	pNode->pMap  = &(*pMap);` |
| 2956780 |  105 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2956780 |  106 | `	pNode->nHash = nHash;` |
| 2956780 |  107 | `	pNode->xKey.iKey = iKey;` |
| 2956780 |  108 | `	pNode->nValIdx  = nValIdx;` |
| 2956780 |  109 | `	return pNode;` |
| 1478391 |  110 |  |
|       - |  111 | `/*` |
|       - |  112 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  115 | ` */` |
|  107732 |  116 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  117 |  |
|       - |  118 | `	ph7_hashmap_node *pNode;` |
|       - |  119 | `	/* Allocate a new node */` |
|  107734 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  107734 |  121 | `	if( pNode == 0 ){` |
|     ! 0 |  122 | `		return 0;` |
|       - |  123 | `	}` |
|       - |  124 | `	/* Zero the stucture */` |
|  107734 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  126 | `	/* Fill in the structure */` |
|  107734 |  127 | `	pNode->pMap  = &(*pMap);` |
|  107734 |  128 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  107734 |  129 | `	pNode->nHash = nHash;` |
|  107734 |  130 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  107734 |  131 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  107734 |  132 | `	pNode->nValIdx = nValIdx;` |
|  107734 |  133 | `	return pNode;` |
|   53868 |  134 |  |
|       - |  135 | `/*` |
|       - |  136 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  137 | ` */` |
| 3064510 |  138 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  139 |  |
|       - |  140 | `	/* Link */` |
| 3064512 |  141 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2762936 |  142 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2762936 |  143 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1381467 |  144 | `	}` |
| 3064512 |  145 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  146 | `	/* Link to the map list */` |
| 3064512 |  147 | `	if( pMap->pFirst == 0 ){` |
|   54000 |  148 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  149 | `		/* Point to the first inserted node */` |
|   54000 |  150 | `		pMap->pCur = pNode;` |
|   27001 |  151 | `	}else{` |
| 3010514 |  152 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  153 | `	}` |
| 3064512 |  154 | `	++pMap->nEntry;` |
| 3064512 |  155 |  |
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
|    6466 |  166 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3234 |  167 | `	}else{` |
|     449 |  168 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  169 | `	}` |
|    6914 |  170 | `	if( pNode->pNextCollide ){` |
|    5395 |  171 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2697 |  172 | `	}` |
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
| 3064510 |  207 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  208 |  |
| 3064512 |  209 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   58278 |  210 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  211 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   58278 |  212 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  213 | `		sxu32 nBucket;` |
|       - |  214 | `		sxu32 n;` |
|   58278 |  215 | `		if( nNew < 1 ){` |
|   54000 |  216 | `			nNew = 16;` |
|   26999 |  217 | `		}` |
|       - |  218 | `		/* Allocate a new bucket */` |
|   58278 |  219 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   58278 |  220 | `		if( apNew == 0 ){` |
|     ! 0 |  221 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  222 | `				return SXERR_MEM; /* Fatal */` |
|       - |  223 | `			}` |
|       - |  224 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Zero the table */` |
|   58278 |  228 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  229 | `		/* Reflect the change */` |
|   58278 |  230 | `		pMap->apBucket = apNew;` |
|   58278 |  231 | `		pMap->nSize = nNew;` |
|   58278 |  232 | `		if( apOld == 0 ){` |
|       - |  233 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   54000 |  234 | `			return SXRET_OK;` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Rehash old entries */` |
|    4280 |  237 | `		pEntry = pMap->pFirst;` |
|    4280 |  238 | `		n = 0;` |
| 2027883 |  239 | `		for( ;; ){` |
| 4055768 |  240 | `			if( n >= pMap->nEntry ){` |
|    4280 |  241 | `				break;` |
|       - |  242 | `			}` |
|       - |  243 | `			/* Clear the old collision link */` |
| 4051490 |  244 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  245 | `			/* Link to the new bucket */` |
| 4051490 |  246 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4051490 |  247 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3509514 |  248 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3509514 |  249 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1754756 |  250 | `			}` |
| 4051490 |  251 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  252 | `			/* Point to the next entry */` |
| 4051490 |  253 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4051490 |  254 | `			n++;` |
|       2 |  255 | `		}` |
|       - |  256 | `		/* Free the old table */` |
|    4280 |  257 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2139 |  258 | `	}` |
| 3010514 |  259 | `	return SXRET_OK;` |
| 1532257 |  260 |  |
|       - |  261 | `/*` |
|       - |  262 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  263 | ` * hashmap.` |
|       - |  264 | ` */` |
| 2956778 |  265 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  266 |  |
|       - |  267 | `	ph7_hashmap_node *pNode;` |
|       - |  268 | `	sxu32 nIdx;` |
|       - |  269 | `	sxu32 nHash;` |
|       - |  270 | `	sxi32 rc;` |
| 2956780 |  271 | `	if( !isForeign ){` |
|       - |  272 | `		ph7_value *pObj;` |
|       - |  273 | `		/* Reserve a ph7_value for the value */` |
| 2956746 |  274 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2956746 |  275 | `		if( pObj == 0 ){` |
|     ! 0 |  276 | `			return SXERR_MEM;` |
|       - |  277 | `		}` |
| 2956746 |  278 | `		if( pValue ){` |
|       - |  279 | `			/* Duplicate the value */` |
| 2956746 |  280 | `			PH7_MemObjStore(pValue,pObj);` |
| 1478372 |  281 | `		}` |
| 2956746 |  282 | `		nIdx = pObj->nIdx;` |
| 1478374 |  283 | `	}else{` |
|      35 |  284 | `		nIdx = nRefIdx;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Hash the key */` |
| 2956780 |  287 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  288 | `	/* Allocate a new int node */` |
| 2956780 |  289 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2956780 |  290 | `	if( pNode == 0 ){` |
|     ! 0 |  291 | `		return SXERR_MEM;` |
|       - |  292 | `	}` |
| 2956780 |  293 | `	if( isForeign ){` |
|       - |  294 | `		/* Mark as a foregin entry */` |
|      35 |  295 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      17 |  296 | `	}` |
|       - |  297 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2956780 |  298 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2956780 |  299 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  300 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  301 | `		return rc;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Perform the insertion */` |
| 2956780 |  304 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  305 | `	/* Install in the reference table */` |
| 2956780 |  306 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  307 | `	/* All done */` |
| 2956780 |  308 | `	return SXRET_OK;` |
| 1478391 |  309 |  |
|       - |  310 | `/*` |
|       - |  311 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  312 | ` * hashmap.` |
|       - |  313 | ` */` |
|  107732 |  314 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  315 |  |
|       - |  316 | `	ph7_hashmap_node *pNode;` |
|       - |  317 | `	sxu32 nHash;` |
|       - |  318 | `	sxu32 nIdx;` |
|       - |  319 | `	sxi32 rc;` |
|  107734 |  320 | `	if( !isForeign ){` |
|       - |  321 | `		ph7_value *pObj;` |
|       - |  322 | `		/* Reserve a ph7_value for the value */` |
|   72598 |  323 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   72598 |  324 | `		if( pObj == 0 ){` |
|     ! 0 |  325 | `			return SXERR_MEM;` |
|       - |  326 | `		}` |
|   72598 |  327 | `		if( pValue ){` |
|       - |  328 | `			/* Duplicate the value */` |
|   72326 |  329 | `			PH7_MemObjStore(pValue,pObj);` |
|   36162 |  330 | `		}` |
|   72598 |  331 | `		nIdx = pObj->nIdx;` |
|   36300 |  332 | `	}else{` |
|   35138 |  333 | `		nIdx = nRefIdx;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Hash the key */` |
|  107734 |  336 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  337 | `	/* Allocate a new blob node */` |
|  107734 |  338 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  107734 |  339 | `	if( pNode == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|  107734 |  342 | `	if( isForeign ){` |
|       - |  343 | `		/* Mark as a foregin entry */` |
|   35138 |  344 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   17568 |  345 | `	}` |
|       - |  346 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  107734 |  347 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  107734 |  348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  349 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  350 | `		return rc;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Perform the insertion */` |
|  107734 |  353 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  354 | `	/* Install in the reference table */` |
|  107734 |  355 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  356 | `	/* All done */` |
|  107734 |  357 | `	return SXRET_OK;` |
|   53868 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  361 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  362 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  363 | ` */` |
|   47828 |  364 | `static sxi32 HashmapLookupIntKey(` |
|       - |  365 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  366 | `	sxi64 iKey,                /* lookup key */` |
|       - |  367 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  368 | `	)` |
|       2 |  369 |  |
|       - |  370 | `	ph7_hashmap_node *pNode;` |
|       - |  371 | `	sxu32 nHash;` |
|   47830 |  372 | `	if( pMap->nEntry < 1 ){` |
|       - |  373 | `		/* Don't bother hashing,there is no entry anyway */` |
|     446 |  374 | `		return SXERR_NOTFOUND;` |
|       - |  375 | `	}` |
|       - |  376 | `	/* Hash the key first */` |
|   47386 |  377 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  378 | `	/* Point to the appropriate bucket */` |
|   47386 |  379 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  380 | `	/* Perform the lookup */` |
|  412047 |  381 | `	for(;;){` |
|  824096 |  382 | `		if( pNode == 0 ){` |
|   45998 |  383 | `			break;` |
|       - |  384 | `		}` |
|  778792 |  385 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  775083 |  386 | `			&& pNode->nHash == nHash` |
|  386730 |  387 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  388 | `				/* Node found */` |
|    1390 |  389 | `				if( ppNode ){` |
|    1378 |  390 | `					*ppNode = pNode;` |
|     688 |  391 | `				}` |
|    1390 |  392 | `				return SXRET_OK;` |
|       - |  393 | `		}` |
|       - |  394 | `		/* Follow the collision link */` |
|  776711 |  395 | `		pNode = pNode->pNextCollide;` |
|       1 |  396 | `	}` |
|       - |  397 | `	/* No such entry */` |
|   45998 |  398 | `	return SXERR_NOTFOUND;` |
|   23916 |  399 |  |
|       - |  400 | `/*` |
|       - |  401 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  402 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  403 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  404 | ` */` |
|  218680 |  405 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  406 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  407 | `	const void *pKey,           /* Lookup key */` |
|       - |  408 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  409 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  410 | `	)` |
|       2 |  411 |  |
|       - |  412 | `	ph7_hashmap_node *pNode;` |
|       - |  413 | `	sxu32 nHash;` |
|  218682 |  414 | `	if( pMap->nEntry < 1 ){` |
|       - |  415 | `		/* Don't bother hashing,there is no entry anyway */` |
|   13362 |  416 | `		return SXERR_NOTFOUND;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Hash the key first */` |
|  205322 |  419 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  420 | `	/* Point to the appropriate bucket */` |
|  205322 |  421 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  422 | `	/* Perform the lookup */` |
|  185359 |  423 | `	for(;;){` |
|  370720 |  424 | `		if( pNode == 0 ){` |
|  156838 |  425 | `			break;` |
|       - |  426 | `		}` |
|  238124 |  427 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  212383 |  428 | `			&& pNode->nHash == nHash` |
|  129684 |  429 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   48486 |  430 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  431 | `				/* Node found */` |
|   48486 |  432 | `				if( ppNode ){` |
|   48458 |  433 | `					*ppNode = pNode;` |
|   24228 |  434 | `				}` |
|   48486 |  435 | `				return SXRET_OK;` |
|       - |  436 | `		}` |
|       - |  437 | `		/* Follow the collision link */` |
|  165400 |  438 | `		pNode = pNode->pNextCollide;` |
|       2 |  439 | `	}` |
|       - |  440 | `	/* No such entry */` |
|  156838 |  441 | `	return SXERR_NOTFOUND;` |
|  109342 |  442 |  |
|       - |  443 | `/*` |
|       - |  444 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  445 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  446 | ` */` |
|  218820 |  447 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  448 |  |
|  218822 |  449 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  218822 |  450 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  218822 |  451 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  452 | `		/* Octal not decimal number */` |
|       5 |  453 | `		return FALSE;` |
|       - |  454 | `	}` |
|  218818 |  455 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  456 | `		zIn++;` |
|     ! 0 |  457 | `	}` |
|  109741 |  458 | `	for(;;){` |
|  219484 |  459 | `		if( zIn >= zEnd ){` |
|     233 |  460 | `			return TRUE;` |
|       - |  461 | `		}` |
|  219252 |  462 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  109294 |  463 | `			break;` |
|       - |  464 | `		}` |
|     667 |  465 | `		zIn++;` |
|       1 |  466 | `	}` |
|       - |  467 | `	/* Key does not look like a decimal number */` |
|  218586 |  468 | `	return FALSE;` |
|  109412 |  469 |  |
|       - |  470 | `/*` |
|       - |  471 | ` * Check if a given key exists in the given hashmap.` |
|       - |  472 | ` * Write a pointer to the target node on success.` |
|       - |  473 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  474 | ` */` |
|  112142 |  475 | `static sxi32 HashmapLookup(` |
|       - |  476 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  477 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  478 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  479 | `	)` |
|       2 |  480 |  |
|  112144 |  481 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  482 | `	sxi32 rc;` |
|  112144 |  483 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  110894 |  484 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  485 | `			/* Force a string cast */` |
|     ! 0 |  486 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  487 | `		}` |
|  110894 |  488 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  489 | `			/* Perform a blob lookup */` |
|  110878 |  490 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  110878 |  491 | `			goto result;` |
|       - |  492 | `		}` |
|       8 |  493 | `	}` |
|       - |  494 | `	/* Perform an int lookup */` |
|    1268 |  495 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  496 | `		/* Force an integer cast */` |
|      27 |  497 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  498 | `	}` |
|       - |  499 | `	/* Perform an int lookup */` |
|    1268 |  500 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   56071 |  501 | `result:` |
|  112144 |  502 | `	if( rc == SXRET_OK ){` |
|       - |  503 | `		/* Node found */` |
|   49610 |  504 | `		if( ppNode ){` |
|   49568 |  505 | `			*ppNode = pNode;` |
|   24783 |  506 | `		}` |
|   49610 |  507 | `		return SXRET_OK;` |
|       - |  508 | `	}` |
|       - |  509 | `	/* No such entry */` |
|   62536 |  510 | `	return SXERR_NOTFOUND;` |
|   56073 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  514 | ` * hashmap.` |
|       - |  515 | ` * If a node with the given key already exists in the database` |
|       - |  516 | ` * then this function overwrite the old value.` |
|       - |  517 | ` */` |
| 3029058 |  518 | `static sxi32 HashmapInsert(` |
|       - |  519 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  520 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  521 | `	ph7_value *pVal    /* Node value */` |
|       - |  522 | `	)` |
|       2 |  523 |  |
| 3029060 |  524 | `	ph7_hashmap_node *pNode = 0;` |
| 3029060 |  525 | `	sxi32 rc = SXRET_OK;` |
| 3029060 |  526 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   72826 |  527 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  528 | `			/* Force a string cast */` |
|       3 |  529 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  530 | `		}` |
|   72826 |  531 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  532 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  533 | `				/* Automatic index assign */` |
|      34 |  534 | `				pKey = 0;` |
|      16 |  535 | `			}` |
|     256 |  536 | `			goto IntKey;` |
|       - |  537 | `		}` |
|  108857 |  538 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   36285 |  539 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|   72502 |  553 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  554 | `			/* Forbidden */` |
|       3 |  555 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Perform a blob-key insertion */` |
|   72500 |  559 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   72500 |  560 | `		return rc;` |
|       - |  561 | `	}` |
| 1478117 |  562 | `IntKey:` |
| 2956490 |  563 | `	if( pKey ){` |
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
| 2933084 |  600 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  601 | `			/* Forbidden */` |
|       3 |  602 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  603 | `			return SXRET_OK;` |
|       - |  604 | `		}` |
|       - |  605 | `		/* Assign an automatic index */` |
| 2933082 |  606 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2933082 |  607 | `		if( rc == SXRET_OK ){` |
| 2933082 |  608 | `			++pMap->iNextIdx;` |
| 1466540 |  609 | `		}` |
|       - |  610 | `	}` |
|       - |  611 | `	/* Insertion result */` |
| 2956400 |  612 | `	return rc;` |
| 1514531 |  613 |  |
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
|   35176 |  641 | `static sxi32 HashmapInsertByRef(` |
|       - |  642 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  643 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  644 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  645 | `	)` |
|       2 |  646 |  |
|   35178 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|   35178 |  648 | `	sxi32 rc = SXRET_OK;` |
|   35178 |  649 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   35144 |  650 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  651 | `			/* Force a string cast */` |
|     ! 0 |  652 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  653 | `		}` |
|   35144 |  654 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  655 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  656 | `				/* Automatic index assign */` |
|     ! 0 |  657 | `				pKey = 0;` |
|     ! 0 |  658 | `			}` |
|     ! 0 |  659 | `			goto IntKey;` |
|       - |  660 | `		}` |
|   52715 |  661 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   17571 |  662 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  663 | `				/* Overwrite */` |
|       7 |  664 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  665 | `				pNode->nValIdx = nRefIdx;` |
|       - |  666 | `				/* Install in the reference table */` |
|       7 |  667 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  668 | `				return SXRET_OK;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Perform a blob-key insertion */` |
|   35138 |  671 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   35138 |  672 | `		return rc;` |
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
|   17590 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * Extract node value.` |
|       - |  712 | ` */` |
| 1187132 |  713 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  714 |  |
|       - |  715 | `	/* Point to the desired object */` |
|       - |  716 | `	ph7_value *pObj;` |
| 1187134 |  717 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1187134 |  718 | `	return pObj;` |
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
|   60015 |  764 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  765 |  |
|       - |  766 | `	ph7_value sObj1,sObj2;` |
|       - |  767 | `	sxi32 rc;` |
|   60017 |  768 | `	if( pLeft == pRight ){` |
|       - |  769 | `		/*` |
|       - |  770 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  771 | `		 * below for more information on this sceanario.` |
|       - |  772 | `		 */` |
|     ! 0 |  773 | `		return 0;` |
|       - |  774 | `	}` |
|       - |  775 | `	/* Do the comparison */` |
|   60017 |  776 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   60017 |  777 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   60017 |  778 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   60017 |  779 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   60017 |  780 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   60017 |  781 | `	PH7_MemObjRelease(&sObj1);` |
|   60017 |  782 | `	PH7_MemObjRelease(&sObj2);` |
|   60017 |  783 | `	return rc;` |
|   30013 |  784 |  |
|       - |  785 | `/*` |
|       - |  786 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  787 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  788 | ` */` |
|   11668 |  789 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  790 |  |
|   11670 |  791 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  792 | `	sxu32 nBucket;` |
|       - |  793 | `	/* Remove old collision links */` |
|   11670 |  794 | `	if( pEntry->pPrevCollide ){` |
|    9403 |  795 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    4708 |  796 | `	}else{` |
|    2269 |  797 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  798 | `	}` |
|   11670 |  799 | `	if( pEntry->pNextCollide ){` |
|     891 |  800 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     433 |  801 | `	}` |
|   11670 |  802 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  803 | `	/* Compute the new hash */` |
|   11670 |  804 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   11670 |  805 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   11670 |  806 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  807 | `	/* Link to the new bucket */` |
|   11670 |  808 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11670 |  809 | `	if( pMap->apBucket[nBucket] ){` |
|    9652 |  810 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    4833 |  811 | `	}` |
|   11670 |  812 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   11670 |  813 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  814 | `	/* Increment the automatic index */` |
|   11670 |  815 | `	pMap->iNextIdx++;` |
|   11670 |  816 |  |
|       - |  817 | `/*` |
|       - |  818 | ` * Perform a linear search on a given hashmap.` |
|       - |  819 | ` * Write a pointer to the target node on success.` |
|       - |  820 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  821 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  822 | ` * for more information.` |
|       - |  823 | ` */` |
|   29182 |  824 | `static int HashmapFindValue(` |
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
|   29184 |  837 | `	pEntry = pMap->pFirst;` |
|   29184 |  838 | `	n = pMap->nEntry;` |
|   29184 |  839 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   29184 |  840 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   69930 |  841 | `	for(;;){` |
|  139862 |  842 | `		if( n < 1 ){` |
|      99 |  843 | `			break;` |
|       - |  844 | `		}` |
|       - |  845 | `		/* Extract node value */` |
|  139764 |  846 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  139764 |  847 | `		if( pVal ){` |
|  139764 |  848 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|  139764 |  860 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  139764 |  861 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  139764 |  862 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  139764 |  863 | `				PH7_MemObjRelease(&sVal);` |
|  139764 |  864 | `				PH7_MemObjRelease(&sNeedle);` |
|  139764 |  865 | `				if( rc == 0 ){` |
|   29086 |  866 | `					if( ppNode ){` |
|      23 |  867 | `						*ppNode = pEntry;` |
|      11 |  868 | `					}` |
|       - |  869 | `					/* Match found*/` |
|   29086 |  870 | `					return SXRET_OK;` |
|       - |  871 | `				}` |
|       - |  872 | `			}` |
|   55339 |  873 | `		}` |
|       - |  874 | `		/* Point to the next entry */` |
|  110680 |  875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  110680 |  876 | `		n--;` |
|       2 |  877 | `	}` |
|       - |  878 | `	/* No such entry */` |
|      99 |  879 | `	return SXERR_NOTFOUND;` |
|   14593 |  880 |  |
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
|  548928 | 1054 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1055 | `	ph7_hashmap *pDest,` |
|       - | 1056 | `	ph7_hashmap_node *pEntry,` |
|       - | 1057 | `	ph7_value *pVal,` |
|       - | 1058 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1059 | `	)` |
|       2 | 1060 |  |
|       - | 1061 | `	ph7_value sSafeVal;` |
|       - | 1062 | `	ph7_value sKey;` |
|       - | 1063 | `	sxi32 rc;` |
|       - | 1064 |  |
|  548930 | 1065 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 1066 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|       - | 1067 | `		 * Re-insert it by reference so the reference survives the duplication` |
|       - | 1068 | `		 * instead of being flattened to a value copy. This keeps spread` |
|       - | 1069 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|       - | 1070 | `		 * with PHP semantics. */` |
|       7 | 1071 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|       7 | 1072 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       3 | 1073 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|       3 | 1074 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|       3 | 1075 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       3 | 1076 | `			PH7_MemObjRelease(&sKey);` |
|       2 | 1077 | `		}else{` |
|       5 | 1078 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|       5 | 1079 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|       2 | 1080 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|     ! 0 | 1081 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|     ! 0 | 1082 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|     ! 0 | 1083 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 1084 | `			}else{ /* Dup: preserve the int key */` |
|     ! 0 | 1085 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|       - | 1086 | `			}` |
|       - | 1087 | `		}` |
|       7 | 1088 | `		return rc;` |
|       - | 1089 | `	}` |
|  548924 | 1090 | `	sSafeVal = *pVal;` |
|       - | 1091 |  |
|  548924 | 1092 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1093 | `		/* Blob key insertion */` |
|      91 | 1094 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      91 | 1095 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      91 | 1096 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      91 | 1097 | `		PH7_MemObjRelease(&sKey);` |
|      46 | 1098 | `	}else{` |
|       - | 1099 | `		/* Int key */` |
|  548834 | 1100 | `		if( iAction == 0 ){ /* Merge */` |
|  548612 | 1101 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  274529 | 1102 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1103 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1104 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1105 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1106 | `		}else{ /* Dup */` |
|     194 | 1107 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1108 | `		}` |
|       - | 1109 | `	}` |
|  548924 | 1110 | `	return rc;` |
|  274466 | 1111 |  |
|       - | 1112 | `/*` |
|       - | 1113 | ` * Merge two hashmaps.` |
|       - | 1114 | ` * Note on the merge process` |
|       - | 1115 | ` * According to the PHP language reference manual.` |
|       - | 1116 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|       - | 1117 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|       - | 1118 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|       - | 1119 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|       - | 1120 | ` *  the later value will not overwrite the original value, but will be appended.` |
|       - | 1121 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|       - | 1122 | ` *  keys starting from zero in the result array.` |
|       - | 1123 | ` */` |
|    1952 | 1124 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1125 |  |
|       - | 1126 | `	ph7_hashmap_node *pEntry;` |
|       - | 1127 | `	ph7_value *pVal;` |
|       - | 1128 | `	sxi32 rc;` |
|       - | 1129 | `	sxu32 n;` |
|    1954 | 1130 | `	if( pSrc == pDest ){` |
|       - | 1131 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1132 | `		 * Unlike the zend engine.` |
|       - | 1133 | `		 */` |
|     ! 0 | 1134 | `		return SXRET_OK;` |
|       - | 1135 | `	}` |
|       - | 1136 | `	/* Point to the first inserted entry in the source */` |
|    1954 | 1137 | `	pEntry = pSrc->pFirst;` |
|       - | 1138 | `	/* Perform the merge */` |
|  550618 | 1139 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1140 | `		/* Extract the node value */` |
|  548666 | 1141 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  548666 | 1142 | `		if( pVal ){` |
|       - | 1143 | `			/* Make a local copy of the value.` |
|       - | 1144 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1145 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1146 | `			 * to the old pool.` |
|       - | 1147 | `			 */` |
|  548666 | 1148 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  274334 | 1149 | `		}else{` |
|     ! 0 | 1150 | `			rc = SXRET_OK;` |
|       - | 1151 | `		}` |
|  548666 | 1152 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1153 | `			return rc;` |
|       - | 1154 | `		}` |
|       - | 1155 | `		/* Point to the next entry */` |
|  548666 | 1156 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  274334 | 1157 | `	}` |
|    1954 | 1158 | `	return SXRET_OK;` |
|     978 | 1159 |  |
|       - | 1160 | `/*` |
|       - | 1161 | ` * Overwrite entries with the same key.` |
|       - | 1162 | ` * Refer to the [array_replace()] implementation for more information.` |
|       - | 1163 | ` *  According to the PHP language reference manual.` |
|       - | 1164 | ` *  array_replace() replaces the values of the first array with the same values` |
|       - | 1165 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|       - | 1166 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|       - | 1167 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|       - | 1168 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|       - | 1169 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|       - | 1170 | ` *  overwriting the previous values.` |
|       - | 1171 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|       - | 1172 | ` *  by whatever type is in the second array.` |
|       - | 1173 | ` */` |
|      34 | 1174 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1175 |  |
|       - | 1176 | `	ph7_hashmap_node *pEntry;` |
|       - | 1177 | `	ph7_value *pVal;` |
|       - | 1178 | `	sxi32 rc;` |
|       - | 1179 | `	sxu32 n;` |
|      36 | 1180 | `	if( pSrc == pDest ){` |
|       - | 1181 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1182 | `		 * Unlike the zend engine.` |
|       - | 1183 | `		 */` |
|     ! 0 | 1184 | `		return SXRET_OK;` |
|       - | 1185 | `	}` |
|       - | 1186 | `	/* Point to the first inserted entry in the source */` |
|      36 | 1187 | `	pEntry = pSrc->pFirst;` |
|       - | 1188 | `	/* Perform the merge */` |
|      80 | 1189 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1190 | `		/* Extract the node value */` |
|      46 | 1191 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      46 | 1192 | `		if( pVal ){` |
|      46 | 1193 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      24 | 1194 | `		}else{` |
|     ! 0 | 1195 | `			rc = SXRET_OK;` |
|       - | 1196 | `		}` |
|      46 | 1197 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1198 | `			return rc;` |
|       - | 1199 | `		}` |
|       - | 1200 | `		/* Point to the next entry */` |
|      46 | 1201 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 1202 | `	}` |
|      36 | 1203 | `	return SXRET_OK;` |
|      19 | 1204 |  |
|       - | 1205 | `/*` |
|       - | 1206 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|       - | 1207 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|       - | 1208 | ` */` |
|     104 | 1209 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1210 |  |
|       - | 1211 | `	ph7_hashmap_node *pEntry;` |
|       - | 1212 | `	ph7_value *pVal;` |
|       - | 1213 | `	sxi32 rc;` |
|       - | 1214 | `	sxu32 n;` |
|     106 | 1215 | `	if( pSrc == pDest ){` |
|       - | 1216 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1217 | `		 * Unlike the zend engine.` |
|       - | 1218 | `		 */` |
|     ! 0 | 1219 | `		return SXRET_OK;` |
|       - | 1220 | `	}` |
|       - | 1221 | `	/* Point to the first inserted entry in the source */` |
|     106 | 1222 | `	pEntry = pSrc->pFirst;` |
|       - | 1223 | `	/* Perform the duplication */` |
|     326 | 1224 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1225 | `		/* Extract the node value */` |
|     222 | 1226 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     222 | 1227 | `		if( pVal ){` |
|     222 | 1228 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|     112 | 1229 | `		}else{` |
|     ! 0 | 1230 | `			rc = SXRET_OK;` |
|       - | 1231 | `		}` |
|     222 | 1232 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1233 | `			return rc;` |
|       - | 1234 | `		}` |
|       - | 1235 | `		/* Point to the next entry */` |
|     222 | 1236 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     112 | 1237 | `	}` |
|     106 | 1238 | `	return SXRET_OK;` |
|      54 | 1239 |  |
|       - | 1240 | `/*` |
|       - | 1241 | ` * Copy-on-write separation for arrays.` |
|       - | 1242 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|       - | 1243 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|       - | 1244 | ` * Returns the (possibly new) hashmap pointer.` |
|       - | 1245 | ` */` |
|  193568 | 1246 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       2 | 1247 |  |
|  193570 | 1248 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1249 | `	ph7_hashmap *pNew;` |
|       - | 1250 | `	ph7_value *pBacking;` |
|  193570 | 1251 | `	if( pMap->iRef < 2 ){` |
|       - | 1252 | `		/* Sole owner, no separation needed */` |
|  191496 | 1253 | `		return pMap;` |
|       - | 1254 | `	}` |
|    2076 | 1255 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1256 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1257 | `		return pMap;` |
|       - | 1258 | `	}` |
|       - | 1259 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1260 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1261 | `	 * frame is popped. */` |
|    2076 | 1262 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2076 | 1263 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    3094 | 1264 | `		if( pBacking && pBacking != pValue` |
|    2057 | 1265 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2042 | 1266 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1267 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2042 | 1268 | `			pMap->iRef--;` |
|    2042 | 1269 | `			if( pMap->iRef < 2 ){` |
|       - | 1270 | `				/* After undoing stack ref, sole owner — no separation */` |
|    2006 | 1271 | `				pMap->iRef++;` |
|    2006 | 1272 | `				return pMap;` |
|       - | 1273 | `			}` |
|      38 | 1274 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|      38 | 1275 | `			if( pNew == 0 ){` |
|     ! 0 | 1276 | `				pMap->iRef++;` |
|     ! 0 | 1277 | `				return pMap;` |
|       - | 1278 | `			}` |
|      38 | 1279 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1280 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|     ! 0 | 1281 | `				PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1282 | `				pMap->iRef++;` |
|     ! 0 | 1283 | `				return pMap;` |
|       - | 1284 | `			}` |
|      38 | 1285 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|      38 | 1286 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|      38 | 1287 | `			pBacking->x.pOther = pNew;` |
|       - | 1288 | `			/* Update the stack value to match */` |
|      38 | 1289 | `			pValue->x.pOther = pNew;` |
|      38 | 1290 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|      38 | 1291 | `			return pNew;` |
|       - | 1292 | `		}` |
|      17 | 1293 | `	}` |
|      35 | 1294 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      35 | 1295 | `	if( pNew == 0 ){` |
|       - | 1296 | `		/* Allocation failure — fall through with shared map */` |
|     ! 0 | 1297 | `		return pMap;` |
|       - | 1298 | `	}` |
|      35 | 1299 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1300 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|     ! 0 | 1301 | `		PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1302 | `		return pMap;` |
|       - | 1303 | `	}` |
|      35 | 1304 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      35 | 1305 | `	pMap->iRef--;` |
|      35 | 1306 | `	pValue->x.pOther = pNew;` |
|      35 | 1307 | `	return pNew;` |
|   96786 | 1308 |  |
|       - | 1309 | `/*` |
|       - | 1310 | ` * Perform the union of two hashmaps.` |
|       - | 1311 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1312 | ` * with a variable holding an array as follows:` |
|       - | 1313 | ` * <?php` |
|       - | 1314 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1315 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1316 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1317 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1318 | ` * var_dump($c);` |
|       - | 1319 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1320 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1321 | ` * var_dump($c);` |
|       - | 1322 | ` * ?>` |
|       - | 1323 | ` * When executed, this script will print the following:` |
|       - | 1324 | ` * Union of $a and $b:` |
|       - | 1325 | ` * array(3) {` |
|       - | 1326 | ` *  ["a"]=>` |
|       - | 1327 | ` *  string(5) "apple"` |
|       - | 1328 | ` *  ["b"]=>` |
|       - | 1329 | ` * string(6) "banana"` |
|       - | 1330 | ` *  ["c"]=>` |
|       - | 1331 | ` * string(6) "cherry"` |
|       - | 1332 | ` * }` |
|       - | 1333 | ` * Union of $b and $a:` |
|       - | 1334 | ` * array(3) {` |
|       - | 1335 | ` * ["a"]=>` |
|       - | 1336 | ` * string(4) "pear"` |
|       - | 1337 | ` * ["b"]=>` |
|       - | 1338 | ` * string(10) "strawberry"` |
|       - | 1339 | ` * ["c"]=>` |
|       - | 1340 | ` * string(6) "cherry"` |
|       - | 1341 | ` * }` |
|       - | 1342 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1343 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1344 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1345 | ` */` |
|      10 | 1346 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       2 | 1347 |  |
|       - | 1348 | `	ph7_hashmap_node *pEntry;` |
|      12 | 1349 | `	sxi32 rc = SXRET_OK;` |
|       - | 1350 | `	ph7_value *pObj;` |
|       - | 1351 | `	sxu32 n;` |
|      12 | 1352 | `	if( pLeft == pRight ){` |
|       - | 1353 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1354 | `		 * Unlike the zend engine.` |
|       - | 1355 | `		 */` |
|     ! 0 | 1356 | `		return SXRET_OK;` |
|       - | 1357 | `	}` |
|       - | 1358 | `	/* Perform the union */` |
|      12 | 1359 | `	pEntry = pRight->pFirst;` |
|      32 | 1360 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1361 | `		/* Make sure the given key does not exists in the left array */` |
|      22 | 1362 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1363 | `			/* BLOB key */` |
|       7 | 1364 | `			if( SXRET_OK !=` |
|       6 | 1365 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|       3 | 1366 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|       3 | 1367 | `					if( pObj ){` |
|       3 | 1368 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1369 | `						/* Perform the insertion */` |
|       3 | 1370 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1371 | `							&sSafeVal,0,FALSE);` |
|       3 | 1372 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1373 | `							return rc;` |
|       - | 1374 | `						}` |
|       1 | 1375 | `					}` |
|       1 | 1376 | `			}` |
|       4 | 1377 | `		}else{` |
|       - | 1378 | `			/* INT key */` |
|      16 | 1379 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|      11 | 1380 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|      11 | 1381 | `				if( pObj ){` |
|      11 | 1382 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1383 | `					/* Perform the insertion */` |
|      11 | 1384 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      11 | 1385 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1386 | `						return rc;` |
|       - | 1387 | `					}` |
|       5 | 1388 | `				}` |
|       5 | 1389 | `			}` |
|       - | 1390 | `		}` |
|       - | 1391 | `		/* Point to the next entry */` |
|      22 | 1392 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      12 | 1393 | `	}` |
|      12 | 1394 | `	return SXRET_OK;` |
|       7 | 1395 |  |
|       - | 1396 | `/*` |
|       - | 1397 | ` * Allocate a new hashmap.` |
|       - | 1398 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1399 | ` */` |
|   85242 | 1400 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1401 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1402 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1403 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1404 | `	)` |
|       2 | 1405 |  |
|       - | 1406 | `	ph7_hashmap *pMap;` |
|       - | 1407 | `	/* Allocate a new instance */` |
|   85244 | 1408 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   85244 | 1409 | `	if( pMap == 0 ){` |
|     ! 0 | 1410 | `		return 0;` |
|       - | 1411 | `	}` |
|       - | 1412 | `	/* Zero the structure */` |
|   85244 | 1413 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1414 | `	/* Fill in the structure */` |
|   85244 | 1415 | `	pMap->pVm = &(*pVm);` |
|   85244 | 1416 | `	pMap->iRef = 1;` |
|       - | 1417 | `	/* Default hash functions */` |
|   85244 | 1418 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   85244 | 1419 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   85244 | 1420 | `	return pMap;` |
|   42623 | 1421 |  |
|       - | 1422 | `/*` |
|       - | 1423 | ` * Install superglobals in the given virtual machine.` |
|       - | 1424 | ` * Note on superglobals.` |
|       - | 1425 | ` *  According to the PHP language reference manual.` |
|       - | 1426 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1427 | `*   Description` |
|       - | 1428 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1429 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1430 | `*   global $variable; to access them within functions or methods.` |
|       - | 1431 | `*   These superglobal variables are:` |
|       - | 1432 | `*    $GLOBALS` |
|       - | 1433 | `*    $_SERVER` |
|       - | 1434 | `*    $_GET` |
|       - | 1435 | `*    $_POST` |
|       - | 1436 | `*    $_FILES` |
|       - | 1437 | `*    $_COOKIE` |
|       - | 1438 | `*    $_SESSION` |
|       - | 1439 | `*    $_REQUEST` |
|       - | 1440 | `*    $_ENV` |
|       - | 1441 | `*/` |
|    2808 | 1442 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       2 | 1443 |  |
|       - | 1444 | `	static const char * azSuper[] = {` |
|       - | 1445 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1446 | `		"_GET",      /* $_GET */` |
|       - | 1447 | `		"_POST",     /* $_POST */` |
|       - | 1448 | `		"_FILES",    /* $_FILES */` |
|       - | 1449 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1450 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1451 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1452 | `		"_ENV",      /* $_ENV */` |
|       - | 1453 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1454 | `		"argv"       /* $argv */` |
|       - | 1455 | `	};` |
|       - | 1456 | `	ph7_hashmap *pMap;` |
|       - | 1457 | `	ph7_value *pObj;` |
|       - | 1458 | `	SyString *pFile;` |
|       - | 1459 | `	sxi32 rc;` |
|       - | 1460 | `	sxu32 n;` |
|       - | 1461 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    2810 | 1462 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    2810 | 1463 | `	if( pMap == 0 ){` |
|     ! 0 | 1464 | `		return SXERR_MEM;` |
|       - | 1465 | `	}` |
|    2810 | 1466 | `	pVm->pGlobal = pMap;` |
|       - | 1467 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    2810 | 1468 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    2810 | 1469 | `	if( pObj == 0 ){` |
|     ! 0 | 1470 | `		return SXERR_MEM;` |
|       - | 1471 | `	}` |
|    2810 | 1472 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1473 | `	/* Record object index */` |
|    2810 | 1474 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1475 | `	/* Install the special $GLOBALS array */` |
|    2810 | 1476 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    2810 | 1477 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1478 | `		return rc;` |
|       - | 1479 | `	}` |
|       - | 1480 | `	/* Install superglobals now */` |
|   30890 | 1481 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1482 | `		ph7_value *pSuper;` |
|       - | 1483 | `		/* Request an empty array */` |
|   28082 | 1484 | `		pSuper = ph7_new_array(&(*pVm));` |
|   28082 | 1485 | `		if( pSuper == 0 ){` |
|     ! 0 | 1486 | `			return SXERR_MEM;` |
|       - | 1487 | `		}` |
|       - | 1488 | `		/* Install */` |
|   28082 | 1489 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   28082 | 1490 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1491 | `			return rc;` |
|       - | 1492 | `		}` |
|       - | 1493 | `		/* Release the value now it have been installed */` |
|   28082 | 1494 | `		ph7_release_value(&(*pVm),pSuper);` |
|   14042 | 1495 | `	}` |
|       - | 1496 | `	/* Set some $_SERVER entries */` |
|    2810 | 1497 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1498 | `	/*` |
|       - | 1499 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1500 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1501 | `	 */` |
|    5614 | 1502 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1503 | `		"SCRIPT_FILENAME",` |
|    1404 | 1504 | `		pFile ? pFile->zString : ":Memory:",` |
|    2804 | 1505 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1506 | `		);` |
|       - | 1507 | `	/* All done,all super-global are installed now */` |
|    2810 | 1508 | `	return SXRET_OK;` |
|    1406 | 1509 |  |
|       - | 1510 | `/*` |
|       - | 1511 | ` * Release a hashmap.` |
|       - | 1512 | ` */` |
|   54118 | 1513 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1514 |  |
|       - | 1515 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   54120 | 1516 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1517 | `	sxu32 n;` |
|   54120 | 1518 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1519 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1520 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1521 | `		return SXRET_OK;` |
|       - | 1522 | `	}` |
|       - | 1523 | `	/* Start the release process */` |
|   54120 | 1524 | `	n = 0;` |
|   54120 | 1525 | `	pEntry = pMap->pFirst;` |
| 1536602 | 1526 | `	for(;;){` |
| 3073206 | 1527 | `		if( n >= pMap->nEntry ){` |
|   54120 | 1528 | `			break;` |
|       - | 1529 | `		}` |
| 3019088 | 1530 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1531 | `		/* Remove the reference from the foreign table */` |
| 3019088 | 1532 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3019088 | 1533 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1534 | `			/* Restore the ph7_value to the free list */` |
| 3019080 | 1535 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1509539 | 1536 | `		}` |
|       - | 1537 | `		/* Release the node */` |
| 3019088 | 1538 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   68440 | 1539 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   34219 | 1540 | `		}` |
| 3019088 | 1541 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1542 | `		/* Point to the next entry */` |
| 3019088 | 1543 | `		pEntry = pNext;` |
| 3019088 | 1544 | `		n++;` |
|       2 | 1545 | `	}` |
|   54120 | 1546 | `	if( pMap->nEntry > 0 ){` |
|       - | 1547 | `		/* Release the hash bucket */` |
|   48082 | 1548 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   24040 | 1549 | `	}` |
|   54120 | 1550 | `	if( FreeDS ){` |
|       - | 1551 | `		/* Free the whole instance */` |
|   54104 | 1552 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   27053 | 1553 | `	}else{` |
|       - | 1554 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1555 | `		pMap->apBucket = 0;` |
|      17 | 1556 | `		pMap->iNextIdx = 0;` |
|      17 | 1557 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1558 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1559 | `	}` |
|   54120 | 1560 | `	return SXRET_OK;` |
|   27061 | 1561 |  |
|       - | 1562 | `/*` |
|       - | 1563 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1564 | ` * If the count reaches zero which mean no more variables` |
|       - | 1565 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1566 | ` */` |
|  597126 | 1567 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1568 |  |
|  597128 | 1569 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1570 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  597128 | 1571 | `	pMap->iRef--;` |
|  597128 | 1572 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   54088 | 1573 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   27043 | 1574 | `	}` |
|  597128 | 1575 |  |
|       - | 1576 | `/*` |
|       - | 1577 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1578 | ` * Write a pointer to the target node on success.` |
|       - | 1579 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1580 | ` */` |
|  112182 | 1581 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1582 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1583 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1584 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1585 | `	)` |
|       2 | 1586 |  |
|       - | 1587 | `	sxi32 rc;` |
|  112184 | 1588 | `	if( pMap->nEntry < 1 ){` |
|       - | 1589 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1590 | `		 */` |
|      42 | 1591 | `		return SXERR_NOTFOUND;` |
|       - | 1592 | `	}` |
|  112144 | 1593 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  112144 | 1594 | `	return rc;` |
|   56093 | 1595 |  |
|       - | 1596 | `/*` |
|       - | 1597 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1598 | ` * hashmap.` |
|       - | 1599 | ` * If a node with the given key already exists in the database` |
|       - | 1600 | ` * then this function overwrite the old value.` |
|       - | 1601 | ` */` |
| 2480236 | 1602 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1603 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1604 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1605 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1606 | `	)` |
|       2 | 1607 |  |
|       - | 1608 | `	sxi32 rc;` |
| 2480238 | 1609 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1610 | `		/*` |
|       - | 1611 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1612 | `		 */` |
|     ! 0 | 1613 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1614 | `		return SXRET_OK;` |
|       - | 1615 | `	}` |
| 2480238 | 1616 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2480238 | 1617 | `	return rc;` |
| 1240120 | 1618 |  |
|       - | 1619 | `/*` |
|       - | 1620 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|       - | 1621 | ` *   - String keys overwrite same-key entries in pDest.` |
|       - | 1622 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|       - | 1623 | ` * This is the same routine that backs array_merge().` |
|       - | 1624 | ` */` |
|      52 | 1625 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1626 |  |
|      53 | 1627 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|       1 | 1628 |  |
|       - | 1629 | `/*` |
|       - | 1630 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1631 | ` * hashmap.` |
|       - | 1632 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1633 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1634 | ` * The insertion by reference is triggered when the following` |
|       - | 1635 | ` * expression is encountered.` |
|       - | 1636 | ` * $var = 10;` |
|       - | 1637 | ` *  $a = array(&var);` |
|       - | 1638 | ` * OR` |
|       - | 1639 | ` *  $a[] =& $var;` |
|       - | 1640 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1641 | ` * over it's contents.` |
|       - | 1642 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1643 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1644 | ` * Example:` |
|       - | 1645 | ` *  $var = 10;` |
|       - | 1646 | ` *  $a[] =& $var;` |
|       - | 1647 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1648 | ` *  //Unset the foreign ph7_value now` |
|       - | 1649 | ` *  unset($var);` |
|       - | 1650 | ` *  echo count($a); //0` |
|       - | 1651 | ` * Note that this is a PH7 eXtension.` |
|       - | 1652 | ` * Refer to the official documentation for more information.` |
|       - | 1653 | ` * If a node with the given key already exists in the database` |
|       - | 1654 | ` * then this function overwrite the old value.` |
|       - | 1655 | ` */` |
|   35170 | 1656 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1657 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1658 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1659 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1660 | `	)` |
|       2 | 1661 |  |
|       - | 1662 | `	sxi32 rc;` |
|   35172 | 1663 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1664 | `		/*` |
|       - | 1665 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1666 | `		 */` |
|     ! 0 | 1667 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1668 | `		return SXRET_OK;` |
|       - | 1669 | `	}` |
|   35172 | 1670 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   35172 | 1671 | `	return rc;` |
|   17587 | 1672 |  |
|       - | 1673 | `/*` |
|       - | 1674 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1675 | ` */` |
|   24222 | 1676 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1677 |  |
|       - | 1678 | `	/* Reset the loop cursor */` |
|   24224 | 1679 | `	pMap->pCur = pMap->pFirst;` |
|   24224 | 1680 |  |
|       - | 1681 | `/*` |
|       - | 1682 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1683 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1684 | ` * return NULL.` |
|       - | 1685 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1686 | ` */` |
|  199276 | 1687 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1688 |  |
|  199278 | 1689 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  199278 | 1690 | `	if( pCur == 0 ){` |
|       - | 1691 | `		/* End of the list,return null */` |
|   12132 | 1692 | `		return 0;` |
|       - | 1693 | `	}` |
|       - | 1694 | `	/* Advance the node cursor */` |
|  187148 | 1695 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  187148 | 1696 | `	return pCur;` |
|   99640 | 1697 |  |
|       - | 1698 | `/*` |
|       - | 1699 | ` * Extract a node value.` |
|       - | 1700 | ` */` |
|  474380 | 1701 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1702 |  |
|  474382 | 1703 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  474382 | 1704 | `	if( pEntry ){` |
|  474382 | 1705 | `		if( bStore ){` |
|  187286 | 1706 | `			PH7_MemObjStore(pEntry,pValue);` |
|   93644 | 1707 | `		}else{` |
|  287098 | 1708 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1709 | `		}` |
|  237199 | 1710 | `	}else{` |
|     ! 0 | 1711 | `		PH7_MemObjRelease(pValue);` |
|       - | 1712 | `	}` |
|  474382 | 1713 |  |
|       - | 1714 | `/*` |
|       - | 1715 | ` * Extract a node key.` |
|       - | 1716 | ` */` |
|  117978 | 1717 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1718 |  |
|       - | 1719 | `	/* Fill with the current key */` |
|  117980 | 1720 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  117660 | 1721 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1722 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1723 | `		}` |
|  117660 | 1724 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  117660 | 1725 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   58831 | 1726 | `	}else{` |
|     322 | 1727 | `		SyBlobReset(&pKey->sBlob);` |
|     322 | 1728 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     322 | 1729 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1730 | `	}` |
|  117980 | 1731 |  |
|       - | 1732 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1733 | `/*` |
|       - | 1734 | ` * Store the address of nodes value in the given container.` |
|       - | 1735 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1736 | ` * defined in 'builtin.c' for more information.` |
|       - | 1737 | ` */` |
|      10 | 1738 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1739 |  |
|      11 | 1740 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1741 | `	ph7_value *pValue;` |
|       - | 1742 | `	sxu32 n;` |
|       - | 1743 | `	/* Initialize the container */` |
|      11 | 1744 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1745 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1746 | `		/* Extract node value */` |
|      17 | 1747 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1748 | `		if( pValue ){` |
|      17 | 1749 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1750 | `		}` |
|       - | 1751 | `		/* Point to the next entry */` |
|      17 | 1752 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1753 | `	}` |
|       - | 1754 | `	/* Total inserted entries */` |
|      11 | 1755 | `	return (int)SySetUsed(pOut);` |
|       1 | 1756 |  |
|       - | 1757 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1758 | `/*` |
|       - | 1759 | ` * Merge sort.` |
|       - | 1760 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1761 | ` * Status: Public domain` |
|       - | 1762 | ` */` |
|       - | 1763 | `/* Node comparison callback signature */` |
|       - | 1764 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1765 | `/*` |
|       - | 1766 | `** Inputs:` |
|       - | 1767 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1768 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1769 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1770 | `**` |
|       - | 1771 | `** Return Value:` |
|       - | 1772 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1773 | `**   of both a and b.` |
|       - | 1774 | `**` |
|       - | 1775 | `** Side effects:` |
|       - | 1776 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1777 | `**   changed.` |
|       - | 1778 | `*/` |
|   30968 | 1779 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1780 |  |
|       - | 1781 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1782 | `    /* Prevent compiler warning */` |
|   30970 | 1783 | `	result.pNext = result.pPrev = 0;` |
|   30970 | 1784 | `	pTail = &result;` |
|   91126 | 1785 | `	while( pA && pB ){` |
|   60158 | 1786 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   40316 | 1787 | `			pTail->pPrev = pA;` |
|   40316 | 1788 | `			pA->pNext = pTail;` |
|   40316 | 1789 | `			pTail = pA;` |
|   40316 | 1790 | `			pA = pA->pPrev;` |
|   20127 | 1791 | `		}else{` |
|   19844 | 1792 | `			pTail->pPrev = pB;` |
|   19844 | 1793 | `			pB->pNext = pTail;` |
|   19844 | 1794 | `			pTail = pB;` |
|   19844 | 1795 | `			pB = pB->pPrev;` |
|       - | 1796 | `		}` |
|       2 | 1797 | `	}` |
|   30970 | 1798 | `	if( pA ){` |
|   22111 | 1799 | `		pTail->pPrev = pA;` |
|   22111 | 1800 | `		pA->pNext = pTail;` |
|   19937 | 1801 | `	}else if( pB ){` |
|    8643 | 1802 | `		pTail->pPrev = pB;` |
|    8643 | 1803 | `		pB->pNext = pTail;` |
|    4301 | 1804 | `	}else{` |
|     220 | 1805 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1806 | `	}` |
|   30970 | 1807 | `	return result.pPrev;` |
|       2 | 1808 |  |
|       - | 1809 | `/*` |
|       - | 1810 | `** Inputs:` |
|       - | 1811 | `**   Map:       Input hashmap` |
|       - | 1812 | `**   cmp:       A comparison function.` |
|       - | 1813 | `**` |
|       - | 1814 | `** Return Value:` |
|       - | 1815 | `**   Sorted hashmap.` |
|       - | 1816 | `**` |
|       - | 1817 | `** Side effects:` |
|       - | 1818 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1819 | `*/` |
|       - | 1820 | `#define N_SORT_BUCKET  32` |
|     656 | 1821 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1822 |  |
|       - | 1823 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1824 | `	sxu32 i;` |
|     658 | 1825 | `	SyZero(a,sizeof(a));` |
|       - | 1826 | `	/* Point to the first inserted entry */` |
|     658 | 1827 | `	pIn = pMap->pFirst;` |
|   12442 | 1828 | `	while( pIn ){` |
|   11786 | 1829 | `		p = pIn;` |
|   11786 | 1830 | `		pIn = p->pPrev;` |
|   11786 | 1831 | `		p->pPrev = 0;` |
|   22418 | 1832 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   22418 | 1833 | `			if( a[i]==0 ){` |
|   11786 | 1834 | `				a[i] = p;` |
|   11786 | 1835 | `				break;` |
|     ! 0 | 1836 | `			}else{` |
|   10634 | 1837 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   10634 | 1838 | `				a[i] = 0;` |
|       - | 1839 | `			}` |
|    5318 | 1840 | `		}` |
|   11786 | 1841 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1842 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1843 | `			 * But that is impossible.` |
|       - | 1844 | `			 */` |
|     ! 0 | 1845 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1846 | `		}` |
|       2 | 1847 | `	}` |
|     658 | 1848 | `	p = a[0];` |
|   20994 | 1849 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   20338 | 1850 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   10170 | 1851 | `	}` |
|     658 | 1852 | `	p->pNext = 0;` |
|       - | 1853 | `	/* Reflect the change */` |
|     658 | 1854 | `	pMap->pFirst = p;` |
|       - | 1855 | `	/* Reset the loop cursor */` |
|     658 | 1856 | `	pMap->pCur = pMap->pFirst;` |
|     658 | 1857 | `	return SXRET_OK;` |
|       2 | 1858 |  |
|       - | 1859 | `/*` |
|       - | 1860 | ` * Node comparison callback.` |
|       - | 1861 | ` * used-by: [sort(),asort(),...]` |
|       - | 1862 | ` */` |
|   59963 | 1863 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1864 |  |
|       - | 1865 | `	ph7_value sA,sB;` |
|       - | 1866 | `	sxi32 iFlags;` |
|       - | 1867 | `	int rc;` |
|   59965 | 1868 | `	if( pCmpData == 0 ){` |
|       - | 1869 | `		/* Perform a standard comparison */` |
|   59941 | 1870 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   59941 | 1871 | `		return rc;` |
|       - | 1872 | `	}` |
|      25 | 1873 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1874 | `	/* Duplicate node values */` |
|      25 | 1875 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 1876 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 1877 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 1878 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 1879 | `	if( iFlags == 5 ){` |
|       - | 1880 | `		/* String cast */` |
|       - | 1881 | `		const char *zA,*zB;` |
|       - | 1882 | `		sxu32 nA,nB,nMin;` |
|      15 | 1883 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1884 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1885 | `		}` |
|      15 | 1886 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1887 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1888 | `		}` |
|       - | 1889 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 1890 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 1891 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 1892 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 1893 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 1894 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 1895 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 1896 | `		if( rc == 0 ){` |
|       5 | 1897 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1898 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1899 | `		}` |
|       8 | 1900 | `	}else{` |
|       - | 1901 | `		/* Numeric cast */` |
|      11 | 1902 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1903 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1904 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1905 | `	}` |
|      25 | 1906 | `	PH7_MemObjRelease(&sA);` |
|      25 | 1907 | `	PH7_MemObjRelease(&sB);` |
|      25 | 1908 | `	return rc;` |
|   29987 | 1909 |  |
|       - | 1910 | `/*` |
|       - | 1911 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1912 | ` * used-by: [ksort()]` |
|       - | 1913 | ` */` |
|      14 | 1914 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1915 |  |
|       - | 1916 | `	sxi32 rc;` |
|       7 | 1917 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 1918 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1919 | `		/* Perform a string comparison */` |
|       5 | 1920 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1921 | `	}else{` |
|       - | 1922 | `		SyString sStr;` |
|       - | 1923 | `		sxi64 iA,iB;` |
|       - | 1924 | `		/* Perform a numeric comparison */` |
|      11 | 1925 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1926 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1927 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1928 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1929 | `				iA = 0;` |
|     ! 0 | 1930 | `			}else{` |
|     ! 0 | 1931 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1932 | `			}` |
|     ! 0 | 1933 | `		}else{` |
|      11 | 1934 | `			iA = pA->xKey.iKey;` |
|       - | 1935 | `		}` |
|      11 | 1936 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1937 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1938 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1939 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1940 | `				iB = 0;` |
|     ! 0 | 1941 | `			}else{` |
|     ! 0 | 1942 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1943 | `			}` |
|     ! 0 | 1944 | `		}else{` |
|      11 | 1945 | `			iB = pB->xKey.iKey;` |
|       - | 1946 | `		}` |
|      11 | 1947 | `		rc = (sxi32)(iA-iB);` |
|       - | 1948 | `	}` |
|       - | 1949 | `	/* Comparison result */` |
|      15 | 1950 | `	return rc;` |
|       1 | 1951 |  |
|       - | 1952 | `/*` |
|       - | 1953 | ` * Node comparison callback.` |
|       - | 1954 | ` * Used by: [rsort(),arsort()];` |
|       - | 1955 | ` */` |
|      78 | 1956 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1957 |  |
|       - | 1958 | `	ph7_value sA,sB;` |
|       - | 1959 | `	sxi32 iFlags;` |
|       - | 1960 | `	int rc;` |
|      79 | 1961 | `	if( pCmpData == 0 ){` |
|       - | 1962 | `		/* Perform a standard comparison */` |
|      59 | 1963 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 1964 | `		return -rc;` |
|       - | 1965 | `	}` |
|      21 | 1966 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1967 | `	/* Duplicate node values */` |
|      21 | 1968 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 1969 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 1970 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 1971 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 1972 | `	if( iFlags == 5 ){` |
|       - | 1973 | `		/* String cast */` |
|       - | 1974 | `		const char *zA,*zB;` |
|       - | 1975 | `		sxu32 nA,nB,nMin;` |
|      11 | 1976 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1977 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1978 | `		}` |
|      11 | 1979 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1980 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1981 | `		}` |
|       - | 1982 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 1983 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 1984 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 1985 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 1986 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 1987 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 1988 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 1989 | `		if( rc == 0 ){` |
|       3 | 1990 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1991 | `			else if( nA > nB ) rc = 1;` |
|       1 | 1992 | `		}` |
|       6 | 1993 | `	}else{` |
|       - | 1994 | `		/* Numeric cast */` |
|      11 | 1995 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 1996 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 1997 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 1998 | `	}` |
|      21 | 1999 | `	PH7_MemObjRelease(&sA);` |
|      21 | 2000 | `	PH7_MemObjRelease(&sB);` |
|      21 | 2001 | `	return -rc;` |
|      40 | 2002 |  |
|       - | 2003 | `/*` |
|       - | 2004 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2005 | ` * used-by: [usort(),uasort()]` |
|       - | 2006 | ` */` |
|      78 | 2007 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 2008 |  |
|       - | 2009 | `	ph7_value sResult,*pCallback;` |
|       - | 2010 | `	ph7_value *pV1,*pV2;` |
|       - | 2011 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2012 | `	sxi32 rc;` |
|       - | 2013 | `	/* Point to the desired callback */` |
|      80 | 2014 | `	pCallback = (ph7_value *)pCmpData;` |
|      80 | 2015 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2016 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2017 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       3 | 2018 | `		return 0;` |
|       - | 2019 | `	}` |
|       - | 2020 | `	/* initialize the result value */` |
|      78 | 2021 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 2022 | `	/* Extract nodes values */` |
|      78 | 2023 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      78 | 2024 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      78 | 2025 | `	apArg[0] = pV1;` |
|      78 | 2026 | `	apArg[1] = pV2;` |
|       - | 2027 | `	/* Invoke the callback */` |
|      78 | 2028 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      78 | 2029 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2030 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2031 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       3 | 2032 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       3 | 2033 | `		rc = 0;` |
|      77 | 2034 | `	}else if( rc != SXRET_OK ){` |
|       - | 2035 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2036 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2037 | `	}else{` |
|       - | 2038 | `		/* Extract callback result */` |
|      76 | 2039 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2040 | `			/* Perform an int cast */` |
|     ! 0 | 2041 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2042 | `		}` |
|      76 | 2043 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2044 | `	}` |
|      78 | 2045 | `	PH7_MemObjRelease(&sResult);` |
|       - | 2046 | `	/* Callback result */` |
|      78 | 2047 | `	return rc;` |
|      41 | 2048 |  |
|       - | 2049 | `/*` |
|       - | 2050 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2051 | ` * used-by: [krsort()]` |
|       - | 2052 | ` */` |
|       4 | 2053 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2054 |  |
|       - | 2055 | `	sxi32 rc;` |
|       2 | 2056 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2057 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2058 | `		/* Perform a string comparison */` |
|       5 | 2059 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2060 | `	}else{` |
|       - | 2061 | `		SyString sStr;` |
|       - | 2062 | `		sxi64 iA,iB;` |
|       - | 2063 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2064 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2065 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2066 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2067 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2068 | `				iA = 0;` |
|     ! 0 | 2069 | `			}else{` |
|     ! 0 | 2070 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2071 | `			}` |
|     ! 0 | 2072 | `		}else{` |
|     ! 0 | 2073 | `			iA = pA->xKey.iKey;` |
|       - | 2074 | `		}` |
|     ! 0 | 2075 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2076 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2077 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2078 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2079 | `				iB = 0;` |
|     ! 0 | 2080 | `			}else{` |
|     ! 0 | 2081 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2082 | `			}` |
|     ! 0 | 2083 | `		}else{` |
|     ! 0 | 2084 | `			iB = pB->xKey.iKey;` |
|       - | 2085 | `		}` |
|     ! 0 | 2086 | `		rc = (sxi32)(iA-iB);` |
|       - | 2087 | `	}` |
|       5 | 2088 | `	return -rc; /* Reverse result */` |
|       1 | 2089 |  |
|       - | 2090 | `/*` |
|       - | 2091 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2092 | ` * used-by: [uksort()]` |
|       - | 2093 | ` */` |
|       6 | 2094 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2095 |  |
|       - | 2096 | `	ph7_value sResult,*pCallback;` |
|       - | 2097 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2098 | `	ph7_value sK1,sK2;` |
|       - | 2099 | `	sxi32 rc;` |
|       - | 2100 | `	/* Point to the desired callback */` |
|       7 | 2101 | `	pCallback = (ph7_value *)pCmpData;` |
|       7 | 2102 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2103 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2104 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     ! 0 | 2105 | `		return 0;` |
|       - | 2106 | `	}` |
|       - | 2107 | `	/* initialize the result value */` |
|       7 | 2108 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2109 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2110 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2111 | `	/* Extract nodes keys */` |
|       7 | 2112 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2113 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2114 | `	apArg[0] = &sK1;` |
|       7 | 2115 | `	apArg[1] = &sK2;` |
|       - | 2116 | `	/* Mark keys as constants */` |
|       7 | 2117 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2118 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2119 | `	/* Invoke the callback */` |
|       7 | 2120 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2121 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2122 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2123 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     ! 0 | 2124 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     ! 0 | 2125 | `		rc = 0;` |
|       7 | 2126 | `	}else if( rc != SXRET_OK ){` |
|       - | 2127 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2128 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2129 | `	}else{` |
|       - | 2130 | `		/* Extract callback result */` |
|       7 | 2131 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2132 | `			/* Perform an int cast */` |
|     ! 0 | 2133 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2134 | `		}` |
|       7 | 2135 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2136 | `	}` |
|       7 | 2137 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2138 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2139 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2140 | `	/* Callback result */` |
|       7 | 2141 | `	return rc;` |
|       4 | 2142 |  |
|       - | 2143 | `/*` |
|       - | 2144 | ` * Node comparison callback: Random node comparison.` |
|       - | 2145 | ` * used-by: [shuffle()]` |
|       - | 2146 | ` */` |
|      13 | 2147 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2148 |  |
|       - | 2149 | `	sxu32 n;` |
|       7 | 2150 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 2151 | `	SXUNUSED(pCmpData);` |
|       - | 2152 | `	/* Grab a random number */` |
|      14 | 2153 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2154 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2155 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2156 | `	 */` |
|      14 | 2157 | `	return n&1 ? 1 : -1;` |
|       1 | 2158 |  |
|       - | 2159 | `/*` |
|       - | 2160 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2161 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2162 | ` */` |
|     608 | 2163 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2164 |  |
|       - | 2165 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2166 | `	sxu32 i;` |
|       - | 2167 | `	/* Rehash all entries */` |
|     610 | 2168 | `	pLast = p = pMap->pFirst;` |
|     610 | 2169 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     610 | 2170 | `	i = 0;` |
|    6111 | 2171 | `	for( ;; ){` |
|   12224 | 2172 | `		if( i >= pMap->nEntry ){` |
|     610 | 2173 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     610 | 2174 | `			break;` |
|       - | 2175 | `		}` |
|   11616 | 2176 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2177 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2178 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2179 | `			/* Change key type */` |
|       5 | 2180 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2181 | `		}` |
|   11616 | 2182 | `		HashmapRehashIntNode(p);` |
|       - | 2183 | `		/* Point to the next entry */` |
|   11616 | 2184 | `		i++;` |
|   11616 | 2185 | `		pLast = p;` |
|   11616 | 2186 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2187 | `	}` |
|     610 | 2188 |  |
|       - | 2189 | `/*` |
|       - | 2190 | ` * Array functions implementation.` |
|       - | 2191 | ` * Status:` |
|       - | 2192 | ` *  Stable.` |
|       - | 2193 | ` */` |
|       - | 2194 | `/*` |
|       - | 2195 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2196 | ` * Sort an array.` |
|       - | 2197 | ` * Parameters` |
|       - | 2198 | ` *  $array` |
|       - | 2199 | ` *   The input array.` |
|       - | 2200 | ` * $sort_flags` |
|       - | 2201 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2202 | ` *  Sorting type flags:` |
|       - | 2203 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2204 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2205 | ` *   SORT_STRING - compare items as strings` |
|       - | 2206 | ` * Return` |
|       - | 2207 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2208 | ` *` |
|       - | 2209 | ` */` |
|     922 | 2210 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2211 |  |
|       - | 2212 | `	ph7_hashmap *pMap;` |
|       - | 2213 | `	/* Make sure we are dealing with a valid hashmap */` |
|     924 | 2214 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2215 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2216 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2217 | `		return PH7_OK;` |
|       - | 2218 | `	}` |
|       - | 2219 | `	/* Point to the internal representation of the input hashmap */` |
|     924 | 2220 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     924 | 2221 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     924 | 2222 | `	if( pMap->nEntry > 1 ){` |
|     598 | 2223 | `		sxi32 iCmpFlags = 0;` |
|     598 | 2224 | `		if( nArg > 1 ){` |
|       - | 2225 | `			/* Extract comparison flags */` |
|       3 | 2226 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2227 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2228 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2229 | `			}` |
|       1 | 2230 | `		}` |
|       - | 2231 | `		/* Do the merge sort */` |
|     598 | 2232 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2233 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     598 | 2234 | `		HashmapSortRehash(pMap);` |
|     298 | 2235 | `	}` |
|       - | 2236 | `	/* All done,return TRUE */` |
|     924 | 2237 | `	ph7_result_bool(pCtx,1);` |
|     924 | 2238 | `	return PH7_OK;` |
|     463 | 2239 |  |
|       - | 2240 | `/*` |
|       - | 2241 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2242 | ` *  Sort an array and maintain index association.` |
|       - | 2243 | ` * Parameters` |
|       - | 2244 | ` *  $array` |
|       - | 2245 | ` *   The input array.` |
|       - | 2246 | ` * $sort_flags` |
|       - | 2247 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2248 | ` *  Sorting type flags:` |
|       - | 2249 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2250 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2251 | ` *   SORT_STRING - compare items as strings` |
|       - | 2252 | ` * Return` |
|       - | 2253 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2254 | ` */` |
|      32 | 2255 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2256 |  |
|       - | 2257 | `	ph7_hashmap *pMap;` |
|       - | 2258 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2259 | `	if( nArg < 1 ){` |
|       3 | 2260 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2261 | `			"ArgumentCountError",` |
|       - | 2262 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2263 | `			);` |
|       - | 2264 | `	}` |
|       - | 2265 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2266 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2267 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2268 | `			"TypeError",` |
|       - | 2269 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2270 | `			ph7_type_name(apArg[0])` |
|       - | 2271 | `			);` |
|       - | 2272 | `	}` |
|       - | 2273 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2274 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2275 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2276 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2277 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2278 | `		if( nArg > 1 ){` |
|       - | 2279 | `			/* Extract comparison flags */` |
|       5 | 2280 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2281 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2282 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2283 | `			}` |
|       2 | 2284 | `		}` |
|       - | 2285 | `		/* Do the merge sort */` |
|      19 | 2286 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2287 | `		/* Fix the last link broken by the merge */` |
|      45 | 2288 | `		while(pMap->pLast->pPrev){` |
|      27 | 2289 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2290 | `		}` |
|       9 | 2291 | `	}` |
|       - | 2292 | `	/* All done,return TRUE */` |
|      23 | 2293 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2294 | `	return PH7_OK;` |
|      18 | 2295 |  |
|       - | 2296 | `/*` |
|       - | 2297 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2298 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2299 | ` * Parameters` |
|       - | 2300 | ` *  $array` |
|       - | 2301 | ` *   The input array.` |
|       - | 2302 | ` * $sort_flags` |
|       - | 2303 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2304 | ` *  Sorting type flags:` |
|       - | 2305 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2306 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2307 | ` *   SORT_STRING - compare items as strings` |
|       - | 2308 | ` * Return` |
|       - | 2309 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2310 | ` */` |
|      32 | 2311 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2312 |  |
|       - | 2313 | `	ph7_hashmap *pMap;` |
|       - | 2314 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      34 | 2315 | `	if( nArg < 1 ){` |
|       3 | 2316 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2317 | `			"ArgumentCountError",` |
|       - | 2318 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2319 | `			);` |
|       - | 2320 | `	}` |
|       - | 2321 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      32 | 2322 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      13 | 2323 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2324 | `			"TypeError",` |
|       - | 2325 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2326 | `			ph7_type_name(apArg[0])` |
|       - | 2327 | `			);` |
|       - | 2328 | `	}` |
|       - | 2329 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2330 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2331 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2332 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2333 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2334 | `		if( nArg > 1 ){` |
|       - | 2335 | `			/* Extract comparison flags */` |
|       5 | 2336 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2337 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2338 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2339 | `			}` |
|       2 | 2340 | `		}` |
|       - | 2341 | `		/* Do the merge sort */` |
|      19 | 2342 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2343 | `		/* Fix the last link broken by the merge */` |
|      35 | 2344 | `		while(pMap->pLast->pPrev){` |
|      17 | 2345 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2346 | `		}` |
|       9 | 2347 | `	}` |
|       - | 2348 | `	/* All done,return TRUE */` |
|      23 | 2349 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2350 | `	return PH7_OK;` |
|      18 | 2351 |  |
|       - | 2352 | `/*` |
|       - | 2353 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2354 | ` *  Sort an array by key.` |
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
|       4 | 2367 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2368 |  |
|       - | 2369 | `	ph7_hashmap *pMap;` |
|       - | 2370 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2371 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2372 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2373 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2374 | `		return PH7_OK;` |
|       - | 2375 | `	}` |
|       - | 2376 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2377 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2378 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2379 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2380 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2381 | `		if( nArg > 1 ){` |
|       - | 2382 | `			/* Extract comparison flags */` |
|     ! 0 | 2383 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2384 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2385 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2386 | `			}` |
|     ! 0 | 2387 | `		}` |
|       - | 2388 | `		/* Do the merge sort */` |
|       5 | 2389 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2390 | `		/* Fix the last link broken by the merge */` |
|      15 | 2391 | `		while(pMap->pLast->pPrev){` |
|      11 | 2392 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2393 | `		}` |
|       2 | 2394 | `	}` |
|       - | 2395 | `	/* All done,return TRUE */` |
|       5 | 2396 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2397 | `	return PH7_OK;` |
|       3 | 2398 |  |
|       - | 2399 | `/*` |
|       - | 2400 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2401 | ` *  Sort an array by key in reverse order.` |
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
|       2 | 2414 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|       3 | 2436 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2437 | `		/* Fix the last link broken by the merge */` |
|       7 | 2438 | `		while(pMap->pLast->pPrev){` |
|       5 | 2439 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2440 | `		}` |
|       1 | 2441 | `	}` |
|       - | 2442 | `	/* All done,return TRUE */` |
|       3 | 2443 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2444 | `	return PH7_OK;` |
|       2 | 2445 |  |
|       - | 2446 | `/*` |
|       - | 2447 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2448 | ` * Sort an array in reverse order.` |
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
|       2 | 2461 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2462 |  |
|       - | 2463 | `	ph7_hashmap *pMap;` |
|       - | 2464 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2465 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2466 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2467 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2468 | `		return PH7_OK;` |
|       - | 2469 | `	}` |
|       - | 2470 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2471 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2472 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2473 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2474 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2475 | `		if( nArg > 1 ){` |
|       - | 2476 | `			/* Extract comparison flags */` |
|     ! 0 | 2477 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2478 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2479 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2480 | `			}` |
|     ! 0 | 2481 | `		}` |
|       - | 2482 | `		/* Do the merge sort */` |
|       3 | 2483 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2484 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2485 | `		HashmapSortRehash(pMap);` |
|       1 | 2486 | `	}` |
|       - | 2487 | `	/* All done,return TRUE */` |
|       3 | 2488 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2489 | `	return PH7_OK;` |
|       2 | 2490 |  |
|       - | 2491 | `/*` |
|       - | 2492 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2493 | ` *  Sort an array by values using a user-defined comparison function.` |
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
|       8 | 2505 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2506 |  |
|       - | 2507 | `	ph7_hashmap *pMap;` |
|       - | 2508 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 | 2509 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2510 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2511 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2512 | `		return PH7_OK;` |
|       - | 2513 | `	}` |
|       - | 2514 | `	/* Point to the internal representation of the input hashmap */` |
|      10 | 2515 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      10 | 2516 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      10 | 2517 | `	if( pMap->nEntry > 1 ){` |
|      10 | 2518 | `		ph7_value *pCallback = 0;` |
|       - | 2519 | `		ProcNodeCmp xCmp;` |
|      10 | 2520 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      10 | 2521 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2522 | `			/* Point to the desired callback */` |
|      10 | 2523 | `			pCallback = apArg[1];` |
|       6 | 2524 | `		}else{` |
|       - | 2525 | `			/* Use the default comparison function */` |
|     ! 0 | 2526 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2527 | `		}` |
|       - | 2528 | `		/* Do the merge sort */` |
|      10 | 2529 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      10 | 2530 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2531 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      10 | 2532 | `		HashmapSortRehash(pMap);` |
|      10 | 2533 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2534 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 2535 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2536 | `			return PH7_EXCEPTION;` |
|       - | 2537 | `		}` |
|       3 | 2538 | `	}` |
|       - | 2539 | `	/* All done,return TRUE */` |
|       8 | 2540 | `	ph7_result_bool(pCtx,1);` |
|       8 | 2541 | `	return PH7_OK;` |
|       6 | 2542 |  |
|       - | 2543 | `/*` |
|       - | 2544 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2545 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2546 | ` *  and maintain index association.` |
|       - | 2547 | ` * Parameters` |
|       - | 2548 | ` *  $array` |
|       - | 2549 | ` *   The input array.` |
|       - | 2550 | ` * $cmp_function` |
|       - | 2551 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2552 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2553 | ` *  to, or greater than the second.` |
|       - | 2554 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2555 | ` * Return` |
|       - | 2556 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2557 | ` */` |
|       2 | 2558 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2559 |  |
|       - | 2560 | `	ph7_hashmap *pMap;` |
|       - | 2561 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2562 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2563 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2564 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2565 | `		return PH7_OK;` |
|       - | 2566 | `	}` |
|       - | 2567 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2568 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2569 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2570 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2571 | `		ph7_value *pCallback = 0;` |
|       - | 2572 | `		ProcNodeCmp xCmp;` |
|       3 | 2573 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2574 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2575 | `			/* Point to the desired callback */` |
|       3 | 2576 | `			pCallback = apArg[1];` |
|       2 | 2577 | `		}else{` |
|       - | 2578 | `			/* Use the default comparison function */` |
|     ! 0 | 2579 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2580 | `		}` |
|       - | 2581 | `		/* Do the merge sort */` |
|       3 | 2582 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2583 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2584 | `		/* Fix the last link broken by the merge */` |
|       5 | 2585 | `		while(pMap->pLast->pPrev){` |
|       3 | 2586 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2587 | `		}` |
|       3 | 2588 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2589 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2590 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2591 | `			return PH7_EXCEPTION;` |
|       - | 2592 | `		}` |
|       1 | 2593 | `	}` |
|       - | 2594 | `	/* All done,return TRUE */` |
|       3 | 2595 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2596 | `	return PH7_OK;` |
|       2 | 2597 |  |
|       - | 2598 | `/*` |
|       - | 2599 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2600 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2601 | ` *  function and maintain index association.` |
|       - | 2602 | ` * Parameters` |
|       - | 2603 | ` *  $array` |
|       - | 2604 | ` *   The input array.` |
|       - | 2605 | ` * $cmp_function` |
|       - | 2606 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2607 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2608 | ` *  to, or greater than the second.` |
|       - | 2609 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2610 | ` * Return` |
|       - | 2611 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2612 | ` */` |
|       2 | 2613 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2614 |  |
|       - | 2615 | `	ph7_hashmap *pMap;` |
|       - | 2616 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2617 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2618 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2619 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2620 | `		return PH7_OK;` |
|       - | 2621 | `	}` |
|       - | 2622 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2623 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2624 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2625 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2626 | `		ph7_value *pCallback = 0;` |
|       - | 2627 | `		ProcNodeCmp xCmp;` |
|       3 | 2628 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2629 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2630 | `			/* Point to the desired callback */` |
|       3 | 2631 | `			pCallback = apArg[1];` |
|       2 | 2632 | `		}else{` |
|       - | 2633 | `			/* Use the default comparison function */` |
|     ! 0 | 2634 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2635 | `		}` |
|       - | 2636 | `		/* Do the merge sort */` |
|       3 | 2637 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2638 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2639 | `		/* Fix the last link broken by the merge */` |
|       3 | 2640 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2641 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2642 | `		}` |
|       3 | 2643 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2644 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2645 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2646 | `			return PH7_EXCEPTION;` |
|       - | 2647 | `		}` |
|       1 | 2648 | `	}` |
|       - | 2649 | `	/* All done,return TRUE */` |
|       3 | 2650 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2651 | `	return PH7_OK;` |
|       2 | 2652 |  |
|       - | 2653 | `/*` |
|       - | 2654 | ` * bool shuffle(array &$array)` |
|       - | 2655 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2656 | ` * Parameters` |
|       - | 2657 | ` *  $array` |
|       - | 2658 | ` *   The input array.` |
|       - | 2659 | ` * Return` |
|       - | 2660 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2661 | ` *` |
|       - | 2662 | ` */` |
|       2 | 2663 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2664 |  |
|       - | 2665 | `	ph7_hashmap *pMap;` |
|       - | 2666 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2667 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2668 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2669 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2670 | `		return PH7_OK;` |
|       - | 2671 | `	}` |
|       - | 2672 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2673 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2674 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2675 | `	if( pMap->nEntry > 1 ){` |
|       - | 2676 | `		/* Do the merge sort */` |
|       3 | 2677 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2678 | `		/* Fix the last link broken by the merge */` |
|      11 | 2679 | `		while(pMap->pLast->pPrev){` |
|       8 | 2680 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2681 | `		}` |
|       1 | 2682 | `	}` |
|       - | 2683 | `	/* All done,return TRUE */` |
|       3 | 2684 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2685 | `	return PH7_OK;` |
|       2 | 2686 |  |
|       - | 2687 | `/*` |
|       - | 2688 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2689 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2690 | ` * Parameters` |
|       - | 2691 | ` *  $var` |
|       - | 2692 | ` *   The array or the object.` |
|       - | 2693 | ` * $mode` |
|       - | 2694 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2695 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2696 | ` *  all the elements of a multidimensional array.` |
|       - | 2697 | ` * Return` |
|       - | 2698 | ` *  Returns the number of elements in the array.` |
|       - | 2699 | ` */` |
|     786 | 2700 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2701 |  |
|     788 | 2702 | `	int bRecursive = FALSE;` |
|     788 | 2703 | `	int bCycleDetected = FALSE;` |
|       - | 2704 | `	sxi64 iCount;` |
|     788 | 2705 | `	if( nArg < 1 ){` |
|       3 | 2706 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2707 | `			"ArgumentCountError",` |
|       - | 2708 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2709 | `			);` |
|       - | 2710 | `	}` |
|     786 | 2711 | `	if( nArg > 2 ){` |
|       4 | 2712 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2713 | `			"ArgumentCountError",` |
|       - | 2714 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2715 | `			nArg` |
|       - | 2716 | `			);` |
|       - | 2717 | `	}` |
|       - | 2718 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - | 2719 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - | 2720 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|     784 | 2721 | `	if( nArg > 1 ){` |
|      42 | 2722 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      42 | 2723 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       9 | 2724 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2725 | `				"ValueError",` |
|       - | 2726 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2727 | `				);` |
|       - | 2728 | `		}` |
|      34 | 2729 | `		bRecursive = iMode == 1;` |
|      16 | 2730 | `	}` |
|     776 | 2731 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2732 | `		/* Countable object: dispatch to ->count() */` |
|      28 | 2733 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      18 | 2734 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      18 | 2735 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      18 | 2736 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      16 | 2737 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2738 | `					"count",sizeof("count")-1);` |
|      16 | 2739 | `				if( pMeth ){` |
|       - | 2740 | `					ph7_value sResult;` |
|      16 | 2741 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      16 | 2742 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      16 | 2743 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      16 | 2744 | `					PH7_MemObjRelease(&sResult);` |
|      16 | 2745 | `					return PH7_OK;` |
|       - | 2746 | `				}` |
|     ! 0 | 2747 | `			}` |
|       1 | 2748 | `		}` |
|      19 | 2749 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2750 | `			"TypeError",` |
|       - | 2751 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       6 | 2752 | `			ph7_type_name(apArg[0])` |
|       - | 2753 | `			);` |
|       - | 2754 | `	}` |
|       - | 2755 | `	/* Count */` |
|     750 | 2756 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     750 | 2757 | `	if( bCycleDetected ){` |
|       3 | 2758 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2759 | `	}` |
|     750 | 2760 | `	ph7_result_int64(pCtx,iCount);` |
|     750 | 2761 | `	return PH7_OK;` |
|     395 | 2762 |  |
|       - | 2763 | `/*` |
|       - | 2764 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2765 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2766 | ` * Parameters` |
|       - | 2767 | ` * $key` |
|       - | 2768 | ` *   Value to check.` |
|       - | 2769 | ` * $search` |
|       - | 2770 | ` *  An array with keys to check.` |
|       - | 2771 | ` * Return` |
|       - | 2772 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2773 | ` */` |
|      82 | 2774 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2775 |  |
|       - | 2776 | `	sxi32 rc;` |
|      84 | 2777 | `	if( nArg != 2 ){` |
|       - | 2778 | `		/* PHP requires exactly two arguments */` |
|      10 | 2779 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2780 | `			"ArgumentCountError",` |
|       - | 2781 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2782 | `			nArg` |
|       - | 2783 | `			);` |
|       - | 2784 | `	}` |
|       - | 2785 | `	/* Make sure we are dealing with a valid hashmap */` |
|      78 | 2786 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2787 | `		/* Type mismatch -> TypeError */` |
|       7 | 2788 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2789 | `			"TypeError",` |
|       - | 2790 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2791 | `			ph7_type_name(apArg[1])` |
|       - | 2792 | `			);` |
|       - | 2793 | `	}` |
|       - | 2794 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      74 | 2795 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2796 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2797 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2798 | `			"use an empty string instead"` |
|       - | 2799 | `			);` |
|      73 | 2800 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2801 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2802 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2803 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2804 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2805 | `				,rVal` |
|       - | 2806 | `				);` |
|       1 | 2807 | `		}` |
|       1 | 2808 | `	}` |
|       - | 2809 | `	/* Perform the lookup */` |
|      74 | 2810 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2811 | `	/* lookup result */` |
|      74 | 2812 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      74 | 2813 | `	return PH7_OK;` |
|      43 | 2814 |  |
|       - | 2815 | `/*` |
|       - | 2816 | ` * value array_pop(array $array)` |
|       - | 2817 | ` *   POP the last inserted element from the array.` |
|       - | 2818 | ` * Parameter` |
|       - | 2819 | ` *  The array to get the value from.` |
|       - | 2820 | ` * Return` |
|       - | 2821 | ` *  Poped value or NULL on failure.` |
|       - | 2822 | ` */` |
|      18 | 2823 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2824 |  |
|       - | 2825 | `	ph7_hashmap *pMap;` |
|       - | 2826 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      20 | 2827 | `	if( nArg != 1 ){` |
|       7 | 2828 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2829 | `			"ArgumentCountError",` |
|       - | 2830 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2831 | `			nArg` |
|       - | 2832 | `			);` |
|       - | 2833 | `	}` |
|       - | 2834 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2835 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      16 | 2836 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2837 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2838 | `			"Error",` |
|       - | 2839 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2840 | `			);` |
|       - | 2841 | `	}` |
|       - | 2842 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2843 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2844 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2845 | `			"TypeError",` |
|       - | 2846 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2847 | `			ph7_type_name(apArg[0])` |
|       - | 2848 | `			);` |
|       - | 2849 | `	}` |
|       9 | 2850 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2851 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2852 | `	if( pMap->nEntry < 1 ){` |
|       - | 2853 | `		/* Nothing to pop,return NULL */` |
|       3 | 2854 | `		ph7_result_null(pCtx);` |
|       2 | 2855 | `	}else{` |
|       7 | 2856 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2857 | `		ph7_value *pObj;` |
|       7 | 2858 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2859 | `		if( pObj ){` |
|       - | 2860 | `			/* Node value */` |
|       7 | 2861 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2862 | `			/* Unlink the node */` |
|       7 | 2863 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2864 | `		}else{` |
|     ! 0 | 2865 | `			ph7_result_null(pCtx);` |
|       - | 2866 | `		}` |
|       - | 2867 | `		/* Reset the cursor */` |
|       7 | 2868 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2869 | `	}` |
|       9 | 2870 | `	return PH7_OK;` |
|      11 | 2871 |  |
|       - | 2872 | `/*` |
|       - | 2873 | ` * int array_push($array,$var,...)` |
|       - | 2874 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2875 | ` * Parameters` |
|       - | 2876 | ` *  array` |
|       - | 2877 | ` *    The input array.` |
|       - | 2878 | ` *  var` |
|       - | 2879 | ` *   On or more value to push.` |
|       - | 2880 | ` * Return` |
|       - | 2881 | ` *  New array count (including old items).` |
|       - | 2882 | ` */` |
|      22 | 2883 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2884 |  |
|       - | 2885 | `	ph7_hashmap *pMap;` |
|       - | 2886 | `	sxi32 rc;` |
|       - | 2887 | `	int i;` |
|      24 | 2888 | `	if( nArg < 1 ){` |
|       4 | 2889 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2890 | `			"ArgumentCountError",` |
|       - | 2891 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2892 | `			nArg` |
|       - | 2893 | `			);` |
|       - | 2894 | `	}` |
|       - | 2895 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2896 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      22 | 2897 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2898 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2899 | `			"Error",` |
|       - | 2900 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2901 | `			);` |
|       - | 2902 | `	}` |
|       - | 2903 | `	/* Make sure we are dealing with a valid hashmap */` |
|      18 | 2904 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2905 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2906 | `			"TypeError",` |
|       - | 2907 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2908 | `			ph7_type_name(apArg[0])` |
|       - | 2909 | `			);` |
|       - | 2910 | `	}` |
|       - | 2911 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2912 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2913 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2914 | `	/* Start pushing given values */` |
|      31 | 2915 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      17 | 2916 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      17 | 2917 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2918 | `			break;` |
|       - | 2919 | `		}` |
|       9 | 2920 | `	}` |
|       - | 2921 | `	/* Return the new count */` |
|      15 | 2922 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 2923 | `	return PH7_OK;` |
|      13 | 2924 |  |
|       - | 2925 | `/*` |
|       - | 2926 | ` * value array_shift(array $array)` |
|       - | 2927 | ` *   Shift an element off the beginning of array.` |
|       - | 2928 | ` * Parameter` |
|       - | 2929 | ` *  The array to get the value from.` |
|       - | 2930 | ` * Return` |
|       - | 2931 | ` *  Shifted value or NULL on failure.` |
|       - | 2932 | ` */` |
|      38 | 2933 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2934 |  |
|       - | 2935 | `	ph7_hashmap *pMap;` |
|       - | 2936 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      40 | 2937 | `	if( nArg != 1 ){` |
|       7 | 2938 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2939 | `			"ArgumentCountError",` |
|       - | 2940 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2941 | `			nArg` |
|       - | 2942 | `			);` |
|       - | 2943 | `	}` |
|       - | 2944 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      36 | 2945 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2946 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2947 | `			"Error",` |
|       - | 2948 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2949 | `			);` |
|       - | 2950 | `	}` |
|       - | 2951 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 2952 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2953 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2954 | `			"TypeError",` |
|       - | 2955 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2956 | `			ph7_type_name(apArg[0])` |
|       - | 2957 | `			);` |
|       - | 2958 | `	}` |
|       - | 2959 | `	/* Point to the internal representation of the hashmap */` |
|      30 | 2960 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      30 | 2961 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      30 | 2962 | `	if( pMap->nEntry < 1 ){` |
|       - | 2963 | `		/* Empty hashmap,return NULL */` |
|       3 | 2964 | `		ph7_result_null(pCtx);` |
|       2 | 2965 | `	}else{` |
|      28 | 2966 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2967 | `		ph7_value *pObj;` |
|       - | 2968 | `		sxu32 n;` |
|      28 | 2969 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      28 | 2970 | `		if( pObj ){` |
|       - | 2971 | `			/* Node value */` |
|      28 | 2972 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2973 | `			/* Unlink the first node */` |
|      28 | 2974 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      15 | 2975 | `		}else{` |
|     ! 0 | 2976 | `			ph7_result_null(pCtx);` |
|       - | 2977 | `		}` |
|       - | 2978 | `		/* Rehash all int keys */` |
|      28 | 2979 | `		n = pMap->nEntry;` |
|      28 | 2980 | `		pEntry = pMap->pFirst;` |
|      28 | 2981 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 2982 | `		for(;;){` |
|      82 | 2983 | `			if( n < 1 ){` |
|      28 | 2984 | `				break;` |
|       - | 2985 | `			}` |
|      56 | 2986 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      56 | 2987 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 2988 | `			}` |
|       - | 2989 | `			/* Point to the next entry */` |
|      56 | 2990 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      56 | 2991 | `			n--;` |
|       2 | 2992 | `		}` |
|       - | 2993 | `		/* Reset the cursor */` |
|      28 | 2994 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2995 | `	}` |
|      30 | 2996 | `	return PH7_OK;` |
|      21 | 2997 |  |
|       - | 2998 | `/*` |
|       - | 2999 | ` * Extract the node cursor value.` |
|       - | 3000 | ` */` |
|      24 | 3001 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 3002 |  |
|      25 | 3003 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 3004 | `	ph7_value *pVal;` |
|      25 | 3005 | `	if( pCur == 0 ){` |
|       - | 3006 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 3007 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3008 | `		return PH7_OK;` |
|       - | 3009 | `	}` |
|      25 | 3010 | `	if( iDirection != 0 ){` |
|       9 | 3011 | `		if( iDirection > 0 ){` |
|       - | 3012 | `			/* Point to the next entry */` |
|       7 | 3013 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 3014 | `			pCur = pMap->pCur;` |
|       4 | 3015 | `		}else{` |
|       - | 3016 | `			/* Point to the previous entry */` |
|       3 | 3017 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 3018 | `			pCur = pMap->pCur;` |
|       - | 3019 | `		}` |
|       9 | 3020 | `		if( pCur == 0 ){` |
|       - | 3021 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 3022 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3023 | `			return PH7_OK;` |
|       - | 3024 | `		}` |
|       4 | 3025 | `	}` |
|       - | 3026 | `	/* Point to the desired element */` |
|      25 | 3027 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 3028 | `	if( pVal ){` |
|      25 | 3029 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 3030 | `	}else{` |
|     ! 0 | 3031 | `		ph7_result_bool(pCtx,0);` |
|       - | 3032 | `	}` |
|      25 | 3033 | `	return PH7_OK;` |
|      13 | 3034 |  |
|       - | 3035 | `/*` |
|       - | 3036 | ` * value current(array $array)` |
|       - | 3037 | ` *  Return the current element in an array.` |
|       - | 3038 | ` * Parameter` |
|       - | 3039 | ` *  $input: The input array.` |
|       - | 3040 | ` * Return` |
|       - | 3041 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 3042 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3043 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3044 | ` *  is empty, current() returns FALSE.` |
|       - | 3045 | ` */` |
|      10 | 3046 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3047 |  |
|      11 | 3048 | `	if( nArg < 1 ){` |
|       - | 3049 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3050 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3051 | `		return PH7_OK;` |
|       - | 3052 | `	}` |
|       - | 3053 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 3054 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3055 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3056 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3057 | `		return PH7_OK;` |
|       - | 3058 | `	}` |
|      11 | 3059 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 3060 | `	return PH7_OK;` |
|       6 | 3061 |  |
|       - | 3062 | `/*` |
|       - | 3063 | ` * value next(array $input)` |
|       - | 3064 | ` *  Advance the internal array pointer of an array.` |
|       - | 3065 | ` * Parameter` |
|       - | 3066 | ` *  $input: The input array.` |
|       - | 3067 | ` * Return` |
|       - | 3068 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 3069 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 3070 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 3071 | ` */` |
|       6 | 3072 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3073 |  |
|       7 | 3074 | `	if( nArg < 1 ){` |
|       - | 3075 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3076 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3077 | `		return PH7_OK;` |
|       - | 3078 | `	}` |
|       - | 3079 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 3080 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3081 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3082 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3083 | `		return PH7_OK;` |
|       - | 3084 | `	}` |
|       7 | 3085 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 3086 | `	return PH7_OK;` |
|       4 | 3087 |  |
|       - | 3088 | `/*` |
|       - | 3089 | ` * value prev(array $input)` |
|       - | 3090 | ` *  Rewind the internal array pointer.` |
|       - | 3091 | ` * Parameter` |
|       - | 3092 | ` *  $input: The input array.` |
|       - | 3093 | ` * Return` |
|       - | 3094 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3095 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3096 | ` *  elements.` |
|       - | 3097 | ` */` |
|       2 | 3098 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3099 |  |
|       3 | 3100 | `	if( nArg < 1 ){` |
|       - | 3101 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3102 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3103 | `		return PH7_OK;` |
|       - | 3104 | `	}` |
|       - | 3105 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3106 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3107 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3108 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3109 | `		return PH7_OK;` |
|       - | 3110 | `	}` |
|       3 | 3111 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3112 | `	return PH7_OK;` |
|       2 | 3113 |  |
|       - | 3114 | `/*` |
|       - | 3115 | ` * value end(array $input)` |
|       - | 3116 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3117 | ` * Parameter` |
|       - | 3118 | ` *  $input: The input array.` |
|       - | 3119 | ` * Return` |
|       - | 3120 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3121 | ` */` |
|       2 | 3122 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3123 |  |
|       - | 3124 | `	ph7_hashmap *pMap;` |
|       3 | 3125 | `	if( nArg < 1 ){` |
|       - | 3126 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3127 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3128 | `		return PH7_OK;` |
|       - | 3129 | `	}` |
|       - | 3130 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3131 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3132 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3133 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3134 | `		return PH7_OK;` |
|       - | 3135 | `	}` |
|       - | 3136 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3137 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3138 | `	/* Point to the last node */` |
|       3 | 3139 | `	pMap->pCur = pMap->pLast;` |
|       - | 3140 | `	/* Return the last node value */` |
|       3 | 3141 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3142 | `	return PH7_OK;` |
|       2 | 3143 |  |
|       - | 3144 | `/*` |
|       - | 3145 | ` * value reset(array $array )` |
|       - | 3146 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3147 | ` * Parameter` |
|       - | 3148 | ` *  $input: The input array.` |
|       - | 3149 | ` * Return` |
|       - | 3150 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3151 | ` */` |
|       4 | 3152 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3153 |  |
|       - | 3154 | `	ph7_hashmap *pMap;` |
|       5 | 3155 | `	if( nArg < 1 ){` |
|       - | 3156 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3157 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3158 | `		return PH7_OK;` |
|       - | 3159 | `	}` |
|       - | 3160 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3161 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3162 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3163 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3164 | `		return PH7_OK;` |
|       - | 3165 | `	}` |
|       - | 3166 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3167 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3168 | `	/* Point to the first node */` |
|       5 | 3169 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3170 | `	/* Return the last node value if available */` |
|       5 | 3171 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3172 | `	return PH7_OK;` |
|       3 | 3173 |  |
|       - | 3174 | `/*` |
|       - | 3175 | ` * value key(array $array)` |
|       - | 3176 | ` *   Fetch a key from an array` |
|       - | 3177 | ` * Parameter` |
|       - | 3178 | ` *  $input` |
|       - | 3179 | ` *   The input array.` |
|       - | 3180 | ` * Return` |
|       - | 3181 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3182 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3183 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3184 | ` *  is empty, key() returns NULL.` |
|       - | 3185 | ` */` |
|       4 | 3186 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3187 |  |
|       - | 3188 | `	ph7_hashmap_node *pCur;` |
|       - | 3189 | `	ph7_hashmap *pMap;` |
|       5 | 3190 | `	if( nArg < 1 ){` |
|       - | 3191 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3192 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3193 | `		return PH7_OK;` |
|       - | 3194 | `	}` |
|       - | 3195 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3196 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3197 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3198 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3199 | `		return PH7_OK;` |
|       - | 3200 | `	}` |
|       5 | 3201 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3202 | `	pCur = pMap->pCur;` |
|       5 | 3203 | `	if( pCur == 0 ){` |
|       - | 3204 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3205 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3206 | `		return PH7_OK;` |
|       - | 3207 | `	}` |
|       5 | 3208 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3209 | `		/* Key is integer */` |
|     ! 0 | 3210 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3211 | `	}else{` |
|       - | 3212 | `		/* Key is blob */` |
|       7 | 3213 | `		ph7_result_string(pCtx,` |
|       4 | 3214 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3215 | `	}` |
|       5 | 3216 | `	return PH7_OK;` |
|       3 | 3217 |  |
|       - | 3218 | `/*` |
|       - | 3219 | ` * array each(array $input)` |
|       - | 3220 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3221 | ` * Parameter` |
|       - | 3222 | ` *  $input` |
|       - | 3223 | ` *    The input array.` |
|       - | 3224 | ` * Return` |
|       - | 3225 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3226 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3227 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3228 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3229 | ` *  each() returns FALSE.` |
|       - | 3230 | ` */` |
|      22 | 3231 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3232 |  |
|       - | 3233 | `	ph7_hashmap_node *pCur;` |
|       - | 3234 | `	ph7_hashmap *pMap;` |
|       - | 3235 | `	ph7_value *pArray;` |
|       - | 3236 | `	ph7_value *pVal;` |
|       - | 3237 | `	ph7_value sKey;` |
|      23 | 3238 | `	if( nArg < 1 ){` |
|       - | 3239 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3240 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3241 | `		return PH7_OK;` |
|       - | 3242 | `	}` |
|       - | 3243 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3244 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3245 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3246 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3247 | `		return PH7_OK;` |
|       - | 3248 | `	}` |
|       - | 3249 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3250 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3251 | `	if( pMap->pCur == 0 ){` |
|       - | 3252 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3253 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3254 | `		return PH7_OK;` |
|       - | 3255 | `	}` |
|      15 | 3256 | `	pCur = pMap->pCur;` |
|       - | 3257 | `	/* Create a new array */` |
|      15 | 3258 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3259 | `	if( pArray == 0 ){` |
|     ! 0 | 3260 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3261 | `		return PH7_OK;` |
|       - | 3262 | `	}` |
|      15 | 3263 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3264 | `	/* Insert the current value */` |
|      15 | 3265 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3266 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3267 | `	/* Make the key */` |
|      15 | 3268 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3269 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3270 | `	}else{` |
|       9 | 3271 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3272 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3273 | `	}` |
|       - | 3274 | `	/* Insert the current key */` |
|      15 | 3275 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3276 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3277 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3278 | `	/* Advance the cursor */` |
|      15 | 3279 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3280 | `	/* Return the current entry */` |
|      15 | 3281 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3282 | `	return PH7_OK;` |
|      12 | 3283 |  |
|       - | 3284 | `/*` |
|       - | 3285 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3286 | ` *  Create an array containing a range of elements` |
|       - | 3287 | ` * Parameter` |
|       - | 3288 | ` *  start` |
|       - | 3289 | ` *   First value of the sequence.` |
|       - | 3290 | ` *  limit` |
|       - | 3291 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3292 | ` *  step` |
|       - | 3293 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3294 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3295 | ` * Return` |
|       - | 3296 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3297 | ` * NOTE:` |
|       - | 3298 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3299 | ` */` |
|       2 | 3300 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3301 |  |
|       - | 3302 | `	ph7_value *pValue,*pArray;` |
|       - | 3303 | `	sxi64 iOfft,iLimit;` |
|       3 | 3304 | `	int iStep = 1;` |
|       - | 3305 |  |
|       3 | 3306 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3307 | `	if( nArg > 0 ){` |
|       - | 3308 | `		/* Extract the offset */` |
|       3 | 3309 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3310 | `		if( nArg > 1 ){` |
|       - | 3311 | `			/* Extract the limit */` |
|       3 | 3312 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3313 | `			if( nArg > 2 ){` |
|       - | 3314 | `				/* Extract the increment */` |
|       3 | 3315 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3316 | `				if( iStep < 1 ){` |
|       - | 3317 | `					/* Only positive number are allowed */` |
|       3 | 3318 | `					iStep = 1;` |
|       1 | 3319 | `				}` |
|       1 | 3320 | `			}` |
|       1 | 3321 | `		}` |
|       1 | 3322 | `	}` |
|       - | 3323 | `	/* Element container */` |
|       3 | 3324 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3325 | `	/* Create the new array */` |
|       3 | 3326 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3327 | `	if( pArray == 0 ){` |
|     ! 0 | 3328 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3329 | `		return PH7_OK;` |
|       - | 3330 | `	}` |
|       - | 3331 | `	/* Start filling */` |
|       3 | 3332 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3333 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3334 | `		/* Perform the insertion */` |
|     ! 0 | 3335 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3336 | `		/* Increment */` |
|     ! 0 | 3337 | `		iOfft += iStep;` |
|     ! 0 | 3338 | `	}` |
|       - | 3339 | `	/* Return the new array */` |
|       3 | 3340 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3341 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3342 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3343 | `	 */` |
|       3 | 3344 | `	return PH7_OK;` |
|       2 | 3345 |  |
|       - | 3346 | `/*` |
|       - | 3347 | ` * array array_values(array $array)` |
|       - | 3348 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3349 | ` * Parameters` |
|       - | 3350 | ` *  $array` |
|       - | 3351 | ` *   The input array.` |
|       - | 3352 | ` * Return` |
|       - | 3353 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3354 | ` */` |
|      30 | 3355 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3356 |  |
|       - | 3357 | `	ph7_hashmap_node *pNode;` |
|       - | 3358 | `	ph7_hashmap *pMap;` |
|       - | 3359 | `	ph7_value *pArray;` |
|       - | 3360 | `	ph7_value *pObj;` |
|       - | 3361 | `	sxu32 n;` |
|      32 | 3362 | `	if( nArg != 1 ){` |
|       - | 3363 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3364 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3365 | `			"ArgumentCountError",` |
|       - | 3366 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3367 | `			nArg` |
|       - | 3368 | `			);` |
|       - | 3369 | `	}` |
|       - | 3370 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3371 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3372 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3373 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3374 | `			"TypeError",` |
|       - | 3375 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3376 | `			ph7_type_name(apArg[0])` |
|       - | 3377 | `			);` |
|       - | 3378 | `	}` |
|       - | 3379 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3380 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3381 | `	/* Create a new array */` |
|      25 | 3382 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3383 | `	if( pArray == 0 ){` |
|     ! 0 | 3384 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3385 | `		return PH7_OK;` |
|       - | 3386 | `	}` |
|       - | 3387 | `	/* Perform the requested operation */` |
|      25 | 3388 | `	pNode = pMap->pFirst;` |
|      83 | 3389 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3390 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3391 | `		if( pObj ){` |
|       - | 3392 | `			/* perform the insertion */` |
|      59 | 3393 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3394 | `		}` |
|       - | 3395 | `		/* Point to the next entry */` |
|      59 | 3396 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3397 | `	}` |
|       - | 3398 | `	/* return the new array */` |
|      25 | 3399 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3400 | `	return PH7_OK;` |
|      17 | 3401 |  |
|       - | 3402 | `/*` |
|       - | 3403 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3404 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3405 | ` * Parameters` |
|       - | 3406 | ` *  $input` |
|       - | 3407 | ` *   An array containing keys to return.` |
|       - | 3408 | ` * $search_value` |
|       - | 3409 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3410 | ` * $strict` |
|       - | 3411 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3412 | ` * Return` |
|       - | 3413 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3414 | ` */` |
|     122 | 3415 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3416 |  |
|       - | 3417 | `	ph7_hashmap_node *pNode;` |
|       - | 3418 | `	ph7_hashmap *pMap;` |
|       - | 3419 | `	ph7_value *pArray;` |
|       - | 3420 | `	ph7_value sObj;` |
|       - | 3421 | `	ph7_value sVal;` |
|       - | 3422 | `	SyString sKey;` |
|       - | 3423 | `	int bStrict;` |
|       - | 3424 | `	sxi32 rc;` |
|       - | 3425 | `	sxu32 n;` |
|     124 | 3426 | `	if( nArg < 1 ){` |
|       - | 3427 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3428 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3429 | `			"ArgumentCountError",` |
|       - | 3430 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3431 | `			);` |
|       - | 3432 | `	}` |
|       - | 3433 | `	/* Make sure we are dealing with a valid hashmap */` |
|     122 | 3434 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3435 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3436 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3437 | `			"TypeError",` |
|       - | 3438 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3439 | `			ph7_type_name(apArg[0])` |
|       - | 3440 | `			);` |
|       - | 3441 | `	}` |
|       - | 3442 | `	/* Point to the internal representation of the input hashmap */` |
|     120 | 3443 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3444 | `	/* Create a new array */` |
|     120 | 3445 | `	pArray = ph7_context_new_array(pCtx);` |
|     120 | 3446 | `	if( pArray == 0 ){` |
|     ! 0 | 3447 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3448 | `		return PH7_OK;` |
|       - | 3449 | `	}` |
|     120 | 3450 | `	bStrict = FALSE;` |
|     120 | 3451 | `	if( nArg > 2 ){` |
|       - | 3452 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3453 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3454 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3455 | `				"TypeError",` |
|       - | 3456 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3457 | `				ph7_type_name(apArg[2])` |
|       - | 3458 | `				);` |
|       - | 3459 | `		}` |
|       5 | 3460 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3461 | `	}` |
|       - | 3462 | `	/* Perform the requested operation */` |
|     117 | 3463 | `	pNode = pMap->pFirst;` |
|     117 | 3464 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     559 | 3465 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     443 | 3466 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     121 | 3467 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      61 | 3468 | `		}else{` |
|     323 | 3469 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3470 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3471 | `		}` |
|     443 | 3472 | `		rc = 0;` |
|     443 | 3473 | `		if( nArg > 1 ){` |
|      31 | 3474 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3475 | `			if( pValue ){` |
|      31 | 3476 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3477 | `				/* Filter key */` |
|      31 | 3478 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3479 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3480 | `			}` |
|      15 | 3481 | `		}` |
|     443 | 3482 | `		if( rc == 0 ){` |
|       - | 3483 | `			/* Perform the insertion */` |
|     425 | 3484 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     212 | 3485 | `		}` |
|     443 | 3486 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3487 | `		/* Point to the next entry */` |
|     443 | 3488 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     222 | 3489 | `	}` |
|       - | 3490 | `	/* return the new array */` |
|     117 | 3491 | `	ph7_result_value(pCtx,pArray);` |
|     117 | 3492 | `	return PH7_OK;` |
|      63 | 3493 |  |
|       - | 3494 | `/*` |
|       - | 3495 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3496 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3497 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3498 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3499 | ` * Parameters` |
|       - | 3500 | ` *  $arr1` |
|       - | 3501 | ` *   First array` |
|       - | 3502 | ` *  $arr2` |
|       - | 3503 | ` *   Second array` |
|       - | 3504 | ` * Return` |
|       - | 3505 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3506 | ` * Note` |
|       - | 3507 | ` *  This function is a symisc eXtension.` |
|       - | 3508 | ` */` |
|       4 | 3509 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3510 |  |
|       - | 3511 | `	ph7_hashmap *p1,*p2;` |
|       - | 3512 | `	int rc;` |
|       5 | 3513 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3514 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3515 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3516 | `		return PH7_OK;` |
|       - | 3517 | `	}` |
|       - | 3518 | `	/* Point to the hashmaps */` |
|       5 | 3519 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3520 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3521 | `	rc = (p1 == p2);` |
|       - | 3522 | `	/* Same instance? */` |
|       5 | 3523 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3524 | `	return PH7_OK;` |
|       3 | 3525 |  |
|       - | 3526 | `/*` |
|       - | 3527 | ` * array array_merge(array ...$arrays)` |
|       - | 3528 | ` *  Merge one or more arrays.` |
|       - | 3529 | ` * Parameters` |
|       - | 3530 | ` *  ...$arrays` |
|       - | 3531 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3532 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3533 | ` * Return` |
|       - | 3534 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3535 | ` *  with no arguments.` |
|       - | 3536 | ` */` |
|     950 | 3537 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3538 |  |
|       - | 3539 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3540 | `	ph7_value *pArray;` |
|       - | 3541 | `	int i;` |
|       - | 3542 | `	/* Create a new array */` |
|     952 | 3543 | `	pArray = ph7_context_new_array(pCtx);` |
|     952 | 3544 | `	if( pArray == 0 ){` |
|     ! 0 | 3545 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3546 | `		return PH7_OK;` |
|       - | 3547 | `	}` |
|       - | 3548 | `	/* Point to the internal representation of the hashmap */` |
|     952 | 3549 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3550 | `	/* Start merging */` |
|    2842 | 3551 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3552 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1896 | 3553 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3554 | `			/* Type mismatch -> TypeError */` |
|       7 | 3555 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3556 | `				"TypeError",` |
|       - | 3557 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3558 | `				i + 1,` |
|       4 | 3559 | `				ph7_type_name(apArg[i])` |
|       - | 3560 | `				);` |
|     ! 0 | 3561 | `		}else{` |
|    1892 | 3562 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3563 | `			/* Merge the two hashmaps */` |
|    1892 | 3564 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3565 | `		}` |
|     947 | 3566 | `	}` |
|       - | 3567 | `	/* Return the freshly created array */` |
|     948 | 3568 | `	ph7_result_value(pCtx,pArray);` |
|     948 | 3569 | `	return PH7_OK;` |
|     477 | 3570 |  |
|       - | 3571 | `/*` |
|       - | 3572 | ` * array array_copy(array $source)` |
|       - | 3573 | ` *  Make a blind copy of the target array.` |
|       - | 3574 | ` * Parameters` |
|       - | 3575 | ` *  $source` |
|       - | 3576 | ` *   Target array` |
|       - | 3577 | ` * Return` |
|       - | 3578 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3579 | ` * Note` |
|       - | 3580 | ` *  This function is a symisc eXtension.` |
|       - | 3581 | ` */` |
|      16 | 3582 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3583 |  |
|       - | 3584 | `	ph7_hashmap *pMap;` |
|       - | 3585 | `	ph7_value *pArray;` |
|      17 | 3586 | `	if( nArg < 1 ){` |
|       - | 3587 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3588 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3589 | `		return PH7_OK;` |
|       - | 3590 | `	}` |
|       - | 3591 | `	/* Create a new array */` |
|      17 | 3592 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3593 | `	if( pArray == 0 ){` |
|     ! 0 | 3594 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3595 | `		return PH7_OK;` |
|       - | 3596 | `	}` |
|       - | 3597 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3598 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3599 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3600 | `		/* Point to the internal representation of the source */` |
|      17 | 3601 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3602 | `		/* Perform the copy */` |
|      17 | 3603 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3604 | `	}else{` |
|       - | 3605 | `		/* Simple insertion */` |
|     ! 0 | 3606 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3607 | `	}` |
|       - | 3608 | `	/* Return the duplicated array */` |
|      17 | 3609 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3610 | `	return PH7_OK;` |
|       9 | 3611 |  |
|       - | 3612 | `/*` |
|       - | 3613 | ` * bool array_erase(array $source)` |
|       - | 3614 | ` *  Remove all elements from a given array.` |
|       - | 3615 | ` * Parameters` |
|       - | 3616 | ` *  $source` |
|       - | 3617 | ` *   Target array` |
|       - | 3618 | ` * Return` |
|       - | 3619 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3620 | ` * Note` |
|       - | 3621 | ` *  This function is a symisc eXtension.` |
|       - | 3622 | ` */` |
|      16 | 3623 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3624 |  |
|       - | 3625 | `	ph7_hashmap *pMap;` |
|      17 | 3626 | `	if( nArg < 1 ){` |
|       - | 3627 | `		/* Missing arguments */` |
|     ! 0 | 3628 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3629 | `		return PH7_OK;` |
|       - | 3630 | `	}` |
|       - | 3631 | `	/* Point to the target hashmap */` |
|      17 | 3632 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 3633 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3634 | `	/* Erase */` |
|      17 | 3635 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3636 | `	return PH7_OK;` |
|       9 | 3637 |  |
|       - | 3638 | `/*` |
|       - | 3639 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3640 | ` *  Extract a slice of the array.` |
|       - | 3641 | ` * Parameters` |
|       - | 3642 | ` *  $array` |
|       - | 3643 | ` *    The input array.` |
|       - | 3644 | ` * $offset` |
|       - | 3645 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3646 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3647 | ` * $length (optional, nullable)` |
|       - | 3648 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3649 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3650 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3651 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3652 | ` * $preserve_keys (optional)` |
|       - | 3653 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3654 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3655 | ` * Return` |
|       - | 3656 | ` *   The new slice.` |
|       - | 3657 | ` */` |
|      46 | 3658 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3659 |  |
|       - | 3660 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3661 | `	ph7_hashmap_node *pCur;` |
|       - | 3662 | `	ph7_value *pArray;` |
|       - | 3663 | `	int iLength,iOfft;` |
|       - | 3664 | `	int bPreserve;` |
|       - | 3665 | `	sxi32 rc;` |
|      48 | 3666 | `	if( nArg < 2 ){` |
|       7 | 3667 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3668 | `			"ArgumentCountError",` |
|       - | 3669 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3670 | `			nArg` |
|       - | 3671 | `			);` |
|       - | 3672 | `	}` |
|      44 | 3673 | `	if( nArg > 4 ){` |
|       4 | 3674 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3675 | `			"ArgumentCountError",` |
|       - | 3676 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3677 | `			nArg` |
|       - | 3678 | `			);` |
|       - | 3679 | `	}` |
|      42 | 3680 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3681 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3682 | `			"TypeError",` |
|       - | 3683 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3684 | `			ph7_type_name(apArg[0])` |
|       - | 3685 | `			);` |
|       - | 3686 | `	}` |
|       - | 3687 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3688 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3689 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3690 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3691 | `			"TypeError",` |
|       - | 3692 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3693 | `			ph7_type_name(apArg[1])` |
|       - | 3694 | `			);` |
|       - | 3695 | `	}` |
|       - | 3696 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3697 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3698 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3699 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3700 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3701 | `				"TypeError",` |
|       - | 3702 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3703 | `				ph7_type_name(apArg[2])` |
|       - | 3704 | `				);` |
|       - | 3705 | `		}` |
|       8 | 3706 | `	}` |
|       - | 3707 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3708 | `	if( nArg > 3 ){` |
|      10 | 3709 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3710 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3711 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3712 | `				"TypeError",` |
|       - | 3713 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3714 | `				ph7_type_name(apArg[3])` |
|       - | 3715 | `				);` |
|       - | 3716 | `		}` |
|       2 | 3717 | `	}` |
|       - | 3718 | `	/* Point the internal representation of the target array */` |
|      33 | 3719 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3720 | `	bPreserve = FALSE;` |
|       - | 3721 | `	/* Get the offset */` |
|      33 | 3722 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3723 | `	if( iOfft < 0 ){` |
|       5 | 3724 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3725 | `		if( iOfft < 0 ){` |
|       3 | 3726 | `			iOfft = 0;` |
|       1 | 3727 | `		}` |
|       2 | 3728 | `	}` |
|      33 | 3729 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3730 | `		/* Offset past end of array, return empty array */` |
|       5 | 3731 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3732 | `		if( pArray == 0 ){` |
|     ! 0 | 3733 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3734 | `			return PH7_OK;` |
|       - | 3735 | `		}` |
|       5 | 3736 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3737 | `		return PH7_OK;` |
|       - | 3738 | `	}` |
|       - | 3739 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3740 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3741 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3742 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3743 | `		if( iLength < 0 ){` |
|       5 | 3744 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3745 | `		}` |
|      15 | 3746 | `		if( iLength < 0 ){` |
|       3 | 3747 | `			iLength = 0;` |
|       1 | 3748 | `		}` |
|      15 | 3749 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3750 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3751 | `		}` |
|       7 | 3752 | `	}` |
|      29 | 3753 | `	if( nArg > 3 ){` |
|       5 | 3754 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3755 | `	}` |
|       - | 3756 | `	/* Create a new array */` |
|      29 | 3757 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3758 | `	if( pArray == 0 ){` |
|     ! 0 | 3759 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3760 | `		return PH7_OK;` |
|       - | 3761 | `	}` |
|      29 | 3762 | `	if( iLength < 1 ){` |
|       - | 3763 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3764 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3765 | `		return PH7_OK;` |
|       - | 3766 | `	}` |
|       - | 3767 | `	/* Point to the desired entry */` |
|      25 | 3768 | `	pCur = pSrc->pFirst;` |
|      24 | 3769 | `	for(;;){` |
|      49 | 3770 | `		if( iOfft < 1 ){` |
|      25 | 3771 | `			break;` |
|       - | 3772 | `		}` |
|       - | 3773 | `		/* Point to the next entry */` |
|      25 | 3774 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3775 | `		iOfft--;` |
|       1 | 3776 | `	}` |
|       - | 3777 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3778 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3779 | `	for(;;){` |
|      79 | 3780 | `		if( iLength < 1 ){` |
|      25 | 3781 | `			break;` |
|       - | 3782 | `		}` |
|       - | 3783 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3784 | `		{` |
|      55 | 3785 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3786 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3787 | `		}` |
|      55 | 3788 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3789 | `			break;` |
|       - | 3790 | `		}` |
|       - | 3791 | `		/* Point to the next entry */` |
|      55 | 3792 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3793 | `		iLength--;` |
|       1 | 3794 | `	}` |
|       - | 3795 | `	/* Return the freshly created array */` |
|      25 | 3796 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3797 | `	return PH7_OK;` |
|      25 | 3798 |  |
|       - | 3799 | `/*` |
|       - | 3800 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 3801 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 3802 | ` * beginning (becomes the new pFirst).` |
|       - | 3803 | ` */` |
|      30 | 3804 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 3805 |  |
|       - | 3806 | `	ph7_hashmap_node *pNode;` |
|       - | 3807 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 3808 | `	pNode = pMap->pLast;` |
|      31 | 3809 | `	if( pNode == 0 ){` |
|     ! 0 | 3810 | `		return;` |
|       - | 3811 | `	}` |
|      31 | 3812 | `	if( pNode->pNext == 0 ){` |
|       - | 3813 | `		/* Only node in the list, nothing to move */` |
|       5 | 3814 | `		return;` |
|       - | 3815 | `	}` |
|      27 | 3816 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 3817 | `		/* Already in the correct position */` |
|       9 | 3818 | `		return;` |
|       - | 3819 | `	}` |
|       - | 3820 | `	/* Unlink pNode from the end of the list */` |
|      19 | 3821 | `	pMap->pLast = pNode->pNext;` |
|      19 | 3822 | `	pMap->pLast->pPrev = 0;` |
|       - | 3823 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 3824 | `	if( pAfter == 0 ){` |
|       - | 3825 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 3826 | `		pNode->pNext = 0;` |
|       3 | 3827 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 3828 | `		if( pMap->pFirst ){` |
|       3 | 3829 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 3830 | `		}` |
|       3 | 3831 | `		pMap->pFirst = pNode;` |
|       2 | 3832 | `	}else{` |
|      17 | 3833 | `		pOldNext = pAfter->pPrev;` |
|      17 | 3834 | `		pNode->pPrev = pOldNext;` |
|      17 | 3835 | `		pNode->pNext = pAfter;` |
|      17 | 3836 | `		pAfter->pPrev = pNode;` |
|      17 | 3837 | `		if( pOldNext ){` |
|      17 | 3838 | `			pOldNext->pNext = pNode;` |
|       9 | 3839 | `		}else{` |
|     ! 0 | 3840 | `			pMap->pLast = pNode;` |
|       - | 3841 | `		}` |
|       - | 3842 | `	}` |
|      16 | 3843 |  |
|       - | 3844 | `/*` |
|       - | 3845 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 3846 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3847 | ` * Parameters` |
|       - | 3848 | ` *  $array` |
|       - | 3849 | ` *    The input array.` |
|       - | 3850 | ` *  $offset` |
|       - | 3851 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 3852 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 3853 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 3854 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 3855 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 3856 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 3857 | ` *  $length (optional)` |
|       - | 3858 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 3859 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 3860 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 3861 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 3862 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 3863 | ` *  $replacement (optional)` |
|       - | 3864 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 3865 | ` *    with elements from this array.` |
|       - | 3866 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 3867 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 3868 | ` *    offset.` |
|       - | 3869 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 3870 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 3871 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 3872 | ` * Return` |
|       - | 3873 | ` *   A new array consisting of the extracted elements.` |
|       - | 3874 | ` */` |
|      54 | 3875 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3876 |  |
|       - | 3877 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 3878 | `	ph7_value *pArray,*pRvalue;` |
|       - | 3879 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3880 | `	int iLength,iOfft,i;` |
|       - | 3881 | `	sxi32 rc;` |
|      56 | 3882 | `	if( nArg < 2 ){` |
|       7 | 3883 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3884 | `			"ArgumentCountError",` |
|       - | 3885 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 3886 | `			nArg` |
|       - | 3887 | `			);` |
|       - | 3888 | `	}` |
|      52 | 3889 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3890 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3891 | `			"TypeError",` |
|       - | 3892 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3893 | `			ph7_type_name(apArg[0])` |
|       - | 3894 | `			);` |
|       - | 3895 | `	}` |
|       - | 3896 | `	/* Point to the internal representation of the target array */` |
|      49 | 3897 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 3898 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3899 | `	/* Get the offset and clamp to valid range */` |
|      49 | 3900 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 3901 | `	if( iOfft < 0 ){` |
|       7 | 3902 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 3903 | `		if( iOfft < 0 ){` |
|       3 | 3904 | `			iOfft = 0;` |
|       2 | 3905 | `		}` |
|      46 | 3906 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 3907 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 3908 | `	}` |
|       - | 3909 | `	/* Get the length and clamp to valid range.` |
|       - | 3910 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 3911 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 3912 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 3913 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 3914 | `		if( iLength < 0 ){` |
|       7 | 3915 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 3916 | `			if( iLength < 0 ){` |
|       3 | 3917 | `				iLength = 0;` |
|       1 | 3918 | `			}` |
|       3 | 3919 | `		}` |
|      31 | 3920 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3921 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3922 | `		}` |
|      15 | 3923 | `	}` |
|       - | 3924 | `	/* Create the result array for removed elements */` |
|      49 | 3925 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3926 | `	if( pArray == 0 ){` |
|     ! 0 | 3927 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3928 | `		return PH7_OK;` |
|       - | 3929 | `	}` |
|       - | 3930 | `	/* Get replacement array if provided */` |
|      49 | 3931 | `	pRep = 0;` |
|      49 | 3932 | `	if( nArg > 3 ){` |
|      21 | 3933 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3934 | `			/* Perform an array cast */` |
|       3 | 3935 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 3936 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 3937 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 3938 | `			}` |
|       2 | 3939 | `		}else{` |
|      19 | 3940 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3941 | `		}` |
|      21 | 3942 | `		if( pRep ){` |
|       - | 3943 | `			/* Reset the loop cursor */` |
|      21 | 3944 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 3945 | `		}` |
|      10 | 3946 | `	}` |
|       - | 3947 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 3948 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 3949 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 3950 | `		return PH7_OK;` |
|       - | 3951 | `	}` |
|       - | 3952 | `	/* Navigate to the offset position */` |
|      41 | 3953 | `	pCur = pSrc->pFirst;` |
|      85 | 3954 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 3955 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 3956 | `	}` |
|       - | 3957 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 3958 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 3959 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 3960 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 3961 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 3962 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 3963 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 3964 | `		pPrev = pCur->pPrev;` |
|      71 | 3965 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 3966 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 3967 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3968 | `			break;` |
|       - | 3969 | `		}` |
|      71 | 3970 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 3971 | `	}` |
|       - | 3972 | `	/* Insert replacement elements at the correct position */` |
|      41 | 3973 | `	if( pRep ){` |
|       - | 3974 | `		ph7_value sSafeVal;` |
|      61 | 3975 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 3976 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 3977 | `			if( pRvalue ){` |
|       - | 3978 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 3979 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 3980 | `				 * since it points into that same pool. */` |
|      31 | 3981 | `				sSafeVal = *pRvalue;` |
|      31 | 3982 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 3983 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 3984 | `					pNewNode = pSrc->pLast;` |
|      31 | 3985 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 3986 | `					pInsertAfter = pNewNode;` |
|      15 | 3987 | `				}` |
|      15 | 3988 | `			}` |
|       1 | 3989 | `		}` |
|      10 | 3990 | `	}` |
|       - | 3991 | `	/* Return the freshly created array */` |
|      41 | 3992 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 3993 | `	return PH7_OK;` |
|      29 | 3994 |  |
|       - | 3995 | `/*` |
|       - | 3996 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3997 | ` *  Checks if a value exists in an array.` |
|       - | 3998 | ` * Parameters` |
|       - | 3999 | ` *  $needle` |
|       - | 4000 | ` *   The searched value.` |
|       - | 4001 | ` *   Note:` |
|       - | 4002 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 4003 | ` * $haystack` |
|       - | 4004 | ` *  The target array.` |
|       - | 4005 | ` * $strict` |
|       - | 4006 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 4007 | ` *  will also check the types of the needle in the haystack.` |
|       - | 4008 | ` */` |
|   28990 | 4009 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4010 |  |
|       - | 4011 | `	ph7_value *pNeedle;` |
|       - | 4012 | `	int bStrict;` |
|       - | 4013 | `	int rc;` |
|   28992 | 4014 | `	if( nArg < 2 ){` |
|       - | 4015 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 4016 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4017 | `		return PH7_OK;` |
|       - | 4018 | `	}` |
|   28992 | 4019 | `	pNeedle = apArg[0];` |
|   28992 | 4020 | `	bStrict = 0;` |
|   28992 | 4021 | `	if( nArg > 2 ){` |
|       5 | 4022 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 4023 | `	}` |
|   28992 | 4024 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4025 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 4026 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 4027 | `		/* Set the comparison result */` |
|     ! 0 | 4028 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 4029 | `		return PH7_OK;` |
|       - | 4030 | `	}` |
|       - | 4031 | `	/* Perform the lookup */` |
|   28992 | 4032 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 4033 | `	/* Lookup result */` |
|   28992 | 4034 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   28992 | 4035 | `	return PH7_OK;` |
|   14497 | 4036 |  |
|       - | 4037 | `/*` |
|       - | 4038 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 4039 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 4040 | ` * Parameters` |
|       - | 4041 | ` * $needle` |
|       - | 4042 | ` *   The searched value.` |
|       - | 4043 | ` * $haystack` |
|       - | 4044 | ` *   The array.` |
|       - | 4045 | ` * $strict` |
|       - | 4046 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 4047 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 4048 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 4049 | ` * Return` |
|       - | 4050 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 4051 | ` */` |
|      28 | 4052 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4053 |  |
|       - | 4054 | `	ph7_hashmap_node *pEntry;` |
|       - | 4055 | `	ph7_value *pVal,sNeedle;` |
|       - | 4056 | `	ph7_hashmap *pMap;` |
|       - | 4057 | `	ph7_value sVal;` |
|       - | 4058 | `	int bStrict;` |
|       - | 4059 | `	sxu32 n;` |
|       - | 4060 | `	int rc;` |
|      30 | 4061 | `	if( nArg < 2 ){` |
|       - | 4062 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 4063 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4064 | `			"ArgumentCountError",` |
|       - | 4065 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4066 | `			nArg` |
|       - | 4067 | `			);` |
|       - | 4068 | `	}` |
|      26 | 4069 | `	bStrict = FALSE;` |
|      26 | 4070 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4071 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4072 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4073 | `			"TypeError",` |
|       - | 4074 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4075 | `			ph7_type_name(apArg[1])` |
|       - | 4076 | `			);` |
|       - | 4077 | `	}` |
|      24 | 4078 | `	if( nArg > 2 ){` |
|       - | 4079 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4080 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4081 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4082 | `				"TypeError",` |
|       - | 4083 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4084 | `				ph7_type_name(apArg[2])` |
|       - | 4085 | `				);` |
|       - | 4086 | `		}` |
|       9 | 4087 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4088 | `	}` |
|       - | 4089 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4090 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4091 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4092 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4093 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4094 | `	pEntry = pMap->pFirst;` |
|      21 | 4095 | `	n = pMap->nEntry;` |
|      23 | 4096 | `	for(;;){` |
|      47 | 4097 | `		if( !n ){` |
|       9 | 4098 | `			break;` |
|       - | 4099 | `		}` |
|       - | 4100 | `		/* Extract node value */` |
|      39 | 4101 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4102 | `		if( pVal ){` |
|       - | 4103 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4104 | `			 * can change their type.` |
|       - | 4105 | `			 */` |
|      39 | 4106 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4107 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4108 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4109 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4110 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4111 | `			if( rc == 0 ){` |
|       - | 4112 | `				/* Match found,return key */` |
|      13 | 4113 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4114 | `					/* INT key */` |
|       7 | 4115 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4116 | `				}else{` |
|       7 | 4117 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4118 | `					/* Blob key */` |
|       7 | 4119 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4120 | `				}` |
|      13 | 4121 | `				return PH7_OK;` |
|       - | 4122 | `			}` |
|      13 | 4123 | `		}` |
|       - | 4124 | `		/* Point to the next entry */` |
|      27 | 4125 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4126 | `		n--;` |
|       1 | 4127 | `	}` |
|       - | 4128 | `	/* No such value,return FALSE */` |
|       9 | 4129 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4130 | `	return PH7_OK;` |
|      16 | 4131 |  |
|       - | 4132 | `/*` |
|       - | 4133 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4134 | ` *  Computes the difference of arrays.` |
|       - | 4135 | ` * Parameters` |
|       - | 4136 | ` *  $array1` |
|       - | 4137 | ` *    The array to compare from` |
|       - | 4138 | ` *  $array2` |
|       - | 4139 | ` *    An array to compare against` |
|       - | 4140 | ` *  $...` |
|       - | 4141 | ` *   More arrays to compare against` |
|       - | 4142 | ` * Return` |
|       - | 4143 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4144 | ` *  are not present in any of the other arrays.` |
|       - | 4145 | ` */` |
|      22 | 4146 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4147 |  |
|       - | 4148 | `	ph7_hashmap_node *pEntry;` |
|       - | 4149 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4150 | `	ph7_value *pArray;` |
|       - | 4151 | `	ph7_value *pVal;` |
|       - | 4152 | `	sxi32 rc;` |
|       - | 4153 | `	sxu32 n;` |
|       - | 4154 | `	int i;` |
|       - | 4155 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4156 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4157 | `	 * debugging difficult. */` |
|      24 | 4158 | `	if( nArg < 1 ){` |
|       4 | 4159 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4160 | `			"ArgumentCountError",` |
|       - | 4161 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4162 | `			nArg` |
|       - | 4163 | `			);` |
|       - | 4164 | `	}` |
|      22 | 4165 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4166 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4167 | `			"TypeError",` |
|       - | 4168 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4169 | `			ph7_type_name(apArg[0])` |
|       - | 4170 | `			);` |
|       - | 4171 | `	}` |
|      36 | 4172 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4173 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4174 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4175 | `				"TypeError",` |
|       - | 4176 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4177 | `				i + 1,` |
|       2 | 4178 | `				ph7_type_name(apArg[i])` |
|       - | 4179 | `				);` |
|       - | 4180 | `		}` |
|       9 | 4181 | `	}` |
|      17 | 4182 | `	if( nArg == 1 ){` |
|       - | 4183 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4184 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4185 | `		return PH7_OK;` |
|       - | 4186 | `	}` |
|       - | 4187 | `	/* Create a new array */` |
|      15 | 4188 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4189 | `	if( pArray == 0 ){` |
|     ! 0 | 4190 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4191 | `		return PH7_OK;` |
|       - | 4192 | `	}` |
|       - | 4193 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4194 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4195 | `	/* Perform the diff */` |
|      15 | 4196 | `	pEntry = pSrc->pFirst;` |
|      15 | 4197 | `	n = pSrc->nEntry;` |
|      27 | 4198 | `	for(;;){` |
|      55 | 4199 | `		if( n < 1 ){` |
|      15 | 4200 | `			break;` |
|       - | 4201 | `		}` |
|       - | 4202 | `		/* Extract the node value */` |
|      41 | 4203 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4204 | `		if( pVal ){` |
|      69 | 4205 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4206 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4207 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4208 | `				/* Perform the lookup */` |
|      45 | 4209 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4210 | `				if( rc == SXRET_OK ){` |
|       - | 4211 | `					/* Value exist */` |
|      17 | 4212 | `					break;` |
|       - | 4213 | `				}` |
|      15 | 4214 | `			}` |
|      41 | 4215 | `			if( i >= nArg ){` |
|       - | 4216 | `				/* Perform the insertion */` |
|      25 | 4217 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4218 | `			}` |
|      20 | 4219 | `		}` |
|       - | 4220 | `		/* Point to the next entry */` |
|      41 | 4221 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4222 | `		n--;` |
|       1 | 4223 | `	}` |
|       - | 4224 | `	/* Return the freshly created array */` |
|      15 | 4225 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4226 | `	return PH7_OK;` |
|      13 | 4227 |  |
|       - | 4228 | `/*` |
|       - | 4229 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4230 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4231 | ` * Parameters` |
|       - | 4232 | ` *  $array1` |
|       - | 4233 | ` *    The array to compare from` |
|       - | 4234 | ` *  $array2` |
|       - | 4235 | ` *    An array to compare against` |
|       - | 4236 | ` *  $...` |
|       - | 4237 | ` *   More arrays to compare against.` |
|       - | 4238 | ` * $callback` |
|       - | 4239 | ` *  The callback comparison function.` |
|       - | 4240 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4241 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4242 | ` *  than the second.` |
|       - | 4243 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4244 | ` * Return` |
|       - | 4245 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4246 | ` *  are not present in any of the other arrays.` |
|       - | 4247 | ` */` |
|      20 | 4248 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4249 |  |
|       - | 4250 | `	ph7_hashmap_node *pEntry;` |
|       - | 4251 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4252 | `	ph7_value *pCallback;` |
|       - | 4253 | `	ph7_value *pArray;` |
|       - | 4254 | `	ph7_value *pVal;` |
|       - | 4255 | `	sxi32 rc;` |
|       - | 4256 | `	sxu32 n;` |
|       - | 4257 | `	int i;` |
|       - | 4258 |  |
|       - | 4259 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      22 | 4260 | `	if( nArg < 2 ){` |
|       4 | 4261 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4262 | `			"ArgumentCountError",` |
|       - | 4263 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4264 | `			nArg` |
|       - | 4265 | `			);` |
|       - | 4266 | `	}` |
|      20 | 4267 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4268 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4269 | `			"TypeError",` |
|       - | 4270 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4271 | `			ph7_type_name(apArg[0])` |
|       - | 4272 | `			);` |
|       - | 4273 | `	}` |
|       - | 4274 |  |
|      18 | 4275 | `	if( nArg == 2 ){` |
|       - | 4276 | `		/* Only the original array and the callback were provided. */` |
|       - | 4277 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4278 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4279 | `		 * validation order.` |
|       - | 4280 | `		 */` |
|       4 | 4281 | `	} else {` |
|       - | 4282 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      20 | 4283 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      14 | 4284 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      10 | 4285 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4286 | `					"TypeError",` |
|       - | 4287 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4288 | `					i + 1,` |
|       6 | 4289 | `					ph7_type_name(apArg[i])` |
|       - | 4290 | `					);` |
|       - | 4291 | `			}` |
|       5 | 4292 | `		}` |
|       - | 4293 | `	}` |
|       - | 4294 |  |
|       - | 4295 | `	/* Identify the callback (always expected as the last argument). */` |
|      12 | 4296 | `	pCallback = apArg[nArg - 1];` |
|       - | 4297 | `	/* Validate the callback to match PHP's error messages. */` |
|      12 | 4298 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       7 | 4299 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4300 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4301 | `				"TypeError",` |
|       - | 4302 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4303 | `				nArg` |
|       - | 4304 | `				);` |
|       - | 4305 | `		}` |
|       5 | 4306 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4307 | `			int len;` |
|       3 | 4308 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4309 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4310 | `				"TypeError",` |
|       - | 4311 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4312 | `				nArg,` |
|       1 | 4313 | `				zName` |
|       - | 4314 | `				);` |
|       - | 4315 | `		}` |
|       4 | 4316 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4317 | `			"TypeError",` |
|       - | 4318 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4319 | `			nArg` |
|       - | 4320 | `			);` |
|       - | 4321 | `	}` |
|       - | 4322 |  |
|       5 | 4323 | `	if( nArg == 2 ){` |
|       - | 4324 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4325 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4326 | `		return PH7_OK;` |
|       - | 4327 | `	}` |
|       - | 4328 |  |
|       - | 4329 | `	/* Create a new array */` |
|       3 | 4330 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4331 | `	if( pArray == 0 ){` |
|     ! 0 | 4332 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4333 | `		return PH7_OK;` |
|       - | 4334 | `	}` |
|       - | 4335 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4336 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4337 | `	/* Perform the diff */` |
|       3 | 4338 | `	pEntry = pSrc->pFirst;` |
|       3 | 4339 | `	n = pSrc->nEntry;` |
|       4 | 4340 | `	for(;;){` |
|       9 | 4341 | `		if( n < 1 ){` |
|       3 | 4342 | `			break;` |
|       - | 4343 | `		}` |
|       - | 4344 | `		/* Extract the node value */` |
|       7 | 4345 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4346 | `		if( pVal ){` |
|      11 | 4347 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4348 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4349 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4350 | `				/* Perform the lookup */` |
|       7 | 4351 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4352 | `				if( rc == SXRET_OK ){` |
|       - | 4353 | `					/* Value exist */` |
|       3 | 4354 | `					break;` |
|       - | 4355 | `				}` |
|       3 | 4356 | `			}` |
|       7 | 4357 | `			if( i >= (nArg - 1)){` |
|       - | 4358 | `				/* Perform the insertion */` |
|       5 | 4359 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4360 | `			}` |
|       3 | 4361 | `		}` |
|       - | 4362 | `		/* Point to the next entry */` |
|       7 | 4363 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4364 | `		n--;` |
|       1 | 4365 | `	}` |
|       - | 4366 | `	/* Return the freshly created array */` |
|       3 | 4367 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4368 | `	return PH7_OK;` |
|      12 | 4369 |  |
|       - | 4370 | `/*` |
|       - | 4371 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4372 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4373 | ` * Parameters` |
|       - | 4374 | ` *  $array1` |
|       - | 4375 | ` *    The array to compare from` |
|       - | 4376 | ` *  $array2` |
|       - | 4377 | ` *    An array to compare against` |
|       - | 4378 | ` *  $...` |
|       - | 4379 | ` *   More arrays to compare against` |
|       - | 4380 | ` * Return` |
|       - | 4381 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4382 | ` *  are not present in any of the other arrays.` |
|       - | 4383 | ` */` |
|      20 | 4384 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4385 |  |
|       - | 4386 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4387 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4388 | `	ph7_value *pArray;` |
|       - | 4389 | `	ph7_value *pVal;` |
|       - | 4390 | `	sxi32 rc;` |
|       - | 4391 | `	sxu32 n;` |
|       - | 4392 | `	int i;` |
|       - | 4393 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4394 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4395 | `	 * accompanying integration tests to pass. */` |
|      22 | 4396 | `	if( nArg < 1 ){` |
|       4 | 4397 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4398 | `			"ArgumentCountError",` |
|       - | 4399 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4400 | `			nArg` |
|       - | 4401 | `			);` |
|       - | 4402 | `	}` |
|      20 | 4403 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4404 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4405 | `			"TypeError",` |
|       - | 4406 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4407 | `			ph7_type_name(apArg[0])` |
|       - | 4408 | `			);` |
|       - | 4409 | `	}` |
|      32 | 4410 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4411 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4412 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4413 | `				"TypeError",` |
|       - | 4414 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4415 | `				i + 1,` |
|       4 | 4416 | `				ph7_type_name(apArg[i])` |
|       - | 4417 | `				);` |
|       - | 4418 | `		}` |
|       9 | 4419 | `	}` |
|      13 | 4420 | `	if( nArg == 1 ){` |
|       - | 4421 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4422 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4423 | `		return PH7_OK;` |
|       - | 4424 | `	}` |
|       - | 4425 | `	/* Create a new array */` |
|      11 | 4426 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4427 | `	if( pArray == 0 ){` |
|     ! 0 | 4428 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4429 | `		return PH7_OK;` |
|       - | 4430 | `	}` |
|       - | 4431 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4432 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4433 | `	/* Perform the diff */` |
|      11 | 4434 | `	pEntry = pSrc->pFirst;` |
|      11 | 4435 | `	n = pSrc->nEntry;` |
|      11 | 4436 | `	pN1 = pN2 = 0;` |
|      29 | 4437 | `	for(;;){` |
|       - | 4438 | `		int keep;` |
|      35 | 4439 | `		if( n < 1 ){` |
|      11 | 4440 | `			break;` |
|       - | 4441 | `		}` |
|       - | 4442 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4443 | `		keep = 1;` |
|      41 | 4444 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4445 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4446 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4447 | `			/* Perform a key lookup first */` |
|      29 | 4448 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4449 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4450 | `			}else{` |
|      17 | 4451 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4452 | `			}` |
|      29 | 4453 | `			if( rc != SXRET_OK ){` |
|       - | 4454 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4455 | `				continue;` |
|       - | 4456 | `			}` |
|       - | 4457 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4458 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4459 | `			if( pVal ){` |
|       - | 4460 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4461 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4462 | `				if( pVal2 ){` |
|      15 | 4463 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4464 | `					if( cmp == 0 ){` |
|       - | 4465 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4466 | `						keep = 0;` |
|      13 | 4467 | `						break;` |
|       - | 4468 | `					}` |
|       1 | 4469 | `				}` |
|       1 | 4470 | `			}` |
|       2 | 4471 | `		}` |
|      25 | 4472 | `		if( keep ){` |
|       - | 4473 | `			/* Perform the insertion */` |
|      13 | 4474 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4475 | `		}` |
|       - | 4476 | `		/* Point to the next entry */` |
|      25 | 4477 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4478 | `		n--;` |
|       1 | 4479 | `	}` |
|       - | 4480 | `	/* Return the freshly created array */` |
|      11 | 4481 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4482 | `	return PH7_OK;` |
|      12 | 4483 |  |
|       - | 4484 | `/*` |
|       - | 4485 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4486 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4487 | ` *  by a user supplied callback function.` |
|       - | 4488 | ` * Parameters` |
|       - | 4489 | ` *  $array1` |
|       - | 4490 | ` *    The array to compare from` |
|       - | 4491 | ` *  $array2` |
|       - | 4492 | ` *    An array to compare against` |
|       - | 4493 | ` *  $...` |
|       - | 4494 | ` *   More arrays to compare against.` |
|       - | 4495 | ` *  $key_compare_func` |
|       - | 4496 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4497 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4498 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4499 | ` * Return` |
|       - | 4500 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4501 | ` *  are not present in any of the other arrays.` |
|       - | 4502 | ` */` |
|      22 | 4503 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4504 |  |
|       - | 4505 | `	ph7_hashmap_node *pEntry;` |
|       - | 4506 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4507 | `	ph7_value *pCallback;` |
|       - | 4508 | `	ph7_value *pArray;` |
|       - | 4509 | `	sxi32 rc;` |
|       - | 4510 | `	sxu32 n;` |
|       - | 4511 | `	int i;` |
|       - | 4512 |  |
|       - | 4513 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4514 | `	if( nArg < 2 ){` |
|       4 | 4515 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4516 | `			"ArgumentCountError",` |
|       - | 4517 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4518 | `			nArg` |
|       - | 4519 | `			);` |
|       - | 4520 | `	}` |
|      22 | 4521 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4522 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4523 | `			"TypeError",` |
|       - | 4524 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4525 | `			ph7_type_name(apArg[0])` |
|       - | 4526 | `			);` |
|       - | 4527 | `	}` |
|       - | 4528 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4529 | `	 * expected to be a callback. */` |
|      32 | 4530 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4531 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4532 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4533 | `				"TypeError",` |
|       - | 4534 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4535 | `				i + 1,` |
|       2 | 4536 | `				ph7_type_name(apArg[i])` |
|       - | 4537 | `				);` |
|       - | 4538 | `		}` |
|       8 | 4539 | `	}` |
|       - | 4540 | `	/* Point to the callback value */` |
|      18 | 4541 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4542 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4543 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4544 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4545 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4546 | `		 * string given" which we also reproduce. */` |
|       7 | 4547 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4548 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4549 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4550 | `				"TypeError",` |
|       - | 4551 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4552 | `				nArg` |
|       - | 4553 | `				);` |
|       - | 4554 | `		}` |
|       5 | 4555 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4556 | `			/* neither array nor string */` |
|       7 | 4557 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4558 | `				"TypeError",` |
|       - | 4559 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4560 | `				nArg` |
|       - | 4561 | `				);` |
|       - | 4562 | `		}` |
|       - | 4563 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4564 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4565 | `			"TypeError",` |
|       - | 4566 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4567 | `			nArg,` |
|     ! 0 | 4568 | `			ph7_type_name(pCallback)` |
|       - | 4569 | `			);` |
|       - | 4570 | `	}` |
|      11 | 4571 | `	if( nArg == 2 ){` |
|       - | 4572 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4573 | `		 * input array. */` |
|       3 | 4574 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4575 | `		return PH7_OK;` |
|       - | 4576 | `	}` |
|       - | 4577 | `	/* Create a new array */` |
|       9 | 4578 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4579 | `	if( pArray == 0 ){` |
|     ! 0 | 4580 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4581 | `		return PH7_OK;` |
|       - | 4582 | `	}` |
|       - | 4583 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4584 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4585 | `	/* Perform the diff */` |
|       9 | 4586 | `	pEntry = pSrc->pFirst;` |
|       9 | 4587 | `	n = pSrc->nEntry;` |
|      20 | 4588 | `	for(;;){` |
|       - | 4589 | `		int keep;` |
|      25 | 4590 | `		if( n < 1 ){` |
|       9 | 4591 | `			break;` |
|       - | 4592 | `		}` |
|      17 | 4593 | `		keep = 1;` |
|      29 | 4594 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4595 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4596 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4597 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4598 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4599 | `			while( pIt ){` |
|       - | 4600 | `				/* build temporary key values for callback */` |
|       - | 4601 | `				ph7_value key1, key2, result;` |
|       - | 4602 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4603 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4604 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4605 | `				}else{` |
|       - | 4606 | `					SyString sStr;` |
|      31 | 4607 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4608 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4609 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4610 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4611 | `				}` |
|      31 | 4612 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4613 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4614 | `				}else{` |
|       - | 4615 | `					SyString sStr;` |
|      31 | 4616 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4617 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4618 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4619 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4620 | `				}` |
|      31 | 4621 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4622 | `				/* call user callback with (key1, key2) */` |
|       - | 4623 | `				{` |
|       - | 4624 | `					ph7_value *apK[2];` |
|      31 | 4625 | `					apK[0] = &key1;` |
|      31 | 4626 | `					apK[1] = &key2;` |
|      31 | 4627 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4628 | `				}` |
|      31 | 4629 | `				if( rc == SXRET_OK ){` |
|      31 | 4630 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4631 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4632 | `					}` |
|      31 | 4633 | `					if( result.x.iVal == 0 ){` |
|       - | 4634 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4635 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4636 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4637 | `						if( pVal1 && pVal2 ){` |
|      13 | 4638 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4639 | `								keep = 0;` |
|       9 | 4640 | `								PH7_MemObjRelease(&result);` |
|       - | 4641 | `								/* release keys too before breaking */` |
|       9 | 4642 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4643 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4644 | `								break;` |
|       - | 4645 | `							}` |
|       2 | 4646 | `						}` |
|       2 | 4647 | `					}` |
|      11 | 4648 | `				}` |
|      23 | 4649 | `				PH7_MemObjRelease(&result);` |
|      23 | 4650 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4651 | `				PH7_MemObjRelease(&key2);` |
|       - | 4652 | `				/* move to next node */` |
|      23 | 4653 | `				pIt = pIt->pPrev;` |
|      23 | 4654 | `				if( keep == 0 ) break;` |
|       1 | 4655 | `			}` |
|      21 | 4656 | `			if( keep == 0 ) break;` |
|       7 | 4657 | `		}` |
|      17 | 4658 | `		if( keep ){` |
|       - | 4659 | `			/* Perform the insertion */` |
|       9 | 4660 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4661 | `		}` |
|       - | 4662 | `		/* Point to the next entry */` |
|      17 | 4663 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4664 | `		n--;` |
|       1 | 4665 | `	}` |
|       - | 4666 | `	/* Return the freshly created array */` |
|       9 | 4667 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4668 | `	return PH7_OK;` |
|      13 | 4669 |  |
|       - | 4670 | `/*` |
|       - | 4671 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4672 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4673 | ` * Parameters` |
|       - | 4674 | ` *  $array1` |
|       - | 4675 | ` *    The array to compare from` |
|       - | 4676 | ` *  $array2` |
|       - | 4677 | ` *    An array to compare against` |
|       - | 4678 | ` *  $...` |
|       - | 4679 | ` *   More arrays to compare against` |
|       - | 4680 | ` * Return` |
|       - | 4681 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4682 | ` *  in any of the other arrays.` |
|       - | 4683 | ` * Note that NULL is returned on failure.` |
|       - | 4684 | ` */` |
|      14 | 4685 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4686 |  |
|       - | 4687 | `	ph7_hashmap_node *pEntry;` |
|       - | 4688 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4689 | `	ph7_value *pArray;` |
|       - | 4690 | `	sxi32 rc;` |
|       - | 4691 | `	sxu32 n;` |
|       - | 4692 | `	int i;` |
|       - | 4693 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4694 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4695 | `	 * helpers. */` |
|      16 | 4696 | `	if( nArg < 1 ){` |
|       4 | 4697 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4698 | `			"ArgumentCountError",` |
|       - | 4699 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4700 | `			nArg` |
|       - | 4701 | `			);` |
|       - | 4702 | `	}` |
|      14 | 4703 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4704 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4705 | `			"TypeError",` |
|       - | 4706 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4707 | `			ph7_type_name(apArg[0])` |
|       - | 4708 | `			);` |
|       - | 4709 | `	}` |
|      20 | 4710 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4711 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4712 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4713 | `				"TypeError",` |
|       - | 4714 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4715 | `				i + 1,` |
|       2 | 4716 | `				ph7_type_name(apArg[i])` |
|       - | 4717 | `				);` |
|       - | 4718 | `		}` |
|       5 | 4719 | `	}` |
|       9 | 4720 | `	if( nArg == 1 ){` |
|       - | 4721 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4722 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4723 | `		return PH7_OK;` |
|       - | 4724 | `	}` |
|       - | 4725 | `	/* Create a new array */` |
|       7 | 4726 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4727 | `	if( pArray == 0 ){` |
|     ! 0 | 4728 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4729 | `		return PH7_OK;` |
|       - | 4730 | `	}` |
|       - | 4731 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4732 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4733 | `	/* Perfrom the diff */` |
|       7 | 4734 | `	pEntry = pSrc->pFirst;` |
|       7 | 4735 | `	n = pSrc->nEntry;` |
|      12 | 4736 | `	for(;;){` |
|      25 | 4737 | `		if( n < 1 ){` |
|       7 | 4738 | `			break;` |
|       - | 4739 | `		}` |
|      31 | 4740 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4741 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4742 | `				/* ignore */` |
|     ! 0 | 4743 | `				continue;` |
|       - | 4744 | `			}` |
|      23 | 4745 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4746 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4747 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4748 | `				/* Blob lookup */` |
|      17 | 4749 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4750 | `			}else{` |
|       - | 4751 | `				/* Int lookup */` |
|       7 | 4752 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4753 | `			}` |
|      23 | 4754 | `			if( rc == SXRET_OK ){` |
|       - | 4755 | `				/* Key exists,break immediately */` |
|      11 | 4756 | `				break;` |
|       - | 4757 | `			}` |
|       7 | 4758 | `		}` |
|      19 | 4759 | `		if( i >= nArg ){` |
|       - | 4760 | `			/* Perform the insertion */` |
|       9 | 4761 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4762 | `		}` |
|       - | 4763 | `		/* Point to the next entry */` |
|      19 | 4764 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4765 | `		n--;` |
|       1 | 4766 | `	}` |
|       - | 4767 | `	/* Return the freshly created array */` |
|       7 | 4768 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4769 | `	return PH7_OK;` |
|       9 | 4770 |  |
|       - | 4771 | `/*` |
|       - | 4772 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4773 | ` *  Computes the intersection of arrays.` |
|       - | 4774 | ` * Parameters` |
|       - | 4775 | ` *  $array1` |
|       - | 4776 | ` *    The array to compare from` |
|       - | 4777 | ` *  $array2` |
|       - | 4778 | ` *    An array to compare against` |
|       - | 4779 | ` *  $...` |
|       - | 4780 | ` *   More arrays to compare against` |
|       - | 4781 | ` * Return` |
|       - | 4782 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4783 | ` *  in all of the parameters.` |
|       - | 4784 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4785 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4786 | ` */` |
|      22 | 4787 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4788 |  |
|       - | 4789 | `	ph7_hashmap_node *pEntry;` |
|       - | 4790 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4791 | `	ph7_value *pArray;` |
|       - | 4792 | `	ph7_value *pVal;` |
|       - | 4793 | `	sxi32 rc;` |
|       - | 4794 | `	sxu32 n;` |
|       - | 4795 | `	int i;` |
|      24 | 4796 | `	if( nArg < 1 ){` |
|       4 | 4797 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4798 | `			"ArgumentCountError",` |
|       - | 4799 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4800 | `			nArg` |
|       - | 4801 | `			);` |
|       - | 4802 | `	}` |
|      22 | 4803 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4804 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4805 | `			"TypeError",` |
|       - | 4806 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4807 | `			ph7_type_name(apArg[0])` |
|       - | 4808 | `			);` |
|       - | 4809 | `	}` |
|      36 | 4810 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4811 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4812 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4813 | `				"TypeError",` |
|       - | 4814 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4815 | `				i + 1,` |
|       2 | 4816 | `				ph7_type_name(apArg[i])` |
|       - | 4817 | `				);` |
|       - | 4818 | `		}` |
|       9 | 4819 | `	}` |
|      17 | 4820 | `	if( nArg == 1 ){` |
|       - | 4821 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4822 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4823 | `		return PH7_OK;` |
|       - | 4824 | `	}` |
|       - | 4825 | `	/* Create a new array */` |
|      15 | 4826 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4827 | `	if( pArray == 0 ){` |
|     ! 0 | 4828 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4829 | `		return PH7_OK;` |
|       - | 4830 | `	}` |
|       - | 4831 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4832 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4833 | `	/* Perform the intersection */` |
|      15 | 4834 | `	pEntry = pSrc->pFirst;` |
|      15 | 4835 | `	n = pSrc->nEntry;` |
|      31 | 4836 | `	for(;;){` |
|      63 | 4837 | `		if( n < 1 ){` |
|      15 | 4838 | `			break;` |
|       - | 4839 | `		}` |
|       - | 4840 | `		/* Extract the node value */` |
|      49 | 4841 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4842 | `		if( pVal ){` |
|      79 | 4843 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4844 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4845 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4846 | `				/* Perform the lookup */` |
|      55 | 4847 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4848 | `				if( rc != SXRET_OK ){` |
|       - | 4849 | `					/* Value does not exist */` |
|      25 | 4850 | `					break;` |
|       - | 4851 | `				}` |
|      16 | 4852 | `			}` |
|      49 | 4853 | `			if( i >= nArg ){` |
|       - | 4854 | `				/* Perform the insertion */` |
|      25 | 4855 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4856 | `			}` |
|      24 | 4857 | `		}` |
|       - | 4858 | `		/* Point to the next entry */` |
|      49 | 4859 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4860 | `		n--;` |
|       1 | 4861 | `	}` |
|       - | 4862 | `	/* Return the freshly created array */` |
|      15 | 4863 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4864 | `	return PH7_OK;` |
|      13 | 4865 |  |
|       - | 4866 | `/*` |
|       - | 4867 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4868 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4869 | ` * Parameters` |
|       - | 4870 | ` *  $array1` |
|       - | 4871 | ` *    The array to compare from` |
|       - | 4872 | ` *  $array2` |
|       - | 4873 | ` *    An array to compare against` |
|       - | 4874 | ` *  $...` |
|       - | 4875 | ` *   More arrays to compare against` |
|       - | 4876 | ` * Return` |
|       - | 4877 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4878 | ` *  in all the arguments, with matching keys.` |
|       - | 4879 | ` */` |
|      22 | 4880 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4881 |  |
|       - | 4882 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4883 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4884 | `	ph7_value *pArray;` |
|       - | 4885 | `	ph7_value *pVal;` |
|       - | 4886 | `	sxi32 rc;` |
|       - | 4887 | `	sxu32 n;` |
|       - | 4888 | `	int i;` |
|      24 | 4889 | `	if( nArg < 1 ){` |
|       4 | 4890 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4891 | `			"ArgumentCountError",` |
|       - | 4892 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4893 | `			nArg` |
|       - | 4894 | `			);` |
|       - | 4895 | `	}` |
|      22 | 4896 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4897 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4898 | `			"TypeError",` |
|       - | 4899 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4900 | `			ph7_type_name(apArg[0])` |
|       - | 4901 | `			);` |
|       - | 4902 | `	}` |
|      36 | 4903 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4904 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4905 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4906 | `				"TypeError",` |
|       - | 4907 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4908 | `				i + 1,` |
|       2 | 4909 | `				ph7_type_name(apArg[i])` |
|       - | 4910 | `				);` |
|       - | 4911 | `		}` |
|       9 | 4912 | `	}` |
|      17 | 4913 | `	if( nArg == 1 ){` |
|       - | 4914 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4915 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4916 | `		return PH7_OK;` |
|       - | 4917 | `	}` |
|       - | 4918 | `	/* Create a new array */` |
|      15 | 4919 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4920 | `	if( pArray == 0 ){` |
|     ! 0 | 4921 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4922 | `		return PH7_OK;` |
|       - | 4923 | `	}` |
|       - | 4924 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4925 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4926 | `	/* Perform the intersection */` |
|      15 | 4927 | `	pEntry = pSrc->pFirst;` |
|      15 | 4928 | `	n = pSrc->nEntry;` |
|      15 | 4929 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4930 | `	for(;;){` |
|      47 | 4931 | `		if( n < 1 ){` |
|      15 | 4932 | `			break;` |
|       - | 4933 | `		}` |
|       - | 4934 | `		/* Extract the node value */` |
|      33 | 4935 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4936 | `		if( pVal ){` |
|      53 | 4937 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4938 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4939 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4940 | `				/* Perform a key lookup first */` |
|      37 | 4941 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4942 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4943 | `				}else{` |
|      23 | 4944 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4945 | `				}` |
|      37 | 4946 | `				if( rc != SXRET_OK ){` |
|       - | 4947 | `					/* No such key,break immediately */` |
|       7 | 4948 | `					break;` |
|       - | 4949 | `				}` |
|       - | 4950 | `				/* Perform the lookup */` |
|      31 | 4951 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4952 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4953 | `					/* Value does not exist */` |
|       6 | 4954 | `					break;` |
|       - | 4955 | `				}` |
|      11 | 4956 | `			}` |
|      33 | 4957 | `			if( i >= nArg ){` |
|       - | 4958 | `				/* Perform the insertion */` |
|      17 | 4959 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4960 | `			}` |
|      16 | 4961 | `		}` |
|       - | 4962 | `		/* Point to the next entry */` |
|      33 | 4963 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4964 | `		n--;` |
|       1 | 4965 | `	}` |
|       - | 4966 | `	/* Return the freshly created array */` |
|      15 | 4967 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4968 | `	return PH7_OK;` |
|      13 | 4969 |  |
|       - | 4970 | `/*` |
|       - | 4971 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 4972 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4973 | ` * Parameters` |
|       - | 4974 | ` *  $array1` |
|       - | 4975 | ` *    The array to compare from` |
|       - | 4976 | ` *  $...` |
|       - | 4977 | ` *   More arrays to compare against` |
|       - | 4978 | ` * Return` |
|       - | 4979 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4980 | ` *  have keys that are present in all arguments.` |
|       - | 4981 | ` * Note that NULL is returned on failure.` |
|       - | 4982 | ` */` |
|      22 | 4983 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4984 |  |
|       - | 4985 | `	ph7_hashmap_node *pEntry;` |
|       - | 4986 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4987 | `	ph7_value *pArray;` |
|       - | 4988 | `	sxi32 rc;` |
|       - | 4989 | `	sxu32 n;` |
|       - | 4990 | `	int i;` |
|      24 | 4991 | `	if( nArg < 1 ){` |
|       4 | 4992 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4993 | `			"ArgumentCountError",` |
|       - | 4994 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 4995 | `			nArg` |
|       - | 4996 | `			);` |
|       - | 4997 | `	}` |
|      22 | 4998 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4999 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5000 | `			"TypeError",` |
|       - | 5001 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5002 | `			ph7_type_name(apArg[0])` |
|       - | 5003 | `			);` |
|       - | 5004 | `	}` |
|      36 | 5005 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5006 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5007 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5008 | `				"TypeError",` |
|       - | 5009 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5010 | `				i + 1,` |
|       2 | 5011 | `				ph7_type_name(apArg[i])` |
|       - | 5012 | `				);` |
|       - | 5013 | `		}` |
|       9 | 5014 | `	}` |
|      17 | 5015 | `	if( nArg == 1 ){` |
|       - | 5016 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5017 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5018 | `		return PH7_OK;` |
|       - | 5019 | `	}` |
|       - | 5020 | `	/* Create a new array */` |
|      15 | 5021 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5022 | `	if( pArray == 0 ){` |
|     ! 0 | 5023 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5024 | `		return PH7_OK;` |
|       - | 5025 | `	}` |
|       - | 5026 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 5027 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5028 | `	/* Perform the intersection */` |
|      15 | 5029 | `	pEntry = pSrc->pFirst;` |
|      15 | 5030 | `	n = pSrc->nEntry;` |
|      24 | 5031 | `	for(;;){` |
|      49 | 5032 | `		if( n < 1 ){` |
|      15 | 5033 | `			break;` |
|       - | 5034 | `		}` |
|      57 | 5035 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 5036 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 5037 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 5038 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5039 | `				/* Blob lookup */` |
|      27 | 5040 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 5041 | `			}else{` |
|       - | 5042 | `				/* Int key */` |
|      13 | 5043 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5044 | `			}` |
|      39 | 5045 | `			if( rc != SXRET_OK ){` |
|       - | 5046 | `				/* Key does not exist, break immediately */` |
|      17 | 5047 | `				break;` |
|       - | 5048 | `			}` |
|      12 | 5049 | `		}` |
|      35 | 5050 | `		if( i >= nArg ){` |
|       - | 5051 | `			/* Perform the insertion */` |
|      19 | 5052 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 5053 | `		}` |
|       - | 5054 | `		/* Point to the next entry */` |
|      35 | 5055 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 5056 | `		n--;` |
|       1 | 5057 | `	}` |
|       - | 5058 | `	/* Return the freshly created array */` |
|      15 | 5059 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5060 | `	return PH7_OK;` |
|      13 | 5061 |  |
|       - | 5062 | `/*` |
|       - | 5063 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 5064 | ` *  Computes the intersection of arrays.` |
|       - | 5065 | ` * Parameters` |
|       - | 5066 | ` *  $array1` |
|       - | 5067 | ` *    The array to compare from` |
|       - | 5068 | ` *  $array2` |
|       - | 5069 | ` *    An array to compare against` |
|       - | 5070 | ` *  $...` |
|       - | 5071 | ` *   More arrays to compare against` |
|       - | 5072 | ` * $callback` |
|       - | 5073 | ` *  The callback comparison function.` |
|       - | 5074 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5075 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5076 | ` *  than the second.` |
|       - | 5077 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5078 | ` * Return` |
|       - | 5079 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5080 | ` *  in all of the parameters. .` |
|       - | 5081 | ` * Note that NULL is returned on failure.` |
|       - | 5082 | ` */` |
|      24 | 5083 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5084 |  |
|       - | 5085 | `	ph7_hashmap_node *pEntry;` |
|       - | 5086 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5087 | `	ph7_value *pCallback;` |
|       - | 5088 | `	ph7_value *pArray;` |
|       - | 5089 | `	ph7_value *pVal;` |
|       - | 5090 | `	sxi32 rc;` |
|       - | 5091 | `	sxu32 n;` |
|       - | 5092 | `	int i;` |
|       - | 5093 |  |
|       - | 5094 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      26 | 5095 | `	if( nArg < 2 ){` |
|       4 | 5096 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5097 | `			"ArgumentCountError",` |
|       - | 5098 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5099 | `			nArg` |
|       - | 5100 | `			);` |
|       - | 5101 | `	}` |
|      24 | 5102 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5103 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5104 | `			"TypeError",` |
|       - | 5105 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5106 | `			ph7_type_name(apArg[0])` |
|       - | 5107 | `			);` |
|       - | 5108 | `	}` |
|       - | 5109 |  |
|      22 | 5110 | `	if( nArg == 2 ){` |
|       - | 5111 | `		/* Only the original array and the callback were provided. */` |
|       - | 5112 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5113 | `		 * validation ordering. */` |
|       3 | 5114 | `	} else {` |
|       - | 5115 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      32 | 5116 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      18 | 5117 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5118 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5119 | `					"TypeError",` |
|       - | 5120 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5121 | `					i + 1,` |
|       2 | 5122 | `					ph7_type_name(apArg[i])` |
|       - | 5123 | `					);` |
|       - | 5124 | `			}` |
|       9 | 5125 | `		}` |
|       - | 5126 | `	}` |
|       - | 5127 |  |
|       - | 5128 | `	/* Identify the callback (always expected as the last argument). */` |
|      20 | 5129 | `	pCallback = apArg[nArg - 1];` |
|       - | 5130 | `	/* Validate the callback to match PHP's error messages. */` |
|      20 | 5131 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      11 | 5132 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5133 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5134 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5135 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5136 | `			 */` |
|       7 | 5137 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       7 | 5138 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5139 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5140 | `					"TypeError",` |
|       - | 5141 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5142 | `					nArg` |
|       - | 5143 | `					);` |
|       - | 5144 | `			}` |
|       - | 5145 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5146 | `			{` |
|       5 | 5147 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       5 | 5148 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       5 | 5149 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5150 | `					int nMethodLen;` |
|       5 | 5151 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       5 | 5152 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       5 | 5153 | `					if( pClass ){` |
|       - | 5154 | `						/* Class exists but method is missing. */` |
|       4 | 5155 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5156 | `							"TypeError",` |
|       - | 5157 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5158 | `							nArg,` |
|       1 | 5159 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5160 | `							zMethod` |
|       - | 5161 | `							);` |
|       - | 5162 | `					}` |
|       - | 5163 | `					/* Class not found */` |
|       - | 5164 | `					{` |
|       - | 5165 | `						int nName;` |
|       3 | 5166 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5167 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5168 | `							"TypeError",` |
|       - | 5169 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5170 | `							nArg,` |
|       1 | 5171 | `							zName` |
|       - | 5172 | `							);` |
|       - | 5173 | `					}` |
|       - | 5174 | `				}` |
|       - | 5175 | `			}` |
|       - | 5176 | `			/* Fallback message */` |
|     ! 0 | 5177 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5178 | `				"TypeError",` |
|       - | 5179 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5180 | `				nArg` |
|       - | 5181 | `				);` |
|       - | 5182 | `		}` |
|       5 | 5183 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5184 | `			int len;` |
|       3 | 5185 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5186 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5187 | `				"TypeError",` |
|       - | 5188 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5189 | `				nArg,` |
|       1 | 5190 | `				zName` |
|       - | 5191 | `				);` |
|       - | 5192 | `		}` |
|       4 | 5193 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5194 | `			"TypeError",` |
|       - | 5195 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5196 | `			nArg` |
|       - | 5197 | `			);` |
|       - | 5198 | `	}` |
|       - | 5199 |  |
|       9 | 5200 | `	if( nArg == 2 ){` |
|       - | 5201 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5202 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5203 | `		return PH7_OK;` |
|       - | 5204 | `	}` |
|       - | 5205 |  |
|       - | 5206 | `	/* Create a new array */` |
|       5 | 5207 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 5208 | `	if( pArray == 0 ){` |
|     ! 0 | 5209 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5210 | `		return PH7_OK;` |
|       - | 5211 | `	}` |
|       - | 5212 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 5213 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5214 | `	/* Perform the intersection */` |
|       5 | 5215 | `	pEntry = pSrc->pFirst;` |
|       5 | 5216 | `	n = pSrc->nEntry;` |
|       8 | 5217 | `	for(;;){` |
|      17 | 5218 | `		if( n < 1 ){` |
|       5 | 5219 | `			break;` |
|       - | 5220 | `		}` |
|       - | 5221 | `		/* Extract the node value */` |
|      13 | 5222 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      13 | 5223 | `		if( pVal ){` |
|      21 | 5224 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      13 | 5225 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5226 | `					/* ignore */` |
|     ! 0 | 5227 | `					continue;` |
|       - | 5228 | `				}` |
|       - | 5229 | `				/* Point to the internal representation of the hashmap */` |
|      13 | 5230 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5231 | `				/* Perform the lookup */` |
|      13 | 5232 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      13 | 5233 | `				if( rc != SXRET_OK ){` |
|       - | 5234 | `					/* Value does not exist */` |
|       5 | 5235 | `					break;` |
|       - | 5236 | `				}` |
|       5 | 5237 | `			}` |
|      13 | 5238 | `			if( i >= (nArg-1) ){` |
|       - | 5239 | `				/* Perform the insertion */` |
|       9 | 5240 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5241 | `			}` |
|       6 | 5242 | `		}` |
|       - | 5243 | `		/* Point to the next entry */` |
|      13 | 5244 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5245 | `		n--;` |
|       1 | 5246 | `	}` |
|       - | 5247 | `	/* Return the freshly created array */` |
|       5 | 5248 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5249 | `	return PH7_OK;` |
|      14 | 5250 |  |
|       - | 5251 | `/*` |
|       - | 5252 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5253 | ` *  Fill an array with values.` |
|       - | 5254 | ` * Parameters` |
|       - | 5255 | ` *  $start_index` |
|       - | 5256 | ` *    The first index of the returned array.` |
|       - | 5257 | ` *  $num` |
|       - | 5258 | ` *   Number of elements to insert.` |
|       - | 5259 | ` *  $value` |
|       - | 5260 | ` *    Value to use for filling.` |
|       - | 5261 | ` * Return` |
|       - | 5262 | ` *  The filled array or null on failure.` |
|       - | 5263 | ` */` |
|     238 | 5264 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5265 |  |
|       - | 5266 | `	ph7_value *pArray;` |
|       - | 5267 | `	int i,nEntry;` |
|       - | 5268 |  |
|       - | 5269 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 5270 | `	if( nArg != 3 ){` |
|       - | 5271 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 5272 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5273 | `			"ArgumentCountError",` |
|       - | 5274 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5275 | `			nArg` |
|       - | 5276 | `			);` |
|       - | 5277 | `	}` |
|       - | 5278 |  |
|       - | 5279 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5280 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5281 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5282 | `	 * and NULLs are rejected outright. */` |
|     466 | 5283 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 5284 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5285 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5286 | `			"TypeError",` |
|       - | 5287 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5288 | `			ph7_type_name(apArg[0])` |
|       - | 5289 | `			);` |
|       - | 5290 | `	}` |
|     234 | 5291 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5292 | `		int len;` |
|       8 | 5293 | `		sxu8 bReal = FALSE;` |
|       8 | 5294 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5295 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5296 | `			/* Non‑numeric string is an error. */` |
|       3 | 5297 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5298 | `				"TypeError",` |
|       - | 5299 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5300 | `				);` |
|       - | 5301 | `		}` |
|       5 | 5302 | `		if( bReal ){` |
|       - | 5303 | `			/* float-string -> deprecation warning */` |
|       4 | 5304 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5305 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5306 | `				zStr` |
|       - | 5307 | `				);` |
|       1 | 5308 | `		}` |
|       2 | 5309 | `	}` |
|       - | 5310 |  |
|       - | 5311 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5312 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 5313 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 5314 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5315 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5316 | `			"TypeError",` |
|       - | 5317 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5318 | `			ph7_type_name(apArg[1])` |
|       - | 5319 | `			);` |
|       - | 5320 | `	}` |
|     232 | 5321 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5322 | `		int len;` |
|       3 | 5323 | `		sxu8 bReal = FALSE;` |
|       3 | 5324 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5325 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5326 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5327 | `				"TypeError",` |
|       - | 5328 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5329 | `				);` |
|       - | 5330 | `		}` |
|     ! 0 | 5331 | `	}` |
|       - | 5332 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5333 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5334 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5335 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5336 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5337 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5338 | `		if( d != (double)i64 ){` |
|       7 | 5339 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5340 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 5341 | `				d` |
|       - | 5342 | `				);` |
|       2 | 5343 | `		}` |
|       2 | 5344 | `	}` |
|       - | 5345 |  |
|       - | 5346 | `	/* Total number of entries to insert */` |
|     230 | 5347 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 5348 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 5349 | `	if( nEntry < 0 ){` |
|       3 | 5350 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5351 | `			"ValueError",` |
|       - | 5352 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 5353 | `			);` |
|       - | 5354 | `	}` |
|       - | 5355 |  |
|       - | 5356 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 5357 | `	if( nEntry == 0 ){` |
|       7 | 5358 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 5359 | `		return PH7_OK;` |
|       - | 5360 | `	}` |
|       - | 5361 |  |
|       - | 5362 | `	/* Create a new array */` |
|     221 | 5363 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5364 | `	if( pArray == 0 ){` |
|     ! 0 | 5365 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5366 | `		return PH7_OK;` |
|       - | 5367 | `	}` |
|       - | 5368 |  |
|       - | 5369 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 5370 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 5371 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 5372 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 5373 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 5374 | `	}` |
|       - | 5375 | `	/* Return the filled array */` |
|     221 | 5376 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 5377 | `	return PH7_OK;` |
|     121 | 5378 |  |
|       - | 5379 | `/*` |
|       - | 5380 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 5381 | ` *  Fill an array with values, specifying keys.` |
|       - | 5382 | ` * Parameters` |
|       - | 5383 | ` *  $input` |
|       - | 5384 | ` *   Array of values that will be used as key.` |
|       - | 5385 | ` *  $value` |
|       - | 5386 | ` *    Value to use for filling.` |
|       - | 5387 | ` * Return` |
|       - | 5388 | ` *  The filled array.` |
|       - | 5389 | ` * Throws` |
|       - | 5390 | ` *  ValueError if $input is not an array.` |
|       - | 5391 | ` */` |
|      26 | 5392 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5393 |  |
|       - | 5394 | `	ph7_hashmap_node *pEntry;` |
|       - | 5395 | `	ph7_hashmap *pSrc;` |
|       - | 5396 | `	ph7_value *pArray;` |
|       - | 5397 | `	sxu32 n;` |
|       - | 5398 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 5399 | `	if( nArg != 2 ){` |
|      10 | 5400 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5401 | `			"ArgumentCountError",` |
|       - | 5402 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 5403 | `			nArg` |
|       - | 5404 | `			);` |
|       - | 5405 | `	}` |
|       - | 5406 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5407 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 5408 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5409 | `			"TypeError",` |
|       - | 5410 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 5411 | `			ph7_type_name(apArg[0])` |
|       - | 5412 | `			);` |
|       - | 5413 | `	}` |
|       - | 5414 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5415 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5416 | `	/* Create a new array */` |
|      17 | 5417 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5418 | `	if( pArray == 0 ){` |
|     ! 0 | 5419 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5420 | `		return PH7_OK;` |
|       - | 5421 | `	}` |
|       - | 5422 | `	/* Perform the requested operation */` |
|      17 | 5423 | `	pEntry = pSrc->pFirst;` |
|      45 | 5424 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 5425 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 5426 | `		/* Point to the next entry */` |
|      29 | 5427 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5428 | `	}` |
|       - | 5429 | `	/* Return the filled array */` |
|      17 | 5430 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5431 | `	return PH7_OK;` |
|      15 | 5432 |  |
|       - | 5433 | `/*` |
|       - | 5434 | ` * array array_combine(array $keys,array $values)` |
|       - | 5435 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 5436 | ` * Parameters` |
|       - | 5437 | ` *  $keys` |
|       - | 5438 | ` *    Array of keys to be used.` |
|       - | 5439 | ` * $values` |
|       - | 5440 | ` *   Array of values to be used.` |
|       - | 5441 | ` * Return` |
|       - | 5442 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 5443 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 5444 | ` *  not an array.` |
|       - | 5445 | ` */` |
|      18 | 5446 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5447 |  |
|       - | 5448 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 5449 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 5450 | `	ph7_value *pArray;` |
|       - | 5451 | `	sxu32 n;` |
|       - | 5452 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 5453 | `	if( nArg != 2 ){` |
|       - | 5454 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 5455 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5456 | `			"ArgumentCountError",` |
|       - | 5457 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 5458 | `			nArg` |
|       - | 5459 | `			);` |
|       - | 5460 | `	}` |
|       - | 5461 | `	/* Validate argument types individually so we can report the correct` |
|       - | 5462 | `	 * argument index in the error message. */` |
|      18 | 5463 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5464 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5465 | `			"TypeError",` |
|       - | 5466 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5467 | `			ph7_type_name(apArg[0])` |
|       - | 5468 | `			);` |
|       - | 5469 | `	}` |
|      16 | 5470 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5471 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5472 | `			"TypeError",` |
|       - | 5473 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5474 | `			ph7_type_name(apArg[1])` |
|       - | 5475 | `			);` |
|       - | 5476 | `	}` |
|       - | 5477 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5478 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5479 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5480 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5481 | `		/* Length mismatch -> ValueError */` |
|       3 | 5482 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5483 | `			"ValueError",` |
|       - | 5484 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5485 | `			);` |
|       - | 5486 | `	}` |
|       - | 5487 | `	/* Create a new array */` |
|      11 | 5488 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5489 | `	if( pArray == 0 ){` |
|     ! 0 | 5490 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5491 | `		return PH7_OK;` |
|       - | 5492 | `	}` |
|       - | 5493 | `	/* Perform the requested operation */` |
|      11 | 5494 | `	pKe = pKey->pFirst;` |
|      11 | 5495 | `	pVe = pValue->pFirst;` |
|      33 | 5496 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5497 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5498 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5499 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5500 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5501 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5502 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5503 | `		 * original array must not be mutated. */` |
|      23 | 5504 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5505 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5506 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5507 | `			if( pTmpKey ){` |
|       5 | 5508 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5509 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5510 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5511 | `				pKeyCopy = pTmpKey;` |
|       2 | 5512 | `			}` |
|       2 | 5513 | `		}` |
|      23 | 5514 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5515 | `		/* Point to the next entry */` |
|      23 | 5516 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5517 | `		pVe = pVe->pPrev;` |
|      12 | 5518 | `	}` |
|       - | 5519 | `	/* Return the filled array */` |
|      11 | 5520 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5521 | `	return PH7_OK;` |
|      11 | 5522 |  |
|       - | 5523 | `/*` |
|       - | 5524 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5525 | ` *  Return an array with elements in reverse order.` |
|       - | 5526 | ` * Parameters` |
|       - | 5527 | ` *  $array` |
|       - | 5528 | ` *   The input array.` |
|       - | 5529 | ` *  $preserve_keys (optional)` |
|       - | 5530 | ` *   If set to TRUE keys are preserved.` |
|       - | 5531 | ` * Return` |
|       - | 5532 | ` *  The reversed array.` |
|       - | 5533 | ` */` |
|      20 | 5534 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5535 |  |
|       - | 5536 | `	ph7_hashmap_node *pEntry;` |
|       - | 5537 | `	ph7_hashmap *pSrc;` |
|       - | 5538 | `	ph7_value *pArray;` |
|       - | 5539 | `	int bPreserve;` |
|       - | 5540 | `	sxu32 n;` |
|      22 | 5541 | `	if( nArg < 1 ){` |
|       4 | 5542 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5543 | `			"ArgumentCountError",` |
|       - | 5544 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5545 | `			nArg` |
|       - | 5546 | `			);` |
|       - | 5547 | `	}` |
|       - | 5548 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5549 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5550 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5551 | `			"TypeError",` |
|       - | 5552 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5553 | `			ph7_type_name(apArg[0])` |
|       - | 5554 | `			);` |
|       - | 5555 | `	}` |
|      17 | 5556 | `	bPreserve = FALSE;` |
|      17 | 5557 | `	if( nArg > 1 ){` |
|       7 | 5558 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5559 | `	}` |
|       - | 5560 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5561 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5562 | `	/* Create a new array */` |
|      17 | 5563 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5564 | `	if( pArray == 0 ){` |
|     ! 0 | 5565 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5566 | `		return PH7_OK;` |
|       - | 5567 | `	}` |
|       - | 5568 | `	/* Perform the requested operation */` |
|      17 | 5569 | `	pEntry = pSrc->pLast;` |
|      55 | 5570 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5571 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5572 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5573 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5574 | `		/* Point to the previous entry */` |
|      39 | 5575 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5576 | `	}` |
|      17 | 5577 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5578 | `	return PH7_OK;` |
|      12 | 5579 |  |
|       - | 5580 | `/*` |
|       - | 5581 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5582 | ` *  Removes duplicate values from an array.` |
|       - | 5583 | ` * Parameters` |
|       - | 5584 | ` *  $array` |
|       - | 5585 | ` *   The input array.` |
|       - | 5586 | ` *  $flags` |
|       - | 5587 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5588 | ` *   behavior using these values:` |
|       - | 5589 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5590 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5591 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5592 | ` * Return` |
|       - | 5593 | ` *  The filtered array.` |
|       - | 5594 | ` */` |
|      24 | 5595 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5596 |  |
|       - | 5597 | `	ph7_hashmap_node *pEntry;` |
|       - | 5598 | `	ph7_value *pNeedle;` |
|       - | 5599 | `	ph7_hashmap *pSrc;` |
|       - | 5600 | `	ph7_value *pArray;` |
|       - | 5601 | `	int bStrict;` |
|       - | 5602 | `	sxi32 rc;` |
|       - | 5603 | `	sxu32 n;` |
|      26 | 5604 | `	if( nArg < 1 ){` |
|       - | 5605 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5606 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5607 | `			"ArgumentCountError",` |
|       - | 5608 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5609 | `			);` |
|       - | 5610 | `	}` |
|      24 | 5611 | `	if( nArg > 2 ){` |
|       - | 5612 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5613 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5614 | `			"ArgumentCountError",` |
|       - | 5615 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5616 | `			nArg` |
|       - | 5617 | `			);` |
|       - | 5618 | `	}` |
|       - | 5619 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5620 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5621 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5622 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5623 | `			"TypeError",` |
|       - | 5624 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5625 | `			ph7_type_name(apArg[0])` |
|       - | 5626 | `			);` |
|       - | 5627 | `	}` |
|      19 | 5628 | `	bStrict = FALSE;` |
|       - | 5629 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5630 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5631 | `	/* Create a new array */` |
|      19 | 5632 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5633 | `	if( pArray == 0 ){` |
|     ! 0 | 5634 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5635 | `		return PH7_OK;` |
|       - | 5636 | `	}` |
|       - | 5637 | `	/* Perform the requested operation */` |
|      19 | 5638 | `	pEntry = pSrc->pFirst;` |
|      83 | 5639 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5640 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5641 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5642 | `		if( pNeedle ){` |
|      65 | 5643 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5644 | `		}` |
|      65 | 5645 | `		if( rc != SXRET_OK ){` |
|       - | 5646 | `			/* Perform the insertion */` |
|      37 | 5647 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5648 | `		}` |
|       - | 5649 | `		/* Point to the next entry */` |
|      65 | 5650 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5651 | `	}` |
|       - | 5652 | `	/* Return the freshly created array */` |
|      19 | 5653 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5654 | `	return PH7_OK;` |
|      14 | 5655 |  |
|       - | 5656 | `/*` |
|       - | 5657 | ` * array array_flip(array $input)` |
|       - | 5658 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5659 | ` * Parameter` |
|       - | 5660 | ` *  $input` |
|       - | 5661 | ` *   Input array.` |
|       - | 5662 | ` * Return` |
|       - | 5663 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5664 | ` */` |
|      34 | 5665 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5666 |  |
|       - | 5667 | `	ph7_hashmap_node *pEntry;` |
|       - | 5668 | `	ph7_hashmap *pSrc;` |
|       - | 5669 | `	ph7_value *pArray;` |
|       - | 5670 | `	ph7_value *pKey;` |
|       - | 5671 | `	ph7_value sVal;` |
|       - | 5672 | `	sxu32 n;` |
|       - | 5673 |  |
|       - | 5674 | `	/* PHP requires exactly one argument */` |
|      36 | 5675 | `	if( nArg != 1 ){` |
|       - | 5676 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5677 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5678 | `			"ArgumentCountError",` |
|       - | 5679 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5680 | `			nArg` |
|       - | 5681 | `			);` |
|       - | 5682 | `	}` |
|       - | 5683 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5684 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5685 | `		/* Type mismatch -> TypeError */` |
|       7 | 5686 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5687 | `			"TypeError",` |
|       - | 5688 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5689 | `			ph7_type_name(apArg[0])` |
|       - | 5690 | `			);` |
|       - | 5691 | `	}` |
|       - | 5692 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5693 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5694 | `	/* Create a new array */` |
|      27 | 5695 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5696 | `	if( pArray == 0 ){` |
|     ! 0 | 5697 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5698 | `		return PH7_OK;` |
|       - | 5699 | `	}` |
|       - | 5700 | `	/* Start processing */` |
|      27 | 5701 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5702 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5703 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5704 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5705 | `		if( pKey ){` |
|       - | 5706 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5707 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5708 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5709 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5710 | `					);` |
|   22236 | 5711 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5712 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5713 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5714 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5715 | `				}else{` |
|       - | 5716 | `					SyString sStr;` |
|    2227 | 5717 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5718 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5719 | `				}` |
|       - | 5720 | `				/* Perform the insertion */` |
|   22227 | 5721 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5722 | `				/* Safely release the value because each inserted entry` |
|       - | 5723 | `				 * has its own private copy of the value.` |
|       - | 5724 | `				 */` |
|   22227 | 5725 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5726 | `			}else{` |
|       - | 5727 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5728 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5729 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5730 | `					);` |
|       - | 5731 | `			}` |
|   11118 | 5732 | `		}` |
|       - | 5733 | `		/* Point to the next entry */` |
|   22237 | 5734 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5735 | `	}` |
|       - | 5736 | `	/* Return the freshly created array */` |
|      27 | 5737 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5738 | `	return PH7_OK;` |
|      19 | 5739 |  |
|       - | 5740 | `/*` |
|       - | 5741 | ` * number array_sum(array $array )` |
|       - | 5742 | ` *  Calculate the sum of values in an array.` |
|       - | 5743 | ` * Parameters` |
|       - | 5744 | ` *  $array: The input array.` |
|       - | 5745 | ` * Return` |
|       - | 5746 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5747 | ` */` |
|      24 | 5748 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5749 |  |
|       - | 5750 | `	ph7_hashmap_node *pEntry;` |
|       - | 5751 | `	ph7_value *pObj;` |
|      25 | 5752 | `	double dSum = 0;` |
|       - | 5753 | `	sxu32 n;` |
|      25 | 5754 | `	pEntry = pMap->pFirst;` |
|      91 | 5755 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5756 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5757 | `		if( pObj ){` |
|      67 | 5758 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5759 | `				dSum += pObj->rVal;` |
|      53 | 5760 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5761 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5762 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5763 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5764 | `					double dv = 0;` |
|      13 | 5765 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5766 | `					dSum += dv;` |
|       7 | 5767 | `				}` |
|      12 | 5768 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5769 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5770 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5771 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5772 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5773 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5774 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5775 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5776 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5777 | `			}` |
|       - | 5778 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5779 | `		}` |
|       - | 5780 | `		/* Point to the next entry */` |
|      67 | 5781 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5782 | `	}` |
|       - | 5783 | `	/* Return sum */` |
|      25 | 5784 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5785 |  |
|      26 | 5786 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5787 |  |
|       - | 5788 | `	ph7_hashmap_node *pEntry;` |
|       - | 5789 | `	ph7_value *pObj;` |
|      28 | 5790 | `	sxi64 nSum = 0;` |
|       - | 5791 | `	sxu32 n;` |
|      28 | 5792 | `	pEntry = pMap->pFirst;` |
|     112 | 5793 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      86 | 5794 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      86 | 5795 | `		if( pObj ){` |
|      86 | 5796 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      76 | 5797 | `				nSum += pObj->x.iVal;` |
|      48 | 5798 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5799 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5800 | `					sxi64 nv = 0;` |
|       5 | 5801 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5802 | `					nSum += nv;` |
|       3 | 5803 | `				}` |
|       8 | 5804 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
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
|      42 | 5815 | `		}` |
|       - | 5816 | `		/* Point to the next entry */` |
|      86 | 5817 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      44 | 5818 | `	}` |
|       - | 5819 | `	/* Return sum */` |
|      28 | 5820 | `	ph7_result_int64(pCtx,nSum);` |
|      28 | 5821 |  |
|       - | 5822 | `/* number array_sum(array $array )` |
|       - | 5823 | ` * (See block-coment above)` |
|       - | 5824 | ` */` |
|      64 | 5825 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5826 |  |
|       - | 5827 | `	ph7_hashmap_node *pEntry;` |
|       - | 5828 | `	ph7_hashmap *pMap;` |
|       - | 5829 | `	ph7_value *pObj;` |
|      66 | 5830 | `	int useDouble = 0;` |
|       - | 5831 | `	sxu32 n;` |
|       - | 5832 | `	/* PHP requires exactly one argument */` |
|      66 | 5833 | `	if( nArg != 1 ){` |
|       7 | 5834 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5835 | `			"ArgumentCountError",` |
|       - | 5836 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5837 | `			nArg` |
|       - | 5838 | `			);` |
|       - | 5839 | `	}` |
|       - | 5840 | `	/* Make sure we are dealing with a valid hashmap */` |
|      62 | 5841 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5842 | `		/* Type mismatch -> TypeError */` |
|       7 | 5843 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5844 | `			"TypeError",` |
|       - | 5845 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5846 | `			ph7_type_name(apArg[0])` |
|       - | 5847 | `			);` |
|       - | 5848 | `	}` |
|      58 | 5849 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      58 | 5850 | `	if( pMap->nEntry < 1 ){` |
|       - | 5851 | `		/* Nothing to compute,return 0 */` |
|       7 | 5852 | `		ph7_result_int(pCtx,0);` |
|       7 | 5853 | `		return PH7_OK;` |
|       - | 5854 | `	}` |
|       - | 5855 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5856 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5857 | `	 */` |
|      52 | 5858 | `	pEntry = pMap->pFirst;` |
|     144 | 5859 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     118 | 5860 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     118 | 5861 | `		if( pObj ){` |
|     118 | 5862 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5863 | `				useDouble = 1;` |
|      19 | 5864 | `				break;` |
|       - | 5865 | `			}` |
|     100 | 5866 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5867 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5868 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5869 | `				sxu32 i;` |
|      23 | 5870 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5871 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5872 | `						useDouble = 1;` |
|       7 | 5873 | `						break;` |
|       - | 5874 | `					}` |
|       6 | 5875 | `				}` |
|      13 | 5876 | `				if( useDouble ){` |
|       7 | 5877 | `					break;` |
|       - | 5878 | `				}` |
|       3 | 5879 | `			}` |
|      46 | 5880 | `		}` |
|      94 | 5881 | `		pEntry = pEntry->pPrev;` |
|      48 | 5882 | `	}` |
|      52 | 5883 | `	if( useDouble ){` |
|      25 | 5884 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5885 | `	}else{` |
|      28 | 5886 | `		Int64Sum(pCtx,pMap);` |
|       - | 5887 | `	}` |
|      52 | 5888 | `	return PH7_OK;` |
|      34 | 5889 |  |
|       - | 5890 | `/*` |
|       - | 5891 | ` * number array_product(array $array )` |
|       - | 5892 | ` *  Calculate the product of values in an array.` |
|       - | 5893 | ` * Parameters` |
|       - | 5894 | ` *  $array: The input array.` |
|       - | 5895 | ` * Return` |
|       - | 5896 | ` *  Returns the product of values as an integer or float.` |
|       - | 5897 | ` */` |
|     ! 0 | 5898 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5899 |  |
|       - | 5900 | `	ph7_hashmap_node *pEntry;` |
|       - | 5901 | `	ph7_value *pObj;` |
|       - | 5902 | `	double dProd;` |
|       - | 5903 | `	sxu32 n;` |
|     ! 0 | 5904 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5905 | `	dProd = 1;` |
|     ! 0 | 5906 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5907 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5908 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5909 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5910 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5911 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5912 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5913 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5914 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5915 | `					double dv = 0;` |
|     ! 0 | 5916 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5917 | `					dProd *= dv;` |
|     ! 0 | 5918 | `				}` |
|     ! 0 | 5919 | `			}` |
|     ! 0 | 5920 | `		}` |
|       - | 5921 | `		/* Point to the next entry */` |
|     ! 0 | 5922 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5923 | `	}` |
|       - | 5924 | `	/* Return product */` |
|     ! 0 | 5925 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5926 |  |
|     ! 0 | 5927 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5928 |  |
|       - | 5929 | `	ph7_hashmap_node *pEntry;` |
|       - | 5930 | `	ph7_value *pObj;` |
|       - | 5931 | `	sxi64 nProd;` |
|       - | 5932 | `	sxu32 n;` |
|     ! 0 | 5933 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5934 | `	nProd = 1;` |
|     ! 0 | 5935 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5936 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5937 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5938 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5939 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5940 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5941 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5942 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5943 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5944 | `					sxi64 nv = 0;` |
|     ! 0 | 5945 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5946 | `					nProd *= nv;` |
|     ! 0 | 5947 | `				}` |
|     ! 0 | 5948 | `			}` |
|     ! 0 | 5949 | `		}` |
|       - | 5950 | `		/* Point to the next entry */` |
|     ! 0 | 5951 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5952 | `	}` |
|       - | 5953 | `	/* Return product */` |
|     ! 0 | 5954 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5955 |  |
|       - | 5956 | `/* number array_product(array $array )` |
|       - | 5957 | ` * (See block-block comment above)` |
|       - | 5958 | ` */` |
|     ! 0 | 5959 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5960 |  |
|       - | 5961 | `	ph7_hashmap *pMap;` |
|       - | 5962 | `	ph7_value *pObj;` |
|     ! 0 | 5963 | `	if( nArg < 1 ){` |
|       - | 5964 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5965 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5966 | `		return PH7_OK;` |
|       - | 5967 | `	}` |
|       - | 5968 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5969 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5970 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5971 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5972 | `		return PH7_OK;` |
|       - | 5973 | `	}` |
|     ! 0 | 5974 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5975 | `	if( pMap->nEntry < 1 ){` |
|       - | 5976 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5977 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5978 | `		return PH7_OK;` |
|       - | 5979 | `	}` |
|       - | 5980 | `	/* If the first element is of type float,then perform floating` |
|       - | 5981 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5982 | `	 */` |
|     ! 0 | 5983 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5984 | `	if( pObj == 0 ){` |
|     ! 0 | 5985 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5986 | `		return PH7_OK;` |
|       - | 5987 | `	}` |
|     ! 0 | 5988 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5989 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5990 | `	}else{` |
|     ! 0 | 5991 | `		Int64Prod(pCtx,pMap);` |
|       - | 5992 | `	}` |
|     ! 0 | 5993 | `	return PH7_OK;` |
|     ! 0 | 5994 |  |
|       - | 5995 | `/*` |
|       - | 5996 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5997 | ` *  Pick one or more random entries out of an array.` |
|       - | 5998 | ` * Parameters` |
|       - | 5999 | ` * $input` |
|       - | 6000 | ` *  The input array.` |
|       - | 6001 | ` * $num_req` |
|       - | 6002 | ` *  Specifies how many entries you want to pick.` |
|       - | 6003 | ` * Return` |
|       - | 6004 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 6005 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 6006 | ` *  NULL is returned on failure.` |
|       - | 6007 | ` */` |
|       6 | 6008 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6009 |  |
|       - | 6010 | `	ph7_hashmap_node *pNode;` |
|       - | 6011 | `	ph7_hashmap *pMap;` |
|       7 | 6012 | `	int nItem = 1;` |
|       7 | 6013 | `	if( nArg < 1 ){` |
|       - | 6014 | `		/* Missing argument,return NULL */` |
|     ! 0 | 6015 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6016 | `		return PH7_OK;` |
|       - | 6017 | `	}` |
|       - | 6018 | `	/* Make sure we are dealing with an array */` |
|       7 | 6019 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6020 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6021 | `		return PH7_OK;` |
|       - | 6022 | `	}` |
|       - | 6023 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 6024 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 6025 | `	if(pMap->nEntry < 1 ){` |
|       - | 6026 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 6027 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6028 | `		return PH7_OK;` |
|       - | 6029 | `	}` |
|       7 | 6030 | `	if( nArg > 1 ){` |
|       3 | 6031 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 6032 | `	}` |
|       7 | 6033 | `	if( nItem < 2 ){` |
|       - | 6034 | `		sxu32 nEntry;` |
|       - | 6035 | `		/* Select a random number */` |
|       5 | 6036 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 6037 | `		/* Extract the desired entry.` |
|       - | 6038 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 6039 | `		 */` |
|       5 | 6040 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       4 | 6041 | `			pNode = pMap->pLast;` |
|       4 | 6042 | `			nEntry = pMap->nEntry - nEntry;` |
|       4 | 6043 | `			if( nEntry > 1 ){` |
|     ! 0 | 6044 | `				for(;;){` |
|     ! 0 | 6045 | `					if( nEntry == 0 ){` |
|     ! 0 | 6046 | `						break;` |
|       - | 6047 | `					}` |
|       - | 6048 | `					/* Point to the previous entry */` |
|     ! 0 | 6049 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 6050 | `					nEntry--;` |
|     ! 0 | 6051 | `				}` |
|     ! 0 | 6052 | `			}` |
|       3 | 6053 | `		}else{` |
|       2 | 6054 | `			pNode = pMap->pFirst;` |
|     ! 0 | 6055 | `			for(;;){` |
|       2 | 6056 | `				if( nEntry == 0 ){` |
|       2 | 6057 | `					break;` |
|       - | 6058 | `				}` |
|       - | 6059 | `				/* Point to the next entry */` |
|       1 | 6060 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 6061 | `				nEntry--;` |
|       1 | 6062 | `			}` |
|       - | 6063 | `		}` |
|       5 | 6064 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6065 | `			/* Int key */` |
|       3 | 6066 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6067 | `		}else{` |
|       - | 6068 | `			/* Blob key */` |
|       3 | 6069 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6070 | `		}` |
|       3 | 6071 | `	}else{` |
|       - | 6072 | `		ph7_value sKey,*pArray;` |
|       - | 6073 | `		ph7_hashmap *pDest;` |
|       - | 6074 | `		/* Create a new array */` |
|       3 | 6075 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6076 | `		if( pArray == 0 ){` |
|     ! 0 | 6077 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6078 | `			return PH7_OK;` |
|       - | 6079 | `		}` |
|       - | 6080 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6081 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6082 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6083 | `		/* Copy the first n items */` |
|       3 | 6084 | `		pNode = pMap->pFirst;` |
|       3 | 6085 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6086 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6087 | `		}` |
|       7 | 6088 | `		while( nItem > 0){` |
|       5 | 6089 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6090 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6091 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6092 | `			/* Point to the next entry */` |
|       5 | 6093 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6094 | `			nItem--;` |
|       1 | 6095 | `		}` |
|       - | 6096 | `		/* Shuffle the array */` |
|       3 | 6097 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6098 | `		/* Rehash node */` |
|       3 | 6099 | `		HashmapSortRehash(pDest);` |
|       - | 6100 | `		/* Return the random array */` |
|       3 | 6101 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6102 | `	}` |
|       7 | 6103 | `	return PH7_OK;` |
|       4 | 6104 |  |
|       - | 6105 | `/*` |
|       - | 6106 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6107 | ` *  Split an array into chunks.` |
|       - | 6108 | ` * Parameters` |
|       - | 6109 | ` * $input` |
|       - | 6110 | ` *   The array to work on` |
|       - | 6111 | ` * $size` |
|       - | 6112 | ` *   The size of each chunk` |
|       - | 6113 | ` * $preserve_keys` |
|       - | 6114 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6115 | ` *   the chunk numerically.` |
|       - | 6116 | ` * Return` |
|       - | 6117 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6118 | ` *  zero, with each dimension containing size elements.` |
|       - | 6119 | ` */` |
|      42 | 6120 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6121 |  |
|       - | 6122 | `	ph7_value *pArray,*pChunk;` |
|       - | 6123 | `	ph7_hashmap_node *pEntry;` |
|       - | 6124 | `	ph7_hashmap *pMap;` |
|       - | 6125 | `	int bPreserve;` |
|       - | 6126 | `	sxu32 nChunk;` |
|       - | 6127 | `	sxu32 nSize;` |
|       - | 6128 | `	sxu32 n;` |
|       - | 6129 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 6130 | `	if( nArg < 2 ){` |
|       - | 6131 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6132 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6133 | `			"ArgumentCountError",` |
|       - | 6134 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6135 | `			nArg` |
|       - | 6136 | `			);` |
|       - | 6137 | `	}` |
|      42 | 6138 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6139 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6140 | `			"TypeError",` |
|       - | 6141 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6142 | `			ph7_type_name(apArg[0])` |
|       - | 6143 | `			);` |
|       - | 6144 | `	}` |
|       - | 6145 | `	/* Create a new array */` |
|      40 | 6146 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 6147 | `	if( pArray == 0 ){` |
|     ! 0 | 6148 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6149 | `		return PH7_OK;` |
|       - | 6150 | `	}` |
|       - | 6151 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 6152 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6153 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6154 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 6155 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 6156 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6157 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6158 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6159 | `			"TypeError",` |
|       - | 6160 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6161 | `			ph7_type_name(apArg[1])` |
|       - | 6162 | `			);` |
|       - | 6163 | `	}` |
|       - | 6164 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6165 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6166 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 6167 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6168 | `		int len;` |
|       3 | 6169 | `		sxu8 bReal = FALSE;` |
|       3 | 6170 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6171 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6172 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6173 | `				"TypeError",` |
|       - | 6174 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6175 | `				);` |
|       - | 6176 | `		}` |
|     ! 0 | 6177 | `		if( bReal ){` |
|       - | 6178 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6179 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6180 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6181 | `				zStr` |
|       - | 6182 | `				);` |
|     ! 0 | 6183 | `		}` |
|     ! 0 | 6184 | `	}` |
|       - | 6185 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6186 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6187 | `	 * later via ph7_value_to_int. */` |
|      38 | 6188 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6189 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6190 | `		sxi64 i = (sxi64)d;` |
|       3 | 6191 | `		if( d != (double)i ){` |
|       4 | 6192 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6193 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6194 | `				d` |
|       - | 6195 | `				);` |
|       1 | 6196 | `		}` |
|       1 | 6197 | `	}` |
|       - | 6198 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6199 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6200 | `	{` |
|      38 | 6201 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 6202 | `		if( nSizeSigned < 1 ){` |
|       - | 6203 | `			/* size <= 0 -> ValueError */` |
|       5 | 6204 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6205 | `				"ValueError",` |
|       - | 6206 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6207 | `				);` |
|       - | 6208 | `		}` |
|      34 | 6209 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6210 | `	}` |
|      34 | 6211 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6212 | `		/* Return the whole array */` |
|       3 | 6213 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6214 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6215 | `		return PH7_OK;` |
|       - | 6216 | `	}` |
|      32 | 6217 | `	bPreserve = 0;` |
|      32 | 6218 | `	if( nArg > 2 ){` |
|       - | 6219 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6220 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6221 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6222 | `		 * normally, matching PHP behaviour. */` |
|      45 | 6223 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 6224 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6225 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 6226 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6227 | `				"TypeError",` |
|       - | 6228 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6229 | `				ph7_type_name(apArg[2])` |
|       - | 6230 | `				);` |
|       - | 6231 | `		}` |
|      21 | 6232 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6233 | `	}` |
|       - | 6234 | `	/* Start processing */` |
|      27 | 6235 | `	pEntry = pMap->pFirst;` |
|      27 | 6236 | `	nChunk = 0;` |
|      27 | 6237 | `	pChunk = 0;` |
|      27 | 6238 | `	n = pMap->nEntry;` |
|      56 | 6239 | `	for( ;; ){` |
|     113 | 6240 | `		if( n < 1 ){` |
|       - | 6241 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6242 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6243 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6244 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6245 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6246 | `			 * exists. */` |
|      27 | 6247 | `			if( pChunk ){` |
|      27 | 6248 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6249 | `			}` |
|      27 | 6250 | `			break;` |
|       - | 6251 | `		}` |
|      87 | 6252 | `		if( nChunk < 1 ){` |
|      71 | 6253 | `			if( pChunk ){` |
|       - | 6254 | `				/* Put the first chunk */` |
|      45 | 6255 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6256 | `			}` |
|       - | 6257 | `			/* Create a new dimension */` |
|      71 | 6258 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6259 | `												   * will be automatically released as soon we return` |
|       - | 6260 | `												   * from this function */` |
|      71 | 6261 | `			if( pChunk == 0 ){` |
|     ! 0 | 6262 | `				break;` |
|       - | 6263 | `			}` |
|      71 | 6264 | `			nChunk = nSize;` |
|      35 | 6265 | `		}` |
|       - | 6266 | `		/* Insert the entry */` |
|      87 | 6267 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6268 | `		/* Point to the next entry */` |
|      87 | 6269 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6270 | `		nChunk--;` |
|      87 | 6271 | `		n--;` |
|       1 | 6272 | `	}` |
|       - | 6273 | `	/* Return the multidimensional array */` |
|      27 | 6274 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6275 | `	return PH7_OK;` |
|      23 | 6276 |  |
|       - | 6277 | `/*` |
|       - | 6278 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6279 | ` *  Pad array to the specified length with a value.` |
|       - | 6280 | ` * $input` |
|       - | 6281 | ` *   Initial array of values to pad.` |
|       - | 6282 | ` * $pad_size` |
|       - | 6283 | ` *   New size of the array.` |
|       - | 6284 | ` * $pad_value` |
|       - | 6285 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6286 | ` */` |
|      28 | 6287 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6288 |  |
|       - | 6289 | `	ph7_hashmap *pMap;` |
|       - | 6290 | `	ph7_value *pArray;` |
|       - | 6291 | `	int nEntry;` |
|      30 | 6292 | `	if( nArg != 3 ){` |
|      10 | 6293 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6294 | `			"ArgumentCountError",` |
|       - | 6295 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6296 | `			nArg` |
|       - | 6297 | `			);` |
|       - | 6298 | `	}` |
|      24 | 6299 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6300 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6301 | `			"TypeError",` |
|       - | 6302 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6303 | `			ph7_type_name(apArg[0])` |
|       - | 6304 | `			);` |
|       - | 6305 | `	}` |
|       - | 6306 | `	/* Create a new array */` |
|      21 | 6307 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6308 | `	if( pArray == 0 ){` |
|     ! 0 | 6309 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6310 | `		return PH7_OK;` |
|       - | 6311 | `	}` |
|       - | 6312 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6313 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6314 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6315 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6316 | `	if( nEntry < 0 ){` |
|       9 | 6317 | `		nEntry = -nEntry;` |
|       9 | 6318 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6319 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6320 | `			/* Insert given items first */` |
|      17 | 6321 | `			while( nEntry > 0 ){` |
|      13 | 6322 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 6323 | `				nEntry--;` |
|       1 | 6324 | `			}` |
|       - | 6325 | `			/* Merge the two arrays */` |
|       5 | 6326 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6327 | `		}else{` |
|       5 | 6328 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6329 | `		}` |
|      17 | 6330 | `	}else if( nEntry > 0 ){` |
|      11 | 6331 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6332 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6333 | `			/* Merge the two arrays first */` |
|       7 | 6334 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6335 | `			/* Insert given items */` |
|      25 | 6336 | `			while( nEntry > 0 ){` |
|      19 | 6337 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 6338 | `				nEntry--;` |
|       1 | 6339 | `			}` |
|       4 | 6340 | `		}else{` |
|       5 | 6341 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6342 | `		}` |
|       6 | 6343 | `	}else{` |
|       - | 6344 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 6345 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6346 | `	}` |
|       - | 6347 | `	/* Return the new array */` |
|      21 | 6348 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 6349 | `	return PH7_OK;` |
|      16 | 6350 |  |
|       - | 6351 | `/*` |
|       - | 6352 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 6353 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 6354 | ` * Parameters` |
|       - | 6355 | ` * $array` |
|       - | 6356 | ` *   The array in which elements are replaced.` |
|       - | 6357 | ` * $array1` |
|       - | 6358 | ` *   The array from which elements will be extracted.` |
|       - | 6359 | ` * ....` |
|       - | 6360 | ` *  More arrays from which elements will be extracted.` |
|       - | 6361 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 6362 | ` * Return` |
|       - | 6363 | ` *  Returns an array.` |
|       - | 6364 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 6365 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 6366 | ` */` |
|      22 | 6367 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6368 |  |
|       - | 6369 | `	ph7_hashmap *pMap;` |
|       - | 6370 | `	ph7_value *pArray;` |
|       - | 6371 | `	int i;` |
|      24 | 6372 | `	if( nArg < 1 ){` |
|       3 | 6373 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6374 | `			"ArgumentCountError",` |
|       - | 6375 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 6376 | `			);` |
|       - | 6377 | `	}` |
|      22 | 6378 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6379 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6380 | `			"TypeError",` |
|       - | 6381 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6382 | `			ph7_type_name(apArg[0])` |
|       - | 6383 | `			);` |
|       - | 6384 | `	}` |
|       - | 6385 | `	/* Create a new array */` |
|      20 | 6386 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 6387 | `	if( pArray == 0 ){` |
|     ! 0 | 6388 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6389 | `		return PH7_OK;` |
|       - | 6390 | `	}` |
|       - | 6391 | `	/* Overwrite from the first array */` |
|      20 | 6392 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 6393 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6394 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 6395 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 6396 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 6397 | `			/* Type mismatch -> TypeError */` |
|       4 | 6398 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6399 | `				"TypeError",` |
|       - | 6400 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 6401 | `				i + 1,` |
|       2 | 6402 | `				ph7_type_name(apArg[i])` |
|       - | 6403 | `				);` |
|       - | 6404 | `		}` |
|       - | 6405 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 6406 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 6407 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 6408 | `	}` |
|       - | 6409 | `	/* Return the new array */` |
|      17 | 6410 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6411 | `	return PH7_OK;` |
|      13 | 6412 |  |
|       - | 6413 | `/*` |
|       - | 6414 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 6415 | ` *  Filters elements of an array using a callback function.` |
|       - | 6416 | ` * Parameters` |
|       - | 6417 | ` *  $input` |
|       - | 6418 | ` *    The array to iterate over` |
|       - | 6419 | ` * $callback` |
|       - | 6420 | ` *    The callback function to use` |
|       - | 6421 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 6422 | ` *    will be removed.` |
|       - | 6423 | ` * Return` |
|       - | 6424 | ` *  The filtered array.` |
|       - | 6425 | ` */` |
|      20 | 6426 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6427 |  |
|       - | 6428 | `	ph7_hashmap_node *pEntry;` |
|       - | 6429 | `	ph7_hashmap *pMap;` |
|       - | 6430 | `	ph7_value *pArray;` |
|       - | 6431 | `	ph7_value sResult;   /* Callback result */` |
|       - | 6432 | `	ph7_value *pValue;` |
|       - | 6433 | `	sxi32 rc;` |
|       - | 6434 | `	int keep;` |
|       - | 6435 | `	sxu32 n;` |
|      22 | 6436 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6437 | `		/* Invalid arguments,return NULL */` |
|       5 | 6438 | `		ph7_result_null(pCtx);` |
|       5 | 6439 | `		return PH7_OK;` |
|       - | 6440 | `	}` |
|       - | 6441 | `	/* Create a new array */` |
|      18 | 6442 | `	pArray = ph7_context_new_array(pCtx);` |
|      18 | 6443 | `	if( pArray == 0 ){` |
|     ! 0 | 6444 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6445 | `		return PH7_OK;` |
|       - | 6446 | `	}` |
|       - | 6447 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 6448 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      18 | 6449 | `	pEntry = pMap->pFirst;` |
|      18 | 6450 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      18 | 6451 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6452 | `	/* Perform the requested operation */` |
|      68 | 6453 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6454 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      56 | 6455 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      56 | 6456 | `		if( pValue == 0 ){` |
|       - | 6457 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 6458 | `			keep = FALSE;` |
|      56 | 6459 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 6460 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 6461 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 6462 | `				* silently dropped the element.  Emit similar message. */` |
|      28 | 6463 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 6464 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6465 | `					int len;` |
|       3 | 6466 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 6467 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6468 | `						"TypeError",` |
|       - | 6469 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 6470 | `						zName` |
|       - | 6471 | `						);` |
|     ! 0 | 6472 | `				}else{` |
|     ! 0 | 6473 | `					return PH7_VmThrowException(pCtx,` |
|       - | 6474 | `						"TypeError",` |
|       - | 6475 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 6476 | `						ph7_type_name(apArg[1])` |
|       - | 6477 | `						);` |
|       - | 6478 | `				}` |
|       - | 6479 | `			}` |
|      25 | 6480 | `			keep = FALSE;` |
|      25 | 6481 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      25 | 6482 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 6483 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6484 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 6485 | `				return PH7_EXCEPTION;` |
|       - | 6486 | `			}` |
|      23 | 6487 | `			if( rc == SXRET_OK ){` |
|       - | 6488 | `				/* Perform a boolean cast */` |
|      23 | 6489 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6490 | `			}` |
|      23 | 6491 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6492 | `		}else{` |
|       - | 6493 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6494 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6495 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6496 | `			 */` |
|      29 | 6497 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6498 | `		}` |
|      51 | 6499 | `		if( keep ){` |
|       - | 6500 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6501 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6502 | `		}` |
|       - | 6503 | `		/* Point to the next entry */` |
|      51 | 6504 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6505 | `	}` |
|      13 | 6506 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6507 | `	return PH7_OK;` |
|      12 | 6508 |  |
|       - | 6509 | `/*` |
|       - | 6510 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 6511 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 6512 | ` * Parameters` |
|       - | 6513 | ` *  $callback` |
|       - | 6514 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 6515 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 6516 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 6517 | ` *   are zipped together.` |
|       - | 6518 | ` *  $array` |
|       - | 6519 | ` *   The first array to run through the callback function.` |
|       - | 6520 | ` *  $arrays` |
|       - | 6521 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 6522 | ` * Return` |
|       - | 6523 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 6524 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 6525 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 6526 | ` *  padding shorter arrays with NULL.` |
|       - | 6527 | ` */` |
|      46 | 6528 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6529 |  |
|       - | 6530 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6531 | `	ph7_hashmap_node *pEntry;` |
|       - | 6532 | `	ph7_hashmap *pMap;` |
|       - | 6533 | `	ph7_vm *pVm;` |
|       - | 6534 | `	int bNullCallback;` |
|       - | 6535 | `	sxi32 rc;` |
|       - | 6536 | `	int i;` |
|       - | 6537 | `	sxu32 n;` |
|      48 | 6538 | `	if( nArg < 2 ){` |
|       7 | 6539 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6540 | `			"ArgumentCountError",` |
|       - | 6541 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6542 | `			nArg` |
|       - | 6543 | `			);` |
|       - | 6544 | `	}` |
|      44 | 6545 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      44 | 6546 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6547 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6548 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6549 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6550 | `				"TypeError",` |
|       - | 6551 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6552 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6553 | `				zFunc` |
|       - | 6554 | `				);` |
|       - | 6555 | `		}` |
|       3 | 6556 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6557 | `			"TypeError",` |
|       - | 6558 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6559 | `			"no array or string given"` |
|       - | 6560 | `			);` |
|       - | 6561 | `	}` |
|       - | 6562 | `	/* Every remaining argument must be an array */` |
|      88 | 6563 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      52 | 6564 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 6565 | `			if( i == 1 ){` |
|       4 | 6566 | `				return PH7_VmThrowException(pCtx,` |
|       - | 6567 | `					"TypeError",` |
|       - | 6568 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6569 | `					ph7_type_name(apArg[1])` |
|       - | 6570 | `					);` |
|       - | 6571 | `			}` |
|     ! 0 | 6572 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6573 | `				"TypeError",` |
|       - | 6574 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 6575 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 6576 | `				);` |
|       - | 6577 | `		}` |
|      26 | 6578 | `	}` |
|      38 | 6579 | `	pVm = pCtx->pVm;` |
|       - | 6580 | `	/* Create a new array */` |
|      38 | 6581 | `	pArray = ph7_context_new_array(pCtx);` |
|      38 | 6582 | `	if( pArray == 0 ){` |
|     ! 0 | 6583 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6584 | `		return PH7_OK;` |
|       - | 6585 | `	}` |
|      38 | 6586 | `	PH7_MemObjInit(pVm,&sResult);` |
|      38 | 6587 | `	PH7_MemObjInit(pVm,&sKey);` |
|      38 | 6588 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      38 | 6589 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      38 | 6590 | `	if( nArg == 2 ){` |
|       - | 6591 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      28 | 6592 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      28 | 6593 | `		pEntry = pMap->pFirst;` |
|      82 | 6594 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6595 | `			/* Extract the node value */` |
|      58 | 6596 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|      58 | 6597 | `			if( pValue ){` |
|       - | 6598 | `				/* Extract the node key */` |
|      58 | 6599 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      58 | 6600 | `				if( bNullCallback ){` |
|       - | 6601 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 6602 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6603 | `				}else{` |
|       - | 6604 | `					/* Invoke the supplied callback */` |
|      48 | 6605 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|      48 | 6606 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 6607 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 6608 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       3 | 6609 | `						PH7_MemObjRelease(&sKey);` |
|       3 | 6610 | `						PH7_MemObjRelease(&sResult);` |
|       3 | 6611 | `						return PH7_EXCEPTION;` |
|       - | 6612 | `					}` |
|       - | 6613 | `					/* Insert the callback return value */` |
|      46 | 6614 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6615 | `				}` |
|      56 | 6616 | `				PH7_MemObjRelease(&sKey);` |
|      56 | 6617 | `				PH7_MemObjRelease(&sResult);` |
|      27 | 6618 | `			}` |
|       - | 6619 | `			/* Point to the next entry */` |
|      56 | 6620 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 6621 | `		}` |
|      14 | 6622 | `	}else{` |
|       - | 6623 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 6624 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 6625 | `		int nArrays = nArg - 1;` |
|       - | 6626 | `		ph7_hashmap_node **apCur;` |
|       - | 6627 | `		ph7_value **apCallArg;` |
|       - | 6628 | `		ph7_value sNull;` |
|      11 | 6629 | `		sxu32 nMax = 0;` |
|      11 | 6630 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 6631 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 6632 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 6633 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 6634 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 6635 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6636 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6637 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 6638 | `			return PH7_OK;` |
|       - | 6639 | `		}` |
|      11 | 6640 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 6641 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 6642 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 6643 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 6644 | `			apCur[i] = pMap->pFirst;` |
|      23 | 6645 | `			if( pMap->nEntry > nMax ){` |
|      13 | 6646 | `				nMax = pMap->nEntry;` |
|       6 | 6647 | `			}` |
|      12 | 6648 | `		}` |
|      35 | 6649 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 6650 | `			ph7_value *pZip = 0;` |
|      25 | 6651 | `			if( bNullCallback ){` |
|       - | 6652 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 6653 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 6654 | `			}` |
|      79 | 6655 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 6656 | `				ph7_value *pv = &sNull;` |
|      55 | 6657 | `				if( apCur[i] ){` |
|      53 | 6658 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 6659 | `					if( pNodeVal ){` |
|      53 | 6660 | `						pv = pNodeVal;` |
|      26 | 6661 | `					}` |
|      53 | 6662 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 6663 | `				}` |
|      55 | 6664 | `				if( bNullCallback ){` |
|       9 | 6665 | `					if( pZip ){` |
|       9 | 6666 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 6667 | `					}` |
|       5 | 6668 | `				}else{` |
|      47 | 6669 | `					apCallArg[i] = pv;` |
|       - | 6670 | `				}` |
|      28 | 6671 | `			}` |
|      25 | 6672 | `			if( bNullCallback ){` |
|       5 | 6673 | `				if( pZip ){` |
|       5 | 6674 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 6675 | `				}` |
|       3 | 6676 | `			}else{` |
|      21 | 6677 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 6678 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6679 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 6680 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 6681 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 6682 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 6683 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 6684 | `					return PH7_EXCEPTION;` |
|       - | 6685 | `				}` |
|      21 | 6686 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 6687 | `				PH7_MemObjRelease(&sResult);` |
|       - | 6688 | `			}` |
|      13 | 6689 | `		}` |
|      11 | 6690 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 6691 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 6692 | `		PH7_MemObjRelease(&sNull);` |
|       - | 6693 | `	}` |
|      36 | 6694 | `	PH7_MemObjRelease(&sKey);` |
|      36 | 6695 | `	PH7_MemObjRelease(&sResult);` |
|      36 | 6696 | `	ph7_result_value(pCtx,pArray);` |
|      36 | 6697 | `	return PH7_OK;` |
|      25 | 6698 |  |
|       - | 6699 | `/*` |
|       - | 6700 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 6701 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6702 | ` * Parameters` |
|       - | 6703 | ` *  $array` |
|       - | 6704 | ` *   The input array.` |
|       - | 6705 | ` *  $callback` |
|       - | 6706 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 6707 | ` *  $initial` |
|       - | 6708 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 6709 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 6710 | ` * Return` |
|       - | 6711 | ` *  Returns the resulting value.` |
|       - | 6712 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6713 | ` */` |
|      32 | 6714 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6715 |  |
|       - | 6716 | `	ph7_hashmap_node *pEntry;` |
|       - | 6717 | `	ph7_hashmap *pMap;` |
|       - | 6718 | `	ph7_value *pValue;` |
|       - | 6719 | `	ph7_value sResult;` |
|       - | 6720 | `	sxi32 rc;` |
|       - | 6721 | `	sxu32 n;` |
|      34 | 6722 | `	if( nArg < 2 ){` |
|       7 | 6723 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6724 | `			"ArgumentCountError",` |
|       - | 6725 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 6726 | `			nArg` |
|       - | 6727 | `			);` |
|       - | 6728 | `	}` |
|      30 | 6729 | `	if( nArg > 3 ){` |
|       4 | 6730 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6731 | `			"ArgumentCountError",` |
|       - | 6732 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 6733 | `			nArg` |
|       - | 6734 | `			);` |
|       - | 6735 | `	}` |
|      28 | 6736 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6737 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6738 | `			"TypeError",` |
|       - | 6739 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6740 | `			ph7_type_name(apArg[0])` |
|       - | 6741 | `			);` |
|       - | 6742 | `	}` |
|      26 | 6743 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       9 | 6744 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6745 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6746 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6747 | `				"TypeError",` |
|       - | 6748 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6749 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6750 | `				zFunc` |
|       - | 6751 | `				);` |
|       - | 6752 | `		}` |
|       7 | 6753 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 6754 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6755 | `				"TypeError",` |
|       - | 6756 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6757 | `				"array callback must have exactly two members"` |
|       - | 6758 | `				);` |
|       - | 6759 | `		}` |
|       5 | 6760 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6761 | `			"TypeError",` |
|       - | 6762 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6763 | `			"no array or string given"` |
|       - | 6764 | `			);` |
|       - | 6765 | `	}` |
|       - | 6766 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 6767 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6768 | `	/* Assume a NULL initial value */` |
|      17 | 6769 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      17 | 6770 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      17 | 6771 | `	if( nArg > 2 ){` |
|       - | 6772 | `		/* Set the initial value */` |
|      11 | 6773 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       5 | 6774 | `	}` |
|       - | 6775 | `	/* Perform the requested operation */` |
|      17 | 6776 | `	pEntry = pMap->pFirst;` |
|      45 | 6777 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6778 | `		/* Extract the node value */` |
|      31 | 6779 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6780 | `		/* Invoke the supplied callback */` |
|      31 | 6781 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      31 | 6782 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 6783 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6784 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 6785 | `			return PH7_EXCEPTION;` |
|       - | 6786 | `		}` |
|       - | 6787 | `		/* Point to the next entry */` |
|      29 | 6788 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6789 | `	}` |
|      15 | 6790 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      15 | 6791 | `	PH7_MemObjRelease(&sResult);` |
|      15 | 6792 | `	return PH7_OK;` |
|      18 | 6793 |  |
|       - | 6794 | `/*` |
|       - | 6795 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6796 | ` *  Apply a user function to every member of an array.` |
|       - | 6797 | ` * Parameters` |
|       - | 6798 | ` *  $array` |
|       - | 6799 | ` *   The input array.` |
|       - | 6800 | ` *  $funcname` |
|       - | 6801 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6802 | ` *   the first, and the key/index second.` |
|       - | 6803 | ` * Note:` |
|       - | 6804 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6805 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6806 | ` *  be made in the original array itself.` |
|       - | 6807 | ` *  $userdata` |
|       - | 6808 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6809 | ` *   to the callback funcname.` |
|       - | 6810 | ` * Return` |
|       - | 6811 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6812 | ` */` |
|      38 | 6813 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6814 |  |
|       - | 6815 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6816 | `	ph7_hashmap_node *pEntry;` |
|       - | 6817 | `	ph7_hashmap *pMap;` |
|       - | 6818 | `	sxu32 n;` |
|      40 | 6819 | `	if( nArg < 2 ){` |
|       7 | 6820 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6821 | `			"ArgumentCountError",` |
|       - | 6822 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 6823 | `			nArg` |
|       - | 6824 | `			);` |
|       - | 6825 | `	}` |
|      36 | 6826 | `	if( nArg > 3 ){` |
|       4 | 6827 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6828 | `			"ArgumentCountError",` |
|       - | 6829 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 6830 | `			nArg` |
|       - | 6831 | `			);` |
|       - | 6832 | `	}` |
|      34 | 6833 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6834 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6835 | `			"TypeError",` |
|       - | 6836 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6837 | `			ph7_type_name(apArg[0])` |
|       - | 6838 | `			);` |
|       - | 6839 | `	}` |
|      32 | 6840 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6841 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6842 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6843 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6844 | `				"TypeError",` |
|       - | 6845 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6846 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6847 | `				zFunc` |
|       - | 6848 | `				);` |
|       - | 6849 | `		}` |
|       9 | 6850 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6851 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6852 | `				"TypeError",` |
|       - | 6853 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6854 | `				"array callback must have exactly two members"` |
|       - | 6855 | `				);` |
|       - | 6856 | `		}` |
|       5 | 6857 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6858 | `			"TypeError",` |
|       - | 6859 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6860 | `			"no array or string given"` |
|       - | 6861 | `			);` |
|       - | 6862 | `	}` |
|      21 | 6863 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6864 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6865 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 6866 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 6867 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 6868 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6869 | `	/* Perform the desired operation */` |
|      21 | 6870 | `	pEntry = pMap->pFirst;` |
|      61 | 6871 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6872 | `		/* Extract the node value */` |
|      43 | 6873 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 6874 | `		if( pValue ){` |
|       - | 6875 | `			sxi32 rcW;` |
|       - | 6876 | `			/* Extract the entry key */` |
|      43 | 6877 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6878 | `			/* Invoke the supplied callback */` |
|      43 | 6879 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 6880 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 6881 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 6882 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 6883 | `				return PH7_EXCEPTION;` |
|       - | 6884 | `			}` |
|      20 | 6885 | `		}` |
|       - | 6886 | `		/* Point to the next entry */` |
|      41 | 6887 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 6888 | `	}` |
|       - | 6889 | `	/* All done, return TRUE */` |
|      19 | 6890 | `	ph7_result_bool(pCtx,1);` |
|      19 | 6891 | `	return PH7_OK;` |
|      21 | 6892 |  |
|       - | 6893 | `/*` |
|       - | 6894 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6895 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6896 | ` */` |
|      22 | 6897 | `static sxi32 HashmapWalkRecursive(` |
|       - | 6898 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6899 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6900 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6901 | `	int iNest             /* Nesting level */` |
|       - | 6902 | `	)` |
|       1 | 6903 |  |
|       - | 6904 | `	ph7_hashmap_node *pEntry;` |
|       - | 6905 | `	ph7_value *pValue,sKey;` |
|       - | 6906 | `	sxi32 rc;` |
|       - | 6907 | `	sxu32 n;` |
|       - | 6908 | `	/* Iterate through hashmap entries */` |
|      23 | 6909 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 6910 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 6911 | `	pEntry = pMap->pFirst;` |
|      59 | 6912 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6913 | `		/* Extract the node value */` |
|      37 | 6914 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 6915 | `		if( pValue ){` |
|      37 | 6916 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 6917 | `				if( iNest < 32 ){` |
|       - | 6918 | `					/* Recurse */` |
|      11 | 6919 | `					iNest++;` |
|      11 | 6920 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 6921 | `					iNest--;` |
|      11 | 6922 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 6923 | `						return PH7_EXCEPTION;` |
|       - | 6924 | `					}` |
|       5 | 6925 | `				}` |
|       6 | 6926 | `			}else{` |
|       - | 6927 | `				/* Extract the node key */` |
|      27 | 6928 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6929 | `				/* Invoke the supplied callback */` |
|      27 | 6930 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 6931 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 6932 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 6933 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 6934 | `					return PH7_EXCEPTION;` |
|       - | 6935 | `				}` |
|       - | 6936 | `			}` |
|      18 | 6937 | `		}` |
|       - | 6938 | `		/* Point to the next entry */` |
|      37 | 6939 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 6940 | `	}` |
|      23 | 6941 | `	return PH7_OK;` |
|      12 | 6942 |  |
|       - | 6943 | `/*` |
|       - | 6944 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 6945 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6946 | ` * Parameters` |
|       - | 6947 | ` *  $array` |
|       - | 6948 | ` *   The input array.` |
|       - | 6949 | ` *  $funcname` |
|       - | 6950 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 6951 | ` *   the first, and the key/index second.` |
|       - | 6952 | ` * Note:` |
|       - | 6953 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 6954 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6955 | ` *  be made in the original array itself.` |
|       - | 6956 | ` *  $userdata` |
|       - | 6957 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6958 | ` *   to the callback funcname.` |
|       - | 6959 | ` * Return` |
|       - | 6960 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6961 | ` */` |
|      30 | 6962 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6963 |  |
|       - | 6964 | `	ph7_hashmap *pMap;` |
|      32 | 6965 | `	if( nArg < 2 ){` |
|       7 | 6966 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6967 | `			"ArgumentCountError",` |
|       - | 6968 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 6969 | `			nArg` |
|       - | 6970 | `			);` |
|       - | 6971 | `	}` |
|      28 | 6972 | `	if( nArg > 3 ){` |
|       4 | 6973 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6974 | `			"ArgumentCountError",` |
|       - | 6975 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 6976 | `			nArg` |
|       - | 6977 | `			);` |
|       - | 6978 | `	}` |
|      26 | 6979 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6980 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6981 | `			"TypeError",` |
|       - | 6982 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6983 | `			ph7_type_name(apArg[0])` |
|       - | 6984 | `			);` |
|       - | 6985 | `	}` |
|      24 | 6986 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      11 | 6987 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 6988 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 6989 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6990 | `				"TypeError",` |
|       - | 6991 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 6992 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6993 | `				zFunc` |
|       - | 6994 | `				);` |
|       - | 6995 | `		}` |
|       9 | 6996 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 | 6997 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6998 | `				"TypeError",` |
|       - | 6999 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7000 | `				"array callback must have exactly two members"` |
|       - | 7001 | `				);` |
|       - | 7002 | `		}` |
|       5 | 7003 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7004 | `			"TypeError",` |
|       - | 7005 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7006 | `			"no array or string given"` |
|       - | 7007 | `			);` |
|       - | 7008 | `	}` |
|       - | 7009 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 7010 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 7011 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7012 | `	/* Perform the desired operation */` |
|      13 | 7013 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 7014 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7015 | `		return PH7_EXCEPTION;` |
|       - | 7016 | `	}` |
|       - | 7017 | `	/* All done, return TRUE */` |
|      13 | 7018 | `	ph7_result_bool(pCtx,1);` |
|      13 | 7019 | `	return PH7_OK;` |
|      17 | 7020 |  |
|       - | 7021 | `/*` |
|       - | 7022 | ` * Table of hashmap functions.` |
|       - | 7023 | ` */` |
|       - | 7024 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 7025 | `	{"count",             ph7_hashmap_count },` |
|       - | 7026 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 7027 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 7028 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 7029 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 7030 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 7031 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 7032 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 7033 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 7034 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 7035 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 7036 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 7037 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 7038 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 7039 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 7040 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 7041 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 7042 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 7043 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 7044 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 7045 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 7046 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 7047 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 7048 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 7049 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 7050 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 7051 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 7052 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 7053 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 7054 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 7055 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 7056 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 7057 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 7058 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 7059 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 7060 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 7061 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 7062 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 7063 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 7064 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 7065 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 7066 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 7067 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 7068 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 7069 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 7070 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 7071 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 7072 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 7073 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 7074 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 7075 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 7076 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 7077 | `	{"range",             ph7_hashmap_range   },` |
|       - | 7078 | `	{"current",           ph7_hashmap_current },` |
|       - | 7079 | `	{"each",              ph7_hashmap_each    },` |
|       - | 7080 | `	{"pos",               ph7_hashmap_current },` |
|       - | 7081 | `	{"next",              ph7_hashmap_next    },` |
|       - | 7082 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 7083 | `	{"end",               ph7_hashmap_end     },` |
|       - | 7084 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 7085 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 7086 | `};` |
|       - | 7087 | `/*` |
|       - | 7088 | ` * Register the built-in hashmap functions defined above.` |
|       - | 7089 | ` */` |
|    2808 | 7090 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 7091 |  |
|       - | 7092 | `	sxu32 n;` |
|  174098 | 7093 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  171290 | 7094 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   85646 | 7095 | `	}` |
|    2810 | 7096 |  |
|       - | 7097 | `/*` |
|       - | 7098 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 7099 | ` * the BLOB given as the first argument.` |
|       - | 7100 | ` * This function is typically invoked when the user issue a call to` |
|       - | 7101 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 7102 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 7103 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 7104 | ` */` |
|      26 | 7105 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 7106 |  |
|       - | 7107 | `	ph7_hashmap_node *pEntry;` |
|       - | 7108 | `	ph7_value *pObj;` |
|      28 | 7109 | `	sxu32 n = 0;` |
|       - | 7110 | `	int isRef;` |
|       - | 7111 | `	sxi32 rc;` |
|       - | 7112 | `	int i;` |
|      28 | 7113 | `	if( nDepth > 31 ){` |
|       - | 7114 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 7115 | `		/* Nesting limit reached */` |
|     ! 0 | 7116 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 7117 | `		if( ShowType ){` |
|     ! 0 | 7118 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 7119 | `		}` |
|     ! 0 | 7120 | `		return SXERR_LIMIT;` |
|       - | 7121 | `	}` |
|       - | 7122 | `	/* Point to the first inserted entry */` |
|      28 | 7123 | `	pEntry = pMap->pFirst;` |
|      28 | 7124 | `	rc = SXRET_OK;` |
|      28 | 7125 | `	if( !ShowType ){` |
|      15 | 7126 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 7127 | `	}` |
|       - | 7128 | `	/* Total entries */` |
|      28 | 7129 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 7130 | `#ifdef __WINNT__` |
|       2 | 7131 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7132 | `#else` |
|      26 | 7133 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7134 | `#endif` |
|      62 | 7135 | `	for(;;){` |
|     126 | 7136 | `		if( n >= pMap->nEntry ){` |
|      28 | 7137 | `			break;` |
|       - | 7138 | `		}` |
|     198 | 7139 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 7140 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 7141 | `		}` |
|       - | 7142 | `		/* Dump key */` |
|     100 | 7143 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 7144 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 7145 | `		}else{` |
|     101 | 7146 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 7147 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 7148 | `		}` |
|       - | 7149 | `#ifdef __WINNT__` |
|       2 | 7150 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 7151 | `#else` |
|      98 | 7152 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 7153 | `#endif` |
|       - | 7154 | `		/* Dump node value */` |
|     100 | 7155 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 7156 | `		isRef = 0;` |
|     100 | 7157 | `		if( pObj ){` |
|     100 | 7158 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 7159 | `				/* Referenced object */` |
|     ! 0 | 7160 | `				isRef = 1;` |
|     ! 0 | 7161 | `			}` |
|     100 | 7162 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 7163 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 7164 | `				break;` |
|       - | 7165 | `			}` |
|      49 | 7166 | `		}` |
|       - | 7167 | `		/* Point to the next entry */` |
|     100 | 7168 | `		n++;` |
|     100 | 7169 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 7170 | `	}` |
|      54 | 7171 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 7172 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 7173 | `	}` |
|      28 | 7174 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 7175 | `	return rc;` |
|      15 | 7176 |  |
|       - | 7177 | `/*` |
|       - | 7178 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 7179 | ` * retrieved entry.` |
|       - | 7180 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 7181 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 7182 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 7183 | ` * a value different from PH7_OK.` |
|       - | 7184 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 7185 | ` */` |
|   29304 | 7186 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 7187 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 7188 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 7189 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 7190 | `	)` |
|       2 | 7191 |  |
|       - | 7192 | `	ph7_hashmap_node *pEntry;` |
|       - | 7193 | `	ph7_value sKey,sValue;` |
|       - | 7194 | `	sxi32 rc;` |
|       - | 7195 | `	sxu32 n;` |
|       - | 7196 | `	/* Initialize walker parameter */` |
|   29306 | 7197 | `	rc = SXRET_OK;` |
|   29306 | 7198 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   29306 | 7199 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   29306 | 7200 | `	n = pMap->nEntry;` |
|   29306 | 7201 | `	pEntry = pMap->pFirst;` |
|       - | 7202 | `	/* Start the iteration process */` |
|   73319 | 7203 | `	for(;;){` |
|  146640 | 7204 | `		if( n < 1 ){` |
|   29306 | 7205 | `			break;` |
|       - | 7206 | `		}` |
|       - | 7207 | `		/* Extract a copy of the key and a copy the current value */` |
|  117336 | 7208 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  117336 | 7209 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 7210 | `		/* Invoke the user callback */` |
|  117336 | 7211 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 7212 | `		/* Release the copy of the key and the value */` |
|  117336 | 7213 | `		PH7_MemObjRelease(&sKey);` |
|  117336 | 7214 | `		PH7_MemObjRelease(&sValue);` |
|  117336 | 7215 | `		if( rc != PH7_OK ){` |
|       - | 7216 | `			/* Callback request an operation abort */` |
|     ! 0 | 7217 | `			return SXERR_ABORT;` |
|       - | 7218 | `		}` |
|       - | 7219 | `		/* Point to the next entry */` |
|  117336 | 7220 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  117336 | 7221 | `		n--;` |
|       2 | 7222 | `	}` |
|       - | 7223 | `	/* All done */` |
|   29306 | 7224 | `	return SXRET_OK;` |
|   14654 | 7225 |  |
|       - | 7226 |  |
